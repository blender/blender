/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ikplugin
 */

#include "BIK_api.h"

#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ikplugin_api.h"

#ifdef WITH_IK_SOLVER
#  include "iksolver_plugin.h"
#endif

#ifdef WITH_IK_ITASC
#  include "itasc_plugin.h"
#endif

static IKPlugin ikplugin_tab[] = {
#ifdef WITH_IK_SOLVER
    /* Legacy IK solver */
    {
        iksolver_initialize_tree,
        iksolver_execute_tree,
        iksolver_release_tree,
        iksolver_clear_data,
        nullptr,
        nullptr,
        nullptr,
    },
#endif

#ifdef WITH_IK_ITASC
    /* iTaSC IK solver */
    {
        itasc_initialize_tree,
        itasc_execute_tree,
        itasc_release_tree,
        itasc_clear_data,
        itasc_clear_cache,
        itasc_update_param,
        itasc_test_constraint,
    },
#endif

    {nullptr}};

static IKPlugin *get_plugin(bPose *pose)
{
  if (!pose || pose->iksolver < 0 || pose->iksolver >= (ARRAY_SIZE(ikplugin_tab) - 1)) {
    return nullptr;
  }

  return &ikplugin_tab[pose->iksolver];
}

/*----------------------------------------*/
/* Plugin API                             */

void BIK_init_tree(Depsgraph *depsgraph, Scene *scene, Object *ob, float ctime)
{
  IKPlugin *plugin = get_plugin(ob->pose);

  if (plugin && plugin->initialize_tree_func) {
    plugin->initialize_tree_func(depsgraph, scene, ob, ctime);
  }
}

void BIK_execute_tree(
    Depsgraph *depsgraph, Scene *scene, Object *ob, bPoseChannel *pchan, float ctime)
{
  IKPlugin *plugin = get_plugin(ob->pose);

  if (plugin && plugin->execute_tree_func) {
    plugin->execute_tree_func(depsgraph, scene, ob, pchan, ctime);
  }
}

void BIK_release_tree(Scene *scene, Object *ob, float ctime)
{
  IKPlugin *plugin = get_plugin(ob->pose);

  if (plugin && plugin->release_tree_func) {
    plugin->release_tree_func(scene, ob, ctime);
  }
}

void BIK_clear_data(bPose *pose)
{
  IKPlugin *plugin = get_plugin(pose);

  if (plugin && plugin->remove_armature_func) {
    plugin->remove_armature_func(pose);
  }
}

void BIK_clear_cache(bPose *pose)
{
  IKPlugin *plugin = get_plugin(pose);

  if (plugin && plugin->clear_cache) {
    plugin->clear_cache(pose);
  }
}

void BIK_update_param(bPose *pose)
{
  IKPlugin *plugin = get_plugin(pose);

  if (plugin && plugin->update_param) {
    plugin->update_param(pose);
  }
}

void BIK_test_constraint(Object *ob, bConstraint *cons)
{
  IKPlugin *plugin = get_plugin(ob->pose);

  if (plugin && plugin->test_constraint) {
    plugin->test_constraint(ob, cons);
  }
}
