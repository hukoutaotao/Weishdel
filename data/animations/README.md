# 单位 Spine 动画资源

正式资源按 `data/animations/units/<unit_id>/` 存放。每个单位可以包含两套独立的 Spine 3.8 导出组：

- `unit_id1.skel`、`unit_id1.atlas`、`unit_id1.png`：准备阶段的 `relax`、战斗移动的 `move`。
- `unit_id2.skel`、`unit_id2.atlas`、`unit_id2.png`：`start`、`attack`、`die` 等战斗动作。

`.atlas` 的第一页名称必须与实际 PNG 文件名一致。资源导入时会直接将老师提供的素材文件改名为项目 `unit_id`，并同步修改 atlas 第一行的 PNG 引用；不会额外生成运行时映射表。

当前暂存素材位于 `temp/wang` 和 `temp/wisdel`，待确认它们分别对应哪个正式 `unit_id` 后导入。
