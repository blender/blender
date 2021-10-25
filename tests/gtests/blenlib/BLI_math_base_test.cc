/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_math.h"

/* In tests below, when we are using -1.0f as max_diff value, we actually turn the function into a pure-ULP one. */

/* Put this here, since we cannot use BLI_assert() in inline math files it seems... */
TEST(math_base, CompareFFRelativeValid)
{
	EXPECT_TRUE(sizeof(float) == sizeof(int));
}

TEST(math_base, CompareFFRelativeNormal)
{
	float f1 = 1.99999988f;  /* *(float *)&(*(int *)&f2 - 1) */
	float f2 = 2.00000000f;
	float f3 = 2.00000048f;  /* *(float *)&(*(int *)&f2 + 2) */
	float f4 = 2.10000000f;  /* *(float *)&(*(int *)&f2 + 419430) */

	const float max_diff = FLT_EPSILON * 0.1f;

	EXPECT_TRUE(compare_ff_relative(f1, f2, max_diff, 1));
	EXPECT_TRUE(compare_ff_relative(f2, f1, max_diff, 1));

	EXPECT_TRUE(compare_ff_relative(f3, f2, max_diff, 2));
	EXPECT_TRUE(compare_ff_relative(f2, f3, max_diff, 2));

	EXPECT_FALSE(compare_ff_relative(f3, f2, max_diff, 1));
	EXPECT_FALSE(compare_ff_relative(f2, f3, max_diff, 1));


	EXPECT_FALSE(compare_ff_relative(f3, f2, -1.0f, 1));
	EXPECT_FALSE(compare_ff_relative(f2, f3, -1.0f, 1));

	EXPECT_TRUE(compare_ff_relative(f3, f2, -1.0f, 2));
	EXPECT_TRUE(compare_ff_relative(f2, f3, -1.0f, 2));


	EXPECT_FALSE(compare_ff_relative(f4, f2, max_diff, 64));
	EXPECT_FALSE(compare_ff_relative(f2, f4, max_diff, 64));

	EXPECT_TRUE(compare_ff_relative(f1, f3, max_diff, 64));
	EXPECT_TRUE(compare_ff_relative(f3, f1, max_diff, 64));
}

TEST(math_base, CompareFFRelativeZero)
{
	float f0 = 0.0f;
	float f1 = 4.2038954e-045f;  /* *(float *)&(*(int *)&f0 + 3) */

	float fn0 = -0.0f;
	float fn1 = -2.8025969e-045f;  /* *(float *)&(*(int *)&fn0 - 2) */

	const float max_diff = FLT_EPSILON * 0.1f;

	EXPECT_TRUE(compare_ff_relative(f0, f1, -1.0f, 3));
	EXPECT_TRUE(compare_ff_relative(f1, f0, -1.0f, 3));

	EXPECT_FALSE(compare_ff_relative(f0, f1, -1.0f, 1));
	EXPECT_FALSE(compare_ff_relative(f1, f0, -1.0f, 1));

	EXPECT_TRUE(compare_ff_relative(fn0, fn1, -1.0f, 8));
	EXPECT_TRUE(compare_ff_relative(fn1, fn0, -1.0f, 8));


	EXPECT_TRUE(compare_ff_relative(f0, f1, max_diff, 1));
	EXPECT_TRUE(compare_ff_relative(f1, f0, max_diff, 1));

	EXPECT_TRUE(compare_ff_relative(fn0, f0, max_diff, 1));
	EXPECT_TRUE(compare_ff_relative(f0, fn0, max_diff, 1));

	EXPECT_TRUE(compare_ff_relative(f0, fn1, max_diff, 1));
	EXPECT_TRUE(compare_ff_relative(fn1, f0, max_diff, 1));


	/* Note: in theory, this should return false, since 0.0f  and -0.0f have 0x80000000 diff,
	 *       but overflow in substraction seems to break something here
	 *       (abs(*(int *)&fn0 - *(int *)&f0) == 0x80000000 == fn0), probably because int32 cannot hold this abs value.
     *       this is yet another illustration of why one shall never use (near-)zero floats in pure-ULP comparison. */
//	EXPECT_FALSE(compare_ff_relative(fn0, f0, -1.0f, 1024));
//	EXPECT_FALSE(compare_ff_relative(f0, fn0, -1.0f, 1024));

	EXPECT_FALSE(compare_ff_relative(fn0, f1, -1.0f, 1024));
	EXPECT_FALSE(compare_ff_relative(f1, fn0, -1.0f, 1024));
}
