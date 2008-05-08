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

#ifndef BIF_INTERFACE_H
#define BIF_INTERFACE_H

struct ID;
struct ListBase;
struct ScrArea;
struct AutoComplete;

/* uiBlock->dt */
#define UI_EMBOSS		0	/* use one of the themes for drawing */
#define UI_EMBOSSN		1	/* Nothing */
#define UI_EMBOSSM		2	/* Minimal builtin emboss, also for logic buttons */
#define UI_EMBOSSP		3	/* Pulldown */
#define UI_EMBOSSR		4	/* Rounded */

#define UI_EMBOSSX		0	/* for a python file, which i can't change.... duh! */

/* uiBlock->direction */
#define UI_TOP		1
#define UI_DOWN		2
#define UI_LEFT		4
#define UI_RIGHT	8
#define UI_DIRECTION	15
#define UI_CENTER		16
#define UI_SHIFT_FLIPPED	32

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
#define UI_BLOCK_NOSHADOW	64
#define UI_BLOCK_FRONTBUFFER	128
#define UI_BLOCK_NO_HILITE	256

	/* block->flag bits 12-15 are identical to but->flag bits */

/* block->font, for now: bold = medium+1 */
#define UI_HELV			0
#define UI_HELVB		1

/* panel controls */
#define UI_PNL_TRANSP	1
#define UI_PNL_SOLID	2

#define UI_PNL_CLOSE	32
#define UI_PNL_STOW		64
#define UI_PNL_TO_MOUSE	128
#define UI_PNL_UNSTOW	256
#define UI_PNL_SCALE	512

/* warning the first 4 flags are internal */
/* but->flag */
#define UI_TEXT_LEFT	16
#define UI_ICON_LEFT	32
#define UI_ICON_RIGHT	64
	/* control for button type block */
#define UI_MAKE_TOP		128
#define UI_MAKE_DOWN	256
#define UI_MAKE_LEFT	512
#define UI_MAKE_RIGHT	1024
	/* dont draw hilite on mouse over */
#define UI_NO_HILITE	2048
	/* button align flag, for drawing groups together */
#define UI_BUT_ALIGN		(15<<12)
#define UI_BUT_ALIGN_TOP	(1<<12)
#define UI_BUT_ALIGN_LEFT	(1<<13)
#define UI_BUT_ALIGN_RIGHT	(1<<14)
#define UI_BUT_ALIGN_DOWN	(1<<15)


/* Button types, bits stored in 1 value... and a short even!
- bits 0-4:  bitnr (0-31)
- bits 5-7:  pointer type
- bit  8:    for 'bit'
- bit  9-15: button type (now 6 bits, 64 types)
*/

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
#define HSVCUBE (26<<9)
#define PULLDOWN (27<<9)
#define ROUNDBOX (28<<9)
#define CHARTAB (29<<9)
#define BUT_COLORBAND (30<<9)
#define BUT_NORMAL (31<<9)
#define BUT_CURVE (32<<9)
#define BUT_TOGDUAL (33<<9)
#define ICONTOGN (34<<9)
#define FTPREVIEW (35<<9)
#define NUMABS	(36<<9)
#define BUTTYPE	(63<<9)



typedef struct uiBut uiBut;
typedef struct uiBlock uiBlock;

void uiEmboss(float x1, float y1, float x2, float y2, int sel);
void uiRoundBoxEmboss(float minx, float miny, float maxx, float maxy, float rad, int active);
void uiRoundBox(float minx, float miny, float maxx, float maxy, float rad);
void uiSetRoundBox(int type);
void uiRoundRect(float minx, float miny, float maxx, float maxy, float rad);

void uiDrawMenuBox(float minx, float miny, float maxx, float maxy, short flag);
void uiTextBoundsBlock(uiBlock *block, int addval);
void uiBoundsBlock(struct uiBlock *block, int addval);
void uiDrawBlock(struct uiBlock *block);
void uiGetMouse(int win, short *adr);
void uiComposeLinks(uiBlock *block);
void uiSetButLock(int val, char *lockstr);
void uiClearButLock(void);
int uiDoBlocks(struct ListBase *lb, int event, int movemouse_quit);
void uiSetCurFont(uiBlock *block, int index);
void uiDefFont(unsigned int index, void *xl, void *large, void *medium, void *small);
void uiFreeBlock(uiBlock *block);
void uiFreeBlocks(struct ListBase *lb);
void uiFreeBlocksWin(struct ListBase *lb, int win);
uiBlock *uiNewBlock(struct ListBase *lb, char *name, short dt, short font, short win);
uiBlock *uiGetBlock(char *name, struct ScrArea *sa);

void uiBlockPickerButtons(struct uiBlock *block, float *col, float *hsv, float *old, char *hexcol, char mode, short retval);


/* automatic aligning, horiz or verical */
void uiBlockBeginAlign(uiBlock *block);
void uiBlockEndAlign(uiBlock *block);

uiBut *uiDefBut(uiBlock *block, 
					   int type, int retval, char *str, 
					   short x1, short y1, 
					   short x2, short y2, 
					   void *poin, 
					   float min, float max, 
					   float a1, float a2,  char *tip);
uiBut *uiDefButF(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButBitF(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButI(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButBitI(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButS(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButBitS(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButC(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButBitC(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);

uiBut *uiDefIconBut(uiBlock *block, 
					   int type, int retval, int icon, 
					   short x1, short y1, 
					   short x2, short y2, 
					   void *poin, 
					   float min, float max, 
					   float a1, float a2,  char *tip);
uiBut *uiDefIconButF(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButBitF(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButI(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButBitI(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButS(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButBitS(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButC(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButBitC(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);

uiBut *uiDefIconTextBut(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip);

uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButBitF(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButBitI(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButBitS(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButBitC(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);

typedef void		(*uiIDPoinFuncFP)	(char *str, struct ID **idpp);
uiBut *uiDefIDPoinBut(struct uiBlock *block, uiIDPoinFuncFP func, short blocktype, int retval, char *str,
						short x1, short y1, short x2, short y2, void *idpp, char *tip);

typedef uiBlock* (*uiBlockFuncFP)	(void *arg1);
uiBut *uiDefBlockBut(uiBlock *block, uiBlockFuncFP func, void *func_arg1, char *str, short x1, short y1, short x2, short y2, char *tip);
uiBut *uiDefPulldownBut(uiBlock *block, uiBlockFuncFP func, void *func_arg1, char *str, short x1, short y1, short x2, short y2, char *tip);

uiBut *uiDefIconTextBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, int icon, char *str, short x1, short y1, short x2, short y2, char *tip);
uiBut *uiDefIconBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, int retval, int icon, short x1, short y1, short x2, short y2, char *tip);

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

void	uiBlockSetButmFunc	(uiBlock *block,	void (*butmfunc)(void *arg, int but_a2), void *arg);

void	uiBlockSetFunc		(uiBlock *block,	void (*func)(void *arg1, void *arg2), void *arg1, void *arg2);
void	uiButSetFunc		(uiBut *but,		void (*func)(void *arg1, void *arg2), void *arg1, void *arg2);

void	uiButSetCompleteFunc(uiBut *but,		void (*func)(char *str, void *arg), void *arg);

void 	uiBlockSetDrawExtraFunc(uiBlock *block, void (*func)(struct ScrArea *sa, uiBlock *block));


extern void pupmenu_set_active(int val);
extern short pupmenu(char *instr); 
extern short pupmenu_col(char *instr, int maxrow);

extern void uiFreePanels(struct ListBase *lb);
extern void uiNewPanelTabbed(char *, char *);
extern int uiNewPanel(struct ScrArea *sa, struct uiBlock *block, char *panelname, char *tabname, int ofsx, int ofsy, int sizex, int sizey);
	
extern void uiSetPanel_view2d(struct ScrArea *sa);
extern void uiMatchPanel_view2d(struct ScrArea *sa);

extern void uiDrawBlocksPanels(struct ScrArea *sa, int re_align);
extern void uiNewPanelHeight(struct uiBlock *block, int sizey);
extern void uiNewPanelTitle(struct uiBlock *block, char *str);
extern void uiPanelPush(struct uiBlock *block);
extern void uiPanelPop(struct uiBlock *block);
extern uiBlock *uiFindOpenPanelBlockName(ListBase *lb, char *name);
extern int uiAlignPanelStep(struct ScrArea *sa, float fac);
extern void uiPanelControl(int);
extern void uiSetPanelHandler(int);

extern void uiDrawBoxShadow(unsigned char alpha, float minx, float miny, float maxx, float maxy);
extern void *uiSetCurFont_ext(float aspect);

void shade_buttons_change_3d(void);

typedef struct AutoComplete AutoComplete;

AutoComplete *autocomplete_begin(char *startname, int maxlen);
void autocomplete_do_name(AutoComplete *autocpl, const char *name);
void autocomplete_end(AutoComplete *autocpl, char *autoname);

#endif /*  BIF_INTERFACE_H */

