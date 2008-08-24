/**
 * header_text.c oct-2003
 *
 * Functions to draw the "Text Editor" window header
 * and handle user events sent to it.
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BMF_Api.h"
#include "BIF_language.h"
#include "MEM_guardedalloc.h"

#include "BSE_headerbuttons.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_constraint_types.h"
#include "DNA_action_types.h"

#include "BIF_gl.h" /* for glRasterPos2i */
#include "BIF_drawtext.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_sca.h"
#include "BKE_text.h"
#include "BKE_depsgraph.h"

#include "BSE_filesel.h"

#include "BPY_extern.h"
#include "BPY_menus.h"

#include "blendef.h"
#include "mydevice.h"

void do_text_buttons(unsigned short event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	ID *id, *idtest;
	int nr= 1;
	Text *text;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	switch (event) {
	case B_TEXTBROWSE:
		if (st->menunr==-2) {
			activate_databrowse((ID *)st->text, ID_TXT, 0, B_TEXTBROWSE,
											&st->menunr, do_text_buttons);
			break;
		}
		if(st->menunr < 0) break;
			
		text= st->text;

		nr= 1;
		id= (ID *)text;
		
		if (st->menunr==32767) {
			st->text= (Text *)add_empty_text( "Text" );

			st->top= 0;
			
			allqueue(REDRAWTEXT, 0);
			allqueue(REDRAWHEADERS, 0); 
		}
		else if (st->menunr==32766) {
			activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, add_text_fs); 
			return;
		}
		else {		
			idtest= G.main->text.first;
			while(idtest) {
				if(nr==st->menunr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) { /* new text */
				activate_fileselect(FILE_SPECIAL, "Open Text File",
												G.sce, add_text_fs); 
				return;
			}
			if(idtest!=id) {
				st->text= (Text *)idtest;
				st->top= 0;
				
				pop_space_text(st);
				if (st->showsyntax) txt_format_text(st);
				allqueue(REDRAWTEXT, 0);
				allqueue(REDRAWHEADERS, 0);
			}
		}
		break;
		
	case B_TEXTDELETE:
		{
			text= st->text;
			if (!text) return;
			
			/* make the previous text active, if its not there make the next text active */
			if (st->text->id.prev) {
				st->text = st->text->id.prev;
				pop_space_text(st);
			} else if (st->text->id.next) {
				st->text = st->text->id.next;
				pop_space_text(st);
			}
			
			BPY_clear_bad_scriptlinks(text);
			BPY_free_pyconstraint_links(text);
			free_text_controllers(text);
			
			unlink_text(text);
			free_libblock(&G.main->text, text);
			
			allqueue(REDRAWTEXT, 0);
			allqueue(REDRAWHEADERS, 0);
			
			/*for if any object constraints were changed.*/
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWBUTSEDIT, 0);
			
			BIF_undo_push("Delete Text");
		}
		break;
		
/*
	case B_TEXTSTORE:
		st->text->flags ^= TXT_ISEXT;
		
		allqueue(REDRAWHEADERS, 0);
		break;
*/		 
	case B_TEXTLINENUM:
		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);
		break;

	case B_TEXTFONT:
		switch(st->font_id) {
		case 0:
			st->lheight= 12; break;
		case 1:
			st->lheight= 15; 
			break;
		}
			
		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);

		break;
	case B_TAB_NUMBERS:
		if (st->showsyntax) txt_format_text(st);
		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);
		break;
	case B_SYNTAX:
		if (st->showsyntax) txt_format_text(st);
		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);
		break;
	case B_TEXTPLUGINS:
		allqueue(REDRAWHEADERS, 0);
		break;
	case B_WORDWRAP:
		st->left= 0;
		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);
		break;
	}
}

static void do_text_template_scriptsmenu(void *arg, int event)
{
	BPY_menu_do_python(PYMENU_SCRIPTTEMPLATE, event);
	
	allqueue(REDRAWIMAGE, 0);
}

static uiBlock *text_template_scriptsmenu (void *args_unused)
{
	uiBlock *block;
	BPyMenu *pym;
	int i= 0;
	short yco = 20, menuwidth = 120;
	
	block= uiNewBlock(&curarea->uiblocks, "text_template_scriptsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_template_scriptsmenu, NULL);
	
	/* note that we acount for the N previous entries with i+20: */
	for (pym = BPyMenuTable[PYMENU_SCRIPTTEMPLATE]; pym; pym = pym->next, i++) {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, 
						 NULL, 0.0, 0.0, 1, i, 
						 pym->tooltip?pym->tooltip:pym->filename);
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

static void do_text_plugin_scriptsmenu(void *arg, int event)
{
	BPY_menu_do_python(PYMENU_TEXTPLUGIN, event);
	
	allqueue(REDRAWIMAGE, 0);
}

static uiBlock *text_plugin_scriptsmenu (void *args_unused)
{
	uiBlock *block;
	BPyMenu *pym;
	int i= 0;
	short yco = 20, menuwidth = 120;
	
	block= uiNewBlock(&curarea->uiblocks, "text_plugin_scriptsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_plugin_scriptsmenu, NULL);
	
	/* note that we acount for the N previous entries with i+20: */
	for (pym = BPyMenuTable[PYMENU_TEXTPLUGIN]; pym; pym = pym->next, i++) {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, 
						 NULL, 0.0, 0.0, 1, i, 
						 pym->tooltip?pym->tooltip:pym->filename);
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

/* action executed after clicking in File menu */
static void do_text_filemenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	Text *text;
	ScrArea *sa;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	text= st->text;
	
	switch(event) {
	case 1:
		st->text= add_empty_text( "Text" );
		st->top=0;
		
		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);
		break;
	case 2:
		activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, add_text_fs);
		break;
	case 3:
		if (text->compiled) BPY_free_compiled_text(text);
		        text->compiled = NULL;
			if (okee("Reopen Text")) {
				if (!reopen_text(text)) {
					error("Could not reopen file");
				}
				if (st->showsyntax) txt_format_text(st);
			}
		break;
	case 5:
		text->flags |= TXT_ISMEM;
	case 4:
		txt_write_file(text);
		break;
	case 6:
		text->flags |= TXT_ISMEM | TXT_ISDIRTY | TXT_ISTMP;
		MEM_freeN(text->name);
		text->name= NULL;
		break;
	case 7:
		run_python_script(st);
		break;
	case 8:
	{
		Object *ob;
		bConstraint *con;
		short update;
		
		/* check all pyconstraints */
		for (ob= G.main->object.first; ob; ob= ob->id.next) {
			update = 0;
			if (ob->type==OB_ARMATURE && ob->pose) {
				bPoseChannel *pchan;
				for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
					for (con = pchan->constraints.first; con; con= con->next) {
						if (con->type==CONSTRAINT_TYPE_PYTHON) {
							bPythonConstraint *data = con->data;
							if (data->text==text) BPY_pyconstraint_update(ob, con);
							update = 1;
							
						}
					}
				}
			}
			for (con = ob->constraints.first; con; con= con->next) {
				if (con->type==CONSTRAINT_TYPE_PYTHON) {
					bPythonConstraint *data = con->data;
					if (data->text==text) BPY_pyconstraint_update(ob, con);
					update = 1;
				}
			}
			
			if (update) {
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			}
		}
	}
		break;
	default:
		break;
	}
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* action executed after clicking in Edit menu */
static void do_text_editmenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	Text *text;
	ScrArea *sa;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	text= st->text;
	
	switch(event) {
	case 1:
		txt_do_undo(text);
		pop_space_text(st);
		break;
	case 2:
		txt_do_redo(text);
		pop_space_text(st);
		break;
	case 3:
		if (text && text->id.lib) {
			error_libdata();
			break;
		}
		txt_copy_clipboard(text);
		txt_cut_sel(text);
		pop_space_text(st);
		break;
	case 4:
		//txt_copy_sel(text);
		txt_copy_clipboard(text);
		break;
	case 5:
		if (text && text->id.lib) {
			error_libdata();
			break;
		}
		txt_paste_clipboard(text);
		if (st->showsyntax) txt_format_text(st);
		break;
	case 6:
		txt_print_cutbuffer();
		break;
	case 7:
		jumptoline_interactive(st);
		break;
	case 8:
	case 9:
		find_and_replace(st, 0);
		break;
	case 10:
		find_and_replace(st, 1);
		break;
	default:
		break;
	}

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* action executed after clicking in View menu */
static void do_text_editmenu_viewmenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	Text *text;
	ScrArea *sa;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	text = st->text;
	
	switch(event) {
		case 1:
			txt_move_bof(text, 0);
			pop_space_text(st);
			break;
		case 2:
			txt_move_eof(text, 0);
			pop_space_text(st);
			break;
		default:
			break;
	}

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* action executed after clicking in Select menu */
static void do_text_editmenu_selectmenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	Text *text;
	ScrArea *sa;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	text = st->text;
	
	switch(event) {
	case 1:
		txt_sel_all(text);
		break;		
	case 2:
		txt_sel_line(text);
		break;
	default:
		break;
	}

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* action executed after clicking in Markers menu */
static void do_text_editmenu_markermenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	Text *text;
	TextMarker *mrk;
	ScrArea *sa;
	int lineno;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	text = st->text;
	
	switch(event) {
	case 1:
		txt_clear_markers(text, 0, 0);
		break;
	case 2:
		lineno= txt_get_span(text->lines.first, text->curl);
		mrk= text->markers.first;
		while (mrk && (mrk->lineno<lineno || (mrk->lineno==lineno && mrk->start <= text->curc)))
			mrk= mrk->next;
		if (!mrk) mrk= text->markers.first;
		if (mrk) {
			txt_move_to(text, mrk->lineno, mrk->start, 0);
			txt_move_to(text, mrk->lineno, mrk->end, 1);
		}
		break;
	case 3:
		lineno= txt_get_span(text->lines.first, text->curl);
		mrk= text->markers.last;
		while (mrk && (mrk->lineno>lineno || (mrk->lineno==lineno && mrk->end > text->curc)))
			mrk= mrk->prev;
		if (!mrk) mrk= text->markers.last;
		if (mrk) {
			txt_move_to(text, mrk->lineno, mrk->start, 0);
			txt_move_to(text, mrk->lineno, mrk->end, 1);
		}
		break;
	default:
		break;
	}

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* action executed after clicking in Format menu */
static void do_text_formatmenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	Text *text;
	ScrArea *sa;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	text = st->text;
	
	switch(event) {
	case 3:
		if (text && text->id.lib) {
			error_libdata();
			break;
		}
		if (txt_has_sel(text)) {
			txt_order_cursors(text);
			indent(text);
			break;
		}
		else {
			txt_add_char(text, '\t');
			break;
		}
	case 4:
		if (text && text->id.lib) {
			error_libdata();
			break;
		}
		if ( txt_has_sel(text)) {
			txt_order_cursors(text);
			unindent(text);
			break;
		}
		break;
	case 5:
		if (text && text->id.lib) {
			error_libdata();
			break;
		}
		if ( txt_has_sel(text)) {
			txt_order_cursors(text);
			comment(text);
			if (st->showsyntax) txt_format_text(st);
			break;
		}
		break;
	case 6:
		if (text && text->id.lib) {
			error_libdata();
			break;
		}
		if ( txt_has_sel(text)) {
			txt_order_cursors(text);
			uncomment(text);
			if (st->showsyntax) txt_format_text(st);
			break;
		}
		break;
	default:
		break;
	}

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* View menu */
static uiBlock *text_editmenu_viewmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "text_editmenu_viewmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_editmenu_viewmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Top of File", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bottom of File", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

/* Select menu */
static uiBlock *text_editmenu_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "text_editmenu_selectmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_editmenu_selectmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select All|Ctrl A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select Line", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

/* Select menu */
static uiBlock *text_editmenu_markermenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "text_editmenu_markermenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_editmenu_markermenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear All", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Next Marker", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Previous Marker", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

void do_text_formatmenu_convert(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	switch(event) {
	case 1: convert_tabs(st, 0); break;
	case 2: convert_tabs(st, 1); break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *text_formatmenu_convert(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "do_text_formatmenu_convert", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_formatmenu_convert, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "To Spaces",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "Converts script whitespace to spaces based on Tab:");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "To Tabs",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "Converts script whitespace to tabs based on Tab:");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

/* Format menu */
static uiBlock *text_formatmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "text_formatmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_text_formatmenu, NULL);

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Indent|Tab", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unindent|Shift Tab", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Comment", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Uncomment|Ctrl Shift D", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBlockBut(block, text_formatmenu_convert, NULL, ICON_RIGHTARROW_THIN, "Convert whitespace", 0, yco-=20, menuwidth, 19, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}


/* action executed after clicking in Object to 3d Sub Menu */
void do_text_editmenu_to3dmenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	Text *text;
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	text = st->text;
	
	switch(event) {
	case 1: txt_export_to_object(text); break;
	case 2: txt_export_to_objects(text); break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

/* Object to 3d Sub Menu */
static uiBlock *text_editmenu_to3dmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "do_text_editmenu_to3dmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_editmenu_to3dmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "One Object | Alt-M",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "One Object Per Line",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}


/* Edit menu */
static uiBlock *text_editmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "text_editmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_text_editmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo|Ctrl Z", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Redo|Ctrl Shift Z", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cut|Alt X", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy|Alt C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Paste|Alt V", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Print Cut Buffer", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBlockBut(block, text_editmenu_viewmenu, NULL, ICON_RIGHTARROW_THIN, "View|Alt Shift V   ", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, text_editmenu_selectmenu, NULL, ICON_RIGHTARROW_THIN, "Select|Alt Shift S   ", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, text_editmenu_markermenu, NULL, ICON_RIGHTARROW_THIN, "Markers", 0, yco-=20, 120, 19, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump...|Alt J", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find And Replace...|Alt F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find Next|Alt F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Replace|Alt H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBlockBut(block, text_editmenu_to3dmenu, NULL, ICON_RIGHTARROW_THIN, "Text to 3d Object", 0, yco-=20, 120, 19, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

/* File menu */
static uiBlock *text_filemenu(void *arg_unused)
{
	SpaceText *st= curarea->spacedata.first; /* bad but cant pass as an arg here */
	Text *text= st->text;
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "text_filemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_text_filemenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "New|Alt N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Open...|Alt O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	
	if(text) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reopen|Alt R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save|Alt S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save As...", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
		
		if (text->name)
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Internal", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");

		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Run Python Script|Alt P", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
		
		if (BPY_is_pyconstraint(text))
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Refresh All PyConstraints", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
			
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	}
	
	uiDefIconTextBlockBut(block, text_template_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Script Templates", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, text_plugin_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Text Plugins", 0, yco-=20, 120, 19, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

/* header */
#define PATH_MAX	260
void text_buttons(void)
{
	uiBlock *block;
	SpaceText *st= curarea->spacedata.first;
	Text *text;
	short xco, xmax;
	char naam[256], fname[PATH_MAX], headtxt[PATH_MAX+17];
	int len;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	text = st->text;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_TEXT;

	xco = 8;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");
	xco+= XIC+14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if(curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Enables display of pulldown menus");
	} else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hides pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	/* pull down menus */
	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("File");
		uiDefPulldownBut(block,text_filemenu, NULL, "File", xco, 0, xmax, 20, "");
		xco+=xmax;
	
		if(text) {
			xmax= GetButStringLength("Edit");
			uiDefPulldownBut(block,text_editmenu, NULL, "Edit", xco, 0, xmax, 20, "");
			xco+=xmax;
			
			xmax= GetButStringLength("Format");
			uiDefPulldownBut(block,text_formatmenu, NULL, "Format", xco, 0, xmax, 20, "");
			xco+=xmax;
		}
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco += 10;
	
	/* FULL WINDOW */
	uiBlockBeginAlign(block);
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
		
	uiDefIconButI(block, ICONTOG, B_TEXTLINENUM, ICON_LONGDISPLAY, xco+=XIC,0,XIC,YIC, &st->showlinenrs, 0, 0, 0, 0, "Displays line numbers");
	uiDefIconButI(block, ICONTOG, B_WORDWRAP, ICON_WORDWRAP, xco+=XIC,0,XIC,YIC, &st->wordwrap, 0, 0, 0, 0, "Enables word wrap");
	uiDefIconButI(block, ICONTOG, B_SYNTAX, ICON_SYNTAX, xco+=XIC,0,XIC,YIC, &st->showsyntax, 0, 0, 0, 0, "Enables syntax highlighting");
	uiDefIconButI(block, ICONTOG, B_TEXTPLUGINS, ICON_PYTHON, xco+=XIC,0,XIC,YIC, &st->doplugins, 0, 0, 0, 0, "Enables Python text plugins");
	uiBlockEndAlign(block);
	
	/* STD TEXT BUTTONS */
	xco+= 2*XIC;
	xco= std_libbuttons(block, xco, 0, 0, NULL, B_TEXTBROWSE, ID_TXT, 0, (ID*)st->text, 0, &(st->menunr), 0, 0, B_TEXTDELETE, 0, 0);

	/*
	if (st->text) {
		if (st->text->flags & TXT_ISDIRTY && (st->text->flags & TXT_ISEXT || !(st->text->flags & TXT_ISMEM)))
			uiDefIconBut(block, BUT,0, ICON_ERROR, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "The text has been changed");
		if (st->text->flags & TXT_ISEXT) 
			uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Stores text in project file");
		else 
			uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Disables storing of text in project file");
		xco+=10;
	}
	*/		

	xco+=XIC;
	if(st->font_id>1) st->font_id= 0;
	uiDefButI(block, MENU, B_TEXTFONT, "Screen 12 %x0|Screen 15%x1", xco,0,100,YIC, &st->font_id, 0, 0, 0, 0, "Displays available fonts");
	xco+=110;
	
	uiDefButI(block, NUM, B_TAB_NUMBERS, "Tab:", xco, 0, XIC+50, YIC, &st->tabnumber, 2, 8, 0, 0, "Set spacing of Tab");
	xco+= XIC+50;

	/* File info */
	if (text) {
		if (text->name) {
			len = strlen(text->name);
			if (len > PATH_MAX-1)
				len = PATH_MAX-1;
			strncpy(fname, text->name, len);
			fname[len]='\0';
			if (text->flags & TXT_ISDIRTY)
				sprintf(headtxt, "File: *%s (unsaved)", fname);
			else
				sprintf(headtxt, "File: %s", fname);
		} else {
			sprintf(headtxt, text->id.lib?"Text: External":"Text: Internal");
		}
		BIF_ThemeColor(TH_MENU_TEXT);
		glRasterPos2i(xco+=XIC, 5);
		BMF_DrawString(G.font, headtxt);
		xco += BMF_GetStringWidth(G.font, headtxt);
	}

	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
