/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BSP_CSGUserData.h"

 

BSP_CSGUserData::
BSP_CSGUserData(
	const int width
):
	m_width (width)
{
}

/**
 * Add a new uninitialized record to the end of the 
 * array
 */

	void
BSP_CSGUserData::
IncSize(
){
	m_data.insert(m_data.end(),m_width,char(0));
}

	int
BSP_CSGUserData::
Duplicate(
	void *record
){
	if (m_width) {
		int output = Size();
		IncSize();

		memcpy(&m_data[ m_data.size() - m_width ], record, m_width);
	
		return output;
	}
	return 0;
}	

	void
BSP_CSGUserData::
Duplicate(
	int record_index
){
	if (m_width) {
		IncSize();
		memcpy(&m_data[ m_data.size() - m_width ], 
			&m_data[ record_index * m_width], m_width);
	}
}	


	void
BSP_CSGUserData::
Copy(
	void *output,
	int pos
){
	if (m_width) {
		memcpy(output, &m_data[m_width*pos],m_width);
	}
}
	void
BSP_CSGUserData::
Reserve(
	int size
){
	m_data.reserve(size * m_width);
}


/// Return the width of an individual record

	int
BSP_CSGUserData::
Width(
) const{
	return m_width;
}


/// return the current number of records stored in the array.
	int
BSP_CSGUserData::
Size(
) const {
	if (m_width == 0) return 0;
	return m_data.size() / m_width;
} 


/// return a pointer to the start of the nth record in the array.

	void *
BSP_CSGUserData::
operator [] (
	const int pos
){
	return &m_data[ m_width*pos ];
}

