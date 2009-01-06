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
struct MVert;
struct NumInput;
struct RadialControl;
struct Scene;
struct SculptData;
struct SculptSession;

typedef struct SculptSession {
	struct ProjVert *projverts;

	struct bglMats *mats;

	int multires;
	int totvert;
	int totface;
	struct MVert *mvert;
	struct MFace *mface;
	float *face_normals;
	
	/* An array of lists; array is sized as
	   large as the number of verts in the mesh,
	   the list for each vert contains the index
	   for all the faces that use that vertex */
	struct ListBase *vertex_users;
	struct IndexNode *vertex_users_mem;
	int vertex_users_size;

	/* Used temporarily per-stroke */
	float *vertexcosnos;
	ListBase damaged_rects;
	ListBase damaged_verts;
	
	/* Used to cache the render of the active texture */
	unsigned int texcache_w, texcache_h, *texcache;
	
	struct RadialControl *radialcontrol;
	
	struct SculptStroke *stroke;
} SculptSession;

void sculptdata_init(struct Scene *sce);
void sculptdata_free(struct Scene *sce);
void sculptsession_free(struct Scene *sce);
void sculpt_vertexusers_free(struct SculptSession *ss);
void sculpt_reset_curve(struct SculptData *sd);

#endif
