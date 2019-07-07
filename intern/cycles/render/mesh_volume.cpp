/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/mesh.h"
#include "render/attribute.h"
#include "render/scene.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_progress.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

static size_t compute_voxel_index(const int3 &resolution, size_t x, size_t y, size_t z)
{
  if (x == -1 || x >= resolution.x) {
    return -1;
  }

  if (y == -1 || y >= resolution.y) {
    return -1;
  }

  if (z == -1 || z >= resolution.z) {
    return -1;
  }

  return x + y * resolution.x + z * resolution.x * resolution.y;
}

struct QuadData {
  int v0, v1, v2, v3;

  float3 normal;
};

enum {
  QUAD_X_MIN = 0,
  QUAD_X_MAX = 1,
  QUAD_Y_MIN = 2,
  QUAD_Y_MAX = 3,
  QUAD_Z_MIN = 4,
  QUAD_Z_MAX = 5,
};

const int quads_indices[6][4] = {
    /* QUAD_X_MIN */
    {4, 0, 3, 7},
    /* QUAD_X_MAX */
    {1, 5, 6, 2},
    /* QUAD_Y_MIN */
    {4, 5, 1, 0},
    /* QUAD_Y_MAX */
    {3, 2, 6, 7},
    /* QUAD_Z_MIN */
    {0, 1, 2, 3},
    /* QUAD_Z_MAX */
    {5, 4, 7, 6},
};

const float3 quads_normals[6] = {
    /* QUAD_X_MIN */
    make_float3(-1.0f, 0.0f, 0.0f),
    /* QUAD_X_MAX */
    make_float3(1.0f, 0.0f, 0.0f),
    /* QUAD_Y_MIN */
    make_float3(0.0f, -1.0f, 0.0f),
    /* QUAD_Y_MAX */
    make_float3(0.0f, 1.0f, 0.0f),
    /* QUAD_Z_MIN */
    make_float3(0.0f, 0.0f, -1.0f),
    /* QUAD_Z_MAX */
    make_float3(0.0f, 0.0f, 1.0f),
};

static int add_vertex(int3 v,
                      vector<int3> &vertices,
                      int3 res,
                      unordered_map<size_t, int> &used_verts)
{
  size_t vert_key = v.x + v.y * (res.x + 1) + v.z * (res.x + 1) * (res.y + 1);
  unordered_map<size_t, int>::iterator it = used_verts.find(vert_key);

  if (it != used_verts.end()) {
    return it->second;
  }

  int vertex_offset = vertices.size();
  used_verts[vert_key] = vertex_offset;
  vertices.push_back(v);
  return vertex_offset;
}

static void create_quad(int3 corners[8],
                        vector<int3> &vertices,
                        vector<QuadData> &quads,
                        int3 res,
                        unordered_map<size_t, int> &used_verts,
                        int face_index)
{
  QuadData quad;
  quad.v0 = add_vertex(corners[quads_indices[face_index][0]], vertices, res, used_verts);
  quad.v1 = add_vertex(corners[quads_indices[face_index][1]], vertices, res, used_verts);
  quad.v2 = add_vertex(corners[quads_indices[face_index][2]], vertices, res, used_verts);
  quad.v3 = add_vertex(corners[quads_indices[face_index][3]], vertices, res, used_verts);
  quad.normal = quads_normals[face_index];

  quads.push_back(quad);
}

struct VolumeParams {
  int3 resolution;
  float3 cell_size;
  float3 start_point;
  int pad_size;
};

static const int CUBE_SIZE = 8;

/* Create a mesh from a volume.
 *
 * The way the algorithm works is as follows:
 *
 * - The coordinates of active voxels from a dense volume (or 3d image) are
 *   gathered inside an auxiliary volume.
 * - Each set of coordinates of an CUBE_SIZE cube are mapped to the same
 *   coordinate of the auxiliary volume.
 * - Quads are created between active and non-active voxels in the auxiliary
 *   volume to generate a tight mesh around the volume.
 */
class VolumeMeshBuilder {
  /* Auxiliary volume that is used to check if a node already added. */
  vector<char> grid;

  /* The resolution of the auxiliary volume, set to be equal to 1/CUBE_SIZE
   * of the original volume on each axis. */
  int3 res;

  size_t number_of_nodes;

  /* Offset due to padding in the original grid. Padding will transform the
   * coordinates of the original grid from 0...res to -padding...res+padding,
   * so some coordinates are negative, and we need to properly account for
   * them. */
  int3 pad_offset;

  VolumeParams *params;

 public:
  VolumeMeshBuilder(VolumeParams *volume_params);

  void add_node(int x, int y, int z);

  void add_node_with_padding(int x, int y, int z);

  void create_mesh(vector<float3> &vertices, vector<int> &indices, vector<float3> &face_normals);

 private:
  void generate_vertices_and_quads(vector<int3> &vertices_is, vector<QuadData> &quads);

  void convert_object_space(const vector<int3> &vertices, vector<float3> &out_vertices);

  void convert_quads_to_tris(const vector<QuadData> &quads,
                             vector<int> &tris,
                             vector<float3> &face_normals);
};

VolumeMeshBuilder::VolumeMeshBuilder(VolumeParams *volume_params)
{
  params = volume_params;
  number_of_nodes = 0;

  const size_t x = divide_up(params->resolution.x, CUBE_SIZE);
  const size_t y = divide_up(params->resolution.y, CUBE_SIZE);
  const size_t z = divide_up(params->resolution.z, CUBE_SIZE);

  /* Adding 2*pad_size since we pad in both positive and negative directions
   * along the axis. */
  const size_t px = divide_up(params->resolution.x + 2 * params->pad_size, CUBE_SIZE);
  const size_t py = divide_up(params->resolution.y + 2 * params->pad_size, CUBE_SIZE);
  const size_t pz = divide_up(params->resolution.z + 2 * params->pad_size, CUBE_SIZE);

  res = make_int3(px, py, pz);
  pad_offset = make_int3(px - x, py - y, pz - z);

  grid.resize(px * py * pz, 0);
}

void VolumeMeshBuilder::add_node(int x, int y, int z)
{
  /* Map coordinates to index space. */
  const int index_x = (x / CUBE_SIZE) + pad_offset.x;
  const int index_y = (y / CUBE_SIZE) + pad_offset.y;
  const int index_z = (z / CUBE_SIZE) + pad_offset.z;

  assert((index_x >= 0) && (index_y >= 0) && (index_z >= 0));

  const size_t index = compute_voxel_index(res, index_x, index_y, index_z);

  /* We already have a node here. */
  if (grid[index] == 1) {
    return;
  }

  ++number_of_nodes;

  grid[index] = 1;
}

void VolumeMeshBuilder::add_node_with_padding(int x, int y, int z)
{
  for (int px = x - params->pad_size; px < x + params->pad_size; ++px) {
    for (int py = y - params->pad_size; py < y + params->pad_size; ++py) {
      for (int pz = z - params->pad_size; pz < z + params->pad_size; ++pz) {
        add_node(px, py, pz);
      }
    }
  }
}

void VolumeMeshBuilder::create_mesh(vector<float3> &vertices,
                                    vector<int> &indices,
                                    vector<float3> &face_normals)
{
  /* We create vertices in index space (is), and only convert them to object
   * space when done. */
  vector<int3> vertices_is;
  vector<QuadData> quads;

  generate_vertices_and_quads(vertices_is, quads);

  convert_object_space(vertices_is, vertices);

  convert_quads_to_tris(quads, indices, face_normals);
}

void VolumeMeshBuilder::generate_vertices_and_quads(vector<ccl::int3> &vertices_is,
                                                    vector<QuadData> &quads)
{
  unordered_map<size_t, int> used_verts;

  for (int z = 0; z < res.z; ++z) {
    for (int y = 0; y < res.y; ++y) {
      for (int x = 0; x < res.x; ++x) {
        size_t voxel_index = compute_voxel_index(res, x, y, z);
        if (grid[voxel_index] == 0) {
          continue;
        }

        /* Compute min and max coords of the node in index space. */
        int3 min = make_int3((x - pad_offset.x) * CUBE_SIZE,
                             (y - pad_offset.y) * CUBE_SIZE,
                             (z - pad_offset.z) * CUBE_SIZE);

        /* Maximum is just CUBE_SIZE voxels away from minimum on each axis. */
        int3 max = make_int3(min.x + CUBE_SIZE, min.y + CUBE_SIZE, min.z + CUBE_SIZE);

        int3 corners[8] = {
            make_int3(min[0], min[1], min[2]),
            make_int3(max[0], min[1], min[2]),
            make_int3(max[0], max[1], min[2]),
            make_int3(min[0], max[1], min[2]),
            make_int3(min[0], min[1], max[2]),
            make_int3(max[0], min[1], max[2]),
            make_int3(max[0], max[1], max[2]),
            make_int3(min[0], max[1], max[2]),
        };

        /* Only create a quad if on the border between an active and
         * an inactive node.
         */

        voxel_index = compute_voxel_index(res, x - 1, y, z);
        if (voxel_index == -1 || grid[voxel_index] == 0) {
          create_quad(corners, vertices_is, quads, res, used_verts, QUAD_X_MIN);
        }

        voxel_index = compute_voxel_index(res, x + 1, y, z);
        if (voxel_index == -1 || grid[voxel_index] == 0) {
          create_quad(corners, vertices_is, quads, res, used_verts, QUAD_X_MAX);
        }

        voxel_index = compute_voxel_index(res, x, y - 1, z);
        if (voxel_index == -1 || grid[voxel_index] == 0) {
          create_quad(corners, vertices_is, quads, res, used_verts, QUAD_Y_MIN);
        }

        voxel_index = compute_voxel_index(res, x, y + 1, z);
        if (voxel_index == -1 || grid[voxel_index] == 0) {
          create_quad(corners, vertices_is, quads, res, used_verts, QUAD_Y_MAX);
        }

        voxel_index = compute_voxel_index(res, x, y, z - 1);
        if (voxel_index == -1 || grid[voxel_index] == 0) {
          create_quad(corners, vertices_is, quads, res, used_verts, QUAD_Z_MIN);
        }

        voxel_index = compute_voxel_index(res, x, y, z + 1);
        if (voxel_index == -1 || grid[voxel_index] == 0) {
          create_quad(corners, vertices_is, quads, res, used_verts, QUAD_Z_MAX);
        }
      }
    }
  }
}

void VolumeMeshBuilder::convert_object_space(const vector<int3> &vertices,
                                             vector<float3> &out_vertices)
{
  out_vertices.reserve(vertices.size());

  for (size_t i = 0; i < vertices.size(); ++i) {
    float3 vertex = make_float3(vertices[i].x, vertices[i].y, vertices[i].z);
    vertex *= params->cell_size;
    vertex += params->start_point;

    out_vertices.push_back(vertex);
  }
}

void VolumeMeshBuilder::convert_quads_to_tris(const vector<QuadData> &quads,
                                              vector<int> &tris,
                                              vector<float3> &face_normals)
{
  int index_offset = 0;
  tris.resize(quads.size() * 6);
  face_normals.reserve(quads.size() * 2);

  for (size_t i = 0; i < quads.size(); ++i) {
    tris[index_offset++] = quads[i].v0;
    tris[index_offset++] = quads[i].v2;
    tris[index_offset++] = quads[i].v1;

    face_normals.push_back(quads[i].normal);

    tris[index_offset++] = quads[i].v0;
    tris[index_offset++] = quads[i].v3;
    tris[index_offset++] = quads[i].v2;

    face_normals.push_back(quads[i].normal);
  }
}

/* ************************************************************************** */

struct VoxelAttributeGrid {
  float *data;
  int channels;
};

void MeshManager::create_volume_mesh(Scene *scene, Mesh *mesh, Progress &progress)
{
  string msg = string_printf("Computing Volume Mesh %s", mesh->name.c_str());
  progress.set_status("Updating Mesh", msg);

  vector<VoxelAttributeGrid> voxel_grids;

  /* Compute volume parameters. */
  VolumeParams volume_params;
  volume_params.resolution = make_int3(0, 0, 0);

  foreach (Attribute &attr, mesh->attributes.attributes) {
    if (attr.element != ATTR_ELEMENT_VOXEL) {
      continue;
    }

    VoxelAttribute *voxel = attr.data_voxel();
    device_memory *image_memory = scene->image_manager->image_memory(voxel->slot);
    int3 resolution = make_int3(
        image_memory->data_width, image_memory->data_height, image_memory->data_depth);

    if (volume_params.resolution == make_int3(0, 0, 0)) {
      volume_params.resolution = resolution;
    }
    else if (volume_params.resolution != resolution) {
      VLOG(1) << "Can't create volume mesh, all voxel grid resolutions must be equal\n";
      return;
    }

    VoxelAttributeGrid voxel_grid;
    voxel_grid.data = static_cast<float *>(image_memory->host_pointer);
    voxel_grid.channels = image_memory->data_elements;
    voxel_grids.push_back(voxel_grid);
  }

  if (voxel_grids.empty()) {
    return;
  }

  /* Compute padding. */
  Shader *volume_shader = NULL;
  int pad_size = 0;

  foreach (Shader *shader, mesh->used_shaders) {
    if (!shader->has_volume) {
      continue;
    }

    volume_shader = shader;

    if (shader->volume_interpolation_method == VOLUME_INTERPOLATION_LINEAR) {
      pad_size = max(1, pad_size);
    }
    else if (shader->volume_interpolation_method == VOLUME_INTERPOLATION_CUBIC) {
      pad_size = max(2, pad_size);
    }

    break;
  }

  if (!volume_shader) {
    return;
  }

  /* Compute start point and cell size from transform. */
  Attribute *attr = mesh->attributes.find(ATTR_STD_GENERATED_TRANSFORM);
  const int3 resolution = volume_params.resolution;
  float3 start_point = make_float3(0.0f, 0.0f, 0.0f);
  float3 cell_size = make_float3(1.0f / resolution.x, 1.0f / resolution.y, 1.0f / resolution.z);

  if (attr) {
    const Transform *tfm = attr->data_transform();
    const Transform itfm = transform_inverse(*tfm);
    start_point = transform_point(&itfm, start_point);
    cell_size = transform_direction(&itfm, cell_size);
  }

  volume_params.start_point = start_point;
  volume_params.cell_size = cell_size;
  volume_params.pad_size = pad_size;

  /* Build bounding mesh around non-empty volume cells. */
  VolumeMeshBuilder builder(&volume_params);
  const float isovalue = mesh->volume_isovalue;

  for (int z = 0; z < resolution.z; ++z) {
    for (int y = 0; y < resolution.y; ++y) {
      for (int x = 0; x < resolution.x; ++x) {
        size_t voxel_index = compute_voxel_index(resolution, x, y, z);

        for (size_t i = 0; i < voxel_grids.size(); ++i) {
          const VoxelAttributeGrid &voxel_grid = voxel_grids[i];
          const int channels = voxel_grid.channels;

          for (int c = 0; c < channels; c++) {
            if (voxel_grid.data[voxel_index * channels + c] >= isovalue) {
              builder.add_node_with_padding(x, y, z);
              break;
            }
          }
        }
      }
    }
  }

  /* Create mesh. */
  vector<float3> vertices;
  vector<int> indices;
  vector<float3> face_normals;
  builder.create_mesh(vertices, indices, face_normals);

  mesh->clear(true);
  mesh->reserve_mesh(vertices.size(), indices.size() / 3);
  mesh->used_shaders.push_back(volume_shader);

  for (size_t i = 0; i < vertices.size(); ++i) {
    mesh->add_vertex(vertices[i]);
  }

  for (size_t i = 0; i < indices.size(); i += 3) {
    mesh->add_triangle(indices[i], indices[i + 1], indices[i + 2], 0, false);
  }

  Attribute *attr_fN = mesh->attributes.add(ATTR_STD_FACE_NORMAL);
  float3 *fN = attr_fN->data_float3();

  for (size_t i = 0; i < face_normals.size(); ++i) {
    fN[i] = face_normals[i];
  }

  /* Print stats. */
  VLOG(1) << "Memory usage volume mesh: "
          << ((vertices.size() + face_normals.size()) * sizeof(float3) +
              indices.size() * sizeof(int)) /
                 (1024.0 * 1024.0)
          << "Mb.";

  VLOG(1) << "Memory usage volume grid: "
          << (resolution.x * resolution.y * resolution.z * sizeof(float)) / (1024.0 * 1024.0)
          << "Mb.";
}

CCL_NAMESPACE_END
