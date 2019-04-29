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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

struct Base;
struct Main;

namespace DEG {

struct Depsgraph;
class DepsgraphBuilderCache;

class DepsgraphBuilder {
 public:
  bool need_pull_base_into_graph(Base *base);

 protected:
  /* NOTE: The builder does NOT take ownership over any of those resources. */
  DepsgraphBuilder(Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache);

  /* State which never changes, same for the whole builder time. */
  Main *bmain_;
  Depsgraph *graph_;
  DepsgraphBuilderCache *cache_;
};

bool deg_check_base_in_depsgraph(const Depsgraph *graph, Base *base);
void deg_graph_build_finalize(struct Main *bmain, struct Depsgraph *graph);

}  // namespace DEG
