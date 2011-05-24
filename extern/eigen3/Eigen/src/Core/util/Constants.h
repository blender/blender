// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2007-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_CONSTANTS_H
#define EIGEN_CONSTANTS_H

/** This value means that a quantity is not known at compile-time, and that instead the value is
  * stored in some runtime variable.
  *
  * Changing the value of Dynamic breaks the ABI, as Dynamic is often used as a template parameter for Matrix.
  */
const int Dynamic = -1;

/** This value means +Infinity; it is currently used only as the p parameter to MatrixBase::lpNorm<int>().
  * The value Infinity there means the L-infinity norm.
  */
const int Infinity = -1;

/** \defgroup flags Flags
  * \ingroup Core_Module
  *
  * These are the possible bits which can be OR'ed to constitute the flags of a matrix or
  * expression.
  *
  * It is important to note that these flags are a purely compile-time notion. They are a compile-time property of
  * an expression type, implemented as enum's. They are not stored in memory at runtime, and they do not incur any
  * runtime overhead.
  *
  * \sa MatrixBase::Flags
  */

/** \ingroup flags
  *
  * for a matrix, this means that the storage order is row-major.
  * If this bit is not set, the storage order is column-major.
  * For an expression, this determines the storage order of
  * the matrix created by evaluation of that expression. 
  * \sa \ref TopicStorageOrders */
const unsigned int RowMajorBit = 0x1;

/** \ingroup flags
  *
  * means the expression should be evaluated by the calling expression */
const unsigned int EvalBeforeNestingBit = 0x2;

/** \ingroup flags
  *
  * means the expression should be evaluated before any assignment */
const unsigned int EvalBeforeAssigningBit = 0x4;

/** \ingroup flags
  *
  * Short version: means the expression might be vectorized
  *
  * Long version: means that the coefficients can be handled by packets
  * and start at a memory location whose alignment meets the requirements
  * of the present CPU architecture for optimized packet access. In the fixed-size
  * case, there is the additional condition that it be possible to access all the
  * coefficients by packets (this implies the requirement that the size be a multiple of 16 bytes,
  * and that any nontrivial strides don't break the alignment). In the dynamic-size case,
  * there is no such condition on the total size and strides, so it might not be possible to access
  * all coeffs by packets.
  *
  * \note This bit can be set regardless of whether vectorization is actually enabled.
  *       To check for actual vectorizability, see \a ActualPacketAccessBit.
  */
const unsigned int PacketAccessBit = 0x8;

#ifdef EIGEN_VECTORIZE
/** \ingroup flags
  *
  * If vectorization is enabled (EIGEN_VECTORIZE is defined) this constant
  * is set to the value \a PacketAccessBit.
  *
  * If vectorization is not enabled (EIGEN_VECTORIZE is not defined) this constant
  * is set to the value 0.
  */
const unsigned int ActualPacketAccessBit = PacketAccessBit;
#else
const unsigned int ActualPacketAccessBit = 0x0;
#endif

/** \ingroup flags
  *
  * Short version: means the expression can be seen as 1D vector.
  *
  * Long version: means that one can access the coefficients
  * of this expression by coeff(int), and coeffRef(int) in the case of a lvalue expression. These
  * index-based access methods are guaranteed
  * to not have to do any runtime computation of a (row, col)-pair from the index, so that it
  * is guaranteed that whenever it is available, index-based access is at least as fast as
  * (row,col)-based access. Expressions for which that isn't possible don't have the LinearAccessBit.
  *
  * If both PacketAccessBit and LinearAccessBit are set, then the
  * packets of this expression can be accessed by packet(int), and writePacket(int) in the case of a
  * lvalue expression.
  *
  * Typically, all vector expressions have the LinearAccessBit, but there is one exception:
  * Product expressions don't have it, because it would be troublesome for vectorization, even when the
  * Product is a vector expression. Thus, vector Product expressions allow index-based coefficient access but
  * not index-based packet access, so they don't have the LinearAccessBit.
  */
const unsigned int LinearAccessBit = 0x10;

/** \ingroup flags
  *
  * Means the expression has a coeffRef() method, i.e. is writable as its individual coefficients are directly addressable.
  * This rules out read-only expressions.
  *
  * Note that DirectAccessBit and LvalueBit are mutually orthogonal, as there are examples of expression having one but note
  * the other:
  *   \li writable expressions that don't have a very simple memory layout as a strided array, have LvalueBit but not DirectAccessBit
  *   \li Map-to-const expressions, for example Map<const Matrix>, have DirectAccessBit but not LvalueBit
  *
  * Expressions having LvalueBit also have their coeff() method returning a const reference instead of returning a new value.
  */
const unsigned int LvalueBit = 0x20;

/** \ingroup flags
  *
  * Means that the underlying array of coefficients can be directly accessed as a plain strided array. The memory layout
  * of the array of coefficients must be exactly the natural one suggested by rows(), cols(),
  * outerStride(), innerStride(), and the RowMajorBit. This rules out expressions such as Diagonal, whose coefficients,
  * though referencable, do not have such a regular memory layout.
  *
  * See the comment on LvalueBit for an explanation of how LvalueBit and DirectAccessBit are mutually orthogonal.
  */
const unsigned int DirectAccessBit = 0x40;

/** \ingroup flags
  *
  * means the first coefficient packet is guaranteed to be aligned */
const unsigned int AlignedBit = 0x80;

const unsigned int NestByRefBit = 0x100;

// list of flags that are inherited by default
const unsigned int HereditaryBits = RowMajorBit
                                  | EvalBeforeNestingBit
                                  | EvalBeforeAssigningBit;

// Possible values for the Mode parameter of triangularView()
enum {
  Lower=0x1, Upper=0x2, UnitDiag=0x4, ZeroDiag=0x8,
  UnitLower=UnitDiag|Lower, UnitUpper=UnitDiag|Upper,
  StrictlyLower=ZeroDiag|Lower, StrictlyUpper=ZeroDiag|Upper,
  SelfAdjoint=0x10};

enum { Unaligned=0, Aligned=1 };
enum { ConditionalJumpCost = 5 };

// FIXME after the corner() API change, this was not needed anymore, except by AlignedBox
// TODO: find out what to do with that. Adapt the AlignedBox API ?
enum CornerType { TopLeft, TopRight, BottomLeft, BottomRight };

enum DirectionType { Vertical, Horizontal, BothDirections };
enum ProductEvaluationMode { NormalProduct, CacheFriendlyProduct };

enum {
  /** \internal Default traversal, no vectorization, no index-based access */
  DefaultTraversal,
  /** \internal No vectorization, use index-based access to have only one for loop instead of 2 nested loops */
  LinearTraversal,
  /** \internal Equivalent to a slice vectorization for fixed-size matrices having good alignment
    * and good size */
  InnerVectorizedTraversal,
  /** \internal Vectorization path using a single loop plus scalar loops for the
    * unaligned boundaries */
  LinearVectorizedTraversal,
  /** \internal Generic vectorization path using one vectorized loop per row/column with some
    * scalar loops to handle the unaligned boundaries */
  SliceVectorizedTraversal,
  /** \internal Special case to properly handle incompatible scalar types or other defecting cases*/
  InvalidTraversal
};

enum {
  NoUnrolling,
  InnerUnrolling,
  CompleteUnrolling
};

enum {
  ColMajor = 0,
  RowMajor = 0x1,  // it is only a coincidence that this is equal to RowMajorBit -- don't rely on that
  /** \internal Align the matrix itself if it is vectorizable fixed-size */
  AutoAlign = 0,
  /** \internal Don't require alignment for the matrix itself (the array of coefficients, if dynamically allocated, may still be requested to be aligned) */ // FIXME --- clarify the situation
  DontAlign = 0x2
};

/** \brief Enum for specifying whether to apply or solve on the left or right. 
  */
enum {
  OnTheLeft = 1,  /**< \brief Apply transformation on the left. */
  OnTheRight = 2  /**< \brief Apply transformation on the right. */
};

/* the following could as well be written:
 *   enum NoChange_t { NoChange };
 * but it feels dangerous to disambiguate overloaded functions on enum/integer types.
 * If on some platform it is really impossible to get rid of "unused variable" warnings, then
 * we can always come back to that solution.
 */
struct NoChange_t {};
namespace {
  EIGEN_UNUSED NoChange_t NoChange;
}

struct Sequential_t {};
namespace {
  EIGEN_UNUSED Sequential_t Sequential;
}

struct Default_t {};
namespace {
  EIGEN_UNUSED Default_t Default;
}

enum {
  IsDense         = 0,
  IsSparse
};

enum AccessorLevels {
  ReadOnlyAccessors, WriteAccessors, DirectAccessors, DirectWriteAccessors
};

enum DecompositionOptions {
  Pivoting            = 0x01, // LDLT,
  NoPivoting          = 0x02, // LDLT,
  ComputeFullU        = 0x04, // SVD,
  ComputeThinU        = 0x08, // SVD,
  ComputeFullV        = 0x10, // SVD,
  ComputeThinV        = 0x20, // SVD,
  EigenvaluesOnly     = 0x40, // all eigen solvers
  ComputeEigenvectors = 0x80, // all eigen solvers
  EigVecMask = EigenvaluesOnly | ComputeEigenvectors,
  Ax_lBx              = 0x100,
  ABx_lx              = 0x200,
  BAx_lx              = 0x400,
  GenEigMask = Ax_lBx | ABx_lx | BAx_lx
};

enum QRPreconditioners {
  NoQRPreconditioner,
  HouseholderQRPreconditioner,
  ColPivHouseholderQRPreconditioner,
  FullPivHouseholderQRPreconditioner
};

/** \brief Enum for reporting the status of a computation.
  */
enum ComputationInfo {
  Success = 0,        /**< \brief Computation was successful. */
  NumericalIssue = 1, /**< \brief The provided data did not satisfy the prerequisites. */
  NoConvergence = 2   /**< \brief Iterative procedure did not converge. */
};

enum TransformTraits {
  Isometry      = 0x1,
  Affine        = 0x2,
  AffineCompact = 0x10 | Affine,
  Projective    = 0x20
};

namespace Architecture
{
  enum Type {
    Generic = 0x0,
    SSE = 0x1,
    AltiVec = 0x2,
#if defined EIGEN_VECTORIZE_SSE
    Target = SSE
#elif defined EIGEN_VECTORIZE_ALTIVEC
    Target = AltiVec
#else
    Target = Generic
#endif
  };
}

enum { CoeffBasedProductMode, LazyCoeffBasedProductMode, OuterProduct, InnerProduct, GemvProduct, GemmProduct };

enum Action {GetAction, SetAction};

/** The type used to identify a dense storage. */
struct Dense {};

/** The type used to identify a matrix expression */
struct MatrixXpr {};

/** The type used to identify an array expression */
struct ArrayXpr {};

#endif // EIGEN_CONSTANTS_H
