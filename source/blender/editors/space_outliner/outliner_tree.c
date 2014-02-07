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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_outliner/outliner_tree.c
 *  \ingroup spoutliner
 */
 
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_sequence_types.h"
#include "DNA_speaker_types.h"
#include "DNA_object_types.h"
#include "DNA_linestyle_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_mempool.h"
#include "BLI_fnmatch.h"

#include "BLF_translation.h"

#include "BKE_fcurve.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_sequencer.h"
#include "BKE_idcode.h"
#include "BKE_treehash.h"

#include "ED_armature.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "outliner_intern.h"

/* ********************************************************* */
/* Persistent Data */

static void outliner_storage_cleanup(SpaceOops *soops)
{
	BLI_mempool *ts = soops->treestore;
	
	if (ts) {
		TreeStoreElem *tselem;
		int unused = 0;
		
		/* each element used once, for ID blocks with more users to have each a treestore */
		BLI_mempool_iter iter;

		BLI_mempool_iternew(ts, &iter);
		while ((tselem = BLI_mempool_iterstep(&iter))) {
			tselem->used = 0;
		}
		
		/* cleanup only after reading file or undo step, and always for
		 * RNA datablocks view in order to save memory */
		if (soops->storeflag & SO_TREESTORE_CLEANUP) {
			BLI_mempool_iternew(ts, &iter);
			while ((tselem = BLI_mempool_iterstep(&iter))) {
				if (tselem->id == NULL) unused++;
			}
			
			if (unused) {
				if (BLI_mempool_count(ts) == unused) {
					BLI_mempool_destroy(ts);
					soops->treestore = NULL;
					if (soops->treehash) {
						BKE_treehash_free(soops->treehash);
						soops->treehash = NULL;
					}
				}
				else {
					TreeStoreElem *tsenew;
					BLI_mempool *new_ts = BLI_mempool_create(sizeof(TreeStoreElem), BLI_mempool_count(ts) - unused,
					                                         512, BLI_MEMPOOL_ALLOW_ITER);
					BLI_mempool_iternew(ts, &iter);
					while ((tselem = BLI_mempool_iterstep(&iter))) {
						if (tselem->id) {
							tsenew = BLI_mempool_alloc(new_ts);
							*tsenew = *tselem;
						}
					}
					BLI_mempool_destroy(ts);
					soops->treestore = new_ts;
					if (soops->treehash) {
						/* update hash table to fix broken pointers */
						BKE_treehash_rebuild_from_treestore(soops->treehash, soops->treestore);
					}
				}
			}
		}
	}
}

static void check_persistent(SpaceOops *soops, TreeElement *te, ID *id, short type, short nr)
{
	TreeStoreElem *tselem;
	
	if (soops->treestore == NULL) {
		/* if treestore was not created in readfile.c, create it here */
		soops->treestore = BLI_mempool_create(sizeof(TreeStoreElem), 1, 512, BLI_MEMPOOL_ALLOW_ITER);
		
	}
	if (soops->treehash == NULL) {
		soops->treehash = BKE_treehash_create_from_treestore(soops->treestore);
	}

	/* find any unused tree element in treestore and mark it as used
	 * (note that there may be multiple unused elements in case of linked objects) */
	tselem = BKE_treehash_lookup_unused(soops->treehash, type, nr, id);
	if (tselem) {
		te->store_elem = tselem;
		tselem->used = 1;
		return;
	}

	/* add 1 element to treestore */
	tselem = BLI_mempool_alloc(soops->treestore);
	tselem->type = type;
	tselem->nr = type ? nr : 0;
	tselem->id = id;
	tselem->used = 0;
	tselem->flag = TSE_CLOSED;
	te->store_elem = tselem;
	BKE_treehash_add_element(soops->treehash, tselem);
}

/* ********************************************************* */
/* Tree Management */

void outliner_free_tree(ListBase *lb)
{
	while (lb->first) {
		TreeElement *te = lb->first;
		
		outliner_free_tree(&te->subtree);
		BLI_remlink(lb, te);
		
		if (te->flag & TE_FREE_NAME) MEM_freeN((void *)te->name);
		MEM_freeN(te);
	}
}

void outliner_cleanup_tree(SpaceOops *soops)
{
	outliner_free_tree(&soops->tree);
	outliner_storage_cleanup(soops);
}

/* Find specific item from the treestore */
static TreeElement *outliner_find_tree_element(ListBase *lb, TreeStoreElem *store_elem)
{
	TreeElement *te, *tes;
	for (te = lb->first; te; te = te->next) {
		if (te->store_elem == store_elem) return te;
		tes = outliner_find_tree_element(&te->subtree, store_elem);
		if (tes) return tes;
	}
	return NULL;
}

/* tse is not in the treestore, we use its contents to find a match */
TreeElement *outliner_find_tse(SpaceOops *soops, TreeStoreElem *tse)
{
	TreeStoreElem *tselem;

	if (tse->id == NULL) return NULL;
	
	/* check if 'tse' is in treestore */
	tselem = BKE_treehash_lookup_any(soops->treehash, tse->type, tse->nr, tse->id);
	if (tselem) 
		return outliner_find_tree_element(&soops->tree, tselem);
	
	return NULL;
}

/* Find treestore that refers to given ID */
TreeElement *outliner_find_id(SpaceOops *soops, ListBase *lb, ID *id)
{
	TreeElement *te, *tes;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->type == 0) {
			if (tselem->id == id) return te;
			/* only deeper on scene or object */
			if (te->idcode == ID_OB || te->idcode == ID_SCE || (soops->outlinevis == SO_GROUPS && te->idcode == ID_GR)) {
				tes = outliner_find_id(soops, &te->subtree, id);
				if (tes) return tes;
			}
		}
	}
	return NULL;
}


ID *outliner_search_back(SpaceOops *UNUSED(soops), TreeElement *te, short idcode)
{
	TreeStoreElem *tselem;
	te = te->parent;
	
	while (te) {
		tselem = TREESTORE(te);
		if (tselem->type == 0 && te->idcode == idcode) return tselem->id;
		te = te->parent;
	}
	return NULL;
}


/* ********************************************************* */

/* Prototype, see functions below */
static TreeElement *outliner_add_element(SpaceOops *soops, ListBase *lb, void *idv, 
                                         TreeElement *parent, short type, short index);

/* -------------------------------------------------------- */

/* special handling of hierarchical non-lib data */
static void outliner_add_bone(SpaceOops *soops, ListBase *lb, ID *id, Bone *curBone, 
                              TreeElement *parent, int *a)
{
	TreeElement *te = outliner_add_element(soops, lb, id, parent, TSE_BONE, *a);
	
	(*a)++;
	te->name = curBone->name;
	te->directdata = curBone;
	
	for (curBone = curBone->childbase.first; curBone; curBone = curBone->next) {
		outliner_add_bone(soops, &te->subtree, id, curBone, te, a);
	}
}

/* -------------------------------------------------------- */

#define LOG2I(x) (int)(log(x) / M_LN2)

static void outliner_add_passes(SpaceOops *soops, TreeElement *tenla, ID *id, SceneRenderLayer *srl)
{
	TreeStoreElem *tselem = NULL;
	TreeElement *te = NULL;

	/* log stuff is to convert bitflags (powers of 2) to small integers,
	 * in order to not overflow short tselem->nr */
	
	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_COMBINED));
	te->name = IFACE_("Combined");
	te->directdata = &srl->passflag;
	
	/* save cpu cycles, but we add the first to invoke an open/close triangle */
	tselem = TREESTORE(tenla);
	if (tselem->flag & TSE_CLOSED)
		return;
	
	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_Z));
	te->name = IFACE_("Z");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_VECTOR));
	te->name = IFACE_("Vector");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_NORMAL));
	te->name = IFACE_("Normal");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_UV));
	te->name = IFACE_("UV");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_MIST));
	te->name = IFACE_("Mist");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_INDEXOB));
	te->name = IFACE_("Index Object");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_INDEXMA));
	te->name = IFACE_("Index Material");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_RGBA));
	te->name = IFACE_("Color");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_DIFFUSE));
	te->name = IFACE_("Diffuse");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_SPEC));
	te->name = IFACE_("Specular");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_SHADOW));
	te->name = IFACE_("Shadow");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_AO));
	te->name = IFACE_("AO");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_REFLECT));
	te->name = IFACE_("Reflection");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_REFRACT));
	te->name = IFACE_("Refraction");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_INDIRECT));
	te->name = IFACE_("Indirect");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_ENVIRONMENT));
	te->name = IFACE_("Environment");
	te->directdata = &srl->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_EMIT));
	te->name = IFACE_("Emit");
	te->directdata = &srl->passflag;
}

#undef LOG2I

static bool outliner_animdata_test(AnimData *adt)
{
	if (adt)
		return (adt->action || adt->drivers.first || adt->nla_tracks.first);
	return false;
}

static void outliner_add_line_styles(SpaceOops *soops, ListBase *lb, Scene *sce, TreeElement *te)
{
	SceneRenderLayer *srl;
	FreestyleLineSet *lineset;

	for (srl = sce->r.layers.first; srl; srl = srl->next) {
		for (lineset = srl->freestyleConfig.linesets.first; lineset; lineset = lineset->next) {
			FreestyleLineStyle *linestyle = lineset->linestyle;
			if (linestyle) {
				linestyle->id.flag |= LIB_DOIT;
			}
		}
	}
	for (srl = sce->r.layers.first; srl; srl = srl->next) {
		for (lineset = srl->freestyleConfig.linesets.first; lineset; lineset = lineset->next) {
			FreestyleLineStyle *linestyle = lineset->linestyle;
			if (linestyle) {
				if (!(linestyle->id.flag & LIB_DOIT))
					continue;
				linestyle->id.flag &= ~LIB_DOIT;
				outliner_add_element(soops, lb, linestyle, te, 0, 0);
			}
		}
	}
}

static void outliner_add_scene_contents(SpaceOops *soops, ListBase *lb, Scene *sce, TreeElement *te)
{
	SceneRenderLayer *srl;
	TreeElement *tenla = outliner_add_element(soops, lb, sce, te, TSE_R_LAYER_BASE, 0);
	int a;
	
	tenla->name = IFACE_("RenderLayers");
	for (a = 0, srl = sce->r.layers.first; srl; srl = srl->next, a++) {
		TreeElement *tenlay = outliner_add_element(soops, &tenla->subtree, sce, te, TSE_R_LAYER, a);
		tenlay->name = srl->name;
		tenlay->directdata = &srl->layflag;
		
		if (srl->light_override)
			outliner_add_element(soops, &tenlay->subtree, srl->light_override, tenlay, TSE_LINKED_LAMP, 0);
		if (srl->mat_override)
			outliner_add_element(soops, &tenlay->subtree, srl->mat_override, tenlay, TSE_LINKED_MAT, 0);
		
		outliner_add_passes(soops, tenlay, &sce->id, srl);
	}
	
	// TODO: move this to the front?
	if (outliner_animdata_test(sce->adt))
		outliner_add_element(soops, lb, sce, te, TSE_ANIM_DATA, 0);
	
	outliner_add_element(soops,  lb, sce->world, te, 0, 0);

	if (STREQ(sce->r.engine, "BLENDER_RENDER") && (sce->r.mode & R_EDGE_FRS))
		outliner_add_line_styles(soops, lb, sce, te);
}

// can be inlined if necessary
static void outliner_add_object_contents(SpaceOops *soops, TreeElement *te, TreeStoreElem *tselem, Object *ob)
{
	int a = 0;
	
	if (outliner_animdata_test(ob->adt))
		outliner_add_element(soops, &te->subtree, ob, te, TSE_ANIM_DATA, 0);
	
	outliner_add_element(soops, &te->subtree, ob->poselib, te, 0, 0); // XXX FIXME.. add a special type for this
	
	if (ob->proxy && ob->id.lib == NULL)
		outliner_add_element(soops, &te->subtree, ob->proxy, te, TSE_PROXY, 0);
	
	outliner_add_element(soops, &te->subtree, ob->data, te, 0, 0);
	
	if (ob->pose) {
		bArmature *arm = ob->data;
		bPoseChannel *pchan;
		TreeElement *ten;
		TreeElement *tenla = outliner_add_element(soops, &te->subtree, ob, te, TSE_POSE_BASE, 0);
		
		tenla->name = IFACE_("Pose");
		
		/* channels undefined in editmode, but we want the 'tenla' pose icon itself */
		if ((arm->edbo == NULL) && (ob->mode & OB_MODE_POSE)) {
			int a = 0, const_index = 1000;    /* ensure unique id for bone constraints */
			
			for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next, a++) {
				ten = outliner_add_element(soops, &tenla->subtree, ob, tenla, TSE_POSE_CHANNEL, a);
				ten->name = pchan->name;
				ten->directdata = pchan;
				pchan->temp = (void *)ten;
				
				if (pchan->constraints.first) {
					//Object *target;
					bConstraint *con;
					TreeElement *ten1;
					TreeElement *tenla1 = outliner_add_element(soops, &ten->subtree, ob, ten, TSE_CONSTRAINT_BASE, 0);
					//char *str;
					
					tenla1->name = IFACE_("Constraints");
					for (con = pchan->constraints.first; con; con = con->next, const_index++) {
						ten1 = outliner_add_element(soops, &tenla1->subtree, ob, tenla1, TSE_CONSTRAINT, const_index);
#if 0 /* disabled as it needs to be reworked for recoded constraints system */
						target = get_constraint_target(con, &str);
						if (str && str[0]) ten1->name = str;
						else if (target) ten1->name = target->id.name + 2;
						else ten1->name = con->name;
#endif
						ten1->name = con->name;
						ten1->directdata = con;
						/* possible add all other types links? */
					}
				}
			}
			/* make hierarchy */
			ten = tenla->subtree.first;
			while (ten) {
				TreeElement *nten = ten->next, *par;
				tselem = TREESTORE(ten);
				if (tselem->type == TSE_POSE_CHANNEL) {
					pchan = (bPoseChannel *)ten->directdata;
					if (pchan->parent) {
						BLI_remlink(&tenla->subtree, ten);
						par = (TreeElement *)pchan->parent->temp;
						BLI_addtail(&par->subtree, ten);
						ten->parent = par;
					}
				}
				ten = nten;
			}
		}
		
		/* Pose Groups */
		if (ob->pose->agroups.first) {
			bActionGroup *agrp;
			TreeElement *ten;
			TreeElement *tenla = outliner_add_element(soops, &te->subtree, ob, te, TSE_POSEGRP_BASE, 0);
			int a = 0;
			
			tenla->name = IFACE_("Bone Groups");
			for (agrp = ob->pose->agroups.first; agrp; agrp = agrp->next, a++) {
				ten = outliner_add_element(soops, &tenla->subtree, ob, tenla, TSE_POSEGRP, a);
				ten->name = agrp->name;
				ten->directdata = agrp;
			}
		}
	}
	
	for (a = 0; a < ob->totcol; a++)
		outliner_add_element(soops, &te->subtree, ob->mat[a], te, 0, a);
	
	if (ob->constraints.first) {
		//Object *target;
		bConstraint *con;
		TreeElement *ten;
		TreeElement *tenla = outliner_add_element(soops, &te->subtree, ob, te, TSE_CONSTRAINT_BASE, 0);
		//char *str;
		
		tenla->name = IFACE_("Constraints");
		for (con = ob->constraints.first, a = 0; con; con = con->next, a++) {
			ten = outliner_add_element(soops, &tenla->subtree, ob, tenla, TSE_CONSTRAINT, a);
#if 0 /* disabled due to constraints system targets recode... code here needs review */
			target = get_constraint_target(con, &str);
			if (str && str[0]) ten->name = str;
			else if (target) ten->name = target->id.name + 2;
			else ten->name = con->name;
#endif
			ten->name = con->name;
			ten->directdata = con;
			/* possible add all other types links? */
		}
	}
	
	if (ob->modifiers.first) {
		ModifierData *md;
		TreeElement *temod = outliner_add_element(soops, &te->subtree, ob, te, TSE_MODIFIER_BASE, 0);
		int index;
		
		temod->name = IFACE_("Modifiers");
		for (index = 0, md = ob->modifiers.first; md; index++, md = md->next) {
			TreeElement *te = outliner_add_element(soops, &temod->subtree, ob, temod, TSE_MODIFIER, index);
			te->name = md->name;
			te->directdata = md;
			
			if (md->type == eModifierType_Lattice) {
				outliner_add_element(soops, &te->subtree, ((LatticeModifierData *) md)->object, te, TSE_LINKED_OB, 0);
			}
			else if (md->type == eModifierType_Curve) {
				outliner_add_element(soops, &te->subtree, ((CurveModifierData *) md)->object, te, TSE_LINKED_OB, 0);
			}
			else if (md->type == eModifierType_Armature) {
				outliner_add_element(soops, &te->subtree, ((ArmatureModifierData *) md)->object, te, TSE_LINKED_OB, 0);
			}
			else if (md->type == eModifierType_Hook) {
				outliner_add_element(soops, &te->subtree, ((HookModifierData *) md)->object, te, TSE_LINKED_OB, 0);
			}
			else if (md->type == eModifierType_ParticleSystem) {
				TreeElement *ten;
				ParticleSystem *psys = ((ParticleSystemModifierData *) md)->psys;
				
				ten = outliner_add_element(soops, &te->subtree, ob, te, TSE_LINKED_PSYS, 0);
				ten->directdata = psys;
				ten->name = psys->part->id.name + 2;
			}
		}
	}
	
	/* vertex groups */
	if (ob->defbase.first) {
		bDeformGroup *defgroup;
		TreeElement *ten;
		TreeElement *tenla = outliner_add_element(soops, &te->subtree, ob, te, TSE_DEFGROUP_BASE, 0);
		
		tenla->name = IFACE_("Vertex Groups");
		for (defgroup = ob->defbase.first, a = 0; defgroup; defgroup = defgroup->next, a++) {
			ten = outliner_add_element(soops, &tenla->subtree, ob, tenla, TSE_DEFGROUP, a);
			ten->name = defgroup->name;
			ten->directdata = defgroup;
		}
	}
	
	/* duplicated group */
	if (ob->dup_group)
		outliner_add_element(soops, &te->subtree, ob->dup_group, te, 0, 0);
}


// can be inlined if necessary
static void outliner_add_id_contents(SpaceOops *soops, TreeElement *te, TreeStoreElem *tselem, ID *id)
{
	/* tuck pointer back in object, to construct hierarchy */
	if (GS(id->name) == ID_OB) id->newid = (ID *)te;
	
	/* expand specific data always */
	switch (GS(id->name)) {
		case ID_LI:
		{
			te->name = ((Library *)id)->name;
			break;
		}
		case ID_SCE:
		{
			outliner_add_scene_contents(soops, &te->subtree, (Scene *)id, te);
			break;
		}
		case ID_OB:
		{
			outliner_add_object_contents(soops, te, tselem, (Object *)id);
			break;
		}
		case ID_ME:
		{
			Mesh *me = (Mesh *)id;
			int a;
			
			if (outliner_animdata_test(me->adt))
				outliner_add_element(soops, &te->subtree, me, te, TSE_ANIM_DATA, 0);
			
			outliner_add_element(soops, &te->subtree, me->key, te, 0, 0);
			for (a = 0; a < me->totcol; a++)
				outliner_add_element(soops, &te->subtree, me->mat[a], te, 0, a);
			/* could do tfaces with image links, but the images are not grouped nicely.
			 * would require going over all tfaces, sort images in use. etc... */
			break;
		}
		case ID_CU:
		{
			Curve *cu = (Curve *)id;
			int a;
			
			if (outliner_animdata_test(cu->adt))
				outliner_add_element(soops, &te->subtree, cu, te, TSE_ANIM_DATA, 0);
			
			for (a = 0; a < cu->totcol; a++)
				outliner_add_element(soops, &te->subtree, cu->mat[a], te, 0, a);
			break;
		}
		case ID_MB:
		{
			MetaBall *mb = (MetaBall *)id;
			int a;
			
			if (outliner_animdata_test(mb->adt))
				outliner_add_element(soops, &te->subtree, mb, te, TSE_ANIM_DATA, 0);
			
			for (a = 0; a < mb->totcol; a++)
				outliner_add_element(soops, &te->subtree, mb->mat[a], te, 0, a);
			break;
		}
		case ID_MA:
		{
			Material *ma = (Material *)id;
			int a;
			
			if (outliner_animdata_test(ma->adt))
				outliner_add_element(soops, &te->subtree, ma, te, TSE_ANIM_DATA, 0);
			
			for (a = 0; a < MAX_MTEX; a++) {
				if (ma->mtex[a]) outliner_add_element(soops, &te->subtree, ma->mtex[a]->tex, te, 0, a);
			}
			break;
		}
		case ID_TE:
		{
			Tex *tex = (Tex *)id;
			
			if (outliner_animdata_test(tex->adt))
				outliner_add_element(soops, &te->subtree, tex, te, TSE_ANIM_DATA, 0);
			
			outliner_add_element(soops, &te->subtree, tex->ima, te, 0, 0);
			break;
		}
		case ID_CA:
		{
			Camera *ca = (Camera *)id;
			
			if (outliner_animdata_test(ca->adt))
				outliner_add_element(soops, &te->subtree, ca, te, TSE_ANIM_DATA, 0);
			break;
		}
		case ID_LA:
		{
			Lamp *la = (Lamp *)id;
			int a;
			
			if (outliner_animdata_test(la->adt))
				outliner_add_element(soops, &te->subtree, la, te, TSE_ANIM_DATA, 0);
			
			for (a = 0; a < MAX_MTEX; a++) {
				if (la->mtex[a]) outliner_add_element(soops, &te->subtree, la->mtex[a]->tex, te, 0, a);
			}
			break;
		}
		case ID_SPK:
		{
			Speaker *spk = (Speaker *)id;

			if (outliner_animdata_test(spk->adt))
				outliner_add_element(soops, &te->subtree, spk, te, TSE_ANIM_DATA, 0);
			break;
		}
		case ID_WO:
		{
			World *wrld = (World *)id;
			int a;
			
			if (outliner_animdata_test(wrld->adt))
				outliner_add_element(soops, &te->subtree, wrld, te, TSE_ANIM_DATA, 0);
			
			for (a = 0; a < MAX_MTEX; a++) {
				if (wrld->mtex[a]) outliner_add_element(soops, &te->subtree, wrld->mtex[a]->tex, te, 0, a);
			}
			break;
		}
		case ID_KE:
		{
			Key *key = (Key *)id;
			
			if (outliner_animdata_test(key->adt))
				outliner_add_element(soops, &te->subtree, key, te, TSE_ANIM_DATA, 0);
			break;
		}
		case ID_AC:
		{
			// XXX do we want to be exposing the F-Curves here?
			//bAction *act = (bAction *)id;
			break;
		}
		case ID_AR:
		{
			bArmature *arm = (bArmature *)id;
			int a = 0;
			
			if (outliner_animdata_test(arm->adt))
				outliner_add_element(soops, &te->subtree, arm, te, TSE_ANIM_DATA, 0);
			
			if (arm->edbo) {
				EditBone *ebone;
				TreeElement *ten;
				
				for (ebone = arm->edbo->first; ebone; ebone = ebone->next, a++) {
					ten = outliner_add_element(soops, &te->subtree, id, te, TSE_EBONE, a);
					ten->directdata = ebone;
					ten->name = ebone->name;
					ebone->temp = ten;
				}
				/* make hierarchy */
				ten = arm->edbo->first ? ((EditBone *)arm->edbo->first)->temp : NULL;
				while (ten) {
					TreeElement *nten = ten->next, *par;
					ebone = (EditBone *)ten->directdata;
					if (ebone->parent) {
						BLI_remlink(&te->subtree, ten);
						par = ebone->parent->temp;
						BLI_addtail(&par->subtree, ten);
						ten->parent = par;
					}
					ten = nten;
				}
			}
			else {
				/* do not extend Armature when we have posemode */
				tselem = TREESTORE(te->parent);
				if (GS(tselem->id->name) == ID_OB && ((Object *)tselem->id)->mode & OB_MODE_POSE) {
					/* pass */
				}
				else {
					Bone *curBone;
					for (curBone = arm->bonebase.first; curBone; curBone = curBone->next) {
						outliner_add_bone(soops, &te->subtree, id, curBone, te, &a);
					}
				}
			}
			break;
		}
		case ID_LS:
		{
			FreestyleLineStyle *linestyle = (FreestyleLineStyle *)id;
			
			if (outliner_animdata_test(linestyle->adt))
				outliner_add_element(soops, &te->subtree, linestyle, te, TSE_ANIM_DATA, 0);
			break;
		}
	}
}

// TODO: this function needs to be split up! It's getting a bit too large...
// Note: "ID" is not always a real ID
static TreeElement *outliner_add_element(SpaceOops *soops, ListBase *lb, void *idv, 
                                         TreeElement *parent, short type, short index)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	ID *id = idv;
	
	if (ELEM3(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		id = ((PointerRNA *)idv)->id.data;
		if (!id) id = ((PointerRNA *)idv)->data;
	}

	/* One exception */
	if (type == TSE_ID_BASE) {
		/* pass */
	}
	else if (id == NULL) {
		return NULL;
	}

	te = MEM_callocN(sizeof(TreeElement), "tree elem");
	/* add to the visual tree */
	BLI_addtail(lb, te);
	/* add to the storage */
	check_persistent(soops, te, id, type, index);
	tselem = TREESTORE(te);
	
	/* if we are searching for something expand to see child elements */
	if (SEARCHING_OUTLINER(soops))
		tselem->flag |= TSE_CHILDSEARCH;
	
	te->parent = parent;
	te->index = index;   // for data arays
	if (ELEM3(type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP)) {
		/* pass */
	}
	else if (ELEM3(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		/* pass */
	}
	else if (type == TSE_ANIM_DATA) {
		/* pass */
	}
	else if (type == TSE_ID_BASE) {
		/* pass */
	}
	else {
		/* do here too, for blend file viewer, own ID_LI then shows file name */
		if (GS(id->name) == ID_LI)
			te->name = ((Library *)id)->name;
		else
			te->name = id->name + 2; // default, can be overridden by Library or non-ID data
		te->idcode = GS(id->name);
	}
	
	if (type == 0) {
		TreeStoreElem *tsepar = parent ? TREESTORE(parent) : NULL;
		
		/* ID datablock */
		if (tsepar == NULL || tsepar->type != TSE_ID_BASE)
			outliner_add_id_contents(soops, te, tselem, id);
	}
	else if (type == TSE_ANIM_DATA) {
		IdAdtTemplate *iat = (IdAdtTemplate *)idv;
		AnimData *adt = (AnimData *)iat->adt;
		
		/* this element's info */
		te->name = IFACE_("Animation");
		te->directdata = adt;
		
		/* Action */
		outliner_add_element(soops, &te->subtree, adt->action, te, 0, 0);
		
		/* Drivers */
		if (adt->drivers.first) {
			TreeElement *ted = outliner_add_element(soops, &te->subtree, adt, te, TSE_DRIVER_BASE, 0);
			ID *lastadded = NULL;
			FCurve *fcu;
			
			ted->name = IFACE_("Drivers");
		
			for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
				if (fcu->driver && fcu->driver->variables.first) {
					ChannelDriver *driver = fcu->driver;
					DriverVar *dvar;
					
					for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
						/* loop over all targets used here */
						DRIVER_TARGETS_USED_LOOPER(dvar) 
						{
							if (lastadded != dtar->id) {
								// XXX this lastadded check is rather lame, and also fails quite badly...
								outliner_add_element(soops, &ted->subtree, dtar->id, ted, TSE_LINKED_OB, 0);
								lastadded = dtar->id;
							}
						}
						DRIVER_TARGETS_LOOPER_END
					}
				}
			}
		}
		
		/* NLA Data */
		if (adt->nla_tracks.first) {
			TreeElement *tenla = outliner_add_element(soops, &te->subtree, adt, te, TSE_NLA, 0);
			NlaTrack *nlt;
			int a = 0;
			
			tenla->name = IFACE_("NLA Tracks");
			
			for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
				TreeElement *tenlt = outliner_add_element(soops, &tenla->subtree, nlt, tenla, TSE_NLA_TRACK, a);
				NlaStrip *strip;
				TreeElement *ten;
				int b = 0;
				
				tenlt->name = nlt->name;
				
				for (strip = nlt->strips.first; strip; strip = strip->next, b++) {
					ten = outliner_add_element(soops, &tenlt->subtree, strip->act, tenlt, TSE_NLA_ACTION, b);
					if (ten) ten->directdata = strip;
				}
			}
		}
	}
	else if (type == TSE_SEQUENCE) {
		Sequence *seq = (Sequence *) idv;
		Sequence *p;

		/*
		 * The idcode is a little hack, but the outliner
		 * only check te->idcode if te->type is equal to zero,
		 * so this is "safe".
		 */
		te->idcode = seq->type;
		te->directdata = seq;
		te->name = seq->name + 2;

		if (seq->type < SEQ_TYPE_EFFECT) {
			/*
			 * This work like the sequence.
			 * If the sequence have a name (not default name)
			 * show it, in other case put the filename.
			 */

			if (seq->type == SEQ_TYPE_META) {
				p = seq->seqbase.first;
				while (p) {
					outliner_add_element(soops, &te->subtree, (void *)p, te, TSE_SEQUENCE, index);
					p = p->next;
				}
			}
			else
				outliner_add_element(soops, &te->subtree, (void *)seq->strip, te, TSE_SEQ_STRIP, index);
		}
	}
	else if (type == TSE_SEQ_STRIP) {
		Strip *strip = (Strip *)idv;

		if (strip->dir[0] != '\0')
			te->name = strip->dir;
		else
			te->name = IFACE_("Strip None");
		te->directdata = strip;
	}
	else if (type == TSE_SEQUENCE_DUP) {
		Sequence *seq = (Sequence *)idv;

		te->idcode = seq->type;
		te->directdata = seq;
		te->name = seq->strip->stripdata->name;
	}
	else if (ELEM3(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		PointerRNA pptr, propptr, *ptr = (PointerRNA *)idv;
		PropertyRNA *prop, *iterprop;
		PropertyType proptype;
		int a, tot;

		/* we do lazy build, for speed and to avoid infinite recusion */

		if (ptr->data == NULL) {
			te->name = IFACE_("(empty)");
		}
		else if (type == TSE_RNA_STRUCT) {
			/* struct */
			te->name = RNA_struct_name_get_alloc(ptr, NULL, 0, NULL);

			if (te->name)
				te->flag |= TE_FREE_NAME;
			else
				te->name = RNA_struct_ui_name(ptr->type);

			/* If searching don't expand RNA entries */
			if (SEARCHING_OUTLINER(soops) && BLI_strcasecmp("RNA", te->name) == 0) tselem->flag &= ~TSE_CHILDSEARCH;

			iterprop = RNA_struct_iterator_property(ptr->type);
			tot = RNA_property_collection_length(ptr, iterprop);

			/* auto open these cases */
			if (!parent || (RNA_property_type(parent->directdata)) == PROP_POINTER)
				if (!tselem->used)
					tselem->flag &= ~TSE_CLOSED;

			if (TSELEM_OPEN(tselem, soops)) {
				for (a = 0; a < tot; a++)
					outliner_add_element(soops, &te->subtree, (void *)ptr, te, TSE_RNA_PROPERTY, a);
			}
			else if (tot)
				te->flag |= TE_LAZY_CLOSED;

			te->rnaptr = *ptr;
		}
		else if (type == TSE_RNA_PROPERTY) {
			/* property */
			iterprop = RNA_struct_iterator_property(ptr->type);
			RNA_property_collection_lookup_int(ptr, iterprop, index, &propptr);

			prop = propptr.data;
			proptype = RNA_property_type(prop);

			te->name = RNA_property_ui_name(prop);
			te->directdata = prop;
			te->rnaptr = *ptr;

			/* If searching don't expand RNA entries */
			if (SEARCHING_OUTLINER(soops) && BLI_strcasecmp("RNA", te->name) == 0) tselem->flag &= ~TSE_CHILDSEARCH;

			if (proptype == PROP_POINTER) {
				pptr = RNA_property_pointer_get(ptr, prop);

				if (pptr.data) {
					if (TSELEM_OPEN(tselem, soops))
						outliner_add_element(soops, &te->subtree, (void *)&pptr, te, TSE_RNA_STRUCT, -1);
					else
						te->flag |= TE_LAZY_CLOSED;
				}
			}
			else if (proptype == PROP_COLLECTION) {
				tot = RNA_property_collection_length(ptr, prop);

				if (TSELEM_OPEN(tselem, soops)) {
					for (a = 0; a < tot; a++) {
						RNA_property_collection_lookup_int(ptr, prop, a, &pptr);
						outliner_add_element(soops, &te->subtree, (void *)&pptr, te, TSE_RNA_STRUCT, a);
					}
				}
				else if (tot)
					te->flag |= TE_LAZY_CLOSED;
			}
			else if (ELEM3(proptype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT)) {
				tot = RNA_property_array_length(ptr, prop);

				if (TSELEM_OPEN(tselem, soops)) {
					for (a = 0; a < tot; a++)
						outliner_add_element(soops, &te->subtree, (void *)ptr, te, TSE_RNA_ARRAY_ELEM, a);
				}
				else if (tot)
					te->flag |= TE_LAZY_CLOSED;
			}
		}
		else if (type == TSE_RNA_ARRAY_ELEM) {
			char c;

			prop = parent->directdata;

			te->directdata = prop;
			te->rnaptr = *ptr;
			te->index = index;

			c = RNA_property_array_item_char(prop, index);

			te->name = MEM_callocN(sizeof(char) * 20, "OutlinerRNAArrayName");
			if (c) sprintf((char *)te->name, "  %c", c);
			else sprintf((char *)te->name, "  %d", index + 1);
			te->flag |= TE_FREE_NAME;
		}
	}
	else if (type == TSE_KEYMAP) {
		wmKeyMap *km = (wmKeyMap *)idv;
		wmKeyMapItem *kmi;
		char opname[OP_MAX_TYPENAME];
		
		te->directdata = idv;
		te->name = km->idname;
		
		if (TSELEM_OPEN(tselem, soops)) {
			int a = 0;
			
			for (kmi = km->items.first; kmi; kmi = kmi->next, a++) {
				const char *key = WM_key_event_string(kmi->type);
				
				if (key[0]) {
					wmOperatorType *ot = NULL;
					
					if (kmi->propvalue) {
						/* pass */
					}
					else {
						ot = WM_operatortype_find(kmi->idname, 0);
					}
					
					if (ot || kmi->propvalue) {
						TreeElement *ten = outliner_add_element(soops, &te->subtree, kmi, te, TSE_KEYMAP_ITEM, a);
						
						ten->directdata = kmi;
						
						if (kmi->propvalue) {
							ten->name = IFACE_("Modal map, not yet");
						}
						else {
							WM_operator_py_idname(opname, ot->idname);
							ten->name = BLI_strdup(opname);
							ten->flag |= TE_FREE_NAME;
						}
					}
				}
			}
		}
		else 
			te->flag |= TE_LAZY_CLOSED;
	}

	return te;
}

/* ======================================================= */
/* Sequencer mode tree building */

/* Helped function to put duplicate sequence in the same tree. */
static int need_add_seq_dup(Sequence *seq)
{
	Sequence *p;

	if ((!seq->strip) || (!seq->strip->stripdata))
		return(1);

	/*
	 * First check backward, if we found a duplicate
	 * sequence before this, don't need it, just return.
	 */
	p = seq->prev;
	while (p) {
		if ((!p->strip) || (!p->strip->stripdata)) {
			p = p->prev;
			continue;
		}

		if (!strcmp(p->strip->stripdata->name, seq->strip->stripdata->name))
			return(2);
		p = p->prev;
	}

	p = seq->next;
	while (p) {
		if ((!p->strip) || (!p->strip->stripdata)) {
			p = p->next;
			continue;
		}

		if (!strcmp(p->strip->stripdata->name, seq->strip->stripdata->name))
			return(0);
		p = p->next;
	}
	return(1);
}

static void outliner_add_seq_dup(SpaceOops *soops, Sequence *seq, TreeElement *te, short index)
{
	/* TreeElement *ch; */ /* UNUSED */
	Sequence *p;

	p = seq;
	while (p) {
		if ((!p->strip) || (!p->strip->stripdata) || (p->strip->stripdata->name[0] == '\0')) {
			p = p->next;
			continue;
		}

		if (!strcmp(p->strip->stripdata->name, seq->strip->stripdata->name))
			/* ch = */ /* UNUSED */ outliner_add_element(soops, &te->subtree, (void *)p, te, TSE_SEQUENCE, index);
		p = p->next;
	}
}

/* ======================================================= */
/* Generic Tree Building helpers - order these are called is top to bottom */

/* Hierarchy --------------------------------------------- */

/* make sure elements are correctly nested */
static void outliner_make_hierarchy(ListBase *lb)
{
	TreeElement *te, *ten, *tep;
	TreeStoreElem *tselem;

	/* build hierarchy */
	// XXX also, set extents here...
	te = lb->first;
	while (te) {
		ten = te->next;
		tselem = TREESTORE(te);
		
		if (tselem->type == 0 && te->idcode == ID_OB) {
			Object *ob = (Object *)tselem->id;
			if (ob->parent && ob->parent->id.newid) {
				BLI_remlink(lb, te);
				tep = (TreeElement *)ob->parent->id.newid;
				BLI_addtail(&tep->subtree, te);
				// set correct parent pointers
				for (te = tep->subtree.first; te; te = te->next) te->parent = tep;
			}
		}
		te = ten;
	}
}

/* Sorting ------------------------------------------------------ */

typedef struct tTreeSort {
	TreeElement *te;
	ID *id;
	const char *name;
	short idcode;
} tTreeSort;

/* alphabetical comparator, tryping to put objects first */
static int treesort_alpha_ob(const void *v1, const void *v2)
{
	const tTreeSort *x1 = v1, *x2 = v2;
	int comp;
	
	/* first put objects last (hierarchy) */
	comp = (x1->idcode == ID_OB);
	if (x2->idcode == ID_OB) comp += 2;
	
	if (comp == 1) return 1;
	else if (comp == 2) return -1;
	else if (comp == 3) {
		comp = strcmp(x1->name, x2->name);
		
		if (comp > 0) return 1;
		else if (comp < 0) return -1;
		return 0;
	}
	return 0;
}

/* alphabetical comparator */
static int treesort_alpha(const void *v1, const void *v2)
{
	const tTreeSort *x1 = v1, *x2 = v2;
	int comp;
	
	comp = strcmp(x1->name, x2->name);
	
	if (comp > 0) return 1;
	else if (comp < 0) return -1;
	return 0;
}


/* this is nice option for later? doesnt look too useful... */
#if 0
static int treesort_obtype_alpha(const void *v1, const void *v2)
{
	const tTreeSort *x1 = v1, *x2 = v2;
	
	/* first put objects last (hierarchy) */
	if (x1->idcode == ID_OB && x2->idcode != ID_OB) {
		return 1;
	}
	else if (x2->idcode == ID_OB && x1->idcode != ID_OB) {
		return -1;
	}
	else {
		/* 2nd we check ob type */
		if (x1->idcode == ID_OB && x2->idcode == ID_OB) {
			if      (((Object *)x1->id)->type > ((Object *)x2->id)->type) return  1;
			else if (((Object *)x1->id)->type > ((Object *)x2->id)->type) return -1;
			else return 0;
		}
		else {
			int comp = strcmp(x1->name, x2->name);
			
			if      (comp > 0) return  1;
			else if (comp < 0) return -1;
			return 0;
		}
	}
}
#endif

/* sort happens on each subtree individual */
static void outliner_sort(SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int totelem = 0;
	
	te = lb->last;
	if (te == NULL) return;
	tselem = TREESTORE(te);
	
	/* sorting rules; only object lists, ID lists, or deformgroups */
	if ( ELEM(tselem->type, TSE_DEFGROUP, TSE_ID_BASE) || (tselem->type == 0 && te->idcode == ID_OB)) {
		
		/* count first */
		for (te = lb->first; te; te = te->next) totelem++;
		
		if (totelem > 1) {
			tTreeSort *tear = MEM_mallocN(totelem * sizeof(tTreeSort), "tree sort array");
			tTreeSort *tp = tear;
			int skip = 0;

			for (te = lb->first; te; te = te->next, tp++) {
				tselem = TREESTORE(te);
				tp->te = te;
				tp->name = te->name;
				tp->idcode = te->idcode;
				
				if (tselem->type && tselem->type != TSE_DEFGROUP)
					tp->idcode = 0;  // don't sort this
				if (tselem->type == TSE_ID_BASE)
					tp->idcode = 1; // do sort this
				
				tp->id = tselem->id;
			}
			
			/* just sort alphabetically */
			if (tear->idcode == 1) {
				qsort(tear, totelem, sizeof(tTreeSort), treesort_alpha);
			}
			else {
				/* keep beginning of list */
				for (tp = tear, skip = 0; skip < totelem; skip++, tp++)
					if (tp->idcode) break;
				
				if (skip < totelem)
					qsort(tear + skip, totelem - skip, sizeof(tTreeSort), treesort_alpha_ob);
			}
			
			BLI_listbase_clear(lb);
			tp = tear;
			while (totelem--) {
				BLI_addtail(lb, tp->te);
				tp++;
			}
			MEM_freeN(tear);
		}
	}
	
	for (te = lb->first; te; te = te->next) {
		outliner_sort(soops, &te->subtree);
	}
}

/* Filtering ----------------------------------------------- */

static int outliner_filter_has_name(TreeElement *te, const char *name, int flags)
{
#if 0
	int found = 0;
	
	/* determine if match */
	if (flags & SO_FIND_CASE_SENSITIVE) {
		if (flags & SO_FIND_COMPLETE)
			found = strcmp(te->name, name) == 0;
		else
			found = strstr(te->name, name) != NULL;
	}
	else {
		if (flags & SO_FIND_COMPLETE)
			found = BLI_strcasecmp(te->name, name) == 0;
		else
			found = BLI_strcasestr(te->name, name) != NULL;
	}
#else
	
	int fn_flag = 0;
	int found = 0;
	
	if ((flags & SO_FIND_CASE_SENSITIVE) == 0)
		fn_flag |= FNM_CASEFOLD;

	if (flags & SO_FIND_COMPLETE) {
		found = fnmatch(name, te->name, fn_flag) == 0;
	}
	else {
		char fn_name[sizeof(((struct SpaceOops *)NULL)->search_string) + 2];
		BLI_snprintf(fn_name, sizeof(fn_name), "*%s*", name);
		found = fnmatch(fn_name, te->name, fn_flag) == 0;
	}
	return found;
#endif
}

static int outliner_filter_tree(SpaceOops *soops, ListBase *lb)
{
	TreeElement *te, *ten;
	TreeStoreElem *tselem;
	
	/* although we don't have any search string, we return TRUE 
	 * since the entire tree is ok then...
	 */
	if (soops->search_string[0] == 0)
		return 1;

	for (te = lb->first; te; te = ten) {
		ten = te->next;
		
		if (0 == outliner_filter_has_name(te, soops->search_string, soops->search_flags)) {
			/* item isn't something we're looking for, but...
			 *  - if the subtree is expanded, check if there are any matches that can be easily found
			 *		so that searching for "cu" in the default scene will still match the Cube
			 *	- otherwise, we can't see within the subtree and the item doesn't match,
			 *		so these can be safely ignored (i.e. the subtree can get freed)
			 */
			tselem = TREESTORE(te);
			
			/* flag as not a found item */
			tselem->flag &= ~TSE_SEARCHMATCH;
			
			if ((!TSELEM_OPEN(tselem, soops)) || outliner_filter_tree(soops, &te->subtree) == 0) {
				outliner_free_tree(&te->subtree);
				BLI_remlink(lb, te);
				
				if (te->flag & TE_FREE_NAME) MEM_freeN((void *)te->name);
				MEM_freeN(te);
			}
		}
		else {
			tselem = TREESTORE(te);
			
			/* flag as a found item - we can then highlight it */
			tselem->flag |= TSE_SEARCHMATCH;
			
			/* filter subtree too */
			outliner_filter_tree(soops, &te->subtree);
		}
	}
	
	/* if there are still items in the list, that means that there were still some matches */
	return (BLI_listbase_is_empty(lb) == false);
}

static void outliner_add_library_contents(Main *mainvar, SpaceOops *soops, TreeElement *te, Library *lib)
{
	TreeElement *ten;
	ListBase *lbarray[MAX_LIBARRAY];
	int a, tot;
	
	tot = set_listbasepointers(mainvar, lbarray);
	for (a = 0; a < tot; a++) {
		if (lbarray[a]->first) {
			ID *id = lbarray[a]->first;
			
			/* check if there's data in current lib */
			for (; id; id = id->next)
				if (id->lib == lib)
					break;
			
			if (id) {
				
				ten = outliner_add_element(soops, &te->subtree, (void *)lbarray[a], NULL, TSE_ID_BASE, 0);
				ten->directdata = lbarray[a];
				
				ten->name = (char *)BKE_idcode_to_name_plural(GS(id->name));
				if (ten->name == NULL)
					ten->name = "UNKNOWN";
				
				for (id = lbarray[a]->first; id; id = id->next) {
					if (id->lib == lib)
						outliner_add_element(soops, &ten->subtree, id, ten, 0, 0);
				}
			}
		}
	}
	
}


/* ======================================================= */
/* Main Tree Building API */

/* Main entry point for building the tree data-structure that the outliner represents */
// TODO: split each mode into its own function?
void outliner_build_tree(Main *mainvar, Scene *scene, SpaceOops *soops)
{
	Base *base;
	Object *ob;
	TreeElement *te = NULL, *ten;
	TreeStoreElem *tselem;
	int show_opened = !soops->treestore || !BLI_mempool_count(soops->treestore); /* on first view, we open scenes */

	/* Are we looking for something - we want to tag parents to filter child matches
	 * - NOT in datablocks view - searching all datablocks takes way too long to be useful
	 * - this variable is only set once per tree build */
	if (soops->search_string[0] != 0 && soops->outlinevis != SO_DATABLOCKS)
		soops->search_flags |= SO_SEARCH_RECURSIVE;
	else
		soops->search_flags &= ~SO_SEARCH_RECURSIVE;

	if (soops->tree.first && (soops->storeflag & SO_TREESTORE_REDRAW))
		return;

	outliner_free_tree(&soops->tree);
	outliner_storage_cleanup(soops);
	
	/* clear ob id.new flags */
	for (ob = mainvar->object.first; ob; ob = ob->id.next) ob->id.newid = NULL;
	
	/* options */
	if (soops->outlinevis == SO_LIBRARIES) {
		Library *lib;
		
		/* current file first - mainvar provides tselem with unique pointer - not used */
		ten = outliner_add_element(soops, &soops->tree, mainvar, NULL, TSE_ID_BASE, 0);
		ten->name = IFACE_("Current File");

		tselem = TREESTORE(ten);
		if (!tselem->used)
			tselem->flag &= ~TSE_CLOSED;
		
		outliner_add_library_contents(mainvar, soops, ten, NULL);
		
		for (lib = mainvar->library.first; lib; lib = lib->id.next) {
			ten = outliner_add_element(soops, &soops->tree, lib, NULL, 0, 0);
			lib->id.newid = (ID *)ten;
			
			outliner_add_library_contents(mainvar, soops, ten, lib);

		}
		/* make hierarchy */
		ten = soops->tree.first;
		ten = ten->next;  /* first one is main */
		while (ten) {
			TreeElement *nten = ten->next, *par;
			tselem = TREESTORE(ten);
			lib = (Library *)tselem->id;
			if (lib && lib->parent) {
				BLI_remlink(&soops->tree, ten);
				par = (TreeElement *)lib->parent->id.newid;
				BLI_addtail(&par->subtree, ten);
				ten->parent = par;
			}
			ten = nten;
		}
		/* restore newid pointers */
		for (lib = mainvar->library.first; lib; lib = lib->id.next)
			lib->id.newid = NULL;
		
	}
	else if (soops->outlinevis == SO_ALL_SCENES) {
		Scene *sce;
		for (sce = mainvar->scene.first; sce; sce = sce->id.next) {
			te = outliner_add_element(soops, &soops->tree, sce, NULL, 0, 0);
			tselem = TREESTORE(te);
			if (sce == scene && show_opened)
				tselem->flag &= ~TSE_CLOSED;
			
			for (base = sce->base.first; base; base = base->next) {
				ten = outliner_add_element(soops, &te->subtree, base->object, te, 0, 0);
				ten->directdata = base;
			}
			outliner_make_hierarchy(&te->subtree);
			/* clear id.newid, to prevent objects be inserted in wrong scenes (parent in other scene) */
			for (base = sce->base.first; base; base = base->next) base->object->id.newid = NULL;
		}
	}
	else if (soops->outlinevis == SO_CUR_SCENE) {
		
		outliner_add_scene_contents(soops, &soops->tree, scene, NULL);
		
		for (base = scene->base.first; base; base = base->next) {
			ten = outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
			ten->directdata = base;
		}
		outliner_make_hierarchy(&soops->tree);
	}
	else if (soops->outlinevis == SO_VISIBLE) {
		for (base = scene->base.first; base; base = base->next) {
			if (base->lay & scene->lay)
				outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
		}
		outliner_make_hierarchy(&soops->tree);
	}
	else if (soops->outlinevis == SO_GROUPS) {
		Group *group;
		GroupObject *go;
		
		for (group = mainvar->group.first; group; group = group->id.next) {
			if (group->gobject.first) {
				te = outliner_add_element(soops, &soops->tree, group, NULL, 0, 0);
				
				for (go = group->gobject.first; go; go = go->next) {
					ten = outliner_add_element(soops, &te->subtree, go->ob, te, 0, 0);
					ten->directdata = NULL; /* eh, why? */
				}
				outliner_make_hierarchy(&te->subtree);
				/* clear id.newid, to prevent objects be inserted in wrong scenes (parent in other scene) */
				for (go = group->gobject.first; go; go = go->next) go->ob->id.newid = NULL;
			}
		}
	}
	else if (soops->outlinevis == SO_SAME_TYPE) {
		Object *ob = OBACT;
		if (ob) {
			for (base = scene->base.first; base; base = base->next) {
				if (base->object->type == ob->type) {
					ten = outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
					ten->directdata = base;
				}
			}
			outliner_make_hierarchy(&soops->tree);
		}
	}
	else if (soops->outlinevis == SO_SELECTED) {
		for (base = scene->base.first; base; base = base->next) {
			if (base->lay & scene->lay) {
				if (base == BASACT || (base->flag & SELECT)) {
					ten = outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
					ten->directdata = base;
				}
			}
		}
		outliner_make_hierarchy(&soops->tree);
	}
	else if (soops->outlinevis == SO_SEQUENCE) {
		Sequence *seq;
		Editing *ed = BKE_sequencer_editing_get(scene, FALSE);
		int op;

		if (ed == NULL)
			return;

		seq = ed->seqbasep->first;
		if (!seq)
			return;

		while (seq) {
			op = need_add_seq_dup(seq);
			if (op == 1) {
				/* ten = */ outliner_add_element(soops, &soops->tree, (void *)seq, NULL, TSE_SEQUENCE, 0);
			}
			else if (op == 0) {
				ten = outliner_add_element(soops, &soops->tree, (void *)seq, NULL, TSE_SEQUENCE_DUP, 0);
				outliner_add_seq_dup(soops, seq, ten, 0);
			}
			seq = seq->next;
		}
	}
	else if (soops->outlinevis == SO_DATABLOCKS) {
		PointerRNA mainptr;

		RNA_main_pointer_create(mainvar, &mainptr);

		ten = outliner_add_element(soops, &soops->tree, (void *)&mainptr, NULL, TSE_RNA_STRUCT, -1);

		if (show_opened) {
			tselem = TREESTORE(ten);
			tselem->flag &= ~TSE_CLOSED;
		}
	}
	else if (soops->outlinevis == SO_USERDEF) {
		PointerRNA userdefptr;

		RNA_pointer_create(NULL, &RNA_UserPreferences, &U, &userdefptr);

		ten = outliner_add_element(soops, &soops->tree, (void *)&userdefptr, NULL, TSE_RNA_STRUCT, -1);

		if (show_opened) {
			tselem = TREESTORE(ten);
			tselem->flag &= ~TSE_CLOSED;
		}
	}
	else {
		ten = outliner_add_element(soops, &soops->tree, OBACT, NULL, 0, 0);
		if (ten) ten->directdata = BASACT;
	}

	outliner_sort(soops, &soops->tree);
	outliner_filter_tree(soops, &soops->tree);
}


