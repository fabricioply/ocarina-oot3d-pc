#include "real_instrument/game_input.h"
#include <cmath>
#include "real_instrument/song_catalog.h"
namespace real_instrument {
OcarinaInputUpdate OcarinaGameInput::Update(const OcarinaGameState& state,bool manual,double t){
    if(!std::isfinite(t)){Reset();return {0,false,1,generation_};}
    if(lastTime_>=0 && t<lastTime_)Reset();
    if(lastTime_>=0 && t-lastTime_>0.25 && phase_==Phase::Replay){phase_=Phase::AwaitClose;pending_.reset();++generation_;}
    lastTime_=t;
    if(!state.canPlay){if(phase_!=Phase::Idle)Reset();return {0,false,1,generation_};}
    if(context_!=state.context){Reset();context_=state.context;lastTime_=t;}
    sariaLearned_=state.songs[8].learned;stormsLearned_=state.songs[9].learned;
    for(size_t i=0;i<12;++i)learned_[i]=state.songs[i].learned;
    if(phase_==Phase::Idle){phase_=Phase::Listening;++generation_;}
    if(manual){phase_=Phase::Manual;pending_.reset();return {0,false,1,generation_};}
    if(phase_==Phase::Listening && pending_){
        const NativeSongInput* song=nullptr;
        const auto& catalog=NativeSongCatalog();
        for(size_t i=0;i<catalog.size();++i)
            if(pending_->id==catalog[i].id)song=&state.songs[i];
        bool valid=song && song->learned && song->buttons.size()>=5 && song->buttons.size()<=8 && std::isfinite(pending_->confidence) && pending_->confidence>=0.8f;
        if(song)for(uint32_t b:song->buttons)if(!b || (b&(b-1)) || (b&0xF01U)!=b)valid=false;
        pending_.reset();++generation_;
        if(valid){replay_=song->buttons;replayStart_=t+replayDelay_;phase_=Phase::Replay;}
        replayDelay_=0;
    }
    if(phase_==Phase::Replay){
        double elapsed=t-replayStart_;
        if(elapsed<0)return {0,false,1,generation_}; // waiting before the replay
        if(elapsed>=static_cast<double>(replay_.size())*0.2-1e-7){phase_=Phase::AwaitClose;return {0,false,1,generation_};}
        size_t index=static_cast<size_t>((elapsed+1e-7)/0.2);
        double within=elapsed-static_cast<double>(index)*0.2;
        return {within<0.12 ? replay_[index] : 0U,false,1,generation_};
    }
    const bool listen=phase_==Phase::Listening;
    return {0,listen,listen?0.f:1.f,generation_};
}
void OcarinaGameInput::Recognized(SongResult result){if(phase_==Phase::Listening)pending_=std::move(result);}
void OcarinaGameInput::OfferConfirmation(){
    if(phase_==Phase::Listening){phase_=Phase::Confirm;pending_.reset();++generation_;offered_={"Saria's Song","Song of Storms"};}
}
bool OcarinaGameInput::OfferConfirmation(const std::vector<std::string>& ids){
    if(phase_!=Phase::Listening)return false;
    std::vector<std::string> kept;
    const auto& catalog=NativeSongCatalog();
    for(const auto& id:ids)for(size_t i=0;i<catalog.size();++i)
        if(id==catalog[i].id && learned_[i])kept.push_back(id);
    if(kept.empty())return false;
    phase_=Phase::Confirm;pending_.reset();++generation_;offered_=std::move(kept);return true;
}
std::optional<SongConfirmation> OcarinaGameInput::Confirmation()const{
    if(phase_!=Phase::Confirm)return {};
    SongConfirmation c{generation_,sariaLearned_,stormsLearned_,{}};
    const auto& catalog=NativeSongCatalog();
    for(const auto& id:offered_){
        bool learned=false;
        for(size_t i=0;i<catalog.size();++i)if(id==catalog[i].id)learned=learned_[i];
        c.choices.push_back({id,learned});
    }
    return c;
}
bool OcarinaGameInput::ResolveConfirmation(uint64_t token,int choice,double replayDelaySeconds){
    if(phase_!=Phase::Confirm || token!=generation_ || choice<0 || static_cast<size_t>(choice)>offered_.size())return false;
    if(static_cast<size_t>(choice)<offered_.size()){
        const auto& catalog=NativeSongCatalog();bool learned=false;
        for(size_t i=0;i<catalog.size();++i)if(offered_[choice]==catalog[i].id)learned=learned_[i];
        if(!learned)return false;
    }
    phase_=Phase::Listening;++generation_;
    if(static_cast<size_t>(choice)<offered_.size()){pending_=SongResult{offered_[choice],1};replayDelay_=replayDelaySeconds>0?replayDelaySeconds:0;}
    return true;
}
void OcarinaGameInput::Reset(){phase_=Phase::Idle;context_=0;pending_.reset();lastTime_=-1;++generation_;}
}
