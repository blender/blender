/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct LibraryForeachIDData;
struct Main;
struct NlaStrip;
struct NlaTrack;
struct Scene;
struct Speaker;
struct bAction;

struct BlendDataReader;
struct BlendExpander;
struct BlendLibReader;
struct BlendWriter;
struct PointerRNA;
struct PropertyRNA;

/* ----------------------------- */
/* Data Management */

/**
 * Remove the given NLA strip from the NLA track it occupies, free the strip's data,
 * and the strip itself.
 */
void BKE_nlastrip_free(ListBase *strips, struct NlaStrip *strip, bool do_id_user);
/**
 * Remove the given NLA track from the set of NLA tracks, free the track's data,
 * and the track itself.
 */
void BKE_nlatrack_free(ListBase *tracks, struct NlaTrack *nlt, bool do_id_user);
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
 * flags in BKE_lib_id.h
 */
struct NlaStrip *BKE_nlastrip_copy(struct Main *bmain,
                                   struct NlaStrip *strip,
                                   bool use_same_action,
                                   int flag);
/**
 * Copy a single NLA Track.
 * \param flag: Control ID pointers management, see LIB_ID_CREATE_.../LIB_ID_COPY_...
 * flags in BKE_lib_id.h
 */
struct NlaTrack *BKE_nlatrack_copy(struct Main *bmain,
                                   struct NlaTrack *nlt,
                                   bool use_same_actions,
                                   int flag);
/**
 * Copy all NLA data.
 * \param flag: Control ID pointers management, see LIB_ID_CREATE_.../LIB_ID_COPY_...
 * flags in BKE_lib_id.h
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
 * Add a NLA Track to the given AnimData.
 * \param prev: NLA-Track to add the new one after.
 */
struct NlaTrack *BKE_nlatrack_add(struct AnimData *adt,
                                  struct NlaTrack *prev,
                                  bool is_liboverride);
/**
 * Create a NLA Strip referencing the given Action.
 */
struct NlaStrip *BKE_nlastrip_new(struct bAction *act);
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
 * isn't currently a member of another list
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
 * Rearrange the strips in the track so that they are always in order
 * (usually only needed after a strip has been moved).
 */
void BKE_nlatrack_sort_strips(struct NlaTrack *nlt);

/**
 * Add the given NLA-Strip to the given NLA-Track, assuming that it
 * isn't currently attached to another one.
 */
bool BKE_nlatrack_add_strip(struct NlaTrack *nlt, struct NlaStrip *strip, bool is_liboverride);

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
 * Find the active NLA-strip within the given track.
 */
struct NlaStrip *BKE_nlastrip_find_active(struct NlaTrack *nlt);
/**
 * Make the given NLA-Strip the active one within the given block.
 */
void BKE_nlastrip_set_active(struct AnimData *adt, struct NlaStrip *strip);

/**
 * Does the given NLA-strip fall within the given bounds (times)?.
 */
bool BKE_nlastrip_within_bounds(struct NlaStrip *strip, float min, float max);
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
void BKE_nla_blend_read_data(struct BlendDataReader *reader, struct ListBase *tracks);
void BKE_nla_blend_read_lib(struct BlendLibReader *reader, struct ID *id, struct ListBase *tracks);
void BKE_nla_blend_read_expand(struct BlendExpander *expander, struct ListBase *tracks);

#ifdef __cplusplus
}
#endif
