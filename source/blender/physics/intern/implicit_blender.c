/*
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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bph
 */

#include "implicit.h"

#ifdef IMPLICIT_SOLVER_BLENDER

#  include "MEM_guardedalloc.h"

#  include "DNA_scene_types.h"
#  include "DNA_object_types.h"
#  include "DNA_object_force_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_texture_types.h"

#  include "BLI_math.h"
#  include "BLI_utildefines.h"

#  include "BKE_cloth.h"
#  include "BKE_collision.h"
#  include "BKE_effect.h"

#  include "BPH_mass_spring.h"

#  ifdef __GNUC__
#    pragma GCC diagnostic ignored "-Wtype-limits"
#  endif

#  ifdef _OPENMP
#    define CLOTH_OPENMP_LIMIT 512
#  endif

//#define DEBUG_TIME

#  ifdef DEBUG_TIME
#    include "PIL_time.h"
#  endif

static float I[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
static float ZERO[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

#  if 0
#    define C99
#    ifdef C99
#      defineDO_INLINE inline
#    else
#      defineDO_INLINE static
#    endif
#  endif /* if 0 */

struct Cloth;

//////////////////////////////////////////
/* fast vector / matrix library, enhancements are welcome :) -dg */
/////////////////////////////////////////

/* DEFINITIONS */
typedef float lfVector[3];
typedef struct fmatrix3x3 {
  float m[3][3];     /* 3x3 matrix */
  unsigned int c, r; /* column and row number */
  /* int pinned; // is this vertex allowed to move? */
  float n1, n2, n3;    /* three normal vectors for collision constrains */
  unsigned int vcount; /* vertex count */
  unsigned int scount; /* spring count */
} fmatrix3x3;

///////////////////////////
// float[3] vector
///////////////////////////
/* simple vector code */
/* STATUS: verified */
DO_INLINE void mul_fvector_S(float to[3], float from[3], float scalar)
{
  to[0] = from[0] * scalar;
  to[1] = from[1] * scalar;
  to[2] = from[2] * scalar;
}
/* simple v^T * v product ("outer product") */
/* STATUS: HAS TO BE verified (*should* work) */
DO_INLINE void mul_fvectorT_fvector(float to[3][3], float vectorA[3], float vectorB[3])
{
  mul_fvector_S(to[0], vectorB, vectorA[0]);
  mul_fvector_S(to[1], vectorB, vectorA[1]);
  mul_fvector_S(to[2], vectorB, vectorA[2]);
}
/* simple v^T * v product with scalar ("outer product") */
/* STATUS: HAS TO BE verified (*should* work) */
DO_INLINE void mul_fvectorT_fvectorS(float to[3][3], float vectorA[3], float vectorB[3], float aS)
{
  mul_fvectorT_fvector(to, vectorA, vectorB);

  mul_fvector_S(to[0], to[0], aS);
  mul_fvector_S(to[1], to[1], aS);
  mul_fvector_S(to[2], to[2], aS);
}

#  if 0
/* printf vector[3] on console: for debug output */
static void print_fvector(float m3[3])
{
  printf("%f\n%f\n%f\n\n", m3[0], m3[1], m3[2]);
}

///////////////////////////
// long float vector float (*)[3]
///////////////////////////
/* print long vector on console: for debug output */
DO_INLINE void print_lfvector(float (*fLongVector)[3], unsigned int verts)
{
  unsigned int i = 0;
  for (i = 0; i < verts; i++) {
    print_fvector(fLongVector[i]);
  }
}
#  endif

/* create long vector */
DO_INLINE lfVector *create_lfvector(unsigned int verts)
{
  /* TODO: check if memory allocation was successful */
  return (lfVector *)MEM_callocN(verts * sizeof(lfVector), "cloth_implicit_alloc_vector");
  // return (lfVector *)cloth_aligned_malloc(&MEMORY_BASE, verts * sizeof(lfVector));
}
/* delete long vector */
DO_INLINE void del_lfvector(float (*fLongVector)[3])
{
  if (fLongVector != NULL) {
    MEM_freeN(fLongVector);
    // cloth_aligned_free(&MEMORY_BASE, fLongVector);
  }
}
/* copy long vector */
DO_INLINE void cp_lfvector(float (*to)[3], float (*from)[3], unsigned int verts)
{
  memcpy(to, from, verts * sizeof(lfVector));
}
/* init long vector with float[3] */
DO_INLINE void init_lfvector(float (*fLongVector)[3], float vector[3], unsigned int verts)
{
  unsigned int i = 0;
  for (i = 0; i < verts; i++) {
    copy_v3_v3(fLongVector[i], vector);
  }
}
/* zero long vector with float[3] */
DO_INLINE void zero_lfvector(float (*to)[3], unsigned int verts)
{
  memset(to, 0.0f, verts * sizeof(lfVector));
}
/* multiply long vector with scalar*/
DO_INLINE void mul_lfvectorS(float (*to)[3],
                             float (*fLongVector)[3],
                             float scalar,
                             unsigned int verts)
{
  unsigned int i = 0;

  for (i = 0; i < verts; i++) {
    mul_fvector_S(to[i], fLongVector[i], scalar);
  }
}
/* multiply long vector with scalar*/
/* A -= B * float */
DO_INLINE void submul_lfvectorS(float (*to)[3],
                                float (*fLongVector)[3],
                                float scalar,
                                unsigned int verts)
{
  unsigned int i = 0;
  for (i = 0; i < verts; i++) {
    VECSUBMUL(to[i], fLongVector[i], scalar);
  }
}
/* dot product for big vector */
DO_INLINE float dot_lfvector(float (*fLongVectorA)[3],
                             float (*fLongVectorB)[3],
                             unsigned int verts)
{
  long i = 0;
  float temp = 0.0;
  // XXX brecht, disabled this for now (first schedule line was already disabled),
  // due to non-commutative nature of floating point ops this makes the sim give
  // different results each time you run it!
  // schedule(guided, 2)
  //#pragma omp parallel for reduction(+: temp) if (verts > CLOTH_OPENMP_LIMIT)
  for (i = 0; i < (long)verts; i++) {
    temp += dot_v3v3(fLongVectorA[i], fLongVectorB[i]);
  }
  return temp;
}
/* A = B + C  --> for big vector */
DO_INLINE void add_lfvector_lfvector(float (*to)[3],
                                     float (*fLongVectorA)[3],
                                     float (*fLongVectorB)[3],
                                     unsigned int verts)
{
  unsigned int i = 0;

  for (i = 0; i < verts; i++) {
    add_v3_v3v3(to[i], fLongVectorA[i], fLongVectorB[i]);
  }
}
/* A = B + C * float --> for big vector */
DO_INLINE void add_lfvector_lfvectorS(float (*to)[3],
                                      float (*fLongVectorA)[3],
                                      float (*fLongVectorB)[3],
                                      float bS,
                                      unsigned int verts)
{
  unsigned int i = 0;

  for (i = 0; i < verts; i++) {
    VECADDS(to[i], fLongVectorA[i], fLongVectorB[i], bS);
  }
}
/* A = B * float + C * float --> for big vector */
DO_INLINE void add_lfvectorS_lfvectorS(float (*to)[3],
                                       float (*fLongVectorA)[3],
                                       float aS,
                                       float (*fLongVectorB)[3],
                                       float bS,
                                       unsigned int verts)
{
  unsigned int i = 0;

  for (i = 0; i < verts; i++) {
    VECADDSS(to[i], fLongVectorA[i], aS, fLongVectorB[i], bS);
  }
}
/* A = B - C * float --> for big vector */
DO_INLINE void sub_lfvector_lfvectorS(float (*to)[3],
                                      float (*fLongVectorA)[3],
                                      float (*fLongVectorB)[3],
                                      float bS,
                                      unsigned int verts)
{
  unsigned int i = 0;
  for (i = 0; i < verts; i++) {
    VECSUBS(to[i], fLongVectorA[i], fLongVectorB[i], bS);
  }
}
/* A = B - C --> for big vector */
DO_INLINE void sub_lfvector_lfvector(float (*to)[3],
                                     float (*fLongVectorA)[3],
                                     float (*fLongVectorB)[3],
                                     unsigned int verts)
{
  unsigned int i = 0;

  for (i = 0; i < verts; i++) {
    sub_v3_v3v3(to[i], fLongVectorA[i], fLongVectorB[i]);
  }
}
///////////////////////////
// 3x3 matrix
///////////////////////////
#  if 0
/* printf 3x3 matrix on console: for debug output */
static void print_fmatrix(float m3[3][3])
{
  printf("%f\t%f\t%f\n", m3[0][0], m3[0][1], m3[0][2]);
  printf("%f\t%f\t%f\n", m3[1][0], m3[1][1], m3[1][2]);
  printf("%f\t%f\t%f\n\n", m3[2][0], m3[2][1], m3[2][2]);
}

static void print_sparse_matrix(fmatrix3x3 *m)
{
  if (m) {
    unsigned int i;
    for (i = 0; i < m[0].vcount + m[0].scount; i++) {
      printf("%d:\n", i);
      print_fmatrix(m[i].m);
    }
  }
}
#  endif

#  if 0
static void print_lvector(lfVector *v, int numverts)
{
  int i;
  for (i = 0; i < numverts; ++i) {
    if (i > 0)
      printf("\n");

    printf("%f,\n", v[i][0]);
    printf("%f,\n", v[i][1]);
    printf("%f,\n", v[i][2]);
  }
}
#  endif

#  if 0
static void print_bfmatrix(fmatrix3x3 *m)
{
  int tot = m[0].vcount + m[0].scount;
  int size = m[0].vcount * 3;
  float *t = MEM_callocN(sizeof(float) * size * size, "bfmatrix");
  int q, i, j;

  for (q = 0; q < tot; ++q) {
    int k = 3 * m[q].r;
    int l = 3 * m[q].c;

    for (j = 0; j < 3; ++j) {
      for (i = 0; i < 3; ++i) {
        //              if (t[k + i + (l + j) * size] != 0.0f) {
        //                  printf("warning: overwriting value at %d, %d\n", m[q].r, m[q].c);
        //              }
        if (k == l) {
          t[k + i + (k + j) * size] += m[q].m[i][j];
        }
        else {
          t[k + i + (l + j) * size] += m[q].m[i][j];
          t[l + j + (k + i) * size] += m[q].m[j][i];
        }
      }
    }
  }

  for (j = 0; j < size; ++j) {
    if (j > 0 && j % 3 == 0)
      printf("\n");

    for (i = 0; i < size; ++i) {
      if (i > 0 && i % 3 == 0)
        printf("  ");

      implicit_print_matrix_elem(t[i + j * size]);
    }
    printf("\n");
  }

  MEM_freeN(t);
}
#  endif

/* copy 3x3 matrix */
DO_INLINE void cp_fmatrix(float to[3][3], float from[3][3])
{
  // memcpy(to, from, sizeof (float) * 9);
  copy_v3_v3(to[0], from[0]);
  copy_v3_v3(to[1], from[1]);
  copy_v3_v3(to[2], from[2]);
}

/* copy 3x3 matrix */
DO_INLINE void initdiag_fmatrixS(float to[3][3], float aS)
{
  cp_fmatrix(to, ZERO);

  to[0][0] = aS;
  to[1][1] = aS;
  to[2][2] = aS;
}

#  if 0
/* calculate determinant of 3x3 matrix */
DO_INLINE float det_fmatrix(float m[3][3])
{
  return m[0][0] * m[1][1] * m[2][2] + m[1][0] * m[2][1] * m[0][2] + m[0][1] * m[1][2] * m[2][0] -
         m[0][0] * m[1][2] * m[2][1] - m[0][1] * m[1][0] * m[2][2] - m[2][0] * m[1][1] * m[0][2];
}

DO_INLINE void inverse_fmatrix(float to[3][3], float from[3][3])
{
  unsigned int i, j;
  float d;

  if ((d = det_fmatrix(from)) == 0) {
    printf("can't build inverse");
    exit(0);
  }
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      int i1 = (i + 1) % 3;
      int i2 = (i + 2) % 3;
      int j1 = (j + 1) % 3;
      int j2 = (j + 2) % 3;
      /** Reverse indexes i&j to take transpose. */
      to[j][i] = (from[i1][j1] * from[i2][j2] - from[i1][j2] * from[i2][j1]) / d;
      /**
       * <pre>
       * if (i == j) {
       *     to[i][j] = 1.0f / from[i][j];
       * }
       * else {
       *     to[i][j] = 0;
       * }
       * </pre>
       */
    }
  }
}
#  endif

/* 3x3 matrix multiplied by a scalar */
/* STATUS: verified */
DO_INLINE void mul_fmatrix_S(float matrix[3][3], float scalar)
{
  mul_fvector_S(matrix[0], matrix[0], scalar);
  mul_fvector_S(matrix[1], matrix[1], scalar);
  mul_fvector_S(matrix[2], matrix[2], scalar);
}

/* a vector multiplied by a 3x3 matrix */
/* STATUS: verified */
DO_INLINE void mul_fvector_fmatrix(float *to, float *from, float matrix[3][3])
{
  to[0] = matrix[0][0] * from[0] + matrix[1][0] * from[1] + matrix[2][0] * from[2];
  to[1] = matrix[0][1] * from[0] + matrix[1][1] * from[1] + matrix[2][1] * from[2];
  to[2] = matrix[0][2] * from[0] + matrix[1][2] * from[1] + matrix[2][2] * from[2];
}

/* 3x3 matrix multiplied by a vector */
/* STATUS: verified */
DO_INLINE void mul_fmatrix_fvector(float *to, float matrix[3][3], float from[3])
{
  to[0] = dot_v3v3(matrix[0], from);
  to[1] = dot_v3v3(matrix[1], from);
  to[2] = dot_v3v3(matrix[2], from);
}
/* 3x3 matrix addition with 3x3 matrix */
DO_INLINE void add_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
  add_v3_v3v3(to[0], matrixA[0], matrixB[0]);
  add_v3_v3v3(to[1], matrixA[1], matrixB[1]);
  add_v3_v3v3(to[2], matrixA[2], matrixB[2]);
}
/* A -= B*x + C*y (3x3 matrix sub-addition with 3x3 matrix) */
DO_INLINE void subadd_fmatrixS_fmatrixS(
    float to[3][3], float matrixA[3][3], float aS, float matrixB[3][3], float bS)
{
  VECSUBADDSS(to[0], matrixA[0], aS, matrixB[0], bS);
  VECSUBADDSS(to[1], matrixA[1], aS, matrixB[1], bS);
  VECSUBADDSS(to[2], matrixA[2], aS, matrixB[2], bS);
}
/* A = B - C (3x3 matrix subtraction with 3x3 matrix) */
DO_INLINE void sub_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
  sub_v3_v3v3(to[0], matrixA[0], matrixB[0]);
  sub_v3_v3v3(to[1], matrixA[1], matrixB[1]);
  sub_v3_v3v3(to[2], matrixA[2], matrixB[2]);
}
/////////////////////////////////////////////////////////////////
// special functions
/////////////////////////////////////////////////////////////////
/* 3x3 matrix multiplied+added by a vector */
/* STATUS: verified */
DO_INLINE void muladd_fmatrix_fvector(float to[3], float matrix[3][3], float from[3])
{
  to[0] += dot_v3v3(matrix[0], from);
  to[1] += dot_v3v3(matrix[1], from);
  to[2] += dot_v3v3(matrix[2], from);
}

DO_INLINE void muladd_fmatrixT_fvector(float to[3], float matrix[3][3], float from[3])
{
  to[0] += matrix[0][0] * from[0] + matrix[1][0] * from[1] + matrix[2][0] * from[2];
  to[1] += matrix[0][1] * from[0] + matrix[1][1] * from[1] + matrix[2][1] * from[2];
  to[2] += matrix[0][2] * from[0] + matrix[1][2] * from[1] + matrix[2][2] * from[2];
}

BLI_INLINE void outerproduct(float r[3][3], const float a[3], const float b[3])
{
  mul_v3_v3fl(r[0], a, b[0]);
  mul_v3_v3fl(r[1], a, b[1]);
  mul_v3_v3fl(r[2], a, b[2]);
}

BLI_INLINE void cross_m3_v3m3(float r[3][3], const float v[3], float m[3][3])
{
  cross_v3_v3v3(r[0], v, m[0]);
  cross_v3_v3v3(r[1], v, m[1]);
  cross_v3_v3v3(r[2], v, m[2]);
}

BLI_INLINE void cross_v3_identity(float r[3][3], const float v[3])
{
  r[0][0] = 0.0f;
  r[1][0] = v[2];
  r[2][0] = -v[1];
  r[0][1] = -v[2];
  r[1][1] = 0.0f;
  r[2][1] = v[0];
  r[0][2] = v[1];
  r[1][2] = -v[0];
  r[2][2] = 0.0f;
}

BLI_INLINE void madd_m3_m3fl(float r[3][3], float m[3][3], float f)
{
  r[0][0] += m[0][0] * f;
  r[0][1] += m[0][1] * f;
  r[0][2] += m[0][2] * f;
  r[1][0] += m[1][0] * f;
  r[1][1] += m[1][1] * f;
  r[1][2] += m[1][2] * f;
  r[2][0] += m[2][0] * f;
  r[2][1] += m[2][1] * f;
  r[2][2] += m[2][2] * f;
}

/////////////////////////////////////////////////////////////////

///////////////////////////
// SPARSE SYMMETRIC big matrix with 3x3 matrix entries
///////////////////////////
/* printf a big matrix on console: for debug output */
#  if 0
static void print_bfmatrix(fmatrix3x3 *m3)
{
  unsigned int i = 0;

  for (i = 0; i < m3[0].vcount + m3[0].scount; i++) {
    print_fmatrix(m3[i].m);
  }
}
#  endif

BLI_INLINE void init_fmatrix(fmatrix3x3 *matrix, int r, int c)
{
  matrix->r = r;
  matrix->c = c;
}

/* create big matrix */
DO_INLINE fmatrix3x3 *create_bfmatrix(unsigned int verts, unsigned int springs)
{
  // TODO: check if memory allocation was successful */
  fmatrix3x3 *temp = (fmatrix3x3 *)MEM_callocN(sizeof(fmatrix3x3) * (verts + springs),
                                               "cloth_implicit_alloc_matrix");
  int i;

  temp[0].vcount = verts;
  temp[0].scount = springs;

  /* vertex part of the matrix is diagonal blocks */
  for (i = 0; i < verts; ++i) {
    init_fmatrix(temp + i, i, i);
  }

  return temp;
}
/* delete big matrix */
DO_INLINE void del_bfmatrix(fmatrix3x3 *matrix)
{
  if (matrix != NULL) {
    MEM_freeN(matrix);
  }
}

/* copy big matrix */
DO_INLINE void cp_bfmatrix(fmatrix3x3 *to, fmatrix3x3 *from)
{
  // TODO bounds checking
  memcpy(to, from, sizeof(fmatrix3x3) * (from[0].vcount + from[0].scount));
}

/* init big matrix */
// slow in parallel
DO_INLINE void init_bfmatrix(fmatrix3x3 *matrix, float m3[3][3])
{
  unsigned int i;

  for (i = 0; i < matrix[0].vcount + matrix[0].scount; i++) {
    cp_fmatrix(matrix[i].m, m3);
  }
}

/* init the diagonal of big matrix */
// slow in parallel
DO_INLINE void initdiag_bfmatrix(fmatrix3x3 *matrix, float m3[3][3])
{
  unsigned int i, j;
  float tmatrix[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

  for (i = 0; i < matrix[0].vcount; i++) {
    cp_fmatrix(matrix[i].m, m3);
  }
  for (j = matrix[0].vcount; j < matrix[0].vcount + matrix[0].scount; j++) {
    cp_fmatrix(matrix[j].m, tmatrix);
  }
}

/* SPARSE SYMMETRIC multiply big matrix with long vector*/
/* STATUS: verified */
DO_INLINE void mul_bfmatrix_lfvector(float (*to)[3], fmatrix3x3 *from, lfVector *fLongVector)
{
  unsigned int vcount = from[0].vcount;
  lfVector *temp = create_lfvector(vcount);

  zero_lfvector(to, vcount);

#  pragma omp parallel sections if (vcount > CLOTH_OPENMP_LIMIT)
  {
#  pragma omp section
    {
      for (unsigned int i = from[0].vcount; i < from[0].vcount + from[0].scount; i++) {
        /* This is the lower triangle of the sparse matrix,
         * therefore multiplication occurs with transposed submatrices. */
        muladd_fmatrixT_fvector(to[from[i].c], from[i].m, fLongVector[from[i].r]);
      }
    }
#  pragma omp section
    {
      for (unsigned int i = 0; i < from[0].vcount + from[0].scount; i++) {
        muladd_fmatrix_fvector(temp[from[i].r], from[i].m, fLongVector[from[i].c]);
      }
    }
  }
  add_lfvector_lfvector(to, to, temp, from[0].vcount);

  del_lfvector(temp);
}

/* SPARSE SYMMETRIC sub big matrix with big matrix*/
/* A -= B * float + C * float --> for big matrix */
/* VERIFIED */
DO_INLINE void subadd_bfmatrixS_bfmatrixS(
    fmatrix3x3 *to, fmatrix3x3 *from, float aS, fmatrix3x3 *matrix, float bS)
{
  unsigned int i = 0;

  /* process diagonal elements */
  for (i = 0; i < matrix[0].vcount + matrix[0].scount; i++) {
    subadd_fmatrixS_fmatrixS(to[i].m, from[i].m, aS, matrix[i].m, bS);
  }
}

///////////////////////////////////////////////////////////////////
// simulator start
///////////////////////////////////////////////////////////////////

typedef struct Implicit_Data {
  /* inputs */
  fmatrix3x3 *bigI;        /* identity (constant) */
  fmatrix3x3 *tfm;         /* local coordinate transform */
  fmatrix3x3 *M;           /* masses */
  lfVector *F;             /* forces */
  fmatrix3x3 *dFdV, *dFdX; /* force jacobians */
  int num_blocks;          /* number of off-diagonal blocks (springs) */

  /* motion state data */
  lfVector *X, *Xnew; /* positions */
  lfVector *V, *Vnew; /* velocities */

  /* internal solver data */
  lfVector *B;   /* B for A*dV = B */
  fmatrix3x3 *A; /* A for A*dV = B */

  lfVector *dV;         /* velocity change (solution of A*dV = B) */
  lfVector *z;          /* target velocity in constrained directions */
  fmatrix3x3 *S;        /* filtering matrix for constraints */
  fmatrix3x3 *P, *Pinv; /* pre-conditioning matrix */
} Implicit_Data;

Implicit_Data *BPH_mass_spring_solver_create(int numverts, int numsprings)
{
  Implicit_Data *id = (Implicit_Data *)MEM_callocN(sizeof(Implicit_Data), "implicit vecmat");

  /* process diagonal elements */
  id->tfm = create_bfmatrix(numverts, 0);
  id->A = create_bfmatrix(numverts, numsprings);
  id->dFdV = create_bfmatrix(numverts, numsprings);
  id->dFdX = create_bfmatrix(numverts, numsprings);
  id->S = create_bfmatrix(numverts, 0);
  id->Pinv = create_bfmatrix(numverts, numsprings);
  id->P = create_bfmatrix(numverts, numsprings);
  id->bigI = create_bfmatrix(numverts, numsprings);  // TODO 0 springs
  id->M = create_bfmatrix(numverts, numsprings);
  id->X = create_lfvector(numverts);
  id->Xnew = create_lfvector(numverts);
  id->V = create_lfvector(numverts);
  id->Vnew = create_lfvector(numverts);
  id->F = create_lfvector(numverts);
  id->B = create_lfvector(numverts);
  id->dV = create_lfvector(numverts);
  id->z = create_lfvector(numverts);

  initdiag_bfmatrix(id->bigI, I);

  return id;
}

void BPH_mass_spring_solver_free(Implicit_Data *id)
{
  del_bfmatrix(id->tfm);
  del_bfmatrix(id->A);
  del_bfmatrix(id->dFdV);
  del_bfmatrix(id->dFdX);
  del_bfmatrix(id->S);
  del_bfmatrix(id->P);
  del_bfmatrix(id->Pinv);
  del_bfmatrix(id->bigI);
  del_bfmatrix(id->M);

  del_lfvector(id->X);
  del_lfvector(id->Xnew);
  del_lfvector(id->V);
  del_lfvector(id->Vnew);
  del_lfvector(id->F);
  del_lfvector(id->B);
  del_lfvector(id->dV);
  del_lfvector(id->z);

  MEM_freeN(id);
}

/* ==== Transformation from/to root reference frames ==== */

BLI_INLINE void world_to_root_v3(Implicit_Data *data, int index, float r[3], const float v[3])
{
  copy_v3_v3(r, v);
  mul_transposed_m3_v3(data->tfm[index].m, r);
}

BLI_INLINE void root_to_world_v3(Implicit_Data *data, int index, float r[3], const float v[3])
{
  mul_v3_m3v3(r, data->tfm[index].m, v);
}

BLI_INLINE void world_to_root_m3(Implicit_Data *data, int index, float r[3][3], float m[3][3])
{
  float trot[3][3];
  copy_m3_m3(trot, data->tfm[index].m);
  transpose_m3(trot);
  mul_m3_m3m3(r, trot, m);
}

BLI_INLINE void root_to_world_m3(Implicit_Data *data, int index, float r[3][3], float m[3][3])
{
  mul_m3_m3m3(r, data->tfm[index].m, m);
}

/* ================================ */

DO_INLINE void filter(lfVector *V, fmatrix3x3 *S)
{
  unsigned int i = 0;

  for (i = 0; i < S[0].vcount; i++) {
    mul_m3_v3(S[i].m, V[S[i].r]);
  }
}

/* this version of the CG algorithm does not work very well with partial constraints
 * (where S has non-zero elements). */
#  if 0
static int cg_filtered(lfVector *ldV, fmatrix3x3 *lA, lfVector *lB, lfVector *z, fmatrix3x3 *S)
{
  // Solves for unknown X in equation AX=B
  unsigned int conjgrad_loopcount = 0, conjgrad_looplimit = 100;
  float conjgrad_epsilon = 0.0001f /* , conjgrad_lasterror=0 */ /* UNUSED */;
  lfVector *q, *d, *tmp, *r;
  float s, starget, a, s_prev;
  unsigned int numverts = lA[0].vcount;
  q = create_lfvector(numverts);
  d = create_lfvector(numverts);
  tmp = create_lfvector(numverts);
  r = create_lfvector(numverts);

  // zero_lfvector(ldV, CLOTHPARTICLES);
  filter(ldV, S);

  add_lfvector_lfvector(ldV, ldV, z, numverts);

  // r = B - Mul(tmp, A, X);    // just use B if X known to be zero
  cp_lfvector(r, lB, numverts);
  mul_bfmatrix_lfvector(tmp, lA, ldV);
  sub_lfvector_lfvector(r, r, tmp, numverts);

  filter(r, S);

  cp_lfvector(d, r, numverts);

  s = dot_lfvector(r, r, numverts);
  starget = s * sqrtf(conjgrad_epsilon);

  while (s > starget && conjgrad_loopcount < conjgrad_looplimit) {
    // Mul(q, A, d); // q = A*d;
    mul_bfmatrix_lfvector(q, lA, d);

    filter(q, S);

    a = s / dot_lfvector(d, q, numverts);

    // X = X + d*a;
    add_lfvector_lfvectorS(ldV, ldV, d, a, numverts);

    // r = r - q*a;
    sub_lfvector_lfvectorS(r, r, q, a, numverts);

    s_prev = s;
    s = dot_lfvector(r, r, numverts);

    //d = r+d*(s/s_prev);
    add_lfvector_lfvectorS(d, r, d, (s / s_prev), numverts);

    filter(d, S);

    conjgrad_loopcount++;
  }
  /* conjgrad_lasterror = s; */ /* UNUSED */

  del_lfvector(q);
  del_lfvector(d);
  del_lfvector(tmp);
  del_lfvector(r);
  // printf("W/O conjgrad_loopcount: %d\n", conjgrad_loopcount);

  return conjgrad_loopcount <
         conjgrad_looplimit;  // true means we reached desired accuracy in given time - ie stable
}
#  endif

static int cg_filtered(lfVector *ldV,
                       fmatrix3x3 *lA,
                       lfVector *lB,
                       lfVector *z,
                       fmatrix3x3 *S,
                       ImplicitSolverResult *result)
{
  // Solves for unknown X in equation AX=B
  unsigned int conjgrad_loopcount = 0, conjgrad_looplimit = 100;
  float conjgrad_epsilon = 0.01f;

  unsigned int numverts = lA[0].vcount;
  lfVector *fB = create_lfvector(numverts);
  lfVector *AdV = create_lfvector(numverts);
  lfVector *r = create_lfvector(numverts);
  lfVector *c = create_lfvector(numverts);
  lfVector *q = create_lfvector(numverts);
  lfVector *s = create_lfvector(numverts);
  float bnorm2, delta_new, delta_old, delta_target, alpha;

  cp_lfvector(ldV, z, numverts);

  /* d0 = filter(B)^T * P * filter(B) */
  cp_lfvector(fB, lB, numverts);
  filter(fB, S);
  bnorm2 = dot_lfvector(fB, fB, numverts);
  delta_target = conjgrad_epsilon * conjgrad_epsilon * bnorm2;

  /* r = filter(B - A * dV) */
  mul_bfmatrix_lfvector(AdV, lA, ldV);
  sub_lfvector_lfvector(r, lB, AdV, numverts);
  filter(r, S);

  /* c = filter(P^-1 * r) */
  cp_lfvector(c, r, numverts);
  filter(c, S);

  /* delta = r^T * c */
  delta_new = dot_lfvector(r, c, numverts);

#  ifdef IMPLICIT_PRINT_SOLVER_INPUT_OUTPUT
  printf("==== A ====\n");
  print_bfmatrix(lA);
  printf("==== z ====\n");
  print_lvector(z, numverts);
  printf("==== B ====\n");
  print_lvector(lB, numverts);
  printf("==== S ====\n");
  print_bfmatrix(S);
#  endif

  while (delta_new > delta_target && conjgrad_loopcount < conjgrad_looplimit) {
    mul_bfmatrix_lfvector(q, lA, c);
    filter(q, S);

    alpha = delta_new / dot_lfvector(c, q, numverts);

    add_lfvector_lfvectorS(ldV, ldV, c, alpha, numverts);

    add_lfvector_lfvectorS(r, r, q, -alpha, numverts);

    /* s = P^-1 * r */
    cp_lfvector(s, r, numverts);
    delta_old = delta_new;
    delta_new = dot_lfvector(r, s, numverts);

    add_lfvector_lfvectorS(c, s, c, delta_new / delta_old, numverts);
    filter(c, S);

    conjgrad_loopcount++;
  }

#  ifdef IMPLICIT_PRINT_SOLVER_INPUT_OUTPUT
  printf("==== dV ====\n");
  print_lvector(ldV, numverts);
  printf("========\n");
#  endif

  del_lfvector(fB);
  del_lfvector(AdV);
  del_lfvector(r);
  del_lfvector(c);
  del_lfvector(q);
  del_lfvector(s);
  // printf("W/O conjgrad_loopcount: %d\n", conjgrad_loopcount);

  result->status = conjgrad_loopcount < conjgrad_looplimit ? BPH_SOLVER_SUCCESS :
                                                             BPH_SOLVER_NO_CONVERGENCE;
  result->iterations = conjgrad_loopcount;
  result->error = bnorm2 > 0.0f ? sqrtf(delta_new / bnorm2) : 0.0f;

  return conjgrad_loopcount <
         conjgrad_looplimit;  // true means we reached desired accuracy in given time - ie stable
}

#  if 0
// block diagonalizer
DO_INLINE void BuildPPinv(fmatrix3x3 *lA, fmatrix3x3 *P, fmatrix3x3 *Pinv)
{
  unsigned int i = 0;

  // Take only the diagonal blocks of A
  // #pragma omp parallel for private(i) if (lA[0].vcount > CLOTH_OPENMP_LIMIT)
  for (i = 0; i < lA[0].vcount; i++) {
    // block diagonalizer
    cp_fmatrix(P[i].m, lA[i].m);
    inverse_fmatrix(Pinv[i].m, P[i].m);
  }
}

#    if 0
// version 1.3
static int cg_filtered_pre(lfVector *dv,
                           fmatrix3x3 *lA,
                           lfVector *lB,
                           lfVector *z,
                           fmatrix3x3 *S,
                           fmatrix3x3 *P,
                           fmatrix3x3 *Pinv)
{
  unsigned int numverts = lA[0].vcount, iterations = 0, conjgrad_looplimit = 100;
  float delta0 = 0, deltaNew = 0, deltaOld = 0, alpha = 0;
  float conjgrad_epsilon = 0.0001;  // 0.2 is dt for steps=5
  lfVector *r = create_lfvector(numverts);
  lfVector *p = create_lfvector(numverts);
  lfVector *s = create_lfvector(numverts);
  lfVector *h = create_lfvector(numverts);

  BuildPPinv(lA, P, Pinv);

  filter(dv, S);
  add_lfvector_lfvector(dv, dv, z, numverts);

  mul_bfmatrix_lfvector(r, lA, dv);
  sub_lfvector_lfvector(r, lB, r, numverts);
  filter(r, S);

  mul_prevfmatrix_lfvector(p, Pinv, r);
  filter(p, S);

  deltaNew = dot_lfvector(r, p, numverts);

  delta0 = deltaNew * sqrt(conjgrad_epsilon);

#      ifdef DEBUG_TIME
  double start = PIL_check_seconds_timer();
#      endif

  while ((deltaNew > delta0) && (iterations < conjgrad_looplimit)) {
    iterations++;

    mul_bfmatrix_lfvector(s, lA, p);
    filter(s, S);

    alpha = deltaNew / dot_lfvector(p, s, numverts);

    add_lfvector_lfvectorS(dv, dv, p, alpha, numverts);

    add_lfvector_lfvectorS(r, r, s, -alpha, numverts);

    mul_prevfmatrix_lfvector(h, Pinv, r);
    filter(h, S);

    deltaOld = deltaNew;

    deltaNew = dot_lfvector(r, h, numverts);

    add_lfvector_lfvectorS(p, h, p, deltaNew / deltaOld, numverts);

    filter(p, S);
  }

#      ifdef DEBUG_TIME
  double end = PIL_check_seconds_timer();
  printf("cg_filtered_pre time: %f\n", (float)(end - start));
#      endif

  del_lfvector(h);
  del_lfvector(s);
  del_lfvector(p);
  del_lfvector(r);

  printf("iterations: %d\n", iterations);

  return iterations < conjgrad_looplimit;
}
#    endif

// version 1.4
static int cg_filtered_pre(lfVector *dv,
                           fmatrix3x3 *lA,
                           lfVector *lB,
                           lfVector *z,
                           fmatrix3x3 *S,
                           fmatrix3x3 *P,
                           fmatrix3x3 *Pinv,
                           fmatrix3x3 *bigI)
{
  unsigned int numverts = lA[0].vcount, iterations = 0, conjgrad_looplimit = 100;
  float delta0 = 0, deltaNew = 0, deltaOld = 0, alpha = 0, tol = 0;
  lfVector *r = create_lfvector(numverts);
  lfVector *p = create_lfvector(numverts);
  lfVector *s = create_lfvector(numverts);
  lfVector *h = create_lfvector(numverts);
  lfVector *bhat = create_lfvector(numverts);
  lfVector *btemp = create_lfvector(numverts);

  BuildPPinv(lA, P, Pinv);

  initdiag_bfmatrix(bigI, I);
  sub_bfmatrix_Smatrix(bigI, bigI, S);

  // x = Sx_0+(I-S)z
  filter(dv, S);
  add_lfvector_lfvector(dv, dv, z, numverts);

  // b_hat = S(b-A(I-S)z)
  mul_bfmatrix_lfvector(r, lA, z);
  mul_bfmatrix_lfvector(bhat, bigI, r);
  sub_lfvector_lfvector(bhat, lB, bhat, numverts);

  // r = S(b-Ax)
  mul_bfmatrix_lfvector(r, lA, dv);
  sub_lfvector_lfvector(r, lB, r, numverts);
  filter(r, S);

  // p = SP^-1r
  mul_prevfmatrix_lfvector(p, Pinv, r);
  filter(p, S);

  // delta0 = bhat^TP^-1bhat
  mul_prevfmatrix_lfvector(btemp, Pinv, bhat);
  delta0 = dot_lfvector(bhat, btemp, numverts);

  // deltaNew = r^TP
  deltaNew = dot_lfvector(r, p, numverts);

#    if 0
  filter(dv, S);
  add_lfvector_lfvector(dv, dv, z, numverts);

  mul_bfmatrix_lfvector(r, lA, dv);
  sub_lfvector_lfvector(r, lB, r, numverts);
  filter(r, S);

  mul_prevfmatrix_lfvector(p, Pinv, r);
  filter(p, S);

  deltaNew = dot_lfvector(r, p, numverts);

  delta0 = deltaNew * sqrt(conjgrad_epsilon);
#    endif

#    ifdef DEBUG_TIME
  double start = PIL_check_seconds_timer();
#    endif

  tol = (0.01 * 0.2);

  while ((deltaNew > delta0 * tol * tol) && (iterations < conjgrad_looplimit)) {
    iterations++;

    mul_bfmatrix_lfvector(s, lA, p);
    filter(s, S);

    alpha = deltaNew / dot_lfvector(p, s, numverts);

    add_lfvector_lfvectorS(dv, dv, p, alpha, numverts);

    add_lfvector_lfvectorS(r, r, s, -alpha, numverts);

    mul_prevfmatrix_lfvector(h, Pinv, r);
    filter(h, S);

    deltaOld = deltaNew;

    deltaNew = dot_lfvector(r, h, numverts);

    add_lfvector_lfvectorS(p, h, p, deltaNew / deltaOld, numverts);

    filter(p, S);
  }

#    ifdef DEBUG_TIME
  double end = PIL_check_seconds_timer();
  printf("cg_filtered_pre time: %f\n", (float)(end - start));
#    endif

  del_lfvector(btemp);
  del_lfvector(bhat);
  del_lfvector(h);
  del_lfvector(s);
  del_lfvector(p);
  del_lfvector(r);

  // printf("iterations: %d\n", iterations);

  return iterations < conjgrad_looplimit;
}
#  endif

bool BPH_mass_spring_solve_velocities(Implicit_Data *data, float dt, ImplicitSolverResult *result)
{
  unsigned int numverts = data->dFdV[0].vcount;

  lfVector *dFdXmV = create_lfvector(numverts);
  zero_lfvector(data->dV, numverts);

  cp_bfmatrix(data->A, data->M);

  subadd_bfmatrixS_bfmatrixS(data->A, data->dFdV, dt, data->dFdX, (dt * dt));

  mul_bfmatrix_lfvector(dFdXmV, data->dFdX, data->V);

  add_lfvectorS_lfvectorS(data->B, data->F, dt, dFdXmV, (dt * dt), numverts);

#  ifdef DEBUG_TIME
  double start = PIL_check_seconds_timer();
#  endif

  cg_filtered(data->dV,
              data->A,
              data->B,
              data->z,
              data->S,
              result); /* conjugate gradient algorithm to solve Ax=b */
  // cg_filtered_pre(id->dV, id->A, id->B, id->z, id->S, id->P, id->Pinv, id->bigI);

#  ifdef DEBUG_TIME
  double end = PIL_check_seconds_timer();
  printf("cg_filtered calc time: %f\n", (float)(end - start));
#  endif

  // advance velocities
  add_lfvector_lfvector(data->Vnew, data->V, data->dV, numverts);

  del_lfvector(dFdXmV);

  return result->status == BPH_SOLVER_SUCCESS;
}

bool BPH_mass_spring_solve_positions(Implicit_Data *data, float dt)
{
  int numverts = data->M[0].vcount;

  // advance positions
  add_lfvector_lfvectorS(data->Xnew, data->X, data->Vnew, dt, numverts);

  return true;
}

void BPH_mass_spring_apply_result(Implicit_Data *data)
{
  int numverts = data->M[0].vcount;
  cp_lfvector(data->X, data->Xnew, numverts);
  cp_lfvector(data->V, data->Vnew, numverts);
}

void BPH_mass_spring_set_vertex_mass(Implicit_Data *data, int index, float mass)
{
  unit_m3(data->M[index].m);
  mul_m3_fl(data->M[index].m, mass);
}

void BPH_mass_spring_set_rest_transform(Implicit_Data *data, int index, float tfm[3][3])
{
#  ifdef CLOTH_ROOT_FRAME
  copy_m3_m3(data->tfm[index].m, tfm);
#  else
  unit_m3(data->tfm[index].m);
  (void)tfm;
#  endif
}

void BPH_mass_spring_set_motion_state(Implicit_Data *data,
                                      int index,
                                      const float x[3],
                                      const float v[3])
{
  world_to_root_v3(data, index, data->X[index], x);
  world_to_root_v3(data, index, data->V[index], v);
}

void BPH_mass_spring_set_position(Implicit_Data *data, int index, const float x[3])
{
  world_to_root_v3(data, index, data->X[index], x);
}

void BPH_mass_spring_set_velocity(Implicit_Data *data, int index, const float v[3])
{
  world_to_root_v3(data, index, data->V[index], v);
}

void BPH_mass_spring_get_motion_state(struct Implicit_Data *data,
                                      int index,
                                      float x[3],
                                      float v[3])
{
  if (x)
    root_to_world_v3(data, index, x, data->X[index]);
  if (v)
    root_to_world_v3(data, index, v, data->V[index]);
}

void BPH_mass_spring_get_position(struct Implicit_Data *data, int index, float x[3])
{
  root_to_world_v3(data, index, x, data->X[index]);
}

void BPH_mass_spring_get_new_position(struct Implicit_Data *data, int index, float x[3])
{
  root_to_world_v3(data, index, x, data->Xnew[index]);
}

void BPH_mass_spring_set_new_position(struct Implicit_Data *data, int index, const float x[3])
{
  world_to_root_v3(data, index, data->Xnew[index], x);
}

void BPH_mass_spring_get_new_velocity(struct Implicit_Data *data, int index, float v[3])
{
  root_to_world_v3(data, index, v, data->Vnew[index]);
}

void BPH_mass_spring_set_new_velocity(struct Implicit_Data *data, int index, const float v[3])
{
  world_to_root_v3(data, index, data->Vnew[index], v);
}

/* -------------------------------- */

static int BPH_mass_spring_add_block(Implicit_Data *data, int v1, int v2)
{
  int s = data->M[0].vcount + data->num_blocks; /* index from array start */
  BLI_assert(s < data->M[0].vcount + data->M[0].scount);
  ++data->num_blocks;

  /* tfm and S don't have spring entries (diagonal blocks only) */
  init_fmatrix(data->bigI + s, v1, v2);
  init_fmatrix(data->M + s, v1, v2);
  init_fmatrix(data->dFdX + s, v1, v2);
  init_fmatrix(data->dFdV + s, v1, v2);
  init_fmatrix(data->A + s, v1, v2);
  init_fmatrix(data->P + s, v1, v2);
  init_fmatrix(data->Pinv + s, v1, v2);

  return s;
}

void BPH_mass_spring_clear_constraints(Implicit_Data *data)
{
  int i, numverts = data->S[0].vcount;
  for (i = 0; i < numverts; ++i) {
    unit_m3(data->S[i].m);
    zero_v3(data->z[i]);
  }
}

void BPH_mass_spring_add_constraint_ndof0(Implicit_Data *data, int index, const float dV[3])
{
  zero_m3(data->S[index].m);

  world_to_root_v3(data, index, data->z[index], dV);
}

void BPH_mass_spring_add_constraint_ndof1(
    Implicit_Data *data, int index, const float c1[3], const float c2[3], const float dV[3])
{
  float m[3][3], p[3], q[3], u[3], cmat[3][3];

  world_to_root_v3(data, index, p, c1);
  mul_fvectorT_fvector(cmat, p, p);
  sub_m3_m3m3(m, I, cmat);

  world_to_root_v3(data, index, q, c2);
  mul_fvectorT_fvector(cmat, q, q);
  sub_m3_m3m3(m, m, cmat);

  /* XXX not sure but multiplication should work here */
  copy_m3_m3(data->S[index].m, m);
  //  mul_m3_m3m3(data->S[index].m, data->S[index].m, m);

  world_to_root_v3(data, index, u, dV);
  add_v3_v3(data->z[index], u);
}

void BPH_mass_spring_add_constraint_ndof2(Implicit_Data *data,
                                          int index,
                                          const float c1[3],
                                          const float dV[3])
{
  float m[3][3], p[3], u[3], cmat[3][3];

  world_to_root_v3(data, index, p, c1);
  mul_fvectorT_fvector(cmat, p, p);
  sub_m3_m3m3(m, I, cmat);

  copy_m3_m3(data->S[index].m, m);
  //  mul_m3_m3m3(data->S[index].m, data->S[index].m, m);

  world_to_root_v3(data, index, u, dV);
  add_v3_v3(data->z[index], u);
}

void BPH_mass_spring_clear_forces(Implicit_Data *data)
{
  int numverts = data->M[0].vcount;
  zero_lfvector(data->F, numverts);
  init_bfmatrix(data->dFdX, ZERO);
  init_bfmatrix(data->dFdV, ZERO);

  data->num_blocks = 0;
}

void BPH_mass_spring_force_reference_frame(Implicit_Data *data,
                                           int index,
                                           const float acceleration[3],
                                           const float omega[3],
                                           const float domega_dt[3],
                                           float mass)
{
#  ifdef CLOTH_ROOT_FRAME
  float acc[3], w[3], dwdt[3];
  float f[3], dfdx[3][3], dfdv[3][3];
  float euler[3], coriolis[3], centrifugal[3], rotvel[3];
  float deuler[3][3], dcoriolis[3][3], dcentrifugal[3][3], drotvel[3][3];

  world_to_root_v3(data, index, acc, acceleration);
  world_to_root_v3(data, index, w, omega);
  world_to_root_v3(data, index, dwdt, domega_dt);

  cross_v3_v3v3(euler, dwdt, data->X[index]);
  cross_v3_v3v3(coriolis, w, data->V[index]);
  mul_v3_fl(coriolis, 2.0f);
  cross_v3_v3v3(rotvel, w, data->X[index]);
  cross_v3_v3v3(centrifugal, w, rotvel);

  sub_v3_v3v3(f, acc, euler);
  sub_v3_v3(f, coriolis);
  sub_v3_v3(f, centrifugal);

  mul_v3_fl(f, mass); /* F = m * a */

  cross_v3_identity(deuler, dwdt);
  cross_v3_identity(dcoriolis, w);
  mul_m3_fl(dcoriolis, 2.0f);
  cross_v3_identity(drotvel, w);
  cross_m3_v3m3(dcentrifugal, w, drotvel);

  add_m3_m3m3(dfdx, deuler, dcentrifugal);
  negate_m3(dfdx);
  mul_m3_fl(dfdx, mass);

  copy_m3_m3(dfdv, dcoriolis);
  negate_m3(dfdv);
  mul_m3_fl(dfdv, mass);

  add_v3_v3(data->F[index], f);
  add_m3_m3m3(data->dFdX[index].m, data->dFdX[index].m, dfdx);
  add_m3_m3m3(data->dFdV[index].m, data->dFdV[index].m, dfdv);
#  else
  (void)data;
  (void)index;
  (void)acceleration;
  (void)omega;
  (void)domega_dt;
#  endif
}

void BPH_mass_spring_force_gravity(Implicit_Data *data, int index, float mass, const float g[3])
{
  /* force = mass * acceleration (in this case: gravity) */
  float f[3];
  world_to_root_v3(data, index, f, g);
  mul_v3_fl(f, mass);

  add_v3_v3(data->F[index], f);
}

void BPH_mass_spring_force_drag(Implicit_Data *data, float drag)
{
  int i, numverts = data->M[0].vcount;
  for (i = 0; i < numverts; i++) {
    float tmp[3][3];

    /* NB: uses root space velocity, no need to transform */
    madd_v3_v3fl(data->F[i], data->V[i], -drag);

    copy_m3_m3(tmp, I);
    mul_m3_fl(tmp, -drag);
    add_m3_m3m3(data->dFdV[i].m, data->dFdV[i].m, tmp);
  }
}

void BPH_mass_spring_force_extern(
    struct Implicit_Data *data, int i, const float f[3], float dfdx[3][3], float dfdv[3][3])
{
  float tf[3], tdfdx[3][3], tdfdv[3][3];
  world_to_root_v3(data, i, tf, f);
  world_to_root_m3(data, i, tdfdx, dfdx);
  world_to_root_m3(data, i, tdfdv, dfdv);

  add_v3_v3(data->F[i], tf);
  add_m3_m3m3(data->dFdX[i].m, data->dFdX[i].m, tdfdx);
  add_m3_m3m3(data->dFdV[i].m, data->dFdV[i].m, tdfdv);
}

static float calc_nor_area_tri(float nor[3],
                               const float v1[3],
                               const float v2[3],
                               const float v3[3])
{
  float n1[3], n2[3];

  sub_v3_v3v3(n1, v1, v2);
  sub_v3_v3v3(n2, v2, v3);

  cross_v3_v3v3(nor, n1, n2);
  return normalize_v3(nor);
}

/* XXX does not support force jacobians yet, since the effector system does not provide them either
 */
void BPH_mass_spring_force_face_wind(
    Implicit_Data *data, int v1, int v2, int v3, const float (*winvec)[3])
{
  const float effector_scale = 0.02f;
  float win[3], nor[3], area;
  float factor;

  /* calculate face normal and area */
  area = calc_nor_area_tri(nor, data->X[v1], data->X[v2], data->X[v3]);
  factor = effector_scale * area / 3.0f;

  world_to_root_v3(data, v1, win, winvec[v1]);
  madd_v3_v3fl(data->F[v1], nor, factor * dot_v3v3(win, nor));

  world_to_root_v3(data, v2, win, winvec[v2]);
  madd_v3_v3fl(data->F[v2], nor, factor * dot_v3v3(win, nor));

  world_to_root_v3(data, v3, win, winvec[v3]);
  madd_v3_v3fl(data->F[v3], nor, factor * dot_v3v3(win, nor));
}

static void edge_wind_vertex(const float dir[3],
                             float length,
                             float radius,
                             const float wind[3],
                             float f[3],
                             float UNUSED(dfdx[3][3]),
                             float UNUSED(dfdv[3][3]))
{
  const float density = 0.01f; /* XXX arbitrary value, corresponds to effect of air density */
  float cos_alpha, sin_alpha, cross_section;
  float windlen = len_v3(wind);

  if (windlen == 0.0f) {
    zero_v3(f);
    return;
  }

  /* angle of wind direction to edge */
  cos_alpha = dot_v3v3(wind, dir) / windlen;
  sin_alpha = sqrtf(1.0f - cos_alpha * cos_alpha);
  cross_section = radius * ((float)M_PI * radius * sin_alpha + length * cos_alpha);

  mul_v3_v3fl(f, wind, density * cross_section);
}

void BPH_mass_spring_force_edge_wind(
    Implicit_Data *data, int v1, int v2, float radius1, float radius2, const float (*winvec)[3])
{
  float win[3], dir[3], length;
  float f[3], dfdx[3][3], dfdv[3][3];

  sub_v3_v3v3(dir, data->X[v1], data->X[v2]);
  length = normalize_v3(dir);

  world_to_root_v3(data, v1, win, winvec[v1]);
  edge_wind_vertex(dir, length, radius1, win, f, dfdx, dfdv);
  add_v3_v3(data->F[v1], f);

  world_to_root_v3(data, v2, win, winvec[v2]);
  edge_wind_vertex(dir, length, radius2, win, f, dfdx, dfdv);
  add_v3_v3(data->F[v2], f);
}

void BPH_mass_spring_force_vertex_wind(Implicit_Data *data,
                                       int v,
                                       float UNUSED(radius),
                                       const float (*winvec)[3])
{
  const float density = 0.01f; /* XXX arbitrary value, corresponds to effect of air density */

  float wind[3];
  float f[3];

  world_to_root_v3(data, v, wind, winvec[v]);
  mul_v3_v3fl(f, wind, density);
  add_v3_v3(data->F[v], f);
}

BLI_INLINE void dfdx_spring(float to[3][3], const float dir[3], float length, float L, float k)
{
  // dir is unit length direction, rest is spring's restlength, k is spring constant.
  // return  ( (I-outerprod(dir, dir))*Min(1.0f, rest/length) - I) * -k;
  outerproduct(to, dir, dir);
  sub_m3_m3m3(to, I, to);

  mul_m3_fl(to, (L / length));
  sub_m3_m3m3(to, to, I);
  mul_m3_fl(to, k);
}

/* unused */
#  if 0
BLI_INLINE void dfdx_damp(float to[3][3],
                          const float dir[3],
                          float length,
                          const float vel[3],
                          float rest,
                          float damping)
{
  // inner spring damping   vel is the relative velocity  of the endpoints.
  //  return (I-outerprod(dir, dir)) * (-damping * -(dot(dir, vel)/Max(length, rest)));
  mul_fvectorT_fvector(to, dir, dir);
  sub_fmatrix_fmatrix(to, I, to);
  mul_fmatrix_S(to, (-damping * -(dot_v3v3(dir, vel) / MAX2(length, rest))));
}
#  endif

BLI_INLINE void dfdv_damp(float to[3][3], const float dir[3], float damping)
{
  // derivative of force wrt velocity
  outerproduct(to, dir, dir);
  mul_m3_fl(to, -damping);
}

BLI_INLINE float fb(float length, float L)
{
  float x = length / L;
  float xx = x * x;
  float xxx = xx * x;
  float xxxx = xxx * x;
  return (-11.541f * xxxx + 34.193f * xxx - 39.083f * xx + 23.116f * x - 9.713f);
}

BLI_INLINE float fbderiv(float length, float L)
{
  float x = length / L;
  float xx = x * x;
  float xxx = xx * x;
  return (-46.164f * xxx + 102.579f * xx - 78.166f * x + 23.116f);
}

BLI_INLINE float fbstar(float length, float L, float kb, float cb)
{
  float tempfb_fl = kb * fb(length, L);
  float fbstar_fl = cb * (length - L);

  if (tempfb_fl < fbstar_fl)
    return fbstar_fl;
  else
    return tempfb_fl;
}

// function to calculae bending spring force (taken from Choi & Co)
BLI_INLINE float fbstar_jacobi(float length, float L, float kb, float cb)
{
  float tempfb_fl = kb * fb(length, L);
  float fbstar_fl = cb * (length - L);

  if (tempfb_fl < fbstar_fl) {
    return -cb;
  }
  else {
    return -kb * fbderiv(length, L);
  }
}

/* calculate elonglation */
BLI_INLINE bool spring_length(Implicit_Data *data,
                              int i,
                              int j,
                              float r_extent[3],
                              float r_dir[3],
                              float *r_length,
                              float r_vel[3])
{
  sub_v3_v3v3(r_extent, data->X[j], data->X[i]);
  sub_v3_v3v3(r_vel, data->V[j], data->V[i]);
  *r_length = len_v3(r_extent);

  if (*r_length > ALMOST_ZERO) {
#  if 0
    if (length > L) {
      if ((clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED) &&
          (((length - L) * 100.0f / L) > clmd->sim_parms->maxspringlen)) {
        // cut spring!
        s->flags |= CSPRING_FLAG_DEACTIVATE;
        return false;
      }
    }
#  endif
    mul_v3_v3fl(r_dir, r_extent, 1.0f / (*r_length));
  }
  else {
    zero_v3(r_dir);
  }

  return true;
}

BLI_INLINE void apply_spring(
    Implicit_Data *data, int i, int j, const float f[3], float dfdx[3][3], float dfdv[3][3])
{
  int block_ij = BPH_mass_spring_add_block(data, i, j);

  add_v3_v3(data->F[i], f);
  sub_v3_v3(data->F[j], f);

  add_m3_m3m3(data->dFdX[i].m, data->dFdX[i].m, dfdx);
  add_m3_m3m3(data->dFdX[j].m, data->dFdX[j].m, dfdx);
  sub_m3_m3m3(data->dFdX[block_ij].m, data->dFdX[block_ij].m, dfdx);

  add_m3_m3m3(data->dFdV[i].m, data->dFdV[i].m, dfdv);
  add_m3_m3m3(data->dFdV[j].m, data->dFdV[j].m, dfdv);
  sub_m3_m3m3(data->dFdV[block_ij].m, data->dFdV[block_ij].m, dfdv);
}

bool BPH_mass_spring_force_spring_linear(Implicit_Data *data,
                                         int i,
                                         int j,
                                         float restlen,
                                         float stiffness_tension,
                                         float damping_tension,
                                         float stiffness_compression,
                                         float damping_compression,
                                         bool resist_compress,
                                         bool new_compress,
                                         float clamp_force)
{
  float extent[3], length, dir[3], vel[3];
  float f[3], dfdx[3][3], dfdv[3][3];
  float damping = 0;

  // calculate elonglation
  spring_length(data, i, j, extent, dir, &length, vel);

  /* This code computes not only the force, but also its derivative.
   * Zero derivative effectively disables the spring for the implicit solver.
   * Thus length > restlen makes cloth unconstrained at the start of simulation. */
  if ((length >= restlen && length > 0) || resist_compress) {
    float stretch_force;

    damping = damping_tension;

    stretch_force = stiffness_tension * (length - restlen);
    if (clamp_force > 0.0f && stretch_force > clamp_force) {
      stretch_force = clamp_force;
    }
    mul_v3_v3fl(f, dir, stretch_force);

    dfdx_spring(dfdx, dir, length, restlen, stiffness_tension);
  }
  else if (new_compress) {
    /* This is based on the Choi and Ko bending model,
     * which works surprisingly well for compression. */
    float kb = stiffness_compression;
    float cb = kb; /* cb equal to kb seems to work, but a factor can be added if necessary */

    damping = damping_compression;

    mul_v3_v3fl(f, dir, fbstar(length, restlen, kb, cb));

    outerproduct(dfdx, dir, dir);
    mul_m3_fl(dfdx, fbstar_jacobi(length, restlen, kb, cb));
  }
  else {
    return false;
  }

  madd_v3_v3fl(f, dir, damping * dot_v3v3(vel, dir));
  dfdv_damp(dfdv, dir, damping);

  apply_spring(data, i, j, f, dfdx, dfdv);

  return true;
}

/* See "Stable but Responsive Cloth" (Choi, Ko 2005) */
bool BPH_mass_spring_force_spring_bending(
    Implicit_Data *data, int i, int j, float restlen, float kb, float cb)
{
  float extent[3], length, dir[3], vel[3];

  // calculate elonglation
  spring_length(data, i, j, extent, dir, &length, vel);

  if (length < restlen) {
    float f[3], dfdx[3][3], dfdv[3][3];

    mul_v3_v3fl(f, dir, fbstar(length, restlen, kb, cb));

    outerproduct(dfdx, dir, dir);
    mul_m3_fl(dfdx, fbstar_jacobi(length, restlen, kb, cb));

    /* XXX damping not supported */
    zero_m3(dfdv);

    apply_spring(data, i, j, f, dfdx, dfdv);

    return true;
  }
  else {
    return false;
  }
}

BLI_INLINE void poly_avg(lfVector *data, int *inds, int len, float r_avg[3])
{
  float fact = 1.0f / (float)len;

  zero_v3(r_avg);

  for (int i = 0; i < len; i++) {
    madd_v3_v3fl(r_avg, data[inds[i]], fact);
  }
}

BLI_INLINE void poly_norm(lfVector *data, int i, int j, int *inds, int len, float r_dir[3])
{
  float mid[3];

  poly_avg(data, inds, len, mid);

  normal_tri_v3(r_dir, data[i], data[j], mid);
}

BLI_INLINE void edge_avg(lfVector *data, int i, int j, float r_avg[3])
{
  r_avg[0] = (data[i][0] + data[j][0]) * 0.5f;
  r_avg[1] = (data[i][1] + data[j][1]) * 0.5f;
  r_avg[2] = (data[i][2] + data[j][2]) * 0.5f;
}

BLI_INLINE void edge_norm(lfVector *data, int i, int j, float r_dir[3])
{
  sub_v3_v3v3(r_dir, data[i], data[j]);
  normalize_v3(r_dir);
}

BLI_INLINE float bend_angle(float dir_a[3], float dir_b[3], float dir_e[3])
{
  float cos, sin;
  float tmp[3];

  cos = dot_v3v3(dir_a, dir_b);

  cross_v3_v3v3(tmp, dir_a, dir_b);
  sin = dot_v3v3(tmp, dir_e);

  return atan2f(sin, cos);
}

BLI_INLINE void spring_angle(Implicit_Data *data,
                             int i,
                             int j,
                             int *i_a,
                             int *i_b,
                             int len_a,
                             int len_b,
                             float r_dir_a[3],
                             float r_dir_b[3],
                             float *r_angle,
                             float r_vel_a[3],
                             float r_vel_b[3])
{
  float dir_e[3], vel_e[3];

  poly_norm(data->X, j, i, i_a, len_a, r_dir_a);
  poly_norm(data->X, i, j, i_b, len_b, r_dir_b);

  edge_norm(data->X, i, j, dir_e);

  *r_angle = bend_angle(r_dir_a, r_dir_b, dir_e);

  poly_avg(data->V, i_a, len_a, r_vel_a);
  poly_avg(data->V, i_b, len_b, r_vel_b);

  edge_avg(data->V, i, j, vel_e);

  sub_v3_v3(r_vel_a, vel_e);
  sub_v3_v3(r_vel_b, vel_e);
}

/* Angular springs roughly based on the bending model proposed by Baraff and Witkin in "Large Steps
 * in Cloth Simulation". */
bool BPH_mass_spring_force_spring_angular(Implicit_Data *data,
                                          int i,
                                          int j,
                                          int *i_a,
                                          int *i_b,
                                          int len_a,
                                          int len_b,
                                          float restang,
                                          float stiffness,
                                          float damping)
{
  float angle, dir_a[3], dir_b[3], vel_a[3], vel_b[3];
  float f_a[3], f_b[3], f_e[3];
  float force;
  int x;

  spring_angle(data, i, j, i_a, i_b, len_a, len_b, dir_a, dir_b, &angle, vel_a, vel_b);

  /* spring force */
  force = stiffness * (angle - restang);

  /* damping force */
  force += -damping * (dot_v3v3(vel_a, dir_a) + dot_v3v3(vel_b, dir_b));

  mul_v3_v3fl(f_a, dir_a, force / len_a);
  mul_v3_v3fl(f_b, dir_b, force / len_b);

  for (x = 0; x < len_a; x++) {
    add_v3_v3(data->F[i_a[x]], f_a);
  }

  for (x = 0; x < len_b; x++) {
    add_v3_v3(data->F[i_b[x]], f_b);
  }

  mul_v3_v3fl(f_a, dir_a, force * 0.5f);
  mul_v3_v3fl(f_b, dir_b, force * 0.5f);

  add_v3_v3v3(f_e, f_a, f_b);

  sub_v3_v3(data->F[i], f_e);
  sub_v3_v3(data->F[j], f_e);

  return true;
}

/* Jacobian of a direction vector.
 * Basically the part of the differential orthogonal to the direction,
 * inversely proportional to the length of the edge.
 *
 * dD_ij/dx_i = -dD_ij/dx_j = (D_ij * D_ij^T - I) / len_ij
 */
BLI_INLINE void spring_grad_dir(
    Implicit_Data *data, int i, int j, float edge[3], float dir[3], float grad_dir[3][3])
{
  float length;

  sub_v3_v3v3(edge, data->X[j], data->X[i]);
  length = normalize_v3_v3(dir, edge);

  if (length > ALMOST_ZERO) {
    outerproduct(grad_dir, dir, dir);
    sub_m3_m3m3(grad_dir, I, grad_dir);
    mul_m3_fl(grad_dir, 1.0f / length);
  }
  else {
    zero_m3(grad_dir);
  }
}

BLI_INLINE void spring_hairbend_forces(Implicit_Data *data,
                                       int i,
                                       int j,
                                       int k,
                                       const float goal[3],
                                       float stiffness,
                                       float damping,
                                       int q,
                                       const float dx[3],
                                       const float dv[3],
                                       float r_f[3])
{
  float edge_ij[3], dir_ij[3];
  float edge_jk[3], dir_jk[3];
  float vel_ij[3], vel_jk[3], vel_ortho[3];
  float f_bend[3], f_damp[3];
  float fk[3];
  float dist[3];

  zero_v3(fk);

  sub_v3_v3v3(edge_ij, data->X[j], data->X[i]);
  if (q == i)
    sub_v3_v3(edge_ij, dx);
  if (q == j)
    add_v3_v3(edge_ij, dx);
  normalize_v3_v3(dir_ij, edge_ij);

  sub_v3_v3v3(edge_jk, data->X[k], data->X[j]);
  if (q == j)
    sub_v3_v3(edge_jk, dx);
  if (q == k)
    add_v3_v3(edge_jk, dx);
  normalize_v3_v3(dir_jk, edge_jk);

  sub_v3_v3v3(vel_ij, data->V[j], data->V[i]);
  if (q == i)
    sub_v3_v3(vel_ij, dv);
  if (q == j)
    add_v3_v3(vel_ij, dv);

  sub_v3_v3v3(vel_jk, data->V[k], data->V[j]);
  if (q == j)
    sub_v3_v3(vel_jk, dv);
  if (q == k)
    add_v3_v3(vel_jk, dv);

  /* bending force */
  sub_v3_v3v3(dist, goal, edge_jk);
  mul_v3_v3fl(f_bend, dist, stiffness);

  add_v3_v3(fk, f_bend);

  /* damping force */
  madd_v3_v3v3fl(vel_ortho, vel_jk, dir_jk, -dot_v3v3(vel_jk, dir_jk));
  mul_v3_v3fl(f_damp, vel_ortho, damping);

  sub_v3_v3(fk, f_damp);

  copy_v3_v3(r_f, fk);
}

/* Finite Differences method for estimating the jacobian of the force */
BLI_INLINE void spring_hairbend_estimate_dfdx(Implicit_Data *data,
                                              int i,
                                              int j,
                                              int k,
                                              const float goal[3],
                                              float stiffness,
                                              float damping,
                                              int q,
                                              float dfdx[3][3])
{
  const float delta = 0.00001f;  // TODO find a good heuristic for this
  float dvec_null[3][3], dvec_pos[3][3], dvec_neg[3][3];
  float f[3];
  int a, b;

  zero_m3(dvec_null);
  unit_m3(dvec_pos);
  mul_m3_fl(dvec_pos, delta * 0.5f);
  copy_m3_m3(dvec_neg, dvec_pos);
  negate_m3(dvec_neg);

  /* XXX TODO offset targets to account for position dependency */

  for (a = 0; a < 3; ++a) {
    spring_hairbend_forces(
        data, i, j, k, goal, stiffness, damping, q, dvec_pos[a], dvec_null[a], f);
    copy_v3_v3(dfdx[a], f);

    spring_hairbend_forces(
        data, i, j, k, goal, stiffness, damping, q, dvec_neg[a], dvec_null[a], f);
    sub_v3_v3(dfdx[a], f);

    for (b = 0; b < 3; ++b) {
      dfdx[a][b] /= delta;
    }
  }
}

/* Finite Differences method for estimating the jacobian of the force */
BLI_INLINE void spring_hairbend_estimate_dfdv(Implicit_Data *data,
                                              int i,
                                              int j,
                                              int k,
                                              const float goal[3],
                                              float stiffness,
                                              float damping,
                                              int q,
                                              float dfdv[3][3])
{
  const float delta = 0.00001f;  // TODO find a good heuristic for this
  float dvec_null[3][3], dvec_pos[3][3], dvec_neg[3][3];
  float f[3];
  int a, b;

  zero_m3(dvec_null);
  unit_m3(dvec_pos);
  mul_m3_fl(dvec_pos, delta * 0.5f);
  copy_m3_m3(dvec_neg, dvec_pos);
  negate_m3(dvec_neg);

  /* XXX TODO offset targets to account for position dependency */

  for (a = 0; a < 3; ++a) {
    spring_hairbend_forces(
        data, i, j, k, goal, stiffness, damping, q, dvec_null[a], dvec_pos[a], f);
    copy_v3_v3(dfdv[a], f);

    spring_hairbend_forces(
        data, i, j, k, goal, stiffness, damping, q, dvec_null[a], dvec_neg[a], f);
    sub_v3_v3(dfdv[a], f);

    for (b = 0; b < 3; ++b) {
      dfdv[a][b] /= delta;
    }
  }
}

/* Angular spring that pulls the vertex toward the local target
 * See "Artistic Simulation of Curly Hair" (Pixar technical memo #12-03a)
 */
bool BPH_mass_spring_force_spring_bending_hair(Implicit_Data *data,
                                               int i,
                                               int j,
                                               int k,
                                               const float target[3],
                                               float stiffness,
                                               float damping)
{
  float goal[3];
  float fj[3], fk[3];
  float dfj_dxi[3][3], dfj_dxj[3][3], dfk_dxi[3][3], dfk_dxj[3][3], dfk_dxk[3][3];
  float dfj_dvi[3][3], dfj_dvj[3][3], dfk_dvi[3][3], dfk_dvj[3][3], dfk_dvk[3][3];

  const float vecnull[3] = {0.0f, 0.0f, 0.0f};

  int block_ij = BPH_mass_spring_add_block(data, i, j);
  int block_jk = BPH_mass_spring_add_block(data, j, k);
  int block_ik = BPH_mass_spring_add_block(data, i, k);

  world_to_root_v3(data, j, goal, target);

  spring_hairbend_forces(data, i, j, k, goal, stiffness, damping, k, vecnull, vecnull, fk);
  negate_v3_v3(fj, fk); /* counterforce */

  spring_hairbend_estimate_dfdx(data, i, j, k, goal, stiffness, damping, i, dfk_dxi);
  spring_hairbend_estimate_dfdx(data, i, j, k, goal, stiffness, damping, j, dfk_dxj);
  spring_hairbend_estimate_dfdx(data, i, j, k, goal, stiffness, damping, k, dfk_dxk);
  copy_m3_m3(dfj_dxi, dfk_dxi);
  negate_m3(dfj_dxi);
  copy_m3_m3(dfj_dxj, dfk_dxj);
  negate_m3(dfj_dxj);

  spring_hairbend_estimate_dfdv(data, i, j, k, goal, stiffness, damping, i, dfk_dvi);
  spring_hairbend_estimate_dfdv(data, i, j, k, goal, stiffness, damping, j, dfk_dvj);
  spring_hairbend_estimate_dfdv(data, i, j, k, goal, stiffness, damping, k, dfk_dvk);
  copy_m3_m3(dfj_dvi, dfk_dvi);
  negate_m3(dfj_dvi);
  copy_m3_m3(dfj_dvj, dfk_dvj);
  negate_m3(dfj_dvj);

  /* add forces and jacobians to the solver data */

  add_v3_v3(data->F[j], fj);
  add_v3_v3(data->F[k], fk);

  add_m3_m3m3(data->dFdX[j].m, data->dFdX[j].m, dfj_dxj);
  add_m3_m3m3(data->dFdX[k].m, data->dFdX[k].m, dfk_dxk);

  add_m3_m3m3(data->dFdX[block_ij].m, data->dFdX[block_ij].m, dfj_dxi);
  add_m3_m3m3(data->dFdX[block_jk].m, data->dFdX[block_jk].m, dfk_dxj);
  add_m3_m3m3(data->dFdX[block_ik].m, data->dFdX[block_ik].m, dfk_dxi);

  add_m3_m3m3(data->dFdV[j].m, data->dFdV[j].m, dfj_dvj);
  add_m3_m3m3(data->dFdV[k].m, data->dFdV[k].m, dfk_dvk);

  add_m3_m3m3(data->dFdV[block_ij].m, data->dFdV[block_ij].m, dfj_dvi);
  add_m3_m3m3(data->dFdV[block_jk].m, data->dFdV[block_jk].m, dfk_dvj);
  add_m3_m3m3(data->dFdV[block_ik].m, data->dFdV[block_ik].m, dfk_dvi);

  /* XXX analytical calculation of derivatives below is incorrect.
   * This proved to be difficult, but for now just using the finite difference method for
   * estimating the jacobians should be sufficient.
   */
#  if 0
  float edge_ij[3], dir_ij[3], grad_dir_ij[3][3];
  float edge_jk[3], dir_jk[3], grad_dir_jk[3][3];
  float dist[3], vel_jk[3], vel_jk_ortho[3], projvel[3];
  float target[3];
  float tmp[3][3];
  float fi[3], fj[3], fk[3];
  float dfi_dxi[3][3], dfj_dxi[3][3], dfj_dxj[3][3], dfk_dxi[3][3], dfk_dxj[3][3], dfk_dxk[3][3];
  float dfdvi[3][3];

  // TESTING
  damping = 0.0f;

  zero_v3(fi);
  zero_v3(fj);
  zero_v3(fk);
  zero_m3(dfi_dxi);
  zero_m3(dfj_dxi);
  zero_m3(dfk_dxi);
  zero_m3(dfk_dxj);
  zero_m3(dfk_dxk);

  /* jacobian of direction vectors */
  spring_grad_dir(data, i, j, edge_ij, dir_ij, grad_dir_ij);
  spring_grad_dir(data, j, k, edge_jk, dir_jk, grad_dir_jk);

  sub_v3_v3v3(vel_jk, data->V[k], data->V[j]);

  /* bending force */
  mul_v3_v3fl(target, dir_ij, restlen);
  sub_v3_v3v3(dist, target, edge_jk);
  mul_v3_v3fl(fk, dist, stiffness);

  /* damping force */
  madd_v3_v3v3fl(vel_jk_ortho, vel_jk, dir_jk, -dot_v3v3(vel_jk, dir_jk));
  madd_v3_v3fl(fk, vel_jk_ortho, damping);

  /* XXX this only holds true as long as we assume straight rest shape!
   * eventually will become a bit more involved since the opposite segment
   * gets its own target, under condition of having equal torque on both sides.
   */
  copy_v3_v3(fi, fk);

  /* counterforce on the middle point */
  sub_v3_v3(fj, fi);
  sub_v3_v3(fj, fk);

  /* === derivatives === */

  madd_m3_m3fl(dfk_dxi, grad_dir_ij, stiffness * restlen);

  madd_m3_m3fl(dfk_dxj, grad_dir_ij, -stiffness * restlen);
  madd_m3_m3fl(dfk_dxj, I, stiffness);

  madd_m3_m3fl(dfk_dxk, I, -stiffness);

  copy_m3_m3(dfi_dxi, dfk_dxk);
  negate_m3(dfi_dxi);

  /* dfj_dfi == dfi_dfj due to symmetry,
   * dfi_dfj == dfk_dfj due to fi == fk
   * XXX see comment above on future bent rest shapes
   */
  copy_m3_m3(dfj_dxi, dfk_dxj);

  /* dfj_dxj == -(dfi_dxj + dfk_dxj) due to fj == -(fi + fk) */
  sub_m3_m3m3(dfj_dxj, dfj_dxj, dfj_dxi);
  sub_m3_m3m3(dfj_dxj, dfj_dxj, dfk_dxj);

  /* add forces and jacobians to the solver data */
  add_v3_v3(data->F[i], fi);
  add_v3_v3(data->F[j], fj);
  add_v3_v3(data->F[k], fk);

  add_m3_m3m3(data->dFdX[i].m, data->dFdX[i].m, dfi_dxi);
  add_m3_m3m3(data->dFdX[j].m, data->dFdX[j].m, dfj_dxj);
  add_m3_m3m3(data->dFdX[k].m, data->dFdX[k].m, dfk_dxk);

  add_m3_m3m3(data->dFdX[block_ij].m, data->dFdX[block_ij].m, dfj_dxi);
  add_m3_m3m3(data->dFdX[block_jk].m, data->dFdX[block_jk].m, dfk_dxj);
  add_m3_m3m3(data->dFdX[block_ik].m, data->dFdX[block_ik].m, dfk_dxi);
#  endif

  return true;
}

bool BPH_mass_spring_force_spring_goal(Implicit_Data *data,
                                       int i,
                                       const float goal_x[3],
                                       const float goal_v[3],
                                       float stiffness,
                                       float damping)
{
  float root_goal_x[3], root_goal_v[3], extent[3], length, dir[3], vel[3];
  float f[3], dfdx[3][3], dfdv[3][3];

  /* goal is in world space */
  world_to_root_v3(data, i, root_goal_x, goal_x);
  world_to_root_v3(data, i, root_goal_v, goal_v);

  sub_v3_v3v3(extent, root_goal_x, data->X[i]);
  sub_v3_v3v3(vel, root_goal_v, data->V[i]);
  length = normalize_v3_v3(dir, extent);

  if (length > ALMOST_ZERO) {
    mul_v3_v3fl(f, dir, stiffness * length);

    // Ascher & Boxman, p.21: Damping only during elonglation
    // something wrong with it...
    madd_v3_v3fl(f, dir, damping * dot_v3v3(vel, dir));

    dfdx_spring(dfdx, dir, length, 0.0f, stiffness);
    dfdv_damp(dfdv, dir, damping);

    add_v3_v3(data->F[i], f);
    add_m3_m3m3(data->dFdX[i].m, data->dFdX[i].m, dfdx);
    add_m3_m3m3(data->dFdV[i].m, data->dFdV[i].m, dfdv);

    return true;
  }
  else {
    return false;
  }
}

#endif /* IMPLICIT_SOLVER_BLENDER */
