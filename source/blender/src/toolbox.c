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

#define PY_TOOLBOX 1

#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif   

#include <fcntl.h>
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"
#include "BIF_language.h"
#include "BIF_resources.h"

#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_lamp_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_plugin_types.h"
#include "BKE_utildefines.h"
#include "BKE_mesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_mainqueue.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_editarmature.h"
#include "BIF_editfont.h"
#include "BIF_editmesh.h"
#include "BIF_editseq.h"
#include "BIF_editlattice.h"
#include "BIF_editsima.h"
#include "BIF_editoops.h"
#include "BIF_imasel.h"
#include "BIF_screen.h"
#include "BIF_tbcallback.h"
#include "BIF_editnla.h"

#include "BDR_editobject.h"
#include "BDR_editcurve.h"
#include "BDR_editmball.h"

#include "BSE_editipo.h"
#include "BSE_buttons.h"
#include "BSE_filesel.h"
#include "BSE_edit.h"
#include "BSE_headerbuttons.h"

#include "IMB_imbuf.h"

#include "mydevice.h"
#include "blendef.h"

static int tbx1, tbx2, tby1, tby2, tbfontyofs, tbmain=0;
static int tbmemx=TBOXX/2, tbmemy=(TBOXEL-0.5)*TBOXH, tboldwin, addmode= 0;
static int oldcursor;

	/* variables per item */
static char *tbstr, *tbstr1, *keystr; 	
static void (*tbfunc)(int);
static int tbval;

/* *********** PC PATCH ************* */

void ColorFunc(int i)
{
	if(i==TBOXBLACK) glColor3ub(0, 0, 0);
	else if(i==TBOXWHITE) glColor3ub(240, 240, 240);
	else if(i==TBOXGREY) glColor3ub(160, 160, 160);
	else glColor3ub(0, 0, 0);
}

/* ********************* PYTHON TOOLBOX CALLBACK ************************* */

#ifdef PY_TOOLBOX 
/* see bpython/intern/py_toolbox.c */

/* moved to BIF_toolbox.h */
/* typedef char** (*tbox_callback)(int, int); */

TBcallback *callback_dummy(int level, int entry)
{
	return NULL;
}	

/* callback func ptr for py_toolbox */
Tbox_callbackfunc g_toolbox_menucallback = &callback_dummy;

void tboxSetCallback(Tbox_callbackfunc f)
{
	g_toolbox_menucallback = f;
}

#endif

/* ********************* TOOLBOX ITEMS ************************* */

void tbox_setinfo(int x, int y)
{
	/* dependant of tbmain vars are set */
	tbstr= 0;
	tbstr1= 0;
	tbfunc= 0;
	tbval= 0;
	keystr = NULL;

/* main menu entries: defined in BIF_toolbox.h */

	if(x==0) {
		switch(y) {
			case TBOX_MAIN_FILE:		tbstr= "FILE";		break;
			case TBOX_MAIN_EDIT:		tbstr= "EDIT";		break;
			case TBOX_MAIN_ADD:		
				if (addmode==OB_MESH) tbstr= "  MESH";
				else if(addmode==OB_CURVE) tbstr= "  CURVE";
				else if(addmode==OB_SURF) tbstr= "  SURF";
				else if(addmode==OB_MBALL) tbstr= "  META";
				else tbstr= "ADD";
				break;
			case TBOX_MAIN_OBJECT1:		tbstr= "OBJECT";	break;
			case TBOX_MAIN_OBJECT2:		tbstr= "OBJECT";	break;
			case TBOX_MAIN_MESH:		tbstr= "MESH";		break;
			case TBOX_MAIN_CURVE:		tbstr= "CURVE";		break;
			case TBOX_MAIN_KEY:			tbstr= "KEY";		break;
			case TBOX_MAIN_RENDER:		tbstr= "RENDER";	break;
			case TBOX_MAIN_VIEW:		tbstr= "VIEW";		break;
			case TBOX_MAIN_SEQ:		tbstr= "SEQUENCE";	break;
#ifdef PY_TOOLBOX
			case TBOX_MAIN_PYTOOL:		
			{
				if (g_toolbox_menucallback(0, 0)) // valid callback?
					tbstr= "PYTOOL";	
				break;
			}
#endif
		}
	}
	
/* TOPICS */
	else {
		
		
/* FILE TOPICS */
		if(tbmain==TBOX_MAIN_FILE) {
			switch(y) {
				case 0: tbstr= "New";				tbstr1= "c|x";		keystr= "Ctrl X";	break;
				case 1: tbstr= "Open";				tbstr1= "F1";		keystr= "F1";		break;
				case 2: tbstr= "Reopen Last";		tbstr1= "c|o";		keystr= "Ctrl O";	break;
				case 3: tbstr= "Append";			tbstr1= "shift+F1";	keystr= "Shift F1";	break;
				case 4: tbstr= "";					tbstr1= "";			keystr= "";			break;
				case 5: tbstr= "Save As";			tbstr1= "F2";		keystr= "F2";		break;
				case 6: tbstr= "Save";				tbstr1= "c|w";		keystr= "Ctrl W";	break;
				case 7: tbstr= "";					tbstr1= "";			keystr= "";			break;
				case 8: tbstr= "Save Image";		tbstr1= "F3";		keystr= "F3";		break;
				case 9: tbstr= "Save VRML";			tbstr1= "c|F2";		keystr= "Ctrl F2";	break;
				case 10: tbstr= "Save DXF";			tbstr1= "shift+F2";	keystr= "Shift F2";	break;
				case 11: tbstr= "Save VideoScape";	tbstr1= "a|w";		keystr= "Alt W";	break;
				case 12: tbstr= "Save UserPrefs";					tbstr1= "c|u";			keystr= "Ctrl U";			break;
				case 13: tbstr= "Quit";				tbstr1= "q";		keystr= "Q";		break;
			}
		}

/* EDIT TOPICS */
		if(tbmain==TBOX_MAIN_EDIT) {
			switch(y) {
				case 0: tbstr= "(De)Select All";	tbstr1= "a";	keystr= "A";		break;
				case 1: tbstr= "Border Select";		tbstr1= "b";	keystr= "B";		break;
				case 2: tbstr= "Select Linked";					tbstr1= "l";		keystr= "L";			break;
				case 3: tbstr= "Hide Selected";					tbstr1= "h";		keystr= "H";			break;
				case 4: tbstr= "Duplicate";			tbstr1= "D";	keystr= "Shift D";	break;
				case 5: tbstr= "Delete";			tbstr1= "x";	keystr= "X";		break;
				case 6: tbstr= "Edit Mode";			tbstr1= "Tab";	keystr= "Tab";		break;
				case 7: tbstr= "Grabber";			tbstr1= "g";	keystr= "G";		break;
				case 8: tbstr= "Rotate";			tbstr1= "r";	keystr= "R";		break;
				case 9: tbstr= "Scale";				tbstr1= "s";	keystr= "S";		break;
				case 10: tbstr= "Shrink/Fatten";					tbstr1= "a|s";		keystr= "Alt S";			break;
				case 11: tbstr= "Shear";			tbstr1= "c|s";	keystr= "Ctrl S";	break;
				case 12: tbstr= "Warp/Bend";		tbstr1= "W";	keystr= "Shift W";	break;
				case 13: tbstr= "Snap Menu";		tbstr1= "S";	keystr= "Shift S";	break;
			}
		}

/* ADD TOPICS */
		if(tbmain==TBOX_MAIN_ADD) {

			if(addmode==0) {
				switch(y) {
					case 0: tbstr= "Mesh";		tbstr1= ">>";	keystr= ">>";	tbval=OB_MESH;										break;
					case 1: tbstr= "Curve";		tbstr1= ">>";	keystr= ">>";	tbval=OB_CURVE;	;									break;
					case 2: tbstr= "Surface";	tbstr1= ">>";	keystr= ">>";	tbval=OB_SURF;										break;
					case 3: tbstr= "Meta";		tbstr1= ">>";	keystr= ">>";	tbval=OB_MBALL;
						break;
					case 4: tbstr= "Text";		tbstr1= "";		keystr= "";		tbval=OB_FONT;		tbfunc= add_primitiveFont;		break;
					case 5: tbstr= "Empty";		tbstr1= "A";	keystr= "";		tbval=OB_EMPTY;										break;
					case 6: tbstr= "";			tbstr1= "";		keystr= "";		tbval=0;											break;
					case 7: tbstr= "Camera";	tbstr1= "A";	keystr= "";		tbval=OB_CAMERA;									break;
					case 8: tbstr= "Lamp";		tbstr1= "A";	keystr= "";		tbval=OB_LAMP;										break;
					case 9: tbstr= "Armature";	tbstr1= "";		keystr= "";		tbval=OB_ARMATURE;	tbfunc=add_primitiveArmature;	break;
					case 10: tbstr= "";			tbstr1= "";		keystr= "";		tbval=0;											break;
					case 11: tbstr= "Lattice";	tbstr1= "A";	keystr= "";		tbval=OB_LATTICE;									break;
					case 12: tbstr= "";			tbstr1= "";		keystr= "";		tbval=0;											break;
					case 13: tbstr= "";			tbstr1= "";		keystr= "";		tbval=0;											break;
				}
				if(tbstr1 && tbstr1[0]=='A') tbfunc= (void (*)(int) )add_object_draw;
			}
			else if(addmode==OB_MESH) {		
				switch(y) {
					case 0: tbstr= ">Plane";	tbstr1= "A";	keystr= "";		tbval=0;	break;
					case 1: tbstr= ">Cube";		tbstr1= "A";	keystr= "";		tbval=1;	break;
					case 2: tbstr= ">Circle";	tbstr1= "A";	keystr= "";		tbval=4;	break;
					case 3: tbstr= ">UVsphere";	tbstr1= "A";	keystr= "";		tbval=11;	break;
					case 4: tbstr= ">Icosphere";tbstr1= "A";	keystr= "";		tbval=12;	break;
					case 5: tbstr= ">Cylinder";	tbstr1= "A";	keystr= "";		tbval=5;	break;
					case 6: tbstr= ">Tube";		tbstr1= "A";	keystr= "";		tbval=6;	break;
					case 7: tbstr= ">Cone";		tbstr1= "A";	keystr= "";		tbval=7;	break;
					case 8: tbstr= ">";			tbstr1= "";		keystr= "";					break;
					case 9: tbstr= ">Grid";		tbstr1= "A";	keystr= "";		tbval=10;	break;
					case 13: tbstr= ">Monkey";	tbstr1= "A";	keystr= "";		tbval=13;	break;
				}
				if(tbstr1 && tbstr1[0]=='A') tbfunc= add_primitiveMesh;
			}
			else if(addmode==OB_SURF) {
				switch(y) {
					case 0: tbstr= ">Curve";	tbstr1= "A";	keystr= "";		tbval=0; break;
					case 1: tbstr= ">Circle";	tbstr1= "A";	keystr= "";		tbval=1; break;
					case 2: tbstr= ">Surface";	tbstr1= "A";	keystr= "";		tbval=2; break;
					case 3: tbstr= ">Tube";		tbstr1= "A";	keystr= "";		tbval=3; break;
					case 4: tbstr= ">Sphere";	tbstr1= "A";	keystr= "";		tbval=4; break;
					case 5: tbstr= ">Donut";	tbstr1= "A";	keystr= "";		tbval=5; break;
				}
				if(tbstr1 && tbstr1[0]=='A') tbfunc= add_primitiveNurb;
			}
/*			else if (addmode==OB_ARMATURE){
				switch(y) {
					case 0: tbstr= ">Bone";		tbstr1= "A";	keystr= "";		tbval=0; break;
					case 1: tbstr= ">Hand";		tbstr1= "A";	keystr= "";		tbval=1; break;
					case 2: tbstr= ">Biped";	tbstr1= "A";	keystr= "";		tbval=2; break;
				}
				if(tbstr1 && tbstr1[0]=='A') tbfunc= add_primitiveArmature;	
			}
*/
			else if(addmode==OB_CURVE) {
				switch(y) {
					case 0: tbstr= ">Bezier Curve";		tbstr1= "A";	keystr= "";	tbval=10;	break;
					case 1: tbstr= ">Bezier Circle";	tbstr1= "A";	keystr= "";	tbval=11;	break;
					case 2: tbstr= ">";					tbstr1= "";		keystr= "";				break;
					case 3: tbstr= ">Nurbs Curve";		tbstr1= "A";	keystr= "";	tbval=40;	break;
					case 4: tbstr= ">Nurbs Circle";		tbstr1= "A";	keystr= "";	tbval=41;	break;
					case 5: tbstr= ">";					tbstr1= "";		keystr= "";				break;
					case 6: tbstr= ">Path";				tbstr1= "A";	keystr= "";	tbval=46;	break;
				}
				if(tbstr1 && tbstr1[0]=='A') tbfunc= add_primitiveCurve;
			}
			else if(addmode==OB_MBALL) {
				switch(y) {
					case 0: tbstr= "Ball";			tbstr1= "A";	tbval=1; break;
					case 1: tbstr= "Tube";			tbstr1= "A";	tbval=2; break;
					case 2: tbstr= "Plane";			tbstr1= "A";	tbval=3; break;
					case 3: tbstr= "Elipsoid";		tbstr1= "A";	tbval=4; break;
					case 4: tbstr= "Cube";			tbstr1= "A";	tbval=5; break;
					case 5: tbstr= "";			tbstr1= "";		break;
					case 6: tbstr= "";			tbstr1= "";		break;
					case 7: tbstr= "";			tbstr1= "";		break;
					case 8: tbstr= "";			tbstr1= "";		break;
					case 9: tbstr= "";			tbstr1= "";		break;
					case 10: tbstr= "";			tbstr1= "";		break;
					case 11: tbstr= "Duplicate";tbstr1= "D";	break;
				}
				if(tbstr1 && tbstr1[0]=='A') tbfunc= add_primitiveMball;
			}
		}
		
/* OB TOPICS 1 */
		else if(tbmain==TBOX_MAIN_OBJECT1) {
			switch(y) {
				case 0: tbstr= "Clear Size";		tbstr1= "a|s";	keystr= "Alt S";	break;
				case 1: tbstr= "Clear Rotation";	tbstr1= "a|r";	keystr= "Alt R";	break;
				case 2: tbstr= "Clear Location";	tbstr1= "a|g";	keystr= "Alt G";	break;
				case 3: tbstr= "Clear Origin";		tbstr1= "a|o";	keystr= "Alt O";	break;
				case 4: tbstr= "Make Parent";		tbstr1= "c|p";	keystr= "Ctrl P";	break;
				case 5: tbstr= "Clear Parent";		tbstr1= "a|p";	keystr= "Alt P";	break;
/* 	Unkown what tbstr1 should be...
				case 6: tbstr= "MkVert Parent";		tbstr1= "c|a|p";	keystr= "Ctrl Alt P";	break;
*/
				case 7: tbstr= "Make Track";		tbstr1= "c|t";	keystr= "Ctrl T";	break;
				case 8: tbstr= "Clear Track";		tbstr1= "a|t";	keystr= "Alt T";	break;
/*				case 9: tbstr= "";					tbstr1= "";		keystr= "";			break; */
				case 10: tbstr= "Image Displist";	tbstr1= "c|d";	keystr= "Ctrl D";	break;
				case 11: tbstr= "Image Aspect";		tbstr1= "a|v";	keystr= "Alt V";	break;
				case 12: tbstr= "Vect Paint";		tbstr1= "v";	keystr= "V";	break;
			}
		}
		
/* OB TOPICS 2 */
		else if(tbmain==TBOX_MAIN_OBJECT2) {
			switch(y) {
				case 0: tbstr= "Edit Mode";			tbstr1= "Tab";	keystr= "Tab";			break;
				case 1: tbstr= "Move To Layer";		tbstr1= "m";	keystr= "M";			break;
				case 2: tbstr= "Delete";			tbstr1= "x";	keystr= "X";			break;
				case 3: tbstr= "Delete All";		tbstr1= "c|x";	keystr= "Ctrl X";		break;
				case 4: tbstr= "Apply Size/Rot";	tbstr1= "c|a";	keystr= "Ctrl A";		break;
				case 5: tbstr= "Apply Deform";		tbstr1= "c|A";	keystr= "Ctrl Shift A";	break;
				case 6: tbstr= "Join";				tbstr1= "c|j";	keystr= "Ctrl J";		break;
				case 7: tbstr= "Make Local";		tbstr1= "l";	keystr= "L";			break;
				case 8: tbstr= "Select Linked";		tbstr1= "L";	keystr= "Shift L";		break;
				case 9: tbstr= "Make Links";		tbstr1= "c|l";	keystr= "Ctrl L";		break;
				case 10: tbstr= "Copy Menu";		tbstr1= "c|c";	keystr= "Ctrl C";		break;
				case 11: tbstr= "Convert Menu";		tbstr1= "a|c";	keystr= "Alt C";		break;
				case 12: tbstr= "Boolean Op";		tbstr1= "w";	keystr= "W";		break;
			}
		}

/* mesh TOPICS */
		else if(tbmain==TBOX_MAIN_MESH) {
			switch(y) {
				case 0: tbstr= "Select Linked";		tbstr1= "l";	keystr= "L";		break;
				case 1: tbstr= "Deselect Linked";	tbstr1= "L";	keystr= "Shift L";	break;
				case 2: tbstr= "Extrude";			tbstr1= "e";	keystr= "E";		break;
				case 3: tbstr= "Delete Menu";		tbstr1= "x";	keystr= "X";		break;
				case 4: tbstr= "Make edge/face";	tbstr1= "f";	keystr= "F";		break;
				case 5: tbstr= "Fill";				tbstr1= "F";	keystr= "Shift F";	break;
				case 6: tbstr= "Split";				tbstr1= "y";	keystr= "Y";		break;
				case 7: tbstr= "Undo/reload";		tbstr1= "u";	keystr= "U";		break;
				case 8: tbstr= "Calc Normals";		tbstr1= "c|n";	keystr= "Ctrl N";	break;
				case 9: tbstr= "Separate";			tbstr1= "p";	keystr= "P";		break;
				case 10: tbstr= "Write Videosc";	tbstr1= "a|w";	keystr= "Alt W";	break;
/*				case 11: tbstr= "";					tbstr1= "";		keystr= "";			break; */
			}
		}
	
/* CURVE TOPICS */
		else if(tbmain==TBOX_MAIN_CURVE) {
			switch(y) {
				case 0: tbstr= "Select Linked";		tbstr1= "l";	keystr= "L";		break;
				case 1: tbstr= "Deselect Linked";	tbstr1= "L";	keystr= "Shift L";	break;
				case 2: tbstr= "Extrude";			tbstr1= "e";	keystr= "E";		break;
				case 3: tbstr= "Delete Menu";		tbstr1= "x";	keystr= "X";		break;
				case 4: tbstr= "Make Segment";		tbstr1= "f";	keystr= "F";		break;
				case 5: tbstr= "Cyclic";			tbstr1= "c";	keystr= "C";		break;
/*				case 6: tbstr= "";					tbstr1= "";		keystr= "";			break; */
				case 7: tbstr= "Select Row";		tbstr1= "R";	keystr= "Shift R";	break;
				case 8: tbstr= "Calc Handle";		tbstr1= "h";	keystr= "H";		break;
				case 9: tbstr= "Auto Handle";		tbstr1= "H";	keystr= "Shift H";	break;
				case 10: tbstr= "Vect Handle";		tbstr1= "v";	keystr= "V";		break;
				case 11: tbstr= "Specials";			tbstr1= "w";	keystr= "W";		break;
			}
		}
	
/* KEY TOPICS */
		else if(tbmain==TBOX_MAIN_KEY) {
			switch(y) {
				case 0: tbstr= "Insert";	tbstr1= "i";		keystr= "I";		break;
				case 1: tbstr= "Show";		tbstr1= "k";		keystr= "K";		break;
				case 2: tbstr= "Next";		tbstr1= "PageUp";	keystr= "PgUp";		break;
				case 3: tbstr= "Prev";		tbstr1= "PageDn";	keystr= "PgDn";		break;
				case 4: tbstr= "Show+Sel";	tbstr1= "K";		keystr= "Shift K";	break;
/*				case 5: tbstr= "";			tbstr1= "";			keystr= "";			break;
				case 6: tbstr= "";			tbstr1= "";			keystr= "";			break;
				case 7: tbstr= "";			tbstr1= "";			keystr= "";			break;
				case 8: tbstr= "";			tbstr1= "";			keystr= "";			break;
				case 9: tbstr= "";			tbstr1= "";			keystr= "";			break;
				case 10: tbstr= "";			tbstr1= "";			keystr= "";			break;
				case 11: tbstr= "";			tbstr1= "";			keystr= "";			break; */
			}
		}
/* SEQUENCER TOPICS */
                else if(tbmain==TBOX_MAIN_SEQ) {
                        switch(y) {
                                case 0: tbstr= "Add Strip"; tbstr1= "A";  keystr= "Shift A";          break;
                                case 1: tbstr= "Change Str"; tbstr1= "c";  keystr= "C";          break;
                                case 2: tbstr= "Delete Str";                              tbstr1= "x";             keystr= "X";                     break;
                                case 3: tbstr= "Make Meta";    tbstr1= "m";    keystr= "M";      break;
                                case 4: tbstr= "Str Params";    tbstr1= "n";    keystr= "N";            break;
                        }
                }

/* RENDER TOPICS */
		else if(tbmain==TBOX_MAIN_RENDER) {
			switch(y) {
				case 0: tbstr= "Render Window";	tbstr1= "F11";	keystr= "F11";		break;
				case 1: tbstr= "Render";		tbstr1= "F12";	keystr= "F12";		break;
				case 2: tbstr= "Set Border";	tbstr1= "B";	keystr= "Shift B";	break;
				case 3: tbstr= "Image Zoom";	tbstr1= "z";	keystr= "Z";		break;
/*				case 4: tbstr= "";				tbstr1= "";		keystr= "";			break;
				case 5: tbstr= "";				tbstr1= "";		keystr= "";			break;
				case 6: tbstr= "";				tbstr1= "";		keystr= "";			break;
				case 7: tbstr= "";				tbstr1= "";		keystr= "";			break;
				case 8: tbstr= "";				tbstr1= "";		keystr= "";			break;
				case 9: tbstr= "";				tbstr1= "";		keystr= "";			break;
				case 10: tbstr= "";				tbstr1= "";		keystr= "";			break;
				case 11: tbstr= "";				tbstr1= "";		keystr= "";			break; */
			}
		}
	
/* VIEW TOPICS */
		else if(tbmain==TBOX_MAIN_VIEW) {
			switch(y) {
/*				case 0: tbstr= "";		tbstr1= "";	break;
				case 1: tbstr= "";		tbstr1= "";	break;
				case 2: tbstr= "";		tbstr1= "";	break;
				case 3: tbstr= "";		tbstr1= "";	break; */
				case 4: tbstr= "Centre";		tbstr1= "c";	keystr= "C";		break;
				case 5: tbstr= "Home";			tbstr1= "C";	keystr= "Shift C";	break;
/*				case 6: tbstr= "";		tbstr1= "";	break;
				case 7: tbstr= "";		tbstr1= "";	break;
				case 8: tbstr= "";		tbstr1= "";	break;*/
				case 9: tbstr= "Z-Buffer";		tbstr1= "z";	keystr= "Z";		break;
/*				case 10: tbstr= "";		tbstr1= "";	break;
				case 11: tbstr= "";		tbstr1= "";	break;*/
			}
		}
#ifdef PY_TOOLBOX
		else if(tbmain==TBOX_MAIN_PYTOOL) {
			TBcallback *t= g_toolbox_menucallback(0, y); // call python menu constructor
			if (t) { 
				tbstr = t->desc; 
				keystr = t->key;
				tbfunc = t->cb;
				tbval = t->val;
			}
		}
#endif
	}
}

/* ******************** INIT ************************** */

void bgnpupdraw(int startx, int starty, int endx, int endy)
{
	#if defined(__sgi) || defined(__sun__) || defined( __sun ) || defined (__sparc) || defined (__sparc__)

	/* this is a dirty patch: XgetImage gets sometimes the backbuffer */
	my_get_frontbuffer_image(0, 0, 1, 1);
	my_put_frontbuffer_image();
	#endif

	tboldwin= mywinget();

	mywinset(G.curscreen->mainwin);
	
	/* tinsy bit larger, 1 pixel on the rand */
	
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);

	glFinish();

	my_get_frontbuffer_image(startx-1, starty-4, endx-startx+5, endy-starty+6);

	oldcursor= get_cursor();
	set_cursor(CURSOR_STD);
	
	tbfontyofs= (TBOXH-11)/2 +1;	/* ypos text in toolbox */
}

void endpupdraw(void)
{
	glFinish();
	my_put_frontbuffer_image();
	
	if(tboldwin) {
		mywinset(tboldwin);
		set_cursor(oldcursor);
	}

	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);
}

/* ********************************************** */

void asciitoraw(int ch, unsigned short *event, unsigned short *qual)
{
	if( isalpha(ch)==0 ) return;
	
	if( isupper(ch) ) {
		*qual= LEFTSHIFTKEY;
		ch= tolower(ch);
	}
	
	switch(ch) {
	case 'a': *event= AKEY; break;
	case 'b': *event= BKEY; break;
	case 'c': *event= CKEY; break;
	case 'd': *event= DKEY; break;
	case 'e': *event= EKEY; break;
	case 'f': *event= FKEY; break;
	case 'g': *event= GKEY; break;
	case 'h': *event= HKEY; break;
	case 'i': *event= IKEY; break;
	case 'j': *event= JKEY; break;
	case 'k': *event= KKEY; break;
	case 'l': *event= LKEY; break;
	case 'm': *event= MKEY; break;
	case 'n': *event= NKEY; break;
	case 'o': *event= OKEY; break;
	case 'p': *event= PKEY; break;
	case 'q': *event= QKEY; break;
	case 'r': *event= RKEY; break;
	case 's': *event= SKEY; break;
	case 't': *event= TKEY; break;
	case 'u': *event= UKEY; break;
	case 'v': *event= VKEY; break;
	case 'w': *event= WKEY; break;
	case 'x': *event= XKEY; break;
	case 'y': *event= YKEY; break;
	case 'z': *event= ZKEY; break;
	}
}

void tbox_execute(void)
{
	/* if tbfunc: call function */
	/* if tbstr1 is a string: put value tbval in queue */
	unsigned short event=0;
	unsigned short qual1=0, qual2=0;

	/* needed to check for valid selected objects */
	Base *base=NULL;
	Object *ob=NULL;

	base= BASACT;
	if (base) ob= base->object;

	if(tbfunc) {
		tbfunc(tbval);
	}
	else if(tbstr1) {
		if(strcmp(tbstr1, "Tab")==0) {
			event= TABKEY;
		}
		else if(strcmp(tbstr1, "PageUp")==0) {
			event= PAGEUPKEY;
		}
		else if(strcmp(tbstr1, "PageDn")==0) {
			event= PAGEDOWNKEY;
		}
		else if(strcmp(tbstr1, "shift+F1")==0) {
			qual1= LEFTSHIFTKEY;
			event= F1KEY;
		}
		else if(strcmp(tbstr1, "shift+F2")==0) {
			qual1= LEFTSHIFTKEY;
			event= F2KEY;
		}
		/* ctrl-s (Shear): switch into editmode ### */
		else if(strcmp(tbstr1, "c|s")==0) {
			/* check that a valid object is selected to prevent crash */
			if(!ob) error("Only selected objects can be sheared");
			else if ((ob->type==OB_LAMP) || (ob->type==OB_EMPTY) || (ob->type==OB_FONT) || (ob->type==OB_CAMERA)) {
				error("Only editable 3D objects can be sheared");
			}
			else if ((base->lay & G.vd->lay)==0) {
				error("Only objects on visible layers can be sheared");
			}
			else {
				if (!G.obedit) {
					enter_editmode();
					/* ### put these into a deselectall_gen() */
					if(G.obedit->type==OB_MESH) deselectall_mesh();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) deselectall_nurb();
					else if(G.obedit->type==OB_MBALL) deselectall_mball();
					else if(G.obedit->type==OB_LATTICE) deselectall_Latt();
					/* ### */
				}
				qual1 = LEFTCTRLKEY;
				event = SKEY;
			}
		}
		else if(strcmp(tbstr1, "W")==0) {
			if (!ob) error ("Only selected objects can be warped");
		        /* check that a valid object is selected to prevent crash */
			else if ((ob->type==OB_LAMP) || (ob->type==OB_EMPTY) || (ob->type==OB_FONT) || (ob->type==OB_CAMERA)) {
				error("Only editable 3D objects can be warped");
			}
			else if ((base->lay & G.vd->lay)==0) {
				error("Only objects on visible layers can be warped");
			}
			else {
				if (!G.obedit) {
					enter_editmode();
					/* ### put these into a deselectall_gen() */
					if(G.obedit->type==OB_MESH) deselectall_mesh();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) deselectall_nurb();
					else if(G.obedit->type==OB_MBALL) deselectall_mball();
					else if(G.obedit->type==OB_LATTICE) deselectall_Latt();
					/* ### */
				}
				qual1 = LEFTSHIFTKEY;
				event = WKEY;
			}
		}

		else if(strlen(tbstr1)<4 || (strlen(tbstr1)==4 && tbstr1[2]=='F')) {
				
			if(tbstr1[1]=='|') {
				if(tbstr1[0]=='c') qual1= LEFTCTRLKEY;
				else if(tbstr1[0]=='a') qual1= LEFTALTKEY;
				
				if (tbstr1[2]=='F') {
					switch(tbstr1[3]) {
					case '1': event= F1KEY; break;
					case '2': event= F2KEY; break;
					case '3': event= F3KEY; break;
					case '4': event= F4KEY; break;
					case '5': event= F5KEY; break;
					case '6': event= F6KEY; break;
					case '7': event= F7KEY; break;
					case '8': event= F8KEY; break;
					case '9': event= F9KEY; break;
					}
				}
				else asciitoraw(tbstr1[2], &event, &qual2);
			}
			else if(tbstr1[1]==0) {
				asciitoraw(tbstr1[0], &event, &qual2);
			}
			else if(tbstr1[0]=='F') {
				event= atoi(tbstr1+1);
				switch(event) {
					case 1: event= F1KEY; break;
					case 2: event= F2KEY; break;
					case 3: event= F3KEY; break;
					case 4: event= F4KEY; break;
					case 5: event= F5KEY; break;
					case 6: event= F6KEY; break;
					case 7: event= F7KEY; break;
					case 8: event= F8KEY; break;
					case 9: event= F9KEY; break;
					case 10: event= F10KEY; break;
					case 11: event= F11KEY; break;
					case 12: event= F12KEY; break;
				}
			}
		}
		
		if(event) {
			if(qual1) mainqenter(qual1, 1);
			if(qual2) mainqenter(qual2, 1);
			mainqenter(event, 1);
			mainqenter(event, 0);
			mainqenter(EXECUTE, 1);
			if(qual1) mainqenter(qual1, 0);
			if(qual2) mainqenter(qual2, 0);
		}
	}
	
}

void tbox_getmouse(mval)
short *mval;
{

	getmouseco_sc(mval);

}

void tbox_setmain(int val)
{
	tbmain= val;

	if(tbmain==0 && G.obedit) {
		addmode= G.obedit->type;
	}
}

void bgntoolbox(void)
{
	short xmax, ymax, mval[2];
	
	xmax = G.curscreen->sizex;
	ymax = G.curscreen->sizey;

   	tbox_getmouse(mval);
	
	if(mval[0]<95) mval[0]= 95;
	if(mval[0]>xmax-95) mval[0]= xmax-95;

	warp_pointer(mval[0], mval[1]);

	tbx1= mval[0]-tbmemx;
	tby1= mval[1]-tbmemy;
	if(tbx1<10) tbx1= 10;
	if(tby1<10) tby1= 10;
	
	tbx2= tbx1+TBOXX;
	tby2= tby1+TBOXY;
	if(tbx2>xmax) {
		tbx2= xmax-10;
		tbx1= tbx2-TBOXX;
	}
	if(tby2>ymax) {
		tby2= ymax-10;
		tby1= tby2-TBOXY;
	}

	bgnpupdraw(tbx1, tby1, tbx2, tby2);
}

void endtoolbox(void)
{
	short mval[2];
	
	tbox_getmouse(mval);
	if(mval[0]>tbx1 && mval[0]<tbx2)
		if(mval[1]>tby1 && mval[1]<tby2) {
			tbmemx= mval[0]-(tbx1);
			tbmemy= mval[1]-(tby1);
	}
	
	endpupdraw();
}


void tbox_embossbox(short x1, short y1, short x2, short y2, short type)	
/* type: 0=menu, 1=menusel, 2=topic, 3=topicsel */
{
	
	if(type==0) {
		glColor3ub(160, 160, 160);
		glRects(x1+1, y1+1, x2-1, y2-1);
	}
	if(type==1) {
		glColor3ub(50, 50, 100);
		glRects(x1+1, y1+1, x2-1, y2-1);
	}
	if(type==2) {
		glColor3ub(190, 190, 190);
		glRects(x1+1, y1+1, x2-1, y2-1);
	}
	if(type==3) {
		cpack(0xc07070);
		glRects(x1+1, y1+1, x2-1, y2-1);
	}
	
	if(type & 1) cpack(0xFFFFFF);
	else cpack(0x0);
}


void tbox_drawelem_body(x, y, type)
{
	int x1 = 0, y1, x2 = 0, y2;
	
	if(x==0) {
		x1= tbx1; x2= tbx1+TBOXXL;
	}
	else if(x==1) {
		x1= tbx1+TBOXXL;
		x2= x1+ TBOXXR-1;
	}
	
	y1= tby1+ (TBOXEL-y-1)*TBOXH;
	y2= y1+TBOXH-1;
	
	tbox_embossbox(x1, y1, x2, y2, type);
	
}

void tbox_drawelem_text(x, y, type)
{
	int x1 = 0, y1, x2 = 0, y2, len1, len2;
	
	if(x==0) {
		x1= tbx1; x2= tbx1+TBOXXL;
	}
	else if(x==1) {
		x1= tbx1+TBOXXL;
		x2= x1+ TBOXXR-1;
	}
	
	y1= tby1+ (TBOXEL-y-1)*TBOXH;
	y2= y1+TBOXH-1;
	
	if(type==0 || type==2) {
		ColorFunc(TBOXBLACK);
	}
	else {
		glColor3ub(240, 240, 240);
	}
	
	/* text */
	tbox_setinfo(x, y);
	if(tbstr && tbstr[0]) {
		len1= 5+BMF_GetStringWidth(G.font, tbstr);
		if(keystr) len2= 5+BMF_GetStringWidth(G.font, keystr); else len2= 0;
		
		while(len1>0 && (len1+len2+5>x2-x1) ) {
			tbstr[strlen(tbstr)-1]= 0;
			len1= BMF_GetStringWidth(G.font, tbstr);
		}
		
		glRasterPos2i(x1+5, y1+tbfontyofs);
		BIF_DrawString(G.font, tbstr, (U.transopts & USER_TR_MENUS));
		
		if(keystr && keystr[0]) {
			if(type & 1) {
				ColorFunc(TBOXBLACK);
	
				glRecti(x2-len2-2,  y1+2,  x2-3,  y2-2);
				ColorFunc(TBOXWHITE);
				glRasterPos2i(x2-len2,  y1+tbfontyofs);
				BIF_DrawString(G.font, keystr, (U.transopts & USER_TR_MENUS));
			}
			else {
				ColorFunc(TBOXBLACK);
				glRasterPos2i(x2-len2,  y1+tbfontyofs);
				BIF_DrawString(G.font, keystr, (U.transopts & USER_TR_MENUS));
			}
		}
	}
}


void tbox_drawelem(x, y, type)
int x, y, type;	
{
	/* type: 0=menu, 1=menusel, 2=topic, 3=topicsel */

	tbox_drawelem_body(x, y, type);
	tbox_drawelem_text(x, y, type);
	
}

void tbox_getactive(x, y)
int *x, *y;
{
	short mval[2];
	
	tbox_getmouse(mval);
	
	mval[0]-=tbx1;
	if(mval[0]<TBOXXL) *x= 0;
	else *x= 1;
	
	*y= mval[1]-tby1;
	*y/= TBOXH;
	*y= TBOXEL- *y-1;
	if(*y<0) *y= 0;
	if(*y>TBOXEL-1) *y= TBOXEL-1;
	
}

void drawtoolbox(void)
{
	int x, y, actx, acty, type;

	tbox_getactive(&actx, &acty);

	/* background */
	for(x=0; x<2; x++) {
		
		for(y=0; y<TBOXEL; y++) {
			
			if(x==0) type= 0; 
			else type= 2;
			
			if(actx==x && acty==y) type++;
			if(type==0) {
				if(tbmain==y) type= 1;
			}
			
			tbox_drawelem_body(x, y, type);
			
		}
	}

	/* text */
	for(x=0; x<2; x++) {
		
		for(y=0; y<TBOXEL; y++) {
			
			if(x==0) type= 0; 
			else type= 2;
			
			if(actx==x && acty==y) type++;
			if(type==0) {
				if(tbmain==y) type= 1;
			}
			
			tbox_drawelem_text(x, y, type);
			
		}
	}
	glFinish();		/* for geforce, to show it in the frontbuffer */

}


void toolbox(void)
{
	int actx, acty, y;
	unsigned short event;
	short val, mval[2], xo= -1, yo=0;
	
	bgntoolbox();
	glColor3ub(0xB0, 0xB0, 0xB0);
	uiDrawMenuBox((float)tbx1, (float)tby1-1, (float)tbx2, (float)tby2);
	drawtoolbox();
	
	/* 
	 *	The active window will be put back in the queue.
	 */

	while(1) {
		event= extern_qread(&val);
		if(event) {
			switch(event) {
				case LEFTMOUSE: case MIDDLEMOUSE: case RIGHTMOUSE: case RETKEY: case PADENTER:
					if(val==1) {
						tbox_getactive(&actx, &acty);
						tbox_setinfo(actx, acty);
						
						if(event==RIGHTMOUSE) {
							if(addmode) {
								addmode= 0;
								drawtoolbox();
							}
						}
						else if(tbstr1 && tbstr1[0]=='>') {
							addmode= tbval;
							drawtoolbox();
						}
						else {
							endtoolbox();
							tbox_execute();
							return;
						}
					}
					break;
				case ESCKEY:
					/* alt keys: to prevent conflicts with over-draw and stow/push/pop at sgis */
#ifndef MAART
/* Temporary for making screen dumps (Alt+PrtSc) */
				case LEFTALTKEY:
				case RIGHTALTKEY:
#endif /* MAART */
					if(val) endtoolbox();
					return;
			}
		}
		
		tbox_getmouse(mval);
		if(mval[0]<tbx1-10 || mval[0]>tbx2+10 || mval[1]<tby1-10 || mval[1]>tby2+10) break;
		
		tbox_getactive(&actx, &acty);
		
		/* mouse handling and redraw */
		if(xo!=actx || yo!=acty) {
			if(actx==0) {
				if (acty==0) addmode=0;
				
				tbox_drawelem(0, tbmain, 0);
				tbox_drawelem(0, acty, 1);
				
				tbmain= acty;
				addmode= 0;
				for(y=0; y<TBOXEL; y++) tbox_drawelem(1, y, 2);
			}
			else if(xo> -1) {
				if(xo==0) tbox_drawelem(xo, yo, 1);
				else tbox_drawelem(xo, yo, 2);
				tbox_drawelem(actx, acty, 3);
			}
			
			glFinish();		/* for geforce, to show it in the frontbuffer */
			
			xo= actx;
			yo= acty;
		}
	}

	endtoolbox();
}

/* ************************************  */

/* this va_ stuff allows printf() style codes in these menus */

static int vconfirm(char *title, char *itemfmt, va_list ap)
{
	char *s, buf[512];

	s= buf;
	if (title) s+= sprintf(s, "%s%%t|", title);
	vsprintf(s, itemfmt, ap);
	
	return (pupmenu(buf)>=0);
}

static int confirm(char *title, char *itemfmt, ...)
{
	va_list ap;
	int ret;
	
	va_start(ap, itemfmt);
	ret= vconfirm(title, itemfmt, ap);
	va_end(ap);
	
	return ret;
}

int okee(char *str, ...)
{
	va_list ap;
	int ret;
	
	va_start(ap, str);
	ret= vconfirm("OK?", str, ap);
	va_end(ap);
	
	return ret;
}

void notice(char *str, ...)
{
	va_list ap;
	
	va_start(ap, str);
	vconfirm(NULL, str, ap);
	va_end(ap);
}

void error(char *fmt, ...)
{
	va_list ap;
	char nfmt[256];

	sprintf(nfmt, "ERROR: %s", fmt);

	va_start(ap, fmt);
	if (G.background || !G.curscreen) {
		vprintf(nfmt, ap);
		printf("\n");
	} else {
		vconfirm(NULL, nfmt, ap);
	}
	va_end(ap);
}

int saveover(char *file)
{
	return (!BLI_exists(file) || confirm("SAVE OVER", file));
}

/* ****************** EXTRA STUFF **************** */

short button(short *var, short min, short max, char *str)
{
	uiBlock *block;
	ListBase listb={0, 0};
	short x1,y1;
	short mval[2], ret=0;

	if(min>max) min= max;

	getmouseco_sc(mval);
	
	if(mval[0]<150) mval[0]=150;
	if(mval[1]<30) mval[1]=30;
	if(mval[0]>G.curscreen->sizex) mval[0]= G.curscreen->sizex-10;
	if(mval[1]>G.curscreen->sizey) mval[1]= G.curscreen->sizey-10;

	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|
				   UI_BLOCK_ENTER_OK);

	x1=mval[0]-150; 
	y1=mval[1]-20; 
	
	uiDefButS(block, NUM, 0, str,	(short)(x1+5),(short)(y1+10),125,20, var,(float)min,(float)max, 0, 0, "");
	uiDefBut(block, BUT, 1, "OK",	(short)(x1+136),(short)(y1+10),25,20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 5);

	ret= uiDoBlocks(&listb, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

short sbutton(char *var, float min, float max, char *str)
{
	uiBlock *block;
	ListBase listb={0, 0};
	short x1,y1;
	short mval[2], ret=0;

	if(min>max) min= max;

	getmouseco_sc(mval);
	
	if(mval[0]<150) mval[0]=150;
	if(mval[1]<30) mval[1]=30;
	if(mval[0]>G.curscreen->sizex) mval[0]= G.curscreen->sizex-10;
	if(mval[1]>G.curscreen->sizey) mval[1]= G.curscreen->sizey-10;

	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);

	x1=mval[0]-150; 
	y1=mval[1]-20; 
	
	uiDefButC(block, TEX, 0, str,	x1+5,y1+10,125,20, var,(float)min,(float)max, 0, 0, "");
	uiDefBut(block, BUT, 1, "OK",	x1+136,y1+10,25,20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 5);

	ret= uiDoBlocks(&listb, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

short fbutton(float *var, float min, float max, float a1, float a2, char *str)
{
	uiBlock *block;
	ListBase listb={0, 0};
	short x1,y1;
	short mval[2], ret=0;

	if(min>max) min= max;

	getmouseco_sc(mval);
	
	if(mval[0]<150) mval[0]=150;
	if(mval[1]<30) mval[1]=30;
	if(mval[0]>G.curscreen->sizex) mval[0]= G.curscreen->sizex-10;
	if(mval[1]>G.curscreen->sizey) mval[1]= G.curscreen->sizey-10;

	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);

	x1=mval[0]-150; 
	y1=mval[1]-20; 
	
	uiDefButF(block, NUM, 0, str,(short)(x1+5),(short)(y1+10),125,20, var, min, max, a1, a2, "");
	uiDefBut(block, BUT, 1, "OK",(short)(x1+136),(short)(y1+10), 35, 20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 2);

	ret= uiDoBlocks(&listb, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

int movetolayer_buts(unsigned int *lay)
{
	uiBlock *block;
	ListBase listb={0, 0};
	int dx, dy, a, x1, y1, sizex=160, sizey=30;
	short pivot[2], mval[2], ret=0;
	
	if(G.vd->localview) {
		error("Not in localview ");
		return ret;
	}

	getmouseco_sc(mval);

	pivot[0]= CLAMPIS(mval[0], (sizex+10), G.curscreen->sizex-30);
	pivot[1]= CLAMPIS(mval[1], (sizey/2)+10, G.curscreen->sizey-(sizey/2)-10);
	
	if (pivot[0]!=mval[0] || pivot[1]!=mval[1])
		warp_pointer(pivot[0], pivot[1]);

	mywinset(G.curscreen->mainwin);
	
	x1= pivot[0]-sizex+10; 
	y1= pivot[1]-sizey/2; 

	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT|UI_BLOCK_ENTER_OK);
	
	dx= (sizex-5)/12;
	dy= sizey/2;
	
	uiBlockBeginAlign(block);
	for(a=0; a<5; a++) 
		uiDefButI(block, TOGR|BIT|a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	for(a=0; a<5; a++) 
		uiDefButI(block, TOGR|BIT|(a+10), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	x1+= 5;
	
	uiBlockBeginAlign(block);
	for(a=5; a<10; a++) 
		uiDefButI(block, TOGR|BIT|a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	for(a=5; a<10; a++) 
		uiDefButI(block, TOGR|BIT|(a+10), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	uiBlockEndAlign(block);

	x1-= 5;
	uiDefBut(block, BUT, 1, "OK", (short)(x1+10*dx+10), (short)y1, (short)(3*dx), (short)(2*dy), NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 2);

	ret= uiDoBlocks(&listb, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

/* ********************** CLEVER_NUMBUTS ******************** */

#define MAXNUMBUTS	24

VarStruct numbuts[MAXNUMBUTS];
void *numbpoin[MAXNUMBUTS];
int numbdata[MAXNUMBUTS];

void draw_numbuts_tip(char *str, int x1, int y1, int x2, int y2)
{
	static char *last=0;	/* avoid ugly updates! */
	int temp;
	
	if(str==last) return;
	last= str;
	if(str==0) return;

	glColor3ub(160, 160, 160); /* MGREY */
	glRecti(x1+4,  y2-36,  x2-4,  y2-16);

	cpack(0x0);

	temp= 0;
	while( BIF_GetStringWidth(G.fonts, str+temp, (U.transopts & USER_TR_BUTTONS))>(x2 - x1-24)) temp++;
	glRasterPos2i(x1+16, y2-30);
	BIF_DrawString(G.fonts, str+temp, (U.transopts & USER_TR_BUTTONS));
}

int do_clever_numbuts(char *name, int tot, int winevent)
{
	ListBase listb= {NULL, NULL};
	uiBlock *block;
	VarStruct *varstr;
	int a, sizex, sizey, x1, y2;
	short mval[2], event;
	
	if(tot<=0 || tot>MAXNUMBUTS) return 0;

	getmouseco_sc(mval);

	/* size */
	sizex= 235;
	sizey= 30+20*(tot+1);
	
	/* center */
	if(mval[0]<sizex/2) mval[0]=sizex/2;
	if(mval[1]<sizey/2) mval[1]=sizey/2;
	if(mval[0]>G.curscreen->sizex -sizex/2) mval[0]= G.curscreen->sizex -sizex/2;
	if(mval[1]>G.curscreen->sizey -sizey/2) mval[1]= G.curscreen->sizey -sizey/2;

	mywinset(G.curscreen->mainwin);
	
	x1= mval[0]-sizex/2; 
	y2= mval[1]+sizey/2;
	
	block= uiNewBlock(&listb, "numbuts", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_ENTER_OK);
	
	/* WATCH IT: TEX BUTTON EXCEPTION */
	/* WARNING: ONLY A SINGLE BIT-BUTTON POSSIBLE: WE WORK AT COPIED DATA! */

	uiDefBut(block, LABEL, 0, name,	(short)(x1+15), (short)(y2-35), (short)(sizex-60), 19, 0, 1.0, 0.0, 0, 0, ""); 

	if(name[0]=='A' && name[7]=='O') {
		y2 -= 20;
		uiDefBut(block, LABEL, 0, "Rotations in degrees!",	(short)(x1+15), (short)(y2-35), (short)(sizex-60), 19, 0, 0.0, 0.0, 0, 0, "");
	}
	
	varstr= &numbuts[0];
	for(a=0; a<tot; a++, varstr++) {
		if(varstr->type==TEX) {
			uiDefBut(block, TEX, 0,	varstr->name,(short)(x1+15),(short)(y2-55-20*a),(short)(sizex-60), 19, numbpoin[a], varstr->min, varstr->max, 0, 0, varstr->tip);
		}
		else  {
			uiDefBut(block, varstr->type, 0,	varstr->name,(short)(x1+15),(short)(y2-55-20*a), (short)(sizex-60), 19, &(numbdata[a]), varstr->min, varstr->max, 100, 0, varstr->tip);
		}
	}

	uiDefBut(block, BUT, 4000, "OK", (short)(x1+sizex-40),(short)(y2-35-20*a), 25, (short)(sizey-50), 0, 0, 0, 0, 0, "OK: Assign Values");

	uiBoundsBlock(block, 5);

	event= uiDoBlocks(&listb, 0);

	areawinset(curarea->win);
	
	if(event & UI_RETURN_OK) {
		
		varstr= &numbuts[0];
		for(a=0; a<tot; a++, varstr++) {
			if(varstr->type==TEX);
			else if ELEM( (varstr->type & BUTPOIN), FLO, INT ) memcpy(numbpoin[a], numbdata+a, 4);
			else if((varstr->type & BUTPOIN)==SHO ) *((short *)(numbpoin[a]))= *( (short *)(numbdata+a));
			
			if( strncmp(varstr->name, "Rot", 3)==0 ) {
				float *fp;
				
				fp= numbpoin[a];
				fp[0]= M_PI*fp[0]/180.0;
			}
		}
		
		if(winevent) {
			ScrArea *sa;
		
			sa= G.curscreen->areabase.first;
			while(sa) {
				if(sa->spacetype==curarea->spacetype) addqueue(sa->win, winevent, 1);
				sa= sa->next;
			}
		}
		
		return 1;
	}
	return 0;
}

void add_numbut(int nr, int type, char *str, float min, float max, void *poin, char *tip)
{
	if(nr>=MAXNUMBUTS) return;

	numbuts[nr].type= type;
	strcpy(numbuts[nr].name, str);
	numbuts[nr].min= min;
	numbuts[nr].max= max;
	if(tip) 
		strcpy(numbuts[nr].tip, tip);
	else
		strcpy(numbuts[nr].tip, "");
	
	
	/*WATCH: TEX BUTTON EXCEPTION */
	
	numbpoin[nr]= poin;
	
	if ELEM( (type & BUTPOIN), FLO, INT ) memcpy(numbdata+nr, poin, 4);
	if((type & BUTPOIN)==SHO ) *((short *)(numbdata+nr))= *( (short *)poin);
	
	if( strncmp(numbuts[nr].name, "Rot", 3)==0 ) {
		float *fp;
		
		fp= (float *)(numbdata+nr);
		fp[0]= 180.0*fp[0]/M_PI;
	}

}

void clever_numbuts(void)
{
	
	if(curarea->spacetype==SPACE_VIEW3D) {
		// panel now
	}
	else if(curarea->spacetype==SPACE_NLA){
		// panel now
	}
	else if(curarea->spacetype==SPACE_IPO) {
		// panel now
	}
	else if(curarea->spacetype==SPACE_SEQ) {
		clever_numbuts_seq();
	}
	else if(curarea->spacetype==SPACE_IMAGE) {
		// panel now
	}
	else if(curarea->spacetype==SPACE_IMASEL) {
		clever_numbuts_imasel();
	}
	else if(curarea->spacetype==SPACE_BUTS){
		clever_numbuts_buts();
	}
	else if(curarea->spacetype==SPACE_OOPS) {
		clever_numbuts_oops();
	}
	else if(curarea->spacetype==SPACE_ACTION){
		// in its own queue
	}
	else if(curarea->spacetype==SPACE_FILE) {
		clever_numbuts_filesel();
	}
}


void replace_names_but(void)
{
	Image *ima= G.main->image.first;
	short len, tot=0;
	char old[64], new[64], temp[80];
	
	strcpy(old, "/");
	strcpy(new, "/");
	
	add_numbut(0, TEX, "Old:", 0, 63, old, 0);
	add_numbut(1, TEX, "New:", 0, 63, new, 0);

	if (do_clever_numbuts("Replace image name", 2, REDRAW) ) {
		
		len= strlen(old);
		
		while(ima) {
			
			if(strncmp(old, ima->name, len)==0) {
				
				strcpy(temp, new);
				strcat(temp, ima->name+len);
				BLI_strncpy(ima->name, temp, sizeof(ima->name));
				
				if(ima->ibuf) IMB_freeImBuf(ima->ibuf);
				ima->ibuf= 0;
				ima->ok= 1;
				
				tot++;
			}
			
			ima= ima->id.next;
		}

		notice("Replaced %d names", tot);
	}
	
}


/* ********************** NEW TOOLBOX ********************** */

ListBase tb_listb= {NULL, NULL};

#define TB_TAB	256
#define TB_ALT	512
#define TB_CTRL	1024
#define TB_PAD	2048
#define TB_SHIFT 4096

typedef struct TBitem {
	int icon;
	char *name;
	int retval;
	void *poin;
} TBitem;

static void tb_do_hotkey(void *arg, int event)
{
	unsigned short key=0, qual1=0, qual2=0;
	
	if(event & TB_CTRL) {
		qual1= LEFTCTRLKEY;
		event &= ~TB_CTRL;
	}
	if(event & TB_ALT) {
		qual1= LEFTALTKEY;
		event &= ~TB_ALT;
	}
	if(event & TB_SHIFT) {
		qual1= LEFTSHIFTKEY;
		event &= ~TB_SHIFT;
	}
	
	if(event & TB_TAB) key= TABKEY;
	else if(event & TB_PAD) {
		event &= ~TB_PAD;
		switch(event) {
		case '0': key= PAD0; break;
		case '5': key= PAD5; break;
		case '/': key= PADSLASHKEY; break;
		case '.': key= PADPERIOD; break;
		case '*': key= PADASTERKEY; break;
		case 'h': key= HOMEKEY; break;
		case 'u': key= PAGEUPKEY; break;
		case 'd': key= PAGEDOWNKEY; break;
		}
	}
	else asciitoraw(event, &key, &qual2);

	if(qual1) mainqenter(qual1, 1);
	if(qual2) mainqenter(qual2, 1);
	mainqenter(key, 1);
	mainqenter(key, 0);
	mainqenter(EXECUTE, 1);
	if(qual1) mainqenter(qual1, 0);
	if(qual2) mainqenter(qual2, 0);
	
}

/* *************Select ********** */

static TBitem tb_object_select_layer1_5[]= {
{	0, "1", 	1, NULL},
{	0, "2", 	2, NULL},
{	0, "3", 	3, NULL},
{	0, "4", 	4, NULL},
{	0, "5", 	5, NULL},
{  -1, "", 		0, do_view3d_select_object_layermenu}};

static TBitem tb_object_select_layer6_10[]= {
{	0, "6", 	6, NULL},
{	0, "7", 	7, NULL},
{	0, "8", 	8, NULL},
{	0, "9", 	9, NULL},
{	0, "10", 	10, NULL},
{  -1, "", 		0, do_view3d_select_object_layermenu}};

static TBitem tb_object_select_layer11_15[]= {
{	0, "11", 	11, NULL},
{	0, "12",	12, NULL},
{	0, "13", 	13, NULL},
{	0, "14", 	14, NULL},
{	0, "15", 	15, NULL},
{  -1, "", 		0, do_view3d_select_object_layermenu}};

static TBitem tb_object_select_layer16_20[]= {
{	0, "16", 	16, NULL},
{	0, "17", 	17, NULL},
{	0, "18", 	18, NULL},
{	0, "19", 	19, NULL},
{	0, "20", 	20, NULL},
{  -1, "", 		0, do_view3d_select_object_layermenu}};

static TBitem tb_object_select_layer[]= {
{	0, "Layers 1-5", 	0, 		tb_object_select_layer1_5},
{	0, "Layers 6-10", 	0, 		tb_object_select_layer6_10},
{	0, "Layers 11-15", 	0, 		tb_object_select_layer11_15},
{	0, "Layers 16-20", 	0, 		tb_object_select_layer16_20},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_object_select_type[]= {
{	0, "Mesh", 		1, NULL},
{	0, "Curve", 	2, NULL},
{	0, "Surface", 	3, NULL},
{	0, "Meta", 		4, NULL},
{	0, "SEPR",		0, NULL},
{	0, "Armature", 	5, NULL},
{	0, "Lattice", 	6, NULL},
{	0, "Text", 		7, NULL},
{	0, "Empty", 	8, NULL},
{	0, "SEPR",		0, NULL},
{	0, "Camera", 	9, NULL},
{	0, "Lamp", 		10, NULL},
{  -1, "", 			0, do_view3d_select_object_typemenu}};

static TBitem tb_object_select_linked[]= {
{	0, "Object Ipo|Shift L, 1", 	1, NULL},
{	0, "ObData|Shift L, 2", 	2, NULL},
{	0, "Material|Shift L, 3", 	3, NULL},
{	0, "Texture|Shift L, 4", 	4, NULL},
{  -1, "", 			0, do_view3d_select_object_linkedmenu}};

static TBitem tb_object_select_grouped[]= {
{	0, "Children|Shift G, 1", 	1, NULL},
{	0, "Immediate Children|Shift G, 2", 	2, NULL},
{	0, "Parent|Shift G, 3", 	3, NULL},
{	0, "Objects on Shared Layers|Shift G, 4", 	4, NULL},
{  -1, "", 			0, do_view3d_select_object_groupedmenu}};

static TBitem tb_object_select[]= {
{	0, "Border Select|B", 	'b', NULL},
{	0, "SEPR",				0, NULL},
{	0, "Select/Deselect All|A", 	'a', NULL},
{	0, "Select All by Layer", 	0, 		tb_object_select_layer},
{	0, "Select All by Type", 	0, 		tb_object_select_type},
{	0, "SEPR",				0, NULL},
{	0, "Linked", 	0, 	tb_object_select_linked},
{	0, "Grouped", 	0, 	tb_object_select_grouped},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_mesh_select[]= {
{	0, "Border Select|B",               0, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "(De)select All|A",              2, NULL},
{	0, "Inverse",                       3, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "Random...",			            5, NULL},
{	0, "Non-Manifold|Shift Ctrl Alt M", 9, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "More|Ctrl NumPad +",            7, NULL},
{	0, "Less|Ctrl NumPad -",            8, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "Vertex Loop...|Alt-B",          10, NULL},
{	0, "Face Loop...|Shift R",          6, NULL},
{	0, "Linked Vertices|Ctrl L",        4, NULL},
{  -1, "", 			0, do_view3d_select_meshmenu}};


static TBitem tb_curve_select[]= {
{	0, "Border Select|B", 	0, NULL},
{	0, "SEPR", 				0, NULL},
{	0, "(De)select All|A", 	2, NULL},
{	0, "Inverse", 			3, NULL},
{	0, "Row", 			5, NULL},
{  -1, "", 				0, do_view3d_select_curvemenu}};

static TBitem tb__select[]= {
{	0, "Border Select|B", 	'b', NULL},
{	0, "(De)select All|A", 	'a', NULL},
{  -1, "", 			0, tb_do_hotkey}};


/* *************Edit ********** */

static TBitem tb_edit[]= {
{	0, "Exit Editmode|Tab", 	TB_TAB, NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_curve_edit_seg[]= {
{	0, "Subdivide", 		0, NULL},
{	0, "Switch Direction", 	1, NULL},
{  -1, "", 			0, do_view3d_edit_curve_segmentsmenu}};

static TBitem tb_curve_edit_cv[]= {
{	0, "Tilt|T", 	't', NULL},
{	0, "Clear Tilt|Alt T", 			TB_ALT|'t', NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Automatic|Shift H", 		'H', NULL},
{	0, "Toggle Free/Aligned|H", 	'h', NULL},
{	0, "Vector|V", 					TB_ALT|'t', NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Make Vertex Parent|Ctrl P", TB_CTRL|'p', NULL},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_curve_edit[]= {
{	0, "Exit Editmode|Tab", 	TB_TAB, NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Extrude|E", 		'e', 		NULL},
{	0, "Duplicate|Shift D", 'D', 		NULL},
{	0, "Make Segment|F", 	'f', 		NULL},
{	0, "Toggle Cyclic|F", 	'c', 		NULL},
{	0, "Delete...|X", 		'x', 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Control Points", 	0, 		tb_curve_edit_cv},
{	0, "Segments", 	0, 		tb_curve_edit_seg},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_mesh_edit_vertex[]= {
{	0, "Merge...|Alt M", 		5, NULL},
{	0, "Split|Y", 				4, 		NULL},
{	0, "Separate|P", 			3, 		NULL},
{	0, "SEPR",					0, NULL},
{	0, "Smooth|W, 0", 			2, NULL},
{	0, "Remove Doubles|W, 5", 			1, NULL},
{	0, "SEPR",					0, NULL},
{	0, "Make Vertex Parent|Ctrl P", 	0, NULL},
{  -1, "", 			0, do_view3d_edit_mesh_verticesmenu}};

static TBitem tb_mesh_edit_edge[]= {
{	0, "Make Edge/Face|F", 			5, 		NULL},
{	0, "SEPR",						0, NULL},
{	0, "Bevel", 					6, 		NULL},
{	0, "Loop Subdivide|Ctrl R", 		4, 		NULL},
{	0, "Knife Subdivide...|Shift K", 	3, 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Subdivide|W, 1", 			2, 		NULL},
{	0, "Subdivide Fractal|W, 2", 	1, 		NULL},
{	0, "Subdivide Smooth|W, 3", 		0, 		NULL},
{  -1, "", 			0, do_view3d_edit_mesh_edgesmenu}};

static TBitem tb_mesh_edit_face[]= {
{	0, "Make Edge/Face|F", 			'f', 		NULL},
{	0, "Fill|Shift F", 				'F', 		NULL},
{	0, "Beaty Fill|Alt F", 			TB_ALT|'f', 		NULL},
{	0, "SEPR",					0, NULL},
{	0, "Convert to Triangles|Ctrl T", 	TB_CTRL|'t', 		NULL},
{	0, "Convert to Quads|Alt J", 		TB_ALT|'j', 		NULL},
{	0, "Flip Triangle Edges|Ctrl F", 	TB_CTRL|'f', 		NULL},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_mesh_edit_normal[]= {
{	0, "Recalculate Outside", 	2, 		NULL},
{	0, "Recalculate Inside", 	1, 		NULL},
{	0, "SEPR",					0, NULL},
{	0, "Flip", 				0, 		NULL},
{  -1, "", 			0, do_view3d_edit_mesh_normalsmenu}};

static TBitem tb_mesh_edit[]= {
{	0, "Exit Editmode|Tab", 	TB_TAB, NULL},
{	0, "Undo|U", 			'u', 		NULL},
{	0, "Redo|Shift U", 		'U', 		NULL},
{	0, "SEPR", 				0, 			NULL},
{	0, "Extrude|E", 		'e', 		NULL},
{	0, "Duplicate|Shift D", 'D', 		NULL},
{	0, "Delete...|X", 		'x', 		NULL},
{	0, "SEPR", 				0, 			NULL},
{	0, "Vertices", 		0, 		tb_mesh_edit_vertex},
{	0, "Edges", 		0, 		tb_mesh_edit_edge},
{	0, "Faces", 		0, 		tb_mesh_edit_face},
{	0, "Normals", 		0, 		tb_mesh_edit_normal},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_object_ipo[]= {
{	0, "Show/Hide", 	'k', NULL},
{	0, "Select Next", 	TB_PAD|'u', NULL},
{	0, "Select Prev", 	TB_PAD|'d', NULL},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_object_edit[]= {
{	0, "Enter Editmode|Tab", 	TB_TAB, NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Duplicate|Shift D", 		'D', 		NULL},
{	0, "Duplicate Linked|Alt D", 	TB_ALT|'d', NULL},
{	0, "Delete|X", 					'x', 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Object Keys", 	0, tb_object_ipo},
{  -1, "", 			0, tb_do_hotkey}};


/* ************* Type  ********** */

static TBitem tb_obdata_hide[]= {
{	0, "Show Hidden|Alt H", 		TB_ALT|'h', 		NULL},
{	0, "Hide Selected|H", 			'h', 		NULL},
{	0, "Hide Deselected|Shift H", 	'H', 		NULL},
{  -1, "", 			0, tb_do_hotkey}};

static void tb_do_mesh(void *arg, int event){
	Mesh *me= get_mesh(OBACT);
	switch(event) {
	case 1: common_insertkey(); break;
	case 2: G.f ^= G_DRAWEDGES; break;
	case 3: G.f ^= G_DRAWFACES; break;
	case 4: G.f ^= G_DRAWNORMALS; break;
	case 5: me->flag ^= ME_SUBSURF; makeDispList(OBACT); break;
	case 6: me->flag ^= ME_OPT_EDGES; makeDispList(OBACT); break;
	}
	addqueue(curarea->win, REDRAW, 1);
}

static TBitem tb_mesh[]= {
{	0, "Insert Keyframe|I", 		1, 		NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Show/Hide Edges", 			2, 		NULL},
{	0, "Show/Hide Faces", 			3, 		NULL},
{	0, "Show/Hide Normals", 		4, 		NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Subdivision Surface", 		5, 		NULL},
{	0, "SubSurf Optimal", 			6, 		NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Show/Hide Vertices", 	0, 		tb_obdata_hide},
{  -1, "", 			0, tb_do_mesh}};

static TBitem tb_curve_hide[]= {
{	0, "Show Hidden|Alt H", 		10, 		NULL},
{	0, "Hide Selected|H", 			11, 		NULL},
{  -1, "", 			0, do_view3d_edit_curve_showhidemenu}};


static TBitem tb_curve[]= {
{	0, "Insert Keyframe|I", 		'i', 		NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Show/Hide Points", 	0, 		tb_curve_hide},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_obdata[]= {
{	0, "Duplicate|Shift D", 		'D', 		NULL},
{	0, "Delete|X", 					'x', 		NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_object_parent[]= {
{	0, "Make Parent...|Ctrl P", 		TB_CTRL|'p', NULL},
{	0, "Clear Parent...|Alt P", 		TB_ALT|'p', NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_object_track[]= {
{	0, "Make Track|Ctrl T", 		TB_CTRL|'t', NULL},
{	0, "Clear Track|Alt T", 		TB_ALT|'t', NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_object[]= {
{	0, "Insert Keyframe|I", 		'i', 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Make Links...|Ctrl L", 		TB_CTRL|'l', NULL},
{	0, "Make Single User...|U", 	'u', 		NULL},
{	0, "Copy Attributes...|Ctrl C", TB_CTRL|'c', NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Parent", 	0, 		tb_object_parent},
{	0, "Track", 	0, 		tb_object_track},
{	0, "SEPR", 								0, NULL},
{	0, "Boolean Operation|W", 	'w', NULL},
{	0, "Join Objects...|Ctrl J", 	TB_CTRL|'j', NULL},
{	0, "Convert Object Type...|Alt C", 	TB_ALT|'c', NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Move to Layer...|M", 		'm', NULL},
{  -1, "", 			0, tb_do_hotkey}};


/* *************VIEW ********** */

static void tb_do_view_dt(void *arg, int event){
	G.vd->drawtype= event;
	addqueue(curarea->win, REDRAW, 1);
}

static TBitem tb_view_dt[]= {
{	ICON_BBOX, "Bounding Box", 	1, NULL},
{	ICON_WIRE, "Wireframe", 	2, NULL},
{	ICON_SOLID, "Solid", 		3, NULL},
{	ICON_SMOOTH, "Shaded", 		5, NULL},
{	ICON_POTATO, "Textured", 	5, NULL},
{  -1, "", 			0, tb_do_view_dt}};

static TBitem tb_view_alignview[]= {
{	0, "Centre View to Cursor|C", 		'c', NULL},
{	0, "Align Active Camera to View|Shift NumPad 0", 		TB_SHIFT|TB_PAD|'0', NULL},
{	0, "Align View to Selected|NumPad *", 		TB_PAD|'*', NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_view[]= {
{	0, "Viewport Shading", 			0, tb_view_dt},
{	0, "SEPR", 						0, NULL},
{	0, "Ortho/Perspective|NumPad 5", 	TB_PAD|'5', NULL},
{	0, "Local/Global View|NumPad /", 	TB_PAD|'/', NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Align View", 			0, tb_view_alignview},
{	0, "SEPR", 		0, NULL},
{	0, "View Selected|NumPad .", 	TB_PAD|'.', NULL},
{	0, "View All|Home", 		TB_PAD|'h', NULL},
{	0, "SEPR", 		0, NULL},
{	0, "Play Back Animation|Alt A", TB_ALT|'a', NULL},
{	0, "Camera Fly Mode|Shift F", TB_SHIFT|'f', NULL},
{  -1, "", 			0, tb_do_hotkey}};


/* *************TRANSFORM ********** */


static void tb_do_transform_moveaxis(void *arg, int event)
{
	switch(event)
	{
	    case 0: /* X Global */
		    transform('g'*'X');
			break;
		case 1: /* Y Global */
			transform('g'*'Y');
			break;
		case 2: /* Z Global */
			transform('g'*'Z');
			break;
		case 3: /* X Local */
			transform('g'*'x');
			break;
		case 4: /* Y Local */
			transform('g'*'y');
			break;
		case 5: /* Z Local */
			transform('g'*'z');
			break;
	}
}

static TBitem tb_transform_moveaxis[]= {
{	0, "X Global|G, X", 	0, NULL},
{	0, "Y Global|G, Y", 	1, NULL},
{	0, "Z Global|G, Z", 	2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X Local|G, X, X", 	3, NULL},
{	0, "Y Local|G, Y, Y", 	4, NULL},
{	0, "Z Local|G, Z, Z", 	5, NULL},
{  -1, "", 			0, tb_do_transform_moveaxis}};

static void tb_do_transform_rotateaxis(void *arg, int event)
{
	switch(event)
	{
	    case 0: /* X Global */
		    transform('r'*'X');
			break;
		case 1: /* Y Global */
			transform('r'*'Y');
			break;
		case 2: /* Z Global */
			transform('r'*'Z');
			break;
		case 3: /* X Local */
			transform('r'*'x');
			break;
		case 4: /* Y Local */
			transform('r'*'y');
			break;
		case 5: /* Z Local */
			transform('r'*'z');
			break;
	}
}

static TBitem tb_transform_rotateaxis[]= {
{	0, "X Global|R, X", 	0, NULL},
{	0, "Y Global|R, Y", 	1, NULL},
{	0, "Z Global|R, Z", 	2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X Local|R, X, X", 	3, NULL},
{	0, "Y Local|R, Y, Y", 	4, NULL},
{	0, "Z Local|R, Z, Z", 	5, NULL},

{  -1, "", 			0, tb_do_transform_rotateaxis}};


static void tb_do_transform_scaleaxis(void *arg, int event)
{
	switch(event)
	{
	    case 0: /* X Global */
		    transform('s'*'X');
			break;
		case 1: /* Y Global */
			transform('s'*'Y');
			break;
		case 2: /* Z Global */
			transform('s'*'Z');
			break;
		case 3: /* X Local */
			transform('s'*'x');
			break;
		case 4: /* Y Local */
			transform('s'*'y');
			break;
		case 5: /* Z Local */
			transform('s'*'z');
			break;
	}
}

static TBitem tb_transform_scaleaxis[]= {
{	0, "X Global|S, X", 	0, NULL},
{	0, "Y Global|S, Y", 	1, NULL},
{	0, "Z Global|S, Z", 	2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X Local|S, X, X", 	3, NULL},
{	0, "Y Local|S, Y, Y", 	4, NULL},
{	0, "Z Local|S, Z, Z", 	5, NULL},
{  -1, "", 			0, tb_do_transform_scaleaxis}};

static void tb_do_transform_clearapply(void *arg, int event)
{
	switch(event)
	{
	    case 0: /* clear location */
			clear_object('g');
			break;
		case 1: /* clear rotation */
			clear_object('r');
			break;
		case 2: /* clear size */
			clear_object('s');
			break;
		case 3: /* apply size/rotation */
			apply_object();
			break;
		case 4: /* apply deformation */
			make_duplilist_real();
			break;
	}
}

static TBitem tb_transform_clearapply[]= {
{	0, "Clear Location", 		0, NULL},
{	0, "Clear Rotation", 		1, NULL},
{	0, "Clear Size", 			2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "Apply Size/Rotation|Ctrl A", 3, NULL},
{	0, "Apply Deformation|Shift Ctrl A", 4, NULL},
{	0, "Make Duplicates Real|Shift Ctrl A", 4, NULL},
{  -1, "", 			0, tb_do_transform_clearapply}};

static TBitem tb_transform_snap[]= {
{	0, "Selection -> Grid|Shift S, 1", 		1, NULL},
{	0, "Selection -> Cursor|Shift S, 2", 	2, NULL},
{	0, "Cursor -> Grid|Shift S, 3", 		3, NULL},
{	0, "Cursor -> Selection|Shift S, 4", 4, NULL},
{	0, "Selection -> Center|Shift S, 5", 5, NULL},
{  -1, "", 			0, do_view3d_edit_snapmenu}};

static void tb_do_transform(void *arg, int event)
{
	switch(event)
	{
		case 0: /* Grab/move */
			transform('g');
			break;
		case 1: /* Rotate */
			transform('r');
			break;
		case 2: /* Scale */
			transform('s');
			break;
		case 3: /* transform properties */
			mainqenter(NKEY, 1);
			break;
		case 4: /* snap */
			snapmenu();
			break;
	}
}

static TBitem tb_transform_object_mirror[]= {
{	0, "X Local|Ctrl M, 1", 	1, NULL},
{	0, "Y Local|Ctrl M, 2", 	2, NULL},
{	0, "Z Local|Ctrl M, 3", 	3, NULL},
{  -1, "", 			0, do_view3d_object_mirrormenu}};

static TBitem tb_transform[]= {
{	0, "Grab/Move|G", 	0, NULL},
{	0, "Grab/Move on Axis| ", 	0, tb_transform_moveaxis},
{	0, "Rotate|R", 		1, NULL},
{	0, "Rotate on Axis", 	0, tb_transform_rotateaxis},
{	0, "Scale|S", 		2, NULL},
{	0, "Scale on Axis", 	0, tb_transform_scaleaxis},
{	0, "SEPR", 					0, NULL},
{	0, "Mirror", 	0, tb_transform_object_mirror},
{	0, "SEPR", 					0, NULL},
{	ICON_MENU_PANEL, "Properties|N", 3, NULL},
{	0, "Snap", 		0, tb_transform_snap},
{	0, "SEPR", 					0, NULL},
{	0, "Clear/Apply", 	0, tb_transform_clearapply},
{  -1, "", 			0, tb_do_transform}};

static TBitem tb_transform_edit_mirror[]= {
{	0, "X Global|Ctrl M, 1", 	1, NULL},
{	0, "Y Global|Ctrl M, 2", 	2, NULL},
{	0, "Z Global|Ctrl M, 3", 	3, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X Local|Ctrl M, 4", 	4, NULL},
{	0, "Y Local|Ctrl M, 5", 	5, NULL},
{	0, "Z Local|Ctrl M, 6", 	6, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X View|Ctrl M, 7", 	7, NULL},
{	0, "Y View|Ctrl M, 8", 	8, NULL},
{	0, "Z View|Ctrl M, 9", 	9, NULL},
{  -1, "", 			0, do_view3d_edit_mirrormenu}};

static TBitem tb_transform_editmode1[]= {
{	0, "Grab/Move|G", 	'g', NULL},
{	0, "Grab/Move on Axis| ", 	0, tb_transform_moveaxis},
{	0, "Rotate|R", 		'r', NULL},
{	0, "Rotate on Axis", 	0, tb_transform_rotateaxis},
{	0, "Scale|S", 		's', NULL},
{	0, "Scale on Axis", 	0, tb_transform_scaleaxis},
{	0, "SEPR", 					0, NULL},
{	0, "Mirror", 	0, tb_transform_edit_mirror},
{	0, "Shrink/Fatten|Alt S", TB_ALT|'s', NULL},
{	0, "Shear|Ctrl S", TB_CTRL|'s', NULL},
{	0, "Warp|Shift W", 	'W', NULL},
{	0, "SEPR", 					0, NULL},
{	ICON_MENU_PANEL, "Properties|N", 'N', NULL},
{	0, "Snap", 		0, tb_transform_snap},
{	0, "SEPR", 					0, NULL},
{	0, "Proportional Edit|O", 	'o', 		NULL},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_transform_editmode2[]= {
{	0, "Grab/Move|G", 	0, NULL},
{	0, "Grab/Move on Axis| ", 	0, tb_transform_moveaxis},
{	0, "Rotate|R", 		1, NULL},
{	0, "Rotate on Axis", 	0, tb_transform_rotateaxis},
{	0, "Scale|S", 		2, NULL},
{	0, "Scale on Axis", 	0, tb_transform_scaleaxis},
{	0, "SEPR", 					0, NULL},
{	ICON_MENU_PANEL, "Properties|N", 'N', NULL},
{	0, "Snap", 		0, tb_transform_snap},
{  -1, "", 			0, tb_do_transform}};


/* *************ADD ********** */

static TBitem addmenu_mesh[]= {
{	0, "Plane", 	0, NULL},
{	0, "Cube", 		1, NULL},
{	0, "Circle", 	2, NULL},
{	0, "UVsphere", 	3, NULL},
{	0, "Icosphere", 4, NULL},
{	0, "Cylinder", 	5, NULL},
{	0, "Tube", 		6, NULL},
{	0, "Cone", 		7, NULL},
{	0, "SEPR",		0, NULL},
{	0, "Grid", 		8, NULL},
{	0, "Monkey", 	9, NULL},
{  -1, "", 			0, do_info_add_meshmenu}};

static TBitem addmenu_curve[]= {
{	0, "Bezier Curve", 	0, NULL},
{	0, "Bezier Circle", 1, NULL},
{	0, "NURBS Curve", 	2, NULL},
{	0, "NURBS Circle", 	3, NULL},
{	0, "Path", 			4, NULL},
{  -1, "", 			0, do_info_add_curvemenu}};

static TBitem addmenu_surf[]= {
{	0, "NURBS Curve", 	0, NULL},
{	0, "NURBS Circle", 	1, NULL},
{	0, "NURBS Surface", 2, NULL},
{	0, "NURBS Tube", 	3, NULL},
{	0, "NURBS Sphere", 	4, NULL},
{	0, "NURBS Donut", 	5, NULL},
{  -1, "", 			0, do_info_add_surfacemenu}};

static TBitem addmenu_meta[]= {
{	0, "Meta Ball", 	0, NULL},
{	0, "Meta Tube", 	1, NULL},
{	0, "Meta Plane", 	2, NULL},
{	0, "Meta Ellipsoid", 3, NULL},
{	0, "Meta Cube", 	4, NULL},
{  -1, "", 			0, do_info_add_metamenu}};

static TBitem addmenu_armature[]= {
{	0, "Bones", 	8, NULL},
{  -1, "", 			0, do_info_addmenu}};


static TBitem tb_add[]= {
{	0, "Mesh", 		0, addmenu_mesh},
{	0, "Curve", 	1, addmenu_curve},
{	0, "Surface", 	2, addmenu_surf},
{	0, "Meta", 	3, addmenu_meta},
{	0, "Text", 		4, NULL},
{	0, "Empty", 	5, NULL},
{	0, "SEPR", 		0, NULL},
{	0, "Camera", 	6, NULL},
{	0, "Lamp", 		7, NULL},
{	0, "SEPR", 		0, NULL},
{	0, "Armature", 	8, NULL},
{	0, "Lattice", 	9, NULL},
{  -1, "", 			0, do_info_addmenu}};

static TBitem tb_empty[]= {
{	0, "Nothing...", 	0, NULL},
{  -1, "", 		0, NULL}};



static uiBlock *tb_makemenu(void *arg)
{
	static int counter=0;
	TBitem *item= arg, *itemt;
	uiBlock *block;
	int yco= 0;
	char str[10];
	
	if(arg==NULL) return NULL;
	
	sprintf(str, "tb %d\n", counter++);
	block= uiNewBlock(&tb_listb, str, UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetCol(block, TH_MENU_ITEM);

	// last item has do_menu func, has to be stored in each button
	itemt= item;
	while(itemt->icon != -1) itemt++;
	uiBlockSetButmFunc(block, itemt->poin, NULL);

	// now make the buttons
	while(item->icon != -1) {

		if(strcmp(item->name, "SEPR")==0) {
			uiDefBut(block, SEPR, 0, "", 0, yco-=6, 50, 6, NULL, 0.0, 0.0, 0, 0, "");
		}
		else if(item->icon) {
			uiDefIconTextBut(block, BUTM, 1, item->icon, item->name, 0, yco-=20, 80, 19, NULL, 0.0, 0.0, 0, item->retval, "");
		}
		else if(item->poin) {
			uiDefIconTextBlockBut(block, tb_makemenu, item->poin, ICON_RIGHTARROW_THIN, item->name, 0, yco-=20, 80, 19, "");
		}
		else {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, item->name, 0, yco-=20, 80, 19, NULL, 0.0, 0.0, 0, item->retval, "");
		}
		item++;
	}
	uiTextBoundsBlock(block, 80);
	
	/* direction is also set in the function that calls this */
	uiBlockSetDirection(block, UI_RIGHT|UI_CENTRE);

	return block;
}

static int tb_mainx= 0, tb_mainy= -5;
static void store_main(void *arg1, void *arg2)
{
	tb_mainx= (int)arg1;
	tb_mainy= (int)arg2;
}

void toolbox_n(void)
{
	uiBlock *block;
	uiBut *but;
	TBitem *menu1=NULL, *menu2=NULL, *menu3=NULL; 
	TBitem *menu4=NULL, *menu5=NULL, *menu6=NULL;
	int dx;
	short event, mval[2], tot=0;
	char *str1=NULL, *str2=NULL, *str3=NULL, *str4=NULL, *str5=NULL, *str6=NULL;
	
	mywinset(G.curscreen->mainwin); // we go to screenspace
	
	block= uiNewBlock(&tb_listb, "toolbox", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);
	uiBlockSetCol(block, TH_MENU_ITEM);
	
	dx= 65;
	
	/* select context for main items */
	if(curarea->spacetype==SPACE_VIEW3D) {
		/* standard menu */
		menu1= tb_object; str1= "Object";
		menu2= tb_add; str2= "Add";
		menu3= tb_object_select; str3= "Select";
		menu4= tb_object_edit; str4= "Edit";
		menu5= tb_transform; str5= "Transform";
		menu6= tb_view; str6= "View";
		
		if(G.obedit) {
			if(G.obedit->type==OB_MESH) {
				menu1= tb_mesh; str1= "Mesh";
				menu2= addmenu_mesh; 
				menu3= tb_mesh_select;
				menu4= tb_mesh_edit; 
				menu5= tb_transform_editmode1;
			}
			else if(G.obedit->type==OB_CURVE) {
				menu1= tb_curve; str1= "Curve";
				menu2= addmenu_curve;
				menu3= tb_curve_select;
				menu4= tb_curve_edit;
				menu5= tb_transform_editmode1;
			}
			else if(G.obedit->type==OB_SURF) {
				menu1= tb_obdata; str1= "Surface";
				menu2= addmenu_surf; 
				menu3= tb_curve_select;
				menu4= tb_curve_edit;
				menu5= tb_transform_editmode1;
			}
			else if(G.obedit->type==OB_MBALL) {
				menu1= tb_obdata; str1= "Meta";
				menu2= addmenu_meta;
				menu3= tb__select;
				menu4= tb_edit;
				menu5= tb_transform_editmode2;
			}
			else if(G.obedit->type==OB_ARMATURE) {
				menu1= tb_obdata;str1= "Armature";
				menu2= addmenu_armature;
				menu3= tb__select;
				menu4= tb_edit;
				menu5= tb_transform_editmode2;
			}
			else if(G.obedit->type==OB_LATTICE) {
				menu1= tb_empty;str1= "Lattice";
				menu2= tb_empty;
				menu3= tb__select;
				menu4= tb_edit;
				menu5= tb_transform_editmode1;
			}
			
		}
		else {
		}
		tot= 6;
	}
	
	getmouseco_sc(mval);
	
	/* create the main buttons menu */
	if(tot==6) {
	
		/* check if it fits */
		if(mval[0]-1.5*dx+tb_mainx < 6) mval[0]= 6 + 1.5*dx -tb_mainx;
		else if(mval[0]+1.5*dx+tb_mainx > G.curscreen->sizex-6) 
			mval[0]= G.curscreen->sizex-6-1.5*dx-tb_mainx;

		if(mval[1]-20+tb_mainy < 6) mval[1]= 6+20 -tb_mainy;
		else if(mval[1]+20+tb_mainy > G.curscreen->sizey-6) 
			mval[1]= G.curscreen->sizey-6-20-tb_mainy;
	
		but=uiDefBlockBut(block, tb_makemenu, menu1, str1,	mval[0]-1.5*dx+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_TOP|UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)dx, (void *)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu2, str2,	mval[0]-0.5*dx+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_TOP);
		uiButSetFunc(but, store_main, (void *)0, (void *)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu3, str3,	mval[0]+0.5*dx+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_TOP|UI_MAKE_LEFT);
		uiButSetFunc(but, store_main, (void *)-dx, (void *)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu4, str4,	mval[0]-1.5*dx+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN|UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)dx, (void *)5);

		but=uiDefBlockBut(block, tb_makemenu, menu5, str5,	mval[0]-0.5*dx+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN);
		uiButSetFunc(but, store_main, (void *)0, (void *)5);

		but=uiDefBlockBut(block, tb_makemenu, menu6, str6,	mval[0]+0.5*dx+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN|UI_MAKE_LEFT);
		uiButSetFunc(but, store_main, (void *)-dx, (void *)5);
	}
	
	uiBoundsBlock(block, 2);
	event= uiDoBlocks(&tb_listb, 0);
	
	mywinset(curarea->win);
}

void toolbox_n_add(void)
{
	tb_mainx= 0;
	tb_mainy= -5;
	toolbox_n();
}

