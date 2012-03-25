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
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_anim.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_particle.h"
#include "BKE_tessmesh.h"

#include "ED_info.h"
#include "ED_armature.h"
#include "ED_mesh.h"


typedef struct SceneStats {
	int totvert, totvertsel;
	int totedge, totedgesel;
	int totface, totfacesel;
	int totbone, totbonesel;
	int totobj, totobjsel;
	int totmesh, totlamp, totcurve;

	char infostr[512];
} SceneStats;

static void stats_object(Object *ob, int sel, int totob, SceneStats *stats)
{
	switch(ob->type) {
	case OB_MESH: {
		/* we assume derivedmesh is already built, this strictly does stats now. */
		DerivedMesh *dm= ob->derivedFinal;
		int totvert, totedge, totface;

		stats->totmesh +=totob;

		if (dm) {
			totvert = dm->getNumVerts(dm);
			totedge = dm->getNumEdges(dm);
			totface = dm->getNumPolys(dm);

			stats->totvert += totvert*totob;
			stats->totedge += totedge*totob;
			stats->totface += totface*totob;

			if (sel) {
				stats->totvertsel += totvert;
				stats->totfacesel += totface;
			}
		}
		break;
	}
	case OB_LAMP:
		stats->totlamp += totob;
		break;
	case OB_SURF:
	case OB_CURVE:
	case OB_FONT: {
		int tot= 0, totf= 0;

		stats->totcurve += totob;

		if (ob->disp.first)
			count_displist(&ob->disp, &tot, &totf);

		tot *= totob;
		totf *= totob;

		stats->totvert+= tot;
		stats->totface+= totf;

		if (sel) {
			stats->totvertsel += tot;
			stats->totfacesel += totf;
		}
		break;
	}
	case OB_MBALL: {
		int tot= 0, totf= 0;

		count_displist(&ob->disp, &tot, &totf);

		tot *= totob;
		totf *= totob;

		stats->totvert += tot;
		stats->totface += totf;

		if (sel) {
			stats->totvertsel += tot;
			stats->totfacesel += totf;
		}
		break;
	}
	}
}

static void stats_object_edit(Object *obedit, SceneStats *stats)
{
	if (obedit->type==OB_MESH) {
		BMEditMesh *em = BMEdit_FromObject(obedit);

		stats->totvert = em->bm->totvert;
		stats->totvertsel = em->bm->totvertsel;
		
		stats->totedge = em->bm->totedge;
		stats->totedgesel = em->bm->totedgesel;
		
		stats->totface = em->bm->totface;
		stats->totfacesel = em->bm->totfacesel;
	}
	else if (obedit->type==OB_ARMATURE) {
		/* Armature Edit */
		bArmature *arm= obedit->data;
		EditBone *ebo;

		for (ebo=arm->edbo->first; ebo; ebo=ebo->next) {
			stats->totbone++;
			
			if ((ebo->flag & BONE_CONNECTED) && ebo->parent)
				stats->totvert--;
			
			if (ebo->flag & BONE_TIPSEL)
				stats->totvertsel++;
			if (ebo->flag & BONE_ROOTSEL)
				stats->totvertsel++;
			
			if (ebo->flag & BONE_SELECTED) stats->totbonesel++;

			/* if this is a connected child and it's parent is being moved, remove our root */
			if ((ebo->flag & BONE_CONNECTED)&& (ebo->flag & BONE_ROOTSEL) && ebo->parent && (ebo->parent->flag & BONE_TIPSEL))
				stats->totvertsel--;

			stats->totvert+=2;
		}
	}
	else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) { /* OB_FONT has no cu->editnurb */
		/* Curve Edit */
		Curve *cu= obedit->data;
		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;
		int a;
		ListBase *nurbs= curve_editnurbs(cu);

		for (nu=nurbs->first; nu; nu=nu->next) {
			if (nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while (a--) {
					stats->totvert+=3;
					if (bezt->f1) stats->totvertsel++;
					if (bezt->f2) stats->totvertsel++;
					if (bezt->f3) stats->totvertsel++;
					bezt++;
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while (a--) {
					stats->totvert++;
					if (bp->f1 & SELECT) stats->totvertsel++;
					bp++;
				}
			}
		}
	}
	else if (obedit->type==OB_MBALL) {
		/* MetaBall Edit */
		MetaBall *mball= obedit->data;
		MetaElem *ml;
		
		for (ml= mball->editelems->first; ml; ml=ml->next) {
			stats->totvert++;
			if (ml->flag & SELECT) stats->totvertsel++;
		}
	}
	else if (obedit->type==OB_LATTICE) {
		/* Lattice Edit */
		Lattice *lt= obedit->data;
		Lattice *editlatt= lt->editlatt->latt;
		BPoint *bp;
		int a;

		bp= editlatt->def;
		
		a= editlatt->pntsu*editlatt->pntsv*editlatt->pntsw;
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
		bArmature *arm= ob->data;
		bPoseChannel *pchan;

		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			stats->totbone++;
			if (pchan->bone && (pchan->bone->flag & BONE_SELECTED))
				if (pchan->bone->layer & arm->layer)
					stats->totbonesel++;
		}
	}
}

static void stats_dupli_object(Base *base, Object *ob, SceneStats *stats)
{
	if (base->flag & SELECT) stats->totobjsel++;

	if (ob->transflag & OB_DUPLIPARTS) {
		/* Dupli Particles */
		ParticleSystem *psys;
		ParticleSettings *part;

		for (psys=ob->particlesystem.first; psys; psys=psys->next) {
			part=psys->part;

			if (part->draw_as==PART_DRAW_OB && part->dup_ob) {
				int tot=count_particles(psys);
				stats_object(part->dup_ob, 0, tot, stats);
			}
			else if (part->draw_as==PART_DRAW_GR && part->dup_group) {
				GroupObject *go;
				int tot, totgroup=0, cur=0;
				
				for (go= part->dup_group->gobject.first; go; go=go->next)
					totgroup++;

				for (go= part->dup_group->gobject.first; go; go=go->next) {
					tot=count_particles_mod(psys,totgroup,cur);
					stats_object(go->ob, 0, tot, stats);
					cur++;
				}
			}
		}
		
		stats_object(ob, base->flag & SELECT, 1, stats);
		stats->totobj++;
	}
	else if (ob->parent && (ob->parent->transflag & (OB_DUPLIVERTS|OB_DUPLIFACES))) {
		/* Dupli Verts/Faces */
		int tot= count_duplilist(ob->parent);
		stats->totobj+=tot;
		stats_object(ob, base->flag & SELECT, tot, stats);
	}
	else if (ob->transflag & OB_DUPLIFRAMES) {
		/* Dupli Frames */
		int tot= count_duplilist(ob);
		stats->totobj+=tot;
		stats_object(ob, base->flag & SELECT, tot, stats);
	}
	else if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
		/* Dupli Group */
		int tot= count_duplilist(ob);
		stats->totobj+=tot;
		stats_object(ob, base->flag & SELECT, tot, stats);
	}
	else {
		/* No Dupli */
		stats_object(ob, base->flag & SELECT, 1, stats);
		stats->totobj++;
	}
}

/* Statistics displayed in info header. Called regularly on scene changes. */
static void stats_update(Scene *scene)
{
	SceneStats stats= {0};
	Object *ob= (scene->basact)? scene->basact->object: NULL;
	Base *base;
	
	if (scene->obedit) {
		/* Edit Mode */
		stats_object_edit(scene->obedit, &stats);
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		/* Pose Mode */
		stats_object_pose(ob, &stats);
	}
	else {
		/* Objects */
		for (base= scene->base.first; base; base=base->next)
			if (scene->lay & base->lay)
				stats_dupli_object(base, base->object, &stats);
	}

	if (!scene->stats)
		scene->stats= MEM_callocN(sizeof(SceneStats), "SceneStats");

	*(scene->stats)= stats;
}

static void stats_string(Scene *scene)
{
	SceneStats *stats= scene->stats;
	Object *ob= (scene->basact)? scene->basact->object: NULL;
	uintptr_t mem_in_use, mmap_in_use;
	char memstr[64];
	char *s;

	mem_in_use= MEM_get_memory_in_use();
	mmap_in_use= MEM_get_mapped_memory_in_use();

	/* get memory statistics */
	s= memstr + sprintf(memstr, " | Mem:%.2fM", (double)((mem_in_use-mmap_in_use)>>10)/1024.0);
	if (mmap_in_use)
		sprintf(s, " (%.2fM)", (double)((mmap_in_use)>>10)/1024.0);

	s= stats->infostr;
	
	s+= sprintf(s, "%s | ", versionstr);

	if (scene->obedit) {
		if (ob_get_keyblock(scene->obedit))
			s+= sprintf(s, "(Key) ");

		if (scene->obedit->type==OB_MESH) {
			if (scene->toolsettings->selectmode & SCE_SELECT_VERTEX)
				s+= sprintf(s, "Ve:%d-%d | Ed:%d-%d | Fa:%d-%d",
						stats->totvertsel, stats->totvert, stats->totedgesel, stats->totedge, stats->totfacesel, stats->totface);
			else if (scene->toolsettings->selectmode & SCE_SELECT_EDGE)
				s+= sprintf(s, "Ed:%d-%d | Fa:%d-%d",
						stats->totedgesel, stats->totedge, stats->totfacesel, stats->totface);
			else
				s+= sprintf(s, "Fa:%d-%d", stats->totfacesel, stats->totface);
		}
		else if (scene->obedit->type==OB_ARMATURE) {
			s+= sprintf(s, "Ve:%d-%d | Bo:%d-%d", stats->totvertsel, stats->totvert, stats->totbonesel, stats->totbone);
		}
		else {
			s+= sprintf(s, "Ve:%d-%d", stats->totvertsel, stats->totvert);
		}

		strcat(s, memstr);
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		s += sprintf(s, "Bo:%d-%d %s",
					stats->totbonesel, stats->totbone, memstr);
	}
	else {
		s += sprintf(s, "Ve:%d | Fa:%d | Ob:%d-%d | La:%d%s",
			stats->totvert, stats->totface, stats->totobjsel, stats->totobj, stats->totlamp, memstr);
	}

	if (ob)
		sprintf(s, " | %s", ob->id.name+2);
}

void ED_info_stats_clear(Scene *scene)
{
	if (scene->stats) {
		MEM_freeN(scene->stats);
		scene->stats= NULL;
	}
}

const char *ED_info_stats_string(Scene *scene)
{
	if (!scene->stats)
		stats_update(scene);
	stats_string(scene);

	return scene->stats->infostr;
}

