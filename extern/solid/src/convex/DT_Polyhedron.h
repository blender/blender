/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef DT_POLYHEDRON_H
#define DT_POLYHEDRON_H

#ifdef HAVE_CONFIG_H
# include "config.h"
# if HAVE_QHULL_QHULL_A_H
#  define QHULL
# endif
#endif


#ifdef QHULL

#include "DT_Convex.h"
#include "DT_IndexArray.h"
#include "DT_VertexBase.h"

class DT_Polyhedron : public DT_Convex {
	typedef DT_Array<DT_IndexArray> T_MultiIndexArray;
public:
	DT_Polyhedron() 
		: m_verts(0),
		  m_cobound(0)
	{}
		
	DT_Polyhedron(const DT_VertexBase *base, DT_Count count, const DT_Index *indices);

	virtual ~DT_Polyhedron();
    
    virtual MT_Scalar supportH(const MT_Vector3& v) const;
    virtual MT_Point3 support(const MT_Vector3& v) const;

	const MT_Point3& operator[](int i) const { return m_verts[i]; }
    DT_Count numVerts() const { return m_count; }

private:
	DT_Count              m_count;
	MT_Point3			 *m_verts;
	T_MultiIndexArray    *m_cobound;
    DT_Index              m_start_vertex;
	mutable DT_Index      m_curr_vertex;
};

#else 

#include "DT_Polytope.h"

typedef DT_Polytope DT_Polyhedron;

#endif

#endif

