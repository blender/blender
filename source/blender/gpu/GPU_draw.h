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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_draw.h
 *  \ingroup gpu
 */

#ifndef __GPU_DRAW_H__
#define __GPU_DRAW_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;
struct Image;
struct ImageUser;
struct MTexPoly;
struct Object;
struct Scene;
struct View3D;
struct RegionView3D;
struct SmokeModifierData;
struct DupliObject;

/* OpenGL drawing functions related to shading. These are also
 * shared with the game engine, where there were previously
 * duplicates of some of these functions. */

/* Initialize
 * - sets the default Blender opengl state, if in doubt, check
 *   the contents of this function
 * - this is called when starting Blender, for opengl rendering,
 *   and for switching back from the game engine for example. */

void GPU_state_init(void);

/* Material drawing
 * - first the state is initialized by a particular object and
 *   it's materials
 * - after this, materials can be quickly enabled by their number,
 *   GPU_object_material_bind returns 0 if drawing should be skipped
 * - after drawing, the material must be disabled again */

void GPU_begin_object_materials(struct View3D *v3d, struct RegionView3D *rv3d, 
                                struct Scene *scene, struct Object *ob, bool glsl, bool *do_alpha_after);
void GPU_end_object_materials(void);
bool GPU_object_materials_check(void);

int GPU_object_material_bind(int nr, void *attribs);
void GPU_object_material_unbind(void);
int GPU_object_material_visible(int nr, void *attribs);

void GPU_begin_dupli_object(struct DupliObject *dob);
void GPU_end_dupli_object(void);

void GPU_material_diffuse_get(int nr, float diff[4]);
bool GPU_material_use_matcaps_get(void);

void GPU_set_material_alpha_blend(int alphablend);
int GPU_get_material_alpha_blend(void);

/* TexFace drawing
 * - this is mutually exclusive with material drawing, a mesh should
 *   be drawn using one or the other
 * - passing NULL clears the state again */

int GPU_set_tpage(struct MTexPoly *mtexpoly, int mipmap, int transp);
void GPU_clear_tpage(bool force);

/* Lights
 * - returns how many lights were enabled
 * - this affects fixed functions materials and texface, not glsl */

int GPU_default_lights(void);
int GPU_scene_object_lights(
        struct Scene *scene, struct Object *ob,
        int lay, float viewmat[4][4], int ortho);

/* Text render
 * - based on moving uv coordinates */

void GPU_render_text(
        struct MTexPoly *mtexpoly, int mode,
        const char *textstr, int textlen, unsigned int *col,
        const float *v_quad[4], const float *uv_quad[4],
        int glattrib);

/* Mipmap settings
 * - these will free textures on changes */

void GPU_set_mipmap(bool mipmap);
bool GPU_get_mipmap(void);
void GPU_set_linear_mipmap(bool linear);
bool GPU_get_linear_mipmap(void);
void GPU_paint_set_mipmap(bool mipmap);

/* Anisotropic filtering settings
 * - these will free textures on changes */
void GPU_set_anisotropic(float value);
float GPU_get_anisotropic(void);

/* enable gpu mipmapping */
void GPU_set_gpu_mipmapping(int gpu_mipmap);

/* Image updates and free
 * - these deal with images bound as opengl textures */

void GPU_paint_update_image(struct Image *ima, struct ImageUser *iuser, int x, int y, int w, int h);
void GPU_update_images_framechange(void);
int GPU_update_image_time(struct Image *ima, double time);
int GPU_verify_image(
        struct Image *ima, struct ImageUser *iuser,
        int textarget, int tftile, bool compare, bool mipmap, bool is_data);
void GPU_create_gl_tex(
        unsigned int *bind, unsigned int *rect, float *frect, int rectw, int recth,
        int textarget, bool mipmap, bool use_hight_bit_depth, struct Image *ima);
void GPU_create_gl_tex_compressed(
        unsigned int *bind, unsigned int *pix, int x, int y, int mipmap,
        int textarget, struct Image *ima, struct ImBuf *ibuf);
bool GPU_upload_dxt_texture(struct ImBuf *ibuf);
void GPU_free_image(struct Image *ima);
void GPU_free_images(void);
void GPU_free_images_anim(void);
void GPU_free_images_old(void);

/* smoke drawing functions */
void GPU_free_smoke(struct SmokeModifierData *smd);
void GPU_create_smoke(struct SmokeModifierData *smd, int highres);

/* Delayed free of OpenGL buffers by main thread */
void GPU_free_unused_buffers(void);

#ifdef WITH_OPENSUBDIV
struct DerivedMesh;
void GPU_draw_update_fvar_offset(struct DerivedMesh *dm);
#endif

/* utilities */
void	GPU_select_index_set(int index);
void	GPU_select_index_get(int index, int *r_col);
int		GPU_select_to_index(unsigned int col);
void	GPU_select_to_index_array(unsigned int *col, const unsigned int size);

#ifdef __cplusplus
}
#endif

#endif

