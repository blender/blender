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

#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include "RNA_types.h"

/* Struct Declarations */

struct ID;
struct Main;
struct ListBase;
struct ARegion;
struct ScrArea;
struct wmWindow;
struct wmWindowManager;
struct wmOperator;
struct AutoComplete;
struct bContext;
struct Panel;
struct PanelType;
struct PointerRNA;
struct PropertyRNA;
struct ReportList;
struct rcti;
struct rctf;
struct uiStyle;
struct uiFontStyle;
struct uiWidgetColors;
struct ColorBand;
struct CurveMapping;
struct Image;
struct ImageUser;
struct uiWidgetColors;
struct Tex;
struct MTex;

typedef struct uiBut uiBut;
typedef struct uiBlock uiBlock;
typedef struct uiPopupBlockHandle uiPopupBlockHandle;
typedef struct uiLayout uiLayout;

/* Defines */

/* uiBlock->dt */
#define UI_EMBOSS		0	/* use widget style for drawing */
#define UI_EMBOSSN		1	/* Nothing, only icon and/or text */
#define UI_EMBOSSP		2	/* Pulldown menu style */
#define UI_EMBOSST		3	/* Table */

/* uiBlock->direction */
#define UI_TOP		1
#define UI_DOWN		2
#define UI_LEFT		4
#define UI_RIGHT	8
#define UI_DIRECTION	15
#define UI_CENTER		16
#define UI_SHIFT_FLIPPED	32

/* uiBlock->autofill (not yet used) */
#define UI_BLOCK_COLLUMNS	1
#define UI_BLOCK_ROWS		2

/* uiBlock->flag (controls) */
#define UI_BLOCK_LOOP			1
#define UI_BLOCK_REDRAW			2
#define UI_BLOCK_RET_1			4		/* XXX 2.5 not implemented */
#define UI_BLOCK_NUMSELECT		8
#define UI_BLOCK_ENTER_OK		16
#define UI_BLOCK_NOSHADOW		32
#define UI_BLOCK_UNUSED			64
#define UI_BLOCK_MOVEMOUSE_QUIT	128
#define UI_BLOCK_KEEP_OPEN		256
#define UI_BLOCK_POPUP			512
#define UI_BLOCK_OUT_1			1024
#define UI_BLOCK_NO_FLIP		2048
#define UI_BLOCK_POPUP_MEMORY	4096

/* uiPopupBlockHandle->menuretval */
#define UI_RETURN_CANCEL	1       /* cancel all menus cascading */
#define UI_RETURN_OK        2       /* choice made */
#define UI_RETURN_OUT       4       /* left the menu */
#define UI_RETURN_UPDATE    8       /* update the button that opened */

	/* block->flag bits 12-15 are identical to but->flag bits */

/* panel controls */
#define UI_PNL_TRANSP	1
#define UI_PNL_SOLID	2

#define UI_PNL_CLOSE	32
#define UI_PNL_STOW		64
#define UI_PNL_TO_MOUSE	128
#define UI_PNL_UNSTOW	256
#define UI_PNL_SCALE	512

/* warning the first 6 flags are internal */
/* but->flag */
#define UI_TEXT_LEFT	64
#define UI_ICON_LEFT	128
#define UI_ICON_SUBMENU	256
	/* control for button type block */
#define UI_MAKE_TOP		512
#define UI_MAKE_DOWN	1024
#define UI_MAKE_LEFT	2048
#define UI_MAKE_RIGHT	4096

	/* button align flag, for drawing groups together */
#define UI_BUT_ALIGN		(15<<14)
#define UI_BUT_ALIGN_TOP	(1<<14)
#define UI_BUT_ALIGN_LEFT	(1<<15)
#define UI_BUT_ALIGN_RIGHT	(1<<16)
#define UI_BUT_ALIGN_DOWN	(1<<17)

#define UI_BUT_DISABLED		(1<<18)
#define UI_BUT_UNUSED		(1<<19)
#define UI_BUT_ANIMATED		(1<<20)
#define UI_BUT_ANIMATED_KEY	(1<<21)
#define UI_BUT_DRIVEN		(1<<22)
#define UI_BUT_INACTIVE		(1<<23)
#define UI_BUT_LAST_ACTIVE	(1<<24)
#define UI_BUT_UNDO			(1<<25)
#define UI_BUT_IMMEDIATE	(1<<26)
#define UI_BUT_NO_TOOLTIP	(1<<27)

#define UI_PANEL_WIDTH			340
#define UI_COMPACT_PANEL_WIDTH	160

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
#define HSVCUBE		(26<<9)
#define PULLDOWN	(27<<9)
#define ROUNDBOX	(28<<9)
#define CHARTAB		(29<<9)
#define BUT_COLORBAND (30<<9)
#define BUT_NORMAL	(31<<9)
#define BUT_CURVE	(32<<9)
#define BUT_TOGDUAL (33<<9)
#define ICONTOGN	(34<<9)
#define FTPREVIEW	(35<<9)
#define NUMABS		(36<<9)
#define TOGBUT		(37<<9)
#define OPTION		(38<<9)
#define OPTIONN		(39<<9)
#define SEARCH_MENU	(40<<9)
#define BUT_EXTRA	(41<<9)
#define HSVCIRCLE	(42<<9)
#define LISTBOX		(43<<9)
#define LISTROW		(44<<9)
#define HOTKEYEVT	(45<<9)
#define BUT_IMAGE	(46<<9)

#define BUTTYPE		(63<<9)

/* Drawing
 *
 * Functions to draw various shapes, taking theme settings into account.
 * Used for code that draws its own UI style elements. */

void uiEmboss(float x1, float y1, float x2, float y2, int sel);
void uiRoundBox(float minx, float miny, float maxx, float maxy, float rad);
void uiSetRoundBox(int type);
int uiGetRoundBox(void);
void uiRoundRect(float minx, float miny, float maxx, float maxy, float rad);
void uiDrawMenuBox(float minx, float miny, float maxx, float maxy, short flag, short direction);
void uiDrawBoxShadow(unsigned char alpha, float minx, float miny, float maxx, float maxy);

/* state for scrolldrawing */
#define UI_SCROLL_PRESSED	1
#define UI_SCROLL_ARROWS	2
void uiWidgetScrollDraw(struct uiWidgetColors *wcol, struct rcti *rect, struct rcti *slider, int state);

/* Menu Callbacks */

typedef void (*uiMenuCreateFunc)(struct bContext *C, struct uiLayout *layout, void *arg1);
typedef void (*uiMenuHandleFunc)(struct bContext *C, void *arg, int event);

/* Popup Menus
 *
 * Functions used to create popup menus. For more extended menus the
 * uiPupMenuBegin/End functions can be used to define own items with
 * the uiItem functions inbetween. If it is a simple confirmation menu
 * or similar, popups can be created with a single function call. */

typedef struct uiPopupMenu uiPopupMenu;

uiPopupMenu *uiPupMenuBegin(struct bContext *C, const char *title, int icon);
void uiPupMenuEnd(struct bContext *C, struct uiPopupMenu *head);
struct uiLayout *uiPupMenuLayout(uiPopupMenu *head);

void uiPupMenuOkee(struct bContext *C, char *opname, char *str, ...);
void uiPupMenuSaveOver(struct bContext *C, struct wmOperator *op, char *filename);
void uiPupMenuNotice(struct bContext *C, char *str, ...);
void uiPupMenuError(struct bContext *C, char *str, ...);
void uiPupMenuReports(struct bContext *C, struct ReportList *reports);
void uiPupMenuInvoke(struct bContext *C, const char *idname); /* popup registered menu */

/* Popup Blocks
 *
 * Functions used to create popup blocks. These are like popup menus
 * but allow using all button types and creating an own layout. */

typedef uiBlock* (*uiBlockCreateFunc)(struct bContext *C, struct ARegion *ar, void *arg1);

void uiPupBlock(struct bContext *C, uiBlockCreateFunc func, void *arg);
void uiPupBlockO(struct bContext *C, uiBlockCreateFunc func, void *arg, char *opname, int opcontext);
void uiPupBlockOperator(struct bContext *C, uiBlockCreateFunc func, struct wmOperator *op, int opcontext);

void uiPupBlockClose(struct bContext *C, uiBlock *block);

/* Blocks
 *
 * Functions for creating, drawing and freeing blocks. A Block is a
 * container of buttons and used for various purposes.
 * 
 * Begin/Define Buttons/End/Draw is the typical order in which these
 * function should be called, though for popup blocks Draw is left out.
 * Freeing blocks is done by the screen/ module automatically.
 *
 * */

uiBlock *uiBeginBlock(const struct bContext *C, struct ARegion *region, const char *name, short dt);
void uiEndBlock(const struct bContext *C, uiBlock *block);
void uiDrawBlock(const struct bContext *C, struct uiBlock *block);

uiBlock *uiGetBlock(char *name, struct ARegion *ar);

void uiBlockSetEmboss(uiBlock *block, short dt);

void uiFreeBlock(const struct bContext *C, uiBlock *block);
void uiFreeBlocks(const struct bContext *C, struct ListBase *lb);
void uiFreeInactiveBlocks(const struct bContext *C, struct ListBase *lb);

void uiBlockSetRegion(uiBlock *block, struct ARegion *region);

void uiBlockSetButLock(uiBlock *block, int val, char *lockstr);
void uiBlockClearButLock(uiBlock *block);

/* automatic aligning, horiz or verical */
void uiBlockBeginAlign(uiBlock *block);
void uiBlockEndAlign(uiBlock *block);

/* block bounds/position calculation */
enum {
	UI_BLOCK_BOUNDS=1,
	UI_BLOCK_BOUNDS_TEXT,
	UI_BLOCK_BOUNDS_POPUP_MOUSE,
	UI_BLOCK_BOUNDS_POPUP_MENU,
	UI_BLOCK_BOUNDS_POPUP_CENTER,
} eBlockBoundsCalc;

void uiBoundsBlock(struct uiBlock *block, int addval);
void uiTextBoundsBlock(uiBlock *block, int addval);
void uiPopupBoundsBlock(uiBlock *block, int addval, int mx, int my);
void uiMenuPopupBoundsBlock(uiBlock *block, int addvall, int mx, int my);
void uiCenteredBoundsBlock(uiBlock *block, int addval);

int		uiBlocksGetYMin		(struct ListBase *lb);

void	uiBlockSetDirection	(uiBlock *block, int direction);
void 	uiBlockFlipOrder	(uiBlock *block);
void	uiBlockSetFlag		(uiBlock *block, int flag);
void	uiBlockClearFlag	(uiBlock *block, int flag);
void	uiBlockSetXOfs		(uiBlock *block, int xofs);

int		uiButGetRetVal		(uiBut *but);

void	uiButSetFlag		(uiBut *but, int flag);
void	uiButClearFlag		(uiBut *but, int flag);

/* special button case, only draw it when used actively, for outliner etc */
int		uiButActiveOnly		(const struct bContext *C, uiBlock *block, uiBut *but);


/* Buttons
 *
 * Functions to define various types of buttons in a block. Postfixes:
 * - F: float
 * - I: int
 * - S: short
 * - C: char
 * - R: RNA
 * - O: operator */

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
uiBut *uiDefButR(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, struct PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefButO(uiBlock *block, int type, char *opname, int opcontext, char *str, short x1, short y1, short x2, short y2, char *tip);

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
uiBut *uiDefIconButR(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, struct PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconButO(uiBlock *block, int type, char *opname, int opcontext, int icon, short x1, short y1, short x2, short y2, char *tip);

uiBut *uiDefIconTextBut(uiBlock *block,
						int type, int retval, int icon, char *str, 
						short x1, short y1,
						short x2, short y2,
						void *poin,
						float min, float max,
						float a1, float a2,  char *tip);
uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButBitF(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButBitI(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButBitS(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButBitC(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButR(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, struct PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  char *tip);
uiBut *uiDefIconTextButO(uiBlock *block, int type, char *opname, int opcontext, int icon, char *str, short x1, short y1, short x2, short y2, char *tip);

/* for passing inputs to ButO buttons */
struct PointerRNA *uiButGetOperatorPtrRNA(uiBut *but);

/* Special Buttons
 *
 * Butons with a more specific purpose:
 * - IDPoinBut: for creating buttons that work on a pointer to an ID block.
 * - MenuBut: buttons that popup a menu (in headers usually).
 * - PulldownBut: like MenuBut, but creating a uiBlock (for compatibility).
 * - BlockBut: buttons that popup a block with more buttons.
 * - KeyevtBut: buttons that can be used to turn key events into values.
 * - PickerButtons: buttons like the color picker (for code sharing).
 * - AutoButR: RNA property button with type automatically defined. */

#define UI_ID_RENAME		1
#define UI_ID_BROWSE		2
#define UI_ID_ADD_NEW		4
#define UI_ID_OPEN			8
#define UI_ID_ALONE			16
#define UI_ID_DELETE		32
#define UI_ID_LOCAL			64
#define UI_ID_AUTO_NAME		128
#define UI_ID_FAKE_USER		256
#define UI_ID_PIN			512
#define UI_ID_BROWSE_RENDER	1024
#define UI_ID_FULL			(UI_ID_RENAME|UI_ID_BROWSE|UI_ID_ADD_NEW|UI_ID_OPEN|UI_ID_ALONE|UI_ID_DELETE|UI_ID_LOCAL)

typedef void (*uiIDPoinFuncFP)(struct bContext *C, char *str, struct ID **idpp);
typedef void (*uiIDPoinFunc)(struct bContext *C, struct ID *id, int event);

uiBut *uiDefIDPoinBut(uiBlock *block, uiIDPoinFuncFP func, short blocktype, int retval, char *str,
						short x1, short y1, short x2, short y2, void *idpp, char *tip);

int uiIconFromID(struct ID *id);

uiBut *uiDefPulldownBut(uiBlock *block, uiBlockCreateFunc func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip);
uiBut *uiDefMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip);
uiBut *uiDefIconTextMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, int icon, char *str, short x1, short y1, short x2, short y2, char *tip);
uiBut *uiDefIconMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, int icon, short x1, short y1, short x2, short y2, char *tip);

uiBut *uiDefBlockBut(uiBlock *block, uiBlockCreateFunc func, void *func_arg1, char *str, short x1, short y1, short x2, short y2, char *tip);
uiBut *uiDefBlockButN(uiBlock *block, uiBlockCreateFunc func, void *argN, char *str, short x1, short y1, short x2, short y2, char *tip);

uiBut *uiDefIconBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, int retval, int icon, short x1, short y1, short x2, short y2, char *tip);
uiBut *uiDefIconTextBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, int icon, char *str, short x1, short y1, short x2, short y2, char *tip);

uiBut *uiDefKeyevtButS(uiBlock *block, int retval, char *str, short x1, short y1, short x2, short y2, short *spoin, char *tip);
uiBut *uiDefHotKeyevtButS(uiBlock *block, int retval, char *str, short x1, short y1, short x2, short y2, short *keypoin, short *modkeypoin, char *tip);

uiBut *uiDefSearchBut(uiBlock *block, void *arg, int retval, int icon, int maxlen, short x1, short y1, short x2, short y2, char *tip);

void uiBlockPickerButtons(struct uiBlock *block, float *col, float *hsv, float *old, char *hexcol, char mode, short retval);
void uiBlockColorbandButtons(struct uiBlock *block, struct ColorBand *coba, struct rctf *butr, int event);

uiBut *uiDefAutoButR(uiBlock *block, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, char *name, int icon, int x1, int y1, int x2, int y2);
void uiDefAutoButsRNA(const struct bContext *C, uiLayout *layout, struct PointerRNA *ptr, int columns);

/* Links
 *
 * Game engine logic brick links. Non-functional currently in 2.5,
 * code to handle and draw these is disabled internally. */

void uiSetButLink(struct uiBut *but,  void **poin,  void ***ppoin,  short *tot,  int from, int to);

void uiComposeLinks(uiBlock *block);
uiBut *uiFindInlink(uiBlock *block, void *poin);

/* Callbacks
 *
 * uiBlockSetHandleFunc/ButmFunc are for handling events through a callback.
 * HandleFunc gets the retval passed on, and ButmFunc gets a2. The latter is
 * mostly for compatibility with older code.
 *
 * uiButSetCompleteFunc is for tab completion.
 *
 * uiButSearchFunc is for name buttons, showing a popup with matches
 *
 * uiBlockSetFunc and uiButSetFunc are callbacks run when a button is used,
 * in case events, operators or RNA are not sufficient to handle the button.
 *
 * uiButSetNFunc will free the argument with MEM_freeN. */

typedef struct uiSearchItems uiSearchItems;

typedef void (*uiButHandleFunc)(struct bContext *C, void *arg1, void *arg2);
typedef void (*uiButHandleRenameFunc)(struct bContext *C, void *arg, char *origstr);
typedef void (*uiButHandleNFunc)(struct bContext *C, void *argN, void *arg2);
typedef void (*uiButCompleteFunc)(struct bContext *C, char *str, void *arg);
typedef void (*uiButSearchFunc)(const struct bContext *C, void *arg, char *str, uiSearchItems *items);
typedef void (*uiBlockHandleFunc)(struct bContext *C, void *arg, int event);
		
		/* use inside searchfunc to add items */
int		uiSearchItemAdd(uiSearchItems *items, const char *name, void *poin, int iconid);
		/* bfunc gets search item *poin as arg2, or if NULL the old string */
void	uiButSetSearchFunc	(uiBut *but,		uiButSearchFunc sfunc, void *arg1, uiButHandleFunc bfunc, void *active);
		/* height in pixels, it's using hardcoded values still */
int		uiSearchBoxhHeight(void);

void	uiBlockSetHandleFunc(uiBlock *block,	uiBlockHandleFunc func, void *arg);
void	uiBlockSetButmFunc	(uiBlock *block,	uiMenuHandleFunc func, void *arg);
void	uiBlockSetFunc		(uiBlock *block,	uiButHandleFunc func, void *arg1, void *arg2);
void	uiBlockSetNFunc		(uiBlock *block,	uiButHandleFunc func, void *argN, void *arg2);

void	uiButSetRenameFunc	(uiBut *but,		uiButHandleRenameFunc func, void *arg1);
void	uiButSetFunc		(uiBut *but,		uiButHandleFunc func, void *arg1, void *arg2);
void	uiButSetNFunc		(uiBut *but,		uiButHandleNFunc func, void *argN, void *arg2);

void	uiButSetCompleteFunc(uiBut *but,		uiButCompleteFunc func, void *arg);

void 	uiBlockSetDrawExtraFunc(uiBlock *block, void (*func)(const struct bContext *C, void *, void *, void *, struct rcti *rect), void *arg1, void *arg2);

/* Autocomplete
 *
 * Tab complete helper functions, for use in uiButCompleteFunc callbacks.
 * Call begin once, then multiple times do_name with all possibilities,
 * and finally end to finish and get the completed name. */

typedef struct AutoComplete AutoComplete;

AutoComplete *autocomplete_begin(char *startname, int maxlen);
void autocomplete_do_name(AutoComplete *autocpl, const char *name);
void autocomplete_end(AutoComplete *autocpl, char *autoname);

/* Panels
 *
 * Functions for creating, freeing and drawing panels. The API here
 * could use a good cleanup, though how they will function in 2.5 is
 * not clear yet so we postpone that. */

void uiBeginPanels(const struct bContext *C, struct ARegion *ar);
void uiEndPanels(const struct bContext *C, struct ARegion *ar);

struct Panel *uiBeginPanel(struct ScrArea *sa, struct ARegion *ar, uiBlock *block, struct PanelType *pt, int *open);
void uiEndPanel(uiBlock *block, int width, int height);

/* Handlers
 *
 * Handlers that can be registered in regions, areas and windows for
 * handling WM events. Mostly this is done automatic by modules such
 * as screen/ if ED_KEYMAP_UI is set, or internally in popup functions. */

void UI_add_region_handlers(struct ListBase *handlers);
void UI_add_area_handlers(struct ListBase *handlers);
void UI_add_popup_handlers(struct bContext *C, struct ListBase *handlers, uiPopupBlockHandle *popup);
void UI_remove_popup_handlers(struct ListBase *handlers, uiPopupBlockHandle *popup);

/* Module
 *
 * init and exit should be called before using this module. init_userdef must
 * be used to reinitialize some internal state if user preferences change. */

void UI_init(void);
void UI_init_userdef(void);
void UI_exit(void);

/* Layout
 *
 * More automated layout of buttons. Has three levels:
 * - Layout: contains a number templates, within a bounded width or height.
 * - Template: predefined layouts for buttons with a number of slots, each
 *   slot can contain multiple items.
 * - Item: item to put in a template slot, being either an RNA property,
 *   operator, label or menu. Also regular buttons can be used when setting
 *   uiBlockCurLayout. */

/* layout */
#define UI_LAYOUT_HORIZONTAL	0
#define UI_LAYOUT_VERTICAL		1

#define UI_LAYOUT_PANEL			0
#define UI_LAYOUT_HEADER		1
#define UI_LAYOUT_MENU			2
#define UI_LAYOUT_TOOLBAR		3
 
#define UI_UNIT_X				20
#define UI_UNIT_Y				20

#define UI_LAYOUT_ALIGN_EXPAND	0
#define UI_LAYOUT_ALIGN_LEFT	1
#define UI_LAYOUT_ALIGN_CENTER	2
#define UI_LAYOUT_ALIGN_RIGHT	3

#define UI_ITEM_O_RETURN_PROPS	1
#define UI_ITEM_R_EXPAND		2
#define UI_ITEM_R_SLIDER		4
#define UI_ITEM_R_TOGGLE		8
#define UI_ITEM_R_ICON_ONLY		16
#define UI_ITEM_R_EVENT			32
#define UI_ITEM_R_FULL_EVENT	64

uiLayout *uiBlockLayout(uiBlock *block, int dir, int type, int x, int y, int size, int em, struct uiStyle *style);
void uiBlockSetCurLayout(uiBlock *block, uiLayout *layout);
void uiBlockLayoutResolve(uiBlock *block, int *x, int *y);

uiBlock *uiLayoutGetBlock(uiLayout *layout);

void uiLayoutSetFunc(uiLayout *layout, uiMenuHandleFunc handlefunc, void *argv);
void uiLayoutSetContextPointer(uiLayout *layout, char *name, struct PointerRNA *ptr);
char *uiLayoutIntrospect(uiLayout *layout); // XXX - testing

void uiLayoutSetOperatorContext(uiLayout *layout, int opcontext);
void uiLayoutSetActive(uiLayout *layout, int active);
void uiLayoutSetEnabled(uiLayout *layout, int enabled);
void uiLayoutSetRedAlert(uiLayout *layout, int redalert);
void uiLayoutSetAlignment(uiLayout *layout, int alignment);
void uiLayoutSetKeepAspect(uiLayout *layout, int keepaspect);
void uiLayoutSetScaleX(uiLayout *layout, float scale);
void uiLayoutSetScaleY(uiLayout *layout, float scale);

int uiLayoutGetOperatorContext(uiLayout *layout);
int uiLayoutGetActive(uiLayout *layout);
int uiLayoutGetEnabled(uiLayout *layout);
int uiLayoutGetRedAlert(uiLayout *layout);
int uiLayoutGetAlignment(uiLayout *layout);
int uiLayoutGetKeepAspect(uiLayout *layout);
int uiLayoutGetWidth(uiLayout *layout);
float uiLayoutGetScaleX(uiLayout *layout);
float uiLayoutGetScaleY(uiLayout *layout);

/* layout specifiers */
uiLayout *uiLayoutRow(uiLayout *layout, int align);
uiLayout *uiLayoutColumn(uiLayout *layout, int align);
uiLayout *uiLayoutColumnFlow(uiLayout *layout, int number, int align);
uiLayout *uiLayoutBox(uiLayout *layout);
uiLayout *uiLayoutListBox(uiLayout *layout, struct PointerRNA *ptr, struct PropertyRNA *prop,
	struct PointerRNA *actptr, struct PropertyRNA *actprop);
uiLayout *uiLayoutAbsolute(uiLayout *layout, int align);
uiLayout *uiLayoutSplit(uiLayout *layout, float percentage, int align);
uiLayout *uiLayoutOverlap(uiLayout *layout);

uiBlock *uiLayoutAbsoluteBlock(uiLayout *layout);

/* templates */
void uiTemplateHeader(uiLayout *layout, struct bContext *C, int menus);
void uiTemplateDopeSheetFilter(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr);
void uiTemplateID(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname,
	char *newop, char *openop, char *unlinkop);
void uiTemplateAnyID(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, 
	char *proptypename, char *text);
void uiTemplatePathBuilder(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, 
	struct PointerRNA *root_ptr, char *text);
uiLayout *uiTemplateModifier(uiLayout *layout, struct PointerRNA *ptr);
uiLayout *uiTemplateConstraint(uiLayout *layout, struct PointerRNA *ptr);
void uiTemplatePreview(uiLayout *layout, struct ID *id, struct ID *parent, struct MTex *slot);
void uiTemplateColorRamp(uiLayout *layout, struct PointerRNA *ptr, char *propname, int expand);
void uiTemplateCurveMapping(uiLayout *layout, struct PointerRNA *ptr, char *propname, int type, int levels);
void uiTemplateTriColorSet(uiLayout *layout, struct PointerRNA *ptr, char *propname);
void uiTemplateLayers(uiLayout *layout, struct PointerRNA *ptr, char *propname,
		      PointerRNA *used_ptr, char *used_propname, int active_layer);
void uiTemplateImage(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *userptr, int compact);
void uiTemplateImageLayers(uiLayout *layout, struct bContext *C, struct Image *ima, struct ImageUser *iuser);
void uiTemplateRunningJobs(uiLayout *layout, struct bContext *C);
void uiTemplateOperatorSearch(uiLayout *layout);
void uiTemplateHeader3D(uiLayout *layout, struct bContext *C);
void uiTemplateTextureImage(uiLayout *layout, struct bContext *C, struct Tex *tex);

void uiTemplateList(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *activeptr, char *activeprop, int rows, int type);

/* items */
void uiItemO(uiLayout *layout, char *name, int icon, char *opname);
void uiItemEnumO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value);
void uiItemEnumO_string(uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value);
void uiItemsEnumO(uiLayout *layout, char *opname, char *propname);
void uiItemBooleanO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value);
void uiItemIntO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value);
void uiItemFloatO(uiLayout *layout, char *name, int icon, char *opname, char *propname, float value);
void uiItemStringO(uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value);
PointerRNA uiItemFullO(uiLayout *layout, char *name, int icon, char *idname, struct IDProperty *properties, int context, int flag);

void uiItemR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname, int flag);
void uiItemFullR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, int value, int flag);
void uiItemEnumR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname, int value);
void uiItemEnumR_string(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname, char *value);
void uiItemsEnumR(uiLayout *layout, struct PointerRNA *ptr, char *propname);
void uiItemPointerR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname, struct PointerRNA *searchptr, char *searchpropname);

void uiItemL(uiLayout *layout, char *name, int icon); /* label */
void uiItemM(uiLayout *layout, struct bContext *C, char *name, int icon, char *menuname); /* menu */
void uiItemV(uiLayout *layout, char *name, int icon, int argval); /* value */
void uiItemS(uiLayout *layout); /* separator */

void uiItemMenuF(uiLayout *layout, char *name, int icon, uiMenuCreateFunc func, void *arg);
void uiItemMenuEnumO(uiLayout *layout, char *name, int icon, char *opname, char *propname);
void uiItemMenuEnumR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname);

/* Helpers for Operators */
void uiAnimContextProperty(const struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop, int *index);
void uiFileBrowseContextProperty(const struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop);
void uiIDContextProperty(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop);

/* Styled text draw */
void uiStyleFontSet(struct uiFontStyle *fs);
void uiStyleFontDraw(struct uiFontStyle *fs, struct rcti *rect, char *str);
void uiStyleFontDrawRotated(struct uiFontStyle *fs, struct rcti *rect, char *str);

int UI_GetStringWidth(char *str); // XXX temp
void UI_DrawString(float x, float y, char *str); // XXX temp

#endif /*  UI_INTERFACE_H */

