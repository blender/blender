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

#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_gpu_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_layer_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_genfile.h"
#include "DNA_workspace_types.h"

#include "BKE_collection.h"
#include "BKE_customdata.h"
#include "BKE_freestyle.h"
#include "BKE_group.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
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
		Scene *scene = screen->scene;
		WorkSpace *workspace;
		ViewLayer *layer = BKE_view_layer_from_scene_get(scene);
		ListBase *transform_orientations;

		if (screen_parent) {
			/* fullscreen with "Back to Previous" option, don't create
			 * a new workspace, add layout workspace containing parent */
			workspace = BLI_findstring(
			        &bmain->workspaces, screen_parent->id.name + 2, offsetof(ID, name) + 2);
		}
		else {
			workspace = BKE_workspace_add(bmain, screen->id.name + 2);
		}
		BKE_workspace_layout_add(workspace, screen, screen->id.name + 2);
		BKE_workspace_view_layer_set(workspace, layer, scene);

#ifdef WITH_CLAY_ENGINE
		BLI_strncpy(workspace->view_render.engine_id, RE_engine_id_BLENDER_CLAY,
		            sizeof(workspace->view_render.engine_id));
#else
		BLI_strncpy(workspace->view_render.engine_id, RE_engine_id_BLENDER_EEVEE,
		            sizeof(workspace->view_render.engine_id));
#endif

		transform_orientations = BKE_workspace_transform_orientations_get(workspace);
		BLI_duplicatelist(transform_orientations, &scene->transform_spaces);
	}
}

/**
 * \brief After lib-link versioning for new workspace design.
 *
 *  *  Adds a workspace for (almost) each screen of the old file
 *     and adds the needed workspace-layout to wrap the screen.
 *  *  Active screen isn't stored directly in window anymore, but in the active workspace.
 *  *  Active scene isn't stored in screen anymore, but in window.
 *  *  Create workspace instance hook for each window.
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
			WorkSpace *workspace = BLI_findstring(&bmain->workspaces, screen->id.name + 2, offsetof(ID, name) + 2);
			ListBase *layouts = BKE_workspace_layouts_get(workspace);

			win->workspace_hook = BKE_workspace_instance_hook_create(bmain);

			BKE_workspace_active_set(win->workspace_hook, workspace);
			BKE_workspace_active_layout_set(win->workspace_hook, layouts->first);

			win->scene = screen->scene;
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

enum {
	DO_VERSION_COLLECTION_VISIBLE     = 0,
	DO_VERSION_COLLECTION_HIDE        = 1,
	DO_VERSION_COLLECTION_HIDE_RENDER = 2,
	DO_VERSION_COLLECTION_HIDE_ALL    = 3,
};

void do_versions_after_linking_280(Main *main)
{
	if (!MAIN_VERSION_ATLEAST(main, 280, 0)) {
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			/* since we don't have access to FileData we check the (always valid) first render layer instead */
			if (scene->view_layers.first == NULL) {
				SceneCollection *sc_master = BKE_collection_master(&scene->id);
				BLI_strncpy(sc_master->name, "Master Collection", sizeof(sc_master->name));

				struct DoVersionSceneCollections {
					SceneCollection *collections[20];
					int created;
					const char *suffix;
					int flag_viewport;
					int flag_render;
				} collections[] =
				{
					{
						.collections = {NULL},
						.created = 0,
						.suffix = "",
						.flag_viewport = COLLECTION_SELECTABLE,
						.flag_render = COLLECTION_SELECTABLE
					},
					{
						.collections = {NULL},
						.created = 0,
						.suffix = " - Hide Viewport",
						.flag_viewport = COLLECTION_SELECTABLE,
						.flag_render = COLLECTION_SELECTABLE
					},
					{
						.collections = {NULL},
						.created = 0,
						.suffix = " - Hide Render",
						.flag_viewport = COLLECTION_SELECTABLE,
						.flag_render = COLLECTION_SELECTABLE | COLLECTION_DISABLED
					},
					{
						.collections = {NULL},
						.created = 0,
						.suffix = " - Hide Render All",
						.flag_viewport = COLLECTION_SELECTABLE | COLLECTION_DISABLED,
						.flag_render = COLLECTION_SELECTABLE | COLLECTION_DISABLED
					}
				};

				for (int layer = 0; layer < 20; layer++) {
					for (Base *base = scene->base.first; base; base = base->next) {
						if (base->lay & (1 << layer)) {
							int collection_index = -1;
							if ((base->object->restrictflag & OB_RESTRICT_VIEW) &&
							    (base->object->restrictflag & OB_RESTRICT_RENDER))
							{
								collection_index = DO_VERSION_COLLECTION_HIDE_ALL;
							}
							else if (base->object->restrictflag & OB_RESTRICT_VIEW) {
								collection_index = DO_VERSION_COLLECTION_HIDE;
							}
							else if (base->object->restrictflag & OB_RESTRICT_RENDER) {
								collection_index = DO_VERSION_COLLECTION_HIDE_RENDER;
							}
							else {
								collection_index = DO_VERSION_COLLECTION_VISIBLE;
							}

							/* Create collections when needed only. */
							if ((collections[collection_index].created & (1 << layer)) == 0) {
								char name[MAX_NAME];

								if ((collections[DO_VERSION_COLLECTION_VISIBLE].created & (1 << layer)) == 0) {
									BLI_snprintf(name,
									             sizeof(sc_master->name),
									             "Collection %d%s",
									             layer + 1,
									             collections[DO_VERSION_COLLECTION_VISIBLE].suffix);
									collections[DO_VERSION_COLLECTION_VISIBLE].collections[layer] =
									        BKE_collection_add(&scene->id, sc_master, COLLECTION_TYPE_NONE, name);
									collections[DO_VERSION_COLLECTION_VISIBLE].created |= (1 << layer);
								}

								if (collection_index != DO_VERSION_COLLECTION_VISIBLE) {
									SceneCollection *sc_parent;
									sc_parent = collections[DO_VERSION_COLLECTION_VISIBLE].collections[layer];
									BLI_snprintf(name,
									             sizeof(sc_master->name),
									             "Collection %d%s",
									             layer + 1,
									             collections[collection_index].suffix);
									collections[collection_index].collections[layer] = BKE_collection_add(
									        &scene->id,
									        sc_parent,
									        COLLECTION_TYPE_NONE,
									        name);
									collections[collection_index].created |= (1 << layer);
								}
							}

							BKE_collection_object_add(
							        &scene->id, collections[collection_index].collections[layer], base->object);
						}

						if (base->flag & SELECT) {
							base->object->flag |= SELECT;
						}
						else {
							base->object->flag &= ~SELECT;
						}
					}
				}

				/* Re-order the nested hidden collections. */
				SceneCollection *scene_collection_parent = sc_master->scene_collections.first;

				for (int layer = 0; layer < 20; layer++) {
					if (collections[DO_VERSION_COLLECTION_VISIBLE].created & (1 << layer)) {

						if ((collections[DO_VERSION_COLLECTION_HIDE].created & (1 << layer)) &&
						    (collections[DO_VERSION_COLLECTION_HIDE].collections[layer] !=
						     scene_collection_parent->scene_collections.first))
						{
							BLI_listbase_swaplinks(
							        &scene_collection_parent->scene_collections,
							        collections[DO_VERSION_COLLECTION_HIDE].collections[layer],
							        scene_collection_parent->scene_collections.first);
						}

						if ((collections[DO_VERSION_COLLECTION_HIDE_ALL].created & (1 << layer)) &&
						    (collections[DO_VERSION_COLLECTION_HIDE_ALL].collections[layer] !=
						     scene_collection_parent->scene_collections.last))
						{
							BLI_listbase_swaplinks(
							        &scene_collection_parent->scene_collections,
							        collections[DO_VERSION_COLLECTION_HIDE_ALL].collections[layer],
							        scene_collection_parent->scene_collections.last);
						}

						scene_collection_parent = scene_collection_parent->next;
					}
				}
				BLI_assert(scene_collection_parent == NULL);

				scene->active_view_layer = 0;

				/* Handle legacy render layers. */
				if (!BKE_scene_uses_blender_game(scene)) {
					for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next) {

						ViewLayer *view_layer = BKE_view_layer_add(scene, srl->name);

						if (srl->samples != 0) {
							/* It is up to the external engine to handle
							 * its own doversion in this case. */
							BKE_override_view_layer_int_add(
							        view_layer,
							        ID_SCE,
							        "samples",
							        srl->samples);
						}

						if (srl->mat_override) {
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

						/* unlink master collection  */
						BKE_collection_unlink(view_layer, view_layer->layer_collections.first);

						/* Add new collection bases. */
						for (int layer = 0; layer < 20; layer++) {
							if ((scene->lay & srl->lay & ~(srl->lay_exclude) & (1 << layer)) ||
							    (srl->lay_zmask & (scene->lay | srl->lay_exclude) & (1 << layer)))
							{
								if (collections[DO_VERSION_COLLECTION_VISIBLE].created & (1 << layer)) {

									LayerCollection *layer_collection_parent;
									layer_collection_parent = BKE_collection_link(
									        view_layer,
									        collections[DO_VERSION_COLLECTION_VISIBLE].collections[layer]);

									if (srl->lay_zmask & (1 << layer)) {
										BKE_override_layer_collection_boolean_add(
										        layer_collection_parent,
										        ID_OB,
										        "cycles.is_holdout",
										        true);
									}

									if ((srl->lay & (1 << layer)) == 0) {
										BKE_override_layer_collection_boolean_add(
										        layer_collection_parent,
										        ID_OB,
										        "cycles_visibility.camera",
										        false);
									}

									LayerCollection *layer_collection_child;
									layer_collection_child = layer_collection_parent->layer_collections.first;

									for (int j = 1; j < 4; j++) {
										if (collections[j].created & (1 << layer)) {
											layer_collection_child->flag = COLLECTION_VIEWPORT |
											                               COLLECTION_RENDER |
											                               collections[j].flag_render;
											layer_collection_child = layer_collection_child->next;
										}
									}
									BLI_assert(layer_collection_child == NULL);
								}
							}
						}

						/* for convenience set the same active object in all the layers */
						if (scene->basact) {
							view_layer->basact = BKE_view_layer_base_find(view_layer, scene->basact->object);
						}

						for (Base *base = view_layer->object_bases.first; base; base = base->next) {
							if ((base->flag & BASE_SELECTABLED) && (base->object->flag & SELECT)) {
								base->flag |= BASE_SELECTED;
							}
						}
					}

					if (BLI_findlink(&scene->view_layers, scene->r.actlay)) {
						scene->active_view_layer = scene->r.actlay;
					}
				}
				else {
					for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next) {
						if (srl->prop) {
							IDP_FreeProperty(srl->prop);
							MEM_freeN(srl->prop);
						}
						BKE_freestyle_config_free(&srl->freestyleConfig, true);
					}
				}
				BLI_freelistN(&scene->r.layers);

				ViewLayer *view_layer = BKE_view_layer_add(scene, "Viewport");
				/* If we ported all the original render layers, we don't need to make the viewport layer renderable. */
				if (!BLI_listbase_is_single(&scene->view_layers)) {
					view_layer->flag &= ~VIEW_LAYER_RENDER;
				}

				/* If layer was not set, disable it. */
				LayerCollection *layer_collection_parent;
				layer_collection_parent =
					((LayerCollection *)view_layer->layer_collections.first)->layer_collections.first;

				for (int layer = 0; layer < 20; layer++) {
					if (collections[DO_VERSION_COLLECTION_VISIBLE].created & (1 << layer)) {
						const bool is_disabled = (scene->lay & (1 << layer)) == 0;

						/* We only need to disable the parent collection. */
						if (is_disabled) {
							layer_collection_parent->flag |= COLLECTION_DISABLED;
						}

						LayerCollection *layer_collection_child;
						layer_collection_child = layer_collection_parent->layer_collections.first;

						for (int j = 1; j < 4; j++) {
							if (collections[j].created & (1 << layer)) {
								layer_collection_child->flag = COLLECTION_VIEWPORT |
								                               COLLECTION_RENDER |
								                               collections[j].flag_viewport;
								layer_collection_child = layer_collection_child->next;
							}
						}
						BLI_assert(layer_collection_child == NULL);
						layer_collection_parent = layer_collection_parent->next;
					}
				}
				BLI_assert(layer_collection_parent == NULL);

				/* convert active base */
				if (scene->basact) {
					view_layer->basact = BKE_view_layer_base_find(view_layer, scene->basact->object);
				}

				/* convert selected bases */
				for (Base *base = scene->base.first; base; base = base->next) {
					if ((base->flag & BASE_SELECTABLED) && (base->object->flag & SELECT)) {
						base->flag |= BASE_SELECTED;
					}

					/* keep lay around for forward compatibility (open those files in 2.79) */
					base->lay = base->object->lay;
				}

				/* remove bases once and for all */
				for (Base *base = scene->base.first; base; base = base->next) {
					id_us_min(&base->object->id);
				}
				BLI_freelistN(&scene->base);
				scene->basact = NULL;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 280, 0)) {
		for (bScreen *screen = main->screen.first; screen; screen = screen->id.next) {
			/* same render-layer as do_version_workspaces_after_lib_link will activate,
			 * so same layer as BKE_view_layer_from_workspace_get would return */
			ViewLayer *layer = screen->scene->view_layers.first;

			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				for (SpaceLink *view_layer = sa->spacedata.first; view_layer; view_layer = view_layer->next) {
					if (view_layer->spacetype == SPACE_OUTLINER) {
						SpaceOops *soutliner = (SpaceOops *)view_layer;

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
	if (!MAIN_VERSION_ATLEAST(main, 280, 1)) {
		do_version_workspaces_after_lib_link(main);
	}

	if (!MAIN_VERSION_ATLEAST(main, 280, 2)) {
		/* Cleanup any remaining SceneRenderLayer data for files that were created
		 * with Blender 2.8 before the SceneRenderLayer > RenderLayer refactor. */
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
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

	if (!MAIN_VERSION_ATLEAST(main, 280, 3)) {
		/* Due to several changes to particle RNA and draw code particles from older files may no longer
		 * be visible. Here we correct this by setting a default draw size for those files. */
		for (Object *object = main->object.first; object; object = object->id.next) {
			for (ParticleSystem *psys = object->particlesystem.first; psys; psys=psys->next) {
				if(psys->part->draw_size == 0.0f) {
					psys->part->draw_size = 0.1f;
				}
			}
		}
	}

	{
		for (WorkSpace *workspace = main->workspaces.first; workspace; workspace = workspace->id.next) {
			if (workspace->view_layer) {
				/* During 2.8 work we temporarly stored view-layer in the
				 * workspace directly, but should be stored there per-scene. */
				for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
					if (BLI_findindex(&scene->view_layers, workspace->view_layer) != -1) {
						BKE_workspace_view_layer_set(workspace, workspace->view_layer, scene);
						workspace->view_layer = NULL;
					}
				}
			}
			/* While this should apply to most cases, it fails when reading workspaces.blend
			 * to get its list of workspaces without actually appending any of them. */
//			BLI_assert(workspace->view_layer == NULL);
		}
	}

	{
		/* Since we don't have access to FileData we check the (always valid) master collection of the group. */
		for (Group *group = main->group.first; group; group = group->id.next) {
			if (group->collection == NULL) {
				BKE_group_init(group);
				SceneCollection *sc = GROUP_MASTER_COLLECTION(group);
				SceneCollection *sc_hidden = NULL;

				for (GroupObject *go = group->gobject.first; go; go = go->next) {
					if (go->ob->lay & group->layer) {
						BKE_collection_object_add(&group->id, sc, go->ob);
					}
					else {
						if (sc_hidden == NULL) {
							sc_hidden = BKE_collection_add(&group->id, sc, COLLECTION_TYPE_GROUP_INTERNAL, "Hidden");
						}
						BKE_collection_object_add(&group->id, sc_hidden, go->ob);
					}
				}

				if (sc_hidden != NULL) {
					LayerCollection *layer_collection_master, *layer_collection_hidden;
					layer_collection_master = group->view_layer->layer_collections.first;
					layer_collection_hidden = layer_collection_master->layer_collections.first;
					layer_collection_hidden->flag |= COLLECTION_DISABLED;
				}
			}

			GroupObject *go;
			while ((go = BLI_pophead(&group->gobject))) {
				MEM_freeN(go);
			}
		}
	}

	{
		for (Object *object = main->object.first; object; object = object->id.next) {
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
			else if (object->transflag & OB_DUPLI){
				object->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT;
			}
			else {
				object->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT | OB_DUPLI_FLAG_RENDER;
			}
		}
	}
}

static void do_version_layer_collections_idproperties(ListBase *lb)
{
	IDPropertyTemplate val = {0};
	for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
		lc->properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
		BKE_layer_collection_engine_settings_create(lc->properties);

		/* No overrides at first */
		for (IDProperty *prop = lc->properties->data.group.first; prop; prop = prop->next) {
			while (prop->data.group.first) {
				IDP_FreeFromGroup(prop, prop->data.group.first);
			}
		}

		/* Do it recursively */
		do_version_layer_collections_idproperties(&lc->layer_collections);
	}
}

static void do_version_view_layer_visibility(ViewLayer *view_layer)
{
	LayerCollection *layer_collection;
	for (layer_collection = view_layer->layer_collections.first;
	     layer_collection;
	     layer_collection = layer_collection->next)
	{
		if (layer_collection->flag & COLLECTION_DISABLED) {
			BKE_collection_enable(view_layer, layer_collection);
			layer_collection->flag &= ~COLLECTION_DISABLED;
		}

		if ((layer_collection->flag & (1 << 0)) == 0) { /* !COLLECTION_VISIBLE */
			layer_collection->flag |= COLLECTION_DISABLED;
		}
		layer_collection->flag |= COLLECTION_VIEWPORT | COLLECTION_RENDER;
	}
}

void blo_do_versions_280(FileData *fd, Library *UNUSED(lib), Main *main)
{

	if (!MAIN_VERSION_ATLEAST(main, 280, 0)) {
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			if (STREQ(scene->r.engine, RE_engine_id_BLENDER_RENDER)) {
#ifdef WITH_CLAY_ENGINE
				BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_CLAY, sizeof(scene->r.engine));
#else
				BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(scene->r.engine));
#endif
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "Scene", "ListBase", "view_layers")) {
			for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
				/* Master Collection */
				scene->collection = MEM_callocN(sizeof(SceneCollection), "Master Collection");
				BLI_strncpy(scene->collection->name, "Master Collection", sizeof(scene->collection->name));
			}
		}

		if (DNA_struct_elem_find(fd->filesdna, "LayerCollection", "ListBase", "engine_settings") &&
		    !DNA_struct_elem_find(fd->filesdna, "LayerCollection", "IDProperty", "properties"))
		{
			for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
				for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
					do_version_layer_collections_idproperties(&view_layer->layer_collections);
				}
			}
		}

		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			scene->r.gauss = 1.5f;
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 280, 1)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Lamp", "float", "bleedexp")) {
			for (Lamp *la = main->lamp.first; la; la = la->id.next) {
				la->bleedexp = 2.5f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "GPUDOFSettings", "float", "ratio")) {
			for (Camera *ca = main->camera.first; ca; ca = ca->id.next) {
				ca->gpu_dof.ratio = 1.0f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "ViewLayer", "IDProperty", "*properties")) {
			for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
				for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
					IDPropertyTemplate val = {0};
					view_layer->properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
					BKE_view_layer_engine_settings_create(view_layer->properties);
				}
			}
		}

		/* MTexPoly now removed. */
		if (DNA_struct_find(fd->filesdna, "MTexPoly")) {
			const int cd_mtexpoly = 15;  /* CD_MTEXPOLY, deprecated */
			for (Mesh *me = main->mesh.first; me; me = me->id.next) {
				/* If we have UV's, so this file will have MTexPoly layers too! */
				if (me->mloopuv != NULL) {
					CustomData_update_typemap(&me->pdata);
					CustomData_free_layers(&me->pdata, cd_mtexpoly, me->totpoly);
					BKE_mesh_update_customdata_pointers(me, false);
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 280, 2)) {
		if (!DNA_struct_elem_find(fd->filesdna, "View3D", "short", "custom_orientation_index")) {
			for (bScreen *screen = main->screen.first; screen; screen = screen->id.next) {
				for (ScrArea *area = screen->areabase.first; area; area = area->next) {
					for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							if (v3d->twmode >= V3D_MANIP_CUSTOM) {
								v3d->custom_orientation_index = v3d->twmode - V3D_MANIP_CUSTOM;
								v3d->twmode = V3D_MANIP_CUSTOM;
							}
							else {
								v3d->custom_orientation_index = -1;
							}
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "Lamp", "float", "cascade_max_dist")) {
			for (Lamp *la = main->lamp.first; la; la = la->id.next) {
				la->cascade_max_dist = 1000.0f;
				la->cascade_count = 4;
				la->cascade_exponent = 0.8f;
				la->cascade_fade = 0.1f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "Lamp", "float", "contact_dist")) {
			for (Lamp *la = main->lamp.first; la; la = la->id.next) {
				la->contact_dist = 1.0f;
				la->contact_bias = 0.03f;
				la->contact_spread = 0.2f;
				la->contact_thickness = 0.5f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "LightProbe", "float", "vis_bias")) {
			for (LightProbe *probe = main->lightprobe.first; probe; probe = probe->id.next) {
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

		FOREACH_NODETREE(main, ntree, id) {
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

		if (!DNA_struct_elem_find(fd->filesdna, "Scene", "ViewRender", "view_render")) {
			for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
				BLI_strncpy_utf8(scene->view_render.engine_id, scene->r.engine,
				                 sizeof(scene->view_render.engine_id));
			}

			for (WorkSpace *workspace = main->workspaces.first; workspace; workspace = workspace->id.next) {
				BKE_viewrender_init(&workspace->view_render);
			}
		}

		if ((DNA_struct_elem_find(fd->filesdna, "ViewLayer", "FreestyleConfig", "freestyle_config") == false) &&
		    DNA_struct_elem_find(fd->filesdna, "Scene", "ListBase", "view_layers"))
		{
			for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
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
	}

	if (!MAIN_VERSION_ATLEAST(main, 280, 3)) {
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			ViewLayer *view_layer;
			for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
				do_version_view_layer_visibility(view_layer);
			}
		}

		for (Group *group = main->group.first; group; group = group->id.next) {
			if (group->view_layer != NULL){
				do_version_view_layer_visibility(group->view_layer);
			}
		}
	}

	{
		if (DNA_struct_elem_find(fd->filesdna, "SpaceOops", "int", "filter") == false) {
			bScreen *sc;
			ScrArea *sa;
			SpaceLink *sl;

			/* Update files using invalid (outdated) outlinevis Outliner values. */
			for (sc = main->screen.first; sc; sc = sc->id.next) {
				for (sa = sc->areabase.first; sa; sa = sa->next) {
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_OUTLINER) {
							SpaceOops *so = (SpaceOops *)sl;

							if (!ELEM(so->outlinevis,
							          SO_SCENES,
							          SO_GROUPS,
							          SO_LIBRARIES,
							          SO_SEQUENCE,
							          SO_DATABLOCKS,
							          SO_ID_ORPHANS,
							          SO_VIEW_LAYER,
							          SO_COLLECTIONS))
							{
								so->outlinevis = SO_VIEW_LAYER;
							}
						}
					}
				}
			}
		}
	}

	{
		if (!DNA_struct_elem_find(fd->filesdna, "LightProbe", "float", "intensity")) {
			for (LightProbe *probe = main->lightprobe.first; probe; probe = probe->id.next) {
				probe->intensity = 1.0f;
			}
		}
	}
}
