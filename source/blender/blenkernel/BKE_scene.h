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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_SCENE_H__
#define __BKE_SCENE_H__

/** \file BKE_scene.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

#ifdef __cplusplus
extern "C" {
#endif

struct AviCodecData;
struct Base;
struct bglMats;
struct Main;
struct Object;
struct QuicktimeCodecData;
struct RenderData;
struct SceneRenderLayer;
struct Scene;
struct Text;
struct Text;

#define SCE_COPY_NEW		0
#define SCE_COPY_EMPTY		1
#define SCE_COPY_LINK_OB	2
#define SCE_COPY_LINK_DATA	3
#define SCE_COPY_FULL		4

#define SETLOOPER(_sce_basis, _sce_iter, _base) _sce_iter= _sce_basis, _base= _setlooper_base_step(&_sce_iter, NULL); _base; _base= _setlooper_base_step(&_sce_iter, _base)
struct Base *_setlooper_base_step(struct Scene **sce_iter, struct Base *base);

void free_avicodecdata(struct AviCodecData *acd);
void free_qtcodecdata(struct QuicktimeCodecData *acd);

void free_scene(struct Scene *sce);
struct Scene *add_scene(const char *name);
struct Base *object_in_scene(struct Object *ob, struct Scene *sce);

void set_scene_bg(struct Main *bmain, struct Scene *sce);
struct Scene *set_scene_name(struct Main *bmain, const char *name);

struct Scene *copy_scene(struct Scene *sce, int type);
void unlink_scene(struct Main *bmain, struct Scene *sce, struct Scene *newsce);

int next_object(struct Scene **scene, int val, struct Base **base, struct Object **ob);
struct Object *scene_find_camera(struct Scene *sc);
struct Object *scene_camera_switch_find(struct Scene *scene); // DURIAN_CAMERA_SWITCH
int scene_camera_switch_update(struct Scene *scene);

char *scene_find_marker_name(struct Scene *scene, int frame);
char *scene_find_last_marker_name(struct Scene *scene, int frame);

struct Base *scene_add_base(struct Scene *sce, struct Object *ob);
void scene_deselect_all(struct Scene *sce);
void scene_select_base(struct Scene *sce, struct Base *selbase);

/* checks for cycle, returns 1 if it's all OK */
int scene_check_setscene(struct Main *bmain, struct Scene *sce);

float BKE_curframe(struct Scene *scene);
float BKE_frame_to_ctime(struct Scene *scene, const float frame);

void scene_update_tagged(struct Main *bmain, struct Scene *sce);

void scene_update_for_newframe(struct Main *bmain, struct Scene *sce, unsigned int lay);

struct SceneRenderLayer *scene_add_render_layer(struct Scene *sce, const char *name);
int scene_remove_render_layer(struct Main *main, struct Scene *scene, struct SceneRenderLayer *srl);

/* render profile */
int get_render_subsurf_level(struct RenderData *r, int level);
int get_render_child_particle_number(struct RenderData *r, int num);
int get_render_shadow_samples(struct RenderData *r, int samples);
float get_render_aosss_error(struct RenderData *r, float error);

int scene_use_new_shading_nodes(struct Scene *scene);

#ifdef __cplusplus
}
#endif

#endif

