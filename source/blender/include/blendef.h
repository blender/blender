/* $Id$
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
#ifndef BLENDEF_H
#define BLENDEF_H


/* **************** MAX ********************* */

#define MAXFRAME	300000
#define MAXFRAMEF	300000.0f

#define MINFRAME	1
#define MINFRAMEF	1.0

/* max length material array, 16 because of bits in matfrom */
#define MAXPICKBUF      10000
#define MAXSEQ          32

/* in buttons.c */
#define MAX_EFFECT      20

#ifndef MAXFLOAT
#define MAXFLOAT  ((float)3.40282347e+38)
#endif

/* also fill in structs itself, dna cannot handle defines, duplicate with utildefines.h still */
#ifndef FILE_MAXDIR
#define FILE_MAXDIR		160
#define FILE_MAXFILE		80
#endif


#include <float.h>	




/* **************** GENERAL ********************* */

// return values

#define RET_OK 0
#define RET_ERROR 1
#define RET_CANCEL 2
#define RET_YES (1 == 1)
#define RET_NO (1 == 0)

#if defined(__sgi) || defined(__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__hppa__) || defined (__BIG_ENDIAN__)
/* big endian */
#define MAKE_ID2(c, d)		( (c)<<8 | (d) )
#define MOST_SIG_BYTE				0
#define BBIG_ENDIAN
#else
/* little endian  */
#define MAKE_ID2(c, d)		( (d)<<8 | (c) )
#define MOST_SIG_BYTE				1
#define BLITTLE_ENDIAN
#endif

#define SELECT			1
#define HIDDEN			1
#define FIRST			1
#define ACTIVE			2
/*#ifdef WITH_VERSE*/
#define VERSE			3
/*#endif*/
#define DESELECT		0
#define NOT_YET			0
#define VISIBLE			0
#define LAST			0

#define TESTBASE(base)	( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && (((base)->object->restrictflag & OB_RESTRICT_VIEW)==0) )
#define TESTBASELIB(base)	( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && ((base)->object->id.lib==0) && (((base)->object->restrictflag & OB_RESTRICT_VIEW)==0))

/* This is a TESTBASELIB that can work without a 3D view */
#define TESTBASELIB_BGMODE(base)	( ((base)->flag & SELECT) && ((base)->lay & (G.vd ? G.vd->lay : G.scene->lay)) && ((base)->object->id.lib==0) && (((base)->object->restrictflag & OB_RESTRICT_VIEW)==0))

#define BASE_SELECTABLE(base)	 ((base->lay & G.vd->lay) && (base->object->restrictflag & (OB_RESTRICT_SELECT|OB_RESTRICT_VIEW))==0)
#define FIRSTBASE		G.scene->base.first
#define LASTBASE		G.scene->base.last
#define BASACT			(G.scene->basact)
#define OBACT			(BASACT? BASACT->object: 0)
#define OB_SUPPORT_MATERIAL(ob) ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)
#define ID_NEW(a)		if( (a) && (a)->id.newid ) (a)= (void *)(a)->id.newid
#define ID_NEW_US(a)	if( (a)->id.newid) {(a)= (void *)(a)->id.newid; (a)->id.us++;}
#define ID_NEW_US2(a)	if( ((ID *)a)->newid) {(a)= ((ID *)a)->newid; ((ID *)a)->us++;}
#define	CFRA			(G.scene->r.cfra)
#define	F_CFRA			((float)(G.scene->r.cfra))
#define	SFRA			(G.scene->r.sfra)
#define	EFRA			(G.scene->r.efra)
#define PSFRA			((G.scene->r.psfra != 0)? (G.scene->r.psfra): (G.scene->r.sfra))
#define PEFRA			((G.scene->r.psfra != 0)? (G.scene->r.pefra): (G.scene->r.efra))
#define FRA2TIME(a)           ((((double) G.scene->r.frs_sec_base) * (a)) / G.scene->r.frs_sec)
#define TIME2FRA(a)           ((((double) G.scene->r.frs_sec) * (a)) / G.scene->r.frs_sec_base)
#define FPS                     (((double) G.scene->r.frs_sec) / G.scene->r.frs_sec_base)

#define ISPOIN(a, b, c)			( (a->b) && (a->c) )
#define ISPOIN3(a, b, c, d)		( (a->b) && (a->c) && (a->d) )
#define ISPOIN4(a, b, c, d, e)	( (a->b) && (a->c) && (a->d) && (a->e) )

#define BEZSELECTED(bezt)   (((bezt)->f1 & SELECT) || ((bezt)->f2 & SELECT) || ((bezt)->f3 & SELECT))
/* for curve objects in editmode that can have hidden handles - may use for IPO's later */
#define BEZSELECTED_HIDDENHANDLES(bezt)   ((G.f & G_HIDDENHANDLES) ? (bezt)->f2 & SELECT : BEZSELECTED(bezt))

/* psfont */
#define FNT_PDRAW 1
#define FNT_HAEBERLI 2

/* getbutton */

/* do_global_buttons(event) */

// (first event)
#define B_LOCAL_ALONE	20


#define B_ACTLOCAL		24	/* __NLA */
#define	B_ACTALONE		25	/* __NLA */
#define B_ARMLOCAL		26	/* __NLA */
#define	B_ARMALONE		27	/* __NLA */

#define B_WORLDLOCAL		28
#define B_WORLDALONE		29
#define B_LATTLOCAL		30
#define B_MBALLLOCAL		31
#define B_CAMERALOCAL		32
#define B_OBLOCAL		33
#define B_IPOLOCAL		34
#define B_LAMPLOCAL		35
#define B_MATLOCAL		36
#define B_TEXLOCAL		37
#define B_MESHLOCAL		38
#define B_CURVELOCAL		39

#define B_LATTALONE		40
#define B_MBALLALONE		41
#define B_CAMERAALONE		42
#define B_OBALONE		43
#define B_IPOALONE		44
#define B_LAMPALONE		45
#define B_MATALONE		46
#define B_TEXALONE		47
#define B_MESHALONE		48
#define B_CURVEALONE		49

/* EVENT < 50: alones en locals */

#define B_KEEPDATA		60
#define B_CONSOLETOG		61
#define B_DRAWINFO		62
#define B_REDRCURW3D		63
#define B_FLIPINFOMENU		64
#define B_FLIPFULLSCREEN	65
#define B_PLAINMENUS		66


#define B_GLRESLIMITCHANGED	69
#define B_SHOWSPLASH		70
#define B_RESETAUTOSAVE		71
#define B_SOUNDTOGGLE		72
#define B_MIPMAPCHANGED		73
#define B_CONSTRAINTBROWSE	74	/* __NLA */
#define B_ACTIONDELETE		75	/* __NLA */
#define B_ACTIONBROWSE		76	/* __NLA */
#define B_IMAGEDELETE		77
#define B_LTEXBROWSE		78
#define B_MESHBROWSE		79
#define B_EXTEXBROWSE		80
#define B_LOADTEMP		81
#define B_MATDELETE		82
#define B_TEXDELETE		83
#define B_IPODELETE		84
#define B_WORLDDELETE		85
#define B_WTEXBROWSE		86
#define B_WORLDBROWSE		87
#define B_IPOBROWSE		88
#define B_NEWFRAME		89
#define B_LAMPBROWSE		90
#define B_MATBROWSE		91
#define	B_TEXBROWSE		92
#define	B_EDITBROWSE		93
#define B_AUTOTEXNAME		94
#define B_AUTOMATNAME		95
#define B_MATLOCK		96
#define B_IDNAME		97
#define B_NEWSPACE		98
#define B_FULL			99
#define B_REDR			100


/* VIEW3D: 100 */
#define B_HOME			101
#define B_LAY			102
/* watch: codes 102-132 in in use for layers */
#define B_AUTOKEY		139
#define B_SCENELOCK		140
#define B_LOCALVIEW		141
#define B_U_CAPSLOCK		142

#define B_VIEWBUT		146
#define B_PERSP			147
#define B_PROPTOOL		148
#define B_VIEWRENDER		149
#define B_STARTGAME		150

#define B_MODESELECT		156
#define B_AROUND		157
#define B_SEL_VERT		158
#define B_SEL_EDGE		159
#define B_SEL_FACE		160
#define B_MAN_TRANS		161
#define B_MAN_ROT		162
#define B_MAN_SCALE		163
#define B_SEL_PATH		166
#define B_SEL_POINT		167
#define B_SEL_END		168
#define B_MAN_MODE		169
#define B_NDOF			170

/* IPO: 200 */
#define B_IPOHOME		201
#define B_IPOBORDER		202
#define B_IPOCOPY		203
#define B_IPOPASTE		204
#define B_IPOCONT		205
#define B_IPOEXTRAP		206
#define B_IPOCYCLIC		207
#define B_IPOMAIN		208
#define B_IPOSHOWKEY		209
#define B_IPOCYCLICX		210
	/* warn: also used for oops and seq */
#define B_VIEW2DZOOM		211
#define B_IPOPIN		212
#define B_IPO_ACTION_OB		213
#define B_IPO_ACTION_KEY	214
#define B_IPOVIEWCENTER		215
#define B_IPOVIEWALL		216
#define B_IPOREDRAW			217

/* OOPS: 250 */
#define B_OOPSHOME		251
#define B_OOPSBORDER		252
#define B_NEWOOPS		253
#define B_OOPSVIEWSEL		254

/* INFO: 300 */
/* watch: also in filesel.c and editobject.c */
#define B_INFOSCR		301
#define B_INFODELSCR		302
#define B_INFOSCE		304
#define B_INFODELSCE		305
#define B_FILEMENU		306
#define B_PACKFILE		307

#define B_CONSOLEOUT		308
#define B_CONSOLENUMLINES	309
#define B_USERPREF		310
#define B_LOADUIFONT		311
#define B_SETLANGUAGE		312
#define B_SETFONTSIZE		313
#define B_SETENCODING		314
#define B_SETTRANSBUTS		315
#define B_DOLANGUIFONT		316
#define B_RESTOREFONT		317
#define B_USETEXTUREFONT	318

#define B_UITHEMECHANGED	320
#define B_UITHEMECOLORMOD	321
#define B_UITHEMERESET		322
#define B_UITHEMEIMPORT		323
#define B_UITHEMEEXPORT		324

#define B_MEMCACHELIMIT		325
#define B_WPAINT_RANGE		326

/* Definitions for the fileselect buttons in user prefs */
#define B_FONTDIRFILESEL  	330
#define B_TEXTUDIRFILESEL  	331
#define B_PLUGTEXDIRFILESEL	332
#define B_PLUGSEQDIRFILESEL	333
#define B_RENDERDIRFILESEL 	334
#define B_PYTHONDIRFILESEL 	335
#define B_SOUNDDIRFILESEL  	336
#define B_TEMPDIRFILESEL  	337
/* yafray: for exportdir select */
#define B_YAFRAYDIRFILESEL	338
#define B_PYMENUEVAL		339 /* re-eval scripts registration in menus */
/* END Definitions for the fileselect buttons in user prefs */

/* IMAGE: 350 */
#define B_SIMAGEHOME		351
#define B_SIMABROWSE		352
#define B_SIMAGELOAD		353
#define B_SIMA_REDR_IMA_3D	354
#define B_SIMAGETILE		355
#define B_BE_SQUARE			356
#define B_TWINANIM			357
#define B_SIMAGEREPLACE		358
#define B_CLIP_UV			359
#define B_SIMAGELOAD1		360
#define B_SIMAGEREPLACE1	361
#define B_SIMAGEPAINTTOOL	362
#define B_SIMAPACKIMA		363
#define B_SIMAGESAVE		364
#define B_SIMACLONEBROWSE	365
#define B_SIMACLONEDELETE	366
#define B_SIMANOTHING		368
#define B_SIMACURVES		369
#define B_SIMARANGE			370
#define B_SIMA_USE_ALPHA	371
#define B_SIMA_SHOW_ALPHA	372
#define B_SIMA_SHOW_ZBUF	373
#define B_SIMABRUSHBROWSE	374
#define B_SIMABRUSHDELETE	375
#define B_SIMABRUSHLOCAL	376
#define B_SIMABRUSHCHANGE	377
#define B_SIMABTEXBROWSE	378
#define B_SIMABTEXDELETE	379
#define B_SIMARELOAD		380
#define B_SIMANAME			381
#define B_SIMAMULTI			382
#define B_TRANS_IMAGE		383
#define B_CURSOR_IMAGE		384
#define B_SIMA_REPACK		385
#define B_SIMA_PLAY			386
#define B_SIMA_RECORD		387
#define B_SIMAPIN			388
#define B_SIMA3DVIEWDRAW	389


/* BUTS: 400 */
#define B_BUTSHOME			401
#define B_BUTSPREVIEW		402
#define B_MATCOPY			403
#define B_MATPASTE			404
#define B_MESHTYPE			405
#define B_CONTEXT_SWITCH	406

/* IMASEL: 450 */
/* in imasel.h - not any more - elubie */
#define B_SORTIMASELLIST	451
#define B_RELOADIMASELDIR	452
#define B_FILTERIMASELDIR	453

/* TEXT: 500 */
#define B_TEXTBROWSE		501
#define B_TEXTALONE		502
#define B_TEXTLOCAL		503
#define B_TEXTDELETE		504
#define B_TEXTFONT		505
#define B_TEXTSTORE		506
#define B_TEXTLINENUM		507
#define B_TAB_NUMBERS		508
#define B_SYNTAX		509

/* SCRIPT: 525 */
#define B_SCRIPTBROWSE		526
#define B_SCRIPT2PREV		527

/* FILE: 550 */
#define B_SORTFILELIST		551
#define B_RELOADDIR		552

/* SEQUENCE: 600 */
#define B_SEQHOME		601
#define B_SEQCLEAR		602

/* SOUND: 650 */
#define B_SOUNDBROWSE		651
#define B_SOUNDBROWSE2  	652
#define B_SOUNDHOME		653
#define B_PACKSOUND		654

/* ACTION: 701 - 750 */
#define B_ACTHOME		701
#define	B_ACTCOPY		702
#define B_ACTPASTE		703
#define B_ACTPASTEFLIP		704
#define B_ACTCYCLIC		705
#define B_ACTCONT		706
#define B_ACTMAIN		707
#define	B_ACTPIN		708
#define B_ACTBAKE		709
#define B_ACTCOPYKEYS		710
#define B_ACTPASTEKEYS		711

#define B_ACTCUSTCOLORS		712
#define B_ACTCOLSSELECTOR	713
#define B_ACTGRP_SELALL		714
#define B_ACTGRP_ADDTOSELF	715
#define B_ACTGRP_UNGROUP	716

/* TIME: 751 - 800 */
#define B_TL_REW		751
#define B_TL_PLAY		752
#define B_TL_FF			753
#define B_TL_PREVKEY		754
#define B_TL_NEXTKEY		755
#define B_TL_STOP		756
#define B_TL_PREVIEWON		757

/* NLA: 801-850 */
#define B_NLAHOME		801

/* NODE: 851-900 */
#define B_NODEHOME		851
#define B_NODE_USEMAT		852
#define B_NODE_USESCENE		853

/* FREE 901 - 999 */


#define B_NOP			-1


/* editbutflag */
#define B_CLOCKWISE		1
#define B_KEEPORIG		2
#define B_BEAUTY		4
#define B_SMOOTH		8
#define B_BEAUTY_SHORT  	16
#define B_AUTOFGON		32
#define B_KNIFE			0x80
#define B_PERCENTSUBD		0x40
#define B_MESH_X_MIRROR		0x100
#define B_JOINTRIA_UV		0x200
#define B_JOINTRIA_VCOL		0X400
#define B_JOINTRIA_SHARP	0X800
#define B_JOINTRIA_MAT		0X1000

/* DISPLAYMODE */
#define R_DISPLAYIMAGE	0
#define R_DISPLAYWIN	1
#define R_DISPLAYSCREEN	2

/* Gvp.flag and Gwp.flag */
#define VP_COLINDEX	1
#define VP_AREA		2
#define VP_SOFT		4
#define VP_NORMALS	8
#define VP_SPRAY	16
#define VP_MIRROR_X	32
#define VP_HARD		64
#define VP_ONLYVGROUP	128

/* Error messages */
#define ERROR_LIBDATA_MESSAGE "Can't edit external libdata"

#define MAX_RENDER_PASS	100

#endif
