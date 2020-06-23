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
#ifndef __USD_HIERARCHY_ITERATOR_H__
#define __USD_HIERARCHY_ITERATOR_H__

#include "IO_abstract_hierarchy_iterator.h"
#include "usd.h"
#include "usd_exporter_context.h"

#include <string>

#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/timeCode.h>

struct Depsgraph;
struct ID;
struct Object;

namespace blender {
namespace io {
namespace usd {

using blender::io::AbstractHierarchyIterator;
using blender::io::AbstractHierarchyWriter;
using blender::io::HierarchyContext;

class USDHierarchyIterator : public AbstractHierarchyIterator {
 private:
  const pxr::UsdStageRefPtr stage_;
  pxr::UsdTimeCode export_time_;
  const USDExportParams &params_;

 public:
  USDHierarchyIterator(Depsgraph *depsgraph,
                       pxr::UsdStageRefPtr stage,
                       const USDExportParams &params);

  void set_export_frame(float frame_nr);
  const pxr::UsdTimeCode &get_export_time_code() const;

  virtual std::string make_valid_name(const std::string &name) const override;

 protected:
  virtual bool mark_as_weak_export(const Object *object) const override;

  virtual AbstractHierarchyWriter *create_transform_writer(
      const HierarchyContext *context) override;
  virtual AbstractHierarchyWriter *create_data_writer(const HierarchyContext *context) override;
  virtual AbstractHierarchyWriter *create_hair_writer(const HierarchyContext *context) override;
  virtual AbstractHierarchyWriter *create_particle_writer(
      const HierarchyContext *context) override;

  virtual void delete_object_writer(AbstractHierarchyWriter *writer) override;

 private:
  USDExporterContext create_usd_export_context(const HierarchyContext *context);
};

}  // namespace usd
}  // namespace io
}  // namespace blender

#endif /* __USD_HIERARCHY_ITERATOR_H__ */
