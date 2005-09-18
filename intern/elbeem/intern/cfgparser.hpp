/* A Bison parser, made by GNU Bison 1.875d.  */

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
     KW_P_RELAXTIME = 267,
     KW_P_REYNOLDS = 268,
     KW_P_VISCOSITY = 269,
     KW_P_SOUNDSPEED = 270,
     KW_P_DOMAINSIZE = 271,
     KW_P_FORCE = 272,
     KW_P_TIMELENGTH = 273,
     KW_P_STEPTIME = 274,
     KW_P_TIMEFACTOR = 275,
     KW_P_ANIFRAMETIME = 276,
     KW_P_ANISTART = 277,
     KW_P_SURFACETENSION = 278,
     KW_P_ACTIVATE = 279,
     KW_P_DEACTIVATE = 280,
     KW_P_DENSITY = 281,
     KW_P_CELLSIZE = 282,
     KW_P_GSTAR = 283,
     KW_PFSPATH = 284,
     KW_PARTLINELENGTH = 285,
     KW_PARTICLES = 286,
     KW_FRAMESPERSEC = 287,
     KW_RAYTRACING = 288,
     KW_PAROPEN = 289,
     KW_PARCLOSE = 290,
     KW_FILENAME = 291,
     KW_PMCAUSTICS = 292,
     KW_MAXRAYDEPTH = 293,
     KW_CAUSTICDIST = 294,
     KW_CAUSTICPHOT = 295,
     KW_SHADOWMAPBIAS = 296,
     KW_TREEMAXDEPTH = 297,
     KW_TREEMAXTRIANGLES = 298,
     KW_RESOLUTION = 299,
     KW_ANTIALIAS = 300,
     KW_EYEPOINT = 301,
     KW_ANISTART = 302,
     KW_ANIFRAMES = 303,
     KW_FRAMESKIP = 304,
     KW_LOOKAT = 305,
     KW_UPVEC = 306,
     KW_FOVY = 307,
     KW_ASPECT = 308,
     KW_AMBIENCE = 309,
     KW_BACKGROUND = 310,
     KW_DEBUGPIXEL = 311,
     KW_TESTMODE = 312,
     KW_OPENGLATTR = 313,
     KW_BLENDERATTR = 314,
     KW_ATTRIBUTE = 315,
     KW_OBJATTR = 316,
     KW_EQUALS = 317,
     KW_DEFINEATTR = 318,
     KW_ATTREND = 319,
     KW_GEOMETRY = 320,
     KW_TYPE = 321,
     KW_GEOTYPE_BOX = 322,
     KW_GEOTYPE_FLUID = 323,
     KW_GEOTYPE_OBJMODEL = 324,
     KW_GEOTYPE_SPHERE = 325,
     KW_CASTSHADOWS = 326,
     KW_RECEIVESHADOWS = 327,
     KW_VISIBLE = 328,
     KW_BOX_END = 329,
     KW_BOX_START = 330,
     KW_POLY = 331,
     KW_NUMVERTICES = 332,
     KW_VERTEX = 333,
     KW_NUMPOLYGONS = 334,
     KW_ISOSURF = 335,
     KW_FILEMODE = 336,
     KW_INVERT = 337,
     KW_MATERIAL = 338,
     KW_MATTYPE_PHONG = 339,
     KW_MATTYPE_BLINN = 340,
     KW_NAME = 341,
     KW_AMBIENT = 342,
     KW_DIFFUSE = 343,
     KW_SPECULAR = 344,
     KW_MIRROR = 345,
     KW_TRANSPARENCE = 346,
     KW_REFRACINDEX = 347,
     KW_TRANSADDITIVE = 348,
     KW_TRANSATTCOL = 349,
     KW_FRESNEL = 350,
     KW_LIGHT = 351,
     KW_ACTIVE = 352,
     KW_COLOUR = 353,
     KW_POSITION = 354,
     KW_LIGHT_OMNI = 355,
     KW_CAUSTICPHOTONS = 356,
     KW_CAUSTICSTRENGTH = 357,
     KW_SHADOWMAP = 358,
     KW_CAUSTICSMAP = 359
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
#define KW_P_RELAXTIME 267
#define KW_P_REYNOLDS 268
#define KW_P_VISCOSITY 269
#define KW_P_SOUNDSPEED 270
#define KW_P_DOMAINSIZE 271
#define KW_P_FORCE 272
#define KW_P_TIMELENGTH 273
#define KW_P_STEPTIME 274
#define KW_P_TIMEFACTOR 275
#define KW_P_ANIFRAMETIME 276
#define KW_P_ANISTART 277
#define KW_P_SURFACETENSION 278
#define KW_P_ACTIVATE 279
#define KW_P_DEACTIVATE 280
#define KW_P_DENSITY 281
#define KW_P_CELLSIZE 282
#define KW_P_GSTAR 283
#define KW_PFSPATH 284
#define KW_PARTLINELENGTH 285
#define KW_PARTICLES 286
#define KW_FRAMESPERSEC 287
#define KW_RAYTRACING 288
#define KW_PAROPEN 289
#define KW_PARCLOSE 290
#define KW_FILENAME 291
#define KW_PMCAUSTICS 292
#define KW_MAXRAYDEPTH 293
#define KW_CAUSTICDIST 294
#define KW_CAUSTICPHOT 295
#define KW_SHADOWMAPBIAS 296
#define KW_TREEMAXDEPTH 297
#define KW_TREEMAXTRIANGLES 298
#define KW_RESOLUTION 299
#define KW_ANTIALIAS 300
#define KW_EYEPOINT 301
#define KW_ANISTART 302
#define KW_ANIFRAMES 303
#define KW_FRAMESKIP 304
#define KW_LOOKAT 305
#define KW_UPVEC 306
#define KW_FOVY 307
#define KW_ASPECT 308
#define KW_AMBIENCE 309
#define KW_BACKGROUND 310
#define KW_DEBUGPIXEL 311
#define KW_TESTMODE 312
#define KW_OPENGLATTR 313
#define KW_BLENDERATTR 314
#define KW_ATTRIBUTE 315
#define KW_OBJATTR 316
#define KW_EQUALS 317
#define KW_DEFINEATTR 318
#define KW_ATTREND 319
#define KW_GEOMETRY 320
#define KW_TYPE 321
#define KW_GEOTYPE_BOX 322
#define KW_GEOTYPE_FLUID 323
#define KW_GEOTYPE_OBJMODEL 324
#define KW_GEOTYPE_SPHERE 325
#define KW_CASTSHADOWS 326
#define KW_RECEIVESHADOWS 327
#define KW_VISIBLE 328
#define KW_BOX_END 329
#define KW_BOX_START 330
#define KW_POLY 331
#define KW_NUMVERTICES 332
#define KW_VERTEX 333
#define KW_NUMPOLYGONS 334
#define KW_ISOSURF 335
#define KW_FILEMODE 336
#define KW_INVERT 337
#define KW_MATERIAL 338
#define KW_MATTYPE_PHONG 339
#define KW_MATTYPE_BLINN 340
#define KW_NAME 341
#define KW_AMBIENT 342
#define KW_DIFFUSE 343
#define KW_SPECULAR 344
#define KW_MIRROR 345
#define KW_TRANSPARENCE 346
#define KW_REFRACINDEX 347
#define KW_TRANSADDITIVE 348
#define KW_TRANSATTCOL 349
#define KW_FRESNEL 350
#define KW_LIGHT 351
#define KW_ACTIVE 352
#define KW_COLOUR 353
#define KW_POSITION 354
#define KW_LIGHT_OMNI 355
#define KW_CAUSTICPHOTONS 356
#define KW_CAUSTICSTRENGTH 357
#define KW_SHADOWMAP 358
#define KW_CAUSTICSMAP 359




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 85 "src/cfgparser.yy"
typedef union YYSTYPE {
  int    intValue;
  float  floatValue;
  char  *charValue;
} YYSTYPE;
/* Line 1285 of yacc.c.  */
#line 251 "bld-std-gcc/src/cfgparser.hpp"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yy_lval;



