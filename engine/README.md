# engine/ — jcolour & gcolour 共享 C++ 引擎库

> 跨产品共用的色彩引擎核心。jcolour（摄影师 App）和 gcolour（打样 App）都依赖这层。
> 详细规划见 [`../docs/PLAN-shared.md`](../docs/PLAN-shared.md)。

---

## 目录结构（实际状态，2026-05）

```
engine/
├── src/                            约 14 K 行 C++
│   ├── channel_layout.h            ★ 11-active / 12-physical 型别系统（PK/MK 互斥编译期保证）
│   ├── spectral.{h,cpp}            36 通道光谱核心（ADR-17）
│   ├── delta_e.{h,cpp}             ΔE76 / ΔE94 / ΔE2000 / Spectral RMS
│   ├── km.{h,cpp}                  Kubelka-Munk 前向模型
│   ├── cynsn.{h,cpp}               Cellular Yule-Nielsen Spectral Neugebauer
│   ├── forward_factory.{h,cpp}     K-M / CYNSN 模型工厂
│   ├── sqp.{h,cpp}                 Sequential Quadratic Programming
│   ├── jacobian.{h,cpp}            数值雅可比
│   ├── constraints.{h,cpp}         优化约束
│   ├── cost.{h,cpp}                耗墨成本
│   ├── g7_validation.{h,cpp}       G7+ NPDC 验证 + 校正曲线（823 行）
│   ├── gamut_mapping.{h,cpp}       3D Gamut Hull（convex hull in Lab）
│   ├── gamut_gate.{h,cpp}          源 profile / 16-bit / display 闸门
│   ├── intent_advisor.{h,cpp}      GamutAdvisor (L2)
│   ├── segment_mapper.h            SemanticGamutMapper (L4，仅声明，v1.5)
│   ├── softproof.{h,cpp}           A/B 软打样 + Or/Gr/V meter + 显示色域检测
│   ├── color_pipeline.{h,cpp}      ProPhoto → ICC 主管线
│   ├── tiff_io.{h,cpp}             16-bit TIFF 读写
│   ├── halftone.{h,cpp}            Floyd-Steinberg + Vector ED
│   ├── pjl.{h,cpp}                 Epson PJL 直连（getaddrinfo / port 9100 / 通道映射）
│   ├── i1sdk.{h,cpp}               X-Rite i1 SDK 封装（394 行）
│   ├── profile_library.{h,cpp}     ICC profile 库
│   ├── true_shape_nesting.{h,cpp}  libnest2d + Clipper
│   ├── tiling.{h,cpp}              大图分块
│   ├── external_cutter.{h,cpp}     OPOS / ARMS / CutContour
│   └── ...
│
├── workflow/                       约 5 K 行（wf1–wf6 全部 11-active 模型，详见 workflow/README.md）
│   ├── wf1_*                       单黑 1-ch B&W
│   ├── wf2_*                       多色黑 3-ch B&W + BlackKind
│   ├── wf3_*                       B&W + 微彩 6-ch + 真实 K-M
│   ├── wf4_*                       全彩 11-ch + Argyll ICC + WF4MeasurementBackend
│   ├── wf5_*                       — 编号保留
│   ├── wf6_*                       ProPhoto → 11-ch + ICC-sampled gamut hull
│   └── README.md                   ★ workflow 规范 + 11-active 通道契约
│
├── calibration/                    约 0.7 K 行
│   ├── state_machine.{h,cpp}       CalibState (4) + PrintConditionState (9，2026-05 新增)
│   ├── db.{h,cpp}                  校准数据库
│   ├── charts.{h,cpp}              图表生成
│   ├── consistency.{h,cpp}         一致性检查
│   └── self_test.{h,cpp}           自测
│
├── epson/                          Epson 驱动层（PJL / ESC-PR）
│   ├── epson.{h,cpp}
│   ├── job.{h,cpp}
│   └── beo.{h,cpp}                 Black Enhance Overcoat
│
├── argyll/                         Argyll CMS 子进程封装（GPL 隔离，ADR-07）
│   └── argyll.{h,cpp}
│
├── tests/                          54 个 Catch2 测试套件（~2460+ assertions 通过）
│
└── CMakeLists.txt
```

---

## 设计原则

### 1. 光谱契约（ADR-17）— 项目宪法
- 内部存储、比较、优化、配方匹配一律用光谱反射率曲线（36 通道）
- Lab / ΔE / XYZ 仅作 UI 显示派生数据
- **任何模块都不允许在中间偷偷压到 Lab 再算**

### 2. 通道契约（2026-05 新增，`channel_layout.h`）
- 内部所有 11-active 通道操作用 `ActiveInks = std::array<float, 11>`
- PJL 边界用 `expand_to_physical(active, kind) → PhysicalInks` 还原 12 槽位
- PK/MK 互斥**编译期强制**——不可能写出双黑同时输出的代码
- 通道顺序见 [`workflow/README.md`](workflow/README.md) → 11-Active 通道索引表

### 3. 平台独立
- engine/ 不依赖 Qt / macOS / Windows 特定代码
- macOS 特定层（如 PJL CUPS 接口）放 apps/jcolour/ 下
- 长期为 Windows v2.0 预留

### 4. 沙盒友好
- `i1sdk` 计划 entitlements 详见 `pjl.h::is_sandbox_safe()` 文档
- 任何文件 IO 走传入路径，不假设当前工作目录

### 5. GPL 隔离（ADR-07）
- engine/ 无 GPL 依赖
- ArgyllCMS（GPL）只在云端子进程运行，engine/ 不链接 Argyll

### 6. 性能
- 36 通道光谱中间表示不能成为性能瓶颈
- SIMD（NEON / AVX2）针对热路径（待优化）
- 大图（44"×60" @ 360dpi）转换 < 30 秒（M2 Pro，Spike 1 验证目标）

---

## 构建

```bash
brew install cmake little-cms2 libtiff sqlite catch2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 跑所有测试
for t in build/test-*; do
    [ -x "$t" ] && "$t" || true
done
```

CMake 3.20+ 需求；C++17；macOS Universal Binary（Apple Silicon + Intel）。

### 依赖
- LCMS2 (MIT)、libtiff、libpng、libjpeg-turbo
- SQLite3
- Catch2 v3（测试）
- X-Rite i1 SDK（商业，需授权）

---

## 下游消费者

| 应用 | 引擎模块使用 |
|------|------------|
| **jcolour**（摄影师专版） | spectral, g7, icc, i1sdk, pjl, kubelka_munk, channel_layout, wf1-wf6, nesting, cutting, test_print |
| **gcolour**（打样专版） | spectral, g7, icc, i1sdk, kubelka_munk（完整）, nesting, cutting, test_print（不用 pjl；CMYK 走 Fiery 兼容协议） |
| **cloud/**（云端 ICC API） | spectral, icc（轻量子集，主要调 Argyll 子进程） |

---

## 当前状态（2026-05）

- ✅ 引擎核心 ~14 K 行，5 个 workflow（wf1/2/3/4/6）+ shared types 全部就位
- ✅ 完整 audit 30 个 bug 全部清空（详见 `apps/jcolour/ROADMAP.md` Phase A–E 章节）
- ✅ 54/55 测试套件通过
- 🟡 5 个 Spike 待实机/沙盒/盲测验收（详见 `apps/jcolour/ROADMAP.md`）

下一步：进入 Spike 验收阶段，逐一拿掉 🟡 标记。
