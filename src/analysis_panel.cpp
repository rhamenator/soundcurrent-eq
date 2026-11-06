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
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
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
    }
    std::unique_ptr<Engine> data;
    View mode=View::Waterfall;
    bool frozen=false;
    std::function<void(bool)> onVisibility;
    std::function<void()> onFrame;
    double displayCeiling=0;
    void resetClock(){clock.invalidate();}
    QSize historyPixelSize() const {return {image.width,image.height};}
    void clearImage(){image={};ensureImageSize();previousColumn.clear();credit=0;clock.invalidate();update();}
    bool push(const QByteArray &pcm) {
        if(!isVisible() || frozen)return false;
        const auto &s=data->settings;const double interval=(mode==View::Waveform || mode==View::Vectorscope)?40:std::max(40.,250.*s.fft/s.rate);
        if(clock.isValid() && clock.elapsed()<interval)return false;
        const double elapsed=clock.isValid()?clock.elapsed()/1000.:interval/1000.;
        if(!data->push(pcm,mode))return false;
        clock.restart();
        if(s.autoRange && mode!=View::Waveform && mode!=View::Vectorscope && !data->db.empty()) {
            const double top=*std::max_element(data->db.begin(),data->db.end());
            const double target=std::clamp(std::ceil((top+6)/6)*6,-120.,24.);
            displayCeiling=std::abs(target-displayCeiling)>=6?target:displayCeiling;
        } else displayCeiling=s.ceiling;
        if(mode==View::Waterfall){
            ensureImageSize();
            auto settings=gramSettings();auto column=spectrogramColumn(data->db,s,image.height);
            credit+=elapsed*image.width*s.speed/s.history;const int columns=std::min(image.width,int(credit));credit-=columns;
            if(columns>0)for(int y=0;y<image.height;++y){auto *row=image.pixels.data()+y*image.width;std::memmove(row,row+columns,std::size_t(image.width-columns)*sizeof(*row));}
            if(s.layout==2 && s.channel==0){column=spectrogramColumn(data->dbLeft,s,image.height/2);auto right=spectrogramColumn(data->dbRight,s,image.height-image.height/2);column.insert(column.end(),right.begin(),right.end());}
            const int count=std::max(1,columns);std::vector<double> blended(column.size());
            for(int x=image.width-count;x<image.width;++x){
                const double blend=double(x-(image.width-count)+1)/count;
                for(int y=0;y<image.height;++y)blended[y]=previousColumn.size()==column.size()?previousColumn[y]+blend*(column[y]-previousColumn[y]):column[y];
                gram::set_image_column(image,x,blended,settings);
                for(int y=0;y<image.height;++y)image.pixels[y*image.width+x]|=0xff000000;
            }
            previousColumn=std::move(column);
        }
        if(onFrame)onFrame();
        update();return true;
    }
    gram::SpectrogramSettings gramSettings() const {
        const auto &s=data->settings;gram::SpectrogramSettings g;g.fft_size=s.fft;g.hop_size=s.fft/4;g.image_height=image.height;
        g.min_db=displayCeiling-s.sensitivity;g.max_db=displayCeiling;g.min_frequency=s.low;g.max_frequency=s.upper();
        g.frequency_scale=gram::FrequencyScale(s.scale);g.palette_mode=gram::PaletteMode(s.palette);return g;
    }
protected:
    void showEvent(QShowEvent *event) override {QWidget::showEvent(event);clock.invalidate();if(onVisibility)onVisibility(true);}
    void hideEvent(QHideEvent *event) override {clock.invalidate();if(onVisibility)onVisibility(false);QWidget::hideEvent(event);}
    void mouseMoveEvent(QMouseEvent *event) override {
        const auto plot=plotRect();if(!plot.contains(event->position()))return;
        const double ratio=std::clamp((event->position().x()-plot.left())/plot.width(),0.,1.);
        const double vertical=(event->position().y()-plot.top())/plot.height();
        if(mode==View::Waveform){setToolTip(QLocale().toString(ratio*data->settings.fft*1000./data->settings.rate,'f',2)+" ms / "+QLocale().toString((1-2*vertical)*waveLimit_,'f',3));return;}
        if(mode==View::Waterfall){const int lanes=data->settings.layout==2 && data->settings.channel==0?2:1;const double lanePosition=vertical*lanes-std::floor(vertical*lanes);setToolTip(QLocale().toString(-(1-ratio)*data->settings.history/data->settings.speed,'f',2)+" s / "+formatFrequency(frequency(1-lanePosition)));return;}
        if(mode==View::Vectorscope || mode==View::Tuner)return;
        const double hz=frequency(ratio),level=data->settings.amplitude && !relativeAxis_?axisTop_+20*std::log10(std::max(1e-12,1-vertical)):axisTop_-vertical*axisSpan_;
        setToolTip(formatFrequency(hz)+" / "+QLocale().toString(level,'f',1)+(relativeAxis_?" dB":" dBFS"));
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setLayoutDirection(Qt::LeftToRight);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#111827"));
        const auto plot=plotRect();const auto &s=data->settings;
        const auto relative=data->relativeDb();
        const bool comparison=mode==View::Microphone && !relative.empty();
        double span=s.sensitivity,top=displayCeiling;
        if(comparison){
            double extent=0;for(double value:relative)if(std::isfinite(value))extent=std::max(extent,std::abs(value));
            if(s.autoRange)span=2*std::max(12.,std::ceil((extent+2)/3)*3);
            top=span/2;
        }
        axisTop_=top;axisSpan_=span;relativeAxis_=comparison;
        waveLimit_=std::pow(10.,s.ceiling/20.);
        if(s.autoRange && (mode==View::Waveform || mode==View::Vectorscope)){
            double extent=0;for(double value:data->left)extent=std::max(extent,std::abs(value));for(double value:data->right)extent=std::max(extent,std::abs(value));waveLimit_=std::max(.001,extent*1.1);
        }

        if(mode==View::Waterfall){
            ensureImageSize();
            QImage q(reinterpret_cast<const uchar *>(image.pixels.data()),image.width,image.height,image.width*4,QImage::Format_RGB32);
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
            for(double r:{0.,.25,.5,.75,1.})p.drawText(QRectF(plot.left()+r*plot.width()-45,plot.bottom()+3,90,18),Qt::AlignCenter,displayNumber(-(1-r)*s.history/s.speed)+" s");
            p.drawText(QRectF(plot.left(),plot.bottom()+20,plot.width(),18),Qt::AlignLeft,displayNumber(displayCeiling-s.sensitivity)+" … "+displayNumber(displayCeiling)+" dBFS");
        } else if(mode!=View::Tuner && mode!=View::Vectorscope){
            for(double r:{0.,.25,.5,.75,1.}) {
                const double y=plot.bottom()-r*plot.height();
                const double value=mode==View::Waveform?(2*r-1)*waveLimit_:(s.amplitude && !comparison)?r:top-span*(1-r);
                p.drawText(QRectF(0,y-9,plot.left()-8,18),Qt::AlignRight,displayNumber(value));
                if(s.grid){p.setPen(QColor("#334155"));p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));p.setPen(QColor("#9bacc2"));}
                if(mode==View::Waveform){
                    const double x=plot.left()+r*plot.width();
                    p.drawText(QRectF(x-45,plot.bottom()+3,90,18),Qt::AlignCenter,displayNumber(1000.*s.fft/s.rate*r));
                    if(s.grid){p.setPen(QColor("#334155"));p.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));p.setPen(QColor("#9bacc2"));}
                }
            }
            if(mode!=View::Waveform)for(double hz:frequencyTicks(plot.width(),92.)){
                const double x=plot.left()+ratio(hz)*plot.width();
                p.setPen(QColor("#9bacc2"));p.drawText(QRectF(x-45,plot.bottom()+3,90,18),Qt::AlignCenter,formatFrequency(hz));
                if(s.grid){p.setPen(QColor("#334155"));p.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));}
            }
            p.setPen(QColor("#9bacc2"));
            p.drawText(QRectF(plot.left(),plot.bottom()+20,plot.width(),18),Qt::AlignCenter,mode==View::Waveform?"ms":"");
        }
        p.save();p.setClipRect(plot.adjusted(-1,-1,1,1));
        if(mode==View::Waveform) {
            auto trace=[&](const std::vector<double> &values,const QColor &color){if(values.empty())return;QPainterPath path;
                // Per-pixel extrema preserve narrow transients when many samples share a pixel.
                for(int x=0;x<int(plot.width());++x){int first=x*values.size()/int(plot.width()),last=std::max(first+1,int((x+1)*values.size()/int(plot.width())));last=std::min(last,int(values.size()));
                    auto limits=std::minmax_element(values.begin()+first,values.begin()+last);const double y1=plot.center().y()-*limits.first*plot.height()/(2*waveLimit_),y2=plot.center().y()-*limits.second*plot.height()/(2*waveLimit_);
                    if(x==0)path.moveTo(plot.left()+x,y1);else path.lineTo(plot.left()+x,y1);path.lineTo(plot.left()+x,y2);}
                p.setPen(QPen(color,1.3));p.drawPath(path);};
            if(s.layout==1){std::vector<double> mono=data->left;for(std::size_t i=0;i<mono.size();++i)mono[i]=s.channel==1?data->left[i]:s.channel==2?data->right[i]:.5*(data->left[i]+data->right[i]);trace(mono,QColor("#55d7c3"));}
            else {trace(data->left,QColor("#55d7c3"));trace(data->right,QColor("#e6b450"));}
        } else if(mode==View::Vectorscope){
            const double radius=std::min(plot.width(),plot.height())*.45;const QPointF c=plot.center();p.setPen(QColor("#475569"));p.drawEllipse(c,radius,radius);p.drawLine(QPointF(c.x()-radius,c.y()),QPointF(c.x()+radius,c.y()));p.drawLine(QPointF(c.x(),c.y()-radius),QPointF(c.x(),c.y()+radius));
            p.setPen(QPen(QColor("#55d7c3"),1));QPainterPath path;for(std::size_t i=0;i<data->left.size();++i){QPointF point(c.x()+(data->left[i]-data->right[i])*.5*radius/waveLimit_,c.y()-(data->left[i]+data->right[i])*.5*radius/waveLimit_);if(i==0)path.moveTo(point);else path.lineTo(point);}p.drawPath(path);
            p.drawText(plot.adjusted(0,0,0,-4),Qt::AlignBottom|Qt::AlignHCenter,QString::fromUtf8("ρ ")+QLocale().toString(data->correlation,'f',2));
        } else if(mode==View::Tuner){
            const auto pitch=data->pitch;p.setPen(QColor("#55d7c3"));auto font=p.font();font.setPointSizeF(30);p.setFont(font);p.drawText(plot,Qt::AlignCenter,noteName(pitch.midi));
            font.setPointSizeF(11);p.setFont(font);p.drawText(plot.adjusted(0,0,0,-10),Qt::AlignBottom|Qt::AlignHCenter,formatFrequency(pitch.hz)+" / "+QLocale().toString(pitch.cents,'f',1)+" ¢");
            const double x=plot.center().x()+std::clamp(pitch.cents,-50.,50.)/50.*plot.width()*.4;p.drawLine(QPointF(x,plot.top()+12),QPointF(x,plot.top()+60));
            static const QStringList tunings={"", "E2 A2 D3 G3 B3 E4","E1 A1 D2 G2","G4 C4 E4 A4","G3 D4 A4 E5","C3 G3 D4 A4","C2 G2 D3 A3","G3 D4 A4 E5"};
            p.drawText(plot.adjusted(0,0,0,-36),Qt::AlignBottom|Qt::AlignHCenter,tunings[s.instrument]);
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
    std::vector<double> previousColumn;
    void ensureImageSize() {
        const auto plot=plotRect();const double dpr=devicePixelRatioF();
        const int w=std::clamp(int(std::ceil(plot.width()*dpr)),1,2048),h=std::clamp(int(std::ceil(plot.height()*dpr)),2,2048);
        if(image.width==w && image.height==h)return;
        auto replacement=gram::make_image(w,h);gram::clear_image(replacement,0xff000000);
        if(!image.pixels.empty()) {
            QImage old(reinterpret_cast<const uchar *>(image.pixels.data()),image.width,image.height,image.width*4,QImage::Format_RGB32);
            const auto scaled=old.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
            for(int y=0;y<h;++y)std::memcpy(replacement.pixels.data()+y*w,scaled.constScanLine(y),std::size_t(w)*4);
        }
        image=std::move(replacement);previousColumn.clear();credit=0;
    }
    QElapsedTimer clock;
    double credit=0,axisTop_=0,axisSpan_=90,waveLimit_=1;
    bool relativeAxis_=false;
    QRectF plotRect() const {const int margin=std::max(70,fontMetrics().horizontalAdvance(formatFrequency(999.999))+12);return QRectF(margin,18,std::max(1,width()-2*margin),std::max(1,height()-62));}
    std::vector<double> frequencyTicks(double pixels,double spacing) const {return frequencyAxisTicks(data->settings,pixels,spacing);}
    double frequency(double r) const {const auto &s=data->settings;return s.scale==0?s.low+r*(s.upper()-s.low):std::max(1.,s.low)*std::pow(s.upper()/std::max(1.,s.low),r);}
    double ratio(double hz) const {const auto &s=data->settings;return s.scale==0?(hz-s.low)/(s.upper()-s.low):std::log(hz/std::max(1.,s.low))/std::log(s.upper()/std::max(1.,s.low));}
};
Panel::Panel(QWidget *parent):QWidget(parent) {
    setObjectName("analysisPanel");auto *root=new QVBoxLayout(this);root->setContentsMargins(12,8,12,8);
    auto *top=new QGridLayout;view_=new QComboBox;view_->setObjectName("analysisView");view_->setAccessibleName(SC_TR("View"));
    const QStringList views={SC_TR("Spectrogram"),SC_TR("Spectrum"),SC_TR("Waveform"),SC_TR("Tuner"),SC_TR("Vectorscope"),SC_TR("Averaged spectrum"),SC_TR("Microphone response")};
    for(int i=0;i<views.size();++i)view_->addItem(views[i],i);
    source_=new QComboBox;source_->setObjectName("analysisSource");source_->setAccessibleName(SC_TR("Microphone"));source_->addItem(SC_TR("Playback monitor"),QString());
    for(auto *combo:{view_,source_}){combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setMinimumContentsLength(8);combo->setMinimumWidth(0);combo->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);}
    freeze_=new QCheckBox(SC_TR("Freeze display"));freeze_->setObjectName("analysisFreeze");auto *settings=new QPushButton(SC_TR("Settings"));settings->setObjectName("analysisSettings");
    top->addWidget(view_,0,0);top->addWidget(source_,0,1);top->addWidget(freeze_,0,2);top->addWidget(settings,0,3);top->setColumnStretch(0,1);top->setColumnStretch(1,1);root->addLayout(top);
    canvas_=new ScopeCanvas(this);root->addWidget(canvas_,1);readout_=new QLabel;readout_->setTextFormat(Qt::PlainText);readout_->setWordWrap(true);readout_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);readout_->setFixedHeight(2*fontMetrics().lineSpacing());root->addWidget(readout_);
    help_=new QLabel(SC_TR("Reference comparisons include the source, room and reference microphone. They are not an isolated or certified microphone measurement."));help_->setWordWrap(true);root->addWidget(help_);
    auto *actions=new QGridLayout;auto *resetButton=new QPushButton(SC_TR("Reset"));reference_=new QPushButton(SC_TR("Capture reference"));profile_=new QPushButton(SC_TR("Create microphone profile"));
    resetButton->setObjectName("analysisReset");reference_->setObjectName("analysisReference");profile_->setObjectName("analysisCreateProfile");
    actions->addWidget(resetButton,0,0);actions->addWidget(reference_,0,1);actions->addWidget(profile_,1,0,1,2);root->addLayout(actions);
    const auto saved=Settings::parse(QJsonDocument::fromJson(QSettings().value("analysis/settings").toByteArray()).object());if(saved)canvas_->data->configure(*saved);
    canvas_->onVisibility=[this](bool visible){if(visible){captureFailed_=false;refreshSources();}if(onActivity)onActivity(source(),visible && active());};
    canvas_->onFrame=[this]{availability();};
    connect(view_,&QComboBox::currentIndexChanged,this,[this](int index){canvas_->data->resetAverage();canvas_->mode=View(index);canvas_->data->settings.view=index;canvas_->clearImage();canvas_->setAccessibleName(view_->currentText());QSettings().setValue("analysis/view",index);availability();});
    connect(source_,&QComboBox::currentIndexChanged,this,[this]{sourceReady_=true;captureFailed_=false;reset();QSettings().setValue("analysis/source",source());QSettings().setValue("analysis/sourceName",source_->currentText());if(onActivity)onActivity(source(),active());});
    connect(freeze_,&QCheckBox::toggled,this,[this](bool on){canvas_->frozen=on;});
    connect(settings,&QPushButton::clicked,this,&Panel::settingsDialog);
    connect(resetButton,&QPushButton::clicked,this,&Panel::reset);
    connect(reference_,&QPushButton::clicked,this,[this]{captureReference();});
    connect(profile_,&QPushButton::clicked,this,[this]{if(auto p=microphoneProfile()){suspendForDialog(true);if(onProfile)onProfile(*p);suspendForDialog(false);}else QMessageBox::information(this,SC_TR("Microphone response"),SC_TR("Not enough broadband reference data."));});
    setView(View(saved?saved->view:std::clamp(QSettings().value("analysis/view",0).toInt(),0,6)));canvas_->setAccessibleName(view_->currentText());availability();
}
Panel::~Panel(){canvas_->onVisibility={};canvas_->onFrame={};onActivity={};}
bool Panel::active() const{return canvas_->isVisible() && sourceReady_ && !captureFailed_ && !suspended_;}
QString Panel::source() const{return source_->currentData().toString();}
Engine &Panel::engine(){return *canvas_->data;}
QSize Panel::historyPixelSize() const{return canvas_->historyPixelSize();}
View Panel::view() const{return canvas_->mode;}
void Panel::setView(View mode){view_->setCurrentIndex(int(mode));canvas_->mode=mode;canvas_->data->settings.view=int(mode);}
void Panel::pushPcm(const QByteArray &bytes){if(sourceReady_ && !captureFailed_ && !suspended_)canvas_->push(bytes);}
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
    if(ready!=sourceReady_ || oldSource!=source()){
        sourceReady_=ready;captureFailed_=false;reset();if(onActivity)onActivity(source(),active());
    }
    if(!ready)readout_->setText(SC_TR("No microphone connected."));
}
bool Panel::applySettings(const Settings &s){if(!s.valid())return false;canvas_->data->configure(s);setView(View(s.view));canvas_->clearImage();QSettings().setValue("analysis/settings",QJsonDocument(s.json()).toJson(QJsonDocument::Compact));availability();return true;}
void Panel::reset(){const bool retry=captureFailed_;captureFailed_=false;engine().resetAverage();canvas_->clearImage();availability();if(retry && onActivity)onActivity(source(),active());}
bool Panel::captureReference(){if(engine().frames<25 || engine().averaged.empty())return false;referenceSource_=source_->currentText();engine().captureReference();canvas_->clearImage();availability();return true;}
std::optional<equipment::Profile> Panel::microphoneProfile() const {
    const auto &e=*canvas_->data;if(source().isEmpty() || view()!=View::Microphone || e.frames<25 || e.reference.size()!=e.averaged.size())return {};
    const auto relative=e.relativeDb();
    if(std::max(20.,e.settings.low)>=std::min(20000.,e.settings.upper()))return {};
    equipment::Profile p;p.kind="microphone";p.custom=true;p.brand=SC_TR("Custom");p.family=SC_TR("Microphone");p.model=source_->currentText();
    p.source=referenceSource_;p.conditions=SC_TR("Reference comparisons include the source, room and reference microphone. They are not an isolated or certified microphone measurement.");p.provenance=p.conditions;p.estimated=true;
    p.estimation={{"method","relative-microphone-reference-db-v1"},{"createdUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"limitations",p.conditions},
        {"referenceSource",referenceSource_},{"targetSource",source_->currentText()},
        {"interpretation","combined-residual"},{"sampleRate",e.settings.rate},{"fftSize",e.settings.fft},
        {"averagedFrames",double(e.frames)}};

    for(int i=0;i<96;++i){double hz=std::max(20.,e.settings.low)*std::pow(std::min(20000.,e.settings.upper())/std::max(20.,e.settings.low),i/95.);
        int bin=std::clamp(int(std::lround(hz*e.settings.fft/e.settings.rate)),1,int(e.averaged.size())-1);
        if(!p.response.isEmpty() && hz<=p.response.back().frequency)continue;
        if(!std::isfinite(relative[bin]))continue;
        p.response.append({hz,relative[bin]});
    }
    if(p.response.size()<6 || p.response.back().frequency/p.response.front().frequency<4)return {};
    p.filters=equipment::fitResponse(p.response);return p;
}
void Panel::availability(){
    const auto &e=engine();const bool microphone=view()==View::Microphone;help_->setVisible(microphone);reference_->setEnabled(e.frames>=25);reference_->setVisible(view()==View::Average || microphone);profile_->setVisible(microphone);profile_->setEnabled(microphone && !source().isEmpty() && e.frames>=25 && e.hasReference());
    readout_->setText(SC_TR("Frames: %1 · FFT: %2 ms · Resolution: %3 · Window: %4 ms").arg(QLocale().toString(e.waveFrames),QLocale().toString(e.fftMs,'f',2),formatFrequency(double(e.settings.rate)/e.settings.fft),QLocale().toString(e.settings.fft*1000./e.settings.rate,'f',1)));
}
void Panel::suspendForDialog(bool suspended){suspended_=suspended;canvas_->resetClock();if(onActivity)onActivity(source(),active());}
void Panel::settingsDialog(){
    suspendForDialog(true);
    QDialog dialog(this);dialog.setWindowTitle(SC_TR("Settings"));dialog.setWindowModality(Qt::ApplicationModal);dialog.setObjectName("analysisSettingsDialog");
    auto *outer=new QVBoxLayout(&dialog);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);auto *body=new QWidget;auto *form=new QFormLayout(body);scroll->setWidget(body);outer->addWidget(scroll,1);
    const auto current=engine().settings;
    auto combo=[&](const QString &label,const QString &name,const QStringList &items,int selected){auto *c=new QComboBox;c->setObjectName(name);c->setAccessibleName(label);c->addItems(items);c->setCurrentIndex(selected);form->addRow(label,c);return c;};
    QStringList viewText;for(int i=0;i<view_->count();++i)viewText<<view_->itemText(i);
    auto *stagedView=combo(SC_TR("View"),"analysisSettingsView",viewText,current.view);
    const QList<int> rates={16000,22050,24000,32000,44100,48000,88200,96000},sizes={512,1024,2048,4096,8192};QStringList rateText,fftText;for(int x:rates)rateText<<formatFrequency(x);for(int x:sizes)fftText<<QLocale().toString(x);
    auto *rate=combo(SC_TR("Analysis sample rate"),"analysisRate",rateText,rates.indexOf(current.rate));auto *fft=combo(SC_TR("FFT size"),"analysisFft",fftText,sizes.indexOf(current.fft));
    auto *palette=combo(SC_TR("Palette"),"analysisPalette",{SC_TR("Spectrum"),SC_TR("Gray"),SC_TR("Blue"),SC_TR("Green"),SC_TR("Amber"),SC_TR("Ice")},current.palette);
    auto *scale=combo(SC_TR("Frequency scale"),"analysisScale",{SC_TR("Linear"),SC_TR("Logarithmic"),SC_TR("Octave")},current.scale);
    auto *layout=combo(SC_TR("Channels"),"analysisLayout",{SC_TR("Automatic"),SC_TR("Mono"),SC_TR("Stereo")},current.layout);
    auto *channel=combo(SC_TR("Channel"),"analysisChannel",{SC_TR("Mix"),SC_TR("Left"),SC_TR("Right")},current.channel);
    auto *amplitude=combo(SC_TR("Amplitude"),"analysisAmplitude",{"dBFS",SC_TR("Linear")},current.amplitude);
    auto *instrument=combo(SC_TR("Instrument"),"analysisInstrument",{SC_TR("Chromatic"),SC_TR("Guitar"),SC_TR("Bass guitar"),SC_TR("Ukulele"),SC_TR("Violin"),SC_TR("Viola"),SC_TR("Cello"),SC_TR("Mandolin")},current.instrument);
    auto spin=[&](const QString &label,const QString &name,double value,double min,double max,const QString &unit){auto *w=new soundcurrent::AcceleratingDoubleSpinBox;w->setObjectName(name);w->setAccessibleName(label);w->setRange(min,max);w->setDecimals(1);w->setValue(value);w->setSuffix(unit);form->addRow(label,w);return w;};
    auto *low=spin(SC_TR("Minimum frequency"),"analysisLow",current.low,0,23999," Hz");auto *high=spin(SC_TR("Maximum frequency"),"analysisHigh",current.high,0,24000," Hz");
    high->setSpecialValueText(SC_TR("Automatic"));
    auto *ceiling=spin(SC_TR("Display ceiling"),"analysisCeiling",current.ceiling,-120,24," dBFS");
    auto *sensitivity=spin(SC_TR("Display sensitivity"),"analysisSensitivity",current.sensitivity,24,180," dB");auto *history=spin(SC_TR("History length"),"analysisHistory",current.history,5,60," s");auto *speed=spin(SC_TR("Scroll speed"),"analysisSpeed",current.speed,.5,4," ×");
    auto check=[&](const QString &label,const QString &name,bool selected){auto *c=new QCheckBox(label);c->setObjectName(name);c->setChecked(selected);form->addRow(c);return c;};
    auto *grid=check(SC_TR("Grid"),"analysisGrid",current.grid);auto *peaks=check(SC_TR("Peak hold"),"analysisPeaks",current.peaks);auto *average=check(SC_TR("Average"),"analysisAverage",current.average);auto *automatic=check(SC_TR("Automatic range"),"analysisAutoRange",current.autoRange);auto *notes=check(SC_TR("Note grid"),"analysisNotes",current.notes);auto *transients=check(SC_TR("Transient markers"),"analysisTransients",current.transients);auto *music=check(SC_TR("Music mode"),"analysisMusic",current.music);
    ceiling->setEnabled(!current.autoRange);connect(automatic,&QCheckBox::toggled,ceiling,&QWidget::setDisabled);
    palette->setEnabled(view()==View::Waterfall);
    auto *feedback=new QLabel;feedback->setObjectName("analysisFeedback");feedback->setWordWrap(true);outer->addWidget(feedback);
    speed->setSingleStep(.5);
    auto *buttons=new QDialogButtonBox;auto *apply=buttons->addButton(SC_TR("Apply"),QDialogButtonBox::ApplyRole);auto *save=buttons->addButton(SC_TR("Save preset"),QDialogButtonBox::ActionRole);auto *load=buttons->addButton(SC_TR("Load preset"),QDialogButtonBox::ActionRole);auto *close=buttons->addButton(SC_TR("Close"),QDialogButtonBox::RejectRole);outer->addWidget(buttons);apply->setObjectName("analysisApply");close->setObjectName("analysisClose");
    auto read=[&]{Settings s;s.view=stagedView->currentIndex();s.rate=rates[rate->currentIndex()];s.fft=sizes[fft->currentIndex()];s.palette=palette->currentIndex();s.scale=scale->currentIndex();s.layout=layout->currentIndex();s.channel=channel->currentIndex();s.amplitude=amplitude->currentIndex();s.instrument=instrument->currentIndex();s.low=low->value();s.high=high->value();s.ceiling=ceiling->value();s.sensitivity=sensitivity->value();s.history=history->value();s.speed=speed->value();s.grid=grid->isChecked();s.peaks=peaks->isChecked();s.average=average->isChecked();s.autoRange=automatic->isChecked();s.notes=notes->isChecked();s.transients=transients->isChecked();s.music=music->isChecked();return s;};
    auto write=[&](const Settings &s){stagedView->setCurrentIndex(s.view);rate->setCurrentIndex(rates.indexOf(s.rate));fft->setCurrentIndex(sizes.indexOf(s.fft));palette->setCurrentIndex(s.palette);scale->setCurrentIndex(s.scale);layout->setCurrentIndex(s.layout);channel->setCurrentIndex(s.channel);amplitude->setCurrentIndex(s.amplitude);instrument->setCurrentIndex(s.instrument);low->setValue(s.low);high->setValue(s.high);ceiling->setValue(s.ceiling);sensitivity->setValue(s.sensitivity);history->setValue(s.history);speed->setValue(s.speed);grid->setChecked(s.grid);peaks->setChecked(s.peaks);average->setChecked(s.average);automatic->setChecked(s.autoRange);notes->setChecked(s.notes);transients->setChecked(s.transients);music->setChecked(s.music);};
    auto perform=[&]{const auto s=read();if(!applySettings(s)){feedback->setText(SC_TR("Invalid analysis settings. Keep the FFT window at or below 250 ms and frequencies within the captured bandwidth."));return false;}feedback->clear();return true;};
    connect(apply,&QPushButton::clicked,&dialog,[&]{perform();});connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);
    connect(save,&QPushButton::clicked,&dialog,[&]{const auto s=read();if(!s.valid()){perform();return;}const auto path=i18n::FileDialogs::getSaveFileName(&dialog,SC_TR("Save preset"),{},"JSON (*.json)");if(path.isEmpty())return;QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)){feedback->setText(SC_TR("Could not save preset."));return;}file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);file.write(QJsonDocument(s.json()).toJson());if(!file.commit())feedback->setText(SC_TR("Could not finish saving preset."));});
    connect(load,&QPushButton::clicked,&dialog,[&]{const auto path=i18n::FileDialogs::getOpenFileName(&dialog,SC_TR("Load preset"),{},"JSON / PRESET / TXT (*.json *.preset *.txt)");if(path.isEmpty())return;QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>65536)return;auto settings=Settings::parsePreset(file.readAll());if(!settings){feedback->setText(SC_TR("Invalid analysis settings. Keep the FFT window at or below 250 ms and frequencies within the captured bandwidth."));return;}write(*settings);});
    ui::protectScrollControls(scroll);
    dialog.resize(std::min(620,screen()->availableGeometry().width()-30),std::min(650,screen()->availableGeometry().height()-40));ui::describeButton(apply);ui::describeButton(save);ui::describeButton(load);dialog.exec();suspendForDialog(false);
}
}
