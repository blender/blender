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

typedef struct ImageTestResult {
	char *path;
	char *rel;
	int ret;
} ImageTestResult;

typedef struct ImageTestData {
	char *path; /* image filename */
	ImageTestResult result[10];
} ImageTestData;

/* check that BKE_copy_images manipulates paths correctly */
START_TEST(test_copy_images)
{
	char **dir;
	ImageTestData *test;
	int i,j;

#ifdef WIN32
	/* TBD... */
#else
	/*
	  XXX are these paths possible in image->name?:

	  ./foo/image.png
	  ../foo/image.png

	  if so, BKE_copy_images currently doesn't support them!
	 */

	const char *blend_dir = "/home/user/foo";
	char *dest_dir[] = {"/home/user/", "/home/user", "/home/user/export/", "/home/user/foo/", NULL};

	static ImageTestData test_data[] = {

		/* image path | [expected output path | corresponding relative path | expected return value] */

		/* relative, 0 level deep */
		{"//image.png", {{"/home/user/image.png", "image.png", 1},
						 {"/home/user/image.png", "image.png", 1},
						 {"/home/user/export/image.png", "image.png", 1},
						 {"/home/user/foo/image.png", "image.png", 2},}},

		/* relative, 1 level deep */
		{"//bar/image.png", {{"/home/user/bar/image.png", "bar/image.png", 1},
							 {"/home/user/bar/image.png", "bar/image.png", 1},
							 {"/home/user/export/bar/image.png", "bar/image.png", 1},
							 {"/home/user/foo/bar/image.png", "bar/image.png", 2},}},

		/* relative, 2 level deep */
		{"//bar/foo/image.png", {{"/home/user/bar/foo/image.png", "bar/foo/image.png", 1},
								 {"/home/user/bar/foo/image.png", "bar/foo/image.png", 1},
								 {"/home/user/export/bar/foo/image.png", "bar/foo/image.png", 1},
								 {"/home/user/foo/bar/foo/image.png", "bar/foo/image.png", 2},}},

		/* absolute, not under .blend dir */
		{"/home/user/bar/image.png", {{"/home/user/image.png", "image.png", 1},
									  {"/home/user/image.png", "image.png", 1},
									  {"/home/user/export/image.png", "image.png", 1},
									  {"/home/user/foo/image.png", "image.png", 1},}},

		/* absolute, under .blend dir, 0 level deep */
		{"/home/user/foo/image.png", {{"/home/user/image.png", "image.png", 1},
									  {"/home/user/image.png", "image.png", 1},
									  {"/home/user/export/image.png", "image.png", 1},
									  {"/home/user/foo/image.png", "image.png", 2},}},

		/* absolute, under .blend dir, 1 level deep */
		{"/home/user/foo/bar/image.png", {{"/home/user/bar/image.png", "bar/image.png", 1},
										  {"/home/user/bar/image.png", "bar/image.png", 1},
										  {"/home/user/export/bar/image.png", "bar/image.png", 1},
										  {"/home/user/foo/bar/image.png", "bar/image.png", 2},}},

		/* absolute, under .blend dir, 2 level deep */
		{"/home/user/foo/bar/foo/image.png", {{"/home/user/bar/foo/image.png", "bar/foo/image.png", 1},
											  {"/home/user/bar/foo/image.png", "bar/foo/image.png", 1},
											  {"/home/user/export/bar/foo/image.png", "bar/foo/image.png", 1},
											  {"/home/user/foo/bar/foo/image.png", "bar/foo/image.png", 2},}},

		/* empty image path, don't let these pass! */
		{"", {{"", 0},
			  {"", 0},
			  {"", 0}}},

		{NULL},
	};
	
	/* substitute G.sce */
	BLI_snprintf(G.sce, sizeof(G.sce), "%s/untitled.blend", blend_dir);
#endif

	for (dir= dest_dir, i= 0; *dir; dir++, i++) {
		for (test= &test_data[0]; test->path; test++) {
			Image image;
			char path[FILE_MAX];
			char rel[FILE_MAX];
			char part[200];
			int ret;

			BLI_strncpy(image.name, test->path, sizeof(image.name));

			/* passing NULL as abs path or rel path or both shouldn't break it */
			int abs_rel_null[][2]= {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {-1}};

			for (j= 0; abs_rel_null[j][0] != -1; j++) {

				int *is_null= abs_rel_null[j];

				ret= BKE_get_image_export_path(&image, *dir,
											   is_null[0] ? NULL : path, sizeof(path),
											   is_null[1] ? NULL : rel, sizeof(rel));

				BLI_snprintf(part, sizeof(part), "For image at %s (output abs path is %s, rel path is %s)",
							 test->path, is_null[0] ? "NULL" : "non-NULL", is_null[1] ? "NULL" : "non-NULL");

				/* we should get what we expect */
				ImageTestResult *res= &test->result[i];
				fail_if(ret != res->ret, "%s, expected to return %d got %d.", part, res->ret, ret);

				if (!is_null[0] && res->path)
					fail_if(strcmp(path, res->path), "%s, expected absolute path \"%s\" got \"%s\".", part, res->path, path);
				if (!is_null[1] && res->rel)
					fail_if(strcmp(rel, res->rel), "%s, expected relative path \"%s\" got \"%s\".", part, res->rel, rel);
			}
		}
	}
}
END_TEST

static Suite *image_suite(void)
{
	Suite *s= suite_create("Image");

	/* Core test case */
	TCase *tc_core= tcase_create("Core");
	tcase_add_test(tc_core, test_copy_images);
	suite_add_tcase(s, tc_core);

	return s;
}

int run_tests()
{
	int totfail;
	Suite *s= image_suite();
	SRunner *sr= srunner_create(s);

	/* run tests */
	srunner_run_all(sr, CK_VERBOSE);

	totfail= srunner_ntests_failed(sr);
	srunner_free(sr);

	return !totfail ? EXIT_SUCCESS : EXIT_FAILURE;
}
