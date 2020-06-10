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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "DEG_depsgraph_debug.h"

#include <algorithm>
#include <cstdarg>

#include "BLI_compiler_attrs.h"
#include "BLI_math_base.h"

#include "intern/depsgraph.h"
#include "intern/node/deg_node_id.h"

#include "DNA_ID.h"

#define NL "\r\n"

namespace DEG {
namespace {

struct DebugContext {
  FILE *file;
  const Depsgraph *graph;
  const char *label;
  const char *output_filename;
};

struct StatsEntry {
  const IDNode *id_node;
  double time;
};

/* TODO(sergey): De-duplicate with graphviz relation debugger. */
static void deg_debug_fprintf(const DebugContext &ctx, const char *fmt, ...)
    ATTR_PRINTF_FORMAT(2, 3);
static void deg_debug_fprintf(const DebugContext &ctx, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(ctx.file, fmt, args);
  va_end(args);
}

BLI_INLINE double get_node_time(const DebugContext & /*ctx*/, const Node *node)
{
  // TODO(sergey): Figure out a nice way to define which exact time
  // we want to show.
  return node->stats.current_time;
}

bool stat_entry_comparator(const StatsEntry &a, const StatsEntry &b)
{
  return a.time > b.time;
}

string gnuplotify_id_code(const string &name)
{
  return string("") + name[0] + name[1];
}

string gnuplotify_name(const string &name)
{
  string result = "";
  const int length = name.length();
  for (int i = 0; i < length; i++) {
    const char ch = name[i];
    if (ch == '_') {
      result += "\\\\\\";
    }
    result += ch;
  }
  return result;
}

void write_stats_data(const DebugContext &ctx)
{
  // Fill in array of all stats which are to be displayed.
  Vector<StatsEntry> stats;
  stats.reserve(ctx.graph->id_nodes.size());
  for (const IDNode *id_node : ctx.graph->id_nodes) {
    const double time = get_node_time(ctx, id_node);
    if (time == 0.0) {
      continue;
    }
    StatsEntry entry;
    entry.id_node = id_node;
    entry.time = time;
    stats.append(entry);
  }
  // Sort the data.
  std::sort(stats.begin(), stats.end(), stat_entry_comparator);
  // We limit number of entries, otherwise things become unreadable.
  stats.resize(min_ii(stats.size(), 32));
  std::reverse(stats.begin(), stats.end());
  // Print data to the file stream.
  deg_debug_fprintf(ctx, "$data << EOD" NL);
  for (const StatsEntry &entry : stats) {
    deg_debug_fprintf(ctx,
                      "\"[%s] %s\",%f" NL,
                      gnuplotify_id_code(entry.id_node->id_orig->name).c_str(),
                      gnuplotify_name(entry.id_node->id_orig->name + 2).c_str(),
                      entry.time);
  }
  deg_debug_fprintf(ctx, "EOD" NL);
}

void deg_debug_stats_gnuplot(const DebugContext &ctx)
{
  // Data itself.
  write_stats_data(ctx);
  // Optional label.
  if (ctx.label && ctx.label[0]) {
    deg_debug_fprintf(ctx, "set title \"%s\"" NL, ctx.label);
  }
  // Rest of the commands.
  // TODO(sergey): Need to decide on the resolution somehow.
  deg_debug_fprintf(ctx, "set terminal pngcairo size 1920,1080" NL);
  deg_debug_fprintf(ctx, "set output \"%s\"" NL, ctx.output_filename);
  deg_debug_fprintf(ctx, "set grid" NL);
  deg_debug_fprintf(ctx, "set datafile separator ','" NL);
  deg_debug_fprintf(ctx, "set style fill solid" NL);
  deg_debug_fprintf(ctx,
                    "plot \"$data\" using "
                    "($2*0.5):0:($2*0.5):(0.2):yticlabels(1) "
                    "with boxxyerrorbars t '' lt rgb \"#406090\"" NL);
}

}  // namespace
}  // namespace DEG

void DEG_debug_stats_gnuplot(const Depsgraph *depsgraph,
                             FILE *f,
                             const char *label,
                             const char *output_filename)
{
  if (depsgraph == nullptr) {
    return;
  }
  DEG::DebugContext ctx;
  ctx.file = f;
  ctx.graph = (DEG::Depsgraph *)depsgraph;
  ctx.label = label;
  ctx.output_filename = output_filename;
  DEG::deg_debug_stats_gnuplot(ctx);
}
