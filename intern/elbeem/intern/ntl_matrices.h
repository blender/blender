/** \file elbeem/intern/ntl_matrices.h
 *  \ingroup elbeem
 */

/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Basic matrix utility include file
 *
 *****************************************************************************/
#ifndef NTL_MATRICES_H

#include "ntl_vector3dim.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

// The basic vector class
template<class Scalar>
class ntlMatrix4x4
{
public:
  // Constructor
  inline ntlMatrix4x4(void );
  // Copy-Constructor
  inline ntlMatrix4x4(const ntlMatrix4x4<Scalar> &v );
  // construct a vector from one Scalar
  inline ntlMatrix4x4(Scalar);
  // construct a vector from three Scalars
  inline ntlMatrix4x4(Scalar, Scalar, Scalar);

  // Assignment operator
  inline const ntlMatrix4x4<Scalar>& operator=  (const ntlMatrix4x4<Scalar>& v);
  // Assignment operator
  inline const ntlMatrix4x4<Scalar>& operator=  (Scalar s);
  // Assign and add operator
  inline const ntlMatrix4x4<Scalar>& operator+= (const ntlMatrix4x4<Scalar>& v);
  // Assign and add operator
  inline const ntlMatrix4x4<Scalar>& operator+= (Scalar s);
  // Assign and sub operator
  inline const ntlMatrix4x4<Scalar>& operator-= (const ntlMatrix4x4<Scalar>& v);
  // Assign and sub operator
  inline const ntlMatrix4x4<Scalar>& operator-= (Scalar s);
  // Assign and mult operator
  inline const ntlMatrix4x4<Scalar>& operator*= (const ntlMatrix4x4<Scalar>& v);
  // Assign and mult operator
  inline const ntlMatrix4x4<Scalar>& operator*= (Scalar s);
  // Assign and div operator
  inline const ntlMatrix4x4<Scalar>& operator/= (const ntlMatrix4x4<Scalar>& v);
  // Assign and div operator
  inline const ntlMatrix4x4<Scalar>& operator/= (Scalar s);

  
  // unary operator
  inline ntlMatrix4x4<Scalar> operator- () const;

  // binary operator add
  inline ntlMatrix4x4<Scalar> operator+ (const ntlMatrix4x4<Scalar>&) const;
  // binary operator add
  inline ntlMatrix4x4<Scalar> operator+ (Scalar) const;
  // binary operator sub
  inline ntlMatrix4x4<Scalar> operator- (const ntlMatrix4x4<Scalar>&) const;
  // binary operator sub
  inline ntlMatrix4x4<Scalar> operator- (Scalar) const;
  // binary operator mult
  inline ntlMatrix4x4<Scalar> operator* (const ntlMatrix4x4<Scalar>&) const;
  // binary operator mult
  inline ntlVector3Dim<Scalar> operator* (const ntlVector3Dim<Scalar>&) const;
  // binary operator mult
  inline ntlMatrix4x4<Scalar> operator* (Scalar) const;
  // binary operator div
  inline ntlMatrix4x4<Scalar> operator/ (Scalar) const;

	// init function
	//! init identity matrix
	inline void initId();
	//! init rotation matrix
	inline void initTranslation(Scalar x, Scalar y, Scalar z);
	//! init rotation matrix
	inline void initRotationX(Scalar rot);
	inline void initRotationY(Scalar rot);
	inline void initRotationZ(Scalar rot);
	inline void initRotationXYZ(Scalar rotx,Scalar roty, Scalar rotz);
	//! init scaling matrix
	inline void initScaling(Scalar scale);
	inline void initScaling(Scalar x, Scalar y, Scalar z);
	//! from 16 value array (init id if all 0)
	inline void initArrayCheck(Scalar *array);

	//! decompose matrix
	void decompose(ntlVector3Dim<Scalar> &trans,ntlVector3Dim<Scalar> &scale,ntlVector3Dim<Scalar> &rot,ntlVector3Dim<Scalar> &shear);

	//! public to avoid [][] operators
  Scalar value[4][4];  //< Storage of vector values


protected:

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:ntlMatrix4x4")
#endif
};



//------------------------------------------------------------------------------
// TYPEDEFS
//------------------------------------------------------------------------------

// a 3D vector for graphics output, typically float?
//typedef ntlMatrix4x4<float>  ntlVec3Gfx; 

//typedef ntlMatrix4x4<double>  ntlMat4d; 
typedef ntlMatrix4x4<double>  ntlMat4d; 

// a 3D vector with single precision
typedef ntlMatrix4x4<float>   ntlMat4f; 

// a 3D vector with grafix precision
typedef ntlMatrix4x4<gfxReal>   ntlMat4Gfx;

// a 3D integer vector
typedef ntlMatrix4x4<int>     ntlMat4i; 





//------------------------------------------------------------------------------
// STREAM FUNCTIONS
//------------------------------------------------------------------------------



/*************************************************************************
  Outputs the object in human readable form using the format
  [x,y,z]
  */
template<class Scalar>
std::ostream&
operator<<( std::ostream& os, const ntlMatrix4x4<Scalar>& m )
{
	for(int i=0; i<4; i++) {
  	os << '|' << m.value[i][0] << ", " << m.value[i][1] << ", " << m.value[i][2] << ", " << m.value[i][3] << '|';
	}
  return os;
}



/*************************************************************************
  Reads the contents of the object from a stream using the same format
  as the output operator.
  */
template<class Scalar>
std::istream&
operator>>( std::istream& is, ntlMatrix4x4<Scalar>& m )
{
  char c;
  char dummy[3];
  
	for(int i=0; i<4; i++) {
  	is >> c >> m.value[i][0] >> dummy >> m.value[i][1] >> dummy >> m.value[i][2] >> dummy >> m.value[i][3] >> c;
	}
  return is;
}


//------------------------------------------------------------------------------
// VECTOR inline FUNCTIONS
//------------------------------------------------------------------------------



/*************************************************************************
  Constructor.
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>::ntlMatrix4x4( void )
{
#ifdef MATRIX_INIT_ZERO
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		value[i][j] = 0.0;
		}
	}
#endif
}



/*************************************************************************
  Copy-Constructor.
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>::ntlMatrix4x4( const ntlMatrix4x4<Scalar> &v )
{
  value[0][0] = v.value[0][0]; value[0][1] = v.value[0][1]; value[0][2] = v.value[0][2]; value[0][3] = v.value[0][3];
  value[1][0] = v.value[1][0]; value[1][1] = v.value[1][1]; value[1][2] = v.value[1][2]; value[1][3] = v.value[1][3];
  value[2][0] = v.value[2][0]; value[2][1] = v.value[2][1]; value[2][2] = v.value[2][2]; value[2][3] = v.value[2][3];
  value[3][0] = v.value[3][0]; value[3][1] = v.value[3][1]; value[3][2] = v.value[3][2]; value[3][3] = v.value[3][3];
}



/*************************************************************************
  Constructor for a vector from a single Scalar. All components of
  the vector get the same value.
  \param s The value to set
  \return The new vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>::ntlMatrix4x4(Scalar s )
{
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		value[i][j] = s;
		}
	}
}



/*************************************************************************
  Copy a ntlMatrix4x4 componentwise.
  \param v vector with values to be copied
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator=( const ntlMatrix4x4<Scalar> &v )
{
  value[0][0] = v.value[0][0]; value[0][1] = v.value[0][1]; value[0][2] = v.value[0][2]; value[0][3] = v.value[0][3];
  value[1][0] = v.value[1][0]; value[1][1] = v.value[1][1]; value[1][2] = v.value[1][2]; value[1][3] = v.value[1][3];
  value[2][0] = v.value[2][0]; value[2][1] = v.value[2][1]; value[2][2] = v.value[2][2]; value[2][3] = v.value[2][3];
  value[3][0] = v.value[3][0]; value[3][1] = v.value[3][1]; value[3][2] = v.value[3][2]; value[3][3] = v.value[3][3];
  return *this;
}


/*************************************************************************
  Copy a Scalar to each component.
  \param s The value to copy
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator=(Scalar s)
{
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		value[i][j] = s;
		}
	}
  return *this;
}


/*************************************************************************
  Add another ntlMatrix4x4 componentwise.
  \param v vector with values to be added
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator+=( const ntlMatrix4x4<Scalar> &v )
{
  value[0][0] += v.value[0][0]; value[0][1] += v.value[0][1]; value[0][2] += v.value[0][2]; value[0][3] += v.value[0][3];
  value[1][0] += v.value[1][0]; value[1][1] += v.value[1][1]; value[1][2] += v.value[1][2]; value[1][3] += v.value[1][3];
  value[2][0] += v.value[2][0]; value[2][1] += v.value[2][1]; value[2][2] += v.value[2][2]; value[2][3] += v.value[2][3];
  value[3][0] += v.value[3][0]; value[3][1] += v.value[3][1]; value[3][2] += v.value[3][2]; value[3][3] += v.value[3][3];
  return *this;
}


/*************************************************************************
  Add a Scalar value to each component.
  \param s Value to add
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator+=(Scalar s)
{
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		value[i][j] += s;
		}
	}
  return *this;
}


/*************************************************************************
  Subtract another vector componentwise.
  \param v vector of values to subtract
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator-=( const ntlMatrix4x4<Scalar> &v )
{
  value[0][0] -= v.value[0][0]; value[0][1] -= v.value[0][1]; value[0][2] -= v.value[0][2]; value[0][3] -= v.value[0][3];
  value[1][0] -= v.value[1][0]; value[1][1] -= v.value[1][1]; value[1][2] -= v.value[1][2]; value[1][3] -= v.value[1][3];
  value[2][0] -= v.value[2][0]; value[2][1] -= v.value[2][1]; value[2][2] -= v.value[2][2]; value[2][3] -= v.value[2][3];
  value[3][0] -= v.value[3][0]; value[3][1] -= v.value[3][1]; value[3][2] -= v.value[3][2]; value[3][3] -= v.value[3][3];
  return *this;
}


/*************************************************************************
  Subtract a Scalar value from each component.
  \param s Value to subtract
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator-=(Scalar s)
{
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		value[i][j] -= s;
		}
	}
  return *this;
}


/*************************************************************************
  Multiply with another vector componentwise.
  \param v vector of values to multiply with
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator*=( const ntlMatrix4x4<Scalar> &v )
{
	ntlMatrix4x4<Scalar> nv(0.0);
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {

			for(int k=0;k<4;k++)
				nv.value[i][j] += (value[i][k] * v.value[k][j]);
		}
	}
  *this = nv;
  return *this;
}


/*************************************************************************
  Multiply each component with a Scalar value.
  \param s Value to multiply with
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator*=(Scalar s)
{
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		value[i][j] *= s;
		}
	}
  return *this;
}



/*************************************************************************
  Divide each component by a Scalar value.
  \param s Value to divide by
  \return Reference to self
  */
template<class Scalar>
inline const ntlMatrix4x4<Scalar>&
ntlMatrix4x4<Scalar>::operator/=(Scalar s)
{
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		value[i][j] /= s;
		}
	}
  return *this;
}


//------------------------------------------------------------------------------
// unary operators
//------------------------------------------------------------------------------


/*************************************************************************
  Build componentwise the negative this vector.
  \return The new (negative) vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>
ntlMatrix4x4<Scalar>::operator-() const
{
	ntlMatrix4x4<Scalar> nv;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		nv[i][j] = -value[i][j];
		}
	}
  return nv;
}



//------------------------------------------------------------------------------
// binary operators
//------------------------------------------------------------------------------


/*************************************************************************
  Build a vector with another vector added componentwise.
  \param v The second vector to add
  \return The sum vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>
ntlMatrix4x4<Scalar>::operator+( const ntlMatrix4x4<Scalar> &v ) const
{
	ntlMatrix4x4<Scalar> nv;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		nv[i][j] = value[i][j] + v.value[i][j];
		}
	}
  return nv;
}


/*************************************************************************
  Build a vector with a Scalar value added to each component.
  \param s The Scalar value to add
  \return The sum vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>
ntlMatrix4x4<Scalar>::operator+(Scalar s) const
{
	ntlMatrix4x4<Scalar> nv;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		nv[i][j] = value[i][j] + s;
		}
	}
  return nv;
}


/*************************************************************************
  Build a vector with another vector subtracted componentwise.
  \param v The second vector to subtract
  \return The difference vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>
ntlMatrix4x4<Scalar>::operator-( const ntlMatrix4x4<Scalar> &v ) const
{
	ntlMatrix4x4<Scalar> nv;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		nv[i][j] = value[i][j] - v.value[i][j];
		}
	}
  return nv;
}


/*************************************************************************
  Build a vector with a Scalar value subtracted componentwise.
  \param s The Scalar value to subtract
  \return The difference vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>
ntlMatrix4x4<Scalar>::operator-(Scalar s ) const
{
	ntlMatrix4x4<Scalar> nv;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		nv[i][j] = value[i][j] - s;
		}
	}
  return nv;
}



/*************************************************************************
  Build a ntlMatrix4x4 with a Scalar value multiplied to each component.
  \param s The Scalar value to multiply with
  \return The product vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>
ntlMatrix4x4<Scalar>::operator*(Scalar s) const
{
	ntlMatrix4x4<Scalar> nv;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		nv[i][j] = value[i][j] * s;
		}
	}
  return nv;
}




/*************************************************************************
  Build a vector divided componentwise by a Scalar value.
  \param s The Scalar value to divide by
  \return The ratio vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>
ntlMatrix4x4<Scalar>::operator/(Scalar s) const
{
	ntlMatrix4x4<Scalar> nv;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
  		nv[i][j] = value[i][j] / s;
		}
	}
  return nv;
}





/*************************************************************************
  Build a vector with another vector multiplied by componentwise.
  \param v The second vector to muliply with
  \return The product vector
  */
template<class Scalar>
inline ntlMatrix4x4<Scalar>
ntlMatrix4x4<Scalar>::operator*( const ntlMatrix4x4<Scalar>& v) const
{
	ntlMatrix4x4<Scalar> nv(0.0);
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {

			for(int k=0;k<4;k++)
				nv.value[i][j] += (value[i][k] * v.value[k][j]);
		}
	}
  return nv;
}


template<class Scalar>
inline ntlVector3Dim<Scalar>
ntlMatrix4x4<Scalar>::operator*( const ntlVector3Dim<Scalar>& v) const
{
	ntlVector3Dim<Scalar> nvec(0.0);
	for(int i=0; i<3; i++) {
		for(int j=0; j<3; j++) {
			nvec[i] += (v[j] * value[i][j]);
		}
	}
	// assume normalized w coord
	for(int i=0; i<3; i++) {
		nvec[i] += (1.0 * value[i][3]);
	}
  return nvec;
}



//------------------------------------------------------------------------------
// Other helper functions
//------------------------------------------------------------------------------

//! init identity matrix
template<class Scalar>
inline void ntlMatrix4x4<Scalar>::initId()
{
	(*this) = (Scalar)(0.0);
	value[0][0] = 
	value[1][1] = 
	value[2][2] = 
	value[3][3] = (Scalar)(1.0);
}

//! init rotation matrix
template<class Scalar>
inline void ntlMatrix4x4<Scalar>::initTranslation(Scalar x, Scalar y, Scalar z)
{
	//(*this) = (Scalar)(0.0);
	this->initId();
	value[0][3] = x;
	value[1][3] = y;
	value[2][3] = z;
}

//! init rotation matrix
template<class Scalar>
inline void 
ntlMatrix4x4<Scalar>::initRotationX(Scalar rot)
{
	double drot = (double)(rot/360.0*2.0*M_PI);
	//? while(drot < 0.0) drot += (M_PI*2.0);

	this->initId();
	value[1][1] = (Scalar)  cos(drot);
	value[1][2] = (Scalar)  sin(drot);
	value[2][1] = (Scalar)(-sin(drot));
	value[2][2] = (Scalar)  cos(drot);
}
template<class Scalar>
inline void 
ntlMatrix4x4<Scalar>::initRotationY(Scalar rot)
{
	double drot = (double)(rot/360.0*2.0*M_PI);
	//? while(drot < 0.0) drot += (M_PI*2.0);

	this->initId();
	value[0][0] = (Scalar)  cos(drot);
	value[0][2] = (Scalar)(-sin(drot));
	value[2][0] = (Scalar)  sin(drot);
	value[2][2] = (Scalar)  cos(drot);
}
template<class Scalar>
inline void 
ntlMatrix4x4<Scalar>::initRotationZ(Scalar rot)
{
	double drot = (double)(rot/360.0*2.0*M_PI);
	//? while(drot < 0.0) drot += (M_PI*2.0);

	this->initId();
	value[0][0] = (Scalar)  cos(drot);
	value[0][1] = (Scalar)  sin(drot);
	value[1][0] = (Scalar)(-sin(drot));
	value[1][1] = (Scalar)  cos(drot);
}
template<class Scalar>
inline void 
ntlMatrix4x4<Scalar>::initRotationXYZ( Scalar rotx, Scalar roty, Scalar rotz)
{
	ntlMatrix4x4<Scalar> val;
	ntlMatrix4x4<Scalar> rot;
	this->initId();

	// org
	/*rot.initRotationX(rotx);
	(*this) *= rot;
	rot.initRotationY(roty);
	(*this) *= rot;
	rot.initRotationZ(rotz);
	(*this) *= rot;
	// org */

	// blender
	rot.initRotationZ(rotz);
	(*this) *= rot;
	rot.initRotationY(roty);
	(*this) *= rot;
	rot.initRotationX(rotx);
	(*this) *= rot;
	// blender */
}

//! init scaling matrix
template<class Scalar>
inline void 
ntlMatrix4x4<Scalar>::initScaling(Scalar scale)
{
	this->initId();
	value[0][0] = scale;
	value[1][1] = scale;
	value[2][2] = scale;
}
//! init scaling matrix
template<class Scalar>
inline void 
ntlMatrix4x4<Scalar>::initScaling(Scalar x, Scalar y, Scalar z)
{
	this->initId();
	value[0][0] = x;
	value[1][1] = y;
	value[2][2] = z;
}


//! from 16 value array (init id if all 0)
template<class Scalar>
inline void 
ntlMatrix4x4<Scalar>::initArrayCheck(Scalar *array)
{
	bool allZero = true;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
			value[i][j] = array[i*4+j];
			if(array[i*4+j]!=0.0) allZero=false;
		}
	}
	if(allZero) this->initId();
}

//! decompose matrix
template<class Scalar>
void 
ntlMatrix4x4<Scalar>::decompose(ntlVector3Dim<Scalar> &trans,ntlVector3Dim<Scalar> &scale,ntlVector3Dim<Scalar> &rot,ntlVector3Dim<Scalar> &shear) {
	ntlVec3Gfx row[3],temp;

	for(int i = 0; i < 3; i++) {
		trans[i] = this->value[3][i];
	}

	for(int i = 0; i < 3; i++) {
		row[i][0] = this->value[i][0];
		row[i][1] = this->value[i][1];
		row[i][2] = this->value[i][2];
	}

	scale[0] = norm(row[0]);
	normalize (row[0]);

	shear[0] = dot(row[0], row[1]);
	row[1][0] = row[1][0] - shear[0]*row[0][0];
	row[1][1] = row[1][1] - shear[0]*row[0][1];
	row[1][2] = row[1][2] - shear[0]*row[0][2];

	scale[1] = norm(row[1]);
	normalize (row[1]);

	if(scale[1] != 0.0)
		shear[0] /= scale[1];

	shear[1] = dot(row[0], row[2]);
	row[2][0] = row[2][0] - shear[1]*row[0][0];
	row[2][1] = row[2][1] - shear[1]*row[0][1];
	row[2][2] = row[2][2] - shear[1]*row[0][2];

	shear[2] = dot(row[1], row[2]);
	row[2][0] = row[2][0] - shear[2]*row[1][0];
	row[2][1] = row[2][1] - shear[2]*row[1][1];
	row[2][2] = row[2][2] - shear[2]*row[1][2];

	scale[2] = norm(row[2]);
	normalize (row[2]);

	if(scale[2] != 0.0) {
		shear[1] /= scale[2];
		shear[2] /= scale[2];
	}

	temp = cross(row[1], row[2]);
	if(dot(row[0], temp) < 0.0) {
		for(int i = 0; i < 3; i++) {
			scale[i]  *= -1.0;
			row[i][0] *= -1.0;
			row[i][1] *= -1.0;
			row[i][2] *= -1.0;
		}
	}

	if(row[0][2] < -1.0) row[0][2] = -1.0;
	if(row[0][2] > +1.0) row[0][2] = +1.0;

	rot[1] = asin(-row[0][2]);

	if(fabs(cos(rot[1])) > VECTOR_EPSILON) {
		rot[0] = atan2 (row[1][2], row[2][2]);
		rot[2] = atan2 (row[0][1], row[0][0]);
	}
	else {
		rot[0] = atan2 (row[1][0], row[1][1]);
		rot[2] = 0.0;
	}

	rot[0] = (180.0/M_PI)*rot[0];
	rot[1] = (180.0/M_PI)*rot[1];
	rot[2] = (180.0/M_PI)*rot[2];
} 

#define NTL_MATRICES_H
#endif

