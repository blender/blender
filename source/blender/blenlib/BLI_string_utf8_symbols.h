/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Unicode characters as UTF-8 strings. Last portion should include the official assigned name.
 * Please do not add defines here that are not actually in use.  */

#define BLI_STR_UTF8_MULTIPLICATION_SIGN u8"\u00D7"                                    /* × */
#define BLI_STR_UTF8_EM_DASH u8"\u2014"                                                /* — */
#define BLI_STR_UTF8_BULLET u8"\u2022"                                                 /* • */
#define BLI_STR_UTF8_HORIZONTAL_ELLIPSIS u8"\u2026"                                    /* … */
#define BLI_STR_UTF8_LEFTWARDS_ARROW u8"\u2190"                                        /* ← */
#define BLI_STR_UTF8_UPWARDS_ARROW u8"\u2191"                                          /* ↑ */
#define BLI_STR_UTF8_RIGHTWARDS_ARROW u8"\u2192"                                       /* → */
#define BLI_STR_UTF8_DOWNWARDS_ARROW u8"\u2193"                                        /* ↓ */
#define BLI_STR_UTF8_UPWARDS_WHITE_ARROW u8"\u21E7"                                    /* ⇧ */
#define BLI_STR_UTF8_UP_ARROWHEAD u8"\u2303"                                           /* ⌃ */
#define BLI_STR_UTF8_PLACE_OF_INTEREST_SIGN u8"\u2318"                                 /* ⌘ */
#define BLI_STR_UTF8_OPTION_KEY u8"\u2325"                                             /* ⌥ */
#define BLI_STR_UTF8_ERASE_TO_THE_LEFT u8"\u232B"                                      /* ⌫ */
#define BLI_STR_UTF8_BROKEN_CIRCLE_WITH_NORTHWEST_ARROW u8"\u238B"                     /* ⎋ */
#define BLI_STR_UTF8_RETURN_SYMBOL u8"\u23CE"                                          /* ⏎ */
#define BLI_STR_UTF8_BLACK_RIGHT_POINTING_DOUBLE_TRIANGLE_WITH_VERTICAL_BAR u8"\u23ED" /* ⏭ */
#define BLI_STR_UTF8_BLACK_LEFT_POINTING_DOUBLE_TRIANGLE_WITH_VERTICAL_BAR u8"\u23EE"  /* ⏮ */
#define BLI_STR_UTF8_BLACK_RIGHT_POINTING_TRIANGLE_WITH_DOUBLE_VERTICAL_BAR u8"\u23EF" /* ⏯ */
#define BLI_STR_UTF8_BLACK_SQUARE_FOR_STOP u8"\u23F9"                                  /* ⏹ */
#define BLI_STR_UTF8_OPEN_BOX u8"\u2423"                                               /* ␣ */
#define BLI_STR_UTF8_BLACK_RIGHT_POINTING_SMALL_TRIANGLE u8"\u25B8"                    /* ▸ */
#define BLI_STR_UTF8_HORIZONTAL_TAB_KEY u8"\u2B7E"                                     /* ⭾ */
#define BLI_STR_UTF8_BLACK_DIAMOND_MINUS_WHITE_X u8"\u2756"                            /* ❖ */

#ifdef __cplusplus
}
#endif
