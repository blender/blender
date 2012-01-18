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

/** \file DNA_controller_types.h
 *  \ingroup DNA
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
	char module[64];
	int mode;
	int flag; /* only used for debug now */
} bPythonCont;

typedef struct bController {
	struct bController *next, *prev, *mynew;
	short type, flag, inputs, totlinks;
	short otype, totslinks, pad2, pad3;
	
	char name[64];
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
#define CONT_LOGIC_NAND	4
#define CONT_LOGIC_NOR	5
#define CONT_LOGIC_XOR	6
#define CONT_LOGIC_XNOR	7

/* controller->flag */
#define CONT_SHOW		1
#define CONT_DEL		2
#define CONT_NEW		4
#define CONT_MASK		8
#define CONT_PRIO		16

/* pyctrl->flag */
#define CONT_PY_DEBUG	1

/* pyctrl->mode */
#define CONT_PY_SCRIPT	0
#define CONT_PY_MODULE	1

#endif

