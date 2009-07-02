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

#ifndef DT_LINESEGMENT_H
#define DT_LINESEGMENT_H

#include "DT_Convex.h"

class DT_LineSegment : public DT_Convex {
public:
    DT_LineSegment(const MT_Point3& source, const MT_Point3& target) : 
	   m_source(source), 
	   m_target(target) {}

    virtual MT_Scalar supportH(const MT_Vector3& v) const;
    virtual MT_Point3 support(const MT_Vector3& v) const;

	const MT_Point3& getSource() const { return m_source; }
	const MT_Point3& getTarget() const { return m_target; }

private:
	MT_Point3 m_source;
	MT_Point3 m_target;
};

#endif






