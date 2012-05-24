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

#include "COM_QualityStepHelper.h"

QualityStepHelper::QualityStepHelper()
{
	this->quality = COM_QUALITY_HIGH;
	this->step = 1;
	this->offsetadd = 4;
}

void QualityStepHelper::initExecution(QualityHelper helper)
{
	switch (helper) {
	case COM_QH_INCREASE:
		switch (this->quality) {
		case COM_QUALITY_HIGH:
		default:
			this->step = 1;
			this->offsetadd = 4;
			break;
		case COM_QUALITY_MEDIUM:
			this->step = 2;
			this->offsetadd = 8;
			break;
		case COM_QUALITY_LOW:
			this->step = 3;
			this->offsetadd = 12;
			break;
		}
		break;
	case COM_QH_MULTIPLY:
		switch (this->quality) {
		case COM_QUALITY_HIGH:
		default:
			this->step = 1;
			this->offsetadd = 4;
			break;
		case COM_QUALITY_MEDIUM:
			this->step = 2;
			this->offsetadd = 8;
			break;
		case COM_QUALITY_LOW:
			this->step = 4;
			this->offsetadd = 16;
			break;
		}
		break;
	}
}

