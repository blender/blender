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

#include <assert.h>

DT_ResponseList DT_RespTable::g_emptyResponseList;

DT_RespTable::~DT_RespTable()
{
	DT_ResponseClass i;
	for (i = 0; i < m_responseClass; ++i) 
	{
		delete [] m_table[i];
	}
}

DT_ResponseClass DT_RespTable::genResponseClass()
{
	DT_ResponseClass newClass = m_responseClass++;
	DT_ResponseList *newList = new DT_ResponseList[m_responseClass];
	assert(newList);
	m_table.push_back(newList);
	m_singleList.resize(m_responseClass);
	DT_ResponseClass i;
	for (i = 0; i < m_responseClass; ++i) 
	{
		newList[i].append(m_default);
		newList[i].append(m_singleList[i]);
	}
	return newClass;
}

void DT_RespTable::setResponseClass(void *object, 
									DT_ResponseClass responseClass) 
{
	assert(responseClass < m_responseClass);
	m_objectMap[object] = responseClass;
}

DT_ResponseClass DT_RespTable::getResponseClass(void *object) const
{
	T_ObjectMap::const_iterator it = m_objectMap.find(object);
	assert(it != m_objectMap.end());
	return (*it).second;
}

void DT_RespTable::clearResponseClass(void *object) 
{
	m_objectMap.erase(object);
}

const DT_ResponseList& DT_RespTable::find(void *object1, void *object2) const
{
	T_ObjectMap::const_iterator it = m_objectMap.find(object1);
	if (it != m_objectMap.end()) 
	{
		DT_ResponseClass responseClass1 = (*it).second;
		it = m_objectMap.find(object2);
		if (it != m_objectMap.end()) 
		{
			DT_ResponseClass responseClass2 = (*it).second;
			if (responseClass1 < responseClass2) 
			{
				std::swap(responseClass1, responseClass2);
			}
			return m_table[responseClass1][responseClass2];
		}
	}
	return g_emptyResponseList;
}

void DT_RespTable::addDefault(const DT_Response& response)
{
	m_default.addResponse(response);
	DT_ResponseClass i;
	for (i = 0; i < m_responseClass; ++i) 
	{
		DT_ResponseClass j;
		for (j = 0; j <= i; ++j) 
		{
			m_table[i][j].addResponse(response);
		}
	}
}

void DT_RespTable::removeDefault(const DT_Response& response)
{
	m_default.removeResponse(response);
	DT_ResponseClass i;
	for (i = 0; i < m_responseClass; ++i) 
	{
		DT_ResponseClass j;
		for (j = 0; j <= i; ++j) 
		{
			m_table[i][j].removeResponse(response);
		}
	}
}

void DT_RespTable::addSingle(DT_ResponseClass responseClass, 
							 const DT_Response& response)
{	
	assert(responseClass < m_responseClass);
	m_singleList[responseClass].addResponse(response);
	DT_ResponseClass j;
	for (j = 0; j < responseClass; ++j) 
	{
		m_table[responseClass][j].addResponse(response);
	}
	
	DT_ResponseClass i;
	for (i = responseClass; i < m_responseClass; ++i) 
	{
		m_table[i][responseClass].addResponse(response);
	}
}

void DT_RespTable::removeSingle(DT_ResponseClass responseClass, 
								const DT_Response& response)
{	
	assert(responseClass < m_responseClass);
	m_singleList[responseClass].removeResponse(response);
	DT_ResponseClass j;
	for (j = 0; j < responseClass; ++j) 
	{
		m_table[responseClass][j].removeResponse(response);
	}
	
	DT_ResponseClass i;
	for (i = responseClass; i < m_responseClass; ++i) 
	{
		m_table[i][responseClass].removeResponse(response);
	}
}

void DT_RespTable::addPair(DT_ResponseClass responseClass1,
						   DT_ResponseClass responseClass2, 
						   const DT_Response& response)
{
	assert(responseClass1 < m_responseClass);
	assert(responseClass2 < m_responseClass);
	if (responseClass1 < responseClass2) 
	{
		std::swap(responseClass1, responseClass2);
	}
	m_table[responseClass1][responseClass2].addResponse(response);
}


void DT_RespTable::removePair(DT_ResponseClass responseClass1,
							  DT_ResponseClass responseClass2, 
							  const DT_Response& response)
{
	assert(responseClass1 < m_responseClass);
	assert(responseClass2 < m_responseClass);
	if (responseClass1 < responseClass2) 
	{
		std::swap(responseClass1, responseClass2);
	}
	m_table[responseClass1][responseClass2].removeResponse(response);
}

