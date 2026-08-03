# jcolour 竞品对标

> 针对 jcolour 的目标用户（摄影师打印自己照片，参见 [`PERSONA.md`](PERSONA.md)）梳理直接和间接竞品。
> 不对标 GMG / Fiery XF / ColorBurst（gcolour 范围）；不对标 ONYX / Caldera / Wasatch（工业喷绘）。

---

## 一、竞品全景

```
易用度
  ↑
  │ ◉ Epson Print Layout（免费）
  │
  │      ◉ Lightroom 打印模块
  │
  │           ★ jcolour（目标位置）
  │
  │                     ◉ Mirage（€499）
  │
  │                            ◉ ImagePrint（$1500+）
  │
  │                                       ◉ QuadToneRIP（$50）
  │                                       （仅 B&W）
  └─────────────────────────────────→ 色彩控制能力
```

**核心机会**：易用度高 + 色彩控制强的象限**没有产品**。Mirage 已老化，ImagePrint 太复杂，QuadToneRIP 太难。

---

## 二、直接竞品详情

### 2.1 ImagePrint by ColorByte

| 维度 | 评估 |
|------|------|
| **价格** | $1,500–$2,500 永久许可 + 年维护 ~$300 |
| **核心卖点** | 27,000+ 内置 paper × printer profile 组合 |
| **平台** | macOS + Windows |
| **macOS Apple Silicon Native** | ❌ 仍是 Rosetta 翻译运行（截至 2026 Q1） |
| **B&W 工作流** | 中等（无分区色温） |
| **批量 / 拼版** | 强 |
| **学习曲线** | 陡（专业工具感重） |
| **目标用户** | 高级摄影师 + 画廊 + 打印店 |
| **强项** | profile 库丰富、稳、有 20 年用户口碑 |
| **弱项** | 贵、UI 老、Win 移植感重、Apple Silicon 滞后、永久许可 + 维护费让续费心理像被绑架 |
| **典型用户切换原因** | macOS 升级后崩溃、想要订阅替代品 |

**jcolour 的差异化打法**：
- 价格 1/7（$199 vs $1500+）
- Apple Silicon Native（M3 Max 实时软打样）
- 现代 macOS 沙盒安全
- 摄影师 UX（不是打印店 UX）

参考：
- [ImagePrint V8 Review - Luminous Landscape](https://luminous-landscape.com/imageprint-v8-review/)
- [Northlight Images ImagePrint coverage](https://www.northlight-images.co.uk/)

### 2.2 Mirage by DINAX

| 维度 | 评估 |
|------|------|
| **价格** | €499 一次性（约 $540） |
| **核心卖点** | 拼版 + 任务管理 + 与 Epson 紧密合作 |
| **平台** | macOS + Windows |
| **B&W 工作流** | 弱（依赖 Epson ABW） |
| **批量 / 拼版** | 中等（手工友好但无 True Shape 自动） |
| **学习曲线** | 中等 |
| **目标用户** | 摄影师 + 小型打印店 |
| **强项** | 一次性付费（摄影师喜欢）、与 Epson 直接合作的专色调用 |
| **弱项** | 德语血统、北美支持弱、社区小、color management 闭环不够、缺验证报告、Apple Silicon Native 也滞后 |
| **典型用户切换原因** | 升级路径模糊、新版本费用又出 |

**jcolour 的差异化打法**：
- 订阅 vs 一次性是双刃剑 — jcolour 提供"永久免费看图档 + 付费订阅核心功能"双轨
- B&W 双模式碾压 Mirage
- macOS 原生体验更现代

参考：
- [DINAX Mirage Review - Francesco Gola](https://www.francescogola.net/review/dinax-mirage-print-software-rip-review/)

### 2.3 Epson Print Layout（官方免费）

| 维度 | 评估 |
|------|------|
| **价格** | 免费 |
| **核心卖点** | 与打印机驱动深度集成 |
| **平台** | macOS + Windows + iOS（受限） |
| **B&W 工作流** | 仅 ABW 4 预设 |
| **批量 / 拼版** | 几乎无 |
| **学习曲线** | 低 |
| **目标用户** | 入门摄影师 |
| **强项** | 免费、官方、零摩擦上手 |
| **弱项** | 太基础、保存预设丢失、ColorSync 冲突、macOS 升级后频繁出问题 |
| **替换难度** | 低（多数用户用得很挫败） |

**jcolour 的策略**：
- **永久免费档**完全对标 Epson Print Layout 全部功能 + 一点点增强（PJL 直连绕开 ColorSync 双管理 + 命名预设稳定）
- 这一档不强求转化，作为流量入口
- 摄影师"打不出来想要的颜色"时自然升级到付费档

### 2.4 QuadToneRIP（QTR）

| 维度 | 评估 |
|------|------|
| **价格** | $50 shareware（一次性） |
| **核心卖点** | B&W 极致控制（每条墨道每个 16th 灰阶可调） |
| **平台** | macOS + Windows（macOS Sonoma+ 兼容性问题报告） |
| **B&W 工作流** | 行业标杆 |
| **批量 / 拼版** | ❌ 无 |
| **学习曲线** | **极陡**（要写 .qidf 文本配置文件） |
| **目标用户** | B&W 极致摄影师（Ansel Adams 信徒） |
| **强项** | B&W 控制力业界第一、价格友好、社区死忠 |
| **弱项** | 仅 B&W、文档分散、macOS 新版本兼容差、UX 是 1990 年代风格、60% 用户下载后放弃 |

**jcolour 的差异化打法**：
- 把 QTR 的能力**全装进 GUI**（[`BW-WORKFLOW.md`](BW-WORKFLOW.md) 的多色黑模式）
- 学习曲线从 20+ 小时压到 30 分钟
- 同时提供单黑模式（QTR 也有但更难配置）
- 加上彩色（QTR 完全不做）

参考：
- [QuadToneRIP](https://www.quadtonerip.com/)
- [DPReview QTR threads](https://www.dpreview.com/forums/threads/i-got-perfectly-neutral-bw-prints-with-quadtone-rip.954934/)

### 2.5 Lightroom Classic 打印模块

| 维度 | 评估 |
|------|------|
| **价格** | Adobe CC 订阅 $9.99/月（仅摄影 plan） |
| **核心卖点** | 直接接 Lightroom 工作流 |
| **平台** | macOS + Windows |
| **B&W 工作流** | 弱 |
| **批量** | 中等 |
| **学习曲线** | 低（已经在用 LR） |
| **强项** | 无缝接 LR 编辑、不用切换软件 |
| **弱项** | 故意做得基础（Adobe 不想替代专业 RIP）、color management 选项有限、软打样模块在 Develop 不在 Print |
| **替换难度** | 中（工作流深度集成是壁垒） |

**jcolour 的策略**：
- 提供 Lightroom 插件 / 导出预设（"Send to jcolour"）— P2 功能
- 不试图替换 LR，作为 LR 的下游协作

### 2.6 Photoshop 打印对话框

| 维度 | 评估 |
|------|------|
| **价格** | Adobe CC 订阅 |
| **核心卖点** | 内置软打样 + 全部图像编辑能力 |
| **B&W 工作流** | 中等（图像调色强，但打印控制弱） |
| **批量** | ❌ 无 |
| **学习曲线** | 中（设置藏在 7+ 层对话框里） |
| **强项** | 软打样精度行业标准、控制最完整 |
| **弱项** | 一次只能打一张、设置散落、macOS 打印对话框冲突频发 |

**jcolour 的策略**：
- 提供 PS 插件（"Send to jcolour Print"）— P2
- jcolour 内置软打样可与 PS 软打样一致（共用 ICC 链路）

---

## 三、间接竞品 / 不对标

| 工具 | 为什么不对标 |
|------|-----------|
| **Canon Professional Print & Layout** | 仅适配 Canon imagePROGRAF（非 Epson） |
| **HP Click（Designjet）** | HP 生态，工业图文 |
| **GMG OpenColor** | €5000+/年，印厂级，gcolour 范围 |
| **Fiery XF** | $1500+/年，CMYK 打样，gcolour 范围 |
| **ColorBurst** | 同上 |
| **CGS ORIS** | 印厂级 |
| **ONYX RIP / Caldera / Wasatch** | 工业喷绘（标牌/车贴/纺织），非摄影 |
| **Roland VersaWorks** | Roland 生态，eco-solvent 方向 |
| **Epson Edge Print** | SC-S/T 工业系列，非摄影 |
| **Piezography** (Cone Editions) | 全套替换墨水系统，硬件改造，门槛极高 |
| **Print Tool by Roy Harrington** | 老 macOS 工具，Roy 也是 QTR 作者，已停更 |

---

## 四、核心差异化矩阵（对位摄影师场景）

| 摄影师场景 | ImagePrint | Mirage | Print Layout | QTR | LR/PS | **jcolour** |
|-----------|-----------|--------|-------------|-----|-------|------------|
| **彩色打印充分用满 P9500 色域** | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ❌ | ⭐⭐⭐ | **⭐⭐⭐⭐⭐** |
| **B&W 单黑（纯碳）** | ⭐⭐ | ⭐ | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐ | **⭐⭐⭐⭐⭐** |
| **B&W 多色黑 split-tone** | ⭐⭐ | ⭐ | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐ | **⭐⭐⭐⭐⭐** |
| **macOS Apple Silicon Native** | ❌ | ⚠️ | ✅ | ⚠️ | ✅ | **✅** |
| **Sonoma 驱动稳定** | ⚠️ | ⚠️ | ❌ | ⚠️ | ⚠️ | **✅（PJL 直连）** |
| **保存预设可靠** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐ | ⭐⭐ | ⭐⭐ | **⭐⭐⭐⭐⭐** |
| **软打样精度（哑光纸）** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | N/A | ⭐⭐⭐⭐ | **⭐⭐⭐⭐**（带 ΔE 警告） |
| **批量多纸队列** | ⭐⭐⭐⭐ | ⭐⭐⭐ | ❌ | ❌ | ⭐⭐ | **⭐⭐⭐⭐** |
| **限量版编号管理** | ⭐⭐ | ⭐⭐ | ❌ | ❌ | ❌ | **⭐⭐⭐⭐** |
| **学习成本** | 高 | 中 | 低 | **极高** | 中 | **低** |
| **价格亲民度** | ❌ | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | **⭐⭐⭐⭐** |

---

## 五、定价对比

| 工具 | 定价 | 5 年总成本 |
|------|------|----------|
| ImagePrint | $1,500 + $300/年维护 | $2,700 |
| Mirage | €499 一次 | ~$540 |
| Epson Print Layout | 免费 | $0 |
| QuadToneRIP | $50 一次 | $50 |
| Lightroom CC（摄影 plan） | $9.99/月 | $600 |
| **jcolour 主档** | $199/年 | $995 |
| **jcolour 早鸟** | $99/年（终身锁） | $495 |
| **jcolour Pro 档（v1.5）** | $399/年 | TBD |

**jcolour 5 年成本约为 ImagePrint 的 1/3**，但功能等价或更强。

---

## 六、上架策略与渠道

### 6.1 Mac App Store（沿用 ADR-05）
- 唯一分发渠道
- 苹果抽成 15-30%
- 自动更新、退款流程统一
- StoreKit 订阅集成

### 6.2 摄影师社区渗透
- DPReview Printers 论坛 — 早期种子用户聚集地
- Luminous Landscape — 评测合作（送软件 + 早期访问）
- Reddit r/photography, r/AnalogCommunity, r/printers
- Fred Miranda forum
- PhotoPXL forum
- Cambridge in Colour
- YouTube 摄影师评测：Keith Cooper (Northlight)、Mark Segal、PiXimperfect 等
- 国内：图虫、500px CN、知乎摄影专栏、B 站摄影 UP 主

### 6.3 纸张品牌联名
- Hahnemühle / Canson / Moab 提供"内置 50+ 纸 profile"作为流量交换
- 纸张盒贴小卡片"用 jcolour 打印体验更佳"

### 6.4 教育内容
- 自有 YouTube 频道：色彩管理科普、QTR 替代教程、B&W split-tone 教程
- 博客：与论坛文化对接，写"如何修 macOS Sonoma Epson 驱动"这类长尾文章

---

## 七、相关文档

- 用户画像：[`PERSONA.md`](PERSONA.md)
- 调色痛点：[`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md)
- B&W 工作流：[`BW-WORKFLOW.md`](BW-WORKFLOW.md)
- v1.0 范围：[`PLAN.md`](PLAN.md)
- 路线图：[`ROADMAP.md`](ROADMAP.md)
