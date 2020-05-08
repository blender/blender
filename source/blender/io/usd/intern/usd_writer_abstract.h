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
#ifndef __USD_WRITER_ABSTRACT_H__
#define __USD_WRITER_ABSTRACT_H__

#include "IO_abstract_hierarchy_iterator.h"
#include "usd_exporter_context.h"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdUtils/sparseValueWriter.h>

#include <vector>

#include "DEG_depsgraph_query.h"

#include "DNA_material_types.h"

struct Material;
struct Object;

namespace USD {

using blender::io::AbstractHierarchyWriter;
using blender::io::HierarchyContext;

class USDAbstractWriter : public AbstractHierarchyWriter {
 protected:
  const USDExporterContext usd_export_context_;
  pxr::UsdUtilsSparseValueWriter usd_value_writer_;

  bool frame_has_been_written_;
  bool is_animated_;

 public:
  USDAbstractWriter(const USDExporterContext &usd_export_context);
  virtual ~USDAbstractWriter();

  virtual void write(HierarchyContext &context) override;

  /* Returns true if the data to be written is actually supported. This would, for example, allow a
   * hypothetical camera writer accept a perspective camera but reject an orthogonal one.
   *
   * Returning false from a transform writer will prevent the object and all its descendants from
   * being exported. Returning false from a data writer (object data, hair, or particles) will
   * only prevent that data from being written (and thus cause the object to be exported as an
   * Empty). */
  virtual bool is_supported(const HierarchyContext *context) const;

  const pxr::SdfPath &usd_path() const;

 protected:
  virtual void do_write(HierarchyContext &context) = 0;
  pxr::UsdTimeCode get_export_time_code() const;

  pxr::UsdShadeMaterial ensure_usd_material(Material *material);
};

}  // namespace USD

#endif /* __USD_WRITER_ABSTRACT_H__ */
