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

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_plugin_types.h"
#include "BKE_utildefines.h"
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

#include "IMB_imbuf.h"

#include "mydevice.h"
#include "blendef.h"

#include "interface.h"
#include "render.h"

/*   ********  NOTES  ***********	*****************************
	
	- Toolbox items zelf invullen
	- de colormap kleuren staan met comments in de bgntoolbox()
	- de funktie extern_qread eventueel vervangen
	- let op de benaming van bijzondere toetsen (NumL etc) 
	- meelinken: Button.c,  ivm rawcodes
	
	*****************************	*****************************
*/	


static int tbx1, tbx2, tby1, tby2, tbfontyofs, tbmain=0;
static int tbmemx=TBOXX/2, tbmemy=(TBOXEL-0.5)*TBOXH, tboldwin, addmode= 0;
static int oldcursor;

	/* variabelen per item */
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
	/* afhankelijk van tbmain worden vars gezet */
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
					case 3: tbstr= "Text";		tbstr1= "";		keystr= "";		tbval=OB_FONT;		tbfunc= add_primitiveFont;		break;
					case 4: tbstr= "MetaBall";	tbstr1= "";		keystr= "";		tbval=OB_MBALL;		tbfunc= add_primitiveMball;		break;
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
/*			else if(addmode==OB_MBALL) {
				switch(y) {
					case 0: tbstr= "Ball";		tbstr1= "A";	tbval=1; break;
					case 1: tbstr= "";			tbstr1= "";		break;
					case 2: tbstr= "";			tbstr1= "";		break;
					case 3: tbstr= "";			tbstr1= "";		break;
					case 4: tbstr= "";			tbstr1= "";		break;
					case 5: tbstr= "";			tbstr1= "";		break;
					case 6: tbstr= "";			tbstr1= "";		break;
					case 7: tbstr= "";			tbstr1= "";		break;
					case 8: tbstr= "";			tbstr1= "";		break;
					case 9: tbstr= "";			tbstr1= "";		break;
					case 10: tbstr= "";			tbstr1= "";		break;
					case 11: tbstr= "Duplicate";tbstr1= "D";	break;
				}
				if(tbstr1 && tbstr1[0]=='A') tbfunc= add_primitiveMball;
			}*/
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
	#if defined(__sgi) || defined(__sun__)
	/* this is a dirty patch: XgetImage gets sometimes the backbuffer */
	my_get_frontbuffer_image(0, 0, 1, 1);
	my_put_frontbuffer_image();
	#endif

	tboldwin= mywinget();

	mywinset(G.curscreen->mainwin);
	
	/* pietsje groter, 1 pixel aan de rand */
	
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);

	glFinish();

	my_get_frontbuffer_image(startx-1, starty-4, endx-startx+5, endy-starty+6);

	oldcursor= get_cursor();
	set_cursor(CURSOR_STD);
	
	tbfontyofs= (TBOXH-11)/2 +2;	/* toolbox, hier stond ooit getheigh */
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
	/* als tbfunc: functie aanroepen */
	/* als tbstr1 is een string: value tbval in queue stopen */
	unsigned short event=0;
	unsigned short qual1=0, qual2=0;

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
		else if(strcmp(tbstr1, "W")==0) {
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
	
	/* tekst */
	tbox_setinfo(x, y);
	if(tbstr && tbstr[0]) {
		len1= 5+BMF_GetStringWidth(G.font, tbstr);
//		if(tbstr1) len2= 5+BMF_GetStringWidth(G.font, tbstr1); else len2= 0;
		if(keystr) len2= 5+BMF_GetStringWidth(G.font, keystr); else len2= 0;
		
		while(len1>0 && (len1+len2+5>x2-x1) ) {
			tbstr[strlen(tbstr)-1]= 0;
			len1= BMF_GetStringWidth(G.font, tbstr);
		}
		
		glRasterPos2i(x1+5, y1+tbfontyofs);
		BMF_DrawString(G.font, tbstr);
		
//		if(tbstr1 && tbstr1[0]) {
		if(keystr && keystr[0]) {
			if(type & 1) {
				ColorFunc(TBOXBLACK);
	
				glRecti(x2-len2-2,  y1+2,  x2-3,  y2-2);
				ColorFunc(TBOXWHITE);
				glRasterPos2i(x2-len2,  y1+tbfontyofs);
//				BMF_DrawString(G.font, tbstr1);
				BMF_DrawString(G.font, keystr);
			}
			else {
				ColorFunc(TBOXBLACK);
				glRasterPos2i(x2-len2,  y1+tbfontyofs);
//				BMF_DrawString(G.font, tbstr1);
				BMF_DrawString(G.font, keystr);
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

	/* de background */
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

	/* de text */
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
	 *	De aktieve window wordt in queue terug gestopt.
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
					/* altkeys: om conflicten met overdraw en stow/push/pop te voorkomen */
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
		
		/* muisafhandeling en redraw */
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

/* ****************** EXTRAATJE **************** */

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

	block= uiNewBlock(&listb, "button", UI_EMBOSSX, UI_HELV, G.curscreen->mainwin);
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

	block= uiNewBlock(&listb, "button", UI_EMBOSSX, UI_HELV, G.curscreen->mainwin);
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

short fbutton(float *var, float min, float max, char *str)
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

	block= uiNewBlock(&listb, "button", UI_EMBOSSX, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);

	x1=mval[0]-150; 
	y1=mval[1]-20; 
	
	uiDefButF(block, NUM, 0, str,(short)(x1+5),(short)(y1+10),125,20, var, min, max, 0, 0, "");
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

	block= uiNewBlock(&listb, "button", UI_EMBOSSX, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT|UI_BLOCK_RET_1);
	
	dx= (sizex-5)/12;
	dy= sizey/2;
	
	for(a=0; a<10; a++) {
		uiDefButI(block, TOGR|BIT|a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
		if(a==4) x1+= 5;
	}
	x1-= 5;

	for(a=0; a<10; a++) {
		uiDefButI(block, TOGR|BIT|(a+10), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
		if(a==4) x1+= 5;
	}
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
	while( BMF_GetStringWidth(G.fonts, str+temp)>(x2 - x1-24)) temp++;
	glRasterPos2i(x1+16, y2-30);
	BMF_DrawString(G.fonts, str+temp);
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
	
	/* midden */
	if(mval[0]<sizex/2) mval[0]=sizex/2;
	if(mval[1]<sizey/2) mval[1]=sizey/2;
	if(mval[0]>G.curscreen->sizex -sizex/2) mval[0]= G.curscreen->sizex -sizex/2;
	if(mval[1]>G.curscreen->sizey -sizey/2) mval[1]= G.curscreen->sizey -sizey/2;

	mywinset(G.curscreen->mainwin);
	
	x1= mval[0]-sizex/2; 
	y2= mval[1]+sizey/2;
	
	block= uiNewBlock(&listb, "numbuts", UI_EMBOSSX, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_ENTER_OK);
	
	/* LET OP: TEX BUTTON UITZONDERING */
	/* WAARSCHUWING: ALLEEN EEN ENKELE BITJES-BUTTON MOGELIJK: ER WORDT OP KOPIEDATA GEWERKT! */

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
	
	
	/* LET OP: TEX BUTTON UITZONDERING */
	
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
	Object *ob;
	float lim;
	char str[128];
	
	if(curarea->spacetype==SPACE_VIEW3D) {
		lim= 1000.0*MAX2(1.0, G.vd->grid);

		if(G.obpose){
			if (G.obpose->type == OB_ARMATURE) clever_numbuts_posearmature();
		}
		else if(G.obedit==0) {
			ob= OBACT;
			if(ob==0) return;

			add_numbut(0, NUM|FLO, "LocX:", -lim, lim, ob->loc, 0);
			add_numbut(1, NUM|FLO, "LocY:", -lim, lim, ob->loc+1, 0);
			add_numbut(2, NUM|FLO, "LocZ:", -lim, lim, ob->loc+2, 0);
			
			add_numbut(3, NUM|FLO, "RotX:", -10.0*lim, 10.0*lim, ob->rot, 0);
			add_numbut(4, NUM|FLO, "RotY:", -10.0*lim, 10.0*lim, ob->rot+1, 0);
			add_numbut(5, NUM|FLO, "RotZ:", -10.0*lim, 10.0*lim, ob->rot+2, 0);
			
			add_numbut(6, NUM|FLO, "SizeX:", -lim, lim, ob->size, 0);
			add_numbut(7, NUM|FLO, "SizeY:", -lim, lim, ob->size+1, 0);
			add_numbut(8, NUM|FLO, "SizeZ:", -lim, lim, ob->size+2, 0);
			
			sprintf(str, "Active Object: %s", ob->id.name+2);
			do_clever_numbuts(str, 9, REDRAW);

		}
		else if(G.obedit->type==OB_MESH) clever_numbuts_mesh();
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) clever_numbuts_curve();
		else if (G.obedit->type==OB_ARMATURE) clever_numbuts_armature();
	}
	else if(curarea->spacetype==SPACE_NLA){
		clever_numbuts_nla();
	}
	else if(curarea->spacetype==SPACE_IPO) {
		clever_numbuts_ipo();
	}
	else if(curarea->spacetype==SPACE_SEQ) {
		clever_numbuts_seq();
	}
	else if(curarea->spacetype==SPACE_IMAGE) {
		clever_numbuts_sima();
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
//	else if(curarea->spacetype==SPACE_ACTION){
//		stupid_damn_numbuts_action();
//	}
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
