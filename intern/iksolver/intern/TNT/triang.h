/**
 */

/*

*
* Template Numerical Toolkit (TNT): Linear Algebra Module
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
* and is in the public domain.  The Template Numerical Toolkit (TNT) is
* an experimental system.  NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*
* BETA VERSION INCOMPLETE AND SUBJECT TO CHANGE
* see http://math.nist.gov/tnt for latest updates.
*
*/



// Triangular Matrices (Views and Adpators)

#ifndef TRIANG_H
#define TRIANG_H

// default to use lower-triangular portions of arrays
// for symmetric matrices.

namespace TNT
{

template <class MaTRiX>
class LowerTriangularView
{
    protected:


        const MaTRiX  &A_;
        const typename MaTRiX::element_type zero_;

    public:


    typedef typename MaTRiX::const_reference const_reference;
    typedef const typename MaTRiX::element_type element_type;
    typedef const typename MaTRiX::element_type value_type;
    typedef element_type T;

    Subscript dim(Subscript d) const {  return A_.dim(d); }
    Subscript lbound() const { return A_.lbound(); }
    Subscript num_rows() const { return A_.num_rows(); }
    Subscript num_cols() const { return A_.num_cols(); }
    
    
    // constructors

    LowerTriangularView(/*const*/ MaTRiX &A) : A_(A),  zero_(0) {}


    inline const_reference get(Subscript i, Subscript j) const
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(lbound()<=i);
        assert(i<=A_.num_rows() + lbound() - 1);
        assert(lbound()<=j);
        assert(j<=A_.num_cols() + lbound() - 1);
#endif
        if (i<j) 
            return zero_;
        else
            return A_(i,j);
    }


    inline const_reference operator() (Subscript i, Subscript j) const
    {
#ifdef TNT_BOUNDS_CHECK
        assert(lbound()<=i);
        assert(i<=A_.num_rows() + lbound() - 1);
        assert(lbound()<=j);
        assert(j<=A_.num_cols() + lbound() - 1);
#endif
        if (i<j) 
            return zero_;
        else
            return A_(i,j);
    }

#ifdef TNT_USE_REGIONS 

    typedef const_Region2D< LowerTriangularView<MaTRiX> > 
                    const_Region;

    const_Region operator()(/*const*/ Index1D &I,
            /*const*/ Index1D &J) const
    {
        return const_Region(*this, I, J);
    }

    const_Region operator()(Subscript i1, Subscript i2,
            Subscript j1, Subscript j2) const
    {
        return const_Region(*this, i1, i2, j1, j2);
    }



#endif
// TNT_USE_REGIONS

};


/* *********** Lower_triangular_view() algorithms ****************** */

template <class MaTRiX, class VecToR>
VecToR matmult(/*const*/ LowerTriangularView<MaTRiX> &A, VecToR &x)
{
    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    assert(N == x.dim());

    Subscript i, j;
    typename MaTRiX::element_type sum=0.0;
    VecToR result(M);

    Subscript start = A.lbound();
    Subscript Mend = M + A.lbound() -1 ;

    for (i=start; i<=Mend; i++)
    {
        sum = 0.0;
        for (j=start; j<=i; j++)
            sum = sum + A(i,j)*x(j);
        result(i) = sum;
    }

    return result;
}

template <class MaTRiX, class VecToR>
inline VecToR operator*(/*const*/ LowerTriangularView<MaTRiX> &A, VecToR &x)
{
    return matmult(A,x);
}

template <class MaTRiX>
class UnitLowerTriangularView
{
    protected:

        const MaTRiX  &A_;
        const typename MaTRiX::element_type zero;
        const typename MaTRiX::element_type one;

    public:

    typedef typename MaTRiX::const_reference const_reference;
    typedef typename MaTRiX::element_type element_type;
    typedef typename MaTRiX::element_type value_type;
    typedef element_type T;

    Subscript lbound() const { return 1; }
    Subscript dim(Subscript d) const {  return A_.dim(d); }
    Subscript num_rows() const { return A_.num_rows(); }
    Subscript num_cols() const { return A_.num_cols(); }

    
    // constructors

    UnitLowerTriangularView(/*const*/ MaTRiX &A) : A_(A), zero(0), one(1) {}


    inline const_reference get(Subscript i, Subscript j) const
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i<=A_.dim(1));
        assert(1<=j);
        assert(j<=A_.dim(2));
        assert(0<=i && i<A_.dim(0) && 0<=j && j<A_.dim(1));
#endif
        if (i>j)
            return A_(i,j);
        else if (i==j)
            return one;
        else 
            return zero;
    }


    inline const_reference operator() (Subscript i, Subscript j) const
    {
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i<=A_.dim(1));
        assert(1<=j);
        assert(j<=A_.dim(2));
#endif
        if (i>j)
            return A_(i,j);
        else if (i==j)
            return one;
        else 
            return zero;
    }


#ifdef TNT_USE_REGIONS 
  // These are the "index-aware" features

    typedef const_Region2D< UnitLowerTriangularView<MaTRiX> > 
                    const_Region;

    const_Region operator()(/*const*/ Index1D &I,
            /*const*/ Index1D &J) const
    {
        return const_Region(*this, I, J);
    }

    const_Region operator()(Subscript i1, Subscript i2,
            Subscript j1, Subscript j2) const
    {
        return const_Region(*this, i1, i2, j1, j2);
    }
#endif
// TNT_USE_REGIONS
};

template <class MaTRiX>
LowerTriangularView<MaTRiX> Lower_triangular_view(
    /*const*/ MaTRiX &A)
{
    return LowerTriangularView<MaTRiX>(A);
}


template <class MaTRiX>
UnitLowerTriangularView<MaTRiX> Unit_lower_triangular_view(
    /*const*/ MaTRiX &A)
{
    return UnitLowerTriangularView<MaTRiX>(A);
}

template <class MaTRiX, class VecToR>
VecToR matmult(/*const*/ UnitLowerTriangularView<MaTRiX> &A, VecToR &x)
{
    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    assert(N == x.dim());

    Subscript i, j;
    typename MaTRiX::element_type sum=0.0;
    VecToR result(M);

    Subscript start = A.lbound();
    Subscript Mend = M + A.lbound() -1 ;

    for (i=start; i<=Mend; i++)
    {
        sum = 0.0;
        for (j=start; j<i; j++)
            sum = sum + A(i,j)*x(j);
        result(i) = sum + x(i);
    }

    return result;
}

template <class MaTRiX, class VecToR>
inline VecToR operator*(/*const*/ UnitLowerTriangularView<MaTRiX> &A, VecToR &x)
{
    return matmult(A,x);
}


//********************** Algorithms *************************************



template <class MaTRiX>
std::ostream& operator<<(std::ostream &s, const LowerTriangularView<MaTRiX>&A)
{
    Subscript M=A.num_rows();
    Subscript N=A.num_cols();

    s << M << " " << N << endl;

    for (Subscript i=1; i<=M; i++)
    {
        for (Subscript j=1; j<=N; j++)
        {
            s << A(i,j) << " ";
        }
        s << endl;
    }


    return s;
}

template <class MaTRiX>
std::ostream& operator<<(std::ostream &s, 
    const UnitLowerTriangularView<MaTRiX>&A)
{
    Subscript M=A.num_rows();
    Subscript N=A.num_cols();

    s << M << " " << N << endl;

    for (Subscript i=1; i<=M; i++)
    {
        for (Subscript j=1; j<=N; j++)
        {
            s << A(i,j) << " ";
        }
        s << endl;
    }


    return s;
}



// ******************* Upper Triangular Section **************************

template <class MaTRiX>
class UpperTriangularView
{
    protected:


        /*const*/ MaTRiX  &A_;
        /*const*/ typename MaTRiX::element_type zero_;

    public:


    typedef typename MaTRiX::const_reference const_reference;
    typedef /*const*/ typename MaTRiX::element_type element_type;
    typedef /*const*/ typename MaTRiX::element_type value_type;
    typedef element_type T;

    Subscript dim(Subscript d) const {  return A_.dim(d); }
    Subscript lbound() const { return A_.lbound(); }
    Subscript num_rows() const { return A_.num_rows(); }
    Subscript num_cols() const { return A_.num_cols(); }
    
    
    // constructors

    UpperTriangularView(/*const*/ MaTRiX &A) : A_(A),  zero_(0) {}


    inline const_reference get(Subscript i, Subscript j) const
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(lbound()<=i);
        assert(i<=A_.num_rows() + lbound() - 1);
        assert(lbound()<=j);
        assert(j<=A_.num_cols() + lbound() - 1);
#endif
        if (i>j) 
            return zero_;
        else
            return A_(i,j);
    }


    inline const_reference operator() (Subscript i, Subscript j) const
    {
#ifdef TNT_BOUNDS_CHECK
        assert(lbound()<=i);
        assert(i<=A_.num_rows() + lbound() - 1);
        assert(lbound()<=j);
        assert(j<=A_.num_cols() + lbound() - 1);
#endif
        if (i>j) 
            return zero_;
        else
            return A_(i,j);
    }

#ifdef TNT_USE_REGIONS 

    typedef const_Region2D< UpperTriangularView<MaTRiX> > 
                    const_Region;

    const_Region operator()(const Index1D &I,
            const Index1D &J) const
    {
        return const_Region(*this, I, J);
    }

    const_Region operator()(Subscript i1, Subscript i2,
            Subscript j1, Subscript j2) const
    {
        return const_Region(*this, i1, i2, j1, j2);
    }



#endif
// TNT_USE_REGIONS

};


/* *********** Upper_triangular_view() algorithms ****************** */

template <class MaTRiX, class VecToR>
VecToR matmult(/*const*/ UpperTriangularView<MaTRiX> &A, VecToR &x)
{
    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    assert(N == x.dim());

    Subscript i, j;
    typename VecToR::element_type sum=0.0;
    VecToR result(M);

    Subscript start = A.lbound();
    Subscript Mend = M + A.lbound() -1 ;

    for (i=start; i<=Mend; i++)
    {
        sum = 0.0;
        for (j=i; j<=N; j++)
            sum = sum + A(i,j)*x(j);
        result(i) = sum;
    }

    return result;
}

template <class MaTRiX, class VecToR>
inline VecToR operator*(/*const*/ UpperTriangularView<MaTRiX> &A, VecToR &x)
{
    return matmult(A,x);
}

template <class MaTRiX>
class UnitUpperTriangularView
{
    protected:

        const MaTRiX  &A_;
        const typename MaTRiX::element_type zero;
        const typename MaTRiX::element_type one;

    public:

    typedef typename MaTRiX::const_reference const_reference;
    typedef typename MaTRiX::element_type element_type;
    typedef typename MaTRiX::element_type value_type;
    typedef element_type T;

    Subscript lbound() const { return 1; }
    Subscript dim(Subscript d) const {  return A_.dim(d); }
    Subscript num_rows() const { return A_.num_rows(); }
    Subscript num_cols() const { return A_.num_cols(); }

    
    // constructors

    UnitUpperTriangularView(/*const*/ MaTRiX &A) : A_(A), zero(0), one(1) {}


    inline const_reference get(Subscript i, Subscript j) const
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i<=A_.dim(1));
        assert(1<=j);
        assert(j<=A_.dim(2));
        assert(0<=i && i<A_.dim(0) && 0<=j && j<A_.dim(1));
#endif
        if (i<j)
            return A_(i,j);
        else if (i==j)
            return one;
        else 
            return zero;
    }


    inline const_reference operator() (Subscript i, Subscript j) const
    {
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i<=A_.dim(1));
        assert(1<=j);
        assert(j<=A_.dim(2));
#endif
        if (i<j)
            return A_(i,j);
        else if (i==j)
            return one;
        else 
            return zero;
    }


#ifdef TNT_USE_REGIONS 
  // These are the "index-aware" features

    typedef const_Region2D< UnitUpperTriangularView<MaTRiX> > 
                    const_Region;

    const_Region operator()(const Index1D &I,
            const Index1D &J) const
    {
        return const_Region(*this, I, J);
    }

    const_Region operator()(Subscript i1, Subscript i2,
            Subscript j1, Subscript j2) const
    {
        return const_Region(*this, i1, i2, j1, j2);
    }
#endif
// TNT_USE_REGIONS
};

template <class MaTRiX>
UpperTriangularView<MaTRiX> Upper_triangular_view(
    /*const*/ MaTRiX &A)
{
    return UpperTriangularView<MaTRiX>(A);
}


template <class MaTRiX>
UnitUpperTriangularView<MaTRiX> Unit_upper_triangular_view(
    /*const*/ MaTRiX &A)
{
    return UnitUpperTriangularView<MaTRiX>(A);
}

template <class MaTRiX, class VecToR>
VecToR matmult(/*const*/ UnitUpperTriangularView<MaTRiX> &A, VecToR &x)
{
    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    assert(N == x.dim());

    Subscript i, j;
    typename VecToR::element_type sum=0.0;
    VecToR result(M);

    Subscript start = A.lbound();
    Subscript Mend = M + A.lbound() -1 ;

    for (i=start; i<=Mend; i++)
    {
        sum = x(i);
        for (j=i+1; j<=N; j++)
            sum = sum + A(i,j)*x(j);
        result(i) = sum + x(i);
    }

    return result;
}

template <class MaTRiX, class VecToR>
inline VecToR operator*(/*const*/ UnitUpperTriangularView<MaTRiX> &A, VecToR &x)
{
    return matmult(A,x);
}


//********************** Algorithms *************************************



template <class MaTRiX>
std::ostream& operator<<(std::ostream &s, 
    /*const*/ UpperTriangularView<MaTRiX>&A)
{
    Subscript M=A.num_rows();
    Subscript N=A.num_cols();

    s << M << " " << N << endl;

    for (Subscript i=1; i<=M; i++)
    {
        for (Subscript j=1; j<=N; j++)
        {
            s << A(i,j) << " ";
        }
        s << endl;
    }


    return s;
}

template <class MaTRiX>
std::ostream& operator<<(std::ostream &s, 
        /*const*/ UnitUpperTriangularView<MaTRiX>&A)
{
    Subscript M=A.num_rows();
    Subscript N=A.num_cols();

    s << M << " " << N << endl;

    for (Subscript i=1; i<=M; i++)
    {
        for (Subscript j=1; j<=N; j++)
        {
            s << A(i,j) << " ";
        }
        s << endl;
    }


    return s;
}

} // namespace TNT

#endif //TRIANG_H

