/*
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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_manipulator_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_MANIPULATOR_TYPES_H__
#define __DNA_MANIPULATOR_TYPES_H__

typedef struct wmManipulatorGroup {
	struct wmManipulatorGroup *next, *prev;

	struct wmManipulatorGroupType *type;
	ListBase manipulators;

	void *py_instance;            /* python stores the class instance here */
	struct ReportList *reports;   /* errors and warnings storage */

	void *customdata;
	void (*customdata_free)(void *); /* for freeing customdata from above */
	int flag; /* private */
	int pad;
} wmManipulatorGroup;

#endif /* __DNA_MANIPULATOR_TYPES_H__ */
