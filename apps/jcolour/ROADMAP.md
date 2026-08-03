# jcolour ROADMAP

> 沿用 ADR-22 三段式发布策略，针对摄影师专用版聚焦后调整。
> 时间轴 M0 = Early Access 闭门发布日；M-2 = Spike 启动日。

---

## 时间轴总览

```
M-2 ── Spike ──── M0 ──── Early Access ──── M12 ──── v1.0 公开 ──── M15 ──── v1.5 Pro ──── M18
        2 个月            12 个月（闭门 20 用户）         3 个月                3 个月
```

---

## 阶段一：Spike（M-2 → M0，2 个月）

**目标**：5 个技术原型全绿，证明可造。详见 [`SPIKES.md`](SPIKES.md)。

| Spike | 关键问题 | 验收 |
|-------|---------|------|
| 1. 色彩管线 CLI | LCMS2 + 36 通道光谱能否保持 ΔE2000 ≤ 1.0 vs Photoshop | CLI 工具跑通 + 测试报告 |
| 2. **PJL 直连 Epson** | 能否绕开 macOS Print Dialog 全程 16-bit + 控制每条墨道 | 实机打出可控墨量灰阶 |
| 3. i1Pro3 SDK 沙盒 | i1 SDK 在 MAS 沙盒里能否取到光谱数据 | 测一张靶得到 36 通道光谱 |
| 4. 云端 Argyll API | Docker Argyll 子进程隔离 + 云端往返 < 30s | 一次完整 ICC 生成往返 |
| 5. **最小 UI 端到端** | B&W 单黑/多色黑切换 + Per-ink 线性化曲线编辑器原型 | 摄影师肉眼对比效果 |

**Gate**：5 项全 PASS 才进入 Early Access 开发。任一红灯 → 重新评估方案。

### Spike 验收状态

> 自评填表；每项标注：⬜ 未开始 / 🟡 代码就位待实机/沙盒/盲测验收 / ✅ 通过 / 🔴 红线。
> 即使代码已写，未达「实机/沙盒/盲测」的验收条件不得标 ✅——避免「自以为过了」。

| # | Spike | 代码状态 | 验收状态 | 待办 / 阻塞 |
|---|-------|---------|---------|------------|
| 1 | 色彩管线 CLI | `color_pipeline.cpp`、`spectral`、`delta_e`、`g7_validation` (823 行)；channel_layout 11-active 模型就位 | 🟡 待实机 | 跑 ΔE2000 vs Photoshop 对比测试；G7+ 校正曲线 `g7_curve.lut` 输出待补 |
| 2 | macOS PJL 直连 | `pjl.cpp` 重写（getaddrinfo / port 9100 / `expand_to_physical` 通道映射）；通道互斥经型别系统编译期强制 | 🟡 待实机 | P9580 / P7580 / P20580 实机测试：16-bit 全程灰阶 + 单黑模式（非 active 黑通道 = 0）+ Sonoma / Sequoia 双版本 |
| 3 | i1Pro3 SDK 沙盒 | `i1sdk.cpp` 394 行；`WF4MeasurementBackend` 介面就位（`SimulatedBackend` 默认、`I1Pro3Backend` stub 待 SDK 接入） | 🟡 待沙盒 | MAS 沙盒 entitlement 配置 + i1Pro3 与 **i1iO 自动扫描臂**双设备识别 + TestFlight 提交不被拒 |
| 4 | 云端 Argyll API | `cloud/api`、`cloud/argyll` 结构就位 | 🟡 待端到端 | 端到端往返 < 30s + 单次成本 < $0.05 + GPL 法律审查 + ICC 质量对照本地 `-qh -d3` 基准 |
| 5 | 最小 UI 端到端 | `bw_mode_widget`、`curve_editor`、Fiery UI 就位（apps/jcolour ~14K 行） | 🟡 待盲测 | 5 位种子摄影师盲测：5 分钟出第一张单黑 / 10 分钟出 split-tone 多色黑 / 易用度 ≥ QTR×5 |

**Gate**：5 项 ✅ 才进入 Early Access 开发。当前任一为 🟡 表示**EA 功能开发是带风险并行的**——若 Spike 翻红需回滚。

### 引擎重构进度（Phase A–E，2026-05）

完整 audit 报告的 30 个 bug 全部清空（6 critical + 14 high + 6 medium + 4 low）。**核心结构变更**：

- ✅ **`engine/src/channel_layout.h`** — 11-active / 12-physical 型别系统，PK/MK 互斥编译期保证
- ✅ **wf1**（单黑 1 ch）— 不变；gray→L* 公式修正（黑色像素现在真的印成黑色）
- ✅ **wf2**（多色黑 3 ch）— 从 4 ch 重构为 `{MAIN_K, GY, LGY}` + `BlackKind` 元数据
- ✅ **wf3**（B&W + 微彩 6 ch）— 从 7 ch 重构；**真实 K-M 模型**（per-wavelength K/S + 真 Y-N 网格搜索）；`Wf3MultiPatch` 多通道 Neugebauer combos
- ✅ **wf4**（全彩 11 ch）— 从 12 ch 重构为 `ActiveInks`；TIL 真实施行；16-bit 输入路径；`WF4MeasurementBackend` 介面
- ✅ **wf6**（ProPhoto + gamut compression）— 11 ch；LCMS2 真实 sRGB→Lab；`build_gamut_hull` 真解析 ICC blob；hue-band 保护色（skin / sky / foliage）
- ✅ **PrintConditionState** — 4 状态 → **9 状态**完整转换表（spec 对齐）
- ✅ **`pjl.cpp`** — `getaddrinfo` / port 9100（不是 80）/ 修正通道映射 / IPv6
- ✅ **`softproof.cpp`** — Or/Gr/V 索引对齐 channel_layout + static_assert
- ✅ **测试覆盖** — 54/55 测试套件通过（test-tiling 预先存在的 hang，无关；2460+ assertions）

---

## 阶段二：Early Access（M0 → M12，12 个月）

### 范围（必须发布）

> 标注约定：`[x]` = 引擎层完成 + 单元测试覆盖；`[ ]` = 未开始；`🟡` = 代码就位但需 UI/集成/实机验证；`Spike-N` = 阻塞在 Spike-N 实机验收上。

#### 引擎与驱动
- 🟡 PJL 直连 P9580 / P7580 / P20580（`pjl.cpp` 重写 + `expand_to_physical` 通道映射；通道互斥编译期保证）— **Spike-2** 实机
- [x] 全程 16-bit 数据通路（`apply_wf4_transform` 改 `uint16_t`、`color_pipeline`、`tiff_io` 全 16-bit）
- [x] 36 通道光谱内核（`spectral.cpp`、ADR-17 项目宪法）
- [x] LCMS2 集成 + ICC 链路（`color_pipeline`、`softproof`、`wf6_chart`、`wf6_gamut`、`generate_wf4_lut_from_icc` 都走 LCMS2）
- 🟡 X-Rite i1 SDK 沙盒封装（`i1sdk.cpp` 394 行 + `WF4MeasurementBackend` 介面）— **Spike-3** 沙盒

#### B&W 双模式
- [x] 单黑模式（wf1 完整：PCHIP LUT + 墨量上限分析 + gray→L\* 公式修正）
- [x] 多色黑模式底层（wf2 重构为 3-ch `{MAIN_K, GY, LGY}` + `BlackKind` 元数据 + K-M 模型）
- 🟡 多色黑 UI 6 个内置预设（Neutral / Selenium / Sepia / Split-Cool-Highlight / Split-Warm-Highlight / Custom）— 引擎 ✓；三段色环 UI ✓（`split_tone_wheel.cpp` CIELAB a*/b* 盘 + 6 预设；H/M/S 拖曳，写入 `BWModeSettings::*Tint`）— 待接 wf3 `Wf3SplitConfig::target_a/b` + 盲测
- 🟡 Per-ink 线性化（wf1 单黑 ✓；4 黑通道 `PK/MK/GY/LGY` 编辑器 UI ✓ — `black_linearization_panel.cpp` 4-tab InkCurveEditor，嵌入多色黑页，`BWModeWidget::blackLinearization()` 暴露 256 点曲线供引擎 LUT）— 待接 wf2/wf3 LUT builder + 实测回填
- 🟡 G7+ 灰阶向导（`g7_validation.cpp` 823 行就位；UI 向导 ✓ — `g7_wizard.cpp` 四步：列印靶 → 量测 → 结果分析（`G7ComplianceChecker` 评级 + 逐塊 ΔE 表）→ HTML 报告导出；接进校准流程第 5 步 G7）— 待接 i1Pro3 实测取代示范数据

#### 彩色与软打样
- 🟡 **GamutAdvisor**（`intent_advisor.cpp` 182 行 — 直方图分析 + 记忆色掩码就位；UI ✓ — `gamut_advisor_widget.cpp` 接进 Job Inspector › Mgmt：场景分析条 + 双意图建议 + 一键套用 intent/BPC；`intent_advisor.cpp` 编进 UI）— 待接预览实际像素取代示范影像
- [x] **A/B 软打样底层**（`softproof.cpp` 完整：paper white sim + ΔE OOG mask + Or/Gr/V meter；7 cases / 20 assertions ✓）— 左右并排 UI ✓（`ab_softproof_view.cpp` QSplitter 双窗格 原图 vs 软打样 + 纸白/热图开关；从 Job Inspector › Mgmt › Soft proofing 启动，纸白参数承接 SoftProofingWidget）— 待接 SoftproofEngine::render 实际 ICC 渲染
- [x] **Or/Gr/V 调用率 meter**（`SoftproofResult::ink_coverage_ogv`，索引对齐 `channel_layout.h`）
- 🟡 软打样精度模块（纸白模拟 ✓ + ΔE 警告 ✓ + BPC 决策提示 ✓ — `SoftProofingWidget` 依「意图 + 纸白 L*（霧面 Dmax 代理）+ 影像深陰影佔比」给建议，颜色区分符合/待调整，一键套用；阴影比例由 `ImagePreviewWidget::loadImage` 实算回填）
- [x] 监视器校准状态检测（`SoftproofEngine::is_display_adequate` + `check_display_coverage`，阈值 95% P3）
- 🟡 蜂巢专色 UI 模式 A 基础版（`spot_color.cpp` 487 + `fiery_spot_color_editor.cpp` 584 就位）— 影响范围滑块 UI ✓（`updateInfluencePreview` 改用真实 Lab→sRGB 专色 + smoothstep 衰减 + 半径边界标记；L*/a*/b* 变更实时刷新；`InfluenceRadiusSlider::setValue` 同步标签；从 Job Inspector 启动）

#### 工作流
- 🟡 命名预设（`preset_manager.cpp` 198 行就位；沙盒集成 ✓ — presets.json 改存 `AppDataLocation`（沙盒容器 Application Support）+ `QSaveFile` 原子写 + mkpath 失败处理；`main.cpp` 补 `setOrganizationDomain("com.jcolour")` 对齐容器路径；已实测路径/原子写/读回往返）— 待接 UI 调用 savePreset/getAllPresets
- 🟡 批量队列（`queue.cpp` 459 + `job_inspector.cpp` 646 就位）— 按黑墨自动分组逻辑 ✓（`PrintJob` 加 `BlackInk{Either,PK,MK}` 字段；`groupByInk` 批为 PK→Either→MK ≤1 次换墨，`countSwitches` 正确处理 Either 不强制换墨；`PrintQueueWidget`/`BatchQueueOptimizer` 共用；算法实测：交错 4→1、纯 PK+Either→0）— 待 UI 创建作业时按纸张/墨模式填 `blackInk`
- 🟡 Edition Tracking 版次管理（`edition_tracking.cpp` 268 就位；EXIF 写入 ✓ — `edition_exif.cpp` 用 libtiff 写基线标签（ImageDescription/Artist/Copyright/Software/DateTime）+ EXIF 子 IFD（UserComment/DateTimeOriginal），`EditionManager::recordPrintWithExif` 串接版次信息；已实测往返读回基线+EXIF 标签 PASS）— 待 UI 在出样时调用
- 🟡 测试打印（`test_print.cpp` 272 就位；缩小样 + 1:1 区域 + 同纸 UI ✓ — 模式由单选钮写入 `currentMode_`，`getConfig` 正确返回（原本硬编码 Full）；`updatePreview` 按模式渲染：Full 整图、ScaledDown 样张置于参考大纸、OneToOne 标出 8×10" 可 1:1 区域、SamePaper 按所选纸张+自动旋转排版；选项变更实时刷新）— 待接出样管线
- 🟡 基础拼版（`nesting.cpp` 316 + `true_shape_nesting.cpp` 364 就位；shelf 算法补全 ✓ — 展开 `copies`、自动旋转入行、正确分页 + 利用率按页数归一 ≤1；RowMajor 改为最高优先入行更紧凑；实测 copies/旋转/利用率 PASS）— TrueShape(libnest2d) 属 v1.0 阶段
- 🟡 本机裁切（`external_cutter.cpp` 776 就位）— OPOS/ARMS/CutContour 完整度评估 ✓（见 [`CUTTER-ASSESSMENT.md`](CUTTER-ASSESSMENT.md)：串口传输/指令成形可用；裁切对位、座标单位换算、轮廓 offset、Code39 条码为缺/占位；更正 OPOS=Graphtec、ARMS=Roland 误标）— 实作归入 v1.0「外部裁切机准备」

#### 校准
- 🟡 云端 Argyll ICC 生成 API + 客户端集成（`cloud/` 目录结构就位）— **Spike-4** 端到端
- 🟡 G7+ 验证报告基础版（`g7_validation.cpp` ADR-11 ✓；PDF 导出 ✓ — 引擎 `generate_pdf` 原本只把 HTML 字节写进 .pdf，改由 G7 向导用 Qt `QTextDocument`→`QPdfWriter`(A4) 渲染真实 PDF；导出对话框 PDF/HTML 双选；实测输出 `%PDF-` 头 PASS）
- 🟡 墨盒批次跟踪基础版（`ink_lot_tracker.cpp` 378 就位；补全 ✓ — `needsReorder` 改为「同类墨全部低于阈值才提示补货」（原本任一低即报，忽略尚有满盒）；`recordUsage` 钳制 printsRemaining≥0 + 接上原本 dead 的 `updateEstimates` 用历史均值重估余量；存储 QSettings+JSON 全字段往返 ✓；实测满盒/低量/新盒共存/钳制 PASS）
- [x] PrintCondition 9 状态机（4 → 9 状态完整转换表 + 测试覆盖）

#### 系统
- 🟡 MAS 上架准备（`storekit_manager.cpp` 331 就位）— sandbox entitlements 验证待 **Spike-2/3**
- 🟡 永久免费档 + 付费订阅档（StoreKit 就位）— 功能门控逻辑待
- 🟡 $99/年早鸟价 + 终身锁定逻辑

#### 测试 / QA
- [x] 引擎单元测试覆盖（54/55 测试套件通过，~2460+ assertions）
- [x] 完整 audit 报告 30 个 bug 全部清空（6 critical + 14 high + 6 medium + 4 low）

### Early Access 用户运营
- 闭门 20 位种子摄影师（DPReview / LL / Reddit / 国内图虫渠道招募）
- 月度反馈循环（私域 Slack/Discord）
- 每 4 周一次更新发布

---

## 阶段三：v1.0 公开（M12 → M15，3 个月）

### Early Access → v1.0 增量

#### 拼版与裁切（沿用 ADR-20 / 21）
- [ ] True Shape Nesting（libnest2d + Clipper）
- [ ] Tiling 大图分块
- [ ] Step & Repeat
- [ ] 批量联系表
- [ ] 光谱驱动耗墨预估
- [ ] 外部裁切机准备（OPOS / ARMS / CutContour 专色层 + PDF/X 导出）

#### 报告与社区
- [ ] G7 验证报告导出（PrUA attribution）
- [ ] 蜂巢专色 UI 模式 A 完整版

#### 纸张生态
- [ ] 内置 50+ paper × printer profile 库（Hahnemühle / Canson / Moab / Epson Legacy / Innova）
- [ ] 第三方 profile 拖入 + 自动放置
- [ ] profile 元数据校验

#### 上架与营销
- [ ] Mac App Store 公开发售
- [ ] 主档 $199.99/年
- [ ] 摄影师 KOL 评测合作（Keith Cooper / 国内 UP 主）
- [ ] 自有 YouTube 频道开通
- [ ] 纸张品牌联名（Hahnemühle/Canson 卡片合作）

---

## 阶段四：v1.5 Pro（M15 → M18，3 个月）

### v1.0 → v1.5 增量

#### B&W 加强
- [ ] 灰阶曲线编辑器（25 控制点）展开模式
- [ ] 自定义半色调点形（圆/椭圆/欧几里得）
- [ ] 全 12 通道 Per-ink 线性化（含 Or/Gr/V）
- [ ] 模拟胶片预设包（Tri-X / Acros / IR）
- [ ] Multi-spectral metamerism（D50/D65/A/F11 切换软打样）

#### 广色域映射加强
- [ ] **SemanticGamutMapper**（CoreML 语义分割 → 分区独立 intent + 局部强度，详见 [`PIPELINE.md`](PIPELINE.md) §8.4 / [`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md) §5）
- [ ] 全 12 通道 G7+ 阴影/高光精修（利用 Or/Gr/V 做超精细中性补偿）
- [ ] 12 通道 TIL 与晕染阈值算法（详见 [`PIPELINE.md`](PIPELINE.md) §3）

#### 专色与拼版
- [ ] 多次迭代专色闭环
- [ ] Imposition 完整（折页 / 16up / 32up）
- [ ] 主体保护 AI（Core ML 人脸/主体识别避让裁切）

#### 硬件扩展（评估）
- [ ] Epson P10000 / P20000 支持
- [ ] Canon imagePROGRAF PRO 评估（用户基数大于 Mutoh 摄影圈）
- [ ] Mutoh 水性颜料款评估（XpertJet 1641WR 等少数型号）

#### 升级与定价
- [ ] MAS in-app upgrade 到 Pro 档
- [ ] $399/年 Pro 档定价

---

## 风险与对冲

| 风险 | 触发概率 | 影响 | 对冲 |
|------|---------|------|------|
| Spike 2 PJL 直连失败 | 中 | 致命，整个产品推翻 | M-1 月内出结果，可改回 macOS Print Dialog 包装方案 |
| i1 SDK 商业授权谈不下 | 低 | 大，影响测靶能力 | 备选 ColorMunki SDK / 自研 ColorChecker 24 块法 |
| MAS 审核拒绝 PJL 直连 | 低 | 中，需要重做沙盒方案 | 提前与 Apple Developer Relations 沟通 |
| Apple Silicon Native 优势被竞品追平 | 中（ImagePrint 可能 2026 出原生版本） | 中 | 加快发布，先占位心智 |
| Sonoma 后续版本破坏 PJL 直连 | 中 | 中 | 内部 macOS Beta 跟进，及时适配 |
| 早期种子用户抱怨 B&W 不够 | 低 | 中 | EA 阶段月度迭代多色黑 UX |

---

## 衡量指标（North Star）

### Early Access 阶段
- 20 位种子用户中 ≥ 15 人月活
- 平均每位种子用户每月打印 ≥ 30 张
- B&W 双模式被使用率 ≥ 60%
- 净推荐值 NPS ≥ 50

### v1.0 公开阶段
- 6 个月下载（含免费档）≥ 5,000
- 付费转化率 ≥ 3%（150+ 付费）
- 评分 ≥ 4.5（MAS）
- 评测覆盖：Luminous Landscape / Northlight / Petapixel ≥ 2 篇

### v1.5 Pro 阶段
- 公开版用户 → Pro 档转化率 ≥ 15%
- 续费率 ≥ 75%
- ARR ≥ $200K

---

## 相关文档

- 全局 PLAN：[`PLAN.md`](PLAN.md)
- 用户画像：[`PERSONA.md`](PERSONA.md)
- 调色痛点：[`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md)
- B&W 工作流：[`BW-WORKFLOW.md`](BW-WORKFLOW.md)
- 竞品对标：[`COMPETITORS.md`](COMPETITORS.md)
- Spike 详细：[`SPIKES.md`](SPIKES.md)
- 共享引擎规划：[`../../docs/PLAN-shared.md`](../../docs/PLAN-shared.md)
- ADR：[`../../docs/DECISIONS.md`](../../docs/DECISIONS.md)
