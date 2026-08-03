# jcolour PLAN — 摄影师专用版

> **版本**：v1.1（2026-05，由 [`docs/PLAN.md`](../../docs/PLAN.md) v1.0 拆分而来，定位收窄）
> **状态**：规划完成，进入 Spike 阶段
> **共享地基规划**：见 [`../../docs/PLAN-shared.md`](../../docs/PLAN-shared.md)
> **决策记录**：见 [`../../docs/DECISIONS.md`](../../docs/DECISIONS.md)（ADR-24~28 涵盖本次拆分）

---

## 0. 一句话定位

> **jcolour 是 macOS 原生、Apple Silicon 优化的色彩管理打印工作站，专为摄影师打印自己的照片设计。
> 深入解决 Epson UltraChrome P 系列调色痛点，B&W 同时支持单黑与多色黑，充分用满 P9500 12 通道色域。
> $199/年，一张废纸的钱。**

---

## 1. 唯一目标用户

参见 [`PERSONA.md`](PERSONA.md)。一句话：**Sam, the printing photographer** — 独立艺术摄影师 / 半专业摄影爱好者，自己打自己照片，每年 100–500 张限量版 + 个人存档。

**明确排除**（属于 gcolour 或 v1.5+）：
- ❌ 画廊代客打印服务
- ❌ 婚摄 / 商业写真出片业务
- ❌ 印前打样、CMYK 转换、GRACoL/Fogra 对标
- ❌ 品牌色彩部门、专色配方
- ❌ 工业喷绘（车贴、展板、纺织）
- ❌ 非 Epson 打印机（Mutoh/Canon/HP 留 v1.5 评估）

---

## 2. 硬件支持

### v1.0
| 型号 | 用户群 |
|------|------|
| Epson SureColor P700（13"） | 桌面摄影师 |
| Epson SureColor P900（17"） | 桌面摄影师 |
| Epson SureColor P7500（24"） | 大幅面 |
| Epson SureColor P9500（44"） | 旗舰大幅面 |

测量仪：X-Rite i1Pro3 / i1Pro3 Plus / i1iO（沿用 ADR-04）。

### v1.5（评估窗口）
- Epson SureColor P5000 / P6000 / P8000（UltraChrome HDX，老但仍在用）
- Epson SureColor P7000 / P9000（UltraChrome HDX）
- Canon imagePROGRAF PRO（用户基数大于 Mutoh 摄影圈）
- Mutoh 水性颜料款（XpertJet 个别型号）— 需进一步调研

---

## 3. 核心功能（按优先级）

### P0 — 必做（v1.0 公开发布）

#### 解决 Epson 调色痛点（详见 [`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md)）

1. **Epson 调色驱动接管层（PJL 直连）**
   - CUPS/IPP raw + Epson PPD + PJL 命令
   - 绕开 macOS Print Dialog 的 ColorSync 双重管理
   - 全程 16-bit 数据通路
   - 解决：Sonoma/Ventura 预设丢失、AirPrint 冲突、双重色彩管理崩坏
2. **Per-ink 线性化曲线编辑器**
   - 暴露 PK/MK/LK/LLK 至少四条墨道（v1.5 全 12 通道）
   - 测靶 → 自动拟合 → 用户可微调
   - QuadToneRIP 唯一对位但仅 B&W
3. **命名预设（Paper × Profile × Ink × Settings）**
   - 一个预设 = {纸张, ICC, 渲染意图, BPC, 黑墨类型, 色密度, 平台间隙}
   - 存在 jcolour 沙盒，不依赖系统对话框
4. **软打样精度提升模块**
   - 哑光纸纸白模拟 + ΔE 警告热力图
   - 渲染意图智能推荐（基于直方图饱和度）
   - BPC 决策提示（显示开/关对比）
   - 监视器校准状态检测

#### B&W 双模式（详见 [`BW-WORKFLOW.md`](BW-WORKFLOW.md)）

5. **单黑模式（Single Black）** — 仅 PK 或 MK，纯碳黑，档案级
6. **多色黑模式（Multi-channel Black）** — LK/LLK/主黑 + 微量彩墨，三段色环 split-tone

#### 加大色域彩色

7. **广色域映射分层（GamutAdvisor + SemanticGamutMapper）**
   - 16-bit ProPhoto → 12 通道 P9500 ICC 完整色域
   - GamutAdvisor：直方图分析 + 记忆色掩码 → 推荐 2 个 intent 候选
   - A/B 软打样：屏幕同时渲染两版，用户拍板（Paper White Simulation 默认 ON）
   - SemanticGamutMapper：CoreML 语义分割（肤色/天空/植物/主体/背景）→ 分区独立 intent
   - Or/Gr/V 调用率 meter：实时显示扩展通道下墨百分比
   - 端到端流程：[`PIPELINE.md`](PIPELINE.md) §7-§9
   - 执行规范：[`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md)
   - 架构决策：[`ADR-29`](../../docs/DECISIONS.md#adr-29)
8. **蜂巢专色 UI 模式 A**（沿用 ADR-18）
   - 图中取色 + 影响范围滑块（窄=准 / 宽=过渡好）

### P1 — 重要（Early Access M0–M12 起步即做）

9. **多张多纸批量队列**
   - 自动按黑墨分组，最小化 PK/MK 切换
   - 用墨预估 + 成本核算
10. **Edition Tracking 版次管理**
    - 自动编号写入 EXIF 和打印日志
    - 历史可查
11. **测试打印（缩小样 + 1:1 区域 + 同纸）** — ADR-23
12. **纸张配置文件库**
    - 内置 50+ 常用 paper × printer profile 组合
    - 一键安装，第三方拖入即用

### P1 — 校准闭环

13. **G7+ 灰阶校准向导** — ADR-08 / ADR-09
14. **云端 Argyll ICC 生成** — ADR-07
15. **墨盒批次跟踪** — 颜色漂移预警

### P2 — 锦上添花（v1.0 时间富裕再做）

16. Lightroom Classic 插件（"Send to jcolour"）
17. Capture One 集成
18. 耗材成本计算器
19. Apple Silicon Native 标识（营销用）

---

## 4. 不做的（明确推到 v1.5+）

- ❌ Mutoh / Canon / HP 支持（v1.5 评估）
- ❌ 多次自动迭代专色闭环（v1.5）
- ❌ True Shape 拼版高级（沿用 ADR-20 分期，v1.0 公开版做）
- ❌ AI 主体保护、生成填充
- ❌ 云端 Profile 同步、多端协作
- ❌ POD / 电商集成
- ❌ Pantone Connect（v2.0 + 授权）
- ❌ Windows 版本

---

## 5. 商业模式

沿用 ADR-06 修订后版本：
- **永久免费档**：查看 ICC、打印、Epson Print Layout 全功能对等 + PJL 直连绕开 ColorSync 双管理 + 命名预设稳定（**只这一项就值得装**）
- **付费订阅 $199.99/年**：自制 ICC、G7+ 校准、Per-ink 线性化、B&W 双模式、广色域映射、批量队列、Edition Tracking、纸张库、云端 Profile 额度
- **Early Access $99/年（终身锁定）** — Spike 完成后 20 位种子用户专用
- **v1.5 Pro $399/年** — 完整专色闭环 / Imposition / 主体保护 AI / 全 12 通道 Per-ink 线性化 / Multi-spectral metamerism

---

## 6. 技术栈

沿用 ADR-13：
- **客户端**：C++17 + Qt 6 + Little CMS 2 + libtiff + X-Rite i1 SDK
- **云端**：Go + PostgreSQL + Redis + Cloudflare R2 + ArgyllCMS（Docker）
- **构建**：CMake 3.20+
- **平台**：macOS 13+ Universal（Apple Silicon 优先）

详细共享引擎库设计见 [`../../docs/PLAN-shared.md`](../../docs/PLAN-shared.md)。

---

## 7. 关键 ADR（jcolour 适用）

| # | 决策 | 适用 jcolour |
|---|------|-------------|
| 02 | 目标硬件 Epson P7500/P9500 | ✅ 扩展含 P700/P900 |
| 03 | macOS 13+ Universal | ✅ |
| 04 | i1Pro3 测量 | ✅ |
| 05 | MAS 分发（修订）| ✅ jcolour 独立上架 |
| 06 | 商业模式（修订）| ✅ jcolour 主档 $199/年 |
| 07 | 云端 Argyll ICC | ✅ |
| 08 | G7+ 本地 | ✅ |
| 09 | G7 与 ICC 强耦合 | ✅ B&W 路径有简化版 |
| 11 | G7 报告最小实用 | ✅ |
| 13 | 技术栈 | ✅ |
| 17 | 全程光谱契约 | ✅ 项目宪法 |
| 18 | RGB 专色影响范围 | ✅ |
| 20 | 拼版分期 | ✅ |
| 21 | 裁切工作流 | ✅ |
| 22 | 三段式发布 | ✅ jcolour 走完整 M0–M18 |
| 23 | 测试打印 | ✅ |
| **24** | 产品拆分 | ✅ |
| **25** | jcolour 用户聚焦 | ✅ |
| **26** | Epson 调色接管 | ✅ |
| **27** | B&W 双模式 | ✅ |
| **28** | Mutoh 推迟 | ✅ |
| **29** | 广色域映射分层（GamutAdvisor + SemanticGamutMapper） | ✅ |

不适用 jcolour：
- ADR-12 Black Generation v1.0（CMYK 黑生成，gcolour 范围）
- ADR-15 专色闭环 Mode B（gcolour）
- ADR-16 蜂巢 Mode B（gcolour）
- ADR-19 精品喷绘扩展（部分含画廊服务，gcolour 评估）

---

## 8. 三段式发布（沿用 ADR-22）

| 阶段 | 时间 | 范围 | 分发 | 定价 |
|------|------|------|------|------|
| **Spike** | M-2–M0 | 5 个技术原型验证 | 内部 | — |
| **Early Access** | M0–M12 | 核心引擎 + B&W 双模式 + Per-ink 线性化 + PJL 直连 + 命名预设 + 批量队列 + Edition + 测试打印 + 软打样精度 + G7+ + 云端 ICC | 闭门 20 位种子摄影师 | $99/年终身锁定 |
| **v1.0 公开** | M12–M15 | + True Shape 拼版 + Tiling + Step&Repeat + 耗墨预估 + 外部裁切机 + 蜂巢模式 A 完整 + G7 报告导出 + 纸张库 50+ profile | Mac App Store | $199.99/年 |
| **v1.5 Pro** | M15–M18 | + 主体保护 AI + 多次迭代专色闭环 + Imposition + 灰阶曲线编辑器（25 控制点）+ 自定义半色调点形 + 全 12 通道 Per-ink 线性化 + 模拟胶片预设 + Multi-spectral metamerism + Mutoh/Canon 评估 + P10000/P20000 | MAS in-app upgrade | $399/年 Pro 档 |

详见 [`ROADMAP.md`](ROADMAP.md)。

---

## 9. 下一步

1. Spike 1：色彩管线 CLI（验证 LCMS2 + 36 通道光谱保持 ΔE2000 ≤ 1.0 vs Photoshop）
2. Spike 2：**macOS PJL 直连**（必须验证绕开 Print Dialog、16-bit 全程、保存预设稳定）
3. Spike 3：i1Pro3 SDK 沙盒行为
4. Spike 4：云端 Profile 生成 API
5. Spike 5：**最小 UI 端到端**（必须含 B&W 单黑/多色黑切换 + Per-ink 线性化曲线编辑器原型）

5 个 Spike 全绿 → 进入 Early Access 开发（M0）。详见 [`SPIKES.md`](SPIKES.md)。

---

## 10. 文档索引

| 文档 | 内容 |
|------|------|
| [`PERSONA.md`](PERSONA.md) | 摄影师唯一画像 + 一天工作流 + 决策路径 |
| [`PIPELINE.md`](PIPELINE.md) | 端到端色彩管线 9 段流程（single source of truth） |
| [`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md) | 广色域调用执行规范（PIPELINE §7-§9 深度版） |
| [`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md) | 调色痛点全集（知识库；执行规范见 PIPELINE / GAMUT-PIPELINE） |
| [`BW-WORKFLOW.md`](BW-WORKFLOW.md) | 单黑 / 多色黑双模式技术与 UI 设计 |
| [`COMPETITORS.md`](COMPETITORS.md) | ImagePrint / Mirage / Print Layout / QTR / LR/PS 对标 |
| [`ROADMAP.md`](ROADMAP.md) | M0–M18 三段式发布路线 |
| [`SPIKES.md`](SPIKES.md) | 5 个技术原型详细范围 |
| [`../../docs/PLAN-shared.md`](../../docs/PLAN-shared.md) | 共享引擎规划（jcolour + gcolour 共用） |
| [`../../docs/DECISIONS.md`](../../docs/DECISIONS.md) | 全部 ADR（含 24~29） |
