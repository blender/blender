/**
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"

#include "DNA_object_types.h"

#include "BLO_sys_types.h" /* needed for intptr_t used in ED_mesh.h */

#include "ED_mesh.h"

#ifdef RNA_RUNTIME

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_bvhutils.h"

#include "BKE_customdata.h"
#include "BKE_anim.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_font.h"
#include "BKE_mball.h"

#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_curve_types.h"
#include "DNA_modifier_types.h"
#include "DNA_constraint_types.h"

#include "MEM_guardedalloc.h"

/* copied from Mesh_getFromObject and adapted to RNA interface */
/* settings: 0 - preview, 1 - render */
static Mesh *rna_Object_create_mesh(Object *ob, bContext *C, ReportList *reports, int apply_modifiers, int settings)
{
	Mesh *tmpmesh;
	Curve *tmpcu = NULL;
	Object *tmpobj = NULL;
	int render = settings, i;
	int cage = !apply_modifiers;
	Scene *sce = CTX_data_scene(C);

	/* perform the mesh extraction based on type */
	 switch (ob->type) {
	 case OB_FONT:
	 case OB_CURVE:
	 case OB_SURF:

		/* copies object and modifiers (but not the data) */
		tmpobj= copy_object(ob);
		tmpcu = (Curve *)tmpobj->data;
		tmpcu->id.us--;

		/* if getting the original caged mesh, delete object modifiers */
		if( cage )
			object_free_modifiers(tmpobj);

		/* copies the data */
		tmpobj->data = copy_curve( (Curve *) ob->data );

#if 0
		/* copy_curve() sets disp.first null, so currently not need */
		{
			Curve *cu;
			cu = (Curve *)tmpobj->data;
			if( cu->disp.first )
				MEM_freeN( cu->disp.first );
			cu->disp.first = NULL;
		}
	
#endif

		/* get updated display list, and convert to a mesh */
		makeDispListCurveTypes( sce, tmpobj, 0 );
		nurbs_to_mesh( tmpobj );
		
		/* nurbs_to_mesh changes the type to a mesh, check it worked */
		if (tmpobj->type != OB_MESH) {
			free_libblock_us( &(CTX_data_main(C)->object), tmpobj );
			BKE_report(reports, RPT_ERROR, "cant convert curve to mesh. Does the curve have any segments?");
			return NULL;
		}
		tmpmesh = tmpobj->data;
		free_libblock_us( &G.main->object, tmpobj );
		break;

	 case OB_MBALL:
		/* metaballs don't have modifiers, so just convert to mesh */
		ob = find_basis_mball( sce, ob );
		/* todo, re-generatre for render-res */
		/* metaball_polygonize(scene, ob) */

		tmpmesh = add_mesh("Mesh");
		mball_to_mesh( &ob->disp, tmpmesh );
		 break;

	 case OB_MESH:
		/* copies object and modifiers (but not the data) */
		if (cage) {
			/* copies the data */
			tmpmesh = copy_mesh( ob->data );
		/* if not getting the original caged mesh, get final derived mesh */
		} else {
			/* Make a dummy mesh, saves copying */
			DerivedMesh *dm;
			/* CustomDataMask mask = CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL; */
			CustomDataMask mask = CD_MASK_MESH; /* this seems more suitable, exporter,
												   for example, needs CD_MASK_MDEFORMVERT */
			
			/* Write the display mesh into the dummy mesh */
			if (render)
				dm = mesh_create_derived_render( sce, ob, mask );
			else
				dm = mesh_create_derived_view( sce, ob, mask );
			
			tmpmesh = add_mesh( "Mesh" );
			DM_to_mesh( dm, tmpmesh );
			dm->release( dm );
		}
		
		break;
	 default:
		BKE_report(reports, RPT_ERROR, "Object does not have geometry data");
		 return NULL;
	  }

	/* Copy materials to new mesh */
	switch (ob->type) {
	case OB_SURF:
		tmpmesh->totcol = tmpcu->totcol;		
		
		/* free old material list (if it exists) and adjust user counts */
		if( tmpcu->mat ) {
			for( i = tmpcu->totcol; i-- > 0; ) {
				/* are we an object material or data based? */
				if (ob->colbits & 1<<i) 
					tmpmesh->mat[i] = ob->mat[i];
				else 
					tmpmesh->mat[i] = tmpcu->mat[i];

				if (tmpmesh->mat[i]) 
					tmpmesh->mat[i]->id.us++;
			}
		}
		break;

#if 0
	/* Crashes when assigning the new material, not sure why */
	case OB_MBALL:
		tmpmb = (MetaBall *)ob->data;
		tmpmesh->totcol = tmpmb->totcol;
		
		/* free old material list (if it exists) and adjust user counts */
		if( tmpmb->mat ) {
			for( i = tmpmb->totcol; i-- > 0; ) {
				tmpmesh->mat[i] = tmpmb->mat[i]; /* CRASH HERE ??? */
				if (tmpmesh->mat[i]) {
					tmpmb->mat[i]->id.us++;
				}
			}
		}
		break;
#endif

	case OB_MESH:
		if (!cage) {
			Mesh *origmesh= ob->data;
			tmpmesh->flag= origmesh->flag;
			tmpmesh->mat = MEM_dupallocN(origmesh->mat);
			tmpmesh->totcol = origmesh->totcol;
			tmpmesh->smoothresh= origmesh->smoothresh;
			if( origmesh->mat ) {
				for( i = origmesh->totcol; i-- > 0; ) {
					/* are we an object material or data based? */
					if (ob->colbits & 1<<i)
						tmpmesh->mat[i] = ob->mat[i];
					else
						tmpmesh->mat[i] = origmesh->mat[i];
					if (tmpmesh->mat[i])
						tmpmesh->mat[i]->id.us++;
				}
			}
		}
		break;
	} /* end copy materials */

	/* we don't assign it to anything */
	tmpmesh->id.us--;
	
	/* make sure materials get updated in objects */
	test_object_materials( ( ID * ) tmpmesh );

	return tmpmesh;
}

/* When no longer needed, duplilist should be freed with Object.free_duplilist */
static void rna_Object_create_duplilist(Object *ob, bContext *C, ReportList *reports)
{
	if (!(ob->transflag & OB_DUPLI)) {
		BKE_report(reports, RPT_ERROR, "Object does not have duplis.");
		return;
	}

	/* free duplilist if a user forgets to */
	if (ob->duplilist) {
		BKE_reportf(reports, RPT_WARNING, "Object.dupli_list has not been freed.");

		free_object_duplilist(ob->duplilist);
		ob->duplilist= NULL;
	}

	ob->duplilist= object_duplilist(CTX_data_scene(C), ob);

	/* ob->duplilist should now be freed with Object.free_duplilist */
}

static void rna_Object_free_duplilist(Object *ob, ReportList *reports)
{
	if (ob->duplilist) {
		free_object_duplilist(ob->duplilist);
		ob->duplilist= NULL;
	}
}

static bDeformGroup *rna_Object_add_vertex_group(Object *ob, char *group_name)
{
	return ED_vgroup_add_name(ob, group_name);
}

static void rna_Object_add_vertex_to_group(Object *ob, int vertex_index, bDeformGroup *def, float weight, int assignmode)
{
	/* creates dverts if needed */
	ED_vgroup_vert_add(ob, def, vertex_index, weight, assignmode);
}

/* copied from old API Object.makeDisplayList (Object.c) */
static void rna_Object_make_display_list(Object *ob, bContext *C)
{
	Scene *sce= CTX_data_scene(C);

	if (ob->type == OB_FONT) {
		Curve *cu = ob->data;
		freedisplist(&cu->disp);
		BKE_text_to_curve(sce, ob, CU_LEFT);
	}

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
}

static Object *rna_Object_find_armature(Object *ob)
{
	Object *ob_arm = NULL;

	if (ob->type != OB_MESH) return NULL;

	if (ob->parent && ob->partype == PARSKEL && ob->parent->type == OB_ARMATURE) {
		ob_arm = ob->parent;
	}
	else {
		ModifierData *mod = (ModifierData*)ob->modifiers.first;
		while (mod) {
			if (mod->type == eModifierType_Armature) {
				ob_arm = ((ArmatureModifierData*)mod)->object;
			}

			mod = mod->next;
		}
	}

	return ob_arm;
}

static PointerRNA rna_Object_add_shape_key(Object *ob, bContext *C, ReportList *reports, char *name, int from_mix)
{
	Scene *scene= CTX_data_scene(C);
	KeyBlock *kb= NULL;

	if((kb=object_insert_shape_key(scene, ob, name, from_mix))) {
		PointerRNA keyptr;

		RNA_pointer_create((ID *)ob->data, &RNA_ShapeKey, kb, &keyptr);
		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
		
		return keyptr;
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "Object \"%s\"does not support shapes.", ob->id.name+2);
		return PointerRNA_NULL;
	}
}

int rna_Object_is_visible(Object *ob, bContext *C)
{
	return !(ob->restrictflag & OB_RESTRICT_VIEW) && ob->lay & CTX_data_scene(C)->lay;
}

/*
static void rna_Mesh_assign_verts_to_group(Object *ob, bDeformGroup *group, int *indices, int totindex, float weight, int assignmode)
{
	if (ob->type != OB_MESH) {
		BKE_report(reports, RPT_ERROR, "Object should be of MESH type.");
		return;
	}

	Mesh *me = (Mesh*)ob->data;
	int group_index = defgroup_find_index(ob, group);
	if (group_index == -1) {
		BKE_report(reports, RPT_ERROR, "No deform groups assigned to mesh.");
		return;
	}

	if (assignmode != WEIGHT_REPLACE && assignmode != WEIGHT_ADD && assignmode != WEIGHT_SUBTRACT) {
		BKE_report(reports, RPT_ERROR, "Bad assignment mode." );
		return;
	}

	// makes a set of dVerts corresponding to the mVerts
	if (!me->dvert) 
		create_dverts(&me->id);

	// loop list adding verts to group 
	for (i= 0; i < totindex; i++) {
		if(i < 0 || i >= me->totvert) {
			BKE_report(reports, RPT_ERROR, "Bad vertex index in list.");
			return;
		}

		add_vert_defnr(ob, group_index, i, weight, assignmode);
	}
}
*/

void rna_Object_ray_cast(Object *ob, ReportList *reports, float ray_start[3], float ray_end[3], float r_location[3], float r_normal[3], int *index)
{
	BVHTreeFromMesh treeData;
	
	if(ob->derivedFinal==NULL) {
		BKE_reportf(reports, RPT_ERROR, "object \"%s\" has no mesh data to be used for ray casting.", ob->id.name+2);
		return;
	}

	/* no need to managing allocation or freeing of the BVH data. this is generated and freed as needed */
	bvhtree_from_mesh_faces(&treeData, ob->derivedFinal, 0.0f, 4, 6);

	if(treeData.tree==NULL) {
		BKE_reportf(reports, RPT_ERROR, "object \"%s\" could not create internal data for ray casting.", ob->id.name+2);
		return;
	}
	else {
		BVHTreeRayHit hit;
		float ray_nor[3], dist;
		sub_v3_v3v3(ray_nor, ray_end, ray_start);

		dist= hit.dist = normalize_v3(ray_nor);
		hit.index = -1;
		
		if(BLI_bvhtree_ray_cast(treeData.tree, ray_start, ray_nor, 0.0f, &hit, treeData.raycast_callback, &treeData) != -1) {
			if(hit.dist<=dist) {
				copy_v3_v3(r_location, hit.co);
				copy_v3_v3(r_normal, hit.no);
				*index= hit.index;
				return;
			}
		}
	}

	zero_v3(r_location);
	zero_v3(r_normal);
	*index= -1;
}

#else

void RNA_api_object(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem mesh_type_items[] = {
		{0, "PREVIEW", 0, "Preview", "Apply modifier preview settings"},
		{1, "RENDER", 0, "Render", "Apply modifier render settings"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem assign_mode_items[] = {
		{WEIGHT_REPLACE, "REPLACE", 0, "Replace", "Replace"}, /* TODO: more meaningful descriptions */
		{WEIGHT_ADD, "ADD", 0, "Add", "Add"},
		{WEIGHT_SUBTRACT, "SUBTRACT", 0, "Subtract", "Subtract"},
		{0, NULL, 0, NULL, NULL}
	};

	/* mesh */
	func= RNA_def_function(srna, "create_mesh", "rna_Object_create_mesh");
	RNA_def_function_ui_description(func, "Create a Mesh datablock with modifiers applied.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_boolean(func, "apply_modifiers", 0, "", "Apply modifiers.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "settings", mesh_type_items, 0, "", "Modifier settings to apply.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh created from object, remove it if it is only used for export.");
	RNA_def_function_return(func, parm);

	/* duplis */
	func= RNA_def_function(srna, "create_dupli_list", "rna_Object_create_duplilist");
	RNA_def_function_ui_description(func, "Create a list of dupli objects for this object, needs to be freed manually with free_dupli_list.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);

	func= RNA_def_function(srna, "free_dupli_list", "rna_Object_free_duplilist");
	RNA_def_function_ui_description(func, "Free the list of dupli objects.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	/* vertex groups */
	func= RNA_def_function(srna, "add_vertex_group", "rna_Object_add_vertex_group");
	RNA_def_function_ui_description(func, "Add vertex group to object.");
	parm= RNA_def_string(func, "name", "Group", 0, "", "Vertex group name."); /* optional */
	parm= RNA_def_pointer(func, "group", "VertexGroup", "", "New vertex group.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "add_vertex_to_group", "rna_Object_add_vertex_to_group");
	RNA_def_function_ui_description(func, "Add vertex to a vertex group.");
	parm= RNA_def_int(func, "vertex_index", 0, 0, 0, "", "Vertex index.", 0, 0);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "group", "VertexGroup", "", "Vertex group to add vertex to.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_float(func, "weight", 0, 0.0f, 1.0f, "", "Vertex weight.", 0.0f, 1.0f);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "type", assign_mode_items, 0, "", "Vertex assign mode.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* Armature */
	func= RNA_def_function(srna, "find_armature", "rna_Object_find_armature");
	RNA_def_function_ui_description(func, "Find armature influencing this object as a parent or via a modifier.");
	parm= RNA_def_pointer(func, "ob_arm", "Object", "", "Armature object influencing this object or NULL.");
	RNA_def_function_return(func, parm);

	/* Shape key */
	func= RNA_def_function(srna, "add_shape_key", "rna_Object_add_shape_key");
	RNA_def_function_ui_description(func, "Add shape key to an object.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_string(func, "name", "Key", 0, "", "Unique name for the new keylock."); /* optional */
	parm= RNA_def_boolean(func, "from_mix", 1, "", "Create new shape from existing mix of shapes.");
	parm= RNA_def_pointer(func, "key", "ShapeKey", "", "New shape keyblock.");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);

	/* Ray Cast */
	func= RNA_def_function(srna, "ray_cast", "rna_Object_ray_cast");
	RNA_def_function_ui_description(func, "Cast a ray onto in object space.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	
	/* ray start and end */
	parm= RNA_def_float_vector(func, "start", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_float_vector(func, "end", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* return location and normal */
	parm= RNA_def_float_vector(func, "location", 3, NULL, -FLT_MAX, FLT_MAX, "Location", "The hit location of this ray cast", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
	parm= RNA_def_float_vector(func, "normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal", "The face normal at the ray cast hit location", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
	
	parm= RNA_def_int(func, "index", 0, 0, 0, "", "The face index, -1 when no intersection is found.", 0, 0);
	RNA_def_function_output(func, parm);

	
	/* DAG */
	func= RNA_def_function(srna, "make_display_list", "rna_Object_make_display_list");
	RNA_def_function_ui_description(func, "Update object's display data."); /* XXX describe better */
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	/* View */
	func= RNA_def_function(srna, "is_visible", "rna_Object_is_visible");
	RNA_def_function_ui_description(func, "Determine if object is visible in active scene.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm= RNA_def_boolean(func, "is_visible", 0, "", "Object visibility.");
	RNA_def_function_return(func, parm);
}

#endif

