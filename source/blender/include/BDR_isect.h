/**
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
 * Some geometrical operations (intersection and such).
 */

#ifndef BDR_ISECT_H
#define BDR_ISECT_H


struct EditVert;
struct EditVlak;

/** 
 * Intersect a face and a linesegment 
 *
 * @param v1 Vertex 1 of the face
 * @param v2 Vertex 2 of the face
 * @param v3 Vertex 3 of the face
 * @param v4 Point 1 on the line
 * @param v5 Point 2 on the line
 * @param vec Location of the intersection (if it exists)
 * 
 * @retval -1 colliniar
 * @retval  0 no intersection
 * @retval  1 exact intersection of edge and line
 * @retval  2 cross-intersection
 */
short IsectFL(float *v1, float *v2, float *v3,                    
			  float *v4, float *v5, float *vec);

/** 
 * Intersect two lines
 *
 * @param v1 Point 1 of line 1
 * @param v2 Point 2 of line 1
 * @param v3 Point 1 of line 2
 * @param v4 Point 2 of line 2
 * @param cox projection (?)
 * @param coy projection (?)
 * @param labda answer (?)
 * @param mu answer (?)
 * @param vec answer (?)
 * 
 * @retval -1 colliniar
 * @retval  0 no intersection of segments
 * @retval  1 exact intersection of segments
 * @retval  2 cross-intersection of segments
 */
short IsectLL(float *v1, float *v2, float *v3, float *v4,
			  short cox, short coy,
			  float *labda, float *mu, float *vec);

int count_comparevlak(struct EditVlak *vl1, struct EditVlak *vl2);
void empty(void);
void addisedge(float *vec, short* edflag,                          
			   struct EditVlak *vl1, struct EditVlak *vl2,
			   short tel);
void oldedsort_andmake(struct EditVert **olded, int edcount, int proj);
short maxco(float *v1, float *v2);
void newfillvert(struct EditVert *v1);
void addisfaces(struct EditVlak *evl);
void intersect_mesh(void);        

#endif /*  BDR_ISECT_H */
