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

#include "COM_WorkPackage.h"

#include "COM_Enums.h"
#include "COM_ExecutionGroup.h"

namespace blender::compositor {

std::ostream &operator<<(std::ostream &os, const WorkPackage &work_package)
{
  os << "WorkPackage(execution_group=" << *work_package.execution_group;
  os << ",chunk=" << work_package.chunk_number;
  os << ",state=" << work_package.state;
  os << ",rect=(" << work_package.rect.xmin << "," << work_package.rect.ymin << ")-("
     << work_package.rect.xmax << "," << work_package.rect.ymax << ")";
  os << ")";
  return os;
}

}  // namespace blender::compositor
