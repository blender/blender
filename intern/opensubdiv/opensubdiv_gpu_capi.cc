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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "opensubdiv_capi.h"

#ifdef _MSC_VER
#  include "iso646.h"
#endif

#include <cstdio>
#include <cmath>
#include <GL/glew.h>

#include <opensubdiv/osd/glMesh.h>

#ifdef OPENSUBDIV_HAS_CUDA
#  include <opensubdiv/osd/cudaGLVertexBuffer.h>
#endif  /* OPENSUBDIV_HAS_CUDA */

#include <opensubdiv/osd/cpuGLVertexBuffer.h>
#include <opensubdiv/osd/cpuEvaluator.h>

#include "MEM_guardedalloc.h"

#include "opensubdiv_capi.h"
#include "opensubdiv_topology_refiner.h"

using OpenSubdiv::Osd::GLMeshInterface;

extern "C" char datatoc_gpu_shader_opensubdiv_vertex_glsl[];
extern "C" char datatoc_gpu_shader_opensubdiv_geometry_glsl[];
extern "C" char datatoc_gpu_shader_opensubdiv_fragment_glsl[];

/* TODO(sergey): This is bit of bad level calls :S */
extern "C" {
void copy_m3_m3(float m1[3][3], float m2[3][3]);
void copy_m3_m4(float m1[3][3], float m2[4][4]);
void adjoint_m3_m3(float m1[3][3], float m[3][3]);
float determinant_m3_array(float m[3][3]);
bool invert_m3_m3(float m1[3][3], float m2[3][3]);
bool invert_m3(float m[3][3]);
void transpose_m3(float mat[3][3]);
}

#define MAX_LIGHTS 8
#define SUPPORT_COLOR_MATERIAL

typedef struct Light {
	float position[4];
	float ambient[4];
	float diffuse[4];
	float specular[4];
	float spot_direction[4];
#ifdef SUPPORT_COLOR_MATERIAL
	float constant_attenuation;
	float linear_attenuation;
	float quadratic_attenuation;
	float spot_cutoff;
	float spot_exponent;
	float spot_cos_cutoff;
	float pad, pad2;
#endif
} Light;

typedef struct Lighting {
	Light lights[MAX_LIGHTS];
	int num_enabled;
} Lighting;

typedef struct Transform {
	float projection_matrix[16];
	float model_view_matrix[16];
	float normal_matrix[9];
} Transform;

static bool g_use_osd_glsl = false;
static int g_active_uv_index = 0;

static GLuint g_flat_fill_solid_program = 0;
static GLuint g_flat_fill_texture2d_program = 0;
static GLuint g_smooth_fill_solid_program = 0;
static GLuint g_smooth_fill_texture2d_program = 0;

static GLuint g_flat_fill_solid_shadeless_program = 0;
static GLuint g_flat_fill_texture2d_shadeless_program = 0;
static GLuint g_smooth_fill_solid_shadeless_program = 0;
static GLuint g_smooth_fill_texture2d_shadeless_program = 0;

static GLuint g_wireframe_program = 0;

static GLuint g_lighting_ub = 0;
static Lighting g_lighting_data;
static Transform g_transform;

struct OpenSubdiv_GLMeshFVarData
{
	OpenSubdiv_GLMeshFVarData() :
		texture_buffer(0), offset_buffer(0) {
	}

	~OpenSubdiv_GLMeshFVarData()
	{
		Release();
	}

	void Release()
	{
		if (texture_buffer) {
			glDeleteTextures(1, &texture_buffer);
		}
		if (offset_buffer) {
			glDeleteTextures(1, &offset_buffer);
		}
		texture_buffer = 0;
		offset_buffer = 0;
		fvar_width = 0;
		channel_offsets.clear();
	}

	void Create(const OpenSubdiv::Far::TopologyRefiner *refiner,
	            const OpenSubdiv::Far::PatchTable *patch_table,
	            int fvar_width,
	            const float *fvar_src_data)
	{
		Release();

		this->fvar_width = fvar_width;

		/* Expand fvar data to per-patch array */
		const int max_level = refiner->GetMaxLevel();
		const int num_channels = patch_table->GetNumFVarChannels();
		std::vector<float> data;
		int fvar_data_offset = 0;
		channel_offsets.resize(num_channels);
		for (int channel = 0; channel < num_channels; ++channel) {
			OpenSubdiv::Far::ConstIndexArray indices =
			        patch_table->GetFVarValues(channel);

			channel_offsets[channel] = data.size();
			data.reserve(data.size() + indices.size() * fvar_width);

			for (int fvert = 0; fvert < (int)indices.size(); ++fvert) {
				int index = indices[fvert] * fvar_width;
				for (int i = 0; i < fvar_width; ++i) {
					data.push_back(fvar_src_data[fvar_data_offset + index++]);
				}
			}
			if (refiner->IsUniform()) {
				const int num_values_max = refiner->GetLevel(max_level).GetNumFVarValues(channel);
				fvar_data_offset += num_values_max * fvar_width;
			} else {
				const int num_values_total = refiner->GetNumFVarValuesTotal(channel);
				fvar_data_offset += num_values_total * fvar_width;
			}
		}

		GLuint buffer;
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, data.size()*sizeof(float),
		             &data[0], GL_STATIC_DRAW);

		glGenTextures(1, &texture_buffer);
		glBindTexture(GL_TEXTURE_BUFFER, texture_buffer);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, buffer);

		glDeleteBuffers(1, &buffer);

		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, channel_offsets.size()*sizeof(int),
		             &channel_offsets[0], GL_STATIC_DRAW);

		glGenTextures(1, &offset_buffer);
		glBindTexture(GL_TEXTURE_BUFFER, offset_buffer);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, buffer);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	GLuint texture_buffer;
	GLuint offset_buffer;
	std::vector<int> channel_offsets;
	int fvar_width;
};

namespace {

GLuint compileShader(GLenum shaderType,
                     const char *version,
                     const char *define,
                     const char *source)
{
	const char *sources[] = {
		version,
		define,
#ifdef SUPPORT_COLOR_MATERIAL
		"#define SUPPORT_COLOR_MATERIAL\n",
#else
		"",
#endif
		source,
	};

	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 4, sources, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLchar emsg[1024];
		glGetShaderInfoLog(shader, sizeof(emsg), 0, emsg);
		fprintf(stderr, "Error compiling GLSL: %s\n", emsg);
		fprintf(stderr, "Version: %s\n", version);
		fprintf(stderr, "Defines: %s\n", define);
		fprintf(stderr, "Source: %s\n", source);
		return 0;
	}

	return shader;
}

GLuint linkProgram(const char *version, const char *define)
{
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER,
	                                    version,
	                                    define,
	                                    datatoc_gpu_shader_opensubdiv_vertex_glsl);
	if (vertexShader == 0) {
		return 0;
	}
	GLuint geometryShader = compileShader(GL_GEOMETRY_SHADER,
	                                      version,
	                                      define,
	                                      datatoc_gpu_shader_opensubdiv_geometry_glsl);
	if (geometryShader == 0) {
		return 0;
	}
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER,
	                                      version,
	                                      define,
	                                      datatoc_gpu_shader_opensubdiv_fragment_glsl );
	if (fragmentShader == 0) {
		return 0;
	}

	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, geometryShader);
	glAttachShader(program, fragmentShader);

	glBindAttribLocation(program, 0, "position");
	glBindAttribLocation(program, 1, "normal");


	if (!GLEW_VERSION_3_2) {
		/* provide input/output layout info */
		glProgramParameteriEXT(program,
		                       GL_GEOMETRY_INPUT_TYPE_EXT,
		                       GL_LINES_ADJACENCY_EXT);

		bool wireframe = strstr(define, "WIREFRAME") != NULL;

		glProgramParameteriEXT(program,
		                       GL_GEOMETRY_OUTPUT_TYPE_EXT,
		                       wireframe ? GL_LINE_STRIP : GL_TRIANGLE_STRIP);

		glProgramParameteriEXT(program,
		                       GL_GEOMETRY_VERTICES_OUT_EXT,
		                       8);
	}

	glLinkProgram(program);

	glDeleteShader(vertexShader);
	glDeleteShader(geometryShader);
	glDeleteShader(fragmentShader);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLchar emsg[1024];
		glGetProgramInfoLog(program, sizeof(emsg), 0, emsg);
		fprintf(stderr, "Error linking GLSL program : %s\n", emsg);
		fprintf(stderr, "Defines: %s\n", define);
		glDeleteProgram(program);
		return 0;
	}

	glUniformBlockBinding(program,
	                      glGetUniformBlockIndex(program, "Lighting"),
	                      0);

	glProgramUniform1i(program,
	                   glGetUniformLocation(program, "texture_buffer"),
	                   0);  /* GL_TEXTURE0 */

	glProgramUniform1i(program,
	                   glGetUniformLocation(program, "FVarDataOffsetBuffer"),
	                   30);  /* GL_TEXTURE30 */

	glProgramUniform1i(program,
	                   glGetUniformLocation(program, "FVarDataBuffer"),
	                   31);  /* GL_TEXTURE31 */

	return program;
}

void bindProgram(OpenSubdiv_GLMesh *gl_mesh, int program)
{
	glUseProgram(program);

	/* Matrices */
	glUniformMatrix4fv(glGetUniformLocation(program, "modelViewMatrix"),
	                   1, false,
	                   g_transform.model_view_matrix);
	glUniformMatrix4fv(glGetUniformLocation(program, "projectionMatrix"),
	                   1, false,
	                   g_transform.projection_matrix);
	glUniformMatrix3fv(glGetUniformLocation(program, "normalMatrix"),
	                   1, false,
	                   g_transform.normal_matrix);

	/* Lighting */
	glBindBuffer(GL_UNIFORM_BUFFER, g_lighting_ub);
	glBufferSubData(GL_UNIFORM_BUFFER,
	                0, sizeof(g_lighting_data), &g_lighting_data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_lighting_ub);

	/* Color */
	GLboolean use_lighting;
	glGetBooleanv(GL_LIGHTING, &use_lighting);

	if (use_lighting) {
		float color[4];
		glGetMaterialfv(GL_FRONT, GL_DIFFUSE, color);
		glUniform4fv(glGetUniformLocation(program, "diffuse"), 1, color);

		glGetMaterialfv(GL_FRONT, GL_SPECULAR, color);
		glUniform4fv(glGetUniformLocation(program, "specular"), 1, color);

		glGetMaterialfv(GL_FRONT, GL_SHININESS, color);
		glUniform1f(glGetUniformLocation(program, "shininess"), color[0]);
	}
	else {
		float color[4];
		glGetFloatv(GL_CURRENT_COLOR, color);
		glUniform4fv(glGetUniformLocation(program, "diffuse"), 1, color);
	}

	/* Face-vertex data */
	if (gl_mesh->fvar_data != NULL) {
		if (gl_mesh->fvar_data->texture_buffer) {
			glActiveTexture(GL_TEXTURE31);
			glBindTexture(GL_TEXTURE_BUFFER, gl_mesh->fvar_data->texture_buffer);
			glActiveTexture(GL_TEXTURE0);
		}

		if (gl_mesh->fvar_data->offset_buffer) {
			glActiveTexture(GL_TEXTURE30);
			glBindTexture(GL_TEXTURE_BUFFER, gl_mesh->fvar_data->offset_buffer);
			glActiveTexture(GL_TEXTURE0);
		}

		glUniform1i(glGetUniformLocation(program, "osd_fvar_count"),
		            gl_mesh->fvar_data->fvar_width);
		if (gl_mesh->fvar_data->channel_offsets.size() > 0 &&
		    g_active_uv_index >= 0)
		{
			glUniform1i(glGetUniformLocation(program, "osd_active_uv_offset"),
			            gl_mesh->fvar_data->channel_offsets[g_active_uv_index]);
		} else {
			glUniform1i(glGetUniformLocation(program, "osd_active_uv_offset"), 0);
		}
	} else {
		glUniform1i(glGetUniformLocation(program, "osd_fvar_count"), 0);
		glUniform1i(glGetUniformLocation(program, "osd_active_uv_offset"), 0);
	}
}

}  /* namespace */

bool openSubdiv_osdGLDisplayInit(void)
{
	static bool need_init = true;
	static bool init_success = false;
	if (need_init) {

		if (!openSubdiv_supportGPUDisplay()) {
			return false;
		}

		const char *version = "";
		if (GLEW_VERSION_3_2) {
			version = "#version 150 compatibility\n";
		}
		else if (GLEW_VERSION_3_1) {
			version = "#version 140\n"
			          "#extension GL_ARB_compatibility: enable\n";
		}
		else {
			version = "#version 130\n";
			/* minimum supported for OpenSubdiv */
		}

		g_flat_fill_solid_program = linkProgram(
		        version,
		        "#define USE_COLOR_MATERIAL\n"
		        "#define USE_LIGHTING\n"
		        "#define FLAT_SHADING\n");
		g_flat_fill_texture2d_program = linkProgram(
		        version,
		        "#define USE_COLOR_MATERIAL\n"
		        "#define USE_LIGHTING\n"
		        "#define USE_TEXTURE_2D\n"
		        "#define FLAT_SHADING\n");
		g_smooth_fill_solid_program = linkProgram(
		        version,
		        "#define USE_COLOR_MATERIAL\n"
		        "#define USE_LIGHTING\n"
		        "#define SMOOTH_SHADING\n");
		g_smooth_fill_texture2d_program = linkProgram(
		        version,
		        "#define USE_COLOR_MATERIAL\n"
		        "#define USE_LIGHTING\n"
		        "#define USE_TEXTURE_2D\n"
		        "#define SMOOTH_SHADING\n");

		g_flat_fill_solid_shadeless_program = linkProgram(
		        version,
		        "#define USE_COLOR_MATERIAL\n"
		        "#define FLAT_SHADING\n");
		g_flat_fill_texture2d_shadeless_program = linkProgram(
		        version,
		        "#define USE_COLOR_MATERIAL\n"
		        "#define USE_TEXTURE_2D\n"
		        "#define FLAT_SHADING\n");
		g_smooth_fill_solid_shadeless_program = linkProgram(
		        version,
		        "#define USE_COLOR_MATERIAL\n"
		        "#define SMOOTH_SHADING\n");
		g_smooth_fill_texture2d_shadeless_program = linkProgram(
		        version,
		        "#define USE_COLOR_MATERIAL\n"
		        "#define USE_TEXTURE_2D\n"
		        "#define SMOOTH_SHADING\n");

		g_wireframe_program = linkProgram(
		        version,
		        "#define WIREFRAME\n");

		glGenBuffers(1, &g_lighting_ub);
		glBindBuffer(GL_UNIFORM_BUFFER, g_lighting_ub);
		glBufferData(GL_UNIFORM_BUFFER,
		             sizeof(g_lighting_data), NULL, GL_STATIC_DRAW);

		need_init = false;
		init_success = g_flat_fill_solid_program != 0 &&
		               g_flat_fill_texture2d_program != 0 &&
		               g_smooth_fill_solid_program != 0 &&
		               g_smooth_fill_texture2d_program != 0 &&
		               g_wireframe_program;
	}
	return init_success;
}

void openSubdiv_osdGLDisplayDeinit(void)
{
	if (g_lighting_ub != 0) {
		glDeleteBuffers(1, &g_lighting_ub);
	}
#define SAFE_DELETE_PROGRAM(program) \
	do { \
		if (program) { \
			glDeleteProgram(program); \
		} \
	} while (false)

	SAFE_DELETE_PROGRAM(g_flat_fill_solid_program);
	SAFE_DELETE_PROGRAM(g_flat_fill_texture2d_program);
	SAFE_DELETE_PROGRAM(g_smooth_fill_solid_program);
	SAFE_DELETE_PROGRAM(g_smooth_fill_texture2d_program);
	SAFE_DELETE_PROGRAM(g_flat_fill_solid_shadeless_program);
	SAFE_DELETE_PROGRAM(g_flat_fill_texture2d_shadeless_program);
	SAFE_DELETE_PROGRAM(g_smooth_fill_solid_shadeless_program);
	SAFE_DELETE_PROGRAM(g_smooth_fill_texture2d_shadeless_program);
	SAFE_DELETE_PROGRAM(g_wireframe_program);

#undef SAFE_DELETE_PROGRAM
}

void openSubdiv_osdGLMeshDisplayPrepare(int use_osd_glsl,
                                        int active_uv_index)
{
	g_active_uv_index = active_uv_index;
	g_use_osd_glsl = (use_osd_glsl != 0);

	/* Update transformation matrices. */
	glGetFloatv(GL_PROJECTION_MATRIX, g_transform.projection_matrix);
	glGetFloatv(GL_MODELVIEW_MATRIX, g_transform.model_view_matrix);

	copy_m3_m4((float (*)[3])g_transform.normal_matrix,
	           (float (*)[4])g_transform.model_view_matrix);
	invert_m3((float (*)[3])g_transform.normal_matrix);
	transpose_m3((float (*)[3])g_transform.normal_matrix);

	/* Update OpenGL lights positions, colors etc. */
	g_lighting_data.num_enabled = 0;
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		GLboolean enabled;
		glGetBooleanv(GL_LIGHT0 + i, &enabled);
		if (enabled) {
			g_lighting_data.num_enabled++;
		}

		glGetLightfv(GL_LIGHT0 + i,
		             GL_POSITION,
		             g_lighting_data.lights[i].position);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_AMBIENT,
		             g_lighting_data.lights[i].ambient);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_DIFFUSE,
		             g_lighting_data.lights[i].diffuse);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_SPECULAR,
		             g_lighting_data.lights[i].specular);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_SPOT_DIRECTION,
		             g_lighting_data.lights[i].spot_direction);
#ifdef SUPPORT_COLOR_MATERIAL
		glGetLightfv(GL_LIGHT0 + i,
		             GL_CONSTANT_ATTENUATION,
		             &g_lighting_data.lights[i].constant_attenuation);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_LINEAR_ATTENUATION,
		             &g_lighting_data.lights[i].linear_attenuation);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_QUADRATIC_ATTENUATION,
		             &g_lighting_data.lights[i].quadratic_attenuation);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_SPOT_CUTOFF,
		             &g_lighting_data.lights[i].spot_cutoff);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_SPOT_EXPONENT,
		             &g_lighting_data.lights[i].spot_exponent);
		g_lighting_data.lights[i].spot_cos_cutoff =
			cos(g_lighting_data.lights[i].spot_cutoff);
#endif
	}
}

static GLuint prepare_patchDraw(OpenSubdiv_GLMesh *gl_mesh,
                                bool fill_quads)
{
	GLint program = 0;
	if (!g_use_osd_glsl) {
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		if (program) {
			GLint model;
			glGetIntegerv(GL_SHADE_MODEL, &model);

			GLint location = glGetUniformLocation(program, "osd_flat_shading");
			if (location != -1) {
				glUniform1i(location, model == GL_FLAT);
			}

			/* Face-vertex data */
			if (gl_mesh->fvar_data != NULL) {
				if (gl_mesh->fvar_data->texture_buffer) {
					glActiveTexture(GL_TEXTURE31);
					glBindTexture(GL_TEXTURE_BUFFER,
					              gl_mesh->fvar_data->texture_buffer);
					glActiveTexture(GL_TEXTURE0);
				}

				if (gl_mesh->fvar_data->offset_buffer) {
					glActiveTexture(GL_TEXTURE30);
					glBindTexture(GL_TEXTURE_BUFFER,
					              gl_mesh->fvar_data->offset_buffer);
					glActiveTexture(GL_TEXTURE0);
				}

				GLint location = glGetUniformLocation(program, "osd_fvar_count");
				if (location != -1) {
					glUniform1i(location, gl_mesh->fvar_data->fvar_width);
				}

				location = glGetUniformLocation(program, "osd_active_uv_offset");
				if (location != -1) {
					if (gl_mesh->fvar_data->channel_offsets.size() > 0 &&
					    g_active_uv_index >= 0)
					{
						glUniform1i(location,
						            gl_mesh->fvar_data->channel_offsets[g_active_uv_index]);
					} else {
						glUniform1i(location, 0);
					}
				}
			} else {
				glUniform1i(glGetUniformLocation(program, "osd_fvar_count"), 0);
				glUniform1i(glGetUniformLocation(program, "osd_active_uv_offset"), 0);
			}
		}
		return program;
	}

	if (fill_quads) {
		int model;
		GLboolean use_texture_2d, use_lighting;
		glGetIntegerv(GL_SHADE_MODEL, &model);
		glGetBooleanv(GL_TEXTURE_2D, &use_texture_2d);
		glGetBooleanv(GL_LIGHTING, &use_lighting);
		if (model == GL_FLAT) {
			if (use_texture_2d) {
				program = use_lighting
				                  ? g_flat_fill_texture2d_program
				                  : g_flat_fill_texture2d_shadeless_program;
			}
			else {
				program = use_lighting
				                  ? g_flat_fill_solid_program
				                  : g_flat_fill_solid_shadeless_program;
			}
		}
		else {
			if (use_texture_2d) {
				program = use_lighting
				                  ? g_smooth_fill_texture2d_program
				                  : g_smooth_fill_texture2d_shadeless_program;
			}
			else {
				program = use_lighting
				                  ? g_smooth_fill_solid_program
				                  : g_smooth_fill_solid_shadeless_program;
			}
		}
	}
	else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		program = g_wireframe_program;
	}

	bindProgram(gl_mesh, program);

	return program;
}

static void perform_drawElements(GLuint program,
                                 int patch_index,
                                 int num_elements,
                                 int start_element)
{
	if (program) {
		glUniform1i(glGetUniformLocation(program, "PrimitiveIdBase"),
		            patch_index);
	}
	glDrawElements(GL_LINES_ADJACENCY,
	               num_elements,
	               GL_UNSIGNED_INT,
	               (void *)(start_element * sizeof(unsigned int)));
}

static void finish_patchDraw(bool fill_quads)
{
	/* TODO(sergey): Some of the stuff could be done once after the whole
	 * mesh is displayed.
	 */

	/* Restore state. */
	if (!fill_quads) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	glBindVertexArray(0);

	if (g_use_osd_glsl) {
		/* TODO(sergey): Store previously used program and roll back to it? */
		glUseProgram(0);
	}
}

static void draw_partition_patches_range(GLMeshInterface *mesh,
                                         GLuint program,
                                         int start_patch,
                                         int num_patches)
{
	int traversed_patches = 0, num_remained_patches = num_patches;
	const OpenSubdiv::Osd::PatchArrayVector& patches =
	        mesh->GetPatchTable()->GetPatchArrays();
	for (int i = 0; i < (int)patches.size(); ++i) {
		const OpenSubdiv::Osd::PatchArray& patch = patches[i];
		OpenSubdiv::Far::PatchDescriptor desc = patch.GetDescriptor();
		OpenSubdiv::Far::PatchDescriptor::Type patchType = desc.GetType();

		if (patchType == OpenSubdiv::Far::PatchDescriptor::QUADS) {
			const int num_block_patches = patch.GetNumPatches();
			if (start_patch >= traversed_patches &&
			    start_patch < traversed_patches + num_block_patches)
			{
				const int num_control_verts = desc.GetNumControlVertices();
				const int start_draw_patch = start_patch - traversed_patches;
				const int num_draw_patches = std::min(num_remained_patches,
				                                      num_block_patches - start_draw_patch);
				perform_drawElements(program,
				                     i + start_draw_patch,
				                     num_draw_patches * num_control_verts,
				                     patch.GetIndexBase() + start_draw_patch * num_control_verts);
				num_remained_patches -= num_draw_patches;
			}
			if (num_remained_patches == 0) {
				break;
			}
			traversed_patches += num_block_patches;
		}
    }
}

static void draw_all_patches(GLMeshInterface *mesh,
                             GLuint program)
{
	const OpenSubdiv::Osd::PatchArrayVector& patches =
	        mesh->GetPatchTable()->GetPatchArrays();
	for (int i = 0; i < (int)patches.size(); ++i) {
		const OpenSubdiv::Osd::PatchArray& patch = patches[i];
		OpenSubdiv::Far::PatchDescriptor desc = patch.GetDescriptor();
		OpenSubdiv::Far::PatchDescriptor::Type patchType = desc.GetType();

		if (patchType == OpenSubdiv::Far::PatchDescriptor::QUADS) {
			perform_drawElements(program,
			                     i,
			                     patch.GetNumPatches() * desc.GetNumControlVertices(),
			                     patch.GetIndexBase());
		}
    }
}

void openSubdiv_osdGLMeshDisplay(OpenSubdiv_GLMesh *gl_mesh,
                                 int fill_quads,
                                 int start_patch,
                                 int num_patches)
{
	GLMeshInterface *mesh =
		(GLMeshInterface *)(gl_mesh->descriptor);

	/* Make sure all global invariants are initialized. */
	if (!openSubdiv_osdGLDisplayInit()) {
		return;
	}

	/* Setup GLSL/OpenGL to draw patches in current context. */
	GLuint program = prepare_patchDraw(gl_mesh, fill_quads != 0);

	if (start_patch != -1) {
		draw_partition_patches_range(mesh,
		                             program,
		                             start_patch,
		                             num_patches);
	}
	else {
		draw_all_patches(mesh, program);
	}

	/* Finish patch drawing by restoring all changes to the OpenGL context. */
	finish_patchDraw(fill_quads != 0);
}

void openSubdiv_osdGLAllocFVar(OpenSubdiv_TopologyRefinerDescr *topology_refiner,
                               OpenSubdiv_GLMesh *gl_mesh,
                               const float *fvar_data)
{
	GLMeshInterface *mesh =
		(GLMeshInterface *)(gl_mesh->descriptor);
	gl_mesh->fvar_data = OBJECT_GUARDED_NEW(OpenSubdiv_GLMeshFVarData);
	gl_mesh->fvar_data->Create(topology_refiner->osd_refiner,
	                           mesh->GetFarPatchTable(),
	                           2,
	                           fvar_data);
}

void openSubdiv_osdGLDestroyFVar(OpenSubdiv_GLMesh *gl_mesh)
{
	if (gl_mesh->fvar_data != NULL) {
		OBJECT_GUARDED_DELETE(gl_mesh->fvar_data, OpenSubdiv_GLMeshFVarData);
	}
}
