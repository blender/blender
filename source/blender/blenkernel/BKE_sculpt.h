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

struct NumInput;
struct RadialControl;
struct Scene;
struct Sculpt;
struct SculptSession;
struct StrokeCache;

typedef struct SculptSession {
	struct ProjVert *projverts;

	/* Mesh connectivity */
	struct ListBase *fmap;
	struct IndexNode *fmap_mem;
	int fmap_size;

	/* Used temporarily per-stroke */
	float *vertexcosnos;
	ListBase damaged_rects;
	ListBase damaged_verts;
	
	/* Used to cache the render of the active texture */
	unsigned int texcache_w, texcache_h, *texcache;

	void *cursor; /* wm handle */

	struct RadialControl *radialcontrol;
	
	struct SculptStroke *stroke;

	struct StrokeCache *cache;
} SculptSession;

void sculptsession_free(struct Sculpt *sculpt);

#endif
