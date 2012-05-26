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
 */

/** \file ghost/GHOST_IEventConsumer.h
 *  \ingroup GHOST
 * Declaration of GHOST_IEventConsumer interface class.
 */

#ifndef __GHOST_IEVENTCONSUMER_H__
#define __GHOST_IEVENTCONSUMER_H__

#include "GHOST_IEvent.h"

/**
 * Interface class for objects interested in receiving events.
 * Objects interested in events should inherit this class and implement the
 * processEvent() method. They should then be registered with the system that
 * they want to receive events. The system will call the processEvent() method
 * for every installed event consumer to pass events.
 * @see GHOST_ISystem#addEventConsumer
 * @author	Maarten Gribnau
 * @date	May 14, 2001
 */
class GHOST_IEventConsumer
{
public:
	/**
	 * Destructor.
	 */
	virtual ~GHOST_IEventConsumer()
	{
	}

	/**
	 * This method is called by the system when it has events to dispatch.
	 * @see GHOST_ISystem#dispatchEvents
	 * @param	event	The event that can be handled or ignored.
	 * @return	Indication as to whether the event was handled.
	 */
	virtual bool processEvent(GHOST_IEvent *event) = 0;
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GHOST:GHOST_IEventConsumer"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // _GHOST_EVENT_CONSUMER_H_

