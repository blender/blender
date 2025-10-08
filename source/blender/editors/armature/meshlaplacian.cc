/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * Algorithms using the mesh laplacian.
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_ordered_edge.hh"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_attribute.hh"
#include "BKE_bvhutils.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"

#include "ED_armature.hh"
#include "ED_mesh.hh"
#include "ED_object_vgroup.hh"

#include "eigen_capi.h"

#include "meshlaplacian.h"

#include <algorithm>

/* ************* XXX *************** */
static void waitcursor(int /*val*/) {}
static void progress_bar(int /*dummy_val*/, const char * /*dummy*/) {}
static void start_progress_bar() {}
static void end_progress_bar() {}
static void error(const char *str)
{
  printf("error: %s\n", str);
}
/* ************* XXX *************** */

/************************** Laplacian System *****************************/

struct LaplacianSystem {
  LinearSolver *context; /* linear solver */

  int verts_num, faces_num;

  float **verts;        /* vertex coordinates */
  float *varea;         /* vertex weights for laplacian computation */
  char *vpinned;        /* vertex pinning */
  int (*faces)[3];      /* face vertex indices */
  float (*fweights)[3]; /* cotangent weights per face */

  int areaweights;    /* use area in cotangent weights? */
  int storeweights;   /* store cotangent weights in fweights */
  bool variablesdone; /* variables set in linear system */

  blender::Map<blender::OrderedEdge, int> edgehash; /* edge hash for construction */

  struct HeatWeighting {
    const blender::int3 *corner_tris;
    blender::Span<int> corner_verts; /* needed to find vertices by index */
    int verts_num;
    int tris_num;
    float (*verts)[3];        /* vertex coordinates */
    float (*vert_normals)[3]; /* vertex normals */

    float (*root)[3]; /* bone root */
    float (*tip)[3];  /* bone tip */
    int numsource;

    float *H;       /* diagonal H matrix */
    float *p;       /* values from all p vectors */
    float *mindist; /* minimum distance to a bone for all vertices */

    BVHTree *bvhtree;             /* ray tracing acceleration structure */
    const blender::int3 **vltree; /* a corner_tri that the vertex belongs to */
  } heat;
};

/* Laplacian matrix construction */

/* Computation of these weights for the laplacian is based on:
 * "Discrete Differential-Geometry Operators for Triangulated 2-Manifolds",
 * Meyer et al, 2002. Section 3.5, formula (8).
 *
 * We do it a bit different by going over faces instead of going over each
 * vertex and adjacent faces, since we don't store this adjacency. Also, the
 * formulas are tweaked a bit to work for non-manifold meshes. */

static void laplacian_increase_edge_count(blender::Map<blender::OrderedEdge, int> &edgehash,
                                          int v1,
                                          int v2)
{
  edgehash.add_or_modify({v1, v2}, [](int *value) { *value = 1; }, [](int *value) { (*value)++; });
}

static int laplacian_edge_count(const blender::Map<blender::OrderedEdge, int> &edgehash,
                                int v1,
                                int v2)
{
  return edgehash.lookup({v1, v2});
}

static void laplacian_triangle_area(LaplacianSystem *sys, int i1, int i2, int i3)
{
  float t1, t2, t3, len1, len2, len3, area;
  float *varea = sys->varea, *v1, *v2, *v3;
  int obtuse = 0;

  v1 = sys->verts[i1];
  v2 = sys->verts[i2];
  v3 = sys->verts[i3];

  t1 = cotangent_tri_weight_v3(v1, v2, v3);
  t2 = cotangent_tri_weight_v3(v2, v3, v1);
  t3 = cotangent_tri_weight_v3(v3, v1, v2);

  if (angle_v3v3v3(v2, v1, v3) > DEG2RADF(90.0f)) {
    obtuse = 1;
  }
  else if (angle_v3v3v3(v1, v2, v3) > DEG2RADF(90.0f)) {
    obtuse = 2;
  }
  else if (angle_v3v3v3(v1, v3, v2) > DEG2RADF(90.0f)) {
    obtuse = 3;
  }

  if (obtuse > 0) {
    area = area_tri_v3(v1, v2, v3);

    varea[i1] += (obtuse == 1) ? area : area * 0.5f;
    varea[i2] += (obtuse == 2) ? area : area * 0.5f;
    varea[i3] += (obtuse == 3) ? area : area * 0.5f;
  }
  else {
    len1 = len_v3v3(v2, v3);
    len2 = len_v3v3(v1, v3);
    len3 = len_v3v3(v1, v2);

    t1 *= len1 * len1;
    t2 *= len2 * len2;
    t3 *= len3 * len3;

    varea[i1] += (t2 + t3) * 0.25f;
    varea[i2] += (t1 + t3) * 0.25f;
    varea[i3] += (t1 + t2) * 0.25f;
  }
}

static void laplacian_triangle_weights(LaplacianSystem *sys, int f, int i1, int i2, int i3)
{
  float t1, t2, t3;
  float *varea = sys->varea, *v1, *v2, *v3;

  v1 = sys->verts[i1];
  v2 = sys->verts[i2];
  v3 = sys->verts[i3];

  /* instead of *0.5 we divided by the number of faces of the edge, it still
   * needs to be verified that this is indeed the correct thing to do! */
  t1 = cotangent_tri_weight_v3(v1, v2, v3) / laplacian_edge_count(sys->edgehash, i2, i3);
  t2 = cotangent_tri_weight_v3(v2, v3, v1) / laplacian_edge_count(sys->edgehash, i3, i1);
  t3 = cotangent_tri_weight_v3(v3, v1, v2) / laplacian_edge_count(sys->edgehash, i1, i2);

  EIG_linear_solver_matrix_add(sys->context, i1, i1, (t2 + t3) * varea[i1]);
  EIG_linear_solver_matrix_add(sys->context, i2, i2, (t1 + t3) * varea[i2]);
  EIG_linear_solver_matrix_add(sys->context, i3, i3, (t1 + t2) * varea[i3]);

  EIG_linear_solver_matrix_add(sys->context, i1, i2, -t3 * varea[i1]);
  EIG_linear_solver_matrix_add(sys->context, i2, i1, -t3 * varea[i2]);

  EIG_linear_solver_matrix_add(sys->context, i2, i3, -t1 * varea[i2]);
  EIG_linear_solver_matrix_add(sys->context, i3, i2, -t1 * varea[i3]);

  EIG_linear_solver_matrix_add(sys->context, i3, i1, -t2 * varea[i3]);
  EIG_linear_solver_matrix_add(sys->context, i1, i3, -t2 * varea[i1]);

  if (sys->storeweights) {
    sys->fweights[f][0] = t1 * varea[i1];
    sys->fweights[f][1] = t2 * varea[i2];
    sys->fweights[f][2] = t3 * varea[i3];
  }
}

static LaplacianSystem *laplacian_system_construct_begin(int verts_num, int faces_num, int lsq)
{
  LaplacianSystem *sys;

  sys = MEM_new<LaplacianSystem>(__func__);

  sys->verts = MEM_calloc_arrayN<float *>(verts_num, "LaplacianSystemVerts");
  sys->vpinned = MEM_calloc_arrayN<char>(verts_num, "LaplacianSystemVpinned");
  sys->faces = static_cast<int (*)[3]>(
      MEM_callocN(sizeof(int[3]) * faces_num, "LaplacianSystemFaces"));

  sys->verts_num = 0;
  sys->faces_num = 0;

  sys->areaweights = 1;
  sys->storeweights = 0;

  /* create linear solver */
  if (lsq) {
    sys->context = EIG_linear_least_squares_solver_new(0, verts_num, 1);
  }
  else {
    sys->context = EIG_linear_solver_new(0, verts_num, 1);
  }

  return sys;
}

void laplacian_add_vertex(LaplacianSystem *sys, float *co, int pinned)
{
  sys->verts[sys->verts_num] = co;
  sys->vpinned[sys->verts_num] = pinned;
  sys->verts_num++;
}

void laplacian_add_triangle(LaplacianSystem *sys, int v1, int v2, int v3)
{
  sys->faces[sys->faces_num][0] = v1;
  sys->faces[sys->faces_num][1] = v2;
  sys->faces[sys->faces_num][2] = v3;
  sys->faces_num++;
}

static void laplacian_system_construct_end(LaplacianSystem *sys)
{
  int (*face)[3];
  int a, verts_num = sys->verts_num, faces_num = sys->faces_num;

  laplacian_begin_solve(sys, 0);

  sys->varea = MEM_calloc_arrayN<float>(verts_num, "LaplacianSystemVarea");

  sys->edgehash.reserve(sys->faces_num);
  for (a = 0, face = sys->faces; a < sys->faces_num; a++, face++) {
    laplacian_increase_edge_count(sys->edgehash, (*face)[0], (*face)[1]);
    laplacian_increase_edge_count(sys->edgehash, (*face)[1], (*face)[2]);
    laplacian_increase_edge_count(sys->edgehash, (*face)[2], (*face)[0]);
  }

  if (sys->areaweights) {
    for (a = 0, face = sys->faces; a < sys->faces_num; a++, face++) {
      laplacian_triangle_area(sys, (*face)[0], (*face)[1], (*face)[2]);
    }
  }

  for (a = 0; a < verts_num; a++) {
    if (sys->areaweights) {
      if (sys->varea[a] != 0.0f) {
        sys->varea[a] = 0.5f / sys->varea[a];
      }
    }
    else {
      sys->varea[a] = 1.0f;
    }

    /* for heat weighting */
    if (sys->heat.H) {
      EIG_linear_solver_matrix_add(sys->context, a, a, sys->heat.H[a]);
    }
  }

  if (sys->storeweights) {
    sys->fweights = static_cast<float (*)[3]>(
        MEM_callocN(sizeof(float[3]) * faces_num, "LaplacianFWeight"));
  }

  for (a = 0, face = sys->faces; a < faces_num; a++, face++) {
    laplacian_triangle_weights(sys, a, (*face)[0], (*face)[1], (*face)[2]);
  }

  MEM_freeN(sys->faces);
  sys->faces = nullptr;

  MEM_SAFE_FREE(sys->varea);
}

static void laplacian_system_delete(LaplacianSystem *sys)
{
  if (sys->verts) {
    MEM_freeN(sys->verts);
  }
  if (sys->varea) {
    MEM_freeN(sys->varea);
  }
  if (sys->vpinned) {
    MEM_freeN(sys->vpinned);
  }
  if (sys->faces) {
    MEM_freeN(sys->faces);
  }
  if (sys->fweights) {
    MEM_freeN(sys->fweights);
  }

  EIG_linear_solver_delete(sys->context);
  MEM_delete(sys);
}

void laplacian_begin_solve(LaplacianSystem *sys, int index)
{
  int a;

  if (!sys->variablesdone) {
    if (index >= 0) {
      for (a = 0; a < sys->verts_num; a++) {
        if (sys->vpinned[a]) {
          EIG_linear_solver_variable_set(sys->context, 0, a, sys->verts[a][index]);
          EIG_linear_solver_variable_lock(sys->context, a);
        }
      }
    }

    sys->variablesdone = true;
  }
}

void laplacian_add_right_hand_side(LaplacianSystem *sys, int v, float value)
{
  EIG_linear_solver_right_hand_side_add(sys->context, 0, v, value);
}

int laplacian_system_solve(LaplacianSystem *sys)
{
  sys->variablesdone = false;

  // EIG_linear_solver_print_matrix(sys->context, );

  return EIG_linear_solver_solve(sys->context);
}

float laplacian_system_get_solution(LaplacianSystem *sys, int v)
{
  return EIG_linear_solver_variable_get(sys->context, 0, v);
}

/************************* Heat Bone Weighting ******************************/
/* From "Automatic Rigging and Animation of 3D Characters"
 * Ilya Baran and Jovan Popovic, SIGGRAPH 2007 */

#define C_WEIGHT 1.0f
#define WEIGHT_LIMIT_START 0.05f
#define WEIGHT_LIMIT_END 0.025f
#define DISTANCE_EPSILON 1e-4f

struct BVHCallbackUserData {
  float start[3];
  float vec[3];
  LaplacianSystem *sys;
};

static void bvh_callback(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
  BVHCallbackUserData *data = static_cast<BVHCallbackUserData *>(userdata);
  const blender::int3 &tri = data->sys->heat.corner_tris[index];
  const blender::Span<int> corner_verts = data->sys->heat.corner_verts;
  float (*verts)[3] = data->sys->heat.verts;
  const float *vtri_co[3];
  float dist_test;

  vtri_co[0] = verts[corner_verts[tri[0]]];
  vtri_co[1] = verts[corner_verts[tri[1]]];
  vtri_co[2] = verts[corner_verts[tri[2]]];

#ifdef USE_KDOPBVH_WATERTIGHT
  if (isect_ray_tri_watertight_v3(
          data->start, ray->isect_precalc, UNPACK3(vtri_co), &dist_test, nullptr))
#else
  UNUSED_VARS(ray);
  if (isect_ray_tri_v3(data->start, data->vec, UNPACK3(vtri_co), &dist_test, nullptr))
#endif
  {
    if (dist_test < hit->dist) {
      float n[3];
      normal_tri_v3(n, UNPACK3(vtri_co));
      if (dot_v3v3(n, data->vec) < -1e-5f) {
        hit->index = index;
        hit->dist = dist_test;
      }
    }
  }
}

/* Ray-tracing for vertex to bone/vertex visibility. */
static void heat_ray_tree_create(LaplacianSystem *sys)
{
  const blender::int3 *corner_tris = sys->heat.corner_tris;
  const blender::Span<int> corner_verts = sys->heat.corner_verts;
  float (*verts)[3] = sys->heat.verts;
  int tris_num = sys->heat.tris_num;
  int verts_num = sys->heat.verts_num;
  int a;

  sys->heat.bvhtree = BLI_bvhtree_new(tris_num, 0.0f, 4, 6);
  sys->heat.vltree = static_cast<const blender::int3 **>(
      MEM_callocN(sizeof(blender::int3 *) * verts_num, "HeatVFaces"));

  for (a = 0; a < tris_num; a++) {
    const blender::int3 &tri = corner_tris[a];
    float bb[6];
    int vtri[3];

    vtri[0] = corner_verts[tri[0]];
    vtri[1] = corner_verts[tri[1]];
    vtri[2] = corner_verts[tri[2]];

    INIT_MINMAX(bb, bb + 3);
    minmax_v3v3_v3(bb, bb + 3, verts[vtri[0]]);
    minmax_v3v3_v3(bb, bb + 3, verts[vtri[1]]);
    minmax_v3v3_v3(bb, bb + 3, verts[vtri[2]]);

    BLI_bvhtree_insert(sys->heat.bvhtree, a, bb, 2);

    /* Setup inverse pointers to use on isect.orig */
    sys->heat.vltree[vtri[0]] = &tri;
    sys->heat.vltree[vtri[1]] = &tri;
    sys->heat.vltree[vtri[2]] = &tri;
  }

  BLI_bvhtree_balance(sys->heat.bvhtree);
}

static int heat_ray_source_visible(LaplacianSystem *sys, int vertex, int source)
{
  BVHTreeRayHit hit;
  BVHCallbackUserData data;
  const blender::int3 *lt;
  float end[3];
  int visible;

  lt = sys->heat.vltree[vertex];
  if (lt == nullptr) {
    return 1;
  }

  data.sys = sys;
  copy_v3_v3(data.start, sys->heat.verts[vertex]);

  closest_to_line_segment_v3(end, data.start, sys->heat.root[source], sys->heat.tip[source]);

  sub_v3_v3v3(data.vec, end, data.start);
  madd_v3_v3v3fl(data.start, data.start, data.vec, 1e-5);
  mul_v3_fl(data.vec, 1.0f - 2e-5f);

  /* pass normalized vec + distance to bvh */
  hit.index = -1;
  hit.dist = normalize_v3(data.vec);

  visible =
      BLI_bvhtree_ray_cast(
          sys->heat.bvhtree, data.start, data.vec, 0.0f, &hit, bvh_callback, (void *)&data) == -1;

  return visible;
}

static float heat_source_distance(LaplacianSystem *sys, int vertex, int source)
{
  float closest[3], d[3], dist, cosine;

  /* compute Euclidean distance */
  closest_to_line_segment_v3(
      closest, sys->heat.verts[vertex], sys->heat.root[source], sys->heat.tip[source]);

  sub_v3_v3v3(d, sys->heat.verts[vertex], closest);
  dist = normalize_v3(d);

  /* if the vertex normal does not point along the bone, increase distance */
  cosine = dot_v3v3(d, sys->heat.vert_normals[vertex]);

  return dist / (0.5f * (cosine + 1.001f));
}

static int heat_source_closest(LaplacianSystem *sys, int vertex, int source)
{
  float dist;

  dist = heat_source_distance(sys, vertex, source);

  if (dist <= sys->heat.mindist[vertex] * (1.0f + DISTANCE_EPSILON)) {
    if (heat_ray_source_visible(sys, vertex, source)) {
      return 1;
    }
  }

  return 0;
}

static void heat_set_H(LaplacianSystem *sys, int vertex)
{
  float dist, mindist, h;
  int j, numclosest = 0;

  mindist = 1e10;

  /* compute minimum distance */
  for (j = 0; j < sys->heat.numsource; j++) {
    dist = heat_source_distance(sys, vertex, j);

    mindist = std::min(dist, mindist);
  }

  sys->heat.mindist[vertex] = mindist;

  /* count number of sources with approximately this minimum distance */
  for (j = 0; j < sys->heat.numsource; j++) {
    if (heat_source_closest(sys, vertex, j)) {
      numclosest++;
    }
  }

  sys->heat.p[vertex] = (numclosest > 0) ? 1.0f / numclosest : 0.0f;

  /* compute H entry */
  if (numclosest > 0) {
    mindist = max_ff(mindist, 1e-4f);
    h = numclosest * C_WEIGHT / (mindist * mindist);
  }
  else {
    h = 0.0f;
  }

  sys->heat.H[vertex] = h;
}

static void heat_calc_vnormals(LaplacianSystem *sys)
{
  float fnor[3];
  int a, v1, v2, v3, (*face)[3];

  sys->heat.vert_normals = static_cast<float (*)[3]>(
      MEM_callocN(sizeof(float[3]) * sys->verts_num, "HeatVNors"));

  for (a = 0, face = sys->faces; a < sys->faces_num; a++, face++) {
    v1 = (*face)[0];
    v2 = (*face)[1];
    v3 = (*face)[2];

    normal_tri_v3(fnor, sys->verts[v1], sys->verts[v2], sys->verts[v3]);

    add_v3_v3(sys->heat.vert_normals[v1], fnor);
    add_v3_v3(sys->heat.vert_normals[v2], fnor);
    add_v3_v3(sys->heat.vert_normals[v3], fnor);
  }

  for (a = 0; a < sys->verts_num; a++) {
    normalize_v3(sys->heat.vert_normals[a]);
  }
}

static void heat_laplacian_create(LaplacianSystem *sys)
{
  const blender::int3 *corner_tris = sys->heat.corner_tris;
  const blender::Span<int> corner_verts = sys->heat.corner_verts;
  int tris_num = sys->heat.tris_num;
  int verts_num = sys->heat.verts_num;
  int a;

  /* heat specific definitions */
  sys->heat.mindist = MEM_calloc_arrayN<float>(verts_num, "HeatMinDist");
  sys->heat.H = MEM_calloc_arrayN<float>(verts_num, "HeatH");
  sys->heat.p = MEM_calloc_arrayN<float>(verts_num, "HeatP");

  /* add verts and faces to laplacian */
  for (a = 0; a < verts_num; a++) {
    laplacian_add_vertex(sys, sys->heat.verts[a], 0);
  }

  for (a = 0; a < tris_num; a++) {
    int vtri[3];
    vtri[0] = corner_verts[corner_tris[a][0]];
    vtri[1] = corner_verts[corner_tris[a][1]];
    vtri[2] = corner_verts[corner_tris[a][2]];
    laplacian_add_triangle(sys, UNPACK3(vtri));
  }

  /* for distance computation in set_H */
  heat_calc_vnormals(sys);

  for (a = 0; a < verts_num; a++) {
    heat_set_H(sys, a);
  }
}

static void heat_system_free(LaplacianSystem *sys)
{
  BLI_bvhtree_free(sys->heat.bvhtree);
  MEM_freeN(sys->heat.vltree);
  MEM_freeN(sys->heat.corner_tris);

  MEM_freeN(sys->heat.mindist);
  MEM_freeN(sys->heat.H);
  MEM_freeN(sys->heat.p);
  MEM_freeN(sys->heat.vert_normals);
}

static float heat_limit_weight(float weight)
{
  float t;

  if (weight < WEIGHT_LIMIT_END) {
    return 0.0f;
  }
  if (weight < WEIGHT_LIMIT_START) {
    t = (weight - WEIGHT_LIMIT_END) / (WEIGHT_LIMIT_START - WEIGHT_LIMIT_END);
    return t * WEIGHT_LIMIT_START;
  }
  return weight;
}

void heat_bone_weighting(Object *ob,
                         Mesh *mesh,
                         float (*verts)[3],
                         int numbones,
                         bDeformGroup **dgrouplist,
                         bDeformGroup **dgroupflip,
                         float (*root)[3],
                         float (*tip)[3],
                         const bool *selected,
                         const char **r_error_str)
{
  using namespace blender;
  LaplacianSystem *sys;
  blender::int3 *corner_tris;
  float solution, weight;
  int *vertsflipped = nullptr, *mask = nullptr;
  int a, tris_num, j, bbone, firstsegment, lastsegment;
  bool use_topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  const blender::Span<blender::float3> vert_positions = mesh->vert_positions();
  const blender::OffsetIndices faces = mesh->faces();
  const blender::Span<int> corner_verts = mesh->corner_verts();
  const bke::AttributeAccessor attributes = mesh->attributes();
  bool use_vert_sel = (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
  bool use_face_sel = (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  *r_error_str = nullptr;

  /* bone heat needs triangulated faces */
  tris_num = poly_to_tri_count(mesh->faces_num, mesh->corners_num);

  /* count triangles and create mask */
  if (ob->mode & OB_MODE_WEIGHT_PAINT && (use_face_sel || use_vert_sel)) {
    mask = MEM_calloc_arrayN<int>(mesh->verts_num, "heat_bone_weighting mask");

    /*  (added selectedVerts content for vertex mask, they used to just equal 1) */
    if (use_vert_sel) {
      const VArray select_vert = *attributes.lookup_or_default<bool>(
          ".select_vert", bke::AttrDomain::Point, false);
      if (select_vert) {
        for (const int i : faces.index_range()) {
          for (const int vert : corner_verts.slice(faces[i])) {
            mask[vert] = select_vert[vert];
          }
        }
      }
    }
    else if (use_face_sel) {
      const VArray select_poly = *attributes.lookup_or_default<bool>(
          ".select_poly", bke::AttrDomain::Face, false);
      if (select_poly) {
        for (const int i : faces.index_range()) {
          if (select_poly[i]) {
            for (const int vert : corner_verts.slice(faces[i])) {
              mask[vert] = 1;
            }
          }
        }
      }
    }
  }

  /* create laplacian */
  sys = laplacian_system_construct_begin(mesh->verts_num, tris_num, 1);

  sys->heat.tris_num = poly_to_tri_count(mesh->faces_num, mesh->corners_num);
  corner_tris = static_cast<blender::int3 *>(
      MEM_mallocN(sizeof(*sys->heat.corner_tris) * sys->heat.tris_num, __func__));

  blender::bke::mesh::corner_tris_calc(
      vert_positions, faces, corner_verts, {corner_tris, sys->heat.tris_num});

  sys->heat.corner_tris = corner_tris;
  sys->heat.corner_verts = corner_verts;
  sys->heat.verts_num = mesh->verts_num;
  sys->heat.verts = verts;
  sys->heat.root = root;
  sys->heat.tip = tip;
  sys->heat.numsource = numbones;

  heat_ray_tree_create(sys);
  heat_laplacian_create(sys);

  laplacian_system_construct_end(sys);

  if (dgroupflip) {
    vertsflipped = MEM_calloc_arrayN<int>(mesh->verts_num, "vertsflipped");
    for (a = 0; a < mesh->verts_num; a++) {
      vertsflipped[a] = mesh_get_x_mirror_vert(ob, nullptr, a, use_topology);
    }
  }

  /* compute weights per bone */
  for (j = 0; j < numbones; j++) {
    if (selected[j] == false) {
      continue;
    }

    firstsegment = (j == 0 || dgrouplist[j - 1] != dgrouplist[j]);
    lastsegment = (j == numbones - 1 || dgrouplist[j] != dgrouplist[j + 1]);
    bbone = !(firstsegment && lastsegment);

    /* clear weights */
    if (bbone && firstsegment) {
      for (a = 0; a < mesh->verts_num; a++) {
        if (mask && !mask[a]) {
          continue;
        }

        blender::ed::object::vgroup_vert_remove(ob, dgrouplist[j], a);
        if (vertsflipped && dgroupflip[j] && vertsflipped[a] >= 0) {
          blender::ed::object::vgroup_vert_remove(ob, dgroupflip[j], vertsflipped[a]);
        }
      }
    }

    /* fill right hand side */
    laplacian_begin_solve(sys, -1);

    for (a = 0; a < mesh->verts_num; a++) {
      if (heat_source_closest(sys, a, j)) {
        laplacian_add_right_hand_side(sys, a, sys->heat.H[a] * sys->heat.p[a]);
      }
    }

    /* solve */
    if (laplacian_system_solve(sys)) {
      /* load solution into vertex groups */
      for (a = 0; a < mesh->verts_num; a++) {
        if (mask && !mask[a]) {
          continue;
        }

        solution = laplacian_system_get_solution(sys, a);

        if (bbone) {
          if (solution > 0.0f) {
            blender::ed::object::vgroup_vert_add(ob, dgrouplist[j], a, solution, WEIGHT_ADD);
          }
        }
        else {
          weight = heat_limit_weight(solution);
          if (weight > 0.0f) {
            blender::ed::object::vgroup_vert_add(ob, dgrouplist[j], a, weight, WEIGHT_REPLACE);
          }
          else {
            blender::ed::object::vgroup_vert_remove(ob, dgrouplist[j], a);
          }
        }

        /* do same for mirror */
        if (vertsflipped && dgroupflip[j] && vertsflipped[a] >= 0) {
          if (bbone) {
            if (solution > 0.0f) {
              blender::ed::object::vgroup_vert_add(
                  ob, dgroupflip[j], vertsflipped[a], solution, WEIGHT_ADD);
            }
          }
          else {
            weight = heat_limit_weight(solution);
            if (weight > 0.0f) {
              blender::ed::object::vgroup_vert_add(
                  ob, dgroupflip[j], vertsflipped[a], weight, WEIGHT_REPLACE);
            }
            else {
              blender::ed::object::vgroup_vert_remove(ob, dgroupflip[j], vertsflipped[a]);
            }
          }
        }
      }
    }
    else if (*r_error_str == nullptr) {
      *r_error_str = N_("Bone Heat Weighting: failed to find solution for one or more bones");
      break;
    }

    /* remove too small vertex weights */
    if (bbone && lastsegment) {
      for (a = 0; a < mesh->verts_num; a++) {
        if (mask && !mask[a]) {
          continue;
        }

        weight = blender::ed::object::vgroup_vert_weight(ob, dgrouplist[j], a);
        weight = heat_limit_weight(weight);
        if (weight <= 0.0f) {
          blender::ed::object::vgroup_vert_remove(ob, dgrouplist[j], a);
        }

        if (vertsflipped && dgroupflip[j] && vertsflipped[a] >= 0) {
          weight = blender::ed::object::vgroup_vert_weight(ob, dgroupflip[j], vertsflipped[a]);
          weight = heat_limit_weight(weight);
          if (weight <= 0.0f) {
            blender::ed::object::vgroup_vert_remove(ob, dgroupflip[j], vertsflipped[a]);
          }
        }
      }
    }
  }

  /* free */
  if (vertsflipped) {
    MEM_freeN(vertsflipped);
  }
  if (mask) {
    MEM_freeN(mask);
  }

  heat_system_free(sys);

  laplacian_system_delete(sys);
}

/************************** Harmonic Coordinates ****************************/
/* From "Harmonic Coordinates for Character Articulation",
 * Pushkar Joshi, Mark Meyer, Tony DeRose, Brian Green and Tom Sanocki,
 * SIGGRAPH 2007. */

#define EPSILON 0.0001f

#define MESHDEFORM_TAG_UNTYPED 0
#define MESHDEFORM_TAG_BOUNDARY 1
#define MESHDEFORM_TAG_INTERIOR 2
#define MESHDEFORM_TAG_EXTERIOR 3

/** minimum length for #MDefBoundIsect.len */
#define MESHDEFORM_LEN_THRESHOLD 1e-6f

#define MESHDEFORM_MIN_INFLUENCE 0.0005f

static const int MESHDEFORM_OFFSET[7][3] = {
    {0, 0, 0},
    {1, 0, 0},
    {-1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
    {0, 0, 1},
    {0, 0, -1},
};

struct MDefBoundIsect {
  /* intersection on the cage 'cagecos' */
  float co[3];
  /* non-facing intersections are considered interior */
  bool facing;
  /* ray-cast index aligned with polygons (ray-hit-triangle isn't needed) */
  int face_index;
  /* distance from 'co' to the ray-cast start (clamped to avoid zero division) */
  float len;
  /* weights aligned with the polygons's loop indices */
  float poly_weights[0];
};

struct MDefBindInfluence {
  MDefBindInfluence *next;
  float weight;
  int vertex;
};

struct MeshDeformBind {
  /* grid dimensions */
  float min[3], max[3];
  float width[3], halfwidth[3];
  int size, size3;

  /* meshes */
  Mesh *cagemesh;
  float (*cagecos)[3];
  float (*vertexcos)[3];
  int verts_num, cage_verts_num;

  /* grids */
  MemArena *memarena;
  MDefBoundIsect *(*boundisect)[6];
  int *semibound;
  int *tag;
  float *phi, *totalphi;

  /* mesh stuff */
  int *inside;
  float *weights;
  MDefBindInfluence **dyngrid;
  float cagemat[4][4];

  /* direct solver */
  int *varidx;

  const BVHTree *bvhtree;
  blender::bke::BVHTreeFromMesh bvhdata;

  /* avoid DM function calls during intersections */
  struct {
    blender::OffsetIndices<int> faces;
    blender::Span<int> corner_verts;
    blender::Span<blender::int3> corner_tris;
    blender::Span<int> tri_faces;
    blender::Span<blender::float3> face_normals;
  } cagemesh_cache;
};

struct MeshDeformIsect {
  float start[3];
  float vec[3];
  float vec_length;
  float lambda;

  bool isect;
  float u, v;
};

/* ray intersection */

struct MeshRayCallbackData {
  MeshDeformBind *mdb;
  MeshDeformIsect *isec;
};

static void harmonic_ray_callback(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit *hit)
{
  MeshRayCallbackData *data = static_cast<MeshRayCallbackData *>(userdata);
  MeshDeformBind *mdb = data->mdb;
  const blender::Span<int> corner_verts = mdb->cagemesh_cache.corner_verts;
  const blender::Span<int> tri_faces = mdb->cagemesh_cache.tri_faces;
  const blender::Span<blender::float3> face_normals = mdb->cagemesh_cache.face_normals;
  MeshDeformIsect *isec = data->isec;
  float no[3], co[3], dist;
  float *face[3];

  const blender::int3 &tri = mdb->cagemesh_cache.corner_tris[index];

  face[0] = mdb->cagecos[corner_verts[tri[0]]];
  face[1] = mdb->cagecos[corner_verts[tri[1]]];
  face[2] = mdb->cagecos[corner_verts[tri[2]]];

  bool isect_ray_tri = isect_ray_tri_watertight_v3(
      ray->origin, ray->isect_precalc, UNPACK3(face), &dist, nullptr);

  if (!isect_ray_tri || dist > isec->vec_length) {
    return;
  }

  if (!face_normals.is_empty()) {
    copy_v3_v3(no, face_normals[tri_faces[index]]);
  }
  else {
    normal_tri_v3(no, UNPACK3(face));
  }

  madd_v3_v3v3fl(co, ray->origin, ray->direction, dist);
  dist /= isec->vec_length;
  if (dist < hit->dist) {
    hit->index = index;
    hit->dist = dist;
    copy_v3_v3(hit->co, co);

    isec->isect = (dot_v3v3(no, ray->direction) <= 0.0f);
    isec->lambda = dist;
  }
}

static MDefBoundIsect *meshdeform_ray_tree_intersect(MeshDeformBind *mdb,
                                                     const float co1[3],
                                                     const float co2[3])
{
  BVHTreeRayHit hit;
  MeshDeformIsect isect_mdef;
  MeshRayCallbackData data = {
      mdb,
      &isect_mdef,
  };
  float end[3], vec_normal[3];

  /* happens binding when a cage has no faces */
  if (UNLIKELY(mdb->bvhtree == nullptr)) {
    return nullptr;
  }

  /* setup isec */
  memset(&isect_mdef, 0, sizeof(isect_mdef));
  isect_mdef.lambda = 1e10f;

  copy_v3_v3(isect_mdef.start, co1);
  copy_v3_v3(end, co2);
  sub_v3_v3v3(isect_mdef.vec, end, isect_mdef.start);
  isect_mdef.vec_length = normalize_v3_v3(vec_normal, isect_mdef.vec);

  hit.index = -1;
  hit.dist = BVH_RAYCAST_DIST_MAX;
  if (BLI_bvhtree_ray_cast_ex(mdb->bvhtree,
                              isect_mdef.start,
                              vec_normal,
                              0.0,
                              &hit,
                              harmonic_ray_callback,
                              &data,
                              BVH_RAYCAST_WATERTIGHT) != -1)
  {
    const blender::Span<int> corner_verts = mdb->cagemesh_cache.corner_verts;
    const int face_i = mdb->cagemesh_cache.tri_faces[hit.index];
    const blender::IndexRange face = mdb->cagemesh_cache.faces[face_i];
    const float (*cagecos)[3] = mdb->cagecos;
    const float len = isect_mdef.lambda;
    MDefBoundIsect *isect;

    blender::Array<blender::float3, 64> mp_cagecos(face.size());

    /* create MDefBoundIsect, and extra for 'poly_weights[]' */
    isect = static_cast<MDefBoundIsect *>(
        BLI_memarena_alloc(mdb->memarena, sizeof(*isect) + (sizeof(float) * face.size())));

    /* compute intersection coordinate */
    madd_v3_v3v3fl(isect->co, co1, isect_mdef.vec, len);

    isect->facing = isect_mdef.isect;

    isect->face_index = face_i;

    isect->len = max_ff(len_v3v3(co1, isect->co), MESHDEFORM_LEN_THRESHOLD);

    /* compute mean value coordinates for interpolation */
    for (int i = 0; i < face.size(); i++) {
      copy_v3_v3(mp_cagecos[i], cagecos[corner_verts[face[i]]]);
    }

    interp_weights_poly_v3(isect->poly_weights,
                           reinterpret_cast<float (*)[3]>(mp_cagecos.data()),
                           face.size(),
                           isect->co);

    return isect;
  }

  return nullptr;
}

static int meshdeform_inside_cage(MeshDeformBind *mdb, float *co)
{
  MDefBoundIsect *isect;
  float outside[3], start[3], dir[3];
  int i;

  for (i = 1; i <= 6; i++) {
    outside[0] = co[0] + (mdb->max[0] - mdb->min[0] + 1.0f) * MESHDEFORM_OFFSET[i][0];
    outside[1] = co[1] + (mdb->max[1] - mdb->min[1] + 1.0f) * MESHDEFORM_OFFSET[i][1];
    outside[2] = co[2] + (mdb->max[2] - mdb->min[2] + 1.0f) * MESHDEFORM_OFFSET[i][2];

    copy_v3_v3(start, co);
    sub_v3_v3v3(dir, outside, start);
    normalize_v3(dir);

    isect = meshdeform_ray_tree_intersect(mdb, start, outside);
    if (isect && !isect->facing) {
      return 1;
    }
  }

  return 0;
}

/* solving */

BLI_INLINE int meshdeform_index(MeshDeformBind *mdb, int x, int y, int z, int n)
{
  int size = mdb->size;

  x += MESHDEFORM_OFFSET[n][0];
  y += MESHDEFORM_OFFSET[n][1];
  z += MESHDEFORM_OFFSET[n][2];

  if (x < 0 || x >= mdb->size) {
    return -1;
  }
  if (y < 0 || y >= mdb->size) {
    return -1;
  }
  if (z < 0 || z >= mdb->size) {
    return -1;
  }

  return x + y * size + z * size * size;
}

BLI_INLINE void meshdeform_cell_center(
    MeshDeformBind *mdb, int x, int y, int z, int n, float *center)
{
  x += MESHDEFORM_OFFSET[n][0];
  y += MESHDEFORM_OFFSET[n][1];
  z += MESHDEFORM_OFFSET[n][2];

  center[0] = mdb->min[0] + x * mdb->width[0] + mdb->halfwidth[0];
  center[1] = mdb->min[1] + y * mdb->width[1] + mdb->halfwidth[1];
  center[2] = mdb->min[2] + z * mdb->width[2] + mdb->halfwidth[2];
}

static void meshdeform_add_intersections(MeshDeformBind *mdb, int x, int y, int z)
{
  MDefBoundIsect *isect;
  float center[3], ncenter[3];
  int i, a;

  a = meshdeform_index(mdb, x, y, z, 0);
  meshdeform_cell_center(mdb, x, y, z, 0, center);

  /* check each outgoing edge for intersection */
  for (i = 1; i <= 6; i++) {
    if (meshdeform_index(mdb, x, y, z, i) == -1) {
      continue;
    }

    meshdeform_cell_center(mdb, x, y, z, i, ncenter);

    isect = meshdeform_ray_tree_intersect(mdb, center, ncenter);
    if (isect) {
      mdb->boundisect[a][i - 1] = isect;
      mdb->tag[a] = MESHDEFORM_TAG_BOUNDARY;
    }
  }
}

static void meshdeform_bind_floodfill(MeshDeformBind *mdb)
{
  int *stack, *tag = mdb->tag;
  int a, b, i, xyz[3], stacksize, size = mdb->size;

  stack = MEM_calloc_arrayN<int>(mdb->size3, __func__);

  /* we know lower left corner is EXTERIOR because of padding */
  tag[0] = MESHDEFORM_TAG_EXTERIOR;
  stack[0] = 0;
  stacksize = 1;

  /* floodfill exterior tag */
  while (stacksize > 0) {
    a = stack[--stacksize];

    xyz[2] = a / (size * size);
    xyz[1] = (a - xyz[2] * size * size) / size;
    xyz[0] = a - xyz[1] * size - xyz[2] * size * size;

    for (i = 1; i <= 6; i++) {
      b = meshdeform_index(mdb, xyz[0], xyz[1], xyz[2], i);

      if (b != -1) {
        if (tag[b] == MESHDEFORM_TAG_UNTYPED ||
            (tag[b] == MESHDEFORM_TAG_BOUNDARY && !mdb->boundisect[a][i - 1]))
        {
          tag[b] = MESHDEFORM_TAG_EXTERIOR;
          stack[stacksize++] = b;
        }
      }
    }
  }

  /* other cells are interior */
  for (a = 0; a < size * size * size; a++) {
    if (tag[a] == MESHDEFORM_TAG_UNTYPED) {
      tag[a] = MESHDEFORM_TAG_INTERIOR;
    }
  }

#if 0
  {
    int tb, ti, te, ts;
    tb = ti = te = ts = 0;
    for (a = 0; a < size * size * size; a++) {
      if (tag[a] == MESHDEFORM_TAG_BOUNDARY) {
        tb++;
      }
      else if (tag[a] == MESHDEFORM_TAG_INTERIOR) {
        ti++;
      }
      else if (tag[a] == MESHDEFORM_TAG_EXTERIOR) {
        te++;

        if (mdb->semibound[a]) {
          ts++;
        }
      }
    }

    printf("interior %d exterior %d boundary %d semi-boundary %d\n", ti, te, tb, ts);
  }
#endif

  MEM_freeN(stack);
}

static float meshdeform_boundary_phi(const MeshDeformBind *mdb,
                                     const MDefBoundIsect *isect,
                                     int cagevert)
{
  const blender::IndexRange face = mdb->cagemesh_cache.faces[isect->face_index];
  const blender::Span<int> corner_verts = mdb->cagemesh_cache.corner_verts;

  for (int i = 0; i < face.size(); i++) {
    if (corner_verts[face[i]] == cagevert) {
      return isect->poly_weights[i];
    }
  }

  return 0.0f;
}

static float meshdeform_interp_w(MeshDeformBind *mdb,
                                 const float *gridvec,
                                 float * /*vec*/,
                                 int /*cagevert*/)
{
  float dvec[3], ivec[3], result = 0.0f;
  float totweight = 0.0f;

  for (int i = 0; i < 3; i++) {
    ivec[i] = int(gridvec[i]);
    dvec[i] = gridvec[i] - ivec[i];
  }

  for (int i = 0; i < 8; i++) {
    int x, y, z;
    float wx, wy, wz;

    if (i & 1) {
      x = ivec[0] + 1;
      wx = dvec[0];
    }
    else {
      x = ivec[0];
      wx = 1.0f - dvec[0];
    }

    if (i & 2) {
      y = ivec[1] + 1;
      wy = dvec[1];
    }
    else {
      y = ivec[1];
      wy = 1.0f - dvec[1];
    }

    if (i & 4) {
      z = ivec[2] + 1;
      wz = dvec[2];
    }
    else {
      z = ivec[2];
      wz = 1.0f - dvec[2];
    }

    CLAMP(x, 0, mdb->size - 1);
    CLAMP(y, 0, mdb->size - 1);
    CLAMP(z, 0, mdb->size - 1);

    int a = meshdeform_index(mdb, x, y, z, 0);
    float weight = wx * wy * wz;
    result += weight * mdb->phi[a];
    totweight += weight;
  }

  if (totweight > 0.0f) {
    result /= totweight;
  }

  return result;
}

static void meshdeform_check_semibound(MeshDeformBind *mdb, int x, int y, int z)
{
  int i, a;

  a = meshdeform_index(mdb, x, y, z, 0);
  if (mdb->tag[a] != MESHDEFORM_TAG_EXTERIOR) {
    return;
  }

  for (i = 1; i <= 6; i++) {
    if (mdb->boundisect[a][i - 1]) {
      mdb->semibound[a] = 1;
    }
  }
}

static float meshdeform_boundary_total_weight(MeshDeformBind *mdb, int x, int y, int z)
{
  float weight, totweight = 0.0f;
  int i, a;

  a = meshdeform_index(mdb, x, y, z, 0);

  /* count weight for neighbor cells */
  for (i = 1; i <= 6; i++) {
    if (meshdeform_index(mdb, x, y, z, i) == -1) {
      continue;
    }

    if (mdb->boundisect[a][i - 1]) {
      weight = 1.0f / mdb->boundisect[a][i - 1]->len;
    }
    else if (!mdb->semibound[a]) {
      weight = 1.0f / mdb->width[0];
    }
    else {
      weight = 0.0f;
    }

    totweight += weight;
  }

  return totweight;
}

static void meshdeform_matrix_add_cell(
    MeshDeformBind *mdb, LinearSolver *context, int x, int y, int z)
{
  MDefBoundIsect *isect;
  float weight, totweight;
  int i, a, acenter;

  acenter = meshdeform_index(mdb, x, y, z, 0);
  if (mdb->tag[acenter] == MESHDEFORM_TAG_EXTERIOR) {
    return;
  }

  EIG_linear_solver_matrix_add(context, mdb->varidx[acenter], mdb->varidx[acenter], 1.0f);

  totweight = meshdeform_boundary_total_weight(mdb, x, y, z);
  for (i = 1; i <= 6; i++) {
    a = meshdeform_index(mdb, x, y, z, i);
    if (a == -1 || mdb->tag[a] == MESHDEFORM_TAG_EXTERIOR) {
      continue;
    }

    isect = mdb->boundisect[acenter][i - 1];
    if (!isect) {
      weight = (1.0f / mdb->width[0]) / totweight;
      EIG_linear_solver_matrix_add(context, mdb->varidx[acenter], mdb->varidx[a], -weight);
    }
  }
}

static void meshdeform_matrix_add_rhs(
    MeshDeformBind *mdb, LinearSolver *context, int x, int y, int z, int cagevert)
{
  MDefBoundIsect *isect;
  float rhs, weight, totweight;
  int i, a, acenter;

  acenter = meshdeform_index(mdb, x, y, z, 0);
  if (mdb->tag[acenter] == MESHDEFORM_TAG_EXTERIOR) {
    return;
  }

  totweight = meshdeform_boundary_total_weight(mdb, x, y, z);
  for (i = 1; i <= 6; i++) {
    a = meshdeform_index(mdb, x, y, z, i);
    if (a == -1) {
      continue;
    }

    isect = mdb->boundisect[acenter][i - 1];

    if (isect) {
      weight = (1.0f / isect->len) / totweight;
      rhs = weight * meshdeform_boundary_phi(mdb, isect, cagevert);
      EIG_linear_solver_right_hand_side_add(context, 0, mdb->varidx[acenter], rhs);
    }
  }
}

static void meshdeform_matrix_add_semibound_phi(
    MeshDeformBind *mdb, int x, int y, int z, int cagevert)
{
  MDefBoundIsect *isect;
  float rhs, weight, totweight;
  int i, a;

  a = meshdeform_index(mdb, x, y, z, 0);
  if (!mdb->semibound[a]) {
    return;
  }

  mdb->phi[a] = 0.0f;

  totweight = meshdeform_boundary_total_weight(mdb, x, y, z);
  for (i = 1; i <= 6; i++) {
    isect = mdb->boundisect[a][i - 1];

    if (isect) {
      weight = (1.0f / isect->len) / totweight;
      rhs = weight * meshdeform_boundary_phi(mdb, isect, cagevert);
      mdb->phi[a] += rhs;
    }
  }
}

static void meshdeform_matrix_add_exterior_phi(
    MeshDeformBind *mdb, int x, int y, int z, int /*cagevert*/)
{
  float phi, totweight;
  int i, a, acenter;

  acenter = meshdeform_index(mdb, x, y, z, 0);
  if (mdb->tag[acenter] != MESHDEFORM_TAG_EXTERIOR || mdb->semibound[acenter]) {
    return;
  }

  phi = 0.0f;
  totweight = 0.0f;
  for (i = 1; i <= 6; i++) {
    a = meshdeform_index(mdb, x, y, z, i);

    if (a != -1 && mdb->semibound[a]) {
      phi += mdb->phi[a];
      totweight += 1.0f;
    }
  }

  if (totweight != 0.0f) {
    mdb->phi[acenter] = phi / totweight;
  }
}

static void meshdeform_matrix_solve(MeshDeformModifierData *mmd, MeshDeformBind *mdb)
{
  LinearSolver *context;
  float vec[3], gridvec[3];
  int a, b, x, y, z, totvar;
  char message[256];

  /* setup variable indices */
  mdb->varidx = MEM_calloc_arrayN<int>(mdb->size3, "MeshDeformDSvaridx");
  for (a = 0, totvar = 0; a < mdb->size3; a++) {
    mdb->varidx[a] = (mdb->tag[a] == MESHDEFORM_TAG_EXTERIOR) ? -1 : totvar++;
  }

  if (totvar == 0) {
    MEM_freeN(mdb->varidx);
    return;
  }

  progress_bar(0, "Starting mesh deform solve");

  /* setup linear solver */
  context = EIG_linear_solver_new(totvar, totvar, 1);

  /* build matrix */
  for (z = 0; z < mdb->size; z++) {
    for (y = 0; y < mdb->size; y++) {
      for (x = 0; x < mdb->size; x++) {
        meshdeform_matrix_add_cell(mdb, context, x, y, z);
      }
    }
  }

  /* solve for each cage vert */
  for (a = 0; a < mdb->cage_verts_num; a++) {
    /* fill in right hand side and solve */
    for (z = 0; z < mdb->size; z++) {
      for (y = 0; y < mdb->size; y++) {
        for (x = 0; x < mdb->size; x++) {
          meshdeform_matrix_add_rhs(mdb, context, x, y, z, a);
        }
      }
    }

    if (EIG_linear_solver_solve(context)) {
      for (z = 0; z < mdb->size; z++) {
        for (y = 0; y < mdb->size; y++) {
          for (x = 0; x < mdb->size; x++) {
            meshdeform_matrix_add_semibound_phi(mdb, x, y, z, a);
          }
        }
      }

      for (z = 0; z < mdb->size; z++) {
        for (y = 0; y < mdb->size; y++) {
          for (x = 0; x < mdb->size; x++) {
            meshdeform_matrix_add_exterior_phi(mdb, x, y, z, a);
          }
        }
      }

      for (b = 0; b < mdb->size3; b++) {
        if (mdb->tag[b] != MESHDEFORM_TAG_EXTERIOR) {
          mdb->phi[b] = EIG_linear_solver_variable_get(context, 0, mdb->varidx[b]);
        }
        mdb->totalphi[b] += mdb->phi[b];
      }

      if (mdb->weights) {
        /* static bind : compute weights for each vertex */
        for (b = 0; b < mdb->verts_num; b++) {
          if (mdb->inside[b]) {
            copy_v3_v3(vec, mdb->vertexcos[b]);
            gridvec[0] = (vec[0] - mdb->min[0] - mdb->halfwidth[0]) / mdb->width[0];
            gridvec[1] = (vec[1] - mdb->min[1] - mdb->halfwidth[1]) / mdb->width[1];
            gridvec[2] = (vec[2] - mdb->min[2] - mdb->halfwidth[2]) / mdb->width[2];

            mdb->weights[b * mdb->cage_verts_num + a] = meshdeform_interp_w(mdb, gridvec, vec, a);
          }
        }
      }
      else {
        MDefBindInfluence *inf;

        /* dynamic bind */
        for (b = 0; b < mdb->size3; b++) {
          if (mdb->phi[b] >= MESHDEFORM_MIN_INFLUENCE) {
            inf = static_cast<MDefBindInfluence *>(
                BLI_memarena_alloc(mdb->memarena, sizeof(*inf)));
            inf->vertex = a;
            inf->weight = mdb->phi[b];
            inf->next = mdb->dyngrid[b];
            mdb->dyngrid[b] = inf;
          }
        }
      }
    }
    else {
      BKE_modifier_set_error(
          mmd->object, &mmd->modifier, "Failed to find bind solution (increase precision?)");
      error("Mesh Deform: failed to find bind solution.");
      break;
    }

    SNPRINTF_UTF8(message, "Mesh deform solve %d / %d       |||", a + 1, mdb->cage_verts_num);
    progress_bar(float(a + 1) / float(mdb->cage_verts_num), message);
  }

#if 0
  /* sanity check */
  for (b = 0; b < mdb->size3; b++) {
    if (mdb->tag[b] != MESHDEFORM_TAG_EXTERIOR) {
      if (fabsf(mdb->totalphi[b] - 1.0f) > 1e-4f) {
        printf("totalphi deficiency [%s|%d] %d: %.10f\n",
               (mdb->tag[b] == MESHDEFORM_TAG_INTERIOR) ? "interior" : "boundary",
               mdb->semibound[b],
               mdb->varidx[b],
               mdb->totalphi[b]);
      }
    }
  }
#endif

  /* free */
  MEM_freeN(mdb->varidx);

  EIG_linear_solver_delete(context);
}

static void harmonic_coordinates_bind(MeshDeformModifierData *mmd, MeshDeformBind *mdb)
{
  using namespace blender;
  MDefBindInfluence *inf;
  MDefInfluence *mdinf;
  MDefCell *cell;
  float center[3], vec[3], maxwidth, totweight;
  int a, b, x, y, z, totinside, offset;

  /* compute bounding box of the cage mesh */
  INIT_MINMAX(mdb->min, mdb->max);

  for (a = 0; a < mdb->cage_verts_num; a++) {
    minmax_v3v3_v3(mdb->min, mdb->max, mdb->cagecos[a]);
  }

  /* allocate memory */
  mdb->size = (2 << (mmd->gridsize - 1)) + 2;
  mdb->size3 = mdb->size * mdb->size * mdb->size;
  mdb->tag = MEM_calloc_arrayN<int>(mdb->size3, "MeshDeformBindTag");
  mdb->phi = MEM_calloc_arrayN<float>(mdb->size3, "MeshDeformBindPhi");
  mdb->totalphi = MEM_calloc_arrayN<float>(mdb->size3, "MeshDeformBindTotalPhi");
  mdb->boundisect = static_cast<MDefBoundIsect *(*)[6]>(
      MEM_callocN(sizeof(*mdb->boundisect) * mdb->size3, "MDefBoundIsect"));
  mdb->semibound = MEM_calloc_arrayN<int>(mdb->size3, "MDefSemiBound");
  mdb->bvhdata = mdb->cagemesh->bvh_corner_tris();
  mdb->bvhtree = mdb->bvhdata.tree;
  mdb->inside = MEM_calloc_arrayN<int>(mdb->verts_num, "MDefInside");

  if (mmd->flag & MOD_MDEF_DYNAMIC_BIND) {
    mdb->dyngrid = MEM_calloc_arrayN<MDefBindInfluence *>(mdb->size3, "MDefDynGrid");
  }
  else {
    mdb->weights = MEM_calloc_arrayN<float>(mdb->verts_num * mdb->cage_verts_num, "MDefWeights");
  }

  mdb->memarena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "harmonic coords arena");
  BLI_memarena_use_calloc(mdb->memarena);

  /* Initialize data from `cagedm` for reuse. */
  {
    Mesh *mesh = mdb->cagemesh;
    mdb->cagemesh_cache.faces = mesh->faces();
    mdb->cagemesh_cache.corner_verts = mesh->corner_verts();
    mdb->cagemesh_cache.corner_tris = mesh->corner_tris();
    mdb->cagemesh_cache.tri_faces = mesh->corner_tri_faces();
    mdb->cagemesh_cache.face_normals = mesh->face_normals();
  }

  /* make bounding box equal size in all directions, add padding, and compute
   * width of the cells */
  maxwidth = -1.0f;
  for (a = 0; a < 3; a++) {
    maxwidth = std::max(mdb->max[a] - mdb->min[a], maxwidth);
  }

  for (a = 0; a < 3; a++) {
    center[a] = (mdb->min[a] + mdb->max[a]) * 0.5f;
    mdb->min[a] = center[a] - maxwidth * 0.5f;
    mdb->max[a] = center[a] + maxwidth * 0.5f;

    mdb->width[a] = (mdb->max[a] - mdb->min[a]) / (mdb->size - 4);
    mdb->min[a] -= 2.1f * mdb->width[a];
    mdb->max[a] += 2.1f * mdb->width[a];

    mdb->width[a] = (mdb->max[a] - mdb->min[a]) / mdb->size;
    mdb->halfwidth[a] = mdb->width[a] * 0.5f;
  }

  progress_bar(0, "Setting up mesh deform system");

  totinside = 0;
  for (a = 0; a < mdb->verts_num; a++) {
    copy_v3_v3(vec, mdb->vertexcos[a]);
    mdb->inside[a] = meshdeform_inside_cage(mdb, vec);
    if (mdb->inside[a]) {
      totinside++;
    }
  }
  (void)totinside; /* Quiet set-but-unused warning (may be removed). */

  /* free temporary MDefBoundIsects */
  BLI_memarena_free(mdb->memarena);
  mdb->memarena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "harmonic coords arena");

  /* start with all cells untyped */
  for (a = 0; a < mdb->size3; a++) {
    mdb->tag[a] = MESHDEFORM_TAG_UNTYPED;
  }

  /* detect intersections and tag boundary cells */
  for (z = 0; z < mdb->size; z++) {
    for (y = 0; y < mdb->size; y++) {
      for (x = 0; x < mdb->size; x++) {
        meshdeform_add_intersections(mdb, x, y, z);
      }
    }
  }

  /* compute exterior and interior tags */
  meshdeform_bind_floodfill(mdb);

  for (z = 0; z < mdb->size; z++) {
    for (y = 0; y < mdb->size; y++) {
      for (x = 0; x < mdb->size; x++) {
        meshdeform_check_semibound(mdb, x, y, z);
      }
    }
  }

  /* solve */
  meshdeform_matrix_solve(mmd, mdb);

  /* assign results */
  if (mmd->flag & MOD_MDEF_DYNAMIC_BIND) {
    mmd->influences_num = 0;
    for (a = 0; a < mdb->size3; a++) {
      for (inf = mdb->dyngrid[a]; inf; inf = inf->next) {
        mmd->influences_num++;
      }
    }

    /* convert MDefBindInfluences to smaller MDefInfluences */
    mmd->dyngrid = MEM_calloc_arrayN<MDefCell>(mdb->size3, "MDefDynGrid");
    mmd->dyngrid_sharing_info = implicit_sharing::info_for_mem_free(mmd->dyngrid);
    mmd->dyninfluences = MEM_calloc_arrayN<MDefInfluence>(mmd->influences_num, "MDefInfluence");
    mmd->dyninfluences_sharing_info = implicit_sharing::info_for_mem_free(mmd->dyninfluences);
    offset = 0;
    for (a = 0; a < mdb->size3; a++) {
      cell = &mmd->dyngrid[a];
      cell->offset = offset;

      totweight = 0.0f;
      mdinf = mmd->dyninfluences + cell->offset;
      for (inf = mdb->dyngrid[a]; inf; inf = inf->next, mdinf++) {
        mdinf->weight = inf->weight;
        mdinf->vertex = inf->vertex;
        totweight += mdinf->weight;
        cell->influences_num++;
      }

      if (totweight > 0.0f) {
        mdinf = mmd->dyninfluences + cell->offset;
        for (b = 0; b < cell->influences_num; b++, mdinf++) {
          mdinf->weight /= totweight;
        }
      }

      offset += cell->influences_num;
    }

    mmd->dynverts = mdb->inside;
    mmd->dynverts_sharing_info = implicit_sharing::info_for_mem_free(mmd->dynverts);
    mmd->dyngridsize = mdb->size;
    copy_v3_v3(mmd->dyncellmin, mdb->min);
    mmd->dyncellwidth = mdb->width[0];
    MEM_freeN(mdb->dyngrid);
  }
  else {
    mmd->bindweights = mdb->weights;
    MEM_freeN(mdb->inside);
  }

  MEM_freeN(mdb->tag);
  MEM_freeN(mdb->phi);
  MEM_freeN(mdb->totalphi);
  MEM_freeN(mdb->boundisect);
  MEM_freeN(mdb->semibound);
  BLI_memarena_free(mdb->memarena);
}

void ED_mesh_deform_bind_callback(Object *object,
                                  MeshDeformModifierData *mmd,
                                  Mesh *cagemesh,
                                  float *vertexcos,
                                  int verts_num,
                                  float cagemat[4][4])
{
  using namespace blender;
  MeshDeformModifierData *mmd_orig = reinterpret_cast<MeshDeformModifierData *>(
      BKE_modifier_get_original(object, &mmd->modifier));
  MeshDeformBind mdb{};
  int a;

  waitcursor(1);
  start_progress_bar();

  /* No need to support other kinds of mesh data as binding is a one-off action. */
  BKE_mesh_wrapper_ensure_mdata(cagemesh);

  /* get mesh and cage mesh */
  mdb.vertexcos = static_cast<float (*)[3]>(
      MEM_callocN(sizeof(float[3]) * verts_num, "MeshDeformCos"));
  mdb.verts_num = verts_num;

  mdb.cagemesh = cagemesh;
  mdb.cage_verts_num = mdb.cagemesh->verts_num;
  mdb.cagecos = static_cast<float (*)[3]>(
      MEM_callocN(sizeof(*mdb.cagecos) * mdb.cage_verts_num, "MeshDeformBindCos"));
  copy_m4_m4(mdb.cagemat, cagemat);

  const blender::Span<blender::float3> positions = mdb.cagemesh->vert_positions();
  for (a = 0; a < mdb.cage_verts_num; a++) {
    copy_v3_v3(mdb.cagecos[a], positions[a]);
  }
  for (a = 0; a < mdb.verts_num; a++) {
    mul_v3_m4v3(mdb.vertexcos[a], mdb.cagemat, vertexcos + a * 3);
  }

  /* solve */
  harmonic_coordinates_bind(mmd_orig, &mdb);

  /* assign bind variables */
  mmd_orig->bindcagecos = (float *)mdb.cagecos;
  mmd_orig->bindcagecos_sharing_info = implicit_sharing::info_for_mem_free(mmd_orig->bindcagecos);
  mmd_orig->verts_num = mdb.verts_num;
  mmd_orig->cage_verts_num = mdb.cage_verts_num;
  copy_m4_m4(mmd_orig->bindmat, mmd_orig->object->object_to_world().ptr());

  /* transform bindcagecos to world space */
  for (a = 0; a < mdb.cage_verts_num; a++) {
    mul_m4_v3(mmd_orig->object->object_to_world().ptr(), mmd_orig->bindcagecos + a * 3);
  }

  /* free */
  MEM_freeN(mdb.vertexcos);

  /* compact weights */
  BKE_modifier_mdef_compact_influences(reinterpret_cast<ModifierData *>(mmd_orig));

  end_progress_bar();
  waitcursor(0);
}
