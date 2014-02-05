/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_CompositorContext.h"
#include "COM_defines.h"
#include <stdio.h>

CompositorContext::CompositorContext()
{
	this->m_scene = NULL;
	this->m_rd = NULL;
	this->m_quality = COM_QUALITY_HIGH;
	this->m_hasActiveOpenCLDevices = false;
	this->m_fastCalculation = false;
	this->m_viewSettings = NULL;
	this->m_displaySettings = NULL;
}

const int CompositorContext::getFramenumber() const
{
	if (this->m_rd) {
		return this->m_rd->cfra;
	}
	else {
		return -1; /* this should never happen */
	}
}
