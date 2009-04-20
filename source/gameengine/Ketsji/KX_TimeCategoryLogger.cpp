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

#include "KX_TimeCategoryLogger.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_TimeCategoryLogger::KX_TimeCategoryLogger(unsigned int maxNumMeasurements)
: m_maxNumMeasurements(maxNumMeasurements)
{
}


KX_TimeCategoryLogger::~KX_TimeCategoryLogger(void)
{
	DisposeLoggers();
}


void KX_TimeCategoryLogger::SetMaxNumMeasurements(unsigned int maxNumMeasurements)
{
	KX_TimeLoggerMap::iterator it;
	for (it = m_loggers.begin(); it != m_loggers.end(); it++) {
		it->second->SetMaxNumMeasurements(maxNumMeasurements);
	}
	m_maxNumMeasurements = maxNumMeasurements;
}


unsigned int KX_TimeCategoryLogger::GetMaxNumMeasurements(void) const
{
	return m_maxNumMeasurements;
}


void KX_TimeCategoryLogger::AddCategory(TimeCategory tc)
{
	// Only add if not already present
	if (m_loggers.find(tc) == m_loggers.end()) {
		KX_TimeLogger* logger = new KX_TimeLogger(m_maxNumMeasurements);
		//assert(logger);
		m_loggers.insert(KX_TimeLoggerMap::value_type(tc, logger));
	}
}


void KX_TimeCategoryLogger::StartLog(TimeCategory tc, double now, bool endOtherCategories)
{
	if (endOtherCategories) {
		KX_TimeLoggerMap::iterator it;
		for (it = m_loggers.begin(); it != m_loggers.end(); it++) {
			if (it->first != tc) {
				it->second->EndLog(now);
			}
		}
	}
	//assert(m_loggers[tc] != m_loggers.end());
	m_loggers[tc]->StartLog(now);
}


void KX_TimeCategoryLogger::EndLog(TimeCategory tc, double now)
{
	//assert(m_loggers[tc] != m_loggers.end());
	m_loggers[tc]->EndLog(now);
}


void KX_TimeCategoryLogger::EndLog(double now)
{
	KX_TimeLoggerMap::iterator it;
	for (it = m_loggers.begin(); it != m_loggers.end(); it++) {
		it->second->EndLog(now);
	}
}


void KX_TimeCategoryLogger::NextMeasurement(double now)
{
	KX_TimeLoggerMap::iterator it;
	for (it = m_loggers.begin(); it != m_loggers.end(); it++) {
		it->second->NextMeasurement(now);
	}
}


double KX_TimeCategoryLogger::GetAverage(TimeCategory tc)
{
	//assert(m_loggers[tc] != m_loggers.end());
	return m_loggers[tc]->GetAverage();
}


double KX_TimeCategoryLogger::GetAverage(void)
{
	double time = 0.;

	KX_TimeLoggerMap::iterator it;
	for (it = m_loggers.begin(); it != m_loggers.end(); it++) {
		time += it->second->GetAverage();
	}

	return time;
}


void KX_TimeCategoryLogger::DisposeLoggers(void)
{
	KX_TimeLoggerMap::iterator it;
	for (it = m_loggers.begin(); it != m_loggers.end(); it++) {
		delete it->second;
	}
}

