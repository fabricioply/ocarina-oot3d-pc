#include "real_instrument/session.h"
#include "test_support.h"
#include "real_instrument/song_catalog.h"
#include <random>
using namespace real_instrument;
int main() {
    auto pcm=tone(440);
    RecognitionSession s;
    CHECK(!s.Process(pcm.data(),4096,1,48000,0).hasPitch);
    s.Start(); auto first=s.Process(pcm.data(),4096,1,48000,0);
    CHECK(first.hasPitch); CHECK(first.pitch.voiced); CHECK(std::abs(first.pitch.midiPitch-69)<0.25f);
    s.Stop(); CHECK(!s.Process(pcm.data(),4096,1,48000,0.1).hasPitch);
    s.Start(); std::vector<float> stereo(8192);
    for(size_t i=0;i<4096;++i) stereo[2*i]=stereo[2*i+1]=pcm[i];
    auto two=s.Process(stereo.data(),4096,2,48000,0); CHECK(two.hasPitch); CHECK(std::abs(two.pitch.midiPitch-69)<0.25f);
    std::vector<float> silence(4096,0);s.Reset();
    CHECK(!s.Process(silence.data(),4096,1,48000,0).pitch.voiced);
    auto changed=s.Process(silence.data(),4096,1,44100,0.09); CHECK(changed.discontinuity);
    auto gap=s.Process(silence.data(),4096,1,44100,3); CHECK(gap.discontinuity);
    std::vector<float> large(24000,0); auto overflow=s.Process(large.data(),large.size(),1,48000,4);
    CHECK(overflow.discontinuity); CHECK(!overflow.song);
    CHECK(!s.Process(nullptr,4096,1,48000,5).hasPitch);
    s.Start(); double t=0;std::optional<SongResult> result;
    for(double hz:{440.0,293.664768,349.228231,440.0,293.664768,349.228231}) {
        auto block=tone(hz,false,12000);
        auto a=s.Process(block.data(),block.size(),1,48000,t); if(a.song)result=a.song; t+=0.25;
        std::vector<float> rest(4800,0);
        auto b=s.Process(rest.data(),rest.size(),1,48000,t);if(b.song)result=b.song;t+=0.1;
    }
    CHECK(result); CHECK(result->id=="Song of Time");
    // A configured quiet microphone must be accepted by every stage, not just
    // the pitch estimator. The default floor must still reject this amplitude.
    auto quiet=tone(440,false,12000);
    for(auto& sample:quiet)sample*=0.02f;
    RecognitionSession sensitive(true,0.002f);sensitive.Start();
    auto quietStart=sensitive.Process(quiet.data(),quiet.size(),1,48000,0);
    CHECK(quietStart.pitch.voiced);
    std::vector<float> quietRest(9600,0);
    auto quietEnd=sensitive.Process(quietRest.data(),quietRest.size(),1,48000,0.25);
    CHECK(quietEnd.notes.size()==1);
    CHECK(std::abs(quietEnd.notes[0].midiPitch-69)<0.25f);
    RecognitionSession normal;normal.Start();
    normal.Process(quiet.data(),quiet.size(),1,48000,0);
    CHECK(normal.Process(quietRest.data(),quietRest.size(),1,48000,0.25).notes.empty());
    // Rhythmic, connected Lullaby: variable note lengths, no artificial gaps,
    // and a decaying envelope within each sustained note.
    RecognitionSession rhythmic;rhythmic.Start();t=0;result.reset();size_t emitted=0;
    const double frequencies[]={493.883301,587.329536,440,493.883301,587.329536,440};
    const size_t lengths[]={40800,20400,62400,40800,20400,80000};
    for(size_t n=0;n<6;++n){
        auto block=tone(frequencies[n],true,lengths[n]);
        for(size_t i=0;i<block.size();++i)block[i]*=static_cast<float>(std::exp(-3.0*i/block.size()));
        for(size_t offset=0;offset<block.size();offset+=480){
            size_t count=std::min<size_t>(480,block.size()-offset);
            auto update=rhythmic.Process(block.data()+offset,count,1,48000,t);
            t+=static_cast<double>(count)/48000;emitted+=update.notes.size();if(update.song)result=update.song;
        }
    }
    std::vector<float> finalRest(9600,0);
    auto finish=rhythmic.Process(finalRest.data(),finalRest.size(),1,48000,t);
    emitted+=finish.notes.size();if(finish.song)result=finish.song;
    CHECK(emitted==6);CHECK(result);CHECK(result->id=="Zelda's Lullaby");
    // Musical timing: short connected notes must survive the analysis-window
    // latency plus the stability gate (the previous combination lost them).
    RecognitionSession quick;quick.Start();t=0;result.reset();emitted=0;
    for(double hz:frequencies){
        auto block=tone(hz,false,7200);
        for(size_t offset=0;offset<block.size();offset+=480){
            auto u=quick.Process(block.data()+offset,480,1,48000,t);t+=0.01;
            emitted+=u.notes.size();if(u.song)result=u.song;
        }
    }
    finish=quick.Process(finalRest.data(),finalRest.size(),1,48000,t);
    emitted+=finish.notes.size();if(finish.song)result=finish.song;
    CHECK(emitted==6);CHECK(result);CHECK(result->id=="Zelda's Lullaby");
    RecognitionSession expressive;expressive.Start();t=0;result.reset();double phase=0;
    auto gesture=[&](float from,float to,size_t count){
        std::vector<float> block(count);
        for(size_t i=0;i<count;++i){
            double pitch=from+(to-from)*static_cast<double>(i)/count;
            double hz=440*std::pow(2.0,(pitch-69)/12);
            phase+=6.283185307179586*hz/48000;
            block[i]=static_cast<float>(0.15*std::sin(phase)+0.04*std::sin(2*phase));
        }
        for(size_t off=0;off<count;off+=480){
            size_t n=std::min<size_t>(480,count-off);
            auto u=expressive.Process(block.data()+off,n,1,48000,t);t+=static_cast<double>(n)/48000;
            if(u.song)result=u.song;
        }
    };
    gesture(69,69,19200);gesture(69,62,24000);gesture(62,62,19200);
    gesture(63,63,5760);gesture(62,62,14400);
    gesture(65,65,19200);gesture(69,69,19200);gesture(62,62,19200);gesture(65,65,19200);
    finish=expressive.Process(finalRest.data(),finalRest.size(),1,48000,t);if(finish.song)result=finish.song;
    CHECK(result);CHECK(result->id=="Song of Time");
    // End-to-end PCM: twelve melodies in a breathy low vocal register and
    // a high whistle register crossing the former 1600 Hz boundary.
    for(bool whistle:{false,true})for(const auto& song:SongCatalog()) {
        RecognitionSession vocal;vocal.Start();double clock=0,angle=0;
        std::optional<SongResult> recognized;
        std::mt19937 rng(42);std::uniform_real_distribution<float> breath(-0.035f,0.035f);
        auto send=[&](const std::vector<float>& block){
            for(size_t off=0;off<block.size();off+=480){
                size_t count=std::min<size_t>(480,block.size()-off);
                auto update=vocal.Process(block.data()+off,count,1,48000,clock);
                clock+=static_cast<double>(count)/48000;
                if(update.song)recognized=update.song;
            }
        };
        for(float interval:song.semitones){
            std::vector<float> block(19200);
            for(size_t i=0;i<block.size();++i){
                double pitch=(whistle?91.:50.)+interval+0.22*std::sin(6.283185307179586*5*i/48000.);
                angle+=6.283185307179586*440*std::pow(2.,(pitch-69)/12)/48000.;
                block[i]=static_cast<float>(0.05*std::sin(angle));
                if(!whistle)block[i]+=static_cast<float>(0.025*std::sin(2*angle)+0.012*std::sin(3*angle))+breath(rng);
            }
            send(block);send(std::vector<float>(5760,0));
        }
        if(!recognized)std::cerr<<"PCM failed: "<<song.id<<" whistle="<<whistle<<'\n';
        CHECK(recognized);CHECK(recognized->id==song.id);
    }
    // Performance-like phrasing: connected notes with a 30 ms transition,
    // 180 ms body and only 40 ms articulation between repeated pitches.
    // The older all-song PCM test used 120 ms silence after EVERY note.
    size_t failedMelodies=0;
    for(const auto& song:SongCatalog()){
        RecognitionSession performance;performance.Start();double clock=0,angle=0;
        std::optional<SongResult> found;std::vector<NoteEvent> events;
        auto send=[&](const std::vector<float>& block){
            for(size_t off=0;off<block.size();off+=480){
                size_t n=std::min<size_t>(480,block.size()-off);
                auto update=performance.Process(block.data()+off,n,1,48000,clock);clock+=static_cast<double>(n)/48000;
                events.insert(events.end(),update.notes.begin(),update.notes.end());if(update.song)found=update.song;
            }
        };
        float previous=song.semitones.front();
        for(size_t note=0;note<song.semitones.size();++note){
            float pitch=song.semitones[note];
            if(note && pitch==previous)send(std::vector<float>(1920,0));
            std::vector<float> block(10080);
            for(size_t i=0;i<block.size();++i){
                double blend=std::min(1.,static_cast<double>(i)/1440.);
                double midi=65+previous+(pitch-previous)*blend;
                angle+=6.283185307179586*440*std::pow(2.,(midi-69)/12)/48000.;
                block[i]=static_cast<float>(0.12*std::sin(angle)+0.04*std::sin(2*angle));
            }
            send(block);previous=pitch;
        }
        send(std::vector<float>(9600,0));
        if(!found || found->id!=song.id){
            ++failedMelodies;std::cerr<<"Phrasing failed: "<<song.id<<" -> "<<(found?found->id:"none")<<" notes:";
            for(const auto& event:events)std::cerr<<' '<<event.midiPitch;
            std::cerr<<'\n';
        }
    }
    CHECK(failedMelodies==0);
}
