/* SPDX-License-Identifier: GPL-2.0-or-later */

namespace blender::io::obj {

/* Note: these OBJ parser helper functions are planned to get fairly large
 * changes "soon", so don't read too much into current implementation... */

/**
 * Store multiple lines separated by an escaped newline character: `\\n`.
 * Use this before doing any parse operations on the read string.
 */
void read_next_line(std::fstream &file, std::string &r_line);
/**
 * Split a line string into the first word (key) and the rest of the line.
 * Also remove leading & trailing spaces as well as `\r` carriage return
 * character if present.
 */
void split_line_key_rest(StringRef line, StringRef &r_line_key, StringRef &r_rest_line);
/**
 * Split the given string by the delimiter and fill the given vector.
 * If an intermediate string is empty, or space or null character, it is not appended to the
 * vector.
 */
void split_by_char(StringRef in_string, const char delimiter, Vector<StringRef> &r_out_list);
/**
 * Convert the given string to float and assign it to the destination value.
 *
 * If the string cannot be converted to a float, the fallback value is used.
 */
void copy_string_to_float(StringRef src, const float fallback_value, float &r_dst);
/**
 * Convert all members of the Span of strings to floats and assign them to the float
 * array members. Usually used for values like coordinates.
 *
 * If a string cannot be converted to a float, the fallback value is used.
 */
void copy_string_to_float(Span<StringRef> src,
                          const float fallback_value,
                          MutableSpan<float> r_dst);
/**
 * Convert the given string to int and assign it to the destination value.
 *
 * If the string cannot be converted to an integer, the fallback value is used.
 */
void copy_string_to_int(StringRef src, const int fallback_value, int &r_dst);
/**
 * Convert the given strings to ints and fill the destination int buffer.
 *
 * If a string cannot be converted to an integer, the fallback value is used.
 */
void copy_string_to_int(Span<StringRef> src, const int fallback_value, MutableSpan<int> r_dst);
std::string replace_all_occurences(StringRef original, StringRef to_remove, StringRef to_add);

}  // namespace blender::io::obj
