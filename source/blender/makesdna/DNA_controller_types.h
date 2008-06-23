/**
 * blenlib/DNA_controller_types.h (mar-2001 nzc)
 *	
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
#ifndef DNA_CONTROLLER_TYPES_H
#define DNA_CONTROLLER_TYPES_H

struct bActuator;
struct Text;
struct bSensor;

/* ****************** CONTROLLERS ********************* */

typedef struct bExpressionCont {
	char str[128];
} bExpressionCont;

typedef struct bPythonCont {
	struct Text *text;
} bPythonCont;

typedef struct bController {
	struct bController *next, *prev, *mynew;
	short type, flag, inputs, totlinks;
	short otype, totslinks, pad2, pad3;
	
	char name[32];
	void *data;
	
	struct bActuator **links;

	struct bSensor **slinks;
	short val, valo;
	unsigned int state_mask;
	
} bController;

/* controller->type */
#define CONT_LOGIC_AND	0
#define CONT_LOGIC_OR	1
#define CONT_EXPRESSION	2
#define CONT_PYTHON		3

/* controller->flag */
#define CONT_SHOW		1
#define CONT_DEL		2
#define CONT_NEW		4
#define CONT_MASK		8

#endif

