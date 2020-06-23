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
#ifndef __USD_WRITER_HAIR_H__
#define __USD_WRITER_HAIR_H__

#include "usd_writer_abstract.h"

namespace blender {
namespace io {
namespace usd {

/* Writer for writing hair particle data as USD curves. */
class USDHairWriter : public USDAbstractWriter {
 public:
  USDHairWriter(const USDExporterContext &ctx);

 protected:
  virtual void do_write(HierarchyContext &context) override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;
};

}  // namespace usd
}  // namespace io
}  // namespace blender

#endif /* __USD_WRITER_HAIR_H__ */
