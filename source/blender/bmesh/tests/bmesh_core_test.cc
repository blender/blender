#include "testing/testing.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "bmesh.h"

TEST(bmesh_core, BMVertCreate)
{
  BMesh *bm;
  BMVert *bv1, *bv2, *bv3;
  const float co1[3] = {1.0f, 2.0f, 0.0f};

  BMeshCreateParams bm_params;
  bm_params.use_toolflags = true;
  bm = BM_mesh_create(&bm_mesh_allocsize_default, &bm_params);
  EXPECT_EQ(bm->totvert, 0);
  /* make a custom layer so we can see if it is copied properly */
  BM_data_layer_add(bm, &bm->vdata, CD_PROP_FLOAT);
  bv1 = BM_vert_create(bm, co1, NULL, BM_CREATE_NOP);
  ASSERT_TRUE(bv1 != NULL);
  EXPECT_EQ(bv1->co[0], 1.0f);
  EXPECT_EQ(bv1->co[1], 2.0f);
  EXPECT_EQ(bv1->co[2], 0.0f);
  EXPECT_TRUE(is_zero_v3(bv1->no));
  EXPECT_EQ(bv1->head.htype, (char)BM_VERT);
  EXPECT_EQ(bv1->head.hflag, 0);
  EXPECT_EQ(bv1->head.api_flag, 0);
  bv2 = BM_vert_create(bm, NULL, NULL, BM_CREATE_NOP);
  ASSERT_TRUE(bv2 != NULL);
  EXPECT_TRUE(is_zero_v3(bv2->co));
  /* create with example should copy custom data but not select flag */
  BM_vert_select_set(bm, bv2, true);
  BM_elem_float_data_set(&bm->vdata, bv2, CD_PROP_FLOAT, 1.5f);
  bv3 = BM_vert_create(bm, co1, bv2, BM_CREATE_NOP);
  ASSERT_TRUE(bv3 != NULL);
  EXPECT_FALSE(BM_elem_flag_test((BMElem *)bv3, BM_ELEM_SELECT));
  EXPECT_EQ(BM_elem_float_data_get(&bm->vdata, bv3, CD_PROP_FLOAT), 1.5f);
  EXPECT_EQ(BM_mesh_elem_count(bm, BM_VERT), 3);
  BM_mesh_free(bm);
}
