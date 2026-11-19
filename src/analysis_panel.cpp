#include "now_playing.h"
#include <QScrollBar>
#include "microphone_access.h"
#include "responsive_ui.h"
// SPDX-License-Identifier: GPL-3.0-only
#include "analysis_panel.h"
#include "localization.h"
#include "localized_file_dialog.h"
#include "button_help.h"
#include "accelerating_spinbox.h"
#include "settings_dialogs.h"
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHideEvent>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QShowEvent>
#include <QWheelEvent>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <deque>
#include <cmath>
#include <cstring>
#include <limits>
namespace soundcurrent::analysis {
static QString noteName(int midi) {
    static const char *names[]={"C","C♯","D","D♯","E","F","F♯","G","G♯","A","A♯","B"};
    return midi<0?QString::fromUtf8("—"):QString::fromUtf8(names[(midi%12+12)%12])+QLocale().toString(midi/12-1);
}
class ScopeCanvas : public QWidget {
public:
    explicit ScopeCanvas(QWidget *parent):QWidget(parent),data(std::make_unique<Engine>()) {
        setObjectName("analysisCanvas");setMinimumSize(220,230);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);setMouseTracking(true);
        clearImage();
        rollBar=new QScrollBar(Qt::Vertical,this);rollBar->setObjectName("analysisNoteScroll");rollBar->setAccessibleName(QStringLiteral("Notes"));rollBar->hide();
        connect(rollBar,&QScrollBar::valueChanged,this,[this](int value){rollLowManual=std::clamp(85-rollSpan-value,36,85-rollSpan);update();});
    }
    std::unique_ptr<Engine> data;
    View mode=View::Waterfall;
    bool frozen=false;
    bool showChords=true;
    // Scrolling note display window: low note (C-aligned), span in semitones (12..48), manual low note or -1 for automatic follow.
    int rollSpan=24,rollLowAuto=48,rollLowManual=-1;
    int rollLowNow() const {return rollLowManual>=0?rollLowManual:rollLowAuto;}
    // The Tuner uses the whole canvas (it has no axes): note display, then chord, note and gauge.
    QRectF tunerArea() const {return QRectF(rect()).adjusted(6,2,-6,-2);}
    QRectF tunerRoll() const {const auto a=tunerArea();const double keys=34,gutter=44,bar=18;return QRectF(a.left()+keys,a.top()+2,std::max(40.,a.width()-keys-gutter-bar-4),std::clamp(a.height()-104,64.,a.height()*.85));}
    QScrollBar *rollBar=nullptr;
    // Positions the scroll bar beside the note display and mirrors the window position (top = highest notes).
    void syncRollBar(){
        if(!rollBar)return;const bool show=mode==View::Tuner;rollBar->setVisible(show);if(!show)return;
        const auto r=tunerRoll(),a=tunerArea();rollBar->setGeometry(int(a.right()-18),int(r.top()),18,int(r.height()));
        const QSignalBlocker block(rollBar);const int range=std::max(0,49-rollSpan);rollBar->setRange(0,range);rollBar->setPageStep(rollSpan);rollBar->setSingleStep(3);
        rollBar->setValue(std::clamp(85-rollSpan-rollLowNow(),0,range));
    }
    void resizeEvent(QResizeEvent *event) override {QWidget::resizeEvent(event);syncRollBar();}
    std::function<void(bool)> onVisibility;
    std::function<void(const Settings &)> onZoom;std::function<void(bool)> onScroll;void resumeLive(){scrollBack_=0;setProperty("scrollBack",0);update();}std::function<void()> onFrame;
    double displayCeiling=0;
    void resetClock(){clock.invalidate();carryMs_=0;}
    // Time since the previous frame STARTED: the clock restarts after the frame's FFT, so that compute time is
    // carried over; without it every frame lost its compute time and the display scrolled ~15% slower than real time.
    double sinceLastMs() const {return clock.isValid()?clock.nsecsElapsed()/1e6+carryMs_:1e9;}
    // Frames are paced by what the machine can actually deliver: about three times the measured
    // cost of computing + painting one frame, never faster than ~60 fps and never slower than the
    // view's classic cadence `slowest`. A busy system therefore degrades gracefully instead of
    // fighting the audio threads; an idle one gets a smooth display.
    double paceMs(double slowest) const {return std::clamp(3.*(computeMs_+paintMs_),16.,std::max(16.,slowest));}
    QSize historyPixelSize() const {return {image.width,image.height};}
    void clearImage(){image={};archive_.clear();scrollBack_=0;ensureImageSize();credit=0;started_=false;waveCols_.clear();waveCap_=0;silenceClock_.invalidate();sinceFrame_.invalidate();latest_.clear();clock.invalidate();carryMs_=0;update();}
    bool advanceIdle() {
        if(!isVisible()||frozen||(mode!=View::Waterfall&&mode!=View::Waveform))return false;
        if(mode==View::Waterfall)return advanceWaterfall();
        return advanceWave();
    }
    // Scrolling waveform on the SAMPLE clock, like the spectrogram: each pixel column is the min/max envelope of the audio between two exact sample positions (history/width seconds apart); late frames catch up by computing the missing columns. Painting is a handful of vertical lines, so it costs almost nothing.
    bool advanceWave() {
        const auto &s=data->settings;
        const int cap=std::clamp(int(plotRect().width()),1,1280);
        if(cap!=waveCap_){waveCap_=cap;waveCols_.clear();started_=false;}
        const double columnsPerSecond=cap*s.speed/s.history;setProperty("columnsPerSecond",columnsPerSecond);
        const bool live=!latest_.isEmpty()&&sinceArrival_.isValid()&&sinceArrival_.elapsed()<kStallMs;
        if(!live){
            if(started_){started_=false;silenceClock_.restart();credit=kStallMs/1000.*columnsPerSecond;}
            if(!silenceClock_.isValid())silenceClock_.restart();
            credit+=silenceClock_.nsecsElapsed()/1e9*columnsPerSecond;silenceClock_.restart();
            const int columns=std::min(cap,int(credit));credit-=columns;
            if(columns<=0)return false;
            for(int i=0;i<columns;++i)pushWaveColumn(WaveColumn{});
            setProperty("idleFrames",property("idleFrames").toInt()+1);update();return true;
        }
        silenceClock_.invalidate();
        const double step=48000./columnsPerSecond;
        const qint64 frames=latest_.size()/4;
        if(!started_){started_=true;nextEnd_=double(latestEnd_);}
        QElapsedTimer budget;budget.start();bool drew=false;
        while(nextEnd_<=double(latestEnd_)&&budget.elapsed()<8){
            const qint64 end=frames-(latestEnd_-qint64(nextEnd_)),begin=end-qint64(std::max(1.,step));
            if(begin<0){nextEnd_=double(latestEnd_);break;} // no longer buffered (far behind): jump to now
            WaveColumn column;const auto *pcm=reinterpret_cast<const int16_t *>(latest_.constData());
            for(int c=0;c<3;++c){column.lo[c]=1.f;column.hi[c]=-1.f;}
            for(qint64 i=begin;i<end;++i){
                const float l=pcm[2*i]/32768.f,r=pcm[2*i+1]/32768.f,m=.5f*(l+r),v[3]={l,r,m};
                for(int c=0;c<3;++c){column.lo[c]=std::min(column.lo[c],v[c]);column.hi[c]=std::max(column.hi[c],v[c]);}
            }
            pushWaveColumn(column);setProperty("columns",property("columns").toInt()+1);
            nextEnd_+=step;drew=true;
        }
        if(nextEnd_+48000.<double(latestEnd_))nextEnd_=double(latestEnd_)-step; // hopelessly behind: resume from now
        if(!drew)return false;
        setProperty("pumpFrames",property("pumpFrames").toInt()+1);update();return true;
    }
    struct WaveColumn{float lo[3]={0,0,0},hi[3]={0,0,0};}; // 0 = left, 1 = right, 2 = mix
    void pushWaveColumn(const WaveColumn &column){waveCols_.push_back(column);while(int(waveCols_.size())>std::max(1,waveCap_)*kKeepScreens)waveCols_.pop_front();}
    // Spectrogram on the SAMPLE clock: every pixel column is the FFT of the window that ends at an exact audio sample, columns are exactly history/width seconds apart, and a late frame is caught up by computing the missing columns (never blended or stretched). The picture therefore depends only on the audio, not on display timing. Real silence (no audio arriving) scrolls black in real time.
    bool advanceWaterfall() {
        ensureImageSize();
        const auto &s=data->settings;
        const double columnsPerSecond=image.width*s.speed/s.history;setProperty("columnsPerSecond",columnsPerSecond);
        const bool live=!latest_.isEmpty()&&sinceArrival_.isValid()&&sinceArrival_.elapsed()<kStallMs;
        if(!live){
            if(started_){started_=false;silenceClock_.restart();credit=kStallMs/1000.*columnsPerSecond;} // the stall-detection delay is time that already passed
            if(!silenceClock_.isValid())silenceClock_.restart();
            credit+=silenceClock_.nsecsElapsed()/1e9*columnsPerSecond;silenceClock_.restart();
            const int columns=std::min(image.width,int(credit));credit-=columns;
            if(columns<=0)return false;
            archiveColumns(columns);
            for(int y=0;y<image.height;++y){auto *row=image.pixels.data()+y*image.width;std::memmove(row,row+columns,std::size_t(image.width-columns)*sizeof(*row));std::fill(row+image.width-columns,row+image.width,0xff000000);}
            setProperty("idleFrames",property("idleFrames").toInt()+1);if(onFrame)onFrame();update();return true;
        }
        silenceClock_.invalidate();
        const double step=48000./columnsPerSecond; // capture frames between two columns
        const int frames=int(latest_.size()/4),needed=int(std::ceil((s.fft-1)*48000./s.rate))+65;
        if(!started_){started_=true;nextEnd_=double(latestEnd_);}
        QElapsedTimer budget;budget.start();bool drew=false;
        // A time budget keeps the UI responsive when the machine cannot keep up: the rest is caught up on the next tick.
        while(nextEnd_<=double(latestEnd_)&&budget.elapsed()<8){
            const qint64 trim=latestEnd_-qint64(nextEnd_);
            if(trim>frames-needed){nextEnd_=double(latestEnd_);break;} // no longer buffered (far behind): jump to now
            if(!renderColumn(int(trim),step))break;
            nextEnd_+=step;drew=true;
        }
        if(nextEnd_+48000.<double(latestEnd_))nextEnd_=double(latestEnd_)-step; // hopelessly behind: resume from now
        if(!drew)return false;
        setProperty("pumpFrames",property("pumpFrames").toInt()+1);if(onFrame)onFrame();update();return true;
    }
    // One spectrogram column for the window that ends `trim` frames before the newest buffered sample.
    bool renderColumn(int trim,double step) {
        QElapsedTimer work;work.start();
        const auto &s=data->settings;
        data->frameSeconds=std::clamp(step/48000.,.005,1.);
        if(!data->push(latest_.left(latest_.size()-qsizetype(trim)*4),View::Waterfall))return false;
        if(s.autoRange&&!data->db.empty()){
            const double top=*std::max_element(data->db.begin(),data->db.end());
            const double target=std::clamp(std::ceil((top+6)/6)*6,-120.,24.);
            displayCeiling=holdRange(displayCeiling,target,ceilingSince_);
        } else displayCeiling=s.ceiling;
        auto settings=gramSettings();auto column=spectrogramColumn(data->db,s,image.height);
        if(s.layout==2&&s.channel==0){column=spectrogramColumn(data->dbLeft,s,image.height/2);auto right=spectrogramColumn(data->dbRight,s,image.height-image.height/2);column.insert(column.end(),right.begin(),right.end());}
        archiveColumns(1);
        for(int y=0;y<image.height;++y){auto *row=image.pixels.data()+y*image.width;std::memmove(row,row+1,std::size_t(image.width-1)*sizeof(*row));}
        gram::set_image_column(image,image.width-1,column,settings);
        for(int y=0;y<image.height;++y)image.pixels[y*image.width+image.width-1]|=0xff000000;
        setProperty("columns",property("columns").toInt()+1);computeMs_=.8*computeMs_+.2*work.nsecsElapsed()/1e6;sinceFrame_.restart();
        return true;
    }
    // Audio arrival only refills the buffer. The Spectrogram is then paced and rendered by the display timer
    // (advanceIdle); the Waveform uses the same jitter buffer so its trace scrolls evenly; the other views render immediately.
    bool push(const QByteArray &pcm,qint64 endFrame=-1) {
        // A source without a sample counter (tests, other callers): estimate the end from the wall clock.
        if(endFrame<0)endFrame=sinceArrival_.isValid()?latestEnd_+qint64(sinceArrival_.nsecsElapsed()*48/1000000):qint64(pcm.size()/4);
        latest_=pcm;latestEnd_=endFrame;sinceArrival_.restart();
        if(mode==View::Waterfall||mode==View::Waveform)return false;
        return render(pcm,0);
    }
    bool render(const QByteArray &source,int trimSamples) {
        if(!isVisible() || frozen)return false;
        const auto &s=data->settings;const double slowest=(mode==View::Microphone || mode==View::Average)?40:(mode==View::Waveform || mode==View::Vectorscope)?50:std::max(60.,250.*s.fft/s.rate);
        const double interval=paceMs(slowest);
        if(clock.isValid() && sinceLastMs()+kTickSlackMs<interval)return false;
        QElapsedTimer work;work.start();
        const double elapsed=clock.isValid()?sinceLastMs()/1000.:interval/1000.;
        data->frameSeconds=elapsed;
        QByteArray pcm=source;
        if(trimSamples>0){const int frames=int(source.size()/4),needed=int(std::ceil((s.fft-1)*48000./s.rate))+65;
            const int trim=std::min(trimSamples,std::max(0,frames-needed));pcm=source.left(source.size()-qsizetype(trim)*4);}
        if(!data->push(pcm,mode))return false;
        clock.restart();carryMs_=work.nsecsElapsed()/1e6;
        if(s.autoRange && mode!=View::Waveform && mode!=View::Vectorscope && !data->db.empty()) {
            const double top=*std::max_element(data->db.begin(),data->db.end());
            const double target=std::clamp(std::ceil((top+6)/6)*6,-120.,24.);
            displayCeiling=holdRange(displayCeiling,target,ceilingSince_);
        } else displayCeiling=s.ceiling;
        computeMs_=.8*computeMs_+.2*work.nsecsElapsed()/1e6;sinceFrame_.restart();
        if(onFrame)onFrame();
        update();return true;
    }
    gram::SpectrogramSettings gramSettings() const {
        const auto &s=data->settings;gram::SpectrogramSettings g;g.fft_size=s.fft;g.hop_size=s.fft/4;g.image_height=image.height;
        g.min_db=displayCeiling-s.sensitivity;g.max_db=displayCeiling;g.min_frequency=s.low;g.max_frequency=s.upper();
        g.frequency_scale=gram::FrequencyScale(s.scale);g.palette_mode=gram::PaletteMode(s.palette);return g;
    }
protected:
    void showEvent(QShowEvent *event) override {QWidget::showEvent(event);clock.invalidate();carryMs_=0;if(onVisibility)onVisibility(true);}
    void hideEvent(QHideEvent *event) override {clock.invalidate();carryMs_=0;if(onVisibility)onVisibility(false);QWidget::hideEvent(event);}
    // Zoom and pan: the wheel zooms the frequency axis around the pointer (or the waveform amplitude), dragging pans the
    // frequency axis, and a double click restores the full range. They edit the ordinary range settings, so the controls follow.
    bool frequencyAxisView() const {return mode==View::Waterfall || mode==View::Spectrum || mode==View::Average || mode==View::Microphone;}
    bool verticalFrequency() const {return mode==View::Waterfall;}
    double axisRatio(QPointF position) const {const auto plot=plotRect();return verticalFrequency()?1-std::clamp((position.y()-plot.top())/plot.height(),0.,1.):std::clamp((position.x()-plot.left())/plot.width(),0.,1.);}
    static void clampRange(Settings &s){
        const double maxHz=std::min(s.rate,48000)/2.,minLow=s.scale==0?0.:1.;
        double low=std::max(minLow,s.low),high=std::min(maxHz,s.upper());
        const double width=high-low;
        if(low<minLow){high+=minLow-low;low=minLow;}
        if(high>maxHz){low-=high-maxHz;high=maxHz;}
        low=std::max(minLow,low);
        s.low=std::round(low*10)/10;s.high=std::round(high*10)/10;
    }
    void wheelEvent(QWheelEvent *event) override {
        if(mode==View::Tuner){if(!tunerArea().contains(event->position())){event->ignore();return;}   // wheel scrolls the note display; Ctrl+wheel zooms its octave range
            const int steps=event->angleDelta().y()>0?1:event->angleDelta().y()<0?-1:0;if(!steps){event->ignore();return;}
            if(event->modifiers()&Qt::ControlModifier){const int low=rollLowNow();rollSpan=std::clamp(rollSpan-12*steps,12,48);rollLowManual=std::clamp(low,36,85-rollSpan);}
            else rollLowManual=std::clamp(rollLowNow()+3*steps,36,85-rollSpan);
            syncRollBar();update();event->accept();return;
        }
        const auto plot=plotRect();if(!plot.contains(event->position()) || !onZoom){event->ignore();return;}
        if((mode==View::Waterfall || mode==View::Waveform) && (event->angleDelta().x()!=0 || (event->modifiers()&Qt::ShiftModifier))){
            const int delta=event->angleDelta().x()!=0?event->angleDelta().x():event->angleDelta().y();
            setScrollBack(scrollBack_+int(delta/120.*std::max(10.,plot.width()*.1)));event->accept();return;
        }
        const double steps=event->angleDelta().y()/120.;if(steps==0){event->ignore();return;}
        auto s=data->settings;
        if(mode==View::Waveform){s.autoRange=false;s.ceiling=std::clamp(20*std::log10(waveLimit_)-3*steps,-120.,24.);onZoom(s);event->accept();return;}
        if(!frequencyAxisView()){event->ignore();return;}
        const double z=std::pow(1.2,steps),f0=frequency(axisRatio(event->position())),low=s.low,upper=s.upper();
        if(s.scale==0){s.low=f0+(low-f0)/z;s.high=f0+(upper-f0)/z;}
        else{const double f=std::max(1.,f0);s.low=f*std::pow(std::max(1.,low)/f,1/z);s.high=f*std::pow(upper/f,1/z);}
        const double minimum=s.scale==0?100.:s.low*1.2-s.low;
        if(s.high-s.low<minimum)return;
        clampRange(s);onZoom(s);event->accept();
    }
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()==Qt::LeftButton && plotRect().contains(event->position()) && (mode==View::Waveform || (mode==View::Waterfall && (event->modifiers()&Qt::ShiftModifier)))){timeDragging_=true;dragStart_=event->position();dragBack_=scrollBack_;setCursor(Qt::ClosedHandCursor);}
        else if(event->button()==Qt::LeftButton && plotRect().contains(event->position()) && frequencyAxisView()){dragging_=true;dragStart_=event->position();dragSettings_=data->settings;setCursor(Qt::ClosedHandCursor);}
        QWidget::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {if(dragging_ || timeDragging_){dragging_=false;timeDragging_=false;unsetCursor();}QWidget::mouseReleaseEvent(event);}
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if(mode==View::Tuner){if(tunerArea().contains(event->position())){rollLowManual=-1;rollSpan=24;syncRollBar();update();}return;}   // back to following the music
        if(!onZoom || !plotRect().contains(event->position())){QWidget::mouseDoubleClickEvent(event);return;}
        if(scrollBack_>0){resumeLive();if(onScroll)onScroll(false);return;}
        auto s=data->settings;
        if(mode==View::Waveform)s.autoRange=true;else if(frequencyAxisView()){s.low=20;s.high=20000;}else return;
        onZoom(s);
    }
    void dragPan(QPointF position){
        const auto plot=plotRect();auto s=dragSettings_;
        const double shift=verticalFrequency()?(position.y()-dragStart_.y())/plot.height():-(position.x()-dragStart_.x())/plot.width();
        const double low=s.low,upper=s.upper();
        if(s.scale==0){const double width=upper-low;s.low=low+shift*width;s.high=upper+shift*width;}
        else{const double ratioSpan=upper/std::max(1.,low);s.low=std::max(1.,low)*std::pow(ratioSpan,shift);s.high=upper*std::pow(ratioSpan,shift);}
        const double width=s.high-s.low,maxHz=std::min(s.rate,48000)/2.;
        if(s.scale==0){if(s.low<0){s.high-=s.low;s.low=0;}if(s.high>maxHz){s.low-=s.high-maxHz;s.high=maxHz;}}
        else{const double factor=s.high/s.low;if(s.low<1){s.low=1;s.high=factor;}if(s.high>maxHz){s.high=maxHz;s.low=maxHz/factor;}}
        (void)width;clampRange(s);
        if(std::abs(s.low-data->settings.low)<.05 && std::abs(s.high-data->settings.high)<.05)return;
        if(dragClock_.isValid() && dragClock_.elapsed()<33)return;dragClock_.start();onZoom(s);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if(timeDragging_ && (event->buttons()&Qt::LeftButton)){
            const auto area=plotRect();const double columnsPerPixel=(mode==View::Waveform?waveCap_:image.width)/std::max(1.,area.width());
            setScrollBack(dragBack_+int(std::lround((event->position().x()-dragStart_.x())*columnsPerPixel)));return;
        }
        if(dragging_ && (event->buttons()&Qt::LeftButton) && onZoom){dragPan(event->position());return;}
        const auto plot=plotRect();if(!plot.contains(event->position()))return;
        const double ratio=std::clamp((event->position().x()-plot.left())/plot.width(),0.,1.);
        const double vertical=(event->position().y()-plot.top())/plot.height();
        if(mode==View::Waveform){setToolTip(QLocale().toString(timeAt(ratio),'f',2)+" s / "+QLocale().toString((1-2*vertical)*waveLimit_,'f',3));return;}
        if(mode==View::Waterfall){const int lanes=data->settings.layout==2 && data->settings.channel==0?2:1;const double lanePosition=vertical*lanes-std::floor(vertical*lanes);setToolTip(QLocale().toString(timeAt(ratio),'f',2)+" s / "+formatFrequency(frequency(1-lanePosition)));return;}
        if(mode==View::Vectorscope || mode==View::Tuner)return;
        const double hz=frequency(ratio),level=data->settings.amplitude && !relativeAxis_?axisTop_+20*std::log10(std::max(1e-12,1-vertical)):axisTop_-vertical*axisSpan_;
        setToolTip(formatFrequency(hz)+" / "+QLocale().toString(level,'f',1)+(relativeAxis_?" dB":" dBFS"));
    }
    void paintEvent(QPaintEvent *) override {
        struct PaintTimer {ScopeCanvas *canvas;QElapsedTimer timer;explicit PaintTimer(ScopeCanvas *c):canvas(c){timer.start();}
            ~PaintTimer(){canvas->paintMs_=.8*canvas->paintMs_+.2*timer.nsecsElapsed()/1e6;}} paintTimer(this);
        QPainter p(this);p.setLayoutDirection(Qt::LeftToRight);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#111827"));
        const auto plot=plotRect();const auto &s=data->settings;
        const auto relative=data->relativeDb();
        const bool comparison=mode==View::Microphone && !relative.empty();
        double span=s.sensitivity,top=displayCeiling;
        if(comparison){
            double extent=0;for(double value:relative)if(std::isfinite(value))extent=std::max(extent,std::abs(value));
            if(s.autoRange){spanHeld_=holdRange(spanHeld_,2*std::max(12.,std::ceil((extent+2)/3)*3),spanSince_);span=spanHeld_;}
            top=span/2;
        }
        axisTop_=top;axisSpan_=span;relativeAxis_=comparison;
        waveLimit_=std::pow(10.,s.ceiling/20.);
        if(s.autoRange && mode==View::Waveform){
            // Quantized to half-octave steps with hysteresis, so the vertical scale changes rarely instead of flickering with every window.
            const int channel=s.layout==1?(s.channel==1?0:s.channel==2?1:2):-1;double extent=0;
            for(int j=std::max(0,int(waveCols_.size())-scrollBack_-waveCap_);j<int(waveCols_.size())-scrollBack_;++j){const auto &column=waveCols_[std::size_t(j)];for(int c=0;c<3;++c)if(channel<0?c<2:c==channel)extent=std::max({extent,double(std::abs(column.lo[c])),double(std::abs(column.hi[c]))});}
            const double target=std::pow(2.,std::ceil(std::log2(std::max(.001,extent*1.1))*2)/2);
            if(target>waveLimit_||target<waveLimit_/1.5)waveLimitHeld_=target;
            waveLimit_=waveLimitHeld_;
        } else if(s.autoRange && mode==View::Vectorscope){
            double extent=0;for(double value:data->left)extent=std::max(extent,std::abs(value));for(double value:data->right)extent=std::max(extent,std::abs(value));waveLimit_=std::max(.001,extent*1.1);
        }

        if(mode==View::Waterfall){
            ensureImageSize();
            QImage q(reinterpret_cast<const uchar *>(image.pixels.data()),image.width,image.height,image.width*4,QImage::Format_RGB32);
            if(scrollBack_>0){ // compose archive + live image shifted back in time
                QImage composed(image.width,image.height,QImage::Format_RGB32);const int archived=int(archive_.size());
                for(int x=0;x<image.width;++x){const int index=archived+x-scrollBack_;
                    for(int y=0;y<image.height;++y)reinterpret_cast<std::uint32_t *>(composed.scanLine(y))[x]=index<0?0xff000000u:index<archived?archive_[std::size_t(index)][std::size_t(y)]:image.pixels[std::size_t(y*image.width+(index-archived))];}
                q=composed;
            }
            p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(plot,q);
        }
        p.setPen(QColor("#9bacc2"));
        if(mode!=View::Tuner && mode!=View::Vectorscope){
            const QString unit=mode==View::Waterfall?QString():mode==View::Waveform?QString():comparison?QString("dB"):s.amplitude?QString("×"):QString("dBFS");
            p.drawText(QRectF(2,0,52,17),Qt::AlignRight,unit);
            if(s.amplitude && !comparison && mode!=View::Waveform && mode!=View::Waterfall)
                p.drawText(QRectF(plot.left(),0,plot.width(),17),Qt::AlignRight,"1 = "+displayNumber(top)+" dBFS");
        }
        if(mode==View::Waterfall) {
            const int lanes=s.layout==2 && s.channel==0?2:1;
            for(int lane=0;lane<lanes;++lane){const double laneTop=plot.top()+lane*plot.height()/lanes,laneBottom=laneTop+plot.height()/lanes;
                for(double hz:frequencyTicks(plot.height()/lanes,28.)){
                    const double y=laneBottom-ratio(hz)*plot.height()/lanes;
                    p.setPen(QColor("#9bacc2"));
                    p.drawText(QRectF(0,y-9,plot.left()-8,18),Qt::AlignRight,formatFrequency(hz));
                    p.drawText(QRectF(plot.right()+5,y-9,width()-plot.right()-7,18),Qt::AlignLeft,formatFrequency(hz));
                    if(s.grid){p.setPen(QColor("#475569"));p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));}
                }
                if(lanes==2)p.drawText(QPointF(plot.left()+5,laneTop+15),lane==0?SC_TR("L"):SC_TR("R"));
            }
            p.setPen(QColor("#9bacc2"));
            for(double r:{0.,.25,.5,.75,1.})p.drawText(QRectF(plot.left()+r*plot.width()-45,plot.bottom()+3,90,18),Qt::AlignCenter,displayNumber(timeAt(r))+" s");
            p.drawText(QRectF(plot.left(),plot.bottom()+20,plot.width(),18),Qt::AlignLeft,displayNumber(displayCeiling-s.sensitivity)+" … "+displayNumber(displayCeiling)+" dBFS");
        } else if(mode!=View::Tuner && mode!=View::Vectorscope){
            for(double r:{0.,.25,.5,.75,1.}) {
                const double y=plot.bottom()-r*plot.height();
                const double value=mode==View::Waveform?(2*r-1)*waveLimit_:(s.amplitude && !comparison)?r:top-span*(1-r);
                p.drawText(QRectF(0,y-9,plot.left()-8,18),Qt::AlignRight,displayNumber(value));
                if(s.grid){p.setPen(QColor("#334155"));p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));p.setPen(QColor("#9bacc2"));}
                if(mode==View::Waveform){
                    const double x=plot.left()+r*plot.width();
                    p.drawText(QRectF(x-45,plot.bottom()+3,90,18),Qt::AlignCenter,displayNumber(timeAt(r))+" s");
                    if(s.grid){p.setPen(QColor("#334155"));p.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));p.setPen(QColor("#9bacc2"));}
                }
            }
            if(mode!=View::Waveform)for(double hz:frequencyTicks(plot.width(),92.)){
                const double x=plot.left()+ratio(hz)*plot.width();
                p.setPen(QColor("#9bacc2"));p.drawText(QRectF(x-45,plot.bottom()+3,90,18),Qt::AlignCenter,formatFrequency(hz));
                if(s.grid){p.setPen(QColor("#334155"));p.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));}
            }
            p.setPen(QColor("#9bacc2"));
            p.drawText(QRectF(plot.left(),plot.bottom()+20,plot.width(),18),Qt::AlignCenter,QString());
        }
        p.save();p.setClipRect(plot.adjusted(-1,-1,1,1));
        if(mode==View::Waveform) {
            // Envelope columns right-aligned on the plot; non-antialiased vertical lines (the old antialiased path cost ~430 ms per frame).
            p.setRenderHint(QPainter::Antialiasing,false);
            const int n=int(waveCols_.size())-scrollBack_,firstColumn=std::max(0,n-waveCap_);const double columnWidth=plot.width()/std::max(1,waveCap_),center=plot.center().y(),scale=plot.height()/(2*waveLimit_);
            auto envelope=[&](int channel,const QColor &color){
                p.setPen(QPen(color,std::max(1.,columnWidth),Qt::SolidLine,Qt::FlatCap));
                for(int j=firstColumn;j<n;++j){const auto &column=waveCols_[std::size_t(j)];const double x=plot.right()-(n-1-j+.5)*columnWidth;
                    const double y1=center-column.hi[channel]*scale,y2=center-column.lo[channel]*scale;p.drawLine(QPointF(x,y1),QPointF(x,std::max(y2,y1+1.)));}};
            if(s.layout==1)envelope(s.channel==1?0:s.channel==2?1:2,QColor("#55d7c3"));
            else {envelope(0,QColor("#55d7c3"));envelope(1,QColor("#e6b450"));}
        } else if(mode==View::Vectorscope){
            const double radius=std::min(plot.width(),plot.height())*.45;const QPointF c=plot.center();p.setPen(QColor("#475569"));p.drawEllipse(c,radius,radius);p.drawLine(QPointF(c.x()-radius,c.y()),QPointF(c.x()+radius,c.y()));p.drawLine(QPointF(c.x(),c.y()-radius),QPointF(c.x(),c.y()+radius));
            p.setPen(QPen(QColor("#55d7c3"),1));QPainterPath path;for(std::size_t i=0;i<data->left.size();++i){QPointF point(c.x()+(data->left[i]-data->right[i])*.5*radius/waveLimit_,c.y()-(data->left[i]+data->right[i])*.5*radius/waveLimit_);if(i==0)path.moveTo(point);else path.lineTo(point);}p.drawPath(path);
            p.drawText(plot.adjusted(0,0,0,-4),Qt::AlignBottom|Qt::AlignHCenter,QString::fromUtf8("ρ ")+QLocale().toString(data->correlation,'f',2));
        } else if(mode==View::Tuner){
            // Scrolling note display on top (time runs left to right, newest at the right), then the chord (optional),
            // the detected note and the tuning gauge. Uses the whole canvas.
            p.setClipping(false);
            const auto area=tunerArea();
            const auto pitch=data->pitch;const auto chord=data->chord;
            const bool hasPitch=pitch.midi>=0 && pitch.hz>0;   // the loudest note, shown even while a chord is named
            const double keysWidth=34,gutterWidth=44;
            const QRectF roll=tunerRoll();
            const int rows=rollSpan;
            if(rollLowManual<0){   // follow the music: keep the window while the recent notes sit inside it, else re-centre (C-aligned)
                std::array<double,128> weight{};double total=0;for(const auto &frame:data->noteHistory)if(data->noteClock-frame.time<3)for(const auto &n:frame.notes){weight[std::size_t(n.midi)]+=n.level;total+=n.level;}
                const int current=std::clamp(rollLowAuto,36,85-rows);rollLowAuto=current;
                if(total>0){   // level-weighted median, so stray blips cannot drag the window
                    double run=0;int median=current+rows/2;for(int m=0;m<128;++m){run+=weight[std::size_t(m)];if(run>=total/2){median=m;break;}}
                    if(median<current+2 || median>=current+rows-2)rollLowAuto=std::clamp((median-rows/2)/12*12,36,85-rows);
                }
            }
            const int lowMidi=std::clamp(rollLowNow(),36,85-rows);const double rowHeight=roll.height()/rows;
            if(rollBar)QMetaObject::invokeMethod(rollBar,[bar=rollBar,rows,lowMidi]{if(!bar->isVisible())return;const QSignalBlocker block(bar);const int range=std::max(0,49-rows);bar->setRange(0,range);bar->setPageStep(rows);bar->setValue(std::clamp(85-rows-lowMidi,0,range));},Qt::QueuedConnection);
            static const bool black[12]={false,true,false,true,false,false,true,false,true,false,true,false};
            p.fillRect(roll,QColor("#0f1722"));
            for(int m=lowMidi;m<lowMidi+rows;++m){const double y=roll.bottom()-(m-lowMidi+1)*rowHeight;if(black[m%12])p.fillRect(QRectF(roll.left(),y,roll.width(),rowHeight),QColor("#16202d"));
                if(m%12==0){p.setPen(QColor("#30445b"));p.drawLine(QPointF(roll.left(),y+rowHeight),QPointF(roll.right(),y+rowHeight));}
                if(m%12==0 || rowHeight>=11){p.setPen(QColor(m%12==0?"#cbd5e1":"#9fb0c3"));p.drawText(QRectF(area.left(),y+rowHeight/2-8,keysWidth-4,16),Qt::AlignRight|Qt::AlignVCenter,noteName(m));}}
            const double seconds=std::clamp(s.history,5.,10.);const double now=data->noteClock;
            double previous=-1;
            for(const auto &frame:data->noteHistory){
                const double age=now-frame.time;if(age>seconds){previous=frame.time;continue;}
                const double x=roll.right()-age/seconds*roll.width();const double width=std::max(2.,((previous<0?.05:frame.time-previous)/seconds)*roll.width()+1);previous=frame.time;
                for(const auto &n:frame.notes){if(n.midi<lowMidi || n.midi>=lowMidi+rows)continue;QColor color("#55d7c3");color.setAlpha(int(90+165*n.level));const double y=roll.bottom()-(n.midi-lowMidi+1)*rowHeight;p.fillRect(QRectF(x-width,y+1,width,std::max(2.,rowHeight-2)),color);}
            }
            p.setPen(QColor("#30445b"));p.drawRect(roll);
            if(!data->noteHistory.empty()){   // names of the notes sounding now, beside the newest column
                p.setPen(QColor("#e6b450"));for(const auto &n:data->noteHistory.back().notes){if(n.midi<lowMidi || n.midi>=lowMidi+rows)continue;const double y=roll.bottom()-(n.midi-lowMidi+1)*rowHeight;p.drawText(QRectF(roll.right()+4,y+rowHeight/2-8,gutterWidth-4,16),Qt::AlignLeft|Qt::AlignVCenter,noteName(n.midi));}
            }
            auto font=p.font();font.setPointSizeF(15);p.setFont(font);
            const QRectF chordRow(area.left(),roll.bottom()+2,area.width(),24);
            if(showChords && chord.root>=0){static const char *names[]={"C","C♯","D","D♯","E","F","F♯","G","G♯","A","A♯","B"};p.setPen(QColor("#e6b450"));p.drawText(chordRow,Qt::AlignCenter,QString::fromUtf8(names[chord.root])+QString::fromLatin1(chordQualitySuffix(chord.quality)));}
            const QRectF noteArea(area.left(),chordRow.bottom(),area.width(),std::max(24.,area.bottom()-chordRow.bottom()-56));
            font.setPointSizeF(18);p.setFont(font);
            const QColor noteColor(data->held?"#6b7f95":"#55d7c3");   // dimmed while the last reading is being held
            if(hasPitch){p.setPen(noteColor);p.drawText(noteArea,Qt::AlignCenter,noteName(pitch.midi));}
            else{p.setPen(QColor("#6b7f95"));p.drawText(noteArea,Qt::AlignCenter,QString::fromUtf8("—"));}
            font.setPointSizeF(11);p.setFont(font);p.setPen(noteColor);
            const double gaugeY=area.bottom()-42;   // the gauge line is always drawn so the layout never jumps
            p.setPen(QColor("#30445b"));p.drawLine(QPointF(area.center().x()-area.width()*.4,gaugeY),QPointF(area.center().x()+area.width()*.4,gaugeY));p.drawLine(QPointF(area.center().x(),gaugeY-8),QPointF(area.center().x(),gaugeY+8));p.setPen(noteColor);
            if(hasPitch){
                p.drawText(area.adjusted(0,0,0,-2),Qt::AlignBottom|Qt::AlignHCenter,formatFrequency(pitch.hz)+" / "+QLocale().toString(pitch.cents,'f',1)+" ¢");
                const double x=area.center().x()+std::clamp(pitch.cents,-50.,50.)/50.*area.width()*.4;
                p.setPen(QPen(noteColor,2));p.drawLine(QPointF(x,gaugeY-14),QPointF(x,gaugeY+14));p.setPen(noteColor);
            }
            static const QStringList tunings={"", "E2 A2 D3 G3 B3 E4","E1 A1 D2 G2","G4 C4 E4 A4","G3 D4 A4 E5","C3 G3 D4 A4","C2 G2 D3 A3","G3 D4 A4 E5",""};
            p.setPen(QColor("#6b7f95"));p.drawText(area.adjusted(0,0,0,-22),Qt::AlignBottom|Qt::AlignHCenter,tunings[s.instrument]);
        } else if(mode!=View::Waterfall){
            auto trace=[&](const std::vector<double> &values,const QColor &color,Qt::PenStyle style=Qt::SolidLine){if(values.empty())return;QPainterPath path;bool connected=false;
                for(int x=0;x<=int(plot.width());++x){const double hz=frequency(double(x)/plot.width());const double bin=hz*s.fft/s.rate;const int i=std::clamp(int(bin),0,int(values.size())-1),j=std::min(i+1,int(values.size())-1);const double db=values[i]+(values[j]-values[i])*(bin-i);
                    if(!std::isfinite(db)){connected=false;continue;}
                    const double fraction=(s.amplitude && !comparison)?std::pow(10.,(db-top)/20.):(db-(top-span))/span;const double y=plot.bottom()-fraction*plot.height();
                    if(!connected){path.moveTo(plot.left()+x,y);connected=true;}else path.lineTo(plot.left()+x,y);}
                p.setPen(QPen(color,1.8,style));p.drawPath(path);};
            const auto &main=comparison?relative:(mode==View::Average || mode==View::Microphone)?data->averaged:data->db;
            trace(main,QColor("#55d7c3"));
            if(mode==View::Spectrum && s.layout!=1 && s.channel==0){trace(data->dbLeft,QColor("#72a7ff"));trace(data->dbRight,QColor("#e6b450"));}
            if(s.average && mode==View::Spectrum)trace(data->averaged,QColor("#e6b450"));
            if(s.peaks && !comparison)trace(data->peak,QColor("#f28d9b"),Qt::DashLine);
            if(comparison)trace(std::vector<double>(relative.size(),0),QColor("#cbd5e1"),Qt::DotLine);
            else if(data->hasReference())trace(data->reference,QColor("#cbd5e1"),Qt::DotLine);
            if(s.notes){for(int midi=12;midi<=127;++midi){double hz=440*std::pow(2.,(midi-69)/12.);if(hz<s.low || hz>s.upper())continue;double r=ratio(hz);p.setPen(QColor("#30445b"));p.drawLine(QPointF(plot.left()+r*plot.width(),plot.top()),QPointF(plot.left()+r*plot.width(),plot.bottom()));if(midi%12==0)p.drawText(QPointF(plot.left()+r*plot.width()+2,plot.top()+14),noteName(midi));}}
        }
        p.restore();
        if(s.transients && data->transient){p.setPen(QPen(QColor("#f28d9b"),3));p.drawLine(plot.topLeft(),plot.topRight());}
        if(s.music && mode!=View::Tuner){p.setPen(QColor("#e6b450"));p.drawText(plot.adjusted(6,3,-6,-3),Qt::AlignTop|Qt::AlignRight,noteName(data->pitch.midi));}
    }
private:
    gram::Image image;
    void ensureImageSize() {
        const auto plot=plotRect();const double dpr=devicePixelRatioF();
        const int w=std::clamp(int(std::ceil(plot.width()*dpr)),1,1280),h=std::clamp(int(std::ceil(plot.height()*dpr)),2,1280);
        if(image.width==w && image.height==h)return;
        archive_.clear();scrollBack_=0;
        auto replacement=gram::make_image(w,h);gram::clear_image(replacement,0xff000000);
        if(!image.pixels.empty()) {
            QImage old(reinterpret_cast<const uchar *>(image.pixels.data()),image.width,image.height,image.width*4,QImage::Format_RGB32);
            const auto scaled=old.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
            for(int y=0;y<h;++y)std::memcpy(replacement.pixels.data()+y*w,scaled.constScanLine(y),std::size_t(w)*4);
        }
        image=std::move(replacement);
    }
    QElapsedTimer clock;
    double computeMs_=4,paintMs_=4;
    double carryMs_=0;
    QByteArray latest_;qint64 latestEnd_=0;double nextEnd_=0;bool started_=false;QElapsedTimer sinceFrame_,sinceArrival_,silenceClock_;
    static constexpr qint64 kStallMs=250; // no audio for this long = silence: the spectrogram keeps scrolling (black) in real time
    static constexpr double kTickSlackMs=4.;
    std::deque<WaveColumn> waveCols_;int waveCap_=0;
    // Auto range grows at once (so nothing clips) but only shrinks after the signal has stayed lower for a while, so it cannot bounce.
    static double holdRange(double current,double target,QElapsedTimer &lowerSince){
        if(target>current || current-target<3){lowerSince.invalidate();return target>current?target:current;}
        if(!lowerSince.isValid()){lowerSince.start();return current;}
        if(lowerSince.elapsed()<3000)return current;
        lowerSince.invalidate();return target;}
    // History kept beyond the visible window so a frozen display can be scrolled back in time.
    static constexpr int kKeepScreens=4;
    std::deque<std::vector<std::uint32_t>> archive_;int scrollBack_=0;bool timeDragging_=false;int dragBack_=0;
    void archiveColumns(int count){
        count=std::min(count,image.width);
        for(int c=0;c<count;++c){std::vector<std::uint32_t> column(std::size_t(image.height));for(int y=0;y<image.height;++y)column[std::size_t(y)]=image.pixels[std::size_t(y*image.width+c)];archive_.push_back(std::move(column));}
        while(int(archive_.size())>image.width*(kKeepScreens-1))archive_.pop_front();
    }
    int maxScrollBack() const {return mode==View::Waterfall?int(archive_.size()):mode==View::Waveform?std::max(0,int(waveCols_.size())-waveCap_):0;}
    double columnsPerSecondNow() const {return std::max(1e-9,property("columnsPerSecond").toDouble());}
    // Seconds (negative = past) at horizontal position r of the plot, including any scroll-back.
    double timeAt(double r) const {const auto &s=data->settings;return -(1-r)*s.history/s.speed-(scrollBack_>0?scrollBack_/columnsPerSecondNow():0.);}
    void setScrollBack(int columns){
        const int clamped=std::clamp(columns,0,maxScrollBack());
        if(clamped==scrollBack_)return;
        const bool wasLive=scrollBack_==0;scrollBack_=clamped;setProperty("scrollBack",scrollBack_);if(wasLive && clamped>0 && onScroll)onScroll(true);update();
    }
    bool dragging_=false;QPointF dragStart_;Settings dragSettings_;QElapsedTimer dragClock_;
    QElapsedTimer ceilingSince_,spanSince_;double spanHeld_=24;
    double credit=0,axisTop_=0,axisSpan_=90,waveLimit_=1,waveLimitHeld_=1;
    bool relativeAxis_=false;
    QRectF plotRect() const {const int margin=std::max(70,fontMetrics().horizontalAdvance(formatFrequency(999.999))+12);return QRectF(margin,18,std::max(1,width()-2*margin),std::max(1,height()-62));}
    std::vector<double> frequencyTicks(double pixels,double spacing) const {return frequencyAxisTicks(data->settings,pixels,spacing);}
    double frequency(double r) const {const auto &s=data->settings;return s.scale==0?s.low+r*(s.upper()-s.low):std::max(1.,s.low)*std::pow(s.upper()/std::max(1.,s.low),r);}
    double ratio(double hz) const {const auto &s=data->settings;return s.scale==0?(hz-s.low)/(s.upper()-s.low):std::log(hz/std::max(1.,s.low))/std::log(s.upper()/std::max(1.,s.low));}
};
Panel::Panel(QWidget *parent):QWidget(parent) {
    setObjectName("analysisPanel");auto *root=new QVBoxLayout(this);root->setContentsMargins(12,8,12,8);
    auto *top=new soundcurrent::ui::ResponsiveRow;view_=new QComboBox;view_->setObjectName("analysisView");view_->setAccessibleName(SC_TR("View"));
    const QStringList views={SC_TR("Spectrogram"),SC_TR("Spectrum"),SC_TR("Waveform"),SC_TR("Tuner"),SC_TR("Vectorscope"),SC_TR("Averaged spectrum"),SC_TR("Microphone response")};
    for(int i=0;i<views.size();++i)view_->addItem(views[i],i);
    source_=new QComboBox;source_->setObjectName("analysisSource");source_->setAccessibleName(SC_TR("Microphone"));source_->addItem(SC_TR("Playback monitor"),QString());
    for(auto *combo:{view_,source_}){combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setMinimumContentsLength(8);combo->setMinimumWidth(0);combo->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);}
    freeze_=new QCheckBox(SC_TR("Freeze display"));freeze_->setObjectName("analysisFreeze");settingsButton_=new QPushButton(SC_TR("Settings"));settingsButton_->setObjectName("analysisSettings");settingsButton_->setCheckable(true);auto *settings=settingsButton_;
    top->addWidget(view_,1);top->addWidget(source_,1);top->addWidget(freeze_);top->addWidget(settings);root->addWidget(top);
    buildSettings(root);
    nowPlaying_=new QLabel;nowPlaying_->setObjectName("analysisNowPlaying");nowPlaying_->setTextFormat(Qt::PlainText);nowPlaying_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);nowPlaying_->setVisible(false);root->addWidget(nowPlaying_);
    nowPlayingTimer_=new QTimer(this);nowPlayingTimer_->setInterval(2000);connect(nowPlayingTimer_,&QTimer::timeout,this,&Panel::updateNowPlaying);
    showNowPlaying_=QSettings().value("analysis/nowPlaying",false).toBool();
    canvas_=new ScopeCanvas(this);canvas_->setObjectName("analysisCanvas");canvas_->showChords=QSettings().value("analysis/showChords",true).toBool();root->addWidget(canvas_,1);readout_=new QLabel;readout_->setTextFormat(Qt::PlainText);readout_->setWordWrap(true);readout_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);readout_->setFixedHeight(2*fontMetrics().lineSpacing());root->addWidget(readout_);
    help_=new QLabel(SC_TR("Reference comparisons include the source, room and reference microphone. They are not an isolated or certified microphone measurement."));help_->setWordWrap(true);root->addWidget(help_);
    auto *actions=new soundcurrent::ui::ResponsiveRow;auto *resetButton=new QPushButton(SC_TR("Reset"));auto *saveImageButton=new QPushButton(SC_TR("Save image"));saveImageButton->setObjectName("analysisSaveImage");reference_=new QPushButton(SC_TR("Capture reference"));profile_=new QPushButton(SC_TR("Create microphone profile"));
    resetButton->setObjectName("analysisReset");reference_->setObjectName("analysisReference");profile_->setObjectName("analysisCreateProfile");
    actions->addWidget(resetButton);actions->addWidget(saveImageButton);actions->addWidget(reference_);actions->addWidget(profile_);root->addWidget(actions);
    const auto saved=Settings::parse(QJsonDocument::fromJson(QSettings().value("analysis/settings").toByteArray()).object());if(saved)canvas_->data->configure(*saved);
    // The ~60 Hz heartbeat only runs while the analysis view is actually showing (no idle wake-ups otherwise).
    auto *displayHeartbeat=new QTimer(this);displayHeartbeat->setInterval(16);displayHeartbeat->setTimerType(Qt::PreciseTimer);connect(displayHeartbeat,&QTimer::timeout,canvas_,[this]{canvas_->advanceIdle();});
    canvas_->onVisibility=[this,displayHeartbeat](bool visible){if(visible){captureFailed_=false;refreshSources();displayHeartbeat->start();}else displayHeartbeat->stop();if(visible && showNowPlaying_){updateNowPlaying();nowPlayingTimer_->start();}else nowPlayingTimer_->stop();if(onActivity)onActivity(source(),visible && active());};
    canvas_->onFrame=[this]{availability();};
    canvas_->onZoom=[this](const Settings &s){if(applySettings(s))feedback_->clear();};
    if(canvas_->isVisible())displayHeartbeat->start();
    connect(view_,&QComboBox::currentIndexChanged,this,[this](int index){canvas_->data->resetAverage();setView(View(index));canvas_->clearImage();canvas_->setAccessibleName(view_->currentText());availability();});
    connect(source_,&QComboBox::currentIndexChanged,this,[this]{sourceReady_=true;captureFailed_=false;reset();QSettings().setValue("analysis/source",source());QSettings().setValue("analysis/sourceName",source_->currentText());if(onActivity)onActivity(source(),active());});
    connect(freeze_,&QCheckBox::toggled,this,[this](bool on){canvas_->frozen=on;if(!on)canvas_->resumeLive();});
    canvas_->onScroll=[this](bool back){if(back)freeze_->setChecked(true);else freeze_->setChecked(false);};
    connect(settings,&QPushButton::toggled,this,[this](bool on){if(settingsPanel_)settingsPanel_->setVisible(on);});
    connect(resetButton,&QPushButton::clicked,this,&Panel::reset);
    connect(saveImageButton,&QPushButton::clicked,this,[this]{const auto path=i18n::FileDialogs::getSaveFileName(this,SC_TR("Save image"),{},"PNG (*.png)");if(!path.isEmpty())saveImage(path.endsWith(".png",Qt::CaseInsensitive)?path:path+".png");});
    connect(reference_,&QPushButton::clicked,this,[this]{captureReference();});
    connect(profile_,&QPushButton::clicked,this,[this]{if(auto p=microphoneProfile()){suspendForDialog(true);if(onProfile)onProfile(*p);suspendForDialog(false);}else QMessageBox::information(this,SC_TR("Microphone response"),SC_TR("Not enough broadband reference data."));});
    setView(View(std::clamp(QSettings().value("analysis/view",saved?saved->view:1).toInt(),0,6)));canvas_->setAccessibleName(view_->currentText());availability();
    if(writeControls_)writeControls_(canvas_->data->settings);
}
Panel::~Panel(){canvas_->onVisibility={};canvas_->onFrame={};onActivity={};}
bool Panel::active() const{return (source().isEmpty() || soundcurrent::microphone::allowed()) && canvas_->isVisible() && sourceReady_ && !captureFailed_ && !suspended_;}
QString Panel::source() const{return source_->currentData().toString();}
Engine &Panel::engine(){return *canvas_->data;}
QSize Panel::historyPixelSize() const{return canvas_->historyPixelSize();}
View Panel::view() const{return canvas_->mode;}
void Panel::setView(View mode){
    const int index=int(mode);if(index<0 || index>=view_->count())return;
    if(view_->currentIndex()!=index){view_->setCurrentIndex(index);return;}
    canvas_->mode=mode;canvas_->data->settings.view=index;canvas_->syncRollBar();
    QSettings settings;settings.setValue("analysis/view",index);
    settings.setValue("analysis/settings",QJsonDocument(canvas_->data->settings.json()).toJson(QJsonDocument::Compact));
}
void Panel::pushPcm(const QByteArray &bytes,qint64 endFrame){if(sourceReady_ && !captureFailed_ && !suspended_)canvas_->push(bytes,endFrame);}
void Panel::captureError(const QString &error){captureFailed_=true;readout_->setText(error);if(onActivity)onActivity(source(),false);}
void Panel::refreshSources(){
    if(!microphoneSources)return;
    const auto oldSource=source();const auto wanted=oldSource.isEmpty()?QSettings().value("analysis/source").toString():oldSource;
    auto choices=microphoneSources();bool ready=wanted.isEmpty();for(const auto &choice:choices)if(choice.first==wanted)ready=true;
    if(!ready){auto name=QSettings().value("analysis/sourceName",SC_TR("Microphone")).toString();choices.append({wanted,name+" — "+SC_TR("No microphone connected.")});}
    bool changed=source_->count()!=choices.size()+1;
    for(int i=0;!changed && i<choices.size();++i)changed=source_->itemData(i+1).toString()!=choices[i].first || source_->itemText(i+1)!=choices[i].second;
    if(changed){
        const QSignalBlocker blocker(source_);source_->clear();source_->addItem(SC_TR("Playback monitor"),QString());
        for(const auto &choice:choices)source_->addItem(choice.second,choice.first);
        source_->setCurrentIndex(std::max(0,source_->findData(wanted)));
        if(!ready)if(auto *model=qobject_cast<QStandardItemModel *>(source_->model()))model->item(source_->currentIndex())->setEnabled(false);
    }
    const bool access=soundcurrent::microphone::allowed();
    if(auto *model=qobject_cast<QStandardItemModel *>(source_->model()))
        for(int i=1;i<source_->count();++i)model->item(i)->setEnabled(access && (source_->itemData(i).toString()!=wanted || ready));
    ready=ready && (wanted.isEmpty() || access);
    if(ready!=sourceReady_ || oldSource!=source()){
        sourceReady_=ready;captureFailed_=false;reset();if(onActivity)onActivity(source(),active());
    }
    if(!ready)readout_->setText(access?SC_TR("No microphone connected."):SC_TR("Microphone access is disabled."));
    availability();
}
bool Panel::applySettings(const Settings &s){if(!s.valid())return false;canvas_->data->configure(s);setView(View(s.view));canvas_->clearImage();QSettings().setValue("analysis/settings",QJsonDocument(s.json()).toJson(QJsonDocument::Compact));if(writeControls_)writeControls_(s);availability();return true;}
bool Panel::saveImage(const QString &path){return canvas_->grab().save(path,"PNG");}
void Panel::reset(){const bool retry=captureFailed_;captureFailed_=false;engine().resetAverage();canvas_->clearImage();availability();if(retry && onActivity)onActivity(source(),active());}
bool Panel::captureReference(){if(!source().isEmpty() && !soundcurrent::microphone::allowed())return false;if(engine().frames<25 || engine().averaged.empty())return false;referenceSource_=source_->currentText();engine().captureReference();canvas_->clearImage();availability();return true;}
std::optional<equipment::Profile> Panel::microphoneProfile() const {
    const auto &e=*canvas_->data;if(!soundcurrent::microphone::allowed() || source().isEmpty() || view()!=View::Microphone || e.frames<25 || e.reference.size()!=e.averaged.size())return {};
    const auto relative=e.relativeDb();
    if(std::max(20.,e.settings.low)>=std::min(20000.,e.settings.upper()))return {};
    int first=-1,last=-1,excited=0;
    const double low=std::max(20.,e.settings.low),high=std::min(20000.,e.settings.upper());
    for(int bin=1;bin<int(relative.size());++bin)if(e.frequencyAt(bin)>=low && e.frequencyAt(bin)<=high && std::isfinite(relative[bin])){
        if(first<0)first=bin;last=bin;++excited;
    }
    if(excited<6 || e.frequencyAt(last)<4*e.frequencyAt(first))return {};
    equipment::Profile p;p.kind="microphone";p.custom=true;p.brand=SC_TR("Custom");p.family=SC_TR("Microphone");p.model=source_->currentText();
    p.source=referenceSource_;p.conditions=SC_TR("Reference comparisons include the source, room and reference microphone. They are not an isolated or certified microphone measurement.");p.provenance=p.conditions;p.estimated=true;
    p.estimation={{"method","relative-microphone-reference-db-v1"},{"createdUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"limitations",p.conditions},
        {"referenceSource",referenceSource_},{"targetSource",source_->currentText()},
        {"interpretation","combined-residual"},{"sampleRate",e.settings.rate},{"fftSize",e.settings.fft},
        {"averagedFrames",double(e.frames)},{"distinctExcitedBins",excited},
        {"lowHz",e.frequencyAt(first)},{"highHz",e.frequencyAt(last)}};

    int previous=-1;
    for(int i=0;i<96;++i){
        const double hz=e.frequencyAt(first)*std::pow(e.frequencyAt(last)/e.frequencyAt(first),i/95.);
        const int bin=std::clamp(int(std::lround(hz*e.settings.fft/e.settings.rate)),first,last);
        if(bin==previous || !std::isfinite(relative[bin]))continue;
        previous=bin;p.response.append({e.frequencyAt(bin),relative[bin]});
    }
    if(p.response.size()<6 || p.response.back().frequency/p.response.front().frequency<4)return {};
    p.filters=equipment::fitResponse(p.response);return p;
}
void Panel::availability(){
    if(!source().isEmpty() && !soundcurrent::microphone::allowed()){
        reference_->setEnabled(false);profile_->setEnabled(false);
        readout_->setText(SC_TR("Microphone access is disabled."));return;
    }
    const auto &e=engine();const bool microphone=view()==View::Microphone;help_->setVisible(microphone);reference_->setEnabled(e.frames>=25);reference_->setVisible(view()==View::Average || microphone);profile_->setVisible(microphone);profile_->setEnabled(microphone && !source().isEmpty() && e.frames>=25 && e.hasReference());
    readout_->setText(SC_TR("Frames: %1 · FFT: %2 ms · Resolution: %3 · Window: %4 ms").arg(QLocale().toString(e.waveFrames),QLocale().toString(e.fftMs,'f',2),formatFrequency(double(e.settings.rate)/e.settings.fft),QLocale().toString(e.settings.fft*1000./e.settings.rate,'f',1)));
}
void Panel::suspendForDialog(bool suspended){suspended_=suspended;canvas_->resetClock();if(onActivity)onActivity(source(),active());}
void Panel::updateNowPlaying(){
    // Off by default: titles are personal, and the Analysis tab may be screenshotted or recorded.
    const QString text=showNowPlaying_?soundcurrent::nowPlayingText():QString();
    nowPlaying_->setText(text.isEmpty()?QString():SC_TR("Now playing: %1").arg(text));nowPlaying_->setToolTip(text);nowPlaying_->setVisible(!text.isEmpty());
}
void Panel::toggleSettings(){settingsButton_->setChecked(!settingsButton_->isChecked());}
bool Panel::settingsVisible() const{return settingsPanel_ && settingsPanel_->isVisibleTo(const_cast<Panel *>(this));}
// Everyday controls sit in a row on the tab; the rest live in a collapsible, scrollable panel. Every control applies live
// (debounced), so the effect is visible without closing anything. Invalid combinations are reported and not applied.
void Panel::buildSettings(QVBoxLayout *root){
    const Settings current=canvas_?canvas_->data->settings:Settings();
    auto *quick=new soundcurrent::ui::ResponsiveRow;
    auto quickCombo=[&](const QString &label,const QString &name,const QStringList &items,int selected){auto *c=new QComboBox;c->setObjectName(name);c->setAccessibleName(label);c->setToolTip(label);c->addItems(items);c->setCurrentIndex(selected);c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);c->setMinimumContentsLength(6);c->setMinimumWidth(0);c->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);quick->addWidget(c,1);return c;};
    auto *channel=quickCombo(SC_TR("Channel"),"analysisChannel",{SC_TR("Mix"),SC_TR("Left"),SC_TR("Right")},current.channel);
    auto *palette=quickCombo(SC_TR("Palette"),"analysisPalette",{SC_TR("Spectrum"),SC_TR("Gray"),SC_TR("Blue"),SC_TR("Green"),SC_TR("Amber"),SC_TR("Ice")},current.palette);
    auto *scale=quickCombo(SC_TR("Frequency scale"),"analysisScale",{SC_TR("Linear"),SC_TR("Logarithmic"),SC_TR("Octave")},current.scale);
    // The Tuner view also offers the Instrument choice here (the same setting as Instrument in the Settings panel).
    const QStringList instrumentNames={SC_TR("Chromatic"),SC_TR("Guitar"),SC_TR("Bass guitar"),SC_TR("Ukulele"),SC_TR("Violin"),SC_TR("Viola"),SC_TR("Cello"),SC_TR("Mandolin"),SC_TR("Voice")};
    auto *instrumentQuick=quickCombo(SC_TR("Instrument"),"analysisInstrumentQuick",instrumentNames,current.instrument);
    auto *chordsQuick=new QCheckBox(SC_TR("Show chords"));chordsQuick->setObjectName("analysisShowChords");chordsQuick->setChecked(QSettings().value("analysis/showChords",true).toBool());quick->addWidget(chordsQuick);
    connect(chordsQuick,&QCheckBox::toggled,this,[this](bool on){canvas_->showChords=on;QSettings().setValue("analysis/showChords",on);canvas_->update();});
    auto *automatic=new QCheckBox(SC_TR("Automatic range"));automatic->setObjectName("analysisAutoRange");automatic->setChecked(current.autoRange);quick->addWidget(automatic);
    root->addWidget(quick);

    settingsPanel_=new QWidget;settingsPanel_->setObjectName("analysisSettingsPanel");auto *outer=new QVBoxLayout(settingsPanel_);outer->setContentsMargins(0,0,0,0);
    auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);auto *body=new QWidget;auto *form=new QFormLayout(body);scroll->setWidget(body);outer->addWidget(scroll,1);
    auto combo=[&](const QString &label,const QString &name,const QStringList &items,int selected){auto *c=new QComboBox;c->setObjectName(name);c->setAccessibleName(label);c->addItems(items);c->setCurrentIndex(selected);form->addRow(label,c);return c;};
    const QList<int> rates={16000,22050,24000,32000,44100,48000,88200,96000},sizes={512,1024,2048,4096,8192};QStringList rateText,fftText;for(int x:rates)rateText<<formatFrequency(x);for(int x:sizes)fftText<<QLocale().toString(x);
    auto *rate=combo(SC_TR("Analysis sample rate"),"analysisRate",rateText,rates.indexOf(current.rate));auto *fft=combo(SC_TR("FFT size"),"analysisFft",fftText,sizes.indexOf(current.fft));
    auto *layout=combo(SC_TR("Channels"),"analysisLayout",{SC_TR("Automatic"),SC_TR("Mono"),SC_TR("Stereo")},current.layout);
    auto *amplitude=combo(SC_TR("Amplitude"),"analysisAmplitude",{"dBFS",SC_TR("Linear")},current.amplitude);
    auto *instrument=combo(SC_TR("Instrument"),"analysisInstrument",instrumentNames,current.instrument);
    connect(instrumentQuick,&QComboBox::currentIndexChanged,instrument,&QComboBox::setCurrentIndex);   // the Settings combo applies the change
    connect(instrument,&QComboBox::currentIndexChanged,instrumentQuick,[instrumentQuick](int index){QSignalBlocker block(instrumentQuick);instrumentQuick->setCurrentIndex(index);});
    auto spin=[&](const QString &label,const QString &name,double value,double min,double max,const QString &unit){auto *w=new soundcurrent::AcceleratingDoubleSpinBox;w->setObjectName(name);w->setAccessibleName(label);w->setRange(min,max);w->setDecimals(1);w->setValue(value);w->setSuffix(unit);form->addRow(label,w);return w;};
    auto *low=spin(SC_TR("Minimum frequency"),"analysisLow",current.low,0,23999," Hz");auto *high=spin(SC_TR("Maximum frequency"),"analysisHigh",current.high,0,24000," Hz");
    high->setSpecialValueText(SC_TR("Automatic"));
    auto *ceiling=spin(SC_TR("Display ceiling"),"analysisCeiling",current.ceiling,-120,24," dBFS");
    auto *sensitivity=spin(SC_TR("Display sensitivity"),"analysisSensitivity",current.sensitivity,24,180," dB");auto *history=spin(SC_TR("History length"),"analysisHistory",current.history,5,60," s");auto *speed=spin(SC_TR("Scroll speed"),"analysisSpeed",current.speed,.5,4," ×");
    auto check=[&](const QString &label,const QString &name,bool selected){auto *c=new QCheckBox(label);c->setObjectName(name);c->setChecked(selected);form->addRow(c);return c;};
    auto *grid=check(SC_TR("Grid"),"analysisGrid",current.grid);auto *peaks=check(SC_TR("Peak hold"),"analysisPeaks",current.peaks);auto *average=check(SC_TR("Average"),"analysisAverage",current.average);auto *notes=check(SC_TR("Note grid"),"analysisNotes",current.notes);auto *transients=check(SC_TR("Transient markers"),"analysisTransients",current.transients);auto *music=check(SC_TR("Music mode"),"analysisMusic",current.music);
    auto *nowPlayingCheck=check(SC_TR("Show now playing"),"analysisShowNowPlaying",showNowPlaying_);
    connect(nowPlayingCheck,&QCheckBox::toggled,this,[this](bool on){showNowPlaying_=on;QSettings().setValue("analysis/nowPlaying",on);if(on && canvas_->isVisible()){updateNowPlaying();nowPlayingTimer_->start();}else{nowPlayingTimer_->stop();updateNowPlaying();}});
    ceiling->setEnabled(!current.autoRange);connect(automatic,&QCheckBox::toggled,ceiling,&QWidget::setDisabled);
    palette->setVisible(current.view==int(View::Waterfall));
    speed->setSingleStep(.5);
    feedback_=new QLabel;feedback_->setObjectName("analysisFeedback");feedback_->setWordWrap(true);outer->addWidget(feedback_);
    auto *buttons=new soundcurrent::ui::ResponsiveRow;auto *save=new QPushButton(SC_TR("Save preset"));auto *load=new QPushButton(SC_TR("Load preset"));save->setObjectName("analysisSavePreset");load->setObjectName("analysisLoadPreset");buttons->addWidget(save);buttons->addWidget(load);outer->addWidget(buttons);
    ui::protectScrollControls(scroll);ui::describeButton(save);ui::describeButton(load);
    settingsPanel_->setMaximumHeight(240);settingsPanel_->setVisible(false);root->addWidget(settingsPanel_);

    readControls_=[=]{Settings s;s.view=view_->currentIndex();s.rate=rates[std::max(0,rate->currentIndex())];s.fft=sizes[std::max(0,fft->currentIndex())];s.palette=palette->currentIndex();s.scale=scale->currentIndex();s.layout=layout->currentIndex();s.channel=channel->currentIndex();s.amplitude=amplitude->currentIndex();s.instrument=instrument->currentIndex();s.low=low->value();s.high=high->value();s.ceiling=ceiling->value();s.sensitivity=sensitivity->value();s.history=history->value();s.speed=speed->value();s.grid=grid->isChecked();s.peaks=peaks->isChecked();s.average=average->isChecked();s.autoRange=automatic->isChecked();s.notes=notes->isChecked();s.transients=transients->isChecked();s.music=music->isChecked();return s;};
    writeControls_=[=](const Settings &s){loading_=true;rate->setCurrentIndex(rates.indexOf(s.rate));fft->setCurrentIndex(sizes.indexOf(s.fft));palette->setCurrentIndex(s.palette);scale->setCurrentIndex(s.scale);layout->setCurrentIndex(s.layout);channel->setCurrentIndex(s.channel);amplitude->setCurrentIndex(s.amplitude);instrument->setCurrentIndex(s.instrument);low->setValue(s.low);high->setValue(s.high);ceiling->setValue(s.ceiling);sensitivity->setValue(s.sensitivity);history->setValue(s.history);speed->setValue(s.speed);grid->setChecked(s.grid);peaks->setChecked(s.peaks);average->setChecked(s.average);automatic->setChecked(s.autoRange);notes->setChecked(s.notes);transients->setChecked(s.transients);music->setChecked(s.music);palette->setVisible(s.view==int(View::Waterfall));instrumentQuick->setVisible(s.view==int(View::Tuner));chordsQuick->setVisible(s.view==int(View::Tuner));loading_=false;};
    commit_=new QTimer(this);commit_->setSingleShot(true);commit_->setInterval(250);
    connect(commit_,&QTimer::timeout,this,[this]{if(loading_)return;const auto s=readControls_();if(applySettings(s))feedback_->clear();else feedback_->setText(SC_TR("Invalid analysis settings. Keep the FFT window at or below 250 ms and frequencies within the captured bandwidth."));});
    auto schedule=[this]{if(!loading_)commit_->start();};
    for(auto *w:{rate,fft,palette,scale,layout,channel,amplitude,instrument})connect(w,&QComboBox::currentIndexChanged,this,schedule);
    for(auto *w:{low,high,ceiling,sensitivity,history,speed})connect(w,&QDoubleSpinBox::valueChanged,this,schedule);
    for(auto *w:{grid,peaks,average,automatic,notes,transients,music})connect(w,&QCheckBox::toggled,this,schedule);
    connect(view_,&QComboBox::currentIndexChanged,this,[this]{if(writeControls_)writeControls_(readControls_());});
    connect(save,&QPushButton::clicked,this,[=]{const auto s=readControls_();if(!s.valid()){feedback_->setText(SC_TR("Invalid analysis settings. Keep the FFT window at or below 250 ms and frequencies within the captured bandwidth."));return;}const auto path=i18n::FileDialogs::getSaveFileName(this,SC_TR("Save preset"),{},"JSON (*.json)");if(path.isEmpty())return;QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)){feedback_->setText(SC_TR("Could not save preset."));return;}file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);file.write(QJsonDocument(s.json()).toJson());if(!file.commit())feedback_->setText(SC_TR("Could not finish saving preset."));});
    connect(load,&QPushButton::clicked,this,[=]{const auto path=i18n::FileDialogs::getOpenFileName(this,SC_TR("Load preset"),{},"JSON / PRESET / TXT (*.json *.preset *.txt)");if(path.isEmpty())return;QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>65536)return;auto settings=Settings::parsePreset(file.readAll());if(!settings || !applySettings(*settings)){feedback_->setText(SC_TR("Invalid analysis settings. Keep the FFT window at or below 250 ms and frequencies within the captured bandwidth."));return;}feedback_->clear();});
    auto *saveKey=new QShortcut(QKeySequence::Save,settingsPanel_);connect(saveKey,&QShortcut::activated,save,&QPushButton::click);
    auto *openKey=new QShortcut(QKeySequence::Open,settingsPanel_);connect(openKey,&QShortcut::activated,load,&QPushButton::click);
}
}
