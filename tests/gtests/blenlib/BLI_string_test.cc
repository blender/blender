/* Apache License, Version 2.0 */

#include "testing/testing.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
}

/* -------------------------------------------------------------------- */
/* stubs */

extern "C" {

int mk_wcwidth(wchar_t ucs);
int mk_wcswidth(const wchar_t *pwcs, size_t n);

int mk_wcwidth(wchar_t ucs)
{
	return 0;
}

int mk_wcswidth(const wchar_t *pwcs, size_t n)
{
	return 0;
}

}


/* -------------------------------------------------------------------- */
/* tests */

/* BLI_str_partition */
TEST(string, StrPartition)
{
	const char delim[] = {'-', '.', '_', '~', '\\', '\0'};
	char *sep, *suf;
	size_t pre_ln;

	{
		const char *str = "mat.e-r_ial";

		/* "mat.e-r_ial" -> "mat", '.', "e-r_ial", 3 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(3, pre_ln);
		EXPECT_EQ(&str[3], sep);
		EXPECT_STREQ("e-r_ial", suf);
	}

	/* Corner cases. */
	{
		const char *str = ".mate-rial--";

		/* ".mate-rial--" -> "", '.', "mate-rial--", 0 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(0, pre_ln);
		EXPECT_EQ(&str[0], sep);
		EXPECT_STREQ("mate-rial--", suf);
	}

	{
		const char *str = ".__.--_";

		/* ".__.--_" -> "", '.', "__.--_", 0 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(0, pre_ln);
		EXPECT_EQ(&str[0], sep);
		EXPECT_STREQ("__.--_", suf);
	}

	{
		const char *str = "";

		/* "" -> "", NULL, NULL, 0 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(0, pre_ln);
		EXPECT_EQ(NULL, sep);
		EXPECT_EQ(NULL, suf);
	}

	{
		const char *str = "material";

		/* "material" -> "material", NULL, NULL, 8 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(8, pre_ln);
		EXPECT_EQ(NULL, sep);
		EXPECT_EQ(NULL, suf);
	}
}

/* BLI_str_rpartition */
TEST(string, StrRPartition)
{
	const char delim[] = {'-', '.', '_', '~', '\\', '\0'};
	char *sep, *suf;
	size_t pre_ln;

	{
		const char *str = "mat.e-r_ial";

		/* "mat.e-r_ial" -> "mat.e-r", '_', "ial", 7 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(7, pre_ln);
		EXPECT_EQ(&str[7], sep);
		EXPECT_STREQ("ial", suf);
	}

	/* Corner cases. */
	{
		const char *str = ".mate-rial--";

		/* ".mate-rial--" -> ".mate-rial-", '-', "", 11 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(11, pre_ln);
		EXPECT_EQ(&str[11], sep);
		EXPECT_STREQ("", suf);
	}

	{
		const char *str = ".__.--_";

		/* ".__.--_" -> ".__.--", '_', "", 6 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(6, pre_ln);
		EXPECT_EQ(&str[6], sep);
		EXPECT_STREQ("", suf);
	}

	{
		const char *str = "";

		/* "" -> "", NULL, NULL, 0 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(0, pre_ln);
		EXPECT_EQ(NULL, sep);
		EXPECT_EQ(NULL, suf);
	}

	{
		const char *str = "material";

		/* "material" -> "material", NULL, NULL, 8 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(8, pre_ln);
		EXPECT_EQ(NULL, sep);
		EXPECT_EQ(NULL, suf);
	}
}

/* BLI_str_partition_utf8 */
TEST(string, StrPartitionUtf8)
{
	const unsigned int delim[] = {'-', '.', '_', 0x00F1 /* n tilde */, 0x262F /* ying-yang */, '\0'};
	char *sep, *suf;
	size_t pre_ln;

	{
		const char *str = "ma\xc3\xb1te-r\xe2\x98\xafial";

		/* "ma\xc3\xb1te-r\xe2\x98\xafial" -> "ma", '\xc3\xb1', "te-r\xe2\x98\xafial", 2 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(2, pre_ln);
		EXPECT_EQ(&str[2], sep);
		EXPECT_STREQ("te-r\xe2\x98\xafial", suf);
	}

	/* Corner cases. */
	{
		const char *str = "\xe2\x98\xafmate-rial-\xc3\xb1";

		/* "\xe2\x98\xafmate-rial-\xc3\xb1" -> "", '\xe2\x98\xaf', "mate-rial-\xc3\xb1", 0 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(0, pre_ln);
		EXPECT_EQ(&str[0], sep);
		EXPECT_STREQ("mate-rial-\xc3\xb1", suf);
	}

	{
		const char *str = "\xe2\x98\xaf.\xc3\xb1_.--\xc3\xb1";

		/* "\xe2\x98\xaf.\xc3\xb1_.--\xc3\xb1" -> "", '\xe2\x98\xaf', ".\xc3\xb1_.--\xc3\xb1", 0 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(0, pre_ln);
		EXPECT_EQ(&str[0], sep);
		EXPECT_STREQ(".\xc3\xb1_.--\xc3\xb1", suf);
	}

	{
		const char *str = "";

		/* "" -> "", NULL, NULL, 0 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(0, pre_ln);
		EXPECT_EQ(NULL, sep);
		EXPECT_EQ(NULL, suf);
	}

	{
		const char *str = "material";

		/* "material" -> "material", NULL, NULL, 8 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(8, pre_ln);
		EXPECT_EQ(NULL, sep);
		EXPECT_EQ(NULL, suf);
	}
}

/* BLI_str_rpartition_utf8 */
TEST(string, StrRPartitionUtf8)
{
	const unsigned int delim[] = {'-', '.', '_', 0x00F1 /* n tilde */, 0x262F /* ying-yang */, '\0'};
	char *sep, *suf;
	size_t pre_ln;

	{
		const char *str = "ma\xc3\xb1te-r\xe2\x98\xafial";

		/* "ma\xc3\xb1te-r\xe2\x98\xafial" -> "mat\xc3\xb1te-r", '\xe2\x98\xaf', "ial", 8 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(8, pre_ln);
		EXPECT_EQ(&str[8], sep);
		EXPECT_STREQ("ial", suf);
	}

	/* Corner cases. */
	{
		const char *str = "\xe2\x98\xafmate-rial-\xc3\xb1";

		/* "\xe2\x98\xafmate-rial-\xc3\xb1" -> "\xe2\x98\xafmate-rial-", '\xc3\xb1', "", 13 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(13, pre_ln);
		EXPECT_EQ(&str[13], sep);
		EXPECT_STREQ("", suf);
	}

	{
		const char *str = "\xe2\x98\xaf.\xc3\xb1_.--\xc3\xb1";

		/* "\xe2\x98\xaf.\xc3\xb1_.--\xc3\xb1" -> "\xe2\x98\xaf.\xc3\xb1_.--", '\xc3\xb1', "", 10 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(10, pre_ln);
		EXPECT_EQ(&str[10], sep);
		EXPECT_STREQ("", suf);
	}

	{
		const char *str = "";

		/* "" -> "", NULL, NULL, 0 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(0, pre_ln);
		EXPECT_EQ(NULL, sep);
		EXPECT_EQ(NULL, suf);
	}

	{
		const char *str = "material";

		/* "material" -> "material", NULL, NULL, 8 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(8, pre_ln);
		EXPECT_EQ(NULL, sep);
		EXPECT_EQ(NULL, suf);
	}
}
