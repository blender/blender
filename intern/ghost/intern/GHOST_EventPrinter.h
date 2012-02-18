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

/** \file ghost/intern/GHOST_EventPrinter.h
 *  \ingroup GHOST
 * Declaration of GHOST_EventPrinter class.
 */

#ifndef __GHOST_EVENTPRINTER_H__
#define __GHOST_EVENTPRINTER_H__

#include "GHOST_IEventConsumer.h"

#include "STR_String.h"

/**
 * An Event consumer that prints all the events to standard out.
 * Really useful when debugging.
 */
class GHOST_EventPrinter : public GHOST_IEventConsumer
{
public:
	/**
	 * Prints all the events received to std out.
	 * @param event	The event that can be handled or not.
	 * @return Indication as to whether the event was handled.
	 */
	virtual	bool processEvent(GHOST_IEvent* event);

protected:
	/**
	 * Converts GHOST key code to a readable string.
	 * @param key The GHOST key code to convert.
	 * @param str The GHOST key code converted to a readable string.
	 */
	void	getKeyString(GHOST_TKey key, char str[32]) const;
};

#endif // __GHOST_EVENTPRINTER_H__

