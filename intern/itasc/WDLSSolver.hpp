/* $Id$
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
    e_matrix m_AWq,m_WyAWq,m_U,m_V,m_WyU,m_WqV;
    e_vector m_S,m_temp,m_WyUt_ydot,m_SinvWyUt_ydot;
    double m_lambda;
public:
    WDLSSolver();
    virtual ~WDLSSolver();

    virtual bool init(unsigned int nq, unsigned int nc, const std::vector<bool>& gc);
    virtual bool solve(const e_matrix& A, const e_vector& Wy, const e_vector& ydot, const e_matrix& Wq, e_vector& qdot, e_scalar& nlcoef);

    void setLambda(double _lambda){m_lambda=_lambda;};

};

}

#endif /* WDLSSOLVER_HPP_ */
