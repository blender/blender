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

#ifndef BSE_EDITACTION_TYPES_H
#define BSE_EDITACTION_TYPES_H

#define	CHANNELHEIGHT	16
#define	CHANNELSKIP		2
#define ACTWIDTH	128

#define CHANNEL_FILTER_LOC		0x00000001	/* Show location keys */
#define CHANNEL_FILTER_ROT		0x00000002	/* Show rotation keys */
#define CHANNEL_FILTER_SIZE		0x00000004	/* Show size keys */
#define CHANNEL_FILTER_CON		0x00000008	/* Show constraint keys */
#define CHANNEL_FILTER_RGB		0x00000010	/* Show object color keys */

#define CHANNEL_FILTER_CU		0x00010000	/* Show curve keys */
#define CHANNEL_FILTER_ME		0x00020000	/* Show mesh keys */
#define CHANNEL_FILTER_LA		0x00040000	/* Show lamp keys */

#endif /* BSE_EDITACTION_TYPES_H */
