#include "../Source/DSP/YinPitchDetector.h"
#include "Baseline/YinPitchDetector061.h"
#include "Checks.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
struct Score { int correct=0, voiced=0, total=0; double milliseconds=0; };
template<class Detector> Score evaluate(const std::vector<float>& signal, double sr, float hz) {
    Detector d; d.prepare(sr); Score result;
    const int step=static_cast<int>(sr*0.008);
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<static_cast<int>(signal.size());++i) {
        d.pushSample(signal[static_cast<size_t>(i)]);
        if(i>sr*0.2 && i%step==0) {
            ++result.total;
            if(d.isVoiced()) {
                ++result.voiced;
                if(hz>0 && d.getFrequencyHz()>0 && std::abs(1200*std::log2(d.getFrequencyHz()/hz))<20) ++result.correct;
            }
        }
    }
    result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    return result;
}
template<class Detector> double stepResponse(double sr) {
    Detector d;d.prepare(sr);
    const int onset=static_cast<int>(sr*0.3), hop=static_cast<int>(sr*0.008);
    int stable=0;
    for(int i=0;i<static_cast<int>(sr*0.6);++i) {
        const double t=i/sr;
        const double phase=i<onset ? 220*t : 220*0.3+330*(t-0.3);
        d.pushSample(0.2f*static_cast<float>(std::sin(2*3.141592653589793*phase)));
        if(i>=onset && i%hop==0) {
            if(d.isVoiced() && std::abs(1200*std::log2(d.getFrequencyHz()/330))<20) ++stable; else stable=0;
            if(stable>=3)return 1000.0*(i-onset)/sr;
        }
    }
    return 300;
}
int main() {
    std::cout<<"sample_rate,fixture,old_correct,new_correct,observations,old_voiced,new_voiced,old_elapsed_ms,new_elapsed_ms\n";
    int oldAlias=0,newAlias=0;
    for(double sr : {44100.0,48000.0,96000.0}) {
        for(int kind=0;kind<8;++kind) {
            const char* names[]{"pure220","harmonics220","missing_fundamental220","harmonics_noise220","hf_interference220","white_noise","harmonics_noise80","harmonics_noise880"};
            const float fundamental = kind==6 ? 80.0f : kind==7 ? 880.0f : 220.0f;
            std::vector<float> signal(static_cast<size_t>(sr*0.6));
            std::mt19937 rng(1701);std::uniform_real_distribution<float> noise(-1,1);
            for(size_t i=0;i<signal.size();++i) {
                const double phase=2*3.141592653589793*fundamental*i/sr;
                float x=0.2f*static_cast<float>(std::sin(phase));
                if(kind==1||kind==3||kind>=6) x+=0.12f*static_cast<float>(std::sin(2*phase))+0.08f*static_cast<float>(std::sin(3*phase));
                if(kind==2)x=0.2f*static_cast<float>(std::sin(2*phase))+0.15f*static_cast<float>(std::sin(3*phase))+0.08f*static_cast<float>(std::sin(4*phase));
                if(kind==3||kind>=6)x+=0.06f*noise(rng);
                if(kind==4)x+=0.8f*static_cast<float>(std::sin(2*3.141592653589793*(sr/4-700)*i/sr));
                if(kind==5)x=0.2f*noise(rng);
                signal[i]=x;
            }
            const auto old=evaluate<BaselineYinPitchDetector>(signal,sr,kind==5?0:fundamental);
            const auto now=evaluate<YinPitchDetector>(signal,sr,kind==5?0:fundamental);
            std::cout<<sr<<','<<names[kind]<<','<<old.correct<<','<<now.correct<<','<<now.total<<','<<old.voiced<<','<<now.voiced<<','<<old.milliseconds<<','<<now.milliseconds<<'\n';
            if(kind<4 || kind>=6)CHECK(now.correct>=now.total*0.95);
            if(kind==5)CHECK(now.voiced<=now.total*0.1);
            if(kind==4){oldAlias+=old.correct;newAlias+=now.correct;CHECK(now.correct>=now.total*0.95);}
        }
        const double before=stepResponse<BaselineYinPitchDetector>(sr),after=stepResponse<YinPitchDetector>(sr);
        std::cout<<"step_response_ms,"<<sr<<','<<before<<','<<after<<'\n';
        if(sr<=48000)CHECK(after<=before);
        CHECK(after<110);
    }
    CHECK(newAlias>oldAlias);
    std::cout<<"PASS: difficult synthetic fixtures, noise rejection and step response\n";
}
