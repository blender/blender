

#ifndef __SM_DEBUG_H__
#define __SM_DEBUG_H__

/* Comment this to disable all SUMO debugging printfs */

#define SM_DEBUG

#ifdef SM_DEBUG

#include <stdio.h>

/* Uncomment this to printf all ray casts */
//#define SM_DEBUG_RAYCAST

/* Uncomment this to printf collision callbacks */
//#define SM_DEBUG_BOING

/* Uncomment this to printf Xform matrix calculations */
//#define SM_DEBUG_XFORM

#endif /* SM_DEBUG */

#endif /* __SM_DEBUG_H__ */

