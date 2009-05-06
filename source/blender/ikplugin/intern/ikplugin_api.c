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
#include "BLI_arithb.h"

#include "BKE_armature.h"
#include "BKE_utildefines.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_armature_types.h"

#include "ikplugin_api.h"
#include "iksolver_plugin.h"


static IKPlugin ikplugin_tab[BIK_SOLVER_COUNT] = {
	/* Legacy IK solver */
	{
		iksolver_initialize_tree,
		iksolver_execute_tree,
		NULL
	}
};


/*----------------------------------------*/
/* Plugin API							  */

void BIK_initialize_tree(Object *ob, float ctime) 
{
	bArmature *arm;
	IKPlugin *plugin;

	arm = get_armature(ob);
	if (arm->iksolver < 0 || arm->iksolver >= BIK_SOLVER_COUNT)
		return;

	plugin = &ikplugin_tab[arm->iksolver];
	if (plugin->initialize_tree_func)
		plugin->initialize_tree_func(ob, ctime);
}

void BIK_execute_tree(Object *ob, bPoseChannel *pchan, float ctime) 
{
	bArmature *arm;
	IKPlugin *plugin;

	arm = get_armature(ob);
	if (arm->iksolver < 0 || arm->iksolver >= BIK_SOLVER_COUNT)
		return;

	plugin = &ikplugin_tab[arm->iksolver];
	if (plugin->execute_tree_func)
		plugin->execute_tree_func(ob, pchan, ctime);
}

void BIK_release_tree(Object *ob, float ctime) 
{
	bArmature *arm;
	IKPlugin *plugin;

	arm = get_armature(ob);
	if (arm->iksolver < 0 || arm->iksolver >= BIK_SOLVER_COUNT)
		return;

	plugin = &ikplugin_tab[arm->iksolver];
	if (plugin->release_tree_func)
		plugin->release_tree_func(ob, ctime);
}
