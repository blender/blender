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
 * The Original Code is Copyright (C) 2004 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*

editmesh_lib: generic (no UI, no menus) operations/evaluators for editmesh data

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"


#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BIF_editmesh.h"

#include "editmesh.h"


/* ********* Selection ************ */

void EM_select_face(EditFace *efa, int sel)
{
	if(sel) {
		efa->f |= SELECT;
		efa->e1->f |= SELECT;
		efa->e2->f |= SELECT;
		efa->e3->f |= SELECT;
		if(efa->e4) efa->e4->f |= SELECT;
		efa->v1->f |= SELECT;
		efa->v2->f |= SELECT;
		efa->v3->f |= SELECT;
		if(efa->v4) efa->v4->f |= SELECT;
	}
	else {
		efa->f &= ~SELECT;
		efa->e1->f &= ~SELECT;
		efa->e2->f &= ~SELECT;
		efa->e3->f &= ~SELECT;
		if(efa->e4) efa->e4->f &= ~SELECT;
		efa->v1->f &= ~SELECT;
		efa->v2->f &= ~SELECT;
		efa->v3->f &= ~SELECT;
		if(efa->v4) efa->v4->f &= ~SELECT;
	}
}

void EM_select_edge(EditEdge *eed, int sel)
{
	if(sel) {
		eed->f |= SELECT;
		eed->v1->f |= SELECT;
		eed->v2->f |= SELECT;
	}
	else {
		eed->f &= ~SELECT;
		eed->v1->f &= ~SELECT;
		eed->v2->f &= ~SELECT;
	}
}

void EM_select_face_fgon(EditFace *efa, int val)
{
	EditMesh *em = G.editMesh;
	short index=0;
	
	if(efa->fgonf==0) EM_select_face(efa, val);
	else {
		if(efa->e1->fgoni) index= efa->e1->fgoni;
		if(efa->e2->fgoni) index= efa->e2->fgoni;
		if(efa->e3->fgoni) index= efa->e3->fgoni;
		if(efa->v4 && efa->e4->fgoni) index= efa->e4->fgoni;
		
		if(index==0) printf("wrong fgon select\n");
		
		// select all ngon faces with index
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->fgonf) {
				if(efa->e1->fgoni==index || efa->e2->fgoni==index || 
				   efa->e3->fgoni==index || (efa->e4 && efa->e4->fgoni==index) ) {
					EM_select_face(efa, val);
				}
			}
		}
	}
}


/* only vertices */
int faceselectedOR(EditFace *efa, int flag)
{
	if ((efa->v1->f | efa->v2->f | efa->v3->f | (efa->v4?efa->v4->f:0))&flag) {
		return 1;
	} else {
		return 0;
	}
}

// replace with (efa->f & SELECT)
int faceselectedAND(EditFace *efa, int flag)
{
	if ((efa->v1->f & efa->v2->f & efa->v3->f & (efa->v4?efa->v4->f:flag))&flag) {
		return 1;
	} else {
		return 0;
	}
}

int EM_nfaces_selected(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	int count= 0;

	for (efa= em->faces.first; efa; efa= efa->next)
		if (efa->f & SELECT)
			count++;

	return count;
}

#if 0
static int EM_nedges(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	int count= 0;

	for (eed= em->edges.first; eed; eed= eed->next) count++;
	return count;
}
#endif

int EM_nvertices_selected(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int count= 0;

	for (eve= em->verts.first; eve; eve= eve->next)
		if (eve->f & SELECT)
			count++;

	return count;
}

void EM_clear_flag_all(int flag)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	for (eve= em->verts.first; eve; eve= eve->next) eve->f &= ~flag;
	for (eed= em->edges.first; eed; eed= eed->next) eed->f &= ~flag;
	for (efa= em->faces.first; efa; efa= efa->next) efa->f &= ~flag;
	
}

void EM_set_flag_all(int flag)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	for (eve= em->verts.first; eve; eve= eve->next) if(eve->h==0) eve->f |= flag;
	for (eed= em->edges.first; eed; eed= eed->next) if(eed->h==0) eed->f |= flag;
	for (efa= em->faces.first; efa; efa= efa->next) if(efa->h==0) efa->f |= flag;
	
}

/* flush for changes in vertices only */
void EM_deselect_flush(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;
	
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->v1->f & eed->v2->f & SELECT);
		else eed->f &= ~SELECT;
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->v4) {
			if(efa->v1->f & efa->v2->f & efa->v3->f & efa->v4->f & SELECT );
			else efa->f &= ~SELECT;
		}
		else {
			if(efa->v1->f & efa->v2->f & efa->v3->f & SELECT );
			else efa->f &= ~SELECT;
		}
	}
}


/* flush selection to edges & faces */

/*  this only based on coherent selected vertices, for example when adding new
    objects. call clear_flag_all() before you select vertices to be sure it ends OK!
	
*/

void EM_select_flush(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;
	
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->v1->f & eed->v2->f & SELECT) eed->f |= SELECT;
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->v4) {
			if(efa->v1->f & efa->v2->f & efa->v3->f & efa->v4->f & SELECT ) efa->f |= SELECT;
		}
		else {
			if(efa->v1->f & efa->v2->f & efa->v3->f & SELECT ) efa->f |= SELECT;
		}
	}
}

/* when vertices or edges can be selected, also make fgon consistant */
static void check_fgons_selection()
{
	EditMesh *em = G.editMesh;
	EditFace *efa, *efan;
	EditEdge *eed;
	ListBase *lbar;
	int sel, desel, index, totfgon= 0;
	
	/* count amount of fgons */
	for(eed= em->edges.first; eed; eed= eed->next) 
		if(eed->fgoni>totfgon) totfgon= eed->fgoni;
	
	if(totfgon==0) return;
	
	lbar= MEM_callocN((totfgon+1)*sizeof(ListBase), "listbase array");
	
	/* put all fgons in lbar */
	for(efa= em->faces.first; efa; efa= efan) {
		efan= efa->next;
		index= efa->e1->fgoni;
		if(index==0) index= efa->e2->fgoni;
		if(index==0) index= efa->e3->fgoni;
		if(index==0 && efa->e4) index= efa->e4->fgoni;
		if(index) {
			BLI_remlink(&em->faces, efa);
			BLI_addtail(&lbar[index], efa);
		}
	}
	
	/* now check the fgons */
	for(index=1; index<=totfgon; index++) {
		/* we count on vertices/faces/edges being set OK, so we only have to set ngon itself */
		sel= desel= 0;
		for(efa= lbar[index].first; efa; efa= efa->next) {
			if(efa->e1->fgoni==0) {
				if(efa->e1->f & SELECT) sel++;
				else desel++;
			}
			if(efa->e2->fgoni==0) {
				if(efa->e2->f & SELECT) sel++;
				else desel++;
			}
			if(efa->e3->fgoni==0) {
				if(efa->e3->f & SELECT) sel++;
				else desel++;
			}
			if(efa->e4 && efa->e4->fgoni==0) {
				if(efa->e4->f & SELECT) sel++;
				else desel++;
			}
			
			if(sel && desel) break;
		}

		if(sel && desel) sel= 0;
		else if(sel) sel= 1;
		else sel= 0;
		
		/* select/deselect and put back */
		for(efa= lbar[index].first; efa; efa= efa->next) {
			if(sel) efa->f |= SELECT;
			else efa->f &= ~SELECT;
		}
		addlisttolist(&em->faces, &lbar[index]);
	}
	
	MEM_freeN(lbar);
}


/* flush to edges & faces */

/* based on select mode it selects edges/faces 
   assumed is that verts/edges/faces were properly selected themselves
   with the calls above
*/

void EM_selectmode_flush(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;
	
	// flush to edges & faces
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->v1->f & eed->v2->f & SELECT) eed->f |= SELECT;
			else eed->f &= ~SELECT;
		}
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->v4) {
				if(efa->v1->f & efa->v2->f & efa->v3->f & efa->v4->f & SELECT) efa->f |= SELECT;
				else efa->f &= ~SELECT;
			}
			else {
				if(efa->v1->f & efa->v2->f & efa->v3->f & SELECT) efa->f |= SELECT;
				else efa->f &= ~SELECT;
			}
		}
	}
	// flush to faces
	else if(G.scene->selectmode & SCE_SELECT_EDGE) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->e4) {
				if(efa->e1->f & efa->e2->f & efa->e3->f & efa->e4->f & SELECT) efa->f |= SELECT;
				else efa->f &= ~SELECT;
			}
			else {
				if(efa->e1->f & efa->e2->f & efa->e3->f & SELECT) efa->f |= SELECT;
				else efa->f &= ~SELECT;
			}
		}
	}	
	// make sure selected faces have selected edges too, for extrude (hack?)
	else if(G.scene->selectmode & SCE_SELECT_FACE) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) EM_select_face(efa, 1);
		}
	}
	check_fgons_selection();

}

/* when switching select mode, makes sure selection is consistant for editing */
/* also for paranoia checks to make sure edge or face mode works */
void EM_selectmode_set(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;

	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		/* vertices -> edges -> faces */
		for (eed= em->edges.first; eed; eed= eed->next) eed->f &= ~SELECT;
		for (efa= em->faces.first; efa; efa= efa->next) efa->f &= ~SELECT;
		
		EM_select_flush();
	}
	else if(G.scene->selectmode & SCE_SELECT_EDGE) {
		/* deselect vertices, and select again based on edge select */
		for(eve= em->verts.first; eve; eve= eve->next) eve->f &= ~SELECT;
		for(eed= em->edges.first; eed; eed= eed->next) 
			if(eed->f & SELECT) EM_select_edge(eed, 1);
		/* selects faces based on edge status */
		EM_selectmode_flush();
		
	}
	else if(G.scene->selectmode == SCE_SELECT_FACE) {
		/* deselect eges, and select again based on face select */
		for(eed= em->edges.first; eed; eed= eed->next) EM_select_edge(eed, 0);
		
		for(efa= em->faces.first; efa; efa= efa->next) 
			if(efa->f & SELECT) EM_select_face(efa, 1);
	}
}

/* paranoia check, actually only for entering editmode. rule:
- vertex hidden, always means edge is hidden too
- edge hidden, always means face is hidden too
- face hidden, dont change anything
*/
void EM_hide_reset(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;
	
	for(eed= em->edges.first; eed; eed= eed->next) 
		if(eed->v1->h || eed->v2->h) eed->h |= 1;
		
	for(efa= em->faces.first; efa; efa= efa->next) 
		if((efa->e1->h & 1) || (efa->e2->h & 1) || (efa->e3->h & 1) || (efa->e4 && (efa->e4->h & 1)))
			efa->h= 1;
		
}


/* ********  EXTRUDE ********* */

static void add_normal_aligned(float *nor, float *add)
{
	if( INPR(nor, add) < 0.0 ) 
		VecSubf(nor, nor, add);
	else
		VecAddf(nor, nor, add);
}

static void set_edge_directions_f2(int val)
{
	EditMesh *em= G.editMesh;
	EditFace *efa;
	int do_all= 1;
	
	/* edge directions are used for extrude, to detect direction of edges that make new faces */
	/* we have set 'f2' flags in edges that need to get a direction set (e.g. get new face) */
	/* the val argument differs... so we need it as arg */
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->f & SELECT) {
			do_all= 0;
			if(efa->e1->f2<val) {
				if(efa->e1->v1 == efa->v1) efa->e1->dir= 0;
				else efa->e1->dir= 1;
			}
			if(efa->e2->f2<val) {
				if(efa->e2->v1 == efa->v2) efa->e2->dir= 0;
				else efa->e2->dir= 1;
			}
			if(efa->e3->f2<val) {
				if(efa->e3->v1 == efa->v3) efa->e3->dir= 0;
				else efa->e3->dir= 1;
			}
			if(efa->e4 && efa->e4->f2<val) {
				if(efa->e4->v1 == efa->v4) efa->e4->dir= 0;
				else efa->e4->dir= 1;
			}
		}
	}	
	/* ok, no faces done... then we at least set it for exterior edges */
	if(do_all) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->e1->v1 == efa->v1) efa->e1->dir= 0;
			else efa->e1->dir= 1;
			if(efa->e2->v1 == efa->v2) efa->e2->dir= 0;
			else efa->e2->dir= 1;
			if(efa->e3->v1 == efa->v3) efa->e3->dir= 0;
			else efa->e3->dir= 1;
			if(efa->e4) {
				if(efa->e4->v1 == efa->v4) efa->e4->dir= 0;
				else efa->e4->dir= 1;
			}
		}	
	}
}

/* individual face extrude */
/* will use vertex normals for extrusion directions, so *nor is unaffected */
short extrudeflag_face_indiv(short flag, float *nor)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *v1, *v2, *v3, *v4;
	EditEdge *eed;
	EditFace *efa, *nextfa;
	
	if(G.obedit==0 || get_mesh(G.obedit)==0) return 0;
	
	/* selected edges with 1 or more selected face become faces */
	/* selected faces each makes new faces */
	/* always remove old faces, keeps volumes manifold */
	/* select the new extrusion, deselect old */
	
	/* step 1; init, count faces in edges */
	recalc_editnormals();
	
	for(eve= em->verts.first; eve; eve= eve->next) eve->f1= 0;	// new select flag

	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->f2= 0; // amount of unselected faces
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->f & SELECT);
		else {
			efa->e1->f2++;
			efa->e2->f2++;
			efa->e3->f2++;
			if(efa->e4) efa->e4->f2++;
		}
	}

	/* step 2: make new faces from faces */
	for(efa= em->faces.last; efa; efa= efa->prev) {
		if(efa->f & SELECT) {
			v1= addvertlist(efa->v1->co);
			v2= addvertlist(efa->v2->co);
			v3= addvertlist(efa->v3->co);
			v1->f1= v2->f1= v3->f1= 1;
			VECCOPY(v1->no, efa->n);
			VECCOPY(v2->no, efa->n);
			VECCOPY(v3->no, efa->n);
			if(efa->v4) {
				v4= addvertlist(efa->v4->co); 
				v4->f1= 1;
				VECCOPY(v4->no, efa->n);
			}
			else v4= NULL;
			
			/* side faces, clockwise */
			addfacelist(efa->v2, v2, v1, efa->v1, efa, NULL);
			addfacelist(efa->v3, v3, v2, efa->v2, efa, NULL);
			if(efa->v4) {
				addfacelist(efa->v4, v4, v3, efa->v3, efa, NULL);
				addfacelist(efa->v1, v1, v4, efa->v4, efa, NULL);
			}
			else {
				addfacelist(efa->v1, v1, v3, efa->v3, efa, NULL);
			}
			/* top face */
			addfacelist(v1, v2, v3, v4, efa, NULL);
		}
	}
	
	/* step 3: remove old faces */
	efa= em->faces.first;
	while(efa) {
		nextfa= efa->next;
		if(efa->f & SELECT) {
			BLI_remlink(&em->faces, efa);
			free_editface(efa);
		}
		efa= nextfa;
	}

	/* step 4: redo selection */
	EM_clear_flag_all(SELECT);
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f1)  eve->f |= SELECT;
	}
	
	EM_select_flush();
	
	return 'n';
}


/* extrudes individual edges */
/* nor is filled with constraint vector */
short extrudeflag_edges_indiv(short flag, float *nor) 
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	for(eve= em->verts.first; eve; eve= eve->next) eve->vn= NULL;
	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->vn= NULL;
		eed->f2= ((eed->f & flag)!=0);
	}
	
	set_edge_directions_f2(2);

	/* sample for next loop */
	for(efa= em->faces.first; efa; efa= efa->next) {
		efa->e1->vn= (EditVert *)efa;
		efa->e2->vn= (EditVert *)efa;
		efa->e3->vn= (EditVert *)efa;
		if(efa->e4) efa->e4->vn= (EditVert *)efa;
	}
	/* make the faces */
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f & flag) {
			if(eed->v1->vn==NULL) eed->v1->vn= addvertlist(eed->v1->co);
			if(eed->v2->vn==NULL) eed->v2->vn= addvertlist(eed->v2->co);
			
			if(eed->dir==1) addfacelist(eed->v1, eed->v2, eed->v2->vn, eed->v1->vn, (EditFace *)eed->vn, NULL);
			else addfacelist(eed->v2, eed->v1, eed->v1->vn, eed->v2->vn, (EditFace *)eed->vn, NULL);

			/* for transform */
			if(eed->vn) {
				efa= (EditFace *)eed->vn;
				if(efa->f & SELECT) add_normal_aligned(nor, efa->n);
			}
		}
	}
	Normalise(nor);
	
	/* set correct selection */
	EM_clear_flag_all(SELECT);
	for(eve= em->verts.last; eve; eve= eve->prev) {
		if(eve->vn) {
			eve->vn->f |= flag;
		}
	}

	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->v1->f & eed->v2->f & flag) eed->f |= flag;
	}
	
	if(nor[0]==0.0 && nor[1]==0.0 && nor[2]==0.0) return 'g'; // g is grab
	return 'n';  // n is for normal constraint
}

/* extrudes individual vertices */
short extrudeflag_verts_indiv(short flag, float *nor) 
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	
	/* make the edges */
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & flag) {
			eve->vn= addvertlist(eve->co);
			addedgelist(eve, eve->vn, NULL);
		}
		else eve->vn= NULL;
	}
	
	/* set correct selection */
	EM_clear_flag_all(SELECT);

	for(eve= em->verts.last; eve; eve= eve->prev) if(eve->vn) eve->vn->f |= flag;

	return 'g';	// g is grab
}


/* this is actually a recode of extrudeflag(), using proper edge/face select */
/* hurms, doesnt use 'flag' yet, but its not called by primitive making stuff anyway */
static short extrudeflag_edge(short flag, float *nor)
{
	/* all select edges/faces: extrude */
	/* old select is cleared, in new ones it is set */
	EditMesh *em = G.editMesh;
	EditVert *eve, *nextve;
	EditEdge *eed, *nexted;
	EditFace *efa, *nextfa;
	short del_old= 0;
	
	if(G.obedit==0 || get_mesh(G.obedit)==0) return 0;
	
	/* selected edges with 0 or 1 selected face become faces */
	/* selected faces generate new faces */

	/* if *one* selected face has edge with unselected face; remove old selected faces */
	
	/* if selected edge is not used anymore; remove */
	/* if selected vertex is not used anymore: remove */
	
	/* select the new extrusion, deselect old */
	
	
	/* step 1; init, count faces in edges */
	recalc_editnormals();
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		eve->vn= NULL;
		eve->f1= 0;
	}

	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->f1= 0; // amount of unselected faces
		eed->f2= 0; // amount of selected faces
		if(eed->f & SELECT) {
			eed->v1->f1= 1; // we call this 'selected vertex' now
			eed->v2->f1= 1;
		}
		eed->vn= NULL;		// here we tuck face pointer, as sample
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->f & SELECT) {
			efa->e1->f2++;
			efa->e2->f2++;
			efa->e3->f2++;
			if(efa->e4) efa->e4->f2++;
		}
		else {
			efa->e1->f1++;
			efa->e2->f1++;
			efa->e3->f1++;
			if(efa->e4) efa->e4->f1++;
		}
		// sample for next loop
		efa->e1->vn= (EditVert *)efa;
		efa->e2->vn= (EditVert *)efa;
		efa->e3->vn= (EditVert *)efa;
		if(efa->e4) efa->e4->vn= (EditVert *)efa;
	}
	
	set_edge_directions_f2(2);
	
	/* step 1.5: if *one* selected face has edge with unselected face; remove old selected faces */
	for(efa= em->faces.last; efa; efa= efa->prev) {
		if(efa->f & SELECT) {
			if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1 || (efa->e4 && efa->e4->f1)) {
				del_old= 1;
				break;
			}
		}
	}
				
	/* step 2: make new faces from edges */
	for(eed= em->edges.last; eed; eed= eed->prev) {
		if(eed->f & SELECT) {
			if(eed->f2<2) {
				if(eed->v1->vn==NULL)
					eed->v1->vn= addvertlist(eed->v1->co);
				if(eed->v2->vn==NULL)
					eed->v2->vn= addvertlist(eed->v2->co);
				
				/* if del_old, the preferred normal direction is exact opposite as for keep old faces */
				if(eed->dir!=del_old) addfacelist(eed->v1, eed->v2, eed->v2->vn, eed->v1->vn, (EditFace *)eed->vn, NULL);
				else addfacelist(eed->v2, eed->v1, eed->v1->vn, eed->v2->vn, (EditFace *)eed->vn, NULL);
			}
		}
	}
	
	/* step 3: make new faces from faces */
	for(efa= em->faces.last; efa; efa= efa->prev) {
		if(efa->f & SELECT) {
			if(efa->v1->vn==NULL) efa->v1->vn= addvertlist(efa->v1->co);
			if(efa->v2->vn==NULL) efa->v2->vn= addvertlist(efa->v2->co);
			if(efa->v3->vn==NULL) efa->v3->vn= addvertlist(efa->v3->co);
			if(efa->v4 && efa->v4->vn==NULL) efa->v4->vn= addvertlist(efa->v4->co);
			
			if(del_old==0) {	// keep old faces means flipping normal
				if(efa->v4)
					addfacelist(efa->v4->vn, efa->v3->vn, efa->v2->vn, efa->v1->vn, efa, efa);
				else
					addfacelist(efa->v3->vn, efa->v2->vn, efa->v1->vn, NULL, efa, efa);
			}
			else {
				if(efa->v4)
					addfacelist(efa->v1->vn, efa->v2->vn, efa->v3->vn, efa->v4->vn, efa, efa);
				else
					addfacelist(efa->v1->vn, efa->v2->vn, efa->v3->vn, NULL, efa, efa);
			}
	
			/* for transform */
			add_normal_aligned(nor, efa->n);
		}
	}
	
	if(del_old) {
		/* step 4: remove old faces, if del_old */
		efa= em->faces.first;
		while(efa) {
			nextfa= efa->next;
			if(efa->f & SELECT) {
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
			efa= nextfa;
		}
	
		/* step 5: remove selected unused edges */
		/* start tagging again */
		for(eed= em->edges.first; eed; eed= eed->next) eed->f1=0;
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->e1->f1= 1;
			efa->e2->f1= 1;
			efa->e3->f1= 1;
			if(efa->e4) efa->e4->f1= 1;
		}
		/* remove */
		eed= em->edges.first; 
		while(eed) {
			nexted= eed->next;
			if(eed->f & SELECT) {
				if(eed->f1==0) {
					remedge(eed);
					free_editedge(eed);
				}
			}
			eed= nexted;
		}
	
		/* step 6: remove selected unused vertices */
		for(eed= em->edges.first; eed; eed= eed->next) 
			eed->v1->f1= eed->v2->f1= 0;
		
		eve= em->verts.first;
		while(eve) {
			nextve= eve->next;
			if(eve->f1) {
				// hack... but we need it for step 7, redoing selection
				if(eve->vn) eve->vn->vn= eve->vn;
				
				BLI_remlink(&em->verts, eve);
				free_editvert(eve);
			}
			eve= nextve;
		}
	}
	
	Normalise(nor);	// translation normal grab
	
	/* step 7: redo selection */
	EM_clear_flag_all(SELECT);

	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->vn) {
			eve->vn->f |= SELECT;
		}
	}

	EM_select_flush();

	if(nor[0]==0.0 && nor[1]==0.0 && nor[2]==0.0) return 'g'; // grab
	return 'n'; // normal constraint 
}

short extrudeflag_vert(short flag, float *nor)
{
	/* all verts/edges/faces with (f & 'flag'): extrude */
	/* from old verts, 'flag' is cleared, in new ones it is set */
	EditMesh *em = G.editMesh;
	EditVert *eve, *v1, *v2, *v3, *v4, *nextve;
	EditEdge *eed, *e1, *e2, *e3, *e4, *nexted;
	EditFace *efa, *efa2, *nextvl;
	short sel=0, del_old= 0, is_face_sel=0;

	if(G.obedit==0 || get_mesh(G.obedit)==0) return 0;

	/* clear vert flag f1, we use this to detect a loose selected vertice */
	eve= em->verts.first;
	while(eve) {
		if(eve->f & flag) eve->f1= 1;
		else eve->f1= 0;
		eve= eve->next;
	}
	/* clear edges counter flag, if selected we set it at 1 */
	eed= em->edges.first;
	while(eed) {
		if( (eed->v1->f & flag) && (eed->v2->f & flag) ) {
			eed->f2= 1;
			eed->v1->f1= 0;
			eed->v2->f1= 0;
		}
		else eed->f2= 0;
		
		eed->f1= 1;		/* this indicates it is an 'old' edge (in this routine we make new ones) */
		eed->vn= NULL;	/* abused as sample */
		
		eed= eed->next;
	}

	/* we set a flag in all selected faces, and increase the associated edge counters */

	efa= em->faces.first;
	while(efa) {
		efa->f1= 0;

		if(faceselectedAND(efa, flag)) {
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			e4= efa->e4;

			if(e1->f2 < 3) e1->f2++;
			if(e2->f2 < 3) e2->f2++;
			if(e3->f2 < 3) e3->f2++;
			if(e4 && e4->f2 < 3) e4->f2++;
			
			efa->f1= 1;
			is_face_sel= 1;	// for del_old
		}
		else if(faceselectedOR(efa, flag)) {
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			e4= efa->e4;
			
			if( (e1->v1->f & flag) && (e1->v2->f & flag) ) e1->f1= 2;
			if( (e2->v1->f & flag) && (e2->v2->f & flag) ) e2->f1= 2;
			if( (e3->v1->f & flag) && (e3->v2->f & flag) ) e3->f1= 2;
			if( e4 && (e4->v1->f & flag) && (e4->v2->f & flag) ) e4->f1= 2;
		}
		
		// sample for next loop
		efa->e1->vn= (EditVert *)efa;
		efa->e2->vn= (EditVert *)efa;
		efa->e3->vn= (EditVert *)efa;
		if(efa->e4) efa->e4->vn= (EditVert *)efa;

		efa= efa->next;
	}

	set_edge_directions_f2(3);

	/* the current state now is:
		eve->f1==1: loose selected vertex 

		eed->f2==0 : edge is not selected, no extrude
		eed->f2==1 : edge selected, is not part of a face, extrude
		eed->f2==2 : edge selected, is part of 1 face, extrude
		eed->f2==3 : edge selected, is part of more faces, no extrude
		
		eed->f1==0: new edge
		eed->f1==1: edge selected, is part of selected face, when eed->f==3: remove
		eed->f1==2: edge selected, part of a partially selected face
					
		efa->f1==1 : duplicate this face
	*/

	/* copy all selected vertices, */
	/* write pointer to new vert in old struct at eve->vn */
	eve= em->verts.last;
	while(eve) {
		eve->f &= ~128;  /* clear, for later test for loose verts */
		if(eve->f & flag) {
			sel= 1;
			v1= addvertlist(0);
			
			VECCOPY(v1->co, eve->co);
			v1->f= eve->f;
			eve->f-= flag;
			eve->vn= v1;
		}
		else eve->vn= 0;
		eve= eve->prev;
	}

	if(sel==0) return 0;

	/* all edges with eed->f2==1 or eed->f2==2 become faces */
	
	/* if del_old==1 then extrude is in partial geometry, to keep it manifold.
					 verts with f1==0 and (eve->f & 128)==0) are removed
	                 edges with eed->f2>2 are removed
					 faces with efa->f1 are removed
	   if del_old==0 the extrude creates a volume.
	*/
	
	 /* find if we delete old faces */
	if(is_face_sel) {
		for(eed= em->edges.first; eed; eed= eed->next) {
			if( (eed->f2==1 || eed->f2==2) ) {
				if(eed->f1==2) {
					del_old= 1;
					break;
				}
			}
		}
	}
	
	eed= em->edges.last;
	while(eed) {
		nexted= eed->prev;
		if( eed->f2<3) {
			eed->v1->f |= 128;  /* = no loose vert! */
			eed->v2->f |= 128;
		}
		if( (eed->f2==1 || eed->f2==2) ) {
	
			/* if del_old, the preferred normal direction is exact opposite as for keep old faces */
			if(eed->dir!=del_old) efa2= addfacelist(eed->v1, eed->v2, eed->v2->vn, eed->v1->vn, NULL, NULL);
			else efa2= addfacelist(eed->v2, eed->v1, eed->v1->vn, eed->v2->vn, NULL, NULL);
			
			if(eed->vn) {
				efa= (EditFace *)eed->vn;
				efa2->mat_nr= efa->mat_nr;
				efa2->tf= efa->tf;
				efa2->flag= efa->flag;
			}
			
			/* Needs smarter adaption of existing creases.
			 * If addedgelist is used, make sure seams are set to 0 on these
			 * new edges, since we do not want to add any seams on extrusion.
			 */
			efa2->e1->crease= eed->crease;
			efa2->e2->crease= eed->crease;
			efa2->e3->crease= eed->crease;
			if(efa2->e4) efa2->e4->crease= eed->crease;
		}

		eed= nexted;
	}
	if(del_old) {
		eed= em->edges.first;
		while(eed) {
			nexted= eed->next;
			if(eed->f2==3 && eed->f1==1) {
				remedge(eed);
				free_editedge(eed);
			}
			eed= nexted;
		}
	}
	/* duplicate faces, if necessary remove old ones  */
	efa= em->faces.first;
	while(efa) {
		nextvl= efa->next;
		if(efa->f1 & 1) {
		
			v1= efa->v1->vn;
			v2= efa->v2->vn;
			v3= efa->v3->vn;
			if(efa->v4) v4= efa->v4->vn; else v4= 0;
			
			if(del_old==0)	// if we keep old, we flip normal
				efa2= addfacelist(v3, v2, v1, v4, efa, efa); /* hmm .. not sure about edges here */
			else
				efa2= addfacelist(v1, v2, v3, v4, efa, efa); /* hmm .. not sure about edges here */
			
			/* for transform */
			add_normal_aligned(nor, efa->n);

			if(del_old) {
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
		}
		efa= nextvl;
	}
	
	Normalise(nor);	// for grab
	
	/* for all vertices with eve->vn!=0 
		if eve->f1==1: make edge
		if flag!=128 : if del_old==1: remove
	*/
	eve= em->verts.last;
	while(eve) {
		nextve= eve->prev;
		if(eve->vn) {
			if(eve->f1==1) addedgelist(eve, eve->vn, NULL);
			else if( (eve->f & 128)==0) {
				if(del_old) {
					BLI_remlink(&em->verts,eve);
					free_editvert(eve);
					eve= NULL;
				}
			}
		}
		if(eve) {
			eve->f &= ~128;
		}
		eve= nextve;
	}
	// since its vertex select mode now, it also deselects higher order
	EM_selectmode_flush();

	if(nor[0]==0.0 && nor[1]==0.0 && nor[2]==0.0) return 'g'; // g is grab, for correct undo print
	return 'n';
}

/* generic extrude */
short extrudeflag(short flag, float *nor)
{
	if(G.scene->selectmode & SCE_SELECT_VERTEX)
		return extrudeflag_vert(flag, nor);
	else 
		return extrudeflag_edge(flag, nor);
		
}

void rotateflag(short flag, float *cent, float rotmat[][3])
{
	/* all verts with (flag & 'flag') rotate */
	EditMesh *em = G.editMesh;
	EditVert *eve;

	eve= em->verts.first;
	while(eve) {
		if(eve->f & flag) {
			eve->co[0]-=cent[0];
			eve->co[1]-=cent[1];
			eve->co[2]-=cent[2];
			Mat3MulVecfl(rotmat,eve->co);
			eve->co[0]+=cent[0];
			eve->co[1]+=cent[1];
			eve->co[2]+=cent[2];
		}
		eve= eve->next;
	}
}

void translateflag(short flag, float *vec)
{
	/* all verts with (flag & 'flag') translate */
	EditMesh *em = G.editMesh;
	EditVert *eve;

	eve= em->verts.first;
	while(eve) {
		if(eve->f & flag) {
			eve->co[0]+=vec[0];
			eve->co[1]+=vec[1];
			eve->co[2]+=vec[2];
		}
		eve= eve->next;
	}
}

void adduplicateflag(int flag)
{
	EditMesh *em = G.editMesh;
	/* old selection has flag 128 set, and flag 'flag' cleared
	   new selection has flag 'flag' set */
	EditVert *eve, *v1, *v2, *v3, *v4;
	EditEdge *eed, *newed;
	EditFace *efa, *newfa;

	EM_clear_flag_all(128);
	EM_selectmode_set();	// paranoia check, selection now is consistant

	/* vertices first */
	for(eve= em->verts.last; eve; eve= eve->prev) {

		if(eve->f & flag) {
			v1= addvertlist(eve->co);
			
			v1->f= eve->f;
			eve->f-= flag;
			eve->f|= 128;
			
			eve->vn= v1;

			/* >>>>> FIXME: Copy deformation weight ? */
			v1->totweight = eve->totweight;
			if (eve->totweight){
				v1->dw = MEM_mallocN (eve->totweight * sizeof(MDeformWeight), "deformWeight");
				memcpy (v1->dw, eve->dw, eve->totweight * sizeof(MDeformWeight));
			}
			else
				v1->dw=NULL;
		}
	}
	
	/* copy edges */
	for(eed= em->edges.last; eed; eed= eed->prev) {
		if( eed->f & flag ) {
			v1= eed->v1->vn;
			v2= eed->v2->vn;
			newed= addedgelist(v1, v2, eed);
			
			newed->f= eed->f;
			eed->f -= flag;
			eed->f |= 128;
		}
	}

	/* then dupicate faces */
	for(efa= em->faces.last; efa; efa= efa->prev) {
		if(efa->f & flag) {
			v1= efa->v1->vn;
			v2= efa->v2->vn;
			v3= efa->v3->vn;
			if(efa->v4) v4= efa->v4->vn; else v4= NULL;
			newfa= addfacelist(v1, v2, v3, v4, efa, efa); 
			
			newfa->f= efa->f;
			efa->f -= flag;
			efa->f |= 128;
		}
	}

	EM_fgon_flags();	// redo flags and indices for fgons
}

void delfaceflag(int flag)
{
	EditMesh *em = G.editMesh;
	/* delete all faces with 'flag', including loose edges and loose vertices */
	/* this is maybe a bit weird, but this function is used for 'split' and 'separate' */
	/* in remaining vertices/edges 'flag' is cleared */
	EditVert *eve,*nextve;
	EditEdge *eed, *nexted;
	EditFace *efa,*nextvl;

	/* to detect loose edges, we put f2 flag on 1 */
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f & flag) eed->f2= 1;
		else eed->f2= 0;
	}
	
	/* delete faces */
	efa= em->faces.first;
	while(efa) {
		nextvl= efa->next;
		if(efa->f & flag) {
			
			efa->e1->f2= 1;
			efa->e2->f2= 1;
			efa->e3->f2= 1;
			if(efa->e4) {
				efa->e4->f2= 1;
			}
								
			BLI_remlink(&em->faces, efa);
			free_editface(efa);
		}
		efa= nextvl;
	}
	
	/* all remaining faces: make sure we keep the edges */
	for(efa= em->faces.first; efa; efa= efa->next) {
		efa->e1->f2= 0;
		efa->e2->f2= 0;
		efa->e3->f2= 0;
		if(efa->e4) {
			efa->e4->f2= 0;
		}
	}
	
	/* remove tagged edges, and clear remaining ones */
	eed= em->edges.first;
	while(eed) {
		nexted= eed->next;
		
		if(eed->f2==1) {
			remedge(eed);
			free_editedge(eed);
		}
		else {
			eed->f &= ~flag;
			eed->v1->f &= ~flag;
			eed->v2->f &= ~flag;
		}
		eed= nexted;
	}
	
	/* vertices with 'flag' now are the loose ones, and will be removed */
	eve= em->verts.first;
	while(eve) {
		nextve= eve->next;
		if(eve->f & flag) {
			BLI_remlink(&em->verts, eve);
			free_editvert(eve);
		}
		eve= nextve;
	}

}

/* ********************* */
#if 0
static int check_vnormal_flip(float *n, float *vnorm) 
{
	float inp;

	inp= n[0]*vnorm[0]+n[1]*vnorm[1]+n[2]*vnorm[2];

	/* angles 90 degrees: dont flip */
	if(inp> -0.000001) return 0;

	return 1;
}
#endif

void flipface(EditFace *efa)
{
	if(efa->v4) {
		SWAP(EditVert *, efa->v2, efa->v4);
		SWAP(EditEdge *, efa->e1, efa->e4);
		SWAP(EditEdge *, efa->e2, efa->e3);
		SWAP(unsigned int, efa->tf.col[1], efa->tf.col[3]);
		SWAP(float, efa->tf.uv[1][0], efa->tf.uv[3][0]);
		SWAP(float, efa->tf.uv[1][1], efa->tf.uv[3][1]);
	}
	else {
		SWAP(EditVert *, efa->v2, efa->v3);
		SWAP(EditEdge *, efa->e1, efa->e3);
		SWAP(unsigned int, efa->tf.col[1], efa->tf.col[2]);
		efa->e2->dir= 1-efa->e2->dir;
		SWAP(float, efa->tf.uv[1][0], efa->tf.uv[2][0]);
		SWAP(float, efa->tf.uv[1][1], efa->tf.uv[2][1]);
	}
	if(efa->v4) CalcNormFloat4(efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co, efa->n);
	else CalcNormFloat(efa->v1->co, efa->v2->co, efa->v3->co, efa->n);
}


void flip_editnormals(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	efa= em->faces.first;
	while(efa) {
		if( efa->f & SELECT ){
			flipface(efa);
		}
		efa= efa->next;
	}
}

/* does face centers too */
void recalc_editnormals(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	EditVert *eve;

	for(eve= em->verts.first; eve; eve=eve->next) {
		eve->no[0] = eve->no[1] = eve->no[2] = 0.0;
	}

	for(efa= em->faces.first; efa; efa=efa->next) {
		if(efa->v4) {
			CalcNormFloat4(efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co, efa->n);
			CalcCent4f(efa->cent, efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
			VecAddf(efa->v4->no, efa->v4->no, efa->n);
		}
		else {
			CalcNormFloat(efa->v1->co, efa->v2->co, efa->v3->co, efa->n);
			CalcCent3f(efa->cent, efa->v1->co, efa->v2->co, efa->v3->co);
		}
		VecAddf(efa->v1->no, efa->v1->no, efa->n);
		VecAddf(efa->v2->no, efa->v2->no, efa->n);
		VecAddf(efa->v3->no, efa->v3->no, efa->n);
	}

	for(eve= em->verts.first; eve; eve=eve->next) {
		if (Normalise(eve->no)==0.0) {
			VECCOPY(eve->no, eve->co);
			Normalise(eve->no);
		}
	}
}

int compareface(EditFace *vl1, EditFace *vl2)
{
	EditVert *v1, *v2, *v3, *v4;
	
	if(vl1->v4 && vl2->v4) {
		v1= vl2->v1;
		v2= vl2->v2;
		v3= vl2->v3;
		v4= vl2->v4;
		
		if(vl1->v1==v1 || vl1->v2==v1 || vl1->v3==v1 || vl1->v4==v1) {
			if(vl1->v1==v2 || vl1->v2==v2 || vl1->v3==v2 || vl1->v4==v2) {
				if(vl1->v1==v3 || vl1->v2==v3 || vl1->v3==v3 || vl1->v4==v3) {
					if(vl1->v1==v4 || vl1->v2==v4 || vl1->v3==v4 || vl1->v4==v4) {
						return 1;
					}
				}
			}
		}
	}
	else if(vl1->v4==0 && vl2->v4==0) {
		v1= vl2->v1;
		v2= vl2->v2;
		v3= vl2->v3;
		
		if(vl1->v1==v1 || vl1->v2==v1 || vl1->v3==v1) {
			if(vl1->v1==v2 || vl1->v2==v2 || vl1->v3==v2) {
				if(vl1->v1==v3 || vl1->v2==v3 || vl1->v3==v3) {
					return 1;
				}
			}
		}
	}
	
	return 0;
}

/* checks for existance, not tria overlapping inside quad */
EditFace *exist_face(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4)
{
	EditMesh *em = G.editMesh;
	EditFace *efa, efatest;
	
	efatest.v1= v1;
	efatest.v2= v2;
	efatest.v3= v3;
	efatest.v4= v4;
	
	efa= em->faces.first;
	while(efa) {
		if(compareface(&efatest, efa)) return efa;
		efa= efa->next;
	}
	return NULL;
}

/* evaluate if entire quad is a proper convex quad */
int convex(float *v1, float *v2, float *v3, float *v4)
{
	float nor[3], nor1[3], nor2[3], vec[4][2];
	
	/* define projection, do both trias apart, quad is undefined! */
	CalcNormFloat(v1, v2, v3, nor1);
	CalcNormFloat(v1, v3, v4, nor2);
	nor[0]= ABS(nor1[0]) + ABS(nor2[0]);
	nor[1]= ABS(nor1[1]) + ABS(nor2[1]);
	nor[2]= ABS(nor1[2]) + ABS(nor2[2]);

	if(nor[2] >= nor[0] && nor[2] >= nor[1]) {
		vec[0][0]= v1[0]; vec[0][1]= v1[1];
		vec[1][0]= v2[0]; vec[1][1]= v2[1];
		vec[2][0]= v3[0]; vec[2][1]= v3[1];
		vec[3][0]= v4[0]; vec[3][1]= v4[1];
	}
	else if(nor[1] >= nor[0] && nor[1]>= nor[2]) {
		vec[0][0]= v1[0]; vec[0][1]= v1[2];
		vec[1][0]= v2[0]; vec[1][1]= v2[2];
		vec[2][0]= v3[0]; vec[2][1]= v3[2];
		vec[3][0]= v4[0]; vec[3][1]= v4[2];
	}
	else {
		vec[0][0]= v1[1]; vec[0][1]= v1[2];
		vec[1][0]= v2[1]; vec[1][1]= v2[2];
		vec[2][0]= v3[1]; vec[2][1]= v3[2];
		vec[3][0]= v4[1]; vec[3][1]= v4[2];
	}
	
	/* linetests, the 2 diagonals have to instersect to be convex */
	if( IsectLL2Df(vec[0], vec[2], vec[1], vec[3]) > 0 ) return 1;
	return 0;
}


/* ********************* Fake Polgon support (FGon) ***************** */


/* results in:
   - faces having ->fgonf flag set (also for draw)
   - edges having ->fgoni index set (for select)
*/

static float editface_area(EditFace *efa)
{
	if(efa->v4) return AreaQ3Dfl(efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
	else return AreaT3Dfl(efa->v1->co, efa->v2->co, efa->v3->co);
}

void EM_fgon_flags(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa, *efan, *efamax;
	EditEdge *eed;
	ListBase listb={NULL, NULL};
	float size, maxsize;
	short done, curindex= 1;
	
	// for each face with fgon edge AND not fgon flag set
	for(eed= em->edges.first; eed; eed= eed->next) eed->fgoni= 0;  // index
	for(efa= em->faces.first; efa; efa= efa->next) efa->fgonf= 0;  // flag
	
	// for speed & simplicity, put fgon face candidates in new listbase
	efa= em->faces.first;
	while(efa) {
		efan= efa->next;
		if( (efa->e1->h & EM_FGON) || (efa->e2->h & EM_FGON) || 
			(efa->e3->h & EM_FGON) || (efa->e4 && (efa->e4->h & EM_FGON)) ) {
			BLI_remlink(&em->faces, efa);
			BLI_addtail(&listb, efa);
		}
		efa= efan;
	}
	
	// find an undone face with fgon edge
	for(efa= listb.first; efa; efa= efa->next) {
		if(efa->fgonf==0) {
			
			// init this face
			efa->fgonf= EM_FGON;
			if(efa->e1->h & EM_FGON) efa->e1->fgoni= curindex;
			if(efa->e2->h & EM_FGON) efa->e2->fgoni= curindex;
			if(efa->e3->h & EM_FGON) efa->e3->fgoni= curindex;
			if(efa->e4 && (efa->e4->h & EM_FGON)) efa->e4->fgoni= curindex;
			
			// we search for largest face, to give facedot drawing rights
			maxsize= editface_area(efa);
			efamax= efa;
			
			// now flush curendex over edges and set faceflags
			done= 1;
			while(done==1) {
				done= 0;
				
				for(efan= listb.first; efan; efan= efan->next) {
					if(efan->fgonf==0) {
						// if one if its edges has index set, do other too
						if( (efan->e1->fgoni==curindex) || (efan->e2->fgoni==curindex) ||
							(efan->e3->fgoni==curindex) || (efan->e4 && (efan->e4->fgoni==curindex)) ) {
							
							efan->fgonf= EM_FGON;
							if(efan->e1->h & EM_FGON) efan->e1->fgoni= curindex;
							if(efan->e2->h & EM_FGON) efan->e2->fgoni= curindex;
							if(efan->e3->h & EM_FGON) efan->e3->fgoni= curindex;
							if(efan->e4 && (efan->e4->h & EM_FGON)) efan->e4->fgoni= curindex;
							
							size= editface_area(efan);
							if(size>maxsize) {
								efamax= efan;
								maxsize= size;
							}
							done= 1;
						}
					}
				}
			}
			
			efamax->fgonf |= EM_FGON_DRAW;
			curindex++;

		}
	}

	// put fgon face candidates back in listbase
	efa= listb.first;
	while(efa) {
		efan= efa->next;
		BLI_remlink(&listb, efa);
		BLI_addtail(&em->faces, efa);
		efa= efan;
	}
	
	// remove fgon flags when edge not in fgon (anymore)
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->fgoni==0) eed->h &= ~EM_FGON;
	}
	
}




