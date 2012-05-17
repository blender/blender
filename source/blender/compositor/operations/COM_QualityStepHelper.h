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

#ifndef _COM_QualityStepHelper_h
#define _COM_QualityStepHelper_h
#include "COM_defines.h"

typedef enum QualityHelper {
	COM_QH_INCREASE,
	COM_QH_MULTIPLY
} QualityHelper;

class QualityStepHelper  {
private:
	CompositorQuality quality;
	int step;
	int offsetadd;

protected:
	/**
	   * Initialize the execution
	   */
	 void initExecution(QualityHelper helper);

	 inline int getStep() const {return this->step;}
	 inline int getOffsetAdd() const {return this->offsetadd;}

public:
	QualityStepHelper();


	void setQuality(CompositorQuality quality) {this->quality = quality;}
};
#endif
