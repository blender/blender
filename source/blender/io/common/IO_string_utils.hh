/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

/*
 * Various text parsing utilities commonly used by text-based input formats.
 */

namespace blender::io {

/**
 * Fetches next line from an input string buffer.
 *
 * The returned line will not have '\n' characters at the end;
 * the `buffer` is modified to contain remaining text without
 * the input line.
 *
 * Note that backslash (\) character is treated as a line
 * continuation, similar to OBJ file format or a C preprocessor.
 */
StringRef read_next_line(StringRef &buffer);

/**
 * Drop leading white-space from a StringRef.
 * Note that backslash character is considered white-space.
 */
StringRef drop_whitespace(StringRef str);

/**
 * Drop leading non-white-space from a StringRef.
 * Note that backslash character is considered white-space.
 */
StringRef drop_non_whitespace(StringRef str);

/**
 * Parse an integer from an input string.
 * The parsed result is stored in `dst`. The function skips
 * leading white-space unless `skip_space=false`. If the
 * number can't be parsed (invalid syntax, out of range),
 * `fallback` value is stored instead.
 *
 * Returns the remainder of the input string after parsing.
 */
StringRef parse_int(StringRef str, int fallback, int &dst, bool skip_space = true);

/**
 * Parse a float from an input string.
 * The parsed result is stored in `dst`. The function skips
 * leading white-space unless `skip_space=false`. If the
 * number can't be parsed (invalid syntax, out of range),
 * `fallback` value is stored instead.
 *
 * Returns the remainder of the input string after parsing.
 */
StringRef parse_float(StringRef str, float fallback, float &dst, bool skip_space = true);

/**
 * Parse a number of white-space separated floats from an input string.
 * The parsed `count` numbers are stored in `dst`. If a
 * number can't be parsed (invalid syntax, out of range),
 * `fallback` value is stored instead.
 *
 * Returns the remainder of the input string after parsing.
 */
StringRef parse_floats(StringRef str, float fallback, float *dst, int count);

}  // namespace blender::io
