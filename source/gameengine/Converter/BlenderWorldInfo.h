/**
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
#ifndef __BLENDERWORLDINFO_H
#define __BLENDERWORLDINFO_H
#include "MT_CmMatrix4x4.h"
#include "KX_WorldInfo.h"
#include "KX_BlenderGL.h"

class BlenderWorldInfo : public KX_WorldInfo
{
	bool			m_hasworld;
	float			m_backgroundred;
	float			m_backgroundgreen;
	float			m_backgroundblue;

	bool			m_hasmist;
	float			m_miststart;
	float			m_mistdistance;
	float			m_mistred;
	float			m_mistgreen;
	float			m_mistblue;
	
public:
	BlenderWorldInfo(struct World* blenderworld);
	~BlenderWorldInfo();

	bool	hasWorld();
    bool	hasMist();
    float	getBackColorRed();
    float	getBackColorGreen();
    float	getBackColorBlue();

    float	getMistStart();
    float	getMistDistance();
    float	getMistColorRed();
    float	getMistColorGreen();
    float	getMistColorBlue();     

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
};

#endif //__BLENDERWORLDINFO_H

