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
 * Adaptive time step
 * Copyright 2011 AutoCRC
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_particle.c
 *  \ingroup RNA
 */


#include <stdio.h>
#include <stdlib.h>

#include "limits.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_cloth_types.h"
#include "DNA_particle_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_boid_types.h"
#include "DNA_texture_types.h"

#include "WM_types.h"
#include "WM_api.h"

EnumPropertyItem part_from_items[] = {
	{PART_FROM_VERT, "VERT", 0, "Verts", ""},
	{PART_FROM_FACE, "FACE", 0, "Faces", ""},
	{PART_FROM_VOLUME, "VOLUME", 0, "Volume", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem part_reactor_from_items[] = {
	{PART_FROM_VERT, "VERT", 0, "Verts", ""},
	{PART_FROM_FACE, "FACE", 0, "Faces", ""},
	{PART_FROM_VOLUME, "VOLUME", 0, "Volume", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem part_dist_items[] = {
	{PART_DISTR_JIT, "JIT", 0, "Jittered", ""},
	{PART_DISTR_RAND, "RAND", 0, "Random", ""},
	{PART_DISTR_GRID, "GRID", 0, "Grid", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem part_hair_dist_items[] = {
	{PART_DISTR_JIT, "JIT", 0, "Jittered", ""},
	{PART_DISTR_RAND, "RAND", 0, "Random", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem part_draw_as_items[] = {
	{PART_DRAW_NOT, "NONE", 0, "None", ""},
	{PART_DRAW_REND, "RENDER", 0, "Rendered", ""},
	{PART_DRAW_DOT, "DOT", 0, "Point", ""},
	{PART_DRAW_CIRC, "CIRC", 0, "Circle", ""},
	{PART_DRAW_CROSS, "CROSS", 0, "Cross", ""},
	{PART_DRAW_AXIS, "AXIS", 0, "Axis", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem part_hair_draw_as_items[] = {
	{PART_DRAW_NOT, "NONE", 0, "None", ""},
	{PART_DRAW_REND, "RENDER", 0, "Rendered", ""},
	{PART_DRAW_PATH, "PATH", 0, "Path", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem part_ren_as_items[] = {
	{PART_DRAW_NOT, "NONE", 0, "None", ""},
	{PART_DRAW_HALO, "HALO", 0, "Halo", ""},
	{PART_DRAW_LINE, "LINE", 0, "Line", ""},
	{PART_DRAW_PATH, "PATH", 0, "Path", ""},
	{PART_DRAW_OB, "OBJECT", 0, "Object", ""},
	{PART_DRAW_GR, "GROUP", 0, "Group", ""},
	{PART_DRAW_BB, "BILLBOARD", 0, "Billboard", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem part_hair_ren_as_items[] = {
	{PART_DRAW_NOT, "NONE", 0, "None", ""},
	{PART_DRAW_PATH, "PATH", 0, "Path", ""},
	{PART_DRAW_OB, "OBJECT", 0, "Object", ""},
	{PART_DRAW_GR, "GROUP", 0, "Group", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_cloth.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_texture.h"

/* use for object space hair get/set */
static void rna_ParticleHairKey_location_object_info(PointerRNA *ptr, ParticleSystemModifierData **psmd_pt,
                                                     ParticleData **pa_pt)
{
	HairKey *hkey = (HairKey *)ptr->data;
	Object *ob = (Object *)ptr->id.data;
	ModifierData *md;
	ParticleSystemModifierData *psmd = NULL;
	ParticleSystem *psys;
	ParticleData *pa;
	int i;

	*psmd_pt = NULL;
	*pa_pt = NULL;

	/* given the pointer HairKey *hkey, we iterate over all particles in all
	 * particle systems in the object "ob" in order to find
	 *- the ParticleSystemData to which the HairKey (and hence the particle)
	 *  belongs (will be stored in psmd_pt)
	 *- the ParticleData to which the HairKey belongs (will be stored in pa_pt)
	 *
	 * not a very efficient way of getting hair key location data,
	 * but it's the best we've got at the present
	 *
	 * IDEAS: include additional information in pointerRNA beforehand,
	 * for example a pointer to the ParticleStstemModifierData to which the
	 * hairkey belongs.
	 */

	for (md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_ParticleSystem) {
			psmd = (ParticleSystemModifierData *) md;
			if (psmd && psmd->dm && psmd->psys) {
				psys = psmd->psys;
				for (i = 0, pa = psys->particles; i < psys->totpart; i++, pa++) {
					/* hairkeys are stored sequentially in memory, so we can
					 * find if it's the same particle by comparing pointers,
					 * without having to iterate over them all */
					if ((hkey >= pa->hair) && (hkey < pa->hair + pa->totkey)) {
						*psmd_pt = psmd;
						*pa_pt = pa;
						return;
					}
				}
			}
		}
	}
}

static void rna_ParticleHairKey_location_object_get(PointerRNA *ptr, float *values)
{
	HairKey *hkey = (HairKey *)ptr->data;
	Object *ob = (Object *)ptr->id.data;
	ParticleSystemModifierData *psmd;
	ParticleData *pa;

	rna_ParticleHairKey_location_object_info(ptr, &psmd, &pa);

	if (pa) {
		DerivedMesh *hairdm = (psmd->psys->flag & PSYS_HAIR_DYNAMICS) ? psmd->psys->hair_out_dm : NULL;

		if (hairdm) {
			MVert *mvert = CDDM_get_vert(hairdm, pa->hair_index + (hkey - pa->hair));
			copy_v3_v3(values, mvert->co);
		}
		else {
			float hairmat[4][4];
			psys_mat_hair_to_object(ob, psmd->dm, psmd->psys->part->from, pa, hairmat);
			copy_v3_v3(values, hkey->co);
			mul_m4_v3(hairmat, values);
		}
	}
	else {
		zero_v3(values);
	}
}

static void rna_ParticleHairKey_location_object_set(PointerRNA *ptr, const float *values)
{
	HairKey *hkey = (HairKey *)ptr->data;
	Object *ob = (Object *)ptr->id.data;
	ParticleSystemModifierData *psmd;
	ParticleData *pa;

	rna_ParticleHairKey_location_object_info(ptr, &psmd, &pa);

	if (pa) {
		DerivedMesh *hairdm = (psmd->psys->flag & PSYS_HAIR_DYNAMICS) ? psmd->psys->hair_out_dm : NULL;

		if (hairdm) {
			MVert *mvert = CDDM_get_vert(hairdm, pa->hair_index + (hkey - pa->hair));
			copy_v3_v3(mvert->co, values);
		}
		else {
			float hairmat[4][4];
			float imat[4][4];

			psys_mat_hair_to_object(ob, psmd->dm, psmd->psys->part->from, pa, hairmat);
			invert_m4_m4(imat, hairmat);
			copy_v3_v3(hkey->co, values);
			mul_m4_v3(imat, hkey->co);
		}
	}
	else {
		zero_v3(hkey->co);
	}
}

/* property update functions */
static void particle_recalc(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr, short flag)
{
	if (ptr->type == &RNA_ParticleSystem) {
		ParticleSystem *psys = (ParticleSystem*)ptr->data;
		
		psys->recalc = flag;

		DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
	}
	else
		DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA|flag);

	WM_main_add_notifier(NC_OBJECT|ND_PARTICLE|NA_EDITED, NULL);
}
static void rna_Particle_redo(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	particle_recalc(bmain, scene, ptr, PSYS_RECALC_REDO);
}

static void rna_Particle_redo_dependency(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DAG_scene_sort(bmain, scene);
	rna_Particle_redo(bmain, scene, ptr);
}

static void rna_Particle_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	particle_recalc(bmain, scene, ptr, PSYS_RECALC_RESET);
}

static void rna_Particle_change_type(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	particle_recalc(bmain, scene, ptr, PSYS_RECALC_RESET|PSYS_RECALC_TYPE);
}

static void rna_Particle_change_physics(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	particle_recalc(bmain, scene, ptr, PSYS_RECALC_RESET|PSYS_RECALC_PHYS);
}

static void rna_Particle_redo_child(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	particle_recalc(bmain, scene, ptr, PSYS_RECALC_CHILD);
}

static ParticleSystem *rna_particle_system_for_target(Object *ob, ParticleTarget *target)
{
	ParticleSystem *psys;
	ParticleTarget *pt;

	for (psys = ob->particlesystem.first; psys; psys = psys->next)
		for (pt = psys->targets.first; pt; pt = pt->next)
			if (pt == target)
				return psys;
	
	return NULL;
}

static void rna_Particle_target_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	if (ptr->type == &RNA_ParticleTarget) {
		Object *ob = (Object*)ptr->id.data;
		ParticleTarget *pt = (ParticleTarget*)ptr->data;
		ParticleSystem *kpsys = NULL, *psys = rna_particle_system_for_target(ob, pt);

		if (pt->ob == ob || pt->ob == NULL) {
			kpsys = BLI_findlink(&ob->particlesystem, pt->psys-1);

			if (kpsys)
				pt->flag |= PTARGET_VALID;
			else
				pt->flag &= ~PTARGET_VALID;
		}
		else {
			if (pt->ob)
				kpsys = BLI_findlink(&pt->ob->particlesystem, pt->psys-1);

			if (kpsys)
				pt->flag |= PTARGET_VALID;
			else
				pt->flag &= ~PTARGET_VALID;
		}
		
		psys->recalc = PSYS_RECALC_RESET;

		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		DAG_scene_sort(bmain, scene);
	}

	WM_main_add_notifier(NC_OBJECT|ND_PARTICLE|NA_EDITED, NULL);
}

static void rna_Particle_target_redo(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	if (ptr->type == &RNA_ParticleTarget) {
		Object *ob = (Object*)ptr->id.data;
		ParticleTarget *pt = (ParticleTarget*)ptr->data;
		ParticleSystem *psys = rna_particle_system_for_target(ob, pt);
		
		psys->recalc = PSYS_RECALC_REDO;

		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_main_add_notifier(NC_OBJECT|ND_PARTICLE|NA_EDITED, NULL);
	}
}

static void rna_Particle_hair_dynamics(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob = (Object*)ptr->id.data;
	ParticleSystem *psys = (ParticleSystem*)ptr->data;
	
	if (psys && !psys->clmd) {
		psys->clmd = (ClothModifierData*)modifier_new(eModifierType_Cloth);
		psys->clmd->sim_parms->goalspring = 0.0f;
		psys->clmd->sim_parms->flags |= CLOTH_SIMSETTINGS_FLAG_GOAL|CLOTH_SIMSETTINGS_FLAG_NO_SPRING_COMPRESS;
		psys->clmd->coll_parms->flags &= ~CLOTH_COLLSETTINGS_FLAG_SELF;
		rna_Particle_redo(bmain, scene, ptr);
	}
	else
		WM_main_add_notifier(NC_OBJECT|ND_PARTICLE|NA_EDITED, NULL);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
}
static PointerRNA rna_particle_settings_get(PointerRNA *ptr)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;
	ParticleSettings *part = psys->part;

	return rna_pointer_inherit_refine(ptr, &RNA_ParticleSettings, part);
}

static void rna_particle_settings_set(PointerRNA *ptr, PointerRNA value)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;
	int old_type = 0;


	if (psys->part) {
		old_type = psys->part->type;
		psys->part->id.us--;
	}

	psys->part = (ParticleSettings *)value.data;

	if (psys->part) {
		psys->part->id.us++;
		psys_check_boid_data(psys);
		if (old_type != psys->part->type)
			psys->recalc |= PSYS_RECALC_TYPE;
	}
}
static void rna_Particle_abspathtime_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	float delta = settings->end + settings->lifetime - settings->sta;
	if (settings->draw & PART_ABS_PATH_TIME) {
		settings->path_start = settings->sta + settings->path_start * delta;
		settings->path_end = settings->sta + settings->path_end * delta;
	}
	else {
		settings->path_start = (settings->path_start - settings->sta)/delta;
		settings->path_end = (settings->path_end - settings->sta)/delta;
	}
	rna_Particle_redo(bmain, scene, ptr);
}
static void rna_PartSettings_start_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;

	/* check for clipping */
	if (value > settings->end)
		value = settings->end;

	/*if(settings->type==PART_REACTOR && value < 1.0) */
	/*	value = 1.0; */
	/*else  */
	if (value < MINAFRAMEF)
		value = MINAFRAMEF;

	settings->sta = value;
}

static void rna_PartSettings_end_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;

	/* check for clipping */
	if (value < settings->sta)
		value = settings->sta;

	settings->end = value;
}

static void rna_PartSetings_timestep_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;

	settings->timetweak = value/0.04f;
}

static float rna_PartSettings_timestep_get(struct PointerRNA *ptr)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;

	return settings->timetweak * 0.04f;
}

static void rna_PartSetting_hairlength_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	settings->normfac = value / 4.f;
}

static float rna_PartSetting_hairlength_get(struct PointerRNA *ptr)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	return settings->normfac * 4.f;
}

static void rna_PartSetting_linelentail_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	settings->draw_line[0] = value;
}

static float rna_PartSetting_linelentail_get(struct PointerRNA *ptr)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	return settings->draw_line[0];
}
static void rna_PartSetting_pathstartend_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;

	if (settings->type == PART_HAIR) {
		*min = 0.0f;
		*max = (settings->draw & PART_ABS_PATH_TIME) ? 100.0f : 1.0f;
	}
	else {
		*min = (settings->draw & PART_ABS_PATH_TIME) ? settings->sta : 0.0f;
		*max = (settings->draw & PART_ABS_PATH_TIME) ? MAXFRAMEF : 1.0f;
	}
}
static void rna_PartSetting_linelenhead_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	settings->draw_line[1] = value;
}

static float rna_PartSetting_linelenhead_get(struct PointerRNA *ptr)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	return settings->draw_line[1];
}


static int rna_PartSettings_is_fluid_get(PointerRNA *ptr)
{
	ParticleSettings *part = (ParticleSettings*)ptr->data;

	return part->type == PART_FLUID;
}

void rna_ParticleSystem_name_set(PointerRNA *ptr, const char *value)
{
	Object *ob = ptr->id.data;
	ParticleSystem *part = (ParticleSystem*)ptr->data;

	/* copy the new name into the name slot */
	BLI_strncpy_utf8(part->name, value, sizeof(part->name));

	BLI_uniquename(&ob->particlesystem, part, "ParticleSystem", '.', offsetof(ParticleSystem, name),
	               sizeof(part->name));
}

static PointerRNA rna_ParticleSystem_active_particle_target_get(PointerRNA *ptr)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;
	ParticleTarget *pt = psys->targets.first;

	for (; pt; pt = pt->next) {
		if (pt->flag & PTARGET_CURRENT)
			return rna_pointer_inherit_refine(ptr, &RNA_ParticleTarget, pt);
	}
	return rna_pointer_inherit_refine(ptr, &RNA_ParticleTarget, NULL);
}
static void rna_ParticleSystem_active_particle_target_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;
	*min = 0;
	*max = BLI_countlist(&psys->targets)-1;
	*max = MAX2(0, *max);
}

static int rna_ParticleSystem_active_particle_target_index_get(PointerRNA *ptr)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;
	ParticleTarget *pt = psys->targets.first;
	int i = 0;

	for (; pt; pt = pt->next, i++)
		if (pt->flag & PTARGET_CURRENT)
			return i;

	return 0;
}

static void rna_ParticleSystem_active_particle_target_index_set(struct PointerRNA *ptr, int value)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;
	ParticleTarget *pt = psys->targets.first;
	int i = 0;

	for (; pt; pt = pt->next, i++) {
		if (i == value)
			pt->flag |= PTARGET_CURRENT;
		else
			pt->flag &= ~PTARGET_CURRENT;
	}
}
static int rna_ParticleTarget_name_length(PointerRNA *ptr)
{
	ParticleTarget *pt = ptr->data;

	if (pt->flag & PTARGET_VALID) {
		ParticleSystem *psys = NULL;

		if (pt->ob)
			psys = BLI_findlink(&pt->ob->particlesystem, pt->psys-1);
		else {
			Object *ob = (Object*) ptr->id.data;
			psys = BLI_findlink(&ob->particlesystem, pt->psys-1);
		}
		
		if (psys) {
			if (pt->ob)
				return strlen(pt->ob->id.name+2) + 2 + strlen(psys->name);
			else
				return strlen(psys->name);
		}
		else
			return 15;
	}
	else
		return 15;
}

static void rna_ParticleTarget_name_get(PointerRNA *ptr, char *str)
{
	ParticleTarget *pt = ptr->data;

	if (pt->flag & PTARGET_VALID) {
		ParticleSystem *psys = NULL;

		if (pt->ob)
			psys = BLI_findlink(&pt->ob->particlesystem, pt->psys-1);
		else {
			Object *ob = (Object*) ptr->id.data;
			psys = BLI_findlink(&ob->particlesystem, pt->psys-1);
		}
		
		if (psys) {
			if (pt->ob)
				sprintf(str, "%s: %s", pt->ob->id.name+2, psys->name);
			else
				strcpy(str, psys->name);
		}
		else
			strcpy(str, "Invalid target!");
	}
	else
		strcpy(str, "Invalid target!");
}

static int particle_id_check(PointerRNA *ptr)
{
	ID *id = ptr->id.data;

	return (GS(id->name) == ID_PA);
}

static char *rna_SPHFluidSettings_path(PointerRNA *ptr)
{
	SPHFluidSettings *fluid = (SPHFluidSettings *)ptr->data;
	
	if (particle_id_check(ptr)) {
		ParticleSettings *part = (ParticleSettings*)ptr->id.data;
		
		if (part->fluid == fluid)
			return BLI_sprintfN("fluid");
	}
	return NULL;
}

static int rna_ParticleSystem_multiple_caches_get(PointerRNA *ptr)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;

	return (psys->ptcaches.first != psys->ptcaches.last);
}
static int rna_ParticleSystem_editable_get(PointerRNA *ptr)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;

	return psys_check_edited(psys);
}
static int rna_ParticleSystem_edited_get(PointerRNA *ptr)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;

	if (psys->part && psys->part->type == PART_HAIR)
		return (psys->flag & PSYS_EDITED || (psys->edit && psys->edit->edited));
	else
		return (psys->pointcache->edit && psys->pointcache->edit->edited);
}
static PointerRNA rna_ParticleDupliWeight_active_get(PointerRNA *ptr)
{
	ParticleSettings *part = (ParticleSettings*)ptr->id.data;
	ParticleDupliWeight *dw = part->dupliweights.first;

	for (; dw; dw = dw->next) {
		if (dw->flag & PART_DUPLIW_CURRENT)
			return rna_pointer_inherit_refine(ptr, &RNA_ParticleDupliWeight, dw);
	}
	return rna_pointer_inherit_refine(ptr, &RNA_ParticleTarget, NULL);
}
static void rna_ParticleDupliWeight_active_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	ParticleSettings *part = (ParticleSettings*)ptr->id.data;
	*min = 0;
	*max = BLI_countlist(&part->dupliweights)-1;
	*max = MAX2(0, *max);
}

static int rna_ParticleDupliWeight_active_index_get(PointerRNA *ptr)
{
	ParticleSettings *part = (ParticleSettings*)ptr->id.data;
	ParticleDupliWeight *dw = part->dupliweights.first;
	int i = 0;

	for (; dw; dw = dw->next, i++)
		if (dw->flag & PART_DUPLIW_CURRENT)
			return i;

	return 0;
}

static void rna_ParticleDupliWeight_active_index_set(struct PointerRNA *ptr, int value)
{
	ParticleSettings *part = (ParticleSettings*)ptr->id.data;
	ParticleDupliWeight *dw = part->dupliweights.first;
	int i = 0;

	for (; dw; dw = dw->next, i++) {
		if (i == value)
			dw->flag |= PART_DUPLIW_CURRENT;
		else
			dw->flag &= ~PART_DUPLIW_CURRENT;
	}
}

static void rna_ParticleDupliWeight_name_get(PointerRNA *ptr, char *str);

static int rna_ParticleDupliWeight_name_length(PointerRNA *ptr)
{
	char tstr[32];
	rna_ParticleDupliWeight_name_get(ptr, tstr);
	return strlen(tstr);
}

static void rna_ParticleDupliWeight_name_get(PointerRNA *ptr, char *str)
{
	ParticleDupliWeight *dw = ptr->data;

	if (dw->ob)
		sprintf(str, "%s: %i", dw->ob->id.name+2, dw->count);
	else
		strcpy(str, "No object");
}

static EnumPropertyItem *rna_Particle_from_itemf(bContext *UNUSED(C), PointerRNA *UNUSED(ptr),
                                                 PropertyRNA *UNUSED(prop), int *UNUSED(free))
{
	/*if(part->type==PART_REACTOR) */
	/*	return part_reactor_from_items; */
	/*else */
		return part_from_items;
}

static EnumPropertyItem *rna_Particle_dist_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                 PropertyRNA *UNUSED(prop), int *UNUSED(free))
{
	ParticleSettings *part = ptr->id.data;

	if (part->type == PART_HAIR)
		return part_hair_dist_items;
	else
		return part_dist_items;
}

static EnumPropertyItem *rna_Particle_draw_as_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                    PropertyRNA *UNUSED(prop), int *UNUSED(free))
{
	ParticleSettings *part = ptr->id.data;

	if (part->type == PART_HAIR)
		return part_hair_draw_as_items;
	else
		return part_draw_as_items;
}

static EnumPropertyItem *rna_Particle_ren_as_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                   PropertyRNA *UNUSED(prop), int *UNUSED(free))
{
	ParticleSettings *part = ptr->id.data;

	if (part->type == PART_HAIR)
		return part_hair_ren_as_items;
	else
		return part_ren_as_items;
}

static PointerRNA rna_Particle_field1_get(PointerRNA *ptr)
{
	ParticleSettings *part = (ParticleSettings*)ptr->id.data;

	/* weak */
	if (!part->pd)
		part->pd = object_add_collision_fields(0);
	
	return rna_pointer_inherit_refine(ptr, &RNA_FieldSettings, part->pd);
}

static PointerRNA rna_Particle_field2_get(PointerRNA *ptr)
{
	ParticleSettings *part = (ParticleSettings*)ptr->id.data;

	/* weak */
	if (!part->pd2)
		part->pd2 = object_add_collision_fields(0);
	
	return rna_pointer_inherit_refine(ptr, &RNA_FieldSettings, part->pd2);
}

static void psys_vg_name_get__internal(PointerRNA *ptr, char *value, int index)
{
	Object *ob = ptr->id.data;
	ParticleSystem *psys = (ParticleSystem*)ptr->data;

	if (psys->vgroup[index] > 0) {
		bDeformGroup *defGroup = BLI_findlink(&ob->defbase, psys->vgroup[index]-1);

		if (defGroup) {
			strcpy(value, defGroup->name);
			return;
		}
	}

	value[0] = '\0';
}
static int psys_vg_name_len__internal(PointerRNA *ptr, int index)
{
	Object *ob = ptr->id.data;
	ParticleSystem *psys = (ParticleSystem*)ptr->data;

	if (psys->vgroup[index] > 0) {
		bDeformGroup *defGroup = BLI_findlink(&ob->defbase, psys->vgroup[index]-1);

		if (defGroup) {
			return strlen(defGroup->name);
		}
	}
	return 0;
}
static void psys_vg_name_set__internal(PointerRNA *ptr, const char *value, int index)
{
	Object *ob = ptr->id.data;
	ParticleSystem *psys = (ParticleSystem*)ptr->data;

	if (value[0] =='\0') {
		psys->vgroup[index] = 0;
	}
	else {
		int vgroup_num = defgroup_name_index(ob, value);

		if (vgroup_num == -1)
			return;

		psys->vgroup[index] = vgroup_num + 1;
	}
}

static char *rna_ParticleSystem_path(PointerRNA *ptr)
{
	ParticleSystem *psys = (ParticleSystem*)ptr->data;
	return BLI_sprintfN("particle_systems[\"%s\"]", psys->name);
}

static void rna_ParticleSettings_mtex_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	ParticleSettings *part = (ParticleSettings*)ptr->data;
	rna_iterator_array_begin(iter, (void*)part->mtex, sizeof(MTex*), MAX_MTEX, 0, NULL);
}

static PointerRNA rna_ParticleSettings_active_texture_get(PointerRNA *ptr)
{
	ParticleSettings *part = (ParticleSettings*)ptr->data;
	Tex *tex;

	tex = give_current_particle_texture(part);
	return rna_pointer_inherit_refine(ptr, &RNA_Texture, tex);
}

static void rna_ParticleSettings_active_texture_set(PointerRNA *ptr, PointerRNA value)
{
	ParticleSettings *part = (ParticleSettings*)ptr->data;

	set_current_particle_texture(part, value.data);
}

/* irritating string functions for each index :/ */
static void rna_ParticleVGroup_name_get_0(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 0); }
static void rna_ParticleVGroup_name_get_1(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 1); }
static void rna_ParticleVGroup_name_get_2(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 2); }
static void rna_ParticleVGroup_name_get_3(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 3); }
static void rna_ParticleVGroup_name_get_4(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 4); }
static void rna_ParticleVGroup_name_get_5(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 5); }
static void rna_ParticleVGroup_name_get_6(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 6); }
static void rna_ParticleVGroup_name_get_7(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 7); }
static void rna_ParticleVGroup_name_get_8(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 8); }
static void rna_ParticleVGroup_name_get_9(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 9); }
static void rna_ParticleVGroup_name_get_10(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 10); }
static void rna_ParticleVGroup_name_get_11(PointerRNA *ptr, char *value) { psys_vg_name_get__internal(ptr, value, 11); }

static int rna_ParticleVGroup_name_len_0(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 0); }
static int rna_ParticleVGroup_name_len_1(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 1); }
static int rna_ParticleVGroup_name_len_2(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 2); }
static int rna_ParticleVGroup_name_len_3(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 3); }
static int rna_ParticleVGroup_name_len_4(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 4); }
static int rna_ParticleVGroup_name_len_5(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 5); }
static int rna_ParticleVGroup_name_len_6(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 6); }
static int rna_ParticleVGroup_name_len_7(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 7); }
static int rna_ParticleVGroup_name_len_8(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 8); }
static int rna_ParticleVGroup_name_len_9(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 9); }
static int rna_ParticleVGroup_name_len_10(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 10); }
static int rna_ParticleVGroup_name_len_11(PointerRNA *ptr) { return psys_vg_name_len__internal(ptr, 11); }

static void rna_ParticleVGroup_name_set_0(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 0); }
static void rna_ParticleVGroup_name_set_1(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 1); }
static void rna_ParticleVGroup_name_set_2(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 2); }
static void rna_ParticleVGroup_name_set_3(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 3); }
static void rna_ParticleVGroup_name_set_4(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 4); }
static void rna_ParticleVGroup_name_set_5(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 5); }
static void rna_ParticleVGroup_name_set_6(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 6); }
static void rna_ParticleVGroup_name_set_7(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 7); }
static void rna_ParticleVGroup_name_set_8(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 8); }
static void rna_ParticleVGroup_name_set_9(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 9); }
static void rna_ParticleVGroup_name_set_10(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 10); }
static void rna_ParticleVGroup_name_set_11(PointerRNA *ptr, const char *value) { psys_vg_name_set__internal(ptr, value, 11); }


#else

static void rna_def_particle_hair_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ParticleHairKey", NULL);
	RNA_def_struct_sdna(srna, "HairKey");
	RNA_def_struct_ui_text(srna, "Particle Hair Key", "Particle key for hair particle system");

	prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Time", "Relative time of key over hair length");

	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Weight", "Weight for cloth simulation");

	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Location (Object Space)", "Location of the hair key in object space");
	RNA_def_property_float_funcs(prop, "rna_ParticleHairKey_location_object_get",
	                             "rna_ParticleHairKey_location_object_set", NULL);
	
	prop = RNA_def_property(srna, "co_hair_space", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_ui_text(prop, "Location",
	                         "Location of the hair key in its internal coordinate system, "
	                         "relative to the emitting face");
}

static void rna_def_particle_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ParticleKey", NULL);
	RNA_def_struct_ui_text(srna, "Particle Key", "Key location for a particle over time");

	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_ui_text(prop, "Location", "Key location");

	prop = RNA_def_property(srna, "velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "vel");
	RNA_def_property_ui_text(prop, "Velocity", "Key velocity");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_QUATERNION);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Rotation", "Key rotation quaternion");

	prop = RNA_def_property(srna, "angular_velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "ave");
	RNA_def_property_ui_text(prop, "Angular Velocity", "Key angular velocity");

	prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Time", "Time of key over the simulation");
}

static void rna_def_child_particle(BlenderRNA *brna)
{
	StructRNA *srna;
	/*PropertyRNA *prop; */

	srna = RNA_def_struct(brna, "ChildParticle", NULL);
	RNA_def_struct_ui_text(srna, "Child Particle",
	                       "Child particle interpolated from simulated or edited particles");

/*	int num, parent;	 *//* num is face index on the final derived mesh */

/*	int pa[4];			 *//* nearest particles to the child, used for the interpolation */
/*	float w[4];			 *//* interpolation weights for the above particles */
/*	float fuv[4], foffset;  *//* face vertex weights and offset */
/*	float rand[3]; */
}

static void rna_def_particle(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem alive_items[] = {
		/*{PARS_KILLED, "KILLED", 0, "Killed", ""}, */
		{PARS_DEAD, "DEAD", 0, "Dead", ""},
		{PARS_UNBORN, "UNBORN", 0, "Unborn", ""},
		{PARS_ALIVE, "ALIVE", 0, "Alive", ""},
		{PARS_DYING, "DYING", 0, "Dying", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Particle", NULL);
	RNA_def_struct_sdna(srna, "ParticleData");
	RNA_def_struct_ui_text(srna, "Particle", "Particle in a particle system");

	/* Particle State & Previous State */
	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "state.co");
	RNA_def_property_ui_text(prop, "Particle Location", "");

	prop = RNA_def_property(srna, "velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "state.vel");
	RNA_def_property_ui_text(prop, "Particle Velocity", "");

	prop = RNA_def_property(srna, "angular_velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "state.ave");
	RNA_def_property_ui_text(prop, "Angular Velocity", "");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_QUATERNION);
	RNA_def_property_float_sdna(prop, NULL, "state.rot");
	RNA_def_property_ui_text(prop, "Rotation", "");

	prop = RNA_def_property(srna, "prev_location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "prev_state.co");
	RNA_def_property_ui_text(prop, "Previous Particle Location", "");

	prop = RNA_def_property(srna, "prev_velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "prev_state.vel");
	RNA_def_property_ui_text(prop, "Previous Particle Velocity", "");

	prop = RNA_def_property(srna, "prev_angular_velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "prev_state.ave");
	RNA_def_property_ui_text(prop, "Previous Angular Velocity", "");

	prop = RNA_def_property(srna, "prev_rotation", PROP_FLOAT, PROP_QUATERNION);
	RNA_def_property_float_sdna(prop, NULL, "prev_state.rot");
	RNA_def_property_ui_text(prop, "Previous Rotation", "");

	/* Hair & Keyed Keys */

	prop = RNA_def_property(srna, "hair_keys", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "hair", "totkey");
	RNA_def_property_struct_type(prop, "ParticleHairKey");
	RNA_def_property_ui_text(prop, "Hair", "");

	prop = RNA_def_property(srna, "particle_keys", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "keys", "totkey");
	RNA_def_property_struct_type(prop, "ParticleKey");
	RNA_def_property_ui_text(prop, "Keyed States", "");
/* */
/*	float fuv[4], foffset;	 *//* coordinates on face/edge number "num" and depth along*/
/*							 *//* face normal for volume emission						*/

	prop = RNA_def_property(srna, "birth_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "time");
/*	RNA_def_property_range(prop, lowerLimitf, upperLimitf); */
	RNA_def_property_ui_text(prop, "Birth Time", "");

	prop = RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_TIME);
/*	RNA_def_property_range(prop, lowerLimitf, upperLimitf); */
	RNA_def_property_ui_text(prop, "Lifetime", "");

	prop = RNA_def_property(srna, "die_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "dietime");
/*	RNA_def_property_range(prop, lowerLimitf, upperLimitf); */
	RNA_def_property_ui_text(prop, "Die Time", "");

	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
/*	RNA_def_property_range(prop, lowerLimitf, upperLimitf); */
	RNA_def_property_ui_text(prop, "Size", "");

/* */
/*	int num;				 *//* index to vert/edge/face */
/*	int num_dmcache;		 *//* index to derived mesh data (face) to avoid slow lookups */
/*	int pad; */
/* */
/*	int totkey; */

	/* flag */
	prop = RNA_def_property(srna, "is_exist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", PARS_UNEXIST);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Exists", "");

	prop = RNA_def_property(srna, "is_visible", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", PARS_NO_DISP);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Visible", "");

	prop = RNA_def_property(srna, "alive_state", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "alive");
	RNA_def_property_enum_items(prop, alive_items);
	RNA_def_property_ui_text(prop, "Alive State", "");

/*	short rt2; */
}

static void rna_def_particle_dupliweight(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ParticleDupliWeight", NULL);
	RNA_def_struct_ui_text(srna, "Particle Dupliobject Weight", "Weight of a particle dupliobject in a group");
	RNA_def_struct_sdna(srna, "ParticleDupliWeight");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleDupliWeight_name_get",
	                              "rna_ParticleDupliWeight_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Particle dupliobject name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "count", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_range(prop, 0, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Count",
	                         "The number of times this object is repeated with respect to other objects");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");
}

static void rna_def_fluid_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SPHFluidSettings", NULL);
	RNA_def_struct_path_func(srna, "rna_SPHFluidSettings_path");
	RNA_def_struct_ui_text(srna, "SPH Fluid Settings", "Settings for particle fluids physics");
	
	/* Fluid settings */
	prop = RNA_def_property(srna, "spring_force", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spring_k");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Spring Force", "Spring force");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "fluid_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radius");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Interaction Radius", "Fluid interaction radius");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "rest_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Rest Length", "Spring rest length (factor of particle radius)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_viscoelastic_springs", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPH_VISCOELASTIC_SPRINGS);
	RNA_def_property_ui_text(prop, "Viscoelastic Springs", "Use viscoelastic springs instead of Hooke's springs");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_initial_rest_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPH_CURRENT_REST_LENGTH);
	RNA_def_property_ui_text(prop, "Initial Rest Length",
	                         "Use the initial length as spring rest length instead of 2 * particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "plasticity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "plasticity_constant");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Plasticity",
	                         "How much the spring rest length can change after the elastic limit is crossed");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "yield_ratio", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yield_ratio");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Elastic Limit",
	                         "How much the spring has to be stretched/compressed in order to change it's rest length");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "spring_frames", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Spring Frames",
	                         "Create springs for this number of frames since particles birth (0 is always)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* Viscosity */
	prop = RNA_def_property(srna, "linear_viscosity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "viscosity_omega");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Viscosity", "Linear viscosity");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "stiff_viscosity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "viscosity_beta");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Stiff viscosity", "Creates viscosity for expanding fluid)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* Double density relaxation */
	prop = RNA_def_property(srna, "stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stiffness_k");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Stiffness", "How incompressible the fluid is");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "repulsion", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stiffness_knear");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Repulsion Factor",
	                         "How strongly the fluid tries to keep from clustering (factor of stiffness)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "rest_density", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rest_density");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Rest Density", "Fluid rest density");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* Buoyancy */
	prop = RNA_def_property(srna, "buoyancy", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "buoyancy");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Buoyancy",
	                         "Artificial buoyancy force in negative gravity direction based on pressure "
	                         "differences inside the fluid");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* Factor flags */

	prop = RNA_def_property(srna, "factor_repulsion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPH_FAC_REPULSION);
	RNA_def_property_ui_text(prop, "Factor Repulsion", "Repulsion is a factor of stiffness");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "factor_density", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPH_FAC_DENSITY);
	RNA_def_property_ui_text(prop, "Factor Density",
	                         "Density is calculated as a factor of default density (depends on particle size)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "factor_radius", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPH_FAC_RADIUS);
	RNA_def_property_ui_text(prop, "Factor Radius", "Interaction radius is a factor of 4 * particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "factor_stiff_viscosity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPH_FAC_VISCOSITY);
	RNA_def_property_ui_text(prop, "Factor Stiff Viscosity", "Stiff viscosity is a factor of normal viscosity");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "factor_rest_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPH_FAC_REST_LENGTH);
	RNA_def_property_ui_text(prop, "Factor Rest Length", "Spring rest length is a factor of 2 * particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");
}

static void rna_def_particle_settings_mtex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem texco_items[] = {
		{TEXCO_GLOB, "GLOBAL", 0, "Global", "Use global coordinates for the texture coordinates"},
		{TEXCO_OBJECT, "OBJECT", 0, "Object", "Use linked object's coordinates for texture coordinates"},
		{TEXCO_UV, "UV", 0, "UV", "Use UV coordinates for texture coordinates"},
		{TEXCO_ORCO, "ORCO", 0, "Generated", "Use the original undeformed coordinates of the object"},
		{TEXCO_STRAND, "STRAND", 0, "Strand / Particle",
		               "Use normalized strand texture coordinate (1D) or particle age (X) and trail position (Y)"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_mapping_items[] = {
		{MTEX_FLAT, "FLAT", 0, "Flat", "Map X and Y coordinates directly"},
		{MTEX_CUBE, "CUBE", 0, "Cube", "Map using the normal vector"},
		{MTEX_TUBE, "TUBE", 0, "Tube", "Map with Z as central axis"},
		{MTEX_SPHERE, "SPHERE", 0, "Sphere", "Map with Z as central axis"},
		{0, NULL, 0, NULL, NULL}};
		
	static EnumPropertyItem prop_x_mapping_items[] = {
		{0, "NONE", 0, "None", ""},
		{1, "X", 0, "X", ""},
		{2, "Y", 0, "Y", ""},
		{3, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}};
		
	static EnumPropertyItem prop_y_mapping_items[] = {
		{0, "NONE", 0, "None", ""},
		{1, "X", 0, "X", ""},
		{2, "Y", 0, "Y", ""},
		{3, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}};
		
	static EnumPropertyItem prop_z_mapping_items[] = {
		{0, "NONE", 0, "None", ""},
		{1, "X", 0, "X", ""},
		{2, "Y", 0, "Y", ""},
		{3, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "ParticleSettingsTextureSlot", "TextureSlot");
	RNA_def_struct_sdna(srna, "MTex");
	RNA_def_struct_ui_text(srna, "Particle Settings Texture Slot",
	                       "Texture slot for textures in a Particle Settings datablock");

	prop = RNA_def_property(srna, "texture_coords", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texco");
	RNA_def_property_enum_items(prop, texco_items);
	RNA_def_property_ui_text(prop, "Texture Coordinates",
	                         "Texture coordinates used to map the texture onto the background");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object to use for mapping with Object texture coordinates");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvname");
	RNA_def_property_ui_text(prop, "UV Map", "UV map to use for mapping with UV texture coordinates");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "mapping_x", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projx");
	RNA_def_property_enum_items(prop, prop_x_mapping_items);
	RNA_def_property_ui_text(prop, "X Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");
	
	prop = RNA_def_property(srna, "mapping_y", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projy");
	RNA_def_property_enum_items(prop, prop_y_mapping_items);
	RNA_def_property_ui_text(prop, "Y Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");
	
	prop = RNA_def_property(srna, "mapping_z", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projz");
	RNA_def_property_enum_items(prop, prop_z_mapping_items);
	RNA_def_property_ui_text(prop, "Z Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");
	
	prop = RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mapping_items);
	RNA_def_property_ui_text(prop, "Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* map to */
	prop = RNA_def_property(srna, "use_map_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_TIME);
	RNA_def_property_ui_text(prop, "Emission Time", "Affect the emission time of the particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_map_life", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_LIFE);
	RNA_def_property_ui_text(prop, "Life Time", "Affect the life time of the particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_map_density", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_DENS);
	RNA_def_property_ui_text(prop, "Density", "Affect the density of the particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_map_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_SIZE);
	RNA_def_property_ui_text(prop, "Size", "Affect the particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_map_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_IVEL);
	RNA_def_property_ui_text(prop, "Initial Velocity", "Affect the particle initial velocity");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_map_field", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_FIELD);
	RNA_def_property_ui_text(prop, "Force Field", "Affect the particle force fields");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_map_gravity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_GRAVITY);
	RNA_def_property_ui_text(prop, "Gravity", "Affect the particle gravity");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_map_damp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_DAMP);
	RNA_def_property_ui_text(prop, "Damp", "Affect the particle velocity damping");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "use_map_clump", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_CLUMP);
	RNA_def_property_ui_text(prop, "Clump", "Affect the child clumping");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_map_kink", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_KINK);
	RNA_def_property_ui_text(prop, "Kink", "Affect the child kink");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "use_map_rough", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_ROUGH);
	RNA_def_property_ui_text(prop, "Rough", "Affect the child rough");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "use_map_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", PAMAP_LENGTH);
	RNA_def_property_ui_text(prop, "Length", "Affect the child hair length");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");


	/* influence factors */
	prop = RNA_def_property(srna, "time_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "timefac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Emission Time Factor", "Amount texture affects particle emission time");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "life_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lifefac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Life Time Factor", "Amount texture affects particle life time");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "density_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "padensfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Density Factor", "Amount texture affects particle density");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "size_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sizefac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Size Factor", "Amount texture affects physical particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "velocity_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ivelfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Velocity Factor", "Amount texture affects particle initial velocity");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");


	prop = RNA_def_property(srna, "field_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fieldfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Field Factor", "Amount texture affects particle force fields");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "gravity_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gravityfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Gravity Factor", "Amount texture affects particle gravity");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "damp_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dampfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Damp Factor", "Amount texture affects particle damping");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");


	prop = RNA_def_property(srna, "length_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lengthfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Length Factor", "Amount texture affects child hair length");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "clump_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clumpfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Clump Factor", "Amount texture affects child clump");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "kink_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kinkfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Kink Factor", "Amount texture affects child kink");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "rough_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "roughfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Rough Factor", "Amount texture affects child roughness");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");
}

static void rna_def_particle_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem type_items[] = {
		{PART_EMITTER, "EMITTER", 0, "Emitter", ""},
		/*{PART_REACTOR, "REACTOR", 0, "Reactor", ""}, */
		{PART_HAIR, "HAIR", 0, "Hair", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem phys_type_items[] = {
		{PART_PHYS_NO, "NO", 0, "No", ""},
		{PART_PHYS_NEWTON, "NEWTON", 0, "Newtonian", ""},
		{PART_PHYS_KEYED, "KEYED", 0, "Keyed", ""},
		{PART_PHYS_BOIDS, "BOIDS", 0, "Boids", ""},
		{PART_PHYS_FLUID, "FLUID", 0, "Fluid", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem rot_mode_items[] = {
		{0, "NONE", 0, "None", ""},
		{PART_ROT_NOR, "NOR", 0, "Normal", ""},
		{PART_ROT_VEL, "VEL", 0, "Velocity / Hair", ""},
		{PART_ROT_GLOB_X, "GLOB_X", 0, "Global X", ""},
		{PART_ROT_GLOB_Y, "GLOB_Y", 0, "Global Y", ""},
		{PART_ROT_GLOB_Z, "GLOB_Z", 0, "Global Z", ""},
		{PART_ROT_OB_X, "OB_X", 0, "Object X", ""},
		{PART_ROT_OB_Y, "OB_Y", 0, "Object Y", ""},
		{PART_ROT_OB_Z, "OB_Z", 0, "Object Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem ave_mode_items[] = {
		{0, "NONE", 0, "None", ""},
		{PART_AVE_VELOCITY, "VELOCITY", 0, "Velocity", ""},
		{PART_AVE_HORIZONTAL, "HORIZONTAL", 0, "Horizontal", ""},
		{PART_AVE_VERTICAL, "VERTICAL", 0, "Vertical", ""},
		{PART_AVE_GLOBAL_X, "GLOBAL_X", 0, "Global X", ""},
		{PART_AVE_GLOBAL_Y, "GLOBAL_Y", 0, "Global Y", ""},
		{PART_AVE_GLOBAL_Z, "GLOBAL_Z", 0, "Global Z", ""},
		{PART_AVE_RAND, "RAND", 0, "Random", ""} ,
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem react_event_items[] = {
		{PART_EVENT_DEATH, "DEATH", 0, "Death", ""},
		{PART_EVENT_COLLIDE, "COLLIDE", 0, "Collision", ""},
		{PART_EVENT_NEAR, "NEAR", 0, "Near", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem child_type_items[] = {
		{0, "NONE", 0, "None", ""},
		{PART_CHILD_PARTICLES, "SIMPLE", 0, "Simple", ""},
		{PART_CHILD_FACES, "INTERPOLATED", 0, "Interpolated", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/*TODO: names, tooltips */
#if 0
	static EnumPropertyItem rot_from_items[] = {
		{PART_ROT_KEYS, "KEYS", 0, "keys", ""},
		{PART_ROT_ZINCR, "ZINCR", 0, "zincr", ""},
		{PART_ROT_IINCR, "IINCR", 0, "iincr", ""},
		{0, NULL, 0, NULL, NULL}
	};
#endif
	static EnumPropertyItem integrator_type_items[] = {
		{PART_INT_EULER, "EULER", 0, "Euler", ""},
		{PART_INT_VERLET, "VERLET", 0, "Verlet", ""},
		{PART_INT_MIDPOINT, "MIDPOINT", 0, "Midpoint", ""},
		{PART_INT_RK4, "RK4", 0, "RK4", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem kink_type_items[] = {
		{PART_KINK_NO, "NO", 0, "Nothing", ""},
		{PART_KINK_CURL, "CURL", 0, "Curl", ""},
		{PART_KINK_RADIAL, "RADIAL", 0, "Radial", ""},
		{PART_KINK_WAVE, "WAVE", 0, "Wave", ""},
		{PART_KINK_BRAID, "BRAID", 0, "Braid", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem kink_axis_items[] = {
		{0, "X", 0, "X", ""},
		{1, "Y", 0, "Y", ""},
		{2, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bb_align_items[] = {
		{PART_BB_X, "X", 0, "X", ""},
		{PART_BB_Y, "Y", 0, "Y", ""},
		{PART_BB_Z, "Z", 0, "Z", ""},
		{PART_BB_VIEW, "VIEW", 0, "View", ""},
		{PART_BB_VEL, "VEL", 0, "Velocity", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bb_anim_items[] = {
		{PART_BB_ANIM_NONE, "NONE", 0, "None", ""},
		{PART_BB_ANIM_AGE, "AGE", 0, "Age", ""},
		{PART_BB_ANIM_FRAME, "FRAME", 0, "Frame", ""},
		{PART_BB_ANIM_ANGLE, "ANGLE", 0, "Angle", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bb_split_offset_items[] = {
		{PART_BB_OFF_NONE, "NONE", 0, "None", ""},
		{PART_BB_OFF_LINEAR, "LINEAR", 0, "Linear", ""},
		{PART_BB_OFF_RANDOM, "RANDOM", 0, "Random", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem draw_col_items[] = {
		{PART_DRAW_COL_NONE, "NONE", 0, "None", ""},
		{PART_DRAW_COL_MAT, "MATERIAL", 0, "Material", ""},
		{PART_DRAW_COL_VEL, "VELOCITY", 0, "Velocity", ""},
		{PART_DRAW_COL_ACC, "ACCELERATION", 0, "Acceleration", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ParticleSettings", "ID");
	RNA_def_struct_ui_text(srna, "Particle Settings", "Particle settings, reusable by multiple particle systems");
	RNA_def_struct_ui_icon(srna, ICON_PARTICLE_DATA);

	rna_def_mtex_common(brna, srna, "rna_ParticleSettings_mtex_begin", "rna_ParticleSettings_active_texture_get",
	                    "rna_ParticleSettings_active_texture_set", NULL, "ParticleSettingsTextureSlot",
	                    "ParticleSettingsTextureSlots", "rna_Particle_reset");

	/* fluid particle type can't be checked from the type value in rna as it's not shown in the menu */
	prop = RNA_def_property(srna, "is_fluid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_PartSettings_is_fluid_get", NULL);
	RNA_def_property_ui_text(prop, "Fluid", "Particles were created by a fluid simulation");

	/* flag */
	prop = RNA_def_property(srna, "use_react_start_end", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_REACT_STA_END);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Start/End", "Give birth to unreacted particles eventually");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_react_multiple", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_REACT_MULTIPLE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Multi React", "React multiple times");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "regrow_hair", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_HAIR_REGROW);
	RNA_def_property_ui_text(prop, "Regrow", "Regrow hair for each frame");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "show_unborn", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_UNBORN);
	RNA_def_property_ui_text(prop, "Unborn", "Show particles before they are emitted");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_dead", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_DIED);
	RNA_def_property_ui_text(prop, "Died", "Show particles after they have died");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_emit_random", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_TRAND);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Random", "Emit in random order of elements");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_even_distribution", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_EDISTR);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Even Distribution",
	                         "Use even distribution from faces based on face areas or edge lengths");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");
 
	prop = RNA_def_property(srna, "use_die_on_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_DIE_ON_COL);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Die on hit", "Particles die when they collide with a deflector object");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_size_deflect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SIZE_DEFL);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Size Deflect", "Use particle's size in deflection");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_rotations", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ROTATIONS);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Rotations", "Calculate particle rotations");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_dynamic_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ROT_DYN);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Dynamic", "Particle rotations are effected by collisions and effectors");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_multiply_size_mass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SIZEMASS);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Mass from Size", "Multiply mass by particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_advanced_hair", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", PART_HIDE_ADVANCED_HAIR);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Advanced", "Use full physics calculations for growing hair");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "lock_boids_to_surface", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_BOIDS_2D);
	RNA_def_property_ui_text(prop, "Boids 2D", "Constrain boids to a surface");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "use_hair_bspline", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_HAIR_BSPLINE);
	RNA_def_property_ui_text(prop, "B-Spline", "Interpolate hair using B-Splines");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "invert_grid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_GRID_INVERT);
	RNA_def_property_ui_text(prop, "Invert Grid", "Invert what is considered object and what is not");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "hexagonal_grid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_GRID_HEXAGONAL);
	RNA_def_property_ui_text(prop, "Hexagonal Grid", "Create the grid in a hexagonal pattern");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "apply_effector_to_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_EFFECT);
	RNA_def_property_ui_text(prop, "Effect Children", "Apply effectors to children");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "create_long_hair_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_LONG_HAIR);
	RNA_def_property_ui_text(prop, "Long Hair", "Calculate children that suit long hair well");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "apply_guide_to_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_GUIDE);
	RNA_def_property_ui_text(prop, "apply_guide_to_children", "");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_self_effect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SELF_EFFECT);
	RNA_def_property_ui_text(prop, "Self Effect", "Particle effectors effect themselves");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");


	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Type", "Particle Type");
	RNA_def_property_update(prop, 0, "rna_Particle_change_type");

	prop = RNA_def_property(srna, "emit_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "from");
	RNA_def_property_enum_items(prop, part_reactor_from_items);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Particle_from_itemf");
	RNA_def_property_ui_text(prop, "Emit From", "Where to emit particles from");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "distr");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, part_dist_items);
	RNA_def_property_enum_items(prop, part_draw_as_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Particle_dist_itemf");
	RNA_def_property_ui_text(prop, "Distribution", "How to distribute particles on selected element");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* physics modes */
	prop = RNA_def_property(srna, "physics_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "phystype");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, phys_type_items);
	RNA_def_property_ui_text(prop, "Physics Type", "Particle physics type");
	RNA_def_property_update(prop, 0, "rna_Particle_change_physics");

	prop = RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rotmode");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, rot_mode_items);
	RNA_def_property_ui_text(prop, "Orientation axis", "Particle orientation axis (does not affect Explode modifier's results)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "angular_velocity_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "avemode");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, ave_mode_items);
	RNA_def_property_ui_text(prop, "Angular Velocity Axis", "What axis is used to change particle rotation with time");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "react_event", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "reactevent");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, react_event_items);
	RNA_def_property_ui_text(prop, "React On", "The event of target particles to react on");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/*draw flag*/
	prop = RNA_def_property(srna, "show_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_VEL);
	RNA_def_property_ui_text(prop, "Velocity", "Show particle velocity");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "show_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_SIZE);
	RNA_def_property_ui_text(prop, "Size", "Show particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_render_emitter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_EMITTER);
	RNA_def_property_ui_text(prop, "Emitter", "Render emitter Object also");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "show_health", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_HEALTH);
	RNA_def_property_ui_text(prop, "Health", "Draw boid health");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_absolute_path_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_ABS_PATH_TIME);
	RNA_def_property_ui_text(prop, "Absolute Path Time", "Path timing is in absolute frames");
	RNA_def_property_update(prop, 0, "rna_Particle_abspathtime_update");

	prop = RNA_def_property(srna, "use_parent_particles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_PARENT);
	RNA_def_property_ui_text(prop, "Parents", "Render parent particles");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "show_number", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_NUM);
	RNA_def_property_ui_text(prop, "Number", "Show particle number");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_group_pick_random", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_RAND_GR);
	RNA_def_property_ui_text(prop, "Pick Random", "Pick objects from group randomly");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_group_count", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_COUNT_GR);
	RNA_def_property_ui_text(prop, "Use Count", "Use object multiple times in the same group");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_global_dupli", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_GLOBAL_OB);
	RNA_def_property_ui_text(prop, "Global", "Use object's global coordinates for duplication");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_rotation_dupli", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_ROTATE_OB);
	RNA_def_property_ui_text(prop, "Rotation",
	                         "Use object's rotation for duplication (global x-axis is aligned "
	                         "particle rotation axis)");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_render_adaptive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_REN_ADAPT);
	RNA_def_property_ui_text(prop, "Adaptive render", "Draw steps of the particle path");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_velocity_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_VEL_LENGTH);
	RNA_def_property_ui_text(prop, "Speed", "Multiply line length by particle speed");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_whole_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_WHOLE_GR);
	RNA_def_property_ui_text(prop, "Whole Group", "Use whole group at once");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "use_strand_primitive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_REN_STRAND);
	RNA_def_property_ui_text(prop, "Strand render", "Use the strand primitive for rendering");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "draw_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "draw_as");
	RNA_def_property_enum_items(prop, part_draw_as_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Particle_draw_as_itemf");
	RNA_def_property_ui_text(prop, "Particle Drawing", "How particles are drawn in viewport");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "render_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ren_as");
	RNA_def_property_enum_items(prop, part_ren_as_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Particle_ren_as_itemf");
	RNA_def_property_ui_text(prop, "Particle Rendering", "How particles are rendered");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "draw_color", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "draw_col");
	RNA_def_property_enum_items(prop, draw_col_items);
	RNA_def_property_ui_text(prop, "Draw Color", "Draw additional particle data as a color");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "draw_size", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_range(prop, 0, 100, 1, 0);
	RNA_def_property_ui_text(prop, "Draw Size", "Size of particles on viewport in pixels (0=default)");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "child_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "childtype");
	RNA_def_property_enum_items(prop, child_type_items);
	RNA_def_property_ui_text(prop, "Children From", "Create child particles");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "draw_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_range(prop, 0, 7, 1, 0);
	RNA_def_property_ui_text(prop, "Steps", "How many steps paths are drawn with (power of 2)");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "render_step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ren_step");
	RNA_def_property_range(prop, 0, 20);
	RNA_def_property_ui_range(prop, 0, 9, 1, 0);
	RNA_def_property_ui_text(prop, "Render", "How many steps paths are rendered with (power of 2)");

	prop = RNA_def_property(srna, "hair_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 2, 50);
	RNA_def_property_ui_text(prop, "Segments", "Number of hair segments");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");


	/*TODO: not found in UI, readonly? */
	prop = RNA_def_property(srna, "keys_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, SHRT_MAX);/*TODO:min,max */
	RNA_def_property_ui_text(prop, "Keys Step", "");

	/* adaptive path rendering */
	prop = RNA_def_property(srna, "adaptive_angle", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "adapt_angle");
	RNA_def_property_range(prop, 0, 45);
	RNA_def_property_ui_text(prop, "Degrees", "How many degrees path has to curve to make another render segment");

	prop = RNA_def_property(srna, "adaptive_pixel", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "adapt_pix");
	RNA_def_property_range(prop, 0, 50);
	RNA_def_property_ui_text(prop, "Pixel", "How many pixels path has to cover to make another render segment");

	prop = RNA_def_property(srna, "draw_percentage", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "disp");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Display", "Percentage of particles to display in 3D view");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "material", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "omat");
	RNA_def_property_range(prop, 1, 32767);
	RNA_def_property_ui_text(prop, "Material", "Material used for the particles");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");


	/* not used anywhere, why is this in DNA??? */
#if 0
	prop = RNA_def_property(srna, "rotate_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rotfrom");
	RNA_def_property_enum_items(prop, rot_from_items);
	RNA_def_property_ui_text(prop, "Rotate From", "");
#endif

	prop = RNA_def_property(srna, "integrator", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, integrator_type_items);
	RNA_def_property_ui_text(prop, "Integration",
	                         "Algorithm used to calculate physics, from the fastest to the "
	                         "most stable/accurate: Midpoint, Euler, Verlet, RK4 (Old)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "kink", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, kink_type_items);
	RNA_def_property_ui_text(prop, "Kink", "Type of periodic offset on the path");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "kink_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, kink_axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Which axis to use for offset");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	/* billboards */
	prop = RNA_def_property(srna, "lock_billboard", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_BB_LOCK);
	RNA_def_property_ui_text(prop, "Lock Billboard", "Lock the billboards align axis");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "billboard_align", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bb_align");
	RNA_def_property_enum_items(prop, bb_align_items);
	RNA_def_property_ui_text(prop, "Align to", "In respect to what the billboards are aligned");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "billboard_uv_split", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "bb_uv_split");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_range(prop, 1, 10, 1, 0);
	RNA_def_property_ui_text(prop, "UV Split", "Number of rows/columns to split UV coordinates for billboards");

	prop = RNA_def_property(srna, "billboard_animation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bb_anim");
	RNA_def_property_enum_items(prop, bb_anim_items);
	RNA_def_property_ui_text(prop, "Animate", "How to animate billboard textures");

	prop = RNA_def_property(srna, "billboard_offset_split", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bb_split_offset");
	RNA_def_property_enum_items(prop, bb_split_offset_items);
	RNA_def_property_ui_text(prop, "Offset", "How to offset billboard textures");

	prop = RNA_def_property(srna, "billboard_tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bb_tilt");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Tilt", "Tilt of the billboards");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "color_maximum", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "color_vec_max");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Color Maximum", "Maximum length of the particle color vector");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "billboard_tilt_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bb_rand_tilt");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Tilt", "Random tilt of the billboards");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "billboard_offset", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "bb_offset");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, -100.0f, 100.0f);
	RNA_def_property_ui_range(prop, -1.0, 1.0, 0.1, 3);
	RNA_def_property_ui_text(prop, "Billboard Offset", "");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "billboard_size", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "bb_size");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, 0.001f, 10.0f);
	RNA_def_property_ui_text(prop, "Billboard Scale", "Scale billboards relative to particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "billboard_velocity_head", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "bb_vel_head");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Billboard Velocity Head", "Scale billboards by velocity");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "billboard_velocity_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "bb_vel_tail");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Billboard Velocity Tail", "Scale billboards by velocity");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	/* simplification */
	prop = RNA_def_property(srna, "use_simplify", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "simplify_flag", PART_SIMPLIFY_ENABLE);
	RNA_def_property_ui_text(prop, "Child Simplification",
	                         "Remove child strands as the object becomes smaller on the screen");

	prop = RNA_def_property(srna, "use_simplify_viewport", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "simplify_flag", PART_SIMPLIFY_VIEWPORT);
	RNA_def_property_ui_text(prop, "Viewport", "");

	prop = RNA_def_property(srna, "simplify_refsize", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "simplify_refsize");
	RNA_def_property_range(prop, 1, 32768);
	RNA_def_property_ui_text(prop, "Reference Size", "Reference size in pixels, after which simplification begins");

	prop = RNA_def_property(srna, "simplify_rate", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Rate", "Speed of simplification");

	prop = RNA_def_property(srna, "simplify_transition", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Transition", "Transition period for fading out strands");

	prop = RNA_def_property(srna, "simplify_viewport", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 0.999f);
	RNA_def_property_ui_text(prop, "Rate", "Speed of Simplification");

	/* general values */
	prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sta");/*optional if prop names are the same */
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_funcs(prop, NULL, "rna_PartSettings_start_set", NULL);
	RNA_def_property_ui_text(prop, "Start", "Frame number to start emitting particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "end");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);

	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_funcs(prop, NULL, "rna_PartSettings_end_set", NULL);
	RNA_def_property_ui_text(prop, "End", "Frame number to stop emitting particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_TIME);
	RNA_def_property_range(prop, 1.0f, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Lifetime", "Life span of the particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "lifetime_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randlife");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random", "Give the particle life a random variation");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "time_tweak", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "timetweak");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0, 10, 1, 3);
	RNA_def_property_ui_text(prop, "Tweak", "A multiplier for physics timestep (1.0 means one frame = 1/25 seconds)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "timestep", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_PartSettings_timestep_get", "rna_PartSetings_timestep_set", NULL);
	RNA_def_property_range(prop, 0.0001, 100.0);
	RNA_def_property_ui_range(prop, 0.01, 10, 1, 3);
	RNA_def_property_ui_text(prop, "Timestep", "The simulation timestep per frame (seconds per frame)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "adaptive_subframes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "time_flag", PART_TIME_AUTOSF);
	RNA_def_property_ui_text(prop, "Automatic Subframes", "Automatically set the number of subframes");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "subframes", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_text(prop, "Subframes",
	                         "Subframes to simulate for improved stability and finer granularity simulations "
	                         "(dt = timestep / (subframes + 1))");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "courant_target", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 10);
	RNA_def_property_float_default(prop, 0.2);
	RNA_def_property_ui_text(prop, "Adaptive Subframe Threshold",
	                         "The relative distance a particle can move before requiring more subframes "
	                         "(target Courant number); 0.1-0.3 is the recommended range");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "jitter_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_sdna(prop, NULL, "jitfac");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Amount", "Amount of jitter applied to the sampling");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "effect_hair", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eff_hair");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Stiffness", "Hair stiffness for effectors");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "count", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "totpart");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	/* This limit is for those freaks who have the machine power to handle it. */
	/* 10M particles take around 2.2 Gb of memory / disk space in saved file and */
	/* each cached frame takes around 0.5 Gb of memory / disk space depending on cache mode. */
	RNA_def_property_range(prop, 0, 10000000);
	RNA_def_property_ui_range(prop, 0, 100000, 1, 0);
	RNA_def_property_ui_text(prop, "Number", "Total number of particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "userjit", PROP_INT, PROP_UNSIGNED);/*TODO: can we get a better name for userjit? */
	RNA_def_property_int_sdna(prop, NULL, "userjit");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_text(prop, "P/F", "Emission locations / face (0 = automatic)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "grid_resolution", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "grid_res");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, 250); /* ~15M particles in a cube (ouch!), but could be very usable in a plane */
	RNA_def_property_ui_range(prop, 1, 50, 1, 0); /* ~100k particles in a cube */
	RNA_def_property_ui_text(prop, "Resolution", "The resolution of the particle grid");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "grid_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "grid_rand");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Grid Randomness", "Add random offset to the grid locations");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "effector_amount", PROP_INT, PROP_UNSIGNED);
	/* in theory PROP_ANIMATABLE perhaps should be cleared, but animating this can give some interesting results! */
	RNA_def_property_range(prop, 0, 10000); /* 10000 effectors will bel SLOW, but who knows */
	RNA_def_property_ui_range(prop, 0, 100, 1, 0);
	RNA_def_property_ui_text(prop, "Effector Number", "How many particles are effectors (0 is all particles)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* initial velocity factors */
	prop = RNA_def_property(srna, "normal_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "normfac");/*optional if prop names are the same */
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_range(prop, 0, 100, 1, 3);
	RNA_def_property_ui_text(prop, "Normal", "Let the surface normal give the particle a starting speed");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "object_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "obfac");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Object", "Let the object give the particle a starting speed");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "factor_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randfac");/*optional if prop names are the same */
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0, 100, 1, 3);
	RNA_def_property_ui_text(prop, "Random", "Give the starting speed a random variation");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "particle_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "partfac");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Particle", "Let the target particle give the particle a starting speed");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "tangent_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tanfac");
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_range(prop, -100, 100, 1, 2);
	RNA_def_property_ui_text(prop, "Tangent", "Let the surface tangent give the particle a starting speed");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "tangent_phase", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tanphase");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Rot", "Rotate the surface tangent");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "reactor_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "reactfac");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Reactor",
	                         "Let the vector away from the target particle's location give the particle "
	                         "a starting speed");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "object_align_factor", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "ob_vel");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, -100, 100, 1, 3);
	RNA_def_property_ui_text(prop, "Object Aligned",
	                         "Let the emitter object orientation give the particle a starting speed");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "angular_velocity_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "avefac");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, -100, 100, 10, 3);
	RNA_def_property_ui_text(prop, "Angular Velocity", "Angular velocity amount (in radians per second)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "phase_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "phasefac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Phase", "Rotation around the chosen orientation axis");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "rotation_factor_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randrotfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Orientation", "Randomize particle orientation");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "phase_factor_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randphasefac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Phase", "Randomize rotation around the chosen orientation axis");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "hair_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_PartSetting_hairlength_get", "rna_PartSetting_hairlength_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Hair Length", "Length of the hair");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* physical properties */
	prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.01, 100, 1, 3);
	RNA_def_property_ui_text(prop, "Mass", "Mass of the particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "particle_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_range(prop, 0.001f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.01, 100, 1, 3);
	RNA_def_property_ui_text(prop, "Size", "The size of the particles");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "size_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randsize");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Size", "Give the particle size a random variation");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");


	/* global physical properties */
	prop = RNA_def_property(srna, "drag_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dragfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Drag", "Amount of air-drag");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "brownian_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "brownfac");
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0, 20, 1, 3);
	RNA_def_property_ui_text(prop, "Brownian", "Amount of Brownian motion");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dampfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Damp", "Amount of damping");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* random length */
	prop = RNA_def_property(srna, "length_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randlength");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Length", "Give path length a random variation");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	/* children */
	prop = RNA_def_property(srna, "child_nbr", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "child_nbr");/*optional if prop names are the same */
	RNA_def_property_range(prop, 0, 100000);
	RNA_def_property_ui_range(prop, 0, 1000, 1, 0);
	RNA_def_property_ui_text(prop, "Children Per Parent", "Number of children/parent");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "rendered_child_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ren_child_nbr");
	RNA_def_property_range(prop, 0, 100000);
	RNA_def_property_ui_range(prop, 0, 10000, 1, 0);
	RNA_def_property_ui_text(prop, "Rendered Children", "Number of children/parent for rendering");

	prop = RNA_def_property(srna, "virtual_parents", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "parents");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Virtual Parents", "Relative amount of virtual parents");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "child_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childsize");
	RNA_def_property_range(prop, 0.001f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.01f, 100.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Child Size", "A multiplier for the child particle size");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "child_size_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childrandsize");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Child Size", "Random variation to the size of the child particles");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "child_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childrad");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Child Radius", "Radius of children around parent");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "child_roundness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childflat");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Child Roundness", "Roundness of children around parent");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	/* clumping */
	prop = RNA_def_property(srna, "clump_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clumpfac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Clump", "Amount of clumping");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "clump_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clumppow");
	RNA_def_property_range(prop, -0.999f, 0.999f);
	RNA_def_property_ui_text(prop, "Shape", "Shape of clumping");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");


	/* kink */
	prop = RNA_def_property(srna, "kink_amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_amp");
	RNA_def_property_range(prop, -100000.0f, 100000.0f);
	RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Amplitude", "The amplitude of the offset");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "kink_amplitude_clump", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_amp_clump");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Amplitude Clump", "How much clump affects kink amplitude");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "kink_frequency", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_freq");
	RNA_def_property_range(prop, -100000.0f, 100000.0f);
	RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Frequency", "The frequency of the offset (1/total length)");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "kink_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -0.999f, 0.999f);
	RNA_def_property_ui_text(prop, "Shape", "Adjust the offset to the beginning/end");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "kink_flat", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Flatness", "How flat the hairs are");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	/* rough */
	prop = RNA_def_property(srna, "roughness_1", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough1");
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Rough1", "Amount of location dependent rough");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "roughness_1_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough1_size");
	RNA_def_property_range(prop, 0.01f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.01f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Size1", "Size of location dependent rough");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "roughness_2", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2");
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Rough2", "Amount of random rough");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "roughness_2_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2_size");
	RNA_def_property_range(prop, 0.01f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.01f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Size2", "Size of random rough");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "roughness_2_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2_thres");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Amount of particles left untouched by random rough");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "roughness_endpoint", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough_end");
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Rough Endpoint", "Amount of end point rough");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "roughness_end_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough_end_shape");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Shape", "Shape of end point rough");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "child_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clength");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Length", "Length of child paths");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "child_length_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clength_thres");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Amount of particles left untouched by child path length");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	/* parting */
	prop = RNA_def_property(srna, "child_parting_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "parting_fac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Parting Factor", "Create parting in the children based on parent strands");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "child_parting_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "parting_min");
	RNA_def_property_range(prop, 0.0f, 180.0f);
	RNA_def_property_ui_text(prop, "Parting Minimum",
	                         "Minimum root to tip angle (tip distance/root distance for long hair)");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "child_parting_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "parting_max");
	RNA_def_property_range(prop, 0.0f, 180.0f);
	RNA_def_property_ui_text(prop, "Parting Maximum",
	                         "Maximum root to tip angle (tip distance/root distance for long hair)");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	/* branching */
	prop = RNA_def_property(srna, "branch_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "branch_thres");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Threshold of branching");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	/* drawing stuff */
	prop = RNA_def_property(srna, "line_length_tail", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_PartSetting_linelentail_get", "rna_PartSetting_linelentail_set", NULL);
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Back", "Length of the line's tail");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "line_length_head", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_PartSetting_linelenhead_get", "rna_PartSetting_linelenhead_set", NULL);
	RNA_def_property_range(prop, 0.0f, 100000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Head", "Length of the line's head");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "path_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "path_start");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_PartSetting_pathstartend_range");
	RNA_def_property_ui_text(prop, "Path Start", "Starting time of drawn path");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "path_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "path_end");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_PartSetting_pathstartend_range");
	RNA_def_property_ui_text(prop, "Path End", "End time of drawn path");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "trail_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "trail_count");
	RNA_def_property_range(prop, 1, 100000);
	RNA_def_property_ui_range(prop, 1, 100, 1, 0);
	RNA_def_property_ui_text(prop, "Trail Count", "Number of trail particles");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	/* keyed particles */
	prop = RNA_def_property(srna, "keyed_loops", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "keyed_loops");
	RNA_def_property_range(prop, 1.0f, 10000.0f);
	RNA_def_property_ui_range(prop, 1.0f, 100.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Loop count", "Number of times the keys are looped");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");
	
	/* draw objects & groups */
	prop = RNA_def_property(srna, "dupli_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "dup_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dupli Group", "Show Objects in this Group in place of particles");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "dupli_weights", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "dupliweights", NULL);
	RNA_def_property_struct_type(prop, "ParticleDupliWeight");
	RNA_def_property_ui_text(prop, "Dupli Group Weights", "Weights for all of the objects in the dupli group");

	prop = RNA_def_property(srna, "active_dupliweight", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ParticleDupliWeight");
	RNA_def_property_pointer_funcs(prop, "rna_ParticleDupliWeight_active_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Dupli Object", "");

	prop = RNA_def_property(srna, "active_dupliweight_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_ParticleDupliWeight_active_index_get",
	                           "rna_ParticleDupliWeight_active_index_set",
	                           "rna_ParticleDupliWeight_active_index_range");
	RNA_def_property_ui_text(prop, "Active Dupli Object Index", "");

	prop = RNA_def_property(srna, "dupli_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "dup_ob");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dupli Object", "Show this Object in place of particles");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_dependency");

	prop = RNA_def_property(srna, "billboard_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "bb_ob");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Billboard Object", "Billboards face this object (default is active camera)");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	/* boids */
	prop = RNA_def_property(srna, "boids", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BoidSettings");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Boid Settings", "");
	
	/* Fluid particles */
	prop = RNA_def_property(srna, "fluid", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "SPHFluidSettings");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "SPH Fluid Settings", "");

	/* Effector weights */
	prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");
	
	/* animation here? */
	rna_def_animdata_common(srna);

	prop = RNA_def_property(srna, "force_field_1", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "pd");
	RNA_def_property_struct_type(prop, "FieldSettings");
	RNA_def_property_pointer_funcs(prop, "rna_Particle_field1_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Force Field 1", "");

	prop = RNA_def_property(srna, "force_field_2", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "pd2");
	RNA_def_property_struct_type(prop, "FieldSettings");
	RNA_def_property_pointer_funcs(prop, "rna_Particle_field2_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Force Field 2", "");
}

static void rna_def_particle_target(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem mode_items[] = {
		{PTARGET_MODE_FRIEND, "FRIEND", 0, "Friend", ""},
		{PTARGET_MODE_NEUTRAL, "NEUTRAL", 0, "Neutral", ""},
		{PTARGET_MODE_ENEMY, "ENEMY", 0, "Enemy", ""},
		{0, NULL, 0, NULL, NULL}
	};


	srna = RNA_def_struct(brna, "ParticleTarget", NULL);
	RNA_def_struct_ui_text(srna, "Particle Target", "Target particle system");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleTarget_name_get", "rna_ParticleTarget_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Particle target name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target Object",
	                         "The object that has the target particle system (empty if same object)");
	RNA_def_property_update(prop, 0, "rna_Particle_target_reset");

	prop = RNA_def_property(srna, "system", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "psys");
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_text(prop, "Target Particle System", "The index of particle system on the target object");
	RNA_def_property_update(prop, 0, "rna_Particle_target_reset");

	prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "time");
	RNA_def_property_range(prop, 0.0, 30000.0f); /*TODO: replace 30000 with MAXFRAMEF when available in 2.5 */
	RNA_def_property_ui_text(prop, "Time", "");
	RNA_def_property_update(prop, 0, "rna_Particle_target_redo");

	prop = RNA_def_property(srna, "duration", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "duration");
	RNA_def_property_range(prop, 0.0, 30000.0f); /*TODO: replace 30000 with MAXFRAMEF when available in 2.5 */
	RNA_def_property_ui_text(prop, "Duration", "");
	RNA_def_property_update(prop, 0, "rna_Particle_target_redo");

	prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTARGET_VALID);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Valid", "Keyed particles target is valid");

	prop = RNA_def_property(srna, "alliance", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, mode_items);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Particle_target_reset");

}
static void rna_def_particle_system(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ParticleSystem", NULL);
	RNA_def_struct_ui_text(srna, "Particle System", "Particle system in an object");
	RNA_def_struct_ui_icon(srna, ICON_PARTICLE_DATA);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Particle system name");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER|NA_RENAME, NULL);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ParticleSystem_name_set");
	RNA_def_struct_name_property(srna, prop);

	/* access to particle settings is redirected through functions */
	/* to allow proper id-buttons functionality */
	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	/*RNA_def_property_pointer_sdna(prop, NULL, "part"); */
	RNA_def_property_struct_type(prop, "ParticleSettings");
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_NULL);
	RNA_def_property_pointer_funcs(prop, "rna_particle_settings_get", "rna_particle_settings_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Settings", "Particle system settings");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "particles", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "particles", "totpart");
	RNA_def_property_struct_type(prop, "Particle");
	RNA_def_property_ui_text(prop, "Particles", "Particles generated by the particle system");

	prop = RNA_def_property(srna, "child_particles", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "child", "totchild");
	RNA_def_property_struct_type(prop, "ChildParticle");
	RNA_def_property_ui_text(prop, "Child Particles", "Child particles generated by the particle system");

	prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Seed", "Offset in the random number table, to get a different randomized result");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "child_seed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Child Seed",
	                         "Offset in the random number table for child particles, to get a different "
	                         "randomized result");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	/* hair */
	prop = RNA_def_property(srna, "is_global_hair", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PSYS_GLOBAL_HAIR);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Global Hair", "Hair keys are in global coordinate space");

	prop = RNA_def_property(srna, "use_hair_dynamics", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PSYS_HAIR_DYNAMICS);
	RNA_def_property_ui_text(prop, "Hair Dynamics", "Enable hair dynamics using cloth simulation");
	RNA_def_property_update(prop, 0, "rna_Particle_hair_dynamics");

	prop = RNA_def_property(srna, "cloth", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "clmd");
	RNA_def_property_struct_type(prop, "ClothModifier");
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Cloth", "Cloth dynamics for hair");

	/* reactor */
	prop = RNA_def_property(srna, "reactor_target_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target_ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Reactor Target Object",
	                         "For reactor systems, the object that has the target particle system "
	                         "(empty if same object)");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "reactor_target_particle_system", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "target_psys");
	RNA_def_property_range(prop, 1, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Reactor Target Particle System",
	                         "For reactor systems, index of particle system on the target object");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* keyed */
	prop = RNA_def_property(srna, "use_keyed_timing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PSYS_KEYED_TIMING);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Keyed timing", "Use key times");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "targets", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "ParticleTarget");
	RNA_def_property_ui_text(prop, "Targets", "Target particle systems");

	prop = RNA_def_property(srna, "active_particle_target", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ParticleTarget");
	RNA_def_property_pointer_funcs(prop, "rna_ParticleSystem_active_particle_target_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Particle Target", "");

	prop = RNA_def_property(srna, "active_particle_target_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_ParticleSystem_active_particle_target_index_get",
	                           "rna_ParticleSystem_active_particle_target_index_set",
	                           "rna_ParticleSystem_active_particle_target_index_range");
	RNA_def_property_ui_text(prop, "Active Particle Target Index", "");


	/* billboard */
	prop = RNA_def_property(srna, "billboard_normal_uv", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "bb_uvname[0]");
	RNA_def_property_string_maxlength(prop, 32);
	RNA_def_property_ui_text(prop, "Billboard Normal UV", "UV map to control billboard normals");

	prop = RNA_def_property(srna, "billboard_time_index_uv", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "bb_uvname[1]");
	RNA_def_property_string_maxlength(prop, 32);
	RNA_def_property_ui_text(prop, "Billboard Time Index UV", "UV map to control billboard time index (X-Y)");

	prop = RNA_def_property(srna, "billboard_split_uv", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "bb_uvname[2]");
	RNA_def_property_string_maxlength(prop, 32);
	RNA_def_property_ui_text(prop, "Billboard Split UV", "UV map to control billboard splitting");

	/* vertex groups */

	/* note, internally store as ints, access as strings */
#if 0 /* int access. works ok but isn't useful for the UI */
	prop = RNA_def_property(srna, "vertex_group_density", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[0]");
	RNA_def_property_ui_text(prop, "Vertex Group Density", "Vertex group to control density");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");
#endif

	prop = RNA_def_property(srna, "vertex_group_density", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_0", "rna_ParticleVGroup_name_len_0",
	                              "rna_ParticleVGroup_name_set_0");
	RNA_def_property_ui_text(prop, "Vertex Group Density", "Vertex group to control density");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "invert_vertex_group_density", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_DENSITY));
	RNA_def_property_ui_text(prop, "Vertex Group Density Negate", "Negate the effect of the density vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "vertex_group_velocity", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_1", "rna_ParticleVGroup_name_len_1",
	                              "rna_ParticleVGroup_name_set_1");
	RNA_def_property_ui_text(prop, "Vertex Group Velocity", "Vertex group to control velocity");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "invert_vertex_group_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_VEL));
	RNA_def_property_ui_text(prop, "Vertex Group Velocity Negate", "Negate the effect of the velocity vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "vertex_group_length", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_2", "rna_ParticleVGroup_name_len_2",
	                              "rna_ParticleVGroup_name_set_2");
	RNA_def_property_ui_text(prop, "Vertex Group Length", "Vertex group to control length");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "invert_vertex_group_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_LENGTH));
	RNA_def_property_ui_text(prop, "Vertex Group Length Negate", "Negate the effect of the length vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	prop = RNA_def_property(srna, "vertex_group_clump", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_3", "rna_ParticleVGroup_name_len_3",
	                              "rna_ParticleVGroup_name_set_3");
	RNA_def_property_ui_text(prop, "Vertex Group Clump", "Vertex group to control clump");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "invert_vertex_group_clump", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_CLUMP));
	RNA_def_property_ui_text(prop, "Vertex Group Clump Negate", "Negate the effect of the clump vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "vertex_group_kink", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_4", "rna_ParticleVGroup_name_len_4",
	                              "rna_ParticleVGroup_name_set_4");
	RNA_def_property_ui_text(prop, "Vertex Group Kink", "Vertex group to control kink");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "invert_vertex_group_kink", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_KINK));
	RNA_def_property_ui_text(prop, "Vertex Group Kink Negate", "Negate the effect of the kink vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "vertex_group_roughness_1", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_5", "rna_ParticleVGroup_name_len_5",
	                              "rna_ParticleVGroup_name_set_5");
	RNA_def_property_ui_text(prop, "Vertex Group Roughness 1", "Vertex group to control roughness 1");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "invert_vertex_group_roughness_1", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_ROUGH1));
	RNA_def_property_ui_text(prop, "Vertex Group Roughness 1 Negate",
	                         "Negate the effect of the roughness 1 vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "vertex_group_roughness_2", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_6", "rna_ParticleVGroup_name_len_6",
	                              "rna_ParticleVGroup_name_set_6");
	RNA_def_property_ui_text(prop, "Vertex Group Roughness 2", "Vertex group to control roughness 2");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "invert_vertex_group_roughness_2", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_ROUGH2));
	RNA_def_property_ui_text(prop, "Vertex Group Roughness 2 Negate",
	                         "Negate the effect of the roughness 2 vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "vertex_group_roughness_end", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_7", "rna_ParticleVGroup_name_len_7",
	                              "rna_ParticleVGroup_name_set_7");
	RNA_def_property_ui_text(prop, "Vertex Group Roughness End", "Vertex group to control roughness end");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "invert_vertex_group_roughness_end", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_ROUGHE));
	RNA_def_property_ui_text(prop, "Vertex Group Roughness End Negate",
	                         "Negate the effect of the roughness end vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

	prop = RNA_def_property(srna, "vertex_group_size", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_8", "rna_ParticleVGroup_name_len_8",
	                              "rna_ParticleVGroup_name_set_8");
	RNA_def_property_ui_text(prop, "Vertex Group Size", "Vertex group to control size");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "invert_vertex_group_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_SIZE));
	RNA_def_property_ui_text(prop, "Vertex Group Size Negate", "Negate the effect of the size vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "vertex_group_tangent", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_9", "rna_ParticleVGroup_name_len_9",
	                              "rna_ParticleVGroup_name_set_9");
	RNA_def_property_ui_text(prop, "Vertex Group Tangent", "Vertex group to control tangent");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "invert_vertex_group_tangent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_TAN));
	RNA_def_property_ui_text(prop, "Vertex Group Tangent Negate", "Negate the effect of the tangent vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "vertex_group_rotation", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_10", "rna_ParticleVGroup_name_len_10",
	                              "rna_ParticleVGroup_name_set_10");
	RNA_def_property_ui_text(prop, "Vertex Group Rotation", "Vertex group to control rotation");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "invert_vertex_group_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_ROT));
	RNA_def_property_ui_text(prop, "Vertex Group Rotation Negate", "Negate the effect of the rotation vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "vertex_group_field", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleVGroup_name_get_11", "rna_ParticleVGroup_name_len_11",
	                              "rna_ParticleVGroup_name_set_11");
	RNA_def_property_ui_text(prop, "Vertex Group Field", "Vertex group to control field");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	prop = RNA_def_property(srna, "invert_vertex_group_field", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_EFFECTOR));
	RNA_def_property_ui_text(prop, "Vertex Group Field Negate", "Negate the effect of the field vertex group");
	RNA_def_property_update(prop, 0, "rna_Particle_reset");

	/* pointcache */
	prop = RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "pointcache");
	RNA_def_property_struct_type(prop, "PointCache");
	RNA_def_property_ui_text(prop, "Point Cache", "");

	prop = RNA_def_property(srna, "has_multiple_caches", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ParticleSystem_multiple_caches_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Multiple Caches", "Particle system has multiple point caches");

	/* offset ob */
	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "parent");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent",
	                         "Use this object's coordinate system instead of global coordinate system");
	RNA_def_property_update(prop, 0, "rna_Particle_redo");

	/* hair or cache editing */
	prop = RNA_def_property(srna, "is_editable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ParticleSystem_editable_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Editable", "Particle system can be edited in particle mode");

	prop = RNA_def_property(srna, "is_edited", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ParticleSystem_edited_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Edited", "Particle system has been edited in particle mode");

	/* Read-only: this is calculated internally. Changing it would only affect
	 * the next time-step. The user should change ParticlSettings.subframes or
	 * ParticleSettings.courant_target instead. */
	prop = RNA_def_property(srna, "dt_frac", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 1.0f/101.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Timestep", "The current simulation time step size, as a fraction of a frame");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	RNA_def_struct_path_func(srna, "rna_ParticleSystem_path");
}

void RNA_def_particle(BlenderRNA *brna)
{
	rna_def_particle_target(brna);
	rna_def_fluid_settings(brna);
	rna_def_particle_hair_key(brna);
	rna_def_particle_key(brna);
	
	rna_def_child_particle(brna);
	rna_def_particle(brna);
	rna_def_particle_dupliweight(brna);
	rna_def_particle_system(brna);
	rna_def_particle_settings_mtex(brna);
	rna_def_particle_settings(brna);
}

#endif
