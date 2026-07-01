// Store abstraction for the multi-store library. Steam is the only store with
// account switching; Epic/GOG/Xbox just enumerate + launch. The Qt backend talks
// to this interface so adding a store doesn't touch the UI.

#pragma once

#include "../model.h"

#include <memory>
#include <string>
#include <vector>

namespace ss {

struct LaunchOptions {
    bool offline = false;
};

class IStore {
public:
    virtual ~IStore() = default;

    virtual Store kind() const = 0;

    // Installed games for this store (already populated with store + launchId).
    virtual std::vector<Game> scan() = 0;

    // Launch a game. `notify`/`shouldCancel` mirror launcher::play; stores that
    // don't switch accounts ignore offline and just fire the launch URI.
    virtual PlayResult launch(const Game& game, const LaunchOptions& opts,
                              const Notify& notify = {},
                              const std::function<bool()>& shouldCancel = {}) = 0;

    // Accounts this store knows about (empty for stores without switching).
    virtual std::vector<Account> accounts() { return {}; }
};

// Built-in stores (each returns nullptr-safe instances; absence handled by scan()).
std::unique_ptr<IStore> makeSteamStore();
std::unique_ptr<IStore> makeEpicStore();
std::unique_ptr<IStore> makeGogStore();
std::unique_ptr<IStore> makeXboxStore();

}  // namespace ss
