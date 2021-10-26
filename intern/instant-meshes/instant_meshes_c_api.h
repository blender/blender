#pragma once

//#define INSTANT_MESHES_VIS_COLOR

/* clang-format off */
//remeshedge->flag
enum {
  REMESH_EDGE_BOUNDARY = (1<<0),
  REMESH_EDGE_USE_DIR = (1<<1)
};
/* clang-format on */

typedef struct RemeshVertex {
  float co[3], no[3];
#ifdef INSTANT_MESHES_VIS_COLOR
  float viscolor[3];
#endif
} RemeshVertex;

// edge constraint
typedef struct RemeshEdge {
  int v1, v2, flag;
  float dir[3];
} RemeshEdge;

typedef struct RemeshTri {
  int v1, v2, v3;
  int eflags[3];
} RemeshTri;

typedef struct RemeshOutFace {
  int *verts;
  int totvert;
} RemeshOutFace;

typedef struct RemeshMesh {
  RemeshTri *tris;
  RemeshVertex *verts;
  RemeshEdge *edges;  // list of constrained edges, need not be all edges in the mesh

  int tottri, totvert, totedge;

  RemeshOutFace *outfaces;
  RemeshVertex *outverts;
  int *out_scracth;

  int totoutface;
  int totoutvert;

  int goal_faces, iterations;
} RemeshMesh;

#ifdef __cplusplus
extern "C" {
#endif

void instant_meshes_run(RemeshMesh *mesh);
void instant_meshes_finish(RemeshMesh *mesh);
void instant_meshes_set_number_of_threads(int n);

#ifdef __cplusplus
}
#endif
