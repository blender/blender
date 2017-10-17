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
 * Contributor(s): Brecht Van Lommel, Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/intern/gpu_debug.c
 *  \ingroup gpu
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"
#include "BLI_sys_types.h"
#include "BLI_system.h"

#include "BKE_global.h"

#include "GPU_glew.h"
#include "GPU_debug.h"
#include "intern/gpu_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CASE_CODE_RETURN_STR(code) case code: return #code;

static const char *gpu_gl_error_symbol(GLenum err)
{
	switch (err) {
		CASE_CODE_RETURN_STR(GL_NO_ERROR)
		CASE_CODE_RETURN_STR(GL_INVALID_ENUM)
		CASE_CODE_RETURN_STR(GL_INVALID_VALUE)
		CASE_CODE_RETURN_STR(GL_INVALID_OPERATION)
		CASE_CODE_RETURN_STR(GL_STACK_OVERFLOW)
		CASE_CODE_RETURN_STR(GL_STACK_UNDERFLOW)
		CASE_CODE_RETURN_STR(GL_OUT_OF_MEMORY)

#if GL_ARB_imaging
		CASE_CODE_RETURN_STR(GL_TABLE_TOO_LARGE)
#endif

#if defined(WITH_GLU)
		CASE_CODE_RETURN_STR(GLU_INVALID_ENUM)
		CASE_CODE_RETURN_STR(GLU_INVALID_VALUE)
		CASE_CODE_RETURN_STR(GLU_OUT_OF_MEMORY)
#endif

		default:
			return "<unknown error>";
	}
}

#undef CASE_CODE_RETURN_STR


static bool gpu_report_gl_errors(const char *file, int line, const char *str)
{
	GLenum gl_error = glGetError();

	if (gl_error == GL_NO_ERROR) {
		return true;
	}
	else {
		/* glGetError should have cleared the error flag, so if we get the
		 * same flag twice that means glGetError itself probably triggered
		 * the error. This happens on Windows if the GL context is invalid.
		 */
		{
			GLenum new_error = glGetError();
			if (gl_error == new_error) {
				fprintf(stderr, "GL: Possible context invalidation issue\n");
				return false;
			}
		}

		fprintf(stderr,
		        "%s:%d: ``%s'' -> GL Error (0x%04X - %s): %s\n",
		        file, line, str, gl_error,
		        gpu_gl_error_symbol(gl_error),
		        gpuErrorString(gl_error));

		return false;
	}
}


const char *gpuErrorString(GLenum err)
{
	switch (err) {
		case GL_NO_ERROR:
			return "No Error";

		case GL_INVALID_ENUM:
			return "Invalid Enumeration";

		case GL_INVALID_VALUE:
			return "Invalid Value";

		case GL_INVALID_OPERATION:
			return "Invalid Operation";

		case GL_STACK_OVERFLOW:
			return "Stack Overflow";

		case GL_STACK_UNDERFLOW:
			return "Stack Underflow";

		case GL_OUT_OF_MEMORY:
			return "Out of Memory";

#if GL_ARB_imaging
		case GL_TABLE_TOO_LARGE:
			return "Table Too Large";
#endif

#if defined(WITH_GLU)
		case GLU_INVALID_ENUM:
			return "Invalid Enum (GLU)";

		case GLU_INVALID_VALUE:
			return "Invalid Value (GLU)";

		case GLU_OUT_OF_MEMORY:
			return "Out of Memory (GLU)";
#endif

		default:
			return "<unknown error>";
	}
}


/* Debug callbacks need the same calling convention as OpenGL functions.
 */
#if defined(_WIN32) && !defined(_WIN32_WCE) && !defined(__SCITECH_SNAP__)
    /* Win32 but not WinCE */
#   define APIENTRY __stdcall
#else
#   define APIENTRY
#endif


static const char *source_name(GLenum source)
{
	switch (source) {
		case GL_DEBUG_SOURCE_API: return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "window system";
		case GL_DEBUG_SOURCE_SHADER_COMPILER: return "shader compiler";
		case GL_DEBUG_SOURCE_THIRD_PARTY: return "3rd party";
		case GL_DEBUG_SOURCE_APPLICATION: return "application";
		case GL_DEBUG_SOURCE_OTHER: return "other";
		default: return "???";
	}
}

static const char *message_type_name(GLenum message)
{
	switch (message) {
		case GL_DEBUG_TYPE_ERROR: return "error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "deprecated behavior";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "undefined behavior";
		case GL_DEBUG_TYPE_PORTABILITY: return "portability";
		case GL_DEBUG_TYPE_PERFORMANCE: return "performance";
		case GL_DEBUG_TYPE_OTHER: return "other";
		case GL_DEBUG_TYPE_MARKER: return "marker"; /* KHR has this, ARB does not */
		default: return "???";
	}
}

static const char *category_name_amd(GLenum category)
{
	switch (category) {
		case GL_DEBUG_CATEGORY_API_ERROR_AMD: return "API error";
		case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD: return "window system";
		case GL_DEBUG_CATEGORY_DEPRECATION_AMD: return "deprecated behavior";
		case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD: return "undefined behavior";
		case GL_DEBUG_CATEGORY_PERFORMANCE_AMD: return "performance";
		case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD: return "shader compiler";
		case GL_DEBUG_CATEGORY_APPLICATION_AMD: return "application";
		case GL_DEBUG_CATEGORY_OTHER_AMD: return "other";
		default: return "???";
	}
}


static void APIENTRY gpu_debug_proc(
        GLenum source, GLenum type, GLuint UNUSED(id),
        GLenum severity, GLsizei UNUSED(length),
        const GLchar *message, const GLvoid *UNUSED(userParm))
{
	if (type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR) {
		/* Blender 2.7x uses OpenGL 2.1, we don't care if features are deprecated */
		return;
	}

	bool backtrace = false;

	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			backtrace = true;
			ATTR_FALLTHROUGH;
		case GL_DEBUG_SEVERITY_MEDIUM:
		case GL_DEBUG_SEVERITY_LOW:
		case GL_DEBUG_SEVERITY_NOTIFICATION: /* KHR has this, ARB does not */
			fprintf(stderr, "GL %s %s: %s\n", source_name(source), message_type_name(type), message);
	}

	if (backtrace) {
		BLI_system_backtrace(stderr);
		fflush(stderr);
	}
}


#ifndef GLEW_ES_ONLY
static void APIENTRY gpu_debug_proc_amd(
        GLuint UNUSED(id), GLenum category,
        GLenum severity, GLsizei UNUSED(length),
        const GLchar *message,  GLvoid *UNUSED(userParm))
{
	if (category == GL_DEBUG_CATEGORY_DEPRECATION_AMD) {
		/* Blender 2.7x uses OpenGL 2.1, we don't care if features are deprecated */
		return;
	}

	bool backtrace = false;

	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			backtrace = true;
			ATTR_FALLTHROUGH;
		case GL_DEBUG_SEVERITY_MEDIUM:
		case GL_DEBUG_SEVERITY_LOW:
			fprintf(stderr, "GL %s: %s\n", category_name_amd(category), message);
	}

	if (backtrace) {
		BLI_system_backtrace(stderr);
		fflush(stderr);
	}
}
#endif


#undef APIENTRY

void gpu_debug_init(void)
{
	const char success[] = "Successfully hooked OpenGL debug callback.";

#if !defined(WITH_GLEW_ES) && !defined(GLEW_ES_ONLY)
	if (GLEW_VERSION_4_3) {
		fprintf(stderr, "Using OpenGL 4.3 debug facilities\n");
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback((GLDEBUGPROC)gpu_debug_proc, mxGetCurrentContext());
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		GPU_string_marker(success);
		return;
	}
#endif

	if (GLEW_KHR_debug) {
#ifndef GLEW_ES_ONLY
		fprintf(stderr, "Using KHR_debug extension\n");
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback((GLDEBUGPROC)gpu_debug_proc, mxGetCurrentContext());
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		GPU_string_marker(success);
#endif
		return;
	}

#ifndef GLEW_ES_ONLY
	if (GLEW_ARB_debug_output) {
		fprintf(stderr, "Using ARB_debug_output extension\n");
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallbackARB((GLDEBUGPROCARB)gpu_debug_proc, mxGetCurrentContext());
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		GPU_string_marker(success);

		return;
	}

	if (GLEW_AMD_debug_output) {
		fprintf(stderr, "Using AMD_debug_output extension\n");
		glDebugMessageCallbackAMD(gpu_debug_proc_amd, mxGetCurrentContext());
		glDebugMessageEnableAMD(GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		GPU_string_marker(success);

		return;
	}
#endif

	fprintf(stderr, "Failed to hook OpenGL debug callback.\n");

	return;
}


void gpu_debug_exit(void)
{
#ifndef WITH_GLEW_ES
#ifndef GLEW_ES_ONLY
	if (GLEW_VERSION_4_3) {
		glDebugMessageCallback(NULL, NULL);

		return;
	}
#endif
#endif

	if (GLEW_KHR_debug) {
#ifndef GLEW_ES_ONLY
		glDebugMessageCallback(NULL, NULL);
#endif
		return;
	}

#ifndef GLEW_ES_ONLY
	if (GLEW_ARB_debug_output) {
		glDebugMessageCallbackARB(NULL, NULL);

		return;
	}

	if (GLEW_AMD_debug_output) {
		glDebugMessageCallbackAMD(NULL, NULL);

		return;
	}
#endif

	return;
}

void GPU_string_marker(const char *buf)
{
#ifndef WITH_GLEW_ES
#ifndef GLEW_ES_ONLY
	if (GLEW_VERSION_4_3) {
		glDebugMessageInsert(
		        GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
		        GL_DEBUG_SEVERITY_NOTIFICATION, -1, buf);

		return;
	}
#endif
#endif

	if (GLEW_KHR_debug) {
#ifndef GLEW_ES_ONLY
		glDebugMessageInsert(
		        GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
		        GL_DEBUG_SEVERITY_NOTIFICATION, -1, buf);
#endif
		return;
	}

#ifndef GLEW_ES_ONLY
	if (GLEW_ARB_debug_output) {
		glDebugMessageInsertARB(
		        GL_DEBUG_SOURCE_APPLICATION_ARB, GL_DEBUG_TYPE_OTHER_ARB, 0,
		        GL_DEBUG_SEVERITY_LOW_ARB, -1, buf);

		return;
	}

	if (GLEW_AMD_debug_output) {
		glDebugMessageInsertAMD(
		        GL_DEBUG_CATEGORY_APPLICATION_AMD, GL_DEBUG_SEVERITY_LOW_AMD, 0,
		        0, buf);

		return;
	}

	if (GLEW_GREMEDY_string_marker) {
		glStringMarkerGREMEDY(0, buf);

		return;
	}
#endif
}

void GPU_print_error_debug(const char *str)
{
	if (G.debug & G_DEBUG)
		fprintf(stderr, "GPU: %s\n", str);
}


void GPU_assert_no_gl_errors(const char *file, int line, const char *str)
{
	if (G.debug) {
		GLboolean gl_ok = gpu_report_gl_errors(file, line, str);

		BLI_assert(gl_ok);
		(void) gl_ok;
	}
}


static void gpu_state_print_fl_ex(const char *name, GLenum type)
{
#define MAX_ARRAY_SIZE 64

	const unsigned char err_mark[4] = {0xff, 0xff, 0xff, 0xff};

	float value[MAX_ARRAY_SIZE];
	int a;

	memset(value, 0xff, sizeof(value));
	glGetFloatv(type, value);

	if (glGetError() == GL_NO_ERROR) {
		printf("%s: ", name);
		for (a = 0; a < MAX_ARRAY_SIZE; a++) {
			if (memcmp(&value[a], err_mark, sizeof(value[a])) == 0) {
				break;
			}
			printf("%.2f ", value[a]);
		}
		printf("\n");
	}

#undef MAX_ARRAY_SIZE
}

#define gpu_state_print_fl(val) gpu_state_print_fl_ex(#val, val)

void GPU_state_print(void)
{
	GPU_ASSERT_NO_GL_ERRORS("GPU_state_print"); /* clear any errors */

	gpu_state_print_fl(GL_ACCUM_ALPHA_BITS);
	gpu_state_print_fl(GL_ACCUM_BLUE_BITS);
	gpu_state_print_fl(GL_ACCUM_CLEAR_VALUE);
	gpu_state_print_fl(GL_ACCUM_GREEN_BITS);
	gpu_state_print_fl(GL_ACCUM_RED_BITS);
	gpu_state_print_fl(GL_ACTIVE_TEXTURE);
	gpu_state_print_fl(GL_ALIASED_LINE_WIDTH_RANGE);
	gpu_state_print_fl(GL_ALIASED_POINT_SIZE_RANGE);
	gpu_state_print_fl(GL_ALPHA_BIAS);
	gpu_state_print_fl(GL_ALPHA_BITS);
	gpu_state_print_fl(GL_ALPHA_SCALE);
	gpu_state_print_fl(GL_ALPHA_TEST);
	gpu_state_print_fl(GL_ALPHA_TEST_FUNC);
	gpu_state_print_fl(GL_ALPHA_TEST_REF);
	gpu_state_print_fl(GL_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_ATTRIB_STACK_DEPTH);
	gpu_state_print_fl(GL_AUTO_NORMAL);
	gpu_state_print_fl(GL_AUX_BUFFERS);
	gpu_state_print_fl(GL_BLEND);
	gpu_state_print_fl(GL_BLEND_COLOR);
	gpu_state_print_fl(GL_BLEND_DST_ALPHA);
	gpu_state_print_fl(GL_BLEND_DST_RGB);
	gpu_state_print_fl(GL_BLEND_EQUATION_ALPHA);
	gpu_state_print_fl(GL_BLEND_EQUATION_RGB);
	gpu_state_print_fl(GL_BLEND_SRC_ALPHA);
	gpu_state_print_fl(GL_BLEND_SRC_RGB);
	gpu_state_print_fl(GL_BLUE_BIAS);
	gpu_state_print_fl(GL_BLUE_BITS);
	gpu_state_print_fl(GL_BLUE_SCALE);
	gpu_state_print_fl(GL_CLIENT_ACTIVE_TEXTURE);
	gpu_state_print_fl(GL_CLIENT_ATTRIB_STACK_DEPTH);
	gpu_state_print_fl(GL_CLIP_PLANE0);
	gpu_state_print_fl(GL_COLOR_ARRAY);
	gpu_state_print_fl(GL_COLOR_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_COLOR_ARRAY_SIZE);
	gpu_state_print_fl(GL_COLOR_ARRAY_STRIDE);
	gpu_state_print_fl(GL_COLOR_ARRAY_TYPE);
	gpu_state_print_fl(GL_COLOR_CLEAR_VALUE);
	gpu_state_print_fl(GL_COLOR_LOGIC_OP);
	gpu_state_print_fl(GL_COLOR_MATERIAL);
	gpu_state_print_fl(GL_COLOR_MATERIAL_FACE);
	gpu_state_print_fl(GL_COLOR_MATERIAL_PARAMETER);
	gpu_state_print_fl(GL_COLOR_MATRIX);
	gpu_state_print_fl(GL_COLOR_MATRIX_STACK_DEPTH);
	gpu_state_print_fl(GL_COLOR_SUM);
	gpu_state_print_fl(GL_COLOR_TABLE);
	gpu_state_print_fl(GL_COLOR_WRITEMASK);
	gpu_state_print_fl(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
	gpu_state_print_fl(GL_COMPRESSED_TEXTURE_FORMATS);
	gpu_state_print_fl(GL_CONVOLUTION_1D);
	gpu_state_print_fl(GL_CONVOLUTION_2D);
	gpu_state_print_fl(GL_CULL_FACE);
	gpu_state_print_fl(GL_CULL_FACE_MODE);
	gpu_state_print_fl(GL_CURRENT_COLOR);
	gpu_state_print_fl(GL_CURRENT_FOG_COORD);
	gpu_state_print_fl(GL_CURRENT_INDEX);
	gpu_state_print_fl(GL_CURRENT_NORMAL);
	gpu_state_print_fl(GL_CURRENT_PROGRAM);
	gpu_state_print_fl(GL_CURRENT_RASTER_COLOR);
	gpu_state_print_fl(GL_CURRENT_RASTER_DISTANCE);
	gpu_state_print_fl(GL_CURRENT_RASTER_INDEX);
	gpu_state_print_fl(GL_CURRENT_RASTER_POSITION);
	gpu_state_print_fl(GL_CURRENT_RASTER_POSITION_VALID);
	gpu_state_print_fl(GL_CURRENT_RASTER_SECONDARY_COLOR);
	gpu_state_print_fl(GL_CURRENT_RASTER_TEXTURE_COORDS);
	gpu_state_print_fl(GL_CURRENT_SECONDARY_COLOR);
	gpu_state_print_fl(GL_CURRENT_TEXTURE_COORDS);
	gpu_state_print_fl(GL_DEPTH_BIAS);
	gpu_state_print_fl(GL_DEPTH_BITS);
	gpu_state_print_fl(GL_DEPTH_CLEAR_VALUE);
	gpu_state_print_fl(GL_DEPTH_FUNC);
	gpu_state_print_fl(GL_DEPTH_RANGE);
	gpu_state_print_fl(GL_DEPTH_SCALE);
	gpu_state_print_fl(GL_DEPTH_TEST);
	gpu_state_print_fl(GL_DEPTH_WRITEMASK);
	gpu_state_print_fl(GL_DITHER);
	gpu_state_print_fl(GL_DOUBLEBUFFER);
	gpu_state_print_fl(GL_DRAW_BUFFER);
	gpu_state_print_fl(GL_DRAW_BUFFER0);
	gpu_state_print_fl(GL_EDGE_FLAG);
	gpu_state_print_fl(GL_EDGE_FLAG_ARRAY);
	gpu_state_print_fl(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_EDGE_FLAG_ARRAY_STRIDE);
	gpu_state_print_fl(GL_ELEMENT_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_FEEDBACK_BUFFER_SIZE);
	gpu_state_print_fl(GL_FEEDBACK_BUFFER_TYPE);
	gpu_state_print_fl(GL_FOG);
	gpu_state_print_fl(GL_FOG_COLOR);
	gpu_state_print_fl(GL_FOG_COORD_ARRAY);
	gpu_state_print_fl(GL_FOG_COORD_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_FOG_COORD_ARRAY_STRIDE);
	gpu_state_print_fl(GL_FOG_COORD_ARRAY_TYPE);
	gpu_state_print_fl(GL_FOG_COORD_SRC);
	gpu_state_print_fl(GL_FOG_DENSITY);
	gpu_state_print_fl(GL_FOG_END);
	gpu_state_print_fl(GL_FOG_HINT);
	gpu_state_print_fl(GL_FOG_INDEX);
	gpu_state_print_fl(GL_FOG_MODE);
	gpu_state_print_fl(GL_FOG_START);
	gpu_state_print_fl(GL_FRAGMENT_PROGRAM_ARB); /* TODO: remove ARB program support */
	gpu_state_print_fl(GL_FRAGMENT_SHADER_DERIVATIVE_HINT);
	gpu_state_print_fl(GL_FRONT_FACE);
	gpu_state_print_fl(GL_GENERATE_MIPMAP_HINT);
	gpu_state_print_fl(GL_GREEN_BIAS);
	gpu_state_print_fl(GL_GREEN_BITS);
	gpu_state_print_fl(GL_GREEN_SCALE);
	gpu_state_print_fl(GL_HISTOGRAM);
	gpu_state_print_fl(GL_INDEX_ARRAY);
	gpu_state_print_fl(GL_INDEX_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_INDEX_ARRAY_STRIDE);
	gpu_state_print_fl(GL_INDEX_ARRAY_TYPE);
	gpu_state_print_fl(GL_INDEX_BITS);
	gpu_state_print_fl(GL_INDEX_CLEAR_VALUE);
	gpu_state_print_fl(GL_INDEX_LOGIC_OP);
	gpu_state_print_fl(GL_INDEX_MODE);
	gpu_state_print_fl(GL_INDEX_OFFSET);
	gpu_state_print_fl(GL_INDEX_SHIFT);
	gpu_state_print_fl(GL_INDEX_WRITEMASK);
	gpu_state_print_fl(GL_LIGHT0);
	gpu_state_print_fl(GL_LIGHT1);
	gpu_state_print_fl(GL_LIGHT2);
	gpu_state_print_fl(GL_LIGHT3);
	gpu_state_print_fl(GL_LIGHT4);
	gpu_state_print_fl(GL_LIGHT5);
	gpu_state_print_fl(GL_LIGHT6);
	gpu_state_print_fl(GL_LIGHT7);
	gpu_state_print_fl(GL_LIGHTING);
	gpu_state_print_fl(GL_LIGHT_MODEL_AMBIENT);
	gpu_state_print_fl(GL_LIGHT_MODEL_COLOR_CONTROL);
	gpu_state_print_fl(GL_LIGHT_MODEL_LOCAL_VIEWER);
	gpu_state_print_fl(GL_LIGHT_MODEL_TWO_SIDE);
	gpu_state_print_fl(GL_LINE_SMOOTH);
	gpu_state_print_fl(GL_LINE_SMOOTH_HINT);
	gpu_state_print_fl(GL_LINE_STIPPLE);
	gpu_state_print_fl(GL_LINE_STIPPLE_PATTERN);
	gpu_state_print_fl(GL_LINE_STIPPLE_REPEAT);
	gpu_state_print_fl(GL_LINE_WIDTH);
	gpu_state_print_fl(GL_LINE_WIDTH_GRANULARITY);
	gpu_state_print_fl(GL_LINE_WIDTH_RANGE);
	gpu_state_print_fl(GL_LIST_BASE);
	gpu_state_print_fl(GL_LIST_INDEX);
	gpu_state_print_fl(GL_LIST_MODE);
	gpu_state_print_fl(GL_LOGIC_OP);
	gpu_state_print_fl(GL_LOGIC_OP_MODE);
	gpu_state_print_fl(GL_MAP1_COLOR_4);
	gpu_state_print_fl(GL_MAP1_GRID_DOMAIN);
	gpu_state_print_fl(GL_MAP1_GRID_SEGMENTS);
	gpu_state_print_fl(GL_MAP1_INDEX);
	gpu_state_print_fl(GL_MAP1_NORMAL);
	gpu_state_print_fl(GL_MAP1_TEXTURE_COORD_1);
	gpu_state_print_fl(GL_MAP1_TEXTURE_COORD_2);
	gpu_state_print_fl(GL_MAP1_TEXTURE_COORD_3);
	gpu_state_print_fl(GL_MAP1_TEXTURE_COORD_4);
	gpu_state_print_fl(GL_MAP1_VERTEX_3);
	gpu_state_print_fl(GL_MAP1_VERTEX_4);
	gpu_state_print_fl(GL_MAP2_COLOR_4);
	gpu_state_print_fl(GL_MAP2_GRID_DOMAIN);
	gpu_state_print_fl(GL_MAP2_GRID_SEGMENTS);
	gpu_state_print_fl(GL_MAP2_INDEX);
	gpu_state_print_fl(GL_MAP2_NORMAL);
	gpu_state_print_fl(GL_MAP2_TEXTURE_COORD_1);
	gpu_state_print_fl(GL_MAP2_TEXTURE_COORD_2);
	gpu_state_print_fl(GL_MAP2_TEXTURE_COORD_3);
	gpu_state_print_fl(GL_MAP2_TEXTURE_COORD_4);
	gpu_state_print_fl(GL_MAP2_VERTEX_3);
	gpu_state_print_fl(GL_MAP2_VERTEX_4);
	gpu_state_print_fl(GL_MAP_COLOR);
	gpu_state_print_fl(GL_MAP_STENCIL);
	gpu_state_print_fl(GL_MATRIX_MODE);
	gpu_state_print_fl(GL_MAX_3D_TEXTURE_SIZE);
	gpu_state_print_fl(GL_MAX_ATTRIB_STACK_DEPTH);
	gpu_state_print_fl(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH);
	gpu_state_print_fl(GL_MAX_CLIP_PLANES);
	gpu_state_print_fl(GL_MAX_COLOR_MATRIX_STACK_DEPTH);
	gpu_state_print_fl(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	gpu_state_print_fl(GL_MAX_CUBE_MAP_TEXTURE_SIZE);
	gpu_state_print_fl(GL_MAX_DRAW_BUFFERS);
	gpu_state_print_fl(GL_MAX_ELEMENTS_INDICES);
	gpu_state_print_fl(GL_MAX_ELEMENTS_VERTICES);
	gpu_state_print_fl(GL_MAX_EVAL_ORDER);
	gpu_state_print_fl(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
	gpu_state_print_fl(GL_MAX_LIGHTS);
	gpu_state_print_fl(GL_MAX_LIST_NESTING);
	gpu_state_print_fl(GL_MAX_MODELVIEW_STACK_DEPTH);
	gpu_state_print_fl(GL_MAX_NAME_STACK_DEPTH);
	gpu_state_print_fl(GL_MAX_PIXEL_MAP_TABLE);
	gpu_state_print_fl(GL_MAX_PROJECTION_STACK_DEPTH);
	gpu_state_print_fl(GL_MAX_TEXTURE_COORDS);
	gpu_state_print_fl(GL_MAX_TEXTURE_IMAGE_UNITS);
	gpu_state_print_fl(GL_MAX_TEXTURE_LOD_BIAS);
	gpu_state_print_fl(GL_MAX_TEXTURE_SIZE);
	gpu_state_print_fl(GL_MAX_TEXTURE_STACK_DEPTH);
	gpu_state_print_fl(GL_MAX_TEXTURE_UNITS);
	gpu_state_print_fl(GL_MAX_VARYING_FLOATS);
	gpu_state_print_fl(GL_MAX_VERTEX_ATTRIBS);
	gpu_state_print_fl(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
	gpu_state_print_fl(GL_MAX_VERTEX_UNIFORM_COMPONENTS);
	gpu_state_print_fl(GL_MAX_VIEWPORT_DIMS);
	gpu_state_print_fl(GL_MINMAX);
	gpu_state_print_fl(GL_MODELVIEW_MATRIX);
	gpu_state_print_fl(GL_MODELVIEW_STACK_DEPTH);
	gpu_state_print_fl(GL_MULTISAMPLE);
	gpu_state_print_fl(GL_NAME_STACK_DEPTH);
	gpu_state_print_fl(GL_NORMALIZE);
	gpu_state_print_fl(GL_NORMAL_ARRAY);
	gpu_state_print_fl(GL_NORMAL_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_NORMAL_ARRAY_STRIDE);
	gpu_state_print_fl(GL_NORMAL_ARRAY_TYPE);
	gpu_state_print_fl(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
	gpu_state_print_fl(GL_PACK_ALIGNMENT);
	gpu_state_print_fl(GL_PACK_IMAGE_HEIGHT);
	gpu_state_print_fl(GL_PACK_LSB_FIRST);
	gpu_state_print_fl(GL_PACK_ROW_LENGTH);
	gpu_state_print_fl(GL_PACK_SKIP_IMAGES);
	gpu_state_print_fl(GL_PACK_SKIP_PIXELS);
	gpu_state_print_fl(GL_PACK_SKIP_ROWS);
	gpu_state_print_fl(GL_PACK_SWAP_BYTES);
	gpu_state_print_fl(GL_PERSPECTIVE_CORRECTION_HINT);
	gpu_state_print_fl(GL_PIXEL_MAP_A_TO_A_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_B_TO_B_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_G_TO_G_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_I_TO_A_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_I_TO_B_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_I_TO_G_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_I_TO_I_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_I_TO_R_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_R_TO_R_SIZE);
	gpu_state_print_fl(GL_PIXEL_MAP_S_TO_S_SIZE);
	gpu_state_print_fl(GL_PIXEL_PACK_BUFFER_BINDING);
	gpu_state_print_fl(GL_PIXEL_UNPACK_BUFFER_BINDING);
	gpu_state_print_fl(GL_POINT_DISTANCE_ATTENUATION);
	gpu_state_print_fl(GL_POINT_FADE_THRESHOLD_SIZE);
	gpu_state_print_fl(GL_POINT_SIZE);
	gpu_state_print_fl(GL_POINT_SIZE_GRANULARITY);
	gpu_state_print_fl(GL_POINT_SIZE_MAX);
	gpu_state_print_fl(GL_POINT_SIZE_MIN);
	gpu_state_print_fl(GL_POINT_SIZE_RANGE);
	gpu_state_print_fl(GL_POINT_SMOOTH);
	gpu_state_print_fl(GL_POINT_SMOOTH_HINT);
	gpu_state_print_fl(GL_POINT_SPRITE);
	gpu_state_print_fl(GL_POLYGON_MODE);
	gpu_state_print_fl(GL_POLYGON_OFFSET_FACTOR);
	gpu_state_print_fl(GL_POLYGON_OFFSET_FILL);
	gpu_state_print_fl(GL_POLYGON_OFFSET_LINE);
	gpu_state_print_fl(GL_POLYGON_OFFSET_POINT);
	gpu_state_print_fl(GL_POLYGON_OFFSET_UNITS);
	gpu_state_print_fl(GL_POLYGON_SMOOTH);
	gpu_state_print_fl(GL_POLYGON_SMOOTH_HINT);
	gpu_state_print_fl(GL_POLYGON_STIPPLE);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_ALPHA_BIAS);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_ALPHA_SCALE);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_BLUE_BIAS);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_BLUE_SCALE);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_COLOR_TABLE);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_GREEN_BIAS);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_GREEN_SCALE);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_RED_BIAS);
	gpu_state_print_fl(GL_POST_COLOR_MATRIX_RED_SCALE);
	gpu_state_print_fl(GL_POST_CONVOLUTION_ALPHA_BIAS);
	gpu_state_print_fl(GL_POST_CONVOLUTION_ALPHA_SCALE);
	gpu_state_print_fl(GL_POST_CONVOLUTION_BLUE_BIAS);
	gpu_state_print_fl(GL_POST_CONVOLUTION_BLUE_SCALE);
	gpu_state_print_fl(GL_POST_CONVOLUTION_COLOR_TABLE);
	gpu_state_print_fl(GL_POST_CONVOLUTION_GREEN_BIAS);
	gpu_state_print_fl(GL_POST_CONVOLUTION_GREEN_SCALE);
	gpu_state_print_fl(GL_POST_CONVOLUTION_RED_BIAS);
	gpu_state_print_fl(GL_POST_CONVOLUTION_RED_SCALE);
	gpu_state_print_fl(GL_PROJECTION_MATRIX);
	gpu_state_print_fl(GL_PROJECTION_STACK_DEPTH);
	gpu_state_print_fl(GL_READ_BUFFER);
	gpu_state_print_fl(GL_RED_BIAS);
	gpu_state_print_fl(GL_RED_BITS);
	gpu_state_print_fl(GL_RED_SCALE);
	gpu_state_print_fl(GL_RENDER_MODE);
	gpu_state_print_fl(GL_RESCALE_NORMAL);
	gpu_state_print_fl(GL_RGBA_MODE);
	gpu_state_print_fl(GL_SAMPLES);
	gpu_state_print_fl(GL_SAMPLE_BUFFERS);
	gpu_state_print_fl(GL_SAMPLE_COVERAGE_INVERT);
	gpu_state_print_fl(GL_SAMPLE_COVERAGE_VALUE);
	gpu_state_print_fl(GL_SCISSOR_BOX);
	gpu_state_print_fl(GL_SCISSOR_TEST);
	gpu_state_print_fl(GL_SECONDARY_COLOR_ARRAY);
	gpu_state_print_fl(GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_SECONDARY_COLOR_ARRAY_SIZE);
	gpu_state_print_fl(GL_SECONDARY_COLOR_ARRAY_STRIDE);
	gpu_state_print_fl(GL_SECONDARY_COLOR_ARRAY_TYPE);
	gpu_state_print_fl(GL_SELECTION_BUFFER_SIZE);
	gpu_state_print_fl(GL_SEPARABLE_2D);
	gpu_state_print_fl(GL_SHADE_MODEL);
	gpu_state_print_fl(GL_SMOOTH_LINE_WIDTH_GRANULARITY);
	gpu_state_print_fl(GL_SMOOTH_LINE_WIDTH_RANGE);
	gpu_state_print_fl(GL_SMOOTH_POINT_SIZE_GRANULARITY);
	gpu_state_print_fl(GL_SMOOTH_POINT_SIZE_RANGE);
	gpu_state_print_fl(GL_STENCIL_BACK_FAIL);
	gpu_state_print_fl(GL_STENCIL_BACK_FUNC);
	gpu_state_print_fl(GL_STENCIL_BACK_PASS_DEPTH_FAIL);
	gpu_state_print_fl(GL_STENCIL_BACK_PASS_DEPTH_PASS);
	gpu_state_print_fl(GL_STENCIL_BACK_REF);
	gpu_state_print_fl(GL_STENCIL_BACK_VALUE_MASK);
	gpu_state_print_fl(GL_STENCIL_BACK_WRITEMASK);
	gpu_state_print_fl(GL_STENCIL_BITS);
	gpu_state_print_fl(GL_STENCIL_CLEAR_VALUE);
	gpu_state_print_fl(GL_STENCIL_FAIL);
	gpu_state_print_fl(GL_STENCIL_FUNC);
	gpu_state_print_fl(GL_STENCIL_PASS_DEPTH_FAIL);
	gpu_state_print_fl(GL_STENCIL_PASS_DEPTH_PASS);
	gpu_state_print_fl(GL_STENCIL_REF);
	gpu_state_print_fl(GL_STENCIL_TEST);
	gpu_state_print_fl(GL_STENCIL_VALUE_MASK);
	gpu_state_print_fl(GL_STENCIL_WRITEMASK);
	gpu_state_print_fl(GL_STEREO);
	gpu_state_print_fl(GL_SUBPIXEL_BITS);
	gpu_state_print_fl(GL_TEXTURE_1D);
	gpu_state_print_fl(GL_TEXTURE_2D);
	gpu_state_print_fl(GL_TEXTURE_3D);
	gpu_state_print_fl(GL_TEXTURE_BINDING_1D);
	gpu_state_print_fl(GL_TEXTURE_BINDING_2D);
	gpu_state_print_fl(GL_TEXTURE_BINDING_3D);
	gpu_state_print_fl(GL_TEXTURE_BINDING_CUBE_MAP);
	gpu_state_print_fl(GL_TEXTURE_COMPRESSION_HINT);
	gpu_state_print_fl(GL_TEXTURE_COORD_ARRAY);
	gpu_state_print_fl(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_TEXTURE_COORD_ARRAY_SIZE);
	gpu_state_print_fl(GL_TEXTURE_COORD_ARRAY_STRIDE);
	gpu_state_print_fl(GL_TEXTURE_COORD_ARRAY_TYPE);
	gpu_state_print_fl(GL_TEXTURE_CUBE_MAP);
	gpu_state_print_fl(GL_TEXTURE_GEN_Q);
	gpu_state_print_fl(GL_TEXTURE_GEN_R);
	gpu_state_print_fl(GL_TEXTURE_GEN_S);
	gpu_state_print_fl(GL_TEXTURE_GEN_T);
	gpu_state_print_fl(GL_TEXTURE_MATRIX);
	gpu_state_print_fl(GL_TEXTURE_STACK_DEPTH);
	gpu_state_print_fl(GL_TRANSPOSE_COLOR_MATRIX);
	gpu_state_print_fl(GL_TRANSPOSE_MODELVIEW_MATRIX);
	gpu_state_print_fl(GL_TRANSPOSE_PROJECTION_MATRIX);
	gpu_state_print_fl(GL_TRANSPOSE_TEXTURE_MATRIX);
	gpu_state_print_fl(GL_UNPACK_ALIGNMENT);
	gpu_state_print_fl(GL_UNPACK_IMAGE_HEIGHT);
	gpu_state_print_fl(GL_UNPACK_LSB_FIRST);
	gpu_state_print_fl(GL_UNPACK_ROW_LENGTH);
	gpu_state_print_fl(GL_UNPACK_SKIP_IMAGES);
	gpu_state_print_fl(GL_UNPACK_SKIP_PIXELS);
	gpu_state_print_fl(GL_UNPACK_SKIP_ROWS);
	gpu_state_print_fl(GL_UNPACK_SWAP_BYTES);
	gpu_state_print_fl(GL_VERTEX_ARRAY);
	gpu_state_print_fl(GL_VERTEX_ARRAY_BUFFER_BINDING);
	gpu_state_print_fl(GL_VERTEX_ARRAY_SIZE);
	gpu_state_print_fl(GL_VERTEX_ARRAY_STRIDE);
	gpu_state_print_fl(GL_VERTEX_ARRAY_TYPE);
	gpu_state_print_fl(GL_VERTEX_PROGRAM_POINT_SIZE);
	gpu_state_print_fl(GL_VERTEX_PROGRAM_TWO_SIDE);
	gpu_state_print_fl(GL_VIEWPORT);
	gpu_state_print_fl(GL_ZOOM_X);
	gpu_state_print_fl(GL_ZOOM_Y);
}

#undef gpu_state_print_fl
