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

/** \file blender/draw/draw_mode_pass.c
 *  \ingroup draw
 */

#include "DNA_userdef_types.h"

#include "GPU_shader.h"

#include "UI_resources.h"

#include "BKE_global.h"

#include "draw_mode_pass.h"

/* ************************** OBJECT MODE ******************************* */

/* Store list of shading group for easy access*/

/* Empties */
static DRWShadingGroup *plain_axes_wire;
static DRWShadingGroup *plain_axes_active;
static DRWShadingGroup *plain_axes_select;
static DRWShadingGroup *plain_axes_transform;
static DRWShadingGroup *plain_axes_group;
static DRWShadingGroup *plain_axes_group_active;

static DRWShadingGroup *cube_wire;
static DRWShadingGroup *cube_active;
static DRWShadingGroup *cube_select;
static DRWShadingGroup *cube_transform;
static DRWShadingGroup *cube_group;
static DRWShadingGroup *cube_group_active;

static DRWShadingGroup *circle_wire;
static DRWShadingGroup *circle_active;
static DRWShadingGroup *circle_select;
static DRWShadingGroup *circle_transform;
static DRWShadingGroup *circle_group;
static DRWShadingGroup *circle_group_active;

static DRWShadingGroup *sphere_wire;
static DRWShadingGroup *sphere_active;
static DRWShadingGroup *sphere_select;
static DRWShadingGroup *sphere_transform;
static DRWShadingGroup *sphere_group;
static DRWShadingGroup *sphere_group_active;

static DRWShadingGroup *cone_wire;
static DRWShadingGroup *cone_active;
static DRWShadingGroup *cone_select;
static DRWShadingGroup *cone_transform;
static DRWShadingGroup *cone_group;
static DRWShadingGroup *cone_group_active;

static DRWShadingGroup *single_arrow_wire;
static DRWShadingGroup *single_arrow_active;
static DRWShadingGroup *single_arrow_select;
static DRWShadingGroup *single_arrow_transform;
static DRWShadingGroup *single_arrow_group;
static DRWShadingGroup *single_arrow_group_active;

static DRWShadingGroup *single_arrow_line_wire;
static DRWShadingGroup *single_arrow_line_active;
static DRWShadingGroup *single_arrow_line_select;
static DRWShadingGroup *single_arrow_line_transform;
static DRWShadingGroup *single_arrow_line_group;
static DRWShadingGroup *single_arrow_line_group_active;

static DRWShadingGroup *arrows_wire;
static DRWShadingGroup *arrows_active;
static DRWShadingGroup *arrows_select;
static DRWShadingGroup *arrows_transform;
static DRWShadingGroup *arrows_group;
static DRWShadingGroup *arrows_group_active;

/* Lamps */
static DRWShadingGroup *lamp_center;
static DRWShadingGroup *lamp_center_group;
static DRWShadingGroup *lamp_groundpoint;
static DRWShadingGroup *lamp_groundline;
static DRWShadingGroup *lamp_circle;
static DRWShadingGroup *lamp_circle_shadow;
static DRWShadingGroup *lamp_sunrays;

/* Helpers */
static DRWShadingGroup *relationship_lines;

/* Objects Centers */
static DRWShadingGroup *center_active;
static DRWShadingGroup *center_selected;
static DRWShadingGroup *center_deselected;

/* Colors & Constant */
static float colorWire[4], colorWireEdit[4];
static float colorActive[4], colorSelect[4], colorTransform[4], colorGroup[4], colorGroupActive[4];
static float colorEmpty[4], colorLamp[4], colorCamera[4], colorSpeaker[4];
static float lampCenterSize, lampCircleRad, lampCircleShadowRad, colorLampNoAlpha[4];

static DRWShadingGroup *shgroup_instance_uniform_color(DRWPass *pass, struct Batch *geom, float color[4])
{
	GPUShader *sh_inst = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR_INSTANCE);

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh_inst, pass, geom);
	DRW_shgroup_attrib_float(grp, "InstanceModelMatrix", 16);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);

	return grp;
}

static DRWShadingGroup *shgroup_dynlines_uniform_color(DRWPass *pass, float color[4])
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);

	DRWShadingGroup *grp = DRW_shgroup_line_batch_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);

	return grp;
}

static DRWShadingGroup *shgroup_dynpoints_uniform_color(DRWPass *pass, float color[4], float *size)
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_SMOOTH);

	DRWShadingGroup *grp = DRW_shgroup_point_batch_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);
	DRW_shgroup_uniform_float(grp, "size", size, 1);
	DRW_shgroup_state_set(grp, DRW_STATE_POINT);

	return grp;
}

static DRWShadingGroup *shgroup_groundlines_uniform_color(DRWPass *pass, float color[4])
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_GROUNDLINE);

	DRWShadingGroup *grp = DRW_shgroup_point_batch_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);

	return grp;
}

static DRWShadingGroup *shgroup_groundpoints_uniform_color(DRWPass *pass, float color[4])
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_GROUNDPOINT);

	DRWShadingGroup *grp = DRW_shgroup_point_batch_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", color, 1);
	DRW_shgroup_state_set(grp, DRW_STATE_POINT);

	return grp;
}

static DRWShadingGroup *shgroup_lamp(DRWPass *pass, struct Batch *geom, float *size)
{
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_LAMP_COMMON);

	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh, pass, geom);
	DRW_shgroup_attrib_float(grp, "lamp_pos", 3);
	DRW_shgroup_attrib_float(grp, "color", 3);
	DRW_shgroup_uniform_float(grp, "size", size, 1);
	DRW_shgroup_uniform_float(grp, "pixel_size", DRW_viewport_pixelsize_get(), 1);
	DRW_shgroup_uniform_vec3(grp, "screen_vecs", DRW_viewport_screenvecs_get(), 2);
	DRW_shgroup_state_set(grp, DRW_STATE_STIPPLE_3);

	return grp;
}

/* This Function setup the passes needed for the mode rendering.
 * The passes are populated by the rendering engine using the DRW_shgroup_* functions. */
void DRW_pass_setup_common(DRWPass **wire_overlay, DRWPass **wire_outline, DRWPass **non_meshes, DRWPass **ob_center)
{
	/* Theses are defined for the whole application so make sure they rely on global settings */


	UI_GetThemeColor4fv(TH_WIRE, colorWire);
	UI_GetThemeColor4fv(TH_WIRE_EDIT, colorWireEdit);
	UI_GetThemeColor4fv(TH_ACTIVE, colorActive);
	UI_GetThemeColor4fv(TH_SELECT, colorSelect);
	UI_GetThemeColor4fv(TH_TRANSFORM, colorTransform);
	UI_GetThemeColor4fv(TH_GROUP_ACTIVE, colorGroupActive);
	UI_GetThemeColor4fv(TH_GROUP, colorGroup);
	UI_GetThemeColor4fv(TH_LAMP, colorLamp);
	UI_GetThemeColor4fv(TH_LAMP, colorLampNoAlpha);
	UI_GetThemeColor4fv(TH_SPEAKER, colorSpeaker);
	UI_GetThemeColor4fv(TH_CAMERA, colorCamera);
	UI_GetThemeColor4fv(TH_EMPTY, colorEmpty);

	colorLampNoAlpha[3] = 1.0f;

	if (wire_overlay) {
		/* This pass can draw mesh edges top of Shaded Meshes without any Z fighting */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_BLEND;
		*wire_overlay = DRW_pass_create("Wire Overlays Pass", state);
	}

	if (wire_outline) {
		/* This pass can draw mesh outlines and/or fancy wireframe */
		/* Fancy wireframes are not meant to be occluded (without Z offset) */
		/* Outlines and Fancy Wires use the same VBO */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		*wire_outline = DRW_pass_create("Wire + Outlines Pass", state);
	}

	if (non_meshes) {
		/* Non Meshes Pass (Camera, empties, lamps ...) */
		DRWShadingGroup *grp;
		struct Batch *geom;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		state |= DRW_STATE_WIRE;
		*non_meshes = DRW_pass_create("Non Meshes Pass", state);

		GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);

		/* Empties */
		geom = DRW_cache_plain_axes_get();
		plain_axes_wire = shgroup_instance_uniform_color(*non_meshes, geom, colorEmpty);
		plain_axes_active = shgroup_instance_uniform_color(*non_meshes, geom, colorActive);
		plain_axes_select = shgroup_instance_uniform_color(*non_meshes, geom, colorSelect);
		plain_axes_transform = shgroup_instance_uniform_color(*non_meshes, geom, colorTransform);
		plain_axes_group = shgroup_instance_uniform_color(*non_meshes, geom, colorGroup);
		plain_axes_group_active = shgroup_instance_uniform_color(*non_meshes, geom, colorGroupActive);

		geom = DRW_cache_cube_get();
		cube_wire = shgroup_instance_uniform_color(*non_meshes, geom, colorEmpty);
		cube_active = shgroup_instance_uniform_color(*non_meshes, geom, colorActive);
		cube_select = shgroup_instance_uniform_color(*non_meshes, geom, colorSelect);
		cube_transform = shgroup_instance_uniform_color(*non_meshes, geom, colorTransform);
		cube_group = shgroup_instance_uniform_color(*non_meshes, geom, colorGroup);
		cube_group_active = shgroup_instance_uniform_color(*non_meshes, geom, colorGroupActive);

		geom = DRW_cache_circle_get();
		circle_wire = shgroup_instance_uniform_color(*non_meshes, geom, colorEmpty);
		circle_active = shgroup_instance_uniform_color(*non_meshes, geom, colorActive);
		circle_select = shgroup_instance_uniform_color(*non_meshes, geom, colorSelect);
		circle_transform = shgroup_instance_uniform_color(*non_meshes, geom, colorTransform);
		circle_group = shgroup_instance_uniform_color(*non_meshes, geom, colorGroup);
		circle_group_active = shgroup_instance_uniform_color(*non_meshes, geom, colorGroupActive);

		geom = DRW_cache_empty_sphere_get();
		sphere_wire = shgroup_instance_uniform_color(*non_meshes, geom, colorEmpty);
		sphere_active = shgroup_instance_uniform_color(*non_meshes, geom, colorActive);
		sphere_select = shgroup_instance_uniform_color(*non_meshes, geom, colorSelect);
		sphere_transform = shgroup_instance_uniform_color(*non_meshes, geom, colorTransform);
		sphere_group = shgroup_instance_uniform_color(*non_meshes, geom, colorGroup);
		sphere_group_active = shgroup_instance_uniform_color(*non_meshes, geom, colorGroupActive);

		geom = DRW_cache_empty_cone_get();
		cone_wire = shgroup_instance_uniform_color(*non_meshes, geom, colorEmpty);
		cone_active = shgroup_instance_uniform_color(*non_meshes, geom, colorActive);
		cone_select = shgroup_instance_uniform_color(*non_meshes, geom, colorSelect);
		cone_transform = shgroup_instance_uniform_color(*non_meshes, geom, colorTransform);
		cone_group = shgroup_instance_uniform_color(*non_meshes, geom, colorGroup);
		cone_group_active = shgroup_instance_uniform_color(*non_meshes, geom, colorGroupActive);

		geom = DRW_cache_single_arrow_get();
		single_arrow_wire = shgroup_instance_uniform_color(*non_meshes, geom, colorEmpty);
		single_arrow_active = shgroup_instance_uniform_color(*non_meshes, geom, colorActive);
		single_arrow_select = shgroup_instance_uniform_color(*non_meshes, geom, colorSelect);
		single_arrow_transform = shgroup_instance_uniform_color(*non_meshes, geom, colorTransform);
		single_arrow_group = shgroup_instance_uniform_color(*non_meshes, geom, colorGroup);
		single_arrow_group_active = shgroup_instance_uniform_color(*non_meshes, geom, colorGroupActive);

		geom = DRW_cache_single_line_get();
		single_arrow_line_wire = shgroup_instance_uniform_color(*non_meshes, geom, colorEmpty);
		single_arrow_line_active = shgroup_instance_uniform_color(*non_meshes, geom, colorActive);
		single_arrow_line_select = shgroup_instance_uniform_color(*non_meshes, geom, colorSelect);
		single_arrow_line_transform = shgroup_instance_uniform_color(*non_meshes, geom, colorTransform);
		single_arrow_line_group = shgroup_instance_uniform_color(*non_meshes, geom, colorGroup);
		single_arrow_line_group_active = shgroup_instance_uniform_color(*non_meshes, geom, colorGroupActive);

		geom = DRW_cache_single_arrow_get();
		arrows_wire = shgroup_instance_uniform_color(*non_meshes, geom, colorEmpty);
		arrows_active = shgroup_instance_uniform_color(*non_meshes, geom, colorActive);
		arrows_select = shgroup_instance_uniform_color(*non_meshes, geom, colorSelect);
		arrows_transform = shgroup_instance_uniform_color(*non_meshes, geom, colorTransform);
		arrows_group = shgroup_instance_uniform_color(*non_meshes, geom, colorGroup);
		arrows_group_active = shgroup_instance_uniform_color(*non_meshes, geom, colorGroupActive);

		/* Lamps */
		lampCenterSize = (U.obcenter_dia + 1.5f) * U.pixelsize;
		lampCircleRad = U.pixelsize * 9.0f;
		lampCircleShadowRad = lampCircleRad + U.pixelsize * 3.0f;
		/* TODO
		 * for now we create 3 times the same VBO with only lamp center coordinates
		 * but ideally we would only create it once */
		lamp_center = shgroup_dynpoints_uniform_color(*non_meshes, colorLampNoAlpha, &lampCenterSize);
		lamp_center_group = shgroup_dynpoints_uniform_color(*non_meshes, colorGroup, &lampCenterSize);

		geom = DRW_cache_lamp_get();
		lamp_circle = shgroup_lamp(*non_meshes, geom, &lampCircleRad);
		lamp_circle_shadow = shgroup_lamp(*non_meshes, geom, &lampCircleShadowRad);

		geom = DRW_cache_lamp_sunrays_get();
		lamp_sunrays = shgroup_lamp(*non_meshes, geom, &lampCircleRad);

		lamp_groundline = shgroup_groundlines_uniform_color(*non_meshes, colorLamp);
		lamp_groundpoint = shgroup_groundpoints_uniform_color(*non_meshes, colorLamp);

		/* Stipple Wires */
		grp = DRW_shgroup_create(sh, *non_meshes);
		DRW_shgroup_state_set(grp, DRW_STATE_STIPPLE_2);

		grp = DRW_shgroup_create(sh, *non_meshes);
		DRW_shgroup_state_set(grp, DRW_STATE_STIPPLE_3);

		grp = DRW_shgroup_create(sh, *non_meshes);
		DRW_shgroup_state_set(grp, DRW_STATE_STIPPLE_4);

		/* Relationship Lines */
		relationship_lines = shgroup_dynlines_uniform_color(*non_meshes, colorWire);
		DRW_shgroup_state_set(relationship_lines, DRW_STATE_STIPPLE_3);
	}

	if (ob_center) {
		/* Object Center pass grouped by State */
		DRWShadingGroup *grp;
		static float colorDeselect[4], outlineColor[4];
		static float outlineWidth, size;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_POINT;
		*ob_center = DRW_pass_create("Obj Center Pass", state);

		outlineWidth = 1.0f * U.pixelsize;
		size = U.obcenter_dia * U.pixelsize + outlineWidth;
		//UI_GetThemeColorShadeAlpha4fv(TH_ACTIVE, 0, -80, colorActive);
		//UI_GetThemeColorShadeAlpha4fv(TH_SELECT, 0, -80, colorSelect);
		UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, colorDeselect);
		UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, outlineColor);

		GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_SMOOTH);

		/* Active */
		grp = DRW_shgroup_point_batch_create(sh, *ob_center);
		DRW_shgroup_uniform_float(grp, "size", &size, 1);
		DRW_shgroup_uniform_float(grp, "outlineWidth", &outlineWidth, 1);
		DRW_shgroup_uniform_vec4(grp, "color", colorActive, 1);
		DRW_shgroup_uniform_vec4(grp, "outlineColor", outlineColor, 1);
		center_active = grp;

		/* Select */
		grp = DRW_shgroup_point_batch_create(sh, *ob_center);
		DRW_shgroup_uniform_vec4(grp, "color", colorSelect, 1);
		center_selected = grp;

		/* Deselect */
		grp = DRW_shgroup_point_batch_create(sh, *ob_center);
		DRW_shgroup_uniform_vec4(grp, "color", colorDeselect, 1);
		center_deselected = grp;
	}
}

/* ******************************************** WIRES *********************************************** */

/* TODO FINISH */
static int draw_object_wire_theme(Object *ob)
{
	const bool is_edit = (ob->mode & OB_MODE_EDIT) != 0;
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
		if ((ob->flag & OB_FROMGROUP) != 0) {
			if ((ob->base_flag & BASE_SELECTED) != 0) {
				/* uses darker active color for non-active + selected */
				theme_id = TH_GROUP_ACTIVE;

				// if (scene->basact != base) {
				// 	theme_shade = -16;
				// }
			}
			else {
				theme_id = TH_GROUP;
			}
		}
		else {
			if ((ob->base_flag & BASE_SELECTED) != 0) {
				theme_id = //scene->basact == base ? TH_ACTIVE :
				TH_SELECT;
			}
			else {
				if (ob->type == OB_LAMP) theme_id = TH_LAMP;
				else if (ob->type == OB_SPEAKER) theme_id = TH_SPEAKER;
				else if (ob->type == OB_CAMERA) theme_id = TH_CAMERA;
				else if (ob->type == OB_EMPTY) theme_id = TH_EMPTY;
				/* fallback to TH_WIRE */
			}
		}
	}

	return theme_id;
}

void DRW_shgroup_wire_overlay(DRWPass *wire_overlay, Object *ob)
{
#if 1
	struct Batch *geom = DRW_cache_wire_overlay_get(ob);
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_OVERLAY);

	DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_overlay);
	DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);

	DRW_shgroup_call_add(grp, geom, ob->obmat);
#else
	static float col[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	struct Batch *geom = DRW_cache_wire_overlay_get(ob);
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);

	DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_overlay);
	DRW_shgroup_uniform_vec4(grp, "color", col, 1);

	DRW_shgroup_call_add(grp, geom, ob->obmat);
#endif
}

void DRW_shgroup_wire_outline(DRWPass *wire_outline, Object *ob,
                              const bool do_front, const bool do_back, const bool do_outline)
{
	GPUShader *sh;
	struct Batch *geom = DRW_cache_wire_outline_get(ob);

	/* Get color */
	/* TODO get the right color depending on ob state (Groups, overides etc..) */
	static float frontcol[4], backcol[4], color[4];
	UI_GetThemeColor4fv(TH_ACTIVE, color);
	copy_v4_v4(frontcol, color);
	copy_v4_v4(backcol, color);
	backcol[3] = 0.333f;
	frontcol[3] = 0.667f;

#if 1 /* New wire */

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

	if (do_front || do_back) {
		bool *bFront = (do_front) ? &bTrue : &bFalse;
		bool *bBack = (do_back) ? &bTrue : &bFalse;

		DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_outline);
		DRW_shgroup_state_set(grp, DRW_STATE_WIRE);
		DRW_shgroup_uniform_vec4(grp, "frontColor", frontcol, 1);
		DRW_shgroup_uniform_vec4(grp, "backColor", backcol, 1);
		DRW_shgroup_uniform_bool(grp, "drawFront", bFront, 1);
		DRW_shgroup_uniform_bool(grp, "drawBack", bBack, 1);
		DRW_shgroup_uniform_bool(grp, "drawSilhouette", &bFalse, 1);
		DRW_shgroup_call_add(grp, geom, ob->obmat);
	}

	if (do_outline) {
		DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_outline);
		DRW_shgroup_state_set(grp, DRW_STATE_WIRE_LARGE);
		DRW_shgroup_uniform_vec4(grp, "silhouetteColor", color, 1);
		DRW_shgroup_uniform_bool(grp, "drawFront", &bFalse, 1);
		DRW_shgroup_uniform_bool(grp, "drawBack", &bFalse, 1);
		DRW_shgroup_uniform_bool(grp, "drawSilhouette", &bTrue, 1);

		DRW_shgroup_call_add(grp, geom, ob->obmat);
	}

#else /* Old (flat) wire */

	sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_outline);
	DRW_shgroup_state_set(grp, DRW_STATE_WIRE_LARGE);
	DRW_shgroup_uniform_vec4(grp, "color", frontcol, 1);

	DRW_shgroup_call_add(grp, geom, ob->obmat);
#endif

}

/* ***************************** NON MESHES ********************** */

static void DRW_draw_lamp(Object *ob)
{
	Lamp *la = ob->data;
	int theme_id = draw_object_wire_theme(ob);
	float *color;

	/* Don't draw the center if it's selected or active */
	if (theme_id == TH_GROUP)
		DRW_shgroup_dynamic_call_add(lamp_center_group, ob->obmat[3]);
	else if (theme_id == TH_LAMP)
		DRW_shgroup_dynamic_call_add(lamp_center, ob->obmat[3]);

	switch (theme_id) {
		case TH_ACTIVE:
			color = colorActive;
			break;
		case TH_SELECT:
			color = colorSelect;
			break;
		case TH_GROUP:
			color = colorGroup;
			break;
		case TH_GROUP_ACTIVE:
			color = colorGroupActive;
			break;
		case TH_TRANSFORM:
			color = colorTransform;
			break;
		default:
			color = colorLampNoAlpha;
			break;
	}

	/* First circle */
	DRW_shgroup_dynamic_call_add(lamp_circle, ob->obmat[3], color);

	/* draw dashed outer circle if shadow is on. remember some lamps can't have certain shadows! */
	if (la->type != LA_HEMI) {
		DRW_shgroup_dynamic_call_add(lamp_circle_shadow, ob->obmat[3], color);
	}

	/* Sunrays */
	if (la->type == LA_SUN) {
		DRW_shgroup_dynamic_call_add(lamp_sunrays, ob->obmat[3]);
	}

	/* Line and point going to the ground */
	DRW_shgroup_dynamic_call_add(lamp_groundline, ob->obmat[3]);
	DRW_shgroup_dynamic_call_add(lamp_groundpoint, ob->obmat[3]);
}

static void DRW_draw_empty(Object *ob)
{
	DRWShadingGroup *grp, *grp2 = NULL;
	int theme_id = draw_object_wire_theme(ob);

	switch (ob->empty_drawtype) {
		case OB_PLAINAXES:
			if (theme_id == TH_ACTIVE)
				grp = plain_axes_active;
			else if (theme_id == TH_SELECT)
				grp = plain_axes_select;
			else if (theme_id == TH_GROUP_ACTIVE)
				grp = plain_axes_group_active;
			else if (theme_id == TH_GROUP)
				grp = plain_axes_group;
			else if (theme_id == TH_TRANSFORM)
				grp = plain_axes_transform;
			else
				grp = plain_axes_wire;
			break;

		case OB_SINGLE_ARROW:
			if (theme_id == TH_ACTIVE) {
				grp = single_arrow_active;
				grp2 = single_arrow_line_active;
			}
			else if (theme_id == TH_SELECT) {
				grp = single_arrow_select;
				grp2 = single_arrow_line_select;
			}
			else if (theme_id == TH_GROUP_ACTIVE) {
				grp = single_arrow_group_active;
				grp2 = single_arrow_line_group_active;
			}
			else if (theme_id == TH_GROUP) {
				grp = single_arrow_group;
				grp2 = single_arrow_line_group;
			}
			else if (theme_id == TH_TRANSFORM) {
				grp = single_arrow_transform;
				grp2 = single_arrow_line_transform;
			}
			else {
				grp = single_arrow_wire;
				grp2 = single_arrow_line_wire;
			}
			break;

		case OB_CUBE:
			if (theme_id == TH_ACTIVE)
				grp = cube_active;
			else if (theme_id == TH_SELECT)
				grp = cube_select;
			else if (theme_id == TH_GROUP_ACTIVE)
				grp = cube_group_active;
			else if (theme_id == TH_GROUP)
				grp = cube_group;
			else if (theme_id == TH_TRANSFORM)
				grp = cube_transform;
			else
				grp = cube_wire;
			break;

		case OB_CIRCLE:
			if (theme_id == TH_ACTIVE)
				grp = circle_active;
			else if (theme_id == TH_SELECT)
				grp = circle_select;
			else if (theme_id == TH_GROUP_ACTIVE)
				grp = circle_group_active;
			else if (theme_id == TH_GROUP)
				grp = circle_group;
			else if (theme_id == TH_TRANSFORM)
				grp = circle_transform;
			else
				grp = circle_wire;
			break;

		case OB_EMPTY_SPHERE:
			if (theme_id == TH_ACTIVE)
				grp = sphere_active;
			else if (theme_id == TH_SELECT)
				grp = sphere_select;
			else if (theme_id == TH_GROUP_ACTIVE)
				grp = sphere_group_active;
			else if (theme_id == TH_GROUP)
				grp = sphere_group;
			else if (theme_id == TH_TRANSFORM)
				grp = sphere_transform;
			else
				grp = sphere_wire;
			break;

		case OB_EMPTY_CONE:
			if (theme_id == TH_ACTIVE)
				grp = cone_active;
			else if (theme_id == TH_SELECT)
				grp = cone_select;
			else if (theme_id == TH_GROUP_ACTIVE)
				grp = cone_group_active;
			else if (theme_id == TH_GROUP)
				grp = cone_group;
			else if (theme_id == TH_TRANSFORM)
				grp = cone_transform;
			else
				grp = cone_wire;
			break;

		case OB_ARROWS:
		default:
			if (theme_id == TH_ACTIVE)
				grp = arrows_active;
			else if (theme_id == TH_SELECT)
				grp = arrows_select;
			else if (theme_id == TH_GROUP_ACTIVE)
				grp = arrows_group_active;
			else if (theme_id == TH_GROUP)
				grp = arrows_group;
			else if (theme_id == TH_TRANSFORM)
				grp = arrows_transform;
			else
				grp = arrows_wire;
			/* TODO Missing axes names */
			break;
	}

	DRW_shgroup_dynamic_call_add(grp, ob->obmat);
	if (grp2 != NULL) {
		DRW_shgroup_dynamic_call_add(grp2, ob->obmat);
	}
}

void DRW_shgroup_non_meshes(DRWPass *UNUSED(non_meshes), Object *ob)
{
	switch (ob->type) {
		case OB_LAMP:
			DRW_draw_lamp(ob);
			break;
		case OB_CAMERA:
		case OB_EMPTY:
			DRW_draw_empty(ob);
		default:
			break;
	}
}

void DRW_shgroup_relationship_lines(DRWPass *UNUSED(non_meshes), Object *ob)
{
	if (ob->parent) {
		DRW_shgroup_dynamic_call_add(relationship_lines, ob->obmat[3]);
		DRW_shgroup_dynamic_call_add(relationship_lines, ob->parent->obmat[3]);
	}
}

/* ***************************** COMMON **************************** */

void DRW_shgroup_object_center(DRWPass *UNUSED(ob_center), Object *ob)
{
	if ((ob->base_flag & BASE_SELECTED) != 0) {
		DRW_shgroup_dynamic_call_add(center_selected, ob->obmat[3]);
	}
	else if (0) {
		DRW_shgroup_dynamic_call_add(center_deselected, ob->obmat[3]);
	}
}
