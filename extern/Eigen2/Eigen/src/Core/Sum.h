// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_SUM_H
#define EIGEN_SUM_H

/***************************************************************************
* Part 1 : the logic deciding a strategy for vectorization and unrolling
***************************************************************************/

template<typename Derived>
struct ei_sum_traits
{
private:
  enum {
    PacketSize = ei_packet_traits<typename Derived::Scalar>::size
  };

public:
  enum {
    Vectorization = (int(Derived::Flags)&ActualPacketAccessBit)
                 && (int(Derived::Flags)&LinearAccessBit)
                  ? LinearVectorization
                  : NoVectorization
  };

private:
  enum {
    Cost = Derived::SizeAtCompileTime * Derived::CoeffReadCost
           + (Derived::SizeAtCompileTime-1) * NumTraits<typename Derived::Scalar>::AddCost,
    UnrollingLimit = EIGEN_UNROLLING_LIMIT * (int(Vectorization) == int(NoVectorization) ? 1 : int(PacketSize))
  };

public:
  enum {
    Unrolling = Cost <= UnrollingLimit
              ? CompleteUnrolling
              : NoUnrolling
  };
};

/***************************************************************************
* Part 2 : unrollers
***************************************************************************/

/*** no vectorization ***/

template<typename Derived, int Start, int Length>
struct ei_sum_novec_unroller
{
  enum {
    HalfLength = Length/2
  };

  typedef typename Derived::Scalar Scalar;

  inline static Scalar run(const Derived &mat)
  {
    return ei_sum_novec_unroller<Derived, Start, HalfLength>::run(mat)
         + ei_sum_novec_unroller<Derived, Start+HalfLength, Length-HalfLength>::run(mat);
  }
};

template<typename Derived, int Start>
struct ei_sum_novec_unroller<Derived, Start, 1>
{
  enum {
    col = Start / Derived::RowsAtCompileTime,
    row = Start % Derived::RowsAtCompileTime
  };

  typedef typename Derived::Scalar Scalar;

  inline static Scalar run(const Derived &mat)
  {
    return mat.coeff(row, col);
  }
};

/*** vectorization ***/
  
template<typename Derived, int Start, int Length>
struct ei_sum_vec_unroller
{
  enum {
    PacketSize = ei_packet_traits<typename Derived::Scalar>::size,
    HalfLength = Length/2
  };

  typedef typename Derived::Scalar Scalar;
  typedef typename ei_packet_traits<Scalar>::type PacketScalar;

  inline static PacketScalar run(const Derived &mat)
  {
    return ei_padd(
            ei_sum_vec_unroller<Derived, Start, HalfLength>::run(mat),
            ei_sum_vec_unroller<Derived, Start+HalfLength, Length-HalfLength>::run(mat) );
  }
};

template<typename Derived, int Start>
struct ei_sum_vec_unroller<Derived, Start, 1>
{
  enum {
    index = Start * ei_packet_traits<typename Derived::Scalar>::size,
    row = int(Derived::Flags)&RowMajorBit
        ? index / int(Derived::ColsAtCompileTime)
        : index % Derived::RowsAtCompileTime,
    col = int(Derived::Flags)&RowMajorBit
        ? index % int(Derived::ColsAtCompileTime)
        : index / Derived::RowsAtCompileTime,
    alignment = (Derived::Flags & AlignedBit) ? Aligned : Unaligned
  };

  typedef typename Derived::Scalar Scalar;
  typedef typename ei_packet_traits<Scalar>::type PacketScalar;

  inline static PacketScalar run(const Derived &mat)
  {
    return mat.template packet<alignment>(row, col);
  }
};

/***************************************************************************
* Part 3 : implementation of all cases
***************************************************************************/

template<typename Derived,
         int Vectorization = ei_sum_traits<Derived>::Vectorization,
         int Unrolling = ei_sum_traits<Derived>::Unrolling
>
struct ei_sum_impl;

template<typename Derived>
struct ei_sum_impl<Derived, NoVectorization, NoUnrolling>
{
  typedef typename Derived::Scalar Scalar;
  static Scalar run(const Derived& mat)
  {
    ei_assert(mat.rows()>0 && mat.cols()>0 && "you are using a non initialized matrix");
    Scalar res;
    res = mat.coeff(0, 0);
    for(int i = 1; i < mat.rows(); ++i)
      res += mat.coeff(i, 0);
    for(int j = 1; j < mat.cols(); ++j)
      for(int i = 0; i < mat.rows(); ++i)
        res += mat.coeff(i, j);
    return res;
  }
};

template<typename Derived>
struct ei_sum_impl<Derived, NoVectorization, CompleteUnrolling>
  : public ei_sum_novec_unroller<Derived, 0, Derived::SizeAtCompileTime>
{};

template<typename Derived>
struct ei_sum_impl<Derived, LinearVectorization, NoUnrolling>
{
  typedef typename Derived::Scalar Scalar;
  typedef typename ei_packet_traits<Scalar>::type PacketScalar;

  static Scalar run(const Derived& mat)
  {
    const int size = mat.size();
    const int packetSize = ei_packet_traits<Scalar>::size;
    const int alignedStart =  (Derived::Flags & AlignedBit)
                           || !(Derived::Flags & DirectAccessBit)
                           ? 0
                           : ei_alignmentOffset(&mat.const_cast_derived().coeffRef(0), size);
    enum {
      alignment = (Derived::Flags & DirectAccessBit) || (Derived::Flags & AlignedBit)
                ? Aligned : Unaligned
    };
    const int alignedSize = ((size-alignedStart)/packetSize)*packetSize;
    const int alignedEnd = alignedStart + alignedSize;
    Scalar res;

    if(alignedSize)
    {
      PacketScalar packet_res = mat.template packet<alignment>(alignedStart);
      for(int index = alignedStart + packetSize; index < alignedEnd; index += packetSize)
        packet_res = ei_padd(packet_res, mat.template packet<alignment>(index));
      res = ei_predux(packet_res);
    }
    else // too small to vectorize anything.
         // since this is dynamic-size hence inefficient anyway for such small sizes, don't try to optimize.
    {
      res = Scalar(0);
    }

    for(int index = 0; index < alignedStart; ++index)
      res += mat.coeff(index);

    for(int index = alignedEnd; index < size; ++index)
      res += mat.coeff(index);

    return res;
  }
};

template<typename Derived>
struct ei_sum_impl<Derived, LinearVectorization, CompleteUnrolling>
{
  typedef typename Derived::Scalar Scalar;
  typedef typename ei_packet_traits<Scalar>::type PacketScalar;
  enum {
    PacketSize = ei_packet_traits<Scalar>::size,
    Size = Derived::SizeAtCompileTime,
    VectorizationSize = (Size / PacketSize) * PacketSize
  };
  static Scalar run(const Derived& mat)
  {
    Scalar res = ei_predux(ei_sum_vec_unroller<Derived, 0, Size / PacketSize>::run(mat));
    if (VectorizationSize != Size)
      res += ei_sum_novec_unroller<Derived, VectorizationSize, Size-VectorizationSize>::run(mat);
    return res;
  }
};

/***************************************************************************
* Part 4 : implementation of MatrixBase methods
***************************************************************************/

/** \returns the sum of all coefficients of *this
  *
  * \sa trace()
  */
template<typename Derived>
inline typename ei_traits<Derived>::Scalar
MatrixBase<Derived>::sum() const
{
  return ei_sum_impl<Derived>::run(derived());
}

/** \returns the trace of \c *this, i.e. the sum of the coefficients on the main diagonal.
  *
  * \c *this can be any matrix, not necessarily square.
  *
  * \sa diagonal(), sum()
  */
template<typename Derived>
inline typename ei_traits<Derived>::Scalar
MatrixBase<Derived>::trace() const
{
  return diagonal().sum();
}

#endif // EIGEN_SUM_H
