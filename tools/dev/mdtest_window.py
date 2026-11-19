"""The cartridge window's contents, written independently of common/mdtest.c.

The host tests check the C code against this module, and `swd.py window
--pattern` checks a device's window with it. Offsets are read from
common/mdtest.h, the window's one source of truth, never copied here.
"""
import os
import re

HEADER_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                           "common", "mdtest.h")


def layout() -> dict:
    """Every integer #define of common/mdtest.h (MDTEST_ prefix dropped)."""
    raw = {}
    for line in open(HEADER_FILE):
        m = re.match(r"#define\s+MDTEST_(\w+)\s+(.+?)\s*(//.*)?$", line)
        if m:
            raw[m.group(1)] = m.group(2)
    out = {}
    for name, expr in raw.items():
        text = re.sub(r"(\d)U\b", r"\1", expr.replace("ul", ""))
        text = re.sub(r"MDTEST_(\w+)", lambda m: str(out.get(m.group(1), m.group(0))), text)
        try:
            out[name] = int(eval(text, {"__builtins__": {}}))
        except Exception:
            if text.startswith('"'):
                out[name] = text.strip('"')
    return out


def pattern_word(index: int) -> int:
    """The test pattern: a bijection of the 16-bit window index."""
    x = index & 0xFFFF
    x ^= x >> 8
    x = (x * 0x6B4D) & 0xFFFF
    x ^= x >> 7
    x = (x * 0x9E37) & 0xFFFF
    x ^= x >> 8
    return x


def text_words(text: str, count: int) -> list:
    data = text.encode("ascii")[: count * 2].ljust(count * 2, b"\0")
    return [(data[2 * i] << 8) | data[2 * i + 1] for i in range(count)]


def header_words(version: str, build: str, state: int = 0) -> list:
    L = layout()
    h = [0] * L["HEADER_WORDS"]
    h[L["HDR_ID"]:L["HDR_ID"] + 4] = text_words(L["ID"], 4)
    h[L["HDR_LAYOUT"]] = L["LAYOUT_VERSION"]
    h[L["HDR_STATE"]] = state
    v, n = L["HDR_VERSION"], L["HDR_VERSION_WORDS"]
    h[v:v + n] = text_words(version, n)
    b, n = L["HDR_BUILD"], L["HDR_BUILD_WORDS"]
    h[b:b + n] = text_words(build, n)
    h[L["HDR_CHECKSUM"]] = (-sum(h)) & 0xFFFF
    return h


def window_words(version: str, build: str, state: int = 0) -> list:
    L = layout()
    words = [pattern_word(i) for i in range(L["WINDOW_WORDS"])]
    i = L["HEADER_INDEX"]
    words[i:i + L["HEADER_WORDS"]] = header_words(version, build, state)
    return words


def read_header(words: list) -> dict:
    """The header as the ST program reads it, from the window's words."""
    L = layout()
    h = words[L["HEADER_INDEX"]:L["HEADER_INDEX"] + L["HEADER_WORDS"]]

    def text(start, count):
        return b"".join(bytes([w >> 8, w & 0xFF]) for w in h[start:start + count]
                        ).rstrip(b"\0").decode("ascii", "replace")
    return {
        "id": text(L["HDR_ID"], 4),
        "layout": h[L["HDR_LAYOUT"]],
        "state": h[L["HDR_STATE"]],
        "checksum_ok": sum(h) & 0xFFFF == 0,
        "version": text(L["HDR_VERSION"], L["HDR_VERSION_WORDS"]),
        "build": text(L["HDR_BUILD"], L["HDR_BUILD_WORDS"]),
    }
