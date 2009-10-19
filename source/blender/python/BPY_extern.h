/*
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
 * The Original Code was in: source/blender/bpython/include/BPY_extern.h
 *
 * Contributor(s): Michel Selten, Willian P. Germano, Chris Keith
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BPY_EXTERN_H
#define BPY_EXTERN_H

extern char bprogname[];	/* holds a copy of argv[0], from creator.c */
extern char btempdir[];		/* use this to store a valid temp directory */

struct Text; /* defined in DNA_text_types.h */
struct ID; /* DNA_ID.h */
struct Object; /* DNA_object_types.h */
struct ChannelDriver; /* DNA_anim_types.h */
struct ListBase; /* DNA_listBase.h */
struct SpaceText; /* DNA_space_types.h */
struct SpaceScript; /* DNA_space_types.h */
struct ScrArea; /* DNA_screen_types.h */
struct bScreen; /* DNA_screen_types.h */
struct bConstraint; /* DNA_constraint_types.h */
struct bPythonConstraint; /* DNA_constraint_types.h */
struct bConstraintOb; /* DNA_constraint_types.h */
struct bConstraintTarget; /* DNA_constraint_types.h*/
struct Script;				/* DNA_screen_types.h */
struct BPyMenu;
struct bContext;
struct ReportList;

#ifdef __cplusplus
extern "C" {
#endif

	/*These two next functions are important for making sure the Draw module
	  works correctly.  Before calling any gui callback using the Draw module,
	  the following code must be executed:
	  
		if (some_drawspace_pylist) {
			BPy_Set_DrawButtonsList(some_drawspace_pylist->but_refs);
			BPy_Free_DrawButtonsList();
		}
		some_drawspace_pylist = PyList_New(0);
		BPy_Set_DrawButtonsList(some_drawspace_pylist);

      Also, BPy_Free_DrawButtonsList() must be called as necassary when a drawspace
      with python callbacks is destroyed.
      
      This is necassary to avoid blender buttons storing invalid pointers to freed
      python data.*/
	void BPy_Set_DrawButtonsList(void *list);
	void BPy_Free_DrawButtonsList(void);
	
	void BPY_pyconstraint_eval(struct bPythonConstraint *con, struct bConstraintOb *cob, struct ListBase *targets);
	void BPY_pyconstraint_settings(void *arg1, void *arg2);
	void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct);
	void BPY_pyconstraint_update(struct Object *owner, struct bConstraint *con);
	int BPY_is_pyconstraint(struct Text *text);
	void BPY_free_pyconstraint_links(struct Text *text);
	
	void BPY_start_python( int argc, char **argv );
	void BPY_end_python( void );
	void BPY_post_start_python( void );
	void init_syspath( int first_time );
	void syspath_append( char *dir );
	void BPY_rebuild_syspath( void );
	int BPY_path_update( void );
	
	int BPY_Err_getLinenumber( void );
	const char *BPY_Err_getFilename( void );

	int BPY_txt_do_python_Text( struct Text *text );
	int BPY_menu_do_python( short menutype, int event );
	int BPY_menu_do_shortcut( short menutype, unsigned short key, unsigned short modifiers );
	int BPY_menu_invoke( struct BPyMenu *pym, short menutype );
	
	/* 2.5 UI Scripts */
	int BPY_run_python_script( struct bContext *C, const char *filename, struct Text *text, struct ReportList *reports ); // 2.5 working
	int BPY_run_script_space_draw(const struct bContext *C, struct SpaceScript * sc); // 2.5 working
	void BPY_run_ui_scripts(struct bContext *C, int reload);
//	int BPY_run_script_space_listener(struct bContext *C, struct SpaceScript * sc, struct ARegion *ar, struct wmNotifier *wmn); // 2.5 working
	void BPY_update_modules( void ); // XXX - annoying, need this for pointers that get out of date
	
	
	
	int BPY_run_script(struct Script *script);
	void BPY_free_compiled_text( struct Text *text );

	int BPY_has_onload_script( void );

	int BPY_is_spacehandler(struct Text *text, char spacetype);
	int BPY_del_spacehandler(struct Text *text, struct ScrArea *sa);
	int BPY_add_spacehandler(struct Text *txt, struct ScrArea *sa,char spacetype);
	int BPY_has_spacehandler(struct Text *text, struct ScrArea *sa);
	void BPY_screen_free_spacehandlers(struct bScreen *sc);
	int BPY_do_spacehandlers(struct ScrArea *sa, unsigned short event,
		short eventValue, unsigned short space_event);

	void BPY_pydriver_update(void);
	float BPY_pydriver_eval(struct ChannelDriver *driver);

	int BPY_button_eval(struct bContext *C, char *expr, double *value);

/* format importer hook */
	int BPY_call_importloader( char *name );

	void BPY_spacescript_do_pywin_draw( struct SpaceScript *sc );
	void BPY_spacescript_do_pywin_event( struct SpaceScript *sc,
					     unsigned short event, short val, char ascii );
	void BPY_clear_script( struct Script *script );
	void BPY_free_finished_script( struct Script *script );
	void BPY_scripts_clear_pyobjects( void );
	
	void error_pyscript( void );
	void BPY_DECREF(void *pyob_ptr);	/* Py_DECREF() */

/* void BPY_Err_Handle(struct Text *text); */
/* int BPY_spacetext_is_pywin(struct SpaceText *st); */

#ifdef __cplusplus
}				/* extern "C" */
#endif

#endif  /* BPY_EXTERN_H */
