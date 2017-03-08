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

/** \file blender/draw/modes/object_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_userdef_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_camera.h"
#include "BKE_global.h"

#include "GPU_shader.h"

#include "UI_resources.h"

#include "draw_mode_engines.h"
#include "draw_common.h"

/* keep it under MAX_PASSES */
typedef struct OBJECT_PassList {
	struct DRWPass *non_meshes;
	struct DRWPass *ob_center;
	struct DRWPass *wire_outline;
	struct DRWPass *bone_solid;
	struct DRWPass *bone_wire;
} OBJECT_PassList;

typedef struct OBJECT_Data {
	char engine_name[32];
	void *fbl;
	void *txl;
	OBJECT_PassList *psl;
	void *stl;
} OBJECT_Data;

/* Empties */
static DRWShadingGroup *plain_axes;
static DRWShadingGroup *cube;
static DRWShadingGroup *circle;
static DRWShadingGroup *sphere;
static DRWShadingGroup *cone;
static DRWShadingGroup *single_arrow;
static DRWShadingGroup *single_arrow_line;
static DRWShadingGroup *arrows;
static DRWShadingGroup *axis_names;

/* Speaker */
static DRWShadingGroup *speaker;

/* Lamps */
static DRWShadingGroup *lamp_center;
static DRWShadingGroup *lamp_center_group;
static DRWShadingGroup *lamp_groundpoint;
static DRWShadingGroup *lamp_groundline;
static DRWShadingGroup *lamp_circle;
static DRWShadingGroup *lamp_circle_shadow;
static DRWShadingGroup *lamp_sunrays;
static DRWShadingGroup *lamp_distance;
static DRWShadingGroup *lamp_buflimit;
static DRWShadingGroup *lamp_buflimit_points;
static DRWShadingGroup *lamp_area;
static DRWShadingGroup *lamp_hemi;
static DRWShadingGroup *lamp_spot_cone;
static DRWShadingGroup *lamp_spot_blend;
static DRWShadingGroup *lamp_spot_pyramid;
static DRWShadingGroup *lamp_spot_blend_rect;

/* Helpers */
static DRWShadingGroup *relationship_lines;

/* Objects Centers */
static DRWShadingGroup *center_active;
static DRWShadingGroup *center_selected;
static DRWShadingGroup *center_deselected;

/* Camera */
static DRWShadingGroup *camera;
static DRWShadingGroup *camera_tria;
static DRWShadingGroup *camera_focus;
static DRWShadingGroup *camera_clip;
static DRWShadingGroup *camera_clip_points;
static DRWShadingGroup *camera_mist;
static DRWShadingGroup *camera_mist_points;

extern GlobalsUboStorage ts;

static OBJECT_Data *vedata;

static void OBJECT_cache_init(void)
{
	/* DRW_viewport_engine_data_get is rather slow, better not do it on every objects */
	vedata = DRW_viewport_engine_data_get("ObjectMode");
	OBJECT_PassList *psl = vedata->psl;

	{
		/* This pass can draw mesh outlines and/or fancy wireframe */
		/* Fancy wireframes are not meant to be occluded (without Z offset) */
		/* Outlines and Fancy Wires use the same VBO */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		psl->wire_outline = DRW_pass_create("Wire + Outlines Pass", state);
	}

	{
		/* Solid bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->bone_solid = DRW_pass_create("Bone Solid Pass", state);
	}

	{
		/* Wire bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		psl->bone_wire = DRW_pass_create("Bone Wire Pass", state);
	}

	{
		/* Non Meshes Pass (Camera, empties, lamps ...) */
		struct Batch *geom;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND | DRW_STATE_POINT;
		state |= DRW_STATE_WIRE;
		psl->non_meshes = DRW_pass_create("Non Meshes Pass", state);

		/* Empties */
		geom = DRW_cache_plain_axes_get();
		plain_axes = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_cube_get();
		cube = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_circle_get();
		circle = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_empty_sphere_get();
		sphere = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_empty_cone_get();
		cone = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_arrow_get();
		single_arrow = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		single_arrow_line = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_arrows_get();
		arrows = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_axis_names_get();
		axis_names = shgroup_instance_axis_names(psl->non_meshes, geom);

		/* Speaker */
		geom = DRW_cache_speaker_get();
		speaker = shgroup_instance(psl->non_meshes, geom);

		/* Camera */
		geom = DRW_cache_camera_get();
		camera = shgroup_camera_instance(psl->non_meshes, geom);

		geom = DRW_cache_camera_tria_get();
		camera_tria = shgroup_camera_instance(psl->non_meshes, geom);

		geom = DRW_cache_plain_axes_get();
		camera_focus = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		camera_clip = shgroup_distance_lines_instance(psl->non_meshes, geom);
		camera_mist = shgroup_distance_lines_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_endpoints_get();
		camera_clip_points = shgroup_distance_lines_instance(psl->non_meshes, geom);
		camera_mist_points = shgroup_distance_lines_instance(psl->non_meshes, geom);

		/* Lamps */
		/* TODO
		 * for now we create multiple times the same VBO with only lamp center coordinates
		 * but ideally we would only create it once */

		/* start with buflimit because we don't want stipples */
		geom = DRW_cache_single_line_get();
		lamp_buflimit = shgroup_distance_lines_instance(psl->non_meshes, geom);

		lamp_center = shgroup_dynpoints_uniform_color(psl->non_meshes, ts.colorLampNoAlpha, &ts.sizeLampCenter);
		lamp_center_group = shgroup_dynpoints_uniform_color(psl->non_meshes, ts.colorGroup, &ts.sizeLampCenter);

		geom = DRW_cache_lamp_get();
		lamp_circle = shgroup_instance_screenspace(psl->non_meshes, geom, &ts.sizeLampCircle);
		lamp_circle_shadow = shgroup_instance_screenspace(psl->non_meshes, geom, &ts.sizeLampCircleShadow);

		geom = DRW_cache_lamp_sunrays_get();
		lamp_sunrays = shgroup_instance_screenspace(psl->non_meshes, geom, &ts.sizeLampCircle);

		lamp_groundline = shgroup_groundlines_uniform_color(psl->non_meshes, ts.colorLamp);
		lamp_groundpoint = shgroup_groundpoints_uniform_color(psl->non_meshes, ts.colorLamp);

		geom = DRW_cache_lamp_area_get();
		lamp_area = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_lamp_hemi_get();
		lamp_hemi = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		lamp_distance = shgroup_distance_lines_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_endpoints_get();
		lamp_buflimit_points = shgroup_distance_lines_instance(psl->non_meshes, geom);

		geom = DRW_cache_lamp_spot_get();
		lamp_spot_cone = shgroup_spot_instance(psl->non_meshes, geom);

		geom = DRW_cache_circle_get();
		lamp_spot_blend = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_lamp_spot_square_get();
		lamp_spot_pyramid = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_square_get();
		lamp_spot_blend_rect = shgroup_instance(psl->non_meshes, geom);

		/* Relationship Lines */
		relationship_lines = shgroup_dynlines_uniform_color(psl->non_meshes, ts.colorWire);
		DRW_shgroup_state_set(relationship_lines, DRW_STATE_STIPPLE_3);
	}

	{
		/* Object Center pass grouped by State */
		DRWShadingGroup *grp;
		static float outlineWidth, size;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_POINT;
		psl->ob_center = DRW_pass_create("Obj Center Pass", state);

		outlineWidth = 1.0f * U.pixelsize;
		size = U.obcenter_dia * U.pixelsize + outlineWidth;

		struct GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);

		/* Active */
		grp = DRW_shgroup_point_batch_create(sh, psl->ob_center);
		DRW_shgroup_uniform_float(grp, "size", &size, 1);
		DRW_shgroup_uniform_float(grp, "outlineWidth", &outlineWidth, 1);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorActive, 1);
		DRW_shgroup_uniform_vec4(grp, "outlineColor", ts.colorOutline, 1);
		center_active = grp;

		/* Select */
		grp = DRW_shgroup_point_batch_create(sh, psl->ob_center);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorSelect, 1);
		center_selected = grp;

		/* Deselect */
		grp = DRW_shgroup_point_batch_create(sh, psl->ob_center);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorDeselect, 1);
		center_deselected = grp;
	}
}

static void DRW_shgroup_wire_outline(Object *ob, const bool do_wire, const bool do_outline)
{
	struct GPUShader *sh;
	OBJECT_PassList *psl = vedata->psl;
	struct Batch *geom = DRW_cache_wire_outline_get(ob);

	float *color;
	DRW_object_wire_theme_get(ob, &color);

	bool is_perps = DRW_viewport_is_persp_get();
	static bool bTrue = true;
	static bool bFalse = false;

	/* Note (TODO) : this requires cache to be discarded on ortho/perp switch
	 * It may be preferable (or not depending on performance implication)
	 * to introduce a shader uniform switch */
	if (is_perps) {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_PERSP);
	}
	else {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_ORTHO);
	}

	if (do_wire) {
		bool *bFront = (do_wire) ? &bTrue : &bFalse;
		bool *bBack = (do_wire) ? &bTrue : &bFalse;

		DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->wire_outline);
		DRW_shgroup_state_set(grp, DRW_STATE_WIRE);
		DRW_shgroup_uniform_vec4(grp, "frontColor", color, 1);
		DRW_shgroup_uniform_vec4(grp, "backColor", color, 1);
		DRW_shgroup_uniform_bool(grp, "drawFront", bFront, 1);
		DRW_shgroup_uniform_bool(grp, "drawBack", bBack, 1);
		DRW_shgroup_uniform_bool(grp, "drawSilhouette", &bFalse, 1);
		DRW_shgroup_call_add(grp, geom, ob->obmat);
	}

	if (do_outline) {
		DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->wire_outline);
		DRW_shgroup_state_set(grp, DRW_STATE_WIRE_LARGE);
		DRW_shgroup_uniform_vec4(grp, "silhouetteColor", color, 1);
		DRW_shgroup_uniform_bool(grp, "drawFront", &bFalse, 1);
		DRW_shgroup_uniform_bool(grp, "drawBack", &bFalse, 1);
		DRW_shgroup_uniform_bool(grp, "drawSilhouette", &bTrue, 1);

		DRW_shgroup_call_add(grp, geom, ob->obmat);
	}
}

static void DRW_shgroup_lamp(Object *ob)
{
	Lamp *la = ob->data;
	float *color;
	int theme_id = DRW_object_wire_theme_get(ob, &color);
	static float zero = 0.0f;

	/* Don't draw the center if it's selected or active */
	if (theme_id == TH_GROUP)
		DRW_shgroup_dynamic_call_add(lamp_center_group, ob->obmat[3]);
	else if (theme_id == TH_LAMP)
		DRW_shgroup_dynamic_call_add(lamp_center, ob->obmat[3]);

	/* First circle */
	DRW_shgroup_dynamic_call_add(lamp_circle, ob->obmat[3], color);

	/* draw dashed outer circle if shadow is on. remember some lamps can't have certain shadows! */
	if (la->type != LA_HEMI) {
		if ((la->mode & LA_SHAD_RAY) || ((la->mode & LA_SHAD_BUF) && (la->type == LA_SPOT))) {
			DRW_shgroup_dynamic_call_add(lamp_circle_shadow, ob->obmat[3], color);
		}
	}

	/* Distance */
	if (ELEM(la->type, LA_HEMI, LA_SUN, LA_AREA)) {
		DRW_shgroup_dynamic_call_add(lamp_distance, color, &zero, &la->dist, ob->obmat);
	}

	copy_m4_m4(la->shapemat, ob->obmat);

	if (la->type == LA_SUN) {
		DRW_shgroup_dynamic_call_add(lamp_sunrays, ob->obmat[3], color);
	}
	else if (la->type == LA_SPOT) {
		float size[3], sizemat[4][4];
		static float one = 1.0f;
		float blend = 1.0f - pow2f(la->spotblend);

		size[0] = size[1] = sinf(la->spotsize * 0.5f) * la->dist;
		size[2] = cosf(la->spotsize * 0.5f) * la->dist;

		size_to_mat4(sizemat, size);
		mul_m4_m4m4(la->spotconemat, ob->obmat, sizemat);

		size[0] = size[1] = blend; size[2] = 1.0f;
		size_to_mat4(sizemat, size);
		translate_m4(sizemat, 0.0f, 0.0f, -1.0f);
		rotate_m4(sizemat, 'X', M_PI / 2.0f);
		mul_m4_m4m4(la->spotblendmat, la->spotconemat, sizemat);

		if (la->mode & LA_SQUARE) {
			DRW_shgroup_dynamic_call_add(lamp_spot_pyramid,    color, &one, la->spotconemat);

			/* hide line if it is zero size or overlaps with outer border,
			 * previously it adjusted to always to show it but that seems
			 * confusing because it doesn't show the actual blend size */
			if (blend != 0.0f && blend != 1.0f) {
				DRW_shgroup_dynamic_call_add(lamp_spot_blend_rect, color, &one, la->spotblendmat);
			}
		}
		else {
			DRW_shgroup_dynamic_call_add(lamp_spot_cone,  color, la->spotconemat);

			/* hide line if it is zero size or overlaps with outer border,
			 * previously it adjusted to always to show it but that seems
			 * confusing because it doesn't show the actual blend size */
			if (blend != 0.0f && blend != 1.0f) {
				DRW_shgroup_dynamic_call_add(lamp_spot_blend, color, &one, la->spotblendmat);
			}
		}

		normalize_m4(la->shapemat);
		DRW_shgroup_dynamic_call_add(lamp_buflimit,        color, &la->clipsta, &la->clipend, ob->obmat);
		DRW_shgroup_dynamic_call_add(lamp_buflimit_points, color, &la->clipsta, &la->clipend, ob->obmat);
	}
	else if (la->type == LA_HEMI) {
		static float hemisize = 2.0f;
		DRW_shgroup_dynamic_call_add(lamp_hemi, color, &hemisize, la->shapemat);
	}
	else if (la->type == LA_AREA) {
		float size[3] = {1.0f, 1.0f, 1.0f}, sizemat[4][4];

		if (la->area_shape == LA_AREA_RECT) {
			size[1] = la->area_sizey / la->area_size;
			size_to_mat4(sizemat, size);
			mul_m4_m4m4(la->shapemat, la->shapemat, sizemat);
		}

		DRW_shgroup_dynamic_call_add(lamp_area, color, &la->area_size, la->shapemat);
	}

	/* Line and point going to the ground */
	DRW_shgroup_dynamic_call_add(lamp_groundline, ob->obmat[3]);
	DRW_shgroup_dynamic_call_add(lamp_groundpoint, ob->obmat[3]);
}

static void DRW_shgroup_camera(Object *ob)
{
	const struct bContext *C = DRW_get_context();
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);

	Camera *cam = ob->data;
	const bool is_active = (ob == v3d->camera);
	float *color;
	DRW_object_wire_theme_get(ob, &color);

	float vec[4][3], asp[2], shift[2], scale[3], drawsize;

	scale[0] = 1.0f / len_v3(ob->obmat[0]);
	scale[1] = 1.0f / len_v3(ob->obmat[1]);
	scale[2] = 1.0f / len_v3(ob->obmat[2]);

	BKE_camera_view_frame_ex(scene, cam, cam->drawsize, false, scale,
	                         asp, shift, &drawsize, vec);

	// /* Frame coords */
	copy_v2_v2(cam->drwcorners[0], vec[0]);
	copy_v2_v2(cam->drwcorners[1], vec[1]);
	copy_v2_v2(cam->drwcorners[2], vec[2]);
	copy_v2_v2(cam->drwcorners[3], vec[3]);

	/* depth */
	cam->drwdepth = vec[0][2];

	/* tria */
	cam->drwtria[0][0] = shift[0] + ((0.7f * drawsize) * scale[0]);
	cam->drwtria[0][1] = shift[1] + ((drawsize * (asp[1] + 0.1f)) * scale[1]);
	cam->drwtria[1][0] = shift[0];
	cam->drwtria[1][1] = shift[1] + ((1.1f * drawsize * (asp[1] + 0.7f)) * scale[1]);

	DRW_shgroup_dynamic_call_add(camera, color, cam->drwcorners, &cam->drwdepth, cam->drwtria, ob->obmat);

	/* Active cam */
	if (is_active) {
		DRW_shgroup_dynamic_call_add(camera_tria, color, cam->drwcorners, &cam->drwdepth, cam->drwtria, ob->obmat);
	}

	/* draw the rest in normalize object space */
	copy_m4_m4(cam->drwnormalmat, ob->obmat);
	normalize_m4(cam->drwnormalmat);

	if (cam->flag & CAM_SHOWLIMITS) {
		static float col[3] = {0.5f, 0.5f, 0.25f}, col_hi[3] = {1.0f, 1.0f, 0.5f};
		float sizemat[4][4], size[3] = {1.0f, 1.0f, 0.0f};
		float focusdist = BKE_camera_object_dof_distance(ob);

		copy_m4_m4(cam->drwfocusmat, cam->drwnormalmat);
		translate_m4(cam->drwfocusmat, 0.0f, 0.0f, -focusdist);
		size_to_mat4(sizemat, size);
		mul_m4_m4m4(cam->drwfocusmat, cam->drwfocusmat, sizemat);

		DRW_shgroup_dynamic_call_add(camera_focus, (is_active ? col_hi : col), &cam->drawsize, cam->drwfocusmat);

		DRW_shgroup_dynamic_call_add(camera_clip, color, &cam->clipsta, &cam->clipend, cam->drwnormalmat);
		DRW_shgroup_dynamic_call_add(camera_clip_points, (is_active ? col_hi : col), &cam->clipsta, &cam->clipend, cam->drwnormalmat);
	}

	if (cam->flag & CAM_SHOWMIST) {
		World *world = scene->world;

		if (world) {
			static float col[3] = {0.5f, 0.5f, 0.5f}, col_hi[3] = {1.0f, 1.0f, 1.0f};
			world->mistend = world->miststa + world->mistdist;
			DRW_shgroup_dynamic_call_add(camera_mist,        color, &world->miststa, &world->mistend, cam->drwnormalmat);
			DRW_shgroup_dynamic_call_add(camera_mist_points, (is_active ? col_hi : col), &world->miststa, &world->mistend, cam->drwnormalmat);
		}
	}
}

static void DRW_shgroup_empty(Object *ob)
{
	float *color;
	DRW_object_wire_theme_get(ob, &color);

	switch (ob->empty_drawtype) {
		case OB_PLAINAXES:
			DRW_shgroup_dynamic_call_add(plain_axes, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_SINGLE_ARROW:
			DRW_shgroup_dynamic_call_add(single_arrow, color, &ob->empty_drawsize, ob->obmat);
			DRW_shgroup_dynamic_call_add(single_arrow_line, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_CUBE:
			DRW_shgroup_dynamic_call_add(cube, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_CIRCLE:
			DRW_shgroup_dynamic_call_add(circle, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_EMPTY_SPHERE:
			DRW_shgroup_dynamic_call_add(sphere, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_EMPTY_CONE:
			DRW_shgroup_dynamic_call_add(cone, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_ARROWS:
			DRW_shgroup_dynamic_call_add(arrows, color, &ob->empty_drawsize, ob->obmat);
			DRW_shgroup_dynamic_call_add(axis_names, color, &ob->empty_drawsize, ob->obmat);
			break;
	}
}

static void DRW_shgroup_speaker(Object *ob)
{
	float *color;
	static float one = 1.0f;
	DRW_object_wire_theme_get(ob, &color);

	DRW_shgroup_dynamic_call_add(speaker, color, &one, ob->obmat);
}

static void DRW_shgroup_relationship_lines(Object *ob)
{
	if (ob->parent) {
		DRW_shgroup_dynamic_call_add(relationship_lines, ob->obmat[3]);
		DRW_shgroup_dynamic_call_add(relationship_lines, ob->parent->obmat[3]);
	}
}

static void DRW_shgroup_object_center(Object *ob)
{
	if ((ob->base_flag & BASE_SELECTED) != 0) {
		DRW_shgroup_dynamic_call_add(center_selected, ob->obmat[3]);
	}
	else if (0) {
		DRW_shgroup_dynamic_call_add(center_deselected, ob->obmat[3]);
	}
}

static void OBJECT_cache_populate(Object *ob)
{
	const struct bContext *C = DRW_get_context();
	Scene *scene = CTX_data_scene(C);

	CollectionEngineSettings *ces_mode_ob = BKE_object_collection_engine_get(ob, COLLECTION_MODE_OBJECT, "");

	bool do_wire = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_wire");
	bool do_outlines = ((ob->base_flag & BASE_SELECTED) != 0) || do_wire;

	switch (ob->type) {
		case OB_MESH:
			{
				Object *obedit = scene->obedit;
				if (ob != obedit) {
					DRW_shgroup_wire_outline(ob, do_wire, do_outlines);
				}
			}
			break;
		case OB_LAMP:
			DRW_shgroup_lamp(ob);
			break;
		case OB_CAMERA:
			DRW_shgroup_camera(ob);
			break;
		case OB_EMPTY:
			DRW_shgroup_empty(ob);
			break;
		case OB_SPEAKER:
			DRW_shgroup_speaker(ob);
			break;
		case OB_ARMATURE:
			{
				bArmature *arm = ob->data;
				if (arm->edbo == NULL) {
					DRW_shgroup_armature_object(ob, vedata->psl->bone_solid,
					                                vedata->psl->bone_wire,
					                                relationship_lines);
				}
			}
			break;
		default:
			break;
	}

	DRW_shgroup_object_center(ob);
	DRW_shgroup_relationship_lines(ob);
}

static void OBJECT_draw_scene(void)
{
	OBJECT_Data *ved = DRW_viewport_engine_data_get("ObjectMode");
	OBJECT_PassList *psl = ved->psl;

	DRW_draw_pass(psl->bone_wire);
	DRW_draw_pass(psl->bone_solid);
	DRW_draw_pass(psl->wire_outline);
	DRW_draw_pass(psl->non_meshes);
	DRW_draw_pass(psl->ob_center);
}

void OBJECT_collection_settings_create(CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	BKE_collection_engine_property_add_int(ces, "show_wire", false);
	BKE_collection_engine_property_add_int(ces, "show_backface_culling", false);
}

DrawEngineType draw_engine_object_type = {
	NULL, NULL,
	N_("ObjectMode"),
	NULL,
	NULL,
	&OBJECT_cache_init,
	&OBJECT_cache_populate,
	NULL,
	NULL,
	&OBJECT_draw_scene
};
