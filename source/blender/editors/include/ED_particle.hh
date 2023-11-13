/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct Object;
struct PTCacheEdit;
struct ParticleEditSettings;
struct ParticleSystem;
struct Scene;
struct SelectPick_Params;
struct UndoType;
struct ViewLayer;
struct bContext;
struct rcti;
struct wmGenericUserData;

/* particle edit mode */

void PE_free_ptcache_edit(PTCacheEdit *edit);
int PE_start_edit(PTCacheEdit *edit);

/* access */

PTCacheEdit *PE_get_current_from_psys(ParticleSystem *psys);
PTCacheEdit *PE_get_current(Depsgraph *depsgraph, Scene *scene, Object *ob);
PTCacheEdit *PE_create_current(Depsgraph *depsgraph, Scene *scene, Object *ob);
void PE_current_changed(Depsgraph *depsgraph, Scene *scene, Object *ob);
int PE_minmax(
    Depsgraph *depsgraph, Scene *scene, ViewLayer *view_layer, float min[3], float max[3]);
ParticleEditSettings *PE_settings(Scene *scene);

/* update calls */

void PE_hide_keys_time(Scene *scene, PTCacheEdit *edit, float cfra);
void PE_update_object(Depsgraph *depsgraph, Scene *scene, Object *ob, int useflag);

/* selection tools */

bool PE_mouse_particles(bContext *C, const int mval[2], const SelectPick_Params *params);
bool PE_box_select(bContext *C, const rcti *rect, int sel_op);
bool PE_circle_select(
    bContext *C, wmGenericUserData *wm_userdata, int sel_op, const int mval[2], float rad);
int PE_lasso_select(bContext *C, const int mcoords[][2], int mcoords_len, int sel_op);
bool PE_deselect_all_visible_ex(PTCacheEdit *edit);
bool PE_deselect_all_visible(bContext *C);

/* `particle_edit_undo.cc` */

/** Export for ED_undo_sys. */
void ED_particle_undosys_type(UndoType *ut);
