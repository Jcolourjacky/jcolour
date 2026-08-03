# jcolour & gcolour 共享地基规划

> 由 [`PLAN.md`](PLAN.md) v1.0 拆分而来，记录两个产品共用的引擎、技术栈、商业基础设施。
> 产品差异化的部分见各自的 `apps/<name>/PLAN.md`。

---

## 0. 技术宪法（保留）

> **jcolour & gcolour 全链路用 36 通道光谱反射率曲线计算。Lab / ΔE / XYZ 只是给人看的派生数据，不参与任何计算决策。**
>
> 详见 [ADR-17 颜色数据契约：全程光谱](DECISIONS.md#adr-17-颜色数据契约全程光谱)。
>
> **这条原则在 engine/ 任何模块、任何应用中都不可违反。**

---

## 1. 产品拆分概览

参见 [ADR-24 产品拆分](DECISIONS.md)。

```
                ┌──────────────────────────┐
                │  共享引擎 engine/        │
                │  (C++ 库，平台独立)      │
                ├──────────────────────────┤
                │  spectral/   光谱核心    │
                │  g7/         灰平衡      │
                │  icc/        LCMS2 封装  │
                │  kubelka_munk/ 油墨混合  │
                │  i1sdk/      测量仪      │
                │  pjl/        Epson 直连  │
                │  nesting/    拼版        │
                │  cutting/    裁切        │
                │  test_print/ 测试打印    │
                └────────┬─────────────────┘
                         │
            ┌────────────┴────────────┐
            ▼                         ▼
    ┌──────────────┐         ┌──────────────┐
    │  apps/jcolour│         │  apps/gcolour│
    │  (摄影师 App)│         │  (打样 App)  │
    │  Qt 6 UI     │         │  Qt 6 UI     │
    │  先做         │         │  jcolour 后  │
    └──────────────┘         └──────────────┘
            │                         │
            └────────────┬────────────┘
                         ▼
                ┌──────────────────┐
                │  cloud/          │
                │  Go API 服务     │
                │  Argyll 子进程   │
                │  Postgres / R2   │
                └──────────────────┘
```

---

## 2. 共享技术栈

沿用 [ADR-13](DECISIONS.md#adr-13-技术栈)：

### 客户端（jcolour + gcolour 共用）
| 层 | 技术 | 备注 |
|----|------|------|
| UI | Qt 6 LGPL | 动态链接，可商业闭源 |
| 色彩引擎 | Little CMS 2 (MIT) | 业界事实标准 |
| 图像 IO | libtiff / libjpeg-turbo / libpng | 标准栈 |
| 分光仪 | X-Rite i1 SDK v3（商业授权） | i1Pro3 / i1Pro3 Plus / i1iO |
| Epson 通信 | CUPS/IPP raw + Epson PPD + PJL（jcolour 主用） | macOS 原生 |
| 打包 | macOS Universal（x86_64 + arm64） | MAS 要求 |

### 云端（jcolour + gcolour 共用）
| 组件 | 技术 |
|------|-----|
| API | Go (Gin/Fiber) |
| 数据库 | PostgreSQL |
| 缓存/队列 | Redis |
| 对象存储 | Cloudflare R2 |
| ICC 生成 | ArgyllCMS（Docker 子进程，GPL 子进程隔离 SaaS loophole）|
| 许可/订阅 | 自建 License 服务器 + App Store IAP |

---

## 3. 共享 ADR 清单（适用两个 App）

| # | 决策 | 状态 |
|---|------|------|
| 02 | 目标硬件：Epson P 系列（jcolour）/ 类似硬件（gcolour 评估）| Accepted |
| 03 | 平台：macOS 13+ Universal | Accepted |
| 04 | 测量仪：X-Rite i1Pro3 / i1Pro3 Plus / i1iO | Accepted |
| 05 | 分发：Mac App Store（修订：两个 App 各自上架）| Revised |
| 06 | 商业模式：Freemium 订阅（修订：jcolour 主档 $199/年，gcolour 待定）| Revised |
| 07 | ICC 生成：云端 Argyll | Accepted |
| 08 | G7+ 引擎：本地 | Accepted |
| 09 | G7 与 ICC 强耦合（jcolour B&W 路径有简化版）| Accepted |
| 11 | G7 验证报告最小实用 | Accepted |
| 13 | 技术栈 | Accepted |
| **17** | **颜色数据契约：全程光谱** | **Accepted（项目宪法）** |
| 20 | 拼版分期 | Accepted |
| 21 | 裁切工作流 | Accepted |
| 22 | 三段式发布 | Accepted |
| 23 | 测试打印工作流 | Accepted |
| **24** | 产品拆分 | Accepted（新增）|
| **25** | jcolour 用户聚焦摄影师 | Accepted（新增）|
| **26** | Epson PJL 直连 | Accepted（新增，jcolour 主用）|

各产品专属 ADR 见各自 PLAN.md。

---

## 4. 共享开发顺序

### 阶段 A：jcolour Spike（M-2 → M0，2 个月）
共享引擎从零开始构建：
- spectral/（光谱核心）— Spike 1 启动
- icc/（LCMS2 封装）— Spike 1 启动
- pjl/（Epson 直连）— Spike 2 启动 ⭐
- i1sdk/（测量仪）— Spike 3 启动
- 这阶段产物会被 jcolour Early Access 直接消费

### 阶段 B：jcolour Early Access（M0 → M12）
继续填充：
- g7/（灰平衡）
- kubelka_munk/（基础版油墨混合，jcolour 用蜂巢模式 A）
- nesting/（基础矩形紧排）
- cutting/（本机裁切）
- test_print/

### 阶段 C：jcolour v1.0 公开（M12 → M15）
完善：
- nesting/（True Shape + Tiling + Step&Repeat）
- cutting/（外部切割机）

### 阶段 D：jcolour v1.5 Pro（M15 → M18）
高级特性：
- spectral/（多光源 metamerism）
- pjl/（全 12 通道线性化）

### 阶段 E：gcolour 启动（M15+，与 v1.5 并行）
基于完整的 engine/ 开始构建 gcolour：
- 直接复用 spectral / g7 / icc / kubelka_munk / nesting / cutting / test_print
- 仅需新增 gcolour 自己的 CMYK UI 和工作流

---

## 5. 共享商业基础设施

### 5.1 Apple Developer 账户
- 单一公司账号即可上架两个 App
- 两个 App 各自的 Bundle ID：
  - `com.jcolour.app`
  - `com.gcolour.app`

### 5.2 订阅与 License
- 共用 License 服务器
- 用户可以同时订阅两个 App，未来可考虑 "Suite Bundle" 折扣

### 5.3 云端基础设施
- 共用云端服务（cloud/）
- API 路径区分产品：`/api/v1/jcolour/*` 和 `/api/v1/gcolour/*`
- 共用 Argyll 容器（CMYK 与 RGB 配置不同的 colprof 参数）

### 5.4 客户支持
- 共用支持邮箱、文档站
- 子域名分流：`docs.jcolour.app` 和 `docs.gcolour.app`

---

## 6. 营销分流

虽然引擎共用，但市场推广完全分开：

| 维度 | jcolour | gcolour |
|------|---------|---------|
| **网站** | 单页强调摄影、艺术、B&W、Apple Silicon | 单页强调 GRACoL/Fogra、专色、合规、ROI |
| **核心渠道** | 摄影社区（DPReview / LL / Reddit / 国内 KOL）| 行业展会、G7 培训、印刷协会 |
| **关键词** | 摄影 / fine art / giclée / B&W / 限量版 | 打样 / GRACoL / Fogra / Pantone / 品牌色 |
| **决策周期** | 1–7 天试用 → 7–30 天付费 | 1–3 个月评估 |
| **客单价** | $99–$399/年 | $499–$999/年 |

**绝不在 jcolour 网站讲打样，绝不在 gcolour 网站讲艺术摄影**。两条线服务两个画像，混淆会双输。

---

## 7. 治理与变更

### 7.1 引擎变更需要双方同意
- 任何 engine/ 模块的破坏性变更必须同时考虑 jcolour 和 gcolour 影响
- ADR 制度持续：架构决策必须先记录为 ADR

### 7.2 应用变更各自自由
- apps/jcolour/ 和 apps/gcolour/ 各自迭代，互不影响
- 应用层 UI 文案、UX 决策不需走 ADR

### 7.3 版本号
- engine/ 走独立 SemVer（如 0.1.0 / 1.0.0 / ...）
- jcolour 和 gcolour 各自 SemVer
- 应用版本号不必一致（jcolour 1.5 可以依赖 engine 2.0）

---

## 8. 相关文档

- jcolour 详细规划：[`../apps/jcolour/PLAN.md`](../apps/jcolour/PLAN.md)
- gcolour 占位规划：[`../apps/gcolour/PLAN.md`](../apps/gcolour/PLAN.md)
- 共享引擎 README：[`../engine/README.md`](../engine/README.md)
- 全部 ADR：[`DECISIONS.md`](DECISIONS.md)
- 早期单产品规划（已被本文件 + apps/* 取代）：[`PLAN.md`](PLAN.md)
