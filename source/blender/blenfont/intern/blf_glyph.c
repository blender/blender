/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup blf
 *
 * Glyph rendering, texturing and caching. Wraps Freetype and OpenGL functions.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_ADVANCES_H /* For FT_Get_Advance. */

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_threads.h"

#include "BLF_api.h"

#include "GPU_capabilities.h"
#include "GPU_immediate.h"

#include "blf_internal.h"
#include "blf_internal_types.h"

#include "BLI_math_vector.h"
#include "BLI_strict_flags.h"
#include "BLI_string_utf8.h"

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * Convert a floating point value to a FreeType 16.16 fixed point value.
 */
static FT_Fixed to_16dot16(double val)
{
  return (FT_Fixed)(lround(val * 65536.0));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Cache
 * \{ */

static GlyphCacheBLF *blf_glyph_cache_find(FontBLF *font, float size, unsigned int dpi)
{
  GlyphCacheBLF *gc = (GlyphCacheBLF *)font->cache.first;
  while (gc) {
    if (gc->size == size && gc->dpi == dpi && (gc->bold == ((font->flags & BLF_BOLD) != 0)) &&
        (gc->italic == ((font->flags & BLF_ITALIC) != 0))) {
      return gc;
    }
    gc = gc->next;
  }
  return NULL;
}

static GlyphCacheBLF *blf_glyph_cache_new(FontBLF *font)
{
  GlyphCacheBLF *gc = (GlyphCacheBLF *)MEM_callocN(sizeof(GlyphCacheBLF), "blf_glyph_cache_new");

  gc->next = NULL;
  gc->prev = NULL;
  gc->size = font->size;
  gc->dpi = font->dpi;
  gc->bold = ((font->flags & BLF_BOLD) != 0);
  gc->italic = ((font->flags & BLF_ITALIC) != 0);

  memset(gc->glyph_ascii_table, 0, sizeof(gc->glyph_ascii_table));
  memset(gc->bucket, 0, sizeof(gc->bucket));

  /* Determine ideal fixed-width size for monospaced output. */
  FT_UInt gindex = FT_Get_Char_Index(font->face, U'0');
  if (gindex) {
    FT_Fixed advance = 0;
    FT_Get_Advance(font->face, gindex, FT_LOAD_NO_HINTING, &advance);
    /* Use CSS 'ch unit' width, advance of zero character. */
    gc->fixed_width = (int)(advance >> 16);
  }
  else {
    /* Font does not contain "0" so use CSS fallback of 1/2 of em. */
    gc->fixed_width = (int)((font->face->size->metrics.height / 2) >> 6);
  }
  if (gc->fixed_width < 1) {
    gc->fixed_width = 1;
  }

  BLI_addhead(&font->cache, gc);
  return gc;
}

GlyphCacheBLF *blf_glyph_cache_acquire(FontBLF *font)
{
  BLI_spin_lock(font->glyph_cache_mutex);

  GlyphCacheBLF *gc = blf_glyph_cache_find(font, font->size, font->dpi);

  if (!gc) {
    gc = blf_glyph_cache_new(font);
  }

  return gc;
}

void blf_glyph_cache_release(FontBLF *font)
{
  BLI_spin_unlock(font->glyph_cache_mutex);
}

static void blf_glyph_cache_free(GlyphCacheBLF *gc)
{
  GlyphBLF *g;
  for (uint i = 0; i < ARRAY_SIZE(gc->bucket); i++) {
    while ((g = BLI_pophead(&gc->bucket[i]))) {
      blf_glyph_free(g);
    }
  }
  if (gc->texture) {
    GPU_texture_free(gc->texture);
  }
  if (gc->bitmap_result) {
    MEM_freeN(gc->bitmap_result);
  }
  MEM_freeN(gc);
}

void blf_glyph_cache_clear(FontBLF *font)
{
  GlyphCacheBLF *gc;

  BLI_spin_lock(font->glyph_cache_mutex);

  while ((gc = BLI_pophead(&font->cache))) {
    blf_glyph_cache_free(gc);
  }

  BLI_spin_unlock(font->glyph_cache_mutex);
}

/**
 * Try to find a glyph in cache.
 *
 * \return NULL if not found.
 */
static GlyphBLF *blf_glyph_cache_find_glyph(GlyphCacheBLF *gc, uint charcode)
{
  if (charcode < GLYPH_ASCII_TABLE_SIZE) {
    return gc->glyph_ascii_table[charcode];
  }

  GlyphBLF *g = gc->bucket[blf_hash(charcode)].first;
  while (g) {
    if (g->c == charcode) {
      return g;
    }
    g = g->next;
  }
  return NULL;
}

/**
 * Add a rendered glyph to a cache.
 */
static GlyphBLF *blf_glyph_cache_add_glyph(
    FontBLF *font, GlyphCacheBLF *gc, FT_GlyphSlot glyph, uint charcode, FT_UInt glyph_index)
{
  GlyphBLF *g = (GlyphBLF *)MEM_callocN(sizeof(GlyphBLF), "blf_glyph_get");
  g->c = charcode;
  g->idx = glyph_index;
  g->advance_x = (ft_pix)glyph->advance.x;
  g->pos[0] = glyph->bitmap_left;
  g->pos[1] = glyph->bitmap_top;
  g->dims[0] = (int)glyph->bitmap.width;
  g->dims[1] = (int)glyph->bitmap.rows;
  g->pitch = glyph->bitmap.pitch;

  FT_BBox bbox;
  FT_Outline_Get_CBox(&(glyph->outline), &bbox);
  g->box_xmin = (ft_pix)bbox.xMin;
  g->box_xmax = (ft_pix)bbox.xMax;
  g->box_ymin = (ft_pix)bbox.yMin;
  g->box_ymax = (ft_pix)bbox.yMax;

  /* Used to improve advance when hinting is enabled. */
  g->lsb_delta = (ft_pix)glyph->lsb_delta;
  g->rsb_delta = (ft_pix)glyph->rsb_delta;

  const int buffer_size = (int)(glyph->bitmap.width * glyph->bitmap.rows);
  if (buffer_size != 0) {
    if (font->flags & BLF_MONOCHROME) {
      /* Font buffer uses only 0 or 1 values, Blender expects full 0..255 range. */
      for (int i = 0; i < buffer_size; i++) {
        glyph->bitmap.buffer[i] = glyph->bitmap.buffer[i] ? 255 : 0;
      }
    }
    g->bitmap = MEM_mallocN((size_t)buffer_size, "glyph bitmap");
    memcpy(g->bitmap, glyph->bitmap.buffer, (size_t)buffer_size);
  }

  unsigned int key = blf_hash(g->c);
  BLI_addhead(&(gc->bucket[key]), g);
  if (charcode < GLYPH_ASCII_TABLE_SIZE) {
    gc->glyph_ascii_table[charcode] = g;
  }

  return g;
}

/* This table can be used to find a coverage bit based on a charcode. later we can get default
 * language and script from codepoint.  */

typedef struct eUnicodeBlock {
  unsigned int first;
  unsigned int last;
  int coverage_bit; /* 0-122. -1 is N/A. */
  /* Later we add primary script and language for Harfbuzz, data from
   * https://en.wikipedia.org/wiki/Unicode_block */
} eUnicodeBlock;

static eUnicodeBlock unicode_blocks[] = {
    /* Must be in ascending order by start of range. */
    {0x0, 0x7F, 0},           /* Basic Latin. */
    {0x80, 0xFF, 1},          /* Latin-1 Supplement. */
    {0x100, 0x17F, 2},        /* Latin Extended-A. */
    {0x180, 0x24F, 3},        /* Latin Extended-B. */
    {0x250, 0x2AF, 4},        /* IPA Extensions. */
    {0x2B0, 0x2FF, 5},        /* Spacing Modifier Letters. */
    {0x300, 0x36F, 6},        /* Combining Diacritical Marks. */
    {0x370, 0x3FF, 7},        /* Greek. */
    {0x400, 0x52F, 9},        /* Cyrillic. */
    {0x530, 0x58F, 10},       /* Armenian. */
    {0x590, 0x5FF, 11},       /* Hebrew. */
    {0x600, 0x6FF, 13},       /* Arabic. */
    {0x700, 0x74F, 71},       /* Syriac. */
    {0x750, 0x77F, 13},       /* Arabic Supplement. */
    {0x780, 0x7BF, 72},       /* Thaana. */
    {0x7C0, 0x7FF, 14},       /* NKo. */
    {0x800, 0x83F, -1},       /* Samaritan. */
    {0x840, 0x85F, -1},       /* Mandaic. */
    {0x900, 0x97F, 15},       /* Devanagari. */
    {0x980, 0x9FF, 16},       /* Bengali. */
    {0xA00, 0xA7F, 17},       /* Gurmukhi. */
    {0xA80, 0xAFF, 18},       /* Gujarati. */
    {0xB00, 0xB7F, 19},       /* Oriya. */
    {0xB80, 0xBFF, 20},       /* Tamil. */
    {0xC00, 0xC7F, 21},       /* Telugu. */
    {0xC80, 0xCFF, 22},       /* Kannada. */
    {0xD00, 0xD7F, 23},       /* Malayalam. */
    {0xD80, 0xDFF, 73},       /* Sinhala. */
    {0xE00, 0xE7F, 24},       /* Thai. */
    {0xE80, 0xEFF, 25},       /* Lao. */
    {0xF00, 0xFFF, 70},       /* Tibetan. */
    {0x1000, 0x109F, 74},     /* Myanmar. */
    {0x10A0, 0x10FF, 26},     /* Georgian. */
    {0x1100, 0x11FF, 28},     /* Hangul Jamo. */
    {0x1200, 0x139F, 75},     /* Ethiopic. */
    {0x13A0, 0x13FF, 76},     /* Cherokee. */
    {0x1400, 0x167F, 77},     /* Canadian Aboriginal. */
    {0x1680, 0x169F, 78},     /* Ogham. */
    {0x16A0, 0x16FF, 79},     /* unic. */
    {0x1700, 0x171F, 84},     /* Tagalog. */
    {0x1720, 0x173F, 84},     /* Hanunoo. */
    {0x1740, 0x175F, 84},     /* Buhid. */
    {0x1760, 0x177F, 84},     /* Tagbanwa. */
    {0x1780, 0x17FF, 80},     /* Khmer. */
    {0x1800, 0x18AF, 81},     /* Mongolian. */
    {0x1900, 0x194F, 93},     /* Limbu. */
    {0x1950, 0x197F, 94},     /* Tai Le. */
    {0x1980, 0x19DF, 95},     /* New Tai Lue". */
    {0x19E0, 0x19FF, 80},     /* Khmer. */
    {0x1A00, 0x1A1F, 96},     /* Buginese. */
    {0x1A20, 0x1AAF, -1},     /* Tai Tham. */
    {0x1B00, 0x1B7F, 27},     /* Balinese. */
    {0x1B80, 0x1BBF, 112},    /* Sundanese. */
    {0x1BC0, 0x1BFF, -1},     /* Batak. */
    {0x1C00, 0x1C4F, 113},    /* Lepcha. */
    {0x1C50, 0x1C7F, 114},    /* Ol Chiki. */
    {0x1D00, 0x1DBF, 4},      /* IPA Extensions. */
    {0x1DC0, 0x1DFF, 6},      /* Combining Diacritical Marks. */
    {0x1E00, 0x1EFF, 29},     /* Latin Extended Additional. */
    {0x1F00, 0x1FFF, 30},     /* Greek Extended. */
    {0x2000, 0x206F, 31},     /* General Punctuation. */
    {0x2070, 0x209F, 32},     /* Superscripts And Subscripts. */
    {0x20A0, 0x20CF, 33},     /* Currency Symbols. */
    {0x20D0, 0x20FF, 34},     /* Combining Diacritical Marks For Symbols. */
    {0x2100, 0x214F, 35},     /* Letterlike Symbols. */
    {0x2150, 0x218F, 36},     /* Number Forms. */
    {0x2190, 0x21FF, 37},     /* Arrows. */
    {0x2200, 0x22FF, 38},     /* Mathematical Operators. */
    {0x2300, 0x23FF, 39},     /* Miscellaneous Technical. */
    {0x2400, 0x243F, 40},     /* Control Pictures. */
    {0x2440, 0x245F, 41},     /* Optical Character Recognition. */
    {0x2460, 0x24FF, 42},     /* Enclosed Alphanumerics. */
    {0x2500, 0x257F, 43},     /* Box Drawing. */
    {0x2580, 0x259F, 44},     /* Block Elements. */
    {0x25A0, 0x25FF, 45},     /* Geometric Shapes. */
    {0x2600, 0x26FF, 46},     /* Miscellaneous Symbols. */
    {0x2700, 0x27BF, 47},     /* Dingbats. */
    {0x27C0, 0x27EF, 38},     /* Mathematical Operators. */
    {0x27F0, 0x27FF, 37},     /* Arrows. */
    {0x2800, 0x28FF, 82},     /* Braille. */
    {0x2900, 0x297F, 37},     /* Arrows. */
    {0x2980, 0x2AFF, 38},     /* Mathematical Operators. */
    {0x2B00, 0x2BFF, 37},     /* Arrows. */
    {0x2C00, 0x2C5F, 97},     /* Glagolitic. */
    {0x2C60, 0x2C7F, 29},     /* Latin Extended Additional. */
    {0x2C80, 0x2CFF, 8},      /* Coptic. */
    {0x2D00, 0x2D2F, 26},     /* Georgian. */
    {0x2D30, 0x2D7F, 98},     /* Tifinagh. */
    {0x2D80, 0x2DDF, 75},     /* Ethiopic. */
    {0x2DE0, 0x2DFF, 9},      /* Cyrillic. */
    {0x2E00, 0x2E7F, 31},     /* General Punctuation. */
    {0x2E80, 0x2FFF, 59},     /* CJK Unified Ideographs. */
    {0x3000, 0x303F, 48},     /* CJK Symbols And Punctuation. */
    {0x3040, 0x309F, 49},     /* Hiragana. */
    {0x30A0, 0x30FF, 50},     /* Katakana. */
    {0x3100, 0x312F, 51},     /* Bopomofo. */
    {0x3130, 0x318F, 52},     /* Hangul Compatibility Jamo. */
    {0x3190, 0x319F, 59},     /* CJK Unified Ideographs. */
    {0x31A0, 0x31BF, 51},     /* Bopomofo. */
    {0x31C0, 0x31EF, 59},     /* CJK Unified Ideographs. */
    {0x31F0, 0x31FF, 50},     /* Katakana. */
    {0x3200, 0x32FF, 54},     /* Enclosed CJK Letters And Months. */
    {0x3300, 0x33FF, 55},     /* CJK Compatibility. */
    {0x3400, 0x4DBF, 59},     /* CJK Unified Ideographs. */
    {0x4DC0, 0x4DFF, 99},     /* Yijing. */
    {0x4E00, 0x9FFF, 59},     /* CJK Unified Ideographs. */
    {0xA000, 0xA4CF, 83},     /* Yi. */
    {0xA4D0, 0xA4FF, -1},     /* Lisu. */
    {0xA500, 0xA63F, 12},     /* Vai. */
    {0xA640, 0xA69F, 9},      /* Cyrillic. */
    {0xA6A0, 0xA6FF, -1},     /* Bamum. */
    {0xA700, 0xA71F, 5},      /* Spacing Modifier Letters. */
    {0xA720, 0xA7FF, 29},     /* Latin Extended Additional. */
    {0xA800, 0xA82F, 100},    /* Syloti Nagri. */
    {0xA840, 0xA87F, 53},     /* Phags-pa. */
    {0xA880, 0xA8DF, 115},    /* Saurashtra. */
    {0xA900, 0xA92F, 116},    /* Kayah Li. */
    {0xA930, 0xA95F, 117},    /* Rejang. */
    {0xA960, 0xA97F, 56},     /* Hangul Syllables. */
    {0xA980, 0xA9DF, -1},     /* Javanese. */
    {0xA9E0, 0xA9FF, 74},     /* Myanmar. */
    {0xAA00, 0xAA5F, 118},    /* Cham. */
    {0xAA60, 0xAA7F, 74},     /* Myanmar. */
    {0xAA80, 0xAADF, -1},     /* Tai Viet. */
    {0xAAE0, 0xAAFF, -1},     /* Meetei Mayek. */
    {0xAB00, 0xAB2F, 75},     /* Ethiopic. */
    {0xAB70, 0xABBF, 76},     /* Cherokee. */
    {0xABC0, 0xABFF, -1},     /* Meetei Mayek. */
    {0xAC00, 0xD7AF, 56},     /* Hangul Syllables. */
    {0xD800, 0xDFFF, 57},     /* Non-Plane 0. */
    {0xE000, 0xF6FF, 60},     /* Private Use Area. */
    {0xE700, 0xEFFF, -1},     /* MS Wingdings. */
    {0xF000, 0xF8FF, -1},     /* MS Symbols. */
    {0xF900, 0xFAFF, 61},     /* CJK Compatibility Ideographs. */
    {0xFB00, 0xFB4F, 62},     /* Alphabetic Presentation Forms. */
    {0xFB50, 0xFDFF, 63},     /* Arabic Presentation Forms-A. */
    {0xFE00, 0xFE0F, 91},     /* Variation Selectors. */
    {0xFE10, 0xFE1F, 65},     /* CJK Compatibility Forms. */
    {0xFE20, 0xFE2F, 64},     /* Combining Half Marks. */
    {0xFE30, 0xFE4F, 65},     /* CJK Compatibility Forms. */
    {0xFE50, 0xFE6F, 66},     /* Small Form Variants. */
    {0xFE70, 0xFEFF, 67},     /* Arabic Presentation Forms-B. */
    {0xFF00, 0xFFEF, 68},     /* Halfwidth And Fullwidth Forms. */
    {0xFFF0, 0xFFFF, 69},     /* Specials. */
    {0x10000, 0x1013F, 101},  /* Linear B. */
    {0x10140, 0x1018F, 102},  /* Ancient Greek Numbers. */
    {0x10190, 0x101CF, 119},  /* Ancient Symbols. */
    {0x101D0, 0x101FF, 120},  /* Phaistos Disc. */
    {0x10280, 0x1029F, 121},  /* Lycian. */
    {0x102A0, 0x102DF, 121},  /* Carian. */
    {0x10300, 0x1032F, 85},   /* Old Italic. */
    {0x10330, 0x1034F, 86},   /* Gothic. */
    {0x10350, 0x1037F, -1},   /* Old Permic. */
    {0x10380, 0x1039F, 103},  /* Ugaritic. */
    {0x103A0, 0x103DF, 104},  /* Old Persian. */
    {0x10400, 0x1044F, 87},   /* Deseret. */
    {0x10450, 0x1047F, 105},  /* Shavian. */
    {0x10480, 0x104AF, 106},  /* Osmanya. */
    {0x104B0, 0x104FF, -1},   /* Osage. */
    {0x10500, 0x1052F, -1},   /* Elbasan. */
    {0x10530, 0x1056F, -1},   /* Caucasian Albanian. */
    {0x10570, 0x105BF, -1},   /* Vithkuqi. */
    {0x10600, 0x1077F, -1},   /* Linear A. */
    {0x10780, 0x107BF, 3},    /* Latin Extended-B. */
    {0x10800, 0x1083F, 107},  /* Cypriot Syllabary. */
    {0x10840, 0x1085F, -1},   /* Imperial Aramaic. */
    {0x10860, 0x1087F, -1},   /* Palmyrene. */
    {0x10880, 0x108AF, -1},   /* Nabataean. */
    {0x108E0, 0x108FF, -1},   /* Hatran. */
    {0x10900, 0x1091F, 58},   /* Phoenician. */
    {0x10920, 0x1093F, 121},  /* Lydian. */
    {0x10980, 0x1099F, -1},   /* Meroitic Hieroglyphs. */
    {0x109A0, 0x109FF, -1},   /* Meroitic Cursive. */
    {0x10A00, 0x10A5F, 108},  /* Kharoshthi. */
    {0x10A60, 0x10A7F, -1},   /* Old South Arabian. */
    {0x10A80, 0x10A9F, -1},   /* Old North Arabian. */
    {0x10AC0, 0x10AFF, -1},   /* Manichaean. */
    {0x10B00, 0x10B3F, -1},   /* Avestan. */
    {0x10B40, 0x10B5F, -1},   /* Inscriptional Parthian. */
    {0x10B60, 0x10B7F, -1},   /* Inscriptional Pahlavi. */
    {0x10B80, 0x10BAF, -1},   /* Psalter Pahlavi. */
    {0x10C00, 0x10C4F, -1},   /* Old Turkic. */
    {0x10C80, 0x10CFF, -1},   /* Old Hungarian. */
    {0x10D00, 0x10D3F, -1},   /* Hanifi Rohingya. */
    {0x108E0, 0x10E7F, -1},   /* Rumi Numeral Symbols. */
    {0x10E80, 0x10EBF, -1},   /* Yezidi. */
    {0x10F00, 0x10F2F, -1},   /* Old Sogdian. */
    {0x10F30, 0x10F6F, -1},   /* Sogdian. */
    {0x10F70, 0x10FAF, -1},   /* Old Uyghur. */
    {0x10FB0, 0x10FDF, -1},   /* Chorasmian. */
    {0x10FE0, 0x10FFF, -1},   /* Elymaic. */
    {0x11000, 0x1107F, -1},   /* Brahmi. */
    {0x11080, 0x110CF, -1},   /* Kaithi. */
    {0x110D0, 0x110FF, -1},   /* Sora Sompeng. */
    {0x11100, 0x1114F, -1},   /* Chakma. */
    {0x11150, 0x1117F, -1},   /* Mahajani. */
    {0x11180, 0x111DF, -1},   /* Sharada. */
    {0x111E0, 0x111FF, -1},   /* Sinhala Archaic Numbers. */
    {0x11200, 0x1124F, -1},   /* Khojki. */
    {0x11280, 0x112AF, -1},   /* Multani. */
    {0x112B0, 0x112FF, -1},   /* Khudawadi. */
    {0x11300, 0x1137F, -1},   /* Grantha. */
    {0x11400, 0x1147F, -1},   /* Newa. */
    {0x11480, 0x114DF, -1},   /* Tirhuta. */
    {0x11580, 0x115FF, -1},   /* Siddham. */
    {0x11600, 0x1165F, -1},   /* Modi. */
    {0x11660, 0x1167F, 81},   /* Mongolian. */
    {0x11680, 0x116CF, -1},   /* Takri. */
    {0x11700, 0x1174F, -1},   /* Ahom. */
    {0x11800, 0x1184F, -1},   /* Dogra. */
    {0x118A0, 0x118FF, -1},   /* Warang Citi. */
    {0x11900, 0x1195F, -1},   /* Dives Akuru. */
    {0x119A0, 0x119FF, -1},   /* Nandinagari. */
    {0x11A00, 0x11A4F, -1},   /* Zanabazar Square. */
    {0x11A50, 0x11AAF, -1},   /* Soyombo. */
    {0x11AB0, 0x11ABF, 77},   /* Canadian Aboriginal Syllabics. */
    {0x11AC0, 0x11AFF, -1},   /* Pau Cin Hau. */
    {0x11C00, 0x11C6F, -1},   /* Bhaiksuki. */
    {0x11C70, 0x11CBF, -1},   /* Marchen. */
    {0x11D00, 0x11D5F, -1},   /* Masaram Gondi. */
    {0x11D60, 0x11DAF, -1},   /* Gunjala Gondi. */
    {0x11EE0, 0x11EFF, -1},   /* Makasar. */
    {0x11FB0, 0x11FBF, -1},   /* Lisu. */
    {0x11FC0, 0x11FFF, 20},   /* Tamil. */
    {0x12000, 0x1254F, 110},  /* Cuneiform. */
    {0x12F90, 0x12FFF, -1},   /* Cypro-Minoan. */
    {0x13000, 0x1343F, -1},   /* Egyptian Hieroglyphs. */
    {0x14400, 0x1467F, -1},   /* Anatolian Hieroglyphs. */
    {0x16800, 0x16A3F, -1},   /* Bamum. */
    {0x16A40, 0x16A6F, -1},   /* Mro. */
    {0x16A70, 0x16ACF, -1},   /* Tangsa. */
    {0x16AD0, 0x16AFF, -1},   /* Bassa Vah. */
    {0x16B00, 0x16B8F, -1},   /* Pahawh Hmong. */
    {0x16E40, 0x16E9F, -1},   /* Medefaidrin. */
    {0x16F00, 0x16F9F, -1},   /* Miao. */
    {0x16FE0, 0x16FFF, -1},   /* Ideographic Symbols. */
    {0x17000, 0x18AFF, -1},   /* Tangut. */
    {0x1B170, 0x1B2FF, -1},   /* Nushu. */
    {0x1BC00, 0x1BC9F, -1},   /* Duployan. */
    {0x1D000, 0x1D24F, 88},   /* Musical Symbols. */
    {0x1D2E0, 0x1D2FF, -1},   /* Mayan Numerals. */
    {0x1D300, 0x1D35F, 109},  /* Tai Xuan Jing. */
    {0x1D360, 0x1D37F, 111},  /* Counting Rod Numerals. */
    {0x1D400, 0x1D7FF, 89},   /* Mathematical Alphanumeric Symbols. */
    {0x1E2C0, 0x1E2FF, -1},   /* Wancho. */
    {0x1E800, 0x1E8DF, -1},   /* Mende Kikakui. */
    {0x1E900, 0x1E95F, -1},   /* Adlam. */
    {0x1EC70, 0x1ECBF, -1},   /* Indic Siyaq Numbers. */
    {0x1F000, 0x1F02F, 122},  /* Mahjong Tiles. */
    {0x1F030, 0x1F09F, 122},  /* Domino Tiles. */
    {0x1F600, 0x1F64F, -1},   /* Emoticons. */
    {0x20000, 0x2A6DF, 59},   /* CJK Unified Ideographs. */
    {0x2F800, 0x2FA1F, 61},   /* CJK Compatibility Ideographs. */
    {0xE0000, 0xE007F, 92},   /* Tags. */
    {0xE0100, 0xE01EF, 91},   /* Variation Selectors. */
    {0xF0000, 0x10FFFD, 90}}; /* Private Use Supplementary. */

/* Find a unicode block that a charcode belongs to. */
static eUnicodeBlock *blf_charcode_to_unicode_block(uint charcode)
{
  if (charcode < 0x80) {
    /* Shortcut to Basic Latin. */
    return &unicode_blocks[0];
  }

  /* Binary search for other blocks. */

  int min = 0;
  int max = ARRAY_SIZE(unicode_blocks) - 1;
  int mid;

  if (charcode < unicode_blocks[0].first || charcode > unicode_blocks[max].last) {
    return NULL;
  }

  while (max >= min) {
    mid = (min + max) / 2;
    if (charcode > unicode_blocks[mid].last) {
      min = mid + 1;
    }
    else if (charcode < unicode_blocks[mid].first) {
      max = mid - 1;
    }
    else {
      return &unicode_blocks[mid];
    }
  }

  return NULL;
}

static int blf_charcode_to_coverage_bit(uint charcode)
{
  int coverage_bit = -1;
  eUnicodeBlock *block = blf_charcode_to_unicode_block(charcode);
  if (block) {
    coverage_bit = block->coverage_bit;
  }

  if (coverage_bit < 0 && charcode > 0xFFFF) {
    /* No coverage bit, but OpenType specs v.1.3+ says bit 57 implies that there
     * are codepoints supported beyond the BMP, so only check fonts with this set. */
    coverage_bit = 57;
  }

  return coverage_bit;
}

static bool blf_font_has_coverage_bit(FontBLF *font, int coverage_bit)
{
  if (coverage_bit < 0) {
    return false;
  }
  return (font->UnicodeRanges[(uint)coverage_bit >> 5] & (1u << ((uint)coverage_bit % 32)));
}

/**
 * Return a glyph index from `charcode`. Not found returns zero, which is a valid
 * printable character (`.notdef` or `tofu`). Font is allowed to change here.
 */
static FT_UInt blf_glyph_index_from_charcode(FontBLF **font, const uint charcode)
{
  FT_UInt glyph_index = FT_Get_Char_Index((*font)->face, charcode);
  if (glyph_index) {
    return glyph_index;
  }

  /* Not found in main font, so look in the others. */
  FontBLF *last_resort = NULL;
  int coverage_bit = blf_charcode_to_coverage_bit(charcode);
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    FontBLF *f = global_font[i];
    if (!f || f == *font || !(f->flags & BLF_DEFAULT)) {
      continue;
    }

    if (f->flags & BLF_LAST_RESORT) {
      last_resort = f;
      continue;
    }
    if (coverage_bit < 0 || blf_font_has_coverage_bit(f, coverage_bit)) {
      glyph_index = FT_Get_Char_Index(f->face, charcode);
      if (glyph_index) {
        *font = f;
        return glyph_index;
      }
    }
  }

  /* Not found in the stack, return from Last Resort if there is one. */
  if (last_resort) {
    glyph_index = FT_Get_Char_Index(last_resort->face, charcode);
    if (glyph_index) {
      *font = last_resort;
      return glyph_index;
    }
  }

  return 0;
}

/**
 * Load a glyph into the glyph slot of a font's face object.
 */
static FT_GlyphSlot blf_glyph_load(FontBLF *font, FT_UInt glyph_index)
{
  int load_flags;

  if (font->flags & BLF_MONOCHROME) {
    load_flags = FT_LOAD_TARGET_MONO;
  }
  else {
    load_flags = FT_LOAD_NO_BITMAP;
    if (font->flags & BLF_HINTING_NONE) {
      load_flags |= FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
    }
    else if (font->flags & BLF_HINTING_SLIGHT) {
      load_flags |= FT_LOAD_TARGET_LIGHT;
    }
    else if (font->flags & BLF_HINTING_FULL) {
      load_flags |= FT_LOAD_TARGET_NORMAL;
    }
    else {
      /* Default, hinting disabled until FreeType has been upgraded
       * to give good results on all platforms. */
      load_flags |= FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
    }
  }

  if (FT_Load_Glyph(font->face, glyph_index, load_flags) == FT_Err_Ok) {
    return font->face->glyph;
  }
  return NULL;
}

/**
 * Convert a glyph from outlines to a bitmap that we can display.
 */
static bool blf_glyph_render_bitmap(FontBLF *font, FT_GlyphSlot glyph)
{
  int render_mode;

  if (font->flags & BLF_MONOCHROME) {
    render_mode = FT_RENDER_MODE_MONO;
  }
  else {
    render_mode = FT_RENDER_MODE_NORMAL;
  }

  /* Render the glyph curves to a bitmap. */
  FT_Error err = FT_Render_Glyph(glyph, render_mode);
  if (err != FT_Err_Ok) {
    return false;
  }

  FT_Bitmap tempbitmap;

  if (font->flags & BLF_MONOCHROME) {
    /* Convert result from 1 bit per pixel to 8 bit per pixel */
    /* Accumulate errors for later, fine if not interested beyond "ok vs any error" */
    FT_Bitmap_New(&tempbitmap);

    /* Does Blender use Pitch 1 always? It works so far */
    err += FT_Bitmap_Convert(font->ft_lib, &glyph->bitmap, &tempbitmap, 1);
    err += FT_Bitmap_Copy(font->ft_lib, &tempbitmap, &glyph->bitmap);
    err += FT_Bitmap_Done(font->ft_lib, &tempbitmap);
  }

  if (err || glyph->format != FT_GLYPH_FORMAT_BITMAP) {
    return false;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Transformations
 * \{ */

/**
 * Adjust the glyphs weight by a factor.
 *
 * \param factor: -1 (min stroke width) <= 0 (normal) => 1 (max boldness).
 */
static bool blf_glyph_transform_weight(FT_GlyphSlot glyph, float factor, bool monospaced)
{
  if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    /* Fake bold if the font does not have this variable axis. */
    const FT_Pos average_width = FT_MulFix(glyph->face->units_per_EM,
                                           glyph->face->size->metrics.x_scale);
    FT_Pos change = (FT_Pos)((float)average_width * factor * 0.1f);
    FT_Outline_EmboldenXY(&glyph->outline, change, change / 2);
    if (monospaced) {
      /* Widened fixed-pitch font needs a nudge left. */
      FT_Outline_Translate(&glyph->outline, change / -2, 0);
    }
    else {
      /* Need to increase advance. */
      glyph->advance.x += change;
      glyph->advance.y += change / 2;
    }
    return true;
  }
  return false;
}

/**
 * Adjust the glyphs slant by a factor (making it oblique).
 *
 * \param factor: -1 (max negative) <= 0 (no slant) => 1 (max positive).
 *
 * \note that left-leaning italics are possible in some RTL writing systems.
 */
static bool blf_glyph_transform_slant(FT_GlyphSlot glyph, float factor)
{
  if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    FT_Matrix transform = {to_16dot16(1), to_16dot16(factor / 2.0f), 0, to_16dot16(1)};
    FT_Outline_Transform(&glyph->outline, &transform);
    return true;
  }
  return false;
}

/**
 * Adjust the glyph width by factor.
 *
 * \param factor: -1 (min width) <= 0 (normal) => 1 (max width).
 */
static bool UNUSED_FUNCTION(blf_glyph_transform_width)(FT_GlyphSlot glyph, float factor)
{
  if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    float scale = (factor * 0.4f) + 1.0f; /* 0.6f - 1.4f */
    FT_Matrix matrix = {to_16dot16(scale), 0, 0, to_16dot16(1)};
    FT_Outline_Transform(&glyph->outline, &matrix);
    glyph->advance.x = (FT_Pos)((double)glyph->advance.x * scale);
    return true;
  }
  return false;
}

/**
 * Transform glyph to fit nicely within a fixed column width.
 */
static bool blf_glyph_transform_monospace(FT_GlyphSlot glyph, int width)
{
  if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    FT_Fixed current = glyph->linearHoriAdvance;
    FT_Fixed target = width << 16; /* Do math in 16.16 values. */
    if (target < current) {
      const FT_Pos embolden = (FT_Pos)((current - target) >> 13);
      /* Horizontally widen strokes to counteract narrowing. */
      FT_Outline_EmboldenXY(&glyph->outline, embolden, 0);
      const float scale = (float)(target - (embolden << 9)) / (float)current;
      FT_Matrix matrix = {to_16dot16(scale), 0, 0, to_16dot16(1)};
      FT_Outline_Transform(&glyph->outline, &matrix);
    }
    else if (target > current) {
      /* Center narrow glyphs. */
      FT_Outline_Translate(&glyph->outline, (FT_Pos)((target - current) >> 11), 0);
    }
    glyph->advance.x = width << 6;
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Access (Ensure/Free)
 * \{ */

/**
 * Create and return a fully-rendered bitmap glyph.
 */
static FT_GlyphSlot blf_glyph_render(FontBLF *settings_font,
                                     FontBLF *glyph_font,
                                     FT_UInt glyph_index,
                                     uint charcode,
                                     int fixed_width)
{
  if (glyph_font != settings_font) {
    FT_Set_Char_Size(glyph_font->face,
                     0,
                     ((FT_F26Dot6)(settings_font->size)) * 64,
                     settings_font->dpi,
                     settings_font->dpi);
    glyph_font->size = settings_font->size;
    glyph_font->dpi = settings_font->dpi;
  }

  FT_GlyphSlot glyph = blf_glyph_load(glyph_font, glyph_index);
  if (!glyph) {
    return NULL;
  }

  if ((settings_font->flags & BLF_MONOSPACED) && (settings_font != glyph_font)) {
    blf_glyph_transform_monospace(glyph, BLI_wcwidth((char32_t)charcode) * fixed_width);
  }

  if ((settings_font->flags & BLF_ITALIC) != 0) {
    /* 37.5% of maximum rightward slant results in 6 degree slope, matching italic
     * version (`DejaVuSans-Oblique.ttf`) of our current font. But a nice median when
     * checking others. Worth reevaluating if we change default font. We could also
     * narrow the glyph slightly as most italics do, but this one does not. */
    blf_glyph_transform_slant(glyph, 0.375f);
  }

  if ((settings_font->flags & BLF_BOLD) != 0) {
    /* 70% of maximum weight results in the same amount of boldness and horizontal
     * expansion as the bold version (`DejaVuSans-Bold.ttf`) of our default font.
     * Worth reevaluating if we change default font. */
    blf_glyph_transform_weight(glyph, 0.7f, glyph->face->face_flags & FT_FACE_FLAG_FIXED_WIDTH);
  }

  if (blf_glyph_render_bitmap(glyph_font, glyph)) {
    return glyph;
  }
  return NULL;
}

GlyphBLF *blf_glyph_ensure(FontBLF *font, GlyphCacheBLF *gc, uint charcode)
{
  GlyphBLF *g = blf_glyph_cache_find_glyph(gc, charcode);
  if (g) {
    return g;
  }

  /* Glyph might not come from the initial font. */
  FontBLF *font_with_glyph = font;
  FT_UInt glyph_index = blf_glyph_index_from_charcode(&font_with_glyph, charcode);

  /* Glyphs are dynamically created as needed by font rendering. this means that
   * to make font rendering thread safe we have to do locking here. note that this
   * must be a lock for the whole library and not just per font, because the font
   * renderer uses a shared buffer internally. */
  BLI_spin_lock(font_with_glyph->ft_lib_mutex);

  FT_GlyphSlot glyph = blf_glyph_render(
      font, font_with_glyph, glyph_index, charcode, gc->fixed_width);

  if (glyph) {
    /* Save this glyph in the initial font's cache. */
    g = blf_glyph_cache_add_glyph(font, gc, glyph, charcode, glyph_index);
  }

  BLI_spin_unlock(font_with_glyph->ft_lib_mutex);
  return g;
}

void blf_glyph_free(GlyphBLF *g)
{
  if (g->bitmap) {
    MEM_freeN(g->bitmap);
  }
  MEM_freeN(g);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Bounds Calculation
 * \{ */

static void blf_glyph_calc_rect(rcti *rect, GlyphBLF *g, const int x, const int y)
{
  rect->xmin = x + g->pos[0];
  rect->xmax = rect->xmin + g->dims[0];
  rect->ymin = y + g->pos[1];
  rect->ymax = rect->ymin - g->dims[1];
}

static void blf_glyph_calc_rect_test(rcti *rect, GlyphBLF *g, const int x, const int y)
{
  /* Intentionally check with `g->advance`, because this is the
   * width used by BLF_width. This allows that the text slightly
   * overlaps the clipping border to achieve better alignment. */
  rect->xmin = x;
  rect->xmax = rect->xmin + MIN2(ft_pix_to_int(g->advance_x), g->dims[0]);
  rect->ymin = y;
  rect->ymax = rect->ymin - g->dims[1];
}

static void blf_glyph_calc_rect_shadow(
    rcti *rect, GlyphBLF *g, const int x, const int y, FontBLF *font)
{
  blf_glyph_calc_rect(rect, g, x + font->shadow_x, y + font->shadow_y);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Drawing
 * \{ */

static void blf_texture_draw(const unsigned char color[4],
                             const int glyph_size[2],
                             const int offset,
                             const int x1,
                             const int y1,
                             const int x2,
                             const int y2)
{
  /* Only one vertex per glyph, geometry shader expand it into a quad. */
  /* TODO: Get rid of Geom Shader because it's not optimal AT ALL for the GPU. */
  copy_v4_fl4(GPU_vertbuf_raw_step(&g_batch.pos_step),
              (float)(x1 + g_batch.ofs[0]),
              (float)(y1 + g_batch.ofs[1]),
              (float)(x2 + g_batch.ofs[0]),
              (float)(y2 + g_batch.ofs[1]));
  copy_v4_v4_uchar(GPU_vertbuf_raw_step(&g_batch.col_step), color);
  copy_v2_v2_int(GPU_vertbuf_raw_step(&g_batch.glyph_size_step), glyph_size);
  *((int *)GPU_vertbuf_raw_step(&g_batch.offset_step)) = offset;

  g_batch.glyph_len++;
  /* Flush cache if it's full. */
  if (g_batch.glyph_len == BLF_BATCH_DRAW_LEN_MAX) {
    blf_batch_draw();
  }
}

static void blf_texture5_draw(const unsigned char color_in[4],
                              const int glyph_size[2],
                              const int offset,
                              const int x1,
                              const int y1,
                              const int x2,
                              const int y2)
{
  int glyph_size_flag[2];
  /* flag the x and y component signs for 5x5 blurring */
  glyph_size_flag[0] = -glyph_size[0];
  glyph_size_flag[1] = -glyph_size[1];

  blf_texture_draw(color_in, glyph_size_flag, offset, x1, y1, x2, y2);
}

static void blf_texture3_draw(const unsigned char color_in[4],
                              const int glyph_size[2],
                              const int offset,
                              const int x1,
                              const int y1,
                              const int x2,
                              const int y2)
{
  int glyph_size_flag[2];
  /* flag the x component sign for 3x3 blurring */
  glyph_size_flag[0] = -glyph_size[0];
  glyph_size_flag[1] = glyph_size[1];

  blf_texture_draw(color_in, glyph_size_flag, offset, x1, y1, x2, y2);
}

void blf_glyph_draw(FontBLF *font, GlyphCacheBLF *gc, GlyphBLF *g, const int x, const int y)
{
  if ((!g->dims[0]) || (!g->dims[1])) {
    return;
  }

  if (g->glyph_cache == NULL) {
    if (font->tex_size_max == -1) {
      font->tex_size_max = GPU_max_texture_size();
    }

    g->offset = gc->bitmap_len;

    int buff_size = g->dims[0] * g->dims[1];
    int bitmap_len = gc->bitmap_len + buff_size;

    if (bitmap_len > gc->bitmap_len_alloc) {
      int w = font->tex_size_max;
      int h = bitmap_len / w + 1;

      gc->bitmap_len_alloc = w * h;
      gc->bitmap_result = MEM_reallocN(gc->bitmap_result, (size_t)gc->bitmap_len_alloc);

      /* Keep in sync with the texture. */
      if (gc->texture) {
        GPU_texture_free(gc->texture);
      }
      gc->texture = GPU_texture_create_2d(__func__, w, h, 1, GPU_R8, NULL);

      gc->bitmap_len_landed = 0;
    }

    memcpy(&gc->bitmap_result[gc->bitmap_len], g->bitmap, (size_t)buff_size);
    gc->bitmap_len = bitmap_len;

    g->glyph_cache = gc;
  }

  if (font->flags & BLF_CLIPPING) {
    rcti rect_test;
    blf_glyph_calc_rect_test(&rect_test, g, x, y);
    BLI_rcti_translate(&rect_test, font->pos[0], font->pos[1]);

    if (!BLI_rcti_inside_rcti(&font->clip_rec, &rect_test)) {
      return;
    }
  }

  if (g_batch.glyph_cache != g->glyph_cache) {
    blf_batch_draw();
    g_batch.glyph_cache = g->glyph_cache;
  }

  if (font->flags & BLF_SHADOW) {
    rcti rect_ofs;
    blf_glyph_calc_rect_shadow(&rect_ofs, g, x, y, font);

    if (font->shadow == 0) {
      blf_texture_draw(font->shadow_color,
                       g->dims,
                       g->offset,
                       rect_ofs.xmin,
                       rect_ofs.ymin,
                       rect_ofs.xmax,
                       rect_ofs.ymax);
    }
    else if (font->shadow <= 4) {
      blf_texture3_draw(font->shadow_color,
                        g->dims,
                        g->offset,
                        rect_ofs.xmin,
                        rect_ofs.ymin,
                        rect_ofs.xmax,
                        rect_ofs.ymax);
    }
    else {
      blf_texture5_draw(font->shadow_color,
                        g->dims,
                        g->offset,
                        rect_ofs.xmin,
                        rect_ofs.ymin,
                        rect_ofs.xmax,
                        rect_ofs.ymax);
    }
  }

  rcti rect;
  blf_glyph_calc_rect(&rect, g, x, y);

#if BLF_BLUR_ENABLE
  switch (font->blur) {
    case 3:
      blf_texture3_draw(
          font->color, g->dims, g->offset, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
      break;
    case 5:
      blf_texture5_draw(
          font->color, g->dims, g->offset, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
      break;
    default:
      blf_texture_draw(
          font->color, g->dims, g->offset, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
  }
#else
  blf_texture_draw(font->color, g->dims, g->offset, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
#endif
}

/** \} */
