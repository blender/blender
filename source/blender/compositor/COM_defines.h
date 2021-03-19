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

/**
 * \brief possible data types for sockets
 * \ingroup Model
 */
enum class DataType {
  /** \brief Value data type */
  Value = 0,
  /** \brief Vector data type */
  Vector = 1,
  /** \brief Color data type */
  Color = 2,
};

/**
 * \brief Possible quality settings
 * \see CompositorContext.quality
 * \ingroup Execution
 */
enum class CompositorQuality {
  /** \brief High quality setting */
  High = 0,
  /** \brief Medium quality setting */
  Medium = 1,
  /** \brief Low quality setting */
  Low = 2,
};

/**
 * \brief Possible priority settings
 * \ingroup Execution
 */
enum class CompositorPriority {
  /** \brief High quality setting */
  High = 2,
  /** \brief Medium quality setting */
  Medium = 1,
  /** \brief Low quality setting */
  Low = 0,
};

// configurable items

// chunk size determination
#define COM_PREVIEW_SIZE 140.0f
#define COM_OPENCL_ENABLED
//#define COM_DEBUG

// workscheduler threading models
/**
 * COM_TM_QUEUE is a multi-threaded model, which uses the BLI_thread_queue pattern.
 * This is the default option.
 */
#define COM_TM_QUEUE 1

/**
 * COM_TM_NOTHREAD is a single threading model, everything is executed in the caller thread.
 * easy for debugging
 */
#define COM_TM_NOTHREAD 0

/**
 * COM_CURRENT_THREADING_MODEL can be one of the above, COM_TM_QUEUE is currently default.
 */
#define COM_CURRENT_THREADING_MODEL COM_TM_QUEUE
// chunk order
/**
 * \brief The order of chunks to be scheduled
 * \ingroup Execution
 */
enum class ChunkOrdering {
  /** \brief order from a distance to centerX/centerY */
  CenterOut = 0,
  /** \brief order randomly */
  Random = 1,
  /** \brief no ordering */
  TopDown = 2,
  /** \brief experimental ordering with 9 hot-spots. */
  RuleOfThirds = 3,

  Default = ChunkOrdering::CenterOut,
};

#define COM_RULE_OF_THIRDS_DIVIDER 100.0f

#define COM_NUM_CHANNELS_VALUE 1
#define COM_NUM_CHANNELS_VECTOR 3
#define COM_NUM_CHANNELS_COLOR 4

#define COM_BLUR_BOKEH_PIXELS 512
