/**
 * $Id$
 *
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

#ifndef BDR_IMAGEPAINT_H
#define BDR_IMAGEPAINT_H

/* ImagePaint.current */
#define IMAGEPAINT_BRUSH 0
#define IMAGEPAINT_AIRBRUSH 1
#define IMAGEPAINT_SOFTEN 2
#define IMAGEPAINT_AUX1 3
#define IMAGEPAINT_AUX2 4
#define IMAGEPAINT_SMEAR 5
#define IMAGEPAINT_CLONE 6
#define IMAGEPAINT_TOOL_SIZE 7

/* ImagePaint.flag */
#define IMAGEPAINT_DRAW_TOOL 1
#define IMAGEPAINT_DRAW_TOOL_DRAWING 2
#define IMAGEPAINT_DRAWING 4
#define IMAGEPAINT_TORUS 8
#define IMAGEPAINT_TIMED 16

typedef struct ImagePaintTool {
	float rgba[4];
	int size;
	float innerradius;
	float timing;
} ImagePaintTool;

typedef struct ImagePaint {
	struct Clone {
		Image *image;
		float offset[2];
		float alpha;
	} clone;

	ImagePaintTool tool[IMAGEPAINT_TOOL_SIZE];

	short flag, current;
} ImagePaint;

extern struct ImagePaint Gip;

void imagepaint_redraw_tool(void);
void imagepaint_paint(short mousebutton);
void imagepaint_pick(short mousebutton);

#endif /*  BDR_IMAGEPAINT_H */

