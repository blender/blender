/* A Bison parser, made by GNU Bison 2.0.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     DT_INTEGER = 258,
     DT_FLOAT = 259,
     DT_STRING = 260,
     DT_ATTRNAME = 261,
     DT_ATTRVALUE = 262,
     KW_LBMSIM = 263,
     KW_COMPARELBM = 264,
     KW_ANIFRAMETIME = 265,
     KW_DEBUGMODE = 266,
     KW_DEBUGLEVEL = 267,
     KW_P_RELAXTIME = 268,
     KW_P_REYNOLDS = 269,
     KW_P_VISCOSITY = 270,
     KW_P_SOUNDSPEED = 271,
     KW_P_DOMAINSIZE = 272,
     KW_P_FORCE = 273,
     KW_P_TIMELENGTH = 274,
     KW_P_STEPTIME = 275,
     KW_P_TIMEFACTOR = 276,
     KW_P_ANIFRAMETIME = 277,
     KW_P_ANISTART = 278,
     KW_P_SURFACETENSION = 279,
     KW_P_ACTIVATE = 280,
     KW_P_DEACTIVATE = 281,
     KW_P_DENSITY = 282,
     KW_P_CELLSIZE = 283,
     KW_P_GSTAR = 284,
     KW_PFSPATH = 285,
     KW_PARTLINELENGTH = 286,
     KW_PARTICLES = 287,
     KW_FRAMESPERSEC = 288,
     KW_RAYTRACING = 289,
     KW_PAROPEN = 290,
     KW_PARCLOSE = 291,
     KW_FILENAME = 292,
     KW_PMCAUSTICS = 293,
     KW_MAXRAYDEPTH = 294,
     KW_CAUSTICDIST = 295,
     KW_CAUSTICPHOT = 296,
     KW_SHADOWMAPBIAS = 297,
     KW_TREEMAXDEPTH = 298,
     KW_TREEMAXTRIANGLES = 299,
     KW_RESOLUTION = 300,
     KW_ANTIALIAS = 301,
     KW_EYEPOINT = 302,
     KW_ANISTART = 303,
     KW_ANIFRAMES = 304,
     KW_FRAMESKIP = 305,
     KW_LOOKAT = 306,
     KW_UPVEC = 307,
     KW_FOVY = 308,
     KW_ASPECT = 309,
     KW_AMBIENCE = 310,
     KW_BACKGROUND = 311,
     KW_DEBUGPIXEL = 312,
     KW_TESTMODE = 313,
     KW_OPENGLATTR = 314,
     KW_BLENDERATTR = 315,
     KW_ATTRIBUTE = 316,
     KW_OBJATTR = 317,
     KW_EQUALS = 318,
     KW_DEFINEATTR = 319,
     KW_ATTREND = 320,
     KW_GEOMETRY = 321,
     KW_TYPE = 322,
     KW_GEOTYPE_BOX = 323,
     KW_GEOTYPE_FLUID = 324,
     KW_GEOTYPE_OBJMODEL = 325,
     KW_GEOTYPE_SPHERE = 326,
     KW_CASTSHADOWS = 327,
     KW_RECEIVESHADOWS = 328,
     KW_VISIBLE = 329,
     KW_BOX_END = 330,
     KW_BOX_START = 331,
     KW_POLY = 332,
     KW_NUMVERTICES = 333,
     KW_VERTEX = 334,
     KW_NUMPOLYGONS = 335,
     KW_ISOSURF = 336,
     KW_FILEMODE = 337,
     KW_INVERT = 338,
     KW_MATERIAL = 339,
     KW_MATTYPE_PHONG = 340,
     KW_MATTYPE_BLINN = 341,
     KW_NAME = 342,
     KW_AMBIENT = 343,
     KW_DIFFUSE = 344,
     KW_SPECULAR = 345,
     KW_MIRROR = 346,
     KW_TRANSPARENCE = 347,
     KW_REFRACINDEX = 348,
     KW_TRANSADDITIVE = 349,
     KW_TRANSATTCOL = 350,
     KW_FRESNEL = 351,
     KW_LIGHT = 352,
     KW_ACTIVE = 353,
     KW_COLOUR = 354,
     KW_POSITION = 355,
     KW_LIGHT_OMNI = 356,
     KW_CAUSTICPHOTONS = 357,
     KW_CAUSTICSTRENGTH = 358,
     KW_SHADOWMAP = 359,
     KW_CAUSTICSMAP = 360
   };
#endif
#define DT_INTEGER 258
#define DT_FLOAT 259
#define DT_STRING 260
#define DT_ATTRNAME 261
#define DT_ATTRVALUE 262
#define KW_LBMSIM 263
#define KW_COMPARELBM 264
#define KW_ANIFRAMETIME 265
#define KW_DEBUGMODE 266
#define KW_DEBUGLEVEL 267
#define KW_P_RELAXTIME 268
#define KW_P_REYNOLDS 269
#define KW_P_VISCOSITY 270
#define KW_P_SOUNDSPEED 271
#define KW_P_DOMAINSIZE 272
#define KW_P_FORCE 273
#define KW_P_TIMELENGTH 274
#define KW_P_STEPTIME 275
#define KW_P_TIMEFACTOR 276
#define KW_P_ANIFRAMETIME 277
#define KW_P_ANISTART 278
#define KW_P_SURFACETENSION 279
#define KW_P_ACTIVATE 280
#define KW_P_DEACTIVATE 281
#define KW_P_DENSITY 282
#define KW_P_CELLSIZE 283
#define KW_P_GSTAR 284
#define KW_PFSPATH 285
#define KW_PARTLINELENGTH 286
#define KW_PARTICLES 287
#define KW_FRAMESPERSEC 288
#define KW_RAYTRACING 289
#define KW_PAROPEN 290
#define KW_PARCLOSE 291
#define KW_FILENAME 292
#define KW_PMCAUSTICS 293
#define KW_MAXRAYDEPTH 294
#define KW_CAUSTICDIST 295
#define KW_CAUSTICPHOT 296
#define KW_SHADOWMAPBIAS 297
#define KW_TREEMAXDEPTH 298
#define KW_TREEMAXTRIANGLES 299
#define KW_RESOLUTION 300
#define KW_ANTIALIAS 301
#define KW_EYEPOINT 302
#define KW_ANISTART 303
#define KW_ANIFRAMES 304
#define KW_FRAMESKIP 305
#define KW_LOOKAT 306
#define KW_UPVEC 307
#define KW_FOVY 308
#define KW_ASPECT 309
#define KW_AMBIENCE 310
#define KW_BACKGROUND 311
#define KW_DEBUGPIXEL 312
#define KW_TESTMODE 313
#define KW_OPENGLATTR 314
#define KW_BLENDERATTR 315
#define KW_ATTRIBUTE 316
#define KW_OBJATTR 317
#define KW_EQUALS 318
#define KW_DEFINEATTR 319
#define KW_ATTREND 320
#define KW_GEOMETRY 321
#define KW_TYPE 322
#define KW_GEOTYPE_BOX 323
#define KW_GEOTYPE_FLUID 324
#define KW_GEOTYPE_OBJMODEL 325
#define KW_GEOTYPE_SPHERE 326
#define KW_CASTSHADOWS 327
#define KW_RECEIVESHADOWS 328
#define KW_VISIBLE 329
#define KW_BOX_END 330
#define KW_BOX_START 331
#define KW_POLY 332
#define KW_NUMVERTICES 333
#define KW_VERTEX 334
#define KW_NUMPOLYGONS 335
#define KW_ISOSURF 336
#define KW_FILEMODE 337
#define KW_INVERT 338
#define KW_MATERIAL 339
#define KW_MATTYPE_PHONG 340
#define KW_MATTYPE_BLINN 341
#define KW_NAME 342
#define KW_AMBIENT 343
#define KW_DIFFUSE 344
#define KW_SPECULAR 345
#define KW_MIRROR 346
#define KW_TRANSPARENCE 347
#define KW_REFRACINDEX 348
#define KW_TRANSADDITIVE 349
#define KW_TRANSATTCOL 350
#define KW_FRESNEL 351
#define KW_LIGHT 352
#define KW_ACTIVE 353
#define KW_COLOUR 354
#define KW_POSITION 355
#define KW_LIGHT_OMNI 356
#define KW_CAUSTICPHOTONS 357
#define KW_CAUSTICSTRENGTH 358
#define KW_SHADOWMAP 359
#define KW_CAUSTICSMAP 360




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 87 "src/cfgparser.yy"
typedef union YYSTYPE {
  int    intValue;
  float  floatValue;
  char  *charValue;
} YYSTYPE;
/* Line 1318 of yacc.c.  */
#line 253 "bld-std-gcc40/src/cfgparser.hpp"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yy_lval;



