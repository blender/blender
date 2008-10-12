/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place * Suite 330, Boston, MA  02111*1307, USA.
 *
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender (with some old code)
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BIF_KEYFRAMING_H
#define BIF_KEYFRAMING_H

struct ListBase;
struct ID;

struct IpoCurve;
struct BezTriple;

/* ************ Keyframing Management **************** */

/* Lesser Keyframing API call:
 *	Use this when validation of necessary animation data isn't necessary as it already
 * 	exists, and there is a beztriple that can be directly copied into the array.
 */
int insert_bezt_icu(struct IpoCurve *icu, struct BezTriple *bezt);

/* Main Keyframing API call: 
 *	Use this when validation of necessary animation data isn't necessary as it
 *	already exists. It will insert a keyframe using the current value being keyframed.
 */
void insert_vert_icu(struct IpoCurve *icu, float x, float y, short flag);


/* flags for use by keyframe creation/deletion calls */
enum {
		/* used by isnertkey() and insert_vert_icu() */
	INSERTKEY_NEEDED 	= (1<<0),	/* only insert keyframes where they're needed */
	INSERTKEY_MATRIX 	= (1<<1),	/* insert 'visual' keyframes where possible/needed */
	INSERTKEY_FAST 		= (1<<2),	/* don't recalculate handles,etc. after adding key */
	INSERTKEY_FASTR		= (1<<3),	/* don't realloc mem (or increase count, as array has already been set out) */
	INSERTKEY_REPLACE 	= (1<<4),	/* only replace an existing keyframe (this overrides INSERTKEY_NEEDED) */
	
		/* used by common_*key() functions */
	COMMONKEY_ADDMAP	= (1<<10),	/* common key: add texture-slot offset bitflag to adrcode before use */
} eInsertKeyFlags;

/* -------- */

/* Main Keyframing API calls: 
 *	Use this to create any necessary animation data, and then insert a keyframe
 *	using the current value being keyframed, in the relevant place. Returns success.
 */
	// TODO: adapt this for new data-api -> this blocktype, etc. stuff is evil!
short insertkey(struct ID *id, int blocktype, char *actname, char *constname, int adrcode, short flag);

/* Main Keyframing API call: 
 * 	Use this to delete keyframe on current frame for relevant channel. Will perform checks just in case.
 */
short deletekey(struct ID *id, int blocktype, char *actname, char *constname, int adrcode, short flag);


/* Main Keyframe Management calls: 
 *	These handle keyframes management from various spaces. They will handle the menus 
 * 	required for each space.
 */
void common_insertkey(void);
void common_deletekey(void);

/* ************ Auto-Keyframing ********************** */
/* Notes:
 * - All the defines for this (User-Pref settings and Per-Scene settings)
 * 	are defined in DNA_userdef_types.h
 * - Scene settings take presidence over those for userprefs, with old files
 * 	inheriting userpref settings for the scene settings
 * - "On/Off + Mode" are stored per Scene, but "settings" are currently stored
 * 	as userprefs
 */

/* Auto-Keying macros for use by various tools */
	/* check if auto-keyframing is enabled (per scene takes presidence) */
#define IS_AUTOKEY_ON			((G.scene) ? (G.scene->autokey_mode & AUTOKEY_ON) : (U.autokey_mode & AUTOKEY_ON))
	/* check the mode for auto-keyframing (per scene takes presidence)  */
#define IS_AUTOKEY_MODE(mode) 	((G.scene) ? (G.scene->autokey_mode == AUTOKEY_MODE_##mode) : (U.autokey_mode == AUTOKEY_MODE_##mode))
	/* check if a flag is set for auto-keyframing (as userprefs only!) */
#define IS_AUTOKEY_FLAG(flag)	(U.autokey_flag & AUTOKEY_FLAG_##flag)

/* ************ Keyframe Checking ******************** */

/* Main Keyframe Checking API call:
 * Checks whether a keyframe exists for the given ID-block one the given frame.
 *  - It is recommended to call this method over the other keyframe-checkers directly,
 * 	  in case some detail of the implementation changes...
 *	- frame: the value of this is quite often result of frame_to_float(CFRA)
 */
short id_frame_has_keyframe(struct ID *id, float frame, short filter);

/* filter flags for id_cfra_has_keyframe 
 *
 * WARNING: do not alter order of these, as also stored in files
 *	(for v3d->keyflags)
 */
enum {
		/* general */
	ANIMFILTER_LOCAL	= (1<<0),		/* only include locally available anim data */
	ANIMFILTER_MUTED	= (1<<1),		/* include muted elements */
	ANIMFILTER_ACTIVE	= (1<<2),		/* only include active-subelements */
	
		/* object specific */
	ANIMFILTER_NOMAT		= (1<<9),		/* don't include material keyframes */
	ANIMFILTER_NOSKEY		= (1<<10),		/* don't include shape keys (for geometry) */
} eAnimFilterFlags;

#endif /*  BIF_KEYFRAMING_H */
