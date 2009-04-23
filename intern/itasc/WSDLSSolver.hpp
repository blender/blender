/* $Id$
 * WSDLSSolver.hpp
 *
 *  Created on: Mar 26, 2009
 *      Author: benoit bolsee
 */

#ifndef WSDLSSOLVER_HPP_
#define WSDLSSOLVER_HPP_

#include "Solver.hpp"

namespace iTaSC {

class WSDLSSolver: public iTaSC::Solver {
private:
    e_matrix m_AWq,m_WyAWq,m_U,m_V,m_WqV;
    e_vector m_S,m_temp,m_Wy_ydot;
	std::vector<bool> m_ytask;
	e_scalar m_qmax;
	unsigned int m_ns, m_nc, m_nq;
public:
    WSDLSSolver();
    virtual ~WSDLSSolver();

    virtual bool init(unsigned int _nq, unsigned int _nc, const std::vector<bool>& gc);
    virtual bool solve(const e_matrix& A, const e_vector& Wy, const e_vector& ydot, const e_matrix& Wq, e_vector& qdot);

    void setQmax(double _qmax){m_qmax=_qmax;};

};

}

#endif /* WSDLSSOLVER_HPP_ */
