<!-- ppt-master-schema: spec-lock/v1 -->
# Execution Lock

## canvas
- viewBox: 0 0 1280 720
- format: PPT 16:9

## communication
- primary_language: zh-CN
- audience: 工创赛智能物流运输赛道答辩评委
- objective: 解释机械与电控设计，展示实车成果，使评委可评估后续定位视觉优化的投入与进度
- core_message: 实车已搭建，电控已完成调试，正在推进OPS全场定位与视觉优化
- consumption_mode: balanced

## mode
- mode: custom
- mode_references: instructional
- mode_behavior: 由整车总览分解机械、电控、感知与流程，再以预算和验收计划收束

## visual_style
- visual_style: custom
- visual_style_references: swiss-minimal
- visual_style_behavior: 白底深蓝标题与青绿链路，严格对齐与细规则线，机构大图和可编辑原理图交替，深蓝结论面强调进展

## colors
- background: #FFFFFF
- secondary_bg: #F1F5F9
- primary: #12304A
- accent: #008C95
- secondary_accent: #C88925
- body_text: #203447
- secondary_text: #596D7E
- divider: #D7E1E8

## typography
- font_family: Microsoft YaHei, Arial
- title_family: Microsoft YaHei, Arial
- body_family: Microsoft YaHei, Arial
- body: 24
- title: 40
- subtitle: 30
- annotation: 18
- footnote: 14
- display: 56

## icons
- library: none
- inventory: none

## page_rhythm
- P01: anchor
- P02: dense
- P03: dense
- P04: dense
- P05: dense
- P06: dense
- P07: dense
- P08: dense
- P09: dense
- P10: anchor

## pptx_structure
- mode: flat

## images
- chassis: images/mechanism_1.jpg | source=user | crop=no-crop
- underside: images/mechanism_2.jpg | source=user | crop=no-crop
- tracking: images/mechanism_3.jpg | source=user | crop=no-crop
- ops: images/mechanism_5.jpg | source=user | crop=no-crop
- assembly: images/mechanism_6.jpg | source=user | crop=no-crop
- lifting: images/mechanism_7.jpg | source=user | crop=no-crop
- storage: images/mechanism_8.jpg | source=user | crop=no-crop
- prototype: images/prototype_1.jpg | source=user | crop=adaptive
- electronics: images/prototype_2.jpg | source=user | crop=adaptive
- detail: images/prototype_3.jpg | source=user | crop=adaptive

## forbidden
- `mask`, `<style>`, `class`, external CSS, `<foreignObject>`, `textPath`, `@font-face`, `<animate*>`, `<set>`, `<script>` / event attributes, `<iframe>`
- HTML named entities in text; write typography as raw Unicode and escape XML reserved characters
