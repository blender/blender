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
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the multiresolution modeling tools.
 *
 * multires.h
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"

#include "BIF_editmesh.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BDR_editobject.h"
#include "BDR_sculptmode.h"

#include "BLI_editVert.h"

#include "BSE_edit.h"
#include "BSE_view.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "blendef.h"
#include "editmesh.h"
#include "multires.h"
#include "mydevice.h"
#include "parametrizer.h"

#include <math.h>

void multires_calc_temp_data(struct MultiresLevel *lvl);

int multires_test()
{
	Mesh *me= get_mesh(OBACT);
	if(me && me->mr) {
		error("Unable to complete action with multires enabled.");
		return 1;
	}
	return 0;
}
int multires_level1_test()
{
	Mesh *me= get_mesh(OBACT);
	if(me && me->mr && me->mr->current != 1) {
		error("Operation only available for multires level 1.");
		return 1;
	}
	return 0;
}

/* Sculptmode */

void multires_check_state()
{
	if(G.f & G_SCULPTMODE && !G.obedit)
		sculptmode_correct_state();
}

static void medge_flag_to_eed(const short flag, const char crease, EditEdge *eed)
{
	if(!eed) return;
	
	if(flag & ME_SEAM) eed->seam= 1;
	if(flag & ME_SHARP) eed->sharp = 1;
	if(flag & SELECT) eed->f |= SELECT;
	if(flag & ME_FGON) eed->h= EM_FGON;
	if(flag & ME_HIDE) eed->h |= 1;
	
	eed->crease= ((float)crease)/255.0;
}

void multires_level_to_editmesh(Object *ob, Mesh *me, const int render)
{
	MultiresLevel *lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
	int i;
	EditMesh *em= (!render && G.obedit) ? G.editMesh : NULL;
	EditVert **eves= NULL;
	EditEdge *eed= NULL;

	if(em) {
		/* Remove editmesh elements */
		free_editMesh(em);
		
		eves= MEM_callocN(sizeof(EditVert*)*lvl->totvert, "editvert pointers");

		/* Vertices/Edges/Faces */
		for(i=0; i<lvl->totvert; ++i) {
			eves[i]= addvertlist(me->mr->verts[i].co, NULL);
			if(me->mr->verts[i].flag & 1) eves[i]->f |= SELECT;
			if(me->mr->verts[i].flag & ME_HIDE) eves[i]->h= 1;
			eves[i]->data= NULL;
		}
		for(i=0; i<lvl->totedge; ++i) {
			addedgelist(eves[lvl->edges[i].v[0]], eves[lvl->edges[i].v[1]], NULL);
		}
		for(i=0; i<lvl->totface; ++i) {
			EditVert *eve4= lvl->faces[i].v[3] ? eves[lvl->faces[i].v[3]] : NULL;
			EditFace *efa= addfacelist(eves[lvl->faces[i].v[0]], eves[lvl->faces[i].v[1]],
						   eves[lvl->faces[i].v[2]], eve4, NULL, NULL);
			efa->flag= lvl->faces[i].flag & ~ME_HIDE;
			efa->mat_nr= lvl->faces[i].mat_nr;
			if(lvl->faces[i].flag & ME_FACE_SEL)
				efa->f |= SELECT;
			if(lvl->faces[i].flag & ME_HIDE) efa->h= 1;
			efa->data= NULL;
		}
	
		/* Edge flags */
		eed= em->edges.first;
		if(lvl==me->mr->levels.first) {
			for(i=0; i<lvl->totedge; ++i) {
				medge_flag_to_eed(me->mr->edge_flags[i], me->mr->edge_creases[i], eed);
				eed= eed->next;
			}
		} else {
			MultiresLevel *lvl1= me->mr->levels.first;
			const int last= lvl1->totedge * pow(2, me->mr->current-1);
			for(i=0; i<last; ++i) {
				const int ndx= i / pow(2, me->mr->current-1);
			
				medge_flag_to_eed(me->mr->edge_flags[ndx], me->mr->edge_creases[ndx], eed);
				eed= eed->next;
			}
		}

		eed= em->edges.first;
		for(i=0, eed= em->edges.first; i<lvl->totedge; ++i, eed= eed->next) {
			eed->h= me->mr->verts[lvl->edges[i].v[0]].flag & ME_HIDE ||
				me->mr->verts[lvl->edges[i].v[1]].flag & ME_HIDE;
		}
	
		EM_select_flush();

		multires_customdata_to_mesh(me, em, lvl, &me->mr->vdata, em ? &em->vdata : &me->vdata, CD_MDEFORMVERT);
		multires_customdata_to_mesh(me, em, lvl, &me->mr->fdata, em ? &em->fdata : &me->fdata, CD_MTFACE);

		/* Colors */
		if(me->mr->use_col) {
			MCol c[4];
			EditFace *efa= NULL;
			CustomData *src= &em->fdata;

			if(me->mr->use_col) EM_add_data_layer(src, CD_MCOL);
			efa= em->faces.first;
		
			for(i=0; i<lvl->totface; ++i) {
				if(me->mr->use_col) {
					multires_to_mcol(&lvl->colfaces[i], c);
					CustomData_em_set(src, efa->data, CD_MCOL, c);
				}
				efa= efa->next;
			}
			
		}
	
		mesh_update_customdata_pointers(me);
	
		MEM_freeN(eves);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		recalc_editnormals();
	}
}

void multires_make(void *ob, void *me_v)
{
	Mesh *me= me_v;
	Key *key;
	
	/* Check for shape keys */
	key= me->key;
	if(key) {
		int ret= okee("Adding multires will delete all shape keys, proceed?");
		if(ret) {
			free_key(key);
			me->key= NULL;
		} else
			return;
	}
	
	waitcursor(1);
	
	multires_check_state();

	multires_create(me);

	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Make multires");
	waitcursor(0);
}

void multires_delete(void *ob, void *me_v)
{
	Mesh *me= me_v;
	multires_free(me->mr);
	me->mr= NULL;
	
	multires_check_state();

	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Apply multires");
}

/* Make sure that all level indices are clipped to [1, mr->level_count] */
void multires_clip_levels(Multires *mr)
{
	if(mr) {
		const int cnt = mr->level_count;
	
		if(mr->current < 1) mr->current = 1;
		if(mr->edgelvl < 1) mr->edgelvl = 1;
		if(mr->pinlvl < 1) mr->pinlvl = 1;
		if(mr->renderlvl < 1) mr->renderlvl = 1;
		
		if(mr->current > cnt) mr->current = cnt;
		if(mr->edgelvl > cnt) mr->edgelvl = cnt;
		if(mr->pinlvl > cnt) mr->pinlvl = cnt;
		if(mr->renderlvl > cnt) mr->renderlvl = cnt;
	}
}

/* Delete all multires levels beneath current level. Subdivide special
   first-level data up to the new lowest level. */
void multires_del_lower(void *ob, void *me)
{
	Multires *mr= ((Mesh*)me)->mr;
	MultiresLevel *lvl1= mr->levels.first, *cr_lvl= current_level(mr);
	MultiresLevel *lvl= NULL, *lvlprev= NULL;
	short *edgeflags= NULL;
	char *edgecreases= NULL;
	int i, last;
	
	if(cr_lvl == lvl1) return;
	
	multires_check_state();
	
	/* Subdivide the edge flags to the current level */
	edgeflags= MEM_callocN(sizeof(short)*current_level(mr)->totedge, "Multires Edge Flags");
	edgecreases= MEM_callocN(sizeof(char)*current_level(mr)->totedge, "Multires Edge Creases");
	last= lvl1->totedge * pow(2, mr->current-1);
	for(i=0; i<last; ++i) {
		edgeflags[i] = mr->edge_flags[(int)(i / pow(2, mr->current-1))];
		edgecreases[i] = mr->edge_creases[(int)(i / pow(2, mr->current-1))];
	}
	MEM_freeN(mr->edge_flags);
	MEM_freeN(mr->edge_creases);
	mr->edge_flags= edgeflags;
	mr->edge_creases= edgecreases;
	
	multires_del_lower_customdata(mr, cr_lvl);
	
	lvl= cr_lvl->prev;
	while(lvl) {
		lvlprev= lvl->prev;
		
		multires_free_level(lvl);
		BLI_freelinkN(&mr->levels, lvl);
		
		mr->current-= 1;
		mr->level_count-= 1;
		
		lvl= lvlprev;
	}
	mr->newlvl= mr->current;
	
	multires_clip_levels(mr);

	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Multires delete lower");
}

void multires_del_higher(void *ob, void *me)
{
	Multires *mr= ((Mesh*)me)->mr;
	MultiresLevel *lvl= BLI_findlink(&mr->levels,mr->current-1);
	MultiresLevel *lvlnext;
	
	multires_check_state();
	
	lvl= lvl->next;
	while(lvl) {
		lvlnext= lvl->next;
		
		multires_free_level(lvl);
		BLI_freelinkN(&mr->levels,lvl);

		mr->level_count-= 1;
		
		lvl= lvlnext;
	}
	
	multires_clip_levels(mr);

	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Multires delete higher");
}

static void multires_finish_mesh_update(Object *ob)
{
	/* friendly check for background render */
	if(G.background==0) {
		object_handle_update(ob);
		countall();
		
		if(G.vd && G.vd->depths) G.vd->depths->damaged= 1;
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
	}
}

void multires_subdivide(void *ob_v, void *me_v)
{
	Mesh *me = me_v;

	multires_check_state();

	if(CustomData_number_of_layers(G.obedit ? &G.editMesh->fdata : &me->fdata, CD_MCOL) > 1) {
		int ret= okee("Adding a level will delete all but the active vertex color layer, proceed?");
		if(!ret)
			return;
	}

	waitcursor(1);
	multires_add_level(ob_v, me, G.scene->toolsettings->multires_subdiv_type);
	multires_level_to_editmesh(ob_v, me, 0);
	multires_finish_mesh_update(ob_v);

	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Add multires level");
	waitcursor(0);
}

void multires_set_level_cb(void *ob, void *me)
{
	waitcursor(1);
	
	multires_check_state();

	multires_set_level(ob, me, 0);
	multires_level_to_editmesh(ob, me, 0);
	multires_finish_mesh_update(ob);
	
	if(G.obedit || G.f & G_SCULPTMODE)
		BIF_undo_push("Multires set level");

	allqueue(REDRAWBUTSEDIT, 0);
	
	waitcursor(0);
}

void multires_edge_level_update_cb(void *ob_v, void *me_v)
{
	multires_edge_level_update(ob_v, me_v);
	allqueue(REDRAWVIEW3D, 0);
}

int multires_modifier_warning()
{
	ModifierData *md;
	
	for(md= modifiers_getVirtualModifierList(OBACT); md; md= md->next) {
		if(md->mode & eModifierMode_Render) {
			switch(md->type) {
			case eModifierType_Subsurf:
			case eModifierType_Build:
			case eModifierType_Mirror:
			case eModifierType_Decimate:
			case eModifierType_Boolean:
			case eModifierType_Array:
			case eModifierType_EdgeSplit:
				return 1;
			}
		}
	}
	
	return 0;
}
