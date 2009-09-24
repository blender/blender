/* $Id: eigen_types.hpp 19905 2009-04-23 13:29:54Z ben2610 $
 * eigen_types.hpp
 *
 *  Created on: March 6, 2009
 *      Author: benoit bolsee
 */

#ifndef EIGEN_TYPES_HPP_
#define EIGEN_TYPES_HPP_

#include <Eigen/Core>
#include "kdl/frames.hpp"
#include "kdl/tree.hpp"
#include "kdl/chain.hpp"
#include "kdl/jacobian.hpp"
#include "kdl/jntarray.hpp"


namespace iTaSC{

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
using KDL::Vector2;
using KDL::Chain;

extern const Frame F_identity;

#define e_scalar double
#define e_vector Eigen::Matrix<e_scalar, Eigen::Dynamic, 1>
#define e_zero_vector Eigen::Matrix<e_scalar, Eigen::Dynamic, 1>::Zero
#define e_matrix Eigen::Matrix<e_scalar, Eigen::Dynamic, Eigen::Dynamic>
#define e_matrix6 Eigen::Matrix<e_scalar,6,6>
#define e_identity_matrix Eigen::Matrix<e_scalar, Eigen::Dynamic, Eigen::Dynamic>::Identity
#define e_scalar_vector Eigen::Matrix<e_scalar, Eigen::Dynamic, 1>::Constant
#define e_zero_matrix Eigen::Matrix<e_scalar, Eigen::Dynamic, Eigen::Dynamic>::Zero
#define e_random_matrix Eigen::Matrix<e_scalar, Eigen::Dynamic, Eigen::Dynamic>::Random
#define e_vector6 Eigen::Matrix<e_scalar,6,1>
#define e_vector3 Eigen::Matrix<e_scalar,3,1>

class Range {
public:
	int start;
	int count;
	Range(int _start, int _count) { start = _start; count=_count; }
	Range(const Range& other) { start=other.start; count=other.count; }
};

template<typename MatrixType> inline Eigen::Block<MatrixType> project(MatrixType& m, Range r)
{
	return Eigen::Block<MatrixType>(m,r.start,0,r.count,1);
}

template<typename MatrixType> inline Eigen::Block<MatrixType> project(MatrixType& m, Range r, Range c)
{
	return Eigen::Block<MatrixType>(m,r.start,c.start,r.count,c.count);
}

template<typename Derived> inline static int changeBase(Eigen::MatrixBase<Derived>& J, const Frame& T) {

    if (J.rows() != 6)
        return -1;
    for (int j = 0; j < J.cols(); ++j) {
		typename Derived::ColXpr Jj = J.col(j);
		Twist arg;
        for(unsigned int i=0;i<6;++i)
            arg(i)=Jj[i];
		Twist tmp(T*arg);
        for(unsigned int i=0;i<6;++i)
            Jj[i]=e_scalar(tmp(i));
    }
    return 0;
}

}
#endif /* UBLAS_TYPES_HPP_ */
