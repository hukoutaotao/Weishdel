from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(r"D:\1\new")
OUT = ROOT / ".report_work" / "diagrams"
FONT = ROOT / "data" / "fonts" / "NotoSansSC-VF.ttf"

NAVY = "#17324D"
BLUE = "#2D5D7E"
LINE = "#3B6F94"
PALE_BLUE = "#EAF4FB"
PALE_GREEN = "#EAF6EF"
PALE_PURPLE = "#F1ECFA"
PALE_ORANGE = "#FFF3DF"
PALE_GRAY = "#F4F7F9"
TEXT = "#203040"


def font(size: int, bold: bool = False):
    return ImageFont.truetype(str(FONT), size=size)


def centered(draw, box, text, size=28, fill=TEXT, spacing=6):
    x1, y1, x2, y2 = box
    f = font(size)
    bbox = draw.multiline_textbbox((0, 0), text, font=f, spacing=spacing, align="center")
    width = bbox[2] - bbox[0]
    height = bbox[3] - bbox[1]
    draw.multiline_text(
        ((x1 + x2 - width) / 2, (y1 + y2 - height) / 2 - bbox[1]),
        text,
        font=f,
        fill=fill,
        spacing=spacing,
        align="center",
    )


def rounded(draw, box, text, fill=PALE_BLUE, outline=LINE, size=28, radius=18, width=3):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)
    centered(draw, box, text, size=size)


def diamond(draw, center, width, height, text, fill=PALE_ORANGE, outline="#C47D10", size=26):
    cx, cy = center
    points = [(cx, cy - height // 2), (cx + width // 2, cy), (cx, cy + height // 2), (cx - width // 2, cy)]
    draw.polygon(points, fill=fill, outline=outline)
    draw.line(points + [points[0]], fill=outline, width=3)
    centered(draw, (cx - width // 2, cy - height // 2, cx + width // 2, cy + height // 2), text, size=size)


def arrow(draw, start, end, color=LINE, width=4):
    draw.line([start, end], fill=color, width=width)
    ex, ey = end
    sx, sy = start
    if abs(ex - sx) >= abs(ey - sy):
        sign = 1 if ex > sx else -1
        pts = [(ex, ey), (ex - sign * 15, ey - 9), (ex - sign * 15, ey + 9)]
    else:
        sign = 1 if ey > sy else -1
        pts = [(ex, ey), (ex - 9, ey - sign * 15), (ex + 9, ey - sign * 15)]
    draw.polygon(pts, fill=color)


def save(image, name):
    OUT.mkdir(parents=True, exist_ok=True)
    image.save(OUT / name, dpi=(220, 220))


def flowchart():
    image = Image.new("RGB", (1000, 1500), "white")
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 1000, 1500), fill=PALE_GRAY)
    centered(draw, (40, 25, 960, 105), "自走棋对局运行流程图", size=48, fill=NAVY)

    x1, x2 = 190, 810
    nodes = [
        (130, 215, "启动 AutoChessGame.exe", PALE_BLUE),
        (315, 405, "进入主菜单\n开始游戏 / 帮助 / 退出", PALE_GREEN),
        (500, 590, "依次选择地图、玩家分队和 AI 策略", "white"),
        (685, 805, "准备阶段\n收入、商店、购买、部署、合成、出售、复活", PALE_BLUE),
        (900, 1020, "固定 1/60 秒战斗\n移动、索敌、攻击或治疗、Spine 动画", PALE_PURPLE),
        (1115, 1235, "回合结算\n回写单位状态与守卫伤害", PALE_GREEN),
        (1330, 1430, "结果页面\n再来一局或返回主菜单", PALE_BLUE),
    ]
    for top, bottom, label, fill in nodes:
        rounded(draw, (x1, top, x2, bottom), label, fill=fill, size=27)

    diamond(draw, (500, 275), 700, 110, "EXE 同级 data、字体、SFML DLL 是否可用？", size=23)
    arrow(draw, (500, 215), (500, 220))
    arrow(draw, (500, 330), (500, 315))
    draw.text((810, 235), "否：退出", font=font(20), fill="#B04444")
    arrow(draw, (815, 275), (930, 275), color="#B04444", width=3)

    for start_y, end_y in [(405, 500), (590, 685), (805, 900), (1020, 1115)]:
        arrow(draw, (500, start_y), (500, end_y))

    diamond(draw, (500, 1280), 640, 105, "守卫归零或已经完成第 3 回合？", size=24)
    arrow(draw, (500, 1235), (500, 1228))
    arrow(draw, (500, 1332), (500, 1330))
    draw.text((535, 1300), "是", font=font(20), fill="#297A4A")

    draw.line([(180, 1280), (95, 1280), (95, 745), (190, 745)], fill="#297A4A", width=4)
    arrow(draw, (95, 745), (190, 745), color="#297A4A")
    draw.text((105, 1240), "否：进入下一回合", font=font(20), fill="#297A4A")
    save(image, "game-flow-current.png")


def module_diagram():
    image = Image.new("RGB", (1600, 1000), "white")
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 1600, 1000), fill=PALE_GRAY)
    centered(draw, (40, 20, 1560, 85), "自走棋对战系统模块与依赖", size=50, fill=NAVY)
    centered(draw, (60, 82, 1540, 126), "C++17 + SFML 2.6.2 + Spine Runtime 3.8", size=24, fill=BLUE)

    bands = [
        (150, 315, PALE_BLUE, "表示层  AutoChessGame", ["GameApp\n窗口与固定步长", "Screen 页面\n菜单 / 帮助 / 对局", "Rendering\n地图 / 单位 / 坐标", "UI 与 Animation\n按钮 / Spine 动作"]),
        (350, 515, PALE_PURPLE, "输入与控制器", ["HumanController\n鼠标操作转命令", "AiController\n只读快照转命令", "IAiStrategy\n进攻 / 防守 / 路线"]),
        (550, 715, PALE_GREEN, "核心规则层  AutoChessCore", ["Match / RoundController\n阶段与回合编排", "Economy\n商店 / 部署 / 合成 / 复活", "Combat\n移动 / 索敌 / 攻击 / 结算", "Config / Model\n解析 / 校验 / 状态"]),
        (750, 915, PALE_ORANGE, "外部依赖与运行数据", ["AutoChessSpine\nspine-cpp + spine-sfml", "SFML 导入目标\ngraphics / window / system", "data 配置与地图\n3 cfg + 4 map", "字体与动画\n1 font + 30 files"]),
    ]

    for top, bottom, fill, label, items in bands:
        draw.rounded_rectangle((70, top, 1530, bottom), radius=22, fill=fill, outline=LINE, width=3)
        draw.text((92, top + 15), label, font=font(25), fill=NAVY)
        count = len(items)
        gap = 28
        inner_left, inner_right = 100, 1500
        box_width = (inner_right - inner_left - gap * (count - 1)) / count
        for idx, item in enumerate(items):
            left = int(inner_left + idx * (box_width + gap))
            right = int(left + box_width)
            rounded(draw, (left, top + 60, right, bottom - 22), item, fill="white", size=22, radius=14, width=2)

    for y1, y2 in [(315, 350), (515, 550), (715, 750)]:
        arrow(draw, (800, y1), (800, y2), width=5)
    centered(draw, (80, 930, 1520, 980), "统一数据流  控制器提交 GameCommand  →  Match 校验推进  →  ReadOnlyGameView 返回界面与 AI", size=24, fill=NAVY)
    save(image, "system-modules-current.png")


def class_diagram():
    image = Image.new("RGB", (1600, 1110), "white")
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 1600, 1110), fill=PALE_GRAY)
    centered(draw, (40, 20, 1560, 85), "核心类层次与多态关系", size=50, fill=NAVY)
    centered(draw, (60, 82, 1540, 122), "当前版本采用抽象类、单继承与接口多态", size=23, fill=BLUE)

    panels = [
        (55, 145, 780, 555, PALE_BLUE, "单位层次"),
        (820, 145, 1545, 555, PALE_PURPLE, "策略多态"),
        (55, 590, 780, 1010, PALE_GREEN, "控制器多态"),
        (820, 590, 1545, 1010, PALE_ORANGE, "页面与动画"),
    ]
    for x1, y1, x2, y2, fill, label in panels:
        draw.rounded_rectangle((x1, y1, x2, y2), radius=20, fill=fill, outline=LINE, width=3)
        draw.text((x1 + 20, y1 + 16), label, font=font(26), fill=NAVY)

    rounded(draw, (275, 205, 560, 300), "Unit\n<<abstract>>\n+ role() = 0", fill="white", size=21)
    child_labels = ["IronGuardUnit\n铁卫", "DuelistUnit\n决斗者", "RangerUnit\n游侠", "ArcanistUnit\n奥术师", "MedicUnit\n医师"]
    for idx, label in enumerate(child_labels):
        left = 80 + idx * 136
        rounded(draw, (left, 420, left + 120, 510), label, fill="white", size=16, radius=12, width=2)
        draw.line([(417, 335), (left + 60, 335), (left + 60, 420)], fill=LINE, width=3)
        arrow(draw, (417, 335), (417, 300), width=3)

    rounded(draw, (1035, 205, 1330, 300), "IAiStrategy\n<<interface>>", fill="white", size=22)
    for idx, label in enumerate(["OffensiveStrategy", "DefensiveStrategy", "RouteStrategy"]):
        top = 350 + idx * 62
        rounded(draw, (1000, top, 1365, top + 48), label, fill="white", size=19, radius=11, width=2)
        arrow(draw, (1182, top), (1182, 300), width=2)

    rounded(draw, (260, 655, 575, 750), "IPlayerController\n<<interface>>", fill="white", size=21)
    rounded(draw, (115, 865, 355, 945), "HumanController", fill="white", size=19)
    rounded(draw, (475, 865, 715, 945), "AiController", fill="white", size=19)
    draw.line([(417, 800), (235, 800), (235, 865)], fill=LINE, width=3)
    draw.line([(417, 800), (595, 800), (595, 865)], fill=LINE, width=3)
    arrow(draw, (417, 800), (417, 750), width=3)

    rounded(draw, (1015, 640, 1335, 725), "Screen\n<<abstract>>", fill="white", size=22)
    for idx, label in enumerate(["MainMenuScreen", "HelpScreen", "MatchScreen"]):
        left = 850 + idx * 225
        rounded(draw, (left, 825, left + 200, 895), label, fill="white", size=17, radius=12, width=2)
        draw.line([(1175, 770), (left + 100, 770), (left + 100, 825)], fill=LINE, width=3)
        arrow(draw, (1175, 770), (1175, 725), width=3)
    rounded(draw, (920, 930, 1170, 985), "SpineAssetRepository", fill="white", size=16, radius=10, width=2)
    rounded(draw, (1200, 930, 1480, 985), "UnitAnimationInstance", fill="white", size=16, radius=10, width=2)
    arrow(draw, (1170, 957), (1200, 957), width=2)
    save(image, "class-hierarchy-current.png")


if __name__ == "__main__":
    flowchart()
    module_diagram()
    class_diagram()
