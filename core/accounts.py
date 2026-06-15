"""Steam accounts: discovery + game->account mapping.

Accounts known to Steam are read from config/loginusers.vdf. Which account owns
which installed game is resolved primarily from the local appmanifest files:
each one records `LastOwner`, the SteamID64 of the account that installed that
copy — exactly the account whose login is needed to launch it. No network or API
key required. The Steam Web API path is kept only as a fallback for the rare
case where `LastOwner` is missing (e.g. some family-shared installs). Manual
overrides and any Web-API results are cached in data/mapping.json.
"""

from __future__ import annotations

import json
import urllib.request
import urllib.error
from dataclasses import dataclass, asdict
from pathlib import Path

from . import vdf, steam_paths, games


DATA_DIR = Path(__file__).resolve().parent.parent / "data"
MAPPING_FILE = DATA_DIR / "mapping.json"


@dataclass
class Account:
    steamid64: str
    account_name: str       # the login name (what AutoLoginUser expects)
    persona_name: str       # the display name
    most_recent: bool
    remember_password: bool

    def to_dict(self) -> dict:
        return asdict(self)


def list_accounts(root: Path | None = None) -> list[Account]:
    root = root or steam_paths.steam_root()
    if not root:
        return []
    f = root / "config" / "loginusers.vdf"
    if not f.exists():
        return []
    data = vdf.load(f).get("users", {})
    out: list[Account] = []
    for steamid64, info in data.items():
        if not isinstance(info, dict):
            continue
        out.append(
            Account(
                steamid64=steamid64,
                account_name=info.get("AccountName", ""),
                persona_name=info.get("PersonaName", info.get("AccountName", "")),
                most_recent=info.get("MostRecent", "0") == "1",
                remember_password=info.get("RememberPassword", "0") == "1",
            )
        )
    return out


# ---------------------------------------------------------------------------
# Local mapping: read LastOwner straight out of the appmanifest files. This is
# the default, offline, key-free way to know which account owns each game.
# ---------------------------------------------------------------------------

STEAMID64_BASE = 76561197960265728

_local_owner_cache: dict[int, str] | None = None
_userdata_cache: dict[int, list[str]] | None = None


def local_owner_map(root: Path | None = None, *, refresh: bool = False) -> dict[int, str]:
    """{appid: steamid64} from each installed game's LastOwner field.

    LastOwner is the account that owns the *license*. For a normally-owned game
    that's also the account you log into; for a family-shared game it's the
    lender, not the local player — see userdata_owner_map for that case.

    Cached after the first scan; pass refresh=True (or call after the installed
    games change) to rebuild it.
    """
    global _local_owner_cache
    if _local_owner_cache is not None and not refresh:
        return _local_owner_cache
    out: dict[int, str] = {}
    for g in games.installed_games(root):
        if g.last_owner and g.last_owner != "0":
            out[g.appid] = g.last_owner
    _local_owner_cache = out
    return out


def userdata_owner_map(root: Path | None = None, *, refresh: bool = False) -> dict[int, list[str]]:
    """{appid: [steamid64, ...]} from local userdata/<accountid>/<appid>/ folders.

    Steam creates a per-app userdata folder for the account that actually has the
    game in its library and has run it — so this resolves the local *player* even
    when LastOwner points at a family-share lender. An account may legitimately
    appear for several games; a game may list several accounts if both played it.
    """
    global _userdata_cache
    if _userdata_cache is not None and not refresh:
        return _userdata_cache
    out: dict[int, list[str]] = {}
    root = root or steam_paths.steam_root()
    udroot = root / "userdata" if root else None
    if udroot and udroot.is_dir():
        for acc_dir in udroot.iterdir():
            if not (acc_dir.is_dir() and acc_dir.name.isdigit()):
                continue
            steamid64 = str(int(acc_dir.name) + STEAMID64_BASE)
            for app_dir in acc_dir.iterdir():
                if app_dir.is_dir() and app_dir.name.isdigit():
                    out.setdefault(int(app_dir.name), []).append(steamid64)
    _userdata_cache = out
    return out


# ---------------------------------------------------------------------------
# Mapping store: { "api_keys": {steamid64: key}, "overrides": {appid: steamid64},
#                  "owned": {steamid64: [appid, ...]} }
# ---------------------------------------------------------------------------

def _load_mapping() -> dict:
    if MAPPING_FILE.exists():
        try:
            return json.loads(MAPPING_FILE.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            pass
    return {"api_keys": {}, "overrides": {}, "owned": {}}


def _save_mapping(m: dict) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    MAPPING_FILE.write_text(json.dumps(m, indent=2), encoding="utf-8")


def set_api_key(steamid64: str, api_key: str) -> None:
    m = _load_mapping()
    m["api_keys"][steamid64] = api_key
    _save_mapping(m)


def set_override(appid: int, steamid64: str) -> None:
    """Manually pin a game to an account (wins over auto-detection)."""
    m = _load_mapping()
    m["overrides"][str(appid)] = steamid64
    _save_mapping(m)


def fetch_owned_games(steamid64: str, api_key: str, timeout: int = 15) -> list[int]:
    """Query the Steam Web API for the appids an account owns."""
    url = (
        "https://api.steampowered.com/IPlayerService/GetOwnedGames/v1/"
        f"?key={api_key}&steamid={steamid64}&include_played_free_games=1&format=json"
    )
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
    except (urllib.error.URLError, json.JSONDecodeError, TimeoutError) as exc:
        raise RuntimeError(f"Steam Web API request failed: {exc}") from exc
    games = payload.get("response", {}).get("games", [])
    return [int(g["appid"]) for g in games]


def refresh_owned(steamid64: str) -> list[int]:
    """Refresh and cache the owned-games list for one account."""
    m = _load_mapping()
    key = m["api_keys"].get(steamid64)
    if not key:
        raise RuntimeError(f"No Steam Web API key stored for {steamid64}")
    appids = fetch_owned_games(steamid64, key)
    m["owned"][steamid64] = appids
    _save_mapping(m)
    return appids


def account_for_game(appid: int, accounts: list[Account] | None = None) -> str | None:
    """Owning account for a game, resolved entirely from local data (no API key).

    Order of preference:
      1. manual override (always wins);
      2. LastOwner, when it's an account logged in on this PC (normal case);
      3. userdata folder owner, when exactly one local account has run the game
         (this is what catches family-shared games, whose LastOwner is the
         lender rather than the local player);
      4. cached Web-API ownership, if present (legacy fallback).

    Returns None when nothing maps — e.g. a family-shared game whose owner isn't
    on this PC and which no local account has launched yet.
    """
    m = _load_mapping()
    override = m["overrides"].get(str(appid))
    if override:
        return override

    local_ids = {a.steamid64 for a in (accounts if accounts is not None else list_accounts())}

    last_owner = local_owner_map().get(appid)
    if last_owner and last_owner in local_ids:
        return last_owner

    # LastOwner missing or not a local account (family share): use userdata.
    players = [sid for sid in userdata_owner_map().get(appid, []) if sid in local_ids]
    if len(players) == 1:
        return players[0]

    for steamid64, appids in m["owned"].items():
        if appid in appids:
            return steamid64
    return None


def all_owners(appid: int) -> list[str]:
    """Every account known to own this game (local LastOwner + cached Web API)."""
    m = _load_mapping()
    owners = [sid for sid, appids in m["owned"].items() if appid in appids]
    local = local_owner_map().get(appid)
    if local and local not in owners:
        owners.insert(0, local)
    return owners


if __name__ == "__main__":
    # Diagnostic: show every installed game, its appmanifest LastOwner, and the
    # account it resolves to (and how). Run: python -m core.accounts
    accts = {a.steamid64: a for a in list_accounts()}

    def label(sid: str | None) -> str:
        return accts[sid].account_name if sid in accts else (sid or "")

    print("Known accounts (loginusers.vdf):")
    for a in accts.values():
        print(f"  {a.steamid64}  {a.account_name} ({a.persona_name})")
    print()
    print(f"{'appid':>8}  {'LastOwner':>17}  game / resolution")
    for g in games.installed_games():
        owner = g.last_owner or "0"
        resolved = account_for_game(g.appid)
        if resolved and resolved in accts:
            if owner in accts:
                how = f"-> {label(resolved)}"
            else:
                how = f"-> {label(resolved)} (via userdata; LastOwner {owner} is a family-share lender)"
        elif owner == "0":
            how = "UNMAPPED — no LastOwner and no local account has run it"
        else:
            how = f"UNMAPPED — owner not on this PC and no local account has run it (id {owner})"
        print(f"{g.appid:>8}  {owner:>17}  {g.name}  [{how}]")
