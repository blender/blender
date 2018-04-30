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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file DNA_lightprobe_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_LIGHTPROBE_TYPES_H__
#define __DNA_LIGHTPROBE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct AnimData;

typedef struct LightProbe {
	ID id;
	struct AnimData *adt;		/* animation data (must be immediately after id for utilities to use it) */

	char type;        /* For realtime probe objects */
	char flag;        /* General purpose flags for probes */
	char attenuation_type; /* Attenuation type */
	char parallax_type;    /* Parallax type */

	float distinf;    /* Influence Radius */
	float distpar;    /* Parallax Radius */
	float falloff;    /* Influence falloff */

	float clipsta, clipend;

	float vis_bias, vis_bleedbias; /* VSM visibility biases */
	float vis_blur;

	float intensity; /* Intensity multiplier */

	int grid_resolution_x;  /* Irradiance grid resolution */
	int grid_resolution_y;
	int grid_resolution_z;
	int pad1;

	struct Object *parallax_ob;    /* Object to use as a parallax origin */
	struct Image *image;           /* Image to use on as lighting data */
	struct Collection *visibility_grp;  /* Object visibility group, inclusive or exclusive */

	float data_draw_size;

	/* Runtime display data */
	float distfalloff, distgridinf;
	float pad;
} LightProbe;

/* Probe->type */
enum {
	LIGHTPROBE_TYPE_CUBE      = 0,
	LIGHTPROBE_TYPE_PLANAR    = 1,
	LIGHTPROBE_TYPE_GRID      = 2,
};

/* Probe->flag */
enum {
	LIGHTPROBE_FLAG_CUSTOM_PARALLAX = (1 << 0),
	LIGHTPROBE_FLAG_SHOW_INFLUENCE  = (1 << 1),
	LIGHTPROBE_FLAG_SHOW_PARALLAX   = (1 << 2),
	LIGHTPROBE_FLAG_SHOW_CLIP_DIST  = (1 << 3),
	LIGHTPROBE_FLAG_SHOW_DATA       = (1 << 4),
	LIGHTPROBE_FLAG_INVERT_GROUP    = (1 << 5),
};

/* Probe->display */
enum {
	LIGHTPROBE_DISP_WIRE         = 0,
	LIGHTPROBE_DISP_SHADED       = 1,
	LIGHTPROBE_DISP_DIFFUSE      = 2,
	LIGHTPROBE_DISP_REFLECTIVE   = 3,
};

/* Probe->parallax && Probe->attenuation_type*/
enum {
	LIGHTPROBE_SHAPE_ELIPSOID   = 0,
	LIGHTPROBE_SHAPE_BOX        = 1,
};

#ifdef __cplusplus
}
#endif

#endif /* __DNA_LIGHTPROBE_TYPES_H__ */
