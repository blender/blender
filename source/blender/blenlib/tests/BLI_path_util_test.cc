/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "IMB_imbuf.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

/* -------------------------------------------------------------------- */
/* tests */

/* BLI_path_normalize */
#ifndef _WIN32
TEST(path_util, Clean)
{
  /* "/./" -> "/" */
  {
    char path[FILE_MAX] = "/a/./b/./c/./";
    BLI_path_normalize(nullptr, path);
    EXPECT_STREQ("/a/b/c/", path);
  }

  {
    char path[FILE_MAX] = "/./././";
    BLI_path_normalize(nullptr, path);
    EXPECT_STREQ("/", path);
  }

  {
    char path[FILE_MAX] = "/a/./././b/";
    BLI_path_normalize(nullptr, path);
    EXPECT_STREQ("/a/b/", path);
  }

  /* "//" -> "/" */
  {
    char path[FILE_MAX] = "a////";
    BLI_path_normalize(nullptr, path);
    EXPECT_STREQ("a/", path);
  }

  if (false) /* FIXME */
  {
    char path[FILE_MAX] = "./a////";
    BLI_path_normalize(nullptr, path);
    EXPECT_STREQ("./a/", path);
  }

  /* "foo/bar/../" -> "foo/" */
  {
    char path[FILE_MAX] = "/a/b/c/../../../";
    BLI_path_normalize(nullptr, path);
    EXPECT_STREQ("/", path);
  }

  {
    char path[FILE_MAX] = "/a/../a/b/../b/c/../c/";
    BLI_path_normalize(nullptr, path);
    EXPECT_STREQ("/a/b/c/", path);
  }

  {
    char path[FILE_MAX] = "//../";
    BLI_path_normalize("/a/b/c/", path);
    EXPECT_STREQ("/a/b/", path);
  }
}
#endif

#define AT_INDEX(str_input, index_input, str_expect) \
  { \
    char path[] = str_input; \
    const char *expect = str_expect; \
    int index_output, len_output; \
    const bool ret = BLI_path_name_at_index(path, index_input, &index_output, &len_output); \
    if (expect == NULL) { \
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

/* BLI_path_name_at_index */
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

TEST(path_util, NameAtIndex_MiscComplex)
{
  AT_INDEX("how//now/brown/cow", 0, "how");
  AT_INDEX("//how///now\\/brown/cow", 1, "now");
  AT_INDEX("/how/now\\//brown\\/cow", 2, "brown");
  AT_INDEX("/how/now/brown/cow//\\", 3, "cow");
  AT_INDEX("/how/now/brown/\\cow", 4, nullptr);
  AT_INDEX("how/now/brown/\\cow\\", 4, nullptr);
}

TEST(path_util, NameAtIndex_MiscComplexNeg)
{
  AT_INDEX("how//now/brown/cow", -4, "how");
  AT_INDEX("//how///now\\/brown/cow", -3, "now");
  AT_INDEX("/how/now\\//brown\\/cow", -2, "brown");
  AT_INDEX("/how/now/brown/cow//\\", -1, "cow");
  AT_INDEX("/how/now/brown/\\cow", -5, nullptr);
  AT_INDEX("how/now/brown/\\cow\\", -5, nullptr);
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

#define JOIN(str_expect, out_size, ...) \
  { \
    const char *expect = str_expect; \
    char result[(out_size) + 1024]; \
    /* check we don't write past the last byte */ \
    result[out_size] = '\0'; \
    BLI_path_join(result, out_size, __VA_ARGS__, NULL); \
    /* simplify expected string */ \
    BLI_str_replace_char(result, '\\', '/'); \
    EXPECT_STREQ(result, expect); \
    EXPECT_EQ(result[out_size], '\0'); \
  } \
  ((void)0)

/* BLI_path_join */
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
  JOIN("/a/b/c/d/e/f/g/", 100, "/", "\\a/b", "//////c/d", "", "e\\\\", "f", "g//");
  JOIN("/aa/bb/cc/dd/ee/ff/gg/", 100, "/", "\\aa/bb", "//////cc/dd", "", "ee\\\\", "ff", "gg//");
  JOIN("1/2/3/", 100, "1", "////////", "", "2", "3\\");
}

#undef JOIN

/* BLI_path_frame */
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

/* BLI_split_dirfile */
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

#define PATH_FRAME_STRIP(input_path, expect_path, expect_ext) \
  { \
    char path[FILE_MAX]; \
    char ext[FILE_MAX]; \
    BLI_strncpy(path, (input_path), FILE_MAX); \
    BLI_path_frame_strip(path, ext, sizeof(ext)); \
    EXPECT_STREQ(path, expect_path); \
    EXPECT_STREQ(ext, expect_ext); \
  } \
  ((void)0)

/* BLI_path_frame_strip */
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

#define PATH_EXTENSION_CHECK(input_path, input_ext, expect_ext) \
  { \
    const bool ret = BLI_path_extension_check(input_path, input_ext); \
    if (strcmp(input_ext, expect_ext) == 0) { \
      EXPECT_TRUE(ret); \
    } \
    else { \
      EXPECT_FALSE(ret); \
    } \
  } \
  ((void)0)

/* BLI_path_extension_check */
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

/* BLI_path_frame_check_chars */
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

#define PATH_FRAME_RANGE(input_path, sta, end, digits, expect_outpath) \
  { \
    char path[FILE_MAX]; \
    bool ret; \
    BLI_strncpy(path, input_path, FILE_MAX); \
    ret = BLI_path_frame_range(path, sta, end, digits); \
    if (expect_outpath == NULL) { \
      EXPECT_FALSE(ret); \
    } \
    else { \
      EXPECT_TRUE(ret); \
      EXPECT_STREQ(path, expect_outpath); \
    } \
  } \
  ((void)0)

/* BLI_path_frame_range */
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

/* BLI_path_frame_get */
TEST(path_util, PathFrameGet)
{
  PATH_FRAME_GET("001.avi", 1, 3, true);
  PATH_FRAME_GET("0000299.ext", 299, 7, true);
  PATH_FRAME_GET("path/to/frame_2810.dummy_quite_long_extension", 2810, 4, true);
  PATH_FRAME_GET("notframe_7_frame00018.bla", 18, 5, true);

  PATH_FRAME_GET("", -1, -1, false);
}
#undef PATH_FRAME_GET

/* BLI_path_extension */
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

/* BLI_path_rel. */
#ifndef _WIN32

#  define PATH_REL(abs_path, ref_path, rel_path) \
    { \
      char path[FILE_MAX]; \
      BLI_strncpy(path, abs_path, sizeof(path)); \
      BLI_path_rel(path, ref_path); \
      EXPECT_STREQ(rel_path, path); \
    } \
    void(0)

TEST(path_util, PathRelPath)
{
  PATH_REL("/foo/bar/blender.blend", "/foo/bar/", "//blender.blend");
  PATH_REL("/foo/bar/blender.blend", "/foo/bar", "//bar/blender.blend");

  /* Check for potential buffer overflows. */
  {
    char abs_path_in[FILE_MAX];
    abs_path_in[0] = '/';
    for (int i = 1; i < FILE_MAX - 1; i++) {
      abs_path_in[i] = 'A';
    }
    abs_path_in[FILE_MAX - 1] = '\0';
    char abs_path_out[FILE_MAX];
    abs_path_out[0] = '/';
    abs_path_out[1] = '/';
    for (int i = 2; i < FILE_MAX - 1; i++) {
      abs_path_out[i] = 'A';
    }
    abs_path_out[FILE_MAX - 1] = '\0';
    PATH_REL(abs_path_in, "/", abs_path_out);

    const char *ref_path_in = "/foo/bar/";
    const size_t ref_path_in_len = strlen(ref_path_in);
    strcpy(abs_path_in, ref_path_in);
    for (int i = ref_path_in_len; i < FILE_MAX - 1; i++) {
      abs_path_in[i] = 'A';
    }
    abs_path_in[FILE_MAX - 1] = '\0';
    abs_path_out[0] = '/';
    abs_path_out[1] = '/';
    for (int i = 2; i < FILE_MAX - ((int)ref_path_in_len - 1); i++) {
      abs_path_out[i] = 'A';
    }
    abs_path_out[FILE_MAX - (ref_path_in_len - 1)] = '\0';
    PATH_REL(abs_path_in, ref_path_in, abs_path_out);
  }
}

#  undef PATH_REL

#endif

/* BLI_path_contains */
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
#endif
