// SteamStore — wraps the ported steam_* core behind IStore. The account-switching
// store; launch() delegates to steam::play (online or offline flag method).

#include "store.h"

#include "../steam_accounts.h"
#include "../steam_games.h"
#include "../steam_launcher.h"

namespace ss {

namespace {

class SteamStore : public IStore {
public:
    Store kind() const override { return Store::Steam; }

    std::vector<Game> scan() override {
        auto games = steam::installedGames();
        for (auto& g : games) {
            g.store = Store::Steam;
            g.launchId = std::to_string(g.appid);
        }
        return games;
    }

    PlayResult launch(const Game& game, const LaunchOptions& opts,
                      const Notify& notify,
                      const std::function<bool()>& shouldCancel) override {
        return steam::play(game.appid, opts.offline, 120.0, notify, shouldCancel);
    }

    std::vector<Account> accounts() override { return steam::listAccounts(); }
};

}  // namespace

std::unique_ptr<IStore> makeSteamStore() { return std::make_unique<SteamStore>(); }

}  // namespace ss
