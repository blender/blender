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

#ifndef ED_KEYFRAMING_H
#define ED_KEYFRAMING_H

struct ListBase;
struct ID;
struct Scene;

struct KeyingSet;

struct bAction;
struct FCurve;
struct BezTriple;

struct bPoseChannel;
struct bConstraint;

struct bContext;
struct wmOperatorType;

struct PointerRNA;
struct PropertyRNA;

/* ************ Keyframing Management **************** */

/* Get (or add relevant data to be able to do so) the Active Action for the given 
 * Animation Data block, given an ID block where the Animation Data should reside.
 */
struct bAction *verify_adt_action(struct ID *id, short add);

/* Get (or add relevant data to be able to do so) F-Curve from the given Action. 
 * This assumes that all the destinations are valid.
 */
struct FCurve *verify_fcurve(struct bAction *act, const char group[], const char rna_path[], const int array_index, short add);

/* -------- */

/* Lesser Keyframing API call:
 *	Use this when validation of necessary animation data isn't necessary as it already
 * 	exists, and there is a beztriple that can be directly copied into the array.
 */
int insert_bezt_fcurve(struct FCurve *fcu, struct BezTriple *bezt, short flag);

/* Main Keyframing API call: 
 *	Use this when validation of necessary animation data isn't necessary as it
 *	already exists. It will insert a keyframe using the current value being keyframed.
 * 	Returns the index at which a keyframe was added (or -1 if failed)
 */
int insert_vert_fcurve(struct FCurve *fcu, float x, float y, short flag);

/* -------- */

/* Secondary Keyframing API calls: 
 *	Use this to insert a keyframe using the current value being keyframed, in the 
 *	nominated F-Curve (no creation of animation data performed). Returns success.
 */
short insert_keyframe_direct(struct PointerRNA ptr, struct PropertyRNA *prop, struct FCurve *fcu, float cfra, short flag);



/* -------- */

/* Main Keyframing API calls: 
 *	Use this to create any necessary animation data, and then insert a keyframe
 *	using the current value being keyframed, in the relevant place. Returns success.
 */
short insert_keyframe(struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag);

/* Main Keyframing API call: 
 * 	Use this to delete keyframe on current frame for relevant channel. Will perform checks just in case.
 */
short delete_keyframe(struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag);

/* -------- */

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
void ANIM_OT_delete_keyframe_v3d(struct wmOperatorType *ot);

/* Keyframe managment operators for UI buttons. */
void ANIM_OT_insert_keyframe_button(struct wmOperatorType *ot);
void ANIM_OT_delete_keyframe_button(struct wmOperatorType *ot);

/* ************ Keying Sets ********************** */

/* temporary struct to gather data combos to keyframe
 * (is used by modify_keyframes for 'relative' KeyingSets, provided via the dsources arg)
 */
typedef struct bCommonKeySrc {
	struct bCommonKeySrc *next, *prev;
		
		/* general data/destination-source settings */
	struct ID *id;					/* id-block this comes from */
	
		/* specific cases */
	struct bPoseChannel *pchan;	
	struct bConstraint *con;
} bCommonKeySrc;

/* -------- */

/* mode for modify_keyframes */
enum {
	MODIFYKEY_MODE_INSERT = 0,
	MODIFYKEY_MODE_DELETE,
} eModifyKey_Modes;

/* Keyframing Helper Call - use the provided Keying Set to Add/Remove Keyframes */
int modify_keyframes(struct bContext *C, struct ListBase *dsources, struct bAction *act, struct KeyingSet *ks, short mode, float cfra);

/* -------- */

/* Generate menu of KeyingSets */
char *ANIM_build_keyingsets_menu(struct ListBase *list, short for_edit);

/* Get the first builtin KeyingSet with the given name, which occurs after the given one (or start of list if none given) */
struct KeyingSet *ANIM_builtin_keyingset_get_named(struct KeyingSet *prevKS, char name[]);

/* Initialise builtin KeyingSets on startup */
void init_builtin_keyingsets(void);

/* -------- */

/* KeyingSet managment operators for UI buttons. */
void ANIM_OT_add_keyingset_button(struct wmOperatorType *ot);
void ANIM_OT_remove_keyingset_button(struct wmOperatorType *ot);

/* ************ Drivers ********************** */

/* Returns whether there is a driver in the copy/paste buffer to paste */
short ANIM_driver_can_paste(void);

/* Main Driver Management API calls:
 * 	Add a new driver for the specified property on the given ID block
 */
short ANIM_add_driver (struct ID *id, const char rna_path[], int array_index, short flag, int type);

/* Main Driver Management API calls:
 * 	Remove the driver for the specified property on the given ID block (if available)
 */
short ANIM_remove_driver(struct ID *id, const char rna_path[], int array_index, short flag);

/* Main Driver Management API calls:
 * 	Make a copy of the driver for the specified property on the given ID block
 */
short ANIM_copy_driver(struct ID *id, const char rna_path[], int array_index, short flag);

/* Main Driver Management API calls:
 * 	Add a new driver for the specified property on the given ID block or replace an existing one
 *	with the driver + driver-curve data from the buffer 
 */
short ANIM_paste_driver(struct ID *id, const char rna_path[], int array_index, short flag);

/* Driver management operators for UI buttons */
void ANIM_OT_add_driver_button(struct wmOperatorType *ot);
void ANIM_OT_remove_driver_button(struct wmOperatorType *ot);
void ANIM_OT_copy_driver_button(struct wmOperatorType *ot);
void ANIM_OT_paste_driver_button(struct wmOperatorType *ot);

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
#define IS_AUTOKEY_ON(scene)	((scene) ? (scene->toolsettings->autokey_mode & AUTOKEY_ON) : (U.autokey_mode & AUTOKEY_ON))
	/* check the mode for auto-keyframing (per scene takes presidence)  */
#define IS_AUTOKEY_MODE(scene, mode) 	((scene) ? (scene->toolsettings->autokey_mode == AUTOKEY_MODE_##mode) : (U.autokey_mode == AUTOKEY_MODE_##mode))
	/* check if a flag is set for auto-keyframing (as userprefs only!) */
#define IS_AUTOKEY_FLAG(flag)	(U.autokey_flag & AUTOKEY_FLAG_##flag)

/* auto-keyframing feature - checks for whether anything should be done for the current frame */
int autokeyframe_cfra_can_key(struct Scene *scene, struct ID *id);

/* ************ Keyframe Checking ******************** */

/* Lesser Keyframe Checking API call:
 *	- Used for the buttons to check for keyframes...
 */
short fcurve_frame_has_keyframe(struct FCurve *fcu, float frame, short filter);

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
