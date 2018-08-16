/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_common.c
 *  \ingroup draw
 */

#include "DRW_render.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "UI_resources.h"

#include "BKE_global.h"
#include "BKE_colorband.h"

#include "draw_common.h"

#if 0
#define UI_COLOR_RGB_FROM_U8(r, g, b, v4) \
	ARRAY_SET_ITEMS(v4, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0)
#endif
#define UI_COLOR_RGBA_FROM_U8(r, g, b, a, v4) \
	ARRAY_SET_ITEMS(v4, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f)

/* Colors & Constant */
GlobalsUboStorage ts;
struct GPUUniformBuffer *globals_ubo = NULL;
struct GPUTexture *globals_ramp = NULL;

void DRW_globals_update(void)
{
	UI_GetThemeColor4fv(TH_WIRE, ts.colorWire);
	UI_GetThemeColor4fv(TH_WIRE_EDIT, ts.colorWireEdit);
	UI_GetThemeColor4fv(TH_ACTIVE, ts.colorActive);
	UI_GetThemeColor4fv(TH_SELECT, ts.colorSelect);
	UI_GetThemeColor4fv(TH_TRANSFORM, ts.colorTransform);
	UI_COLOR_RGBA_FROM_U8(0x88, 0xFF, 0xFF, 155, ts.colorLibrarySelect);
	UI_COLOR_RGBA_FROM_U8(0x55, 0xCC, 0xCC, 155, ts.colorLibrary);
	UI_GetThemeColor4fv(TH_LAMP, ts.colorLamp);
	UI_GetThemeColor4fv(TH_SPEAKER, ts.colorSpeaker);
	UI_GetThemeColor4fv(TH_CAMERA, ts.colorCamera);
	UI_GetThemeColor4fv(TH_EMPTY, ts.colorEmpty);
	UI_GetThemeColor4fv(TH_VERTEX, ts.colorVertex);
	UI_GetThemeColor4fv(TH_VERTEX_SELECT, ts.colorVertexSelect);
	UI_GetThemeColor4fv(TH_EDITMESH_ACTIVE, ts.colorEditMeshActive);
	UI_GetThemeColor4fv(TH_EDGE_SELECT, ts.colorEdgeSelect);

	UI_GetThemeColor4fv(TH_EDGE_SEAM, ts.colorEdgeSeam);
	UI_GetThemeColor4fv(TH_EDGE_SHARP, ts.colorEdgeSharp);
	UI_GetThemeColor4fv(TH_EDGE_CREASE, ts.colorEdgeCrease);
	UI_GetThemeColor4fv(TH_EDGE_BEVEL, ts.colorEdgeBWeight);
	UI_GetThemeColor4fv(TH_EDGE_FACESEL, ts.colorEdgeFaceSelect);
	UI_GetThemeColor4fv(TH_FACE, ts.colorFace);
	UI_GetThemeColor4fv(TH_FACE_SELECT, ts.colorFaceSelect);
	UI_GetThemeColor4fv(TH_NORMAL, ts.colorNormal);
	UI_GetThemeColor4fv(TH_VNORMAL, ts.colorVNormal);
	UI_GetThemeColor4fv(TH_LNORMAL, ts.colorLNormal);
	UI_GetThemeColor4fv(TH_FACE_DOT, ts.colorFaceDot);
	UI_GetThemeColor4fv(TH_BACK, ts.colorBackground);

	/* Curve */
	UI_GetThemeColor4fv(TH_HANDLE_FREE, ts.colorHandleFree);
	UI_GetThemeColor4fv(TH_HANDLE_AUTO, ts.colorHandleAuto);
	UI_GetThemeColor4fv(TH_HANDLE_VECT, ts.colorHandleVect);
	UI_GetThemeColor4fv(TH_HANDLE_ALIGN, ts.colorHandleAlign);
	UI_GetThemeColor4fv(TH_HANDLE_AUTOCLAMP, ts.colorHandleAutoclamp);
	UI_GetThemeColor4fv(TH_HANDLE_SEL_FREE, ts.colorHandleSelFree);
	UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTO, ts.colorHandleSelAuto);
	UI_GetThemeColor4fv(TH_HANDLE_SEL_VECT, ts.colorHandleSelVect);
	UI_GetThemeColor4fv(TH_HANDLE_SEL_ALIGN, ts.colorHandleSelAlign);
	UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTOCLAMP, ts.colorHandleSelAutoclamp);
	UI_GetThemeColor4fv(TH_NURB_ULINE, ts.colorNurbUline);
	UI_GetThemeColor4fv(TH_NURB_SEL_ULINE, ts.colorNurbSelUline);
	UI_GetThemeColor4fv(TH_ACTIVE_SPLINE, ts.colorActiveSpline);

	UI_GetThemeColor4fv(TH_BONE_POSE, ts.colorBonePose);

	UI_GetThemeColor4fv(TH_CFRAME, ts.colorCurrentFrame);

	/* Grid */
	UI_GetThemeColorShade4fv(TH_GRID, 10, ts.colorGrid);
	/* emphasise division lines lighter instead of darker, if background is darker than grid */
	UI_GetThemeColorShade4fv(
	        TH_GRID,
	        (ts.colorGrid[0] + ts.colorGrid[1] + ts.colorGrid[2] + 0.12f >
	         ts.colorBackground[0] + ts.colorBackground[1] + ts.colorBackground[2]) ?
	        20 : -10, ts.colorGridEmphasise);
	/* Grid Axis */
	UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_X, 0.5f, -10, ts.colorGridAxisX);
	UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Y, 0.5f, -10, ts.colorGridAxisY);
	UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Z, 0.5f, -10, ts.colorGridAxisZ);

	UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, ts.colorDeselect);
	UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, ts.colorOutline);
	UI_GetThemeColorShadeAlpha4fv(TH_LAMP, 0, 255, ts.colorLampNoAlpha);

	ts.sizeLampCenter = (U.obcenter_dia + 1.5f) * U.pixelsize;
	ts.sizeLampCircle = U.pixelsize * 9.0f;
	ts.sizeLampCircleShadow = ts.sizeLampCircle + U.pixelsize * 3.0f;

	/* M_SQRT2 to be at least the same size of the old square */
	ts.sizeVertex = max_ff(1.0f, UI_GetThemeValuef(TH_VERTEX_SIZE) * (float)M_SQRT2 / 2.0f);
	ts.sizeFaceDot = UI_GetThemeValuef(TH_FACEDOT_SIZE);
	ts.sizeEdge = 1.0f / 2.0f; /* TODO Theme */
	ts.sizeEdgeFix = 0.5f + 2.0f * (2.0f * (MAX2(ts.sizeVertex, ts.sizeEdge)) * (float)M_SQRT1_2);


	if (globals_ubo == NULL) {
		globals_ubo = DRW_uniformbuffer_create(sizeof(GlobalsUboStorage), &ts);
	}

	DRW_uniformbuffer_update(globals_ubo, &ts);

	ColorBand ramp = {0};
	float *colors;
	int col_size;

	ramp.tot = 3;
	ramp.data[0].a = 1.0f;
	ramp.data[0].b = 1.0f;
	ramp.data[0].pos = 0.0f;
	ramp.data[1].a = 1.0f;
	ramp.data[1].g = 1.0f;
	ramp.data[1].pos = 0.5f;
	ramp.data[2].a = 1.0f;
	ramp.data[2].r = 1.0f;
	ramp.data[2].pos = 1.0f;

	BKE_colorband_evaluate_table_rgba(&ramp, &colors, &col_size);

	if (globals_ramp) {
		GPU_texture_free(globals_ramp);
	}
	globals_ramp = GPU_texture_create_1D(col_size, GPU_RGBA8, colors, NULL);

	MEM_freeN(colors);
}

/* ********************************* SHGROUP ************************************* */

extern char datatoc_animviz_mpath_lines_vert_glsl[];
extern char datatoc_animviz_mpath_lines_geom_glsl[];
extern char datatoc_animviz_mpath_points_vert_glsl[];

extern char datatoc_volume_velocity_vert_glsl[];

extern char datatoc_armature_axes_vert_glsl[];
extern char datatoc_armature_sphere_solid_vert_glsl[];
extern char datatoc_armature_sphere_solid_frag_glsl[];
extern char datatoc_armature_sphere_outline_vert_glsl[];
extern char datatoc_armature_envelope_solid_vert_glsl[];
extern char datatoc_armature_envelope_solid_frag_glsl[];
extern char datatoc_armature_envelope_outline_vert_glsl[];
extern char datatoc_armature_envelope_distance_frag_glsl[];
extern char datatoc_armature_shape_solid_vert_glsl[];
extern char datatoc_armature_shape_solid_frag_glsl[];
extern char datatoc_armature_shape_outline_vert_glsl[];
extern char datatoc_armature_shape_outline_geom_glsl[];
extern char datatoc_armature_stick_vert_glsl[];
extern char datatoc_armature_stick_frag_glsl[];

extern char datatoc_common_globals_lib_glsl[];

extern char datatoc_gpu_shader_flat_color_frag_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];
extern char datatoc_gpu_shader_point_varying_color_frag_glsl[];

extern char datatoc_object_mball_handles_vert_glsl[];

static struct {
	struct GPUShader *shape_outline;
	struct GPUShader *shape_solid;
	struct GPUShader *bone_axes;
	struct GPUShader *bone_envelope;
	struct GPUShader *bone_envelope_distance;
	struct GPUShader *bone_envelope_outline;
	struct GPUShader *bone_sphere;
	struct GPUShader *bone_sphere_outline;
	struct GPUShader *bone_stick;

	struct GPUShader *mpath_line_sh;
	struct GPUShader *mpath_points_sh;

	struct GPUShader *volume_velocity_needle_sh;
	struct GPUShader *volume_velocity_sh;

	struct GPUShader *mball_handles;
} g_shaders = {NULL};

static struct {
	struct GPUVertFormat *instance_screenspace;
	struct GPUVertFormat *instance_color;
	struct GPUVertFormat *instance_screen_aligned;
	struct GPUVertFormat *instance_scaled;
	struct GPUVertFormat *instance_sized;
	struct GPUVertFormat *instance_outline;
	struct GPUVertFormat *instance;
	struct GPUVertFormat *instance_camera;
	struct GPUVertFormat *instance_distance_lines;
	struct GPUVertFormat *instance_spot;
	struct GPUVertFormat *instance_bone;
	struct GPUVertFormat *instance_bone_stick;
	struct GPUVertFormat *instance_bone_outline;
	struct GPUVertFormat *instance_bone_envelope;
	struct GPUVertFormat *instance_bone_envelope_distance;
	struct GPUVertFormat *instance_bone_envelope_outline;
	struct GPUVertFormat *instance_mball_handles;
	struct GPUVertFormat *dynlines_color;
} g_formats = {NULL};

void DRW_globals_free(void)
{
	struct GPUVertFormat **format = &g_formats.instance_screenspace;
	for (int i = 0; i < sizeof(g_formats) / sizeof(void *); ++i, ++format) {
		MEM_SAFE_FREE(*format);
	}

	struct GPUShader **shader = &g_shaders.shape_outline;
	for (int i = 0; i < sizeof(g_shaders) / sizeof(void *); ++i, ++shader) {
		DRW_SHADER_FREE_SAFE(*shader);
	}
}

DRWShadingGroup *shgroup_dynlines_flat_color(DRWPass *pass)
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_FLAT_COLOR);

	DRW_shgroup_instance_format(g_formats.dynlines_color, {
		{"pos",       DRW_ATTRIB_FLOAT, 3},
		{"color",     DRW_ATTRIB_FLOAT, 4}
	});

	DRWShadingGroup *grp = DRW_shgroup_line_batch_create_with_format(sh, pass, g_formats.dynlines_color);

	return grp;
}

DRWShadingGroup *shgroup_dynlines_dashed_uniform_color(DRWPass *pass, float color[4])
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

	static float dash_width = 6.0f;
	static float dash_factor = 0.5f;
	DRWShadingGroup *grp = DRW_shgroup_line_batch_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);
	DRW_shgroup_uniform_vec2(grp, "viewport_size", DRW_viewport_size_get(), 1);
	DRW_shgroup_uniform_float(grp, "dash_width", &dash_width, 1);
	DRW_shgroup_uniform_float(grp, "dash_factor", &dash_factor, 1);
	DRW_shgroup_uniform_int_copy(grp, "colors_len", 0); /* "simple" mode */

	return grp;
}

DRWShadingGroup *shgroup_dynpoints_uniform_color(DRWPass *pass, float color[4], float *size)
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

	DRWShadingGroup *grp = DRW_shgroup_point_batch_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);
	DRW_shgroup_uniform_float(grp, "size", size, 1);
	DRW_shgroup_state_enable(grp, DRW_STATE_POINT);

	return grp;
}

DRWShadingGroup *shgroup_groundlines_uniform_color(DRWPass *pass, float color[4])
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_GROUNDLINE);

	DRWShadingGroup *grp = DRW_shgroup_point_batch_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);

	return grp;
}

DRWShadingGroup *shgroup_groundpoints_uniform_color(DRWPass *pass, float color[4])
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_GROUNDPOINT);

	DRWShadingGroup *grp = DRW_shgroup_point_batch_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);
	DRW_shgroup_state_enable(grp, DRW_STATE_POINT);

	return grp;
}

DRWShadingGroup *shgroup_instance_screenspace(DRWPass *pass, struct GPUBatch *geom, float *size)
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_SCREENSPACE_VARIYING_COLOR);

	DRW_shgroup_instance_format(g_formats.instance_screenspace, {
		{"world_pos", DRW_ATTRIB_FLOAT, 3},
		{"color",     DRW_ATTRIB_FLOAT, 3}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh, pass, geom, g_formats.instance_screenspace);
	DRW_shgroup_uniform_float(grp, "size", size, 1);
	DRW_shgroup_uniform_float(grp, "pixel_size", DRW_viewport_pixelsize_get(), 1);
	DRW_shgroup_uniform_vec3(grp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);
	DRW_shgroup_state_enable(grp, DRW_STATE_STIPPLE_3);

	return grp;
}

DRWShadingGroup *shgroup_instance_solid(DRWPass *pass, struct GPUBatch *geom)
{
	static float light[3] = {0.0f, 0.0f, 1.0f};
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_OBJECTSPACE_SIMPLE_LIGHTING_VARIYING_COLOR);

	DRW_shgroup_instance_format(g_formats.instance_color, {
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16},
		{"color",               DRW_ATTRIB_FLOAT, 4}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh, pass, geom, g_formats.instance_color);
	DRW_shgroup_uniform_vec3(grp, "light", light, 1);

	return grp;
}

DRWShadingGroup *shgroup_instance_wire(DRWPass *pass, struct GPUBatch *geom)
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_OBJECTSPACE_VARIYING_COLOR);

	DRW_shgroup_instance_format(g_formats.instance_color, {
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16},
		{"color",               DRW_ATTRIB_FLOAT, 4}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh, pass, geom, g_formats.instance_color);

	return grp;
}

DRWShadingGroup *shgroup_instance_screen_aligned(DRWPass *pass, struct GPUBatch *geom)
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_INSTANCE_SCREEN_ALIGNED);

	DRW_shgroup_instance_format(g_formats.instance_screen_aligned, {
		{"color",               DRW_ATTRIB_FLOAT, 3},
		{"size",                DRW_ATTRIB_FLOAT, 1},
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh, pass, geom, g_formats.instance_screen_aligned);
	DRW_shgroup_uniform_vec3(grp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);

	return grp;
}

DRWShadingGroup *shgroup_instance_axis_names(DRWPass *pass, struct GPUBatch *geom)
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_INSTANCE_SCREEN_ALIGNED_AXIS);

	DRW_shgroup_instance_format(g_formats.instance_screen_aligned, {
		{"color",               DRW_ATTRIB_FLOAT, 3},
		{"size",                DRW_ATTRIB_FLOAT, 1},
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh, pass, geom, g_formats.instance_screen_aligned);
	DRW_shgroup_uniform_vec3(grp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);

	return grp;
}

DRWShadingGroup *shgroup_instance_scaled(DRWPass *pass, struct GPUBatch *geom)
{
	GPUShader *sh_inst = GPU_shader_get_builtin_shader(GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SCALE);

	DRW_shgroup_instance_format(g_formats.instance_scaled, {
		{"color",               DRW_ATTRIB_FLOAT, 3},
		{"size",                DRW_ATTRIB_FLOAT, 3},
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh_inst, pass, geom, g_formats.instance_scaled);

	return grp;
}

DRWShadingGroup *shgroup_instance(DRWPass *pass, struct GPUBatch *geom)
{
	GPUShader *sh_inst = GPU_shader_get_builtin_shader(GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SIZE);

	DRW_shgroup_instance_format(g_formats.instance_sized, {
		{"color",               DRW_ATTRIB_FLOAT, 3},
		{"size",                DRW_ATTRIB_FLOAT, 1},
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh_inst, pass, geom, g_formats.instance_sized);

	return grp;
}

DRWShadingGroup *shgroup_instance_outline(DRWPass *pass, struct GPUBatch *geom, int *baseid)
{
	GPUShader *sh_inst = GPU_shader_get_builtin_shader(GPU_SHADER_INSTANCE_VARIYING_ID_VARIYING_SIZE);

	DRW_shgroup_instance_format(g_formats.instance_outline, {
		{"callId",              DRW_ATTRIB_INT,   1},
		{"size",                DRW_ATTRIB_FLOAT, 1},
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh_inst, pass, geom, g_formats.instance_outline);
	DRW_shgroup_uniform_int(grp, "baseId", baseid, 1);

	return grp;
}

DRWShadingGroup *shgroup_camera_instance(DRWPass *pass, struct GPUBatch *geom)
{
	GPUShader *sh_inst = GPU_shader_get_builtin_shader(GPU_SHADER_CAMERA);

	DRW_shgroup_instance_format(g_formats.instance_camera, {
		{"color",               DRW_ATTRIB_FLOAT, 3},
		{"corners",             DRW_ATTRIB_FLOAT, 8},
		{"depth",               DRW_ATTRIB_FLOAT, 1},
		{"tria",                DRW_ATTRIB_FLOAT, 4},
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh_inst, pass, geom, g_formats.instance_camera);

	return grp;
}

DRWShadingGroup *shgroup_distance_lines_instance(DRWPass *pass, struct GPUBatch *geom)
{
	GPUShader *sh_inst = GPU_shader_get_builtin_shader(GPU_SHADER_DISTANCE_LINES);
	static float point_size = 4.0f;

	DRW_shgroup_instance_format(g_formats.instance_distance_lines, {
		{"color",               DRW_ATTRIB_FLOAT, 3},
		{"start",               DRW_ATTRIB_FLOAT, 1},
		{"end",                 DRW_ATTRIB_FLOAT, 1},
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh_inst, pass, geom, g_formats.instance_distance_lines);
	DRW_shgroup_uniform_float(grp, "size", &point_size, 1);

	return grp;
}

DRWShadingGroup *shgroup_spot_instance(DRWPass *pass, struct GPUBatch *geom)
{
	GPUShader *sh_inst = GPU_shader_get_builtin_shader(GPU_SHADER_INSTANCE_EDGES_VARIYING_COLOR);
	static const int True = true;
	static const int False = false;

	DRW_shgroup_instance_format(g_formats.instance_spot, {
		{"color",               DRW_ATTRIB_FLOAT, 3},
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh_inst, pass, geom, g_formats.instance_spot);
	DRW_shgroup_uniform_bool(grp, "drawFront", &False, 1);
	DRW_shgroup_uniform_bool(grp, "drawBack", &False, 1);
	DRW_shgroup_uniform_bool(grp, "drawSilhouette", &True, 1);

	return grp;
}

DRWShadingGroup *shgroup_instance_bone_axes(DRWPass *pass)
{
	if (g_shaders.bone_axes == NULL) {
		g_shaders.bone_axes = DRW_shader_create(
		            datatoc_armature_axes_vert_glsl, NULL,
		            datatoc_gpu_shader_flat_color_frag_glsl, NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_color, {
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16},
		{"color",               DRW_ATTRIB_FLOAT, 4}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.bone_axes,
	        pass, DRW_cache_bone_arrows_get(),
	        g_formats.instance_color);
	DRW_shgroup_uniform_vec3(grp, "screenVecs[0]", DRW_viewport_screenvecs_get(), 2);

	return grp;
}

DRWShadingGroup *shgroup_instance_bone_envelope_outline(DRWPass *pass)
{
	if (g_shaders.bone_envelope_outline == NULL) {
		g_shaders.bone_envelope_outline = DRW_shader_create(
		            datatoc_armature_envelope_outline_vert_glsl, NULL,
		            datatoc_gpu_shader_flat_color_frag_glsl, NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_bone_envelope_outline, {
		{"headSphere",           DRW_ATTRIB_FLOAT, 4},
		{"tailSphere",           DRW_ATTRIB_FLOAT, 4},
		{"outlineColorSize",     DRW_ATTRIB_FLOAT, 4},
		{"xAxis",                DRW_ATTRIB_FLOAT, 3}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.bone_envelope_outline,
	        pass, DRW_cache_bone_envelope_outline_get(),
	        g_formats.instance_bone_envelope_outline);
	DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);

	return grp;
}

DRWShadingGroup *shgroup_instance_bone_envelope_distance(DRWPass *pass)
{
	if (g_shaders.bone_envelope_distance == NULL) {
		g_shaders.bone_envelope_distance = DRW_shader_create(
		            datatoc_armature_envelope_solid_vert_glsl, NULL,
		            datatoc_armature_envelope_distance_frag_glsl, NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_bone_envelope_distance, {
		{"headSphere",           DRW_ATTRIB_FLOAT, 4},
		{"tailSphere",           DRW_ATTRIB_FLOAT, 4},
		{"xAxis",                DRW_ATTRIB_FLOAT, 3}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.bone_envelope_distance,
	        pass, DRW_cache_bone_envelope_solid_get(),
	        g_formats.instance_bone_envelope_distance);

	return grp;
}

DRWShadingGroup *shgroup_instance_bone_envelope_solid(DRWPass *pass, bool transp)
{
	if (g_shaders.bone_envelope == NULL) {
		g_shaders.bone_envelope = DRW_shader_create(
		            datatoc_armature_envelope_solid_vert_glsl, NULL,
		            datatoc_armature_envelope_solid_frag_glsl, "#define SMOOTH_ENVELOPE\n");
	}

	DRW_shgroup_instance_format(g_formats.instance_bone_envelope, {
		{"headSphere",           DRW_ATTRIB_FLOAT, 4},
		{"tailSphere",           DRW_ATTRIB_FLOAT, 4},
		{"boneColor",            DRW_ATTRIB_FLOAT, 3},
		{"stateColor",           DRW_ATTRIB_FLOAT, 3},
		{"xAxis",                DRW_ATTRIB_FLOAT, 3}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.bone_envelope,
	        pass, DRW_cache_bone_envelope_solid_get(),
	        g_formats.instance_bone_envelope);
	DRW_shgroup_uniform_float_copy(grp, "alpha", transp ? 0.6f : 1.0f);

	return grp;
}

DRWShadingGroup *shgroup_instance_mball_handles(DRWPass *pass)
{
	if (g_shaders.mball_handles == NULL) {
		g_shaders.mball_handles = DRW_shader_create(
		            datatoc_object_mball_handles_vert_glsl, NULL,
		            datatoc_gpu_shader_flat_color_frag_glsl, NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_mball_handles, {
		{"ScaleTranslationMatrix",  DRW_ATTRIB_FLOAT, 12},
		{"radius",                  DRW_ATTRIB_FLOAT, 1},
		{"color",                   DRW_ATTRIB_FLOAT, 3}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.mball_handles, pass,
	        DRW_cache_screenspace_circle_get(),
	        g_formats.instance_mball_handles);
	DRW_shgroup_uniform_vec3(grp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);

	return grp;
}

/* Only works with batches with adjacency infos. */
DRWShadingGroup *shgroup_instance_bone_shape_outline(DRWPass *pass, struct GPUBatch *geom)
{
	if (g_shaders.shape_outline == NULL) {
		g_shaders.shape_outline = DRW_shader_create(
		            datatoc_armature_shape_outline_vert_glsl,
		            datatoc_armature_shape_outline_geom_glsl,
		            datatoc_gpu_shader_flat_color_frag_glsl,
		            NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_bone_outline, {
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16},
		{"outlineColorSize",    DRW_ATTRIB_FLOAT, 4}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.shape_outline,
	        pass, geom, g_formats.instance_bone_outline);
	DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);

	return grp;
}

DRWShadingGroup *shgroup_instance_bone_shape_solid(DRWPass *pass, struct GPUBatch *geom, bool transp)
{
	if (g_shaders.shape_solid == NULL) {
		g_shaders.shape_solid = DRW_shader_create(
		            datatoc_armature_shape_solid_vert_glsl, NULL,
		            datatoc_armature_shape_solid_frag_glsl, NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_bone, {
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16},
		{"boneColor",            DRW_ATTRIB_FLOAT, 3},
		{"stateColor",           DRW_ATTRIB_FLOAT, 3}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.shape_solid,
	        pass, geom, g_formats.instance_bone);
	DRW_shgroup_uniform_float_copy(grp, "alpha", transp ? 0.6f : 1.0f);

	return grp;
}

DRWShadingGroup *shgroup_instance_bone_sphere_solid(DRWPass *pass, bool transp)
{
	if (g_shaders.bone_sphere == NULL) {
		g_shaders.bone_sphere = DRW_shader_create(
		            datatoc_armature_sphere_solid_vert_glsl, NULL,
		            datatoc_armature_sphere_solid_frag_glsl, NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_bone, {
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16},
		{"boneColor",           DRW_ATTRIB_FLOAT, 3},
		{"stateColor",          DRW_ATTRIB_FLOAT, 3}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.bone_sphere,
	        pass, DRW_cache_bone_point_get(), g_formats.instance_bone);
	/* More transparent than the shape to be less distractive. */
	DRW_shgroup_uniform_float_copy(grp, "alpha", transp ? 0.4f : 1.0f);

	return grp;
}

DRWShadingGroup *shgroup_instance_bone_sphere_outline(DRWPass *pass)
{
	if (g_shaders.bone_sphere_outline == NULL) {
		g_shaders.bone_sphere_outline = DRW_shader_create(
		            datatoc_armature_sphere_outline_vert_glsl, NULL,
		            datatoc_gpu_shader_flat_color_frag_glsl, NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_bone_outline, {
		{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16},
		{"outlineColorSize",    DRW_ATTRIB_FLOAT, 4}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.bone_sphere_outline,
	        pass, DRW_cache_bone_point_wire_outline_get(),
	        g_formats.instance_bone_outline);
	DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);

	return grp;
}

DRWShadingGroup *shgroup_instance_bone_stick(DRWPass *pass)
{
	if (g_shaders.bone_stick == NULL) {
		g_shaders.bone_stick = DRW_shader_create(
		            datatoc_armature_stick_vert_glsl, NULL,
		            datatoc_armature_stick_frag_glsl, NULL);
	}

	DRW_shgroup_instance_format(g_formats.instance_bone_stick, {
		{"boneStart", DRW_ATTRIB_FLOAT, 3},
		{"boneEnd",   DRW_ATTRIB_FLOAT, 3},
		{"wireColor", DRW_ATTRIB_FLOAT, 4}, /* TODO port theses to uchar color */
		{"boneColor", DRW_ATTRIB_FLOAT, 4},
		{"headColor", DRW_ATTRIB_FLOAT, 4},
		{"tailColor", DRW_ATTRIB_FLOAT, 4}
	});

	DRWShadingGroup *grp = DRW_shgroup_instance_create(
	        g_shaders.bone_stick,
	        pass, DRW_cache_bone_stick_get(),
	        g_formats.instance_bone_stick);
	DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);

	return grp;
}

struct GPUShader *mpath_line_shader_get(void)
{
	if (g_shaders.mpath_line_sh == NULL) {
		g_shaders.mpath_line_sh = DRW_shader_create_with_lib(
		        datatoc_animviz_mpath_lines_vert_glsl,
		        datatoc_animviz_mpath_lines_geom_glsl,
		        datatoc_gpu_shader_3D_smooth_color_frag_glsl,
		        datatoc_common_globals_lib_glsl,
		        NULL);
	}
	return g_shaders.mpath_line_sh;
}


struct GPUShader *mpath_points_shader_get(void)
{
	if (g_shaders.mpath_points_sh == NULL) {
		g_shaders.mpath_points_sh = DRW_shader_create_with_lib(
		        datatoc_animviz_mpath_points_vert_glsl,
		        NULL,
		        datatoc_gpu_shader_point_varying_color_frag_glsl,
		        datatoc_common_globals_lib_glsl,
		        NULL);
	}
	return g_shaders.mpath_points_sh;
}

struct GPUShader *volume_velocity_shader_get(bool use_needle)
{
	if (use_needle) {
		if (g_shaders.volume_velocity_needle_sh == NULL) {
			g_shaders.volume_velocity_needle_sh = DRW_shader_create(
			        datatoc_volume_velocity_vert_glsl, NULL,
			        datatoc_gpu_shader_flat_color_frag_glsl, "#define USE_NEEDLE");
		}
		return g_shaders.volume_velocity_needle_sh;
	}
	else {
		if (g_shaders.volume_velocity_sh == NULL) {
			g_shaders.volume_velocity_sh = DRW_shader_create(
			        datatoc_volume_velocity_vert_glsl, NULL,
			        datatoc_gpu_shader_flat_color_frag_glsl, NULL);
		}
		return g_shaders.volume_velocity_sh;
	}
}

/* ******************************************** COLOR UTILS *********************************************** */

/* TODO FINISH */
/**
 * Get the wire color theme_id of an object based on it's state
 * \a r_color is a way to get a pointer to the static color var associated
 */
int DRW_object_wire_theme_get(Object *ob, ViewLayer *view_layer, float **r_color)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const bool is_edit = (draw_ctx->object_mode & OB_MODE_EDIT) != 0;
	const bool active = (view_layer->basact && view_layer->basact->object == ob);
	/* confusing logic here, there are 2 methods of setting the color
	 * 'colortab[colindex]' and 'theme_id', colindex overrides theme_id.
	 *
	 * note: no theme yet for 'colindex' */
	int theme_id = is_edit ? TH_WIRE_EDIT : TH_WIRE;

	if (//(scene->obedit == NULL) &&
	    ((G.moving & G_TRANSFORM_OBJ) != 0) &&
	    ((ob->base_flag & BASE_SELECTED) != 0))
	{
		theme_id = TH_TRANSFORM;
	}
	else {
		/* Sets the 'theme_id' or fallback to wire */
		if ((ob->base_flag & BASE_SELECTED) != 0) {
			theme_id = (active) ? TH_ACTIVE : TH_SELECT;
		}
		else {
			if (ob->type == OB_LAMP) theme_id = TH_LAMP;
			else if (ob->type == OB_SPEAKER) theme_id = TH_SPEAKER;
			else if (ob->type == OB_CAMERA) theme_id = TH_CAMERA;
			else if (ob->type == OB_EMPTY) theme_id = TH_EMPTY;
			else if (ob->type == OB_LIGHTPROBE) theme_id = TH_EMPTY; /* TODO add lightprobe color */
			/* fallback to TH_WIRE */
		}
	}

	if (r_color != NULL) {
		switch (theme_id) {
			case TH_WIRE_EDIT:    *r_color = ts.colorWireEdit; break;
			case TH_ACTIVE:       *r_color = ts.colorActive; break;
			case TH_SELECT:       *r_color = ts.colorSelect; break;
			case TH_TRANSFORM:    *r_color = ts.colorTransform; break;
			case TH_SPEAKER:      *r_color = ts.colorSpeaker; break;
			case TH_CAMERA:       *r_color = ts.colorCamera; break;
			case TH_EMPTY:        *r_color = ts.colorEmpty; break;
			case TH_LAMP:         *r_color = ts.colorLamp; break;
			default:              *r_color = ts.colorWire; break;
		}
	}

	return theme_id;
}

/* XXX This is utter shit, better find something more general */
float *DRW_color_background_blend_get(int theme_id)
{
	static float colors[11][4];
	float *ret;

	switch (theme_id) {
		case TH_WIRE_EDIT:    ret = colors[0]; break;
		case TH_ACTIVE:       ret = colors[1]; break;
		case TH_SELECT:       ret = colors[2]; break;
		case TH_TRANSFORM:    ret = colors[5]; break;
		case TH_SPEAKER:      ret = colors[6]; break;
		case TH_CAMERA:       ret = colors[7]; break;
		case TH_EMPTY:        ret = colors[8]; break;
		case TH_LAMP:         ret = colors[9]; break;
		default:              ret = colors[10]; break;
	}

	UI_GetThemeColorBlendShade4fv(theme_id, TH_BACK, 0.5, 0, ret);

	return ret;
}
