/* SPDX-License-Identifier: GPL-2.0-or-later */

namespace blender::io::obj {

/* Note: these OBJ parser helper functions are planned to get fairly large
 * changes "soon", so don't read too much into current implementation... */

void read_next_line(std::fstream &file, std::string &r_line);
void split_line_key_rest(StringRef line, StringRef &r_line_key, StringRef &r_rest_line);
void split_by_char(StringRef in_string, const char delimiter, Vector<StringRef> &r_out_list);
void copy_string_to_float(StringRef src, const float fallback_value, float &r_dst);
void copy_string_to_float(Span<StringRef> src,
                          const float fallback_value,
                          MutableSpan<float> r_dst);
void copy_string_to_int(StringRef src, const int fallback_value, int &r_dst);
void copy_string_to_int(Span<StringRef> src, const int fallback_value, MutableSpan<int> r_dst);
std::string replace_all_occurences(StringRef original, StringRef to_remove, StringRef to_add);

}  // namespace blender::io::obj
