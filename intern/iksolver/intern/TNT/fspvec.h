/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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


//  Templated sparse vector (Fortran conventions).
//  Used primarily to interface with Fortran sparse matrix libaries.
//  (CANNOT BE USED AS AN STL CONTAINER.)

#ifndef FSPVEC_H
#define FSPVEC_H

#include "tnt.h"
#include "vec.h"
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <strstream>

using namespace std;

namespace TNT
{

template <class T>
class Fortran_Sparse_Vector 
{


  public:

    typedef Subscript   size_type;
    typedef         T   value_type;
    typedef         T   element_type;
    typedef         T*  pointer;
    typedef         T*  iterator;
    typedef         T&  reference;
    typedef const   T*  const_iterator;
    typedef const   T&  const_reference;

    Subscript lbound() const { return 1;}
 
  protected:
    Vector<T>   val_;
    Vector<Subscript> index_;
    Subscript dim_;                  // prescribed dimension


  public:

    // size and shape information

    Subscript dim() const { return dim_; }
    Subscript num_nonzeros() const { return val_.dim(); }

    // access

    T& val(Subscript i) { return val_(i); }
    const T& val(Subscript i) const { return val_(i); }

    Subscript &index(Subscript i) { return index_(i); }
    const Subscript &index(Subscript i) const { return index_(i); }

    // constructors

    Fortran_Sparse_Vector() : val_(), index_(), dim_(0)  {};
    Fortran_Sparse_Vector(Subscript N, Subscript nz) : val_(nz), 
            index_(nz), dim_(N)  {};
    Fortran_Sparse_Vector(Subscript N, Subscript nz, const T *values,
        const Subscript *indices): val_(nz, values), index_(nz, indices),
            dim_(N) {}

    Fortran_Sparse_Vector(const Fortran_Sparse_Vector<T> &S): 
        val_(S.val_), index_(S.index_), dim_(S.dim_) {}

    // initialize from string, e.g.
    //
    //  Fortran_Sparse_Vector<T> A(N, 2, "1.0 2.1", "1 3");
    //
    Fortran_Sparse_Vector(Subscript N, Subscript nz, char *v,
        char *ind) : val_(nz, v), index_(nz, ind), dim_(N) {}
    
    // assignments

    Fortran_Sparse_Vector<T> & newsize(Subscript N, Subscript nz)
    {
        val_.newsize(nz);
        index_.newsize(nz);
        dim_ = N;
        return *this;
    }

    Fortran_Sparse_Vector<T> & operator=( const Fortran_Sparse_Vector<T> &A)
    {
        val_ = A.val_;
        index_ = A.index_;
        dim_ = A.dim_;

        return *this;
    }

    // methods



};


/* ***************************  I/O  ********************************/

template <class T>
ostream& operator<<(ostream &s, const Fortran_Sparse_Vector<T> &A)
{
    // output format is :   N nz val1 ind1 val2 ind2 ... 
    Subscript nz=A.num_nonzeros();

    s <<  A.dim() << " " << nz << endl;

    for (Subscript i=1; i<=nz; i++)
        s   << A.val(i) << "  " << A.index(i) << endl;
    s << endl;

    return s;
}


template <class T>
istream& operator>>(istream &s, Fortran_Sparse_Vector<T> &A)
{
    // output format is :   N nz val1 ind1 val2 ind2 ... 

    Subscript N;
    Subscript nz;

    s >> N >> nz;

    A.newsize(N, nz);

    for (Subscript i=1; i<=nz; i++)
            s >>  A.val(i) >> A.index(i);


    return s;
}

} // namespace TNT

#endif // FSPVEC_H

