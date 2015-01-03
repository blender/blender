/* Apache License, Version 2.0 */

#include "testing/testing.h"

extern "C" {
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "../../../source/blender/imbuf/IMB_imbuf.h"
}

/* -------------------------------------------------------------------- */
/* stubs */

extern "C" {

const char *GHOST_getUserDir(int version, const char *versionstr);
const char *GHOST_getSystemDir(int version, const char *versionstr);
#ifdef __linux__
char *zLhm65070058860608_br_find_exe(const char *default_exe);
#endif

const char *GHOST_getUserDir(int version, const char *versionstr)
{
	return "/home/user";
}

const char *GHOST_getSystemDir(int version, const char *versionstr)
{
	return "/system/path";
}

struct ImBuf;
void IMB_freeImBuf(struct ImBuf *ibuf) {}
struct ImBuf *IMB_dupImBuf(struct ImBuf *ibuf) {return NULL;}

#ifdef __linux__
char *zLhm65070058860608_br_find_exe(const char *default_exe)
{
	return NULL;
}
#endif

}


/* -------------------------------------------------------------------- */
/* tests */

/* BLI_cleanup_path */
TEST(path_util, PathUtilClean)
{
	/* "/./" -> "/" */
	{
		char path[FILE_MAX] = "/a/./b/./c/./";
		BLI_cleanup_path(NULL, path);
		EXPECT_STREQ("/a/b/c/", path);
	}

	{
		char path[FILE_MAX] = "/./././";
		BLI_cleanup_path(NULL, path);
		EXPECT_STREQ("/", path);
	}

	{
		char path[FILE_MAX] = "/a/./././b/";
		BLI_cleanup_path(NULL, path);
		EXPECT_STREQ("/a/b/", path);
	}

	/* "//" -> "/" */
	{
		char path[FILE_MAX] = "a////";
		BLI_cleanup_path(NULL, path);
		EXPECT_STREQ("a/", path);
	}

	if (0)  /* FIXME */
	{
		char path[FILE_MAX] = "./a////";
		BLI_cleanup_path(NULL, path);
		EXPECT_STREQ("./a/", path);
	}

	/* "foo/bar/../" -> "foo/" */
	{
		char path[FILE_MAX] = "/a/b/c/../../../";
		BLI_cleanup_path(NULL, path);
		EXPECT_STREQ("/", path);
	}

	{
		char path[FILE_MAX] = "/a/../a/b/../b/c/../c/";
		BLI_cleanup_path(NULL, path);
		EXPECT_STREQ("/a/b/c/", path);
	}

	{
		char path[FILE_MAX] = "//../";
		BLI_cleanup_path("/a/b/c/", path);
		EXPECT_STREQ("/a/b/", path);
	}
}

/* BLI_path_frame */
TEST(path_util, PathUtilFrame)
{
	bool ret;

	{
		char path[FILE_MAX] = "";
		ret = BLI_path_frame(path, 123, 1);
		EXPECT_EQ(1, ret);
		EXPECT_STREQ("123", path);
	}

	{
		char path[FILE_MAX] = "";
		ret = BLI_path_frame(path, 123, 12);
		EXPECT_EQ(1, ret);
		EXPECT_STREQ("000000000123", path);
	}

	{
		char path[FILE_MAX] = "test_";
		ret = BLI_path_frame(path, 123, 1);
		EXPECT_EQ(1, ret);
		EXPECT_STREQ("test_123", path);
	}

	{
		char path[FILE_MAX] = "test_";
		ret = BLI_path_frame(path, 1, 12);
		EXPECT_EQ(1, ret);
		EXPECT_STREQ("test_000000000001", path);
	}

	{
		char path[FILE_MAX] = "test_############";
		ret = BLI_path_frame(path, 1, 0);
		EXPECT_EQ(1, ret);
		EXPECT_STREQ("test_000000000001", path);
	}

	{
		char path[FILE_MAX] = "test_#_#_middle";
		ret = BLI_path_frame(path, 123, 0);
		EXPECT_EQ(1, ret);
		EXPECT_STREQ("test_#_123_middle", path);
	}

	/* intentionally fail */
	{
		char path[FILE_MAX] = "";
		ret = BLI_path_frame(path, 123, 0);
		EXPECT_EQ(0, ret);
		EXPECT_STREQ("", path);
	}

	{
		char path[FILE_MAX] = "test_middle";
		ret = BLI_path_frame(path, 123, 0);
		EXPECT_EQ(0, ret);
		EXPECT_STREQ("test_middle", path);
	}
}

/* BLI_split_dirfile */
TEST(path_util, PathUtilSplitDirfile)
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
