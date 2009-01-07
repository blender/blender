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

#ifndef DT_HULL_H
#define DT_HULL_H

#include "DT_Convex.h"

class DT_Hull : public DT_Convex {
public:
	DT_Hull(const DT_Convex& lchild, const DT_Convex& rchild) :
		m_lchild(lchild), 
		m_rchild(rchild) 
	{}

	virtual MT_Scalar supportH(const MT_Vector3& v) const 
	{
		return GEN_max(m_lchild.supportH(v), m_rchild.supportH(v));
	}

	virtual MT_Point3 support(const MT_Vector3& v) const 
	{
		MT_Point3 lpnt = m_lchild.support(v);
		MT_Point3 rpnt = m_rchild.support(v);
		return v.dot(lpnt) > v.dot(rpnt) ? lpnt : rpnt;
	}

private:
	const DT_Convex& m_lchild;
	const DT_Convex& m_rchild;
};

#endif
