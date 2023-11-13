/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/debug/deg_debug.h"

#include "BLI_console.h"
#include "BLI_hash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "PIL_time_utildefines.h"

#include "BKE_global.h"

#include "intern/depsgraph.hh"

namespace blender::deg {

DepsgraphDebug::DepsgraphDebug()
    : flags(G.debug), is_ever_evaluated(false), graph_evaluation_start_time_(0)
{
}

bool DepsgraphDebug::do_time_debug() const
{
  return ((G.debug & G_DEBUG_DEPSGRAPH_TIME) != 0);
}

void DepsgraphDebug::begin_graph_evaluation()
{
  if (!do_time_debug()) {
    return;
  }

  const double current_time = PIL_check_seconds_timer();

  if (is_ever_evaluated) {
    fps_samples_.add_sample(current_time - graph_evaluation_start_time_);
  }

  graph_evaluation_start_time_ = current_time;
}

void DepsgraphDebug::end_graph_evaluation()
{
  if (!do_time_debug()) {
    return;
  }

  const double graph_eval_end_time = PIL_check_seconds_timer();
  const double graph_eval_time = graph_eval_end_time - graph_evaluation_start_time_;

  const double fps_samples = fps_samples_.get_averaged();
  const double fps = fps_samples ? 1.0 / fps_samples_.get_averaged() : 0.0;

  if (name.empty()) {
    printf("Depsgraph updated in %f seconds.\n", graph_eval_time);
    printf("Depsgraph evaluation FPS: %f\n", fps);
  }
  else {
    printf("Depsgraph [%s] updated in %f seconds.\n", name.c_str(), graph_eval_time);
    printf("Depsgraph [%s] evaluation FPS: %f\n", name.c_str(), fps);
  }

  is_ever_evaluated = true;
}

bool terminal_do_color()
{
  return (G.debug & G_DEBUG_DEPSGRAPH_PRETTY) != 0;
}

string color_for_pointer(const void *pointer)
{
  if (!terminal_do_color()) {
    return "";
  }
  int r, g, b;
  BLI_hash_pointer_to_color(pointer, &r, &g, &b);
  char buffer[64];
  SNPRINTF(buffer, TRUECOLOR_ANSI_COLOR_FORMAT, r, g, b);
  return string(buffer);
}

string color_end()
{
  if (!terminal_do_color()) {
    return "";
  }
  return string(TRUECOLOR_ANSI_COLOR_FINISH);
}

}  // namespace blender::deg
