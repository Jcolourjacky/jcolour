# PLAN.md（已拆分，请前往新位置）

> ⚠️ **本文件 v1.0（2026-04）已被 2026-05 的产品拆分（[ADR-24](DECISIONS.md)）取代。**
> 内容已经分散到下列三处。请前往对应文档阅读最新规划。

---

## 新位置导航

| 你想读的内容 | 现在的位置 |
|------------|-----------|
| **共享地基**（光谱契约、技术栈、共享 ADR、引擎规划） | [`PLAN-shared.md`](PLAN-shared.md) |
| **jcolour 详细规划**（摄影师专版） | [`../apps/jcolour/PLAN.md`](../apps/jcolour/PLAN.md) |
| **gcolour 占位规划**（打样专版，jcolour v1.0 后启动） | [`../apps/gcolour/PLAN.md`](../apps/gcolour/PLAN.md) |
| **架构决策记录** | [`DECISIONS.md`](DECISIONS.md) |
| **monorepo 总览** | [`../README.md`](../README.md) |

---

## 主要变更（2026-05）

参见 [ADR-24 产品拆分](DECISIONS.md) / [ADR-25 jcolour 用户聚焦](DECISIONS.md) / [ADR-26 Epson PJL 直连](DECISIONS.md) / [ADR-27 B&W 双模式](DECISIONS.md) / [ADR-28 Mutoh 推迟](DECISIONS.md)。

简要：
1. 单产品双产品线（旧 PLAN.md）→ **两个独立 App + 共享 C++ 引擎**
2. jcolour 范围进一步收窄 → **仅"摄影师打印自己作品"**，画廊/婚摄/打样移出
3. **B&W 必须同时支持单黑 + 多色黑**（用户明确要求）
4. **Mutoh 推迟到 v1.5**（调研显示摄影师占比极低）
5. 新增"Epson PJL 直连"决策，用于绕开 macOS Print Dialog 痛点

---

## 历史保留

为方便回溯历史决策上下文，旧 PLAN.md v1.0 的核心信息已被以下文档继承：

| 旧 PLAN.md 章节 | 现位置 |
|----------------|--------|
| 0. 技术宪法（光谱契约）| [`PLAN-shared.md`](PLAN-shared.md) §0 |
| 1. 定位 | [`../apps/jcolour/PLAN.md`](../apps/jcolour/PLAN.md) §0 / [`../apps/gcolour/PLAN.md`](../apps/gcolour/PLAN.md) §1 |
| 2. 产品结构 | [`PLAN-shared.md`](PLAN-shared.md) §1 |
| 3. 核心技术差异化 | [`../apps/jcolour/COMPETITORS.md`](../apps/jcolour/COMPETITORS.md) §四 |
| 4. 商业模式 | [`../apps/jcolour/PLAN.md`](../apps/jcolour/PLAN.md) §5 + [`../apps/gcolour/PLAN.md`](../apps/gcolour/PLAN.md) §5 |
| 5. 技术栈 | [`PLAN-shared.md`](PLAN-shared.md) §2 |
| 6. 关键决策（ADR 摘要）| [`DECISIONS.md`](DECISIONS.md) 索引表 |
| 7. 三段式发布 | [`../apps/jcolour/ROADMAP.md`](../apps/jcolour/ROADMAP.md) |
| 8. 下一步动作（Spike）| [`../apps/jcolour/SPIKES.md`](../apps/jcolour/SPIKES.md) |
