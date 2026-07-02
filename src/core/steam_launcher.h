// Orchestrate 'click a game -> play it'. Port of core/launcher.py.
//
//   1. Resolve the owning account.
//   2. Online: switch account, restart Steam, log in via the picker, launch.
//   3. Offline (shared-account use case): online login mints the cached session ->
//      set WantsOfflineMode=1 while Steam runs -> restart so Steam comes up OFFLINE
//      -> confirm via window+flag -> launch. Never uses the shared account online.

#pragma once

#include "model.h"
#include "steam_switcher.h"

#include <functional>
#include <string>

namespace ss::steam {

// Switch to the owning account and launch the game. `notify` (optional) gets
// progress text; `shouldCancel` (optional) is polled at each step and, if true,
// stops WITHOUT launching (so a wrong-game pick can be aborted).
PlayResult play(int64_t appid, bool offline = false, double loginWaitSec = 120.0,
                const Notify& notify = {}, const switcher::ShouldCancel& shouldCancel = {});

// Switch Steam to `steamid64` and restart it WITHOUT launching anything — the
// Accounts screen's "Switch now" action. Same validated sequence as the online
// launch (shutdown -> switchAccount -> start -> picker login), minus the game.
PlayResult switchTo(const std::string& steamid64, double loginWaitSec = 120.0,
                    const Notify& notify = {}, const switcher::ShouldCancel& shouldCancel = {});

}  // namespace ss::steam
