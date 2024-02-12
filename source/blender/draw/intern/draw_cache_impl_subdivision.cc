/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_subdivision.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"
#include "BKE_subdiv_foreach.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_modifier.hh"

#include "BLI_linklist.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_time.h"
#include "BLI_virtual_array.hh"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_index_buffer.h"
#include "GPU_state.h"
#include "GPU_vertex_buffer.h"

#include "opensubdiv_capi.hh"
#include "opensubdiv_capi_type.hh"
#include "opensubdiv_converter_capi.hh"
#include "opensubdiv_evaluator_capi.hh"
#include "opensubdiv_topology_refiner_capi.hh"

#include "draw_cache_extract.hh"
#include "draw_cache_impl.hh"
#include "draw_cache_inline.hh"
#include "mesh_extractors/extract_mesh.hh"

extern "C" char datatoc_common_subdiv_custom_data_interp_comp_glsl[];
extern "C" char datatoc_common_subdiv_ibo_lines_comp_glsl[];
extern "C" char datatoc_common_subdiv_ibo_tris_comp_glsl[];
extern "C" char datatoc_common_subdiv_lib_glsl[];
extern "C" char datatoc_common_subdiv_normals_accumulate_comp_glsl[];
extern "C" char datatoc_common_subdiv_normals_finalize_comp_glsl[];
extern "C" char datatoc_common_subdiv_patch_evaluation_comp_glsl[];
extern "C" char datatoc_common_subdiv_vbo_edge_fac_comp_glsl[];
extern "C" char datatoc_common_subdiv_vbo_lnor_comp_glsl[];
extern "C" char datatoc_common_subdiv_vbo_sculpt_data_comp_glsl[];
extern "C" char datatoc_common_subdiv_vbo_edituv_strech_angle_comp_glsl[];
extern "C" char datatoc_common_subdiv_vbo_edituv_strech_area_comp_glsl[];

namespace blender::draw {

enum {
  SHADER_BUFFER_LINES,
  SHADER_BUFFER_LINES_LOOSE,
  SHADER_BUFFER_EDGE_FAC,
  SHADER_BUFFER_LNOR,
  SHADER_BUFFER_TRIS,
  SHADER_BUFFER_TRIS_MULTIPLE_MATERIALS,
  SHADER_BUFFER_NORMALS_ACCUMULATE,
  SHADER_BUFFER_NORMALS_FINALIZE,
  SHADER_BUFFER_CUSTOM_NORMALS_FINALIZE,
  SHADER_PATCH_EVALUATION,
  SHADER_PATCH_EVALUATION_FVAR,
  SHADER_PATCH_EVALUATION_FACE_DOTS,
  SHADER_PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS,
  SHADER_PATCH_EVALUATION_ORCO,
  SHADER_COMP_CUSTOM_DATA_INTERP_1D,
  SHADER_COMP_CUSTOM_DATA_INTERP_2D,
  SHADER_COMP_CUSTOM_DATA_INTERP_3D,
  SHADER_COMP_CUSTOM_DATA_INTERP_4D,
  SHADER_BUFFER_SCULPT_DATA,
  SHADER_BUFFER_UV_STRETCH_ANGLE,
  SHADER_BUFFER_UV_STRETCH_AREA,

  NUM_SHADERS,
};

static GPUShader *g_subdiv_shaders[NUM_SHADERS];

#define SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS 4
static GPUShader
    *g_subdiv_custom_data_shaders[SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS][GPU_COMP_MAX];

static const char *get_shader_code(int shader_type)
{
  switch (shader_type) {
    case SHADER_BUFFER_LINES:
    case SHADER_BUFFER_LINES_LOOSE: {
      return datatoc_common_subdiv_ibo_lines_comp_glsl;
    }
    case SHADER_BUFFER_EDGE_FAC: {
      return datatoc_common_subdiv_vbo_edge_fac_comp_glsl;
    }
    case SHADER_BUFFER_LNOR: {
      return datatoc_common_subdiv_vbo_lnor_comp_glsl;
    }
    case SHADER_BUFFER_TRIS:
    case SHADER_BUFFER_TRIS_MULTIPLE_MATERIALS: {
      return datatoc_common_subdiv_ibo_tris_comp_glsl;
    }
    case SHADER_BUFFER_NORMALS_ACCUMULATE: {
      return datatoc_common_subdiv_normals_accumulate_comp_glsl;
    }
    case SHADER_BUFFER_NORMALS_FINALIZE:
    case SHADER_BUFFER_CUSTOM_NORMALS_FINALIZE: {
      return datatoc_common_subdiv_normals_finalize_comp_glsl;
    }
    case SHADER_PATCH_EVALUATION:
    case SHADER_PATCH_EVALUATION_FVAR:
    case SHADER_PATCH_EVALUATION_FACE_DOTS:
    case SHADER_PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS:
    case SHADER_PATCH_EVALUATION_ORCO: {
      return datatoc_common_subdiv_patch_evaluation_comp_glsl;
    }
    case SHADER_COMP_CUSTOM_DATA_INTERP_1D:
    case SHADER_COMP_CUSTOM_DATA_INTERP_2D:
    case SHADER_COMP_CUSTOM_DATA_INTERP_3D:
    case SHADER_COMP_CUSTOM_DATA_INTERP_4D: {
      return datatoc_common_subdiv_custom_data_interp_comp_glsl;
    }
    case SHADER_BUFFER_SCULPT_DATA: {
      return datatoc_common_subdiv_vbo_sculpt_data_comp_glsl;
    }
    case SHADER_BUFFER_UV_STRETCH_ANGLE: {
      return datatoc_common_subdiv_vbo_edituv_strech_angle_comp_glsl;
    }
    case SHADER_BUFFER_UV_STRETCH_AREA: {
      return datatoc_common_subdiv_vbo_edituv_strech_area_comp_glsl;
    }
  }
  return nullptr;
}

static const char *get_shader_name(int shader_type)
{
  switch (shader_type) {
    case SHADER_BUFFER_LINES: {
      return "subdiv lines build";
    }
    case SHADER_BUFFER_LINES_LOOSE: {
      return "subdiv lines loose build";
    }
    case SHADER_BUFFER_LNOR: {
      return "subdiv lnor build";
    }
    case SHADER_BUFFER_EDGE_FAC: {
      return "subdiv edge fac build";
    }
    case SHADER_BUFFER_TRIS:
    case SHADER_BUFFER_TRIS_MULTIPLE_MATERIALS: {
      return "subdiv tris";
    }
    case SHADER_BUFFER_NORMALS_ACCUMULATE: {
      return "subdiv normals accumulate";
    }
    case SHADER_BUFFER_NORMALS_FINALIZE: {
      return "subdiv normals finalize";
    }
    case SHADER_PATCH_EVALUATION: {
      return "subdiv patch evaluation";
    }
    case SHADER_PATCH_EVALUATION_FVAR: {
      return "subdiv patch evaluation face-varying";
    }
    case SHADER_PATCH_EVALUATION_FACE_DOTS: {
      return "subdiv patch evaluation face dots";
    }
    case SHADER_PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS: {
      return "subdiv patch evaluation face dots with normals";
    }
    case SHADER_PATCH_EVALUATION_ORCO: {
      return "subdiv patch evaluation orco";
    }
    case SHADER_COMP_CUSTOM_DATA_INTERP_1D: {
      return "subdiv custom data interp 1D";
    }
    case SHADER_COMP_CUSTOM_DATA_INTERP_2D: {
      return "subdiv custom data interp 2D";
    }
    case SHADER_COMP_CUSTOM_DATA_INTERP_3D: {
      return "subdiv custom data interp 3D";
    }
    case SHADER_COMP_CUSTOM_DATA_INTERP_4D: {
      return "subdiv custom data interp 4D";
    }
    case SHADER_BUFFER_SCULPT_DATA: {
      return "subdiv sculpt data";
    }
    case SHADER_BUFFER_UV_STRETCH_ANGLE: {
      return "subdiv uv stretch angle";
    }
    case SHADER_BUFFER_UV_STRETCH_AREA: {
      return "subdiv uv stretch area";
    }
  }
  return nullptr;
}

static GPUShader *get_patch_evaluation_shader(int shader_type)
{
  if (g_subdiv_shaders[shader_type] == nullptr) {
    const char *compute_code = get_shader_code(shader_type);

    const char *defines = nullptr;
    if (shader_type == SHADER_PATCH_EVALUATION) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n";
    }
    else if (shader_type == SHADER_PATCH_EVALUATION_FVAR) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n"
          "#define FVAR_EVALUATION\n";
    }
    else if (shader_type == SHADER_PATCH_EVALUATION_FACE_DOTS) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n"
          "#define FDOTS_EVALUATION\n";
    }
    else if (shader_type == SHADER_PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n"
          "#define FDOTS_EVALUATION\n"
          "#define FDOTS_NORMALS\n";
    }
    else if (shader_type == SHADER_PATCH_EVALUATION_ORCO) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n"
          "#define ORCO_EVALUATION\n";
    }
    else {
      BLI_assert_unreachable();
    }

    /* Merge OpenSubdiv library code with our own library code. */
    const char *patch_basis_source = openSubdiv_getGLSLPatchBasisSource();
    const char *subdiv_lib_code = datatoc_common_subdiv_lib_glsl;
    char *library_code = BLI_string_joinN(patch_basis_source, subdiv_lib_code);
    g_subdiv_shaders[shader_type] = GPU_shader_create_compute(
        compute_code, library_code, defines, get_shader_name(shader_type));
    MEM_freeN(library_code);
  }

  return g_subdiv_shaders[shader_type];
}

static GPUShader *get_subdiv_shader(int shader_type)
{
  if (ELEM(shader_type,
           SHADER_PATCH_EVALUATION,
           SHADER_PATCH_EVALUATION_FVAR,
           SHADER_PATCH_EVALUATION_FACE_DOTS,
           SHADER_PATCH_EVALUATION_ORCO))
  {
    return get_patch_evaluation_shader(shader_type);
  }

  BLI_assert(!ELEM(shader_type,
                   SHADER_COMP_CUSTOM_DATA_INTERP_1D,
                   SHADER_COMP_CUSTOM_DATA_INTERP_2D,
                   SHADER_COMP_CUSTOM_DATA_INTERP_3D,
                   SHADER_COMP_CUSTOM_DATA_INTERP_4D));

  if (g_subdiv_shaders[shader_type] == nullptr) {
    const char *compute_code = get_shader_code(shader_type);
    const char *defines = nullptr;

    if (ELEM(shader_type,
             SHADER_BUFFER_LINES,
             SHADER_BUFFER_LNOR,
             SHADER_BUFFER_TRIS_MULTIPLE_MATERIALS,
             SHADER_BUFFER_UV_STRETCH_AREA))
    {
      defines = "#define SUBDIV_POLYGON_OFFSET\n";
    }
    else if (shader_type == SHADER_BUFFER_TRIS) {
      defines =
          "#define SUBDIV_POLYGON_OFFSET\n"
          "#define SINGLE_MATERIAL\n";
    }
    else if (shader_type == SHADER_BUFFER_LINES_LOOSE) {
      defines = "#define LINES_LOOSE\n";
    }
    else if (shader_type == SHADER_BUFFER_EDGE_FAC) {
      /* No separate shader for the AMD driver case as we assume that the GPU will not change
       * during the execution of the program. */
      defines = GPU_crappy_amd_driver() ? "#define GPU_AMD_DRIVER_BYTE_BUG\n" : nullptr;
    }
    else if (shader_type == SHADER_BUFFER_CUSTOM_NORMALS_FINALIZE) {
      defines = "#define CUSTOM_NORMALS\n";
    }

    g_subdiv_shaders[shader_type] = GPU_shader_create_compute(
        compute_code, datatoc_common_subdiv_lib_glsl, defines, get_shader_name(shader_type));
  }
  return g_subdiv_shaders[shader_type];
}

static GPUShader *get_subdiv_custom_data_shader(int comp_type, int dimensions)
{
  BLI_assert(dimensions >= 1 && dimensions <= SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS);
  if (comp_type == GPU_COMP_U16) {
    BLI_assert(dimensions == 4);
  }

  GPUShader *&shader = g_subdiv_custom_data_shaders[dimensions - 1][comp_type];

  if (shader == nullptr) {
    const char *compute_code = get_shader_code(SHADER_COMP_CUSTOM_DATA_INTERP_1D + dimensions - 1);

    int shader_type = SHADER_COMP_CUSTOM_DATA_INTERP_1D + dimensions - 1;

    std::string defines = "#define SUBDIV_POLYGON_OFFSET\n";
    defines += "#define DIMENSIONS " + std::to_string(dimensions) + "\n";
    switch (comp_type) {
      case GPU_COMP_U16:
        defines += "#define GPU_COMP_U16\n";
        break;
      case GPU_COMP_I32:
        defines += "#define GPU_COMP_I32\n";
        break;
      case GPU_COMP_F32:
        /* float is the default */
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

    shader = GPU_shader_create_compute(compute_code,
                                       datatoc_common_subdiv_lib_glsl,
                                       defines.c_str(),
                                       get_shader_name(shader_type));
  }
  return shader;
}

/* -------------------------------------------------------------------- */
/** \name Vertex Formats
 *
 * Used for data transfer from OpenSubdiv, and for data processing on our side.
 * \{ */

static GPUVertFormat *get_uvs_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "uvs", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  }
  return &format;
}

/* Vertex format for `OpenSubdiv::Osd::PatchArray`. */
static GPUVertFormat *get_patch_array_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "regDesc", GPU_COMP_I32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "desc", GPU_COMP_I32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "numPatches", GPU_COMP_I32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "indexBase", GPU_COMP_I32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "stride", GPU_COMP_I32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "primitiveIdBase", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  return &format;
}

/* Vertex format used for the `PatchTable::PatchHandle`. */
static GPUVertFormat *get_patch_handle_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "vertex_index", GPU_COMP_I32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "array_index", GPU_COMP_I32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "patch_index", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  return &format;
}

/* Vertex format used for the quad-tree nodes of the PatchMap. */
static GPUVertFormat *get_quadtree_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "child", GPU_COMP_U32, 4, GPU_FETCH_INT);
  }
  return &format;
}

/* Vertex format for `OpenSubdiv::Osd::PatchParam`, not really used, it is only for making sure
 * that the #GPUVertBuf used to wrap the OpenSubdiv patch param buffer is valid. */
static GPUVertFormat *get_patch_param_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  return &format;
}

/* Vertex format for the patches' vertices index buffer. */
static GPUVertFormat *get_patch_index_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  return &format;
}

/* Vertex format for the OpenSubdiv vertex buffer. */
static GPUVertFormat *get_subdiv_vertex_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* We use 4 components for the vectors to account for padding in the compute shaders, where
     * vec3 is promoted to vec4. */
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  return &format;
}

struct CompressedPatchCoord {
  int ptex_face_index;
  /* UV coordinate encoded as u << 16 | v, where u and v are quantized on 16-bits. */
  uint encoded_uv;
};

MINLINE CompressedPatchCoord make_patch_coord(int ptex_face_index, float u, float v)
{
  CompressedPatchCoord patch_coord = {
      ptex_face_index,
      (uint(u * 65535.0f) << 16) | uint(v * 65535.0f),
  };
  return patch_coord;
}

/* Vertex format used for the #CompressedPatchCoord. */
static GPUVertFormat *get_blender_patch_coords_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING! Adjust #CompressedPatchCoord accordingly. */
    GPU_vertformat_attr_add(&format, "ptex_face_index", GPU_COMP_U32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "uv", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }
  return &format;
}

static GPUVertFormat *get_origindex_format()
{
  static GPUVertFormat format;
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "index", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  return &format;
}

GPUVertFormat *draw_subdiv_get_pos_nor_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "vnor");
  }
  return &format;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities to initialize a OpenSubdiv_Buffer for a GPUVertBuf.
 * \{ */

static void vertbuf_bind_gpu(const OpenSubdiv_Buffer *buffer)
{
  GPUVertBuf *verts = (GPUVertBuf *)(buffer->data);
  GPU_vertbuf_use(verts);
}

static void *vertbuf_alloc(const OpenSubdiv_Buffer *interface, const uint len)
{
  GPUVertBuf *verts = (GPUVertBuf *)(interface->data);
  GPU_vertbuf_data_alloc(verts, len);
  return GPU_vertbuf_get_data(verts);
}

static void vertbuf_device_alloc(const OpenSubdiv_Buffer *interface, const uint len)
{
  GPUVertBuf *verts = (GPUVertBuf *)(interface->data);
  /* This assumes that GPU_USAGE_DEVICE_ONLY was used, which won't allocate host memory. */
  // BLI_assert(GPU_vertbuf_get_usage(verts) == GPU_USAGE_DEVICE_ONLY);
  GPU_vertbuf_data_alloc(verts, len);
}

static void vertbuf_wrap_device_handle(const OpenSubdiv_Buffer *interface, uint64_t handle)
{
  GPUVertBuf *verts = (GPUVertBuf *)(interface->data);
  GPU_vertbuf_wrap_handle(verts, handle);
}

static void vertbuf_update_data(const OpenSubdiv_Buffer *interface,
                                uint start,
                                uint len,
                                const void *data)
{
  GPUVertBuf *verts = (GPUVertBuf *)(interface->data);
  GPU_vertbuf_update_sub(verts, start, len, data);
}

static void opensubdiv_gpu_buffer_init(OpenSubdiv_Buffer *buffer_interface, GPUVertBuf *vertbuf)
{
  buffer_interface->data = vertbuf;
  buffer_interface->bind_gpu = vertbuf_bind_gpu;
  buffer_interface->buffer_offset = 0;
  buffer_interface->wrap_device_handle = vertbuf_wrap_device_handle;
  buffer_interface->alloc = vertbuf_alloc;
  buffer_interface->device_alloc = vertbuf_device_alloc;
  buffer_interface->device_update = vertbuf_update_data;
}

static GPUVertBuf *create_buffer_and_interface(OpenSubdiv_Buffer *interface, GPUVertFormat *format)
{
  GPUVertBuf *buffer = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(buffer, format, GPU_USAGE_DEVICE_ONLY);
  opensubdiv_gpu_buffer_init(interface, buffer);
  return buffer;
}

/** \} */

// --------------------------------------------------------

static uint tris_count_from_number_of_loops(const uint number_of_loops)
{
  const uint32_t number_of_quads = number_of_loops / 4;
  return number_of_quads * 2;
}

/* -------------------------------------------------------------------- */
/** \name Utilities to build a GPUVertBuf from an origindex buffer.
 * \{ */

void draw_subdiv_init_origindex_buffer(GPUVertBuf *buffer,
                                       int32_t *vert_origindex,
                                       uint num_loops,
                                       uint loose_len)
{
  GPU_vertbuf_init_with_format_ex(buffer, get_origindex_format(), GPU_USAGE_STATIC);
  GPU_vertbuf_data_alloc(buffer, num_loops + loose_len);

  int32_t *vbo_data = (int32_t *)GPU_vertbuf_get_data(buffer);
  memcpy(vbo_data, vert_origindex, num_loops * sizeof(int32_t));
}

GPUVertBuf *draw_subdiv_build_origindex_buffer(int *vert_origindex, uint num_loops)
{
  GPUVertBuf *buffer = GPU_vertbuf_calloc();
  draw_subdiv_init_origindex_buffer(buffer, vert_origindex, num_loops, 0);
  return buffer;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities for DRWPatchMap.
 * \{ */

static void draw_patch_map_build(DRWPatchMap *gpu_patch_map, Subdiv *subdiv)
{
  GPUVertBuf *patch_map_handles = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(patch_map_handles, get_patch_handle_format(), GPU_USAGE_STATIC);

  GPUVertBuf *patch_map_quadtree = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(patch_map_quadtree, get_quadtree_format(), GPU_USAGE_STATIC);

  OpenSubdiv_Buffer patch_map_handles_interface;
  opensubdiv_gpu_buffer_init(&patch_map_handles_interface, patch_map_handles);

  OpenSubdiv_Buffer patch_map_quad_tree_interface;
  opensubdiv_gpu_buffer_init(&patch_map_quad_tree_interface, patch_map_quadtree);

  int min_patch_face = 0;
  int max_patch_face = 0;
  int max_depth = 0;
  int patches_are_triangular = 0;

  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;
  evaluator->getPatchMap(evaluator,
                         &patch_map_handles_interface,
                         &patch_map_quad_tree_interface,
                         &min_patch_face,
                         &max_patch_face,
                         &max_depth,
                         &patches_are_triangular);

  gpu_patch_map->patch_map_handles = patch_map_handles;
  gpu_patch_map->patch_map_quadtree = patch_map_quadtree;
  gpu_patch_map->min_patch_face = min_patch_face;
  gpu_patch_map->max_patch_face = max_patch_face;
  gpu_patch_map->max_depth = max_depth;
  gpu_patch_map->patches_are_triangular = patches_are_triangular;
}

static void draw_patch_map_free(DRWPatchMap *gpu_patch_map)
{
  GPU_VERTBUF_DISCARD_SAFE(gpu_patch_map->patch_map_handles);
  GPU_VERTBUF_DISCARD_SAFE(gpu_patch_map->patch_map_quadtree);
  gpu_patch_map->min_patch_face = 0;
  gpu_patch_map->max_patch_face = 0;
  gpu_patch_map->max_depth = 0;
  gpu_patch_map->patches_are_triangular = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivCache
 * \{ */

static bool draw_subdiv_cache_need_face_data(const DRWSubdivCache &cache)
{
  return cache.subdiv && cache.subdiv->evaluator && cache.num_subdiv_loops != 0;
}

static void draw_subdiv_cache_free_material_data(DRWSubdivCache &cache)
{
  GPU_VERTBUF_DISCARD_SAFE(cache.face_mat_offset);
  MEM_SAFE_FREE(cache.mat_start);
  MEM_SAFE_FREE(cache.mat_end);
}

static void draw_subdiv_free_edit_mode_cache(DRWSubdivCache &cache)
{
  GPU_VERTBUF_DISCARD_SAFE(cache.verts_orig_index);
  GPU_VERTBUF_DISCARD_SAFE(cache.edges_orig_index);
  GPU_VERTBUF_DISCARD_SAFE(cache.edges_draw_flag);
  GPU_VERTBUF_DISCARD_SAFE(cache.fdots_patch_coords);
}

void draw_subdiv_cache_free(DRWSubdivCache &cache)
{
  GPU_VERTBUF_DISCARD_SAFE(cache.patch_coords);
  GPU_VERTBUF_DISCARD_SAFE(cache.corner_patch_coords);
  GPU_VERTBUF_DISCARD_SAFE(cache.face_ptex_offset_buffer);
  GPU_VERTBUF_DISCARD_SAFE(cache.subdiv_face_offset_buffer);
  GPU_VERTBUF_DISCARD_SAFE(cache.extra_coarse_face_data);
  MEM_SAFE_FREE(cache.subdiv_loop_subdiv_vert_index);
  MEM_SAFE_FREE(cache.subdiv_loop_subdiv_edge_index);
  MEM_SAFE_FREE(cache.subdiv_loop_face_index);
  MEM_SAFE_FREE(cache.subdiv_face_offset);
  GPU_VERTBUF_DISCARD_SAFE(cache.subdiv_vertex_face_adjacency_offsets);
  GPU_VERTBUF_DISCARD_SAFE(cache.subdiv_vertex_face_adjacency);
  cache.resolution = 0;
  cache.num_subdiv_loops = 0;
  cache.num_subdiv_edges = 0;
  cache.num_subdiv_verts = 0;
  cache.num_subdiv_triangles = 0;
  cache.num_coarse_faces = 0;
  cache.num_subdiv_quads = 0;
  cache.may_have_loose_geom = false;
  draw_subdiv_free_edit_mode_cache(cache);
  draw_subdiv_cache_free_material_data(cache);
  draw_patch_map_free(&cache.gpu_patch_map);
  if (cache.ubo) {
    GPU_uniformbuf_free(cache.ubo);
    cache.ubo = nullptr;
  }
  MEM_SAFE_FREE(cache.loose_geom.edges);
  MEM_SAFE_FREE(cache.loose_geom.verts);
  cache.loose_geom.edge_len = 0;
  cache.loose_geom.vert_len = 0;
  cache.loose_geom.loop_len = 0;
}

/* Flags used in #DRWSubdivCache.extra_coarse_face_data. The flags are packed in the upper bits of
 * each uint (one per coarse face), #SUBDIV_COARSE_FACE_FLAG_OFFSET tells where they are in the
 * packed bits. */
#define SUBDIV_COARSE_FACE_FLAG_SMOOTH 1u
#define SUBDIV_COARSE_FACE_FLAG_SELECT 2u
#define SUBDIV_COARSE_FACE_FLAG_ACTIVE 4u
#define SUBDIV_COARSE_FACE_FLAG_HIDDEN 8u

#define SUBDIV_COARSE_FACE_FLAG_OFFSET 28u

#define SUBDIV_COARSE_FACE_FLAG_SMOOTH_MASK \
  (SUBDIV_COARSE_FACE_FLAG_SMOOTH << SUBDIV_COARSE_FACE_FLAG_OFFSET)
#define SUBDIV_COARSE_FACE_FLAG_SELECT_MASK \
  (SUBDIV_COARSE_FACE_FLAG_SELECT << SUBDIV_COARSE_FACE_FLAG_OFFSET)
#define SUBDIV_COARSE_FACE_FLAG_ACTIVE_MASK \
  (SUBDIV_COARSE_FACE_FLAG_ACTIVE << SUBDIV_COARSE_FACE_FLAG_OFFSET)
#define SUBDIV_COARSE_FACE_FLAG_HIDDEN_MASK \
  (SUBDIV_COARSE_FACE_FLAG_HIDDEN << SUBDIV_COARSE_FACE_FLAG_OFFSET)

#define SUBDIV_COARSE_FACE_LOOP_START_MASK \
  ~((SUBDIV_COARSE_FACE_FLAG_SMOOTH | SUBDIV_COARSE_FACE_FLAG_SELECT | \
     SUBDIV_COARSE_FACE_FLAG_ACTIVE | SUBDIV_COARSE_FACE_FLAG_HIDDEN) \
    << SUBDIV_COARSE_FACE_FLAG_OFFSET)

static uint32_t compute_coarse_face_flag_bm(BMFace *f, BMFace *efa_act)
{
  uint32_t flag = 0;
  if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
    flag |= SUBDIV_COARSE_FACE_FLAG_SELECT;
  }
  if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    flag |= SUBDIV_COARSE_FACE_FLAG_HIDDEN;
  }
  if (f == efa_act) {
    flag |= SUBDIV_COARSE_FACE_FLAG_ACTIVE;
  }
  return flag;
}

static void draw_subdiv_cache_extra_coarse_face_data_bm(BMesh *bm,
                                                        BMFace *efa_act,
                                                        uint32_t *flags_data)
{
  BMFace *f;
  BMIter iter;

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const int index = BM_elem_index_get(f);
    uint32_t flag = compute_coarse_face_flag_bm(f, efa_act);
    if (BM_elem_flag_test(f, BM_ELEM_SMOOTH)) {
      flag |= SUBDIV_COARSE_FACE_FLAG_SMOOTH;
    }
    const int loopstart = BM_elem_index_get(f->l_first);
    flags_data[index] = uint(loopstart) | (flag << SUBDIV_COARSE_FACE_FLAG_OFFSET);
  }
}

static void draw_subdiv_cache_extra_coarse_face_data_mesh(const MeshRenderData &mr,
                                                          Mesh *mesh,
                                                          uint32_t *flags_data)
{
  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    uint32_t flag = 0;
    if (!(mr.normals_domain == bke::MeshNormalDomain::Face ||
          (!mr.sharp_faces.is_empty() && mr.sharp_faces[i])))
    {
      flag |= SUBDIV_COARSE_FACE_FLAG_SMOOTH;
    }
    if (!mr.select_poly.is_empty() && mr.select_poly[i]) {
      flag |= SUBDIV_COARSE_FACE_FLAG_SELECT;
    }
    if (!mr.hide_poly.is_empty() && mr.hide_poly[i]) {
      flag |= SUBDIV_COARSE_FACE_FLAG_HIDDEN;
    }
    flags_data[i] = uint(faces[i].start()) | (flag << SUBDIV_COARSE_FACE_FLAG_OFFSET);
  }
}

static void draw_subdiv_cache_extra_coarse_face_data_mapped(Mesh *mesh,
                                                            BMesh *bm,
                                                            MeshRenderData &mr,
                                                            uint32_t *flags_data)
{
  if (bm == nullptr) {
    draw_subdiv_cache_extra_coarse_face_data_mesh(mr, mesh, flags_data);
    return;
  }

  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    BMFace *f = bm_original_face_get(mr, i);
    /* Selection and hiding from bmesh. */
    uint32_t flag = (f) ? compute_coarse_face_flag_bm(f, mr.efa_act) : 0;
    /* Smooth from mesh. */
    if (!(mr.normals_domain == bke::MeshNormalDomain::Face ||
          (!mr.sharp_faces.is_empty() && mr.sharp_faces[i])))
    {
      flag |= SUBDIV_COARSE_FACE_FLAG_SMOOTH;
    }
    flags_data[i] = uint(faces[i].start()) | (flag << SUBDIV_COARSE_FACE_FLAG_OFFSET);
  }
}

static void draw_subdiv_cache_update_extra_coarse_face_data(DRWSubdivCache &cache,
                                                            Mesh *mesh,
                                                            MeshRenderData &mr)
{
  if (cache.extra_coarse_face_data == nullptr) {
    cache.extra_coarse_face_data = GPU_vertbuf_calloc();
    static GPUVertFormat format;
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "data", GPU_COMP_U32, 1, GPU_FETCH_INT);
    }
    GPU_vertbuf_init_with_format_ex(cache.extra_coarse_face_data, &format, GPU_USAGE_DYNAMIC);
    GPU_vertbuf_data_alloc(cache.extra_coarse_face_data,
                           mr.extract_type == MR_EXTRACT_BMESH ? cache.bm->totface :
                                                                 mesh->faces_num);
  }

  uint32_t *flags_data = (uint32_t *)GPU_vertbuf_get_data(cache.extra_coarse_face_data);

  if (mr.extract_type == MR_EXTRACT_BMESH) {
    draw_subdiv_cache_extra_coarse_face_data_bm(cache.bm, mr.efa_act, flags_data);
  }
  else if (mr.p_origindex != nullptr) {
    draw_subdiv_cache_extra_coarse_face_data_mapped(mesh, cache.bm, mr, flags_data);
  }
  else {
    draw_subdiv_cache_extra_coarse_face_data_mesh(mr, mesh, flags_data);
  }

  /* Make sure updated data is re-uploaded. */
  GPU_vertbuf_tag_dirty(cache.extra_coarse_face_data);
}

static DRWSubdivCache &mesh_batch_cache_ensure_subdiv_cache(MeshBatchCache &mbc)
{
  DRWSubdivCache *subdiv_cache = mbc.subdiv_cache;
  if (subdiv_cache == nullptr) {
    subdiv_cache = static_cast<DRWSubdivCache *>(
        MEM_callocN(sizeof(DRWSubdivCache), "DRWSubdivCache"));
  }
  mbc.subdiv_cache = subdiv_cache;
  return *subdiv_cache;
}

static void draw_subdiv_invalidate_evaluator_for_orco(Subdiv *subdiv, Mesh *mesh)
{
  if (!(subdiv && subdiv->evaluator)) {
    return;
  }

  const bool has_orco = CustomData_has_layer(&mesh->vert_data, CD_ORCO);
  if (has_orco && !subdiv->evaluator->hasVertexData(subdiv->evaluator)) {
    /* If we suddenly have/need original coordinates, recreate the evaluator if the extra
     * source was not created yet. The refiner also has to be recreated as refinement for source
     * and vertex data is done only once. */
    openSubdiv_deleteEvaluator(subdiv->evaluator);
    subdiv->evaluator = nullptr;

    if (subdiv->topology_refiner != nullptr) {
      openSubdiv_deleteTopologyRefiner(subdiv->topology_refiner);
      subdiv->topology_refiner = nullptr;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subdivision grid traversal.
 *
 * Traverse the uniform subdivision grid over coarse faces and gather useful information for
 * building the draw buffers on the GPU. We primarily gather the patch coordinates for all
 * subdivision faces, as well as the original coarse indices for each subdivision element (vertex,
 * face, or edge) which directly maps to its coarse counterpart (note that all subdivision faces
 * map to a coarse face). This information will then be cached in #DRWSubdivCache for subsequent
 * reevaluations, as long as the topology does not change.
 * \{ */

struct DRWCacheBuildingContext {
  const Mesh *coarse_mesh;
  const Subdiv *subdiv;
  const SubdivToMeshSettings *settings;

  DRWSubdivCache *cache;

  /* Pointers into #DRWSubdivCache buffers for easier access during traversal. */
  CompressedPatchCoord *patch_coords;
  int *subdiv_loop_vert_index;
  int *subdiv_loop_subdiv_vert_index;
  int *subdiv_loop_edge_index;
  int *subdiv_loop_edge_draw_flag;
  int *subdiv_loop_subdiv_edge_index;
  int *subdiv_loop_face_index;

  /* Temporary buffers used during traversal. */
  int *vert_origindex_map;
  int *edge_draw_flag_map;
  int *edge_origindex_map;

  /* #CD_ORIGINDEX layers from the mesh to directly look up during traversal the original-index
   * from the base mesh for edit data so that we do not have to handle yet another GPU buffer and
   * do this in the shaders. */
  const int *v_origindex;
  const int *e_origindex;
};

static bool draw_subdiv_topology_info_cb(const SubdivForeachContext *foreach_context,
                                         const int num_verts,
                                         const int num_edges,
                                         const int num_loops,
                                         const int num_faces,
                                         const int *subdiv_face_offset)
{
  /* num_loops does not take into account meshes with only loose geometry, which might be meshes
   * used as custom bone shapes, so let's check the num_verts also. */
  if (num_verts == 0 && num_loops == 0) {
    return false;
  }

  DRWCacheBuildingContext *ctx = (DRWCacheBuildingContext *)(foreach_context->user_data);
  DRWSubdivCache *cache = ctx->cache;

  /* Set topology information only if we have loops. */
  if (num_loops != 0) {
    cache->num_subdiv_edges = uint(num_edges);
    cache->num_subdiv_loops = uint(num_loops);
    cache->num_subdiv_verts = uint(num_verts);
    cache->num_subdiv_quads = uint(num_faces);
    cache->subdiv_face_offset = static_cast<int *>(MEM_dupallocN(subdiv_face_offset));
  }

  cache->may_have_loose_geom = num_verts != 0 || num_edges != 0;

  /* Initialize cache buffers, prefer dynamic usage so we can reuse memory on the host even after
   * it was sent to the device, since we may use the data while building other buffers on the CPU
   * side. */
  cache->patch_coords = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      cache->patch_coords, get_blender_patch_coords_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(cache->patch_coords, cache->num_subdiv_loops);

  cache->corner_patch_coords = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      cache->corner_patch_coords, get_blender_patch_coords_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(cache->corner_patch_coords, cache->num_subdiv_loops);

  cache->verts_orig_index = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      cache->verts_orig_index, get_origindex_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(cache->verts_orig_index, cache->num_subdiv_loops);

  cache->edges_orig_index = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      cache->edges_orig_index, get_origindex_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(cache->edges_orig_index, cache->num_subdiv_loops);

  cache->edges_draw_flag = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      cache->edges_draw_flag, get_origindex_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(cache->edges_draw_flag, cache->num_subdiv_loops);

  cache->subdiv_loop_subdiv_vert_index = static_cast<int *>(
      MEM_mallocN(cache->num_subdiv_loops * sizeof(int), "subdiv_loop_subdiv_vert_index"));

  cache->subdiv_loop_subdiv_edge_index = static_cast<int *>(
      MEM_mallocN(cache->num_subdiv_loops * sizeof(int), "subdiv_loop_subdiv_edge_index"));

  cache->subdiv_loop_face_index = static_cast<int *>(
      MEM_mallocN(cache->num_subdiv_loops * sizeof(int), "subdiv_loop_face_index"));

  /* Initialize context pointers and temporary buffers. */
  ctx->patch_coords = (CompressedPatchCoord *)GPU_vertbuf_get_data(cache->patch_coords);
  ctx->subdiv_loop_vert_index = (int *)GPU_vertbuf_get_data(cache->verts_orig_index);
  ctx->subdiv_loop_edge_index = (int *)GPU_vertbuf_get_data(cache->edges_orig_index);
  ctx->subdiv_loop_edge_draw_flag = (int *)GPU_vertbuf_get_data(cache->edges_draw_flag);
  ctx->subdiv_loop_subdiv_vert_index = cache->subdiv_loop_subdiv_vert_index;
  ctx->subdiv_loop_subdiv_edge_index = cache->subdiv_loop_subdiv_edge_index;
  ctx->subdiv_loop_face_index = cache->subdiv_loop_face_index;

  ctx->v_origindex = static_cast<const int *>(
      CustomData_get_layer(&ctx->coarse_mesh->vert_data, CD_ORIGINDEX));

  ctx->e_origindex = static_cast<const int *>(
      CustomData_get_layer(&ctx->coarse_mesh->edge_data, CD_ORIGINDEX));

  if (cache->num_subdiv_verts) {
    ctx->vert_origindex_map = static_cast<int *>(
        MEM_mallocN(cache->num_subdiv_verts * sizeof(int), "subdiv_vert_origindex_map"));
    for (int i = 0; i < num_verts; i++) {
      ctx->vert_origindex_map[i] = -1;
    }
  }

  if (cache->num_subdiv_edges) {
    ctx->edge_origindex_map = static_cast<int *>(
        MEM_mallocN(cache->num_subdiv_edges * sizeof(int), "subdiv_edge_origindex_map"));
    for (int i = 0; i < num_edges; i++) {
      ctx->edge_origindex_map[i] = -1;
    }
    ctx->edge_draw_flag_map = static_cast<int *>(
        MEM_callocN(cache->num_subdiv_edges * sizeof(int), "subdiv_edge_draw_flag_map"));
  }

  return true;
}

static void draw_subdiv_vertex_corner_cb(const SubdivForeachContext *foreach_context,
                                         void * /*tls*/,
                                         const int /*ptex_face_index*/,
                                         const float /*u*/,
                                         const float /*v*/,
                                         const int coarse_vertex_index,
                                         const int /*coarse_face_index*/,
                                         const int /*coarse_corner*/,
                                         const int subdiv_vertex_index)
{
  BLI_assert(coarse_vertex_index != ORIGINDEX_NONE);
  DRWCacheBuildingContext *ctx = (DRWCacheBuildingContext *)(foreach_context->user_data);
  ctx->vert_origindex_map[subdiv_vertex_index] = coarse_vertex_index;
}

static void draw_subdiv_vertex_edge_cb(const SubdivForeachContext * /*foreach_context*/,
                                       void * /*tls_v*/,
                                       const int /*ptex_face_index*/,
                                       const float /*u*/,
                                       const float /*v*/,
                                       const int /*coarse_edge_index*/,
                                       const int /*coarse_face_index*/,
                                       const int /*coarse_corner*/,
                                       const int /*subdiv_vertex_index*/)
{
  /* Required if SubdivForeachContext.vertex_corner is also set. */
}

static void draw_subdiv_edge_cb(const SubdivForeachContext *foreach_context,
                                void * /*tls*/,
                                const int coarse_edge_index,
                                const int subdiv_edge_index,
                                const bool /*is_loose*/,
                                const int /*subdiv_v1*/,
                                const int /*subdiv_v2*/)
{
  DRWCacheBuildingContext *ctx = (DRWCacheBuildingContext *)(foreach_context->user_data);

  if (!ctx->edge_origindex_map) {
    return;
  }

  if (coarse_edge_index == ORIGINDEX_NONE) {
    /* Not mapped to edge in the subdivision base mesh. */
    ctx->edge_origindex_map[subdiv_edge_index] = ORIGINDEX_NONE;
    if (!ctx->cache->optimal_display) {
      ctx->edge_draw_flag_map[subdiv_edge_index] = 1;
    }
  }
  else {
    if (ctx->e_origindex) {
      const int origindex = ctx->e_origindex[coarse_edge_index];
      ctx->edge_origindex_map[subdiv_edge_index] = origindex;
      if (!(origindex == ORIGINDEX_NONE && ctx->cache->hide_unmapped_edges)) {
        /* Not mapped to edge in original mesh (generated by a preceding modifier). */
        ctx->edge_draw_flag_map[subdiv_edge_index] = 1;
      }
    }
    else {
      ctx->edge_origindex_map[subdiv_edge_index] = coarse_edge_index;
      ctx->edge_draw_flag_map[subdiv_edge_index] = 1;
    }
  }
}

static void draw_subdiv_loop_cb(const SubdivForeachContext *foreach_context,
                                void * /*tls_v*/,
                                const int ptex_face_index,
                                const float u,
                                const float v,
                                const int /*coarse_loop_index*/,
                                const int coarse_face_index,
                                const int /*coarse_corner*/,
                                const int subdiv_loop_index,
                                const int subdiv_vertex_index,
                                const int subdiv_edge_index)
{
  DRWCacheBuildingContext *ctx = (DRWCacheBuildingContext *)(foreach_context->user_data);
  ctx->patch_coords[subdiv_loop_index] = make_patch_coord(ptex_face_index, u, v);

  int coarse_vertex_index = ctx->vert_origindex_map[subdiv_vertex_index];

  ctx->subdiv_loop_subdiv_vert_index[subdiv_loop_index] = subdiv_vertex_index;
  ctx->subdiv_loop_subdiv_edge_index[subdiv_loop_index] = subdiv_edge_index;
  ctx->subdiv_loop_face_index[subdiv_loop_index] = coarse_face_index;
  ctx->subdiv_loop_vert_index[subdiv_loop_index] = coarse_vertex_index;
}

static void draw_subdiv_foreach_callbacks(SubdivForeachContext *foreach_context)
{
  memset(foreach_context, 0, sizeof(*foreach_context));
  foreach_context->topology_info = draw_subdiv_topology_info_cb;
  foreach_context->loop = draw_subdiv_loop_cb;
  foreach_context->edge = draw_subdiv_edge_cb;
  foreach_context->vertex_corner = draw_subdiv_vertex_corner_cb;
  foreach_context->vertex_edge = draw_subdiv_vertex_edge_cb;
}

static void do_subdiv_traversal(DRWCacheBuildingContext *cache_building_context, Subdiv *subdiv)
{
  SubdivForeachContext foreach_context;
  draw_subdiv_foreach_callbacks(&foreach_context);
  foreach_context.user_data = cache_building_context;

  BKE_subdiv_foreach_subdiv_geometry(subdiv,
                                     &foreach_context,
                                     cache_building_context->settings,
                                     cache_building_context->coarse_mesh);

  /* Now that traversal is done, we can set up the right original indices for the
   * subdiv-loop-to-coarse-edge map.
   */
  for (int i = 0; i < cache_building_context->cache->num_subdiv_loops; i++) {
    const int edge_index = cache_building_context->subdiv_loop_subdiv_edge_index[i];
    cache_building_context->subdiv_loop_edge_index[i] =
        cache_building_context->edge_origindex_map[edge_index];
    cache_building_context->subdiv_loop_edge_draw_flag[i] =
        cache_building_context->edge_draw_flag_map[edge_index];
  }
}

static GPUVertBuf *gpu_vertbuf_create_from_format(GPUVertFormat *format, uint len)
{
  GPUVertBuf *verts = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format(verts, format);
  GPU_vertbuf_data_alloc(verts, len);
  return verts;
}

/* Build maps to hold enough information to tell which face is adjacent to which vertex; those will
 * be used for computing normals if limit surfaces are unavailable. */
static void build_vertex_face_adjacency_maps(DRWSubdivCache &cache)
{
  /* +1 so that we do not require a special case for the last vertex, this extra offset will
   * contain the total number of adjacent faces. */
  cache.subdiv_vertex_face_adjacency_offsets = gpu_vertbuf_create_from_format(
      get_origindex_format(), cache.num_subdiv_verts + 1);

  MutableSpan<int> vertex_offsets(
      static_cast<int *>(GPU_vertbuf_get_data(cache.subdiv_vertex_face_adjacency_offsets)),
      cache.num_subdiv_verts + 1);
  vertex_offsets.fill(0);

  offset_indices::build_reverse_offsets(
      {cache.subdiv_loop_subdiv_vert_index, cache.num_subdiv_loops}, vertex_offsets);

  cache.subdiv_vertex_face_adjacency = gpu_vertbuf_create_from_format(get_origindex_format(),
                                                                      cache.num_subdiv_loops);
  int *adjacent_faces = (int *)GPU_vertbuf_get_data(cache.subdiv_vertex_face_adjacency);
  int *tmp_set_faces = static_cast<int *>(
      MEM_callocN(sizeof(int) * cache.num_subdiv_verts, "tmp subdiv vertex offset"));

  for (int i = 0; i < cache.num_subdiv_loops / 4; i++) {
    for (int j = 0; j < 4; j++) {
      const int subdiv_vertex = cache.subdiv_loop_subdiv_vert_index[i * 4 + j];
      int first_face_offset = vertex_offsets[subdiv_vertex] + tmp_set_faces[subdiv_vertex];
      adjacent_faces[first_face_offset] = i;
      tmp_set_faces[subdiv_vertex] += 1;
    }
  }

  MEM_freeN(tmp_set_faces);
}

static bool draw_subdiv_build_cache(DRWSubdivCache &cache,
                                    Subdiv *subdiv,
                                    Mesh *mesh_eval,
                                    const SubsurfRuntimeData *runtime_data)
{
  SubdivToMeshSettings to_mesh_settings;
  to_mesh_settings.resolution = runtime_data->resolution;
  to_mesh_settings.use_optimal_display = false;

  if (cache.resolution != to_mesh_settings.resolution) {
    /* Resolution changed, we need to rebuild, free any existing cached data. */
    draw_subdiv_cache_free(cache);
  }

  /* If the resolution between the cache and the settings match for some reason, check if the patch
   * coordinates were not already generated. Those coordinates are specific to the resolution, so
   * they should be null either after initialization, or after freeing if the resolution (or some
   * other subdivision setting) changed.
   */
  if (cache.patch_coords != nullptr) {
    return true;
  }

  DRWCacheBuildingContext cache_building_context;
  memset(&cache_building_context, 0, sizeof(DRWCacheBuildingContext));
  cache_building_context.coarse_mesh = mesh_eval;
  cache_building_context.settings = &to_mesh_settings;
  cache_building_context.cache = &cache;

  do_subdiv_traversal(&cache_building_context, subdiv);
  if (cache.num_subdiv_loops == 0 && cache.num_subdiv_verts == 0 && !cache.may_have_loose_geom) {
    /* Either the traversal failed, or we have an empty mesh, either way we cannot go any further.
     * The subdiv_face_offset cannot then be reliably stored in the cache, so free it directly.
     */
    MEM_SAFE_FREE(cache.subdiv_face_offset);
    return false;
  }

  /* Only build face related data if we have polygons. */
  const OffsetIndices faces = mesh_eval->faces();
  if (cache.num_subdiv_loops != 0) {
    /* Build buffers for the PatchMap. */
    draw_patch_map_build(&cache.gpu_patch_map, subdiv);

    cache.face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv);

    /* Build patch coordinates for all the face dots. */
    cache.fdots_patch_coords = gpu_vertbuf_create_from_format(get_blender_patch_coords_format(),
                                                              mesh_eval->faces_num);
    CompressedPatchCoord *blender_fdots_patch_coords = (CompressedPatchCoord *)
        GPU_vertbuf_get_data(cache.fdots_patch_coords);
    for (int i = 0; i < mesh_eval->faces_num; i++) {
      const int ptex_face_index = cache.face_ptex_offset[i];
      if (faces[i].size() == 4) {
        /* For quads, the center coordinate of the coarse face has `u = v = 0.5`. */
        blender_fdots_patch_coords[i] = make_patch_coord(ptex_face_index, 0.5f, 0.5f);
      }
      else {
        /* For N-gons, since they are split into quads from the center, and since the center is
         * chosen to be the top right corner of each quad, the center coordinate of the coarse face
         * is any one of those top right corners with `u = v = 1.0`. */
        blender_fdots_patch_coords[i] = make_patch_coord(ptex_face_index, 1.0f, 1.0f);
      }
    }

    cache.subdiv_face_offset_buffer = draw_subdiv_build_origindex_buffer(cache.subdiv_face_offset,
                                                                         faces.size());

    cache.face_ptex_offset_buffer = draw_subdiv_build_origindex_buffer(cache.face_ptex_offset,
                                                                       faces.size() + 1);

    build_vertex_face_adjacency_maps(cache);
  }

  cache.resolution = to_mesh_settings.resolution;
  cache.num_coarse_faces = faces.size();

  /* To avoid floating point precision issues when evaluating patches at patch boundaries,
   * ensure that all loops sharing a vertex use the same patch coordinate. This could cause
   * the mesh to not be watertight, leading to shadowing artifacts (see #97877). */
  Vector<int> first_loop_index(cache.num_subdiv_verts, -1);

  /* Save coordinates for corners, as attributes may vary for each loop connected to the same
   * vertex. */
  memcpy(GPU_vertbuf_get_data(cache.corner_patch_coords),
         cache_building_context.patch_coords,
         sizeof(CompressedPatchCoord) * cache.num_subdiv_loops);

  for (int i = 0; i < cache.num_subdiv_loops; i++) {
    const int vertex = cache_building_context.subdiv_loop_subdiv_vert_index[i];
    if (first_loop_index[vertex] != -1) {
      continue;
    }
    first_loop_index[vertex] = i;
  }

  for (int i = 0; i < cache.num_subdiv_loops; i++) {
    const int vertex = cache_building_context.subdiv_loop_subdiv_vert_index[i];
    cache_building_context.patch_coords[i] =
        cache_building_context.patch_coords[first_loop_index[vertex]];
  }

  /* Cleanup. */
  MEM_SAFE_FREE(cache_building_context.vert_origindex_map);
  MEM_SAFE_FREE(cache_building_context.edge_origindex_map);
  MEM_SAFE_FREE(cache_building_context.edge_draw_flag_map);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivUboStorage.
 *
 * Common uniforms for the various shaders.
 * \{ */

struct DRWSubdivUboStorage {
  /* Offsets in the buffers data where the source and destination data start. */
  int src_offset;
  int dst_offset;

  /* Parameters for the DRWPatchMap. */
  int min_patch_face;
  int max_patch_face;
  int max_depth;
  int patches_are_triangular;

  /* Coarse topology information. */
  int coarse_face_count;
  uint edge_loose_offset;

  /* Refined topology information. */
  uint num_subdiv_loops;

  /* The sculpt mask data layer may be null. */
  int has_sculpt_mask;

  /* Masks for the extra coarse face data. */
  uint coarse_face_select_mask;
  uint coarse_face_smooth_mask;
  uint coarse_face_active_mask;
  uint coarse_face_hidden_mask;
  uint coarse_face_loopstart_mask;

  /* Number of elements to process in the compute shader (can be the coarse quad count, or the
   * final vertex count, depending on which compute pass we do). This is used to early out in case
   * of out of bond accesses as compute dispatch are of fixed size. */
  uint total_dispatch_size;

  int is_edit_mode;
  int use_hide;
  int _pad3;
  int _pad4;
};

static_assert((sizeof(DRWSubdivUboStorage) % 16) == 0,
              "DRWSubdivUboStorage is not padded to a multiple of the size of vec4");

static void draw_subdiv_init_ubo_storage(const DRWSubdivCache &cache,
                                         DRWSubdivUboStorage *ubo,
                                         const int src_offset,
                                         const int dst_offset,
                                         const uint total_dispatch_size,
                                         const bool has_sculpt_mask)
{
  ubo->src_offset = src_offset;
  ubo->dst_offset = dst_offset;
  ubo->min_patch_face = cache.gpu_patch_map.min_patch_face;
  ubo->max_patch_face = cache.gpu_patch_map.max_patch_face;
  ubo->max_depth = cache.gpu_patch_map.max_depth;
  ubo->patches_are_triangular = cache.gpu_patch_map.patches_are_triangular;
  ubo->coarse_face_count = cache.num_coarse_faces;
  ubo->num_subdiv_loops = cache.num_subdiv_loops;
  ubo->edge_loose_offset = cache.num_subdiv_loops * 2;
  ubo->has_sculpt_mask = has_sculpt_mask;
  ubo->coarse_face_smooth_mask = SUBDIV_COARSE_FACE_FLAG_SMOOTH_MASK;
  ubo->coarse_face_select_mask = SUBDIV_COARSE_FACE_FLAG_SELECT_MASK;
  ubo->coarse_face_active_mask = SUBDIV_COARSE_FACE_FLAG_ACTIVE_MASK;
  ubo->coarse_face_hidden_mask = SUBDIV_COARSE_FACE_FLAG_HIDDEN_MASK;
  ubo->coarse_face_loopstart_mask = SUBDIV_COARSE_FACE_LOOP_START_MASK;
  ubo->total_dispatch_size = total_dispatch_size;
  ubo->is_edit_mode = cache.is_edit_mode;
  ubo->use_hide = cache.use_hide;
}

static void draw_subdiv_ubo_update_and_bind(const DRWSubdivCache &cache,
                                            GPUShader *shader,
                                            const int src_offset,
                                            const int dst_offset,
                                            const uint total_dispatch_size,
                                            const bool has_sculpt_mask = false)
{
  DRWSubdivUboStorage storage;
  draw_subdiv_init_ubo_storage(
      cache, &storage, src_offset, dst_offset, total_dispatch_size, has_sculpt_mask);

  if (!cache.ubo) {
    const_cast<DRWSubdivCache *>(&cache)->ubo = GPU_uniformbuf_create_ex(
        sizeof(DRWSubdivUboStorage), &storage, "DRWSubdivUboStorage");
  }

  GPU_uniformbuf_update(cache.ubo, &storage);

  const int binding = GPU_shader_get_ubo_binding(shader, "shader_data");
  GPU_uniformbuf_bind(cache.ubo, binding);
}

/** \} */

// --------------------------------------------------------

#define SUBDIV_LOCAL_WORK_GROUP_SIZE 64
static uint get_dispatch_size(uint elements)
{
  return divide_ceil_u(elements, SUBDIV_LOCAL_WORK_GROUP_SIZE);
}

/**
 * Helper to ensure that the UBO is always initialized before dispatching computes and that the
 * same number of elements that need to be processed is used for the UBO and the dispatch size.
 * Use this instead of a raw call to #GPU_compute_dispatch.
 */
static void drw_subdiv_compute_dispatch(const DRWSubdivCache &cache,
                                        GPUShader *shader,
                                        const int src_offset,
                                        const int dst_offset,
                                        uint total_dispatch_size,
                                        const bool has_sculpt_mask = false)
{
  const uint max_res_x = uint(GPU_max_work_group_count(0));

  const uint dispatch_size = get_dispatch_size(total_dispatch_size);
  uint dispatch_rx = dispatch_size;
  uint dispatch_ry = 1u;
  if (dispatch_rx > max_res_x) {
    /* Since there are some limitations with regards to the maximum work group size (could be as
     * low as 64k elements per call), we split the number elements into a "2d" number, with the
     * final index being computed as `res_x + res_y * max_work_group_size`. Even with a maximum
     * work group size of 64k, that still leaves us with roughly `64k * 64k = 4` billion elements
     * total, which should be enough. If not, we could also use the 3rd dimension. */
    /* TODO(fclem): We could dispatch fewer groups if we compute the prime factorization and
     * get the smallest rect fitting the requirements. */
    dispatch_rx = dispatch_ry = ceilf(sqrtf(dispatch_size));
    /* Avoid a completely empty dispatch line caused by rounding. */
    if ((dispatch_rx * (dispatch_ry - 1)) >= dispatch_size) {
      dispatch_ry -= 1;
    }
  }

  /* X and Y dimensions may have different limits so the above computation may not be right, but
   * even with the standard 64k minimum on all dimensions we still have a lot of room. Therefore,
   * we presume it all fits. */
  BLI_assert(dispatch_ry < uint(GPU_max_work_group_count(1)));

  draw_subdiv_ubo_update_and_bind(
      cache, shader, src_offset, dst_offset, total_dispatch_size, has_sculpt_mask);

  GPU_compute_dispatch(shader, dispatch_rx, dispatch_ry, 1);
}

void draw_subdiv_extract_pos_nor(const DRWSubdivCache &cache,
                                 GPUVertBuf *flags_buffer,
                                 GPUVertBuf *pos_nor,
                                 GPUVertBuf *orco)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  Subdiv *subdiv = cache.subdiv;
  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;

  OpenSubdiv_Buffer src_buffer_interface;
  GPUVertBuf *src_buffer = create_buffer_and_interface(&src_buffer_interface,
                                                       get_subdiv_vertex_format());
  evaluator->wrapSrcBuffer(evaluator, &src_buffer_interface);

  GPUVertBuf *src_extra_buffer = nullptr;
  if (orco) {
    OpenSubdiv_Buffer src_extra_buffer_interface;
    src_extra_buffer = create_buffer_and_interface(&src_extra_buffer_interface,
                                                   get_subdiv_vertex_format());
    evaluator->wrapSrcVertexDataBuffer(evaluator, &src_extra_buffer_interface);
  }

  OpenSubdiv_Buffer patch_arrays_buffer_interface;
  GPUVertBuf *patch_arrays_buffer = create_buffer_and_interface(&patch_arrays_buffer_interface,
                                                                get_patch_array_format());
  evaluator->fillPatchArraysBuffer(evaluator, &patch_arrays_buffer_interface);

  OpenSubdiv_Buffer patch_index_buffer_interface;
  GPUVertBuf *patch_index_buffer = create_buffer_and_interface(&patch_index_buffer_interface,
                                                               get_patch_index_format());
  evaluator->wrapPatchIndexBuffer(evaluator, &patch_index_buffer_interface);

  OpenSubdiv_Buffer patch_param_buffer_interface;
  GPUVertBuf *patch_param_buffer = create_buffer_and_interface(&patch_param_buffer_interface,
                                                               get_patch_param_format());
  evaluator->wrapPatchParamBuffer(evaluator, &patch_param_buffer_interface);

  GPUShader *shader = get_patch_evaluation_shader(orco ? SHADER_PATCH_EVALUATION_ORCO :
                                                         SHADER_PATCH_EVALUATION);
  GPU_shader_bind(shader);

  int binding_point = 0;
  GPU_vertbuf_bind_as_ssbo(src_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_handles, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_quadtree, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.patch_coords, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.verts_orig_index, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_arrays_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_index_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_param_buffer, binding_point++);
  if (flags_buffer) {
    GPU_vertbuf_bind_as_ssbo(flags_buffer, binding_point);
  }
  binding_point++;
  GPU_vertbuf_bind_as_ssbo(pos_nor, binding_point++);
  if (orco) {
    GPU_vertbuf_bind_as_ssbo(src_extra_buffer, binding_point++);
    GPU_vertbuf_bind_as_ssbo(orco, binding_point++);
  }
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array.
   * We also need it for subsequent compute shaders, so a barrier on the shader storage is also
   * needed. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();

  GPU_vertbuf_discard(patch_index_buffer);
  GPU_vertbuf_discard(patch_param_buffer);
  GPU_vertbuf_discard(patch_arrays_buffer);
  GPU_vertbuf_discard(src_buffer);
  GPU_VERTBUF_DISCARD_SAFE(src_extra_buffer);
}

void draw_subdiv_extract_uvs(const DRWSubdivCache &cache,
                             GPUVertBuf *uvs,
                             const int face_varying_channel,
                             const int dst_offset)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  Subdiv *subdiv = cache.subdiv;
  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;

  OpenSubdiv_Buffer src_buffer_interface;
  GPUVertBuf *src_buffer = create_buffer_and_interface(&src_buffer_interface, get_uvs_format());
  evaluator->wrapFVarSrcBuffer(evaluator, face_varying_channel, &src_buffer_interface);

  OpenSubdiv_Buffer patch_arrays_buffer_interface;
  GPUVertBuf *patch_arrays_buffer = create_buffer_and_interface(&patch_arrays_buffer_interface,
                                                                get_patch_array_format());
  evaluator->fillFVarPatchArraysBuffer(
      evaluator, face_varying_channel, &patch_arrays_buffer_interface);

  OpenSubdiv_Buffer patch_index_buffer_interface;
  GPUVertBuf *patch_index_buffer = create_buffer_and_interface(&patch_index_buffer_interface,
                                                               get_patch_index_format());
  evaluator->wrapFVarPatchIndexBuffer(
      evaluator, face_varying_channel, &patch_index_buffer_interface);

  OpenSubdiv_Buffer patch_param_buffer_interface;
  GPUVertBuf *patch_param_buffer = create_buffer_and_interface(&patch_param_buffer_interface,
                                                               get_patch_param_format());
  evaluator->wrapFVarPatchParamBuffer(
      evaluator, face_varying_channel, &patch_param_buffer_interface);

  GPUShader *shader = get_patch_evaluation_shader(SHADER_PATCH_EVALUATION_FVAR);
  GPU_shader_bind(shader);

  int binding_point = 0;
  GPU_vertbuf_bind_as_ssbo(src_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_handles, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_quadtree, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.corner_patch_coords, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.verts_orig_index, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_arrays_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_index_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_param_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(uvs, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  /* The buffer offset has the stride baked in (which is 2 as we have UVs) so remove the stride by
   * dividing by 2 */
  const int src_offset = src_buffer_interface.buffer_offset / 2;
  drw_subdiv_compute_dispatch(cache, shader, src_offset, dst_offset, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array.
   * Since it may also be used for computing UV stretches, we also need a barrier on the shader
   * storage. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY | GPU_BARRIER_SHADER_STORAGE);

  /* Cleanup. */
  GPU_shader_unbind();

  GPU_vertbuf_discard(patch_index_buffer);
  GPU_vertbuf_discard(patch_param_buffer);
  GPU_vertbuf_discard(patch_arrays_buffer);
  GPU_vertbuf_discard(src_buffer);
}

void draw_subdiv_interp_custom_data(const DRWSubdivCache &cache,
                                    GPUVertBuf *src_data,
                                    GPUVertBuf *dst_data,
                                    int comp_type, /*GPUVertCompType*/
                                    int dimensions,
                                    int dst_offset)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  GPUShader *shader = get_subdiv_custom_data_shader(comp_type, dimensions);
  GPU_shader_bind(shader);

  int binding_point = 0;
  /* subdiv_face_offset is always at binding point 0 for each shader using it. */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(src_data, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.face_ptex_offset_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.corner_patch_coords, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data, binding_point++);
  GPU_vertbuf_bind_as_ssbo(dst_data, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, dst_offset, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. Put
   * a barrier on the shader storage as we may use the result in another compute shader. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_sculpt_data_buffer(const DRWSubdivCache &cache,
                                          GPUVertBuf *mask_vbo,
                                          GPUVertBuf *face_set_vbo,
                                          GPUVertBuf *sculpt_data)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_SCULPT_DATA);
  GPU_shader_bind(shader);

  /* Mask VBO is always at binding point 0. */
  if (mask_vbo) {
    GPU_vertbuf_bind_as_ssbo(mask_vbo, 0);
  }

  int binding_point = 1;
  GPU_vertbuf_bind_as_ssbo(face_set_vbo, binding_point++);
  GPU_vertbuf_bind_as_ssbo(sculpt_data, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads, mask_vbo != nullptr);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_accumulate_normals(const DRWSubdivCache &cache,
                                    GPUVertBuf *pos_nor,
                                    GPUVertBuf *face_adjacency_offsets,
                                    GPUVertBuf *face_adjacency_lists,
                                    GPUVertBuf *vertex_loop_map,
                                    GPUVertBuf *vert_normals)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_NORMALS_ACCUMULATE);
  GPU_shader_bind(shader);

  int binding_point = 0;

  GPU_vertbuf_bind_as_ssbo(pos_nor, binding_point++);
  GPU_vertbuf_bind_as_ssbo(face_adjacency_offsets, binding_point++);
  GPU_vertbuf_bind_as_ssbo(face_adjacency_lists, binding_point++);
  GPU_vertbuf_bind_as_ssbo(vertex_loop_map, binding_point++);
  GPU_vertbuf_bind_as_ssbo(vert_normals, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_verts);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array.
   * We also need it for subsequent compute shaders, so a barrier on the shader storage is also
   * needed. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_finalize_normals(const DRWSubdivCache &cache,
                                  GPUVertBuf *vert_normals,
                                  GPUVertBuf *subdiv_loop_subdiv_vert_index,
                                  GPUVertBuf *pos_nor)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_NORMALS_FINALIZE);
  GPU_shader_bind(shader);

  int binding_point = 0;
  GPU_vertbuf_bind_as_ssbo(vert_normals, binding_point++);
  GPU_vertbuf_bind_as_ssbo(subdiv_loop_subdiv_vert_index, binding_point++);
  GPU_vertbuf_bind_as_ssbo(pos_nor, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array.
   * We also need it for subsequent compute shaders, so a barrier on the shader storage is also
   * needed. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_finalize_custom_normals(const DRWSubdivCache &cache,
                                         GPUVertBuf *src_custom_normals,
                                         GPUVertBuf *pos_nor)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_CUSTOM_NORMALS_FINALIZE);
  GPU_shader_bind(shader);

  int binding_point = 0;
  GPU_vertbuf_bind_as_ssbo(src_custom_normals, binding_point++);
  /* outputPosNor is bound at index 2 in the base shader. */
  binding_point = 2;
  GPU_vertbuf_bind_as_ssbo(pos_nor, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array.
   * We also need it for subsequent compute shaders, so a barrier on the shader storage is also
   * needed. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_tris_buffer(const DRWSubdivCache &cache,
                                   GPUIndexBuf *subdiv_tris,
                                   const int material_count)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  const bool do_single_material = material_count <= 1;

  GPUShader *shader = get_subdiv_shader(
      do_single_material ? SHADER_BUFFER_TRIS : SHADER_BUFFER_TRIS_MULTIPLE_MATERIALS);
  GPU_shader_bind(shader);

  int binding_point = 0;

  /* subdiv_face_offset is always at binding point 0 for each shader using it. */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data, binding_point++);

  /* Outputs */
  GPU_indexbuf_bind_as_ssbo(subdiv_tris, binding_point++);

  if (!do_single_material) {
    GPU_vertbuf_bind_as_ssbo(cache.face_mat_offset, binding_point++);
  }

  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates an index buffer, so we need to put a barrier on the element array. */
  GPU_memory_barrier(GPU_BARRIER_ELEMENT_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_fdots_buffers(const DRWSubdivCache &cache,
                                     GPUVertBuf *fdots_pos,
                                     GPUVertBuf *fdots_nor,
                                     GPUIndexBuf *fdots_indices)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  Subdiv *subdiv = cache.subdiv;
  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;

  OpenSubdiv_Buffer src_buffer_interface;
  GPUVertBuf *src_buffer = create_buffer_and_interface(&src_buffer_interface,
                                                       get_subdiv_vertex_format());
  evaluator->wrapSrcBuffer(evaluator, &src_buffer_interface);

  OpenSubdiv_Buffer patch_arrays_buffer_interface;
  GPUVertBuf *patch_arrays_buffer = create_buffer_and_interface(&patch_arrays_buffer_interface,
                                                                get_patch_array_format());
  opensubdiv_gpu_buffer_init(&patch_arrays_buffer_interface, patch_arrays_buffer);
  evaluator->fillPatchArraysBuffer(evaluator, &patch_arrays_buffer_interface);

  OpenSubdiv_Buffer patch_index_buffer_interface;
  GPUVertBuf *patch_index_buffer = create_buffer_and_interface(&patch_index_buffer_interface,
                                                               get_patch_index_format());
  evaluator->wrapPatchIndexBuffer(evaluator, &patch_index_buffer_interface);

  OpenSubdiv_Buffer patch_param_buffer_interface;
  GPUVertBuf *patch_param_buffer = create_buffer_and_interface(&patch_param_buffer_interface,
                                                               get_patch_param_format());
  evaluator->wrapPatchParamBuffer(evaluator, &patch_param_buffer_interface);

  GPUShader *shader = get_patch_evaluation_shader(
      fdots_nor ? SHADER_PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS :
                  SHADER_PATCH_EVALUATION_FACE_DOTS);
  GPU_shader_bind(shader);

  int binding_point = 0;
  GPU_vertbuf_bind_as_ssbo(src_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_handles, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_quadtree, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.fdots_patch_coords, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.verts_orig_index, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_arrays_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_index_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(patch_param_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(fdots_pos, binding_point++);
  /* F-dots normals may not be requested, still reserve the binding point. */
  if (fdots_nor) {
    GPU_vertbuf_bind_as_ssbo(fdots_nor, binding_point);
  }
  binding_point++;
  GPU_indexbuf_bind_as_ssbo(fdots_indices, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_coarse_faces);

  /* This generates two vertex buffers and an index buffer, so we need to put a barrier on the
   * vertex attributes and element arrays. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY | GPU_BARRIER_ELEMENT_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();

  GPU_vertbuf_discard(patch_index_buffer);
  GPU_vertbuf_discard(patch_param_buffer);
  GPU_vertbuf_discard(patch_arrays_buffer);
  GPU_vertbuf_discard(src_buffer);
}

void draw_subdiv_build_lines_buffer(const DRWSubdivCache &cache, GPUIndexBuf *lines_indices)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_LINES);
  GPU_shader_bind(shader);

  int binding_point = 0;
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.edges_draw_flag, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data, binding_point++);
  GPU_indexbuf_bind_as_ssbo(lines_indices, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates an index buffer, so we need to put a barrier on the element array. */
  GPU_memory_barrier(GPU_BARRIER_ELEMENT_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_lines_loose_buffer(const DRWSubdivCache &cache,
                                          GPUIndexBuf *lines_indices,
                                          GPUVertBuf *lines_flags,
                                          uint num_loose_edges)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_LINES_LOOSE);
  GPU_shader_bind(shader);

  GPU_indexbuf_bind_as_ssbo(lines_indices, 3);
  GPU_vertbuf_bind_as_ssbo(lines_flags, 4);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, num_loose_edges);

  /* This generates an index buffer, so we need to put a barrier on the element array. */
  GPU_memory_barrier(GPU_BARRIER_ELEMENT_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_edge_fac_buffer(const DRWSubdivCache &cache,
                                       GPUVertBuf *pos_nor,
                                       GPUVertBuf *edge_draw_flag,
                                       GPUVertBuf *poly_other_map,
                                       GPUVertBuf *edge_fac)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_EDGE_FAC);
  GPU_shader_bind(shader);

  int binding_point = 0;
  GPU_vertbuf_bind_as_ssbo(pos_nor, binding_point++);
  GPU_vertbuf_bind_as_ssbo(edge_draw_flag, binding_point++);
  GPU_vertbuf_bind_as_ssbo(poly_other_map, binding_point++);
  GPU_vertbuf_bind_as_ssbo(edge_fac, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_lnor_buffer(const DRWSubdivCache &cache,
                                   GPUVertBuf *pos_nor,
                                   GPUVertBuf *lnor)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_LNOR);
  GPU_shader_bind(shader);

  int binding_point = 0;
  /* Inputs */
  /* subdiv_face_offset is always at binding point 0 for each shader using it. */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(pos_nor, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data, binding_point++);
  GPU_vertbuf_bind_as_ssbo(cache.verts_orig_index, binding_point++);

  /* Outputs */
  GPU_vertbuf_bind_as_ssbo(lnor, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_edituv_stretch_area_buffer(const DRWSubdivCache &cache,
                                                  GPUVertBuf *coarse_data,
                                                  GPUVertBuf *subdiv_data)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_UV_STRETCH_AREA);
  GPU_shader_bind(shader);

  int binding_point = 0;
  /* Inputs */
  /* subdiv_face_offset is always at binding point 0 for each shader using it. */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, binding_point++);
  GPU_vertbuf_bind_as_ssbo(coarse_data, binding_point++);

  /* Outputs */
  GPU_vertbuf_bind_as_ssbo(subdiv_data, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_edituv_stretch_angle_buffer(const DRWSubdivCache &cache,
                                                   GPUVertBuf *pos_nor,
                                                   GPUVertBuf *uvs,
                                                   int uvs_offset,
                                                   GPUVertBuf *stretch_angles)
{
  GPUShader *shader = get_subdiv_shader(SHADER_BUFFER_UV_STRETCH_ANGLE);
  GPU_shader_bind(shader);

  int binding_point = 0;
  /* Inputs */
  GPU_vertbuf_bind_as_ssbo(pos_nor, binding_point++);
  GPU_vertbuf_bind_as_ssbo(uvs, binding_point++);

  /* Outputs */
  GPU_vertbuf_bind_as_ssbo(stretch_angles, binding_point++);
  BLI_assert(binding_point <= MAX_GPU_SUBDIV_SSBOS);

  drw_subdiv_compute_dispatch(cache, shader, uvs_offset, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

/* -------------------------------------------------------------------- */

/**
 * For material assignments we want indices for triangles that share a common material to be laid
 * out contiguously in memory. To achieve this, we sort the indices based on which material the
 * coarse face was assigned. The sort is performed by offsetting the loops indices so that they
 * are directly assigned to the right sorted indices.
 *
 * \code{.unparsed}
 * Here is a visual representation, considering four quads:
 * +---------+---------+---------+---------+
 * | 3     2 | 7     6 | 11   10 | 15   14 |
 * |         |         |         |         |
 * | 0     1 | 4     5 | 8     9 | 12   13 |
 * +---------+---------+---------+---------+
 *
 * If the first and third quads have the same material, we should have:
 * +---------+---------+---------+---------+
 * | 3     2 | 11   10 | 7     6 | 15   14 |
 * |         |         |         |         |
 * | 0     1 | 8     9 | 4     5 | 12   13 |
 * +---------+---------+---------+---------+
 *
 * So the offsets would be:
 * +---------+---------+---------+---------+
 * | 0     0 | 4     4 | -4   -4 | 0     0 |
 * |         |         |         |         |
 * | 0     0 | 4     4 | -4   -4 | 0     0 |
 * +---------+---------+---------+---------+
 * \endcode
 *
 * The offsets are computed not based on the loops indices, but on the number of subdivided
 * polygons for each coarse face. We then only store a single offset for each coarse face,
 * since all sub-faces are contiguous, they all share the same offset.
 */
static void draw_subdiv_cache_ensure_mat_offsets(DRWSubdivCache &cache,
                                                 Mesh *mesh_eval,
                                                 uint mat_len)
{
  draw_subdiv_cache_free_material_data(cache);

  const int number_of_quads = cache.num_subdiv_loops / 4;

  if (mat_len == 1) {
    cache.mat_start = static_cast<int *>(MEM_callocN(sizeof(int), "subdiv mat_end"));
    cache.mat_end = static_cast<int *>(MEM_callocN(sizeof(int), "subdiv mat_end"));
    cache.mat_start[0] = 0;
    cache.mat_end[0] = number_of_quads;
    return;
  }

  const bke::AttributeAccessor attributes = mesh_eval->attributes();
  const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);

  /* Count number of subdivided polygons for each material. */
  int *mat_start = static_cast<int *>(MEM_callocN(sizeof(int) * mat_len, "subdiv mat_start"));
  int *subdiv_face_offset = cache.subdiv_face_offset;

  /* TODO: parallel_reduce? */
  for (int i = 0; i < mesh_eval->faces_num; i++) {
    const int next_offset = (i == mesh_eval->faces_num - 1) ? number_of_quads :
                                                              subdiv_face_offset[i + 1];
    const int quad_count = next_offset - subdiv_face_offset[i];
    const int mat_index = material_indices[i];
    mat_start[mat_index] += quad_count;
  }

  /* Accumulate offsets. */
  int ofs = mat_start[0];
  mat_start[0] = 0;
  for (uint i = 1; i < mat_len; i++) {
    int tmp = mat_start[i];
    mat_start[i] = ofs;
    ofs += tmp;
  }

  /* Compute per face offsets. */
  int *mat_end = static_cast<int *>(MEM_dupallocN(mat_start));
  int *per_face_mat_offset = static_cast<int *>(
      MEM_mallocN(sizeof(int) * mesh_eval->faces_num, "per_face_mat_offset"));

  for (int i = 0; i < mesh_eval->faces_num; i++) {
    const int mat_index = material_indices[i];
    const int single_material_index = subdiv_face_offset[i];
    const int material_offset = mat_end[mat_index];
    const int next_offset = (i == mesh_eval->faces_num - 1) ? number_of_quads :
                                                              subdiv_face_offset[i + 1];
    const int quad_count = next_offset - subdiv_face_offset[i];
    mat_end[mat_index] += quad_count;

    per_face_mat_offset[i] = material_offset - single_material_index;
  }

  cache.face_mat_offset = draw_subdiv_build_origindex_buffer(per_face_mat_offset,
                                                             mesh_eval->faces_num);
  cache.mat_start = mat_start;
  cache.mat_end = mat_end;

  MEM_freeN(per_face_mat_offset);
}

static bool draw_subdiv_create_requested_buffers(Object *ob,
                                                 Mesh *mesh,
                                                 MeshBatchCache &batch_cache,
                                                 MeshBufferCache &mbc,
                                                 const bool is_editmode,
                                                 const bool is_paint_mode,
                                                 const bool is_mode_active,
                                                 const float obmat[4][4],
                                                 const bool do_final,
                                                 const bool do_uvedit,
                                                 const bool do_cage,
                                                 const ToolSettings *ts,
                                                 const bool use_hide,
                                                 OpenSubdiv_EvaluatorCache *evaluator_cache)
{
  SubsurfRuntimeData *runtime_data = mesh->runtime->subsurf_runtime_data;
  BLI_assert(runtime_data && runtime_data->has_gpu_subdiv);

  if (runtime_data->settings.level == 0) {
    return false;
  }

  Mesh *mesh_eval = mesh;
  BMesh *bm = nullptr;
  if (mesh->edit_mesh) {
    mesh_eval = BKE_object_get_editmesh_eval_final(ob);
    bm = mesh->edit_mesh->bm;
  }

  draw_subdiv_invalidate_evaluator_for_orco(runtime_data->subdiv_gpu, mesh_eval);

  Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(runtime_data, mesh_eval, true);
  if (!subdiv) {
    return false;
  }

  if (!BKE_subdiv_eval_begin_from_mesh(
          subdiv, mesh_eval, nullptr, SUBDIV_EVALUATOR_TYPE_GPU, evaluator_cache))
  {
    /* This could happen in two situations:
     * - OpenSubdiv is disabled.
     * - Something totally bad happened, and OpenSubdiv rejected our
     *   topology.
     * In either way, we can't safely continue. However, we still have to handle potential loose
     * geometry, which is done separately. */
    if (mesh_eval->faces_num) {
      return false;
    }
  }

  DRWSubdivCache &draw_cache = mesh_batch_cache_ensure_subdiv_cache(batch_cache);

  draw_cache.optimal_display = runtime_data->use_optimal_display;
  /* If there is no distinct cage, hide unmapped edges that can't be selected. */
  draw_cache.hide_unmapped_edges = is_editmode && !do_cage;
  draw_cache.bm = bm;
  draw_cache.mesh = mesh_eval;
  draw_cache.subdiv = subdiv;

  if (!draw_subdiv_build_cache(draw_cache, subdiv, mesh_eval, runtime_data)) {
    return false;
  }

  draw_cache.num_subdiv_triangles = tris_count_from_number_of_loops(draw_cache.num_subdiv_loops);

  /* Copy topology information for stats display. */
  runtime_data->stats_totvert = draw_cache.num_subdiv_verts;
  runtime_data->stats_totedge = draw_cache.num_subdiv_edges;
  runtime_data->stats_faces_num = draw_cache.num_subdiv_quads;
  runtime_data->stats_totloop = draw_cache.num_subdiv_loops;

  draw_cache.use_custom_loop_normals = (runtime_data->use_loop_normals) &&
                                       mesh_eval->normals_domain() ==
                                           bke::MeshNormalDomain::Corner;

  if (DRW_ibo_requested(mbc.buff.ibo.tris)) {
    draw_subdiv_cache_ensure_mat_offsets(draw_cache, mesh_eval, batch_cache.mat_len);
  }

  MeshRenderData *mr = mesh_render_data_create(
      ob, mesh, is_editmode, is_paint_mode, is_mode_active, obmat, do_final, do_uvedit, ts);
  mr->use_hide = use_hide;
  draw_cache.use_hide = use_hide;

  /* Used for setting loop normals flags. Mapped extraction is only used during edit mode.
   * See comments in #extract_lnor_iter_face_mesh.
   */
  draw_cache.is_edit_mode = mr->edit_bmesh != nullptr;

  draw_subdiv_cache_update_extra_coarse_face_data(draw_cache, mesh_eval, *mr);

  mesh_buffer_cache_create_requested_subdiv(batch_cache, mbc, draw_cache, *mr);

  mesh_render_data_free(mr);

  return true;
}

void DRW_subdivide_loose_geom(DRWSubdivCache *subdiv_cache, MeshBufferCache *cache)
{
  const int coarse_loose_vert_len = cache->loose_geom.verts.size();
  const int coarse_loose_edge_len = cache->loose_geom.edges.size();

  if (coarse_loose_vert_len == 0 && coarse_loose_edge_len == 0) {
    /* Nothing to do. */
    return;
  }

  if (subdiv_cache->loose_geom.edges || subdiv_cache->loose_geom.verts) {
    /* Already processed. */
    return;
  }

  const Mesh *coarse_mesh = subdiv_cache->mesh;
  const bool is_simple = subdiv_cache->subdiv->settings.is_simple;
  const int resolution = subdiv_cache->resolution;
  const int resolution_1 = resolution - 1;
  const float inv_resolution_1 = 1.0f / float(resolution_1);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;

  const int num_subdivided_edge = coarse_loose_edge_len *
                                  (num_subdiv_vertices_per_coarse_edge + 1);

  /* Each edge will store data for its 2 verts, that way we can keep the overall logic simple, here
   * and in the buffer extractors. Although it duplicates memory (and work), the buffers also store
   * duplicate values. */
  const int num_subdivided_verts = num_subdivided_edge * 2;

  DRWSubdivLooseEdge *loose_subd_edges = static_cast<DRWSubdivLooseEdge *>(
      MEM_callocN(sizeof(DRWSubdivLooseEdge) * num_subdivided_edge, "DRWSubdivLooseEdge"));

  DRWSubdivLooseVertex *loose_subd_verts = static_cast<DRWSubdivLooseVertex *>(
      MEM_callocN(sizeof(DRWSubdivLooseVertex) * (num_subdivided_verts + coarse_loose_vert_len),
                  "DRWSubdivLooseEdge"));

  int subd_edge_offset = 0;
  int subd_vert_offset = 0;

  /* Subdivide each loose coarse edge. */
  const Span<float3> coarse_positions = coarse_mesh->vert_positions();
  const Span<int2> coarse_edges = coarse_mesh->edges();

  Array<int> vert_to_edge_offsets;
  Array<int> vert_to_edge_indices;
  const GroupedSpan<int> vert_to_edge_map = bke::mesh::build_vert_to_edge_map(
      coarse_edges, coarse_mesh->verts_num, vert_to_edge_offsets, vert_to_edge_indices);

  for (int i = 0; i < coarse_loose_edge_len; i++) {
    const int coarse_edge_index = cache->loose_geom.edges[i];
    const int2 &coarse_edge = coarse_edges[cache->loose_geom.edges[i]];

    /* Perform interpolation of each vertex. */
    for (int i = 0; i < resolution - 1; i++, subd_edge_offset++) {
      DRWSubdivLooseEdge &subd_edge = loose_subd_edges[subd_edge_offset];
      subd_edge.coarse_edge_index = coarse_edge_index;

      /* First vert. */
      DRWSubdivLooseVertex &subd_v1 = loose_subd_verts[subd_vert_offset];
      subd_v1.coarse_vertex_index = (i == 0) ? coarse_edge[0] : -1u;
      const float u1 = i * inv_resolution_1;
      BKE_subdiv_mesh_interpolate_position_on_edge(
          reinterpret_cast<const float(*)[3]>(coarse_positions.data()),
          coarse_edges.data(),
          vert_to_edge_map,
          coarse_edge_index,
          is_simple,
          u1,
          subd_v1.co);

      subd_edge.loose_subdiv_v1_index = subd_vert_offset++;

      /* Second vert. */
      DRWSubdivLooseVertex &subd_v2 = loose_subd_verts[subd_vert_offset];
      subd_v2.coarse_vertex_index = ((i + 1) == resolution - 1) ? coarse_edge[1] : -1u;
      const float u2 = (i + 1) * inv_resolution_1;
      BKE_subdiv_mesh_interpolate_position_on_edge(
          reinterpret_cast<const float(*)[3]>(coarse_positions.data()),
          coarse_edges.data(),
          vert_to_edge_map,
          coarse_edge_index,
          is_simple,
          u2,
          subd_v2.co);

      subd_edge.loose_subdiv_v2_index = subd_vert_offset++;
    }
  }

  /* Copy the remaining loose_verts. */
  for (int i = 0; i < coarse_loose_vert_len; i++) {
    const int coarse_vertex_index = cache->loose_geom.verts[i];

    DRWSubdivLooseVertex &subd_v = loose_subd_verts[subd_vert_offset++];
    subd_v.coarse_vertex_index = cache->loose_geom.verts[i];
    copy_v3_v3(subd_v.co, coarse_positions[coarse_vertex_index]);
  }

  subdiv_cache->loose_geom.edges = loose_subd_edges;
  subdiv_cache->loose_geom.verts = loose_subd_verts;
  subdiv_cache->loose_geom.edge_len = num_subdivided_edge;
  subdiv_cache->loose_geom.vert_len = coarse_loose_vert_len;
  subdiv_cache->loose_geom.loop_len = num_subdivided_edge * 2 + coarse_loose_vert_len;
}

Span<DRWSubdivLooseEdge> draw_subdiv_cache_get_loose_edges(const DRWSubdivCache &cache)
{
  return {cache.loose_geom.edges, int64_t(cache.loose_geom.edge_len)};
}

Span<DRWSubdivLooseVertex> draw_subdiv_cache_get_loose_verts(const DRWSubdivCache &cache)
{
  return {cache.loose_geom.verts + cache.loose_geom.edge_len * 2,
          int64_t(cache.loose_geom.vert_len)};
}

static OpenSubdiv_EvaluatorCache *g_evaluator_cache = nullptr;

void DRW_create_subdivision(Object *ob,
                            Mesh *mesh,
                            MeshBatchCache &batch_cache,
                            MeshBufferCache *mbc,
                            const bool is_editmode,
                            const bool is_paint_mode,
                            const bool is_mode_active,
                            const float obmat[4][4],
                            const bool do_final,
                            const bool do_uvedit,
                            const bool do_cage,
                            const ToolSettings *ts,
                            const bool use_hide)
{
  if (g_evaluator_cache == nullptr) {
    g_evaluator_cache = openSubdiv_createEvaluatorCache(OPENSUBDIV_EVALUATOR_GPU);
  }

#undef TIME_SUBDIV

#ifdef TIME_SUBDIV
  const double begin_time = BLI_check_seconds_timer();
#endif

  if (!draw_subdiv_create_requested_buffers(ob,
                                            mesh,
                                            batch_cache,
                                            *mbc,
                                            is_editmode,
                                            is_paint_mode,
                                            is_mode_active,
                                            obmat,
                                            do_final,
                                            do_uvedit,
                                            do_cage,
                                            ts,
                                            use_hide,
                                            g_evaluator_cache))
  {
    return;
  }

#ifdef TIME_SUBDIV
  const double end_time = BLI_check_seconds_timer();
  fprintf(stderr, "Time to update subdivision: %f\n", end_time - begin_time);
  fprintf(stderr, "Maximum FPS: %f\n", 1.0 / (end_time - begin_time));
#endif
}

void DRW_subdiv_free()
{
  for (int i = 0; i < NUM_SHADERS; ++i) {
    GPU_shader_free(g_subdiv_shaders[i]);
  }

  DRW_cache_free_old_subdiv();

  if (g_evaluator_cache) {
    openSubdiv_deleteEvaluatorCache(g_evaluator_cache);
    g_evaluator_cache = nullptr;
  }
}

static LinkNode *gpu_subdiv_free_queue = nullptr;
static ThreadMutex gpu_subdiv_queue_mutex = BLI_MUTEX_INITIALIZER;

void DRW_subdiv_cache_free(Subdiv *subdiv)
{
  BLI_mutex_lock(&gpu_subdiv_queue_mutex);
  BLI_linklist_prepend(&gpu_subdiv_free_queue, subdiv);
  BLI_mutex_unlock(&gpu_subdiv_queue_mutex);
}

void DRW_cache_free_old_subdiv()
{
  if (gpu_subdiv_free_queue == nullptr) {
    return;
  }

  BLI_mutex_lock(&gpu_subdiv_queue_mutex);

  while (gpu_subdiv_free_queue != nullptr) {
    Subdiv *subdiv = static_cast<Subdiv *>(BLI_linklist_pop(&gpu_subdiv_free_queue));
    /* Set the type to CPU so that we do actually free the cache. */
    subdiv->evaluator->type = OPENSUBDIV_EVALUATOR_CPU;
    BKE_subdiv_free(subdiv);
  }

  BLI_mutex_unlock(&gpu_subdiv_queue_mutex);
}

}  // namespace blender::draw
