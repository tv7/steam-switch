"""Read-only diagnostic: capture what Steam's on-disk + registry state changes
when you use the *working* manual "Steam menu -> Go Offline".

Why this exists: every external offline *flag* we set (WantsOfflineMode, a
read-only loginusers.vdf, the -offline switch) gets overridden -- when Steam can
reach Valve it authenticates and comes up online anyway. The menu click is the
only thing that reliably works, because it sets Steam's own *internal* offline
state. This probe measures exactly which keys/values that click flips, on *this*
client version, so we can stop guessing forum flags and instead either
  (a) replicate + lock precisely that state (and skip clicking entirely), or
  (b) at minimum learn a dependable signal to detect "offline" before launching.

It NEVER writes to Steam. It only *reads* config files and the registry; the
only thing it writes is its own snapshots + the diff, into an output folder.

Usage:
    python -m core.offline_diff                  # guided before/after run + diff
    python -m core.offline_diff snapshot LABEL   # capture one snapshot to a file
    python -m core.offline_diff diff A B         # diff two previously saved snapshots
    python -m core.offline_diff --out DIR ...    # choose the output folder

Guided run: have Steam logged in ONLINE -> Enter (captures "before") -> do your
normal Steam menu -> Go Offline and wait for Steam to come back up offline ->
Enter (captures "after"). The diff is printed and saved to the output folder.

Pure standard library (json, difflib, winreg on Windows) -- no pip dependency.
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path

from . import steam_paths, vdf

# Value-name fragments that most likely carry the real offline state, so we can
# surface them above the noise (localconfig.vdf in particular churns timestamps).
_OFFLINE_HINTS = ("offline", "online", "connect", "reconnect")

# Sources shown first in the diff -- the prime suspects for the offline flag.
_SOURCE_ORDER = ("[registry]", "config.vdf", "loginusers.vdf", "steam.cfg")


# ---------------------------------------------------------------------------
# What we snapshot
# ---------------------------------------------------------------------------

def _tracked_files(root: Path) -> list[tuple[str, Path]]:
    """(label, path) for every file whose offline state we want to watch."""
    out = [
        ("config.vdf", root / "config" / "config.vdf"),
        ("loginusers.vdf", root / "config" / "loginusers.vdf"),
        ("steam.cfg", root / "steam.cfg"),
    ]
    udir = root / "userdata"
    if udir.exists():
        for sub in sorted(udir.glob("*/config/localconfig.vdf")):
            out.append((f"localconfig[{sub.parent.parent.name}].vdf", sub))
    # On Linux/macOS the "registry" is a plain VDF file rather than winreg.
    if not sys.platform.startswith("win"):
        for cand in (Path.home() / ".steam" / "registry.vdf",
                     Path.home() / ".steam" / "steam" / "registry.vdf"):
            if cand.exists():
                out.append(("registry.vdf", cand))
                break
    return out


def _flatten(prefix: str, node, out: dict) -> None:
    """Flatten a parsed-VDF nested dict to {a/b/c: leaf}."""
    if isinstance(node, dict):
        for k, v in node.items():
            _flatten(f"{prefix}/{k}" if prefix else str(k), v, out)
    else:
        out[prefix] = "" if node is None else str(node)


def _parse_cfg(text: str) -> dict:
    """steam.cfg is plain `Key=Value` lines, not VDF."""
    out = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("//") or "=" not in line:
            continue
        key, _, val = line.partition("=")
        out[key.strip()] = val.strip()
    return out


def _ingest_file(label: str, path: Path, flat: dict) -> None:
    """Parse one tracked file into the flat namespace under its label."""
    try:
        if path.suffix.lower() == ".vdf":
            tmp: dict = {}
            _flatten("", vdf.load(path), tmp)
            for k, v in tmp.items():
                flat[f"{label}::{k}"] = v
        elif path.name.lower() == "steam.cfg":
            for k, v in _parse_cfg(path.read_text(errors="replace")).items():
                flat[f"{label}::{k}"] = v
        else:
            flat[f"{label}::<raw>"] = path.read_text(errors="replace")
    except Exception as exc:  # never let one unreadable file abort the snapshot
        flat[f"{label}::<parse-error>"] = repr(exc)


def _reg_type_name(typ: int) -> str:
    try:
        import winreg
    except ImportError:
        return str(typ)
    for name in ("REG_SZ", "REG_EXPAND_SZ", "REG_BINARY", "REG_DWORD",
                 "REG_QWORD", "REG_MULTI_SZ", "REG_NONE"):
        if getattr(winreg, name, object()) == typ:
            return name[4:]  # drop "REG_"
    return str(typ)


def _fmt_regval(typ: int, data) -> str:
    if isinstance(data, bytes):
        body = data.hex()
    elif isinstance(data, (list, tuple)):
        body = "|".join(str(x) for x in data)
    else:
        body = str(data)
    return f"{_reg_type_name(typ)}:{body}"


def _registry_flat() -> dict:
    """Flat {subkeypath\\valuename: 'TYPE:value'} for HKCU\\Software\\Valve\\Steam.

    Windows only; read-only (KEY_READ). Empty dict elsewhere.
    """
    out: dict = {}
    try:
        import winreg
    except ImportError:
        return out
    base = r"Software\Valve\Steam"

    def walk(full: str) -> None:
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, full, 0,
                                winreg.KEY_READ) as key:
                nsub, nval, _ = winreg.QueryInfoKey(key)
                rel = full[len(base):].lstrip("\\")
                for i in range(nval):
                    name, data, typ = winreg.EnumValue(key, i)
                    name = name or "(Default)"
                    out[f"{rel}\\{name}" if rel else name] = _fmt_regval(typ, data)
                subnames = [winreg.EnumKey(key, i) for i in range(nsub)]
        except OSError:
            return
        for s in subnames:
            walk(f"{full}\\{s}")

    walk(base)
    return out


def capture(root: Path | None = None) -> dict:
    """A read-only snapshot of Steam's offline-relevant state.

    {"files": {label: meta}, "flat": {source::key: value}}. `flat` merges every
    tracked VDF/cfg file and (on Windows) the Steam registry subtree into one
    namespace, which is what we diff.
    """
    root = root or steam_paths.steam_root()
    snap: dict = {"platform": sys.platform, "root": str(root) if root else None,
                  "files": {}, "flat": {}}
    if not root:
        return snap
    for label, path in _tracked_files(root):
        meta = {"exists": path.exists()}
        if path.exists():
            try:
                st = path.stat()
                meta.update(size=st.st_size, mtime=st.st_mtime,
                            readonly=not os.access(path, os.W_OK))
            except OSError:
                pass
            _ingest_file(label, path, snap["flat"])
        snap["files"][label] = meta
    for k, v in _registry_flat().items():
        snap["flat"][f"[registry]::{k}"] = v
    return snap


# ---------------------------------------------------------------------------
# Diffing
# ---------------------------------------------------------------------------

def _trim(s: str, n: int = 140) -> str:
    s = " ".join(str(s).split())
    return s if len(s) <= n else s[: n - 1] + "…"


def _is_hot(key: str) -> bool:
    return any(h in key.lower() for h in _OFFLINE_HINTS)


def _source_of(key: str) -> str:
    return key.split("::", 1)[0]


def _source_sort_key(src: str):
    return (_SOURCE_ORDER.index(src) if src in _SOURCE_ORDER
            else len(_SOURCE_ORDER), src)


def diff_snapshots(before: dict, after: dict) -> list[str]:
    """Return the human-readable diff as a list of lines (printed + saved)."""
    fb, fa = before.get("flat", {}), after.get("flat", {})
    changed, added, removed = [], [], []
    for k in set(fb) | set(fa):
        if k in fb and k in fa:
            if fb[k] != fa[k]:
                changed.append(k)
        elif k in fa:
            added.append(k)
        else:
            removed.append(k)

    lines: list[str] = []
    lines.append("=" * 70)
    lines.append(f"OFFLINE STATE DIFF  (online -> offline)")
    lines.append(f"  before: {len(fb)} keys   after: {len(fa)} keys")
    lines.append(f"  changed: {len(changed)}   added: {len(added)}   "
                 f"removed: {len(removed)}")
    lines.append("=" * 70)

    # 1) The signal: anything whose name looks offline-related, regardless of file.
    hot = sorted(k for k in (changed + added + removed) if _is_hot(k))
    lines.append("")
    lines.append(f"### LIKELY OFFLINE FLAG(S) -- {len(hot)} match "
                 f"{'/'.join(_OFFLINE_HINTS)} ###")
    if not hot:
        lines.append("  (none -- the offline state may be a non-obvious key; "
                     "scan the per-source changes below)")
    for k in hot:
        if k in changed:
            lines.append(f"  ~ {k}")
            lines.append(f"        {_trim(fb[k])}  ->  {_trim(fa[k])}")
        elif k in added:
            lines.append(f"  + {k}  =  {_trim(fa[k])}")
        else:
            lines.append(f"  - {k}  (was {_trim(fb[k])})")

    # 2) Everything else, grouped by source file (prime suspects first).
    by_source: dict[str, dict[str, list]] = {}
    for kind, keys in (("~", changed), ("+", added), ("-", removed)):
        for k in keys:
            by_source.setdefault(_source_of(k), {"~": [], "+": [], "-": []})[kind].append(k)

    lines.append("")
    lines.append("### ALL VALUE CHANGES, BY SOURCE ###")
    for src in sorted(by_source, key=_source_sort_key):
        groups = by_source[src]
        total = sum(len(v) for v in groups.values())
        note = "  <- churns timestamps; usually noise" if "localconfig" in src else ""
        lines.append("")
        lines.append(f"-- {src}  ({total} change{'s' if total != 1 else ''}){note}")
        for k in sorted(groups["~"]):
            short = k.split("::", 1)[1] if "::" in k else k
            lines.append(f"   ~ {short}:  {_trim(fb[k])}  ->  {_trim(fa[k])}")
        for k in sorted(groups["+"]):
            short = k.split("::", 1)[1] if "::" in k else k
            lines.append(f"   + {short}  =  {_trim(fa[k])}")
        for k in sorted(groups["-"]):
            short = k.split("::", 1)[1] if "::" in k else k
            lines.append(f"   - {short}  (was {_trim(fb[k])})")

    # 3) File-level metadata (read-only bit flips, file appearing/vanishing).
    meta_lines = []
    fbm, fam = before.get("files", {}), after.get("files", {})
    for label in sorted(set(fbm) | set(fam)):
        b, a = fbm.get(label, {}), fam.get(label, {})
        notes = []
        if b.get("exists") != a.get("exists"):
            notes.append(f"exists {b.get('exists')} -> {a.get('exists')}")
        if b.get("readonly") != a.get("readonly"):
            notes.append(f"read-only {b.get('readonly')} -> {a.get('readonly')}")
        if b.get("size") != a.get("size"):
            notes.append(f"size {b.get('size')} -> {a.get('size')}")
        if notes:
            meta_lines.append(f"   {label}: {', '.join(notes)}")
    lines.append("")
    lines.append("### FILE METADATA CHANGES (existence / read-only / size) ###")
    lines.extend(meta_lines or ["   (none)"])
    return lines


# ---------------------------------------------------------------------------
# Save / load / state hint
# ---------------------------------------------------------------------------

def _save(snap: dict, path: Path) -> None:
    path.write_text(json.dumps(snap, indent=2, sort_keys=True))


def _load(path: Path) -> dict:
    return json.loads(path.read_text())


def _state_hint() -> str:
    """A read-only one-liner so you can confirm online vs offline at capture time."""
    try:
        from . import switcher
        au = switcher.active_user_accountid()
        ro = switcher.is_loginusers_readonly()
        return (f"ActiveUser={au} (0 = logged out / offline login screen), "
                f"loginusers.vdf read-only={ro}")
    except Exception as exc:
        return f"(state hint unavailable: {exc})"


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def _guided(out_dir: Path) -> int:
    root = steam_paths.steam_root()
    print(f"Steam root: {root}")
    if not root:
        print("Steam install not found -- set STEAM_ROOT or run on the Steam PC.")
        return 1
    out_dir.mkdir(parents=True, exist_ok=True)

    print("\nStep 1/2 -- BEFORE (online).")
    print(f"  state: {_state_hint()}")
    input("  Make sure Steam is logged in ONLINE, then press Enter to capture... ")
    before = capture(root)
    _save(before, out_dir / "before.json")
    print(f"  captured {len(before['flat'])} keys -> {out_dir / 'before.json'}")

    print("\nStep 2/2 -- AFTER (offline).")
    print("  Now do it BY HAND: Steam menu -> Go Offline, and WAIT until Steam has")
    print("  restarted and is showing offline. (Don't launch a game yet.)")
    input("  Then press Enter to capture... ")
    after = capture(root)
    _save(after, out_dir / "after.json")
    print(f"  captured {len(after['flat'])} keys -> {out_dir / 'after.json'}")
    print(f"  state: {_state_hint()}")

    lines = diff_snapshots(before, after)
    text = "\n".join(lines)
    print("\n" + text)
    (out_dir / "diff.txt").write_text(text)
    print(f"\nSaved: {out_dir / 'before.json'}, {out_dir / 'after.json'}, "
          f"{out_dir / 'diff.txt'}")
    print("Send me diff.txt (or paste the output above) and we'll design the fix.")
    return 0


def main(argv=None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    out_dir = Path.cwd() / "offline_diff"
    if "--out" in argv:
        i = argv.index("--out")
        try:
            out_dir = Path(argv[i + 1])
            del argv[i:i + 2]
        except IndexError:
            print("--out needs a directory")
            return 2

    if argv and argv[0] == "snapshot":
        label = argv[1] if len(argv) > 1 else "snapshot"
        out_dir.mkdir(parents=True, exist_ok=True)
        snap = capture()
        dest = out_dir / f"{label}.json"
        _save(snap, dest)
        print(f"state: {_state_hint()}")
        print(f"captured {len(snap['flat'])} keys -> {dest}")
        return 0

    if argv and argv[0] == "diff":
        if len(argv) < 3:
            print("usage: diff <labelA> <labelB>  (looks in the --out folder)")
            return 2

        def _resolve(name: str) -> Path:
            p = Path(name)
            if p.exists():
                return p
            return out_dir / (name if name.endswith(".json") else f"{name}.json")

        a, b = _resolve(argv[1]), _resolve(argv[2])
        if not a.exists() or not b.exists():
            print(f"snapshot not found: {a if not a.exists() else b}")
            return 1
        print("\n".join(diff_snapshots(_load(a), _load(b))))
        return 0

    return _guided(out_dir)


if __name__ == "__main__":
    raise SystemExit(main())
