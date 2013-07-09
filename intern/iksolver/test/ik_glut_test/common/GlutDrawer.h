/**
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

#ifndef __GLUTDRAWER_H__
#define __GLUTDRAWER_H__

#include "MEM_NonCopyable.h"
#include "MEM_SmartPtr.h"

// So pissed off with Glut callback stuff
// that is impossible to call objects unless they are global

// inherit from GlutDrawer and installl the drawer in the singleton
// class GlutDrawManager.

class GlutDrawer {
public :

	virtual 
		void
	Draw(
	)= 0;

	virtual 
	~GlutDrawer(
	){};		
};

class GlutDrawManager : public MEM_NonCopyable{

public :

	static
		GlutDrawManager *
	Instance(
	);

	// this is the function you should pass to glut

	static
		void
	Draw(
	);

		void
	InstallDrawer(
		GlutDrawer *
	);

		void
	ReleaseDrawer(
	);

	~GlutDrawManager(
	);

private :

	GlutDrawManager (
	) :
		m_drawer (0)
	{
	};
	
	GlutDrawer * m_drawer;

	static MEM_SmartPtr<GlutDrawManager> m_s_instance;
};	

#endif

