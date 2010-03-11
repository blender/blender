/**
 * blenlib/BKE_scene.h (mar-2001 nzc)
 *	
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_SCENE_H
#define BKE_SCENE_H

struct bglMats;
struct Scene;
struct Object;
struct Base;
struct Text;
struct AviCodecData;
struct QuicktimeCodecData;
struct RenderData;
struct Text;
struct Main;

#define SCE_COPY_EMPTY		0
#define SCE_COPY_LINK_OB	1
#define SCE_COPY_LINK_DATA	2
#define SCE_COPY_FULL		3

/* note; doesn't work when scene is empty */
#define SETLOOPER(s, b) sce= s, b= (Base*)sce->base.first; b; b= (Base*)(b->next?b->next:sce->set?(sce=sce->set)->base.first:NULL)


void free_avicodecdata(struct AviCodecData *acd);
void free_qtcodecdata(struct QuicktimeCodecData *acd);

void free_scene(struct Scene *sce);
struct Scene *add_scene(char *name);
struct Base *object_in_scene(struct Object *ob, struct Scene *sce);

void set_scene_bg(struct Scene *sce);
void set_scene_name(char *name);

struct Scene *copy_scene(struct Main *bmain, struct Scene *sce, int type);
void unlink_scene(struct Main *bmain, struct Scene *sce, struct Scene *newsce);

int next_object(struct Scene *scene, int val, struct Base **base, struct Object **ob);
struct Object *scene_find_camera(struct Scene *sc);
struct Object *scene_camera_switch_find(struct Scene *scene); // DURIAN_CAMERA_SWITCH
int scene_camera_switch_update(struct Scene *scene);

char *scene_find_marker_name(struct Scene *scene, int frame);
char *scene_find_last_marker_name(struct Scene *scene, int frame);
int scene_marker_tfm_translate(struct Scene *scene, int delta, int flag);
int scene_marker_tfm_extend(struct Scene *scene, int delta, int flag, int frame, char side);

struct Base *scene_add_base(struct Scene *sce, struct Object *ob);
void scene_deselect_all(struct Scene *sce);
void scene_select_base(struct Scene *sce, struct Base *selbase);

/* checks for cycle, returns 1 if it's all OK */
int scene_check_setscene(struct Scene *sce);

void scene_update_tagged(struct Scene *sce);
void scene_update_for_newframe(struct Scene *sce, unsigned int lay);

void scene_add_render_layer(struct Scene *sce);

/* render profile */
int get_render_subsurf_level(struct RenderData *r, int level);
int get_render_child_particle_number(struct RenderData *r, int num);
int get_render_shadow_samples(struct RenderData *r, int samples);
float get_render_aosss_error(struct RenderData *r, float error);

#endif

