/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file ED_particle.h
 *  \ingroup editors
 */

#ifndef __ED_PARTICLE_H__
#define __ED_PARTICLE_H__

struct bContext;
struct Object;
struct ParticleEditSettings;
struct rcti;
struct PTCacheEdit;
struct Scene;
struct UndoType;

/* particle edit mode */
void PE_free_ptcache_edit(struct PTCacheEdit *edit);
int PE_start_edit(struct PTCacheEdit *edit);

/* access */
struct PTCacheEdit *PE_get_current(struct Main *bmain, struct Scene *scene, struct Object *ob);
struct PTCacheEdit *PE_create_current(struct Main *bmain, struct Scene *scene, struct Object *ob);
void PE_current_changed(struct Main *bmain, struct Scene *scene, struct Object *ob);
int PE_minmax(struct Main *bmain, struct Scene *scene, float min[3], float max[3]);
struct ParticleEditSettings *PE_settings(struct Scene *scene);

/* update calls */
void PE_hide_keys_time(struct Scene *scene, struct PTCacheEdit *edit, float cfra);
void PE_update_object(struct Main *bmain, struct Scene *scene, struct Object *ob, int useflag);

/* selection tools */
int PE_mouse_particles(struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);
int PE_border_select(struct bContext *C, const struct rcti *rect, bool select, bool extend);
int PE_circle_select(struct bContext *C, int selecting, const int mval[2], float rad);
int PE_lasso_select(struct bContext *C, const int mcords[][2], const short moves, bool extend, bool select);
void PE_deselect_all_visible(struct PTCacheEdit *edit);

/* particle_edit_undo.c */
void ED_particle_undosys_type(struct UndoType *ut);

#endif /* __ED_PARTICLE_H__ */
