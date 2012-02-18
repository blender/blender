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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file memutil/MEM_NonCopyable.h
 *  \ingroup memutil
 */

/**
 * @file	MEM_NonCopyable.h
 * Declaration of MEM_NonCopyable class.
 */

#ifndef __MEM_NONCOPYABLE_H__
#define __MEM_NONCOPYABLE_H__

/**
 * Simple class that makes sure sub classes cannot
 * generate standard copy constructors.
 * If you want to make sure that your class does
 * not have any of these cheesy hidden constructors
 * inherit from this class.
 */

class MEM_NonCopyable {
protected :

	MEM_NonCopyable(
	) {
	};

private :

	MEM_NonCopyable (const MEM_NonCopyable *);
	MEM_NonCopyable (const MEM_NonCopyable &);
};

#endif

