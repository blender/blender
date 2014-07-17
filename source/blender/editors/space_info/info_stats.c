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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_info/info_stats.c
 *  \ingroup spinfo
 */


#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_anim.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_key.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_editmesh.h"

#include "ED_info.h"
#include "ED_armature.h"

#define MAX_INFO_LEN 512
#define MAX_INFO_NUM_LEN 16

typedef struct SceneStats {
	int totvert, totvertsel;
	int totedge, totedgesel;
	int totface, totfacesel;
	int totbone, totbonesel;
	int totobj,  totobjsel;
	int totlamp, totlampsel; 
	int tottri;

	char infostr[MAX_INFO_LEN];
} SceneStats;

typedef struct SceneStatsFmt {
	/* Totals */
	char totvert[MAX_INFO_NUM_LEN], totvertsel[MAX_INFO_NUM_LEN];
	char totface[MAX_INFO_NUM_LEN], totfacesel[MAX_INFO_NUM_LEN];
	char totedge[MAX_INFO_NUM_LEN], totedgesel[MAX_INFO_NUM_LEN];
	char totbone[MAX_INFO_NUM_LEN], totbonesel[MAX_INFO_NUM_LEN];
	char totobj[MAX_INFO_NUM_LEN], totobjsel[MAX_INFO_NUM_LEN];
	char totlamp[MAX_INFO_NUM_LEN], totlampsel[MAX_INFO_NUM_LEN];
	char tottri[MAX_INFO_NUM_LEN];
} SceneStatsFmt;

static void stats_object(Object *ob, int sel, int totob, SceneStats *stats)
{
	switch (ob->type) {
		case OB_MESH:
		{
			/* we assume derivedmesh is already built, this strictly does stats now. */
			DerivedMesh *dm = ob->derivedFinal;
			int totvert, totedge, totface, totloop;

			if (dm) {
				totvert = dm->getNumVerts(dm);
				totedge = dm->getNumEdges(dm);
				totface = dm->getNumPolys(dm);
				totloop = dm->getNumLoops(dm);

				stats->totvert += totvert * totob;
				stats->totedge += totedge * totob;
				stats->totface += totface * totob;
				stats->tottri  += poly_to_tri_count(totface, totloop) * totob;

				if (sel) {
					stats->totvertsel += totvert;
					stats->totfacesel += totface;
				}
			}
			break;
		}
		case OB_LAMP:
			stats->totlamp += totob;
			if (sel) {
				stats->totlampsel += totob;
			}
			break;
		case OB_SURF:
		case OB_CURVE:
		case OB_FONT:
		case OB_MBALL:
		{
			int totv = 0, totf = 0, tottri = 0;

			if (ob->curve_cache && ob->curve_cache->disp.first)
				BKE_displist_count(&ob->curve_cache->disp, &totv, &totf, &tottri);

			totv   *= totob;
			totf   *= totob;
			tottri *= totob;

			stats->totvert += totv;
			stats->totface += totf;
			stats->tottri  += tottri;

			if (sel) {
				stats->totvertsel += totv;
				stats->totfacesel += totf;
			}
			break;
		}
	}
}

static void stats_object_edit(Object *obedit, SceneStats *stats)
{
	if (obedit->type == OB_MESH) {
		BMEditMesh *em = BKE_editmesh_from_object(obedit);

		stats->totvert = em->bm->totvert;
		stats->totvertsel = em->bm->totvertsel;
		
		stats->totedge = em->bm->totedge;
		stats->totedgesel = em->bm->totedgesel;
		
		stats->totface = em->bm->totface;
		stats->totfacesel = em->bm->totfacesel;

		stats->tottri = em->tottri;
	}
	else if (obedit->type == OB_ARMATURE) {
		/* Armature Edit */
		bArmature *arm = obedit->data;
		EditBone *ebo;

		for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
			stats->totbone++;
			
			if ((ebo->flag & BONE_CONNECTED) && ebo->parent)
				stats->totvert--;
			
			if (ebo->flag & BONE_TIPSEL)
				stats->totvertsel++;
			if (ebo->flag & BONE_ROOTSEL)
				stats->totvertsel++;
			
			if (ebo->flag & BONE_SELECTED) stats->totbonesel++;

			/* if this is a connected child and it's parent is being moved, remove our root */
			if ((ebo->flag & BONE_CONNECTED) && (ebo->flag & BONE_ROOTSEL) &&
			    ebo->parent && (ebo->parent->flag & BONE_TIPSEL))
			{
				stats->totvertsel--;
			}

			stats->totvert += 2;
		}
	}
	else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) { /* OB_FONT has no cu->editnurb */
		/* Curve Edit */
		Curve *cu = obedit->data;
		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;
		int a;
		ListBase *nurbs = BKE_curve_editNurbs_get(cu);

		for (nu = nurbs->first; nu; nu = nu->next) {
			if (nu->type == CU_BEZIER) {
				bezt = nu->bezt;
				a = nu->pntsu;
				while (a--) {
					stats->totvert += 3;
					if (bezt->f1 & SELECT) stats->totvertsel++;
					if (bezt->f2 & SELECT) stats->totvertsel++;
					if (bezt->f3 & SELECT) stats->totvertsel++;
					bezt++;
				}
			}
			else {
				bp = nu->bp;
				a = nu->pntsu * nu->pntsv;
				while (a--) {
					stats->totvert++;
					if (bp->f1 & SELECT) stats->totvertsel++;
					bp++;
				}
			}
		}
	}
	else if (obedit->type == OB_MBALL) {
		/* MetaBall Edit */
		MetaBall *mball = obedit->data;
		MetaElem *ml;
		
		for (ml = mball->editelems->first; ml; ml = ml->next) {
			stats->totvert++;
			if (ml->flag & SELECT) stats->totvertsel++;
		}
	}
	else if (obedit->type == OB_LATTICE) {
		/* Lattice Edit */
		Lattice *lt = obedit->data;
		Lattice *editlatt = lt->editlatt->latt;
		BPoint *bp;
		int a;

		bp = editlatt->def;
		
		a = editlatt->pntsu * editlatt->pntsv * editlatt->pntsw;
		while (a--) {
			stats->totvert++;
			if (bp->f1 & SELECT) stats->totvertsel++;
			bp++;
		}
	}
}

static void stats_object_pose(Object *ob, SceneStats *stats)
{
	if (ob->pose) {
		bArmature *arm = ob->data;
		bPoseChannel *pchan;

		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			stats->totbone++;
			if (pchan->bone && (pchan->bone->flag & BONE_SELECTED))
				if (pchan->bone->layer & arm->layer)
					stats->totbonesel++;
		}
	}
}

static void stats_object_sculpt_dynamic_topology(Object *ob, SceneStats *stats)
{
	stats->totvert = ob->sculpt->bm->totvert;
	stats->tottri = ob->sculpt->bm->totface;
}

static void stats_dupli_object(Base *base, Object *ob, SceneStats *stats)
{
	if (base->flag & SELECT) stats->totobjsel++;

	if (ob->transflag & OB_DUPLIPARTS) {
		/* Dupli Particles */
		ParticleSystem *psys;
		ParticleSettings *part;

		for (psys = ob->particlesystem.first; psys; psys = psys->next) {
			part = psys->part;

			if (part->draw_as == PART_DRAW_OB && part->dup_ob) {
				int tot = count_particles(psys);
				stats_object(part->dup_ob, 0, tot, stats);
			}
			else if (part->draw_as == PART_DRAW_GR && part->dup_group) {
				GroupObject *go;
				int tot, totgroup = 0, cur = 0;
				
				for (go = part->dup_group->gobject.first; go; go = go->next)
					totgroup++;

				for (go = part->dup_group->gobject.first; go; go = go->next) {
					tot = count_particles_mod(psys, totgroup, cur);
					stats_object(go->ob, 0, tot, stats);
					cur++;
				}
			}
		}
		
		stats_object(ob, base->flag & SELECT, 1, stats);
		stats->totobj++;
	}
	else if (ob->parent && (ob->parent->transflag & (OB_DUPLIVERTS | OB_DUPLIFACES))) {
		/* Dupli Verts/Faces */
		int tot;

		/* metaball dupli-instances are tessellated once */
		if (ob->type == OB_MBALL) {
			tot = 1;
		}
		else {
			tot = count_duplilist(ob->parent);
		}

		stats->totobj += tot;
		stats_object(ob, base->flag & SELECT, tot, stats);
	}
	else if (ob->transflag & OB_DUPLIFRAMES) {
		/* Dupli Frames */
		int tot = count_duplilist(ob);
		stats->totobj += tot;
		stats_object(ob, base->flag & SELECT, tot, stats);
	}
	else if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
		/* Dupli Group */
		int tot = count_duplilist(ob);
		stats->totobj += tot;
		stats_object(ob, base->flag & SELECT, tot, stats);
	}
	else {
		/* No Dupli */
		stats_object(ob, base->flag & SELECT, 1, stats);
		stats->totobj++;
	}
}

static bool stats_is_object_dynamic_topology_sculpt(Object *ob)
{
	return (ob && (ob->mode & OB_MODE_SCULPT) &&
	        ob->sculpt && ob->sculpt->bm);
}

/* Statistics displayed in info header. Called regularly on scene changes. */
static void stats_update(Scene *scene)
{
	SceneStats stats = {0};
	Object *ob = (scene->basact) ? scene->basact->object : NULL;
	Base *base;
	
	if (scene->obedit) {
		/* Edit Mode */
		stats_object_edit(scene->obedit, &stats);
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		/* Pose Mode */
		stats_object_pose(ob, &stats);
	}
	else if (stats_is_object_dynamic_topology_sculpt(ob)) {
		/* Dynamic-topology sculpt mode */
		stats_object_sculpt_dynamic_topology(ob, &stats);
	}
	else {
		/* Objects */
		for (base = scene->base.first; base; base = base->next)
			if (scene->lay & base->lay)
				stats_dupli_object(base, base->object, &stats);
	}

	if (!scene->stats)
		scene->stats = MEM_callocN(sizeof(SceneStats), "SceneStats");

	*(scene->stats) = stats;
}

static void stats_string(Scene *scene)
{
#define MAX_INFO_MEM_LEN  64
	SceneStats *stats = scene->stats;
	SceneStatsFmt stats_fmt;
	Object *ob = (scene->basact) ? scene->basact->object : NULL;
	uintptr_t mem_in_use, mmap_in_use;
	char memstr[MAX_INFO_MEM_LEN];
	char *s;
	size_t ofs = 0;

	mem_in_use = MEM_get_memory_in_use();
	mmap_in_use = MEM_get_mapped_memory_in_use();


	/* Generate formatted numbers */
#define SCENE_STATS_FMT_INT(_id) \
	BLI_str_format_int_grouped(stats_fmt._id, stats->_id)

	SCENE_STATS_FMT_INT(totvert);
	SCENE_STATS_FMT_INT(totvertsel);

	SCENE_STATS_FMT_INT(totedge);
	SCENE_STATS_FMT_INT(totedgesel);

	SCENE_STATS_FMT_INT(totface);
	SCENE_STATS_FMT_INT(totfacesel);

	SCENE_STATS_FMT_INT(totbone);
	SCENE_STATS_FMT_INT(totbonesel);

	SCENE_STATS_FMT_INT(totobj);
	SCENE_STATS_FMT_INT(totobjsel);

	SCENE_STATS_FMT_INT(totlamp);
	SCENE_STATS_FMT_INT(totlampsel);

	SCENE_STATS_FMT_INT(tottri);

#undef SCENE_STATS_FMT_INT


	/* get memory statistics */
	s = memstr;
	ofs += BLI_snprintf(s + ofs, MAX_INFO_MEM_LEN - ofs, IFACE_(" | Mem:%.2fM"),
	                    (double)((mem_in_use - mmap_in_use) >> 10) / 1024.0);
	if (mmap_in_use)
		BLI_snprintf(s + ofs, MAX_INFO_MEM_LEN - ofs, IFACE_(" (%.2fM)"), (double)((mmap_in_use) >> 10) / 1024.0);

	s = stats->infostr;
	ofs = 0;

	ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs, "%s | ", versionstr);

	if (scene->obedit) {
		if (BKE_keyblock_from_object(scene->obedit))
			ofs += BLI_strncpy_rlen(s + ofs, IFACE_("(Key) "), MAX_INFO_LEN - ofs);

		if (scene->obedit->type == OB_MESH) {
			ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs,
			                    IFACE_("Verts:%s/%s | Edges:%s/%s | Faces:%s/%s | Tris:%s"),
			                    stats_fmt.totvertsel, stats_fmt.totvert, stats_fmt.totedgesel, stats_fmt.totedge,
			                    stats_fmt.totfacesel, stats_fmt.totface, stats_fmt.tottri);
		}
		else if (scene->obedit->type == OB_ARMATURE) {
			ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs, IFACE_("Verts:%s/%s | Bones:%s/%s"), stats_fmt.totvertsel,
			                    stats_fmt.totvert, stats_fmt.totbonesel, stats_fmt.totbone);
		}
		else {
			ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs, IFACE_("Verts:%s/%s"), stats_fmt.totvertsel,
			                    stats_fmt.totvert);
		}

		ofs += BLI_strncpy_rlen(s + ofs, memstr, MAX_INFO_LEN - ofs);
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs, IFACE_("Bones:%s/%s %s"),
		                    stats_fmt.totbonesel, stats_fmt.totbone, memstr);
	}
	else if (stats_is_object_dynamic_topology_sculpt(ob)) {
		ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs, IFACE_("Verts:%s | Tris:%s"), stats_fmt.totvert,
		                    stats_fmt.tottri);
	}
	else {
		ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs,
		                    IFACE_("Verts:%s | Faces:%s | Tris:%s | Objects:%s/%s | Lamps:%s/%s%s"),
		                    stats_fmt.totvert, stats_fmt.totface,
		                    stats_fmt.tottri, stats_fmt.totobjsel,
		                    stats_fmt.totobj, stats_fmt.totlampsel,
		                    stats_fmt.totlamp, memstr);
	}

	if (ob)
		BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs, " | %s", ob->id.name + 2);
#undef MAX_INFO_MEM_LEN
}

#undef MAX_INFO_LEN

void ED_info_stats_clear(Scene *scene)
{
	if (scene->stats) {
		MEM_freeN(scene->stats);
		scene->stats = NULL;
	}
}

const char *ED_info_stats_string(Scene *scene)
{
	if (!scene->stats)
		stats_update(scene);
	stats_string(scene);

	return scene->stats->infostr;
}
