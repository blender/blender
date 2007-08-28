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

#ifndef DT_RESPONSE_H
#define DT_RESPONSE_H

#include "SOLID.h"

class DT_Response {
public:
    DT_Response(DT_ResponseCallback response    = 0, 
				DT_ResponseType     type        = DT_NO_RESPONSE, 
				void               *client_data = 0) 
	  : m_response(response), 
		m_type(type), 
		m_client_data(client_data) {}
    
	DT_ResponseType getType() const { return m_type; }

	DT_Bool operator()(void *a, void *b, const DT_CollData *coll_data) const 
	{  
		return (*m_response)(m_client_data, a, b, coll_data); 
	}

	friend bool operator==(const DT_Response& a, const DT_Response& b) 
	{
		return a.m_response == b.m_response;
	}
    
	friend bool operator!=(const DT_Response& a, const DT_Response& b) 
	{
		return a.m_response != b.m_response;
	}
    
private:
    DT_ResponseCallback  m_response;
    DT_ResponseType      m_type;
    void                *m_client_data;
};

#endif  


