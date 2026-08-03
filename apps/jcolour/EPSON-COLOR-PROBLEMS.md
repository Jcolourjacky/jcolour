# Epson 调色痛点全集 + jcolour 对策

> **本文档定位：痛点知识库**。执行规范见 [`PIPELINE.md`](PIPELINE.md) / [`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md)；架构决策见 [`ADR-29`](../../docs/DECISIONS.md#adr-29)。本文记录 jcolour 设计的需求来源，不是工程实现规范。
>
> 本文档基于 2026 年 4 月对 DPReview / Luminous Landscape / Northlight Images / PhotoPXL / Reddit r/printers / Fred Miranda / Cambridge in Colour / Red River / PrintPlanet 论坛与社区的深度调研。
>
> 每条痛点都附 **症状 / 根因 / 当前 workaround / jcolour 对策**。这是 jcolour v1.0 P0 功能的需求来源。

---

## 一、色偏与色域问题（Color Cast & Gamut）

### 1.1 哑光纸暗部偏品红 / 亮光纸高光偏绿

- **症状**：同一张照片在哑光（Hahnemühle Photo Rag）上打偏品红，在亮光（Epson Premium Luster）上打偏绿
- **根因**：
  - Epson 出厂 ICC profile 对老款纸张未更新（特别是 PK 和 MK 黑墨切换后的色彩平衡）
  - Photo Black 与 Matte Black 切换时，LK / LLK 灰阶比例没有同步重新平衡
  - 亮光纸高光区的 Cyan 通道用量低，微小偏差被放大
- **当前 workaround**：
  - 花 $300–800 找 Breathing Color / Image Science 做 custom profile
  - 自己用 i1Pro 测靶后用 ArgyllCMS 命令行生成（需要 1–3 小时学习）
  - 在 Photoshop 里加 "曲线" 图层手动补偿
- **jcolour 对策**：
  - **Per-ink 线性化曲线编辑器**（P0 功能）：暴露每条墨水通道的灰阶响应曲线，用户可视化调整
  - **G7+ 灰平衡校准**：测一张靶就把灰轴拉直，覆盖大部分系统性偏色
  - **i1Pro3 测靶 + 云端 Argyll** 一键生成 ICC（沿用 ADR-07）

**证据**：
- [DPReview "Skin tones with P800/P900"](https://www.dpreview.com/forums/forum/16) 多个长帖
- [Luminous Landscape P9570 review](https://luminous-landscape.com/epson-surecolor-p9570-full-review-the-finest-color-printer-yet-made/)
- [PhotoPXL "Custom paper presets for 9570/7570/900/700 — a mess"](https://photopxl.com/forums/) ★

### 1.2 蓝天/紫色 Banding（紫色发紫蓝、蓝天断阶）

- **症状**：蓝色渐变（天空、傍晚）出现明显条带；紫色花朵（薰衣草、紫罗兰）发紫蓝色不准
- **根因**：
  - 8-bit 通路在阴影段精度不足（8-bit 256 灰阶在 Cyan-only 区显得粗）
  - Cyan 喷嘴局部堵塞 → 紫色由 Magenta + 少 Cyan 组成 → Cyan 不足时偏紫
  - profile 的 perceptual rendering intent 在饱和蓝向降饱和过激
- **当前 workaround**：
  - 强制 16-bit 输出（但 macOS Print Layout 报告 16-bit 切换会引起 Saturation 跳变）
  - 喷嘴清洗（费墨）
  - 改用 relative colorimetric + BPC
- **jcolour 对策**：
  - **全程 16-bit 数据通路**（P0 功能）：从输入到 PJL 数据流不降到 8-bit
  - **渲染意图智能推荐**：根据图像饱和度和打印机色域自动建议 perceptual 或 relative
  - **喷嘴健康检测**（v1.5 候选）：定期内部测试图 + 自动诊断

### 1.3 绿色草地 / 树叶发黄

- **症状**：自然绿色（草地、树叶）打出来偏黄不够鲜
- **根因**：
  - 普通打印机 Cyan + Yellow 调出的绿色色域有限
  - P9500 UltraChrome Pro12 有专用 Green 墨，但默认 profile 不充分调用
- **当前 workaround**：
  - Photoshop 里把绿色 Hue 推 −5（但人工，每张要做）
  - ImagePrint 27,000 个 profile 库里挑一个绿色更好的
- **jcolour 对策**：
  - **广色域映射引擎**（P0 功能）：充分用满 Pro12 的 Or/Gr/V 三色（Orange/Green/Violet）扩展色域
  - **蜂巢专色 UI 模式 A**：从作品里框选绿色 → 推到目标值 → 影响范围滑块控制溢出

### 1.4 肤色 / 红色发橙或发洋红

- **症状**：人像皮肤发橙、嘴唇发紫、白人脸发粉色不自然
- **根因**：肤色处于 Magenta-Yellow 中间，profile 在该区域非线性映射偏差
- **当前 workaround**：分别给"室内人像"、"室外人像"做不同 profile
- **jcolour 对策**：
  - **G7+ 灰平衡 + 中性调** 对肤色有间接修正作用
  - **专色蜂巢从图取色**：在样张里框选脸部不满意区 → 沿光谱距离微调

---

## 二、黑白与中性度（Black & White / Neutrality）

> 完整设计见 [`BW-WORKFLOW.md`](BW-WORKFLOW.md)。本节只列痛点。

### 2.1 ABW 模式只有 4 个色调预设（Neutral / Cool / Warm / Sepia）

- **症状**：想要"高光暖、阴影冷"的分裂调色（split-tone），ABW 做不到
- **根因**：ABW 是单色调全图统一，不分区
- **当前 workaround**：用 QuadToneRIP $50 软件，但学习曲线极陡（写 .qidf 配置文件）
- **jcolour 对策**：
  - **B&W 多色黑模式**（P0）— 三段色环（高光/中间调/阴影）独立调色温
  - 单一开关切换"单黑模式"（仅 PK 或 MK）和"多色黑模式"

### 2.2 黑白照灰阶横向有色偏（Horizontal Color Drift）

- **症状**：从纯黑到纯白的灰阶条，会出现"中间调偏绿、阴影偏品红"
- **根因**：
  - LK 和 LLK 浅灰墨水的色彩平衡不在严格中性
  - PK 和 MK 配方不同，单独使用时偏色方向不同
- **当前 workaround**：手画灰阶曲线在 Photoshop 里降饱和
- **jcolour 对策**：
  - **Per-ink 线性化** + **灰平衡向导**：测灰阶靶 → 自动算出每段补偿
  - **Pure Black 单黑模式**：避开浅灰墨偏色问题，纯靠 PK 或 MK 单墨打 B&W

### 2.3 PK ↔ MK 切换浪费墨水 + 色彩漂移

- **症状**：从打哑光纸（MK）切到亮光纸（PK），等 2–4 分钟，浪费 30–50ml 墨水（~$30 成本）
- **根因**：Epson 物理切换墨道，必须用新墨灌满管路
- **当前 workaround**：尽量批量打同一种黑墨纸张，再切换
- **jcolour 对策**：
  - **批量队列按黑墨自动分组**（P1 功能）：Sam 入队 12 张混合纸，jcolour 自动重排为 [所有 MK 纸先打] → [切换] → [所有 PK 纸再打]
  - **PK/MK 切换成本提示**：在批量界面实时显示"当前顺序需切换 3 次，重排后只切 1 次"

---

## 三、软打样精度（Soft Proofing Fidelity）

### 3.1 软打样在哑光纸上偏差 ΔE 15–30（最高频投诉）

- **症状**：屏幕上软打样看起来正常，实印哑光纸出来差 10–30 个 ΔE
- **根因**：
  - 哑光纸纸白点不是纯 #FFFFFF（实际是带黄绿的暖白）
  - profile 的 white point simulation 默认关闭，多数用户不知道
  - 监视器没校准 / 校准过期，软打样基础不准
  - 暗部 Black Point Compensation 行为不一致
- **当前 workaround**：
  - 打小测试样在同一张纸上确认（贵）
  - 反复试印（更贵）
- **jcolour 对策**（P0 功能"软打样精度提升模块"）：
  - **强制开启 Paper White Simulation** 并在屏幕上明显标记"已模拟纸白"
  - **ΔE 警告热力图**：软打样上叠加红色提示"该区域预计实印偏差 12 ΔE"
  - **监视器校准状态检测**：未校准 / 超过 30 天未校准时弹提示
  - **黑场补偿决策提示**：根据 profile 类型推荐 BPC on/off

**证据**：
- [Cambridge in Colour "Soft proofing in Lightroom"](https://www.cambridgeincolour.com/forums/thread65052.htm)
- [Red River "Soft Proofing Explained"](https://www.redrivercatalog.com/infocenter/soft-proofing-explained.html)
- [Luminous Landscape "Understanding Printer Colour Management"](https://luminous-landscape.com/understanding-printer-colour-management/)

### 3.2 渲染意图选错（Perceptual vs Relative Colorimetric）

- **症状**：摄影师看到选项不知道选哪个，硬印出错色
- **根因**：Epson 驱动让用户选但不解释，错选在饱和色和阴影区差异大
- **当前 workaround**：背口诀"风景 perceptual / 人像 relative"（不准确）
- **jcolour 对策**：
  - **智能推荐**：根据 profile 年龄、纸张类型、图像饱和度直方图自动推荐
  - **A/B 软打样对比**：让用户在屏幕上看到两种意图的差异

### 3.3 黑场补偿（BPC）开关混乱

- **症状**：BPC 开 → 暗部细节救回但发灰；BPC 关 → 黑实但暗部死黑
- **根因**：BPC 的效果与 profile 内部黑点定义有关，新老 profile 行为不一致
- **当前 workaround**：摄影师只能"两边都试"
- **jcolour 对策**：
  - 在软打样里实时**显示开/关两种状态对比**
  - 给出基于 profile 测试的"建议状态"

---

## 四、驱动 / 系统集成（Driver & macOS Integration）

### 4.1 macOS Ventura/Sonoma 下保存的预设丢失

- **症状**：精心配置的"Hahnemühle Baryta + perceptual + BPC + 16-bit + Photo Black + 0 Color Density"，下次打开变成"默认"
- **根因**：
  - Epson Print Layout 的 .epl 文件格式不稳定
  - macOS Print Dialog 的预设系统在系统升级后被覆盖
  - ColorSync 与 Epson 驱动双重管理时一方覆盖另一方
- **当前 workaround**：每次打印前手动从头配置（30 分钟）
- **jcolour 对策**（P0 功能）：
  - **命名预设 = {纸张, profile, 渲染意图, BPC, 黑墨, 色密度, 平台间隙}**
  - 预设存在 jcolour 自己的应用沙盒里，不依赖系统打印对话框
  - 一键打开 Sam 的 12 个常用预设

**证据**：
- [PhotoPXL "Custom paper presets for P9570/7570/900/700 — a mess"](https://photopxl.com/forums/)
- [Lightroom Queen "Print quality issues from Epson on macOS 15"](https://www.lightroomqueen.com/community/threads/print-quality-issues-from-epson-xp-15000-on-macos-15.52116/)

### 4.2 ColorSync 与 Epson 驱动双重色彩管理

- **症状**：勾了"Photoshop Manages Colors"，又在 Epson 驱动里没关 ColorSync，结果色彩被处理两次，结果"不可预测，通常很糟"（Luminous Landscape 原文）
- **根因**：macOS 设计上允许两层色彩管理，UI 不够明显
- **当前 workaround**：摄影师社区的口诀"Photoshop manages colors → driver 关闭 ColorSync"
- **jcolour 对策**（P0 功能 "Epson 调色驱动接管层"）：
  - **PJL 直连**：直接走 CUPS/IPP + PPD + PJL 与打印机通信，绕开 macOS Print Dialog
  - 色彩管理只有一层（jcolour 自己），不允许双重管理

### 4.3 16-bit 输出导致饱和度跳变

- **症状**：从 8-bit 切到 16-bit 输出，红色/绿色饱和度突然跳变 5–10%
- **根因**：Epson 驱动内部对 16-bit 数据的渐变量化算法与 8-bit 不一致
- **当前 workaround**：固定用 8-bit（损失精度）
- **jcolour 对策**：**全程 16-bit 数据通路**，从 ICC 应用到 PJL 命令统一精度

### 4.4 AirPrint 与 Epson 专用驱动冲突

- **症状**：插上 USB 后系统识别为 AirPrint 设备，色彩管理彻底失效
- **根因**：macOS 偏好 AirPrint，自动覆盖 Epson 专用驱动
- **当前 workaround**：手动在系统偏好里切换打印机驱动
- **jcolour 对策**：jcolour 内置驱动选择器，强制使用 Epson PPD

---

## 五、纸张配置文件（Paper Profiles）

### 5.1 Hahnemühle / Canson / Moab 等 profile 找不到 / 装不对

- **症状**：买了 Hahnemühle Photo Rag 308 纸，去官网下载 P9500 的 .icc，放进 ColorSync 文件夹，Photoshop 里看不到
- **根因**：
  - 第三方 profile 文件路径要放在 `~/Library/ColorSync/Profiles/` 而不是其他位置
  - 部分 profile 文件名空格 / 中文导致识别失败
  - 不同 macOS 版本 ColorSync 路径变更
- **当前 workaround**：摄影师论坛口口相传的"放对位置"教程
- **jcolour 对策**（P1 功能 "纸张配置文件库"）：
  - 内置 50+ 常用纸张 × P700/P900/P7500/P9500 的 OEM profile，一键应用
  - 支持拖拽第三方 profile，自动放置到正确位置
  - profile 元数据校验（创建日期 / 测试纸张 / 制造商）

### 5.2 Epson Legacy 纸张停产后 profile 失效

- **症状**：Epson Legacy Etching 停产后官网找不到 profile
- **根因**：Epson 不维护停产 profile
- **当前 workaround**：从老下载链接 / 其他摄影师那里要
- **jcolour 对策**：jcolour 自有 profile 库存档，包括停产纸张（v1.5）

### 5.3 一张作品打了几次记不清用过哪个 profile

- **症状**：去年打的版本现在重打，颜色不一致
- **根因**：摄影师靠手写笔记，记录散落
- **当前 workaround**：Photoshop 文件名加纸张名（半人工）
- **jcolour 对策**（P1 功能 "Edition Tracking"）：
  - 每次打印自动记录 {图像 ID, profile, 设置, 时间, 张数, 编号}
  - 历史可查："这张签名作品 Bay Bridge 12/50 是 2025-08-13 用 Hahnemühle Photo Rag 打的"

---

## 六、墨水管理与一致性（Ink & Consistency）

### 6.1 LK / LLK 浅灰墨耗用比 Or/Gr/V 快

- **症状**：用了几个月，LK 和 LLK 见底，但 Orange / Green / Violet 还满
- **根因**：浅灰墨参与几乎所有打印，彩墨用得少
- **当前 workaround**：备货时多买 LK / LLK
- **jcolour 对策**（v1.5 候选）：耗墨预估 + 进货建议

### 6.2 不同批次墨盒之间色彩漂移

- **症状**：今年的墨盒打出来比去年微偏（实测 ΔE 1–3）
- **根因**：墨水生产批次差异 + 墨盒老化
- **当前 workaround**：定期重做 G7 校准
- **jcolour 对策**：
  - **墨盒批次跟踪**（P1）：记录每个墨盒的安装日期 / 批次号
  - **漂移预警**：与上次校准对比，超过阈值提示重新校准

### 6.3 长时间不用导致喷嘴堵塞 + 浪费清洗墨

- **症状**：一周不打印，再打就要清洗 2–3 次喷嘴
- **根因**：UltraChrome 颜料墨在喷嘴干燥
- **当前 workaround**：定期打"健身画"
- **jcolour 对策**（v1.5 候选）：
  - 内置健身画自动调度（每 3 天自动提醒打一张极小测试）

---

## 七、其他高频抱怨

### 7.1 Bronzing（铜面反光）

- **症状**：亮光纸暗部从某些角度看泛金属铜光
- **根因**：颜料颗粒堆积在表面 + 不同角度反射差异
- **当前 workaround**：选 Lustre 半光纸；或加涂保护层
- **jcolour 对策**：
  - 软打样里 **Bronzing 风险预警**（基于墨量预估）
  - 推荐降低暗部 Total Ink Limit (TIL)

### 7.2 Metamerism（同色异谱）

- **症状**：在画廊聚光灯下颜色对，到客户家自然光下偏色
- **根因**：染料/颜料的光谱反射曲线与原片不同，光源变化时偏离方向不同
- **当前 workaround**：摄影师只能交付前在多种光源下肉眼检查
- **jcolour 对策**：
  - **光谱契约（ADR-17）**：内部全程 36 通道光谱计算，从根上比 Lab-based 工具更抗 metamerism
  - **多光源软打样**（v1.5）：D50 / D65 / A / F11 切换预览

### 7.3 Total Ink Limit (TIL) 不可调

- **症状**：哑光纸暗部墨量过大，纸面起皱、发亮、慢干
- **根因**：profile 内置 TIL 不暴露给用户调
- **当前 workaround**：买"硬挺"纸张，避开柔软美术纸
- **jcolour 对策**（v1.5 候选）：
  - **TIL 调节滑块**：让用户根据纸张特性调整总墨量上限

### 7.4 平台间隙 (Platen Gap) 不会调

- **症状**：厚美术纸（>0.4mm）打的时候喷头蹭脏纸面
- **根因**：Platen Gap 设置藏在驱动深处
- **当前 workaround**：阅读论坛找"该纸建议设 Standard / Wider / Widest"
- **jcolour 对策**：纸张数据库里附带推荐 platen gap 值，自动配置

---

## 八、痛点 → jcolour P0 功能映射

| jcolour P0 功能 | 解决的痛点编号 |
|---------------|-------------|
| **Epson 调色驱动接管层（PJL 直连）** | 4.1, 4.2, 4.3, 4.4 |
| **Per-ink 线性化曲线编辑器** | 1.1, 1.3, 2.2 |
| **命名预设（Paper × Profile × Ink × Settings）** | 4.1, 5.3 |
| **软打样精度提升模块** | 3.1, 3.2, 3.3, 7.1, 7.2 |
| **B&W 单黑模式** | 2.1, 2.2 |
| **B&W 多色黑模式** | 2.1, 2.2 |
| **广色域映射引擎** | 1.2, 1.3 |
| **蜂巢专色 UI 模式 A** | 1.3, 1.4 |
| **批量队列（按黑墨自动分组）** | 2.3 |
| **Edition Tracking** | 5.3 |
| **测试打印（缩小样 + 1:1 区域）** | 3.1（间接） |
| **纸张配置文件库** | 5.1, 5.2 |
| **G7+ 灰阶校准向导** | 1.1, 1.4, 2.2 |
| **云端 Argyll ICC 生成** | 1.1, 1.4 |
| **墨盒批次跟踪** | 6.2 |

---

## 九、调研引用

| 来源 | URL | 价值 |
|------|-----|------|
| DPReview Printers Forum | https://www.dpreview.com/forums/forum/16 | 摄影师真实抱怨集中地 |
| Luminous Landscape | https://luminous-landscape.com/ | 行业意见领袖深度评测 |
| Northlight Images (Keith Cooper) | https://www.northlight-images.co.uk/ | P 系列详细评测 |
| PhotoPXL Forum | https://photopxl.com/forums/ | 摄影师论坛，实操经验 |
| Red River Catalog | https://www.redrivercatalog.com/ | 工程化色彩管理教程 |
| Cambridge in Colour | https://www.cambridgeincolour.com/ | 色彩科学教育 |
| Reddit r/printers | https://www.reddit.com/r/printers/ | 普通用户痛点（含老型号） |
| Lightroom Queen | https://www.lightroomqueen.com/community/ | macOS 集成痛点 |
| QuadToneRIP | https://www.quadtonerip.com/ | B&W 极致用户的去向 |
| Breathing Color | https://www.breathingcolor.com/ | 第三方 profile 服务参考价 |
| Image Science | https://imagescience.com.au/ | 同上 |
| Hahnemühle Tech | https://www.hahnemuehle.com/en/digital-papers/icc-profile.html | 纸张厂商 profile 实践 |
| Canson Infinity | https://www.canson-infinity.com/ | 同上 |
| Epson Pro Imaging Color Mgmt PDF | https://files.support.epson.com/docid/cpd3/cpd39134.pdf | 官方文档（不全） |

---

## 十、广色域链路完整指南（如何充分用满 P9500 / P7500）

这是 jcolour 广色域映射引擎要自动化的整套流程。摄影师手动操作非常容易在某一环掉链子。

### 10.1 关键原则：色域取决于最弱的一环

```
原片色域 → 编辑空间 → 软件处理 → 打印 ICC 转换 → 打印机色域
   ↑          ↑            ↑              ↑              ↑
 取决于相机  必须 ≥ 打印机  必须 16-bit  rendering intent  P9500 极限
```

只要中途有一处压到 sRGB / 8-bit / 错的渲染意图，前后再大都没用。

### 10.2 链路每一环的正确做法

#### 拍摄输入
- 使用 **RAW**（NEF/CR3/ARW），12–14 bit 线性，超过任何 RGB 空间
- 别用 JPEG（已经压到 sRGB 8-bit）

#### 编辑空间

| 空间 | 色域 | 推荐 |
|------|------|------|
| **ProPhoto RGB** | 极大，覆盖 P9500 全部 + 不可印色 | ✅ 唯一正确选择 |
| Adobe RGB (1998) | 中等，部分 P9500 鲜艳色被砍 | ⚠️ 折损 10–15% |
| Display P3 | 中等，主要为屏幕设计 | ⚠️ |
| sRGB | 小 | ❌ 直接砍掉 30%+ |

- Lightroom Classic：导出 → 颜色空间 → ProPhoto RGB
- Photoshop：编辑 → 颜色设置 → RGB 工作空间 → ProPhoto RGB
- Capture One：默认 ProPhoto

#### 位深度
- **必须 16-bit**（48-bit total）
- 8-bit 在 ProPhoto 这么大的空间会出现明显 banding
- 16-bit 65,536 灰阶，渐变平滑

#### 打印 ICC Profile
- P9500 + 具体纸张的专属 ICC（不是通用 RGB profile）
- 优先级：自测 custom profile > 纸张厂商官方 > Epson 默认
- 老 profile（>2 年）建议重做，墨水批次有微差

#### 渲染意图（最容易选错）

| 意图 | 行为 | 何时用 |
|------|------|------|
| **Perceptual（感知）** | 整体压缩到打印色域，保持相对关系，饱和色被柔化 | 风景、自然色为主，希望整体协调 |
| **Relative Colorimetric（相对色度）** | 色域内保留原样，超色域硬切到边缘 | 可印色域内容多，要充分调用 P9500 鲜艳色 |
| **Saturation（饱和度）** | 推到饱和 | ❌ 摄影不用，会失真 |
| **Absolute Colorimetric** | 模拟纸白 | ❌ 打样用，摄影不用 |

充分用满色域的最佳组合：
- 自然偏淡 → **Perceptual + BPC on**
- 鲜艳色为主、要调用 Or/Gr/V → **Relative Colorimetric + BPC on**

#### 黑场补偿（BPC）
- 一般打开：图像最深黑映射到打印机最深黑，扩展暗部细节
- 关闭会"暗部死黑、细节丢失"

### 10.3 P9500 / P7500 独有色域优势（Pro12 三色扩展）

| 墨色 | 扩展方向 | 典型用途 |
|------|---------|---------|
| **Orange (Or)** | 红橙黄饱和段 | 夕阳、花朵、高饱和肤色 |
| **Green (Gr)** | 草绿、青绿 | 自然风景 |
| **Violet (V)** | 紫罗兰、深紫、靛青 | 花朵、夜空 |

**关键**：默认 profile 不一定充分调用这三色。要充分用满需要：
1. 最新版 P9500/P7500 出厂 profile（旧型号 profile 没有 Or/Gr/V 通道映射）
2. 自己用 i1Pro3 测靶生成 custom profile（jcolour P0 功能）
3. Saturation Match Index 高的 profile 比 ΔE Match 高的 profile 更能调用扩展色

### 10.4 纸张对色域的影响（容易忽略）

| 纸张类型 | 色域大小 | Dmax |
|---------|---------|------|
| **Lustre / Glossy（亮光）** | 最大 | 2.4–2.7 |
| **Baryta（钡地光面）** | 接近最大 | 2.3–2.5 |
| **Matte FineArt（哑光美术）** | 中等 | 1.5–1.8 |
| **Canvas（画布）** | 小 | 1.4–1.6 |

要最大色域 → Epson Premium Luster / Canson Baryta Photographique。哑光美术纸天花板就低。

### 10.5 Photoshop 实操步骤

```
1. 打开 16-bit ProPhoto RGB 图像
2. 文件 → 打印 (Cmd+P)
3. 颜色处理 → "Photoshop 管理颜色"（不是打印机管理）
4. 打印机配置文件 → 选 P9500 + 你的纸 ICC
5. 渲染方法 → Perceptual 或 Relative Colorimetric
6. 黑场补偿 → ✅ 勾上
7. 打印设置 → 16-bit 输出 ✅
8. Epson 驱动 → 颜色管理 → "OFF (No Color Adjustment)" ⚠️ 关键
9. 介质类型 → 选对应纸张
10. 质量 → 最高 2880×1440
```

### 10.6 验证链路是否真的全色域

1. Photoshop 视图 → 校样设置 → 自定 → P9500 ICC
2. 视图 → 色域警告（Cmd+Shift+Y）
   - 少量灰色区域 = 充分调用
   - 整片灰色 = 源图色域已被砍小
3. 软打样开"模拟纸张颜色"+"模拟黑色油墨"
4. 打色域测试图（northlight-images.co.uk 提供）

### 10.7 jcolour 自动化策略

调研显示 90% 摄影师在某一环出错（最常见第 2/5/6 步），花了 P9500 的钱只用上 sRGB 的色域。

**jcolour 广色域映射引擎自动化以下决策**：
- 输入图像色域检测（ProPhoto / AdobeRGB / sRGB）→ 不足时警告
- 位深度检测 → 强制 16-bit 通路
- 渲染意图智能推荐（基于直方图饱和度分析）
- BPC 自动决策（基于 profile 黑点定义）
- Epson 驱动 ColorSync 强制关闭（PJL 直连绕开）
- 纸张色域天花板提示（哑光纸不要期望 Lustre 的 Dmax）
- Or/Gr/V 调用率显示（让用户看到"这张照片用上了多少扩展色"）

---

## 十一、相关文档

- 用户画像：[`PERSONA.md`](PERSONA.md)
- 黑白工作流详细设计：[`BW-WORKFLOW.md`](BW-WORKFLOW.md)
- 竞品对标：[`COMPETITORS.md`](COMPETITORS.md)
- v1.0 范围：[`PLAN.md`](PLAN.md)
- 全局 ADR：[`../../docs/DECISIONS.md`](../../docs/DECISIONS.md)
