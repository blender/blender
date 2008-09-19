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

#ifndef BIF_SPACE_H
#define BIF_SPACE_H

struct ListBase;
struct ScrArea;
struct SpaceButs;
struct View2D;
struct BWinEvent;
struct SpaceOops;

#define REMAKEIPO		1
#define OOPS_TEST		2

#define BUT_HORIZONTAL 	1
#define BUT_VERTICAL 	2

/* is hardcoded in DNA_space_types.h */
#define SPACE_MAXHANDLER	8

/* view3d handler codes */
#define VIEW3D_HANDLER_BACKGROUND	1
#define VIEW3D_HANDLER_PROPERTIES	2
#define VIEW3D_HANDLER_OBJECT		3
#define VIEW3D_HANDLER_PREVIEW		4
#define VIEW3D_HANDLER_MULTIRES         5
#define VIEW3D_HANDLER_TRANSFORM	6
#define VIEW3D_HANDLER_GREASEPENCIL 7

/* ipo handler codes */
#define IPO_HANDLER_PROPERTIES	20

/* image handler codes */
#define IMAGE_HANDLER_PROPERTIES	30
#define IMAGE_HANDLER_PAINT			31
#define IMAGE_HANDLER_CURVES		32
#define IMAGE_HANDLER_PREVIEW		33
#define IMAGE_HANDLER_GAME_PROPERTIES	34
#define IMAGE_HANDLER_VIEW_PROPERTIES	35
/*#define IMAGE_HANDLER_TRANSFORM_PROPERTIES	36*/

/* action handler codes */
#define ACTION_HANDLER_PROPERTIES	40

/* nla handler codes */
#define NLA_HANDLER_PROPERTIES	50

/* sequence handler codes */
#define SEQ_HANDLER_PROPERTIES		60
#define SEQ_HANDLER_GREASEPENCIL 	61

/* imasel handler codes */
#define IMASEL_HANDLER_IMAGE	70

/* nodes handler codes */
#define NODES_HANDLER_GREASEPENCIL		80

/* theme codes */
#define B_ADD_THEME 	3301
#define B_DEL_THEME 	3302
#define B_NAME_THEME 	3303
#define B_THEMECOL 		3304
#define B_UPDATE_THEME 	3305
#define B_CHANGE_THEME 	3306
#define B_THEME_COPY 	3307
#define B_THEME_PASTE 	3308
#define B_UPDATE_THEME_ICONS 	3309

#define B_RECALCLIGHT 	3310


void	scrarea_do_winprefetchdraw	(struct ScrArea *sa);
void	scrarea_do_windraw		(struct ScrArea *sa);
void	scrarea_do_winchange	(struct ScrArea *sa);
void	scrarea_do_winhandle	(struct ScrArea *sa, struct BWinEvent *evt);
void	scrarea_do_headdraw		(struct ScrArea *sa);
void	scrarea_do_headchange	(struct ScrArea *sa);

/* space.c */
extern		void add_blockhandler(struct ScrArea *sa, short eventcode, short action);
extern		void rem_blockhandler(struct ScrArea *sa, short eventcode);
extern		void toggle_blockhandler(struct ScrArea *sa, short eventcode, short action);

extern		 void space_set_commmandline_options(void);
extern       void allqueue(unsigned short event, short val);
extern       void allspace(unsigned short event, short val);
extern       void copy_view3d_lock(short val);
extern       void drawemptyspace(struct ScrArea *sa, void *spacedata);
extern       void drawinfospace(struct ScrArea *sa, void *spacedata);
extern       void duplicatespacelist(struct ScrArea *area, struct ListBase *lb1, struct ListBase *lb2);
extern       void extern_set_butspace(int fkey, int do_cycle);
extern       void force_draw(int header);
extern		 void force_draw_all(int header);
extern		 void force_draw_plus(int type, int header);
extern       void freespacelist(struct ScrArea *sa);
extern       void handle_view3d_around(void);
extern       void handle_view3d_lock(void);
extern		 void handle_view_middlemouse(void);
extern       void init_v2d_oops(struct ScrArea *, struct SpaceOops *);
extern       void initipo(struct ScrArea *sa);
extern       void newspace(struct ScrArea *sa, int type);
extern       void set_rects_butspace(struct SpaceButs *buts);
extern       void test_butspace(void);
extern       void start_game(void);
extern		 void select_object_grouped(short nr);
extern		 void join_menu(void);

extern 		void BIF_undo_push(char *str);
extern 		void BIF_undo(void);
extern 		void BIF_redo(void);
extern 		void BIF_undo_menu(void);

#if 0
//#ifdef _WIN32	// FULLSCREEN
extern		 void mainwindow_toggle_fullscreen(int fullscreen);
#endif

extern		 void mainwindow_set_filename_to_title(char *title);
extern		 void mainwindow_raise(void);
extern		 void mainwindow_make_active(void);
extern		 void mainwindow_close(void);

#endif


