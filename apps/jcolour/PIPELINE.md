# jcolour 端到端色彩管线（PIPELINE）

> **目标版本**：v1.5 完整规格（不再为 v1.0 折中保留 4 通道版）
> **定位**：从 12 通道墨量靶到打印验证的 single source of truth
> **文档版本**：2026-05 初稿
> **关联**：[`ADR-29`](../../docs/DECISIONS.md#adr-29) 架构决策；[`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md) §7-§9 深度规范；[`PLAN.md`](PLAN.md) 产品范围

---

## 流程一览（9 段连读）

> 校准期（一次性 / 月度复检）：§1 → §2 → §3 → §4 → §5 → §6
> 出片期（每张照片）：§7（含 §8 决策） → §4/§5 LUT 应用 → PJL 打印 → §9 验证

| § | 阶段 | 一句话 | 输出 |
|---|---|---|---|
| §1 | 12 通道墨量靶 | 用 PJL raw 在选定纸张上打 1,150 个 patches | 物理印张 |
| §2 | i1Pro3 测量 | 把印张转 36 通道光谱 | `.ti3` |
| §3 | TIL & 晕染阈值 | 推单通道上限 + 总墨量上限 + 邻通道交互 | `ink_limit.json` |
| §4 | Per-ink 线性化 | 12 道墨各自线性响应（设备地基，先于 G7） | `linearization.lut` |
| §5 | G7+ 灰平衡 | 在已线性化设备上让中性轴真中性（本地算） | `g7_curve.lut` |
| §6 | 云端 ICC 生成 | Argyll colprof 出 **device-RGB** profile（表征 ProPhoto→分离→实打 复合系统；非 12 通道 ICC——colprof 不支持 N 色） | `printer.icc` |
| §7 | ProPhoto → ICC → 分离 | LCMS2 转 device-RGB（16-bit 全程）→ jcolour 分离引擎出 12 通道 | 12 通道 raster |
| §8 | Or/Gr/V 大色域调用 | GamutAdvisor + 语义分割 + A/B 软打样 | 选定 intent + 局部 mask |
| §9 | 实打验证 | i1Pro3 测 ΔE2000，不通过自动定位回环 | 验证报告 |

---

## §0 概览

目标：让一张 ProPhoto 16-bit 文件通过 P9500 打印出，**充分用满 PRO 12 油墨族（CMYK + LC + LM + LK + LLK + Or + Gr + V + 双黑同上机）的色域**，每个环节损失定量化、可验证。

### 数据流总图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  §1 12 通道墨量靶 ──┐                                                        │
│                     ↓ targen + PJL raw                                       │
│                  P9500 打印                                                  │
│                     ↓ 印张                                                   │
│  §2 i1Pro3/i1iO 测量 → .ti3 (36 通道光谱)                                   │
│                     ↓                                                        │
│  §3 TIL & 晕染阈值分析 → ink_limit.json                                     │
│                     ↓                                                        │
│  §4 Per-ink 线性化（全 12 通道，设备地基） → linearization.lut             │
│                     ↓                                                        │
│  §5 G7+ 灰平衡（本地，建于已线性化设备） → g7_curve.lut                     │
│                     ↓                                                        │
│  §6 云端 Argyll colprof → printer.icc (device-RGB profile, 非 N 色)         │
│                                                                              │
│  ─── 校准期（一次性 / 月度复检） ────────────────────────────────────────  │
│  ─── 出片期（每张照片） ────────────────────────────────────────────────  │
│                                                                              │
│  ProPhoto 16-bit TIFF                                                        │
│       ↓                                                                      │
│  §7 ProPhoto → device-RGB ICC 转换（LCMS2 高质量 transform）                │
│       │   含 §8 Or/Gr/V 大色域调用层（GamutAdvisor + SemanticGamutMapper）  │
│       ↓                                                                      │
│  jcolour 分离引擎：device-RGB → 12 墨（淺墨拆分 + 多黑 GCR + OGV 调用）      │
│       ↓                                                                      │
│  设备侧 12 通道 16-bit raster                                                │
│       ↓ 应用 §4 §5 LUT + 半色调                                             │
│  PJL 16-bit raw 数据流（ADR-26）                                             │
│       ↓                                                                      │
│  P9500 打印                                                                  │
│       ↓ 印张                                                                 │
│  §9 i1Pro3 实测 ΔE2000 验证 → 通过 / 反馈环                                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 阶段责任分层

| 阶段 | 客户端 jcolour app | engine 共享库 | 云端 cloud/ |
|---|---|---|---|
| §1 测靶 | Qt UI 触发 | `engine/icc/targen_wrapper.h` | — |
| §2 测量 | UI 引导 | `engine/i1sdk/` + `engine/spectral/` | — |
| §3 TIL/晕染 | UI 显示 | `engine/icc/ink_limit.h` | — |
| §4 线性化 | 曲线编辑器 UI | `engine/pjl/per_ink.h` | — |
| §5 G7+ | UI 向导 | `engine/g7/` | — |
| §6 ICC 生成 | 上传/下载 UI | `engine/icc/profile_io.h` | Go API + Argyll Docker |
| §7 转换 | A/B 软打样 UI | `engine/icc/lcms_wrapper.h` | — |
| §8 大色域调用 | GamutAdvisor + 语义分割 | `engine/icc/intent_advisor.h`、`segment_mapper.h` | — |
| §9 验证 | 测量 UI | `engine/spectral/delta_e.h` | — |

---

## §1 12 通道墨量限制测试靶

### 目的
测出 P9500 在当前墨水批次 + 当前纸张下，每条墨道（CMY + LC + LM + LK + LLK + Or + Gr + V + PK + MK = 12 通道）单独以及与邻通道叠加时的**响应曲线 + 安全墨量上限**，为 §3 TIL、§4 线性化、§6 ICC 生成提供原始数据。

### 输入
- 当前墨水批次（cartridge IDs，从 PJL 状态读）
- 当前纸张：用户从纸张库选择（Hahnemühle Photo Rag / Canson Baryta / Epson Premium Luster …）
- 测靶规格：Argyll `targen -d 12` 生成 device 12 维空间上的均匀采样
  - 单通道 ramp：12 道 × 16 段（0%, 6.25%, …, 100%）= 192 patches
  - 双通道交互：选 28 对相邻通道（CMY×Or、Or×Gr、Gr×V、CMY 互混等）× 25 patches = 700 patches
  - 中性灰阶：256 段全级数 × 1 = 256 patches
  - 总计 ~1,150 patches，分 4–8 张 A3+ 印张（取决于 patch size）

### 输出
印在选定纸张上的测靶物理印张，附 `targen_<paper>_<batch>_<date>.ti1`（Argyll 格式靶位描述）。

### 关键约束
- **必须走 PJL raw 直送**（ADR-26）。绝不能走 Epson RGB driver——driver 内部会自己做 separation，导致测出来的不是单条墨道的真实响应，而是 driver 决策后的混合响应
- 12 通道 device 空间数据通路必须 16-bit
- 半色调算法在测靶阶段固定为 Stochastic（FM screening），保持与 §4 后续打印一致

### 失败模式
| 现象 | 根因 | 处理 |
|---|---|---|
| 印张某通道某段缺墨 / 断线 | 喷嘴堵 | 跳到 §1.5：Nozzle Check + Cleaning，重打 |
| 印张整体偏色（明显非中性） | PJL `COLORMODE=OFF` 未生效，driver 仍在管色 | Spike 2 红线，需要重新校验 PJL 命令族 |
| 同样的靶第二次打颜色明显不同 | 墨水批次切换 / 喷头老化 / 环境温湿度大变 | 触发月度复检流程 |

### 验证指标
- Nozzle Check 通过
- 印张目视无白斑、墨流、纸张起皱
- 用户可肉眼区分单通道 ramp 16 段（说明输出未被压扁到 8-bit）

### 归属
- ADR：ADR-04（i1Pro3）、ADR-26（PJL 直连）、ADR-17（光谱契约）
- Spike：Spike 2（PJL）、Spike 3（i1 SDK）

---

## §2 i1Pro3 / i1iO 扫描测量

### 目的
将 §1 的物理印张转为可计算的 36 通道光谱反射率数据（ADR-17）。

### 输入
- §1 印张
- i1Pro3 / i1Pro3 Plus（手持单点）或 i1iO（自动扫描台）
- `targen` 输出的 `.ti1` 靶位定义

### 输出
- `.ti3` 文件：每个 patch 的 36 通道光谱反射率（380–730nm @ 10nm）+ 派生 Lab/XYZ
- 副产品：`.cgats` 元数据（仪器型号、测量时间、几何条件 M0/M1/M2）

### 关键约束
- 必须 36 通道光谱，不能只取 Lab（ADR-17 项目宪法）
- 测量几何统一 **M1**（D50 + UV 包含）—与后续 colprof 假设一致；M0 / M2 仅作对照不入主链
- macOS App Sandbox 下需 USB / Bluetooth entitlements（Spike 3 验证）
- i1iO 自动扫描：单张 A3+ < 90s；i1Pro3 手扫：<5 min/张

### 测量流程
1. 校准 i1Pro3（白板 + 黑板）
2. 装载靶图 + 选 `.ti1`
3. i1iO 自动扫描所有 patches；i1Pro3 用户按行手扫
4. 异常 patch（光谱曲线突变 / Lab 值离群）自动标记，提示重测该行

### 失败模式
| 现象 | 处理 |
|---|---|
| i1Pro3 校准超时 | 重启 SDK，检查白板清洁度 |
| 单 patch 光谱曲线异常（如全平） | 标记 → UI 提示重测该 patch |
| 整列 Lab 漂移（白板污染或仪器温漂） | 重新校准 + 整列重测 |
| 沙盒 entitlement 拒绝 USB | Spike 3 红线，需要联系 Apple Developer Relations |

### 验证指标
- 灰阶 patch 列 Lab a*/b* 标准差 < 0.5（仪器一致性）
- 重复测同一 patch 3 次，光谱 RMS 偏差 < 0.5%
- 全部 patches 测量完成无 abort

### 归属
- ADR：ADR-04、ADR-17
- Spike：Spike 3（i1 SDK 沙盒）

---

## §3 TIL（总墨量限制）与晕染阈值

### 目的
从 §2 的光谱数据推导：
1. **每条墨道的安全单通道上限**（超过此墨量纸张吸不进，开始浮墨/反射不稳）
2. **TIL（Total Ink Limit）**：所有 12 通道下墨之和的物理上限
3. **晕染阈值**：相邻墨道叠加时何时出现 bleeding（点扩散异常增大）

这些参数喂给 §6 colprof 作为约束（colprof `-l <TIL>` 等参数）。

### 输入
- §2 的 `.ti3` 文件
- 当前纸张声明的吸墨等级（来自纸张库元数据）

### 输出
- `ink_limit.json`：
  ```json
  {
    "single_channel_max": {"C": 0.92, "M": 0.95, "Y": 1.0, ...12 道},
    "TIL_total": 3.4,
    "bleeding_threshold": {"CMY": 2.1, "Or+Y": 1.8, ...},
    "wet_ink_interaction": [...28 对相邻通道交互曲线]
  }
  ```

### 算法
1. **单通道上限检测**：单道 ramp 上 16 段 patches → 拟合密度 vs 墨量曲线 → 找拐点（密度增长率 < 5% 处即"饱和"）→ 取饱和点之前的安全工作上限
2. **晕染阈值检测**：相邻通道双 ramp patches → 测点扩散（patch 边缘扩散区域宽度 / 用 i1iO 高分辨率几何或图像分析）→ 阈值 = 扩散显著加速时的总墨量
3. **TIL 计算**：用线性规划或经验公式 `TIL = min(物理吸墨上限, ∑ 各道安全上限 × 0.85)`，因为多道叠加时纸张吸墨能力下降
4. **纸张联动**：纸张库元数据里有 base TIL，根据本次实测做 ±10% 修正

### 关键约束
- 全 12 通道分析（CMY + LC + LM + LK + LLK + Or + Gr + V + PK + MK，无简化）
- 算法运行在客户端 engine（不上云）：数据隐私 + 离线可用
- 用户可在 UI 上手动覆盖（高级模式），但默认不暴露

### 失败模式
| 现象 | 根因 | 处理 |
|---|---|---|
| 某通道找不到饱和拐点 | 16 段不够细 | 提示用户加测 32 段精细 ramp |
| TIL 算出 > 4.5（明显偏高） | 纸张元数据错 / 吸墨等级误判 | 拒绝结果，提示重选纸张 |
| 晕染阈值在某些通道对上 < 单通道上限和 | 强湿墨交互（如 V + Y） | 写入 wet_ink_interaction 表，§6 colprof 用作 sub-domain 约束 |

### 验证指标
- TIL 在合理区间（matte 类纸 2.5–3.0；luster/baryta 3.2–3.6；canvas ≤ 2.8）
- 晕染阈值检测出至少 5 对显著交互（PRO 12 油墨族特征）
- ink_limit.json 通过 schema 校验

### 归属
- ADR：ADR-17
- Spike：Spike 1（色彩管线 CLI 含此模块）

---

> **⚠ 建档顺序再订正（2026-11，ADR-31）**：单通道墨限 → §4 每通道线性化（墨限内）→ §3 总墨量 TIL（在已线性化设备上测）→ §5 G7 → §6 ICC。建档向导里墨限 + 线性化与 TIL 是固定步骤，引擎 `profiling_gate()` 不通过就不能进入 G7 / ICC。换纸 / 换墨批次后的「再校准」只更新 §4 曲线（ADR-33）。
>
> **⚠ 建档顺序订正（2026-06，v1.2）**：建档/测量顺序为 **§3 TIL → §4 Per-ink 线性化 → §5 G7+ 灰平衡 → §6 ICC**。
> **线性化必须先于 G7**：线性化逐通道把每条墨的密度响应锁成稳定线性，是设备地基；G7 是跨通道的高阶灰平衡校正，**必须建立在已线性化的通道上**——若先做 G7 再做线性化，后做的线性化会改变各通道响应、**破坏刚建好的灰平衡**。原 v1.1 把 G7 编为 §4、线性化编为 §5（顺序反了），现已对调，且与 jcolour 五模式模型（「线性」为基底、「+G7」为修正）一致。
> **注意区分两种顺序**：上为**建档顺序**；**列印套用顺序**则相反（图像 → ICC → 分离 → G7 曲线 → 线性化 LUT → 半色调），因为线性化曲线把「已线性化空间的请求」映回原始设备驱动，是套用时的最后一道设备侧校正——两者互为反向，均正确。

## §4 Per-ink 线性化（全 12 通道）

### 目的
让每条墨道的"输入墨量百分比 → 实际输出密度"成为线性关系，逐通道把设备响应锁成稳定线性。**这是 §5 G7 灰平衡与 §6 ICC 的设备地基**——必须先于 G7。

### 输入
- §2 的 12 通道单通道 ramp 数据（16 段或 v1.5 的 32 段精细版）
- §3 的 `ink_limit.json`（LUT 上限不超过单通道 max）

### 输出
- `linearization.lut`：12 张曲线（PK/MK/LK/LLK/C/M/Y/LC/LM/Or/Gr/V），每张 256 或 1024 段输入 → 输出墨量

### 算法
1. 对每条墨道的 16/32 段 patches 拟合密度（或 ΔE 距纸白）vs 墨量曲线
2. 反相得到"目标线性输出"对应的 device 墨量
3. 用户可在曲线编辑器 UI 上手动微调（25 控制点）

### 关键约束
- 全 12 通道独立线性化（PK/MK/LK/LLK/C/M/Y/LC/LM/Or/Gr/V），无 4 通道简化
- **建档先于 §5 G7**；列印**套用时**则在 G7 之后（见本节顶部「两种顺序」说明）
- 用户可在曲线编辑器 UI 上微调（25 控制点）

### 失败模式
| 现象 | 处理 |
|---|---|
| 某通道曲线非单调（中段反转） | 喷头老化或墨水批次问题 → 触发 nozzle clean + 重测 §1 |
| Or/Gr/V 三道在低墨量段 ΔE 噪声大 | 16 段不够 → 用 32 段精细 ramp 重测 |

### 验证指标
- 应用 LUT 后再测灰阶：256 段密度差与理想直线的 RMS < 1.5%
- 12 道 LUT 曲线全单调

### 归属
- ADR：ADR-26（PJL 实现层）、ADR-27（B&W 共用）、ADR-17
- Spike：Spike 1（色彩管线 CLI）、Spike 5（曲线编辑器 UI）

---

## §5 G7+ 灰平衡校正

### 目的
在 §4 已线性化的设备上，让中性轴输出**视觉中性**的灰阶（消除色偏）+ 满足 G7 HDS（Highlight / Mid / Shadow）色调一致性规范。这是 ICC 生成的前置必要步骤（ADR-09 强耦合）。

### 输入
- §2 的 `.ti3` 中性灰阶 256 段数据（在 §4 线性化后的设备上测得）
- §4 的 `linearization.lut`（G7 建立在已线性化的通道之上）
- §3 的 `ink_limit.json`（限制 G7 曲线生成时不超过墨量上限）

### 输出
- `g7_curve.lut`：从 input gray 0–100% 到 device 灰阶（PK/MK/LK/LLK 多通道分配）的 LUT，附 a*/b* ≈ (0,0) 约束

### 算法
1. 拟合中性灰阶 256 段的 L*a*b* → 检测偏色方向（a*/b* drift）
2. 计算反向补偿曲线：在哪些灰度段需要加多少 C/M/Y 补色
3. HDS 一致性：保证 highlight (5–25%)、mid (40–60%)、shadow (75–95%) 三段的 a*/b* 都收敛到中性轴 ±0.5
4. 生成 G7 曲线；列印**套用顺序**：用户图像灰度 → G7 曲线 → §4 线性化 → 多通道 separation → 半色调

### 关键约束
- **本地计算**（ADR-08）—不上云
- **建档时在 §4 线性化之后**（灰平衡须建立在已线性化的通道上）；**必须先于 ICC 生成**（ADR-09 强耦合）；G7 不绿，ICC 不出
- 12 通道下 G7 不仅修 PK/MK/LK/LLK，还利用 Or/Gr/V 在阴影 / 高光段做超精细中性补偿

### 失败模式
| 现象 | 处理 |
|---|---|
| 灰阶 a*/b* 全段超过 ±2.0 | 警告：纸张/墨水极不匹配，建议换纸 |
| HDS 三段无法同时收敛 ±0.5 | 触发 v1.5 灰阶曲线编辑器（25 控制点）人工干预 |

### 验证指标（ADR-11 最小实用）
- HDS 三段 |Δa*| ≤ 0.5、|Δb*| ≤ 0.5
- 全段灰阶平均 |Δa*|+|Δb*| ≤ 1.5
- 与目标 ISO 12647 G7 NPDC 曲线偏差 < ±5%

### 归属
- ADR：ADR-08（本地）、ADR-09（强耦合）、ADR-11（验证报告）

---

## §6 输出 ICC Profile 生成（云端 · device-RGB）

> **⚠ 2026-06 订正（取代原「12 通道 ICC」设计）**
> 原稿写「Argyll colprof 出 12 通道 device profile（`-d3 -kp`）」——**做不到**。已连网核实 + 本机 Argyll 3.3.0 实证：
> - 官方 colprof 文档明文「**Monochrome and N-Color profiles are not supported**」，只输出 **RGB / CMY / CMYK**；`-d3` 不是合法 colprof 旗标；`-k*` 黑生成仅 CMYK。
> - 喂 7 色 CMYKOGV `.ti3` → 报 `unhandled color representation 'CMYKOGV_XYZ'`。
> - 自研 N 色 profiler = ColorLogic 二十年核心，几人年起跳，不做。
>
> **新模型**：ICC 是**复合系统的 device-RGB 输出 profile**——它表征的「设备」是
> `ProPhoto → jcolour 分离引擎(12墨) → 实打`这整条链。**12 墨分离（淺墨拆分 + 多黑 GCR + OGV 调用）全部在 jcolour 引擎里，不在 ICC 里**；ICC 只做 3→3 的 RGB↔Lab，B2A 反演良置、无 N 色分离决策。OGV 的「按图调用」放 §8（PCS 端），分离引擎本身是固定的满色域渲染器。详见 [`docs/multicolor-icc-strategy.md`](../../docs/multicolor-icc-strategy.md)（待建）与 ColorLogic/Esko 对比结论。

### 目的
生成 **device-RGB → PCS（Lab）** 的双向 ICC profile，表征「ProPhoto → jcolour 分离 → 实打」复合系统的色彩响应。这份 profile 是 §7 转换的目标，也是 §8 软打样 / 色域警告的色域边界来源。

### 输入
- **复合系统 RGB 特征化靶的 `.ti3`**：把一组 device-RGB 色块**经 jcolour 分离引擎**渲成 12 墨 → PJL 实打 → i1Pro3 测 36 通道光谱 → Lab。
  （注意：这与 §1 的「12 通道**原始**墨量靶」是两张不同的靶——§1 量原始色道响应供 §3 TIL / §4 线性化 / 色域边界；§6 量的是分离后的复合 RGB 设备。）
- §3 `ink_limit.json`、§4 `linearization.lut`、§5 `g7_curve.lut` **已经作用在分离引擎里**，因此已"烤"进 §6 靶的实打结果——**不作为 colprof 旗标传入**（RGB profile 没有 TIL / 黑生成参数）。

### 输出
- `printer.icc`：v4 RGB cLUT profile，device-RGB → PCS Lab
  - A2B0/1/2/3：perceptual / relative / saturation / absolute 四张表（colprof 算）
  - B2A 反向表：RGB↔Lab 的 3→3 反演（**不含 N 色分离决策——那一层在 jcolour 引擎**）

### 流程（ADR-07，云端 Argyll）
```
客户端
  ├─ 收集复合系统 RGB 靶的 .ti3（分离引擎已应用 TIL/G7/lin）
  ├─ 打包上传到 cloud/ Go API（HTTPS + 用户认证）
  ↓
云端
  ├─ 落地 R2（用户隔离桶）
  ├─ spawn Docker 容器（debian:slim + ArgyllCMS）
  ├─ 容器内：colprof -v -qh <base>
  │   - colprof 由 .ti3 的 COLOR_REP 自动判定为 RGB（无 -d / -k / -l 通道旗标）
  │   - -qh: high quality cLUT
  │   - PCS = Lab（cLUT profile 默认）
  ├─ 输出 printer.icc + 元数据（生成参数、测量时间、墨水批次）
  ├─ 上传 R2 → 生成签名 URL
  ↓
客户端
  └─ 下载 ICC + 落地到本地 profile 库 + 关联 paper × printer × ink batch
```

### 关键约束
- **GPL 隔离**（ADR-07）：Argyll 是子进程；jcolour 客户端不链接 Argyll，符合 SaaS loophole
- 端到端 < 30s（Spike 4 验证）；RGB profile 比设想的 12 通道轻得多，内存/CPU 不再是瓶颈
- 单次成本 < $0.05（R2 + 容器 spot price）
- **色域边界正确性取决于 §6 靶必须经分离引擎实打**（量裸 RGB 会丢 OGV——见失败模式②）

### 失败模式
| 现象 | 根因 | 处理 |
|---|---|---|
| 想直接建 7/12 色 ICC | colprof 不支持 N 色 | 走 device-RGB 复合 profile（本节）；真要 N 色分离 → 买 ColorLogic CoPrA |
| profile 色域只到 CMYK、丢 OGV | §6 靶对**裸 RGB**测、分离没 recruit OGV | 靶必须经分离引擎（分离在边界会用 OGV）后再测 |
| device-RGB 被当 sRGB/AdobeRGB clamp | 中间层用了标准 RGB 语义 | 用满色域线性 device-RGB，不 clamp |
| 生成的 ICC ΔE 验证不过 | §3-§5 输入或分离不一致 | 检查分离引擎与 §3-§5 一致性 |
| 网络中断 | — | 离线缓存 .ti3，重试上传 |
| GPL 法律审查不过 | — | 备选：买商业 Profile SDK（ColorLogic CoPrA，年费 $10K+） |

### 验证指标
- B2A 正反一致性：随机 100 个 Lab(色域内) → RGB → 反算 Lab，ΔE2000 平均 < 1.5
- 测试图实测：印 IT8.7/4 → i1Pro3 测 → ΔE2000 平均 < 3、95th < 5
- 色域体积：device-RGB profile 的 gamut 必须 ≥ CMYK-only（含 OGV 扩展）——否则靶没经分离，回退失败模式②
- 与 Argyll 本地命令行同参数生成的 ICC 字节级对照（云端不引入额外偏差）

### 归属
- ADR：ADR-07（云端）、ADR-09（G7→ICC 强耦合）、ADR-17；**订正依据见 Argyll 多色能力 spike（2026-06）**
- Spike：Spike 4（云端 API）

---

## §7 ProPhoto → device-RGB ICC → 分离 12 通道

### 目的
把摄影师交付的 ProPhoto RGB 16-bit 文件，**最小损失**地经 §6 的 **device-RGB** profile 映射，再由 jcolour 分离引擎渲成 12 墨 device 数据。这是出片环节的核心 transform。（注：ICC 转出的是 device-RGB，不是 12 通道；12 墨分离在 ICC 之后的 jcolour 引擎层。）

详细执行级规范见 [`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md) §1-§3。

### 输入
- 用户图像：ProPhoto RGB 16-bit TIFF / PSD（嵌入 ICC profile）
- §6 的 `printer.icc`（device-RGB → Lab）
- §8 GamutAdvisor / SemanticGamutMapper 的决策（rendering intent + 局部 mask）

### 输出
- device-RGB 16-bit（LCMS2 transform 产物）→ 经 jcolour 分离引擎 → 12 通道 16-bit device raster（待应用 §4 §5 LUT 后送 PJL）

### 6 决策点

#### 7.1 源就绪检测（前置闸门）
| 检查项 | 正确状态 | 错误处理 |
|---|---|---|
| 嵌入 profile = ProPhoto RGB | ✅ | 拒绝；提示用户在 LR/PS 转 ProPhoto 重出 |
| Bit depth = 16 | ✅ | 拒绝；8-bit ProPhoto 必然 banding |
| 无 alpha 预乘 | ✅ | 警告 + 反预乘 |
| 数值范围 0–65535 无 hard clip | ✅ | 警告：高光/暗部已裁剪 |

#### 7.2 CMM = LCMS2 + 关键 flag
```c
cmsHTRANSFORM xform = cmsCreateTransform(
    src_prophoto, TYPE_RGB_16,
    dst_printer_12ch, TYPE_NAMED_COLOR_INDEX,  // 12-ch device
    intent,                                     // 来自 §8 决策
    cmsFLAGS_HIGHRESPRECALC |                   // 高分辨率预计算 LUT
    cmsFLAGS_BLACKPOINTCOMPENSATION |           // BPC 默认 ON
    cmsFLAGS_COPY_ALPHA                         // 保 alpha
);
```

注意：默认 `cmsCreateTransform` 不开 BPC、不预计算高分辨率 LUT —— 这是常见坑。

#### 7.3 渲染意图（来自 §8）
不在此处选；由 §8 GamutAdvisor 全局推荐 + SemanticGamutMapper 分区覆写。本环节只是 LCMS2 的输入参数。

#### 7.4 BPC 默认 ON
源 (0,0,0) → printer 最深黑（L*≈3–8），暗部线性映射保留细节。

#### 7.5 LUT 插值
LCMS2 默认 Tetrahedral（ICC v4 标准）。不动。

#### 7.6 16-bit 全程贯通
- 文件 IO 16-bit（libtiff `BitsPerSample=16`）
- LCMS2 transform 16-bit
- 12 通道 device 输出 16-bit
- §4 §5 LUT 应用 16-bit
- 半色调输出：Stochastic 1-bit 点阵 / 但用 16-bit 输入决定密度
- PJL raw 16-bit（Spike 2 验证不被 macOS 降到 8-bit）

### 性能
- 24×36" @ 360dpi 12 通道 transform < 30 秒（M2 Pro 基准，Spike 1 验收）
- 大图（44×60" @ 360dpi）< 90 秒

### 失败模式
| 现象 | 处理 |
|---|---|
| LCMS2 transform 偏色（不应该出现） | 检查 BPC flag、profile PCS 类型（应都为 Lab） |
| 输出某通道异常高 / 低 | 检查 ICC B2A 表完整性，可能 §6 生成不全 |
| 性能崩坏（>5 分钟） | 退化到 16³ LUT；监控原因 |

### 归属
- ADR：ADR-07、ADR-13（LCMS2）、ADR-17、ADR-26
- Spike：Spike 1

---

## §8 Or/Gr/V 大色域调用层

### 目的
单一 ICC + 全局 intent 无法**充分**用满 PRO 12 的 Or/Gr/V 扩展色域，因为：
1. ICC 的 4 张 intent 表是"对平均图像"的最优，不针对特定图像内容
2. 全局直方图无法区分主体（脸 / 蓝天 / 记忆色）与背景
3. 摄影师对鲜艳色 vs 自然色的偏好高度个性化

§8 解决以上三点，分四层：

> **L1**（基础）：高质量 ICC + 全局固定 intent —— 由 §6 + §7 已交付
> **L2**：GamutAdvisor —— 直方图分析 → intent 候选
> **L3**：A/B 软打样 —— 用户在两个候选中拍板
> **L4**：SemanticGamutMapper —— CoreML 语义分割 + 分区 intent + 局部强度

详细见 [`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md) §4-§6。架构决策见 [`ADR-29`](../../docs/DECISIONS.md#adr-29)。

### §8.1 入口色域闸门
- 检测源 profile：必须 ProPhoto；AdobeRGB → 黄色警告"15% 黄绿区被截"；sRGB → 红色警告"30% 色域已损失，建议换源文件"
- 检测 bit depth：必须 16-bit
- 检测显示器 ICC：屏幕色域 < Display P3 → 软打样按钮禁用 + 提示

### §8.2 GamutAdvisor (L2)
分析 ProPhoto 图像 →
1. 饱和度直方图（C* in Lab）
2. 出色域像素比例（vs printer.icc 色域 boundary）
3. 记忆色掩码：肤色（基于 ITU-R BT.601 范围）/ 蓝天 / 绿植占比
4. 输出：`[(Perceptual, conf_a), (Relative+BPC, conf_b)]` 两个候选 + 推理依据文本

### §8.3 A/B 软打样 (L3)
- 屏幕同时渲染两版（Photoshop View → Proof Setup 同等效果）
- 含 Paper White Simulation（默认 ON 并可视化标注）
- 用户拍板 → 该决策写入图像元数据
- 高级模式：用户可第三个选项"自定义 intent + 强度"

### §8.4 SemanticGamutMapper (L4)
- 用 CoreML 模型（选型在 Spike 时确定，候选：DeepLab v3+ / MobileNet-SAM）做语义分割
- 分类：肤色 / 天空 / 植物 / 主体物体 / 背景 / 其他
- 每段独立 intent + 强度系数（0–1，0=纯 perceptual / 1=纯 relative）
- 失败降级：分割置信度 < 0.7 → 退回 L2/L3 全局 intent

### §8.5 Or/Gr/V 调用率 meter
- 转换完成后实时统计：Or 通道平均下墨百分比、Gr、V
- UI 显示三个 meter（百分比 + sparkline 历史）
- < 2% → 提示"扩展色基本未用，是否重选 intent？"
- > 30% → "充分调用"提示

### 关键约束
- **不在 ICC 层做语义映射**：ICC 保持纯净 device profile 角色；语义映射在 ICC transform **之前**（在 ProPhoto 空间内做局部预调整 + 选 intent）
- **不重写 perceptual table**：直接用 Argyll colprof 默认 perceptual 表（v2.0 之前不研发自有 PCS 映射）
- **A/B 软打样是合同**：jcolour 永不替用户拍板，只缩小到 2 个候选

### 失败模式
| 现象 | 处理 |
|---|---|
| GamutAdvisor 把暗调人像推荐 Saturation | 加入肤色优先级覆盖规则 |
| CoreML 把皮肤识别成衣物 | 用户可手动调整 mask；置信度低自动降级到 L2 |
| 屏幕色域不够，软打样不可信 | 强制禁用软打样按钮 + 显示"建议 Display P3 屏" |

### 验证指标
- L2 推荐准确率（盲测：种子摄影师选哪个 vs GamutAdvisor 推哪个）≥ 70%
- L4 语义分割主体识别率（人脸 + 蓝天）≥ 90%
- Or/Gr/V 调用率 meter：饱和图（夕阳/花卉）平均 > 15%；自然图平均 5–15%

### 归属
- ADR：ADR-29（架构决策）、ADR-17、ADR-18（专色影响范围）
- Spike：Spike 1（GamutAdvisor 算法验证）、Spike 5（A/B 软打样 UI）

---

## §9 实打验证 + 反馈环

### 目的
用 i1Pro3 测印张关键色 ΔE2000，闭环验证整条管线。不通过 → 自动定位到哪个阶段重做。

### 输入
- 印张（§7 + §4 + §5 + PJL → 实物）
- 测试目标图：除了用户原图，可选打"验证靶"（含 ColorChecker 24 + 灰阶 256 段 + 肤色 ramp + 蓝天 / 绿植 ramp）

### 输出
- 验证报告：
  - 各色块 ΔE2000
  - 灰阶曲线偏差
  - Or/Gr/V 实际下墨 vs §7 计算预期
  - 通过 / 不通过 + 失败定位

### 通过标准（ADR-11 最小实用 + Spike 1 红线）
| 类别 | ΔE2000 阈值 |
|---|---|
| 关键色（肤色 / 灰阶 / 记忆色） | < 2 |
| 平均 | < 3 |
| 最大（出色域饱和色） | < 6 |
| Banding / 块状失色 | 无 |
| 暗部细节（0–10 L\*） | 可分 5 档 |
| 高光过渡（90–100 L\*） | 可分 5 档 |

### 不通过的反馈环路由
| 失败现象 | 路由到 |
|---|---|
| 灰阶 a*/b* 偏移 | §5 G7+ 重做 |
| 单通道密度不线性 | §4 线性化重做 |
| 整体 ΔE 大、profile 偏 | §6 ICC 重做（含 §3 TIL 检查） |
| Or/Gr/V 调用率低于预期 | §8 intent 重选 / 重测 §1 12 通道靶覆盖度 |
| 同色异谱（光源切换变色） | v1.5 Multi-spectral metamerism 模块（D50/D65/A/F11 切换软打样） |

### 验证指标
- 整条管线一次跑通（§1 → §9 全绿）的种子用户比例 ≥ 80%
- 失败时自动定位准确率 ≥ 90%（不需要用户手动找哪环出错）

### 归属
- ADR：ADR-11、ADR-17、ADR-23（测试打印工作流）
- Spike：Spike 1

---

## §10 范围与边界

### 已包含
- 全 12 通道墨量靶 + 测量
- 12 通道 TIL + 晕染阈值
- 全 12 通道 Per-ink 线性化（设备地基，先于 G7）
- 全 12 通道 G7+ 灰平衡（建于已线性化设备；含 Or/Gr/V 阴影/高光精修）
- device-RGB 输出 ICC 生成（云端 Argyll colprof -qh；非 N 色 ICC——见 §6 订正）
- 12 墨分离引擎（淺墨拆分 + 多黑 GCR + OGV 调用）在 jcolour 客户端，ICC 之下
- ProPhoto 16-bit → device-RGB ICC 转换（LCMS2 高质量参数）
- L2 GamutAdvisor + L3 A/B 软打样 + L4 SemanticGamutMapper
- Or/Gr/V 调用率 meter
- 同色异谱多光源软打样切换（D50/D65/A/F11）

### 不包含（明确边界）
- ❌ Mutoh / Canon / HP 打印机（ADR-28）
- ❌ CMYK 转换 / GRACoL / Fogra（gcolour 范围，ADR-24）
- ❌ Pantone 专色（v2.0 + 授权）
- ❌ IDGM 图像依赖 gamut mapping（v2.0+ 研究）
- ❌ Kubelka-Munk 直接合成（gcolour，ADR-15 / ADR-17）
- ❌ Windows 版（ADR-03）

---

## §11 关联文档与决策

| 文档 / 决策 | 引用关系 |
|---|---|
| [`PLAN.md`](PLAN.md) §3 | 拆"广色域映射引擎"为 GamutAdvisor + SemanticGamutMapper |
| [`ROADMAP.md`](ROADMAP.md) | §8 全部组件均在 EA / v1.0 / v1.5 节点对应 |
| [`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md) §10 | 痛点知识库；本文档 §7-§8 落地为执行规范 |
| [`BW-WORKFLOW.md`](BW-WORKFLOW.md) | §4 线性化 + §5 G7+ 共用；B&W 是本管线的特殊变体（§7 跳过 RGB intent，直接走灰度→多通道分配） |
| [`COMPETITORS.md`](COMPETITORS.md) | jcolour 与 ImagePrint / Mirage / QTR 在 §8 Or/Gr/V 调用率与 SemanticGamutMapper 上的差异化 |
| [`SPIKES.md`](SPIKES.md) | §1 §4 §7 → Spike 1；§1 §4 §6 → Spike 2；§2 → Spike 3；§6 → Spike 4；§8 → Spike 5 |
| [`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md) | §7 §8 §9 的执行级深度版（参数、伪代码、测试用例） |
| [`../../docs/DECISIONS.md`](../../docs/DECISIONS.md) ADR-04/07/08/09/11/13/17/22/23/26/27/29 | 各阶段架构决策 |

---

## §12 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-05 | v1.0 | 初稿，端到端 v1.5 规格 |
| 2026-06 | v1.1 | **订正 §6**：Argyll colprof 不支持 N 色 ICC（已连网+本机 3.3.0 实证），改为 **device-RGB 复合系统 profile**；12 墨分离（淺墨/多黑/OGV）移到 jcolour 引擎、ICC 之下；OGV 按图调用归 §8。同步改 §0 表/数据流图/§5 顺序/§7/§10。 |
| 2026-06 | v1.2 | **订正建档顺序**：§4↔§5 对调为 **§4 Per-ink 线性化 → §5 G7+ 灰平衡**（线性化是设备地基、必须先于 G7；G7 须建于已线性化通道，否则后做线性化破坏灰平衡）。原 v1.1 顺序反了且与五模式模型「线性 + G7」矛盾。明确区分建档顺序（线性化→G7）与列印套用顺序（G7→线性化，互为反向）。同步改 §0 表/数据流图/责任表/§1/§6/§9/§10/关联文档。A2 专利同步订正。 |
