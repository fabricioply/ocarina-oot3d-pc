#include "real_instrument/note_tracker.h"
#include "test_support.h"
using namespace real_instrument;
int main() {
    NoteTracker tracker; std::vector<NoteEvent> notes;
    double t=0;
    auto feed=[&](float pitch,int frames,float rms=0.15f,bool vibrato=false) {
        for(int i=0;i<frames;++i,t+=0.01) {
            auto event=tracker.Push({t,pitch+(vibrato ? 0.3f*static_cast<float>(std::sin(i)) : 0),0.99f,rms,rms>0});
            if(event) notes.push_back(*event);
        }
    };
    feed(69,100,0.15f,true); CHECK(notes.empty());
    feed(0,10,0); CHECK(notes.size()==1); CHECK(std::abs(notes[0].midiPitch-69)<0.1f);
    CHECK(notes[0].endedAt-notes[0].startedAt>0.9);
    tracker.Reset(); notes.clear();
    feed(69,20); feed(0,8,0); feed(69,20); feed(0,8,0); CHECK(notes.size()==2);
    tracker.Reset(); notes.clear();
    feed(69,20); feed(72,1); feed(69,20); feed(0,8,0); CHECK(notes.size()==1);
    tracker.Reset(); notes.clear();
    feed(69,1); feed(0,8,0); CHECK(notes.empty());
    feed(69,20); tracker.Reset(); feed(0,8,0); CHECK(notes.empty());
    feed(69,20); feed(69,8,0.01f); feed(69,20); feed(0,8,0); CHECK(notes.size()==2);
    tracker.Reset(); notes.clear();
    feed(69,20); feed(62,20); feed(65,20); feed(0,8,0); CHECK(notes.size()==3);
    tracker.Reset(); notes.clear();
    feed(69,20); t+=1; feed(0,8,0); CHECK(notes.empty());
    tracker.Reset(); notes.clear();
    // A vocal attack can begin slightly sharp and then settle. The completed
    // note must represent the stable body, rather than freezing the attack.
    feed(69.6f,10); feed(69,60); feed(0,8,0);
    CHECK(notes.size()==1); CHECK(std::abs(notes[0].midiPitch-69)<0.2f);
    tracker.Reset(); notes.clear();
    // Sustaining an instrument while its volume decays is still ONE note.
    // A lifetime peak gate used to manufacture an extra onset below 25%.
    for(int i=0;i<150;++i)feed(69,1,0.2f*static_cast<float>(std::exp(-i/50.0)));
    feed(0,10,0);CHECK(notes.size()==1);
    tracker.Reset();notes.clear();
    // D -> D# -> D while holding the note is an excursion, not three notes.
    feed(62,40);feed(63,12);feed(62,40);feed(0,10,0);
    CHECK(notes.size()==1);CHECK(std::abs(notes[0].midiPitch-62)<0.2f);
    tracker.Reset();notes.clear();
    // A slow continuous slide must not become staircase notes. Only its
    // stable start and landing count.
    feed(69,30);
    for(int i=1;i<=70;++i)feed(69-static_cast<float>(i)*0.1f,1);
    feed(62,30);feed(0,10,0);
    CHECK(notes.size()==2);CHECK(std::abs(notes[0].midiPitch-69)<0.2f);
    CHECK(std::abs(notes[1].midiPitch-62)<0.2f);
    tracker.Reset();notes.clear();
    // An abrupt pull-off is a true new landing even without silence.
    feed(69,20);feed(62,12);feed(0,10,0);CHECK(notes.size()==2);
    tracker.Reset();notes.clear();
    // An intentional, sustained semitone change still counts.
    feed(62,30);feed(63,35);feed(0,10,0);CHECK(notes.size()==2);
}
