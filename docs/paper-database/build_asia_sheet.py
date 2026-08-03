#!/usr/bin/env python3
"""Regenerate 「亚洲品牌 Asia」 sheet & 「全部合并 All」with strict 23 columns."""
from __future__ import annotations

import sys
from pathlib import Path

import openpyxl
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter

ROOT = Path(__file__).resolve().parents[2]
PATH = ROOT / "docs/paper-database/Europe-US-FineArt-Papers-Database.xlsx"

thin = Side(style="thin", color="CCCCCC")
border = Border(left=thin, right=thin, top=thin, bottom=thin)
hdr_fill = PatternFill("solid", fgColor="4472C4")
hdr_font = Font(bold=True, color="FFFFFF")


def make_row(
    *,
    region: str,
    country: str,
    brand: str,
    line: str,
    paper: str,
    gsm: str = "",
    thick: str = "",
    surf: str = "",
    base: str = "",
    arch: str = "",
    oba: str = "",
    pnote: str = "",
    media: str = "",
    pk: str = "",
    cd: str = "",
    pl: str = "",
    pt: str = "",
    dry: str = "",
    til: str = "",
    ac: str = "",
    url: str = "",
    notes: str = "",
    verified: str = "",
) -> list:
    vs = paper, gsm, thick, surf, base, arch, oba, pnote, media, pk, cd, pl, pt, dry, til, ac, url, notes, verified
    return [region, country, brand, line, *vs]


def rebuild() -> None:
    wb = openpyxl.load_workbook(PATH)
    # 不再批量清空欧/美表第 19 列（总墨量/TIL）：该列仅允许从各厂 ICC 包内 PDF/readme 人工摘录后填写，
    # 运行本脚本重建「亚洲 / 全部合并」时不应抹掉已录入内容。
    eu = wb["欧洲品牌 EU"]
    headers = [eu.cell(row=1, column=c).value for c in range(1, eu.max_column + 1)]
    assert len(headers) == 23

    wb.remove(wb["亚洲品牌 Asia"])
    ws = wb.create_sheet(title="亚洲品牌 Asia")
    for c, h in enumerate(headers, 1):
        cell = ws.cell(row=1, column=c, value=h)
        cell.fill = hdr_fill
        cell.font = hdr_font
        cell.border = border
        cell.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)

    AR = ("https://awagami.com/pages/awagami-aijp-icc-profiles")

    packs: list[dict] = []
    jp_a = dict(country="JP", brand="Awagami（阿波和纸）", line="A.I.J.P")

    packs += [
        {**jp_a, "paper": "AIJP Bamboo", "gsm": "170/250/365", "surf": "哑光", "base": "竹+木浆", "arch": "和纸传统工艺", "oba": "以厂方说明为准",
         "pnote": "SureColor/P 等多机型见 ICC 列表", "media": "Velvet Fine Art / Ultra Premium Presentation Matte", "pk": "MK",
         "url": AR,
         "notes": "介质名、纸厚校准、干燥、墨量限值等仅从 Awagami 各机型 ICC 附带 PDF/README 摘录；勿凭纸类类推。"},
        {**jp_a, "paper": "AIJP Bamboo Business", "gsm": "275", "surf": "哑光", "base": "竹纤维", "media": "Velvet Fine Art", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Bizan", "surf": "纹理", "base": "棉+草混合和纸", "media": "Velvet Fine Art / UPPPM", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Hosho", "surf": "较平滑", "base": "楮皮层", "media": "见各机型 ICC 附带 PDF", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Inbe", "surf": "中等纹理", "base": "三桠+草浆", "media": "Velvet Fine Art", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Kozo", "surf": "和纸纹", "base": "楮皮为主体", "media": "Velvet Fine Art（或 Washi）", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Mitsumata", "surf": "细腻", "base": "三桠", "media": "Velvet Fine Art", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Murakumo Kozo", "surf": "云流纤维", "base": "楮皮", "media": "Velvet Fine Art", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Shiramine", "surf": "偏平滑", "media": "Ultra Premium Presentation Matte", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Unryu", "surf": "可见麻丝纤维", "base": "草浆等", "media": "Velvet Fine Art", "pk": "MK", "url": AR},
        {**jp_a, "paper": "AIJP Natural Silky / Silky Surface", "surf": "丝手感", "media": "以 profile PDF 为准", "pk": "MK", "url": AR},
    ]

    jp_mpm = dict(country="JP", brand="Mitsubishi Paper Mills", line="Production inkjet coated")
    packs += [
        {**jp_mpm, "paper": "涂布卷材（多 SKU）", "gsm": "—", "surf": "哑光至高光均有", "base": "涂布", "pnote": "商业/条幅多于纯艺术",
         "media": "由代理商技术表对标 Epson Media Type",
         "url": "https://www.mpm.co.jp/pro-ij/eng/coated-paper.html", "notes": "逐 SKU 补全厚度/墨量"},
        {**jp_mpm, "paper": "涂布亮光/半光泽取向", "surf": "高光·半光", "base": "涂布",
         "media": "类比 Premium Glossy/Luster（依代理）", "pk": "PK", "url": "https://www.mpm.co.jp/pro-ij/eng/"},
    ]

    jp_hit = dict(country="JP", brand="Mitsubishi HiTec Paper", line="jetscript®")
    packs += [{
        **jp_hit,
        "paper": "jetscript 系列（lustre/gloss/matte …）",
        "surf": "多种光泽",
        "base": "涂布（部分欧洲产）",
        "pnote": "亚太亦有经销",
        "media": "依产品 datasheet",
        "url": "https://www.mitsubishi-paper.com/en/hitec-paper/products/jetscript/",
    }]

    packs += [{
        "country": "KR", "brand": "Hansol Paper", "line": "Inkjet glossy",
        "paper": "相纸涂层产品线（Inkjet glossy 系列等）",
        "surf": "高光·缎面",
        "base": "涂布",
        "pnote": "办公·商业为多",
        "media": "参考 Premium Glossy / Enhanced Matte 类比",
        "url": "https://www.hansol.com/eng/page/papers",
        "notes": "艺术展陈需查证具体 SKU，再录墨量参数",
    }]

    packs += [{
        "country": "TW", "brand": "台湾渠道", "line": "进口纸分装",
        "paper": "Hahnemühle/Moab/Canson 等区域代理 SKU",
        "surf": "依原纸", "base": "依原纸",
        "pnote": "与欧美同 SKU",
        "media": "原厂 ICC ZIP 附带「Media Type」",
        "notes": "无双品牌统一 ICC",
    }, {
        "country": "CN", "brand": "中国大陆渠道", "line": "国产/典藏纸",
        "paper": "无统一商标棉 Rag、典藏涂布等",
        "surf": "多为哑光",
        "base": "棉/混合浆",
        "oba": "向厂方索要",
        "pnote": "电商/策展供应零散",
        "media": "常类比 Velvet Fine Art / Thick Fine Art（需校准条验证）",
        "pk": "MK",
        "notes": "有可用的厂家 ICC/readme 时，再逐项抄录介质名、间隙、干燥、墨量上限等",
    }, {
        "country": "HK", "brand": "港澳·东南亚分销", "line": "国际型号",
        "paper": "与欧美同款（包装电压差异）",
        "surf": "—",
        "base": "—",
        "media": "同原厂 ICC ZIP",
        "notes": "不改变纸基化学",
    }, {
        "country": "JP", "brand": "王子控股 Oji Holdings", "line": "产业材料（参考）",
        "paper": "大批量写真/包装涂布卷材",
        "surf": "半光/高光",
        "base": "涂布",
        "pnote": "POP、轻展示",
        "pk": "PK",
        "url": "https://www.ojiholdings.co.jp/",
        "notes": "艺术展陈更多选用 Awagami 或西欧 Rag",
    }]

    for r, d in enumerate(packs, start=2):
        assert "paper" in d and "brand" in d
        row = make_row(
            region="亚洲",
            country=d.get("country", ""),
            brand=d["brand"],
            line=d.get("line", ""),
            paper=d["paper"],
            gsm=d.get("gsm", ""),
            thick=d.get("thick", ""),
            surf=d.get("surf", ""),
            base=d.get("base", ""),
            arch=d.get("arch", ""),
            oba=d.get("oba", ""),
            pnote=d.get("pnote", ""),
            media=d.get("media", ""),
            pk=d.get("pk", ""),
            cd=d.get("cd", ""),
            pl=d.get("pl", ""),
            pt=d.get("pt", ""),
            dry=d.get("dry", ""),
            til=d.get("til", ""),
            ac=d.get("ac", ""),
            url=d.get("url", ""),
            notes=d.get("notes", ""),
            verified=d.get("verified", ""),
        )
        assert len(row) == len(headers), (d["paper"], len(row))
        for c, v in enumerate(row, 1):
            cell = ws.cell(row=r, column=c, value=v)
            cell.border = border
            cell.alignment = Alignment(wrap_text=True, vertical="top")

    widths = {5: 32, 12: 22, 13: 36, 21: 40, 22: 34}
    for col in range(1, len(headers) + 1):
        ws.column_dimensions[get_column_letter(col)].width = widths.get(col, 12)
    ws.freeze_panes = "A2"
    ws.auto_filter.ref = f"A1:{get_column_letter(len(headers))}{ws.max_row}"

    wb.remove(wb["全部合并 All"])
    ws_all = wb.create_sheet(title="全部合并 All")
    for c, h in enumerate(headers, 1):
        cell = ws_all.cell(row=1, column=c, value=h)
        cell.fill = hdr_fill
        cell.font = hdr_font
        cell.border = border
    rn = 2
    for name in ("欧洲品牌 EU", "美国品牌 US", "亚洲品牌 Asia"):
        src = wb[name]
        for r in range(2, src.max_row + 1):
            for c in range(1, len(headers) + 1):
                ws_all.cell(row=rn, column=c, value=src.cell(row=r, column=c).value).border = border
            rn += 1
    ws_all.freeze_panes = "A2"
    ws_all.auto_filter.ref = f"A1:{get_column_letter(len(headers))}{rn - 1}"
    ws_all.column_dimensions["E"].width = 28
    ws_all.column_dimensions["M"].width = 36
    ws_all.column_dimensions["U"].width = 36

    usage = wb["使用说明"]
    for r in range(1, usage.max_row + 1):
        cell = usage.cell(r, 1)
        v = cell.value
        if isinstance(v, str) and "fill_til_notes" in v:
            cell.value = (
                "「总墨量 / 厚度 / 干燥」等运维参数：仅从各厂 ICC 下载包附带 PDF/readme/handling sheet 逐项摘录 "
                "（建议在备注写明文件名与页码）。文档未写明则留空，勿用纸类通则或脚本批量生成。"
            )
            break

    wb.save(PATH)
    print(f"Asia rows: {ws.max_row - 1}, merge rows: {rn - 2}")


if __name__ == "__main__":
    rebuild()
    sys.exit(0)
