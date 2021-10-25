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

/** \file ghost/intern/GHOST_CallbackEventConsumer.h
 *  \ingroup GHOST
 * Declaration of GHOST_CallbackEventConsumer class.
 */

#ifndef __GHOST_CALLBACKEVENTCONSUMER_H__
#define __GHOST_CALLBACKEVENTCONSUMER_H__

#include "GHOST_IEventConsumer.h"
#include "GHOST_C-api.h"

/**
 * Event consumer that will forward events to a call-back routine.
 * Especially useful for the C-API.
 * \author	Maarten Gribnau
 * \date	October 25, 2001
 */
class GHOST_CallbackEventConsumer : public GHOST_IEventConsumer
{
public:
	/**
	 * Constructor.
	 * \param	eventCallback	The call-back routine invoked.
	 * \param	userData		The data passed back though the call-back routine.
	 */
	GHOST_CallbackEventConsumer(
	    GHOST_EventCallbackProcPtr eventCallback,
	    GHOST_TUserDataPtr userData);

	/**
	 * Destructor.
	 */
	~GHOST_CallbackEventConsumer(void)
	{
	}

	/**
	 * This method is called by an event producer when an event is available.
	 * \param event	The event that can be handled or ignored.
	 * \return Indication as to whether the event was handled.
	 */
	bool processEvent(GHOST_IEvent *event);

protected:
	/** The call-back routine invoked. */
	GHOST_EventCallbackProcPtr m_eventCallback;
	/** The data passed back though the call-back routine. */
	GHOST_TUserDataPtr m_userData;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_CallbackEventConsumer")
#endif
};

#endif // __GHOST_CALLBACKEVENTCONSUMER_H__

