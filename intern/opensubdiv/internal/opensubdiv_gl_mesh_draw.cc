// Copyright 2013 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#include "internal/opensubdiv_gl_mesh_draw.h"

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdio>

#include <opensubdiv/osd/glMesh.h>

#ifdef OPENSUBDIV_HAS_CUDA
#  include <opensubdiv/osd/cudaGLVertexBuffer.h>
#endif  // OPENSUBDIV_HAS_CUDA

#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/cpuGLVertexBuffer.h>

#include "internal/opensubdiv_gl_mesh_fvar.h"
#include "internal/opensubdiv_gl_mesh_internal.h"
#include "opensubdiv_capi.h"
#include "opensubdiv_gl_mesh_capi.h"

using OpenSubdiv::Osd::GLMeshInterface;

extern "C" char datatoc_gpu_shader_opensubdiv_vertex_glsl[];
extern "C" char datatoc_gpu_shader_opensubdiv_geometry_glsl[];
extern "C" char datatoc_gpu_shader_opensubdiv_fragment_glsl[];

// TODO(sergey): Those are a bit of bad level calls :S
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

namespace {

GLuint compileShader(GLenum shaderType,
                     const char* version,
                     const char* define,
                     const char* source) {
  const char* sources[] = {
      version, define,
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

GLuint linkProgram(const char* version, const char* define) {
  GLuint vertexShader =
      compileShader(GL_VERTEX_SHADER, version, define,
                    datatoc_gpu_shader_opensubdiv_vertex_glsl);
  if (vertexShader == 0) {
    return 0;
  }
  GLuint geometryShader =
      compileShader(GL_GEOMETRY_SHADER, version, define,
                    datatoc_gpu_shader_opensubdiv_geometry_glsl);
  if (geometryShader == 0) {
    return 0;
  }
  GLuint fragmentShader =
      compileShader(GL_FRAGMENT_SHADER, version, define,
                    datatoc_gpu_shader_opensubdiv_fragment_glsl);
  if (fragmentShader == 0) {
    return 0;
  }

  GLuint program = glCreateProgram();

  glAttachShader(program, vertexShader);
  glAttachShader(program, geometryShader);
  glAttachShader(program, fragmentShader);

  glBindAttribLocation(program, 0, "position");
  glBindAttribLocation(program, 1, "normal");

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

  glUniformBlockBinding(program, glGetUniformBlockIndex(program, "Lighting"),
                        0);

  if (GLEW_VERSION_4_1) {
    glProgramUniform1i(
        program, glGetUniformLocation(program, "texture_buffer"), 0);
    glProgramUniform1i(
      program, glGetUniformLocation(program, "FVarDataOffsetBuffer"), 30);
    glProgramUniform1i(
      program, glGetUniformLocation(program, "FVarDataBuffer"), 31);
  } else {
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "texture_buffer"), 0);
    glUniform1i(glGetUniformLocation(program, "FVarDataOffsetBuffer"), 30);
    glUniform1i(glGetUniformLocation(program, "FVarDataBuffer"), 31);
    glUseProgram(0);
  }

  return program;
}

void bindProgram(OpenSubdiv_GLMesh* gl_mesh, int program) {
  glUseProgram(program);
  // Matrices
  glUniformMatrix4fv(glGetUniformLocation(program, "modelViewMatrix"),
                     1,
                     false,
                     g_transform.model_view_matrix);
  glUniformMatrix4fv(glGetUniformLocation(program, "projectionMatrix"),
                     1,
                     false,
                     g_transform.projection_matrix);
  glUniformMatrix3fv(glGetUniformLocation(program, "normalMatrix"),
                     1,
                     false,
                     g_transform.normal_matrix);
  // Lighting.
  glBindBuffer(GL_UNIFORM_BUFFER, g_lighting_ub);
  glBufferSubData(
      GL_UNIFORM_BUFFER, 0, sizeof(g_lighting_data), &g_lighting_data);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_lighting_ub);
  // Color.
  {
    // TODO(sergey): Stop using glGetMaterial.
    float color[4];
    glGetMaterialfv(GL_FRONT, GL_DIFFUSE, color);
    glUniform4fv(glGetUniformLocation(program, "diffuse"), 1, color);
    glGetMaterialfv(GL_FRONT, GL_SPECULAR, color);
    glUniform4fv(glGetUniformLocation(program, "specular"), 1, color);
    glGetMaterialfv(GL_FRONT, GL_SHININESS, color);
    glUniform1f(glGetUniformLocation(program, "shininess"), color[0]);
  }
  // Face-vertex data.
  opensubdiv_capi::GLMeshFVarData* fvar_data = gl_mesh->internal->fvar_data;
  if (fvar_data != NULL) {
    if (fvar_data->texture_buffer) {
      glActiveTexture(GL_TEXTURE31);
      glBindTexture(GL_TEXTURE_BUFFER, fvar_data->texture_buffer);
      glActiveTexture(GL_TEXTURE0);
    }
    if (fvar_data->offset_buffer) {
      glActiveTexture(GL_TEXTURE30);
      glBindTexture(GL_TEXTURE_BUFFER, fvar_data->offset_buffer);
      glActiveTexture(GL_TEXTURE0);
    }
    glUniform1i(glGetUniformLocation(program, "osd_fvar_count"),
               fvar_data->fvar_width);
    if (fvar_data->channel_offsets.size() > 0 && g_active_uv_index >= 0) {
      glUniform1i(glGetUniformLocation(program, "osd_active_uv_offset"),
                  fvar_data->channel_offsets[g_active_uv_index]);
    } else {
      glUniform1i(glGetUniformLocation(program, "osd_active_uv_offset"), 0);
    }
  } else {
    glUniform1i(glGetUniformLocation(program, "osd_fvar_count"), 0);
    glUniform1i(glGetUniformLocation(program, "osd_active_uv_offset"), 0);
  }
}

}  // namespace

bool openSubdiv_initGLMeshDrawingResources(void) {
  static bool need_init = true;
  static bool init_success = false;
  if (!need_init) {
    return init_success;
  }
  // TODO(sergey): Update OSD drawing to OpenGL 3.3 core,
  // then remove following line.
  return false;
  const char* version = "";
  if (GLEW_VERSION_3_2) {
    version = "#version 150 compatibility\n";
  } else if (GLEW_VERSION_3_1) {
    version = "#version 140\n"
              "#extension GL_ARB_compatibility: enable\n";
  } else {
    version = "#version 130\n";
    // Minimum supported for OpenSubdiv.
    }
  g_flat_fill_solid_program = linkProgram(version,
                                          "#define USE_COLOR_MATERIAL\n"
                                          "#define USE_LIGHTING\n"
                                          "#define FLAT_SHADING\n");
  g_flat_fill_texture2d_program = linkProgram(version,
                                              "#define USE_COLOR_MATERIAL\n"
                                              "#define USE_LIGHTING\n"
                                              "#define USE_TEXTURE_2D\n"
                                              "#define FLAT_SHADING\n");
  g_smooth_fill_solid_program = linkProgram(version,
                                            "#define USE_COLOR_MATERIAL\n"
                                            "#define USE_LIGHTING\n"
                                            "#define SMOOTH_SHADING\n");
  g_smooth_fill_texture2d_program = linkProgram(version,
                                                "#define USE_COLOR_MATERIAL\n"
                                                "#define USE_LIGHTING\n"
                                                "#define USE_TEXTURE_2D\n"
                                                "#define SMOOTH_SHADING\n");

  g_flat_fill_solid_shadeless_program =
      linkProgram(version,
                  "#define USE_COLOR_MATERIAL\n"
                  "#define FLAT_SHADING\n");
  g_flat_fill_texture2d_shadeless_program =
      linkProgram(version,
                  "#define USE_COLOR_MATERIAL\n"
                  "#define USE_TEXTURE_2D\n"
                  "#define FLAT_SHADING\n");
  g_smooth_fill_solid_shadeless_program =
      linkProgram(version,
                  "#define USE_COLOR_MATERIAL\n"
                  "#define SMOOTH_SHADING\n");
  g_smooth_fill_texture2d_shadeless_program =
      linkProgram(version,
                  "#define USE_COLOR_MATERIAL\n"
                  "#define USE_TEXTURE_2D\n"
                  "#define SMOOTH_SHADING\n");
  g_wireframe_program = linkProgram(version, "#define WIREFRAME\n");

  glGenBuffers(1, &g_lighting_ub);
  glBindBuffer(GL_UNIFORM_BUFFER, g_lighting_ub);
  glBufferData(GL_UNIFORM_BUFFER,
               sizeof(g_lighting_data),
               NULL,
               GL_STATIC_DRAW);
  need_init = false;
  init_success = g_flat_fill_solid_program != 0 &&
                 g_flat_fill_texture2d_program != 0 &&
                 g_smooth_fill_solid_program != 0 &&
                 g_smooth_fill_texture2d_program != 0 && g_wireframe_program;
  return init_success;
}

void openSubdiv_deinitGLMeshDrawingResources(void) {
  if (g_lighting_ub != 0) {
    glDeleteBuffers(1, &g_lighting_ub);
  }
#define SAFE_DELETE_PROGRAM(program)  \
  do {                                \
    if (program) {                    \
      glDeleteProgram(program);       \
    }                                 \
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

namespace opensubdiv_capi {

namespace {

GLuint prepare_patchDraw(OpenSubdiv_GLMesh* gl_mesh, bool fill_quads) {
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
      // Face-vertex data.
      opensubdiv_capi::GLMeshFVarData* fvar_data = gl_mesh->internal->fvar_data;
      if (fvar_data != NULL) {
        if (fvar_data->texture_buffer) {
          glActiveTexture(GL_TEXTURE31);
          glBindTexture(GL_TEXTURE_BUFFER, fvar_data->texture_buffer);
          glActiveTexture(GL_TEXTURE0);
        }
        if (fvar_data->offset_buffer) {
          glActiveTexture(GL_TEXTURE30);
          glBindTexture(GL_TEXTURE_BUFFER, fvar_data->offset_buffer);
          glActiveTexture(GL_TEXTURE0);
        }
        GLint location = glGetUniformLocation(program, "osd_fvar_count");
        if (location != -1) {
          glUniform1i(location, fvar_data->fvar_width);
        }
        location = glGetUniformLocation(program, "osd_active_uv_offset");
        if (location != -1) {
          if (fvar_data->channel_offsets.size() > 0 && g_active_uv_index >= 0) {
            glUniform1i(location,
                        fvar_data->channel_offsets[g_active_uv_index]);
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
    GLboolean use_texture_2d;
    glGetIntegerv(GL_SHADE_MODEL, &model);
    glGetBooleanv(GL_TEXTURE_2D, &use_texture_2d);
    if (model == GL_FLAT) {
      if (use_texture_2d) {
        program = g_flat_fill_texture2d_program;
      } else {
        program = g_flat_fill_solid_program;
      }
    } else {
      if (use_texture_2d) {
        program = g_smooth_fill_texture2d_program;
      } else {
        program = g_smooth_fill_solid_program;
      }
    }

  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    program = g_wireframe_program;
  }
  bindProgram(gl_mesh, program);
  return program;
}

void perform_drawElements(GLuint program,
                          int patch_index,
                          int num_elements,
                          int start_element) {
  if (program) {
    glUniform1i(glGetUniformLocation(program, "PrimitiveIdBase"), patch_index);
  }
  glDrawElements(GL_LINES_ADJACENCY,
                 num_elements,
                 GL_UNSIGNED_INT,
                 reinterpret_cast<void*>(start_element * sizeof(unsigned int)));
}

void finishPatchDraw(bool fill_quads) {
  // TODO(sergey): Some of the stuff could be done once after the whole
  // mesh is displayed.
  /// Restore state.
  if (!fill_quads) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
  glBindVertexArray(0);
  if (g_use_osd_glsl) {
    // TODO(sergey): Store previously used program and roll back to it?
    glUseProgram(0);
  }
}

void drawPartitionPatchesRange(GLMeshInterface* mesh,
                               GLuint program,
                               int start_patch,
                               int num_patches) {
  int traversed_patches = 0, num_remained_patches = num_patches;
  const OpenSubdiv::Osd::PatchArrayVector &patches =
      mesh->GetPatchTable()->GetPatchArrays();
  for (int i = 0; i < patches.size(); ++i) {
    const OpenSubdiv::Osd::PatchArray &patch = patches[i];
    OpenSubdiv::Far::PatchDescriptor desc = patch.GetDescriptor();
    OpenSubdiv::Far::PatchDescriptor::Type patchType = desc.GetType();
    if (patchType == OpenSubdiv::Far::PatchDescriptor::QUADS) {
      const int num_block_patches = patch.GetNumPatches();
      if (start_patch >= traversed_patches &&
          start_patch < traversed_patches + num_block_patches) {
        const int num_control_verts = desc.GetNumControlVertices();
        const int start_draw_patch = start_patch - traversed_patches;
        const int num_draw_patches = std::min(
            num_remained_patches, num_block_patches - start_draw_patch);
        perform_drawElements(
            program, i + start_draw_patch, num_draw_patches * num_control_verts,
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

static void drawAllPatches(GLMeshInterface* mesh, GLuint program) {
  const OpenSubdiv::Osd::PatchArrayVector &patches =
      mesh->GetPatchTable()->GetPatchArrays();
  for (int i = 0; i < patches.size(); ++i) {
    const OpenSubdiv::Osd::PatchArray &patch = patches[i];
    OpenSubdiv::Far::PatchDescriptor desc = patch.GetDescriptor();
    OpenSubdiv::Far::PatchDescriptor::Type patchType = desc.GetType();

    if (patchType == OpenSubdiv::Far::PatchDescriptor::QUADS) {
      perform_drawElements(program, i,
                           patch.GetNumPatches() * desc.GetNumControlVertices(),
                           patch.GetIndexBase());
    }
  }
}

}  // namespace

void GLMeshDisplayPrepare(struct OpenSubdiv_GLMesh* /*gl_mesh*/,
                          const bool use_osd_glsl,
                          const int active_uv_index) {
  g_active_uv_index = active_uv_index;
  g_use_osd_glsl = (use_osd_glsl != 0);
  // Update transformation matrices.
  glGetFloatv(GL_PROJECTION_MATRIX, g_transform.projection_matrix);
  glGetFloatv(GL_MODELVIEW_MATRIX, g_transform.model_view_matrix);
  copy_m3_m4((float(*)[3])g_transform.normal_matrix,
             (float(*)[4])g_transform.model_view_matrix);
  invert_m3((float(*)[3])g_transform.normal_matrix);
  transpose_m3((float(*)[3])g_transform.normal_matrix);
  // Update OpenGL lights positions, colors etc.
  g_lighting_data.num_enabled = 0;
  for (int i = 0; i < MAX_LIGHTS; ++i) {
    GLboolean enabled;
    glGetBooleanv(GL_LIGHT0 + i, &enabled);
    if (enabled) {
      g_lighting_data.num_enabled++;
    }
    // TODO(sergey): Stop using glGetLight.
    glGetLightfv(GL_LIGHT0 + i, GL_POSITION,
                 g_lighting_data.lights[i].position);
    glGetLightfv(GL_LIGHT0 + i, GL_AMBIENT, g_lighting_data.lights[i].ambient);
    glGetLightfv(GL_LIGHT0 + i, GL_DIFFUSE, g_lighting_data.lights[i].diffuse);
    glGetLightfv(GL_LIGHT0 + i, GL_SPECULAR,
                 g_lighting_data.lights[i].specular);
    glGetLightfv(GL_LIGHT0 + i, GL_SPOT_DIRECTION,
                 g_lighting_data.lights[i].spot_direction);
#ifdef SUPPORT_COLOR_MATERIAL
    glGetLightfv(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION,
                 &g_lighting_data.lights[i].constant_attenuation);
    glGetLightfv(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION,
                 &g_lighting_data.lights[i].linear_attenuation);
    glGetLightfv(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION,
                 &g_lighting_data.lights[i].quadratic_attenuation);
    glGetLightfv(GL_LIGHT0 + i, GL_SPOT_CUTOFF,
                 &g_lighting_data.lights[i].spot_cutoff);
    glGetLightfv(GL_LIGHT0 + i, GL_SPOT_EXPONENT,
                 &g_lighting_data.lights[i].spot_exponent);
    g_lighting_data.lights[i].spot_cos_cutoff =
        cos(g_lighting_data.lights[i].spot_cutoff);
#endif
  }
}

void GLMeshDisplayDrawPatches(OpenSubdiv_GLMesh* gl_mesh,
                              const bool fill_quads,
                              const int start_patch,
                              const int num_patches) {
  GLMeshInterface* mesh = gl_mesh->internal->mesh_interface;
  // Make sure all global invariants are initialized.
  if (!openSubdiv_initGLMeshDrawingResources()) {
    return;
  }
  /// Setup GLSL/OpenGL to draw patches in current context.
  GLuint program = prepare_patchDraw(gl_mesh, fill_quads != 0);
  if (start_patch != -1) {
    drawPartitionPatchesRange(mesh, program, start_patch, num_patches);
  } else {
    drawAllPatches(mesh, program);
  }
  // Finish patch drawing by restoring all changes to the OpenGL context.
  finishPatchDraw(fill_quads != 0);
}

}  // namespace opensubdiv_capi
