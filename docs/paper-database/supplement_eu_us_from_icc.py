#!/usr/bin/env python3
"""扫描 docs/paper-icc，解析 ICC desc，合并进 Europe-US 工作表「打印机」「备注」。"""
from __future__ import annotations

import re
import zipfile
from collections import defaultdict
from dataclasses import dataclass, field
from difflib import SequenceMatcher
from pathlib import Path

import openpyxl

ROOT = Path(__file__).resolve().parents[2]
XLSX = ROOT / "docs/paper-database/Europe-US-FineArt-Papers-Database.xlsx"
PAPER_ICC = ROOT / "docs/paper-icc"
MATCH_MIN = 0.86
VERIFIED = "2025-04-29"

BRAND_FOR_VENDOR = {
    "moab": "Moab (Legion Paper)",
    "red_river": "Red River Paper",
    "hahnemuehle": "Hahnemühle",
}


@dataclass
class Observation:
    vendor: str
    rel_bundle: str
    icc_leaf: str
    desc: str
    printers_hint: str = ""
    media_token: str = ""
    pk_hint: str = ""
    fingerprints: tuple[str, ...] = field(default_factory=tuple)
    match_key: str = ""


def norm_key(s: str) -> str:
    if not s:
        return ""
    t = str(s).replace("®", "").replace("™", "").replace("(", " ").replace(")", " ").lower()
    t = re.sub(r"[^\w\s\-]+", " ", t)
    t = re.sub(r"\s+", " ", t).strip()
    if "somerset enh velvet" in t and "enhanced" not in t:
        t = t.replace("somerset enh velvet", "somerset enhanced velvet")
    return t


def fp_from_paper_name(s: str) -> tuple[str, ...]:
    nk = norm_key(s)
    cand = [nk]
    if "somerset enhanced velvet" in nk:
        cand.append(nk.replace("somerset enhanced velvet", "somerset enh velvet"))
    return tuple(dict.fromkeys(x for x in cand if x))


def tag_table(data: bytes) -> dict[str, tuple[int, int]]:
    if len(data) < 132:
        return {}
    psz = int.from_bytes(data[0:4], "big")
    if psz < 132 or psz != len(data):
        return {}
    n = int.from_bytes(data[128:132], "big")
    if n > 4096:
        return {}
    pos = 132
    out: dict[str, tuple[int, int]] = {}
    for _ in range(n):
        if pos + 12 > len(data):
            break
        sig = data[pos : pos + 4].decode("ascii", errors="replace")
        o = int.from_bytes(data[pos + 4 : pos + 8], "big")
        sz = int.from_bytes(data[pos + 8 : pos + 12], "big")
        if o + sz <= len(data) and sz > 0:
            out[sig] = (o, sz)
        pos += 12
    return out


def read_desc(data: bytes) -> str:
    tags = tag_table(data)
    if "desc" not in tags:
        return ""
    o, sz = tags["desc"]
    ch = data[o : o + sz]
    if len(ch) < 12:
        return ""
    typ = ch[:4]
    if typ == b"mluc":
        nrec = int.from_bytes(ch[8:12], "big")
        rsize = int.from_bytes(ch[12:16], "big")
        if rsize < 12 or nrec > 64:
            return ""
        want = {(b"en", b"US"), (b"en", b"GB")}
        fb = ""
        for i in range(nrec):
            r0 = 16 + i * rsize
            if r0 + 12 > len(ch):
                break
            lang, cty = ch[r0 : r0 + 2], ch[r0 + 2 : r0 + 4]
            ln = int.from_bytes(ch[r0 + 4 : r0 + 8], "big")
            off = int.from_bytes(ch[r0 + 8 : r0 + 12], "big")
            if off + ln > len(ch):
                continue
            tx = ch[off : off + ln].decode("utf-16-be", errors="replace").strip("\x00")
            if not tx:
                continue
            if (lang, cty) in want:
                return tx
            fb = fb or tx
        return fb
    if typ == b"desc":
        return ch[12:79].split(b"\x00")[0].decode("latin-1", errors="replace").strip()
    return ""


def is_zip(b: bytes) -> bool:
    return len(b) >= 4 and b[:4] == b"PK\x03\x04"


def printers_from_string(t: str) -> str:
    ps = sorted({int(x) for x in re.findall(r"\bP(\d{3,5})\b", t, flags=re.I)})
    if ps:
        return "/".join(f"P{n}" for n in ps)
    ys = sorted({int(x) for x in re.findall(r"\b(\d{4})\b", t)})
    ys = [n for n in ys if 7000 <= n <= 20000]
    if ys:
        return "/".join(f"P{n}" for n in ys)
    return ""


def moab_obs(rel: str, leaf: str, desc: str) -> Observation:
    dc = desc.replace(".icc", "").strip()
    token = ""
    m = re.match(r"^MOAB\s+(.+?)\s+(P\d+(?:/[A-Za-z0-9]+)?)\s+(.+)$", dc, re.I)
    if m:
        paper_raw, pslot, suf = m.group(1).strip(), m.group(2), m.group(3).strip()
        nk = norm_key(paper_raw)
        t0 = suf.split()[0].split("-")[0].replace("%", "").strip()
        token = re.sub(r"[\.\s]+$", "", t0)
        printers = printers_from_string(pslot + " " + desc)
    else:
        nk = norm_key(re.sub(r"^MOAB\s+", "", dc, flags=re.I))
        printers = printers_from_string(dc)
    return Observation(
        vendor="moab",
        rel_bundle=rel,
        icc_leaf=leaf,
        desc=desc.strip(),
        printers_hint=printers,
        media_token=token,
        fingerprints=fp_from_paper_name(nk),
        match_key=nk,
    )


def rr_obs(rel: str, leaf: str, desc: str) -> Observation:
    s = re.sub(r"^RR\s+", "", desc.strip(), flags=re.I)
    tail = re.sub(r"\s*P9570\s*P7570.*$", "", s, flags=re.I)
    tail = re.sub(r"\s*Ep\s*[Pp]?\s*[\d\-/\s]+$", "", tail)
    tail = re.sub(r"\s*Ep\d+.*$", "", tail, flags=re.I)
    nk = norm_key(tail)
    return Observation(
        vendor="red_river",
        rel_bundle=rel,
        icc_leaf=leaf,
        desc=desc.strip(),
        printers_hint=printers_from_string(desc),
        fingerprints=fp_from_paper_name(nk),
        match_key=nk,
    )


def walk_vendor_zips(sub: str, vendor: str) -> list[Observation]:
    base = PAPER_ICC / sub
    if not base.is_dir():
        return []
    out: list[Observation] = []
    for zp in sorted(base.rglob("*.zip")):
        rel = str(zp.relative_to(PAPER_ICC))
        try:
            with zipfile.ZipFile(zp) as zf:
                for m in zf.namelist():
                    if "__MACOSX" in m or m.endswith("/") or not m.lower().endswith(".icc"):
                        continue
                    blob = zf.read(m)
                    if is_zip(blob):
                        continue
                    d = read_desc(blob)
                    out.append(moab_obs(rel, m, d) if vendor == "moab" else rr_obs(rel, m, d))
        except (zipfile.BadZipFile, OSError):
            continue
    return out


def walk_hahn_fake_icc() -> list[Observation]:
    hn = PAPER_ICC / "Hahnemuhle_ICC_Epson_EN"
    if not hn.is_dir():
        return []
    out: list[Observation] = []
    for bundle in sorted(hn.rglob("*.icc")):
        raw = bundle.read_bytes()
        if not is_zip(raw):
            continue
        rel_b = str(bundle.relative_to(PAPER_ICC))
        parent_nm = bundle.parent.name
        pr_hint = ""
        mm = re.search(r"SC-P(\d+)/P(\d+)", parent_nm)
        if mm:
            pr_hint = f"SC-P{mm.group(1)}/SC-P{mm.group(2)}"
        bundle_nm = bundle.stem
        slug = re.sub(r"^hm_dl\d+_", "", bundle_nm, flags=re.I).strip()

        slug = re.sub(r"\s*\((icc|emx)\)\s*$", "", slug, flags=re.I).strip()
        slug_nk = norm_key(slug)
        fp = fp_from_paper_name(slug)
        with zipfile.ZipFile(bundle) as zf:
            for n in zf.namelist():
                if "__MACOSX" in n or not n.lower().endswith(".icc"):
                    continue
                blob = zf.read(n)
                dk = read_desc(blob)
                up = n.upper()
                pk = "MK" if "_MK_" in up else ("PK" if "_PK_" in up else "")
                out.append(
                    Observation(
                        vendor="hahnemuehle",
                        rel_bundle=rel_b,
                        icc_leaf=n,
                        desc=(dk or Path(n).name).strip(),
                        printers_hint=pr_hint,
                        pk_hint=pk,
                        fingerprints=fp,
                        match_key=slug_nk,
                    )
                )
    return out


def match_score(paper_cell: str, ob: Observation) -> float:
    row_keys = set(fp_from_paper_name(paper_cell)) | {norm_key(paper_cell)}
    ob_key = ob.match_key or (ob.fingerprints[0] if ob.fingerprints else "")
    if not ob_key:
        return 0.0
    best = 0.0
    for rk in row_keys:
        if not rk:
            continue
        if rk == ob_key:
            best = max(best, 1.0)
        else:
            best = max(best, SequenceMatcher(None, rk, ob_key).ratio())
    return best


def stitch_printer(old: str | None, parts: list[str]) -> str:
    acc: list[str] = []
    if old and str(old).strip():
        acc.append(str(old).strip())
    for p in parts:
        p = p.strip()
        if p and p not in acc:
            acc.append(p)
    return "；".join(acc)


def purge_icc_tail(s: str | None) -> str:
    if not s:
        return ""
    t = str(s)
    i = t.find("【ICC 本地样本")
    if i >= 0:
        return t[:i].rstrip()
    return t


def stitch_note(old: str | None, block: str) -> str:
    mark = "【ICC 本地样本"
    o = (old or "").strip()
    b = block.strip()
    if not b:
        return old or ""
    if mark in o and b[:80] in o:
        return old or ""
    return (o + "\n\n" + b) if o else b


def main() -> None:
    if not PAPER_ICC.is_dir():
        raise SystemExit(f"缺少目录: {PAPER_ICC}")
    obs: list[Observation] = []
    obs.extend(walk_vendor_zips("Moab_Epson", "moab"))
    obs.extend(walk_vendor_zips("Red_River_PRO12", "red_river"))
    obs.extend(walk_hahn_fake_icc())
    by_v: dict[str, list[Observation]] = defaultdict(list)
    for o in obs:
        by_v[o.vendor].append(o)

    wb = openpyxl.load_workbook(XLSX)
    todo = [
        ("欧洲品牌 EU", "hahnemuehle"),
        ("美国品牌 US", "moab"),
        ("美国品牌 US", "red_river"),
    ]
    for title, vk in todo:
        ws = wb[title]
        brand_expect = BRAND_FOR_VENDOR[vk]
        buckets: dict[int, list[Observation]] = defaultdict(list)
        pool = by_v[vk]
        for r in range(2, ws.max_row + 1):
            br = ws.cell(r, 3).value
            pn = ws.cell(r, 5).value
            if br != brand_expect or not pn:
                continue
            for ob in pool:
                if match_score(str(pn), ob) >= MATCH_MIN:
                    buckets[r].append(ob)

        for r, lst in sorted(buckets.items()):
            uniq_b = sorted({x.rel_bundle for x in lst})
            uniq_d: list[str] = []
            seen: set[str] = set()
            toks = sorted({x.media_token for x in lst if x.media_token})
            pbits: list[str] = []
            for x in lst:
                if x.desc and x.desc not in seen:
                    uniq_d.append(x.desc)
                    seen.add(x.desc)
                if x.printers_hint:
                    pbits.append(x.printers_hint)
            pks = [x.pk_hint for x in lst if x.pk_hint]
            if pks:
                pbits.append(",".join(sorted(set(pks))))
            desc_line = "; ".join(uniq_d[:10])
            if len(uniq_d) > 10:
                desc_line += f" …共{len(uniq_d)}条"
            block = "【ICC 本地样本 docs/paper-icc】\n"
            block += "- 包：" + "；".join(uniq_b[:5])
            if len(uniq_b) > 5:
                block += f" …等{len(uniq_b)}包"
            block += "\n- ICC desc：" + desc_line
            if toks:
                block += "\n- profile 文件名 token：" + ",".join(toks) + "（释义见各厂 ICC 包说明）"
            ws.cell(r, 12).value = stitch_printer(None, pbits)
            base_note = purge_icc_tail(ws.cell(r, 22).value)
            ws.cell(r, 22).value = stitch_note(base_note, block)
            ws.cell(r, 23).value = VERIFIED

    wb.save(XLSX)
    print(f"已更新 {XLSX}；ICC 观察 {len(obs)} 条。")


if __name__ == "__main__":
    main()
