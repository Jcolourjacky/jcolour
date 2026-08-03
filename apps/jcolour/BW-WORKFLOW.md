# 黑白照片工作流：单黑 + 多色黑双模式

> 用户明确要求：B&W 必须同时支持**单黑打印**（仅用 PK 或 MK 单一墨水）和**多色黑打印**（LK + LLK + 主黑 + 微量彩墨拓展层次）。
> 这是 jcolour 直接对位 Epson ABW 与 QuadToneRIP 的核心差异化功能（ADR-27）。

---

## 一、为什么 B&W 需要专用工作流

普通彩色管线打 B&W 会出现：
1. **横向色偏**（horizontal color drift）— 灰阶从纯黑到纯白，中间调偏一种颜色，阴影偏另一种
2. **同色异谱**（metamerism）— 画廊聚光灯下中性，到客户家自然光下偏紫
3. **纯黑不黑** — 用 LK + LLK + 主黑混合而非纯碳黑，Dmax 损失
4. **饱和度残留** — 即使数字 RGB 是 (50,50,50) 中性，混色出来仍带微彩偏

摄影师的 B&W 是艺术核心，对中性度和层次的要求**比彩色苛刻 10 倍**。Ansel Adams 的 Zone System 文化让这一点根植于行业。

---

## 二、Epson 的 B&W 现状（用户痛点）

### 2.1 Epson ABW（Advanced Black & White）模式

- **原理**：驱动内置算法用 LK + LLK + Photo/Matte Black 调灰，加微量彩墨补色温
- **优点**：开箱即用，比纯彩色管线打 B&W 强
- **缺点**（用户抱怨集中点）：
  - 只有 4 个色调预设：Neutral / Cool / Warm / Sepia
  - 无法分区调色（无法做到 "高光暖、阴影冷" 的 split-tone）
  - 16-bit 输出有 bug（饱和度跳变）
  - Sonoma macOS 下对话框预设保存不稳定
  - 无法自定义灰阶曲线，每段灰度的色温固定

### 2.2 QuadToneRIP（社区救星）

- $50 shareware，B&W 极致用户的真爱
- 暴露**每条墨水通道的灰阶响应曲线**（.qidf 配置文件）
- 支持**任意分段调色**（每个 16th 灰阶的色温独立）
- 支持**单黑模式**（只用 PK 或 MK，纯碳黑）
- 缺点：
  - 学习曲线极陡（要写 .qidf 文本配置文件）
  - 文档分散在多个老网页
  - macOS Sonoma 兼容性问题
  - 60% 摄影师下载后放弃
  - 仅 B&W，不解决彩色

### 2.3 jcolour 的位置

**目标**：QuadToneRIP 的所有能力 + 比 ABW 还简单的 UX。

```
易用度  低 ─────────────────────────────────────── 高
                                          ABW
                              jcolour B&W ★
            QuadToneRIP

色调控制能力  弱 ─────────────────────────────── 强
                ABW
                          jcolour B&W ★
                                       QuadToneRIP
```

---

## 三、jcolour B&W 双模式设计

UI 顶部一个分段开关：**[ Single Black | Multi-channel Black ]**

### 模式 1：Single Black（单黑打印）

#### 何时用
- 极致中性、档案级要求
- 摄影师明确要"纯碳黑"质感
- 哈内姆勒 Photo Rag、Canson Rag Photographique 这类高棉纤维美术纸
- 黑白胶片复刻、Ansel Adams 风格

#### 技术原理
- 仅使用 **PK（Photo Black）** 或 **MK（Matte Black）** 单一墨水
- 不掺 LK / LLK / 任何彩墨
- 灰度通过**半色调算法（dithering / stochastic screening）** 控制墨点密度
- 优势：无横向色偏（只有一种颜料的反射光谱）；无同色异谱（光源变了，所有灰度同方向偏）；最深 Dmax

#### 输入条件
| 项目 | 要求 |
|------|------|
| 输入图像 | 已转灰度（grayscale）或 RGB 三通道完全相等 |
| 位深度 | 16-bit 推荐，8-bit 可接受 |
| 黑墨选择 | 用户选 PK 还是 MK（基于纸张） |

#### 引擎处理流程

```
1. 输入 16-bit 灰度图（来自 Photoshop / Lightroom）
2. 应用 G7+ 灰阶曲线（提前校准，确保线性）
3. 应用用户选择的色调（中性 / 微暖 / 微冷）— 通过曲线变形而非加彩墨
4. 选择半色调算法：
   · Stochastic（推荐，无规则斑点，自然胶片感）
   · Error Diffusion（细节锐利但有方向性纹理）
   · Custom dot pattern（v1.5）
5. 转为单通道 PK 或 MK 墨点密度图
6. 通过 PJL 直连发送到打印机
7. 其他所有墨水通道在 PJL 命令里强制设为 0
```

#### UI 设计（文字描述）

```
┌─────────────────────────────────────┐
│ B&W 模式: ●Single Black  ○Multi    │
├─────────────────────────────────────┤
│                                     │
│ 黑墨选择：                          │
│ [ Photo Black ▼ ]  推荐配亮光纸     │
│ ( PK / MK 切换会消耗 30ml 墨水 )   │
│                                     │
│ 色调微调：                          │
│   冷  ─────●─────  暖              │
│   纯中性                            │
│                                     │
│ 半色调：                            │
│ ◉ Stochastic（推荐）                │
│ ○ Error Diffusion                  │
│                                     │
│ 灰阶预览：                          │
│ [████████ 16 段灰阶预览条 ████████]│
│                                     │
│ 预估：墨耗 0.4ml / Dmax 2.6        │
└─────────────────────────────────────┘
```

#### 限制与注意
- 单墨打印**层次相对粗**（仅靠半色调，没有浅墨过渡）
- 在阴影区可能出现轻微 dot pattern（亮光纸尤其）
- 不适合大画幅高细节风光（多色黑模式更合适）

---

### 模式 2：Multi-channel Black（多色黑打印）

#### 何时用
- 大多数 B&W 场景（细节丰富、层次多）
- 想做 split-tone（高光暖、阴影冷之类）
- 风景、人像、街拍 B&W
- 模拟胶片色调（柯达 Tri-X / 富士 Acros / 红外 IR）

#### 技术原理
- 使用 **PK/MK + LK + LLK** 三通道灰墨为主
- 加 **微量 C/M/Y**（甚至 Or/V）用于色温微调和扩展暗部 Dmax
- 通过**分区曲线**：高光段（0-25%）/ 中间调（25-75%）/ 阴影（75-100%）各自独立色温
- 对标 QuadToneRIP 的 .qidf 思路，但 UI 是 GUI 而非文本配置

#### 输入条件
| 项目 | 要求 |
|------|------|
| 输入图像 | 灰度或 RGB 等通道（彩色源也可）|
| 位深度 | 必须 16-bit（多通道混色对精度敏感） |
| profile | 多色黑专用 profile（jcolour 内置 + 用户可校准） |

#### 引擎处理流程

```
1. 输入 16-bit 灰度（彩色源自动转）
2. 应用 G7+ 灰阶曲线（线性化）
3. 拆分为三段曲线区：高光 / 中间调 / 阴影
4. 每段曲线区独立应用：
   · 主灰墨配比（PK/MK + LK + LLK 三通道权重）
   · 色温补偿（CMY 微量加色）
   · 用户可调"色环位置"（参见 UI）
5. 应用 Per-ink 线性化
6. 多通道半色调（每条墨道独立 dithering）
7. 通过 PJL 直连发送
```

#### UI 设计（文字描述）

```
┌────────────────────────────────────────────┐
│ B&W 模式: ○Single  ●Multi-channel          │
├────────────────────────────────────────────┤
│                                            │
│   ┌──────┐  ┌──────┐  ┌──────┐            │
│   │ 高光 │  │中间调│  │ 阴影 │            │
│   │ 0-25%│  │25-75%│  │75-100│            │
│   │      │  │      │  │      │            │
│   │  ◉   │  │  ◉   │  │  ◉   │            │
│   │      │  │      │  │      │            │
│   │      │  │      │  │      │            │
│   └──────┘  └──────┘  └──────┘            │
│   微暖      纯中性     微冷                │
│  （sepia） （neutral）（selenium）         │
│                                            │
│ 全局中性度：                               │
│   严格中性 ●─────  允许小偏移              │
│                                            │
│ 灰阶曲线编辑器：[ 展开 ▼ ]                │
│ [25 控制点曲线，高级用户可拉曲线]          │
│                                            │
│ 实时灰阶预览：                             │
│ [█████ 64 段 + 色温微变化预览 █████]       │
│                                            │
│ 预设：                                     │
│ [Neutral][Selenium][Sepia][Split-tone][自定义]│
└────────────────────────────────────────────┘
```

#### 三色环（高光/中间调/阴影）的具体作用

每个色环代表该灰阶段的色温补偿方向：
- 中心 = 严格中性（仅 PK/MK + LK + LLK）
- 上方 = 加微量 C → 偏冷蓝
- 右上 = 加微量 C+G → 偏冷绿（selenium 调）
- 右 = 加微量 G → 偏绿
- 右下 = 加微量 Y+G → 偏暖绿
- 下 = 加微量 Y → 偏暖黄（sepia 调）
- 左下 = 加微量 Y+M → 偏暖红
- 左 = 加微量 M → 偏品红
- 左上 = 加微量 C+M → 偏冷品红

用户拖拽圆点位置 → 偏移量 = 距中心半径 → 色温 = 角度。

#### 高级模式（v1.0 P1）：灰阶曲线编辑器

当用户点开 "[ 展开 ▼ ]"，进入 25 控制点曲线编辑：
- X 轴：输入灰度 0-100%
- Y 轴：输出三个值（PK/MK 总量、LK 量、LLK 量、CMY 微量）
- 4 条曲线分别可拖
- 测靶后自动绘制初始曲线
- 与 QuadToneRIP .qidf 的 GUI 化等价

---

## 四、与 ABW / QuadToneRIP 的对比

| 能力 | Epson ABW | QuadToneRIP | jcolour Single Black | jcolour Multi-channel |
|------|-----------|-------------|---------------------|---------------------|
| **预设色调数** | 4 | 任意（写 .qidf） | 简单微调 | 任意（GUI） |
| **分区色温** | ❌ | ✅ | ❌ | ✅ |
| **单黑模式** | ❌ | ✅ | ✅ | ❌ |
| **GUI 易用度** | ⭐⭐⭐⭐ | ⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| **学习曲线** | 5 分钟 | 20+ 小时 | 5 分钟 | 30 分钟 |
| **每条墨道线性化** | ❌ | ✅ | ✅ | ✅ |
| **macOS 原生** | 半（驱动） | ✅（但 Sonoma 兼容差） | ✅ | ✅ |
| **16-bit 全程** | 有 bug | ✅ | ✅ | ✅ |
| **macOS 沙盒安全** | N/A | ❌ | ✅ | ✅ |
| **i1Pro3 测靶集成** | ❌ | 半（命令行） | ✅ | ✅ |
| **价格** | 免费 | $50 | $199/年（含全部功能） | 同左 |

---

## 五、技术实现要点

### 5.1 Epson PJL 直连（绕开 macOS 打印对话框）

- B&W 双模式必须**完全控制每条墨道的输出量**，macOS Print Dialog 不暴露这个能力
- 走 CUPS / IPP raw mode + Epson PJL 命令
- 关键 PJL 指令族：
  - `@PJL SET INKMODE = ...`（控制黑墨切换）
  - `@PJL SET COLORMODE = ...`（颜色模式）
  - `ESC ( G` 系列（输入数据格式控制）
- Spike 2 验证（见 [SPIKES.md](SPIKES.md)）

### 5.2 Per-ink Linearization

- 用户用 i1Pro3 测一张专用灰阶靶（256 段或 N×8 段）
- 每条墨道（PK/MK/LK/LLK 至少）独立拟合响应曲线
- 反演得到线性化校正曲线
- 引擎在分通道半色调前应用

### 5.3 半色调算法

| 算法 | 优点 | 缺点 |
|------|------|------|
| **Stochastic（FM screening）** | 无规则斑点，自然 | 高光区颗粒感 |
| **Error Diffusion (Floyd-Steinberg)** | 细节锐利 | 方向性纹理 |
| **Ordered Dither** | 快 | 网格感强，B&W 不推荐 |

v1.0 默认 Stochastic，v1.5 加 Error Diffusion 可选。

### 5.4 灰阶预览（屏幕模拟）

- 单黑模式：仅显示选定黑墨在选定纸张上的反射光谱模拟
- 多色黑模式：显示每段灰阶的实际色温偏移（≤ 1 ΔE 视觉可辨）
- 模拟基于光谱契约（ADR-17），不压 Lab

---

## 六、v1.0 vs v1.5 范围

### v1.0 必做（Early Access M0–M12 + 公开 M12–M15）

- ✅ 单黑模式（PK/MK 选择 + 微调色温 + Stochastic 半色调）
- ✅ 多色黑模式（三段色环 + 全局中性度 + 6 个内置预设）
- ✅ Per-ink 线性化（基础 4 条墨道：PK/MK/LK/LLK）
- ✅ G7+ 灰阶向导
- ✅ PJL 直连 Epson P700/P900/P7500/P9500

### v1.5 加强（M15–M18）

- ⏸️ 灰阶曲线编辑器（25 控制点）展开模式
- ⏸️ 自定义半色调点形（圆/椭圆/欧几里得）
- ⏸️ 全 12 通道 Per-ink 线性化（含 Or/Gr/V）
- ⏸️ 模拟胶片预设包（Tri-X / Acros / IR / 红外）
- ⏸️ Multi-spectral metamerism 检测（D50/D65/A/F11 切换软打样）

---

## 七、营销话术（jcolour B&W 卖点）

> "**ABW 不够用，QuadToneRIP 学不会？jcolour 让 B&W 摄影师 5 分钟做出 split-tone，30 分钟做出胶片感。**
> 单黑纯碳档案级，多色黑分区调色温。Mac 原生，全程 16-bit，i1Pro3 测靶一键校准。"

---

## 八、调研引用

- [QuadToneRIP](https://www.quadtonerip.com/) — 技术参考
- [Roy Harrington's QuadToneRIP documentation](http://www.quadtonerip.com/html/QTRoverview.html)
- [DPReview "I got perfectly neutral B&W prints with QuadToneRIP"](https://www.dpreview.com/forums/threads/i-got-perfectly-neutral-bw-prints-with-quadtone-rip.954934/)
- [Red River "Epson 3800 Advanced Black and White"](https://www.redrivercatalog.com/infocenter/epson3800advancedblackandwhite.html)
- [Peachpit Jeff Schewe "B&W on Epson"](https://www.peachpit.com/articles/article.aspx?p=2119011&seqNum=4)
- [Cone Editions Piezography](https://piezography.com/) — 多色黑专用墨水系统（不在 jcolour 范围但思路参考）

---

## 九、相关文档

- 用户画像：[`PERSONA.md`](PERSONA.md)
- Epson 调色痛点：[`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md)
- 竞品对标：[`COMPETITORS.md`](COMPETITORS.md)
- v1.0 范围：[`PLAN.md`](PLAN.md)
- ADR-27 B&W 双模式：[`../../docs/DECISIONS.md#adr-27`](../../docs/DECISIONS.md)
