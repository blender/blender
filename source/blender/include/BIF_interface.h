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

#ifndef BIF_INTERFACE_H
#define BIF_INTERFACE_H

struct ID;
struct ListBase;
struct ScrArea;

typedef struct uiBut uiBut;
typedef struct uiBlock uiBlock;

void uiEmboss(float x1, float y1, float x2, float y2, int sel);
void uiRoundBoxEmboss(float minx, float miny, float maxx, float maxy, float rad);
void uiRoundBox(float minx, float miny, float maxx, float maxy, float rad);
void uiSetRoundBox(int type);
void uiRoundRect(float minx, float miny, float maxx, float maxy, float rad);

void uiDrawMenuBox(float minx, float miny, float maxx, float maxy);
void uiTextBoundsBlock(uiBlock *block, int addval);
void uiBoundsBlock(struct uiBlock *block, int addval);
void uiDrawBlock(struct uiBlock *block);
void uiGetMouse(int win, short *adr);
void uiComposeLinks(uiBlock *block);
void uiSetButLock(int val, char *lockstr);
void uiClearButLock(void);
int uiDoBlocks(struct ListBase *lb, int event);
void uiSetCurFont(uiBlock *block, int index);
void uiDefFont(unsigned int index, void *xl, void *large, void *medium, void *small);
void uiFreeBlock(uiBlock *block);
void uiFreeBlocks(struct ListBase *lb);
void uiFreeBlocksWin(struct ListBase *lb, int win);
uiBlock *uiNewBlock(struct ListBase *lb, char *name, short dt, short font, short win);
uiBlock *uiGetBlock(char *name, struct ScrArea *sa);
uiBut *uiDefBut(uiBlock *block, 
					   int type, int retval, char *str, 
					   short x1, short y1, 
					   short x2, short y2, 
					   void *poin, 
					   float min, float max, 
					   float a1, float a2,  char *tip);
uiBut *uiDefButF(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButI(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButS(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButC(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);

uiBut *uiDefIconBut(uiBlock *block, 
					   int type, int retval, int icon, 
					   short x1, short y1, 
					   short x2, short y2, 
					   void *poin, 
					   float min, float max, 
					   float a1, float a2,  char *tip);
uiBut *uiDefIconButF(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButI(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButS(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButC(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);

uiBut *uiDefIconTextBut(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip);

uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);

typedef void		(*uiIDPoinFuncFP)	(char *str, struct ID **idpp);
void uiDefIDPoinBut(struct uiBlock *block,
						uiIDPoinFuncFP func, int retval, char *str,
						short x1, short y1,
						short x2, short y2,
						void *idpp, char *tip);

typedef uiBlock* (*uiBlockFuncFP)	(void *arg1);
uiBut *uiDefBlockBut(uiBlock *block, uiBlockFuncFP func, void *func_arg1, char *str, short x1, short y1, short x2, short y2, char *tip);

uiBut *uiDefIconTextBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, int icon, char *str, short x1, short y1, short x2, short y2, char *tip);

void uiDefKeyevtButS(uiBlock *block, int retval, char *str, short x1, short y1, short x2, short y2, short *spoin, char *tip);

void uiAutoBlock(struct uiBlock *block, 
				 float minx, float miny, 
				 float sizex, float sizey, int flag);
void uiSetButLink(struct uiBut *but, 
				  void **poin, 
				  void ***ppoin, 
				  short *tot, 
				  int from, int to);

int		uiBlocksGetYMin		(ListBase *lb);
int		uiBlockGetCol		(uiBlock *block);
void*	uiBlockGetCurFont	(uiBlock *block);

void	uiBlockSetCol		(uiBlock *block, int col);
void	uiBlockSetEmboss	(uiBlock *block, int emboss);
void	uiBlockSetDirection	(uiBlock *block, int direction);
void 	uiBlockFlipOrder	(uiBlock *block);
void	uiBlockSetFlag		(uiBlock *block, int flag);
void	uiBlockSetXOfs		(uiBlock *block, int xofs);

int		uiButGetRetVal		(uiBut *but);

void	uiButSetFlag		(uiBut *but, int flag);
void	uiButClearFlag		(uiBut *but, int flag);

void	uiBlockSetButmFunc	(uiBlock *block, void (*butmfunc)(void *arg, int but_a2), void *arg);

void	uiBlockSetFunc		(uiBlock *block,	void (*func)(void *arg1, void *arg2), void *arg1, void *arg2);
void	uiButSetFunc		(uiBut *but,		void (*func)(void *arg1, void *arg2), void *arg1, void *arg2);

short pupmenu(char *instr); 
short pupmenu_col(char *instr, int maxrow);

extern void uiFreePanels(struct ListBase *lb);
extern void uiNewPanel(struct ScrArea *sa, struct uiBlock *block, char *panelname, char *tabname, int ofsx, int ofsy, int sizex, int sizey);
extern void uiScalePanelBlock(struct uiBlock *block);
extern int uiIsPanelClosed(struct uiBlock *block);
extern void uiAnimatePanels(struct ScrArea *sa);	
extern void uiSetPanel_view2d(struct ScrArea *sa);

#endif /*  BIF_INTERFACE_H */

