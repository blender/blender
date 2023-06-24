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

void itasc_initialize_tree(struct Depsgraph *depsgraph,
                           struct Scene *scene,
                           struct Object *ob,
                           float ctime);
void itasc_execute_tree(struct Depsgraph *depsgraph,
                        struct Scene *scene,
                        struct Object *ob,
                        struct bPoseChannel *pchan_root,
                        float ctime);
void itasc_release_tree(struct Scene *scene, struct Object *ob, float ctime);
void itasc_clear_data(struct bPose *pose);
void itasc_clear_cache(struct bPose *pose);
void itasc_update_param(struct bPose *pose);
void itasc_test_constraint(struct Object *ob, struct bConstraint *cons);

#ifdef __cplusplus
}
#endif
