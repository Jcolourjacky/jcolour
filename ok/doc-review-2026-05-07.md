# 技术文档全面审核报告

> **日期**：2026-05-07
> **范围**：PIPELINE-gcolour.md §1–§15 + 附录 A、WORKFLOW-gcolour.md（生产/维护模式 + UI 布局 + 数据模型）、UI-SPEC-FIERY.md、apps/jcolour/PIPELINE.md
> **触发**：本地 Argyll A19 E2E 测试通过，评估云端部署前的文档完整性

---

## 总体结论

**三份文档整体质量高，架构设计完整自洽。** 发现 5 个 PIPELINE 问题（P1–P5）+ 2 个 WORKFLOW 问题（W1–W2），均为文档级别的遗漏或歧义，无架构级问题。云端部署骨架已就绪，但尚未达到生产标准，需完成 5 项前置工作。

---

## 一、PIPELINE-gcolour.md §1–§15 + 附录 A

### ✅ 已确认正确

- **§2 OBA 检测前移**：已在建档最早期执行，后续所有阶段（§6、§7B）直接读取 `media_config.oba_status`，无重复检测
- **§6.5 批量门禁**：相对阈值（3%-8% WARN, >8% BLOCK）正确——TC4 的 3% ≈ 49 块，CYNSN 靶的 3% ≈ 12 块，与文档说明一致
- **§7B LUT 版本控制**：三种情形（VALID / SOFT_VALID / STALE）与 §14 轻校三级处置完全对应，无矛盾
- **§8 active.json 写入时机**：明确在 §8 建档完成后写入，calibration_baseline.json 也在此时写入，§8.5 "首次出样基准必定存在"的假设成立
- **§10 对象级路由**：以连通区域为单位 + 边界外扩 0.15mm（≈ 720dpi 下 4px），消除专色边缘色带
- **§11 B-FALLBACK 三种类型**：Type-A（回退 Route A）/ Type-B（主导专色近似）/ Type-C（BLOCK），覆盖所有降级场景
- **§12 TCP RAW 9100 + Bonjour mDNS**：MAS Sandbox 兼容，打印机用序列号持久标识，DHCP 环境安全
- **§15 快速/精确双路径**：快速诊断用现有控制条数据（无需额外打印）；精确诊断打印 112 patch 诊断靶；area curve 同步更新已说明

### ⚠️ 发现的问题

#### P1 — gcolour §7A colprof 命令缺少 `-qh` 和 `-d3`

**当前文档**（PIPELINE-gcolour.md §7A）：
```
colprof -v -D "PrinterName_Media_Date"
        -q h
        -k r
        -K 0.5,1,1,0.3,0.25,0
        -l <TAC_total>
        -S <source.icc>
        -c mt
        profile_name
```

> 注：命令中有 `-q h`（带空格），但标准 Argyll 语法应为 `-qh`（无空格）。需确认。

**对比 jcolour §6**：
```
colprof -qh -al -kp -l<TIL> -d3 -u <linearization>
```

**问题**：
1. `-qh` / `-q h` 语法需统一（Argyll colprof 文档用 `-qh`，无空格）
2. gcolour 命令未显式指定 `-d3`（12 通道维度），虽 colprof 可从 .ti3 自动推断，但显式更安全
3. gcolour 命令未指定 `-qh`（High Quality LUT = 33³），若省略默认为 `-qm`（Medium = 17³），精度差 3–4 倍

**建议**：在 §7A 中将命令修正为：
```
colprof -v -D "PrinterName_Media_Date"
        -qh
        -k r
        -K 0.5,1,1,0.3,0.25,0
        -l <TAC_total>
        -d3
        -S <source.icc>
        -c mt
        profile_name
```

---

#### P2 — `calibration_baseline.json` 结构未定义

**现状**：§8、§8.5、§14 均引用此文件，但文档中没有定义字段结构。其他所有数据文件（ink_limit.json、active.json、measurement_qa.json 等）都有完整的 JSON 示例。

**建议**：在 §8（绑定打样标准）末尾补充 schema，例如：
```json
// calibration_baseline.json（§8 建档时写入，§8.5 AutoCal Check 对比基准）
{
  "created_at": "2026-05-07T10:30:00Z",
  "print_condition_id": "EPSON_P9500_SN123456_FANTAC_UH190",
  "measurement_mode": "M1",
  "patches": [
    {"name": "Paper White",  "Lab": [96.2, -0.3, 1.8]},
    {"name": "100C",         "Lab": [55.3, -36.1, -48.4]},
    {"name": "100M",         "Lab": [46.8, 71.0, -2.5]},
    {"name": "100Y",         "Lab": [88.2, -5.9, 91.0]},
    {"name": "100K",         "Lab": [16.0, 0.2, -0.1]},
    // ... CMY 二次色 + 灰阶 3 档 + 专色实地（最多 5）
  ],
  "avg_dE00_vs_standard": 0.9
}
```

---

#### P3 — G7+ 校正曲线输出（g7_validation.h）未实现，无跟踪条目

**现状**：附录 A.4.2 注释：
> "已实现：`g7_validation.h` → `G7ComplianceChecker`（验证部分），**需扩展校正曲线输出**"

这是一个明确的 engine 实现 gap，但在 `apps/jcolour/SPIKES.md` 和 `apps/jcolour/ROADMAP.md` 中没有对应跟踪条目。

**影响**：
- §4 G7+ NPDC 校正的"输出 g7_curve.lut"步骤依赖此功能
- Spike 1（色彩管线 CLI）包含此模块，但未细化到"校正曲线输出"子任务

**建议**：在 SPIKES.md Spike 1 中增加子任务，或在 `engine/g7/g7_validation.h` 中添加 TODO 注释。

---

#### P4 — §9 W（白墨）通道与 MaxSpots ≤ 8 的关系未说明

**现状**：§9 声明 `MaxSpots ≤ 8`（Epson P9500 12 通道 - 4 CMYK），但 §3 的通道列表包含 W（白墨）。

**问题**：白墨（W）通常用于透明/深色基材打印。若 Ghostscript 将 W 视为 DeviceN 专色通道，则 MaxSpots 的 8 个限额中 1 个被 W 占用，实际可用专色数为 7 个。

**建议**：在 §9 注释中明确：
- 标准纸张模式（无白墨）：MaxSpots = 8（全部可用于客户专色）
- 白墨基材模式：W 通道占用 1 个 DeviceN 槽，MaxSpots 实效 = 7

---

#### P5 — §15 轻修中 area curve 重拟合流程未引用 A.3.4

**现状**：
- §15 的"轻修"说"同步更新 cynsn_model.bin 中该通道的 effective_area_curve（Murray-Davies 重拟合，见 A.3.4）"
- A.3.4 给出了 Murray-Davies 修正的数学公式
- 但 §15 的流程代码没有说明如何从"8 级诊断 ramp"触发重拟合

**建议**：在 §15 的"轻修"步骤中补充伪代码：
```
// 轻修后 area curve 同步更新（A.3.4 Murray-Davies 修正）：
for ink_level in [14%, 29%, 43%, 57%, 71%, 86%, 100%]:
    R_measured = measured_spectrum[top_drift_channel][ink_level]
    a_eff = murray_davies_yn(R_measured, R_paper, R_solid, n_yn)
    area_curve_points[top_drift_channel][level] = a_eff

// 重新三次样条拟合 8 个 (ā, a_eff) 点
// 更新 cynsn_model.bin 中该通道的 effective_area_curve
cynsn_model.effective_area_curves[top_drift_channel] = refit_spline(area_curve_points)
```

---

## 二、WORKFLOW-gcolour.md

### ✅ 已确认正确

- **PrintCondition 状态机**：8 种状态（UNCONFIGURED → HARDWARE_OK → CALIBRATED → PROFILED → READY → NEEDS_RECAL / DEGRADED / BATCH_CHANGED / ON_HOLD）与颜色系统完整
- **三大工作模式**：Setup（§1–§8）/ Production（§9–§13）/ Maintenance（§14–§15）对应关系清晰
- **AutoCal Check Upfront**：每 Job 前自动执行，与 PIPELINE §8.5 一致，GREEN/AMBER/RED 三档处理
- **Dynamic Control Strip**：Media Wedge V3（72 patch）+ 动态专色追加，两者共存，与 PIPELINE §12 一致
- **Job 状态机**：QUEUED → AUTOCAL → RENDERING → ROUTING → CONVERTING → PRINTING → MEASURING → VERIFIED → DONE，完整
- **NEEDS_RECAL 出样策略**：允许出样但标注 `SOFT-UNCAL`，比 GMG 更灵活，可按条件配置
- **ON_HOLD 机制**：不自动恢复，操作员手动释放，急件可绕过并留痕
- **Profile Checksum 保护**：SHA-256，文件变更 → DEGRADED，与 PIPELINE 存储管理一致
- **跨机一致性验证**：≤1.0 PASS / 1.0-2.0 WARN / >3.0 FAIL，阈值合理

### ⚠️ 发现的问题

#### W1 — 向导映射表（第八节）§13 描述易混淆

**现状**：映射表中"向导 Step 8 子步骤 8.3"标注为"§13 ISO 验证"，但 PIPELINE 的 §13 属于"出样期"章节，不在"建档期"。

**事实**：首次验证打样（Step 8.3）确实在建档期执行，逻辑正确。但读者可能误认为"建档期调用了出样期的 §13"，造成理解混乱。

**建议**：将映射表中的描述改为"§13 **首次验证实例**（建档期执行一次）"，与出样期的 §13 区分。

---

#### W2 — Maintenance Mode 触发表与 §14 三级处置入口不一致

**现状**：

WORKFLOW §六 触发表：
```
CYNSN RMSE > 0.03（诊断报告） → 重建 CYNSN 该通道 Primary，~60 分钟
```

PIPELINE §14：
```
ΔE00 ≥ 3.0 → 第一级（§15 精确诊断）
  → 若 CYNSN RMSE > 0.05 → 第二级（重建线性化层）
  → 若仍不收敛 → 第三级（完整重建）
```

**差异**：WORKFLOW 将"CYNSN RMSE > 0.03"作为独立的 Maintenance 触发条件（用户直接从诊断报告进入），而 PIPELINE §14 将其嵌套为 ΔE > 3.0 之后的 §15 诊断结果。两者逻辑相同，入口描述不同。

另外阈值也有差异：WORKFLOW 用 > 0.03，PIPELINE §14 的第二级用 > 0.05。需统一。

**建议**：WORKFLOW 触发表改为：
```
通过 §15 精确诊断，CYNSN RMSE > 0.03（操作员主动或 ΔE > 3.0 触发）→ 重建 CYNSN 该通道 Primary，~60 分钟
```
同时确认阈值统一为 0.03（PIPELINE §7B 验收门禁）还是 0.05（PIPELINE §14 第二级）。

---

## 三、UI-SPEC-FIERY.md

**总体**：jcolour 专色编辑器的 Qt 样式规范，与 gcolour WORKFLOW 的六视图布局不冲突（针对不同产品）。颜色主题 CSS 变量可被 gcolour 直接复用。

**一个问题**：
- §10 参考资源：`Fiery Command WorkStation 截图: /Users/jacky/Desktop/20250527-014.jpg`
- 这是开发机绝对路径，其他人无法访问，也无法进版本控制。
- **建议**：将截图移到 `docs/screenshots/fiery-cws-reference.jpg` 并更新引用，或删除路径只保留说明。

---

## 四、数据模型完整性

| 数据文件 | 结构定义 | 状态 |
|---------|---------|------|
| `media_config.json` | §2 完整 JSON | ✅ |
| `ink_limit.json` | §3 完整 JSON | ✅ |
| `g7_curve.lut` | §4 格式描述 | ✅ |
| `linearization.lut` | §5 格式描述 | ✅ |
| `measurement_qa.json` | §6.5 完整 JSON | ✅ |
| `cynsn_model.bin` | §7B 头部格式（magic + version + hash）| ✅ |
| `cynsn_lut.bin` | §7B 头部格式 | ✅ |
| `gamut_boundary.gbd` | §7B 描述（~200KB 凸包），无详细格式 | ✅ 可接受 |
| `active.json` | §8 完整 JSON（PrintCondition 指针）| ✅ |
| **`calibration_baseline.json`** | 仅引用，无 schema | ⚠️ **缺失**（见 P2）|
| `SpotColorLibrary CxF/X-4` | 完整 XML 示例 | ✅ |
| `profile_metadata.json` | 仅提及，无 schema | ⚠️ 轻微缺失 |
| `route_mask` | §10 注释中有结构体定义 | ✅ |

---

## 五、云端 Argyll 部署评估

### 本地测试状态

✅ `engine/tests/test_argyll.cpp` A19 测试通过（最新 commit: `4e6192b`）：
- `targen` 生成 `.ti1` 测试靶（5 样本，11-bit，density 模式）
- `colprof` E2E 生成 >100KB ICC profile（含 "argl" 签名，Argyll 特征）
- 重复测量 ΔE 平均 < 2.0（通过质量阈值）

### 云端现状

| 组件 | 文件 | 状态 |
|------|------|------|
| Argyll Docker 容器 | `cloud/argyll/Dockerfile` | ✅ debian:bookworm-slim + ArgyllCMS |
| Go API 骨架 | `cloud/api/` | ⚠️ 骨架已有（Gin + PostgreSQL + Redis + R2），colprof 子进程实现待确认 |
| PostgreSQL schema | 未见 migration 文件 | ❓ |
| Redis 配置 | 未见 | ❓ |
| Cloudflare R2 | 环境变量方式，未见 `.env.example` | ❓ |
| 部署脚本 / CI | 未见 | ❓ |

### 部署前必须完成（按优先级）

1. **`cloud/api/` colprof 子进程实现**
   - 参考 `engine/argyll/argyll.cpp` 的本地实现（进程超时、stdout/stderr 捕获）
   - 实现 `POST /api/v1/jcolour/icc/generate` → 接收 .ti3 → spawn Docker colprof → 上传 R2

2. **数据库 migration 文件**（job 状态表：job_id, status, created_at, r2_key, error_msg）

3. **`.env.example`**（R2 bucket/endpoint/key、Postgres DSN、Redis URL、API auth key）

4. **容器健康检查**（验证 colprof 可执行：`colprof -V`）

5. **本地 Docker Compose 端到端冒烟测试**
   - 无需真实云服务，用 MinIO 模拟 R2
   - 流程：.ti3 上传 → 异步 colprof → ICC 轮询下载
   - 通过后再部署到真实云环境

### 结论

> **当前状态：本地验证通过，云端骨架就绪，未达生产部署标准。**
>
> 建议路径：完成 5 项前置工作 → 本地 Docker Compose 冒烟测试通过 → 再上线。

---

## 六、问题优先级汇总

| ID | 来源 | 优先级 | 描述 | 修复难度 |
|----|------|--------|------|---------|
| P1 | PIPELINE-gcolour §7A | 🔴 高 | colprof 命令缺 `-qh -d3`，可能生成低精度 ICC | 一行改动 |
| P2 | PIPELINE-gcolour §8 | 🟡 中 | calibration_baseline.json 无 schema | 补文档 |
| P3 | 附录 A.4.2 | 🟡 中 | g7_validation.h 校正曲线未实现，无跟踪 | 加 TODO/任务 |
| P4 | PIPELINE-gcolour §9 | 🟡 中 | W 通道与 MaxSpots 关系未说明 | 补注释 |
| P5 | PIPELINE-gcolour §15 | 🟢 低 | area curve 重拟合无伪代码 | 补文档 |
| W1 | WORKFLOW §八 | 🟢 低 | §13 首次验证描述易混淆 | 改措辞 |
| W2 | WORKFLOW §六 | 🟡 中 | CYNSN RMSE 阈值不一致（0.03 vs 0.05） | 统一阈值 |

---

*审核人：Claude Code | 日期：2026-05-07*
