/*
 * Copyright 2011, Blender Foundation.
 *
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#ifndef _COM_defines_h_
#define _COM_defines_h_

/**
 * @brief possible data types for SocketConnection
 * @ingroup Model
 */
typedef enum DataType {
	/** @brief Value data type */
	COM_DT_VALUE   = 1,
	/** @brief Vector data type */
	COM_DT_VECTOR  = 2,
	/** @brief Color data type */
	COM_DT_COLOR   = 4
} DataType;

/**
 * @brief Possible quality settings
 * @see CompositorContext.quality
 * @ingroup Execution
 */
typedef enum CompositorQuality {
	/** @brief High quality setting */
	COM_QUALITY_HIGH   = 0,
	/** @brief Medium quality setting */
	COM_QUALITY_MEDIUM = 1,
	/** @brief Low quality setting */
	COM_QUALITY_LOW    = 2
} CompositorQuality;

/**
 * @brief Possible priority settings
 * @ingroup Execution
 */
typedef enum CompositorPriority {
	/** @brief High quality setting */
	COM_PRIORITY_HIGH   = 2,
	/** @brief Medium quality setting */
	COM_PRIORITY_MEDIUM = 1,
	/** @brief Low quality setting */
	COM_PRIORITY_LOW    = 0
} CompositorPriority;

// configurable items

// chunk size determination
#define COM_PREVIEW_SIZE 140.0f
#define COM_OPENCL_ENABLED
//#define COM_DEBUG

// workscheduler threading models
/**
 * COM_TM_QUEUE is a multithreaded model, which uses the BLI_thread_queue pattern. This is the default option.
 */
#define COM_TM_QUEUE 1

/**
 * COM_TM_NOTHREAD is a single threading model, everything is executed in the caller thread. easy for debugging
 */
#define COM_TM_NOTHREAD 0

/**
 * COM_CURRENT_THREADING_MODEL can be one of the above, COM_TM_QUEUE is currently default.
 */
#define COM_CURRENT_THREADING_MODEL COM_TM_QUEUE
// chunk order
/**
 * @brief The order of chunks to be scheduled
 * @ingroup Execution
 */
typedef enum OrderOfChunks {
	/** @brief order from a distance to centerX/centerY */
	COM_TO_CENTER_OUT = 0,
	/** @brief order randomly */
	COM_TO_RANDOM = 1,
	/** @brief no ordering */
	COM_TO_TOP_DOWN = 2,
	/** @brief experimental ordering with 9 hotspots */
	COM_TO_RULE_OF_THIRDS = 3
} OrderOfChunks;

#define COM_ORDER_OF_CHUNKS_DEFAULT COM_TO_CENTER_OUT

#define COM_RULE_OF_THIRDS_DIVIDER 100.0f

#define COM_NUMBER_OF_CHANNELS 4

#endif
