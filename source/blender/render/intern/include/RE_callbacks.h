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
 * Callbacks to make the renderer interact with calling modules.
 */

#ifndef RE_CALLBACKS_H
#define RE_CALLBACKS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * Test whether operation should be prematurely terminated.
	 *
	 * @returns 0 to continue, any other value to break.
	 */
	int RE_local_test_break(void);

	/**
	 * Set a red square with the argument as text as cursor.
	 */
	void RE_local_timecursor(int i);

	/**
	 * Render these lines from the renderbuffer on screen (needs better spec) 
	 */
	void RE_local_render_display(int i, int j, int k, int l, unsigned int *m);

	/**
	 * Initialise a render display (needs better spec)
	 */
	void RE_local_init_render_display(void);

	/**
	 * Clear/close a render display (needs better spec)
	 */
	void RE_local_clear_render_display(short);

	/**
	 * Print render statistics.
	 */
	void RE_local_printrenderinfo(double time, int i);

	/** Get the data for the scene to render. */
	void RE_local_get_renderdata(void);
	
	/** Release the data for the scene that was rendered. */
	void RE_local_free_renderdata(void);

	
#ifdef __cplusplus
}
#endif

#endif

