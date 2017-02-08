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
	const char *sep, *suf;
	size_t pre_ln;

	{
		const char *str = "mat.e-r_ial";

		/* "mat.e-r_ial" -> "mat", '.', "e-r_ial", 3 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 3);
		EXPECT_EQ(&str[3], sep);
		EXPECT_STREQ("e-r_ial", suf);
	}

	/* Corner cases. */
	{
		const char *str = ".mate-rial--";

		/* ".mate-rial--" -> "", '.', "mate-rial--", 0 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 0);
		EXPECT_EQ(&str[0], sep);
		EXPECT_STREQ("mate-rial--", suf);
	}

	{
		const char *str = ".__.--_";

		/* ".__.--_" -> "", '.', "__.--_", 0 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 0);
		EXPECT_EQ(&str[0], sep);
		EXPECT_STREQ("__.--_", suf);
	}

	{
		const char *str = "";

		/* "" -> "", NULL, NULL, 0 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 0);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}

	{
		const char *str = "material";

		/* "material" -> "material", NULL, NULL, 8 */
		pre_ln = BLI_str_partition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 8);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}
}

/* BLI_str_rpartition */
TEST(string, StrRPartition)
{
	const char delim[] = {'-', '.', '_', '~', '\\', '\0'};
	const char *sep, *suf;
	size_t pre_ln;

	{
		const char *str = "mat.e-r_ial";

		/* "mat.e-r_ial" -> "mat.e-r", '_', "ial", 7 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 7);
		EXPECT_EQ(&str[7], sep);
		EXPECT_STREQ("ial", suf);
	}

	/* Corner cases. */
	{
		const char *str = ".mate-rial--";

		/* ".mate-rial--" -> ".mate-rial-", '-', "", 11 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 11);
		EXPECT_EQ(&str[11], sep);
		EXPECT_STREQ("", suf);
	}

	{
		const char *str = ".__.--_";

		/* ".__.--_" -> ".__.--", '_', "", 6 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 6);
		EXPECT_EQ(&str[6], sep);
		EXPECT_STREQ("", suf);
	}

	{
		const char *str = "";

		/* "" -> "", NULL, NULL, 0 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 0);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}

	{
		const char *str = "material";

		/* "material" -> "material", NULL, NULL, 8 */
		pre_ln = BLI_str_rpartition(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 8);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}
}

/* BLI_str_partition_ex */
TEST(string, StrPartitionEx)
{
	const char delim[] = {'-', '.', '_', '~', '\\', '\0'};
	const char *sep, *suf;
	size_t pre_ln;

	/* Only considering 'from_right' cases here. */

	{
		const char *str = "mat.e-r_ia.l";

		/* "mat.e-r_ia.l" over "mat.e-r" -> "mat.e", '.', "r_ia.l", 3 */
		pre_ln = BLI_str_partition_ex(str, str + 6, delim, &sep, &suf, true);
		EXPECT_EQ(pre_ln, 5);
		EXPECT_EQ(&str[5], sep);
		EXPECT_STREQ("r_ia.l", suf);
	}

	/* Corner cases. */
	{
		const char *str = "mate.rial";

		/* "mate.rial" over "mate" -> "mate.rial", NULL, NULL, 4 */
		pre_ln = BLI_str_partition_ex(str, str + 4, delim, &sep, &suf, true);
		EXPECT_EQ(pre_ln, 4);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}
}

/* BLI_str_partition_utf8 */
TEST(string, StrPartitionUtf8)
{
	const unsigned int delim[] = {'-', '.', '_', 0x00F1 /* n tilde */, 0x262F /* ying-yang */, '\0'};
	const char *sep, *suf;
	size_t pre_ln;

	{
		const char *str = "ma\xc3\xb1te-r\xe2\x98\xafial";

		/* "ma\xc3\xb1te-r\xe2\x98\xafial" -> "ma", '\xc3\xb1', "te-r\xe2\x98\xafial", 2 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 2);
		EXPECT_EQ(&str[2], sep);
		EXPECT_STREQ("te-r\xe2\x98\xafial", suf);
	}

	/* Corner cases. */
	{
		const char *str = "\xe2\x98\xafmate-rial-\xc3\xb1";

		/* "\xe2\x98\xafmate-rial-\xc3\xb1" -> "", '\xe2\x98\xaf', "mate-rial-\xc3\xb1", 0 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 0);
		EXPECT_EQ(&str[0], sep);
		EXPECT_STREQ("mate-rial-\xc3\xb1", suf);
	}

	{
		const char *str = "\xe2\x98\xaf.\xc3\xb1_.--\xc3\xb1";

		/* "\xe2\x98\xaf.\xc3\xb1_.--\xc3\xb1" -> "", '\xe2\x98\xaf', ".\xc3\xb1_.--\xc3\xb1", 0 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 0);
		EXPECT_EQ(&str[0], sep);
		EXPECT_STREQ(".\xc3\xb1_.--\xc3\xb1", suf);
	}

	{
		const char *str = "";

		/* "" -> "", NULL, NULL, 0 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 0);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}

	{
		const char *str = "material";

		/* "material" -> "material", NULL, NULL, 8 */
		pre_ln = BLI_str_partition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 8);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}
}

/* BLI_str_rpartition_utf8 */
TEST(string, StrRPartitionUtf8)
{
	const unsigned int delim[] = {'-', '.', '_', 0x00F1 /* n tilde */, 0x262F /* ying-yang */, '\0'};
	const char *sep, *suf;
	size_t pre_ln;

	{
		const char *str = "ma\xc3\xb1te-r\xe2\x98\xafial";

		/* "ma\xc3\xb1te-r\xe2\x98\xafial" -> "mat\xc3\xb1te-r", '\xe2\x98\xaf', "ial", 8 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 8);
		EXPECT_EQ(&str[8], sep);
		EXPECT_STREQ("ial", suf);
	}

	/* Corner cases. */
	{
		const char *str = "\xe2\x98\xafmate-rial-\xc3\xb1";

		/* "\xe2\x98\xafmate-rial-\xc3\xb1" -> "\xe2\x98\xafmate-rial-", '\xc3\xb1', "", 13 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 13);
		EXPECT_EQ(&str[13], sep);
		EXPECT_STREQ("", suf);
	}

	{
		const char *str = "\xe2\x98\xaf.\xc3\xb1_.--\xc3\xb1";

		/* "\xe2\x98\xaf.\xc3\xb1_.--\xc3\xb1" -> "\xe2\x98\xaf.\xc3\xb1_.--", '\xc3\xb1', "", 10 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 10);
		EXPECT_EQ(&str[10], sep);
		EXPECT_STREQ("", suf);
	}

	{
		const char *str = "";

		/* "" -> "", NULL, NULL, 0 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 0);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}

	{
		const char *str = "material";

		/* "material" -> "material", NULL, NULL, 8 */
		pre_ln = BLI_str_rpartition_utf8(str, delim, &sep, &suf);
		EXPECT_EQ(pre_ln, 8);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}
}

/* BLI_str_partition_ex_utf8 */
TEST(string, StrPartitionExUtf8)
{
	const unsigned int delim[] = {'-', '.', '_', 0x00F1 /* n tilde */, 0x262F /* ying-yang */, '\0'};
	const char *sep, *suf;
	size_t pre_ln;

	/* Only considering 'from_right' cases here. */

	{
		const char *str = "ma\xc3\xb1te-r\xe2\x98\xafial";

		/* "ma\xc3\xb1te-r\xe2\x98\xafial" over "ma\xc3\xb1te" -> "ma", '\xc3\xb1', "te-r\xe2\x98\xafial", 2 */
		pre_ln = BLI_str_partition_ex_utf8(str, str + 6, delim, &sep, &suf, true);
		EXPECT_EQ(pre_ln, 2);
		EXPECT_EQ(&str[2], sep);
		EXPECT_STREQ("te-r\xe2\x98\xafial", suf);
	}

	/* Corner cases. */
	{
		const char *str = "mate\xe2\x98\xafrial";

		/* "mate\xe2\x98\xafrial" over "mate" -> "mate\xe2\x98\xafrial", NULL, NULL, 4 */
		pre_ln = BLI_str_partition_ex_utf8(str, str + 4, delim, &sep, &suf, true);
		EXPECT_EQ(pre_ln, 4);
		EXPECT_EQ(sep, (void*)NULL);
		EXPECT_EQ(suf, (void*)NULL);
	}
}

/* BLI_str_format_int_grouped */
TEST(string, StrFormatIntGrouped)
{
	char num_str[16];
	int num;

	BLI_str_format_int_grouped(num_str, num = 0);
	EXPECT_STREQ("0", num_str);

	BLI_str_format_int_grouped(num_str, num = 1);
	EXPECT_STREQ("1", num_str);

	BLI_str_format_int_grouped(num_str, num = -1);
	EXPECT_STREQ("-1", num_str);

	BLI_str_format_int_grouped(num_str, num = -2147483648);
	EXPECT_STREQ("-2,147,483,648", num_str);

	BLI_str_format_int_grouped(num_str, num = 2147483647);
	EXPECT_STREQ("2,147,483,647", num_str);

	BLI_str_format_int_grouped(num_str, num = 1000);
	EXPECT_STREQ("1,000", num_str);

	BLI_str_format_int_grouped(num_str, num = -1000);
	EXPECT_STREQ("-1,000", num_str);

	BLI_str_format_int_grouped(num_str, num = 999);
	EXPECT_STREQ("999", num_str);

	BLI_str_format_int_grouped(num_str, num = -999);
	EXPECT_STREQ("-999", num_str);
}

#define STRING_FIND_SPLIT_WORDS_EX(word_str_src, word_str_src_len, limit_words, ...) \
{ \
	int word_info[][2] = \
		{{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}}; \
	const int word_cmp[][2] = __VA_ARGS__; \
	const int word_cmp_size_input = ARRAY_SIZE(word_cmp) - (limit_words ? 1 : 0); \
	const int word_cmp_size = ARRAY_SIZE(word_cmp); \
	const int word_num = BLI_string_find_split_words( \
	        word_str_src, word_str_src_len, ' ', word_info, word_cmp_size_input); \
	EXPECT_EQ(word_cmp_size - 1, word_num); \
	EXPECT_EQ_ARRAY_ND<const int[2]>(word_cmp, word_info, word_cmp_size, 2); \
} ((void)0)

#define STRING_FIND_SPLIT_WORDS(word_str_src, ...) \
	STRING_FIND_SPLIT_WORDS_EX(word_str_src, strlen(word_str_src), false, __VA_ARGS__)

/* BLI_string_find_split_words */
TEST(string, StringFindSplitWords_Single)
{
	STRING_FIND_SPLIT_WORDS("t",    {{0, 1}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS("test", {{0, 4}, {-1, -1}});
}
TEST(string, StringFindSplitWords_Triple)
{
	STRING_FIND_SPLIT_WORDS("f t w",            {{0, 1}, {2, 1}, {4, 1}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS("find three words", {{0, 4}, {5, 5}, {11, 5}, {-1, -1}});
}
TEST(string, StringFindSplitWords_Spacing)
{
	STRING_FIND_SPLIT_WORDS("# ## ### ####",   {{0, 1}, {2, 2}, {5, 3}, {9, 4}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS("#  #   #    #",   {{0, 1}, {3, 1}, {7, 1}, {12, 1}, {-1, -1}});
}
TEST(string, StringFindSplitWords_Trailing_Left)
{
	STRING_FIND_SPLIT_WORDS("   t",    {{3, 1}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS("   test", {{3, 4}, {-1, -1}});
}
TEST(string, StringFindSplitWords_Trailing_Right)
{
	STRING_FIND_SPLIT_WORDS("t   ",    {{0, 1}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS("test   ", {{0, 4}, {-1, -1}});
}
TEST(string, StringFindSplitWords_Trailing_LeftRight)
{
	STRING_FIND_SPLIT_WORDS("   surrounding space test   123   ", {{3, 11}, {15, 5}, {21, 4}, {28, 3}, {-1, -1}});
}
TEST(string, StringFindSplitWords_Blank)
{
	STRING_FIND_SPLIT_WORDS("", {{-1, -1}});
}
TEST(string, StringFindSplitWords_Whitespace)
{
	STRING_FIND_SPLIT_WORDS(" ",    {{-1, -1}});
	STRING_FIND_SPLIT_WORDS("    ", {{-1, -1}});
}
TEST(string, StringFindSplitWords_LimitWords)
{
	const char *words = "too many words";
	const int words_len = strlen(words);
	STRING_FIND_SPLIT_WORDS_EX(words, words_len, false, {{0, 3}, {4, 4}, {9, 5}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS_EX(words, words_len, true,  {{0, 3}, {4, 4}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS_EX(words, words_len, true,  {{0, 3}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS_EX(words, words_len, true,  {{-1, -1}});
}
TEST(string, StringFindSplitWords_LimitChars)
{
	const char *words = "too many chars";
	const int words_len = strlen(words);
	STRING_FIND_SPLIT_WORDS_EX(words, words_len,      false, {{0, 3}, {4, 4}, {9, 5}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS_EX(words, words_len -  1, false, {{0, 3}, {4, 4}, {9, 4}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS_EX(words, words_len -  5, false, {{0, 3}, {4, 4}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS_EX(words, 1,              false, {{0, 1}, {-1, -1}});
	STRING_FIND_SPLIT_WORDS_EX(words, 0,              false, {{-1, -1}});
}

#undef STRING_FIND_SPLIT_WORDS


/* BLI_strncasestr */
TEST(string, StringStrncasestr)
{
	const char *str_test0 = "search here";
	const char *res;

	res = BLI_strncasestr(str_test0, "", 0);
	EXPECT_EQ(res, str_test0);

	res = BLI_strncasestr(str_test0, " ", 1);
	EXPECT_EQ(res, str_test0 + 6);

	res = BLI_strncasestr(str_test0, "her", 3);
	EXPECT_EQ(res, str_test0 + 7);

	res = BLI_strncasestr(str_test0, "ARCh", 4);
	EXPECT_EQ(res, str_test0 + 2);

	res = BLI_strncasestr(str_test0, "earcq", 4);
	EXPECT_EQ(res, str_test0 + 1);

	res = BLI_strncasestr(str_test0, "not there", 9);
	EXPECT_EQ(res, (void*)NULL);
}
