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

struct ScrArea;
struct BWinEvent;

typedef struct _SpaceType	SpaceType;

typedef	void	(*SpacePrefetchDrawFP)	(struct ScrArea *sa, void *spacedata);
typedef	void	(*SpaceDrawFP)		(struct ScrArea *sa, void *spacedata);
typedef	void	(*SpaceChangeFP)	(struct ScrArea *sa, void *spacedata);
typedef	void	(*SpaceHandleFP)	(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

	/***/

SpaceType*	spacetype_new			(char *name);

void		spacetype_set_winfuncs	(SpaceType *st, SpacePrefetchDrawFP prefetch, SpaceDrawFP draw, SpaceChangeFP change, SpaceHandleFP handle);

	/***/

SpaceType *spaceaction_get_type		(void);
SpaceType *spacebuts_get_type		(void);
SpaceType *spacefile_get_type		(void);
SpaceType *spaceimage_get_type		(void);
SpaceType *spaceimasel_get_type		(void);
SpaceType *spaceinfo_get_type		(void);
SpaceType *spaceipo_get_type		(void);
SpaceType *spacenla_get_type		(void);
SpaceType *spaceoops_get_type		(void);
SpaceType *spaceseq_get_type		(void);
SpaceType *spacesound_get_type		(void);
SpaceType *spacetext_get_type		(void);
SpaceType *spacescript_get_type		(void);
SpaceType *spaceview3d_get_type		(void);
SpaceType *spacetime_get_type		(void);
SpaceType *spacenode_get_type		(void);

