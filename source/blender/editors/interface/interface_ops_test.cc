/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "ED_keyframing.hh"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"

#include "interface_intern.hh"

#include "CLG_log.h"
#include "testing/testing.h"

namespace blender::interface::tests {

class CopyDriversToSelected : public testing::Test {
 public:
  Main *bmain;

  Object *cube;
  Object *suzanne;

  PointerRNA cube_ptr;
  PropertyRNA *cube_quaternion_prop;
  PropertyRNA *cube_rotation_mode_prop;

  PointerRNA suzanne_ptr;
  PropertyRNA *suzanne_quaternion_prop;
  PropertyRNA *suzanne_rotation_mode_prop;

  static void SetUpTestSuite()
  {
    /* BKE_id_free() hits a code path that uses CLOG, which crashes if not initialized properly. */
    CLG_init();

    /* To make id_can_have_animdata() and friends work, the `id_types` array needs to be set up. */
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    bmain = BKE_main_new();

    cube = BKE_object_add_only_object(bmain, OB_EMPTY, "OBCube");
    suzanne = BKE_object_add_only_object(bmain, OB_EMPTY, "OBSuzanne");

    cube_ptr = RNA_pointer_create(&cube->id, &RNA_Object, &cube->id);
    cube_quaternion_prop = RNA_struct_find_property(&cube_ptr, "rotation_quaternion");
    cube_rotation_mode_prop = RNA_struct_find_property(&cube_ptr, "rotation_mode");

    suzanne_ptr = RNA_pointer_create(&suzanne->id, &RNA_Object, &suzanne->id);
    suzanne_quaternion_prop = RNA_struct_find_property(&suzanne_ptr, "rotation_quaternion");
    suzanne_rotation_mode_prop = RNA_struct_find_property(&suzanne_ptr, "rotation_mode");

    AnimData *adt_cube = BKE_animdata_ensure_id(&cube->id);
    AnimData *adt_suzanne = BKE_animdata_ensure_id(&suzanne->id);

    ReportList tmp_report_list;

    /* Set up cube drivers. */
    ANIM_add_driver(&tmp_report_list, &cube->id, "rotation_quaternion", 0, 0, DRIVER_TYPE_PYTHON);
    ANIM_add_driver(&tmp_report_list, &cube->id, "rotation_quaternion", 1, 0, DRIVER_TYPE_PYTHON);
    FCurve *cube_quat_0_driver = static_cast<FCurve *>(BLI_findlink(&adt_cube->drivers, 0));
    FCurve *cube_quat_1_driver = static_cast<FCurve *>(BLI_findlink(&adt_cube->drivers, 1));
    STRNCPY(cube_quat_0_driver->driver->expression, "0.0");
    STRNCPY(cube_quat_1_driver->driver->expression, "1.0");

    /* Set up suzanne drivers. */
    ANIM_add_driver(
        &tmp_report_list, &suzanne->id, "rotation_quaternion", 0, 0, DRIVER_TYPE_PYTHON);
    ANIM_add_driver(
        &tmp_report_list, &suzanne->id, "rotation_quaternion", 2, 0, DRIVER_TYPE_PYTHON);
    ANIM_add_driver(
        &tmp_report_list, &suzanne->id, "rotation_quaternion", 3, 0, DRIVER_TYPE_PYTHON);
    ANIM_add_driver(&tmp_report_list, &suzanne->id, "rotation_mode", 0, 0, DRIVER_TYPE_PYTHON);
    FCurve *suzanne_quat_0_driver = static_cast<FCurve *>(BLI_findlink(&adt_suzanne->drivers, 0));
    FCurve *suzanne_quat_2_driver = static_cast<FCurve *>(BLI_findlink(&adt_suzanne->drivers, 1));
    FCurve *suzanne_quat_3_driver = static_cast<FCurve *>(BLI_findlink(&adt_suzanne->drivers, 2));
    FCurve *suzanne_rotation_mode_driver = static_cast<FCurve *>(
        BLI_findlink(&adt_suzanne->drivers, 3));
    STRNCPY(suzanne_quat_0_driver->driver->expression, "0.5");
    STRNCPY(suzanne_quat_2_driver->driver->expression, "2.5");
    STRNCPY(suzanne_quat_3_driver->driver->expression, "3.5");
    STRNCPY(suzanne_rotation_mode_driver->driver->expression, "4");

    /* Add animation to cube's fourth quaternion element. */
    PointerRNA cube_ptr = RNA_pointer_create(&cube->id, &RNA_Object, &cube->id);
    bAction *act = animrig::id_action_ensure(bmain, &cube->id);
    FCurve *fcu = animrig::action_fcurve_ensure(
        bmain, act, "Object Transforms", &cube_ptr, "rotation_quaternion", 3);
    animrig::KeyframeSettings keyframe_settings = {BEZT_KEYTYPE_KEYFRAME, HD_AUTO, BEZT_IPO_BEZ};
    insert_vert_fcurve(fcu, {1.0, 1.0}, keyframe_settings, INSERTKEY_NOFLAGS);
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(CopyDriversToSelected, get_property_drivers)
{
  /* Cube quaternion: get all drivers. */
  {
    bool is_array_prop;
    Vector<FCurve *> drivers = internal::get_property_drivers(
        &cube_ptr, cube_quaternion_prop, true, -1, &is_array_prop);

    EXPECT_EQ(is_array_prop, true);
    EXPECT_EQ(drivers.size(), 4);

    EXPECT_NE(drivers[0], nullptr);
    EXPECT_NE(drivers[1], nullptr);
    EXPECT_EQ(drivers[2], nullptr);
    EXPECT_EQ(drivers[3], nullptr);

    EXPECT_STREQ(drivers[0]->driver->expression, "0.0");
    EXPECT_STREQ(drivers[1]->driver->expression, "1.0");
  }

  /* Cube quaternion: get first element driver. */
  {
    bool is_array_prop;
    Vector<FCurve *> drivers = internal::get_property_drivers(
        &cube_ptr, cube_quaternion_prop, false, 0, &is_array_prop);

    EXPECT_EQ(is_array_prop, true);
    EXPECT_EQ(drivers.size(), 4);

    EXPECT_NE(drivers[0], nullptr);
    EXPECT_EQ(drivers[1], nullptr);
    EXPECT_EQ(drivers[2], nullptr);
    EXPECT_EQ(drivers[3], nullptr);

    EXPECT_STREQ(drivers[0]->driver->expression, "0.0");
  }

  /* Cube quaternion: try to get fourth element driver. Since there is none, we
   * should get back an empty vector, indicating that no drivers were found. */
  {
    bool is_array_prop;
    Vector<FCurve *> drivers = internal::get_property_drivers(
        &cube_ptr, cube_quaternion_prop, false, 3, &is_array_prop);

    EXPECT_EQ(drivers.size(), 0);
  }

  /* Cube rotation mode: get driver. Since there is none, we should get back an
   * empty vector, indicating that no drivers were found. */
  {
    bool is_array_prop;
    Vector<FCurve *> drivers = internal::get_property_drivers(
        &cube_ptr, cube_rotation_mode_prop, false, 0, &is_array_prop);

    EXPECT_EQ(drivers.size(), 0);
  }

  /* Suzanne quaternion: get all drivers. */
  {
    bool is_array_prop;
    Vector<FCurve *> drivers = internal::get_property_drivers(
        &suzanne_ptr, suzanne_quaternion_prop, true, -1, &is_array_prop);

    EXPECT_EQ(is_array_prop, true);
    EXPECT_EQ(drivers.size(), 4);

    EXPECT_NE(drivers[0], nullptr);
    EXPECT_EQ(drivers[1], nullptr);
    EXPECT_NE(drivers[2], nullptr);
    EXPECT_NE(drivers[3], nullptr);

    EXPECT_STREQ(drivers[0]->driver->expression, "0.5");
    EXPECT_STREQ(drivers[2]->driver->expression, "2.5");
    EXPECT_STREQ(drivers[3]->driver->expression, "3.5");
  }

  /* Suzanne quaternion: get first element driver. */
  {
    bool is_array_prop;
    Vector<FCurve *> drivers = internal::get_property_drivers(
        &suzanne_ptr, suzanne_quaternion_prop, false, 0, &is_array_prop);

    EXPECT_EQ(is_array_prop, true);
    EXPECT_EQ(drivers.size(), 4);

    EXPECT_NE(drivers[0], nullptr);
    EXPECT_EQ(drivers[1], nullptr);
    EXPECT_EQ(drivers[2], nullptr);
    EXPECT_EQ(drivers[3], nullptr);

    EXPECT_STREQ(drivers[0]->driver->expression, "0.5");
  }

  /* Suzanne quaternion: get second element driver. Since there is none, we
   * should get back an empty vector, indicating that no drivers were found. */
  {
    bool is_array_prop;
    Vector<FCurve *> drivers = internal::get_property_drivers(
        &suzanne_ptr, suzanne_quaternion_prop, false, 1, &is_array_prop);

    EXPECT_EQ(drivers.size(), 0);
  }

  /* Suzanne rotation mode: get driver. */
  {
    bool is_array_prop;
    Vector<FCurve *> drivers = internal::get_property_drivers(
        &suzanne_ptr, suzanne_rotation_mode_prop, false, 0, &is_array_prop);

    EXPECT_EQ(is_array_prop, false);
    EXPECT_EQ(drivers.size(), 1);
    EXPECT_STREQ(drivers[0]->driver->expression, "4");
  }
}

TEST_F(CopyDriversToSelected, paste_property_drivers)
{
  /* Copy all quaternion channel drivers from Suzanne to Cube. The result on
   * Cube should be the following:
   *
   * - [0]: overwritten by the driver from Suzanne.
   * - [1]: Cube's driver remains, since there was no driver here on Suzanne.
   * - [2]: Suzanne's driver is pasted.  There was no driver on Cube before.
   * - [3]: remains without a driver.  Cube has animation on this channel,
   *   preventing driver pasting.
   */
  {
    bool is_array_prop;
    Vector<FCurve *> suzanne_location_drivers = internal::get_property_drivers(
        &suzanne_ptr, suzanne_quaternion_prop, true, -1, &is_array_prop);

    internal::paste_property_drivers(
        suzanne_location_drivers.as_span(), is_array_prop, &cube_ptr, cube_quaternion_prop);

    Vector<FCurve *> cube_location_drivers = internal::get_property_drivers(
        &cube_ptr, cube_quaternion_prop, true, -1, &is_array_prop);

    EXPECT_NE(cube_location_drivers[0], nullptr);
    EXPECT_NE(cube_location_drivers[1], nullptr);
    EXPECT_NE(cube_location_drivers[2], nullptr);
    EXPECT_EQ(cube_location_drivers[3], nullptr);

    EXPECT_STREQ(cube_location_drivers[0]->driver->expression, "0.5");
    EXPECT_STREQ(cube_location_drivers[1]->driver->expression, "1.0");
    EXPECT_STREQ(cube_location_drivers[2]->driver->expression, "2.5");
  }

  /* Copy the rotation_mode driver from Suzanne to Cube. */
  {
    bool is_array_prop;
    Vector<FCurve *> suzanne_rotation_mode_driver = internal::get_property_drivers(
        &suzanne_ptr, suzanne_rotation_mode_prop, false, 0, &is_array_prop);

    internal::paste_property_drivers(
        suzanne_rotation_mode_driver.as_span(), is_array_prop, &cube_ptr, cube_rotation_mode_prop);

    Vector<FCurve *> cube_rotation_mode_drivers = internal::get_property_drivers(
        &cube_ptr, cube_rotation_mode_prop, false, 0, &is_array_prop);

    EXPECT_NE(cube_rotation_mode_drivers[0], nullptr);
    EXPECT_STREQ(cube_rotation_mode_drivers[0]->driver->expression, "4");
  }
}

}  // namespace blender::interface::tests
