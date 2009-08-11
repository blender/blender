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
#include "ED_transform.h"

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
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	int cuts= RNA_int_get(op->ptr,"number_cuts");
	float smooth= 0.292f*RNA_float_get(op->ptr, "smoothness");
	float fractal= RNA_float_get(op->ptr, "fractal")/100;
	int flag= 0;

	if(smooth != 0.0f)
		flag |= B_SMOOTH;
	if(fractal != 0.0f)
		flag |= B_FRACTAL;

	BM_esubdivideflag(obedit, em->bm, BM_SELECT, smooth, fractal, scene->toolsettings->editbutflag|flag, cuts, 0);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
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

	/* properties */
	RNA_def_int(ot->srna, "number_cuts", 1, 1, 20, "Number of Cuts", "", 1, INT_MAX);
	RNA_def_float(ot->srna, "fractal", 0.0, 0.0f, FLT_MAX, "Fractal", "Fractal randomness factor.", 0.0f, 1000.0f);
	RNA_def_float(ot->srna, "smoothness", 0.0f, 0.0f, 1000.0f, "Smoothness", "Smoothness factor.", 0.0f, FLT_MAX);
}

#if 0
static int subdivide_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	
	BM_esubdivideflag(obedit, em->bm, 1, 0.0, scene->toolsettings->editbutflag, 1, 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

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
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

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
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

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
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

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
		{0, "SIMPLE", 0, "Simple", ""},
		{1, "MULTI", 0, "Multi", ""},
		{2, "FRACTAL", 0, "Fractal", ""},
		{3, "SMOOTH", 0, "Smooth", ""},
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
#endif

/* individual face extrude */
/* will use vertex normals for extrusion directions, so *nor is unaffected */
short EDBM_Extrude_face_indiv(BMEditMesh *em, wmOperator *op, short flag, float *nor) 
{
	BMOIter siter;
	BMIter liter;
	BMFace *f;
	BMLoop *l;
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "extrude_face_indiv faces=%hf", flag);

	/*deselect original verts*/
	EDBM_clear_flag_all(em, BM_SELECT);

	BMO_Exec_Op(em->bm, &bmop);
	
	BMO_ITER(f, &siter, em->bm, &bmop, "faceout", BM_FACE) {
		BM_Select(em->bm, f, 1);

		/*set face vertex normals to face normal*/
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
			VECCOPY(l->v->no, f->no);
		}
	}

	if (!EDBM_FinishOp(em, &bmop, op, 1)) return 0;

	return 's'; // s is shrink/fatten
}

#if 0
short EDBM_Extrude_face_indiv(BMEditMesh *em, wmOperator *op, short flag, float *nor) 
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
}
#endif

/* extrudes individual edges */
short EDBM_Extrude_edges_indiv(BMEditMesh *em, wmOperator *op, short flag, float *nor) 
{
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "extrude_edge_only edges=%he", flag);

	/*deselect original verts*/
	EDBM_clear_flag_all(em, BM_SELECT);

	BMO_Exec_Op(em->bm, &bmop);
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "geomout", BM_SELECT, BM_VERT|BM_EDGE);

	if (!EDBM_FinishOp(em, &bmop, op, 1)) return 0;

	return 'n'; // n is normal grab
}

#if 0
/* nor is filled with constraint vector */
short EDBM_Extrude_edges_indiv(BMEditMesh *em, short flag, float *nor) 
{
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
	return 'n';  // n is for normal constraint
}
#endif

/* extrudes individual vertices */
short EDBM_Extrude_verts_indiv(BMEditMesh *em, wmOperator *op, short flag, float *nor) 
{
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "extrude_vert_indiv verts=%hv", flag);

	/*deselect original verts*/
	BMO_UnHeaderFlag_Buffer(em->bm, &bmop, "verts", BM_SELECT, BM_VERT);

	BMO_Exec_Op(em->bm, &bmop);
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "vertout", BM_SELECT, BM_VERT);

	if (!EDBM_FinishOp(em, &bmop, op, 1)) return 0;

	return 'g'; // g is grab
}

short EDBM_Extrude_edge(Object *obedit, BMEditMesh *em, int flag, float *nor)
{
	BMesh *bm = em->bm;
	BMIter iter;
	BMOIter siter;
	BMOperator extop;
	BMVert *vert;
	BMEdge *edge;
	BMFace *f;
	ModifierData *md;
	BMHeader *el;
	
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

	nor[0] = nor[1] = nor[2] = 0.0f;
	
	BMO_ITER(el, &siter, bm, &extop, "geomout", BM_ALL) {
		BM_Select(bm, el, 1);

		if (el->type == BM_FACE) {
			f = (BMFace*)el;
			add_normal_aligned(nor, f->no);
		};
	}

	Normalize(nor);

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

}

static int extrude_repeat_mesh(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
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
		EDBM_Extrude_edge(obedit, em, BM_SELECT, nor);
		//BMO_CallOpf(em->bm, "extrudefaceregion edgefacein=%hef", BM_SELECT);
		BMO_CallOpf(em->bm, "translate vec=%v verts=%hv", (float*)dvec, BM_SELECT);
		//extrudeflag(obedit, em, SELECT, nor);
		//translateflag(em, SELECT, dvec);
	}
	
	EDBM_RecalcNormals(em);

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
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
int EDBM_Extrude_Mesh(Object *obedit, BMEditMesh *em, wmOperator *op, float *norin)
{
	Scene *scene= NULL;		// XXX CTX!
	short nr, transmode= 0;
	float stacknor[3] = {0.0f, 0.0f, 0.0f};
	float *nor = norin ? norin : stacknor;

	nor[0] = nor[1] = nor[2] = 0.0f;

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

	if(nr<1) return 'g';

	if(nr==1 && em->selectmode & SCE_SELECT_VERTEX)	
		transmode= EDBM_Extrude_vert(obedit, em, SELECT, nor);
	else if (nr == 1) transmode= EDBM_Extrude_edge(obedit, em, SELECT, nor);
	else if(nr==4) transmode= EDBM_Extrude_verts_indiv(em, op, SELECT, nor);
	else if(nr==3) transmode= EDBM_Extrude_edges_indiv(em, op, SELECT, nor);
	else transmode= EDBM_Extrude_face_indiv(em, op, SELECT, nor);
	
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
	
	return transmode;
}

/* extrude without transform */
static int mesh_extrude_region_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	
	EDBM_Extrude_Mesh(obedit, em, op, NULL);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

static int mesh_extrude_region_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	float nor[3];
	int constraint_axis[3] = {0, 0, 1};
	int tmode;

	tmode = EDBM_Extrude_edge(obedit, em, BM_SELECT, nor);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	RNA_enum_set(op->ptr, "proportional", 0);
	RNA_boolean_set(op->ptr, "mirror", 0);

	if (tmode == 'n') {
		RNA_enum_set(op->ptr, "constraint_orientation", V3D_MANIP_NORMAL);
		RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
	}
	WM_operator_name_call(C, "TFM_OT_translate", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_region(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Extrude Region";
	ot->idname= "MESH_OT_extrude_region";
	
	/* api callbacks */
	ot->invoke= mesh_extrude_region_invoke;
	ot->exec= mesh_extrude_region_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* to give to transform */
	Properties_Proportional(ot);
	Properties_Constraints(ot);
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

static int mesh_extrude_verts_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	float nor[3];

	EDBM_Extrude_verts_indiv(em, op, BM_SELECT, nor);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

static int mesh_extrude_verts_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	float nor[3];
	int constraint_axis[3] = {0, 0, 1};
	int tmode;

	tmode = EDBM_Extrude_verts_indiv(em, op, BM_SELECT, nor);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	RNA_enum_set(op->ptr, "proportional", 0);
	RNA_boolean_set(op->ptr, "mirror", 0);

	if (tmode == 'n') {
		RNA_enum_set(op->ptr, "constraint_orientation", V3D_MANIP_NORMAL);
		RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
	}
	WM_operator_name_call(C, "TFM_OT_translate", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_verts_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Extrude Only Vertices";
	ot->idname= "MESH_OT_extrude_verts_indiv";
	
	/* api callbacks */
	ot->invoke= mesh_extrude_verts_invoke;
	ot->exec= mesh_extrude_verts_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* to give to transform */
	Properties_Proportional(ot);
	Properties_Constraints(ot);
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

static int mesh_extrude_edges_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	float nor[3];

	EDBM_Extrude_edges_indiv(em, op, BM_SELECT, nor);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

static int mesh_extrude_edges_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	float nor[3];
	int constraint_axis[3] = {0, 0, 1};
	int tmode;

	tmode = EDBM_Extrude_edges_indiv(em, op, BM_SELECT, nor);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	RNA_enum_set(op->ptr, "proportional", 0);
	RNA_boolean_set(op->ptr, "mirror", 0);

	/*if (tmode == 'n') {
		RNA_enum_set(op->ptr, "constraint_orientation", V3D_MANIP_NORMAL);
		RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
	}*/
	WM_operator_name_call(C, "TFM_OT_translate", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_edges_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Extrude Only Edges";
	ot->idname= "MESH_OT_extrude_edges_indiv";
	
	/* api callbacks */
	ot->invoke= mesh_extrude_edges_invoke;
	ot->exec= mesh_extrude_edges_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* to give to transform */
	Properties_Proportional(ot);
	Properties_Constraints(ot);
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

static int mesh_extrude_faces_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	float nor[3];

	EDBM_Extrude_face_indiv(em, op, BM_SELECT, nor);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

static int mesh_extrude_faces_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	float nor[3];
	int constraint_axis[3] = {0, 0, 1};
	int tmode;

	tmode = EDBM_Extrude_face_indiv(em, op, BM_SELECT, nor);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	RNA_enum_set(op->ptr, "proportional", 0);
	RNA_boolean_set(op->ptr, "mirror", 0);
	
	if (tmode == 's') {
		WM_operator_name_call(C, "TFM_OT_shrink_fatten", WM_OP_INVOKE_REGION_WIN, op->ptr);
	} else {
		if (tmode == 'n') {
			RNA_enum_set(op->ptr, "constraint_orientation", V3D_MANIP_NORMAL);
			RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
		}
		WM_operator_name_call(C, "TFM_OT_translate", WM_OP_INVOKE_REGION_WIN, op->ptr);
	}
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_faces_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Extrude Individual Faces";
	ot->idname= "MESH_OT_extrude_faces_indiv";
	
	/* api callbacks */
	ot->invoke= mesh_extrude_faces_invoke;
	ot->exec= mesh_extrude_faces_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* to give to transform */
	Properties_Proportional(ot);
	Properties_Constraints(ot);
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

int extrude_menu_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	uiPopupMenu *pup;
	uiLayout *layout;

	if(em->selectmode & SCE_SELECT_VERTEX) {
		if(em->bm->totvertsel==0) {
			return OPERATOR_CANCELLED;
		} else if(em->bm->totvertsel==1) {
			WM_operator_name_call(C, "MESH_OT_extrude_verts_indiv", WM_OP_INVOKE_REGION_WIN, op->ptr);
		} else if(em->bm->totedgesel==0) {
			WM_operator_name_call(C, "MESH_OT_extrude_verts_indiv", WM_OP_INVOKE_REGION_WIN, op->ptr);
		} else if(em->bm->totfacesel==0) {
			// pupmenu("Extrude %t|Only Edges%x3|Only Vertices%x4");
			pup= uiPupMenuBegin(C, "Extrude", 0);
			layout= uiPupMenuLayout(pup);
			uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
			
			uiItemO(layout, "Only Edges", 0, "MESH_OT_extrude_edges_indiv");
			uiItemO(layout, "Only Verts", 0, "MESH_OT_extrude_verts_indiv");
			
			uiPupMenuEnd(C, pup);
		} else if(em->bm->totfacesel==1) {
			// pupmenu("Extrude %t|Region %x1|Only Edges%x3|Only Vertices%x4");
			pup= uiPupMenuBegin(C, "Extrude", 0);
			layout= uiPupMenuLayout(pup);
			uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
			
			uiItemO(layout, "Region", 0, "MESH_OT_extrude_region");
			uiItemO(layout, "Only Edges", 0, "MESH_OT_extrude_edges_indiv");
			uiItemO(layout, "Only Verts", 0, "MESH_OT_extrude_verts_indiv");
			
			uiPupMenuEnd(C, pup);
		} else  {
			// pupmenu("Extrude %t|Region %x1||Individual Faces %x2|Only Edges%x3|Only Vertices%x4");
			pup= uiPupMenuBegin(C, "Extrude", 0);
			layout= uiPupMenuLayout(pup);
			uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
			
			uiItemO(layout, "Region", 0, "MESH_OT_extrude_region");
			uiItemO(layout, "Individual Faces", 0, "MESH_OT_extrude_faces_indiv");
			uiItemO(layout, "Only Edges", 0, "MESH_OT_extrude_edges_indiv");
			uiItemO(layout, "Only Verts", 0, "MESH_OT_extrude_verts_indiv");
			
			uiPupMenuEnd(C, pup);
		}
	} else if (em->selectmode & SCE_SELECT_EDGE) {
		if (em->bm->totedge==0)
			return OPERATOR_CANCELLED;
		else if (em->bm->totedgesel==1)
			WM_operator_name_call(C, "MESH_OT_extrude_edges_indiv", WM_OP_INVOKE_REGION_WIN, op->ptr);
		else if (em->bm->totfacesel==0) {
			WM_operator_name_call(C, "MESH_OT_extrude_edges_indiv", WM_OP_INVOKE_REGION_WIN, op->ptr);
		} else if (em->bm->totfacesel==1) {
			// pupmenu("Extrude %t|Region %x1|Only Edges%x3");
			pup= uiPupMenuBegin(C, "Extrude", 0);
			layout= uiPupMenuLayout(pup);
			uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
			
			uiItemO(layout, "Region", 0, "MESH_OT_extrude_region");
			uiItemO(layout, "Only Edges", 0, "MESH_OT_extrude_edges_indiv");
			
			uiPupMenuEnd(C, pup);
		} else {
			// pupmenu("Extrude %t|Region %x1||Individual Faces %x2|Only Edges%x3");
			pup= uiPupMenuBegin(C, "Extrude", 0);
			layout= uiPupMenuLayout(pup);
			uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
			
			uiItemO(layout, "Region", 0, "MESH_OT_extrude_region");
			uiItemO(layout, "Individual Faces", 0, "MESH_OT_extrude_faces_indiv");
			uiItemO(layout, "Only Edges", 0, "MESH_OT_extrude_edges_indiv");
			
			uiPupMenuEnd(C, pup);
		}

	} else if (em->selectmode & SCE_SELECT_FACE) {
		if (em->bm->totfacesel==0)
			return OPERATOR_CANCELLED;
		else if (em->bm->totfacesel==1)
			WM_operator_name_call(C, "MESH_OT_extrude_region", WM_OP_INVOKE_REGION_WIN, op->ptr);
		else {
			// pupmenu("Extrude %t|Region %x1||Individual Faces %x2");
			pup= uiPupMenuBegin(C, "Extrude", 0);
			layout= uiPupMenuLayout(pup);
			uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
			
			uiItemO(layout, "Region", 0, "MESH_OT_extrude_region");
			uiItemO(layout, "Individual Faces", 0, "MESH_OT_extrude_faces_indiv");
			
			uiPupMenuEnd(C, pup);
		}
	}

	return OPERATOR_CANCELLED;
}

void MESH_OT_extrude(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Extrude";
	ot->idname= "MESH_OT_extrude";
	
	/* api callbacks */
	ot->invoke= extrude_menu_invoke;
	ot->poll= ED_operator_editmesh;
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
	ot->name= "Select/Deselect All";
	ot->idname= "MESH_OT_select_all_toggle";
	
	/* api callbacks */
	ot->exec= toggle_select_all_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *************** add-click-mesh (extrude) operator ************** */

static int dupli_extrude_cursor(bContext *C, wmOperator *op, wmEvent *event)
{
	ViewContext vc;
	BMVert *v1;
	BMIter iter;
	float min[3], max[3];
	int done= 0;
	
	em_setup_viewcontext(C, &vc);
	
	INIT_MINMAX(min, max);
	
	BM_ITER_SELECT(v1, &iter, vc.em->bm, BM_VERTS_OF_MESH, NULL)
		DO_MINMAX(v1->co, min, max);
		done= 1;
	}

	/* call extrude? */
	if(done) {
		BMEdge *eed;
		float vec[3], cent[3], mat[3][3];
		float nor[3]= {0.0, 0.0, 0.0};
		
		/* check for edges that are half selected, use for rotation */
		done= 0;
		BM_ITER(eed, &iter, vc.em->bm, BM_EDGES_OF_MESH, NULL) {
			if (BM_TestHFlag(eed->v1, BM_SELECT) ^ BM_TestHFlag(eed->v2, BM_SELECT)) {
				if(BM_TestHFlag(eed->v1, BM_SELECT)) 
					VecSubf(vec, eed->v1->co, eed->v2->co);
				else 
					VecSubf(vec, eed->v2->co, eed->v1->co);
				VecAddf(nor, nor, vec);
				done= 1;
			}
		}
		if(done) Normalize(nor);
		
		/* center */
		VecAddf(cent, min, max);
		VecMulf(cent, 0.5f);
		VECCOPY(min, cent);
		
		Mat4MulVecfl(vc.obedit->obmat, min);	// view space
		view3d_get_view_aligned_coordinate(&vc, min, event->mval);
		Mat4Invert(vc.obedit->imat, vc.obedit->obmat); 
		Mat4MulVecfl(vc.obedit->imat, min); // back in object space
		
		VecSubf(min, min, cent);
		
		/* calculate rotation */
		Mat3One(mat);
		if(done) {
			float dot;
			
			VECCOPY(vec, min);
			Normalize(vec);
			dot= INPR(vec, nor);

			if( fabs(dot)<0.999) {
				float cross[3], si, q1[4];
				
				Crossf(cross, nor, vec);
				Normalize(cross);
				dot= 0.5f*saacos(dot);
				si= (float)sin(dot);
				q1[0]= (float)cos(dot);
				q1[1]= cross[0]*si;
				q1[2]= cross[1]*si;
				q1[3]= cross[2]*si;
				
				QuatToMat3(q1, mat);
			}
		}
		

		EDBM_Extrude_edge(vc.obedit, vc.em, SELECT, nor);
		EDBM_CallOpf(vc.em, op, "rotate verts=%hv cent=%v mat=%m3",
			BM_SELECT, cent, mat);
		EDBM_CallOpf(vc.em, op, "translate verts=%hv vec=%v",
			BM_SELECT, min);
	}
	else {
		float *curs= give_cursor(vc.scene, vc.v3d);
		BMOperator bmop;
		BMOIter oiter;
		
		VECCOPY(min, curs);

		view3d_get_view_aligned_coordinate(&vc, min, event->mval);
		Mat4Invert(vc.obedit->imat, vc.obedit->obmat); 
		Mat4MulVecfl(vc.obedit->imat, min); // back in object space
		
		EDBM_InitOpf(vc.em, &bmop, op, "makevert co=%v", min);
		BMO_Exec_Op(vc.em->bm, &bmop);

		BMO_ITER(v1, &oiter, vc.em->bm, &bmop, "newvertout", BM_VERT) {
			BM_Select(vc.em->bm, v1, 1);
		}

		if (!EDBM_FinishOp(vc.em, &bmop, op, 1))
			return OPERATOR_CANCELLED;
	}

	//retopo_do_all();
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, vc.obedit); 
	DAG_object_flush_update(vc.scene, vc.obedit, OB_RECALC_DATA);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_dupli_extrude_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate or Extrude at 3D Cursor";
	ot->idname= "MESH_OT_dupli_extrude_cursor";
	
	/* api callbacks */
	ot->invoke= dupli_extrude_cursor;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int delete_mesh(Object *obedit, wmOperator *op, int event, Scene *scene)
{
	BMEditMesh *bem = ((Mesh*)obedit->data)->edit_btmesh;
	
	if(event<1) return OPERATOR_CANCELLED;

	if(event==10 ) {
		//"Erase Vertices";

		if (!EDBM_CallOpf(bem, op, "del geom=%hv context=%i", BM_SELECT, DEL_VERTS))
			return OPERATOR_CANCELLED;
	} 
	else if(event==11) {
		//"Edge Loop"
		if (!EDBM_CallOpf(bem, op, "dissolveedgeloop edges=%he", BM_SELECT))
			return OPERATOR_CANCELLED;
	}
	else if(event==7) {
		//"Dissolve"
		if (bem->selectmode & SCE_SELECT_FACE) {
			if (!EDBM_CallOpf(bem, op, "dissolvefaces faces=%hf",BM_SELECT))
				return OPERATOR_CANCELLED;
		} else if (bem->selectmode & SCE_SELECT_EDGE) {
			if (!EDBM_CallOpf(bem, op, "dissolveedges edges=%he",BM_SELECT))
				return OPERATOR_CANCELLED;
		} else if (bem->selectmode & SCE_SELECT_VERTEX) {
			if (!EDBM_CallOpf(bem, op, "dissolveverts verts=%hv",BM_SELECT))
				return OPERATOR_CANCELLED;
		}
	}
	else if(event==4) {
		//Edges and Faces
		if (!EDBM_CallOpf(bem, op, "del geom=%hef context=%i", BM_SELECT, DEL_EDGESFACES))
			return OPERATOR_CANCELLED;
	} 
	else if(event==1) {
		//"Erase Edges"
		if (!EDBM_CallOpf(bem, op, "del geom=%he context=%i", BM_SELECT, DEL_EDGES))
			return OPERATOR_CANCELLED;
	}
	else if(event==2) {
		//"Erase Faces";
		if (!EDBM_CallOpf(bem, op, "del geom=%hf context=%i", BM_SELECT, DEL_FACES))
			return OPERATOR_CANCELLED;
	}
	else if(event==5) {
		//"Erase Only Faces";
		if (!EDBM_CallOpf(bem, op, "del geom=%hf context=%d",
		                  BM_SELECT, DEL_ONLYFACES))
			return OPERATOR_CANCELLED;
	}
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

	return OPERATOR_FINISHED;
}

/* Note, these values must match delete_mesh() event values */
static EnumPropertyItem prop_mesh_delete_types[] = {
	{7, "DISSOLVE",         0, "Dissolve", ""},
	{10,"VERT",		0, "Vertices", ""},
	{1, "EDGE",		0, "Edges", ""},
	{2, "FACE",		0, "Faces", ""},
	{11, "EDGE_LOOP", 0, "Edge Loop", ""},
	{4, "EDGE_FACE", 0, "Edges & Faces", ""},
	{5, "ONLY_FACE", 0, "Only Faces", ""},
	{0, NULL, 0, NULL, NULL}
};

static int delete_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);

	delete_mesh(obedit, op, RNA_enum_get(op->ptr, "type"), scene);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_DATA|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete";
	ot->idname= "MESH_OT_delete";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= delete_mesh_exec;
	
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/*props */
	RNA_def_enum(ot->srna, "type", prop_mesh_delete_types, 10, "Type", "Method used for deleting mesh data");
}


static int addedgeface_mesh_exec(bContext *C, wmOperator *op)
{
	BMOperator bmop;
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	
	if (!EDBM_InitOpf(em, &bmop, op, "contextual_create geom=%hfev", BM_SELECT))
		return OPERATOR_CANCELLED;
	
	BMO_Exec_Op(em->bm, &bmop);
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "faceout", BM_SELECT, BM_FACE);

	if (!EDBM_FinishOp(em, &bmop, op, 1))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	DAG_object_flush_update(CTX_data_scene(C), obedit, OB_RECALC_DATA);	
	
	return OPERATOR_FINISHED;
}

void MESH_OT_edge_face_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Edge/Face";
	ot->idname= "MESH_OT_edge_face_add";
	
	/* api callbacks */
	ot->exec= addedgeface_mesh_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}

static EnumPropertyItem prop_mesh_edit_types[] = {
	{1, "VERT", 0, "Vertices", ""},
	{2, "EDGE", 0, "Edges", ""},
	{3, "FACE", 0, "Faces", ""},
	{0, NULL, 0, NULL, NULL}
};

static int mesh_selection_type_exec(bContext *C, wmOperator *op)
{		
	
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	int type = RNA_enum_get(op->ptr,"type");

	switch (type) {
		case 1:
			em->selectmode = SCE_SELECT_VERTEX;
			break;
		case 2:
			em->selectmode = SCE_SELECT_EDGE;
			break;
		case 3:
			em->selectmode = SCE_SELECT_FACE;
			break;
	}

	EDBM_selectmode_set(em);
	CTX_data_scene(C)->toolsettings->selectmode = em->selectmode;

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_selection_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Selection Mode";
	ot->idname= "MESH_OT_selection_type";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= mesh_selection_type_exec;
	
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "type", prop_mesh_edit_types, 0, "Type", "Set the mesh selection type");
	RNA_def_boolean(ot->srna, "inclusive", 0, "Inclusive", "Selects geometry around selected geometry, occording to selection mode");	
}

/* ************************* SEAMS AND EDGES **************** */

static int editbmesh_mark_seam(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Mesh *me= ((Mesh *)obedit->data);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMEdge *eed;
	BMIter iter;
	int clear = RNA_boolean_get(op->ptr, "clear");
	
	/* auto-enable seams drawing */
	if(clear==0) {
		me->drawflag |= ME_DRAWSEAMS;
	}

	if(clear) {
		BM_ITER_SELECT(eed, &iter, bm, BM_EDGES_OF_MESH, NULL)
			BM_ClearHFlag(eed, BM_SEAM);
		}
	}
	else {
		BM_ITER_SELECT(eed, &iter, bm, BM_EDGES_OF_MESH, NULL)
			BM_SetHFlag(eed, BM_SEAM);
		}
	}

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_seam(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mark Seam";
	ot->idname= "MESH_OT_mark_seam";
	
	/* api callbacks */
	ot->exec= editbmesh_mark_seam;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
}

static int editbmesh_mark_sharp(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Mesh *me= ((Mesh *)obedit->data);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMEdge *eed;
	BMIter iter;
	int clear = RNA_boolean_get(op->ptr, "clear");

	/* auto-enable sharp edge drawing */
	if(clear == 0) {
		me->drawflag |= ME_DRAWSHARP;
	}

	if(!clear) {
		BM_ITER_SELECT(eed, &iter, bm, BM_EDGES_OF_MESH, NULL)
			BM_SetHFlag(eed, BM_SHARP);
		}
	} else {
		BM_ITER_SELECT(eed, &iter, bm, BM_EDGES_OF_MESH, NULL)
			BM_ClearHFlag(eed, BM_SHARP);
		}
	}


	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_sharp(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mark Sharp";
	ot->idname= "MESH_OT_mark_sharp";
	
	/* api callbacks */
	ot->exec= editbmesh_mark_sharp;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
}


static int editbmesh_vert_connect(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Mesh *me= ((Mesh *)obedit->data);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMOperator bmop;
	int len = 0;
	
	BMO_InitOpf(bm, &bmop, "connectverts verts=%hv", BM_SELECT);
	BMO_Exec_Op(bm, &bmop);
	len = BMO_GetSlot(&bmop, "edgeout")->len;
	BMO_Finish_Op(bm, &bmop);
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return len ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void MESH_OT_vert_connect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Vertex Connect";
	ot->idname= "MESH_OT_vert_connect";
	
	/* api callbacks */
	ot->exec= editbmesh_vert_connect;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int editbmesh_edge_split(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Mesh *me= ((Mesh *)obedit->data);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMOperator bmop;
	int len = 0;
	
	BMO_InitOpf(bm, &bmop, "edgesplit edges=%he numcuts=%d", BM_SELECT, RNA_int_get(op->ptr,"number_cuts"));
	BMO_Exec_Op(bm, &bmop);
	len = BMO_GetSlot(&bmop, "outsplit")->len;
	BMO_Finish_Op(bm, &bmop);
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return len ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void MESH_OT_edge_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Edge Split";
	ot->idname= "MESH_OT_edge_split";
	
	/* api callbacks */
	ot->exec= editbmesh_edge_split;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "number_cuts", 1, 1, 10, "Number of Cuts", "", 1, INT_MAX);
}

/****************** add duplicate operator ***************/

static int mesh_duplicate_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)ob->data)->edit_btmesh;
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "dupe geom=%hvef", BM_SELECT);
	
	BMO_Exec_Op(em->bm, &bmop);
	EDBM_clear_flag_all(em, BM_SELECT);

	BMO_HeaderFlag_Buffer(em->bm, &bmop, "newout", BM_SELECT, BM_ALL);

	if (!EDBM_FinishOp(em, &bmop, op, 1))
		return OPERATOR_CANCELLED;

	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

static int mesh_duplicate_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	WM_cursor_wait(1);
	mesh_duplicate_exec(C, op);
	WM_cursor_wait(0);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate";
	ot->idname= "MESH_OT_duplicate";
	
	/* api callbacks */
	ot->invoke= mesh_duplicate_invoke;
	ot->exec= mesh_duplicate_exec;
	
	ot->poll= ED_operator_editmesh;
	
	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

static int flip_normals(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= (((Mesh *)obedit->data))->edit_btmesh;
	
	if (!EDBM_CallOpf(em, op, "reversefaces facaes=%hf", BM_SELECT))
		return OPERATOR_CANCELLED;
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_flip_normals(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Flip Normals";
	ot->idname= "MESH_OT_flip_normals";
	
	/* api callbacks */
	ot->exec= flip_normals;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

#define DIRECTION_CW	1
#define DIRECTION_CCW	2

static const EnumPropertyItem direction_items[]= {
	{DIRECTION_CW, "CW", 0, "Clockwise", ""},
	{DIRECTION_CCW, "CCW", 0, "Counter Clockwise", ""},
	{0, NULL, 0, NULL, NULL}};

/* only accepts 1 selected edge, or 2 selected faces */
static int edge_rotate_selected(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	BMOperator bmop;
	BMOIter siter;
	BMEdge *eed;
	BMIter iter;
	int ccw = RNA_int_get(op->ptr, "direction") == 1; // direction == 2 when clockwise and ==1 for counter CW.
	short edgeCount = 0;
	
	if (!(em->bm->totfacesel == 2 || em->bm->totedgesel == 1)) {
		BKE_report(op->reports, RPT_ERROR, "Select one edge or two adjacent faces");
		return OPERATOR_CANCELLED;
	}

	/*first see if we have two adjacent faces*/
	BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (BM_Edge_FaceCount(eed) == 2) {
			if ((BM_TestHFlag(eed->loop->f, BM_SELECT) && BM_TestHFlag(((BMLoop*)eed->loop->radial.next->data)->f, BM_SELECT))
			     && !(BM_TestHFlag(eed->loop->f, BM_HIDDEN) || BM_TestHFlag(((BMLoop*)eed->loop->radial.next->data)->f, BM_HIDDEN)))
			{
				break;
			}
		}
	}
	
	/*ok, we don't have two adjacent faces, but we do have two selected ones.
	  that's an error condition.*/
	if (!eed && em->bm->totfacesel == 2) {
		BKE_report(op->reports, RPT_ERROR, "Select one edge or two adjacent faces");
		return OPERATOR_CANCELLED;
	}

	if (!eed) {
		BM_ITER_SELECT(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL)
			if (BM_TestHFlag(eed, BM_SELECT))
				break;
		}
	}

	/*this should never happen*/
	if (!eed)
		return OPERATOR_CANCELLED;
	
	EDBM_InitOpf(em, &bmop, op, "edgerotate edges=%e ccw=%d", eed, ccw);
	BMO_Exec_Op(em->bm, &bmop);

	BMO_HeaderFlag_Buffer(em->bm, &bmop, "edgeout", BM_SELECT, BM_EDGE);

	if (!EDBM_FinishOp(em, &bmop, op, 1))
		return OPERATOR_CANCELLED;

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_edge_rotate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Rotate Selected Edge";
	ot->idname= "MESH_OT_edge_rotate";

	/* api callbacks */
	ot->exec= edge_rotate_selected;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items, DIRECTION_CW, "direction", "direction to rotate edge around.");
}

/* swap is 0 or 1, if 1 it hides not selected */
void EDBM_hide_mesh(BMEditMesh *em, int swap)
{
	BMIter iter;
	BMHeader *h;
	int itermode;

	if(em==NULL) return;
	
	if (em->selectmode & SCE_SELECT_VERTEX)
		itermode = BM_VERTS_OF_MESH;
	else if (em->selectmode & SCE_SELECT_EDGE)
		itermode = BM_EDGES_OF_MESH;
	else
		itermode = BM_FACES_OF_MESH;

	BM_ITER(h, &iter, em->bm, itermode, NULL) {
		if (BM_TestHFlag(h, BM_SELECT) ^ swap)
			BM_Hide(em->bm, h, 1);
	}

	/*original hide flushing comment (OUTDATED): 
	  hide happens on least dominant select mode, and flushes up, not down! (helps preventing errors in subsurf) */
	/*  - vertex hidden, always means edge is hidden too
		- edge hidden, always means face is hidden too
		- face hidden, only set face hide
		- then only flush back down what's absolute hidden
	*/

}

static int hide_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= (((Mesh *)obedit->data))->edit_btmesh;
	
	EDBM_hide_mesh(em, RNA_boolean_get(op->ptr, "unselected"));
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);	

	return OPERATOR_FINISHED;	
}

void MESH_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide Selection";
	ot->idname= "MESH_OT_hide";
	
	/* api callbacks */
	ot->exec= hide_mesh_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected.");
}


void EDBM_reveal_mesh(BMEditMesh *em)
{
	BMIter iter;
	BMHeader *ele;
	int i, types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};
	int sels[3] = {1, !(em->selectmode & SCE_SELECT_VERTEX), !(em->selectmode & SCE_SELECT_VERTEX | SCE_SELECT_EDGE)};

	for (i=0; i<3; i++) {
		BM_ITER(ele, &iter, em->bm, types[i], NULL) {
			if (BM_TestHFlag(ele, BM_HIDDEN)) {
				BM_Hide(em->bm, ele, 0);

				if (sels[i])
					BM_Select(em->bm, ele, 1);
			}
		}
	}

	EDBM_selectmode_flush(em);
}

static int reveal_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= (((Mesh *)obedit->data))->edit_btmesh;
	
	EDBM_reveal_mesh(em);

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);	

	return OPERATOR_FINISHED;	
}

void MESH_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reveal Hidden";
	ot->idname= "MESH_OT_reveal";
	
	/* api callbacks */
	ot->exec= reveal_mesh_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int normals_make_consistent_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	
	if (!EDBM_CallOpf(em, op, "righthandfaces faces=%hf", BM_SELECT))
		return OPERATOR_CANCELLED;
	
	if (RNA_boolean_get(op->ptr, "inside"))
		EDBM_CallOpf(em, op, "reversefaces faces=%hf", BM_SELECT);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit); //TODO is this needed ?

	return OPERATOR_FINISHED;	
}

void MESH_OT_normals_make_consistent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Normals Consistent";
	ot->idname= "MESH_OT_normals_make_consistent";
	
	/* api callbacks */
	ot->exec= normals_make_consistent_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "inside", 0, "Inside", "");
}



static int do_smooth_vertex(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	ModifierData *md;
	int mirrx=0, mirry=0, mirrz=0;
	int i, repeat;

	/* if there is a mirror modifier with clipping, flag the verts that
	 * are within tolerance of the plane(s) of reflection 
	 */
	for(md=obedit->modifiers.first; md; md=md->next) {
		if(md->type==eModifierType_Mirror) {
			MirrorModifierData *mmd = (MirrorModifierData*) md;	
		
			if(mmd->flag & MOD_MIR_CLIPPING) {
				if (mmd->flag & MOD_MIR_AXIS_X)
					mirrx = 1;
				if (mmd->flag & MOD_MIR_AXIS_Y)
					mirry = 1;
				if (mmd->flag & MOD_MIR_AXIS_Z)
					mirrz = 1;
			}
		}
	}

	repeat = RNA_int_get(op->ptr,"repeat");
	if (!repeat)
		repeat = 1;
	
	for (i=0; i<repeat; i++) {
		if (!EDBM_CallOpf(em, op, "vertexsmooth verts=%hv mirror_clip_x=%d mirror_clip_y=%d mirror_clip_z=%d",
				  BM_SELECT, mirrx, mirry, mirrz))
		{
			return OPERATOR_CANCELLED;
		}
	}

	//BMESH_TODO: need to handle the x-axis editing option here properly.
	//should probably make a helper function for that? I dunno.

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit); //TODO is this needed ?

	return OPERATOR_FINISHED;
}	
	
void MESH_OT_vertices_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Smooth Vertex";
	ot->idname= "MESH_OT_vertices_smooth";
	
	/* api callbacks */
	ot->exec= do_smooth_vertex;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "repeat", 1, 1, 100, "Number of times to smooth the mesh", "", 1, INT_MAX);
}


static int bm_test_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
#if 1
	if (!EDBM_CallOpf(em, op, "collapse edges=%he", BM_SELECT))
		return OPERATOR_CANCELLED;

#else //uv island walker test
	BMIter iter, liter;
	BMFace *f;
	BMLoop *l, *l2;
	MLoopUV *luv;
	BMWalker walker;
	int i=0;

	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		}
	}

	BMW_Init(&walker, em->bm, BMW_UVISLAND, 0);

	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			if (luv->flag & MLOOPUV_VERTSEL) {
				l2 = BMW_Begin(&walker, l);
				for (; l2; l2=BMW_Step(&walker)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l2->head.data, CD_MLOOPUV);
					luv->flag |= MLOOPUV_VERTSEL;
				}				
			}
		}
	}

	BMW_End(&walker);
#endif
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit); //TODO is this needed ?

	return OPERATOR_FINISHED;
}	
	
void MESH_OT_bm_test(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "BMesh Test Operator";
	ot->idname= "MESH_OT_bm_test";
	
	/* api callbacks */
	ot->exec= bm_test_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	//RNA_def_int(ot->srna, "repeat", 1, 1, 100, "Number of times to smooth the mesh", "", 1, INT_MAX);
}
