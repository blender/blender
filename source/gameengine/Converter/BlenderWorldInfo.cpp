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

#include <stdio.h>  // printf()

#include "BlenderWorldInfo.h"
#include "KX_BlenderGL.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* This list includes only data type definitions */
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_group_types.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_property_types.h"
#include "DNA_text_types.h"
#include "DNA_sensor_types.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_screen_types.h"

#include "BKE_global.h"
/* end of blender include block */


BlenderWorldInfo::BlenderWorldInfo(struct World* blenderworld)
{
	if (blenderworld)
	{
		m_hasworld = true;

		// do we have mist?
		if ((blenderworld->mode) & WO_MIST)
		{
			m_hasmist = true;
			m_miststart = blenderworld->miststa;
			m_mistdistance = blenderworld->mistdist;
			m_mistred = blenderworld->horr;
			m_mistgreen = blenderworld->horg;
			m_mistblue = blenderworld->horb;
		}
		else
		{
			m_hasmist = false;
			m_miststart = 0.0;
			m_mistdistance = 0.0;
			m_mistred = 0.0;
			m_mistgreen = 0.0;
			m_mistblue = 0.0;
		}

		m_backgroundred = blenderworld->horr;
		m_backgroundgreen = blenderworld->horg;
		m_backgroundblue = blenderworld->horb;
		
		m_ambientred = blenderworld->ambr;
		m_ambientgreen = blenderworld->ambg;
		m_ambientblue = blenderworld->ambb;
	}
	else
	{
		m_hasworld = false;
	}
}



BlenderWorldInfo::~BlenderWorldInfo()
{

}


bool BlenderWorldInfo::hasWorld()
{
	return m_hasworld;
}



bool BlenderWorldInfo::hasMist()
{
	return m_hasmist;
}



float BlenderWorldInfo::getBackColorRed()
{
	return m_backgroundred;
}



float BlenderWorldInfo::getBackColorGreen()
{
	return m_backgroundgreen;
}  



float BlenderWorldInfo::getBackColorBlue()
{
	return m_backgroundblue;
}  


float BlenderWorldInfo::getAmbientColorRed()
{
	return m_ambientred;
}

float BlenderWorldInfo::getAmbientColorGreen()
{
	return m_ambientgreen;
}

float BlenderWorldInfo::getAmbientColorBlue()
{
	return m_ambientblue;
}

float BlenderWorldInfo::getMistStart()
{
	return m_miststart;
}



float BlenderWorldInfo::getMistDistance()
{
	return m_mistdistance;
}  
    

    
float BlenderWorldInfo::getMistColorRed()
{
	return m_mistred;
}



float BlenderWorldInfo::getMistColorGreen()
{
	return m_mistgreen;
}   



float BlenderWorldInfo::getMistColorBlue()
{
	return m_mistblue;
}


	void	
BlenderWorldInfo::setMistStart(
	float d
) {
	m_miststart = d;
}


	void
BlenderWorldInfo::setMistDistance(
	float d
) {
	m_mistdistance = d;
}


	void	
BlenderWorldInfo::setMistColorRed(
	float d
) {
	m_mistred = d;
}


	void
BlenderWorldInfo::setMistColorGreen(
	float d
) {
	m_mistgreen = d;
}


	void	
BlenderWorldInfo::setMistColorBlue(
	float d
) {
	m_mistblue = d;
}
