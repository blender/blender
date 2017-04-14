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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"

#include "BKE_global.h"

#include "GPU_compositing.h"
#include "GPU_debug.h"
#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_material.h"

/* TODO(sergey): Find better default values for this constants. */
#define MAX_DEFINE_LENGTH 1024
#define MAX_EXT_DEFINE_LENGTH 1024

/* Non-generated shaders */
extern char datatoc_gpu_shader_fire_frag_glsl[];
extern char datatoc_gpu_shader_smoke_vert_glsl[];
extern char datatoc_gpu_shader_smoke_frag_glsl[];
extern char datatoc_gpu_shader_vsm_store_vert_glsl[];
extern char datatoc_gpu_shader_vsm_store_frag_glsl[];
extern char datatoc_gpu_shader_sep_gaussian_blur_vert_glsl[];
extern char datatoc_gpu_shader_sep_gaussian_blur_frag_glsl[];
extern char datatoc_gpu_shader_fx_vert_glsl[];
extern char datatoc_gpu_shader_fx_ssao_frag_glsl[];
extern char datatoc_gpu_shader_fx_dof_frag_glsl[];
extern char datatoc_gpu_shader_fx_dof_vert_glsl[];
extern char datatoc_gpu_shader_fx_dof_hq_frag_glsl[];
extern char datatoc_gpu_shader_fx_dof_hq_vert_glsl[];
extern char datatoc_gpu_shader_fx_dof_hq_geo_glsl[];
extern char datatoc_gpu_shader_fx_depth_resolve_glsl[];
extern char datatoc_gpu_shader_fx_lib_glsl[];

static struct GPUShadersGlobal {
	struct {
		GPUShader *vsm_store;
		GPUShader *sep_gaussian_blur;
		GPUShader *smoke;
		GPUShader *smoke_fire;
		GPUShader *smoke_coba;
		/* cache for shader fx. Those can exist in combinations so store them here */
		GPUShader *fx_shaders[MAX_FX_SHADERS * 2];
	} shaders;
} GG = {{NULL}};

/* GPUShader */

struct GPUShader {
	GLuint program;  /* handle for full program (links shader stages below) */

	GLuint vertex;   /* handle for vertex shader */
	GLuint geometry; /* handle for geometry shader */
	GLuint fragment; /* handle for fragment shader */

	int totattrib;   /* total number of attributes */
	int uniforms;    /* required uniforms */

	void *uniform_interface; /* cached uniform interface for shader. Data depends on shader */
};

static void shader_print_errors(const char *task, const char *log, const char **code, int totcode)
{
	int i;
	int line = 1;

	fprintf(stderr, "GPUShader: %s error:\n", task);

	for (i = 0; i < totcode; i++) {
		const char *c, *pos, *end = code[i] + strlen(code[i]);

		if (G.debug & G_DEBUG) {
			fprintf(stderr, "===== shader string %d ====\n", i + 1);

			c = code[i];
			while ((c < end) && (pos = strchr(c, '\n'))) {
				fprintf(stderr, "%2d  ", line);
				fwrite(c, (pos + 1) - c, 1, stderr);
				c = pos + 1;
				line++;
			}
			
			fprintf(stderr, "%s", c);
		}
	}
	
	fprintf(stderr, "%s\n", log);
}

static const char *gpu_shader_version(void)
{
	if (GLEW_VERSION_3_2) {
		if (GLEW_ARB_compatibility) {
			return "#version 150 compatibility\n";
			/* highest version that is widely supported
			 * gives us native geometry shaders!
			 * use compatibility profile so we can continue using builtin shader input/output names
			 */
		}
		else {
			return "#version 130\n";
			/* latest version that is compatible with existing shaders */
		}
	}
	else if (GLEW_VERSION_3_1) {
		if (GLEW_ARB_compatibility) {
			return "#version 140\n";
			/* also need the ARB_compatibility extension, handled below */
		}
		else {
			return "#version 130\n";
			/* latest version that is compatible with existing shaders */
		}
	}
	else if (GLEW_VERSION_3_0) {
		return "#version 130\n";
		/* GLSL 1.3 has modern syntax/keywords/datatypes so use if available
		 * older features are deprecated but still available without compatibility extension or profile
		 */
	}
	else {
		return "#version 120\n";
		/* minimum supported */
	}
}


static void gpu_shader_standard_extensions(char defines[MAX_EXT_DEFINE_LENGTH], bool use_geometry_shader)
{
	/* enable extensions for features that are not part of our base GLSL version
	 * don't use an extension for something already available!
	 */

	if (GLEW_ARB_texture_query_lod) {
		/* a #version 400 feature, but we use #version 150 maximum so use extension */
		strcat(defines, "#extension GL_ARB_texture_query_lod: enable\n");
	}

	if (use_geometry_shader && GPU_geometry_shader_support_via_extension()) {
		strcat(defines, "#extension GL_EXT_geometry_shader4: enable\n");
	}

	if (GLEW_VERSION_3_1 && !GLEW_VERSION_3_2 && GLEW_ARB_compatibility) {
		strcat(defines, "#extension GL_ARB_compatibility: enable\n");
	}

	if (!GLEW_VERSION_3_1) {
		if (GLEW_ARB_draw_instanced) {
			strcat(defines, "#extension GL_ARB_draw_instanced: enable\n");
		}

		if (!GLEW_VERSION_3_0 && GLEW_EXT_gpu_shader4) {
			strcat(defines, "#extension GL_EXT_gpu_shader4: enable\n");
			/* TODO: maybe require this? shaders become so much nicer */
		}
	}
}

static void gpu_shader_standard_defines(char defines[MAX_DEFINE_LENGTH],
                                        bool use_opensubdiv,
                                        bool use_new_shading)
{
	/* some useful defines to detect GPU type */
	if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
		strcat(defines, "#define GPU_ATI\n");
		if (GLEW_VERSION_3_0) {
			/* TODO(merwin): revisit this version check; GLEW_VERSION_3_0 means GL 3.0 or newer */
			strcat(defines, "#define CLIP_WORKAROUND\n");
		}
	}
	else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY))
		strcat(defines, "#define GPU_NVIDIA\n");
	else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY))
		strcat(defines, "#define GPU_INTEL\n");

	if (GPU_bicubic_bump_support())
		strcat(defines, "#define BUMP_BICUBIC\n");

	if (GLEW_VERSION_3_0) {
		strcat(defines, "#define BIT_OPERATIONS\n");
	}

#ifdef WITH_OPENSUBDIV
	/* TODO(sergey): Check whether we actually compiling shader for
	 * the OpenSubdiv mesh.
	 */
	if (use_opensubdiv) {
		strcat(defines, "#define USE_OPENSUBDIV\n");

		/* TODO(sergey): not strictly speaking a define, but this is
		 * a global typedef which we don't have better place to define
		 * in yet.
		 */
		strcat(defines, "struct VertexData {\n"
		                "  vec4 position;\n"
		                "  vec3 normal;\n"
		                "  vec2 uv;"
		                "};\n");
	}
#else
	UNUSED_VARS(use_opensubdiv);
#endif

	if (use_new_shading) {
		strcat(defines, "#define USE_NEW_SHADING\n");
	}

	return;
}

GPUShader *GPU_shader_create(const char *vertexcode,
                             const char *fragcode,
                             const char *geocode,
                             const char *libcode,
                             const char *defines,
                             int input,
                             int output,
                             int number)
{
	return GPU_shader_create_ex(vertexcode,
	                            fragcode,
	                            geocode,
	                            libcode,
	                            defines,
	                            input,
	                            output,
	                            number,
	                            GPU_SHADER_FLAGS_NONE);
}

GPUShader *GPU_shader_create_ex(const char *vertexcode,
                                const char *fragcode,
                                const char *geocode,
                                const char *libcode,
                                const char *defines,
                                int input,
                                int output,
                                int number,
                                const int flags)
{
#ifdef WITH_OPENSUBDIV
	/* TODO(sergey): used to add #version 150 to the geometry shader.
	 * Could safely be renamed to "use_geometry_code" since it's very
	 * likely any of geometry code will want to use GLSL 1.5.
	 */
	bool use_opensubdiv = (flags & GPU_SHADER_FLAGS_SPECIAL_OPENSUBDIV) != 0;
#else
	UNUSED_VARS(flags);
	bool use_opensubdiv = false;
#endif
	GLint status;
	GLchar log[5000];
	GLsizei length = 0;
	GPUShader *shader;
	char standard_defines[MAX_DEFINE_LENGTH] = "";
	char standard_extensions[MAX_EXT_DEFINE_LENGTH] = "";

	if (geocode && !GPU_geometry_shader_support())
		return NULL;

	shader = MEM_callocN(sizeof(GPUShader), "GPUShader");

	if (vertexcode)
		shader->vertex = glCreateShader(GL_VERTEX_SHADER);
	if (fragcode)
		shader->fragment = glCreateShader(GL_FRAGMENT_SHADER);
	if (geocode)
		shader->geometry = glCreateShader(GL_GEOMETRY_SHADER_EXT);

	shader->program = glCreateProgram();

	if (!shader->program ||
	    (vertexcode && !shader->vertex) ||
	    (fragcode && !shader->fragment) ||
	    (geocode && !shader->geometry))
	{
		fprintf(stderr, "GPUShader, object creation failed.\n");
		GPU_shader_free(shader);
		return NULL;
	}

	gpu_shader_standard_defines(standard_defines,
	                            use_opensubdiv,
	                            (flags & GPU_SHADER_FLAGS_NEW_SHADING) != 0);
	gpu_shader_standard_extensions(standard_extensions, geocode != NULL);

	if (vertexcode) {
		const char *source[5];
		/* custom limit, may be too small, beware */
		int num_source = 0;

		source[num_source++] = gpu_shader_version();
		source[num_source++] = standard_extensions;
		source[num_source++] = standard_defines;

		if (defines) source[num_source++] = defines;
		source[num_source++] = vertexcode;

		glAttachShader(shader->program, shader->vertex);
		glShaderSource(shader->vertex, num_source, source, NULL);

		glCompileShader(shader->vertex);
		glGetShaderiv(shader->vertex, GL_COMPILE_STATUS, &status);

		if (!status) {
			glGetShaderInfoLog(shader->vertex, sizeof(log), &length, log);
			shader_print_errors("compile", log, source, num_source);

			GPU_shader_free(shader);
			return NULL;
		}
	}

	if (fragcode) {
		const char *source[7];
		int num_source = 0;

		source[num_source++] = gpu_shader_version();
		source[num_source++] = standard_extensions;
		source[num_source++] = standard_defines;

#ifdef WITH_OPENSUBDIV
		/* TODO(sergey): Move to fragment shader source code generation. */
		if (use_opensubdiv) {
			source[num_source++] =
			        "#ifdef USE_OPENSUBDIV\n"
			        "in block {\n"
			        "	VertexData v;\n"
			        "} inpt;\n"
			        "#endif\n";
		}
#endif

		if (defines) source[num_source++] = defines;
		if (libcode) source[num_source++] = libcode;
		source[num_source++] = fragcode;

		glAttachShader(shader->program, shader->fragment);
		glShaderSource(shader->fragment, num_source, source, NULL);

		glCompileShader(shader->fragment);
		glGetShaderiv(shader->fragment, GL_COMPILE_STATUS, &status);

		if (!status) {
			glGetShaderInfoLog(shader->fragment, sizeof(log), &length, log);
			shader_print_errors("compile", log, source, num_source);

			GPU_shader_free(shader);
			return NULL;
		}
	}

	if (geocode) {
		const char *source[6];
		int num_source = 0;

		source[num_source++] = gpu_shader_version();
		source[num_source++] = standard_extensions;
		source[num_source++] = standard_defines;

		if (defines) source[num_source++] = defines;
		source[num_source++] = geocode;

		glAttachShader(shader->program, shader->geometry);
		glShaderSource(shader->geometry, num_source, source, NULL);

		glCompileShader(shader->geometry);
		glGetShaderiv(shader->geometry, GL_COMPILE_STATUS, &status);

		if (!status) {
			glGetShaderInfoLog(shader->geometry, sizeof(log), &length, log);
			shader_print_errors("compile", log, source, num_source);

			GPU_shader_free(shader);
			return NULL;
		}
		
		if (!use_opensubdiv) {
			GPU_shader_geometry_stage_primitive_io(shader, input, output, number);
		}
	}

#ifdef WITH_OPENSUBDIV
	if (use_opensubdiv) {
		glBindAttribLocation(shader->program, 0, "position");
		glBindAttribLocation(shader->program, 1, "normal");
		GPU_shader_geometry_stage_primitive_io(shader,
		                                       GL_LINES_ADJACENCY_EXT,
		                                       GL_TRIANGLE_STRIP,
		                                       4);
	}
#endif

	glLinkProgram(shader->program);
	glGetProgramiv(shader->program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(shader->program, sizeof(log), &length, log);
		/* print attached shaders in pipeline order */
		if (vertexcode) shader_print_errors("linking", log, &vertexcode, 1);
		if (geocode) shader_print_errors("linking", log, &geocode, 1);
		if (libcode) shader_print_errors("linking", log, &libcode, 1);
		if (fragcode) shader_print_errors("linking", log, &fragcode, 1);

		GPU_shader_free(shader);
		return NULL;
	}

#ifdef WITH_OPENSUBDIV
	/* TODO(sergey): Find a better place for this. */
	if (use_opensubdiv && GLEW_VERSION_4_1) {
		glProgramUniform1i(shader->program,
		                   glGetUniformLocation(shader->program, "FVarDataOffsetBuffer"),
		                   30);  /* GL_TEXTURE30 */

		glProgramUniform1i(shader->program,
		                   glGetUniformLocation(shader->program, "FVarDataBuffer"),
		                   31);  /* GL_TEXTURE31 */
	}
#endif

	return shader;
}

void GPU_shader_bind(GPUShader *shader)
{
	GPU_ASSERT_NO_GL_ERRORS("Pre Shader Bind");
	glUseProgram(shader->program);
	GPU_ASSERT_NO_GL_ERRORS("Post Shader Bind");
}

void GPU_shader_unbind(void)
{
	GPU_ASSERT_NO_GL_ERRORS("Pre Shader Unbind");
	glUseProgram(0);
	GPU_ASSERT_NO_GL_ERRORS("Post Shader Unbind");
}

void GPU_shader_free(GPUShader *shader)
{
	if (shader->vertex)
		glDeleteShader(shader->vertex);
	if (shader->geometry)
		glDeleteShader(shader->geometry);
	if (shader->fragment)
		glDeleteShader(shader->fragment);
	if (shader->program)
		glDeleteProgram(shader->program);

	if (shader->uniform_interface)
		MEM_freeN(shader->uniform_interface);

	MEM_freeN(shader);
}

int GPU_shader_get_uniform(GPUShader *shader, const char *name)
{
	return glGetUniformLocation(shader->program, name);
}

void *GPU_shader_get_interface(GPUShader *shader)
{
	return shader->uniform_interface;
}

void GPU_shader_set_interface(GPUShader *shader, void *interface)
{
	shader->uniform_interface = interface;
}

void GPU_shader_uniform_vector(GPUShader *UNUSED(shader), int location, int length, int arraysize, const float *value)
{
	if (location == -1 || value == NULL)
		return;

	GPU_ASSERT_NO_GL_ERRORS("Pre Uniform Vector");

	if (length == 1) glUniform1fv(location, arraysize, value);
	else if (length == 2) glUniform2fv(location, arraysize, value);
	else if (length == 3) glUniform3fv(location, arraysize, value);
	else if (length == 4) glUniform4fv(location, arraysize, value);
	else if (length == 9) glUniformMatrix3fv(location, arraysize, 0, value);
	else if (length == 16) glUniformMatrix4fv(location, arraysize, 0, value);

	GPU_ASSERT_NO_GL_ERRORS("Post Uniform Vector");
}

void GPU_shader_uniform_vector_int(GPUShader *UNUSED(shader), int location, int length, int arraysize, const int *value)
{
	if (location == -1)
		return;

	GPU_ASSERT_NO_GL_ERRORS("Pre Uniform Vector");

	if (length == 1) glUniform1iv(location, arraysize, value);
	else if (length == 2) glUniform2iv(location, arraysize, value);
	else if (length == 3) glUniform3iv(location, arraysize, value);
	else if (length == 4) glUniform4iv(location, arraysize, value);

	GPU_ASSERT_NO_GL_ERRORS("Post Uniform Vector");
}

void GPU_shader_uniform_int(GPUShader *UNUSED(shader), int location, int value)
{
	if (location == -1)
		return;

	GPU_CHECK_ERRORS_AROUND(glUniform1i(location, value));
}

void GPU_shader_geometry_stage_primitive_io(GPUShader *shader, int input, int output, int number)
{
	if (GPU_geometry_shader_support_via_extension()) {
		/* geometry shaders must provide this info themselves for #version 150 and up */
		glProgramParameteriEXT(shader->program, GL_GEOMETRY_INPUT_TYPE_EXT, input);
		glProgramParameteriEXT(shader->program, GL_GEOMETRY_OUTPUT_TYPE_EXT, output);
		glProgramParameteriEXT(shader->program, GL_GEOMETRY_VERTICES_OUT_EXT, number);
	}
}

void GPU_shader_uniform_texture(GPUShader *UNUSED(shader), int location, GPUTexture *tex)
{
	GLenum arbnumber;
	int number = GPU_texture_bound_number(tex);
	int bindcode = GPU_texture_opengl_bindcode(tex);
	int target = GPU_texture_target(tex);

	if (number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}
		
	if (number == -1)
		return;

	if (location == -1)
		return;

	GPU_ASSERT_NO_GL_ERRORS("Pre Uniform Texture");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0 + number);

	if (number != 0) glActiveTexture(arbnumber);
	if (bindcode != 0)
		glBindTexture(target, bindcode);
	else
		GPU_invalid_tex_bind(target);
	glUniform1i(location, number);
	glEnable(target);
	if (number != 0) glActiveTexture(GL_TEXTURE0);

	GPU_ASSERT_NO_GL_ERRORS("Post Uniform Texture");
}

int GPU_shader_get_attribute(GPUShader *shader, const char *name)
{
	int index;
	
	GPU_CHECK_ERRORS_AROUND(index = glGetAttribLocation(shader->program, name));

	return index;
}

GPUShader *GPU_shader_get_builtin_shader(GPUBuiltinShader shader)
{
	GPUShader *retval = NULL;

	switch (shader) {
		case GPU_SHADER_VSM_STORE:
			if (!GG.shaders.vsm_store)
				GG.shaders.vsm_store = GPU_shader_create(
				        datatoc_gpu_shader_vsm_store_vert_glsl, datatoc_gpu_shader_vsm_store_frag_glsl,
				        NULL, NULL, NULL, 0, 0, 0);
			retval = GG.shaders.vsm_store;
			break;
		case GPU_SHADER_SEP_GAUSSIAN_BLUR:
			if (!GG.shaders.sep_gaussian_blur)
				GG.shaders.sep_gaussian_blur = GPU_shader_create(
				        datatoc_gpu_shader_sep_gaussian_blur_vert_glsl,
				        datatoc_gpu_shader_sep_gaussian_blur_frag_glsl,
				        NULL, NULL, NULL, 0, 0, 0);
			retval = GG.shaders.sep_gaussian_blur;
			break;
		case GPU_SHADER_SMOKE:
			if (!GG.shaders.smoke)
				GG.shaders.smoke = GPU_shader_create(
				        datatoc_gpu_shader_smoke_vert_glsl, datatoc_gpu_shader_smoke_frag_glsl,
				        NULL, NULL, NULL, 0, 0, 0);
			retval = GG.shaders.smoke;
			break;
		case GPU_SHADER_SMOKE_FIRE:
			if (!GG.shaders.smoke_fire)
				GG.shaders.smoke_fire = GPU_shader_create(
				        datatoc_gpu_shader_smoke_vert_glsl, datatoc_gpu_shader_fire_frag_glsl,
				        NULL, NULL, NULL, 0, 0, 0);
			retval = GG.shaders.smoke_fire;
			break;
		case GPU_SHADER_SMOKE_COBA:
			if (!GG.shaders.smoke_coba)
				GG.shaders.smoke_coba = GPU_shader_create(
				        datatoc_gpu_shader_smoke_vert_glsl, datatoc_gpu_shader_smoke_frag_glsl,
				        NULL, NULL, "#define USE_COBA;\n", 0, 0, 0);
			retval = GG.shaders.smoke_coba;
			break;
	}

	if (retval == NULL)
		printf("Unable to create a GPUShader for builtin shader: %u\n", shader);

	return retval;
}

#define MAX_DEFINES 100

GPUShader *GPU_shader_get_builtin_fx_shader(int effect, bool persp)
{
	int offset;
	char defines[MAX_DEFINES] = "";
	/* avoid shaders out of range */
	if (effect >= MAX_FX_SHADERS)
		return NULL;

	offset = 2 * effect;

	if (persp) {
		offset += 1;
		strcat(defines, "#define PERSP_MATRIX\n");
	}

	if (!GG.shaders.fx_shaders[offset]) {
		GPUShader *shader = NULL;

		switch (effect) {
			case GPU_SHADER_FX_SSAO:
				shader = GPU_shader_create(datatoc_gpu_shader_fx_vert_glsl, datatoc_gpu_shader_fx_ssao_frag_glsl, NULL, datatoc_gpu_shader_fx_lib_glsl, defines, 0, 0, 0);
				break;

			case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_ONE:
				strcat(defines, "#define FIRST_PASS\n");
				shader = GPU_shader_create(datatoc_gpu_shader_fx_dof_vert_glsl, datatoc_gpu_shader_fx_dof_frag_glsl, NULL, datatoc_gpu_shader_fx_lib_glsl, defines, 0, 0, 0);
				break;

			case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_TWO:
				strcat(defines, "#define SECOND_PASS\n");
				shader = GPU_shader_create(datatoc_gpu_shader_fx_dof_vert_glsl, datatoc_gpu_shader_fx_dof_frag_glsl, NULL, datatoc_gpu_shader_fx_lib_glsl, defines, 0, 0, 0);
				break;

			case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_THREE:
				strcat(defines, "#define THIRD_PASS\n");
				shader = GPU_shader_create(datatoc_gpu_shader_fx_dof_vert_glsl, datatoc_gpu_shader_fx_dof_frag_glsl, NULL, datatoc_gpu_shader_fx_lib_glsl, defines, 0, 0, 0);
				break;

			case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FOUR:
				strcat(defines, "#define FOURTH_PASS\n");
				shader = GPU_shader_create(datatoc_gpu_shader_fx_dof_vert_glsl, datatoc_gpu_shader_fx_dof_frag_glsl, NULL, datatoc_gpu_shader_fx_lib_glsl, defines, 0, 0, 0);
				break;

			case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FIVE:
				strcat(defines, "#define FIFTH_PASS\n");
				shader = GPU_shader_create(datatoc_gpu_shader_fx_dof_vert_glsl, datatoc_gpu_shader_fx_dof_frag_glsl, NULL, datatoc_gpu_shader_fx_lib_glsl, defines, 0, 0, 0);
				break;

			case GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_ONE:
				strcat(defines, "#define FIRST_PASS\n");
				shader = GPU_shader_create(datatoc_gpu_shader_fx_dof_hq_vert_glsl, datatoc_gpu_shader_fx_dof_hq_frag_glsl, NULL, datatoc_gpu_shader_fx_lib_glsl, defines, 0, 0, 0);
				break;

			case GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_TWO:
				strcat(defines, "#define SECOND_PASS\n");
				shader = GPU_shader_create(datatoc_gpu_shader_fx_dof_hq_vert_glsl, datatoc_gpu_shader_fx_dof_hq_frag_glsl, datatoc_gpu_shader_fx_dof_hq_geo_glsl, datatoc_gpu_shader_fx_lib_glsl,
				                           defines, GL_POINTS, GL_TRIANGLE_STRIP, 4);
				break;

			case GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_THREE:
				strcat(defines, "#define THIRD_PASS\n");
				shader = GPU_shader_create(datatoc_gpu_shader_fx_dof_hq_vert_glsl, datatoc_gpu_shader_fx_dof_hq_frag_glsl, NULL, datatoc_gpu_shader_fx_lib_glsl, defines, 0, 0, 0);
				break;

			case GPU_SHADER_FX_DEPTH_RESOLVE:
				shader = GPU_shader_create(datatoc_gpu_shader_fx_vert_glsl, datatoc_gpu_shader_fx_depth_resolve_glsl, NULL, NULL, defines, 0, 0, 0);
				break;
		}

		GG.shaders.fx_shaders[offset] = shader;
		GPU_fx_shader_init_interface(shader, effect);
	}

	return GG.shaders.fx_shaders[offset];
}


void GPU_shader_free_builtin_shaders(void)
{
	int i;

	if (GG.shaders.vsm_store) {
		GPU_shader_free(GG.shaders.vsm_store);
		GG.shaders.vsm_store = NULL;
	}

	if (GG.shaders.sep_gaussian_blur) {
		GPU_shader_free(GG.shaders.sep_gaussian_blur);
		GG.shaders.sep_gaussian_blur = NULL;
	}

	if (GG.shaders.smoke) {
		GPU_shader_free(GG.shaders.smoke);
		GG.shaders.smoke = NULL;
	}

	if (GG.shaders.smoke_fire) {
		GPU_shader_free(GG.shaders.smoke_fire);
		GG.shaders.smoke_fire = NULL;
	}

	if (GG.shaders.smoke_coba) {
		GPU_shader_free(GG.shaders.smoke_coba);
		GG.shaders.smoke_coba = NULL;
	}

	for (i = 0; i < 2 * MAX_FX_SHADERS; ++i) {
		if (GG.shaders.fx_shaders[i]) {
			GPU_shader_free(GG.shaders.fx_shaders[i]);
			GG.shaders.fx_shaders[i] = NULL;
		}
	}
}


