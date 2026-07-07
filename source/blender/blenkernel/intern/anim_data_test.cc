/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_action.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "ANIM_action.hh"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"

#include "BLI_vector.hh"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(anim_data, BKE_fcurves_id_cb_test)
{
  /* BKE_id_free() hits a code path that uses CLOG, which crashes if not initialized properly. */
  CLG_init();
  /* To make id_can_have_animdata() and friends work, the `id_types` array needs to be set up. */
  BKE_idtype_init();

  Main *bmain = BKE_main_new();
  bAction *action = BKE_id_new<bAction>(bmain, "ACÄnimåtië");
  Object *cube = BKE_object_add_only_object(bmain, OB_EMPTY, "Küüübus");
  Object *suzanne = BKE_object_add_only_object(bmain, OB_EMPTY, "OBSuzanne");

  /* Create F-Curves for Cube. */
  ASSERT_TRUE(animrig::assign_action(action, cube->id));
  FCurve &fcurve_cube_loc1 = animrig::action_fcurve_ensure(
      bmain, *action, cube->id, {"location", 1});
  FCurve &fcurve_cube_scale2 = animrig::action_fcurve_ensure(
      bmain, *action, cube->id, {"scale", 2});

  /* Create F-Curves for Suzanne. */
  ASSERT_TRUE(animrig::assign_action(action, suzanne->id));
  FCurve &fcurve_suzanne_loc0 = animrig::action_fcurve_ensure(
      bmain, *action, suzanne->id, {"location", 0});
  FCurve &fcurve_suzanne_scale1 = animrig::action_fcurve_ensure(
      bmain, *action, suzanne->id, {"scale", 1});

  /* Test that BKE_fcurves_id_cb only reports F-Curves that are meant for that ID. */
  Set<ID *> reported_ids;
  Vector<FCurve *> reported_fcurves;
  const auto callback = [&](ID *id, FCurve *fcurve) {
    reported_ids.add(id);
    reported_fcurves.append(fcurve);
  };

  BKE_fcurves_id_cb(&cube->id, callback);
  EXPECT_EQ(1, reported_ids.size());
  EXPECT_EQ(2, reported_fcurves.size());
  EXPECT_EQ(&cube->id, *reported_ids.begin());
  EXPECT_EQ(&fcurve_cube_loc1, reported_fcurves[0]);
  EXPECT_EQ(&fcurve_cube_scale2, reported_fcurves[1]);

  /* Also test for Suzanne, as that's a different slot. The first slot could
   * have been covered by legacy-compatible code. */
  reported_ids.clear();
  reported_fcurves.clear();

  BKE_fcurves_id_cb(&suzanne->id, callback);
  EXPECT_EQ(1, reported_ids.size());
  EXPECT_EQ(2, reported_fcurves.size());
  EXPECT_EQ(&suzanne->id, *reported_ids.begin());
  EXPECT_EQ(&fcurve_suzanne_loc0, reported_fcurves[0]);
  EXPECT_EQ(&fcurve_suzanne_scale1, reported_fcurves[1]);

  BKE_main_free(bmain);
  CLG_exit();
}

}  // namespace blender::bke::tests
