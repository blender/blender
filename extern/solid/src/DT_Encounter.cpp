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

#include "DT_RespTable.h"
#include "DT_Encounter.h"
#include "DT_Object.h"
#include "GEN_MinMax.h"

DT_Bool DT_Encounter::exactTest(const DT_RespTable *respTable, int& count) const 
{
	const DT_ResponseList& responseList = respTable->find(m_obj_ptr1, m_obj_ptr2);

   switch (responseList.getType()) 
   {
   case DT_BROAD_RESPONSE:
	   return (respTable->getResponseClass(m_obj_ptr1) < respTable->getResponseClass(m_obj_ptr2)) ?
			   responseList(m_obj_ptr1->getClientObject(), m_obj_ptr2->getClientObject(), 0) :   
			   responseList(m_obj_ptr2->getClientObject(), m_obj_ptr1->getClientObject(), 0);    
   case DT_SIMPLE_RESPONSE: 
	   if (intersect(*m_obj_ptr1, *m_obj_ptr2, m_sep_axis)) 
	   {
		   ++count;
		   return (respTable->getResponseClass(m_obj_ptr1) < respTable->getResponseClass(m_obj_ptr2)) ?
			   responseList(m_obj_ptr1->getClientObject(), m_obj_ptr2->getClientObject(), 0) :   
			   responseList(m_obj_ptr2->getClientObject(), m_obj_ptr1->getClientObject(), 0);    
 
	   }
	   break;
   case DT_WITNESSED_RESPONSE: {
	   MT_Point3  p1, p2;
	   
	   if (common_point(*m_obj_ptr1, *m_obj_ptr2, m_sep_axis, p1, p2)) 
	   { 
		   ++count;
           if (respTable->getResponseClass(m_obj_ptr1) < respTable->getResponseClass(m_obj_ptr2))
           {
			   DT_CollData coll_data;
			   
			   p1.getValue(coll_data.point1);
			   p2.getValue(coll_data.point2);
			   
               return responseList(m_obj_ptr1->getClientObject(), m_obj_ptr2->getClientObject(), &coll_data);
           }
           else
           {
			   DT_CollData coll_data;
			   
			   p1.getValue(coll_data.point2);
			   p2.getValue(coll_data.point1);
			   
               return responseList(m_obj_ptr2->getClientObject(), m_obj_ptr1->getClientObject(), &coll_data);
           }
	   }
	   break;
   }
   case DT_DEPTH_RESPONSE: {
	   MT_Point3  p1, p2;
	   
	   if (penetration_depth(*m_obj_ptr1, *m_obj_ptr2, m_sep_axis, p1, p2)) 
	   { 
		   ++count;
           if (respTable->getResponseClass(m_obj_ptr1) < respTable->getResponseClass(m_obj_ptr2))
           {
			   DT_CollData coll_data;
			   
			   p1.getValue(coll_data.point1);
			   p2.getValue(coll_data.point2);	
               (p2 - p1).getValue(coll_data.normal);
			   
               return responseList(m_obj_ptr1->getClientObject(), m_obj_ptr2->getClientObject(), &coll_data);
           }
           else
           {
			   DT_CollData coll_data;
			   
			   p1.getValue(coll_data.point2);
			   p2.getValue(coll_data.point1); 
               (p1 - p2).getValue(coll_data.normal);
			   
               return responseList(m_obj_ptr2->getClientObject(), m_obj_ptr1->getClientObject(), &coll_data);
           }
	   }
	   break;
   }
   case DT_NO_RESPONSE:
	   break;
   default:
	   assert(false);
   }
   return DT_CONTINUE;
}
