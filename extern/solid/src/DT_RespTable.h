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

#ifndef DT_RESPTABLE_H
#define DT_RESPTABLE_H

#include <algorithm>
#include <vector>
#include <list>
#include <map>
#include "GEN_MinMax.h"
#include "DT_Response.h"

class DT_ResponseList : public std::list<DT_Response> {
public:
    DT_ResponseList() : m_type(DT_NO_RESPONSE) {}

	DT_ResponseType getType() const { return m_type; }

    void addResponse(const DT_Response& response) 
	{
        if (response.getType() != DT_NO_RESPONSE) 
		{
            push_back(response);
            GEN_set_max(m_type, response.getType());
        }
    }

    void removeResponse(const DT_Response& response) 
	{
		iterator it = std::find(begin(), end(), response);
		if (it != end()) 
		{
			erase(it);
			m_type = DT_NO_RESPONSE;
			for (it = begin(); it != end(); ++it) 
			{
				GEN_set_max(m_type, (*it).getType());
			}
		}
    }
	
    void append(const DT_ResponseList& responseList) 
	{
        if (responseList.getType() != DT_NO_RESPONSE) 
		{
			const_iterator it;
			for (it = responseList.begin(); it != responseList.end(); ++it) 
			{
				addResponse(*it);
			}
		}
	}

    DT_Bool operator()(void *a, void *b, const DT_CollData *coll_data) const 
	{
		DT_Bool done = DT_CONTINUE;
		const_iterator it;
        for (it = begin(); !done && it != end(); ++it) 
		{
            done = (*it)(a, b, coll_data);
        }
		return done;
    }
    
private:
	DT_ResponseType    m_type;
};

class DT_RespTable {
private:
	typedef std::map<void *, DT_ResponseClass> T_ObjectMap; 
	typedef std::vector<DT_ResponseList *> T_PairTable;
	typedef std::vector<DT_ResponseList> T_SingleList;

public:
	DT_RespTable() : m_responseClass(0) {}

	~DT_RespTable();

	DT_ResponseClass genResponseClass();
	
	void setResponseClass(void *object, DT_ResponseClass responseClass);
	DT_ResponseClass getResponseClass(void *object) const;
	
	void clearResponseClass(void *object);
	
	const DT_ResponseList& find(void *object1, void *object2) const;

    void addDefault(const DT_Response& response); 
    void removeDefault(const DT_Response& response); 

    void addSingle(DT_ResponseClass responseClass, 
				   const DT_Response& response);
    void removeSingle(DT_ResponseClass responseClass, 
					  const DT_Response& response);
	
    void addPair(DT_ResponseClass responseClass1, 
				 DT_ResponseClass responseClass2, 
				 const DT_Response& response);
    void removePair(DT_ResponseClass responseClass1, 
					DT_ResponseClass responseClass2, 
					const DT_Response& response);

private:
	static DT_ResponseList g_emptyResponseList;

	T_ObjectMap      m_objectMap;
	DT_ResponseClass m_responseClass;
	T_PairTable      m_table;
	T_SingleList     m_singleList;
    DT_ResponseList  m_default;
};

#endif




