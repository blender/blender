 /* $Id: bmesh_tools.c
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_key_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_types.h"
#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_screen.h"
#include "BIF_transform.h"

#include "UI_interface.h"

#include "mesh_intern.h"
#include "bmesh.h"

static void add_normal_aligned(float *nor, float *add)
{
	if( INPR(nor, add) < -0.9999f)
		VecSubf(nor, nor, add);
	else
		VecAddf(nor, nor, add);
}

static int subdivide_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	
	BM_esubdivideflag(obedit, em->bm, 1, 0.0, scene->toolsettings->editbutflag, 1, 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

void MESH_OT_subdivide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide";
	ot->idname= "MESH_OT_subdivide";
	
	/* api callbacks */
	ot->exec= subdivide_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int subdivide_multi_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	
	BM_esubdivideflag(obedit, em->bm, 1, 0.0, scene->toolsettings->editbutflag, RNA_int_get(op->ptr,"number_cuts"), 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

void MESH_OT_subdivide_multi(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide Multi";
	ot->idname= "MESH_OT_subdivide_multi";
	
	/* api callbacks */
	ot->exec= subdivide_multi_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "number_cuts", 4, 1, 100, "Number of Cuts", "", 1, INT_MAX);
}

static int subdivide_multi_fractal_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;

	BM_esubdivideflag(obedit, em->bm, 1, -(RNA_float_get(op->ptr, "random_factor")/100), scene->toolsettings->editbutflag, RNA_int_get(op->ptr, "number_cuts"), 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

void MESH_OT_subdivide_multi_fractal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide Multi Fractal";
	ot->idname= "MESH_OT_subdivide_multi_fractal";
	
	/* api callbacks */
	ot->exec= subdivide_multi_fractal_exec;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "number_cuts", 4, 1, 100, "Number of Cuts", "", 1, INT_MAX);
	RNA_def_float(ot->srna, "random_factor", 5.0, 0.0f, FLT_MAX, "Random Factor", "", 0.0f, 1000.0f);
}

static int subdivide_smooth_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;

	BM_esubdivideflag(obedit, em->bm, 1, 0.292f*RNA_float_get(op->ptr, "smoothness"), scene->toolsettings->editbutflag | B_SMOOTH, 1, 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

void MESH_OT_subdivide_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide Smooth";
	ot->idname= "MESH_OT_subdivide_smooth";
	
	/* api callbacks */
	ot->exec= subdivide_smooth_exec;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "smoothness", 1.0f, 0.0f, 1000.0f, "Smoothness", "", 0.0f, FLT_MAX);
}

static int subdivs_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Subdivision Type", 0);
	layout= uiPupMenuLayout(pup);
	uiItemsEnumO(layout, "MESH_OT_subdivs", "type");
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

static int subdivs_exec(bContext *C, wmOperator *op)
{	
	switch(RNA_int_get(op->ptr, "type"))
	{
		case 0: // simple
			subdivide_exec(C,op);
			break;
		case 1: // multi
			subdivide_multi_exec(C,op);
			break;
		case 2: // fractal;
			subdivide_multi_fractal_exec(C,op);
			break;
		case 3: //smooth
			subdivide_smooth_exec(C,op);
			break;
	}
					 
	return OPERATOR_FINISHED;
}

void MESH_OT_subdivs(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[]= {
		{0, "SIMPLE", "Simple", ""},
		{1, "MULTI", "Multi", ""},
		{2, "FRACTAL", "Fractal", ""},
		{3, "SMOOTH", "Smooth", ""},
		{0, NULL, NULL}};

	/* identifiers */
	ot->name= "subdivs";
	ot->idname= "MESH_OT_subdivs";
	
	/* api callbacks */
	ot->invoke= subdivs_invoke;
	ot->exec= subdivs_exec;
	
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/*props */
	RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
	
	/* this is temp, the ops are different, but they are called from subdivs, so all the possible props should be here as well*/
	RNA_def_int(ot->srna, "number_cuts", 4, 1, 10, "Number of Cuts", "", 1, INT_MAX);
	RNA_def_float(ot->srna, "random_factor", 5.0, 0.0f, FLT_MAX, "Random Factor", "", 0.0f, 1000.0f);
	RNA_def_float(ot->srna, "smoothness", 1.0f, 0.0f, 1000.0f, "Smoothness", "", 0.0f, FLT_MAX);		
}

/* individual face extrude */
/* will use vertex normals for extrusion directions, so *nor is unaffected */
short EDBM_Extrude_face_indiv(BMEditMesh *em, short flag, float *nor)
{
#if 0
	EditVert *eve, *v1, *v2, *v3, *v4;
	EditEdge *eed;
	EditFace *efa, *nextfa;
	
	if(em==NULL) return 0;
	
	/* selected edges with 1 or more selected face become faces */
	/* selected faces each makes new faces */
	/* always remove old faces, keeps volumes manifold */
	/* select the new extrusion, deselect old */
	
	/* step 1; init, count faces in edges */
	recalc_editnormals(em);
	
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
			v1= addvertlist(em, efa->v1->co, efa->v1);
			v2= addvertlist(em, efa->v2->co, efa->v2);
			v3= addvertlist(em, efa->v3->co, efa->v3);
			
			v1->f1= v2->f1= v3->f1= 1;
			VECCOPY(v1->no, efa->n);
			VECCOPY(v2->no, efa->n);
			VECCOPY(v3->no, efa->n);
			if(efa->v4) {
				v4= addvertlist(em, efa->v4->co, efa->v4); 
				v4->f1= 1;
				VECCOPY(v4->no, efa->n);
			}
			else v4= NULL;
			
			/* side faces, clockwise */
			addfacelist(em, efa->v2, v2, v1, efa->v1, efa, NULL);
			addfacelist(em, efa->v3, v3, v2, efa->v2, efa, NULL);
			if(efa->v4) {
				addfacelist(em, efa->v4, v4, v3, efa->v3, efa, NULL);
				addfacelist(em, efa->v1, v1, v4, efa->v4, efa, NULL);
			}
			else {
				addfacelist(em, efa->v1, v1, v3, efa->v3, efa, NULL);
			}
			/* top face */
			addfacelist(em, v1, v2, v3, v4, efa, NULL);
		}
	}
	
	/* step 3: remove old faces */
	efa= em->faces.first;
	while(efa) {
		nextfa= efa->next;
		if(efa->f & SELECT) {
			BLI_remlink(&em->faces, efa);
			free_editface(em, efa);
		}
		efa= nextfa;
	}

	/* step 4: redo selection */
	EM_clear_flag_all(em, SELECT);
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f1)  eve->f |= SELECT;
	}
	
	EM_select_flush(em);
	
	return 'n';
#endif
}


/* extrudes individual edges */
/* nor is filled with constraint vector */
short EDBM_Extrude_edges_indiv(BMEditMesh *em, short flag, float *nor) 
{
#if 0
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	for(eve= em->verts.first; eve; eve= eve->next) eve->tmp.v = NULL;
	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->tmp.f = NULL;
		eed->f2= ((eed->f & flag)!=0);
	}
	
	set_edge_directions_f2(em, 2);

	/* sample for next loop */
	for(efa= em->faces.first; efa; efa= efa->next) {
		efa->e1->tmp.f = efa;
		efa->e2->tmp.f = efa;
		efa->e3->tmp.f = efa;
		if(efa->e4) efa->e4->tmp.f = efa;
	}
	/* make the faces */
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f & flag) {
			if(eed->v1->tmp.v == NULL)
				eed->v1->tmp.v = addvertlist(em, eed->v1->co, eed->v1);
			if(eed->v2->tmp.v == NULL)
				eed->v2->tmp.v = addvertlist(em, eed->v2->co, eed->v2);

			if(eed->dir==1) 
				addfacelist(em, eed->v1, eed->v2, 
							eed->v2->tmp.v, eed->v1->tmp.v, 
							eed->tmp.f, NULL);
			else 
				addfacelist(em, eed->v2, eed->v1, 
							eed->v1->tmp.v, eed->v2->tmp.v, 
							eed->tmp.f, NULL);

			/* for transform */
			if(eed->tmp.f) {
				efa = eed->tmp.f;
				if (efa->f & SELECT) add_normal_aligned(nor, efa->n);
			}
		}
	}
	Normalize(nor);
	
	/* set correct selection */
	EM_clear_flag_all(em, SELECT);
	for(eve= em->verts.last; eve; eve= eve->prev) {
		if(eve->tmp.v) {
			eve->tmp.v->f |= flag;
		}
	}

	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->v1->f & eed->v2->f & flag) eed->f |= flag;
	}
	
	if(nor[0]==0.0 && nor[1]==0.0 && nor[2]==0.0) return 'g'; // g is grab
#endif
	return 'n';  // n is for normal constraint
}

/* extrudes individual vertices */
short EDBM_Extrude_verts_indiv(BMEditMesh *em, wmOperator *op, short flag, float *nor) 
{
	BMOperator bmop;
	BMIter iter;
	BMOIter siter;
	BMVert *v;

	EDBM_InitOpf(em, &bmop, op, "extrude_vert_indiv verts=%hv", flag);

	/*deselect original verts*/
	v = BMO_IterNew(&siter, em->bm, &bmop, "verts");
	for (; v; v=BMO_IterStep(&siter)) {
		BM_Select(em->bm, v, 0);
	}

	BMO_Exec_Op(em->bm, &bmop);

	v = BMO_IterNew(&siter, em->bm, &bmop, "vertout");
	for (; v; v=BMO_IterStep(&siter)) {
		BM_Select(em->bm, v, 1);
	}

	if (!EDBM_FinishOp(em, &bmop, op, 1)) return 0;

	return 'g'; // g is grab
}

short EDBM_Extrude_edge(Object *obedit, BMEditMesh *em, int eflag, float *nor)
{
	BMesh *bm = em->bm;
	BMIter iter;
	BMOIter siter;
	BMOperator extop;
	BMVert *vert;
	BMEdge *edge;
	BMFace *f;
	void *el;
	ModifierData *md;
	int flag;
	
	switch (eflag) {
		case SELECT:
			flag = BM_SELECT;
			break;
		default:
			flag = BM_SELECT;
			break;
	}

	for (f=BMIter_New(&iter, bm, BM_FACES_OF_MESH, NULL);f;f=BMIter_Step(&iter)) {
		add_normal_aligned(nor, f->no);
	}
	Normalize(nor);

	BMO_Init_Op(&extop, "extrudefaceregion");
	BMO_HeaderFlag_To_Slot(bm, &extop, "edgefacein",
		               flag, BM_VERT|BM_EDGE|BM_FACE);

	BM_ITER(vert, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BM_Select(bm, vert, 0);
	}

	BM_ITER(edge, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BM_Select(bm, edge, 0);
	}

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		BM_Select(bm, f, 0);
	}

	/* If a mirror modifier with clipping is on, we need to adjust some 
	 * of the cases above to handle edges on the line of symmetry.
	 */
	md = obedit->modifiers.first;
	for (; md; md=md->next) {
		if (md->type==eModifierType_Mirror) {
			MirrorModifierData *mmd = (MirrorModifierData*) md;	
		
			if(mmd->flag & MOD_MIR_CLIPPING) {
				float mtx[4][4];
				if (mmd->mirror_ob) {
					float imtx[4][4];
					Mat4Invert(imtx, mmd->mirror_ob->obmat);
					Mat4MulMat4(mtx, obedit->obmat, imtx);
				}

				for (edge=BMIter_New(&iter,bm,BM_EDGES_OF_MESH,NULL);
				     edge; edge=BMIter_Step(&iter))
				{
					if(edge->head.flag & flag) {
						float co1[3], co2[3];

						VecCopyf(co1, edge->v1->co);
						VecCopyf(co2, edge->v2->co);

						if (mmd->mirror_ob) {
							VecMat4MulVecfl(co1, mtx, co1);
							VecMat4MulVecfl(co2, mtx, co2);
						}

						if (mmd->flag & MOD_MIR_AXIS_X)
							if ( (fabs(co1[0]) < mmd->tolerance) &&
								 (fabs(co2[0]) < mmd->tolerance) )
								BMO_Insert_MapPointer(bm, &extop, "exclude", edge, NULL);

						if (mmd->flag & MOD_MIR_AXIS_Y)
							if ( (fabs(co1[1]) < mmd->tolerance) &&
								 (fabs(co2[1]) < mmd->tolerance) )
								BMO_Insert_MapPointer(bm, &extop, "exclude", edge, NULL);

						if (mmd->flag & MOD_MIR_AXIS_Z)
							if ( (fabs(co1[2]) < mmd->tolerance) &&
								 (fabs(co2[2]) < mmd->tolerance) )
								BMO_Insert_MapPointer(bm, &extop, "exclude", edge, NULL);
					}
				}
			}
		}
	}

	BMO_Exec_Op(bm, &extop);

	BMO_ITER(el, &siter, bm, &extop, "geomout") {
		BM_Select(bm, el, 1);
	}

	BMO_Finish_Op(bm, &extop);

	if(nor[0]==0.0 && nor[1]==0.0 && nor[2]==0.0) return 'g'; // grab
	return 'n'; // normal constraint 

}
short EDBM_Extrude_vert(Object *obedit, BMEditMesh *em, short flag, float *nor)
{
		BMIter iter;
		BMEdge *eed;
		
		/*ensure vert flags are consistent for edge selections*/
		eed = BMIter_New(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; eed; eed=BMIter_Step(&iter)) {
			if (BM_TestHFlag(eed, flag)) {
				BM_SetHFlag(eed->v1, flag);
				BM_SetHFlag(eed->v2, flag);
			} else {
				if (BM_TestHFlag(eed->v1, flag) && BM_TestHFlag(eed->v2, flag))
					BM_SetHFlag(eed, flag);
			}
		}

		return EDBM_Extrude_edge(obedit, em, flag, nor);

}

/* generic extrude */
short EDBM_Extrude(Object *obedit, BMEditMesh *em, short flag, float *nor)
{
	if(em->selectmode & SCE_SELECT_VERTEX) {
		BMIter iter;
		BMEdge *eed;
		
		/*ensure vert flags are consistent for edge selections*/
		eed = BMIter_New(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; eed; eed=BMIter_Step(&iter)) {
			if (BM_TestHFlag(eed, flag)) {
				if (flag != BM_SELECT) {
					BM_SetHFlag(eed->v1, flag);
					BM_SetHFlag(eed->v2, flag);
				} else {
					BM_Select(em->bm, eed->v1, 1);
					BM_Select(em->bm, eed->v2, 1);
				}
			} else {
				if (BM_TestHFlag(eed->v1, flag) && BM_TestHFlag(eed->v2, flag)) {
					if (flag != BM_SELECT)
						BM_SetHFlag(eed, flag);
					else BM_Select(em->bm, eed, 1);
				}
			}
		}

		return EDBM_Extrude_edge(obedit, em, flag, nor);
	} else 
		return EDBM_Extrude_edge(obedit, em, flag, nor);
		
}


static int extrude_repeat_mesh(bContext *C, wmOperator *op)
{
#if 0
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= EM_GetEditMesh((Mesh *)obedit->data);

	RegionView3D *rv3d = CTX_wm_region_view3d(C);		
		
	int steps = RNA_int_get(op->ptr,"steps");
	
	float offs = RNA_float_get(op->ptr,"offset");

	float dvec[3], tmat[3][3], bmat[3][3], nor[3]= {0.0, 0.0, 0.0};
	short a;

	/* dvec */
	dvec[0]= rv3d->persinv[2][0];
	dvec[1]= rv3d->persinv[2][1];
	dvec[2]= rv3d->persinv[2][2];
	Normalize(dvec);
	dvec[0]*= offs;
	dvec[1]*= offs;
	dvec[2]*= offs;

	/* base correction */
	Mat3CpyMat4(bmat, obedit->obmat);
	Mat3Inv(tmat, bmat);
	Mat3MulVecfl(tmat, dvec);

	for(a=0; a<steps; a++) {
		extrudeflag(obedit, em, SELECT, nor);
		translateflag(em, SELECT, dvec);
	}
	
	recalc_editnormals(em);
	
	EM_fgon_flags(em);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

//	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	EM_EndEditMesh(obedit->data, em);
#endif
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_repeat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Extrude Repeat Mesh";
	ot->idname= "MESH_OT_extrude_repeat";
	
	/* api callbacks */
	ot->exec= extrude_repeat_mesh;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "offset", 2.0f, 0.0f, 100.0f, "Offset", "", 0.0f, FLT_MAX);
	RNA_def_int(ot->srna, "steps", 10, 0, 180, "Steps", "", 0, INT_MAX);
}

/* generic extern called extruder */
void EDBM_Extrude_Mesh(Object *obedit, BMEditMesh *em, wmOperator *op)
{
	Scene *scene= NULL;		// XXX CTX!
	float nor[3]= {0.0, 0.0, 0.0};
	short nr, transmode= 0;

	if(em->selectmode & SCE_SELECT_VERTEX) {
		if(em->bm->totvertsel==0) nr= 0;
		else if(em->bm->totvertsel==1) nr= 4;
		else if(em->bm->totedgesel==0) nr= 4;
		else if(em->bm->totfacesel==0) 
			nr= 3; // pupmenu("Extrude %t|Only Edges%x3|Only Vertices%x4");
		else if(em->bm->totfacesel==1)
			nr= 1; // pupmenu("Extrude %t|Region %x1|Only Edges%x3|Only Vertices%x4");
		else 
			nr= 1; // pupmenu("Extrude %t|Region %x1||Individual Faces %x2|Only Edges%x3|Only Vertices%x4");
	}
	else if(em->selectmode & SCE_SELECT_EDGE) {
		if (em->bm->totedgesel==0) nr = 0;
		
		nr = 1;
		/*else if (em->totedgesel==1) nr = 3;
		else if(em->totfacesel==0) nr = 3;
		else if(em->totfacesel==1)
			nr= 1; // pupmenu("Extrude %t|Region %x1|Only Edges%x3");
		else
			nr= 1; // pupmenu("Extrude %t|Region %x1||Individual Faces %x2|Only Edges%x3");
		*/
	}
	else {
		if (em->bm->totfacesel == 0) nr = 0;
		else if (em->bm->totfacesel == 1) nr = 1;
		else
			nr= 1; // pupmenu("Extrude %t|Region %x1||Individual Faces %x2");
	}
		
	if(nr<1) return;

	if(nr==1)  transmode= EDBM_Extrude(obedit, em, SELECT, nor);
	else if(nr==4) transmode= EDBM_Extrude_verts_indiv(em, op, SELECT, nor);
	else if(nr==3) transmode= EDBM_Extrude_edges_indiv(em, SELECT, nor);
	else transmode= EDBM_Extrude_face_indiv(em, SELECT, nor);
	
	if(transmode==0) {
		BKE_report(op->reports, RPT_ERROR, "Not a valid selection for extrude");
	}
	else {
		
			/* We need to force immediate calculation here because 
			* transform may use derived objects (which are now stale).
			*
			* This shouldn't be necessary, derived queries should be
			* automatically building this data if invalid. Or something.
			*/
//		DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);	
		object_handle_update(scene, obedit);

		/* individual faces? */
//		BIF_TransformSetUndo("Extrude");
		if(nr==2) {
//			initTransform(TFM_SHRINKFATTEN, CTX_NO_PET|CTX_NO_MIRROR);
//			Transform();
		}
		else {
//			initTransform(TFM_TRANSLATION, CTX_NO_PET|CTX_NO_MIRROR);
			if(transmode=='n') {
				Mat4MulVecfl(obedit->obmat, nor);
				VecSubf(nor, nor, obedit->obmat[3]);
//				BIF_setSingleAxisConstraint(nor, "along normal");
			}
//			Transform();
		}
	}

}

// XXX should be a menu item
static int mesh_extrude_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;

	EDBM_Extrude_Mesh(obedit,em, op);
	
	RNA_int_set(op->ptr, "mode", TFM_TRANSLATION);
	WM_operator_name_call(C, "TFM_OT_transform", WM_OP_INVOKE_REGION_WIN, op->ptr);

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

/* extrude without transform */
static int mesh_extrude_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	
	EDBM_Extrude_Mesh(obedit, em, op);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}


void MESH_OT_extrude(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Extrude Mesh";
	ot->idname= "MESH_OT_extrude";
	
	/* api callbacks */
	ot->invoke= mesh_extrude_invoke;
	ot->exec= mesh_extrude_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

/* ******************** (de)select all operator **************** */

void EDBM_toggle_select_all(BMEditMesh *em) /* exported for UV */
{
	if(em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel)
		EDBM_clear_flag_all(em, SELECT);
	else 
		EDBM_set_flag_all(em, SELECT);
}

static int toggle_select_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	
	EDBM_toggle_select_all(em);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_all_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select or Deselect All";
	ot->idname= "MESH_OT_select_all_toggle";
	
	/* api callbacks */
	ot->exec= toggle_select_all_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
