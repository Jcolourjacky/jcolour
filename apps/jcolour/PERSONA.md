# jcolour 用户画像

> jcolour v1.0 服务**唯一一种用户**：摄影师打印自己的照片。
> 一切功能、UI、营销都围绕这个画像。任何被画像排除的需求一律不做（属于 gcolour 或 v1.5+）。

---

## 一、画像速写（Primary Persona）

```
代号：     "Sam, the printing photographer"
身份：     独立艺术摄影师 / 半专业摄影爱好者
年龄段：    32–58
性别比：    男性偏多（约 70%），女性增长中
地理：     北美 + 西欧 + 东亚（日本/韩国/中国大陆/港台）核心区
工作模式：  自由职业 / 副业 / 退休再就业
设备：     Mac mini / MacBook Pro M1+M2+M3 + 一台 Epson SureColor
打印机：    P700 / P900（桌面）或 P7500 / P9500（大幅面）
测色仪：    一半人有 i1Pro3 / ColorMunki / SpyderPrint，另一半没有
预算：     打印机本身 $1,500–$15,000，纸+墨年支出 $500–$5,000
软件栈：    Lightroom Classic + Photoshop（80%）/ Capture One（20%）
```

---

## 二、Sam 的一天（Day in the Life）

```
07:30  早咖啡，看昨天打印的样张干透后的颜色 — 哑光纸暗部偏品红，又得调
09:00  Lightroom 里挑出 8 张限量版要打的作品，开始软打样
10:00  Photoshop 打开第一张，配置 Hahnemühle Photo Rag profile，渲染意图选 perceptual
10:15  打第一张 1:1 区域测试样在 A3 纸上
10:35  样张出来了 — 高光偏绿了一点点，软打样上看不出
10:36  回 Photoshop 加曲线 → 重新打 → 又错 → 再调...（循环 30 分钟）
11:30  终于通过测试。开始正打 24"×36" 限量版第 23/50 张
12:30  正打完成，签字编号。Photoshop 文件名手动改成 "20260501-bay-bridge-23of50.psd"
14:00  下一张作品，Canson Baryta 纸。Epson 驱动从 MK 切到 PK，等了 4 分钟，浪费 30ml 墨
14:10  发现保存的"Canson Baryta 预设"打开后渲染意图变成 relative 了 — Epson Print Layout 又抽风
14:30  正打。完成。
17:00  打卡日记：今天废了 4 张 A3 测试 + 1 张 A2 正打错色 = 损失约 480 元 + 2 小时
        明天还要打 6 张。
```

**Sam 一天的痛点出现在每一个时段**。jcolour 的目标：把这一天压缩到 4 小时，废纸量压到 ≤ 1 张。

---

## 三、Sam 的产出形态

### 主要产品
| 类别 | 占比 | 典型尺寸 | 典型纸张 | 典型用途 |
|------|------|---------|---------|---------|
| **限量版作品** | 40% | A2–A0 (16"×20"–44"×60") | Hahnemühle Photo Rag / Canson Baryta | 自有网站、画廊寄售、艺术博览会 |
| **个人存档** | 25% | A3–A2 | Hahnemühle FineArt / Moab Entrada | 自留、家庭装饰 |
| **画册/Portfolio** | 15% | A4 | Hahnemühle PhotoRag Book | 向画廊投稿、客户提案 |
| **客户定制（少量）** | 10% | 多变 | 多变 | 收入补充 |
| **测试/学习** | 10% | A4–A3 | 廉价光面纸 | 调色、学习 |

**特别注意**：限量版 + 个人存档 = 65%，**这是 jcolour 真正服务的核心场景**。

### 不属于 Sam（明确排除）
- ❌ 婚礼相册、儿童写真等批量影楼业务（属于商业摄影服务）
- ❌ 帮其他艺术家代客打印（属于打印店服务，gcolour 范围）
- ❌ 大量打 50×100 cm 商业广告/陈列（属于精品喷绘服务，v1.5+）
- ❌ 印前打样（属于 gcolour）
- ❌ 标牌喷绘（不是 jcolour 用户）

---

## 四、Sam 的工具链与工作流

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Capture (相机) │──→│ Lightroom 调色 │──→│ Photoshop 局部 │
└──────────────┘     └──────────────┘     └───────┬──────┘
                                                   │
                                                   ▼
                              ┌─────────────────────────────────┐
                              │ 当前痛点：Epson Print Layout / PS │
                              │ 打印对话框 / Lightroom 打印模块   │
                              │ 三选一，每个都半残                │
                              └────────────────┬────────────────┘
                                               │
                                               ▼
                                  ┌─────────────────────┐
                                  │  jcolour 介入点 ★   │
                                  │  从 LR/PS 一键发送  │
                                  └─────────────────────┘
```

**关键洞察**：Sam **不会换 Lightroom/Photoshop/Capture One**。jcolour 不是图像编辑器，是**接管"图片 → 打印机"这最后一公里**的色彩工作流工具。

---

## 五、Sam 的痛点优先级

按出现频率 × 经济损失排序：

| Rank | 痛点 | 频率 | 单次损失 | jcolour 对策 |
|------|------|------|---------|------------|
| 1 | 软打样和实印对不上（哑光纸最严重） | 每次打印 | 50–300 元/废张 | 哑光纸纸白模拟 + ΔE 警告 |
| 2 | Epson Print Layout / 打印对话框预设丢失 | 每周 1–2 次 | 重新设置 5 分钟 + 错印 | 命名预设强绑定，绕开系统对话框 |
| 3 | 黑白照偏色，ABW 4 个预设不够 | 每张 B&W | 反复试印 1–3 张 | B&W 双模式（单黑 + 多色黑） |
| 4 | macOS 升级后驱动崩溃 / ColorSync 双管理 | 每次系统升级 | 几天工作流瘫痪 | PJL 直连，绕开 macOS 打印系统 |
| 5 | 限量版编号靠手动文件名管理 | 每次正打 | 偶尔记错版次 | 内置 Edition Tracking |
| 6 | 多张多纸不同设置，反复切换 | 每次批量 | 30 分钟设置时间 | 批量队列 + 自动分组 |
| 7 | 哑光纸暗部偏品红、亮光纸高光偏绿 | 长期 | 慢性废纸 | Per-ink 线性化曲线编辑器 |
| 8 | Hahnemühle/Canson profile 找不到或装不对 | 偶尔 | 1 小时折腾 | 内置 50+ 纸张 profile 一键安装 |
| 9 | i1Pro3 测了不知道怎么生成 ICC | 校准时 | 1–3 小时学习 + 失败 | 测靶 → 上云 → 一键回包 |
| 10 | 一张作品打了多年记不清纸/设置 | 翻旧账 | 复打不一致 | 历史日志全可查 |

---

## 六、Sam 的决策路径（购买动机）

### 触发购买的事件（按概率）
1. **macOS 系统升级后老 RIP 崩溃** — ImagePrint/Mirage 用户切换的最大单一触发点
2. **新买了 P900 / P7500 / P9500** — 想用满色域，发现 Epson Print Layout 不够
3. **看到 Reddit / DPReview / Luminous Landscape 推荐** — 朋辈传播
4. **YouTube 上摄影师评测视频** — 关键词：B&W、哑光纸、Apple Silicon Native
5. **客户投诉颜色不对** — 决定花钱买好工具

### 决策时长
- 从听说到下载试用：**1–7 天**
- 从试用到付费：**7–30 天**（订阅决策慢于一次性买断）
- 关键转化点：**第一次成功打出无废纸 B&W**

### 价格敏感度
- $99/年 → "不假思索就买"（≤ 一张废纸成本）
- $199/年 → "比较一下竞品，多半会买"（jcolour 主档定价）
- $299/年 → "犹豫，要看是否真比 ImagePrint 强"
- $399/年 → "Pro 档可以接受，前提是 v1.5 多色黑 + 主体保护 AI 真好用"

---

## 七、Sam 不是什么人

明确排除以避免功能膨胀：

| 误判画像 | 为什么不是 jcolour 目标 | 应去的产品 |
|---------|----------------------|-----------|
| **画廊/打印店老板** | 服务别人，需要用户管理、批量发货、计价 | gcolour 或 v1.5+ |
| **婚礼摄影师卖相册** | 商业出片业务，重在批量速度而非单张色彩极致 | 第三方影楼软件 |
| **印前打样师傅** | 对标 GRACoL/Fogra，要 CMYK 工作流 | gcolour |
| **品牌色彩部门** | 专色、Pantone、跨厂协同 | gcolour 或 GMG 级软件 |
| **大幅面广告喷绘** | 要 60"+、要 ONYX 级速度、要 SC-S/T 系列机器 | ONYX/Caldera |
| **Mutoh 标牌打印工** | 生态溶剂、户外耐久、SignSavvy 用户 | Wasatch/Caldera |
| **手机/家用打印用户** | EcoTank、桌面 6 色机，要简单 | Epson Photo+ / Apple Photos |

---

## 八、Sam 的语言与文化

营销文案、UI 文案、社区互动都要讲 Sam 的语言：

- 喜欢说："**giclée**"、"**fine art print**"、"**limited edition**"、"**archival**"、"**Dmax**"、"**neutral grays**"、"**OBA-free**"（无荧光增白剂）
- 喜欢的纸张品牌词："**Hahnemühle Photo Rag**"、"**Canson Infinity Baryta**"、"**Moab Entrada Rag**"
- 标志性反对："**oversaturated**"、"**color cast**"、"**bronzing**"、"**metamerism**"、"**banding**"
- 喜欢的标杆："**Ansel Adams Zone System**"、"**Cone Editions**"、"**West Coast Imaging**"
- 反对的："**Adobe 强迫订阅**"、"**Rosetta 跑得慢**"、"**Windows 软件**"、"**塑料感的 saturation slider**"

UI 默认语言可以双语，但**调色相关术语应使用英文行业惯用词**（perceptual / relative colorimetric / soft proofing 等），中文文案可以加在副标题里。

---

## 九、相关文档

- 调色痛点详解：[`EPSON-COLOR-PROBLEMS.md`](EPSON-COLOR-PROBLEMS.md)
- 黑白工作流设计：[`BW-WORKFLOW.md`](BW-WORKFLOW.md)
- 竞品对标：[`COMPETITORS.md`](COMPETITORS.md)
- v1.0 范围：[`PLAN.md`](PLAN.md)
- 路线图：[`ROADMAP.md`](ROADMAP.md)
