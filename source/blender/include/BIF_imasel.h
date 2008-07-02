/**
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
#ifndef BIF_IMASEL_H
#define BIF_IMASEL_H

struct SpaceImaSel;
struct ScrArea;
struct ID;

void free_imasel(struct SpaceImaSel *simasel);
void reset_imaselspace(struct ScrArea *sa);

void clever_numbuts_imasel(void);

void activate_imageselect(int type, char *title, char *file, void (*func)(char *));
void activate_imageselect_menu(int type, char *title, char *file, char *pupmenu, short *menup, void (*func)(char *));
void activate_imageselect_args(int type, char *title, char *file, void (*func)(char *, void *, void *), void *arg1, void *arg2);

void activate_databrowse_imasel(struct ID *id, int idcode, int fromcode, int retval, short *menup, void (*func)(unsigned short));
/*
void activate_databrowse_imasel_args(struct ID *id, int idcode, int fromcode, short *menup, void (*func)(char *, void *, void *), void *arg1, void *arg2);
*/
#endif

