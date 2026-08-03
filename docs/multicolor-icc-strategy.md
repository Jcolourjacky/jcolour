# 多色 / OGV 分离策略（buy vs build）

> **版本** 2026-06 · 初稿
> **关联** [`ip-strategy.md`](ip-strategy.md)（本文是「别人的专利/FTO」面；IP 策略的「我方申请/秘密/公开」面在那）、[`apps/jcolour/PIPELINE.md`](../apps/jcolour/PIPELINE.md) §6（已据此订正）、
> [`apps/jcolour/GAMUT-PIPELINE.md`](../apps/jcolour/GAMUT-PIPELINE.md) §4-§6（§8 大色域调用）
> **结论先行**：Argyll 出 **device-RGB** profile；**12 墨分离（淺墨/多黑/OGV）自管在 jcolour 引擎**；
> profiler **不自研**（买 ColorLogic 当兜底）；分离引擎走**公开色彩科学 + 自研**（避专利雷区）。

---

## 1. 为什么不是「12 通道 ICC」（实证）

| 验证 | 结果 |
|---|---|
| ArgyllCMS 官方文档 | colprof「**Monochrome and N-Color profiles are not supported**」，只出 RGB/CMY/CMYK |
| 本机 Argyll **3.3.0** 实测 | 喂 7 色 CMYKOGV `.ti3` → `unhandled color representation 'CMYKOGV_XYZ'`；`-d3` 非法旗标；`-k*` 仅 CMYK |
| 自研 N 色 profiler | = ColorLogic 二十年核心，几人年起跳，**不做** |

→ **ICC = device-RGB**，表征 `ProPhoto → jcolour 分离引擎(12墨) → 实打` 复合系统；
3→3 反演良置、无 N 色分离决策。**OGV / 淺墨 / 多黑分离全在 ICC 之下的 jcolour 引擎**。

---

## 2. 自管分离：参考竞争对手多少

分离拆两个子问题，竞品各管一块：

| 子问题 | 竞品参考 | 可参考度 | 形式 |
|---|---|---|---|
| **(A) 同色相多墨拆分**（淺墨 LC/LM、多黑 K/GY/LGY）| **QuadToneRIP** 分墨曲线法（商业免费，非开源；方法公开）| **~60–70%** | 仿方法（交叉点曲线），非代码 |
| **(B) OGV 扩展色域调用** | ColorLogic（墨限优先/GCR 控制面/DeviceLink 保纯色）、Esko Equinox（最少墨/干净分离）、GMG OpenColor（光谱前向/cellular）| **~20–30%** | 仅设计原则，算法闭源 |
| **§8 图像相关 OGV 调用** | 无（Esko/ColorLogic/QTR 都是图像无关）| **~0%** | jcolour 原创 |

**实质基础是公开色彩科学**（YNSN 前向 + 约束反演 + GCR），且 **jcolour 引擎已具备**：
[cynsn](../engine/src/cynsn.cpp)·[km](../engine/src/km.cpp)·[forward_factory](../engine/src/forward_factory.cpp)·
[sqp](../engine/src/sqp.cpp)·[jacobian](../engine/src/jacobian.cpp)·[cost](../engine/src/cost.cpp)·[constraints](../engine/src/constraints.cpp)·
[gamut_mapping](../engine/src/gamut_mapping.cpp)·[segment_mapper](../engine/src/segment_mapper.h)。
→ **走第一性原理，不抄竞品实现。**

### ⚠ IP 雷区 / FTO 扫描（2026-06，已逐条读权利要求）
> **非法律意见**，是工程风险地图。结论基于 Google Patents 的 claim 1 / 法律状态，商用前仍须专利律师正式 FTO（读全部 claims + 查 USPTO 维持费/审查历史 + 海外同族）。

**公开科学基础（可自由用）**：Neugebauer(1937) / Yule-Nielsen(1951) 前向模型 + ΔE 最小化反演 + SQP/Hooke-Jeeves 优化，数十年 prior-art。基础 n-color 分离 [US5440652](https://patents.google.com/patent/US5440652A/en)（1995）已过期。→ 这正是 jcolour 引擎（cynsn/km/sqp/black_split/ogv_meter）走的路。

**逐专利结论**：

| 专利 / 权利人 | 状态 | claim 1 覆盖 | 对 jcolour |
|---|---|---|---|
| **[US9373063](https://patents.google.com/patent/US9373063) · Onyx Graphics**（RIP 竞品）| **活跃 → 2032** | **同色相多墨拆分**（同 hue 的 light+dark 墨）：印色块→测光谱→密度→**PVR 曲线**→组合密度模型→分离曲线 | **🔴 最高相关** — 正覆盖 (A) 子问题域。**design-around**：jcolour `black_split` 用**参数化 QTR smoothstep 交叉 + KM/YNSN 前向**，不用 PVR 曲线、不用「实测组合密度模型」反推分离曲线 → 方法不同，**大概率落在 claim 外**，但须逐条核 |
| **[US8873102](https://patents.google.com/patent/US8873102B2) · HP** | **活跃 → 2033** | **图像相关/动态** N 色分离，但限定「**在数位印刷机上、依实际安装的墨、含印刷机分离参数自动构建**」 | **🟡 §8 相邻** — jcolour 是**离线按纸张标准建档 + 主机端 §8**，非「机上依装墨动态」→ 落在「at a digital press / installed inks」限定外，可区隔，仍 flag |
| **[US12034903](https://patents.google.com/patent/US12034903) · Fiery LLC**（EFI Fiery RIP 竞品）| **活跃 → 2043** | **语义分割 → 物件指派离散专色（Pantone/品牌色）** | **🟡 §8 相邻** — jcolour §8 做**记忆色分割→分区 intent + 连续 O/G/V【制程墨】调用**（非离散专色映射）→ 可区隔；§8 落地须避开「为物件指派专色」动作。详见 [`ip-strategy.md`](ip-strategy.md) §3.4 |
| **[US9649868](https://patents.google.com/patent/US9649868B2) · Sun Chemical** | 活跃 → 2033 | **油墨/主色相设计**（移 CMY 主色相 + 加墨），明确**不含** gamut mapping / 图像相关调用 | **🟢 无风险**（软体分离不沾油墨化学）|
| ~~US7084991 · HP~~ | **已过期 2024** | RIP 商业安排 + 2-bit pipeline + drop-table（非分离算法）| 🟢 过期且不相关（先前高估，**降级**）|

**Esko / Agfa 扫描（2026-06 补）—— 重要修正**：
- **Esko Equinox 的专利强项是「网屏/制版」不是「分离算法」**：那「~11 项」多为 FlexoCal / Hybrid Screening / Plate Cell Patterning / Concentric Screening / PressSync（半色调网屏 + 柔印制版）。jcolour 用自家 FM/error-diffusion 半色调（[halftone.cpp](../engine/src/halftone.cpp)），**与 Esko 网屏专利不同域 → 基本无关**。
- **真正的「多色分离算法」专利系谱是 Agfa-Gevaert（Marc Mahy 等）**，不是 Esko：

| 专利 / 权利人 | 状态 | claim 1 覆盖 | 对 jcolour |
|---|---|---|---|
| **[US8054504](https://patents.google.com/patent/US8054504B2/en) · Agfa→Eco3 BV**（Mahy/Verbeeck）| **活跃 → 2028** | 很**特定**的策略：**对立墨**（cyan↔orange）配对 + 两个部分 4 色子流程 + 重叠区「**封闭曲面**」+ 边界**最小用量**分离 | **🟡** 同域但策略不同——jcolour 走**全 N 墨 ΔE 最小化反演 + §8 图像相关**，不用「双 4 色子流程/封闭曲面/对立墨最小量」构造 → 大概率落 claim 外，须核 |
| [US7265870](https://patents.google.com/patent/US7265870B2/en) · Agfa（Mahy）| 待查状态（~2004 priority，可能将到期）| 「Colour separation method」 | 待核 |
| Mahy《Inversion of the Neugebauer Equations》论文 | **公开 prior-art** | 通用 Neugebauer 反演 | 🟢 jcolour 反演方法的理论源，**公开可用**（专利覆盖特定策略，非通用反演）|

**待核**：[US9767397](https://patents.google.com/patent/US9767397)、[US8711429](https://patents.google.com/patent/US8711429B2/en)、[US7265870](https://patents.google.com/patent/US7265870B2/en)、[US7164498](https://patents.google.com/patent/US7164498)（>4 色经 4 色 CM 匹配）—— 留给正式 FTO。

**要避开的具体方法**：「**限制每像素 ≤4 种墨 / 最少墨**」乾净分离规则有专利系谱（Esko/早期）。**改判**我此前建议借 Esko「最少墨」——若用，框成**通用优化的软正则（ink-count 惩罚为公知正则）**，不硬编复刻该规则。

**净结论**：真正活跃且相关的有 **3 项**，且各有 design-around：
1. **Onyx [US9373063](https://patents.google.com/patent/US9373063)**（→2032）覆盖 (A) 同色相拆分域 — jcolour 用 QTR smoothstep+KM 前向、非 PVR/密度模型 → 落 claim 外；
2. **HP [US8873102](https://patents.google.com/patent/US8873102B2)**（→2033）§8 域但限「机上依装墨动态」— jcolour 离线建档+主机端 → 区隔；
3. **Agfa [US8054504](https://patents.google.com/patent/US8054504B2/en)**（→2028）多色分离但限「对立墨/封闭曲面/双 4 色子流程」特定策略 — jcolour 全 N 墨 ΔE 反演 → 不同构造。

**Esko Equinox 专利＝网屏/制版，不沾 jcolour 分离**；油墨化学类（Sun Chemical）与已过期类（US7084991/US5440652）无软体风险。**jcolour 较安全**：① 底层公开 prior-art（Neugebauer/YN/Mahy 论文），② 不逆向任何商业实现，③ **§8 图像相关 OGV 调用是原创**（可能是 jcolour 自己可申请的空间）。**动作**：商用前对 US9373063 / US8873102 / US8054504 + 待核项做律师级 claim FTO。

---

## 3. Spike 数字（2026-06，引擎实测）

### (A) 同色相拆分 — `engine/tests/test_black_split.cpp`（31 断言过）
QTR 式三黑（K/GY/LGY）smoothstep 交叉曲线 → KM 前向（合成中性灰墨谱）预测 21 级灰阶：

| 指标 | 结果 |
|---|---|
| L\* 走向 | **单调 97.6 → 20.0**，无反转/凸起 |
| 三墨曲线 | **各单峰**，峰位序 LGY < GY < K（ownership 正确）|
| 总墨量 | 1.71（三中性墨，远在 TIL 内）|
| 步进 ΔE2000 | mean 3.07 / CV 86%（**线性化前**；§5 per-ink 线性化负责压成等步）|

→ 结构成立，**自研同色相拆分可行**。模块：[engine/calibration/black_split.{h,cpp}](../engine/calibration/black_split.cpp)。

### (B) OGV 调用 — `engine/tests/test_ogv_recruit.cpp`（11 断言过）
KM 前向（合成：OGV 比 CMY 二次色更纯）+ Hooke-Jeeves 多起点反演，目标 = 各纯色墨满色 Lab（该色相物理边界）：

| 色相 | targetC\* | CMYK-only ΔE | CMYKOGV ΔE | 缺口闭合 | recruit |
|---|---|---|---|---|---|
| Orange | 39 | **9.72** | **0.00** | 100% | Or=1.00 |
| Green  | 50 | **7.78** | **0.00** | 100% | Gr=1.00 |
| Violet | 82 | **5.41** | **0.00** | 100% | V =1.00 |
| Brown（in-gamut 控制）| — | 0.00 | 0.00 | — | 两者都中 |

→ CMYK 明确够不到的饱和橙/绿/紫（ΔE 5–10），**自研分离靠 recruit OGV 100% 追回**；in-gamut 控制不误用 OGV。**(B) 自研路线成立。**

> 注：以上墨谱为合成（无仪器实测），**绝对 ΔE 仅示意**；结论性质（CMYK 出色域、OGV 闭合、recruit 正确墨）是结构结果，与具体墨值无关。真实数字待 i1Pro3 实测 CMYKOGV 靶后用 `iccgamut`/`profcheck` 复算。

---

## 4. 求解器选型（实测教训）

引擎 [optimizer::minimize](../engine/src/sqp.cpp)（SQP+BFGS）在**平滑凸**问题完美（二次函数 conv✓），但对 **ΔE-of-KM 这类非凸目标不可靠**：
- KM predict 对重叠高吸收会出**负反射率** → `xyz_to_lab` 开负数立方根 → **NaN** → BFGS 炸；
- 即便 NaN-guard，仍易陷局部解（多起点也漂走）。

**动作项**：
1. 引擎 follow-up：给 `xyz_to_lab` / KM `predict` 加 NaN/范围保护（clamp 反射率到 (0,1]）。
2. 分离反演用 **Hooke-Jeeves 多起点**（derivative-free，box 约束天然满足，已验证命中全局解 ΔE 0.000）；SQP 留给平滑子问题。

---

## 5. buy vs build 决策与触发

| 组件 | 决策 | 触发改判 |
|---|---|---|
| 输出 profile（A2B/B2A, RGB）| **build**（Argyll colprof）| — |
| 12 墨分离（A 同色相 + B OGV）| **build**（自研，引擎已有积木）| §9 ΔE 验证证明自研分离达不到藝術微噴要求 → **buy ColorLogic CoPrA**（年费 $10K+）|
| N 色 profiler | **永不自研** | — |

---

## 6. 待办

- [x] 引擎 NaN-guard（§4 动作项 1）— 2026-06 已修：`KMForwardModel::ks_to_reflectance` clamp r≥0 + 反射率∈[0,1]（根因：负 K/S → sqrt(neg) → NaN）。回归 `test-km` 新增 "extreme overprints" 用例（含曾 NaN 的 `{0,0,1,0,1,1,0}`），1038 断言过；cynsn/forward-factory/color-pipeline 等全回归通过。
- [x] black_split 交叉点接曲线编辑器 UI — 2026-06 已加「分墨（K/GY/LGY）」预览页
- [x] OGV 调用接 §8 PCS 端 — 2026-06 已建 §8.5 调用率 meter [ogv_meter.{h,cpp}](../engine/src/ogv_meter.cpp)（图像相关；`test-ogv-meter` 16 断言过：vivid 橙/绿/紫 → Or82%/Gr79%/V81%，中性/暗橙 0%）。**剩**：接 UI 三个 meter（gamut_advisor_widget / 右面板）+ §8.4 CoreML 语义分割（Spike 5）
- [x] i1Pro3 measure→match 管线 — 2026-06 已建 [target_measure.{h,cpp}](../engine/calibration/target_measure.cpp)（parse CGATS .ti3 + join `.patches.tsv` 侧录 + OGV 角点→最近 CMY ΔEab 色域增益）；`test-target-measure` 合成 .ti3 验证：Or 19.9 / Gr 15.7 / V 22.8 ΔEab。**剩**：插真 i1Pro3 实测取代合成数字
- [x] FTO 专利初步扫描 — 见 §2「IP 雷区 / FTO 初步扫描」；**剩**：商用前专利律师正式 FTO
