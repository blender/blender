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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#ifndef __USD_EXPORTER_CONTEXT_H__
#define __USD_EXPORTER_CONTEXT_H__

#include "usd.h"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/common.h>

struct Depsgraph;
struct Object;

namespace blender {
namespace io {
namespace usd {

class USDHierarchyIterator;

struct USDExporterContext {
  Depsgraph *depsgraph;
  const pxr::UsdStageRefPtr stage;
  const pxr::SdfPath usd_path;
  const USDHierarchyIterator *hierarchy_iterator;
  const USDExportParams &export_params;
};

}  // namespace usd
}  // namespace io
}  // namespace blender

#endif /* __USD_EXPORTER_CONTEXT_H__ */
