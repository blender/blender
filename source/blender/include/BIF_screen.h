/* replacement for screen.h */
/*
 * 
 *	Leftovers here are actually editscreen.c thingies
 * 
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

#ifndef BIF_SCREEN_H
#define BIF_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif


/* externs in editscreen.c */
extern int displaysizex, displaysizey;
extern struct ScrArea *curarea;

struct View3D;
struct bScreen;
struct ScrArea;
struct ScrVert;
struct ScrEdge;
struct ListBase;

struct View3D *find_biggest_view3d(void);
struct ScrArea *find_biggest_area_of_type(int spacecode);
struct ScrArea *find_biggest_area(void);

void scrarea_queue_redraw(struct ScrArea *area);
void scrarea_queue_winredraw(struct ScrArea *area);
void scrarea_queue_headredraw(struct ScrArea *area);

int blender_test_break(void);

void duplicate_screen(void);
void init_screen_cursors(void);
void set_timecursor(int nr);
void waitcursor(int val);
void wich_cursor(struct ScrArea *sa);
void setcursor_space(int spacetype, short cur);
void set_g_activearea(struct ScrArea *sa);
void getmouseco_sc(short *mval);
void getmouseco_areawin(short *mval);
void getmouseco_headwin(short *mval);
int qtest(void);
int anyqtest(void);
void areawinset(short win);
void headerbox(struct ScrArea *sa);
void defheaddraw(void);
void defheadchange(void);
unsigned short winqtest(struct ScrArea *sa);
unsigned short headqtest(struct ScrArea *sa);
void winqdelete(struct ScrArea *sa);
void winqclear(struct ScrArea *sa);
void addqueue(short win, unsigned short event, short val);
void addafterqueue(short win, unsigned short event, short val);
short ext_qtest(void);
unsigned short extern_qread(short *val);
unsigned short extern_qread_ext(short *val, char *ascii);

extern void markdirty_all(void); // also sets redraw event
extern void markdirty_all_back(void);
extern void markdirty_win_back(short winid);
	
void screen_swapbuffers(void);
void set_debug_swapbuffers_ovveride(struct bScreen *sc, int mode);
int is_allowed_to_change_screen(struct bScreen *newp);
void splash(void * data, int datasizei, char * string);
void screen_delayed_undo_push(char *name);
void screenmain(void);
void getdisplaysize(void);
void setprefsize(int stax, int stay, int sizx, int sizy, int maximized);
void calc_arearcts(struct ScrArea *sa);
void resize_screen(int x, int y, int w, int h);
struct ScrArea *closest_bigger_area(void);
int mywinopen(int mode, short posx, short posy, short sizex, short sizey);
void setscreen(struct bScreen *sc);
void area_fullscreen(void);
int select_area(int spacetype);
struct bScreen *default_twosplit(void);
void initscreen(void);
void unlink_screen(struct bScreen *sc);
void reset_autosave(void);
int area_is_active_area(struct ScrArea *area);
void draw_area_emboss(struct ScrArea *sa);

void headerprint(char *str);

/* ******* handlers ****** */
void add_screenhandler(struct bScreen *sc, short eventcode, short val);
void rem_screenhandler(struct bScreen *sc, short eventcode);
int do_screenhandlers(struct bScreen *sc);
int has_screenhandler(struct bScreen *sc, short eventcode);

	/***/

/* return all layers of all 3d windows in a screen */
unsigned int screen_view3d_layers(void);

#ifdef __cplusplus
}
#endif

#endif /* BIF_SCREEN_H */

