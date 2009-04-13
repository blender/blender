/* Testing code for 2.5 animation system 
 * Copyright 2009, Joshua Leung
 */
 
#ifndef ANIM_INTERN_H
#define ANIM_INTERN_H

/* ----------- Common Keyframe Destination Sources ------------ */
/* (used as part of KeyingSets/Keyframing interface as separation from context) */

/* temporary struct to gather data combos to keyframe */
typedef struct bCommonKeySrc {
	struct bCommonKeySrc *next, *prev;
		
		/* general data/destination-source settings */
	ID *id;					/* id-block this comes from */
	
		/* specific cases */
	bPoseChannel *pchan;	
	bConstraint *con;
} bCommonKeySrc;

/* KeyingSets/Keyframing Interface ------------- */

/* list of builtin KeyingSets (defined in keyingsets.c) */
extern ListBase builtin_keyingsets;

/* mode for modify_keyframes */
enum {
	MODIFYKEY_MODE_INSERT = 0,
	MODIFYKEY_MODE_DELETE,
} eModifyKey_Modes;

short keyingset_context_ok_poll(bContext *C, KeyingSet *ks);

short modifykey_get_context_data (bContext *C, ListBase *dsources, KeyingSet *ks);

int modify_keyframes(bContext *C, ListBase *dsources, KeyingSet *ks, short mode, float cfra);

#endif // ANIM_INTERN_H
