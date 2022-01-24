
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

/* Uniform block for #DRWSubivUboStorage. */
layout(std140) uniform shader_data
{
  /* Offsets in the buffers data where the source and destination data start. */
  int src_offset;
  int dst_offset;

  /* Parameters for the DRWPatchMap. */
  int min_patch_face;
  int max_patch_face;
  int max_depth;
  int patches_are_triangular;

  /* Coarse topology information. */
  int coarse_poly_count;
  uint edge_loose_offset;

  /* Subdiv topology information. */
  uint num_subdiv_loops;

  /* Subdivision settings. */
  bool optimal_display;

  /* Sculpt data. */
  bool has_sculpt_mask;

  /* Masks for the extra coarse face data. */
  uint coarse_face_select_mask;
  uint coarse_face_smooth_mask;
  uint coarse_face_active_mask;
  uint coarse_face_loopstart_mask;

  /* Total number of elements to process. */
  uint total_dispatch_size;
};

uint get_global_invocation_index()
{
  uint invocations_per_row = gl_WorkGroupSize.x * gl_NumWorkGroups.x;
  return gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * invocations_per_row;
}

/* Structure for #CompressedPatchCoord. */
struct BlenderPatchCoord {
  int patch_index;
  uint encoded_uv;
};

vec2 decode_uv(uint encoded_uv)
{
  float u = float((encoded_uv >> 16) & 0xFFFFu) / 65535.0;
  float v = float(encoded_uv & 0xFFFFu) / 65535.0;
  return vec2(u, v);
}

/* This structure is a carbon copy of OpenSubDiv's PatchTable::PatchHandle. */
struct PatchHandle {
  int array_index;
  int patch_index;
  int vertex_index;
};

/* This structure is a carbon copy of OpenSubDiv's PatchCoord. */
struct PatchCoord {
  int array_index;
  int patch_index;
  int vertex_index;
  float u;
  float v;
};

/* This structure is a carbon copy of OpenSubDiv's PatchCoord.QuadNode.
 * Each child is a bitfield. */
struct QuadNode {
  uvec4 child;
};

bool is_set(uint i)
{
  /* QuadNode.Child.isSet is the first bit of the bitfield. */
  return (i & 0x1u) != 0;
}

bool is_leaf(uint i)
{
  /* QuadNode.Child.isLeaf is the second bit of the bitfield. */
  return (i & 0x2u) != 0;
}

uint get_index(uint i)
{
  /* QuadNode.Child.index is made of the remaining bits. */
  return (i >> 2) & 0x3FFFFFFFu;
}

/* Duplicate of #PosNorLoop from the mesh extract CPU code.
 * We do not use a vec3 for the position as it will be padded to a vec4 which is incompatible with
 * the format. */
struct PosNorLoop {
  float x, y, z;
  /* TODO(kevindietrich) : figure how to compress properly as GLSL does not have char/short types,
   * bit operations get tricky. */
  float nx, ny, nz;
  float flag;
};

vec3 get_vertex_pos(PosNorLoop vertex_data)
{
  return vec3(vertex_data.x, vertex_data.y, vertex_data.z);
}

vec3 get_vertex_nor(PosNorLoop vertex_data)
{
  return vec3(vertex_data.nx, vertex_data.ny, vertex_data.nz);
}

void set_vertex_pos(inout PosNorLoop vertex_data, vec3 pos)
{
  vertex_data.x = pos.x;
  vertex_data.y = pos.y;
  vertex_data.z = pos.z;
}

void set_vertex_nor(inout PosNorLoop vertex_data, vec3 nor, uint flag)
{
  vertex_data.nx = nor.x;
  vertex_data.ny = nor.y;
  vertex_data.nz = nor.z;
  vertex_data.flag = float(flag);
}

/* Set the vertex normal but preserve the existing flag. This is for when we compute manually the
 * vertex normals when we cannot use the limit surface, in which case the flag and the normal are
 * set by two separate compute pass. */
void set_vertex_nor(inout PosNorLoop vertex_data, vec3 nor)
{
  set_vertex_nor(vertex_data, nor, 0);
}

#define ORIGINDEX_NONE -1

#ifdef SUBDIV_POLYGON_OFFSET
layout(std430, binding = 0) readonly buffer inputSubdivPolygonOffset
{
  uint subdiv_polygon_offset[];
};

/* Given the index of the subdivision quad, return the index of the corresponding coarse polygon.
 * This uses subdiv_polygon_offset and since it is a growing list of offsets, we can use binary
 * search to locate the right index. */
uint coarse_polygon_index_from_subdiv_quad_index(uint subdiv_quad_index, uint coarse_poly_count)
{
  uint first = 0;
  uint last = coarse_poly_count;

  while (first != last) {
    uint middle = (first + last) / 2;

    if (subdiv_polygon_offset[middle] < subdiv_quad_index) {
      first = middle + 1;
    }
    else {
      last = middle;
    }
  }

  if (subdiv_polygon_offset[first] == subdiv_quad_index) {
    return first;
  }

  return first - 1;
}
#endif
