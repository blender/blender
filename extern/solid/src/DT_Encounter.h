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

#ifndef DT_ENCOUNTER_H
#define DT_ENCOUNTER_H

#include <set>

#include "MT_Vector3.h"
#include "DT_Object.h"
#include "DT_Shape.h"

class DT_RespTable;

class DT_Encounter {
public:
    DT_Encounter() {}
    DT_Encounter(DT_Object *obj_ptr1, DT_Object *obj_ptr2) 
        : m_sep_axis(MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(0.0)) 
    {
		assert(obj_ptr1 != obj_ptr2);
        if (obj_ptr2->getType() < obj_ptr1->getType() || 
            (obj_ptr2->getType() == obj_ptr1->getType() &&
             obj_ptr2 < obj_ptr1))
        { 
            m_obj_ptr1 = obj_ptr2; 
            m_obj_ptr2 = obj_ptr1; 
        }
        else 
        { 
            m_obj_ptr1 = obj_ptr1; 
            m_obj_ptr2 = obj_ptr2; 
        }
    }

    DT_Object         *first()          const { return m_obj_ptr1; }
    DT_Object         *second()         const { return m_obj_ptr2; }
    const MT_Vector3&  separatingAxis() const { return m_sep_axis; }

 	DT_Bool exactTest(const DT_RespTable *respTable, int& count) const;

private:
    DT_Object          *m_obj_ptr1;
    DT_Object          *m_obj_ptr2;
    mutable MT_Vector3  m_sep_axis;
};

inline bool operator<(const DT_Encounter& a, const DT_Encounter& b) 
{ 
    return a.first() < b.first() || 
        (a.first() == b.first() && a.second() < b.second()); 
}



inline std::ostream& operator<<(std::ostream& os, const DT_Encounter& a) {
    return os << '(' << a.first() << ", " << a.second() << ')';
}



typedef std::set<DT_Encounter> DT_EncounterTable;

#endif
