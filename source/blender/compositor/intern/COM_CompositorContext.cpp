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

CompositorContext::CompositorContext() {
	this->scene = NULL;
	this->quality = COM_QUALITY_HIGH;
	this->hasActiveOpenCLDevices = false;
}

const int CompositorContext::getFramenumber() const {
	if (this->scene) {
		return this->scene->r.cfra;
	} else {
		return -1; /* this should never happen */
	}
}

const int CompositorContext::isColorManaged() const {
	if (this->scene) {
		return this->scene->r.color_mgt_flag & R_COLOR_MANAGEMENT;
	} else {
		return 0; /* this should never happen */
	}
}
