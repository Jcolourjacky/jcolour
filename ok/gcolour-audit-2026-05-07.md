# gcolour-packages.md 审核报告

> **审核日期**：2026-05-07
> **被审核文件**：`/Users/jacky/Desktop/jcolour/ok/gcolour-packages.md`（G01–G58）
> **参考文档**：
> - `docs/PIPELINE-gcolour.md`（§1–§15 + 附录 A）
> - `docs/WORKFLOW-gcolour.md`（生产/维护/UI/状态机）
> - `docs/UI-SPEC-FIERY.md`（Qt 组件规范）

---

## 总体结论

58 个工作包覆盖了核心管线的大部分路径，但存在 **8 项规格错误**（直接影响运行时正确性，必须在编码前修复）和 **6 项遗漏包**（未被任何现有包覆盖的功能域）。

---

## 一、规格错误（Critical — 编码前必须修复）

### E01 — G20 magic bytes 字节序错误
**包**：G20（`engine/src/cynsn.h`，cynsn_model.bin 序列化）

| | 当前 G20 | PIPELINE §7B 规格 |
|--|---------|----------------|
| magic[4] | `"CYSN"` | `0x43 0x59 0x4E 0x53`（= `"CYNS"`）|

**错误**：N 和 S 互换（`"CYSN"` vs `"CYNS"`）。

**修正**：将 G20 文件格式注释中 `"CYSN"` 改为 `"CYNS"`，对应的读写验证 magic 比对必须用 `{'C','Y','N','S'}` 字节数组。

---

### E02 — G22 magic bytes + version 字段类型双错
**包**：G22（`engine/src/cynsn.h`，cynsn_lut.bin 读写）

| | 当前 G22 | PIPELINE §7B 规格 |
|--|---------|----------------|
| magic[4] | `"CLUT"` | `0x43 0x59 0x4E 0x53`（= `"CYNS"`）|
| version 字段 | `uint16_t = 1`（2 字节）| `uint32_le = 1`（4 字节）|

**影响**：两文件格式不一致。`version` 类型错导致 `linearization_hash[32]` 偏移计算错（文件中实际偏移 6 字节而非 8 字节），读出的 hash 和 LUT 数据全部错位。

**修正**：
```
[0..3]   magic bytes: 0x43 0x59 0x4E 0x53  ("CYNS")  ← 与 cynsn_model.bin 一致
[4..7]   version: uint32_le（当前 1）                  ← 4 字节，不是 uint16
[8..39]  linearization_hash: 32 bytes SHA-256
[40+]    LUT 节点数据
```
文件大小精确：4+4+32 + 35937×12×2 = 40 + 862,488 = **862,528 字节**（注：G22 现写 862,534 是用旧偏移算出的错误值）。

---

### E03 — G16 S-G 多项式阶数错 + k-NN 参数不区分路线
**包**：G16（`engine/calibration/spectral_cleaning.h`）

**问题 1 — 多项式阶数**：

| 区间 | 当前 G16 | PIPELINE §6.5 |
|------|---------|--------------|
| 标准区 440–680nm | `多项式阶=2` | `多项式阶数=3` |
| OBA 区 380–440nm | `多项式阶=2` | 窗口=3（阶数未明确，沿用=3 即可）|
| 末端区 680–730nm | `多项式阶=2` | 窗口=3（同上）|

标准区阶数写 2 是错的，S-G 窗口=5 配合阶数=2 曲率约束过弱，谱峰会被压平。

**问题 2 — k-NN 参数不区分 §7A / §7B**：

PIPELINE §6.5 明确要求两条路线独立清洗：

| 参数 | §7A（ICC 生成） | §7B（CYNSN 建模） |
|------|---------------|-----------------|
| OUTLIER 阈值 | RMSE > **0.030** | RMSE > **0.025** |
| k-NN k 值 | **8** | **12** |

G16 的 `clean_spectrum` 签名只有单个 `rmse_threshold=0.030f`，k 值由调用方传入但无路线区分文档，§7B 需要 0.025 阈值。

**修正**：
```cpp
// 明确两套参数
constexpr float SG_POLY_DEGREE_STANDARD = 3;   // 440-680nm
constexpr float SG_POLY_DEGREE_EDGE     = 2;   // 380-440nm, 680-730nm（可用 3，降到 2 也可）

// 函数签名增加路线区分
enum class CleaningRoute { ROUTE_A, ROUTE_B };
CleaningResult clean_spectrum(const spectral::Spectrum& raw,
                               const std::vector<spectral::Spectrum>& neighbors,
                               CleaningRoute route);
// ROUTE_A: k_default=8, rmse_threshold=0.030
// ROUTE_B: k_default=12, rmse_threshold=0.025
```

---

### E04 — G09 GCRParams 缺少 `gcr_amount` 字段
**包**：G09（`engine/calibration/ink_limit.h`，ink_limit.json 读写）

PIPELINE §3.3 输出 JSON：
```json
"gcr": {"black_start": 0.20, "black_max": 0.95, "gcr_amount": 0.70, "ucr": 0.30}
```

G09 的 `GCRParams` 只有 3 个字段：
```cpp
struct GCRParams {
    float black_start = 0.20f;
    float black_max   = 0.95f;
    float ucr         = 0.30f;
    // ← 缺少 gcr_amount = 0.70f
};
```

**修正**：
```cpp
struct GCRParams {
    float black_start  = 0.20f;
    float black_max    = 0.95f;
    float gcr_amount   = 0.70f;  // 灰分替换强度（0–1），§3.3 ADR-12 参数
    float ucr          = 0.30f;
};
```
注：`gcr_amount` 目前不直接传入 `colprof -K`（Argyll 的 `-K` 格式为 `bs,1,1,ucr,bm,0`），但 ink_limit.json schema 有此字段，G18/G40 colprof 调用无需修改。

---

### E05 — G35 LUT 状态边界值错（`<` vs `≤`）
**包**：G35（`engine/calibration/quick_cal.h`，LUT 状态机）

| | G35 注释 | PIPELINE §7B 情形 B |
|--|---------|---------------------|
| SOFT_VALID 条件 | `post_cal_dE < 1.5`（严格小于）| `ΔE00 ≤ 1.5`（含等号）|

G35 测试用例：`post_cal=1.5 → STALE`。
PIPELINE 规定 ΔE00 ≤ 1.5 为 SOFT_VALID，`post_cal=1.5` 应返回 **SOFT_VALID**，不是 STALE。

**修正**：将 `<` 改为 `<=`，同时修正测试用例：`post_cal=1.5 → SOFT_VALID`。

---

### E06 — G02 + G24 active.json 缺少 `checksum_linearization` 字段
**包**：G02（`engine/calibration/active_config.h`）、G24（`engine/calibration/binding.h`）

PIPELINE §8：
```
→ 计算并写入 checksum_profile（SHA-256 of printer.icc + cynsn_model.bin）
→ 计算并写入 checksum_linearization（SHA-256 of g7_curve.lut + linearization.lut）
→ 此后每次 §14 轻校：更新 active.json 中的 calibration_valid_until + checksum_linearization
```

G02 的 `ActiveConfig` 只有 `checksum_profile`，缺少 `checksum_linearization`。

**修正**：
```cpp
struct ActiveConfig {
    std::string print_condition_id;
    std::string paper_id;
    std::string ink_batch_id;
    std::string checksum_profile;        // SHA-256(printer.icc || cynsn_model.bin)
    std::string checksum_linearization;  // ← 新增。SHA-256(g7_curve.lut || linearization.lut)
    std::string measurement_mode;
    int64_t     built_at           = 0;
    int64_t     calibration_valid_until = 0;  // ← 新增。§14 每次校准后更新
};
```

同步修正 G24 的绑定注释步骤 4：`写 active.json（含 checksum_profile、checksum_linearization、measurement_mode）`。

---

### E07 — G31 Dynamic Control Strip 固定 patch 数量错
**包**：G31（`engine/epson/job.h`，动态控制条生成）

| | G31 | WORKFLOW §五 |
|--|-----|------------|
| 固定 patch 构成 | `UGRA/FOGRA Media Wedge V3 = 72 固定 patch` | `CMYK 标准 patches（固定 8 个，ISO 12647-7 要求）` |
| 专色追加上限 | 不限 | 最多 4 个 |
| 自定义追加上限 | 无 | 最多 8 个 |

G31 使用 72 固定 patch 的旧方案（UGRA Media Wedge），WORKFLOW 明确要求新的动态 3 段结构：8 固定 + ≤4 专色 + ≤8 自定义。

**修正**：
```cpp
struct DynamicStripConfig {
    static constexpr int CMYK_FIXED   = 8;   // ISO 12647-7 要求，固定
    static constexpr int SPOT_MAX     = 4;   // Job 中出现的专色
    static constexpr int CUSTOM_MAX   = 8;   // 用户手动追加

    std::vector<SpotColorInfo> spot_colors;    // 从 PDF 提取，最多 4 个
    std::vector<SpotColorInfo> custom_colors;  // 用户追加，最多 8 个
};

std::vector<uint8_t> generate_dynamic_control_strip(
    const DynamicStripConfig& config,
    int strip_width_px,
    int dpi = 720);
```

---

### E08 — G47 主窗口尺寸 + 标签页名称错
**包**：G47（`apps/gcolour/ui/main_window.h`，主窗口三栏骨架）

**尺寸错误**：

| 面板 | G47 | WORKFLOW §七 |
|------|-----|------------|
| 左侧导航宽度 | `200px` | `220px` |
| 右侧属性面板宽度 | `280px` | `300px` |

（注：200/280 是 UI-SPEC-FIERY.md 中 jcolour 专色编辑器的规格，gcolour 主窗口尺寸由 WORKFLOW §七 定义）

**标签页名称错误**：

| 位置 | G47 | WORKFLOW §七 |
|------|-----|------------|
| 第 2 标签 | `Reports` | `History` |
| 第 6 标签 | `Settings` | `Diagnostics` |

正确六视图顺序：`Jobs / History / Workflows / Output / Database / Diagnostics`

**修正**：
```cpp
// 三栏固定布局（gcolour 尺寸，来自 WORKFLOW §七）：
//   左侧：PrintConditionTree（220px 固定宽）← 不是 200px
//   中央：QStackedWidget（六视图切换）
//   右侧：PropertiesPanel（300px 固定宽）← 不是 280px
//
// 六视图：Jobs / History / Workflows / Output / Database / Diagnostics
//          ← 不是 Reports；← 不是 Settings
```

---

### E09 — G56 RMSE 阈值混淆：错误删除 0.05 第二级门禁
**包**：G56（`apps/gcolour/ui/maintenance_panel.h`，维护面板）

G56 末行注释："阈值统一为 0.03（§7B CYNSN 验收门禁），不用 §14 第二级的 0.05"

PIPELINE §7B 明确三个阈值各有独立语义：

| RMSE 值 | 环节 | 含义 |
|---------|------|------|
| 0.02 | §15 验证通过门禁 | 轻修后 RMSE < 0.02 → 确认修复成功 |
| 0.03 | §7B 建档 + §14/§15 轻修触发 | ≥ 0.03 → 触发精确诊断/轻修 |
| **0.05** | **§14 第二级升级** | **轻修后仍 > 0.05 → 升级为重建线性化层** |

G56 将 0.05 从维护触发表中删除，导致维护面板缺少第二级升级路径。

**修正**：维护触发表需增加 0.05 阈值行：
```
Maintenance 触发表：
  ΔE < 1.5（AutoCal PASS）                → 无需操作
  1.5 ≤ ΔE < 3.0                          → Quick Calibration（§14，~10 min）
  §15 精确诊断 CYNSN RMSE > 0.03         → 通道轻修（§15，~30 min）
  §15 轻修后 validation_rmse > 0.05      → ← 新增。升级为重建线性化层（§3–§5 重做，~90 min）
  ΔE ≥ 3.0（ON_HOLD）                    → §14 三级处置，最终可能完整重建（~3 hr）
```

---

## 二、数据模型问题（Important — 编码前修复）

### D01 — G03 calibration_baseline.json 结构不完整
**包**：G03（`engine/calibration/baseline.h`）

PIPELINE §8 明确定义了 JSON schema，G03 的 `CalibrationBaseline` 结构缺少多个必须字段：

| 字段 | PIPELINE §8 JSON | G03 结构体 |
|------|----------------|----------|
| `schema_version` | `"1.0"` | ❌ 缺 |
| `created_at` | ISO 8601 UTC | `built_at int64`（格式不符）|
| `printer_id` | `"SC-P9500-SN12345"` | ❌ 缺 |
| `media_name` | `"Canson_Platine..."` | ❌ 缺 |
| `measurement_mode` | `"M1"` | ✅ 有 |
| `instrument` | `"i1Pro3"` | ❌ 缺 |
| `illuminant` | `"D50"` | ❌ 缺 |
| `observer` | `"2deg"` | ❌ 缺 |
| patch.`nominal_cmyk` | `[0,0,0,0]` | ❌ 缺 |
| patch.`spectrum_nm` | 36 通道 | ✅ 有（字段名需改为 `spectrum_nm`）|

同时，`active_json_checksum` 字段位置：G03 定义在 struct 顶层，PIPELINE 将其嵌入到绑定流程（G24 步骤 5-6），语义一致但字段名需确认（建议保留在 JSON 中作为 `active_config_checksum` 字段）。

**修正**：
```cpp
struct CalibrationBaseline {
    std::string schema_version = "1.0";
    std::string created_at;           // ISO 8601 UTC，如 "2026-05-07T09:30:00Z"
    std::string printer_id;           // 来自 PrintCondition
    std::string media_name;           // 来自 media_config.paper_id
    std::string measurement_mode;     // "M0" 或 "M1"
    std::string instrument = "i1Pro3";
    std::string illuminant = "D50";
    std::string observer   = "2deg";
    std::string active_config_checksum;  // SHA-256(active.json 文件内容)
    std::vector<BaselinePatch> patches;
};

struct BaselinePatch {
    std::string name;                      // e.g. "Media_Wedge_V3_001"
    std::array<float, 4>  nominal_cmyk;    // ← 新增。标称 CMYK，用于显示
    std::array<float, 3>  lab;             // CIE Lab D50/2°
    std::array<float, 36> spectrum_nm;     // ← 改名（原为 spectrum）。380–730nm @ 10nm
};
```

---

## 三、遗漏包（Missing — 需新增工作包）

### M01 — 缺少 Gamut Boundary Descriptor (GBD) 预计算包
**遗漏功能**：PIPELINE §7B "Gamut Boundary Descriptor（色域边界预计算）"

`gamut_boundary.gbd` 文件（~200KB 凸包，O(log N) 超色域查找）是 §7B 的三个输出之一，但没有对应工作包。

**建议新增** G59：
- 文件：`engine/src/gamut_boundary.h/.cpp`
- 内容：遍历 cynsn_lut.bin 收敛节点 → 3D 凸包 → `.gbd`；加载后 `is_in_gamut(Lab)` O(log N) 查找

---

### M02 — 缺少 CxF/X-4 IO 包
**遗漏功能**：PIPELINE §6.5 "清洗后 CxF（送 §7B）"

`cxf_io.h` 负责 §6 双轨持久化（光谱 → CxF/X-4 格式 → §7B；同时 → .ti3 → §7A），没有对应工作包。G14/G15 生成靶图，但测量数据从仪器读回后的 CxF 写入没有被覆盖。

**建议新增** G60：
- 文件：`engine/calibration/cxf_io.h/.cpp`
- 内容：读写 ISO 17972-4 CxF/X-4 XML；含完整测量条件元数据（仪器/模式/观测者）；往返序列化测试

---

### M03 — 缺少 KM 双层模型包
**遗漏功能**：PIPELINE §7B "高墨量区切换（pixel_total_ink > 250%）"

```
if pixel_total_ink_percent > 250:
    switch to Kubelka-Munk 双层模型
    R_predicted = KM_two_layer(substrate, ink_layers[])
```

现有 `engine/km.h` 已有骨架，但没有对应工作包验证其与 CYNSN 的切换逻辑（G26 Route B 转换包未覆盖此分支）。

**建议新增** G61：
- 文件：`engine/km.h/.cpp`（扩展）
- 内容：`km_two_layer()` 函数；切换逻辑集成到 G26；测试：50% CMYK（单层）vs 300% CMY 实地（双层）

---

### M04 — 缺少 SpotColorLibrary + 名称标准化包
**遗漏功能**：PIPELINE §8 "品牌色绑定（SpotColorLibrary）"+ 6 步规范化管线 + Levenshtein 模糊匹配

G24 的绑定流程注释提到"品牌色绑定"，但没有独立工作包实现：
- `SpotColorEntry` 数据模型
- 6 步名称标准化（trim / 折叠空格 / toLower / `pms→pantone` / 数字边界 / 后缀大写）
- Levenshtein 距离计算（≤1 自动接受，=2 UI 确认，>2 无匹配）

**建议新增** G62：
- 文件：`engine/calibration/spot_color_library.h/.cpp`
- 内容：`SpotColorEntry` 结构体；`normalize_spot_name()`；`match_spot_color()` 返回 `MATCH/FUZZY_MATCH/FUZZY_CANDIDATE/NO_MATCH`

---

### M05 — 缺少 History 标签页 UI 包
**遗漏功能**：WORKFLOW §七 六视图中的 `History` 标签页

G47 将第 2 标签命名为 `Reports`（错），且没有任何包实现 History 视图的功能。

**建议新增** G63：
- 文件：`apps/gcolour/ui/history_view.h/.cpp`
- 内容：校准历史表格（时间 / 触发方式 / GREEN/AMBER/RED / ΔE mean+max）；ISO 报告列表（按 PrintCondition 分组）；PDF 导出按钮

---

### M06 — 缺少 §6 自适应补测（批量 WARN 时）
**遗漏功能**：PIPELINE §6.5 批量门禁 WARN 时的处置路径

当批量门禁触发 WARN（3%–8% 异常 patch），需要提示用户"建议重扫"并给出异常 patch 列表。补测后需要合并新旧光谱数据。这个**数据合并**逻辑没有工作包覆盖。

**建议新增** G64（可选，v1.0 优先级 P2）：
- 文件：`engine/calibration/spectral_cleaning.h`（扩展 G17）
- 内容：`merge_rescan_patches()` — 用新测量替换指定 patch 索引，重新触发 G17 批量门禁验证

---

## 四、参考一致性问题（Minor）

### R01 — G40 ICCRequest 命名一致性
G40 的 `ICCRequest` 字段名 `DeviceChannels` 对应 PIPELINE `-d3` 参数，但 `-d3` 在 Argyll 中是设备维度（device dimension），不是 channels。建议改为 `DeviceDimensions` 或注释说明 `-d3 = 设备色彩空间维度（12ch 打印机固定为 3 = 三维 Lab PCS）`。实际不影响功能，但命名容易误导。

### R02 — G16 G17 分离设计合理但文档不明
G16（单 patch 清洗）和 G17（批量门禁）是同一 `spectral_cleaning.h` 的两个层次，设计合理。建议在 G16 注释中明确："G16 是单 patch 级别；G17 是批量级别，依赖 G16 结果聚合"。

### R03 — G57 属性面板宽度未与 E08 对齐
G57 写"右侧 280px 面板"，与 E08 修正后（300px）不符。需同步修正。

---

## 五、修改优先级汇总

| 优先级 | 包 | 类型 | 修改内容 |
|--------|---|------|---------|
| P0（阻断） | G20 | E01 | `"CYSN"` → `"CYNS"` |
| P0（阻断） | G22 | E02 | `magic="CLUT"→"CYNS"`；`version uint16→uint32_le` |
| P0（阻断） | G16 | E03 | S-G degree=2→3；k-NN 双路线参数 |
| P0（阻断） | G02, G24 | E06 | active.json 增加 `checksum_linearization` |
| P1（重要） | G09 | E04 | GCRParams 增加 `gcr_amount=0.70` |
| P1（重要） | G35 | E05 | 边界值 `<` → `≤`，测试用例修正 |
| P1（重要） | G31 | E07 | 72固定patch → 8+≤4+≤8 动态结构 |
| P1（重要） | G47, G57 | E08 | 尺寸 200/280 → 220/300；标签页名修正 |
| P1（重要） | G56 | E09 | 增加 0.05 阈值升级路径 |
| P1（重要） | G03 | D01 | baseline 结构体增加缺失字段 |
| P2（新包） | — | M01 | 新增 G59：GBD 预计算 |
| P2（新包） | — | M02 | 新增 G60：CxF/X-4 IO |
| P2（新包） | — | M03 | 新增 G61：KM 双层模型集成 |
| P2（新包） | — | M04 | 新增 G62：SpotColorLibrary + 名称匹配 |
| P2（新包） | — | M05 | 新增 G63：History 标签页 UI |
| P3（可选） | — | M06 | 新增 G64：§6 补测合并（v1.0 P2）|

---

*审核完成。下一步：按优先级顺序更新 `gcolour-packages.md`。*
