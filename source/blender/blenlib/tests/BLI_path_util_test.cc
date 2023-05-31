/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "IMB_imbuf.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void str_replace_char_with_relative_exception(char *str, char src, char dst)
{
  /* Always keep "//" or more leading slashes (special meaning). */
  if (src == '/') {
    if (str[0] == '/' && str[1] == '/') {
      str += 2;
      while (*str == '/') {
        str++;
      }
    }
  }
  BLI_str_replace_char(str, src, dst);
}

static char *str_replace_char_strdup(const char *str, char src, char dst)
{
  if (str == nullptr) {
    return nullptr;
  }
  char *str_dupe = strdup(str);
  BLI_str_replace_char(str_dupe, src, dst);
  return str_dupe;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_normalize
 * \{ */

#define NORMALIZE(input, output_expect) \
  { \
    char path[FILE_MAX] = input; \
    if (SEP == '\\') { \
      str_replace_char_with_relative_exception(path, '/', '\\'); \
    } \
    const int path_len_test = BLI_path_normalize(path); \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '\\', '/'); \
    } \
    EXPECT_STREQ(path, output_expect); \
    EXPECT_EQ(path_len_test, strlen(path)); \
  } \
  ((void)0)

/* #BLI_path_normalize: do nothing. */
TEST(path_util, Normalize_Nop)
{
  NORMALIZE(".", ".");
  NORMALIZE("./", "./");
  NORMALIZE("/", "/");
  NORMALIZE("//", "//");
  NORMALIZE("//a", "//a");
}

TEST(path_util, Normalize_NopRelative)
{
  NORMALIZE("..", "..");
  NORMALIZE("../", "../");
  NORMALIZE("../", "../");
  NORMALIZE("../..", "../..");
  NORMALIZE("../../", "../../");
}

/* #BLI_path_normalize: "/./" -> "/" */
TEST(path_util, Normalize_Dot)
{
  NORMALIZE("/./", "/");
  NORMALIZE("/a/./b/./c/./", "/a/b/c/");
  NORMALIZE("/./././", "/");
  NORMALIZE("/a/./././b/", "/a/b/");
}
/* #BLI_path_normalize: complex "/./" -> "/", "//" -> "/", "./path/../" -> "./". */
TEST(path_util, Normalize_ComplexAbsolute)
{
  NORMALIZE("/a/./b/./c/./.././.././", "/a/");
  NORMALIZE("/a//.//b//.//c//.//..//.//..//.//", "/a/");
}
TEST(path_util, Normalize_ComplexRelative)
{
  NORMALIZE("a/b/c/d/e/f/g/../a/../b/../../c/../../../d/../../../..", ".");
  NORMALIZE("a/b/c/d/e/f/g/../a/../../../../b/../../../c/../../d/..", ".");
}
/* #BLI_path_normalize: "//" -> "/" */
TEST(path_util, Normalize_DoubleSlash)
{
  NORMALIZE("//", "//"); /* Exception, double forward slash. */
  NORMALIZE(".//", "./");
  NORMALIZE("a////", "a/");
  NORMALIZE("./a////", "a/");
}
/* #BLI_path_normalize: "foo/bar/../" -> "foo/" */
TEST(path_util, Normalize_Parent)
{
  NORMALIZE("/a/b/c/../../../", "/");
  NORMALIZE("/a/../a/b/../b/c/../c/", "/a/b/c/");
}
/* #BLI_path_normalize: with too many "/../", match Python's behavior. */
TEST(path_util, Normalize_UnbalancedAbsolute)
{
  NORMALIZE("/../", "/");
  NORMALIZE("/../a", "/a");
  NORMALIZE("/a/b/c/../../../../../d", "/d");
  NORMALIZE("/a/b/c/../../../../d", "/d");
  NORMALIZE("/a/b/c/../../../d", "/d");

  /* Use a longer path as it may hit corner cases. */
  NORMALIZE("/home/username/Downloads/../../../../../Users/Example/Desktop/test.jpg",
            "/Users/Example/Desktop/test.jpg");
}

/* #BLI_path_normalize: with relative paths that result in leading "../". */
TEST(path_util, Normalize_UnbalancedRelative)
{
  NORMALIZE("./a/b/c/../../../", ".");
  NORMALIZE("a/b/c/../../../", ".");
  NORMALIZE("//a/b/c/../../../", "//");

  NORMALIZE("./a/../../../", "../../");
  NORMALIZE("a/../../../", "../../");

  NORMALIZE("///a/../../../", "//../../");
  NORMALIZE("//./a/../../../", "//../../");

  NORMALIZE("../a/../../../", "../../../");
  NORMALIZE("a/b/../c/../../d/../../../e/../../../../f", "../../../../../f");
  NORMALIZE(".../.../a/.../b/../c/../../d/../../../e/../../../.../../f", "../f");
}

TEST(path_util, Normalize_UnbalancedRelativeTrailing)
{
  NORMALIZE("./a/b/c/../../..", ".");
  NORMALIZE("a/b/c/../../..", ".");
  NORMALIZE("//a/b/c/../../..", "//");

  NORMALIZE("./a/../../..", "../..");
  NORMALIZE("a/../../..", "../..");

  NORMALIZE("///a/../../..", "//../..");
  NORMALIZE("//./a/../../..", "//../..");

  NORMALIZE("../a/../../..", "../../..");
}

#undef NORMALIZE

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_cmp_normalized
 *
 * \note #BLI_path_normalize tests handle most of the corner cases.
 * \{ */

TEST(path_util, CompareNormalized)
{
  /* Trailing slash should not matter. */
  EXPECT_EQ(BLI_path_cmp_normalized("/tmp/", "/tmp"), 0);
  /* Slash direction should not matter. */
  EXPECT_EQ(BLI_path_cmp_normalized("c:\\tmp\\", "c:/tmp/"), 0);
  /* Empty paths should be supported. */
  EXPECT_EQ(BLI_path_cmp_normalized("", ""), 0);

  EXPECT_NE(BLI_path_cmp_normalized("A", "B"), 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_parent_dir
 * \{ */

#define PARENT_DIR(input, output_expect) \
  { \
    char path[FILE_MAX] = input; \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '/', '\\'); \
    } \
    BLI_path_parent_dir(path); \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '\\', '/'); \
    } \
    EXPECT_STREQ(path, output_expect); \
  } \
  ((void)0)

TEST(path_util, ParentDir_Simple)
{
  PARENT_DIR("/a/b/", "/a/");
  PARENT_DIR("/a/b", "/a/");
  PARENT_DIR("/a", "/");
}

TEST(path_util, ParentDir_NOP)
{
  PARENT_DIR("/", "/");
  PARENT_DIR("", "");
  PARENT_DIR(".", ".");
  PARENT_DIR("./", "./");
  PARENT_DIR(".//", ".//");
  PARENT_DIR("./.", "./.");
}

TEST(path_util, ParentDir_TrailingPeriod)
{
  /* Ensure trailing dots aren't confused with parent path. */
  PARENT_DIR("/.../.../.../", "/.../.../");
  PARENT_DIR("/.../.../...", "/.../.../");

  PARENT_DIR("/a../b../c../", "/a../b../");
  PARENT_DIR("/a../b../c..", "/a../b../");

  PARENT_DIR("/a./b./c./", "/a./b./");
  PARENT_DIR("/a./b./c.", "/a./b./");
}

TEST(path_util, ParentDir_Complex)
{
  PARENT_DIR("./a/", "./");
  PARENT_DIR("./a", "./");
  PARENT_DIR("../a/", "../");
  PARENT_DIR("../a", "../");
}

#undef PARENT_DIR

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_name_at_index
 * \{ */

#define AT_INDEX(str_input, index_input, str_expect) \
  { \
    char path[] = str_input; \
    /* Test input assumes forward slash, support back-slash on WIN32. */ \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '/', '\\'); \
    } \
    const char *expect = str_expect; \
    int index_output, len_output; \
    const bool ret = BLI_path_name_at_index(path, index_input, &index_output, &len_output); \
    if (expect == nullptr) { \
      EXPECT_FALSE(ret); \
    } \
    else { \
      EXPECT_TRUE(ret); \
      EXPECT_EQ(len_output, strlen(expect)); \
      path[index_output + len_output] = '\0'; \
      EXPECT_STREQ(&path[index_output], expect); \
    } \
  } \
  ((void)0)

TEST(path_util, NameAtIndex_Single)
{
  AT_INDEX("/a", 0, "a");
  AT_INDEX("/a/", 0, "a");
  AT_INDEX("a/", 0, "a");
  AT_INDEX("//a//", 0, "a");
  AT_INDEX("a/b", 0, "a");

  AT_INDEX("/a", 1, nullptr);
  AT_INDEX("/a/", 1, nullptr);
  AT_INDEX("a/", 1, nullptr);
  AT_INDEX("//a//", 1, nullptr);
}
TEST(path_util, NameAtIndex_SingleNeg)
{
  AT_INDEX("/a", -1, "a");
  AT_INDEX("/a/", -1, "a");
  AT_INDEX("a/", -1, "a");
  AT_INDEX("//a//", -1, "a");
  AT_INDEX("a/b", -1, "b");

  AT_INDEX("/a", -2, nullptr);
  AT_INDEX("/a/", -2, nullptr);
  AT_INDEX("a/", -2, nullptr);
  AT_INDEX("//a//", -2, nullptr);
}

TEST(path_util, NameAtIndex_Double)
{
  AT_INDEX("/ab", 0, "ab");
  AT_INDEX("/ab/", 0, "ab");
  AT_INDEX("ab/", 0, "ab");
  AT_INDEX("//ab//", 0, "ab");
  AT_INDEX("ab/c", 0, "ab");

  AT_INDEX("/ab", 1, nullptr);
  AT_INDEX("/ab/", 1, nullptr);
  AT_INDEX("ab/", 1, nullptr);
  AT_INDEX("//ab//", 1, nullptr);
}

TEST(path_util, NameAtIndex_DoublNeg)
{
  AT_INDEX("/ab", -1, "ab");
  AT_INDEX("/ab/", -1, "ab");
  AT_INDEX("ab/", -1, "ab");
  AT_INDEX("//ab//", -1, "ab");
  AT_INDEX("ab/c", -1, "c");

  AT_INDEX("/ab", -2, nullptr);
  AT_INDEX("/ab/", -2, nullptr);
  AT_INDEX("ab/", -2, nullptr);
  AT_INDEX("//ab//", -2, nullptr);
}

TEST(path_util, NameAtIndex_Misc)
{
  AT_INDEX("/how/now/brown/cow", 0, "how");
  AT_INDEX("/how/now/brown/cow", 1, "now");
  AT_INDEX("/how/now/brown/cow", 2, "brown");
  AT_INDEX("/how/now/brown/cow", 3, "cow");
  AT_INDEX("/how/now/brown/cow", 4, nullptr);
  AT_INDEX("/how/now/brown/cow/", 4, nullptr);
}

TEST(path_util, NameAtIndex_MiscNeg)
{
  AT_INDEX("/how/now/brown/cow", 0, "how");
  AT_INDEX("/how/now/brown/cow", 1, "now");
  AT_INDEX("/how/now/brown/cow", 2, "brown");
  AT_INDEX("/how/now/brown/cow", 3, "cow");
  AT_INDEX("/how/now/brown/cow", 4, nullptr);
  AT_INDEX("/how/now/brown/cow/", 4, nullptr);
}

#define TEST_STR "./a/./b/./c/."

TEST(path_util, NameAtIndex_SingleDot)
{
  AT_INDEX(TEST_STR, 0, ".");
  AT_INDEX(TEST_STR, 1, "a");
  AT_INDEX(TEST_STR, 2, "b");
  AT_INDEX(TEST_STR, 3, "c");
  AT_INDEX(TEST_STR, 4, nullptr);
}

TEST(path_util, NameAtIndex_SingleDotNeg)
{
  AT_INDEX(TEST_STR, -5, nullptr);
  AT_INDEX(TEST_STR, -4, ".");
  AT_INDEX(TEST_STR, -3, "a");
  AT_INDEX(TEST_STR, -2, "b");
  AT_INDEX(TEST_STR, -1, "c");
}

#undef TEST_STR

#define TEST_STR ".//a//.//b//.//c//.//"

TEST(path_util, NameAtIndex_SingleDotDoubleSlash)
{
  AT_INDEX(TEST_STR, 0, ".");
  AT_INDEX(TEST_STR, 1, "a");
  AT_INDEX(TEST_STR, 2, "b");
  AT_INDEX(TEST_STR, 3, "c");
  AT_INDEX(TEST_STR, 4, nullptr);
}

TEST(path_util, NameAtIndex_SingleDotDoubleSlashNeg)
{
  AT_INDEX(TEST_STR, -5, nullptr);
  AT_INDEX(TEST_STR, -4, ".");
  AT_INDEX(TEST_STR, -3, "a");
  AT_INDEX(TEST_STR, -2, "b");
  AT_INDEX(TEST_STR, -1, "c");
}

#undef TEST_STR

TEST(path_util, NameAtIndex_SingleDotSeries)
{
  AT_INDEX("abc/././/././xyz", 0, "abc");
  AT_INDEX("abc/././/././xyz", 1, "xyz");
  AT_INDEX("abc/././/././xyz", 2, nullptr);
}

TEST(path_util, NameAtIndex_SingleDotSeriesNeg)
{
  AT_INDEX("abc/././/././xyz", -3, nullptr);
  AT_INDEX("abc/././/././xyz", -2, "abc");
  AT_INDEX("abc/././/././xyz", -1, "xyz");
}

TEST(path_util, NameAtIndex_MiscComplex)
{
  AT_INDEX("how//now/brown/cow", 0, "how");
  AT_INDEX("//how///now//brown/cow", 1, "now");
  AT_INDEX("/how/now///brown//cow", 2, "brown");
  AT_INDEX("/how/now/brown/cow///", 3, "cow");
  AT_INDEX("/how/now/brown//cow", 4, nullptr);
  AT_INDEX("how/now/brown//cow/", 4, nullptr);
}

TEST(path_util, NameAtIndex_MiscComplexNeg)
{
  AT_INDEX("how//now/brown/cow", -4, "how");
  AT_INDEX("//how///now//brown/cow", -3, "now");
  AT_INDEX("/how/now///brown//cow", -2, "brown");
  AT_INDEX("/how/now/brown/cow///", -1, "cow");
  AT_INDEX("/how/now/brown//cow", -5, nullptr);
  AT_INDEX("how/now/brown//cow/", -5, nullptr);
}

TEST(path_util, NameAtIndex_NoneComplex)
{
  AT_INDEX("", 0, nullptr);
  AT_INDEX("/", 0, nullptr);
  AT_INDEX("//", 0, nullptr);
  AT_INDEX("///", 0, nullptr);
}

TEST(path_util, NameAtIndex_NoneComplexNeg)
{
  AT_INDEX("", -1, nullptr);
  AT_INDEX("/", -1, nullptr);
  AT_INDEX("//", -1, nullptr);
  AT_INDEX("///", -1, nullptr);
}

#undef AT_INDEX

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_join
 * \{ */

/* For systems with `/` path separator (non WIN32). */
#define JOIN_FORWARD_SLASH(str_expect, out_size, ...) \
  { \
    const char *expect = str_expect; \
    char result[(out_size) + 1024]; \
    /* Check we don't write past the last byte. */ \
    result[out_size] = '\0'; \
    BLI_path_join(result, out_size, __VA_ARGS__); \
    EXPECT_STREQ(result, expect); \
    EXPECT_EQ(result[out_size], '\0'); \
  } \
  ((void)0)

/* For systems with `\` path separator (WIN32).
 * Perform additional manipulation to behave as if input arguments used `\` separators.
 * Needed since #BLI_path_join uses native slashes. */
#define JOIN_BACK_SLASH(str_expect, out_size, ...) \
  { \
    const char *expect = str_expect; \
    char result[(out_size) + 1024]; \
    const char *input_forward_slash[] = {__VA_ARGS__}; \
    char *input_back_slash[ARRAY_SIZE(input_forward_slash)] = {nullptr}; \
    for (int i = 0; i < ARRAY_SIZE(input_forward_slash); i++) { \
      input_back_slash[i] = strdup(input_forward_slash[i]); \
      BLI_str_replace_char(input_back_slash[i], '/', '\\'); \
    } \
    /* Check we don't write past the last byte. */ \
    result[out_size] = '\0'; \
    BLI_path_join_array(result, \
                        out_size, \
                        const_cast<const char **>(input_back_slash), \
                        ARRAY_SIZE(input_back_slash)); \
    BLI_str_replace_char(result, '\\', '/'); \
    EXPECT_STREQ(result, expect); \
    EXPECT_EQ(result[out_size], '\0'); \
    for (int i = 0; i < ARRAY_SIZE(input_forward_slash); i++) { \
      free(input_back_slash[i]); \
    } \
  } \
  ((void)0)

#ifdef WIN32
#  define JOIN JOIN_BACK_SLASH
#else
#  define JOIN JOIN_FORWARD_SLASH
#endif

TEST(path_util, JoinNop)
{
  JOIN("", 100, "");
  JOIN("", 100, "", "");
  JOIN("", 100, "", "", "");
  JOIN("/", 100, "/", "", "");
  JOIN("/", 100, "/", "/");
  JOIN("/", 100, "/", "", "/");
  JOIN("/", 100, "/", "", "/", "");
}

TEST(path_util, JoinSingle)
{
  JOIN("test", 100, "test");
  JOIN("", 100, "");
  JOIN("a", 100, "a");
  JOIN("/a", 100, "/a");
  JOIN("a/", 100, "a/");
  JOIN("/a/", 100, "/a/");
  JOIN("/a/", 100, "/a//");
  JOIN("//a/", 100, "//a//");
}

TEST(path_util, JoinTriple)
{
  JOIN("/a/b/c", 100, "/a", "b", "c");
  JOIN("/a/b/c", 100, "/a/", "/b/", "/c");
  JOIN("/a/b/c", 100, "/a/b/", "/c");
  JOIN("/a/b/c", 100, "/a/b/c");
  JOIN("/a/b/c", 100, "/", "a/b/c");

  JOIN("/a/b/c/", 100, "/a/", "/b/", "/c/");
  JOIN("/a/b/c/", 100, "/a/b/c/");
  JOIN("/a/b/c/", 100, "/a/b/", "/c/");
  JOIN("/a/b/c/", 100, "/a/b/c", "/");
  JOIN("/a/b/c/", 100, "/", "a/b/c", "/");
}

TEST(path_util, JoinTruncateShort)
{
  JOIN("", 1, "/");
  JOIN("/", 2, "/");
  JOIN("a", 2, "", "aa");
  JOIN("a", 2, "", "a/");
  JOIN("a/b", 4, "a", "bc");
  JOIN("ab/", 4, "ab", "c");
  JOIN("/a/", 4, "/a", "b");
  JOIN("/a/", 4, "/a/", "b/");
  JOIN("/a/", 4, "/a", "/b/");
  JOIN("/a/", 4, "/", "a/b/");
  JOIN("//a", 4, "//", "a/b/");

  JOIN("/a/b", 5, "/a", "b", "c");
}

TEST(path_util, JoinTruncateLong)
{
  JOIN("", 1, "//", "//longer", "path");
  JOIN("/", 2, "//", "//longer", "path");
  JOIN("//", 3, "//", "//longer", "path");
  JOIN("//l", 4, "//", "//longer", "path");
  /* snip */
  JOIN("//longe", 8, "//", "//longer", "path");
  JOIN("//longer", 9, "//", "//longer", "path");
  JOIN("//longer/", 10, "//", "//longer", "path");
  JOIN("//longer/p", 11, "//", "//longer", "path");
  JOIN("//longer/pa", 12, "//", "//longer", "path");
  JOIN("//longer/pat", 13, "//", "//longer", "path");
  JOIN("//longer/path", 14, "//", "//longer", "path"); /* not truncated. */
  JOIN("//longer/path", 14, "//", "//longer", "path/");
  JOIN("//longer/path/", 15, "//", "//longer", "path/"); /* not truncated. */
  JOIN("//longer/path/", 15, "//", "//longer", "path/", "trunc");
  JOIN("//longer/path/t", 16, "//", "//longer", "path/", "trunc");
}

TEST(path_util, JoinComplex)
{
  JOIN("/a/b/c/d/e/f/g/", 100, "/", "a/b", "//////c/d", "", "e", "f", "g//");
  JOIN("/aa/bb/cc/dd/ee/ff/gg/", 100, "/", "aa/bb", "//////cc/dd", "", "ee", "ff", "gg//");
  JOIN("1/2/3/", 100, "1", "////////", "", "2", "3///");
}

TEST(path_util, JoinRelativePrefix)
{
  JOIN("//a/b/c", 100, "//a", "b", "c");
  JOIN("//a/b/c", 100, "//", "//a//", "//b//", "//c");
  JOIN("//a/b/c", 100, "//", "//", "a", "//", "b", "//", "c");
}

#undef JOIN
#undef JOIN_BACK_SLASH
#undef JOIN_FORWARD_SLASH

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_append
 * \{ */

/* For systems with `/` path separator (non WIN32). */
#define APPEND(str_expect, size, path, filename) \
  { \
    const char *expect = str_expect; \
    char result[(size) + 1024] = path; \
    char filename_native[] = filename; \
    /* Check we don't write past the last byte. */ \
    if (SEP == '\\') { \
      BLI_str_replace_char(filename_native, '/', '\\'); \
      BLI_str_replace_char(result, '/', '\\'); \
    } \
    BLI_path_append(result, size, filename_native); \
    if (SEP == '\\') { \
      BLI_str_replace_char(result, '\\', '/'); \
    } \
    EXPECT_STREQ(result, expect); \
  } \
  ((void)0)

TEST(path_util, AppendFile)
{
  APPEND("a/b", 100, "a", "b");
  APPEND("a/b", 100, "a/", "b");
}

TEST(path_util, AppendFile_Truncate)
{
  APPEND("/A", 3, "/", "ABC");
  APPEND("/", 2, "/", "test");
  APPEND("X", 2, "X", "ABC");
  APPEND("X/", 3, "X/", "ABC");
}

#undef APPEND

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_frame
 * \{ */

TEST(path_util, Frame)
{
  bool ret;

  {
    char path[FILE_MAX] = "";
    ret = BLI_path_frame(path, sizeof(path), 123, 1);
    EXPECT_TRUE(ret);
    EXPECT_STREQ(path, "123");
  }

  {
    char path[FILE_MAX] = "";
    ret = BLI_path_frame(path, sizeof(path), 123, 12);
    EXPECT_TRUE(ret);
    EXPECT_STREQ(path, "000000000123");
  }

  {
    char path[FILE_MAX] = "test_";
    ret = BLI_path_frame(path, sizeof(path), 123, 1);
    EXPECT_TRUE(ret);
    EXPECT_STREQ(path, "test_123");
  }

  {
    char path[FILE_MAX] = "test_";
    ret = BLI_path_frame(path, sizeof(path), 1, 12);
    EXPECT_TRUE(ret);
    EXPECT_STREQ(path, "test_000000000001");
  }

  {
    char path[FILE_MAX] = "test_############";
    ret = BLI_path_frame(path, sizeof(path), 1, 0);
    EXPECT_TRUE(ret);
    EXPECT_STREQ(path, "test_000000000001");
  }

  {
    char path[FILE_MAX] = "test_#_#_middle";
    ret = BLI_path_frame(path, sizeof(path), 123, 0);
    EXPECT_TRUE(ret);
    EXPECT_STREQ(path, "test_#_123_middle");
  }

  /* intentionally fail */
  {
    char path[FILE_MAX] = "";
    ret = BLI_path_frame(path, sizeof(path), 123, 0);
    EXPECT_FALSE(ret);
    EXPECT_STREQ(path, "");
  }

  {
    char path[FILE_MAX] = "test_middle";
    ret = BLI_path_frame(path, sizeof(path), 123, 0);
    EXPECT_FALSE(ret);
    EXPECT_STREQ(path, "test_middle");
  }

  /* negative frame numbers */
  {
    char path[FILE_MAX] = "test_####";
    ret = BLI_path_frame(path, sizeof(path), -1, 4);
    EXPECT_TRUE(ret);
    EXPECT_STREQ(path, "test_-0001");
  }
  {
    char path[FILE_MAX] = "test_####";
    ret = BLI_path_frame(path, sizeof(path), -100, 4);
    EXPECT_TRUE(ret);
    EXPECT_STREQ(path, "test_-0100");
  }

  /* Ensure very large ranges work. */
  {
    char path[FILE_MAX * 2];
    memset(path, '#', sizeof(path));
    path[sizeof(path) - 1] = '\0';
    ret = BLI_path_frame(path, sizeof(path), 123456789, 0);
    EXPECT_TRUE(BLI_str_endswith(path, "0123456789"));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_split_dir_file
 * \{ */

TEST(path_util, SplitDirfile)
{
  {
    const char *path = "";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_path_split_dir_file(path, dir, sizeof(dir), file, sizeof(file));
    EXPECT_STREQ(dir, "");
    EXPECT_STREQ(file, "");
  }

  {
    const char *path = "/";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_path_split_dir_file(path, dir, sizeof(dir), file, sizeof(file));
    EXPECT_STREQ(dir, "/");
    EXPECT_STREQ(file, "");
  }

  {
    const char *path = "fileonly";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_path_split_dir_file(path, dir, sizeof(dir), file, sizeof(file));
    EXPECT_STREQ(dir, "");
    EXPECT_STREQ(file, "fileonly");
  }

  {
    const char *path = "dironly/";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_path_split_dir_file(path, dir, sizeof(dir), file, sizeof(file));
    EXPECT_STREQ(dir, "dironly/");
    EXPECT_STREQ(file, "");
  }

  {
    const char *path = "/a/b";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_path_split_dir_file(path, dir, sizeof(dir), file, sizeof(file));
    EXPECT_STREQ(dir, "/a/");
    EXPECT_STREQ(file, "b");
  }

  {
    const char *path = "/dirtoobig/filetoobig";
    char dir[5], file[5];
    BLI_path_split_dir_file(path, dir, sizeof(dir), file, sizeof(file));
    EXPECT_STREQ(dir, "/dir");
    EXPECT_STREQ(file, "file");

    BLI_path_split_dir_file(path, dir, 1, file, 1);
    EXPECT_STREQ(dir, "");
    EXPECT_STREQ(file, "");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_frame_strip
 * \{ */

#define PATH_FRAME_STRIP(input_path, expect_path, expect_ext) \
  { \
    char path[FILE_MAX]; \
    char ext[FILE_MAX]; \
    STRNCPY(path, (input_path)); \
    BLI_path_frame_strip(path, ext, sizeof(ext)); \
    EXPECT_STREQ(path, expect_path); \
    EXPECT_STREQ(ext, expect_ext); \
  } \
  ((void)0)

TEST(path_util, FrameStrip)
{
  PATH_FRAME_STRIP("", "", "");
  PATH_FRAME_STRIP("nonum.abc", "nonum", ".abc");
  PATH_FRAME_STRIP("fileonly.001.abc", "fileonly.###", ".abc");
  PATH_FRAME_STRIP("/abspath/to/somefile.001.abc", "/abspath/to/somefile.###", ".abc");
  PATH_FRAME_STRIP("/ext/longer/somefile.001.alembic", "/ext/longer/somefile.###", ".alembic");
  PATH_FRAME_STRIP("/ext/shorter/somefile.123001.abc", "/ext/shorter/somefile.######", ".abc");
}
#undef PATH_FRAME_STRIP

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_extension
 * \{ */

TEST(path_util, Extension)
{
  EXPECT_EQ(BLI_path_extension("some.def/file"), nullptr);
  EXPECT_EQ(BLI_path_extension("Text"), nullptr);
  EXPECT_EQ(BLI_path_extension("Text…001"), nullptr);
  EXPECT_EQ(BLI_path_extension(".hidden"), nullptr);
  EXPECT_EQ(BLI_path_extension(".hidden/"), nullptr);
  EXPECT_EQ(BLI_path_extension("/.hidden"), nullptr);
  EXPECT_EQ(BLI_path_extension("dir/.hidden"), nullptr);
  EXPECT_EQ(BLI_path_extension("/dir/.hidden"), nullptr);

  EXPECT_EQ(BLI_path_extension("."), nullptr);
  EXPECT_EQ(BLI_path_extension(".."), nullptr);
  EXPECT_EQ(BLI_path_extension("..."), nullptr);
  EXPECT_STREQ(BLI_path_extension("...a."), ".");
  EXPECT_STREQ(BLI_path_extension("...a.."), ".");
  EXPECT_EQ(BLI_path_extension("...a../"), nullptr);

  EXPECT_STREQ(BLI_path_extension("some/file."), ".");
  EXPECT_STREQ(BLI_path_extension("some/file.tar.gz"), ".gz");
  EXPECT_STREQ(BLI_path_extension("some.def/file.abc"), ".abc");
  EXPECT_STREQ(BLI_path_extension("C:\\some.def\\file.abc"), ".abc");
  EXPECT_STREQ(BLI_path_extension("Text.001"), ".001");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_extension_check
 * \{ */

#define PATH_EXTENSION_CHECK(input_path, input_ext, expect_ext) \
  { \
    const bool ret = BLI_path_extension_check(input_path, input_ext); \
    if (STREQ(input_ext, expect_ext)) { \
      EXPECT_TRUE(ret); \
    } \
    else { \
      EXPECT_FALSE(ret); \
    } \
  } \
  ((void)0)

TEST(path_util, ExtensionCheck)
{
  PATH_EXTENSION_CHECK("a/b/c.exe", ".exe", ".exe");
  PATH_EXTENSION_CHECK("correct/path/to/file.h", ".h", ".h");
  PATH_EXTENSION_CHECK("correct/path/to/file.BLEND", ".BLEND", ".BLEND");
  PATH_EXTENSION_CHECK("../tricky/path/to/file.h", ".h", ".h");
  PATH_EXTENSION_CHECK("../dirty//../path\\to/file.h", ".h", ".h");
  PATH_EXTENSION_CHECK("a/b/c.veryveryverylonglonglongextension",
                       ".veryveryverylonglonglongextension",
                       ".veryveryverylonglonglongextension");
  PATH_EXTENSION_CHECK("filename.PNG", "pnG", "pnG");
  PATH_EXTENSION_CHECK("a/b/c.h.exe", ".exe", ".exe");
  PATH_EXTENSION_CHECK("a/b/c.h.exe", "exe", "exe");
  PATH_EXTENSION_CHECK("a/b/c.exe", "c.exe", "c.exe");
  PATH_EXTENSION_CHECK("a/b/noext", "noext", "noext");

  PATH_EXTENSION_CHECK("a/b/c.exe", ".png", ".exe");
  PATH_EXTENSION_CHECK("a/b/c.exe", "c.png", ".exe");
  PATH_EXTENSION_CHECK("a/b/s.l", "l.s", "s.l");
  PATH_EXTENSION_CHECK(".hiddenfolder", "", ".hiddenfolder");
  PATH_EXTENSION_CHECK("../dirty//../path\\to/actual.h.file.ext", ".h", ".ext");
  PATH_EXTENSION_CHECK("..\\dirty//../path//to/.hiddenfile.JPEG", ".hiddenfile", ".JPEG");
}
#undef PATH_EXTENSION_CHECK

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_extension_replace
 * \{ */

#define PATH_EXTENSION_REPLACE_WITH_MAXLEN( \
    input_path, input_ext, expect_result, expect_path, maxlen) \
  { \
    BLI_assert(maxlen <= FILE_MAX); \
    char path[FILE_MAX]; \
    STRNCPY(path, input_path); \
    const bool ret = BLI_path_extension_replace(path, maxlen, input_ext); \
    if (expect_result) { \
      EXPECT_TRUE(ret); \
    } \
    else { \
      EXPECT_FALSE(ret); \
    } \
    EXPECT_STREQ(path, expect_path); \
  } \
  ((void)0)

#define PATH_EXTENSION_REPLACE(input_path, input_ext, expect_result, expect_path) \
  PATH_EXTENSION_REPLACE_WITH_MAXLEN(input_path, input_ext, expect_result, expect_path, FILE_MAX)

TEST(path_util, ExtensionReplace)
{
  PATH_EXTENSION_REPLACE("test", ".txt", true, "test.txt");
  PATH_EXTENSION_REPLACE("test.", ".txt", true, "test.txt");
  /* Unlike #BLI_path_extension_ensure, excess '.' are not stripped. */
  PATH_EXTENSION_REPLACE("test..", ".txt", true, "test..txt");

  PATH_EXTENSION_REPLACE("test.txt", ".txt", true, "test.txt");
  PATH_EXTENSION_REPLACE("test.ext", ".txt", true, "test.txt");

  PATH_EXTENSION_REPLACE("test", "_txt", true, "test_txt");
  PATH_EXTENSION_REPLACE("test.ext", "_txt", true, "test_txt");

  PATH_EXTENSION_REPLACE("test", "", true, "test");

  /* Same as #BLI_path_extension_strip. */
  PATH_EXTENSION_REPLACE("test.txt", "", true, "test");

  /* Empty strings. */
  PATH_EXTENSION_REPLACE("test", "", true, "test");
  PATH_EXTENSION_REPLACE("", "_txt", true, "_txt");
  PATH_EXTENSION_REPLACE("", "", true, "");

  /* Ensure leading '.' isn't treated as an extension. */
  PATH_EXTENSION_REPLACE(".hidden", ".hidden", true, ".hidden.hidden");
  PATH_EXTENSION_REPLACE("..hidden", ".hidden", true, "..hidden.hidden");
  PATH_EXTENSION_REPLACE("._.hidden", ".hidden", true, "._.hidden");
}

TEST(path_util, ExtensionReplace_Overflow)
{
  /* Small values. */
  PATH_EXTENSION_REPLACE_WITH_MAXLEN("test", ".txt", false, "test", 0);
  PATH_EXTENSION_REPLACE_WITH_MAXLEN("test", ".txt", false, "test", 1);
  /* One under fails, and exactly enough space succeeds. */
  PATH_EXTENSION_REPLACE_WITH_MAXLEN("test", ".txt", false, "test", 8);
  PATH_EXTENSION_REPLACE_WITH_MAXLEN("test", ".txt", true, "test.txt", 9);

  PATH_EXTENSION_REPLACE_WITH_MAXLEN("test.xx", ".txt", false, "test.xx", 8);
  PATH_EXTENSION_REPLACE_WITH_MAXLEN("test.xx", ".txt", true, "test.txt", 9);
}

#undef PATH_EXTENSION_REPLACE
#undef PATH_EXTENSION_REPLACE_WITH_MAXLEN

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_extension_ensure
 * \{ */

#define PATH_EXTENSION_ENSURE_WITH_MAXLEN( \
    input_path, input_ext, expect_result, expect_path, maxlen) \
  { \
    BLI_assert(maxlen <= FILE_MAX); \
    char path[FILE_MAX]; \
    STRNCPY(path, input_path); \
    const bool ret = BLI_path_extension_ensure(path, maxlen, input_ext); \
    if (expect_result) { \
      EXPECT_TRUE(ret); \
    } \
    else { \
      EXPECT_FALSE(ret); \
    } \
    EXPECT_STREQ(path, expect_path); \
  } \
  ((void)0)

#define PATH_EXTENSION_ENSURE(input_path, input_ext, expect_result, expect_path) \
  PATH_EXTENSION_ENSURE_WITH_MAXLEN(input_path, input_ext, expect_result, expect_path, FILE_MAX)

TEST(path_util, ExtensionEnsure)
{
  PATH_EXTENSION_ENSURE("test", ".txt", true, "test.txt");
  PATH_EXTENSION_ENSURE("test.", ".txt", true, "test.txt");
  PATH_EXTENSION_ENSURE("test..", ".txt", true, "test.txt");

  PATH_EXTENSION_ENSURE("test.txt", ".txt", true, "test.txt");
  PATH_EXTENSION_ENSURE("test.ext", ".txt", true, "test.ext.txt");

  PATH_EXTENSION_ENSURE("test", "_txt", true, "test_txt");
  PATH_EXTENSION_ENSURE("test.ext", "_txt", true, "test.ext_txt");

  /* An empty string does nothing (unlike replace which strips). */
  PATH_EXTENSION_ENSURE("test.txt", "", true, "test.txt");

  /* Empty strings. */
  PATH_EXTENSION_ENSURE("test", "", true, "test");
  PATH_EXTENSION_ENSURE("", "_txt", true, "_txt");
  PATH_EXTENSION_ENSURE("", "", true, "");

  /* Ensure leading '.' isn't treated as an extension. */
  PATH_EXTENSION_ENSURE(".hidden", ".hidden", true, ".hidden.hidden");
  PATH_EXTENSION_ENSURE("..hidden", ".hidden", true, "..hidden.hidden");
  PATH_EXTENSION_ENSURE("._.hidden", ".hidden", true, "._.hidden");
}

TEST(path_util, ExtensionEnsure_Overflow)
{
  /* Small values. */
  PATH_EXTENSION_ENSURE_WITH_MAXLEN("test", ".txt", false, "test", 0);
  PATH_EXTENSION_ENSURE_WITH_MAXLEN("test", ".txt", false, "test", 1);
  /* One under fails, and exactly enough space succeeds. */
  PATH_EXTENSION_ENSURE_WITH_MAXLEN("test", ".txt", false, "test", 8);
  PATH_EXTENSION_ENSURE_WITH_MAXLEN("test", ".txt", true, "test.txt", 9);
}

#undef PATH_EXTENSION_ENSURE
#undef PATH_EXTENSION_ENSURE_WITH_MAXLEN

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_frame_check_chars
 * \{ */

#define PATH_FRAME_CHECK_CHARS(input_path, expect_hasChars) \
  { \
    const bool ret = BLI_path_frame_check_chars(input_path); \
    if (expect_hasChars) { \
      EXPECT_TRUE(ret); \
    } \
    else { \
      EXPECT_FALSE(ret); \
    } \
  } \
  ((void)0)

TEST(path_util, FrameCheckChars)
{
  PATH_FRAME_CHECK_CHARS("a#", true);
  PATH_FRAME_CHECK_CHARS("aaaaa#", true);
  PATH_FRAME_CHECK_CHARS("#aaaaa", true);
  PATH_FRAME_CHECK_CHARS("a##.###", true);
  PATH_FRAME_CHECK_CHARS("####.abc#", true);
  PATH_FRAME_CHECK_CHARS("path/to/chars/a#", true);
  PATH_FRAME_CHECK_CHARS("path/to/chars/123#123.exe", true);

  PATH_FRAME_CHECK_CHARS("&", false);
  PATH_FRAME_CHECK_CHARS("\35", false);
  PATH_FRAME_CHECK_CHARS("path#/to#/chars#/$.h", false);
  PATH_FRAME_CHECK_CHARS("path#/to#/chars#/nochars.h", false);
  PATH_FRAME_CHECK_CHARS("..\\dirty\\path#/..//to#\\chars#/nochars.h", false);
  PATH_FRAME_CHECK_CHARS("..\\dirty\\path#/..//to#/chars#\\nochars.h", false);
}
#undef PATH_FRAME_CHECK_CHARS

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_frame_range
 * \{ */

#define PATH_FRAME_RANGE(input_path, sta, end, digits, expect_outpath) \
  { \
    char path[FILE_MAX]; \
    bool ret; \
    STRNCPY(path, input_path); \
    ret = BLI_path_frame_range(path, sizeof(path), sta, end, digits); \
    if (expect_outpath == nullptr) { \
      EXPECT_FALSE(ret); \
    } \
    else { \
      EXPECT_TRUE(ret); \
      EXPECT_STREQ(path, expect_outpath); \
    } \
  } \
  ((void)0)

TEST(path_util, FrameRange)
{
  int dummy = -1;
  PATH_FRAME_RANGE("#", 1, 2, dummy, "1-2");
  PATH_FRAME_RANGE("##", 1, 2, dummy, "01-02");
  PATH_FRAME_RANGE("##", 1000, 2000, dummy, "1000-2000");
  PATH_FRAME_RANGE("###", 100, 200, dummy, "100-200");
  PATH_FRAME_RANGE("###", 8, 9, dummy, "008-009");

  PATH_FRAME_RANGE("", 100, 200, 1, "100-200");
  PATH_FRAME_RANGE("", 123, 321, 4, "0123-0321");
  PATH_FRAME_RANGE("", 1, 0, 20, "00000000000000000001-00000000000000000000");
}
#undef PATH_FRAME_RANGE

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_frame_get
 * \{ */

#define PATH_FRAME_GET(input_path, expect_frame, expect_numdigits, expect_pathisvalid) \
  { \
    char path[FILE_MAX]; \
    int out_frame = -1, out_numdigits = -1; \
    STRNCPY(path, input_path); \
    const bool ret = BLI_path_frame_get(path, &out_frame, &out_numdigits); \
    if (expect_pathisvalid) { \
      EXPECT_TRUE(ret); \
    } \
    else { \
      EXPECT_FALSE(ret); \
    } \
    EXPECT_EQ(out_frame, expect_frame); \
    EXPECT_EQ(out_numdigits, expect_numdigits); \
  } \
  ((void)0)

TEST(path_util, FrameGet)
{
  PATH_FRAME_GET("001.avi", 1, 3, true);
  PATH_FRAME_GET("0000299.ext", 299, 7, true);
  PATH_FRAME_GET("path/to/frame_2810.dummy_quite_long_extension", 2810, 4, true);
  PATH_FRAME_GET("notframe_7_frame00018.bla", 18, 5, true);

  PATH_FRAME_GET("", -1, -1, false);
}
#undef PATH_FRAME_GET

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_sequence_decode
 * \{ */

#define PATH_SEQ_DECODE(path_literal, expect_result, expect_head, expect_tail, expect_numdigits) \
  { \
    const char *path = path_literal; \
    char head[FILE_MAX]; \
    char tail[FILE_MAX]; \
    ushort numdigits = 0; \
    const int result = BLI_path_sequence_decode( \
        path, head, sizeof(head), tail, sizeof(tail), &numdigits); \
    EXPECT_EQ(result, expect_result); \
    EXPECT_STREQ(head, expect_head); \
    EXPECT_STREQ(tail, expect_tail); \
    EXPECT_EQ(numdigits, expect_numdigits); \
  } \
  (void)0;

TEST(path_util, SequenceDecode)
{
  /* Basic use. */
  PATH_SEQ_DECODE("file_123.txt", 123, "file_", ".txt", 3);
  PATH_SEQ_DECODE("file_123.321", 123, "file_", ".321", 3);
  PATH_SEQ_DECODE(".file_123.txt", 123, ".file_", ".txt", 3);

  /* No-op. */
  PATH_SEQ_DECODE("file.txt", 0, "file", ".txt", 0);
  PATH_SEQ_DECODE("file.123", 0, "file", ".123", 0);
  PATH_SEQ_DECODE("file", 0, "file", "", 0);
  PATH_SEQ_DECODE("file_123.txt/", 0, "file_123.txt/", "", 0);
}

#undef PATH_SEQ_DECODE

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_suffix
 * \{ */

#define PATH_SUFFIX(path_literal, path_literal_max, sep, suffix, expect_result, expect_path) \
  { \
    char path[FILE_MAX] = path_literal; \
    const bool result = BLI_path_suffix(path, path_literal_max, suffix, sep); \
    EXPECT_EQ(result, expect_result); \
    EXPECT_STREQ(path, expect_path); \
  } \
  (void)0;

TEST(path_util, Suffix)
{
  /* Extension. */
  PATH_SUFFIX("file.txt", FILE_MAX, "_", "123", true, "file_123.txt");
  PATH_SUFFIX("/dir/file.txt", FILE_MAX, "_", "123", true, "/dir/file_123.txt");
  /* No-extension. */
  PATH_SUFFIX("file", FILE_MAX, "_", "123", true, "file_123");
  PATH_SUFFIX("/dir/file", FILE_MAX, "_", "123", true, "/dir/file_123");
  /* No-op. */
  PATH_SUFFIX("file.txt", FILE_MAX, "", "", true, "file.txt");
  /* Size limit, too short by 1. */
  PATH_SUFFIX("file.txt", 10, "A", "B", false, "file.txt");
  /* Size limit, fits exactly. */
  PATH_SUFFIX("file.txt", 11, "A", "B", true, "fileAB.txt");
  /* Empty path. */
  PATH_SUFFIX("", FILE_MAX, "_", "123", true, "_123");
  /* Empty input/output. */
  PATH_SUFFIX("", FILE_MAX, "", "", true, "");

  /* Long suffix. */
  PATH_SUFFIX("file.txt", FILE_MAX, "_", "1234567890", true, "file_1234567890.txt");
  /* Long extension. */
  PATH_SUFFIX("file.txt1234567890", FILE_MAX, "_", "123", true, "file_123.txt1234567890");
}

#undef PATH_SUFFIX

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_rel
 * \{ */

#define PATH_REL(abs_path, ref_path, rel_path_expect) \
  { \
    char path[FILE_MAX]; \
    const char *ref_path_test = ref_path; \
    STRNCPY(path, abs_path); \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '/', '\\'); \
      ref_path_test = str_replace_char_strdup(ref_path_test, '/', '\\'); \
    } \
    BLI_path_rel(path, ref_path_test); \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '\\', '/'); \
      free((void *)ref_path_test); \
    } \
    EXPECT_STREQ(path, rel_path_expect); \
  } \
  void(0)

#ifdef WIN32
#  define ABS_PREFIX "C:"
#else
#  define ABS_PREFIX ""
#endif

TEST(path_util, RelPath_Simple)
{
  PATH_REL(ABS_PREFIX "/foo/bar/blender.blend", ABS_PREFIX "/foo/bar/", "//blender.blend");
}

TEST(path_util, RelPath_SimpleSubdir)
{
  PATH_REL(ABS_PREFIX "/foo/bar/blender.blend", ABS_PREFIX "/foo/bar", "//bar/blender.blend");
}

TEST(path_util, RelPath_BufferOverflowRoot)
{
  char abs_path_in[FILE_MAX];
  const char *abs_prefix = ABS_PREFIX "/";
  for (int i = STRNCPY_RLEN(abs_path_in, abs_prefix); i < FILE_MAX - 1; i++) {
    abs_path_in[i] = 'A';
  }
  abs_path_in[FILE_MAX - 1] = '\0';
  char abs_path_out[FILE_MAX];
  for (int i = STRNCPY_RLEN(abs_path_out, "//"); i < FILE_MAX - 1; i++) {
    abs_path_out[i] = 'A';
  }
  abs_path_out[FILE_MAX - std::max((strlen(abs_prefix) - 1), size_t(1))] = '\0';
  PATH_REL(abs_path_in, abs_prefix, abs_path_out);
}

TEST(path_util, RelPath_BufferOverflowSubdir)
{
  char abs_path_in[FILE_MAX];
  const char *ref_path_in = ABS_PREFIX "/foo/bar/";
  const size_t ref_path_in_len = strlen(ref_path_in);
  for (int i = STRNCPY_RLEN(abs_path_in, ref_path_in); i < FILE_MAX - 1; i++) {
    abs_path_in[i] = 'A';
  }
  abs_path_in[FILE_MAX - 1] = '\0';
  char abs_path_out[FILE_MAX];
  for (int i = STRNCPY_RLEN(abs_path_out, "//"); i < FILE_MAX - (int(ref_path_in_len) - 1); i++) {
    abs_path_out[i] = 'A';
  }
  abs_path_out[FILE_MAX - std::max((ref_path_in_len - 1), size_t(1))] = '\0';
  PATH_REL(abs_path_in, ref_path_in, abs_path_out);
}

#undef PATH_REL
#undef ABS_PREFIX

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_contains
 * \{ */

TEST(path_util, Contains)
{
  EXPECT_TRUE(BLI_path_contains("/some/path", "/some/path")) << "A path contains itself";
  EXPECT_TRUE(BLI_path_contains("/some/path", "/some/path/inside"))
      << "A path contains its subdirectory";
  EXPECT_TRUE(BLI_path_contains("/some/path", "/some/path/../path/inside"))
      << "Paths should be normalized";
  EXPECT_TRUE(BLI_path_contains("C:\\some\\path", "C:\\some\\path\\inside"))
      << "Windows paths should be supported as well";

  EXPECT_FALSE(BLI_path_contains("C:\\some\\path", "C:\\some\\other\\path"))
      << "Windows paths should be supported as well";
  EXPECT_FALSE(BLI_path_contains("/some/path", "/"))
      << "Root directory not be contained in a subdirectory";
  EXPECT_FALSE(BLI_path_contains("/some/path", "/some/path/../outside"))
      << "Paths should be normalized";
  EXPECT_FALSE(BLI_path_contains("/some/path", "/some/path_library"))
      << "Just sharing a suffix is not enough, path semantics should be followed";
  EXPECT_FALSE(BLI_path_contains("/some/path", "./contents"))
      << "Relative paths are not supported";
}

#ifdef WIN32
TEST(path_util, Contains_Windows_case_insensitive)
{
  EXPECT_TRUE(BLI_path_contains("C:\\some\\path", "c:\\SOME\\path\\inside"))
      << "On Windows path comparison should ignore case";
}
#endif /* WIN32 */

/** \} */
