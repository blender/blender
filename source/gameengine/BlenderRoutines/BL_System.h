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
 * System specific information / access.
 * Interface to the commandline arguments
 */

/** \file gameengine/BlenderRoutines/BL_System.h
 *  \ingroup blroutines
 */

#ifndef __BL_SYSTEM_H__
#define __BL_SYSTEM_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Game Engine command line parameters */

typedef void* SYS_SystemHandle;

extern SYS_SystemHandle SYS_GetSystem(void);
extern void SYS_DeleteSystem(SYS_SystemHandle sys);

extern int SYS_GetCommandLineInt(SYS_SystemHandle sys, const char *paramname, int defaultvalue);
extern float SYS_GetCommandLineFloat(SYS_SystemHandle sys, const char *paramname, float defaultvalue);
extern const char *SYS_GetCommandLineString(SYS_SystemHandle sys, const char *paramname, const char *defaultvalue);

extern void SYS_WriteCommandLineInt(SYS_SystemHandle sys, const char *paramname, int value);
extern void SYS_WriteCommandLineFloat(SYS_SystemHandle sys, const char *paramname, float value);
extern void SYS_WriteCommandLineString(SYS_SystemHandle sys, const char *paramname, const char *value);

/* Start game engine */

struct bContext;
struct ARegion;
struct rcti;

extern void StartKetsjiShell(struct bContext *C, struct ARegion *ar,
	struct rcti *cam_frame, int always_use_expand_framing);

#ifdef __cplusplus
}
#endif

#endif /* __BL_SYSTEM_H__ */

