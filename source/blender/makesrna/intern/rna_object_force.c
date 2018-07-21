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
 * Contributor(s): Blender Foundation (2008), Thomas Dinges
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_object_force.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_cloth_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

static const EnumPropertyItem effector_shape_items[] = {
	{PFIELD_SHAPE_POINT, "POINT", 0, "Point", ""},
	{PFIELD_SHAPE_PLANE, "PLANE", 0, "Plane", ""},
	{PFIELD_SHAPE_SURFACE, "SURFACE", 0, "Surface", ""},
	{PFIELD_SHAPE_POINTS, "POINTS", 0, "Every Point", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "BLI_math_base.h"


/* type specific return values only used from functions */
static const EnumPropertyItem curve_shape_items[] = {
	{PFIELD_SHAPE_POINT, "POINT", 0, "Point", ""},
	{PFIELD_SHAPE_PLANE, "PLANE", 0, "Plane", ""},
	{PFIELD_SHAPE_SURFACE, "SURFACE", 0, "Curve", ""},
	{0, NULL, 0, NULL, NULL}
};

static const EnumPropertyItem empty_shape_items[] = {
	{PFIELD_SHAPE_POINT, "POINT", 0, "Point", ""},
	{PFIELD_SHAPE_PLANE, "PLANE", 0, "Plane", ""},
	{0, NULL, 0, NULL, NULL}
};

static const EnumPropertyItem vortex_shape_items[] = {
	{PFIELD_SHAPE_POINT, "POINT", 0, "Point", ""},
	{PFIELD_SHAPE_PLANE, "PLANE", 0, "Plane", ""},
	{PFIELD_SHAPE_SURFACE, "SURFACE", 0, "Surface falloff (New)", ""},
	{PFIELD_SHAPE_POINTS, "POINTS", 0, "Every Point (New)", ""},
	{0, NULL, 0, NULL, NULL}
};

static const EnumPropertyItem curve_vortex_shape_items[] = {
	{PFIELD_SHAPE_POINT, "POINT", 0, "Point", ""},
	{PFIELD_SHAPE_PLANE, "PLANE", 0, "Plane", ""},
	{PFIELD_SHAPE_SURFACE, "SURFACE", 0, "Curve (New)", ""},
	{0, NULL, 0, NULL, NULL}
};

static const EnumPropertyItem empty_vortex_shape_items[] = {
	{PFIELD_SHAPE_POINT, "POINT", 0, "Point", ""},
	{PFIELD_SHAPE_PLANE, "PLANE", 0, "Plane", ""},
	{0, NULL, 0, NULL, NULL}
};

#include "MEM_guardedalloc.h"

#include "DNA_modifier_types.h"
#include "DNA_texture_types.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_object.h"

static void rna_Cache_change(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointCache *cache = (PointCache *)ptr->data;

	if (!ob)
		return;

	cache->flag |= PTCACHE_OUTDATED;

	PTCacheID pid = BKE_ptcache_id_find(ob, NULL, cache);

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);

	if (pid.cache) {
		/* Just make sure this wasn't changed. */
		if (pid.type == PTCACHE_TYPE_SMOKE_DOMAIN)
			cache->step = 1;
		BKE_ptcache_update_info(&pid);
	}
}

static void rna_Cache_toggle_disk_cache(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointCache *cache = (PointCache *)ptr->data;

	if (!ob)
		return;

	PTCacheID pid = BKE_ptcache_id_find(ob, NULL, cache);

	/* smoke can only use disk cache */
	if (pid.cache && pid.type != PTCACHE_TYPE_SMOKE_DOMAIN)
		BKE_ptcache_toggle_disk_cache(&pid);
	else
		cache->flag ^= PTCACHE_DISK_CACHE;
}

static void rna_Cache_idname_change(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointCache *cache = (PointCache *)ptr->data;
	bool use_new_name = true;

	if (!ob)
		return;

	/* TODO: check for proper characters */

	if (cache->flag & PTCACHE_EXTERNAL) {
		PTCacheID pid = BKE_ptcache_id_find(ob, NULL, cache);

		if (pid.cache) {
			BKE_ptcache_load_external(&pid);
		}

		DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_main_add_notifier(NC_OBJECT | ND_POINTCACHE, ob);
	}
	else {
		PTCacheID *pid = NULL, *pid2 = NULL;
		ListBase pidlist;

		BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);

		for (pid = pidlist.first; pid; pid = pid->next) {
			if (pid->cache == cache)
				pid2 = pid;
			else if (cache->name[0] != '\0' && STREQ(cache->name, pid->cache->name)) {
				/*TODO: report "name exists" to user */
				BLI_strncpy(cache->name, cache->prev_name, sizeof(cache->name));
				use_new_name = false;
			}
		}

		if (use_new_name) {
			BLI_filename_make_safe(cache->name);

			if (pid2 && cache->flag & PTCACHE_DISK_CACHE) {
				char old_name[80];
				char new_name[80];

				BLI_strncpy(old_name, cache->prev_name, sizeof(old_name));
				BLI_strncpy(new_name, cache->name, sizeof(new_name));

				BKE_ptcache_disk_cache_rename(pid2, old_name, new_name);
			}

			BLI_strncpy(cache->prev_name, cache->name, sizeof(cache->prev_name));
		}

		BLI_freelistN(&pidlist);
	}
}

static void rna_Cache_list_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	PointCache *cache = ptr->data;
	ListBase lb;

	while (cache->prev)
		cache = cache->prev;

	lb.first = cache;
	lb.last = NULL; /* not used by listbase_begin */

	rna_iterator_listbase_begin(iter, &lb, NULL);
}
static void rna_Cache_active_point_cache_index_range(PointerRNA *ptr, int *min, int *max,
                                                     int *UNUSED(softmin), int *UNUSED(softmax))
{
	Object *ob = ptr->id.data;
	PointCache *cache = ptr->data;
	PTCacheID pid = BKE_ptcache_id_find(ob, NULL, cache);

	*min = 0;
	*max = 0;

	if (pid.cache) {
		*max = max_ii(0, BLI_listbase_count(pid.ptcaches) - 1);
	}
}

static int rna_Cache_active_point_cache_index_get(PointerRNA *ptr)
{
	Object *ob = ptr->id.data;
	PointCache *cache = ptr->data;
	PTCacheID pid = BKE_ptcache_id_find(ob, NULL, cache);
	int num = 0;

	if (pid.cache) {
		num = BLI_findindex(pid.ptcaches, cache);
	}

	return num;
}

static void rna_Cache_active_point_cache_index_set(struct PointerRNA *ptr, int value)
{
	Object *ob = ptr->id.data;
	PointCache *cache = ptr->data;
	PTCacheID pid = BKE_ptcache_id_find(ob, NULL, cache);

	if (pid.cache) {
		*(pid.cache_ptr) = BLI_findlink(pid.ptcaches, value);
	}
}

static void rna_PointCache_frame_step_range(PointerRNA *ptr, int *min, int *max,
                                            int *UNUSED(softmin), int *UNUSED(softmax))
{
	Object *ob = ptr->id.data;
	PointCache *cache = ptr->data;
	PTCacheID pid = BKE_ptcache_id_find(ob, NULL, cache);

	*min = 1;
	*max = 20;

	if (pid.cache) {
		*max = pid.max_step;
	}
}

static char *rna_CollisionSettings_path(PointerRNA *UNUSED(ptr))
{
	/* both methods work ok, but return the shorter path */
#if 0
	Object *ob = (Object *)ptr->id.data;
	ModifierData *md = (ModifierData *)modifiers_findByType(ob, eModifierType_Collision);

	if (md) {
		char name_esc[sizeof(md->name) * 2];

		BLI_strescape(name_esc, md->name, sizeof(name_esc));
		return BLI_sprintfN("modifiers[\"%s\"].settings", name_esc);
	}
	else {
		return BLI_strdup("");
	}
#else
	/* more reliable */
	return BLI_strdup("collision");
#endif
}

static bool rna_SoftBodySettings_use_edges_get(PointerRNA *ptr)
{
	Object *data = (Object *)(ptr->id.data);
	return (((data->softflag) & OB_SB_EDGES) != 0);
}

static void rna_SoftBodySettings_use_edges_set(PointerRNA *ptr, bool value)
{
	Object *data = (Object *)(ptr->id.data);
	if (value) data->softflag |= OB_SB_EDGES;
	else data->softflag &= ~OB_SB_EDGES;
}

static bool rna_SoftBodySettings_use_goal_get(PointerRNA *ptr)
{
	Object *data = (Object *)(ptr->id.data);
	return (((data->softflag) & OB_SB_GOAL) != 0);
}

static void rna_SoftBodySettings_use_goal_set(PointerRNA *ptr, bool value)
{
	Object *data = (Object *)(ptr->id.data);
	if (value) data->softflag |= OB_SB_GOAL;
	else data->softflag &= ~OB_SB_GOAL;
}

static bool rna_SoftBodySettings_stiff_quads_get(PointerRNA *ptr)
{
	Object *data = (Object *)(ptr->id.data);
	return (((data->softflag) & OB_SB_QUADS) != 0);
}

static void rna_SoftBodySettings_stiff_quads_set(PointerRNA *ptr, bool value)
{
	Object *data = (Object *)(ptr->id.data);
	if (value) data->softflag |= OB_SB_QUADS;
	else data->softflag &= ~OB_SB_QUADS;
}

static bool rna_SoftBodySettings_self_collision_get(PointerRNA *ptr)
{
	Object *data = (Object *)(ptr->id.data);
	return (((data->softflag) & OB_SB_SELF) != 0);
}

static void rna_SoftBodySettings_self_collision_set(PointerRNA *ptr, bool value)
{
	Object *data = (Object *)(ptr->id.data);
	if (value) data->softflag |= OB_SB_SELF;
	else data->softflag &= ~OB_SB_SELF;
}

static int rna_SoftBodySettings_new_aero_get(PointerRNA *ptr)
{
	Object *data = (Object *)(ptr->id.data);
	if (data->softflag & OB_SB_AERO_ANGLE)
		return 1;
	else
		return 0;
}

static void rna_SoftBodySettings_new_aero_set(PointerRNA *ptr, int value)
{
	Object *data = (Object *)(ptr->id.data);
	if (value == 1)
		data->softflag |= OB_SB_AERO_ANGLE;
	else    /* value == 0 */
		data->softflag &= ~OB_SB_AERO_ANGLE;
}

static bool rna_SoftBodySettings_face_collision_get(PointerRNA *ptr)
{
	Object *data = (Object *)(ptr->id.data);
	return (((data->softflag) & OB_SB_FACECOLL) != 0);
}

static void rna_SoftBodySettings_face_collision_set(PointerRNA *ptr, bool value)
{
	Object *data = (Object *)(ptr->id.data);
	if (value) data->softflag |= OB_SB_FACECOLL;
	else data->softflag &= ~OB_SB_FACECOLL;
}

static bool rna_SoftBodySettings_edge_collision_get(PointerRNA *ptr)
{
	Object *data = (Object *)(ptr->id.data);
	return (((data->softflag) & OB_SB_EDGECOLL) != 0);
}

static void rna_SoftBodySettings_edge_collision_set(PointerRNA *ptr, bool value)
{
	Object *data = (Object *)(ptr->id.data);
	if (value) data->softflag |= OB_SB_EDGECOLL;
	else data->softflag &= ~OB_SB_EDGECOLL;
}

static void rna_SoftBodySettings_goal_vgroup_get(PointerRNA *ptr, char *value)
{
	SoftBody *sb = (SoftBody *)ptr->data;
	rna_object_vgroup_name_index_get(ptr, value, sb->vertgroup);
}

static int rna_SoftBodySettings_goal_vgroup_length(PointerRNA *ptr)
{
	SoftBody *sb = (SoftBody *)ptr->data;
	return rna_object_vgroup_name_index_length(ptr, sb->vertgroup);
}

static void rna_SoftBodySettings_goal_vgroup_set(PointerRNA *ptr, const char *value)
{
	SoftBody *sb = (SoftBody *)ptr->data;
	rna_object_vgroup_name_index_set(ptr, value, &sb->vertgroup);
}

static void rna_SoftBodySettings_mass_vgroup_set(PointerRNA *ptr, const char *value)
{
	SoftBody *sb = (SoftBody *)ptr->data;
	rna_object_vgroup_name_set(ptr, value, sb->namedVG_Mass, sizeof(sb->namedVG_Mass));
}

static void rna_SoftBodySettings_spring_vgroup_set(PointerRNA *ptr, const char *value)
{
	SoftBody *sb = (SoftBody *)ptr->data;
	rna_object_vgroup_name_set(ptr, value, sb->namedVG_Spring_K, sizeof(sb->namedVG_Spring_K));
}


static char *rna_SoftBodySettings_path(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	ModifierData *md = (ModifierData *)modifiers_findByType(ob, eModifierType_Softbody);
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"].settings", name_esc);
}

static int particle_id_check(PointerRNA *ptr)
{
	ID *id = ptr->id.data;

	return (GS(id->name) == ID_PA);
}

static void rna_FieldSettings_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	if (particle_id_check(ptr)) {
		ParticleSettings *part = (ParticleSettings *)ptr->id.data;

		if (part->pd->forcefield != PFIELD_TEXTURE && part->pd->tex) {
			id_us_min(&part->pd->tex->id);
			part->pd->tex = NULL;
		}

		if (part->pd2 && part->pd2->forcefield != PFIELD_TEXTURE && part->pd2->tex) {
			id_us_min(&part->pd2->tex->id);
			part->pd2->tex = NULL;
		}

		DEG_id_tag_update(&part->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME | PSYS_RECALC_RESET);
		WM_main_add_notifier(NC_OBJECT | ND_DRAW, NULL);

	}
	else {
		Object *ob = (Object *)ptr->id.data;

		if (ob->pd->forcefield != PFIELD_TEXTURE && ob->pd->tex) {
			id_us_min(&ob->pd->tex->id);
			ob->pd->tex = NULL;
		}

		DEG_id_tag_update(&ob->id, OB_RECALC_OB);
		WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
	}
}

static void rna_FieldSettings_shape_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	if (!particle_id_check(ptr)) {
		Object *ob = (Object *)ptr->id.data;
		ED_object_check_force_modifiers(bmain, scene, ob);
		WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
		WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
	}
}

static void rna_FieldSettings_type_set(PointerRNA *ptr, int value)
{
	PartDeflect *part_deflect = (PartDeflect *) ptr->data;

	part_deflect->forcefield = value;

	if (!particle_id_check(ptr)) {
		Object *ob = (Object *)ptr->id.data;
		ob->pd->forcefield = value;
		if (ELEM(value, PFIELD_WIND, PFIELD_VORTEX)) {
			ob->empty_drawtype = OB_SINGLE_ARROW;
		}
		else {
			ob->empty_drawtype = OB_PLAINAXES;
		}
	}
}

static void rna_FieldSettings_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	if (particle_id_check(ptr)) {
		DEG_id_tag_update((ID *)ptr->id.data, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME | PSYS_RECALC_RESET);
	}
	else {
		Object *ob = (Object *)ptr->id.data;

		/* do this before scene sort, that one checks for CU_PATH */
#if 0 /* XXX */
		if (ob->type == OB_CURVE && ob->pd->forcefield == PFIELD_GUIDE) {
			Curve *cu = ob->data;
			cu->flag |= (CU_PATH | CU_3D);
			do_curvebuts(B_CU3D);  /* all curves too */
		}
#endif

		rna_FieldSettings_shape_update(bmain, scene, ptr);

		DEG_relations_tag_update(bmain);

		if (ob->type == OB_CURVE && ob->pd->forcefield == PFIELD_GUIDE)
			DEG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		else
			DEG_id_tag_update(&ob->id, OB_RECALC_OB);

		WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
	}
}

static char *rna_FieldSettings_path(PointerRNA *ptr)
{
	PartDeflect *pd = (PartDeflect *)ptr->data;

	/* Check through all possible places the settings can be to find the right one */

	if (particle_id_check(ptr)) {
		/* particle system force field */
		ParticleSettings *part = (ParticleSettings *)ptr->id.data;

		if (part->pd == pd)
			return BLI_sprintfN("force_field_1");
		else if (part->pd2 == pd)
			return BLI_sprintfN("force_field_2");
	}
	else {
		/* object force field */
		Object *ob = (Object *)ptr->id.data;

		if (ob->pd == pd)
			return BLI_sprintfN("field");
	}
	return NULL;
}

static void rna_EffectorWeight_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;

	if (id && GS(id->name) == ID_SCE) {
		Scene *scene = (Scene *)id;
		FOREACH_SCENE_OBJECT_BEGIN(scene, ob)
		{
			BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_DEPSGRAPH);
		}
		FOREACH_SCENE_OBJECT_END;
	}
	else {
		DEG_id_tag_update(id, OB_RECALC_DATA | PSYS_RECALC_RESET);
		WM_main_add_notifier(NC_OBJECT | ND_DRAW, NULL);
	}
}

static void rna_EffectorWeight_dependency_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	DEG_relations_tag_update(bmain);

	DEG_id_tag_update((ID *)ptr->id.data, OB_RECALC_DATA | PSYS_RECALC_RESET);

	WM_main_add_notifier(NC_OBJECT | ND_DRAW, NULL);
}

static char *rna_EffectorWeight_path(PointerRNA *ptr)
{
	EffectorWeights *ew = (EffectorWeights *)ptr->data;
	/* Check through all possible places the settings can be to find the right one */

	if (particle_id_check(ptr)) {
		/* particle effector weights */
		ParticleSettings *part = (ParticleSettings *)ptr->id.data;

		if (part->effector_weights == ew)
			return BLI_sprintfN("effector_weights");
	}
	else {
		Object *ob = (Object *)ptr->id.data;
		ModifierData *md;

		/* check softbody modifier */
		md = (ModifierData *)modifiers_findByType(ob, eModifierType_Softbody);
		if (md) {
			/* no pointer from modifier data to actual softbody storage, would be good to add */
			if (ob->soft->effector_weights == ew) {
				char name_esc[sizeof(md->name) * 2];
				BLI_strescape(name_esc, md->name, sizeof(name_esc));
				return BLI_sprintfN("modifiers[\"%s\"].settings.effector_weights", name_esc);
			}
		}

		/* check cloth modifier */
		md = (ModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
		if (md) {
			ClothModifierData *cmd = (ClothModifierData *)md;
			if (cmd->sim_parms->effector_weights == ew) {
				char name_esc[sizeof(md->name) * 2];
				BLI_strescape(name_esc, md->name, sizeof(name_esc));
				return BLI_sprintfN("modifiers[\"%s\"].settings.effector_weights", name_esc);
			}
		}

		/* check smoke modifier */
		md = (ModifierData *)modifiers_findByType(ob, eModifierType_Smoke);
		if (md) {
			SmokeModifierData *smd = (SmokeModifierData *)md;
			if (smd->domain->effector_weights == ew) {
				char name_esc[sizeof(md->name) * 2];
				BLI_strescape(name_esc, md->name, sizeof(name_esc));
				return BLI_sprintfN("modifiers[\"%s\"].settings.effector_weights", name_esc);
			}
		}

		/* check dynamic paint modifier */
		md = (ModifierData *)modifiers_findByType(ob, eModifierType_DynamicPaint);
		if (md) {
			DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

			if (pmd->canvas) {
				DynamicPaintSurface *surface = pmd->canvas->surfaces.first;

				for (; surface; surface = surface->next) {
					if (surface->effector_weights == ew) {
						char name_esc[sizeof(md->name) * 2];
						char name_esc_surface[sizeof(surface->name) * 2];

						BLI_strescape(name_esc, md->name, sizeof(name_esc));
						BLI_strescape(name_esc_surface, surface->name, sizeof(name_esc_surface));
						return BLI_sprintfN("modifiers[\"%s\"].canvas_settings.canvas_surfaces[\"%s\"]"
						                    ".effector_weights", name_esc, name_esc_surface);
					}
				}
			}
		}
	}
	return NULL;
}

static void rna_CollisionSettings_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	ModifierData *md = modifiers_findByType(ob, eModifierType_Collision);

	/* add/remove modifier as needed */
	if (ob->pd->deflect && !md)
		ED_object_modifier_add(NULL, bmain, scene, ob, NULL, eModifierType_Collision);
	else if (!ob->pd->deflect && md)
		ED_object_modifier_remove(NULL, bmain, ob, md);

	WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_CollisionSettings_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;

	DEG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
	WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_softbody_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
}

static void rna_softbody_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DEG_relations_tag_update(bmain);
	rna_softbody_update(bmain, scene, ptr);
}

static const EnumPropertyItem *rna_Effector_shape_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                  PropertyRNA *UNUSED(prop), bool *UNUSED(r_free))
{
	Object *ob = NULL;

	if (particle_id_check(ptr))
		return empty_shape_items;

	ob = (Object *)ptr->id.data;

	if (ob->type == OB_CURVE) {
		if (ob->pd->forcefield == PFIELD_VORTEX)
			return curve_vortex_shape_items;

		return curve_shape_items;
	}
	else if (ELEM(ob->type, OB_MESH, OB_SURF, OB_FONT)) {
		if (ob->pd->forcefield == PFIELD_VORTEX)
			return vortex_shape_items;

		return effector_shape_items;
	}
	else {
		if (ob->pd->forcefield == PFIELD_VORTEX)
			return empty_vortex_shape_items;

		return empty_shape_items;
	}
}

#else

static void rna_def_pointcache_common(StructRNA *srna)
{
	PropertyRNA *prop;

	static const EnumPropertyItem point_cache_compress_items[] = {
		{PTCACHE_COMPRESS_NO, "NO", 0, "No", "No compression"},
		{PTCACHE_COMPRESS_LZO, "LIGHT", 0, "Light", "Fast but not so effective compression"},
		{PTCACHE_COMPRESS_LZMA, "HEAVY", 0, "Heavy", "Effective but slow compression"},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startframe");
	RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
	RNA_def_property_ui_range(prop, 1, MAXFRAME, 1, 1);
	RNA_def_property_ui_text(prop, "Start", "Frame on which the simulation starts");

	prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "endframe");
	RNA_def_property_range(prop, 1, MAXFRAME);
	RNA_def_property_ui_text(prop, "End", "Frame on which the simulation stops");

	prop = RNA_def_property(srna, "frame_step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "step");
	RNA_def_property_range(prop, 1, 20);
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_PointCache_frame_step_range");
	RNA_def_property_ui_text(prop, "Cache Step", "Number of frames between cached frames");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_change");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "index");
	RNA_def_property_range(prop, -1, 100);
	RNA_def_property_ui_text(prop, "Cache Index", "Index number of cache files");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");

	prop = RNA_def_property(srna, "compression", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, point_cache_compress_items);
	RNA_def_property_ui_text(prop, "Cache Compression", "Compression method to be used");

	/* flags */
	prop = RNA_def_property(srna, "is_baked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_BAKED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "is_baking", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_BAKING);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_disk_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_DISK_CACHE);
	RNA_def_property_ui_text(prop, "Disk Cache", "Save cache files to disk (.blend file must be saved first)");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_toggle_disk_cache");

	prop = RNA_def_property(srna, "is_outdated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_OUTDATED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Cache is outdated", "");

	prop = RNA_def_property(srna, "is_frame_skip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_FRAMES_SKIPPED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Cache name");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "path");
	RNA_def_property_ui_text(prop, "File Path", "Cache file path");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");

	/* removed, see PTCACHE_QUICK_CACHE */
#if 0
	prop = RNA_def_property(srna, "use_quick_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_QUICK_CACHE);
	RNA_def_property_ui_text(prop, "Quick Cache", "Update simulation with cache steps");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_change");
#endif

	prop = RNA_def_property(srna, "info", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "info");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Cache Info", "Info on current cache status");

	prop = RNA_def_property(srna, "use_external", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_EXTERNAL);
	RNA_def_property_ui_text(prop, "External", "Read cache from an external location");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");

	prop = RNA_def_property(srna, "use_library_path", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", PTCACHE_IGNORE_LIBPATH);
	RNA_def_property_ui_text(prop, "Library Path",
	                         "Use this file's path for the disk cache when library linked into another file "
	                         "(for local bakes per scene file, disable this option)");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");
}

static void rna_def_ptcache_point_caches(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* FunctionRNA *func; */
	/* PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "PointCaches");
	srna = RNA_def_struct(brna, "PointCaches", NULL);
	RNA_def_struct_sdna(srna, "PointCache");
	RNA_def_struct_ui_text(srna, "Point Caches", "Collection of point caches");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Cache_active_point_cache_index_get",
	                           "rna_Cache_active_point_cache_index_set",
	                           "rna_Cache_active_point_cache_index_range");
	RNA_def_property_ui_text(prop, "Active Point Cache Index", "");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_change");

	/* And define another RNA type for those collection items. */
	srna = RNA_def_struct(brna, "PointCacheItem", NULL);
	RNA_def_struct_sdna(srna, "PointCache");
	RNA_def_struct_ui_text(srna, "Point Cache", "point cache for physics simulations");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);

	rna_def_pointcache_common(srna);
}

static void rna_def_pointcache_active(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "PointCache", NULL);
	RNA_def_struct_ui_text(srna, "Active Point Cache", "Active point cache for physics simulations");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);

	rna_def_pointcache_common(srna);

	/* This first-level RNA pointer also has list of all caches from owning ID.
	 * Those caches items have exact same content as 'active' one, except for that collection,
	 * to prevent ugly recursive layout pattern.
	 * Note: This shall probably be redone from scratch in a proper way at some poitn, but for now that will do,
	 *       and shall not break anything in the API. */
	prop = RNA_def_property(srna, "point_caches", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_Cache_list_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "PointCacheItem");
	RNA_def_property_ui_text(prop, "Point Cache List", "");
	rna_def_ptcache_point_caches(brna, prop);
}

static void rna_def_collision(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CollisionSettings", NULL);
	RNA_def_struct_sdna(srna, "PartDeflect");
	RNA_def_struct_path_func(srna, "rna_CollisionSettings_path");
	RNA_def_struct_ui_text(srna, "Collision Settings", "Collision settings for object in physics simulation");

	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deflect", 1);
	RNA_def_property_ui_text(prop, "Enabled", "Enable this objects as a collider for physics systems");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_dependency_update");

	/* Particle Interaction */

	prop = RNA_def_property(srna, "damping_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_damp");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Damping Factor", "Amount of damping during particle collision");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "damping_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_rdamp");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Damping", "Random variation of damping");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "friction_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_frict");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Friction Factor", "Amount of friction during particle collision");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "friction_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_rfrict");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Friction", "Random variation of friction");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "permeability", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_perm");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Permeability", "Chance that the particle will pass through the mesh");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "use_particle_kill", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PDEFLE_KILL_PART);
	RNA_def_property_ui_text(prop, "Kill Particles", "Kill collided particles");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "stickiness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_stickness");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Stickiness", "Amount of stickiness to surface collision");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	/* Soft Body and Cloth Interaction */

	prop = RNA_def_property(srna, "thickness_inner", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_sbift");
	RNA_def_property_range(prop, 0.001f, 1.0f);
	RNA_def_property_ui_text(prop, "Inner Thickness", "Inner face thickness (only used by softbodies)");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "thickness_outer", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_sboft");
	RNA_def_property_range(prop, 0.001f, 1.0f);
	RNA_def_property_ui_text(prop, "Outer Thickness", "Outer face thickness");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_sbdamp");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Damping", "Amount of damping during collision");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");

	prop = RNA_def_property(srna, "absorption", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 2);
	RNA_def_property_ui_text(prop, "Absorption",
	                         "How much of effector force gets lost during collision with this object (in percent)");
	RNA_def_property_update(prop, 0, "rna_CollisionSettings_update");
}

static void rna_def_effector_weight(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "EffectorWeights", NULL);
	RNA_def_struct_sdna(srna, "EffectorWeights");
	RNA_def_struct_path_func(srna, "rna_EffectorWeight_path");
	RNA_def_struct_ui_text(srna, "Effector Weights", "Effector weights for physics simulation");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);

	/* Flags */
	prop = RNA_def_property(srna, "apply_to_hair_growing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", EFF_WEIGHT_DO_HAIR);
	RNA_def_property_ui_text(prop, "Use For Growing Hair", "Use force fields when growing hair");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	/* General */
	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Collection");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Collection", "Limit effectors to this collection");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_dependency_update");

	prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "global_gravity");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Gravity", "Global gravity weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	/* Effector weights */
	prop = RNA_def_property(srna, "all", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[0]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "All", "All effector's weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "force", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[1]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Force", "Force effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "vortex", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[2]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Vortex", "Vortex effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "magnetic", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[3]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Magnetic", "Magnetic effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "wind", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[4]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Wind", "Wind effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "curve_guide", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[5]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Curve Guide", "Curve guide effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "texture", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[6]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Texture", "Texture effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "harmonic", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[7]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Harmonic", "Harmonic effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "charge", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[8]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Charge", "Charge effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "lennardjones", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[9]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Lennard-Jones", "Lennard-Jones effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "boid", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[10]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Boid", "Boid effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[11]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Turbulence", "Turbulence effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "drag", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[12]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Drag", "Drag effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");

	prop = RNA_def_property(srna, "smokeflow", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight[13]");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Smoke Flow", "Smoke Flow effector weight");
	RNA_def_property_update(prop, 0, "rna_EffectorWeight_update");
}

static void rna_def_field(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem field_type_items[] = {
		{0, "NONE", 0, "None", ""},
		{PFIELD_FORCE, "FORCE", ICON_FORCE_FORCE, "Force", "Radial field toward the center of object"},
		{PFIELD_WIND, "WIND", ICON_FORCE_WIND, "Wind", "Constant force along the force object's local Z axis"},
		{PFIELD_VORTEX, "VORTEX", ICON_FORCE_VORTEX, "Vortex",
		                "Spiraling force that twists the force object's local Z axis"},
		{PFIELD_MAGNET, "MAGNET", ICON_FORCE_MAGNETIC, "Magnetic", "Forcefield depends on the speed of the particles"},
		{PFIELD_HARMONIC, "HARMONIC", ICON_FORCE_HARMONIC, "Harmonic",
		                  "The source of this force field is the zero point of a harmonic oscillator"},
		{PFIELD_CHARGE, "CHARGE", ICON_FORCE_CHARGE, "Charge",
		                "Spherical forcefield based on the charge of particles, "
		                "only influences other charge force fields"},
		{PFIELD_LENNARDJ, "LENNARDJ", ICON_FORCE_LENNARDJONES, "Lennard-Jones",
		                  "Forcefield based on the Lennard-Jones potential"},
		{PFIELD_TEXTURE, "TEXTURE", ICON_FORCE_TEXTURE, "Texture", "Forcefield based on a texture"},
		{PFIELD_GUIDE, "GUIDE", ICON_FORCE_CURVE, "Curve Guide", "Create a force along a curve object"},
		{PFIELD_BOID, "BOID", ICON_FORCE_BOID, "Boid", ""},
		{PFIELD_TURBULENCE, "TURBULENCE", ICON_FORCE_TURBULENCE, "Turbulence", "Create turbulence with a noise field"},
		{PFIELD_DRAG, "DRAG", ICON_FORCE_DRAG, "Drag", "Create a force that dampens motion"},
		{PFIELD_SMOKEFLOW, "SMOKE_FLOW", ICON_FORCE_SMOKEFLOW, "Smoke Flow", "Create a force based on smoke simulation air flow"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem falloff_items[] = {
		{PFIELD_FALL_SPHERE, "SPHERE", 0, "Sphere", ""},
		{PFIELD_FALL_TUBE, "TUBE", 0, "Tube", ""},
		{PFIELD_FALL_CONE, "CONE", 0, "Cone", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem texture_items[] = {
		{PFIELD_TEX_RGB, "RGB", 0, "RGB", ""},
		{PFIELD_TEX_GRAD, "GRADIENT", 0, "Gradient", ""},
		{PFIELD_TEX_CURL, "CURL", 0, "Curl", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem zdirection_items[] = {
		{PFIELD_Z_BOTH, "BOTH", 0, "Both Z", ""},
		{PFIELD_Z_POS, "POSITIVE", 0, "+Z", ""},
		{PFIELD_Z_NEG, "NEGATIVE", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem guide_kink_items[] = {
		{0, "NONE", 0, "Nothing", ""},
		{1, "CURL", 0, "Curl", ""},
		{2, "RADIAL", 0, "Radial", ""},
		{3, "WAVE", 0, "Wave", ""},
		{4, "BRAID", 0, "Braid", ""},
		{5, "ROTATION", 0, "Rotation", ""},
		{6, "ROLL", 0, "Roll", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "FieldSettings", NULL);
	RNA_def_struct_sdna(srna, "PartDeflect");
	RNA_def_struct_path_func(srna, "rna_FieldSettings_path");
	RNA_def_struct_ui_text(srna, "Field Settings", "Field settings for an object in physics simulation");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);

	/* Enums */

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "forcefield");
	RNA_def_property_enum_items(prop, field_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_FieldSettings_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "Type of field");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_dependency_update");

	prop = RNA_def_property(srna, "shape", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, effector_shape_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Effector_shape_itemf");
	RNA_def_property_ui_text(prop, "Shape", "Which direction is used to calculate the effector force");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_shape_update");

	prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "falloff");
	RNA_def_property_enum_items(prop, falloff_items);
	RNA_def_property_ui_text(prop, "Fall-Off", "");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "texture_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tex_mode");
	RNA_def_property_enum_items(prop, texture_items);
	RNA_def_property_ui_text(prop, "Texture Mode",
	                         "How the texture effect is calculated (RGB & Curl need a RGB texture, "
	                         "else Gradient will be used instead)");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "z_direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "zdir");
	RNA_def_property_enum_items(prop, zdirection_items);
	RNA_def_property_ui_text(prop, "Z Direction", "Effect in full or only positive/negative Z direction");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/* Float */

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_strength");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Strength", "Strength of force field");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/* different ui range to above */
	prop = RNA_def_property(srna, "linear_drag", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_strength");
	RNA_def_property_range(prop, -2.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Linear Drag", "Drag component proportional to velocity");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "harmonic_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_damp");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Harmonic Damping", "Damping of the harmonic force");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/* different ui range to above */
	prop = RNA_def_property(srna, "quadratic_drag", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_damp");
	RNA_def_property_range(prop, -2.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Quadratic Drag", "Drag component proportional to the square of velocity");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "flow", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_flow");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Flow", "Convert effector force into air flow velocity");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/* different ui range to above */
	prop = RNA_def_property(srna, "inflow", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_flow");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Inflow", "Inwards component of the vortex force");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_size");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1.0f, 3);
	RNA_def_property_ui_text(prop, "Size", "Size of the turbulence");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "rest_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_size");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Rest Length", "Rest length of the harmonic force");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "falloff_power", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_power");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Falloff Power", "");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "distance_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mindist");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Minimum Distance", "Minimum distance for the field's fall-off");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "distance_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxdist");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1000.0f, 1.0f, 3);
	RNA_def_property_ui_text(prop, "Maximum Distance", "Maximum distance for the field to work");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "radial_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "minrad");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Minimum Radial Distance", "Minimum radial distance for the field's fall-off");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "radial_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxrad");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Maximum Radial Distance", "Maximum radial distance for the field to work");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "radial_falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_power_r");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Radial Falloff Power", "Radial falloff power (real gravitational falloff = 2)");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "texture_nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tex_nabla");
	RNA_def_property_range(prop, 0.0001f, 1.0f);
	RNA_def_property_ui_text(prop, "Nabla",
	                         "Defines size of derivative offset used for calculating gradient and curl");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "noise", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_noise");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Noise", "Amount of noise for the force strength");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_range(prop, 1, 128);
	RNA_def_property_ui_text(prop, "Seed", "Seed of the noise");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/* Boolean */

	prop = RNA_def_property(srna, "use_min_distance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_USEMIN);
	RNA_def_property_ui_text(prop, "Use Min", "Use a minimum distance for the field's fall-off");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_max_distance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_USEMAX);
	RNA_def_property_ui_text(prop, "Use Max", "Use a maximum distance for the field to work");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_radial_min", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_USEMINR);
	RNA_def_property_ui_text(prop, "Use Min", "Use a minimum radial distance for the field's fall-off");
	/* "Use a minimum angle for the field's fall-off" */
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_radial_max", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_USEMAXR);
	RNA_def_property_ui_text(prop, "Use Max", "Use a maximum radial distance for the field to work");
	/* "Use a maximum angle for the field to work" */
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_object_coords", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_TEX_OBJECT);
	RNA_def_property_ui_text(prop, "Use Coordinates", "Use object/global coordinates for texture");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_global_coords", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_GLOBAL_CO);
	RNA_def_property_ui_text(prop, "Use Global Coordinates", "Use effector/global coordinates for turbulence");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_2d_force", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_TEX_2D);
	RNA_def_property_ui_text(prop, "2D", "Apply force only in 2D");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_root_coords", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_TEX_ROOTCO);
	RNA_def_property_ui_text(prop, "Root Texture Coordinates", "Texture coordinates from root particle locations");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "apply_to_location", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_DO_LOCATION);
	RNA_def_property_ui_text(prop, "Location", "Effect particles' location");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "apply_to_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_DO_ROTATION);
	RNA_def_property_ui_text(prop, "Rotation", "Effect particles' dynamic rotation");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_absorption", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_VISIBILITY);
	RNA_def_property_ui_text(prop, "Absorption", "Force gets absorbed by collision objects");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_dependency_update");

	prop = RNA_def_property(srna, "use_multiple_springs", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_MULTIPLE_SPRINGS);
	RNA_def_property_ui_text(prop, "Multiple Springs", "Every point is effected by multiple springs");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_smoke_density", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_SMOKE_DENSITY);
	RNA_def_property_ui_text(prop, "Apply Density", "Adjust force strength based on smoke density");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");
	prop = RNA_def_property(srna, "use_gravity_falloff", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_GRAVITATION);
	RNA_def_property_ui_text(prop, "Gravity Falloff", "Multiply force by 1/distance²");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");


	/* Pointer */

	prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tex");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture", "Texture to use as force");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "source_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "f_source");
	RNA_def_property_ui_text(prop, "Domain Object", "Select domain object of the smoke simulation");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/********** Curve Guide Field Settings **********/

	prop = RNA_def_property(srna, "guide_minimum", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_strength");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Minimum Distance", "The distance from which particles are affected fully");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "guide_free", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "free_end");
	RNA_def_property_range(prop, 0.0f, 0.99f);
	RNA_def_property_ui_text(prop, "Free", "Guide-free time from particle life's end");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_guide_path_add", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_GUIDE_PATH_ADD);
	RNA_def_property_ui_text(prop, "Additive", "Based on distance/falloff it adds a portion of the entire path");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "use_guide_path_weight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_GUIDE_PATH_WEIGHT);
	RNA_def_property_ui_text(prop, "Weights", "Use curve weights to influence the particle influence along the curve");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/* Clump Settings */

	prop = RNA_def_property(srna, "guide_clump_amount", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clump_fac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Amount", "Amount of clumping");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "guide_clump_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clump_pow");
	RNA_def_property_range(prop, -0.999f, 0.999f);
	RNA_def_property_ui_text(prop, "Shape", "Shape of clumping");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/* Kink Settings */

	prop = RNA_def_property(srna, "guide_kink_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "kink");
	RNA_def_property_enum_items(prop, guide_kink_items);
	RNA_def_property_ui_text(prop, "Kink", "Type of periodic offset on the curve");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "guide_kink_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "kink_axis");
	RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
	RNA_def_property_ui_text(prop, "Axis", "Which axis to use for offset");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "guide_kink_frequency", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_freq");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Frequency", "The frequency of the offset (1/total length)");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "guide_kink_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_shape");
	RNA_def_property_range(prop, -0.999f, 0.999f);
	RNA_def_property_ui_text(prop, "Shape", "Adjust the offset to the beginning/end");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	prop = RNA_def_property(srna, "guide_kink_amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_amp");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Amplitude", "The amplitude of the offset");
	RNA_def_property_update(prop, 0, "rna_FieldSettings_update");

	/* Variables used for Curve Guide, already wrapped, used for other fields too */
	/* falloff_power, use_max_distance, maximum_distance */
}

static void rna_def_softbody(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem collision_type_items[] = {
		{SBC_MODE_MANUAL, "MANUAL", 0, "Manual", "Manual adjust"},
		{SBC_MODE_AVG, "AVERAGE", 0, "Average", "Average Spring length * Ball Size"},
		{SBC_MODE_MIN, "MINIMAL", 0, "Minimal", "Minimal Spring length * Ball Size"},
		{SBC_MODE_MAX, "MAXIMAL", 0, "Maximal", "Maximal Spring length * Ball Size"},
		{SBC_MODE_AVGMINMAX, "MINMAX", 0, "AvMinMax", "(Min+Max)/2 * Ball Size"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem aerodynamics_type[] = {
		{0, "SIMPLE", 0, "Simple", "Edges receive a drag force from surrounding media"},
		{1, "LIFT_FORCE", 0, "Lift Force", "Edges receive a lift force when passing through surrounding media"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "SoftBodySettings", NULL);
	RNA_def_struct_sdna(srna, "SoftBody");
	RNA_def_struct_path_func(srna, "rna_SoftBodySettings_path");
	RNA_def_struct_ui_text(srna, "Soft Body Settings", "Soft body simulation settings for an object");

	/* General Settings */

	prop = RNA_def_property(srna, "friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mediafrict");
	RNA_def_property_range(prop, 0.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Friction", "General media friction for point movements");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "nodemass");
	RNA_def_property_range(prop, 0.0f, 50000.0f);
	RNA_def_property_ui_text(prop, "Mass", "General Mass value");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "vertex_group_mass", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "namedVG_Mass");
	RNA_def_property_ui_text(prop, "Mass Vertex Group", "Control point mass values");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SoftBodySettings_mass_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	/* no longer used */
	prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_ACCELERATION);
	RNA_def_property_float_sdna(prop, NULL, "grav");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Gravitation", "Apply gravitation to point movement");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "physics_speed");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Speed", "Tweak timing for physics to control frequency and speed");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	/* Goal */

	prop = RNA_def_property(srna, "vertex_group_goal", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vertgroup");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* not impossible .. but not supported yet */
	RNA_def_property_string_funcs(prop, "rna_SoftBodySettings_goal_vgroup_get",
	                              "rna_SoftBodySettings_goal_vgroup_length",
	                              "rna_SoftBodySettings_goal_vgroup_set");
	RNA_def_property_ui_text(prop, "Goal Vertex Group", "Control point weight values");

	prop = RNA_def_property(srna, "goal_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mingoal");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Goal Minimum", "Goal minimum, vertex weights are scaled to match this range");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "goal_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxgoal");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Goal Maximum", "Goal maximum, vertex weights are scaled to match this range");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "goal_default", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "defgoal");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Goal Default",
	                         "Default Goal (vertex target position) value");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "goal_spring", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "goalspring");
	RNA_def_property_range(prop, 0.0f, 0.999f);
	RNA_def_property_ui_text(prop, "Goal Stiffness", "Goal (vertex target position) spring stiffness");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "goal_friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "goalfrict");
	RNA_def_property_range(prop, 0.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Goal Damping", "Goal (vertex target position) friction");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	/* Edge Spring Settings */

	prop = RNA_def_property(srna, "pull", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "inspring");
	RNA_def_property_range(prop, 0.0f, 0.999f);
	RNA_def_property_ui_text(prop, "Pull", "Edge spring stiffness when longer than rest length");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "push", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "inpush");
	RNA_def_property_range(prop, 0.0f, 0.999f);
	RNA_def_property_ui_text(prop, "Push", "Edge spring stiffness when shorter than rest length");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "infrict");
	RNA_def_property_range(prop, 0.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Damp", "Edge spring friction");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "spring_length", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "springpreload");
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_text(prop, "view_layer", "Alter spring length to shrink/blow up (unit %) 0 to disable");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "aero", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "aeroedge");
	RNA_def_property_range(prop, 0.0f, 30000.0f);
	RNA_def_property_ui_text(prop, "Aero", "Make edges 'sail'");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "plastic", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "plastic");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Plastic", "Permanent deform");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "bend", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "secondspring");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Bending", "Bending Stiffness");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "shear", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shearstiff");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Shear", "Shear Stiffness");

	prop = RNA_def_property(srna, "vertex_group_spring", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "namedVG_Spring_K");
	RNA_def_property_ui_text(prop, "Spring Vertex Group", "Control point spring strength values");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SoftBodySettings_spring_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	/* Collision */

	prop = RNA_def_property(srna, "collision_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "sbc_mode");
	RNA_def_property_enum_items(prop, collision_type_items);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Collision Type", "Choose Collision Type");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "ball_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "colball");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* code is not ready for that yet */
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Ball Size", "Absolute ball size or factor if not manually adjusted");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "ball_stiff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ballstiff");
	RNA_def_property_range(prop, 0.001f, 100.0f);
	RNA_def_property_ui_text(prop, "Ball Size", "Ball inflating pressure");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "ball_damp", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "balldamp");
	RNA_def_property_range(prop, 0.001f, 1.0f);
	RNA_def_property_ui_text(prop, "Ball Size", "Blending to inelastic collision");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	/* Solver */

	prop = RNA_def_property(srna, "error_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rklimit");
	RNA_def_property_range(prop, 0.001f, 10.0f);
	RNA_def_property_ui_text(prop, "Error Limit",
	                         "The Runge-Kutta ODE solver error limit, low value gives more precision, "
	                         "high values speed");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "step_min", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "minloops");
	RNA_def_property_range(prop, 0, 30000);
	RNA_def_property_ui_text(prop, "Min Step", "Minimal # solver steps/frame");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "step_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxloops");
	RNA_def_property_range(prop, 0, 30000);
	RNA_def_property_ui_text(prop, "Max Step", "Maximal # solver steps/frame");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "choke", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "choke");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Choke", "'Viscosity' inside collision target");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "fuzzy", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "fuzzyness");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Fuzzy",
	                         "Fuzziness while on collision, high values make collision handling faster "
	                         "but less stable");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "use_auto_step", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "solverflags", SBSO_OLDERR);
	RNA_def_property_ui_text(prop, "V", "Use velocities for automagic step sizes");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "use_diagnose", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "solverflags", SBSO_MONITOR);
	RNA_def_property_ui_text(prop, "Print Performance to Console", "Turn on SB diagnose console prints");

	prop = RNA_def_property(srna, "use_estimate_matrix", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "solverflags", SBSO_ESTIMATEIPO);
	RNA_def_property_ui_text(prop, "Estimate matrix", "Estimate matrix... split to COM, ROT, SCALE");


	/***********************************************************************************/
	/* these are not exactly settings, but reading calculated results*/
	/* but i did not want to start a new property struct */
	/* so rather rename this from SoftBodySettings to SoftBody */
	/* translation */
	prop = RNA_def_property(srna, "location_mass_center", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "lcom");
	RNA_def_property_ui_text(prop, "Center of mass", "Location of Center of mass");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

	/* matrix */
	prop = RNA_def_property(srna, "rotation_estimate", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "lrot");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_3x3);
	RNA_def_property_ui_text(prop, "Rot Matrix", "Estimated rotation matrix");

	prop = RNA_def_property(srna, "scale_estimate", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "lscale");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_3x3);
	RNA_def_property_ui_text(prop, "Scale Matrix", "Estimated scale matrix");
	/***********************************************************************************/


	/* Flags */

	prop = RNA_def_property(srna, "use_goal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SoftBodySettings_use_goal_get", "rna_SoftBodySettings_use_goal_set");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Use Goal", "Define forces for vertices to stick to animated position");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "use_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SoftBodySettings_use_edges_get", "rna_SoftBodySettings_use_edges_set");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Use Edges", "Use Edges as springs");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "use_stiff_quads", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SoftBodySettings_stiff_quads_get",
	                               "rna_SoftBodySettings_stiff_quads_set");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Stiff Quads", "Add diagonal springs on 4-gons");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "use_edge_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SoftBodySettings_edge_collision_get",
	                               "rna_SoftBodySettings_edge_collision_set");
	RNA_def_property_ui_text(prop, "Edge Collision", "Edges collide too");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "use_face_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SoftBodySettings_face_collision_get",
	                               "rna_SoftBodySettings_face_collision_set");
	RNA_def_property_ui_text(prop, "Face Collision", "Faces collide too, can be very slow");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "aerodynamics_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, aerodynamics_type);
	RNA_def_property_enum_funcs(prop, "rna_SoftBodySettings_new_aero_get", "rna_SoftBodySettings_new_aero_set", NULL);
	RNA_def_property_ui_text(prop, "Aerodynamics Type", "Method of calculating aerodynamic interaction");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "use_self_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SoftBodySettings_self_collision_get",
	                               "rna_SoftBodySettings_self_collision_set");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Self Collision", "Enable naive vertex ball self collision");
	RNA_def_property_update(prop, 0, "rna_softbody_update");

	prop = RNA_def_property(srna, "collision_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Collection");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Collision Collection", "Limit colliders to this collection");
	RNA_def_property_update(prop, 0, "rna_softbody_dependency_update");

	prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "effector_weights");
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");
}

void RNA_def_object_force(BlenderRNA *brna)
{
	rna_def_pointcache_active(brna);
	rna_def_collision(brna);
	rna_def_effector_weight(brna);
	rna_def_field(brna);
	rna_def_softbody(brna);
}

#endif
