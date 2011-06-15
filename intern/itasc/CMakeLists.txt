# $Id: CMakeLists.txt 19905 2009-04-23 13:29:54Z ben2610 $
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../../extern/Eigen2
)

set(INC_SYS
	
)

set(SRC
	Armature.cpp
	Cache.cpp
	ConstraintSet.cpp
	ControlledObject.cpp
	CopyPose.cpp
	Distance.cpp
	FixedObject.cpp
	MovingFrame.cpp
	Scene.cpp
	UncontrolledObject.cpp
	WDLSSolver.cpp
	WSDLSSolver.cpp
	WorldObject.cpp
	eigen_types.cpp
	kdl/chain.cpp
	kdl/chainfksolverpos_recursive.cpp
	kdl/chainjnttojacsolver.cpp
	kdl/frameacc.cpp
	kdl/frames.cpp
	kdl/frames_io.cpp
	kdl/framevel.cpp
	kdl/inertia.cpp
	kdl/jacobian.cpp
	kdl/jntarray.cpp
	kdl/jntarrayacc.cpp
	kdl/jntarrayvel.cpp
	kdl/joint.cpp
	kdl/kinfam_io.cpp
	kdl/segment.cpp
	kdl/tree.cpp
	kdl/treefksolverpos_recursive.cpp
	kdl/treejnttojacsolver.cpp
	kdl/utilities/error_stack.cpp
	kdl/utilities/utility.cpp
	kdl/utilities/utility_io.cpp

	Armature.hpp
	Cache.hpp
	ConstraintSet.hpp
	ControlledObject.hpp
	CopyPose.hpp
	Distance.hpp
	FixedObject.hpp
	MovingFrame.hpp
	Object.hpp
	Scene.hpp
	Solver.hpp
	UncontrolledObject.hpp
	WDLSSolver.hpp
	WSDLSSolver.hpp
	WorldObject.hpp
	eigen_types.hpp
	kdl/chain.hpp
	kdl/chainfksolver.hpp
	kdl/chainfksolverpos_recursive.hpp
	kdl/chainjnttojacsolver.hpp
	kdl/frameacc.hpp
	kdl/frames.hpp
	kdl/frames_io.hpp
	kdl/framevel.hpp
	kdl/inertia.hpp
	kdl/jacobian.hpp
	kdl/jntarray.hpp
	kdl/jntarrayacc.hpp
	kdl/jntarrayvel.hpp
	kdl/joint.hpp
	kdl/kinfam_io.hpp
	kdl/segment.hpp
	kdl/tree.hpp
	kdl/treefksolver.hpp
	kdl/treefksolverpos_recursive.hpp
	kdl/treejnttojacsolver.hpp
	kdl/utilities/error.h
	kdl/utilities/error_stack.h
	kdl/utilities/kdl-config.h
	kdl/utilities/rall1d.h
	kdl/utilities/rall2d.h
	kdl/utilities/svd_eigen_HH.hpp
	kdl/utilities/traits.h
	kdl/utilities/utility.h
	kdl/utilities/utility_io.h
	ublas_types.hpp

	# until we have another user...
	../../extern/Eigen2/Eigen/src/Array/BooleanRedux.h
	../../extern/Eigen2/Eigen/src/Array/CwiseOperators.h
	../../extern/Eigen2/Eigen/src/Array/Functors.h
	../../extern/Eigen2/Eigen/src/Array/Norms.h
	../../extern/Eigen2/Eigen/src/Array/PartialRedux.h
	../../extern/Eigen2/Eigen/src/Array/Random.h
	../../extern/Eigen2/Eigen/src/Array/Select.h
	../../extern/Eigen2/Eigen/src/Cholesky/LDLT.h
	../../extern/Eigen2/Eigen/src/Cholesky/LLT.h
	../../extern/Eigen2/Eigen/src/Core/Assign.h
	../../extern/Eigen2/Eigen/src/Core/Block.h
	../../extern/Eigen2/Eigen/src/Core/CacheFriendlyProduct.h
	../../extern/Eigen2/Eigen/src/Core/Coeffs.h
	../../extern/Eigen2/Eigen/src/Core/CommaInitializer.h
	../../extern/Eigen2/Eigen/src/Core/Cwise.h
	../../extern/Eigen2/Eigen/src/Core/CwiseBinaryOp.h
	../../extern/Eigen2/Eigen/src/Core/CwiseNullaryOp.h
	../../extern/Eigen2/Eigen/src/Core/CwiseUnaryOp.h
	../../extern/Eigen2/Eigen/src/Core/DiagonalCoeffs.h
	../../extern/Eigen2/Eigen/src/Core/DiagonalMatrix.h
	../../extern/Eigen2/Eigen/src/Core/DiagonalProduct.h
	../../extern/Eigen2/Eigen/src/Core/Dot.h
	../../extern/Eigen2/Eigen/src/Core/Flagged.h
	../../extern/Eigen2/Eigen/src/Core/Functors.h
	../../extern/Eigen2/Eigen/src/Core/Fuzzy.h
	../../extern/Eigen2/Eigen/src/Core/GenericPacketMath.h
	../../extern/Eigen2/Eigen/src/Core/IO.h
	../../extern/Eigen2/Eigen/src/Core/Map.h
	../../extern/Eigen2/Eigen/src/Core/MapBase.h
	../../extern/Eigen2/Eigen/src/Core/MathFunctions.h
	../../extern/Eigen2/Eigen/src/Core/Matrix.h
	../../extern/Eigen2/Eigen/src/Core/MatrixBase.h
	../../extern/Eigen2/Eigen/src/Core/MatrixStorage.h
	../../extern/Eigen2/Eigen/src/Core/Minor.h
	../../extern/Eigen2/Eigen/src/Core/NestByValue.h
	../../extern/Eigen2/Eigen/src/Core/NumTraits.h
	../../extern/Eigen2/Eigen/src/Core/Part.h
	../../extern/Eigen2/Eigen/src/Core/Product.h
	../../extern/Eigen2/Eigen/src/Core/Redux.h
	../../extern/Eigen2/Eigen/src/Core/SolveTriangular.h
	../../extern/Eigen2/Eigen/src/Core/Sum.h
	../../extern/Eigen2/Eigen/src/Core/Swap.h
	../../extern/Eigen2/Eigen/src/Core/Transpose.h
	../../extern/Eigen2/Eigen/src/Core/Visitor.h
	../../extern/Eigen2/Eigen/src/Core/arch/AltiVec/PacketMath.h
	../../extern/Eigen2/Eigen/src/Core/arch/SSE/PacketMath.h
	../../extern/Eigen2/Eigen/src/Core/util/Constants.h
	../../extern/Eigen2/Eigen/src/Core/util/DisableMSVCWarnings.h
	../../extern/Eigen2/Eigen/src/Core/util/EnableMSVCWarnings.h
	../../extern/Eigen2/Eigen/src/Core/util/ForwardDeclarations.h
	../../extern/Eigen2/Eigen/src/Core/util/Macros.h
	../../extern/Eigen2/Eigen/src/Core/util/Memory.h
	../../extern/Eigen2/Eigen/src/Core/util/Meta.h
	../../extern/Eigen2/Eigen/src/Core/util/StaticAssert.h
	../../extern/Eigen2/Eigen/src/Core/util/XprHelper.h
	../../extern/Eigen2/Eigen/src/Geometry/AlignedBox.h
	../../extern/Eigen2/Eigen/src/Geometry/AngleAxis.h
	../../extern/Eigen2/Eigen/src/Geometry/EulerAngles.h
	../../extern/Eigen2/Eigen/src/Geometry/Hyperplane.h
	../../extern/Eigen2/Eigen/src/Geometry/OrthoMethods.h
	../../extern/Eigen2/Eigen/src/Geometry/ParametrizedLine.h
	../../extern/Eigen2/Eigen/src/Geometry/Quaternion.h
	../../extern/Eigen2/Eigen/src/Geometry/Rotation2D.h
	../../extern/Eigen2/Eigen/src/Geometry/RotationBase.h
	../../extern/Eigen2/Eigen/src/Geometry/Scaling.h
	../../extern/Eigen2/Eigen/src/Geometry/Transform.h
	../../extern/Eigen2/Eigen/src/Geometry/Translation.h
	../../extern/Eigen2/Eigen/src/LU/Determinant.h
	../../extern/Eigen2/Eigen/src/LU/Inverse.h
	../../extern/Eigen2/Eigen/src/LU/LU.h
	../../extern/Eigen2/Eigen/src/LeastSquares/LeastSquares.h
	../../extern/Eigen2/Eigen/src/QR/EigenSolver.h
	../../extern/Eigen2/Eigen/src/QR/HessenbergDecomposition.h
	../../extern/Eigen2/Eigen/src/QR/QR.h
	../../extern/Eigen2/Eigen/src/QR/SelfAdjointEigenSolver.h
	../../extern/Eigen2/Eigen/src/QR/Tridiagonalization.h
	../../extern/Eigen2/Eigen/src/SVD/SVD.h
	../../extern/Eigen2/Eigen/src/Sparse/AmbiVector.h
	../../extern/Eigen2/Eigen/src/Sparse/CholmodSupport.h
	../../extern/Eigen2/Eigen/src/Sparse/CompressedStorage.h
	../../extern/Eigen2/Eigen/src/Sparse/CoreIterators.h
	../../extern/Eigen2/Eigen/src/Sparse/DynamicSparseMatrix.h
	../../extern/Eigen2/Eigen/src/Sparse/MappedSparseMatrix.h
	../../extern/Eigen2/Eigen/src/Sparse/RandomSetter.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseAssign.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseBlock.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseCwise.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseCwiseBinaryOp.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseCwiseUnaryOp.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseDiagonalProduct.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseDot.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseFlagged.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseFuzzy.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseLDLT.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseLLT.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseLU.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseMatrix.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseMatrixBase.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseProduct.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseRedux.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseTranspose.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseUtil.h
	../../extern/Eigen2/Eigen/src/Sparse/SparseVector.h
	../../extern/Eigen2/Eigen/src/Sparse/SuperLUSupport.h
	../../extern/Eigen2/Eigen/src/Sparse/TaucsSupport.h
	../../extern/Eigen2/Eigen/src/Sparse/TriangularSolver.h
	../../extern/Eigen2/Eigen/src/Sparse/UmfPackSupport.h
)

blender_add_lib(bf_intern_itasc "${SRC}" "${INC}" "${INC_SYS}")
