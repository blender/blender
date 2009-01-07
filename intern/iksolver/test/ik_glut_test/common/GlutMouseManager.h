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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef NAN_INCLUDED_GlutMouseManager_h
#define NAN_INCLUDED_GlutMouseManager_h

#include "MEM_NonCopyable.h"
#include "MEM_SmartPtr.h"

class GlutMouseHandler {
public :

	virtual		
		void
	Mouse(
		int button,
		int state,
		int x,
		int y
	) = 0;

	virtual
		void
	Motion(
		int x,
		int y
	) = 0;

	virtual 
	~GlutMouseHandler(
	){};		
};

class GlutMouseManager : public MEM_NonCopyable{

public :

	static
		GlutMouseManager *
	Instance(
	);

	// these are the functions you should pass to GLUT	

	static
		void
	Mouse(
		int button,
		int state,
		int x,
		int y
	);

	static
		void
	Motion(
		int x,
		int y
	);

		void
	InstallHandler(
		GlutMouseHandler *
	);

		void
	ReleaseHandler(
	);

	~GlutMouseManager(
	);

private :

	GlutMouseManager (
	) :
		m_handler (0)
	{
	};
	
	GlutMouseHandler * m_handler;

	static MEM_SmartPtr<GlutMouseManager> m_s_instance;
};	

#endif

