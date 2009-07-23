#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "MEM_guardedalloc.h"

#include "BKE_blender.h"
#include "BKE_image.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BLI_listbase.h"

#include "DNA_image_types.h"

char bprogname[FILE_MAXDIR+FILE_MAXFILE];
char btempdir[FILE_MAXDIR+FILE_MAXFILE];

typedef struct ImageTestData {
	char *path; /* image filename */
	char *expect_path; /* file path that we expect */
	int type; /* image type */
	int ret;  /* expected function return value */
} ImageTestData;

/* check that BKE_copy_images manipulates paths correctly */
START_TEST(test_copy_images)
{
	char *dest_dir[] = {"/tmp/", "/tmp", NULL};
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
		{"//bar/image.png", "/tmp/bar/image.png", IMA_TYPE_IMAGE, 1},
		{"/foo/bar/image.png", "/tmp/image.png", IMA_TYPE_IMAGE, 1},
		{"//image.png", "/tmp/image.png", IMA_TYPE_IMAGE, 1},
		{"//../../../foo/bar/image.png", "/tmp/image.png", IMA_TYPE_IMAGE, 1},
		{"//./foo/bar/image.png", "/tmp/foo/bar/image.png", IMA_TYPE_IMAGE, 1},
		{"/tmp/image.png", "/tmp/image.png", IMA_TYPE_IMAGE, 1},
		{"//textures/test/foo/bar/image.png", "/tmp/textures/test/foo/bar/image.png", IMA_TYPE_IMAGE, 1},
		{"//textures/test/foo/bar/image.png", "", IMA_TYPE_MULTILAYER, 0},
		{"", "", IMA_TYPE_IMAGE, 0},
		{NULL, NULL},
	};

	/* substitute G.sce */
	BLI_strncpy(G.sce, "/tmp/foo/bar/untitled.blend", sizeof(G.sce));
#endif

	for (dir = dest_dir; *dir; dir++) {
		for (test= &test_data[0]; test->path; test++) {
			Image image;
			char path[FILE_MAX];
			int ret;

			BLI_strncpy(image.name, test->path, sizeof(image.name));
			image.type= test->type;

			ret= BKE_export_image(&image, *dest_dir, path, sizeof(path));

			/* check if we got correct output */
			fail_if(ret != test->ret, "For image with filename %s and type %d, expected %d as return value got %d.",
					test->path, test->type, test->ret, ret);
			fail_if(strcmp(path, test->expect_path), "For image with filename %s and type %d, expected path '%s' got '%s'.",
					test->path, test->type, test->expect_path, path);
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
