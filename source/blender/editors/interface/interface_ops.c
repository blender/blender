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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "PIL_time.h"

#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "UI_interface.h"
#include "UI_text.h"
#include "interface.h"

#include "WM_api.h"
#include "WM_types.h"

/***************** structs and defines ****************/

#define BUTTON_TOOLTIP_DELAY		500
#define BUTTON_FLASH_DELAY			20
#define BUTTON_AUTO_OPEN_THRESH		0.3
#define BUTTON_MOUSE_TOWARDS_THRESH	1.0

typedef enum uiActivateButState {
	BUTTON_STATE_INIT,
	BUTTON_STATE_HIGHLIGHT,
	BUTTON_STATE_WAIT_FLASH,
	BUTTON_STATE_WAIT_RELEASE,
	BUTTON_STATE_WAIT_KEY_EVENT,
	BUTTON_STATE_NUM_EDITING,
	BUTTON_STATE_TEXT_EDITING,
	BUTTON_STATE_TEXT_SELECTING,
	BUTTON_STATE_BLOCK_OPEN,
	BUTTON_STATE_EXIT
} uiActivateButState;

typedef struct uiActivateBut {
	ARegion *region;
	wmOperator *operator;

	int interactive;

	/* overall state */
	uiActivateButState state;
	int cancel, retval;
	int applied, appliedinteractive;
	wmTimerHandle *flashtimer;

	/* edited value */
	char *str, *origstr;
	double value, origvalue;
	float vec[3], origvec[3];
	int togdual, togonly;
	ColorBand *coba;
	CurveMapping *cumap;

	/* tooltip */
	ARegion *tooltip;
	wmTimerHandle *tooltiptimer;
	wmTimerHandle *autoopentimer;
	int tooltipdisabled;

	/* text selection/editing */
	int maxlen, selextend, selstartx;

	/* number editing / dragging */
	int draglastx, draglasty;
	int dragstartx, dragstarty;
	int dragchange, draglock, dragsel;
	float dragf, dragfstart;
	CBData *dragcbd;

	/* block open */
	uiMenuBlockHandle *blockhandle;
	int blockretval;
} uiActivateBut;

static void button_activate_state(bContext *C, uiBut *but, uiActivateButState state);

/* ********************** button apply/revert ************************ */

static void ui_apply_but_func(uiBut *but)
{
	if (but->func)
		but->func(but->func_arg1, but->func_arg2);
	if(but->func3)
		but->func3(but->func_arg1, but->func_arg2, but->func_arg3);
}

static void ui_apply_but_BUT(uiBut *but, uiActivateBut *data)
{
	ui_apply_but_func(but);

	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_BUTM(uiBut *but, uiActivateBut *data)
{
	ui_set_but_val(but, but->min);
	ui_apply_but_func(but);

	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_BLOCK(uiBut *but, uiActivateBut *data)
{
	if(but->type == COL)
		ui_set_but_vectorf(but, data->vec);
	else if(ELEM3(but->type, MENU, ICONROW, ICONTEXTROW))
		ui_set_but_val(but, data->value);

	ui_check_but(but);
	ui_apply_but_func(but);
	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_TOG(uiBlock *block, uiBut *but, uiActivateBut *data)
{
	double value;
	int w, lvalue, push;
	
	/* local hack... */
	if(but->type==BUT_TOGDUAL && data->togdual) {
		if(but->pointype==SHO)
			but->poin += 2;
		else if(but->pointype==INT)
			but->poin += 4;
	}
	
	value= ui_get_but_val(but);
	lvalue= (int)value;
	
	if(but->bit) {
		w= BTST(lvalue, but->bitnr);
		if(w) lvalue = BCLR(lvalue, but->bitnr);
		else lvalue = BSET(lvalue, but->bitnr);
		
		if(but->type==TOGR) {
			if(!data->togonly) {
				lvalue= 1<<(but->bitnr);
	
				ui_set_but_val(but, (double)lvalue);
			}
			else {
				if(lvalue==0) lvalue= 1<<(but->bitnr);
			}
		}
		
		ui_set_but_val(but, (double)lvalue);
		if(but->type==ICONTOG || but->type==ICONTOGN) ui_check_but(but);
	}
	else {
		
		if(value==0.0) push= 1; 
		else push= 0;
		
		if(but->type==TOGN || but->type==ICONTOGN) push= !push;
		ui_set_but_val(but, (double)push);
		if(but->type==ICONTOG || but->type==ICONTOGN) ui_check_but(but);		
	}
	
	/* end local hack... */
	if(but->type==BUT_TOGDUAL && data->togdual) {
		if(but->pointype==SHO)
			but->poin -= 2;
		else if(but->pointype==INT)
			but->poin -= 4;
	}
	
	ui_apply_but_func(but);

	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_ROW(uiBlock *block, uiBut *but, uiActivateBut *data)
{
	ui_set_but_val(but, but->max);
	ui_apply_but_func(but);

	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_TEX(uiBut *but, uiActivateBut *data)
{
	if(!data->str)
		return;

	ui_set_but_string(but, data->str);
	ui_check_but(but);

	/* give butfunc the original text too */
	/* feature used for bone renaming, channels, etc */
	if(but->func_arg2==NULL) but->func_arg2= data->origstr;
	ui_apply_but_func(but);
	if(but->func_arg2==data->origstr) but->func_arg2= NULL;

	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_NUM(uiBut *but, uiActivateBut *data)
{
	if(data->str) {
		/* XXX 2.50 missing python api */
#if 0
		if(BPY_button_eval(data->str, &data->value)) {
			WM_report(C, WM_LOG_WARNING, "Invalid Python expression, check console");
			data->value = 0.0f; /* Zero out value on error */
			
			if(data->str[0]) {
				data->cancel= 1; /* invalidate return value if eval failed, except when string was null */
				return;
			}
		}
#else
		data->value= atof(data->str);
#endif

		if(!ui_is_but_float(but)) data->value= (int)data->value;
		if(but->type==NUMABS) data->value= fabs(data->value);
		if(data->value<but->min) data->value= but->min;
		if(data->value>but->max) data->value= but->max;
	}

	ui_set_but_val(but, data->value);
	ui_check_but(but);
	ui_apply_but_func(but);

	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_LABEL(uiBut *but, uiActivateBut *data)
{
	ui_apply_but_func(but);
	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_TOG3(uiBut *but, uiActivateBut *data)
{ 
	if(but->pointype==SHO ) {
		short *sp= (short *)but->poin;
		
		if( BTST(sp[1], but->bitnr)) {
			sp[1]= BCLR(sp[1], but->bitnr);
			sp[0]= BCLR(sp[0], but->bitnr);
		}
		else if( BTST(sp[0], but->bitnr)) {
			sp[1]= BSET(sp[1], but->bitnr);
		} else {
			sp[0]= BSET(sp[0], but->bitnr);
		}
	}
	else {
		if( BTST(*(but->poin+2), but->bitnr)) {
			*(but->poin+2)= BCLR(*(but->poin+2), but->bitnr);
			*(but->poin)= BCLR(*(but->poin), but->bitnr);
		}
		else if( BTST(*(but->poin), but->bitnr)) {
			*(but->poin+2)= BSET(*(but->poin+2), but->bitnr);
		} else {
			*(but->poin)= BSET(*(but->poin), but->bitnr);
		}
	}
	
	ui_check_but(but);
	ui_apply_but_func(but);
	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_VEC(uiBut *but, uiActivateBut *data)
{
	ui_set_but_vectorf(but, data->vec);
	ui_check_but(but);
	ui_apply_but_func(but);

	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_COLORBAND(uiBut *but, uiActivateBut *data)
{
	ui_apply_but_func(but);
	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_CURVE(uiBut *but, uiActivateBut *data)
{
	ui_apply_but_func(but);
	data->retval= but->retval;
	data->applied= 1;
}

static void ui_apply_but_IDPOIN(uiBut *but, uiActivateBut *data)
{
	but->idpoin_func(data->str, but->idpoin_idpp);
	ui_check_but(but);
	ui_apply_but_func(but);
	data->retval= but->retval;
	data->applied= 1;
}

#ifdef INTERNATIONAL
static void ui_apply_but_CHARTAB(uiBut *but, uiActivateBut *data)
{
	ui_apply_but_func(but);
	data->retval= but->retval;
	data->applied= 1;
}
#endif

static void ui_apply_button(uiBlock *block, uiBut *but, uiActivateBut *data, int interactive)
{
	char *editstr;
	double *editval;
	float *editvec;
	ColorBand *editcoba;
	CurveMapping *editcumap;

	data->retval= 0;

	/* if we cancel and have not applied yet, there is nothing to do,
	 * otherwise we have to restore the original value again */
	if(data->cancel) {
		if(!data->applied)
			return;

		if(data->str) MEM_freeN(data->str);
		data->str= data->origstr;
		data->origstr= NULL;
		data->value= data->origvalue;
		data->origvalue= 0.0;
		VECCOPY(data->vec, data->origvec);
		data->origvec[0]= data->origvec[1]= data->origvec[2]= 0.0f;
	}
	else {
		/* we avoid applying interactive edits a second time
		 * at the end with the appliedinteractive flag */
		if(interactive)
			data->appliedinteractive= 1;
		else if(data->appliedinteractive)
			return;
	}

	/* ensures we are writing actual values */
	editstr= but->editstr;
	editval= but->editval;
	editvec= but->editvec;
	editcoba= but->editcoba;
	editcumap= but->editcumap;
	but->editstr= NULL;
	but->editval= NULL;
	but->editvec= NULL;
	but->editcoba= NULL;
	but->editcumap= NULL;

	/* handle different types */
	switch(but->type) {
		case BUT:
			ui_apply_but_BUT(but, data);
			break;
		case TEX:
			ui_apply_but_TEX(but, data);
			break;
		case TOG: 
		case TOGR: 
		case ICONTOG:
		case ICONTOGN:
		case TOGN:
		case BUT_TOGDUAL:
			ui_apply_but_TOG(block, but, data);
			break;
		case ROW:
			ui_apply_but_ROW(block, but, data);
			break;
		case SCROLL:
			break;
		case NUM:
		case NUMABS:
			ui_apply_but_NUM(but, data);
			break;
		case SLI:
		case NUMSLI:
			ui_apply_but_NUM(but, data);
			break;
		case HSVSLI:
			break;
		case ROUNDBOX:	
		case LABEL:	
			ui_apply_but_LABEL(but, data);
			break;
		case TOG3:	
			ui_apply_but_TOG3(but, data);
			break;
		case MENU:
		case ICONROW:
		case ICONTEXTROW:
		case BLOCK:
		case PULLDOWN:
		case COL:
			ui_apply_but_BLOCK(but, data);
			break;
		case BUTM:
			ui_apply_but_BUTM(but, data);
			break;
		case BUT_NORMAL:
		case HSVCUBE:
			ui_apply_but_VEC(but, data);
			break;
		case BUT_COLORBAND:
			ui_apply_but_COLORBAND(but, data);
			break;
		case BUT_CURVE:
			ui_apply_but_CURVE(but, data);
			break;
		case IDPOIN:
			ui_apply_but_IDPOIN(but, data);
			break;
#ifdef INTERNATIONAL
		case CHARTAB:
			ui_apply_but_CHARTAB(but, data);
			break;
#endif
		case LINK:
		case INLINK:
			break;
	}

	but->editstr= editstr;
	but->editval= editval;
	but->editvec= editvec;
	but->editcoba= editcoba;
	but->editcumap= editcumap;
}

/* ******************* copy and paste ********************  */

/* c = copy, v = paste */
static void ui_but_copy_paste(bContext *C, uiBut *but, uiActivateBut *data, char mode)
{
	static ColorBand but_copypaste_coba = {0};
	char buf[UI_MAX_DRAW_STR+1]= {0};
	double val;
	
	if(mode=='v' && but->lock)
		return;

	if(mode=='v') {
		/* extract first line from clipboard in case of multi-line copies */
		char *p = NULL; /* XXX 2.48 getClipboard(0); */
		if(p) {
			int i = 0;
			while (*p && *p!='\r' && *p!='\n' && i<UI_MAX_DRAW_STR) {
				buf[i++]=*p;
				p++;
			}
			buf[i]= 0;
		}
	}
	
	/* numeric value */
	if ELEM4(but->type, NUM, NUMABS, NUMSLI, HSVSLI) {
		
		if(but->poin==NULL && but->rnapoin.data==NULL);
		else if(mode=='c') {
			sprintf(buf, "%f", ui_get_but_val(but));
			/* XXX 2.48 putClipboard(buf, 0); */
		}
		else {
			if (sscanf(buf, " %lf ", &val) == 1) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				data->value= val;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}

	/* RGB triple */
	else if(but->type==COL) {
		float rgb[3];
		
		if(but->poin==NULL && but->rnapoin.data==NULL);
		else if(mode=='c') {

			ui_get_but_vectorf(but, rgb);
			sprintf(buf, "[%f, %f, %f]", rgb[0], rgb[1], rgb[2]);
			/* XXX 2.48 putClipboard(buf, 0); */
			
		}
		else {
			if (sscanf(buf, "[%f, %f, %f]", &rgb[0], &rgb[1], &rgb[2]) == 3) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				VECCOPY(data->vec, rgb);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}

	/* text/string and ID data */
	else if(ELEM(but->type, TEX, IDPOIN)) {
		uiActivateBut *data= but->activate;

		if(but->poin==NULL && but->rnapoin.data==NULL);
		else if(mode=='c') {
			button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			BLI_strncpy(buf, data->str, UI_MAX_DRAW_STR);
			/* XXX 2.48 putClipboard(data->str, 0); */
			data->cancel= 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else {
			button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			BLI_strncpy(data->str, buf, data->maxlen);
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}
	/* colorband (not supported by system clipboard) */
	else if(but->type==BUT_COLORBAND) {
		if(mode=='c') {
			if(but->poin)
				return;

			memcpy(&but_copypaste_coba, but->poin, sizeof(ColorBand));
		}
		else {
			if(but_copypaste_coba.tot==0)
				return;

			if(!but->poin)
				but->poin= MEM_callocN(sizeof(ColorBand), "colorband");

			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			memcpy(data->coba, &but_copypaste_coba, sizeof(ColorBand) );
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}
}

/* ************* in-button text selection/editing ************* */

/* return 1 if char ch is special character, otherwise return 0 */
static short test_special_char(char ch)
{
	switch(ch) {
		case '\\':
		case '/':
		case '~':
		case '!':
		case '@':
		case '#':
		case '$':
		case '%':
		case '^':
		case '&':
		case '*':
		case '(':
		case ')':
		case '+':
		case '=':
		case '{':
		case '}':
		case '[':
		case ']':
		case ':':
		case ';':
		case '\'':
		case '\"':
		case '<':
		case '>':
		case ',':
		case '.':
		case '?':
		case '_':
		case '-':
		case ' ':
			return 1;
			break;
		default:
			break;
	}
	return 0;
}

static int ui_textedit_delete_selection(uiBut *but, uiActivateBut *data)
{
	char *str;
	int x, changed;
	
	str= data->str;
	changed= (but->selsta != but->selend);
	
	for(x=0; x< strlen(str); x++) {
		if (but->selend + x <= strlen(str) ) {
			str[but->selsta + x]= str[but->selend + x];
		} else {
			str[but->selsta + x]= '\0';
			break;
		}
	}

	but->pos = but->selend = but->selsta;

	return changed;
}

static void ui_textedit_set_cursor_pos(uiBut *but, uiActivateBut *data, short x)
{
	char *origstr;
	
	origstr= MEM_callocN(sizeof(char)*(data->maxlen+1), "ui_textedit origstr");
	
	BLI_strncpy(origstr, but->drawstr, data->maxlen+1);
	but->pos= strlen(origstr)-but->ofs;
	
	while((but->aspect*UI_GetStringWidth(but->font, origstr+but->ofs, 0) + but->x1) > x) {
		if (but->pos <= 0) break;
		but->pos--;
		origstr[but->pos+but->ofs] = 0;
	}
	
	but->pos -= strlen(but->str);
	but->pos += but->ofs;
	if(but->pos<0) but->pos= 0;

	MEM_freeN(origstr);
}

static void ui_textedit_set_cursor_select(uiBut *but, uiActivateBut *data, short x)
{
	if (x > data->selstartx) data->selextend = EXTEND_RIGHT;
	else if (x < data->selstartx) data->selextend = EXTEND_LEFT;

	ui_textedit_set_cursor_pos(but, data, x);
						
	if (data->selextend == EXTEND_RIGHT) but->selend = but->pos;
	if (data->selextend == EXTEND_LEFT) but->selsta = but->pos;

	ui_check_but(but);
}

static int ui_textedit_type_ascii(uiBut *but, uiActivateBut *data, char ascii)
{
	char *str;
	int len, x, changed= 0;

	str= data->str;
	len= strlen(str);

	if(len-(but->selend - but->selsta)+1 <= data->maxlen) {
		/* type over the current selection */
		if ((but->selend - but->selsta) > 0)
			changed= ui_textedit_delete_selection(but, data);

		len= strlen(str);
		if(len < data->maxlen) {
			for(x= data->maxlen; x>but->pos; x--)
				str[x]= str[x-1];
			str[but->pos]= ascii;
			str[len+1]= '\0';

			but->pos++; 
			changed= 1;
		}
	}

	return 1;
}

void ui_textedit_move(uiBut *but, uiActivateBut *data, int direction, int select, int jump)
{
	char *str;
	int len;

	str= data->str;
	len= strlen(str);

	if(direction) { /* right*/
		/* if there's a selection */
		if ((but->selend - but->selsta) > 0) {
			/* extend the selection based on the first direction taken */
			if(select) {
				if (!data->selextend) {
					data->selextend = EXTEND_RIGHT;
				}
				if (data->selextend == EXTEND_RIGHT) {
					but->selend++;
					if (but->selend > len) but->selend = len;
				} else if (data->selextend == EXTEND_LEFT) {
					but->selsta++;
					/* if the selection start has gone past the end,
					* flip them so they're in sync again */
					if (but->selsta == but->selend) {
						but->pos = but->selsta;
						data->selextend = EXTEND_RIGHT;
					}
				}
			} else {
				but->selsta = but->pos = but->selend;
				data->selextend = 0;
			}
		} else {
			if(select) {
				/* make a selection, starting from the cursor position */
				but->selsta = but->pos;
				
				but->pos++;
				if(but->pos>strlen(str)) but->pos= strlen(str);
				
				but->selend = but->pos;
			} else if(jump) {
				/* jump betweenn special characters (/,\,_,-, etc.),
				 * look at function test_special_char() for complete
				 * list of special character, ctr -> */
				while(but->pos < len) {
					but->pos++;
					if(test_special_char(str[but->pos])) break;
				}
			} else {
				but->pos++;
				if(but->pos>strlen(str)) but->pos= strlen(str);
			}
		}
	}
	else { /* left */
		/* if there's a selection */
		if ((but->selend - but->selsta) > 0) {
			/* extend the selection based on the first direction taken */
			if(select) {
				if (!data->selextend) {
					data->selextend = EXTEND_LEFT;
				}
				if (data->selextend == EXTEND_LEFT) {
					but->selsta--;
					if (but->selsta < 0) but->selsta = 0;
				} else if (data->selextend == EXTEND_RIGHT) {
					but->selend--;
					/* if the selection start has gone past the end,
					* flip them so they're in sync again */
					if (but->selsta == but->selend) {
						but->pos = but->selsta;
						data->selextend = EXTEND_LEFT;
					}
				}
			} else {
				but->pos = but->selend = but->selsta;
				data->selextend = 0;
			}
		} else {
			if(select) {
				/* make a selection, starting from the cursor position */
				but->selend = but->pos;
				
				but->pos--;
				if(but->pos<0) but->pos= 0;
				
				but->selsta = but->pos;
			} else if(jump) {
				/* jump betweenn special characters (/,\,_,-, etc.),
				 * look at function test_special_char() for complete
				 * list of special character, ctr -> */
				while(but->pos > 0){
					but->pos--;
					if(test_special_char(str[but->pos])) break;
				}
			} else {
				if(but->pos>0) but->pos--;
			}
		}
	}
}

void ui_textedit_move_end(uiBut *but, uiActivateBut *data, int direction, int select)
{
	char *str;

	str= data->str;

	if(direction) { /* right */
		if(select) {
			but->selsta = but->pos;
			but->selend = strlen(str);
			data->selextend = EXTEND_RIGHT;
		} else {
			but->selsta = but->selend = but->pos= strlen(str);
		}
	}
	else { /* left */
		if(select) {
			but->selend = but->pos;
			but->selsta = 0;
			data->selextend = EXTEND_LEFT;
		} else {
			but->selsta = but->selend = but->pos= 0;
		}
	}
}

static int ui_textedit_delete(uiBut *but, uiActivateBut *data, int direction, int all)
{
	char *str;
	int len, x, changed= 0;

	str= data->str;
	len= strlen(str);

	if(all) {
		if(len) changed=1;
		str[0]= 0;
		but->pos= 0;
	}
	else if(direction) { /* delete */
		if ((but->selend - but->selsta) > 0) {
			changed= ui_textedit_delete_selection(but, data);
		}
		else if(but->pos>=0 && but->pos<len) {
			for(x=but->pos; x<len; x++)
				str[x]= str[x+1];
			str[len-1]='\0';
			changed= 1;
		}
	}
	else { /* backspace */
		if(len!=0) {
			if ((but->selend - but->selsta) > 0) {
				ui_textedit_delete_selection(but, data);
			}
			else if(but->pos>0) {
				for(x=but->pos; x<len; x++)
					str[x-1]= str[x];
				str[len-1]='\0';

				but->pos--;
				changed= 1;
			}
		} 
	}

	return changed;
}

static int ui_textedit_autocomplete(uiBut *but, uiActivateBut *data)
{
	char *str;
	int changed= 1;

	str= data->str;
	but->autocomplete_func(str, but->autofunc_arg);
	but->pos= strlen(str);

	return changed;
}

static int ui_textedit_copypaste(uiBut *but, uiActivateBut *data, int paste, int copy, int cut)
{
	char buf[UI_MAX_DRAW_STR]={0};
	char *str, *p;
	int len, x, y, i, changed= 0;

	str= data->str;
	len= strlen(str);
	
	/* paste */
	if (paste) {
		/* extract the first line from the clipboard */
		p = NULL; /* XXX 2.48 getClipboard(0); */

		if(p && p[0]) {
			while (*p && *p!='\r' && *p!='\n' && i<UI_MAX_DRAW_STR) {
				buf[i++]=*p;
				p++;
			}
			buf[i]= 0;

			/* paste over the current selection */
			if ((but->selend - but->selsta) > 0)
				ui_textedit_delete_selection(but, data);
			
			for (y=0; y<strlen(buf); y++)
			{
				/* add contents of buffer */
				if(len < data->maxlen) {
					for(x= data->maxlen; x>but->pos; x--)
						str[x]= str[x-1];
					str[but->pos]= buf[y];
					but->pos++; 
					len++;
					str[len]= '\0';
				}
			}

			changed= 1;
		}
	}
	/* cut & copy */
	else if (copy || cut) {
		/* copy the contents to the copypaste buffer */
		for(x= but->selsta; x <= but->selend; x++) {
			if (x==but->selend)
				buf[x] = '\0';
			else
				buf[(x - but->selsta)] = str[x];
		}
		/* XXX 2.48 putClipboard(buf, 0); */
		
		/* for cut only, delete the selection afterwards */
		if(cut)
			if((but->selend - but->selsta) > 0)
				changed= ui_textedit_delete_selection(but, data);
	} 

	return changed;
}

static void ui_textedit_begin(uiBut *but, uiActivateBut *data)
{
	if(data->str) {
		MEM_freeN(data->str);
		data->str= NULL;
	}

	/* retrieve string */
	if(but->type == TEX) {
		data->maxlen= but->max;
		data->str= MEM_callocN(sizeof(char)*(data->maxlen+1), "textedit str");

		ui_get_but_string(but, data->str, data->maxlen+1);
	}
	else if(but->type == IDPOIN) {
		ID *id;
		
		data->maxlen= 22;
		data->str= MEM_callocN(sizeof(char)*(data->maxlen+1), "textedit str");

		id= *but->idpoin_idpp;
		if(id) BLI_strncpy(data->str, id->name+2, data->maxlen+1);
		else data->str[0]= 0;
	}
	else {
		double value;

		data->maxlen= UI_MAX_DRAW_STR;
		data->str= MEM_callocN(sizeof(char)*(data->maxlen+1), "textedit str");
		
		value= ui_get_but_val(but);
		if(ui_is_but_float(but)) {
			if(but->a2) { /* amount of digits defined */
				if(but->a2==1) sprintf(data->str, "%.1f", value);
				else if(but->a2==2) sprintf(data->str, "%.2f", value);
				else if(but->a2==3) sprintf(data->str, "%.3f", value);
				else sprintf(data->str, "%.4f", value);
			}
			else sprintf(data->str, "%.3f", value);
		}
		else {
			sprintf(data->str, "%d", (int)value);
		}
	}

	data->origstr= BLI_strdup(data->str);
	data->selextend= 0;
	data->selstartx= 0;

	/* set cursor pos to the end of the text */
	but->editstr= data->str;
	but->pos= strlen(data->str);
	but->selsta= 0;
	but->selend= strlen(but->drawstr) - strlen(but->str);

	ui_check_but(but);
}

static void ui_textedit_end(uiBut *but, uiActivateBut *data)
{
	if(but) {
		but->editstr= 0;
		but->pos= -1;
	}
}

static void ui_textedit_next_but(uiBlock *block, uiBut *actbut)
{
	uiBut *but;

	/* label and roundbox can overlap real buttons (backdrops...) */
	if(actbut->type==LABEL && actbut->type==ROUNDBOX)
		return;

	for(but= actbut->next; but; but= but->next) {
		if(ELEM5(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI)) {
			but->activateflag= UI_ACTIVATE_TEXT_EDITING;
			return;
		}
	}
	for(but= block->buttons.first; but!=actbut; but= but->next) {
		if(ELEM5(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI)) {
			but->activateflag= UI_ACTIVATE_TEXT_EDITING;
			return;
		}
	}
}

static void ui_textedit_prev_but(uiBlock *block, uiBut *actbut)
{
	uiBut *but;

	/* label and roundbox can overlap real buttons (backdrops...) */
	if(actbut->type==LABEL && actbut->type==ROUNDBOX)
		return;

	for(but= actbut->prev; but; but= but->prev) {
		if(ELEM5(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI)) {
			but->activateflag= UI_ACTIVATE_TEXT_EDITING;
			return;
		}
	}
	for(but= block->buttons.last; but!=actbut; but= but->prev) {
		if(ELEM5(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI)) {
			but->activateflag= UI_ACTIVATE_TEXT_EDITING;
			return;
		}
	}
}

static void ui_do_but_textedit(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	int mx, my, changed= 0, handled= 0;

	switch(event->type) {
		case RIGHTMOUSE:
		case ESCKEY:
			data->cancel= 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			handled= 1;
			break;
		case LEFTMOUSE: {
			if(event->val) {
				mx= event->x;
				my= event->y;
				ui_window_to_block(data->region, block, &mx, &my);

				if ((but->y1 <= my) && (my <= but->y2) && (but->x1 <= mx) && (mx <= but->x2)) {
					ui_textedit_set_cursor_pos(but, data, mx);
					but->selsta = but->selend = but->pos;
					data->selstartx= mx;

					button_activate_state(C, but, BUTTON_STATE_TEXT_SELECTING);
					handled= 1;
				}
				else {
					button_activate_state(C, but, BUTTON_STATE_EXIT);
					handled= 1;
				}
			}
			break;
		}
	}

	if(event->val) {
		switch (event->type) {
			case VKEY:
			case XKEY:
			case CKEY:
				if(event->ctrl || event->oskey) {
					if(event->type == VKEY)
						changed= ui_textedit_copypaste(but, data, 1, 0, 0);
					else if(event->type == XKEY)
						changed= ui_textedit_copypaste(but, data, 0, 1, 0);
					else if(event->type == CKEY)
						changed= ui_textedit_copypaste(but, data, 0, 0, 1);

					handled= 1;
				}
				break;
			case RIGHTARROWKEY:
				ui_textedit_move(but, data, 1, event->shift, event->ctrl);
				handled= 1;
				break;
			case LEFTARROWKEY:
				ui_textedit_move(but, data, 0, event->shift, event->ctrl);
				handled= 1;
				break;
			case DOWNARROWKEY:
			case ENDKEY:
				ui_textedit_move_end(but, data, 1, event->shift);
				handled= 1;
				break;
			case UPARROWKEY:
			case HOMEKEY:
				ui_textedit_move_end(but, data, 0, event->shift);
				handled= 1;
				break;
			case PADENTER:
			case RETKEY:
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				handled= 1;
				break;
			case DELKEY:
				changed= ui_textedit_delete(but, data, 1, 0);
				handled= 1;
				break;

			case BACKSPACEKEY:
				changed= ui_textedit_delete(but, data, 0, event->shift);
				handled= 1;
				break;
				
			case TABKEY:
				/* there is a key conflict here, we can't tab with autocomplete */
				if(but->autocomplete_func) {
					changed= ui_textedit_autocomplete(but, data);
					handled= 1;
				}
				/* the hotkey here is not well defined, was G.qual so we check all */
				else if(event->shift || event->ctrl || event->alt || event->oskey) {
					ui_textedit_prev_but(block, but);
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				else {
					ui_textedit_next_but(block, but);
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				handled= 1;
				break;
		}

		if(event->ascii && !handled) {
			changed= ui_textedit_type_ascii(but, data, event->ascii);
			handled= 1;
		}
	}

	if(changed) {
		if(data->interactive) ui_apply_button(block, but, data, 1);
		else ui_check_but(but);
	}

	if(changed || handled)
		WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
}

static void ui_do_but_textedit_select(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	int mx, my, handled= 0;

	switch(event->type) {
		case MOUSEMOVE: {
			mx= event->x;
			my= event->y;
			ui_window_to_block(data->region, block, &mx, &my);

			ui_textedit_set_cursor_select(but, data, mx);
			handled= 1;
			break;
		}
		case LEFTMOUSE:
			if(event->val == 0)
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			handled= 1;
			break;
	}

	if(handled) {
		ui_check_but(but);
		WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
	}
}

/* ************* number editing for various types ************* */

static void ui_numedit_begin(uiBut *but, uiActivateBut *data)
{
	float butrange;

	if(but->type == BUT_CURVE) {
		data->cumap= (CurveMapping*)but->poin;
		but->editcumap= data->coba;
	}
	else if(but->type == BUT_COLORBAND) {
		data->coba= (ColorBand*)but->poin;
		but->editcoba= data->coba;
	}
	else if(ELEM(but->type, BUT_NORMAL, HSVCUBE)) {
		ui_get_but_vectorf(but, data->origvec);
		VECCOPY(data->vec, data->origvec);
		but->editvec= data->vec;
	}
	else {
		data->origvalue= ui_get_but_val(but);
		data->value= data->origvalue;
		but->editval= &data->value;

		butrange= (but->max - but->min);
		data->dragfstart= (butrange == 0.0)? 0.0f: (data->value - but->min)/butrange;
		data->dragf= data->dragfstart;
	}

	data->dragchange= 0;
	data->draglock= 1;
}

static void ui_numedit_end(uiBut *but, uiActivateBut *data)
{
	but->editval= NULL;
	but->editvec= NULL;
	but->editcoba= NULL;
	but->editcumap= NULL;

	data->dragstartx= 0;
	data->draglastx= 0;
	data->dragchange= 0;
	data->dragcbd= NULL;
	data->dragsel= 0;
}

static void ui_numedit_apply(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data)
{
	if(data->interactive) ui_apply_button(block, but, data, 1);
	else ui_check_but(but);

	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
}

/* ****************** block opening for various types **************** */

static void ui_blockopen_begin(bContext *C, uiBut *but, uiActivateBut *data)
{
	uiBlockFuncFP func= NULL;
	void *arg= NULL;

	switch(but->type) {
		case BLOCK:
		case PULLDOWN:
			func= but->block_func;
			arg= but->poin;
			break;
		case MENU:
			data->origvalue= ui_get_but_val(but);
			data->value= data->origvalue;
			but->editval= &data->value;

			func= ui_block_func_MENU;
			arg= but;
			break;
		case ICONROW:
			func= ui_block_func_ICONROW;
			arg= but;
			break;
		case ICONTEXTROW:
			func= ui_block_func_ICONTEXTROW;
			arg= but;
			break;
		case COL:
			ui_get_but_vectorf(but, data->origvec);
			VECCOPY(data->vec, data->origvec);
			but->editvec= data->vec;

			func= ui_block_func_COL;
			arg= but;
			break;
	}

	if(func)
		data->blockhandle= ui_menu_block_create(C, data->region, but, func, arg);

	/* this makes adjacent blocks auto open from now on */
	if(but->block->auto_open==0) but->block->auto_open= 1;
}

static void ui_blockopen_end(bContext *C, uiBut *but, uiActivateBut *data)
{
	if(but) {
		but->editval= NULL;
		but->editvec= NULL;

		but->block->auto_open_last= PIL_check_seconds_timer();
	}

	if(data->blockhandle) {
		ui_menu_block_free(C, data->blockhandle);
		data->blockhandle= NULL;
	}
}

/* ***************** events for different button types *************** */

static void ui_do_but_BUT(bContext *C, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(event->type == LEFTMOUSE && event->val)
			button_activate_state(C, but, BUTTON_STATE_WAIT_RELEASE);
		else if(ELEM(event->type, PADENTER, RETKEY) && event->val)
			button_activate_state(C, but, BUTTON_STATE_WAIT_FLASH);
	}
	else if(data->state == BUTTON_STATE_WAIT_RELEASE) {
		if(event->type == LEFTMOUSE && event->val==0) {
			if(!(but->flag & UI_SELECT))
				data->cancel= 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}
}

static void ui_do_but_KEYEVT(bContext *C, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val)
			button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
	}
	else if(data->state == BUTTON_STATE_WAIT_KEY_EVENT) {
		if(event->type == MOUSEMOVE)
			return;

		/* XXX 2.50 missing function */
#if 0
		if(event->val) {
			if(!key_event_to_string(event)[0])
				data->cancel= 1;
			else
				ui_set_but_val(but, event->type);

			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
#endif
	}
}

static void ui_do_but_TEX(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val)
			button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
	}
	else if(data->state == BUTTON_STATE_TEXT_EDITING)
		ui_do_but_textedit(C, block, but, data, event);
	else if(data->state == BUTTON_STATE_TEXT_SELECTING)
		ui_do_but_textedit_select(C, block, but, data, event);
}

static void ui_do_but_TOG(bContext *C, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val) {
			data->togdual= event->ctrl;
			data->togonly= !event->shift;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}
}

static void ui_do_but_EXIT(bContext *C, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	if(data->state == BUTTON_STATE_HIGHLIGHT)
		if(ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val)
			button_activate_state(C, but, BUTTON_STATE_EXIT);
}

static int ui_numedit_but_NUM(uiBut *but, uiActivateBut *data, float fac, int snap, int mx)
{
	float deler, tempf;
	int lvalue, temp, changed= 0;
	
	if(mx == data->draglastx)
		return changed;
	
	/* drag-lock - prevent unwanted scroll adjustments */
	/* change value (now 3) to adjust threshold in pixels */
	if(data->draglock) {
		if(abs(mx-data->dragstartx) <= 3)
			return changed;

		data->draglock= 0;
		data->dragstartx= mx;  /* ignore mouse movement within drag-lock */
	}

	deler= 500;
	if(!ui_is_but_float(but)) {
		if((but->max-but->min)<100) deler= 200.0;
		if((but->max-but->min)<25) deler= 50.0;
	}
	deler /= fac;

	if(ui_is_but_float(but) && but->max-but->min > 11) {
		/* non linear change in mouse input- good for high precicsion */
		data->dragf+= (((float)(mx-data->draglastx))/deler) * (fabs(data->dragstartx-mx)*0.002);
	} else if (!ui_is_but_float(but) && but->max-but->min > 129) { /* only scale large int buttons */
		/* non linear change in mouse input- good for high precicsionm ints need less fine tuning */
		data->dragf+= (((float)(mx-data->draglastx))/deler) * (fabs(data->dragstartx-mx)*0.004);
	} else {
		/*no scaling */
		data->dragf+= ((float)(mx-data->draglastx))/deler ;
	}
	
	if(data->dragf>1.0) data->dragf= 1.0;
	if(data->dragf<0.0) data->dragf= 0.0;
	data->draglastx= mx;
	tempf= ( but->min + data->dragf*(but->max-but->min));
	
	if(!ui_is_but_float(but)) {
		
		temp= floor(tempf+.5);
		
		if(tempf==but->min || tempf==but->max);
		else if(snap) {
			if(snap == 2) temp= 100*(temp/100);
			else temp= 10*(temp/10);
		}
		if( temp>=but->min && temp<=but->max) {
			lvalue= (int)data->value;
			
			if(temp != lvalue ) {
				data->dragchange= 1;
				data->value= (double)temp;
				changed= 1;
			}
		}

	}
	else {
		temp= 0;
		if(snap) {
			if(snap == 2) {
				if(tempf==but->min || tempf==but->max);
				else if(but->max-but->min < 2.10) tempf= 0.01*floor(100.0*tempf);
				else if(but->max-but->min < 21.0) tempf= 0.1*floor(10.0*tempf);
				else tempf= floor(tempf);
			}
			else {
				if(tempf==but->min || tempf==but->max);
				else if(but->max-but->min < 2.10) tempf= 0.1*floor(10*tempf);
				else if(but->max-but->min < 21.0) tempf= floor(tempf);
				else tempf= 10.0*floor(tempf/10.0);
			}
		}

		if( tempf>=but->min && tempf<=but->max) {
			if(tempf != data->value) {
				data->dragchange= 1;
				data->value= tempf;
				changed= 1;
			}
		}
	}

	return changed;
}

static void ui_do_but_NUM(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	int mx, my, click= 0;

	mx= event->x;
	my= event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(event->val) {
			if(ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->shift) {
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			}
			else if(event->type == LEFTMOUSE) {
				data->dragstartx= mx;
				data->draglastx= mx;
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			}
			else if(ELEM(event->type, PADENTER, RETKEY) && event->val)
				click= 1;
		}
	}
	else if(data->state == BUTTON_STATE_NUM_EDITING) {
		if(event->type == LEFTMOUSE && event->val==0) {
			if(data->dragchange)
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			else
				click= 1;
		}
		else if(event->type == MOUSEMOVE) {
			float fac;
			int snap;

			fac= 1.0f;
			if(event->shift) fac /= 10.0f;
			if(event->alt) fac /= 20.0f;

			if(event->custom == EVT_TABLET) {
				wmTabletData *wmtab= event->customdata;

				/* de-sensitise based on tablet pressure */
				if (ELEM(wmtab->Active, DEV_STYLUS, DEV_ERASER))
				 	fac *= wmtab->Pressure;
			}
			
			snap= (event->ctrl)? (event->shift)? 2: 1: 0;

			if(ui_numedit_but_NUM(but, data, fac, snap, mx))
				ui_numedit_apply(C, block, but, data);
		}
	}
	else if(data->state == BUTTON_STATE_TEXT_EDITING)
		ui_do_but_textedit(C, block, but, data, event);
	else if(data->state == BUTTON_STATE_TEXT_SELECTING)
		ui_do_but_textedit_select(C, block, but, data, event);

	if(click) {
		/* we can click on the side arrows to increment/decrement,
		 * or click inside to edit the value directly */
		float tempf;
		int temp;

		if(!ui_is_but_float(but)) {
			if(mx < (but->x1 + (but->x2 - but->x1)/3 - 3)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				temp= (int)data->value - 1;
				if(temp>=but->min && temp<=but->max)
					data->value= (double)temp;
				else
					data->cancel= 1;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else if(mx > (but->x1 + (2*(but->x2 - but->x1)/3) + 3)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				temp= (int)data->value + 1;
				if(temp>=but->min && temp<=but->max)
					data->value= (double)temp;
				else
					data->cancel= 1;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
		}
		else {
			if(mx < (but->x1 + (but->x2 - but->x1)/3 - 3)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				tempf= data->value - 0.01*but->a1;
				if (tempf < but->min) tempf = but->min;
				data->value= tempf;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else if(mx > but->x1 + (2*((but->x2 - but->x1)/3) + 3)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				tempf= data->value + 0.01*but->a1;
				if (tempf < but->min) tempf = but->min;
				data->value= tempf;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
		}
	}
}

static int ui_numedit_but_SLI(uiBut *but, uiActivateBut *data, int shift, int ctrl, int mx)
{
	float deler, f, tempf;
	int temp, lvalue, changed= 0;

	if(but->type==NUMSLI) deler= ((but->x2-but->x1)/2 - 5.0*but->aspect);
	else if(but->type==HSVSLI) deler= ((but->x2-but->x1)/2 - 5.0*but->aspect);
	else deler= (but->x2-but->x1- 5.0*but->aspect);

	f= (float)(mx-data->dragstartx)/deler + data->dragfstart;
	
	if(shift)
		f= (f-data->dragfstart)/10.0 + data->dragfstart;

	CLAMP(f, 0.0, 1.0);
	tempf= but->min+f*(but->max-but->min);		
	temp= floor(tempf+.5);

	if(ctrl) {
		if(tempf==but->min || tempf==but->max);
		else if(ui_is_but_float(but)) {

			if(shift) {
				if(tempf==but->min || tempf==but->max);
				else if(but->max-but->min < 2.10) tempf= 0.01*floor(100.0*tempf);
				else if(but->max-but->min < 21.0) tempf= 0.1*floor(10.0*tempf);
				else tempf= floor(tempf);
			}
			else {
				if(but->max-but->min < 2.10) tempf= 0.1*floor(10*tempf);
				else if(but->max-but->min < 21.0) tempf= floor(tempf);
				else tempf= 10.0*floor(tempf/10.0);
			}
		}
		else {
			temp= 10*(temp/10);
			tempf= temp;
		}
	}

	if(!ui_is_but_float(but)) {
		lvalue= floor(data->value+0.5);

		if(temp != lvalue) {
			data->value= temp;
			data->dragchange= 1;
			changed= 1;
		}
	}
	else {
		if(tempf != data->value) {
			data->value= tempf;
			data->dragchange= 1;
			changed= 1;
		}
	}

	return changed;
}

static void ui_do_but_SLI(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	int mx, my, click= 0;

	mx= event->x;
	my= event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val) {
			/* start either dragging as slider, or editing as text */
			if(mx>= -6+(but->x1+but->x2)/2) {
				if(event->type == LEFTMOUSE) {
					data->dragstartx= mx;
					data->draglastx= mx;
					button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				}
				else
					click= 1;
			}
			else
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
		}
	}
	else if(data->state == BUTTON_STATE_NUM_EDITING) {
		if(event->type == LEFTMOUSE && event->val==0) {
			if(data->dragchange)
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			else
				click= 1;
		}
		else if(event->type == MOUSEMOVE) {
			if(ui_numedit_but_SLI(but, data, event->shift, event->ctrl, mx))
				ui_numedit_apply(C, block, but, data);
		}
	}
	else if(data->state == BUTTON_STATE_TEXT_EDITING)
		ui_do_but_textedit(C, block, but, data, event);
	else if(data->state == BUTTON_STATE_TEXT_SELECTING)
		ui_do_but_textedit_select(C, block, but, data, event);

	if(click) {
		float f, h;
		float tempf;
		int temp;
		
		button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

		tempf= data->value;
		temp= (int)data->value;

		h= but->y2-but->y1;

		if(but->type==SLI) f= (float)(mx-but->x1)/(but->x2-but->x1-h);
		else f= (float)(mx- (but->x1+but->x2)/2)/((but->x2-but->x1)/2 - h);
		
		f= but->min+f*(but->max-but->min);

		if(!ui_is_but_float(but)) {
			if(f<temp) temp--;
			else temp++;

			if(temp>=but->min && temp<=but->max)
				data->value= temp;
			else
				data->cancel= 1;
		} 
		else {
			if(f<tempf) tempf-=.01;
			else tempf+=.01;

			if(tempf>=but->min && tempf<=but->max)
				data->value= tempf;
			else
				data->cancel= 1;
		}

		button_activate_state(C, but, BUTTON_STATE_EXIT);
	}
}

static void ui_do_but_BLOCK(bContext *C, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val)
			button_activate_state(C, but, BUTTON_STATE_BLOCK_OPEN);
	}
	else if(data->state == BUTTON_STATE_BLOCK_OPEN) {
		if(event->type == MESSAGE) {
			uiMenuBlockHandle *handle= event->customdata;

			if(handle == data->blockhandle) {
				data->blockretval= handle->blockretval;

				if(handle->blockretval == UI_RETURN_OK) {
					if(but->type == COL)
						VECCOPY(data->vec, handle->retvec)
					else if(ELEM3(but->type, MENU, ICONROW, ICONTEXTROW))
						data->value= handle->retvalue;
				}

				if(handle->blockretval == UI_RETURN_OUT)
					/* we close the block and proceed as if nothing happened */
					button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);
				else
					/* ok/cancel, we exit and will send message in _exit */
					button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}
}

static int ui_numedit_but_NORMAL(uiBut *but, uiActivateBut *data, int mx, int my)
{
	float dx, dy, rad, radsq, mrad, *fp;
	int mdx, mdy, changed= 1;
	
	/* button is presumed square */
	/* if mouse moves outside of sphere, it does negative normal */

	fp= data->origvec;
	rad= (but->x2 - but->x1);
	radsq= rad*rad;
	
	if(fp[2]>0.0f) {
		mdx= (rad*fp[0]);
		mdy= (rad*fp[1]);
	}
	else if(fp[2]> -1.0f) {
		mrad= rad/sqrt(fp[0]*fp[0] + fp[1]*fp[1]);
		
		mdx= 2.0f*mrad*fp[0] - (rad*fp[0]);
		mdy= 2.0f*mrad*fp[1] - (rad*fp[1]);
	}
	else mdx= mdy= 0;
	
	dx= (float)(mx+mdx-data->dragstartx);
	dy= (float)(my+mdy-data->dragstarty);

	fp= data->vec;
	mrad= dx*dx+dy*dy;
	if(mrad < radsq) {	/* inner circle */
		fp[0]= dx;
		fp[1]= dy;
		fp[2]= sqrt( radsq-dx*dx-dy*dy );
	}
	else {	/* outer circle */
		
		mrad= rad/sqrt(mrad);	// veclen
		
		dx*= (2.0f*mrad - 1.0f);
		dy*= (2.0f*mrad - 1.0f);
		
		mrad= dx*dx+dy*dy;
		if(mrad < radsq) {
			fp[0]= dx;
			fp[1]= dy;
			fp[2]= -sqrt( radsq-dx*dx-dy*dy );
		}
	}
	Normalize(fp);

	data->draglastx= mx;
	data->draglasty= my;

	return changed;
}

static void ui_do_but_NORMAL(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	int mx, my;

	mx= event->x;
	my= event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(event->type==LEFTMOUSE && event->val) {
			data->dragstartx= mx;
			data->dragstarty= my;
			data->draglastx= mx;
			data->draglasty= my;
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

			/* also do drag the first time */
			if(ui_numedit_but_NORMAL(but, data, mx, my))
				ui_numedit_apply(C, block, but, data);
		}
	}
	else if(data->state == BUTTON_STATE_NUM_EDITING) {
		if(event->type == MOUSEMOVE) {
			if(mx!=data->draglastx || my!=data->draglasty) {
				if(ui_numedit_but_NORMAL(but, data, mx, my))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if(event->type==LEFTMOUSE && event->val==0)
			button_activate_state(C, but, BUTTON_STATE_EXIT);
	}
}

static int ui_numedit_but_HSVCUBE(uiBut *but, uiActivateBut *data, int mx, int my)
{
	float x, y;
	int changed= 1;

	/* relative position within box */
	x= ((float)mx-but->x1)/(but->x2-but->x1);
	y= ((float)my-but->y1)/(but->y2-but->y1);
	CLAMP(x, 0.0, 1.0);
	CLAMP(y, 0.0, 1.0);
	
	if(but->a1==0) {
		but->hsv[0]= x; 
		but->hsv[2]= y; 
	}
	else if(but->a1==1) {
		but->hsv[0]= x; 				
		but->hsv[1]= y; 				
	}
	else if(but->a1==2) {
		but->hsv[2]= x; 
		but->hsv[1]= y; 
	}
	else
		but->hsv[0]= x; 

	ui_set_but_hsv(but);	// converts to rgb
	
	// update button values and strings
	ui_update_block_buts_hsv(but->block, but->hsv);

	data->draglastx= mx;
	data->draglasty= my;

	return changed;
}

static void ui_do_but_HSVCUBE(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	int mx, my;

	mx= event->x;
	my= event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(event->type==LEFTMOUSE && event->val) {
			data->dragstartx= mx;
			data->dragstarty= my;
			data->draglastx= mx;
			data->draglasty= my;
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

			/* also do drag the first time */
			if(ui_numedit_but_HSVCUBE(but, data, mx, my))
				ui_numedit_apply(C, block, but, data);
		}
	}
	else if(data->state == BUTTON_STATE_NUM_EDITING) {
		if(event->type == MOUSEMOVE) {
			if(mx!=data->draglastx || my!=data->draglasty) {
				if(ui_numedit_but_HSVCUBE(but, data, mx, my))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if(event->type==LEFTMOUSE && event->val==0)
			button_activate_state(C, but, BUTTON_STATE_EXIT);
	}
}

static int verg_colorband(const void *a1, const void *a2)
{
	const CBData *x1=a1, *x2=a2;
	
	if( x1->pos > x2->pos ) return 1;
	else if( x1->pos < x2->pos) return -1;
	return 0;
}

static void ui_colorband_update(ColorBand *coba)
{
	int a;
	
	if(coba->tot<2) return;
	
	for(a=0; a<coba->tot; a++) coba->data[a].cur= a;
		qsort(coba->data, coba->tot, sizeof(CBData), verg_colorband);
	for(a=0; a<coba->tot; a++) {
		if(coba->data[a].cur==coba->cur) {
			coba->cur= a;
			break;
		}
	}
}

static int ui_numedit_but_COLORBAND(uiBut *but, uiActivateBut *data, int mx)
{
	float dx;
	int changed= 0;

	if(data->draglastx == mx)
		return changed;

	dx= ((float)(mx - data->draglastx))/(but->x2-but->x1);
	data->dragcbd->pos += dx;
	CLAMP(data->dragcbd->pos, 0.0, 1.0);
	
	ui_colorband_update(data->coba);
	data->dragcbd= data->coba->data + data->coba->cur;	/* because qsort */
	
	data->draglastx= mx;
	changed= 1;

	return changed;
}

static void ui_do_but_COLORBAND(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	ColorBand *coba;
	CBData *cbd;
	int mx, my, a, xco, mindist= 12;

	mx= event->x;
	my= event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(event->type==LEFTMOUSE && event->val) {
			coba= (ColorBand*)but->poin;

			if(event->ctrl) {
				/* insert new key on mouse location */
				if(coba->tot < MAXCOLORBAND-1) {
					float pos= ((float)(mx - but->x1))/(but->x2-but->x1);
					float col[4];
					
					do_colorband(coba, pos, col);	/* executes it */
					
					coba->tot++;
					coba->cur= coba->tot-1;
					
					coba->data[coba->cur].r= col[0];
					coba->data[coba->cur].g= col[1];
					coba->data[coba->cur].b= col[2];
					coba->data[coba->cur].a= col[3];
					coba->data[coba->cur].pos= pos;

					ui_colorband_update(coba);
				}

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else {
				data->dragstartx= mx;
				data->dragstarty= my;
				data->draglastx= mx;
				data->draglasty= my;

				/* activate new key when mouse is close */
				for(a=0, cbd= coba->data; a<coba->tot; a++, cbd++) {
					xco= but->x1 + (cbd->pos*(but->x2-but->x1));
					xco= ABS(xco-mx);
					if(a==coba->cur) xco+= 5; // selected one disadvantage
					if(xco<mindist) {
						coba->cur= a;
						mindist= xco;
					}
				}
		
				data->dragcbd= coba->data + coba->cur;
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			}
		}
	}
	else if(data->state == BUTTON_STATE_NUM_EDITING) {
		if(event->type == MOUSEMOVE) {
			if(mx!=data->draglastx || my!=data->draglasty) {
				if(ui_numedit_but_COLORBAND(but, data, mx))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if(event->type==LEFTMOUSE && event->val==0)
			button_activate_state(C, but, BUTTON_STATE_EXIT);
	}
}

static int ui_numedit_but_CURVE(uiBut *but, uiActivateBut *data, int snap, int mx, int my)
{
	CurveMapping *cumap= data->cumap;
	CurveMap *cuma= cumap->cm+cumap->cur;
	CurveMapPoint *cmp= cuma->curve;
	float fx, fy, zoomx, zoomy, offsx, offsy;
	int a, changed= 0;

	zoomx= (but->x2-but->x1)/(cumap->curr.xmax-cumap->curr.xmin);
	zoomy= (but->y2-but->y1)/(cumap->curr.ymax-cumap->curr.ymin);
	offsx= cumap->curr.xmin;
	offsy= cumap->curr.ymin;

	if(data->dragsel != -1) {
		int moved_point= 0;		/* for ctrl grid, can't use orig coords because of sorting */
		
		fx= (mx-data->draglastx)/zoomx;
		fy= (my-data->draglasty)/zoomy;
		for(a=0; a<cuma->totpoint; a++) {
			if(cmp[a].flag & SELECT) {
				float origx= cmp[a].x, origy= cmp[a].y;
				cmp[a].x+= fx;
				cmp[a].y+= fy;
				if(snap) {
					cmp[a].x= 0.125f*floor(0.5f + 8.0f*cmp[a].x);
					cmp[a].y= 0.125f*floor(0.5f + 8.0f*cmp[a].y);
				}
				if(cmp[a].x!=origx || cmp[a].y!=origy)
					moved_point= 1;
			}
		}

		curvemapping_changed(cumap, 0);	/* no remove doubles */
		
		if(moved_point) {
			data->draglastx= mx;
			data->draglasty= my;
			changed= 1;
		}

		data->dragchange= 1; /* mark for selection */
	}
	else {
		fx= (mx-data->draglastx)/zoomx;
		fy= (my-data->draglasty)/zoomy;
		
		/* clamp for clip */
		if(cumap->flag & CUMA_DO_CLIP) {
			if(cumap->curr.xmin-fx < cumap->clipr.xmin)
				fx= cumap->curr.xmin - cumap->clipr.xmin;
			else if(cumap->curr.xmax-fx > cumap->clipr.xmax)
				fx= cumap->curr.xmax - cumap->clipr.xmax;
			if(cumap->curr.ymin-fy < cumap->clipr.ymin)
				fy= cumap->curr.ymin - cumap->clipr.ymin;
			else if(cumap->curr.ymax-fy > cumap->clipr.ymax)
				fy= cumap->curr.ymax - cumap->clipr.ymax;
		}				

		cumap->curr.xmin-=fx;
		cumap->curr.ymin-=fy;
		cumap->curr.xmax-=fx;
		cumap->curr.ymax-=fy;
		
		data->draglastx= mx;
		data->draglasty= my;

		changed= 1;
	}

	return changed;
}

static void ui_do_but_CURVE(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	int mx, my, a, changed= 0;

	mx= event->x;
	my= event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(event->type==LEFTMOUSE && event->val) {
			CurveMapping *cumap= (CurveMapping*)but->poin;
			CurveMap *cuma= cumap->cm+cumap->cur;
			CurveMapPoint *cmp= cuma->curve;
			float fx, fy, zoomx, zoomy, offsx, offsy;
			float dist, mindist= 200.0f; // 14 pixels radius
			int sel= -1;

			zoomx= (but->x2-but->x1)/(cumap->curr.xmax-cumap->curr.xmin);
			zoomy= (but->y2-but->y1)/(cumap->curr.ymax-cumap->curr.ymin);
			offsx= cumap->curr.xmin;
			offsy= cumap->curr.ymin;

			if(event->ctrl) {
				fx= ((float)my - but->x1)/zoomx + offsx;
				fy= ((float)my - but->y1)/zoomy + offsy;
				
				curvemap_insert(cuma, fx, fy);
				curvemapping_changed(cumap, 0);
				changed= 1;
			}

			/* check for selecting of a point */
			cmp= cuma->curve;	/* ctrl adds point, new malloc */
			for(a=0; a<cuma->totpoint; a++) {
				fx= but->x1 + zoomx*(cmp[a].x-offsx);
				fy= but->y1 + zoomy*(cmp[a].y-offsy);
				dist= (fx-mx)*(fx-mx) + (fy-my)*(fy-my);
				if(dist < mindist) {
					sel= a;
					mindist= dist;
				}
			}

			if (sel == -1) {
				/* if the click didn't select anything, check if it's clicked on the 
				 * curve itself, and if so, add a point */
				fx= ((float)mx - but->x1)/zoomx + offsx;
				fy= ((float)my - but->y1)/zoomy + offsy;
				
				cmp= cuma->table;

				/* loop through the curve segment table and find what's near the mouse.
				 * 0.05 is kinda arbitrary, but seems to be what works nicely. */
				for(a=0; a<=CM_TABLE; a++) {			
					if ( ( fabs(fx - cmp[a].x) < (0.05) ) && ( fabs(fy - cmp[a].y) < (0.05) ) ) {
					
						curvemap_insert(cuma, fx, fy);
						curvemapping_changed(cumap, 0);

						changed= 1;
						
						/* reset cmp back to the curve points again, rather than drawing segments */		
						cmp= cuma->curve;
						
						/* find newly added point and make it 'sel' */
						for(a=0; a<cuma->totpoint; a++)
							if(cmp[a].x == fx)
								sel = a;
							
						break;
					}
				}
			}

			if(sel!= -1) {
				/* ok, we move a point */
				/* deselect all if this one is deselect. except if we hold shift */
				if(event->shift==0 && (cmp[sel].flag & SELECT)==0)
					for(a=0; a<cuma->totpoint; a++)
						cmp[a].flag &= ~SELECT;
				cmp[sel].flag |= SELECT;
			}
			else {
				/* move the view */
				data->cancel= 1;
			}

			data->dragsel= sel;

			data->dragstartx= mx;
			data->dragstarty= my;
			data->draglastx= mx;
			data->draglasty= my;

			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
		}
	}
	else if(data->state == BUTTON_STATE_NUM_EDITING) {
		if(event->type == MOUSEMOVE) {
			if(mx!=data->draglastx || my!=data->draglasty) {
				if(ui_numedit_but_CURVE(but, data, event->shift, mx, my))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if(event->type==LEFTMOUSE && event->val==0) {
			if(data->dragsel != -1) {
				CurveMapping *cumap= data->cumap;
				CurveMap *cuma= cumap->cm+cumap->cur;
				CurveMapPoint *cmp= cuma->curve;

				if(!data->dragchange) {
					/* deselect all, select one */
					if(event->shift==0) {
						for(a=0; a<cuma->totpoint; a++)
							cmp[a].flag &= ~SELECT;
						cmp[data->dragsel].flag |= SELECT;
					}
				}
				else
					curvemapping_changed(cumap, 1);	/* remove doubles */
			}

			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}
}

#ifdef INTERNATIONAL
static void ui_do_but_CHARTAB(bContext *C, uiBlock *block, uiBut *but, uiActivateBut *data, wmEvent *event)
{
	/* XXX 2.50 bad global and state access */
#if 0
	float sx, sy, ex, ey;
	float width, height;
	float butw, buth;
	int mx, my, x, y, cs, che;

	mx= event->x;
	my= event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val) {
			/* Calculate the size of the button */
			width = abs(but->x2 - but->x1);
			height = abs(but->y2 - but->y1);

			butw = floor(width / 12);
			buth = floor(height / 6);

			/* Initialize variables */
			sx = but->x1;
			ex = but->x1 + butw;
			sy = but->y1 + height - buth;
			ey = but->y1 + height;

			cs = G.charstart;

			/* And the character is */
			x = (int) ((mx / butw) - 0.5);
			y = (int) (6 - ((my / buth) - 0.5));

			che = cs + (y*12) + x;

			if(che > G.charmax)
				che = 0;

			if(G.obedit)
			{
				do_textedit(0,0,che);
			}

			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if(ELEM(event->type, WHEELUPMOUSE, PAGEUPKEY)) {
			for(but= block->buttons.first; but; but= but->next) {
				if(but->type == CHARTAB) {
					G.charstart = G.charstart - (12*6);
					if(G.charstart < 0)
						G.charstart = 0;
					if(G.charstart < G.charmin)
						G.charstart = G.charmin;
					ui_draw_but(but);

					//Really nasty... to update the num button from the same butblock
					for(bt= block->buttons.first; bt; bt= bt->next)
					{
						if(ELEM(bt->type, NUM, NUMABS)) {
							ui_check_but(bt);
							ui_draw_but(bt);
						}
					}
					retval=UI_CONT;
					break;
				}
			}
			break;
		}
		else if(ELEM(event->type, WHEELDOWNMOUSE, PAGEDOWNKEY)) {
			for(but= block->buttons.first; but; but= but->next)
			{
				if(but->type == CHARTAB)
				{
					G.charstart = G.charstart + (12*6);
					if(G.charstart > (0xffff - 12*6))
						G.charstart = 0xffff - (12*6);
					if(G.charstart > G.charmax - 12*6)
						G.charstart = G.charmax - 12*6;
					ui_draw_but(but);

					for(bt= block->buttons.first; bt; bt= bt->next)
					{
						if(ELEM(bt->type, NUM, NUMABS)) {
							ui_check_but(bt);
							ui_draw_but(bt);
						}
					}
					
					but->flag |= UI_ACTIVE;
					retval=UI_RETURN_OK;
					break;
				}
			}
			break;
		}
	}
#endif
}
#endif

static int ui_do_button(bContext *C, uiBlock *block, uiBut *but, wmEvent *event)
{
	uiActivateBut *data;
	int handled;

	data= but->activate;
	handled= 0;

	/* handle copy-paste */
	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		if(ELEM(event->type, CKEY, VKEY) && event->val && (event->ctrl || event->oskey)) {
			ui_but_copy_paste(C, but, data, (event->type == CKEY)? 'c': 'v');
			return 1;
		}
	}

	/* these events are swallowed */
	if(ELEM5(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE, WHEELUPMOUSE, WHEELDOWNMOUSE))
		handled= 1;
	else if(ELEM(event->type, PADENTER, RETKEY))
		handled= 1;

	/* verify if we can edit this button */
	if(but->lock) {
		if(but->lockstr) {
			WM_report(C, WM_LOG_WARNING, but->lockstr);
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return handled;
		}
	} 
	else if(but->pointype && but->poin==0) {
		/* there's a pointer needed */
		WM_reportf(C, WM_LOG_WARNING, "DoButton pointer error: %s", but->str);
		button_activate_state(C, but, BUTTON_STATE_EXIT);
		return handled;
	}

	switch(but->type) {
	case BUT:
		ui_do_but_BUT(C, but, data, event);
		break;
	case KEYEVT:
		ui_do_but_KEYEVT(C, but, data, event);
		break;
	case TOG: 
	case TOGR: 
	case ICONTOG:
	case ICONTOGN:
	case TOGN:
	case BUT_TOGDUAL:
		ui_do_but_TOG(C, but, data, event);
		break;
#if 0
	case SCROLL:
		/* DrawBut(b, 1); */
		/* do_scrollbut(b); */
		/* DrawBut(b,0); */
		break;
#endif
	case NUM:
	case NUMABS:
		ui_do_but_NUM(C, block, but, data, event);
		break;
	case SLI:
	case NUMSLI:
	case HSVSLI:
		ui_do_but_SLI(C, block, but, data, event);
		break;
	case ROUNDBOX:	
	case LABEL:	
	case TOG3:	
	case ROW:
		ui_do_but_EXIT(C, but, data, event);
		break;
	case TEX:
	case IDPOIN:
		ui_do_but_TEX(C, block, but, data, event);
		break;
	case MENU:
		ui_do_but_BLOCK(C, but, data, event);
		break;
	case ICONROW:
		ui_do_but_BLOCK(C, but, data, event);
		break;
	case ICONTEXTROW:
		ui_do_but_BLOCK(C, but, data, event);
		break;
	case BLOCK:
	case PULLDOWN:
		ui_do_but_BLOCK(C, but, data, event);
		break;
	case BUTM:
		ui_do_but_BUT(C, but, data, event);
		break;
	case COL:
		if(but->a1 == -1)  // signal to prevent calling up color picker
			ui_do_but_EXIT(C, but, data, event);
		else
			ui_do_but_BLOCK(C, but, data, event);
		break;
	case BUT_NORMAL:
		ui_do_but_NORMAL(C, block, but, data, event);
		break;
	case BUT_COLORBAND:
		ui_do_but_COLORBAND(C, block, but, data, event);
		break;
	case BUT_CURVE:
		ui_do_but_CURVE(C, block, but, data, event);
		break;
	case HSVCUBE:
		ui_do_but_HSVCUBE(C, block, but, data, event);
		break;
#ifdef INTERNATIONAL
	case CHARTAB:
		ui_do_but_CHARTAB(C, block, but, data, event);
		break;
#endif
	/* XXX 2.50 links not implemented yet */
#if 0
	case LINK:
	case INLINK:
		retval= ui_do_but_LINK(block, but);
		break;
#endif
	}
	
	return handled;
}

/* ************************ button utilities *********************** */

static int ui_but_contains_pt(uiBut *but, int mx, int my)
{
	return ((but->x1<mx && but->x2>=mx) && (but->y1<my && but->y2>=my));
}

static uiBut *ui_but_find_activated(ARegion *ar, uiActivateBut *data, uiBlock **rblock)
{
	uiBlock *block;
	uiBut *but;

	for(block=ar->uiblocks.first; block; block=block->next) {
		for(but=block->buttons.first; but; but= but->next) {
			if((but->activate == data && data) || (but->activate && data == NULL)) {
				if(rblock) *rblock= block;
				return but;
			}
		}
	}

	if(rblock) *rblock= NULL;
	return NULL;
}

static uiBut *ui_but_find_signal(ARegion *ar, uiActivateBut *data, uiBlock **rblock)
{
	uiBlock *block;
	uiBut *but;

	for(block=ar->uiblocks.first; block; block=block->next) {
		for(but=block->buttons.first; but; but= but->next) {
			if(but->activateflag) {
				if(rblock) *rblock= block;
				return but;
			}
		}
	}

	if(rblock) *rblock= NULL;
	return NULL;
}

static uiBut *ui_but_find_mouse_over(ARegion *ar, int x, int y, uiBlock **rblock)
{
	uiBlock *block, *blockover= NULL;
	uiBut *but, *butover= NULL;
	int mx, my;

	for(block=ar->uiblocks.first; block; block=block->next) {
		mx= x;
		my= y;
		ui_window_to_block(ar, block, &mx, &my);

		for(but=block->buttons.first; but; but= but->next) {
			/* give precedence to already activated buttons */
			if(ui_but_contains_pt(but, mx, my)) {
				if(!butover || (!butover->activate && but->activate)) {
					butover= but;
					blockover= block;
				}
			}
		}
	}

	if(rblock)
		*rblock= blockover;

	return butover;
}

/*********************** button activate operator ***************************
 * this operator runs from the moment the mouse is position over a button and
 * handles all interaction with the button, from highlight to text editing,
 * dragging, having a block open to leaving the button to activate another. */

static void button_disable_timers(bContext *C, uiActivateBut *data)
{
	if(data->tooltiptimer) {
		WM_event_remove_window_timer(C->window, data->tooltiptimer);
		data->tooltiptimer= NULL;
	}
	if(data->tooltip) {
		ui_tooltip_free(C, data->tooltip);
		data->tooltip= NULL;
	}
	data->tooltipdisabled= 1;

	if(data->autoopentimer) {
		WM_event_remove_window_timer(C->window, data->autoopentimer);
		data->autoopentimer= NULL;
	}
}

static void button_activate_state(bContext *C, uiBut *but, uiActivateButState state)
{
	uiActivateBut *data;

	data= but->activate;
	if(data->state == state)
		return;

	if(state == BUTTON_STATE_HIGHLIGHT) {
		but->flag &= ~UI_SELECT;

		/* XXX 2.50 U missing from context */
		if(U.flag & USER_TOOLTIPS)
			if(!data->tooltiptimer && !data->tooltipdisabled)
				data->tooltiptimer= WM_event_add_window_timer(C->window, BUTTON_TOOLTIP_DELAY, ~0);

		/* automatic open pulldown block timer */
		if(but->type==BLOCK || but->type==MENU || but->type==PULLDOWN || but->type==ICONTEXTROW) {
			if(!data->autoopentimer) {
				int time;

				if(but->block->auto_open==2) time= 1;    // test for toolbox
				else if(but->block->flag & UI_BLOCK_LOOP || but->block->auto_open) time= 5*U.menuthreshold2;
				else if(U.uiflag & USER_MENUOPENAUTO) time= 5*U.menuthreshold1;
				else time= -1;

				if(time >= 0)
					data->autoopentimer= WM_event_add_window_timer(C->window, time*20, ~0);
			}
		}
	}
	else {
		but->flag |= UI_SELECT;
		button_disable_timers(C, data);
	}

	if(state == BUTTON_STATE_TEXT_EDITING && data->state != BUTTON_STATE_TEXT_SELECTING)
		ui_textedit_begin(but, data);
	else if(data->state == BUTTON_STATE_TEXT_EDITING && state != BUTTON_STATE_TEXT_SELECTING)
		ui_textedit_end(but, data);
	
	if(state == BUTTON_STATE_NUM_EDITING)
		ui_numedit_begin(but, data);
	else if(data->state == BUTTON_STATE_NUM_EDITING)
		ui_numedit_end(but, data);

	if(state == BUTTON_STATE_BLOCK_OPEN) {
		ui_blockopen_begin(C, but, data);
		/* note we move the handler to the region when the block is open,
		 * so we don't interfere with the events as long as it's open */
		WM_event_remove_modal_handler(&C->window->handlers, data->operator);
		WM_event_add_modal_handler(&data->region->handlers, data->operator);
	}
	else if(data->state == BUTTON_STATE_BLOCK_OPEN) {
		ui_blockopen_end(C, but, data);
		WM_event_remove_modal_handler(&data->region->handlers, data->operator);
		WM_event_add_modal_handler(&C->window->handlers, data->operator);
	}
	
	if(state == BUTTON_STATE_WAIT_FLASH) {
		data->flashtimer= WM_event_add_window_timer(C->window, BUTTON_FLASH_DELAY, ~0);
	}
	else if(data->flashtimer) {
		WM_event_remove_window_timer(C->window, data->flashtimer);
		data->flashtimer= NULL;
	}

	data->state= state;
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
}

static void button_activate_init(bContext *C, ARegion *ar, wmOperator *op, uiBut *but, uiBut *lastbut)
{
	uiActivateBut *data;

	/* setup struct */
	data= MEM_callocN(sizeof(uiActivateBut), "uiActivateBut");
	data->region= ar;
	data->operator= op;
	data->interactive= 0;
	data->state = BUTTON_STATE_INIT;
	op->customdata= data;

	/* activate button */
	but->flag |= UI_ACTIVE;
	but->activate= data;

	if(but == lastbut)
		data->tooltipdisabled= 1;

	/* we disable auto_open in the block after a threshold, because we still
	 * want to allow auto opening adjacent menus even if no button is activated
	 * inbetween going over to the other button, but only for a short while */
	if(!lastbut && but->block->auto_open)
		if(but->block->auto_open_last+BUTTON_AUTO_OPEN_THRESH < PIL_check_seconds_timer())
			but->block->auto_open= 0;
	
	/* modal handler */
	WM_event_add_modal_handler(&C->window->handlers, op);

	button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);

	if(but->activateflag == UI_ACTIVATE_OPEN)
		button_activate_state(C, but, BUTTON_STATE_BLOCK_OPEN);
	else if(but->activateflag == UI_ACTIVATE_TEXT_EDITING)
		button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
	else if(but->activateflag == UI_ACTIVATE_APPLY)
		button_activate_state(C, but, BUTTON_STATE_WAIT_FLASH);
	
	but->activateflag= 0;
}

static void button_activate_exit(bContext *C, uiActivateBut *data, wmOperator *op)
{
	uiBut *but;
	uiBlock *block;

	/* verify if we still have a button, can be NULL! */
	but= ui_but_find_activated(data->region, data, &block);

	/* stop text editing */
	if(data->state == BUTTON_STATE_TEXT_EDITING || data->state == BUTTON_STATE_TEXT_SELECTING) {
		data->cancel= 1;
		ui_textedit_end(but, data);
	}

	if(data->state == BUTTON_STATE_NUM_EDITING)
		ui_numedit_end(but, data);
	
	if(data->state == BUTTON_STATE_BLOCK_OPEN) {
		ui_blockopen_end(C, but, data);
		WM_event_remove_modal_handler(&data->region->handlers, data->operator);
	}
	else
		WM_event_remove_modal_handler(&C->window->handlers, op);				

	if(but) {
		/* if someone is expecting a message */
		if(but->block->handle && !(but->block->flag & UI_BLOCK_KEEP_OPEN) && !data->cancel) {
			uiMenuBlockHandle *handle;

			handle= but->block->handle;
			handle->butretval= data->retval;
			if(data->blockretval) {
				handle->blockretval= data->blockretval;

				/* if we got a cancel from a block menu, then also cancel this
				 * button, we set that here to instead of in do_BLOCK to make
				 * a distinction with cancel after lost highlight for example */
				if(data->blockretval == UI_RETURN_CANCEL)
					data->cancel= 1;
			}
			else
				handle->blockretval= UI_RETURN_OK;

			WM_event_add_message(C->wm, handle, 0);
		}

		/* apply the button action or value */
		ui_apply_button(block, but, data, 0);

		/* clean up button */
		but->activate= NULL;
		but->flag &= ~(UI_ACTIVE|UI_SELECT);
	}

	/* redraw */
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);

	/* clean up */
	button_disable_timers(C, data);

	if(data->str)
		MEM_freeN(data->str);
	if(data->origstr)
		MEM_freeN(data->origstr);
	if(data->flashtimer)
		WM_event_remove_window_timer(C->window, data->flashtimer);

	MEM_freeN(op->customdata);
	op->customdata= NULL;
}

static int button_activate_try_init(bContext *C, ARegion *ar, wmOperator *op, wmEvent *event, uiBut *lastbut)
{
	uiBut *but;

	/* try to activate a button */
	if(!ar)
		return OPERATOR_PASS_THROUGH;

	if(ui_but_find_activated(ar, NULL, NULL))
		return OPERATOR_PASS_THROUGH;

	but= ui_but_find_signal(ar, NULL, NULL);

	if(!but)
		but= ui_but_find_mouse_over(ar, event->x, event->y, NULL);

	if(lastbut && but && but!=lastbut)
		return OPERATOR_PASS_THROUGH;

	if(but && !but->activate) {
		button_activate_init(C, ar, op, but, lastbut);
		return OPERATOR_RUNNING_MODAL;
	}

	return OPERATOR_PASS_THROUGH;
}

static int button_activate_try_exit(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar;
	uiActivateBut *data;
	uiBut *but;
	int state;

	data= op->customdata;
	ar= data->region;

	but= ui_but_find_activated(data->region, data, NULL);

	/* exit the current button, but try to re-init as well */
	button_activate_exit(C, op->customdata, op);
	state= button_activate_try_init(C, ar, op, event, but);

	return (state != OPERATOR_RUNNING_MODAL);
}

static int button_activate_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return button_activate_try_init(C, C->region, op, event, NULL);
}

static int button_activate_cancel(bContext *C, wmOperator *op)
{
	uiActivateBut *data;

	data= op->customdata;
	data->cancel= 1;
	button_activate_exit(C, op->customdata, op);

	return OPERATOR_CANCELLED;
}

static int button_activate_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	uiActivateBut *data;
	uiBut *but;
	uiBlock *block;
	int handled= 0;

	data= op->customdata;

	/* check if the button dissappeared somehow */
	if(!(but= ui_but_find_activated(data->region, data, &block))) {
		data->cancel= 1;
		if(button_activate_try_exit(C, op, event))
			return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
		else
			return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
	}

	if(data->state == BUTTON_STATE_HIGHLIGHT) {
		switch(event->type) {
			case MOUSEMOVE:
				/* verify if we are still over the button, if not exit */
				but= ui_but_find_mouse_over(data->region, event->x, event->y, &block);

				if(!but || but->activate != data) {
					data->cancel= 1;
					if(button_activate_try_exit(C, op, event))
						return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
					else
						return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
				}
				break;
			case TIMER: {
				/* handle tooltip timer */
				if(event->customdata == data->tooltiptimer) {
					if(!data->tooltip) {
						data->tooltip= ui_tooltip_create(C, data->region, but);
						WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
					}

					WM_event_remove_window_timer(C->window, data->tooltiptimer);
					data->tooltiptimer= NULL;
				}
				else if(event->customdata == data->autoopentimer) {
					button_activate_state(C, but, BUTTON_STATE_BLOCK_OPEN);

					WM_event_remove_window_timer(C->window, data->autoopentimer);
					data->autoopentimer= NULL;
				}

				break;
			}
		}

		handled= ui_do_button(C, block, but, event);
	}
	else if(data->state == BUTTON_STATE_WAIT_RELEASE) {
		switch(event->type) {
			case MOUSEMOVE:
				/* deselect the button when moving the mouse away */
				but= ui_but_find_mouse_over(data->region, event->x, event->y, &block);

				if(but && but->activate == data) {
					if(!(but->flag & UI_SELECT)) {
						but->flag |= UI_SELECT;
						WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
					}
				}
				else {
					but= ui_but_find_activated(data->region, data, &block);
					if(but->flag & UI_SELECT) {
						but->flag &= ~UI_SELECT;
						WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
					}
				}
				break;
		}

		ui_do_button(C, block, but, event);
		handled= 1;
	}
	else if(data->state == BUTTON_STATE_WAIT_FLASH) {
		switch(event->type) {
			case TIMER: {
				if(event->customdata == data->flashtimer)
					button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}
	else if(data->state == BUTTON_STATE_BLOCK_OPEN) {
		switch(event->type) {
			case MOUSEMOVE: {
				uiBut *bt= ui_but_find_mouse_over(data->region, event->x, event->y, &block);

				if(bt && bt->activate != data) {
					data->cancel= 1;
					if(button_activate_try_exit(C, op, event))
						return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
					else
						return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
				}
				break;
			}
		}

		ui_do_button(C, block, but, event);
		handled= 0;
	}
	else {
		ui_do_button(C, block, but, event);
		handled= 1;
	}

	if(data->state == BUTTON_STATE_EXIT) {
		if(button_activate_try_exit(C, op, event))
			return OPERATOR_CANCELLED|(handled==0? OPERATOR_PASS_THROUGH: 0);
		else
			return OPERATOR_RUNNING_MODAL|(handled==0? OPERATOR_PASS_THROUGH: 0);
	}

	if(handled)
		return OPERATOR_RUNNING_MODAL;
	else
		return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
}

static int button_activate_poll(bContext *C)
{
	if(C->region==NULL) return 0;
	if(C->region->uiblocks.first==NULL) return 0;
	return 1;
}

void ED_UI_OT_button_activate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Button Highlight";
	ot->idname= "ED_UI_OT_button_activate";
	
	/* api callbacks */
	ot->invoke= button_activate_invoke;
	ot->cancel= button_activate_cancel;
	ot->modal= button_activate_modal;
	ot->poll= button_activate_poll;
}

/* ******************** menu navigation helpers ************** */

static uiBut *ui_but_prev(uiBut *but)
{
	while(but->prev) {
		but= but->prev;
		if(but->type!=LABEL && but->type!=SEPR && but->type!=ROUNDBOX) return but;
	}
	return NULL;
}

static uiBut *ui_but_next(uiBut *but)
{
	while(but->next) {
		but= but->next;
		if(but->type!=LABEL && but->type!=SEPR && but->type!=ROUNDBOX) return but;
	}
	return NULL;
}

static uiBut *ui_but_first(uiBlock *block)
{
	uiBut *but;
	
	but= block->buttons.first;
	while(but) {
		if(but->type!=LABEL && but->type!=SEPR && but->type!=ROUNDBOX) return but;
		but= but->next;
	}
	return NULL;
}

static uiBut *ui_but_last(uiBlock *block)
{
	uiBut *but;
	
	but= block->buttons.last;
	while(but) {
		if(but->type!=LABEL && but->type!=SEPR && but->type!=ROUNDBOX) return but;
		but= but->prev;
	}
	return NULL;
}

/* ********************** menu navigate operator *****************************
 * this operator is invoked when the block/region is opened and cancelled when
 * it closes, it blocks nearly all events. this operator will not close the
 * actual block, instead this must be done by the button activate operator when
 * it receives the result message */

typedef struct uiBlockHandle {
	ARegion *region;

	int towardsx, towardsy;
	double towardstime;
	int dotowards;
} uiBlockHandle;

static int menu_block_handle_cancel(bContext *C, wmOperator *op)
{
	if(op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata= NULL;
		WM_event_remove_modal_handler(&C->window->handlers, op);				
	}

	return OPERATOR_CANCELLED;
}

static void menu_block_handle_return(bContext *C, wmOperator *op, uiBlock *block, int retval)
{
	uiMenuBlockHandle *handle;

	handle= block->handle;
	handle->blockretval= retval;
	handle->butretval= 0;

	WM_event_add_message(C->wm, handle, 0);
	menu_block_handle_cancel(C, op);
}

static int menu_block_handle_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiMenuBlockHandle *handle;

	handle= MEM_callocN(sizeof(uiBlockHandle), "uiBlockHandle");
	handle->region= C->region;

	op->customdata= handle;
	WM_event_add_modal_handler(&C->window->handlers, op);

	return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
}

static int menu_block_handle_block_open(uiBlock *block)
{
	uiBut *but;
	uiActivateBut *data;

	for(but=block->buttons.first; but; but=but->next) {
		data= but->activate;

		if(data && data->state == BUTTON_STATE_BLOCK_OPEN)
			return 1;
	}

	return 0;
}

static void menu_block_handle_activate_button(bContext *C, wmEvent *event, ARegion *butregion, uiBut *but, int activateflag)
{
	wmOperatorType *ot;

	ot= WM_operatortype_find("ED_UI_OT_button_activate");

	WM_operator_cancel(C, &butregion->modalops, ot);
	but->activateflag= activateflag;

	SWAP(ARegion*, C->region, butregion); /* XXX 2.50 bad state manipulation? */
	WM_operator_invoke(C, ot, event);
	SWAP(ARegion*, C->region, butregion);
}

/* function used to prevent loosing the open menu when using nested pulldowns,
 * when moving mouse towards the pulldown menu over other buttons that could
 * steal the highlight from the current button, only checks:
 *
 * - while mouse moves in triangular area defined old mouse position and
 *   left/right side of new menu
 * - only for 1 second
 */

static void ui_mouse_motion_towards_init(uiBlockHandle *bhandle, int mx, int my)
{
	if(!bhandle->dotowards) {
		bhandle->dotowards= 1;
		bhandle->towardsx= mx;
		bhandle->towardsy= my;
		bhandle->towardstime= PIL_check_seconds_timer();
	}
}

static int ui_mouse_motion_towards_check(uiBlock *block, uiBlockHandle *bhandle, int mx, int my)
{
	int fac, dx, dy, domx, domy;

	if(!bhandle->dotowards) return 0;
	if((block->direction & UI_TOP) || (block->direction & UI_DOWN)) {
		bhandle->dotowards= 0;
		return bhandle->dotowards;
	}
	
	/* calculate dominant direction */
	domx= (-bhandle->towardsx + (block->maxx+block->minx)/2);
	domy= (-bhandle->towardsy + (block->maxy+block->miny)/2);

	/* we need some accuracy */
	if(abs(domx) < 4) {
		bhandle->dotowards= 0;
		return bhandle->dotowards;
	}
	
	/* check direction */
	dx= mx - bhandle->towardsx;
	dy= my - bhandle->towardsy;
	
	/* threshold */
	if(abs(dx)+abs(dy) > 4) {
		/* menu to right */
		if(domx>0) {
			fac= (mx - bhandle->towardsx)*( bhandle->towardsy - (int)(block->maxy+20)) +
			     (my - bhandle->towardsy)*(-bhandle->towardsx + (int)block->minx);
			if(fac>0) bhandle->dotowards= 0;
			
			fac= (mx - bhandle->towardsx)*( bhandle->towardsy - (int)(block->miny-20)) +
			     (my - bhandle->towardsy)*(-bhandle->towardsx + (int)block->minx);
			if(fac<0) bhandle->dotowards= 0;
		}
		else {
			fac= (mx - bhandle->towardsx)*( bhandle->towardsy - (int)(block->maxy+20)) +
			     (my - bhandle->towardsy)*(-bhandle->towardsx + (int)block->maxx);
			if(fac<0) bhandle->dotowards= 0;
			
			fac= (mx - bhandle->towardsx)*( bhandle->towardsy - (int)(block->miny-20)) +
			     (my - bhandle->towardsy)*(-bhandle->towardsx + (int)block->maxx);
			if(fac>0) bhandle->dotowards= 0;
		}
	}

	/* 1 second timer */
	if(PIL_check_seconds_timer() - bhandle->towardstime > BUTTON_MOUSE_TOWARDS_THRESH)
		bhandle->dotowards= 0;

	return bhandle->dotowards;
}

static int menu_block_handle_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar;
	uiBlock *block;
	uiBut *but, *bt;
	uiBlockHandle *bhandle;
	int inside, act, count, mx, my, handled;

	bhandle= op->customdata;
	ar= bhandle->region;
	block= ar->uiblocks.first;

	act= 0;
	handled= 0;

	/* if we have another block opened, we shouldn't do anything
	 * until that block is closed again, otherwise we interfere */
	if(menu_block_handle_block_open(block))
		return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;

	mx= event->x;
	my= event->y;
	ui_window_to_block(ar, block, &mx, &my);

	/* for ui_mouse_motion_towards_block */
	if(event->type == MOUSEMOVE)
		ui_mouse_motion_towards_init(bhandle, mx, my);

	/* check if mouse is inside block */
	inside= 0;
	if(block->minx <= mx && block->maxx >= mx)
		if(block->miny <= my && block->maxy >= my)
			inside= 1;

	switch(event->type) {
		/* closing sublevels of pulldowns */
		case LEFTARROWKEY:
			if(event->val && (block->flag & UI_BLOCK_LOOP))
				if(BLI_countlist(&block->saferct) > 0)
					menu_block_handle_return(C, op, block, UI_RETURN_OUT);

			handled= 1;
			break;
			
		/* opening sublevels of pulldowns */
		case RIGHTARROWKEY:	
			if(event->val && (block->flag & UI_BLOCK_LOOP)) {
				but= ui_but_find_activated(ar, NULL, NULL);

				if(!but) {
					/* no item active, we make first active */
					if(block->direction & UI_TOP) but= ui_but_last(block);
					else but= ui_but_first(block);
				}

				if(but && but->type==BLOCK)
					menu_block_handle_activate_button(C, event, ar, but, UI_ACTIVATE_OPEN);
			}
			handled= 1;
			break;
		
		case UPARROWKEY:
		case DOWNARROWKEY:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			/* arrowkeys: only handle for block_loop blocks */
			if(inside || (block->flag & UI_BLOCK_LOOP)) {
				if(event->val) {
					but= ui_but_find_activated(ar, NULL, NULL);

					if(but) {
						if(ELEM(event->type, UPARROWKEY, WHEELUPMOUSE)) {
							if(block->direction & UI_TOP) but= ui_but_next(but);
							else but= ui_but_prev(but);
						}
						else {
							if(block->direction & UI_TOP) but= ui_but_prev(but);
							else but= ui_but_next(but);
						}

						if(but)
							menu_block_handle_activate_button(C, event, ar, but, UI_ACTIVATE);
					}

					if(!but) {
						if(ELEM(event->type, UPARROWKEY, WHEELUPMOUSE)) {
							if(block->direction & UI_TOP) bt= ui_but_first(block);
							else bt= ui_but_last(block);
						}
						else {
							if(block->direction & UI_TOP) bt= ui_but_last(block);
							else bt= ui_but_first(block);
						}

						if(bt)
							menu_block_handle_activate_button(C, event, ar, bt, UI_ACTIVATE);
					}
				}
			}
			handled= 1;
			break;

		case ONEKEY: 	case PAD1: 
			act= 1;
		case TWOKEY: 	case PAD2: 
			if(act==0) act= 2;
		case THREEKEY: 	case PAD3: 
			if(act==0) act= 3;
		case FOURKEY: 	case PAD4: 
			if(act==0) act= 4;
		case FIVEKEY: 	case PAD5: 
			if(act==0) act= 5;
		case SIXKEY: 	case PAD6: 
			if(act==0) act= 6;
		case SEVENKEY: 	case PAD7: 
			if(act==0) act= 7;
		case EIGHTKEY: 	case PAD8: 
			if(act==0) act= 8;
		case NINEKEY: 	case PAD9: 
			if(act==0) act= 9;
		case ZEROKEY: 	case PAD0: 
			if(act==0) act= 10;
		
			if(block->flag & UI_BLOCK_NUMSELECT) {
				if(event->alt) act+= 10;
				
				count= 0;
				for(but= block->buttons.first; but; but= but->next) {
					int doit= 0;
					
					if(but->type!=LABEL && but->type!=SEPR)
						count++;

					/* exception for menus like layer buts, with button aligning they're not drawn in order */
					if(but->type==TOGR) {
						if(but->bitnr==act-1)
							doit= 1;
					}
					else if(count==act)
						doit=1;
					
					if(doit) {
						menu_block_handle_activate_button(C, event, ar, but, UI_ACTIVATE_APPLY);
						break;
					}
				}
			}
			handled= 1;
			break;
	}

	/* here we check return conditions for menus */
	if(block->flag & UI_BLOCK_LOOP) {
		/* if we click outside the block, verify if we clicked on the
		 * button that opened us, otherwise we need to close */
		if(inside==0) {
			uiSafetyRct *saferct= block->saferct.first;

			if(ELEM3(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE) && event->val) {
				if(saferct && !BLI_in_rctf(&saferct->parent, event->x, event->y)) {
					menu_block_handle_return(C, op, block, UI_RETURN_OK);
					return OPERATOR_RUNNING_MODAL;
				}
			}
		}

		/* esc cancel this and all preceding menus */
		if(event->type==ESCKEY && event->val) {
			menu_block_handle_return(C, op, block, UI_RETURN_CANCEL);
			return OPERATOR_RUNNING_MODAL;
		}

		if(ELEM(event->type, RETKEY, PADENTER) && event->val) {
			/* enter will always close this block, but note that the event
			 * can still get pass through so the button is executed */
			menu_block_handle_return(C, op, block, UI_RETURN_OK);
			handled= 1;
		}
		else {
			ui_mouse_motion_towards_check(block, bhandle, mx, my);

			/* check mouse moving outside of the menu */
			if(inside==0 && (block->flag & UI_BLOCK_MOVEMOUSE_QUIT)) {
				uiSafetyRct *saferct;
				
				/* check for all parent rects, enables arrowkeys to be used */
				for(saferct=block->saferct.first; saferct; saferct= saferct->next) {
					/* for mouse move we only check our own rect, for other
					 * events we check all preceding block rects too to make
					 * arrow keys navigation work */
					if(event->type!=MOUSEMOVE || saferct==block->saferct.first) {
						if(BLI_in_rctf(&saferct->parent, (float)event->x, (float)event->y))
							break;
						if(BLI_in_rctf(&saferct->safety, (float)event->x, (float)event->y))
							break;
					}
				}

				/* strict check, and include the parent rect */
				if(!bhandle->dotowards && !saferct) {
					menu_block_handle_return(C, op, block, UI_RETURN_OK);
					return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
				}

				if(bhandle->dotowards && event->type==MOUSEMOVE)
					handled= 1;
			}
		}
	}

	/* we pass on events only when the mouse pointer is inside, this ensures
	 * events only go to handlers inside this region, and no other region.
	 * that's an indirect way to get this behavior, maybe it should be done
	 * in a more explicit way somewhow. */
	if(inside && !handled)
		return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
	else
		return OPERATOR_RUNNING_MODAL;
}

static int menu_block_handle_poll(bContext *C)
{
	uiBlock *block;

	if(C->region==NULL) return 0;
	block= C->region->uiblocks.first;
	if(!block) return 0;

	return 1;
}

void ED_UI_OT_menu_block_handle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Menu Block Handle";
	ot->idname= "ED_UI_OT_menu_block_handle";
	
	/* api callbacks */
	ot->invoke= menu_block_handle_invoke;
	ot->modal= menu_block_handle_modal;
	ot->cancel= menu_block_handle_cancel;
	ot->poll= menu_block_handle_poll;
}

/* ************************** registration **********************************/

void ui_operatortypes(void)
{
	WM_operatortype_append(ED_UI_OT_button_activate);
	WM_operatortype_append(ED_UI_OT_menu_block_handle);
}

void UI_keymap(wmWindowManager *wm)
{
	ui_operatortypes();

	WM_keymap_add_item(&wm->uikeymap, "ED_UI_OT_button_activate", MOUSEMOVE, 0, 0, 0);
}

