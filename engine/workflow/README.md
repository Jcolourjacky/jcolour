# engine/workflow/

按编号分组的端到端工作流实现（wf = workflow）。每个 `wfN_*` 套件对应一条独立工作流，内部按阶段拆为多个文件（`chart` / `measure` / `lut` / `print` / `self_test` / 等）。

## 现有编号

| # | 套件 | 主要文件 |
|---|-----|-----|
| **wf1** | 单黑 B&W（1 通道） | `wf1_chart`、`wf1_param`、`wf1_print`、`wf1_measure`、`wf1_ink_limit`、`wf1_lut`、`wf1_end_to_end` |
| **wf2** | 多色黑 B&W（含黑墨族） | `wf2_chart`、`wf2_print`、`wf2_measure`、`wf2_lut`、`wf2_self_test` |
| **wf3** | B&W + 微彩 split-tone（7 通道） | `wf3_chart`、`wf3_print`、`wf3_measure`、`wf3_lut`、`wf3_self_test` |
| **wf4** | 全 12 通道彩色（Argyll ICC） | `wf4_chart`、`wf4_print`、`wf4_measure`、`wf4_profile`、`wf4_transform`、`wf4_self_test`、`wf4_end_to_end` |
| **wf5** | **编号保留**（未来扩充用） | — |
| **wf6** | ProPhoto RGB → 12 通道 + 色域压缩 | `wf6_chart`、`wf6_print`、`wf6_gamut`、`wf6_self_test` |

### 为什么没有 wf5？

**编号保留**，不是缺失实现。预留 wf5 给未来插入的工作流（单通道纯净度复检、多光源 metamerism 验证、或 v1.5 新校准环节），避免后续重编号破坏所有引用与外部脚本。

---

## 支持的打印机（v1.0）

仅 Epson SureColor PRO12 系列（中国版型号 P\*580 = 全球版 P\*570，同硬件、同墨水）：

| 中国型号 | 全球型号 | 幅宽 |
|---------|---------|------|
| **SureColor P7580** | P7570 | 24" / 610mm |
| **SureColor P9580** | P9570 | 44" / 1118mm |
| **SureColor P20580** | P20570 | 64" / 1626mm |

三机共享同一 12 色墨水组（UltraChrome PRO12 / 国内称「活的色彩 PRO」），打印头、ICC 流程、本仓库的工作流定义**完全一致**。

---

## 12 装载 / 11 实效（**心智模型核心**）

PRO12 标称「12 色」但**单张图实际只用 11 个通道**：

```
┌────────────────────────────────────────────────────┐
│ 硬件层（PRO12 喷头）                                 │
│   12 个噴頭通道，全部装载                            │
│   PK + MK + LC + C + VLM + VM + Y + Or + GY + LGY +│
│   V + Gr                                            │
├────────────────────────────────────────────────────┤
│ 固件 / 驱动层                                        │
│   按纸张选 PK 或 MK（另一个的喷嘴被 mask 掉）         │
│   ── 一张图永远只喷 11 道墨 ──                       │
├────────────────────────────────────────────────────┤
│ jcolour 软件层                                       │
│   内部模型 = 11 active + BlackKind 元数据             │
│   边界 = expand_to_physical() 还原 12 槽位            │
└────────────────────────────────────────────────────┘
```

### 数据契约

| 层 | 类型 | 说明 |
|----|------|------|
| 内部（halftone、SQP、ICC LUT 应用） | `ActiveInks = std::array<float, 11>` | 11 个 active 通道；MAIN_K 在 index 0 |
| 边界（PJL 输出、Argyll device profile） | `PhysicalInks = std::array<float, 12>` | 12 个物理槽位，其中一个黑墨永远 = 0 |
| 元数据 | `BlackKind { PK, MK }` | 由 paper_id 决定，**session 不变** |

```cpp
// 内部 → PJL 边界
auto physical = expand_to_physical(active_inks, BlackKind::PK);
// MAIN_K 落到 physical[0]（PK 槽），physical[1]（MK 槽）= 0
```

**这是型别系统级别的保证**——`array<float, 11>` 跟 `array<float, 12>` 不能直接互换，写出 PK+MK 同时输出的代码会编译失败。规范定义在 [`../src/channel_layout.h`](../src/channel_layout.h)。

### 11-Active 通道索引

| Active Index | jcolour 符号 | Epson 官方名 | 中文 |
|:-----:|------|-----------------|------|
| **0** | **MAIN_K** | Photo Black 或 Matte Black（由 BlackKind 决定） | 主黑 |
| 1 | LC | Light Cyan | 淡青 |
| 2 | C | Cyan | 青 |
| 3 | VLM | Vivid Light Magenta | 淡鲜洋红 |
| 4 | VM | Vivid Magenta | 鲜洋红 |
| 5 | Y | Yellow | 黄 |
| 6 | Or | Orange | 橙 |
| 7 | GY | Gray | 灰 |
| 8 | LGY | Light Gray | 淡灰 |
| 9 | V | Violet | 紫罗兰 |
| 10 | Gr | Green | 绿 |

### 12-Physical 物理通道映射（PJL 输出）

| Physical Index | 通道 | 装载状态 | 来源 |
|:---:|------|---------|------|
| 0 | PK | 永远装载 | BlackKind::PK 时 = ActiveInks[0]；BlackKind::MK 时 = 0 |
| 1 | MK | 永远装载 | BlackKind::MK 时 = ActiveInks[0]；BlackKind::PK 时 = 0 |
| 2 | LC | 永远装载 | ActiveInks[1] |
| 3 | C  | 永远装载 | ActiveInks[2] |
| 4 | VLM| 永远装载 | ActiveInks[3] |
| 5 | VM | 永远装载 | ActiveInks[4] |
| 6 | Y  | 永远装载 | ActiveInks[5] |
| 7 | Or | 永远装载 | ActiveInks[6] |
| 8 | GY | 永远装载 | ActiveInks[7] |
| 9 | LGY| 永远装载 | ActiveInks[8] |
| 10| V  | 永远装载 | ActiveInks[9] |
| 11| Gr | 永远装载 | ActiveInks[10] |

### ICC profile 约定

由于 PK/MK 互斥，**同一张纸只对应一份 11 通道 ICC**：

```
hahnemühle_photo_rag_PK.icc    ← 用 PK 主黑训练（11 通道）
hahnemühle_photo_rag_MK.icc    ← 用 MK 主黑训练（11 通道）
canson_baryta_PK.icc           ← baryta 类只能用 PK
epson_legacy_matte_MK.icc      ← matte 类只能用 MK
```

Argyll `colprof -d3` 命令时仍传 12 通道 device space，但目标 ti3 文件中的其中一个黑墨通道恒为 0（targen 不要把那个通道纳入 patch 生成）。jcolour 内部加载 ICC 后立刻 `collapse_from_physical()` 回 11 通道。

### 规范约束

1. **黑墨互斥规则**（编译期强制）：
   - 任何内部代码看到的都是 `ActiveInks`（11 维），不可能写出 PK+MK 同时输出
   - PJL 边界用 `expand_to_physical(active, kind)`，函数本身保证只有一个黑墨槽位非零
   - 调试用：`obeys_black_exclusivity(physical)` 可在送印前做断言
2. **VM / VLM ≠ M / LM**：PRO12 的洋红是 Vivid 配方，色域比旧 HDX 的 M/LM 宽。不可混用旧 ICC 或旧曲线。
3. **GY / LGY ≠ LK / LLK**：PRO12 的灰墨是真灰（中性灰，不带蓝偏），不是旧 HDX 的"淡黑/超淡黑"。
4. **per-Paper × per-BlackKind ICC**：见上节，不存在「同一张纸两个黑都能用」的 ICC。

---

## 旧表（保留作 alias 查询）

| Index | 符号 | Epson 官方英文名 | Epson 中国官方中文名 | 别称 / 旧称（仅说明） |
|:-----:|------|-----------------|--------------------|---------------------|
| **0** | **PK** | Photo Black | 照片黑 | — |
| **1** | **MK** | Matte Black | 粗面黑 | — |
| **2** | **LC** | Light Cyan | 淡青 | — |
| **3** | **C** | Cyan | 青 | — |
| **4** | **VLM** | Vivid Light Magenta | 淡鲜洋红 | LM（旧 HDX 用语） |
| **5** | **VM** | Vivid Magenta | 鲜洋红 | M（旧 HDX 用语） |
| **6** | **Y** | Yellow | 黄 | — |
| **7** | **Or** | Orange | 橙 | OR |
| **8** | **GY** | Gray | 灰 | LK（"Light Black"，旧 HDX 用语） |
| **9** | **LGY** | Light Gray | 淡灰 | LLK（"Light Light Black"，旧 HDX 用语） |
| **10** | **V** | Violet | 紫罗兰 | Vi |
| **11** | **Gr** | Green | 绿 | GR |

### 规范约束

1. **黑墨互斥规则**（重要）：
   - PK 和 MK **同时装载**，但**单张图只用其中一种**，由纸张类型决定：
     - 光面 / 半光面 / 绢丝 / Baryta / 一切 photo 类纸 → **PK**
     - 哑面 / 棉质 / 美术 / 油画布 → **MK**
   - LUT 必须按 paper_id 选黑墨，**不得**在同一像素同时输出 PK + MK
   - 这是 Epson "Realtime Photo and Matte Black ink switching" 功能的代价：硬件无需切换，但内容生成端有义务保证互斥
2. **VM / VLM ≠ M / LM**：PRO12 的洋红是 Vivid 配方，色域比旧 HDX 的 M/LM 宽。不可混用旧 ICC 或旧曲线。
3. **GY / LGY ≠ LK / LLK**：PRO12 的灰墨是真灰（中性灰，不带蓝偏），不是旧 HDX 的"淡黑/超淡黑"。
4. **ICC 通道顺序**：Argyll `colprof -d3` 生成的 12 通道 ICC 应使用本表 index 顺序。若驱动 PPD 顺序不同，需在 `pjl.cpp` 层做 channel mapping，不在 ICC 层做。

---

## 各 workflow 实际用到的通道

依「黑墨族 + 灰墨族 + 彩墨族」分组。打勾 = 该 wf 主动使用；XOR 表示二选一（依纸张）。

| Index | 符号 | wf1 单黑 | wf2 多色黑 | wf3 B&W+CMY | wf4 全彩 | wf6 RGB→12 |
|:-----:|------|:--------:|:----------:|:-----------:|:--------:|:----------:|
| 0 | PK | XOR | XOR | XOR | XOR | XOR |
| 1 | MK | XOR | XOR | XOR | XOR | XOR |
| 2 | LC |   |   |   | ✅ | ✅ |
| 3 | C  |   |   | ✅ | ✅ | ✅ |
| 4 | VLM|   |   |   | ✅ | ✅ |
| 5 | VM |   |   | ✅ | ✅ | ✅ |
| 6 | Y  |   |   | ✅ | ✅ | ✅ |
| 7 | Or |   |   |   | ✅ | ✅ |
| 8 | GY |   | ✅ | ✅ | ✅ | ✅ |
| 9 | LGY|   | ✅ | ✅ | ✅ | ✅ |
| 10| V  |   |   |   | ✅ | ✅ |
| 11| Gr |   |   |   | ✅ | ✅ |

### wf2 设计已修正（DONE 2026-05）

已重构为 3 通道 `{MAIN_K, GY, LGY}` + `BlackKind` 元数据。详见 `wf2_chart.h`。

### wf3 / wf4 / wf6 设计修正（IN PROGRESS）

同样的 PK/MK 互斥问题存在于 wf3（7 ch 含 PK+MK）、wf4（12 ch 含 PK+MK）、wf6（12 ch 含 PK+MK），正在按 `channel_layout.h` 的 `ActiveInks`/`PhysicalInks` 模型重构：

- wf3：6 ch active `{MAIN_K, GY, LGY, C, VM, Y}`
- wf4：11 ch active（完整 PRO12 active 集）
- wf6：同 wf4 + gamut compression

---

## 文件命名约定

- `wfN_chart.{h,cpp}` — 测靶设计与生成
- `wfN_print.{h,cpp}` — PJL 列印 + halftoning
- `wfN_measure.{h,cpp}` — i1Pro3 / i1iO 测量 + 油墨模型 fit
- `wfN_lut.{h,cpp}` — LUT 生成（含 SQP / CYNSN 优化）
- `wfN_self_test.{h,cpp}` — 三段粒度重校（self-test / light / full）
- `wfN_end_to_end.{h,cpp}` — orchestration（仅 wf1、wf4 有）
- `wfN_ink_limit.{h,cpp}` — 墨量上限分析（仅 wf1）
- `wfN_param.{h,cpp}` — session 参数序列化（仅 wf1）
- `wfN_profile.{h,cpp}` — Argyll ICC 生成（仅 wf4）
- `wfN_transform.{h,cpp}` — ICC LUT 应用（仅 wf4）
- `wfN_gamut.{h,cpp}` — 色域压缩（仅 wf6）

---

## 相关参考

- 官方产品页：
  - [Epson SureColor P9580](https://www.epson.com.cn/products/large-format/p9580/)
  - [Epson SureColor P7580](https://www.epson.com.cn/products/large-format/P7580/)
  - [SC-P7580/SC-P9580 用户指南 PDF](https://eposs.epson.com.cn/EPSON/assets/resource/Download/Service/manual/LFP/SC-P7580_P9580/SC-P7580SC-P9580_UG.pdf)
- 项目内文档：
  - [`apps/jcolour/PIPELINE.md`](../../apps/jcolour/PIPELINE.md) — 端到端色彩管线
  - [`apps/jcolour/BW-WORKFLOW.md`](../../apps/jcolour/BW-WORKFLOW.md) — B&W 双模式
  - [`docs/DECISIONS.md`](../../docs/DECISIONS.md) — ADR-26（PJL 直连）、ADR-17（光谱契约）
