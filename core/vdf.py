"""Minimal parser for Valve's KeyValues text format (.vdf / .acf).

We roll our own instead of depending on the `vdf` package so the whole project
stays install-free (stdlib only). The format is simple:

    "key"
    {
        "subkey"   "value"
        "nested"
        {
            "a"  "b"
        }
    }

We handle quoted tokens, escape sequences (\\\\, \\", \\n, \\t) and `//` line
comments. That covers loginusers.vdf, libraryfolders.vdf and appmanifest_*.acf.
"""

from __future__ import annotations

from typing import Any


_ESCAPES = {"n": "\n", "t": "\t", '"': '"', "\\": "\\"}


def _tokenize(text: str):
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        # whitespace
        if c in " \t\r\n":
            i += 1
            continue
        # line comment
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        # braces
        if c in "{}":
            yield c
            i += 1
            continue
        # quoted string
        if c == '"':
            i += 1
            buf = []
            while i < n:
                ch = text[i]
                if ch == "\\" and i + 1 < n:
                    buf.append(_ESCAPES.get(text[i + 1], text[i + 1]))
                    i += 2
                    continue
                if ch == '"':
                    i += 1
                    break
                buf.append(ch)
                i += 1
            yield ("str", "".join(buf))
            continue
        # bare token (unquoted) — read until whitespace or brace
        buf = []
        while i < n and text[i] not in ' \t\r\n{}"':
            buf.append(text[i])
            i += 1
        yield ("str", "".join(buf))


def loads(text: str) -> dict[str, Any]:
    """Parse VDF text into a nested dict. Top level may have one or more keys."""
    tokens = list(_tokenize(text))
    pos = 0

    def parse_block() -> dict[str, Any]:
        nonlocal pos
        obj: dict[str, Any] = {}
        while pos < len(tokens):
            tok = tokens[pos]
            if tok == "}":
                pos += 1
                break
            # expect a key (string)
            if not (isinstance(tok, tuple) and tok[0] == "str"):
                pos += 1
                continue
            key = tok[1]
            pos += 1
            if pos >= len(tokens):
                obj[key] = ""
                break
            nxt = tokens[pos]
            if nxt == "{":
                pos += 1
                obj[key] = parse_block()
            elif isinstance(nxt, tuple) and nxt[0] == "str":
                obj[key] = nxt[1]
                pos += 1
            else:
                obj[key] = ""
        return obj

    return parse_block()


def load(path) -> dict[str, Any]:
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return loads(fh.read())


def _escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def dumps(obj: dict[str, Any], indent: int = 0) -> str:
    """Serialize back to VDF text (tabs for indentation, Valve-style)."""
    pad = "\t" * indent
    out = []
    for key, val in obj.items():
        if isinstance(val, dict):
            out.append(f'{pad}"{_escape(key)}"')
            out.append(f"{pad}{{")
            out.append(dumps(val, indent + 1))
            out.append(f"{pad}}}")
        else:
            out.append(f'{pad}"{_escape(key)}"\t\t"{_escape(str(val))}"')
    return "\n".join(out)


def dump(obj: dict[str, Any], path) -> None:
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(dumps(obj))
        fh.write("\n")
