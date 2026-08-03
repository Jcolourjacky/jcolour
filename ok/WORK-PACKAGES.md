# jcolour / gcolour 工作包分解

> 每包约 30 分钟。按依赖顺序排列，同组内无依赖可并行。  
> 规格来源：`docs/PIPELINE-gcolour.md`（§1–§15）、`docs/WORKFLOW-gcolour.md`、`docs/UI-SPEC-FIERY.md`。

---

## 第一组：引擎基础层（无依赖）

### P01 — PrintCondition 状态机
**文件**：`engine/calibration/state_machine.h/.cpp`（替换现有 4-state 骨架）

现有骨架只有 `UNCALIBRATED/FRESH/STALE_SOFT/STALE_HARD` 4 个状态，与规格不符。

**实现**：
```cpp
enum class PrintConditionState {
    UNCONFIGURED,   // 刚创建，未执行任何步骤
    HARDWARE_OK,    // 喷头检查通过（Step 1 完成后即进入）
    CALIBRATED,     // ink_limit + g7_curve + linearization 就绪
    PROFILED,       // printer.icc + cynsn_model.bin 就绪
    READY,          // active.json 绑定完成，可出样
    NEEDS_RECAL,    // AutoCal 触发（1.5 ≤ ΔE < 3.0）
    DEGRADED,       // §15 轻修失败 或 profile checksum 不符
    BATCH_CHANGED,  // ink_batch_id 变更
    ON_HOLD,        // AutoCal RED（ΔE ≥ 3.0）或操作员手动
};

struct PrintCondition {
    PrintConditionState state = PrintConditionState::UNCONFIGURED;
    std::string id;           // UUID
    std::string paper_id;
    std::string ink_batch_id;
    int64_t     last_cal_at = 0;
};

// 合法转换表（非表中组合抛 std::logic_error）：
// UNCONFIGURED → HARDWARE_OK（Step 1 喷头检查通过）
// HARDWARE_OK  → CALIBRATED（Step 3-5 完成）
// CALIBRATED   → PROFILED（Step 7A/7B 完成）
// PROFILED     → READY（Step 8 active.json 写入）
// READY        → NEEDS_RECAL（AutoCal AMBER 触发）
// READY        → ON_HOLD（AutoCal RED 触发 / 操作员）
// READY        → BATCH_CHANGED（ink_batch_id 变化检测到）
// NEEDS_RECAL  → READY（§14 轻校成功）
// NEEDS_RECAL  → DEGRADED（§15 轻修失败）
// ON_HOLD      → NEEDS_RECAL（AutoCal/ISO 失败后 release）
// ON_HOLD      → READY（操作员手动 release）
// BATCH_CHANGED→ HARDWARE_OK（重建）
// BATCH_CHANGED→ NEEDS_RECAL（风险接受）
// 任意状态    → DEGRADED（profile checksum 失败）
PrintCondition transition(PrintCondition pc, PrintConditionState to);
```

**测试**：覆盖全部合法转换 + 3 个非法转换（期望抛异常）。

---

### P02 — active.json 读写 + checksum
**文件**：`engine/calibration/active_config.h/.cpp`（新建）

**实现**：
```cpp
struct ActiveConfig {
    std::string print_condition_id;
    std::string paper_id;
    std::string ink_batch_id;
    std::string checksum_profile;   // SHA-256 of printer.icc + cynsn_model.bin
    std::string measurement_mode;   // "M0" 或 "M1"
    int64_t     built_at = 0;
};

// 写入 active.json（JSON 序列化）
void write_active_config(const ActiveConfig& cfg, const std::string& path);

// 读取 active.json
ActiveConfig read_active_config(const std::string& path);

// 计算当前 profile 文件的 SHA-256 联合 checksum
// sha256( file_bytes(icc_path) || file_bytes(cynsn_model_path) )
std::string compute_profile_checksum(const std::string& icc_path,
                                     const std::string& cynsn_model_path);

// 验证 checksum，不匹配时返回 false（触发 DEGRADED 状态）
bool verify_profile_checksum(const ActiveConfig& cfg,
                             const std::string& icc_path,
                             const std::string& cynsn_model_path);
```

**测试**：往返序列化、checksum 匹配、checksum 不匹配返回 false。

---

### P03 — calibration_baseline.json 读写
**文件**：`engine/calibration/baseline.h/.cpp`（新建）

**实现**：
```cpp
struct BaselinePatch {
    std::string name;
    std::array<float, 36> spectrum;  // 380–730nm @ 10nm
    std::array<float, 3>  lab;       // D50 2°
};

struct CalibrationBaseline {
    std::string active_json_checksum;  // SHA-256 of active.json（写入时计算）
    std::string measurement_mode;      // "M0" 或 "M1"
    int64_t     built_at = 0;
    std::vector<BaselinePatch> patches; // Quick Check Strip 的参考光谱
};

void write_baseline(const CalibrationBaseline& b, const std::string& path);
CalibrationBaseline read_baseline(const std::string& path);
```

**注意**：`active_json_checksum` 在写入时由调用方传入（§8 建档流程结束时计算 SHA-256(active.json 文件内容)）。

**测试**：往返序列化，patches 数组至少 11 个元素。

---

### P04 — calibration_history.json 读写
**文件**：`engine/calibration/history.h/.cpp`（新建）

**实现**：
```cpp
struct HistoryPatchEntry {
    std::string name;
    std::array<float, 3> lab_measured;
    std::array<float, 3> lab_reference;
    float delta_e = 0.0f;
};

struct CalibrationHistoryEntry {
    std::string timestamp;            // ISO 8601
    std::string trigger;              // "autocal_check" / "manual" / "expired"
    std::string result;               // "GREEN" / "AMBER" / "RED"
    float delta_e_mean = 0.0f;
    float delta_e_max  = 0.0f;
    int   patch_count  = 0;
    std::vector<HistoryPatchEntry> patches;
    std::string checksum_linearization;  // SHA-256(g7_curve.lut + linearization.lut)
};

// 追加一条记录；自动删除超过 50 条或超过 12 个月的旧记录（FIFO）
void append_history(const CalibrationHistoryEntry& entry, const std::string& path);

// 读取所有历史记录（最新在最后）
std::vector<CalibrationHistoryEntry> read_history(const std::string& path);

// 获取最后一条记录；若文件不存在或为空，返回 std::nullopt
std::optional<CalibrationHistoryEntry> get_last_entry(const std::string& path);
```

**测试**：追加 55 条后确认只保留最新 50 条；超 12 个月的条目被删除。

---

### P05 — linearization.lut 读写
**文件**：`engine/calibration/linearization_lut.h/.cpp`（新建）

**格式**：二进制文件，float32 × 256 采样点 × 12 通道，小端序，无文件头（共 256×12×4 = 12,288 字节）。通道顺序：C, M, Y, K, Lc, Lm, Lk, Llk, Or, Gr, V, W（索引 0–11）。

**实现**：
```cpp
constexpr int LIN_LUT_POINTS   = 256;
constexpr int LIN_LUT_CHANNELS = 12;

struct LinearizationLUT {
    // curve[ch][i]：输入值 i/255.0 对应的输出墨量（0.0–1.0）
    std::array<std::array<float, LIN_LUT_POINTS>, LIN_LUT_CHANNELS> curve;
};

void write_linearization_lut(const LinearizationLUT& lut, const std::string& path);
LinearizationLUT read_linearization_lut(const std::string& path);

// 对单个值应用线性化（线性插值）
float apply_lut_channel(const LinearizationLUT& lut, int channel, float input);
```

**测试**：往返读写，对 channel=3（K）做已知输入值验证插值输出。

---

## 第二组：§2 介质注册

### P06 — OBA 检测算法
**文件**：`engine/calibration/oba_detection.h/.cpp`（新建）

**实现**：
```cpp
enum class OBAStatus { NONE, WEAK, STRONG };

struct OBAResult {
    OBAStatus status;
    float     delta_e_m0_m1;  // ΔE00(M0 vs M1)
    std::string measurement_mode_locked;  // "M0" 或 "M1"
    std::string warning_message;
};

// 规格：
//   delta_E_m0_m1 < 1.0  → NONE（M0/M1 均可）
//   1.0 ≤ delta_E < 3.0  → WEAK（推荐 M1，黄色警告）
//   delta_E ≥ 3.0        → STRONG（强制锁定 M1，橙色警告）
// 测量顺序：先测 M1，再测 M0（与 WORKFLOW Step 2 一致）
OBAResult detect_oba(const spectral::Spectrum& substrate_M1,
                     const spectral::Spectrum& substrate_M0);
```

**依赖**：`spectral.h`（已有）、`delta_e.h`（已有）

**测试**：
- 相同光谱输入 → NONE
- M0/M1 差异 ΔE = 1.5 → WEAK
- M0/M1 差异 ΔE = 4.0 → STRONG，measurement_mode_locked == "M1"

---

### P07 — media_config.json 读写
**文件**：`engine/calibration/media_config.h/.cpp`（新建）

**实现**（与 PIPELINE §2 JSON 结构对齐）：
```cpp
struct MediaConfig {
    std::string paper_id;
    std::string paper_name;
    std::string substrate_type;        // "coated" / "matte" / "canvas" / "transparent" / "dark"
    std::string ink_mode;              // "PhotoBlack" / "MattBlack"
    int         dpi = 720;
    std::string measurement_mode;      // "M0" 或 "M1"（§2 OBA 检测后锁定）
    std::array<float, 36> substrate_spectrum_M1;  // 380–730nm
    std::array<float, 36> substrate_spectrum_M0;  // M0 补充
    std::string oba_status;            // "NONE" / "WEAK" / "STRONG"
};

void write_media_config(const MediaConfig& cfg, const std::string& path);
MediaConfig read_media_config(const std::string& path);
```

**测试**：往返序列化，验证 substrate_spectrum_M1[0] 精度（float32 往返误差 < 1e-5）。

---

## 第三组：§3 墨量限制

### P08 — 单通道饱和拐点检测
**文件**：`engine/calibration/ink_limit.h/.cpp`（新建）

**实现**（规格 §3.1 + A.12）：
```cpp
// 输入：各墨量级别（0–1）的密度测量值
// 返回：饱和拐点（0–1），即一阶导数降至峰值 5% 以下的点
float find_saturation_point(const std::vector<float>& ink_levels,    // 0–1，均匀间隔
                             const std::vector<float>& densities);    // 对应密度值

// 完整单通道墨量限制计算
// single_channel_max = find_saturation_point(...) × 0.95
float compute_single_channel_max(const std::vector<float>& ink_levels,
                                  const std::vector<float>& densities);
```

**关键约束**：
- 使用相对阈值 `threshold = 0.05 × max(relative_gain)`，其中 `relative_gain[i] = (density[i+1] - density[i]) / (ink_level[i+1] - ink_level[i])`
- 不使用绝对阈值 0.05（对低密度通道如 Lc 永远不触发）

**测试**：
- 线性密度曲线 → 最大值附近触发
- 在 70% 处有明显饱和弯折的曲线 → 触发点在 70% 附近
- Lc 低密度通道（peak gain ≈ 0.5）→ 相对阈值正确触发

---

### P09 — ink_limit.json 读写
**文件**：`engine/calibration/ink_limit.h/.cpp`（续 P08）

**实现**（PIPELINE §3 JSON 结构）：
```cpp
struct GCRParams {
    float black_start = 0.20f;  // bs（0–1）
    float black_max   = 0.95f;  // bm（0–1）
    float ucr         = 0.30f;  // ucr（0–1）
};

struct ChannelLimits {
    std::map<std::string, float> single_channel_max;   // e.g. {"C": 0.92, "K": 0.88, ...}
    float                        TAC_total = 3.2f;     // 倍率（3.2 = 320%）
    GCRParams                    gcr;
};

void write_ink_limit(const ChannelLimits& lim, const std::string& path);
ChannelLimits read_ink_limit(const std::string& path);
```

**关键约束**：
- `TAC_total` 存储为倍率（0–N），传 Argyll `-l` 时乘以 100
- `gcr` 字段存 0–1 分数，直接传 `-K bs,1,1,ucr,bm,0`，无需额外转换

**测试**：往返序列化，验证 TAC_total = 3.2 正确存储（不存为 320）。

---

### P10 — Multi-channel TAC 测量
**文件**：`engine/calibration/ink_limit.h/.cpp`（续 P09）

**实现**：
```cpp
struct TACMeasurement {
    std::string channel_combo;   // e.g. "C+M+Y"
    float       tac_value;       // 倍率
    float       rsd_percent;     // RSD（%），< 2% 为稳定
};

// 从多次重复测量计算 RSD（相对标准差）
float compute_rsd(const std::vector<float>& measurements);

// 从多组叠印测量中选出最小 TAC（RSD < 2% 的组合）
// 再应用物理安全上限：min(selected_TAC, 3.80) × 0.90
float select_tac_total(const std::vector<TACMeasurement>& measurements);
```

**关键约束**：
- 物理安全上限 3.80（= 380%），单位为倍率
- 若任意组合超过 3.80 → 生成 WARN（不 BLOCK）
- 最终 TAC_total = min(selected_TAC, 3.80) × 0.90

**测试**：
- 全部组合 RSD < 2% → 选最小值 × 0.90
- 某组合 TAC = 4.0 → 上限截断为 3.80，输出 3.80 × 0.90 = 3.42，且产生 WARN

---

## 第四组：§4 G7+ NPDC

### P11 — G7 NPDC 目标查找表
**文件**：`engine/calibration/g7_npdc_table.h/.cpp`（新建）

**实现**：将 IDEAlliance P2P51 L* 目标值编码为编译期常量，不可用线性公式代替。

```cpp
// P2P51 靶图共 ~250 个阶调点
struct NPDCTarget {
    float ink_percent;   // 0–100
    float L_star_cmyk;   // CMY 复合灰目标 L*
    float L_star_k;      // K 单独灰目标 L*
};

// 返回 P2P51 标准 NPDC 目标表（IDEAlliance 定义，非线性，在高光区有凹形特征）
const std::vector<NPDCTarget>& get_npdc_table();

// 对给定阶调值插值获取目标 L*
float npdc_target_L(float ink_percent, bool k_only = false);
```

**关键约束**：
- 必须使用 IDEAlliance 官方 P2P51 数据表，不可用 `L* = 100 - t*(100-L_solid)/100` 线性公式
- 数据表中高光区（t < 25%）目标 L* 高于线性，是 G7 认证的关键
- 数据来源：从 IDEAlliance 官网下载 P2P51 .txt 文件，解析后硬编码

**测试**：
- 0% 阶调 → L* = 100.0（纸白）
- 100% 阶调 → L* ≈ L*_solid（实测密度）
- 50% 阶调 → L* 与线性公式差异 > 0.5（确认非线性）

---

### P12 — G7 校正曲线计算与 g7_curve.lut 输出
**文件**：`engine/src/g7_validation.h/.cpp`（扩展现有文件，SPIKES.md Spike 1 子任务）

现有 `g7_validation.h` 有 `G7ComplianceChecker`（验证部分），需要扩展输出校正曲线。

**实现**：
```cpp
// 新增：从 P2P51 实测数据计算校正曲线
struct G7CorrectionCurve {
    // 256 个控制点：输入灰度 % → 校正后灰度 %
    std::array<float, 256> cmyk_curve;  // CMY 复合灰
    std::array<float, 256> k_curve;     // K 单独灰
};

// 输入：P2P51 实测数据
// 输出：三次样条拟合后的校正 LUT
// 算法：error[i] = L*_measured[i] - npdc_target_L(t_i)
//       correction[i] = t_i + α*error_L[i] + β*error_ab[i]
//       → 三次样条拟合 → 256 点 LUT
G7CorrectionCurve compute_g7_correction(
    const std::vector<float>& ink_percents,
    const std::vector<float>& L_measured,
    const std::vector<float>& a_measured,
    const std::vector<float>& b_measured);

// 写入 g7_curve.lut（二进制，float32 × 256 × 2，CMY 曲线 + K 曲线）
void write_g7_curve_lut(const G7CorrectionCurve& curve, const std::string& path);
G7CorrectionCurve read_g7_curve_lut(const std::string& path);
```

**测试**：
- 无偏差输入（实测 = 目标）→ 校正曲线是恒等变换（输出 = 输入）
- 在 50% 处偏暗 3 ΔE → 50% 附近校正值 > 50%

---

## 第五组：§5 联合线性化

### P13 — 分组线性化曲线拟合
**文件**：`engine/calibration/joint_linearization.h/.cpp`（新建）

**实现**：对 PIPELINE §5 定义的 8 个通道组（G1=Lc+C, G2=Lm+M, G3=Lk+Llk+K, G4=Or, G5=Gr, G6=V, G7=W, G8=W 白墨）分别做线性化。

```cpp
struct ChannelGroup {
    std::string name;                    // "G1", "G2", ...
    std::vector<std::string> channels;   // e.g. {"Lc", "C"}
};

// 拟合单组的联合线性化曲线（单调三次样条）
// 输入：该组的阶调值（0–1）和测量密度
// 输出：256 点 LUT，保证单调性
std::array<float, 256> fit_group_linearization(
    const std::vector<float>& ink_levels,
    const std::vector<float>& densities);

// 对 256 点 LUT 执行单调性强制（若有非单调点则插值修正）
void enforce_monotone(std::array<float, 256>& lut);
```

**依赖**：P05（linearization_lut 读写）

**测试**：
- 纯线性输入 → 输出近似恒等变换
- 有弯折的输入 → 输出单调且平滑

---

## 第六组：§6 靶图生成

### P14 — Route A 靶图生成（TC4/ECI2002）
**文件**：`engine/calibration/chart_generator.h/.cpp`（新建）

**实现**：
```cpp
// 生成 TC4（1617 patches）或 ECI2002（1485 patches）布局
// 输出：.ti1 格式（Argyll targen 输入格式）
struct ChartSpec {
    enum class Type { TC4, ECI2002 } type = Type::TC4;
    int n_channels = 4;
};

struct PatchSet {
    std::vector<std::string> names;
    std::vector<std::vector<float>> values;  // 每个 patch 的 n_channels 个值（0–1）
};

PatchSet generate_route_a_chart(const ChartSpec& spec);
void write_ti1(const PatchSet& ps, const std::string& path);
```

**测试**：TC4 输出恰好 1617 个 patches，通道值在 [0, 1] 范围内。

---

### P15 — Route B 靶图 LHS 采样（9–12 通道）
**文件**：`engine/calibration/chart_generator.h/.cpp`（续 P14）

**实现**：
```cpp
// Latin Hypercube Sampling，n_channels = 9–12
// 覆盖率 ~30% of 2^N，patches ~500–600
PatchSet generate_route_b_lhs_chart(int n_channels, int n_patches = 550,
                                     uint64_t seed = 42);

// 均匀网格采样，n_channels ≤ 8
// 内容：64 primaries（2^6 核心 Neugebauer）+ 各通道 ramp（16 级）
// patches ~400–600
PatchSet generate_route_b_grid_chart(int n_channels);
```

**关键约束**：
- LHS：每个维度的边缘分布均匀（标准 Latin Hypercube）
- 均匀网格：不是 2^8 = 256 全格网格，是 64 核心 primaries + 补测

**测试**：
- 12 通道 LHS → 生成 550 个 patches，每维度边缘分布方差 < 0.01
- 8 通道均匀网格 → 包含 64 primaries + 8×16 = 128 ramp patches

---

### P16 — §6.5 Savitzky-Golay 三窗口光谱平滑
**文件**：`engine/calibration/spectral_cleaning.h/.cpp`（新建）

**实现**：
```cpp
// 三窗口 S-G 平滑（规格 §6.5）：
//   标准区间 440–680nm：窗口 = 5，多项式阶 = 2
//   OBA 区间 380–440nm：窗口 = 3，多项式阶 = 2（保留荧光特征）
//   末端区间 680–730nm：窗口 = 3，多项式阶 = 2（低 SNR）
// 输入：36 通道光谱（380–730nm @ 10nm），float32
spectral::Spectrum sg_smooth(const spectral::Spectrum& raw);

// 批量清洗（含异常检测）
struct CleaningResult {
    spectral::Spectrum smoothed;
    bool is_outlier;      // RMSE vs 邻居 > threshold
    float outlier_score;  // 异常分数
};

CleaningResult clean_spectrum(const spectral::Spectrum& raw,
                               const std::vector<spectral::Spectrum>& neighbors,
                               float rmse_threshold = 0.030f);
```

**测试**：
- 平坦光谱 + 单点噪声 → 异常点被平滑
- OBA 区（380–440nm）的尖锐荧光峰被保留（不被过度平滑）
- 验证 S-G 系数：5点二阶与标准表一致

---

### P17 — §6.5 批量门禁（相对阈值）
**文件**：`engine/calibration/spectral_cleaning.h/.cpp`（续 P16）

**实现**：
```cpp
struct BatchQAResult {
    int total_patches;
    int outlier_count;
    float outlier_ratio;  // outlier_count / total_patches
    bool block;           // outlier_ratio > 0.08
    bool warn;            // 0.03 < outlier_ratio ≤ 0.08
    std::vector<int> outlier_indices;
};

// 规格 §6.5：
//   outlier_ratio > 8%  → BLOCK（全批报废，重新扫描）
//   3% < ratio ≤ 8%     → WARN（标注异常 patch，继续建档）
//   ratio ≤ 3%          → PASS
BatchQAResult check_batch_qa(const std::vector<CleaningResult>& results);
```

**测试**：
- 50 patches 中 6 个异常（12%）→ BLOCK
- 50 patches 中 2 个异常（4%）→ WARN
- 50 patches 中 1 个异常（2%）→ PASS

---

## 第七组：§7A Route A ICC 生成

### P18 — colprof 完整参数调用
**文件**：`engine/argyll/argyll.h/.cpp`（扩展现有）

现有 `argyll.h` 已有基本调用，需扩展为完整参数集。

**实现**：
```cpp
struct ColprofParams {
    std::string ti3_path;
    std::string output_profile_name;
    char        quality = 'h';           // '-qh'（高质量，33³ LUT）
    int         device_channels = 12;    // '-d3'（12 通道）
    char        k_type = 'r';            // '-k r'（GCR 替换）
    // -K <bs>,1,1,<ucr>,<bm>,0（从 ink_limit.json.gcr 读取，0–1 分数，直接传入）
    float       k_black_start = 0.20f;   // bs
    float       k_black_max   = 0.95f;   // bm
    float       k_ucr         = 0.30f;   // ucr
    int         tac_percent   = 320;     // '-l'，TAC_total × 100（整数）
    // '-c mt'（matrix + tetrahedral 插值）
};

// 调用 colprof 子进程，返回生成的 ICC 文件路径
// 抛出 std::runtime_error 若 colprof 返回非零退出码
std::string run_colprof(const ColprofParams& params, const std::string& work_dir);
```

**关键约束**：
- `tac_percent = (int)(ink_limit.TAC_total × 100)`，例如 3.2 → `-l 320`
- `k_black_start/max/ucr` 直接取 `ink_limit.gcr` 字段（已是 0–1 分数），不除以 100
- 使用 `-qh -d3`（高质量，33³ LUT）

**测试**：用 test_argyll.cpp 中现有 A19 fixture 验证完整参数调用成功，ICC > 100KB。

---

### P19 — ICC 评估集验证（100-patch 评估）
**文件**：`engine/calibration/profile_validator.h/.cpp`（新建）

**实现**：
```cpp
struct ProfileValidationResult {
    float avg_delta_e = 0.0f;
    float max_delta_e = 0.0f;
    int   evaluated_patches = 0;
    bool  pass = false;  // avg < 2.0 AND max < 5.0（§7A 规格）
};

// 从 .ti3 数据中分层随机抽取 100 patches（L* < 30/30-70/>70 各约 33 个）
// 注：这 100 patches 仍参与 colprof 全量拟合（不删除），用于事后评估拟合质量
std::vector<int> sample_evaluation_patches(const TI3Data& ti3, int n = 100,
                                            uint64_t seed = 42);

// 用 LCMS2 对 ICC profile 做前向预测，计算与实测的 ΔE00
ProfileValidationResult validate_profile(const std::string& icc_path,
                                          const TI3Data& ti3,
                                          const std::vector<int>& patch_indices);
```

**测试**：已知 ΔE00 < 2.0 的测试数据 → pass = true；ΔE00 = 3.0 的数据 → pass = false。

---

## 第八组：§7B CYNSN 模型与 LUT

### P20 — cynsn_model.bin 读写
**文件**：`engine/src/cynsn.h/.cpp`（扩展现有，新增持久化）

**实现**（规格 §7B cynsn_model.bin 格式）：
```
magic[4]:       "CYSN"
version[2]:     uint16_t = 1
n_inks[2]:      uint16_t（12 for gcolour）
n_wavelengths[2]: uint16_t = 36
divisions[2]:   uint16_t（Cellular 分格数，默认 2）
reserved_hash[32]: 32 bytes，全零（reserved，不用于 validity）
coefficients[...]: float32 × (divisions+1)^n_inks × n_wavelengths
area_curves[n_inks × 256]: float32（各通道 Murray-Davies area curve，256 点 LUT）
```

```cpp
// 序列化现有 CellularYNSNModel 到 .bin 文件
void write_cynsn_model(const CellularYNSNModel& model,
                        const std::array<std::array<float,256>, 12>& area_curves,
                        const std::string& path);

// 从 .bin 文件反序列化
struct CYSNModelData {
    CellularYNSNModel model;
    std::array<std::array<float,256>, 12> area_curves;
};
CYSNModelData read_cynsn_model(const std::string& path);
```

**测试**：往返序列化，对同一输入 `predict()` 输出一致（误差 < 1e-6）。

---

### P21 — Murray-Davies area curve 拟合（§7B 初始路径）
**文件**：`engine/calibration/area_curve.h/.cpp`（新建）

**实现**（规格 §7B，16-segment ramp，15 个拟合对）：
```cpp
// 输入：16 个墨量级别（0%, 6.25%, ..., 100%）的测量反射率 + 基材反射率
// 输出：256 点 area curve（dot area fraction，单调递增，[0,1]）
// 算法：Murray-Davies：a = (R_paper - R_measured) / (R_paper - R_ink_solid)
//       然后用单调样条拟合 16 个数据点 → 256 点 LUT
std::array<float, 256> fit_area_curve_initial(
    const std::vector<float>& ink_levels,     // 16 个值
    const std::vector<float>& reflectances,   // 对应测量反射率（单波长或光谱均值）
    float R_paper,                             // 基材反射率（0% 墨量时）
    float R_solid);                            // 实地反射率（100% 墨量时）
```

**注意**：此函数只用于 §7B 初始建档。§15 轻修使用 `fit_area_curve_refit()`（P51）。

**测试**：
- 0% 墨量 → area curve 输出 0.0
- 100% 墨量 → area curve 输出 1.0
- 曲线单调递增

---

### P22 — cynsn_lut.bin 读写
**文件**：`engine/src/cynsn.h/.cpp`（扩展）

**格式**（规格 §7B，cynsn_lut.bin）：
```
magic[4]:             "CLUT"
version[2]:           uint16_t = 1
grid_size[2]:         uint16_t = 33
n_channels[2]:        uint16_t = 12
n_wavelengths[2]:     uint16_t = 36（保留，当前 LUT 存 Device 值，非光谱）
linearization_hash[32]: SHA-256(linearization.lut + g7_curve.lut，建构时计算)
entries[35937 × 12]:  uint16_t，量化到 [0, 65535]（对应 0.0–1.0 Device 值）
// 精确大小：4+2+2+2+2+32 + 35937×12×2 = 46 + 862,488 = 862,534 bytes ≈ 843 KB
```

```cpp
struct CYSNLutHeader {
    uint16_t grid_size = 33;
    uint16_t n_channels = 12;
    std::array<uint8_t, 32> linearization_hash;
};

void write_cynsn_lut(const CYSNLutHeader& header,
                      const std::vector<std::array<uint16_t, 12>>& entries,
                      const std::string& path);

// entries[L_idx][a_idx][b_idx] → 12 通道设备值（0.0–1.0）
struct CYSNLut {
    CYSNLutHeader header;
    // 索引：node_idx = L_idx * 33*33 + a_idx * 33 + b_idx
    std::vector<std::array<float, 12>> device_values;  // 35937 个节点
};

CYSNLut read_cynsn_lut(const std::string& path);

// 计算 linearization_hash = SHA-256(linearization.lut bytes || g7_curve.lut bytes)
std::array<uint8_t, 32> compute_linearization_hash(const std::string& lin_path,
                                                      const std::string& g7_path);
```

**测试**：往返读写，验证文件大小 862,534 字节（允许 ±20 bytes for header padding）。

---

### P23 — 33³ LUT 预计算（GCD 并行）
**文件**：`engine/src/cynsn.h/.cpp`（扩展）

**实现**（规格 §7B LUT 构建并行化）：
```cpp
// Lab 网格坐标（D50 2°，CIELAB）：
//   L*: [0, 100]，33 点，步长 100/32 ≈ 3.125
//   a*: [-80, 80]，33 点，步长 160/32 = 5.0
//   b*: [-80, 80]，33 点，步长 160/32 = 5.0

// 构建完整 33³ LUT（约 3–5 分钟，GCD 并行）
// 每节点：SQP 50 iter，含 12 通道 central-difference 数值梯度（步长 6e-6）
// 超色域节点：标记 is_out_of_gamut = true，存入最近可达设备值
CYSNLut build_cynsn_lut(const CYSNModelData& model_data,
                          const ChannelLimits& ink_limits,
                          bool parallel = true);  // false = 单线程（测试用）
```

**关键约束**：
- 调用 `SQP_inverse_solve()`（现有 sqp.h）
- 使用 `DispatchQueue.concurrentPerform` 或 `std::for_each(std::execution::par_unseq, ...)`
- 节点完全独立，天然并行

**测试**（单线程，只测少量节点）：
- Lab=[50,0,0]（中性灰）→ 设备值接近中性（C≈M≈Y，K > 0）
- Lab=[0,0,0]（黑色）→ 设备值高墨量
- Lab=[100,0,0]（白色）→ 设备值全 0

---

## 第九组：§8 PrintCondition 绑定

### P24 — §8 完整绑定流程
**文件**：`engine/calibration/binding.h/.cpp`（新建）

**实现**（将 §8 的 7 个步骤封装为原子操作）：
```cpp
struct BindingInput {
    std::string icc_path;           // printer.icc
    std::string cynsn_model_path;   // cynsn_model.bin
    std::string cynsn_lut_path;     // cynsn_lut.bin
    std::string lin_lut_path;       // linearization.lut
    std::string g7_lut_path;        // g7_curve.lut
    std::string media_config_path;  // media_config.json
    std::string output_dir;         // PrintCondition 根目录
};

struct BindingResult {
    bool        success = false;
    std::string print_condition_id;  // UUID
    std::string active_json_path;
    std::string baseline_json_path;
    std::string error;
};

// 执行 §8 绑定：
//  1. 读取所有输入文件，验证存在
//  2. 计算 profile checksum（SHA-256 of icc + cynsn_model）
//  3. 计算 linearization_hash（SHA-256 of lin_lut + g7_lut）
//  4. 写入 active.json（含 checksum_profile）
//  5. 计算 SHA-256(active.json 文件内容) 写入 calibration_baseline.json 的 active_json_checksum
//  6. 写入 calibration_baseline.json（含 Quick Check Strip 参考 patch 光谱）
//  7. 写入 profile_metadata.json
BindingResult bind_print_condition(const BindingInput& input,
                                    const std::vector<BaselinePatch>& patches);
```

**测试**：提供全套 fixture 文件，验证 active.json / baseline.json 均写入，checksum 一致。

---

## 第十组：§8.5 AutoCal Check

### P25 — AutoCal Check 三路判断
**文件**：`engine/calibration/autocal_check.h/.cpp`（新建）

**实现**（规格 §8.5）：
```cpp
enum class AutoCalResult { GREEN, AMBER, RED };

struct AutoCalCheck {
    AutoCalResult result;
    float         avg_delta_e = 0.0f;
    std::string   base_source;  // "baseline" 或 "history"（来源说明）
    bool          is_expired;   // 距上次校准 > cal_valid_days
};

// 与基准比对（无需打印）：
//   < 1.5  → GREEN（严格小于，边界值 1.5 判 AMBER）
//   1.5–3.0 → AMBER（打印 Quick Check Strip）
//   ≥ 3.0  → RED（ON_HOLD）
// base_patches：若 calibration_history 为空，使用 calibration_baseline；
//               否则使用 history 最后一条记录
AutoCalCheck check_autocal(
    const std::vector<std::array<float,3>>& measured_lab,   // 当前控制条测量
    const std::vector<BaselinePatch>&       base_patches,   // 比对基准
    int64_t last_cal_timestamp,
    int     cal_valid_days = 30);
```

**关键约束**：GREEN 判断使用 `< 1.5`（严格小于），1.5 本身判 AMBER。

**测试**：
- ΔE = 1.4 → GREEN
- ΔE = 1.5 → AMBER（边界值）
- ΔE = 3.0 → RED
- 超过 30 天 → is_expired = true

---

## 第十一组：§9 PDF 预检

### P26 — PDF DeviceN 专色提取
**文件**：`engine/calibration/pdf_preflight.h/.cpp`（新建）

**实现**：
```cpp
struct SpotColorInfo {
    std::string name;           // e.g. "PANTONE 485 C"
    std::array<float,3> lab;    // CIE Lab 目标色
    bool is_W_channel = false;  // 白墨通道标志
};

struct PreflightResult {
    int                        n_channels;      // CMYK + spot 总通道数
    int                        max_spots_N;     // min(pdf_spot_count_excl_W, 8)
    std::vector<SpotColorInfo> spot_colors;
    bool                       has_W_channel;   // PDF 是否包含白墨专色
    std::string                routing_mode;    // "DeviceN" 或 "DeviceCMYK"
};

// 用 Ghostscript 提取 PDF 中的专色信息
// 注：W 通道在 §12 打印驱动层单独注入，不占 MaxSpots 配额
// MaxSpots N = min(pdf 中非 W 专色数量, 8)
PreflightResult preflight_pdf(const std::string& pdf_path,
                               const std::string& gs_path = "gs");
```

**关键约束**：W 通道（白墨）不计入 MaxSpots 的 8 个配额。

**测试**：给一个含 3 个专色 + 1 个白墨通道的测试 PDF → MaxSpots = 3, has_W_channel = true。

---

### P27 — 专色注册区域膨胀
**文件**：`engine/calibration/pdf_preflight.h/.cpp`（续 P26）

**实现**（规格 §9 dilation 公式）：
```cpp
// 膨胀量（像素） = registration_tolerance_mm × dpi / 25.4
// registration_tolerance_mm 默认 0.15mm（±1.5 像素 @ 254dpi）
int compute_dilation_pixels(float registration_tolerance_mm,
                             int dpi);
// 例：0.15mm × 720dpi / 25.4 = 4.25 → 取整 4 像素
```

**测试**：0.15mm × 720dpi / 25.4 = 4（取整），0.15mm × 1440dpi / 25.4 = 8。

---

## 第十二组：§11 Route A/B 渲染

### P28 — Route A 链路（g7_curve → linearization → DeviceLink）
**文件**：`engine/calibration/route_a.h/.cpp`（新建）

**实现**（规格 §11 Route A，LUT 应用顺序：g7_curve 先，linearization 后）：
```cpp
struct RouteAInput {
    std::array<float, 4> cmyk;   // 来自 PDF 渲染的 CMYK 值（0–1）
};

struct RouteAOutput {
    std::array<float, 12> device_values;   // 12 通道设备值（0–1）
};

// Route A 链路：
// 1. 应用 g7_curve.lut（灰平衡校正，1D LUT，先执行）
// 2. 应用 linearization.lut（通道线性化，后执行）
// 注：顺序不可颠倒，颠倒后灰平衡校正作用在已线性化通道上，输出颜色错误
RouteAOutput apply_route_a(const RouteAInput& input,
                            const G7CorrectionCurve& g7_curve,
                            const LinearizationLUT& lin_lut);
```

**测试**：
- 用接近中性的 CMYK 输入，确认 g7_curve 先于 lin_lut 执行（交换顺序后输出不同）
- 纯 K（0, 0, 0, 0.5）→ 输出 K 通道非零，CMY 接近零

---

### P29 — Route B 三线性插值 + 梯度下降精化
**文件**：`engine/src/cynsn.h/.cpp`（扩展）

**实现**（规格 §11 出样期，每像素 <1μs）：
```cpp
// 步骤 1：三线性插值查 33³ LUT（<0.1μs）
std::array<float, 12> lut_lookup_trilinear(const CYSNLut& lut,
                                             std::array<float, 3> lab);

// 步骤 2：局部梯度下降精化（1–3 iter，将残差从 ~0.5ΔE 降到 <0.1ΔE）
// 仅在 delta_e(lut_result, target_lab) > 0.1 时执行
std::array<float, 12> refine_with_gradient(
    const CYSNModelData& model_data,
    std::array<float, 3> target_lab,
    std::array<float, 12> initial_guess,
    const ChannelLimits& ink_limits,
    int max_iter = 3);
```

**测试**：
- LUT 查找对已知节点 Lab 坐标 → 误差 < 1e-4
- 梯度下降精化对插值结果 → ΔE 降低 ≥ 50%

---

### P30 — Route B 降级（B-FALLBACK 三种类型）
**文件**：`engine/calibration/route_b_fallback.h/.cpp`（新建）

**实现**（规格 §11 Route B fallback）：
```cpp
enum class FallbackType {
    A,   // Type-A：LUT STALE → 自动切 Route A，标注 SOFT-UNCAL
    B,   // Type-B：超色域 → 色域边界截断，标注 OUT-OF-GAMUT
    C,   // Type-C：SQP 不收敛 → 最近邻节点，不中断出样
};

struct FallbackResult {
    FallbackType type;
    std::array<float, 12> device_values;
    std::string annotation;   // "SOFT-UNCAL" / "OUT-OF-GAMUT" / "FALLBACK-C"
};

FallbackResult apply_fallback(FallbackType type,
                               std::array<float, 3> target_lab,
                               const CYSNLut& lut,
                               const RouteAInput& route_a_input,
                               const G7CorrectionCurve& g7_curve,
                               const LinearizationLUT& lin_lut);
```

**测试**：
- Type-A：调用 Route A chain 并标注 SOFT-UNCAL
- Type-B：返回色域边界上最近可达设备值并标注 OUT-OF-GAMUT

---

## 第十三组：§12 打印驱动

### P31 — PJL 打印任务组装（含 Dynamic Control Strip）
**文件**：`engine/epson/job.h/.cpp`（扩展现有）

**实现**：
```cpp
struct PrintJob {
    std::string print_condition_id;
    std::vector<uint8_t> cmyk_raster;     // §11 输出的 16-bit raster
    std::vector<uint8_t> control_strip;   // §12 生成的 72 patch + dynamic patches
    int width_px;
    int height_px;
    int dpi;
    bool has_W_channel = false;
};

// 生成 Dynamic Control Strip（UGRA/FOGRA Media Wedge V3 = 72 patch + 专色实地）
std::vector<uint8_t> generate_dynamic_control_strip(
    const std::vector<SpotColorInfo>& spot_colors,
    int strip_width_px,
    int dpi);

// 组装完整 PJL 任务（§12 规格：g7_curve → lin_lut → raster → control strip）
std::vector<uint8_t> assemble_pjl_job(const PrintJob& job,
                                        const MediaConfig& media_cfg);
```

**测试**：
- control_strip 宽度 = strip_width_px，高度固定 20px（72 patches 均分）
- PJL 头包含 `@PJL SET COLORMODE=OFF`（关闭驱动色彩管理）

---

### P32 — W 通道两遍打印
**文件**：`engine/epson/job.h/.cpp`（续 P31）

**实现**（规格 §12 W channel 两遍打印）：
```cpp
// 当 media_config.substrate_type 为 "transparent" 或 "dark" 时触发
// 第一遍：底涂 pass（仅 W 通道数据）
// 第二遍：彩色 pass（CMYK + Spot，不含 W）
struct TwoPassJob {
    std::vector<uint8_t> pjl_coat_pass;   // W 通道底涂
    std::vector<uint8_t> pjl_color_pass;  // 彩色主 pass
    int drying_delay_seconds = 30;        // 两遍间等待时间
};

TwoPassJob assemble_two_pass_job(const PrintJob& job,
                                  const LinearizationLUT& lin_lut,
                                  const ChannelLimits& ink_limits);
```

**关键约束**：W 覆盖率 → `lin_lut.apply(W_coverage)` → 输出 ≤ `ink_limits.single_channel_max["W"]`。

**测试**：
- substrate_type = "coated" → 返回 nullopt（不生成 TwoPassJob）
- substrate_type = "transparent" → pjl_coat_pass 非空

---

## 第十四组：§13 ISO 12647-7 验证

### P33 — ΔE00 批量验收 + 灰平衡 ΔH00
**文件**：`engine/calibration/iso_validation.h/.cpp`（新建）

**实现**（规格 §13 + A.9.5）：
```cpp
struct ISOValidationResult {
    float substrate_delta_e   = 0.0f;  // 纸白 ΔE00
    float substrate_delta_H00 = 0.0f;  // 纸白色相差（CIEDE2000 hue 分量）
    float avg_delta_e         = 0.0f;  // 所有 patch 平均
    float max_delta_e         = 0.0f;
    float gray_balance_dH00   = 0.0f;  // 灰平衡色相差（ISO 12647-7，ΔH00）

    // 各项 PASS/FAIL（规格 §13 阈值表）
    bool substrate_pass = false;  // ΔH00 ≤ 1.5 AND ΔE00 ≤ 3.0
    bool avg_pass       = false;  // avg ΔE00 ≤ 1.5
    bool max_pass       = false;  // max ΔE00 ≤ 4.0
    bool gray_pass      = false;  // 灰平衡 ΔH00 ≤ 1.5

    bool overall_pass = false;
    std::vector<int> out_of_gamut_pixels;  // 来自 route_metadata，不从测量重新检测
};

// 计算 CIEDE2000 色相分量 ΔH00（精确版，使用 a'/C'/h' 修正，含 S_H 加权）
// 不使用近似公式 2·√(C*·C*)·sin(Δh/2)（会在高彩度时偏小）
float compute_dH00(std::array<float,3> lab_measured,
                   std::array<float,3> lab_reference);

ISOValidationResult validate_iso12647(
    const std::vector<std::array<float,3>>& measured_lab,
    const std::vector<std::array<float,3>>& reference_lab,
    const std::vector<bool>& out_of_gamut_flags);  // 来自 route_metadata
```

**测试**：
- 完美匹配 → 全 pass
- 灰阶 a*/b* 偏移 2.0 → gray_pass = false
- 调用 compute_dH00 并与 CIEDE2000 标准公式比对（误差 < 0.01）

---

## 第十五组：§14 轻校准

### P34 — 轻校准流程 + LUT 状态机
**文件**：`engine/calibration/quick_cal.h/.cpp`（新建）

**实现**：
```cpp
enum class LUTStatus { VALID, SOFT_VALID, STALE };

struct QuickCalResult {
    bool        success = false;
    float       post_cal_delta_e = 0.0f;
    LUTStatus   lut_status;
    bool        needs_rebuild = false;  // SOFT_VALID/STALE 时触发后台重建
    std::string error;
};

// 执行轻校（更新 g7_curve.lut）
// 参数中 new_g7_curve 来自重新运行 P12（compute_g7_correction）
// 轻校后 linearization_hash 不变（linearization.lut 未更新）
QuickCalResult run_light_cal(
    const G7CorrectionCurve& new_g7_curve,
    float post_cal_delta_e,     // 轻校后重测的 ΔE00
    const std::string& g7_lut_path,
    const std::string& lin_lut_path,
    const std::string& cynsn_lut_path);

// 根据校准结果确定 LUT 状态（规格 §7B LUT 版本控制）：
//   PASS（ΔE < 1.5，无更新）        → VALID
//   轻校 + post-cal ≤ 1.5            → SOFT_VALID + 后台重建
//   轻校 + post-cal > 1.5（情形 B'）→ STALE + 阻断 Route B
//   §15 轻修成功（area_curve 更新）  → VALID 或 SOFT_VALID（hash 不变）
//   §3-§5 线性化重建（情形 C）       → STALE
LUTStatus determine_lut_status(bool linearization_changed,
                                float post_cal_residual_delta_e);
```

**关键约束**：
- `情形 B'`：轻校后 hash 变化（g7_curve 更新）但 post-cal > 1.5 → STALE（同情形 C）
- §15 轻修成功后 linearization.lut 不变，hash 不变，LUT 状态按 post-cal 残差判 B 或 B'

**测试**：
- post-cal = 1.3 → SOFT_VALID
- post-cal = 1.5 → STALE（边界值，情形 B'）
- 无校准更新（PASS）→ VALID

---

## 第十六组：§15 光谱诊断

### P35 — §15 快速诊断（历史控制条数据）
**文件**：`engine/calibration/spectral_diagnosis.h/.cpp`（新建）

**实现**（规格 §15 快速诊断路径，无需打印）：
```cpp
struct DriftVote {
    std::string channel;  // "C", "M", "Y", "K", "Lc", ...
    float vote_score;     // 0–1，越高越可能是漂移通道
};

struct QuickDiagnosisResult {
    std::vector<DriftVote> votes;      // 按 vote_score 降序
    float confidence;                  // top_drift 的 vote_score
    bool  actionable;                  // confidence ≥ 0.7 → 可直接执行轻修
    std::string top_drift_channel;
};

// 从最近出样的控制条光谱数据中推断漂移通道
// 算法：对每个控制条 patch，计算预测光谱 vs 实测光谱的差异；
//       对各通道分解差异（类似 §15 A.10 规格），统计投票
QuickDiagnosisResult run_quick_diagnosis(
    const std::vector<spectral::Spectrum>& measured_spectra,
    const std::vector<spectral::Spectrum>& reference_spectra,
    const CYSNModelData& model_data);
```

**测试**：
- K 通道偏移 5% → top_drift = "K", confidence ≥ 0.7
- 多通道同时偏移 → confidence < 0.7（无法单一归因）

---

### P36 — §15 精确诊断（96-patch 靶图 + 重拟合）
**文件**：`engine/calibration/spectral_diagnosis.h/.cpp`（续 P35）

**实现**：
```cpp
// 生成精确诊断靶图：每通道 8 级 ramp × 12 通道 = 96 patches
PatchSet generate_precise_diagnosis_chart();

struct PreciseDiagnosisResult {
    std::string attributed_channel;   // 单一归因通道（置信度高）
    float attribution_confidence;
    std::array<float, 256> new_area_curve;  // 重拟合后的 area curve（256 点 LUT）
};

// 从 96-patch 实测光谱归因并重拟合目标通道的 area curve
// §15 轻修路径（8-ramp，7 个拟合对；0% 为 R_p 参考）
PreciseDiagnosisResult run_precise_diagnosis(
    const PatchSet& chart,
    const std::vector<spectral::Spectrum>& measured_spectra,
    const CYSNModelData& model_data);
```

**关键约束**：
- 精确诊断 area curve 重拟合使用 8-ramp 7 个拟合对（0% 是 R_p 参考，不是拟合点）
- 不同于 §7B 初始路径（16-segment ramp，15 个拟合对）

**测试**：归因通道与人为引入漂移的通道一致。

---

### P37 — §15 轻修三步 API 调用
**文件**：`engine/calibration/spectral_diagnosis.h/.cpp`（续 P36）

**实现**（规格 §15 轻修后必须执行的三个 API，顺序固定）：
```cpp
// 执行 §15 轻修：更新模型 + 标记 LUT 重建 + 更新 active.json checksum
// 三步必须原子完成（任一步失败回滚）
struct LightRepairResult {
    bool success = false;
    std::string error;
    float validation_rmse;   // 修复后 RMSE，< 0.020 为通过
};

LightRepairResult apply_light_repair(
    const PreciseDiagnosisResult& diagnosis,
    CYSNModelData& model_data,              // 就地修改
    const std::string& model_path,          // 写回磁盘
    const std::string& cynsn_lut_path,      // 标记重建 pending
    const std::string& active_json_path);   // 更新 checksum_profile

// 步骤分解：
// 1. cynsn_model.update_area_curve(channel, new_curve) → 写回 cynsn_model.bin
// 2. cynsn_lut.set_rebuild_pending(true) → 后台触发 LUT 重建
// 3. active_json.update_checksum_profile(new_sha256) → 更新 active.json
```

**测试**：三步全部执行；若 model_path 不可写则失败并回滚（cynsn_lut 不标记 pending）。

---

## 第十七组：云端 API（Go）

### P38 — colprof 请求/响应 schema 对齐 §7A
**文件**：`cloud/api/icc_handler.go`（修改现有）

现有 `ICCRequest` 字段为通用格式，不包含 §7A 必须的参数。

**实现**：
```go
// 替换现有 ICCRequest
type ICCRequest struct {
    TI3Data          []byte  `json:"ti3_data"`           // .ti3 文件内容（base64）
    ProfileName      string  `json:"profile_name"`
    Quality          string  `json:"quality"`             // "h" = -qh（默认）
    DeviceChannels   int     `json:"device_channels"`     // -d3（默认 12）
    BlackStart       float64 `json:"black_start"`         // bs（0–1）
    BlackMax         float64 `json:"black_max"`           // bm（0–1）
    UCR              float64 `json:"ucr"`                 // ucr（0–1）
    TACPercent       int     `json:"tac_percent"`         // TAC_total × 100（整数）
}
```

**关键约束**：
- `TACPercent` 是整数（如 320），直接传给 `-l 320`
- `black_start/max/ucr` 是 0–1 分数，直接传给 `-K`，不除以 100

**测试**：用 TAC=3.2 的请求（TACPercent=320）验证生成命令包含 `-l 320`。

---

### P39 — colprof 子进程调用（Go）
**文件**：`cloud/api/colprof_runner.go`（新建）

**实现**：
```go
type ColprofRunner struct {
    WorkDir    string
    ArgyllPath string  // colprof 二进制路径，如 "/usr/bin/colprof"
}

// 从 ICCRequest 构建 colprof 命令行
func (r *ColprofRunner) BuildCmd(req ICCRequest, ti3Path, outputName string) []string
// 返回形如：
// ["colprof", "-qh", "-d3", "-k", "r",
//  "-K", "0.20,1,1,0.30,0.95,0", "-l", "320",
//  "-c", "mt", outputName]

// 执行 colprof，返回生成的 ICC 文件路径
func (r *ColprofRunner) Run(ctx context.Context, req ICCRequest) (string, error)
```

**关键约束**：
- `-K` 参数格式：`fmt.Sprintf("%.2f,1,1,%.2f,%.2f,0", req.BlackStart, req.UCR, req.BlackMax)`
- `-l` 参数：`fmt.Sprintf("%d", req.TACPercent)`
- 超时：与现有 `MaxProcessingTime = 30s` 集成

**测试**：验证 BuildCmd 输出包含 `-qh -d3 -k r -K 0.20,1,1,0.30,0.95,0 -l 320`。

---

### P40 — PostgreSQL job 状态表 migration
**文件**：`cloud/db/migrations/001_icc_jobs.sql`（新建）

**实现**：
```sql
CREATE TABLE icc_jobs (
    id           UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    status       TEXT NOT NULL DEFAULT 'pending',
    -- pending / processing / completed / failed
    ti3_r2_key   TEXT,          -- 上传的 .ti3 在 R2 中的 key
    icc_r2_key   TEXT,          -- 生成的 .icc 在 R2 中的 key
    request_json JSONB NOT NULL, -- 完整 ICCRequest
    cost         DECIMAL(10,4),
    error        TEXT,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMPTZ
);
CREATE INDEX ON icc_jobs (status, created_at);
```

**测试**：migration 可正常 apply 和 rollback。

---

### P41 — 异步 colprof 任务 + R2 上传
**文件**：`cloud/api/job_processor.go`（新建）

**实现**：
```go
type JobProcessor struct {
    DB       *pgxpool.Pool
    Redis    *redis.Client
    R2       R2Client
    Runner   *ColprofRunner
}

// 异步处理一个 ICC 生成任务
// 1. 从 DB 取出 pending 任务
// 2. 下载 .ti3 from R2
// 3. 调用 ColprofRunner.Run()
// 4. 上传 .icc 到 R2
// 5. 生成签名 URL（7 天有效期）
// 6. 更新 DB status = completed
func (p *JobProcessor) Process(ctx context.Context, jobID string) error
```

**测试**（mock R2 和 Runner）：
- 成功路径：DB 状态从 pending → completed，result_url 非空
- colprof 失败：DB 状态从 pending → failed，error 非空

---

## 第十八组：Qt UI — Setup Wizard

### P42 — Setup Wizard 框架 + Step 1（硬件检查）
**文件**：`apps/jcolour/ui/setup_wizard.h/.cpp`（新建）

**实现**：
- `QWizard` 子类，7 个步骤
- Step 1 页面：显示喷头检测图，用户点击"打印检测图" → 调用 `pjl.h` 发送检测 PJL
- 用户确认喷头正常后 → `PrintCondition::transition(state, HARDWARE_OK)`
- 进度条显示当前步骤

**关键约束**：Step 1 完成后即转换 → HARDWARE_OK，Step 2（纸白测量）在 HARDWARE_OK 状态内完成，不产生新状态转换。

---

### P43 — Setup Wizard Step 2（介质注册 + OBA 检测）
**文件**：`apps/jcolour/ui/setup_wizard.h/.cpp`（续 P42）

**实现**：
- 介质参数表单（纸张名称、型号、墨水类型 PK/MK）
- 引导用户执行 5 点纸白测量（先测 M1，再测 M0）
- 显示 OBA 检测结果（调用 P06 `detect_oba()`）：
  - NONE：绿色 ✓
  - WEAK：黄色警告"建议使用 M1 测量"
  - STRONG：橙色警告 + 锁定 M1
- 写入 media_config.json（P07）

---

### P44 — Setup Wizard Steps 3–5（墨量 + G7 + 线性化）
**文件**：`apps/jcolour/ui/setup_wizard.h/.cpp`（续 P43）

**实现**：
- Step 3：打印墨量测试靶图 → i1Pro3 引导测量 → 显示各通道饱和点 → 生成 ink_limit.json（P09）
- Step 4：打印 P2P51 靶图 → 测量 → 显示 NPDC 曲线偏差 → 生成 g7_curve.lut（P12）
- Step 5：打印线性化靶图 → 测量 → 显示各组曲线 → 生成 linearization.lut（P05/P13）
- 每步显示 ΔE 偏差指示（绿/黄/红）

---

### P45 — Setup Wizard Steps 6–8（靶图 + ICC + 绑定）
**文件**：`apps/jcolour/ui/setup_wizard.h/.cpp`（续 P44）

**实现**：
- Step 6：打印 Route A/B 靶图 → i1Pro3 引导扫描（支持 i1iO 自动扫描）→ 清洗 → 生成 .ti3
- Step 7A：提交 ICC 生成（P38–P41）→ 显示进度 → 下载 ICC → 显示验证 ΔE
- Step 7B：CYNSN LUT 构建（P22/P23）→ 进度条（约 3–5 分钟）→ 完成后显示 PASS/WARNING/FAIL
- Step 8：执行 PrintCondition 绑定（P24）→ 显示 READY 状态

---

## 第十九组：Qt UI — 生产模式

### P46 — AutoCal Check 流程 UI
**文件**：`apps/jcolour/ui/autocal_dialog.h/.cpp`（新建）

**实现**：
- Job 提交时自动触发 AutoCal Check（P25）
- GREEN：无 UI 弹出，直接继续（UI 状态栏显示 ✓ 绿色）
- AMBER：弹出 "打印 Quick Check Strip？" 对话框 → 用户确认 → 打印 → 测量 → 结果显示
- RED：弹出 "打印机进入 ON_HOLD" 警告，提供"急件绕过"选项

---

### P47 — Job 队列 + 路由显示
**文件**：`apps/jcolour/ui/queue.h/.cpp`（扩展现有）

**实现**：
- 列表每行显示：Job 名称 / PrintCondition / Route A+B 比例（如"Route A: 73%, Route B: 27%"）
- 超色域像素统计（来自 route_metadata，格式如"OUT-OF-GAMUT: 0.3%"）
- 拖拽排序
- UNCALIBRATED 状态 Job 以橙色高亮

---

### P48 — ISO 12647-7 报告查看器
**文件**：`apps/jcolour/ui/iso_report_viewer.h/.cpp`（新建）

**实现**：
- 显示 §13 `ISOValidationResult` 的各项指标
- ΔE00 逐 patch 明细表（L*/a*/b* 实测 vs 基准，ΔE00，PASS/FAIL）
- 灰平衡 a*/b* 轨迹折线图（横轴 L*，纵轴 a*/b*）
- 超色域热力图叠加在预览图上（来自 route_metadata）

---

## 第二十组：Qt UI — 维护模式

### P49 — §14/§15 诊断面板
**文件**：`apps/jcolour/ui/maintenance_panel.h/.cpp`（新建）

**实现**：
- §14 入口：显示当前 ΔE，提供"轻校"按钮（1.5–3.0 区间）和"精确诊断"按钮（≥ 3.0）
- §15 快速诊断结果：通道漂移投票柱状图（12 通道，sorted by vote_score）
- 轻修进度：执行三步 API（P37）后显示 RMSE 改善值
- LUT 重建进度条（STALE → 后台重建 → VALID，约 3–5 分钟）

---

### P50 — PrintCondition 状态面板（Fiery 风格）
**文件**：`apps/jcolour/ui/print_condition_panel.h/.cpp`（新建）

**实现**（UI-SPEC-FIERY.md §2 颜色系统）：
- 9 种状态对应颜色（UNCONFIGURED=灰/HARDWARE_OK=蓝/#0078d4/CALIBRATED=蓝/PROFILED=蓝/READY=绿/#2f8c2f/NEEDS_RECAL=橙/#ff8c00/DEGRADED=红/#e81123/BATCH_CHANGED=橙/ON_HOLD=红）
- 左侧导航树：服务器 → 打印机 → PrintCondition 列表，●颜色指示状态
- 打印机物理状态（就緒/列印中/警告/錯誤/離線）与 PrintCondition 逻辑状态独立显示
- 点击 PrintCondition → 右侧属性面板显示详情

---

## 工作包依赖图

```
P01(状态机) ──┐
P02(active.json) ──┤
P03(baseline) ──┤──→ P24(绑定流程) ──→ P42-P45(Wizard)
P04(history) ──┤
P05(lin.lut) ──┘
                    
P06(OBA) ──→ P07(media_config) ──→ P43(Wizard Step 2)

P08/P09(ink limit) ──→ P10(TAC) ──→ P13(lin curve) ──→ P44(Wizard Step 3)

P11(NPDC table) ──→ P12(g7 curve) ──→ P44(Wizard Step 4)

P14/P15(chart gen) ──→ P16/P17(cleaning) ──→ P18/P19(ICC) ──→ P45(Wizard 6-7A)

P20(model bin) ──→ P21(area curve) ──→ P22/P23(LUT) ──→ P23(LUT bin) ──→ P45(Wizard 7B)

P25(AutoCal) ──→ P46(UI AutoCal)
P26/P27(PDF) ──→ P47(UI Queue)
P28/P29/P30(Route A/B) ──→ P31/P32(PJL) ──→ P33(ISO) ──→ P48(UI Report)
P34(QuickCal) ──→ P35/P36/P37(Diagnosis) ──→ P49(UI Maintenance)
P38-P41(Cloud) ──→ P45(Wizard 7A)

P01(状态机) + P02(active) ──→ P50(UI Panel)
```

---

## 快速参考：关键数值

| 量 | 值 | 来源 |
|----|-----|------|
| cynsn_lut.bin 大小 | 843 KB（uint16）/ 1.7 MB（float32） | 35937×12×2 bytes |
| LUT 网格 | 33×33×33 = 35,937 节点 | L*[0,100]/a*[-80,80]/b*[-80,80] |
| SQP iter（建档） | 50 次 | 每节点 ~50ms |
| SQP iter（出样精化） | 1–3 次 | 每像素 <1μs |
| TAC 安全上限 | 3.80（倍率）= 380% | ink_limit.json 倍率格式 |
| Argyll -l 参数 | TAC_total × 100（整数）| 3.2 → `-l 320` |
| -K 参数 | bs,1,1,ucr,bm,0（0–1 分数，直传）| 不除以 100 |
| AutoCal GREEN | ΔE00 < 1.5（严格小于）| 1.5 本身判 AMBER |
| LUT 情形 B' | 轻校 + post-cal > 1.5 → STALE | 同情形 C |
| Murray-Davies §7B | 16 段 ramp，15 拟合对 | 初始建档 |
| Murray-Davies §15 | 8 段 ramp，7 拟合对（0% = R_p 参考）| 轻修重拟合 |
| linearization.lut 格式 | float32 × 256 × 12，12,288 bytes | 通道索引 0–11 = C..W |
| G7 NPDC | P2P51 查找表，非线性 | 不可用线性公式 |
| ΔH00 | CIEDE2000 hue 分量，含 S_H 加权 | 不可用 ΔH' 近似 |
| 批量门禁 | > 8% → BLOCK；3–8% → WARN | 相对阈值 |
| 超色域标注来源 | route_metadata（§11 写入）| §13 不重新检测 |
| LUT 应用顺序 | g7_curve.lut **先**，linearization.lut **后** | §11 Route A |
| W 通道 | §12 打印驱动层注入，不经 DeviceN，不占 MaxSpots | 8 个配额仅 spot colors |
