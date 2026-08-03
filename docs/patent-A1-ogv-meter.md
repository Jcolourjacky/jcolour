# 专利申请草案 — A1：扩展色域油墨调用率仪表与建议回路

> **状态** 草案 v0.1（2026-06）· **⚠ 非法律意见**，供专利代理人/律师定稿。
> **申请路线建议** 先递交中国发明专利（或美国临时申请 provisional 占位），12 个月内走 PCT / 巴黎公约进美国。本文＝共用技术披露（中文说明书为母本，美国申请之说明书为其英文译本）＋中/英两套权利要求＋摘要＋附图说明。
> **商业秘密边界**（见 [`ip-strategy.md`](ip-strategy.md)）：权利要求与说明书**仅以功能性术语**描述「色相相关的可达性上限」「超出量比例」等；**具体色相带中心/半宽、CMYK 可达彩度上限数值、各墨头空间、点火/建议阈值、分类器参数一律不写入**，作为商业秘密保留。
> **IDS / 在先技术**（须在 IDS 中列明）：US12034903（Fiery，语义分割→离散专色）、US8873102（HP，机上动态分离）、US9373063（Onyx，PVR 同色相拆分）、US8054504（Agfa）、学术 Image-Dependent Gamut Mapping（Morovic 等）、Neugebauer / Yule-Nielsen 前向模型。

---

## 第一部分 · 说明书（中文母本）

### 发明名称
一种扩展色域打印机的逐图像油墨调用率提示方法、系统及存储介质
（A per-image extended-gamut ink recruitment metering method, system and storage medium）

### 技术领域
本发明涉及彩色打印与色彩管理技术领域，具体涉及一种在光栅图像处理器（RIP）中，针对具有扩展色域油墨（如橙、绿、紫中的至少一种）的打印机，于出片前依据图像内容估计并提示各扩展色域油墨调用程度的方法、系统及计算机可读存储介质。

### 背景技术
[0001] 为扩大打印色域，业界在传统青（C）、品红（M）、黄（Y）、黑（K）四色基础上增设橙（Or）、绿（Gr）、紫（V）等扩展色域油墨（统称 O/G/V），以覆盖 CMYK 在橙、绿、紫高饱和区的色域缺口。然而扩展色域油墨增加了介质与油墨成本。

[0002] 现有 RIP 的颜色分离与输出配置文件通常**与图像内容无关**：无论待打印图像是否落入扩展色域，分离策略保持不变。操作员因而**无法在出片前获知某一具体图像是否实际用到扩展色域油墨**。

[0003] 实践中，操作员常因源图像色域不足（例如误用较小色域的颜色空间）或渲染意图选择不当，导致扩展色域形同虚设，却**缺乏任何反馈**；待打印后才发现，已浪费介质、油墨与时间，且事后难以归因。

[0004] 已知一些方案对图像做语义分割并为对象指派**离散专色**（品牌色），或在数字印刷机上依实际安装油墨**动态构建分离**；前者面向离散专色而非连续的扩展色域油墨调用，后者亦未在出片前向操作员提示逐图像的扩展油墨调用程度并据以引导渲染意图的重新选择。

[0005] 因此，需要一种能在出片前、按图像内容估计并直观呈现各扩展色域油墨调用程度，并据以引导操作员调整的技术方案。

### 发明内容
[0006] **要解决的技术问题**：在出片前，针对具体图像内容，估计并呈现各扩展色域油墨（O/G/V）的调用程度，使操作员据以判断扩展色域是否被有效利用，并在利用不足时获得引导，从而避免无效打印、节省介质与油墨。

[0007] **技术方案**：本发明提供一种扩展色域打印机的逐图像油墨调用率提示方法，所述打印机具有 C、M、Y、K 及橙、绿、紫中的至少一种扩展色域油墨，所述方法包括：

- S110 获取源图像；
- S120 分析所述源图像的内容，针对所述至少一种扩展色域油墨中的每一种，计算一调用率度量，所述调用率度量指示打印所述源图像将使用该扩展色域油墨的程度；其中所述分析包括：将所述源图像的像素按色相与彩度，相对于 C、M、Y、K 子集在相应色相处的**可达性上限**进行分类，并对落入与该扩展色域油墨相关联的色相范围内、且彩度超过所述可达性上限的像素，按其**超出量**累计对该调用率度量的贡献；
- S130 经由显示装置呈现每一所述扩展色域油墨的所述调用率度量；
- S140 当所述调用率度量满足预设条件时，呈现一建议及一用于重新选择渲染意图的可操作项；
- S150 响应于渲染意图的重新选择，重新计算并重新呈现所述调用率度量；
- S160 由所述打印机打印所述源图像，从而将油墨沉积于承印物上形成实体印张。

[0008] 优选地，所述调用率度量包括该扩展色域油墨在整幅图像上的平均调用覆盖率与触发该油墨的像素比例中的至少一者。

[0009] 优选地，所述可达性上限依据所述打印机的输出颜色配置文件（output ICC profile）所表征的 C、M、Y、K 子集色域边界确定。

[0010] 优选地，所述预设条件包括所述调用率度量低于一下阈值；所述建议指示扩展色域基本未被利用并提示重新选择渲染意图或更换源图像。

[0011] 优选地，所述呈现包括为每一扩展色域油墨显示一仪表及其历史曲线。

[0012] 优选地，所述方法进一步包括对所述源图像进行语义分割得到多个区域，并针对每一区域分别计算所述调用率度量。

[0013] 优选地，所述调用率度量替代地或附加地由分离后的设备侧光栅数据统计得到。

[0014] 本发明还提供相应的系统及计算机可读存储介质（见权利要求）。

[0015] **有益效果**：(1) 操作员在出片前即可直观获知扩展色域油墨的利用程度，量化扩展色域投入的回报；(2) 在源图像色域不足或渲染意图不当时及时获得引导并可即时重估，避免无效打印，节省介质、油墨与时间；(3) 提示与重选构成闭环，与软打样决策衔接。

### 附图说明
> 线稿草图见 [`figures/patent-A1-figures.svg`](figures/patent-A1-figures.svg)（黑线白底+参考标号，供代理人照画正式图；每个 FIG 应单独成一张正式图纸）。

[0016]
- 图 1 为本发明系统的结构框图；
- 图 2 为本发明方法的流程图；
- 图 3 为调用率仪表与建议条的界面示意图；
- 图 4 为色相—可达性上限及超出量判定的示意图；
- 图 5 为提示—重选—重估闭环的示意图。

### 具体实施方式
[0017] 参见图 1，系统 100 包括处理器 110、存储器 120、显示装置 130 与扩展色域打印机 140，打印机 140 具有打印头 142 及 C、M、Y、K 与橙、绿、紫扩展色域油墨。存储器 120 存有指令，由处理器 110 执行以实现下述方法。

[0018] 参见图 2 及图 4。S110 获取源图像 200。S120 将图像 200 转换至与设备无关的颜色空间得到各像素 210 的明度—色度表示（如 Lab），对每一像素求色相 h 与彩度 C。对每一扩展色域油墨，定义一与之关联的色相范围 320 及该色相处 C、M、Y、K 子集的可达性上限 330；当像素彩度 C 超过上限 330 且色相落入范围 320 时，判定该像素需要该扩展色域油墨，其贡献正比于超出量 340（即 C 超过 330 的量相对于该油墨头空间的比例），并可按色相隶属度加权。**所述色相范围、上限、头空间的具体取值为实现细节，可由实施者依油墨/介质标定确定，本说明书不限定其具体数值。**

[0019] 累计得到每一扩展色域油墨的调用率度量 350，其可包括平均调用覆盖率、触发像素比例，以及任一扩展色域油墨被调用的像素比例。

[0020] 参见图 3。经由显示装置 130 呈现三个仪表 361、362、363 分别对应橙、绿、紫的调用率度量 350，并可附历史曲线 365。当度量 350 满足预设条件（如峰值低于下阈值）时，呈现建议条 370（例如「扩展色基本未用，建议重选渲染意图/更换源文件」）及可操作项 380。

[0021] 参见图 5。响应于经由可操作项 380 对渲染意图的重新选择，返回 S120 重新计算并经 S130 重新呈现度量 350，形成闭环。最终 S160 以所选渲染意图驱动打印头 142 将油墨沉积于承印物，形成实体印张。

[0022] 在一实施例中，可达性上限 330 由打印机 140 的输出颜色配置文件所表征的 C、M、Y、K 子集色域边界导出；落于该边界之外而处于含扩展色域油墨的全色域之内的像素被计入相应扩展色域油墨的调用。

[0023] 在一实施例中，对图像 200 进行语义分割得到肤色、天空、植被、主体、背景等区域，对每一区域分别计算并呈现调用率度量，并可分区呈现建议。

[0024] 在一实施例中，调用率度量 350 替代地由所述源图像经颜色分离后的设备侧多通道光栅数据按通道统计实际覆盖率得到，作为出片前估计的校验或替代。

[0025] 上述实施例可任意组合。所属领域技术人员可在不脱离本发明范围内作出变型，例如：扩展色域油墨包括红、蓝等其他附加色；阈值与建议文案可配置；度量以批次方式统计。本发明的保护范围以权利要求为准。

---

## 第二部分 · 权利要求书（中文，用于中国申请）

**1.** 一种扩展色域打印机的逐图像油墨调用率提示方法，所述打印机具有青、品红、黄、黑油墨以及橙、绿、紫中的至少一种扩展色域油墨，其特征在于，所述方法包括：
获取源图像；
分析所述源图像的内容，针对所述至少一种扩展色域油墨中的每一种，计算指示打印所述源图像将使用该扩展色域油墨之程度的调用率度量；其中所述分析包括：将所述源图像的像素按色相与彩度，相对于青、品红、黄、黑子集在相应色相处的可达性上限进行分类，并对落入与该扩展色域油墨相关联的色相范围内且彩度超过所述可达性上限的像素，按其超出量累计对所述调用率度量的贡献；
经由显示装置呈现每一所述扩展色域油墨的所述调用率度量；
当所述调用率度量满足预设条件时，呈现建议以及用于重新选择渲染意图的可操作项；
响应于渲染意图的重新选择，重新计算并重新呈现所述调用率度量；以及
由所述打印机打印所述源图像，将油墨沉积于承印物上形成实体印张。

**2.** 根据权利要求 1 所述的方法，其特征在于，所述调用率度量包括该扩展色域油墨在整幅图像上的平均调用覆盖率与触发该扩展色域油墨的像素比例中的至少一者。

**3.** 根据权利要求 1 所述的方法，其特征在于，所述可达性上限依据所述打印机的输出颜色配置文件所表征的青、品红、黄、黑子集的色域边界确定。

**4.** 根据权利要求 1 所述的方法，其特征在于，所述预设条件包括所述调用率度量低于下阈值，且所述建议指示扩展色域未被有效利用并提示重新选择渲染意图或更换源图像。

**5.** 根据权利要求 1 所述的方法，其特征在于，所述呈现包括为每一所述扩展色域油墨显示一仪表及其历史曲线。

**6.** 根据权利要求 1 所述的方法，其特征在于，还包括对所述源图像进行语义分割得到多个区域，并针对每一所述区域分别计算所述调用率度量。

**7.** 根据权利要求 1 所述的方法，其特征在于，所述调用率度量还由所述源图像经颜色分离后的设备侧多通道光栅数据按通道统计的实际覆盖率得到。

**8.** 根据权利要求 1 所述的方法，其特征在于，所述分析中对像素的所述贡献还按该像素色相相对于所述色相范围的隶属度加权。

**9.** 一种逐图像油墨调用率提示系统，其特征在于，包括处理器、显示装置以及存储有指令的存储器，所述指令被所述处理器执行时实现权利要求 1 至 8 中任一项所述的方法。

**10.** 一种计算机可读存储介质，其上存储有计算机程序，其特征在于，所述程序被处理器执行时实现权利要求 1 至 8 中任一项所述的方法。

> CN 撰写注意：中国不允许多项从属权利要求作为另一多项从属权利要求的基础；上列从属项均直接引用权利要求 1，符合规定。

---

## 第三部分 · Claims（English, for US filing）

**1.** A method of operating an extended-gamut printer having cyan, magenta, yellow and black inks and at least one extended process ink among orange, green and violet, the method comprising:
obtaining a source image;
analyzing content of the source image to compute, for each of the at least one extended process ink, a recruitment metric indicative of an amount of that extended process ink that printing the source image would use, wherein the analyzing comprises classifying pixels of the source image, by hue and chroma, relative to a hue-dependent reachability limit of a subset consisting of the cyan, magenta, yellow and black inks, and, for pixels that fall within a hue range associated with the extended process ink and have a chroma exceeding the reachability limit, accumulating a contribution to the recruitment metric in proportion to an amount by which the chroma exceeds the reachability limit;
presenting, on a display, the recruitment metric for each of the at least one extended process ink;
responsive to the recruitment metric satisfying a threshold condition, presenting an advisory and an affordance to reselect a rendering intent;
responsive to a reselection of the rendering intent, recomputing and re-presenting the recruitment metric; and
printing the source image with the printer to deposit ink on a substrate.

**2.** The method of claim 1, wherein the recruitment metric comprises at least one of a mean recruited coverage of the extended process ink over the source image and a fraction of pixels that recruit the extended process ink.

**3.** The method of claim 1, wherein the reachability limit is derived from a gamut boundary of said subset as characterized by an output color profile of the printer.

**4.** The method of claim 1, wherein the threshold condition comprises the recruitment metric being below a lower threshold, and the advisory indicates that the extended gamut is underused and prompts reselection of the rendering intent or replacement of the source image.

**5.** The method of claim 1, wherein presenting the recruitment metric comprises displaying, for each extended process ink, a meter and a history thereof.

**6.** The method of claim 1, further comprising performing semantic segmentation of the source image into a plurality of regions, and computing the recruitment metric separately for each region.

**7.** The method of claim 1, wherein the recruitment metric is further obtained from per-channel actual coverage of a device-side multi-channel raster produced by color-separating the source image.

**8.** The method of claim 1, wherein the contribution of a pixel is further weighted by a membership of a hue of the pixel within the hue range.

**9.** The method of claim 1, wherein the at least one extended process ink comprises orange, green and violet, and a respective recruitment metric is computed and displayed for each of orange, green and violet.

**10.** A raster image processing system comprising: a processor; a display; and memory storing instructions that, when executed by the processor, cause the system to perform the method of any one of claims 1–9.

**11.** A non-transitory computer-readable medium storing instructions that, when executed by a processor, cause the processor to perform the method of any one of claims 1–9.

> US 撰写注意：可将系统/介质权项展开为各自独立的多项以利侵权覆盖（直接侵权主体不同）；claim 1 已绑定「printing … to deposit ink on a substrate」物理步骤以满足 §101 适格性；避免 means-plus-function 表述以免触发 112(f)。

---

## 第四部分 · 摘要 / Abstract

**说明书摘要（中文，≤300 字，附图 3）**
本发明公开一种扩展色域打印机的逐图像油墨调用率提示方法、系统及存储介质。该打印机具有青品红黄黑及橙绿紫中至少一种扩展色域油墨。方法在出片前分析源图像内容：将像素按色相与彩度，相对于青品红黄黑子集在相应色相的可达性上限分类，对超过上限的像素按超出量累计，得到各扩展色域油墨的调用率度量并以仪表呈现；当度量满足预设条件时呈现建议及重选渲染意图的可操作项，重选后即时重估，构成闭环，最终打印形成实体印张。本发明使操作员在出片前直观获知扩展色域利用程度，避免无效打印，节省介质与油墨。（指定图 3 为摘要附图）

**Abstract (English, ≤150 words)**
A method, system and storage medium estimate, before printing, how much of each extended process ink (orange/green/violet) an extended-gamut printer would use for a given image. Pixels are classified by hue and chroma against a hue-dependent reachability limit of the CMYK subset; pixels exceeding the limit contribute, in proportion to the excess, to a per-ink recruitment metric shown as meters. When the metric meets a threshold (e.g., underuse), an advisory and an affordance to reselect a rendering intent are presented; the metric is recomputed upon reselection, forming a closed loop. The image is then printed, depositing ink on a substrate. Operators thereby see, per image, whether the extended gamut is used, avoiding wasted media and ink.

---

## 第五部分 · 中美申请要点（交代理人）
- **客体适格**：方法权项均以「打印…沉积油墨于承印物」收束于物理打印过程 → 满足美国 §101（避 Alice 抽象）与中国可专利客体（具技术特征的技术方案）。
- **新颖性区隔**：相对 US12034903（离散专色）强调「扩展色域【制程墨】的连续调用率」；相对 US8873102（机上动态分离）强调「出片前 per-image 提示 + 引导意图重选的闭环」。
- **商业秘密**：色相带/上限/阈值/分类器具体数值不写入；权项以功能性术语表述。
- **优先权/家族**：建议先 CN 申请或 US provisional 占位，12 个月内 PCT 进多国；保持 CN/US 说明书一致（US 为本母本英文译本）。
- **附图**：图 3 须含三仪表 361/362/363 + 历史曲线 365 + 建议条 370 + 可操作项 380；图 4 须含色相轴、可达性上限曲线 330、超出量 340。
- **单一性**：方法、系统、介质共享同一发明构思，满足单一性。
