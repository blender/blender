/* Testing code for 2.5 animation system 
 * Copyright 2009, Joshua Leung
 */
 
#ifndef ANIM_INTERN_H
#define ANIM_INTERN_H


/* KeyingSets/Keyframing Interface ------------- */

/* list of builtin KeyingSets (defined in keyingsets.c) */
extern ListBase builtin_keyingsets;

short keyingset_context_ok_poll(bContext *C, KeyingSet *ks);

short modifykey_get_context_data (bContext *C, ListBase *dsources, KeyingSet *ks);

#endif // ANIM_INTERN_H
