/**
 * BKE_softbody.h 
 *	
 * $Id: BKE_softbody.h 
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_SOFTBODY_H
#define BKE_SOFTBODY_H

typedef struct BodyPoint {
	float orig[3], pos[3], vec[3], force[3];
	float weight, goal;
} BodyPoint;

typedef struct BodySpring {
	int v1, v2;
	float len, strength;
} BodySpring;

typedef struct SoftBody {
	int totpoint, totspring;
	
	BodyPoint *bpoint;
	BodySpring *bspring;
	
	float ctime;	// last time calculated
} SoftBody;

/* temporal data, nothing saved in file */
extern void free_softbody(SoftBody *sb);

/* makes totally fresh start situation */
extern void object_to_softbody(Object *ob);

/* copy original (but new) situation in softbody, as result of matrices or deform */
void object_update_softbody(Object *ob);

/* copies softbody result back to object (in displist) */
extern void softbody_to_object(Object *ob);

/* go one step in simulation */
extern void object_softbody_step(Object *ob, float ctime);

#endif

