#pragma once
// OcarinaMod (PC only): the in-game menu's "Microfone da Ocarina" tab over the
// microphone owner of real_instrument (MicController). Host configuration
// only; the song confirmation stays the game's native text box.
#include "oot3d_in_game_menu.h"
#include "real_instrument/mic_controller.h"

#include <memory>

namespace Oot3dNativeGame {

std::shared_ptr<InGameMenuMicrophone>
CreateOcarinaMicrophoneMenu(std::shared_ptr<real_instrument::MicController> controller);

} // namespace Oot3dNativeGame
