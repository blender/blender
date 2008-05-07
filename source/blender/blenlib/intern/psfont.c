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
 * fromtype1 - Convert an Adobe type 1 font into .of or .sf format.
 *				Paul Haeberli - 1990
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_vfontdata.h"
#include "BLI_blenlib.h"

#include "DNA_packedFile_types.h"
#include "DNA_curve_types.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

	/* ObjFnt types */

typedef struct chardesc {
    short movex, movey;		/* advance */
    short llx, lly;		/* bounding box */
    short urx, ury;
    short *data;		/* char data */
    long datalen;		
} chardesc;

typedef struct objfnt {
    struct objfnt *freeaddr;	/* if freeaddr != 0, objfnt is one chunck */
    short type;
    short charmin, charmax;
    short my_nchars;
    short scale;
    chardesc *my_chars;
} objfnt;

#define OFMAGIC		0x93339333

#define TM_TYPE		1
#define PO_TYPE		2
#define SP_TYPE		3

/* ops for tmesh characters */

#define	TM_BGNTMESH	(1)
#define	TM_SWAPTMESH	(2)
#define	TM_ENDBGNTMESH	(3)
#define	TM_RETENDTMESH	(4)
#define	TM_RET		(5)

/* ops for poly characters */

#define	PO_BGNLOOP	(1)
#define	PO_ENDBGNLOOP	(2)
#define	PO_RETENDLOOP	(3)
#define	PO_RET		(4)

/* ops for spline  characters */

#define	SP_MOVETO	(1)
#define	SP_LINETO	(2)
#define	SP_CURVETO	(3)
#define	SP_CLOSEPATH	(4)
#define	SP_RETCLOSEPATH	(5)
#define	SP_RET		(6)


#define MIN_ASCII 	' '
#define MAX_ASCII 	'~'
#define NASCII		(256 - 32)

#define NOBBOX		(30000)

typedef struct pschar {
    char *name;
    int code;
    int prog;
} pschar;

	/***/

#define SKIP 4
#define LINELEN 2048
#define NOTHEX		(100)
#define MC1 52845
#define MC2 22719
#define MAXSUBRS 4000
#define MAXCHARS 4000
#define MAXTRIES 30

/* some local thingies */
static void rcurveto( int dx1, int dy1, int dx2, int dy2, int dx3, int dy3);
static void makeobjfont(int savesplines);
static void drawchar(int c);
static void runprog(void);
static int chartoindex(objfnt *fnt, int c);
static short STDtoISO(short c);
static char * newfgets(char * s, int n, PackedFile * pf);
static int readfontmatrix(PackedFile * pf, float mat[2][2]);
static char mdecrypt(char cipher);
static void decryptall(void);
static int decodetype1(PackedFile * pf, char *outname);
static void fakefopen(void);
static char *fakefread(int n);
static void setcharlist(void);
static void initpcstack(void);
static char *poppc(void);
static void initstack(void);
static void push(int val);
static int pop(void);
static void initretstack(void);
static void retpush(int val);
static int retpop(void);
static void subr1(void);
static void subr2(void);
static void subr0(void);
static void append_poly_offset(short ofsx, short ofsy, short * data);
static void append_spline_offset(short ofsx, short ofsy, short * data);
static void setwidth(int w, int x);
static void poly_beginchar(void);
static void poly_endchar(void);
static void poly_close(void);
static void poly_pnt(float x, float y);
static void spline_beginchar(void);
static void spline_endchar(void);
static void spline_close(void);
static void spline_line(float x0, float y0, float x1, float y1);
static void spline_curveto(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3);
static void savestart(int x, int y);
static void sbpoint( int x, int y);
static void rmoveto( int x, int y);
static void drawline(float x0, float y0, float x1, float y1, float dx0, float dy0, float dx1, float dy1);
static void rlineto( int x, int y);
static void closepath(void);
static void bezadapt( float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float beztol);
static void drawbez( float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3);
static int docommand(int cmd);

/* some local vars */
static int startx, starty;
static int curx, cury;
static int nextx, nexty;
static int delx, dely;
static int started;


/* postscript commands */
#define HSTEM		(1)
#define VSTEM		(3)
#define VMOVETO		(4)
#define RLINETO		(5)
#define HLINETO		(6)
#define VLINETO		(7)
#define RRCURVETO	(8)
#define CLOSEPATH	(9)
#define CALLSUBR	(10)
#define RETURN		(11)
#define HSBW		(13)
#define ENDCHAR		(14)
#define RMOVETO		(21)
#define HMOVETO		(22)
#define VHCURVETO	(30)
#define HVCURVETO	(31)
#define DOTSECTION	(256+0)
#define VSTEM3		(256+1)
#define HSTEM3		(256+2)
#define SEAC		(256+6)
#define SBW		(256+7)
#define DIV		(256+12)
#define CALLOTHERSUBR	(256+16)
#define POP		(256+17)
#define SETCURRENTPOINT	(256+33)
#define WHAT0		(0)

static char oneline[LINELEN];
static objfnt *fnt;

static unsigned short int mr;

static char *bindat;
static int datbytes;
static int firsted;
static short chardata[20000];
static int nshorts;

static int thecharwidth, thesidebearing;
static int npnts, nloops;
static int nvertpos;

static int fakepos;
static int fakemax;

static float beztol = 100.0;

/* extern: from libfm */

static char *my_subrs[MAXSUBRS];
static unsigned int my_sublen[MAXSUBRS];
static char *my_chars[MAXCHARS];
static unsigned int my_charlen[MAXCHARS];
static char *my_charname[MAXCHARS];
static int my_nsubrs, my_nchars;

static short sidebearing[MAXCHARS];
static char tok[LINELEN];
static int sp_npnts, sp_nloops;

/* 
 *	interpreter globals
 */


static float mat[2][2];
static char *pcstack[100];
static char *pc;
static int pcsp;
static int coordpos;
static int coordsave[7][2];
static int incusp;
static int retstack[1000];
static int retsp;
static int stack[1000];
static int sp;
static int savesplines = 1;

static pschar ISOcharlist[NASCII] = {
	{"/space", 			040,	0},
	    {"/exclam", 	041,	0},
	    {"/quotedbl", 	042,	0},
	    {"/numbersign", 043,	0},
	    {"/dollar", 	044,	0},
	    {"/percent", 	045,	0},
	    {"/ampersand", 	046,	0},
	    {"/quoteright", 047,	0},

	    {"/parenleft", 	050,	0},
	    {"/parenright", 051,	0},
	    {"/asterisk", 	052,	0},
	    {"/plus", 		053,	0},
	    {"/comma", 		054,	0},
	    {"/hyphen", 	055,	0},
	    {"/period", 	056,	0},
	    {"/slash", 		057,	0},

	    {"/zero", 		060,	0},
	    {"/one", 		061,	0},
	    {"/two", 		062,	0},
	    {"/three", 		063,	0},
	    {"/four", 		064,	0},
	    {"/five", 		065,	0},
	    {"/six", 		066,	0},
	    {"/seven", 		067,	0},

	    {"/eight", 		070,	0},
	    {"/nine", 		071,	0},
	    {"/colon", 		072,	0},
	    {"/semicolon", 	073,	0},
	    {"/less",	 	074,	0},
	    {"/equal",	 	075,	0},
	    {"/greater",	076,	0},
	    {"/question", 	077,	0},

	    {"/at",	 	0100,	0},
	    {"/A",	 	0101,	0},
	    {"/B",	 	0102,	0},
	    {"/C",	 	0103,	0},
	    {"/D",	 	0104,	0},
	    {"/E",	 	0105,	0},
	    {"/F",	 	0106,	0},
	    {"/G",	 	0107,	0},

	    {"/H",	 	0110,	0},
	    {"/I",	 	0111,	0},
	    {"/J",	 	0112,	0},
	    {"/K",	 	0113,	0},
	    {"/L",	 	0114,	0},
	    {"/M",	 	0115,	0},
	    {"/N",	 	0116,	0},
	    {"/O",	 	0117,	0},

	    {"/P",	 	0120,	0},
	    {"/Q",	 	0121,	0},
	    {"/R",	 	0122,	0},
	    {"/S",	 	0123,	0},
	    {"/T",	 	0124,	0},
	    {"/U",	 	0125,	0},
	    {"/V",	 	0126,	0},
	    {"/W",	 	0127,	0},

	    {"/X",	 			0130,	0},
	    {"/Y",	 			0131,	0},
	    {"/Z",	 			0132,	0},
	    {"/bracketleft", 	0133,	0},
	    {"/backslash",		0134,	0},
	    {"/bracketright", 	0135,	0},
	    {"/asciicircum",	0136,	0},
	    {"/underscore", 	0137,	0},

	    {"/quoteleft", 	0140,	0},
	    {"/a",	 		0141,	0},
	    {"/b",	 		0142,	0},
	    {"/c", 			0143,	0},
	    {"/d",			0144,	0},
	    {"/e", 			0145,	0},
	    {"/f",			0146,	0},
	    {"/g",	 		0147,	0},

	    {"/h",	 	0150,	0},
	    {"/i",	 	0151,	0},
	    {"/j",	 	0152,	0},
	    {"/k", 		0153,	0},
	    {"/l",		0154,	0},
	    {"/m", 		0155,	0},
	    {"/n",		0156,	0},
	    {"/o",	 	0157,	0},

	    {"/p",	 	0160,	0},
	    {"/q",	 	0161,	0},
	    {"/r",	 	0162,	0},
	    {"/s", 		0163,	0},
	    {"/t",		0164,	0},
	    {"/u",		0165,	0},
	    {"/v", 		0166,	0},
	    {"/w",		0167,	0},

	    {"/x",	 		0170,	0},
	    {"/y",		 	0171,	0},
	    {"/z",	 		0172,	0},
	    {"/braceleft", 	0173,	0},
	    {"/bar",		0174,	0},
	    {"/braceright",	0175,	0},
	    {"/asciitilde", 0176,	0},
	    {"/",			0177,	0},


	    /* nonstandard defs */

	{"/quotedblleft",		0200,	0},
	    {"/quotedblright",	0201,	0},
	    {"/quotedblbase",	0202,	0},
	    {"/quotesinglbase",	0203,	0},
	    {"/guilsinglleft",	0204,	0},
	    {"/guilsinglright",	0205,	0},
	    {"/endash",			0206,	0},
	    {"/dagger",			0207,	0},

	    {"/daggerdbl",		0210,	0},
	    {"/trademark",		0211,	0},
	    {"/bullet",			0212,	0},
	    {"/perthousand",	0213,	0},
	    {"/Lslash",			0214,	0},
	    {"/OE",				0215,	0},
	    {"/lslash",			0216,	0},
	    {"/oe",				0217,	0},

	    /* endnonstandard defs */

	{"/dotlessi",		0220,	0},
	    {"/grave",		0221,	0},
	    {"/acute",		0222,	0},
	    {"/circumflex",	0223,	0},
	    {"/tilde",		0224,	0},
	    {"/",			0225,	0},
	    {"/breve",		0226,	0},
	    {"/dotaccent",	0227,	0},

	    {"/",				0230,	0},
	    {"/",				0231,	0},
	    {"/ring",			0232,	0},
	    {"/",				0233,	0},
	    {"/",				0234,	0},
	    {"/hungarumlaut",	0235,	0},
	    {"/ogonek",			0236,	0},
	    {"/caron",			0237,	0},

	    {"/",			0240,	0},
	    {"/exclamdown",	0241,	0},
	    {"/cent",		0242,	0},
	    {"/sterling",	0243,	0},
	    {"/florin",		0244,	0},
	    {"/yen",		0245,	0},
	    {"/brokenbar",	0246,	0},
	    {"/section",	0247,	0},

	    {"/dieresis",		0250,	0},
	    {"/copyright",		0251,	0},
	    {"/ordfeminine",	0252,	0},
	    {"/guillemotleft",	0253,	0},
	    {"/logicalnot",		0254,	0},
	    {"/hyphen",			0255,	0},
	    {"/registered",		0256,	0},
	    {"/macron",			0257,	0},

	    {"/degree",			0260,	0},
	    {"/plusminus",		0261,	0},
	    {"/twosuperior",	0262,	0},
	    {"/threesuperior",	0263,	0},
	    {"/acute",			0264,	0},
	    {"/mu",				0265,	0},
	    {"/paragraph",		0266,	0},
	    {"/periodcentered",	0267,	0},

	    {"/cedilla",		0270,	0},
	    {"/onesuperior",	0271,	0},
	    {"/ordmasculine",	0272,	0},
	    {"/guillemotright",	0273,	0},
	    {"/onequarter",		0274,	0},
	    {"/onehalf",		0275,	0},
	    {"/threequarters",	0276,	0},
	    {"/questiondown",	0277,	0},

	    {"/Agrave",			0300,	0},
	    {"/Aacute",			0301,	0},
	    {"/Acircumflex",	0302,	0},
	    {"/Atilde",			0303,	0},
	    {"/Adieresis",		0304,	0},
	    {"/Aring",			0305,	0},
	    {"/AE",				0306,	0},
	    {"/Ccedilla",		0307,	0},

	    {"/Egrave",			0310,	0},
	    {"/Eacute",			0311,	0},
	    {"/Ecircumflex",	0312,	0},
	    {"/Edieresis",		0313,	0},
	    {"/Igrave",			0314,	0},
	    {"/Iacute",			0315,	0},
	    {"/Icircumflex",	0316,	0},
	    {"/Idieresis",		0317,	0},

	    {"/Eth",			0320,	0},
	    {"/Ntilde",			0321,	0},
	    {"/Ograve",			0322,	0},
	    {"/Oacute",			0323,	0},
	    {"/Ocircumflex",	0324,	0},
	    {"/Otilde",			0325,	0},
	    {"/Odieresis",		0326,	0},
	    {"/multiply",		0327,	0},

	    {"/Oslash",		0330,	0},
	    {"/Ugrave",		0331,	0},
	    {"/Uacute",		0332,	0},
	    {"/Ucircumflex",0333,	0},
	    {"/Udieresis",	0334,	0},
	    {"/Yacute",		0335,	0},
	    {"/Thorn",		0336,	0},
	    {"/germandbls",	0337,	0},

	    {"/agrave",		0340,	0},
	    {"/aacute",		0341,	0},
	    {"/acircumflex",0342,	0},
	    {"/atilde",		0343,	0},
	    {"/adieresis",	0344,	0},
	    {"/aring",		0345,	0},
	    {"/ae",			0346,	0},
	    {"/ccedilla",	0347,	0},

	    {"/egrave",			0350,	0},
	    {"/eacute",			0351,	0},
	    {"/ecircumflex",	0352,	0},
	    {"/edieresis",		0353,	0},
	    {"/igrave",			0354,	0},
	    {"/iacute",			0355,	0},
	    {"/icircumflex",	0356,	0},
	    {"/idieresis",		0357,	0},

	    {"/eth",		0360,	0},
	    {"/ntilde",		0361,	0},
	    {"/ograve",		0362,	0},
	    {"/oacute",		0363,	0},
	    {"/ocircumflex",0364,	0},
	    {"/otilde",		0365,	0},
	    {"/odieresis",	0366,	0},
	    {"/divide",		0367,	0},

	    {"/oslash",		0370,	0},
	    {"/ugrave",		0371,	0},
	    {"/uacute",		0372,	0},
	    {"/ucircumflex",0373,	0},
	    {"/udieresis",	0374,	0},
	    {"/yacute",		0375,	0},
	    {"/thorn",		0376,	0},
	    {"/ydieresis",	0377,	0},
};


static short STDvsISO [][2] = {
	{0341, 0306}, /* AE */
	{0351, 0330}, /* Oslash */
	{0302, 0222}, /* acute */
	{0361, 0346}, /* ae */
	{0306, 0226}, /* breve */
	{0317, 0237}, /* caron */
	{0313, 0270}, /* cedilla */
	{0303, 0223}, /* circumflex */
	{0250, 0244}, /* currency */
	{0310, 0250}, /* dieresis */
	{0307, 0227}, /* dotaccent */
	{0365, 0220}, /* dotlessi */
	{0373, 0337}, /* germandbls */
	{0301, 0221}, /* grave */
	{0315, 0235}, /* hungarumlaut */
	{0055, 0255}, /* hyphen */
	{0305, 0257}, /* macron */
	{0316, 0236}, /* ogenek */
	{0343, 0252}, /* ordfeminine */
	{0353, 0272}, /* ordmasculine */
	{0371, 0370}, /* oslash */
	{0264, 0267}, /* periodcentered */
	{0312, 0232}, /* ring */
	{0304, 0224}, /* tilde */
};

/* from objfont.c, rest is in lfm_s !!*/

/* START 5.2 */

static int chartoindex(objfnt *fnt, int c)
{
	if(c<fnt->charmin)
		return -1;
	if(c>fnt->charmax)
		return -1;
	return c-fnt->charmin;
}


static chardesc *getchardesc(objfnt *fnt, int c)
{
	int index;

	index = chartoindex(fnt,c);
	if(index<0)
		return 0;
	return fnt->my_chars+index;
}

static objfnt *newobjfnt(int type, int charmin, int charmax, int fscale)
{
	objfnt *fnt;

	fnt = (objfnt *)MEM_mallocN(sizeof(objfnt), "newobjfnt");
	fnt->freeaddr = 0;
	fnt->type = type;
	fnt->charmin = charmin;
	fnt->charmax = charmax;
	fnt->my_nchars = fnt->charmax-fnt->charmin+1;
	fnt->scale = fscale;
	fnt->my_chars = (chardesc *)MEM_mallocN(fnt->my_nchars*sizeof(chardesc), "newobjfnt2");
	memset(fnt->my_chars, 0, fnt->my_nchars*sizeof(chardesc));
	return fnt;
}


static void addchardata (objfnt * fnt, int c, short * data, int nshorts)
{
	int index;
	chardesc *cd;

	index = chartoindex(fnt,c);
	if(index<0) {
		fprintf(stderr,"Addchardata bad poop\n");
		return;
	}
	cd = fnt->my_chars+index;
	fnt->freeaddr = 0;
	cd->datalen = nshorts*sizeof(short);
	cd->data = (short *)MEM_mallocN(cd->datalen, "addchardata");
	memcpy(cd->data, data, cd->datalen);
}

static void addcharmetrics(objfnt *fnt, int c, int movex, int movey)
{
	int index;
	chardesc *cd;

	index = chartoindex(fnt,c);
	if(index<0) {
		fprintf(stderr,"Addcharmetrics bad poop\n");
		return;
	}
	cd = fnt->my_chars+index;
	cd->movex = movex;
	cd->movey = movey;
}


static void fakechar(objfnt *fnt, int c, int width)
{
	short chardata[1];

	chardata[0] = PO_RET;
	addchardata(fnt,c,chardata,1);
	addcharmetrics(fnt,c,width,0);
}


static void freeobjfnt(objfnt * fnt)
{
	int i;
	chardesc *cd;

	cd = fnt->my_chars;
	for(i=0; i<fnt->my_nchars; i++) {
		if(cd->data)
			MEM_freeN(cd->data);
		cd++;
	}
	MEM_freeN(fnt->my_chars);
	MEM_freeN(fnt);
}


/* END 5.2 */

static short STDtoISO(short c)
{
	short i = (sizeof(STDvsISO) / (2 * sizeof(short))) - 1;

	for (;i >= 0; i--){
		if (STDvsISO[i][0] == c) return (STDvsISO[i][1]);
	}
	return(c);
}


/*
 *	read the font matrix out of the font file
 *
 */

static char * newfgets(char * s, int n, PackedFile * pf){
	int c;
	char * p;

	p = s;
	while (n > 0){
		c = ((char *) pf->data)[pf->seek];
		pf->seek++;
		if (pf->seek > pf->size){
			return (0);
		}
		if (c == 10 || c == 13){
			*p = 0;
			return(s);
		}
		*p++ = c;
		n--;
	}
	*p = 0;
	return(s);
}

static int readfontmatrix(PackedFile * pf, float mat[2][2])
{
	char *cptr;
	float a, b, c, d, e, f;

	pf->seek = 0;
	
	/* look for the FontMatrix def */
	while(1) {
		if(!newfgets(oneline, LINELEN, pf)) {
			fprintf(stderr,"fromtype1: no FontMatrix found\n");
			return(-1);
		}
		cptr = strchr(oneline,'/');
		if(cptr) {
			if(strncmp(cptr,"/FontMatrix",11) == 0) {
				cptr = strchr(cptr,'[');
				if(!cptr) {
					fprintf(stderr,"fromtype1: bad FontMatrix line\n");
					return(-1);
				}
				sscanf(cptr+1,"%f %f %f %f %f %f\n",&a,&b,&c,&d,&e,&f);
				break;
			}
		}
	}

	mat[0][0] = 1000.0*a;
	mat[1][0] = 1000.0*b;
	mat[0][1] = 1000.0*c;
	mat[1][1] = 1000.0*d;

	return(0);
}

/*
 *	Decryption support
 *
 *
 */
static void resetdecrypt(int n)
{
	mr = n;
}



/*
 * 	decryption subroutines
 *
 */

static char mdecrypt(char cipher)
{
	char plain;

	plain = (cipher^(mr>>8));
	mr = (cipher+mr)*MC1 + MC2;
	return plain;
}

static void decryptdata(char * cptr, int n)
{
	while(n--) {
		*cptr = mdecrypt(*cptr);
		cptr++;
	}
}

static int decryptprogram(char *buf, int len)
{
	int i;

	resetdecrypt(4330);
	for(i=0; i<len; i++) {
		if(i<SKIP) {
			mdecrypt(buf[i]);
		}
		else {
			buf[i-SKIP] = mdecrypt(buf[i]);
		}
	}
	return len-SKIP;
}

static void decryptall(void)
{
	int i;

	for(i=0; i<my_nsubrs; i++)
		my_sublen[i] = decryptprogram(my_subrs[i],my_sublen[i]);
	for(i=0; i<my_nchars; i++)
		my_charlen[i] = decryptprogram(my_chars[i],my_charlen[i]);
}


/*
 *	decode the eexec part of the file
 *
 */

static int decodetype1(PackedFile * pf, char *outname)
{
	char *hptr, *bptr;
	int i, totlen, hexbytes, c;
	char *hexdat;
	char hextab[256];

	/* make hex table */
	if(!firsted) {
		for(i=0; i<256; i++) {
			if(i>='0' && i<='9')
				hextab[i] = i-'0';
			else if(i>='a' && i<='f')
				hextab[i] = 10+i-'a';
			else if(i>='A' && i<='F')
				hextab[i] = 10+i-'A';
			else
				hextab[i] = NOTHEX;
		}
	}

	pf->seek = 0;
	
	/* allocate buffers */
	totlen = pf->size;
	hexdat = (char *)MEM_mallocN(totlen, "hexdat");
	bindat = (char *)MEM_mallocN(totlen, "bindat");

	/* look for eexec part of file */
	while(1) {
		if(!newfgets(oneline, LINELEN, pf)) {
			fprintf(stderr,"fromtype1: no currentfile eexec found\n");
			return(-1);
		}
		oneline[16] = 0;
		if(strcmp(oneline,"currentfile eexe") == 0)
			break;
	}

	/* initialize decryption variables */
	mr = 55665;

	/* first byte == 0 for binary data (???) */
	
	c = ((char *) pf->data)[pf->seek];

	if (hextab[c] != NOTHEX){
		/* read all the hex bytes into the hex buffer */
		hexbytes = 0;
		while(newfgets(oneline, LINELEN, pf)) {
			hptr = (char *)oneline;
			while(*hptr) {
				if(hextab[*hptr] != NOTHEX)
					hexdat[hexbytes++] = *hptr;
				hptr++;
			}
		}

		/* check number of hex bytes */
		if(hexbytes & 1)
			hexbytes--;
		datbytes = hexbytes/2;

		/* translate hex data to binary */
		hptr = hexdat;
		bptr = bindat;
		c = datbytes;
		while(c--) {
			*bptr++  = (hextab[hptr[0]]<<4)+hextab[hptr[1]];
			hptr += 2;
		}

		/* decrypt the data */
		decryptdata(bindat,datbytes);

	} else {
		datbytes = pf->size - pf->seek;
		memcpy(bindat, ((char *) pf->data) + pf->seek, datbytes);

		if ((bindat[2] << (8 + bindat[3])) == 0x800){
			/* order data (remove 6 bytes headers) */
			i = datbytes;
			hptr = bptr = bindat + 4;
			hptr += 2;

			while (i > 0){
				if (i > 2046) c = 2046;
				else c = i;

				memcpy(bptr, hptr, c);
				bptr += 2046;
				hptr += 2046 + 6;
				i -= 2046 + 6;
				datbytes -= 6;
			}

			/* decrypt the data */
			decryptdata(bindat+4,datbytes);
		} else{
			decryptdata(bindat+6,datbytes-6);
		}
	}

#ifdef DEBUG
	outf = fopen(outname,"wb");
	fwrite(bindat,datbytes,1,outf);
	fclose(outf);
#endif 

	MEM_freeN(hexdat);
	
	return 1;
}

/* 
 *	fake file reading funcs
 *
 *
 */

static void fakefopen(void)
{
	fakepos = 0;
	fakemax = datbytes;
}


static void fakegettoken(char *str)
{
	int c;
	char *cptr;
	char *start;

	start = (char *) str;
	cptr = bindat+fakepos;
	c = *cptr++;
	fakepos++;
	if(c != '\n') {
		while(isspace(c)) {
			c = *cptr++;
			fakepos++;
		}
		while (fakepos<fakemax && !isspace(c)) {
			*str++ = c;
			c = *cptr++;
			fakepos++;
		}
		if(c == '\n')
			fakepos--;
	}
	*str = 0;
	if(fakepos>fakemax) {
		fprintf(stderr,"fromtype1: unexpected eof\n");
		strcpy(start, "end");
	}
}

static int fakefgets(char *buf,int max)
{
	char *cptr;

	cptr = (char *)(bindat+fakepos);
	while(max--) {
		*buf++ = *cptr;
		fakepos++;
		if(*cptr == 10 || *cptr == 13)
			return 1;
		cptr++;
		if(fakepos>fakemax)
			return 0;
	}
	return 0;
}

static char *fakefread(int n)
{
	fakepos += n;
	return bindat+fakepos-n;
}

static void applymat(float mat[][2], float *x, float *y)
{
	float tx, ty;

	tx = ((*x)*mat[0][0])+((*y)*mat[0][1]);
	ty = ((*x)*mat[1][0])+((*y)*mat[1][1]);
	*x = tx;
	*y = ty;
}

static void setcharlist(void)
{
	char *name, found;
	int i, j;

	for(i=0; i<NASCII; i++) ISOcharlist[i].prog = -1;

	for(j=0; j<my_nchars; j++) {
		name = my_charname[j];
		if(name) {
			found = 0;
			for(i=0; i<NASCII; i++) {
				if(ISOcharlist[i].name && (strcmp(name,ISOcharlist[i].name) == 0)){
					ISOcharlist[i].prog = j;
					found = 1;
				}
			}
			/*if (found == 0) printf("no match found for: %s\n", name);*/
			MEM_freeN(name);
			my_charname[j] = 0;
		}
	}
}


static objfnt * objfnt_from_psfont(PackedFile * pf)
{
	int i, k, index;
	int nread, namelen;
	char *cptr;
			
	fnt = 0;
	bindat = 0;
	
	/* read the font matrix from the font */
	if (readfontmatrix(pf,mat)) return(0);

	/* decode the font data */
	decodetype1(pf, "/usr/tmp/type1.dec");

	/* open the input file */
	fakefopen();

	/* look for the /Subrs def and get my_nsubrs */
	while(1) {
		if(!fakefgets(oneline,LINELEN)) {
			fprintf(stderr,"fromtype1: no /Subrs found\n");
			my_nsubrs = 0;
			fakefopen();
			break;
		}
		cptr = strchr(oneline,'/');
		if(cptr) {
			if(strncmp(cptr,"/Subrs",6) == 0) {
				my_nsubrs = atoi(cptr+6);
				break;
			}
		}
	}

	/* read the Subrs in one by one */
	for(i=0; i<my_nsubrs; i++)
		my_sublen[i] = 0;
	for(i=0; i<my_nsubrs; i++) {
		for(k=0; k<MAXTRIES; k++) {
			fakegettoken(tok);
			if(strcmp(tok,"dup") == 0)
				break;
		}
		if(k == MAXTRIES) {
			fprintf(stderr,"dup for subr %d not found in range\n", i);
			/*exit(1);*/
		}

		/* get the Subr index here */
		fakegettoken(tok);
		index = atoi(tok);

		/* check to make sure it is in range */
		if(index<0 || index>my_nsubrs) {
			fprintf(stderr,"bad Subr index %d\n",index);
			/*exit(1);*/
		}

		/* get the number of bytes to read */
		fakegettoken(tok);
		nread = atoi(tok);
		fakegettoken(tok);

		/* read in the subroutine */
		my_sublen[index] = nread;
		my_subrs[index] = fakefread(nread);
		fakegettoken(tok);
	}

	/* look for the CharStrings */
	while(1) {
		fakegettoken(tok);
		cptr = strchr(tok,'/');
		if(cptr && strcmp(cptr,"/CharStrings") == 0)
			break;
	}

	fakegettoken(tok);	/* skip my_ncharscrings */
	fakegettoken(tok);	/* skip dict */
	fakegettoken(tok);	/* skip dup */
	fakegettoken(tok);	/* skip begin */
	fakegettoken(tok);	/* skip newline */

	/* read the CharStrings one by one */
	my_nchars = 0;
	for(i=0; i<MAXCHARS; i++) {

		/* check for end */
		fakegettoken(tok);
		if(strcmp(tok,"end") == 0)
			break;

		/* get the char name and allocate space for it */
		namelen = strlen(tok);
		my_charname[i] = (char *)MEM_mallocN(namelen+1, "my_charname");
		strcpy(my_charname[i],tok);

		/* get the number of bytes to read */
		fakegettoken(tok);
		nread = atoi(tok);
		fakegettoken(tok);

		/* read in the char description */
		my_charlen[i] = nread;
		my_chars[i] = fakefread(nread);

		/* skip the end of line */
		fakegettoken(tok);
		fakegettoken(tok);
		my_nchars++;
	}

	/* decrypt the character descriptions */
	decryptall();
	setcharlist();

	/* make the obj font */
	makeobjfont(savesplines);

	if (bindat) MEM_freeN(bindat);
	/* system("rm /usr/tmp/type1.dec"); */

	return (fnt);
}




/*
 *	pc stack support
 *
 */

static void initpcstack(void)
{
	pcsp = 0;
}

static void pushpc(char *pc)
{
	pcstack[pcsp] = pc;
	pcsp++;
}

static char *poppc(void)
{
	pcsp--;
	if(pcsp<0) {
		fprintf(stderr,"\nYUCK: pc stack under flow\n");
		pcsp = 0;
		return 0;
	}
	return pcstack[pcsp];
}

/*
 *	Data stack support
 *
 */

static void initstack(void)
{
	sp = 0;
}

static void push(int val)
/*  int val; */
{
	stack[sp] = val;
	sp++;
}

static int pop(void)
{
	sp--;
	if(sp<0) {
		fprintf(stderr,"\nYUCK: stack under flow\n");
		sp = 0;
		return 0;
	}
	return stack[sp];
}

/*
 *	call/return data stack
 *
 */

static void initretstack(void)
{
	retsp = 0;
}

static void retpush(int val)
/*  int val; */
{
	retstack[retsp] = val;
	retsp++;
}

static int retpop(void)
{
	retsp--;
	if(retsp<0) {
		fprintf(stderr,"\nYUCK: ret stack under flow\n");
		retsp = 0;
		return 0;
	}
	return retstack[retsp];
}


/*
 *	execute the program:
 *
 *
 */

static void getmove(int *x, int *y)
{
	*x = delx;
	*y = dely;
	/* 	printf("ingetmove\n"); */
}

static void getpos(int *x, int *y)
{
	*x = curx;
	*y = cury;
}

static void subr1(void)
{
	coordpos = 0;
	incusp = 1;
}

static void subr2(void)
{
	int x, y;

	getmove(&x,&y);
	if(coordpos>=7) {
		fprintf(stderr,"subr2: bad poop\n");
		/*exit(1);*/
	}
	coordsave[coordpos][0] = x;
	coordsave[coordpos][1] = y;
	coordpos++;
}

static void subr0(void)
{
	int x0, y0;
	int x1, y1;
	int x2, y2;
	int x3, y3;
	int xpos, ypos, noise;

	ypos = pop();
	xpos = pop();
	noise = pop();
	if(coordpos!=7) {
		fprintf(stderr,"subr0: bad poop\n");
		/*exit(1);*/
	}
	x0 =  coordsave[0][0];
	y0 =  coordsave[0][1];

	x1 =  coordsave[1][0]+x0;
	y1 =  coordsave[1][1]+y0;
	x2 =  coordsave[2][0];
	y2 =  coordsave[2][1];
	x3 =  coordsave[3][0];
	y3 =  coordsave[3][1];
	rcurveto(x1,y1,x1+x2,y1+y2,x1+x2+x3,y1+y2+y3);
	x1 =  coordsave[4][0];
	y1 =  coordsave[4][1];
	x2 =  coordsave[5][0];
	y2 =  coordsave[5][1];
	x3 =  coordsave[6][0];
	y3 =  coordsave[6][1];
	rcurveto(x1,y1,x1+x2,y1+y2,x1+x2+x3,y1+y2+y3);
	getpos(&x0,&y0);
	retpush(y0);
	retpush(x0);
	incusp = 0;
}

static void append_poly_offset(short ofsx, short ofsy, short * data)
{
	int nverts;

	if (data == 0) return;

	while(1) {
		switch(chardata[nshorts++] = *data++) {
		case PO_BGNLOOP:
			nshorts --;	/* for the first time */
			break;
		case PO_RETENDLOOP:
		case PO_RET:
			return;
		}
		nverts = chardata[nshorts++] = *data++;
		while(nverts--) {
			chardata[nshorts++] = (*data++) + ofsx;
			chardata[nshorts++] = (*data++) + ofsy;
		}
	}
}


static void append_spline_offset(short ofsx, short ofsy, short * data)
{
	int nverts = 0;

	if (data == 0) return;

	while(1) {
		switch(chardata[nshorts++] = *data++) {
		case SP_MOVETO:
		case SP_LINETO:
			nverts = 1;
			break;
		case SP_CURVETO:
			nverts = 3;
			break;
		case SP_RETCLOSEPATH:
		case SP_RET:
			return;
		}

		for (; nverts > 0; nverts--) {
			chardata[nshorts++] = (*data++) + ofsx;
			chardata[nshorts++] = (*data++) + ofsy;
		}
	}
}



/* 
 *    graphics follows 
 *
 *
 */


/* poly output stuff */

static void setwidth(int w, int x)
{
	thecharwidth = w;
	thesidebearing = x;
}

static void poly_beginchar(void)
{
	npnts = 0;
	nloops = 0;
}

static void poly_endchar(void)
{
	if(nloops == 0)
		chardata[nshorts++] = PO_RET;
	else
		chardata[nshorts++] = PO_RETENDLOOP;
}

static void poly_close(void)
{
	chardata[nvertpos] = npnts;
	npnts = 0;
}

static void poly_pnt(float x, float y)
{
	int ix, iy;

	applymat(mat,&x,&y);
	ix = floor(x);
	iy = floor(y);
	if(npnts == 0) {
		if(nloops == 0) {
			chardata[nshorts++] = PO_BGNLOOP;
			nvertpos = nshorts++;
		} else {
			chardata[nshorts++] = PO_ENDBGNLOOP;
			nvertpos = nshorts++;
		}
		nloops++;
	}
	chardata[nshorts++] = ix;
	chardata[nshorts++] = iy;
	npnts++;

}

/* spline output stuff */

static void spline_beginchar(void)
{
	sp_npnts = 0;
	sp_nloops = 0;
}

static void spline_endchar(void)
{
	if(sp_nloops == 0)
		chardata[nshorts++] = SP_RET;
	else
		chardata[nshorts++] = SP_RETCLOSEPATH;
}

static void spline_close(void)
{
	chardata[nshorts++] = SP_CLOSEPATH;
	sp_npnts = 0;
	sp_nloops = 0;
}

static void spline_line(float x0, float y0, float x1, float y1)
{
	applymat(mat,&x0,&y0);
	applymat(mat,&x1,&y1);

	if(sp_npnts == 0) {
		chardata[nshorts++] = SP_MOVETO;
		chardata[nshorts++] = floor(x0);
		chardata[nshorts++] = floor(y0);
		sp_npnts++;
		sp_nloops++;
	}
	chardata[nshorts++] = SP_LINETO;
	chardata[nshorts++] = floor(x1);
	chardata[nshorts++] = floor(y1);
	sp_npnts++;
}

static void spline_curveto(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
	applymat(mat,&x0,&y0);

	applymat(mat,&x1,&y1);
	applymat(mat,&x2,&y2);
	applymat(mat,&x3,&y3);
	if(sp_npnts == 0) {
		chardata[nshorts++] = SP_MOVETO;
		chardata[nshorts++] = floor(x0);
		chardata[nshorts++] = floor(y0);
		sp_npnts++;
		sp_nloops++;
	}
	chardata[nshorts++] = SP_CURVETO;
	chardata[nshorts++] = floor(x1);
	chardata[nshorts++] = floor(y1);
	chardata[nshorts++] = floor(x2);
	chardata[nshorts++] = floor(y2);
	chardata[nshorts++] = floor(x3);
	chardata[nshorts++] = floor(y3);
}

static void savestart(int x, int y)
{
	startx = x;
	starty = y;
	started = 1;
}

static void sbpoint( int x, int y)
{
	curx = x;
	cury = y;
}

static void rmoveto( int x, int y)
{
	if(incusp) {
		delx = x;
		dely = y;
	} else {
		curx += x;
		cury += y;
		savestart(curx,cury);
	}
}

static void drawline(float x0, float y0, float x1, float y1, float dx0, float dy0, float dx1, float dy1)
{
	if(x0!=x1 || y0!=y1)
		poly_pnt(x1,y1);
}


static void rlineto( int x, int y)
{
	float dx, dy;

	nextx = curx + x;
	nexty = cury + y;
	dx = nextx-curx;
	dy = nexty-cury;
	if (savesplines) spline_line( curx, cury, nextx, nexty);
	else drawline( curx, cury, nextx, nexty,dx,dy,dx,dy);
	curx = nextx;
	cury = nexty;
}

static void closepath(void)
{
	float dx, dy;

	if(started) {
		dx = startx-curx;
		dy = starty-cury;
		if (savesplines) {
			spline_close();
		} else {
			drawline( curx, cury, startx, starty,dx,dy,dx,dy);
			poly_close();
		}
		started = 0;
	}
}

static void bezadapt( float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float beztol)
{
	float ax0,ay0,ax1,ay1,ax2,ay2,ax3,ay3;
	float bx0,by0,bx1,by1,bx2,by2,bx3,by3;
	float midx, midy;
	float linx, liny, dx, dy, mag;

	midx = (x0+3*x1+3*x2+x3)/8.0;
	midy = (y0+3*y1+3*y2+y3)/8.0;
	linx = (x0+x3)/2.0;
	liny = (y0+y3)/2.0;
	dx = midx-linx;
	dy = midy-liny;
	mag = dx*dx+dy*dy;
	if(mag<(beztol*beztol))
		drawline(x0,y0,x3,y3,x1-x0,y1-y0,x3-x2,y3-y2);
	else {
		ax0 = x0;
		ay0 = y0;
		ax1 = (x0+x1)/2;
		ay1 = (y0+y1)/2;
		ax2 = (x0+2*x1+x2)/4;
		ay2 = (y0+2*y1+y2)/4;
		ax3 = midx;
		ay3 = midy;
		bezadapt(ax0,ay0,ax1,ay1,ax2,ay2,ax3,ay3,beztol);

		bx0 = midx;
		by0 = midy;
		bx1 = (x1+2*x2+x3)/4;
		by1 = (y1+2*y2+y3)/4;
		bx2 = (x2+x3)/2;
		by2 = (y2+y3)/2;
		bx3 = x3;
		by3 = y3;
		bezadapt(bx0,by0,bx1,by1,bx2,by2,bx3,by3,beztol);
	}
}

static void drawbez( float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
	bezadapt(x0,y0,x1,y1,x2,y2,x3,y3,beztol);
}


static void rcurveto( int dx1, int dy1, int dx2, int dy2, int dx3, int dy3)
{
	int x0, y0;
	int x1, y1;
	int x2, y2;
	int x3, y3;

	x0 = curx;
	y0 = cury;
	x1 = curx+dx1;
	y1 = cury+dy1;
	x2 = curx+dx2;
	y2 = cury+dy2;
	x3 = curx+dx3;
	y3 = cury+dy3;

	if (savesplines) {
		spline_curveto( x0, y0, x1, y1, x2, y2, x3, y3);
	} else{
		drawbez( x0, y0, x1, y1, x2, y2, x3, y3);
	}
	curx = x3;
	cury = y3;
}

/*
 *	saveobjfont -
 *		save an object font.
 *
 */

/* generic routines */

static void makeobjfont(int savesplines)
{
	int i, c;

	if(savesplines)
		fnt = newobjfnt(SP_TYPE, 32, 32+NASCII-1, 9840);
	else
		fnt = newobjfnt(PO_TYPE, 32, 32+NASCII-1, 9840);

	for(i=0; i<NASCII; i++) {
		c = i+32;
		if(ISOcharlist[i].prog>=0) {
			/*printf("decoding %s\n", ISOcharlist[i].name);*/

			nshorts = 0;
			drawchar(ISOcharlist[i].prog);
			addchardata(fnt,c,chardata,nshorts);
			addcharmetrics(fnt,c,thecharwidth,0);
			sidebearing[c] = thesidebearing;
		} else if(c == ' ') {
			printf("faking space %d\n",i);
			fakechar(fnt,' ',400);
		}
	}
}

/*
 * run the character program	
 *
 *
 */

static void drawchar(int c)
{
	if (savesplines) {
		spline_beginchar();
	} else {
		poly_beginchar();
	}
	initstack();
	initpcstack();
	initretstack();
	pc = my_chars[c];
	runprog();
	if (savesplines){
		spline_endchar();
	} else {
		poly_endchar();
	}
}

static int docommand(int cmd)
{
	int x, y, w, c1, c2;
	int dx1, dy1;
	int dx2, dy2;
	int dx3, dy3;
	float fdx1, fdy1;
	int i, sub, n;
	char *subpc;
	chardesc *cd;
	short *ndata;

	switch(cmd) {
	case WHAT0:
		fprintf(stderr,"\nYUCK: WHAT0\n");
		break;
	case HSTEM:
		pop();
		pop();
		/*printf("hstem: %d %d\n", pop(), pop());*/
		break;
	case VSTEM:
		pop();
		pop();
		/*printf("vstem: %d %d\n", pop(), pop());*/
		break;
	case VMOVETO:
		y = pop();
		rmoveto(0,y);
		break;
	case RLINETO:
		y = pop();
		x = pop();
		rlineto(x,y);
		break;
	case HLINETO:
		x = pop();
		rlineto(x,0);
		break;
	case VLINETO:
		y = pop();
		rlineto(0,y);
		break;
	case RRCURVETO:
		dy3 = pop();
		dx3 = pop();
		dy2 = pop();
		dx2 = pop();
		dy1 = pop();
		dx1 = pop();
		rcurveto(dx1,dy1,dx1+dx2,dy1+dy2,dx1+dx2+dx3,dy1+dy2+dy3);
		break;
	case CLOSEPATH:
		closepath();
		break;
	case CALLSUBR:
		sub = pop();
		subpc = my_subrs[sub];
		if(!subpc) {
			fprintf(stderr,"\nYUCK no sub addr\n");
		}
		pushpc(pc);
		pc = subpc;
		break;
	case RETURN:
		pc = poppc();
		break;
	case HSBW:
		w = pop();
		x = pop();
		setwidth(w, x);
		sbpoint(x,0);
		break;
	case ENDCHAR:
		closepath();
		break;
	case RMOVETO:
		y = pop();
		x = pop();
		rmoveto(x,y);
		break;
	case HMOVETO:
		x = pop();
		rmoveto(x,0);
		break;
	case VHCURVETO:
		dy3 = 0;
		dx3 = pop();
		dy2 = pop();
		dx2 = pop();
		dy1 = pop();
		dx1 = 0;
		rcurveto(dx1,dy1,dx1+dx2,dy1+dy2,dx1+dx2+dx3,dy1+dy2+dy3);
		break;
	case HVCURVETO:
		dy3 = pop();
		dx3 = 0;
		dy2 = pop();
		dx2 = pop();
		dy1 = 0;
		dx1 = pop();
		rcurveto(dx1,dy1,dx1+dx2,dy1+dy2,dx1+dx2+dx3,dy1+dy2+dy3);
		break;
	case DOTSECTION:
		break;
	case VSTEM3:
		/*printf("vstem3\n");*/
		pop();
		pop();
		pop();
		pop();
		pop();
		pop();
		break;
	case HSTEM3:
		/*printf("hstem3\n");*/
		pop();
		pop();
		pop();
		pop();
		pop();
		pop();
		break;
	case SEAC:
		if (0) {
			printf("seac: %3d %3d %3d %3d %3d\n", pop(), pop(), pop(), pop(), pop());
		} else{
			c2 = STDtoISO(pop());	/* accent */
			c1 = STDtoISO(pop());	/* letter */

			cd = getchardesc(fnt, c1);
			if (cd) {
				memcpy(chardata, cd->data, cd->datalen);
				nshorts = cd->datalen / sizeof(short);
			}

			cd = getchardesc(fnt, c2);
			if (cd && cd->data && cd->datalen) {
				ndata = cd->data;

				if (nshorts) {
					if (savesplines) {
						switch (chardata[nshorts - 1]){
						case SP_RET:
							nshorts--;
							break;
						case SP_RETCLOSEPATH:
							chardata[nshorts - 1] = SP_CLOSEPATH;
							break;
						}
					} else {
						switch (chardata[nshorts - 1]){
						case PO_RET:
							printf("PO_RET in character disription ?\n");
							nshorts--;
							break;
						case PO_RETENDLOOP:
							if (ndata[0] == PO_BGNLOOP) {
								chardata[nshorts - 1] = PO_ENDBGNLOOP;
							} else {
								printf("new character doesn't start with PO_BGNLOOP ?\n");
							}
							break;
						}
					}
				}

				/* instead of the sidebearing[c1] maybe thesidebearing should be used */

				dy1 = pop();
				dx1 = pop() + sidebearing[c1] - sidebearing[c2];
				pop();

				fdx1 = dx1; 
				fdy1 = dy1;
				applymat(mat, &fdx1, &fdy1);
				dx1 = floor(fdx1); 
				dy1 = floor(fdy1);

				if (savesplines) {
					append_spline_offset(dx1, dy1, ndata);
				} else{
					append_poly_offset(dx1, dy1, ndata);
				}

				/*printf("first: %d %d\n", cd->data[0], cd->data[1]);*/
			}
			fflush(stdout);
		}
		break;
	case SBW:
		w = pop();
		y = pop();
		fprintf(stderr,"sbw: width: %d %d\n",w,y);
		y = pop();
		x = pop();
		fprintf(stderr,"sbw: side: %d %d\n",x,y);
		setwidth(w, x);
		sbpoint(x,y);
		break;
	case DIV:
		x = pop();
		y = pop();
		push(x/y);
		break;
	case CALLOTHERSUBR:
		sub = pop();
		n = pop();
		if(sub == 0)
			subr0();
		else if(sub == 1)
			subr1();
		else if(sub == 2)
			subr2();
		else {
			for(i=0; i<n; i++) {
				retpush(pop());
			}
		}
		break;
	case POP:
		push(retpop());
		break;
	case SETCURRENTPOINT:
		y = pop();
		x = pop();
		sbpoint(x,y);
		break;
	default:
		/*fprintf(stderr,"\nYUCK bad instruction %d\n",cmd);*/
		break;
	}
	if(pc == 0 || cmd == ENDCHAR || cmd == WHAT0 || cmd == SEAC)
		return 0;
	else
		return 1;
}


/*
 *	Character interpreter
 *
 */

static void runprog(void)
{
	int v, w, num, cmd;

	while(1) {
		v  = *pc++;
		if(v>=0 && v<=31) {
			if(v == 12) {
				w  = *pc++;
				cmd = 256+w;
			} else 
				cmd = v;
			if(!docommand(cmd)) {
				return;
			}
		} else if(v>=32 && v<=246) {
			num = v-139;
			push(num);
		} else if(v>=247 && v<=250) {
			w  = *pc++;
			num = (v-247)*256+w+108;
			push(num);
		} else if(v>=251 && v<=254) {
			w  = *pc++;
			num = -(v-251)*256-w-108;
			push(num);
		} else if(v == 255) {
			num  = *pc++;
			num <<= 8;
			num |= *pc++;
			num <<= 8;
			num |= *pc++;
			num <<= 8;
			num |= *pc++;
			push(num);
		}
	}
}

/***/

static VFontData *objfnt_to_vfontdata(objfnt *fnt)
{
	VFontData *vfd;
	chardesc *cd;
	short *_data, *data;
	int a, i, count, stop, ready, meet;
	short first[2]={0,0}, last[2]={0,0};
	struct Nurb *nu;
	struct BezTriple *bezt, *bez2;
	float scale, dx, dy;
	struct VChar *che;

	if (!fnt || (fnt->type!=SP_TYPE)) {
		return NULL;
	}

	vfd= MEM_callocN(sizeof(*vfd), "VFontData");
	scale = 10.0/(float)fnt->scale;	/* after IRIX 6.2, scaling went wrong */

	for (i = 0; i < MAX_VF_CHARS; i++) {
		cd = getchardesc(fnt, i);
		if (cd && cd->data && cd->datalen) {
			che = (VChar *) MEM_callocN(sizeof(VChar), "objfnt_char");
			BLI_addtail(&vfd->characters, che);
			che->index = i;
			che->width = scale * cd->movex;

			_data = data = cd->data;

			do{
				/* count first */
				_data = data;
				count = 0;
				ready = stop = 0;

				do{
					switch(*data++){
					case SP_MOVETO:
						first[0] = data[0];
						first[1] = data[1];
					case SP_LINETO:
						count++;
						last[0] = data[0];
						last[1] = data[1];
						data += 2;
						break;
					case SP_CURVETO:
						count++;
						last[0] = data[4];
						last[1] = data[5];
						data += 6;
						break;
					case SP_RET:
					case SP_RETCLOSEPATH:
						stop = 1;
						ready = 1;
						break;
					case SP_CLOSEPATH:
						stop = 1;
						break;
					}
				} while (!stop);

				if ((count>0) && last[0] == first[0] && last[1] == first[1]) meet = 1;
				else meet = 0;

				/* is there more than 1 unique point ?*/

				if (count - meet > 0) {
					data = _data;
					nu  =  (Nurb*)MEM_callocN(sizeof(struct Nurb),"objfnt_nurb");
					bezt = (BezTriple*)MEM_callocN((count)* sizeof(BezTriple),"objfnt_bezt") ;
					if (nu != 0 && bezt != 0) {
						BLI_addtail(&che->nurbsbase, nu);
						nu->type= CU_BEZIER+CU_2D;
						nu->pntsu = count;
						nu->resolu= 8;
						nu->flagu= 1;
						nu->bezt = bezt;
						stop = 0;

						/* read points */
						do {
							switch(*data++){
							case SP_MOVETO:
								bezt->vec[1][0] = scale * *data++;
								bezt->vec[1][1] = scale * *data++;
								
								break;
							case SP_LINETO:
								bez2 = bezt++;
								bezt->vec[1][0] = scale * *data++;
								bezt->vec[1][1] = scale * *data++;
								/* vector handles */
								bezt->h1= HD_VECT;
								bez2->h2= HD_VECT;
								dx = (bezt->vec[1][0] - bez2->vec[1][0]) / 3.0;
								dy = (bezt->vec[1][1] - bez2->vec[1][1]) / 3.0;
								bezt->vec[0][0] = bezt->vec[1][0] - dx;
								bezt->vec[0][1] = bezt->vec[1][1] - dy;
								bez2->vec[2][0] = bez2->vec[1][0] + dx;
								bez2->vec[2][1] = bez2->vec[1][1] + dy;
								break;
								
							case SP_CURVETO:
								bezt->vec[2][0] = scale * *data++;
								bezt->vec[2][1] = scale * *data++;
								bezt->h2= HD_ALIGN;
								bezt++;
								bezt->vec[0][0] = scale * *data++;
								bezt->vec[0][1] = scale * *data++;
								bezt->vec[1][0] = scale * *data++;
								bezt->vec[1][1] = scale * *data++;
								bezt->h1= HD_ALIGN;
								break;
								
							case SP_RET:
							case SP_RETCLOSEPATH:
								stop = 1;
								ready = 1;
								break;
							case SP_CLOSEPATH:
								stop = 1;
								break;
							}
						} while (stop == 0);

						if (meet) {
							/* copy handles */
							nu->bezt->vec[0][0] = bezt->vec[0][0];
							nu->bezt->vec[0][1] = bezt->vec[0][1];
							/* and forget last point */
							nu->pntsu--;
						}
						else {
							/* vector handles */
							bez2 = nu->bezt;
							dx = (bezt->vec[1][0] - bez2->vec[1][0]) / 3.0;
							dy = (bezt->vec[1][1] - bez2->vec[1][1]) / 3.0;
							bezt->vec[2][0] = bezt->vec[1][0] - dx;
							bezt->vec[2][1] = bezt->vec[1][1] - dy;
							bez2->vec[0][0] = bez2->vec[1][0] + dx;
							bez2->vec[0][1] = bez2->vec[1][1] + dy;
							bezt->h2= bez2->h1= HD_VECT;
						}
						
						/* forbidden handle combinations */
						a= nu->pntsu;
						bezt= nu->bezt;
						while(a--) {
							if(bezt->h1!=HD_ALIGN && bezt->h2==HD_ALIGN) bezt->h2= 0;
							else if(bezt->h2!=HD_ALIGN && bezt->h1==HD_ALIGN) bezt->h1= 0;
							bezt++;
						}
						
					}
					else {
						if (nu) MEM_freeN(nu);
						if (bezt) MEM_freeN(bezt);
					}
				}
				_data = data;
			} while (ready == 0);
		}
	}

	return vfd;
}

VFontData *BLI_vfontdata_from_psfont(PackedFile *pf)
{
	objfnt *fnt= objfnt_from_psfont(pf);
	VFontData *vfd= NULL;
	
	if (fnt) {
		vfd= objfnt_to_vfontdata(fnt);
		freeobjfnt(fnt);
	}
	
	return vfd;
}
