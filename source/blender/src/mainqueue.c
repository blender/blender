/*
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**
 * \file mainqueue.c
 * \brief Just the functions to maintain a central event queue.
 * 
 * Creates the functionality of a FIFO queue for events.
 *
 * \note The documentor (me) doesn't know the full description of 
 * the fields of the QEvent structure, and the parameters to the 
 * functions (event, val, ascii).  The comments should be updated
 * with more detailed descriptions of what is stored in each one.
 *
 * \warning This queue structure uses a method that assumes the presence
 * of allocated memory.  As well it doesn't de-allocate memory after
 * a read off of the queue, thereby causing a situation whereby memory
 * isn't being freed and can grow with out bound even though there is
 * a limit on the queue size.
 */

#include <stdlib.h>
#include <string.h>
#include "BIF_mainqueue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 *	\struct QEvent
 *	\brief  This is the definition for the event queue datastructure
 */
typedef struct {
	/**
	 * \var unsigned short event
	 * \brief holds the event type
	 */
	unsigned short event;
	/**
	 * \var short val
	 */
	short val;
	/**
	* \var char ascii
	*/
	char ascii;
} QEvent;

/**
 * \var static QEvent mainqueue[MAXQUEUE]
 * \brief The Main Queue store.
 */
static QEvent mainqueue[MAXQUEUE];

/**
 * \var static unsigned int nevents=0
 * \brief The count of the events currently stored.
 */
static unsigned int nevents= 0;

/**
 * \brief Reads and removes events from the queue and returns the event type,
 * if the queue is empty return 0.
 * \param val the val of the event to read into.
 * \param ascii the buffer of the event to read into.
 * \return the event type or 0 if no event.
 *
 * Pops off the last item in the queue and returns the pieces in their
 * little parts. The last item was the oldest item in the queue.
 * 
 */
unsigned short mainqread(short *val, char *ascii)
{
	if (nevents) {
		nevents--;
		
		*val= mainqueue[nevents].val;
		*ascii= mainqueue[nevents].ascii;
		return mainqueue[nevents].event;
	} else
		return 0;
}

/**
 * \brief A short cut to mainqenter_ext setting ascii to 0
 */
void mainqenter(unsigned short event, short val)
{
	mainqenter_ext(event, val, 0);
}

/**
 * \brief Adds event to the beginning of the queue.
 * \param event the event type.
 * \param val the val of the event.
 * \param ascii the event characters.
 *
 * If the event isn't nothing, and if the queue still
 * has some room, then add to the queue.  Otherwise the
 * event is lost.
 */
void mainqenter_ext(unsigned short event, short val, char ascii)
{
	if (!event)
		return;

	if (nevents<MAXQUEUE) {
		memmove(mainqueue+1, mainqueue, sizeof(*mainqueue)*nevents);	
		mainqueue[0].event= event;
		mainqueue[0].val= val;
		mainqueue[0].ascii= ascii;
		
		nevents++;
	}
}

/**
 * \brief Pushes and event back on to the front of the queue.
 * \param event
 * \param val
 * \param ascii
 *
 * Pushes an event back onto the queue, possibly after a peek
 * at the item.  This method assumes that the memory has already
 * been allocated and should be mentioned in a precondition.
 *
 * \pre This method assumes that the memory is already allocated
 * for the event.
 */
void mainqpushback(unsigned short event, short val, char ascii)
{
	if (nevents<MAXQUEUE) {
		mainqueue[nevents].event= event;
		mainqueue[nevents].val= val;
		mainqueue[nevents].ascii= ascii;
		nevents++;
	}
}

/**
 * \brief Returns the event type from the last item in the queue
 * (the next one that would be popped off).  Probably used as a test
 * to see if the queue is empty or if a valid event is still around.
 * \return the event type of the last item in the queue
 */
unsigned short mainqtest()
{
	if (nevents)
		return mainqueue[nevents-1].event;
	else
		return 0;
}
