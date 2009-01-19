/* Testing code for new animation system in 2.5 
 * Copyright 2009, Joshua Leung
 */

#ifndef BKE_ANIM_SYS_H
#define BKE_ANIM_SYS_H

struct ID;
struct ListBase;
struct Main;
struct AnimData;

/* ************************************* */
/* AnimData API */

/* Get AnimData from the given ID-block. */
struct AnimData *BKE_animdata_from_id(struct ID *id);

/* Add AnimData to the given ID-block */
struct AnimData *BKE_id_add_animdata(struct ID *id);

/* Free AnimData */
void BKE_free_animdata(struct ID *id);

/* Copy AnimData */
struct AnimData *BKE_copy_animdata(struct AnimData *adt);

/* ************************************* */
// TODO: overrides, remapping, and path-finding api's

/* ************************************* */
/* Evaluation API */

/* Evaluation loop for evaluating animation data  */
void BKE_animsys_evaluate_animdata(struct ID *id, struct AnimData *adt, float ctime, short recalc);

/* Evaluation of all ID-blocks with Animation Data blocks - Animation Data Only */
void BKE_animsys_evaluate_all_animation(struct Main *main, float ctime);


/* ************************************* */

#endif /* BKE_ANIM_SYS_H*/
