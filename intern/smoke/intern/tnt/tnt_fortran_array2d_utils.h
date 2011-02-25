/** \file smoke/intern/tnt/tnt_fortran_array2d_utils.h
 *  \ingroup smoke
 */
/*
*
* Template Numerical Toolkit (TNT)
*
* Mathematical and Computational Sciences Division
* National Institute of Technology,
* Gaithersburg, MD USA
*
*
* This software was developed at the National Institute of Standards and
* Technology (NIST) by employees of the Federal Government in the course
* of their official duties. Pursuant to title 17 Section 105 of the
* United States Code, this software is not subject to copyright protection
* and is in the public domain. NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*
*/


#ifndef TNT_FORTRAN_ARRAY2D_UTILS_H
#define TNT_FORTRAN_ARRAY2D_UTILS_H

#include <iostream>

namespace TNT
{


template <class T>
std::ostream& operator<<(std::ostream &s, const Fortran_Array2D<T> &A)
{
    int M=A.dim1();
    int N=A.dim2();

    s << M << " " << N << "\n";

    for (int i=1; i<=M; i++)
    {
        for (int j=1; j<=N; j++)
        {
            s << A(i,j) << " ";
        }
        s << "\n";
    }


    return s;
}

template <class T>
std::istream& operator>>(std::istream &s, Fortran_Array2D<T> &A)
{

    int M, N;

    s >> M >> N;

	Fortran_Array2D<T> B(M,N);

    for (int i=1; i<=M; i++)
        for (int j=1; j<=N; j++)
        {
            s >>  B(i,j);
        }

	A = B;
    return s;
}




template <class T>
Fortran_Array2D<T> operator+(const Fortran_Array2D<T> &A, const Fortran_Array2D<T> &B)
{
	int m = A.dim1();
	int n = A.dim2();

	if (B.dim1() != m ||  B.dim2() != n )
		return Fortran_Array2D<T>();

	else
	{
		Fortran_Array2D<T> C(m,n);

		for (int i=1; i<=m; i++)
		{
			for (int j=1; j<=n; j++)
				C(i,j) = A(i,j) + B(i,j);
		}
		return C;
	}
}

template <class T>
Fortran_Array2D<T> operator-(const Fortran_Array2D<T> &A, const Fortran_Array2D<T> &B)
{
	int m = A.dim1();
	int n = A.dim2();

	if (B.dim1() != m ||  B.dim2() != n )
		return Fortran_Array2D<T>();

	else
	{
		Fortran_Array2D<T> C(m,n);

		for (int i=1; i<=m; i++)
		{
			for (int j=1; j<=n; j++)
				C(i,j) = A(i,j) - B(i,j);
		}
		return C;
	}
}


template <class T>
Fortran_Array2D<T> operator*(const Fortran_Array2D<T> &A, const Fortran_Array2D<T> &B)
{
	int m = A.dim1();
	int n = A.dim2();

	if (B.dim1() != m ||  B.dim2() != n )
		return Fortran_Array2D<T>();

	else
	{
		Fortran_Array2D<T> C(m,n);

		for (int i=1; i<=m; i++)
		{
			for (int j=1; j<=n; j++)
				C(i,j) = A(i,j) * B(i,j);
		}
		return C;
	}
}


template <class T>
Fortran_Array2D<T> operator/(const Fortran_Array2D<T> &A, const Fortran_Array2D<T> &B)
{
	int m = A.dim1();
	int n = A.dim2();

	if (B.dim1() != m ||  B.dim2() != n )
		return Fortran_Array2D<T>();

	else
	{
		Fortran_Array2D<T> C(m,n);

		for (int i=1; i<=m; i++)
		{
			for (int j=1; j<=n; j++)
				C(i,j) = A(i,j) / B(i,j);
		}
		return C;
	}
}



template <class T>
Fortran_Array2D<T>&  operator+=(Fortran_Array2D<T> &A, const Fortran_Array2D<T> &B)
{
	int m = A.dim1();
	int n = A.dim2();

	if (B.dim1() == m ||  B.dim2() == n )
	{
		for (int i=1; i<=m; i++)
		{
			for (int j=1; j<=n; j++)
				A(i,j) += B(i,j);
		}
	}
	return A;
}

template <class T>
Fortran_Array2D<T>&  operator-=(Fortran_Array2D<T> &A, const Fortran_Array2D<T> &B)
{
	int m = A.dim1();
	int n = A.dim2();

	if (B.dim1() == m ||  B.dim2() == n )
	{
		for (int i=1; i<=m; i++)
		{
			for (int j=1; j<=n; j++)
				A(i,j) -= B(i,j);
		}
	}
	return A;
}

template <class T>
Fortran_Array2D<T>&  operator*=(Fortran_Array2D<T> &A, const Fortran_Array2D<T> &B)
{
	int m = A.dim1();
	int n = A.dim2();

	if (B.dim1() == m ||  B.dim2() == n )
	{
		for (int i=1; i<=m; i++)
		{
			for (int j=1; j<=n; j++)
				A(i,j) *= B(i,j);
		}
	}
	return A;
}

template <class T>
Fortran_Array2D<T>&  operator/=(Fortran_Array2D<T> &A, const Fortran_Array2D<T> &B)
{
	int m = A.dim1();
	int n = A.dim2();

	if (B.dim1() == m ||  B.dim2() == n )
	{
		for (int i=1; i<=m; i++)
		{
			for (int j=1; j<=n; j++)
				A(i,j) /= B(i,j);
		}
	}
	return A;
}

} // namespace TNT

#endif
