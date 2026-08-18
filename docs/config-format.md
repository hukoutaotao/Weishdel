# 自走棋配置文件格式规范

- 规范版本：1.0
- 状态：第 2 天冻结
- 适用语言标准：C++17
- 适用范围：`data/` 下的游戏、单位、技能、分队和地图配置

本规范是配置解析器、配置数据和配置测试的共同依据。字段名称、数据类型、枚举值、坐标规则和错误处理方式在第 2 天结束后不再修改；后续只能增加带有明确默认值的可选字段。

## 1. 文件布局

```text
data/
  game.cfg
  units.cfg
  skills.cfg
  factions.cfg
  maps/
    map_01.map
    map_02.map
```

配置加载顺序固定为：

1. `game.cfg`
2. `skills.cfg`
3. `units.cfg`
4. `factions.cfg`
5. `maps/map_01.map`
6. `maps/map_02.map`

技能先于单位加载，因为单位通过 `skill_id` 引用技能；分队最后加载，因为分队修正可以引用单位。

## 2. 通用语法

### 2.1 编码和行

- 文件必须使用 UTF-8 编码且不带 BOM。
- Windows 的 CRLF 和 Unix 的 LF 换行均为合法换行。
- 空行合法。
- 解析每一行前，先移除行首和行尾空白。
- 移除空白后，以 `#` 开头的整行是注释。
- 不支持行尾注释。
- 不支持引号、转义字符、嵌套节或多行字段值。

### 2.2 节

单例节只有以下两种：

```text
[game]
[map]
```

带 ID 的节使用以下格式：

```text
[unit:unit_id]
[skill:skill_id]
[faction:faction_id]
[faction_modifier:modifier_id]
[route:route_id]
```

节类型和 ID 之间只允许一个英文冒号。节标题内部不允许空格。

每种文件允许出现的节如下：

| 文件 | 允许的节 |
|---|---|
| `game.cfg` | 一个 `[game]` |
| `units.cfg` | 一个或多个 `[unit:id]` |
| `skills.cfg` | 一个或多个 `[skill:id]` |
| `factions.cfg` | 一个或多个 `[faction:id]` 和零个或多个 `[faction_modifier:id]` |
| `*.map` | 一个 `[map]` 和一个或多个 `[route:id]` |

单例节不得重复。同一节类型中的 ID 不得重复。

### 2.3 字段

字段使用以下格式：

```text
key=value
```

规则如下：

- 字段名使用小写 `snake_case`。
- 字段名区分大小写。
- 使用第一个等号分隔字段名和值。
- 字段名和值在移除首尾空白后均不得为空。
- 字段必须位于合法节中。
- 同一节内字段顺序不限。
- 同一节内不得出现重复字段。
- 未在本规范中声明的节和字段必须报错，不能静默忽略。
- 缺少必填字段必须报错。

### 2.4 ID

普通 ID 必须满足以下规则：

```text
[a-z][a-z0-9_]*
```

例如 `training_guard`、`map_01` 合法，`TrainingGuard`、`01_guard`、`guard-1` 不合法。

`faction_modifier` 的 `unit_id` 字段可以使用特殊值 `*`，表示该修正适用于所有单位；除此之外均须引用已存在的单位 ID。

## 3. 基础数据写法

### 3.1 整数

- 格式：可选负号后跟一个或多个十进制数字。
- 不允许正号、千位分隔符、小数点或科学计数法。
- 示例：`0`、`10`、`-2`。

### 3.2 小数

- 使用英文句点作为小数点。
- 不允许科学计数法、百分号或逗号小数点。
- 示例：`0`、`0.75`、`1.5`。

### 3.3 布尔值

布尔值只允许以下两个小写值：

```text
true
false
```

### 3.4 字符串

- 字符串不使用引号。
- 字符串移除首尾空白后不得为空。
- 显示名称和描述可以包含中文及普通空格。
- ID、枚举值和字段名只能使用本规范指定的 ASCII 字符。

### 3.5 列表

普通列表使用英文逗号分隔：

```text
melee,tank
1.0,1.5,2.0
```

列表不得包含空元素。

### 3.6 坐标

- 单个坐标格式为 `x,y`。
- 坐标使用从 0 开始的整数。
- 原点 `0,0` 位于地图左上角。
- x 向右增加，y 向下增加。
- 坐标列表使用英文分号分隔。

```text
1,2
1,2;2,2;3,2
```

## 4. 错误处理

配置错误必须包含以下信息：

- 错误类别
- 中文错误说明
- 源文件路径
- 行号

建议的显示格式为：

```text
data/maps/map_01.map:12: [地图校验] 路线 route_a 包含斜向连接：2,3 -> 3,4
```

错误类别固定为：

| 类别 | 用途 |
|---|---|
| `文件打开` | 文件不存在、无法读取 |
| `语法错误` | 节标题或 `key=value` 格式错误 |
| `重复定义` | 重复字段、重复节或重复 ID |
| `未知字段` | 未声明的节或字段 |
| `缺少字段` | 必填字段不存在 |
| `类型错误` | 整数、小数、布尔值、列表或坐标无法转换 |
| `范围错误` | 数值超出允许范围 |
| `引用错误` | 引用了不存在的技能、单位或分队 |
| `地图校验` | 网格或路线不合法 |

解析器遇到错误后返回本次加载的第一个错误，不允许把部分有效配置交给游戏继续运行。

行号规则：

- 普通字段错误使用该字段所在行。
- 重复定义使用第二次定义所在行。
- 缺少字段使用所属节标题所在行。
- 文件无法打开时使用行号 0。

## 5. `game.cfg`

`game.cfg` 必须包含且只包含一个 `[game]` 节。

| 字段 | 类型 | 合法范围 | 第 2 天初始值 |
|---|---|---|---:|
| `max_rounds` | 整数 | 大于 0 | 3 |
| `preparation_seconds` | 整数 | 大于 0 | 45 |
| `combat_timeout_seconds` | 整数 | 大于 0 | 60 |
| `starting_gold` | 整数 | 大于或等于 0 | 10 |
| `round_income` | 整数 | 大于或等于 0 | 5 |
| `loser_bonus` | 整数 | 大于或等于 0 | 2 |
| `shop_slots` | 整数 | 必须等于 6 | 6 |
| `shop_refresh_cost` | 整数 | 大于或等于 0 | 2 |
| `roster_capacity` | 整数 | 大于 0 | 8 |
| `sell_ratio` | 小数 | `0.0` 到 `1.0` | 0.75 |
| `merge_refund_ratio` | 小数 | `0.0` 到 `1.0` | 0.40 |
| `revive_ratio` | 小数 | `0.0` 到 `1.0` | 0.50 |
| `max_unit_level` | 整数 | 必须等于 3 | 3 |
| `random_seed` | 整数 | `0` 到 `4294967295` | 20260814 |

完整示例：

```text
[game]
max_rounds=3
preparation_seconds=45
combat_timeout_seconds=60
starting_gold=10
round_income=5
loser_bonus=2
shop_slots=6
shop_refresh_cost=2
roster_capacity=8
sell_ratio=0.75
merge_refund_ratio=0.40
revive_ratio=0.50
max_unit_level=3
random_seed=20260814
```

这些数值是开发初始值，可以在后续平衡阶段调整；字段名称、类型和范围已经冻结。

## 6. `units.cfg`

每个单位使用一个 `[unit:id]` 节。

| 字段 | 类型 | 规则 |
|---|---|---|
| `name` | 字符串 | 非空显示名称 |
| `tags` | ID 列表 | 至少一个，不得重复 |
| `max_health` | 整数 | 大于 0 |
| `attack_power` | 整数 | 攻击行动时大于或等于 0；治疗行动时必须大于 0 |
| `physical_defense` | 整数 | 大于或等于 0 |
| `magic_resistance` | 整数 | `0` 到 `100` |
| `attack_range` | 小数 | 大于 0，单位为格 |
| `move_speed` | 小数 | 大于 0，单位为格/秒 |
| `attack_speed` | 整数 | `0` 到 `600` |
| `guard_damage` | 整数 | 大于 0 |
| `price` | 整数 | 大于 0 |
| `initial_mana` | 整数 | 大于或等于 0 |
| `max_mana` | 整数 | 大于 0，且不小于 `initial_mana` |
| `basic_action` | 枚举 | `attack` 或 `heal` |
| `basic_damage_type` | 枚举 | `physical`、`magic` 或 `none` |
| `skill_id` | ID | 必须引用已存在的技能 |
| `level_multipliers` | 小数列表 | 恰好三个正数，第一个必须为 `1.0`，不得递减 |

条件规则：

- `basic_action=attack` 时，`basic_damage_type` 必须为 `physical` 或 `magic`。
- `basic_action=heal` 时，`basic_damage_type` 必须为 `none`，`attack_power` 表示每次普通治疗的基础治疗量。
- `level_multipliers` 只作用于最大生命、攻击力、物理防御和守卫伤害。
- 魔法抗性、攻击距离、移动速度、攻击速度、价格和技力不随等级倍率变化。
- 使用等级倍率得到整数属性时，统一使用 `std::round` 后转换为整数。

占位示例：

```text
[unit:training_guard]
name=训练守卫
tags=melee,tank
max_health=100
attack_power=20
physical_defense=5
magic_resistance=10
attack_range=1.0
move_speed=1.0
attack_speed=100
guard_damage=5
price=3
initial_mana=0
max_mana=10
basic_action=attack
basic_damage_type=physical
skill_id=training_strike
level_multipliers=1.0,1.5,2.0
```

该示例只用于验证格式，不代表第 5 天的正式单位设计。

## 7. `skills.cfg`

每个技能使用一个 `[skill:id]` 节。所有技能都写出全部字段；不适用的字段使用本节规定的明确值，避免依赖字段缺失表达语义。

| 字段 | 类型 | 规则 |
|---|---|---|
| `name` | 字符串 | 非空显示名称 |
| `description` | 字符串 | 非空中文说明 |
| `effect_type` | 枚举 | `damage`、`heal` 或 `buff` |
| `target_rule` | 枚举 | `self`、`enemy` 或 `lowest_health_ally` |
| `target_count` | 整数 | 大于 0 |
| `effect_range` | 小数 | 大于或等于 0，单位为格 |
| `damage_type` | 枚举 | `physical`、`magic` 或 `none` |
| `level_values` | 小数列表 | 恰好三个大于或等于 0 的值 |
| `duration_seconds` | 小数 | 大于或等于 0 |
| `buff_stat` | 枚举 | `none`、`attack_power`、`physical_defense`、`magic_resistance`、`move_speed` 或 `attack_speed` |
| `modifier_mode` | 枚举 | `none`、`add` 或 `multiply` |
| `allow_self` | 布尔值 | 是否允许把施法者作为目标 |
| `allow_move` | 布尔值 | 技能持续期间是否允许移动 |
| `allow_basic_action` | 布尔值 | 技能持续期间是否允许普通行动 |

条件规则：

- `damage`：`damage_type` 必须为 `physical` 或 `magic`，`duration_seconds=0`，`buff_stat=none`，`modifier_mode=none`。
- `heal`：`damage_type=none`，`duration_seconds=0`，`buff_stat=none`，`modifier_mode=none`。
- `buff`：`damage_type=none`，`duration_seconds` 必须大于 0，`buff_stat` 不得为 `none`，`modifier_mode` 必须为 `add` 或 `multiply`。
- `target_rule=self` 时，`target_count=1`、`effect_range=0` 且 `allow_self=true`。
- `level_values` 分别对应单位等级 1、2、3。
- 技能没有额外冷却时间；释放时消耗全部技力。这两条是游戏规则，不在配置中重复设置。

占位示例：

```text
[skill:training_strike]
name=训练打击
description=对一个敌方单位造成物理伤害
effect_type=damage
target_rule=enemy
target_count=1
effect_range=2.0
damage_type=physical
level_values=20,30,45
duration_seconds=0
buff_stat=none
modifier_mode=none
allow_self=false
allow_move=true
allow_basic_action=true
```

该示例只用于验证格式，不代表第 5 天的正式技能设计。

## 8. `factions.cfg`

### 8.1 分队定义

每个分队使用一个 `[faction:id]` 节。

| 字段 | 类型 | 规则 |
|---|---|---|
| `name` | 字符串 | 非空显示名称 |
| `initial_guard` | 整数 | 大于 0 |
| `max_deployed` | 整数 | 大于 0，且不得超过 `roster_capacity` |
| `price_multiplier` | 小数 | 大于 0 |

### 8.2 分队修正

每项修正使用一个 `[faction_modifier:id]` 节。

| 字段 | 类型 | 规则 |
|---|---|---|
| `faction_id` | ID | 必须引用已存在的分队 |
| `unit_id` | ID 或 `*` | 单个单位，或所有单位 |
| `attribute` | 枚举 | `max_health`、`attack_power`、`physical_defense`、`magic_resistance`、`attack_range`、`move_speed`、`attack_speed`、`guard_damage` 或 `price` |
| `operation` | 枚举 | `add` 或 `multiply` |
| `value` | 小数 | `multiply` 时必须大于 0 |

修正规则：

- 每次需要修正属性时，都从单位基础定义重新计算。
- 分队修正不得跨回合累积。
- 同一单位可以同时受到 `unit_id=*` 和指定单位 ID 的修正。
- 先按配置出现顺序应用全部 `add`，再按配置出现顺序应用全部 `multiply`。
- 得到整数属性或价格时，统一使用 `std::round` 后转换为整数。
- 修正后的属性仍必须满足对应单位字段的合法范围。

占位示例：

```text
[faction:training_team]
name=训练分队
initial_guard=100
max_deployed=4
price_multiplier=1.0

[faction_modifier:training_attack_bonus]
faction_id=training_team
unit_id=training_guard
attribute=attack_power
operation=multiply
value=1.10
```

该示例只用于验证格式，不代表第 5 天的正式分队设计。

## 9. 地图文件

### 9.1 地图定义

每个地图文件必须包含且只包含一个 `[map]` 节。

| 字段 | 类型 | 规则 |
|---|---|---|
| `id` | ID | 在全部地图中唯一 |
| `name` | 字符串 | 非空显示名称 |
| `width` | 整数 | 大于或等于 3 |
| `height` | 整数 | 大于或等于 3 |
| `row_0` 至 `row_n` | 字符串 | 必须恰好有 `height` 行 |

网格字符固定为：

| 字符 | 含义 | 是否可作为路线点 |
|---|---|---|
| `.` | 普通可通行格 | 是 |
| `#` | 障碍 | 否 |
| `A` | A 方部署格 | 是 |
| `B` | B 方部署格 | 是 |
| `X` | A 方守卫格 | 是 |
| `Y` | B 方守卫格 | 是 |

网格规则：

- 必须包含至少一个 `A` 和至少一个 `B`。
- 必须恰好包含一个 `X` 和一个 `Y`。
- `row_0` 到 `row_(height-1)` 必须连续，不得缺失或多出。
- 每行字符数必须等于 `width`。
- 网格不得包含表格中未声明的字符。

### 9.2 路线定义

每条路线使用一个 `[route:id]` 节。

| 字段 | 类型 | 规则 |
|---|---|---|
| `side` | 枚举 | `A` 或 `B` |
| `start` | 坐标 | 对应一方的部署格 |
| `points` | 坐标列表 | 至少包含起点和终点 |

路线校验顺序固定为：

1. 路线 ID 不重复。
2. `points` 的第一个坐标等于 `start`。
3. A 方起点是 `A`，B 方起点是 `B`。
4. 每个部署格恰好对应一条路线。
5. 所有坐标均在地图范围内。
6. 所有路线点均为可通行格。
7. 相邻点的曼哈顿距离必须等于 1，即只能上下左右移动。
8. A 方路线终点必须是 `Y`，B 方路线终点必须是 `X`。

不同路线允许重合或交叉。单位之间没有碰撞和阻挡。

最小合法示例：

```text
[map]
id=training_map
name=训练地图
width=7
height=5
row_0=#######
row_1=#X...B#
row_2=#.....#
row_3=#A...Y#
row_4=#######

[route:a_lane]
side=A
start=1,3
points=1,3;2,3;3,3;4,3;5,3

[route:b_lane]
side=B
start=5,1
points=5,1;4,1;3,1;2,1;1,1
```

该示例只用于解释格式。正式的 `map_01.map` 和 `map_02.map` 将在第 2 天后续步骤中设计。

## 10. 必须拒绝的输入

后续测试至少覆盖：

- 文件不存在。
- 字段出现在任何节之前。
- 节标题缺少右方括号。
- 字段缺少等号。
- 字段名或值为空。
- 重复节、重复 ID 或重复字段。
- 未知节或未知字段。
- 缺少必填字段。
- 整数、小数、布尔值、列表或坐标格式错误。
- 数值超出合法范围。
- 单位引用不存在的技能。
- 分队修正引用不存在的分队或单位。
- 地图宽高与实际网格不符。
- 地图包含未知字符。
- 路线起点错误。
- 部署格缺少路线或拥有多条路线。
- 路线坐标越界。
- 路线穿过障碍。
- 路线包含斜向连接或跳格。
- 路线终点不是敌方守卫格。

## 11. 冻结边界

从本规范完成之日起，以下内容冻结：

- 文件编码和通用语法。
- 节名称和字段名称。
- 字段数据类型和枚举值。
- ID、列表、坐标及地图字符规则。
- 路线合法性规则。
- 错误类别、路径和行号要求。

以下内容尚未冻结：

- 五个正式单位的名称和数值。
- 五个正式技能的描述和数值。
- 三个正式分队的名称和数值。
- 两张正式地图的精确布局和路线。
- 游戏参数的最终平衡数值。

后续如确需扩展格式，只允许增加带有明确默认值的可选字段；不得重命名已有字段、改变已有字段含义或放宽非法输入处理规则。

本格式只使用 C++17 标准库解析，不引入 JSON、XML 或其他第三方配置库。
