/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * NetworkGame_NetworkMessage generic Network Message class
 */
#ifndef NG_NETWORKMESSAGE_H
#define NG_NETWORKMESSAGE_H

#include "STR_HashedString.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class NG_NetworkMessage
{
	static int			s_nextID;
	int					m_uniqueMessageID;	// intern counting MessageID
	unsigned int		m_ClientId;
	int					m_refcount;

	STR_String			m_to;			// receiver
	STR_String			m_from;			// sender
	STR_String			m_subject;		// empty or propName
	STR_String			m_message;		// message or propValue

protected:
	~NG_NetworkMessage();

public:
	NG_NetworkMessage(
		const STR_String& to,
		const STR_String& from,
		const STR_String& subject,
		const STR_String& body);

	void AddRef() {
		m_refcount++;
	}

	// This is not nice code you should'nt need to resort to
	// delete this.
	void Release()
	{
		if (! --m_refcount)
		{
 			delete this;
		}
	}

	/**
	 * set the content of this message
	 */
	void SetMessageText(const STR_String& msgtext) {
		m_message = msgtext;
	}

	/**
	 * get the (read-only) To part of this message
	 */
	const STR_String& GetDestinationName() { return m_to;};

	/**
	 * get the (read-only) From part of this message
	 */
	const STR_String& GetSenderName() { return m_from;};

	/**
	 * get the (read-only) Subject part of this message
	 */
	const STR_String& GetSubject() { return m_subject;};

	/**
	 * get the (read-only) Body part of this message
	 */
	const STR_String& GetMessageText() {
//cout << "GetMessageText " << m_message << "\n";
		return m_message;
	}
	const STR_String& GetMessageText() const {
//cout << "GetMessageText " << m_message << "\n";
		return m_message;
	}

	/**
	 * Set the NetworkMessage sender identifier
	 */
	void SetSender(unsigned int ClientId) {
		m_ClientId = ClientId;
	}

	/**
	 * Get the NetworkMessage sender identifier
	 */
	unsigned int GetSender(void) {
		return m_ClientId;
	}

	/**
	  * get the unique Network Message ID
	  */
	int GetMessageID() {
		return m_uniqueMessageID;
	}
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:NG_NetworkMessage"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //NG_NETWORKMESSAGE_H

