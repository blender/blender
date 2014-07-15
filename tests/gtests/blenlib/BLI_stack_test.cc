/* Apache License, Version 2.0 */

#include "testing/testing.h"
#include <string.h>

extern "C" {
#include "BLI_stack.h"
#include "BLI_utildefines.h"
#include "BLI_array.h"
};

#define SIZE 1024

TEST(stack, Empty)
{
	BLI_Stack *stack;

	stack = BLI_stack_new(sizeof(int), __func__);
	EXPECT_EQ(BLI_stack_is_empty(stack), true);
	EXPECT_EQ(BLI_stack_count(stack), 0);
	BLI_stack_free(stack);
}

TEST(stack, One)
{
	BLI_Stack *stack;
	unsigned int in = -1, out = 1;

	stack = BLI_stack_new(sizeof(in), __func__);

	BLI_stack_push(stack, (void *)&in);
	EXPECT_EQ(BLI_stack_is_empty(stack), false);
	EXPECT_EQ(BLI_stack_count(stack), 1);
	BLI_stack_pop(stack, (void *)&out);
	EXPECT_EQ(in, out);
	EXPECT_EQ(BLI_stack_is_empty(stack), true);
	EXPECT_EQ(BLI_stack_count(stack), 0);
	BLI_stack_free(stack);
}

TEST(stack, Range)
{
	const int tot = SIZE;
	BLI_Stack *stack;
	int in, out;

	stack = BLI_stack_new(sizeof(in), __func__);

	for (in = 0; in < tot; in++) {
		BLI_stack_push(stack, (void *)&in);
	}

	for (in = tot - 1; in >= 0; in--) {
		EXPECT_EQ(BLI_stack_is_empty(stack), false);
		BLI_stack_pop(stack, (void *)&out);
		EXPECT_EQ(in, out);

	}
	EXPECT_EQ(BLI_stack_is_empty(stack), true);

	BLI_stack_free(stack);
}

TEST(stack, String)
{
	const int tot = SIZE;
	int i;

	BLI_Stack *stack;
	char in[] = "hello world!";
	char out[sizeof(in)];

	stack = BLI_stack_new(sizeof(in), __func__);

	for (i = 0; i < tot; i++) {
		*((int *)in) = i;
		BLI_stack_push(stack, (void *)in);
	}

	for (i = tot - 1; i >= 0; i--) {
		EXPECT_EQ(BLI_stack_is_empty(stack), false);
		*((int *)in) = i;
		BLI_stack_pop(stack, (void *)&out);
		EXPECT_STREQ(in, out);
	}
	EXPECT_EQ(BLI_stack_is_empty(stack), true);

	BLI_stack_free(stack);
}

TEST(stack, Reuse)
{
	const int sizes[] = {3, 11, 81, 400, 999, 12, 1, 9721, 7, 99, 5, 0};
	int sizes_test[ARRAY_SIZE(sizes)];
	const int *s;
	int in, out, i;
	int sum, sum_test;

	BLI_Stack *stack;

	stack = BLI_stack_new(sizeof(in), __func__);

	/* add a bunch of numbers, ensure we get same sum out */
	sum = 0;
	for (s = sizes; *s; s++) {
		for (i = *s; i != 0; i--) {
			BLI_stack_push(stack, (void *)&i);
			sum += i;
		}
	}
	sum_test = 0;
	while (!BLI_stack_is_empty(stack)) {
		BLI_stack_pop(stack, (void *)&out);
		sum_test += out;
	}
	EXPECT_EQ(sum, sum_test);

	/* add and remove all except last */
	for (s = sizes; *s; s++) {
		for (i = *s; i >= 0; i--) {
			BLI_stack_push(stack, (void *)&i);
		}
		for (i = *s; i > 0; i--) {
			BLI_stack_pop(stack, (void *)&out);
		}
	}

	i = ARRAY_SIZE(sizes) - 1;
	while (!BLI_stack_is_empty(stack)) {
		i--;
		BLI_stack_pop(stack, (void *)&sizes_test[i]);
		EXPECT_EQ(sizes[i], sizes_test[i]);
		EXPECT_GT(i, -1);
	}
	EXPECT_EQ(i, 0);
	EXPECT_EQ(memcmp(sizes, sizes_test, sizeof(sizes) - sizeof(int)), 0);


	/* finally test BLI_stack_pop_n */
	for (i = ARRAY_SIZE(sizes); i--; ) {
		BLI_stack_push(stack, (void *)&sizes[i]);
	}
	EXPECT_EQ(BLI_stack_count(stack), ARRAY_SIZE(sizes));
	BLI_stack_pop_n(stack, (void *)sizes_test, ARRAY_SIZE(sizes));
	EXPECT_EQ(memcmp(sizes, sizes_test, sizeof(sizes) - sizeof(int)), 0);

	BLI_stack_free(stack);
}
