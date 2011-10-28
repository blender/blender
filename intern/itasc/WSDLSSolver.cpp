/** \file itasc/WSDLSSolver.cpp
 *  \ingroup itasc
 */
/*
 * WDLSSolver.hpp.cpp
 *
 *  Created on: Jan 8, 2009
 *      Author: rubensmits
 */

#include "WSDLSSolver.hpp"
#include "kdl/utilities/svd_eigen_HH.hpp"
#include <cstdio>

namespace iTaSC {

WSDLSSolver::WSDLSSolver() :
	m_ns(0), m_nc(0), m_nq(0)

{
	// default maximum speed: 50 rad/s
	m_qmax = 50.0;
}

WSDLSSolver::~WSDLSSolver() {
}

bool WSDLSSolver::init(unsigned int _nq, unsigned int _nc, const std::vector<bool>& gc)
{
	if (_nc == 0 || _nq == 0 || gc.size() != _nc)
		return false;
	m_nc = _nc;
	m_nq = _nq;
	m_ns = std::min(m_nc,m_nq);
    m_AWq = e_zero_matrix(m_nc,m_nq);
    m_WyAWq = e_zero_matrix(m_nc,m_nq);
    m_WyAWqt = e_zero_matrix(m_nq,m_nc);
	m_S = e_zero_vector(std::max(m_nc,m_nq));
	m_Wy_ydot = e_zero_vector(m_nc);
	m_ytask = gc;
	if (m_nq > m_nc) {
		m_transpose = true;
	    m_temp = e_zero_vector(m_nc);
	    m_U = e_zero_matrix(m_nc,m_nc);
		m_V = e_zero_matrix(m_nq,m_nc);
	    m_WqV = e_zero_matrix(m_nq,m_nc);
	} else {
		m_transpose = false;
	    m_temp = e_zero_vector(m_nq);
	    m_U = e_zero_matrix(m_nc,m_nq);
		m_V = e_zero_matrix(m_nq,m_nq);
	    m_WqV = e_zero_matrix(m_nq,m_nq);
	}
    return true;
}

bool WSDLSSolver::solve(const e_matrix& A, const e_vector& Wy, const e_vector& ydot, const e_matrix& Wq, e_vector& qdot, e_scalar& nlcoef)
{
	unsigned int i, j, l;
	e_scalar N, M;

	// Create the Weighted jacobian
    m_AWq.noalias() = A*Wq;
	for (i=0; i<m_nc; i++)
		m_WyAWq.row(i) = Wy(i)*m_AWq.row(i);

    // Compute the SVD of the weighted jacobian
	int ret;
	if (m_transpose) {
		m_WyAWqt = m_WyAWq.transpose();
		ret = KDL::svd_eigen_HH(m_WyAWqt,m_V,m_S,m_U,m_temp);
	} else {
		ret = KDL::svd_eigen_HH(m_WyAWq,m_U,m_S,m_V,m_temp);
	}
    if(ret<0)
        return false;

	m_Wy_ydot = Wy.array() * ydot.array();
    m_WqV.noalias() = Wq*m_V;
	qdot.setZero();
	e_scalar maxDeltaS = e_scalar(0.0);
	e_scalar prevS = e_scalar(0.0);
	e_scalar maxS = e_scalar(1.0);
	for(i=0;i<m_ns;++i) {
		e_scalar norm, mag, alpha, _qmax, Sinv, vmax, damp;
		e_scalar S = m_S(i);
		bool prev;
		if (S < KDL::epsilon)
			break;
		Sinv = e_scalar(1.)/S;
		if (i > 0) {
			if ((prevS-S) > maxDeltaS) {
				maxDeltaS = (prevS-S);
				maxS = prevS;
			}
		}
		N = M = e_scalar(0.);
		for (l=0, prev=m_ytask[0], norm=e_scalar(0.); l<m_nc; l++) {
			if (prev == m_ytask[l]) {
				norm += m_U(l,i)*m_U(l,i);
			} else {
				N += std::sqrt(norm);
				norm = m_U(l,i)*m_U(l,i);
			}
			prev = m_ytask[l];
		}
		N += std::sqrt(norm);
		for (j=0; j<m_nq; j++) {
			for (l=0, prev=m_ytask[0], norm=e_scalar(0.), mag=e_scalar(0.); l<m_nc; l++) {
				if (prev == m_ytask[l]) {
					norm += m_WyAWq(l,j)*m_WyAWq(l,j);
				} else {
					mag += std::sqrt(norm);
					norm = m_WyAWq(l,j)*m_WyAWq(l,j);
				}
				prev = m_ytask[l];
			}
			mag += std::sqrt(norm);
			M += fabs(m_V(j,i))*mag;
		}
		M *= Sinv;
		alpha = m_U.col(i).dot(m_Wy_ydot);
		_qmax = (N < M) ? m_qmax*N/M : m_qmax;
		vmax = m_WqV.col(i).array().abs().maxCoeff();
		norm = fabs(Sinv*alpha*vmax);
		if (norm > _qmax) {
			damp = Sinv*alpha*_qmax/norm;
		} else {
			damp = Sinv*alpha;
		}
		qdot += m_WqV.col(i)*damp;
		prevS = S;
	}
	if (maxDeltaS == e_scalar(0.0))
		nlcoef = e_scalar(KDL::epsilon);
	else
		nlcoef = (maxS-maxDeltaS)/maxS;
    return true;
}

}
