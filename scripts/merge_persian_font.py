#!/usr/bin/env python3
"""Add Persian/Arabic glyphs to a Latin TTF family, for dictionary lookups.

CrossPoint draws dictionary definitions in the reading font. A Latin-only font
shows '?' for Persian, so this copies the Arabic-script glyphs (base letters
and the presentation forms the reader's shaping uses) from a Persian font such
as Vazirmatn into each style of your reading font. Latin glyphs are untouched,
so books look the same. Persian glyphs are scaled to the target unitsPerEm and
lose their hinting instructions (they depend on the source font's programs).

Usage:
  python3 scripts/merge_persian_font.py <latin_dir> <LatinFamily> <fa_dir> <out_dir> <OutFamily>
  e.g. python3 scripts/merge_persian_font.py ~/Literata/ttf Literata ~/Vazirmatn/ttf ./LiterataFa LiterataFa

Expects <latin_dir>/<LatinFamily>-{Regular,Italic,Bold,BoldItalic}.ttf and
<fa_dir>/Vazirmatn-{Regular,Bold}.ttf. Copy <out_dir> to /fonts/<OutFamily>/ on
the SD card and pick <OutFamily> in Settings > Reader > Text Settings. Needs
fontTools (pip install fonttools).
"""
import copy
import sys

from fontTools.ttLib import TTFont
from fontTools.ttLib.scaleUpem import scale_upem
from fontTools.ttLib.tables import ttProgram

RANGES = [(0x0600, 0x06FF), (0x0750, 0x077F), (0xFB50, 0xFDFF), (0xFE70, 0xFEFF), (0x200C, 0x200F)]


def merge(latin_path, fa_path, out_path, family):
    t = TTFont(latin_path)
    s = TTFont(fa_path)
    if s["head"].unitsPerEm != t["head"].unitsPerEm:
        scale_upem(s, t["head"].unitsPerEm)
    tcmap = t.getBestCmap()
    scmap = s.getBestCmap()
    cps = [c for a, b in RANGES for c in range(a, b + 1) if c in scmap and c not in tcmap]
    order = list(t.getGlyphOrder())
    have = set(order)
    mapped = {}
    sg, tg = s["glyf"], t["glyf"]

    def copy_glyph(name):
        if name in mapped:
            return mapped[name]
        new = "fa_" + name
        while new in have:
            new += "_"
        g = copy.deepcopy(sg[name])
        if g.isComposite():
            for comp in g.components:
                comp.glyphName = copy_glyph(comp.glyphName)
        if hasattr(g, "program"):
            g.program = ttProgram.Program()
            g.program.fromBytecode(b"")
        tg[new] = g
        t["hmtx"][new] = s["hmtx"][name]
        order.append(new)
        have.add(new)
        mapped[name] = new
        return new

    for c in cps:
        copy_glyph(scmap[c])
    t.setGlyphOrder(order)
    for st in t["cmap"].tables:
        if st.isUnicode():
            for c in cps:
                if st.format == 4 and c > 0xFFFF:
                    continue
                st.cmap[c] = mapped[scmap[c]]
    for tag in ("hdmx", "LTSH", "VDMX"):  # per-glyph tables that would be stale
        if tag in t:
            del t[tag]
    if "post" in t:
        t["post"].formatType = 2.0
    for rec in t["name"].names:
        if rec.nameID in (1, 16):
            rec.string = family
    t.save(out_path)
    return len(cps)


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    latin_dir, latin_family, fa_dir, out_dir, out_family = sys.argv[1:6]
    for style, fa_style in (("Regular", "Regular"), ("Italic", "Regular"), ("Bold", "Bold"), ("BoldItalic", "Bold")):
        n = merge(f"{latin_dir}/{latin_family}-{style}.ttf", f"{fa_dir}/Vazirmatn-{fa_style}.ttf",
                  f"{out_dir}/{out_family}-{style}.ttf", out_family)
        print(f"{out_family}-{style}.ttf: added {n} code points")


if __name__ == "__main__":
    main()
