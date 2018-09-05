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

class WorkPackage;

#ifndef __COM_WORKPACKAGE_H__
#define __COM_WORKPACKAGE_H__
class ExecutionGroup;
#include "COM_ExecutionGroup.h"

/**
 * \brief contains data about work that can be scheduled
 * \see WorkScheduler
 */
class WorkPackage {
private:
	/**
	 * \brief executionGroup with the operations-setup to be evaluated
	 */
	ExecutionGroup *m_executionGroup;

	/**
	 * \brief number of the chunk to be executed
	 */
	unsigned int m_chunkNumber;
public:
	/**
	 * constructor
	 * \param group the ExecutionGroup
	 * \param chunkNumber the number of the chunk
	 */
	WorkPackage(ExecutionGroup *group, unsigned int chunkNumber);

	/**
	 * \brief get the ExecutionGroup
	 */
	ExecutionGroup *getExecutionGroup() const { return this->m_executionGroup; }

	/**
	 * \brief get the number of the chunk
	 */
	unsigned int getChunkNumber() const { return this->m_chunkNumber; }

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:WorkPackage")
#endif
};

#endif
