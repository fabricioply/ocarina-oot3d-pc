#include "real_instrument/game_input.h"
#include "test_support.h"
using namespace real_instrument;
int main(){
    OcarinaGameInput game;OcarinaGameState state;
    CHECK(!game.Update(state,false,0).listen);
    state.canPlay=true;state.context=123;
    state.songs[5]={true,{2048,512,256,2048,512,256}};
    state.songs[0]={true,{1024,1,2048,1024,1,2048}};
    auto listening=game.Update(state,false,0.1);CHECK(listening.listen);CHECK(listening.audioGain==0);
    game.Recognized({"Song of Time",0.95f});
    auto first=game.Update(state,false,0.2);CHECK(first.buttons==2048);CHECK(!first.listen);CHECK(first.audioGain==1);
    CHECK(game.Update(state,false,0.33).buttons==0);
    CHECK(game.Update(state,false,0.41).buttons==512);
    CHECK(game.Update(state,false,0.61).buttons==256);
    CHECK(game.Update(state,false,0.81).buttons==2048);
    CHECK(game.Update(state,false,1.01).buttons==512);
    CHECK(game.Update(state,false,1.21).buttons==256);
    CHECK(game.Update(state,false,1.41).buttons==0);
    game.Recognized({"Song of Time",1});CHECK(!game.Update(state,false,1.5).listen);
    state.canPlay=false;game.Update(state,false,1.6);state.canPlay=true;
    CHECK(game.Update(state,false,1.7).listen);
    game.Recognized({"Zelda's Lullaby",1});CHECK(game.Update(state,false,1.8).buttons==1024);
    CHECK(game.Update(state,true,1.9).buttons==0);CHECK(!game.Update(state,false,2).listen);
    game.Reset();state.songs[5].learned=false;CHECK(game.Update(state,false,3).listen);
    game.Recognized({"Song of Time",1});auto denied=game.Update(state,false,3.1);
    CHECK(denied.listen);CHECK(!denied.buttons);
    CHECK(denied.generation!=listening.generation);
    game.Recognized({"unknown",1});CHECK(!game.Update(state,false,3.2).buttons);
    game.Recognized({"Zelda's Lullaby",1});state.context=456;
    CHECK(game.Update(state,false,3.3).listen);CHECK(!game.Update(state,false,3.4).buttons);
    game.Recognized({"Zelda's Lullaby",1});game.Update(state,false,3.5);
    state.canPlay=false;CHECK(!game.Update(state,false,3.6).buttons);
    CHECK(game.Update(state,false,3.7).audioGain==1);
    state.canPlay=true;game.Update(state,false,3.8);game.Recognized({"Zelda's Lullaby",1});game.Update(state,false,3.9);
    CHECK(game.Update(state,false,1).buttons==0); // savestate/clock rewind
    // Every standard melody replays its entire native sequence, including
    // the last notes of the 7/8-note songs and the 5-note Serenade.
    const size_t lengths[]={6,6,6,6,6,6,8,7,6,6,5,6};
    const char* ids[]={"Zelda's Lullaby","Sun's Song","Minuet of Forest","Requiem of Spirit",
        "Epona's Song","Song of Time","Bolero of Fire","Nocturne of Shadow","Saria's Song",
        "Song of Storms","Serenade of Water","Prelude of Light"};
    for(size_t i=0;i<12;++i){
        OcarinaGameInput input;OcarinaGameState active;active.canPlay=true;active.context=1;
        active.songs[i].learned=true;
        for(size_t j=0;j<lengths[i];++j)active.songs[i].buttons.push_back(j%2?0x100:0x400);
        CHECK(input.Update(active,false,0).listen);input.Recognized({ids[i],1});
        for(size_t j=0;j<lengths[i];++j){
            CHECK(input.Update(active,false,0.1+j*0.2).buttons==active.songs[i].buttons[j]);
            CHECK(input.Update(active,false,0.24+j*0.2).buttons==0);
        }
        auto complete=input.Update(active,false,0.1+lengths[i]*0.2);
        CHECK(!complete.buttons);CHECK(!complete.listen);
        input.Reset();active.songs[i].learned=false;input.Update(active,false,0);
        input.Recognized({ids[i],1});CHECK(input.Update(active,false,0.1).listen);
    }
    OcarinaGameInput chooser;OcarinaGameState choiceState;
    choiceState.canPlay=true;choiceState.context=5;
    choiceState.songs[8]={true,{256,2048,1024,256,2048,1024}};
    choiceState.songs[9]={true,{512,256,1,512,256,1}};
    chooser.Update(choiceState,false,0);chooser.OfferConfirmation();
    auto offer=chooser.Confirmation();CHECK(offer);CHECK(offer->sariaLearned);
    CHECK(!chooser.Update(choiceState,false,.1).listen);CHECK(!chooser.Update(choiceState,false,.2).buttons);
    CHECK(!chooser.ResolveConfirmation(offer->token+1,0));
    CHECK(chooser.ResolveConfirmation(offer->token,0));
    CHECK(!chooser.ResolveConfirmation(offer->token,1)); // never double-apply
    CHECK(chooser.Update(choiceState,false,.3).buttons==256);
    chooser.Reset();chooser.Update(choiceState,false,1);chooser.OfferConfirmation();
    offer=chooser.Confirmation();CHECK(offer);CHECK(chooser.ResolveConfirmation(offer->token,1));
    CHECK(chooser.Update(choiceState,false,1.1).buttons==512);
    chooser.Reset();choiceState.songs[8].learned=false;
    chooser.Update(choiceState,false,2);chooser.OfferConfirmation();offer=chooser.Confirmation();CHECK(offer);
    CHECK(!chooser.ResolveConfirmation(offer->token,0));
    CHECK(chooser.ResolveConfirmation(offer->token,2));CHECK(chooser.Update(choiceState,false,2.1).listen);
    chooser.OfferConfirmation();offer=chooser.Confirmation();choiceState.canPlay=false;
    chooser.Update(choiceState,false,2.2);CHECK(!chooser.Confirmation());
    CHECK(!chooser.ResolveConfirmation(offer->token,1));
    // Generic candidates: only learned songs are offered; any index can be chosen.
    OcarinaGameInput many;OcarinaGameState manyState;manyState.canPlay=true;manyState.context=9;
    manyState.songs[0]={true,{1,2048,1024,1,2048}};
    manyState.songs[1]={true,{256,2048,1024,256,2048}};
    many.Update(manyState,false,0);
    CHECK(!many.OfferConfirmation({"Song of Time"})); // not learned
    CHECK(many.OfferConfirmation({"Zelda's Lullaby","Song of Time","Sun's Song"}));
    auto manyOffer=many.Confirmation();CHECK(manyOffer);CHECK(manyOffer->choices.size()==2);
    CHECK(many.ResolveConfirmation(manyOffer->token,5)==false);
}
