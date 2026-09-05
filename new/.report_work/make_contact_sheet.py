from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_dir", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--columns", type=int, default=4)
    parser.add_argument("--thumb-width", type=int, default=300)
    args = parser.parse_args()

    paths = sorted(args.input_dir.glob("page-*.png"))
    if not paths:
        raise SystemExit("no page PNGs found")

    opened = [Image.open(path).convert("RGB") for path in paths]
    thumb_height = round(opened[0].height * args.thumb_width / opened[0].width)
    label_height = 30
    gap = 16
    rows = math.ceil(len(opened) / args.columns)
    width = gap + args.columns * (args.thumb_width + gap)
    height = gap + rows * (thumb_height + label_height + gap)
    sheet = Image.new("RGB", (width, height), "#d9d9d9")
    draw = ImageDraw.Draw(sheet)

    for index, (path, image) in enumerate(zip(paths, opened)):
        row, column = divmod(index, args.columns)
        x = gap + column * (args.thumb_width + gap)
        y = gap + row * (thumb_height + label_height + gap)
        thumb = image.resize((args.thumb_width, thumb_height), Image.Resampling.LANCZOS)
        sheet.paste(thumb, (x, y))
        draw.text((x, y + thumb_height + 5), path.stem, fill="black")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)


if __name__ == "__main__":
    main()
