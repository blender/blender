/* Apache License, Version 2.0 */

#include "testing/testing.h"
#include <string.h>

extern "C" {
#include "BLI_compiler_attrs.h"
#include "BLI_heap.h"
#include "BLI_utildefines.h"
#include "BLI_rand.h"

#include "MEM_guardedalloc.h"
};

#define SIZE 1024


static void range_fl(float *array_tar, const int size)
{
	float *array_pt = array_tar + (size - 1);
	int i = size;
	while (i--) {
		*(array_pt--) = (float)i;
	}
}

TEST(heap, Empty)
{
	Heap *heap;

	heap = BLI_heap_new();
	EXPECT_TRUE(BLI_heap_is_empty(heap));
	EXPECT_EQ(BLI_heap_len(heap), 0);
	BLI_heap_free(heap, NULL);
}

TEST(heap, One)
{
	Heap *heap;
	const char *in = "test";

	heap = BLI_heap_new();

	BLI_heap_insert(heap, 0.0f, (void *)in);
	EXPECT_FALSE(BLI_heap_is_empty(heap));
	EXPECT_EQ(BLI_heap_len(heap), 1);
	EXPECT_EQ(in, BLI_heap_pop_min(heap));
	EXPECT_TRUE(BLI_heap_is_empty(heap));
	EXPECT_EQ(BLI_heap_len(heap), 0);
	BLI_heap_free(heap, NULL);
}

TEST(heap, Range)
{
	const int items_total = SIZE;
	Heap *heap = BLI_heap_new();
	for (int in = 0; in < items_total; in++) {
		BLI_heap_insert(heap, (float)in, POINTER_FROM_INT(in));
	}
	for (int out_test = 0; out_test < items_total; out_test++) {
		EXPECT_EQ(out_test, POINTER_AS_INT(BLI_heap_pop_min(heap)));

	}
	EXPECT_TRUE(BLI_heap_is_empty(heap));
	BLI_heap_free(heap, NULL);
}

TEST(heap, RangeReverse)
{
	const int items_total = SIZE;
	Heap *heap = BLI_heap_new();
	for (int in = 0; in < items_total; in++) {
		BLI_heap_insert(heap, (float)-in, POINTER_FROM_INT(-in));
	}
	for (int out_test = items_total - 1; out_test >= 0; out_test--) {
		EXPECT_EQ(-out_test, POINTER_AS_INT(BLI_heap_pop_min(heap)));
	}
	EXPECT_TRUE(BLI_heap_is_empty(heap));
	BLI_heap_free(heap, NULL);
}

TEST(heap, RangeRemove)
{
	const int items_total = SIZE;
	Heap *heap = BLI_heap_new();
	HeapNode **nodes = (HeapNode **)MEM_mallocN(sizeof(HeapNode *) * items_total, __func__);
	for (int in = 0; in < items_total; in++) {
		nodes[in] = BLI_heap_insert(heap, (float)in, POINTER_FROM_INT(in));
	}
	for (int i = 0; i < items_total; i += 2) {
		BLI_heap_remove(heap, nodes[i]);
		nodes[i] = NULL;
	}
	for (int out_test = 1; out_test < items_total; out_test += 2) {
		EXPECT_EQ(out_test, POINTER_AS_INT(BLI_heap_pop_min(heap)));
	}
	EXPECT_TRUE(BLI_heap_is_empty(heap));
	BLI_heap_free(heap, NULL);
	MEM_freeN(nodes);
}

TEST(heap, Duplicates)
{
	const int items_total = SIZE;
	Heap *heap = BLI_heap_new();
	for (int in = 0; in < items_total; in++) {
		BLI_heap_insert(heap, 1.0f, 0);
	}
	for (int out_test = 0; out_test < items_total; out_test++) {
		EXPECT_EQ(0, POINTER_AS_INT(BLI_heap_pop_min(heap)));
	}
	EXPECT_TRUE(BLI_heap_is_empty(heap));
	BLI_heap_free(heap, NULL);
}

static void random_heap_helper(
        const int items_total,
        const int random_seed)
{
	Heap *heap = BLI_heap_new();
	float *values = (float *)MEM_mallocN(sizeof(float) * items_total, __func__);
	range_fl(values, items_total);
	BLI_array_randomize(values, sizeof(float), items_total, random_seed);
	for (int i = 0; i < items_total; i++) {
		BLI_heap_insert(heap, values[i], POINTER_FROM_INT((int)values[i]));
	}
	for (int out_test = 0; out_test < items_total; out_test++) {
		EXPECT_EQ(out_test, POINTER_AS_INT(BLI_heap_pop_min(heap)));
	}
	EXPECT_TRUE(BLI_heap_is_empty(heap));
	BLI_heap_free(heap, NULL);
	MEM_freeN(values);
}

TEST(heap, Rand1)       { random_heap_helper(1, 1234); }
TEST(heap, Rand2)       { random_heap_helper(2, 1234); }
TEST(heap, Rand100)     { random_heap_helper(100, 4321); }


TEST(heap, ReInsertSimple)
{
	const int items_total = SIZE;
	Heap *heap = BLI_heap_new();
	HeapNode **nodes = (HeapNode **)MEM_mallocN(sizeof(HeapNode *) * items_total, __func__);
	for (int in = 0; in < items_total; in++) {
		nodes[in] = BLI_heap_insert(heap, (float)in, POINTER_FROM_INT(in));
	}
	for (int i = 0; i < items_total; i++) {
		BLI_heap_node_value_update(heap, nodes[i], (float)(items_total + i));
	}

	for (int out_test = 0; out_test < items_total; out_test++) {
		EXPECT_EQ(out_test, POINTER_AS_INT(BLI_heap_pop_min(heap)));
	}

	EXPECT_TRUE(BLI_heap_is_empty(heap));
	BLI_heap_free(heap, NULL);
	MEM_freeN(nodes);
}

static void random_heap_reinsert_helper(
        const int items_total,
        const int random_seed)
{
	Heap *heap = BLI_heap_new();
	HeapNode **nodes = (HeapNode **)MEM_mallocN(sizeof(HeapNode *) * items_total, __func__);
	for (int in = 0; in < items_total; in++) {
		nodes[in] = BLI_heap_insert(heap, (float)in, POINTER_FROM_INT(in));
	}
	BLI_array_randomize(nodes, sizeof(HeapNode *), items_total, random_seed);
	for (int i = 0; i < items_total; i++) {
		BLI_heap_node_value_update(heap, nodes[i], (float)i);
	}
	EXPECT_TRUE(BLI_heap_is_valid(heap));

	for (int out_test = 0; out_test < items_total; out_test++) {
		HeapNode *node_top = BLI_heap_top(heap);
		float out = BLI_heap_node_value(node_top);
		EXPECT_EQ((float)out_test, out);
		BLI_heap_pop_min(heap);
	}
	EXPECT_TRUE(BLI_heap_is_empty(heap));
	BLI_heap_free(heap, NULL);
	MEM_freeN(nodes);
}

TEST(heap, ReInsertRandom1)       { random_heap_reinsert_helper(1, 1234); }
TEST(heap, ReInsertRandom2)       { random_heap_reinsert_helper(2, 1234); }
TEST(heap, ReInsertRandom100)     { random_heap_reinsert_helper(100, 4321); }
TEST(heap, ReInsertRandom1024)     { random_heap_reinsert_helper(1024, 9876); }
TEST(heap, ReInsertRandom2048)     { random_heap_reinsert_helper(2048, 5321); }
