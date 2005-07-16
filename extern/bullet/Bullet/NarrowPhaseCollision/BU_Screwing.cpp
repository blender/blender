/*
 * Copyright (c) 2005 Stephane Redon / Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#include "BU_Screwing.h"





BU_Screwing::BU_Screwing(const SimdVector3& relLinVel,const SimdVector3& relAngVel) {


	const SimdScalar dx=relLinVel[0];
	const SimdScalar dy=relLinVel[1];
	const SimdScalar dz=relLinVel[2];
	const SimdScalar wx=relAngVel[0];
	const SimdScalar wy=relAngVel[1];
	const SimdScalar wz=relAngVel[2];

	// Compute the screwing parameters :
	// w : total amount of rotation
	// s : total amount of translation
	// u : vector along the screwing axis (||u||=1)
	// o : point on the screwing axis
	
	m_w=sqrtf(wx*wx+wy*wy+wz*wz);
	//if (!w) {
	if (fabs(m_w)<SCREWEPSILON ) {

		assert(m_w == 0.f);

		m_w=0.;
		m_s=sqrtf(dx*dx+dy*dy+dz*dz);
		if (fabs(m_s)<SCREWEPSILON ) {
			assert(m_s == 0.);

			m_s=0.;
			m_u=SimdPoint3(0.,0.,1.);
			m_o=SimdPoint3(0.,0.,0.);
		}
		else {
			float t=1.f/m_s;
			m_u=SimdPoint3(dx*t,dy*t,dz*t);
			m_o=SimdPoint3(0.f,0.f,0.f);
		}
	}
	else { // there is some rotation
		
		// we compute u
		
		float v(1.f/m_w);
		m_u=SimdPoint3(wx*v,wy*v,wz*v); // normalization
		
		// decomposition of the translation along u and one orthogonal vector
		
		SimdPoint3 t(dx,dy,dz);
		m_s=t.dot(m_u); // component along u
		if (fabs(m_s)<SCREWEPSILON)
		{
			//printf("m_s component along u < SCREWEPSILION\n");
			m_s=0.f;
		}
		SimdPoint3 n1(t-(m_s*m_u)); // the remaining part (which is orthogonal to u)
		
		// now we have to compute o
		
		SimdScalar len = n1.length2();
		//(len >= BUM_EPSILON2) {
		if (n1[0] || n1[1] || n1[2]) { // n1 is not the zero vector
			n1.normalize();
			SimdVector3 n1orth=m_u.cross(n1);

			float n2x=cosf(0.5f*m_w);
			float n2y=sinf(0.5f*m_w);
			
			m_o=0.5f*t.dot(n1)*(n1+n2x/n2y*n1orth);
		}
		else 
		{
			m_o=SimdPoint3(0.f,0.f,0.f);
		}
		
	}
	
}

//Then, I need to compute Pa, the matrix from the reference (global) frame to
//the screwing frame :


void BU_Screwing::LocalMatrix(SimdTransform &t) const {
//So the whole computations do this : align the Oz axis along the
//	screwing axis (thanks to u), and then find two others orthogonal axes to
//	complete the basis.
		
		if ((m_u[0]>SCREWEPSILON)||(m_u[0]<-SCREWEPSILON)||(m_u[1]>SCREWEPSILON)||(m_u[1]<-SCREWEPSILON)) 
		{ 
			// to avoid numerical problems
			float n=sqrtf(m_u[0]*m_u[0]+m_u[1]*m_u[1]);
			float invn=1.0f/n;
			SimdMatrix3x3 mat;

	  		mat[0][0]=-m_u[1]*invn;
			mat[0][1]=m_u[0]*invn;
			mat[0][2]=0.f;
	
			mat[1][0]=-m_u[0]*invn*m_u[2];
			mat[1][1]=-m_u[1]*invn*m_u[2];
			mat[1][2]=n;

			mat[2][0]=m_u[0];
			mat[2][1]=m_u[1];
			mat[2][2]=m_u[2];
			
			t.setOrigin(SimdPoint3(
				  m_o[0]*m_u[1]*invn-m_o[1]*m_u[0]*invn,
				-(m_o[0]*mat[1][0]+m_o[1]*mat[1][1]+m_o[2]*n),
				-(m_o[0]*m_u[0]+m_o[1]*m_u[1]+m_o[2]*m_u[2])));

			t.setBasis(mat);

		}
		else {

			SimdMatrix3x3 m;

			m[0][0]=1.;
			m[1][0]=0.;
			m[2][0]=0.;

			m[0][1]=0.f;
			m[1][1]=float(SimdSign(m_u[2]));
			m[2][1]=0.f;

			m[0][2]=0.f;
			m[1][2]=0.f;
			m[2][2]=float(SimdSign(m_u[2]));

			t.setOrigin(SimdPoint3(
					-m_o[0],
					-SimdSign(m_u[2])*m_o[1],
					-SimdSign(m_u[2])*m_o[2]
				));
			t.setBasis(m);

		}
}

//gives interpolated transform for time in [0..1] in screwing frame
SimdTransform	BU_Screwing::InBetweenTransform(const SimdTransform& tr,SimdScalar t) const
{
	SimdPoint3 org = tr.getOrigin();

	SimdPoint3 neworg (
	org.x()*cosf(m_w*t)-org.y()*sinf(m_w*t),
	org.x()*sinf(m_w*t)+org.y()*cosf(m_w*t),
	org.z()+m_s*CalculateF(t));
		
	SimdTransform newtr;
	newtr.setOrigin(neworg);
	SimdMatrix3x3 basis = tr.getBasis();
	SimdMatrix3x3 basisorg = tr.getBasis();

	SimdQuaternion rot(SimdVector3(0.,0.,1.),m_w*t);
	SimdQuaternion tmpOrn;
	tr.getBasis().getRotation(tmpOrn);
	rot = rot *  tmpOrn;

	//to avoid numerical drift, normalize quaternion
	rot.normalize();
	newtr.setBasis(SimdMatrix3x3(rot));
	return newtr;

}


SimdScalar BU_Screwing::CalculateF(SimdScalar t) const
{
	SimdScalar result;
	if (!m_w)
	{
		result = t;
	} else
	{
		result = ( tanf((m_w*t)/2.f) / tanf(m_w/2.f));
	}
	return result;
}
	
