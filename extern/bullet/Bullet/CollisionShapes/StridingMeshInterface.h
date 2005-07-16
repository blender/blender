/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#ifndef STRIDING_MESHINTERFACE_H
#define STRIDING_MESHINTERFACE_H

#include "SimdVector3.h"

/// PHY_ScalarType enumerates possible scalar types.
/// See the StridingMeshInterface for its use
typedef enum PHY_ScalarType {
	PHY_FLOAT,
	PHY_DOUBLE,
	PHY_INTEGER,
	PHY_SHORT,
	PHY_FIXEDPOINT88
} PHY_ScalarType;

///	StridingMeshInterface is the interface class for high performance access to triangle meshes
/// It allows for sharing graphics and collision meshes. Also it provides locking/unlocking of graphics meshes that are in gpu memory.
class  StridingMeshInterface
{
	protected:
	
		SimdVector3 m_scaling;

	public:
		StridingMeshInterface() :m_scaling(1.f,1.f,1.f)
		{
			
		}

		virtual ~StridingMeshInterface();
		/// get read and write access to a subpart of a triangle mesh
		/// this subpart has a continuous array of vertices and indices
		/// in this way the mesh can be handled as chunks of memory with striding
		/// very similar to OpenGL vertexarray support
		/// make a call to unLockVertexBase when the read and write access is finished	
	virtual void	getLockedVertexIndexBase(unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& stride,unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0)=0;
		

		/// unLockVertexBase finishes the access to a subpart of the triangle mesh
		/// make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
		virtual void	unLockVertexBase(int subpart)=0;

		/// getNumSubParts returns the number of seperate subparts
		/// each subpart has a continuous array of vertices and indices
		virtual int		getNumSubParts()=0;

		virtual void	preallocateVertices(int numverts)=0;
		virtual void	preallocateIndices(int numindices)=0;

		const SimdVector3&	getScaling() {
			return m_scaling;
		}
		void	setScaling(const SimdVector3& scaling)
		{
			m_scaling = scaling;
		}
};

#endif //STRIDING_MESHINTERFACE_H
