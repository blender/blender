/* util defines  -- might go away ?*/

/* 
	$Id$

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

#ifndef BKE_UTILDEFINES_H
#define BKE_UTILDEFINES_H

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define ELEM(a, b, c)           ( (a)==(b) || (a)==(c) )
#define ELEM3(a, b, c, d)       ( ELEM(a, b, c) || (a)==(d) )
#define ELEM4(a, b, c, d, e)    ( ELEM(a, b, c) || ELEM(a, d, e) )
#define ELEM5(a, b, c, d, e, f) ( ELEM(a, b, c) || ELEM3(a, d, e, f) )
#define ELEM6(a, b, c, d, e, f, g)      ( ELEM(a, b, c) || ELEM4(a, d, e, f, g) )
#define ELEM7(a, b, c, d, e, f, g, h)   ( ELEM3(a, b, c, d) || ELEM4(a, e, f, g, h) )
#define ELEM8(a, b, c, d, e, f, g, h, i)        ( ELEM4(a, b, c, d, e) || ELEM4(a, f, g, h, i) )

	
/* string compare */
#define STREQ(str, a)           ( strcmp((str), (a))==0 )
#define STREQ2(str, a, b)       ( STREQ(str, a) || STREQ(str, b) )
#define STREQ3(str, a, b, c)    ( STREQ2(str, a, b) || STREQ(str, c) )

/* min/max */
#define MIN2(x,y)               ( (x)<(y) ? (x) : (y) )
#define MIN3(x,y,z)             MIN2( MIN2((x),(y)) , (z) )
#define MIN4(x,y,z,a)           MIN2( MIN2((x),(y)) , MIN2((z),(a)) )

#define MAX2(x,y)               ( (x)>(y) ? (x) : (y) )
#define MAX3(x,y,z)             MAX2( MAX2((x),(y)) , (z) )
#define MAX4(x,y,z,a)           MAX2( MAX2((x),(y)) , MAX2((z),(a)) )

#define SWAP(type, a, b)        { type sw_ap; sw_ap=(a); (a)=(b); (b)=sw_ap; }

#define ABS(a)					( (a)<0 ? (-a) : (a) )

#define VECCOPY(v1,v2)          {*(v1)= *(v2); *(v1+1)= *(v2+1); *(v1+2)= *(v2+2);}
#define QUATCOPY(v1,v2)         {*(v1)= *(v2); *(v1+1)= *(v2+1); *(v1+2)= *(v2+2); *(v1+3)= *(v2+3);}

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


/* interferes elsewhere */
/* deze getallen ook invullen in blender.h SpaceFile: struct dna herkent geen defines */
#define FILE_MAXDIR			160
#define FILE_MAXFILE		80


/* some misc stuff.... */
#define CLAMP(a, b, c)		if((a)<(b)) (a)=(b); else if((a)>(c)) (a)=(c)

#define KNOTSU(nu)        ( (nu)->orderu+ (nu)->pntsu+ (nu->orderu-1)*((nu)->flagu & 1) )
#define KNOTSV(nu)        ( (nu)->orderv+ (nu)->pntsv+ (nu->orderv-1)*((nu)->flagv & 1) )  

/* this weirdo pops up in two places ... */
#if !defined(WIN32) && !defined(__BeOS)
#define O_BINARY 0
#endif

/* INTEGER CODES */
#if defined(__sgi) || defined (__sparc) || defined (__SPARC__) || defined (__PPC__) || defined (__ppc__) || defined (__BIG_ENDIAN__)
	/* Big Endian */
#define MAKE_ID(a,b,c,d) ( (int)(a)<<24 | (int)(b)<<16 | (c)<<8 | (d) )
#else
	/* Little Endian */
#define MAKE_ID(a,b,c,d) ( (int)(d)<<24 | (int)(c)<<16 | (b)<<8 | (a) )
#endif

#define ID_NEW(a)		if( (a) && (a)->id.newid ) (a)= (void *)(a)->id.newid

#define FORM MAKE_ID('F','O','R','M')
#define DDG1 MAKE_ID('3','D','G','1')
#define DDG2 MAKE_ID('3','D','G','2')
#define DDG3 MAKE_ID('3','D','G','3')
#define DDG4 MAKE_ID('3','D','G','4')

#define GOUR MAKE_ID('G','O','U','R')

#define BLEN MAKE_ID('B','L','E','N')
#define DER_ MAKE_ID('D','E','R','_')
#define V100 MAKE_ID('V','1','0','0')

#define DATA MAKE_ID('D','A','T','A')
#define GLOB MAKE_ID('G','L','O','B')
#define IMAG MAKE_ID('I','M','A','G')

#define DNA1 MAKE_ID('D','N','A','1')
#define TEST MAKE_ID('T','E','S','T')
#define REND MAKE_ID('R','E','N','D')
#define USER MAKE_ID('U','S','E','R')

#define ENDB MAKE_ID('E','N','D','B')

/* This should, of course, become a function */
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

/* This one rotates the bytes in an int */
#define SWITCH_INT(a) { \
    char s_i, *p_i; \
    p_i= (char *)&(a); \
    s_i=p_i[0]; p_i[0]=p_i[3]; p_i[3]=s_i; \
    s_i=p_i[1]; p_i[1]=p_i[2]; p_i[2]=s_i; }

/* More brain damage. Only really used by packedFile.c  */
// return values
#define RET_OK 0
#define RET_ERROR 1
/* and these aren't used at all */
/*  #define RET_CANCEL 2 */
/*  #define RET_YES (1 == 1) */
/*  #define RET_NO (1 == 0) */

/* sequence related defines */
#define WHILE_SEQ(base)	{											\
							int totseq_, seq_; Sequence **seqar;	\
							build_seqar( base,  &seqar, &totseq_);	\
							for(seq_ = 0; seq_ < totseq_; seq_++) {	\
									seq= seqar[seq_];
								

#define END_SEQ					}						\
							if(seqar) MEM_freeN(seqar);		\
							}


/* not really sure about these...  some kind of event codes ?*/ 
/* INFO: 300 */
/* pas op: ook in filesel.c en editobject.c */
#define B_INFOSCR		301
#define B_INFODELSCR	302
#define B_INFOSCE		304
#define B_INFODELSCE	305
#define B_FILEMENU		306
#define B_PACKFILE		307

/* From iff.h, but seemingly detached from anything else... To which
 * encoding scheme do they belong? */
#define AMI	    (1 << 31)
#define CDI	    (1 << 30)
#define Anim	(1 << 29)
#define TGA	    (1 << 28)
#define JPG		(1 << 27)
#define TIM	    (1 << 26)

#define TIM_CLUT	(010)
#define TIM_4		(TIM | TIM_CLUT | 0)
#define TIM_8		(TIM | TIM_CLUT | 1)
#define TIM_16		(TIM | 2)
#define TIM_24		(TIM | 3)

#define RAWTGA	(TGA | 1)

#define JPG_STD	(JPG | (0 << 8))
#define JPG_VID	(JPG | (1 << 8))
#define JPG_JST	(JPG | (2 << 8))
#define JPG_MAX	(JPG | (3 << 8))
#define JPG_MSK	(0xffffff00)

#define AM_ham	    (0x0800 | AMI)
#define AM_hbrite   (0x0080 | AMI)
#define AM_lace	    (0x0004 | AMI)
#define AM_hires    (0x8000 | AMI)
#define AM_hblace   (AM_hbrite | AM_lace)
#define AM_hilace   (AM_hires | AM_lace)
#define AM_hamlace  (AM_ham | AM_lace)

#define RGB888	1
#define RGB555	2
#define DYUV	3
#define CLUT8	4
#define CLUT7	5
#define CLUT4	6
#define CLUT3	7
#define RL7	8
#define RL3	9
#define MPLTE	10

#define DYUV1	0
#define DYUVE	1

#define CD_rgb8		(RGB888 | CDI)
#define CD_rgb5		(RGB555 | CDI)
#define CD_dyuv		(DYUV | CDI)
#define CD_clut8	(CLUT8 | CDI)
#define CD_clut7	(CLUT7 | CDI)
#define CD_clut4	(CLUT4 | CDI)
#define CD_clut3	(CLUT3 | CDI)
#define CD_rl7		(RL7 | CDI)
#define CD_rl3		(RL3 | CDI)
#define CD_mplte	(MPLTE | CDI)

#define C233	1
#define YUVX	2
#define HAMX	3
#define TANX	4

#define AN_c233			(Anim | C233)
#define AN_yuvx			(Anim | YUVX)
#define AN_hamx			(Anim | HAMX)
#define AN_tanx			(Anim | TANX)

#define IMAGIC 	0732

/* This used to reside in render.h. It does some texturing. */
#define BRICON		Tin= (Tin-0.5)*tex->contrast+tex->bright-0.5; \
					if(Tin<0.0) Tin= 0.0; else if(Tin>1.0) Tin= 1.0;

#define BRICONRGB	Tr= tex->rfac*((Tr-0.5)*tex->contrast+tex->bright-0.5); \
					if(Tr<0.0) Tr= 0.0; else if(Tr>1.0) Tr= 1.0; \
					Tg= tex->gfac*((Tg-0.5)*tex->contrast+tex->bright-0.5); \
					if(Tg<0.0) Tg= 0.0; else if(Tg>1.0) Tg= 1.0; \
					Tb= tex->bfac*((Tb-0.5)*tex->contrast+tex->bright-0.5); \
					if(Tb<0.0) Tb= 0.0; else if(Tb>1.0) Tb= 1.0;

/* mystifying stuff from blendef... */
#define SELECT			1
#define ACTIVE			2
#define NOT_YET			0

/* ???? */
#define BTST(a,b)     ( ( (a) & 1<<(b) )!=0 )   
#define BSET(a,b)     ( (a) | 1<<(b) )

/* needed for material.c*/
#define REDRAWBUTSMAT         0x4015

/* useless game shit */
#define MA_FH_NOR	2


#endif
