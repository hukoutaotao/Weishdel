# 第10天最终提交审计证据

日期：2026-08-22

## 1. 最终生产候选

- 分支：`步骤2`
- 生产候选提交：`69940461c03a75df7dac8ff71a0fdd37577920be`（`6994046`）
- 提交标题：`10.9 完成使用说明与提交文档`
- 候选范围：截至 10.9 的生产代码、测试、界面、报告和独立说明文档；10.10 只重新构建、重新测试并固定审计证据。
- 构建配置：C++17、MSVC x64 Release、SFML 2.6.2、CMake 预设 `msvc-release`。
- 本轮没有引入新第三方库，没有修改生产代码或测试判定。

## 2. 本轮 Release 构建

- 命令顺序：`cmake --preset msvc-release --fresh`，随后 `cmake --build --preset msvc-release`。
- 配置与构建退出码：0；生成 Release 游戏、测试程序和核心静态库。
- 最终构建日志：`docs/test-results/final-release-build-2026-08-22.log`。
- 最终构建日志 SHA-256：`0AB02153DC8B8F80F55B4B4D314B2CB056487458D745D9FC344E1269C3EE2CC8`。
- `build/msvc-release/AutoChessGame.exe` SHA-256：`9BA2F2D751203BE62446EF980812B74BE14E0A576EA60A719CAE3566EB0469E4`。
- `build/msvc-release/AutoChessTests.exe` SHA-256：`50D47F5B9BCDEFC124E4591A5B4CE53FFD2AE7796910EC76E9E870673D697373`。

## 3. 最终自动测试

- 测试程序退出码：0。
- 结果：402 PASS / 0 FAIL。
- AI 集成矩阵：18 组，覆盖 3 种策略 × 2 张地图 × 3 个分队。
- 18 组均为 `command_failures=0`、`rounds=3`。
- 固定随机种子：`20260814`。
- 最终测试日志：`docs/test-results/final-release-tests-2026-08-22.log`。
- 最终测试日志 SHA-256：`A75B294C947EFB2EF8FF1EC52AFD69F237994C5076D1D955805CF33A6859DA06`。

## 4. AI 十八组结果

| 策略 | 地图 | 分队 | 帧数 | AI 命令 | 命令失败 | 结果 | 回合 |
|---|---|---|---:|---:|---:|---|---:|
| offensive | map_01 | training_team | 363 | 9 | 0 | draw | 3 |
| offensive | map_01 | assault_team | 363 | 9 | 0 | draw | 3 |
| offensive | map_01 | route_team | 363 | 9 | 0 | A_win | 3 |
| offensive | map_02 | training_team | 363 | 13 | 0 | draw | 3 |
| offensive | map_02 | assault_team | 363 | 13 | 0 | draw | 3 |
| offensive | map_02 | route_team | 363 | 13 | 0 | A_win | 3 |
| defensive | map_01 | training_team | 363 | 9 | 0 | draw | 3 |
| defensive | map_01 | assault_team | 363 | 9 | 0 | draw | 3 |
| defensive | map_01 | route_team | 363 | 9 | 0 | A_win | 3 |
| defensive | map_02 | training_team | 363 | 13 | 0 | draw | 3 |
| defensive | map_02 | assault_team | 363 | 13 | 0 | draw | 3 |
| defensive | map_02 | route_team | 363 | 13 | 0 | A_win | 3 |
| route | map_01 | training_team | 363 | 9 | 0 | draw | 3 |
| route | map_01 | assault_team | 363 | 9 | 0 | draw | 3 |
| route | map_01 | route_team | 363 | 9 | 0 | A_win | 3 |
| route | map_02 | training_team | 363 | 13 | 0 | draw | 3 |
| route | map_02 | assault_team | 363 | 13 | 0 | draw | 3 |
| route | map_02 | route_team | 363 | 13 | 0 | A_win | 3 |

## 5. 独立 dist 发布目录

- 已执行 `cmake --build --preset msvc-release --target AutoChessDist`，退出码为 0。
- `dist` 恰好 10 个文件：1 个 EXE、3 个 SFML Release DLL、4 个 cfg、2 张地图。
- 六个 `dist/data` 文件与源码 `data` 对应文件 SHA-256 全部一致。
- dist 构建日志：`docs/test-results/final-dist-build-2026-08-22.log`。
- dist 构建日志 SHA-256：`D4EEA85301D6A5AA16387270B6B74AF367918FED84A863502AC76842C64DA6A4`。

| 相对路径 | 字节数 | SHA-256 |
|---|---:|---|
| `AutoChessGame.exe` | 484864 | `9BA2F2D751203BE62446EF980812B74BE14E0A576EA60A719CAE3566EB0469E4` |
| `data/factions.cfg` | 1147 | `C5DE0D244B4553ECD98FD7CDA30F0F59C665713CA139C268D3811C33111C6337` |
| `data/game.cfg` | 262 | `E2472F937AF6129792F6386DCB1C37882FC68B6AA860ED5241DF81E713D8A95A` |
| `data/maps/map_01.map` | 543 | `AA871811FD2D803701C1AD35E047425B108AE37A7515616BC20C5A15244F4DC5` |
| `data/maps/map_02.map` | 1140 | `C2A7448FD7973F8E4531EFA6BB92FDC01601E8FE5422699F8C9D9655A12ADD8A` |
| `data/skills.cfg` | 1689 | `C4EC66A3DFF3B7DDF7A13E6DFEE28D1F0AFA0C92555DC21895B5132260C90089` |
| `data/units.cfg` | 1684 | `0410D1D471C9D95421C1A5ADC7451D4B3B045831177E6F6EF41622863F4647B7` |
| `sfml-graphics-2.dll` | 892928 | `A9ED0ABC04C55142757804147F005BBDBCE80BF0FC063D797EB14DF3C98833D0` |
| `sfml-system-2.dll` | 51200 | `F91C896B016AE4BC57425FE86EDDF05C159422E93C1CAE1E2405C254DC0AAED1` |
| `sfml-window-2.dll` | 145408 | `6FBF5E2011B91BF10CB64713ED488F7E0E96806602324CE1BEB57A600B6E0C9C` |

## 6. 项目外启动验收

- 将本轮 `dist` 复制到 `D:\1` 之外的唯一临时目录，并写入本次所有权标记。
- 从 `D:\` 工作目录启动外部副本，成功创建标题为 `AutoChess` 的窗口。
- 通过窗口关闭消息正常关闭，进程退出码为 0。
- 清理前再次核对绝对路径、目录前缀和所有权标记，只删除本轮创建的外部临时目录。
- 结论：发布程序不依赖项目当前工作目录。
- 项目外启动日志：`docs/test-results/final-runtime-acceptance-2026-08-22.log`。
- 项目外启动日志 SHA-256：`512494764015C79A17951344F600EFE07030801AE80A804C453FCCFAAA7C56F3`。

## 7. 非法输入样本 SHA-256

| 相对路径 | SHA-256 |
|---|---|
| `tests/data/faction_missing_faction_reference.cfg` | `3E42F2663D0F924FCA6A82803E3D1153F8E9B9C07EFE35C18A9E30F8DBE9676B` |
| `tests/data/faction_missing_unit_reference.cfg` | `001F8BE72480F289FF7A5B26392687C6B9729F707E7C011DE92481566E09A569` |
| `tests/data/faction_too_many_deployed.cfg` | `D18AE45E83809F8FE561C663E9701B501263A01237FA0E1E0BA5269100565F36` |
| `tests/data/game_invalid_integer.cfg` | `B2B5304F27F4D6E3274F5A75011BC80D68991D4D83E582C7C612F2420C49D9A4` |
| `tests/data/game_invalid_ratio.cfg` | `53F08C243630C4237856F0B2DBB5453A1A083C0EEA0606731BDD36E48C7C3B6A` |
| `tests/data/game_missing_field.cfg` | `5AA1F3BC313C78D989BB1349A25CDFACE22BDF9A054A550A6705E451CFF38307` |
| `tests/data/game_seed_overflow.cfg` | `D3A52075104C0635B2547E58D40519BF836CE1A4C72E223B6093565A7D39CCD4` |
| `tests/data/game_unknown_field.cfg` | `3383E142D1B9453D813C9834941808E037E9DBFCE92BB873060AECFC574717E5` |
| `tests/data/map_duplicate_deployment_route.map` | `3A932A83890E9BD0B303A8F42AA85E4E82E01ED3DC877F72D017B23FA667D4D4` |
| `tests/data/map_first_point_mismatch.map` | `15F0C3F881DC19B053A9B240AE09C1ED526803BB052F6CD01A79297C554A59DB` |
| `tests/data/map_missing_deployment_route.map` | `31CCAE38581ACA90CEF8A7E90800F923B63EB0EC7F7BBD668B998BA777EAA15B` |
| `tests/data/map_missing_points.map` | `E60A61EBAF4720FE6D4DF1BE95DC65EC6E4CD1F90116FE5A40913B23C980D110` |
| `tests/data/map_route_diagonal.map` | `0095253C7D4130DB3809F2F433CCEF8FECFA27BC96773E3071DB9546163855EA` |
| `tests/data/map_route_out_of_bounds.map` | `068FF62CACCF73BC591D848ECF7FCFA23E027CDD442C125862342D5C084FE649` |
| `tests/data/map_route_through_obstacle.map` | `3A573C12905B65979D397F4495BCBCE2CBA8E97EFA6B8F3463895C01D819A720` |
| `tests/data/map_route_wrong_guard.map` | `664DC76BE240ADF2037EA6C674F7B9CBFC4D7FA66E7358DEF9F824F954445E80` |
| `tests/data/map_wrong_side_start.map` | `B8E3A79C033E0922188E7A641AD408048C03420364F77A44B711A242F7A03572` |
| `tests/data/parser_duplicate_field.cfg` | `D995E2024FB8FCC3CB79E29CA8E5FE39636526FAA36F9B91B3A9C16A8B53F745` |
| `tests/data/parser_duplicate_section.cfg` | `935A2185EFFF94DB0A35E701C51251E233D582939465CC5EB2B23A1D78B2BB2D` |
| `tests/data/parser_field_before_section.cfg` | `35AF6CBAC43F560DC61F5F16E7222A2C094D6CEFBC7F66C39217A2060184B62D` |
| `tests/data/parser_missing_bracket.cfg` | `53CC7F6F788F04919FF4E3D1A32C7B758DE556695FE07D3FBA2C3398B87895A8` |
| `tests/data/parser_missing_equals.cfg` | `534F54A6E67C2E7107ED76643F5BD5DDC6E85ED10D79389AFE557830271C3C01` |
| `tests/data/parser_valid.cfg` | `846BD017011FEC381EA4842FCB3EF00EB3935B87F382378EF9F7928FEF945591` |
| `tests/data/skill_invalid_condition.cfg` | `A0CED1088D9F545EE97F27B45A0EE374CB40EA9E65F3C076B97D2D65F15FA0AA` |
| `tests/data/skill_invalid_enum.cfg` | `FB659EF379DEB7E63662C7CABDC3862B78D52A189AE9672866FFA16F05ECAB18` |
| `tests/data/skill_invalid_list.cfg` | `B25F81E01E5EA4D594A1CEE014907B0F0351EFC2879001BE1F8EA4640D775BC1` |
| `tests/data/unit_invalid_heal_power.cfg` | `146AF3189C27172E09A283085D7F8A504D3D9D0B691152F6C7656E1ACB7DA14F` |
| `tests/data/unit_invalid_range.cfg` | `3625114914E35E396D7E537C217B94AC82C7F99DE2C22329D00AF959F7228E9A` |
| `tests/data/unit_missing_field.cfg` | `AA839F8914F63095E8C21D28B75F21CC751AA97506E305447DBE704402A311B7` |
| `tests/data/unit_missing_skill_reference.cfg` | `194E24B26ECFAEDA6F9AE0043E19CE989775AC7D7B3BDDCAFBA1CC0BC91A6C6E` |

## 8. 报告与演示材料审计

- 最终报告：`docs/自走棋对战系统课程设计报告.docx`，共 28 页。
- 报告结构：18 张行内图片、0 张浮动图片、0 个超宽图片、0 个边缘警告、0 个空白页。
- 附录 A 的源程序清单包含 104 项；附录 B 完整保留原任务书和评分表。
- 最终 AI 截图与报告内嵌 `word/media/image8.png` 的 SHA-256 完全一致。
- 报告只读审计日志：`docs/test-results/final-report-audit-2026-08-22.log`。
- 报告只读审计日志 SHA-256：`FDB0D274957C7D2C47AAD4418FB441569CD871E610B8BBCEAE86552D581C9A8D`。
- 演示视频录制脚本：`docs/演示视频录制脚本.md`。
- 演示视频录制脚本 SHA-256：`A2356E9D2689EF24E394C4D131871EE2D9ECF96503F08D82377824418F0DF49B`。
- 本机存在 Xbox Game Bar 和 Windows 截图工具；视频仍需学生本人完成隐私检查、连续录制和完整播放。
- 报告封面个人信息及视频文件均未虚构。

## 9. 最终结论

- 本轮 fresh Release 构建、402 项自动测试、18 组 AI、dist 重建和项目外启动均已通过。
- 候选提交、输入文件、测试输出、可执行文件和发布文件均已用路径与 SHA-256 关联。
- 演示视频尚未实际录制和播放验收，因此不登记文件名、时长、大小或哈希，也不在提交检查表中勾选视频项目。
- 报告封面的班级、学号、姓名和指导教师继续留空，必须由学生本人填写。
