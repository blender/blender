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
 * 
 */

#ifndef BSE_FILESEL_H
#define BSE_FILESEL_H

struct SpaceFile;
struct direntry;
struct ID;
struct ScrArea;
struct BWinEvent;

void filesel_statistics(struct SpaceFile *sfile, int *totfile, int *selfile, float *totlen, float *sellen);
void test_flags_file(struct SpaceFile *sfile);
void sort_filelist(struct SpaceFile *sfile);
void read_dir(struct SpaceFile *sfile);
void freefilelist(struct SpaceFile *sfile);
void parent(struct SpaceFile *sfile);
void swapselect_file(struct SpaceFile *sfile);
void drawfilespace(struct ScrArea *sa, void *spacedata);

void activate_fileselect(int type, char *title, char *file, void (*func)(char *));
void activate_fileselect_menu(int type, char *title, char *file, char *pupmenu, short *menup, void (*func)(char *));
void activate_fileselect_args(int type, char *title, char *file, void (*func)(char *, void *, void *), void *arg1, void *arg2);

void activate_databrowse(struct ID *id, int idcode, int fromcode, int retval, short *menup, void (*func)(unsigned short));
void activate_databrowse_args(struct ID *id, int idcode, int fromcode, short *menup, void (*func)(char *, void *, void *), void *arg1, void *arg2);

void filesel_prevspace(void);
void reset_filespace(struct ScrArea *sa);
void free_filesel_spec(char *dir);
void winqreadfilespace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);
void main_to_filelist(struct SpaceFile *sfile);   

void clever_numbuts_filesel(void);
#endif

