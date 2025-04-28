/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to modify FCurves.
 */
#pragma once

#include "ANIM_keyframing.hh"

#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

#include "DNA_anim_types.h"

struct AnimData;
struct FCurve;

namespace blender::animrig {

/**
 * All the information needed to look up or create an FCurve.
 *
 * The `std::optional<>` fields are only used for creation. The mandatory fields
 * are used for both creation and lookup.
 */
struct FCurveDescriptor {
  StringRefNull rna_path;
  int array_index;
  std::optional<PropertyType> prop_type;
  std::optional<PropertySubType> prop_subtype;
  std::optional<blender::StringRefNull> channel_group;
};

/* This is used to pass in the settings for a keyframe into a function. */
struct KeyframeSettings {
  eBezTriple_KeyframeType keyframe_type;
  eBezTriple_Handle handle;
  eBezTriple_Interpolation interpolation;
};

/**
 * Helper function to generate the KeyframeSettings struct.
 *
 * \param from_userprefs: if true read the user preferences for the settings, else return static
 * defaults.
 */
KeyframeSettings get_keyframe_settings(bool from_userprefs);

/**
 * Return the first fcurve in `fcurves` that matches `fcurve_descriptor`.
 *
 * If no matching fcurve is found, returns nullptr.
 */
const FCurve *fcurve_find(Span<const FCurve *> fcurves, const FCurveDescriptor &fcurve_descriptor);
FCurve *fcurve_find(Span<FCurve *> fcurves, const FCurveDescriptor &fcurve_descriptor);

/**
 * Create an fcurve for a specific channel, pre-set-up with default flags and
 * interpolation mode.
 *
 * If the channel's property subtype is provided, the fcurve will also be set to
 * the correct color mode based on user preferences.
 */
FCurve *create_fcurve_for_channel(const FCurveDescriptor &fcurve_descriptor);

/**
 * Determine the F-Curve flags suitable for animating an RNA property of the given type.
 */
eFCurve_Flags fcurve_flags_for_property_type(PropertyType prop_type);

/** Initialize the given BezTriple with default values. */
void initialize_bezt(BezTriple *beztr,
                     float2 position,
                     const KeyframeSettings &settings,
                     eFCurve_Flags fcu_flags);

/**
 * Delete the keyframe at `time` on `fcurve` if a key exists there.
 *
 * This does NOT delete the FCurve if it ends up empty. That is for the caller to do.
 *
 * \note `time` is in fcurve time, not scene time.  Any time remapping must be
 * done prior to calling this function.
 *
 * \return True if a keyframe was found at `time` and deleted, false otherwise.
 */
bool fcurve_delete_keyframe_at_time(FCurve *fcurve, float time);

/**
 * Deletes the keyframe at `cfra` on `fcu` if a key exists there, and deletes
 * the fcurve if it was the only keyframe.
 *
 * \note For fcurves on legacy actions only. More specifically, this assumes
 * that the fcurve lives on `adt->action` and that `adt->action` is a legacy
 * action.
 *
 * \note The caller needs to run #BKE_nla_tweakedit_remap to get NLA relative frame.
 *       The caller should also check #BKE_fcurve_is_protected before keying.
 */
bool delete_keyframe_fcurve_legacy(AnimData *adt, FCurve *fcu, float cfra);

/**
 * \brief Lesser Key-framing API call.
 *
 * Use this when validation of necessary animation data isn't necessary as it already
 * exists, and there is a #BezTriple that can be directly copied into the array.
 *
 * This function adds a given #BezTriple to an F-Curve. It will allocate
 * memory for the array if needed, and will insert the #BezTriple into a
 * suitable place in chronological order.
 *
 * \returns The index of the keyframe array into which the bezt has been added.
 *
 * \note Any recalculate of the F-Curve that needs to be done will need to be done by the caller.
 */
int insert_bezt_fcurve(FCurve *fcu, const BezTriple *bezt, eInsertKeyFlags flag);

/**
 * \brief Main Key-framing API call.
 *
 * Use this when validation of necessary animation data isn't necessary as it
 * already exists. It will insert a keyframe using the current value being keyframed.
 *
 * This function is a wrapper for #insert_bezt_fcurve(), and should be used when
 * adding a new keyframe to a curve, when the keyframe doesn't exist anywhere else yet.
 *
 * \returns Either success or an indicator of why keying failed.
 *
 * \param keyframe_type: The type of keyframe (#eBezTriple_KeyframeType).
 * \param flag: Optional flags (#eInsertKeyFlags) for controlling how keys get added
 * and/or whether updates get done.
 */
SingleKeyingResult insert_vert_fcurve(FCurve *fcu,
                                      const float2 position,
                                      const KeyframeSettings &settings,
                                      eInsertKeyFlags flag);

/**
 * \param sample_rate: indicates how many samples per frame should be generated.
 * \param r_samples: Is expected to be an array large enough to hold `sample_count`.
 */
void sample_fcurve_segment(
    const FCurve *fcu, float start_frame, float sample_rate, float *samples, int sample_count);

enum class BakeCurveRemove {
  NONE = 0,
  IN_RANGE = 1,
  OUT_RANGE = 2,
  ALL = 3,
};

/**
 * Creates keyframes in the given range at the given step interval.
 * \param range: start and end frame to bake. Is inclusive on both ends.
 * \param remove_existing: choice which keys to remove in relation to the given range.
 */
void bake_fcurve(FCurve *fcu, blender::int2 range, float step, BakeCurveRemove remove_existing);

/**
 * Fill the space between selected keyframes with keyframes on full frames.
 * E.g. With a key selected on frame 1 and 3 it will insert a key on frame 2.
 */
void bake_fcurve_segments(FCurve *fcu);

/**
 * Checks if some F-Curve has a keyframe for a given frame.
 * \note Used for the buttons to check for keyframes.
 *
 * \param frame: The frame on which to check for a keyframe. A binary search with a threshold is
 * used to find the key, so the float doesn't need to match exactly.
 */
bool fcurve_frame_has_keyframe(const FCurve *fcu, float frame);

}  // namespace blender::animrig
