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

/** \file BlenderWorldInfo.h
 *  \ingroup bgeconv
 */

#ifndef __BLENDERWORLDINFO_H__
#define __BLENDERWORLDINFO_H__
#include "MT_CmMatrix4x4.h"
#include "KX_WorldInfo.h"
#include "KX_BlenderGL.h"

class BlenderWorldInfo : public KX_WorldInfo
{
	bool			m_hasworld;
	float			m_backgroundcolor[3];

	bool			m_hasmist;
	float			m_miststart;
	float			m_mistdistance;
	float			m_mistcolor[3];

	float			m_ambientcolor[3];

public:
	BlenderWorldInfo(struct Scene *blenderscene, struct World* blenderworld);
	~BlenderWorldInfo();

	bool	hasWorld();
	bool	hasMist();
	float	getBackColorRed();
	float	getBackColorGreen();
	float	getBackColorBlue();
	
	float	getAmbientColorRed();
	float	getAmbientColorGreen();
	float	getAmbientColorBlue();

	float	getMistStart();
	float	getMistDistance();
	float	getMistColorRed();
	float	getMistColorGreen();
	float	getMistColorBlue();

	void
	setBackColor(
		float r,
		float g,
		float b
	);
		void	
	setMistStart(
		float d
	);

		void	
	setMistDistance(
		float d
	);

		void	
	setMistColorRed(
		float d
	);

		void	
	setMistColorGreen(
		float d
	);

		void	
	setMistColorBlue(
		float d
	);   
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:BlenderWorldInfo"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__BLENDERWORLDINFO_H__

