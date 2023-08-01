/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ReportList;
struct bContext;
struct wmKeyConfig;

struct Object;
struct Scene;

/* `particle_edit.cc` */

bool PE_poll(struct bContext *C);
bool PE_hair_poll(struct bContext *C);
bool PE_poll_view3d(struct bContext *C);

/* `rigidbody_object.cc` */

bool ED_rigidbody_object_add(struct Main *bmain,
                             struct Scene *scene,
                             struct Object *ob,
                             int type,
                             struct ReportList *reports);
void ED_rigidbody_object_remove(struct Main *bmain, struct Scene *scene, struct Object *ob);

/* `rigidbody_constraint.cc` */

bool ED_rigidbody_constraint_add(struct Main *bmain,
                                 struct Scene *scene,
                                 struct Object *ob,
                                 int type,
                                 struct ReportList *reports);
void ED_rigidbody_constraint_remove(struct Main *bmain, struct Scene *scene, struct Object *ob);

/* operators */
void ED_operatortypes_physics(void);
void ED_keymap_physics(struct wmKeyConfig *keyconf);

#ifdef __cplusplus
}
#endif
