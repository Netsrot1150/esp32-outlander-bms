"""Minimal parser för KiCads s-uttryck (symbolbibliotek, netlistor, kort)."""
from __future__ import annotations
import re

_TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))')


def parse(text: str):
    stack, cur = [], []
    pos = 0
    n = len(text)
    while pos < n:
        m = _TOKEN.match(text, pos)
        if not m:
            break
        pos = m.end()
        if m.group(1):
            stack.append(cur)
            cur = []
        elif m.group(2):
            done = cur
            cur = stack.pop()
            cur.append(done)
        elif m.group(3) is not None:
            cur.append(m.group(3).replace('\\"', '"'))
        elif m.group(4) is not None:
            cur.append(m.group(4))
    return cur[0] if len(cur) == 1 else cur


def find_all(node, head: str):
    """Alla underlistor (rekursivt) vars första element är head."""
    out = []
    if isinstance(node, list):
        if node and node[0] == head:
            out.append(node)
        for ch in node:
            if isinstance(ch, list):
                out.extend(find_all(ch, head))
    return out


def child(node, head: str):
    for ch in node:
        if isinstance(ch, list) and ch and ch[0] == head:
            return ch
    return None
