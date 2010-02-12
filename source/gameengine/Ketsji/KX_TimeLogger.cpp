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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "KX_TimeLogger.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_TimeLogger::KX_TimeLogger(unsigned int maxNumMeasurements) : 
	m_maxNumMeasurements(maxNumMeasurements), 
	m_logStart(0),
	m_logging(false)
{
}


KX_TimeLogger::~KX_TimeLogger(void)
{
}


void KX_TimeLogger::SetMaxNumMeasurements(unsigned int maxNumMeasurements)
{
	if ((m_maxNumMeasurements != maxNumMeasurements) && maxNumMeasurements) {
		// Actual removing is done in NextMeasurement()
		m_maxNumMeasurements = maxNumMeasurements;
	}
}


unsigned int KX_TimeLogger::GetMaxNumMeasurements(void) const
{
	return m_maxNumMeasurements;
}


void KX_TimeLogger::StartLog(double now)
{
	if (!m_logging) {
		m_logging = true;
		m_logStart = now;
	}
}


void KX_TimeLogger::EndLog(double now)
{
	if (m_logging) {
		m_logging = false;
		double time = now - m_logStart;
		if (m_measurements.size() > 0) {
			m_measurements[0] += time;
		}
	}
}


void KX_TimeLogger::NextMeasurement(double now)
{
	// End logging to current measurement
	EndLog(now);

	// Add a new measurement at the front
	double m = 0.;
	m_measurements.push_front(m);

	// Remove measurement if we grow beyond the maximum size
	if ((m_measurements.size()) > m_maxNumMeasurements) {
		while (m_measurements.size() > m_maxNumMeasurements) {
			m_measurements.pop_back();
		}
	}
}



double KX_TimeLogger::GetAverage(void) const
{
	double avg = 0.;

	unsigned int numMeasurements = m_measurements.size();
	if (numMeasurements > 1) {
		for (unsigned int i = 1; i < numMeasurements; i++) {
			avg += m_measurements[i];
		}
		avg /= (float)numMeasurements - 1;
	}

	return avg;
}

