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

/* uiBlock->dt */
#define UI_EMBOSSX		0	/* Rounded embossed button */
#define UI_EMBOSSW		1	/* Flat bordered button */
#define UI_EMBOSSN		2	/* No border */
#define UI_EMBOSSF		3	/* Square embossed button */
#define UI_EMBOSSM		4	/* Colored Border */
#define UI_EMBOSSP		5	/* Borderless coloured button */
#define UI_EMBOSSA		6	/* same as EMBOSSX but with arrows to simulate */
#define UI_EMBOSSTABL	7
#define UI_EMBOSSTABM	8
#define UI_EMBOSSTABR	9
#define UI_EMBOSST		10
#define UI_EMBOSSMB		11	/* emboss menu button */

/* uiBlock->direction */
#define UI_TOP		0
#define UI_DOWN		1
#define UI_LEFT		2
#define UI_RIGHT	3

/* uiBlock->autofill */
#define UI_BLOCK_COLLUMNS	1
#define UI_BLOCK_ROWS		2

/* return from uiDoBlock */
#define UI_CONT				0
#define UI_NOTHING			1
#define UI_RETURN_CANCEL	2
#define UI_RETURN_OK		4
#define UI_RETURN_OUT		8
#define UI_RETURN			14
#define UI_EXIT_LOOP		16

/* uiBlock->flag (controls) */
#define UI_BLOCK_LOOP		1
#define UI_BLOCK_REDRAW		2
#define UI_BLOCK_RET_1		4
#define UI_BLOCK_BUSY		8
#define UI_BLOCK_NUMSELECT	16
#define UI_BLOCK_ENTER_OK	32

/* block->font, for now: bold = medium+1 */
#define UI_HELV			0
#define UI_HELVB		1

/* panel controls */
#define UI_PNL_TRANSP	1
#define UI_PNL_SOLID	2

#define UI_PNL_CLOSE	32
#define UI_PNL_STOW		64
#define UI_PNL_TO_MOUSE	128


/* definitions for icons (and their alignment) in buttons */
/* warning the first 4 flags are internal */
#define UI_TEXT_LEFT	16
#define UI_ICON_LEFT	32
#define UI_ICON_RIGHT	64


/* Button types */
#define CHA	32
#define SHO	64
#define INT	96
#define FLO	128
#define FUN	192
#define BIT	256

#define BUTPOIN	(128+64+32)

#define BUT	(1<<9)
#define ROW	(2<<9)
#define TOG	(3<<9)
#define SLI	(4<<9)
#define	NUM	(5<<9)
#define TEX	(6<<9)
#define TOG3	(7<<9)
#define TOGR	(8<<9)
#define TOGN	(9<<9)
#define LABEL	(10<<9)
#define MENU	(11<<9)
#define ICONROW	(12<<9)
#define ICONTOG	(13<<9)
#define NUMSLI	(14<<9)
#define COL		(15<<9)
#define IDPOIN	(16<<9)
#define HSVSLI 	(17<<9)
#define SCROLL	(18<<9)
#define BLOCK	(19<<9)
#define BUTM	(20<<9)
#define SEPR	(21<<9)
#define LINK	(22<<9)
#define INLINK	(23<<9)
#define KEYEVT	(24<<9)
#define ICONTEXTROW (25<<9)

#define BUTTYPE	(31<<9)



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
void 	uiBlockSetDrawExtraFunc(uiBlock *block, void (*func)());

short pupmenu(char *instr); 
short pupmenu_col(char *instr, int maxrow);

extern void uiFreePanels(struct ListBase *lb);
extern void uiNewPanelTabbed(char *, char *);
extern int uiNewPanel(struct ScrArea *sa, struct uiBlock *block, char *panelname, char *tabname, int ofsx, int ofsy, int sizex, int sizey);
	
extern void uiSetPanel_view2d(struct ScrArea *sa);
extern void uiMatchPanel_view2d(struct ScrArea *sa);

extern void uiDrawBlocksPanels(struct ScrArea *sa, int re_align);
extern void uiNewPanelHeight(struct uiBlock *block, int sizey);
void uiPanelPush(uiBlock *block);
void uiPanelPop(uiBlock *block);
extern uiBlock *uiFindOpenPanelBlockName(ListBase *lb, char *name);
extern int uiAlignPanelStep(struct ScrArea *sa, float fac);
extern void uiPanelControl(int);
extern void uiSetPanelHandler(int);

#endif /*  BIF_INTERFACE_H */

