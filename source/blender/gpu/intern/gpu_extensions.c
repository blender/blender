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

/** \file blender/gpu/intern/gpu_extensions.c
 *  \ingroup gpu
 *
 * Wrap OpenGL features such as textures, shaders and GLSL
 * with checks for drivers and GPU support.
 */

#include "BLI_utildefines.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"

#include "BKE_global.h"

#include "GPU_basic_shader.h"
#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_texture.h"

#include "intern/gpu_private.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

/* Extensions support */

/* -- extension: version of GL that absorbs it
 * ARB_framebuffer object: 3.0
 * EXT_framebuffer_object: 3.0
 * EXT_framebuffer_blit: 3.0
 * EXT_framebuffer_multisample: 3.0
 * EXT_framebuffer_multisample_blit_scaled: ???
 * ARB_draw_instanced: 3.1
 * ARB_texture_multisample: 3.2
 * EXT_geometry_shader4: 3.2
 * ARB_texture_query_lod: 4.0
 */

static struct GPUGlobal {
	GLint maxtexsize;
	GLint maxcubemapsize;
	GLint maxtextures;
	bool extdisabled;
	int colordepth;
	int samples_color_texture_max;
	GPUDeviceType device;
	GPUOSType os;
	GPUDriverType driver;
	/* workaround for different calculation of dfdy factors on GPUs. Some GPUs/drivers
	 * calculate dfdy in shader differently when drawing to an offscreen buffer. First
	 * number is factor on screen and second is off-screen */
	float dfdyfactors[2];
	float max_anisotropy;
} GG = {1, 0};

/* GPU Types */

bool GPU_type_matches(GPUDeviceType device, GPUOSType os, GPUDriverType driver)
{
	return (GG.device & device) && (GG.os & os) && (GG.driver & driver);
}

/* GPU Extensions */

void GPU_extensions_disable(void)
{
	GG.extdisabled = true;
}

int GPU_max_texture_size(void)
{
	return GG.maxtexsize;
}

int GPU_max_textures(void)
{
	return GG.maxtextures;
}

float GPU_max_texture_anisotropy(void)
{
	return GG.max_anisotropy;
}

int GPU_max_color_texture_samples(void)
{
	return GG.samples_color_texture_max;
}

int GPU_max_cube_map_size(void)
{
	return GG.maxcubemapsize;
}

void GPU_get_dfdy_factors(float fac[2])
{
	copy_v2_v2(fac, GG.dfdyfactors);
}

void gpu_extensions_init(void)
{
	/* BLI_assert(GLEW_VERSION_2_1); */
	/* ^-- maybe a bit extreme? */

	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &GG.maxtextures);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &GG.maxtexsize);
	glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &GG.maxcubemapsize);

	if (GLEW_EXT_texture_filter_anisotropic)
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &GG.max_anisotropy);
	else
		GG.max_anisotropy = 1.0f;

	GLint r, g, b;
	glGetIntegerv(GL_RED_BITS, &r);
	glGetIntegerv(GL_GREEN_BITS, &g);
	glGetIntegerv(GL_BLUE_BITS, &b);
	GG.colordepth = r + g + b; /* assumes same depth for RGB */

	if (GLEW_VERSION_3_2 || GLEW_ARB_texture_multisample) {
		glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &GG.samples_color_texture_max);
	}

	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	const char *version = (const char *)glGetString(GL_VERSION);

	if (strstr(vendor, "ATI")) {
		GG.device = GPU_DEVICE_ATI;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if (strstr(vendor, "NVIDIA")) {
		GG.device = GPU_DEVICE_NVIDIA;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if (strstr(vendor, "Intel") ||
	         /* src/mesa/drivers/dri/intel/intel_context.c */
	         strstr(renderer, "Mesa DRI Intel") ||
	         strstr(renderer, "Mesa DRI Mobile Intel"))
	{
		GG.device = GPU_DEVICE_INTEL;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if ((strstr(renderer, "Mesa DRI R")) ||
	         (strstr(renderer, "Gallium ") && strstr(renderer, " on ATI ")) ||
	         (strstr(renderer, "Gallium ") && strstr(renderer, " on AMD ")))
	{
		GG.device = GPU_DEVICE_ATI;
		GG.driver = GPU_DRIVER_OPENSOURCE;
	}
	else if (strstr(renderer, "Nouveau") || strstr(vendor, "nouveau")) {
		GG.device = GPU_DEVICE_NVIDIA;
		GG.driver = GPU_DRIVER_OPENSOURCE;
	}
	else if (strstr(vendor, "Mesa")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else if (strstr(vendor, "Microsoft")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else if (strstr(renderer, "Apple Software Renderer")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else {
		GG.device = GPU_DEVICE_ANY;
		GG.driver = GPU_DRIVER_ANY;
	}

	/* make sure double side isn't used by default and only getting enabled in places where it's
	 * really needed to prevent different unexpected behaviors like with intel gme965 card (sergey) */
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

#ifdef _WIN32
	GG.os = GPU_OS_WIN;
#elif defined(__APPLE__)
	GG.os = GPU_OS_MAC;
#else
	GG.os = GPU_OS_UNIX;
#endif


	/* df/dy calculation factors, those are dependent on driver */
	if ((strstr(vendor, "ATI") && strstr(version, "3.3.10750"))) {
		GG.dfdyfactors[0] = 1.0;
		GG.dfdyfactors[1] = -1.0;
	}
	else if ((GG.device == GPU_DEVICE_INTEL) && (GG.os == GPU_OS_WIN) &&
	         (strstr(version, "4.0.0 - Build 10.18.10.3308") ||
	          strstr(version, "4.0.0 - Build 9.18.10.3186") ||
	          strstr(version, "4.0.0 - Build 9.18.10.3165") ||
	          strstr(version, "3.1.0 - Build 9.17.10.3347") ||
	          strstr(version, "3.1.0 - Build 9.17.10.4101") ||
	          strstr(version, "3.3.0 - Build 8.15.10.2618")))
	{
		GG.dfdyfactors[0] = -1.0;
		GG.dfdyfactors[1] = 1.0;
	}
	else {
		GG.dfdyfactors[0] = 1.0;
		GG.dfdyfactors[1] = 1.0;
	}


	GPU_invalid_tex_init();
	GPU_basic_shaders_init();
}

void gpu_extensions_exit(void)
{
	GPU_basic_shaders_exit();
	GPU_invalid_tex_free();
}

bool GPU_legacy_support(void)
{
	/* return whether or not current GL context is compatible with legacy OpenGL */
	static bool checked = false;
	static bool support = true;

	if (!checked) {
		if (GLEW_VERSION_3_2) {
			GLint profile;
			glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);

			if (G.debug & G_DEBUG_GPU) {
				printf("GL_CONTEXT_PROFILE_MASK = %#x (%s profile)\n", (unsigned int)profile,
				       (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) ? "compatibility" :
				       (profile & GL_CONTEXT_CORE_PROFILE_BIT) ? "core" : "unknown");
			}

			if (profile == 0) {
				/* workaround for nVidia's Linux driver */
				support = GLEW_ARB_compatibility;
			}
			else {
				support = profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
			}
		}
		else if (GLEW_VERSION_3_1) {
			support = GLEW_ARB_compatibility;
		}

		/* any OpenGL version <= 3.0 is legacy, so support remains true */

		checked = true;
	}

	return support;
}

bool GPU_full_non_power_of_two_support(void)
{
	/* always supported on full GL but still relevant for OpenGL ES 2.0 where
	 * NPOT textures can't use mipmaps or repeat wrap mode */
	return true;
}

bool GPU_display_list_support(void)
{
	/* deprecated in GL 3
	 * supported on older GL and compatibility profile
	 * still queried by game engine
	 */
	return true;
}

bool GPU_bicubic_bump_support(void)
{
	return GLEW_VERSION_4_0 || (GLEW_ARB_texture_query_lod && GLEW_VERSION_3_0);
}

bool GPU_geometry_shader_support(void)
{
	/* in GL 3.2 geometry shaders are fully supported
	 * core profile clashes with our other shaders so accept compatibility only
	 * other GL versions can use EXT_geometry_shader4 if available
	 */
	return (GLEW_VERSION_3_2 && GPU_legacy_support()) || GLEW_EXT_geometry_shader4;
}

bool GPU_geometry_shader_support_via_extension(void)
{
	return GLEW_EXT_geometry_shader4 && !(GLEW_VERSION_3_2 && GPU_legacy_support());
}

bool GPU_instanced_drawing_support(void)
{
	return GLEW_VERSION_3_1 || GLEW_ARB_draw_instanced;
}

int GPU_color_depth(void)
{
	return GG.colordepth;
}

bool GPU_mem_stats_supported(void)
{
	return (GLEW_NVX_gpu_memory_info || (GLEW_ATI_meminfo)) && (G.debug & G_DEBUG_GPU_MEM);
}


void GPU_mem_stats_get(int *totalmem, int *freemem)
{
	if (GLEW_NVX_gpu_memory_info) {
		/* returned value in Kb */
		glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, totalmem);

		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, freemem);
	}
	else if (GLEW_ATI_meminfo) {
		int stats[4];

		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, stats);
		*freemem = stats[0];
		*totalmem = 0;
	}
	else {
		*totalmem = 0;
		*freemem = 0;
	}
}

