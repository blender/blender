/* IMB_anim.h */
#ifndef IMB_ANIM5_H
#define IMB_ANIM5_H

struct anim;

/**
 *
 * @attention Defined in anim5.c
 */
int nextanim5(struct anim * anim);
int rewindanim5(struct anim * anim);
int startanim5(struct anim * anim);
void free_anim_anim5(struct anim * anim);
struct ImBuf * anim5_fetchibuf(struct anim * anim);


#endif


