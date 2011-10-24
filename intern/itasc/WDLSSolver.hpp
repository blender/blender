/*
 * WDLSSolver.hpp
 *
 *  Created on: Jan 8, 2009
 *      Author: rubensmits
 */

#ifndef WDLSSOLVER_HPP_
#define WDLSSOLVER_HPP_

#include "Solver.hpp"

namespace iTaSC {

class WDLSSolver: public iTaSC::Solver {
private:
    e_matrix m_AWq,m_WyAWq,m_WyAWqt,m_U,m_V,m_WqV;
    e_vector m_S,m_temp,m_Wy_ydot;
    double m_lambda;
    double m_epsilon;
	double m_qmax;
	int m_ns;
	bool m_transpose;
public:
    WDLSSolver();
    virtual ~WDLSSolver();

    virtual bool init(unsigned int nq, unsigned int nc, const std::vector<bool>& gc);
    virtual bool solve(const e_matrix& A, const e_vector& Wy, const e_vector& ydot, const e_matrix& Wq, e_vector& qdot, e_scalar& nlcoef);
	virtual void setParam(SolverParam param, double value)
	{
		switch (param) {
		case DLS_QMAX:
			m_qmax = value;
			break;
		case DLS_LAMBDA_MAX:
			m_lambda = value;
			break;
		case DLS_EPSILON:
			m_epsilon = value;
			break;
		}	
	}
};

}

#endif /* WDLSSOLVER_HPP_ */
