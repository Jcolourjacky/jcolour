# 专利申请草案 — A2：打印验证失败的校准阶段自动定位与定向重校

> **状态** 草案 v0.1（2026-06）· **⚠ 非法律意见**，供专利代理人/律师定稿。
> **申请路线建议** 同 A1：先 CN 申请或 US provisional 占位，12 个月内 PCT 进多国；CN 说明书为母本，US 说明书为其英文译本。
> **商业秘密边界**（见 [`ip-strategy.md`](ip-strategy.md)）：权利要求与说明书**仅以功能性术语**描述「失败签名 → 阶段」的映射结构；**具体度量阈值、判别权重、分类器参数一律不写入**，作为商业秘密保留。
> **IDS / 在先技术**：US8873102（HP，机上动态分离）、US9373063（Onyx）、US8054504（Agfa）、通用闭环色彩验证/ΔE 测量文献、G7 / ISO 12647 灰平衡规范。

---

## 第一部分 · 说明书（中文母本）

### 发明名称
一种多阶段打印校准的验证失败自动定位与定向重校方法、系统及存储介质
（A method, system and storage medium for automatically localizing a print-verification failure to a calibration stage and directing recalibration）

### 技术领域
本发明涉及打印校准与闭环色彩管理技术领域，具体涉及一种在由多个有序校准阶段构成的打印校准流程中，依据打印验证的测量结果自动将验证失败归因到具体某一校准阶段并引导定向重校的方法、系统及计算机可读存储介质。

### 背景技术
[0001] 高质量打印校准通常由多个有序阶段串联构成，例如：总墨量限制（TIL）、逐通道线性化、灰平衡（如 G7）、输出颜色配置文件（ICC）生成等；出片期还包括渲染意图/扩展色域调用等环节。各阶段相互耦合且有先后依赖——尤其逐通道线性化为设备地基、须先于灰平衡，灰平衡建立于已线性化的各通道之上。

[0002] 完成校准后，通常打印一验证目标并以测量仪器（如分光光度计）测量色差（如 ΔE2000）以判断是否合格。

[0003] 现有方案在验证不合格时，一般仅给出合格/不合格或总体色差，**不能指出失败由哪一校准阶段引起**。操作员因而只能**重做全部阶段或凭经验猜测**，导致重复打印与测量、浪费介质/油墨/时间，且定位不确定。

[0004] 因此，需要一种能依据验证测量结果，自动将失败归因到具体某一校准阶段并引导只重做该阶段的技术方案。

### 发明内容
[0005] **要解决的技术问题**：在多阶段打印校准中，依据验证测量结果自动定位失败所属的校准阶段，并引导仅对该阶段定向重校，从而避免全量重校、缩短校准周期、节省介质与油墨。

[0006] **技术方案**：本发明提供一种多阶段打印校准的验证失败定位方法，所述校准由多个有序校准阶段构成，所述方法包括：

- S210 打印一验证目标并对其测量，得到测量数据；
- S220 由所述测量数据计算多个分阶段诊断度量，每一所述诊断度量与所述多个有序校准阶段中之一相关联；
- S230 当所述测量数据不满足验证合格判据时，依据所述多个诊断度量构成的失败签名，将所述失败归因到所述多个有序校准阶段中的一个被识别阶段；
- S240 引导对所述被识别阶段进行重校而不重校其余阶段；
- S250 重校后对所述被识别阶段对应项重新验证，构成闭环，直至满足合格判据，从而打印形成满足判据的实体印张。

[0007] 优选地，所述多个有序校准阶段包括墨量限制阶段、逐通道线性化阶段、灰平衡阶段与输出颜色配置文件阶段中的至少两个。

[0008] 优选地，所述失败签名到阶段的归因包括：中性轴色度偏移显著归因于灰平衡阶段；某单通道密度响应非线性归因于逐通道线性化阶段；总体色差偏大或配置文件偏色归因于输出颜色配置文件阶段。

[0009] 优选地，所述验证目标包括中性灰阶、单通道阶调、整体色块与扩展色相梯度中的至少两者，所述诊断度量相应地包括中性轴色度偏移、单通道线性度与总体色差中的至少两者。

[0010] 优选地，所述归因由将所述失败签名映射至阶段的决策结构执行，或由经训练的分类器执行。

[0011] 优选地，所述引导包括呈现指向所述被识别阶段的一键导航控件。

[0012] 优选地，所述诊断度量还包括扩展色域油墨的实际覆盖相对预期的偏差，且该偏差显著时归因于出片期的渲染意图/扩展色域调用阶段或验证目标覆盖。

[0013] 优选地，所述方法针对多种打印模式分别执行，各打印模式具有不同的校准深度。

[0014] 本发明还提供相应系统及计算机可读存储介质（见权利要求）。

[0015] **有益效果**：(1) 将重校从「全量重做」缩减为「仅重做被识别的单一阶段」，大幅节省介质、油墨与时间；(2) 定位确定、可复现，降低对操作员经验的依赖；(3) 重校后仅重验该项，构成高效闭环。

### 附图说明
> 线稿草图见 [`figures/patent-A2-figures.svg`](figures/patent-A2-figures.svg)（黑线白底+参考标号，供代理人照画正式图；每个 FIG 应单独成一张正式图纸）。

[0016]
- 图 1 为本发明系统的结构框图；
- 图 2 为多阶段校准流程与本发明定位—定向重校闭环的总体示意图；
- 图 3 为本发明方法的流程图；
- 图 4 为失败签名到校准阶段的映射结构示意图；
- 图 5 为引导界面（被识别阶段 + 一键导航）的示意图。

### 具体实施方式
[0017] 参见图 1 与图 2，系统 100 包括处理器 110、存储器 120、显示装置 130、打印机 140 与测量仪器 150。校准流程包含有序阶段：墨量限制阶段 410、逐通道线性化阶段 420、灰平衡阶段 430、输出颜色配置文件阶段 440，以及出片期的渲染意图/扩展色域调用阶段 450（建档时线性化阶段 420 先于灰平衡阶段 430；列印套用时则相反，灰平衡曲线先作用、线性化 LUT 作为最后一道设备侧校正，两者互为反向）。

[0018] 参见图 3。S210 经打印机 140 打印验证目标 200（含中性灰阶 201、单通道阶调 202、整体色块 203、扩展色相梯度 204），由仪器 150 测量得到测量数据 210。

[0019] S220 计算分阶段诊断度量 220：中性轴色度偏移 221（与灰平衡阶段 430 关联）、单通道密度线性度 222（与线性化阶段 420 关联）、总体/记忆色色差 223（与阶段 440 关联）、扩展油墨覆盖偏差 224（与阶段 450 关联）等。**具体度量的阈值、权重为实现细节，本说明书不限定其取值。**

[0020] S230 当测量数据 210 不满足合格判据时，由失败签名（即诸度量 220 的取值组合）经映射结构 300（图 4）归因到被识别阶段 310。例如：中性轴色度偏移 221 显著 → 灰平衡阶段 430；某单通道线性度 222 异常 → 线性化阶段 420；总体色差 223 偏大/偏色 → 配置文件阶段 440（并提示核查墨量限制阶段 410）；扩展油墨覆盖偏差 224 显著 → 阶段 450 或重测目标覆盖；若发生光源切换变色（同色异谱），则归因于多光谱处理模块。

[0021] 参见图 5。S240 经显示装置 130 呈现被识别阶段 310 及指向该阶段的一键导航控件 320，引导操作员仅对该阶段重校。S250 重校后仅对被识别阶段对应项重新验证，若仍不合格则回到 S230，构成闭环，直至合格并打印形成实体印张。

[0022] 在一实施例中，映射结构 300 为将失败签名映射至阶段的决策表；在另一实施例中由经训练的分类器实现，以测量数据/诊断度量为输入、以阶段标识为输出。

[0023] 在一实施例中，所述方法针对单黑、黑加灰、黑加灰加彩、RGB、16 位 RGB 等多种打印模式分别执行，各模式校准深度不同（例如单黑/黑加灰仅线性化、黑加灰加彩加灰平衡、RGB 类加配置文件），相应地按其包含的阶段进行定位。

[0024] 上述实施例可任意组合，所属领域技术人员可作出变型，例如度量与阶段集合的增减、自动重做替代提示、扩展至走纸/喷头等前置阶段。本发明保护范围以权利要求为准。

---

## 第二部分 · 权利要求书（中文，用于中国申请）

**1.** 一种多阶段打印校准的验证失败定位方法，所述校准由多个有序校准阶段构成，其特征在于，所述方法包括：
打印一验证目标并对其测量，得到测量数据；
由所述测量数据计算多个分阶段诊断度量，每一所述诊断度量与所述多个有序校准阶段中之一相关联；
当所述测量数据不满足验证合格判据时，依据由所述多个诊断度量构成的失败签名，将所述失败归因到所述多个有序校准阶段中的一个被识别阶段；
引导对所述被识别阶段进行重校而不重校其余阶段；以及
重校后对所述被识别阶段对应项重新验证，直至满足所述合格判据并打印形成实体印张。

**2.** 根据权利要求 1 所述的方法，其特征在于，所述多个有序校准阶段包括墨量限制阶段、逐通道线性化阶段、灰平衡阶段与输出颜色配置文件阶段中的至少两个。

**3.** 根据权利要求 2 所述的方法，其特征在于，所述归因包括：中性轴色度偏移显著时归因于所述灰平衡阶段；单通道密度响应非线性时归因于所述逐通道线性化阶段；总体色差偏大或配置文件偏色时归因于所述输出颜色配置文件阶段。

**4.** 根据权利要求 1 所述的方法，其特征在于，所述验证目标包括中性灰阶、单通道阶调、整体色块与扩展色相梯度中的至少两者，所述多个诊断度量相应地包括中性轴色度偏移、单通道线性度与总体色差中的至少两者。

**5.** 根据权利要求 1 所述的方法，其特征在于，所述归因由将所述失败签名映射至阶段的决策结构执行，或由经训练的分类器执行。

**6.** 根据权利要求 1 所述的方法，其特征在于，所述引导包括呈现指向所述被识别阶段的一键导航控件。

**7.** 根据权利要求 1 所述的方法，其特征在于，所述多个诊断度量还包括扩展色域油墨的实际覆盖相对预期的偏差，并在该偏差显著时归因于出片期的渲染意图或扩展色域调用阶段。

**8.** 根据权利要求 1 所述的方法，其特征在于，所述方法针对多种具有不同校准深度的打印模式分别执行。

**9.** 一种打印校准验证系统，其特征在于，包括处理器、显示装置、测量接口以及存储有指令的存储器，所述指令被所述处理器执行时实现权利要求 1 至 8 中任一项所述的方法。

**10.** 一种计算机可读存储介质，其上存储有计算机程序，其特征在于，所述程序被处理器执行时实现权利要求 1 至 8 中任一项所述的方法。

---

## 第三部分 · Claims（English, for US filing）

**1.** A method of verifying a printer calibrated by a pipeline having a plurality of ordered calibration stages, the method comprising:
printing a verification target and measuring the verification target to obtain measurement data;
computing, from the measurement data, a plurality of stage-diagnostic metrics, each stage-diagnostic metric being associated with a respective one of the plurality of ordered calibration stages;
when the measurement data fails a verification criterion, localizing the failure to an identified one of the plurality of ordered calibration stages based on a signature formed by the plurality of stage-diagnostic metrics;
directing recalibration of the identified stage without recalibrating others of the plurality of ordered calibration stages; and
after the recalibration, re-verifying the identified stage until the verification criterion is met, and printing to produce a physical print meeting the criterion.

**2.** The method of claim 1, wherein the plurality of ordered calibration stages comprises at least two of an ink-limit stage, a per-channel linearization stage, a gray-balance stage, and an output color profile stage.

**3.** The method of claim 2, wherein the localizing comprises: attributing a significant chromaticity drift of a neutral axis to the gray-balance stage; attributing a non-linear single-channel density response to the per-channel linearization stage; and attributing an excessive overall color difference or a profile color bias to the output color profile stage.

**4.** The method of claim 1, wherein the verification target comprises at least two of a neutral gray ramp, single-channel tone steps, overall color patches, and an extended-hue gradient, and the plurality of stage-diagnostic metrics correspondingly comprises at least two of a neutral-axis chromaticity drift, a single-channel linearity, and an overall color difference.

**5.** The method of claim 1, wherein the localizing is performed by a decision structure mapping the signature to a stage, or by a trained classifier.

**6.** The method of claim 1, wherein the directing comprises presenting a one-tap navigation control to the identified stage.

**7.** The method of claim 1, wherein the plurality of stage-diagnostic metrics further comprises a deviation of actual coverage of an extended-gamut ink from an expected coverage, and a significant said deviation is localized to a rendering-intent or extended-gamut-recruitment stage.

**8.** The method of claim 1, performed separately for each of a plurality of print modes having different calibration depths.

**9.** A print-calibration verification system comprising a processor, a display, a measurement interface, and memory storing instructions that, when executed by the processor, cause the system to perform the method of any one of claims 1–8.

**10.** A non-transitory computer-readable medium storing instructions that, when executed by a processor, cause the processor to perform the method of any one of claims 1–8.

---

## 第四部分 · 摘要 / Abstract

**说明书摘要（中文，≤300 字，附图 4）**
本发明公开一种多阶段打印校准的验证失败自动定位与定向重校方法、系统及存储介质。校准由墨量限制、逐通道线性化、灰平衡、输出配置文件等有序阶段构成。打印并测量验证目标后，计算各与某一阶段相关联的诊断度量；当不合格时，依据诸度量构成的失败签名将失败归因到具体某一被识别阶段，引导仅重做该阶段而不重做其余阶段，重校后仅重验该项，构成闭环，直至合格并打印形成实体印张。本发明将重校从全量重做缩减为单阶段重做，节省介质、油墨与时间，定位确定可复现。（指定图 4 为摘要附图）

**Abstract (English, ≤150 words)**
A method, system and medium localize a print-verification failure to a specific stage of a multi-stage calibration pipeline. After printing and measuring a verification target, a plurality of stage-diagnostic metrics—each associated with a respective calibration stage such as ink-limit, per-channel linearization, gray-balance, or output profile—are computed. When the measurement fails a criterion, the failure is localized to one identified stage from a signature of the metrics (e.g., neutral-axis drift→gray-balance; single-channel non-linearity→linearization; overall ΔE bias→profile). The system directs recalibration of only the identified stage, then re-verifies that stage in a closed loop until the criterion is met and a conforming physical print is produced. Recalibration is thereby reduced from redoing all stages to redoing one, saving media, ink and time.

---

## 第五部分 · 中美申请要点（交代理人）
- **客体适格**：方法收束于「打印形成满足判据的实体印张」+ 测量/重校物理过程 → 满足美国 §101 与中国可专利客体。
- **新颖性区隔**：核心在「失败签名 → 具体校准【阶段】的自动归因 + 仅定向重做该阶段」，区别于通用「测 ΔE 给合格/不合格」（无阶段定位）与人工全量重校。
- **商业秘密**：诊断度量阈值、判别权重、分类器参数不写入；权项以「失败签名」「映射结构/分类器」等功能性术语表述。
- **可举证性**：失败时产品「告知具体阶段 + 一键跳转 + 仅重做该阶段」的可观察行为即被主张方法。
- **附图**：图 4 须含诊断度量 221–224 → 映射结构 300 → 被识别阶段 310；图 5 须含一键导航控件 320。
- **单一性 / 优先权**：方法、系统、介质共享同一构思；CN/US 家族保持说明书一致。
