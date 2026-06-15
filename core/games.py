"""Enumerate games installed on disk by parsing appmanifest_*.acf files."""

from __future__ import annotations

from dataclasses import dataclass, asdict
from pathlib import Path

from . import vdf, steam_paths


# StateFlags is a bitfield; bit 2 (value 4) = "fully installed".
STATE_FULLY_INSTALLED = 4


@dataclass
class Game:
    appid: int
    name: str
    installdir: str
    library: str          # library folder path this game lives in
    fully_installed: bool
    last_owner: str = ""  # SteamID64 of the account that installed this copy

    def cover_url(self) -> str:
        # Portrait library capsule; the frontend falls back to header.jpg.
        return f"https://cdn.cloudflare.steamstatic.com/steam/apps/{self.appid}/library_600x900.jpg"

    def header_url(self) -> str:
        return f"https://cdn.cloudflare.steamstatic.com/steam/apps/{self.appid}/header.jpg"

    def to_dict(self) -> dict:
        d = asdict(self)
        d["cover_url"] = self.cover_url()
        d["header_url"] = self.header_url()
        return d


def _parse_manifest(path: Path, library: Path) -> Game | None:
    try:
        data = vdf.load(path).get("AppState", {})
    except Exception:
        return None
    if not data.get("appid"):
        return None
    try:
        state = int(data.get("StateFlags", "0"))
    except ValueError:
        state = 0
    return Game(
        appid=int(data["appid"]),
        name=data.get("name", f"App {data['appid']}"),
        installdir=data.get("installdir", ""),
        library=str(library),
        fully_installed=bool(state & STATE_FULLY_INSTALLED),
        last_owner=str(data.get("LastOwner", "0")),
    )


def installed_games(root: Path | None = None) -> list[Game]:
    """Every installed game across all library folders, sorted by name."""
    games: dict[int, Game] = {}
    for lib in steam_paths.library_folders(root):
        steamapps = lib / "steamapps"
        if not steamapps.exists():
            continue
        for manifest in steamapps.glob("appmanifest_*.acf"):
            game = _parse_manifest(manifest, lib)
            if game and game.appid not in games:
                games[game.appid] = game
    return sorted(games.values(), key=lambda g: g.name.lower())


if __name__ == "__main__":
    for g in installed_games():
        flag = "" if g.fully_installed else "  (partial)"
        owner = f"  owner={g.last_owner}" if g.last_owner and g.last_owner != "0" else "  owner=0"
        print(f"{g.appid:>8}  {g.name}{flag}{owner}")
