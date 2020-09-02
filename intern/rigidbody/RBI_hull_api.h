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
 * The Original Code is Copyright (C) 2020 Blender Foundation,
 * All rights reserved.
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
int plConvexHullNumFaces(plConvexHull hull);
void plConvexHullGetVertex(plConvexHull hull, int n, float coords[3], int *original_index);
int plConvexHullGetFaceSize(plConvexHull hull, int n);
void plConvexHullGetFaceVertices(plConvexHull hull, int n, int *vertices);

#ifdef __cplusplus
}
#endif

#endif /* __RB_HULL_API_H__ */
