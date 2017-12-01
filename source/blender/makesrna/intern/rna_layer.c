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
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_layer.c
 *  \ingroup RNA
 */

#include "DNA_scene_types.h"
#include "DNA_layer_types.h"

#include "BLI_math.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "ED_object.h"
#include "ED_render.h"

#include "RE_engine.h"

#include "DRW_engine.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

const EnumPropertyItem rna_enum_layer_collection_mode_settings_type_items[] = {
	{COLLECTION_MODE_OBJECT, "OBJECT", 0, "Object", ""},
	{COLLECTION_MODE_EDIT, "EDIT", 0, "Edit", ""},
	{COLLECTION_MODE_PAINT_WEIGHT, "PAINT_WEIGHT", 0, "Weight Paint", ""},
	{COLLECTION_MODE_PAINT_WEIGHT, "PAINT_VERTEX", 0, "Vertex Paint", ""},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_collection_type_items[] = {
	{COLLECTION_TYPE_NONE, "NONE", 0, "Normal", ""},
	{COLLECTION_TYPE_GROUP_INTERNAL, "GROUP_INTERNAL", 0, "Group Internal", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "DNA_group_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"

#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_mesh.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static StructRNA *rna_SceneCollection_refine(PointerRNA *ptr)
{
	SceneCollection *scene_collection = (SceneCollection *)ptr->data;
	switch (scene_collection->type) {
		case COLLECTION_TYPE_GROUP_INTERNAL:
		case COLLECTION_TYPE_NONE:
			return &RNA_SceneCollection;
		default:
			BLI_assert(!"Collection type not fully implemented");
			break;
	}
	return &RNA_SceneCollection;
}

static void rna_SceneCollection_name_set(PointerRNA *ptr, const char *value)
{
	Scene *scene = (Scene *)ptr->id.data;
	SceneCollection *sc = (SceneCollection *)ptr->data;
	BKE_collection_rename(scene, sc, value);
}

static void rna_SceneCollection_filter_set(PointerRNA *ptr, const char *value)
{
	Scene *scene = (Scene *)ptr->id.data;
	SceneCollection *sc = (SceneCollection *)ptr->data;
	BLI_strncpy_utf8(sc->filter, value, sizeof(sc->filter));

	TODO_LAYER_SYNC_FILTER;
	(void)scene;
}

static PointerRNA rna_SceneCollection_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	/* we are actually iterating a LinkData list, so override get */
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, ((LinkData *)internal->link)->data);
}

static int rna_SceneCollection_move_above(ID *id, SceneCollection *sc_src, Main *bmain, SceneCollection *sc_dst)
{
	if (!BKE_collection_move_above(id, sc_dst, sc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static int rna_SceneCollection_move_below(ID *id, SceneCollection *sc_src, Main *bmain, SceneCollection *sc_dst)
{
	if (!BKE_collection_move_below(id, sc_dst, sc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static int rna_SceneCollection_move_into(ID *id, SceneCollection *sc_src, Main *bmain, SceneCollection *sc_dst)
{
	if (!BKE_collection_move_into(id, sc_dst, sc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static SceneCollection *rna_SceneCollection_new(
        ID *id, SceneCollection *sc_parent, Main *bmain, const char *name)
{
	SceneCollection *sc = BKE_collection_add(id, sc_parent, COLLECTION_TYPE_NONE, name);

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return sc;
}

static void rna_SceneCollection_remove(
        ID *id, SceneCollection *sc_parent, Main *bmain, ReportList *reports, PointerRNA *sc_ptr)
{
	SceneCollection *sc = sc_ptr->data;

	const int index = BLI_findindex(&sc_parent->scene_collections, sc);
	if (index == -1) {
		BKE_reportf(reports, RPT_ERROR, "Collection '%s' is not a sub-collection of '%s'",
		            sc->name, sc_parent->name);
		return;
	}

	if (!BKE_collection_remove(id, sc)) {
		BKE_reportf(reports, RPT_ERROR, "Collection '%s' could not be removed from collection '%s'",
		            sc->name, sc_parent->name);
		return;
	}

	RNA_POINTER_INVALIDATE(sc_ptr);

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
}

static int rna_SceneCollection_objects_active_index_get(PointerRNA *ptr)
{
	SceneCollection *sc = (SceneCollection *)ptr->data;
	return sc->active_object_index;
}

static void rna_SceneCollection_objects_active_index_set(PointerRNA *ptr, int value)
{
	SceneCollection *sc = (SceneCollection *)ptr->data;
	sc->active_object_index = value;
}

static void rna_SceneCollection_objects_active_index_range(
        PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
	SceneCollection *sc = (SceneCollection *)ptr->data;
	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&sc->objects) - 1);
}

void rna_SceneCollection_object_link(
        ID *id, SceneCollection *sc, Main *bmain, ReportList *reports, Object *ob)
{
	Scene *scene = (Scene *)id;

	if (BLI_findptr(&sc->objects, ob, offsetof(LinkData, data))) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' is already in collection '%s'", ob->id.name + 2, sc->name);
		return;
	}

	BKE_collection_object_add(&scene->id, sc, ob);

	/* TODO(sergey): Only update relations for the current scene. */
	DEG_relations_tag_update(bmain);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);

	DEG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	WM_main_add_notifier(NC_SCENE | ND_LAYER | ND_OB_ACTIVE, scene);
}

static void rna_SceneCollection_object_unlink(
        ID *id, SceneCollection *sc, Main *bmain, ReportList *reports, Object *ob)
{
	Scene *scene = (Scene *)id;

	if (!BLI_findptr(&sc->objects, ob, offsetof(LinkData, data))) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' is not in collection '%s'", ob->id.name + 2, sc->name);
		return;
	}

	BKE_collection_object_remove(bmain, &scene->id, sc, ob, false);

	/* needed otherwise the depgraph will contain freed objects which can crash, see [#20958] */
	DEG_relations_tag_update(bmain);

	WM_main_add_notifier(NC_SCENE | ND_LAYER | ND_OB_ACTIVE, scene);
}

/****** layer collection engine settings *******/

#define RNA_LAYER_ENGINE_GET_SET(_TYPE_, _ENGINE_, _MODE_, _NAME_)                 \
static _TYPE_ rna_LayerEngineSettings_##_ENGINE_##_##_NAME_##_get(PointerRNA *ptr) \
{                                                                                  \
	IDProperty *props = (IDProperty *)ptr->data;                                   \
	return BKE_collection_engine_property_value_get_##_TYPE_(props, #_NAME_);      \
}                                                                                  \
	                                                                               \
static void rna_LayerEngineSettings_##_ENGINE_##_##_NAME_##_set(PointerRNA *ptr, _TYPE_ value)  \
{                                                                                  \
	IDProperty *props = (IDProperty *)ptr->data;                                   \
	BKE_collection_engine_property_value_set_##_TYPE_(props, #_NAME_, value);      \
}

#define RNA_LAYER_ENGINE_GET_SET_ARRAY(_TYPE_, _ENGINE_, _MODE_, _NAME_, _LEN_)    \
static void rna_LayerEngineSettings_##_ENGINE_##_##_NAME_##_get(PointerRNA *ptr, _TYPE_ *values) \
{                                                                                  \
	IDProperty *props = (IDProperty *)ptr->data;                                   \
	IDProperty *idprop = IDP_GetPropertyFromGroup(props, #_NAME_);                 \
	if (idprop != NULL) {                                                          \
		memcpy(values, IDP_Array(idprop), sizeof(_TYPE_) * idprop->len);           \
	}                                                                              \
}                                                                                  \
	                                                                               \
static void rna_LayerEngineSettings_##_ENGINE_##_##_NAME_##_set(PointerRNA *ptr, const _TYPE_ *values) \
{                                                                                  \
	IDProperty *props = (IDProperty *)ptr->data;                                   \
	BKE_collection_engine_property_value_set_##_TYPE_##_array(props, #_NAME_, values); \
}

#define RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(float, Clay, COLLECTION_MODE_NONE, _NAME_)

#define RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT_ARRAY(_NAME_, _LEN_) \
	RNA_LAYER_ENGINE_GET_SET_ARRAY(float, Clay, COLLECTION_MODE_NONE, _NAME_, _LEN_)

#define RNA_LAYER_ENGINE_CLAY_GET_SET_INT(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(int, Clay, COLLECTION_MODE_NONE, _NAME_)

#define RNA_LAYER_ENGINE_CLAY_GET_SET_BOOL(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(bool, Clay, COLLECTION_MODE_NONE, _NAME_)

#define RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(float, Eevee, COLLECTION_MODE_NONE, _NAME_)

#define RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT_ARRAY(_NAME_, _LEN_) \
	RNA_LAYER_ENGINE_GET_SET_ARRAY(float, Eevee, COLLECTION_MODE_NONE, _NAME_, _LEN_)

#define RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(int, Eevee, COLLECTION_MODE_NONE, _NAME_)

#define RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(bool, Eevee, COLLECTION_MODE_NONE, _NAME_)

/* mode engines */

#define RNA_LAYER_MODE_OBJECT_GET_SET_FLOAT(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(float, ObjectMode, COLLECTION_MODE_OBJECT, _NAME_)

#define RNA_LAYER_MODE_OBJECT_GET_SET_INT(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(int, ObjectMode, COLLECTION_MODE_OBJECT, _NAME_)

#define RNA_LAYER_MODE_OBJECT_GET_SET_BOOL(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(bool, ObjectMode, COLLECTION_MODE_OBJECT, _NAME_)

#define RNA_LAYER_MODE_EDIT_GET_SET_FLOAT(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(float, EditMode, COLLECTION_MODE_EDIT, _NAME_)

#define RNA_LAYER_MODE_EDIT_GET_SET_INT(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(int, EditMode, COLLECTION_MODE_EDIT, _NAME_)

#define RNA_LAYER_MODE_EDIT_GET_SET_BOOL(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(bool, EditMode, COLLECTION_MODE_EDIT, _NAME_)

#define RNA_LAYER_MODE_PAINT_WEIGHT_GET_SET_BOOL(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(bool, PaintWeightMode, COLLECTION_MODE_PAINT_WEIGHT, _NAME_)

#define RNA_LAYER_MODE_PAINT_VERTEX_GET_SET_BOOL(_NAME_) \
	RNA_LAYER_ENGINE_GET_SET(bool, PaintVertexMode, COLLECTION_MODE_PAINT_VERTEX, _NAME_)

/* clay engine */
#ifdef WITH_CLAY_ENGINE
/* ViewLayer settings. */
RNA_LAYER_ENGINE_CLAY_GET_SET_INT(ssao_samples)

/* LayerCollection settings. */
RNA_LAYER_ENGINE_CLAY_GET_SET_INT(matcap_icon)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(matcap_rotation)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(matcap_hue)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(matcap_saturation)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(matcap_value)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(ssao_factor_cavity)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(ssao_factor_edge)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(ssao_distance)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(ssao_attenuation)
RNA_LAYER_ENGINE_CLAY_GET_SET_FLOAT(hair_brightness_randomness)
#endif /* WITH_CLAY_ENGINE */

/* eevee engine */
/* ViewLayer settings. */
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(gtao_enable)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(gtao_use_bent_normals)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(gtao_denoise)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(gtao_bounce)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(gtao_factor)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(gtao_quality)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(gtao_distance)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(gtao_samples)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(dof_enable)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(bokeh_max_size)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(bokeh_threshold)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(bloom_enable)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(bloom_threshold)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT_ARRAY(bloom_color, 3)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(bloom_knee)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(bloom_radius)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(bloom_clamp)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(bloom_intensity)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(motion_blur_enable)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(motion_blur_samples)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(motion_blur_shutter)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(volumetric_enable)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(volumetric_start)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(volumetric_end)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(volumetric_tile_size)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(volumetric_samples)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(volumetric_sample_distribution)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(volumetric_lights)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(volumetric_light_clamp)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(volumetric_shadows)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(volumetric_shadow_samples)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(volumetric_colored_transmittance)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(sss_enable)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(sss_samples)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(sss_jitter_threshold)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(sss_separate_albedo)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(ssr_refraction)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(ssr_enable)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(ssr_halfres)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(ssr_ray_count)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(ssr_quality)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(ssr_max_roughness)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(ssr_thickness)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(ssr_border_fade)
RNA_LAYER_ENGINE_EEVEE_GET_SET_FLOAT(ssr_firefly_fac)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(shadow_method)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(shadow_size)
RNA_LAYER_ENGINE_EEVEE_GET_SET_BOOL(shadow_high_bitdepth)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(taa_samples)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(gi_diffuse_bounces)
RNA_LAYER_ENGINE_EEVEE_GET_SET_INT(gi_cubemap_resolution)

/* object engine */
RNA_LAYER_MODE_OBJECT_GET_SET_BOOL(show_wire)
RNA_LAYER_MODE_OBJECT_GET_SET_BOOL(show_backface_culling)

/* mesh engine */
RNA_LAYER_MODE_EDIT_GET_SET_BOOL(show_occlude_wire)
RNA_LAYER_MODE_EDIT_GET_SET_BOOL(show_weight)
RNA_LAYER_MODE_EDIT_GET_SET_BOOL(face_normals_show)
RNA_LAYER_MODE_EDIT_GET_SET_BOOL(vert_normals_show)
RNA_LAYER_MODE_EDIT_GET_SET_BOOL(loop_normals_show)
RNA_LAYER_MODE_EDIT_GET_SET_FLOAT(normals_length)
RNA_LAYER_MODE_EDIT_GET_SET_FLOAT(backwire_opacity)

/* weight paint engine */
RNA_LAYER_MODE_PAINT_WEIGHT_GET_SET_BOOL(use_shading)
RNA_LAYER_MODE_PAINT_WEIGHT_GET_SET_BOOL(use_wire)

/* vertex paint engine */
RNA_LAYER_MODE_PAINT_VERTEX_GET_SET_BOOL(use_shading)
RNA_LAYER_MODE_PAINT_VERTEX_GET_SET_BOOL(use_wire)

#undef RNA_LAYER_ENGINE_GET_SET

static void rna_ViewLayerEngineSettings_update(bContext *C, PointerRNA *UNUSED(ptr))
{
	Scene *scene = CTX_data_scene(C);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);
}

static void rna_LayerCollectionEngineSettings_update(bContext *UNUSED(C), PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);

	/* Instead of passing 'noteflag' to the rna update function, we handle the notifier ourselves.
	 * We need to do this because the LayerCollection may be coming from different ID types (Scene or Group)
	 * and when using NC_SCENE the id most match the active scene for the listener to receive the notification.*/

	WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
}

static void rna_LayerCollectionEngineSettings_wire_update(bContext *C, PointerRNA *UNUSED(ptr))
{
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object *ob = OBACT(view_layer);

	if (ob != NULL && ob->type == OB_MESH) {
		BKE_mesh_batch_cache_dirty(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
	}

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);
}

/***********************************/

static void engine_settings_use(IDProperty *root, IDProperty *props, PointerRNA *props_ptr, const char *identifier)
{
	PropertyRNA *prop = RNA_struct_find_property(props_ptr, identifier);

	switch (RNA_property_type(prop)) {
		case PROP_FLOAT:
		{
			float value = BKE_collection_engine_property_value_get_float(props, identifier);
			BKE_collection_engine_property_add_float(root, identifier, value);
			break;
		}
		case PROP_ENUM:
		{
			int value = BKE_collection_engine_property_value_get_int(props, identifier);
			BKE_collection_engine_property_add_int(root, identifier, value);
			break;
		}
		case PROP_INT:
		{
			int value = BKE_collection_engine_property_value_get_int(props, identifier);
			BKE_collection_engine_property_add_int(root, identifier, value);
			break;
		}
		case PROP_BOOLEAN:
		{
			int value = BKE_collection_engine_property_value_get_int(props, identifier);
			BKE_collection_engine_property_add_bool(root, identifier, value);
			break;
		}
		case PROP_STRING:
		case PROP_POINTER:
		case PROP_COLLECTION:
		default:
			break;
	}
}

static StructRNA *rna_ViewLayerSettings_refine(PointerRNA *ptr)
{
	IDProperty *props = (IDProperty *)ptr->data;
	BLI_assert(props && props->type == IDP_GROUP);

	switch (props->subtype) {
		case IDP_GROUP_SUB_ENGINE_RENDER:
#ifdef WITH_CLAY_ENGINE
			if (STREQ(props->name, RE_engine_id_BLENDER_CLAY)) {
				return &RNA_ViewLayerEngineSettingsClay;
			}
#endif
			if (STREQ(props->name, RE_engine_id_BLENDER_EEVEE)) {
				return &RNA_ViewLayerEngineSettingsEevee;
			}
			break;
		case IDP_GROUP_SUB_MODE_OBJECT:
		case IDP_GROUP_SUB_MODE_EDIT:
		case IDP_GROUP_SUB_MODE_PAINT_WEIGHT:
		case IDP_GROUP_SUB_MODE_PAINT_VERTEX:
		default:
			BLI_assert(!"Mode not fully implemented");
			break;
	}

	return &RNA_ViewLayerSettings;
}

static void rna_ViewLayerSettings_name_get(PointerRNA *ptr, char *value)
{
	IDProperty *props = (IDProperty *)ptr->data;
	strcpy(value, props->name);
}

static int rna_ViewLayerSettings_name_length(PointerRNA *ptr)
{
	IDProperty *props = (IDProperty *)ptr->data;
	return strnlen(props->name, sizeof(props->name));
}

static void rna_ViewLayerSettings_use(ID *id, IDProperty *props, const char *identifier)
{
	Scene *scene = (Scene *)id;
	PointerRNA scene_props_ptr;
	IDProperty *scene_props;

	scene_props = BKE_view_layer_engine_scene_get(scene, COLLECTION_MODE_NONE, props->name);
	RNA_pointer_create(id, &RNA_ViewLayerSettings, scene_props, &scene_props_ptr);

	engine_settings_use(props, scene_props, &scene_props_ptr, identifier);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
}

static void rna_ViewLayerSettings_unuse(ID *id, IDProperty *props, const char *identifier)
{
	IDProperty *prop_to_remove = IDP_GetPropertyFromGroup(props, identifier);
	IDP_FreeFromGroup(props, prop_to_remove);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
}

static StructRNA *rna_LayerCollectionSettings_refine(PointerRNA *ptr)
{
	IDProperty *props = (IDProperty *)ptr->data;
	BLI_assert(props && props->type == IDP_GROUP);

	switch (props->subtype) {
		case IDP_GROUP_SUB_ENGINE_RENDER:
#ifdef WITH_CLAY_ENGINE
			if (STREQ(props->name, RE_engine_id_BLENDER_CLAY)) {
				return &RNA_LayerCollectionEngineSettingsClay;
			}
#endif
			if (STREQ(props->name, RE_engine_id_BLENDER_EEVEE)) {
				/* printf("Mode not fully implemented\n"); */
				return &RNA_LayerCollectionSettings;
			}
			break;
		case IDP_GROUP_SUB_MODE_OBJECT:
			return &RNA_LayerCollectionModeSettingsObject;
			break;
		case IDP_GROUP_SUB_MODE_EDIT:
			return &RNA_LayerCollectionModeSettingsEdit;
			break;
		case IDP_GROUP_SUB_MODE_PAINT_WEIGHT:
			return &RNA_LayerCollectionModeSettingsPaintWeight;
			break;
		case IDP_GROUP_SUB_MODE_PAINT_VERTEX:
			return &RNA_LayerCollectionModeSettingsPaintVertex;
			break;
		default:
			BLI_assert(!"Mode not fully implemented");
			break;
	}

	return &RNA_LayerCollectionSettings;
}

static void rna_LayerCollectionSettings_name_get(PointerRNA *ptr, char *value)
{
	IDProperty *props = (IDProperty *)ptr->data;
	strcpy(value, props->name);
}

static int rna_LayerCollectionSettings_name_length(PointerRNA *ptr)
{
	IDProperty *props = (IDProperty *)ptr->data;
	return strnlen(props->name, sizeof(props->name));
}

static void rna_LayerCollectionSettings_use(ID *id, IDProperty *props, const char *identifier)
{
	Scene *scene = (Scene *)id;
	PointerRNA scene_props_ptr;
	IDProperty *scene_props;

	scene_props = BKE_layer_collection_engine_scene_get(scene, COLLECTION_MODE_NONE, props->name);
	RNA_pointer_create(id, &RNA_LayerCollectionSettings, scene_props, &scene_props_ptr);
	engine_settings_use(props, scene_props, &scene_props_ptr, identifier);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
}

static void rna_LayerCollectionSettings_unuse(ID *id, IDProperty *props, const char *identifier)
{
	IDProperty *prop_to_remove = IDP_GetPropertyFromGroup(props, identifier);
	IDP_FreeFromGroup(props, prop_to_remove);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
}

static void rna_LayerCollection_name_get(PointerRNA *ptr, char *value)
{
	SceneCollection *sc = ((LayerCollection *)ptr->data)->scene_collection;
	strcpy(value, sc->name);
}

static int rna_LayerCollection_name_length(PointerRNA *ptr)
{
	SceneCollection *sc = ((LayerCollection *)ptr->data)->scene_collection;
	return strnlen(sc->name, sizeof(sc->name));
}

static void rna_LayerCollection_name_set(PointerRNA *ptr, const char *value)
{
	Scene *scene = (Scene *)ptr->id.data;
	SceneCollection *sc = ((LayerCollection *)ptr->data)->scene_collection;
	BKE_collection_rename(scene, sc, value);
}

static PointerRNA rna_LayerCollection_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;
	Base *base = ((LinkData *)internal->link)->data;
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, base->object);
}

static int rna_LayerCollection_move_above(ID *id, LayerCollection *lc_src, Main *bmain, LayerCollection *lc_dst)
{
	if (!BKE_layer_collection_move_above(id, lc_dst, lc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static int rna_LayerCollection_move_below(ID *id, LayerCollection *lc_src, Main *bmain, LayerCollection *lc_dst)
{
	if (!BKE_layer_collection_move_below(id, lc_dst, lc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static int rna_LayerCollection_move_into(ID *id, LayerCollection *lc_src, Main *bmain, LayerCollection *lc_dst)
{
	if (!BKE_layer_collection_move_into(id, lc_dst, lc_src)) {
		return 0;
	}

	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

	return 1;
}

static void rna_LayerCollection_flag_update(bContext *C, PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
}

static void rna_LayerCollection_enable_set(
        ID *id, LayerCollection *layer_collection, Main *bmain, bContext *C, ReportList *reports, int value)
{
	ViewLayer *view_layer;
	if (GS(id->name) == ID_SCE) {
		Scene *scene = (Scene *)id;
		view_layer = BKE_view_layer_find_from_collection(&scene->id, layer_collection);
	}
	else {
		BLI_assert(GS(id->name) == ID_GR);
		Group *group = (Group *)id;
		view_layer = group->view_layer;
	}

	if (layer_collection->flag & COLLECTION_DISABLED) {
		if (value == 1) {
			BKE_collection_enable(view_layer, layer_collection);
		}
		else {
			BKE_reportf(reports, RPT_ERROR, "Layer collection '%s' is already disabled",
			            layer_collection->scene_collection->name);
			return;
		}
	}
	else {
		if (value == 0) {
			BKE_collection_disable(view_layer, layer_collection);
		}
		else {
			BKE_reportf(reports, RPT_ERROR, "Layer collection '%s' is already enabled",
			            layer_collection->scene_collection->name);
		}
	}

	Scene *scene = CTX_data_scene(C);
	DEG_relations_tag_update(bmain);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
}

static Group *rna_LayerCollection_create_group(
        ID *id, LayerCollection *layer_collection, Main *bmain, bContext *C, ReportList *reports)
{
	Group *group;
	Scene *scene = (Scene *)id;
	SceneCollection *scene_collection = layer_collection->scene_collection;

	/* The master collection can't be converted. */
	if (scene_collection == BKE_collection_master(&scene->id)) {
		BKE_report(reports, RPT_ERROR, "The master collection can't be converted to group");
		return NULL;
	}

	group = BKE_collection_group_create(bmain, scene, layer_collection);
	if (group == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Failed to convert collection %s", scene_collection->name);
		return NULL;
	}

	DEG_relations_tag_update(bmain);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);
	return group;
}

static int rna_LayerCollections_active_collection_index_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	return view_layer->active_collection;
}

static void rna_LayerCollections_active_collection_index_set(PointerRNA *ptr, int value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	int num_collections = BKE_layer_collection_count(view_layer);
	view_layer->active_collection = min_ff(value, num_collections - 1);
}

static void rna_LayerCollections_active_collection_index_range(
        PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	*min = 0;
	*max = max_ii(0, BKE_layer_collection_count(view_layer) - 1);
}

static PointerRNA rna_LayerCollections_active_collection_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
	return rna_pointer_inherit_refine(ptr, &RNA_LayerCollection, lc);
}

static void rna_LayerCollections_active_collection_set(PointerRNA *ptr, PointerRNA value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	LayerCollection *lc = (LayerCollection *)value.data;
	const int index = BKE_layer_collection_findindex(view_layer, lc);
	if (index != -1) view_layer->active_collection = index;
}

LayerCollection * rna_ViewLayer_collection_link(
        ID *id, ViewLayer *view_layer, Main *bmain, SceneCollection *sc)
{
	Scene *scene = (Scene *)id;
	LayerCollection *lc = BKE_collection_link(view_layer, sc);

	DEG_relations_tag_update(bmain);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
	WM_main_add_notifier(NC_SCENE | ND_LAYER, scene);

	return lc;
}

static void rna_ViewLayer_collection_unlink(
        ID *id, ViewLayer *view_layer, Main *bmain, ReportList *reports, LayerCollection *lc)
{
	Scene *scene = (Scene *)id;

	if (BLI_findindex(&view_layer->layer_collections, lc) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Layer collection '%s' is not in '%s'",
		            lc->scene_collection->name, view_layer->name);
		return;
	}

	BKE_collection_unlink(view_layer, lc);

	DEG_relations_tag_update(bmain);
	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);
	WM_main_add_notifier(NC_SCENE | ND_LAYER | ND_OB_ACTIVE, scene);
}

static PointerRNA rna_LayerObjects_active_object_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Object, view_layer->basact ? view_layer->basact->object : NULL);
}

static void rna_LayerObjects_active_object_set(PointerRNA *ptr, PointerRNA value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	if (value.data)
		view_layer->basact = BKE_view_layer_base_find(view_layer, (Object *)value.data);
	else
		view_layer->basact = NULL;
}

static IDProperty *rna_ViewLayer_idprops(PointerRNA *ptr, bool create)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;

	if (create && !view_layer->id_properties) {
		IDPropertyTemplate val = {0};
		view_layer->id_properties = IDP_New(IDP_GROUP, &val, "ViewLayer ID properties");
	}

	return view_layer->id_properties;
}

static void rna_ViewLayer_update_render_passes(ID *id)
{
	Scene *scene = (Scene *)id;
	if (scene->nodetree)
		ntreeCompositUpdateRLayers(scene->nodetree);
}

static PointerRNA rna_ViewLayer_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	/* we are actually iterating a ObjectBase list, so override get */
	Base *base = (Base *)internal->link;
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, base->object);
}

static int rna_ViewLayer_objects_selected_skip(CollectionPropertyIterator *iter, void *UNUSED(data))
{
	ListBaseIterator *internal = &iter->internal.listbase;
	Base *base = (Base *)internal->link;

	if ((base->flag & BASE_SELECTED) != 0) {
		return 0;
	}

	return 1;
};

static PointerRNA rna_ViewLayer_depsgraph_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, false);
	return rna_pointer_inherit_refine(ptr, &RNA_Depsgraph, depsgraph);
}

static void rna_LayerObjects_selected_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	rna_iterator_listbase_begin(iter, &view_layer->object_bases, rna_ViewLayer_objects_selected_skip);
}

static void rna_ViewLayer_update_tagged(ViewLayer *UNUSED(view_layer), bContext *C)
{
	Depsgraph *graph = CTX_data_depsgraph(C);
	DEG_OBJECT_ITER(graph, ob, DEG_ITER_OBJECT_FLAG_ALL)
	{
		/* Don't do anything, we just need to run the iterator to flush
		 * the base info to the objects. */
		UNUSED_VARS(ob);
	}
	DEG_OBJECT_ITER_END
}

static void rna_ObjectBase_select_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Base *base = (Base *)ptr->data;
	short mode = (base->flag & BASE_SELECTED) ? BA_SELECT : BA_DESELECT;
	ED_object_base_select(base, mode);
}

static char *rna_ViewRenderSettings_path(PointerRNA *UNUSED(ptr))
{
	return BLI_sprintfN("viewport_render");
}

static void rna_ViewRenderSettings_engine_set(PointerRNA *ptr, int value)
{
	ViewRender *view_render = (ViewRender *)ptr->data;
	RenderEngineType *type = BLI_findlink(&R_engines, value);

	if (type) {
		BLI_strncpy_utf8(view_render->engine_id, type->idname, sizeof(view_render->engine_id));
		DEG_id_tag_update(ptr->id.data, DEG_TAG_COPY_ON_WRITE);
	}
}

static const EnumPropertyItem *rna_ViewRenderSettings_engine_itemf(
        bContext *UNUSED(C), PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	RenderEngineType *type;
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	int a = 0, totitem = 0;

	for (type = R_engines.first; type; type = type->next, a++) {
		tmp.value = a;
		tmp.identifier = type->idname;
		tmp.name = type->name;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static int rna_ViewRenderSettings_engine_get(PointerRNA *ptr)
{
	ViewRender *view_render = (ViewRender *)ptr->data;
	RenderEngineType *type;
	int a = 0;

	for (type = R_engines.first; type; type = type->next, a++)
		if (STREQ(type->idname, view_render->engine_id))
			return a;

	return 0;
}

static void rna_ViewRenderSettings_engine_update(Main *bmain, Scene *UNUSED(unused), PointerRNA *UNUSED(ptr))
{
	ED_render_engine_changed(bmain);
}

static int rna_ViewRenderSettings_multiple_engines_get(PointerRNA *UNUSED(ptr))
{
	return (BLI_listbase_count(&R_engines) > 1);
}

static int rna_ViewRenderSettings_use_shading_nodes_get(PointerRNA *ptr)
{
	ViewRender *view_render = (ViewRender *)ptr->data;
	return BKE_viewrender_use_new_shading_nodes(view_render);
}

static int rna_ViewRenderSettings_use_spherical_stereo_get(PointerRNA *ptr)
{
	ViewRender *view_render = (ViewRender *)ptr->data;
	return BKE_viewrender_use_spherical_stereo(view_render);
}

static int rna_ViewRenderSettings_use_game_engine_get(PointerRNA *ptr)
{
	ViewRender *view_render = (ViewRender *)ptr->data;
	RenderEngineType *type;

	for (type = R_engines.first; type; type = type->next)
		if (STREQ(type->idname, view_render->engine_id))
			return (type->flag & RE_GAME) != 0;

	return 0;
}

#else

static void rna_def_scene_collections(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "SceneCollections");
	srna = RNA_def_struct(brna, "SceneCollections", NULL);
	RNA_def_struct_sdna(srna, "SceneCollection");
	RNA_def_struct_ui_text(srna, "Scene Collection", "Collection of scene collections");

	func = RNA_def_function(srna, "new", "rna_SceneCollection_new");
	RNA_def_function_ui_description(func, "Add a collection to scene");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_string(func, "name", "SceneCollection", 0, "", "New name for the collection (not unique)");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "result", "SceneCollection", "", "Newly created collection");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_SceneCollection_remove");
	RNA_def_function_ui_description(func, "Remove a collection layer");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "layer", "SceneCollection", "", "Collection to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_collection_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "CollectionObjects");
	srna = RNA_def_struct(brna, "CollectionObjects", NULL);
	RNA_def_struct_sdna(srna, "SceneCollection");
	RNA_def_struct_ui_text(srna, "Collection Objects", "Objects of a collection");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_SceneCollection_objects_active_index_get",
	                           "rna_SceneCollection_objects_active_index_set",
	                           "rna_SceneCollection_objects_active_index_range");
	RNA_def_property_ui_text(prop, "Active Object Index", "Active index in collection objects array");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, NULL);

	func = RNA_def_function(srna, "link", "rna_SceneCollection_object_link");
	RNA_def_function_ui_description(func, "Link an object to collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "object", "Object", "", "Object to add to collection");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	func = RNA_def_function(srna, "unlink", "rna_SceneCollection_object_unlink");
	RNA_def_function_ui_description(func, "Unlink object from collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "object", "Object", "", "Object to remove from collection");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_scene_collection(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "SceneCollection", NULL);
	RNA_def_struct_ui_text(srna, "Scene Collection", "Collection");
	RNA_def_struct_refine_func(srna, "rna_SceneCollection_refine");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SceneCollection_name_set");
	RNA_def_property_ui_text(prop, "Name", "Collection name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, NULL);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_collection_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of collection");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "filter", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SceneCollection_filter_set");
	RNA_def_property_ui_text(prop, "Filter", "Filter to dynamically include objects based on their names (e.g., CHAR_*)");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, NULL);

	prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "scene_collections", NULL);
	RNA_def_property_struct_type(prop, "SceneCollection");
	RNA_def_property_ui_text(prop, "SceneCollections", "");
	rna_def_scene_collections(brna, prop);

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "objects", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_SceneCollection_objects_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Objects", "All the objects directly added to this collection (not including sub-collection objects)");
	rna_def_collection_objects(brna, prop);

	prop = RNA_def_property(srna, "filters_objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "filter_objects", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_SceneCollection_objects_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Filter Objects", "All the objects dynamically added to this collection via the filter");

	/* Functions */
	func = RNA_def_function(srna, "move_above", "rna_SceneCollection_move_above");
	RNA_def_function_ui_description(func, "Move collection after another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "sc_dst", "SceneCollection", "Collection", "Reference collection above which the collection will move");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "move_below", "rna_SceneCollection_move_below");
	RNA_def_function_ui_description(func, "Move collection before another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "sc_dst", "SceneCollection", "Collection", "Reference collection below which the collection will move");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "move_into", "rna_SceneCollection_move_into");
	RNA_def_function_ui_description(func, "Move collection into another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "sc_dst", "SceneCollection", "Collection", "Collection to insert into");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);
}

static void rna_def_layer_collection_override(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LayerCollectionOverride", NULL);
	RNA_def_struct_sdna(srna, "CollectionOverride");
	RNA_def_struct_ui_text(srna, "Collection Override", "Collection Override");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Collection name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, NULL);
}


#ifdef WITH_CLAY_ENGINE
static void rna_def_view_layer_engine_settings_clay(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ViewLayerEngineSettingsClay", "ViewLayerSettings");
	RNA_def_struct_ui_text(srna, "Clay Scene Layer Settings", "Clay Engine settings");

	RNA_define_verify_sdna(0); /* not in sdna */

	/* see RNA_LAYER_ENGINE_GET_SET macro */
	prop = RNA_def_property(srna, "ssao_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Clay_ssao_samples_get",
	                           "rna_LayerEngineSettings_Clay_ssao_samples_set", NULL);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples");
	RNA_def_property_range(prop, 1, 500);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	RNA_define_verify_sdna(1); /* not in sdna */
}
#endif /* WITH_CLAY_ENGINE */

static void rna_def_view_layer_engine_settings_eevee(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* Keep in sync with eevee_private.h */
	static const  EnumPropertyItem eevee_shadow_method_items[] = {
		{1, "ESM", 0, "ESM", "Exponential Shadow Mapping"},
		{2, "VSM", 0, "VSM", "Variance Shadow Mapping"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem eevee_shadow_size_items[] = {
		{64, "64", 0, "64px", ""},
		{128, "128", 0, "128px", ""},
		{256, "256", 0, "256px", ""},
		{512, "512", 0, "512px", ""},
		{1024, "1024", 0, "1024px", ""},
		{2048, "2048", 0, "2048px", ""},
		{4096, "4096", 0, "4096px", ""},
		{8192, "8192", 0, "8192px", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem eevee_volumetric_tile_size_items[] = {
		{2, "2", 0, "2px", ""},
		{4, "4", 0, "4px", ""},
		{8, "8", 0, "8px", ""},
		{16, "16", 0, "16px", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ViewLayerEngineSettingsEevee", "ViewLayerSettings");
	RNA_def_struct_ui_text(srna, "Eevee Scene Layer Settings", "Eevee Engine settings");

	RNA_define_verify_sdna(0); /* not in sdna */

	/* see RNA_LAYER_ENGINE_GET_SET macro */

	/* Indirect Lighting */
	prop = RNA_def_property(srna, "gi_diffuse_bounces", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Eevee_gi_diffuse_bounces_get",
	                               "rna_LayerEngineSettings_Eevee_gi_diffuse_bounces_set", NULL);
	RNA_def_property_ui_text(prop, "Diffuse Bounces", "Number of time the light is reinjected inside light grids, "
	                                                  "0 disable indirect diffuse light");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "gi_cubemap_resolution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_LayerEngineSettings_Eevee_gi_cubemap_resolution_get", "rna_LayerEngineSettings_Eevee_gi_cubemap_resolution_set", NULL);
	RNA_def_property_enum_items(prop, eevee_shadow_size_items);
	RNA_def_property_ui_text(prop, "Cubemap Size", "Size of every cubemaps");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Temporal Anti-Aliasing (super sampling) */
	prop = RNA_def_property(srna, "taa_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Eevee_taa_samples_get",
	                               "rna_LayerEngineSettings_Eevee_taa_samples_set", NULL);
	RNA_def_property_ui_text(prop, "Viewport Samples", "Number of temporal samples, unlimited if 0, "
	                                                   "disabled if 1");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Screen Space Subsurface Scattering */
	prop = RNA_def_property(srna, "sss_enable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_sss_enable_get",
	                               "rna_LayerEngineSettings_Eevee_sss_enable_set");
	RNA_def_property_ui_text(prop, "Subsurface Scattering", "Enable screen space subsurface scattering");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "sss_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Eevee_sss_samples_get",
	                               "rna_LayerEngineSettings_Eevee_sss_samples_set", NULL);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples to compute the scattering effect");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "sss_jitter_threshold", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_sss_jitter_threshold_get",
	                               "rna_LayerEngineSettings_Eevee_sss_jitter_threshold_set", NULL);
	RNA_def_property_ui_text(prop, "Jitter Threshold", "Rotate samples that are below this threshold");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "sss_separate_albedo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_sss_separate_albedo_get",
	                               "rna_LayerEngineSettings_Eevee_sss_separate_albedo_set");
	RNA_def_property_ui_text(prop, "Separate Albedo", "Avoid albedo being blured by the subsurface scattering "
	                                                  "but uses more video memory");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Screen Space Reflection */
	prop = RNA_def_property(srna, "ssr_enable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_enable_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_enable_set");
	RNA_def_property_ui_text(prop, "Screen Space Reflections", "Enable screen space reflection");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "ssr_refraction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_refraction_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_refraction_set");
	RNA_def_property_ui_text(prop, "Screen Space Refractions", "Enable screen space Refractions");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "ssr_halfres", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_halfres_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_halfres_set");
	RNA_def_property_ui_text(prop, "Half Res Trace", "Raytrace at a lower resolution");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "ssr_quality", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_quality_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_quality_set", NULL);
	RNA_def_property_ui_text(prop, "Trace Quality", "Quality of the screen space raytracing");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "ssr_max_roughness", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_max_roughness_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_max_roughness_set", NULL);
	RNA_def_property_ui_text(prop, "Max Roughness", "Do not raytrace reflections for roughness above this value");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "ssr_ray_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_ray_count_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_ray_count_set", NULL);
	RNA_def_property_ui_text(prop, "Samples", "Number of rays to trace per pixels");
	RNA_def_property_range(prop, 1, 4);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "ssr_thickness", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_thickness_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_thickness_set", NULL);
	RNA_def_property_ui_text(prop, "Thickness", "Pixel thickness used to detect intersection");
	RNA_def_property_range(prop, 1e-6f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 5, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "ssr_border_fade", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_border_fade_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_border_fade_set", NULL);
	RNA_def_property_ui_text(prop, "Edge Fading", "Screen percentage used to fade the SSR");
	RNA_def_property_range(prop, 0.0f, 0.5f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "ssr_firefly_fac", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_ssr_firefly_fac_get",
	                               "rna_LayerEngineSettings_Eevee_ssr_firefly_fac_set", NULL);
	RNA_def_property_ui_text(prop, "Clamp", "Clamp pixel intensity to remove noise (0 to disabled)");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Volumetrics */
	prop = RNA_def_property(srna, "volumetric_enable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_enable_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_enable_set");
	RNA_def_property_ui_text(prop, "Volumetrics", "Enable scattering and absorbance of volumetric material");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_start", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_start_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_start_set", NULL);
	RNA_def_property_ui_text(prop, "Start", "Start distance of the volumetric effect");
	RNA_def_property_range(prop, 1e-6f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_end", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_end_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_end_set", NULL);
	RNA_def_property_ui_text(prop, "End", "End distance of the volumetric effect");
	RNA_def_property_range(prop, 1e-6f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_tile_size", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_tile_size_get",
	                                  "rna_LayerEngineSettings_Eevee_volumetric_tile_size_set", NULL);
	RNA_def_property_enum_items(prop, eevee_volumetric_tile_size_items);
	RNA_def_property_ui_text(prop, "Tile Size", "Control the quality of the volumetric effects "
	                                            "(lower size increase vram usage and quality)");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_samples_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_samples_set", NULL);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples to compute volumetric effects");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_range(prop, 1, 256);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_sample_distribution", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_sample_distribution_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_sample_distribution_set", NULL);
	RNA_def_property_ui_text(prop, "Exponential Sampling", "Distribute more samples closer to the camera");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_lights", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_lights_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_lights_set");
	RNA_def_property_ui_text(prop, "Volumetric Lighting", "Enable scene lamps interactions with volumetrics");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_light_clamp", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_light_clamp_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_light_clamp_set", NULL);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Clamp", "Maximum light contribution, reducing noise");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_shadows", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_shadows_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_shadows_set");
	RNA_def_property_ui_text(prop, "Volumetric Shadows", "Generate shadows from volumetric material (Very expensive)");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_shadow_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_shadow_samples_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_shadow_samples_set", NULL);
	RNA_def_property_range(prop, 1, 128);
	RNA_def_property_ui_text(prop, "Volumetric Shadow Samples", "Number of samples to compute volumetric shadowing");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "volumetric_colored_transmittance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_volumetric_colored_transmittance_get",
	                               "rna_LayerEngineSettings_Eevee_volumetric_colored_transmittance_set");
	RNA_def_property_ui_text(prop, "Colored Transmittance", "Enable wavelength dependent volumetric transmittance");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Ambient Occlusion */
	prop = RNA_def_property(srna, "gtao_enable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_gtao_enable_get",
	                               "rna_LayerEngineSettings_Eevee_gtao_enable_set");
	RNA_def_property_ui_text(prop, "Ambient Occlusion", "Enable ambient occlusion to simulate medium scale indirect shadowing");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "gtao_use_bent_normals", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_gtao_use_bent_normals_get",
	                               "rna_LayerEngineSettings_Eevee_gtao_use_bent_normals_set");
	RNA_def_property_ui_text(prop, "Bent Normals", "Compute main non occluded direction to sample the environment");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "gtao_denoise", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_gtao_denoise_get",
	                               "rna_LayerEngineSettings_Eevee_gtao_denoise_set");
	RNA_def_property_ui_text(prop, "Denoise", "Use denoising to filter the resulting occlusion and bent normal but exhibit 2x2 pixel blocks");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "gtao_bounce", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_gtao_bounce_get",
	                               "rna_LayerEngineSettings_Eevee_gtao_bounce_set");
	RNA_def_property_ui_text(prop, "Bounces Approximation", "An approximation to simulate light bounces "
	                                                        "giving less occlusion on brighter objects");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "gtao_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_gtao_factor_get", "rna_LayerEngineSettings_Eevee_gtao_factor_set", NULL);
	RNA_def_property_ui_text(prop, "Factor", "Factor for ambient occlusion blending");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 2);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "gtao_quality", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_gtao_quality_get", "rna_LayerEngineSettings_Eevee_gtao_quality_set", NULL);
	RNA_def_property_ui_text(prop, "Trace Quality", "Quality of the horizon search");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "gtao_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_gtao_distance_get", "rna_LayerEngineSettings_Eevee_gtao_distance_set", NULL);
	RNA_def_property_ui_text(prop, "Distance", "Distance of object that contribute to the ambient occlusion effect");
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "gtao_samples", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Eevee_gtao_samples_get",
	                           "rna_LayerEngineSettings_Eevee_gtao_samples_set", NULL);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples to take to compute occlusion");
	RNA_def_property_range(prop, 2, 32);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Depth of Field */
	prop = RNA_def_property(srna, "dof_enable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_dof_enable_get",
	                               "rna_LayerEngineSettings_Eevee_dof_enable_set");
	RNA_def_property_ui_text(prop, "Depth of Field", "Enable depth of field using the values from the active camera");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "bokeh_max_size", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_bokeh_max_size_get",
	                             "rna_LayerEngineSettings_Eevee_bokeh_max_size_set", NULL);
	RNA_def_property_ui_text(prop, "Max Size", "Max size of the bokeh shape for the depth of field (lower is faster)");
	RNA_def_property_range(prop, 0.0f, 2000.0f);
	RNA_def_property_ui_range(prop, 2.0f, 200.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "bokeh_threshold", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_bokeh_threshold_get",
	                             "rna_LayerEngineSettings_Eevee_bokeh_threshold_set", NULL);
	RNA_def_property_ui_text(prop, "Sprite Threshold", "Brightness threshold for using sprite base depth of field");
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Bloom */
	prop = RNA_def_property(srna, "bloom_enable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_bloom_enable_get",
	                               "rna_LayerEngineSettings_Eevee_bloom_enable_set");
	RNA_def_property_ui_text(prop, "Bloom", "High brighness pixels generate a glowing effect");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "bloom_threshold", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_bloom_threshold_get",
	                             "rna_LayerEngineSettings_Eevee_bloom_threshold_set", NULL);
	RNA_def_property_ui_text(prop, "Threshold", "Filters out pixels under this level of brightness");
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "bloom_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_bloom_color_get",
	                             "rna_LayerEngineSettings_Eevee_bloom_color_set", NULL);
	RNA_def_property_ui_text(prop, "Color", "Color applied to the bloom effect");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "bloom_knee", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_bloom_knee_get",
	                             "rna_LayerEngineSettings_Eevee_bloom_knee_set", NULL);
	RNA_def_property_ui_text(prop, "Knee", "Makes transition between under/over-threshold gradual");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "bloom_radius", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_bloom_radius_get",
	                             "rna_LayerEngineSettings_Eevee_bloom_radius_set", NULL);
	RNA_def_property_ui_text(prop, "Radius", "Bloom spread distance");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "bloom_clamp", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_bloom_clamp_get",
	                             "rna_LayerEngineSettings_Eevee_bloom_clamp_set", NULL);
	RNA_def_property_ui_text(prop, "Clamp", "Maximum intensity a bloom pixel can have");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "bloom_intensity", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_bloom_intensity_get",
	                             "rna_LayerEngineSettings_Eevee_bloom_intensity_set", NULL);
	RNA_def_property_ui_text(prop, "Intensity", "Blend factor");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Motion blur */
	prop = RNA_def_property(srna, "motion_blur_enable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_motion_blur_enable_get",
	                               "rna_LayerEngineSettings_Eevee_motion_blur_enable_set");
	RNA_def_property_ui_text(prop, "Motion Blur", "Enable motion blur effect (only in camera view)");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "motion_blur_samples", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_LayerEngineSettings_Eevee_motion_blur_samples_get",
	                           "rna_LayerEngineSettings_Eevee_motion_blur_samples_set", NULL);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples to take with motion blur");
	RNA_def_property_range(prop, 1, 64);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "motion_blur_shutter", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Eevee_motion_blur_shutter_get",
	                             "rna_LayerEngineSettings_Eevee_motion_blur_shutter_set", NULL);
	RNA_def_property_ui_text(prop, "Shutter", "Time taken in frames between shutter open and close");
	RNA_def_property_ui_range(prop, 0.01f, 2.0f, 1, 2);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	/* Shadows */
	prop = RNA_def_property(srna, "shadow_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_LayerEngineSettings_Eevee_shadow_method_get", "rna_LayerEngineSettings_Eevee_shadow_method_set", NULL);
	RNA_def_property_enum_items(prop, eevee_shadow_method_items);
	RNA_def_property_ui_text(prop, "Method", "Technique use to compute the shadows");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "shadow_size", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_LayerEngineSettings_Eevee_shadow_size_get", "rna_LayerEngineSettings_Eevee_shadow_size_set", NULL);
	RNA_def_property_enum_items(prop, eevee_shadow_size_items);
	RNA_def_property_ui_text(prop, "Size", "Size of every shadow maps");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	prop = RNA_def_property(srna, "shadow_high_bitdepth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_Eevee_shadow_high_bitdepth_get", "rna_LayerEngineSettings_Eevee_shadow_high_bitdepth_set");
	RNA_def_property_ui_text(prop, "High Bitdepth", "Use 32bit shadows");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_ViewLayerEngineSettings_update");

	RNA_define_verify_sdna(1); /* not in sdna */
}

#ifdef WITH_CLAY_ENGINE
static void rna_def_layer_collection_engine_settings_clay(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem clay_matcap_items[] = {
	    {ICON_MATCAP_01, "01", ICON_MATCAP_01, "", ""},
	    {ICON_MATCAP_02, "02", ICON_MATCAP_02, "", ""},
	    {ICON_MATCAP_03, "03", ICON_MATCAP_03, "", ""},
	    {ICON_MATCAP_04, "04", ICON_MATCAP_04, "", ""},
	    {ICON_MATCAP_05, "05", ICON_MATCAP_05, "", ""},
	    {ICON_MATCAP_06, "06", ICON_MATCAP_06, "", ""},
	    {ICON_MATCAP_07, "07", ICON_MATCAP_07, "", ""},
	    {ICON_MATCAP_08, "08", ICON_MATCAP_08, "", ""},
	    {ICON_MATCAP_09, "09", ICON_MATCAP_09, "", ""},
	    {ICON_MATCAP_10, "10", ICON_MATCAP_10, "", ""},
	    {ICON_MATCAP_11, "11", ICON_MATCAP_11, "", ""},
	    {ICON_MATCAP_12, "12", ICON_MATCAP_12, "", ""},
	    {ICON_MATCAP_13, "13", ICON_MATCAP_13, "", ""},
	    {ICON_MATCAP_14, "14", ICON_MATCAP_14, "", ""},
	    {ICON_MATCAP_15, "15", ICON_MATCAP_15, "", ""},
	    {ICON_MATCAP_16, "16", ICON_MATCAP_16, "", ""},
	    {ICON_MATCAP_17, "17", ICON_MATCAP_17, "", ""},
	    {ICON_MATCAP_18, "18", ICON_MATCAP_18, "", ""},
	    {ICON_MATCAP_19, "19", ICON_MATCAP_19, "", ""},
	    {ICON_MATCAP_20, "20", ICON_MATCAP_20, "", ""},
	    {ICON_MATCAP_21, "21", ICON_MATCAP_21, "", ""},
	    {ICON_MATCAP_22, "22", ICON_MATCAP_22, "", ""},
	    {ICON_MATCAP_23, "23", ICON_MATCAP_23, "", ""},
	    {ICON_MATCAP_24, "24", ICON_MATCAP_24, "", ""},
	    {0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "LayerCollectionEngineSettingsClay", "LayerCollectionSettings");
	RNA_def_struct_ui_text(srna, "Collections Clay Engine Settings", "Engine specific settings for this collection");

	RNA_define_verify_sdna(0); /* not in sdna */

	/* see RNA_LAYER_ENGINE_GET_SET macro */
	prop = RNA_def_property(srna, "matcap_icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_LayerEngineSettings_Clay_matcap_icon_get", "rna_LayerEngineSettings_Clay_matcap_icon_set", NULL);
	RNA_def_property_enum_items(prop, clay_matcap_items);
	RNA_def_property_ui_text(prop, "Matcap", "Image to use for Material Capture by this material");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "matcap_rotation", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_matcap_rotation_get", "rna_LayerEngineSettings_Clay_matcap_rotation_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Matcap Rotation", "Orientation of the matcap on the model");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "matcap_hue", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_matcap_hue_get", "rna_LayerEngineSettings_Clay_matcap_hue_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Matcap Hue Shift", "Hue correction of the matcap");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "matcap_saturation", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_matcap_saturation_get", "rna_LayerEngineSettings_Clay_matcap_saturation_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Matcap Saturation", "Saturation correction of the matcap");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "matcap_value", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_matcap_value_get", "rna_LayerEngineSettings_Clay_matcap_value_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Matcap Value", "Value correction of the matcap");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "ssao_factor_cavity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_ssao_factor_cavity_get", "rna_LayerEngineSettings_Clay_ssao_factor_cavity_set", NULL);
	RNA_def_property_ui_text(prop, "Cavity Strength", "Strength of the Cavity effect");
	RNA_def_property_range(prop, 0.0f, 250.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "ssao_factor_edge", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_ssao_factor_edge_get", "rna_LayerEngineSettings_Clay_ssao_factor_edge_set", NULL);
	RNA_def_property_ui_text(prop, "Edge Strength", "Strength of the Edge effect");
	RNA_def_property_range(prop, 0.0f, 250.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "ssao_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_ssao_distance_get", "rna_LayerEngineSettings_Clay_ssao_distance_set", NULL);
	RNA_def_property_ui_text(prop, "Distance", "Distance of object that contribute to the Cavity/Edge effect");
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "ssao_attenuation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_ssao_attenuation_get", "rna_LayerEngineSettings_Clay_ssao_attenuation_set", NULL);
	RNA_def_property_ui_text(prop, "Attenuation", "Attenuation constant");
	RNA_def_property_range(prop, 1.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 1.0f, 100.0f, 1, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "hair_brightness_randomness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_Clay_hair_brightness_randomness_get", "rna_LayerEngineSettings_Clay_hair_brightness_randomness_set", NULL);
	RNA_def_property_ui_text(prop, "Hair Brightness Randomness", "Brightness randomness for hair");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	RNA_define_verify_sdna(1); /* not in sdna */
}
#endif /* WITH_CLAY_ENGINE */

static void rna_def_layer_collection_mode_settings_object(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LayerCollectionModeSettingsObject", "LayerCollectionSettings");
	RNA_def_struct_ui_text(srna, "Collections Object Mode Settings", "Object Mode specific settings for this collection");
	RNA_define_verify_sdna(0); /* not in sdna */

	/* see RNA_LAYER_ENGINE_GET_SET macro */

	prop = RNA_def_property(srna, "show_wire", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Wire", "Add the object's wireframe over solid drawing");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_ObjectMode_show_wire_get", "rna_LayerEngineSettings_ObjectMode_show_wire_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "show_backface_culling", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Backface Culling", "");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_ObjectMode_show_backface_culling_get", "rna_LayerEngineSettings_ObjectMode_show_backface_culling_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_layer_collection_mode_settings_edit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LayerCollectionModeSettingsEdit", "LayerCollectionSettings");
	RNA_def_struct_ui_text(srna, "Collections Edit Mode Settings", "Edit Mode specific settings to be overridden per collection");
	RNA_define_verify_sdna(0); /* not in sdna */

	/* see RNA_LAYER_ENGINE_GET_SET macro */

	prop = RNA_def_property(srna, "show_occlude_wire", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Hidden Wire", "Use hidden wireframe display");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_EditMode_show_occlude_wire_get", "rna_LayerEngineSettings_EditMode_show_occlude_wire_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "show_weight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Show Weights", "Draw weights in editmode");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_EditMode_show_weight_get", "rna_LayerEngineSettings_EditMode_show_weight_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "face_normals_show", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Draw Normals", "Display face normals as lines");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_EditMode_face_normals_show_get", "rna_LayerEngineSettings_EditMode_face_normals_show_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "vert_normals_show", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Draw Vertex Normals", "Display vertex normals as lines");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_EditMode_vert_normals_show_get", "rna_LayerEngineSettings_EditMode_vert_normals_show_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "loop_normals_show", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Draw Split Normals", "Display vertex-per-face normals as lines");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_EditMode_loop_normals_show_get", "rna_LayerEngineSettings_EditMode_loop_normals_show_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "normals_length", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_ui_text(prop, "Normal Size", "Display size for normals in the 3D view");
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_EditMode_normals_length_get", "rna_LayerEngineSettings_EditMode_normals_length_set", NULL);
	RNA_def_property_range(prop, 0.00001, 1000.0);
	RNA_def_property_ui_range(prop, 0.01, 10.0, 10.0, 2);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "backwire_opacity", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_ui_text(prop, "Backwire Opacity", "Opacity when rendering transparent wires");
	RNA_def_property_float_funcs(prop, "rna_LayerEngineSettings_EditMode_backwire_opacity_get", "rna_LayerEngineSettings_EditMode_backwire_opacity_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_layer_collection_mode_settings_paint_weight(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LayerCollectionModeSettingsPaintWeight", "LayerCollectionSettings");
	RNA_def_struct_ui_text(srna, "Collections Weight Paint Mode Settings", "Weight Paint Mode specific settings to be overridden per collection");
	RNA_define_verify_sdna(0); /* not in sdna */

	/* see RNA_LAYER_ENGINE_GET_SET macro */

	prop = RNA_def_property(srna, "use_shading", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Use Shading", "Whether to use shaded or shadeless drawing");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_PaintWeightMode_use_shading_get", "rna_LayerEngineSettings_PaintWeightMode_use_shading_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "use_wire", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Show Wire", "Whether to overlay wireframe onto the mesh");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_PaintWeightMode_use_wire_get", "rna_LayerEngineSettings_PaintWeightMode_use_wire_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollectionEngineSettings_wire_update");

	RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_layer_collection_mode_settings_paint_vertex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LayerCollectionModeSettingsPaintVertex", "LayerCollectionSettings");
	RNA_def_struct_ui_text(srna, "Collections Vertex Paint Mode Settings", "Vertex Paint Mode specific settings to be overridden per collection");
	RNA_define_verify_sdna(0); /* not in sdna */

	/* see RNA_LAYER_ENGINE_GET_SET macro */

	prop = RNA_def_property(srna, "use_shading", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Use Shading", "Whether to use shaded or shadeless drawing");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_PaintVertexMode_use_shading_get", "rna_LayerEngineSettings_PaintVertexMode_use_shading_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_LayerCollectionEngineSettings_update");

	prop = RNA_def_property(srna, "use_wire", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Show Wire", "Whether to overlay wireframe onto the mesh");
	RNA_def_property_boolean_funcs(prop, "rna_LayerEngineSettings_PaintVertexMode_use_wire_get", "rna_LayerEngineSettings_PaintVertexMode_use_wire_set");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollectionEngineSettings_wire_update");

	RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_view_layer_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "ViewLayerSettings", NULL);
	RNA_def_struct_sdna(srna, "IDProperty");
	RNA_def_struct_ui_text(srna, "Scene Layer Settings",
	                       "Engine specific settings that can be overriden by ViewLayer");
	RNA_def_struct_refine_func(srna, "rna_ViewLayerSettings_refine");

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ViewLayerSettings_name_get", "rna_ViewLayerSettings_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Engine Name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

	func = RNA_def_function(srna, "use", "rna_ViewLayerSettings_use");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Initialize this property to use");
	parm = RNA_def_string(func, "identifier", NULL, 0, "Property Name", "Name of the property to set");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "unuse", "rna_ViewLayerSettings_unuse");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Remove the property");
	parm = RNA_def_string(func, "identifier", NULL, 0, "Property Name", "Name of the property to unset");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

#ifdef WITH_CLAY_ENGINE
	rna_def_view_layer_engine_settings_clay(brna);
#endif
	rna_def_view_layer_engine_settings_eevee(brna);

#if 0
	rna_def_view_layer_mode_settings_object(brna);
	rna_def_view_layer_mode_settings_edit(brna);
	rna_def_view_layer_mode_settings_paint_weight(brna);
	rna_def_view_layer_mode_settings_paint_vertex(brna);
#endif

	RNA_define_verify_sdna(1);
}

static void rna_def_layer_collection_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "LayerCollectionSettings", NULL);
	RNA_def_struct_sdna(srna, "IDProperty");
	RNA_def_struct_ui_text(srna, "Layer Collection Settings",
	                       "Engine specific settings that can be overriden by LayerCollection");
	RNA_def_struct_refine_func(srna, "rna_LayerCollectionSettings_refine");

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_LayerCollectionSettings_name_get", "rna_LayerCollectionSettings_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Engine Name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

	func = RNA_def_function(srna, "use", "rna_LayerCollectionSettings_use");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Initialize this property to use");
	parm = RNA_def_string(func, "identifier", NULL, 0, "Property Name", "Name of the property to set");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "unuse", "rna_LayerCollectionSettings_unuse");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Remove the property");
	parm = RNA_def_string(func, "identifier", NULL, 0, "Property Name", "Name of the property to unset");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

#ifdef WITH_CLAY_ENGINE
	rna_def_layer_collection_engine_settings_clay(brna);
#endif

	rna_def_layer_collection_mode_settings_object(brna);
	rna_def_layer_collection_mode_settings_edit(brna);
	rna_def_layer_collection_mode_settings_paint_weight(brna);
	rna_def_layer_collection_mode_settings_paint_vertex(brna);

	RNA_define_verify_sdna(1);
}

static void rna_def_layer_collection(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "LayerCollection", NULL);
	RNA_def_struct_ui_text(srna, "Layer Collection", "Layer collection");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_LayerCollection_name_get", "rna_LayerCollection_name_length", "rna_LayerCollection_name_set");
	RNA_def_property_ui_text(prop, "Name", "Collection name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, NULL);

	prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "scene_collection");
	RNA_def_property_struct_type(prop, "SceneCollection");
	RNA_def_property_ui_text(prop, "Collection", "Collection this layer collection is wrapping");

	prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layer_collections", NULL);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_ui_text(prop, "Layer Collections", "");

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_LayerCollection_objects_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Objects", "All the objects directly or indirectly added to this collection (not including sub-collection objects)");

	prop = RNA_def_property(srna, "overrides", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "overrides", NULL);
	RNA_def_property_struct_type(prop, "LayerCollectionOverride");
	RNA_def_property_ui_text(prop, "Collection Overrides", "");

	/* Override settings */
	prop = RNA_def_property(srna, "engine_overrides", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "properties->data.group", NULL);
	RNA_def_property_struct_type(prop, "LayerCollectionSettings");
	RNA_def_property_ui_text(prop, "Collection Settings", "Override of engine specific render settings");

	/* Functions */
	func = RNA_def_function(srna, "move_above", "rna_LayerCollection_move_above");
	RNA_def_function_ui_description(func, "Move collection after another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "lc_dst", "LayerCollection", "Collection", "Reference collection above which the collection will move");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "move_below", "rna_LayerCollection_move_below");
	RNA_def_function_ui_description(func, "Move collection before another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "lc_dst", "LayerCollection", "Collection", "Reference collection below which the collection will move");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "move_into", "rna_LayerCollection_move_into");
	RNA_def_function_ui_description(func, "Move collection into another");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "lc_dst", "LayerCollection", "Collection", "Collection to insert into");
	parm = RNA_def_boolean(func, "result", false, "Result", "Whether the operation succeded");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "enable_set", "rna_LayerCollection_enable_set");
	RNA_def_function_ui_description(func, "Enable or disable a collection");
	parm = RNA_def_boolean(func, "value", 1, "Enable", "");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);

	func = RNA_def_function(srna, "create_group", "rna_LayerCollection_create_group");
	RNA_def_function_ui_description(func, "Enable or disable a collection");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "result", "Group", "", "Newly created Group");
	RNA_def_function_return(func, parm);

	/* Flags */
	prop = RNA_def_property(srna, "is_enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", COLLECTION_DISABLED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Enabled", "Enable or disable collection from depsgraph");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", COLLECTION_VISIBLE);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 1);
	RNA_def_property_ui_text(prop, "Hide", "Restrict visiblity");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollection_flag_update");

	prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", COLLECTION_SELECTABLE);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 1);
	RNA_def_property_ui_text(prop, "Hide Selectable", "Restrict selection");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollection_flag_update");

	/* TODO_LAYER_OVERRIDE */
}

static void rna_def_layer_collections(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "LayerCollections");
	srna = RNA_def_struct(brna, "LayerCollections", NULL);
	RNA_def_struct_sdna(srna, "ViewLayer");
	RNA_def_struct_ui_text(srna, "Layer Collections", "Collections of render layer");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "active_collection");
	RNA_def_property_int_funcs(prop, "rna_LayerCollections_active_collection_index_get",
	                           "rna_LayerCollections_active_collection_index_set",
	                           "rna_LayerCollections_active_collection_index_range");
	RNA_def_property_ui_text(prop, "Active Collection Index", "Active index in layer collection array");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_pointer_funcs(prop, "rna_LayerCollections_active_collection_get",
	                               "rna_LayerCollections_active_collection_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Active Layer Collection", "Active Layer Collection");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	func = RNA_def_function(srna, "link", "rna_ViewLayer_collection_link");
	RNA_def_function_ui_description(func, "Link a collection to render layer");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	parm = RNA_def_pointer(func, "scene_collection", "SceneCollection", "", "Collection to add to render layer");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "result", "LayerCollection", "", "Newly created layer collection");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "unlink", "rna_ViewLayer_collection_unlink");
	RNA_def_function_ui_description(func, "Unlink a collection from render layer");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "layer_collection", "LayerCollection", "", "Layer collection to remove from render layer");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_layer_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "LayerObjects");
	srna = RNA_def_struct(brna, "LayerObjects", NULL);
	RNA_def_struct_sdna(srna, "ViewLayer");
	RNA_def_struct_ui_text(srna, "Layer Objects", "Collections of objects");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, "rna_LayerObjects_active_object_get", "rna_LayerObjects_active_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Object", "Active object for this layer");
	/* Could call: ED_object_base_activate(C, rl->basact);
	 * but would be a bad level call and it seems the notifier is enough */
	RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, NULL);

	prop = RNA_def_property(srna, "selected", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, "rna_LayerObjects_selected_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_ViewLayer_objects_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Selected Objects", "All the selected objects of this layer");
}

static void rna_def_object_base(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ObjectBase", NULL);
	RNA_def_struct_sdna(srna, "Base");
	RNA_def_struct_ui_text(srna, "Object Base", "An object instance in a render layer");
	RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Object", "Object this base links to");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BASE_SELECTED);
	RNA_def_property_ui_text(prop, "Select", "Object base selection state");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ObjectBase_select_update");
}

static void rna_def_scene_view_render(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem engine_items[] = {
		{0, "BLENDER_RENDER", 0, "Blender Render", "Use the Blender internal rendering engine for rendering"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ViewRenderSettings", NULL);
	RNA_def_struct_sdna(srna, "ViewRender");
	RNA_def_struct_path_func(srna, "rna_ViewRenderSettings_path");
	RNA_def_struct_ui_text(srna, "View Render", "Rendering settings related to viewport drawing/rendering");

	/* engine */
	prop = RNA_def_property(srna, "engine", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, engine_items);
	RNA_def_property_enum_funcs(prop, "rna_ViewRenderSettings_engine_get", "rna_ViewRenderSettings_engine_set",
	                            "rna_ViewRenderSettings_engine_itemf");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Engine", "Engine to use for rendering");
	RNA_def_property_update(prop, NC_WINDOW, "rna_ViewRenderSettings_engine_update");

	prop = RNA_def_property(srna, "has_multiple_engines", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ViewRenderSettings_multiple_engines_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Multiple Engines", "More than one rendering engine is available");

	prop = RNA_def_property(srna, "use_shading_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ViewRenderSettings_use_shading_nodes_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Use Shading Nodes", "Active render engine uses new shading nodes system");

	prop = RNA_def_property(srna, "use_spherical_stereo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ViewRenderSettings_use_spherical_stereo_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Use Spherical Stereo", "Active render engine supports spherical stereo rendering");

	prop = RNA_def_property(srna, "use_game_engine", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ViewRenderSettings_use_game_engine_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Use Game Engine", "Current rendering engine is a game engine");
}

void RNA_def_view_layer(BlenderRNA *brna)
{
	FunctionRNA *func;
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ViewLayer", NULL);
	RNA_def_struct_ui_text(srna, "Render Layer", "Render layer");
	RNA_def_struct_ui_icon(srna, ICON_RENDERLAYERS);
	RNA_def_struct_idprops_func(srna, "rna_ViewLayer_idprops");

	rna_def_view_layer_common(srna, 1);

	func = RNA_def_function(srna, "update_render_passes", "rna_ViewLayer_update_render_passes");
	RNA_def_function_ui_description(func, "Requery the enabled render passes from the render engine");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF);

	prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layer_collections", NULL);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_ui_text(prop, "Layer Collections", "");
	rna_def_layer_collections(brna, prop);

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_ViewLayer_objects_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Objects", "All the objects in this layer");
	rna_def_layer_objects(brna, prop);

	/* layer options */
	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VIEW_LAYER_RENDER);
	RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the render layer");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VIEW_LAYER_FREESTYLE);
	RNA_def_property_ui_text(prop, "Freestyle", "Render stylized strokes in this Layer");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	/* Freestyle */
	rna_def_freestyle_settings(brna);

	prop = RNA_def_property(srna, "freestyle_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "freestyle_config");
	RNA_def_property_struct_type(prop, "FreestyleSettings");
	RNA_def_property_ui_text(prop, "Freestyle Settings", "");

	/* Override settings */
	prop = RNA_def_property(srna, "engine_overrides", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "properties->data.group", NULL);
	RNA_def_property_struct_type(prop, "ViewLayerSettings");
	RNA_def_property_ui_text(prop, "Layer Settings", "Override of engine specific render settings");

	/* debug update routine */
	func = RNA_def_function(srna, "update", "rna_ViewLayer_update_tagged");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func,
	                                "Update data tagged to be updated from previous access to data or operators");

	/* Dependency Graph */
	prop = RNA_def_property(srna, "depsgraph", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Depsgraph");
	RNA_def_property_ui_text(prop, "Dependency Graph", "Dependencies in the scene data");
	RNA_def_property_pointer_funcs(prop, "rna_ViewLayer_depsgraph_get", NULL, NULL, NULL);

	/* Nested Data  */
	/* *** Non-Animated *** */
	RNA_define_animate_sdna(false);
	rna_def_scene_collection(brna);
	rna_def_layer_collection(brna);
	rna_def_layer_collection_override(brna);
	rna_def_object_base(brna);
	RNA_define_animate_sdna(true);
	/* *** Animated *** */
	rna_def_view_layer_settings(brna);
	rna_def_layer_collection_settings(brna);
	rna_def_scene_view_render(brna);
}

#endif
