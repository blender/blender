/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

/** Temp constant defined for these functions only. */
#define NLASTRIP_MIN_LEN_THRESH 0.1f

#include "DNA_listBase.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"

#include "BLI_function_ref.hh"

struct AnimData;
struct ID;
struct LibraryForeachIDData;
struct Main;
struct NlaStrip;
struct NlaTrack;
struct Scene;
struct Speaker;
struct bAction;

struct BlendDataReader;
struct BlendWriter;
struct PointerRNA;
struct PropertyRNA;

/* ----------------------------- */
/* Data Management */

/**
 * Create new NLA Track.
 * The returned pointer is owned by the caller.
 */
struct NlaTrack *BKE_nlatrack_new();

/**
 * Frees the given NLA strip, and calls #BKE_nlastrip_remove_and_free to
 * remove and free all children strips.
 */
void BKE_nlastrip_free(NlaStrip *strip, bool do_id_user);
/**
 * Remove & Frees all NLA strips from the given NLA track,
 * then frees (doesn't remove) the track itself.
 */
void BKE_nlatrack_free(NlaTrack *nlt, bool do_id_user);
/**
 * Free the elements of type NLA Tracks provided in the given list, but do not free
 * the list itself since that is not free-standing
 */
void BKE_nla_tracks_free(ListBase *tracks, bool do_id_user);

/**
 * Copy NLA strip
 *
 * \param use_same_action: When true, the existing action is used (instead of being duplicated)
 * \param flag: Control ID pointers management, see LIB_ID_CREATE_.../LIB_ID_COPY_...
 * flags in BKE_lib_id.hh
 */
NlaStrip *BKE_nlastrip_copy(Main *bmain, NlaStrip *strip, bool use_same_action, int flag);
/**
 * Copy a single NLA Track.
 * \param flag: Control ID pointers management, see LIB_ID_CREATE_.../LIB_ID_COPY_...
 * flags in BKE_lib_id.hh
 */
NlaTrack *BKE_nlatrack_copy(Main *bmain, NlaTrack *nlt, bool use_same_actions, int flag);
/**
 * Copy all NLA data.
 * \param flag: Control ID pointers management, see LIB_ID_CREATE_.../LIB_ID_COPY_...
 * flags in BKE_lib_id.hh
 */
void BKE_nla_tracks_copy(Main *bmain, ListBase *dst, const ListBase *src, int flag);

/**
 * Copy NLA tracks from #adt_source to #adt_dest, and update the active track/strip pointers to
 * point at those copies.
 */
void BKE_nla_tracks_copy_from_adt(Main *bmain,
                                  AnimData *adt_dest,
                                  const AnimData *adt_source,
                                  int flag);

/**
 * Inserts a given NLA track before a specified NLA track within the
 * passed NLA track list.
 */
void BKE_nlatrack_insert_before(ListBase *nla_tracks,
                                NlaTrack *next,
                                NlaTrack *new_track,
                                bool is_liboverride);

/**
 * Inserts a given NLA track after a specified NLA track within the
 * passed NLA track list.
 */
void BKE_nlatrack_insert_after(ListBase *nla_tracks,
                               NlaTrack *prev,
                               NlaTrack *new_track,
                               bool is_liboverride);

/**
 * Calls #BKE_nlatrack_new to create a new NLA track, inserts it before the
 * given NLA track with #BKE_nlatrack_insert_before.
 */
NlaTrack *BKE_nlatrack_new_before(ListBase *nla_tracks, NlaTrack *next, bool is_liboverride);

/**
 * Calls #BKE_nlatrack_new to create a new NLA track, inserts it after the
 * given NLA track with #BKE_nlatrack_insert_after.
 */
NlaTrack *BKE_nlatrack_new_after(ListBase *nla_tracks, NlaTrack *prev, bool is_liboverride);

/**
 * Calls #BKE_nlatrack_new to create a new NLA track, inserts it as the head of the
 * NLA track list with #BKE_nlatrack_new_before.
 */
NlaTrack *BKE_nlatrack_new_head(ListBase *nla_tracks, bool is_liboverride);

/**
 * Calls #BKE_nlatrack_new to create a new NLA track, inserts it as the tail of the
 * NLA track list with #BKE_nlatrack_new_after.
 */
NlaTrack *BKE_nlatrack_new_tail(ListBase *nla_tracks, const bool is_liboverride);

/**
 * Removes the given NLA track from the list of tracks provided.
 */
void BKE_nlatrack_remove(ListBase *tracks, NlaTrack *nlt);

/**
 * Remove the given NLA track from the list of NLA tracks, free the track's data,
 * and the track itself.
 */
void BKE_nlatrack_remove_and_free(ListBase *tracks, NlaTrack *nlt, bool do_id_user);

/**
 * Return whether this NLA track is enabled.
 *
 * If any track is solo'ed: returns true when this is the solo'ed one.
 * If no track is solo'ed: returns true when this track is not muted.
 */
bool BKE_nlatrack_is_enabled(const AnimData &adt, const NlaTrack &nlt);

/**
 * Compute the length of the passed strip's clip, unless the clip length
 * is zero in which case a non-zero value is returned.
 *
 * WARNING: this function is *very narrow* and special-cased in its
 * application.  It was introduced as part of the fix for issue #107030,
 * as a way to collect a bunch of whack-a-mole inline applications of this
 * logic in one place.  The logic itself isn't principled in any way,
 * and should almost certainly not be used anywhere that it isn't already,
 * short of one of those whack-a-mole inline places being overlooked.
 *
 * The underlying purpose of this function is to ensure that the computed
 * clip length for an NLA strip is (in certain places) never zero, in order to
 * avoid the strip's scale having to be infinity.  In other words, it's a
 * hack.  But at least now it's a hack collected in one place.
 *
 */
float BKE_nla_clip_length_get_nonzero(const NlaStrip *strip);

/**
 * Ensure the passed range has non-zero length, using the same logic as
 * `BKE_nla_clip_length_get_nonzero` to determine the new non-zero length.
 *
 * See the documentation for `BKE_nla_clip_length_get_nonzero` for the
 * reason this function exists and the issues around its use.
 *
 * Usage: both `actstart` and `r_actend` should already be set to the
 * start/end values of a strip's clip.  `r_actend` will be modified
 * if necessary to ensure the range is non-zero in length.
 */
void BKE_nla_clip_length_ensure_nonzero(const float *actstart, float *r_actend);

/**
 * Create a NLA Strip referencing the given Action.
 *
 * If this is a layered Action, a suitable slot is automatically chosen. If
 * there is none available, no slot will be assigned.
 */
NlaStrip *BKE_nlastrip_new(bAction *act, ID &animated_id);

/**
 * Create a NLA Strip referencing the given Action & Slot.
 *
 * If the Action is legacy, the slot is ignored.
 *
 * This can return nullptr only when act == nullptr or when the slot ID type
 * does not match the given animated ID.
 */
NlaStrip *BKE_nlastrip_new_for_slot(bAction *act,
                                    blender::animrig::slot_handle_t slot_handle,
                                    ID &animated_id);

/*
 * Removes the given NLA strip from the list of strips provided.
 */
void BKE_nlastrip_remove(ListBase *strips, NlaStrip *strip);

/*
 * Removes the given NLA strip from the list of strips provided, and frees it's memory.
 */
void BKE_nlastrip_remove_and_free(ListBase *strips, NlaStrip *strip, const bool do_id_user);

/**
 * Add new NLA-strip to the top of the NLA stack - i.e.
 * into the last track if space, or a new one otherwise.
 */
NlaStrip *BKE_nlastack_add_strip(OwnedAnimData owned_adt, const bool is_liboverride);

/**
 * Add a NLA Strip referencing the given speaker's sound.
 */
NlaStrip *BKE_nla_add_soundstrip(Main *bmain, Scene *scene, Speaker *speaker);

/**
 * Callback used by lib_query to walk over all ID usages
 * (mimics `foreach_id` callback of #IDTypeInfo structure).
 */
void BKE_nla_strip_foreach_id(NlaStrip *strip, LibraryForeachIDData *data);

/* ----------------------------- */
/* API */

/**
 * Check if there is any space in the given list to add the given strip.
 */
bool BKE_nlastrips_has_space(ListBase *strips, float start, float end);
/**
 * Rearrange the strips in the track so that they are always in order
 * (usually only needed after a strip has been moved)
 */
void BKE_nlastrips_sort_strips(ListBase *strips);

/**
 * Add the given NLA-Strip to the given list of strips, assuming that it
 * isn't currently a member of another list, NULL, or conflicting with existing
 * strips position.
 */
void BKE_nlastrips_add_strip_unsafe(ListBase *strips, NlaStrip *strip);

/**
 * NULL checks incoming strip and verifies no overlap / invalid
 * configuration against other strips in NLA Track before calling
 * #BKE_nlastrips_add_strip_unsafe.
 */
bool BKE_nlastrips_add_strip(ListBase *strips, NlaStrip *strip);

/**
 * Convert 'islands' (i.e. continuous string of) selected strips to be
 * contained within 'Meta-Strips' which act as strips which contain strips.
 *
 * \param is_temp: are the meta-strips to be created 'temporary' ones used for transforms?
 */
void BKE_nlastrips_make_metas(ListBase *strips, bool is_temp);
/**
 * Remove meta-strips (i.e. flatten the list of strips) from the top-level of the list of strips.
 *
 * \param only_sel: only consider selected meta-strips, otherwise all meta-strips are removed
 * \param only_temp: only remove the 'temporary' meta-strips used for transforms
 */
void BKE_nlastrips_clear_metas(ListBase *strips, bool only_sel, bool only_temp);
/**
 * Split a meta-strip into a set of normal strips.
 */
void BKE_nlastrips_clear_metastrip(ListBase *strips, NlaStrip *strip);
/**
 * Add the given NLA-Strip to the given Meta-Strip, assuming that the
 * strip isn't attached to any list of strips
 */
bool BKE_nlameta_add_strip(NlaStrip *mstrip, NlaStrip *strip);
/**
 * Adjust the settings of NLA-Strips contained within a Meta-Strip (recursively),
 * until the Meta-Strips children all fit within the Meta-Strip's new dimensions
 */
void BKE_nlameta_flush_transforms(NlaStrip *mstrip);

/* ............ */

/**
 * Find the active NLA-track for the given stack.
 */
NlaTrack *BKE_nlatrack_find_active(ListBase *tracks);
/**
 * Make the given NLA-track the active one for the given stack. If no track is provided,
 * this function can be used to simply deactivate all the NLA tracks in the given stack too.
 */
void BKE_nlatrack_set_active(ListBase *tracks, NlaTrack *nlt);

/**
 * Get the NLA Track that the active action/action strip comes from,
 * since this info is not stored in AnimData. It also isn't as simple
 * as just using the active track, since multiple tracks may have been
 * entered at the same time.
 */
NlaTrack *BKE_nlatrack_find_tweaked(AnimData *adt);

/**
 * Toggle the 'solo' setting for the given NLA-track, making sure that it is the only one
 * that has this status in its AnimData block.
 */
void BKE_nlatrack_solo_toggle(AnimData *adt, NlaTrack *nlt);

/**
 * Check if there is any space in the given track to add a strip of the given length.
 */
bool BKE_nlatrack_has_space(NlaTrack *nlt, float start, float end);

/**
 * Check to see if there are any NLA strips in the NLA tracks.
 */
bool BKE_nlatrack_has_strips(ListBase *tracks);

/**
 * Rearrange the strips in the track so that they are always in order
 * (usually only needed after a strip has been moved).
 */
void BKE_nlatrack_sort_strips(NlaTrack *nlt);

/**
 * Add the given NLA-Strip to the given NLA-Track.
 * Calls #BKE_nlastrips_add_strip to check if strip can be added.
 */
bool BKE_nlatrack_add_strip(NlaTrack *nlt, NlaStrip *strip, bool is_liboverride);

/**
 * Remove the NLA-Strip from the given NLA-Track.
 */
void BKE_nlatrack_remove_strip(NlaTrack *track, NlaStrip *strip);

/**
 * Get the extents of the given NLA-Track including gaps between strips,
 * returning whether this succeeded or not
 */
bool BKE_nlatrack_get_bounds(NlaTrack *nlt, float bounds[2]);
/**
 * Check whether given NLA track is not local (i.e. from linked data) when the object is a library
 * override.
 *
 * \param nlt: May be NULL, in which case we consider it as a non-local track case.
 */
bool BKE_nlatrack_is_nonlocal_in_liboverride(const ID *id, const NlaTrack *nlt);

/* ............ */

/**
 * Compute the left-hand-side 'frame limit' of that strip, in its NLA track.
 *
 * \details This is either :
 * - the end frame of the previous strip, if the strip's track contains another strip on it left
 * - the macro MINFRAMEF, if no strips are to the left of this strip in its track
 *
 * \param strip: The strip to compute the left-hand-side 'frame limit' of.
 * \return The beginning frame of the previous strip, or MINFRAMEF if no strips are next in that
 * track.
 */
float BKE_nlastrip_compute_frame_from_previous_strip(NlaStrip *strip);
/**
 * Compute the right-hand-side 'frame limit' of that strip, in its NLA track.
 *
 * \details This is either :
 *
 * - the begin frame of the next strip, if the strip's track contains another strip on it right
 * - the macro MAXFRAMEF, if no strips are to the right of this strip in its track
 *
 * \param strip: The strip to compute the right-hand-side 'frame limit' of.
 * \return The beginning frame of the next strip, or MAXFRAMEF if no strips are next in that track.
 */
float BKE_nlastrip_compute_frame_to_next_strip(NlaStrip *strip);

/**
 * Returns the next strip in this strip's NLA track, or a null pointer.
 *
 * \param strip: The strip to find the next trip from.
 * \param check_transitions: Whether or not to skip transitions.
 * \return The next strip in the track, or NULL if none are present.
 */
NlaStrip *BKE_nlastrip_next_in_track(NlaStrip *strip, bool skip_transitions);

/**
 * Returns the previous strip in this strip's NLA track, or a null pointer.
 *
 * \param strip: The strip to find the previous trip from.
 * \param check_transitions: Whether or not to skip transitions.
 * \return The previous strip in the track, or NULL if none are present.
 */
NlaStrip *BKE_nlastrip_prev_in_track(NlaStrip *strip, bool skip_transitions);

/* ............ */

/**
 * Find the active NLA-strip within the given track.
 */
NlaStrip *BKE_nlastrip_find_active(NlaTrack *nlt);
/**
 * Make the given NLA-Strip the active one within the given block.
 */
void BKE_nlastrip_set_active(AnimData *adt, NlaStrip *strip);
/**
 * Find the NLA-strip with the given name within the given track.
 *
 * \return pointer to the strip, or nullptr when not found.
 */
NlaStrip *BKE_nlastrip_find_by_name(NlaTrack *nlt, const char *name);

/**
 * Does the given NLA-strip fall within the given bounds (times)?.
 */
bool BKE_nlastrip_within_bounds(NlaStrip *strip, float min, float max);
/**
 * Return the distance from the given frame to the NLA strip, measured in frames.
 * If the given frame intersects the NLA strip, the distance is zero.
 */
float BKE_nlastrip_distance_to_frame(const NlaStrip *strip, float timeline_frame);
/**
 * Recalculate the start and end frames for the current strip, after changing
 * the extents of the action or the mapping (repeats or scale factor) info.
 */
void BKE_nlastrip_recalculate_bounds(NlaStrip *strip);
/**
 * Recalculate the start and end frames for the strip to match the bounds of its action such that
 * the overall NLA animation result is unchanged.
 */
void BKE_nlastrip_recalculate_bounds_sync_action(NlaStrip *strip);

/**
 * Recalculate the blend-in and blend-out values after a strip transform update.
 */
void BKE_nlastrip_recalculate_blend(NlaStrip *strip);

/**
 * Find (and set) a unique name for a strip from the whole AnimData block
 * Uses a similar method to the BLI method, but is implemented differently
 * as we need to ensure that the name is unique over several lists of tracks,
 * not just a single track.
 */
void BKE_nlastrip_validate_name(AnimData *adt, NlaStrip *strip);

/* ............ */

/**
 * Check if the given NLA-Track has any strips with their own F-Curves.
 */
bool BKE_nlatrack_has_animated_strips(NlaTrack *nlt);
/**
 * Check if given NLA-Tracks have any strips with their own F-Curves.
 */
bool BKE_nlatracks_have_animated_strips(ListBase *tracks);
/**
 * Validate the NLA-Strips 'control' F-Curves based on the flags set.
 */
void BKE_nlastrip_validate_fcurves(NlaStrip *strip);

/**
 * Delete the NLA-Strip's control F-Curve.
 *
 * This also ensures that the strip's flags are correctly updated.
 *
 * \return Whether the F-Curve was actually removed.
 */
bool BKE_nlastrip_controlcurve_remove(NlaStrip *strip, FCurve *fcurve);

/**
 * Check if the given RNA pointer + property combo should be handled by
 * NLA strip curves or not.
 */
bool BKE_nlastrip_has_curves_for_property(const PointerRNA *ptr, const PropertyRNA *prop);

/**
 * Ensure that auto-blending and other settings are set correctly.
 */
void BKE_nla_validate_state(AnimData *adt);

/* ............ */

/**
 * Check if an action+slot combination is "stashed" in the NLA already.
 *
 * The criteria for this are:
 * 1) The action+slot in question lives in a "stash" track.
 * 2) We only check first-level strips. That is, we will not check inside meta strips.
 */
bool BKE_nla_action_slot_is_stashed(AnimData *adt,
                                    bAction *act,
                                    blender::animrig::slot_handle_t slot_handle);
/**
 * "Stash" an action (i.e. store it as a track/layer in the NLA, but non-contributing)
 * to retain it in the file for future uses.
 */
bool BKE_nla_action_stash(OwnedAnimData owned_adt, bool is_liboverride);

/* ............ */

/**
 * For the given AnimData block, add the active action to the NLA
 * stack (i.e. 'push-down' action). The UI should only allow this
 * for normal editing only (i.e. not in edit-mode for some strip's action),
 * so no checks for this are performed.
 *
 * TODO: maybe we should have checks for this too.
 */
void BKE_nla_action_pushdown(OwnedAnimData owned_adt, bool is_liboverride);

/**
 * Find the active strip + track combination, and set them up as the tweaking track,
 * and return if successful or not.
 */
bool BKE_nla_tweakmode_enter(OwnedAnimData owned_adt);
/**
 * Exit tweak-mode for this AnimData block.
 */
void BKE_nla_tweakmode_exit(OwnedAnimData owned_adt);

/**
 * Clear all NLA Tweak Mode related flags on the ADT, tracks, and strips.
 */
void BKE_nla_tweakmode_clear_flags(AnimData *adt);

/**
 * Partially exit NLA tweak-mode for this AnimData block, without following any
 * pointers to other data-blocks. This means no strip length syncing (as that
 * needs to know info about the strip's Action), no reference counting on the
 * Action, and no user update on the Action Slot.
 *
 * This function just writes to the AnimData-owned data. It is intended to be
 * used in blend-file reading code, which performs a reference count + rebuilds
 * the slot user map later anyway.
 */
void BKE_nla_tweakmode_exit_nofollowptr(AnimData *adt);

/* ----------------------------- */
/* Time Mapping */

/* time mapping conversion modes */
enum eNlaTime_ConvertModes {
  /* convert from global time to strip time - for evaluation */
  NLATIME_CONVERT_EVAL = 0,
  /* convert from global time to strip time - for editing corrections */
  /* XXX: old 0 invert. */
  NLATIME_CONVERT_UNMAP,
  /* convert from strip time to global time */
  /* XXX: old 1 invert. */
  NLATIME_CONVERT_MAP,
};

/**
 * Non clipped mapping for strip-time <-> global time.
 *
 * Public API method - perform this mapping using the given AnimData block
 * and perform any necessary sanity checks on the value.
 *
 * \note Do not call this with an `adt` obtained from an `bAnimListElem`.
 * Instead, use `ANIM_nla_tweakedit_remap()` for that. This is because not all
 * data that might be in an `bAnimListElem` should be nla remapped, and this
 * function cannot account for that, whereas `ANIM_nla_tweakedit_remap()` takes
 * the `bAnimListElem` directly and makes sure the right thing is done.
 */
float BKE_nla_tweakedit_remap(AnimData *adt, float cframe, eNlaTime_ConvertModes mode);

/* ----------------------------- */
/* .blend file API */

void BKE_nla_blend_write(BlendWriter *writer, ListBase *tracks);
void BKE_nla_blend_read_data(BlendDataReader *reader, ID *id_owner, ListBase *tracks);

/**
 * Ensure NLA Tweak Mode related flags & pointers are consistent.
 *
 * This may mean that tweak mode is exited, if not all relevant pointers can be
 * set correctly.
 */
void BKE_nla_liboverride_post_process(ID *id, AnimData *adt);

/**
 * Print the ADT flags, NLA tracks, strips, their flags, and other info, to the console.
 *
 * \param adt: the ADT to show. If NULL, it will be determined from owner_id.
 * \param owner_id: the ID that owns this ADT. If given, its name will be printed in the console
 * output. If NULL, that won't happen.
 *
 * Either of the parameters can be NULL, but not both.
 */
void BKE_nla_debug_print_flags(AnimData *adt, ID *owner_id);

namespace blender::bke::nla {

/**
 * Call the callback for every strip of this ID's NLA.
 *
 * Automatically recurses into meta-strips.
 *
 * The callback should return a 'keep going' status, i.e. `true` to keep
 * looping, and `false` to break the loop.
 *
 * \return the last value returned by the callback, so `true` if the loop ran
 * until the end, and `false` it was stopped by the callback. When there is no
 * NLA or it has no strips, returns `true` because the loop ran until its
 * natural end and wasn't stopped by the callback.
 */
bool foreach_strip(ID *id, blender::FunctionRef<bool(NlaStrip *)> callback);

/**
 * Call the callback for every strip of this AnimData's NLA.
 *
 * Automatically recurses into meta-strips.
 *
 * The callback should return a 'keep going' status, i.e. `true` to keep
 * looping, and `false` to break the loop.
 *
 * \return the last value returned by the callback, so `true` if the loop ran
 * until the end, and `false` it was stopped by the callback. When there is no
 * NLA or it has no strips, returns `true` because the loop ran until its
 * natural end and wasn't stopped by the callback.
 */
bool foreach_strip_adt(const AnimData &adt, blender::FunctionRef<bool(NlaStrip *)> callback);

}  // namespace blender::bke::nla
