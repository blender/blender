/* Apache License, Version 2.0 */

#include "testing/testing.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_array_utils.h"
}

/* -------------------------------------------------------------------- */
/* tests */

/* BLI_array_reverse */
TEST(array_utils, ReverseStringEmpty)
{
	char data[] = "";
	BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
	EXPECT_STREQ("", data);
}

TEST(array_utils, ReverseStringSingle)
{
	char data[] = "0";
	BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
	EXPECT_STREQ("0", data);
}

TEST(array_utils, ReverseString4)
{
	char data[] = "0123";
	BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
	EXPECT_STREQ("3210", data);
}

TEST(array_utils, ReverseInt4)
{
	const int data_cmp[] = {3, 2, 1, 0};
	int data[]           = {0, 1, 2, 3};
	BLI_array_reverse(data, ARRAY_SIZE(data));
	EXPECT_EQ_ARRAY(data_cmp, data, ARRAY_SIZE(data));
}

/* BLI_array_findindex */
TEST(array_utils, FindIndexStringEmpty)
{
	char data[] = "", find = '0';
	EXPECT_EQ(-1, BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find));
}

TEST(array_utils, FindIndexStringSingle)
{
	char data[] = "0", find = '0';
	EXPECT_EQ(0, BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find));
}

TEST(array_utils, FindIndexStringSingleMissing)
{
	char data[] = "1", find = '0';
	EXPECT_EQ(-1, BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find));
}

TEST(array_utils, FindIndexString4)
{
	char data[] = "0123", find = '3';
	EXPECT_EQ(3, BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find));
}

TEST(array_utils, FindIndexInt4)
{
	int data[] = {0, 1, 2, 3}, find = 2;
	EXPECT_EQ(2, BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find));
}

/* BLI_array_binary_and */
#define BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, length) \
{ \
	BLI_array_binary_and(data_combine, data_a, data_b, length); \
	EXPECT_EQ_ARRAY(data_cmp, data_combine, length); \
} ((void)0)

TEST(array_utils, BinaryAndInt4Zero)
{
	const int data_cmp[] = {0, 0, 0, 0};
	int data_a[] = {0, 1, 0, 1}, data_b[] = {1, 0, 1, 0};
	int data_combine[ARRAY_SIZE(data_cmp)];
	BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}

TEST(array_utils, BinaryAndInt4Mix)
{
	const int data_cmp[] = {1, 0, 1, 0};
	int data_a[] = {1, 1, 1, 1}, data_b[] = {1, 0, 1, 0};
	int data_combine[ARRAY_SIZE(data_cmp)];
	BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}
#undef BINARY_AND_TEST


/* BLI_array_binary_or */
#define BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, length) \
{ \
	BLI_array_binary_or(data_combine, data_a, data_b, length); \
	EXPECT_EQ_ARRAY(data_combine, data_cmp, length); \
} ((void)0)

TEST(array_utils, BinaryOrInt4Alternate)
{
	int data_a[] = {0, 1, 0, 1}, data_b[] = {1, 0, 1, 0}, data_cmp[] = {1, 1, 1, 1};
	int data_combine[ARRAY_SIZE(data_cmp)];
	BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}

TEST(array_utils, BinaryOrInt4Mix)
{
	int data_a[] = {1, 1, 0, 0}, data_b[] = {0, 0, 1, 0}, data_cmp[] = {1, 1, 1, 0};
	int data_combine[ARRAY_SIZE(data_cmp)];
	BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}
#undef BINARY_OR_TEST

