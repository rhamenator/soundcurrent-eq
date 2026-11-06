// SPDX-License-Identifier: GPL-3.0-only
#include "analysis_scope.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <chrono>
#include <stdexcept>
#include <limits>
#include <QJsonDocument>
#include <QSet>
#include <QStringList>
namespace soundcurrent::analysis {
std::vector<double> frequencyAxisTicks(const Settings &s,double pixels,double spacing) {
    std::vector<double> ticks;
    if(!s.valid() || !std::isfinite(pixels) || !std::isfinite(spacing) || pixels<=0 || spacing<=0)return ticks;
    const double low=s.scale==0?s.low:std::max(1.,s.low),high=s.upper();
    if(high<=low)return ticks;
    const int intervals=int(std::clamp(std::floor(pixels/spacing),1.,40.));
    if(s.scale==0) {
        const double desired=(high-low)/intervals;
        if(desired<=0)return ticks;
        const double power=std::pow(10.,std::floor(std::log10(desired)));
        if(power<=0)return ticks;
        double step=10*power;for(double factor:{1.,2.,2.5,5.,10.})if(factor*power>=desired){step=factor*power;break;}
        const double first=std::ceil(low/step-1e-10)*step;
        for(int i=0;i<=40;++i){const double hz=first+i*step;if(hz>high+step*1e-10)break;if(hz>=low-step*1e-10)ticks.push_back(std::max(0.,hz));}
    }else if(s.scale==2) {
        const int stride=std::max(1,int(std::ceil(std::log2(high/low)/intervals)));
        const int first=int(std::ceil(std::log2(low/1000.)/stride))*stride;
        for(int i=0;i<=40;++i){const double hz=1000.*std::exp2(first+i*stride);if(hz>high*(1+1e-10))break;ticks.push_back(hz);}
    }else {
        ticks.push_back(low);double last=0;
        for(int exponent=-3;exponent<=5;++exponent)for(double multiplier:{1.,2.,5.}){
            const double hz=multiplier*std::pow(10.,exponent);
            if(hz<=low || hz>=high)continue;
            const double position=std::log(hz/low)/std::log(high/low)*pixels;
            if(position-last>=spacing && pixels-position>=spacing){ticks.push_back(hz);last=position;}
        }
        ticks.push_back(high);
    }
    return ticks;
}
QString displayNumber(double value, int decimals, const QLocale &locale) {
    if(!std::isfinite(value)) return QString(QChar(0x2014));
    decimals=std::clamp(decimals,0,6);
    if(value!=0 && std::abs(value)<1)decimals=std::max(decimals,std::min(6,int(std::ceil(-std::log10(std::abs(value))))+2));
    if(std::abs(value)<.5*std::pow(10.,-decimals))value=0;
    auto text=locale.toString(value,'f',decimals);
    if(text.contains(locale.decimalPoint())) {
        while(text.endsWith(locale.zeroDigit())) text.chop(locale.zeroDigit().size());
        if(text.endsWith(locale.decimalPoint())) text.chop(locale.decimalPoint().size());
    }
    return text;
}
QString formatFrequency(double hz,const QLocale &locale) {
    if(!std::isfinite(hz) || hz<0) return QString(QChar(0x2014));
    return displayNumber(hz>=1000?hz/1000:hz,3,locale)+(hz>=1000?" kHz":" Hz");
}
std::vector<double> spectrogramColumn(const std::vector<double> &bins,const Settings &s,int rows) {
    rows=std::clamp(rows,1,2048);
    std::vector<double> column(rows,-240.);
    if(bins.empty() || !s.valid()) return column;
    auto frequency=[&](double r){return s.scale==0?s.low+r*(s.upper()-s.low):
        std::max(1.,s.low)*std::pow(s.upper()/std::max(1.,s.low),r);};
    auto binAt=[&](double r){return std::clamp(frequency(std::clamp(r,0.,1.))*s.fft/s.rate,0.,double(bins.size()-1));};
    for(int y=0;y<rows;++y) {
        const double r=rows==1?.5:double(rows-1-y)/(rows-1);
        const double a=binAt(r-.5/rows),b=binAt(r+.5/rows),center=binAt(r);
        const int first=int(center),next=std::min(first+1,int(bins.size())-1);
        double value=bins[first]+(bins[next]-bins[first])*(center-first);
        // Retain narrow FFT peaks when several bins must share a display row.
        if(b-a>1.)for(int i=int(std::ceil(a));i<=int(std::floor(b));++i)value=std::max(value,bins[i]);
        column[y]=value;
    }
    return column;
}
bool Settings::valid() const {
    const auto finite=[](double x){return std::isfinite(x);};
    return view>=0 && view<=6 && (rate==16000 || rate==22050 || rate==24000 || rate==32000 || rate==44100 || rate==48000 || rate==88200 || rate==96000)
        && (fft==512 || fft==1024 || fft==2048 || fft==4096 || fft==8192) && double(fft)/rate<=.25
        && palette>=0 && palette<=5 && scale>=0 && scale<=2 && layout>=0 && layout<=2 && channel>=0 && channel<=2 && amplitude>=0 && amplitude<=1 && instrument>=0 && instrument<=7
        && finite(low) && finite(high) && low>=0 && upper()>low && high>=0 && high<=std::min(rate,48000)/2.
        && finite(sensitivity) && sensitivity>=24 && sensitivity<=180 && finite(ceiling) && ceiling>=-120 && ceiling<=24
        && finite(history) && history>=5 && history<=60 && finite(speed) && speed>=.5 && speed<=4;
}
QJsonObject Settings::json() const {
    return {{"schema",1},{"view",view},{"rate",rate},{"fft",fft},{"palette",palette},{"scale",scale},{"layout",layout},{"channel",channel},{"amplitude",amplitude},{"instrument",instrument},
        {"low",low},{"high",high},{"sensitivity",sensitivity},{"history",history},{"speed",speed},{"ceiling",ceiling},
        {"grid",grid},{"peaks",peaks},{"average",average},{"autoRange",autoRange},{"notes",notes},{"transients",transients},{"music",music}};
}
std::optional<Settings> Settings::parse(const QJsonObject &j) {
    if(j.value("schema").toInt()!=1)return {};
    Settings s;
    // All serialized fields are required: malformed imports cannot silently become defaults.
    const auto defaults=s.json();for(auto i=defaults.begin();i!=defaults.end();++i) {
        if(i.value().isBool() ? !j.value(i.key()).isBool() : !j.value(i.key()).isDouble())return {};
        if(i.value().isDouble() && (!std::isfinite(j.value(i.key()).toDouble())))return {};
    }
    for(const auto &key:{"view","rate","fft","palette","scale","layout","channel","amplitude","instrument"})
        if(j.value(key).toDouble()!=j.value(key).toInt())return {};
#define INT(field) s.field=j.value(#field).toInt()
    INT(view);INT(rate);INT(fft);INT(palette);INT(scale);INT(layout);INT(channel);INT(amplitude);INT(instrument);
#undef INT
#define DBL(field) s.field=j.value(#field).toDouble()
    DBL(low);DBL(high);DBL(sensitivity);DBL(history);DBL(speed);DBL(ceiling);
#undef DBL
#define BOOL(field) s.field=j.value(#field).toBool()
    BOOL(grid);BOOL(peaks);BOOL(average);BOOL(autoRange);BOOL(notes);BOOL(transients);BOOL(music);
#undef BOOL
    return s.valid()?std::optional<Settings>(s):std::nullopt;
}
std::optional<Settings> Settings::parsePreset(const QByteArray &bytes) {
    if(bytes.size()>65536)return {};
    if(bytes.trimmed().startsWith('{'))return parse(QJsonDocument::fromJson(bytes).object());
    Settings s;s.low=0;s.high=0;QSet<QString> seen;
    for(const auto &raw:bytes.split('\n')){
        const auto line=QString::fromUtf8(raw).trimmed();if(line.isEmpty() || line.startsWith('#'))continue;
        const int separator=line.indexOf('=');if(separator<1)return {};
        const auto key=line.left(separator).trimmed(),value=line.mid(separator+1).trimmed().toLower();
        if(seen.contains(key))return {};seen.insert(key);bool ok=false;
        auto number=[&](double &target){target=value.toDouble(&ok);return ok && std::isfinite(target);};
        auto choice=[&](const QStringList &choices,int &target){target=choices.indexOf(value);return target>=0;};
        auto flag=[&](bool &target){if(value!="0" && value!="1")return false;target=value=="1";return true;};
        if(key=="sample_rate"){s.rate=value.toInt(&ok);if(!ok)return {};}
        else if(key=="fft"){s.fft=value.toInt(&ok);if(!ok)return {};}
        else if(key=="palette"){if(!choice({"spectrum","gray","blue","green","amber","ice"},s.palette))return {};}
        else if(key=="scale"){if(!choice({"linear","logarithmic","octave"},s.scale))return {};}
        else if(key=="layout"){if(!choice({"auto","mono","stereo"},s.layout))return {};}
        else if(key=="display"){if(!choice({"spectrogram","spectrum","waveform","tuner","vectorscope","room","microphone"},s.view))return {};}
        else if(key=="amplitude"){if(!choice({"db","linear"},s.amplitude))return {};}
        else if(key=="mono_channel"){if(value=="stereo"){s.channel=0;s.layout=2;}else if(!choice({"mix","left","right"},s.channel))return {};}
        else if(key=="instrument"){if(!choice({"chromatic","guitar","bass","ukulele","violin","viola","cello","mandolin"},s.instrument))return {};}
        else if(key=="min_freq"){if(!number(s.low))return {};}
        else if(key=="max_freq"){if(value=="auto")s.high=0;else if(!number(s.high))return {};}
        else if(key=="history"){if(!number(s.history))return {};}
        else if(key=="scroll"){if(!number(s.speed))return {};}
        else if(key=="sensitivity"){if(!number(s.sensitivity))return {};}
        else if(key=="ceiling"){if(!number(s.ceiling))return {};}
        else if(key=="grid"){if(!flag(s.grid))return {};}
        else if(key=="peak_hold"){if(!flag(s.peaks))return {};}
        else if(key=="average"){if(!flag(s.average))return {};}
        else if(key=="music_mode"){if(!flag(s.music))return {};}
        else if(key=="note_grid"){if(!flag(s.notes))return {};}
        else if(key=="beat_marks"){if(!flag(s.transients))return {};}
        else if(key=="auto_range"){if(!flag(s.autoRange))return {};}
        else return {};
    }
    return seen.contains("sample_rate") && seen.contains("fft") && s.valid()?std::optional<Settings>(s):std::nullopt;
}
Engine::Engine(Settings s):settings(s) {if(!s.valid())throw std::invalid_argument("analysis settings");}
void Engine::configure(const Settings &s) {
    if(!s.valid())throw std::invalid_argument("analysis settings");
    const bool pipeline=s.rate!=settings.rate || s.fft!=settings.fft;
    const bool channelChanged=s.channel!=settings.channel;
    settings=s;
    if(pipeline){plan_.reset();kernelReady_=false;reference.clear();}
    if(pipeline || channelChanged){reference.clear();resetAverage();}
}
void Engine::resetAverage(){meanPower_.clear();averagePower_.clear();averaged.clear();peak.clear();frames=0;waveFrames=0;fftMs=0;lastRms_=0;pitch={};}
void Engine::captureReference(){reference=averaged;resetAverage();}
std::vector<double> Engine::relativeDb() const {
    if(reference.empty() || reference.size()!=averaged.size())return {};
    const double refTop=*std::max_element(reference.begin(),reference.end()),targetTop=*std::max_element(averaged.begin(),averaged.end());
    std::vector<double> result(averaged.size(),std::numeric_limits<double>::quiet_NaN());
    for(std::size_t i=0;i<result.size();++i)if(frequencyAt(i)>=settings.low && frequencyAt(i)<=settings.upper() && reference[i]>=std::max(-80.,refTop-50) && averaged[i]>=std::max(-80.,targetTop-50))result[i]=averaged[i]-reference[i];
    const double low=std::max(20.,settings.low),high=std::min(20000.,settings.upper());std::vector<double> anchors;
    int previous=-1;
    if(high>low)for(int i=0;i<96;++i){const double hz=low*std::pow(high/low,i/95.);const int bin=std::clamp(int(std::lround(hz*settings.fft/settings.rate)),1,int(result.size())-1);
        if(bin!=previous && frequencyAt(bin)>=low && frequencyAt(bin)<=high && std::isfinite(result[bin])){anchors.push_back(result[bin]);previous=bin;}}
    if(!anchors.empty()){std::sort(anchors.begin(),anchors.end());const double median=anchors[anchors.size()/2];for(auto &value:result)if(std::isfinite(value))value-=median;}
    return result;
}
void Engine::prepareKernel() {
    const double cutoff=std::min(1.,settings.rate/48000.)*.92;
    for(int phase=0;phase<256;++phase){double sum=0;
        for(int tap=0;tap<65;++tap){const double x=(tap-32)-phase/256.;
            const double sinc=std::abs(x)<1e-12?cutoff:std::sin(std::numbers::pi*cutoff*x)/(std::numbers::pi*x);
            const double window=std::abs(x)>32?0:.5+.5*std::cos(std::numbers::pi*x/32);
            sum+=(kernel_[phase][tap]=sinc*window);
        }
        for(auto &x:kernel_[phase])x/=sum;
    }
    kernelReady_=true;
}
static double pcm(const QByteArray &bytes,int frame,int channel) {
    const auto *p=reinterpret_cast<const unsigned char *>(bytes.constData())+frame*4+channel*2;
    return std::int16_t(std::uint16_t(p[0]) | (std::uint16_t(p[1])<<8))/32768.;
}
void Engine::analyzePitch(const QByteArray &bytes) {
    // Bounded 4 kHz YIN-like normalized-difference estimator; no work on the audio callback.
    const int count=std::min(1024,int(bytes.size()/4)/12);pitch={};if(count<256)return;
    std::vector<double> x(count);const int start=bytes.size()/4-count*12;double energy=0;
    for(int i=0;i<count;++i){double value=0;for(int j=0;j<12;++j){int k=start+i*12+j;value+=settings.channel==1?pcm(bytes,k,0):settings.channel==2?pcm(bytes,k,1):.5*(pcm(bytes,k,0)+pcm(bytes,k,1));}x[i]=value/12.;energy+=x[i]*x[i];}
    if(energy/count<1e-7)return;
    const int maxLag=std::min(146,count/2),minLag=2;std::vector<double> d(maxLag+1);double sum=0;
    for(int lag=1;lag<=maxLag;++lag){double v=0;for(int i=0;i<count-maxLag;++i){const double delta=x[i]-x[i+lag];v+=delta*delta;}sum+=v;d[lag]=sum>1e-20?v*lag/sum:1;}
    int best=0;
    for(int lag=minLag;lag<maxLag;++lag)if(d[lag]<.15 && d[lag]<=d[lag-1] && d[lag]<d[lag+1]){best=lag;break;}
    if(!best)return;
    const double a=d[best-1],b=d[best],c=d[best+1],den=a-2*b+c;
    const double refined=best+(std::abs(den)>1e-15?std::clamp(.5*(a-c)/den,-.5,.5):0);
    double hz=4000/refined;
    // Refine the coarse period against the original-rate samples. Interpolating
    // only a 4 kHz lag minimum is not precise enough for a readable tuner.
    const int center=int(std::lround(48000/hz)),available=bytes.size()/4;
    if(center>=4 && center+6<available/2){
        const int length=std::min(4096,available-center-6),first=available-length-center-6;
        auto sample=[&](int i){return settings.channel==1?pcm(bytes,i,0):settings.channel==2?pcm(bytes,i,1):.5*(pcm(bytes,i,0)+pcm(bytes,i,1));};
        auto difference=[&](int lag){double sum=0;for(int i=0;i<length;++i){const double delta=sample(first+i)-sample(first+i+lag);sum+=delta*delta;}return sum;};
        int period=center;double minimum=difference(center);
        for(int lag=std::max(2,center-4);lag<=center+4;++lag){const double error=difference(lag);if(error<minimum){minimum=error;period=lag;}}
        const double left=difference(period-1),right=difference(period+1),denominator=left-2*minimum+right;
        const double fine=period+(std::abs(denominator)>1e-18?std::clamp(.5*(left-right)/denominator,-.5,.5):0);
        hz=48000/fine;
    }
    const double midi=69+12*std::log2(hz/440.);
    int target=int(std::lround(midi));
    static const std::vector<std::vector<int>> strings={{},{40,45,50,55,59,64},{28,33,38,43},{67,60,64,69},{55,62,69,76},{48,55,62,69},{36,43,50,57},{55,62,69,76}};
    if(settings.instrument>0){const auto &notes=strings[settings.instrument];target=*std::min_element(notes.begin(),notes.end(),[midi](int a,int b){return std::abs(a-midi)<std::abs(b-midi);});}
    pitch={hz,100*(midi-target),1-b,target};
}
bool Engine::push(const QByteArray &bytes,View view) {
    if(bytes.size()%4)return false;
    const int available=bytes.size()/4;
    const int needed=int(std::ceil((settings.fft-1)*48000./settings.rate))+65;
    if(available<needed)return false;
    const bool frequency=view==View::Waterfall || view==View::Spectrum || view==View::Average || view==View::Microphone;

    left.resize(settings.fft);right.resize(settings.fft);
    if(settings.rate==48000){for(int i=0;i<settings.fft;++i){left[i]=pcm(bytes,available-settings.fft+i,0);right[i]=pcm(bytes,available-settings.fft+i,1);}}
    else {
        if(!kernelReady_)prepareKernel();
        const int start=available-needed+32;
        for(int i=0;i<settings.fft;++i){const double pos=i*48000./settings.rate;int base=int(pos),phase=int(std::lround((pos-base)*256));if(phase==256){phase=0;++base;}
            double l=0,r=0;for(int tap=0;tap<65;++tap){int k=start+base+tap-32;const double w=kernel_[phase][tap];l+=w*pcm(bytes,k,0);r+=w*pcm(bytes,k,1);}left[i]=l;right[i]=r;
        }
    }
    ++waveFrames;double ll=0,rr=0,lr=0;
    for(int i=0;i<settings.fft;++i){ll+=left[i]*left[i];rr+=right[i]*right[i];lr+=left[i]*right[i];}
    rms=std::sqrt((ll+rr)/(2*settings.fft));correlation=ll*rr>1e-20?lr/std::sqrt(ll*rr):0;
    transient=rms>std::max(.02,lastRms_*2);lastRms_=.9*lastRms_+.1*rms;
    if(view==View::Tuner || settings.music)analyzePitch(bytes);
    if(!frequency){fftMs=0;return true;}
    if(!plan_)plan_=std::make_unique<gram::FftPlan>(settings.fft);
    gram::ProfilingStats timing;
    plan_->compute_db(left.data(),dbLeft,-240,&timing);plan_->compute_db(right.data(),dbRight,-240,&timing);fftCalls+=2;
    db.resize(dbLeft.size());if(meanPower_.size()!=db.size()){meanPower_.assign(db.size(),0);averagePower_.assign(db.size(),0);peak.assign(db.size(),-240);}
    ++frames;averaged.resize(db.size());const double amplitudeGain=4.*settings.fft/(settings.fft-1);
    for(std::size_t i=0;i<db.size();++i){const double normalization=20*std::log10((i==0 || i==db.size()-1)?amplitudeGain/2:amplitudeGain);
        dbLeft[i]+=normalization;dbRight[i]+=normalization;
        const double l=std::pow(10.,dbLeft[i]/10.),r=std::pow(10.,dbRight[i]/10.);
        const double p=settings.channel==1?l:settings.channel==2?r:(l+r)/2;
        db[i]=10*std::log10(std::max(p,1e-24));
        meanPower_[i]+=(p-meanPower_[i])/double(frames);
        averagePower_[i]=frames==1?p:.95*averagePower_[i]+.05*p;
        averaged[i]=10*std::log10(std::max((view==View::Microphone?meanPower_[i]:averagePower_[i]),1e-24));
        peak[i]=std::max(peak[i],db[i]);
    }
    const double frameFftMs=timing.average_fft_ms*2;
    fftMs=fftCalls==2?frameFftMs:.9*fftMs+.1*frameFftMs;return true;
}
}
