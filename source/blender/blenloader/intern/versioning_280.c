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

#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_gpu_types.h"
#include "DNA_lamp_types.h"
#include "DNA_layer_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_genfile.h"
#include "DNA_workspace_types.h"

#include "BKE_collection.h"
#include "BKE_customdata.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

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
		SceneLayer *layer = BKE_scene_layer_render_active(screen->scene);
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
		BKE_workspace_render_layer_set(workspace, layer);

		transform_orientations = BKE_workspace_transform_orientations_get(workspace);
		BLI_duplicatelist(transform_orientations, &screen->scene->transform_spaces);
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

void do_versions_after_linking_280(Main *main)
{
	if (!MAIN_VERSION_ATLEAST(main, 280, 0)) {
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			/* since we don't have access to FileData we check the (always valid) first render layer instead */
			if (scene->render_layers.first == NULL) {
				SceneCollection *sc_master = BKE_collection_master(scene);
				BLI_strncpy(sc_master->name, "Master Collection", sizeof(sc_master->name));

				SceneCollection *collections[20] = {NULL};
				bool is_visible[20];

				int lay_used = 0;
				for (int i = 0; i < 20; i++) {
					char name[MAX_NAME];

					BLI_snprintf(name, sizeof(collections[i]->name), "Collection %d", i + 1);
					collections[i] = BKE_collection_add(scene, sc_master, name);

					is_visible[i] = (scene->lay & (1 << i));
				}

				for (Base *base = scene->base.first; base; base = base->next) {
					lay_used |= base->lay & ((1 << 20) - 1); /* ignore localview */

					for (int i = 0; i < 20; i++) {
						if ((base->lay & (1 << i)) != 0) {
							BKE_collection_object_add(scene, collections[i], base->object);
						}
					}

					if (base->flag & SELECT) {
						base->object->flag |= SELECT;
					}
					else {
						base->object->flag &= ~SELECT;
					}
				}

				scene->active_layer = 0;

				if (!BKE_scene_uses_blender_game(scene)) {
					for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next) {

						SceneLayer *sl = BKE_scene_layer_add(scene, srl->name);
						BKE_scene_layer_engine_set(sl, scene->r.engine);

						if (srl->mat_override) {
							BKE_collection_override_datablock_add((LayerCollection *)sl->layer_collections.first, "material", (ID *)srl->mat_override);
						}

						if (srl->light_override && BKE_scene_uses_blender_internal(scene)) {
							/* not sure how we handle this, pending until we design the override system */
							TODO_LAYER_OVERRIDE;
						}

						if (srl->lay != scene->lay) {
							/* unlink master collection  */
							BKE_collection_unlink(sl, sl->layer_collections.first);

							/* add new collection bases */
							for (int i = 0; i < 20; i++) {
								if ((srl->lay & (1 << i)) != 0) {
									BKE_collection_link(sl, collections[i]);
								}
							}
						}

						/* for convenience set the same active object in all the layers */
						if (scene->basact) {
							sl->basact = BKE_scene_layer_base_find(sl, scene->basact->object);
						}

						for (Base *base = sl->object_bases.first; base; base = base->next) {
							if ((base->flag & BASE_SELECTABLED) && (base->object->flag & SELECT)) {
								base->flag |= BASE_SELECTED;
							}
						}

						/* TODO: passes, samples, mask_layesr, exclude, ... */
					}

					if (BLI_findlink(&scene->render_layers, scene->r.actlay)) {
						scene->active_layer = scene->r.actlay;
					}
				}

				SceneLayer *sl = BKE_scene_layer_add(scene, "Viewport");

				/* In this particular case we can safely assume the data struct */
				LayerCollection *lc = ((LayerCollection *)sl->layer_collections.first)->layer_collections.first;
				for (int i = 0; i < 20; i++) {
					if (!is_visible[i]) {
						lc->flag &= ~COLLECTION_VISIBLE;
					}
					lc = lc->next;
				}

				/* convert active base */
				if (scene->basact) {
					sl->basact = BKE_scene_layer_base_find(sl, scene->basact->object);
				}

				/* convert selected bases */
				for (Base *base = scene->base.first; base; base = base->next) {
					if ((base->flag & BASE_SELECTABLED) && (base->object->flag & SELECT)) {
						base->flag |= BASE_SELECTED;
					}

					/* keep lay around for forward compatibility (open those files in 2.79) */
					base->lay = base->object->lay;
				}

				/* TODO: copy scene render data to layer */

				/* Cleanup */
				for (int i = 0; i < 20; i++) {
					if ((lay_used & (1 << i)) == 0) {
						BKE_collection_remove(scene, collections[i]);
					}
				}

				/* Fallback name if only one layer was found in the original file */
				if (BLI_listbase_count_ex(&sc_master->scene_collections, 2) == 1) {
					BKE_collection_rename(scene, sc_master->scene_collections.first, "Default Collection");
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
			 * so same layer as BKE_scene_layer_context_active would return */
			SceneLayer *layer = screen->scene->render_layers.first;

			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_OUTLINER) {
						SpaceOops *soutliner = (SpaceOops *)sl;

						soutliner->outlinevis = SO_ACT_LAYER;

						if (BLI_listbase_count_ex(&layer->layer_collections, 2) == 1) {
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

void blo_do_versions_280(FileData *fd, Library *UNUSED(lib), Main *main)
{
	if (!MAIN_VERSION_ATLEAST(main, 280, 0)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Scene", "ListBase", "render_layers")) {
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
				for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
					do_version_layer_collections_idproperties(&sl->layer_collections);
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 280, 1)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Lamp", "float", "bleedexp"))	{
			for (Lamp *la = main->lamp.first; la; la = la->id.next) {
				la->bleedexp = 120.0f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "GPUDOFSettings", "float", "ratio"))	{
			for (Camera *ca = main->camera.first; ca; ca = ca->id.next) {
				ca->gpu_dof.ratio = 1.0f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "SceneLayer", "IDProperty", "*properties")) {
			for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
				for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
					IDPropertyTemplate val = {0};
					sl->properties = IDP_New(IDP_GROUP, &val, ROOT_PROP);
					BKE_scene_layer_engine_settings_create(sl->properties);
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
	}

	{
		{
			/* Eevee shader nodes renamed because of the output node system.
			 * Note that a new output node is not being added here, because it would be overkill
			 * to handle this case in lib_verify_nodetree. */
			bool error = false;
			FOREACH_NODETREE(main, ntree, id) {
				if (ntree->type == NTREE_SHADER) {
					for (bNode *node = ntree->nodes.first; node; node = node->next) {
						if (node->type == SH_NODE_EEVEE_METALLIC && STREQ(node->idname, "ShaderNodeOutputMetallic")) {
							BLI_strncpy(node->idname, "ShaderNodeEeveeMetallic", sizeof(node->idname));
							error = true;
						}

						if (node->type == SH_NODE_EEVEE_SPECULAR && STREQ(node->idname, "ShaderNodeOutputSpecular")) {
							BLI_strncpy(node->idname, "ShaderNodeEeveeSpecular", sizeof(node->idname));
							error = true;
						}
					}
				}
			} FOREACH_NODETREE_END
			if (error) {
				BKE_report(fd->reports, RPT_ERROR, "Eevee material conversion problem. Error in console");
				printf("You need to connect Eevee Metallic and Specular shader nodes to new material output nodes.\n");
			}
		}
	}
}
