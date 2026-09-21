/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_listbase.hh"
#include "BLI_math_matrix_c.hh"
#include "BLI_math_rotation_c.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "BKE_action.hh"
#include "BKE_appdir.hh"
#include "BKE_armature.hh"
#include "BKE_blender_copybuffer.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_anim_transformable.hh"
#include "ED_screen.hh"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"
#include "ANIM_rna.hh"

#include "anim_intern.hh"

namespace blender::ed::animrig {

constexpr const char *clipboard_name = "world_space_buffer.blend";

/* Maps the name of the entity as stored in the clipboard to the 12 FCurves that make the world
 * space matrix. */
using ClipboardData = Map<StringRefNull, Array<FCurve *>>;
/* For each transformable, maps to an entity name on the clipboard to read FCurves from. */
using PasteMap = Map<AnimTransformable *, StringRefNull>;

/* Stores which other AnimTransformables have to be applied before it. */
struct TransformableRelations {
  AnimTransformable *transformable = nullptr;
  /* Other transformable_relations that need to be applied before this. */
  Vector<TransformableRelations *> ancestors = {};
  /* Indicates that this data has been processed. */
  bool done = false;

  bool all_ancestors_done()
  {
    for (TransformableRelations *ancestor : ancestors) {
      if (!ancestor->done) {
        /* All ancestors must be applied before this transformable. */
        return false;
      }
    }
    return true;
  }
};

/* Uniquely Identifies a component of the depsgraph. */
struct DegComponentIdentifier {
  ID *id = nullptr;
  StringRef name;
  eDepsObjectComponentType type;

  bool operator==(const DegComponentIdentifier &other) const
  {
    return id == other.id && type == other.type && name == other.name;
  }

  uint64_t hash() const
  {
    return get_default_hash(id, type, name);
  }
};

struct PasteFCurve {
  FCurve *fcurve = nullptr;
  /* Store info if that FCurve was created by this code. If yes, we can potentially remove it if
   * the keys are all on the same value after pasting. */
  bool created_on_paste = false;
  /* The index into the bezt array from which to start pasting. */
  int paste_start_index = 0;
};

struct TransformFCurves {
  /* The channelbag that houses those FCurves. */
  blender::animrig::Channelbag *channelbag;
  eRotationModes rotation_mode;
  Array<PasteFCurve, 3> location;
  Array<PasteFCurve, 4> rotation;
  Array<PasteFCurve, 3> scale;

  TransformFCurves()
  {
    location.reinitialize(3);
    rotation.reinitialize(4);
    scale.reinitialize(3);
  }
};

enum class AnimationPasteOffset {
  /** Paste data to the same frames it was copied from. */
  NONE,
  /** Paste keys starting at current frame. */
  CURRENT_FRAME,
};

static void matrix_to_fcurves(const float4x4 &matrix,
                              Span<FCurve *> fcurves,
                              const int frame,
                              const int key_index)
{
  /* Using IndexRange(3) because the last row is always 0/0/0/1 which means we don't need to
   * store it. */
  for (const int y : IndexRange(3)) {
    for (const int x : IndexRange(4)) {
      FPoint &fpt = fcurves[y * 4 + x]->fpt[key_index];
      fpt.vec[0] = frame;
      fpt.vec[1] = matrix[x][y];
    }
  }
}

static float4x4 fcurves_to_matrix(const Span<const FCurve *> fcurves, const int key_index)
{
  float4x4 mat = float4x4::identity();
  for (const int y : IndexRange(3)) {
    for (const int x : IndexRange(4)) {
      FPoint &fpt = fcurves[y * 4 + x]->fpt[key_index];
      mat[x][y] = fpt.vec[1];
    }
  }
  return mat;
}

static Vector<ID *> get_unique_ids(const Span<AnimTransformable *> transformables)
{
  /* We need the ID pointers to build the depsgraph, but every ID in the Vector
   * should be unique. */
  Vector<ID *> ids;
  Set<ID *> added_ids;
  for (const AnimTransformable *transformable : transformables) {
    if (added_ids.add(transformable->owner_id())) {
      ids.append(transformable->owner_id());
    }
  }
  return ids;
}

static Vector<ID *> get_unique_ids(const Span<AnimTransformable> transformables)
{
  /* We need the ID pointers to build the depsgraph, but every ID in the Vector
   * should be unique. */
  Vector<ID *> ids;
  Set<ID *> added_ids;
  for (const AnimTransformable &transformable : transformables) {
    if (added_ids.add(transformable.owner_id())) {
      ids.append(transformable.owner_id());
    }
  }
  return ids;
}

static DegComponentIdentifier transformable_to_deg_identifier(
    const AnimTransformable &transformable)
{
  switch (transformable.type()) {
    case AnimTransformable::Type::POSE_BONE:
      return {transformable.owner_id(), transformable.name(), DEG_OB_COMP_BONE};

    case AnimTransformable::Type::OBJECT:
      return {transformable.owner_id(), "", DEG_OB_COMP_TRANSFORM};
  }

  BLI_assert_unreachable();
  return {transformable.owner_id(), "", DEG_OB_COMP_TRANSFORM};
}

static Array<TransformableRelations> build_transformable_relations(
    const Depsgraph *depsgraph, const MutableSpan<AnimTransformable *> transformables)
{
  Array<TransformableRelations> transformable_relations(transformables.size());
  Map<DegComponentIdentifier, TransformableRelations *> component_map;

  for (const int i : transformables.index_range()) {
    AnimTransformable &transformable = *transformables[i];
    transformable_relations[i].transformable = &transformable;
    transformable_relations[i].done = false;
    component_map.add(transformable_to_deg_identifier(transformable), &transformable_relations[i]);
  }

  for (const DegComponentIdentifier &deg_identifier : component_map.keys()) {
    TransformableRelations *transformable_relation = component_map.lookup(deg_identifier);
    DEG_foreach_dependent_component(
        depsgraph,
        deg_identifier.id,
        deg_identifier.type,
        deg_identifier.name,
        [&](ID *other_id, eDepsObjectComponentType type, StringRef component_name) {
          if (!ELEM(type, DEG_OB_COMP_TRANSFORM, DEG_OB_COMP_BONE)) {
            return true;
          }
          DegComponentIdentifier dependent_deg_id(other_id, component_name, type);
          if (dependent_deg_id == deg_identifier) {
            /* Skip self. */
            return true;
          }
          TransformableRelations *dependent = component_map.lookup_default(dependent_deg_id,
                                                                           nullptr);
          if (!dependent) {
            return true;
          }
          dependent->ancestors.append(transformable_relation);
          if (dependent->done) {
            /* This is used as an optimization. If we already visited that component for a previous
             * transformable, the dependency of that branch is already recorded and we can stop
             * iterating into it. */
            return false;
          }
          dependent->done = true;
          return true;
        });
  }
  for (TransformableRelations &relation : transformable_relations) {
    /* Resetting the bool for the next use. */
    relation.done = false;
  }
  return transformable_relations;
}

static Vector<AnimTransformable *> depsgraph_sorted_transformables(
    const Depsgraph *depsgraph, const MutableSpan<AnimTransformable *> transformables)
{
  Array<TransformableRelations> transformable_relations = build_transformable_relations(
      depsgraph, transformables);
  Vector<AnimTransformable *> sorted_transformables;

  while (true) {
    bool inserted_any = false;
    for (TransformableRelations &relations : transformable_relations) {
      if (relations.done || !relations.all_ancestors_done()) {
        continue;
      }
      sorted_transformables.append(relations.transformable);
      inserted_any = true;
      relations.done = true;
    }
    if (!inserted_any) {
      /* There are 2 cases in which this can happen. Either we applied all transforms, or there
       * is a dependency cycle where 2 transformable_relations have each other in their
       * ancestors. In the latter case the returned Vector will not contain those transformables
       * with a cycle.*/
      break;
    }
  }

  return sorted_transformables;
}

/**
 * \param range inclusive/exclusive
 */
static void ensure_baked_fcurves(Main &bmain,
                                 const AnimTransformable &transformable,
                                 MutableSpan<PasteFCurve> fcus,
                                 blender::animrig::Channelbag &channelbag,
                                 const StringRefNull rna_path,
                                 const Bounds<int> range)
{
  namespace ar = blender::animrig;

  bool has_key_on_frame = false;
  const StringRefNull group_name = transformable.fcurve_group_name();
  /* Ensuring all FCurves exist. */
  for (const int i : fcus.index_range()) {
    PasteFCurve &paste_fcu = fcus[i];
    if (!paste_fcu.fcurve) {
      FCurve &fcurve = channelbag.fcurve_ensure(&bmain,
                                                {rna_path, i, PROP_FLOAT, PROP_NONE, group_name});
      paste_fcu.fcurve = &fcurve;
      paste_fcu.created_on_paste = true;
    }
    if (paste_fcu.fcurve->fpt) {
      /* Avoid crashes with sampled FCurves. */
      continue;
    }
    ar::bake_fcurve(
        paste_fcu.fcurve, {range.min, range.max - 1}, 1, ar::BakeCurveRemove::IN_RANGE);
    paste_fcu.paste_start_index = BKE_fcurve_bezt_binarysearch_index(
        paste_fcu.fcurve->bezt, range.min, paste_fcu.fcurve->totvert, &has_key_on_frame);
    BLI_assert(has_key_on_frame);
  }
}

/**
 * Returns true if the FCurve keeps the property it targets at a constant value throughout time.
 */
static bool is_fcurve_flat(FCurve &fcurve)
{
  if (!fcurve.modifiers.is_empty()) {
    /* Any modifiers count as potentially modifying values over time. */
    return false;
  }
  if (fcurve.totvert == 0) {
    return true;
  }

  if (!fcurve.bezt) {
    /* FPoint is not yet supported. */
    return false;
  }

  if (fcurve.totvert == 1) {
    if (fcurve.extend == FCURVE_EXTRAPOLATE_CONSTANT) {
      /* Handles don't matter in this case. */
      return true;
    }
    const BezTriple &key = fcurve.bezt[0];
    return key.vec[1][1] == key.vec[0][1] && key.vec[1][1] == key.vec[2][1];
  }

  constexpr float threshold = 0.0001f;
  const float reference_value = fcurve.bezt[0].vec[1][1];
  for (int i = 0; i < fcurve.totvert; i++) {
    if (fabsf(reference_value - fcurve.bezt[i].vec[1][1]) > threshold) {
      return false;
    }
    if (fcurve.bezt[i].ipo != BEZT_IPO_BEZ) {
      /* Handles have no effect. */
      continue;
    }
    if (fabsf(reference_value - fcurve.bezt[i].vec[0][1]) > threshold) {
      return false;
    }
    if (fabsf(reference_value - fcurve.bezt[i].vec[2][1]) > threshold) {
      return false;
    }
  }
  return true;
}

/* Remove any FCurves that have been created for pasting and remain static after pasting. */
static void clean_baked_fcurves(AnimTransformable &transformable,
                                AnimTransformable::PropertyType property_type,
                                MutableSpan<PasteFCurve> fcurves,
                                blender::animrig::Channelbag &channelbag)
{
  /* When we delete FCurves we have to flush the values to the transformable.
   * Otherwise they are lost. */
  TransformFloats values = transformable.get_property(property_type);
  for (PasteFCurve &paste_fcurve : fcurves) {
    if (!paste_fcurve.created_on_paste) {
      continue;
    }
    if (is_fcurve_flat(*paste_fcurve.fcurve)) {
      BLI_assert(paste_fcurve.fcurve->bezt != nullptr);
      values[paste_fcurve.fcurve->array_index] = paste_fcurve.fcurve->bezt[0].vec[1][1];
      channelbag.fcurve_remove(*paste_fcurve.fcurve);
      paste_fcurve.fcurve = nullptr;
    }
    if (paste_fcurve.fcurve) {
      BKE_fcurve_handles_recalc(*paste_fcurve.fcurve);
    }
  }
  transformable.set_property(property_type, values, AxisMutable::AXIS_MUTABLE_ALL);
}

static Rotation set_keys_to_transform(TransformFCurves &t_fcus,
                                      const float4x4 &matrix,
                                      const Rotation &reference_rotation,
                                      const int paste_index)
{
  float3 location;
  float3 scale;
  Rotation rotation_quat;
  rotation_quat.mode = ROT_MODE_QUAT;
  rotation_quat.values.reinitialize(4);
  mat4_decompose(location, rotation_quat.values.data(), scale, matrix.ptr());

  const Rotation rotation = rotation_quat.converted_to_mode(t_fcus.rotation_mode,
                                                            &reference_rotation);

  for (const int i : t_fcus.location.index_range()) {
    PasteFCurve &pfcu = t_fcus.location[i];
    BLI_assert(pfcu.paste_start_index + paste_index < pfcu.fcurve->totvert);
    if (!pfcu.fcurve->bezt) {
      continue;
    }
    const int bezt_index = pfcu.paste_start_index + paste_index;
    BezTriple &key = pfcu.fcurve->bezt[bezt_index];
    BKE_fcurve_keyframe_move_value_with_handles(&key, location[i]);
  }

  for (const int i : t_fcus.rotation.index_range()) {
    PasteFCurve &pfcu = t_fcus.rotation[i];
    BLI_assert(pfcu.paste_start_index + paste_index < pfcu.fcurve->totvert);
    if (!pfcu.fcurve->bezt) {
      continue;
    }
    const int bezt_index = pfcu.paste_start_index + paste_index;
    BezTriple &key = pfcu.fcurve->bezt[bezt_index];
    BKE_fcurve_keyframe_move_value_with_handles(&key, rotation.values[i]);
  }

  for (const int i : t_fcus.scale.index_range()) {
    PasteFCurve &pfcu = t_fcus.scale[i];
    BLI_assert(pfcu.paste_start_index + paste_index < pfcu.fcurve->totvert);
    if (!pfcu.fcurve->bezt) {
      continue;
    }
    const int bezt_index = pfcu.paste_start_index + paste_index;
    BezTriple &key = pfcu.fcurve->bezt[bezt_index];
    BKE_fcurve_keyframe_move_value_with_handles(&key, scale[i]);
  }
  return rotation;
}

/* -------------------------------------------------------------------- */
/** \name Main Functions
 * \{ */

/**
 * \param range inclusive/exclusive
 */
static void copy_world_space(Main &bmain,
                             Scene &scene,
                             ViewLayer &view_layer,
                             ReportList &reports,
                             const Span<AnimTransformable> transformables,
                             const Bounds<int> range)
{
  namespace ar = blender::animrig;
  namespace bf = bke::blendfile;
  bf::PartialWriteContext copybuffer{bmain};
  bAction *dna_action = reinterpret_cast<bAction *>(
      copybuffer.id_create(ID_AC,
                           "world_space_copy",
                           nullptr,
                           {(bf::PartialWriteContext::IDAddOperations::SET_FAKE_USER |
                             bf::PartialWriteContext::IDAddOperations::SET_CLIPBOARD_MARK)}));

  /* Using the frame start and end of the action to store the range of the copied keys. */
  dna_action->frame_start = range.min;
  dna_action->frame_end = range.max;
  ar::Action &action = dna_action->wrap();
  action.layer_keystrip_ensure();
  ar::StripKeyframeData &strip_data = action.layer(0)->strip(0)->data<ar::StripKeyframeData>(
      action);
  ar::Slot &slot = action.slot_add();
  ar::Channelbag &channelbag = strip_data.channelbag_for_slot_ensure(slot);

  /* We are storing the world space matrix in separate FCurves so the data can be stored in a
   * blend file. */
  Set<StringRefNull> added_names;
  Array<Array<FCurve *>> world_space_data(transformables.size());
  for (const int transformable_index : transformables.index_range()) {
    const AnimTransformable &transformable = transformables[transformable_index];
    if (!added_names.add(transformable.name())) {
      /* When copying bones from different armatures, we can get identical names which would create
       * an invalid copy buffer. */
      continue;
    }
    Array<FCurve *> fcurves(12);
    for (const int i : fcurves.index_range()) {
      FCurve *fcurve = BKE_fcurve_create();
      fcurve->rna_path_set(transformable.name());
      fcurve->array_index = i;
      const int vert_count = range.size();
      BLI_assert(vert_count > 0);
      /* Using FPoint because we only need 2 floats per key, not the huge struct that
       * is BezTriple.  */
      fcurve->fpt = MEM_new_array_uninitialized<FPoint>(vert_count, "world_space_copy_points");
      fcurve->totvert = vert_count;
      /* Could allocate space on the channelbag in big chunks instead of appending which is a
       * MEM_new every time. */
      channelbag.fcurve_append(*fcurve);
      fcurves[i] = fcurve;
    }

    world_space_data[transformable_index] = std::move(fcurves);
  }

  if (added_names.size() != transformables.size()) {
    BKE_report(&reports,
               RPT_WARNING,
               "Found duplicate names in selection. Not all selected were saved into clipboard.");
  }

  Depsgraph *depsgraph = DEG_graph_new(&bmain, &scene, &view_layer, DAG_EVAL_VIEWPORT);
  Vector<ID *> ids = get_unique_ids(transformables);
  DEG_graph_build_from_ids(depsgraph, ids);

  for (int frame = range.min; frame < range.max; frame++) {
    const int key_index = frame - range.min;
    DEG_evaluate_on_framechange(depsgraph, frame);
    for (const int transformable_index : transformables.index_range()) {
      if (world_space_data[transformable_index].size() == 0) {
        /* May be empty if skipped due to name collisions. */
        continue;
      }
      const AnimTransformable &transformable = transformables[transformable_index];
      const float4x4 world_matrix = transformable.get_world_space(*depsgraph);
      matrix_to_fcurves(world_matrix, world_space_data[transformable_index], frame, key_index);
    }
  }

  DEG_graph_free(depsgraph);

  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), BKE_tempdir_base(), clipboard_name);
  BLI_assert(copybuffer.is_valid());
  copybuffer.write_as_copypaste_buffer(filepath, reports);
  if (added_names.size() == transformables.size()) {
    const char *transformable_type_name = transformables[0].type() ==
                                                  AnimTransformable::Type::OBJECT ?
                                              "objects" :
                                              "bones";
    BKE_reportf(&reports,
                RPT_INFO,
                "Copied %d %s across %d frames to the clipboard",
                int(added_names.size()),
                transformable_type_name,
                range.max - range.min);
  }
}

/* Build a map of the transformable name to the name in the clipboard to read from. */
static PasteMap generate_paste_mapping(const MutableSpan<AnimTransformable> transformables,
                                       const ClipboardData &clipboard_data)
{
  PasteMap paste_map;

  const bool from_single = clipboard_data.size() == 1;
  const bool to_single = transformables.size() == 1;
  if (from_single && to_single) {
    paste_map.add(&transformables[0], *clipboard_data.keys().begin());
    return paste_map;
  }

  /* Pasting the same data to all transformables. */
  if (from_single && !to_single) {
    StringRefNull source_name = *clipboard_data.keys().begin();
    for (AnimTransformable &transformable : transformables) {
      paste_map.add(&transformable, source_name);
    }
    return paste_map;
  }

  /* Strict name matching. */
  for (AnimTransformable &transformable : transformables) {
    const Array<FCurve *> *fcurves = clipboard_data.lookup_ptr(transformable.name());
    if (fcurves) {
      paste_map.add(&transformable, transformable.name());
    }
  }
  return paste_map;
}

/**
 * Ensures that all FCurves exist to paste keys into and they have keys on all required frames.
 * The returned array will have the same length as the given `transformables`.
 */
static Array<TransformFCurves> build_fcurves_for_paste(
    Main &bmain, const Span<AnimTransformable *> transformables, const Bounds<int> range)
{
  namespace ar = blender::animrig;
  Array<TransformFCurves> fcurve_buffer(transformables.size());
  for (const int i : transformables.index_range()) {
    AnimTransformable *transformable = transformables[i];
    ID *owner_id = transformable->owner_id();
    bAction *paste_dna_action = ar::id_action_ensure(&bmain, owner_id);
    /* When adding layers this becomes a lot more complicated. We'll have to answer where keys go
     * in this case. Multiple things to consider:
     * - The active layer may have no effect on the final pose
     * - Not every layer may have a channelbag for this transformable.
     * - When inserting keys into a layer that is additive, we need to adjust the inserted values.
     * - When inserting keys into a layer with an influence < 1 we'll also have to adjust the
     * values.
     * - In all cases, the result has to be that the final world space of the transformable ends up
     * where it was copied from.
     */
    ar::assert_baklava_phase_1_invariants(paste_dna_action->wrap());
    ar::Channelbag &channelbag = ar::action_channelbag_ensure(*paste_dna_action, *owner_id);
    TransformFCurves &transform_fcurves = fcurve_buffer[i];
    transform_fcurves.channelbag = &channelbag;
    transform_fcurves.rotation_mode = transformable->get_rotation_mode();
    if (transform_fcurves.rotation_mode >= ROT_MODE_EUL) {
      transform_fcurves.rotation.reinitialize(3);
    }
    else {
      transform_fcurves.rotation.reinitialize(4);
    }
    const std::string loc_path = transformable->rna_path_to_property(
        AnimTransformable::PropertyType::LOCATION);
    const std::string rot_path = transformable->rna_path_to_property(
        AnimTransformable::PropertyType::ROTATION);
    const std::string scale_path = transformable->rna_path_to_property(
        AnimTransformable::PropertyType::SCALE);
    const std::string rotation_mode_path = transformable->rna_path_to_rotation_mode();

    FCurve *rotation_mode_fcurve = nullptr;

    for (FCurve *fcurve : channelbag.fcurves()) {
      StringRefNull fcurve_path(fcurve->rna_path());
      if (fcurve_path == loc_path) {
        transform_fcurves.location[fcurve->array_index].fcurve = fcurve;
      }
      else if (fcurve_path == rot_path) {
        transform_fcurves.rotation[fcurve->array_index].fcurve = fcurve;
      }
      else if (fcurve_path == scale_path) {
        transform_fcurves.scale[fcurve->array_index].fcurve = fcurve;
      }
      else if (fcurve_path == rotation_mode_path) {
        BLI_assert(rotation_mode_fcurve == nullptr);
        rotation_mode_fcurve = fcurve;
      }
    }

    /* Ensuring all FCurves exist. */
    ensure_baked_fcurves(
        bmain, *transformable, transform_fcurves.location, channelbag, loc_path, range);
    ensure_baked_fcurves(
        bmain, *transformable, transform_fcurves.rotation, channelbag, rot_path, range);
    ensure_baked_fcurves(
        bmain, *transformable, transform_fcurves.scale, channelbag, scale_path, range);
    if (rotation_mode_fcurve && rotation_mode_fcurve->bezt) {
      /* We have to ensure the whole range uses the same rotation mode, otherwise it wouldn't be
       * guaranteed that the rotation FCurves will be used over the full range. Changing euler to
       * quaternion would change which FCurves are read from. */
      const eRotationModes current_mode = transformable->get_rotation_mode();
      ar::bake_fcurve(
          rotation_mode_fcurve, {range.min, range.max - 1}, 1, ar::BakeCurveRemove::IN_RANGE);
      bool has_key_on_frame;
      const int range_start_index = BKE_fcurve_bezt_binarysearch_index(
          rotation_mode_fcurve->bezt, range.min, rotation_mode_fcurve->totvert, &has_key_on_frame);
      BLI_assert(has_key_on_frame);
      for (int frame = range.min; frame < range.max; frame++) {
        BKE_fcurve_keyframe_move_value_with_handles(
            &rotation_mode_fcurve->bezt[range_start_index + frame - range.min], current_mode);
      }
    }
  }

  return fcurve_buffer;
}

static bAction *read_action_from_clipboard(Main &clipboard_bmain, ReportList &reports)
{
  namespace ar = blender::animrig;

  if (clipboard_bmain.actions.is_empty()) {
    BKE_report(&reports, RPT_ERROR, "Clipboard data has no animation");
    return nullptr;
  }

  bAction *clipboard_dna_action = clipboard_bmain.actions.first();
  ar::Action &clipboard_action = clipboard_dna_action->wrap();
  if (clipboard_action.strip_keyframe_data().is_empty() ||
      clipboard_action.strip_keyframe_data()[0]->channelbags().is_empty())
  {
    BKE_report(&reports, RPT_ERROR, "Clipboard data has no animation");
    return nullptr;
  }

  return clipboard_dna_action;
}

static void paste_world_space(Main &bmain,
                              Scene &scene,
                              ViewLayer &view_layer,
                              ReportList &reports,
                              bAction &clipboard_dna_action,
                              const MutableSpan<AnimTransformable> transformables,
                              const AnimationPasteOffset offset)
{
  namespace ar = blender::animrig;

  ar::Action &clipboard_action = clipboard_dna_action.wrap();
  BLI_assert(clipboard_action.strip_keyframe_data_array_num == 1 &&
             clipboard_action.strip_keyframe_data()[0]->channelbag_array_num == 1);
  ar::Channelbag &channelbag = *clipboard_action.strip_keyframe_data()[0]->channelbags()[0];
  ClipboardData clipboard_data;
  for (FCurve *fcurve : channelbag.fcurves()) {
    BLI_assert(fcurve != nullptr);
    Array<FCurve *> &fcurves = clipboard_data.lookup_or_add(fcurve->rna_path(),
                                                            Array<FCurve *>(12));
    fcurves[fcurve->array_index] = fcurve;
  }

  for (Array<FCurve *> &fcurves : clipboard_data.values()) {
    for (FCurve *fcurve : fcurves) {
      if (fcurve == nullptr) {
        BKE_report(&reports, RPT_ERROR, "Clipboard contains incomplete animation data");
        return;
      }
    }
  }

  PasteMap paste_map = generate_paste_mapping(transformables, clipboard_data);

  if (paste_map.size() == 0) {
    BKE_report(&reports, RPT_ERROR, "Cannot match selection to clipboard data");
    return;
  }

  Vector<AnimTransformable *> pasteables;
  pasteables.reserve(paste_map.size());
  for (AnimTransformable *anim_transformable : paste_map.keys()) {
    pasteables.append(anim_transformable);
  }
  /* Build a minimal depsgraph because we need to evaluate the scene on every frame to correctly
   * invert world to local space. */
  Depsgraph *depsgraph = DEG_graph_new(&bmain, &scene, &view_layer, DAG_EVAL_VIEWPORT);
  Vector<ID *> ids = get_unique_ids(pasteables);
  DEG_graph_build_from_ids(depsgraph, ids);

  /* We need to first apply the values to those transformables that are not affected by any
   * other transformables. This is why we need to sort using the depsgraph. */
  Vector<AnimTransformable *> sorted_transformables = depsgraph_sorted_transformables(depsgraph,
                                                                                      pasteables);

  if (sorted_transformables.size() != pasteables.size()) {
    BKE_report(
        &reports, RPT_ERROR, "Failed to figure out pasting order. Potential dependency cycle");
    DEG_graph_free(depsgraph);
    return;
  }

  Bounds<int> range = {int(clipboard_action.frame_start), int(clipboard_action.frame_end)};
  if (offset == AnimationPasteOffset::CURRENT_FRAME) {
    const int paste_length = range.size();
    const int start_frame = scene.r.cfra;
    range = {start_frame, start_frame + paste_length};
  }
  /* Building the FCurves with all their required keys beforehand to avoid constantly inserting
   * keys into the bezt array. */
  Array<TransformFCurves> fcurve_buffer = build_fcurves_for_paste(
      bmain, sorted_transformables, range);
  BLI_assert(fcurve_buffer.size() == sorted_transformables.size());

  /* Since we potentially added FCurves, we have to rebuild the depsgraph relations. */
  DEG_graph_tag_relations_update(depsgraph);
  DEG_graph_relations_update(depsgraph);

  for (const int i : sorted_transformables.index_range()) {
    AnimTransformable *transformable = sorted_transformables[i];
    TransformFCurves &transform_fcurves = fcurve_buffer[i];

    /* This could be optimized by batching together transformables that don't have a relation to
     * avoid a few depsgraph evaluations. */
    Rotation previous_rotation = transformable->get_rotation();
    for (int frame = range.min; frame < range.max; frame++) {
      DEG_evaluate_on_framechange(depsgraph, frame);
      /* Assuming that all FCurves have the range baked on 1s. */
      const int key_index = frame - range.min;
      const StringRefNull clipboard_name = paste_map.lookup(transformable);
      const Array<FCurve *> *fcurves = clipboard_data.lookup_ptr(clipboard_name);
      BLI_assert_msg(fcurves != nullptr,
                     "Only transformables with matching matrix data should iterated here");
      const float4x4 world_matrix = fcurves_to_matrix(*fcurves, key_index);
      const float4x4 local_matrix = transformable->world_to_local(*depsgraph, world_matrix);
      previous_rotation = set_keys_to_transform(
          transform_fcurves, local_matrix, previous_rotation, key_index);
    }
    /* Action will have been created in `build_fcurves_for_paste`. */
    bAction *paste_dna_action = BKE_animdata_from_id(transformable->owner_id())->action;
    /* We have to update the action, otherwise the key values we just changed won't be visible to
     * the next transformable. */
    DEG_graph_id_tag_update(&bmain, depsgraph, &paste_dna_action->id, ID_RECALC_ANIMATION);
  }

  for (const int i : sorted_transformables.index_range()) {
    AnimTransformable &transformable = *sorted_transformables[i];
    TransformFCurves &transform_fcurves = fcurve_buffer[i];
    clean_baked_fcurves(transformable,
                        AnimTransformable::PropertyType::LOCATION,
                        transform_fcurves.location,
                        *transform_fcurves.channelbag);
    clean_baked_fcurves(transformable,
                        AnimTransformable::PropertyType::ROTATION,
                        transform_fcurves.rotation,
                        *transform_fcurves.channelbag);
    clean_baked_fcurves(transformable,
                        AnimTransformable::PropertyType::SCALE,
                        transform_fcurves.scale,
                        *transform_fcurves.channelbag);
  }

  const char *transformable_type_name = transformables[0].type() ==
                                                AnimTransformable::Type::OBJECT ?
                                            "objects" :
                                            "bones";
  if (sorted_transformables.size() == clipboard_data.size()) {
    BKE_reportf(&reports,
                RPT_INFO,
                "Pasted all %d %s over %d frames",
                int(sorted_transformables.size()),
                transformable_type_name,
                range.max - range.min);
  }
  else {
    BKE_reportf(&reports,
                RPT_INFO,
                "Pasted %d/%d %s over %d frames",
                int(sorted_transformables.size()),
                int(clipboard_data.size()),
                transformable_type_name,
                range.max - range.min);
  }
  DEG_graph_free(depsgraph);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operators
 * \{ */

enum class CopyRange : uint8_t {
  /* Use the range provided by the operator. */
  CUSTOM,
  /* Use the playback range of the scene. This includes the preview range in case it is set. */
  PLAYBACK_RANGE
};

const EnumPropertyItem rna_enum_copy_range_items[] = {
    {int(CopyRange::CUSTOM),
     "CUSTOM",
     0,
     "Custom",
     "Use the range provided in the operator properties"},
    {int(CopyRange::PLAYBACK_RANGE),
     "PLAYBACK",
     0,
     "Playback Range",
     "Use the playback range of the scene"},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus world_space_copy_exec(bContext *C, wmOperator *op)
{
  Vector<AnimTransformable> transformables = selected_transformables_from_context(*C);
  const CopyRange range_mode = CopyRange(RNA_enum_get(op->ptr, "range_mode"));
  Bounds<int> bounds;
  switch (range_mode) {
    case CopyRange::CUSTOM:
      bounds = {RNA_int_get(op->ptr, "start"), RNA_int_get(op->ptr, "end") + 1};
      break;
    case CopyRange::PLAYBACK_RANGE: {
      Scene *scene = CTX_data_scene(C);
      ScenePlaybackRange range = BKE_scene_get_playback_range(scene);
      bounds = {range.start_frame, range.end_frame + 1};
      break;
    }

    default:
      return OPERATOR_CANCELLED;
      break;
  }
  if (bounds.is_empty()) {
    BKE_reportf(op->reports, RPT_ERROR, "Invalid frame range %d-%d", bounds.min, bounds.max);
    return OPERATOR_CANCELLED;
  }
  copy_world_space(*CTX_data_main(C),
                   *CTX_data_scene(C),
                   *CTX_data_view_layer(C),
                   *op->reports,
                   transformables,
                   bounds);
  return OPERATOR_FINISHED;
}

static bool world_space_copy_poll(bContext *C)
{
  return ED_operator_posemode(C) || ED_operator_objectmode(C);
}

void ANIM_OT_world_space_copy(wmOperatorType *ot)
{
  ot->name = "Copy World Space Range";
  ot->idname = "ANIM_OT_world_space_copy";
  ot->description = "Copy animation from selected elements to the clipboard";

  ot->exec = world_space_copy_exec;
  ot->poll = world_space_copy_poll;

  /* No undo possible since this creates data outside the current blend file. */
  ot->flag = OPTYPE_REGISTER;
  RNA_def_enum(ot->srna,
               "range_mode",
               rna_enum_copy_range_items,
               int(CopyRange::PLAYBACK_RANGE),
               "Range Mode",
               "Determines which range should be copied");
  RNA_def_int(
      ot->srna, "start", 0, -INT_MAX, INT_MAX, "Start", "Start frame to copy from", 0, INT_MAX);
  RNA_def_int(
      ot->srna, "end", 250, -INT_MAX, INT_MAX, "End", "End frame to copy from", 0, INT_MAX);
}

static bool has_constraints(const Span<AnimTransformable> transformables)
{
  for (const AnimTransformable &transformable : transformables) {
    switch (transformable.type()) {
      case AnimTransformable::Type::POSE_BONE: {
        bPoseChannel *pose_bone = transformable.data<bPoseChannel *>();
        if (!pose_bone->constraints.is_empty()) {
          return true;
        }
        break;
      }

      case AnimTransformable::Type::OBJECT: {
        Object *object = transformable.data<Object *>();
        if (!object->constraints.is_empty()) {
          return true;
        }
      }
    }
  }
  return false;
}

static wmOperatorStatus world_space_paste_exec(bContext *C, wmOperator *op)
{
  Vector<AnimTransformable> transformables = selected_transformables_from_context(*C);
  if (has_constraints(transformables)) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Selection contains constraints. Perfect world space match cannot be guaranteed");
  }

  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), BKE_tempdir_base(), clipboard_name);
  Main *clipboard_bmain = BKE_copybuffer_read(
      *CTX_data_main(C), filepath, op->reports, FILTER_ID_ALL);
  if (!clipboard_bmain) {
    BKE_report(op->reports, RPT_ERROR, "No clipboard to read from");
    return OPERATOR_CANCELLED;
  }
  bAction *clipboard_dna_action = read_action_from_clipboard(*clipboard_bmain, *op->reports);

  if (!clipboard_dna_action) {
    BKE_main_free(clipboard_bmain);
    return OPERATOR_CANCELLED;
  }

  const AnimationPasteOffset offset = AnimationPasteOffset(RNA_enum_get(op->ptr, "offset"));
  paste_world_space(*CTX_data_main(C),
                    *CTX_data_scene(C),
                    *CTX_data_view_layer(C),
                    *op->reports,
                    *clipboard_dna_action,
                    transformables,
                    offset);

  for (AnimTransformable &transformable : transformables) {
    DEG_id_tag_update(transformable.owner_id(), ID_RECALC_ANIMATION);
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, transformable.owner_id());
  }

  BKE_main_free(clipboard_bmain);
  return OPERATOR_FINISHED;
}

static bool world_space_paste_poll(bContext *C)
{
  return ED_operator_posemode(C) || ED_operator_objectmode(C);
}

const EnumPropertyItem rna_enum_animation_paste_offset_items[] = {
    {int(AnimationPasteOffset::NONE),
     "NONE",
     0,
     "No Offset",
     "Paste data to the same frames they were copied from"},
    {int(AnimationPasteOffset::CURRENT_FRAME),
     "START",
     0,
     "Start at Current Frame",
     "Paste data starting at current frame"},
    {0, nullptr, 0, nullptr, nullptr},
};

void ANIM_OT_world_space_paste(wmOperatorType *ot)
{
  ot->name = "Paste World Space";
  ot->idname = "ANIM_OT_world_space_paste";
  ot->description = "Paste the animation from the clipboard to selected elements";

  ot->exec = world_space_paste_exec;
  ot->poll = world_space_paste_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "offset",
               rna_enum_animation_paste_offset_items,
               int(AnimationPasteOffset::NONE),
               "Frame Offset",
               "Paste time offset of keys");
}

/** \} */

}  // namespace blender::ed::animrig
