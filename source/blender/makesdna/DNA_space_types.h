/**
 * blenlib/DNA_space_types.h (mar-2001 nzc)
 *	
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
#ifndef DNA_SPACE_TYPES_H
#define DNA_SPACE_TYPES_H

#include "DNA_listBase.h"
#include "DNA_vec_types.h"
/* Hum ... Not really nice... but needed for spacebuts. */
#include "DNA_view2d_types.h"

struct Ipo;
struct ID;
struct Text;
struct ImBuf;
struct Image;
struct SpaceIpo;
struct BlendHandle;

	/**
	 * The base structure all the other spaces
	 * are derived (implicitly) from. Would be
	 * good to make this explicit.
	 */
typedef struct SpaceLink SpaceLink;
struct SpaceLink {
	SpaceLink *next, *prev;
	int spacetype, pad;
	struct ScrArea *area;
};

typedef struct SpaceInfo {
	SpaceLink *next, *prev;
	int spacetype, pad1;
	struct ScrArea *area;
} SpaceInfo;

typedef struct SpaceIpo {
	SpaceLink *next, *prev;
	int spacetype, pad1;
	struct ScrArea *area;

	unsigned int rowbut, pad2; 
	View2D v2d;
	
	void *editipo;
	ListBase ipokey;
	struct Ipo *ipo;
	struct ID *from;

	short totipo, pin;
	short butofs, channel;
	short showkey, blocktype;
	short menunr, lock;
	int flag;
	int	reserved1;
	rctf tot;
} SpaceIpo;

typedef struct SpaceButs {
	SpaceLink *next, *prev;
	int spacetype, pad1;
	struct ScrArea *area;

	short cursens, curact;
	int pad2;
	View2D v2d;
	
	short mainb, menunr;	/* texnr en menunr moeten shorts blijven */
	short pin, mainbo;	
	void *lockpoin;
	
	short texnr;
	char texfrom, showgroup;

	short rectx, recty;		/* preview render */
	unsigned int *rect;
	short cury, modeltype;

	short scriptblock;
	short scaflag;
	
	char texact, pad3[7];
	
		/* a hackish link the anim buts keep
		 * to a SpaceIpo.
		 */
	struct SpaceIpo *anim_linked_sipo;
} SpaceButs;

typedef struct SpaceSeq {
	SpaceLink *next, *prev;
	int spacetype, pad;
	struct ScrArea *area;

	View2D v2d;
	
	short mainb, zoom;
	int pad2;
	
} SpaceSeq;

typedef struct SpaceFile {
	SpaceLink *next, *prev;
	int spacetype, pad;
	struct ScrArea *area;
	
	struct direntry *filelist;
	int totfile;
	char title[24];
	char dir[160];
	char file[80];
	short type, ofs, flag, sort;
	short maxnamelen, collums;
	
	struct BlendHandle *libfiledata;
	
	short retval, ipotype;
	short menu, act;

	/* changed type for compiling */
	/* void (*returnfunc)(short); ? used with char* ....*/
	/**
	 * @attention Called in filesel.c: 
	 * @attention returnfunc(this->retval) : short
	 * @attention returnfunc(name)         : char*
	 * @attention Other uses are limited to testing against
	 * @attention the value. How do we resolve this? Two args?
	 * @attention For now, keep the char*, as it seems stable.
	 * @attention Be warned that strange behaviour _has_ been spotted!
	 */
	void (*returnfunc)(char*);
 		
	short *menup;
} SpaceFile;

typedef struct SpaceOops {
	SpaceLink *next, *prev;
	int spacetype, pad;
	struct ScrArea *area;

	View2D v2d;
	
	ListBase oops;
	short pin, visiflag, flag, rt;
	void *lockpoin;
	
} SpaceOops;

typedef struct SpaceImage {
	SpaceLink *next, *prev;
	int spacetype, pad;
	struct ScrArea *area;

	View2D v2d;
	
	struct Image *image;
	float zoom;
	float pad2; /* MAART: is this needed? Ton: yes, padding with 8 bytes aligned  */
	short mode, pin;
	short imanr, curtile;
	short xof, yof;
	short flag, lock;
	
} SpaceImage;

typedef struct SpaceNla{
	struct SpaceLink *next, *prev;
	int spacetype;
	int lock;
	struct ScrArea *area;

	View2D v2d;	
} SpaceNla;

typedef struct SpaceText {
	SpaceLink *next, *prev;
	int spacetype, pad;
	struct ScrArea *area;

	struct Text *text;	

	int top, viewlines;
	short flags, menunr;
	
	int font_id;	
	int lheight;
	int left;
	int showlinenrs;
	
	float pix_per_line;

	struct rcti txtscroll, txtbar;
	
	void *py_draw;
	void *py_event;
	void *py_button;
	void *py_globaldict;
} SpaceText;

#
#
typedef struct OneSelectableIma {
	int   header;						
	int   ibuf_type;
	struct ImBuf *pict;					
	struct OneSelectableIma *next;		
	struct OneSelectableIma *prev;		
	
	short  cmap, image, draw_me, rt;
	short  sx, sy, ex, ey, dw, dh;				
	short  selectable, selected;		
	int   mtime, disksize;				
	char   file_name[64];
	
	short  orgx, orgy, orgd, anim;		/* same as ibuf->x...*/
	char   dummy[4];					/* 128 */

	char   pict_rect[3968];				/* 4096   (RECT = 64 * 62) */
	
} OneSelectableIma;

#
#
typedef struct ImaDir {
	struct ImaDir *next, *prev;
	int  selected, hilite; 
	int  type,  size;
	int mtime;
	char name[100];
} ImaDir;

typedef struct SpaceImaSel {
	SpaceLink *next, *prev;
	int spacetype, pad1;
	struct ScrArea *area;
	
	char   title[28];
	
	int   fase; 
	short  mode, subfase;
	short  mouse_move_redraw, imafase;
	short  mx, my;
	
	short  dirsli, dirsli_lines;
	short  dirsli_sx, dirsli_ey , dirsli_ex, dirsli_h;
	short  imasli, fileselmenuitem;
	short  imasli_sx, imasli_ey , imasli_ex, imasli_h;
	
	short  dssx, dssy, dsex, dsey; 
	short  desx, desy, deex, deey; 
	short  fssx, fssy, fsex, fsey; 
	short  dsdh, fsdh; 
	short  fesx, fesy, feex, feey; 
	short  infsx, infsy, infex, infey; 
	short  dnsx, dnsy, dnw, dnh;
	short  fnsx, fnsy, fnw, fnh;

	
	char   fole[128], dor[128];
	char   file[128], dir[128];
	ImaDir *firstdir, *firstfile;
	int    topdir,  totaldirs,  hilite; 
	int    topfile, totalfiles;
	
	float  image_slider;
	float  slider_height;
	float  slider_space;
	short  topima,  totalima;
	short  curimax, curimay;
	OneSelectableIma *first_sel_ima;
	OneSelectableIma *hilite_ima;
	short  total_selected, ima_redraw;
	int pad2;
	
	struct ImBuf  *cmap;

	/* Also fucked. Needs to change so things compile, but breaks sdna
	* ... */	
/*  	void (*returnfunc)(void); */
	void (*returnfunc)(char*);
	void *arg1;
} SpaceImaSel;


/* **************** SPACE ********************* */


/* view3d->flag */ /* Now in DNA_view3d_types.h */
/*
#define V3D_DISPIMAGE		1
#define V3D_DISPBGPIC		2
#define V3D_SETUPBUTS		4
#define V3D_NEEDBACKBUFDRAW	8
#define V3D_MODE			(16+32+64+128)
#define V3D_EDITMODE		16
#define V3D_VERTEXPAINT		32
#define V3D_FACESELECT		64
#define V3D_POSEMODE		128
*/

/* view3d->around */ /* Now in DNA_view3d_types.h */
/*
#define V3D_CENTRE		0
#define V3D_CENTROID	3
#define V3D_CURSOR		1
#define V3D_LOCAL		2
*/

/* buts->mainb */
#define BUTS_VIEW			0
#define BUTS_LAMP			1
#define BUTS_MAT			2
#define BUTS_TEX			3
#define BUTS_ANIM			4
#define BUTS_WORLD			5
#define BUTS_RENDER			6
#define BUTS_EDIT			7
#define BUTS_GAME			8
#define BUTS_FPAINT			9
#define BUTS_RADIO			10
#define BUTS_SCRIPT			11
#define BUTS_SOUND			12
#define BUTS_CONSTRAINT		13

/* buts->scaflag */		
#define BUTS_SENS_SEL		1
#define BUTS_SENS_ACT		2
#define BUTS_SENS_LINK		4
#define BUTS_CONT_SEL		8
#define BUTS_CONT_ACT		16
#define BUTS_CONT_LINK		32
#define BUTS_ACT_SEL		64
#define BUTS_ACT_ACT		128
#define BUTS_ACT_LINK		256

/* deze getallen ook invullen in blender.h SpaceFile: struct dna herkent geen defines */
#define FILE_MAXDIR			160
#define FILE_MAXFILE		80

/* filesel types */
#define FILE_UNIX			8
#define FILE_BLENDER		8
#define FILE_SPECIAL		9

#define FILE_LOADLIB		1
#define FILE_MAIN			2

/* sfile->flag */
#define FILE_SHOWSHORT		1
#define FILE_STRINGCODE		2
#define FILE_LINK			4
#define FILE_HIDE_DOT		8

/* sfile->sort */
#define FILE_SORTALPHA		0
#define FILE_SORTDATE		1
#define FILE_SORTSIZE		2
#define FILE_SORTEXTENS		3

/* files in filesel list: 2=ACTIVE  */
#define HILITE				1
#define BLENDERFILE			4
#define PSXFILE				8
#define IMAGEFILE			16
#define MOVIEFILE			32

#define SCROLLH	16			/* hoogte scrollbar */
#define SCROLLB	16			/* breedte scrollbar */

/* SpaceImage->mode */
#define SI_TEXTURE		0
#define SI_SHOW			1

/* SpaceImage->flag */
#define SI_BE_SQUARE	1
#define SI_EDITTILE		2
#define SI_CLIP_UV		4
#define SI_DRAWTOOL		8

/* SpaceText flags (moved from DNA_text_types.h) */

#define ST_SCROLL_SELECT        0x0001 // scrollable
#define ST_CLEAR_NAMESPACE      0x0010 // clear namespace after script
                                       // execution (see BPY_main.c)

/* SpaceOops->flag */
#define SO_TESTBLOCKS	1
#define SO_NEWSELECTED	2

/* SpaceOops->visiflag */
#define OOPS_SCE	1
#define OOPS_OB		2
#define OOPS_ME		4
#define OOPS_CU		8
#define OOPS_MB		16
#define OOPS_LT		32
#define OOPS_LA		64
#define OOPS_MA		128
#define OOPS_TE		256
#define OOPS_IP		512
#define OOPS_LAY	1024
#define OOPS_LI		2048
#define OOPS_IM		4096

/* headerbuttons: 450-499 */

#define B_IMASELHOME		451
#define B_IMASELREMOVEBIP	452

#define C_BACK  0xBAAAAA
#define C_DARK  0x665656
#define C_DERK  0x766666
#define C_HI	0xCBBBBB
#define C_LO	0x544444

/* queue settings */
#define IMS_KNOW_WIN        1
#define IMS_KNOW_BIP        2
#define IMS_KNOW_DIR        4
#define IMS_DOTHE_INF		8
#define IMS_KNOW_INF	   16
#define IMS_DOTHE_IMA	   32
#define IMS_KNOW_IMA	   64
#define IMS_FOUND_BIP	  128
#define IMS_DOTHE_BIP	  256
#define IMS_WRITE_NO_BIP  512

/* imasel->mode */
#define IMS_NOIMA			0
#define IMS_IMA				1
#define IMS_ANIM			2
#define IMS_DIR				4
#define IMS_FILE			8
#define IMS_STRINGCODE		16

#define IMS_INDIR			1
#define IMS_INDIRSLI		2
#define IMS_INFILE			3
#define IMS_INFILESLI		4

#endif

