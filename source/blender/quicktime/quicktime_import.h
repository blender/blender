/*
 * Quicktime_import.h
 *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2002-2003 by TNCCI Inc.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/quicktime/quicktime_import.h
 *  \ingroup quicktime
 */



#ifndef __QUICKTIME_IMPORT_H__
#define __QUICKTIME_IMPORT_H__

#define __AIFF__

#include "../imbuf/IMB_imbuf.h"
#include "../imbuf/IMB_imbuf_types.h"

#ifdef _WIN32
#  ifndef __FIXMATH__
#    include <FixMath.h>
#  endif /* __FIXMATH__ */
#endif /* _WIN32 _ */

/* init/exit */

void quicktime_init(void);
void quicktime_exit(void);

/* quicktime movie import functions */

int		anim_is_quicktime(const char *name);
int		startquicktime(struct anim *anim);
void	free_anim_quicktime(struct anim *anim);
ImBuf  *qtime_fetchibuf(struct anim *anim, int position);

#endif  /* __QUICKTIME_IMPORT_H__ */
