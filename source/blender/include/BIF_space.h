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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef BIF_SPACE_H
#define BIF_SPACE_H

struct ListBase;
struct ScrArea;
struct SpaceButs;
struct View2D;

#define REMAKEIPO		1
#define OOPS_TEST		2
#define REMAKEALLIPO	3	/* Reevan's ipo fixing test */

void	scrarea_do_windraw		(struct ScrArea *sa);
void	scrarea_do_winchange	(struct ScrArea *sa);
void	scrarea_do_winhandle	(struct ScrArea *sa, unsigned short event, short val, char ascii);
void	scrarea_do_headdraw		(struct ScrArea *sa);
void	scrarea_do_headchange	(struct ScrArea *sa);

/* space.c */
extern		 void space_set_commmandline_options(void);
extern       void allqueue(unsigned short event, short val);
extern       void allspace(unsigned short event, short val);
extern       void changebutspace(void);
extern       void changeview2d(void);
extern       void changeview3d(void);
extern       void copy_view3d_lock(short val);
extern       void drawemptyspace(void);
extern       void drawinfospace(void);
extern       void duplicatespacelist(struct ScrArea *area, struct ListBase *lb1, struct ListBase *lb2);
extern       void extern_set_butspace(int fkey);
extern       void force_draw(void);
extern		 void force_draw_all(void);
extern		 void force_draw_plus(int type);
extern       void freespacelist(struct ListBase *lb);
extern       void handle_view3d_lock(void);
extern       void init_butspace(struct ScrArea *sa);
extern       void init_filespace(struct ScrArea *sa);
extern       void init_imagespace(struct ScrArea *sa);
extern       void init_oopsspace(struct ScrArea *sa);
extern       void init_nlaspace(struct ScrArea *sa);
extern       void init_seqspace(struct ScrArea *sa);
extern       void init_v2d_oops(struct View2D *v2d);
extern       void initipo(struct ScrArea *sa);
extern       void initview3d(struct ScrArea *sa);
extern       void newspace(struct ScrArea *sa, int type);
extern       void set_rects_butspace(struct SpaceButs *buts);
extern       void winqread3d(unsigned short event, short val, char ascii);
extern       void winqreadbutspace(unsigned short event, short val, char ascii);
extern       void winqreadimagespace(unsigned short event, short val, char ascii);
extern       void winqreadinfospace(unsigned short event, short val, char ascii);
extern       void winqreadipo(unsigned short event, short val, char ascii);
extern       void winqreadoopsspace(unsigned short event, short val, char ascii);
extern       void winqreadnlaspace(unsigned short event, short val, char ascii);
extern       void winqreadsequence(unsigned short event, short val, char ascii);
extern       void test_butspace(void);
extern       void start_game(void);

extern		 void mainwindow_raise(void);
extern		 void mainwindow_make_active(void);
extern		 void mainwindow_close(void);

#endif

