/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/device.h"

#include "integrator/shader_eval.h"

#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader.h"

#include "util/foreach.h"
#include "util/map.h"
#include "util/progress.h"
#include "util/set.h"

CCL_NAMESPACE_BEGIN

static float3 compute_face_normal(const Mesh::Triangle &t, float3 *verts)
{
  float3 v0 = verts[t.v[0]];
  float3 v1 = verts[t.v[1]];
  float3 v2 = verts[t.v[2]];

  float3 norm = cross(v1 - v0, v2 - v0);
  float normlen = len(norm);

  if (normlen == 0.0f) {
    return make_float3(1.0f, 0.0f, 0.0f);
  }

  return norm / normlen;
}

/* Fill in coordinates for mesh displacement shader evaluation on device. */
static int fill_shader_input(const Scene *scene,
                             const Mesh *mesh,
                             const size_t object_index,
                             device_vector<KernelShaderEvalInput> &d_input)
{
  int d_input_size = 0;
  KernelShaderEvalInput *d_input_data = d_input.data();

  const array<int> &mesh_shaders = mesh->get_shader();
  const array<Node *> &mesh_used_shaders = mesh->get_used_shaders();
  const array<float3> &mesh_verts = mesh->get_verts();

  const int num_verts = mesh_verts.size();
  vector<bool> done(num_verts, false);

  int num_triangles = mesh->num_triangles();
  for (int i = 0; i < num_triangles; i++) {
    Mesh::Triangle t = mesh->get_triangle(i);
    int shader_index = mesh_shaders[i];
    Shader *shader = (shader_index < mesh_used_shaders.size()) ?
                         static_cast<Shader *>(mesh_used_shaders[shader_index]) :
                         scene->default_surface;

    if (!shader->has_displacement || shader->get_displacement_method() == DISPLACE_BUMP) {
      continue;
    }

    for (int j = 0; j < 3; j++) {
      if (done[t.v[j]]) {
        continue;
      }

      done[t.v[j]] = true;

      /* set up object, primitive and barycentric coordinates */
      int object = object_index;
      int prim = mesh->prim_offset + i;
      float u, v;

      switch (j) {
        case 0:
          u = 0.0f;
          v = 0.0f;
          break;
        case 1:
          u = 1.0f;
          v = 0.0f;
          break;
        default:
          u = 0.0f;
          v = 1.0f;
          break;
      }

      /* back */
      KernelShaderEvalInput in;
      in.object = object;
      in.prim = prim;
      in.u = u;
      in.v = v;
      d_input_data[d_input_size++] = in;
    }
  }

  return d_input_size;
}

/* Read back mesh displacement shader output. */
static void read_shader_output(const Scene *scene,
                               Mesh *mesh,
                               const device_vector<float> &d_output)
{
  const array<int> &mesh_shaders = mesh->get_shader();
  const array<Node *> &mesh_used_shaders = mesh->get_used_shaders();
  array<float3> &mesh_verts = mesh->get_verts();

  const int num_verts = mesh_verts.size();
  const int num_motion_steps = mesh->get_motion_steps();
  vector<bool> done(num_verts, false);

  const float *d_output_data = d_output.data();
  int d_output_index = 0;

  Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  int num_triangles = mesh->num_triangles();
  for (int i = 0; i < num_triangles; i++) {
    Mesh::Triangle t = mesh->get_triangle(i);
    int shader_index = mesh_shaders[i];
    Shader *shader = (shader_index < mesh_used_shaders.size()) ?
                         static_cast<Shader *>(mesh_used_shaders[shader_index]) :
                         scene->default_surface;

    if (!shader->has_displacement || shader->get_displacement_method() == DISPLACE_BUMP) {
      continue;
    }

    for (int j = 0; j < 3; j++) {
      if (!done[t.v[j]]) {
        done[t.v[j]] = true;
        float3 off = make_float3(d_output_data[d_output_index + 0],
                                 d_output_data[d_output_index + 1],
                                 d_output_data[d_output_index + 2]);
        d_output_index += 3;

        /* Avoid illegal vertex coordinates. */
        off = ensure_finite(off);
        mesh_verts[t.v[j]] += off;
        if (attr_mP != NULL) {
          for (int step = 0; step < num_motion_steps - 1; step++) {
            float3 *mP = attr_mP->data_float3() + step * num_verts;
            mP[t.v[j]] += off;
          }
        }
      }
    }
  }
}

bool GeometryManager::displace(Device *device, Scene *scene, Mesh *mesh, Progress &progress)
{
  /* verify if we have a displacement shader */
  if (!mesh->has_true_displacement()) {
    return false;
  }

  const size_t num_verts = mesh->verts.size();
  const size_t num_triangles = mesh->num_triangles();

  if (num_triangles == 0) {
    return false;
  }

  string msg = string_printf("Computing Displacement %s", mesh->name.c_str());
  progress.set_status("Updating Mesh", msg);

  /* find object index. todo: is arbitrary */
  size_t object_index = OBJECT_NONE;

  for (size_t i = 0; i < scene->objects.size(); i++) {
    if (scene->objects[i]->get_geometry() == mesh) {
      object_index = i;
      break;
    }
  }

  /* Evaluate shader on device. */
  ShaderEval shader_eval(device, progress);
  if (!shader_eval.eval(SHADER_EVAL_DISPLACE,
                        num_verts,
                        3,
                        function_bind(&fill_shader_input, scene, mesh, object_index, _1),
                        function_bind(&read_shader_output, scene, mesh, _1)))
  {
    return false;
  }

  /* stitch */
  unordered_set<int> stitch_keys;
  for (pair<int, int> i : mesh->vert_to_stitching_key_map) {
    stitch_keys.insert(i.second); /* stitching index */
  }

  typedef unordered_multimap<int, int>::iterator map_it_t;

  for (int key : stitch_keys) {
    pair<map_it_t, map_it_t> verts = mesh->vert_stitching_map.equal_range(key);

    float3 pos = zero_float3();
    int num = 0;

    for (map_it_t v = verts.first; v != verts.second; ++v) {
      int vert = v->second;

      pos += mesh->verts[vert];
      num++;
    }

    if (num <= 1) {
      continue;
    }

    pos *= 1.0f / num;

    for (map_it_t v = verts.first; v != verts.second; ++v) {
      mesh->verts[v->second] = pos;
    }
  }

  /* for displacement method both, we only need to recompute the face
   * normals, as bump mapping in the shader will already alter the
   * vertex normal, so we start from the non-displaced vertex normals
   * to avoid applying the perturbation twice. */
  mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
  mesh->add_face_normals();

  bool need_recompute_vertex_normals = false;

  foreach (Node *node, mesh->get_used_shaders()) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->has_displacement && shader->get_displacement_method() == DISPLACE_TRUE) {
      need_recompute_vertex_normals = true;
      break;
    }
  }

  if (need_recompute_vertex_normals) {
    bool flip = mesh->transform_negative_scaled;
    vector<bool> tri_has_true_disp(num_triangles, false);

    for (size_t i = 0; i < num_triangles; i++) {
      int shader_index = mesh->shader[i];
      Shader *shader = (shader_index < mesh->used_shaders.size()) ?
                           static_cast<Shader *>(mesh->used_shaders[shader_index]) :
                           scene->default_surface;

      tri_has_true_disp[i] = shader->has_displacement &&
                             shader->get_displacement_method() == DISPLACE_TRUE;
    }

    /* static vertex normals */

    /* get attributes */
    Attribute *attr_fN = mesh->attributes.find(ATTR_STD_FACE_NORMAL);
    Attribute *attr_vN = mesh->attributes.find(ATTR_STD_VERTEX_NORMAL);

    float3 *fN = attr_fN->data_float3();
    float3 *vN = attr_vN->data_float3();

    /* compute vertex normals */

    /* zero vertex normals on triangles with true displacement */
    for (size_t i = 0; i < num_triangles; i++) {
      if (tri_has_true_disp[i]) {
        for (size_t j = 0; j < 3; j++) {
          vN[mesh->get_triangle(i).v[j]] = zero_float3();
        }
      }
    }

    /* add face normals to vertex normals */
    for (size_t i = 0; i < num_triangles; i++) {
      if (tri_has_true_disp[i]) {
        for (size_t j = 0; j < 3; j++) {
          int vert = mesh->get_triangle(i).v[j];
          vN[vert] += fN[i];

          /* add face normals to stitched vertices */
          if (stitch_keys.size()) {
            map_it_t key = mesh->vert_to_stitching_key_map.find(vert);

            if (key != mesh->vert_to_stitching_key_map.end()) {
              pair<map_it_t, map_it_t> verts = mesh->vert_stitching_map.equal_range(key->second);

              for (map_it_t v = verts.first; v != verts.second; ++v) {
                if (v->second == vert) {
                  continue;
                }

                vN[v->second] += fN[i];
              }
            }
          }
        }
      }
    }

    /* normalize vertex normals */
    vector<bool> done(num_verts, false);

    for (size_t i = 0; i < num_triangles; i++) {
      if (tri_has_true_disp[i]) {
        for (size_t j = 0; j < 3; j++) {
          int vert = mesh->get_triangle(i).v[j];

          if (done[vert]) {
            continue;
          }

          vN[vert] = normalize(vN[vert]);
          if (flip) {
            vN[vert] = -vN[vert];
          }

          done[vert] = true;
        }
      }
    }

    /* motion vertex normals */
    Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    Attribute *attr_mN = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);

    if (mesh->has_motion_blur() && attr_mP && attr_mN) {
      for (int step = 0; step < mesh->motion_steps - 1; step++) {
        float3 *mP = attr_mP->data_float3() + step * mesh->verts.size();
        float3 *mN = attr_mN->data_float3() + step * mesh->verts.size();

        /* compute */

        /* zero vertex normals on triangles with true displacement */
        for (size_t i = 0; i < num_triangles; i++) {
          if (tri_has_true_disp[i]) {
            for (size_t j = 0; j < 3; j++) {
              mN[mesh->get_triangle(i).v[j]] = zero_float3();
            }
          }
        }

        /* add face normals to vertex normals */
        for (size_t i = 0; i < num_triangles; i++) {
          if (tri_has_true_disp[i]) {
            for (size_t j = 0; j < 3; j++) {
              int vert = mesh->get_triangle(i).v[j];
              float3 fN = compute_face_normal(mesh->get_triangle(i), mP);
              mN[vert] += fN;

              /* add face normals to stitched vertices */
              if (stitch_keys.size()) {
                map_it_t key = mesh->vert_to_stitching_key_map.find(vert);

                if (key != mesh->vert_to_stitching_key_map.end()) {
                  pair<map_it_t, map_it_t> verts = mesh->vert_stitching_map.equal_range(
                      key->second);

                  for (map_it_t v = verts.first; v != verts.second; ++v) {
                    if (v->second == vert) {
                      continue;
                    }

                    mN[v->second] += fN;
                  }
                }
              }
            }
          }
        }

        /* normalize vertex normals */
        vector<bool> done(num_verts, false);

        for (size_t i = 0; i < num_triangles; i++) {
          if (tri_has_true_disp[i]) {
            for (size_t j = 0; j < 3; j++) {
              int vert = mesh->get_triangle(i).v[j];

              if (done[vert]) {
                continue;
              }

              mN[vert] = normalize(mN[vert]);
              if (flip) {
                mN[vert] = -mN[vert];
              }

              done[vert] = true;
            }
          }
        }
      }
    }
  }

  return true;
}

CCL_NAMESPACE_END
