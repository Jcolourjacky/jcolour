# 本機裁切完整度評估 — OPOS / ARMS / CutContour

> 對 `engine/src/external_cutter.{h,cpp}`（776 行）的 EA 階段評估。
> 結論：**串口傳輸與指令字串成形可用；但「裁切對位」（OPOS/ARMS 的核心）尚未實作，且座標單位、輪廓 offset、條碼為佔位實作。** 屬 ROADMAP v1.0「外部裁切機準備」範圍，EA 不阻塞。

## 完整度矩陣

| 區塊 | 狀態 | 說明 |
|------|------|------|
| 串口傳輸 `connect/send/disconnect` | ✅ 可用 | QSerialPort，8N1、逾時、`parse_response` 偵測 ERROR/NAK |
| 機型能力表 `capabilities` / `model_name` | ✅ 可用 | 靜態資料 |
| `bounding_box` / `calculate_total_length` | ✅ 正確 | |
| OPOS / SCPL 指令字串成形 | 🟡 形似 | 指令格式合理但**未經實機驗證**；OPOS 的 `MD/D` 系列為自訂、非真 GP-GL |
| CutContour 由影像產生（矩形/border） | 🟡 基本 | `generate_from_image` / `generate_rectangle` 走 border_mm，僅矩形 |
| **座標單位轉換** | 🔴 缺 | 直接輸出 mm；GP-GL/HP-GL 單位為 0.025mm（40 units/mm）→ 實機 **40× 比例錯誤** |
| **OPOS/ARMS 標記偵測流程** | 🔴 缺 | 只「繪製」十字標記，沒有命令叫裁切機**讀取**標記對位 → 裁切不會對齊列印 |
| 輪廓 offset / bleed（Inside/Outside/Oversized） | 🔴 佔位 | `offset_contour`/`generate_bleed_cut` 對每點做對角平移 (±d,±d)，非法線方向多邊形 offset（程式碼註解自承 "in production use proper contour offsetting"） |
| `merge_contours` | 🟡 粗糙 | O(n²) 貪婪就近串接，會把不相關點連起、一律標 closed |
| `render_barcode`（Code 39） | 🔴 佔位 | 「Simplified — draw as rectangles」；patterns 表只有 0–9 且用 `char_idx % 10`、bar 間無抬筆 → 不可掃描 |
| 機型↔語言對應 | 🔴 存疑 | Graphtec FC9000/FC8600 被對應到 SCPL（Summa 語言），實際應為 GP-GL/HP-GL |
| 列舉文件註解 | ✅ 已修 | OPOS=Graphtec、ARMS=Roland（原誤標 Epson/Adobe）— 本次更正 |

## 阻塞「真正可出活」的三件事

1. **單位轉換層**：在送出前把 mm → 機型 plotter units（GP-GL 0.025mm、HP-GL 1016/inch）。目前全部當 mm 直送。
2. **OPOS/ARMS 對位**：印件帶 registration marks 後，需送機型專屬的「掃描標記」指令（Graphtec OPOS: `\x1bFW`/`OPOS` 流程；Roland ARMS: quad-mark 偵測），讀回偏移再做仿射校正套到 cut path。**這是 OPOS/ARMS 的全部意義，目前完全缺。**
3. **真實輪廓 offset**：用法線方向偏移 + 自交清理（建議 Clipper，與 ROADMAP v1.0 True Shape Nesting 的 libnest2d/Clipper 同源），支援 Inside/Outside/Oversized/Exact 五種 `CutContourMode`。

## 本次處理

- 更正 `CutLanguage` 列舉的領域註解（OPOS=Graphtec 光學定位、ARMS=Roland 自動標記對位、SCPL=Summa、GPGL=Graphtec），並加註「OPOS/ARMS 是標記感測系統、非指令語言」。
- 不在 EA 重寫 offset/對位/條碼——屬 v1.0「外部裁切機準備（OPOS/ARMS/CutContour 專色層 + PDF/X 導出）」工作項；本文件作為該階段的起點清單。

## 建議 ROADMAP 動作

- EA：維持現狀（驅動骨架就位即可），本評估歸檔。
- v1.0：依「阻塞三件事」順序實作；CutContour 專色層改由 PDF/X spot channel 觸發（與 `spot_color` 模組對接），而非影像矩形。
