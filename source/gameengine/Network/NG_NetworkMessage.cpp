/*
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
 * generic Network Message implementation
 */

/** \file gameengine/Network/NG_NetworkMessage.cpp
 *  \ingroup bgenet
 */

#include "NG_NetworkMessage.h"
#include <assert.h>

int NG_NetworkMessage::s_nextID = 3; // just some number to start with

NG_NetworkMessage::NG_NetworkMessage(
	const STR_String& to,
	const STR_String& from,
	const STR_String& subject,
	const STR_String& body) :
	m_uniqueMessageID(s_nextID++),
	m_refcount(1),
	m_to(to),
	m_from(from),
	m_subject(subject),
	m_message(body)
{
}

NG_NetworkMessage::~NG_NetworkMessage()
{
	assert(m_refcount==0);
}
