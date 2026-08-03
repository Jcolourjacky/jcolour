#!/usr/bin/env python3
"""遍历 docs/paper-icc 内 PDF（含 ZIP、Hahn 伪装 .icc 包），抽取总墨量/TIL/限墨及邻近运维措辞草稿。

须人工对照 PDF 核对后再写入表。依赖：`pdftotext`（brew install poppler）。"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import tempfile
import zipfile
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterator

ROOT = Path(__file__).resolve().parents[2]
PAPER_ICC = ROOT / "docs/paper-icc"

_PATTERN_TIL = [
    r"total\s+ink(?:\s+limit)?",
    r"global\s+ink\s+limit",
    r"\bink\s+limit\b",
    r"maximum\s+(?:overall\s+)?ink",
    r"overall\s+ink\s+coverage",
    r"墨水\s*[限制量]",
    r"墨量\s*[限制上]?",
    r"总\s*墨\s*量",
    r"限\s*墨",
    r"\bTIL\b",
    r"total\s*(?:overall\s*)?coverage",
]
_PATTERN_DENSITY = [
    r"reduce\s+color\s+density",
    r"increase\s+color\s+density",
    r"colour\s*density[^\n.%]{0,24}-?\s*\d+\s*%",
    r"color\s*density[^\n.%]{0,24}-?\s*\d+\s*%",
]
_PATTERN_NEAR = [
    r"drying\s*time(?:\s*per\s*pass)?",
    r"干燥",
    r"platen\s*gap",
    r"纸张?\s*(?:厚度|间隙)",
    r"paper\s+thickness",
    r"color\s*density",
    r"色\s*密度",
    r"Quality:\s*Level\s*\d+",
]


def _rx(parts: list[str]) -> re.Pattern[str]:
    return re.compile("|".join(f"(?:{p})" for p in parts), flags=re.I)


RX_TIL = _rx(_PATTERN_TIL)
RX_NEAR = _rx(_PATTERN_NEAR)
RX_DEN = _rx(_PATTERN_DENSITY)


@dataclass
class PdfHit:
    source: str
    language_guess: str
    kind: str
    matched: str
    snippet: str


def pdf_bytes_to_text(pdftotext: str, data: bytes) -> str:
    fd, tmp = tempfile.mkstemp(suffix=".pdf")
    os.close(fd)
    try:
        Path(tmp).write_bytes(data)
        r = subprocess.run(
            [pdftotext, "-layout", "-enc", "UTF-8", tmp, "-"],
            capture_output=True,
            text=True,
            timeout=180,
            check=False,
        )
        return (r.stdout or "").replace("\x00", "")
    finally:
        try:
            os.unlink(tmp)
        except OSError:
            pass


def is_zip_magic(b: bytes) -> bool:
    return len(b) >= 4 and b.startswith(b"PK\x03\x04")


def guess_lang(leaf: str) -> str:
    u = leaf.upper()
    if "_EN." in u:
        return "en"
    if "_DE." in u:
        return "de"
    if "_FR." in u:
        return "fr"
    if "_ES." in u:
        return "es"
    return "unknown"


def collapse(s: str) -> str:
    return re.sub(r"\s+", " ", s).strip()


def snippets(flat: str, rx: re.Pattern[str]) -> list[tuple[str, str]]:
    out: list[tuple[str, str]] = []
    seen: set[str] = set()
    for m in rx.finditer(flat):
        matched = (m.group(0) or "")[:120]
        a, b = max(0, m.start() - 140), min(len(flat), m.end() + 220)
        sn = flat[a:b]
        if len(sn) < 20:
            continue
        key = sn[:260]
        if key in seen:
            continue
        seen.add(key)
        out.append((matched, sn))
    return out


def iter_pdf_blobs(base: Path) -> Iterator[tuple[str, bytes]]:
    for p in sorted(base.rglob("*.pdf")):
        if "__MACOSX" in str(p):
            continue
        yield str(p.relative_to(base)), p.read_bytes()
    for zp in sorted(base.rglob("*.zip")):
        rel = str(zp.relative_to(base))
        try:
            with zipfile.ZipFile(zp) as zf:
                for name in zf.namelist():
                    if "__MACOSX" in name or name.endswith("/"):
                        continue
                    if not name.lower().endswith(".pdf"):
                        continue
                    yield f"{rel}!/{name}", zf.read(name)
        except (zipfile.BadZipFile, OSError):
            continue
    for icc in sorted(base.rglob("*.icc")):
        blob = icc.read_bytes()
        if not is_zip_magic(blob):
            continue
        outer = str(icc.relative_to(base))
        try:
            with zipfile.ZipFile(icc) as zf:
                for name in zf.namelist():
                    if "__MACOSX" in name or name.endswith("/"):
                        continue
                    if name.lower().endswith(".pdf"):
                        yield f"{outer}!/{name}", zf.read(name)
        except (zipfile.BadZipFile, OSError):
            continue


def harvest(pdftotext: str, base: Path) -> list[PdfHit]:
    hits: list[PdfHit] = []
    MAX_NEAR = 6
    for source, blob in iter_pdf_blobs(base):
        leaf = source.split("/")[-1]

        lg = guess_lang(leaf)
        text = pdf_bytes_to_text(pdftotext, blob)
        flat = collapse(text)

        if len(flat) < 30:
            continue
        for matched, sn in snippets(flat, RX_TIL):
            hits.append(
                PdfHit(
                    source=source,
                    language_guess=lg,
                    kind="til_related",
                    matched=matched,
                    snippet=sn,
                )
            )
        for matched, sn in snippets(flat, RX_DEN):
            hits.append(
                PdfHit(
                    source=source,
                    language_guess=lg,
                    kind="color_density_adj",
                    matched=matched,
                    snippet=sn,
                )
            )
        cnt = 0
        for matched, sn in snippets(flat, RX_NEAR):
            if cnt >= MAX_NEAR:
                break
            hits.append(
                PdfHit(
                    source=source,
                    language_guess=lg,
                    kind="adjacent_ops",
                    matched=matched,
                    snippet=sn,
                )
            )
            cnt += 1


    return hits


def write_md(path: Path, hits: list[PdfHit]) -> None:
    lines = [
        "# PDF 与总墨量 / TIL / 限墨及邻近运维 — 草稿",
        "",
        "机器抽取；**写入表前请对照 PDF 原文与页码核对。**",
        "",
    ]
    by: dict[str, list[PdfHit]] = {}
    for h in hits:
        by.setdefault(h.source, []).append(h)
    for src in sorted(by):
        lines.append(f"## `{src}`")
        lines.append("")
        til = [h for h in by[src] if h.kind == "til_related"]
        den = [h for h in by[src] if h.kind == "color_density_adj"]
        near = [h for h in by[src] if h.kind not in {"til_related", "color_density_adj"}]
        if not til and not den:
            lines.append("（未命中 Total Ink/TIL 及「色密度微调」措辞；若为扫描 PDF 或未含用语则正常）")
            lines.append("")
        for h in til:
            lines.append(f"- **til_related（总墨量/TIL 线索）** `{h.matched}` · lang≈{h.language_guess}")
            lines.append(f"  > {h.snippet}")
            lines.append("")
        for h in den:
            lines.append(f"- **color_density_adj（色密度±%，非必然等于 TIL）** `{h.matched}` · lang≈{h.language_guess}")
            lines.append(f"  > {h.snippet}")
            lines.append("")
        for h in near:
            lines.append(f"- **adjacent_ops** `{h.matched}`")
            lines.append(f"  > {h.snippet}")
            lines.append("")
        lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-md", type=Path, default=PAPER_ICC / "TIL_PDF_SNIPPETS_DRAFT.md")
    ap.add_argument("--out-json", type=Path, default=None)
    args = ap.parse_args()
    wt = shutil.which("pdftotext")
    if not wt:
        raise SystemExit("未找到 pdftotext（macOS：`brew install poppler`）")
    if not PAPER_ICC.is_dir():
        raise SystemExit(f"缺少 {PAPER_ICC}")
    hs = harvest(wt, PAPER_ICC)
    til_n = sum(1 for x in hs if x.kind == "til_related")
    den_n = sum(1 for x in hs if x.kind == "color_density_adj")
    args.out_md.parent.mkdir(parents=True, exist_ok=True)
    write_md(args.out_md, hs)
    print(
        f"PDF 源 {len({h.source for h in hs})} 个；命中 {len(hs)} "
        f"（til_related {til_n} · color_density_adj {den_n}）"
    )
    print(args.out_md)
    if args.out_json:
        args.out_json.write_text(json.dumps([asdict(h) for h in hs], ensure_ascii=False, indent=2), encoding="utf-8")
        print(args.out_json)


if __name__ == "__main__":
    main()
