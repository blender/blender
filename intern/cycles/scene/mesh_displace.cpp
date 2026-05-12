/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/device.h"

#include "integrator/shader_eval.h"

#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader.h"

#include "util/progress.h"

CCL_NAMESPACE_BEGIN

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

  const int num_triangles = mesh->num_triangles();
  for (int i = 0; i < num_triangles; i++) {
    const Mesh::Triangle t = mesh->get_triangle(i);
    const int shader_index = mesh_shaders[i];
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
      const int object = object_index;
      const int prim = mesh->prim_offset + i;
      float u;
      float v;

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
  const array<float3> &mesh_verts = mesh->get_verts();

  const int num_verts = mesh_verts.size();
  const int num_motion_steps = mesh->get_motion_steps();
  vector<bool> done(num_verts, false);

  const float *d_output_data = d_output.data();
  int d_output_index = 0;

  Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  const int num_triangles = mesh->num_triangles();
  for (int i = 0; i < num_triangles; i++) {
    const Mesh::Triangle t = mesh->get_triangle(i);
    const int shader_index = mesh_shaders[i];
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
        if (attr_mP != nullptr) {
          for (int step = 0; step < num_motion_steps - 1; step++) {
            float3 *mP = attr_mP->data_float3_for_write() + step * num_verts;
            mP[t.v[j]] += off;
          }
        }
      }
    }
  }
}

/* Compute unnormalized vertex normals by accumulating face normals from the
 * specified triangles. Vertices not touched by any included triangle are left
 * at zero. */
static void compute_vertex_normals(const Mesh *mesh,
                                   const float3 *verts_data,
                                   const vector<bool> &tri_recompute,
                                   vector<float3> &vN)
{
  const size_t num_triangles = mesh->num_triangles();

  for (size_t i = 0; i < num_triangles; i++) {
    if (tri_recompute[i]) {
      const Mesh::Triangle triangle = mesh->get_triangle(i);
      for (size_t j = 0; j < 3; j++) {
        vN[triangle.v[j]] = zero_float3();
      }
    }
  }

  for (size_t i = 0; i < num_triangles; i++) {
    if (tri_recompute[i]) {
      const Mesh::Triangle triangle = mesh->get_triangle(i);
      const float3 fN = triangle.compute_normal(verts_data);
      for (size_t j = 0; j < 3; j++) {
        vN[triangle.v[j]] += fN;
      }
    }
  }
}

/* Store normalized vertex normals into a packed_normal attribute, applying
 * flip for negative-scaled transforms. Only vertices of included triangles
 * are written. */
static void store_vertex_normals(const Mesh *mesh,
                                 const vector<float3> &vN_float,
                                 const vector<bool> &tri_recompute,
                                 const bool flip,
                                 packed_normal *vN)
{
  const size_t num_verts = mesh->get_verts().size();
  vector<bool> done(num_verts, false);

  for (size_t i = 0; i < mesh->num_triangles(); i++) {
    if (tri_recompute[i]) {
      const Mesh::Triangle triangle = mesh->get_triangle(i);
      for (size_t j = 0; j < 3; j++) {
        const int vert = triangle.v[j];
        if (done[vert]) {
          continue;
        }

        float3 N = safe_normalize(vN_float[vert]);
        if (flip) {
          N = -N;
        }
        vN[vert] = packed_normal(N);
        done[vert] = true;
      }
    }
  }
}

/* Apply vertex normal delta from displacement to a set of corner normals.
 * For flat shaded triangles, use the new face normal directly. */
static void apply_corner_normal_delta(const Mesh *mesh,
                                      const float3 *verts_data,
                                      const vector<float3> &post_vN,
                                      const float3 *pre_vN,
                                      const vector<bool> &tri_recompute,
                                      const bool flip,
                                      packed_normal *cN)
{
  const bool *smooth = mesh->get_smooth().data();

  for (size_t i = 0; i < mesh->num_triangles(); i++) {
    if (!tri_recompute[i]) {
      continue;
    }
    const Mesh::Triangle triangle = mesh->get_triangle(i);
    if (smooth && smooth[i]) {
      for (size_t j = 0; j < 3; j++) {
        const int vert = triangle.v[j];
        float3 post = safe_normalize(post_vN[vert]);
        if (flip) {
          post = -post;
        }
        const float3 delta = post - pre_vN[vert];
        cN[i * 3 + j] = packed_normal(safe_normalize(cN[i * 3 + j].decode() + delta));
      }
    }
    else {
      float3 post_fN = triangle.compute_normal(verts_data);
      if (flip) {
        post_fN = -post_fN;
      }
      for (size_t j = 0; j < 3; j++) {
        cN[i * 3 + j] = packed_normal(post_fN);
      }
    }
  }
}

/* Save pre-displacement vertex normals so we can compute the delta after
 * displacement and apply it to corner normals. Also saves per motion step. */
static void save_pre_displacement_normals(const Mesh *mesh,
                                          array<float3> &pre_displace_vN,
                                          vector<array<float3>> &pre_displace_motion_vN)
{
  const size_t num_verts = mesh->get_verts().size();
  const size_t num_triangles = mesh->num_triangles();
  const bool flip = mesh->transform_negative_scaled;
  const vector<bool> all_tris(num_triangles, true);

  auto compute_normals = [&](const float3 *verts_data) {
    array<float3> result;
    result.resize(num_verts, zero_float3());
    vector<float3> vN(num_verts, zero_float3());
    compute_vertex_normals(mesh, verts_data, all_tris, vN);
    for (size_t i = 0; i < num_verts; i++) {
      float3 N = safe_normalize(vN[i]);
      if (flip) {
        N = -N;
      }
      result[i] = N;
    }
    return result;
  };

  pre_displace_vN = compute_normals(mesh->get_verts().data());

  const Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  const Attribute *attr_mcN = mesh->attributes.find(ATTR_STD_MOTION_CORNER_NORMAL);
  if (mesh->has_motion_blur() && attr_mP && attr_mcN) {
    const int num_steps = mesh->get_motion_steps() - 1;
    pre_displace_motion_vN.resize(num_steps);
    for (int step = 0; step < num_steps; step++) {
      const float3 *mP = attr_mP->data_float3() + step * num_verts;
      pre_displace_motion_vN[step] = compute_normals(mP);
    }
  }
}

/* Update corner normals after displacement, including motion blur steps. */
static void recompute_displaced_corner_normals(Mesh *mesh,
                                               const vector<float3> &vN_float,
                                               const array<float3> &pre_displace_vN,
                                               const vector<array<float3>> &pre_displace_motion_vN,
                                               const vector<bool> &tri_recompute,
                                               const bool flip)
{
  /* Static corner normals. */
  Attribute *attr_cN = mesh->attributes.find(ATTR_STD_CORNER_NORMAL);
  apply_corner_normal_delta(mesh,
                            mesh->get_verts().data(),
                            vN_float,
                            pre_displace_vN.data(),
                            tri_recompute,
                            flip,
                            attr_cN->data_normal_for_write());

  /* Motion corner normals. */
  Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  Attribute *attr_mcN = mesh->attributes.find(ATTR_STD_MOTION_CORNER_NORMAL);

  if (mesh->has_motion_blur() && attr_mP && attr_mcN) {
    const size_t num_verts = mesh->get_verts().size();
    const size_t num_corners = mesh->num_triangles() * 3;

    for (int step = 0; step < mesh->get_motion_steps() - 1; step++) {
      const float3 *mP = attr_mP->data_float3() + step * num_verts;
      packed_normal *mcN = attr_mcN->data_normal_for_write() + step * num_corners;

      vector<float3> mN_float(num_verts, zero_float3());
      compute_vertex_normals(mesh, mP, tri_recompute, mN_float);

      apply_corner_normal_delta(
          mesh, mP, mN_float, pre_displace_motion_vN[step].data(), tri_recompute, flip, mcN);
    }
  }
}

/* Update vertex normals after displacement, including motion blur steps. */
static void recompute_displaced_vertex_normals(Mesh *mesh,
                                               const vector<float3> &vN_float,
                                               const vector<bool> &tri_recompute,
                                               const bool flip)
{
  const size_t num_verts = mesh->get_verts().size();

  /* Static vertex normals. */
  Attribute *attr_vN = mesh->attributes.find(ATTR_STD_VERTEX_NORMAL);
  store_vertex_normals(mesh, vN_float, tri_recompute, flip, attr_vN->data_normal_for_write());

  /* Motion vertex normals. */
  Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  Attribute *attr_mN = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);

  if (mesh->has_motion_blur() && attr_mP && attr_mN) {
    for (int step = 0; step < mesh->get_motion_steps() - 1; step++) {
      const float3 *mP = attr_mP->data_float3() + step * num_verts;
      packed_normal *mN = attr_mN->data_normal_for_write() + step * num_verts;

      vector<float3> mN_float(num_verts, zero_float3());
      compute_vertex_normals(mesh, mP, tri_recompute, mN_float);
      store_vertex_normals(mesh, mN_float, tri_recompute, flip, mN);
    }
  }
}

bool GeometryManager::displace(Device *device, Scene *scene, Mesh *mesh, Progress &progress)
{
  /* verify if we have a displacement shader */
  if (!mesh->has_true_displacement()) {
    return false;
  }

  const size_t num_verts = mesh->get_verts().size();
  const size_t num_triangles = mesh->num_triangles();

  if (num_triangles == 0) {
    return false;
  }

  /* Corner normals for sharp edges and faces should be preserved, but we can not
   * individually displace corners as the mesh would break apart. Instead we
   * compute the delta between vertex normals before and after displacement and
   * apply the delta to corner normals. */
  bool need_recompute_vertex_normals = false;
  bool need_recompute_all_vertex_normals = false;

  const bool has_corner_normals = mesh->attributes.find(ATTR_STD_CORNER_NORMAL) != nullptr;
  array<float3> pre_displace_vN;
  vector<array<float3>> pre_displace_motion_vN;

  if (has_corner_normals) {
    need_recompute_vertex_normals = true;
    need_recompute_all_vertex_normals = true;
    save_pre_displacement_normals(mesh, pre_displace_vN, pre_displace_motion_vN);
  }

  /* Add undisplaced attributes right before doing displacement. */
  mesh->add_undisplaced(scene);

  const string msg = string_printf("Computing Displacement %s", mesh->name.c_str());
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
  if (!shader_eval.eval(
          SHADER_EVAL_DISPLACE,
          num_verts,
          3,
          [scene, mesh, object_index](device_vector<KernelShaderEvalInput> &d_input) {
            return fill_shader_input(scene, mesh, object_index, d_input);
          },
          [scene, mesh](const device_vector<float> &d_output) {
            read_shader_output(scene, mesh, d_output);
          }))
  {
    return false;
  }

  /* For displacement method both, we don't need to recompute the vertex normals
   * as bump mapping in the shader will already alter the vertex normal, so we start
   * from the non-displaced vertex normals to avoid applying the perturbation twice. */
  for (Node *node : mesh->get_used_shaders()) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->has_displacement && shader->get_displacement_method() == DISPLACE_TRUE) {
      need_recompute_vertex_normals = true;
      break;
    }
  }

  if (need_recompute_vertex_normals) {
    const bool flip = mesh->transform_negative_scaled;
    vector<bool> tri_recompute(num_triangles, need_recompute_all_vertex_normals);

    if (!need_recompute_all_vertex_normals) {
      for (size_t i = 0; i < num_triangles; i++) {
        const int shader_index = mesh->shader[i];
        Shader *shader = (shader_index < mesh->used_shaders.size()) ?
                             static_cast<Shader *>(mesh->used_shaders[shader_index]) :
                             scene->default_surface;

        tri_recompute[i] = shader->has_displacement &&
                           shader->get_displacement_method() == DISPLACE_TRUE;
      }
    }

    vector<float3> vN_float(num_verts, zero_float3());
    compute_vertex_normals(mesh, mesh->get_verts().data(), tri_recompute, vN_float);

    if (has_corner_normals) {
      recompute_displaced_corner_normals(
          mesh, vN_float, pre_displace_vN, pre_displace_motion_vN, tri_recompute, flip);
    }
    else {
      recompute_displaced_vertex_normals(mesh, vN_float, tri_recompute, flip);
    }
  }

  mesh->update_tangents(scene, false);

  return true;
}

CCL_NAMESPACE_END
