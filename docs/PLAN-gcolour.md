# gcolour PLAN — CMYK 打样专用版

> **版本**：v1.0（2026-05 初稿）
> **状态**：设计阶段
> **共享地基规划**：见 [`PLAN-shared.md`](PLAN-shared.md)
> **决策记录**：见 [`DECISIONS.md`](DECISIONS.md)（ADR-24 定义 jcolour/gcolour 拆分）
> **管线技术规格**：见 [`PIPELINE-gcolour.md`](PIPELINE-gcolour.md)
> **jcolour 管线参考**：见 [`../apps/jcolour/PIPELINE.md`](../apps/jcolour/PIPELINE.md)

---

## 0. 一句话定位

> **gcolour 是 macOS 原生的数码打样色彩管理工作站，面向打样工作室和品牌色彩部门。
> 双路线引擎（ICC 标准 + CYNSN 光谱）超越 GMG ColorProof 的纯 Lab/ICC 局限，
> 解决多专色叠印、非纸基承印物、同色异谱三大行业痛点。
> 全链路满足 ISO 12647-7:2016 Contract Proof 标准。**

---

## 1. 目标用户

**主要用户**：

| 角色 | 场景 | 痛点 |
|------|------|------|
| 打样工作室色彩技师 | 每天 50-200 张 contract proof | GMG 贵（$15K+）、不支持光谱级多专色 |
| 品牌色彩部门 | Pantone 专色打样 + 品牌色一致性 | 多专色叠印不准、ICC 处理不了 N 通道 |
| 包装印刷打样师 | 软包装 / 印铁 / PVC 等非纸基 | ICC 假设漫反射纸基，非纸基偏色严重 |
| 独立打样服务商 | 多客户多标准（FOGRA/GRACoL/客户自定义） | 每个客户一套条件，管理混乱 |

**明确排除**（属于 jcolour 或不做）：
- ❌ 摄影师个人打印（→ jcolour）
- ❌ 工业喷绘（车贴/展板/纺织 → ONYX/Caldera）
- ❌ 凹印/柔印制版（→ EskoArtwork/ESKO）
- ❌ 非 Epson 打印机 v1.0（v1.5 评估 Canon/HP）

---

## 2. 硬件支持

### v1.0
| 类别 | 型号 |
|------|------|
| 打印机 | Epson SureColor P7500 / P9500（UltraChrome Pro12, 12 通道） |
| 测量仪 | X-Rite i1Pro3 / i1Pro3 Plus / i1iO（36 通道光谱） |
| 测量条件 | M0 / M1 / M2（推荐 M1，OBA 纸必须 M1） |

### v1.5
- Epson SC-P6500 / P8500 / P10000 / P20000
- Canon imagePROGRAF PRO 系列
- Barbieri Spectro LFP / iSis 2 自动扫描（扩展测量仪支持）

---

## 3. 竞品对标：GMG ColorProof

### GMG 的核心流程（10 步）

从 GMG ColorProof 实操流程（PDF）提炼：

```
1. 准备（喷头检查 + Starterkit 校准包）
2. 添加打印机（型号 + 驱动 + 打样标准）
3. 添加自定义介质（纸张 × 尺寸 × 打印机绑定）
4. 制作最大色域 FullGamut（打印 TC4 → i1iO 扫描 → .csc）
5. 制作压缩色域 ReduceGamut（打印 ECI2002 → 扫描 → .csc）
6. 制作打印机校准集 MX4（打印 TC3K → 扫描 → .mx4）
7. 添加校准基（绑定四级：打印机 + 介质 + 驱动 + 标准）
8. 循环校正（日常：打印控制条 → 测量 → 补偿曲线）
9. 绑定打样标准（FOGRA51/GRACoL2013 → 多次迭代收敛）
10. 出样 + 验证
```

### GMG 值得采纳的设计

| 设计模式 | gcolour 对应 |
|---------|-------------|
| 四级数据库绑定（打印机→介质→校准集→标准） | PrintCondition 模型 |
| Starterkit 预置包 | 内置已知纸张/打印机的基础 profile |
| 三张靶图分离（TC4/ECI2002/TC3K） | 采纳，但 Route B 用 CYNSN 专用靶 |
| 循环校正（日常轻校准） | A-Fix-6 |
| 质量准则（ΔE 容差 + 有效期） | ISO 12647-7 + 可配置阈值 |

### gcolour 超越 GMG 的点

| GMG 局限 | gcolour 方案 |
|---------|-------------|
| 纯 Lab/ICC 内部 → 同色异谱无法控制 | ADR-17 全程光谱 + Route B |
| 专色叠印靠 DeviceLink 近似 → 多专色不准 | CYNSN 预计算 33³ LUT + 局部精化（A2@720dpi ≤ 2-3 分钟） |
| 不支持非纸基承印物光谱建模 | Kubelka-Munk 层叠 + 后衬补偿 |
| 校准循环只做整体 1D 补偿 → 不能定位单通道 | 光谱级单通道漂移诊断 |
| Windows only | macOS 原生 + Apple Silicon 优化 |

---

## 4. 双路线架构

### Route A — ICC 标准模式

对标 GMG ColorProof，适用于：
- 常规 CMYK 打样（FOGRA51 / GRACoL2013）
- 单专色作业（无叠印）
- 客户只认 ICC 的场景

**6 处修正**（对比原始方案）：

| 修正 | 问题 | 解决 |
|------|------|------|
| A-Fix-1 | 缺 TAC 控制 | 增加 multi-channel TAC test + GCR 参数 |
| A-Fix-2 | 只做 per-channel TVI | 用 G7+ NPDC 替代传统线性化 |
| A-Fix-3 | 每通道独立线性化 | 浅墨+主色墨联合处理（C+Lc, M+Lm, K+Lk+Llk） |
| A-Fix-4 | 只做专色单循环追色 | 混合路由：多专色叠印强制走 Route B |
| A-Fix-5 | 无 ISO 12647-7 验证 | 每张打样含控制条 + 自动验收判定 |
| A-Fix-6 | 无循环校正 | 借鉴 GMG：轻校/重校两级 + 质量准则 |

### Route B — 光谱高精模式

gcolour 核心差异化，适用于：
- 多专色叠印（≥2 个专色互相叠加）
- 非纸基承印物（PVC / 金属 / 半透明薄膜）
- 要求多光源一致（D50 + D65 + A）
- 高精度需求（ΔE00 平均 < 1.0）

**4 处修正**：

| 修正 | 问题 | 解决 |
|------|------|------|
| B-Fix-1 | PDF 内嵌 ICC 如何转光谱 | 同源回放 + spectral DB 插值 + GS 输出 Lab raster |
| B-Fix-2 | G7 灰平衡如何处理 | 内化到反求解目标函数（不做前处理 NPDC 曲线） |
| B-Fix-3 | 专色叠印工程方案 | 按通道数分级（Full/Cellular CYNSN） |
| B-Fix-4 | 逐像素 SQP 不可行 | 建档期预计算 33³ LUT，出样期三线性插值 + 1-3 iter 局部精化（A2@720dpi <3 分钟） |

### 混合路由（Hybrid Router）

per-pixel 决策逻辑：

```
if pixel 仅含 CMYK（无专色）        → Route A: ICC DeviceLink
elif pixel 含 1 专色（无叠印）       → Route A: 专色 1D 曲线匹配
elif pixel 含 多专色叠印             → Route B: CYNSN LUT 查表 + 局部精化
elif 用户强制全光谱模式              → Route B: 全像素走光谱
elif 非纸基承印物                    → Route B: KM 层叠模型
```

---

## 5. PDF 引擎

**决策**：**Ghostscript 商业授权**（Artifex Commercial License）。

| 方案 | 选定 | 理由 |
|------|------|------|
| Ghostscript AGPL | ❌ | 客户端嵌入有 AGPL 传染风险 |
| **Ghostscript 商业授权** | **✅** | ~$5K-20K/年，本地嵌入无 GPL 问题，PDF/X-4 支持成熟 |
| Poppler + 自研分色 | ❌ | 工期太长，PDF 兼容性风险 |
| 云端渲染 | ❌ | 必须联网，不适合打样工作室 |

### 打印机连接：TCP RAW 9100

**决策**：通过 TCP socket 连接打印机 IP:9100（RAW 9100 协议），不使用 CUPS。

| 方案 | 选定 | 理由 |
|------|------|------|
| CUPS raw print | ❌ | macOS App Sandbox 中 `com.apple.security.print` 为受限授权，Apple 不向第三方开放 |
| USB 直连 | ❌ | 同上，需要受限授权 |
| **TCP RAW 9100** | **✅** | 仅需 `com.apple.security.network.client`（标准网络权限），MAS 可申请；GMG/EFI 均走此路径 |

打印机发现：Bonjour mDNS 自动扫描（主要方式，动态解析 IP 防 DHCP 漂移）；手动输入固定 IP（备用，参照 EFI Fiery/GMG 均走 TCP RAW 9100 + Bonjour 路径）。

Ghostscript 配置要点：
- 透明展平在 CMYK 空间（不经 RGB 中转）
- DeviceN 专色通道保留不转
- 叠印按 PDF 指令处理（OPM=1）
- `-dRenderingIntent` 按 PDF 内 OutputIntent 指定
- Route B 时输出 Lab raster + 专色分版

---

## 6. ISO 12647-7:2016 合规

### 每张打样必须包含

- **控制条**：UGRA/FOGRA Media Wedge V3 或 IDEAlliance ISO 12647-7 Control Wedge 2013
- **测量条件**：M1 illumination（D50 + UV 包含），D50/2° observer
- **白底衬**：标准白色衬底

### Contract Proof 验收阈值

| 指标 | 阈值 |
|------|------|
| Substrate ΔE00 | ≤ 3 |
| Substrate ΔH00 | ≤ 1.5 |
| CMYK primaries ΔE00 | ≤ 3.5 |
| RGB overprints ΔE00 | ≤ 4 |
| Average ΔE00（所有 patch） | ≤ 1.5 |
| Max ΔE00 | ≤ 4 |
| Gray balance ΔH00 | ≤ 1.5 |

### 报告输出

- PASS / FAIL 直接判定（对齐上表，不让客户自己算）
- 每 patch ΔE00 详细值 + 色差方向
- 灰平衡偏差图（a*/b* 轨迹）
- M0/M1/M2 测量条件注明
- 支持 PDF / HTML / JSON / CSV 四种格式

---

## 7. 建档流程（8 步）

对标 GMG 并超越（对应 PIPELINE §1–§8；Step 编号与 PIPELINE § 编号一一对应）：

```
Step 1:  准备 — 喷头检查 + 墨水批次确认 + 仪器白板校准
Step 2:  添加介质 — 品牌_型号_克重 + 尺寸 + 打印设置 + 纸白测量 + OBA 检测
Step 3:  墨量限制 — §3.1 单通道 ramp → single_channel_max
                     §3.2 多通道 TAC 叠印梯度（RSD<2%）→ TAC_total
                     §3.3 GCR/UCR 黑版策略 → ink_limit.json
Step 4:  G7+ NPDC 校正 — P2P51 靶图 → 本地 G7+ 引擎 → g7_curve.lut
Step 5:  全通道联合线性化 — 分组靶图（C+Lc/M+Lm/K+Lk+Llk/...）→ SQP → linearization.lut
Step 6:  靶图打印 + 测量 + 前处理
         ├─ §6  打印 TC4/ECI2002（Route A）+ CYNSN 专用靶（Route B，LHS 采样）
         │       i1iO 扫描 → CxF/X-4（主）+ .ti3（Argyll 备用），双轨持久化
         └─ §6.5 测量数据自动前处理（冗余合并 / k-NN 异常检测 / S-G 平滑 / 条件检查）
                  → 清洗后 CxF + 清洗后 .ti3 + measurement_qa.json
Step 7A: ICC profile 生成 — 清洗后 .ti3 → 云端 Argyll colprof → printer.icc（与 7B 并行）
Step 7B: CYNSN 模型建档 — 清洗后 CxF → 系数拟合 + 33³ LUT 预计算 → cynsn_model.bin（与 7A 并行）
Step 8:  绑定标准 + 首次验证 + 入库（对应 PIPELINE §8 + §13）
         ├─ 8.1 绑定打样标准 — FOGRA51/GRACoL2013/客户标准 → DeviceLink 或 spectral CGATS
         ├─ 8.2 品牌色绑定（可选）— 专色目标上传或手工测量 → SpotColorLibrary
         ├─ 8.3 首次验证打样 — Media Wedge → ISO 12647-7 全项检查 → PASS/FAIL
         └─ 8.4 入库 — 写入 active.json（含 lut_status/lut_rebuild_date/checksum）
                        写入 calibration_baseline.json（§8.5 AutoCal Check 首次对比基准）
                        PrintCondition 状态 → READY 🟢
```

---

## 8. 日常出样流程

```
1. 接收 PDF
2. §8.5 AutoCal Check Upfront（自动，<90秒）：
   ├─ GREEN → 直接继续
   ├─ AMBER → Quick Cal（5-8 分钟）→ 继续
   └─ RED → ON_HOLD → 通知操作员
3. Ghostscript 渲染（PDF/X-4 + DeviceN 保留）
4. 混合路由决策（对象级：A 或 B）
5. 色彩转换（Route A: ICC DeviceLink；Route B: LUT 三线性插值 + 局部精化）
6. 自动生成控制条（ISO Media Wedge + Dynamic 专色追加）
7. TCP RAW 9100 打印（打印机序列号标识，Bonjour 动态解析 IP）
8. i1Pro3 测量控制条
9. ISO 12647-7 验证 + 报告（含品牌色 ΔE00）
```

---

## 9. 闭环校正

### 轻校准（10 分钟，日常）

```
打印控制条 → 测量 → 计算 ΔE 偏差
├─ 平均 ΔE00 < 1.5 → PASS，不动
├─ 平均 ΔE00 在 1.5-3.0 → 重做 G7+ NPDC 曲线（更新 g7_curve.lut）
└─ 平均 ΔE00 ≥ 3.0 → 大偏差：三级渐进处置（参照 EFI Fiery"渐进式校准"策略）
     第一级：触发 §15 精确诊断（30-60 分钟）→ 定位漂移通道 → 单通道修正
             修正后重测：若 ΔE00 < 3.0 → PASS，退出
     第二级（§15 修正后仍超标）：重建线性化层（§3-§5，约 60 分钟）
             不重建介质 Profile（printer.icc / cynsn_model.bin 可保留）
     第三级（线性化重建后仍不收敛）：提示完整重建（§3-§8，约 3 小时）
             常见原因：墨水批次完全更换 / 打印头更换 / 固件升级
```

### Route B 光谱级诊断

```
测控制条全光谱 → 对每 patch 比较 predicted vs measured spectrum
→ 通道归因（ink_signature_spectrum 相关性分析）
→ 自动定位漂移通道 → 只重做该通道线性化
→ 持续高漂 → 提示重建该通道 primary
```

### 质量准则

- 校准有效期：7天 / 14天 / **30天（默认）** / 不限（可配置）
- ΔE 容差：可配置（默认 ISO 12647-7 阈值）
- 过期自动提醒（橙色打印机图标）
- 急件可绕过（用户确认 + 报告标注 "UNCALIBRATED"）

---

## 10. 数据模型

### PrintCondition（四级绑定）

```
PrintCondition
├─ Printer        (型号 + 连接方式 + 墨水类型)
├─ Media           (品牌_型号_克重 + 可用尺寸)
├─ InkSet          (墨水批次 + 通道配置)
├─ DriverMode      (分辨率 + 单双向 + 墨水模式)
│
└─ 包含文件（分离存储，见 PIPELINE §PrintCondition 存储）：

   【介质 Profile，per-media，跨打印机共享】
   ├─ printer.icc            Route A: ICC output profile
   ├─ cynsn_model.bin        Route B: CYNSN 拟合系数（~50KB）
   ├─ cynsn_lut.bin          Route B: 预计算 Lab→Device LUT（~1MB，33³ × 12ch × uint16）
   ├─ gamut_boundary.gbd     超色域预检用色域边界描述符（~200KB，3D 凸包；§10/§11 加载）
   ├─ fullgamut.cxf/.ti3     全色域光谱测量数据（CxF 主 + .ti3 Argyll 备用）
   ├─ reducegamut.cxf/.ti3   标准色域光谱测量数据
   └─ measurement_qa.json    §6.5 清洗报告（原始/清洗后 patch 数、outlier 列表等）

   【线性化，per-printer，日常重校】
   ├─ ink_limit.json              TAC + 单通道上限 + GCR 参数
   ├─ g7_curve.lut                G7+ NPDC 校正曲线
   ├─ linearization.lut           全通道联合线性化 LUT
   ├─ media_config.json           介质配置（含 OBA 状态）
   ├─ calibration_baseline.json   §8 建档完成时写入的基准测量值（§8.5 AutoCal Check 首次对比用）
   └─ calibration_history[]       校准历史记录（最近 50 次或 12 个月）
```

### 打样标准（Source Reference）

```
ProofingStandard
├─ name              (FOGRA51 / GRACoL2013 / 客户自定义)
├─ icc_profile       (source ICC，如 ISOcoated_v2_300.icc)
├─ spectral_cgats    (source spectral CGATS，如 FOGRA51_spectral.txt)
├─ measurement_mode  (M0 / M1 / M2)
├─ paper_type        (Coated / Uncoated / CCNB)
└─ tolerance         (ISO 12647-7 阈值覆盖)
```

---

## 11. 需要新增/修改的引擎文件

| 文件 | 状态 | 说明 |
|------|------|------|
| `engine/src/ink_limit.h` | 扩展 | multi-channel TAC test + GCR 参数 |
| `engine/src/g7_validation.h` | 扩展 | NPDC→LUT 输出 + P2P51 靶图 |
| `engine/src/sqp.h` | 扩展 | 联合通道约束（浅墨+主色墨分组） |
| `engine/src/iso12647_validation.h` | **新增** | ISO 12647-7:2016 验收 + 报告 |
| `engine/src/spectral_target.h` | **新增** | Lab→光谱映射 + spectral CGATS DB |
| `engine/src/hybrid_router.h` | **新增** | 对象级 A/B 路由决策 |
| `engine/src/cynsn.h` | 扩展 | Cellular 分块 + KM 层叠 + 反求解 + LUT 构建 |
| `engine/src/cynsn_lut.h` | **新增** | 33³ Lab→Device LUT 构建（GCD 并行）+ 三线性插值 + 局部精化 + LUT 版本控制 |
| `engine/src/gamut_boundary.h` | **新增** | GBD 预计算（3D 凸包 + 边界三角化）+ O(log N) 最近可达点查找 |
| `engine/src/calibration/autocal_check.h` | **新增** | AutoCal Check Upfront（每 Job 出样前自动校准检查） |
| `engine/src/cost.h` | 扩展 | 多目标函数（7 权重项，含 E_gamut_smooth）|
| `engine/src/calibration/state_machine.h` | 扩展 | 循环校正 + 质量准则 |
| `engine/src/print_condition.h` | **新增** | PrintCondition 数据模型（分离存储架构）|
| `engine/src/pdf_pipeline.h` | **新增** | Ghostscript 商业授权封装 |
| `engine/src/control_strip.h` | **新增** | Dynamic Control Strip 生成（从渲染设备值提取）|
| `engine/src/pjl.h` | 扩展 | TCP RAW 9100 传输层（替代 CUPS/USB 路径）|
| `engine/src/measurement_qa.h` | **新增** | 测量数据前处理（4 算法，§6.5）|
| `engine/src/cxf_io.h` | **新增** | CxF/X-4 读写接口 |
| `engine/src/spot_normalize.h` | **新增** | 6 步专色名称规范化 + Levenshtein 模糊匹配 |
| `apps/gcolour/` | **新增** | gcolour UI 应用 |

---

## 12. 与 jcolour 的关系

```
jcolour（摄影师 RGB）          gcolour（打样 CMYK）
├─ ProPhoto → ICC → PJL        ├─ PDF → GS → ICC/CYNSN → PJL
├─ 大色域线                     ├─ 精确色线
├─ 蜂巢 Mode A（视觉分形）      ├─ 蜂巢 Mode B（光谱驱动）
├─ B&W 双模式                   ├─ ISO 12647-7 验证
├─ 无 PDF 引擎                  ├─ Ghostscript 商业授权
├─ 无打样标准                   ├─ FOGRA/GRACoL/客户标准
│                               │
└─── 共享 engine/ ──────────────┘
     spectral.h, delta_e.h, color_pipeline.h,
     km.h, cynsn.h, pjl.h, i1sdk.h,
     g7_validation.h, sqp.h, gamut_mapping.h,
     tiff_io.h, profile_library.h
```

两个 App 独立 Bundle ID，各自上架 MAS，共享 engine/ 库。
