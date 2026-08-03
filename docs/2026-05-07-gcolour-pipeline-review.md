# gcolour 规划评审日志 — 2026-05-07

> **项目**：gcolour CMYK 打样色彩管理工作站
> **工作内容**：竞品对标 → 规划修改 → 多轮再评审 → 问题修复
> **涉及文档**：PIPELINE-gcolour.md / PLAN-gcolour.md / WORKFLOW-gcolour.md

---

## 一、整体流程

```
竞品调研（EFI Fiery XF / GMG ColorProof / ColorLogic ColorAnt）
    ↓
第一轮架构修改（8 项决策）
    ↓
规划再评审 R1–R15（15 个问题，竞品参考修复）
    ↓
最终综合审核 F1–F13（13 个问题，竞品参考修复）
```

---

## 二、第一轮架构修改（8 项决策）

### B-Fix-4：逐像素 SQP → 预计算 33³ LUT
- **问题**：Route B 逐像素 SQP 50 iter，A2@720dpi 20% Route B ≈ 20 分钟，不可用
- **竞品参考**：GMG OpenColor"build-time model / runtime lookup"分离原则
- **方案**：建档期 SQP 预计算 33³ Lab→Device LUT（GCD 并行，M1 Max 约 3-5 分钟）；出样期三线性插值 + 1-3 iter 局部梯度下降，A2@720dpi ≤ 2-3 分钟
- **存储**：`cynsn_lut.bin`（约 1MB，33³ × 12ch × uint16）

### TCP RAW 9100 打印机连接
- **问题**：macOS App Sandbox 的 `com.apple.security.print` 为受限授权，Apple 不向第三方开放
- **竞品参考**：GMG + EFI 均通过 TCP socket :9100 绕过 CUPS
- **方案**：TCP RAW 9100，只需标准 `com.apple.security.network.client`
- **打印机标识**：序列号（`EPSON_P9500_SN{SN}`，via `@PJL DINQUIRE SERIAL`）+ Bonjour 动态解析 IP，防 DHCP 漂移

### 校准有效期默认值 14 天 → 30 天
- **竞品参考**：EFI Fiery 行业默认 30 天

### OBA 检测移至 §2（建档最前端）
- **问题**：原在 §6.5，打完所有靶图才检测，浪费纸张
- **方案**：§2 介质注册时同步测 M0 + M1，ΔE ≥ 3.0 锁定 `measurement_mode=M1`

### Dynamic Control Strip 改为并存（非替代）
- **竞品参考**：EFI Dynamic Wedge — ISO Media Wedge V3 + 动态专色追加，两者共存
- **方案**：`[UGRA/FOGRA Media Wedge V3 (72 patch)] | [Dynamic patches]`

### 专色名称 6 步规范化 + Levenshtein 模糊匹配
- 解决 PDF 内"PMS 485 C" vs "Pantone 485 C" vs "PANTONE 485C"命名不一致
- d ≤ 1 自动接受；d = 2 确认；d > 2 手动绑定

### 存储架构分离（媲美 GMG MX3/MX4）
- `media_profiles/<MediaName>/`：ICC + CYNSN 模型，跨打印机共享，建一次
- `linearization/<PrinterID>/<MediaName>/`：线性化，per-printer，日常重校
- 重校只动 linearization/，不触 media_profiles/，速度快 10×

### active.json + §8.5 AutoCal Check Upfront
- `active.json`：§8 完成时创建，记录 media_profile_path + linearization_path + checksum（SHA-256）
- `§8.5 AutoCal Check`：每 Job 出样前自动打迷你控制条，对比 calibration_baseline.json（参照 GMG AutoCali Wizard）
- GREEN（ΔE < 1.5）→ 直接出样；AMBER（1.5-3.0）→ Quick Cal；RED（≥ 3.0）→ ON_HOLD

---

## 三、第一轮再评审（R1–R15）

| # | 问题 | 来源 | 修复方案 |
|---|------|------|---------|
| R1 | §7B 验收无 PASS/WARNING/FAIL 三级门禁 | GMG profile quality gate | 新增三级门禁表（RMSE/ΔE00/R² 各阈值） |
| R2 | §15 单通道修正后 area curve 不同步 | GMG 整体重建缺陷的反面教材 | 轻修后同步 Murray-Davies 重拟合 effective_area_curve |
| R3 | OBA 荧光 R(λ) > 1.0 在 CYNSN 模型中未处理 | GMG OpenColor OBA 处理规范 | R_normalized = R / R_substrate 前归一化 |
| R4 | §14 轻校后 LUT 状态无更新机制 | EFI LUT versioning | linearization_hash（SHA-256）+ VALID/SOFT_VALID/STALE 状态机 |
| R5 | B-FALLBACK 对多专色像素无 Route A 可用 | 内部分析 | Type-A/B/C 三种降级策略 |
| R6 | Dynamic Control Strip 与 ISO Media Wedge 互斥 | EFI Dynamic Wedge 文档 | 改为并存：Media Wedge + 动态追加 |
| R7 | GCR 参数在 §3 配置但 §4 才测量 | 内部分析 | §3.3 GCR 是配置输入（非测量输出），加注释说明顺序无冲突 |
| R8 | active.json first-use 为空时崩溃 | 内部分析 | 注意到，F3 完整修复 |
| R9 | TAC > 380% 无 WARN 上限 | 打印机物理约束 | TAC > 380% 强制 WARN + cap at min(measured, 380%) |
| R10 | 打印机 IP 变化导致 PrintCondition 失效 | EFI Fiery 序列号标识 | 序列号持久化 + Bonjour 动态解析 IP |
| R11 | §3→§4→§5 GCR 参数传递路径不明 | 内部分析 | GCR 从 §3 ink_limit.json 流向 §7A Argyll colprof 参数，文档补充说明 |
| R12 | calibration_baseline.json 创建时机不明 | GMG AutoCali Wizard baseline | §8 建档完成时写入，作为长期基准 |
| R13 | §7B 无 Gamut Boundary Descriptor | GMG 色域预检机制 | GBD（凸包，约 200KB），O(log N) 最近可达点查找 |
| R14 | Route A 多专色强制覆盖策略未定义 | EFI 合规 override 机制 | 四种场景 × 是否允许强制 A 的决策表 |
| R15 | §15 快速诊断与精确诊断未区分 | GMG 诊断分层 | 快速（日常，控制条数据）/ 精确（96 patch 专用靶，手动触发） |

---

## 四、最终综合审核（F1–F13）

### 🔴 严重（5 个）

**F1 — §11 Route B 链路保留旧 SQP 描述，与新 LUT 算法并存**
- 症状：Route B 链路代码块里还有 `α·spectral_RMSE + β·ΔE00...` 的 SQP 成本函数，和下方 LUT 算法并存，开发者看不懂哪个是运行时
- 修复：旧 SQP 块替换为注释 `// 反求解目标函数见 A.6（LUT 构建期使用，非运行时）`
- 竞品参考：GMG OpenColor"build-time / runtime"分离原则

**F2 — §12 PJL `@PJL ENTER LANGUAGE=EPSONPCLXL` 协议错误**
- 症状：PCL-XL 是 HP 的打印语言，Epson P9500 用 ESC/P-R；发送 EPSONPCLXL 会导致打印机拒绝整个作业
- 修复：删除该行，加注释说明 RAW 9100 模式打印机自动进入 ESC/P-R 接收状态

**F3 — §8.5 首次 Job 出样时 `calibration_history` 为空，null 引用崩溃**
- 症状：新建的 PrintCondition 刚进入 READY，`calibration_history[]` 为空，§8.5 读最后一条记录会崩溃
- 修复：三路分支 — 空 history 用 `calibration_baseline.json` / 连 baseline 也没有则 WARN 跳过 / 有 history 才读最后一条
- 竞品参考：GMG AutoCali Wizard 的 first-use baseline 策略

**F4 — active.json schema 缺 `lut_rebuild_date` 字段**
- 症状：§14 checksum 节写"更新 `active.json.lut_rebuild_date`"，但 active.json 示例无此字段，实现无参考
- 修复：active.json 示例补充 `"lut_rebuild_date": null` 和 `"lut_status": "VALID"`

**F5 — WORKFLOW §5 作业流转图写"CYNSN inverse SQP"，与 LUT 架构矛盾**
- 症状：WORKFLOW-gcolour.md 第 444 行用了 SQP 术语，操作员培训材料若从这里取用会传达错误概念
- 修复：改为"预计算 LUT 三线性插值 + 局部梯度下降精化（GCD tile 并行，显示 ETA）"

### 🟡 中等（5 个）

**F6 — §14 大偏差直跳完整重建，跳过 §15 中间路径**
- 症状：ΔE ≥ 3.0 → 直接提示"完整重建 §3-§7（3 小时）"，但 §15 光谱诊断（30-60 分钟）是更合理的中间选项
- 修复：三级渐进处置 — §15 精确诊断 → 线性化层重建 → 完整重建
- 竞品参考：EFI Fiery"渐进式校准：日常 / 深度 / 重建"三级策略

**F7 — `E_gamut_smooth` 用到的 `boundary_Lab` 未定义 §10→§11 数据流**
- 症状：A.6.2 说 boundary_Lab 来自"§10 超色域预检"，但 §10 和 §11 是两个独立管线节点，数据怎么传从未定义
- 修复：定义 `RoutePixelMeta` 结构体（route / is_out_of_gamut / boundary_Lab / fallback），§10 写入，§11 读取
- 竞品参考：EFI Fiery per-pixel metadata 管线贯通设计

**F8 — WORKFLOW 属性面板 mock 数据显示"14 天"有效期（规格是 30 天）**
- 修复：改为 30 天，保持与 §14 规格一致

**F9 — WORKFLOW §2 OBA 检测只写了 `R(λ) > 1.0` 判断，缺 M0+M1 双测量步骤**
- 症状：§2 向导步骤描述不完整，操作员不知道需要切换 M0 模式补测
- 修复：补充 M0 测量 + ΔE(M0,M1) 三档判定（OBA_NONE / OBA_WEAK / OBA_STRONG）+ 380-450nm 光谱对比曲线
- 竞品参考：ISO 13655:2017 M0/M1 对比判定荧光增白剂的标准方法

**F10 — §15"验证 patch"只写了"打印验证 patch"，未定义内容**
- 修复：明确为"该通道 8 级 ramp，8 个 patch，约 2 分钟，RMSE < 0.020 为通过"

### 🔵 低优先级（3 个）

**F11 — PLAN §3 Route B 竞品对比仍说"CYNSN spectral inverse solve"旧术语**
- 修复：改为"CYNSN 预计算 33³ LUT + 局部精化（A2@720dpi ≤ 2-3 分钟）"

**F12 — SpotColorLibrary CxF 示例 namespace 与 ISO 17972-4 声明不匹配**
- 症状：文档声明用"ISO 17972-4:2015"但 namespace 是 ColorLogic 私有 CxF/3
- 修复：加注释说明 CxF/3 = ISO 17972-4 基础层，互通工具均可接受

**F13 — §7B 完成后 §8 崩溃时孤立 Profile 无恢复路径**
- 症状：printer.icc / cynsn_model.bin 已写入但 active.json 未创建，重启后无法从 Step 8 续建，需要重跑全部 §3-§7（2.5 小时）
- 修复：PIPELINE 存储节增加"孤立 Profile"启动时扫描逻辑；WORKFLOW 状态机增加 PROFILED（孤立）→ 从 Step 8 继续
- 竞品参考：GMG"条件复用"思路

---

## 五、关键架构决策速查

| 决策 | 结论 |
|------|------|
| Route B 出样性能 | 预计算 33³ LUT（约 1MB），三线性插值 + 1-3 iter 局部精化 |
| 打印机连接 | TCP RAW 9100（IP:9100），无需 CUPS |
| 打印机标识 | 序列号持久化 + Bonjour 动态 IP |
| 存储分离 | media_profiles（共享）/ linearization（per-printer） |
| 校准有效期默认 | 30 天 |
| OBA 检测时机 | §2 介质注册，M0+M1 双测量 |
| 控制条格式 | ISO Media Wedge V3（72 patch）+ 动态专色追加 |
| 光谱格式 | CxF/X-4 主存储 + .ti3 Argyll 备用，双轨持久化 |
| Route A/B 边界 | 对象级路由（connected component），不做像素级切割 |
| §14 大偏差恢复 | §15 精确诊断 → 线性化重建 → 完整重建（三级渐进） |
| §8.5 first-use | 无 calibration_history 时用 calibration_baseline.json 作基准 |
| active.json 字段 | 含 lut_rebuild_date / lut_status / checksum_profile / checksum_linearization |
