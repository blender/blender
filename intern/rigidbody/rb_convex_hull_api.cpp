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

#include "LinearMath/btConvexHullComputer.h"
#include "RBI_hull_api.h"

plConvexHull plConvexHullCompute(float (*coords)[3], int count)
{
  btConvexHullComputer *computer = new btConvexHullComputer;
  computer->compute(reinterpret_cast<float *>(coords), sizeof(*coords), count, 0, 0);
  return reinterpret_cast<plConvexHull>(computer);
}

void plConvexHullDelete(plConvexHull hull)
{
  btConvexHullComputer *computer(reinterpret_cast<btConvexHullComputer *>(hull));
  delete computer;
}

int plConvexHullNumVertices(plConvexHull hull)
{
  btConvexHullComputer *computer(reinterpret_cast<btConvexHullComputer *>(hull));
  return computer->vertices.size();
}

int plConvexHullNumFaces(plConvexHull hull)
{
  btConvexHullComputer *computer(reinterpret_cast<btConvexHullComputer *>(hull));
  return computer->faces.size();
}

void plConvexHullGetVertex(plConvexHull hull, int n, float coords[3], int *original_index)
{
  btConvexHullComputer *computer(reinterpret_cast<btConvexHullComputer *>(hull));
  const btVector3 &v(computer->vertices[n]);
  coords[0] = v[0];
  coords[1] = v[1];
  coords[2] = v[2];
  (*original_index) = computer->original_vertex_index[n];
}

int plConvexHullGetFaceSize(plConvexHull hull, int n)
{
  btConvexHullComputer *computer(reinterpret_cast<btConvexHullComputer *>(hull));
  const btConvexHullComputer::Edge *e_orig, *e;
  int count;

  for (e_orig = &computer->edges[computer->faces[n]], e = e_orig, count = 0;
       count == 0 || e != e_orig;
       e = e->getNextEdgeOfFace(), count++) {
    ;
  }
  return count;
}

void plConvexHullGetFaceVertices(plConvexHull hull, int n, int *vertices)
{
  btConvexHullComputer *computer(reinterpret_cast<btConvexHullComputer *>(hull));
  const btConvexHullComputer::Edge *e_orig, *e;
  int count;

  for (e_orig = &computer->edges[computer->faces[n]], e = e_orig, count = 0;
       count == 0 || e != e_orig;
       e = e->getNextEdgeOfFace(), count++) {
    vertices[count] = e->getTargetVertex();
  }
}
