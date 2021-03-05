/*
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
 * Copyright 2011, Blender Foundation.
 */

#pragma once

class ExecutionGroup;
#include "COM_ExecutionGroup.h"

/**
 * \brief contains data about work that can be scheduled
 * \see WorkScheduler
 */
struct WorkPackage {
  /**
   * \brief executionGroup with the operations-setup to be evaluated
   */
  ExecutionGroup *execution_group;

  /**
   * \brief number of the chunk to be executed
   */
  unsigned int chunk_number;

  /**
   * constructor
   * \param group: the ExecutionGroup
   * \param chunk_number: the number of the chunk
   */
  WorkPackage(ExecutionGroup *group, unsigned int chunk_number);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:WorkPackage")
#endif
};
