/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 */

#pragma once

#include "BLI_enum_flags.hh"

enum class FontShadowType {
  None = 0,
  Blur3x3 = 3,
  Blur5x5 = 5,
  Outline = 6,
};

enum class BLFWrapMode : int {
  /** Only on ASCII space and line feed. Legacy and invariant. */
  Minimal = 0,
  /** Multilingual, informed by Unicode Standard Annex #14. */
  Typographical = 1 << 0,
  /** Wrap on file path separators, space, underscores. */
  Path = 1 << 1,
  /** Line break at limit. */
  HardLimit = 1 << 2,
};

enum FontFlags {
  BLF_NONE = 0,
  BLF_ROTATION = 1 << 0,
  BLF_CLIPPING = 1 << 1,
  BLF_SHADOW = 1 << 2,
  // BLF_FLAG_UNUSED_3 = 1 << 3, /* dirty */
  // BLF_MATRIX = 1 << 4,
  BLF_ASPECT = 1 << 5,
  BLF_WORD_WRAP = 1 << 6,
  /** No anti-aliasing. */
  BLF_MONOCHROME = 1 << 7,
  BLF_HINTING_NONE = 1 << 8,
  BLF_HINTING_SLIGHT = 1 << 9,
  BLF_HINTING_FULL = 1 << 10,
  BLF_BOLD = 1 << 11,
  BLF_ITALIC = 1 << 12,
  /** Intended USE is monospaced, regardless of font type. */
  BLF_MONOSPACED = 1 << 13,
  /** A font within the default stack of fonts. */
  BLF_DEFAULT = 1 << 14,
  /** Must only be used as last font in the stack. */
  BLF_LAST_RESORT = 1 << 15,
  /** Failure to load this font. Don't try again. */
  BLF_BAD_FONT = 1 << 16,
  /** This font is managed by the FreeType cache subsystem. */
  BLF_CACHED = 1 << 17,
  /**
   * At small sizes glyphs are rendered at multiple sub-pixel positions.
   *
   * \note Can be checked without checking #BLF_MONOSPACED which can be assumed to be disabled.
   */
  BLF_RENDER_SUBPIXELAA = 1 << 18,

  /** Do not look in other fonts when a glyph is not found in this font. */
  BLF_NO_FALLBACK = 1 << 19,
};
ENUM_OPERATORS(FontFlags);
