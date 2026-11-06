// SPDX-License-Identifier: GPL-3.0-only
#include "analysis_panel.h"
#ifndef Q_OS_WIN
#include "analysis_capture.h"
#endif
#include "update_panel.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QNetworkReply>
#include <QDialog>
#include <QTabWidget>
#include <QLabel>
#include <QProcess>
#include <QSettings>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <cmath>
#include <cstring>
#include <numbers>
#include <random>
#include <iostream>
#include <stdexcept>
using namespace soundcurrent::analysis;
static void check(bool okay,const char *message){if(!okay)throw std::runtime_error(message);}
static void events(int ms=0){QElapsedTimer timer;timer.start();do{QApplication::processEvents();if(ms)QThread::msleep(2);}while(timer.elapsed()<ms);}
static QByteArray tone(double hz,double level=.5,bool opposite=false,int frames=16384){QByteArray pcm(frames*4,0);for(int i=0;i<frames;++i){const auto sample=std::int16_t(std::lround(32767*level*std::sin(2*std::numbers::pi*hz*i/48000.)));for(int c=0;c<2;++c){const auto raw=std::uint16_t(opposite && c==1?-sample:sample);pcm[4*i+2*c]=char(raw&255);pcm[4*i+2*c+1]=char(raw>>8);}}return pcm;}
static QByteArray noise(bool shaped=false){std::mt19937 random(42);std::uniform_real_distribution<double> value(-.2,.2);QByteArray pcm(16384*4,0);double low=0;for(int i=0;i<16384;++i){double x=value(random);low=.97*low+.03*x;if(shaped)x=.5*x+3*low;auto raw=std::uint16_t(std::int16_t(x*32767));for(int c=0;c<2;++c){pcm[4*i+2*c]=char(raw&255);pcm[4*i+2*c+1]=char(raw>>8);}}return pcm;}
class Reply : public QNetworkReply {
public:
 Reply(const QNetworkRequest &request,QByteArray payload,QObject *parent):QNetworkReply(parent),payload_(std::move(payload)){setRequest(request);setUrl(request.url());open(QIODevice::ReadOnly);setAttribute(QNetworkRequest::HttpStatusCodeAttribute,200);QTimer::singleShot(0,this,[this]{emit readyRead();setFinished(true);emit finished();});}
 void abort() override {setError(OperationCanceledError,"test abort");setFinished(true);emit finished();}
 qint64 bytesAvailable() const override {return payload_.size()-offset_+QNetworkReply::bytesAvailable();}
protected:
 qint64 readData(char *out,qint64 max) override {auto n=std::min(max,qint64(payload_.size()-offset_));if(n<=0)return -1;std::memcpy(out,payload_.constData()+offset_,std::size_t(n));offset_+=n;return n;}
private:QByteArray payload_;qint64 offset_=0;
};
int main(int argc,char **argv){QApplication app(argc,argv);QTemporaryDir temporary;app.setOrganizationName("SoundCurrentAnalysisTests");app.setApplicationName("isolated");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());soundcurrent::i18n::Runtime language;language.initialize(!app.arguments().contains("--language"),{},qEnvironmentVariable("SOUNDCURRENT_TEST_FORMAT_LOCALE"));if(qEnvironmentVariableIsSet("SOUNDCURRENT_TEST_UI_SCALE")){auto font=app.font();font.setPointSizeF(font.pointSizeF()*qEnvironmentVariable("SOUNDCURRENT_TEST_UI_SCALE").toDouble());app.setFont(font);}try{
#ifndef Q_OS_WIN
 if(app.arguments().contains("--live-loopback")){
     auto command=[](const QString &program,const QStringList &args){QProcess p;p.start(program,args);check(p.waitForStarted(3000) && p.waitForFinished(5000) && p.exitCode()==0,"loopback fixture command failed");return QString::fromUtf8(p.readAllStandardOutput()).trimmed();};
     const auto before=command("pactl",{"get-default-sink"});const auto sink="sc_scope_fixture_"+QString::number(QCoreApplication::applicationPid());
     const auto module=command("pactl",{"load-module","module-null-sink","sink_name="+sink,"rate=48000","channels=2"});
     Capture capture;QProcess player;bool received=false;Engine monitor;
     try {
         capture.onPcm=[&](const QByteArray &bytes){if(monitor.push(bytes,View::Spectrum) && monitor.db[80]>-7 && monitor.db[80]<-5)received=true;};
         capture.start(sink,true);events(100);player.start("pacat",{"--playback","--raw","--device="+sink,"--format=s16le","--rate=48000","--channels=2"});check(player.waitForStarted(2000),"loopback writer failed");player.write(tone(937.5,.5,false,48000));player.closeWriteChannel();
         QElapsedTimer wait;wait.start();while(!received && wait.elapsed()<3500)events(10);
         capture.stop();check(!capture.running(),"capture did not stop");player.kill();player.waitForFinished(500);command("pactl",{"unload-module",module});
         check(received,"passive output monitor did not deliver the known tone");check(command("pactl",{"get-default-sink"})==before,"analysis changed default audio output");
     }catch(...){capture.stop();player.kill();player.waitForFinished(500);command("pactl",{"unload-module",module});throw;}
     std::cout<<"PASS: live silent PulseAudio/PipeWire loopback captured 937.5 Hz at expected level; capture stopped, fixture removed, default output unchanged\n";return 0;
 }
#endif
 Settings settings;check(settings.valid(),"default settings invalid");check(Settings::parse(settings.json()).has_value(),"settings round-trip failed");
 for(const auto &key:{"view","rate","fft","palette","scale","layout","channel","amplitude","instrument"}){auto bad=settings.json();bad[key]=-1;check(!Settings::parse(bad),"invalid enum/rate accepted");bad=settings.json();bad[key]=1.5;check(!Settings::parse(bad),"fractional integer accepted");}
 auto tooLong=settings;tooLong.rate=16000;tooLong.fft=8192;tooLong.high=0;check(!tooLong.valid(),"unstable >250 ms FFT window accepted");settings.high=0;check(settings.valid() && settings.upper()==24000,"automatic bandwidth");
 auto legacy=Settings::parsePreset("sample_rate=48000\nfft=4096\npalette=amber\nscale=octave\nmax_freq=Auto\nlayout=stereo\ndisplay=room\namplitude=linear\nmono_channel=left\nhistory=30\nscroll=2.0\nsensitivity=90\ngrid=1\npeak_hold=1\naverage=1\nmusic_mode=1\n");
 check(legacy && legacy->view==5 && legacy->palette==4 && legacy->scale==2 && legacy->channel==1 && legacy->high==0 && legacy->music && legacy->peaks,"C++ spectrogram preset compatibility");
 check(!Settings::parsePreset("sample_rate=48000\nfft=4096\nsensitivity=oops\n"),"malformed legacy number accepted");
 check(!Settings::parsePreset("sample_rate=48000\nfft=4096\nfft=8192\n"),"duplicate preset keys accepted");
 Engine engine;const auto signal=tone(937.5);check(engine.push(signal,View::Spectrum),"tone analysis rejected");const auto peak=std::max_element(engine.db.begin(),engine.db.end())-engine.db.begin();check(peak==80 && std::abs(engine.db[peak]+6.0206)<.05,"Hann/coherent-gain spectrum normalization or frequency incorrect");
 check(engine.push(tone(937.5,.5,true),View::Spectrum) && std::abs(engine.db[80]+6.0206)<.05,"opposite stereo polarity cancelled the spectrum");check(engine.correlation<-.99,"negative correlation not detected");
 const auto fftCalls=engine.fftCalls;check(engine.push(signal,View::Waveform) && engine.push(signal,View::Vectorscope),"time-domain views rejected audio");check(engine.fftCalls==fftCalls,"waveform/vectorscope ran unnecessary FFTs");
 check(!engine.push(signal.left(7),View::Spectrum),"partial stereo PCM accepted");
 Settings down;down.rate=16000;down.high=0;down.fft=2048;engine.configure(down);check(engine.push(tone(12000),View::Spectrum),"resampling failed");check(*std::max_element(engine.db.begin(),engine.db.end())<-45,"downsampling aliases above Nyquist");
 for(int rate:{16000,22050,24000,32000,44100,48000,88200,96000}){auto s=Settings{};s.rate=rate;s.high=0;s.fft=2048;engine.configure(s);check(engine.push(tone(1000),View::Spectrum),"analysis rate failed");auto i=std::max_element(engine.db.begin(),engine.db.end())-engine.db.begin();check(std::abs(engine.frequencyAt(i)-1000)<double(rate)/s.fft,"resampled tone frequency incorrect");}
 Settings tuner;engine.configure(tuner);check(engine.push(tone(440),View::Tuner),"tuner rejected audio");check(engine.pitch.midi==69 && std::abs(engine.pitch.cents)<8,"tuner A4 inaccurate");tuner.instrument=1;engine.configure(tuner);engine.push(tone(110),View::Tuner);check(engine.pitch.midi==45 && std::abs(engine.pitch.cents)<8,"instrument tuning not applied");
 Settings cumulative;engine.configure(cumulative);engine.resetAverage();engine.push(signal,View::Microphone);engine.push(tone(937.5,.25),View::Microphone);const double expected=10*std::log10((.25+.0625)/2);check(std::abs(engine.averaged[80]-expected)<.05,"cumulative power average incorrect");
 Panel panel;panel.resize(800,600);int activity=0;bool active=false;panel.onActivity=[&](const QString &,bool on){++activity;active=on;};bool connected=true;panel.microphoneSources=[&connected] {return connected?QVector<QPair<QString,QString>>{{"reference","Reference mic"},{"target","Target mic"}}:QVector<QPair<QString,QString>>{};};
 const auto inactive=panel.engine().fftCalls;panel.pushPcm(signal);check(panel.engine().fftCalls==inactive && !active,"hidden canvas analyzed audio");panel.show();events();check(active,"visible canvas not activated");panel.setView(View::Spectrum);panel.pushPcm(signal);check(panel.engine().fftCalls>inactive,"visible spectrum did not analyze");
 const auto visibleCalls=panel.engine().fftCalls;panel.hide();events();panel.pushPcm(signal);check(!active && panel.engine().fftCalls==visibleCalls,"hidden tab retained analysis");panel.show();events();panel.setView(View::Waveform);events(45);const auto timeCalls=panel.engine().fftCalls;panel.pushPcm(signal);check(panel.engine().fftCalls==timeCalls && !panel.engine().left.empty(),"waveform activation/FFT gate");
 auto *source=panel.findChild<QComboBox *>("analysisSource");check(source && source->count()==3,"microphone source list missing");source->setCurrentIndex(1);panel.setView(View::Microphone);
 for(int i=0;i<25;++i){events(45);panel.pushPcm(noise());}check(panel.captureReference(),"reference capture failed");source->setCurrentIndex(2);for(int i=0;i<25;++i){events(45);panel.pushPcm(noise(true));}
 auto profile=panel.microphoneProfile();check(profile && profile->kind=="microphone" && profile->response.size()>=6 && !profile->filters.isEmpty(),"relative microphone profile not proposed");
 for(const auto &point:profile->response){const int bin=int(std::lround(point.frequency*panel.engine().settings.fft/panel.engine().settings.rate));check(std::abs(point.db-panel.engine().relativeDb()[bin])<1e-10,"saved microphone curve differs from the scope curve");}
 check(profile->response.front().db-profile->response.back().db>6,"reference comparison lost target frequency shaping");auto parsed=soundcurrent::equipment::parse(QJsonDocument(soundcurrent::equipment::serialize(*profile)).toJson());check(parsed.response.size()==profile->response.size(),"proposed profile does not round-trip through profile editor format");
 connected=false;panel.refreshSources();check(!panel.active(),"disconnected microphone remained active");const auto disconnectedCalls=panel.engine().fftCalls;panel.pushPcm(signal);check(panel.engine().fftCalls==disconnectedCalls,"disconnected capture still analyzed");connected=true;panel.refreshSources();check(panel.active(),"reconnected microphone did not reactivate");
 panel.captureError("synthetic capture failure");check(!panel.active(),"capture failure did not stop activity");panel.hide();panel.show();events();check(panel.active(),"tab reopen did not permit capture retry");
 for(View view:{View::Waterfall,View::Spectrum,View::Waveform,View::Tuner,View::Vectorscope,View::Average,View::Microphone}){panel.setView(view);events(45);if(view==View::Microphone){for(int n=0;n<25;++n){events(45);panel.pushPcm(noise(true));}}else panel.pushPcm(signal);events();auto image=panel.grab().toImage();check(!image.isNull(),"analysis view failed to render");const auto directory=qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");if(!directory.isEmpty()){QDir().mkpath(directory);check(image.save(directory+"/analysis-"+QString::number(int(view))+".png"),"could not save visual QA");}}
 // Fundamental analysis settings are staged; cancel leaves the running analyzer untouched.
 QTimer::singleShot(0,&panel,[&]{auto *dialog=panel.findChild<QDialog *>("analysisSettingsDialog");check(dialog,"missing settings dialog");auto *low=dialog->findChild<QDoubleSpinBox *>("analysisLow");check(low,"frequency setting missing");low->setValue(100);dialog->reject();});const auto old=panel.engine().settings.json();panel.settingsDialog();check(panel.engine().settings.json()==old,"cancel applied staged settings");
 QTimer::singleShot(0,&panel,[&]{auto *dialog=panel.findChild<QDialog *>("analysisSettingsDialog");auto *low=dialog->findChild<QDoubleSpinBox *>("analysisLow");auto *high=dialog->findChild<QDoubleSpinBox *>("analysisHigh");auto *apply=dialog->findChild<QPushButton *>("analysisApply");check(low && high && apply,"staged settings actions missing");low->setValue(10000);high->setValue(1000);apply->click();check(panel.engine().settings.json()==old && !dialog->findChild<QLabel *>("analysisFeedback")->text().isEmpty(),"invalid settings changed analyzer or failed silently");low->setValue(100);high->setValue(16000);apply->click();check(panel.engine().settings.low==100 && panel.engine().settings.high==16000 && apply->isVisible(),"Apply did not take effect or disappeared");const auto directory=qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");if(!directory.isEmpty())dialog->grab().save(directory+"/analysis-settings.png");dialog->reject();});panel.settingsDialog();
 {QTabWidget tabs;auto *scope=new Panel;tabs.addTab(new QWidget,"Other");tabs.addTab(scope,"Analysis");tabs.resize(800,600);tabs.show();events();scope->pushPcm(signal);check(scope->engine().fftCalls==0,"unselected tab ran FFT");tabs.setCurrentIndex(1);events();scope->pushPcm(signal);check(scope->engine().fftCalls>0,"selected tab did not run FFT");const auto calls=scope->engine().fftCalls;tabs.setCurrentIndex(0);events(50);scope->pushPcm(signal);check(scope->engine().fftCalls==calls,"tab switch did not deactivate FFT");}
 panel.hide();
 QSettings().setValue("updates/reminders",false);QSettings().setValue("updates/lastCheck",QDateTime::currentDateTimeUtc());
 soundcurrent::UpdatePanel updates("soundcurrent-eq","EQ","1.2.1",true);int requests=0;bool available=false;updates.onAvailabilityChanged=[&](bool on){available=on;};updates.requestSender=[&](const QNetworkRequest &request){++requests;return new Reply(request,"[{\"tag_name\":\"v1.2.2\",\"html_url\":\"https://github.com/rhamenator/soundcurrent-eq/releases/tag/v1.2.2\"}]",&updates);};
 events(1700);check(requests==1 && available && updates.updateAvailable(),"startup update check was blocked by daily cache or reminder preference");
 QTemporaryDir download;QSettings().setValue("updates/folder",download.path());
 soundcurrent::UpdatePanel local("soundcurrent-eq","EQ","1.2.1",false);bool localAvailable=false;local.onAvailabilityChanged=[&](bool on){localAvailable=on;};
#ifdef Q_OS_WIN
 const auto installer=download.filePath("SoundCurrent-EQ-1.2.2-windows-x64-setup.exe");
#else
 const auto installer=download.filePath("soundcurrent-eq_1.2.2_amd64.deb");
#endif
 QFile package(installer);check(package.open(QIODevice::WriteOnly),"local update fixture");package.write("fixture only");package.close();local.checkDownloadedUpdates();check(localAvailable,"downloaded update did not reveal shortcut");QFile::remove(installer);local.checkDownloadedUpdates();check(!localAvailable,"stale downloaded update kept shortcut visible");
 std::cout<<"PASS: bounded FFT/resampling, stereo polarity, tuner/instruments, cumulative reference profiles, seven views, hidden-tab shutdown, staged settings, and startup update availability\n";return 0;
 }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}
