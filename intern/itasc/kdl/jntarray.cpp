// Copyright  (C)  2007  Ruben Smits <ruben dot smits at mech dot kuleuven dot be>

// Version: 1.0
// Author: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// Maintainer: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// URL: http://www.orocos.org/kdl

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include "jntarray.hpp"

namespace KDL
{
    JntArray::JntArray():
            size(0),
            data(NULL)
    {
    }

    JntArray::JntArray(unsigned int _size):
        size(_size)
    {
        assert(0 < size);
        data = new double[size];
        SetToZero(*this);
    }


    JntArray::JntArray(const JntArray& arg):
        size(arg.size)
    {
        data = ((0 < size) ? new double[size] : NULL);
        for(unsigned int i=0;i<size;i++)
            data[i]=arg.data[i];
    }

    JntArray& JntArray::operator = (const JntArray& arg)
    {
        assert(size==arg.size);
        for(unsigned int i=0;i<size;i++)
            data[i]=arg.data[i];
        return *this;
    }


    JntArray::~JntArray()
    {
        delete [] data;
    }

    void JntArray::resize(unsigned int newSize)
    {
        delete [] data;
        size = newSize;
        data = new double[size];
        SetToZero(*this);
    }

    double JntArray::operator()(unsigned int i,unsigned int j)const
    {
        assert(i<size&&j==0);
        assert(0 != size);  // found JntArray containing no data
        return data[i];
    }

    double& JntArray::operator()(unsigned int i,unsigned int j)
    {
        assert(i<size&&j==0);
        assert(0 != size);  // found JntArray containing no data
        return data[i];
    }

    unsigned int JntArray::rows()const
    {
        return size;
    }

    unsigned int JntArray::columns()const
    {
        return 0;
    }

    void Add(const JntArray& src1,const JntArray& src2,JntArray& dest)
    {
        assert(src1.size==src2.size&&src1.size==dest.size);
        for(unsigned int i=0;i<dest.size;i++)
            dest.data[i]=src1.data[i]+src2.data[i];
    }

    void Subtract(const JntArray& src1,const JntArray& src2,JntArray& dest)
    {
        assert(src1.size==src2.size&&src1.size==dest.size);
        for(unsigned int i=0;i<dest.size;i++)
            dest.data[i]=src1.data[i]-src2.data[i];
    }

    void Multiply(const JntArray& src,const double& factor,JntArray& dest)
    {
        assert(src.size==dest.size);
        for(unsigned int i=0;i<dest.size;i++)
            dest.data[i]=factor*src.data[i];
    }

    void Divide(const JntArray& src,const double& factor,JntArray& dest)
    {
        assert(src.rows()==dest.size);
        for(unsigned int i=0;i<dest.size;i++)
            dest.data[i]=src.data[i]/factor;
    }

    void MultiplyJacobian(const Jacobian& jac, const JntArray& src, Twist& dest)
    {
        assert(jac.columns()==src.size);
        SetToZero(dest);
        for(unsigned int i=0;i<6;i++)
            for(unsigned int j=0;j<src.size;j++)
                dest(i)+=jac(i,j)*src.data[j];
    }

    void SetToZero(JntArray& array)
    {
        for(unsigned int i=0;i<array.size;i++)
            array.data[i]=0;
    }

    bool Equal(const JntArray& src1, const JntArray& src2,double eps)
    {
        assert(src1.size==src2.size);
        bool ret = true;
        for(unsigned int i=0;i<src1.size;i++)
            ret = ret && Equal(src1.data[i],src2.data[i],eps);
        return ret;
    }

    bool operator==(const JntArray& src1,const JntArray& src2){return Equal(src1,src2);};
    //bool operator!=(const JntArray& src1,const JntArray& src2){return Equal(src1,src2);};

}


