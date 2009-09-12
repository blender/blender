/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __KX_TIME_CATEGORY_LOGGER_H
#define __KX_TIME_CATEGORY_LOGGER_H

#ifdef WIN32
#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#endif

#include <map>

#include "KX_TimeLogger.h"

/**
 * Stores and manages time measurements by category.
 * Categories can be added dynamically.
 * Average measurements can be established for each separate category
 * or for all categories together.
 */
class KX_TimeCategoryLogger {
public:
	typedef int TimeCategory;

	/**
	 * Constructor.
	 * @param maxNumMesasurements Maximum number of measurements stored (> 1).
	 */
	KX_TimeCategoryLogger(unsigned int maxNumMeasurements = 10);

	/**
	 * Destructor.
	 */
	virtual ~KX_TimeCategoryLogger(void);

	/**
	 * Changes the maximum number of measurements that can be stored.
	 */
	virtual void SetMaxNumMeasurements(unsigned int maxNumMeasurements);

	/**
	 * Changes the maximum number of measurements that can be stored.
	 */
	virtual unsigned int GetMaxNumMeasurements(void) const;

	/**
	 * Adds a category.
	 * @param category	The new category.
	 */
	virtual void AddCategory(TimeCategory tc);

	/**
	 * Starts logging in current measurement for the given category.
	 * @param tc					The category to log to.
	 * @param now					The current time.
	 * @param endOtherCategories	Whether to stop logging to other categories.
	 */
	virtual void StartLog(TimeCategory tc, double now, bool endOtherCategories = true);

	/**
	 * End logging in current measurement for the given category.
	 * @param tc	The category to log to.
	 * @param now	The current time.
	 */
	virtual void EndLog(TimeCategory tc, double now);

	/**
	 * End logging in current measurement for all categories.
	 * @param now	The current time.
	 */
	virtual void EndLog(double now);

	/**
	 * Logs time in next measurement.
	 * @param now	The current time.
	 */
	virtual void NextMeasurement(double now);

	/**
	 * Returns average of all but the current measurement time.
	 * @return The average of all but the current measurement.
	 */
	virtual double GetAverage(TimeCategory tc);

	/**
	 * Returns average for grand total.
	 */
	virtual double GetAverage(void);

protected:
	/**  
	 * Disposes loggers.
	 */  
	virtual void DisposeLoggers(void);

	/** Storage for the loggers. */
	typedef std::map<TimeCategory, KX_TimeLogger*> KX_TimeLoggerMap;
	KX_TimeLoggerMap m_loggers;
	/** Maximum number of measurements. */
	unsigned int m_maxNumMeasurements;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_TimeCategoryLogger"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // __KX_TIME_CATEGORY_LOGGER_H

