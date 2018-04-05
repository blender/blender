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
#include "DNA_cachefile_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_sequence_types.h"
#include "DNA_speaker_types.h"
#include "DNA_object_types.h"
#include "DNA_linestyle_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_mempool.h"
#include "BLI_fnmatch.h"

#include "BLT_translation.h"

#include "BKE_fcurve.h"
#include "BKE_main.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_sequencer.h"
#include "BKE_idcode.h"
#include "BKE_outliner_treehash.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_armature.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "outliner_intern.h"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

/* prototypes */
static void outliner_add_layer_collections_recursive(
        SpaceOops *soops, ListBase *tree, ID *id, ListBase *layer_collections, TreeElement *parent_ten,
        const bool show_objects);
static void outliner_add_view_layer(
        SpaceOops *soops, ListBase *tree, TreeElement *parent,
        Scene *scene, ViewLayer *layer, const bool show_objects);
static void outliner_make_hierarchy(ListBase *lb);

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
			soops->storeflag &= ~SO_TREESTORE_CLEANUP;

			BLI_mempool_iternew(ts, &iter);
			while ((tselem = BLI_mempool_iterstep(&iter))) {
				if (tselem->id == NULL) unused++;
			}
			
			if (unused) {
				if (BLI_mempool_len(ts) == unused) {
					BLI_mempool_destroy(ts);
					soops->treestore = NULL;
					if (soops->treehash) {
						BKE_outliner_treehash_free(soops->treehash);
						soops->treehash = NULL;
					}
				}
				else {
					TreeStoreElem *tsenew;
					BLI_mempool *new_ts = BLI_mempool_create(sizeof(TreeStoreElem), BLI_mempool_len(ts) - unused,
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
						BKE_outliner_treehash_rebuild_from_treestore(soops->treehash, soops->treestore);
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
		soops->treehash = BKE_outliner_treehash_create_from_treestore(soops->treestore);
	}

	/* find any unused tree element in treestore and mark it as used
	 * (note that there may be multiple unused elements in case of linked objects) */
	tselem = BKE_outliner_treehash_lookup_unused(soops->treehash, type, nr, id);
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
	BKE_outliner_treehash_add_element(soops->treehash, tselem);
}

/* ********************************************************* */
/* Tree Management */

void outliner_free_tree(ListBase *tree)
{
	for (TreeElement *element = tree->first, *element_next; element; element = element_next) {
		element_next = element->next;
		outliner_free_tree_element(element, tree);
	}
}

void outliner_cleanup_tree(SpaceOops *soops)
{
	outliner_free_tree(&soops->tree);
	outliner_storage_cleanup(soops);
}

/**
 * Free \a element and its sub-tree and remove its link in \a parent_subtree.
 *
 * \note Does not remove the TreeStoreElem of \a element!
 * \param parent_subtree Subtree of the parent element, so the list containing \a element.
 */
void outliner_free_tree_element(TreeElement *element, ListBase *parent_subtree)
{
	BLI_assert(BLI_findindex(parent_subtree, element) > -1);
	BLI_remlink(parent_subtree, element);

	outliner_free_tree(&element->subtree);

	if (element->flag & TE_FREE_NAME) {
		MEM_freeN((void *)element->name);
	}
	MEM_freeN(element);
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

static void outliner_add_passes(SpaceOops *soops, TreeElement *tenla, ID *id, ViewLayer *view_layer)
{
	TreeStoreElem *tselem = NULL;
	TreeElement *te = NULL;

	/* log stuff is to convert bitflags (powers of 2) to small integers,
	 * in order to not overflow short tselem->nr */
	
	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_COMBINED));
	te->name = IFACE_("Combined");
	te->directdata = &view_layer->passflag;
	
	/* save cpu cycles, but we add the first to invoke an open/close triangle */
	tselem = TREESTORE(tenla);
	if (tselem->flag & TSE_CLOSED)
		return;
	
	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_Z));
	te->name = IFACE_("Z");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_VECTOR));
	te->name = IFACE_("Vector");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_NORMAL));
	te->name = IFACE_("Normal");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_UV));
	te->name = IFACE_("UV");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_MIST));
	te->name = IFACE_("Mist");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_INDEXOB));
	te->name = IFACE_("Index Object");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_INDEXMA));
	te->name = IFACE_("Index Material");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_RGBA));
	te->name = IFACE_("Color");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_DIFFUSE));
	te->name = IFACE_("Diffuse");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_SPEC));
	te->name = IFACE_("Specular");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_SHADOW));
	te->name = IFACE_("Shadow");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_AO));
	te->name = IFACE_("AO");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_REFLECT));
	te->name = IFACE_("Reflection");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_REFRACT));
	te->name = IFACE_("Refraction");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_INDIRECT));
	te->name = IFACE_("Indirect");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_ENVIRONMENT));
	te->name = IFACE_("Environment");
	te->directdata = &view_layer->passflag;

	te = outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, LOG2I(SCE_PASS_EMIT));
	te->name = IFACE_("Emit");
	te->directdata = &view_layer->passflag;
}

#undef LOG2I

static bool outliner_animdata_test(AnimData *adt)
{
	if (adt)
		return (adt->action || adt->drivers.first || adt->nla_tracks.first);
	return false;
}

#ifdef WITH_FREESTYLE
static void outliner_add_line_styles(SpaceOops *soops, ListBase *lb, Scene *sce, TreeElement *te)
{
	ViewLayer *view_layer;
	FreestyleLineSet *lineset;

	for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
		for (lineset = view_layer->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
			FreestyleLineStyle *linestyle = lineset->linestyle;
			if (linestyle) {
				linestyle->id.tag |= LIB_TAG_DOIT;
			}
		}
	}
	for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
		for (lineset = view_layer->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
			FreestyleLineStyle *linestyle = lineset->linestyle;
			if (linestyle) {
				if (!(linestyle->id.tag & LIB_TAG_DOIT))
					continue;
				linestyle->id.tag &= ~LIB_TAG_DOIT;
				outliner_add_element(soops, lb, linestyle, te, 0, 0);
			}
		}
	}
}
#endif

static void outliner_add_scene_contents(SpaceOops *soops, ListBase *lb, Scene *sce, TreeElement *te)
{
	ViewLayer *view_layer;
	TreeElement *tenla = outliner_add_element(soops, lb, sce, te, TSE_R_LAYER_BASE, 0);
	int a;
	
	tenla->name = IFACE_("View Layers");
	for (a = 0, view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next, a++) {
		TreeElement *tenlay = outliner_add_element(soops, &tenla->subtree, sce, te, TSE_R_LAYER, a);
		tenlay->name = view_layer->name;
		tenlay->directdata = &view_layer->flag;

		TreeElement *te_view_layers;
		te_view_layers = outliner_add_element(soops, &tenlay->subtree, sce, tenlay, TSE_LAYER_COLLECTION_BASE, 0);
		te_view_layers->name = IFACE_("Collections");
		outliner_add_view_layer(soops, &te_view_layers->subtree, te_view_layers, sce, view_layer, false);

		TreeElement *te_passes;
		te_passes = outliner_add_element(soops, &tenlay->subtree, sce, tenlay, TSE_LAYER_COLLECTION_BASE, 0);
		te_passes->name = IFACE_("Passes");
		outliner_add_passes(soops, te_passes, &sce->id, view_layer);
	}
	
	// TODO: move this to the front?
	if (outliner_animdata_test(sce->adt))
		outliner_add_element(soops, lb, sce, te, TSE_ANIM_DATA, 0);
		
	outliner_add_element(soops, lb, sce->gpd, te, 0, 0);
	
#ifdef WITH_FREESTYLE
	if (STREQ(sce->view_render->engine_id, RE_engine_id_BLENDER_RENDER) && (sce->r.mode & R_EDGE_FRS))
		outliner_add_line_styles(soops, lb, sce, te);
#endif
}

TreeTraversalAction outliner_find_selected_objects(TreeElement *te, void *customdata)
{
	struct ObjectsSelectedData *data = customdata;
	TreeStoreElem *tselem = TREESTORE(te);

	if (ELEM(tselem->type, TSE_LAYER_COLLECTION, TSE_SCENE_COLLECTION)) {
		return TRAVERSE_CONTINUE;
	}

	if (tselem->type || (tselem->id == NULL) || (GS(tselem->id->name) != ID_OB)) {
		return TRAVERSE_SKIP_CHILDS;
	}

	BLI_addtail(&data->objects_selected_array, BLI_genericNodeN(te));

	return TRAVERSE_CONTINUE;
}

/**
 * Move objects from a collection to another.
 * We ignore the original object being inserted, we used it for polling only.
 * Instead we move all the selected objects around.
 */
static void outliner_object_reorder(
        Main *bmain, SpaceOops *soops,
        TreeElement *insert_element,
        TreeElement *insert_handle, TreeElementInsertType action,
        const wmEvent *event)
{
	SceneCollection *sc = outliner_scene_collection_from_tree_element(insert_handle);
	SceneCollection *sc_ob_parent = NULL;
	ID *id = insert_handle->store_elem->id;

	BLI_assert(action == TE_INSERT_INTO);
	UNUSED_VARS_NDEBUG(action);

	struct ObjectsSelectedData data = {
		.objects_selected_array  = {NULL, NULL},
	};

	const bool is_append = event->ctrl;

	/* Make sure we include the originally inserted element as well. */
	TREESTORE(insert_element)->flag |= TSE_SELECTED;

	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, outliner_find_selected_objects, &data);
	LISTBASE_FOREACH (LinkData *, link, &data.objects_selected_array) {
		TreeElement *ten_selected = (TreeElement *)link->data;
		Object *ob = (Object *)TREESTORE(ten_selected)->id;

		if (is_append) {
			BKE_collection_object_add(id, sc, ob);
			continue;
		}

		/* Find parent scene-collection of object. */
		if (ten_selected->parent) {
			for (TreeElement *te_ob_parent = ten_selected->parent; te_ob_parent; te_ob_parent = te_ob_parent->parent) {
				if (ELEM(TREESTORE(te_ob_parent)->type, TSE_SCENE_COLLECTION, TSE_LAYER_COLLECTION)) {
					sc_ob_parent = outliner_scene_collection_from_tree_element(te_ob_parent);
					break;
				}
			}
		}
		else {
			sc_ob_parent = BKE_collection_master(id);
		}

		BKE_collection_object_move(id, sc, sc_ob_parent, ob);
	}

	BLI_freelistN(&data.objects_selected_array);

	DEG_relations_tag_update(bmain);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(id, 0);

	WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
}

static bool outliner_object_reorder_poll(
        const TreeElement *insert_element,
        TreeElement **io_insert_handle, TreeElementInsertType *io_action)
{
	TreeStoreElem *tselem_handle = TREESTORE(*io_insert_handle);
	if (ELEM(tselem_handle->type, TSE_SCENE_COLLECTION, TSE_LAYER_COLLECTION) &&
	    (insert_element->parent != *io_insert_handle))
	{
		*io_action = TE_INSERT_INTO;
		return true;
	}

	return false;
}

// can be inlined if necessary
static void outliner_add_object_contents(SpaceOops *soops, TreeElement *te, TreeStoreElem *tselem, Object *ob)
{
	te->reinsert = outliner_object_reorder;
	te->reinsert_poll = outliner_object_reorder_poll;

	if (outliner_animdata_test(ob->adt))
		outliner_add_element(soops, &te->subtree, ob, te, TSE_ANIM_DATA, 0);

	outliner_add_element(soops, &te->subtree, ob->poselib, te, 0, 0); // XXX FIXME.. add a special type for this
	
	if (ob->proxy && !ID_IS_LINKED(ob))
		outliner_add_element(soops, &te->subtree, ob->proxy, te, TSE_PROXY, 0);
		
	outliner_add_element(soops, &te->subtree, ob->gpd, te, 0, 0);
	
	outliner_add_element(soops, &te->subtree, ob->data, te, 0, 0);
	
	if (ob->pose) {
		bArmature *arm = ob->data;
		bPoseChannel *pchan;
		TreeElement *tenla = outliner_add_element(soops, &te->subtree, ob, te, TSE_POSE_BASE, 0);
		
		tenla->name = IFACE_("Pose");
		
		/* channels undefined in editmode, but we want the 'tenla' pose icon itself */
		if ((arm->edbo == NULL) && (ob->mode & OB_MODE_POSE)) {
			TreeElement *ten;
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
			TreeElement *ten_bonegrp = outliner_add_element(soops, &te->subtree, ob, te, TSE_POSEGRP_BASE, 0);
			int a = 0;

			ten_bonegrp->name = IFACE_("Bone Groups");
			for (agrp = ob->pose->agroups.first; agrp; agrp = agrp->next, a++) {
				TreeElement *ten;
				ten = outliner_add_element(soops, &ten_bonegrp->subtree, ob, ten_bonegrp, TSE_POSEGRP, a);
				ten->name = agrp->name;
				ten->directdata = agrp;
			}
		}
	}
	
	for (int a = 0; a < ob->totcol; a++) {
		outliner_add_element(soops, &te->subtree, ob->mat[a], te, 0, a);
	}
	
	if (ob->constraints.first) {
		//Object *target;
		bConstraint *con;
		TreeElement *ten;
		TreeElement *tenla = outliner_add_element(soops, &te->subtree, ob, te, TSE_CONSTRAINT_BASE, 0);
		//char *str;
		int a;
		
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
		TreeElement *ten_mod = outliner_add_element(soops, &te->subtree, ob, te, TSE_MODIFIER_BASE, 0);
		int index;
		
		ten_mod->name = IFACE_("Modifiers");
		for (index = 0, md = ob->modifiers.first; md; index++, md = md->next) {
			TreeElement *ten = outliner_add_element(soops, &ten_mod->subtree, ob, ten_mod, TSE_MODIFIER, index);
			ten->name = md->name;
			ten->directdata = md;
			
			if (md->type == eModifierType_Lattice) {
				outliner_add_element(soops, &ten->subtree, ((LatticeModifierData *) md)->object, ten, TSE_LINKED_OB, 0);
			}
			else if (md->type == eModifierType_Curve) {
				outliner_add_element(soops, &ten->subtree, ((CurveModifierData *) md)->object, ten, TSE_LINKED_OB, 0);
			}
			else if (md->type == eModifierType_Armature) {
				outliner_add_element(soops, &ten->subtree, ((ArmatureModifierData *) md)->object, ten, TSE_LINKED_OB, 0);
			}
			else if (md->type == eModifierType_Hook) {
				outliner_add_element(soops, &ten->subtree, ((HookModifierData *) md)->object, ten, TSE_LINKED_OB, 0);
			}
			else if (md->type == eModifierType_ParticleSystem) {
				ParticleSystem *psys = ((ParticleSystemModifierData *) md)->psys;
				TreeElement *ten_psys;
				
				ten_psys = outliner_add_element(soops, &ten->subtree, ob, te, TSE_LINKED_PSYS, 0);
				ten_psys->directdata = psys;
				ten_psys->name = psys->part->id.name + 2;
			}
		}
	}
	
	/* vertex groups */
	if (ob->defbase.first) {
		bDeformGroup *defgroup;
		TreeElement *ten;
		TreeElement *tenla = outliner_add_element(soops, &te->subtree, ob, te, TSE_DEFGROUP_BASE, 0);
		int a;
		
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
		case ID_CF:
		{
			CacheFile *cache_file = (CacheFile *)id;

			if (outliner_animdata_test(cache_file->adt)) {
				outliner_add_element(soops, &te->subtree, cache_file, te, TSE_ANIM_DATA, 0);
			}

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
		case ID_LP:
		{
			LightProbe *prb = (LightProbe *)id;

			if (outliner_animdata_test(prb->adt))
				outliner_add_element(soops, &te->subtree, prb, te, TSE_ANIM_DATA, 0);
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
					ebone->temp.p = ten;
				}
				/* make hierarchy */
				ten = arm->edbo->first ? ((EditBone *)arm->edbo->first)->temp.p : NULL;
				while (ten) {
					TreeElement *nten = ten->next, *par;
					ebone = (EditBone *)ten->directdata;
					if (ebone->parent) {
						BLI_remlink(&te->subtree, ten);
						par = ebone->parent->temp.p;
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
			int a;
			
			if (outliner_animdata_test(linestyle->adt))
				outliner_add_element(soops, &te->subtree, linestyle, te, TSE_ANIM_DATA, 0);

			for (a = 0; a < MAX_MTEX; a++) {
				if (linestyle->mtex[a])
					outliner_add_element(soops, &te->subtree, linestyle->mtex[a]->tex, te, 0, a);
			}
			break;
		}
		case ID_GD:
		{
			bGPdata *gpd = (bGPdata *)id;
			bGPDlayer *gpl;
			int a = 0;
			
			if (outliner_animdata_test(gpd->adt))
				outliner_add_element(soops, &te->subtree, gpd, te, TSE_ANIM_DATA, 0);
			
			// TODO: base element for layers?
			for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
				outliner_add_element(soops, &te->subtree, gpl, te, TSE_GP_LAYER, a);
				a++;
			}
			break;
		}
		default:
			break;
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
	
	if (ELEM(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		id = ((PointerRNA *)idv)->id.data;
		if (!id) id = ((PointerRNA *)idv)->data;
	}
	else if (type == TSE_GP_LAYER) {
		/* idv is the layer its self */
		id = TREESTORE(parent)->id;
	}

	/* exceptions */
	if (type == TSE_ID_BASE) {
		/* pass */
	}
	else if (id == NULL) {
		return NULL;
	}

	if (type == 0) {
		/* Zero type means real ID, ensure we do not get non-outliner ID types here... */
		BLI_assert(TREESTORE_ID_TYPE(id));
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
	if (ELEM(type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP)) {
		/* pass */
	}
	else if (ELEM(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		/* pass */
	}
	else if (type == TSE_ANIM_DATA) {
		/* pass */
	}
	else if (type == TSE_GP_LAYER) {
		/* pass */
	}
	else if (ELEM(type, TSE_LAYER_COLLECTION, TSE_SCENE_COLLECTION)) {
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
	else if (type == TSE_GP_LAYER) {
		bGPDlayer *gpl = (bGPDlayer *)idv;
		
		te->name = gpl->info;
		te->directdata = gpl;
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

		if (!(seq->type & SEQ_TYPE_EFFECT)) {
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
	else if (ELEM(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		PointerRNA pptr, propptr, *ptr = (PointerRNA *)idv;
		PropertyRNA *prop, *iterprop;
		PropertyType proptype;

		/* Don't display arrays larger, weak but index is stored as a short,
		 * also the outliner isn't intended for editing such large data-sets. */
		BLI_STATIC_ASSERT(sizeof(te->index) == 2, "Index is no longer short!");
		const int tot_limit = SHRT_MAX;

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
			CLAMP_MAX(tot, tot_limit);

			/* auto open these cases */
			if (!parent || (RNA_property_type(parent->directdata)) == PROP_POINTER)
				if (!tselem->used)
					tselem->flag &= ~TSE_CLOSED;

			if (TSELEM_OPEN(tselem, soops)) {
				for (a = 0; a < tot; a++) {
					RNA_property_collection_lookup_int(ptr, iterprop, a, &propptr);
					if (!(RNA_property_flag(propptr.data) & PROP_HIDDEN)) {
						outliner_add_element(soops, &te->subtree, (void *)ptr, te, TSE_RNA_PROPERTY, a);
					}
				}
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
				CLAMP_MAX(tot, tot_limit);

				if (TSELEM_OPEN(tselem, soops)) {
					for (a = 0; a < tot; a++) {
						RNA_property_collection_lookup_int(ptr, prop, a, &pptr);
						outliner_add_element(soops, &te->subtree, (void *)&pptr, te, TSE_RNA_STRUCT, a);
					}
				}
				else if (tot)
					te->flag |= TE_LAZY_CLOSED;
			}
			else if (ELEM(proptype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT)) {
				tot = RNA_property_array_length(ptr, prop);
				CLAMP_MAX(tot, tot_limit);

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
				const char *key = WM_key_event_string(kmi->type, false);
				
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

	if ((type != TSE_LAYER_COLLECTION) && (te->idcode == ID_GR)) {
		Group *group = (Group *)id;
		outliner_add_layer_collections_recursive(soops, &te->subtree, id, &group->view_layer->layer_collections, NULL, true);
	}

	return te;
}

/**
 * \note Really only removes \a tselem, not it's TreeElement instance or any children.
 */
void outliner_remove_treestore_element(SpaceOops *soops, TreeStoreElem *tselem)
{
	BKE_outliner_treehash_remove_element(soops->treehash, tselem);
	BLI_mempool_free(soops->treestore, tselem);
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

		if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name))
			return(2);
		p = p->prev;
	}

	p = seq->next;
	while (p) {
		if ((!p->strip) || (!p->strip->stripdata)) {
			p = p->next;
			continue;
		}

		if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name))
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

		if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name))
			/* ch = */ /* UNUSED */ outliner_add_element(soops, &te->subtree, (void *)p, te, TSE_SEQUENCE, index);
		p = p->next;
	}
}


/* ----------------------------------------------- */


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
				ten = outliner_add_element(soops, &te->subtree, lbarray[a], NULL, TSE_ID_BASE, 0);
				ten->directdata = lbarray[a];
				
				ten->name = BKE_idcode_to_name_plural(GS(id->name));
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

static void outliner_add_orphaned_datablocks(Main *mainvar, SpaceOops *soops)
{
	TreeElement *ten;
	ListBase *lbarray[MAX_LIBARRAY];
	int a, tot;
	
	tot = set_listbasepointers(mainvar, lbarray);
	for (a = 0; a < tot; a++) {
		if (lbarray[a]->first) {
			ID *id = lbarray[a]->first;
			
			/* check if there are any datablocks of this type which are orphans */
			for (; id; id = id->next) {
				if (ID_REAL_USERS(id) <= 0)
					break;
			}
			
			if (id) {
				/* header for this type of datablock */
				/* TODO's:
				 *   - Add a parameter to BKE_idcode_to_name_plural to get a sane "user-visible" name instead?
				 *   - Ensure that this uses nice icons for the datablock type involved instead of the dot?
				 */
				ten = outliner_add_element(soops, &soops->tree, lbarray[a], NULL, TSE_ID_BASE, 0);
				ten->directdata = lbarray[a];
				
				ten->name = BKE_idcode_to_name_plural(GS(id->name));
				if (ten->name == NULL)
					ten->name = "UNKNOWN";
				
				/* add the orphaned datablocks - these will not be added with any subtrees attached */
				for (id = lbarray[a]->first; id; id = id->next) {
					if (ID_REAL_USERS(id) <= 0)
						outliner_add_element(soops, &ten->subtree, id, ten, 0, 0);
				}
			}
		}
	}
}

static void outliner_layer_collections_reorder(
        Main *bmain,
        SpaceOops *UNUSED(soops),
        TreeElement *insert_element, TreeElement *insert_handle, TreeElementInsertType action,
        const wmEvent *UNUSED(event))
{
	LayerCollection *lc_insert = insert_element->directdata;
	LayerCollection *lc_handle = insert_handle->directdata;
	ID *id = insert_element->store_elem->id;

	if (action == TE_INSERT_BEFORE) {
		BKE_layer_collection_move_above(id, lc_handle, lc_insert);
	}
	else if (action == TE_INSERT_AFTER) {
		BKE_layer_collection_move_below(id, lc_handle, lc_insert);
	}
	else if (action == TE_INSERT_INTO) {
		BKE_layer_collection_move_into(id, lc_handle, lc_insert);
	}
	else {
		BLI_assert(0);
	}

	DEG_relations_tag_update(bmain);
}
static bool outliner_layer_collections_reorder_poll(
        const TreeElement *insert_element,
        TreeElement **io_insert_handle, TreeElementInsertType *UNUSED(io_action))
{
	const TreeStoreElem *tselem_handle = TREESTORE(*io_insert_handle);

	if (tselem_handle->id != insert_element->store_elem->id) {
        return false;
	}

	return ELEM(tselem_handle->type, TSE_LAYER_COLLECTION);
}

static void outliner_add_layer_collections_recursive(
        SpaceOops *soops, ListBase *tree, ID *id, ListBase *layer_collections, TreeElement *parent_ten,
        const bool show_objects)
{
	for (LayerCollection *collection = layer_collections->first; collection; collection = collection->next) {
		TreeElement *ten = outliner_add_element(soops, tree, id, parent_ten, TSE_LAYER_COLLECTION, 0);

		ten->name = collection->scene_collection->name;
		ten->directdata = collection;
		ten->reinsert = outliner_layer_collections_reorder;
		ten->reinsert_poll = outliner_layer_collections_reorder_poll;

		outliner_add_layer_collections_recursive(soops, &ten->subtree, id, &collection->layer_collections, ten, show_objects);
		if (show_objects) {
			for (LinkData *link = collection->object_bases.first; link; link = link->next) {
				Base *base = (Base *)link->data;
				TreeElement *te_object = outliner_add_element(soops, &ten->subtree, base->object, ten, 0, 0);
				te_object->directdata = base;
			}
		}
		outliner_make_hierarchy(&ten->subtree);
	}
}

static void outliner_add_view_layer(SpaceOops *soops, ListBase *tree, TreeElement *parent,
                                    Scene *scene, ViewLayer *layer, const bool show_objects)
{
	outliner_add_layer_collections_recursive(soops, tree, &scene->id, &layer->layer_collections, parent, show_objects);
}

static void outliner_scene_collections_reorder(
        Main *bmain,
        SpaceOops *UNUSED(soops),
        TreeElement *insert_element, TreeElement *insert_handle, TreeElementInsertType action,
        const wmEvent *UNUSED(event))
{
	SceneCollection *sc_insert = insert_element->directdata;
	SceneCollection *sc_handle = insert_handle->directdata;
	ID *id = insert_handle->store_elem->id;
	BLI_assert(id == insert_element->store_elem->id);

	BLI_assert((action == TE_INSERT_INTO) || (sc_handle != BKE_collection_master(id)));
	if (action == TE_INSERT_BEFORE) {
		BKE_collection_move_above(id, sc_handle, sc_insert);
	}
	else if (action == TE_INSERT_AFTER) {
		BKE_collection_move_below(id, sc_handle, sc_insert);
	}
	else if (action == TE_INSERT_INTO) {
		BKE_collection_move_into(id, sc_handle, sc_insert);
	}
	else {
		BLI_assert(0);
	}

	DEG_relations_tag_update(bmain);
}
static bool outliner_scene_collections_reorder_poll(
        const TreeElement *insert_element,
        TreeElement **io_insert_handle, TreeElementInsertType *io_action)
{
	const TreeStoreElem *tselem_handle = TREESTORE(*io_insert_handle);
	ID *id = tselem_handle->id;

	if (id != insert_element->store_elem->id) {
		return false;
	}

	if (!ELEM(tselem_handle->type, TSE_SCENE_COLLECTION)) {
		return false;
	}

	SceneCollection *sc_master = BKE_collection_master(id);
	SceneCollection *sc_handle = (*io_insert_handle)->directdata;

	if (sc_handle == sc_master) {
		/* exception: Can't insert before/after master selection, has to be one of its childs */
		TreeElement *te_master = *io_insert_handle;
		if (*io_action == TE_INSERT_BEFORE) {
			/* can't go higher than master collection, insert into it */
			*io_action = TE_INSERT_INTO;
		}
		else if (*io_action == TE_INSERT_AFTER) {
			*io_insert_handle = te_master->subtree.last;
		}
	}
	return true;
}

BLI_INLINE void outliner_add_scene_collection_init(TreeElement *te, SceneCollection *collection)
{
	te->name = collection->name;
	te->directdata = collection;
	te->reinsert = outliner_scene_collections_reorder;
	te->reinsert_poll = outliner_scene_collections_reorder_poll;
}

BLI_INLINE void outliner_add_scene_collection_objects(
        SpaceOops *soops, ListBase *tree, SceneCollection *collection, TreeElement *parent)
{
	for (LinkData *link = collection->objects.first; link; link = link->next) {
		outliner_add_element(soops, tree, link->data, parent, 0, 0);
	}
}

static TreeElement *outliner_add_scene_collection_recursive(
        SpaceOops *soops, ListBase *tree, ID *id, SceneCollection *scene_collection, TreeElement *parent_ten)
{
	TreeElement *ten = outliner_add_element(soops, tree, id, parent_ten, TSE_SCENE_COLLECTION, 0);
	outliner_add_scene_collection_init(ten, scene_collection);
	outliner_add_scene_collection_objects(soops, &ten->subtree, scene_collection, ten);

	for (SceneCollection *scene_collection_nested = scene_collection->scene_collections.first;
	     scene_collection_nested != NULL;
	     scene_collection_nested = scene_collection_nested->next)
	{
		outliner_add_scene_collection_recursive(soops, &ten->subtree, id, scene_collection_nested, ten);
	}

	outliner_make_hierarchy(&ten->subtree);
	return ten;
}

static void outliner_add_collections(SpaceOops *soops, Scene *scene)
{
	SceneCollection *master_collection = BKE_collection_master(&scene->id);
	TreeElement *ten = outliner_add_scene_collection_recursive(soops, &soops->tree, &scene->id, master_collection, NULL);
	/* Master Collection should always be expanded. */
	TREESTORE(ten)->flag &= ~TSE_CLOSED;
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
static void outliner_sort(ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;

	te = lb->last;
	if (te == NULL) return;
	tselem = TREESTORE(te);

	/* sorting rules; only object lists, ID lists, or deformgroups */
	if (ELEM(tselem->type, TSE_DEFGROUP, TSE_ID_BASE) || (tselem->type == 0 && te->idcode == ID_OB)) {
		int totelem = BLI_listbase_count(lb);

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
		outliner_sort(&te->subtree);
	}
}

/* Filtering ----------------------------------------------- */

typedef struct OutlinerTreeElementFocus {
	TreeStoreElem *tselem;
	int ys;
} OutlinerTreeElementFocus;

/**
 * Bring the outliner scrolling back to where it was in relation to the original focus element
 * Caller is expected to handle redrawing of ARegion.
 */
static void outliner_restore_scrolling_position(SpaceOops *soops, ARegion *ar, OutlinerTreeElementFocus *focus)
{
	View2D *v2d = &ar->v2d;
	int ytop;

	if (focus->tselem != NULL) {
		outliner_set_coordinates(ar, soops);

		TreeElement *te_new = outliner_find_tree_element(&soops->tree, focus->tselem);

		if (te_new != NULL) {
			int ys_new, ys_old;

			ys_new = te_new->ys;
			ys_old = focus->ys;

			ytop = v2d->cur.ymax + (ys_new - ys_old) -1;
			if (ytop > 0) ytop = 0;

			v2d->cur.ymax = (float)ytop;
			v2d->cur.ymin = (float)(ytop - BLI_rcti_size_y(&v2d->mask));
		}
		else {
			return;
		}

		soops->storeflag |= SO_TREESTORE_REDRAW;
	}
}

static bool test_collection_callback(TreeElement *te)
{
	TreeStoreElem *tselem = TREESTORE(te);
	return ELEM(tselem->type, TSE_LAYER_COLLECTION, TSE_SCENE_COLLECTION);
}

static bool test_object_callback(TreeElement *te)
{
	TreeStoreElem *tselem = TREESTORE(te);
	return ((tselem->type == 0) && (te->idcode == ID_OB));
}

/**
 * See if TreeElement or any of its children pass the callback_test.
 */
static TreeElement *outliner_find_first_desired_element_at_y_recursive(
        const SpaceOops *soops,
        TreeElement *te,
        const float limit,
        bool (*callback_test)(TreeElement *))
{
	if (callback_test(te)) {
		return te;
	}

	if (TSELEM_OPEN(te->store_elem, soops)) {
		TreeElement *te_iter, *te_sub;
		for (te_iter = te->subtree.first; te_iter; te_iter = te_iter->next) {
			te_sub = outliner_find_first_desired_element_at_y_recursive(soops, te_iter, limit, callback_test);
			if (te_sub != NULL) {
				return te_sub;
			}
		}
	}

	return NULL;
}

/**
 * Find the first element that passes a test starting from a reference vertical coordinate
 *
 * If the element that is in the position is not what we are looking for, keep looking for its
 * children, siblings, and eventually, aunts, cousins, disntant families, ...
 *
 * Basically we keep going up and down the outliner tree from that point forward, until we find
 * what we are looking for. If we are past the visible range and we can't find a valid element
 * we return NULL.
 */
static TreeElement *outliner_find_first_desired_element_at_y(
        const SpaceOops *soops,
        const float view_co,
        const float view_co_limit)
{
	TreeElement *te, *te_sub;
	te = outliner_find_item_at_y(soops, &soops->tree, view_co);

	bool (*callback_test)(TreeElement *);
	if (soops->filter & SO_FILTER_NO_COLLECTION) {
		callback_test = test_object_callback;
	}
	else {
		callback_test = test_collection_callback;
	}

	while (te != NULL) {
		te_sub = outliner_find_first_desired_element_at_y_recursive(soops, te, view_co_limit, callback_test);
		if (te_sub != NULL) {
			/* Skip the element if it was not visible to start with. */
			if (te->ys + UI_UNIT_Y > view_co_limit) {
				return te_sub;
			}
			else {
				return NULL;
			}
		}

		if (te->next) {
			te = te->next;
			continue;
		}

		if (te->parent == NULL) {
			break;
		}

		while (te->parent) {
			if (te->parent->next) {
				te = te->parent->next;
				break;
			}
			te = te->parent;
		}
	}

	return NULL;
}

/**
 * Store information of current outliner scrolling status to be restored later
 *
 * Finds the top-most collection visible in the outliner and populates the OutlinerTreeElementFocus
 * struct to retrieve this element later to make sure it is in the same original position as before filtering
 */
static void outliner_store_scrolling_position(SpaceOops *soops, ARegion *ar, OutlinerTreeElementFocus *focus)
{
	TreeElement *te;
	float limit = ar->v2d.cur.ymin;

	outliner_set_coordinates(ar, soops);

	te = outliner_find_first_desired_element_at_y(soops, ar->v2d.cur.ymax, limit);

	if (te != NULL) {
		focus->tselem = TREESTORE(te);
		focus->ys = te->ys;
	}
	else {
		focus->tselem = NULL;
	}
}

static int outliner_exclude_filter_get(SpaceOops *soops)
{
	int exclude_filter = soops->filter & ~(SO_FILTER_OB_STATE_VISIBLE |
	                                       SO_FILTER_OB_STATE_SELECTED |
	                                       SO_FILTER_OB_STATE_ACTIVE);

	if (soops->filter & SO_FILTER_SEARCH) {
		if (soops->search_string[0] == 0) {
			exclude_filter &= ~SO_FILTER_SEARCH;
		}
	}

	/* Let's have this for the collection options at first. */
	if (!SUPPORT_FILTER_OUTLINER(soops)) {
		return (exclude_filter & SO_FILTER_SEARCH);
	}

	if ((exclude_filter & SO_FILTER_NO_OB_ALL) == 0) {
		exclude_filter &= ~SO_FILTER_OB_TYPE;
	}

	if (exclude_filter & SO_FILTER_OB_STATE) {
		switch (soops->filter_state) {
			case SO_FILTER_OB_VISIBLE:
				exclude_filter |= SO_FILTER_OB_STATE_VISIBLE;
				break;
			case SO_FILTER_OB_SELECTED:
				exclude_filter |= SO_FILTER_OB_STATE_SELECTED;
				break;
			case SO_FILTER_OB_ACTIVE:
				exclude_filter |= SO_FILTER_OB_STATE_ACTIVE;
				break;
		}
	}

	if ((exclude_filter & SO_FILTER_ANY) == 0) {
		exclude_filter &= ~(SO_FILTER_OB_STATE);
	}

	return exclude_filter;
}

static bool outliner_element_visible_get(ViewLayer *view_layer, TreeElement *te, const int exclude_filter)
{
	if ((exclude_filter & SO_FILTER_ENABLE) == 0) {
		return true;
	}

	TreeStoreElem *tselem = TREESTORE(te);
	if ((tselem->type == 0) && (te->idcode == ID_OB)) {
		if ((exclude_filter & SO_FILTER_NO_OBJECT)) {
			return false;
		}

		Object *ob = (Object *)tselem->id;
		Base *base = (Base *)te->directdata;
		BLI_assert((base == NULL) || (base->object == ob));

		if (exclude_filter & SO_FILTER_OB_TYPE) {
			switch (ob->type) {
				case OB_MESH:
					if (exclude_filter & SO_FILTER_NO_OB_MESH) {
						return false;
					}
					break;
				case OB_ARMATURE:
					if (exclude_filter & SO_FILTER_NO_OB_ARMATURE) {
						return false;
					}
					break;
				case OB_EMPTY:
					if (exclude_filter & SO_FILTER_NO_OB_EMPTY) {
						return false;
					}
					break;
				case OB_LAMP:
					if (exclude_filter & SO_FILTER_NO_OB_LAMP) {
						return false;
					}
					break;
				case OB_CAMERA:
					if (exclude_filter & SO_FILTER_NO_OB_CAMERA) {
						return false;
					}
					break;
				default:
					if (exclude_filter & SO_FILTER_NO_OB_OTHERS) {
						return false;
					}
					break;
			}
		}

		if (exclude_filter & SO_FILTER_OB_STATE) {
			if (base == NULL) {
				base = BKE_view_layer_base_find(view_layer, ob);

				if (base == NULL) {
					return false;
				}
			}

			if (exclude_filter & SO_FILTER_OB_STATE_VISIBLE) {
				if ((base->flag & BASE_VISIBLED) == 0) {
					return false;
				}
			}
			else if (exclude_filter & SO_FILTER_OB_STATE_SELECTED) {
				if ((base->flag & BASE_SELECTED) == 0) {
					return false;
				}
			}
			else {
				BLI_assert(exclude_filter & SO_FILTER_OB_STATE_ACTIVE);
				if (base != BASACT(view_layer)) {
					return false;
				}
			}
		}

		if ((te->parent != NULL) &&
		    (TREESTORE(te->parent)->type == 0) && (te->parent->idcode == ID_OB))
		{
			if (exclude_filter & SO_FILTER_NO_CHILDREN) {
				return false;
			}
		}
	}
	else if (te->parent != NULL &&
	    TREESTORE(te->parent)->type == 0 && te->parent->idcode == ID_OB)
	{
		if (exclude_filter & SO_FILTER_NO_OB_CONTENT) {
			return false;
		}
	}

	return true;
}

static bool outliner_filter_has_name(TreeElement *te, const char *name, int flags)
{
	int fn_flag = 0;

	if ((flags & SO_FIND_CASE_SENSITIVE) == 0)
		fn_flag |= FNM_CASEFOLD;

	return fnmatch(name, te->name, fn_flag) == 0;
}

static int outliner_filter_subtree(
        SpaceOops *soops, ViewLayer *view_layer, ListBase *lb, const char *search_string, const int exclude_filter)
{
	TreeElement *te, *te_next;
	TreeStoreElem *tselem;

	for (te = lb->first; te; te = te_next) {
		te_next = te->next;

		if ((outliner_element_visible_get(view_layer, te, exclude_filter) == false)) {
			outliner_free_tree_element(te, lb);
			continue;
		}
		else if ((exclude_filter & SO_FILTER_SEARCH) == 0) {
			/* Filter subtree too. */
			outliner_filter_subtree(soops, view_layer, &te->subtree, search_string, exclude_filter);
			continue;
		}

		if (!outliner_filter_has_name(te, search_string, soops->search_flags)) {
			/* item isn't something we're looking for, but...
			 *  - if the subtree is expanded, check if there are any matches that can be easily found
			 *		so that searching for "cu" in the default scene will still match the Cube
			 *	- otherwise, we can't see within the subtree and the item doesn't match,
			 *		so these can be safely ignored (i.e. the subtree can get freed)
			 */
			tselem = TREESTORE(te);

			/* flag as not a found item */
			tselem->flag &= ~TSE_SEARCHMATCH;
			
			if ((!TSELEM_OPEN(tselem, soops)) ||
			    outliner_filter_subtree(soops, view_layer, &te->subtree, search_string, exclude_filter) == 0)
			{
				outliner_free_tree_element(te, lb);
			}
		}
		else {
			tselem = TREESTORE(te);

			/* flag as a found item - we can then highlight it */
			tselem->flag |= TSE_SEARCHMATCH;

			/* filter subtree too */
			outliner_filter_subtree(soops, view_layer, &te->subtree, search_string, exclude_filter);
		}
	}

	/* if there are still items in the list, that means that there were still some matches */
	return (BLI_listbase_is_empty(lb) == false);
}

static void outliner_filter_tree(SpaceOops *soops, ViewLayer *view_layer)
{
	char search_buff[sizeof(((struct SpaceOops *)NULL)->search_string) + 2];
	char *search_string;

	const int exclude_filter = outliner_exclude_filter_get(soops);

	if (exclude_filter == 0) {
		return;
	}

	if (soops->search_flags & SO_FIND_COMPLETE) {
		search_string = soops->search_string;
	}
	else {
		/* Implicitly add heading/trailing wildcards if needed. */
		BLI_strncpy_ensure_pad(search_buff, soops->search_string, '*', sizeof(search_buff));
		search_string = search_buff;
	}

	outliner_filter_subtree(soops, view_layer, &soops->tree, search_string, exclude_filter);
}

/* ======================================================= */
/* Main Tree Building API */

/* Main entry point for building the tree data-structure that the outliner represents */
// TODO: split each mode into its own function?
void outliner_build_tree(Main *mainvar, Scene *scene, ViewLayer *view_layer, SpaceOops *soops, ARegion *ar)
{
	TreeElement *te = NULL, *ten;
	TreeStoreElem *tselem;
	int show_opened = !soops->treestore || !BLI_mempool_len(soops->treestore); /* on first view, we open scenes */

	/* Are we looking for something - we want to tag parents to filter child matches
	 * - NOT in datablocks view - searching all datablocks takes way too long to be useful
	 * - this variable is only set once per tree build */
	if (soops->search_string[0] != 0 && soops->outlinevis != SO_DATABLOCKS)
		soops->search_flags |= SO_SEARCH_RECURSIVE;
	else
		soops->search_flags &= ~SO_SEARCH_RECURSIVE;

	if (soops->treehash && (soops->storeflag & SO_TREESTORE_REBUILD)) {
		soops->storeflag &= ~SO_TREESTORE_REBUILD;
		BKE_outliner_treehash_rebuild_from_treestore(soops->treehash, soops->treestore);
	}

	if (soops->tree.first && (soops->storeflag & SO_TREESTORE_REDRAW))
		return;

	OutlinerTreeElementFocus focus;
	outliner_store_scrolling_position(soops, ar, &focus);

	outliner_free_tree(&soops->tree);
	outliner_storage_cleanup(soops);
	
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
				par = (TreeElement *)lib->parent->id.newid;
				if (tselem->id->tag & LIB_TAG_INDIRECT) {
					/* Only remove from 'first level' if lib is not also directly used. */
					BLI_remlink(&soops->tree, ten);
					BLI_addtail(&par->subtree, ten);
					ten->parent = par;
				}
				else {
					/* Else, make a new copy of the libtree for our parent. */
					TreeElement *dupten = outliner_add_element(soops, &par->subtree, lib, NULL, 0, 0);
					outliner_add_library_contents(mainvar, soops, dupten, lib);
					dupten->parent = par;
				}
			}
			ten = nten;
		}
		/* restore newid pointers */
		for (lib = mainvar->library.first; lib; lib = lib->id.next)
			lib->id.newid = NULL;
		
	}
	else if (soops->outlinevis == SO_SCENES) {
		Scene *sce;
		for (sce = mainvar->scene.first; sce; sce = sce->id.next) {
			te = outliner_add_element(soops, &soops->tree, sce, NULL, 0, 0);
			tselem = TREESTORE(te);

			if (sce == scene && show_opened) {
				tselem->flag &= ~TSE_CLOSED;
			}

			outliner_make_hierarchy(&te->subtree);
		}
	}
	else if (soops->outlinevis == SO_GROUPS) {
		Group *group;
		for (group = mainvar->group.first; group; group = group->id.next) {
			te = outliner_add_element(soops, &soops->tree, group, NULL, 0, 0);
			outliner_make_hierarchy(&te->subtree);
		}
	}
	else if (soops->outlinevis == SO_SEQUENCE) {
		Sequence *seq;
		Editing *ed = BKE_sequencer_editing_get(scene, false);
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
	else if (soops->outlinevis == SO_ID_ORPHANS) {
		outliner_add_orphaned_datablocks(mainvar, soops);
	}
	else if (soops->outlinevis == SO_VIEW_LAYER) {
		if ((soops->filter & SO_FILTER_ENABLE) && (soops->filter & SO_FILTER_NO_COLLECTION)) {
			for (Base *base = view_layer->object_bases.first; base; base = base->next) {
				TreeElement *te_object = outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
				te_object->directdata = base;
			}
			outliner_make_hierarchy(&soops->tree);
		}
		else {
			outliner_add_view_layer(soops, &soops->tree, NULL, scene, view_layer, true);
		}
	}
	else if (soops->outlinevis == SO_COLLECTIONS) {
		if ((soops->filter & SO_FILTER_ENABLE) && (soops->filter & SO_FILTER_NO_COLLECTION)) {
			FOREACH_SCENE_OBJECT_BEGIN(scene, ob)
			{
				outliner_add_element(soops, &soops->tree, ob, NULL, 0, 0);
			}
			FOREACH_SCENE_OBJECT_END;
			outliner_make_hierarchy(&soops->tree);
		}
		else {
			outliner_add_collections(soops, scene);
		}
	}
	else {
		if (BASACT(view_layer)) {
			ten = outliner_add_element(soops, &soops->tree, OBACT(view_layer), NULL, 0, 0);
			ten->directdata = BASACT(view_layer);
		}
	}

	if ((soops->flag & SO_SKIP_SORT_ALPHA) == 0) {
		outliner_sort(&soops->tree);
	}

	outliner_filter_tree(soops, view_layer);
	outliner_restore_scrolling_position(soops, ar, &focus);

	BKE_main_id_clear_newpoins(mainvar);
}


