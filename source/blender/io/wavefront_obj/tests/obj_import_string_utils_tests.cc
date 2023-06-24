/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "obj_import_string_utils.hh"

#include "testing/testing.h"

namespace blender::io::obj {

#define EXPECT_STRREF_EQ(str1, str2) EXPECT_STREQ(str1, std::string(str2).c_str())

TEST(obj_import_string_utils, read_next_line)
{
  std::string str = "abc\n  \n\nline with \t spaces\nCRLF ending:\r\na";
  StringRef s = str;
  EXPECT_STRREF_EQ("abc", read_next_line(s));
  EXPECT_STRREF_EQ("  ", read_next_line(s));
  EXPECT_STRREF_EQ("", read_next_line(s));
  EXPECT_STRREF_EQ("line with \t spaces", read_next_line(s));
  EXPECT_STRREF_EQ("CRLF ending:\r", read_next_line(s));
  EXPECT_STRREF_EQ("a", read_next_line(s));
  EXPECT_TRUE(s.is_empty());
}

TEST(obj_import_string_utils, fixup_line_continuations)
{
  const char *str =
      "backslash \\\n eol\n"
      "backslash spaces \\   \n eol\n"
      "without eol \\ is \\\\ \\ left intact\n"
      "\\";
  const char *exp =
      "backslash    eol\n"
      "backslash spaces       eol\n"
      "without eol \\ is \\\\ \\ left intact\n"
      "\\";
  std::string buf(str);
  fixup_line_continuations(buf.data(), buf.data() + buf.size());
  EXPECT_STRREF_EQ(exp, buf);
}

static StringRef drop_whitespace(StringRef s)
{
  return StringRef(drop_whitespace(s.begin(), s.end()), s.end());
}
static StringRef parse_int(StringRef s, int fallback, int &dst, bool skip_space = true)
{
  return StringRef(parse_int(s.begin(), s.end(), fallback, dst, skip_space), s.end());
}
static StringRef parse_float(StringRef s,
                             float fallback,
                             float &dst,
                             bool skip_space = true,
                             bool require_trailing_space = false)
{
  return StringRef(
      parse_float(s.begin(), s.end(), fallback, dst, skip_space, require_trailing_space), s.end());
}

TEST(obj_import_string_utils, drop_whitespace)
{
  /* Empty */
  EXPECT_STRREF_EQ("", drop_whitespace(""));
  /* Only whitespace */
  EXPECT_STRREF_EQ("", drop_whitespace(" "));
  EXPECT_STRREF_EQ("", drop_whitespace("   "));
  EXPECT_STRREF_EQ("", drop_whitespace(" \t\n\r "));
  /* Drops leading whitespace */
  EXPECT_STRREF_EQ("a", drop_whitespace(" a"));
  EXPECT_STRREF_EQ("a b", drop_whitespace("   a b"));
  EXPECT_STRREF_EQ("a b   ", drop_whitespace(" a b   "));
  /* No leading whitespace */
  EXPECT_STRREF_EQ("c", drop_whitespace("c"));
  /* Case with backslash, should be treated as whitespace */
  EXPECT_STRREF_EQ("d", drop_whitespace(" \t d"));
}

TEST(obj_import_string_utils, parse_int_valid)
{
  std::string str = "1 -10 \t  1234 1234567890 +7 123a";
  StringRef s = str;
  int val;
  s = parse_int(s, 0, val);
  EXPECT_EQ(1, val);
  s = parse_int(s, 0, val);
  EXPECT_EQ(-10, val);
  s = parse_int(s, 0, val);
  EXPECT_EQ(1234, val);
  s = parse_int(s, 0, val);
  EXPECT_EQ(1234567890, val);
  s = parse_int(s, 0, val);
  EXPECT_EQ(7, val);
  s = parse_int(s, 0, val);
  EXPECT_EQ(123, val);
  EXPECT_STRREF_EQ("a", s);
}

TEST(obj_import_string_utils, parse_int_invalid)
{
  int val;
  /* Invalid syntax */
  EXPECT_STRREF_EQ("--123", parse_int("--123", -1, val));
  EXPECT_EQ(val, -1);
  EXPECT_STRREF_EQ("foobar", parse_int("foobar", -2, val));
  EXPECT_EQ(val, -2);
  /* Out of integer range */
  EXPECT_STRREF_EQ(" a", parse_int("1234567890123 a", -3, val));
  EXPECT_EQ(val, -3);
  /* Has leading white-space when we don't expect it */
  EXPECT_STRREF_EQ(" 1", parse_int(" 1", -4, val, false));
  EXPECT_EQ(val, -4);
}

TEST(obj_import_string_utils, parse_float_valid)
{
  std::string str = "1 -10 123.5 -17.125 0.1 1e6 50.0e-1";
  StringRef s = str;
  float val;
  s = parse_float(s, 0, val);
  EXPECT_EQ(1.0f, val);
  s = parse_float(s, 0, val);
  EXPECT_EQ(-10.0f, val);
  s = parse_float(s, 0, val);
  EXPECT_EQ(123.5f, val);
  s = parse_float(s, 0, val);
  EXPECT_EQ(-17.125f, val);
  s = parse_float(s, 0, val);
  EXPECT_EQ(0.1f, val);
  s = parse_float(s, 0, val);
  EXPECT_EQ(1.0e6f, val);
  s = parse_float(s, 0, val);
  EXPECT_EQ(5.0f, val);
  EXPECT_TRUE(s.is_empty());
}

TEST(obj_import_string_utils, parse_float_invalid)
{
  float val;
  /* Invalid syntax */
  EXPECT_STRREF_EQ("_0", parse_float("_0", -1.0f, val));
  EXPECT_EQ(val, -1.0f);
  EXPECT_STRREF_EQ("..5", parse_float("..5", -2.0f, val));
  EXPECT_EQ(val, -2.0f);
  /* Out of float range. */
  EXPECT_STRREF_EQ(" a", parse_float("9.0e500 a", -3.0f, val));
  EXPECT_EQ(val, -3.0f);
  /* Has leading white-space when we don't expect it */
  EXPECT_STRREF_EQ(" 1", parse_float(" 1", -4.0f, val, false));
  EXPECT_EQ(val, -4.0f);
  /* Has trailing non-number characters when we don't want them */
  EXPECT_STRREF_EQ("123.5.png", parse_float("  123.5.png", -5.0f, val, true, true));
  EXPECT_EQ(val, -5.0f);
}

}  // namespace blender::io::obj
