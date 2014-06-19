#include "testing/testing.h"

#include "BLI_utildefines.h"
#include "bmesh.h"
#include "BLI_math.h"

TEST(bmesh_core, BMVertCreate) {
	BMesh *bm;
	BMVert *bv1, *bv2, *bv3;
	const float co1[3] = {1.0f, 2.0f, 0.0f};

	bm = BM_mesh_create(&bm_mesh_allocsize_default);
	EXPECT_EQ(0, bm->totvert);
	/* make a custom layer so we can see if it is copied properly */
	BM_data_layer_add(bm, &bm->vdata, CD_PROP_FLT);
	bv1 = BM_vert_create(bm, co1, NULL, BM_CREATE_NOP);
	ASSERT_TRUE(bv1 != NULL);
	EXPECT_EQ(1.0f, bv1->co[0]);
	EXPECT_EQ(2.0f, bv1->co[1]);
	EXPECT_EQ(0.0f, bv1->co[2]);
	EXPECT_TRUE(is_zero_v3(bv1->no));
	EXPECT_EQ((char)BM_VERT, bv1->head.htype);
	EXPECT_EQ(0, bv1->head.hflag);
	EXPECT_EQ(0, bv1->head.api_flag);
	bv2 = BM_vert_create(bm, NULL, NULL, BM_CREATE_NOP);
	ASSERT_TRUE(bv2 != NULL);
	EXPECT_TRUE(is_zero_v3(bv2->co));
	/* create with example should copy custom data but not select flag */
	BM_vert_select_set(bm, bv2, true);
	BM_elem_float_data_set(&bm->vdata, bv2, CD_PROP_FLT, 1.5f);
	bv3 = BM_vert_create(bm, co1, bv2, BM_CREATE_NOP);
	ASSERT_TRUE(bv3 != NULL);
	EXPECT_FALSE(BM_elem_flag_test((BMElem *)bv3, BM_ELEM_SELECT));
	EXPECT_EQ(1.5f, BM_elem_float_data_get(&bm->vdata, bv3, CD_PROP_FLT));
	EXPECT_EQ(3, BM_mesh_elem_count(bm, BM_VERT));
}
