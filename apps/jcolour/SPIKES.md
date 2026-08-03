# jcolour 5 Spike — 技术原型范围

> Spike 阶段（M-2 → M0，2 个月）。每 Spike 1–2 周。
> 5 项全绿才进入 Early Access 开发。任一红灯需要重新评估架构。

---

## Spike 1：色彩管线 CLI

### 目标
验证 LCMS2 + 36 通道光谱保持能否做到 **ΔE2000 ≤ 1.0 vs Photoshop** 的颜色转换精度。

### 输入
- 16-bit ProPhoto RGB TIFF 测试图（含 ColorChecker、灰阶、肤色、高饱和色块）
- 多个目标 ICC（Hahnemühle Photo Rag for P9500、Canson Baryta for P9500）

### 实现
- C++17 CLI 工具 `jcolour-convert`
- 链路：TIFF in → LCMS2 转换 + 36 通道光谱中间表示 → TIFF out
- Photoshop 用同样链路（Photoshop manages colors + 同 ICC）出参考样
- 用 i1Pro3 实测打印样比对

### 验收标准
- 测试图块 95% 区域 ΔE2000 ≤ 1.0
- 灰阶段 100% ΔE2000 ≤ 0.5
- 肤色段 ΔE2000 ≤ 1.5
- 性能：1 张 24 寸图 < 5 秒（M2 Pro 基准）

### 子任务
- [ ] LCMS2 + 36 通道光谱转换（主验收）
- [ ] G7+ NPDC 灰阶校准验证（`G7ComplianceChecker` 已有验证部分）
- [ ] **G7+ 校正曲线输出**（`g7_validation.h` 目前仅验证，未输出 `g7_curve.lut`；需扩展为 Spike 1 子任务，对标 PIPELINE-gcolour.md §4 的校正曲线格式）

### 红线
- 任一阶 ΔE > 3.0 → 链路设计有根本问题
- 性能 > 30 秒 → 36 通道光谱中间表示成本过高，需重新设计存储
- G7+ 校正曲线不能输出 → §14 轻校准（Quick Cal）无法实现，影响整个校准闭环

---

## Spike 2：macOS PJL 直连 Epson（关键 Spike）★

### 目标
**这是 jcolour 整个产品定位的技术基础**。验证能否绕开 macOS Print Dialog，直接走 PJL 全程控制 Epson 大幅面打印机。

### 关键问题
1. CUPS / IPP raw mode 能否在 macOS 沙盒应用里使用
2. Epson PPD 能否在 jcolour 内部捆绑而不依赖系统驱动
3. 关键 PJL 命令族是否能控制：
   - `@PJL SET INKMODE = ...`（PK/MK 切换）
   - `@PJL SET COLORMODE = OFF`（关闭驱动色彩管理）
   - `ESC ( G` 系列（输入数据格式 / 16-bit）
   - 每条墨道的输出量（如果文档支持）
4. 16-bit RGBA 数据流能否完整送到打印机不被降到 8-bit
5. 打印任务状态回读（进度、错误）

### 实现
- 实机：Epson P900（桌面）+ Epson P7500（大幅面）
- 测试程序：C++ 调 CUPS API 发送预制 PJL + 光栅数据
- 测试 1：发空白测试图 → 确认能打
- 测试 2：发可控墨量灰阶图 → 用 i1Pro3 测每段墨水实际密度
- 测试 3：B&W 单黑模式（仅 PK 输出，其他墨道强制 0）→ 测出真的只有 PK 墨水

### 验收标准
- 全程 16-bit 数据流，颜色未被降精度（i1Pro3 实测灰阶平滑无 banding）
- 单黑模式：除 PK/MK 外，其他通道墨水实测 < 0.5%
- B&W 多色黑：能控制 PK + LK + LLK 三通道独立比例
- macOS Sonoma 14.x 和 Sequoia 15.x 都通过

### 红线
- 16-bit 不可控（macOS 强制降到 8-bit）→ B&W 双模式不可能做，整个产品定位推翻
- PJL 命令在 macOS 沙盒被 Apple 拒绝 → MAS 上架不可能，需要重做分发策略
- 无法控制单条墨道 → 单黑模式无法实现

### 应急方案
- 如果 PJL 直连失败：回到 macOS Print Dialog 包装方案，但接受功能降级
  - 失去单黑模式（但保留多色黑通过 Epson ABW 包装）
  - 失去全程 16-bit 保证
  - 失去保存预设可靠性优势
- 这种情况下 jcolour 退化为 "Epson Print Layout 增强版" 而非颠覆者

---

## Spike 3：X-Rite i1 SDK 沙盒行为

### 目标
验证 i1 SDK v3 在 macOS App Sandbox 内能否：
1. 检测到 i1Pro3 / i1Pro3 Plus / i1iO 设备
2. 触发测量 + 取到 36 通道光谱数据
3. 在 MAS 沙盒 entitlements 限制下不被打回

### 实现
- 与 X-Rite 商谈 i1 SDK 商业授权（参考价 $500–2000/年 Runtime License）
- 写最小 macOS App，加 i1 SDK
- 配置 com.apple.security.* entitlements（USB / Bluetooth / network）
- 测一张 24 块 ColorChecker 靶

### 验收标准
- USB 连接 i1Pro3 能识别
- **i1iO 自动扫描臂也能在沙盒内识别并控制**（扫描 A4 靶图，确认 X/Y 定位成功）
  — §6 大靶图（~3600 patch）依赖 i1iO 自动扫描，若不测试会留下高风险盲点
- 测量返回 380–730nm @ 10nm 共 36 通道光谱（i1Pro3 + i1iO 均验证）
- 数据精度与 i1Profiler 应用一致
- MAS 沙盒模拟通过（codesign + 提交至 TestFlight 不被拒）

### 红线
- i1 SDK Runtime License 年费 > $10K → 砸预算
- 沙盒里设备识别失败（i1Pro3）→ 需要 Apple Bluetooth/USB entitlement 申请，可能数月
- **沙盒里 i1iO 扫描臂失败** → 需额外 USB entitlement（机械臂 USB 设备类与 i1Pro3 不同）；若申请受阻，§6 大靶图必须改为 i1Pro3 手扫（约 3600 patch × 手扫时间 = 显著影响建档时效）

### 备选
- ColorMunki SDK（X-Rite 同公司，便宜但精度低）
- 自研 ColorChecker 24 块法（没有真实光谱，但能做大部分功能）

---

## Spike 4：云端 Argyll Profile 生成 API

### 目标
验证云端 Docker 隔离 Argyll 子进程的合规性 + 性能。

### 实现
- Go API 服务（Gin/Fiber）部署在 Cloudflare R2 + 自建容器
- Docker 镜像：debian:slim + ArgyllCMS（GPL，子进程隔离）
- 流程：
  1. 客户端上传测靶光谱数据 (JSON)
  2. 云端转格式 → spawn Argyll `colprof` → 生成 ICC
  3. ICC 通过 R2 签名 URL 返回
- GPL 隔离：Argyll 是子进程，jcolour 客户端不嵌入 → SaaS loophole

### 验收标准
- 端到端往返（上传 → 生成 → 下载）< 30 秒
- 单次成本 < $0.05（基于 R2 + 容器算力 spot price）
- 法律审查：GPL v3 子进程隔离合规
- ICC 质量：**以本地完整 §7A colprof 参数（`-qh -d3 -k r -K <bs>,1,1,<ucr>,<bm>,0 -l <TAC×100>`）生成结果为基准**，云端生成 ICC 前向预测 ΔE 与本地对比一致（平均 ΔE00 < 2.0，最大 < 5.0）；不得以默认 `-qm`（17³）或省略 GCR 参数为基准，否则无法验证 P1 修复带来的质量提升（完整参数规格见 PIPELINE §7A colprof 命令块）

### 红线
- 法律审查认为子进程隔离不够 → 需买商业 Profile SDK 年费 $10K+
- 性能 > 2 分钟 → 用户体验崩坏

---

## Spike 5：最小 UI 端到端（关键 Spike）★

### 目标
**用户能否直观理解 B&W 双模式 + Per-ink 线性化曲线编辑器**。这是 jcolour 差异化体验的核心。

### 实现
- Qt 6 最小 macOS App
- 功能：
  1. 导入 16-bit TIFF
  2. 选纸张 → 选 ICC
  3. **B&W 模式开关：单黑 / 多色黑**
  4. 单黑模式 UI：PK/MK 选择 + 色温微调
  5. 多色黑 UI：三段色环（高光/中间调/阴影）+ 全局中性度滑块
  6. **Per-ink 线性化曲线编辑器原型**（PK/MK/LK/LLK 四曲线，可拖控制点）
  7. 软打样预览（纸白模拟）
  8. 调 Spike 2 的 PJL 直连发送打印
  9. 用 Spike 3 的 i1Pro3 集成测灰阶回采

### 验收标准
- 摄影师种子用户（5 位）盲测：能在 5 分钟内打出第一张 B&W 单黑、10 分钟内打出 split-tone 多色黑
- 与 QuadToneRIP 对照盲测：摄影师反馈 jcolour UI 易用度提升 ≥ 5 倍
- Per-ink 线性化：测一次靶 → UI 自动绘曲线 → 用户拖动 → 实时灰阶预览刷新

### 红线
- 5 位种子用户 ≥ 3 位看不懂三段色环 UI → 重新设计 B&W 模式 UX（推到 Multi-channel Black 仅做单一色温滑块）
- 控制点曲线编辑器交互体验差 → 简化为预设组合（v1.0 不暴露曲线编辑器）

---

## Spike Gate 决策矩阵

```
全 5 项 PASS  → 进入 Early Access 开发，按 ROADMAP 走
Spike 2 红线 → 重新评估 PJL 方案 / 改 macOS Print Dialog 包装路径
Spike 5 红线 → B&W UX 简化，保留双模式但 v1.0 不做曲线编辑器
其他单项红线 → Spike 延长 1–2 周修复
≥ 2 项红线  → 推迟 M0 日期 1 个月，重新审视架构
≥ 3 项红线  → 项目暂停，回到 PLAN 重新选择技术栈
```

---

## 未入 Spike 的 P0 技术风险（显式挂起）

以下 P0 功能（见 PLAN.md §3）未设独立 Spike，在 5 Spike 通过后优先处理：

| 风险项 | 影响 | 建议处置 |
|--------|------|---------|
| **SemanticGamutMapper（CoreML 语义分割）** | PLAN §3 P0 第 7 项；需 CoreML 模型训练/授权 + ANE 推理性能验证；若 CoreML 不可行则 GamutAdvisor 降级为纯直方图模式（非语义）；执行规范见 [`GAMUT-PIPELINE.md`](GAMUT-PIPELINE.md) | Early Access M0 时增加 Spike 6（CoreML 语义分割可行性，1 周）：验证肤色/天空/植物分割精度 + A16/M1 ANE 推理 < 500ms/张 |

---

## 相关文档

- PLAN：[`PLAN.md`](PLAN.md)
- ROADMAP：[`ROADMAP.md`](ROADMAP.md)
- B&W 详细设计：[`BW-WORKFLOW.md`](BW-WORKFLOW.md)
- Epson 痛点：[`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md)
- ADR-26 PJL 直连决策：[`../../docs/DECISIONS.md`](../../docs/DECISIONS.md)
