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

/** \file DNA_probe_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_PROBE_TYPES_H__
#define __DNA_PROBE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct AnimData;

typedef struct Probe {
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

	struct Object *parallax_ob;    /* Object to use as a parallax origin */
	struct Image *image;           /* Image to use on as lighting data */

	/* Runtime display data */
	float distfalloff, pad;
	float clipmat[6][4][4];
} Probe;

/* Probe->type */
enum {
	PROBE_CUBE      = 0,
	PROBE_PLANAR    = 1,
	PROBE_IMAGE     = 2,
};

/* Probe->flag */
enum {
	PRB_CUSTOM_PARALLAX = (1 << 0),
	PRB_SHOW_INFLUENCE  = (1 << 1),
	PRB_SHOW_PARALLAX   = (1 << 2),
	PRB_SHOW_CLIP_DIST  = (1 << 3),
};

/* Probe->display */
enum {
	PROBE_WIRE         = 0,
	PROBE_SHADED       = 1,
	PROBE_DIFFUSE      = 2,
	PROBE_REFLECTIVE   = 3,
};

/* Probe->parallax && Probe->attenuation_type*/
enum {
	PROBE_ELIPSOID   = 0,
	PROBE_BOX        = 1,
};

#ifdef __cplusplus
}
#endif

#endif /* __DNA_PROBE_TYPES_H__ */
