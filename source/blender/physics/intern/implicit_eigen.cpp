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

#ifdef IMPLICIT_SOLVER_EIGEN

//#define USE_EIGEN_CORE
#  define USE_EIGEN_CONSTRAINED_CG

#  ifdef __GNUC__
#    pragma GCC diagnostic push
/* XXX suppress verbose warnings in eigen */
//#  pragma GCC diagnostic ignored "-Wlogical-op"
#  endif

#  ifndef IMPLICIT_ENABLE_EIGEN_DEBUG
#    ifdef NDEBUG
#      define IMPLICIT_NDEBUG
#    endif
#    define NDEBUG
#  endif

#  include <Eigen/Sparse>
#  include <Eigen/src/Core/util/DisableStupidWarnings.h>

#  ifdef USE_EIGEN_CONSTRAINED_CG
#    include <intern/ConstrainedConjugateGradient.h>
#  endif

#  ifndef IMPLICIT_ENABLE_EIGEN_DEBUG
#    ifndef IMPLICIT_NDEBUG
#      undef NDEBUG
#    else
#      undef IMPLICIT_NDEBUG
#    endif
#  endif

#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif

#  include "MEM_guardedalloc.h"

extern "C" {
#  include "DNA_scene_types.h"
#  include "DNA_object_types.h"
#  include "DNA_object_force_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_texture_types.h"

#  include "BLI_math.h"
#  include "BLI_linklist.h"
#  include "BLI_utildefines.h"

#  include "BKE_cloth.h"
#  include "BKE_collision.h"
#  include "BKE_effect.h"
#  include "BKE_global.h"

#  include "BPH_mass_spring.h"
}

typedef float Scalar;

static float I[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

/* slightly extended Eigen vector class
 * with conversion to/from plain C float array
 */
class fVector : public Eigen::Vector3f {
 public:
  typedef float *ctype;

  fVector()
  {
  }

  fVector(const ctype &v)
  {
    for (int k = 0; k < 3; ++k)
      coeffRef(k) = v[k];
  }

  fVector &operator=(const ctype &v)
  {
    for (int k = 0; k < 3; ++k)
      coeffRef(k) = v[k];
    return *this;
  }

  operator ctype()
  {
    return data();
  }
};

/* slightly extended Eigen matrix class
 * with conversion to/from plain C float array
 */
class fMatrix : public Eigen::Matrix3f {
 public:
  typedef float (*ctype)[3];

  fMatrix()
  {
  }

  fMatrix(const ctype &v)
  {
    for (int k = 0; k < 3; ++k)
      for (int l = 0; l < 3; ++l)
        coeffRef(l, k) = v[k][l];
  }

  fMatrix &operator=(const ctype &v)
  {
    for (int k = 0; k < 3; ++k)
      for (int l = 0; l < 3; ++l)
        coeffRef(l, k) = v[k][l];
    return *this;
  }

  operator ctype()
  {
    return (ctype)data();
  }
};

/* Extension of dense Eigen vectors,
 * providing 3-float block access for blenlib math functions
 */
class lVector : public Eigen::VectorXf {
 public:
  typedef Eigen::VectorXf base_t;

  lVector()
  {
  }

  template<typename T> lVector &operator=(T rhs)
  {
    base_t::operator=(rhs);
    return *this;
  }

  float *v3(int vertex)
  {
    return &coeffRef(3 * vertex);
  }

  const float *v3(int vertex) const
  {
    return &coeffRef(3 * vertex);
  }
};

typedef Eigen::Triplet<Scalar> Triplet;
typedef std::vector<Triplet> TripletList;

typedef Eigen::SparseMatrix<Scalar> lMatrix;

/* Constructor type that provides more convenient handling of Eigen triplets
 * for efficient construction of sparse 3x3 block matrices.
 * This should be used for building lMatrix instead of writing to such lMatrix directly (which is
 * very inefficient). After all elements have been defined using the set() method, the actual
 * matrix can be filled using construct().
 */
struct lMatrixCtor {
  lMatrixCtor()
  {
  }

  void reset()
  {
    m_trips.clear();
  }

  void reserve(int numverts)
  {
    /* reserve for diagonal entries */
    m_trips.reserve(numverts * 9);
  }

  void add(int i, int j, const fMatrix &m)
  {
    i *= 3;
    j *= 3;
    for (int k = 0; k < 3; ++k)
      for (int l = 0; l < 3; ++l)
        m_trips.push_back(Triplet(i + k, j + l, m.coeff(l, k)));
  }

  void sub(int i, int j, const fMatrix &m)
  {
    i *= 3;
    j *= 3;
    for (int k = 0; k < 3; ++k)
      for (int l = 0; l < 3; ++l)
        m_trips.push_back(Triplet(i + k, j + l, -m.coeff(l, k)));
  }

  inline void construct(lMatrix &m)
  {
    m.setFromTriplets(m_trips.begin(), m_trips.end());
    m_trips.clear();
  }

 private:
  TripletList m_trips;
};

#  ifdef USE_EIGEN_CORE
typedef Eigen::ConjugateGradient<lMatrix, Eigen::Lower, Eigen::DiagonalPreconditioner<Scalar>>
    ConjugateGradient;
#  endif
#  ifdef USE_EIGEN_CONSTRAINED_CG
typedef Eigen::ConstrainedConjugateGradient<lMatrix,
                                            Eigen::Lower,
                                            lMatrix,
                                            Eigen::DiagonalPreconditioner<Scalar>>
    ConstraintConjGrad;
#  endif
using Eigen::ComputationInfo;

static void print_lvector(const lVector &v)
{
  for (int i = 0; i < v.rows(); ++i) {
    if (i > 0 && i % 3 == 0)
      printf("\n");

    printf("%f,\n", v[i]);
  }
}

static void print_lmatrix(const lMatrix &m)
{
  for (int j = 0; j < m.rows(); ++j) {
    if (j > 0 && j % 3 == 0)
      printf("\n");

    for (int i = 0; i < m.cols(); ++i) {
      if (i > 0 && i % 3 == 0)
        printf("  ");

      implicit_print_matrix_elem(m.coeff(j, i));
    }
    printf("\n");
  }
}

BLI_INLINE void lMatrix_reserve_elems(lMatrix &m, int num)
{
  m.reserve(Eigen::VectorXi::Constant(m.cols(), num));
}

BLI_INLINE float *lVector_v3(lVector &v, int vertex)
{
  return v.data() + 3 * vertex;
}

BLI_INLINE const float *lVector_v3(const lVector &v, int vertex)
{
  return v.data() + 3 * vertex;
}

#  if 0
BLI_INLINE void triplets_m3(TripletList &tlist, float m[3][3], int i, int j)
{
  i *= 3;
  j *= 3;
  for (int l = 0; l < 3; ++l) {
    for (int k = 0; k < 3; ++k) {
      tlist.push_back(Triplet(i + k, j + l, m[k][l]));
    }
  }
}

BLI_INLINE void triplets_m3fl(TripletList &tlist, float m[3][3], int i, int j, float factor)
{
  i *= 3;
  j *= 3;
  for (int l = 0; l < 3; ++l) {
    for (int k = 0; k < 3; ++k) {
      tlist.push_back(Triplet(i + k, j + l, m[k][l] * factor));
    }
  }
}

BLI_INLINE void lMatrix_add_triplets(lMatrix &r, const TripletList &tlist)
{
  lMatrix t(r.rows(), r.cols());
  t.setFromTriplets(tlist.begin(), tlist.end());
  r += t;
}

BLI_INLINE void lMatrix_madd_triplets(lMatrix &r, const TripletList &tlist, float f)
{
  lMatrix t(r.rows(), r.cols());
  t.setFromTriplets(tlist.begin(), tlist.end());
  r += f * t;
}

BLI_INLINE void lMatrix_sub_triplets(lMatrix &r, const TripletList &tlist)
{
  lMatrix t(r.rows(), r.cols());
  t.setFromTriplets(tlist.begin(), tlist.end());
  r -= t;
}
#  endif

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

BLI_INLINE void madd_m3_m3m3fl(float r[3][3], float a[3][3], float b[3][3], float f)
{
  r[0][0] = a[0][0] + b[0][0] * f;
  r[0][1] = a[0][1] + b[0][1] * f;
  r[0][2] = a[0][2] + b[0][2] * f;
  r[1][0] = a[1][0] + b[1][0] * f;
  r[1][1] = a[1][1] + b[1][1] * f;
  r[1][2] = a[1][2] + b[1][2] * f;
  r[2][0] = a[2][0] + b[2][0] * f;
  r[2][1] = a[2][1] + b[2][1] * f;
  r[2][2] = a[2][2] + b[2][2] * f;
}

struct Implicit_Data {
  typedef std::vector<fMatrix> fMatrixVector;

  Implicit_Data(int numverts)
  {
    resize(numverts);
  }

  void resize(int numverts)
  {
    this->numverts = numverts;
    int tot = 3 * numverts;

    M.resize(tot, tot);
    F.resize(tot);
    dFdX.resize(tot, tot);
    dFdV.resize(tot, tot);

    tfm.resize(numverts, I);

    X.resize(tot);
    Xnew.resize(tot);
    V.resize(tot);
    Vnew.resize(tot);

    A.resize(tot, tot);
    B.resize(tot);

    dV.resize(tot);
    z.resize(tot);
    S.resize(tot, tot);

    iM.reserve(numverts);
    idFdX.reserve(numverts);
    idFdV.reserve(numverts);
    iS.reserve(numverts);
  }

  int numverts;

  /* inputs */
  lMatrix M;          /* masses */
  lVector F;          /* forces */
  lMatrix dFdX, dFdV; /* force jacobians */

  fMatrixVector tfm; /* local coordinate transform */

  /* motion state data */
  lVector X, Xnew; /* positions */
  lVector V, Vnew; /* velocities */

  /* internal solver data */
  lVector B; /* B for A*dV = B */
  lMatrix A; /* A for A*dV = B */

  lVector dV; /* velocity change (solution of A*dV = B) */
  lVector z;  /* target velocity in constrained directions */
  lMatrix S;  /* filtering matrix for constraints */

  /* temporary constructors */
  lMatrixCtor iM;           /* masses */
  lMatrixCtor idFdX, idFdV; /* force jacobians */
  lMatrixCtor iS;           /* filtering matrix for constraints */
};

Implicit_Data *BPH_mass_spring_solver_create(int numverts, int numsprings)
{
  Implicit_Data *id = new Implicit_Data(numverts);
  return id;
}

void BPH_mass_spring_solver_free(Implicit_Data *id)
{
  if (id)
    delete id;
}

int BPH_mass_spring_solver_numvert(Implicit_Data *id)
{
  if (id)
    return id->numverts;
  else
    return 0;
}

/* ==== Transformation from/to root reference frames ==== */

BLI_INLINE void world_to_root_v3(Implicit_Data *data, int index, float r[3], const float v[3])
{
  copy_v3_v3(r, v);
  mul_transposed_m3_v3(data->tfm[index], r);
}

BLI_INLINE void root_to_world_v3(Implicit_Data *data, int index, float r[3], const float v[3])
{
  mul_v3_m3v3(r, data->tfm[index], v);
}

BLI_INLINE void world_to_root_m3(Implicit_Data *data, int index, float r[3][3], float m[3][3])
{
  float trot[3][3];
  copy_m3_m3(trot, data->tfm[index]);
  transpose_m3(trot);
  mul_m3_m3m3(r, trot, m);
}

BLI_INLINE void root_to_world_m3(Implicit_Data *data, int index, float r[3][3], float m[3][3])
{
  mul_m3_m3m3(r, data->tfm[index], m);
}

/* ================================ */

bool BPH_mass_spring_solve_velocities(Implicit_Data *data, float dt, ImplicitSolverResult *result)
{
#  ifdef USE_EIGEN_CORE
  typedef ConjugateGradient solver_t;
#  endif
#  ifdef USE_EIGEN_CONSTRAINED_CG
  typedef ConstraintConjGrad solver_t;
#  endif

  data->iM.construct(data->M);
  data->idFdX.construct(data->dFdX);
  data->idFdV.construct(data->dFdV);
  data->iS.construct(data->S);

  solver_t cg;
  cg.setMaxIterations(100);
  cg.setTolerance(0.01f);

#  ifdef USE_EIGEN_CONSTRAINED_CG
  cg.filter() = data->S;
#  endif

  data->A = data->M - dt * data->dFdV - dt * dt * data->dFdX;
  cg.compute(data->A);

  data->B = dt * data->F + dt * dt * data->dFdX * data->V;

#  ifdef IMPLICIT_PRINT_SOLVER_INPUT_OUTPUT
  printf("==== A ====\n");
  print_lmatrix(id->A);
  printf("==== z ====\n");
  print_lvector(id->z);
  printf("==== B ====\n");
  print_lvector(id->B);
  printf("==== S ====\n");
  print_lmatrix(id->S);
#  endif

#  ifdef USE_EIGEN_CORE
  data->dV = cg.solve(data->B);
#  endif
#  ifdef USE_EIGEN_CONSTRAINED_CG
  data->dV = cg.solveWithGuess(data->B, data->z);
#  endif

#  ifdef IMPLICIT_PRINT_SOLVER_INPUT_OUTPUT
  printf("==== dV ====\n");
  print_lvector(id->dV);
  printf("========\n");
#  endif

  data->Vnew = data->V + data->dV;

  switch (cg.info()) {
    case Eigen::Success:
      result->status = BPH_SOLVER_SUCCESS;
      break;
    case Eigen::NoConvergence:
      result->status = BPH_SOLVER_NO_CONVERGENCE;
      break;
    case Eigen::InvalidInput:
      result->status = BPH_SOLVER_INVALID_INPUT;
      break;
    case Eigen::NumericalIssue:
      result->status = BPH_SOLVER_NUMERICAL_ISSUE;
      break;
  }

  result->iterations = cg.iterations();
  result->error = cg.error();

  return cg.info() == Eigen::Success;
}

bool BPH_mass_spring_solve_positions(Implicit_Data *data, float dt)
{
  data->Xnew = data->X + data->Vnew * dt;
  return true;
}

/* ================================ */

void BPH_mass_spring_apply_result(Implicit_Data *data)
{
  data->X = data->Xnew;
  data->V = data->Vnew;
}

void BPH_mass_spring_set_vertex_mass(Implicit_Data *data, int index, float mass)
{
  float m[3][3];
  copy_m3_m3(m, I);
  mul_m3_fl(m, mass);
  data->iM.add(index, index, m);
}

void BPH_mass_spring_set_rest_transform(Implicit_Data *data, int index, float tfm[3][3])
{
#  ifdef CLOTH_ROOT_FRAME
  copy_m3_m3(data->tfm[index], tfm);
#  else
  unit_m3(data->tfm[index]);
  (void)tfm;
#  endif
}

void BPH_mass_spring_set_motion_state(Implicit_Data *data,
                                      int index,
                                      const float x[3],
                                      const float v[3])
{
  world_to_root_v3(data, index, data->X.v3(index), x);
  world_to_root_v3(data, index, data->V.v3(index), v);
}

void BPH_mass_spring_set_position(Implicit_Data *data, int index, const float x[3])
{
  world_to_root_v3(data, index, data->X.v3(index), x);
}

void BPH_mass_spring_set_velocity(Implicit_Data *data, int index, const float v[3])
{
  world_to_root_v3(data, index, data->V.v3(index), v);
}

void BPH_mass_spring_get_motion_state(struct Implicit_Data *data,
                                      int index,
                                      float x[3],
                                      float v[3])
{
  if (x)
    root_to_world_v3(data, index, x, data->X.v3(index));
  if (v)
    root_to_world_v3(data, index, v, data->V.v3(index));
}

void BPH_mass_spring_get_position(struct Implicit_Data *data, int index, float x[3])
{
  root_to_world_v3(data, index, x, data->X.v3(index));
}

void BPH_mass_spring_get_new_velocity(Implicit_Data *data, int index, float v[3])
{
  root_to_world_v3(data, index, v, data->V.v3(index));
}

void BPH_mass_spring_set_new_velocity(Implicit_Data *data, int index, const float v[3])
{
  world_to_root_v3(data, index, data->V.v3(index), v);
}

void BPH_mass_spring_clear_constraints(Implicit_Data *data)
{
  int numverts = data->numverts;
  for (int i = 0; i < numverts; ++i) {
    data->iS.add(i, i, I);
    zero_v3(data->z.v3(i));
  }
}

void BPH_mass_spring_add_constraint_ndof0(Implicit_Data *data, int index, const float dV[3])
{
  data->iS.sub(index, index, I);

  world_to_root_v3(data, index, data->z.v3(index), dV);
}

void BPH_mass_spring_add_constraint_ndof1(
    Implicit_Data *data, int index, const float c1[3], const float c2[3], const float dV[3])
{
  float m[3][3], p[3], q[3], u[3], cmat[3][3];

  world_to_root_v3(data, index, p, c1);
  outerproduct(cmat, p, p);
  copy_m3_m3(m, cmat);

  world_to_root_v3(data, index, q, c2);
  outerproduct(cmat, q, q);
  add_m3_m3m3(m, m, cmat);

  /* XXX not sure but multiplication should work here */
  data->iS.sub(index, index, m);
  //  mul_m3_m3m3(data->S[index].m, data->S[index].m, m);

  world_to_root_v3(data, index, u, dV);
  add_v3_v3(data->z.v3(index), u);
}

void BPH_mass_spring_add_constraint_ndof2(Implicit_Data *data,
                                          int index,
                                          const float c1[3],
                                          const float dV[3])
{
  float m[3][3], p[3], u[3], cmat[3][3];

  world_to_root_v3(data, index, p, c1);
  outerproduct(cmat, p, p);
  copy_m3_m3(m, cmat);

  data->iS.sub(index, index, m);
  //  mul_m3_m3m3(data->S[index].m, data->S[index].m, m);

  world_to_root_v3(data, index, u, dV);
  add_v3_v3(data->z.v3(index), u);
}

void BPH_mass_spring_clear_forces(Implicit_Data *data)
{
  data->F.setZero();
  data->dFdX.setZero();
  data->dFdV.setZero();
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

  cross_v3_v3v3(euler, dwdt, data->X.v3(index));
  cross_v3_v3v3(coriolis, w, data->V.v3(index));
  mul_v3_fl(coriolis, 2.0f);
  cross_v3_v3v3(rotvel, w, data->X.v3(index));
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

  add_v3_v3(data->F.v3(index), f);
  data->idFdX.add(index, index, dfdx);
  data->idFdV.add(index, index, dfdv);
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

  add_v3_v3(data->F.v3(index), f);
}

void BPH_mass_spring_force_drag(Implicit_Data *data, float drag)
{
  int numverts = data->numverts;
  for (int i = 0; i < numverts; i++) {
    float tmp[3][3];

    /* NB: uses root space velocity, no need to transform */
    madd_v3_v3fl(data->F.v3(i), data->V.v3(i), -drag);

    copy_m3_m3(tmp, I);
    mul_m3_fl(tmp, -drag);
    data->idFdV.add(i, i, tmp);
  }
}

void BPH_mass_spring_force_extern(
    struct Implicit_Data *data, int i, const float f[3], float dfdx[3][3], float dfdv[3][3])
{
  float tf[3], tdfdx[3][3], tdfdv[3][3];
  world_to_root_v3(data, i, tf, f);
  world_to_root_m3(data, i, tdfdx, dfdx);
  world_to_root_m3(data, i, tdfdv, dfdv);

  add_v3_v3(data->F.v3(i), tf);
  data->idFdX.add(i, i, tdfdx);
  data->idFdV.add(i, i, tdfdv);
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

/* XXX does not support force jacobians yet,
 * since the effector system does not provide them either. */
void BPH_mass_spring_force_face_wind(
    Implicit_Data *data, int v1, int v2, int v3, const float (*winvec)[3])
{
  const float effector_scale = 0.02f;
  float win[3], nor[3], area;
  float factor;

  // calculate face normal and area
  area = calc_nor_area_tri(nor, data->X.v3(v1), data->X.v3(v2), data->X.v3(v3));
  factor = effector_scale * area / 3.0f;

  world_to_root_v3(data, v1, win, winvec[v1]);
  madd_v3_v3fl(data->F.v3(v1), nor, factor * dot_v3v3(win, nor));

  world_to_root_v3(data, v2, win, winvec[v2]);
  madd_v3_v3fl(data->F.v3(v2), nor, factor * dot_v3v3(win, nor));

  world_to_root_v3(data, v3, win, winvec[v3]);
  madd_v3_v3fl(data->F.v3(v3), nor, factor * dot_v3v3(win, nor));
}

void BPH_mass_spring_force_edge_wind(Implicit_Data *data, int v1, int v2, const float (*winvec)[3])
{
  const float effector_scale = 0.01;
  float win[3], dir[3], nor[3], length;

  sub_v3_v3v3(dir, data->X.v3(v1), data->X.v3(v2));
  length = normalize_v3(dir);

  world_to_root_v3(data, v1, win, winvec[v1]);
  madd_v3_v3v3fl(nor, win, dir, -dot_v3v3(win, dir));
  madd_v3_v3fl(data->F.v3(v1), nor, effector_scale * length);

  world_to_root_v3(data, v2, win, winvec[v2]);
  madd_v3_v3v3fl(nor, win, dir, -dot_v3v3(win, dir));
  madd_v3_v3fl(data->F.v3(v2), nor, effector_scale * length);
}

BLI_INLINE void dfdx_spring(float to[3][3], const float dir[3], float length, float L, float k)
{
  /* dir is unit length direction, rest is spring's restlength, k is spring constant. */
  // return ((I - outerprod(dir, dir)) * Min(1.0f, rest / length) - I) * -k;
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
  return (-11.541f * powf(x, 4) + 34.193f * powf(x, 3) - 39.083f * powf(x, 2) + 23.116f * x -
          9.713f);
}

BLI_INLINE float fbderiv(float length, float L)
{
  float x = length / L;

  return (-46.164f * powf(x, 3) + 102.579f * powf(x, 2) - 78.166f * x + 23.116f);
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
  sub_v3_v3v3(r_extent, data->X.v3(j), data->X.v3(i));
  sub_v3_v3v3(r_vel, data->V.v3(j), data->V.v3(i));
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
  add_v3_v3(data->F.v3(i), f);
  sub_v3_v3(data->F.v3(j), f);

  data->idFdX.add(i, i, dfdx);
  data->idFdX.add(j, j, dfdx);
  data->idFdX.sub(i, j, dfdx);
  data->idFdX.sub(j, i, dfdx);

  data->idFdV.add(i, i, dfdv);
  data->idFdV.add(j, j, dfdv);
  data->idFdV.sub(i, j, dfdv);
  data->idFdV.sub(j, i, dfdv);
}

bool BPH_mass_spring_force_spring_linear(Implicit_Data *data,
                                         int i,
                                         int j,
                                         float restlen,
                                         float stiffness,
                                         float damping,
                                         bool no_compress,
                                         float clamp_force,
                                         float r_f[3],
                                         float r_dfdx[3][3],
                                         float r_dfdv[3][3])
{
  float extent[3], length, dir[3], vel[3];

  // calculate elonglation
  spring_length(data, i, j, extent, dir, &length, vel);

  if (length > restlen || no_compress) {
    float stretch_force, f[3], dfdx[3][3], dfdv[3][3];

    stretch_force = stiffness * (length - restlen);
    if (clamp_force > 0.0f && stretch_force > clamp_force) {
      stretch_force = clamp_force;
    }
    mul_v3_v3fl(f, dir, stretch_force);

    // Ascher & Boxman, p.21: Damping only during elonglation
    // something wrong with it...
    madd_v3_v3fl(f, dir, damping * dot_v3v3(vel, dir));

    dfdx_spring(dfdx, dir, length, restlen, stiffness);
    dfdv_damp(dfdv, dir, damping);

    apply_spring(data, i, j, f, dfdx, dfdv);

    if (r_f)
      copy_v3_v3(r_f, f);
    if (r_dfdx)
      copy_m3_m3(r_dfdx, dfdx);
    if (r_dfdv)
      copy_m3_m3(r_dfdv, dfdv);

    return true;
  }
  else {
    if (r_f)
      zero_v3(r_f);
    if (r_dfdx)
      zero_m3(r_dfdx);
    if (r_dfdv)
      zero_m3(r_dfdv);

    return false;
  }
}

/* See "Stable but Responsive Cloth" (Choi, Ko 2005) */
bool BPH_mass_spring_force_spring_bending(Implicit_Data *data,
                                          int i,
                                          int j,
                                          float restlen,
                                          float kb,
                                          float cb,
                                          float r_f[3],
                                          float r_dfdx[3][3],
                                          float r_dfdv[3][3])
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

    if (r_f)
      copy_v3_v3(r_f, f);
    if (r_dfdx)
      copy_m3_m3(r_dfdx, dfdx);
    if (r_dfdv)
      copy_m3_m3(r_dfdv, dfdv);

    return true;
  }
  else {
    if (r_f)
      zero_v3(r_f);
    if (r_dfdx)
      zero_m3(r_dfdx);
    if (r_dfdv)
      zero_m3(r_dfdv);

    return false;
  }
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

  sub_v3_v3v3(edge, data->X.v3(j), data->X.v3(i));
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

BLI_INLINE void spring_angbend_forces(Implicit_Data *data,
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

  sub_v3_v3v3(edge_ij, data->X.v3(j), data->X.v3(i));
  if (q == i)
    sub_v3_v3(edge_ij, dx);
  if (q == j)
    add_v3_v3(edge_ij, dx);
  normalize_v3_v3(dir_ij, edge_ij);

  sub_v3_v3v3(edge_jk, data->X.v3(k), data->X.v3(j));
  if (q == j)
    sub_v3_v3(edge_jk, dx);
  if (q == k)
    add_v3_v3(edge_jk, dx);
  normalize_v3_v3(dir_jk, edge_jk);

  sub_v3_v3v3(vel_ij, data->V.v3(j), data->V.v3(i));
  if (q == i)
    sub_v3_v3(vel_ij, dv);
  if (q == j)
    add_v3_v3(vel_ij, dv);

  sub_v3_v3v3(vel_jk, data->V.v3(k), data->V.v3(j));
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
BLI_INLINE void spring_angbend_estimate_dfdx(Implicit_Data *data,
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
    spring_angbend_forces(
        data, i, j, k, goal, stiffness, damping, q, dvec_pos[a], dvec_null[a], f);
    copy_v3_v3(dfdx[a], f);

    spring_angbend_forces(
        data, i, j, k, goal, stiffness, damping, q, dvec_neg[a], dvec_null[a], f);
    sub_v3_v3(dfdx[a], f);

    for (b = 0; b < 3; ++b) {
      dfdx[a][b] /= delta;
    }
  }
}

/* Finite Differences method for estimating the jacobian of the force */
BLI_INLINE void spring_angbend_estimate_dfdv(Implicit_Data *data,
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
    spring_angbend_forces(
        data, i, j, k, goal, stiffness, damping, q, dvec_null[a], dvec_pos[a], f);
    copy_v3_v3(dfdv[a], f);

    spring_angbend_forces(
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
bool BPH_mass_spring_force_spring_bending_angular(Implicit_Data *data,
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

  world_to_root_v3(data, j, goal, target);

  spring_angbend_forces(data, i, j, k, goal, stiffness, damping, k, vecnull, vecnull, fk);
  negate_v3_v3(fj, fk); /* counterforce */

  spring_angbend_estimate_dfdx(data, i, j, k, goal, stiffness, damping, i, dfk_dxi);
  spring_angbend_estimate_dfdx(data, i, j, k, goal, stiffness, damping, j, dfk_dxj);
  spring_angbend_estimate_dfdx(data, i, j, k, goal, stiffness, damping, k, dfk_dxk);
  copy_m3_m3(dfj_dxi, dfk_dxi);
  negate_m3(dfj_dxi);
  copy_m3_m3(dfj_dxj, dfk_dxj);
  negate_m3(dfj_dxj);

  spring_angbend_estimate_dfdv(data, i, j, k, goal, stiffness, damping, i, dfk_dvi);
  spring_angbend_estimate_dfdv(data, i, j, k, goal, stiffness, damping, j, dfk_dvj);
  spring_angbend_estimate_dfdv(data, i, j, k, goal, stiffness, damping, k, dfk_dvk);
  copy_m3_m3(dfj_dvi, dfk_dvi);
  negate_m3(dfj_dvi);
  copy_m3_m3(dfj_dvj, dfk_dvj);
  negate_m3(dfj_dvj);

  /* add forces and jacobians to the solver data */

  add_v3_v3(data->F.v3(j), fj);
  add_v3_v3(data->F.v3(k), fk);

  data->idFdX.add(j, j, dfj_dxj);
  data->idFdX.add(k, k, dfk_dxk);

  data->idFdX.add(i, j, dfj_dxi);
  data->idFdX.add(j, i, dfj_dxi);
  data->idFdX.add(j, k, dfk_dxj);
  data->idFdX.add(k, j, dfk_dxj);
  data->idFdX.add(i, k, dfk_dxi);
  data->idFdX.add(k, i, dfk_dxi);

  data->idFdV.add(j, j, dfj_dvj);
  data->idFdV.add(k, k, dfk_dvk);

  data->idFdV.add(i, j, dfj_dvi);
  data->idFdV.add(j, i, dfj_dvi);
  data->idFdV.add(j, k, dfk_dvj);
  data->idFdV.add(k, j, dfk_dvj);
  data->idFdV.add(i, k, dfk_dvi);
  data->idFdV.add(k, i, dfk_dvi);

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
                                       float damping,
                                       float r_f[3],
                                       float r_dfdx[3][3],
                                       float r_dfdv[3][3])
{
  float root_goal_x[3], root_goal_v[3], extent[3], length, dir[3], vel[3];
  float f[3], dfdx[3][3], dfdv[3][3];

  /* goal is in world space */
  world_to_root_v3(data, i, root_goal_x, goal_x);
  world_to_root_v3(data, i, root_goal_v, goal_v);

  sub_v3_v3v3(extent, root_goal_x, data->X.v3(i));
  sub_v3_v3v3(vel, root_goal_v, data->V.v3(i));
  length = normalize_v3_v3(dir, extent);

  if (length > ALMOST_ZERO) {
    mul_v3_v3fl(f, dir, stiffness * length);

    // Ascher & Boxman, p.21: Damping only during elonglation
    // something wrong with it...
    madd_v3_v3fl(f, dir, damping * dot_v3v3(vel, dir));

    dfdx_spring(dfdx, dir, length, 0.0f, stiffness);
    dfdv_damp(dfdv, dir, damping);

    add_v3_v3(data->F.v3(i), f);
    data->idFdX.add(i, i, dfdx);
    data->idFdV.add(i, i, dfdv);

    if (r_f)
      copy_v3_v3(r_f, f);
    if (r_dfdx)
      copy_m3_m3(r_dfdx, dfdx);
    if (r_dfdv)
      copy_m3_m3(r_dfdv, dfdv);

    return true;
  }
  else {
    if (r_f)
      zero_v3(r_f);
    if (r_dfdx)
      zero_m3(r_dfdx);
    if (r_dfdv)
      zero_m3(r_dfdv);

    return false;
  }
}

#endif /* IMPLICIT_SOLVER_EIGEN */
