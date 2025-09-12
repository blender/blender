/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <optional>

#include <fmt/format.h>

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_attribute.hh"
#include "BKE_blendfile_link_append.hh"
#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_fcurve.hh"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_grease_pencil_legacy_convert.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"
#include "BKE_screen.hh"

#include "BLO_readfile.hh"

#include "BLI_color.hh"
#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"

namespace blender::bke::greasepencil::convert {

/**
 * Data shared across most of GP conversion code.
 */
struct ConversionData {
  Main &bmain;
  BlendfileLinkAppendContext *lapp_context;
  /** A mapping between a library and a generated 'offset radius' node group. */
  Map<Library *, bNodeTree *> offset_radius_ntree_by_library = {};
  /** A mapping between a legacy GPv2 ID and its converted GPv3 ID. */
  Map<bGPdata *, GreasePencil *> legacy_to_greasepencil_data = {};

  ConversionData(Main &bmain, BlendfileLinkAppendContext *lapp_context)
      : bmain(bmain), lapp_context(lapp_context)
  {
  }
};

/* -------------------------------------------------------------------- */
/** \name Animation conversion helpers.
 *
 * These utilities will call given callback over all relevant F-curves
 * (also includes drivers, and actions linked through the NLA).
 * \{ */

using FCurveConvertCB = void(FCurve &fcurve);

/**
 * Conversion data for the FCurve of a specific RNA property.
 *
 * Used as sub-data of #AnimDataConvertor when precise control over which and how FCurves are
 * converted.
 */
struct AnimDataFCurveConvertor {
  /**
   * Source and destination RNA paths
   * (relative to the relevant root paths stored in the owner #AnimDataConvertor data).
   */
  const char *relative_rna_path_src;
  const char *relative_rna_path_dst;

  /** Optional callback to perform additional conversion work on a specific FCurve. */
  blender::FunctionRef<FCurveConvertCB> convert_cb;

  AnimDataFCurveConvertor(const char *relative_rna_path_src,
                          const char *relative_rna_path_dst,
                          blender::FunctionRef<FCurveConvertCB> convert_cb = {nullptr})
      : relative_rna_path_src(relative_rna_path_src),
        relative_rna_path_dst(relative_rna_path_dst),
        convert_cb(convert_cb)
  {
  }
};

/**
 * This class contains data and logic to handle conversion of animation data (FCurves).
 *
 * It can be used to either:
 *  - Convert FCurves within a same animation data (essentially updating the RNA paths).
 *  - Convert FCurves and move them from the source to the destination IDs animation data.
 * The constructor used defines which of these two 'modes' will be the used by a given convertor.
 *
 * RNA paths to convert can be specified in two ways:
 *  - Complete paths, with a list of source to destination pairs of paths (relative to the relevant
 * root paths).
 *  - Only by the source and destination root paths (in which case all FCurves starting by these
 * paths will be converted).
 *
 * \note In case of transfer of animation data between IDs, NLA animation in source data is not
 * converted. This would require (partially) re-creating a copy of the potential source NLA into
 * the destination NLA, which is too complex for the few potential use cases.
 */
class AnimDataConvertor {
  ConversionData &conversion_data;

  ID &id_src;
  ID &id_dst;
  AnimData *animdata_src;
  AnimData *animdata_dst;

  /** Whether the source and destination IDs are different or not. */
  const bool is_transfer_between_ids;
  /**
   * Whether to skip NLA animation processing or not.
   * \note Currently NLA is skipped if this convertor transfers animation data between different
   * source and destination IDs.
   */
  const bool skip_nla;

  /**
   * Source (old) RNA property path in source ID to destination (new) matching property RNA path in
   * destination ID.
   *
   * \note All paths here are relative to their respective (source or destination) root path.
   * \note If this array is empty, all FCurves starting with `root_path_source` will be "rebased"
   * on `root_path_dst`.
   */
  const Array<AnimDataFCurveConvertor> fcurve_convertors;

 public:
  /**
   * Source and destination RNA root path. These can be modified by user code at any time (e.g.
   * when processing animation data for different modifiers...).
   */
  std::string root_path_src;
  std::string root_path_dst;

 private:
  /**
   * Store all FCurves that need to be moved from source animation to destination animation,
   * respectively for the main action, the temp action, and the drivers.
   *
   * Currently only used when moving animation from one source ID to a different destination ID.
   */
  blender::Vector<FCurve *> fcurves_from_src_main_action = {};
  blender::Vector<FCurve *> fcurves_from_src_tmp_action = {};
  blender::Vector<FCurve *> fcurves_from_src_drivers = {};
  /**
   * Generic 'has done something' flag, used to decide whether depsgraph tagging for updates is
   * needed.
   */
  bool has_changes = false;

 public:
  /** Constructor to use when only processing FCurves within a same ID animation data. */
  AnimDataConvertor(ConversionData &conversion_data,
                    ID &id_src,
                    const Array<AnimDataFCurveConvertor> fcurve_convertors = {})
      : conversion_data(conversion_data),
        id_src(id_src),
        id_dst(id_src),
        animdata_src(BKE_animdata_from_id(&id_src)),
        animdata_dst(BKE_animdata_from_id(&id_dst)),
        is_transfer_between_ids(false),
        skip_nla(false),
        fcurve_convertors(fcurve_convertors)
  {
  }
  /** Constructor to use when moving FCurves from one ID to another. */
  AnimDataConvertor(ConversionData &conversion_data,
                    ID &id_dst,
                    ID &id_src,
                    const Array<AnimDataFCurveConvertor> fcurve_convertors = {})
      : conversion_data(conversion_data),
        id_src(id_src),
        id_dst(id_dst),
        animdata_src(BKE_animdata_from_id(&id_src)),
        animdata_dst(BKE_animdata_from_id(&id_dst)),
        is_transfer_between_ids(true),
        skip_nla(true),
        fcurve_convertors(fcurve_convertors)
  {
  }
  AnimDataConvertor() = delete;

 private:
  using FCurveCallback = bool(bAction *owner_action, FCurve &fcurve);
  using ActionCallback = bool(bAction &action);

  /** \return True if this AnimDataConvertor is valid, i.e. can be used to process animation data
   * from source ID. */
  bool is_valid() const
  {
    return this->animdata_src != nullptr;
  }

  /* Basic common check to decide whether a legacy fcurve should be processed or not. */
  bool legacy_fcurves_is_valid_for_root_path(FCurve &fcurve, StringRefNull legacy_root_path) const
  {
    if (!fcurve.rna_path) {
      return false;
    }
    StringRefNull rna_path = fcurve.rna_path;
    if (!rna_path.startswith(legacy_root_path)) {
      return false;
    }
    return true;
  }

  /**
   * Common filtering of FCurve RNA path to decide whether they can/need to be processed here or
   * not.
   */
  bool animation_fcurve_is_valid(bAction *owner_action, FCurve &fcurve) const
  {
    if (!this->is_valid()) {
      return false;
    }
    /* Only take into account drivers (nullptr `action_owner`), and Actions directly assigned
     * to the animdata, not the NLA ones. */
    if (owner_action &&
        !ELEM(owner_action, this->animdata_src->action, this->animdata_src->tmpact))
    {
      return false;
    }
    if (!legacy_fcurves_is_valid_for_root_path(fcurve, this->root_path_src)) {
      return false;
    }
    return true;
  }

  /* Iterator over all FCurves in a given animation data. */

  bool fcurve_foreach_in_action(bAction *owner_action,
                                blender::FunctionRef<FCurveCallback> callback) const
  {
    bool is_changed = false;
    animrig::foreach_fcurve_in_action(owner_action->wrap(), [&](FCurve &fcurve) {
      const bool local_is_changed = callback(owner_action, fcurve);
      is_changed = is_changed || local_is_changed;
    });

    return is_changed;
  }

  bool fcurve_foreach_in_listbase(ListBase &fcurves,
                                  blender::FunctionRef<FCurveCallback> callback) const
  {
    bool is_changed = false;
    LISTBASE_FOREACH (FCurve *, fcurve, &fcurves) {
      const bool local_is_changed = callback(nullptr, *fcurve);
      is_changed = is_changed || local_is_changed;
    }
    return is_changed;
  }

  bool nla_strip_fcurve_foreach(NlaStrip &nla_strip,
                                blender::FunctionRef<FCurveCallback> callback) const
  {
    bool is_changed = false;
    if (nla_strip.act) {
      if (this->fcurve_foreach_in_action(nla_strip.act, callback)) {
        DEG_id_tag_update(&nla_strip.act->id, ID_RECALC_ANIMATION);
        is_changed = true;
      }
    }
    LISTBASE_FOREACH (NlaStrip *, nla_strip_children, &nla_strip.strips) {
      const bool local_is_changed = this->nla_strip_fcurve_foreach(*nla_strip_children, callback);
      is_changed = is_changed || local_is_changed;
    }
    return is_changed;
  }

  bool animdata_fcurve_foreach(AnimData &anim_data,
                               blender::FunctionRef<FCurveCallback> callback) const
  {
    bool is_changed = false;
    if (anim_data.action) {
      if (this->fcurve_foreach_in_action(anim_data.action, callback)) {
        DEG_id_tag_update(&anim_data.action->id, ID_RECALC_ANIMATION);
        is_changed = true;
      }
    }
    if (anim_data.tmpact) {
      if (this->fcurve_foreach_in_action(anim_data.tmpact, callback)) {
        DEG_id_tag_update(&anim_data.tmpact->id, ID_RECALC_ANIMATION);
        is_changed = true;
      }
    }

    {
      const bool local_is_changed = this->fcurve_foreach_in_listbase(anim_data.drivers, callback);
      is_changed = is_changed || local_is_changed;
    }

    /* NOTE: New layered actions system can be ignored here, it did not exist together with GPv2.
     */

    if (this->skip_nla) {
      return is_changed;
    }

    LISTBASE_FOREACH (NlaTrack *, nla_track, &anim_data.nla_tracks) {
      LISTBASE_FOREACH (NlaStrip *, nla_strip, &nla_track->strips) {
        const bool local_is_changed = this->nla_strip_fcurve_foreach(*nla_strip, callback);
        is_changed = is_changed || local_is_changed;
      }
    }
    return is_changed;
  }

  bool action_process(bAction &action, blender::FunctionRef<ActionCallback> callback) const
  {
    if (callback(action)) {
      DEG_id_tag_update(&action.id, ID_RECALC_ANIMATION);
      return true;
    }
    return false;
  }

  bool nla_strip_action_foreach(NlaStrip &nla_strip,
                                blender::FunctionRef<ActionCallback> callback) const
  {
    bool is_changed = false;
    if (nla_strip.act) {
      is_changed = action_process(*nla_strip.act, callback);
    }
    LISTBASE_FOREACH (NlaStrip *, nla_strip_children, &nla_strip.strips) {
      is_changed = is_changed || this->nla_strip_action_foreach(*nla_strip_children, callback);
    }
    return is_changed;
  }

  bool animdata_action_foreach(AnimData &anim_data,
                               blender::FunctionRef<ActionCallback> callback) const
  {
    bool is_changed = false;

    if (anim_data.action) {
      is_changed = is_changed || action_process(*anim_data.action, callback);
    }
    if (anim_data.tmpact) {
      is_changed = is_changed || action_process(*anim_data.tmpact, callback);
    }

    /* NOTE: New layered actions system can be ignored here, it did not exist together with GPv2.
     */

    if (this->skip_nla) {
      return is_changed;
    }

    LISTBASE_FOREACH (NlaTrack *, nla_track, &anim_data.nla_tracks) {
      LISTBASE_FOREACH (NlaStrip *, nla_strip, &nla_track->strips) {
        is_changed = is_changed || this->nla_strip_action_foreach(*nla_strip, callback);
      }
    }
    return is_changed;
  }

 public:
  /**
   * Check whether the source animation data contains FCurves that need to be converted/moved to
   * the destination animation data.
   */
  bool source_has_animation_to_convert() const
  {
    if (!this->is_valid()) {
      return false;
    }

    if (GS(id_src.name) != GS(id_dst.name)) {
      return true;
    }

    bool has_animation = false;
    auto animation_detection_cb = [&](bAction *owner_action, FCurve &fcurve) -> bool {
      /* Early out if we already know that the target data is animated. */
      if (has_animation) {
        return false;
      }
      if (!this->animation_fcurve_is_valid(owner_action, fcurve)) {
        return false;
      }
      if (this->fcurve_convertors.is_empty()) {
        has_animation = true;
        return false;
      }
      StringRefNull rna_path = fcurve.rna_path;
      for (const AnimDataFCurveConvertor &fcurve_convertor : this->fcurve_convertors) {
        const std::string rna_path_src = fmt::format(
            "{}{}", this->root_path_src, fcurve_convertor.relative_rna_path_src);
        if (rna_path == rna_path_src) {
          has_animation = true;
          return false;
        }
      }
      return false;
    };

    this->animdata_fcurve_foreach(*this->animdata_src, animation_detection_cb);
    return has_animation;
  }

  /**
   * Convert relevant FCurves, i.e. modify their RNA path to match destination data.
   *
   * \note Edited FCurves will remain in the source animation data after this process. Once all
   * source animation data has been processed, #fcurves_convert_finalize has to be called.
   */
  void fcurves_convert()
  {
    if (!this->is_valid()) {
      return;
    }

    auto fcurve_convert_cb = [&](const AnimDataFCurveConvertor *fcurve_convertor,
                                 bAction *owner_action,
                                 FCurve &fcurve,
                                 const std::string &rna_path_dst) {
      MEM_freeN(fcurve.rna_path);
      fcurve.rna_path = BLI_strdupn(rna_path_dst.c_str(), rna_path_dst.size());
      if (fcurve_convertor && fcurve_convertor->convert_cb) {
        fcurve_convertor->convert_cb(fcurve);
      }
      this->has_changes = true;

      if (!this->is_transfer_between_ids) {
        return;
      }
      if (owner_action) {
        if (owner_action == this->animdata_src->action) {
          this->fcurves_from_src_main_action.append(&fcurve);
        }
        else if (owner_action == this->animdata_src->tmpact) {
          this->fcurves_from_src_tmp_action.append(&fcurve);
        }
      }
      else { /* Driver */
        this->fcurves_from_src_drivers.append(&fcurve);
      }
    };

    /* Update all FCurves which RNA path starts with the #root_path_src. */
    if (this->fcurve_convertors.is_empty()) {
      auto fcurve_root_path_convert_cb = [&](bAction *owner_action, FCurve &fcurve) -> bool {
        if (!legacy_fcurves_is_valid_for_root_path(fcurve, this->root_path_src)) {
          return false;
        }
        StringRefNull rna_path = fcurve.rna_path;
        const std::string rna_path_dst = fmt::format(
            "{}{}", this->root_path_dst, rna_path.substr(int64_t(this->root_path_src.size())));
        fcurve_convert_cb(nullptr, owner_action, fcurve, rna_path_dst);
        return true;
      };

      this->animdata_fcurve_foreach(*(this->animdata_src), fcurve_root_path_convert_cb);
      return;
    }

    /* Update all FCurves which RNA path starts with the #root_path_src, and remains of the path
     * matches one of the entries in #fcurve_convertors. */
    auto fcurve_full_path_convert_cb = [&](bAction *owner_action, FCurve &fcurve) -> bool {
      if (!animation_fcurve_is_valid(owner_action, fcurve)) {
        return false;
      }
      StringRefNull rna_path = fcurve.rna_path;
      for (const AnimDataFCurveConvertor &fcurve_convertor : this->fcurve_convertors) {
        const std::string rna_path_src = fmt::format(
            "{}{}", this->root_path_src, fcurve_convertor.relative_rna_path_src);
        if (rna_path == rna_path_src) {
          const std::string rna_path_dst = fmt::format(
              "{}{}", this->root_path_dst, fcurve_convertor.relative_rna_path_dst);
          fcurve_convert_cb(&fcurve_convertor, owner_action, fcurve, rna_path_dst);
          return true;
        }
      }
      return false;
    };

    this->animdata_fcurve_foreach(*(this->animdata_src), fcurve_full_path_convert_cb);
  }

  /**
   * Finalize FCurves conversion. Typically, this #AnimDataConvertor should not be used after this
   * call.
   *
   * Currently, this call merely ensure depsgraph update in case of conversion of animation data
   * within a same ID.
   *
   * When transferring animation between source and destination IDs, this call actually moves the
   * processed fcurves accumulated by previous call(s) to #fcurves_convert.
   */
  void fcurves_convert_finalize()
  {
    if (!this->is_valid()) {
      return;
    }

    /* Ensure existing actions moved to a different ID type keep a 'valid' `idroot` value. Not
     * essential, but 'nice to have'. */
    if (GS(this->id_src.name) != GS(this->id_dst.name)) {
      if (!this->animdata_dst) {
        this->animdata_dst = BKE_animdata_ensure_id(&this->id_dst);
      }
      auto actions_idroot_ensure = [&](bAction &action) -> bool {
        BKE_animdata_action_ensure_idroot(&this->id_dst, &action);
        return true;
      };
      this->animdata_action_foreach(*this->animdata_dst, actions_idroot_ensure);
    }

    if (&id_src == &id_dst) {
      if (this->has_changes) {
        DEG_id_tag_update(&this->id_src, ID_RECALC_ANIMATION);
        DEG_relations_tag_update(&this->conversion_data.bmain);
      }
      return;
    }

    if (this->fcurves_from_src_main_action.is_empty() &&
        this->fcurves_from_src_tmp_action.is_empty() && this->fcurves_from_src_drivers.is_empty())
    {
      return;
    }
    if (!this->animdata_dst) {
      this->animdata_dst = BKE_animdata_ensure_id(&this->id_dst);
    }

    auto fcurves_move = [&](bAction *action_dst,
                            const animrig::slot_handle_t slot_handle_dst,
                            bAction *action_src,
                            const Span<FCurve *> fcurves) {
      for (FCurve *fcurve : fcurves) {
        animrig::action_fcurve_move(
            action_dst->wrap(), slot_handle_dst, action_src->wrap(), *fcurve);
      }
    };

    auto fcurves_move_between_listbases =
        [&](ListBase &fcurves_dst, ListBase &fcurves_src, const Span<FCurve *> fcurves) {
          for (FCurve *fcurve : fcurves) {
            BLI_assert(BLI_findindex(&fcurves_src, fcurve) >= 0);
            BLI_remlink(&fcurves_src, fcurve);
            BLI_addtail(&fcurves_dst, fcurve);
          }
        };

    if (!this->fcurves_from_src_main_action.is_empty()) {
      if (!this->animdata_dst->action) {
        /* Create a new action. */
        animrig::Action &action = animrig::action_add(
            this->conversion_data.bmain,
            this->animdata_src->action ? this->animdata_src->action->id.name + 2 : nullptr);
        action.slot_add_for_id(this->id_dst);

        const bool ok = animrig::assign_action(&action, {this->id_dst, *this->animdata_dst});
        BLI_assert_msg(ok, "Expecting action assignment to work when converting Grease Pencil");
        UNUSED_VARS_NDEBUG(ok);
      }
      fcurves_move(this->animdata_dst->action,
                   this->animdata_dst->slot_handle,
                   this->animdata_src->action,
                   this->fcurves_from_src_main_action);
      this->fcurves_from_src_main_action.clear();
    }
    if (!this->fcurves_from_src_tmp_action.is_empty()) {
      if (!this->animdata_dst->tmpact) {
        /* Create a new tmpact. */
        animrig::Action &tmpact = animrig::action_add(
            this->conversion_data.bmain,
            this->animdata_src->tmpact ? this->animdata_src->tmpact->id.name + 2 : nullptr);
        tmpact.slot_add_for_id(this->id_dst);

        const bool ok = animrig::assign_tmpaction(&tmpact, {this->id_dst, *this->animdata_dst});
        BLI_assert_msg(ok, "Expecting tmpact assignment to work when converting Grease Pencil");
        UNUSED_VARS_NDEBUG(ok);
      }
      fcurves_move(this->animdata_dst->tmpact,
                   this->animdata_dst->tmp_slot_handle,
                   this->animdata_src->tmpact,
                   this->fcurves_from_src_tmp_action);
      this->fcurves_from_src_tmp_action.clear();
    }
    if (!this->fcurves_from_src_drivers.is_empty()) {
      fcurves_move_between_listbases(this->animdata_dst->drivers,
                                     this->animdata_src->drivers,
                                     this->fcurves_from_src_drivers);
      this->fcurves_from_src_drivers.clear();
    }

    DEG_id_tag_update(&this->id_dst, ID_RECALC_ANIMATION);
    DEG_id_tag_update(&this->id_src, ID_RECALC_ANIMATION);
    DEG_relations_tag_update(&this->conversion_data.bmain);
  }
};

/** \} */

/**
 * Find vertex groups that have assigned vertices in this drawing.
 * Returns:
 * - ListBase with used vertex group names (bDeformGroup)
 * - Array of indices in the new vertex group list for remapping
 */
static void find_used_vertex_groups(const bGPDframe &gpf,
                                    const ListBase &vertex_group_names,
                                    const int num_vertex_groups,
                                    ListBase &r_vertex_group_names,
                                    Array<int> &r_indices)
{
  Array<int> is_group_used(num_vertex_groups, false);
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf.strokes) {
    if (!gps->dvert) {
      continue;
    }
    Span<MDeformVert> dverts = {gps->dvert, gps->totpoints};
    for (const MDeformVert &dvert : dverts) {
      for (const MDeformWeight &weight : Span<MDeformWeight>{dvert.dw, dvert.totweight}) {
        if (weight.def_nr >= num_vertex_groups) {
          /* Ignore invalid deform weight group indices. */
          continue;
        }
        is_group_used[weight.def_nr] = true;
      }
    }
  }
  BLI_listbase_clear(&r_vertex_group_names);
  r_indices.reinitialize(num_vertex_groups);
  int new_group_i = 0;
  int old_group_i;
  LISTBASE_FOREACH_INDEX (const bDeformGroup *, def_group, &vertex_group_names, old_group_i) {
    if (!is_group_used[old_group_i]) {
      r_indices[old_group_i] = -1;
      continue;
    }
    r_indices[old_group_i] = new_group_i++;

    bDeformGroup *def_group_copy = static_cast<bDeformGroup *>(MEM_dupallocN(def_group));
    BLI_addtail(&r_vertex_group_names, def_group_copy);
  }
}

/*
 * This takes the legacy UV transforms and returns the stroke-space to texture-space matrix.
 */
static float3x2 get_legacy_stroke_to_texture_matrix(const float2 uv_translation,
                                                    const float uv_rotation,
                                                    const float2 uv_scale)
{
  using namespace blender;

  /* Bounding box data. */
  const float2 minv = float2(-1.0f, -1.0f);
  const float2 maxv = float2(1.0f, 1.0f);
  /* Center of rotation. */
  const float2 center = float2(0.5f, 0.5f);

  const float2 uv_scale_inv = math::safe_rcp(uv_scale);
  const float2 diagonal = maxv - minv;
  const float sin_rotation = sin(uv_rotation);
  const float cos_rotation = cos(uv_rotation);
  const float2x2 rotation = float2x2(float2(cos_rotation, sin_rotation),
                                     float2(-sin_rotation, cos_rotation));

  float3x2 texture_matrix = float3x2::identity();

  /* Apply bounding box re-scaling. */
  texture_matrix[2] -= minv;
  texture_matrix = math::from_scale<float2x2>(1.0f / diagonal) * texture_matrix;

  /* Apply translation. */
  texture_matrix[2] += uv_translation;

  /* Apply rotation. */
  texture_matrix[2] -= center;
  texture_matrix = rotation * texture_matrix;
  texture_matrix[2] += center;

  /* Apply scale. */
  texture_matrix = math::from_scale<float2x2>(uv_scale_inv) * texture_matrix;

  return texture_matrix;
}

/*
 * This gets the legacy layer-space to stroke-space matrix.
 */
static blender::float4x2 get_legacy_layer_to_stroke_matrix(bGPDstroke *gps)
{
  using namespace blender;
  using namespace blender::math;

  const bGPDspoint *points = gps->points;
  const int totpoints = gps->totpoints;

  if (totpoints < 2) {
    return float4x2::identity();
  }

  const bGPDspoint *point0 = &points[0];
  const bGPDspoint *point1 = &points[1];
  const bGPDspoint *point3 = &points[int(totpoints * 0.75f)];

  const float3 pt0 = float3(point0->x, point0->y, point0->z);
  const float3 pt1 = float3(point1->x, point1->y, point1->z);
  const float3 pt3 = float3(point3->x, point3->y, point3->z);

  /* Local X axis (p0 -> p1) */
  const float3 local_x = normalize(pt1 - pt0);

  /* Point vector at 3/4 */
  const float3 local_3 = (totpoints == 2) ? (pt3 * 0.001f) - pt0 : pt3 - pt0;

  /* Vector orthogonal to polygon plane. */
  const float3 normal = cross(local_x, local_3);

  /* Local Y axis (cross to normal/x axis). */
  const float3 local_y = normalize(cross(normal, local_x));

  /* Get local space using first point as origin. */
  const float4x2 mat = transpose(
      float2x4(float4(local_x, -dot(pt0, local_x)), float4(local_y, -dot(pt0, local_y))));

  return mat;
}

static blender::float4x2 get_legacy_texture_matrix(bGPDstroke *gps)
{
  const float3x2 texture_matrix = get_legacy_stroke_to_texture_matrix(
      float2(gps->uv_translation), gps->uv_rotation, float2(gps->uv_scale));

  const float4x2 strokemat = get_legacy_layer_to_stroke_matrix(gps);
  float4x3 strokemat4x3 = float4x3(strokemat);
  /*
   * We need the diagonal of ones to start from the bottom right instead top left to properly apply
   * the two matrices.
   *
   * i.e.
   *          # # # #              # # # #
   * We need  # # # #  Instead of  # # # #
   *          0 0 0 1              0 0 1 0
   *
   */
  strokemat4x3[2][2] = 0.0f;
  strokemat4x3[3][2] = 1.0f;

  return texture_matrix * strokemat4x3;
}

static Drawing legacy_gpencil_frame_to_grease_pencil_drawing(const bGPDframe &gpf,
                                                             const ListBase &vertex_group_names)
{
  /* Create a new empty drawing. */
  Drawing drawing;

  /* Get the number of points, number of strokes and the offsets for each stroke. */
  Vector<int> offsets;
  Vector<int8_t> curve_types;
  offsets.append(0);
  int num_strokes = 0;
  int num_points = 0;
  bool has_bezier_stroke = false;
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf.strokes) {
    /* Check for a valid edit curve. This is only the case when the `editcurve` exists and wasn't
     * tagged for a stroke update. This tag indicates that the stroke points have changed,
     * invalidating the edit curve. */
    if (gps->editcurve != nullptr && (gps->editcurve->flag & GP_CURVE_NEEDS_STROKE_UPDATE) == 0) {
      if (gps->editcurve->tot_curve_points == 0) {
        continue;
      }
      has_bezier_stroke = true;
      num_points += gps->editcurve->tot_curve_points;
      curve_types.append(CURVE_TYPE_BEZIER);
    }
    else {
      if (gps->totpoints == 0) {
        continue;
      }
      num_points += gps->totpoints;
      curve_types.append(CURVE_TYPE_POLY);
    }
    num_strokes++;
    offsets.append(num_points);
  }

  /* Return if the legacy frame contains no strokes (or zero points). */
  if (num_strokes == 0) {
    return drawing;
  }

  /* Resize the CurvesGeometry. */
  CurvesGeometry &curves = drawing.strokes_for_write();
  curves.resize(num_points, num_strokes);
  curves.offsets_for_write().copy_from(offsets);

  OffsetIndices<int> points_by_curve = curves.points_by_curve();
  MutableAttributeAccessor attributes = curves.attributes_for_write();

  if (!has_bezier_stroke) {
    /* All strokes are poly curves. */
    curves.fill_curve_types(CURVE_TYPE_POLY);
  }
  else {
    curves.curve_types_for_write().copy_from(curve_types);
    curves.update_curve_types();
  }

  /* Find used vertex groups in this drawing. */
  ListBase stroke_vertex_group_names;
  Array<int> stroke_def_nr_map;
  const int num_vertex_groups = BLI_listbase_count(&vertex_group_names);
  find_used_vertex_groups(
      gpf, vertex_group_names, num_vertex_groups, stroke_vertex_group_names, stroke_def_nr_map);
  BLI_assert(BLI_listbase_is_empty(&curves.vertex_group_names));
  curves.vertex_group_names = stroke_vertex_group_names;
  const bool use_dverts = !BLI_listbase_is_empty(&curves.vertex_group_names);

  /* Copy vertex weights and map the vertex group indices. */
  auto copy_dvert = [&](const MDeformVert &src_dvert, MDeformVert &dst_dvert) {
    dst_dvert = src_dvert;
    dst_dvert.dw = static_cast<MDeformWeight *>(MEM_dupallocN(src_dvert.dw));
    const MutableSpan<MDeformWeight> vertex_weights = {dst_dvert.dw, dst_dvert.totweight};
    for (MDeformWeight &weight : vertex_weights) {
      if (weight.def_nr >= num_vertex_groups) {
        /* Ignore invalid deform weight group indices. */
        continue;
      }
      /* Map def_nr to the reduced vertex group list. */
      weight.def_nr = stroke_def_nr_map[weight.def_nr];
    }
  };

  /* Point Attributes. */
  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<float3> handle_positions_left = has_bezier_stroke ?
                                                  curves.handle_positions_left_for_write() :
                                                  MutableSpan<float3>();
  MutableSpan<float3> handle_positions_right = has_bezier_stroke ?
                                                   curves.handle_positions_right_for_write() :
                                                   MutableSpan<float3>();
  MutableSpan<float> radii = drawing.radii_for_write();
  MutableSpan<float> opacities = drawing.opacities_for_write();
  /* Note: Since we *know* the drawing are created from scratch, we assume that the following
   * `lookup_or_add_for_write_span` calls always return valid writers. */
  SpanAttributeWriter<float> delta_times = attributes.lookup_or_add_for_write_span<float>(
      "delta_time", AttrDomain::Point);
  SpanAttributeWriter<float> rotations = attributes.lookup_or_add_for_write_span<float>(
      "rotation", AttrDomain::Point);
  MutableSpan<ColorGeometry4f> vertex_colors = drawing.vertex_colors_for_write();
  SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
      ".selection", AttrDomain::Point);
  MutableSpan<MDeformVert> dverts = use_dverts ? curves.wrap().deform_verts_for_write() :
                                                 MutableSpan<MDeformVert>();

  /* Curve Attributes. */
  SpanAttributeWriter<bool> stroke_cyclic = attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", AttrDomain::Curve);
  SpanAttributeWriter<float> stroke_init_times = attributes.lookup_or_add_for_write_span<float>(
      "init_time", AttrDomain::Curve);
  SpanAttributeWriter<int8_t> stroke_start_caps = attributes.lookup_or_add_for_write_span<int8_t>(
      "start_cap", AttrDomain::Curve);
  SpanAttributeWriter<int8_t> stroke_end_caps = attributes.lookup_or_add_for_write_span<int8_t>(
      "end_cap", AttrDomain::Curve);
  SpanAttributeWriter<float> stroke_softness = attributes.lookup_or_add_for_write_span<float>(
      "softness", AttrDomain::Curve);
  SpanAttributeWriter<float> stroke_point_aspect_ratios =
      attributes.lookup_or_add_for_write_span<float>("aspect_ratio", AttrDomain::Curve);
  MutableSpan<ColorGeometry4f> stroke_fill_colors = drawing.fill_colors_for_write();
  SpanAttributeWriter<int> stroke_materials = attributes.lookup_or_add_for_write_span<int>(
      "material_index", AttrDomain::Curve);

  Array<float4x2> legacy_texture_matrices(num_strokes);

  int stroke_i = 0;
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf.strokes) {
    /* In GPv2 strokes with 0 points could technically be represented. In `CurvesGeometry` this is
     * not the case and would be a bug. So we explicitly make sure to skip over strokes with no
     * points. */
    if (gps->totpoints == 0 ||
        (gps->editcurve != nullptr && gps->editcurve->tot_curve_points == 0))
    {
      continue;
    }

    stroke_cyclic.span[stroke_i] = (gps->flag & GP_STROKE_CYCLIC) != 0;
    /* Truncating time in ms to uint32 then we don't lose precision in lower bits. */
    const uint32_t clamped_init_time = uint32_t(
        std::clamp(gps->inittime * 1e3, 0.0, double(std::numeric_limits<uint32_t>::max())));
    stroke_init_times.span[stroke_i] = float(clamped_init_time) / float(1e3);
    stroke_start_caps.span[stroke_i] = int8_t(gps->caps[0]);
    stroke_end_caps.span[stroke_i] = int8_t(gps->caps[1]);
    stroke_softness.span[stroke_i] = 1.0f - gps->hardness;
    stroke_point_aspect_ratios.span[stroke_i] = gps->aspect_ratio[0] /
                                                max_ff(gps->aspect_ratio[1], 1e-8);
    stroke_fill_colors[stroke_i] = ColorGeometry4f(gps->vert_color_fill);
    stroke_materials.span[stroke_i] = gps->mat_nr;

    const IndexRange points = points_by_curve[stroke_i];

    const float stroke_thickness = float(gps->thickness) * LEGACY_RADIUS_CONVERSION_FACTOR;
    MutableSpan<float3> dst_positions = positions.slice(points);
    MutableSpan<float3> dst_handle_positions_left = has_bezier_stroke ?
                                                        handle_positions_left.slice(points) :
                                                        MutableSpan<float3>();
    MutableSpan<float3> dst_handle_positions_right = has_bezier_stroke ?
                                                         handle_positions_right.slice(points) :
                                                         MutableSpan<float3>();
    MutableSpan<float> dst_radii = radii.slice(points);
    MutableSpan<float> dst_opacities = opacities.slice(points);
    MutableSpan<float> dst_deltatimes = delta_times.span.slice(points);
    MutableSpan<float> dst_rotations = rotations.span.slice(points);
    MutableSpan<ColorGeometry4f> dst_vertex_colors = vertex_colors.slice(points);
    MutableSpan<bool> dst_selection = selection.span.slice(points);
    MutableSpan<MDeformVert> dst_dverts = use_dverts ? dverts.slice(points) :
                                                       MutableSpan<MDeformVert>();

    if (curve_types[stroke_i] == CURVE_TYPE_POLY) {
      BLI_assert(points.size() == gps->totpoints);
      const Span<bGPDspoint> src_points{gps->points, gps->totpoints};
      threading::parallel_for(src_points.index_range(), 4096, [&](const IndexRange range) {
        for (const int point_i : range) {
          const bGPDspoint &pt = src_points[point_i];
          dst_positions[point_i] = float3(pt.x, pt.y, pt.z);
          dst_radii[point_i] = stroke_thickness * pt.pressure;
          dst_opacities[point_i] = pt.strength;
          dst_deltatimes[point_i] = pt.time;
          dst_rotations[point_i] = pt.uv_rot;
          dst_vertex_colors[point_i] = ColorGeometry4f(pt.vert_color);
          dst_selection[point_i] = (pt.flag & GP_SPOINT_SELECT) != 0;
          if (use_dverts && gps->dvert) {
            copy_dvert(gps->dvert[point_i], dst_dverts[point_i]);
          }
        }
      });
    }
    else if (curve_types[stroke_i] == CURVE_TYPE_BEZIER) {
      BLI_assert(gps->editcurve != nullptr);
      BLI_assert(points.size() == gps->editcurve->tot_curve_points);
      Span<bGPDcurve_point> src_curve_points{gps->editcurve->curve_points,
                                             gps->editcurve->tot_curve_points};

      threading::parallel_for(src_curve_points.index_range(), 4096, [&](const IndexRange range) {
        for (const int point_i : range) {
          const bGPDcurve_point &cpt = src_curve_points[point_i];
          dst_positions[point_i] = float3(cpt.bezt.vec[1]);
          dst_handle_positions_left[point_i] = float3(cpt.bezt.vec[0]);
          dst_handle_positions_right[point_i] = float3(cpt.bezt.vec[2]);
          dst_radii[point_i] = stroke_thickness * cpt.pressure;
          dst_opacities[point_i] = cpt.strength;
          dst_rotations[point_i] = cpt.uv_rot;
          dst_vertex_colors[point_i] = ColorGeometry4f(cpt.vert_color);
          dst_selection[point_i] = (cpt.flag & GP_CURVE_POINT_SELECT) != 0;
          if (use_dverts && gps->dvert) {
            copy_dvert(gps->dvert[point_i], dst_dverts[point_i]);
          }
        }
      });
    }
    else {
      /* Unknown curve type. */
      BLI_assert_unreachable();
    }

    const float4x2 legacy_texture_matrix = get_legacy_texture_matrix(gps);
    legacy_texture_matrices[stroke_i] = legacy_texture_matrix;

    stroke_i++;
  }

  /* Ensure that the normals are up to date. */
  curves.tag_normals_changed();
  drawing.set_texture_matrices(legacy_texture_matrices.as_span(), curves.curves_range());

  delta_times.finish();
  rotations.finish();
  selection.finish();

  stroke_cyclic.finish();
  stroke_init_times.finish();
  stroke_start_caps.finish();
  stroke_end_caps.finish();
  stroke_softness.finish();
  stroke_point_aspect_ratios.finish();
  stroke_materials.finish();

  return drawing;
}

static void legacy_gpencil_to_grease_pencil(ConversionData &conversion_data,
                                            GreasePencil &grease_pencil,
                                            bGPdata &gpd)
{
  using namespace blender::bke::greasepencil;

  if (gpd.flag & ID_FLAG_FAKEUSER) {
    id_fake_user_set(&grease_pencil.id);
  }

  BLI_assert(!grease_pencil.id.properties);
  if (gpd.id.properties) {
    grease_pencil.id.properties = IDP_CopyProperty(gpd.id.properties);
    grease_pencil.id.system_properties = IDP_CopyProperty(gpd.id.properties);
  }

  /** Convert Grease Pencil data flag. */
  SET_FLAG_FROM_TEST(
      grease_pencil.flag, (gpd.flag & GP_DATA_EXPAND) != 0, GREASE_PENCIL_ANIM_CHANNEL_EXPANDED);
  SET_FLAG_FROM_TEST(grease_pencil.flag,
                     (gpd.flag & GP_DATA_AUTOLOCK_LAYERS) != 0,
                     GREASE_PENCIL_AUTOLOCK_LAYERS);
  SET_FLAG_FROM_TEST(
      grease_pencil.flag, (gpd.draw_mode == GP_DRAWMODE_3D), GREASE_PENCIL_STROKE_ORDER_3D);

  int layer_idx = 0;
  LISTBASE_FOREACH_INDEX (bGPDlayer *, gpl, &gpd.layers, layer_idx) {
    /* Create a new layer. */
    Layer &new_layer = grease_pencil.add_layer(StringRefNull(gpl->info, STRNLEN(gpl->info)));

    /* Flags. */
    new_layer.set_visible((gpl->flag & GP_LAYER_HIDE) == 0);
    new_layer.set_locked((gpl->flag & GP_LAYER_LOCKED) != 0);
    new_layer.set_selected((gpl->flag & GP_LAYER_SELECT) != 0);
    SET_FLAG_FROM_TEST(
        new_layer.base.flag, (gpl->flag & GP_LAYER_FRAMELOCK) != 0, GP_LAYER_TREE_NODE_MUTE);
    SET_FLAG_FROM_TEST(new_layer.base.flag,
                       (gpl->flag & GP_LAYER_USE_LIGHTS) != 0,
                       GP_LAYER_TREE_NODE_USE_LIGHTS);
    SET_FLAG_FROM_TEST(new_layer.base.flag,
                       (gpl->onion_flag & GP_LAYER_ONIONSKIN) == 0,
                       GP_LAYER_TREE_NODE_HIDE_ONION_SKINNING);
    SET_FLAG_FROM_TEST(
        new_layer.base.flag, (gpl->flag & GP_LAYER_USE_MASK) == 0, GP_LAYER_TREE_NODE_HIDE_MASKS);

    /* Copy Dope-sheet channel color. */
    copy_v3_v3(new_layer.base.color, gpl->color);
    new_layer.blend_mode = int8_t(gpl->blend_mode);

    new_layer.parent = gpl->parent;
    new_layer.set_parent_bone_name(gpl->parsubstr);
    /* GPv2 parent inverse matrix is only valid when parent is set. */
    if (gpl->parent) {
      copy_m4_m4(new_layer.parentinv, gpl->inverse);
    }

    copy_v3_v3(new_layer.translation, gpl->location);
    copy_v3_v3(new_layer.rotation, gpl->rotation);
    copy_v3_v3(new_layer.scale, gpl->scale);

    new_layer.set_view_layer_name(gpl->viewlayername);
    SET_FLAG_FROM_TEST(new_layer.base.flag,
                       (gpl->flag & GP_LAYER_DISABLE_MASKS_IN_VIEWLAYER) != 0,
                       GP_LAYER_TREE_NODE_DISABLE_MASKS_IN_VIEWLAYER);

    /* Convert the layer masks. */
    LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl->mask_layers) {
      LayerMask *new_mask = MEM_new<LayerMask>(__func__, mask->name);
      new_mask->flag = mask->flag;
      BLI_addtail(&new_layer.masks, new_mask);
    }
    new_layer.opacity = gpl->opacity;

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      Drawing *dst_drawing = grease_pencil.insert_frame(
          new_layer, gpf->framenum, 0, eBezTriple_KeyframeType(gpf->key_type));
      if (dst_drawing == nullptr) {
        /* Might fail because GPv2 technically allowed overlapping keyframes on the same frame
         * (very unlikely to occur in real world files). In GPv3, keyframes always have to be on
         * different frames. In this case we can't create a keyframe and have to skip it. */
        continue;
      }
      /* Convert the frame to a drawing. */
      *dst_drawing = legacy_gpencil_frame_to_grease_pencil_drawing(*gpf, gpd.vertex_group_names);

      /* This frame was just inserted above, so it should always exist. */
      GreasePencilFrame &new_frame = *new_layer.frame_at(gpf->framenum);
      SET_FLAG_FROM_TEST(new_frame.flag, (gpf->flag & GP_FRAME_SELECT), GP_FRAME_SELECTED);
    }

    if ((gpl->flag & GP_LAYER_ACTIVE) != 0) {
      grease_pencil.set_active_layer(&new_layer);
    }
  }

  /* Second loop, to write to layer attributes after all layers were created. */
  MutableAttributeAccessor layer_attributes = grease_pencil.attributes_for_write();
  /* NOTE: Layer Adjustments like the tint and the radius offsets are deliberately ignored here!
   * These are converted to modifiers at the bottom of the stack to keep visual compatibility with
   * GPv2. */
  SpanAttributeWriter<int> layer_passes = layer_attributes.lookup_or_add_for_write_span<int>(
      "pass_index", bke::AttrDomain::Layer);

  layer_idx = 0;
  LISTBASE_FOREACH_INDEX (bGPDlayer *, gpl, &gpd.layers, layer_idx) {
    layer_passes.span[layer_idx] = int(gpl->pass_index);
  }

  layer_passes.finish();

  /* Copy vertex group names and settings. */
  BKE_defgroup_copy_list(&grease_pencil.vertex_group_names, &gpd.vertex_group_names);
  grease_pencil.vertex_group_active_index = gpd.vertex_group_active_index;

  /* Convert the onion skinning settings. */
  GreasePencilOnionSkinningSettings &settings = grease_pencil.onion_skinning_settings;
  settings.opacity = gpd.onion_factor;
  settings.mode = gpd.onion_mode;
  SET_FLAG_FROM_TEST(settings.flag,
                     ((gpd.onion_flag & GP_ONION_GHOST_PREVCOL) != 0 &&
                      (gpd.onion_flag & GP_ONION_GHOST_NEXTCOL) != 0),
                     GP_ONION_SKINNING_USE_CUSTOM_COLORS);
  SET_FLAG_FROM_TEST(
      settings.flag, (gpd.onion_flag & GP_ONION_FADE) != 0, GP_ONION_SKINNING_USE_FADE);
  SET_FLAG_FROM_TEST(
      settings.flag, (gpd.onion_flag & GP_ONION_LOOP) != 0, GP_ONION_SKINNING_SHOW_LOOP);
  /* Convert keytype filter to a bit flag. */
  if (gpd.onion_keytype == -1) {
    settings.filter = GREASE_PENCIL_ONION_SKINNING_FILTER_ALL;
  }
  else {
    settings.filter = (1 << gpd.onion_keytype);
  }
  settings.num_frames_before = gpd.gstep;
  settings.num_frames_after = gpd.gstep_next;
  copy_v3_v3(settings.color_before, gpd.gcolor_prev);
  copy_v3_v3(settings.color_after, gpd.gcolor_next);

  BKE_id_materials_copy(&conversion_data.bmain, &gpd.id, &grease_pencil.id);

  /* Copy animation data from legacy GP data.
   *
   * Note that currently, Actions IDs are not duplicated. They may be needed ultimately, but for
   * the time being, assuming invalid fcurves/drivers are fine here. */
  if (AnimData *gpd_animdata = BKE_animdata_from_id(&gpd.id)) {
    grease_pencil.adt = BKE_animdata_copy_in_lib(
        &conversion_data.bmain, gpd.id.lib, gpd_animdata, LIB_ID_COPY_DEFAULT);

    /* Some property was renamed between legacy GP layers and new GreasePencil ones. */
    AnimDataConvertor animdata_gpdata_transfer(
        conversion_data, grease_pencil.id, gpd.id, {{".location", ".translation"}});
    for (const Layer *layer_iter : grease_pencil.layers()) {
      /* Data comes from versioned GPv2 layers, which have a fixed max length. */
      char layer_name_esc[sizeof((bGPDlayer{}).info) * 2];
      BLI_str_escape(layer_name_esc, layer_iter->name().c_str(), sizeof(layer_name_esc));
      std::string layer_root_path = fmt::format("layers[\"{}\"]", layer_name_esc);
      animdata_gpdata_transfer.root_path_dst = layer_root_path;
      animdata_gpdata_transfer.root_path_src = layer_root_path;
      animdata_gpdata_transfer.fcurves_convert();
    }
    animdata_gpdata_transfer.fcurves_convert_finalize();
  }
}

constexpr const char *OFFSET_RADIUS_NODETREE_NAME = "Offset Radius GPv3 Conversion";
static bNodeTree *offset_radius_node_tree_add(ConversionData &conversion_data, Library *library)
{
  using namespace blender;
  /* NOTE: DO NOT translate this ID name, it is used to find a potentially already existing
   * node-tree. */
  bNodeTree *group = bke::node_tree_add_in_lib(
      &conversion_data.bmain, library, OFFSET_RADIUS_NODETREE_NAME, "GeometryNodeTree");

  if (!group->geometry_node_asset_traits) {
    group->geometry_node_asset_traits = MEM_callocN<GeometryNodeAssetTraits>(__func__);
  }
  group->geometry_node_asset_traits->flag |= GEO_NODE_ASSET_MODIFIER;

  group->tree_interface.add_socket(
      DATA_("Geometry"), "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  group->tree_interface.add_socket(
      DATA_("Geometry"), "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_OUTPUT, nullptr);

  bNodeTreeInterfaceSocket *radius_offset = group->tree_interface.add_socket(
      DATA_("Offset"), "", "NodeSocketFloat", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  auto &radius_offset_data = *static_cast<bNodeSocketValueFloat *>(radius_offset->socket_data);
  radius_offset_data.subtype = PROP_DISTANCE;
  radius_offset_data.min = -FLT_MAX;
  radius_offset_data.max = FLT_MAX;

  group->tree_interface.add_socket(
      DATA_("Layer"), "", "NodeSocketString", NODE_INTERFACE_SOCKET_INPUT, nullptr);

  bNode *group_output = bke::node_add_node(nullptr, *group, "NodeGroupOutput");
  group_output->location[0] = 800;
  group_output->location[1] = 160;
  bNode *group_input = bke::node_add_node(nullptr, *group, "NodeGroupInput");
  group_input->location[0] = 0;
  group_input->location[1] = 160;

  bNode *set_curve_radius = bke::node_add_node(nullptr, *group, "GeometryNodeSetCurveRadius");
  set_curve_radius->location[0] = 600;
  set_curve_radius->location[1] = 160;
  bNode *named_layer_selection = bke::node_add_node(
      nullptr, *group, "GeometryNodeInputNamedLayerSelection");
  named_layer_selection->location[0] = 200;
  named_layer_selection->location[1] = 100;
  bNode *input_radius = bke::node_add_node(nullptr, *group, "GeometryNodeInputRadius");
  input_radius->location[0] = 0;
  input_radius->location[1] = 0;

  bNode *add = bke::node_add_node(nullptr, *group, "ShaderNodeMath");
  add->custom1 = NODE_MATH_ADD;
  add->location[0] = 200;
  add->location[1] = 0;

  bNode *clamp_radius = bke::node_add_node(nullptr, *group, "ShaderNodeClamp");
  clamp_radius->location[0] = 400;
  clamp_radius->location[1] = 0;
  bNodeSocket *sock_max = bke::node_find_socket(*clamp_radius, SOCK_IN, "Max");
  static_cast<bNodeSocketValueFloat *>(sock_max->default_value)->value = FLT_MAX;

  bke::node_add_link(*group,
                     *group_input,
                     *bke::node_find_socket(*group_input, SOCK_OUT, "Socket_0"),
                     *set_curve_radius,
                     *bke::node_find_socket(*set_curve_radius, SOCK_IN, "Curve"));
  bke::node_add_link(*group,
                     *set_curve_radius,
                     *bke::node_find_socket(*set_curve_radius, SOCK_OUT, "Curve"),
                     *group_output,
                     *bke::node_find_socket(*group_output, SOCK_IN, "Socket_1"));

  bke::node_add_link(*group,
                     *group_input,
                     *bke::node_find_socket(*group_input, SOCK_OUT, "Socket_3"),
                     *named_layer_selection,
                     *bke::node_find_socket(*named_layer_selection, SOCK_IN, "Name"));
  bke::node_add_link(*group,
                     *named_layer_selection,
                     *bke::node_find_socket(*named_layer_selection, SOCK_OUT, "Selection"),
                     *set_curve_radius,
                     *bke::node_find_socket(*set_curve_radius, SOCK_IN, "Selection"));

  bke::node_add_link(*group,
                     *group_input,
                     *bke::node_find_socket(*group_input, SOCK_OUT, "Socket_2"),
                     *add,
                     *bke::node_find_socket(*add, SOCK_IN, "Value"));
  bke::node_add_link(*group,
                     *input_radius,
                     *bke::node_find_socket(*input_radius, SOCK_OUT, "Radius"),
                     *add,
                     *bke::node_find_socket(*add, SOCK_IN, "Value_001"));
  bke::node_add_link(*group,
                     *add,
                     *bke::node_find_socket(*add, SOCK_OUT, "Value"),
                     *clamp_radius,
                     *bke::node_find_socket(*clamp_radius, SOCK_IN, "Value"));
  bke::node_add_link(*group,
                     *clamp_radius,
                     *bke::node_find_socket(*clamp_radius, SOCK_OUT, "Result"),
                     *set_curve_radius,
                     *bke::node_find_socket(*set_curve_radius, SOCK_IN, "Radius"));

  LISTBASE_FOREACH (bNode *, node, &group->nodes) {
    bke::node_set_selected(*node, false);
  }

  return group;
}

static void thickness_factor_to_modifier(ConversionData &conversion_data,
                                         bGPdata &src_object_data,
                                         Object &dst_object)
{
  AnimDataConvertor animdata_thickness_transfer(
      conversion_data, dst_object.id, src_object_data.id, {{"pixel_factor", ".thickness_factor"}});
  animdata_thickness_transfer.root_path_src = "";

  const float thickness_factor = src_object_data.pixfactor;
  const bool has_thickness_factor_animation =
      animdata_thickness_transfer.source_has_animation_to_convert();
  const bool has_thickness_factor = thickness_factor != 1.0f || has_thickness_factor_animation;

  if (!has_thickness_factor) {
    return;
  }

  ModifierData *md = BKE_modifier_new(eModifierType_GreasePencilThickness);
  GreasePencilThickModifierData *tmd = reinterpret_cast<GreasePencilThickModifierData *>(md);

  tmd->thickness_fac = thickness_factor;

  STRNCPY_UTF8(md->name, DATA_("Thickness"));
  BKE_modifier_unique_name(&dst_object.modifiers, md);

  BLI_addtail(&dst_object.modifiers, md);
  BKE_modifiers_persistent_uid_init(dst_object, *md);

  if (has_thickness_factor_animation) {
    char modifier_name_esc[MAX_NAME * 2];
    BLI_str_escape(modifier_name_esc, md->name, sizeof(modifier_name_esc));
    animdata_thickness_transfer.root_path_dst = fmt::format("modifiers[\"{}\"]",
                                                            modifier_name_esc);

    animdata_thickness_transfer.fcurves_convert();
  }

  animdata_thickness_transfer.fcurves_convert_finalize();
}

static void fcurve_convert_thickness_cb(FCurve &fcurve)
{
  if (fcurve.bezt) {
    for (uint i = 0; i < fcurve.totvert; i++) {
      BezTriple &bezier_triple = fcurve.bezt[i];
      bezier_triple.vec[0][1] *= LEGACY_RADIUS_CONVERSION_FACTOR;
      bezier_triple.vec[1][1] *= LEGACY_RADIUS_CONVERSION_FACTOR;
      bezier_triple.vec[2][1] *= LEGACY_RADIUS_CONVERSION_FACTOR;
    }
  }
  if (fcurve.fpt) {
    for (uint i = 0; i < fcurve.totvert; i++) {
      FPoint &fpoint = fcurve.fpt[i];
      fpoint.vec[1] *= LEGACY_RADIUS_CONVERSION_FACTOR;
    }
  }
  fcurve.flag &= ~FCURVE_INT_VALUES;
  BKE_fcurve_handles_recalc(&fcurve);
}

static void legacy_object_thickness_modifier_thickness_anim(ConversionData &conversion_data,
                                                            Object &object)
{
  if (BKE_animdata_from_id(&object.id) == nullptr) {
    return;
  }

  /* NOTE: At this point, the animation was already transferred to the destination object. Now we
   * just need to convert the fcurve data to be in the right space. */
  AnimDataConvertor animdata_convert_thickness(
      conversion_data,
      object.id,
      object.id,
      {{".thickness", ".thickness", fcurve_convert_thickness_cb}});

  LISTBASE_FOREACH (ModifierData *, tmd, &object.modifiers) {
    if (ModifierType(tmd->type) != eModifierType_GreasePencilThickness) {
      continue;
    }

    char modifier_name[MAX_NAME * 2];
    BLI_str_escape(modifier_name, tmd->name, sizeof(modifier_name));
    animdata_convert_thickness.root_path_src = fmt::format("modifiers[\"{}\"]", modifier_name);
    animdata_convert_thickness.root_path_dst = fmt::format("modifiers[\"{}\"]", modifier_name);

    if (!animdata_convert_thickness.source_has_animation_to_convert()) {
      continue;
    }
    animdata_convert_thickness.fcurves_convert();
  }

  animdata_convert_thickness.fcurves_convert_finalize();
  DEG_relations_tag_update(&conversion_data.bmain);
}

static void layer_adjustments_to_modifiers(ConversionData &conversion_data,
                                           bGPdata &src_object_data,
                                           Object &dst_object)
{
  /* Handling of animation here is a bit complex, since paths needs to be updated, but also
   * FCurves need to be transferred from legacy GPData animation to Object animation. */
  AnimDataConvertor animdata_tint_transfer(
      conversion_data,
      dst_object.id,
      src_object_data.id,
      {{".tint_color", ".color"}, {".tint_factor", ".factor"}});

  AnimDataConvertor animdata_thickness_transfer(
      conversion_data,
      dst_object.id,
      src_object_data.id,
      {{".line_change", "[\"Socket_2\"]", fcurve_convert_thickness_cb}});

  /* Replace layer adjustments with modifiers. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &src_object_data.layers) {
    const float3 tint_color = float3(gpl->tintcolor);
    const float tint_factor = gpl->tintcolor[3];
    const int thickness_px = gpl->line_change;

    char layer_name_esc[sizeof(gpl->info) * 2];
    BLI_str_escape(layer_name_esc, gpl->info, sizeof(layer_name_esc));
    animdata_tint_transfer.root_path_src = fmt::format("layers[\"{}\"]", layer_name_esc);
    animdata_thickness_transfer.root_path_src = fmt::format("layers[\"{}\"]", layer_name_esc);

    const bool has_tint_adjustment_animation =
        animdata_tint_transfer.source_has_animation_to_convert();
    const bool has_thickness_adjustment_animation =
        animdata_thickness_transfer.source_has_animation_to_convert();

    /* If tint or thickness are animated, relevant modifiers also need to be created. */
    const bool has_tint_adjustment = tint_factor > 0.0f || has_tint_adjustment_animation;
    const bool has_thickness_adjustment = thickness_px != 0 || has_thickness_adjustment_animation;

    /* Tint adjustment. */
    if (has_tint_adjustment) {
      ModifierData *md = BKE_modifier_new(eModifierType_GreasePencilTint);
      GreasePencilTintModifierData *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);

      copy_v3_v3(tmd->color, tint_color);
      tmd->factor = tint_factor;
      STRNCPY_UTF8(tmd->influence.layer_name, gpl->info);

      char modifier_name[MAX_NAME];
      SNPRINTF_UTF8(modifier_name, "Tint %s", gpl->info);
      STRNCPY_UTF8(md->name, modifier_name);
      BKE_modifier_unique_name(&dst_object.modifiers, md);

      BLI_addtail(&dst_object.modifiers, md);
      BKE_modifiers_persistent_uid_init(dst_object, *md);

      if (has_tint_adjustment_animation) {
        char modifier_name_esc[MAX_NAME * 2];
        BLI_str_escape(modifier_name_esc, md->name, sizeof(modifier_name_esc));
        animdata_tint_transfer.root_path_dst = fmt::format("modifiers[\"{}\"]", modifier_name_esc);

        animdata_tint_transfer.fcurves_convert();
      }
    }
    /* Thickness adjustment. */
    if (has_thickness_adjustment) {
      /* Convert the "pixel" offset value into a radius value.
       * GPv2 used a conversion of 1 "px" = 0.001. */
      /* NOTE: this offset may be negative. */
      const float uniform_object_scale = math::average(float3(dst_object.scale));
      const float radius_offset = math::safe_divide(
          float(thickness_px) * LEGACY_RADIUS_CONVERSION_FACTOR, uniform_object_scale);

      const auto offset_radius_ntree_ensure = [&](Library *owner_library) {
        if (bNodeTree **ntree = conversion_data.offset_radius_ntree_by_library.lookup_ptr(
                owner_library))
        {
          /* Node tree has already been found/created for this versioning call. */
          return *ntree;
        }
        /* Try to find an existing group added by previous versioning to avoid adding duplicates.
         */
        LISTBASE_FOREACH (bNodeTree *, ntree_iter, &conversion_data.bmain.nodetrees) {
          if (ntree_iter->id.lib != owner_library) {
            continue;
          }
          if (STREQ(ntree_iter->id.name + 2, OFFSET_RADIUS_NODETREE_NAME)) {
            conversion_data.offset_radius_ntree_by_library.add_new(owner_library, ntree_iter);
            return ntree_iter;
          }
        }
        bNodeTree *new_ntree = offset_radius_node_tree_add(conversion_data, owner_library);
        /* Remove the default user. The count is tracked manually when assigning to modifiers. */
        id_us_min(&new_ntree->id);
        conversion_data.offset_radius_ntree_by_library.add_new(owner_library, new_ntree);
        BKE_ntree_update_after_single_tree_change(conversion_data.bmain, *new_ntree);
        return new_ntree;
      };
      bNodeTree *offset_radius_node_tree = offset_radius_ntree_ensure(dst_object.id.lib);

      auto *md = reinterpret_cast<NodesModifierData *>(BKE_modifier_new(eModifierType_Nodes));

      char modifier_name[MAX_NAME];
      SNPRINTF_UTF8(modifier_name, "Thickness %s", gpl->info);
      STRNCPY_UTF8(md->modifier.name, modifier_name);
      BKE_modifier_unique_name(&dst_object.modifiers, &md->modifier);
      md->node_group = offset_radius_node_tree;

      BLI_addtail(&dst_object.modifiers, md);
      BKE_modifiers_persistent_uid_init(dst_object, md->modifier);

      md->settings.properties = bke::idprop::create_group("Nodes Modifier Settings").release();
      IDProperty *radius_offset_prop =
          bke::idprop::create(DATA_("Socket_2"), radius_offset).release();
      auto *ui_data = reinterpret_cast<IDPropertyUIDataFloat *>(
          IDP_ui_data_ensure(radius_offset_prop));
      ui_data->soft_min = 0.0;
      ui_data->base.rna_subtype = PROP_TRANSLATION;
      IDP_AddToGroup(md->settings.properties, radius_offset_prop);
      IDP_AddToGroup(md->settings.properties,
                     bke::idprop::create(DATA_("Socket_3"), gpl->info).release());

      if (has_thickness_adjustment_animation) {
        char modifier_name_esc[MAX_NAME * 2];
        BLI_str_escape(modifier_name_esc, md->modifier.name, sizeof(modifier_name_esc));
        animdata_thickness_transfer.root_path_dst = fmt::format("modifiers[\"{}\"]",
                                                                modifier_name_esc);

        animdata_thickness_transfer.fcurves_convert();
      }
    }
  }

  animdata_tint_transfer.fcurves_convert_finalize();
  animdata_thickness_transfer.fcurves_convert_finalize();

  DEG_relations_tag_update(&conversion_data.bmain);
}

static ModifierData &legacy_object_modifier_common(ConversionData &conversion_data,
                                                   Object &object,
                                                   const ModifierType type,
                                                   GpencilModifierData &legacy_md)
{
  /* TODO: Copy of most of #ed::object::modifier_add, this should be a BKE_modifiers function
   * actually. */
  const ModifierTypeInfo *mti = BKE_modifier_get_info(type);

  ModifierData &new_md = *BKE_modifier_new(type);

  if (mti->flags & eModifierTypeFlag_RequiresOriginalData) {
    ModifierData *md;
    for (md = static_cast<ModifierData *>(object.modifiers.first);
         md && BKE_modifier_get_info(ModifierType(md->type))->type == ModifierTypeType::OnlyDeform;
         md = md->next)
    {
      ;
    }
    BLI_insertlinkbefore(&object.modifiers, md, &new_md);
  }
  else {
    BLI_addtail(&object.modifiers, &new_md);
  }

  /* Generate new persistent UID and best possible unique name. */
  BKE_modifiers_persistent_uid_init(object, new_md);
  if (legacy_md.name[0]) {
    STRNCPY_UTF8(new_md.name, legacy_md.name);
  }
  BKE_modifier_unique_name(&object.modifiers, &new_md);

  /* Handle common modifier data. */
  new_md.mode = legacy_md.mode;
  new_md.flag |= legacy_md.flag & (eModifierFlag_OverrideLibrary_Local | eModifierFlag_Active);

  /* Attempt to copy UI state (panels) as best as possible. */
  new_md.ui_expand_flag = legacy_md.ui_expand_flag;

  /* Convert animation data if needed. */
  if (BKE_animdata_from_id(&object.id)) {
    AnimDataConvertor anim_convertor(conversion_data, object.id);

    char legacy_name_esc[MAX_NAME * 2];
    BLI_str_escape(legacy_name_esc, legacy_md.name, sizeof(legacy_name_esc));
    anim_convertor.root_path_src = fmt::format("grease_pencil_modifiers[\"{}\"]", legacy_name_esc);

    char new_name_esc[MAX_NAME * 2];
    BLI_str_escape(new_name_esc, new_md.name, sizeof(new_name_esc));
    anim_convertor.root_path_dst = fmt::format("modifiers[\"{}\"]", new_name_esc);

    anim_convertor.fcurves_convert();
    anim_convertor.fcurves_convert_finalize();
  }

  return new_md;
}

static void legacy_object_modifier_influence(GreasePencilModifierInfluenceData &influence,
                                             StringRef layername,
                                             const int layer_pass,
                                             const bool invert_layer,
                                             const bool invert_layer_pass,
                                             Material **material,
                                             const int material_pass,
                                             const bool invert_material,
                                             const bool invert_material_pass,
                                             StringRef vertex_group_name,
                                             const bool invert_vertex_group,
                                             CurveMapping **custom_curve,
                                             const bool use_custom_curve)
{
  influence.flag = 0;

  layername.copy_utf8_truncated(influence.layer_name);
  if (invert_layer) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_LAYER_FILTER;
  }
  influence.layer_pass = layer_pass;
  if (layer_pass > 0) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_USE_LAYER_PASS_FILTER;
  }
  if (invert_layer_pass) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_LAYER_PASS_FILTER;
  }

  if (material) {
    influence.material = *material;
    *material = nullptr;
  }
  if (invert_material) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_FILTER;
  }
  influence.material_pass = material_pass;
  if (material_pass > 0) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_USE_MATERIAL_PASS_FILTER;
  }
  if (invert_material_pass) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_PASS_FILTER;
  }

  vertex_group_name.copy_utf8_truncated(influence.vertex_group_name);
  if (invert_vertex_group) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP;
  }

  if (custom_curve) {
    if (influence.custom_curve) {
      BKE_curvemapping_free(influence.custom_curve);
    }
    influence.custom_curve = *custom_curve;
    *custom_curve = nullptr;
  }
  if (use_custom_curve) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_USE_CUSTOM_CURVE;
  }
}

static void legacy_object_modifier_armature(ConversionData &conversion_data,
                                            Object &object,
                                            GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilArmature, legacy_md);
  auto &md_armature = reinterpret_cast<GreasePencilArmatureModifierData &>(md);
  auto &legacy_md_armature = reinterpret_cast<ArmatureGpencilModifierData &>(legacy_md);

  md_armature.object = legacy_md_armature.object;
  legacy_md_armature.object = nullptr;
  md_armature.deformflag = legacy_md_armature.deformflag;

  legacy_object_modifier_influence(md_armature.influence,
                                   "",
                                   0,
                                   false,
                                   false,
                                   nullptr,
                                   0,
                                   false,
                                   false,
                                   legacy_md_armature.vgname,
                                   legacy_md_armature.deformflag & ARM_DEF_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_array(ConversionData &conversion_data,
                                         Object &object,
                                         GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilArray, legacy_md);
  auto &md_array = reinterpret_cast<GreasePencilArrayModifierData &>(md);
  auto &legacy_md_array = reinterpret_cast<ArrayGpencilModifierData &>(legacy_md);

  md_array.object = legacy_md_array.object;
  legacy_md_array.object = nullptr;
  md_array.count = legacy_md_array.count;
  md_array.flag = 0;
  if (legacy_md_array.flag & GP_ARRAY_UNIFORM_RANDOM_SCALE) {
    md_array.flag |= MOD_GREASE_PENCIL_ARRAY_UNIFORM_RANDOM_SCALE;
  }
  if (legacy_md_array.flag & GP_ARRAY_USE_OB_OFFSET) {
    md_array.flag |= MOD_GREASE_PENCIL_ARRAY_USE_OB_OFFSET;
  }
  if (legacy_md_array.flag & GP_ARRAY_USE_OFFSET) {
    md_array.flag |= MOD_GREASE_PENCIL_ARRAY_USE_OFFSET;
  }
  if (legacy_md_array.flag & GP_ARRAY_USE_RELATIVE) {
    md_array.flag |= MOD_GREASE_PENCIL_ARRAY_USE_RELATIVE;
  }
  copy_v3_v3(md_array.offset, legacy_md_array.offset);
  copy_v3_v3(md_array.shift, legacy_md_array.shift);
  copy_v3_v3(md_array.rnd_offset, legacy_md_array.rnd_offset);
  copy_v3_v3(md_array.rnd_rot, legacy_md_array.rnd_rot);
  copy_v3_v3(md_array.rnd_scale, legacy_md_array.rnd_scale);
  md_array.seed = legacy_md_array.seed;
  md_array.mat_rpl = legacy_md_array.mat_rpl;

  legacy_object_modifier_influence(md_array.influence,
                                   legacy_md_array.layername,
                                   legacy_md_array.layer_pass,
                                   legacy_md_array.flag & GP_ARRAY_INVERT_LAYER,
                                   legacy_md_array.flag & GP_ARRAY_INVERT_LAYERPASS,
                                   &legacy_md_array.material,
                                   legacy_md_array.pass_index,
                                   legacy_md_array.flag & GP_ARRAY_INVERT_MATERIAL,
                                   legacy_md_array.flag & GP_ARRAY_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_color(ConversionData &conversion_data,
                                         Object &object,
                                         GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilColor, legacy_md);
  auto &md_color = reinterpret_cast<GreasePencilColorModifierData &>(md);
  auto &legacy_md_color = reinterpret_cast<ColorGpencilModifierData &>(legacy_md);

  switch (eModifyColorGpencil_Flag(legacy_md_color.modify_color)) {
    case GP_MODIFY_COLOR_BOTH:
      md_color.color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
      break;
    case GP_MODIFY_COLOR_STROKE:
      md_color.color_mode = MOD_GREASE_PENCIL_COLOR_STROKE;
      break;
    case GP_MODIFY_COLOR_FILL:
      md_color.color_mode = MOD_GREASE_PENCIL_COLOR_FILL;
      break;
    case GP_MODIFY_COLOR_HARDNESS:
      md_color.color_mode = MOD_GREASE_PENCIL_COLOR_HARDNESS;
      break;
  }
  copy_v3_v3(md_color.hsv, legacy_md_color.hsv);

  legacy_object_modifier_influence(md_color.influence,
                                   legacy_md_color.layername,
                                   legacy_md_color.layer_pass,
                                   legacy_md_color.flag & GP_COLOR_INVERT_LAYER,
                                   legacy_md_color.flag & GP_COLOR_INVERT_LAYERPASS,
                                   &legacy_md_color.material,
                                   legacy_md_color.pass_index,
                                   legacy_md_color.flag & GP_COLOR_INVERT_MATERIAL,
                                   legacy_md_color.flag & GP_COLOR_INVERT_PASS,
                                   "",
                                   false,
                                   &legacy_md_color.curve_intensity,
                                   legacy_md_color.flag & GP_COLOR_CUSTOM_CURVE);
}

static void legacy_object_modifier_dash(ConversionData &conversion_data,
                                        Object &object,
                                        GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilDash, legacy_md);
  auto &md_dash = reinterpret_cast<GreasePencilDashModifierData &>(md);
  auto &legacy_md_dash = reinterpret_cast<DashGpencilModifierData &>(legacy_md);

  md_dash.dash_offset = legacy_md_dash.dash_offset;
  md_dash.segment_active_index = legacy_md_dash.segment_active_index;
  md_dash.segments_num = legacy_md_dash.segments_len;
  MEM_SAFE_FREE(md_dash.segments_array);
  md_dash.segments_array = MEM_calloc_arrayN<GreasePencilDashModifierSegment>(
      legacy_md_dash.segments_len, __func__);
  for (const int i : IndexRange(md_dash.segments_num)) {
    GreasePencilDashModifierSegment &dst_segment = md_dash.segments_array[i];
    const DashGpencilModifierSegment &src_segment = legacy_md_dash.segments[i];
    STRNCPY(dst_segment.name, src_segment.name);
    dst_segment.flag = 0;
    if (src_segment.flag & GP_DASH_USE_CYCLIC) {
      dst_segment.flag |= MOD_GREASE_PENCIL_DASH_USE_CYCLIC;
    }
    dst_segment.dash = src_segment.dash;
    dst_segment.gap = src_segment.gap;
    dst_segment.opacity = src_segment.opacity;
    dst_segment.radius = src_segment.radius;
    dst_segment.mat_nr = src_segment.mat_nr;
  }

  legacy_object_modifier_influence(md_dash.influence,
                                   legacy_md_dash.layername,
                                   legacy_md_dash.layer_pass,
                                   legacy_md_dash.flag & GP_DASH_INVERT_LAYER,
                                   legacy_md_dash.flag & GP_DASH_INVERT_LAYERPASS,
                                   &legacy_md_dash.material,
                                   legacy_md_dash.pass_index,
                                   legacy_md_dash.flag & GP_DASH_INVERT_MATERIAL,
                                   legacy_md_dash.flag & GP_DASH_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_envelope(ConversionData &conversion_data,
                                            Object &object,
                                            GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilEnvelope, legacy_md);
  auto &md_envelope = reinterpret_cast<GreasePencilEnvelopeModifierData &>(md);
  auto &legacy_md_envelope = reinterpret_cast<EnvelopeGpencilModifierData &>(legacy_md);

  switch (eEnvelopeGpencil_Mode(legacy_md_envelope.mode)) {
    case GP_ENVELOPE_DEFORM:
      md_envelope.mode = MOD_GREASE_PENCIL_ENVELOPE_DEFORM;
      break;
    case GP_ENVELOPE_SEGMENTS:
      md_envelope.mode = MOD_GREASE_PENCIL_ENVELOPE_SEGMENTS;
      break;
    case GP_ENVELOPE_FILLS:
      md_envelope.mode = MOD_GREASE_PENCIL_ENVELOPE_FILLS;
      break;
  }
  md_envelope.mat_nr = legacy_md_envelope.mat_nr;
  md_envelope.thickness = legacy_md_envelope.thickness;
  md_envelope.strength = legacy_md_envelope.strength;
  md_envelope.skip = legacy_md_envelope.skip;
  md_envelope.spread = legacy_md_envelope.spread;

  legacy_object_modifier_influence(md_envelope.influence,
                                   legacy_md_envelope.layername,
                                   legacy_md_envelope.layer_pass,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_LAYER,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_LAYERPASS,
                                   &legacy_md_envelope.material,
                                   legacy_md_envelope.pass_index,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_MATERIAL,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_PASS,
                                   legacy_md_envelope.vgname,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_hook(ConversionData &conversion_data,
                                        Object &object,
                                        GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilHook, legacy_md);
  auto &md_hook = reinterpret_cast<GreasePencilHookModifierData &>(md);
  auto &legacy_md_hook = reinterpret_cast<HookGpencilModifierData &>(legacy_md);

  md_hook.flag = 0;
  if (legacy_md_hook.flag & GP_HOOK_UNIFORM_SPACE) {
    md_hook.flag |= MOD_GREASE_PENCIL_HOOK_UNIFORM_SPACE;
  }
  switch (eHookGpencil_Falloff(legacy_md_hook.falloff_type)) {
    case eGPHook_Falloff_None:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_None;
      break;
    case eGPHook_Falloff_Curve:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Curve;
      break;
    case eGPHook_Falloff_Sharp:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Sharp;
      break;
    case eGPHook_Falloff_Smooth:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Smooth;
      break;
    case eGPHook_Falloff_Root:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Root;
      break;
    case eGPHook_Falloff_Linear:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Linear;
      break;
    case eGPHook_Falloff_Const:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Const;
      break;
    case eGPHook_Falloff_Sphere:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Sphere;
      break;
    case eGPHook_Falloff_InvSquare:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_InvSquare;
      break;
  }
  md_hook.object = legacy_md_hook.object;
  legacy_md_hook.object = nullptr;
  STRNCPY(md_hook.subtarget, legacy_md_hook.subtarget);
  copy_m4_m4(md_hook.parentinv, legacy_md_hook.parentinv);
  copy_v3_v3(md_hook.cent, legacy_md_hook.cent);
  md_hook.falloff = legacy_md_hook.falloff;
  md_hook.force = legacy_md_hook.force;

  legacy_object_modifier_influence(md_hook.influence,
                                   legacy_md_hook.layername,
                                   legacy_md_hook.layer_pass,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_LAYER,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_LAYERPASS,
                                   &legacy_md_hook.material,
                                   legacy_md_hook.pass_index,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_MATERIAL,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_PASS,
                                   legacy_md_hook.vgname,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_VGROUP,
                                   &legacy_md_hook.curfalloff,
                                   true);
}

static void legacy_object_modifier_lattice(ConversionData &conversion_data,
                                           Object &object,
                                           GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilLattice, legacy_md);
  auto &md_lattice = reinterpret_cast<GreasePencilLatticeModifierData &>(md);
  auto &legacy_md_lattice = reinterpret_cast<LatticeGpencilModifierData &>(legacy_md);

  md_lattice.object = legacy_md_lattice.object;
  legacy_md_lattice.object = nullptr;
  md_lattice.strength = legacy_md_lattice.strength;

  legacy_object_modifier_influence(md_lattice.influence,
                                   legacy_md_lattice.layername,
                                   legacy_md_lattice.layer_pass,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_LAYER,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_LAYERPASS,
                                   &legacy_md_lattice.material,
                                   legacy_md_lattice.pass_index,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_MATERIAL,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_PASS,
                                   legacy_md_lattice.vgname,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_length(ConversionData &conversion_data,
                                          Object &object,
                                          GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilLength, legacy_md);
  auto &md_length = reinterpret_cast<GreasePencilLengthModifierData &>(md);
  auto &legacy_md_length = reinterpret_cast<LengthGpencilModifierData &>(legacy_md);

  md_length.flag = legacy_md_length.flag;
  md_length.start_fac = legacy_md_length.start_fac;
  md_length.end_fac = legacy_md_length.end_fac;
  md_length.rand_start_fac = legacy_md_length.rand_start_fac;
  md_length.rand_end_fac = legacy_md_length.rand_end_fac;
  md_length.rand_offset = legacy_md_length.rand_offset;
  md_length.overshoot_fac = legacy_md_length.overshoot_fac;
  md_length.seed = legacy_md_length.seed;
  md_length.step = legacy_md_length.step;
  md_length.mode = legacy_md_length.mode;
  md_length.point_density = legacy_md_length.point_density;
  md_length.segment_influence = legacy_md_length.segment_influence;
  md_length.max_angle = legacy_md_length.max_angle;

  legacy_object_modifier_influence(md_length.influence,
                                   legacy_md_length.layername,
                                   legacy_md_length.layer_pass,
                                   legacy_md_length.flag & GP_LENGTH_INVERT_LAYER,
                                   legacy_md_length.flag & GP_LENGTH_INVERT_LAYERPASS,
                                   &legacy_md_length.material,
                                   legacy_md_length.pass_index,
                                   legacy_md_length.flag & GP_LENGTH_INVERT_MATERIAL,
                                   legacy_md_length.flag & GP_LENGTH_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_mirror(ConversionData &conversion_data,
                                          Object &object,
                                          GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilMirror, legacy_md);
  auto &md_mirror = reinterpret_cast<GreasePencilMirrorModifierData &>(md);
  auto &legacy_md_mirror = reinterpret_cast<MirrorGpencilModifierData &>(legacy_md);

  md_mirror.object = legacy_md_mirror.object;
  legacy_md_mirror.object = nullptr;
  md_mirror.flag = 0;
  if (legacy_md_mirror.flag & GP_MIRROR_AXIS_X) {
    md_mirror.flag |= MOD_GREASE_PENCIL_MIRROR_AXIS_X;
  }
  if (legacy_md_mirror.flag & GP_MIRROR_AXIS_Y) {
    md_mirror.flag |= MOD_GREASE_PENCIL_MIRROR_AXIS_Y;
  }
  if (legacy_md_mirror.flag & GP_MIRROR_AXIS_Z) {
    md_mirror.flag |= MOD_GREASE_PENCIL_MIRROR_AXIS_Z;
  }

  legacy_object_modifier_influence(md_mirror.influence,
                                   legacy_md_mirror.layername,
                                   legacy_md_mirror.layer_pass,
                                   legacy_md_mirror.flag & GP_MIRROR_INVERT_LAYER,
                                   legacy_md_mirror.flag & GP_MIRROR_INVERT_LAYERPASS,
                                   &legacy_md_mirror.material,
                                   legacy_md_mirror.pass_index,
                                   legacy_md_mirror.flag & GP_MIRROR_INVERT_MATERIAL,
                                   legacy_md_mirror.flag & GP_MIRROR_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_multiply(ConversionData &conversion_data,
                                            Object &object,
                                            GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilMultiply, legacy_md);
  auto &md_multiply = reinterpret_cast<GreasePencilMultiModifierData &>(md);
  auto &legacy_md_multiply = reinterpret_cast<MultiplyGpencilModifierData &>(legacy_md);

  md_multiply.flag = 0;
  if (legacy_md_multiply.flags & GP_MULTIPLY_ENABLE_FADING) {
    md_multiply.flag |= MOD_GREASE_PENCIL_MULTIPLY_ENABLE_FADING;
  }
  md_multiply.duplications = legacy_md_multiply.duplications;
  md_multiply.distance = legacy_md_multiply.distance;
  md_multiply.offset = legacy_md_multiply.offset;
  md_multiply.fading_center = legacy_md_multiply.fading_center;
  md_multiply.fading_thickness = legacy_md_multiply.fading_thickness;
  md_multiply.fading_opacity = legacy_md_multiply.fading_opacity;

  /* NOTE: This looks wrong, but GPv2 version uses Mirror modifier flags in its `flag` property
   * and own flags in its `flags` property. */
  legacy_object_modifier_influence(md_multiply.influence,
                                   legacy_md_multiply.layername,
                                   legacy_md_multiply.layer_pass,
                                   legacy_md_multiply.flag & GP_MIRROR_INVERT_LAYER,
                                   legacy_md_multiply.flag & GP_MIRROR_INVERT_LAYERPASS,
                                   &legacy_md_multiply.material,
                                   legacy_md_multiply.pass_index,
                                   legacy_md_multiply.flag & GP_MIRROR_INVERT_MATERIAL,
                                   legacy_md_multiply.flag & GP_MIRROR_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_noise(ConversionData &conversion_data,
                                         Object &object,
                                         GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilNoise, legacy_md);
  auto &md_noise = reinterpret_cast<GreasePencilNoiseModifierData &>(md);
  auto &legacy_md_noise = reinterpret_cast<NoiseGpencilModifierData &>(legacy_md);

  md_noise.flag = legacy_md_noise.flag;
  md_noise.factor = legacy_md_noise.factor;
  md_noise.factor_strength = legacy_md_noise.factor_strength;
  md_noise.factor_thickness = legacy_md_noise.factor_thickness;
  md_noise.factor_uvs = legacy_md_noise.factor_uvs;
  md_noise.noise_scale = legacy_md_noise.noise_scale;
  md_noise.noise_offset = legacy_md_noise.noise_offset;
  md_noise.noise_mode = legacy_md_noise.noise_mode;
  md_noise.step = legacy_md_noise.step;
  md_noise.seed = legacy_md_noise.seed;

  legacy_object_modifier_influence(md_noise.influence,
                                   legacy_md_noise.layername,
                                   legacy_md_noise.layer_pass,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_LAYER,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_LAYERPASS,
                                   &legacy_md_noise.material,
                                   legacy_md_noise.pass_index,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_MATERIAL,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_PASS,
                                   legacy_md_noise.vgname,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_VGROUP,
                                   &legacy_md_noise.curve_intensity,
                                   legacy_md_noise.flag & GP_NOISE_CUSTOM_CURVE);
}

static void legacy_object_modifier_offset(ConversionData &conversion_data,
                                          Object &object,
                                          GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilOffset, legacy_md);
  auto &md_offset = reinterpret_cast<GreasePencilOffsetModifierData &>(md);
  auto &legacy_md_offset = reinterpret_cast<OffsetGpencilModifierData &>(legacy_md);

  md_offset.flag = 0;
  if (legacy_md_offset.flag & GP_OFFSET_UNIFORM_RANDOM_SCALE) {
    md_offset.flag |= MOD_GREASE_PENCIL_OFFSET_UNIFORM_RANDOM_SCALE;
  }
  switch (eOffsetGpencil_Mode(legacy_md_offset.mode)) {
    case GP_OFFSET_RANDOM:
      md_offset.offset_mode = MOD_GREASE_PENCIL_OFFSET_RANDOM;
      break;
    case GP_OFFSET_LAYER:
      md_offset.offset_mode = MOD_GREASE_PENCIL_OFFSET_LAYER;
      break;
    case GP_OFFSET_MATERIAL:
      md_offset.offset_mode = MOD_GREASE_PENCIL_OFFSET_MATERIAL;
      break;
    case GP_OFFSET_STROKE:
      md_offset.offset_mode = MOD_GREASE_PENCIL_OFFSET_STROKE;
      break;
  }
  copy_v3_v3(md_offset.loc, legacy_md_offset.loc);
  copy_v3_v3(md_offset.rot, legacy_md_offset.rot);
  copy_v3_v3(md_offset.scale, legacy_md_offset.scale);
  copy_v3_v3(md_offset.stroke_loc, legacy_md_offset.rnd_offset);
  copy_v3_v3(md_offset.stroke_rot, legacy_md_offset.rnd_rot);
  copy_v3_v3(md_offset.stroke_scale, legacy_md_offset.rnd_scale);
  md_offset.seed = legacy_md_offset.seed;
  md_offset.stroke_step = legacy_md_offset.stroke_step;
  md_offset.stroke_start_offset = legacy_md_offset.stroke_start_offset;

  legacy_object_modifier_influence(md_offset.influence,
                                   legacy_md_offset.layername,
                                   legacy_md_offset.layer_pass,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_LAYER,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_LAYERPASS,
                                   &legacy_md_offset.material,
                                   legacy_md_offset.pass_index,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_MATERIAL,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_PASS,
                                   legacy_md_offset.vgname,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_opacity(ConversionData &conversion_data,
                                           Object &object,
                                           GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilOpacity, legacy_md);
  auto &md_opacity = reinterpret_cast<GreasePencilOpacityModifierData &>(md);
  auto &legacy_md_opacity = reinterpret_cast<OpacityGpencilModifierData &>(legacy_md);

  md_opacity.flag = 0;
  if (legacy_md_opacity.flag & GP_OPACITY_NORMALIZE) {
    md_opacity.flag |= MOD_GREASE_PENCIL_OPACITY_USE_UNIFORM_OPACITY;
  }
  if (legacy_md_opacity.flag & GP_OPACITY_WEIGHT_FACTOR) {
    md_opacity.flag |= MOD_GREASE_PENCIL_OPACITY_USE_WEIGHT_AS_FACTOR;
  }
  switch (eModifyColorGpencil_Flag(legacy_md_opacity.modify_color)) {
    case GP_MODIFY_COLOR_BOTH:
      md_opacity.color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
      break;
    case GP_MODIFY_COLOR_STROKE:
      md_opacity.color_mode = MOD_GREASE_PENCIL_COLOR_STROKE;
      break;
    case GP_MODIFY_COLOR_FILL:
      md_opacity.color_mode = MOD_GREASE_PENCIL_COLOR_FILL;
      break;
    case GP_MODIFY_COLOR_HARDNESS:
      md_opacity.color_mode = MOD_GREASE_PENCIL_COLOR_HARDNESS;
      break;
  }
  md_opacity.color_factor = legacy_md_opacity.factor;
  md_opacity.hardness_factor = legacy_md_opacity.hardness;

  /* Account for animation on renamed properties. */
  char modifier_name[MAX_NAME * 2];
  BLI_str_escape(modifier_name, md.name, sizeof(modifier_name));
  AnimDataConvertor anim_convertor_factor(
      conversion_data, object.id, object.id, {{".factor", ".color_factor"}});
  anim_convertor_factor.root_path_src = fmt::format("modifiers[\"{}\"]", modifier_name);
  anim_convertor_factor.root_path_dst = fmt::format("modifiers[\"{}\"]", modifier_name);
  anim_convertor_factor.fcurves_convert();
  anim_convertor_factor.fcurves_convert_finalize();
  AnimDataConvertor anim_convertor_hardness(
      conversion_data, object.id, object.id, {{".hardness", ".hardness_factor"}});
  anim_convertor_hardness.root_path_src = fmt::format("modifiers[\"{}\"]", modifier_name);
  anim_convertor_hardness.root_path_dst = fmt::format("modifiers[\"{}\"]", modifier_name);
  anim_convertor_hardness.fcurves_convert();
  anim_convertor_hardness.fcurves_convert_finalize();
  DEG_relations_tag_update(&conversion_data.bmain);

  legacy_object_modifier_influence(md_opacity.influence,
                                   legacy_md_opacity.layername,
                                   legacy_md_opacity.layer_pass,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_LAYER,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_LAYERPASS,
                                   &legacy_md_opacity.material,
                                   legacy_md_opacity.pass_index,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_MATERIAL,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_PASS,
                                   legacy_md_opacity.vgname,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_VGROUP,
                                   &legacy_md_opacity.curve_intensity,
                                   legacy_md_opacity.flag & GP_OPACITY_CUSTOM_CURVE);
}

static void legacy_object_modifier_outline(ConversionData &conversion_data,
                                           Object &object,
                                           GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilOutline, legacy_md);
  auto &md_outline = reinterpret_cast<GreasePencilOutlineModifierData &>(md);
  auto &legacy_md_outline = reinterpret_cast<OutlineGpencilModifierData &>(legacy_md);

  md_outline.flag = 0;
  if (legacy_md_outline.flag & GP_OUTLINE_KEEP_SHAPE) {
    md_outline.flag |= MOD_GREASE_PENCIL_OUTLINE_KEEP_SHAPE;
  }
  md_outline.object = legacy_md_outline.object;
  legacy_md_outline.object = nullptr;
  md_outline.outline_material = legacy_md_outline.outline_material;
  legacy_md_outline.outline_material = nullptr;
  md_outline.sample_length = legacy_md_outline.sample_length;
  md_outline.subdiv = legacy_md_outline.subdiv;
  md_outline.thickness = legacy_md_outline.thickness;

  legacy_object_modifier_influence(md_outline.influence,
                                   legacy_md_outline.layername,
                                   legacy_md_outline.layer_pass,
                                   legacy_md_outline.flag & GP_OUTLINE_INVERT_LAYER,
                                   legacy_md_outline.flag & GP_OUTLINE_INVERT_LAYERPASS,
                                   &legacy_md_outline.material,
                                   legacy_md_outline.pass_index,
                                   legacy_md_outline.flag & GP_OUTLINE_INVERT_MATERIAL,
                                   legacy_md_outline.flag & GP_OUTLINE_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_shrinkwrap(ConversionData &conversion_data,
                                              Object &object,
                                              GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilShrinkwrap, legacy_md);
  auto &md_shrinkwrap = reinterpret_cast<GreasePencilShrinkwrapModifierData &>(md);
  auto &legacy_md_shrinkwrap = reinterpret_cast<ShrinkwrapGpencilModifierData &>(legacy_md);

  /* Shrinkwrap enums and flags do not have named types. */
  /* MOD_SHRINKWRAP_NEAREST_SURFACE etc. */
  md_shrinkwrap.shrink_type = legacy_md_shrinkwrap.shrink_type;
  /* MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR etc. */
  md_shrinkwrap.shrink_opts = legacy_md_shrinkwrap.shrink_opts;
  /* MOD_SHRINKWRAP_ON_SURFACE etc. */
  md_shrinkwrap.shrink_mode = legacy_md_shrinkwrap.shrink_mode;
  /* MOD_SHRINKWRAP_PROJECT_OVER_NORMAL etc. */
  md_shrinkwrap.proj_axis = legacy_md_shrinkwrap.proj_axis;

  md_shrinkwrap.target = legacy_md_shrinkwrap.target;
  legacy_md_shrinkwrap.target = nullptr;
  md_shrinkwrap.aux_target = legacy_md_shrinkwrap.aux_target;
  legacy_md_shrinkwrap.aux_target = nullptr;
  md_shrinkwrap.keep_dist = legacy_md_shrinkwrap.keep_dist;
  md_shrinkwrap.proj_limit = legacy_md_shrinkwrap.proj_limit;
  md_shrinkwrap.subsurf_levels = legacy_md_shrinkwrap.subsurf_levels;
  md_shrinkwrap.smooth_factor = legacy_md_shrinkwrap.smooth_factor;
  md_shrinkwrap.smooth_step = legacy_md_shrinkwrap.smooth_step;

  legacy_object_modifier_influence(md_shrinkwrap.influence,
                                   legacy_md_shrinkwrap.layername,
                                   legacy_md_shrinkwrap.layer_pass,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_LAYER,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_LAYERPASS,
                                   &legacy_md_shrinkwrap.material,
                                   legacy_md_shrinkwrap.pass_index,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_MATERIAL,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_PASS,
                                   legacy_md_shrinkwrap.vgname,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_smooth(ConversionData &conversion_data,
                                          Object &object,
                                          GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilSmooth, legacy_md);
  auto &md_smooth = reinterpret_cast<GreasePencilSmoothModifierData &>(md);
  auto &legacy_md_smooth = reinterpret_cast<SmoothGpencilModifierData &>(legacy_md);

  md_smooth.flag = 0;
  if (legacy_md_smooth.flag & GP_SMOOTH_MOD_LOCATION) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_MOD_LOCATION;
  }
  if (legacy_md_smooth.flag & GP_SMOOTH_MOD_STRENGTH) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_MOD_STRENGTH;
  }
  if (legacy_md_smooth.flag & GP_SMOOTH_MOD_THICKNESS) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_MOD_THICKNESS;
  }
  if (legacy_md_smooth.flag & GP_SMOOTH_MOD_UV) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_MOD_UV;
  }
  if (legacy_md_smooth.flag & GP_SMOOTH_KEEP_SHAPE) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_KEEP_SHAPE;
  }
  md_smooth.factor = legacy_md_smooth.factor;
  md_smooth.step = legacy_md_smooth.step;

  legacy_object_modifier_influence(md_smooth.influence,
                                   legacy_md_smooth.layername,
                                   legacy_md_smooth.layer_pass,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_LAYER,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_LAYERPASS,
                                   &legacy_md_smooth.material,
                                   legacy_md_smooth.pass_index,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_MATERIAL,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_PASS,
                                   legacy_md_smooth.vgname,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_VGROUP,
                                   &legacy_md_smooth.curve_intensity,
                                   legacy_md_smooth.flag & GP_SMOOTH_CUSTOM_CURVE);
}

static void legacy_object_modifier_subdiv(ConversionData &conversion_data,
                                          Object &object,
                                          GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilSubdiv, legacy_md);
  auto &md_subdiv = reinterpret_cast<GreasePencilSubdivModifierData &>(md);
  auto &legacy_md_subdiv = reinterpret_cast<SubdivGpencilModifierData &>(legacy_md);

  switch (eSubdivGpencil_Type(legacy_md_subdiv.type)) {
    case GP_SUBDIV_CATMULL:
      md_subdiv.type = MOD_GREASE_PENCIL_SUBDIV_CATMULL;
      break;
    case GP_SUBDIV_SIMPLE:
      md_subdiv.type = MOD_GREASE_PENCIL_SUBDIV_SIMPLE;
      break;
  }
  md_subdiv.level = legacy_md_subdiv.level;

  legacy_object_modifier_influence(md_subdiv.influence,
                                   legacy_md_subdiv.layername,
                                   legacy_md_subdiv.layer_pass,
                                   legacy_md_subdiv.flag & GP_SUBDIV_INVERT_LAYER,
                                   legacy_md_subdiv.flag & GP_SUBDIV_INVERT_LAYERPASS,
                                   &legacy_md_subdiv.material,
                                   legacy_md_subdiv.pass_index,
                                   legacy_md_subdiv.flag & GP_SUBDIV_INVERT_MATERIAL,
                                   legacy_md_subdiv.flag & GP_SUBDIV_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_texture(ConversionData &conversion_data,
                                           Object &object,
                                           GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilTexture, legacy_md);
  auto &md_texture = reinterpret_cast<GreasePencilTextureModifierData &>(md);
  auto &legacy_md_texture = reinterpret_cast<TextureGpencilModifierData &>(legacy_md);

  switch (eTextureGpencil_Mode(legacy_md_texture.mode)) {
    case STROKE:
      md_texture.mode = MOD_GREASE_PENCIL_TEXTURE_STROKE;
      break;
    case FILL:
      md_texture.mode = MOD_GREASE_PENCIL_TEXTURE_FILL;
      break;
    case STROKE_AND_FILL:
      md_texture.mode = MOD_GREASE_PENCIL_TEXTURE_STROKE_AND_FILL;
      break;
  }
  switch (eTextureGpencil_Fit(legacy_md_texture.fit_method)) {
    case GP_TEX_FIT_STROKE:
      md_texture.fit_method = MOD_GREASE_PENCIL_TEXTURE_FIT_STROKE;
      break;
    case GP_TEX_CONSTANT_LENGTH:
      md_texture.fit_method = MOD_GREASE_PENCIL_TEXTURE_CONSTANT_LENGTH;
      break;
  }
  md_texture.uv_offset = legacy_md_texture.uv_offset;
  md_texture.uv_scale = legacy_md_texture.uv_scale;
  md_texture.fill_rotation = legacy_md_texture.fill_rotation;
  copy_v2_v2(md_texture.fill_offset, legacy_md_texture.fill_offset);
  md_texture.fill_scale = legacy_md_texture.fill_scale;
  md_texture.layer_pass = legacy_md_texture.layer_pass;
  md_texture.alignment_rotation = legacy_md_texture.alignment_rotation;

  legacy_object_modifier_influence(md_texture.influence,
                                   legacy_md_texture.layername,
                                   legacy_md_texture.layer_pass,
                                   legacy_md_texture.flag & GP_TEX_INVERT_LAYER,
                                   legacy_md_texture.flag & GP_TEX_INVERT_LAYERPASS,
                                   &legacy_md_texture.material,
                                   legacy_md_texture.pass_index,
                                   legacy_md_texture.flag & GP_TEX_INVERT_MATERIAL,
                                   legacy_md_texture.flag & GP_TEX_INVERT_PASS,
                                   legacy_md_texture.vgname,
                                   legacy_md_texture.flag & GP_TEX_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_thickness(ConversionData &conversion_data,
                                             Object &object,
                                             GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilThickness, legacy_md);
  auto &md_thickness = reinterpret_cast<GreasePencilThickModifierData &>(md);
  auto &legacy_md_thickness = reinterpret_cast<ThickGpencilModifierData &>(legacy_md);

  md_thickness.flag = 0;
  if (legacy_md_thickness.flag & GP_THICK_NORMALIZE) {
    md_thickness.flag |= MOD_GREASE_PENCIL_THICK_NORMALIZE;
  }
  if (legacy_md_thickness.flag & GP_THICK_WEIGHT_FACTOR) {
    md_thickness.flag |= MOD_GREASE_PENCIL_THICK_WEIGHT_FACTOR;
  }
  md_thickness.thickness_fac = legacy_md_thickness.thickness_fac;
  md_thickness.thickness = legacy_md_thickness.thickness * LEGACY_RADIUS_CONVERSION_FACTOR;

  legacy_object_modifier_influence(md_thickness.influence,
                                   legacy_md_thickness.layername,
                                   legacy_md_thickness.layer_pass,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_LAYER,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_LAYERPASS,
                                   &legacy_md_thickness.material,
                                   legacy_md_thickness.pass_index,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_MATERIAL,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_PASS,
                                   legacy_md_thickness.vgname,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_VGROUP,
                                   &legacy_md_thickness.curve_thickness,
                                   legacy_md_thickness.flag & GP_THICK_CUSTOM_CURVE);
}

static void legacy_object_modifier_time(ConversionData &conversion_data,
                                        Object &object,
                                        GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilTime, legacy_md);
  auto &md_time = reinterpret_cast<GreasePencilTimeModifierData &>(md);
  auto &legacy_md_time = reinterpret_cast<TimeGpencilModifierData &>(legacy_md);

  md_time.flag = 0;
  if (legacy_md_time.flag & GP_TIME_CUSTOM_RANGE) {
    md_time.flag |= MOD_GREASE_PENCIL_TIME_CUSTOM_RANGE;
  }
  if (legacy_md_time.flag & GP_TIME_KEEP_LOOP) {
    md_time.flag |= MOD_GREASE_PENCIL_TIME_KEEP_LOOP;
  }
  switch (eTimeGpencil_Mode(legacy_md_time.mode)) {
    case GP_TIME_MODE_NORMAL:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_NORMAL;
      break;
    case GP_TIME_MODE_REVERSE:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_REVERSE;
      break;
    case GP_TIME_MODE_FIX:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_FIX;
      break;
    case GP_TIME_MODE_PINGPONG:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_PINGPONG;
      break;
    case GP_TIME_MODE_CHAIN:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_CHAIN;
      break;
  }
  md_time.offset = legacy_md_time.offset;
  md_time.frame_scale = legacy_md_time.frame_scale;
  md_time.sfra = legacy_md_time.sfra;
  md_time.efra = legacy_md_time.efra;
  md_time.segment_active_index = legacy_md_time.segment_active_index;
  md_time.segments_num = legacy_md_time.segments_len;
  MEM_SAFE_FREE(md_time.segments_array);
  md_time.segments_array = MEM_calloc_arrayN<GreasePencilTimeModifierSegment>(
      legacy_md_time.segments_len, __func__);
  for (const int i : IndexRange(md_time.segments_num)) {
    GreasePencilTimeModifierSegment &dst_segment = md_time.segments_array[i];
    const TimeGpencilModifierSegment &src_segment = legacy_md_time.segments[i];
    STRNCPY(dst_segment.name, src_segment.name);
    switch (eTimeGpencil_Seg_Mode(src_segment.seg_mode)) {
      case GP_TIME_SEG_MODE_NORMAL:
        dst_segment.segment_mode = MOD_GREASE_PENCIL_TIME_SEG_MODE_NORMAL;
        break;
      case GP_TIME_SEG_MODE_REVERSE:
        dst_segment.segment_mode = MOD_GREASE_PENCIL_TIME_SEG_MODE_REVERSE;
        break;
      case GP_TIME_SEG_MODE_PINGPONG:
        dst_segment.segment_mode = MOD_GREASE_PENCIL_TIME_SEG_MODE_PINGPONG;
        break;
    }
    dst_segment.segment_start = src_segment.seg_start;
    dst_segment.segment_end = src_segment.seg_end;
    dst_segment.segment_repeat = src_segment.seg_repeat;
  }

  /* NOTE: GPv2 time modifier has a material pointer but it is unused. */
  legacy_object_modifier_influence(md_time.influence,
                                   legacy_md_time.layername,
                                   legacy_md_time.layer_pass,
                                   legacy_md_time.flag & GP_TIME_INVERT_LAYER,
                                   legacy_md_time.flag & GP_TIME_INVERT_LAYERPASS,
                                   nullptr,
                                   0,
                                   false,
                                   false,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_tint(ConversionData &conversion_data,
                                        Object &object,
                                        GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilTint, legacy_md);
  auto &md_tint = reinterpret_cast<GreasePencilTintModifierData &>(md);
  auto &legacy_md_tint = reinterpret_cast<TintGpencilModifierData &>(legacy_md);

  md_tint.flag = 0;
  if (legacy_md_tint.flag & GP_TINT_WEIGHT_FACTOR) {
    md_tint.flag |= MOD_GREASE_PENCIL_TINT_USE_WEIGHT_AS_FACTOR;
  }
  switch (eGp_Vertex_Mode(legacy_md_tint.mode)) {
    case GPPAINT_MODE_BOTH:
      md_tint.color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
      break;
    case GPPAINT_MODE_STROKE:
      md_tint.color_mode = MOD_GREASE_PENCIL_COLOR_STROKE;
      break;
    case GPPAINT_MODE_FILL:
      md_tint.color_mode = MOD_GREASE_PENCIL_COLOR_FILL;
      break;
  }
  switch (eTintGpencil_Type(legacy_md_tint.type)) {
    case GP_TINT_UNIFORM:
      md_tint.tint_mode = MOD_GREASE_PENCIL_TINT_UNIFORM;
      break;
    case GP_TINT_GRADIENT:
      md_tint.tint_mode = MOD_GREASE_PENCIL_TINT_GRADIENT;
      break;
  }
  md_tint.factor = legacy_md_tint.factor;
  md_tint.radius = legacy_md_tint.radius;
  copy_v3_v3(md_tint.color, legacy_md_tint.rgb);
  md_tint.object = legacy_md_tint.object;
  legacy_md_tint.object = nullptr;
  MEM_SAFE_FREE(md_tint.color_ramp);
  md_tint.color_ramp = legacy_md_tint.colorband;
  legacy_md_tint.colorband = nullptr;

  legacy_object_modifier_influence(md_tint.influence,
                                   legacy_md_tint.layername,
                                   legacy_md_tint.layer_pass,
                                   legacy_md_tint.flag & GP_TINT_INVERT_LAYER,
                                   legacy_md_tint.flag & GP_TINT_INVERT_LAYERPASS,
                                   &legacy_md_tint.material,
                                   legacy_md_tint.pass_index,
                                   legacy_md_tint.flag & GP_TINT_INVERT_MATERIAL,
                                   legacy_md_tint.flag & GP_TINT_INVERT_PASS,
                                   legacy_md_tint.vgname,
                                   legacy_md_tint.flag & GP_TINT_INVERT_VGROUP,
                                   &legacy_md_tint.curve_intensity,
                                   legacy_md_tint.flag & GP_TINT_CUSTOM_CURVE);
}

static void legacy_object_modifier_weight_angle(ConversionData &conversion_data,
                                                Object &object,
                                                GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilWeightAngle, legacy_md);
  auto &md_weight_angle = reinterpret_cast<GreasePencilWeightAngleModifierData &>(md);
  auto &legacy_md_weight_angle = reinterpret_cast<WeightAngleGpencilModifierData &>(legacy_md);

  md_weight_angle.flag = 0;
  if (legacy_md_weight_angle.flag & GP_WEIGHT_MULTIPLY_DATA) {
    md_weight_angle.flag |= MOD_GREASE_PENCIL_WEIGHT_ANGLE_MULTIPLY_DATA;
  }
  if (legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_OUTPUT) {
    md_weight_angle.flag |= MOD_GREASE_PENCIL_WEIGHT_ANGLE_INVERT_OUTPUT;
  }
  switch (eGpencilModifierSpace(legacy_md_weight_angle.space)) {
    case GP_SPACE_LOCAL:
      md_weight_angle.space = MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_LOCAL;
      break;
    case GP_SPACE_WORLD:
      md_weight_angle.space = MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_WORLD;
      break;
  }
  md_weight_angle.axis = legacy_md_weight_angle.axis;
  STRNCPY(md_weight_angle.target_vgname, legacy_md_weight_angle.target_vgname);
  md_weight_angle.min_weight = legacy_md_weight_angle.min_weight;
  md_weight_angle.angle = legacy_md_weight_angle.angle;

  legacy_object_modifier_influence(md_weight_angle.influence,
                                   legacy_md_weight_angle.layername,
                                   legacy_md_weight_angle.layer_pass,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_LAYER,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_LAYERPASS,
                                   &legacy_md_weight_angle.material,
                                   legacy_md_weight_angle.pass_index,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_MATERIAL,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_PASS,
                                   legacy_md_weight_angle.vgname,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_weight_proximity(ConversionData &conversion_data,
                                                    Object &object,
                                                    GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilWeightProximity, legacy_md);
  auto &md_weight_prox = reinterpret_cast<GreasePencilWeightProximityModifierData &>(md);
  auto &legacy_md_weight_prox = reinterpret_cast<WeightProxGpencilModifierData &>(legacy_md);

  md_weight_prox.flag = 0;
  if (legacy_md_weight_prox.flag & GP_WEIGHT_MULTIPLY_DATA) {
    md_weight_prox.flag |= MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_MULTIPLY_DATA;
  }
  if (legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_OUTPUT) {
    md_weight_prox.flag |= MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_INVERT_OUTPUT;
  }
  STRNCPY(md_weight_prox.target_vgname, legacy_md_weight_prox.target_vgname);
  md_weight_prox.min_weight = legacy_md_weight_prox.min_weight;
  md_weight_prox.dist_start = legacy_md_weight_prox.dist_start;
  md_weight_prox.dist_end = legacy_md_weight_prox.dist_end;
  md_weight_prox.object = legacy_md_weight_prox.object;
  legacy_md_weight_prox.object = nullptr;

  legacy_object_modifier_influence(md_weight_prox.influence,
                                   legacy_md_weight_prox.layername,
                                   legacy_md_weight_prox.layer_pass,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_LAYER,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_LAYERPASS,
                                   &legacy_md_weight_prox.material,
                                   legacy_md_weight_prox.pass_index,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_MATERIAL,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_PASS,
                                   legacy_md_weight_prox.vgname,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_lineart(ConversionData &conversion_data,
                                           Object &object,
                                           GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilLineart, legacy_md);
  auto &md_lineart = reinterpret_cast<GreasePencilLineartModifierData &>(md);
  auto &legacy_md_lineart = reinterpret_cast<LineartGpencilModifierData &>(legacy_md);

  greasepencil::convert::lineart_wrap_v3(&legacy_md_lineart, &md_lineart);
}

static void legacy_object_modifier_build(ConversionData &conversion_data,
                                         Object &object,
                                         GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilBuild, legacy_md);
  auto &md_build = reinterpret_cast<GreasePencilBuildModifierData &>(md);
  auto &legacy_md_build = reinterpret_cast<BuildGpencilModifierData &>(legacy_md);

  md_build.flag = 0;
  if (legacy_md_build.flag & GP_BUILD_RESTRICT_TIME) {
    md_build.flag |= MOD_GREASE_PENCIL_BUILD_RESTRICT_TIME;
  }
  if (legacy_md_build.flag & GP_BUILD_USE_FADING) {
    md_build.flag |= MOD_GREASE_PENCIL_BUILD_USE_FADING;
  }

  switch (legacy_md_build.mode) {
    case GP_BUILD_MODE_ADDITIVE:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_MODE_ADDITIVE;
      break;
    case GP_BUILD_MODE_CONCURRENT:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_MODE_CONCURRENT;
      break;
    case GP_BUILD_MODE_SEQUENTIAL:
    default:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_MODE_SEQUENTIAL;
      break;
  }

  switch (legacy_md_build.time_alignment) {
    default:
    case GP_BUILD_TIMEALIGN_START:
      md_build.time_alignment = MOD_GREASE_PENCIL_BUILD_TIMEALIGN_START;
      break;
    case GP_BUILD_TIMEALIGN_END:
      md_build.time_alignment = MOD_GREASE_PENCIL_BUILD_TIMEALIGN_END;
      break;
  }

  switch (legacy_md_build.time_mode) {
    default:
    case GP_BUILD_TIMEMODE_FRAMES:
      md_build.time_mode = MOD_GREASE_PENCIL_BUILD_TIMEMODE_FRAMES;
      break;
    case GP_BUILD_TIMEMODE_PERCENTAGE:
      md_build.time_mode = MOD_GREASE_PENCIL_BUILD_TIMEMODE_PERCENTAGE;
      break;
    case GP_BUILD_TIMEMODE_DRAWSPEED:
      md_build.time_mode = MOD_GREASE_PENCIL_BUILD_TIMEMODE_DRAWSPEED;
      break;
  }

  switch (legacy_md_build.transition) {
    default:
    case GP_BUILD_TRANSITION_GROW:
      md_build.transition = MOD_GREASE_PENCIL_BUILD_TRANSITION_GROW;
      break;
    case GP_BUILD_TRANSITION_SHRINK:
      md_build.transition = MOD_GREASE_PENCIL_BUILD_TRANSITION_SHRINK;
      break;
    case GP_BUILD_TRANSITION_VANISH:
      md_build.transition = MOD_GREASE_PENCIL_BUILD_TRANSITION_VANISH;
      break;
  }

  md_build.start_frame = legacy_md_build.start_frame;
  md_build.end_frame = legacy_md_build.end_frame;
  md_build.start_delay = legacy_md_build.start_delay;
  md_build.length = legacy_md_build.length;
  md_build.fade_fac = legacy_md_build.fade_fac;
  md_build.fade_opacity_strength = legacy_md_build.fade_opacity_strength;
  md_build.fade_thickness_strength = legacy_md_build.fade_thickness_strength;
  md_build.percentage_fac = legacy_md_build.percentage_fac;
  md_build.speed_fac = legacy_md_build.speed_fac;
  md_build.speed_maxgap = legacy_md_build.speed_maxgap;
  md_build.object = legacy_md_build.object;
  STRNCPY(md_build.target_vgname, legacy_md_build.target_vgname);

  legacy_object_modifier_influence(md_build.influence,
                                   legacy_md_build.layername,
                                   legacy_md_build.layer_pass,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_LAYER,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_LAYERPASS,
                                   &legacy_md_build.material,
                                   legacy_md_build.pass_index,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_MATERIAL,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_PASS,
                                   legacy_md_build.target_vgname,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_simplify(ConversionData &conversion_data,
                                            Object &object,
                                            GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      conversion_data, object, eModifierType_GreasePencilSimplify, legacy_md);
  auto &md_simplify = reinterpret_cast<GreasePencilSimplifyModifierData &>(md);
  auto &legacy_md_simplify = reinterpret_cast<SimplifyGpencilModifierData &>(legacy_md);

  switch (legacy_md_simplify.mode) {
    case GP_SIMPLIFY_FIXED:
      md_simplify.mode = MOD_GREASE_PENCIL_SIMPLIFY_FIXED;
      break;
    case GP_SIMPLIFY_ADAPTIVE:
      md_simplify.mode = MOD_GREASE_PENCIL_SIMPLIFY_ADAPTIVE;
      break;
    case GP_SIMPLIFY_SAMPLE:
      md_simplify.mode = MOD_GREASE_PENCIL_SIMPLIFY_SAMPLE;
      break;
    case GP_SIMPLIFY_MERGE:
      md_simplify.mode = MOD_GREASE_PENCIL_SIMPLIFY_MERGE;
      break;
  }

  md_simplify.step = legacy_md_simplify.step;
  md_simplify.factor = legacy_md_simplify.factor;
  md_simplify.length = legacy_md_simplify.length;
  md_simplify.sharp_threshold = legacy_md_simplify.sharp_threshold;
  md_simplify.distance = legacy_md_simplify.distance;

  legacy_object_modifier_influence(md_simplify.influence,
                                   legacy_md_simplify.layername,
                                   legacy_md_simplify.layer_pass,
                                   legacy_md_simplify.flag & GP_SIMPLIFY_INVERT_LAYER,
                                   legacy_md_simplify.flag & GP_SIMPLIFY_INVERT_LAYERPASS,
                                   &legacy_md_simplify.material,
                                   legacy_md_simplify.pass_index,
                                   legacy_md_simplify.flag & GP_SIMPLIFY_INVERT_MATERIAL,
                                   legacy_md_simplify.flag & GP_SIMPLIFY_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifiers(ConversionData &conversion_data, Object &object)
{
  BLI_assert(BLI_listbase_is_empty(&object.modifiers));

  while (GpencilModifierData *gpd_md = static_cast<GpencilModifierData *>(
             BLI_pophead(&object.greasepencil_modifiers)))
  {
    switch (gpd_md->type) {
      case eGpencilModifierType_None:
        /* Unknown type, just ignore. */
        break;
      case eGpencilModifierType_Armature:
        legacy_object_modifier_armature(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Array:
        legacy_object_modifier_array(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Color:
        legacy_object_modifier_color(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Dash:
        legacy_object_modifier_dash(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Envelope:
        legacy_object_modifier_envelope(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Hook:
        legacy_object_modifier_hook(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Lattice:
        legacy_object_modifier_lattice(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Length:
        legacy_object_modifier_length(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Mirror:
        legacy_object_modifier_mirror(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Multiply:
        legacy_object_modifier_multiply(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Noise:
        legacy_object_modifier_noise(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Offset:
        legacy_object_modifier_offset(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Opacity:
        legacy_object_modifier_opacity(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Outline:
        legacy_object_modifier_outline(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Shrinkwrap:
        legacy_object_modifier_shrinkwrap(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Smooth:
        legacy_object_modifier_smooth(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Subdiv:
        legacy_object_modifier_subdiv(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Texture:
        legacy_object_modifier_texture(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Thick:
        legacy_object_modifier_thickness(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Time:
        legacy_object_modifier_time(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Tint:
        legacy_object_modifier_tint(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_WeightAngle:
        legacy_object_modifier_weight_angle(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_WeightProximity:
        legacy_object_modifier_weight_proximity(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Lineart:
        legacy_object_modifier_lineart(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Build:
        legacy_object_modifier_build(conversion_data, object, *gpd_md);
        break;
      case eGpencilModifierType_Simplify:
        legacy_object_modifier_simplify(conversion_data, object, *gpd_md);
        break;
    }

    BKE_gpencil_modifier_free_ex(gpd_md, 0);
  }
}

/**
 * Ensure that both use-cases of legacy grease pencil data (as object, and as annotations) are
 * fully separated and valid (have proper tag). Annotation IDs are left as-is currently, while GPv2
 * data used by objects need to be converted to GPv3 data.
 *
 * \note GPv2 IDs not used by object nor annotations are just left in their current state - the
 * ones tagged as annotations will be left untouched, the others will be converted to GPv3 data.
 *
 * \note De-duplication needs to assign the new, duplicated GPv2 ID to annotations, and keep the
 * original one assigned to objects. That way, when the object data are converted, other potential
 * references to the old GPv2 ID (drivers, custom properties, etc.) can be safely remapped to the
 * new GPv3 ID.
 *
 * \warning: This makes the assumption that annotation data is not typically referenced by anything
 * else than annotation pointers. If this is not true, then some data might be lost during
 * conversion process.
 */
static void legacy_gpencil_sanitize_annotations(Main &bmain)
{
  /* Annotations mapping, keys are existing GPv2 IDs actually used as annotations, values are
   * either:
   *  - nullptr: this annotation is never used by any object, there was no need to duplicate it.
   *  - new GPv2 ID: this annotation was also used by objects, this is its new duplicated ID. */
  Map<bGPdata *, bGPdata *> annotations_gpv2;
  /* All legacy GPv2 data used by objects. */
  Set<bGPdata *> object_gpv2;

  /* Check all GP objects. */
  LISTBASE_FOREACH (Object *, object, &bmain.objects) {
    if (object->type != OB_GPENCIL_LEGACY) {
      continue;
    }
    bGPdata *legacy_gpd = static_cast<bGPdata *>(object->data);
    if (!legacy_gpd) {
      continue;
    }
    if ((legacy_gpd->flag & GP_DATA_ANNOTATIONS) != 0) {
      legacy_gpd->flag &= ~GP_DATA_ANNOTATIONS;
    }
    object_gpv2.add(legacy_gpd);
  }

  /* Detect all annotations currently used as such, and de-duplicate them if they are also used by
   * objects. */

  auto sanitize_gpv2_annotation = [&](bGPdata **legacy_gpd_p) {
    bGPdata *legacy_gpd = *legacy_gpd_p;
    if (!legacy_gpd) {
      return;
    }

    bGPdata *new_annotation_gpd = annotations_gpv2.lookup_or_add(legacy_gpd, nullptr);
    if (!object_gpv2.contains(legacy_gpd)) {
      /* Legacy annotation GPv2 data not used by any object, just ensure that it is properly
       * tagged. */
      BLI_assert(!new_annotation_gpd);
      if ((legacy_gpd->flag & GP_DATA_ANNOTATIONS) == 0) {
        legacy_gpd->flag |= GP_DATA_ANNOTATIONS;
      }
      return;
    }

    /* Legacy GP data also used by objects. Create the duplicate of legacy GPv2 data for
     * annotations, if not yet done. */
    if (!new_annotation_gpd) {
      new_annotation_gpd = reinterpret_cast<bGPdata *>(BKE_id_copy_in_lib(&bmain,
                                                                          legacy_gpd->id.lib,
                                                                          &legacy_gpd->id,
                                                                          std::nullopt,
                                                                          nullptr,
                                                                          LIB_ID_COPY_DEFAULT));
      new_annotation_gpd->flag |= GP_DATA_ANNOTATIONS;
      id_us_min(&new_annotation_gpd->id);
      annotations_gpv2.add_overwrite(legacy_gpd, new_annotation_gpd);
    }

    /* Assign the annotation duplicate ID to the annotation pointer. */
    BLI_assert(new_annotation_gpd->flag & GP_DATA_ANNOTATIONS);
    id_us_min(&legacy_gpd->id);
    *legacy_gpd_p = new_annotation_gpd;
    id_us_plus_no_lib(&new_annotation_gpd->id);
  };

  LISTBASE_FOREACH (Scene *, scene, &bmain.scenes) {
    sanitize_gpv2_annotation(&scene->gpd);
  }

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (&bmain, id_iter) {
    if (bNodeTree *node_tree = bke::node_tree_from_id(id_iter)) {
      sanitize_gpv2_annotation(&node_tree->gpd);
    }
  }
  FOREACH_MAIN_ID_END;
  LISTBASE_FOREACH (bNodeTree *, node_tree, &bmain.nodetrees) {
    sanitize_gpv2_annotation(&node_tree->gpd);
  }

  LISTBASE_FOREACH (MovieClip *, movie_clip, &bmain.movieclips) {
    sanitize_gpv2_annotation(&movie_clip->gpd);

    LISTBASE_FOREACH (MovieTrackingObject *, mvc_tracking_object, &movie_clip->tracking.objects) {
      LISTBASE_FOREACH (MovieTrackingTrack *, mvc_track, &mvc_tracking_object->tracks) {
        sanitize_gpv2_annotation(&mvc_track->gpd);
      }
      LISTBASE_FOREACH (
          MovieTrackingPlaneTrack *, mvc_plane_track, &mvc_tracking_object->plane_tracks)
      {
        for (int i = 0; i < mvc_plane_track->point_tracksnr; i++) {
          sanitize_gpv2_annotation(&mvc_plane_track->point_tracks[i]->gpd);
        }
      }
    }
  }

  LISTBASE_FOREACH (bScreen *, screen, &bmain.screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, space_link, &area->spacedata) {
        switch (eSpace_Type(space_link->spacetype)) {
          case SPACE_SEQ: {
            SpaceSeq *space_sequencer = reinterpret_cast<SpaceSeq *>(space_link);
            sanitize_gpv2_annotation(&space_sequencer->gpd);
            break;
          }
          case SPACE_IMAGE: {
            SpaceImage *space_image = reinterpret_cast<SpaceImage *>(space_link);
            sanitize_gpv2_annotation(&space_image->gpd);
            break;
          }
          case SPACE_NODE: {
            SpaceNode *space_node = reinterpret_cast<SpaceNode *>(space_link);
            sanitize_gpv2_annotation(&space_node->gpd);
            break;
          }
          case SPACE_EMPTY:
          case SPACE_VIEW3D:
            /* #View3D.gpd is deprecated and can be ignored here. */
          case SPACE_GRAPH:
          case SPACE_OUTLINER:
          case SPACE_PROPERTIES:
          case SPACE_FILE:
          case SPACE_INFO:
          case SPACE_TEXT:
          case SPACE_ACTION:
          case SPACE_NLA:
          case SPACE_SCRIPT:
          case SPACE_CONSOLE:
          case SPACE_USERPREF:
          case SPACE_CLIP:
          case SPACE_TOPBAR:
          case SPACE_STATUSBAR:
          case SPACE_SPREADSHEET:
            break;
        }
      }
    }
  }
}

static void legacy_gpencil_object(ConversionData &conversion_data, Object &object)
{
  BLI_assert((GS(static_cast<ID *>(object.data)->name) == ID_GD_LEGACY));

  bGPdata *gpd = static_cast<bGPdata *>(object.data);

  GreasePencil *new_grease_pencil = conversion_data.legacy_to_greasepencil_data.lookup_default(
      gpd, nullptr);
  const bool do_gpencil_data_conversion = (new_grease_pencil == nullptr);

  if (!new_grease_pencil) {
    new_grease_pencil = static_cast<GreasePencil *>(
        BKE_id_new_in_lib(&conversion_data.bmain, gpd->id.lib, ID_GP, gpd->id.name + 2));
    id_us_min(&new_grease_pencil->id);
  }

  object.data = new_grease_pencil;
  object.type = OB_GREASE_PENCIL;

  /* NOTE: Could also use #BKE_id_free_us, to also free the legacy GP if not used anymore? */
  id_us_min(&gpd->id);
  id_us_plus(&new_grease_pencil->id);

  if (do_gpencil_data_conversion) {
    legacy_gpencil_to_grease_pencil(conversion_data, *new_grease_pencil, *gpd);
    conversion_data.legacy_to_greasepencil_data.add(gpd, new_grease_pencil);
  }

  legacy_object_modifiers(conversion_data, object);
  /* Convert the animation of the "uniform thickness" setting of the thickness modifier. */
  legacy_object_thickness_modifier_thickness_anim(conversion_data, object);

  /* Layer adjustments should be added after all other modifiers. */
  layer_adjustments_to_modifiers(conversion_data, *gpd, object);
  /* Thickness factor is applied after all other changes to the radii. */
  thickness_factor_to_modifier(conversion_data, *gpd, object);

  BKE_object_free_derived_caches(&object);
}

void legacy_main(Main &bmain,
                 BlendfileLinkAppendContext *lapp_context,
                 BlendFileReadReport & /*reports*/)
{
  ConversionData conversion_data(bmain, lapp_context);

  /* Ensure that annotations are fully separated from object usages of legacy GPv2 data. */
  legacy_gpencil_sanitize_annotations(bmain);

  LISTBASE_FOREACH (Object *, object, &bmain.objects) {
    if (object->type != OB_GPENCIL_LEGACY) {
      continue;
    }
    legacy_gpencil_object(conversion_data, *object);
  }

  /* Potential other usages of legacy bGPdata IDs also need to be remapped to their matching new
   * GreasePencil counterparts. */
  bke::id::IDRemapper gpd_remapper;
  /* Allow remapping from legacy bGPdata IDs to new GreasePencil ones. */
  gpd_remapper.allow_idtype_mismatch = true;

  LISTBASE_FOREACH (bGPdata *, legacy_gpd, &bmain.gpencils) {
    /* Annotations still use legacy `bGPdata`, these should not be converted. Call to
     * #legacy_gpencil_sanitize_annotations above ensured to fully separate annotations from object
     * legacy grease pencil. */
    if ((legacy_gpd->flag & GP_DATA_ANNOTATIONS) != 0) {
      continue;
    }
    GreasePencil *new_grease_pencil = conversion_data.legacy_to_greasepencil_data.lookup_default(
        legacy_gpd, nullptr);
    if (!new_grease_pencil) {
      new_grease_pencil = static_cast<GreasePencil *>(
          BKE_id_new_in_lib(&bmain, legacy_gpd->id.lib, ID_GP, legacy_gpd->id.name + 2));
      id_us_min(&new_grease_pencil->id);
      legacy_gpencil_to_grease_pencil(conversion_data, *new_grease_pencil, *legacy_gpd);
      conversion_data.legacy_to_greasepencil_data.add(legacy_gpd, new_grease_pencil);
    }
    gpd_remapper.add(&legacy_gpd->id, &new_grease_pencil->id);
  }

  BKE_libblock_remap_multiple(&bmain, gpd_remapper, ID_REMAP_ALLOW_IDTYPE_MISMATCH);

  if (conversion_data.lapp_context) {
    BKE_blendfile_link_append_context_item_foreach(
        conversion_data.lapp_context,
        [&conversion_data](BlendfileLinkAppendContext *lapp_context,
                           BlendfileLinkAppendContextItem *item) -> bool {
          ID *item_new_id = BKE_blendfile_link_append_context_item_newid_get(lapp_context, item);
          if (!item_new_id || GS(item_new_id->name) != ID_GD_LEGACY) {
            return true;
          }
          GreasePencil **item_grease_pencil =
              conversion_data.legacy_to_greasepencil_data.lookup_ptr(
                  reinterpret_cast<bGPdata *>(item_new_id));
          if (item_grease_pencil && *item_grease_pencil) {
            BKE_blendfile_link_append_context_item_newid_set(
                lapp_context, item, &(*item_grease_pencil)->id);
          }
          return true;
        },
        eBlendfileLinkAppendForeachItemFlag(
            BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_DIRECT |
            BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_INDIRECT));
  }
}

void lineart_wrap_v3(const LineartGpencilModifierData *lmd_legacy,
                     GreasePencilLineartModifierData *lmd)
{
  lmd->edge_types = lmd_legacy->edge_types;
  lmd->source_type = lmd_legacy->source_type;
  lmd->use_multiple_levels = lmd_legacy->use_multiple_levels;
  lmd->level_start = lmd_legacy->level_start;
  lmd->level_end = lmd_legacy->level_end;
  lmd->source_camera = lmd_legacy->source_camera;
  lmd->light_contour_object = lmd_legacy->light_contour_object;
  lmd->source_object = lmd_legacy->source_object;
  lmd->source_collection = lmd_legacy->source_collection;
  lmd->target_material = lmd_legacy->target_material;
  STRNCPY(lmd->target_layer, lmd_legacy->target_layer);
  STRNCPY(lmd->source_vertex_group, lmd_legacy->source_vertex_group);
  STRNCPY(lmd->vgname, lmd_legacy->vgname);
  lmd->overscan = lmd_legacy->overscan;
  lmd->shadow_camera_fov = lmd_legacy->shadow_camera_fov;
  lmd->shadow_camera_size = lmd_legacy->shadow_camera_size;
  lmd->shadow_camera_near = lmd_legacy->shadow_camera_near;
  lmd->shadow_camera_far = lmd_legacy->shadow_camera_far;
  lmd->opacity = lmd_legacy->opacity;
  lmd->radius = float(lmd_legacy->thickness) * LEGACY_RADIUS_CONVERSION_FACTOR;
  lmd->mask_switches = lmd_legacy->mask_switches;
  lmd->material_mask_bits = lmd_legacy->material_mask_bits;
  lmd->intersection_mask = lmd_legacy->intersection_mask;
  lmd->shadow_selection = lmd_legacy->shadow_selection;
  lmd->silhouette_selection = lmd_legacy->silhouette_selection;
  lmd->crease_threshold = lmd_legacy->crease_threshold;
  lmd->angle_splitting_threshold = lmd_legacy->angle_splitting_threshold;
  lmd->chain_smooth_tolerance = lmd_legacy->chain_smooth_tolerance;
  lmd->chaining_image_threshold = lmd_legacy->chaining_image_threshold;
  lmd->calculation_flags = lmd_legacy->calculation_flags;
  lmd->flags = lmd_legacy->flags;
  lmd->stroke_depth_offset = lmd_legacy->stroke_depth_offset;
  lmd->level_start_override = lmd_legacy->level_start_override;
  lmd->level_end_override = lmd_legacy->level_end_override;
  lmd->edge_types_override = lmd_legacy->edge_types_override;
  lmd->shadow_selection_override = lmd_legacy->shadow_selection_override;
  lmd->shadow_use_silhouette_override = lmd_legacy->shadow_use_silhouette_override;
  lmd->cache = lmd_legacy->cache;
  lmd->la_data_ptr = lmd_legacy->la_data_ptr;
}

void lineart_unwrap_v3(LineartGpencilModifierData *lmd_legacy,
                       const GreasePencilLineartModifierData *lmd)
{
  lmd_legacy->edge_types = lmd->edge_types;
  lmd_legacy->source_type = lmd->source_type;
  lmd_legacy->use_multiple_levels = lmd->use_multiple_levels;
  lmd_legacy->level_start = lmd->level_start;
  lmd_legacy->level_end = lmd->level_end;
  lmd_legacy->source_camera = lmd->source_camera;
  lmd_legacy->light_contour_object = lmd->light_contour_object;
  lmd_legacy->source_object = lmd->source_object;
  lmd_legacy->source_collection = lmd->source_collection;
  lmd_legacy->target_material = lmd->target_material;
  STRNCPY(lmd_legacy->source_vertex_group, lmd->source_vertex_group);
  STRNCPY(lmd_legacy->vgname, lmd->vgname);
  lmd_legacy->overscan = lmd->overscan;
  lmd_legacy->shadow_camera_fov = lmd->shadow_camera_fov;
  lmd_legacy->shadow_camera_size = lmd->shadow_camera_size;
  lmd_legacy->shadow_camera_near = lmd->shadow_camera_near;
  lmd_legacy->shadow_camera_far = lmd->shadow_camera_far;
  lmd_legacy->opacity = lmd->opacity;
  lmd_legacy->thickness = lmd->radius / LEGACY_RADIUS_CONVERSION_FACTOR;
  lmd_legacy->mask_switches = lmd->mask_switches;
  lmd_legacy->material_mask_bits = lmd->material_mask_bits;
  lmd_legacy->intersection_mask = lmd->intersection_mask;
  lmd_legacy->shadow_selection = lmd->shadow_selection;
  lmd_legacy->silhouette_selection = lmd->silhouette_selection;
  lmd_legacy->crease_threshold = lmd->crease_threshold;
  lmd_legacy->angle_splitting_threshold = lmd->angle_splitting_threshold;
  lmd_legacy->chain_smooth_tolerance = lmd->chain_smooth_tolerance;
  lmd_legacy->chaining_image_threshold = lmd->chaining_image_threshold;
  lmd_legacy->calculation_flags = lmd->calculation_flags;
  lmd_legacy->flags = lmd->flags;
  lmd_legacy->stroke_depth_offset = lmd->stroke_depth_offset;
  lmd_legacy->level_start_override = lmd->level_start_override;
  lmd_legacy->level_end_override = lmd->level_end_override;
  lmd_legacy->edge_types_override = lmd->edge_types_override;
  lmd_legacy->shadow_selection_override = lmd->shadow_selection_override;
  lmd_legacy->shadow_use_silhouette_override = lmd->shadow_use_silhouette_override;
  lmd_legacy->cache = lmd->cache;
  lmd_legacy->la_data_ptr = lmd->la_data_ptr;
}

}  // namespace blender::bke::greasepencil::convert
