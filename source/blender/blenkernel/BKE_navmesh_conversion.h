/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_NAVMESH_CONVERSION_H__
#define __BKE_NAVMESH_CONVERSION_H__

/** \file BKE_navmesh_conversion.h
 *  \ingroup bke
 */

struct DerivedMesh;

/* navmesh_conversion.c */
int buildNavMeshDataByDerivedMesh(struct DerivedMesh *dm, int *vertsPerPoly,
                                  int *nverts, float **verts,
                                  int *ndtris, unsigned short **dtris,
                                  int *npolys, unsigned short **dmeshes,
                                  unsigned short **polys, int **dtrisToPolysMap,
                                  int **dtrisToTrisMap, int **trisToFacesMap);

int buildRawVertIndicesData(struct DerivedMesh *dm, int *nverts, float **verts,
                            int *ntris, unsigned short **tris, int **trisToFacesMap,
                            int **recastData);

int buildNavMeshData(const int nverts, const float *verts,
                     const int ntris, const unsigned short *tris,
                     const int *recastData, const int *trisToFacesMap,
                     int *ndtris, unsigned short **dtris,
                     int *npolys, unsigned short **dmeshes, unsigned short **polys,
                     int *vertsPerPoly, int **dtrisToPolysMap, int **dtrisToTrisMap);

int buildPolygonsByDetailedMeshes(const int vertsPerPoly, const int npolys,
                                  unsigned short *polys, const unsigned short *dmeshes,
                                  const float *verts, const unsigned short *dtris,
                                  const int *dtrisToPolysMap);

int polyNumVerts(const unsigned short *p, const int vertsPerPoly);
int polyIsConvex(const unsigned short *p, const int vertsPerPoly, const float *verts);
int polyFindVertex(const unsigned short *p, const int vertsPerPoly, unsigned short vertexIdx);
float distPointToSegmentSq(const float *point, const float *a, const float *b);


#endif  /* NAVMESH_CONVERSION_H */
