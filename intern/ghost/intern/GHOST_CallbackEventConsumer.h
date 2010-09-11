/**
 * $Id$
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
/**
 * @file	GHOST_CallbackEventConsumer.h
 * Declaration of GHOST_CallbackEventConsumer class.
 */

#ifndef _GHOST_CALLBACK_EVENT_CONSUMER_H_
#define _GHOST_CALLBACK_EVENT_CONSUMER_H_

#include "GHOST_IEventConsumer.h"
#include "GHOST_C-api.h"

/**
 * Event consumer that will forward events to a call-back routine.
 * Especially useful for the C-API.
 * @author	Maarten Gribnau
 * @date	October 25, 2001
 */
class GHOST_CallbackEventConsumer : public GHOST_IEventConsumer
{
public:
	/**
	 * Constructor.
	 * @param	eventCallback	The call-back routine invoked.
	 * @param	userData		The data passed back though the call-back routine.
	 */
	GHOST_CallbackEventConsumer(
		GHOST_EventCallbackProcPtr eventCallback, 
		GHOST_TUserDataPtr userData);

	/**
	 * Destructor.
	 */
	virtual ~GHOST_CallbackEventConsumer(void)
	{
	}

	/**
	 * This method is called by an event producer when an event is available.
	 * @param event	The event that can be handled or ignored.
	 * @return Indication as to whether the event was handled.
	 */
	virtual	bool processEvent(GHOST_IEvent* event);

protected:
	/** The call-back routine invoked. */
	GHOST_EventCallbackProcPtr	m_eventCallback;
	/** The data passed back though the call-back routine. */
	GHOST_TUserDataPtr			m_userData;
};

#endif // _GHOST_CALLBACK_EVENT_CONSUMER_H_

