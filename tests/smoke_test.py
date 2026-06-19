"""Smoke tests for the OS-agnostic core, runnable on any platform.

Builds a synthetic Steam directory and verifies path discovery, game enumeration,
account listing, VDF round-tripping, and the loginusers MostRecent switch logic.
No real Steam install needed.
"""

import os
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from core import vdf, steam_paths, games, accounts, switcher  # noqa: E402


def build_fake_steam(root: Path):
    (root / "steamapps").mkdir(parents=True)
    (root / "config").mkdir(parents=True)

    # libraryfolders.vdf with the root + a second library
    lib2 = root.parent / "SteamLibrary"
    (lib2 / "steamapps").mkdir(parents=True)
    (root / "steamapps" / "libraryfolders.vdf").write_text(
        '"libraryfolders"\n{\n'
        f'\t"0"\n\t{{\n\t\t"path"\t\t"{root}"\n\t}}\n'
        f'\t"1"\n\t{{\n\t\t"path"\t\t"{lib2}"\n\t}}\n'
        "}\n"
    )

    # two installed games in different libraries
    # 440 is owned by alice, 570 by bob (per appmanifest LastOwner)
    (root / "steamapps" / "appmanifest_440.acf").write_text(
        '"AppState"\n{\n\t"appid"\t\t"440"\n\t"name"\t\t"Team Fortress 2"\n'
        '\t"StateFlags"\t\t"4"\n\t"installdir"\t\t"Team Fortress 2"\n'
        '\t"LastOwner"\t\t"76561198000000001"\n}\n'
    )
    (lib2 / "steamapps" / "appmanifest_570.acf").write_text(
        '"AppState"\n{\n\t"appid"\t\t"570"\n\t"name"\t\t"Dota 2"\n'
        '\t"StateFlags"\t\t"4"\n\t"installdir"\t\t"dota 2 beta"\n'
        '\t"LastOwner"\t\t"76561198000000002"\n}\n'
    )
    # 730 is family-shared: LastOwner is a lender NOT on this PC, but bob has run
    # it, so a userdata/<bob accountid>/730 folder exists -> should map to bob.
    (root / "steamapps" / "appmanifest_730.acf").write_text(
        '"AppState"\n{\n\t"appid"\t\t"730"\n\t"name"\t\t"Counter-Strike 2"\n'
        '\t"StateFlags"\t\t"4"\n\t"installdir"\t\t"csgo"\n'
        '\t"LastOwner"\t\t"76561198999999999"\n}\n'
    )
    bob_accountid = 76561198000000002 - 76561197960265728
    (root / "userdata" / str(bob_accountid) / "730").mkdir(parents=True)

    # 999 is a family share BOTH local accounts have run (each has a userdata
    # folder) -> ambiguous by folder alone. Bob actually plays it (more playtime),
    # alice just launched it once -> account_for_game must pick bob via playtime.
    # localconfig casing is varied on purpose to exercise the case-insensitive read.
    (root / "steamapps" / "appmanifest_999.acf").write_text(
        '"AppState"\n{\n\t"appid"\t\t"999"\n\t"name"\t\t"Shared Game"\n'
        '\t"StateFlags"\t\t"4"\n\t"installdir"\t\t"shared"\n'
        '\t"LastOwner"\t\t"76561198999999999"\n}\n'
    )
    alice_accountid = 76561198000000001 - 76561197960265728
    (root / "userdata" / str(alice_accountid) / "999").mkdir(parents=True)
    (root / "userdata" / str(bob_accountid) / "999").mkdir(parents=True)
    (root / "userdata" / str(alice_accountid) / "config").mkdir(parents=True)
    (root / "userdata" / str(bob_accountid) / "config").mkdir(parents=True)
    (root / "userdata" / str(alice_accountid) / "config" / "localconfig.vdf").write_text(
        '"UserLocalConfigStore"\n{\n\t"Software"\n\t{\n\t\t"Valve"\n\t\t{\n'
        '\t\t\t"steam"\n\t\t\t{\n\t\t\t\t"apps"\n\t\t\t\t{\n\t\t\t\t\t"999"\n'
        '\t\t\t\t\t{\n\t\t\t\t\t\t"Playtime"\t\t"5"\n\t\t\t\t\t}\n'
        '\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\n'
    )
    (root / "userdata" / str(bob_accountid) / "config" / "localconfig.vdf").write_text(
        '"UserLocalConfigStore"\n{\n\t"Software"\n\t{\n\t\t"Valve"\n\t\t{\n'
        '\t\t\t"Steam"\n\t\t\t{\n\t\t\t\t"apps"\n\t\t\t\t{\n\t\t\t\t\t"999"\n'
        '\t\t\t\t\t{\n\t\t\t\t\t\t"Playtime"\t\t"600"\n\t\t\t\t\t}\n'
        '\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\n'
    )

    # config.vdf with the account picker turned OFF — switching must turn it back
    # ON, since the picker click is what logs these accounts in.
    (root / "config" / "config.vdf").write_text(
        '"InstallConfigStore"\n{\n\t"Software"\n\t{\n\t\t"Valve"\n\t\t{\n'
        '\t\t\t"Steam"\n\t\t\t{\n\t\t\t\t"AlwaysShowUserChooser"\t\t"0"\n'
        '\t\t\t}\n\t\t}\n\t}\n}\n'
    )

    # two accounts
    (root / "config" / "loginusers.vdf").write_text(
        '"users"\n{\n'
        '\t"76561198000000001"\n\t{\n\t\t"AccountName"\t\t"alice"\n'
        '\t\t"PersonaName"\t\t"Alice"\n\t\t"RememberPassword"\t\t"1"\n'
        '\t\t"MostRecent"\t\t"1"\n\t}\n'
        '\t"76561198000000002"\n\t{\n\t\t"AccountName"\t\t"bob"\n'
        '\t\t"PersonaName"\t\t"Bob"\n\t\t"RememberPassword"\t\t"1"\n'
        '\t\t"MostRecent"\t\t"0"\n\t}\n'
        "}\n"
    )


def main():
    failures = 0

    def check(name, cond):
        nonlocal failures
        status = "PASS" if cond else "FAIL"
        if not cond:
            failures += 1
        print(f"  [{status}] {name}")

    # --- VDF round trip ---
    print("VDF parser:")
    sample = '"root"\n{\n\t"a"\t\t"1"\n\t"nested"\n\t{\n\t\t"b"\t\t"two"\n\t}\n}\n'
    parsed = vdf.loads(sample)
    check("parses nested keys", parsed["root"]["nested"]["b"] == "two")
    reparsed = vdf.loads(vdf.dumps(parsed))
    check("round-trips", reparsed["root"]["a"] == "1")
    check("escapes backslashes", vdf.loads(vdf.dumps({"p": "C:\\Steam"}))["p"] == "C:\\Steam")

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Steam"
        build_fake_steam(root)
        os.environ["STEAM_ROOT"] = str(root)
        # keep the mapping store inside the temp dir (don't touch the repo's data/)
        accounts.DATA_DIR = Path(tmp) / "data"
        accounts.MAPPING_FILE = accounts.DATA_DIR / "mapping.json"

        print("steam_paths:")
        check("finds root", steam_paths.steam_root() == root)
        libs = steam_paths.library_folders()
        check("finds 2 libraries", len(libs) == 2)

        print("games:")
        found = games.installed_games()
        names = {g.name for g in found}
        check("finds all games",
              names == {"Team Fortress 2", "Dota 2", "Counter-Strike 2", "Shared Game"})
        tf2 = next(g for g in found if g.name == "Team Fortress 2")
        check("cover url uses appid", "440" in tf2.cover_url())

        print("accounts:")
        accts = accounts.list_accounts()
        check("finds 2 accounts", len(accts) == 2)
        check("reads MostRecent", switcher.current_account_name() == "alice")

        print("local game->account mapping (LastOwner, no API key):")
        accounts.userdata_owner_map(refresh=True)
        omap = accounts.local_owner_map(refresh=True)
        check("LastOwner mapped for owned games",
              omap == {440: "76561198000000001", 570: "76561198000000002",
                       730: "76561198999999999", 999: "76561198999999999"})
        check("account_for_game uses LastOwner",
              accounts.account_for_game(570) == "76561198000000002")
        check("family-shared game maps to local player via userdata",
              accounts.account_for_game(730) == "76561198000000002")
        check("ambiguous family share resolves to the account with more playtime",
              accounts.account_for_game(999) == "76561198000000002")
        accounts.set_override(570, "76561198000000001")
        check("manual override beats LastOwner",
              accounts.account_for_game(570) == "76561198000000001")

        print("switcher (loginusers MostRecent flip):")

        def picker_on():
            cfg = vdf.load(root / "config" / "config.vdf")
            return cfg["InstallConfigStore"]["Software"]["Valve"]["Steam"].get(
                "AlwaysShowUserChooser")

        switcher.switch_account("bob")
        check("bob now MostRecent", switcher.current_account_name() == "bob")
        check("backup created", (root / "config" / "loginusers.vdf.bak").exists())
        check("switch turns picker ON", picker_on() == "1")
        check("config.vdf backed up", (root / "config" / "config.vdf.bak").exists())

        # A switch must clear any stale WantsOfflineMode=1 (Steam sets it itself on
        # "Go Offline"; a leftover "1" would hang an online cold start).
        u = vdf.load(root / "config" / "loginusers.vdf")
        u["users"]["76561198000000001"]["WantsOfflineMode"] = "1"
        vdf.dump(u, root / "config" / "loginusers.vdf")
        switcher.switch_account("alice")
        users = vdf.load(root / "config" / "loginusers.vdf")["users"]
        alice = users["76561198000000001"]
        check("alice back to MostRecent", alice.get("MostRecent") == "1")
        check("alice auto-login enabled", alice.get("AllowAutoLogin") == "1")
        check("switch clears stale offline flag", alice.get("WantsOfflineMode") == "0")
        check("non-target bob auto-login disabled",
              users["76561198000000002"].get("AllowAutoLogin") == "0")
        check("switch keeps picker ON", picker_on() == "1")

        print("login-failure detection:")
        check("accountid math", switcher.accountid_from_steamid64("76561198000000001")
              == 76561198000000001 - 76561197960265728)
        ok, _ = switcher.can_autologin("alice")
        check("can_autologin true for remembered account", ok)
        ok2, why2 = switcher.can_autologin("charlie")
        check("can_autologin false for unknown account", (not ok2) and bool(why2))
        # bob has RememberPassword=1 in fixture too -> ok; flip it off and re-check
        u = vdf.load(root / "config" / "loginusers.vdf")
        u["users"]["76561198000000002"]["RememberPassword"] = "0"
        vdf.dump(u, root / "config" / "loginusers.vdf")
        ok3, why3 = switcher.can_autologin("bob")
        check("can_autologin false when Remember me off", (not ok3) and "Remember" in why3)

        print("process control + cancellation:")
        # No real Steam in the sandbox, so nothing is running -> shutdown is a no-op
        # that should report success immediately.
        check("shutdown_steam true when Steam not running", switcher.shutdown_steam())
        # wait_for_login must bail out the instant a cancel signal is set.
        t0 = time.time()
        cancelled = switcher.wait_for_login("76561198000000001", timeout=10,
                                            should_cancel=lambda: True)
        check("wait_for_login honours cancel", (not cancelled) and time.time() - t0 < 1)

        print("offline readiness signal:")
        # steam_window_present is the offline "Steam is really up" gate. No real Steam
        # here, and it's a Windows-only API, so it must return False without raising.
        check("steam_window_present false off-Windows / no Steam",
              switcher.steam_window_present() is False)

        print("offline flag method (set_offline_mode / wants_offline_mode):")
        # Clean slate: both accounts online.
        switcher.set_offline_mode("76561198000000001", False)
        switcher.set_offline_mode("76561198000000002", False)
        check("wants_offline_mode false when all online",
              switcher.wants_offline_mode() is False)
        # Set alice offline by SteamID64; bob stays online.
        check("set_offline_mode writes for a known account",
              switcher.set_offline_mode("76561198000000001", True) is True)
        u = vdf.load(root / "config" / "loginusers.vdf")["users"]
        check("WantsOfflineMode set to 1", u["76561198000000001"].get("WantsOfflineMode") == "1")
        check("SkipOfflineModeWarning set to 1",
              u["76561198000000001"].get("SkipOfflineModeWarning") == "1")
        check("wants_offline_mode true for the offline account by id",
              switcher.wants_offline_mode("76561198000000001") is True)
        check("wants_offline_mode true for the offline account by name",
              switcher.wants_offline_mode("alice") is True)
        check("wants_offline_mode false for the online account",
              switcher.wants_offline_mode("76561198000000002") is False)
        # Match by AccountName too, and clear it back.
        check("set_offline_mode matches by AccountName",
              switcher.set_offline_mode("alice", False) is True)
        check("wants_offline_mode false after clearing",
              switcher.wants_offline_mode("alice") is False)
        # Unknown account -> no write, returns False.
        check("set_offline_mode false for unknown account",
              switcher.set_offline_mode("charlie", True) is False)

        print("add-account flow (restart_to_add_account):")
        # Add-account must take Steam to its LOGIN screen, not the saved-account
        # picker: turn the picker ON, then the restart must turn it OFF (and, on
        # Windows, empty AutoLoginUser — not testable here). start_steam raises in the
        # sandbox (no real Steam exe); the config writes happen before that.
        switcher.set_account_picker(root, show=True)
        try:
            switcher.restart_to_add_account()
        except Exception:
            pass  # start_steam can't launch a real Steam in the sandbox — that's fine
        check("add-account opens the login screen (picker off)", picker_on() == "0")

        print("cover resolver (local cache + disk cache, no network):")
        from core import covers  # noqa: E402
        covers.COVER_DIR = Path(tmp) / "covers"   # don't touch the repo's data/
        # Newer per-app librarycache layout: the portrait capsule is used.
        lc = root / "appcache" / "librarycache" / "440"
        lc.mkdir(parents=True)
        (lc / "header.jpg").write_bytes(b"HEADER440")           # must be ignored
        (lc / "library_600x900.jpg").write_bytes(b"PORTRAIT440")
        check("local cache uses the portrait capsule",
              covers.cover_bytes(440, allow_network=False) == b"PORTRAIT440")
        # Steam's local art wins and is NOT pinned by the disk cache: when Steam
        # updates the local portrait (e.g. a new season), we serve the new one.
        check("local portrait is not disk-cached (stays fresh)",
              not covers._disk_path(440).exists())
        (lc / "library_600x900.jpg").write_bytes(b"NEWSEASON440")
        check("updated local portrait is picked up (not a frozen copy)",
              covers.cover_bytes(440, allow_network=False) == b"NEWSEASON440")
        # Disk cache (network results) is read when there's no local art.
        covers._save_disk(441, b"NETCACHED441")
        check("disk cache serves when no local art",
              covers.cover_bytes(441, allow_network=False) == b"NETCACHED441")
        # Older flat layout: {appid}_library_600x900.jpg.
        flat = root / "appcache" / "librarycache"
        (flat / "570_library_600x900.jpg").write_bytes(b"PORTRAIT570FLAT")
        check("old flat portrait layout resolves",
              covers.cover_bytes(570, allow_network=False) == b"PORTRAIT570FLAT")
        # REGRESSION GUARD: a per-app folder with only hashed/non-portrait files must
        # NOT be grabbed offline (that picked a low-res logo over the real portrait).
        hashed = root / "appcache" / "librarycache" / "730"
        hashed.mkdir(parents=True)
        (hashed / "deadbeefdeadbeef.jpg").write_bytes(b"LOWRES_LOGO")
        (hashed / "logo.png").write_bytes(b"LOGO")
        check("arbitrary/hashed local files are not used (Apex regression guard)",
              covers.cover_bytes(730, allow_network=False) is None)
        # No local art + no network -> None (never a silent wrong result).
        check("no local art + offline -> None",
              covers.cover_bytes(999999, allow_network=False) is None)

    print()
    if failures:
        print(f"{failures} test(s) FAILED")
        sys.exit(1)
    print("All tests passed.")


if __name__ == "__main__":
    main()
