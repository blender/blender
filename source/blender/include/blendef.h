/* $Id$ 
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
#ifndef BLENDEF_H
#define BLENDEF_H

#ifdef WIN32
#else
#ifndef __BeOS
#define O_BINARY	0
#endif
#endif

#ifndef MAXFLOAT
#define MAXFLOAT  ((float)3.40282347e+38)
#endif

#include <float.h>	/* deze moet een keer naar de blender.h */




/* **************** ALGEMEEN ********************* */

#define VECCOPY(v1,v2) 		{*(v1)= *(v2); *(v1+1)= *(v2+1); *(v1+2)= *(v2+2);}
#define QUATCOPY(v1,v2) 	{*(v1)= *(v2); *(v1+1)= *(v2+1); *(v1+2)= *(v2+2); *(v1+3)= *(v2+3);}

#define INPR(v1, v2)		( (v1)[0]*(v2)[0] + (v1)[1]*(v2)[1] + (v1)[2]*(v2)[2] )
#define CLAMP(a, b, c)		if((a)<(b)) (a)=(b); else if((a)>(c)) (a)=(c)
#define CLAMPIS(a, b, c)	((a)<(b) ? (b) : (a)>(c) ? (c) : (a))
#define CLAMPTEST(a, b, c)	if((b)<(c)) {CLAMP(a, b, c);} else {CLAMP(a, c, b);}

#define IS_EQ(a,b) ((fabs((double)(a)-(b)) >= (double) FLT_EPSILON) ? 0 : 1)

#define INIT_MINMAX(min, max) (min)[0]= (min)[1]= (min)[2]= 1.0e30; (max)[0]= (max)[1]= (max)[2]= -1.0e30;
#define DO_MINMAX(vec, min, max) if( (min)[0]>(vec)[0] ) (min)[0]= (vec)[0]; \
							  if( (min)[1]>(vec)[1] ) (min)[1]= (vec)[1]; \
							  if( (min)[2]>(vec)[2] ) (min)[2]= (vec)[2]; \
							  if( (max)[0]<(vec)[0] ) (max)[0]= (vec)[0]; \
							  if( (max)[1]<(vec)[1] ) (max)[1]= (vec)[1]; \
							  if( (max)[2]<(vec)[2] ) (max)[2]= (vec)[2]; \

#define DO_MINMAX2(vec, min, max) if( (min)[0]>(vec)[0] ) (min)[0]= (vec)[0]; \
							  if( (min)[1]>(vec)[1] ) (min)[1]= (vec)[1]; \
							  if( (max)[0]<(vec)[0] ) (max)[0]= (vec)[0]; \
							  if( (max)[1]<(vec)[1] ) (max)[1]= (vec)[1];

#define MINSIZE(val, size)	( ((val)>=0.0) ? (((val)<(size)) ? (size): (val)) : ( ((val)>(-size)) ? (-size) : (val)))

#define BTST(a,b)	( ( (a) & 1<<(b) )!=0 )
#define BCLR(a,b)	( (a) & ~(1<<(b)) )
#define BSET(a,b)	( (a) | 1<<(b) )
/* bit-row */
#define BROW(min, max)	(((max)>=31? 0xFFFFFFFF: (1<<(max+1))-1) - ((min)? ((1<<(min))-1):0) )

// return values

#define RET_OK 0
#define RET_ERROR 1
#define RET_CANCEL 2
#define RET_YES (1 == 1)
#define RET_NO (1 == 0)

#define LONGCOPY(a, b, c)	{int lcpc=c, *lcpa=(int *)a, *lcpb=(int *)b; while(lcpc-->0) *(lcpa++)= *(lcpb++);}

#if defined(__sgi) || defined(__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__BIG_ENDIAN__)
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
#define ACTIVE			2
#define NOT_YET			0


#define TESTBASE(base)	( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) )
#define TESTBASELIB(base)	( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && ((base)->object->id.lib==0))
#define FIRSTBASE		G.scene->base.first
#define LASTBASE		G.scene->base.last
#define BASACT			(G.scene->basact)
#define OBACT			(BASACT? BASACT->object: 0)
#define ID_NEW(a)		if( (a) && (a)->id.newid ) (a)= (void *)(a)->id.newid
#define ID_NEW_US(a)	if( (a)->id.newid) {(a)= (void *)(a)->id.newid; (a)->id.us++;}
#define ID_NEW_US2(a)	if( ((ID *)a)->newid) {(a)= ((ID *)a)->newid; ((ID *)a)->us++;}
#define	CFRA			(G.scene->r.cfra)
#define	F_CFRA			((float)(G.scene->r.cfra))
#define	SFRA			(G.scene->r.sfra)
#define	EFRA			(G.scene->r.efra)

#define ISPOIN(a, b, c)			( (a->b) && (a->c) )
#define ISPOIN3(a, b, c, d)		( (a->b) && (a->c) && (a->d) )
#define ISPOIN4(a, b, c, d, e)	( (a->b) && (a->c) && (a->d) && (a->e) )


#define KNOTSU(nu)	    ( (nu)->orderu+ (nu)->pntsu+ (nu->orderu-1)*((nu)->flagu & 1) )
#define KNOTSV(nu)	    ( (nu)->orderv+ (nu)->pntsv+ (nu->orderv-1)*((nu)->flagv & 1) )

/* psfont */
#define FNT_PDRAW 1
#define FNT_HAEBERLI 2


/* isect en scanfill */
#define COMPLIMIT	0.0003


/* **************** MAX ********************* */


#define MAXLAMP		256
/* max lengte material array, 16 vanwege bitjes in matfrom */
#define MAXPICKBUF	2000
#define MAXSEQ		32
/*  in Image struct */
#define MAXMIPMAP	10
/* in buttons.c */
#define MAX_EFFECT	20

/* getbutton */

/* do_global_buttons(event) */

#define B_ACTLOCAL		24	/* __NLA */
#define	B_ACTALONE		25	/* __NLA */
#define B_ARMLOCAL		26	/* __NLA */
#define	B_ARMALONE		27	/* __NLA */

#define B_WORLDLOCAL	28
#define B_WORLDALONE	29
#define B_LATTLOCAL		30
#define B_MBALLLOCAL	31
#define B_CAMERALOCAL	32
#define B_OBLOCAL		33
#define B_IPOLOCAL		34
#define B_LAMPLOCAL		35
#define B_MATLOCAL		36
#define B_TEXLOCAL		37
#define B_MESHLOCAL		38
#define B_CURVELOCAL	39

#define B_LATTALONE		40
#define B_MBALLALONE	41
#define B_CAMERAALONE	42
#define B_OBALONE		43
#define B_IPOALONE		44
#define B_LAMPALONE		45
#define B_MATALONE		46
#define B_TEXALONE		47
#define B_MESHALONE		48
#define B_CURVEALONE	49
/* EVENT < 50: alone's en locals */

#define B_KEEPDATA			60

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
#define B_LOADTEMP			81
#define B_MATDELETE			82
#define B_TEXDELETE			83
#define B_IPODELETE			84
#define B_WORLDDELETE		85
#define B_WTEXBROWSE		86
#define B_WORLDBROWSE		87
#define B_IPOBROWSE			88
#define B_NEWFRAME			89
#define B_LAMPBROWSE		90
#define B_MATBROWSE			91
#define	B_TEXBROWSE			92
#define	B_EDITBROWSE		93
#define B_AUTOTEXNAME		94
#define B_AUTOMATNAME		95
#define B_MATLOCK			96
#define B_IDNAME			97
#define B_NEWSPACE			98
#define B_FULL				99
#define B_REDR				100


/* VIEW3D: 100 */
#define B_HOME			101
#define B_LAY			102
/* pasop: codes 102-132 in gebuik voor layers */
#define B_AUTOKEY		139
#define B_SCENELOCK		140
#define B_LOCALVIEW		141
#define B_U_CAPSLOCK	142
#define B_EDITMODE		143
#define B_VPAINT		144
#define B_FACESEL		145
#define B_VIEWBUT		146
#define B_PERSP			147
#define B_PROPTOOL		148
#define B_VIEWRENDER	149
#define B_VIEWTRANS		150
#define B_VIEWZOOM		151
#define B_STARTGAME		152
#define	B_POSEMODE		153
#define	B_TEXTUREPAINT	154
#define B_WPAINT		155

/* IPO: 200 */
#define B_IPOHOME		201
#define B_IPOBORDER		202
#define B_IPOCOPY		203
#define B_IPOPASTE		204
#define B_IPOCONT		205
#define B_IPOEXTRAP		206
#define B_IPOCYCLIC		207
#define B_IPOMAIN		208
#define B_IPOSHOWKEY	209
#define B_IPOCYCLICX	210
	/* warn: also used for oops and seq */
#define B_VIEW2DZOOM	211
#define B_IPOPIN		212

/* OOPS: 250 */
#define B_OOPSHOME		251
#define B_OOPSBORDER	252
#define B_NEWOOPS		253

/* INFO: 300 */
/* pas op: ook in filesel.c en editobject.c */
#define B_INFOSCR		301
#define B_INFODELSCR	302
#define B_INFOSCE		304
#define B_INFODELSCE	305
#define B_FILEMENU		306
#define B_PACKFILE		307

/* IMAGE: 350 */
#define B_SIMAGEHOME		351
#define B_SIMABROWSE		352
#define B_SIMAGELOAD		353
#define B_SIMAGEDRAW		354
#define B_BE_SQUARE			355
#define B_SIMAGEDRAW1		356
#define B_TWINANIM			357
#define B_SIMAGEREPLACE		358
#define B_CLIP_UV			359
#define B_SIMAGELOAD1		360
#define B_SIMAGEREPLACE1	361
#define B_SIMAGEPAINTTOOL	362
#define B_SIMAPACKIMA		363
#define B_SIMAGESAVE		364

/* BUTS: 400 */
#define B_BUTSHOME		401
#define B_BUTSPREVIEW	402
#define B_MATCOPY		403
#define B_MATPASTE		404
#define B_MESHTYPE		405

/* IMASEL: 450 */
/* in de imasel.h */

/* TEXT: 500 */
#define B_TEXTBROWSE	501
#define B_TEXTALONE		502
#define B_TEXTLOCAL		503
#define B_TEXTDELETE	504
#define B_TEXTFONT		505
#define B_TEXTSTORE		506
#define B_TEXTLINENUM	507

/* FILE: 550 */
#define B_SORTFILELIST	551
#define B_RELOADDIR		552

/* SEQUENCE: 600 */
#define B_SEQHOME		601
#define B_SEQCLEAR		602

/* SOUND: 650 */
#define B_SOUNDBROWSE	651
#define B_SOUNDBROWSE2  652
#define B_SOUNDHOME		653
#define B_PACKSOUND	654

/* ACTION: 701 - 800 */
#define B_ACTHOME		701
#define	B_ACTCOPY		702
#define B_ACTPASTE		703
#define B_ACTPASTEFLIP	704
#define B_ACTCYCLIC		705
#define B_ACTCONT		706
#define B_ACTMAIN		707
#define	B_ACTPIN		708
#define B_ACTBAKE		709

#define B_NOTHING		-1
#define B_NOP			-1

/* NLA: 801-900 */
#define B_NLAHOME		801

/* editbutflag */
#define B_CLOCKWISE		1
#define B_KEEPORIG		2
#define B_BEAUTY		4
#define B_SMOOTH		8


/* ***************** DISPLIST ***************** */

#define DL_POLY			0
#define DL_SEGM			1
#define DL_SURF			2
#define DL_TRIA			3
#define DL_INDEX3		4
#define DL_INDEX4		5
#define DL_VERTCOL		6
#define DL_VERTS		7
#define DL_NORS			8

#define DL_SURFINDEX(cyclu, cyclv, sizeu, sizev)	    \
							    \
    if( (cyclv)==0 && a==(sizev)-1) break;		    \
    if(cyclu) {						    \
	p1= sizeu*a;					    \
	p2= p1+ sizeu-1;				    \
	p3= p1+ sizeu;					    \
	p4= p2+ sizeu;					    \
	b= 0;						    \
    }							    \
    else {						    \
	p2= sizeu*a;					    \
	p1= p2+1;					    \
	p4= p2+ sizeu;					    \
	p3= p1+ sizeu;					    \
	b= 1;						    \
    }							    \
    if( (cyclv) && a==sizev-1) {			    \
	p3-= sizeu*sizev;				    \
	p4-= sizeu*sizev;				    \
    }

/* DISPLAYMODE */
#define R_DISPLAYVIEW	0
#define R_DISPLAYWIN	1
#define R_DISPLAYAUTO	2



#if defined(__sgi) || defined(__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__BIG_ENDIAN__)
#define RCOMP	3
#define GCOMP	2
#define BCOMP	1
#define ACOMP	0

#else

#define RCOMP	0
#define GCOMP	1
#define BCOMP	2
#define ACOMP	3
#endif

#ifdef GS
#undef GS
#endif
#define GS(a)	(*((short *)(a)))

#endif

