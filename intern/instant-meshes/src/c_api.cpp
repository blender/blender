#include "MEM_guardedalloc.h"

#include "adjacency.h"
#include "cleanup.h"
#include "common.h"
#include "dedge.h"
#include "extract.h"
#include "field.h"
#include "hierarchy.h"
#include "normal.h"
#include "smoothcurve.h"
#include "subdivide.h"

#include "meshstats.h"

#include "../instant_meshes_c_api.h"

#include <set>

int instant_meshes_nprocs = 1;

static const float len_v3v3(const float *a, const float *b)
{
  float dx = a[0] - b[0];
  float dy = a[1] - b[1];
  float dz = a[2] - b[2];

  float len = dx * dx + dy * dy + dz * dz;

  return len > 0.0f ? sqrtf(len) : 0.0f;
}

static void cross_tri_v3(float n[3], const float v1[3], const float v2[3], const float v3[3])
{
  float n1[3], n2[3];

  n1[0] = v1[0] - v2[0];
  n2[0] = v2[0] - v3[0];
  n1[1] = v1[1] - v2[1];
  n2[1] = v2[1] - v3[1];
  n1[2] = v1[2] - v2[2];
  n2[2] = v2[2] - v3[2];
  n[0] = n1[1] * n2[2] - n1[2] * n2[1];
  n[1] = n1[2] * n2[0] - n1[0] * n2[2];
  n[2] = n1[0] * n2[1] - n1[1] * n2[0];
}

/* Triangles */
static float area_tri_v3(const float v1[3], const float v2[3], const float v3[3])
{
  float n[3];
  cross_tri_v3(n, v1, v2, v3);
  return sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) * 0.5f;
}

static float fract(float f)
{
  return f - floorf(f);
}

extern "C" {
void instant_meshes_run(RemeshMesh *mesh)
{
  MatrixXu F;
  MatrixXf V;
  MatrixXf N;

  float elen = 0.0f;
  int totlen = 0;
  float scale = 0.1f;

  std::set<uint32_t> creaseSet;

  F.resize(3, mesh->tottri);
  V.resize(3, mesh->totvert);
  N.resize(3, mesh->totvert);

  float area = 0.0f;

  for (int i = 0; i < mesh->tottri; i++) {
    RemeshTri *tri = mesh->tris + i;

    float *co1 = mesh->verts[tri->v1].co;
    float *co2 = mesh->verts[tri->v2].co;
    float *co3 = mesh->verts[tri->v3].co;

    elen += len_v3v3(co1, co2);
    elen += len_v3v3(co2, co3);
    elen += len_v3v3(co3, co1);
    totlen += 3;

    area += area_tri_v3(co1, co2, co3);

    F(0, i) = tri->v1;
    F(1, i) = tri->v2;
    F(2, i) = tri->v3;
  }

  for (int i = 0; i < mesh->totvert; i++) {
    RemeshVertex *v = mesh->verts + i;

    for (int j = 0; j < 3; j++) {
      V(j, i) = v->co[j];
      N(j, i) = v->no[j];
    }
  }

  if (totlen > 0) {
    elen /= totlen;
  }

  scale = sqrt(area / (float)mesh->goal_faces);
  if (scale == 0.0f) {
    return;  // error
  }

  const bool extrinsic = true;
  const bool deterministic = false;
  const int rosy = 2, posy = 4;

  printf("totarea: %.4f, scale: %.5f\n", area, scale);

  VectorXu V2E, E2E;
  VectorXb boundary;
  VectorXb nonManifold;

  nonManifold.resize(V.cols());

  build_dedge(F, V, V2E, E2E, boundary, nonManifold);
  // AdjacencyMatrix adj = generate_adjacency_matrix_cotan(F, V, V2E, E2E, nonManifold);
  AdjacencyMatrix adj = generate_adjacency_matrix_uniform(F, V2E, E2E, nonManifold);

  VectorXf A;

  generate_smooth_normals(F, V, V2E, E2E, nonManifold, N);
  compute_dual_vertex_areas(F, V, V2E, E2E, nonManifold, A);

  MultiResolutionHierarchy mRes;

  mRes.setF(std::move(F));
  mRes.setV(std::move(V));
  mRes.setN(std::move(N));
  mRes.setE2E(std::move(E2E));
  mRes.setAdj(std::move(adj));
  mRes.setA(std::move(A));
  mRes.setScale(scale);

  printf("building multiresolution hierarchy\n");
  mRes.build(deterministic);
  mRes.resetSolution();

  /* set up edge constraints */
  mRes.clearConstraints();

  for (int i = 0; i < mesh->totedge; i++) {
    RemeshEdge *e = mesh->edges + i;

    const MatrixXu &F = mRes.F();
    const MatrixXf &N = mRes.N(), &V = mRes.V();
    const VectorXu &E2E = mRes.E2E();

    uint32_t i0 = (uint32_t)e->v1;
    uint32_t i1 = (uint32_t)e->v2;
    Vector3f p0 = V.col(i0), p1 = V.col(i1);

    if (0 && (e->flag & REMESH_EDGE_USE_DIR)) {
      Vector3f dir(e->dir);

      if (dir.squaredNorm() > 0) {
        mRes.CO().col(i0) = p0;
        mRes.CO().col(i1) = p1;
        mRes.CQ().col(i0) = mRes.CQ().col(i1) = dir;
        mRes.CQw()[i0] = mRes.CQw()[i1] = mRes.COw()[i0] = mRes.COw()[i1] = 0.05f;
      }
    }
    else if (e->flag & REMESH_EDGE_BOUNDARY) {
      Vector3f edge = p1 - p0;

      creaseSet.insert((uint32_t)e->v1);
      creaseSet.insert((uint32_t)e->v2);

      if (edge.squaredNorm() > 0) {
        edge.normalize();
        mRes.CO().col(i0) = p0;
        mRes.CO().col(i1) = p1;
        mRes.CQ().col(i0) = mRes.CQ().col(i1) = edge;
        mRes.CQw()[i0] = mRes.CQw()[i1] = mRes.COw()[i0] = mRes.COw()[i1] = 1.0f;
      }
    }
  }

  mRes.propagateConstraints(rosy, posy);

  Optimizer opt(mRes, false);

  opt.setExtrinsic(extrinsic);
  opt.setRoSy(rosy);
  opt.setPoSy(posy);

  const int iterations = mesh->iterations;

  printf("optimizing orientation field\n");
  for (int step = 0; step < iterations; step++) {
    opt.optimizeOrientations(-1);
    opt.notify();
    opt.wait();
  }

  std::map<uint32_t, uint32_t> sing;
  compute_orientation_singularities(mRes, sing, extrinsic, rosy);
  cout << "Orientation field has " << sing.size() << " singularities." << endl;

  printf("optimizing position field\n");
  for (int step = 0; step < iterations; step++) {
    opt.optimizePositions(-1);
    opt.notify();
    opt.wait();
  }

  std::map<uint32_t, Vector2i> pos_sing;
  compute_position_singularities(mRes, sing, pos_sing, extrinsic, rosy, posy);
  cout << "Position field has " << pos_sing.size() << " singularities." << endl;

#ifdef INSTANT_MESHES_VIS_COLOR
  {
    mesh->totoutface = mesh->tottri;
    mesh->totoutvert = mesh->totvert;

    mesh->outfaces = (RemeshOutFace *)MEM_malloc_arrayN(
        mesh->tottri, sizeof(RemeshOutFace), "RemeshOutFace");
    mesh->outverts = (RemeshVertex *)MEM_malloc_arrayN(
        mesh->totvert, sizeof(RemeshVertex), "RemeshVertex");
    memcpy(mesh->outverts, mesh->verts, sizeof(*mesh->outverts) * mesh->totvert);
    mesh->out_scracth = (int *)MEM_malloc_arrayN(mesh->tottri * 3, sizeof(int), "out_scratch");

    int li = 0;
    RemeshOutFace *f = mesh->outfaces;
    RemeshTri *tri = mesh->tris;
    RemeshVertex *v = mesh->outverts;

    auto O = mRes.O(0);
    printf("O size: [%d][%d]\n", (int)O.cols(), (int)O.rows());

    for (int i = 0; i < mesh->totvert; i++, v++) {
      v->viscolor[0] = O(0, i);
      v->viscolor[1] = O(1, i);
      // v->viscolor[2] = O(2, i);
    }

    for (int i = 0; i < mesh->tottri; i++, f++, tri++) {
      f->verts = mesh->out_scracth + li;
      li += 3;

      f->verts[0] = tri->v1;
      f->verts[1] = tri->v2;
      f->verts[2] = tri->v3;
      f->totvert = 3;
    }
  }
#endif

  opt.shutdown();

#ifdef INSTANT_MESHES_VIS_COLOR
  return;
#endif

  std::vector<std::vector<TaggedLink>> adj_extracted;

  MatrixXu F_extracted;
  MatrixXf V_extracted;
  MatrixXf N_extracted;
  MatrixXf Nf_extracted;

  std::set<uint32_t> creaseOut;

  printf("extracting mesh\n");

  extract_graph(mRes,
                extrinsic,
                rosy,
                posy,
                adj_extracted,
                V_extracted,
                N_extracted,
                creaseSet,
                creaseOut,
                deterministic,
                true,
                true,
                true);

  extract_faces(adj_extracted,
                V_extracted,
                N_extracted,
                Nf_extracted,
                F_extracted,
                posy,
                mRes.scale(),
                creaseOut,
                true,
                true,
                nullptr,
                0);

  // F_extracted = mRes.F();
  // V_extracted = mRes.V();

  mesh->totoutface = F_extracted.cols();
  mesh->totoutvert = V_extracted.cols();

  int sides = F_extracted.rows();

  mesh->outfaces = (RemeshOutFace *)MEM_malloc_arrayN(
      mesh->totoutface, sizeof(RemeshOutFace), "RemeshOutFaces");
  mesh->outverts = (RemeshVertex *)MEM_malloc_arrayN(
      mesh->totoutvert, sizeof(RemeshVertex), "RemeshOutVerts");
  mesh->out_scracth = (int *)MEM_malloc_arrayN(
      mesh->totoutface * sides, sizeof(int), "out_scratch");

  printf(
      "sides:%d\ntotal faces: %d\ntotal verts: %d\n", sides, mesh->totoutface, mesh->totoutvert);

  RemeshOutFace *f = mesh->outfaces;
  int totface = mesh->totoutface;
  int i = 0, fi = 0, li = 0;

  /* Check for irregular faces */
  std::map<uint32_t, std::pair<uint32_t, std::map<uint32_t, uint32_t>>> irregular;
  size_t nIrregular = 0;

  for (; fi < totface; fi++) {
    if (F_extracted(2, fi) == F_extracted(3, fi)) {
      nIrregular++;
      auto &value = irregular[F_extracted(2, fi)];
      value.first = fi;
      value.second[F_extracted(0, fi)] = F_extracted(1, fi);
      continue;
    }

    f->verts = mesh->out_scracth + li;
    li += sides;

    int j = 0;

    for (j = 0; j < sides; j++) {
      f->verts[j] = F_extracted(j, fi);
    }

    //  if (j != sides) {
    //      i--;
    //}

    f->totvert = sides;
    i++;
    f++;
  }

  for (auto item : irregular) {
    auto face = item.second;
    uint32_t v = face.second.begin()->first, first = v, k = 0;

    int j = 0;
    f->verts = mesh->out_scracth + li;

    while (true) {
      v = face.second[v];
      f->verts[j] = (int)v;

      li++;
      j++;

      if (v == first || ++k == face.second.size())
        break;
    }

    f->totvert = j;

    f++;
    i++;
  }

  printf("final totface: %d, alloc totface: %d\n", i, mesh->totoutface);
  printf("final totloop: %d, alloc totloop: %d\n", li, mesh->totoutface * sides);

  mesh->totoutface = i;

  RemeshVertex *v = mesh->outverts;
  for (int i = 0; i < mesh->totoutvert; i++, v++) {
    for (int j = 0; j < 3; j++) {
      v->co[j] = V_extracted(j, i);
      v->no[j] = N_extracted(j, i);
    }
  }
}

void instant_meshes_finish(RemeshMesh *mesh)
{
  /*
  if (mesh->verts) {
    MEM_freeN((void *)mesh->verts);
  }

  if (mesh->edges) {
    MEM_freeN((void *)mesh->edges);
  }

  if (mesh->tris) {
    MEM_freeN((void *)mesh->tris);
  }*/

  if (mesh->outverts) {
    MEM_freeN((void *)mesh->outverts);
  }
  if (mesh->outfaces) {
    MEM_freeN((void *)mesh->outfaces);
  }
  if (mesh->out_scracth) {
    MEM_freeN((void *)mesh->out_scracth);
  }
}

void instant_meshes_set_number_of_threads(int n)
{
  instant_meshes_nprocs = n;
}
};
