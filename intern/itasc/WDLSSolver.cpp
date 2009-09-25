/* $Id$
 * WDLSSolver.hpp.cpp
 *
 *  Created on: Jan 8, 2009
 *      Author: rubensmits
 */

#include "WDLSSolver.hpp"
#include "kdl/utilities/svd_eigen_HH.hpp"

namespace iTaSC {

WDLSSolver::WDLSSolver() : m_lambda(0.5), m_epsilon(0.1) 
{
	// maximum joint velocity
	m_qmax = 50.0;
}

WDLSSolver::~WDLSSolver() {
}

bool WDLSSolver::init(unsigned int nq, unsigned int nc, const std::vector<bool>& gc)
{
	m_ns = std::min(nc,nq);
    m_AWq = e_zero_matrix(nc,nq);
    m_WyAWq = e_zero_matrix(nc,nq);
    m_U = e_zero_matrix(nc,nq);
	m_S = e_zero_vector(std::max(nc,nq));
    m_temp = e_zero_vector(nq);
    m_V = e_zero_matrix(nq,nq);
    m_WqV = e_zero_matrix(nq,nq);
	m_Wy_ydot = e_zero_vector(nc);
    return true;
}

bool WDLSSolver::solve(const e_matrix& A, const e_vector& Wy, const e_vector& ydot, const e_matrix& Wq, e_vector& qdot, e_scalar& nlcoef)
{
	double alpha, vmax, norm;
	// Create the Weighted jacobian
    m_AWq = A*Wq;
	for (int i=0; i<Wy.size(); i++)
		m_WyAWq.row(i) = Wy(i)*m_AWq.row(i);

    // Compute the SVD of the weighted jacobian
	int ret = KDL::svd_eigen_HH(m_WyAWq,m_U,m_S,m_V,m_temp);
    if(ret<0)
        return false;

    m_WqV = (Wq*m_V).lazy();

    //Wy*ydot
	m_Wy_ydot = Wy.cwise() * ydot;
    //S^-1*U'*Wy*ydot
	e_scalar maxDeltaS = e_scalar(0.0);
	e_scalar prevS = e_scalar(0.0);
	e_scalar maxS = e_scalar(1.0);
	e_scalar S, lambda;
	qdot.setZero();
	for(int i=0;i<m_ns;++i) {
		S = m_S(i);
		if (S <= KDL::epsilon)
			break;
		if (i > 0 && (prevS-S) > maxDeltaS) {
			maxDeltaS = (prevS-S);
			maxS = prevS;
		}
		lambda = (S < m_epsilon) ? (e_scalar(1.0)-KDL::sqr(S/m_epsilon))*m_lambda*m_lambda : e_scalar(0.0);
		alpha = m_U.col(i).dot(m_Wy_ydot)*S/(S*S+lambda);
		vmax = m_WqV.col(i).cwise().abs().maxCoeff();
		norm = fabs(alpha*vmax);
		if (norm > m_qmax) {
			qdot += m_WqV.col(i)*(alpha*m_qmax/norm);
		} else {
			qdot += m_WqV.col(i)*alpha;
		}
		prevS = S;
	}
	if (maxDeltaS == e_scalar(0.0))
		nlcoef = e_scalar(KDL::epsilon);
	else
		nlcoef = (maxS-maxDeltaS)/maxS;
    return true;
}

}
