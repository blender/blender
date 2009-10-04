/* 
 * $Id$
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

struct bContext;
struct Object;
struct ParticleEditSettings;
struct ParticleSystem;
struct RadialControl;
struct rcti;
struct wmWindowManager;
struct PTCacheEdit;
struct Scene;

/* particle edit mode */
void PE_free_ptcache_edit(struct PTCacheEdit *edit);
int PE_start_edit(struct PTCacheEdit *edit);

/* access */
struct PTCacheEdit *PE_get_current(struct Scene *scene, struct Object *ob);
int PE_minmax(struct Scene *scene, float *min, float *max);
struct ParticleEditSettings *PE_settings(struct Scene *scene);

/* update calls */
void PE_hide_keys_time(struct Scene *scene, struct PTCacheEdit *edit, float cfra);
void PE_update_object(struct Scene *scene, struct Object *ob, int useflag);

/* selection tools */
int PE_mouse_particles(struct bContext *C, short *mval, int extend);
int PE_border_select(struct bContext *C, struct rcti *rect, int select);
int PE_circle_select(struct bContext *C, int selecting, short *mval, float rad);
int PE_lasso_select(struct bContext *C, short mcords[][2], short moves, short select);

/* undo */
void PE_undo_push(struct Scene *scene, char *str);
void PE_undo_step(struct Scene *scene, int step);
void PE_undo(struct Scene *scene);
void PE_redo(struct Scene *scene);
void PE_undo_menu(struct Scene *scene, struct Object *ob);

#endif /* ED_PARTICLE_H */

