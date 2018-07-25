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
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/versioning_280.c
 *  \ingroup blenloader
 */

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <string.h>
#include <float.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpu_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_layer_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_genfile.h"
#include "DNA_workspace_types.h"

#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_customdata.h"
#include "BKE_freestyle.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_studiolight.h"
#include "BKE_workspace.h"

#include "BLO_readfile.h"
#include "readfile.h"

#include "MEM_guardedalloc.h"

static bScreen *screen_parent_find(const bScreen *screen)
{
	/* can avoid lookup if screen state isn't maximized/full (parent and child store the same state) */
	if (ELEM(screen->state, SCREENMAXIMIZED, SCREENFULL)) {
		for (const ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
			if (sa->full && sa->full != screen) {
				BLI_assert(sa->full->state == screen->state);
				return sa->full;
			}
		}
	}

	return NULL;
}

static void do_version_workspaces_create_from_screens(Main *bmain)
{
	for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
		const bScreen *screen_parent = screen_parent_find(screen);
		WorkSpace *workspace;
		if (screen->temp) {
			continue;
		}

		if (screen_parent) {
			/* fullscreen with "Back to Previous" option, don't create
			 * a new workspace, add layout workspace containing parent */
			workspace = BLI_findstring(
			        &bmain->workspaces, screen_parent->id.name + 2, offsetof(ID, name) + 2);
		}
		else {
			workspace = BKE_workspace_add(bmain, screen->id.name + 2);
		}
		BKE_workspace_layout_add(bmain, workspace, screen, screen->id.name + 2);
	}
}

static void do_version_area_change_space_to_space_action(ScrArea *area, const Scene *scene)
{
	SpaceType *stype = BKE_spacetype_from_id(SPACE_ACTION);
	SpaceAction *saction = (SpaceAction *)stype->new(area, scene);
	ARegion *region_channels;

	/* Properly free current regions */
	for (ARegion *region = area->regionbase.first; region; region = region->next) {
		BKE_area_region_free(area->type, region);
	}
	BLI_freelistN(&area->regionbase);

	area->type = stype;
	area->spacetype = stype->spaceid;

	BLI_addhead(&area->spacedata, saction);
	area->regionbase = saction->regionbase;
	BLI_listbase_clear(&saction->regionbase);

	/* Different defaults for timeline */
	region_channels = BKE_area_find_region_type(area, RGN_TYPE_CHANNELS);
	region_channels->flag |= RGN_FLAG_HIDDEN;

	saction->mode = SACTCONT_TIMELINE;
	saction->ads.flag |= ADS_FLAG_SUMMARY_COLLAPSED;
	saction->ads.filterflag |= ADS_FILTER_SUMMARY;
}

/**
 * \brief After lib-link versioning for new workspace design.
 *
 * - Adds a workspace for (almost) each screen of the old file
 *   and adds the needed workspace-layout to wrap the screen.
 * - Active screen isn't stored directly in window anymore, but in the active workspace.
 * - Active scene isn't stored in screen anymore, but in window.
 * - Create workspace instance hook for each window.
 *
 * \note Some of the created workspaces might be deleted again in case of reading the default startup.blend.
 */
static void do_version_workspaces_after_lib_link(Main *bmain)
{
	BLI_assert(BLI_listbase_is_empty(&bmain->workspaces));

	do_version_workspaces_create_from_screens(bmain);

	for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
		for (wmWindow *win = wm->windows.first; win; win = win->next) {
			bScreen *screen_parent = screen_parent_find(win->screen);
			bScreen *screen = screen_parent ? screen_parent : win->screen;

			if (screen->temp) {
				/* We do not generate a new workspace for those screens... still need to set some data in win. */
				win->workspace_hook = BKE_workspace_instance_hook_create(bmain);
				win->scene = screen->scene;
				/* Deprecated from now on! */
				win->screen = NULL;
				continue;
			}

			WorkSpace *workspace = BLI_findstring(&bmain->workspaces, screen->id.name + 2, offsetof(ID, name) + 2);
			BLI_assert(workspace != NULL);
			ListBase *layouts = BKE_workspace_layouts_get(workspace);

			win->workspace_hook = BKE_workspace_instance_hook_create(bmain);

			BKE_workspace_active_set(win->workspace_hook, workspace);
			BKE_workspace_active_layout_set(win->workspace_hook, layouts->first);

			/* Move scene and view layer to window. */
			Scene *scene = screen->scene;
			ViewLayer *layer = BLI_findlink(&scene->view_layers, scene->r.actlay);
			if (!layer) {
				layer = BKE_view_layer_default_view(scene);
			}

			win->scene = scene;
			STRNCPY(win->view_layer_name, layer->name);

			/* Deprecated from now on! */
			win->screen = NULL;
		}
	}

	for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
		/* Deprecated from now on! */
		BLI_freelistN(&screen->scene->transform_spaces);
		screen->scene = NULL;
	}
}

#ifdef USE_COLLECTION_COMPAT_28
enum {
	COLLECTION_DEPRECATED_VISIBLE    = (1 << 0),
	COLLECTION_DEPRECATED_VIEWPORT   = (1 << 0),
	COLLECTION_DEPRECATED_SELECTABLE = (1 << 1),
	COLLECTION_DEPRECATED_DISABLED   = (1 << 2),
	COLLECTION_DEPRECATED_RENDER     = (1 << 3),
};

static void do_version_view_layer_visibility(ViewLayer *view_layer)
{
	/* Convert from deprecated VISIBLE flag to DISABLED */
	LayerCollection *lc;
	for (lc = view_layer->layer_collections.first;
	     lc;
	     lc = lc->next)
	{
		if (lc->flag & COLLECTION_DEPRECATED_DISABLED) {
			lc->flag &= ~COLLECTION_DEPRECATED_DISABLED;
		}

		if ((lc->flag & COLLECTION_DEPRECATED_VISIBLE) == 0) {
			lc->flag |= COLLECTION_DEPRECATED_DISABLED;
		}

		lc->flag |= COLLECTION_DEPRECATED_VIEWPORT | COLLECTION_DEPRECATED_RENDER;
	}
}

static void do_version_layer_collection_pre(
        ViewLayer *view_layer,
        ListBase *lb,
        GSet *enabled_set,
        GSet *selectable_set)
{
	/* Convert from deprecated DISABLED to new layer collection and collection flags */
	for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
		if (lc->scene_collection) {
			if (!(lc->flag & COLLECTION_DEPRECATED_DISABLED)) {
				BLI_gset_insert(enabled_set, lc->scene_collection);
			}
			if (lc->flag & COLLECTION_DEPRECATED_SELECTABLE) {
				BLI_gset_insert(selectable_set, lc->scene_collection);
			}
		}

		do_version_layer_collection_pre(view_layer, &lc->layer_collections, enabled_set, selectable_set);
	}
}

static void do_version_layer_collection_post(
        ViewLayer *view_layer,
        ListBase *lb,
        GSet *enabled_set,
        GSet *selectable_set,
        GHash *collection_map)
{
	/* Apply layer collection exclude flags. */
	for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
		if (!(lc->collection->flag & COLLECTION_IS_MASTER)) {
			SceneCollection *sc = BLI_ghash_lookup(collection_map, lc->collection);
			const bool enabled = (sc && BLI_gset_haskey(enabled_set, sc));
			const bool selectable = (sc && BLI_gset_haskey(selectable_set, sc));

			if (!enabled) {
				lc->flag |= LAYER_COLLECTION_EXCLUDE;
			}
			if (enabled && !selectable) {
				lc->collection->flag |= COLLECTION_RESTRICT_SELECT;
			}
		}

		do_version_layer_collection_post(
		        view_layer, &lc->layer_collections, enabled_set, selectable_set, collection_map);
	}
}

static void do_version_scene_collection_convert(
        Main *bmain,
        ID *id,
        SceneCollection *sc,
        Collection *collection,
        GHash *collection_map)
{
	if (collection_map) {
		BLI_ghash_insert(collection_map, collection, sc);
	}

	for (SceneCollection *nsc = sc->scene_collections.first; nsc;) {
		SceneCollection *nsc_next = nsc->next;
		Collection *ncollection = BKE_collection_add(bmain, collection, nsc->name);
		ncollection->id.lib = id->lib;
		do_version_scene_collection_convert(bmain, id, nsc, ncollection, collection_map);
		nsc = nsc_next;
	}

	for (LinkData *link = sc->objects.first; link; link = link->next) {
		Object *ob = link->data;
		if (ob) {
			BKE_collection_object_add(bmain, collection, ob);
			id_us_min(&ob->id);
		}
	}

	BLI_freelistN(&sc->objects);
	MEM_freeN(sc);
}

static void do_version_group_collection_to_collection(Main *bmain, Collection *group)
{
	/* Convert old 2.8 group collections to new unified collections. */
	if (group->collection) {
		do_version_scene_collection_convert(bmain, &group->id, group->collection, group, NULL);
	}

	group->collection = NULL;
	group->view_layer = NULL;
	id_fake_user_set(&group->id);
}

static void do_version_scene_collection_to_collection(Main *bmain, Scene *scene)
{
	/* Convert old 2.8 scene collections to new unified collections. */

	/* Temporarily clear view layers so we don't do any layer collection syncing
	 * and destroy old flags that we want to restore. */
	ListBase view_layers = scene->view_layers;
	BLI_listbase_clear(&scene->view_layers);

	if (!scene->master_collection) {
		scene->master_collection = BKE_collection_master_add();
	}

	/* Convert scene collections. */
	GHash *collection_map = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
	if (scene->collection) {
		do_version_scene_collection_convert(bmain, &scene->id, scene->collection, scene->master_collection, collection_map);
		scene->collection = NULL;
	}

	scene->view_layers = view_layers;

	/* Convert layer collections. */
	ViewLayer *view_layer;
	for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		GSet *enabled_set = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
		GSet *selectable_set = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);

		do_version_layer_collection_pre(
		        view_layer, &view_layer->layer_collections, enabled_set, selectable_set);

		BKE_layer_collection_sync(scene, view_layer);

		do_version_layer_collection_post(
		        view_layer, &view_layer->layer_collections, enabled_set, selectable_set, collection_map);

		BLI_gset_free(enabled_set, NULL);
		BLI_gset_free(selectable_set, NULL);

		BKE_layer_collection_sync(scene, view_layer);
	}

	BLI_ghash_free(collection_map, NULL, NULL);
}
#endif


static void do_version_layers_to_collections(Main *bmain, Scene *scene)
{
	/* Since we don't have access to FileData we check the (always valid) first
	 * render layer instead. */
	if (!scene->master_collection) {
		scene->master_collection = BKE_collection_master_add();
	}

	if (scene->view_layers.first) {
		return;
	}

	/* Create collections from layers. */
	Collection *collection_master = BKE_collection_master(scene);
	Collection *collections[20] = {NULL};

	for (int layer = 0; layer < 20; layer++) {
		for (Base *base = scene->base.first; base; base = base->next) {
			if (base->lay & (1 << layer)) {
				/* Create collections when needed only. */
				if (collections[layer] == NULL) {
					char name[MAX_NAME];

					BLI_snprintf(name,
					             sizeof(collection_master->id.name),
					             "Collection %d",
					             layer + 1);

					Collection *collection = BKE_collection_add(bmain, collection_master, name);
					collection->id.lib = scene->id.lib;
					collections[layer] = collection;

					if (!(scene->lay & (1 << layer))) {
						collection->flag |= COLLECTION_RESTRICT_VIEW | COLLECTION_RESTRICT_RENDER;
					}
				}

				/* Note usually this would do slow collection syncing for view layers,
				 * but since no view layers exists yet at this point it's fast. */
				BKE_collection_object_add(
				        bmain,
				        collections[layer], base->object);
			}

			if (base->flag & SELECT) {
				base->object->flag |= SELECT;
			}
			else {
				base->object->flag &= ~SELECT;
			}
		}
	}

	/* Handle legacy render layers. */
	bool have_override = false;

	for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next) {
		ViewLayer *view_layer = BKE_view_layer_add(scene, srl->name);

		if (srl->samples != 0) {
			have_override = true;

			/* It is up to the external engine to handle
			 * its own doversion in this case. */
			BKE_override_view_layer_int_add(
			        view_layer,
			        ID_SCE,
			        "samples",
			        srl->samples);
		}

		if (srl->mat_override) {
			have_override = true;

			BKE_override_view_layer_datablock_add(
			        view_layer,
			        ID_MA,
			        "self",
			        (ID *)srl->mat_override);
		}

		if (srl->layflag & SCE_LAY_DISABLE) {
			view_layer->flag &= ~VIEW_LAYER_RENDER;
		}

		if ((srl->layflag & SCE_LAY_FRS) == 0) {
			view_layer->flag &= ~VIEW_LAYER_FREESTYLE;
		}

		/* XXX If we are to keep layflag it should be merged with flag (dfelinto). */
		view_layer->layflag = srl->layflag;
		/* XXX Not sure if we should keep the passes (dfelinto). */
		view_layer->passflag = srl->passflag;
		view_layer->pass_xor = srl->pass_xor;
		view_layer->pass_alpha_threshold = srl->pass_alpha_threshold;

		BKE_freestyle_config_free(&view_layer->freestyle_config, true);
		view_layer->freestyle_config = srl->freestyleConfig;
		view_layer->id_properties = srl->prop;

		/* Set exclusion and overrides. */
		for (int layer = 0; layer < 20; layer++) {
			Collection *collection = collections[layer];
			if (collection) {
				LayerCollection *lc = BKE_layer_collection_first_from_scene_collection(view_layer, collection);

				if (srl->lay_exclude & (1 << layer)) {
					/* Disable excluded layer. */
					have_override = true;
					lc->flag |= LAYER_COLLECTION_EXCLUDE;
					for (LayerCollection *nlc = lc->layer_collections.first; nlc; nlc = nlc->next) {
						nlc->flag |= LAYER_COLLECTION_EXCLUDE;
					}
				}
				else if ((scene->lay & srl->lay & ~(srl->lay_exclude) & (1 << layer)) ||
				         (srl->lay_zmask & (scene->lay | srl->lay_exclude) & (1 << layer)))
				{
					if (srl->lay_zmask & (1 << layer)) {
						have_override = true;
						lc->flag |= LAYER_COLLECTION_HOLDOUT;

						BKE_override_layer_collection_boolean_add(
						        lc,
						        ID_OB,
						        "cycles.is_holdout",
						        true);
					}

					if ((srl->lay & (1 << layer)) == 0) {
						have_override = true;

						BKE_override_layer_collection_boolean_add(
						        lc,
						        ID_OB,
						        "cycles_visibility.camera",
						        false);
					}
				}
			}
		}

		/* for convenience set the same active object in all the layers */
		if (scene->basact) {
			view_layer->basact = BKE_view_layer_base_find(view_layer, scene->basact->object);
		}

		for (Base *base = view_layer->object_bases.first; base; base = base->next) {
			if ((base->flag & BASE_SELECTABLE) && (base->object->flag & SELECT)) {
				base->flag |= BASE_SELECTED;
			}
		}
	}

	BLI_freelistN(&scene->r.layers);

	/* If render layers included overrides, we also create a vanilla
	 * viewport layer without them. */
	if (have_override) {
		ViewLayer *view_layer = BKE_view_layer_add(scene, "Viewport");

		/* Make it first in the list. */
		BLI_remlink(&scene->view_layers, view_layer);
		BLI_addhead(&scene->view_layers, view_layer);

		/* If we ported all the original render layers, we don't need to make the viewport layer renderable. */
		if (!BLI_listbase_is_single(&scene->view_layers)) {
			view_layer->flag &= ~VIEW_LAYER_RENDER;
		}

		/* convert active base */
		if (scene->basact) {
			view_layer->basact = BKE_view_layer_base_find(view_layer, scene->basact->object);
		}

		/* convert selected bases */
		for (Base *base = view_layer->object_bases.first; base; base = base->next) {
			if ((base->flag & BASE_SELECTABLE) && (base->object->flag & SELECT)) {
				base->flag |= BASE_SELECTED;
			}

			/* keep lay around for forward compatibility (open those files in 2.79) */
			base->lay = base->object->lay;
		}
	}

	/* remove bases once and for all */
	for (Base *base = scene->base.first; base; base = base->next) {
		id_us_min(&base->object->id);
	}

	BLI_freelistN(&scene->base);
	scene->basact = NULL;
}

void do_versions_after_linking_280(Main *bmain)
{
	bool use_collection_compat_28 = true;

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
		use_collection_compat_28 = false;

		/* Convert group layer visibility flags to hidden nested collection. */
		for (Collection *collection = bmain->collection.first; collection; collection = collection->id.next) {
			/* Add fake user for all existing groups. */
			id_fake_user_set(&collection->id);

			if (collection->flag & (COLLECTION_RESTRICT_VIEW | COLLECTION_RESTRICT_RENDER)) {
				continue;
			}

			Collection *collection_hidden = NULL;
			for (CollectionObject *cob = collection->gobject.first, *cob_next = NULL; cob; cob = cob_next) {
				cob_next = cob->next;
				Object *ob = cob->ob;

				if (!(ob->lay & collection->layer)) {
					if (collection_hidden == NULL) {
						collection_hidden = BKE_collection_add(bmain, collection, "Hidden");
						collection_hidden->id.lib = collection->id.lib;
						collection_hidden->flag |= COLLECTION_RESTRICT_VIEW | COLLECTION_RESTRICT_RENDER;
					}

					BKE_collection_object_add(bmain, collection_hidden, ob);
					BKE_collection_object_remove(bmain, collection, ob, true);
				}
			}
		}

		/* Convert layers to collections. */
		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			do_version_layers_to_collections(bmain, scene);
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
		for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
			/* same render-layer as do_version_workspaces_after_lib_link will activate,
			 * so same layer as BKE_view_layer_default_view would return */
			ViewLayer *layer = screen->scene->view_layers.first;

			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				for (SpaceLink *space = sa->spacedata.first; space; space = space->next) {
					if (space->spacetype == SPACE_OUTLINER) {
						SpaceOops *soutliner = (SpaceOops *)space;

						soutliner->outlinevis = SO_VIEW_LAYER;

						if (BLI_listbase_count_at_most(&layer->layer_collections, 2) == 1) {
							if (soutliner->treestore == NULL) {
								soutliner->treestore = BLI_mempool_create(
								        sizeof(TreeStoreElem), 1, 512, BLI_MEMPOOL_ALLOW_ITER);
							}

							/* Create a tree store element for the collection. This is normally
							 * done in check_persistent (outliner_tree.c), but we need to access
							 * it here :/ (expand element if it's the only one) */
							TreeStoreElem *tselem = BLI_mempool_calloc(soutliner->treestore);
							tselem->type = TSE_LAYER_COLLECTION;
							tselem->id = layer->layer_collections.first;
							tselem->nr = tselem->used = 0;
							tselem->flag &= ~TSE_CLOSED;
						}
					}
				}
			}
		}
	}

	/* New workspace design */
	if (!MAIN_VERSION_ATLEAST(bmain, 280, 1)) {
		do_version_workspaces_after_lib_link(bmain);
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 2)) {
		/* Cleanup any remaining SceneRenderLayer data for files that were created
		 * with Blender 2.8 before the SceneRenderLayer > RenderLayer refactor. */
		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next) {
				if (srl->prop) {
					IDP_FreeProperty(srl->prop);
					MEM_freeN(srl->prop);
				}
				BKE_freestyle_config_free(&srl->freestyleConfig, true);
			}
			BLI_freelistN(&scene->r.layers);
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 3)) {
		/* Due to several changes to particle RNA and draw code particles from older files may no longer
		 * be visible. Here we correct this by setting a default draw size for those files. */
		for (Object *object = bmain->object.first; object; object = object->id.next) {
			for (ParticleSystem *psys = object->particlesystem.first; psys; psys = psys->next) {
				if (psys->part->draw_size == 0.0f) {
					psys->part->draw_size = 0.1f;
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 4)) {
		for (Object *object = bmain->object.first; object; object = object->id.next) {
#ifndef VERSION_280_SUBVERSION_4
			/* If any object already has an initialized value for
			 * duplicator_visibility_flag it means we've already doversioned it.
			 * TODO(all) remove the VERSION_280_SUBVERSION_4 code once the subversion was bumped. */
			if (object->duplicator_visibility_flag != 0) {
				break;
			}
#endif
			if (object->particlesystem.first) {
				object->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT;
				for (ParticleSystem *psys = object->particlesystem.first; psys; psys = psys->next) {
					if (psys->part->draw & PART_DRAW_EMITTER) {
						object->duplicator_visibility_flag |= OB_DUPLI_FLAG_RENDER;
#ifndef VERSION_280_SUBVERSION_4
						psys->part->draw &= ~PART_DRAW_EMITTER;
#else
						break;
#endif
					}
				}
			}
			else if (object->transflag & OB_DUPLI) {
				object->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT;
			}
			else {
				object->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT | OB_DUPLI_FLAG_RENDER;
			}
		}
	}

	/* SpaceTime & SpaceLogic removal/replacing */
	if (!MAIN_VERSION_ATLEAST(bmain, 280, 9)) {
		const wmWindowManager *wm = bmain->wm.first;
		const Scene *scene = bmain->scene.first;

		if (wm != NULL) {
			/* Action editors need a scene for creation. First, update active
			 * screens using the active scene of the window they're displayed in.
			 * Next, update remaining screens using first scene in main listbase. */

			for (wmWindow *win = wm->windows.first; win; win = win->next) {
				const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);
				for (ScrArea *area = screen->areabase.first; area; area = area->next) {
					if (ELEM(area->butspacetype, SPACE_TIME, SPACE_LOGIC)) {
						do_version_area_change_space_to_space_action(area, win->scene);

						/* Don't forget to unset! */
						area->butspacetype = SPACE_EMPTY;
					}
				}
			}
		}
		if (scene != NULL) {
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *area = screen->areabase.first; area; area = area->next) {
					if (ELEM(area->butspacetype, SPACE_TIME, SPACE_LOGIC)) {
						/* Areas that were already handled won't be handled again */
						do_version_area_change_space_to_space_action(area, scene);

						/* Don't forget to unset! */
						area->butspacetype = SPACE_EMPTY;
					}
				}
			}
		}
	}

#ifdef USE_COLLECTION_COMPAT_28
	if (use_collection_compat_28 && !MAIN_VERSION_ATLEAST(bmain, 280, 14)) {
		for (Collection *group = bmain->collection.first; group; group = group->id.next) {
			do_version_group_collection_to_collection(bmain, group);
		}

		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			do_version_scene_collection_to_collection(bmain, scene);
		}
	}
#endif
}

/* NOTE: this version patch is intended for versions < 2.52.2, but was initially introduced in 2.27 already.
 *       But in 2.79 another case generating non-unique names was discovered (see T55668, involving Meta strips)... */
static void do_versions_seq_unique_name_all_strips(Scene *sce, ListBase *seqbasep)
{
	for (Sequence *seq = seqbasep->first; seq != NULL; seq = seq->next) {
		BKE_sequence_base_unique_name_recursive(&sce->ed->seqbase, seq);
		if (seq->seqbase.first != NULL) {
			do_versions_seq_unique_name_all_strips(sce, &seq->seqbase);
		}
	}
}

void blo_do_versions_280(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
	bool use_collection_compat_28 = true;

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
		use_collection_compat_28 = false;

		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			scene->r.gauss = 1.5f;
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 1)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Lamp", "float", "bleedexp")) {
			for (Lamp *la = bmain->lamp.first; la; la = la->id.next) {
				la->bleedexp = 2.5f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "GPUDOFSettings", "float", "ratio")) {
			for (Camera *ca = bmain->camera.first; ca; ca = ca->id.next) {
				ca->gpu_dof.ratio = 1.0f;
			}
		}

		/* MTexPoly now removed. */
		if (DNA_struct_find(fd->filesdna, "MTexPoly")) {
			const int cd_mtexpoly = 15;  /* CD_MTEXPOLY, deprecated */
			for (Mesh *me = bmain->mesh.first; me; me = me->id.next) {
				/* If we have UV's, so this file will have MTexPoly layers too! */
				if (me->mloopuv != NULL) {
					CustomData_update_typemap(&me->pdata);
					CustomData_free_layers(&me->pdata, cd_mtexpoly, me->totpoly);
					BKE_mesh_update_customdata_pointers(me, false);
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 2)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Lamp", "float", "cascade_max_dist")) {
			for (Lamp *la = bmain->lamp.first; la; la = la->id.next) {
				la->cascade_max_dist = 1000.0f;
				la->cascade_count = 4;
				la->cascade_exponent = 0.8f;
				la->cascade_fade = 0.1f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "Lamp", "float", "contact_dist")) {
			for (Lamp *la = bmain->lamp.first; la; la = la->id.next) {
				la->contact_dist = 0.2f;
				la->contact_bias = 0.03f;
				la->contact_spread = 0.2f;
				la->contact_thickness = 0.2f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "LightProbe", "float", "vis_bias")) {
			for (LightProbe *probe = bmain->lightprobe.first; probe; probe = probe->id.next) {
				probe->vis_bias = 1.0f;
				probe->vis_blur = 0.2f;
			}
		}

		typedef enum eNTreeDoVersionErrors {
			NTREE_DOVERSION_NO_ERROR = 0,
			NTREE_DOVERSION_NEED_OUTPUT = (1 << 0),
			NTREE_DOVERSION_TRANSPARENCY_EMISSION = (1 << 1),
		} eNTreeDoVersionErrors;

		/* Eevee shader nodes renamed because of the output node system.
		 * Note that a new output node is not being added here, because it would be overkill
		 * to handle this case in lib_verify_nodetree.
		 *
		 * Also, metallic node is now unified into the principled node. */
		eNTreeDoVersionErrors error = NTREE_DOVERSION_NO_ERROR;

		FOREACH_NODETREE(bmain, ntree, id) {
			if (ntree->type == NTREE_SHADER) {
				for (bNode *node = ntree->nodes.first; node; node = node->next) {
					if (node->type == 194 /* SH_NODE_EEVEE_METALLIC */ &&
					    STREQ(node->idname, "ShaderNodeOutputMetallic"))
					{
						BLI_strncpy(node->idname, "ShaderNodeEeveeMetallic", sizeof(node->idname));
						error |= NTREE_DOVERSION_NEED_OUTPUT;
					}

					else if (node->type == SH_NODE_EEVEE_SPECULAR && STREQ(node->idname, "ShaderNodeOutputSpecular")) {
						BLI_strncpy(node->idname, "ShaderNodeEeveeSpecular", sizeof(node->idname));
						error |= NTREE_DOVERSION_NEED_OUTPUT;
					}

					else if (node->type == 196 /* SH_NODE_OUTPUT_EEVEE_MATERIAL */ &&
					         STREQ(node->idname, "ShaderNodeOutputEeveeMaterial"))
					{
						node->type = SH_NODE_OUTPUT_MATERIAL;
						BLI_strncpy(node->idname, "ShaderNodeOutputMaterial", sizeof(node->idname));
					}

					else if (node->type == 194 /* SH_NODE_EEVEE_METALLIC */ &&
					         STREQ(node->idname, "ShaderNodeEeveeMetallic"))
					{
						node->type = SH_NODE_BSDF_PRINCIPLED;
						BLI_strncpy(node->idname, "ShaderNodeBsdfPrincipled", sizeof(node->idname));
						node->custom1 = SHD_GLOSSY_MULTI_GGX;
						error |= NTREE_DOVERSION_TRANSPARENCY_EMISSION;
					}
				}
			}
		} FOREACH_NODETREE_END

		if (error & NTREE_DOVERSION_NEED_OUTPUT) {
			BKE_report(fd->reports, RPT_ERROR, "Eevee material conversion problem. Error in console");
			printf("You need to connect Principled and Eevee Specular shader nodes to new material output nodes.\n");
		}

		if (error & NTREE_DOVERSION_TRANSPARENCY_EMISSION) {
			BKE_report(fd->reports, RPT_ERROR, "Eevee material conversion problem. Error in console");
			printf("You need to combine transparency and emission shaders to the converted Principled shader nodes.\n");
		}

#ifdef USE_COLLECTION_COMPAT_28
		if (use_collection_compat_28 &&
		    (DNA_struct_elem_find(fd->filesdna, "ViewLayer", "FreestyleConfig", "freestyle_config") == false) &&
		    DNA_struct_elem_find(fd->filesdna, "Scene", "ListBase", "view_layers"))
		{
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				ViewLayer *view_layer;
				for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
					view_layer->flag |= VIEW_LAYER_FREESTYLE;
					view_layer->layflag = 0x7FFF;   /* solid ztra halo edge strand */
					view_layer->passflag = SCE_PASS_COMBINED | SCE_PASS_Z;
					view_layer->pass_alpha_threshold = 0.5f;
					BKE_freestyle_config_init(&view_layer->freestyle_config);
				}
			}
		}
#endif
	}

#ifdef USE_COLLECTION_COMPAT_28
	if (use_collection_compat_28 && !MAIN_VERSION_ATLEAST(bmain, 280, 3)) {
		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			ViewLayer *view_layer;
			for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
				do_version_view_layer_visibility(view_layer);
			}
		}

		for (Collection *group = bmain->collection.first; group; group = group->id.next) {
			if (group->view_layer != NULL) {
				do_version_view_layer_visibility(group->view_layer);
			}
		}
	}
#endif

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 6)) {
		if (DNA_struct_elem_find(fd->filesdna, "SpaceOops", "int", "filter") == false) {
			bScreen *sc;
			ScrArea *sa;
			SpaceLink *sl;

			/* Update files using invalid (outdated) outlinevis Outliner values. */
			for (sc = bmain->screen.first; sc; sc = sc->id.next) {
				for (sa = sc->areabase.first; sa; sa = sa->next) {
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_OUTLINER) {
							SpaceOops *so = (SpaceOops *)sl;

							if (!ELEM(so->outlinevis,
							          SO_SCENES,
							          SO_LIBRARIES,
							          SO_SEQUENCE,
							          SO_DATA_API,
							          SO_ID_ORPHANS))
							{
								so->outlinevis = SO_VIEW_LAYER;
							}
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "LightProbe", "float", "intensity")) {
			for (LightProbe *probe = bmain->lightprobe.first; probe; probe = probe->id.next) {
				probe->intensity = 1.0f;
			}
		}

		for (Object *ob = bmain->object.first; ob; ob = ob->id.next) {
			bConstraint *con, *con_next;
			con = ob->constraints.first;
			while (con) {
				con_next = con->next;
				if (con->type == 17) { /* CONSTRAINT_TYPE_RIGIDBODYJOINT */
					BLI_remlink(&ob->constraints, con);
					BKE_constraint_free_data(con);
					MEM_freeN(con);
				}
				con = con_next;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "Scene", "int", "orientation_index_custom")) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				scene->orientation_index_custom = -1;
			}
		}

		for (bScreen *sc = bmain->screen.first; sc; sc = sc->id.next) {
			for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
				for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_VIEW3D) {
						View3D *v3d = (View3D *)sl;
						v3d->shading.light = V3D_LIGHTING_STUDIO;
						v3d->shading.flag |= V3D_SHADING_OBJECT_OUTLINE;

						/* Assume (demo) files written with 2.8 want to show
						 * Eevee renders in the viewport. */
						if (MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
							v3d->drawtype = OB_MATERIAL;
						}
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 7)) {
		/* Render engine storage moved elsewhere and back during 2.8
		 * development, we assume any files saved in 2.8 had Eevee set
		 * as scene render engine. */
		if (MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(scene->r.engine));
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 8)) {
		/* Blender Internal removal */
		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			if (STREQ(scene->r.engine, "BLENDER_RENDER") ||
			    STREQ(scene->r.engine, "BLENDER_GAME"))
			{
				BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(scene->r.engine));
			}

			scene->r.bake_mode = 0;
		}

		for (Tex *tex = bmain->tex.first; tex; tex = tex->id.next) {
			/* Removed envmap, pointdensity, voxeldata, ocean textures. */
			if (ELEM(tex->type, 10, 14, 15, 16)) {
				tex->type = 0;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 11)) {

		/* Remove info editor, but only if at the top of the window. */
		for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
			/* Calculate window width/height from screen vertices */
			int win_width = 0, win_height = 0;
			for (ScrVert *vert = screen->vertbase.first; vert; vert = vert->next) {
				win_width  = MAX2(win_width, vert->vec.x);
				win_height = MAX2(win_height, vert->vec.y);
			}

			for (ScrArea *area = screen->areabase.first, *area_next; area; area = area_next) {
				area_next = area->next;

				if (area->spacetype == SPACE_INFO) {
					if ((area->v2->vec.y == win_height) && (area->v1->vec.x == 0) && (area->v4->vec.x == win_width)) {
						BKE_screen_area_free(area);

						BLI_remlink(&screen->areabase, area);

						BKE_screen_remove_double_scredges(screen);
						BKE_screen_remove_unused_scredges(screen);
						BKE_screen_remove_unused_scrverts(screen);

						MEM_freeN(area);
					}
				}
				/* AREA_TEMP_INFO is deprecated from now on, it should only be set for info areas
				 * which are deleted above, so don't need to unset it. Its slot/bit can be reused */
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 11)) {
		for (Lamp *lamp = bmain->lamp.first; lamp; lamp = lamp->id.next) {
			if (lamp->mode & (1 << 13)) { /* LA_SHAD_RAY */
				lamp->mode |= LA_SHADOW;
				lamp->mode &= ~(1 << 13);
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 12)) {
		/* Remove tool property regions. */
		for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
					if (ELEM(sl->spacetype, SPACE_VIEW3D, SPACE_CLIP)) {
						ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;

						for (ARegion *region = regionbase->first, *region_next; region; region = region_next) {
							region_next = region->next;

							if (region->regiontype == RGN_TYPE_TOOL_PROPS) {
								BKE_area_region_free(NULL, region);
								BLI_freelinkN(regionbase, region);
							}
						}
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 13)) {
		/* Initialize specular factor. */
		if (!DNA_struct_elem_find(fd->filesdna, "Lamp", "float", "spec_fac")) {
			for (Lamp *la = bmain->lamp.first; la; la = la->id.next) {
				la->spec_fac = 1.0f;
			}
		}

		/* Initialize new view3D options. */
		for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_VIEW3D) {
						View3D *v3d = (View3D *)sl;
						v3d->shading.light = V3D_LIGHTING_STUDIO;
						v3d->shading.color_type = V3D_SHADING_MATERIAL_COLOR;
						copy_v3_fl(v3d->shading.single_color, 0.8f);
						v3d->shading.shadow_intensity = 0.5;

						v3d->overlay.backwire_opacity = 0.5f;
						v3d->overlay.normals_length = 0.1f;
						v3d->overlay.flag = 0;
					}
				}
			}
		}

		if (!DNA_struct_find(fd->filesdna, "View3DCursor")) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				unit_qt(scene->cursor.rotation);
			}
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							unit_qt(v3d->cursor.rotation);
						}
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 14)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Scene", "SceneDisplay", "display")) {
			/* Initialize new scene.SceneDisplay */
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				copy_v3_v3(scene->display.light_direction, (float[3]){-M_SQRT1_3, -M_SQRT1_3, M_SQRT1_3});
			}
		}
		if (!DNA_struct_elem_find(fd->filesdna, "SceneDisplay", "float", "shadow_shift")) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				scene->display.shadow_shift = 0.1;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "Object", "ObjectDisplay", "display")) {
			/* Initialize new object.ObjectDisplay */
			for (Object *ob = bmain->object.first; ob; ob = ob->id.next) {
				ob->display.flag = OB_SHOW_SHADOW;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "ToolSettings", "char", "transform_pivot_point")) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				scene->toolsettings->transform_pivot_point = V3D_AROUND_CENTER_MEAN;
			}
		}

		if (!DNA_struct_find(fd->filesdna, "SceneEEVEE")) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				/* First set the default for all the properties. */

				scene->eevee.gi_diffuse_bounces = 3;
				scene->eevee.gi_cubemap_resolution = 512;
				scene->eevee.gi_visibility_resolution = 32;

				scene->eevee.taa_samples = 16;
				scene->eevee.taa_render_samples = 64;

				scene->eevee.sss_samples = 7;
				scene->eevee.sss_jitter_threshold = 0.3f;

				scene->eevee.ssr_quality = 0.25f;
				scene->eevee.ssr_max_roughness = 0.5f;
				scene->eevee.ssr_thickness = 0.2f;
				scene->eevee.ssr_border_fade = 0.075f;
				scene->eevee.ssr_firefly_fac = 10.0f;

				scene->eevee.volumetric_start = 0.1f;
				scene->eevee.volumetric_end = 100.0f;
				scene->eevee.volumetric_tile_size = 8;
				scene->eevee.volumetric_samples = 64;
				scene->eevee.volumetric_sample_distribution = 0.8f;
				scene->eevee.volumetric_light_clamp = 0.0f;
				scene->eevee.volumetric_shadow_samples = 16;

				scene->eevee.gtao_distance = 0.2f;
				scene->eevee.gtao_factor = 1.0f;
				scene->eevee.gtao_quality = 0.25f;

				scene->eevee.bokeh_max_size = 100.0f;
				scene->eevee.bokeh_threshold = 1.0f;

				copy_v3_fl(scene->eevee.bloom_color, 1.0f);
				scene->eevee.bloom_threshold = 0.8f;
				scene->eevee.bloom_knee = 0.5f;
				scene->eevee.bloom_intensity = 0.8f;
				scene->eevee.bloom_radius = 6.5f;
				scene->eevee.bloom_clamp = 1.0f;

				scene->eevee.motion_blur_samples = 8;
				scene->eevee.motion_blur_shutter = 1.0f;

				scene->eevee.shadow_method = SHADOW_ESM;
				scene->eevee.shadow_cube_size = 512;
				scene->eevee.shadow_cascade_size = 1024;

				scene->eevee.flag =
					SCE_EEVEE_VOLUMETRIC_LIGHTS |
					SCE_EEVEE_GTAO_BENT_NORMALS |
					SCE_EEVEE_GTAO_BOUNCE |
					SCE_EEVEE_TAA_REPROJECTION |
					SCE_EEVEE_SSR_HALF_RESOLUTION;


				/* If the file is pre-2.80 move on. */
				if (scene->layer_properties == NULL) {
					continue;
				}

				/* Now we handle eventual properties that may be set in the file. */
#define EEVEE_GET_BOOL(_props, _name, _flag) \
				{ \
					IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
					if (_idprop != NULL) { \
						const int _value = IDP_Int(_idprop); \
						if (_value) { \
							scene->eevee.flag |= _flag; \
						} \
						else { \
							scene->eevee.flag &= ~_flag; \
						} \
					} \
				}

#define EEVEE_GET_INT(_props, _name) \
				{ \
					IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
					if (_idprop != NULL) { \
						scene->eevee._name = IDP_Int(_idprop); \
					} \
				}

#define EEVEE_GET_FLOAT(_props, _name) \
				{ \
					IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
					if (_idprop != NULL) { \
						scene->eevee._name = IDP_Float(_idprop); \
					} \
				}

#define EEVEE_GET_FLOAT_ARRAY(_props, _name, _length) \
				{ \
					IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
					if (_idprop != NULL) { \
						const float *_values = IDP_Array(_idprop); \
						for (int _i = 0; _i < _length; _i++) { \
							scene->eevee._name [_i] = _values[_i]; \
						} \
					} \
				}

				IDProperty *props = IDP_GetPropertyFromGroup(scene->layer_properties, RE_engine_id_BLENDER_EEVEE);
				EEVEE_GET_BOOL(props, volumetric_enable, SCE_EEVEE_VOLUMETRIC_ENABLED);
				EEVEE_GET_BOOL(props, volumetric_lights, SCE_EEVEE_VOLUMETRIC_LIGHTS);
				EEVEE_GET_BOOL(props, volumetric_shadows, SCE_EEVEE_VOLUMETRIC_SHADOWS);
				EEVEE_GET_BOOL(props, gtao_enable, SCE_EEVEE_GTAO_ENABLED);
				EEVEE_GET_BOOL(props, gtao_use_bent_normals, SCE_EEVEE_GTAO_BENT_NORMALS);
				EEVEE_GET_BOOL(props, gtao_bounce, SCE_EEVEE_GTAO_BOUNCE);
				EEVEE_GET_BOOL(props, dof_enable, SCE_EEVEE_DOF_ENABLED);
				EEVEE_GET_BOOL(props, bloom_enable, SCE_EEVEE_BLOOM_ENABLED);
				EEVEE_GET_BOOL(props, motion_blur_enable, SCE_EEVEE_MOTION_BLUR_ENABLED);
				EEVEE_GET_BOOL(props, shadow_high_bitdepth, SCE_EEVEE_SHADOW_HIGH_BITDEPTH);
				EEVEE_GET_BOOL(props, taa_reprojection, SCE_EEVEE_TAA_REPROJECTION);
				EEVEE_GET_BOOL(props, sss_enable, SCE_EEVEE_SSS_ENABLED);
				EEVEE_GET_BOOL(props, sss_separate_albedo, SCE_EEVEE_SSS_SEPARATE_ALBEDO);
				EEVEE_GET_BOOL(props, ssr_enable, SCE_EEVEE_SSR_ENABLED);
				EEVEE_GET_BOOL(props, ssr_refraction, SCE_EEVEE_SSR_REFRACTION);
				EEVEE_GET_BOOL(props, ssr_halfres, SCE_EEVEE_SSR_HALF_RESOLUTION);

				EEVEE_GET_INT(props, gi_diffuse_bounces);
				EEVEE_GET_INT(props, gi_diffuse_bounces);
				EEVEE_GET_INT(props, gi_cubemap_resolution);
				EEVEE_GET_INT(props, gi_visibility_resolution);

				EEVEE_GET_INT(props, taa_samples);
				EEVEE_GET_INT(props, taa_render_samples);

				EEVEE_GET_INT(props, sss_samples);
				EEVEE_GET_FLOAT(props, sss_jitter_threshold);

				EEVEE_GET_FLOAT(props, ssr_quality);
				EEVEE_GET_FLOAT(props, ssr_max_roughness);
				EEVEE_GET_FLOAT(props, ssr_thickness);
				EEVEE_GET_FLOAT(props, ssr_border_fade);
				EEVEE_GET_FLOAT(props, ssr_firefly_fac);

				EEVEE_GET_FLOAT(props, volumetric_start);
				EEVEE_GET_FLOAT(props, volumetric_end);
				EEVEE_GET_INT(props, volumetric_tile_size);
				EEVEE_GET_INT(props, volumetric_samples);
				EEVEE_GET_FLOAT(props, volumetric_sample_distribution);
				EEVEE_GET_FLOAT(props, volumetric_light_clamp);
				EEVEE_GET_INT(props, volumetric_shadow_samples);

				EEVEE_GET_FLOAT(props, gtao_distance);
				EEVEE_GET_FLOAT(props, gtao_factor);
				EEVEE_GET_FLOAT(props, gtao_quality);

				EEVEE_GET_FLOAT(props, bokeh_max_size);
				EEVEE_GET_FLOAT(props, bokeh_threshold);

				EEVEE_GET_FLOAT_ARRAY(props, bloom_color, 3);
				EEVEE_GET_FLOAT(props, bloom_threshold);
				EEVEE_GET_FLOAT(props, bloom_knee);
				EEVEE_GET_FLOAT(props, bloom_intensity);
				EEVEE_GET_FLOAT(props, bloom_radius);
				EEVEE_GET_FLOAT(props, bloom_clamp);

				EEVEE_GET_INT(props, motion_blur_samples);
				EEVEE_GET_FLOAT(props, motion_blur_shutter);

				EEVEE_GET_INT(props, shadow_method);
				EEVEE_GET_INT(props, shadow_cube_size);
				EEVEE_GET_INT(props, shadow_cascade_size);

				/* Cleanup. */
				IDP_FreeProperty(scene->layer_properties);
				MEM_freeN(scene->layer_properties);
				scene->layer_properties = NULL;

#undef EEVEE_GET_FLOAT_ARRAY
#undef EEVEE_GET_FLOAT
#undef EEVEE_GET_INT
#undef EEVEE_GET_BOOL
			}
		}


		if (!MAIN_VERSION_ATLEAST(bmain, 280, 15)) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				scene->display.matcap_ssao_distance = 0.2f;
				scene->display.matcap_ssao_attenuation = 1.0f;
				scene->display.matcap_ssao_samples = 16;
			}

			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_OUTLINER) {
							SpaceOops *soops = (SpaceOops *)sl;
							soops->filter_id_type = ID_GR;
							soops->outlinevis = SO_VIEW_LAYER;
						}
					}
				}
			}

			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				switch (scene->toolsettings->snap_mode) {
					case 0: scene->toolsettings->snap_mode = SCE_SNAP_MODE_INCREMENT; break;
					case 1: scene->toolsettings->snap_mode = SCE_SNAP_MODE_VERTEX   ; break;
					case 2: scene->toolsettings->snap_mode = SCE_SNAP_MODE_EDGE     ; break;
					case 3: scene->toolsettings->snap_mode = SCE_SNAP_MODE_FACE     ; break;
					case 4: scene->toolsettings->snap_mode = SCE_SNAP_MODE_VOLUME   ; break;
				}
				switch (scene->toolsettings->snap_node_mode) {
					case 5: scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_NODE_X; break;
					case 6: scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_NODE_Y; break;
					case 7: scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_NODE_X | SCE_SNAP_MODE_NODE_Y; break;
					case 8: scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_GRID  ; break;
				}
				switch (scene->toolsettings->snap_uv_mode) {
					case 0: scene->toolsettings->snap_uv_mode = SCE_SNAP_MODE_INCREMENT; break;
					case 1: scene->toolsettings->snap_uv_mode = SCE_SNAP_MODE_VERTEX   ; break;
				}
			}

			ParticleSettings *part;
			for (part = bmain->particle.first; part; part = part->id.next) {
				part->shape_flag = PART_SHAPE_CLOSE_TIP;
				part->shape = 0.0f;
				part->rad_root = 1.0f;
				part->rad_tip = 0.0f;
				part->rad_scale = 0.01f;
			}
		}

	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 18)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Material", "float", "roughness")) {
			for (Material *mat = bmain->mat.first; mat; mat = mat->id.next) {
				if (mat->use_nodes) {
					if (MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
						mat->roughness = mat->gloss_mir;
					}
					else {
						mat->roughness = 0.25f;
					}
				}
				else {
					mat->roughness = 1.0f - mat->gloss_mir;
				}
				mat->metallic = mat->ray_mirror;
			}

			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							v3d->shading.flag |= V3D_SHADING_SPECULAR_HIGHLIGHT;
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "xray_alpha")) {
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							v3d->shading.xray_alpha = 0.5f;
						}
					}
				}
			}
		}
		if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "char", "matcap[256]")) {
			StudioLight *default_matcap = BKE_studiolight_find_first(STUDIOLIGHT_ORIENTATION_VIEWNORMAL);
			/* when loading the internal file is loaded before the matcaps */
			if (default_matcap) {
				for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
					for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
						for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
							if (sl->spacetype == SPACE_VIEW3D) {
								View3D *v3d = (View3D *)sl;
								BLI_strncpy(v3d->shading.matcap, default_matcap->name, FILE_MAXFILE);
							}
						}
					}
				}
			}
		}
		if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "wireframe_threshold")) {
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							v3d->overlay.wireframe_threshold = 0.5f;
						}
					}
				}
			}
		}
		if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "cavity_valley_factor")) {
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							v3d->shading.cavity_valley_factor = 1.0f;
							v3d->shading.cavity_ridge_factor = 1.0f;
						}
					}
				}
			}
		}
		if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "bone_select_alpha")) {
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							v3d->overlay.bone_select_alpha = 0.5f;
						}
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 19)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Image", "ListBase", "renderslot")) {
			for (Image *ima = bmain->image.first; ima; ima = ima->id.next) {
				if (ima->type == IMA_TYPE_R_RESULT) {
					for (int i = 0; i < 8; i++) {
						RenderSlot *slot = MEM_callocN(sizeof(RenderSlot), "Image Render Slot Init");
						BLI_snprintf(slot->name, sizeof(slot->name), "Slot %d", i + 1);
						BLI_addtail(&ima->renderslots, slot);
					}
				}
			}
		}
		if (!DNA_struct_elem_find(fd->filesdna, "SpaceAction", "char", "mode_prev")) {
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_ACTION) {
							SpaceAction *saction = (SpaceAction *)sl;
							/* "Dopesheet" should be default here, unless it looks like the Action Editor was active instead */
							if ((saction->mode_prev == 0) && (saction->action == NULL)) {
								saction->mode_prev = SACTCONT_DOPESHEET;
							}
						}
					}
				}
			}
		}

		for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_VIEW3D) {
						View3D *v3d = (View3D *)sl;
						if (v3d->drawtype == OB_TEXTURE) {
							v3d->drawtype = OB_SOLID;
							v3d->shading.light = V3D_LIGHTING_STUDIO;
							v3d->shading.color_type = V3D_SHADING_TEXTURE_COLOR;
						}
					}
				}
			}
		}

	}

	if (!MAIN_VERSION_ATLEAST(bmain, 280, 21)) {
		for (Scene *sce = bmain->scene.first; sce != NULL; sce = sce->id.next) {
			if (sce->ed != NULL && sce->ed->seqbase.first != NULL) {
				do_versions_seq_unique_name_all_strips(sce, &sce->ed->seqbase);
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "texture_paint_mode_opacity")) {
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							float alpha = v3d->flag2 & V3D_SHOW_MODE_SHADE_OVERRIDE ? 0.0f : 0.8f;
							v3d->overlay.texture_paint_mode_opacity = alpha;
							v3d->overlay.vertex_paint_mode_opacity = alpha;
							v3d->overlay.weight_paint_mode_opacity = alpha;
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "gi_cubemap_draw_size")) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				scene->eevee.gi_irradiance_draw_size = 0.1f;
				scene->eevee.gi_cubemap_draw_size = 0.3f;
			}
		}

		for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
			if (scene->toolsettings->gizmo_flag == 0) {
				scene->toolsettings->gizmo_flag = SCE_MANIP_TRANSLATE | SCE_MANIP_ROTATE | SCE_MANIP_SCALE;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "RigidBodyWorld", "RigidBodyWorld_Shared", "*shared")) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				RigidBodyWorld *rbw = scene->rigidbody_world;

				if (rbw == NULL) {
					continue;
				}

				if (rbw->shared == NULL) {
					rbw->shared = MEM_callocN(sizeof(*rbw->shared), "RigidBodyWorld_Shared");
				}

				/* Move shared pointers from deprecated location to current location */
				rbw->shared->pointcache = rbw->pointcache;
				rbw->shared->ptcaches = rbw->ptcaches;

				rbw->pointcache = NULL;
				BLI_listbase_clear(&rbw->ptcaches);

				if (rbw->shared->pointcache == NULL) {
					rbw->shared->pointcache = BKE_ptcache_add(&(rbw->shared->ptcaches));
				}

			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "SoftBody", "SoftBody_Shared", "*shared")) {
			for (Object *ob = bmain->object.first; ob; ob = ob->id.next) {
				SoftBody *sb = ob->soft;
				if (sb == NULL) {
					continue;
				}
				if (sb->shared == NULL) {
					sb->shared = MEM_callocN(sizeof(*sb->shared), "SoftBody_Shared");
				}

				/* Move shared pointers from deprecated location to current location */
				sb->shared->pointcache = sb->pointcache;
				sb->shared->ptcaches = sb->ptcaches;

				sb->pointcache = NULL;
				BLI_listbase_clear(&sb->ptcaches);
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "short", "type")) {
			for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							if (v3d->drawtype == OB_RENDER) {
								v3d->drawtype = OB_SOLID;
							}
							v3d->shading.type = v3d->drawtype;
							v3d->shading.prev_type = OB_SOLID;
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "SceneDisplay", "View3DShading", "shading")) {
			for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
				BKE_screen_view3d_shading_init(&scene->display.shading);
			}
		}
	}

}
