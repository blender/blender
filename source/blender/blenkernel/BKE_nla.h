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

#ifdef __cplusplus
extern "C" {
#endif

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
struct BlendLibReader;
struct BlendWriter;
struct PointerRNA;
struct PropertyRNA;

/* ----------------------------- */
/* Data Management */

/**
 * Create new NLA Track.
 * The returned pointer is owned by the caller.
 */
struct NlaTrack *BKE_nlatrack_new(void);

/**
 * Frees the given NLA strip, and calls #BKE_nlastrip_remove_and_free to
 * remove and free all children strips.
 */
void BKE_nlastrip_free(struct NlaStrip *strip, bool do_id_user);
/**
 * Remove & Frees all NLA strips from the given NLA track,
 * then frees (doesn't remove) the track itself.
 */
void BKE_nlatrack_free(struct NlaTrack *nlt, bool do_id_user);
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
struct NlaStrip *BKE_nlastrip_copy(struct Main *bmain,
                                   struct NlaStrip *strip,
                                   bool use_same_action,
                                   int flag);
/**
 * Copy a single NLA Track.
 * \param flag: Control ID pointers management, see LIB_ID_CREATE_.../LIB_ID_COPY_...
 * flags in BKE_lib_id.hh
 */
struct NlaTrack *BKE_nlatrack_copy(struct Main *bmain,
                                   struct NlaTrack *nlt,
                                   bool use_same_actions,
                                   int flag);
/**
 * Copy all NLA data.
 * \param flag: Control ID pointers management, see LIB_ID_CREATE_.../LIB_ID_COPY_...
 * flags in BKE_lib_id.hh
 */
void BKE_nla_tracks_copy(struct Main *bmain, ListBase *dst, const ListBase *src, int flag);

/**
 * Copy NLA tracks from #adt_source to #adt_dest, and update the active track/strip pointers to
 * point at those copies.
 */
void BKE_nla_tracks_copy_from_adt(struct Main *bmain,
                                  struct AnimData *adt_dest,
                                  const struct AnimData *adt_source,
                                  int flag);

/**
 * Inserts a given NLA track before a specified NLA track within the
 * passed NLA track list.
 */
void BKE_nlatrack_insert_before(ListBase *nla_tracks,
                                struct NlaTrack *next,
                                struct NlaTrack *new_track,
                                bool is_liboverride);

/**
 * Inserts a given NLA track after a specified NLA track within the
 * passed NLA track list.
 */
void BKE_nlatrack_insert_after(ListBase *nla_tracks,
                               struct NlaTrack *prev,
                               struct NlaTrack *new_track,
                               bool is_liboverride);

/**
 * Calls #BKE_nlatrack_new to create a new NLA track, inserts it before the
 * given NLA track with #BKE_nlatrack_insert_before.
 */
struct NlaTrack *BKE_nlatrack_new_before(ListBase *nla_tracks,
                                         struct NlaTrack *next,
                                         bool is_liboverride);

/**
 * Calls #BKE_nlatrack_new to create a new NLA track, inserts it after the
 * given NLA track with #BKE_nlatrack_insert_after.
 */
struct NlaTrack *BKE_nlatrack_new_after(ListBase *nla_tracks,
                                        struct NlaTrack *prev,
                                        bool is_liboverride);

/**
 * Calls #BKE_nlatrack_new to create a new NLA track, inserts it as the head of the
 * NLA track list with #BKE_nlatrack_new_before.
 */
struct NlaTrack *BKE_nlatrack_new_head(ListBase *nla_tracks, bool is_liboverride);

/**
 * Calls #BKE_nlatrack_new to create a new NLA track, inserts it as the tail of the
 * NLA track list with #BKE_nlatrack_new_after.
 */
struct NlaTrack *BKE_nlatrack_new_tail(ListBase *nla_tracks, const bool is_liboverride);

/**
 * Removes the given NLA track from the list of tracks provided.
 */
void BKE_nlatrack_remove(ListBase *tracks, struct NlaTrack *nlt);

/**
 * Remove the given NLA track from the list of NLA tracks, free the track's data,
 * and the track itself.
 */
void BKE_nlatrack_remove_and_free(ListBase *tracks, struct NlaTrack *nlt, bool do_id_user);

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
 */
struct NlaStrip *BKE_nlastrip_new(struct bAction *act);

/*
 * Removes the given NLA strip from the list of strips provided.
 */
void BKE_nlastrip_remove(ListBase *strips, struct NlaStrip *strip);

/*
 * Removes the given NLA strip from the list of strips provided, and frees it's memory.
 */
void BKE_nlastrip_remove_and_free(ListBase *strips, struct NlaStrip *strip, const bool do_id_user);

/**
 * Add new NLA-strip to the top of the NLA stack - i.e.
 * into the last track if space, or a new one otherwise.
 */
struct NlaStrip *BKE_nlastack_add_strip(struct AnimData *adt,
                                        struct bAction *act,
                                        bool is_liboverride);
/**
 * Add a NLA Strip referencing the given speaker's sound.
 */
struct NlaStrip *BKE_nla_add_soundstrip(struct Main *bmain,
                                        struct Scene *scene,
                                        struct Speaker *speaker);

/**
 * Callback used by lib_query to walk over all ID usages
 * (mimics `foreach_id` callback of #IDTypeInfo structure).
 */
void BKE_nla_strip_foreach_id(struct NlaStrip *strip, struct LibraryForeachIDData *data);

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
void BKE_nlastrips_add_strip_unsafe(ListBase *strips, struct NlaStrip *strip);

/**
 * NULL checks incoming strip and verifies no overlap / invalid
 * configuration against other strips in NLA Track before calling
 * #BKE_nlastrips_add_strip_unsafe.
 */
bool BKE_nlastrips_add_strip(ListBase *strips, struct NlaStrip *strip);

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
void BKE_nlastrips_clear_metastrip(ListBase *strips, struct NlaStrip *strip);
/**
 * Add the given NLA-Strip to the given Meta-Strip, assuming that the
 * strip isn't attached to any list of strips
 */
bool BKE_nlameta_add_strip(struct NlaStrip *mstrip, struct NlaStrip *strip);
/**
 * Adjust the settings of NLA-Strips contained within a Meta-Strip (recursively),
 * until the Meta-Strips children all fit within the Meta-Strip's new dimensions
 */
void BKE_nlameta_flush_transforms(struct NlaStrip *mstrip);

/* ............ */

/**
 * Find the active NLA-track for the given stack.
 */
struct NlaTrack *BKE_nlatrack_find_active(ListBase *tracks);
/**
 * Make the given NLA-track the active one for the given stack. If no track is provided,
 * this function can be used to simply deactivate all the NLA tracks in the given stack too.
 */
void BKE_nlatrack_set_active(ListBase *tracks, struct NlaTrack *nlt);

/**
 * Get the NLA Track that the active action/action strip comes from,
 * since this info is not stored in AnimData. It also isn't as simple
 * as just using the active track, since multiple tracks may have been
 * entered at the same time.
 */
struct NlaTrack *BKE_nlatrack_find_tweaked(struct AnimData *adt);

/**
 * Toggle the 'solo' setting for the given NLA-track, making sure that it is the only one
 * that has this status in its AnimData block.
 */
void BKE_nlatrack_solo_toggle(struct AnimData *adt, struct NlaTrack *nlt);

/**
 * Check if there is any space in the given track to add a strip of the given length.
 */
bool BKE_nlatrack_has_space(struct NlaTrack *nlt, float start, float end);

/**
 * Check to see if there are any NLA strips in the NLA tracks.
 */
bool BKE_nlatrack_has_strips(ListBase *tracks);

/**
 * Rearrange the strips in the track so that they are always in order
 * (usually only needed after a strip has been moved).
 */
void BKE_nlatrack_sort_strips(struct NlaTrack *nlt);

/**
 * Add the given NLA-Strip to the given NLA-Track.
 * Calls #BKE_nlastrips_add_strip to check if strip can be added.
 */
bool BKE_nlatrack_add_strip(struct NlaTrack *nlt, struct NlaStrip *strip, bool is_liboverride);

/**
 * Remove the NLA-Strip from the given NLA-Track.
 */
void BKE_nlatrack_remove_strip(struct NlaTrack *track, struct NlaStrip *strip);

/**
 * Get the extents of the given NLA-Track including gaps between strips,
 * returning whether this succeeded or not
 */
bool BKE_nlatrack_get_bounds(struct NlaTrack *nlt, float bounds[2]);
/**
 * Check whether given NLA track is not local (i.e. from linked data) when the object is a library
 * override.
 *
 * \param nlt: May be NULL, in which case we consider it as a non-local track case.
 */
bool BKE_nlatrack_is_nonlocal_in_liboverride(const struct ID *id, const struct NlaTrack *nlt);

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
float BKE_nlastrip_compute_frame_from_previous_strip(struct NlaStrip *strip);
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
float BKE_nlastrip_compute_frame_to_next_strip(struct NlaStrip *strip);

/**
 * Returns the next strip in this strip's NLA track, or a null pointer.
 *
 * \param strip: The strip to find the next trip from.
 * \param check_transitions: Whether or not to skip transitions.
 * \return The next strip in the track, or NULL if none are present.
 */
struct NlaStrip *BKE_nlastrip_next_in_track(struct NlaStrip *strip, bool skip_transitions);

/**
 * Returns the previous strip in this strip's NLA track, or a null pointer.
 *
 * \param strip: The strip to find the previous trip from.
 * \param check_transitions: Whether or not to skip transitions.
 * \return The previous strip in the track, or NULL if none are present.
 */
struct NlaStrip *BKE_nlastrip_prev_in_track(struct NlaStrip *strip, bool skip_transitions);

/* ............ */

/**
 * Find the active NLA-strip within the given track.
 */
struct NlaStrip *BKE_nlastrip_find_active(struct NlaTrack *nlt);
/**
 * Make the given NLA-Strip the active one within the given block.
 */
void BKE_nlastrip_set_active(struct AnimData *adt, struct NlaStrip *strip);
/**
 * Find the NLA-strip with the given name within the given track.
 *
 * \return pointer to the strip, or nullptr when not found.
 */
struct NlaStrip *BKE_nlastrip_find_by_name(struct NlaTrack *nlt, const char *name);

/**
 * Does the given NLA-strip fall within the given bounds (times)?.
 */
bool BKE_nlastrip_within_bounds(struct NlaStrip *strip, float min, float max);
/**
 * Return the distance from the given frame to the NLA strip, measured in frames.
 * If the given frame intersects the NLA strip, the distance is zero.
 */
float BKE_nlastrip_distance_to_frame(const struct NlaStrip *strip, float timeline_frame);
/**
 * Recalculate the start and end frames for the current strip, after changing
 * the extents of the action or the mapping (repeats or scale factor) info.
 */
void BKE_nlastrip_recalculate_bounds(struct NlaStrip *strip);
/**
 * Recalculate the start and end frames for the strip to match the bounds of its action such that
 * the overall NLA animation result is unchanged.
 */
void BKE_nlastrip_recalculate_bounds_sync_action(struct NlaStrip *strip);

/**
 * Recalculate the blend-in and blend-out values after a strip transform update.
 */
void BKE_nlastrip_recalculate_blend(struct NlaStrip *strip);

/**
 * Find (and set) a unique name for a strip from the whole AnimData block
 * Uses a similar method to the BLI method, but is implemented differently
 * as we need to ensure that the name is unique over several lists of tracks,
 * not just a single track.
 */
void BKE_nlastrip_validate_name(struct AnimData *adt, struct NlaStrip *strip);

/* ............ */

/**
 * Check if the given NLA-Track has any strips with own F-Curves.
 */
bool BKE_nlatrack_has_animated_strips(struct NlaTrack *nlt);
/**
 * Check if given NLA-Tracks have any strips with own F-Curves.
 */
bool BKE_nlatracks_have_animated_strips(ListBase *tracks);
/**
 * Validate the NLA-Strips 'control' F-Curves based on the flags set.
 */
void BKE_nlastrip_validate_fcurves(struct NlaStrip *strip);

/**
 * Check if the given RNA pointer + property combo should be handled by
 * NLA strip curves or not.
 */
bool BKE_nlastrip_has_curves_for_property(const struct PointerRNA *ptr,
                                          const struct PropertyRNA *prop);

/**
 * Ensure that auto-blending and other settings are set correctly.
 */
void BKE_nla_validate_state(struct AnimData *adt);

/* ............ */

/**
 * Check if an action is "stashed" in the NLA already
 *
 * The criteria for this are:
 * 1) The action in question lives in a "stash" track.
 * 2) We only check first-level strips. That is, we will not check inside meta strips.
 */
bool BKE_nla_action_is_stashed(struct AnimData *adt, struct bAction *act);
/**
 * "Stash" an action (i.e. store it as a track/layer in the NLA, but non-contributing)
 * to retain it in the file for future uses.
 */
bool BKE_nla_action_stash(struct AnimData *adt, bool is_liboverride);

/* ............ */

/**
 * For the given AnimData block, add the active action to the NLA
 * stack (i.e. 'push-down' action). The UI should only allow this
 * for normal editing only (i.e. not in edit-mode for some strip's action),
 * so no checks for this are performed.
 *
 * TODO: maybe we should have checks for this too.
 */
void BKE_nla_action_pushdown(struct AnimData *adt, bool is_liboverride);

/**
 * Find the active strip + track combination, and set them up as the tweaking track,
 * and return if successful or not.
 */
bool BKE_nla_tweakmode_enter(struct AnimData *adt);
/**
 * Exit tweak-mode for this AnimData block.
 */
void BKE_nla_tweakmode_exit(struct AnimData *adt);

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
 * Non clipped mapping for strip-time <-> global time:
 * `mode = eNlaTime_ConvertModes -> NLATIME_CONVERT_*`
 *
 * Public API method - perform this mapping using the given AnimData block
 * and perform any necessary sanity checks on the value
 */
float BKE_nla_tweakedit_remap(struct AnimData *adt, float cframe, short mode);

/* ----------------------------- */
/* .blend file API */

void BKE_nla_blend_write(struct BlendWriter *writer, struct ListBase *tracks);
void BKE_nla_blend_read_data(struct BlendDataReader *reader,
                             struct ID *id_owner,
                             struct ListBase *tracks);

#ifdef __cplusplus
}
#endif
