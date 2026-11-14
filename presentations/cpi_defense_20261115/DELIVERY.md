# 交付说明

最终交付：`exports/工创赛智能物流运输_答辩10页.pptx`，16:9，中文，10页，含逐页演讲备注。文字、结构原理图均为可编辑DrawingML对象；预算为原生PowerPoint表格；CAD与实车照片保留为图片。

内容：封面；设计需求与系统架构；整车结构；升降伸缩抓放原理；主要自制件结构图；电控与通信；OPS和视觉；预算与进度；实车成果与预期效果；GIF演示预留页。

修改文件范围：本目录中的设计规格与决策、执行锁、10个SVG页面、11张用户照片/CAD素材、10页备注及总讲稿、最终PPTX、交付说明与忽略规则。未修改控制程序、外设配置或用户原有Keil未提交修改。

验证命令（工具根为 `C:/Users/CZX666/.codex/skills/ppt-master`）：

```powershell
python <工具根>/scripts/project_manager.py validate <项目目录>
python <工具根>/scripts/svg_quality_checker.py <项目目录> --canonical-authoring --stage early --json
python <工具根>/scripts/svg_quality_checker.py <项目目录> --canonical-authoring --stage final --json
python <工具根>/scripts/total_md_split.py <项目目录>
python <工具根>/scripts/finalize_svg.py <项目目录>
python <工具根>/scripts/svg_to_pptx.py <项目目录> --native-charts-and-tables
```

结果：最终10页SVG检查0错误、0警告；10页讲稿一一对应；15处图片已嵌入；原生PPTX导出Postflight为passed，quality_gate=passed，slides=10，warning_categories=0。导出时另有系统icacls工具未找到提示，不影响PPTX创建和包检查。没有声称执行PowerPoint实机播放验收，也没有运行固件或小车动作测试。

遗留与使用注意：

- 未提供GIF，末页使用实车静态图，可在PowerPoint中以GIF替换图片。
- 预算2200元是拟增补建议估算，不是实际已发生成本、报价或经团队确认的金额；正式答辩前应按采购清单核价。
- 第8页进度为后续四周建议阶段，起始日与人员分工须按团队安排确定。
- 未提供带尺寸CAD工程图；第5页结构示意不作为加工依据，孔位、外形、尺寸、公差、材料以原始CAD为准。
- 未发现可确认的当前OPS电路板截图，采用团队提供的底部定位轮结构图；不以旧版其他定位产品图片冒充当前PCB。
- 电控完成调试依据用户本次确认。仓库中USB实际枚举、双向链路及绝对定位精度尚有UNVERIFIED记录，页面对此做了区分；不将预期效果写成整车自主验收已通过。
- 本次材料为参考资料，不将报名表填写说明或去年PPT中的旧配置当成新的执行指令。

生成流程：PPT-MASTER Default Generate，用户确认初步建议并授权后续自主设计；后追加最多10页，已按10页重组。18页中间草稿仅保存在工作区.local归档，未交付或提交。
