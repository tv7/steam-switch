"""Orchestrate the full 'click a game -> play it' flow.

    1. Figure out which account owns the game.
    2. Online launch: switch to that account, restart Steam, log in via the
       picker, then launch.
    3. Offline launch (the shared-account use case): bring Steam up ONLINE as the
       owning account (normal picker login — that mints the cached session offline
       mode needs), set WantsOfflineMode=1 while Steam is running, then restart
       Steam so it comes up OFFLINE on the cached session, and only THEN launch the
       game. So the game session runs offline (cloud saves aren't synced over the
       shared account's), while the brief online login matches the user's own
       manual method. We confirm Steam stayed offline before launching, so the
       shared account is never used online. (Flag method — resolution-independent,
       no menu clicking.)
    4. Launch the game via the steam:// protocol.
"""

from __future__ import annotations

import subprocess
import sys
import time
from dataclasses import dataclass

from . import accounts, switcher


@dataclass
class PlayResult:
    ok: bool
    message: str
    switched: bool = False
    needs_login: bool = False   # user must finish login + Steam Guard manually


def _launch_appid(appid: int) -> None:
    uri = f"steam://rungameid/{appid}"
    if sys.platform.startswith("win"):
        import os
        os.startfile(uri)  # type: ignore[attr-defined]
    elif sys.platform == "darwin":
        subprocess.Popen(["open", uri])
    else:
        subprocess.Popen(["xdg-open", uri])


def play(appid: int, *, offline: bool = False, login_wait: float = 120.0,
         notify=None, should_cancel=None) -> PlayResult:
    """Switch to the owning account and launch the game.

    `notify(msg)` (optional) is called with progress text while we wait.
    `should_cancel()` (optional) is polled at each step; if it returns True we
    stop and do NOT launch the game — used so the user can abort after picking
    the wrong game.
    """
    def _say(msg: str) -> None:
        if notify:
            notify(msg)

    def _cancelled() -> bool:
        return bool(should_cancel and should_cancel())

    target_sid = accounts.account_for_game(appid)
    if not target_sid:
        return PlayResult(
            False,
            "No account is mapped to this game yet. Its appmanifest records no "
            "owner — pin the game to an account manually (or add an API key and "
            "refresh owned games as a fallback).",
        )

    account = next(
        (a for a in accounts.list_accounts() if a.steamid64 == target_sid), None
    )
    if not account:
        return PlayResult(
            False, f"Account {target_sid} owns this game but isn't logged in on this PC."
        )

    if offline:
        return _play_offline(appid, account, login_wait=login_wait, say=_say,
                             cancelled=_cancelled, should_cancel=should_cancel)

    # ----- Online launch -----------------------------------------------------
    # Fast path: already logged in as the right account.
    if switcher.is_logged_in_as(account.steamid64):
        _launch_appid(appid)
        return PlayResult(True, f"Launching on '{account.persona_name}'.")

    # Pre-flight: will silent login even be possible? Catch the common failure
    # before we tear down the running Steam session.
    ok, why = switcher.can_autologin(account.account_name)
    if not ok:
        return PlayResult(
            False,
            f"Can't auto-login to '{account.persona_name}': {why}. "
            "Log into that account in Steam once with “Remember me” checked, "
            "then try again.",
            needs_login=True,
        )

    if _cancelled():
        return PlayResult(False, "Launch cancelled.")

    _say("Closing Steam to switch accounts…")
    if not switcher.shutdown_steam():
        return PlayResult(
            False,
            "Couldn't close Steam to switch accounts. Close it manually "
            "(right-click the tray icon → Exit) and try again.",
        )
    if _cancelled():
        return PlayResult(False, "Launch cancelled before switching accounts.")

    switcher.switch_account(account.account_name)  # picker ON
    if _cancelled():
        return PlayResult(
            False,
            f"Launch cancelled. Steam is set to '{account.persona_name}' but "
            "wasn't started.",
            switched=True,
        )
    # start_steam starts Steam with its window visible so the account picker
    # appears — the picker click IS our login path (CLAUDE.md constraint #2).
    switcher.start_steam()
    _say(f"Steam is restarting. When the account picker appears, click "
         f"'{account.persona_name}' (no password needed) to log in.")

    logged_in = switcher.wait_for_login(account.steamid64, timeout=login_wait,
                                        should_cancel=should_cancel)
    if _cancelled():
        return PlayResult(False, "Launch cancelled.", switched=True)
    if not logged_in:
        # Maybe it logged in just after the poll window; otherwise report why.
        active = switcher.active_user_accountid()
        target_id = switcher.accountid_from_steamid64(account.steamid64)
        if active != target_id:
            if active in (None, 0):
                return PlayResult(
                    False,
                    f"Steam is still finishing its restart, or it's showing "
                    f"the account picker. Click '{account.persona_name}' "
                    "there (no password needed), then click the game again. "
                    "(Only enter a password if Steam asks — that means the "
                    "saved login expired.)",
                    switched=True, needs_login=True,
                )
            return PlayResult(
                False,
                f"Steam logged into a different account instead of "
                f"'{account.persona_name}'. Switch to it in the Steam window, "
                "then click the game again.",
                switched=True, needs_login=True,
            )

    _launch_appid(appid)
    return PlayResult(True, f"Launching on '{account.persona_name}'.", switched=True)


def _play_offline(appid, account, *, login_wait, say, cancelled, should_cancel):
    """Bring Steam up online as `account`, then restart it offline via the flag.

    The flag method (validated manually, resolution-independent, no clicking): get
    Steam logged in ONLINE as the owning account (its normal picker login — that
    mints the cached session offline mode needs), set WantsOfflineMode=1 while Steam
    is still running, then exit + restart Steam so it comes up OFFLINE on the cached
    session. We confirm the flag stuck (Steam stayed offline, didn't reset it to
    online) before launching, so the shared account is never used online. The game
    session is offline, so its cloud saves aren't synced over the shared account's;
    the brief online login is the user's own manual method.
    """
    if not sys.platform.startswith("win"):
        return PlayResult(False, "Offline mode is Windows-only for now.")

    # Fast path: already logged in as the right account.
    if switcher.is_logged_in_as(account.steamid64):
        if switcher.wants_offline_mode(account.steamid64):
            _launch_appid(appid)   # already offline as this account — just launch
            return PlayResult(
                True, f"Launching '{account.persona_name}' (already offline).",
            )
        # Online as the account: session is fresh, go straight to the offline restart.
        return _go_offline_and_launch(appid, account, login_wait=login_wait,
                                      say=say, cancelled=cancelled,
                                      should_cancel=should_cancel)

    # Otherwise bring Steam up ONLINE as the owning account first (picker login).
    ok, why = switcher.can_autologin(account.account_name)
    if not ok:
        return PlayResult(
            False,
            f"Can't start '{account.persona_name}': {why}. Log into that account "
            "once online with “Remember me” checked, then try again.",
            needs_login=True,
        )
    if cancelled():
        return PlayResult(False, "Launch cancelled.")

    say("Closing Steam to switch accounts…")
    if not switcher.shutdown_steam():
        return PlayResult(
            False,
            "Couldn't close Steam to switch accounts. Close it manually "
            "(right-click the tray icon → Exit) and try again.",
        )
    if cancelled():
        return PlayResult(False, "Launch cancelled before switching accounts.")

    switcher.switch_account(account.account_name)  # online, picker ON
    if cancelled():
        return PlayResult(
            False,
            f"Launch cancelled. Steam is set to '{account.persona_name}' but "
            "wasn't started.",
            switched=True,
        )
    switcher.start_steam()
    say(f"Steam is restarting. When the account picker appears, click "
        f"'{account.persona_name}' (no password needed) to log in.")

    logged_in = switcher.wait_for_login(account.steamid64, timeout=login_wait,
                                        should_cancel=should_cancel)
    if cancelled():
        return PlayResult(False, "Launch cancelled.", switched=True)
    if not logged_in:
        return PlayResult(
            False,
            f"Steam isn't logged in as '{account.persona_name}' yet — finish the "
            "picker login (only enter a password if Steam asks, which means the "
            "saved login expired), then click the game again.",
            switched=True, needs_login=True,
        )

    return _go_offline_and_launch(appid, account, login_wait=login_wait, say=say,
                                  cancelled=cancelled, should_cancel=should_cancel)


def _recover_offline(account) -> None:
    """Undo a failed offline attempt so the user isn't left with a wedged Steam.

    A leftover WantsOfflineMode=1 makes Steam re-read the flag and (on a bad config)
    re-hang on every launch. So on failure we force Steam down and clear the flag,
    leaving Steam ready to start NORMALLY next time.
    """
    switcher.shutdown_steam()                       # force-kills if Steam is hung
    switcher.set_offline_mode(account.steamid64, False)


def _go_offline_and_launch(appid, account, *, login_wait, say, cancelled,
                           should_cancel):
    """Steam is up + online as `account`: set the offline flag, restart, then launch.

    User-validated order: WantsOfflineMode is written while Steam is still running,
    THEN Steam is exited and restarted so it reads the flag at startup and comes up
    offline on the cached session.

    Readiness/safety: offline login does NOT reliably report via ActiveUser, so we
    do NOT gate the launch on a detected login. Instead we wait for Steam's main
    window to appear (it reaches its UI = not stuck on the connecting splash) while
    the offline flag stays "1". If Steam resets the flag (came back ONLINE), or never
    reaches its UI (hung), we recover and do NOT launch — so the shared account is
    never used online.
    """
    say(f"Setting Steam to offline mode for '{account.persona_name}'…")
    if not switcher.set_offline_mode(account.steamid64, True):
        return PlayResult(
            False,
            f"Couldn't set the offline flag for '{account.persona_name}' "
            "(account not found in loginusers.vdf).",
            switched=True,
        )
    if cancelled():
        return PlayResult(False, "Launch cancelled.", switched=True)

    say("Restarting Steam in offline mode…")
    if not switcher.shutdown_steam():
        return PlayResult(
            False,
            "Couldn't close Steam to restart it offline. Close it manually "
            "(right-click the tray icon → Exit) and try again.",
            switched=True,
        )
    if cancelled():
        return PlayResult(False, "Launch cancelled.", switched=True)
    switcher.start_steam()
    say(f"Steam is coming up offline for '{account.persona_name}'…")

    # Wait for Steam's UI to appear while it stays flagged offline.
    deadline = time.time() + login_wait
    window_up = False
    while time.time() < deadline:
        if should_cancel and should_cancel():
            return PlayResult(False, "Launch cancelled.", switched=True)
        if not switcher.wants_offline_mode(account.steamid64):
            # Steam rewrote the flag to 0 → it came back ONLINE. Steam itself resets
            # the flag, so no recovery write is needed; just don't launch.
            return PlayResult(
                False,
                f"Steam came back ONLINE for '{account.persona_name}' instead of "
                "offline. Not launching, to avoid using the shared account online. "
                "Try again.",
                switched=True,
            )
        if switcher.steam_window_present():
            window_up = True
            break
        time.sleep(1.0)

    if not window_up:
        # Steam never reached its UI within login_wait — most likely the connecting
        # splash hang. Recover so the next start is normal, and don't launch.
        _recover_offline(account)
        return PlayResult(
            False,
            f"Steam didn't come up offline for '{account.persona_name}' (it may have "
            "hung on the connecting screen). Cleared the offline flag so Steam starts "
            "normally next time. This is usually a corrupt Steam cache — clear the "
            "download cache / delete Steam's appcache folder, then try again.",
            switched=True,
        )

    # Steam's UI is up and still flagged offline. Let it settle, re-check the flag
    # once more (it must not have flipped online while loading), then launch.
    time.sleep(2.0)
    if not switcher.wants_offline_mode(account.steamid64):
        return PlayResult(
            False,
            f"Steam went online for '{account.persona_name}' during startup. Not "
            "launching, to avoid using the shared account online. Try again.",
            switched=True,
        )

    _launch_appid(appid)
    return PlayResult(
        True,
        f"Launching '{account.persona_name}' offline — Steam is offline now, so the "
        "shared session and its cloud saves aren't touched.",
        switched=True,
    )
