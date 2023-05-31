/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_WorkPackage.h"

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
