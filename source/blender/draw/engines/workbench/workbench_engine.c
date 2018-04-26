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

/** \file workbench_engine.c
 *  \ingroup draw_engine
 *
 * Simple engine for drawing color and/or depth.
 * When we only need simple flat shaders.
 */

#include "DRW_render.h"

#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_main.h"

#include "GPU_shader.h"

#include "workbench_engine.h"
#include "workbench_private.h"
/* Shaders */

#define WORKBENCH_ENGINE "BLENDER_WORKBENCH"


/* Functions */

static void workbench_layer_collection_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);

	BKE_collection_engine_property_add_float(props, "random_object_color_saturation", 0.5f);
	BKE_collection_engine_property_add_float(props, "random_object_color_value", 0.9f);
}

static void workbench_view_layer_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);

	const float diffuse_x_pos[3] = {1.0,  0.8,   0.6};
	const float diffuse_x_neg[3] = {1.0,  0.6,   0.6};
	const float diffuse_y_pos[3] = {0.9,  0.9,   1.0};
	const float diffuse_y_neg[3] = {0.05, 0.025, 0.025};
	const float diffuse_z_pos[3] = {0.8,  0.8,   0.75};
	const float diffuse_z_neg[3] = {1.0,  0.95,  0.8};

	BKE_collection_engine_property_add_float_array(props, "diffuse_light_x_pos", diffuse_x_pos, 3);
	BKE_collection_engine_property_add_float_array(props, "diffuse_light_x_neg", diffuse_x_neg, 3);
	BKE_collection_engine_property_add_float_array(props, "diffuse_light_y_pos", diffuse_y_pos, 3);
	BKE_collection_engine_property_add_float_array(props, "diffuse_light_y_neg", diffuse_y_neg, 3);
	BKE_collection_engine_property_add_float_array(props, "diffuse_light_z_pos", diffuse_z_pos, 3);
	BKE_collection_engine_property_add_float_array(props, "diffuse_light_z_neg", diffuse_z_neg, 3);
}

/* Note: currently unused, we may want to register so we can see this when debugging the view. */

RenderEngineType DRW_engine_viewport_workbench_type = {
	NULL, NULL,
	WORKBENCH_ENGINE, N_("Workbench"), RE_INTERNAL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, &workbench_layer_collection_settings_create, &workbench_view_layer_settings_create,
	&draw_engine_workbench_solid,
	{NULL, NULL, NULL}
};


#undef WORKBENCH_ENGINE
