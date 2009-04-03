/**
 * $Id: BIF_keyframing.h 17216 2008-10-29 11:20:02Z aligorith $
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

#ifndef ED_KEYFRAMING_H
#define ED_KEYFRAMING_H

struct ListBase;
struct ID;

struct KeyingSet;

struct FCurve;
struct BezTriple;

struct wmOperatorType;

/* ************ Keyframing Management **************** */

/* Lesser Keyframing API call:
 *	Use this when validation of necessary animation data isn't necessary as it already
 * 	exists, and there is a beztriple that can be directly copied into the array.
 */
int insert_bezt_fcurve(struct FCurve *fcu, struct BezTriple *bezt);

/* Main Keyframing API call: 
 *	Use this when validation of necessary animation data isn't necessary as it
 *	already exists. It will insert a keyframe using the current value being keyframed.
 */
void insert_vert_fcurve(struct FCurve *fcu, float x, float y, short flag);

/* -------- */

/* Main Keyframing API calls: 
 *	Use this to create any necessary animation data, and then insert a keyframe
 *	using the current value being keyframed, in the relevant place. Returns success.
 */
short insertkey(struct ID *id, const char group[], const char rna_path[], int array_index, float cfra, short flag);

/* Main Keyframing API call: 
 * 	Use this to delete keyframe on current frame for relevant channel. Will perform checks just in case.
 */
short deletekey(struct ID *id, const char group[], const char rna_path[], int array_index, float cfra, short flag);


/* Generate menu of KeyingSets */
char *ANIM_build_keyingsets_menu(struct ListBase *list, short for_edit);

/* Initialise builtin KeyingSets on startup */
void init_builtin_keyingsets(void);

/* KeyingSet Editing Operators:
 *	These can add a new KeyingSet and/or add 'destinations' to the KeyingSets,
 *	acting as a means by which they can be added outside the Outliner.
 */
void ANIM_OT_keyingset_add_new(struct wmOperatorType *ot);
void ANIM_OT_keyingset_add_destination(struct wmOperatorType *ot);

/* Main Keyframe Management operators: 
 *	These handle keyframes management from various spaces. They only make use of
 * 	Keying Sets.
 */
void ANIM_OT_insert_keyframe(struct wmOperatorType *ot);
void ANIM_OT_delete_keyframe(struct wmOperatorType *ot);

/* Main Keyframe Management operators: 
 *	These handle keyframes management from various spaces. They will handle the menus 
 * 	required for each space.
 */
void ANIM_OT_insert_keyframe_menu(struct wmOperatorType *ot);
void ANIM_OT_delete_keyframe_menu(struct wmOperatorType *ot); // xxx unimplemented yet
void ANIM_OT_delete_keyframe_old(struct wmOperatorType *ot); // xxx rename and keep?

/* Keyframe managment operators for UI buttons. */

void ANIM_OT_insert_keyframe_button(struct wmOperatorType *ot);
void ANIM_OT_delete_keyframe_button(struct wmOperatorType *ot);

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
#define IS_AUTOKEY_ON(scene)	((scene) ? (scene->autokey_mode & AUTOKEY_ON) : (U.autokey_mode & AUTOKEY_ON))
	/* check the mode for auto-keyframing (per scene takes presidence)  */
#define IS_AUTOKEY_MODE(scene, mode) 	((scene) ? (scene->autokey_mode == AUTOKEY_MODE_##mode) : (U.autokey_mode == AUTOKEY_MODE_##mode))
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
	ANIMFILTER_KEYS_LOCAL	= (1<<0),		/* only include locally available anim data */
	ANIMFILTER_KEYS_MUTED	= (1<<1),		/* include muted elements */
	ANIMFILTER_KEYS_ACTIVE	= (1<<2),		/* only include active-subelements */
	
		/* object specific */
	ANIMFILTER_KEYS_NOMAT		= (1<<9),		/* don't include material keyframes */
	ANIMFILTER_KEYS_NOSKEY		= (1<<10),		/* don't include shape keys (for geometry) */
} eAnimFilterFlags;

#endif /*  ED_KEYFRAMING_H */
