/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 *
 * Utility function to generate font preview images.
 *
 * Isolate since this needs to be called by #ImBuf code (bad level call).
 */

#include <algorithm>
#include <cstdlib>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_ADVANCES_H        /* For FT_Get_Advance */
#include FT_TRUETYPE_IDS_H    /* Code-point coverage constants. */
#include FT_TRUETYPE_TABLES_H /* For TT_OS2 */

#include "BLI_math_bits.h"
#include "BLI_utildefines.h"

#include "blf_internal_types.hh"

#include "BLF_api.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* Maximum length of text sample in char32_t, including null terminator. */
#define BLF_SAMPLE_LEN 5

struct UnicodeSample {
  char32_t sample[BLF_SAMPLE_LEN];
  int field;     /* 'OS/2' table ulUnicodeRangeX field (1-4). */
  FT_ULong mask; /* 'OS/2' table ulUnicodeRangeX bit mask. */
};

/* The seemingly arbitrary order that follows is to help quickly find the most-likely designed
 * intent of the font. Many feature-specific fonts contain Latin, Greek, & Coptic characters so
 * those need to be checked last. */
static const UnicodeSample unicode_samples[] = {
    /* Chinese, Japanese, Korean, ordered specific to general. */
    {U"\ud55c\uad6d\uc5b4", 2, TT_UCR_HANGUL},                 /* 한국어 */
    {U"\u3042\u30a2\u4e9c", 2, TT_UCR_HIRAGANA},               /* あア亜 */
    {U"\u30a2\u30a4\u4e9c", 2, TT_UCR_KATAKANA},               /* アイ亜 */
    {U"\u1956\u195b\u1966", 3, TT_UCR_TAI_LE},                 /* ᥖᥛᥦ */
    {U"\u3105\u3106\u3107", 2, TT_UCR_BOPOMOFO},               /* ㄅㄆㄇ */
    {U"\ua840\ua841\ua85d", 2, TT_UCR_PHAGSPA},                /* ꡀꡁꡝ */
    {U"\u5e03\u4e01\u4f53", 2, TT_UCR_CJK_UNIFIED_IDEOGRAPHS}, /* 布丁体 */
    /* Languages in the BMP with a coverage bit. */
    {U"\u05d0\u05da\u05e4", 1, TT_UCR_HEBREW},
    {U"\ua500\ua502\ua549", 1, TT_UCR_VAI},
    {U"\ufee6\ufef4\ufeb3", 1, TT_UCR_ARABIC},
    {U"\u07C1\u07C2\u07C3", 1, TT_UCR_NKO},
    {U"\u0905\u093f\u092a", 1, TT_UCR_DEVANAGARI},
    {U"\u0986\u0987\u098c", 1, TT_UCR_BENGALI},
    {U"\u0a05\u0a16\u0a30", 1, TT_UCR_GURMUKHI},
    {U"\u0aaa\u0aaf\u0ab8", 1, TT_UCR_GUJARATI},
    {U"\u0b2a\u0b30\u0b37", 1, TT_UCR_ORIYA},
    {U"\u0b85\u0b88\u0b8f", 1, TT_UCR_TAMIL},
    {U"\u0c05\u0c0c\u0c36", 1, TT_UCR_TELUGU},
    {U"\u0c85\u0c87\u0c8e", 1, TT_UCR_KANNADA},
    {U"\u0d05\u0d09\u0d3d", 1, TT_UCR_MALAYALAM},
    {U"\u0e05\u0e06\u0e07", 1, TT_UCR_THAI},
    {U"\u0e81\u0e82\u0e84", 1, TT_UCR_LAO},
    {U"\u10a0\u10a1\u10a2", 1, TT_UCR_GEORGIAN},
    {U"\u1B05\u1B07\u1B09", 1, TT_UCR_BALINESE},
    {U"\u0f00\u0f04\u0f08", 3, TT_UCR_TIBETAN},
    {U"\u0710\u0717\u071c", 3, TT_UCR_SYRIAC},
    {U"\u0784\u0783\u0798", 3, TT_UCR_THAANA},
    {U"\u0d85\u0d89\u0daf", 3, TT_UCR_SINHALA},
    {U"\u1000\u1001\u1014", 3, TT_UCR_MYANMAR},
    {U"\u1202\u1207\u1250", 3, TT_UCR_ETHIOPIC},
    {U"\u13a3\u13a4\u13a8", 3, TT_UCR_CHEROKEE},
    {U"\u1401\u144d\u156e", 3, TT_UCR_CANADIAN_ABORIGINAL_SYLLABICS},
    {U"\u1681\u1687\u168b", 3, TT_UCR_OGHAM},
    {U"\u16A0\u16A4\u16AA", 3, TT_UCR_RUNIC},
    {U"\u1780\u1781\u1783", 3, TT_UCR_KHMER},
    {U"\u1820\u1826\u1845", 3, TT_UCR_MONGOLIAN},
    {U"\ua188\ua320\ua4bf", 3, TT_UCR_YI},
    {U"\u1900\u1901\u1902", 3, TT_UCR_LIMBU},
    {U"\u1950\u1951\u1952", 3, TT_UCR_TAI_LE},
    {U"\u1980\u1982\u1986", 3, (FT_ULong)TT_UCR_NEW_TAI_LUE},
    {U"\u1A00\u1A01\u1A02", 4, TT_UCR_BUGINESE},
    {U"\u2c01\u2c05\u2c0c", 4, TT_UCR_GLAGOLITIC},
    {U"\u2d31\u2d33\u2d37", 4, TT_UCR_TIFINAGH},
    {U"\u2d31\u2d33\u2d37", 4, TT_UCR_YIJING},
    {U"\u1B83\u1B84\u1B88", 4, TT_UCR_SUNDANESE},
    {U"\u1C00\u1C01\u1C02", 4, TT_UCR_LEPCHA},
    {U"\u1C50\u1C51\u1C52", 4, TT_UCR_OL_CHIKI},
    {U"\uA800\uA801\uA805", 4, TT_UCR_SYLOTI_NAGRI},
    {U"\uA882\uA88a\uA892", 4, TT_UCR_SAURASHTRA},
    {U"\uA901\uA902\uA904", 4, TT_UCR_KAYAH_LI},
    {U"\uA930\uA932\uA943", 4, TT_UCR_REJANG},
    {U"\uaa00\uaa02\uaa05", 4, TT_UCR_CHAM},
    /* Indexed languages in the Supplementary Multilingual Plane. */
    {U"\U00010000\U00010001\U00010002", 4, TT_UCR_LINEAR_B},
    {U"\U00010300\U00010301\U00010302", 3, TT_UCR_OLD_ITALIC},
    {U"\U00010330\U00010331\U00010332", 3, TT_UCR_GOTHIC},
    {U"\U00010380\U00010381\U00010382", 4, TT_UCR_UGARITIC},
    {U"\U000103A0\U000103A1\U000103A2", 4, TT_UCR_OLD_PERSIAN},
    {U"\U00010400\U00010401\U00010402", 3, TT_UCR_DESERET},
    {U"\U00010450\U00010451\U00010452", 4, TT_UCR_SHAVIAN},
    {U"\U00010480\U00010481\U00010482", 4, TT_UCR_OSMANYA},
    {U"\U00010800\U00010803\U00010805", 4, TT_UCR_CYPRIOT_SYLLABARY},
    {U"\U00010900\U00010901\U00010902", 2, TT_UCR_PHOENICIAN},
    {U"\U00010A10\U00010A11\U00010A12", 4, TT_UCR_KHAROSHTHI},
    {U"\U00012000\U00012001\U00012002", 4, TT_UCR_CUNEIFORM},
    /* Philippine languages use a single OS2 coverage bit. */
    {U"\u1700\u1701\u1702", 3, TT_UCR_PHILIPPINE}, /* Tagalog */
    {U"\u1720\u1721\u1722", 3, TT_UCR_PHILIPPINE}, /* Hanunoo */
    {U"\u1740\u1741\u1742", 3, TT_UCR_PHILIPPINE}, /* Buhid */
    {U"\u1760\u1761\u1762", 3, TT_UCR_PHILIPPINE}, /* Tagbanwa */
    /* Anatolian languages use a single OS2 coverage bit. */
    {U"\U000102A3\U000102A8\U000102CB", 4, TT_UCR_OLD_ANATOLIAN}, /* Carian */
    {U"\U00010280\U00010281\U00010282", 4, TT_UCR_OLD_ANATOLIAN}, /* Lycian */
    {U"\U00010920\U00010921\U00010922", 4, TT_UCR_OLD_ANATOLIAN}, /* Lydian */
    /* Symbol blocks. */
    {U"\U0001f600\U0001f638", 0, 0}, /* Emoticons 😀😸 */
    {U"\uf021\uf022\uf023", 0, 0},   /* MS Symbols */
    {U"\u280f\u2815\u283f", 3, TT_UCR_BRAILLE},
    {U"\U0001D11e\U0001D161\U0001D130", 3, TT_UCR_MUSICAL_SYMBOLS},
    {U"\u2700\u2708\u2709", 2, TT_UCR_DINGBATS},
    {U"\u2600\u2601\u2602", 2, TT_UCR_MISCELLANEOUS_SYMBOLS},
    {U"\ue000\ue001\ue002", 2, TT_UCR_PRIVATE_USE},
    {U"\ue702\ue703\ue704", 2, TT_UCR_PRIVATE_USE},
    {U"\U000F0001\U000F0002\U000F0003", 2, TT_UCR_PRIVATE_USE_SUPPLEMENTARY},
    /* Languages in the Supplementary Multilingual Plane. */
    {U"\U00010350\U00010352\U00010353", 2, TT_UCR_NON_PLANE_0}, /* Old Permic */
    {U"\U000104B0\U000104B6\U000104B8", 2, TT_UCR_NON_PLANE_0}, /* Osage */
    {U"\U00010500\U00010501\U00010502", 2, TT_UCR_NON_PLANE_0}, /* Elbasan */
    {U"\U00010530\U00010531\U00010532", 2, TT_UCR_NON_PLANE_0}, /* Caucasian Albanian */
    {U"\U00010600\U00010601\U00010602", 2, TT_UCR_NON_PLANE_0}, /* Linear A */
    {U"\U00010840\U00010841\U00010842", 2, TT_UCR_NON_PLANE_0}, /* Imperial Aramaic */
    {U"\U00010860\U00010861\U00010862", 2, TT_UCR_NON_PLANE_0}, /* Palmyrene */
    {U"\U00010880\U00010881\U00010882", 2, TT_UCR_NON_PLANE_0}, /* Nabataean */
    {U"\U000108E0\U000108E3\U000108E4", 2, TT_UCR_NON_PLANE_0}, /* Hatran */
    {U"\U00010980\U00010983\U00010989", 2, TT_UCR_NON_PLANE_0}, /* Meroitic Hieroglyphs */
    {U"\U000109A0\U000109A1\U000109A2", 2, TT_UCR_NON_PLANE_0}, /* Meroitic Cursive */
    {U"\U00010A60\U00010A61\U00010A62", 2, TT_UCR_NON_PLANE_0}, /* Old South Arabian */
    {U"\U00010A80\U00010A81\U00010A82", 2, TT_UCR_NON_PLANE_0}, /* Old North Arabian */
    {U"\U00010ac0\U00010ac3\U00010ac6", 2, TT_UCR_NON_PLANE_0}, /* Manichaean */
    {U"\U00010B00\U00010B04\U00010B08", 2, TT_UCR_NON_PLANE_0}, /* Avestan */
    {U"\U00010B40\U00010B41\U00010B42", 2, TT_UCR_NON_PLANE_0}, /* Inscriptional Parthian */
    {U"\U00010B60\U00010B61\U00010B62", 2, TT_UCR_NON_PLANE_0}, /* Inscriptional Pahlavi */
    {U"\U00010B80\U00010B84\U00010B87", 2, TT_UCR_NON_PLANE_0}, /* Psalter Pahlavi */
    {U"\U00010C00\U00010C01\U00010C02", 2, TT_UCR_NON_PLANE_0}, /* Old Turkic */
    {U"\U00010C80\U00010C81\U00010C82", 2, TT_UCR_NON_PLANE_0}, /* Old Hungarian */
    {U"\U00010D00\U00010D07\U00010D0D", 2, TT_UCR_NON_PLANE_0}, /* Hanifi Rohingya */
    {U"\U00010E80\U00010E81\U00010E82", 2, TT_UCR_NON_PLANE_0}, /* Yezidi */
    {U"\U00010F00\U00010F01\U00010F02", 2, TT_UCR_NON_PLANE_0}, /* Old Sogdian */
    {U"\U00010F30\U00010F32\U00010F34", 2, TT_UCR_NON_PLANE_0}, /* Sogdian */
    {U"\U00010F70\U00010F71\U00010F72", 2, TT_UCR_NON_PLANE_0}, /* Old Uyghur */
    {U"\U00010FB0\U00010FB1\U00010FB2", 2, TT_UCR_NON_PLANE_0}, /* Chorasmian */
    {U"\U00010FE0\U00010FE1\U00010FE2", 2, TT_UCR_NON_PLANE_0}, /* Elymaic */
    {U"\U00011003\U00011004\U00011005", 2, TT_UCR_NON_PLANE_0}, /* Brahmi */
    {U"\U00011083\U00011085\U00011087", 2, TT_UCR_NON_PLANE_0}, /* Kaithi */
    {U"\U000110D0\U000110D1\U000110D2", 2, TT_UCR_NON_PLANE_0}, /* Sora Sompeng */
    {U"\U00011103\U00011104\U00011105", 2, TT_UCR_NON_PLANE_0}, /* Chakma */
    {U"\U00011150\U00011151\U00011152", 2, TT_UCR_NON_PLANE_0}, /* Mahajani */
    {U"\U00011183\U00011185\U0001118b", 2, TT_UCR_NON_PLANE_0}, /* Sharada */
    {U"\U00011200\U00011201\U00011202", 2, TT_UCR_NON_PLANE_0}, /* Khojki */
    {U"\U00011280\U00011281\U00011282", 2, TT_UCR_NON_PLANE_0}, /* Multani */
    {U"\U000112B0\U000112B2\U000112B4", 2, TT_UCR_NON_PLANE_0}, /* Khudawadi */
    {U"\U00011305\U00011309\U0001130b", 2, TT_UCR_NON_PLANE_0}, /* Grantha */
    {U"\U00011400\U00011404\U00011409", 2, TT_UCR_NON_PLANE_0}, /* Newa */
    {U"\U00011480\U00011481\U00011482", 2, TT_UCR_NON_PLANE_0}, /* Tirhuta */
    {U"\U00011580\U00011582\U00011589", 2, TT_UCR_NON_PLANE_0}, /* Siddham */
    {U"\U00011600\U00011604\U00011609", 2, TT_UCR_NON_PLANE_0}, /* Modi */
    {U"\U00011680\U00011682\U0001168A", 2, TT_UCR_NON_PLANE_0}, /* Takri */
    {U"\U00011700\U00011701\U00011702", 2, TT_UCR_NON_PLANE_0}, /* Ahom */
    {U"\U00011800\U00011801\U00011802", 2, TT_UCR_NON_PLANE_0}, /* Dogri */
    {U"\U000118A0\U000118A1\U000118AA", 2, TT_UCR_NON_PLANE_0}, /* Warang Citi */
    {U"\U00011900\U00011901\U00011902", 2, TT_UCR_NON_PLANE_0}, /* Dives Akuru */
    {U"\U00011A00\U00011A10\U00011A15", 2, TT_UCR_NON_PLANE_0}, /* Zanabazar Square */
    {U"\U00011A50\U00011A5C\U00011A6B", 2, TT_UCR_NON_PLANE_0}, /* Soyombo */
    {U"\U00011AC0\U00011AC1\U00011AC2", 2, TT_UCR_NON_PLANE_0}, /* Pau Cin Hau */
    {U"\U00011C00\U00011C01\U00011C02", 2, TT_UCR_NON_PLANE_0}, /* Bhaiksuki */
    {U"\U00011C70\U00011C71\U00011C72", 2, TT_UCR_NON_PLANE_0}, /* Marchen */
    {U"\U00011D00\U00011D02\U00011D08", 2, TT_UCR_NON_PLANE_0}, /* Masaram Gondi */
    {U"\U00011D60\U00011D62\U00011D6c", 2, TT_UCR_NON_PLANE_0}, /* Gunjala Gondi */
    {U"\U00011FC1\U00011FC2\U00011FC8", 2, TT_UCR_NON_PLANE_0}, /* Tamil Supplement */
    {U"\U00012F90\U00012F91\U00012F92", 2, TT_UCR_NON_PLANE_0}, /* Cypro-Minoan */
    {U"\U00013000\U00013076\U0001307f", 2, TT_UCR_NON_PLANE_0}, /* Egyptian Hieroglyphs */
    {U"\U00014400\U00014409\U00014447", 2, TT_UCR_NON_PLANE_0}, /* Anatolian Hieroglyphs */
    {U"\U00016A40\U00016A41\U00016A42", 2, TT_UCR_NON_PLANE_0}, /* Mro */
    {U"\U00016A70\U00016A71\U00016A72", 2, TT_UCR_NON_PLANE_0}, /* Tangsa */
    {U"\U00016AD0\U00016AD2\U00016ADA", 2, TT_UCR_NON_PLANE_0}, /* Bassa Vah */
    {U"\U00016B00\U00016B01\U00016B02", 2, TT_UCR_NON_PLANE_0}, /* Pahawh Hmong */
    {U"\U00016F01\U00016F05\U00016F09", 2, TT_UCR_NON_PLANE_0}, /* Miao */
    {U"\U0001BC19\U0001BC1f\U0001BC0e", 2, TT_UCR_NON_PLANE_0}, /* Duployan */
    {U"\U0001D2E0\U0001D2E6\U0001D2f3", 2, TT_UCR_NON_PLANE_0}, /* Mayan Numerals */
    {U"\U0001E800\U0001E80A\U0001E80F", 2, TT_UCR_NON_PLANE_0}, /* Mende Kikakui */
    {U"\U0001E900\U0001E902\U0001E907", 2, TT_UCR_NON_PLANE_0}, /* Adlam */
    {U"\U0001E2C0\U0001E2C2\U0001E2C7", 2, TT_UCR_NON_PLANE_0}, /* Wancho */
    {U"\U0001EC71\U0001EC72\U0001EC73", 2, TT_UCR_NON_PLANE_0}, /* Indic Siyaq Numbers */
    /* Basic Multilingual Plane but are not indexed with an OS2 coverage bit. */
    {U"\u0638\u0630\u0633", 0, 0}, /* Urdu */
    {U"\u0800\u0801\u0802", 0, 0}, /* Samaritan */
    {U"\u0841\u0842\u084c", 0, 0}, /* Mandaic */
    {U"\u1A20\u1A21\u1A22", 0, 0}, /* Tai Tham */
    {U"\u1BC0\u1BC1\u1BC2", 0, 0}, /* Batak */
    {U"\uA4EF\uA4E8\uA4ED", 0, 0}, /* Lisu */
    {U"\uA6A0\uA6A1\uA6A2", 0, 0}, /* Bamum */
    {U"\ua983\ua984\ua98d", 0, 0}, /* Javanese */
    {U"\uaa80\uaa81\uaa82", 0, 0}, /* Tai Viet */
    {U"\uABC0\uABC1\uABC2", 0, 0}, /* Meetei Mayek */
    /* Near the end since many fonts contain these. */
    {U"\u03e2\u03e4\u03e8", 1, TT_UCR_COPTIC},
    {U"\u1f08\u03a6\u03a8", 1, TT_UCR_GREEK},
    {U"\u0518\u0409\u040f", 1, TT_UCR_CYRILLIC},
    {U"\u0533\u0537\u0539", 1, TT_UCR_ARMENIAN},
};

static const char32_t *blf_get_sample_text(const FT_Face face)
{
  /* First check for fonts with MS Symbol character map. */
  if (face->charmap->encoding == FT_ENCODING_MS_SYMBOL) {
    /* Many of these have characters starting from F020. */
    if (FT_Get_Char_Index(face, U'\uf041') != 0) {
      return U"\uf041\uf044\uf048";
    }
    if (FT_Get_Char_Index(face, U'\uf030') != 0) {
      return U"\uf030\uf031\uf032";
    }
    return U"ADH";
  }

  const char32_t *def = U"Aabg";
  const char32_t *sample = def;

  /* Fonts too old to have a Unicode character map. */
  if (face->charmap->encoding != FT_ENCODING_UNICODE) {
    return def;
  }

  /* TrueType table with bits to quickly test most Unicode block coverage. */
  TT_OS2 *os2_table = (TT_OS2 *)FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
  if (!os2_table) {
    return def;
  }

  /* Detect "Last resort" fonts. They have everything, except the last 5 bits. */
  if (os2_table->ulUnicodeRange1 == 0xffffffffU && os2_table->ulUnicodeRange2 == 0xffffffffU &&
      os2_table->ulUnicodeRange3 == 0xffffffffU && os2_table->ulUnicodeRange4 >= 0x7FFFFFFU)
  {
    return U"\xE000\xFFFF";
  }

  int language_count = count_bits_i(uint(os2_table->ulUnicodeRange1)) +
                       count_bits_i(uint(os2_table->ulUnicodeRange2)) +
                       count_bits_i(uint(os2_table->ulUnicodeRange3)) +
                       count_bits_i(uint(os2_table->ulUnicodeRange4));

  /* Use OS/2 Table code page range bits to differentiate between (combined) CJK fonts.
   * See https://learn.microsoft.com/en-us/typography/opentype/spec/os2#cpr */
  FT_ULong codepages = os2_table->ulCodePageRange1;
  if (codepages & (1 << 19) || codepages & (1 << 21)) {
    return U"\ud55c\uad6d\uc5b4"; /* 한국어 Korean. */
  }
  if (codepages & (1 << 20)) {
    return U"\u7E41\u9AD4\u5B57"; /* 繁體字 Traditional Chinese. */
  }
  if (codepages & (1 << 17) && !(codepages & (1 << 18))) {
    return U"\u65E5\u672C\u8A9E"; /* 日本語 Japanese. */
  }
  if (codepages & (1 << 18) && !(codepages & (1 << 17))) {
    return U"\u7B80\u4F53\u5B57"; /* 简体字 Simplified Chinese. */
  }

  for (uint i = 0; i < ARRAY_SIZE(unicode_samples); ++i) {
    const UnicodeSample *s = &unicode_samples[i];
    if (os2_table && s->field && s->mask) {
      /* OS/2 Table contains 4 contiguous integers of script coverage bit flags. */
      const FT_ULong *unicode_range = &os2_table->ulUnicodeRange1;
      const int index = (s->field - 1);
      BLI_assert(index < 4);
      if (!(unicode_range[index] & s->mask)) {
        continue;
      }
    }
    if (FT_Get_Char_Index(face, s->sample[0]) != 0) {
      sample = s->sample;
      break;
    }
  }

  bool has_latin = (os2_table && (os2_table->ulUnicodeRange1 & TT_UCR_BASIC_LATIN) &&
                    (FT_Get_Char_Index(face, U'A') != 0));
  bool has_cjk = (os2_table && (os2_table->ulUnicodeRange2 & TT_UCR_CJK_UNIFIED_IDEOGRAPHS));

  if (has_latin && ((has_cjk && language_count > 40) || (!has_cjk && language_count > 5))) {
    return def;
  }

  return sample;
}

bool BLF_thumb_preview(
    const char *filepath, uchar *buf, const int w, const int h, const int /*channels*/)
{
  /* Use own FT_Library and direct FreeType calls as this is called from multiple threads. */
  FT_Library ft_lib = nullptr;
  if (FT_Init_FreeType(&ft_lib) != FT_Err_Ok) {
    return false;
  }

  FT_Face face;
  if (FT_New_Face(ft_lib, filepath, 0, &face) != FT_Err_Ok) {
    FT_Done_FreeType(ft_lib);
    return false;
  }

  if (!(face->face_flags & FT_FACE_FLAG_SCALABLE)) {
    return false;
  }

  FT_Error err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
  if (err) {
    err = FT_Select_Charmap(face, FT_ENCODING_MS_SYMBOL);
  }
  if (err) {
    err = FT_Select_Charmap(face, FT_ENCODING_APPLE_ROMAN);
  }
  if (err && face->num_charmaps > 0) {
    err = FT_Select_Charmap(face, face->charmaps[0]->encoding);
  }
  if (err != FT_Err_Ok) {
    FT_Done_Face(face);
    FT_Done_FreeType(ft_lib);
    return false;
  }

  const char32_t *codepoints = blf_get_sample_text(face);
  uint glyph_ids[BLF_SAMPLE_LEN] = {0};

  /* A large initial font size for measuring. Nothing will be rendered this size. */
  if (FT_Set_Char_Size(face, w * 64, 0, 72, 72) != FT_Err_Ok) {
    FT_Done_Face(face);
    FT_Done_FreeType(ft_lib);
    return false;
  }

  /* Approximate length of the sample. Uses only advances, ignores bearings. */
  int width = 0;
  for (uint i = 0; i < BLF_SAMPLE_LEN && codepoints[i]; i++) {
    glyph_ids[i] = FT_Get_Char_Index(face, codepoints[i]);
    /* If sample glyph is not found, use another. */
    if (!glyph_ids[i]) {
      glyph_ids[i] = uint(face->num_glyphs / (BLF_SAMPLE_LEN + 1)) * (i + 1);
    }
    /* Get advance without loading the glyph. */
    FT_Fixed advance;
    FT_Get_Advance(face, glyph_ids[i], FT_LOAD_NO_HINTING, &advance);
    /* Advance is returned in 16.16 format, so divide by 65536 for pixels. */
    width += int(advance >> 16);
  }

  int height = ft_pix_to_int((ft_pix)face->size->metrics.ascender -
                             (ft_pix)face->size->metrics.descender);
  width = std::max(width, height);

  /* Fill up to 96% horizontally or vertically. */
  float font_size = std::min({float(w),
                              (float(w) * 0.96f / float(width) * float(w)),
                              float(h) * 0.96f / float(height) * float(h)});

  if (font_size < 1 || FT_Set_Char_Size(face, int(font_size * 64.0f), 0, 72, 72) != FT_Err_Ok) {
    /* Sizing can fail, but very rarely. */
    FT_Done_Face(face);
    FT_Done_FreeType(ft_lib);
    return false;
  }

  /* Horizontally center, line up baselines vertically. */
  int left = int((float(w) - (float(width) * (font_size / float(w)))) / 2.0f);
  int top = int(float(h) * 0.7f);

  /* Print out to buffer. */

  FT_Pos advance_x = 0;
  int glyph_count = 0; /* How many are successfully loaded and rendered. */

  for (int i = 0; i < BLF_SAMPLE_LEN && glyph_ids[i]; i++) {
    if (FT_Load_Glyph(face, glyph_ids[i], FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING) != FT_Err_Ok)
    {
      break;
    }

    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != FT_Err_Ok ||
        face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
    {
      break;
    }

    glyph_count++;

    for (int y = 0; y < int(face->glyph->bitmap.rows); y++) {
      int dest_row = (h - y - 1 + int(face->glyph->bitmap_top) - top);
      if (dest_row >= 0 && dest_row < h) {
        for (int x = 0; x < int(face->glyph->bitmap.width); x++) {
          int dest_col = (x + ft_pix_to_int((ft_pix)advance_x) + face->glyph->bitmap_left + left);
          if (dest_col >= 0 && dest_col < w) {
            uchar *source = &face->glyph->bitmap.buffer[y * int(face->glyph->bitmap.width) + x];
            uchar *dest = &buf[dest_row * w * 4 + (dest_col * 4 + 3)];
            *dest = uchar(std::min((uint(*dest) + uint(*source)), 255u));
          }
        }
      }
    }

    advance_x += face->glyph->advance.x;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft_lib);

  /* Return success if we printed at least one glyph. */
  return glyph_count > 0;
}
