# jcolour & gcolour Monorepo

macOS 色彩管理打印工作站项目，针对 **Epson SureColor P 系列 UltraChrome** 大幅面打印机。

> **状态**：Spike 阶段实作进行中。`engine/`（~17K 行 C++）、`apps/jcolour/`（~14K 行 Qt）已就位；引擎完整 audit 30 个 bug 全部清空（2026-05 Phase A–E 重构）。Spike 验收需实机 / 沙盒 / 盲测，详见 [`apps/jcolour/ROADMAP.md`](apps/jcolour/ROADMAP.md) → Spike 验收状态。

---

## 两个产品

```
┌──────────────────────────┐  ┌──────────────────────────┐
│ jcolour                  │  │ gcolour                  │
│ 摄影师打印自己照片       │  │ 印前打样 / 品牌色彩      │
│ RGB · 大色域 · B&W       │  │ CMYK · GRACoL · 专色     │
│ ★ 先做                   │  │ jcolour v1.0 后启动       │
│ apps/jcolour/            │  │ apps/gcolour/            │
└──────────────────────────┘  └──────────────────────────┘
              ↘                          ↙
                ┌────────────────────┐
                │  共享 C++ 引擎     │
                │  engine/           │
                │  · 36 通道光谱核心 │
                │  · G7+ 灰平衡      │
                │  · LCMS2 / ICC     │
                │  · Kubelka-Munk    │
                │  · X-Rite i1 SDK   │
                │  · Epson PJL 直连  │
                │  · 拼版 / 裁切     │
                └────────────────────┘
                          ↓
                ┌────────────────────┐
                │  cloud/            │
                │  云端 ICC 生成 API │
                │  Go + PG + R2      │
                │  Argyll 子进程隔离 │
                └────────────────────┘
```

---

## 一句话定位

### jcolour（先做）
> macOS 原生、Apple Silicon 优化的色彩管理打印工作站，专为摄影师打印自己的照片设计。
> 深入解决 Epson UltraChrome P 系列调色痛点，B&W 同时支持单黑与多色黑，充分用满 P9500 12 通道色域。
> $199/年，一张废纸的钱。

### gcolour（后做）
> macOS 原生 CMYK 打样工作站，对标 Fiery XF / ColorBurst / GMG OpenColor，
> 为印前打样师傅、品牌色彩部门、印刷服务商提供 GRACoL/Fogra 精确对标 + 蜂巢光谱专色闭环。

---

## 仓库结构

```
.
├── README.md                     ← 本文件
├── apps/
│   ├── jcolour/                  ← 摄影师 App（先做）
│   │   ├── PLAN.md               核心规划
│   │   ├── PERSONA.md            摄影师唯一画像
│   │   ├── EPSON-COLOR-PROBLEMS.md  调色痛点 + 广色域指南
│   │   ├── BW-WORKFLOW.md        单黑 / 多色黑双模式设计
│   │   ├── COMPETITORS.md        ImagePrint / Mirage / QTR 对标
│   │   ├── ROADMAP.md            M0–M18 路线
│   │   └── SPIKES.md             5 个技术原型
│   └── gcolour/                  ← 打样 App（占位）
│       └── PLAN.md
├── engine/                       ← 共享 C++ 引擎（暂无代码）
│   └── README.md                 模块结构与设计原则
├── cloud/                        ← 云端 ICC API（占位，待 Spike 4）
└── docs/
    ├── PLAN.md                   ← 早期单产品规划（已被拆分）
    ├── PLAN-shared.md            共享地基规划（jcolour + gcolour 共用）
    └── DECISIONS.md              全部 ADR（28 条）
```

---

## 入门阅读顺序

如果你**第一次进入这个项目**：

1. [`docs/PLAN-shared.md`](docs/PLAN-shared.md) — 项目总览、技术宪法、共享基础
2. [`apps/jcolour/PLAN.md`](apps/jcolour/PLAN.md) — jcolour 摄影师专版规划
3. [`apps/jcolour/PERSONA.md`](apps/jcolour/PERSONA.md) — 唯一目标用户画像
4. [`apps/jcolour/EPSON-COLOR-PROBLEMS.md`](apps/jcolour/EPSON-COLOR-PROBLEMS.md) — 调色痛点与广色域指南
5. [`apps/jcolour/BW-WORKFLOW.md`](apps/jcolour/BW-WORKFLOW.md) — 黑白工作流设计
6. [`apps/jcolour/COMPETITORS.md`](apps/jcolour/COMPETITORS.md) — 竞品对标
7. [`apps/jcolour/ROADMAP.md`](apps/jcolour/ROADMAP.md) — 路线图
8. [`apps/jcolour/SPIKES.md`](apps/jcolour/SPIKES.md) — 5 个技术原型
9. [`docs/DECISIONS.md`](docs/DECISIONS.md) — 所有 ADR 决策记录

如果你**只想了解技术架构**：

1. [`docs/DECISIONS.md`](docs/DECISIONS.md) **ADR-17**（颜色数据契约：全程光谱 — 项目宪法）
2. [`docs/DECISIONS.md`](docs/DECISIONS.md) **ADR-26**（Epson PJL 直连）
3. [`engine/README.md`](engine/README.md) — 引擎模块结构

---

## 当前进度

### 规划（全部 ✅）
- [x] 市场与竞品研究
- [x] 产品结构 / 商业模式 / 技术栈确认
- [x] 早期单产品规划定稿（`docs/PLAN.md`）
- [x] **拆分为 jcolour + gcolour 双 App**（ADR-24，2026-05）
- [x] **jcolour 用户聚焦摄影师**（ADR-25）
- [x] **B&W 双模式设计**（ADR-27 / `BW-WORKFLOW.md`）
- [x] **Epson PJL 直连决策**（ADR-26）
- [x] **Mutoh 推迟到 v1.5 决策**（ADR-28）
- [x] jcolour 完整规划文档（PLAN / PERSONA / EPSON-COLOR-PROBLEMS / BW-WORKFLOW / COMPETITORS / ROADMAP / SPIKES）

### 引擎实作（Phase A–E，2026-05）
- [x] **`channel_layout.h` 型别系统** — 11-active / 12-physical，PK/MK 互斥编译期保证
- [x] **wf1 / wf2 / wf3 / wf4 / wf6 全部重构成 11-active 模型**（含 BlackKind 元数据）
- [x] **wf3 真实 K-M 模型**（per-wavelength K/S + Y-N 网格搜索，2026-05 替换之前的 scalar 假模型）
- [x] **wf4 真实 11-channel ICC 路径**（16-bit 全程 + TIL 强制 + `WF4MeasurementBackend` 介面）
- [x] **wf6 真实 sRGB→Lab + ICC 采样 gamut hull**（替换硬编码 8 点 ProPhoto 近似）
- [x] **PrintCondition 9 状态机** 完整转换表
- [x] **`pjl.cpp` 通道映射 + 网路修正**（`getaddrinfo` / port 9100 / IPv6 / 修正旧通道乱映射）
- [x] **完整 audit 30 个 bug 全部清空**（6 critical + 14 high + 6 medium + 4 low）
- [x] **测试覆盖** — 54/55 测试套件通过（test-tiling 预先存在的 hang，无关）

### Spike 验收（待实机/沙盒/盲测）
- 🟡 Spike 1：色彩管线 CLI（代码 ✓ / ΔE2000 vs Photoshop 对照待）
- 🟡 Spike 2：macOS PJL 直连 ★ — 代码 ✓ / P9580 / P7580 / P20580 实机待
- 🟡 Spike 3：i1Pro3 SDK 沙盒 — 介面 ✓ / SDK 接入 + MAS 沙盒待
- 🟡 Spike 4：云端 Profile 生成 API — 结构 ✓ / 端到端 < 30s 待
- 🟡 Spike 5：最小 UI 端到端 ★ — 代码 ✓ / 5 位摄影师盲测待

### 发布
- [ ] Early Access M0–M12（jcolour 闭门 20 用户）
- [ ] v1.0 公开 M12–M15（MAS 上架）
- [ ] v1.5 Pro M15–M18
- [ ] gcolour 启动（jcolour M15 后）

---

## 技术栈速览

- **客户端**：C++17 + Qt 6 + Little CMS 2 + libtiff + X-Rite i1 SDK，macOS 13+ Universal
- **云端**：Go + PostgreSQL + Redis + Cloudflare R2 + ArgyllCMS（Docker 子进程，GPL 合规）
- **分发**：Mac App Store + 自建订阅服务器

详见 [ADR-13 技术栈](docs/DECISIONS.md)。

---

## 构建（待 Spike 后定型）

```bash
brew install cmake little-cms2 libtiff
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## 许可

尚未定型。客户端预计闭源（商业产品），云端引擎部分通过 ArgyllCMS（GPL）子进程隔离合规。详见 [ADR-07 ICC 生成在云端](docs/DECISIONS.md) / [ADR-13 技术栈](docs/DECISIONS.md)。

---

## 决策追踪

任何架构决策都先写 ADR 后实现。完整 28 条决策记录见 [`docs/DECISIONS.md`](docs/DECISIONS.md)。

最近的 5 条（2026-05）：
- ADR-24 产品拆分：jcolour + gcolour 两个独立 App
- ADR-25 jcolour 用户聚焦摄影师
- ADR-26 Epson PJL 直连
- ADR-27 B&W 双模式
- ADR-28 Mutoh 推迟到 v1.5
