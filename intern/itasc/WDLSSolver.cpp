/* $Id$
 * WDLSSolver.hpp.cpp
 *
 *  Created on: Jan 8, 2009
 *      Author: rubensmits
 */

#include "WDLSSolver.hpp"
#include "kdl/utilities/svd_eigen_HH.hpp"

namespace iTaSC {

WDLSSolver::WDLSSolver() : m_lambda(0.0) {
}

WDLSSolver::~WDLSSolver() {
}

bool WDLSSolver::init(unsigned int nq, unsigned int nc, const std::vector<bool>& gc)
{
    m_AWq = e_zero_matrix(nc,nq);
    m_WyAWq = e_zero_matrix(nc,nq);
    m_U = e_zero_matrix(nc,nq);
	m_S = e_zero_vector(std::max(nc,nq));
    m_temp = e_zero_vector(nq);
    m_V = e_zero_matrix(nq,nq);
	m_WyU = e_zero_matrix(nc,std::min(nc,nq));
    m_WqV = e_zero_matrix(nq,nq);
	m_WyUt_ydot = e_zero_vector(std::min(nc,nq));
    m_SinvWyUt_ydot = e_zero_vector(nq);
	m_lambda = 0.01;
    return true;
}

bool WDLSSolver::solve(const e_matrix& A, const e_vector& Wy, const e_vector& ydot, const e_matrix& Wq, e_vector& qdot){
// Create the Weighted jacobian
    m_AWq = A*Wq;
	for (int i=0; i<Wy.size(); i++)
		m_WyAWq.row(i) = Wy(i)*m_AWq.row(i);

    // Compute the SVD of the weighted jacobian
	int ret = KDL::svd_eigen_HH(m_WyAWq,m_U,m_S,m_V,m_temp);
    if(ret<0)
        return false;

    //Pre-multiply m_U and V by the task space and joint space weighting matrix respectively
	Eigen::Block<e_matrix> U_reduct(m_U,0,0,m_U.rows(),m_WyU.cols());
	for (int i=0; i<Wy.size(); i++)
		m_WyU.row(i) = Wy(i)*U_reduct.row(i);
    m_WqV = (Wq*m_V).lazy();

    //U'*Wy'*ydot
	m_WyUt_ydot = (m_WyU.transpose()*ydot).lazy();
    //S^-1*U'*Wy'*ydot
    for(int i=0;i<m_WyUt_ydot.size();++i)
        m_SinvWyUt_ydot(i) = m_WyUt_ydot(i)*m_S(i)/(m_S(i)*m_S(i)+m_lambda*m_lambda);
    //qdot=Wq*V*S^-1*U'*Wy'*ydot
    qdot=(m_WqV*m_SinvWyUt_ydot).lazy();
    return true;
}

}
