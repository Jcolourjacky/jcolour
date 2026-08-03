#!/usr/bin/env python3
"""Moab ICC ZIP 内 *Media Settings*.pdf → 写入「美国品牌 US」表第 19 列（总墨量/TIL）。

PDF 多半无 Total Ink %；摘录介质与「Reduce Color Density」等表格原文，并明示「PDF 未给出 TIL%」。
需：pdftotext（poppler）、openpyxl。"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import tempfile
import zipfile
from collections import defaultdict
from difflib import SequenceMatcher
from pathlib import Path

import openpyxl

ROOT = Path(__file__).resolve().parents[2]
XLSX = ROOT / "docs/paper-database/Europe-US-FineArt-Papers-Database.xlsx"
MOAB_DIR = ROOT / "docs/paper-icc/Moab_Epson"
BRAND = "Moab (Legion Paper)"
SHEET_US = "美国品牌 US"
COL_TIL = 19
MARK = "【Moab Media Settings PDF摘录】"
MATCH_MIN = 0.86

LINE_RX = re.compile(r"^\s{2,}(.+?\S)\s{2,}(\S.+)$")


def norm_key(s: str) -> str:
    if not s:
        return ""
    t = str(s).replace("®", "").replace("™", "").replace("(", " ").replace(")", " ").lower()
    t = re.sub(r"[^\w\s\-]+", " ", t)
    t = re.sub(r"\s+", " ", t).strip()
    if "somerset enh velvet" in t and "enhanced" not in t:
        t = t.replace("somerset enh velvet", "somerset enhanced velvet")
    return t


def fp_variants(paper: str) -> set[str]:
    nk = norm_key(paper)
    vs = {nk}
    if "somerset enhanced velvet" in nk:
        vs.add(nk.replace("somerset enhanced velvet", "somerset enh velvet"))
    slim = re.sub(r"\s+\d{2,4}(?:\s*/\s*\d{2,4})?(?:\s*/\s*\d{2,4})?\s*$", "", nk).strip()
    if slim:
        vs.add(slim)
    slim2 = re.sub(r"\s+\d{2,4}\s*$", "", slim).strip()
    if slim2:
        vs.add(slim2)
    return {x for x in vs if x}


def match_score(row_paper: str, left_cell: str) -> float:
    best = 0.0
    for a in fp_variants(row_paper):
        for b in fp_variants(left_cell):
            if not a or not b:
                continue
            best = max(best, 1.0 if a == b else SequenceMatcher(None, a, b).ratio())
    return best


def pdf_layout_text(td: str, data: bytes) -> str:
    fd, tmp = tempfile.mkstemp(suffix=".pdf")
    os.close(fd)
    try:
        Path(tmp).write_bytes(data)
        r = subprocess.run(
            [td, "-layout", "-enc", "UTF-8", tmp, "-"],
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


def parse_moab_rows(text: str) -> list[tuple[str, str, str]]:
    printer = ""
    out: list[tuple[str, str, str]] = []
    for raw in text.splitlines():
        line = raw.rstrip()
        pm = re.search(r"Printer:\s*(.+)", line, flags=re.I)
        if pm:
            printer = pm.group(1).strip()
            continue
        m = LINE_RX.match(line)
        if not m:
            continue
        left, right = m.group(1).strip(), m.group(2).strip()
        low = left.lower()
        if low in {"moab paper", "media type"}:
            continue
        if len(left) < 5:
            continue
        if right.lower().startswith("custom media file"):
            continue
        out.append((printer or "（Printer 未写）", left, right))
    return out


def format_line(printer: str, media_right: str, zip_name: str) -> str:
    mr = re.sub(r"\s+", " ", media_right).strip()
    if len(mr) > 200:
        mr = mr[:197] + "…"
    if re.search(r"reduce\s+color\s+density", mr, flags=re.I):
        return (
            f"{printer}（{zip_name}）：原厂 PDF 未给出 Total Ink/TIL%。" f"表格含色密度相关：{mr}"
        )
    return f"{printer}（{zip_name}）：原厂 PDF 未给出 Total Ink/TIL%。介质/驱动建议：{mr}"


def purge(cell: object) -> str:
    if cell is None:
        return ""
    s = str(cell)
    i = s.find(MARK)
    if i >= 0:
        return s[:i].rstrip()
    return s


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    td = shutil.which("pdftotext")
    if not td:
        raise SystemExit("缺少 pdftotext（brew install poppler）")
    if not MOAB_DIR.is_dir():
        raise SystemExit(f"缺少 {MOAB_DIR}")

    wb = openpyxl.load_workbook(XLSX)
    ws = wb[SHEET_US]
    rows_meta: list[tuple[int, str]] = []
    for r in range(2, ws.max_row + 1):
        if ws.cell(r, 3).value != BRAND:
            continue
        pn = ws.cell(r, 5).value
        if not pn:
            continue
        rows_meta.append((r, str(pn)))

    def match_row(left: str) -> tuple[int, str] | None:
        best_r, best_pn, best_sc = None, None, 0.0
        for r, pn in rows_meta:
            sc = match_score(pn, left)
            if sc > best_sc:
                best_sc, best_r, best_pn = sc, r, pn
        if best_r is not None and best_sc >= MATCH_MIN:
            return best_r, str(best_pn)
        return None

    buckets: dict[int, list[str]] = defaultdict(list)

    for zp in sorted(MOAB_DIR.glob("*.zip")):
        try:
            with zipfile.ZipFile(zp) as zf:
                for name in zf.namelist():
                    if "__MACOSX" in name or name.endswith("/"):
                        continue
                    if not name.lower().endswith(".pdf"):
                        continue
                    leaf = Path(name).name
                    if "media settings" not in leaf.lower():
                        continue
                    txt = pdf_layout_text(td, zf.read(name))
                    for printer, left, right in parse_moab_rows(txt):
                        hit = match_row(left)
                        if not hit:
                            continue
                        r, pn = hit
                        buckets[r].append(format_line(printer, right, zp.name))
                        if args.dry_run:
                            print(f"row{r} {pn} <= {left} | {right[:70]}")
        except (zipfile.BadZipFile, OSError) as e:
            print("skip", zp, e)

    if args.dry_run:

        print(f"dry-run：将更新第 {COL_TIL} 列的行数 ≈ {len(buckets)}（未改写 xlsx）")

        return

    for r, parts in sorted(buckets.items()):
        block = MARK + "\n" + "\n".join(f"- {p}" for p in sorted(set(parts)))
        old = ws.cell(r, COL_TIL).value
        base = purge(old)
        ws.cell(r, COL_TIL).value = ((base + "\n\n").strip() + "\n\n" + block).strip() if base else block

    wb.save(XLSX)
    print(f"已写入 {XLSX}：{len(buckets)} 行（第 {COL_TIL} 列）")


if __name__ == "__main__":
    main()
