/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_action.hh"
#include "ANIM_keyframing.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_nla.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "CLG_log.h"
#include "testing/testing.h"

namespace blender::animrig::tests {
class KeyframingTest : public testing::Test {
 public:
  Main *bmain;

  /* For standard single-action testing. */
  Object *object;
  PointerRNA object_rna_pointer;

  /* For pose bone single-action testing. */
  Object *armature_object;
  bArmature *armature;
  PointerRNA armature_object_rna_pointer;

  /* For NLA testing. */
  Object *object_with_nla;
  PointerRNA object_with_nla_rna_pointer;
  bAction *nla_action;

  /* For action reuse testing. */
  Object *cube;
  PointerRNA cube_rna_pointer;
  Mesh *cube_mesh;
  PointerRNA cube_mesh_rna_pointer;
  Material *material;
  PointerRNA material_rna_pointer;

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

    object = BKE_object_add_only_object(bmain, OB_EMPTY, "Empty");
    object_rna_pointer = RNA_id_pointer_create(&object->id);

    Bone *bone = static_cast<Bone *>(MEM_mallocN(sizeof(Bone), "BONE"));
    memset(bone, 0, sizeof(Bone));
    STRNCPY(bone->name, "Bone");

    armature = BKE_armature_add(bmain, "Armature");
    BLI_addtail(&armature->bonebase, bone);

    armature_object = BKE_object_add_only_object(bmain, OB_ARMATURE, "Armature");
    armature_object->data = armature;
    BKE_pose_ensure(bmain, armature_object, armature, false);
    armature_object_rna_pointer = RNA_id_pointer_create(&armature_object->id);

    object_with_nla = BKE_object_add_only_object(bmain, OB_EMPTY, "EmptyWithNLA");
    object_with_nla_rna_pointer = RNA_id_pointer_create(&object_with_nla->id);
    nla_action = static_cast<bAction *>(BKE_id_new(bmain, ID_AC, "NLAAction"));
    this->ensure_action_is_legacy(*nla_action);

    cube = BKE_object_add_only_object(bmain, OB_MESH, "cube");
    cube_rna_pointer = RNA_id_pointer_create(&cube->id);
    cube_mesh = BKE_mesh_add(bmain, "cube_mesh");
    cube_mesh_rna_pointer = RNA_id_pointer_create(&cube_mesh->id);
    /* Removing the implicit id user. Using BKE_mesh_assign_object increments the user count which
     * would leave it at 2 otherwise. */
    id_us_min(&cube_mesh->id);
    BKE_mesh_assign_object(bmain, cube, cube_mesh);
    material = BKE_material_add(bmain, "material");
    material_rna_pointer = RNA_id_pointer_create(&material->id);

    id_us_min(&material->id);
    BKE_object_material_assign(bmain, cube, material, 0, BKE_MAT_ASSIGN_OBDATA);

    /* Set up an NLA system with a single NLA track with a single offset-in-time
     * NLA strip, and make that strip active and in tweak mode. */
    AnimData *adt = BKE_animdata_ensure_id(&object_with_nla->id);
    NlaTrack *track = BKE_nlatrack_new_head(&adt->nla_tracks, false);
    ASSERT_TRUE(animrig::assign_action(nla_action, object_with_nla->id));
    NlaStrip *strip = BKE_nlastack_add_strip({object_with_nla->id, *adt}, false);
    track->flag |= NLATRACK_ACTIVE;
    strip->flag |= NLASTRIP_FLAG_ACTIVE;
    strip->start = -10.0;
    strip->end = 990.0;
    strip->actstart = 0.0;
    strip->actend = 1000.0;
    strip->scale = 1.0;
    strip->blendmode = NLASTRIP_MODE_COMBINE;
    BKE_nla_tweakmode_enter({object_with_nla->id, *adt});
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }

  /**
   * Create a legacy Action and assign it to the ID.
   *
   * Use of this function indicates that the unit test should be converted to
   * use layered Actions.
   */
  void ensure_legacy_action_assigned(ID &id)
  {
    AnimData *adt = BKE_animdata_from_id(&id);
    BLI_assert(!adt || !adt->action);
    UNUSED_VARS_NDEBUG(adt);

    bAction &action = animrig::action_add(*bmain, "LegacyAction");
    this->ensure_action_is_legacy(action);

    const bool ok = assign_action(&action, id);
    BLI_assert(ok);
    UNUSED_VARS_NDEBUG(ok);
  }

  /**
   * Add an F-Curve Group. This marks the Action as legacy. Since most tests are on F-Curves,
   * adding a Group here is the less invasive way to make this a Legacy Action.
   *
   * Use of this function indicates the test should be converted to layered Actions.
   */
  void ensure_action_is_legacy(bAction &action)
  {
    bActionGroup *new_group = static_cast<bActionGroup *>(
        MEM_callocN(sizeof(bActionGroup), __func__));
    STRNCPY(new_group->name, "Legacy Forcer");
    BLI_addtail(&action.groups, new_group);
  }
};

/* ------------------------------------------------------------
 * Tests for `insert_keyframes()` with layered actions.
 */

/* Keying a non-array property. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__non_array_property)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First time should create:
   * - AnimData
   * - Action
   * - Slot
   * - Layer
   * - Infinite KeyframeStrip
   * - FCurve with a single key
   */
  object->empty_drawsize = 42.0;
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"empty_display_size"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_1.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  Action &action = object->adt->action->wrap();

  /* The action has a slot, it's named properly, and it's correctly assigned
   * to the object. */
  ASSERT_EQ(1, action.slots().size());
  Slot *slot = action.slot(0);
  EXPECT_STREQ(object->id.name, slot->identifier);
  EXPECT_STREQ(object->adt->last_slot_identifier, slot->identifier);
  EXPECT_EQ(object->adt->slot_handle, slot->handle);

  /* We have the default layer and strip. */
  ASSERT_TRUE(action.is_action_layered());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  EXPECT_TRUE(strlen(action.layer(0)->name) > 0);
  Strip *strip = action.layer(0)->strip(0);
  ASSERT_TRUE(strip->is_infinite());
  ASSERT_EQ(Strip::Type::Keyframe, strip->type());
  StripKeyframeData *strip_data = &strip->data<StripKeyframeData>(action);

  /* We have a channel bag for the slot. */
  Channelbag *channelbag = strip_data->channelbag_for_slot(*slot);
  ASSERT_NE(nullptr, channelbag);

  /* The fcurves in the channel bag are what we expect. */
  EXPECT_EQ(1, channelbag->fcurves().size());
  const FCurve *fcurve = channelbag->fcurve_find({"empty_display_size", 0});
  ASSERT_NE(nullptr, fcurve);
  ASSERT_NE(nullptr, fcurve->bezt);
  EXPECT_EQ(1, fcurve->totvert);
  EXPECT_EQ(1.0, fcurve->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve->bezt[0].vec[1][1]);

  /* Second time inserting with a different value on the same frame should
   * simply replace the key. */
  object->empty_drawsize = 86.0;
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"empty_display_size"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_2.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(1, fcurve->totvert);
  EXPECT_EQ(1.0, fcurve->bezt[0].vec[1][0]);
  EXPECT_EQ(86.0, fcurve->bezt[0].vec[1][1]);

  /* Third time inserting on a different time should add a second key. */
  object->empty_drawsize = 7.0;
  const CombinedKeyingResult result_3 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"empty_display_size"}},
                                                         10.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_3.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(2, fcurve->totvert);
  EXPECT_EQ(1.0, fcurve->bezt[0].vec[1][0]);
  EXPECT_EQ(86.0, fcurve->bezt[0].vec[1][1]);
  EXPECT_EQ(10.0, fcurve->bezt[1].vec[1][0]);
  EXPECT_EQ(7.0, fcurve->bezt[1].vec[1][1]);
}

TEST_F(KeyframingTest, insert_keyframes__layered_action__action_reuse)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};
  CombinedKeyingResult result_ob;
  result_ob = insert_keyframes(bmain,
                               &armature_object_rna_pointer,
                               std::nullopt,
                               {{"location"}},
                               10.0,
                               anim_eval_context,
                               BEZT_KEYTYPE_KEYFRAME,
                               INSERTKEY_NOFLAGS);

  ASSERT_EQ(result_ob.get_count(SingleKeyingResult::SUCCESS), 3);
  ASSERT_TRUE(armature_object->adt != nullptr);
  ASSERT_TRUE(armature_object->adt->action != nullptr);

  PointerRNA armature_rna_pointer = RNA_id_pointer_create(&armature->id);

  result_ob = insert_keyframes(bmain,
                               &armature_rna_pointer,
                               std::nullopt,
                               {{"display_type"}},
                               10.0,
                               anim_eval_context,
                               BEZT_KEYTYPE_KEYFRAME,
                               INSERTKEY_NOFLAGS);
  ASSERT_EQ(result_ob.get_count(SingleKeyingResult::SUCCESS), 1);
  ASSERT_TRUE(armature->adt != nullptr);
  ASSERT_TRUE(armature->adt->action != nullptr);

  /* Action is expected to be reused between object and data. */
  ASSERT_EQ(armature->adt->action, armature_object->adt->action);

  Action &action = armature->adt->action->wrap();
  /* Should have two slots now. */
  ASSERT_EQ(action.slot_array_num, 2);
  for (Slot *slot : action.slots()) {
    ASSERT_TRUE(slot->idtype == ID_AR || slot->idtype == ID_OB);
  }
}

TEST_F(KeyframingTest, insert_keyframes__layered_action__action_reuse_material)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};
  CombinedKeyingResult result_ob;

  result_ob = insert_keyframes(bmain,
                               &material_rna_pointer,
                               std::nullopt,
                               {{"pass_index"}},
                               1.0,
                               anim_eval_context,
                               BEZT_KEYTYPE_KEYFRAME,
                               INSERTKEY_NOFLAGS);

  ASSERT_EQ(result_ob.get_count(SingleKeyingResult::SUCCESS), 1);
  ASSERT_TRUE(material->adt != nullptr);
  ASSERT_TRUE(material->adt->action != nullptr);

  result_ob = insert_keyframes(bmain,
                               &cube_rna_pointer,
                               std::nullopt,
                               {{"location"}},
                               1.0,
                               anim_eval_context,
                               BEZT_KEYTYPE_KEYFRAME,
                               INSERTKEY_NOFLAGS);

  ASSERT_EQ(result_ob.get_count(SingleKeyingResult::SUCCESS), 3);
  ASSERT_TRUE(cube->adt != nullptr);
  ASSERT_TRUE(cube->adt->action != nullptr);

  /* Actions are not shared between object and material. */
  ASSERT_NE(cube->adt->action, material->adt->action);

  result_ob = insert_keyframes(bmain,
                               &cube_mesh_rna_pointer,
                               std::nullopt,
                               {{"remesh_voxel_size"}},
                               1.0,
                               anim_eval_context,
                               BEZT_KEYTYPE_KEYFRAME,
                               INSERTKEY_NOFLAGS);

  ASSERT_EQ(result_ob.get_count(SingleKeyingResult::SUCCESS), 1);
  ASSERT_TRUE(cube_mesh->adt != nullptr);
  ASSERT_TRUE(cube_mesh->adt->action != nullptr);

  /* Reuse between Object and object data. */
  ASSERT_EQ(cube_mesh->adt->action, cube->adt->action);
  /* Still no reuse from mesh to material. */
  ASSERT_NE(cube_mesh->adt->action, material->adt->action);

  Action &action = cube->adt->action->wrap();
  /* Should have two slots now. */
  ASSERT_EQ(action.slot_array_num, 2);

  /* Material action should have only 1 slot. */
  ASSERT_EQ(material->adt->action->wrap().slot_array_num, 1);

  for (Slot *slot : action.slots()) {
    ASSERT_TRUE(slot->idtype == ID_ME || slot->idtype == ID_OB);
    ASSERT_NE(slot->idtype, ID_MA);
  }
}

TEST_F(KeyframingTest, insert_keyframes__layered_action__action_reuse_multiuser)
{
  Object *another_object = BKE_object_add_only_object(bmain, OB_MESH, "another_object");
  PointerRNA another_object_rna_pointer = RNA_id_pointer_create(&another_object->id);
  BKE_mesh_assign_object(bmain, another_object, cube_mesh);

  ASSERT_EQ(ID_REFCOUNTING_USERS(&cube_mesh->id), 2);

  AnimationEvalContext anim_eval_context = {nullptr, 1.0};
  CombinedKeyingResult result_ob;

  result_ob = insert_keyframes(bmain,
                               &cube_rna_pointer,
                               std::nullopt,
                               {{"location"}},
                               1.0,
                               anim_eval_context,
                               BEZT_KEYTYPE_KEYFRAME,
                               INSERTKEY_NOFLAGS);

  ASSERT_EQ(result_ob.get_count(SingleKeyingResult::SUCCESS), 3);
  ASSERT_TRUE(cube->adt != nullptr);
  ASSERT_TRUE(cube->adt->action != nullptr);

  result_ob = insert_keyframes(bmain,
                               &cube_mesh_rna_pointer,
                               std::nullopt,
                               {{"remesh_voxel_size"}},
                               1.0,
                               anim_eval_context,
                               BEZT_KEYTYPE_KEYFRAME,
                               INSERTKEY_NOFLAGS);

  ASSERT_EQ(result_ob.get_count(SingleKeyingResult::SUCCESS), 1);
  ASSERT_TRUE(cube_mesh->adt != nullptr);
  ASSERT_TRUE(cube_mesh->adt->action != nullptr);

  /* When an ID is used more than once, the action should not be reused. */
  ASSERT_NE(cube->adt->action, cube_mesh->adt->action);

  result_ob = insert_keyframes(bmain,
                               &another_object_rna_pointer,
                               std::nullopt,
                               {{"location"}},
                               1.0,
                               anim_eval_context,
                               BEZT_KEYTYPE_KEYFRAME,
                               INSERTKEY_NOFLAGS);

  ASSERT_EQ(result_ob.get_count(SingleKeyingResult::SUCCESS), 3);
  ASSERT_TRUE(another_object->adt != nullptr);
  ASSERT_TRUE(another_object->adt->action != nullptr);

  /* Given that those two objects are connected by a mesh (which due to this has two users) the
   * action shouldn't be reused between them. */
  ASSERT_NE(cube->adt->action, another_object->adt->action);
}

/* Keying a single element of an array property. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__single_element)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &object_rna_pointer,
                                                       std::nullopt,
                                                       {{"rotation_euler", std::nullopt, 0}},
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(1, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  Action &action = object->adt->action->wrap();
  ASSERT_EQ(1, action.slots().size());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag = strip_data->channelbag(0);

  EXPECT_EQ(1, channelbag->fcurves().size());
  EXPECT_NE(nullptr, channelbag->fcurve_find({"rotation_euler", 0}));
}

/* Keying all elements of an array property. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__all_elements)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &object_rna_pointer,
                                                       std::nullopt,
                                                       {{"rotation_euler"}},
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(3, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  Action &action = object->adt->action->wrap();
  ASSERT_EQ(1, action.slots().size());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag = strip_data->channelbag(0);

  EXPECT_EQ(3, channelbag->fcurves().size());
  EXPECT_NE(nullptr, channelbag->fcurve_find({"rotation_euler", 0}));
  EXPECT_NE(nullptr, channelbag->fcurve_find({"rotation_euler", 1}));
  EXPECT_NE(nullptr, channelbag->fcurve_find({"rotation_euler", 2}));
}

/* Keying a pose bone from its own RNA pointer. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__pose_bone_rna_pointer)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};
  bPoseChannel *pchan = BKE_pose_channel_find_name(armature_object->pose, "Bone");
  PointerRNA pose_bone_rna_pointer = RNA_pointer_create_discrete(
      &armature_object->id, &RNA_PoseBone, pchan);

  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &pose_bone_rna_pointer,
                                                       std::nullopt,
                                                       {{"rotation_euler", std::nullopt, 0}},
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(1, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, armature_object->adt);
  ASSERT_NE(nullptr, armature_object->adt->action);
  Action &action = armature_object->adt->action->wrap();
  ASSERT_EQ(1, action.slots().size());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag = strip_data->channelbag(0);

  EXPECT_EQ(1, channelbag->fcurves().size());
  EXPECT_NE(nullptr, channelbag->fcurve_find({"pose.bones[\"Bone\"].rotation_euler", 0}));
}

/* Keying a pose bone from its owning ID's RNA pointer. */
TEST_F(KeyframingTest, insert_keyframes__pose_bone_owner_id_pointer)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  const CombinedKeyingResult result = insert_keyframes(
      bmain,
      &armature_object_rna_pointer,
      std::nullopt,
      {{"pose.bones[\"Bone\"].rotation_euler", std::nullopt, 0}},
      1.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_NOFLAGS);

  EXPECT_EQ(1, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, armature_object->adt);
  ASSERT_NE(nullptr, armature_object->adt->action);
  Action &action = armature_object->adt->action->wrap();
  ASSERT_EQ(1, action.slots().size());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag = strip_data->channelbag(0);

  EXPECT_EQ(1, channelbag->fcurves().size());
  EXPECT_NE(nullptr, channelbag->fcurve_find({"pose.bones[\"Bone\"].rotation_euler", 0}));
}

/* Keying multiple elements of multiple properties at once. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__multiple_properties)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &object_rna_pointer,
                                                       std::nullopt,
                                                       {
                                                           {"empty_display_size"},
                                                           {"location"},
                                                           {"rotation_euler", std::nullopt, 0},
                                                           {"rotation_euler", std::nullopt, 2},
                                                       },
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(6, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  Action &action = object->adt->action->wrap();
  ASSERT_EQ(1, action.slots().size());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag = strip_data->channelbag(0);

  EXPECT_EQ(6, channelbag->fcurves().size());
  EXPECT_NE(nullptr, channelbag->fcurve_find({"empty_display_size", 0}));
  EXPECT_NE(nullptr, channelbag->fcurve_find({"location", 0}));
  EXPECT_NE(nullptr, channelbag->fcurve_find({"location", 1}));
  EXPECT_NE(nullptr, channelbag->fcurve_find({"location", 2}));
  EXPECT_NE(nullptr, channelbag->fcurve_find({"rotation_euler", 0}));
  EXPECT_NE(nullptr, channelbag->fcurve_find({"rotation_euler", 2}));
}

/* Keying more than one ID on the same action. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__multiple_ids)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First object should crate the action and get a slot and channel bag. */
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"empty_display_size"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_1.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  Action &action = object->adt->action->wrap();

  /* The action has a slot and it's assigned to the first object. */
  ASSERT_EQ(1, action.slots().size());
  Slot *slot_1 = action.slot_for_handle(object->adt->slot_handle);
  ASSERT_NE(nullptr, slot_1);
  EXPECT_STREQ(object->id.name, slot_1->identifier);
  EXPECT_STREQ(object->adt->last_slot_identifier, slot_1->identifier);

  /* Get the keyframe strip. */
  ASSERT_TRUE(action.is_action_layered());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);

  /* We have a single channel bag, and it's for the first object's slot. */
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag_1 = strip_data->channelbag_for_slot(*slot_1);
  ASSERT_NE(nullptr, channelbag_1);

  /* Assign the action to the second object, with no slot. */
  ASSERT_TRUE(assign_action(&action, armature_object->id));
  ASSERT_EQ(assign_action_slot(nullptr, armature_object->id), ActionSlotAssignmentResult::OK);

  /* Keying the second object should go into the same action, creating a new
   * slot and channel bag. */
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &armature_object_rna_pointer,
                                                         std::nullopt,
                                                         {{"empty_display_size"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_2.get_count(SingleKeyingResult::SUCCESS));

  ASSERT_EQ(2, action.slots().size());
  Slot *slot_2 = action.slot_for_handle(armature_object->adt->slot_handle);
  ASSERT_NE(nullptr, slot_2);
  EXPECT_STREQ(armature_object->id.name, slot_2->identifier);
  EXPECT_STREQ(armature_object->adt->last_slot_identifier, slot_2->identifier);

  ASSERT_EQ(2, strip_data->channelbags().size());
  Channelbag *channelbag_2 = strip_data->channelbag_for_slot(*slot_2);
  ASSERT_NE(nullptr, channelbag_2);
}

/* Keying an object with an already-existing legacy action should do legacy
 * keying. */
TEST_F(KeyframingTest, insert_keyframes__baklava_legacy_action)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* Create a legacy Action and assign it the legacy way. */
  {
    bAction *action = reinterpret_cast<bAction *>(BKE_id_new(bmain, ID_AC, "LegacyAction"));
    action_fcurve_ensure_legacy(bmain, action, nullptr, nullptr, {"testprop", 47});
    BKE_animdata_ensure_id(&object->id)->action = action;
  }

  bAction *action = object->adt->action;
  EXPECT_TRUE(action->wrap().is_action_legacy());
  EXPECT_FALSE(action->wrap().is_action_layered());
  EXPECT_EQ(1, BLI_listbase_count(&action->curves));

  /* Insert more keys, which should also get inserted as part of the same legacy
   * action, not a layered action. */
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"location"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(3, result_2.get_count(SingleKeyingResult::SUCCESS));

  EXPECT_EQ(action, object->adt->action);
  EXPECT_TRUE(action->wrap().is_action_legacy());
  EXPECT_FALSE(action->wrap().is_action_layered());
  EXPECT_EQ(4, BLI_listbase_count(&action->curves));
}

/* Keying with the "Only Insert Available" flag. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__only_available)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First attempt should fail, because there are no fcurves yet. */
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_AVAILABLE);

  EXPECT_EQ(0, result_1.get_count(SingleKeyingResult::SUCCESS));

  /* It's unclear why an AnimData should be created if keying fails
   * here. It may even be undesirable. This check is just here to ensure no
   * *unintentional* changes in behavior. */
  ASSERT_NE(nullptr, object->adt);
  /* No action is created when using the flag INSERTKEY_AVAILABLE on an
   * object without an action. */
  ASSERT_EQ(nullptr, object->adt->action);

  /* Insert a key on two of the elements without using the flag so that there
   * will be two fcurves. */
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {
                                                             {"rotation_euler", std::nullopt, 0},
                                                             {"rotation_euler", std::nullopt, 2},
                                                         },
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);

  /* If an action is created, it should be the default action with one
   * layer and an infinite keyframe strip. */
  Action &action = object->adt->action->wrap();
  ASSERT_EQ(1, action.slots().size());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  EXPECT_EQ(object->adt->slot_handle, action.slot(0)->handle);
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);

  EXPECT_EQ(2, result_2.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag = strip_data->channelbag(0);

  /* Second attempt should succeed with two keys, because two of the elements
   * now have fcurves. */
  const CombinedKeyingResult result_3 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_AVAILABLE);

  EXPECT_EQ(2, result_3.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(2, channelbag->fcurves().size());
  EXPECT_NE(nullptr, channelbag->fcurve_find({"rotation_euler", 0}));
  EXPECT_NE(nullptr, channelbag->fcurve_find({"rotation_euler", 2}));
}

/* Keying with the "Only Replace" flag. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__only_replace)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First attempt should fail, because there are no fcurves yet. */
  object->rot[0] = 42.0;
  object->rot[1] = 42.0;
  object->rot[2] = 42.0;
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_REPLACE);
  EXPECT_EQ(0, result_1.get_count(SingleKeyingResult::SUCCESS));

  /* Insert a key for two of the elements so that there will be two fcurves with
   * one key each. */
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {
                                                             {"rotation_euler", std::nullopt, 0},
                                                             {"rotation_euler", std::nullopt, 2},
                                                         },
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(2, result_2.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  Action &action = object->adt->action->wrap();
  ASSERT_EQ(1, action.slots().size());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag = strip_data->channelbag(0);

  ASSERT_EQ(2, channelbag->fcurves().size());
  const FCurve *fcurve_x = channelbag->fcurve_find({"rotation_euler", 0});
  const FCurve *fcurve_z = channelbag->fcurve_find({"rotation_euler", 2});
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);
  EXPECT_EQ(1.0, fcurve_x->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve_x->bezt[0].vec[1][1]);
  EXPECT_EQ(1.0, fcurve_z->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve_z->bezt[0].vec[1][1]);

  /* Second attempt should also fail, because we insert on a different frame
   * than the two keys we just created. */
  object->rot[0] = 86.0;
  object->rot[1] = 86.0;
  object->rot[2] = 86.0;
  const CombinedKeyingResult result_3 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         5.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_REPLACE);
  EXPECT_EQ(0, result_3.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(2, channelbag->fcurves().size());
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);
  EXPECT_EQ(1.0, fcurve_x->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve_x->bezt[0].vec[1][1]);
  EXPECT_EQ(1.0, fcurve_z->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve_z->bezt[0].vec[1][1]);

  /* The third attempt, keying on the original frame, should succeed and replace
   * the existing key on each fcurve. */
  const CombinedKeyingResult result_4 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_REPLACE);
  EXPECT_EQ(2, result_4.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(2, channelbag->fcurves().size());
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);
  EXPECT_EQ(1.0, fcurve_x->bezt[0].vec[1][0]);
  EXPECT_EQ(86.0, fcurve_x->bezt[0].vec[1][1]);
  EXPECT_EQ(1.0, fcurve_z->bezt[0].vec[1][0]);
  EXPECT_EQ(86.0, fcurve_z->bezt[0].vec[1][1]);
}

/* Keying with the "Only Insert Needed" flag. */
TEST_F(KeyframingTest, insert_keyframes__layered_action__only_needed)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First attempt should succeed, because there are no fcurves yet. */
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NEEDED);
  EXPECT_EQ(3, result_1.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  Action &action = object->adt->action->wrap();
  ASSERT_EQ(1, action.slots().size());
  ASSERT_EQ(1, action.layers().size());
  ASSERT_EQ(1, action.layer(0)->strips().size());
  StripKeyframeData *strip_data = &action.layer(0)->strip(0)->data<StripKeyframeData>(action);
  ASSERT_EQ(1, strip_data->channelbags().size());
  Channelbag *channelbag = strip_data->channelbag(0);

  ASSERT_EQ(3, channelbag->fcurves().size());
  const FCurve *fcurve_x = channelbag->fcurve_find({"rotation_euler", 0});
  const FCurve *fcurve_y = channelbag->fcurve_find({"rotation_euler", 1});
  const FCurve *fcurve_z = channelbag->fcurve_find({"rotation_euler", 2});
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_y->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);

  /* Second attempt should fail, because there is now an fcurve for the
   * property, but its value matches the current property value. */
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         10.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NEEDED);
  EXPECT_EQ(0, result_2.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(3, channelbag->fcurves().size());
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_y->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);

  /* Third attempt should succeed on two elements, because we change the value
   * of those elements to differ from the existing fcurves. */
  object->rot[0] = 123.0;
  object->rot[2] = 123.0;
  const CombinedKeyingResult result_3 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         10.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NEEDED);

  EXPECT_EQ(2, result_3.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(3, channelbag->fcurves().size());
  EXPECT_EQ(2, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_y->totvert);
  EXPECT_EQ(2, fcurve_z->totvert);
}

/* ------------------------------------------------------------
 * Tests for `insert_keyframes()` with legacy actions.
 */

/* Keying a non-array property. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__non_array_property)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First time should create the AnimData, Action, and FCurve with a single
   * key. */
  object->empty_drawsize = 42.0;
  ensure_legacy_action_assigned(object->id);
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"empty_display_size"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_1.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  EXPECT_EQ(1, BLI_listbase_count(&object->adt->action->curves));
  FCurve *fcurve = BKE_fcurve_find(&object->adt->action->curves, "empty_display_size", 0);
  ASSERT_NE(nullptr, fcurve);
  ASSERT_NE(nullptr, fcurve->bezt);
  EXPECT_EQ(1, fcurve->totvert);
  EXPECT_EQ(1.0, fcurve->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve->bezt[0].vec[1][1]);

  /* Second time inserting with a different value on the same frame should
   * simply replace the key. */
  object->empty_drawsize = 86.0;
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"empty_display_size"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_2.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(1, fcurve->totvert);
  EXPECT_EQ(1.0, fcurve->bezt[0].vec[1][0]);
  EXPECT_EQ(86.0, fcurve->bezt[0].vec[1][1]);

  /* Third time inserting on a different time should add a second key. */
  object->empty_drawsize = 7.0;
  const CombinedKeyingResult result_3 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"empty_display_size"}},
                                                         10.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_3.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(2, fcurve->totvert);
  EXPECT_EQ(1.0, fcurve->bezt[0].vec[1][0]);
  EXPECT_EQ(86.0, fcurve->bezt[0].vec[1][1]);
  EXPECT_EQ(10.0, fcurve->bezt[1].vec[1][0]);
  EXPECT_EQ(7.0, fcurve->bezt[1].vec[1][1]);
}

/* Passing the frame number explicitly vs not. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__optional_frame)
{
  /* If the frame number is not explicitly passed, the eval frame from the
   * animation evaluation context should be used. */
  AnimationEvalContext anim_eval_context = {nullptr, 5.0};

  object->rotmode = ROT_MODE_XYZ;
  ensure_legacy_action_assigned(object->id);
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_mode"}},
                                                         std::nullopt,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_1.get_count(SingleKeyingResult::SUCCESS));
  FCurve *fcurve = BKE_fcurve_find(&object->adt->action->curves, "rotation_mode", 0);
  EXPECT_EQ(5.0, fcurve->bezt[0].vec[1][0]);
  EXPECT_EQ(float(ROT_MODE_XYZ), fcurve->bezt[0].vec[1][1]);

  /* If the frame number *is* explicitly passed, it should be used. */
  object->rotmode = ROT_MODE_QUAT;
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_mode"}},
                                                         10.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(1, result_2.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(5.0, fcurve->bezt[0].vec[1][0]);
  EXPECT_EQ(float(ROT_MODE_XYZ), fcurve->bezt[0].vec[1][1]);
  EXPECT_EQ(10.0, fcurve->bezt[1].vec[1][0]);
  EXPECT_EQ(float(ROT_MODE_QUAT), fcurve->bezt[1].vec[1][1]);
}

/* Passing the channel group explicitly vs not. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__optional_channel_group)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* If the channel group is not explicitly passed, the default should be used. */
  ensure_legacy_action_assigned(object->id);
  const CombinedKeyingResult result_1 = insert_keyframes(
      bmain,
      &object_rna_pointer,
      std::nullopt,
      {{"location", std::nullopt, 0}, {"visible_shadow"}},
      1.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_NOFLAGS);
  EXPECT_EQ(2, result_1.get_count(SingleKeyingResult::SUCCESS));

  /* Location X should get the default transform group. */
  FCurve *fcurve_location_x = BKE_fcurve_find(&object->adt->action->curves, "location", 0);
  ASSERT_NE(nullptr, fcurve_location_x->grp);
  EXPECT_EQ(0, strcmp("Object Transforms", fcurve_location_x->grp->name));

  /* Shadow visibility should get no group. */
  FCurve *fcurve_visible_shadow = BKE_fcurve_find(
      &object->adt->action->curves, "visible_shadow", 0);
  ASSERT_EQ(nullptr, fcurve_visible_shadow->grp);

  /* If the channel group *is* explicitly passed, it should override the default. */
  const CombinedKeyingResult result_2 = insert_keyframes(
      bmain,
      &object_rna_pointer,
      "Foo",
      {{"location", std::nullopt, 1}, {"hide_render"}},
      1.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_NOFLAGS);
  EXPECT_EQ(2, result_2.get_count(SingleKeyingResult::SUCCESS));

  /* Both location Y and render visibility should get the "Foo" group. */
  FCurve *fcurve_location_y = BKE_fcurve_find(&object->adt->action->curves, "location", 1);
  ASSERT_NE(nullptr, fcurve_location_y->grp);
  EXPECT_EQ(0, strcmp("Foo", fcurve_location_y->grp->name));
  FCurve *fcurve_hide_render = BKE_fcurve_find(&object->adt->action->curves, "hide_render", 0);
  ASSERT_NE(nullptr, fcurve_hide_render->grp);
  EXPECT_EQ(0, strcmp("Foo", fcurve_hide_render->grp->name));
}

/* Keying a single element of an array property. */
TEST_F(KeyframingTest, insert_keyframes__single_element)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  ensure_legacy_action_assigned(object->id);
  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &object_rna_pointer,
                                                       std::nullopt,
                                                       {{"rotation_euler", std::nullopt, 0}},
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(1, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  EXPECT_EQ(1, BLI_listbase_count(&object->adt->action->curves));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 0));
}

/* Keying all elements of an array property. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__all_elements)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  ensure_legacy_action_assigned(object->id);
  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &object_rna_pointer,
                                                       std::nullopt,
                                                       {{"rotation_euler"}},
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(3, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  EXPECT_EQ(3, BLI_listbase_count(&object->adt->action->curves));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 0));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 1));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 2));
}

/* Keying a pose bone from its own RNA pointer. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__pose_bone_rna_pointer)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};
  bPoseChannel *pchan = BKE_pose_channel_find_name(armature_object->pose, "Bone");
  PointerRNA pose_bone_rna_pointer = RNA_pointer_create_discrete(
      &armature_object->id, &RNA_PoseBone, pchan);

  ensure_legacy_action_assigned(armature_object->id);
  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &pose_bone_rna_pointer,
                                                       std::nullopt,
                                                       {{"rotation_euler", std::nullopt, 0}},
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(1, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, armature_object->adt);
  ASSERT_NE(nullptr, armature_object->adt->action);
  EXPECT_EQ(1, BLI_listbase_count(&armature_object->adt->action->curves));
  EXPECT_NE(nullptr,
            BKE_fcurve_find(
                &armature_object->adt->action->curves, "pose.bones[\"Bone\"].rotation_euler", 0));
}

/* Keying a pose bone from its owning ID's RNA pointer. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__pose_bone_owner_id_pointer)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  ensure_legacy_action_assigned(armature_object->id);
  const CombinedKeyingResult result = insert_keyframes(
      bmain,
      &armature_object_rna_pointer,
      std::nullopt,
      {{"pose.bones[\"Bone\"].rotation_euler", std::nullopt, 0}},
      1.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_NOFLAGS);

  EXPECT_EQ(1, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, armature_object->adt);
  ASSERT_NE(nullptr, armature_object->adt->action);
  EXPECT_EQ(1, BLI_listbase_count(&armature_object->adt->action->curves));
  EXPECT_NE(nullptr,
            BKE_fcurve_find(
                &armature_object->adt->action->curves, "pose.bones[\"Bone\"].rotation_euler", 0));
}

/* Keying multiple elements of multiple properties at once. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__multiple_properties)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  ensure_legacy_action_assigned(object->id);
  const CombinedKeyingResult result =

      insert_keyframes(bmain,
                       &object_rna_pointer,
                       std::nullopt,
                       {
                           {"empty_display_size"},
                           {"location"},
                           {"rotation_euler", std::nullopt, 0},
                           {"rotation_euler", std::nullopt, 2},
                       },
                       1.0,
                       anim_eval_context,
                       BEZT_KEYTYPE_KEYFRAME,
                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(6, result.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  EXPECT_EQ(6, BLI_listbase_count(&object->adt->action->curves));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "empty_display_size", 0));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "location", 0));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "location", 1));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "location", 2));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 0));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 2));
}

/* Keying with the "Only Insert Available" flag. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__only_available)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First attempt should fail, because there are no fcurves yet. */
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_AVAILABLE);

  EXPECT_EQ(0, result_1.get_count(SingleKeyingResult::SUCCESS));

  /* It's unclear why an AnimData should be created if keying fails
   * here. It may even be undesirable. This check is just here to ensure no
   * *unintentional* changes in behavior. */
  ASSERT_NE(nullptr, object->adt);
  /* No action is created when using the flag INSERTKEY_AVAILABLE on an
   * object without an action. */
  ASSERT_EQ(nullptr, object->adt->action);

  /* This will create & assign an Action, which is necessary for the
   * insert_keyframes() function to take the 'legacy Action' code path. */
  ensure_legacy_action_assigned(object->id);

  /* Insert a key on two of the elements without using the flag so that there
   * will be two fcurves. */
  insert_keyframes(bmain,
                   &object_rna_pointer,
                   std::nullopt,
                   {
                       {"rotation_euler", std::nullopt, 0},
                       {"rotation_euler", std::nullopt, 2},
                   },
                   1.0,
                   anim_eval_context,
                   BEZT_KEYTYPE_KEYFRAME,
                   INSERTKEY_NOFLAGS);

  /* Second attempt should succeed with two keys, because two of the elements
   * now have fcurves. */
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_AVAILABLE);

  EXPECT_EQ(2, result_2.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(2, BLI_listbase_count(&object->adt->action->curves));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 0));
  EXPECT_NE(nullptr, BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 2));
}

/* Keying with the "Only Replace" flag. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__only_replace)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First attempt should fail, because there are no fcurves yet. */
  object->rot[0] = 42.0;
  object->rot[1] = 42.0;
  object->rot[2] = 42.0;
  ensure_legacy_action_assigned(object->id);
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_REPLACE);
  EXPECT_EQ(0, result_1.get_count(SingleKeyingResult::SUCCESS));

  /* Insert a key for two of the elements so that there will be two fcurves with
   * one key each. */
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {
                                                             {"rotation_euler", std::nullopt, 0},
                                                             {"rotation_euler", std::nullopt, 2},
                                                         },
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NOFLAGS);
  EXPECT_EQ(2, result_2.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  EXPECT_EQ(2, BLI_listbase_count(&object->adt->action->curves));
  FCurve *fcurve_x = BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 0);
  FCurve *fcurve_z = BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 2);
  ASSERT_NE(nullptr, fcurve_x);
  ASSERT_NE(nullptr, fcurve_z);
  ASSERT_NE(nullptr, fcurve_x->bezt);
  ASSERT_NE(nullptr, fcurve_z->bezt);
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);
  EXPECT_EQ(1.0, fcurve_x->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve_x->bezt[0].vec[1][1]);
  EXPECT_EQ(1.0, fcurve_z->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve_z->bezt[0].vec[1][1]);

  /* Second attempt should also fail, because we insert on a different frame
   * than the two keys we just created. */
  object->rot[0] = 86.0;
  object->rot[1] = 86.0;
  object->rot[2] = 86.0;
  const CombinedKeyingResult result_3 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         5.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_REPLACE);
  EXPECT_EQ(0, result_3.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(2, BLI_listbase_count(&object->adt->action->curves));
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);
  EXPECT_EQ(1.0, fcurve_x->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve_x->bezt[0].vec[1][1]);
  EXPECT_EQ(1.0, fcurve_z->bezt[0].vec[1][0]);
  EXPECT_EQ(42.0, fcurve_z->bezt[0].vec[1][1]);

  /* The third attempt, keying on the original frame, should succeed and replace
   * the existing key on each fcurve. */
  const CombinedKeyingResult result_4 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_REPLACE);
  EXPECT_EQ(2, result_4.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(2, BLI_listbase_count(&object->adt->action->curves));
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);
  EXPECT_EQ(1.0, fcurve_x->bezt[0].vec[1][0]);
  EXPECT_EQ(86.0, fcurve_x->bezt[0].vec[1][1]);
  EXPECT_EQ(1.0, fcurve_z->bezt[0].vec[1][0]);
  EXPECT_EQ(86.0, fcurve_z->bezt[0].vec[1][1]);
}

/* Keying with the "Only Insert Needed" flag. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__only_needed)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  /* First attempt should succeed, because there are no fcurves yet. */
  ensure_legacy_action_assigned(object->id);
  const CombinedKeyingResult result_1 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         1.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NEEDED);

  EXPECT_EQ(3, result_1.get_count(SingleKeyingResult::SUCCESS));
  ASSERT_NE(nullptr, object->adt);
  ASSERT_NE(nullptr, object->adt->action);
  EXPECT_EQ(3, BLI_listbase_count(&object->adt->action->curves));
  FCurve *fcurve_x = BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 0);
  FCurve *fcurve_y = BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 1);
  FCurve *fcurve_z = BKE_fcurve_find(&object->adt->action->curves, "rotation_euler", 2);
  ASSERT_NE(nullptr, fcurve_x);
  ASSERT_NE(nullptr, fcurve_y);
  ASSERT_NE(nullptr, fcurve_z);

  /* Second attempt should fail, because there is now an fcurve for the
   * property, but its value matches the current property value. */
  anim_eval_context.eval_time = 10.0;
  const CombinedKeyingResult result_2 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         10.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NEEDED);

  EXPECT_EQ(0, result_2.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(3, BLI_listbase_count(&object->adt->action->curves));
  EXPECT_EQ(1, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_y->totvert);
  EXPECT_EQ(1, fcurve_z->totvert);

  /* Third attempt should succeed on two elements, because we change the value
   * of those elements to differ from the existing fcurves. */
  object->rot[0] = 123.0;
  object->rot[2] = 123.0;
  const CombinedKeyingResult result_3 = insert_keyframes(bmain,
                                                         &object_rna_pointer,
                                                         std::nullopt,
                                                         {{"rotation_euler"}},
                                                         10.0,
                                                         anim_eval_context,
                                                         BEZT_KEYTYPE_KEYFRAME,
                                                         INSERTKEY_NEEDED);

  EXPECT_EQ(2, result_3.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(3, BLI_listbase_count(&object->adt->action->curves));
  EXPECT_EQ(2, fcurve_x->totvert);
  EXPECT_EQ(1, fcurve_y->totvert);
  EXPECT_EQ(2, fcurve_z->totvert);
}

/* Inserting a key into an NLA strip that has a time offset should remap the
 * key's time to the local time of the strip. */
TEST_F(KeyframingTest, insert_keyframes__nla_time_remapping)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};

  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &object_with_nla_rna_pointer,
                                                       std::nullopt,
                                                       {{"location", std::nullopt, 0}},
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(1, result.get_count(SingleKeyingResult::SUCCESS));
  EXPECT_EQ(1, BLI_listbase_count(&nla_action->curves));
  FCurve *fcurve = BKE_fcurve_find(&nla_action->curves, "location", 0);
  ASSERT_NE(nullptr, fcurve);
  ASSERT_NE(nullptr, fcurve->bezt);
  EXPECT_EQ(1, fcurve->totvert);
  EXPECT_EQ(11.0, fcurve->bezt[0].vec[1][0]);
}

/* ------------------------------------------------------------
 * Testing a special case of the NLA system:
 * When keying a strip with the "replace" or "combine" mix mode, keying a single
 * quaternion element should be treated as keying all quaternion elements in an
 * all-or-nothing fashion.
 */

/* With no special keyframing flags *all* quaternion elements should get keyed
 * if any of them are. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__quaternion_on_nla)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};
  object_with_nla->rotmode = ROT_MODE_QUAT;

  const CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &object_with_nla_rna_pointer,
                                                       std::nullopt,
                                                       {{"rotation_quaternion", std::nullopt, 0}},
                                                       1.0,
                                                       anim_eval_context,
                                                       BEZT_KEYTYPE_KEYFRAME,
                                                       INSERTKEY_NOFLAGS);

  EXPECT_EQ(4, result.get_count(SingleKeyingResult::SUCCESS));
}

/* With the "Only Insert Available" flag enabled, keys for all four elements
 * should be inserted if *any* of the elements have fcurves already, and
 * otherwise none of the elements should be keyed. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__quaternion_on_nla__only_available)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};
  object_with_nla->rotmode = ROT_MODE_QUAT;

  /* There are no fcurves at all yet, so all elements getting no keys is what
   * should happen. */
  const CombinedKeyingResult result_1 = insert_keyframes(
      bmain,
      &object_with_nla_rna_pointer,
      std::nullopt,
      {{"rotation_quaternion", std::nullopt, 0}},
      1.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_AVAILABLE);
  EXPECT_EQ(0, result_1.get_count(SingleKeyingResult::SUCCESS));

  /* Create an fcurve and key for a single quaternion channel. */
  PointerRNA id_rna_ptr = RNA_id_pointer_create(&object_with_nla->id);
  FCurve *fcu = action_fcurve_ensure_legacy(
      bmain, nla_action, nullptr, &id_rna_ptr, {"rotation_quaternion", 0});
  const KeyframeSettings keyframe_settings = {BEZT_KEYTYPE_KEYFRAME, HD_AUTO_ANIM, BEZT_IPO_BEZ};
  insert_vert_fcurve(fcu, {1.0, 1.0}, keyframe_settings, INSERTKEY_NOFLAGS);

  /* Now that there is one fcurve, all elements should get keyed. */
  const CombinedKeyingResult result_2 = insert_keyframes(
      bmain,
      &object_with_nla_rna_pointer,
      std::nullopt,
      {{"rotation_quaternion", std::nullopt, 0}},
      1.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_AVAILABLE);
  EXPECT_EQ(4, result_2.get_count(SingleKeyingResult::SUCCESS));
}

/* With the "Only Replace" flag enabled, keys for all four elements should be
 * inserted if *any* of them replace an existing key, and otherwise none of them
 * should. */
TEST_F(KeyframingTest, insert_keyframes__legacy_action__quaternion_on_nla__only_replace)
{
  AnimationEvalContext anim_eval_context = {nullptr, 1.0};
  object_with_nla->rotmode = ROT_MODE_QUAT;

  /* First time should insert nothing, since there are no fcurves yet. */
  const CombinedKeyingResult result_1 = insert_keyframes(
      bmain,
      &object_with_nla_rna_pointer,
      std::nullopt,
      {{"rotation_quaternion", std::nullopt, 0}},
      1.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_REPLACE);
  EXPECT_EQ(0, result_1.get_count(SingleKeyingResult::SUCCESS));

  /* Directly create an fcurve and key for a single quaternion channel. */
  PointerRNA id_rna_ptr = RNA_id_pointer_create(&object_with_nla->id);
  FCurve *fcu = action_fcurve_ensure_legacy(
      bmain, nla_action, nullptr, &id_rna_ptr, {"rotation_quaternion", 0});
  const KeyframeSettings keyframe_settings = {BEZT_KEYTYPE_KEYFRAME, HD_AUTO_ANIM, BEZT_IPO_BEZ};
  insert_vert_fcurve(fcu, {11.0, 1.0}, keyframe_settings, INSERTKEY_NOFLAGS);

  /* Second time should also insert nothing, since we attempt to insert on a
   * frame other than the one with the key. */
  const CombinedKeyingResult result_2 = insert_keyframes(
      bmain,
      &object_with_nla_rna_pointer,
      std::nullopt,
      {{"rotation_quaternion", std::nullopt, 0}},
      5.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_REPLACE);
  EXPECT_EQ(0, result_2.get_count(SingleKeyingResult::SUCCESS));

  /* Third time should succeed and key all elements, since we're inserting on a
   * frame where one of the elements already has a key.
   * NOTE: because of NLA time remapping, this 1.0 is the same as the 11.0 we
   * used above when inserting directly into the fcurve. */
  const CombinedKeyingResult result_3 = insert_keyframes(
      bmain,
      &object_with_nla_rna_pointer,
      std::nullopt,
      {{"rotation_quaternion", std::nullopt, 0}},
      1.0,
      anim_eval_context,
      BEZT_KEYTYPE_KEYFRAME,
      INSERTKEY_REPLACE);
  EXPECT_EQ(4, result_3.get_count(SingleKeyingResult::SUCCESS));
}

}  // namespace blender::animrig::tests
