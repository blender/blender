/**
 * $Id: 
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

/* Internal for editmesh_xxxx.c functions */

#ifndef EDITMESH_H
#define EDITMESH_H

#define TEST_EDITMESH	if(G.obedit==0) return; \
						if( (G.vd->lay & G.obedit->lay)==0 ) return;

#define UVCOPY(t, s) memcpy(t, s, 2 * sizeof(float));



/* ******************* editmesh.c */
extern void free_editvert(EditVert *eve);
extern void free_editedge(EditEdge *eed);
extern void free_editface(EditFace *efa);

extern void free_vertlist(ListBase *edve);
extern void free_edgelist(ListBase *lb);
extern void free_facelist(ListBase *lb);

extern void remedge(EditEdge *eed);

extern struct EditVert *addvertlist(float *vec);
extern struct EditEdge *addedgelist(struct EditVert *v1, struct EditVert *v2, struct EditEdge *example);
extern struct EditFace *addfacelist(struct EditVert *v1, struct EditVert *v2, struct EditVert *v3, struct EditVert *v4, struct EditFace *example, struct EditFace *exampleEdges);
extern struct EditEdge *findedgelist(struct EditVert *v1, struct EditVert *v2);

/* ******************* editmesh_add.c */


/* ******************* editmesh_lib.c */
extern void EM_fgon_flags(void);
extern void EM_hide_reset(void);

extern int faceselectedOR(EditFace *efa, int flag);
extern int faceselectedAND(EditFace *efa, int flag);

extern EditFace *exist_face(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4);
extern void flipface(EditFace *efa); // flips for normal direction
extern int compareface(EditFace *vl1, EditFace *vl2);

extern short extrudeflag_face_indiv(short flag);
extern short extrudeflag_verts_indiv(short flag);
extern short extrudeflag_edges_indiv(short flag);
extern short extrudeflag_vert(short flag);
extern short extrudeflag(short flag);

extern void adduplicateflag(int flag);
extern void delfaceflag(int flag);

extern void rotateflag(short flag, float *cent, float rotmat[][3]);
extern void translateflag(short flag, float *vec);

extern int convex(float *v1, float *v2, float *v3, float *v4);

/* ******************* editmesh_mods.c */
extern EditEdge *findnearestedge(short *dist);

/* ******************* editmesh_tools.c */


#endif

