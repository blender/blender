/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to insert, delete or modify keyframes.
 */

#pragma once

#include <array>
#include <string>

#include "BLI_array.hh"
#include "BLI_bit_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_anim_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_path.hh"
#include "RNA_types.hh"

struct ID;
struct Main;
struct Scene;

struct AnimationEvalContext;
struct NlaKeyframingContext;

namespace blender::animrig {

/**
 * Represents a single success/failure in the keyframing process.
 *
 * What is considered "single" depends on the level at which the failure
 * happens. For example, it can be at the level of a single key on a single
 * fcurve, all the way up to the level of an entire ID not being animatable.
 * Both are considered "single" events.
 */
enum class SingleKeyingResult {
  SUCCESS = 0,
  /* TODO: remove `UNKNOWN_FAILURE` and replace all usages with proper, specific
   * cases. This is needed right now as a stop-gap while progressively moving
   * the keyframing code over to propagate errors properly. */
  UNKNOWN_FAILURE,
  CANNOT_CREATE_FCURVE,
  FCURVE_NOT_KEYFRAMEABLE,
  NO_KEY_NEEDED,
  UNABLE_TO_INSERT_TO_NLA_STACK,
  ID_NOT_EDITABLE,
  ID_NOT_ANIMATABLE,
  NO_VALID_LAYER,
  NO_VALID_STRIP,
  NO_VALID_SLOT,
  CANNOT_RESOLVE_PATH,
  /* Make sure to always keep this at the end of the enum. */
  _KEYING_RESULT_MAX,
};

/**
 * Class for tracking the result of inserting keyframes. Tracks how often each of
 * `SingleKeyingResult` has happened.
 * */
class CombinedKeyingResult {
 private:
  /* The index to the array maps a `SingleKeyingResult` to the number of times this result has
   * occurred. */
  std::array<int, size_t(SingleKeyingResult::_KEYING_RESULT_MAX)> result_counter;

 public:
  CombinedKeyingResult();

  /**
   * Increase the count of the given `SingleKeyingResult` by `count`.
   */
  void add(SingleKeyingResult result, int count = 1);

  /* Add values of the given result to this result. */
  void merge(const CombinedKeyingResult &other);

  int get_count(const SingleKeyingResult result) const;

  bool has_errors() const;

  void generate_reports(ReportList *reports, eReportType report_level = RPT_ERROR);
};

/**
 * Return the default channel group name for the given RNA pointer and property
 * path, or none if it has no default.
 *
 * For example, for object location/rotation/scale this returns the standard
 * "Object Transforms" channel group name.
 */
const std::optional<StringRefNull> default_channel_group_for_path(
    const PointerRNA *animated_struct, const StringRef prop_rna_path);

/* -------------------------------------------------------------------- */

/**
 * Return whether key insertion functions are allowed to create new fcurves,
 * according to the given flags.
 *
 * Specifically, both `INSERTKEY_REPLACE` and `INSERTKEY_AVAILABLE` prohibit the
 * creation of new F-Curves.
 */
bool key_insertion_may_create_fcurve(eInsertKeyFlags insert_key_flags);

/* -------------------------------------------------------------------- */
/** \name Key-Framing Management
 * \{ */

/* Set the FCurve flag based on the property type of `prop`. */
void update_autoflags_fcurve_direct(FCurve *fcu, PropertyRNA *prop);

/**
 * \brief Main key-frame insertion API.
 *
 * Insert keys for `struct_pointer`, for all paths in `rna_paths`. Any necessary
 * animation data (AnimData, Action, ...) is created if it doesn't already
 * exist.
 *
 * Note that this function was created as part of an ongoing refactor by merging
 * two other functions that were *almost* identical to each other. There are
 * still things left over from that which can and should be improved (such as
 * the partially redundant `scene_frame` and `anim_eval_context`parameters).
 * Additionally, it's a bit of a mega-function now, and can probably be stripped
 * down to a clearer core functionality.
 *
 * \param struct_pointer: RNA pointer to the struct to be keyed. This is often
 * an ID, but not necessarily. For example, pose bones are also common. Note
 * that if you have an `ID` and want to pass it here for keying, you can create
 * the `PointerRNA` for it with `RNA_id_pointer_create()`.
 *
 * \param channel_group: the channel group to put any newly created fcurves
 * under. If not given, the standard groups are used.
 *
 * \param rna_paths: the RNA paths to key. These paths are relative to
 * `struct_pointer`. Note that for paths to array properties, if the array index
 * is specified then only that element is keyed, but if the index is not
 * specified then *all* array elements are keyed.
 *
 * \param scene_frame: the frame to insert the keys at. This is in scene time,
 * not NLA mapped (NLA mapping is already handled internally by this function).
 * If not given, the evaluation time from `anim_eval_context` is used instead.
 *
 * \returns A summary of the successful and failed keyframe insertions, with
 * reasons for the failures.
 */
CombinedKeyingResult insert_keyframes(Main *bmain,
                                      PointerRNA *struct_pointer,
                                      std::optional<StringRefNull> channel_group,
                                      const blender::Span<RNAPath> rna_paths,
                                      std::optional<float> scene_frame,
                                      const AnimationEvalContext &anim_eval_context,
                                      eBezTriple_KeyframeType key_type,
                                      eInsertKeyFlags insert_key_flags);

/**
 * \brief Secondary Insert Key-framing API call.
 *
 * Use this when validation of necessary animation data is not necessary,
 * since an RNA-pointer to the necessary data being keyframed,
 * and a pointer to the F-Curve to use have both been provided.
 *
 * This function can't keyframe quaternion channels on some NLA strip types.
 *
 * \param keytype: The "keyframe type" (eBezTriple_KeyframeType), as shown in the Dope Sheet.
 *
 * \param flag: Used for special settings that alter the behavior of the keyframe insertion.
 * These include the 'visual' key-framing modes, quick refresh,
 * and extra keyframe filtering.
 * \return Success.
 */
bool insert_keyframe_direct(ReportList *reports,
                            PointerRNA ptr,
                            PropertyRNA *prop,
                            FCurve *fcu,
                            const AnimationEvalContext *anim_eval_context,
                            eBezTriple_KeyframeType keytype,
                            NlaKeyframingContext *nla_context,
                            eInsertKeyFlags flag);

/**
 * \brief Main Delete Key-Framing API call.
 *
 * Use this to delete keyframe on current frame for relevant channel.
 * Will perform checks just in case.
 * \return The number of key-frames deleted.
 */
int delete_keyframe(Main *bmain, ReportList *reports, ID *id, const RNAPath &rna_path, float cfra);

/**
 * Main Keyframing API call:
 * Use this when validation of necessary animation data isn't necessary as it
 * already exists. It will clear the current buttons fcurve(s).
 *
 * \return The number of f-curves removed.
 */
int clear_keyframe(Main *bmain, ReportList *reports, ID *id, const RNAPath &rna_path);

/** Check if a flag is set for keyframing (per scene takes precedence). */
bool is_keying_flag(const Scene *scene, eKeying_Flag flag);

/**
 * Checks whether a keyframe exists for the given ID-block one the given frame.
 *
 * \param frame: The frame on which to check for a keyframe. This uses a threshold so the float
 * doesn't need to match exactly.
 */
bool id_frame_has_keyframe(ID *id, float frame);

/**
 * Get the settings for key-framing from the given scene.
 */
eInsertKeyFlags get_keyframing_flags(Scene *scene);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto keyframing
 * Notes:
 * - All the defines for this (User-Pref settings and Per-Scene settings)
 *   are defined in DNA_userdef_types.h
 * - Scene settings take precedence over those for user-preferences, with old files
 *   inheriting user-preferences settings for the scene settings
 * - "On/Off + Mode" are stored per Scene, but "settings" are currently stored as user-preferences.
 * \{ */

/** Check if auto-key-framing is enabled (per scene takes precedence). */
bool is_autokey_on(const Scene *scene);

/** Check the mode for auto-keyframing (per scene takes precedence). */
bool is_autokey_mode(const Scene *scene, eAutokey_Mode mode);

/**
 * Auto-keyframing feature - checks for whether anything should be done for the current frame.
 */
bool autokeyframe_cfra_can_key(const Scene *scene, ID *id);

/**
 * Insert keyframes on the given object `ob` based on the auto-keying settings.
 *
 * \param rna_paths: Only inserts keys on those RNA paths.
 */
void autokeyframe_object(bContext *C, Scene *scene, Object *ob, Span<RNAPath> rna_paths);
/**
 * Auto-keyframing feature - for objects
 *
 * \note Context may not always be available,
 * so must check before using it as it's a luxury for a few cases.
 */
bool autokeyframe_object(bContext *C, Scene *scene, Object *ob, KeyingSet *ks);
bool autokeyframe_pchan(bContext *C, Scene *scene, Object *ob, bPoseChannel *pchan, KeyingSet *ks);
/**
 * Auto-keyframing feature - for poses/pose-channels
 *
 * \param targetless_ik: Has targetless ik been done on any channels?
 * \param rna_paths: Only inserts keys on those RNA paths.
 *
 * \note Context may not always be available,
 * so must check before using it as it's a luxury for a few cases.
 */
void autokeyframe_pose_channel(bContext *C,
                               Scene *scene,
                               Object *ob,
                               bPoseChannel *pose_channel,
                               Span<RNAPath> rna_paths,
                               short targetless_ik);
/**
 * Use for auto-key-framing.
 * \param only_if_property_keyed: if true, auto-key-framing only creates keyframes on already keyed
 * properties. This is by design when using buttons. For other callers such as gizmos or sequencer
 * preview transform, creating new animation/keyframes also on non-keyed properties is desired.
 */
bool autokeyframe_property(bContext *C,
                           Scene *scene,
                           PointerRNA *ptr,
                           PropertyRNA *prop,
                           int rnaindex,
                           float cfra,
                           bool only_if_property_keyed);

/** \} */

}  // namespace blender::animrig
