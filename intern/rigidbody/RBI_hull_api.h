/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_rigidbody
 */

#ifndef __RB_HULL_API_H__
#define __RB_HULL_API_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plConvexHull__ {
  int unused;
} * plConvexHull;

plConvexHull plConvexHullCompute(float (*coords)[3], int count);
void plConvexHullDelete(plConvexHull hull);
int plConvexHullNumVertices(plConvexHull hull);
int plConvexHullNumLoops(plConvexHull hull);
int plConvexHullNumFaces(plConvexHull hull);
void plConvexHullGetVertex(plConvexHull hull, int n, float coords[3], int *original_index);
void plConvexHullGetLoop(plConvexHull hull, int n, int *v_from, int *v_to);
int plConvexHullGetReversedLoopIndex(plConvexHull hull, int n);
int plConvexHullGetFaceSize(plConvexHull hull, int n);
void plConvexHullGetFaceLoops(plConvexHull hull, int n, int *loops);
void plConvexHullGetFaceVertices(plConvexHull hull, int n, int *vertices);

#ifdef __cplusplus
}
#endif

#endif /* __RB_HULL_API_H__ */
