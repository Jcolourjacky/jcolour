文件: Europe-US-FineArt-Papers-Database.xlsx

工作表:
- 使用说明
- 欧洲品牌 EU
- 美国品牌 US
- 亚洲品牌 Asia（日本 Awagami、三菱系、韩国 Hansol、台港澳/大陆渠道等）
- 全部合并 All（欧+美+亚）

说明: 初版为常见品牌种子数据。下列字段只能来自「各厂 ICC 下载包内的 PDF / readme / handling sheet」摘录（文件名+页码/段落写进备注或单独维护来源列），不可用纸类通则、脚本或经验数字代替填写：
厚度、平台间隙、纸厚校准、干燥（机内/pass 间隔 或印后静置）、总墨量/限墨/TIL/Epson Media 微调等。

ICC 包里常见写什么（评估）：
● 几乎每个包：介质类型英文名、机型范围、MK/PK；
● 常写：色密度 Color density ±%；部分写 Platen gap、Paper thickness、drying time per pass；
● 单独给出「总墨量 %」的 PDF 相对较少，更多在 ICC/驱动「自定义纸张」链路里隐含——若 PDF 无明文，表中宁留空，备注「原厂 PDF 未载明」；
● mm 厚度有时不在 ICC readme，而在纸业 technical datasheet——若采用须注明第二张文档的来源，否则会与『ICC 摘录库』的定位混淆。

禁止使用 `fill_til_notes.py` 一类脚本向表中批量写入推断性说明（已移除）。

重建「亚洲品牌」与合并表：`python docs/paper-database/build_asia_sheet.py`（需已安装 openpyxl）。**不会**再批量清空欧/美表「总墨量/TIL」列；该列请仅从各厂 ICC 包内 PDF/readme 摘录后手填。

ICC 抓取缓存（Awagami / Moab PRO12 / Red River / Hahnemühle 等）：仓库根执行 `python docs/paper-icc/fetch_icc_samples.py`；输出写入 `docs/paper-icc/`。若脚本不在仓库内路径，可设环境变量 `JCOLOUR_ROOT` 指向 jcolour 仓库根后再运行同上命令。

将已下载 ICC 的 desc/机型提示写回「欧洲/美国」表（仅备注与「适用打印机」列，不自动填厚度/TIL）：`python docs/paper-database/supplement_eu_us_from_icc.py`。随后若需刷新「亚洲」「全部合并」工作表，再运行 `python docs/paper-database/build_asia_sheet.py`。

Moab《Media Settings》PDF → 写入「美国」表总墨量/TIL 列（第 19 列，含「PDF 未给 TIL%」+ 表格摘录）：`python docs/paper-database/populate_moab_media_til_column.py`（`--dry-run` 仅预览）。需 `pdftotext`。

从 ICC 包内 PDF 抽取「总墨量/TIL/色密度±%」等**原文片段草稿**（须人工核对后粘贴）：`python docs/paper-database/extract_pdf_til_snippets.py` → 输出 `docs/paper-icc/TIL_PDF_SNIPPETS_DRAFT.md`（可选 `--out-json`）。需已安装 `pdftotext`（`brew install poppler`）。