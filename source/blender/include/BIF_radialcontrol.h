/*
 * $Id: multires.h 13015 2007-12-27 07:27:03Z nicholasbishop $
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RADIALCONTROL_H
#define RADIALCONTROL_H

struct NumInput;

#define RADIALCONTROL_NONE 0
#define RADIALCONTROL_SIZE 1
#define RADIALCONTROL_STRENGTH 2
#define RADIALCONTROL_ROTATION 3

typedef void (*RadialControlCallback)(const int, const int);

typedef struct RadialControl {
	int mode;
	short origloc[2];

	unsigned int tex;
	
	int new_value;
	int original_value;
	int max_value;
	RadialControlCallback callback;
	
	struct NumInput *num;
} RadialControl;

RadialControl *radialcontrol_start(const int mode, RadialControlCallback callback,
				   const int original_value, const int max_value,
				   const unsigned int tex);
void radialcontrol_do_events(RadialControl *rc, const unsigned short event);
void radialcontrol_draw(RadialControl *rc);

#endif
