/*
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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_object.c
 *  \ingroup RNA
 */


#include <stdio.h>
#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_action_types.h"
#include "DNA_customdata_types.h"
#include "DNA_controller_types.h"
#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_meta_types.h"

#include "BLI_utildefines.h"

#include "BKE_tessmesh.h"
#include "BKE_group.h" /* needed for object_in_group() */

#include "BLO_sys_types.h" /* needed for intptr_t used in ED_mesh.h */
#include "ED_mesh.h"

#include "WM_api.h"
#include "WM_types.h"

EnumPropertyItem object_mode_items[] = {
	{OB_MODE_OBJECT, "OBJECT", ICON_OBJECT_DATAMODE, "Object", ""},
	{OB_MODE_EDIT, "EDIT", ICON_EDITMODE_HLT, "Edit", ""},
	{OB_MODE_SCULPT, "SCULPT", ICON_SCULPTMODE_HLT, "Sculpt", ""},
	{OB_MODE_VERTEX_PAINT, "VERTEX_PAINT", ICON_VPAINT_HLT, "Vertex Paint", ""},
	{OB_MODE_WEIGHT_PAINT, "WEIGHT_PAINT", ICON_WPAINT_HLT, "Weight Paint", ""},
	{OB_MODE_TEXTURE_PAINT, "TEXTURE_PAINT", ICON_TPAINT_HLT, "Texture Paint", ""},
	{OB_MODE_PARTICLE_EDIT, "PARTICLE_EDIT", ICON_PARTICLEMODE, "Particle Edit", ""},
	{OB_MODE_POSE, "POSE", ICON_POSE_HLT, "Pose", ""},
	{0, NULL, 0, NULL, NULL}};

static EnumPropertyItem parent_type_items[] = {
	{PAROBJECT, "OBJECT", 0, "Object", "The object is parented to an object"},
	{PARCURVE, "CURVE", 0, "Curve", "The object is parented to a curve"},
	{PARKEY, "KEY", 0, "Key", ""},
	{PARSKEL, "ARMATURE", 0, "Armature", ""},
	{PARSKEL, "LATTICE", 0, "Lattice", "The object is parented to a lattice"}, /* PARSKEL reuse will give issues */
	{PARVERT1, "VERTEX", 0, "Vertex", "The object is parented to a vertex"},
	{PARVERT3, "VERTEX_3", 0, "3 Vertices", ""},
	{PARBONE, "BONE", 0, "Bone", "The object is parented to a bone"},
	{0, NULL, 0, NULL, NULL}};
	
static EnumPropertyItem collision_bounds_items[] = {
	{OB_BOUND_BOX, "BOX", 0, "Box", ""},
	{OB_BOUND_SPHERE, "SPHERE", 0, "Sphere", ""},
	{OB_BOUND_CYLINDER, "CYLINDER", 0, "Cylinder", ""},
	{OB_BOUND_CONE, "CONE", 0, "Cone", ""},
	{OB_BOUND_CONVEX_HULL, "CONVEX_HULL", 0, "Convex Hull", ""},
	{OB_BOUND_TRIANGLE_MESH, "TRIANGLE_MESH", 0, "Triangle Mesh", ""},
	{OB_BOUND_CAPSULE, "CAPSULE", 0, "Capsule", ""},
	/*{OB_DYN_MESH, "DYNAMIC_MESH", 0, "Dynamic Mesh", ""}, */
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem metaelem_type_items[] = {
	{MB_BALL, "BALL", ICON_META_BALL, "Ball", ""},
	{MB_TUBE, "CAPSULE", ICON_META_CAPSULE, "Capsule", ""},
	{MB_PLANE, "PLANE", ICON_META_PLANE, "Plane", ""},
	{MB_ELIPSOID, "ELLIPSOID", ICON_META_ELLIPSOID, "Ellipsoid", ""}, /* NOTE: typo at original definition! */
	{MB_CUBE, "CUBE", ICON_META_CUBE, "Cube", ""},
	{0, NULL, 0, NULL, NULL}};

/* used for 2 enums */
#define OBTYPE_CU_CURVE {OB_CURVE, "CURVE", 0, "Curve", ""}
#define OBTYPE_CU_SURF {OB_SURF, "SURFACE", 0, "Surface", ""}
#define OBTYPE_CU_FONT {OB_FONT, "FONT", 0, "Font", ""}

EnumPropertyItem object_type_items[] = {
	{OB_MESH, "MESH", 0, "Mesh", ""},
	OBTYPE_CU_CURVE,
	OBTYPE_CU_SURF,
	{OB_MBALL, "META", 0, "Meta", ""},
	OBTYPE_CU_FONT,
	{0, "", 0, NULL, NULL},
	{OB_ARMATURE, "ARMATURE", 0, "Armature", ""},
	{OB_LATTICE, "LATTICE", 0, "Lattice", ""},
	{OB_EMPTY, "EMPTY", 0, "Empty", ""},
	{0, "", 0, NULL, NULL},
	{OB_CAMERA, "CAMERA", 0, "Camera", ""},
	{OB_LAMP, "LAMP", 0, "Lamp", ""},
	{OB_SPEAKER, "SPEAKER", 0, "Speaker", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem object_type_curve_items[] = {
	OBTYPE_CU_CURVE,
	OBTYPE_CU_SURF,
	OBTYPE_CU_FONT,
	{0, NULL, 0, NULL, NULL}};


#ifdef RNA_RUNTIME

#include "BLI_math.h"

#include "DNA_key_types.h"
#include "DNA_constraint_types.h"
#include "DNA_lattice_types.h"
#include "DNA_node_types.h"

#include "BKE_armature.h"
#include "BKE_bullet.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_effect.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_deform.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_particle.h"
#include "ED_curve.h"
#include "ED_lattice.h"

static void rna_Object_internal_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	DAG_id_tag_update(ptr->id.data, OB_RECALC_OB);
}

static void rna_Object_matrix_world_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	/* don't use compat so we get predictable rotation */
	object_apply_mat4(ptr->id.data, ((Object *)ptr->id.data)->obmat, FALSE, TRUE);
	rna_Object_internal_update(bmain, scene, ptr);
}

static void rna_Object_matrix_local_get(PointerRNA *ptr, float values[16])
{
	Object *ob = ptr->id.data;

	if (ob->parent) {
		float invmat[4][4]; /* for inverse of parent's matrix */
		invert_m4_m4(invmat, ob->parent->obmat);
		mult_m4_m4m4((float(*)[4])values, invmat, ob->obmat);
	}
	else {
		copy_m4_m4((float(*)[4])values, ob->obmat);
	}
}

static void rna_Object_matrix_local_set(PointerRNA *ptr, const float values[16])
{
	Object *ob = ptr->id.data;

	/* localspace matrix is truly relative to the parent, but parameters
	 * stored in object are relative to parentinv matrix.  Undo the parent
	 * inverse part before updating obmat and calling apply_obmat() */
	if (ob->parent) {
		float invmat[4][4];
		invert_m4_m4(invmat, ob->parentinv);
		mult_m4_m4m4(ob->obmat, invmat, (float(*)[4])values);
	}
	else {
		copy_m4_m4(ob->obmat, (float(*)[4])values);
	}

	/* don't use compat so we get predictable rotation */
	object_apply_mat4(ob, ob->obmat, FALSE, FALSE);
}

static void rna_Object_matrix_basis_get(PointerRNA *ptr, float values[16])
{
	Object *ob = ptr->id.data;
	object_to_mat4(ob, (float(*)[4])values);
}

static void rna_Object_matrix_basis_set(PointerRNA *ptr, const float values[16])
{
	Object *ob = ptr->id.data;
	object_apply_mat4(ob, (float(*)[4])values, FALSE, FALSE);
}

void rna_Object_internal_update_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT|ND_DRAW, ptr->id.data);
}

void rna_Object_active_shape_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob = ptr->id.data;

	if (scene->obedit == ob) {
		/* exit/enter editmode to get new shape */
		switch (ob->type) {
			case OB_MESH:
				EDBM_mesh_load(ob);
				EDBM_mesh_make(scene->toolsettings, scene, ob);
				EDBM_mesh_normals_update(((Mesh*)ob->data)->edit_btmesh);
				BMEdit_RecalcTessellation(((Mesh*)ob->data)->edit_btmesh);
				break;
			case OB_CURVE:
			case OB_SURF:
				load_editNurb(ob);
				make_editNurb(ob);
				break;
			case OB_LATTICE:
				load_editLatt(ob);
				make_editLatt(ob);
				break;
		}
	}

	rna_Object_internal_update_data(bmain, scene, ptr);
}

static void rna_Object_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DAG_id_tag_update(ptr->id.data, OB_RECALC_OB);
	if (scene) {
		DAG_scene_sort(bmain, scene);
	}
	WM_main_add_notifier(NC_OBJECT|ND_PARENT, ptr->id.data);
}

/* when changing the selection flag the scene needs updating */
static void rna_Object_select_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
	if (scene) {
		Object *ob = (Object*)ptr->id.data;
		short mode = ob->flag & SELECT ? BA_SELECT : BA_DESELECT;
		ED_base_object_select(object_in_scene(ob, scene), mode);
	}
}

static void rna_Base_select_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Base *base = (Base*)ptr->data;
	short mode = base->flag & BA_SELECT ? BA_SELECT : BA_DESELECT;
	ED_base_object_select(base, mode);
}

static void rna_Object_layer_update__internal(Main *bmain, Scene *scene, Base *base, Object *ob)
{
	/* try to avoid scene sort */
	if (scene == NULL) {
		/* pass - unlikely but when running scripts on startup it happens */
	}
	else if ((ob->lay & scene->lay) && (base->lay & scene->lay)) {
		 /* pass */
	}
	else if ((ob->lay & scene->lay) == 0 && (base->lay & scene->lay) == 0) {
		/* pass */
	}
	else {
		DAG_scene_sort(bmain, scene);
	}

	DAG_id_type_tag(bmain, ID_OB);
}

static void rna_Object_layer_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	Base *base;

	base = scene ? object_in_scene(ob, scene) : NULL;
	if (!base)
		return;
	
	SWAP(int, base->lay, ob->lay);

	rna_Object_layer_update__internal(bmain, scene, base, ob);
	ob->lay = base->lay;

	WM_main_add_notifier(NC_SCENE|ND_LAYER_CONTENT, scene);
}

static void rna_Base_layer_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Base *base = (Base*)ptr->data;
	Object *ob = (Object*)base->object;

	rna_Object_layer_update__internal(bmain, scene, base, ob);
	ob->lay = base->lay;

	WM_main_add_notifier(NC_SCENE|ND_LAYER_CONTENT, scene);
}

static void rna_Object_data_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object*)ptr->data;
	ID *id = value.data;

	if (id == NULL || ob->mode & OB_MODE_EDIT)
		return;

	if (ob->type == OB_EMPTY) {
		if (ob->data) {
			id_us_min((ID*)ob->data);
			ob->data = NULL;
		}

		if (id && GS(id->name) == ID_IM) {
			id_us_plus(id);
			ob->data = id;
		}
	}
	else if (ob->type == OB_MESH) {
		set_mesh(ob, (Mesh*)id);
	}
	else {
		if (ob->data)
			id_us_min((ID*)ob->data);
		if (id)
			id_us_plus(id);

		ob->data = id;
		test_object_materials(id);

		if (GS(id->name) == ID_CU)
			test_curve_type(ob);
		else if (ob->type == OB_ARMATURE)
			armature_rebuild_pose(ob, ob->data);
	}
}

static StructRNA *rna_Object_data_typef(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->data;

	switch (ob->type) {
		case OB_EMPTY: return &RNA_Image;
		case OB_MESH: return &RNA_Mesh;
		case OB_CURVE: return &RNA_Curve;
		case OB_SURF: return &RNA_Curve;
		case OB_FONT: return &RNA_Curve;
		case OB_MBALL: return &RNA_MetaBall;
		case OB_LAMP: return &RNA_Lamp;
		case OB_CAMERA: return &RNA_Camera;
		case OB_LATTICE: return &RNA_Lattice;
		case OB_ARMATURE: return &RNA_Armature;
		case OB_SPEAKER: return &RNA_Speaker;
		default: return &RNA_ID;
	}
}

static void rna_Object_parent_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object*)ptr->data;
	Object *par = (Object*)value.data;

	ED_object_parent(ob, par, ob->partype, ob->parsubstr);
}

static void rna_Object_parent_type_set(PointerRNA *ptr, int value)
{
	Object *ob = (Object*)ptr->data;

	ED_object_parent(ob, ob->parent, value, ob->parsubstr);
}

static EnumPropertyItem *rna_Object_parent_type_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                      PropertyRNA *UNUSED(prop), int *free)
{
	Object *ob = (Object*)ptr->data;
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	RNA_enum_items_add_value(&item, &totitem, parent_type_items, PAROBJECT);

	if (ob->parent) {
		Object *par = ob->parent;
		
		if (par->type == OB_CURVE)
			RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARCURVE);
		else if (par->type == OB_LATTICE)
			/* special hack: prevents this overriding others */
			RNA_enum_items_add_value(&item, &totitem, &parent_type_items[4], PARSKEL);
		else if (par->type == OB_ARMATURE) {
			/* special hack: prevents this being overrided */
			RNA_enum_items_add_value(&item, &totitem, &parent_type_items[3], PARSKEL);
			RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARBONE);
		}
		else if (par->type == OB_MESH) {
			RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARVERT1);
			RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARVERT3);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*free = 1;

	return item;
}

static EnumPropertyItem *rna_Object_collision_bounds_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                           PropertyRNA *UNUSED(prop), int *free)
{
	Object *ob = (Object*)ptr->data;
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	RNA_enum_items_add_value(&item, &totitem, collision_bounds_items, OB_BOUND_TRIANGLE_MESH);
	RNA_enum_items_add_value(&item, &totitem, collision_bounds_items, OB_BOUND_CONVEX_HULL);

	if (ob->body_type != OB_BODY_TYPE_SOFT) {
		RNA_enum_items_add_value(&item, &totitem, collision_bounds_items, OB_BOUND_CONE);
		RNA_enum_items_add_value(&item, &totitem, collision_bounds_items, OB_BOUND_CYLINDER);
		RNA_enum_items_add_value(&item, &totitem, collision_bounds_items, OB_BOUND_SPHERE);
		RNA_enum_items_add_value(&item, &totitem, collision_bounds_items, OB_BOUND_BOX);
		RNA_enum_items_add_value(&item, &totitem, collision_bounds_items, OB_BOUND_CAPSULE);
	}

	RNA_enum_item_end(&item, &totitem);
	*free = 1;

	return item;
}

static void rna_Object_parent_bone_set(PointerRNA *ptr, const char *value)
{
	Object *ob = (Object*)ptr->data;

	ED_object_parent(ob, ob->parent, ob->partype, value);
}

static void rna_Object_dup_group_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object *)ptr->data;
	Group *grp = (Group *)value.data;
	
	/* must not let this be set if the object belongs in this group already,
	 * thus causing a cycle/infinite-recursion leading to crashes on load [#25298]
	 */
	if (object_in_group(ob, grp) == 0)
		ob->dup_group = grp;
	else
		BKE_report(NULL, RPT_ERROR,
		           "Cannot set dupli-group as object belongs in group being instanced thus causing a cycle");
}

void rna_VertexGroup_name_set(PointerRNA *ptr, const char *value)
{
	Object *ob = (Object *)ptr->id.data;
	bDeformGroup *dg = (bDeformGroup *)ptr->data;
	BLI_strncpy_utf8(dg->name, value, sizeof(dg->name));
	defgroup_unique_name(dg, ob);
}

static int rna_VertexGroup_index_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;

	return BLI_findindex(&ob->defbase, ptr->data);
}

static PointerRNA rna_Object_active_vertex_group_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	return rna_pointer_inherit_refine(ptr, &RNA_VertexGroup, BLI_findlink(&ob->defbase, ob->actdef-1));
}

static int rna_Object_active_vertex_group_index_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	return ob->actdef-1;
}

static void rna_Object_active_vertex_group_index_set(PointerRNA *ptr, int value)
{
	Object *ob = (Object*)ptr->id.data;
	ob->actdef = value+1;
}

static void rna_Object_active_vertex_group_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	Object *ob = (Object*)ptr->id.data;

	*min = 0;
	*max = BLI_countlist(&ob->defbase)-1;
	*max = MAX2(0, *max);
}

void rna_object_vgroup_name_index_get(PointerRNA *ptr, char *value, int index)
{
	Object *ob = (Object*)ptr->id.data;
	bDeformGroup *dg;

	dg = BLI_findlink(&ob->defbase, index-1);

	if (dg) BLI_strncpy(value, dg->name, sizeof(dg->name));
	else value[0] = '\0';
}

int rna_object_vgroup_name_index_length(PointerRNA *ptr, int index)
{
	Object *ob = (Object*)ptr->id.data;
	bDeformGroup *dg;

	dg = BLI_findlink(&ob->defbase, index-1);
	return (dg)? strlen(dg->name): 0;
}

void rna_object_vgroup_name_index_set(PointerRNA *ptr, const char *value, short *index)
{
	Object *ob = (Object*)ptr->id.data;
	*index = defgroup_name_index(ob, value) + 1;
}

void rna_object_vgroup_name_set(PointerRNA *ptr, const char *value, char *result, int maxlen)
{
	Object *ob = (Object*)ptr->id.data;
	bDeformGroup *dg = defgroup_find_name(ob, value);
	if (dg) {
		BLI_strncpy(result, value, maxlen); /* no need for BLI_strncpy_utf8, since this matches an existing group */
		return;
	}

	result[0] = '\0';
}

void rna_object_uvlayer_name_set(PointerRNA *ptr, const char *value, char *result, int maxlen)
{
	Object *ob = (Object*)ptr->id.data;
	Mesh *me;
	CustomDataLayer *layer;
	int a;

	if (ob->type == OB_MESH && ob->data) {
		me = (Mesh*)ob->data;

		for (a = 0; a<me->pdata.totlayer; a++) {
			layer = &me->pdata.layers[a];

			if (layer->type == CD_MTEXPOLY && strcmp(layer->name, value) == 0) {
				BLI_strncpy(result, value, maxlen);
				return;
			}
		}
	}

	result[0] = '\0';
}

void rna_object_vcollayer_name_set(PointerRNA *ptr, const char *value, char *result, int maxlen)
{
	Object *ob = (Object*)ptr->id.data;
	Mesh *me;
	CustomDataLayer *layer;
	int a;

	if (ob->type == OB_MESH && ob->data) {
		me = (Mesh*)ob->data;

		for (a = 0; a<me->fdata.totlayer; a++) {
			layer = &me->fdata.layers[a];

			if (layer->type == CD_MCOL && strcmp(layer->name, value) == 0) {
				BLI_strncpy(result, value, maxlen);
				return;
			}
		}
	}

	result[0] = '\0';
}

static int rna_Object_active_material_index_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	return MAX2(ob->actcol-1, 0);
}

static void rna_Object_active_material_index_set(PointerRNA *ptr, int value)
{
	Object *ob = (Object*)ptr->id.data;
	ob->actcol = value+1;

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me->edit_btmesh)
			me->edit_btmesh->mat_nr = value;
	}
}

static void rna_Object_active_material_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	Object *ob = (Object*)ptr->id.data;
	*min = 0;
	*max = MAX2(ob->totcol-1, 0);
}

/* returns active base material */
static PointerRNA rna_Object_active_material_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	Material *ma;
	
	ma = (ob->totcol)? give_current_material(ob, ob->actcol): NULL;
	return rna_pointer_inherit_refine(ptr, &RNA_Material, ma);
}

static void rna_Object_active_material_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object*)ptr->id.data;

	DAG_id_tag_update(value.data, 0);
	assign_material(ob, value.data, ob->actcol);
}

static void rna_Object_active_particle_system_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	Object *ob = (Object*)ptr->id.data;
	*min = 0;
	*max = BLI_countlist(&ob->particlesystem)-1;
	*max = MAX2(0, *max);
}

static int rna_Object_active_particle_system_index_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	return psys_get_current_num(ob);
}

static void rna_Object_active_particle_system_index_set(PointerRNA *ptr, int value)
{
	Object *ob = (Object*)ptr->id.data;
	psys_set_current_num(ob, value);
}

static void rna_Object_particle_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;

	PE_current_changed(scene, ob);
}

/* rotation - axis-angle */
static void rna_Object_rotation_axis_angle_get(PointerRNA *ptr, float *value)
{
	Object *ob = ptr->data;
	
	/* for now, assume that rotation mode is axis-angle */
	value[0] = ob->rotAngle;
	copy_v3_v3(&value[1], ob->rotAxis);
}

/* rotation - axis-angle */
static void rna_Object_rotation_axis_angle_set(PointerRNA *ptr, const float *value)
{
	Object *ob = ptr->data;
	
	/* for now, assume that rotation mode is axis-angle */
	ob->rotAngle = value[0];
	copy_v3_v3(ob->rotAxis, (float *)&value[1]);
	
	/* TODO: validate axis? */
}

static void rna_Object_rotation_mode_set(PointerRNA *ptr, int value)
{
	Object *ob = ptr->data;
	
	/* use API Method for conversions... */
	BKE_rotMode_change_values(ob->quat, ob->rot, ob->rotAxis, &ob->rotAngle, ob->rotmode, (short)value);
	
	/* finally, set the new rotation type */
	ob->rotmode = value;
}

static void rna_Object_dimensions_get(PointerRNA *ptr, float *value)
{
	Object *ob = ptr->data;
	object_get_dimensions(ob, value);
}

static void rna_Object_dimensions_set(PointerRNA *ptr, const float *value)
{
	Object *ob = ptr->data;
	object_set_dimensions(ob, value);
}

static int rna_Object_location_editable(PointerRNA *ptr, int index)
{
	Object *ob = (Object *)ptr->data;
	
	/* only if the axis in question is locked, not editable... */
	if ((index == 0) && (ob->protectflag & OB_LOCK_LOCX))
		return 0;
	else if ((index == 1) && (ob->protectflag & OB_LOCK_LOCY))
		return 0;
	else if ((index == 2) && (ob->protectflag & OB_LOCK_LOCZ))
		return 0;
	else
		return PROP_EDITABLE;
}

static int rna_Object_scale_editable(PointerRNA *ptr, int index)
{
	Object *ob = (Object *)ptr->data;
	
	/* only if the axis in question is locked, not editable... */
	if ((index == 0) && (ob->protectflag & OB_LOCK_SCALEX))
		return 0;
	else if ((index == 1) && (ob->protectflag & OB_LOCK_SCALEY))
		return 0;
	else if ((index == 2) && (ob->protectflag & OB_LOCK_SCALEZ))
		return 0;
	else
		return PROP_EDITABLE;
}

static int rna_Object_rotation_euler_editable(PointerRNA *ptr, int index)
{
	Object *ob = (Object *)ptr->data;
	
	/* only if the axis in question is locked, not editable... */
	if ((index == 0) && (ob->protectflag & OB_LOCK_ROTX))
		return 0;
	else if ((index == 1) && (ob->protectflag & OB_LOCK_ROTY))
		return 0;
	else if ((index == 2) && (ob->protectflag & OB_LOCK_ROTZ))
		return 0;
	else
		return PROP_EDITABLE;
}

static int rna_Object_rotation_4d_editable(PointerRNA *ptr, int index)
{
	Object *ob = (Object *)ptr->data;
	
	/* only consider locks if locking components individually... */
	if (ob->protectflag & OB_LOCK_ROT4D) {
		/* only if the axis in question is locked, not editable... */
		if ((index == 0) && (ob->protectflag & OB_LOCK_ROTW))
			return 0;
		else if ((index == 1) && (ob->protectflag & OB_LOCK_ROTX))
			return 0;
		else if ((index == 2) && (ob->protectflag & OB_LOCK_ROTY))
			return 0;
		else if ((index == 3) && (ob->protectflag & OB_LOCK_ROTZ))
			return 0;
	}
		
	return PROP_EDITABLE;
}


static PointerRNA rna_MaterialSlot_material_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	Material *ma;
	int index = (Material**)ptr->data - ob->mat;

	ma = give_current_material(ob, index+1);
	return rna_pointer_inherit_refine(ptr, &RNA_Material, ma);
}

static void rna_MaterialSlot_material_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object*)ptr->id.data;
	int index = (Material**)ptr->data - ob->mat;

	assign_material(ob, value.data, index+1);
}

static int rna_MaterialSlot_link_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	int index = (Material**)ptr->data - ob->mat;

	return ob->matbits[index] != 0;
}

static void rna_MaterialSlot_link_set(PointerRNA *ptr, int value)
{
	Object *ob = (Object*)ptr->id.data;
	int index = (Material**)ptr->data - ob->mat;
	
	if (value) {
		ob->matbits[index] = 1;
		/* ob->colbits |= (1<<index); */ /* DEPRECATED */
	}
	else {
		ob->matbits[index] = 0;
		/* ob->colbits &= ~(1<<index); */ /* DEPRECATED */
	}
}

static int rna_MaterialSlot_name_length(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	Material *ma;
	int index = (Material**)ptr->data - ob->mat;

	ma = give_current_material(ob, index+1);

	if (ma)
		return strlen(ma->id.name+2);
	
	return 0;
}

static void rna_MaterialSlot_name_get(PointerRNA *ptr, char *str)
{
	Object *ob = (Object*)ptr->id.data;
	Material *ma;
	int index = (Material**)ptr->data - ob->mat;

	ma = give_current_material(ob, index+1);

	if (ma)
		strcpy(str, ma->id.name+2);
	else
		str[0] = '\0';
}

static void rna_MaterialSlot_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_Object_internal_update(bmain, scene, ptr);
	WM_main_add_notifier(NC_OBJECT|ND_OB_SHADING, ptr->id.data);
}

/* why does this have to be so complicated?, can't all this crap be
 * moved to in BGE conversion function? - Campbell *
 *
 * logic from check_body_type()
 *  */
static int rna_GameObjectSettings_physics_type_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;

	/* determine the body_type setting based on flags */
	if (!(ob->gameflag & OB_COLLISION)) {
		if (ob->gameflag & OB_OCCLUDER) {
			ob->body_type = OB_BODY_TYPE_OCCLUDER;
		}
		else if (ob->gameflag & OB_NAVMESH) {
			ob->body_type = OB_BODY_TYPE_NAVMESH;
		}
		else {
			ob->body_type = OB_BODY_TYPE_NO_COLLISION;
		}
	}
	else if (ob->gameflag & OB_SENSOR) {
		ob->body_type = OB_BODY_TYPE_SENSOR;
	}
	else if (!(ob->gameflag & OB_DYNAMIC)) {
		ob->body_type = OB_BODY_TYPE_STATIC;
	}
	else if (!(ob->gameflag & (OB_RIGID_BODY|OB_SOFT_BODY))) {
		ob->body_type = OB_BODY_TYPE_DYNAMIC;
	}
	else if (ob->gameflag & OB_RIGID_BODY) {
		ob->body_type = OB_BODY_TYPE_RIGID;
	}
	else {
		ob->body_type = OB_BODY_TYPE_SOFT;
		/* create the structure here because we display soft body buttons in the main panel */
		if (!ob->bsoft)
			ob->bsoft = bsbNew();
	}

	return ob->body_type;
}

static void rna_GameObjectSettings_physics_type_set(PointerRNA *ptr, int value)
{
	Object *ob = (Object*)ptr->id.data;
	const int was_navmesh = (ob->gameflag & OB_NAVMESH);
	ob->body_type = value;

	switch (ob->body_type) {
	case OB_BODY_TYPE_SENSOR:
		ob->gameflag |= OB_SENSOR|OB_COLLISION|OB_GHOST;
		ob->gameflag &= ~(OB_OCCLUDER|OB_DYNAMIC|OB_RIGID_BODY|OB_SOFT_BODY|OB_ACTOR|OB_ANISOTROPIC_FRICTION
		                  |OB_DO_FH|OB_ROT_FH|OB_COLLISION_RESPONSE|OB_NAVMESH);
		break;
	case OB_BODY_TYPE_OCCLUDER:
		ob->gameflag |= OB_OCCLUDER;
		ob->gameflag &= ~(OB_SENSOR|OB_RIGID_BODY|OB_SOFT_BODY|OB_COLLISION|OB_DYNAMIC|OB_NAVMESH);
		break;
	case OB_BODY_TYPE_NAVMESH:
		ob->gameflag |= OB_NAVMESH;
		ob->gameflag &= ~(OB_SENSOR|OB_RIGID_BODY|OB_SOFT_BODY|OB_COLLISION|OB_DYNAMIC|OB_OCCLUDER);

		if (ob->type == OB_MESH) {
			/* could be moved into mesh UI but for now ensure mesh data layer */
			BKE_mesh_ensure_navmesh(ob->data);
		}

		break;
	case OB_BODY_TYPE_NO_COLLISION:
		ob->gameflag &= ~(OB_SENSOR|OB_RIGID_BODY|OB_SOFT_BODY|OB_COLLISION|OB_OCCLUDER|OB_DYNAMIC|OB_NAVMESH);
		break;
	case OB_BODY_TYPE_STATIC:
		ob->gameflag |= OB_COLLISION;
		ob->gameflag &= ~(OB_DYNAMIC|OB_RIGID_BODY|OB_SOFT_BODY|OB_OCCLUDER|OB_SENSOR|OB_NAVMESH);
		break;
	case OB_BODY_TYPE_DYNAMIC:
		ob->gameflag |= OB_COLLISION|OB_DYNAMIC|OB_ACTOR;
		ob->gameflag &= ~(OB_RIGID_BODY|OB_SOFT_BODY|OB_OCCLUDER|OB_SENSOR|OB_NAVMESH);
		break;
	case OB_BODY_TYPE_RIGID:
		ob->gameflag |= OB_COLLISION|OB_DYNAMIC|OB_RIGID_BODY|OB_ACTOR;
		ob->gameflag &= ~(OB_SOFT_BODY|OB_OCCLUDER|OB_SENSOR|OB_NAVMESH);
		break;
	default:
	case OB_BODY_TYPE_SOFT:
		ob->gameflag |= OB_COLLISION|OB_DYNAMIC|OB_SOFT_BODY|OB_ACTOR;
		ob->gameflag &= ~(OB_RIGID_BODY|OB_OCCLUDER|OB_SENSOR|OB_NAVMESH);

		/* assume triangle mesh, if no bounds chosen for soft body */
		if ((ob->gameflag & OB_BOUNDS) && (ob->boundtype<OB_BOUND_TRIANGLE_MESH)) {
			ob->boundtype = OB_BOUND_TRIANGLE_MESH;
		}
		/* create a BulletSoftBody structure if not already existing */
		if (!ob->bsoft)
			ob->bsoft = bsbNew();
		break;
	}

	if (was_navmesh != (ob->gameflag & OB_NAVMESH)) {
		if (ob->type == OB_MESH) {
			/* this is needed to refresh the derived meshes draw func */
			DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
		}
	}

	WM_main_add_notifier(NC_OBJECT|ND_DRAW, ptr->id.data);
}

static PointerRNA rna_Object_active_particle_system_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	ParticleSystem *psys = psys_get_current(ob);
	return rna_pointer_inherit_refine(ptr, &RNA_ParticleSystem, psys);
}

static PointerRNA rna_Object_game_settings_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_GameObjectSettings, ptr->id.data);
}


static unsigned int rna_Object_layer_validate__internal(const int *values, unsigned int lay)
{
	int i, tot = 0;

	/* ensure we always have some layer selected */
	for (i = 0; i<20; i++)
		if (values[i])
			tot++;

	if (tot == 0)
		return 0;

	for (i = 0; i<20; i++) {
		if (values[i])	lay |= (1<<i);
		else			lay &= ~(1<<i);
	}

	return lay;
}

static void rna_Object_layer_set(PointerRNA *ptr, const int *values)
{
	Object *ob = (Object*)ptr->data;
	unsigned int lay;

	lay = rna_Object_layer_validate__internal(values, ob->lay);
	if (lay)
		ob->lay = lay;
}

static void rna_Base_layer_set(PointerRNA *ptr, const int *values)
{
	Base *base = (Base*)ptr->data;

	unsigned int lay;
	lay = rna_Object_layer_validate__internal(values, base->lay);
	if (lay)
		base->lay = lay;

	/* rna_Base_layer_update updates the objects layer */
}

static void rna_GameObjectSettings_state_get(PointerRNA *ptr, int *values)
{
	Object *ob = (Object*)ptr->data;
	int i;
	int all_states = (ob->scaflag & OB_ALLSTATE?1:0);

	memset(values, 0, sizeof(int)*OB_MAX_STATES);
	for (i = 0; i<OB_MAX_STATES; i++)
		values[i] = (ob->state & (1<<i)) | all_states;
}

static void rna_GameObjectSettings_state_set(PointerRNA *ptr, const int *values)
{
	Object *ob = (Object*)ptr->data;
	int i, tot = 0;

	/* ensure we always have some state selected */
	for (i = 0; i<OB_MAX_STATES; i++)
		if (values[i])
			tot++;
	
	if (tot == 0)
		return;

	for (i = 0; i<OB_MAX_STATES; i++) {
		if (values[i]) ob->state |= (1<<i);
		else ob->state &= ~(1<<i);
	}
}

static void rna_GameObjectSettings_used_state_get(PointerRNA *ptr, int *values)
{
	Object *ob = (Object*)ptr->data;
	bController *cont;

	memset(values, 0, sizeof(int)*OB_MAX_STATES);
	for (cont = ob->controllers.first; cont; cont = cont->next) {
		int i;

		for (i = 0; i<OB_MAX_STATES; i++) {
			if (cont->state_mask & (1<<i))
				values[i] = 1;
		}
	}
}

static void rna_Object_active_shape_key_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	Object *ob = (Object*)ptr->id.data;
	Key *key = ob_get_key(ob);

	*min = 0;
	if (key) {
		*max = BLI_countlist(&key->block)-1;
		if (*max < 0) *max = 0;
	}
	else {
		*max = 0;
	}
}

static int rna_Object_active_shape_key_index_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;

	return MAX2(ob->shapenr-1, 0);
}

static void rna_Object_active_shape_key_index_set(PointerRNA *ptr, int value)
{
	Object *ob = (Object*)ptr->id.data;

	ob->shapenr = value+1;
}

static PointerRNA rna_Object_active_shape_key_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	Key *key = ob_get_key(ob);
	KeyBlock *kb;
	PointerRNA keyptr;

	if (key == NULL)
		return PointerRNA_NULL;
	
	kb = BLI_findlink(&key->block, ob->shapenr-1);
	RNA_pointer_create((ID *)key, &RNA_ShapeKey, kb, &keyptr);
	return keyptr;
}

static PointerRNA rna_Object_field_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;

	/* weak */
	if (!ob->pd)
		ob->pd = object_add_collision_fields(0);
	
	return rna_pointer_inherit_refine(ptr, &RNA_FieldSettings, ob->pd);
}

static PointerRNA rna_Object_collision_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;

	if (ob->type != OB_MESH)
		return PointerRNA_NULL;

	/* weak */
	if (!ob->pd)
		ob->pd = object_add_collision_fields(0);
	
	return rna_pointer_inherit_refine(ptr, &RNA_CollisionSettings, ob->pd);
}

static PointerRNA rna_Object_active_constraint_get(PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	bConstraint *con = constraints_get_active(&ob->constraints);
	return rna_pointer_inherit_refine(ptr, &RNA_Constraint, con);
}

static void rna_Object_active_constraint_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object*)ptr->id.data;
	constraints_set_active(&ob->constraints, (bConstraint *)value.data);
}

static bConstraint *rna_Object_constraints_new(Object *object, int type)
{
	WM_main_add_notifier(NC_OBJECT|ND_CONSTRAINT|NA_ADDED, object);
	return add_ob_constraint(object, NULL, type);
}

static void rna_Object_constraints_remove(Object *object, ReportList *reports, bConstraint *con)
{
	if (BLI_findindex(&object->constraints, con) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Constraint '%s' not found in object '%s'", con->name, object->id.name+2);
		return;
	}

	remove_constraint(&object->constraints, con);
	ED_object_constraint_update(object);
	ED_object_constraint_set_active(object, NULL);
	WM_main_add_notifier(NC_OBJECT|ND_CONSTRAINT|NA_REMOVED, object);
}

static void rna_Object_constraints_clear(Object *object)
{
	free_constraints(&object->constraints);

	ED_object_constraint_update(object);
	ED_object_constraint_set_active(object, NULL);

	WM_main_add_notifier(NC_OBJECT|ND_CONSTRAINT|NA_REMOVED, object);
}

static ModifierData *rna_Object_modifier_new(Object *object, bContext *C, ReportList *reports,
                                             const char *name, int type)
{
	return ED_object_modifier_add(reports, CTX_data_main(C), CTX_data_scene(C), object, name, type);
}

static void rna_Object_modifier_remove(Object *object, bContext *C, ReportList *reports, ModifierData *md)
{
	ED_object_modifier_remove(reports, CTX_data_main(C), CTX_data_scene(C), object, md);

	WM_main_add_notifier(NC_OBJECT|ND_MODIFIER|NA_REMOVED, object);
}

static void rna_Object_modifier_clear(Object *object, bContext *C)
{
	ED_object_modifier_clear(CTX_data_main(C), CTX_data_scene(C), object);

	WM_main_add_notifier(NC_OBJECT|ND_MODIFIER|NA_REMOVED, object);
}

static void rna_Object_boundbox_get(PointerRNA *ptr, float *values)
{
	Object *ob = (Object*)ptr->id.data;
	BoundBox *bb = object_get_boundbox(ob);
	if (bb) {
		memcpy(values, bb->vec, sizeof(bb->vec));
	}
	else {
		fill_vn_fl(values, sizeof(bb->vec)/sizeof(float), 0.0f);
	}

}

static bDeformGroup *rna_Object_vgroup_new(Object *ob, const char *name)
{
	bDeformGroup *defgroup = ED_vgroup_add_name(ob, name);

	WM_main_add_notifier(NC_OBJECT|ND_DRAW, ob);

	return defgroup;
}

static void rna_Object_vgroup_remove(Object *ob, bDeformGroup *defgroup)
{
	ED_vgroup_delete(ob, defgroup);

	WM_main_add_notifier(NC_OBJECT|ND_DRAW, ob);
}

static void rna_Object_vgroup_clear(Object *ob)
{
	ED_vgroup_clear(ob);

	WM_main_add_notifier(NC_OBJECT|ND_DRAW, ob);
}

static void rna_VertexGroup_vertex_add(ID *id, bDeformGroup *def, ReportList *reports, int index_len,
                                       int *index, float weight, int assignmode)
{
	Object *ob = (Object *)id;

	if (ED_vgroup_object_is_edit_mode(ob)) {
		BKE_reportf(reports, RPT_ERROR, "VertexGroup.add(): Can't be called while object is in edit mode");
		return;
	}

	while (index_len--)
		ED_vgroup_vert_add(ob, def, *index++, weight, assignmode); /* XXX, not efficient calling within loop*/

	WM_main_add_notifier(NC_GEOM|ND_DATA, (ID *)ob->data);
}

static void rna_VertexGroup_vertex_remove(ID *id, bDeformGroup *dg, ReportList *reports, int index_len, int *index)
{
	Object *ob = (Object *)id;

	if (ED_vgroup_object_is_edit_mode(ob)) {
		BKE_reportf(reports, RPT_ERROR, "VertexGroup.remove(): Can't be called while object is in edit mode");
		return;
	}

	while (index_len--)
		ED_vgroup_vert_remove(ob, dg, *index++);

	WM_main_add_notifier(NC_GEOM|ND_DATA, (ID *)ob->data);
}

static float rna_VertexGroup_weight(ID *id, bDeformGroup *dg, ReportList *reports, int index)
{
	float weight = ED_vgroup_vert_weight((Object *)id, dg, index);

	if (weight < 0) {
		BKE_reportf(reports, RPT_ERROR, "Vertex not in group");
	}
	return weight;
}

/* generic poll functions */
int rna_Lattice_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	return ((Object *)value.id.data)->type == OB_LATTICE;
}

int rna_Curve_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	return ((Object *)value.id.data)->type == OB_CURVE;
}

int rna_Armature_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	return ((Object *)value.id.data)->type == OB_ARMATURE;
}

int rna_Mesh_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	return ((Object *)value.id.data)->type == OB_MESH;
}

int rna_Camera_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	return ((Object *)value.id.data)->type == OB_CAMERA;
}

#else

static int rna_matrix_dimsize_4x4[] = {4, 4};

static void rna_def_vertex_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;

	static EnumPropertyItem assign_mode_items[] = {
		{WEIGHT_REPLACE,  "REPLACE",  0, "Replace",  "Replace"},
		{WEIGHT_ADD,      "ADD",      0, "Add",      "Add"},
		{WEIGHT_SUBTRACT, "SUBTRACT", 0, "Subtract", "Subtract"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "VertexGroup", NULL);
	RNA_def_struct_sdna(srna, "bDeformGroup");
	RNA_def_struct_ui_text(srna, "Vertex Group", "Group of vertices, used for armature deform and other purposes");
	RNA_def_struct_ui_icon(srna, ICON_GROUP_VERTEX);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Vertex group name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_VertexGroup_name_set");
		/* update data because modifiers may use [#24761] */
	RNA_def_property_update(prop, NC_GEOM|ND_DATA|NA_RENAME, "rna_Object_internal_update_data");
	
	prop = RNA_def_property(srna, "lock_weight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "", "Maintain the relative weights for the group");
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 0);
		/* update data because modifiers may use [#24761] */
	RNA_def_property_update(prop, NC_GEOM|ND_DATA|NA_RENAME, "rna_Object_internal_update_data");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_VertexGroup_index_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Index", "Index number of the vertex group");

	func = RNA_def_function(srna, "add", "rna_VertexGroup_vertex_add");
	RNA_def_function_ui_description(func, "Add vertices to the group");
	RNA_def_function_flag(func, FUNC_USE_REPORTS|FUNC_USE_SELF_ID);
	/* TODO, see how array size of 0 works, this shouldnt be used */
	prop = RNA_def_int_array(func, "index", 1, NULL, 0, 0, "", "Index List", 0, 0);
	RNA_def_property_flag(prop, PROP_DYNAMIC|PROP_REQUIRED);
	prop = RNA_def_float(func, "weight", 0, 0.0f, 1.0f, "", "Vertex weight", 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_enum(func, "type", assign_mode_items, 0, "", "Vertex assign mode");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "remove", "rna_VertexGroup_vertex_remove");
	RNA_def_function_ui_description(func, "Remove a vertex from the group");
	RNA_def_function_flag(func, FUNC_USE_REPORTS|FUNC_USE_SELF_ID);
	/* TODO, see how array size of 0 works, this shouldnt be used */
	prop = RNA_def_int_array(func, "index", 1, NULL, 0, 0, "", "Index List", 0, 0);
	RNA_def_property_flag(prop, PROP_DYNAMIC|PROP_REQUIRED);

	func = RNA_def_function(srna, "weight", "rna_VertexGroup_weight");
	RNA_def_function_ui_description(func, "Get a vertex weight from the group");
	RNA_def_function_flag(func, FUNC_USE_REPORTS|FUNC_USE_SELF_ID);
	prop = RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "The index of the vertex", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_float(func, "weight", 0, 0.0f, 1.0f, "", "Vertex weight", 0.0f, 1.0f);
	RNA_def_function_return(func, prop);
}

static void rna_def_material_slot(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem link_items[] = {
		{1, "OBJECT", 0, "Object", ""},
		{0, "DATA", 0, "Data", ""},
		{0, NULL, 0, NULL, NULL}};
	
	/* NOTE: there is no MaterialSlot equivalent in DNA, so the internal
	 * pointer data points to ob->mat + index, and we manually implement
	 * get/set for the properties. */

	srna = RNA_def_struct(brna, "MaterialSlot", NULL);
	RNA_def_struct_ui_text(srna, "Material Slot", "Material slot in an object");
	RNA_def_struct_ui_icon(srna, ICON_MATERIAL_DATA);

	prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_MaterialSlot_material_get", "rna_MaterialSlot_material_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Material", "Material datablock used by this material slot");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_MaterialSlot_update");

	prop = RNA_def_property(srna, "link", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, link_items);
	RNA_def_property_enum_funcs(prop, "rna_MaterialSlot_link_get", "rna_MaterialSlot_link_set", NULL);
	RNA_def_property_ui_text(prop, "Link", "Link material to object or the object's data");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_MaterialSlot_update");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_MaterialSlot_name_get", "rna_MaterialSlot_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Material slot name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);
}

static void rna_def_object_game_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem body_type_items[] = {
		{OB_BODY_TYPE_NO_COLLISION, "NO_COLLISION", 0, "No Collision", "Disable collision for this object"},
		{OB_BODY_TYPE_STATIC, "STATIC", 0, "Static", "Stationary object"},
		{OB_BODY_TYPE_DYNAMIC, "DYNAMIC", 0, "Dynamic", "Linear physics"},
		{OB_BODY_TYPE_RIGID, "RIGID_BODY", 0, "Rigid Body", "Linear and angular physics"},
		{OB_BODY_TYPE_SOFT, "SOFT_BODY", 0, "Soft Body", "Soft body"},
		{OB_BODY_TYPE_OCCLUDER, "OCCLUDE", 0, "Occlude", "Occluder for optimizing scene rendering"},
		{OB_BODY_TYPE_SENSOR, "SENSOR", 0, "Sensor",
		                      "Collision Sensor, detects static and dynamic objects but not the other "
		                      "collision sensor objects"},
		{OB_BODY_TYPE_NAVMESH, "NAVMESH", 0, "Navigation Mesh", "Navigation mesh"},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "GameObjectSettings", NULL);
	RNA_def_struct_sdna(srna, "Object");
	RNA_def_struct_nested(brna, srna, "Object");
	RNA_def_struct_ui_text(srna, "Game Object Settings", "Game engine related settings for the object");
	RNA_def_struct_ui_icon(srna, ICON_GAME);

	/* logic */

	prop = RNA_def_property(srna, "sensors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Sensor");
	RNA_def_property_ui_text(prop, "Sensors", "Game engine sensor to detect events");

	prop = RNA_def_property(srna, "controllers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Controller");
	RNA_def_property_ui_text(prop, "Controllers",
	                         "Game engine controllers to process events, connecting sensors to actuators");

	prop = RNA_def_property(srna, "actuators", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Actuator");
	RNA_def_property_ui_text(prop, "Actuators", "Game engine actuators to act on events");

	prop = RNA_def_property(srna, "properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "prop", NULL);
	RNA_def_property_struct_type(prop, "GameProperty"); /* rna_property.c */
	RNA_def_property_ui_text(prop, "Properties", "Game engine properties");

	prop = RNA_def_property(srna, "show_sensors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", OB_SHOWSENS);
	RNA_def_property_ui_text(prop, "Show Sensors", "Shows sensors for this object in the user interface");

	prop = RNA_def_property(srna, "show_controllers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", OB_SHOWCONT);
	RNA_def_property_ui_text(prop, "Show Controllers", "Shows controllers for this object in the user interface");

	prop = RNA_def_property(srna, "show_actuators", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", OB_SHOWACT);
	RNA_def_property_ui_text(prop, "Show Actuators", "Shows actuators for this object in the user interface");

	/* physics */

	prop = RNA_def_property(srna, "physics_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "body_type");
	RNA_def_property_enum_items(prop, body_type_items);
	RNA_def_property_enum_funcs(prop, "rna_GameObjectSettings_physics_type_get",
	                            "rna_GameObjectSettings_physics_type_set", NULL);
	RNA_def_property_ui_text(prop, "Physics Type", "Select the type of physical representation");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_actor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_ACTOR);
	RNA_def_property_ui_text(prop, "Actor", "Object is detected by the Near and Radar sensor");

	prop = RNA_def_property(srna, "use_ghost", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_GHOST);
	RNA_def_property_ui_text(prop, "Ghost", "Object does not restitute collisions, like a ghost");

	prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 10000.0);
	RNA_def_property_ui_text(prop, "Mass", "Mass of the object");

	prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE|PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "inertia");
	RNA_def_property_range(prop, 0.01f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.01f, 10.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Radius", "Radius of bounding sphere and material physics");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "use_sleep", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_COLLISION_RESPONSE);
	RNA_def_property_ui_text(prop, "No Sleeping", "Disable auto (de)activation in physics simulation");

	prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "damping");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Damping", "General movement damping");

	prop = RNA_def_property(srna, "rotation_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rdamping");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Rotation Damping", "General rotation damping");

	prop = RNA_def_property(srna, "velocity_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min_vel");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Velocity Min", "Clamp velocity to this minimum speed (except when totally still)");

	prop = RNA_def_property(srna, "velocity_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_vel");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Velocity Max", "Clamp velocity to this maximum speed");

	/* lock position */
	prop = RNA_def_property(srna, "lock_location_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag2", OB_LOCK_RIGID_BODY_X_AXIS);
	RNA_def_property_ui_text(prop, "Lock X Axis", "Disable simulation of linear motion along the X axis");
	
	prop = RNA_def_property(srna, "lock_location_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag2", OB_LOCK_RIGID_BODY_Y_AXIS);
	RNA_def_property_ui_text(prop, "Lock Y Axis", "Disable simulation of linear motion along the Y axis");
	
	prop = RNA_def_property(srna, "lock_location_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag2", OB_LOCK_RIGID_BODY_Z_AXIS);
	RNA_def_property_ui_text(prop, "Lock Z Axis", "Disable simulation of linear motion along the Z axis");
	
	
	/* lock rotation */
	prop = RNA_def_property(srna, "lock_rotation_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag2", OB_LOCK_RIGID_BODY_X_ROT_AXIS);
	RNA_def_property_ui_text(prop, "Lock X Rotation Axis", "Disable simulation of angular motion along the X axis");
	
	prop = RNA_def_property(srna, "lock_rotation_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag2", OB_LOCK_RIGID_BODY_Y_ROT_AXIS);
	RNA_def_property_ui_text(prop, "Lock Y Rotation Axis", "Disable simulation of angular motion along the Y axis");
	
	prop = RNA_def_property(srna, "lock_rotation_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag2", OB_LOCK_RIGID_BODY_Z_ROT_AXIS);
	RNA_def_property_ui_text(prop, "Lock Z Rotation Axis", "Disable simulation of angular motion along the Z axis");
	
	/* is this used anywhere ? */
	prop = RNA_def_property(srna, "use_activity_culling", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "gameflag2", OB_NEVER_DO_ACTIVITY_CULLING);
	RNA_def_property_ui_text(prop, "Lock Z Rotation Axis", "Disable simulation of angular motion along the Z axis");
	

	prop = RNA_def_property(srna, "use_material_physics_fh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_DO_FH);
	RNA_def_property_ui_text(prop, "Use Material Force Field", "React to force field physics settings in materials");

	prop = RNA_def_property(srna, "use_rotate_from_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_ROT_FH);
	RNA_def_property_ui_text(prop, "Rotate From Normal",
	                         "Use face normal to rotate object, so that it points away from the surface");

	prop = RNA_def_property(srna, "form_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "formfactor");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Form Factor", "Form factor scales the inertia tensor");

	prop = RNA_def_property(srna, "use_anisotropic_friction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_ANISOTROPIC_FRICTION);
	RNA_def_property_ui_text(prop, "Anisotropic Friction", "Enable anisotropic friction");

	prop = RNA_def_property(srna, "friction_coefficients", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "anisotropicFriction");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Friction Coefficients",
	                         "Relative friction coefficients in the in the X, Y and Z directions, "
	                         "when anisotropic friction is enabled");

	prop = RNA_def_property(srna, "use_collision_bounds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_BOUNDS);
	RNA_def_property_ui_text(prop, "Use Collision Bounds", "Specify a collision bounds type other than the default");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "collision_bounds_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "collision_boundtype");
	RNA_def_property_enum_items(prop, collision_bounds_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Object_collision_bounds_itemf");
	RNA_def_property_ui_text(prop, "Collision Bounds",  "Select the collision type");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "use_collision_compound", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_CHILD);
	RNA_def_property_ui_text(prop, "Collision Compound", "Add children to form a compound collision object");

	prop = RNA_def_property(srna, "collision_margin", PROP_FLOAT, PROP_NONE|PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "margin");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Collision Margin",
	                         "Extra margin around object for collision detection, small amount required "
	                         "for stability");

	prop = RNA_def_property(srna, "soft_body", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "bsoft");
	RNA_def_property_ui_text(prop, "Soft Body Settings", "Settings for Bullet soft body simulation");

	prop = RNA_def_property(srna, "use_obstacle_create", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_HASOBSTACLE);
	RNA_def_property_ui_text(prop, "Create obstacle", "Create representation for obstacle simulation");

	prop = RNA_def_property(srna, "obstacle_radius", PROP_FLOAT, PROP_NONE|PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "obstacleRad");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Obstacle Radius", "Radius of object representation in obstacle simulation");
	
	/* state */

	prop = RNA_def_property(srna, "states_visible", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "state", 1);
	RNA_def_property_array(prop, OB_MAX_STATES);
	RNA_def_property_ui_text(prop, "State", "State determining which controllers are displayed");
	RNA_def_property_boolean_funcs(prop, "rna_GameObjectSettings_state_get", "rna_GameObjectSettings_state_set");

	prop = RNA_def_property(srna, "used_states", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_array(prop, OB_MAX_STATES);
	RNA_def_property_ui_text(prop, "Used State", "States which are being used by controllers");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_GameObjectSettings_used_state_get", NULL);
	
	prop = RNA_def_property(srna, "states_initial", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "init_state", 1);
	RNA_def_property_array(prop, OB_MAX_STATES);
	RNA_def_property_ui_text(prop, "Initial State", "Initial state when the game starts");

	prop = RNA_def_property(srna, "show_debug_state", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", OB_DEBUGSTATE);
	RNA_def_property_ui_text(prop, "Debug State", "Print state debug info in the game engine");
	RNA_def_property_ui_icon(prop, ICON_INFO, 0);

	prop = RNA_def_property(srna, "use_all_states", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", OB_ALLSTATE);
	RNA_def_property_ui_text(prop, "All", "Set all state bits");

	prop = RNA_def_property(srna, "show_state_panel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", OB_SHOWSTATE);
	RNA_def_property_ui_text(prop, "States", "Show state panel");
	RNA_def_property_ui_icon(prop, ICON_DISCLOSURE_TRI_RIGHT, 1);
}

static void rna_def_object_constraints(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "ObjectConstraints");
	srna = RNA_def_struct(brna, "ObjectConstraints", NULL);
	RNA_def_struct_sdna(srna, "Object");
	RNA_def_struct_ui_text(srna, "Object Constraints", "Collection of object constraints");


	/* Collection active property */
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Constraint");
	RNA_def_property_pointer_funcs(prop, "rna_Object_active_constraint_get",
	                               "rna_Object_active_constraint_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Constraint", "Active Object constraint");


	/* Constraint collection */
	func = RNA_def_function(srna, "new", "rna_Object_constraints_new");
	RNA_def_function_ui_description(func, "Add a new constraint to this object");
	/* object to add */
	parm = RNA_def_enum(func, "type", constraint_type_items, 1, "", "Constraint type to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm = RNA_def_pointer(func, "constraint", "Constraint", "", "New constraint");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Object_constraints_remove");
	RNA_def_function_ui_description(func, "Remove a constraint from this object");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* constraint to remove */
	parm = RNA_def_pointer(func, "constraint", "Constraint", "", "Removed constraint");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	func = RNA_def_function(srna, "clear", "rna_Object_constraints_clear");
	RNA_def_function_ui_description(func, "Remove all constraint from this object");
}

/* object.modifiers */
static void rna_def_object_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "ObjectModifiers");
	srna = RNA_def_struct(brna, "ObjectModifiers", NULL);
	RNA_def_struct_sdna(srna, "Object");
	RNA_def_struct_ui_text(srna, "Object Modifiers", "Collection of object modifiers");

#if 0
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EditBone");
	RNA_def_property_pointer_sdna(prop, NULL, "act_edbone");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active EditBone", "Armatures active edit bone");
	/*RNA_def_property_update(prop, 0, "rna_Armature_act_editbone_update"); */
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Armature_act_edit_bone_set", NULL, NULL);

	/* todo, redraw */
/*		RNA_def_property_collection_active(prop, prop_act); */
#endif

	/* add target */
	func = RNA_def_function(srna, "new", "rna_Object_modifier_new");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a new modifier");
	parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the bone");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* modifier to add */
	parm = RNA_def_enum(func, "type", modifier_type_items, 1, "", "Modifier type to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm = RNA_def_pointer(func, "modifier", "Modifier", "", "Newly created modifier");
	RNA_def_function_return(func, parm);

	/* remove target */
	func = RNA_def_function(srna, "remove", "rna_Object_modifier_remove");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove an existing modifier from the object");
	/* target to remove*/
	parm = RNA_def_pointer(func, "modifier", "Modifier", "", "Modifier to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	/* clear all modifiers */
	func = RNA_def_function(srna, "clear", "rna_Object_modifier_clear");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Remove all modifiers from the object");
}

/* object.particle_systems */
static void rna_def_object_particle_systems(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	
	PropertyRNA *prop;

	/* FunctionRNA *func; */
	/* PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "ParticleSystems");
	srna = RNA_def_struct(brna, "ParticleSystems", NULL);
	RNA_def_struct_sdna(srna, "Object");
	RNA_def_struct_ui_text(srna, "Particle Systems", "Collection of particle systems");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_pointer_funcs(prop, "rna_Object_active_particle_system_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Particle System", "Active particle system being displayed");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_funcs(prop, "rna_Object_active_particle_system_index_get",
	                           "rna_Object_active_particle_system_index_set",
	                           "rna_Object_active_particle_system_index_range");
	RNA_def_property_ui_text(prop, "Active Particle System Index", "Index of active particle system slot");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_particle_update");
}


/* object.vertex_groups */
static void rna_def_object_vertex_groups(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "VertexGroups");
	srna = RNA_def_struct(brna, "VertexGroups", NULL);
	RNA_def_struct_sdna(srna, "Object");
	RNA_def_struct_ui_text(srna, "Vertex Groups", "Collection of vertex groups");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "VertexGroup");
	RNA_def_property_pointer_funcs(prop, "rna_Object_active_vertex_group_get",
	                               "rna_Object_active_vertex_group_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Vertex Group", "Vertex groups of the object");
	RNA_def_property_update(prop, NC_GEOM|ND_DATA, "rna_Object_internal_update_data");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "actdef");
	RNA_def_property_int_funcs(prop, "rna_Object_active_vertex_group_index_get",
	                           "rna_Object_active_vertex_group_index_set",
	                           "rna_Object_active_vertex_group_index_range");
	RNA_def_property_ui_text(prop, "Active Vertex Group Index", "Active index in vertex group array");
	RNA_def_property_update(prop, NC_GEOM|ND_DATA, "rna_Object_internal_update_data");
	
	/* vertex groups */ /* add_vertex_group */
	func = RNA_def_function(srna, "new", "rna_Object_vgroup_new");
	RNA_def_function_ui_description(func, "Add vertex group to object");
	RNA_def_string(func, "name", "Group", 0, "", "Vertex group name"); /* optional */
	parm = RNA_def_pointer(func, "group", "VertexGroup", "", "New vertex group");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Object_vgroup_remove");
	RNA_def_function_ui_description(func, "Delete vertex group from object");
	parm = RNA_def_pointer(func, "group", "VertexGroup", "", "Vertex group to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	func = RNA_def_function(srna, "clear", "rna_Object_vgroup_clear");
	RNA_def_function_ui_description(func, "Delete all vertex groups from object");
}


static void rna_def_object(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem empty_drawtype_items[] = {
		{OB_PLAINAXES, "PLAIN_AXES", 0, "Plain Axes", ""},
		{OB_ARROWS, "ARROWS", 0, "Arrows", ""},
		{OB_SINGLE_ARROW, "SINGLE_ARROW", 0, "Single Arrow", ""},
		{OB_CIRCLE, "CIRCLE", 0, "Circle", ""},
		{OB_CUBE, "CUBE", 0, "Cube", ""},
		{OB_EMPTY_SPHERE, "SPHERE", 0, "Sphere", ""},
		{OB_EMPTY_CONE, "CONE", 0, "Cone", ""},
		{OB_EMPTY_IMAGE, "IMAGE", 0, "Image", ""},
		{0, NULL, 0, NULL, NULL}};
	
	static EnumPropertyItem track_items[] = {
		{OB_POSX, "POS_X", 0, "+X", ""},
		{OB_POSY, "POS_Y", 0, "+Y", ""},
		{OB_POSZ, "POS_Z", 0, "+Z", ""},
		{OB_NEGX, "NEG_X", 0, "-X", ""},
		{OB_NEGY, "NEG_Y", 0, "-Y", ""},
		{OB_NEGZ, "NEG_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem up_items[] = {
		{OB_POSX, "X", 0, "X", ""},
		{OB_POSY, "Y", 0, "Y", ""},
		{OB_POSZ, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem drawtype_items[] = {
		{OB_BOUNDBOX, "BOUNDS", 0, "Bounds", "Draw the bounds of the object"},
		{OB_WIRE, "WIRE", 0, "Wire", "Draw the object as a wireframe"},
		{OB_SOLID, "SOLID", 0, "Solid", "Draw the object as a solid (if solid drawing is enabled in the viewport)"},
		{OB_TEXTURE, "TEXTURED", 0, "Textured",
		             "Draw the object with textures (if textures are enabled in the viewport)"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem boundtype_items[] = {
		{OB_BOUND_BOX, "BOX", 0, "Box", "Draw bounds as box"},
		{OB_BOUND_SPHERE, "SPHERE", 0, "Sphere", "Draw bounds as sphere"},
		{OB_BOUND_CYLINDER, "CYLINDER", 0, "Cylinder", "Draw bounds as cylinder"},
		{OB_BOUND_CONE, "CONE", 0, "Cone", "Draw bounds as cone"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem dupli_items[] = {
		{0, "NONE", 0, "None", ""},
		{OB_DUPLIFRAMES, "FRAMES", 0, "Frames", "Make copy of object for every frame"},
		{OB_DUPLIVERTS, "VERTS", 0, "Verts", "Duplicate child objects on all vertices"},
		{OB_DUPLIFACES, "FACES", 0, "Faces", "Duplicate child objects on all faces"},
		{OB_DUPLIGROUP, "GROUP", 0, "Group", "Enable group instancing"},
		{0, NULL, 0, NULL, NULL}};
		
	/* XXX: this RNA enum define is currently duplicated for objects,
	 *      since there is some text here which is not applicable */
	static EnumPropertyItem prop_rotmode_items[] = {
		{ROT_MODE_QUAT, "QUATERNION", 0, "Quaternion (WXYZ)", "No Gimbal Lock"},
		{ROT_MODE_XYZ, "XYZ", 0, "XYZ Euler", "XYZ Rotation Order - prone to Gimbal Lock (default)"},
		{ROT_MODE_XZY, "XZY", 0, "XZY Euler", "XZY Rotation Order - prone to Gimbal Lock"},
		{ROT_MODE_YXZ, "YXZ", 0, "YXZ Euler", "YXZ Rotation Order - prone to Gimbal Lock"},
		{ROT_MODE_YZX, "YZX", 0, "YZX Euler", "YZX Rotation Order - prone to Gimbal Lock"},
		{ROT_MODE_ZXY, "ZXY", 0, "ZXY Euler", "ZXY Rotation Order - prone to Gimbal Lock"},
		{ROT_MODE_ZYX, "ZYX", 0, "ZYX Euler", "ZYX Rotation Order - prone to Gimbal Lock"},
		{ROT_MODE_AXISANGLE, "AXIS_ANGLE", 0, "Axis Angle",
		                     "Axis Angle (W+XYZ), defines a rotation around some axis defined by 3D-Vector"},
		{0, NULL, 0, NULL, NULL}};
	
	static float default_quat[4] = {1,0,0,0};	/* default quaternion values */
	static float default_axisAngle[4] = {0,0,1,0};	/* default axis-angle rotation values */
	static float default_scale[3] = {1,1,1}; /* default scale values */
	static int boundbox_dimsize[] = {8, 3};

	srna = RNA_def_struct(brna, "Object", "ID");
	RNA_def_struct_ui_text(srna, "Object", "Object datablock defining an object in a scene");
	RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);
	RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA);

	prop = RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Object_data_set", "rna_Object_data_typef", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Data", "Object data");
	RNA_def_property_update(prop, 0, "rna_Object_internal_update_data");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, object_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Type of Object");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, object_mode_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mode", "Object interaction mode");

	prop = RNA_def_property(srna, "layers", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "lay", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Layers", "Layers the object is on");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Object_layer_set");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_layer_update");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Select", "Object selection state");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_select_update");

	/* for data access */
	prop = RNA_def_property(srna, "bound_box", PROP_FLOAT, PROP_NONE);
	RNA_def_property_multi_array(prop, 2, boundbox_dimsize);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_Object_boundbox_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Bounding Box",
	                         "Object's bounding box in object-space coordinates, all values are -1.0 when "
	                         "not available");

	/* parent */
	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Object_parent_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_ID_SELF_CHECK);
	RNA_def_property_ui_text(prop, "Parent", "Parent Object");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_dependency_update");
	
	prop = RNA_def_property(srna, "parent_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "partype");
	RNA_def_property_enum_items(prop, parent_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Object_parent_type_set", "rna_Object_parent_type_itemf");
	RNA_def_property_ui_text(prop, "Parent Type", "Type of parent relation");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_dependency_update");

	prop = RNA_def_property(srna, "parent_vertices", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "par1");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Parent Vertices", "Indices of vertices in case of a vertex parenting relation");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "parent_bone", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "parsubstr");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Object_parent_bone_set");
	RNA_def_property_ui_text(prop, "Parent Bone", "Name of parent bone in case of a bone parenting relation");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_dependency_update");
	
	/* Track and Up flags */
	/* XXX: these have been saved here for a bit longer (after old track was removed),
	 *      since some other tools still refer to this */
	prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "trackflag");
	RNA_def_property_enum_items(prop, track_items);
	RNA_def_property_ui_text(prop, "Track Axis",
	                         "Axis that points in 'forward' direction (applies to DupliFrame when "
	                         "parent 'Follow' is enabled)");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "upflag");
	RNA_def_property_enum_items(prop, up_items);
	RNA_def_property_ui_text(prop, "Up Axis",
	                         "Axis that points in the upward direction (applies to DupliFrame when "
	                         "parent 'Follow' is enabled)");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");
	
	/* proxy */
	prop = RNA_def_property(srna, "proxy", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Proxy", "Library object this proxy object controls");

	prop = RNA_def_property(srna, "proxy_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Proxy Group", "Library group duplicator object this proxy object controls");

	/* materials */
	prop = RNA_def_property(srna, "material_slots", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
	RNA_def_property_struct_type(prop, "MaterialSlot");
		/* don't dereference pointer! */
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_iterator_array_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Material Slots", "Material slots in the object");

	prop = RNA_def_property(srna, "active_material", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_pointer_funcs(prop, "rna_Object_active_material_get",
	                               "rna_Object_active_material_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Material", "Active material being displayed");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_MaterialSlot_update");

	prop = RNA_def_property(srna, "active_material_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "actcol");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_funcs(prop, "rna_Object_active_material_index_get", "rna_Object_active_material_index_set",
	                           "rna_Object_active_material_index_range");
	RNA_def_property_ui_text(prop, "Active Material Index", "Index of active material slot");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	/* transform */
	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_editable_array_func(prop, "rna_Object_location_editable");
	RNA_def_property_ui_text(prop, "Location", "Location of the object");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
	prop = RNA_def_property(srna, "rotation_quaternion", PROP_FLOAT, PROP_QUATERNION);
	RNA_def_property_float_sdna(prop, NULL, "quat");
	RNA_def_property_editable_array_func(prop, "rna_Object_rotation_4d_editable");
	RNA_def_property_float_array_default(prop, default_quat);
	RNA_def_property_ui_text(prop, "Quaternion Rotation", "Rotation in Quaternions");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
		/* XXX: for axis-angle, it would have been nice to have 2 separate fields for UI purposes, but
		 * having a single one is better for Keyframing and other property-management situations...
		 */
	prop = RNA_def_property(srna, "rotation_axis_angle", PROP_FLOAT, PROP_AXISANGLE);
	RNA_def_property_array(prop, 4);
	RNA_def_property_float_funcs(prop, "rna_Object_rotation_axis_angle_get",
	                             "rna_Object_rotation_axis_angle_set", NULL);
	RNA_def_property_editable_array_func(prop, "rna_Object_rotation_4d_editable");
	RNA_def_property_float_array_default(prop, default_axisAngle);
	RNA_def_property_ui_text(prop, "Axis-Angle Rotation", "Angle of Rotation for Axis-Angle rotation representation");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
	prop = RNA_def_property(srna, "rotation_euler", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_editable_array_func(prop, "rna_Object_rotation_euler_editable");
	RNA_def_property_ui_text(prop, "Euler Rotation", "Rotation in Eulers");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
	prop = RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rotmode");
	RNA_def_property_enum_items(prop, prop_rotmode_items); /* XXX move to using a single define of this someday */
	RNA_def_property_enum_funcs(prop, NULL, "rna_Object_rotation_mode_set", NULL);
	RNA_def_property_ui_text(prop, "Rotation Mode", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_editable_array_func(prop, "rna_Object_scale_editable");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
	RNA_def_property_float_array_default(prop, default_scale);
	RNA_def_property_ui_text(prop, "Scale", "Scaling of the object");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "dimensions", PROP_FLOAT, PROP_XYZ_LENGTH);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_Object_dimensions_get", "rna_Object_dimensions_set", NULL);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
	RNA_def_property_ui_text(prop, "Dimensions", "Absolute bounding box dimensions of the object");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	

	/* delta transforms */
	prop = RNA_def_property(srna, "delta_location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "dloc");
	RNA_def_property_ui_text(prop, "Delta Location", "Extra translation added to the location of the object");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
	prop = RNA_def_property(srna, "delta_rotation_euler", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "drot");
	RNA_def_property_ui_text(prop, "Delta Rotation (Euler)",
	                         "Extra rotation added to the rotation of the object (when using Euler rotations)");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
	prop = RNA_def_property(srna, "delta_rotation_quaternion", PROP_FLOAT, PROP_QUATERNION);
	RNA_def_property_float_sdna(prop, NULL, "dquat");
	RNA_def_property_float_array_default(prop, default_quat);
	RNA_def_property_ui_text(prop, "Delta Rotation (Quaternion)",
	                         "Extra rotation added to the rotation of the object (when using Quaternion rotations)");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
#if 0 /* XXX not supported well yet... */
	prop = RNA_def_property(srna, "delta_rotation_axis_angle", PROP_FLOAT, PROP_AXISANGLE);
		/* FIXME: this is not a single field any more! (drotAxis and drotAngle) */
	RNA_def_property_float_sdna(prop, NULL, "dquat");
	RNA_def_property_float_array_default(prop, default_axisAngle);
	RNA_def_property_ui_text(prop, "Delta Rotation (Axis Angle)",
	                         "Extra rotation added to the rotation of the object (when using Axis-Angle rotations)");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
#endif

	prop = RNA_def_property(srna, "delta_scale", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "dscale");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
	RNA_def_property_float_array_default(prop, default_scale);
	RNA_def_property_ui_text(prop, "Delta Scale", "Extra scaling added to the scale of the object");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
	/* transform locks */
	prop = RNA_def_property(srna, "lock_location", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_LOCX);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Lock Location", "Lock editing of location in the interface");
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "lock_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROTX);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Lock Rotation", "Lock editing of rotation in the interface");
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
		/* XXX this is sub-optimal - it really should be included above,
		 *     but due to technical reasons we can't do this! */
	prop = RNA_def_property(srna, "lock_rotation_w", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROTW);
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
	RNA_def_property_ui_text(prop, "Lock Rotation (4D Angle)",
	                         "Lock editing of 'angle' component of four-component rotations in the interface");
		/* XXX this needs a better name */
	prop = RNA_def_property(srna, "lock_rotations_4d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROT4D);
	RNA_def_property_ui_text(prop, "Lock Rotations (4D)",
	                         "Lock editing of four component rotations by components (instead of as Eulers)");

	prop = RNA_def_property(srna, "lock_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_SCALEX);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Lock Scale", "Lock editing of scale in the interface");
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");

	/* matrix */
	prop = RNA_def_property(srna, "matrix_world", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "obmat");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Matrix World", "Worldspace transformation matrix");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_matrix_world_update");

	prop = RNA_def_property(srna, "matrix_local", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Local Matrix", "Parent relative transformation matrix");
	RNA_def_property_float_funcs(prop, "rna_Object_matrix_local_get", "rna_Object_matrix_local_set", NULL);
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, NULL);

	prop = RNA_def_property(srna, "matrix_basis", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Input Matrix",
	                         "Matrix access to location, rotation and scale (including deltas), "
	                         "before constraints and parenting are applied");
	RNA_def_property_float_funcs(prop, "rna_Object_matrix_basis_get", "rna_Object_matrix_basis_set", NULL);
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");

	/*parent_inverse*/
	prop = RNA_def_property(srna, "matrix_parent_inverse", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "parentinv");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Matrix", "Inverse of object's parent matrix at time of parenting");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");

	/* modifiers */
	prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Modifier");
	RNA_def_property_ui_text(prop, "Modifiers", "Modifiers affecting the geometric data of the object");
	rna_def_object_modifiers(brna, prop);

	/* constraints */
	prop = RNA_def_property(srna, "constraints", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Constraint");
	RNA_def_property_ui_text(prop, "Constraints", "Constraints affecting the transformation of the object");
/*	RNA_def_property_collection_funcs(prop, 0, 0, 0, 0, 0, 0, 0, "constraints__add", "constraints__remove"); */
	rna_def_object_constraints(brna, prop);

	/* game engine */
	prop = RNA_def_property(srna, "game", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "GameObjectSettings");
	RNA_def_property_pointer_funcs(prop, "rna_Object_game_settings_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Game Settings", "Game engine related settings for the object");

	/* vertex groups */
	prop = RNA_def_property(srna, "vertex_groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "defbase", NULL);
	RNA_def_property_struct_type(prop, "VertexGroup");
	RNA_def_property_ui_text(prop, "Vertex Groups", "Vertex groups of the object");
	rna_def_object_vertex_groups(brna, prop);

	/* empty */
	prop = RNA_def_property(srna, "empty_draw_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "empty_drawtype");
	RNA_def_property_enum_items(prop, empty_drawtype_items);
	RNA_def_property_ui_text(prop, "Empty Display Type", "Viewport display style for empties");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "empty_draw_size", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "empty_drawsize");
	RNA_def_property_range(prop, 0.0001f, 1000.0f);
	RNA_def_property_ui_range(prop, 0.01, 100, 1, 2);
	RNA_def_property_ui_text(prop, "Empty Display Size", "Size of display for empties in the viewport");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "empty_image_offset", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "ima_ofs");
	RNA_def_property_ui_text(prop, "Origin Offset", "Origin offset distance");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 0.1f, 2);
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	/* render */
	prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "index");
	RNA_def_property_ui_text(prop, "Pass Index", "Index number for the IndexOB render pass");
	RNA_def_property_update(prop, NC_OBJECT, NULL);
	
	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "col");
	RNA_def_property_ui_text(prop, "Color", "Object color and alpha, used when faces have the ObColor mode enabled");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	/* physics */
	prop = RNA_def_property(srna, "field", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "pd");
	RNA_def_property_struct_type(prop, "FieldSettings");
	RNA_def_property_pointer_funcs(prop, "rna_Object_field_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Field Settings", "Settings for using the object as a field in physics simulation");

	prop = RNA_def_property(srna, "collision", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "pd");
	RNA_def_property_struct_type(prop, "CollisionSettings");
	RNA_def_property_pointer_funcs(prop, "rna_Object_collision_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Collision Settings",
	                         "Settings for using the object as a collider in physics simulation");

	prop = RNA_def_property(srna, "soft_body", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "soft");
	RNA_def_property_struct_type(prop, "SoftBodySettings");
	RNA_def_property_ui_text(prop, "Soft Body Settings", "Settings for soft body simulation");

	prop = RNA_def_property(srna, "particle_systems", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "particlesystem", NULL);
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_ui_text(prop, "Particle Systems", "Particle systems emitted from the object");
	rna_def_object_particle_systems(brna, prop);

	/* restrict */
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", OB_RESTRICT_VIEW);
	RNA_def_property_ui_text(prop, "Restrict View", "Restrict visibility in the viewport");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 1);
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", OB_RESTRICT_SELECT);
	RNA_def_property_ui_text(prop, "Restrict Select", "Restrict selection in the viewport");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 1);
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "hide_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", OB_RESTRICT_RENDER);
	RNA_def_property_ui_text(prop, "Restrict Render", "Restrict renderability");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	/* anim */
	rna_def_animdata_common(srna);
	
	rna_def_animviz_common(srna);
	rna_def_motionpath_common(srna);
	
	/* slow parenting */
	/* XXX: evil old crap */
	prop = RNA_def_property(srna, "use_slow_parent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "partype", PARSLOW);
	RNA_def_property_ui_text(prop, "Slow Parent",
	                         "Create a delay in the parent relationship (beware: this isn't renderfarm "
	                         "safe and may be invalid after jumping around the timeline)");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");
	
	prop = RNA_def_property(srna, "slow_parent_offset", PROP_FLOAT, PROP_NONE|PROP_UNIT_TIME);
	RNA_def_property_float_sdna(prop, NULL, "sf");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Slow Parent Offset", "Delay in the parent relationship");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_internal_update");
	
	/* duplicates */
	prop = RNA_def_property(srna, "dupli_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "transflag");
	RNA_def_property_enum_items(prop, dupli_items);
	RNA_def_property_ui_text(prop, "Dupli Type", "If not None, object duplication method to use");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_dependency_update");

	prop = RNA_def_property(srna, "use_dupli_frames_speed", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "transflag", OB_DUPLINOSPEED);
	RNA_def_property_ui_text(prop, "Dupli Frames Speed",
	                         "Set dupliframes to use the current frame instead of parent curve's evaluation time");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "use_dupli_vertices_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "transflag", OB_DUPLIROT);
	RNA_def_property_ui_text(prop, "Dupli Verts Rotation", "Rotate dupli according to vertex normal");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "use_dupli_faces_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "transflag", OB_DUPLIFACES_SCALE);
	RNA_def_property_ui_text(prop, "Dupli Faces Inherit Scale", "Scale dupli based on face size");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "dupli_faces_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dupfacesca");
	RNA_def_property_range(prop, 0.001f, 10000.0f);
	RNA_def_property_ui_text(prop, "Dupli Faces Scale", "Scale the DupliFace objects");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "dupli_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "dup_group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Object_dup_group_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Dupli Group", "Instance an existing group");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_dependency_update");

	prop = RNA_def_property(srna, "dupli_frames_start", PROP_INT, PROP_NONE|PROP_UNIT_TIME);
	RNA_def_property_int_sdna(prop, NULL, "dupsta");
	RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Dupli Frames Start", "Start frame for DupliFrames");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "dupli_frames_end", PROP_INT, PROP_NONE|PROP_UNIT_TIME);
	RNA_def_property_int_sdna(prop, NULL, "dupend");
	RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Dupli Frames End", "End frame for DupliFrames");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "dupli_frames_on", PROP_INT, PROP_NONE|PROP_UNIT_TIME);
	RNA_def_property_int_sdna(prop, NULL, "dupon");
	RNA_def_property_range(prop, MINFRAME, MAXFRAME);
	RNA_def_property_ui_range(prop, 1, 1500, 1, 0);
	RNA_def_property_ui_text(prop, "Dupli Frames On", "Number of frames to use between DupOff frames");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "dupli_frames_off", PROP_INT, PROP_NONE|PROP_UNIT_TIME);
	RNA_def_property_int_sdna(prop, NULL, "dupoff");
	RNA_def_property_range(prop, 0, MAXFRAME);
	RNA_def_property_ui_range(prop, 0, 1500, 1, 0);
	RNA_def_property_ui_text(prop, "Dupli Frames Off", "Recurring frames to exclude from the Dupliframes");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Object_internal_update");

	prop = RNA_def_property(srna, "dupli_list", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "duplilist", NULL);
	RNA_def_property_struct_type(prop, "DupliObject");
	RNA_def_property_ui_text(prop, "Dupli list", "Object duplis");

	prop = RNA_def_property(srna, "is_duplicator", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "transflag", OB_DUPLI);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	/* drawing */
	prop = RNA_def_property(srna, "draw_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dt");
	RNA_def_property_enum_items(prop, drawtype_items);
	RNA_def_property_ui_text(prop, "Maximum Draw Type",  "Maximum draw type to display object with in viewport");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "show_bounds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_BOUNDBOX);
	RNA_def_property_ui_text(prop, "Draw Bounds", "Display the object's bounds");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop = RNA_def_property(srna, "draw_bounds_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "boundtype");
	RNA_def_property_enum_items(prop, boundtype_items);
	RNA_def_property_ui_text(prop, "Draw Bounds Type", "Object boundary display type");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	prop = RNA_def_property(srna, "show_name", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWNAME);
	RNA_def_property_ui_text(prop, "Draw Name", "Display the object's name");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	prop = RNA_def_property(srna, "show_axis", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_AXIS);
	RNA_def_property_ui_text(prop, "Draw Axes", "Display the object's origin and axes");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	prop = RNA_def_property(srna, "show_texture_space", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_TEXSPACE);
	RNA_def_property_ui_text(prop, "Draw Texture Space", "Display the object's texture space");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	prop = RNA_def_property(srna, "show_wire", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWWIRE);
	RNA_def_property_ui_text(prop, "Draw Wire", "Add the object's wireframe over solid drawing");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	prop = RNA_def_property(srna, "show_transparent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWTRANSP);
	RNA_def_property_ui_text(prop, "Draw Transparent",
	                         "Display material transparency in the object (unsupported for duplicator drawing)");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	prop = RNA_def_property(srna, "show_x_ray", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWXRAY);
	RNA_def_property_ui_text(prop, "X-Ray",
	                         "Make the object draw in front of others (unsupported for duplicator drawing)");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	/* Grease Pencil */
	prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "gpd");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "GreasePencil");
	RNA_def_property_ui_text(prop, "Grease Pencil Data", "Grease Pencil datablock");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	/* pose */
	prop = RNA_def_property(srna, "pose_library", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "poselib");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Action");
	RNA_def_property_ui_text(prop, "Pose Library", "Action used as a pose library for armatures");

	prop = RNA_def_property(srna, "pose", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "pose");
	RNA_def_property_struct_type(prop, "Pose");
	RNA_def_property_ui_text(prop, "Pose", "Current pose for armatures");

	/* shape keys */
	prop = RNA_def_property(srna, "show_only_shape_key", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shapeflag", OB_SHAPE_LOCK);
	RNA_def_property_ui_text(prop, "Shape Key Lock", "Always show the current Shape for this Object");
	RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
	RNA_def_property_update(prop, 0, "rna_Object_internal_update_data");

	prop = RNA_def_property(srna, "use_shape_key_edit_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shapeflag", OB_SHAPE_EDIT_MODE);
	RNA_def_property_ui_text(prop, "Shape Key Edit Mode", "Apply shape keys in edit mode (for Meshes only)");
	RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);
	RNA_def_property_update(prop, 0, "rna_Object_internal_update_data");

	prop = RNA_def_property(srna, "active_shape_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_pointer_funcs(prop, "rna_Object_active_shape_key_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Shape Key", "Current shape key");

	prop = RNA_def_property(srna, "active_shape_key_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "shapenr");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* XXX this is really unpredictable... */
	RNA_def_property_int_funcs(prop, "rna_Object_active_shape_key_index_get", "rna_Object_active_shape_key_index_set",
	                           "rna_Object_active_shape_key_index_range");
	RNA_def_property_ui_text(prop, "Active Shape Key Index", "Current shape key index");
	RNA_def_property_update(prop, 0, "rna_Object_active_shape_update");

	RNA_api_object(srna);
}

static void rna_def_dupli_object(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DupliObject", NULL);
	RNA_def_struct_sdna(srna, "DupliObject");
	RNA_def_struct_ui_text(srna, "Object Duplicate", "An object duplicate");
	/* RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA); */

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	/* RNA_def_property_pointer_funcs(prop, "rna_DupliObject_object_get", NULL, NULL, NULL); */
	RNA_def_property_ui_text(prop, "Object", "Object being duplicated");

	prop = RNA_def_property(srna, "matrix_original", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "omat");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE|PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object Matrix", "The original matrix of this object before it was duplicated");

	prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "mat");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE|PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object Duplicate Matrix", "Object duplicate transformation matrix");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "no_draw", 0);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE|PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hide", "Don't show dupli object in viewport or render");

	/* TODO: DupliObject has more properties that can be wrapped */
}

static void rna_def_object_base(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ObjectBase", NULL);
	RNA_def_struct_sdna(srna, "Base");
	RNA_def_struct_ui_text(srna, "Object Base", "An object instance in a scene");
	RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Object", "Object this base links to");

	/* same as object layer */
	prop = RNA_def_property(srna, "layers", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "lay", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Layers", "Layers the object base is on");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Base_layer_set");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Base_layer_update");
	
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BA_SELECT);
	RNA_def_property_ui_text(prop, "Select", "Object base selection state");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Base_select_update");
	
	RNA_api_object_base(srna);
}

void RNA_def_object(BlenderRNA *brna)
{
	rna_def_object(brna);
	rna_def_object_game_settings(brna);
	rna_def_object_base(brna);
	rna_def_vertex_group(brna);
	rna_def_material_slot(brna);
	rna_def_dupli_object(brna);
}

#endif
