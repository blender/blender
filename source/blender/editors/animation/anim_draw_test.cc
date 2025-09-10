/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_constants.h"

#include "ED_anim_api.hh"

#include "DNA_anim_types.h"

#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "CLG_log.h"
#include "testing/testing.h"

namespace blender::animrig::tests {

class AnimDrawTest : public testing::Test {
 public:
  Main *bmain;
  Object *object;

  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    this->bmain = BKE_main_new();
    this->object = BKE_id_new<Object>(this->bmain, "OBTestObject");
  }

  void TearDown() override
  {
    BKE_main_free(this->bmain);
  }
};

TEST_F(AnimDrawTest, anim_unit_mapping_get_factor_not_normalizing)
{
  FCurve *fcurve = MEM_callocN<FCurve>(__func__);
  fcurve->array_index = 0;

  /* Avoid creating a Scene via BKE_id_new<Scene>(this->bmain, "SCTestScene"); as that requires
   * much more setup (appdirs, imbuf for color management, and maybe more). This test doesn't
   * actually need a full Scene, it just needs its `units` field. */
  Scene scene;
  memset(&scene.unit, 0, sizeof(scene.unit));
  scene.unit.scale_length = 1.0f;

  { /* Rotation: Degrees. */
    scene.unit.system_rotation = 0;
    BKE_fcurve_rnapath_set(*fcurve, "rotation_euler");

    EXPECT_FLOAT_EQ(RAD2DEGF(1.0f),
                    ANIM_unit_mapping_get_factor(&scene, &this->object->id, fcurve, 0, nullptr));
    EXPECT_FLOAT_EQ(1.0f / RAD2DEGF(1.0f),
                    ANIM_unit_mapping_get_factor(
                        &scene, &this->object->id, fcurve, ANIM_UNITCONV_RESTORE, nullptr));
  }

  { /* Rotation: Radians. */
    scene.unit.system_rotation = USER_UNIT_ROT_RADIANS;
    BKE_fcurve_rnapath_set(*fcurve, "rotation_euler");

    EXPECT_FLOAT_EQ(1.0f,
                    ANIM_unit_mapping_get_factor(&scene, &this->object->id, fcurve, 0, nullptr));
    EXPECT_FLOAT_EQ(1.0f,
                    ANIM_unit_mapping_get_factor(
                        &scene, &this->object->id, fcurve, ANIM_UNITCONV_RESTORE, nullptr));
  }

  BKE_fcurve_free(fcurve);
}

}  // namespace blender::animrig::tests
