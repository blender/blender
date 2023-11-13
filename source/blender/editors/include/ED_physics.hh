/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ReportList;
struct bContext;
struct wmKeyConfig;
struct Object;
struct Scene;

/* `particle_edit.cc` */

bool PE_poll(bContext *C);
bool PE_hair_poll(bContext *C);
bool PE_poll_view3d(bContext *C);

/* `rigidbody_object.cc` */

bool ED_rigidbody_object_add(Main *bmain, Scene *scene, Object *ob, int type, ReportList *reports);
void ED_rigidbody_object_remove(Main *bmain, Scene *scene, Object *ob);

/* `rigidbody_constraint.cc` */

bool ED_rigidbody_constraint_add(
    Main *bmain, Scene *scene, Object *ob, int type, ReportList *reports);
void ED_rigidbody_constraint_remove(Main *bmain, Scene *scene, Object *ob);

/* operators */
void ED_operatortypes_physics();
void ED_keymap_physics(wmKeyConfig *keyconf);
