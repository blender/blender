/**
 * blenlib/BKE_anim.h (mar-2001 nzc);
 *	
 * $Id$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_ANIM_H
#define BKE_ANIM_H

struct Path;
struct Object;
struct PartEff;
struct Scene;

void free_path(struct Path *path);
void calc_curvepath(struct Object *ob);
int interval_test(int min, int max, int p1, int cycl);
int where_on_path(struct Object *ob, float ctime, float *vec, float *dir);
void frames_duplilist(struct Object *ob);
void vertex_duplilist(struct Scene *sce, struct Object *par);
void particle_duplilist(struct Scene *sce, struct Object *par, struct PartEff *paf);
void free_duplilist(void);
void make_duplilist(struct Scene *sce, struct Object *ob);
	
#endif
