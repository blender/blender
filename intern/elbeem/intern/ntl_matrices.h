
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Basic matrix utility include file
 *
 *****************************************************************************/
#ifndef NTL_MATRICES_H

#include "ntl_vector3dim.h"


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
	//! init scaling matrix
	inline void initScaling(Scalar scale);
	inline void initScaling(Scalar x, Scalar y, Scalar z);

	//! public to avoid [][] operators
  Scalar value[4][4];  //< Storage of vector values


protected:

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
	double drot = (double)rot;
	while(drot < 0.0) drot += (M_PI*2.0);

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
	double drot = (double)rot;
	while(drot < 0.0) drot += (M_PI*2.0);

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
	double drot = (double)rot;
	while(drot < 0.0) drot += (M_PI*2.0);

	this->initId();
	value[0][0] = (Scalar)  cos(drot);
	value[0][1] = (Scalar)  sin(drot);
	value[1][0] = (Scalar)(-sin(drot));
	value[1][1] = (Scalar)  cos(drot);
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




#define NTL_MATRICES_H
#endif

