# 课程设计报告资源清单

日期：2026-08-22

本目录只保存第 10 天最终验收后选定的报告图示和运行截图；未修改任何生产代码、配置或发布文件。

## 图示

| 文件 | 用途 | SHA-256 |
|---|---|---|
| `diagrams/module-diagram.png` | 系统功能模块图与分层依赖 | `78D5CF0866A3672CB8CC515219A91DD87D942622D673849F0116CA03B344D565` |
| `diagrams/class-hierarchy.png` | 抽象类、虚函数、多态和多重继承类层次图 | `48763D538D130BCAE66C0F8717E93E6C57BFB5000F39A3C94B80F852C0180934` |
| `diagrams/game-flow.png` | 从启动检查到三回合结算的系统流程图 | `67080A4AF964D35C289B1568AB6E3636B47666B79C9A61CA237527456A3F9F58` |

## 正式运行截图

| 报告文件 | 原始验收文件 | 场景 | SHA-256 |
|---|---|---|---|
| `screenshots/01-main-menu.png` | `build/qa-temp/autochess-10.5-accepted/01-main-menu.png` | 主菜单 | `A39B45876A24D5080B8348A3ED6D443BF652F543351496A1832D81858410E0AC` |
| `screenshots/02-help.png` | `build/qa-temp/autochess-10.5-accepted/02-help.png` | 帮助页 | `061349E79F65449AD9475065BD7D32F377059AFC7EB0CCC0AE72F9C08E8674CD` |
| `screenshots/03-map-selection.png` | `build/qa-temp/autochess-10.5-accepted/04-map-selection.png` | 地图选择 | `14E264A5A992608ED57B711DB6F0D68DA97F449CE82A908CF606C315A8C76CB7` |
| `screenshots/04-faction-selection.png` | `build/qa-temp/autochess-10.5-accepted/05-faction-selection.png` | 分队选择 | `0440C4EFBA666D2697823F3F9144C20E52E5D02B3A583CCDE6B8D9AA31EF0800` |
| `screenshots/05-ai-selection.png` | `build/qa-temp/autochess-10.5-accepted/06-ai-selection.png` | AI 策略选择 | `4E886E9FF3773BB7C82CBDBFC0E664EF691E711BBE7B88D13E51AFA0169CC7C3` |
| `screenshots/06-preparation.png` | `build/qa-temp/autochess-10.5-accepted/07-preparation.png` | 准备阶段 | `3155AC7231D534F13652652C44C5A8E730831E3854089FBD487B30732A4536AF` |
| `screenshots/07-purchased.png` | `build/qa-temp/autochess-10.5-accepted/08-purchased.png` | 购买单位 | `EC22B4143CAA085585E594198F44E79C8271FA9C63BA840E587388B15A8EBF2B` |
| `screenshots/08-deployed.png` | `build/qa-temp/autochess-10.5-accepted/09-deployed.png` | 拖拽部署 | `7B998CBFA9D2620E36958115F74C086A052B89E6A6B48FDC22B79A52A6999213` |
| `screenshots/09-combat.png` | `build/qa-temp/autochess-10.5-accepted/10-combat.png` | 战斗阶段 | `283896A60125542064CA73CE7E4CB9276AF912D5F232664E6C2B8BF61E4BE859` |
| `screenshots/10-paused.png` | `build/qa-temp/autochess-10.5-accepted/11-paused.png` | 暂停覆盖层 | `DBA88B66CF6F8975B0CF3C1497FC47204829195502808B14818A072F414B4936` |
| `screenshots/11-resumed.png` | `build/qa-temp/autochess-10.5-accepted/13-resumed.png` | 继续战斗 | `DB5F8280C641B030A0EAC7630938B69514F53DEF0B0A50AE1CF86CCB3BA5B0B5` |

## 选择规则

- 原始 13 张验收截图中，返回主菜单图与初始主菜单图重复，暂停延时图与暂停图重复，因此正式材料只保留 11 张唯一画面。
- 所有截图均为 1280×720，来自 Release x64 程序在 192 DPI（200% 缩放）环境中的实际运行。
- 图示由项目实际类声明、CMake 目标、数据目录和运行状态流绘制，不表示未实现功能。
- 本步骤未引入项目第三方库；Pillow 仅来自 Codex 文档工作区，用于生成报告 PNG，不参与游戏构建或发布。
