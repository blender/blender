/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_constants.hh"

#include "ED_anim_api.hh"

#include "DNA_anim_types.h"

#include "BKE_effect.h"
#include "BKE_fcurve.hh"
#include "BKE_gtest_base.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_unit.hh"

#include "RNA_define.hh"
#include "RNA_types.hh"

#include "testing/testing.h"

namespace blender::animrig::tests {

class AnimDrawTest : public bke::BlenderGTestBase {
 public:
  Main *bmain;
  Object *object;

  void SetUp() override
  {
    this->bmain = BKE_main_new();
    this->object = BKE_id_new<Object>(this->bmain, "OBTestObject");
    this->object->id.properties = bke::idprop::create_group("IDPropertyGroup").release();

    IDProperty *time_prop = bke::idprop::create("time", 1.0f).release();
    IDP_ui_data_ensure(time_prop)->rna_subtype = PROP_TIME;
    IDProperty *mass_prop = bke::idprop::create("mass", 1.0f).release();
    IDP_ui_data_ensure(mass_prop)->rna_subtype = PROP_MASS;

    IDP_AddToGroup(object->id.properties, time_prop);
    IDP_AddToGroup(object->id.properties, mass_prop);
  }

  void TearDown() override
  {
    BKE_main_free(this->bmain);
    RNA_exit();
  }
};

TEST_F(AnimDrawTest, anim_unit_mapping_get_factor_not_normalizing)
{
  FCurve *fcurve = BKE_fcurve_create();
  fcurve->array_index = 0;

  /* Avoid creating a Scene via BKE_id_new<Scene>(this->bmain, "SCTestScene"); as that requires
   * much more setup (appdirs, imbuf for color management, and maybe more). This test doesn't
   * actually need a full Scene, it just needs its `units` field. */
  Scene scene = {};
  scene.unit.scale_length = 1.0f;

  { /* Rotation: Degrees. */
    scene.unit.system_rotation = USER_UNIT_ROT_DEGREES;
    fcurve->rna_path_set("rotation_euler");

    EXPECT_FLOAT_EQ(RAD2DEGF(1.0f),
                    ANIM_unit_mapping_get_factor(&scene, &this->object->id, fcurve, 0, nullptr));
    EXPECT_FLOAT_EQ(1.0f / RAD2DEGF(1.0f),
                    ANIM_unit_mapping_get_factor(
                        &scene, &this->object->id, fcurve, ANIM_UNITCONV_RESTORE, nullptr));
  }

  { /* Rotation: Radians. */
    scene.unit.system_rotation = USER_UNIT_ROT_RADIANS;
    fcurve->rna_path_set("rotation_euler");

    EXPECT_FLOAT_EQ(1.0f,
                    ANIM_unit_mapping_get_factor(&scene, &this->object->id, fcurve, 0, nullptr));
    EXPECT_FLOAT_EQ(1.0f,
                    ANIM_unit_mapping_get_factor(
                        &scene, &this->object->id, fcurve, ANIM_UNITCONV_RESTORE, nullptr));
  }

  { /* Length */
    fcurve->rna_path_set("location");
    const auto test_unit_scalar = [&](const int unit_system,
                                      const int unit_idx,
                                      const float expected_scalar,
                                      const float expected_restore_scalar) {
      scene.unit.system = unit_system;
      scene.unit.length_unit = unit_idx;

      EXPECT_FLOAT_EQ(expected_scalar,
                      ANIM_unit_mapping_get_factor(&scene, &this->object->id, fcurve, 0, nullptr));
      EXPECT_FLOAT_EQ(expected_restore_scalar,
                      ANIM_unit_mapping_get_factor(
                          &scene, &this->object->id, fcurve, ANIM_UNITCONV_RESTORE, nullptr));
    };

    /* No unit system */
    test_unit_scalar(USER_UNIT_NONE, 0, 1.0f, 1.0f);

    /* Metric */
    test_unit_scalar(USER_UNIT_METRIC, 0 /* km */, 1.0f / 1000.0f, 1000.0f);
    test_unit_scalar(USER_UNIT_METRIC, 3 /* m */, 1.0f, 1.0f);
    test_unit_scalar(USER_UNIT_METRIC, 5 /* cm */, 1.0f / 0.01f, 0.01f);
    test_unit_scalar(USER_UNIT_METRIC, 6 /* mm */, 1.0f / 0.001f, 0.001f);
    test_unit_scalar(USER_UNIT_METRIC, 7 /* um */, 1.0f / 0.000001f, 0.000001f);
    /* Adaptive - meter */
    test_unit_scalar(USER_UNIT_METRIC, USER_UNIT_ADAPTIVE, 1.0f, 1.0f);

    /* Imperial */
    test_unit_scalar(USER_UNIT_IMPERIAL, 0 /* mi */, 1.0f / 1609.344f, 1609.344f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 4 /* ft */, 1.0f / 0.3048f, 0.3048f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 5 /* in */, 1.0f / 0.0254f, 0.0254f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 6 /* thou */, 1.0f / 0.0000254f, 0.0000254f);
    test_unit_scalar(USER_UNIT_IMPERIAL, USER_UNIT_ADAPTIVE, 1.0f / 0.3048f, 0.3048f);

    /* Unit Scale = 2.0 */
    scene.unit.scale_length = 2.0f;
    test_unit_scalar(USER_UNIT_METRIC, 3 /* m */, 2.0f, 0.5f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 4 /* ft */, 2.0f / 0.3048f, 0.3048f / 2.0f);
    scene.unit.scale_length = 1.0f;
  }

  { /* Mass */
    fcurve->rna_path_set("[\"mass\"]");
    const auto test_unit_scalar = [&](const int unit_system,
                                      const int unit_idx,
                                      const float expected_scalar,
                                      const float expected_restore_scalar) {
      scene.unit.system = unit_system;
      scene.unit.mass_unit = unit_idx;

      EXPECT_FLOAT_EQ(expected_scalar,
                      ANIM_unit_mapping_get_factor(&scene, &this->object->id, fcurve, 0, nullptr));
      EXPECT_FLOAT_EQ(expected_restore_scalar,
                      ANIM_unit_mapping_get_factor(
                          &scene, &this->object->id, fcurve, ANIM_UNITCONV_RESTORE, nullptr));
    };

    /* Metric */
    test_unit_scalar(USER_UNIT_METRIC, 0 /* t */, 1.0f / 1000.0f, 1000.0f);
    test_unit_scalar(USER_UNIT_METRIC, 2 /* kg */, 1.0f, 1.0f);
    test_unit_scalar(USER_UNIT_METRIC, 5 /* g */, 1.0f / 0.001f, 0.001f);
    test_unit_scalar(USER_UNIT_METRIC, 6 /* mg */, 1.0f / 0.000001f, 0.000001f);
    test_unit_scalar(USER_UNIT_METRIC, USER_UNIT_ADAPTIVE, 1.0f, 1.0f);

    /* Imperial */
    test_unit_scalar(USER_UNIT_IMPERIAL, 0 /* tn */, 1.0f / 907.18474f, 907.18474f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 1 /* cwt */, 1.0f / 45.359237f, 45.359237f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 2 /* st */, 1.0f / 6.35029318f, 6.35029318f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 3 /* lb */, 1.0f / 0.45359237f, 0.45359237f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 4 /* oz */, 1.0f / 0.028349523125f, 0.028349523125f);
    test_unit_scalar(USER_UNIT_IMPERIAL, USER_UNIT_ADAPTIVE, 1.0f / 0.45359237f, 0.45359237f);

    /* Unit Scale = 2.0 */
    scene.unit.scale_length = 2.0f;
    test_unit_scalar(USER_UNIT_METRIC, 2 /* kg */, 8.0f, 1.0f / 8.0f);
    test_unit_scalar(USER_UNIT_IMPERIAL, 3 /* lb */, 8.0f / 0.45359237f, 0.45359237f / 8.0f);
    scene.unit.scale_length = 1.0f;
  }

  { /* Time */
    fcurve->rna_path_set("[\"time\"]");
    const auto test_unit_scalar = [&](const int unit_idx,
                                      const float expected_scalar,
                                      const float expected_restore_scalar) {
      scene.unit.time_unit = unit_idx;

      EXPECT_FLOAT_EQ(expected_scalar,
                      ANIM_unit_mapping_get_factor(&scene, &this->object->id, fcurve, 0, nullptr));
      EXPECT_FLOAT_EQ(expected_restore_scalar,
                      ANIM_unit_mapping_get_factor(
                          &scene, &this->object->id, fcurve, ANIM_UNITCONV_RESTORE, nullptr));
    };

    test_unit_scalar(0 /* d */, 1.0f / 86400.0f, 86400.0f);
    test_unit_scalar(1 /* h */, 1.0f / 3600.0f, 3600.0f);
    test_unit_scalar(2 /* min */, 1.0f / 60.0f, 60.0f);
    test_unit_scalar(3 /* s */, 1.0f, 1.0f);
    test_unit_scalar(4 /* ms */, 1.0f / 0.001f, 0.001f);
    test_unit_scalar(5 /* us */, 1.0f / 0.000001f, 0.000001f);
    test_unit_scalar(USER_UNIT_ADAPTIVE, 1.0f, 1.0f);
  }

  BKE_fcurve_free(fcurve);
}

}  // namespace blender::animrig::tests
