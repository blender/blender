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



// Vector/Matrix/Array Index Module  

#ifndef INDEX_H
#define INDEX_H

#include "subscript.h"

namespace TNT
{

class Index1D
{
    Subscript lbound_;
    Subscript ubound_;

    public:

    Subscript lbound() const { return lbound_; }
    Subscript ubound() const { return ubound_; }

    Index1D(const Index1D &D) : lbound_(D.lbound_), ubound_(D.ubound_) {}
    Index1D(Subscript i1, Subscript i2) : lbound_(i1), ubound_(i2) {}

    Index1D & operator=(const Index1D &D)
    {
        lbound_ = D.lbound_;
        ubound_ = D.ubound_;
        return *this;
    }

};

inline Index1D operator+(const Index1D &D, Subscript i)
{
    return Index1D(i+D.lbound(), i+D.ubound());
}

inline Index1D operator+(Subscript i, const Index1D &D)
{
    return Index1D(i+D.lbound(), i+D.ubound());
}



inline Index1D operator-(Index1D &D, Subscript i)
{
    return Index1D(D.lbound()-i, D.ubound()-i);
}

inline Index1D operator-(Subscript i, Index1D &D)
{
    return Index1D(i-D.lbound(), i-D.ubound());
}

} // namespace TNT

#endif

