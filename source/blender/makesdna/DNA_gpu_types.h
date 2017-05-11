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

/** \file DNA_gpu_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_GPU_TYPES_H__
#define __DNA_GPU_TYPES_H__

/* properties for dof effect */
typedef struct GPUDOFSettings {
	float focus_distance; /* focal distance for depth of field */
	float fstop;
	float focal_length;
	float sensor;
	float rotation;
	float ratio;
	int num_blades;
	int high_quality;
} GPUDOFSettings;

/* properties for SSAO effect */
typedef struct GPUSSAOSettings {
	float factor;
	float color[3];
	float distance_max;
	float attenuation;
	int samples; /* ray samples, we use presets here for easy control instead of */
	int pad;
} GPUSSAOSettings;

typedef struct GPUFXSettings {
	GPUDOFSettings *dof;
	GPUSSAOSettings *ssao;
	char fx_flag;  /* eGPUFXFlags */
	char pad[7];
} GPUFXSettings;

/* shaderfx enables */
typedef enum eGPUFXFlags {
	GPU_FX_FLAG_DOF         = (1 << 0),
	GPU_FX_FLAG_SSAO        = (1 << 1),
} eGPUFXFlags;

#endif  /* __DNA_GPU_TYPES_H__ */
