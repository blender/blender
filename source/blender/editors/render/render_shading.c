/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_world.h"

#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_listbase.h"

#include "GPU_material.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_mesh.h"
#include "ED_render.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "render_intern.h"	// own include

/***************************** Updates ***********************************
 * ED_render_id_flush_update gets called from DAG_id_flush_update, to do *
 * editor level updates when the ID changes. when these ID blocks are in *
 * the dependency graph, we can get rid of the manual dependency checks  */

static int mtex_use_tex(MTex **mtex, int tot, Tex *tex)
{
	int a;

	if(!mtex)
		return 0;

	for(a=0; a<tot; a++)
		if(mtex[a] && mtex[a]->tex == tex)
			return 1;
	
	return 0;
}

static int nodes_use_tex(bNodeTree *ntree, Tex *tex)
{
	bNode *node;

	for(node=ntree->nodes.first; node; node= node->next) {
		if(node->id) {
			if(node->id == (ID*)tex) {
				return 1;
			}
			else if(node->type==NODE_GROUP) {
				if(nodes_use_tex((bNodeTree *)node->id, tex))
					return 1;
			}
		}
	}

	return 0;
}

static void material_changed(Main *bmain, Material *ma)
{
	/* icons */
	BKE_icon_changed(BKE_icon_getid(&ma->id));

	/* glsl */
	if(ma->gpumaterial.first)
		GPU_material_free(ma);
}

static void texture_changed(Main *bmain, Tex *tex)
{
	Material *ma;
	Lamp *la;
	World *wo;

	/* icons */
	BKE_icon_changed(BKE_icon_getid(&tex->id));

	/* find materials */
	for(ma=bmain->mat.first; ma; ma=ma->id.next) {
		if(mtex_use_tex(ma->mtex, MAX_MTEX, tex));
		else if(ma->use_nodes && ma->nodetree && nodes_use_tex(ma->nodetree, tex));
		else continue;

		BKE_icon_changed(BKE_icon_getid(&ma->id));

		if(ma->gpumaterial.first)
			GPU_material_free(ma);
	}

	/* find lamps */
	for(la=bmain->lamp.first; la; la=la->id.next) {
		if(mtex_use_tex(la->mtex, MAX_MTEX, tex));
		else continue;

		BKE_icon_changed(BKE_icon_getid(&la->id));
	}

	/* find worlds */
	for(wo=bmain->world.first; wo; wo=wo->id.next) {
		if(mtex_use_tex(wo->mtex, MAX_MTEX, tex));
		else continue;

		BKE_icon_changed(BKE_icon_getid(&wo->id));
	}
}

static void lamp_changed(Main *bmain, Lamp *la)
{
	Object *ob;
	Material *ma;

	/* icons */
	BKE_icon_changed(BKE_icon_getid(&la->id));

	/* glsl */
	for(ob=bmain->object.first; ob; ob=ob->id.next)
		if(ob->data == la && ob->gpulamp.first)
			GPU_lamp_free(ob);

	for(ma=bmain->mat.first; ma; ma=ma->id.next)
		if(ma->gpumaterial.first)
			GPU_material_free(ma);
}

static void world_changed(Main *bmain, World *wo)
{
	Material *ma;

	/* icons */
	BKE_icon_changed(BKE_icon_getid(&wo->id));

	/* glsl */
	for(ma=bmain->mat.first; ma; ma=ma->id.next)
		if(ma->gpumaterial.first)
			GPU_material_free(ma);
}

void ED_render_id_flush_update(Main *bmain, ID *id)
{
	switch(GS(id->name)) {
		case ID_MA:
			material_changed(bmain, (Material*)id);
			break;
		case ID_TE:
			texture_changed(bmain, (Tex*)id);
			break;
		case ID_WO:
			world_changed(bmain, (World*)id);
			break;
		case ID_LA:
			lamp_changed(bmain, (Lamp*)id);
			break;
		default:
			break;
	}
}

/********************** material slot operators *********************/

static int material_slot_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ob)
		return OPERATOR_CANCELLED;

	object_add_material_slot(ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Material Slot";
	ot->idname= "OBJECT_OT_material_slot_add";
	ot->description="Add a new material slot or duplicate the selected one.";
	
	/* api callbacks */
	ot->exec= material_slot_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ob)
		return OPERATOR_CANCELLED;

	object_remove_material_slot(ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Material Slot";
	ot->idname= "OBJECT_OT_material_slot_remove";
	ot->description="Remove the selected material slot.";
	
	/* api callbacks */
	ot->exec= material_slot_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_assign_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ob)
		return OPERATOR_CANCELLED;

	if(ob && ob->actcol>0) {
		if(ob->type == OB_MESH) {
			EditMesh *em= ((Mesh*)ob->data)->edit_mesh;
			EditFace *efa;

			if(em) {
				for(efa= em->faces.first; efa; efa=efa->next)
					if(efa->f & SELECT)
						efa->mat_nr= ob->actcol-1;
			}
		}
		else if(ELEM(ob->type, OB_CURVE, OB_SURF)) {
			ListBase *editnurb= ((Curve*)ob->data)->editnurb;
			Nurb *nu;

			if(editnurb) {
				for(nu= editnurb->first; nu; nu= nu->next)
					if(isNurbsel(nu))
						nu->mat_nr= nu->charidx= ob->actcol-1;
			}
		}
		else if(ob->type == OB_FONT) {
			EditFont *ef= ((Curve*)ob->data)->editfont;
    		int i, selstart, selend;

			if(ef && BKE_font_getselection(ob, &selstart, &selend)) {
				for(i=selstart; i<=selend; i++)
					ef->textbufinfo[i].mat_nr = ob->actcol-1;
			}
		}
	}

    DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
    WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_assign(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Assign Material Slot";
	ot->idname= "OBJECT_OT_material_slot_assign";
	ot->description="Assign the material in the selected material slot to the selected vertices.";
	
	/* api callbacks */
	ot->exec= material_slot_assign_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_de_select(bContext *C, int select)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ob)
		return OPERATOR_CANCELLED;

	if(ob->type == OB_MESH) {
		EditMesh *em= ((Mesh*)ob->data)->edit_mesh;

		if(em) {
			if(select)
				EM_select_by_material(em, ob->actcol-1);
			else
				EM_deselect_by_material(em, ob->actcol-1);
		}
	}
	else if ELEM(ob->type, OB_CURVE, OB_SURF) {
		ListBase *editnurb= ((Curve*)ob->data)->editnurb;
		Nurb *nu;
		BPoint *bp;
		BezTriple *bezt;
		int a;

		for(nu= editnurb->first; nu; nu=nu->next) {
			if(nu->mat_nr==ob->actcol-1) {
				if(nu->bezt) {
					a= nu->pntsu;
					bezt= nu->bezt;
					while(a--) {
						if(bezt->hide==0) {
							if(select) {
								bezt->f1 |= SELECT;
								bezt->f2 |= SELECT;
								bezt->f3 |= SELECT;
							}
							else {
								bezt->f1 &= ~SELECT;
								bezt->f2 &= ~SELECT;
								bezt->f3 &= ~SELECT;
							}
						}
						bezt++;
					}
				}
				else if(nu->bp) {
					a= nu->pntsu*nu->pntsv;
					bp= nu->bp;
					while(a--) {
						if(bp->hide==0) {
							if(select) bp->f1 |= SELECT;
							else bp->f1 &= ~SELECT;
						}
						bp++;
					}
				}
			}
		}
	}

    WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);

	return OPERATOR_FINISHED;
}

static int material_slot_select_exec(bContext *C, wmOperator *op)
{
	return material_slot_de_select(C, 1);
}

void OBJECT_OT_material_slot_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Material Slot";
	ot->idname= "OBJECT_OT_material_slot_select";
	ot->description="Select vertices assigned to the selected material slot.";
	
	/* api callbacks */
	ot->exec= material_slot_select_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_deselect_exec(bContext *C, wmOperator *op)
{
	return material_slot_de_select(C, 0);
}

void OBJECT_OT_material_slot_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Deselect Material Slot";
	ot->idname= "OBJECT_OT_material_slot_deselect";
	ot->description="Deselect vertices assigned to the selected material slot.";
	
	/* api callbacks */
	ot->exec= material_slot_deselect_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int material_slot_copy_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Material ***matar;

	if(!ob || !(matar= give_matarar(ob)))
		return OPERATOR_CANCELLED;

	CTX_DATA_BEGIN(C, Object*, ob_iter, selected_editable_objects) {
		if(ob != ob_iter && give_matarar(ob_iter)) {
			if (ob->data != ob_iter->data)
				assign_matarar(ob_iter, matar, ob->totcol);
			
			if(ob_iter->totcol==ob->totcol) {
				ob_iter->actcol= ob->actcol;
				WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob_iter);
			}
		}
	}
	CTX_DATA_END;

	return OPERATOR_FINISHED;
}


void OBJECT_OT_material_slot_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Material to Others";
	ot->idname= "OBJECT_OT_material_slot_copy";
	ot->description="Copies materials to other selected objects.";

	/* api callbacks */
	ot->exec= material_slot_copy_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new material operator *********************/

static int new_material_exec(bContext *C, wmOperator *op)
{
	Material *ma= CTX_data_pointer_get_type(C, "material", &RNA_Material).data;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	/* add or copy material */
	if(ma)
		ma= copy_material(ma);
	else
		ma= add_material("Material");

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if(prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		ma->id.us--;

		RNA_id_pointer_create(&ma->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}

	WM_event_add_notifier(C, NC_MATERIAL|NA_ADDED, ma);
	
	return OPERATOR_FINISHED;
}

void MATERIAL_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Material";
	ot->idname= "MATERIAL_OT_new";
	ot->description="Add a new material.";
	
	/* api callbacks */
	ot->exec= new_material_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new texture operator *********************/

static int new_texture_exec(bContext *C, wmOperator *op)
{
	Tex *tex= CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	/* add or copy texture */
	if(tex)
		tex= copy_texture(tex);
	else
		tex= add_texture("Texture");

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if(prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		tex->id.us--;

		RNA_id_pointer_create(&tex->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}

	WM_event_add_notifier(C, NC_TEXTURE|NA_ADDED, tex);
	
	return OPERATOR_FINISHED;
}

void TEXTURE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Texture";
	ot->idname= "TEXTURE_OT_new";
	ot->description="Add a new texture.";
	
	/* api callbacks */
	ot->exec= new_texture_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new world operator *********************/

static int new_world_exec(bContext *C, wmOperator *op)
{
	World *wo= CTX_data_pointer_get_type(C, "world", &RNA_World).data;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	/* add or copy world */
	if(wo)
		wo= copy_world(wo);
	else
		wo= add_world("World");

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if(prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		wo->id.us--;

		RNA_id_pointer_create(&wo->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}

	WM_event_add_notifier(C, NC_WORLD|NA_ADDED, wo);
	
	return OPERATOR_FINISHED;
}

void WORLD_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New World";
	ot->idname= "WORLD_OT_new";
	ot->description= "Add a new world.";
	
	/* api callbacks */
	ot->exec= new_world_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** render layer operators *********************/

static int render_layer_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);

	scene_add_render_layer(scene);
	scene->r.actlay= BLI_countlist(&scene->r.layers) - 1;

	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS, scene);
	
	return OPERATOR_FINISHED;
}

void SCENE_OT_render_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Render Layer";
	ot->idname= "SCENE_OT_render_layer_add";
	ot->description="Add a render layer.";
	
	/* api callbacks */
	ot->exec= render_layer_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int render_layer_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SceneRenderLayer *rl;
	int act= scene->r.actlay;

	if(BLI_countlist(&scene->r.layers) <= 1)
		return OPERATOR_CANCELLED;
	
	rl= BLI_findlink(&scene->r.layers, scene->r.actlay);
	BLI_remlink(&scene->r.layers, rl);
	MEM_freeN(rl);

	scene->r.actlay= 0;
	
	if(scene->nodetree) {
		bNode *node;
		for(node= scene->nodetree->nodes.first; node; node= node->next) {
			if(node->type==CMP_NODE_R_LAYERS && node->id==NULL) {
				if(node->custom1==act)
					node->custom1= 0;
				else if(node->custom1>act)
					node->custom1--;
			}
		}
	}

	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS, scene);
	
	return OPERATOR_FINISHED;
}

void SCENE_OT_render_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Render Layer";
	ot->idname= "SCENE_OT_render_layer_remove";
	ot->description="Remove the selected render layer.";
	
	/* api callbacks */
	ot->exec= render_layer_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int texture_slot_move(bContext *C, wmOperator *op)
{
	ID *id= CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).id.data;

	if(id) {
		MTex **mtex_ar, *mtexswap;
		short act;
		int type= RNA_enum_get(op->ptr, "type");

		give_active_mtex(id, &mtex_ar, &act);

		if(type == -1) { /* Up */
			if(act > 0) {
				mtexswap = mtex_ar[act];
				mtex_ar[act] = mtex_ar[act-1];
				mtex_ar[act-1] = mtexswap;

				if(GS(id->name)==ID_MA) {
					Material *ma= (Material *)id;
					int mtexuse = ma->septex & (1<<act);
					ma->septex &= ~(1<<act);
					ma->septex |= (ma->septex & (1<<(act-1))) << 1;
					ma->septex &= ~(1<<(act-1));
					ma->septex |= mtexuse >> 1;
				}

				set_active_mtex(id, act-1);
			}
		}
		else { /* Down */
			if(act < MAX_MTEX-1) {
				mtexswap = mtex_ar[act];
				mtex_ar[act] = mtex_ar[act+1];
				mtex_ar[act+1] = mtexswap;

				if(GS(id->name)==ID_MA) {
					Material *ma= (Material *)id;
					int mtexuse = ma->septex & (1<<act);
					ma->septex &= ~(1<<act);
					ma->septex |= (ma->septex & (1<<(act+1))) >> 1;
					ma->septex &= ~(1<<(act+1));
					ma->septex |= mtexuse << 1;
				}

				set_active_mtex(id, act+1);
			}
		}

		WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));
	}

	return OPERATOR_FINISHED;
}

void TEXTURE_OT_slot_move(wmOperatorType *ot)
{
	static EnumPropertyItem slot_move[] = {
		{-1, "UP", 0, "Up", ""},
		{1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name= "Move Texture Slot";
	ot->idname= "TEXTURE_OT_slot_move";
	ot->description="Move texture slots up and down.";

	/* api callbacks */
	ot->exec= texture_slot_move;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}



/* material copy/paste */
static int copy_material_exec(bContext *C, wmOperator *op)
{
	Material *ma= CTX_data_pointer_get_type(C, "material", &RNA_Material).data;

	if(ma==NULL)
		return;

	copy_matcopybuf(ma);

	WM_event_add_notifier(C, NC_MATERIAL, ma);

	return OPERATOR_FINISHED;
}

void MATERIAL_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Material";
	ot->idname= "MATERIAL_OT_copy";
	ot->description="Copy the material settings and nodes.";

	/* api callbacks */
	ot->exec= copy_material_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int paste_material_exec(bContext *C, wmOperator *op)
{
	Material *ma= CTX_data_pointer_get_type(C, "material", &RNA_Material).data;

	if(ma==NULL)
		return;

	paste_matcopybuf(ma);

	WM_event_add_notifier(C, NC_MATERIAL|ND_SHADING_DRAW, ma);

	return OPERATOR_FINISHED;
}

void MATERIAL_OT_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste Material";
	ot->idname= "MATERIAL_OT_paste";
	ot->description="Copy the material settings and nodes.";

	/* api callbacks */
	ot->exec= paste_material_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
