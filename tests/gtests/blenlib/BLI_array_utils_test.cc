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
	const std::vector<int> data_cmp  = {3, 2, 1, 0};
	std::vector<int> data            = {0, 1, 2, 3};
	BLI_array_reverse(data.data(), data.size());
	EXPECT_EQ(data, data_cmp);
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
#define BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine) \
{ \
	data_combine.resize(data_cmp.size()); \
	BLI_array_binary_and(data_combine.data(), data_a.data(), data_b.data(), data_cmp.size()); \
	EXPECT_EQ(data_combine, data_cmp); \
} ((void)0)

TEST(array_utils, BinaryAndInt4Zero)
{
	std::vector<int> data_a = {0, 1, 0, 1}, data_b = {1, 0, 1, 0}, data_cmp = {0, 0, 0, 0};
	std::vector<int> data_combine;
	BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine);
}

TEST(array_utils, BinaryAndInt4Mix)
{
	std::vector<int> data_a = {1, 1, 1, 1}, data_b = {1, 0, 1, 0}, data_cmp = {1, 0, 1, 0};
	std::vector<int> data_combine;
	BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine);
}
#undef BINARY_AND_TEST


/* BLI_array_binary_or */
#define BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine) \
	{ \
		data_combine.resize(data_cmp.size()); \
		BLI_array_binary_or(data_combine.data(), data_a.data(), data_b.data(), data_cmp.size()); \
		EXPECT_EQ(data_combine, data_cmp); \
	} ((void)0)

TEST(array_utils, BinaryOrInt4Alternate)
{
	std::vector<int> data_a = {0, 1, 0, 1}, data_b = {1, 0, 1, 0}, data_cmp = {1, 1, 1, 1};
	std::vector<int> data_combine;
	BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine);
}

TEST(array_utils, BinaryOrInt4Mix)
{
	std::vector<int> data_a = {1, 1, 0, 0}, data_b = {0, 0, 1, 0}, data_cmp = {1, 1, 1, 0};
	std::vector<int> data_combine;
	BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine);
}
#undef BINARY_OR_TEST

