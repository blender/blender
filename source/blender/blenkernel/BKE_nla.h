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

#ifndef __BKE_NLA_H__
#define __BKE_NLA_H__

/** \file
 * \ingroup bke
 */

struct AnimData;
struct Main;
struct NlaStrip;
struct NlaTrack;
struct Scene;
struct Speaker;
struct bAction;

struct PointerRNA;
struct PropertyRNA;

/* ----------------------------- */
/* Data Management */

void BKE_nlastrip_free(ListBase *strips, struct NlaStrip *strip, bool do_id_user);
void BKE_nlatrack_free(ListBase *tracks, struct NlaTrack *nlt, bool do_id_user);
void BKE_nla_tracks_free(ListBase *tracks, bool do_id_user);

struct NlaStrip *BKE_nlastrip_copy(struct Main *bmain,
                                   struct NlaStrip *strip,
                                   const bool use_same_action,
                                   const int flag);
struct NlaTrack *BKE_nlatrack_copy(struct Main *bmain,
                                   struct NlaTrack *nlt,
                                   const bool use_same_actions,
                                   const int flag);
void BKE_nla_tracks_copy(struct Main *bmain, ListBase *dst, ListBase *src, const int flag);

struct NlaTrack *BKE_nlatrack_add(struct AnimData *adt, struct NlaTrack *prev);
struct NlaStrip *BKE_nlastrip_new(struct bAction *act);
struct NlaStrip *BKE_nlastack_add_strip(struct AnimData *adt, struct bAction *act);
struct NlaStrip *BKE_nla_add_soundstrip(struct Main *bmain,
                                        struct Scene *scene,
                                        struct Speaker *spk);

/* ----------------------------- */
/* API */

bool BKE_nlastrips_has_space(ListBase *strips, float start, float end);
void BKE_nlastrips_sort_strips(ListBase *strips);

bool BKE_nlastrips_add_strip(ListBase *strips, struct NlaStrip *strip);

void BKE_nlastrips_make_metas(ListBase *strips, bool is_temp);
void BKE_nlastrips_clear_metas(ListBase *strips, bool only_sel, bool only_temp);
void BKE_nlastrips_clear_metastrip(ListBase *strips, struct NlaStrip *strip);
bool BKE_nlameta_add_strip(struct NlaStrip *mstrip, struct NlaStrip *strip);
void BKE_nlameta_flush_transforms(struct NlaStrip *mstrip);

/* ............ */

struct NlaTrack *BKE_nlatrack_find_active(ListBase *tracks);
void BKE_nlatrack_set_active(ListBase *tracks, struct NlaTrack *nlt);

struct NlaTrack *BKE_nlatrack_find_tweaked(struct AnimData *adt);

void BKE_nlatrack_solo_toggle(struct AnimData *adt, struct NlaTrack *nlt);

bool BKE_nlatrack_has_space(struct NlaTrack *nlt, float start, float end);
void BKE_nlatrack_sort_strips(struct NlaTrack *nlt);

bool BKE_nlatrack_add_strip(struct NlaTrack *nlt, struct NlaStrip *strip);

bool BKE_nlatrack_get_bounds(struct NlaTrack *nlt, float bounds[2]);

/* ............ */

struct NlaStrip *BKE_nlastrip_find_active(struct NlaTrack *nlt);
void BKE_nlastrip_set_active(struct AnimData *adt, struct NlaStrip *strip);

bool BKE_nlastrip_within_bounds(struct NlaStrip *strip, float min, float max);
void BKE_nlastrip_recalculate_bounds(struct NlaStrip *strip);

void BKE_nlastrip_validate_name(struct AnimData *adt, struct NlaStrip *strip);

/* ............ */

bool BKE_nlatrack_has_animated_strips(struct NlaTrack *nlt);
bool BKE_nlatracks_have_animated_strips(ListBase *tracks);
void BKE_nlastrip_validate_fcurves(struct NlaStrip *strip);

bool BKE_nlastrip_has_curves_for_property(const struct PointerRNA *ptr,
                                          const struct PropertyRNA *prop);

void BKE_nla_validate_state(struct AnimData *adt);

/* ............ */

bool BKE_nla_action_is_stashed(struct AnimData *adt, struct bAction *act);
bool BKE_nla_action_stash(struct AnimData *adt);

/* ............ */

void BKE_nla_action_pushdown(struct AnimData *adt);

bool BKE_nla_tweakmode_enter(struct AnimData *adt);
void BKE_nla_tweakmode_exit(struct AnimData *adt);

/* ----------------------------- */
/* Time Mapping */

/* time mapping conversion modes */
enum eNlaTime_ConvertModes {
  /* convert from global time to strip time - for evaluation */
  NLATIME_CONVERT_EVAL = 0,
  /* convert from global time to strip time - for editing corrections */
  // XXX old 0 invert
  NLATIME_CONVERT_UNMAP,
  /* convert from strip time to global time */
  // xxx old 1 invert
  NLATIME_CONVERT_MAP,
};

float BKE_nla_tweakedit_remap(struct AnimData *adt, float cframe, short mode);

#endif
