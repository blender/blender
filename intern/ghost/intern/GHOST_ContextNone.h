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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_ContextNone.h
 *  \ingroup GHOST
 *
 * Declaration of GHOST_Context class.
 */

#ifndef __GHOST_CONTEXTNONE_H__
#define __GHOST_CONTEXTNONE_H__

#include "GHOST_Context.h"

class GHOST_ContextNone : public GHOST_Context
{
public:

	GHOST_ContextNone(
	        bool stereoVisual,
	        GHOST_TUns16 numOfAASamples)
	    : GHOST_Context(stereoVisual, numOfAASamples),
	      m_swapInterval(1)
	{}

	/**
	 * Dummy function
	 * \return  Always succeeds
	 */
	virtual GHOST_TSuccess swapBuffers();

	/**
	 * Dummy function
	 * \return  Always succeeds
	 */
	virtual GHOST_TSuccess activateDrawingContext();

	/**
	 * Dummy function
	 * \return Always succeeds
	 */
	virtual GHOST_TSuccess updateDrawingContext();

	/**
	 * Dummy function
	 * \return Always succeeds
	 */
	virtual GHOST_TSuccess initializeDrawingContext();

	/**
	 * Dummy function
	 * \return Always succeeds
	 */
	virtual GHOST_TSuccess releaseNativeHandles();

	/**
	 * Dummy function
	 * \return Always succeeds
	 */
	virtual GHOST_TSuccess setSwapInterval(int interval);

	/**
	 * Dummy function
	 * \param intervalOut Gets whatever was set by setSwapInterval
	 * \return Always succeeds
	 */
	virtual GHOST_TSuccess getSwapInterval(int &intervalOut);

private:
	int m_swapInterval;
};

#endif // __GHOST_CONTEXTNONE_H__
