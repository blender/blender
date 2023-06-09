/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ikplugin
 */

#pragma once

#include "ikplugin_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void iksolver_initialize_tree(struct Depsgraph *depsgraph,
                              struct Scene *scene,
                              struct Object *ob,
                              float ctime);
void iksolver_execute_tree(struct Depsgraph *depsgraph,
                           struct Scene *scene,
                           struct Object *ob,
                           struct bPoseChannel *pchan_root,
                           float ctime);
void iksolver_release_tree(struct Scene *scene, struct Object *ob, float ctime);
void iksolver_clear_data(struct bPose *pose);

#ifdef __cplusplus
}
#endif
