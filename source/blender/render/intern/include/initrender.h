/*
 * initrender_ext.h
 *
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

#ifndef INITRENDER_EXT_H
#define INITRENDER_EXT_H 

/* type includes */

#include "DNA_effect_types.h"        /* for PartEff type */
#include "render_types.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Functions */

void init_def_material(void);
void init_render_jit(int nr);
float  calc_weight(float *weight, int i, int j);
void defaultlamp(void);
void schrijfplaatje(char *name);
void initparts(void);
short setpart(short nr); /* return 0 als geen goede part */
void addparttorect(short nr, Part *part);
void add_to_blurbuf(int blur);
void oldRenderLoop(void); /* Calls the old renderer. Contains the PART and FIELD loops. */
void render(void);  /* Switch between the old and the unified renderer. */
/*  void write_screendump(char *name); not here !*/

#endif /* INITRENDER_EXT_H */

