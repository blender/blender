/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Original author: Benoit Bolsee
 * Contributor(s): 
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BIK_api.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_armature.h"
#include "BKE_utildefines.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"
#include "DNA_constraint_types.h"
#include "DNA_armature_types.h"

#include "ikplugin_api.h"
#include "iksolver_plugin.h"
#include "itasc_plugin.h"


static IKPlugin ikplugin_tab[BIK_SOLVER_COUNT] = {
	/* Legacy IK solver */
	{
		iksolver_initialize_tree,
		iksolver_execute_tree,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
	},
	/* iTaSC IK solver */
	{
		itasc_initialize_tree,
		itasc_execute_tree,
		itasc_release_tree,
		itasc_clear_data,
		itasc_clear_cache,
		itasc_update_param,
		itasc_test_constraint,
	}
};


static IKPlugin *get_plugin(bPose *pose)
{
	if (!pose || pose->iksolver < 0 || pose->iksolver >= BIK_SOLVER_COUNT)
		return NULL;

	return &ikplugin_tab[pose->iksolver];
}

/*----------------------------------------*/
/* Plugin API							  */

void BIK_initialize_tree(Scene *scene, Object *ob, float ctime) 
{
	IKPlugin *plugin = get_plugin(ob->pose);

	if (plugin && plugin->initialize_tree_func)
		plugin->initialize_tree_func(scene, ob, ctime);
}

void BIK_execute_tree(struct Scene *scene, Object *ob, bPoseChannel *pchan, float ctime) 
{
	IKPlugin *plugin = get_plugin(ob->pose);

	if (plugin && plugin->execute_tree_func)
		plugin->execute_tree_func(scene, ob, pchan, ctime);
}

void BIK_release_tree(struct Scene *scene, Object *ob, float ctime) 
{
	IKPlugin *plugin = get_plugin(ob->pose);

	if (plugin && plugin->release_tree_func)
		plugin->release_tree_func(scene, ob, ctime);
}

void BIK_clear_data(struct bPose *pose)
{
	IKPlugin *plugin = get_plugin(pose);

	if (plugin && plugin->remove_armature_func)
		plugin->remove_armature_func(pose);
}

void BIK_clear_cache(struct bPose *pose)
{
	IKPlugin *plugin = get_plugin(pose);

	if (plugin && plugin->clear_cache)
		plugin->clear_cache(pose);
}

void BIK_update_param(struct bPose *pose)
{
	IKPlugin *plugin = get_plugin(pose);

	if (plugin && plugin->update_param)
		plugin->update_param(pose);
}

void BIK_test_constraint(struct Object *ob, struct bConstraint *cons)
{
	IKPlugin *plugin = get_plugin(ob->pose);

	if (plugin && plugin->test_constraint)
		plugin->test_constraint(ob, cons);
}
