# jcolour 架构决策记录（ADR）

> 每条决策采用经典 ADR 格式：**Context（背景）/ Decision（决策）/ Consequences（后果）/ Alternatives（备选）**。
> 按时间顺序编号，不重排。推翻的决策保留记录，状态改为 `Superseded by ADR-XX`。

---

## ADR-01 目标用户

**Status**：**Revised by ADR-25**（产品拆分后 jcolour 用户聚焦摄影师个人；gcolour 单独定义打样/品牌；精品喷绘暂从 jcolour 移除）
**Date**：2026-04（修订 2026-05）

**Context**  
早期方向可能是"给摄影师"、"给印厂"、"给家庭"，需要收敛。

**Decision**  
双目标用户（ADR-19 后扩展为三细分）：
- **打样工作室的色彩技师 / 打样师傅**
- **艺术摄影师 / 画廊 / 艺术打印店**
- **精品喷绘工作室**（展览、博物馆复制、高端视觉陈列）—— ADR-19 扩展

**Consequences**  
- 产品形态走"专业工具"而非"消费级便捷工具"
- UX 需要支持"有分光仪"和"无分光仪"两类入门
- 决定了"双产品线"结构（ADR-10）

**Alternatives**  
- 只做摄影师：市场太小
- 只做印厂：进不去 GMG/CGS 统治的客户群

---

## ADR-02 目标硬件

**Status**：Accepted
**Date**：2026-04

**Context**  
打印机型号繁多，全适配不现实，需要聚焦。

**Decision**  
v1.0 仅支持 **Epson SureColor P7500 / P9500**（24"/44"，10+2 色 UltraChrome Pro12）。

**Consequences**  
- 色彩工程只针对这两款机器调优
- UltraChrome Pro12 的 10 种基墨是专色闭环的基础墨库
- P7500/P9500 是打样+艺术打印+精品喷绘的交集机型，一台三身份
- 后续扩展路径：
  - v1.1：P6500 / P8500（同系列新款）
  - v1.5：P10000 / P20000（60"+，大幅面精品喷绘）
  - v2.0：Canon imagePROGRAF Pro 横向扩展

**Alternatives**  
- 一期就支持 SC-T/S 系列（工业喷绘）：用户画像错位，不符合 ADR-19
- 全品牌兼容：一年内做不出来

---

## ADR-03 平台

**Status**：Accepted
**Date**：2026-04

**Decision**  
- **macOS 13+ Universal**（Apple Silicon + Intel）
- 核心色彩引擎 C++ 无 Qt 依赖部分保持跨平台，为 Windows v2.0 预留

**Consequences**  
- 摄影师市场以 Mac 为主，契合
- 开发成本降低（不做 Win 测试矩阵）
- 打样师傅中国市场偏 Windows 多，短期损失一部分

**Alternatives**  
- Mac + Win 双平台：工作量 +50%
- 只 Win：错过摄影师市场和 MAS 渠道

---

## ADR-04 测量仪

**Status**：Accepted
**Date**：2026-04

**Decision**  
一期支持 **X-Rite i1Pro3 + i1Pro3 Plus + i1iO**（官方 i1 SDK v3）。

**Consequences**  
- 覆盖全球打样工作室 ~70% 存量设备
- i1Pro3 提供 36 通道光谱数据（380–730nm @ 10nm），符合 ADR-17
- i1 SDK 商业授权 Runtime License 费用需要和 X-Rite 谈（参考 $500–2000/年）

**Alternatives**  
- Konica Minolta / Techkon：后期扩展
- 自研 SDK + ColorChecker：开发周期过长

---

## ADR-05 分发渠道

**Status**：**Revised by ADR-24**（仍走 MAS，但拆分后 jcolour 与 gcolour 各自独立 Bundle ID 上架）
**Date**：2026-04（修订 2026-05）

**Decision**  
**只走 Mac App Store**，不做独立下载。

**Consequences**  
- Apple 抽成 15–30%
- 自动更新、用户信任、退款流程统一
- 强制沙盒：导致 Argyll 必须走云端（ADR-07）
- 订阅必须用 StoreKit
- 每次大版本面对 MAS 审核

**Alternatives**  
- 独立下载 + Stripe：绕开抽成但失去 MAS 流量
- 双版本：维护两套栈不值

---

## ADR-06 商业模式

**Status**：**Revised by ADR-24**（jcolour 主档 $199.99/年 + 永久免费看图档 + Early Access $99/年终身锁定 + v1.5 Pro $399/年；gcolour 定价待 v1.0 后定，预期 $499–999/年区间）
**Date**：2026-04（修订 2026-05）

**Decision**  
**Freemium + 年订阅 $199.99/年**：
- 永久免费：查看 ICC / 打印 / G7 验证报告预览 / Epson Print Layout 全功能对等 + 微增强
- 付费解锁：自制 ICC / G7+ 校准 / 专色闭环 / 验证报告导出 / 批量 > 10 张 / 云端 Profile 额度

**Consequences**  
- 免费版触达所有 P7500/P9500 用户，转化入口大
- $199.99 对 Adobe CC 被驯化的用户心理门槛低
- LTV 远高于一次性买断

**Alternatives**  
- 一次性 $199：现金流弱、无长期服务支撑
- 纯付费墙：获客成本暴涨

---

## ADR-07 ICC 生成在云端

**Status**：Accepted
**Date**：2026-04

**Context**  
MAS 沙盒禁止嵌入 GPL 二进制（Argyll 是 GPL v3）；商业 Profile SDK Runtime License 年费 $10k+，$199 定价每卖一份都亏钱。

**Decision**  
**ICC 生成走云端**：客户端上传测量数据 → 云端 Docker 隔离 Argyll 子进程 → 返回 ICC profile。

**Consequences**  
- 沙盒干净（客户端不嵌 Argyll）
- GPL 合规（SaaS loophole）
- 云端算力可控
- 必须在线：无网不能生成 ICC（G7 校准本地可做，见 ADR-08）
- 云端成本：每次 ICC 生成 $0.01–0.05

**Alternatives**  
- 嵌入商业 SDK：$199 定价亏钱
- 自研 ICC 生成：3–5 人 2 年
- 让用户自装 Argyll：MAS 不允许

---

## ADR-08 G7+ 引擎本地计算

**Status**：Accepted
**Date**：2026-04

**Decision**  
G7+ 引擎完全本地计算，不走云端。

**Consequences**  
- 本地 G7 校准无需联网
- 响应即时（测完 5 秒出曲线）
- 算法可审计
- 需要我们自己实现 G7+ 数学（非 Argyll 现成）

**Alternatives**  
- 云端算 G7：无网时基本校准都不能做

---

## ADR-09 G7 与 ICC 强耦合

**Status**：Accepted
**Date**：2026-04

**Decision**  
**强耦合**：官方向导 "G7 校准 → ICC 生成" 是唯一路径。

**Consequences**  
- 产品主张清晰："G7+ 是标配不是选配"
- 防 Fiery 常见"选错工作流"坑
- 和 Calibration Session 状态机契合

**Alternatives**  
- 允许只做 ICC：与产品主张冲突
- 允许只做 G7：已作为"G7 Only 模式"保留在大色域线

---

## ADR-10 双产品线

**Status**：**Superseded by ADR-24**（拆为两个独立 macOS App + 共享 C++ engine/ 库；不再是单产品双产品线）
**Date**：2026-04（推翻 2026-05）

**Decision**  
首次启动分流为两条产品线：
- **大色域线 / RGB 颜色漂亮**：艺术摄影 + 精品喷绘 + 高端视觉陈列（ADR-19 扩展）
- **精确色线 / CMYK 颜色准确**：印刷打样 + 品牌色彩

两条线 UI、术语、工作流全部差异化，共用底层引擎（G7+、光谱测量、Kubelka-Munk、蜂巢 UI）。

**Consequences**  
- 两类用户各得其所，不互相打扰
- 底层复用度高，UI 层分叉
- 用户可切换产品线

**Alternatives**  
- 单一工作流：摄影师觉得复杂，打样师傅觉得不专业
- 两个独立 App：MAS 审核/订阅成本翻倍

---

## ADR-11 G7 验证报告最小实用

**Status**：Accepted
**Date**：2026-04

**Decision**  
**最小实用版**：jcolour 品牌页眉 + PrUA attribution + 分项 pass/fail 表 + 总体合规判定。不做模板化、不做多品牌、不做自定义字段。

**Consequences**  
- 工期短，v1.0 可发布
- 满足 99% 打样客户交付需求
- v1.5 再考虑模板化

---

## ADR-12 Black Generation v1.0 基础

**Status**：Accepted
**Date**：2026-04

**Decision**  
v1.0 提供 **4 个基础参数**：Black Start / Black Max / GCR Amount / UCR。Advanced 抽屉默认折叠，容纳 Black Width、Purity 等高阶参数。

**Consequences**  
- 专业用户"打开 ICC 向导就能找到黑版控制"
- v1.0 UI 整洁
- v1.5 可扩展到 Argyll 全部黑生成参数

---

## ADR-13 技术栈

**Status**：Accepted
**Date**：2026-04

**Decision**  
- 客户端：C++17 + Qt 6 + Little CMS 2 + libtiff + X-Rite i1 SDK
- 云端：Go + PostgreSQL + Redis + Cloudflare R2 + ArgyllCMS（Docker 子进程）
- 构建：CMake 3.20+

**Consequences**  
- Qt 6 LGPL 可商业闭源（动态链接）
- LCMS2 MIT 无顾虑
- Argyll GPL 只在云端 → SaaS loophole
- i1 SDK 需商业授权

---

## ADR-14 MVP 工期

**Status**：Accepted
**Date**：2026-04

**Decision**  
**11–13 个月**到 v1.0 MAS 上线（详见 `ROADMAP.md`）。

**Consequences**  
- 假设：1–2 人全职 + 0.5 人兼职后端
- 若增加 v1.5 完整专色闭环范围：+3–4 个月

---

## ADR-15 专色闭环分期

**Status**：Accepted
**Date**：2026-04

**Decision**  
分期实施：
- **v1.0**：基础版 = 专色库 + 单次光谱预测 + 手动蜂巢微调
- **v1.5**：完整多次自动迭代闭环
- **v2.0**：考虑 Pantone 商业授权

**Consequences**  
- v1.0 即可立住"专色修正"卖点
- v1.0 → v1.5 升级路径清晰，续费动力强
- v1.0 工期可控

---

## ADR-16 蜂巢专色 UI

**Status**：**Revised by ADR-24**（拆分后 jcolour 仅实现 Mode A 视觉分形；gcolour 仅实现 Mode B 光谱驱动；组件可共享但 UI 简化为各自专属）
**Date**：2026-04（修订 2026-05）

**Decision**  
**同一个六边形蜂巢组件，双模式**：
- **模式 A 视觉分形**（肉眼 + 逐级放大）：7 格铺色 → 点一格放大 → 再 7 格细分 → 3–5 级深入。适合大色域线、无仪器场景。
- **模式 B 光谱驱动**（i1 扫描 + 曲线匹配）：中心 = 实测；6 向邻格 = 光谱曲线形状的补偿方向；用户点一格 = 选下一轮迭代方向。适合精确色线、有仪器场景。

**Consequences**  
- 单一 UI 组件两种数据驱动，代码复用度高
- 独家 UX，行业内无对标
- 视觉差异化强，营销素材

---

## ADR-17 颜色数据契约：全程光谱

**Status**：Accepted（**项目技术宪法**）
**Date**：2026-04

**Context**  
业内常规做法是测量后立即压 Lab 存/传/比较，导致同色异谱（metamerism）无法处理。GMG OpenColor 是唯一保留光谱全程的顶级对手。

**Decision**  
**全系统颜色数据契约**：
- 内部存储、比较、优化、配方匹配一律用**光谱反射率曲线**（36 通道，380–730nm @ 10nm）
- Lab / ΔE / XYZ 仅作**UI 显示的派生数据**，**不参与任何计算决策**
- 油墨混合模型用 **Kubelka-Munk 光谱版**
- ΔE 保留作用户熟悉的"报告数值"，但**内部不用于优化目标**
- 专色匹配用 **光谱 R.M.S.** 或 **ISO 13655 Spectral Match Index**
- 专色库必须存光谱（HKS 有，Pantone 新版有，老色库首次测量建档）

**Consequences**  
- 技术深度达 GMG OpenColor 同档，4 家主要对手一出场就被比下去
- 存储空间 ×10（36 通道 vs 3 数字），但数据量仍小
- 计算复杂度上升，性能工程重要
- i1Pro3 天然输出光谱，硬件零差
- 整个项目代码库都要按这个契约设计，**不允许中间某层偷偷压 Lab 再算**

**Alternatives**  
- 纯 Lab：工程简单，但永远做不到同色异谱控制
- 可切换模式：复杂度翻倍，收益不值

---

## ADR-18 RGB 专色修正的影响范围

**Status**：Accepted
**Date**：2026-04

**Context**  
大色域线的摄影/喷绘用户从图片框选专色目标做修正时，硬边切割的修正会在修正区和非修正区之间形成色彩断层，视觉不自然。但完全羽化又会牺牲修正精度。

**Decision**  
专色修正后暴露一个**影响范围滑块**：
- 滑块刻度从"**窄**"到"**宽**"
- 窄端：以专色目标为中心，只修正光谱距离极近的像素，**颜色准**
- 宽端：修正区域在色彩空间里按高斯/余弦函数衰减延伸到相邻色，**过渡好**
- 计算上：像素的修正强度 = `exp(-spectral_distance² / (2·radius²))`，radius 由滑块控制

**Consequences**  
- 摄影师"只改樱花粉、背景绿不动"（窄）和"整张调子统一地往樱花方向拉一点"（宽）两个场景都能用
- 打样线不提供此滑块（打样需要硬边精确，不允许羽化）
- 实现复杂度中等（光谱空间的加权函数）

**Alternatives**  
- 不提供范围控制：摄影师会觉得修正"硬"
- 多个预设（极窄/窄/中/宽/极宽）：不连续，用户难以微调

---

## ADR-19 产品边界扩展到精品喷绘

**Status**：Accepted
**Date**：2026-04

**Context**  
原 PLAN 定位明确排除"大幅面喷绘 RIP"市场。但经过竞品调研发现：ONYX/Caldera/Wasatch 都是"工业喷绘取向，色彩精度不够"；Fiery/GMG 是"打样取向，幅面/速度不适合喷绘"；Mirage/ImagePrint 是"艺术摄影向，幅面受限"。**"对色彩有要求的精品喷绘"这个细分市场在现有产品里没有合适答案**。

**Decision**  
扩展产品边界到**精品喷绘**，但明确排除**工业喷绘**：
- **包含**：艺术展览大图、博物馆复制品、摄影展大图、高端零售视觉陈列、电影/设计公司提案大图
- **排除**：车贴、展板、常规广告大图、纺织印花（这些归 ONYX/Caldera/Wasatch 市场）

技术上，精品喷绘和艺术摄影共享"大色域线"产品线，共用硬件（P7500/P9500）、共用引擎。UX 上产品线内部不再细分，但营销文案明确覆盖三细分。

**Consequences**  
- TAM 显著扩大（精品喷绘市场虽然比全喷绘小，但大于艺术摄影单细分）
- 不和 ONYX/Caldera 正面竞争，避开工业赛道
- 营销素材需要"博物馆复制"、"摄影展大图"这类垂直案例
- 硬件范围暂不扩到 SC-T/S 系列（工业喷绘机）

**Alternatives**  
- 维持只做艺术摄影 + 打样：放弃一个明显的市场空白
- 扩到全喷绘（含工业）：需要支持 SC-S/T、Roland、Mimaki，硬件矩阵爆炸

---

## ADR-20 拼版模块：自动 + 手工 + 裁切一体

**Status**：Accepted
**Date**：2026-04

**Context**  
拼版是大色域线用户（喷绘 + 艺术摄影）日常高频操作，也是精确色线校色向导内的便利。市场上：ONYX/Caldera 自动强但 UX 陈旧 + Win only；Mirage/ImagePrint 手工友好但没 True Shape 自动；没有产品做到"Mac 原生 + 现代 UX + 自动/手工一体 + 色彩闭环绑定"。

**Decision**  
v1.0 拼版模块**一体化自动 + 手工**，Figma 级手工精修 + libnest2d True Shape 自动嵌套。**分两期发布**：

### Early Access（M0–M12）
- 手工拼版：画板 + 拖拽 + 对齐线 + 分布 + 基础变换（Mirage 级 → Figma 级过渡）
- 矩形自动紧排（bottom-left fill）
- 色靶图自动拼版（规则型，校色向导内）
- 打样简化 Imposition（4up / 8up / 基础折页）
- 本机裁切准备（见 ADR-21）
- 测试打印工作流（见 ADR-23）

### 公开发布 v1.0（M12–M15）
- True Shape Nesting（libnest2d + Clipper）
- Tiling 大图分块（60"+ 输出用 44" 机器拼出）
- Step & Repeat（图案重复）
- 批量联系表（摄影师看片、画廊存档）
- 光谱驱动耗墨预估（基于 ADR-17 + Kubelka-Munk）
- 外部裁切机准备完整版（见 ADR-21）

### v1.5 Pro（M15–M18）
- 主体保护 AI（Core ML 人脸/主体识别，避让裁切条）
- 专门的艺术家画廊预设

**UX 核心创新**："**AI 建议 + 一键接受 / 一键精修**"三路径：
- 系统自动 True Shape 拼版 + 耗墨预估 → 给"建议方案"
- 用户路径 A：一键接受直接打印（90% 情况）
- 用户路径 B：进入手工精修（保留自动结果作起点）
- 用户路径 C：换策略重跑（紧排优先 / 构图优先 / 成本优先）

**Consequences**  
- 工期：总计 4.8–6.8 个月模块工期；并行化后净增 MVP **+3.0–4.0 个月**
- 拼版是两条产品线共用的核心引擎，开发投入 ROI 高
- UX 差异化强（行业里无此集成度）
- 主体保护 AI 和耗墨光谱预估是行业独家

**Alternatives**  
- 纯手工：放弃 True Shape 自动价值（15–30% 省纸）
- 纯自动：艺术摄影用户会走掉
- 拼版外包给 Mirage 等现有工具：破坏单软件一站式的产品承诺

---

## ADR-21 裁切工作流：本机 + 去外部切割机

**Status**：Accepted
**Date**：2026-04

**Context**  
精品喷绘、视觉陈列、艺展大图这三个细分客户**几乎都会用外部切割机**（Summa / Graphtec / Roland）。打印和切割分软件是行业常态但用户体验割裂。市场上 ONYX Cut Server / Caldera PrimeCenter / VersaWorks / Graphtec Studio 都做，但没有一个 Mac 原生 + 现代 UX。

**Decision**  
裁切工作流作为拼版模块的下游输出能力，**两种路径**：

### 路径 1：本机 / 桌面裁切
- 裁切线（crop marks）
- 切角标记（corner marks）
- 对位线（registration marks）
- 出血边（bleed，用户可配置 0–10mm）
- **Epson 打印机自动裁刀 PJL 命令封装**（P9500 支持的卷纸自动裁切）
- Early Access 就做

### 路径 2：去外部裁切机
- **OPOS 套准点**（Summa 标准）
- **ARMS 套准点**（Graphtec 标准）
- **CutContour 专色层**识别和处理（PANTONE CutContour / Roland CutContour 通用）
- **工单二维码**（大批量流水追踪，可选）
- **切割文件导出**：PDF/X 带 CutContour 专色层（行业事实标准，所有切割机能读）
- 公开发布 v1.0 阶段做

### v1.5 考虑
- 直连切割机（RS232/USB/网络发送 HPGL/PLT）
- 切割进度跟踪
- 切割机驱动集成

**Consequences**  
- 精品喷绘市场能真正落地（之前断腿，裁切是下游必备）
- 工期 +0.5–0.8 个月
- 不直连切割机可避免集成 10+ 种切割机协议的工程负担

**Alternatives**  
- 不做裁切：精品喷绘客户去用别的软件做切割，工作流割裂
- 全做（含直连）：工程代价过大（切割机协议 ≠ 统一标准）

---

## ADR-22 三段式发布策略

**Status**：Accepted（取代 ADR-14 的单一 MVP 工期定义；ADR-24 拆分后 jcolour 走完整 M0–M18，gcolour 在 jcolour M15 后启动）
**Date**：2026-04（适用范围调整 2026-05）

**Context**  
累积的所有决策（ADR-01 到 ADR-21）导致 MVP 全配工期扩到 14–17 个月，远超 ADR-14 原定的 11–13 个月。必须决定：要么砍功能要么分阶段。砍功能会破坏产品主张；分阶段能保住完整性同时缩短反馈循环。

**Decision**  
**三段式发布**，取代单一 MVP 目标：

| 阶段 | 时间 | 范围 | 分发 | 定价 |
|------|------|------|------|------|
| **Early Access** | M0–M12 | 核心引擎 + 基础拼版 + 色靶图 + 本机裁切 + 基础专色 + 测试打印 | 闭门 20 位种子用户 | $99/年（终身锁定） |
| **v1.0 公开发布** | M12–M15 | +True Shape + Tiling + Step&Repeat + 耗墨预估 + 外部切割机 + 蜂巢模式 B + G7 报告导出 | Mac App Store | $199.99/年 |
| **v1.5 Pro 升级** | M15–M18 | +主体 AI + 多次迭代专色闭环 + Imposition + 模板化报告 + P10000/P20000 支持 | MAS in-app upgrade | $399.99/年 Pro 档 |

**Consequences**  
- **M12 就有产品在真实用户手里**，反馈循环缩短 3–5 个月
- Early Access $99 提供早期现金流 + 种子用户口碑
- v1.0 公开上线时已是完整体
- v1.5 升级驱动订阅续费和档位跃升
- 若 Early Access 发现方向错，还有 3 个月纠偏余地
- 风险对冲比一次性 17 个月发布强得多

**Alternatives**  
- 一次性 14–17 个月发布：资金压力大，反馈循环长
- 砍功能保 11–13 个月：破坏产品主张，失去技术护城河
- 更激进分期（M6 Early Access）：核心引擎 + 光谱契约 6 个月做不完

**Supersedes**：ADR-14（MVP 11–13 个月的工期口径改为三阶段）

---

## ADR-23 测试打印工作流：缩小样 + 区域原大

**Status**：Accepted
**Date**：2026-04

**Context**  
大图打印成本高（20"+ 一张几十到几百元纸+墨+时间），直接正打是业余做法。摄影师/艺术家的日常工作流：先打缩小样看整体，再框选关键区域（人脸/肤色/花瓣）打 1:1 原尺寸样，两样都通过再正打。Mirage、ImagePrint 都有此功能，但 UX 陈旧；Epson Print Layout、Lightroom、Photoshop 只有部分。

**Decision**  
在两条产品线的打印向导里内嵌**测试打印模式**，三种输出：

1. **缩小样**：整图按 1:4 / 1:8 / 自定义比例缩小打
2. **区域原大**：用户橡皮筋框选 → 该区域以 1:1 原尺寸打
3. **两者同纸**：缩小样 + 区域原大拼在同一张 A4/A3 上

UI 集成：
- 打印对话框里的 "测试打印" 切换
- 图像预览区直接拖框选区
- 实时显示预估耗纸/耗墨/时间
- 复用拼版模块的画板和光谱耗墨预估

**Consequences**  
- 工期 +0.3 个月
- 日常高频功能，Early Access 就要有
- 大图用户节省 60–80% 试样成本
- 和拼版模块共用画板组件，代码复用度高

**Alternatives**  
- 让用户用 Photoshop 裁剪后再打：工作流割裂
- 只做缩小样，不做区域原大：用户打过正样后发现肤色有问题就再也不打样了

---

## ADR-24 产品拆分：jcolour + gcolour 两个独立 macOS App

**Status**：Accepted
**Date**：2026-05

**Context**
ADR-10 原本设计为"单一产品双产品线"（大色域线 + 精确色线共用底层 + UI 分叉）。深入到具体功能与用户画像后，发现两条线的：
- 用户购买决策周期差异巨大（摄影师个人 1–7 天 vs 打样 B2B 1–3 个月）
- 营销渠道完全不重叠（摄影社区 vs 行业展会）
- 文档/教程/案例分支化严重，影响产品聚焦
- 单 App 内"两条线切换"会让两边用户都觉得复杂

**Decision**
拆为两个独立 macOS App + 共享 C++ 引擎库：
- **jcolour** = RGB / 摄影 / 大色域线（**先做**，目标用户：摄影师打印自己照片）
- **gcolour** = CMYK / 打样 / 精确色线（jcolour v1.0 公开后启动）
- **engine/** = 共享 C++ 库（spectral / g7 / icc / kubelka_munk / i1sdk / pjl / nesting / cutting / test_print）
- 两个 App 各自上架 MAS（独立 Bundle ID `com.jcolour.app` / `com.gcolour.app`）
- 各自独立订阅、独立定价、独立营销

**Consequences**
- 引擎层完全复用，避免重复造轮子
- UI 层完全分离，各自定位清晰
- MAS 审核工作量翻倍（但每个 App 体量小、变化频率低）
- 用户可能同时购买两个，未来可考虑 "Suite Bundle" 折扣
- jcolour 验证产品力 + 摄影师社区口碑可帮 gcolour 树立信任
- B2B 打样市场在 jcolour 先证明引擎质量后，销售容易得多

**Alternatives**
- 维持单 App 双线（ADR-10）：用户切换成本高、文档分支难管
- 做两个完全独立的 App（无共享）：引擎重复实现 6 倍工作量
- 只做 jcolour 不做 gcolour：放弃 B2B 打样市场（市值更大但启动慢）

**Supersedes**：ADR-10（双产品线）
**Triggers Revision**：ADR-01 (目标用户) / ADR-05 (分发) / ADR-06 (商业模式) / ADR-16 (蜂巢 UI) / ADR-22 (发布范围调整)

---

## ADR-25 jcolour 用户聚焦：仅"摄影师打印自己照片"

**Status**：Accepted
**Date**：2026-05

**Context**
ADR-01 原本定义三细分（打样师傅 / 艺术摄影师 / 精品喷绘）。ADR-24 拆分后 jcolour 范围进一步收窄。如果不明确用户画像，会再次发生功能膨胀，为画廊、婚摄、打印店等服务型业务做妥协，导致 jcolour 失焦。

**Decision**
**jcolour v1.0 唯一目标用户：摄影师打印自己作品**（Sam, the printing photographer）。

**明确包含**：
- 独立艺术摄影师（个人创作 + 限量版销售）
- 半专业摄影爱好者（自存档 + 偶尔卖片）
- 摄影教师（教学 + 个人作品）

**明确排除**（属于 gcolour 或 v1.5+ 评估）：
- ❌ 画廊代客打印（服务业）
- ❌ 婚摄/商业写真出片（批量服务）
- ❌ 印前打样、CMYK 转换、GRACoL/Fogra 对标（gcolour）
- ❌ 品牌色彩部门、专色配方（gcolour）
- ❌ 工业喷绘（车贴、展板、纺织）
- ❌ Mutoh / Canon / HP 等非 Epson 打印机（v1.5 评估）

**Consequences**
- 功能优先级清晰：B&W 双模式、广色域映射、Edition Tracking 优先
- 拒绝功能膨胀：任何"为画廊批量发货"、"为婚摄批价计算"提案直接拒绝
- 营销话语聚焦：摄影圈语言（giclée / fine art / OBA-free），不沾沾 GRACoL
- TAM 看似变小，但客户付费转化率提升（聚焦人群信任度高）
- gcolour 在 v1.0 后独立服务 B2B，互不打扰

**Triggered By**：ADR-24

---

## ADR-26 Epson PJL 直连（绕开 macOS Print Dialog）

**Status**：Accepted（依赖 Spike 2 验证）
**Date**：2026-05

**Context**
摄影师对 Epson 打印的最大抱怨集中在：
- macOS Sonoma/Ventura 下保存的预设丢失
- ColorSync 与 Epson 驱动双重色彩管理冲突
- AirPrint 与专用驱动冲突
- 16-bit 输出在系统对话框中精度损失
- 单条墨道控制不暴露（B&W 双模式需要这个能力）

详见 [`apps/jcolour/EPSON-COLOR-PROBLEMS.md`](../apps/jcolour/EPSON-COLOR-PROBLEMS.md) 第四章。

通用解法是用系统 macOS Print Dialog 包装一层 UI，但这种解法治标不治本，无法解决核心痛点。

**Decision**
**jcolour 走 PJL 直连方案**：
- 通过 CUPS / IPP raw mode + Epson PPD 文件 + PJL 命令直接与打印机通信
- 不调用 macOS Print Dialog
- 自己实现色彩管理链路（ICC + 渲染意图 + BPC）
- 自己实现保存预设的存储与读取（在应用沙盒内）
- 全程 16-bit 数据通路
- 控制每条墨道独立输出（B&W 双模式必要前提）

**Consequences**
- 解决预设丢失、ColorSync 双管理、16-bit 精度损失问题
- 启用 B&W 单黑模式（能强制其他墨道为 0）
- 启用 Per-ink 线性化（能控制每条墨道）
- 启用与 ABW/QuadToneRIP 真正差异化的能力
- 实施风险：macOS 版本升级可能破坏 PJL 兼容（需要 macOS Beta 跟进）
- 沙盒风险：MAS 审核可能对 PJL 直连有意见（需要提前与 Apple Developer Relations 沟通）
- 工程复杂度：必须自己处理所有 macOS 不再帮忙的部分（任务队列、错误处理、状态回读）

**Spike 2 必须验证此方案技术可行性**。如果 Spike 2 红线 → 回退到 macOS Print Dialog 包装方案，但接受功能降级（失去单黑模式、失去 16-bit 全程保证）。

**Alternatives**
- macOS Print Dialog 包装：UI 上能做、但解决不了核心痛点
- AppleScript 自动化系统对话框：脆弱、无法做单墨道控制
- 自研 USB 协议：Epson 不公开协议，逆向工程不可持续

**Triggered By**：ADR-24 + 用户调研（详见 [`apps/jcolour/EPSON-COLOR-PROBLEMS.md`](../apps/jcolour/EPSON-COLOR-PROBLEMS.md)）

---

## ADR-27 B&W 双模式：单黑 + 多色黑

**Status**：Accepted
**Date**：2026-05

**Context**
摄影师对 B&W 工作流的需求强烈：
- Epson ABW 4 个色调预设不够，无法分区调色温
- QuadToneRIP $50 学习曲线极陡，60% 用户下载后放弃
- 单黑（仅 PK 或 MK，纯碳黑）和多色黑（LK+LLK+主黑+微量彩墨）各有不可替代场景
- ImagePrint / Mirage 等竞品 B&W 工作流均较弱

详见 [`apps/jcolour/BW-WORKFLOW.md`](../apps/jcolour/BW-WORKFLOW.md)。

**Decision**
jcolour v1.0 同时支持两种 B&W 模式，UI 顶部一个分段开关切换：

**Mode 1：Single Black（单黑打印）**
- 仅使用 PK 或 MK 单一墨水
- 通过半色调算法（Stochastic / Error Diffusion）控制墨点密度
- 优势：无横向色偏、无同色异谱、最深 Dmax
- 适合：极致中性、档案级、Ansel Adams 风格
- UI：黑墨选择 + 微调色温 + 半色调算法

**Mode 2：Multi-channel Black（多色黑打印）**
- PK/MK + LK + LLK + 微量 CMY 补色温
- 三段色环（高光 0–25% / 中间调 25–75% / 阴影 75–100%）独立调色温
- 实现 split-tone（高光暖、阴影冷之类胶片色调）
- 对标 QuadToneRIP 但学习曲线降低 10 倍

**共用**：
- Per-ink 线性化（PK/MK/LK/LLK 至少四条墨道，v1.5 全 12 通道）
- G7+ 灰阶向导
- 通过 ADR-26 PJL 直连实现单墨道精确控制

**Consequences**
- 替代 Epson ABW 与 QuadToneRIP 双重市场
- B&W 是摄影师细分核心痛点，差异化能力强
- UI 复杂度可控：单一开关切换避免选择困难
- 工期影响：Spike 5 验证 UI 易用度，再投入 v1.0 完整开发
- v1.0 仅做基础四通道线性化；v1.5 扩展到全 12 通道 + 灰阶曲线编辑器（25 控制点）+ 模拟胶片预设

**Alternatives**
- 仅做 ABW 增强版：失去 QuadToneRIP 用户群
- 仅做多色黑：失去档案级单黑用户
- 沿用 ABW + 提供高级"曲线编辑器"暴露所有控制：UX 复杂，回到 QuadToneRIP 的劝退路径

**Triggered By**：ADR-25 + 用户明确要求（"黑白照：单黑打印和多色黑打印，加大色域颜色"）

---

## ADR-28 Mutoh 推迟到 v1.5

**Status**：Accepted
**Date**：2026-05

**Context**
用户曾提出"加上武藤（Mutoh）用户的需求"。深入调研后发现：
- Mutoh 用户 60%+ 是标牌/喷绘行业（生态溶剂打印机：ValueJet 1604/1638/1948，XpertJet 1641SR/1682SR）
- Mutoh 水性墨水线（XpertJet 1682WR / 1642WR Pro）使用 DH21 **dye-sublimation** 墨水（热升华转印用），**不适合艺术纸**
- Mutoh 水性颜料款型号极少（XpertJet 个别水性 SR 型号）且摄影师占比极低
- 摄影师圈用 Mutoh 做艺术打印的比例 < 5%（vs Epson 70%+ / Canon 15–20% / HP/其他 < 10%）
- 地理分布：Mutoh 在亚洲（日本、韩国、中国大陆）较强，但即便在亚洲摄影圈仍以 Epson 为主流
- macOS 生态：Mutoh 自家 RIP（VerteLith）仅 Windows；macOS 用户依赖 Caldera 或 Fiery XF（均与 jcolour 定位/价位不重叠）

**Decision**
**v1.0 不做 Mutoh，推迟到 v1.5 重新评估**。

v1.5 评估时考虑：
- 是否进一步聚焦个别 Mutoh 水性颜料款（如 XpertJet 1641WR）
- Canon imagePROGRAF PRO 优先级是否更高（用户基数大于 Mutoh 摄影圈）
- 评估前先在 v1.0 摄影师社区调研：是否真有 Mutoh 用户在等
- 如果 v1.0 公开后只有 < 50 位 Mutoh 摄影师反馈，则永久搁置

**Consequences**
- v1.0 工程聚焦 Epson 一家，工期与质量更可控
- 不冒险投入 3–6 个月开发 Mutoh 兼容（PJL 命令族、PPD、墨道映射全要重做）
- 摄影师社区不会因 Mutoh 缺失流失（基数太小）
- v1.5 时机更成熟：用户社区起来后能真实判断需求

**Alternatives**
- v1.0 同时支持 Epson + Mutoh：3–6 个月额外工期、ROI 极低
- v1.0 仅 Mutoh 颜料款（XpertJet 个别）：调研显示具体型号待确认，风险大
- 永远不做 Mutoh：可能遗漏部分亚洲市场，但 Canon 替代价值更大

**Triggered By**：ADR-25 + 用户调研（详见研究报告中 Mutoh 章节）

---

## ADR-29 广色域映射分层架构（GamutAdvisor + SemanticGamutMapper）

**Status**：Accepted
**Date**：2026-05

**Context**
[`apps/jcolour/PLAN.md`](../apps/jcolour/PLAN.md) §3 P0 列了"广色域映射引擎"作为 jcolour 充分调用 P9500 PRO 12 油墨族（Or/Gr/V）的核心卖点，但概念笼统，未定义算法、责任分层、与 ICC 的边界。

[`apps/jcolour/EPSON-COLOR-PROBLEMS.md`](../apps/jcolour/EPSON-COLOR-PROBLEMS.md) §10.7 列了"自动化十条"，散落涵盖渲染意图智能推荐、BPC 决策、ProPhoto 强制、Or/Gr/V 调用率 meter，但没合成执行级规范。

讨论中确认三个事实：
1. 单一 ICC profile + 全局固定 intent 不能充分用满 PRO 12 色域，因 ICC 的 4 张 intent 表是对"平均图像"的最优，不针对具体图像内容
2. 全局直方图分析（仅看饱和度分布）无法区分主体（脸 / 蓝天 / 记忆色）与背景，必然在某些图上推错 intent
3. 业界对此问题的成熟方案是局部分区 + 用户参与决策，更前沿是 IDGM（图像依赖 gamut mapping），但 IDGM 研发成本极高且 ΔE 改进对摄影师感知不显著

**Decision**
广色域映射拆为分层架构，按层独立设计、独立交付：

| 层 | 方法 | 角色 |
|---|---|---|
| **L1** | 高质量 ICC（colprof -qh -d3 -kp，33³ LUT，全 12 通道靶） | 地板。由 [`PIPELINE.md`](../apps/jcolour/PIPELINE.md) §1-§6 交付 |
| **L2** | **GamutAdvisor**：基于直方图（饱和度 + 出色域比例 + 记忆色掩码）推荐 2 个 intent 候选 | 自动化辅助 |
| **L3** | **A/B 软打样**：屏幕同时渲染两版 + Paper White Simulation，用户拍板 | 用户参与决策 |
| **L4** | **SemanticGamutMapper**：CoreML 语义分割（肤色/天空/植物/主体/背景）+ 各区独立 intent + 强度 | 差异化护城河 |
| L5 | IDGM 图像依赖映射 | v2.0+ 研究，**本 ADR 不做** |
| L6 | 光谱 + Kubelka-Munk 直接合成 | gcolour 范畴（ADR-15 / ADR-17），**本 ADR 不做** |

**责任分层**：
- 客户端 jcolour app：GamutAdvisor / A/B 软打样 UI / 屏幕色域守门 / SemanticGamutMapper / Or/Gr/V 调用率 meter
- engine 共享库：LCMS2 transform（高质量 flag）、intent 应用、分区 mask 加权
- ICC profile：保持纯净 device profile，不嵌语义信息

**架构原则**：
1. 不在 ICC 层做语义映射，语义映射在 ICC transform **之前**（ProPhoto 空间内做局部预调整 + 选 intent）
2. 不重写 perceptual table，使用 Argyll colprof 默认 perceptual
3. A/B 软打样是合同：jcolour 永不替用户决定 intent，只缩小到 2 个候选
4. L4 失败降级到 L2/L3（CoreML 置信度 < 0.7 时退回全局 intent）

**算法、参数、UI 契约、验证矩阵**详见 [`apps/jcolour/PIPELINE.md`](../apps/jcolour/PIPELINE.md) §7-§9 与 [`apps/jcolour/GAMUT-PIPELINE.md`](../apps/jcolour/GAMUT-PIPELINE.md)。

**Consequences**
- jcolour 在"用满 P9500 大色域"这个核心卖点上有可执行设计，不再悬空
- L4 SemanticGamutMapper 是相对 ImagePrint / Mirage / QTR 的差异化护城河（竞品都没把分区 intent 自动化）
- L2/L3 在 EA/v1.0 阶段即可上线，立即让用户感知 Or/Gr/V 调用率提升
- 工程复杂度：L4 引入 CoreML 依赖 + 模型评估流程；L2 算法在 Spike 1 范围内验证
- ICC profile 保持纯净，跨 jcolour / gcolour / 第三方应用兼容性不受影响

**Alternatives 拒绝**
- 单一 IDGM 引擎：研发成本极高，ΔE 改进 < 1，对摄影师感知不显著，ROI 低
- 纯 ICC 优化（更多测靶 / 更细 LUT / 更优 perceptual table）：天花板低，已 ≈ Argyll colprof -qh 极限
- 纯直方图自动化（无 A/B 软打样）：必然在记忆色场景下做错决定（如把暗调人像的红衣衬衫推到 Saturation 致脸色失真）
- 在 ICC 层嵌入语义信息：破坏 ICC 标准兼容性，违反 device profile 纯净原则

**与既有 ADR 关系**
- ADR-09（G7 与 ICC 强耦合）：ADR-29 衔接 ICC 之后到打印之前的映射阶段，不替代 G7→ICC 唯一路径
- ADR-17（全程光谱契约）：ADR-29 在 PCS（Lab）层工作，不涉及光谱；光谱方案保留给 gcolour
- ADR-13（技术栈：LCMS2）：ADR-29 强化 LCMS2 关键 flag 的使用规范
- ADR-26（PJL 直连）：ADR-29 假设 16-bit 全程贯通由 PJL 直连保证，二者前后衔接

**风险与缓解**
| 风险 | 缓解 |
|---|---|
| CoreML 把皮肤识别成衣物 | 用户可手动调整 mask；置信度低自动降级 L2/L3 |
| GamutAdvisor 推错 intent | A/B 软打样兜底，用户最终拍板 |
| 屏幕色域不够导致软打样不可信 | 强制检测显示器 ICC，色域 < Display P3 时禁用软打样 |
| Or/Gr/V 调用率 meter 数据失真 | 验证：饱和图基线 > 15%、自然图 5–15%（[`PIPELINE.md`](../apps/jcolour/PIPELINE.md) §8 验证指标） |

**Triggered By**：ADR-25 + 用户多轮讨论确认 P9500 大色域调用是核心卖点；详见 [`PIPELINE.md`](../apps/jcolour/PIPELINE.md) §0 概览

---

## ADR-30 每打印模式的线性化方法（单黑走 SCTV）

**Status**：**Accepted**
**Date**：2026-06

**Context**  
校准集对 5 种打印方式各自线性化。单黑（WF1：照片黑 / 碳黑 / 消光黑）若用传统密度法（status T/E），在消光与碳黑墨上密度读数不可靠、暗部压缩，得到的阶调不符人眼感知。

**Decision**（职责分工）  
- **感知线性化（中性）一律用 SCTV（Spot Colour Tone Value, ISO 20654）**：涵盖 **WF1 单黑、WF2/WF3 的中性黑**线性化。SCTV 以 CIELAB 计算 substrate→tint→solid 的阶调值，对中性近似等明度(ΔL*)步阶、并以实测纸白/solid 正规化 → 感知均匀且稳健（消光/碳黑可靠）。目标：使测得 SCTV 对输入百分比线性。
- **密度（status T/E）只用于「墨量上限」**（找溢墨/青铜化前的最大墨量），不再当线性化目标。
- **彩色 / 灰平衡 / 最终色由 G7 + ICC 负责**（RGB / 16-bit RGB 模式与彩色辅助通道），不靠单通道密度。
- L* 视为 SCTV 的简化近似，不单独采用（SCTV 对完美中性≈正规化 L*，但更稳）。

**Consequences**  
- 引擎 calibration 需新增「线性化方法」参数与 SCTV 计算单元（目前引擎无 SCTV/ISO 20654）。
- 自动校准集：WF1/WF2/WF3 线性化子步骤用 SCTV；墨量上限子步骤用密度；RGB/16-bit 走 G7+ICC。
- 自动校验（比对线性+G7）须按同法（SCTV）重算线性化。

**Alternatives**  
- 全模式统一密度法：被否，消光/碳黑不可靠。
- 全模式 L*：接近 SCTV 但未按 ISO 20654 规范，且彩色模式不适用。

---

## 索引

| 编号 | 决策 | 状态 |
|------|------|------|
| ADR-01 | 目标用户 | **Revised by ADR-25** |
| ADR-02 | 目标硬件：Epson P7500/P9500 | Accepted（jcolour 扩展含 P700/P900）|
| ADR-03 | 平台：macOS 13+ Universal | Accepted |
| ADR-04 | 测量仪：X-Rite i1Pro3 | Accepted |
| ADR-05 | 分发：Mac App Store | **Revised by ADR-24**（两个 App 独立上架）|
| ADR-06 | 商业模式：Freemium $199.99/年 | **Revised by ADR-24** |
| ADR-07 | ICC 生成：云端 Argyll | Accepted |
| ADR-08 | G7+ 引擎：本地 | Accepted |
| ADR-09 | G7 与 ICC 强耦合 | Accepted |
| ADR-10 | 双产品线 | **Superseded by ADR-24** |
| ADR-11 | G7 验证报告最小实用 | Accepted |
| ADR-12 | Black Generation v1.0 基础 | Accepted（gcolour 范围）|
| ADR-13 | 技术栈 | Accepted |
| ADR-14 | MVP 工期 11–13 个月 | **Superseded by ADR-22** |
| ADR-15 | 专色闭环分期 | Accepted |
| ADR-16 | 蜂巢专色 UI 双模式 | **Revised by ADR-24**（jcolour 仅 Mode A，gcolour 仅 Mode B）|
| ADR-17 | **颜色数据契约：全程光谱**（技术宪法）| Accepted |
| ADR-18 | RGB 专色影响范围滑块 | Accepted（jcolour 适用）|
| ADR-19 | 产品边界扩展到精品喷绘 | Accepted（jcolour v1.0 暂收窄到摄影师，详见 ADR-25）|
| ADR-20 | 拼版模块：自动 + 手工 + 裁切一体 | Accepted |
| ADR-21 | 裁切工作流：本机 + 去外部切割机 | Accepted |
| ADR-22 | 三段式发布策略 | Accepted（jcolour 走完整 M0–M18，gcolour 在 M15+ 启动）|
| ADR-23 | 测试打印工作流：缩小样 + 区域原大 | Accepted |
| **ADR-24** | **产品拆分：jcolour + gcolour 两个独立 App** | **Accepted** |
| **ADR-25** | **jcolour 用户聚焦：摄影师打印自己照片** | **Accepted** |
| **ADR-26** | **Epson PJL 直连（绕开 macOS Print Dialog）** | **Accepted（依赖 Spike 2）** |
| **ADR-27** | **B&W 双模式：单黑 + 多色黑** | **Accepted** |
| **ADR-28** | **Mutoh 推迟到 v1.5** | **Accepted** |
| **ADR-29** | **广色域映射分层架构（GamutAdvisor + SemanticGamutMapper）** | **Accepted** |
| **ADR-30** | **线性化职责分工：中性(WF1/2/3) 用 SCTV；密度只管墨量上限；彩色靠 G7+ICC** | **Accepted** |
