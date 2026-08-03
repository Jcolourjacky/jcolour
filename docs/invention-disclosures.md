# 发明揭露书草稿（Invention Disclosures）— 首批 A1 / A2

> **状态** 草稿 v0.1（2026-06）· **交付对象** 专利律师
> **⚠ 非法律意见**；这是工程侧「发明揭露书」初稿，供律师据以撰写权利要求、做正式新颖性/FTO 检索、定 claim 宽窄与申请国别。
> **策略前提** 见 [`ip-strategy.md`](ip-strategy.md)：**只揭露「可被观察到/可举证」的方法与 UI 行为；核心数值内核（阈值、常数、分类器、分离/调用数学）保留为商业秘密，不写进权利要求。**
> **填写项** `[ ]` 处由发明人/公司补全：发明人姓名、构思日期、首次书面/实作日期、见证人、雇佣 IP 归属。

---

## 公共信息（两件共用）

| 项 | 内容 |
|---|---|
| 申请人 / 受让人 | `[公司主体]` |
| 发明人 | `[姓名、地址、国籍；按贡献排序]` |
| 构思日期 / 首次实作 | `[date]` / 本仓库提交记录（engine/src/ogv_meter.*、apps/jcolour 验证流程） |
| 见证人 | `[姓名、签名、日期]` |
| 技术领域 | 多通道喷墨彩色管理 / 扩展色域（CMYK+Orange/Green/Violet）RIP 软件 / 打印校准 |
| 相关产品 | jcolour（藝術微噴 RIP，Epson SureColor PRO12，PJL 直送） |

### 对 USPTO 的告知义务（IDS）— 发明人已知的在先技术
撰写时须列入 Information Disclosure Statement（详见 [`multicolor-icc-strategy.md`](multicolor-icc-strategy.md) §2、[`ip-strategy.md`](ip-strategy.md) §3.4）：
- **US12034903**（Fiery LLC）语义分割→指派**离散专色**（非 O/G/V 制程墨、非调用率 meter）
- **US8873102**（HP）机上依装墨**动态分离**（非验证/非调用率提示）
- **US9373063**（Onyx）同色相多墨拆分（PVR 法；属 A3，非 A1/A2）
- **US8054504**（Agfa）对立墨/封闭曲面多色分离
- 学术：Image-Dependent Gamut Mapping（Morovic 等）；Neugebauer/Yule-Nielsen 前向模型

---

# 发明揭露 A1 — 图像相关的扩展色域油墨「调用率仪表 + 建议回路」

## 1. 名称
**A pre-print, per-image meter and advisory method for extended-gamut ink (Orange/Green/Violet) recruitment in a raster image processor.**
（一种 RIP 中、出片前按图估计并提示扩展色域油墨（O/G/V）调用率的方法与界面。）

## 2. 要解决的问题
扩展色域打印机（CMYK + O/G/V）让用户为「更大色域」付了油墨/硬件成本，但：
- 现有 RIP 的分离/profile **与图像内容无关**（Esko Equinox / ColorLogic 静态），**不告诉操作员「这张图到底用没用到 O/G/V」**；
- 操作员常因**源文件色域不足**（误用 sRGB/AdobeRGB）或**渲染意图选错**，使扩展色域形同虚设却**毫无反馈**，照印 → 浪费介质/油墨/时间，且事后难归因。

## 3. 发明概述（可观察的方法）
出片前，对**当前图像**估计每条扩展油墨（Or/Gr/V）的**调用率**，以**三个仪表**显示；并按阈值给出**建议**（如调用率过低 → 提示「扩展色基本未用，建议重选渲染意图 / 更换源文件」），点击建议**直接回到意图选择**，重选后**实时重估**——形成闭环。

## 4. 详细实现（足以使能；数值内核留商业秘密）
> 实作参照 `engine/src/ogv_meter.{h,cpp}`、`test-ogv-meter`。**以下描述「方法步骤」；具体色相带中心/半宽、CMYK 彩度上限、各墨 headroom、点火阈值等【常数与曲线】保留为商业秘密，权利要求以「色相相关的可达性上限」「超出量比例」等功能性术语表述。**

1. 将源图像转换到与设备无关的颜色空间（Lab）。
2. 逐像素求色相 h 与彩度 C。
3. 对每条扩展油墨定义一个**色相带**与该色相下的**CMYK 可达彩度上限**；当像素彩度**超过**该上限且落在该带内时，判定其**需要**该扩展油墨，调用量正比于**超出量 / 该墨头空间**，并按色相带隶属度加权。
4. 聚合为：每墨**平均调用率**（驱动仪表）、**触发像素比例**、以及**任一扩展墨被调用的像素比例**（"扩展色域在用吗"）。
5. 以**峰值调用率**对照阈值，生成**建议文案**（<下阈：基本未用→重选意图/换源；>上阈：充分调用；中间：适度）。
6. UI：三条仪表（百分比 + 历史 sparkline）+ 建议条；**点击建议 → 重新打开渲染意图选择 → 重选后回到步骤 1 实时重估**（闭环）。
7. 出片：所选意图/分离 → 驱动打印头沉积油墨 → **输出实体印张**。

## 5. 新颖点（vs 已知在先技术）
- **图像相关**（per-image），区别于 Esko/ColorLogic 的**图像无关**静态 profile；
- 针对**扩展色域【制程墨】O/G/V**（连续调用率），区别于 **US12034903（Fiery）的离散专色映射**；
- 是**出片前的【建议回路】**——估计 → 提示 → **引导操作员重选意图** → 实时重估，区别于 **US8873102（HP）的机上动态分离**（无 per-image 提示/无操作员闭环）；
- **组合单元**：「per-image 扩展墨调用率估计 + 阈值建议 + 引导意图重选的闭环」为整体新颖。

## 6. 优点
- 操作员**看得见扩展油墨的 ROI**；出片前即捕获「源 profile 错 / 意图错」→ **省介质/油墨/时间**；与 A/B 软打样决策衔接。

## 7. 变体（供从属项）
- 调用率改由**实际分离后的光栅**（ground-truth 覆盖率）统计，而非出片前估计；
- 上限改用**profile 实测边界**（CMYK 色域外且 CMYKOGV 色域内）而非色相带启发式；
- 与**语义分割**结合做**分区**调用率；扩展到其他附加墨（Red/Blue）；阈值/文案可配置；sparkline 历史与批次统计。

## 8. 可举证性（为何选它申请）
仪表与建议是**产品中可见的 UI 行为**，竞品成品中**可观察、可复现** → 侵权可举证。**而真正的调用率数学（§7/§8 内核）仍是商业秘密，不在本揭露/权利要求内。**

## 9. 权利要求种子（英文，待律师定稿）
- *Independent (method):* A method of operating an extended-gamut printer having cyan, magenta, yellow, black and at least one of orange, green and violet process inks, comprising: analyzing content of a source image to estimate, per said at least one extended ink, a recruitment metric indicative of how much of that ink the printing of the image would use; displaying the per-ink recruitment metric; responsive to the metric satisfying a threshold condition, prompting reselection of a rendering intent; re-estimating the metric upon reselection; and printing the image, thereby depositing the process inks onto a substrate.
- *Dependent seeds:* the estimating comprises classifying image pixels by hue and chroma against a hue-dependent reachability bound of the CMYK subset; the metric is a mean recruited coverage and a fraction of recruiting pixels; the prompt is a selectable affordance returning to an intent-selection UI; the displaying comprises a per-ink meter with history; the estimate is derived from the post-separation device raster; combined with per-region semantic segmentation.

---

# 发明揭露 A2 — 闭环打印验证的「失败自动定位到校准阶段」

## 1. 名称
**A closed-loop print-verification method that automatically localizes a failure to a specific stage of a multi-stage calibration pipeline and routes recalibration.**
（一种闭环打印验证方法：自动把验证失败归因到多阶段校准管线中的【具体某一阶段】并引导只重做该阶段。）

## 2. 要解决的问题
高质量校准是多阶段串联的：**总墨量/TIL → G7 灰平衡 → 单墨线性化 → ICC profile →（出片期）图像相关意图调用**。打印验证 ΔE 不过时，操作员**无法判断是哪一阶段坏了**，只能**全部重做或瞎猜** → 慢、费介质/油墨。

## 3. 发明概述（可观察的方法）
打印验证靶并测量后，系统**自动**依失败的**度量特征**把问题**归因到具体某一校准阶段**，并**引导操作员只重做该阶段**（一键跳转）；重做后**只重验该项**，形成闭环。

## 4. 详细实现（足以使能；阈值/分类器留商业秘密）
> 实作参照 `apps/jcolour` 验证流程、`PIPELINE.md` §9。**以下描述「方法步骤与映射结构」；具体阈值、判别权重、分类器参数保留为商业秘密。**

1. 打印**验证靶**（含：中性灰阶、单通道阶调、整体色块/ColorChecker、扩展色相 ramp），i1Pro3 测量。
2. 计算**分阶段诊断度量**：中性轴 a\*/b\* 偏移；各单通道密度线性度；整体/记忆色 ΔE2000；扩展墨实际覆盖 vs 预期。
3. **失败签名 → 阶段映射**（决策结构）：
   - 灰阶 a\*/b\* 偏移显著 → **G7 阶段**；
   - 某单通道密度非线性 → **线性化阶段**；
   - 整体 ΔE 高 / profile 偏 → **ICC 阶段**（并提示查 TIL）；
   - 扩展墱调用低于预期 → **出片意图/§8** 或重测靶覆盖；
   - 光源切换变色（同色异谱）→ **多光谱模块**。
4. 呈现「**重做阶段 X**」+ 一键导航；**只重运行该阶段** → **只重验该项**（闭环）。

## 5. 新颖点（vs 已知在先技术）
- **闭环验证 + 自动【阶段定位】+ 引导只重做该阶段**，区别于通用「测 ΔE 给 pass/fail」（无定位）与人工全量重校；
- 绑定**具体多阶段校准管线**（TIL/G7/线性化/ICC/意图）的**失败签名→阶段**映射为核心新颖。

## 6. 优点
- 把重校从「全做」降到「**只做坏掉的那一阶段**」→ 大幅省介质/油墨/时间；定位确定、可复现。

## 7. 变体（供从属项）
- 度量集合与阶段集合可变；映射用**决策表 vs 训练分类器**；**自动重做 vs 提示**；按 5 种打印模式分别定位；扩展到走纸/喷头阶段。

## 8. 可举证性
失败时产品的**可观察行为**（告知具体阶段 + 跳转）即被主张的方法，竞品成品可观察。**具体阈值/分类器留商业秘密。**

## 9. 权利要求种子（英文，待律师定稿）
- *Independent (method):* A method of verifying a printer calibrated by a pipeline having a plurality of ordered calibration stages, comprising: printing and measuring a verification target; computing a plurality of stage-diagnostic metrics from the measurement; when a verification criterion is not met, classifying the failure to one stage of said plurality by a signature of the metrics; directing recalibration of the identified stage without recalibrating the others; and re-verifying, thereby producing a physical print meeting the criterion.
- *Dependent seeds:* the stages include ink-limit, gray-balance (G7), per-ink linearization, and ICC profiling; gray a\*/b\* drift maps to G7, single-channel non-linearity maps to linearization, overall ΔE bias maps to ICC; the directing comprises a one-click navigation to the stage; classifying uses a decision table; classifying uses a trained classifier; extended to per-print-mode.

---

## 附：撰写/申请待办（交律师）
- [ ] 补全发明人 / 日期 / 见证 / IP 归属
- [ ] A1/A2 正式新颖性 + FTO（重点核 US12034903 全 claims、US8873102；A1 与「调用率 meter」、A2 与「验证失败定位」的全球检索）
- [ ] §101 框定（均已绑「驱动打印头→输出实体印张」物理过程）
- [ ] 申请国别（US 优先；EP/JP/CN 视市场）
- [ ] 图（A1：三仪表+建议条+闭环箭头；A2：管线阶段+失败签名→阶段映射+一键跳转）
- [ ] 确认核心内核（S1–S3）确未落入权利要求（保商业秘密）
