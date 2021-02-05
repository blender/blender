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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup iksolver
 */

#include "IK_QJacobian.h"

IK_QJacobian::IK_QJacobian() : m_sdls(true), m_min_damp(1.0)
{
}

IK_QJacobian::~IK_QJacobian()
{
}

void IK_QJacobian::ArmMatrices(int dof, int task_size)
{
  m_dof = dof;
  m_task_size = task_size;

  m_jacobian.resize(task_size, dof);
  m_jacobian.setZero();

  m_alpha.resize(dof);
  m_alpha.setZero();

  m_nullspace.resize(dof, dof);

  m_d_theta.resize(dof);
  m_d_theta_tmp.resize(dof);
  m_d_norm_weight.resize(dof);

  m_norm.resize(dof);
  m_norm.setZero();

  m_beta.resize(task_size);

  m_weight.resize(dof);
  m_weight_sqrt.resize(dof);
  m_weight.setOnes();
  m_weight_sqrt.setOnes();

  if (task_size >= dof) {
    m_transpose = false;

    m_jacobian_tmp.resize(task_size, dof);

    m_svd_u.resize(task_size, dof);
    m_svd_v.resize(dof, dof);
    m_svd_w.resize(dof);

    m_svd_u_beta.resize(dof);
  }
  else {
    // use the SVD of the transpose jacobian, it works just as well
    // as the original, and often allows using smaller matrices.
    m_transpose = true;

    m_jacobian_tmp.resize(dof, task_size);

    m_svd_u.resize(task_size, task_size);
    m_svd_v.resize(dof, task_size);
    m_svd_w.resize(task_size);

    m_svd_u_beta.resize(task_size);
  }
}

void IK_QJacobian::SetBetas(int id, int, const Vector3d &v)
{
  m_beta[id + 0] = v.x();
  m_beta[id + 1] = v.y();
  m_beta[id + 2] = v.z();
}

void IK_QJacobian::SetDerivatives(int id, int dof_id, const Vector3d &v, double norm_weight)
{
  m_jacobian(id + 0, dof_id) = v.x() * m_weight_sqrt[dof_id];
  m_jacobian(id + 1, dof_id) = v.y() * m_weight_sqrt[dof_id];
  m_jacobian(id + 2, dof_id) = v.z() * m_weight_sqrt[dof_id];

  m_d_norm_weight[dof_id] = norm_weight;
}

void IK_QJacobian::Invert()
{
  if (m_transpose) {
    // SVD will decompose Jt into V*W*Ut with U,V orthogonal and W diagonal,
    // so J = U*W*Vt and Jinv = V*Winv*Ut
    Eigen::JacobiSVD<MatrixXd> svd(m_jacobian.transpose(),
                                   Eigen::ComputeThinU | Eigen::ComputeThinV);
    m_svd_u = svd.matrixV();
    m_svd_w = svd.singularValues();
    m_svd_v = svd.matrixU();
  }
  else {
    // SVD will decompose J into U*W*Vt with U,V orthogonal and W diagonal,
    // so Jinv = V*Winv*Ut
    Eigen::JacobiSVD<MatrixXd> svd(m_jacobian, Eigen::ComputeThinU | Eigen::ComputeThinV);
    m_svd_u = svd.matrixU();
    m_svd_w = svd.singularValues();
    m_svd_v = svd.matrixV();
  }

  if (m_sdls)
    InvertSDLS();
  else
    InvertDLS();
}

bool IK_QJacobian::ComputeNullProjection()
{
  double epsilon = 1e-10;

  // compute null space projection based on V
  int i, j, rank = 0;
  for (i = 0; i < m_svd_w.size(); i++)
    if (m_svd_w[i] > epsilon)
      rank++;

  if (rank < m_task_size)
    return false;

  MatrixXd basis(m_svd_v.rows(), rank);
  int b = 0;

  for (i = 0; i < m_svd_w.size(); i++)
    if (m_svd_w[i] > epsilon) {
      for (j = 0; j < m_svd_v.rows(); j++)
        basis(j, b) = m_svd_v(j, i);
      b++;
    }

  m_nullspace = basis * basis.transpose();

  for (i = 0; i < m_nullspace.rows(); i++)
    for (j = 0; j < m_nullspace.cols(); j++)
      if (i == j)
        m_nullspace(i, j) = 1.0 - m_nullspace(i, j);
      else
        m_nullspace(i, j) = -m_nullspace(i, j);

  return true;
}

void IK_QJacobian::SubTask(IK_QJacobian &jacobian)
{
  if (!ComputeNullProjection())
    return;

  // restrict lower priority jacobian
  jacobian.Restrict(m_d_theta, m_nullspace);

  // add angle update from lower priority
  jacobian.Invert();

  // note: now damps secondary angles with minimum damping value from
  // SDLS, to avoid shaking when the primary task is near singularities,
  // doesn't work well at all
  int i;
  for (i = 0; i < m_d_theta.size(); i++)
    m_d_theta[i] = m_d_theta[i] + /*m_min_damp * */ jacobian.AngleUpdate(i);
}

void IK_QJacobian::Restrict(VectorXd &d_theta, MatrixXd &nullspace)
{
  // subtract part already moved by higher task from beta
  m_beta = m_beta - m_jacobian * d_theta;

  // note: should we be using the norm of the unrestricted jacobian for SDLS?

  // project jacobian on to null space of higher priority task
  m_jacobian = m_jacobian * nullspace;
}

void IK_QJacobian::InvertSDLS()
{
  // Compute the dampeds least squeares pseudo inverse of J.
  //
  // Since J is usually not invertible (most of the times it's not even
  // square), the psuedo inverse is used. This gives us a least squares
  // solution.
  //
  // This is fine when the J*Jt is of full rank. When J*Jt is near to
  // singular the least squares inverse tries to minimize |J(dtheta) - dX)|
  // and doesn't try to minimize  dTheta. This results in eratic changes in
  // angle. The damped least squares minimizes |dtheta| to try and reduce this
  // erratic behavior.
  //
  // The selectively damped least squares (SDLS) is used here instead of the
  // DLS. The SDLS damps individual singular values, instead of using a single
  // damping term.

  double max_angle_change = M_PI / 4.0;
  double epsilon = 1e-10;
  int i, j;

  m_d_theta.setZero();
  m_min_damp = 1.0;

  for (i = 0; i < m_dof; i++) {
    m_norm[i] = 0.0;
    for (j = 0; j < m_task_size; j += 3) {
      double n = 0.0;
      n += m_jacobian(j, i) * m_jacobian(j, i);
      n += m_jacobian(j + 1, i) * m_jacobian(j + 1, i);
      n += m_jacobian(j + 2, i) * m_jacobian(j + 2, i);
      m_norm[i] += sqrt(n);
    }
  }

  for (i = 0; i < m_svd_w.size(); i++) {
    if (m_svd_w[i] <= epsilon)
      continue;

    double wInv = 1.0 / m_svd_w[i];
    double alpha = 0.0;
    double N = 0.0;

    // compute alpha and N
    for (j = 0; j < m_svd_u.rows(); j += 3) {
      alpha += m_svd_u(j, i) * m_beta[j];
      alpha += m_svd_u(j + 1, i) * m_beta[j + 1];
      alpha += m_svd_u(j + 2, i) * m_beta[j + 2];

      // note: for 1 end effector, N will always be 1, since U is
      // orthogonal, .. so could be optimized
      double tmp;
      tmp = m_svd_u(j, i) * m_svd_u(j, i);
      tmp += m_svd_u(j + 1, i) * m_svd_u(j + 1, i);
      tmp += m_svd_u(j + 2, i) * m_svd_u(j + 2, i);
      N += sqrt(tmp);
    }
    alpha *= wInv;

    // compute M, dTheta and max_dtheta
    double M = 0.0;
    double max_dtheta = 0.0, abs_dtheta;

    for (j = 0; j < m_d_theta.size(); j++) {
      double v = m_svd_v(j, i);
      M += fabs(v) * m_norm[j];

      // compute tmporary dTheta's
      m_d_theta_tmp[j] = v * alpha;

      // find largest absolute dTheta
      // multiply with weight to prevent unnecessary damping
      abs_dtheta = fabs(m_d_theta_tmp[j]) * m_weight_sqrt[j];
      if (abs_dtheta > max_dtheta)
        max_dtheta = abs_dtheta;
    }

    M *= wInv;

    // compute damping term and damp the dTheta's
    double gamma = max_angle_change;
    if (N < M)
      gamma *= N / M;

    double damp = (gamma < max_dtheta) ? gamma / max_dtheta : 1.0;

    for (j = 0; j < m_d_theta.size(); j++) {
      // slight hack: we do 0.80*, so that if there is some oscillation,
      // the system can still converge (for joint limits). also, it's
      // better to go a little to slow than to far

      double dofdamp = damp / m_weight[j];
      if (dofdamp > 1.0)
        dofdamp = 1.0;

      m_d_theta[j] += 0.80 * dofdamp * m_d_theta_tmp[j];
    }

    if (damp < m_min_damp)
      m_min_damp = damp;
  }

  // weight + prevent from doing angle updates with angles > max_angle_change
  double max_angle = 0.0, abs_angle;

  for (j = 0; j < m_dof; j++) {
    m_d_theta[j] *= m_weight[j];

    abs_angle = fabs(m_d_theta[j]);

    if (abs_angle > max_angle)
      max_angle = abs_angle;
  }

  if (max_angle > max_angle_change) {
    double damp = (max_angle_change) / (max_angle_change + max_angle);

    for (j = 0; j < m_dof; j++)
      m_d_theta[j] *= damp;
  }
}

void IK_QJacobian::InvertDLS()
{
  // Compute damped least squares inverse of pseudo inverse
  // Compute damping term lambda

  // Note when lambda is zero this is equivalent to the
  // least squares solution. This is fine when the m_jjt is
  // of full rank. When m_jjt is near to singular the least squares
  // inverse tries to minimize |J(dtheta) - dX)| and doesn't
  // try to minimize  dTheta. This results in eratic changes in angle.
  // Damped least squares minimizes |dtheta| to try and reduce this
  // erratic behavior.

  // We don't want to use the damped solution everywhere so we
  // only increase lamda from zero as we approach a singularity.

  // find the smallest non-zero W value, anything below epsilon is
  // treated as zero

  double epsilon = 1e-10;
  double max_angle_change = 0.1;
  double x_length = sqrt(m_beta.dot(m_beta));

  int i, j;
  double w_min = std::numeric_limits<double>::max();

  for (i = 0; i < m_svd_w.size(); i++) {
    if (m_svd_w[i] > epsilon && m_svd_w[i] < w_min)
      w_min = m_svd_w[i];
  }

  // compute lambda damping term

  double d = x_length / max_angle_change;
  double lambda;

  if (w_min <= d / 2)
    lambda = d / 2;
  else if (w_min < d)
    lambda = sqrt(w_min * (d - w_min));
  else
    lambda = 0.0;

  lambda *= lambda;

  if (lambda > 10)
    lambda = 10;

  // immediately multiply with Beta, so we can do matrix*vector products
  // rather than matrix*matrix products

  // compute Ut*Beta
  m_svd_u_beta = m_svd_u.transpose() * m_beta;

  m_d_theta.setZero();

  for (i = 0; i < m_svd_w.size(); i++) {
    if (m_svd_w[i] > epsilon) {
      double wInv = m_svd_w[i] / (m_svd_w[i] * m_svd_w[i] + lambda);

      // compute V*Winv*Ut*Beta
      m_svd_u_beta[i] *= wInv;

      for (j = 0; j < m_d_theta.size(); j++)
        m_d_theta[j] += m_svd_v(j, i) * m_svd_u_beta[i];
    }
  }

  for (j = 0; j < m_d_theta.size(); j++)
    m_d_theta[j] *= m_weight[j];
}

void IK_QJacobian::Lock(int dof_id, double delta)
{
  int i;

  for (i = 0; i < m_task_size; i++) {
    m_beta[i] -= m_jacobian(i, dof_id) * delta;
    m_jacobian(i, dof_id) = 0.0;
  }

  m_norm[dof_id] = 0.0;  // unneeded
  m_d_theta[dof_id] = 0.0;
}

double IK_QJacobian::AngleUpdate(int dof_id) const
{
  return m_d_theta[dof_id];
}

double IK_QJacobian::AngleUpdateNorm() const
{
  int i;
  double mx = 0.0, dtheta_abs;

  for (i = 0; i < m_d_theta.size(); i++) {
    dtheta_abs = fabs(m_d_theta[i] * m_d_norm_weight[i]);
    if (dtheta_abs > mx)
      mx = dtheta_abs;
  }

  return mx;
}

void IK_QJacobian::SetDoFWeight(int dof, double weight)
{
  m_weight[dof] = weight;
  m_weight_sqrt[dof] = sqrt(weight);
}
