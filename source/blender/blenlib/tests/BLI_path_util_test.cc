/* SPDX-License-Identifier: Apache-2.0 */

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

#define NORMALIZE_WITH_BASEDIR(input, input_base, output) \
  { \
    char path[FILE_MAX] = input; \
    const char *input_base_test = input_base; \
    if (SEP == '\\') { \
      str_replace_char_with_relative_exception(path, '/', '\\'); \
      input_base_test = str_replace_char_strdup(input_base_test, '/', '\\'); \
    } \
    BLI_path_normalize(input_base_test, path); \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '\\', '/'); \
      if (input_base_test) { \
        free((void *)input_base_test); \
      } \
    } \
    EXPECT_STREQ(output, path); \
  } \
  ((void)0)

#define NORMALIZE(input, output) NORMALIZE_WITH_BASEDIR(input, nullptr, output)

/* #BLI_path_normalize: "/./" -> "/" */
TEST(path_util, Clean_Dot)
{
  NORMALIZE("/./", "/");
  NORMALIZE("/a/./b/./c/./", "/a/b/c/");
  NORMALIZE("/./././", "/");
  NORMALIZE("/a/./././b/", "/a/b/");
}
/* #BLI_path_normalize: complex "/./" -> "/", "//" -> "/", "./path/../" -> "./". */
TEST(path_util, Clean_Complex)
{
  NORMALIZE("/a/./b/./c/./.././.././", "/a/");
  NORMALIZE("/a//.//b//.//c//.//..//.//..//.//", "/a/");
}
/* #BLI_path_normalize: "//" -> "/" */
TEST(path_util, Clean_DoubleSlash)
{
  NORMALIZE("//", "//"); /* Exception, double forward slash. */
  NORMALIZE(".//", "./");
  NORMALIZE("a////", "a/");
  NORMALIZE("./a////", "./a/");
}
/* #BLI_path_normalize: "foo/bar/../" -> "foo/" */
TEST(path_util, Clean_Parent)
{
  NORMALIZE("/a/b/c/../../../", "/");
  NORMALIZE("/a/../a/b/../b/c/../c/", "/a/b/c/");
  NORMALIZE_WITH_BASEDIR("//../", "/a/b/c/", "/a/b/");
}

#undef NORMALIZE_WITH_BASEDIR
#undef NORMALIZE

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_parent_dir
 * \{ */

#define PARENT_DIR(input, output) \
  { \
    char path[FILE_MAX] = input; \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '/', '\\'); \
    } \
    BLI_path_parent_dir(path); \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '\\', '/'); \
    } \
    EXPECT_STREQ(output, path); \
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
      EXPECT_EQ(strlen(expect), len_output); \
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
/** \name Tests for: #BLI_path_frame
 * \{ */

TEST(path_util, Frame)
{
  bool ret;

  {
    char path[FILE_MAX] = "";
    ret = BLI_path_frame(path, 123, 1);
    EXPECT_TRUE(ret);
    EXPECT_STREQ("123", path);
  }

  {
    char path[FILE_MAX] = "";
    ret = BLI_path_frame(path, 123, 12);
    EXPECT_TRUE(ret);
    EXPECT_STREQ("000000000123", path);
  }

  {
    char path[FILE_MAX] = "test_";
    ret = BLI_path_frame(path, 123, 1);
    EXPECT_TRUE(ret);
    EXPECT_STREQ("test_123", path);
  }

  {
    char path[FILE_MAX] = "test_";
    ret = BLI_path_frame(path, 1, 12);
    EXPECT_TRUE(ret);
    EXPECT_STREQ("test_000000000001", path);
  }

  {
    char path[FILE_MAX] = "test_############";
    ret = BLI_path_frame(path, 1, 0);
    EXPECT_TRUE(ret);
    EXPECT_STREQ("test_000000000001", path);
  }

  {
    char path[FILE_MAX] = "test_#_#_middle";
    ret = BLI_path_frame(path, 123, 0);
    EXPECT_TRUE(ret);
    EXPECT_STREQ("test_#_123_middle", path);
  }

  /* intentionally fail */
  {
    char path[FILE_MAX] = "";
    ret = BLI_path_frame(path, 123, 0);
    EXPECT_FALSE(ret);
    EXPECT_STREQ("", path);
  }

  {
    char path[FILE_MAX] = "test_middle";
    ret = BLI_path_frame(path, 123, 0);
    EXPECT_FALSE(ret);
    EXPECT_STREQ("test_middle", path);
  }

  /* negative frame numbers */
  {
    char path[FILE_MAX] = "test_####";
    ret = BLI_path_frame(path, -1, 4);
    EXPECT_TRUE(ret);
    EXPECT_STREQ("test_-0001", path);
  }
  {
    char path[FILE_MAX] = "test_####";
    ret = BLI_path_frame(path, -100, 4);
    EXPECT_TRUE(ret);
    EXPECT_STREQ("test_-0100", path);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_split_dirfile
 * \{ */

TEST(path_util, SplitDirfile)
{
  {
    const char *path = "";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_split_dirfile(path, dir, file, sizeof(dir), sizeof(file));
    EXPECT_STREQ("", dir);
    EXPECT_STREQ("", file);
  }

  {
    const char *path = "/";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_split_dirfile(path, dir, file, sizeof(dir), sizeof(file));
    EXPECT_STREQ("/", dir);
    EXPECT_STREQ("", file);
  }

  {
    const char *path = "fileonly";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_split_dirfile(path, dir, file, sizeof(dir), sizeof(file));
    EXPECT_STREQ("", dir);
    EXPECT_STREQ("fileonly", file);
  }

  {
    const char *path = "dironly/";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_split_dirfile(path, dir, file, sizeof(dir), sizeof(file));
    EXPECT_STREQ("dironly/", dir);
    EXPECT_STREQ("", file);
  }

  {
    const char *path = "/a/b";
    char dir[FILE_MAX], file[FILE_MAX];
    BLI_split_dirfile(path, dir, file, sizeof(dir), sizeof(file));
    EXPECT_STREQ("/a/", dir);
    EXPECT_STREQ("b", file);
  }

  {
    const char *path = "/dirtoobig/filetoobig";
    char dir[5], file[5];
    BLI_split_dirfile(path, dir, file, sizeof(dir), sizeof(file));
    EXPECT_STREQ("/dir", dir);
    EXPECT_STREQ("file", file);

    BLI_split_dirfile(path, dir, file, 1, 1);
    EXPECT_STREQ("", dir);
    EXPECT_STREQ("", file);
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
    BLI_strncpy(path, (input_path), FILE_MAX); \
    BLI_path_frame_strip(path, ext); \
    EXPECT_STREQ(path, expect_path); \
    EXPECT_STREQ(ext, expect_ext); \
  } \
  ((void)0)

TEST(path_util, PathFrameStrip)
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

TEST(path_util, PathExtensionCheck)
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

TEST(path_util, PathFrameCheckChars)
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
    BLI_strncpy(path, input_path, FILE_MAX); \
    ret = BLI_path_frame_range(path, sta, end, digits); \
    if (expect_outpath == nullptr) { \
      EXPECT_FALSE(ret); \
    } \
    else { \
      EXPECT_TRUE(ret); \
      EXPECT_STREQ(path, expect_outpath); \
    } \
  } \
  ((void)0)

TEST(path_util, PathFrameRange)
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
    BLI_strncpy(path, input_path, FILE_MAX); \
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

TEST(path_util, PathFrameGet)
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
/** \name Tests for: #BLI_path_extension
 * \{ */

TEST(path_util, PathExtension)
{
  EXPECT_EQ(nullptr, BLI_path_extension("some.def/file"));
  EXPECT_EQ(nullptr, BLI_path_extension("Text"));
  EXPECT_EQ(nullptr, BLI_path_extension("Text…001"));

  EXPECT_STREQ(".", BLI_path_extension("some/file."));
  EXPECT_STREQ(".gz", BLI_path_extension("some/file.tar.gz"));
  EXPECT_STREQ(".abc", BLI_path_extension("some.def/file.abc"));
  EXPECT_STREQ(".abc", BLI_path_extension("C:\\some.def\\file.abc"));
  EXPECT_STREQ(".001", BLI_path_extension("Text.001"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for: #BLI_path_rel
 * \{ */

#define PATH_REL(abs_path, ref_path, rel_path) \
  { \
    char path[FILE_MAX]; \
    const char *ref_path_test = ref_path; \
    BLI_strncpy(path, abs_path, sizeof(path)); \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '/', '\\'); \
      ref_path_test = str_replace_char_strdup(ref_path_test, '/', '\\'); \
    } \
    BLI_path_rel(path, ref_path_test); \
    if (SEP == '\\') { \
      BLI_str_replace_char(path, '\\', '/'); \
      free((void *)ref_path_test); \
    } \
    EXPECT_STREQ(rel_path, path); \
  } \
  void(0)

#ifdef WIN32
#  define ABS_PREFIX "C:"
#else
#  define ABS_PREFIX ""
#endif

TEST(path_util, PathRelPath_Simple)
{
  PATH_REL(ABS_PREFIX "/foo/bar/blender.blend", ABS_PREFIX "/foo/bar/", "//blender.blend");
}

TEST(path_util, PathRelPath_SimpleSubdir)
{
  PATH_REL(ABS_PREFIX "/foo/bar/blender.blend", ABS_PREFIX "/foo/bar", "//bar/blender.blend");
}

TEST(path_util, PathRelPath_BufferOverflowRoot)
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

TEST(path_util, PathRelPath_BufferOverflowSubdir)
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

TEST(path_util, PathContains)
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
TEST(path_util, PathContains_Windows_case_insensitive)
{
  EXPECT_TRUE(BLI_path_contains("C:\\some\\path", "c:\\SOME\\path\\inside"))
      << "On Windows path comparison should ignore case";
}
#endif /* WIN32 */

/** \} */
