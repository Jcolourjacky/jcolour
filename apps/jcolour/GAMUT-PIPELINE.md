# GAMUT-PIPELINE — 广色域调用执行规范

> **范围**：[`PIPELINE.md`](PIPELINE.md) §7–§9 的执行级深度版
> **架构决策**：[`ADR-29`](../../docs/DECISIONS.md#adr-29)
> **目标读者**：engine 开发者、客户端工程师、QA
> **文档版本**：2026-05 初稿

PIPELINE.md 给完整流程，本文档给参数、伪代码、UI 契约、测试用例，让工程师能直接落地。

---

## §1 五道色域闸门（入口检测）

任何一道闸门不通过，**拒绝进入转换管线**。

### G1：源 profile = ProPhoto RGB
```cpp
auto src_profile = cmsOpenProfileFromFile(path, "r");
auto colorspace = cmsGetColorSpace(src_profile);
auto desc = cmsGetProfileInfoASCII(src_profile, cmsInfoDescription, ...);

if (colorspace != cmsSigRgbData) → REJECT
if (!matches_prophoto_primaries(src_profile, ε=0.001)) {
    if (matches_adobergb(...))   → WARN_YELLOW("AdobeRGB 截 ~15% P9500 黄绿区")
    else if (matches_srgb(...))  → WARN_RED("sRGB 已损失 30% 色域，建议重出文件")
    else                         → WARN_AMBER("未识别色域，按 sRGB 处理")
}
```

### G2：bit depth = 16
```cpp
if (tiff.BitsPerSample != 16) → REJECT("ProPhoto 8-bit 必然 banding")
```

### G3：无 alpha 预乘 / 无 hard clip
- TIFF `ExtraSamples = ASSOCALPHA` → 反预乘后再进
- 检测 0/65535 像素占比 > 0.5% → 警告"高光/暗部已裁剪"

### G4：显示器色域 ≥ Display P3（仅软打样需要）
```cpp
auto display_profile = ColorSyncProfileCreateWithDisplayID(0);
auto coverage = compute_gamut_coverage(display_profile, "DisplayP3");
if (coverage < 0.95) → DISABLE_SOFTPROOF_BUTTON("屏幕色域 < P3，软打样不可信")
```

### G5：目标 ICC 完整性
```cpp
auto dst = cmsOpenProfileFromFile("printer.icc", "r");
if (cmsIsTag(dst, cmsSigAToB0Tag) == FALSE)  → REJECT  // perceptual
if (cmsIsTag(dst, cmsSigAToB1Tag) == FALSE)  → REJECT  // relative
if (cmsIsTag(dst, cmsSigBToA0Tag) == FALSE)  → REJECT  // 反向必备
if (channels != 12)                          → WARN("非 PRO 12 设备，色域优势打折")
```

---

## §2 转换 6 决策点详细参数

### D1：CMM = LCMS2，关键 flag

```cpp
cmsHTRANSFORM xform = cmsCreateTransform(
    src_prophoto,                              // ProPhoto RGB
    TYPE_RGB_16,                               // 16-bit input
    dst_printer_icc,                           // 12 通道 device profile
    TYPE_NCOLOR(12) | BYTES_SH(2),             // 12 通道 16-bit output
    selected_intent,                           // 来自 GamutAdvisor + 用户确认
    cmsFLAGS_HIGHRESPRECALC          |         // 强制高分辨率预计算 LUT
    cmsFLAGS_BLACKPOINTCOMPENSATION  |         // BPC 默认 ON
    cmsFLAGS_COPY_ALPHA                        // 保 alpha
    // 不加 cmsFLAGS_NOOPTIMIZE 除非 debug
);
```

**坑点**：
- 默认 `cmsCreateTransform` 不开 BPC、不开 HIGHRESPRECALC —— 多数 CMM 实现的"第一手错误"
- LCMS2 ≥ 2.13，否则 12 通道 NCOLOR 支持有 bug

### D2：Rendering Intent

由 GamutAdvisor + 用户拍板（见 §3）。本环节只是 LCMS2 的 `intent` 参数：

| 选择 | LCMS2 const |
|---|---|
| Perceptual | `INTENT_PERCEPTUAL` |
| Relative + BPC | `INTENT_RELATIVE_COLORIMETRIC` + `cmsFLAGS_BLACKPOINTCOMPENSATION` |
| Saturation（罕用） | `INTENT_SATURATION` |
| Absolute（罕用） | `INTENT_ABSOLUTE_COLORIMETRIC` |

### D3：BPC

默认 ON。例外：源已是某 device 黑（极少摄影场景）。UI 提供"高级模式"开关，但默认隐藏。

### D4：LUT 插值

LCMS2 默认 Tetrahedral（ICC v4 标准）。**不动**。仅在 v2.0+ 研究阶段评估高阶 spline。

### D5：16-bit 全程贯通

| 段 | 必须 16-bit |
|---|---|
| 文件 IO | libtiff `BitsPerSample = 16` |
| LCMS2 transform | `TYPE_RGB_16` / `TYPE_NCOLOR(12) \| BYTES_SH(2)` |
| §4/§5 LUT 应用 | uint16 数组 |
| 半色调输入 | 16-bit；输出 1-bit 点阵但密度由 16-bit 决定 |
| PJL raw | `ESC ( G` 系列设 16-bit data format（Spike 2 验证） |

任一段降到 8-bit → 整段管线作废。

### D6：性能预算

| 图像 | 预算（M2 Pro） |
|---|---|
| 24×36" @ 360dpi 12 通道 | < 30 秒 |
| 44×60" @ 360dpi 12 通道 | < 90 秒 |

超出预算 → 降级到 16³ LUT，告警监控。

---

## §3 GamutAdvisor (L2) 算法规范

### 输入
- ProPhoto 16-bit 图像
- `printer.icc` 的色域边界（cmsCreateProofingTransform 副产）

### 输出
```cpp
struct IntentRecommendation {
    cmsUInt32Number intent;              // INTENT_PERCEPTUAL / INTENT_RELATIVE_COLORIMETRIC
    bool bpc;
    float confidence;                    // 0.0–1.0
    std::string rationale;               // UI 显示给用户的"为什么推荐"
};
std::array<IntentRecommendation, 2> recommend(...);
```

### 伪代码

```python
def gamut_advisor(image, dst_icc):
    # 1. 直方图：Lab C* 分布
    lab = transform_image_to_lab(image)
    chroma_hist = histogram(lab.C, bins=20, range=(0, 150))
    high_chroma_ratio = sum(chroma_hist[12:]) / image.pixel_count  # C* > 90 占比

    # 2. 出色域比例
    boundary = dst_icc.gamut_boundary_descriptor()
    out_of_gamut = count_pixels_outside(lab, boundary) / image.pixel_count

    # 3. 记忆色掩码
    skin_mask = detect_skin(lab)             # ITU-R BT.601 范围 + 经验调整
    sky_mask = detect_sky(lab)               # b* < -10, L* > 50, 上 1/3 区域权重
    foliage_mask = detect_foliage(lab)       # a* < -5, b* > 5, L* in (20,70)

    skin_ratio = skin_mask.mean()
    sky_ratio = sky_mask.mean()
    foliage_ratio = foliage_mask.mean()
    memory_color_ratio = max(skin_ratio, sky_ratio + foliage_ratio)

    # 4. 决策
    if high_chroma_ratio > 0.20 and memory_color_ratio < 0.30:
        # 大量饱和色，记忆色少 → 推荐 Relative + BPC（顶到 Or/Gr/V）
        primary = (RELATIVE_COLORIMETRIC, bpc=True, confidence=0.85,
                   rationale="高饱和图（C*>90 像素占 {:.0%}），相对色度 + BPC 顶到 Or/Gr/V 边界更鲜艳")
        secondary = (PERCEPTUAL, bpc=True, confidence=0.50,
                     rationale="如不喜欢硬切感，可退回感知")
    elif memory_color_ratio > 0.30:
        # 含大量记忆色 → 优先 Perceptual 保自然
        primary = (PERCEPTUAL, bpc=True, confidence=0.80,
                   rationale="含记忆色（肤色/天空/植物 {:.0%}），感知模式保色调自然")
        secondary = (RELATIVE_COLORIMETRIC, bpc=True, confidence=0.40,
                     rationale="若需要饱和度更高可换相对色度，但记忆色可能偏色")
    elif out_of_gamut < 0.05:
        # 几乎全在色域内
        primary = (RELATIVE_COLORIMETRIC, bpc=True, confidence=0.90,
                   rationale="图像几乎全在 P9500 色域内（出色域 {:.1%}），相对色度 1:1 还原")
        secondary = (PERCEPTUAL, bpc=True, confidence=0.40, rationale="...")
    else:
        # 默认中间地带
        primary = (PERCEPTUAL, bpc=True, confidence=0.65, ...)
        secondary = (RELATIVE_COLORIMETRIC, bpc=True, confidence=0.55, ...)

    return [primary, secondary]
```

### 测试用例

| 测试图 | 期望 primary | 验证 |
|---|---|---|
| 夕阳风光（高 C*，无肤色） | Relative + BPC | confidence ≥ 0.80 |
| 室内人像（中等 C*，肤色 > 30%） | Perceptual | confidence ≥ 0.75 |
| 黑白照片伪装（C* ≈ 0） | Perceptual | confidence ≥ 0.85 |
| 花卉特写（极高 C*） | Relative + BPC | confidence ≥ 0.90 |
| 标准 ColorChecker 24 块 | 任意（应有低置信度告警） | confidence < 0.50 |

### 准确率目标
盲测：种子摄影师 5 人，每人 20 张图自选 intent vs GamutAdvisor 推荐 → 一致率 ≥ 70%。

---

## §4 A/B 软打样 (L3) UI 契约

### 行为规范
1. 用户进入"打印准备"界面 → 自动调用 GamutAdvisor → 返回 2 个候选
2. 屏幕**同时**渲染两版（Photoshop View → Proof Setup 同等效果）
3. 每版显示：
   - 软打样图像
   - intent 名称 + GamutAdvisor 推理文本（rationale 字段）
   - Or/Gr/V 调用率预测（Per-channel ink coverage 预估）
   - Gamut warning 灰色覆盖（出色域像素）
4. 用户点选其一 → 决策写入图像元数据 (`com.jcolour.intent`, `com.jcolour.bpc`)
5. 高级用户可点"自定义" → 暴露第三个候选（任意 intent + BPC + 强度滑块）

### Paper White Simulation
- **默认 ON**（ADR-26 EPSON-COLOR-PROBLEMS §10 P0）
- 状态条标注："已模拟 [纸张名] 纸白 (a*={..}, b*={..})"
- 用户可临时关闭对比，但保存预设时强制 ON

### 屏幕色域守门
- 显示器 ICC 覆盖率检测（参见 §1 G4）
- < 95% Display P3：软打样按钮**禁用**（不是隐藏）+ tooltip："软打样需要至少 Display P3 屏；当前屏幕仅 {:.0%} P3 覆盖，结果不可信"
- 不强制阻止打印，但禁止软打样

### 性能
- 两版同时渲染：< 2 秒（大图缩略 1024×768 即可，不需要全分辨率）

---

## §5 SemanticGamutMapper (L4) 接口设计

### 数据流

```
ProPhoto 16-bit image
       │
       ↓
[CoreML 语义分割]  ←── model.mlpackage
       │
       ↓
SegmentMap {
    skin_mask: float[H, W],
    sky_mask: float[H, W],
    foliage_mask: float[H, W],
    subject_mask: float[H, W],
    background_mask: float[H, W],
    other_mask: float[H, W],
    confidence: float[H, W],
}
       │
       ↓
[Per-segment intent assignment]
       │
       ├─ skin     → INTENT_PERCEPTUAL,  strength=1.0
       ├─ sky      → INTENT_RELATIVE,    strength=0.7
       ├─ foliage  → INTENT_RELATIVE,    strength=0.8
       ├─ subject  → 用户主导（默认 GamutAdvisor primary）
       ├─ background → INTENT_RELATIVE,  strength=1.0
       └─ other    → fallback to global
       │
       ↓
[Two-pass LCMS2 transform with mask blending]
   - Pass 1: 全图用 INTENT_PERCEPTUAL 转
   - Pass 2: 全图用 INTENT_RELATIVE 转
   - 最终输出 = pass_1 * (1 - relative_weight_mask) + pass_2 * relative_weight_mask
       │
       ↓
12 通道 device raster
```

### 接口

```cpp
namespace jcolour::gamut {

struct SegmentMap {
    enum Class { Skin, Sky, Foliage, Subject, Background, Other };
    std::array<cv::Mat, 6> masks;          // float32, [H,W], 0-1
    cv::Mat confidence;                    // float32, [H,W], 0-1
};

struct SegmentIntent {
    cmsUInt32Number intent;
    bool bpc;
    float strength;                        // 0.0=纯 perceptual, 1.0=纯 relative
};

class SemanticGamutMapper {
public:
    // CoreML 推理；< 0.7 全局置信度 → 抛 LowConfidence 由调用方降级
    SegmentMap segment(const Image16& img);

    // 应用分区 intent 到 LCMS2 transform pipeline
    Image12ch transform(const Image16& src,
                        cmsHPROFILE dst_icc,
                        const SegmentMap& segments,
                        const std::map<SegmentMap::Class, SegmentIntent>& policy);

    struct Telemetry {
        std::array<float, 6> class_pixel_ratios;
        float overall_confidence;
        double inference_ms;
    };
    Telemetry last_telemetry();
};

}
```

### 失败降级

```cpp
try {
    auto seg = mapper.segment(image);
    if (seg.overall_confidence < 0.70) throw LowConfidence{};
    output = mapper.transform(image, icc, seg, policy);
} catch (LowConfidence&) {
    // 退回 L2/L3：仅用 GamutAdvisor 全局 intent
    auto reco = advisor.recommend(image, icc);
    output = lcms_transform(image, icc, reco[0].intent, reco[0].bpc);
    log_telemetry("semantic_fallback", reco[0].confidence);
}
```

### 模型选型（Spike 阶段确定）
候选：
- DeepLab v3+ (MobileNetV3 backbone) — 准确率高，推理 ~200ms/4MP M2 Pro
- MobileNet-SAM — 实时，质量略低
- Apple Vision `VNGenerateAttentionBasedSaliencyImageRequest` — 系统自带，免费但分类粗

最终选型在 Spike 5 验证（5 位摄影师盲测 + 性能 benchmark）。

---

## §6 验证矩阵

整条 §1–§5 的端到端验收基于一组**测试图集**：

| 测试图 | 检查 |
|---|---|
| Northlight 色域测试图 | Or/Gr/V 调用率 > 25% |
| ColorChecker 24 块 | 平均 ΔE2000 < 2 |
| 灰阶 256 段 | 全段 ΔE2000 < 1.5，无 banding |
| 肤色 ramp（含浅黄、亚洲、深棕） | 所有色块 ΔE2000 < 2 |
| 蓝天 ramp（清晨到正午到夕阳） | 平均 ΔE2000 < 2.5；视觉无色阶断层 |
| 高饱和花卉特写 | Or/Gr/V 任一通道 > 30%；视觉无块状失色 |
| 暗调人像（红衣 + 阴影脸） | 肤色 ΔE < 2；衣服 ΔE < 4；语义分割正确分肤 vs 衣 |

每次 ICC profile 重生成（§6）后跑一遍这组测试，整套 < 15 分钟。

---

## §7 失败模式表

| 现象 | 可能根因 | 自动定位 |
|---|---|---|
| Or/Gr/V 调用率 < 5% | 源色域过小（sRGB/AdobeRGB） / 选错 intent / §1 测靶 12 通道覆盖不全 | 查 §1 G1 / §3 推荐结果 / §1 印张通道完整性 |
| 平均 ΔE > 5 | §6 ICC 偏 / §4 G7+ 不绿 / §5 线性化漂 | 跑 §6 验证矩阵；不通过 → §6/§5/§4 反查 |
| 肤色偏色 | 语义分割误判（衣物当皮肤） / GamutAdvisor 推 Saturation | 看 SegmentMap 可视化；置信度低 → 强制 L2/L3 |
| Banding | 8-bit 中断 / LUT 17³ 太粗 | 查 §1 G2 / §6 LUT 大小 |
| 软打样与实打不符 | 屏幕色域 < P3 / Paper White Simulation 关 | 查 §1 G4 / §4 状态条 |
| 性能崩坏 | 大图 + 12 通道 + L4 同时启用 | 退化到 16³ LUT；CoreML 推理放低分辨率 |
| 同色异谱（D50→D65 偏色） | 单一光源 profile | 启用 Multi-spectral metamerism 模块（D50/D65/A/F11 切换） |

---

## §8 关联

- 流程总览：[`PIPELINE.md`](PIPELINE.md)
- 架构决策：[`ADR-29`](../../docs/DECISIONS.md#adr-29)
- 痛点知识库：[`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md) §10
- LCMS2 / 技术栈：[`ADR-13`](../../docs/DECISIONS.md#adr-13)
- 16-bit 全程：[`ADR-26`](../../docs/DECISIONS.md#adr-26)
- 光谱契约：[`ADR-17`](../../docs/DECISIONS.md#adr-17)
- Spike：Spike 1（GamutAdvisor 算法）、Spike 5（A/B + L4 UI）

---

## §9 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-05 | v1.0 | 初稿，§1–§7 端到端覆盖 |
