/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Unicode characters as UTF-8 strings.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Notes:
 * - Last portion should include the official assigned name.
 * - Please do not add defines here that are not actually in use.
 * - Use literal UTF-8 encoding as this `u8` prefixes cause both
 *   `-Wc++20-compat` & `-Wc99-compat` warnings under CLANG.
 */

/** u00D7: `×`. */
#define BLI_STR_UTF8_MULTIPLICATION_SIGN "\xc3\x97"
/** u2014: `—` */
#define BLI_STR_UTF8_EM_DASH "\xe2\x80\x94"
/** u2022: `•` */
#define BLI_STR_UTF8_BULLET "\xe2\x80\xa2"
/** u2026: `…` */
#define BLI_STR_UTF8_HORIZONTAL_ELLIPSIS "\xe2\x80\xa6"
/** u2190: `←` */
#define BLI_STR_UTF8_LEFTWARDS_ARROW "\xe2\x86\x90"
/** u2191: `↑` */
#define BLI_STR_UTF8_UPWARDS_ARROW "\xe2\x86\x91"
/** u2192: `→` */
#define BLI_STR_UTF8_RIGHTWARDS_ARROW "\xe2\x86\x92"
/** u2193: `↓` */
#define BLI_STR_UTF8_DOWNWARDS_ARROW "\xe2\x86\x93"
/** u21E7: `⇧` */
#define BLI_STR_UTF8_UPWARDS_WHITE_ARROW "\xe2\x87\xa7"
/** u2303: `⌃` */
#define BLI_STR_UTF8_UP_ARROWHEAD "\xe2\x8c\x83"
/** u2318: `⌘` */
#define BLI_STR_UTF8_PLACE_OF_INTEREST_SIGN "\xe2\x8c\x98"
/** u2325: `⌥` */
#define BLI_STR_UTF8_OPTION_KEY "\xe2\x8c\xa5"
/** u232B: `⌫` */
#define BLI_STR_UTF8_ERASE_TO_THE_LEFT "\xe2\x8c\xab"
/** u238B: `⎋` */
#define BLI_STR_UTF8_BROKEN_CIRCLE_WITH_NORTHWEST_ARROW "\xe2\x8e\x8b"
/** u23CE: `⏎` */
#define BLI_STR_UTF8_RETURN_SYMBOL "\xe2\x8f\x8e"
/** u23ED: `⏭` */
#define BLI_STR_UTF8_BLACK_RIGHT_POINTING_DOUBLE_TRIANGLE_WITH_VERTICAL_BAR "\xe2\x8f\xad"
/** u23EE: `⏮` */
#define BLI_STR_UTF8_BLACK_LEFT_POINTING_DOUBLE_TRIANGLE_WITH_VERTICAL_BAR "\xe2\x8f\xae"
/** u23EF: `⏯` */
#define BLI_STR_UTF8_BLACK_RIGHT_POINTING_TRIANGLE_WITH_DOUBLE_VERTICAL_BAR "\xe2\x8f\xaf"
/** u23F9: `⏹` */
#define BLI_STR_UTF8_BLACK_SQUARE_FOR_STOP "\xe2\x8f\xb9"
/** u2423: `␣` */
#define BLI_STR_UTF8_OPEN_BOX "\xe2\x90\xa3"
/** u25B8: `▸` */
#define BLI_STR_UTF8_BLACK_RIGHT_POINTING_SMALL_TRIANGLE "\xe2\x96\xb8"
/** u2B7E: `⭾` */
#define BLI_STR_UTF8_HORIZONTAL_TAB_KEY "\xe2\xad\xbe"
/** u2756: `❖` */
#define BLI_STR_UTF8_BLACK_DIAMOND_MINUS_WHITE_X "\xe2\x9d\x96"

#ifdef __cplusplus
}
#endif
