<!-- ppt-master-schema: design-spec/v1 -->
# C-PI Defense - Design Spec

## I. Project Information
| Item | Value |
| --- | --- |
| Project Name | cpi_defense |
| Canvas Format | ppt169 1280x720 |
| Page Count | 10 |
| Primary Language | zh-CN |
| Target Audience | 工创赛智能物流运输赛道答辩评委 |
| Communication Intent | 解释机械与电控设计，展示实车基础，说明OPS定位和视觉优化计划及投入 |
| Desired Audience Outcome | 理解设计原理，认可实物基础，能评估后续预算、计划和预期成果 |
| Core Message / Ask / Action | 实车已搭建，电控已完成调试，正在推进OPS全场定位与视觉优化 |
| Delivery Context | 中文现场答辩，有主讲，16:9可编辑PPT |
| Artifact Afterlife | 校赛答辩、团队技术交流与后续更新 |
| Reading Mode | balanced |
| Content Strategy | 平衡改编旧材料，更新当前方案；真实照片和可编辑原理图并重 |
| Design Style | 工程证据，自由设计，白底深蓝青绿 |
| AI Image Acquisition Path | not applicable |
| Generation Mode | continuous |
| Spec Refinement | disabled |
| Speaker Notes | enabled — workflow default |
| Custom Animations | disabled — delegated decision |
| Narration Audio | disabled — delegated decision |
| Created Date | 2026-11-15 |

## II. Canvas Specification
| Property | Value |
| --- | --- |
| Format | PPT 16:9 |
| Dimensions | 1280 × 720 |
| viewBox | `0 0 1280 720` |
| Margins | 56px |
| Content Area | 1168 × 600 |

## III. Visual Theme
### Theme Style
- **Mode**: custom
- **Mode References**: instructional
- **Mode Behavior**: 先交代已有成果，再从整体分解机械、电控与感知，最后以投入、计划和预期效果收束。
- **Visual style**: custom
- **Visual Style References**: swiss-minimal
- **Visual Style Behavior**: 浅色大图页面与深蓝结论页面交替，严格对齐，细规则线标注结构，示意图保留独立可编辑对象，青绿只强调关键链路及当前工作。
- **Theme**: 工程证据
- **Tone**: 清晰、务实、自信
### Color Scheme
| Role | HEX | Purpose |
| --- | --- | --- |
| Background | #FFFFFF | 主画布 |
| Secondary background | #F1F5F9 | 次级区域 |
| Primary | #12304A | 标题与深色面 |
| Accent | #008C95 | 关键链路 |
| Secondary accent | #C88925 | 待验证与预算 |
| Body text | #203447 | 正文 |
| Secondary text | #596D7E | 注释 |
| Divider | #D7E1E8 | 规则线 |

## IV. Typography System
### Font Plan
| Role | Character (Reference) | Primary | English if non-English | Fallback tail |
| --- | --- | --- | --- | --- |
| Title | 精确无衬线、粗体 | Microsoft YaHei | Arial | sans-serif |
| Body | 清晰无衬线 | Microsoft YaHei | Arial | sans-serif |
- **Title stack**: Microsoft YaHei, Arial
- **Body stack**: Microsoft YaHei, Arial
### Font Size Hierarchy
| Purpose | Anchor Size (px) |
| --- | ---: |
| Body | 24 |
| Title | 40 |
| Subtitle | 30 |
| Annotation | 18 |
| Footnote | 14 |
| Display | 56 |

## V. Layout Principles
### Deck-wide Direction
- **Hierarchy direction**: 一句主张后用图或原理关系解释
- **Composition tendency**: 整车大图、结构分解、横向流程、双视图、数字预算交替
- **Cross-page continuity**: 深蓝标题、青绿短线、页脚与章节短标签
- **Spacing posture**: 机构页开放，电控与预算适度紧凑
- **Spacing anchors**: page margin 56; block gap 24; column gutter 32; corner radius 4; body leading 36

## VI. Icon Usage Specification
- **Primary bundled library**: none
| Icon Path | Suitable Scenarios |
| --- | --- |

## VIII. Image Resource List
| Filename | Dimensions | Ratio | Purpose | Type | Image pattern | Crop Policy | Acquire Via | Status | Reference | text_policy | page_role |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| mechanism_1.jpg | 1027x572 | 1.80 | 底盘承载结构 | CAD | 大幅完整结构图 | no-crop | user | Existing | 用户图1 | none | local |
| mechanism_2.jpg | 786x581 | 1.35 | 底部结构 | CAD | 底视图与说明并置 | no-crop | user | Existing | 用户图2 | none | local |
| mechanism_3.jpg | 658x502 | 1.31 | 定位轮安装 | CAD | 局部结构证据 | no-crop | user | Existing | 用户图3 | none | local |
| mechanism_5.jpg | 756x642 | 1.18 | 定位组件 | CAD | 大图说明安装关系 | no-crop | user | Existing | 用户图5 | none | local |
| mechanism_6.jpg | 800x752 | 1.06 | 整车总装 | CAD | 封面主视觉及结构总览 | no-crop | user | Existing | 用户图6 | none | local |
| mechanism_7.jpg | 595x550 | 1.08 | 抓取机构总装 | CAD | 总览配运动分解 | no-crop | user | Existing | 用户图7 | none | local |
| mechanism_8.jpg | 565x681 | 0.83 | 顶层储料与夹爪 | CAD | 俯视结构说明 | no-crop | user | Existing | 用户图8 | none | local |
| prototype_1.jpg | 1280x1707 | 0.75 | 实车基础 | Photo | 竖图保留整车 | adaptive | user | Existing | 用户实车1 | none | local |
| prototype_2.jpg | 1707x1280 | 1.33 | 电控联调 | Photo | 横幅调试现场 | adaptive | user | Existing | 用户实车2 | none | local |
| prototype_3.jpg | 960x1708 | 0.56 | 机构细节 | Photo | 竖图说明实物实现 | adaptive | user | Existing | 用户实车3 | none | local |

## IX. Content Outline

### Part 1: Engineering design and progress

#### Slide 01 - 智能物流运输机器人
- **Audience move**: 封面看见已落地的整车
- **Relationships**: 实车与电控完成，OPS和视觉正在优化
- **Title**: 智能物流运输机器人
- **Core message**: 实车与电控完成，OPS和视觉正在优化
- **Content**: 实车已搭建；电控完成调试；工创赛智能物流运输赛道
- **Images**: mechanism_6.jpg

#### Slide 02 - 以任务闭环组织分层设计
- **Audience move**: 理解需求与系统分工
- **Relationships**: 摄像头与OPS输入树莓派，树莓派规划，C板实时执行
- **Title**: 以任务闭环组织分层设计
- **Core message**: 摄像头与OPS输入树莓派，树莓派规划，C板实时执行
- **Content**: 读取任务、识别、抓取、储存、运输、放置、返航；初始300×300mm、400mm高、单电源、自主运行；USB CDC+V1

#### Slide 03 - 整车布局兼顾移动、抓取与存储
- **Audience move**: 认识整体结构与全向底盘
- **Relationships**: 全向底盘承载立柱、储料盘与控制器，底部定位轮提供测量输入
- **Title**: 整车布局兼顾移动、抓取与存储
- **Core message**: 全向底盘承载立柱、储料盘与控制器，底部定位轮提供测量输入
- **Content**: 麦克纳姆轮横移与转向；三工位储料；升降伸缩夹爪；驱动与测量分离
- **Images**: mechanism_6.jpg; mechanism_1.jpg; mechanism_2.jpg

#### Slide 04 - 升降、伸缩与开合完成抓放动作
- **Audience move**: 从实物理解机构运动
- **Relationships**: 丝杆与导轨配合升降，齿轮齿条驱动伸缩，舵机驱动夹爪
- **Title**: 升降、伸缩与开合完成抓放动作
- **Core message**: 丝杆与导轨配合升降，齿轮齿条驱动伸缩，舵机驱动夹爪
- **Content**: 高度对齐、接近、夹持、收回、车载储存；动作分解便于调试
- **Images**: mechanism_7.jpg; prototype_3.jpg

#### Slide 05 - 自制零件围绕安装与作业设计
- **Audience move**: 认识主要自制零件的结构
- **Relationships**: 承载板连接底盘与立柱，支架连接电机，夹爪适配物料，定位组件安装于底部
- **Title**: 自制零件围绕安装与作业设计
- **Core message**: 承载板连接底盘与立柱，支架连接电机，夹爪适配物料，定位组件安装于底部
- **Content**: 承载板和支架可编辑结构图；夹爪与定位组件配CAD；无尺寸，不作为加工图
- **Images**: mechanism_5.jpg; mechanism_8.jpg

#### Slide 06 - 分层电控，统一仲裁与异常停车
- **Audience move**: 理解电控原理与通信保护
- **Relationships**: 树莓派规划与C板控制双向交换；手动优先；错误帧不更新目标；超时停止
- **Title**: 分层电控，统一仲裁与异常停车
- **Core message**: 树莓派规划与C板控制双向交换；手动优先；错误帧不更新目标；超时停止
- **Content**: STM32F407、FreeRTOS、CAN底盘、升降伸缩舵机、BMI088、CRSF；USB CDC+V1帧长度序号CRC16心跳；USB链路UNVERIFIED
- **Images**: prototype_2.jpg

#### Slide 07 - OPS管全场，视觉修正工位偏差
- **Audience move**: 明确当前优化路线和待测项
- **Relationships**: 测量位移与姿态形成全场位姿，视觉输出目标颜色、中心与有效性，服务工位修正
- **Title**: OPS管全场，视觉修正工位偏差
- **Core message**: 测量位移与姿态形成全场位姿，视觉输出目标颜色、中心与有效性，服务工位修正
- **Content**: OPS方向尺度坐标标定；OpenCV二维码颜色与上表面中心；dx=cx-290、dy=260-cy；实验尚未达标，绝对精度UNVERIFIED
- **Images**: mechanism_3.jpg; mechanism_5.jpg

#### Slide 08 - 投入定位与联调，四阶段推进
- **Audience move**: 能评估投入与计划
- **Relationships**: 6项拟增补费用合计2200元；标定、单工位、融合、整场验证依序推进
- **Title**: 投入定位与联调，四阶段推进
- **Core message**: 6项拟增补费用合计2200元；标定、单工位、融合、整场验证依序推进
- **Content**: OPS800、视觉300、结构300、电气300、耗材300、机动200；预算待核价；第1至4周建议阶段计划
- **Visualization**: budget-costs: 自定义费用明细表
- **Native-ready**: budget-costs=yes

#### Slide 09 - 实车已有基础，整场效果以实测验收
- **Audience move**: 区分完成成果与预期指标
- **Relationships**: 现有整车机构与电控成果支撑后续全场自主；全场、抓放、安全各有验收证据
- **Title**: 实车已有基础，整场效果以实测验收
- **Core message**: 现有整车机构与电控成果支撑后续全场自主；全场、抓放、安全各有验收证据
- **Content**: 实车照片；定位与视觉进行中；两批任务与返航、停靠误差、抓放成功率、耗时、失联停车；指标待测
- **Images**: prototype_1.jpg; prototype_2.jpg; prototype_3.jpg

#### Slide 10 - 小车进展演示
- **Audience move**: 留出实车GIF展示入口
- **Relationships**: 底盘、抓放与视觉是进展演示的三个片段
- **Title**: 小车进展演示
- **Core message**: 底盘、抓放与视觉是进展演示的三个片段
- **Content**: 暂用实车静态图；GIF预留区；感谢评委
- **Images**: prototype_1.jpg

## X. Speaker Notes Requirements
- **Generation**: enabled
- **Filename**: match each SVG filename under notes/
- **Content**: 每页中文讲稿，区分已完成、正在做和预期；不虚构指标或加工尺寸
- **Total duration**: 约5分钟，可按答辩时长裁剪
- **Notes style**: 正式、简洁
- **Presentation purpose**: 说明工程设计与阶段成果，提出后续计划
