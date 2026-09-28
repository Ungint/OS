#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_assets.py - Cong cu chuyen doi anh / video / font thuong sang cac
dinh dang RAW rieng ma kernel (image.c / video.c / font.c) doc duoc.

Yeu cau: pip install pillow

=== .img (dung cho loadimage/useimage) ===
Header 10 byte:
  magic[4] = "MIMG"
  width  : uint16 LE
  height : uint16 LE
  bpp    : uint8  (24 hoac 32)
  reserved: uint8 (=0)
Theo sau la width*height*(bpp/8) byte pixel R,G,B[,A], hang tren xuong duoi.

=== .vid (dung cho loadvideo/usevideo) ===
Header 14 byte:
  magic[4] = "MVID"
  width, height : uint16 LE
  bpp : uint8
  reserved : uint8 (=0)
  frame_count : uint16 LE
  delay_ms : uint16 LE
Theo sau la frame_count khung hinh, moi khung dung dinh dang pixel giong .img.

=== .f (dung cho loadfont / Font/*.f, kernel tu quet luc boot) ===
Header 8 byte:
  magic[4] = "MFNT"
  glyph_w  : uint8 (toi da 8)
  glyph_h  : uint8 (toi da 32)
  first_char: uint8 (thuong = 32, ky tu space)
  count    : uint8 (so glyph, thuong = 95 cho ASCII 32..126)
Theo sau la count*glyph_h byte: moi hang cua moi glyph la 1 byte,
bit 7 (MSB) la pixel ben trai nhat.

Cach dung:
  python3 make_assets.py image input.png output.img [--width 320 --height 200] [--bpp 24]
  python3 make_assets.py video frame1.png frame2.png ... output.vid [--delay 100] [--width W --height H]
  python3 make_assets.py gif input.gif output.vid [--delay 100] [--width W --height H]
  python3 make_assets.py font output.f [--ttf path/to/font.ttf] [--size 16] [--height 16]
"""
import sys
import struct
import argparse

from PIL import Image, ImageFont, ImageDraw


def encode_pixels(img: Image.Image, bpp: int) -> bytes:
    if bpp == 24:
        img = img.convert("RGB")
    else:
        img = img.convert("RGBA")
    return img.tobytes()


def cmd_image(args):
    img = Image.open(args.input)
    if args.width and args.height:
        img = img.resize((args.width, args.height))
    w, h = img.size
    bpp = args.bpp
    data = encode_pixels(img, bpp)

    with open(args.output, "wb") as f:
        f.write(b"MIMG")
        f.write(struct.pack("<HHBB", w, h, bpp, 0))
        f.write(data)

    print(f"[image] {args.output}: {w}x{h} bpp={bpp} ({len(data)} byte pixel)")


def cmd_video(args):
    frames = [Image.open(p) for p in args.frames]
    if args.width and args.height:
        frames = [f.resize((args.width, args.height)) for f in frames]
    w, h = frames[0].size
    for f in frames:
        if f.size != (w, h):
            sys.exit(f"Loi: tat ca khung hinh phai cung kich thuoc ({w}x{h}), thay {f.size}")

    bpp = args.bpp
    with open(args.output, "wb") as out:
        out.write(b"MVID")
        out.write(struct.pack("<HHBBHH", w, h, bpp, 0, len(frames), args.delay))
        for fr in frames:
            out.write(encode_pixels(fr, bpp))

    print(f"[video] {args.output}: {w}x{h} bpp={bpp} frames={len(frames)} delay={args.delay}ms")


def cmd_gif(args):
    im = Image.open(args.input)
    frames = []
    delay = args.delay
    try:
        i = 0
        while True:
            im.seek(i)
            frames.append(im.convert("RGBA").copy())
            if "duration" in im.info and i == 0:
                delay = im.info["duration"] or args.delay
            i += 1
    except EOFError:
        pass

    if args.width and args.height:
        frames = [f.resize((args.width, args.height)) for f in frames]
    w, h = frames[0].size
    bpp = args.bpp

    with open(args.output, "wb") as out:
        out.write(b"MVID")
        out.write(struct.pack("<HHBBHH", w, h, bpp, 0, len(frames), delay))
        for fr in frames:
            out.write(encode_pixels(fr, bpp))

    print(f"[gif->video] {args.output}: {w}x{h} bpp={bpp} frames={len(frames)} delay={delay}ms")


def cmd_font(args):
    glyph_w = 8
    glyph_h = args.height
    first_char = 32
    count = 95  # ASCII 32..126

    if args.ttf:
        font = ImageFont.truetype(args.ttf, args.size)
    else:
        font = ImageFont.load_default()

    out_rows = bytearray()
    for code in range(first_char, first_char + count):
        ch = chr(code)
        canvas = Image.new("L", (glyph_w, glyph_h), 0)
        draw = ImageDraw.Draw(canvas)
        draw.text((0, 0), ch, fill=255, font=font)
        px = canvas.load()
        for row in range(glyph_h):
            byte = 0
            for col in range(glyph_w):
                if px[col, row] > 128:
                    byte |= (1 << (7 - col))
            out_rows.append(byte)

    with open(args.output, "wb") as f:
        f.write(b"MFNT")
        f.write(struct.pack("<BBBB", glyph_w, glyph_h, first_char, count))
        f.write(bytes(out_rows))

    print(f"[font] {args.output}: {glyph_w}x{glyph_h} first_char={first_char} count={count}")


def main():
    p = argparse.ArgumentParser(description="Tao file .img/.vid/.f cho kernel")
    sub = p.add_subparsers(dest="mode", required=True)

    pi = sub.add_parser("image")
    pi.add_argument("input")
    pi.add_argument("output")
    pi.add_argument("--width", type=int, default=None)
    pi.add_argument("--height", type=int, default=None)
    pi.add_argument("--bpp", type=int, default=24, choices=[24, 32])
    pi.set_defaults(func=cmd_image)

    pv = sub.add_parser("video")
    pv.add_argument("frames", nargs="+", help="anh khung hinh 1 2 3 ... roi output.vid o cuoi")
    pv.add_argument("--delay", type=int, default=100)
    pv.add_argument("--width", type=int, default=None)
    pv.add_argument("--height", type=int, default=None)
    pv.add_argument("--bpp", type=int, default=24, choices=[24, 32])
    pv.set_defaults(func=lambda a: (
        setattr(a, "output", a.frames.pop()), cmd_video(a))[1])

    pg = sub.add_parser("gif")
    pg.add_argument("input")
    pg.add_argument("output")
    pg.add_argument("--delay", type=int, default=100)
    pg.add_argument("--width", type=int, default=None)
    pg.add_argument("--height", type=int, default=None)
    pg.add_argument("--bpp", type=int, default=24, choices=[24, 32])
    pg.set_defaults(func=cmd_gif)

    pf = sub.add_parser("font")
    pf.add_argument("output")
    pf.add_argument("--ttf", default=None, help="duong dan file .ttf (khong co thi dung font mac dinh PIL)")
    pf.add_argument("--size", type=int, default=12)
    pf.add_argument("--height", type=int, default=16, help="chieu cao glyph (<=32)")
    pf.set_defaults(func=cmd_font)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
