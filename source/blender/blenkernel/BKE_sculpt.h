/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_SCULPT_H
#define BKE_SCULPT_H

struct MFace;
struct MultireModifierData;
struct MVert;
struct Sculpt;
struct StrokeCache;

typedef struct SculptSession {
	struct ProjVert *projverts;

	/* Mesh data (not copied) can come either directly from a Mesh, or from a MultiresDM */
	struct MultiresModifierData *multires; /* Special handling for multires meshes */
	struct MVert *mvert;
	struct MFace *mface;
	int totvert, totface;
	float *face_normals;
	
	/* Mesh connectivity */
	struct ListBase *fmap;
	struct IndexNode *fmap_mem;
	int fmap_size;

	/* Used temporarily per-stroke */
	float *vertexcosnos;
	ListBase damaged_rects;
	ListBase damaged_verts;
	
	/* Used to cache the render of the active texture */
	unsigned int texcache_side, *texcache, texcache_actual;

	/* Layer brush persistence between strokes */
 	float (*mesh_co_orig)[3]; /* Copy of the mesh vertices' locations */
	float *layer_disps; /* Displacements for each vertex */

	void *cursor; /* wm handle */

	struct SculptStroke *stroke;

	struct StrokeCache *cache;
} SculptSession;

void sculptsession_free(struct Sculpt *sculpt);

#endif
