// XboxStore — wraps xbox_games behind IStore. Game Pass / Microsoft Store titles
// are UWP packages: no account switching, and launch() goes through the shell by
// AUMID (the game's launchId). The `offline` option is ignored.

#include "store.h"

#include "../xbox_games.h"

namespace ss {

namespace {

class XboxStore : public IStore {
public:
    Store kind() const override { return Store::Xbox; }

    std::vector<Game> scan() override { return xbox::installedGames(); }

    PlayResult launch(const Game& game, const LaunchOptions& /*opts*/,
                      const Notify& notify,
                      const std::function<bool()>& shouldCancel) override {
        if (game.launchId.empty())
            return PlayResult::fail("This Xbox game has no launch id.");
        if (shouldCancel && shouldCancel())
            return PlayResult::fail("Cancelled.");
        return xbox::launch(game.launchId, game.name, notify);
    }
};

}  // namespace

std::unique_ptr<IStore> makeXboxStore() { return std::make_unique<XboxStore>(); }

}  // namespace ss
