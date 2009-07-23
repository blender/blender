#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "MEM_guardedalloc.h"

#include "BKE_blender.h"
#include "BKE_image.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BLI_listbase.h"
#include "BLI_util.h"
#include "BLI_fileops.h"
#include "BLI_string.h"

#include "DNA_image_types.h"

char bprogname[FILE_MAXDIR+FILE_MAXFILE];
char btempdir[FILE_MAXDIR+FILE_MAXFILE];

typedef struct ImageTestData {
	char *path; /* image filename */
	char *expect_path; /* file path that we expect */
	int type; /* image type */
	int ret;  /* expected function return value */
	int create_file; /* whether the file should be created */
} ImageTestData;

/* recursively deletes a directory only if it is under /tmp */
static void delete_only_tmp(char *path, int dir) {
#ifdef WIN32
#else
	if (!strncmp(path, "/tmp/", 5) && BLI_exists(path)) {
		BLI_delete(path, dir, 1);
	}
#endif
}

static void touch_only_tmp(char *path) {
#ifdef WIN32
#else
	if (!strncmp(path, "/tmp/", 5)) {
		BLI_touch(path);
	}
#endif
}

/* check that BKE_copy_images manipulates paths correctly */
START_TEST(test_copy_images)
{
	char **dir;
	ImageTestData *test;

	/* XXX Windows not tested */	
#ifdef WIN32
	static ImageTestData test_data[] = {
		{"//bar/image.png", "C:\\Temp\\bar\\image.png"},
		/* TODO add more */
		{NULL, NULL},
	};
	
	BLI_strncpy(G.sce, "C:\\Temp\untitled.blend", sizeof(G.sce));
#else
	/*
	  XXX are these paths possible in image->name?:

	  ./foo/image.png
	  ../foo/image.png

	  if so, BKE_copy_images currently doesn't support them!
	 */
	static ImageTestData test_data[] = {
		{"//bar/image.png", "/tmp/blender/dest/bar/image.png", IMA_TYPE_IMAGE, 1, 1},
		{"//image.png", "/tmp/blender/dest/image.png", IMA_TYPE_IMAGE, 1, 1},
		{"//textures/test/foo/bar/image.png", "/tmp/blender/dest/textures/test/foo/bar/image.png", IMA_TYPE_IMAGE, 1, 1},
		{"//textures/test/foo/bar/image.png", "", IMA_TYPE_MULTILAYER, 0, 1},
		{"//./foo/bar/image.png", "/tmp/blender/dest/foo/bar/image.png", IMA_TYPE_IMAGE, 1, 1},
		{"//../foo/bar/image.png", "/tmp/blender/dest/image.png", IMA_TYPE_IMAGE, 1, 1},
		{"/tmp/blender/image.png", "/tmp/blender/dest/image.png", IMA_TYPE_IMAGE, 1, 1},
		/* expecting it to return 1 when src and dest are the same file */
		{"/tmp/blender/foo/bar/image.png", "/tmp/blender/dest/image.png", IMA_TYPE_IMAGE, 1, 1},
		{"/tmp/blender/dest/image.png", "/tmp/blender/dest/image.png", IMA_TYPE_IMAGE, 1, 1},
		/* expecting empty path and 0 return value for non-existing files */
		{"/tmp/blender/src/file-not-created", "", IMA_TYPE_IMAGE, 0, 0},
		{"", "", IMA_TYPE_IMAGE, 0, 0},
		{NULL, NULL},
	};

	char *dest_dir[] = {"/tmp/blender/dest/", "/tmp/blender/dest", NULL};
	const char *blend_dir = "/tmp/blender/src";
	
	/* substitute G.sce */
	BLI_snprintf(G.sce, sizeof(G.sce), "%s/untitled.blend", blend_dir);
#endif

	/* only delete files/directories under /tmp/ ! */
	delete_only_tmp(blend_dir, 1);

	for (dir = dest_dir; *dir; dir++) {
		delete_only_tmp(*dir, 1);
	}

	/* create files */
	BLI_recurdir_fileops(blend_dir);

	/* create fake empty source files */
	for (test= &test_data[0]; test->path; test++) {
		char dir[FILE_MAX];
		char path[FILE_MAX];

		if (!test->create_file) continue;

		/* expand "//" */
		BLI_strncpy(path, test->path, sizeof(path));
		BLI_convertstringcode(path, G.sce);

		/* create a directory */
		BLI_split_dirfile_basic(path, dir, NULL);
		BLI_recurdir_fileops(dir);

		/* create a file */
		touch_only_tmp(path);
	}

	for (dir = dest_dir; *dir; dir++) {
		for (test= &test_data[0]; test->path; test++) {
			Image image;
			char path[FILE_MAX];
			char part[200];
			int ret;

			BLI_strncpy(image.name, test->path, sizeof(image.name));
			image.type= test->type;

			ret= BKE_export_image(&image, *dir, path, sizeof(path));

			/* check if we got correct output */
			BLI_snprintf(part, sizeof(part), "For image with filename %s and type %d", test->path, test->type);
			fail_if(ret != test->ret, "%s, expected %d as return value got %d.", part, test->ret, ret);
			fail_if(strcmp(path, test->expect_path), "%s, expected path %s got \"%s\".", part, test->expect_path, path);
			if (test->ret == ret && ret == 1) {
				fail_if(!BLI_exists(test->expect_path), "%s, expected %s to be created.", part, test->expect_path);
			}
		}
	}
}
END_TEST

static Suite *image_suite(void)
{
	Suite *s = suite_create("Image");

	/* Core test case */
	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, test_copy_images);
	suite_add_tcase(s, tc_core);

	return s;
}

int run_tests()
{
	int totfail;
	Suite *s = image_suite();
	SRunner *sr = srunner_create(s);

	/* run tests */
	srunner_run_all(sr, CK_VERBOSE);

	totfail= srunner_ntests_failed(sr);
	srunner_free(sr);

	return !totfail ? EXIT_SUCCESS : EXIT_FAILURE;
}
