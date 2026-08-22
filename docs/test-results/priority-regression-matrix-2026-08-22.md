# 第10天最高优先级回归矩阵

日期：2026-08-22
构建：MSVC x64 Release
配置随机种子：20260814
原始日志：`release-regression-2026-08-22.log`

## 结果摘要

- Release 增量构建：通过，0 条编译或链接诊断。
- 自动测试：396 PASS / 0 FAIL。
- AI 组合：18/18 正常结束，所有 `command_failures=0`。
- 两次 AI 矩阵输出逐行一致，固定配置下结果可重复。
- 本步骤补充了普通攻击目标同帧进入范围时按较小单位编号并列的直接回归测试。

## 配置与地图

| 验收规则 | 直接证据 | 结果 |
| --- | --- | --- |
| 缺少配置文件 | `ConfigParser reports a missing file` | 通过 |
| 配置格式错误 | `ConfigParser rejects a field without equals sign` | 通过 |
| 配置范围越界 | `GameConfigLoader rejects an out-of-range ratio` | 通过 |
| 重复字段或ID | `ConfigParser rejects duplicate fields`、`ConfigParser rejects duplicate sections`、`PlayerStateService rejects duplicate active IDs` | 通过 |
| 路线越界 | `MapConfigLoader rejects an out-of-bounds route point` | 通过 |
| 路线穿过障碍 | `MapConfigLoader rejects a route through an obstacle` | 通过 |
| 路线斜向连接 | `MapConfigLoader rejects a diagonal route segment` | 通过 |
| 路线未到敌方守卫 | `MapConfigLoader requires routes to end at the enemy guard` | 通过 |
| 每个部署格只有一条路线 | `MapConfigLoader requires one route for every deployment cell`、`rejects multiple routes` | 通过 |

## 战斗、索敌与治疗

| 验收规则 | 直接证据 | 结果 |
| --- | --- | --- |
| 攻速0的攻击间隔 | `Combat rules calculate the zero-speed attack interval` | 通过 |
| 攻速600及上限钳制 | `Combat rules calculate the maximum-speed attack interval`、`clamp attack speed above 600` | 通过 |
| 物理伤害最低5 | `Combat rules enforce five minimum physical damage` | 通过 |
| 法抗0和100边界 | `Combat rules apply the magic-resistance boundaries` | 通过 |
| 同帧互相击杀 | `Battle attacks resolve simultaneous mutual deaths in one frame` | 通过 |
| 双方守卫同帧归零 | `Match result detects simultaneous guard depletion`、`Battle batches simultaneous guard arrivals and damage` | 通过 |
| 优先首次进入范围目标 | `Target selector prefers the earlier entering target` | 通过 |
| 同帧进入按较小编号 | `Target selector breaks same-frame ties by lower ID` | 通过 |
| 离开范围清除并重新索敌 | `Target selector clears an out-of-range target and reacquires` | 通过 |
| 治疗按最低生命比例及编号 | `Target selector heals the lowest-ratio ally with ID tie-break` | 通过 |
| 治疗忽略满血目标并允许自疗 | `Skill healing targets use health ratio, ID, and self policy`、`Battle healing permits a healer to heal itself` | 通过 |
| 治疗不超过生命上限 | `Battle healing restores an ally without exceeding max health` | 通过 |

## 商店、部署和经济

| 验收规则 | 直接证据 | 结果 |
| --- | --- | --- |
| 金币不足不能购买 | `ShopService rejects insufficient purchase gold atomically` | 通过 |
| 持有上限阻止购买 | `ShopService rejects a full roster atomically` | 通过 |
| 非法敌方部署格 | `DeploymentService rejects the opposing deployment start` | 通过 |
| 部署格或备用槽占用 | `DeploymentService rejects an occupied deployment start`、`rejects an occupied reserve slot` | 通过 |
| 不同类型不能合成 | `MergeService rejects different unit types` | 通过 |
| 不同等级不能合成 | `MergeService rejects different unit levels` | 通过 |
| 三级不能继续合成 | `MergeService rejects units at the maximum level` | 通过 |
| 40%合成、75%出售、50%复活及四舍五入 | `PriceRules calculates merge, sell, and revive amounts` | 通过 |
| 经济命令失败保持原子性 | `Economy integration rejects an unaffordable purchase atomically` 等集成案例 | 通过 |

## 跨回合和最终胜负

| 验收规则 | 直接证据 | 结果 |
| --- | --- | --- |
| 死亡单位进入死亡列表 | `Round settlement moves dead and surviving side A units` | 通过 |
| 存活单位返回备用区 | `Round settlement moves dead and surviving side A units` | 通过 |
| 到达守卫和超时单位返回 | `Round settlement returns reached and timeout survivors by ID` | 通过 |
| 败者补助 | `Round preparation grants loser bonus only to side A`、验收矩阵中的 side B 案例 | 通过 |
| 最大回合比较守卫值 | `Match result compares guards after maximum rounds` | 通过 |
| 守卫相同为平局 | `Match result draws equal guards after maximum rounds` | 通过 |

## 18组AI组合

矩阵由 `3种策略 × 2张地图 × 3个分队` 组成：

- 策略：`offensive`、`defensive`、`route`；
- 地图：`map_01`、`map_02`；
- 分队：`training_team`、`assault_team`、`route_team`。

全部18组满足：

- 正常执行3回合；
- 没有非法命令失败；
- 没有超过帧保护或出现死循环；
- 两次 Release 测试中的矩阵输出完全相同；
- 策略单元测试确认三种AI在购买、路线优先级和技能选择上存在稳定差异。
