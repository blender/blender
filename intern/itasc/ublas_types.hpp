/*
 * ublas_types.hpp
 *
 *  Created on: Jan 5, 2009
 *      Author: rubensmits
 */

#ifndef UBLAS_TYPES_HPP_
#define UBLAS_TYPES_HPP_

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/vector_proxy.hpp>
#include "kdl/frames.hpp"
#include "kdl/tree.hpp"
#include "kdl/chain.hpp"
#include "kdl/jacobian.hpp"
#include "kdl/jntarray.hpp"


namespace iTaSC{

namespace ublas = boost::numeric::ublas;
using KDL::Twist;
using KDL::Frame;
using KDL::Joint;
using KDL::Inertia;
using KDL::SegmentMap;
using KDL::Tree;
using KDL::JntArray;
using KDL::Jacobian;
using KDL::Segment;
using KDL::Rotation;
using KDL::Vector;
using KDL::Chain;

#define u_scalar double
#define u_vector ublas::vector<u_scalar>
#define u_zero_vector ublas::zero_vector<u_scalar>
#define u_matrix ublas::matrix<u_scalar>
#define u_matrix6 ublas::matrix<u_scalar,6,6>
#define u_identity_matrix ublas::identity_matrix<u_scalar>
#define u_scalar_vector ublas::scalar_vector<u_scalar>
#define u_zero_matrix ublas::zero_matrix<u_scalar>
#define u_vector6 ublas::bounded_vector<u_scalar,6>

inline static int changeBase(const u_matrix& J_in, const Frame& T, u_matrix& J_out) {

    if (J_out.size1() != 6 || J_in.size1() != 6 || J_in.size2() != J_out.size2())
        return -1;
    for (unsigned int j = 0; j < J_in.size2(); ++j) {
        ublas::matrix_column<const u_matrix > Jj_in = column(J_in,j);
        ublas::matrix_column<u_matrix > Jj_out = column(J_out,j);
		Twist arg;
        for(unsigned int i=0;i<6;++i)
            arg(i)=Jj_in(i);
		Twist tmp(T*arg);
        for(unsigned int i=0;i<6;++i)
                    Jj_out(i)=tmp(i);
    }
    return 0;
}
inline static int changeBase(const ublas::matrix_range<u_matrix >& J_in, const Frame& T, ublas::matrix_range<u_matrix >& J_out) {

    if (J_out.size1() != 6 || J_in.size1() != 6 || J_in.size2() != J_out.size2())
        return -1;
    for (unsigned int j = 0; j < J_in.size2(); ++j) {
        ublas::matrix_column<const ublas::matrix_range<u_matrix > > Jj_in = column(J_in,j);
        ublas::matrix_column<ublas::matrix_range<u_matrix > > Jj_out = column(J_out,j);
		Twist arg;
        for(unsigned int i=0;i<6;++i)
            arg(i)=Jj_in(i);
		Twist tmp(T*arg);
        for(unsigned int i=0;i<6;++i)
                    Jj_out(i)=tmp(i);
    }
    return 0;
}

}
#endif /* UBLAS_TYPES_HPP_ */
