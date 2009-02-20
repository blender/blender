/* 
 * $Id: ED_editparticle.h $
 *
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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_PARTICLE_H
#define ED_PARTICLE_H

struct Object;
struct ParticleSystem;
struct ParticleEditSettings;
struct RadialControl;
struct ViewContext;
struct rcti;
struct wmWindowManager;

/* particle edit mode */
void PE_set_particle_edit(struct Scene *scene);
void PE_create_particle_edit(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);
void PE_free_particle_edit(struct ParticleSystem *psys);

void PE_change_act(void *ob_v, void *act_v);
void PE_change_act_psys(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);
int PE_can_edit(struct ParticleSystem *psys);

/* access */
struct ParticleSystem *PE_get_current(struct Scene *scene, struct Object *ob);
short PE_get_current_num(struct Object *ob);
int PE_minmax(struct Scene *scene, float *min, float *max);
void PE_get_colors(char sel[4], char nosel[4]);
struct ParticleEditSettings *PE_settings(Scene *scene);
struct RadialControl **PE_radialcontrol(void);

/* update calls */
void PE_hide_keys_time(struct Scene *scene, struct ParticleSystem *psys, float cfra);
void PE_update_object(struct Scene *scene, struct Object *ob, int useflag);
void PE_update_selection(struct Scene *scene, struct Object *ob, int useflag);
void PE_recalc_world_cos(struct Object *ob, struct ParticleSystem *psys);

/* selection tools */
void PE_select_root(void);
void PE_select_tip(void);
void PE_deselectall(void);
void PE_select_linked(void);
void PE_select_less(void);
void PE_select_more(void);

void PE_mouse_particles(void);
void PE_border_select(struct ViewContext *vc, struct rcti *rect, int select);
void PE_circle_select(struct ViewContext *vc, int selecting, short *mval, float rad);
void PE_lasso_select(struct ViewContext *vc, short mcords[][2], short moves, short select);

/* tools */
void PE_hide(int mode);
void PE_rekey(void);
void PE_subdivide(Object *ob);
int PE_brush_particles(void);
void PE_remove_doubles(void);
void PE_selectbrush_menu(Scene *scene);
void PE_remove_doubles(void);
void PE_radialcontrol_start(const int mode);

/* undo */
void PE_undo_push(Scene *scene, char *str);
void PE_undo_step(Scene *scene, int step);
void PE_undo(Scene *scene);
void PE_redo(Scene *scene);
void PE_undo_menu(Scene *scene, Object *ob);

/* operators */
void ED_operatortypes_particle(void);
void ED_keymap_particle(struct wmWindowManager *wm);

#endif /* ED_PARTICLE_H */

