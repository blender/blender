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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/depsgraph.c
 *  \ingroup bke
 */

 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_query.h"

/* ************************ DAG ********************* */

/**
 * Get the object as properly evaluated by depsgraph.
 */
Object *DAG_get_object(Depsgraph *depsgraph, Object *ob)
{
	/* XXX TODO */
	UNUSED_VARS(depsgraph);
	return ob;
}

/* ************************************
 * This functions are to be supported *
 * ************************************
 */

void DAG_init(void)
{
	DEG_register_node_types();
}

void DAG_exit(void)
{
	DEG_free_node_types();
}

/* ************************ API *********************** */

void DAG_editors_update_cb(DEG_EditorUpdateIDCb id_func,
                           DEG_EditorUpdateSceneCb scene_func,
                           DEG_EditorUpdateScenePreCb scene_func_pre)
{
	DEG_editors_set_update_cb(id_func, scene_func, scene_func_pre);
}

void DAG_editors_update_pre(Main *bmain, Scene *scene, bool time)
{
	DEG_editors_update_pre(bmain, scene, time);
}

/* Tag all relations for update. */
void DAG_relations_tag_update(Main *bmain)
{
	DEG_relations_tag_update(bmain);
}

/* Rebuild dependency graph only for a given scene. */
void DAG_scene_relations_rebuild(Main *bmain, Scene *scene)
{
	DEG_scene_relations_rebuild(bmain, scene);
}

/* Create dependency graph if it was cleared or didn't exist yet. */
void DAG_scene_relations_update(Main *bmain, Scene *scene)
{
	DEG_scene_relations_update(bmain, scene);
}

void DAG_scene_relations_validate(Main *bmain, Scene *scene)
{
	DEG_debug_scene_relations_validate(bmain, scene);
}

void DAG_scene_free(Scene *scene)
{
	DEG_scene_graph_free(scene);
}

void DAG_on_visible_update(Main *bmain, const bool do_time)
{
	DEG_on_visible_update(bmain, do_time);
}

void DAG_ids_check_recalc(Main *bmain, Scene *scene, bool time)
{
	DEG_ids_check_recalc(bmain, scene, time);
}

void DAG_id_tag_update(ID *id, short flag)
{
	DEG_id_tag_update_ex(G.main, id, flag);
}

void DAG_id_tag_update_ex(Main *bmain, ID *id, short flag)
{
	DEG_id_tag_update_ex(bmain, id, flag);
}

void DAG_id_type_tag(Main *bmain, short idtype)
{
	DEG_id_type_tag(bmain, idtype);
}

int DAG_id_type_tagged(Main *bmain, short idtype)
{
	return DEG_id_type_tagged(bmain, idtype);
}

void DAG_ids_clear_recalc(Main *bmain)
{
	DEG_ids_clear_recalc(bmain);
}

short DAG_get_eval_flags_for_object(Scene *scene, void *object)
{
	return DEG_get_eval_flags_for_id(scene->depsgraph, (ID *)object);
}

void DAG_ids_flush_tagged(Main *bmain)
{
	DEG_ids_flush_tagged(bmain);
}

/* ************************ DAG DEBUGGING ********************* */

void DAG_print_dependencies(Main *UNUSED(bmain),
                            Scene *scene,
                            Object *UNUSED(ob))
{
	DEG_debug_graphviz(scene->depsgraph, stdout, "Depsgraph", false);
}
