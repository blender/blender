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
 * Original author: Laurence
 */

#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <cmath>

using Eigen::Affine3d;
using Eigen::Matrix3d;
using Eigen::MatrixXd;
using Eigen::Vector3d;
using Eigen::VectorXd;

static const double IK_EPSILON = 1e-20;

static inline bool FuzzyZero(double x)
{
	return fabs(x) < IK_EPSILON;
}

static inline double Clamp(const double x, const double min, const double max)
{
	return (x < min) ? min : (x > max) ? max : x;
}

static inline Eigen::Matrix3d CreateMatrix(double xx, double xy, double xz,
                                           double yx, double yy, double yz,
                                           double zx, double zy, double zz)
{
	Eigen::Matrix3d M;
	M(0, 0) = xx; M(0, 1) = xy; M(0, 2) = xz;
	M(1, 0) = yx; M(1, 1) = yy; M(1, 2) = yz;
	M(2, 0) = zx; M(2, 1) = zy; M(2, 2) = zz;
	return M;
}

static inline Eigen::Matrix3d RotationMatrix(double sine, double cosine, int axis)
{
	if (axis == 0)
		return CreateMatrix(1.0, 0.0, 0.0,
		                    0.0, cosine, -sine,
		                    0.0, sine, cosine);
	else if (axis == 1)
		return CreateMatrix(cosine, 0.0, sine,
		                    0.0, 1.0, 0.0,
		                    -sine, 0.0, cosine);
	else
		return CreateMatrix(cosine, -sine, 0.0,
		                    sine, cosine, 0.0,
		                    0.0, 0.0, 1.0);
}

static inline Eigen::Matrix3d RotationMatrix(double angle, int axis)
{
	return RotationMatrix(sin(angle), cos(angle), axis);
}


static inline double EulerAngleFromMatrix(const Eigen::Matrix3d& R, int axis)
{
	double t = sqrt(R(0, 0) * R(0, 0) + R(0, 1) * R(0, 1));

	if (t > 16.0 * IK_EPSILON) {
		if      (axis == 0) return -atan2(R(1, 2), R(2, 2));
		else if (axis == 1) return  atan2(-R(0, 2), t);
		else                return -atan2(R(0, 1), R(0, 0));
	}
	else {
		if      (axis == 0) return -atan2(-R(2, 1), R(1, 1));
		else if (axis == 1) return  atan2(-R(0, 2), t);
		else                return 0.0f;
	}
}

static inline double safe_acos(double f)
{
	// acos that does not return NaN with rounding errors
	if (f <= -1.0)
		return M_PI;
	else if (f >= 1.0)
		return 0.0;
	else
		return acos(f);
}

static inline Eigen::Vector3d normalize(const Eigen::Vector3d& v)
{
	// a sane normalize function that doesn't give (1, 0, 0) in case
	// of a zero length vector
	double len = v.norm();
	return FuzzyZero(len) ?  Eigen::Vector3d(0, 0, 0) : Eigen::Vector3d(v / len);
}

static inline double angle(const Eigen::Vector3d& v1, const Eigen::Vector3d& v2)
{
	return safe_acos(v1.dot(v2));
}

static inline double ComputeTwist(const Eigen::Matrix3d& R)
{
	// qy and qw are the y and w components of the quaternion from R
	double qy = R(0, 2) - R(2, 0);
	double qw = R(0, 0) + R(1, 1) + R(2, 2) + 1;

	double tau = 2.0 * atan2(qy, qw);

	return tau;
}

static inline Eigen::Matrix3d ComputeTwistMatrix(double tau)
{
	return RotationMatrix(tau, 1);
}

static inline void RemoveTwist(Eigen::Matrix3d& R)
{
	// compute twist parameter
	double tau = ComputeTwist(R);

	// compute twist matrix
	Eigen::Matrix3d T = ComputeTwistMatrix(tau);

	// remove twist
	R = R * T.transpose();
}

static inline Eigen::Vector3d SphericalRangeParameters(const Eigen::Matrix3d& R)
{
	// compute twist parameter
	double tau = ComputeTwist(R);

	// compute swing parameters
	double num = 2.0 * (1.0 + R(1, 1));

	// singularity at pi
	if (fabs(num) < IK_EPSILON)
		// TODO: this does now rotation of size pi over z axis, but could
		// be any axis, how to deal with this i'm not sure, maybe don't
		// enforce limits at all then
		return Eigen::Vector3d(0.0, tau, 1.0);

	num = 1.0 / sqrt(num);
	double ax = -R(2, 1) * num;
	double az =  R(0, 1) * num;

	return Eigen::Vector3d(ax, tau, az);
}

static inline Eigen::Matrix3d ComputeSwingMatrix(double ax, double az)
{
	// length of (ax, 0, az) = sin(theta/2)
	double sine2 = ax * ax + az * az;
	double cosine2 = sqrt((sine2 >= 1.0) ? 0.0 : 1.0 - sine2);

	// compute swing matrix
	Eigen::Matrix3d S(Eigen::Quaterniond(-cosine2, ax, 0.0, az));

	return S;
}

static inline Eigen::Vector3d MatrixToAxisAngle(const Eigen::Matrix3d& R)
{
	Eigen::Vector3d delta = Eigen::Vector3d(R(2, 1) - R(1, 2),
	                              R(0, 2) - R(2, 0),
	                              R(1, 0) - R(0, 1));

	double c = safe_acos((R(0, 0) + R(1, 1) + R(2, 2) - 1) / 2);
	double l = delta.norm();
	
	if (!FuzzyZero(l))
		delta *= c / l;
	
	return delta;
}

static inline bool EllipseClamp(double& ax, double& az, double *amin, double *amax)
{
	double xlim, zlim, x, z;

	if (ax < 0.0) {
		x = -ax;
		xlim = -amin[0];
	}
	else {
		x = ax;
		xlim = amax[0];
	}

	if (az < 0.0) {
		z = -az;
		zlim = -amin[1];
	}
	else {
		z = az;
		zlim = amax[1];
	}

	if (FuzzyZero(xlim) || FuzzyZero(zlim)) {
		if (x <= xlim && z <= zlim)
			return false;

		if (x > xlim)
			x = xlim;
		if (z > zlim)
			z = zlim;
	}
	else {
		double invx = 1.0 / (xlim * xlim);
		double invz = 1.0 / (zlim * zlim);

		if ((x * x * invx + z * z * invz) <= 1.0)
			return false;

		if (FuzzyZero(x)) {
			x = 0.0;
			z = zlim;
		}
		else {
			double rico = z / x;
			double old_x = x;
			x = sqrt(1.0 / (invx + invz * rico * rico));
			if (old_x < 0.0)
				x = -x;
			z = rico * x;
		}
	}

	ax = (ax < 0.0) ? -x : x;
	az = (az < 0.0) ? -z : z;

	return true;
}

