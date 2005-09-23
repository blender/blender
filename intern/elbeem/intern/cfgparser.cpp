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

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0

/* If NAME_PREFIX is specified substitute the variables and functions
   names.  */
#define yyparse yy_parse
#define yylex   yy_lex
#define yyerror yy_error
#define yylval  yy_lval
#define yychar  yy_char
#define yydebug yy_debug
#define yynerrs yy_nerrs


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




/* Copy the first part of user declarations.  */
#line 14 "src/cfgparser.yy"


#define YYDEBUG 1

/* library functions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "attributes.h"

	void yy_warn(char *s);
	void yy_error(const char *s);

  /* imported from flex... */
  extern int yy_lex();
  extern int lineCount;
  extern FILE *yy_in;

  /* the parse function from bison */
  int yy_parse( void );

// local variables to access objects 
#include "simulation_object.h"
#ifdef LBM_INCLUDE_TESTSOLVERS
#include "simulation_complbm.h"
#endif // LBM_INCLUDE_TESTSOLVERS

#include "parametrizer.h"
#include "ntl_renderglobals.h"
#include "ntl_scene.h"

#include "ntl_lightobject.h"
#include "ntl_material.h"
#include "ntl_geometrybox.h"
#include "ntl_geometrysphere.h"
#include "ntl_geometrymodel.h"
#include "globals.h"
	
	/* global variables */
	static map<string,AttributeList*> attrs; /* global attribute storage */
	vector<string> 			currentAttrValue;    /* build string vector */
	
	// global raytracing settings, stores object,lights,material lists etc.
	// lists are freed by ntlScene upon deletion
  static ntlRenderGlobals *reglob;	/* raytracing global settings */

	/* light initialization checks */
	ntlLightObject      *currentLight;
	ntlLightObject      *currentLightOmni;

	/* geometry initialization checks */
	ntlGeometryClass    *currentGeoClass;
	ntlGeometryObject   *currentGeoObj;
	ntlGeometryBox      *currentGeometryBox;
	ntlGeometrySphere   *currentGeometrySphere;
	SimulationObject    *currentGeometrySim;
	ntlGeometryObjModel *currentGeometryModel;
	AttributeList				*currentAttrib;
	string							currentAttrName, currentAttribAddName;

	/* material init checks */
	ntlMaterial  				*currentMaterial;



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 85 "src/cfgparser.yy"
typedef union YYSTYPE {
  int    intValue;
  float  floatValue;
  char  *charValue;
} YYSTYPE;
/* Line 191 of yacc.c.  */
#line 364 "bld-std-gcc40/src/cfgparser.cpp"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 376 "bld-std-gcc40/src/cfgparser.cpp"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

# ifndef YYFREE
#  define YYFREE free
# endif
# ifndef YYMALLOC
#  define YYMALLOC malloc
# endif

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   define YYSTACK_ALLOC alloca
#  endif
# else
#  if defined (alloca) || defined (_ALLOCA_H)
#   define YYSTACK_ALLOC alloca
#  else
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE))			\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  12
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   278

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  105
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  77
/* YYNRULES -- Number of rules. */
#define YYNRULES  136
/* YYNRULES -- Number of states. */
#define YYNSTATES  237

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   359

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     6,     8,    10,    12,    14,    17,    22,
      25,    27,    29,    31,    33,    35,    37,    39,    41,    43,
      45,    47,    49,    51,    53,    55,    57,    59,    61,    63,
      65,    67,    69,    71,    73,    75,    77,    80,    83,    86,
      89,    93,    96,   101,   106,   111,   114,   117,   122,   127,
     130,   133,   136,   139,   143,   146,   149,   152,   159,   162,
     164,   166,   168,   170,   172,   174,   177,   180,   185,   190,
     191,   199,   202,   204,   206,   208,   210,   212,   214,   216,
     218,   220,   222,   224,   226,   228,   230,   232,   235,   238,
     241,   244,   247,   252,   257,   260,   265,   272,   275,   277,
     279,   281,   283,   285,   287,   289,   291,   293,   295,   297,
     299,   301,   304,   309,   314,   318,   321,   324,   327,   330,
     335,   338,   339,   346,   349,   351,   352,   353,   360,   363,
     365,   367,   369,   371,   373,   375,   377
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     106,     0,    -1,   106,   107,    -1,   107,    -1,   109,    -1,
     169,    -1,   108,    -1,    11,     3,    -1,    33,    34,   110,
      35,    -1,   110,   111,    -1,   111,    -1,   114,    -1,   112,
      -1,   113,    -1,   115,    -1,   116,    -1,   117,    -1,   118,
      -1,   119,    -1,   120,    -1,   121,    -1,   122,    -1,   123,
      -1,   124,    -1,   125,    -1,   126,    -1,   127,    -1,   128,
      -1,   129,    -1,   130,    -1,   131,    -1,   132,    -1,   133,
      -1,   141,    -1,   172,    -1,   155,    -1,    47,   180,    -1,
      10,   180,    -1,    48,   179,    -1,    49,   181,    -1,    44,
     179,   179,    -1,    45,     3,    -1,    46,   178,   178,   178,
      -1,    50,   178,   178,   178,    -1,    51,   178,   178,   178,
      -1,    52,   178,    -1,    53,   178,    -1,    54,   178,   178,
     178,    -1,    55,   178,   178,   178,    -1,    36,     5,    -1,
      42,   179,    -1,    43,   179,    -1,    38,   179,    -1,    56,
       3,     3,    -1,    57,   181,    -1,    58,     5,    -1,    59,
       5,    -1,    96,    34,    66,   135,   134,    35,    -1,   134,
     136,    -1,   136,    -1,   100,    -1,   137,    -1,   138,    -1,
     139,    -1,   140,    -1,    97,   181,    -1,    71,   181,    -1,
      98,   178,   178,   178,    -1,    99,   178,   178,   178,    -1,
      -1,    65,    34,    66,   144,   142,   143,    35,    -1,   143,
     145,    -1,   145,    -1,    67,    -1,    70,    -1,    69,    -1,
       8,    -1,     9,    -1,   146,    -1,   147,    -1,   148,    -1,
     149,    -1,   150,    -1,   151,    -1,   152,    -1,   153,    -1,
     154,    -1,    86,     5,    -1,    83,     5,    -1,    71,   181,
      -1,    72,   181,    -1,    73,   181,    -1,    75,   178,   178,
     178,    -1,    74,   178,   178,   178,    -1,    61,     5,    -1,
      63,    34,   171,    35,    -1,    83,    34,    66,   157,   156,
      35,    -1,   156,   158,    -1,   158,    -1,    84,    -1,    85,
      -1,   159,    -1,   160,    -1,   161,    -1,   162,    -1,   163,
      -1,   165,    -1,   164,    -1,   166,    -1,   167,    -1,   168,
      -1,    86,     5,    -1,    87,   176,   176,   176,    -1,    88,
     176,   176,   176,    -1,    89,   178,   178,    -1,    90,   177,
      -1,    91,   177,    -1,    92,   178,    -1,    93,   177,    -1,
      94,   178,   178,   178,    -1,    95,   180,    -1,    -1,    60,
       6,    34,   170,   171,    35,    -1,   171,   172,    -1,   172,
      -1,    -1,    -1,     6,    62,   173,   175,   174,    64,    -1,
     175,     7,    -1,     7,    -1,   177,    -1,   178,    -1,     4,
      -1,     3,    -1,     3,    -1,     3,    -1,     3,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   144,   144,   145,   148,   149,   150,   154,   164,   165,
     165,   168,   169,   170,   171,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   191,   192,   193,   194,   199,   203,   208,   212,
     218,   222,   226,   230,   234,   238,   242,   246,   250,   254,
     258,   262,   266,   270,   274,   278,   283,   292,   303,   304,
     307,   315,   316,   317,   318,   322,   327,   332,   337,   355,
     354,   374,   375,   378,   383,   388,   393,   399,   411,   412,
     413,   414,   415,   416,   417,   418,   419,   424,   429,   435,
     441,   447,   452,   464,   477,   483,   493,   504,   505,   508,
     513,   517,   518,   519,   520,   521,   522,   523,   524,   525,
     526,   531,   536,   541,   546,   552,   557,   562,   567,   572,
     577,   588,   588,   598,   598,   601,   602,   601,   609,   612,
     622,   625,   637,   639,   645,   656,   668
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DT_INTEGER", "DT_FLOAT", "DT_STRING",
  "DT_ATTRNAME", "DT_ATTRVALUE", "KW_LBMSIM", "KW_COMPARELBM",
  "KW_ANIFRAMETIME", "KW_DEBUGMODE", "KW_P_RELAXTIME", "KW_P_REYNOLDS",
  "KW_P_VISCOSITY", "KW_P_SOUNDSPEED", "KW_P_DOMAINSIZE", "KW_P_FORCE",
  "KW_P_TIMELENGTH", "KW_P_STEPTIME", "KW_P_TIMEFACTOR",
  "KW_P_ANIFRAMETIME", "KW_P_ANISTART", "KW_P_SURFACETENSION",
  "KW_P_ACTIVATE", "KW_P_DEACTIVATE", "KW_P_DENSITY", "KW_P_CELLSIZE",
  "KW_P_GSTAR", "KW_PFSPATH", "KW_PARTLINELENGTH", "KW_PARTICLES",
  "KW_FRAMESPERSEC", "KW_RAYTRACING", "KW_PAROPEN", "KW_PARCLOSE",
  "KW_FILENAME", "KW_PMCAUSTICS", "KW_MAXRAYDEPTH", "KW_CAUSTICDIST",
  "KW_CAUSTICPHOT", "KW_SHADOWMAPBIAS", "KW_TREEMAXDEPTH",
  "KW_TREEMAXTRIANGLES", "KW_RESOLUTION", "KW_ANTIALIAS", "KW_EYEPOINT",
  "KW_ANISTART", "KW_ANIFRAMES", "KW_FRAMESKIP", "KW_LOOKAT", "KW_UPVEC",
  "KW_FOVY", "KW_ASPECT", "KW_AMBIENCE", "KW_BACKGROUND", "KW_DEBUGPIXEL",
  "KW_TESTMODE", "KW_OPENGLATTR", "KW_BLENDERATTR", "KW_ATTRIBUTE",
  "KW_OBJATTR", "KW_EQUALS", "KW_DEFINEATTR", "KW_ATTREND", "KW_GEOMETRY",
  "KW_TYPE", "KW_GEOTYPE_BOX", "KW_GEOTYPE_FLUID", "KW_GEOTYPE_OBJMODEL",
  "KW_GEOTYPE_SPHERE", "KW_CASTSHADOWS", "KW_RECEIVESHADOWS", "KW_VISIBLE",
  "KW_BOX_END", "KW_BOX_START", "KW_POLY", "KW_NUMVERTICES", "KW_VERTEX",
  "KW_NUMPOLYGONS", "KW_ISOSURF", "KW_FILEMODE", "KW_INVERT",
  "KW_MATERIAL", "KW_MATTYPE_PHONG", "KW_MATTYPE_BLINN", "KW_NAME",
  "KW_AMBIENT", "KW_DIFFUSE", "KW_SPECULAR", "KW_MIRROR",
  "KW_TRANSPARENCE", "KW_REFRACINDEX", "KW_TRANSADDITIVE",
  "KW_TRANSATTCOL", "KW_FRESNEL", "KW_LIGHT", "KW_ACTIVE", "KW_COLOUR",
  "KW_POSITION", "KW_LIGHT_OMNI", "KW_CAUSTICPHOTONS",
  "KW_CAUSTICSTRENGTH", "KW_SHADOWMAP", "KW_CAUSTICSMAP", "$accept",
  "desc_line", "desc_expression", "toggledebug_expression",
  "raytrace_section", "raytrace_line", "raytrace_expression",
  "anistart_expression", "aniframetime_expression", "aniframes_expression",
  "frameskip_expression", "resolution_expression", "antialias_expression",
  "eyepoint_expression", "lookat_expression", "upvec_expression",
  "fovy_expression", "aspect_expression", "ambience_expression",
  "background_expression", "filename_expression",
  "treemaxdepth_expression", "treemaxtriangles_expression",
  "maxraydepth_expression", "debugpixel_expression", "testmode_expression",
  "openglattr_expr", "blenderattr_expr", "light_expression",
  "lightsettings_line", "lighttype_expression", "lightsettings_expression",
  "lightactive_expression", "lightcastshadows_expression",
  "lightcolor_expression", "lightposition_expression",
  "geometry_expression", "@1", "geometrysettings_line",
  "geometrytype_expression", "geometrysettings_expression",
  "geometryexpression_name", "geometryexpression_propname",
  "geometryexpression_castshadows", "geometryexpression_recshadows",
  "geometryexpression_visible", "geometryexpression_boxstart",
  "geometryexpression_boxend", "geometryexpression_attrib",
  "geometryexpression_defattrib", "material_expression",
  "materialsettings_line", "materialtype_expression",
  "materialsettings_expression", "materialexpression_name",
  "materialexpression_ambient", "materialexpression_diffuse",
  "materialexpression_specular", "materialexpression_mirror",
  "materialexpression_transparence", "materialexpression_refracindex",
  "materialexpression_transadd", "materialexpression_transattcol",
  "materialexpression_fresnel", "attribute_section", "@2",
  "attribute_line", "attribute_expression", "@3", "@4", "attrvalue_list",
  "DT_COLOR", "DT_ZEROTOONE", "DT_REALVAL", "DT_INTLTZERO", "DT_POSINT",
  "DT_BOOLEAN", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,   105,   106,   106,   107,   107,   107,   108,   109,   110,
     110,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   134,
     135,   136,   136,   136,   136,   137,   138,   139,   140,   142,
     141,   143,   143,   144,   144,   144,   144,   144,   145,   145,
     145,   145,   145,   145,   145,   145,   145,   146,   147,   148,
     149,   150,   151,   152,   153,   154,   155,   156,   156,   157,
     157,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   170,   169,   171,   171,   173,   174,   172,   175,   175,
     176,   177,   178,   178,   179,   180,   181
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     2,     1,     1,     1,     1,     2,     4,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     2,     2,
       3,     2,     4,     4,     4,     2,     2,     4,     4,     2,
       2,     2,     2,     3,     2,     2,     2,     6,     2,     1,
       1,     1,     1,     1,     1,     2,     2,     4,     4,     0,
       7,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     2,
       2,     2,     4,     4,     2,     4,     6,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     4,     4,     3,     2,     2,     2,     2,     4,
       2,     0,     6,     2,     1,     0,     0,     6,     2,     1,
       1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       0,     0,     0,     0,     0,     3,     6,     4,     5,     7,
       0,     0,     1,     2,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      10,    12,    13,    11,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    35,    34,   121,   125,   135,    37,
      49,   134,    52,    50,    51,     0,    41,   133,   132,     0,
      36,    38,   136,    39,     0,     0,    45,    46,     0,     0,
       0,    54,    55,    56,     0,     0,     0,     8,     9,     0,
       0,    40,     0,     0,     0,     0,     0,    53,     0,     0,
       0,     0,   124,   129,   126,    42,    43,    44,    47,    48,
      76,    77,    73,    75,    74,    69,    99,   100,     0,    60,
       0,   122,   123,   128,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    98,   101,   102,
     103,   104,   105,   107,   106,   108,   109,   110,     0,     0,
       0,     0,     0,    59,    61,    62,    63,    64,   127,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    72,
      78,    79,    80,    81,    82,    83,    84,    85,    86,   111,
       0,   130,   131,     0,     0,   115,   116,   117,   118,     0,
     120,    96,    97,    66,    65,     0,     0,    57,    58,    94,
       0,    89,    90,    91,     0,     0,    88,    87,    70,    71,
       0,     0,   114,     0,     0,     0,     0,     0,     0,   112,
     113,   119,    67,    68,    95,    93,    92
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,     4,     5,     6,     7,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,   162,
     130,   163,   164,   165,   166,   167,    63,   135,   178,   125,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
      64,   146,   128,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,     8,    99,   111,    65,   100,   134,
     114,   190,   191,   192,    72,    69,    83
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -111
static const short int yypact[] =
{
      31,    10,   -18,    22,    12,  -111,  -111,  -111,  -111,  -111,
     172,    -2,  -111,  -111,   -26,    41,    45,    43,    43,    43,
      43,    48,    21,    41,    43,    51,    21,    21,    21,    21,
      21,    21,    55,    51,    54,    60,    19,    34,    35,    50,
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,    43,  -111,  -111,  -111,    21,
    -111,  -111,  -111,  -111,    21,    21,  -111,  -111,    21,    21,
      67,  -111,  -111,  -111,   -11,     5,    23,  -111,  -111,    81,
      83,  -111,    21,    21,    21,    21,    21,  -111,     9,   -50,
      11,     8,  -111,  -111,   105,  -111,  -111,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,   183,  -111,
      52,  -111,  -111,  -111,    57,   171,   113,    21,    21,    21,
      21,    21,    21,    21,    21,    41,    75,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,    51,    51,
      21,    21,   -24,  -111,  -111,  -111,  -111,  -111,  -111,   114,
      90,    51,    51,    51,    21,    21,   120,   121,   -34,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,
      21,  -111,  -111,    21,    21,  -111,  -111,  -111,  -111,    21,
    -111,  -111,  -111,  -111,  -111,    21,    21,  -111,  -111,  -111,
      81,  -111,  -111,  -111,    21,    21,  -111,  -111,  -111,  -111,
      21,    21,  -111,    21,    21,    21,    13,    21,    21,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,  -111
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
    -111,  -111,   125,  -111,  -111,  -111,    92,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,
    -111,   -28,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,
     -43,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,
    -111,  -111,  -111,   -10,  -111,  -111,  -111,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,   -73,   -96,  -111,  -111,
    -111,   -77,  -110,   -22,     2,   -13,   -31
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
      79,   218,    91,   112,    84,    85,    86,    87,    88,    89,
      80,   207,    12,     9,    14,   132,    10,   120,   121,    14,
      73,    74,    75,     1,    77,    78,    81,   169,    11,   170,
     195,   196,    66,   198,   126,   127,    67,   171,   172,   173,
     174,   175,     1,   131,    68,     2,    71,   158,   234,   176,
      70,    76,   177,    94,    82,   108,    14,   102,    90,    92,
      15,   193,   103,   104,     2,    93,   105,   106,    95,    96,
     107,   109,     3,   159,   160,   161,   122,   101,   123,   124,
     115,   116,   117,   118,   119,    97,    16,    14,    17,   110,
     113,     3,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
     201,   129,   133,   220,   112,    36,   221,   194,   189,   209,
     197,   168,   199,   158,   210,   216,   217,   203,   204,    13,
     132,    98,   200,    37,   208,   219,   202,   226,   205,   206,
     211,   212,   213,   229,   230,     0,    38,     0,     0,   159,
     160,   161,   214,   215,     0,     0,     0,     0,     0,     0,
       0,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,     0,   222,     0,     0,     0,     0,   223,    14,     0,
       0,     0,    15,   224,   225,     0,     0,     0,     0,     0,
       0,     0,   227,   228,     0,     0,     0,     0,     0,     0,
       0,   231,   232,   233,     0,   235,   236,     0,    16,     0,
      17,     0,     0,     0,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,   169,     0,   170,     0,     0,    36,     0,     0,
       0,     0,   171,   172,   173,   174,   175,     0,     0,     0,
       0,     0,     0,     0,   176,    37,     0,   177,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    38,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145
};

static const short int yycheck[] =
{
      22,    35,    33,    99,    26,    27,    28,    29,    30,    31,
      23,    35,     0,     3,     6,   111,    34,     8,     9,     6,
      18,    19,    20,    11,     3,     4,    24,    61,     6,    63,
     140,   141,    34,   143,    84,    85,    62,    71,    72,    73,
      74,    75,    11,    35,     3,    33,     3,    71,    35,    83,
       5,     3,    86,    34,     3,    66,     6,    79,     3,     5,
      10,   138,    84,    85,    33,     5,    88,    89,    34,    34,
       3,    66,    60,    97,    98,    99,    67,    75,    69,    70,
     102,   103,   104,   105,   106,    35,    36,     6,    38,    66,
       7,    60,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      35,   100,     7,   190,   210,    65,   193,   139,     5,     5,
     142,    64,   144,    71,    34,     5,     5,   158,   159,     4,
     226,    39,   145,    83,   162,   178,   146,   210,   160,   161,
     171,   172,   173,   220,   221,    -1,    96,    -1,    -1,    97,
      98,    99,   174,   175,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    -1,   194,    -1,    -1,    -1,    -1,   199,     6,    -1,
      -1,    -1,    10,   205,   206,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   214,   215,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   223,   224,   225,    -1,   227,   228,    -1,    36,    -1,
      38,    -1,    -1,    -1,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    61,    -1,    63,    -1,    -1,    65,    -1,    -1,
      -1,    -1,    71,    72,    73,    74,    75,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    83,    83,    -1,    86,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    96,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    11,    33,    60,   106,   107,   108,   109,   169,     3,
      34,     6,     0,   107,     6,    10,    36,    38,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    65,    83,    96,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   141,   155,   172,    34,    62,     3,   180,
       5,     3,   179,   179,   179,   179,     3,     3,     4,   178,
     180,   179,     3,   181,   178,   178,   178,   178,   178,   178,
       3,   181,     5,     5,    34,    34,    34,    35,   111,   170,
     173,   179,   178,   178,   178,   178,   178,     3,    66,    66,
      66,   171,   172,     7,   175,   178,   178,   178,   178,   178,
       8,     9,    67,    69,    70,   144,    84,    85,   157,   100,
     135,    35,   172,     7,   174,   142,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,   156,   158,   159,   160,
     161,   162,   163,   164,   165,   166,   167,   168,    71,    97,
      98,    99,   134,   136,   137,   138,   139,   140,    64,    61,
      63,    71,    72,    73,    74,    75,    83,    86,   143,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,     5,
     176,   177,   178,   176,   178,   177,   177,   178,   177,   178,
     180,    35,   158,   181,   181,   178,   178,    35,   136,     5,
      34,   181,   181,   181,   178,   178,     5,     5,    35,   145,
     176,   176,   178,   178,   178,   178,   171,   178,   178,   176,
     176,   178,   178,   178,    35,   178,   178
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)		\
   ((Current).first_line   = (Rhs)[1].first_line,	\
    (Current).first_column = (Rhs)[1].first_column,	\
    (Current).last_line    = (Rhs)[N].last_line,	\
    (Current).last_column  = (Rhs)[N].last_column)
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)

# define YYDSYMPRINTF(Title, Token, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Token, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
# define YYDSYMPRINTF(Title, Token, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if defined (YYMAXDEPTH) && YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
    }
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yytype, yyvaluep)
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  register short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;


  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short int *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YYDSYMPRINTF ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %s, ", yytname[yytoken]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 7:
#line 154 "src/cfgparser.yy"
    { yy_debug = yyvsp[0].intValue; }
    break;

  case 36:
#line 200 "src/cfgparser.yy"
    { reglob->setAniStart( yyvsp[0].intValue ); }
    break;

  case 37:
#line 204 "src/cfgparser.yy"
    { /*reglob->setAniFrameTime( $2 );*/ debMsgStd("cfgparser",DM_NOTIFY,"Deprecated setting aniframetime!",1); }
    break;

  case 38:
#line 209 "src/cfgparser.yy"
    { reglob->setAniFrames( (yyvsp[0].intValue)-1 ); }
    break;

  case 39:
#line 213 "src/cfgparser.yy"
    { reglob->setFrameSkip( (yyvsp[0].intValue) ); }
    break;

  case 40:
#line 219 "src/cfgparser.yy"
    { reglob->setResX( yyvsp[-1].intValue ); reglob->setResY( yyvsp[0].intValue); }
    break;

  case 41:
#line 223 "src/cfgparser.yy"
    { reglob->setAADepth( yyvsp[0].intValue ); }
    break;

  case 42:
#line 227 "src/cfgparser.yy"
    { reglob->setEye( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); }
    break;

  case 43:
#line 231 "src/cfgparser.yy"
    { reglob->setLookat( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); }
    break;

  case 44:
#line 235 "src/cfgparser.yy"
    { reglob->setUpVec( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); }
    break;

  case 45:
#line 239 "src/cfgparser.yy"
    { reglob->setFovy( yyvsp[0].floatValue ); }
    break;

  case 46:
#line 243 "src/cfgparser.yy"
    { reglob->setAspect( yyvsp[0].floatValue ); }
    break;

  case 47:
#line 247 "src/cfgparser.yy"
    { reglob->setAmbientLight( ntlColor(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue)  ); }
    break;

  case 48:
#line 251 "src/cfgparser.yy"
    { reglob->setBackgroundCol( ntlColor(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); }
    break;

  case 49:
#line 255 "src/cfgparser.yy"
    { reglob->setOutFilename( yyvsp[0].charValue ); }
    break;

  case 50:
#line 259 "src/cfgparser.yy"
    { reglob->setTreeMaxDepth( yyvsp[0].intValue ); }
    break;

  case 51:
#line 263 "src/cfgparser.yy"
    { reglob->setTreeMaxTriangles( yyvsp[0].intValue ); }
    break;

  case 52:
#line 267 "src/cfgparser.yy"
    { reglob->setRayMaxDepth( yyvsp[0].intValue ); }
    break;

  case 53:
#line 271 "src/cfgparser.yy"
    { reglob->setDebugPixel( yyvsp[-1].intValue, yyvsp[0].intValue ); }
    break;

  case 54:
#line 275 "src/cfgparser.yy"
    { reglob->setTestMode( yyvsp[0].intValue ); }
    break;

  case 55:
#line 279 "src/cfgparser.yy"
    { if(attrs[yyvsp[0].charValue] == NULL){ yyerror("OPENGL ATTRIBUTES: The attribute was not found!"); }
			reglob->getOpenGlAttributes()->import( attrs[yyvsp[0].charValue] ); }
    break;

  case 56:
#line 284 "src/cfgparser.yy"
    { if(attrs[yyvsp[0].charValue] == NULL){ yyerror("BLENDER ATTRIBUTES: The attribute was not found!"); }
			reglob->getBlenderAttributes()->import( attrs[yyvsp[0].charValue] ); }
    break;

  case 57:
#line 296 "src/cfgparser.yy"
    { 
				/* reset light pointers */
				currentLightOmni = NULL; 
			}
    break;

  case 60:
#line 308 "src/cfgparser.yy"
    { currentLightOmni = new ntlLightObject( reglob );
		  currentLight = currentLightOmni;
			reglob->getLightList()->push_back(currentLight);
		}
    break;

  case 65:
#line 322 "src/cfgparser.yy"
    { 
			currentLight->setActive( yyvsp[0].intValue ); 
		}
    break;

  case 66:
#line 327 "src/cfgparser.yy"
    { 
			currentLight->setCastShadows( yyvsp[0].intValue ); 
		}
    break;

  case 67:
#line 332 "src/cfgparser.yy"
    { 
			currentLight->setColor( ntlColor(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); 
		}
    break;

  case 68:
#line 337 "src/cfgparser.yy"
    { 
		int init = 0;
		if(currentLightOmni != NULL) {
			currentLightOmni->setPosition( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); init = 1; }
		if(!init) yyerror("This property can only be set for omni-directional and rectangular lights!");
	}
    break;

  case 69:
#line 355 "src/cfgparser.yy"
    {
				// geo classes have attributes...
				reglob->getScene()->addGeoClass(currentGeoClass);
				currentAttrib = currentGeoClass->getAttributeList();
			}
    break;

  case 70:
#line 361 "src/cfgparser.yy"
    { 
				/* reset geometry object pointers */
				currentGeoObj			 = NULL; 
				currentGeoClass		 = NULL; 
				currentGeometryBox = NULL; 
				currentGeometrySim = NULL; 
				currentGeometryModel = NULL; 
				currentGeometrySphere = NULL; 
				currentAttrib = NULL;
			}
    break;

  case 73:
#line 378 "src/cfgparser.yy"
    { 
			currentGeometryBox = new ntlGeometryBox( );
			currentGeoClass = currentGeometryBox;
			currentGeoObj = (ntlGeometryObject*)( currentGeometryBox );
		}
    break;

  case 74:
#line 383 "src/cfgparser.yy"
    { 
			currentGeometrySphere = new ntlGeometrySphere( );
			currentGeoClass = currentGeometrySphere;
			currentGeoObj = (ntlGeometryObject*)( currentGeometrySphere );
		}
    break;

  case 75:
#line 388 "src/cfgparser.yy"
    { 
			currentGeometryModel = new ntlGeometryObjModel( );
			currentGeoClass = currentGeometryModel;
			currentGeoObj = (ntlGeometryObject*)( currentGeometryModel );
		}
    break;

  case 76:
#line 393 "src/cfgparser.yy"
    { 
			currentGeometrySim = new SimulationObject();
			currentGeoClass = currentGeometrySim;
			reglob->getSims()->push_back(currentGeometrySim);
			// dont add mcubes to geo list!
		}
    break;

  case 77:
#line 399 "src/cfgparser.yy"
    { 
#ifdef LBM_INCLUDE_TESTSOLVERS
			currentGeometrySim = new SimulationCompareLbm();
			currentGeoClass = currentGeometrySim;
			reglob->getSims()->push_back(currentGeometrySim);
#else // LBM_INCLUDE_TESTSOLVERS
			errMsg("El'Beem::cfg","compare test solver not supported!");
#endif // LBM_INCLUDE_TESTSOLVERS
		}
    break;

  case 87:
#line 424 "src/cfgparser.yy"
    { 
			currentGeoClass->setName( yyvsp[0].charValue ); 
		}
    break;

  case 88:
#line 429 "src/cfgparser.yy"
    { 
			if(currentGeoObj == NULL){ yyerror(" MATERIAL : This property can only be set for geometry objects!"); }
			currentGeoObj->setMaterialName( yyvsp[0].charValue ); 
		}
    break;

  case 89:
#line 435 "src/cfgparser.yy"
    { 
			if(currentGeoObj == NULL){ yyerror(" CAST_SHADOW : This property can only be set for geometry objects!"); }
			currentGeoObj->setCastShadows( yyvsp[0].intValue ); 
		}
    break;

  case 90:
#line 441 "src/cfgparser.yy"
    { 
			if(currentGeoObj == NULL){ yyerror(" RECEIVE_SHADOW : This property can only be set for geometry objects!"); }
			currentGeoObj->setReceiveShadows( yyvsp[0].intValue ); 
		}
    break;

  case 91:
#line 447 "src/cfgparser.yy"
    { 
			currentGeoClass->setVisible( yyvsp[0].intValue ); 
		}
    break;

  case 92:
#line 452 "src/cfgparser.yy"
    { 
		int init = 0;
		if(currentGeometryBox != NULL){ 
			currentGeometryBox->setStart( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); init=1; }
		if(currentGeometrySim != NULL){ 
			currentGeometrySim->setGeoStart( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); init=1; }
		if(currentGeometryModel != NULL){ 
			currentGeometryModel->setStart( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); init=1; }
		if(!init ){ yyerror("BOXSTART : This property can only be set for box, objmodel, fluid and isosurface objects!"); }
	}
    break;

  case 93:
#line 464 "src/cfgparser.yy"
    { 
		int init = 0;
		if(currentGeometryBox != NULL){ 
			currentGeometryBox->setEnd( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); init=1; }
		if(currentGeometrySim != NULL){ 
			currentGeometrySim->setGeoEnd( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); init=1; }
		if(currentGeometryModel != NULL){ 
			currentGeometryModel->setEnd( ntlVec3Gfx(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); init=1; }
		if(!init ){ yyerror("BOXEND : This property can only be set for box, objmodel, fluid and isosurface objects!"); }
	}
    break;

  case 94:
#line 477 "src/cfgparser.yy"
    { 
			if(attrs[yyvsp[0].charValue] == NULL){ yyerror("GEO ATTRIBUTES: The attribute was not found!"); }
			currentGeoClass->getAttributeList()->import( attrs[yyvsp[0].charValue] ); 
		}
    break;

  case 95:
#line 485 "src/cfgparser.yy"
    { }
    break;

  case 96:
#line 497 "src/cfgparser.yy"
    { 
				/* reset geometry object pointers */
				currentMaterial = NULL; 
			}
    break;

  case 99:
#line 509 "src/cfgparser.yy"
    { currentMaterial = new ntlMaterial( );
			currentMaterial = currentMaterial;
			reglob->getMaterials()->push_back(currentMaterial);
		}
    break;

  case 100:
#line 513 "src/cfgparser.yy"
    {
		yyerror("MATTYPE: Blinn NYI!"); }
    break;

  case 111:
#line 531 "src/cfgparser.yy"
    { 
			currentMaterial->setName( yyvsp[0].charValue ); 
		}
    break;

  case 112:
#line 536 "src/cfgparser.yy"
    {
			currentMaterial->setAmbientRefl( ntlColor(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); 
		}
    break;

  case 113:
#line 541 "src/cfgparser.yy"
    { 
			currentMaterial->setDiffuseRefl( ntlColor(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); 
		}
    break;

  case 114:
#line 546 "src/cfgparser.yy"
    { 
			currentMaterial->setSpecular( yyvsp[-1].floatValue ); 
			currentMaterial->setSpecExponent( yyvsp[0].floatValue ); 
		}
    break;

  case 115:
#line 552 "src/cfgparser.yy"
    { 
			currentMaterial->setMirror( yyvsp[0].floatValue ); 
		}
    break;

  case 116:
#line 557 "src/cfgparser.yy"
    { 
			currentMaterial->setTransparence( yyvsp[0].floatValue ); 
		}
    break;

  case 117:
#line 562 "src/cfgparser.yy"
    { 
			currentMaterial->setRefracIndex( yyvsp[0].floatValue ); 
		}
    break;

  case 118:
#line 567 "src/cfgparser.yy"
    { 
			currentMaterial->setTransAdditive( yyvsp[0].floatValue ); 
		}
    break;

  case 119:
#line 572 "src/cfgparser.yy"
    { 
			currentMaterial->setTransAttCol( ntlColor(yyvsp[-2].floatValue,yyvsp[-1].floatValue,yyvsp[0].floatValue) ); 
		}
    break;

  case 120:
#line 577 "src/cfgparser.yy"
    {
		currentMaterial->setFresnel( yyvsp[0].intValue ); 
	}
    break;

  case 121:
#line 588 "src/cfgparser.yy"
    { 
		currentAttrib = new AttributeList(yyvsp[-1].charValue); 
		currentAttrName = yyvsp[-1].charValue; }
    break;

  case 122:
#line 591 "src/cfgparser.yy"
    { // store attribute
			//std::cerr << " NEW ATTR " << currentAttrName << std::endl;
			//currentAttrib->print();
			attrs[currentAttrName] = currentAttrib;
			currentAttrib = NULL; }
    break;

  case 125:
#line 601 "src/cfgparser.yy"
    { currentAttrValue.clear(); currentAttribAddName = yyvsp[-1].charValue; }
    break;

  case 126:
#line 602 "src/cfgparser.yy"
    {
      currentAttrib->addAttr( currentAttribAddName, currentAttrValue, lineCount); 
			//std::cerr << " ADD ATTR " << currentAttribAddName << std::endl;
			//currentAttrib->find( currentAttribAddName )->print();
		}
    break;

  case 128:
#line 609 "src/cfgparser.yy"
    { 
		//std::cerr << "LLL "<<$2<<endl; 
		currentAttrValue.push_back(yyvsp[0].charValue); }
    break;

  case 129:
#line 612 "src/cfgparser.yy"
    {  
		//std::cerr << "LRR "<<$1<<endl; 
		currentAttrValue.push_back(yyvsp[0].charValue); }
    break;

  case 131:
#line 626 "src/cfgparser.yy"
    {
  if ( (yyvsp[0].floatValue < 0.0) || (yyvsp[0].floatValue > 1.0) ) {
    yyerror("Value out of range (only 0 to 1 allowed)");
  }

  /* pass that value up the tree */
  yyval.floatValue = yyvsp[0].floatValue;
}
    break;

  case 132:
#line 638 "src/cfgparser.yy"
    { yyval.floatValue = yyvsp[0].floatValue; }
    break;

  case 133:
#line 640 "src/cfgparser.yy"
    { yyval.floatValue = (float) yyvsp[0].intValue; /* conversion from integers */ }
    break;

  case 134:
#line 646 "src/cfgparser.yy"
    {
  if ( yyvsp[0].intValue <= 0 ) {
    yy_error("Value out of range (has to be above zero)");
  }

  /* pass that value up the tree */
  yyval.intValue = yyvsp[0].intValue;
}
    break;

  case 135:
#line 657 "src/cfgparser.yy"
    {
  //cout << " " << $1 << " ";
  if ( yyvsp[0].intValue < 0 ) {
    yy_error("Value out of range (has to be above or equal to zero)");
  }

  /* pass that value up the tree */
  yyval.intValue = yyvsp[0].intValue;
}
    break;

  case 136:
#line 669 "src/cfgparser.yy"
    {
  if( ( yyvsp[0].intValue != 0 ) && ( yyvsp[0].intValue != 1 ) ) {
    yy_error("Boolean value has to be 1|0, 'true'|'false' or 'on'|'off'!");
  }
  /* pass that value up the tree */
  yyval.intValue = yyvsp[0].intValue;
}
    break;


    }

/* Line 1010 of yacc.c.  */
#line 2051 "bld-std-gcc40/src/cfgparser.cpp"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  const char* yyprefix;
	  char *yymsg;
	  int yyx;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 0;

	  yyprefix = ", expecting ";
	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		yysize += yystrlen (yyprefix) + yystrlen (yytname [yyx]);
		yycount += 1;
		if (yycount == 5)
		  {
		    yysize = 0;
		    break;
		  }
	      }
	  yysize += (sizeof ("syntax error, unexpected ")
		     + yystrlen (yytname[yytype]));
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yyprefix = ", expecting ";
		  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			yyp = yystpcpy (yyp, yyprefix);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yyprefix = " or ";
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* If at end of input, pop the error token,
	     then the rest of the stack, then return failure.  */
	  if (yychar == YYEOF)
	     for (;;)
	       {
		 YYPOPSTACK;
		 if (yyssp == yyss)
		   YYABORT;
		 YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
		 yydestruct (yystos[*yyssp], yyvsp);
	       }
        }
      else
	{
	  YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
	  yydestruct (yytoken, &yylval);
	  yychar = YYEMPTY;

	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

#ifdef __GNUC__
  /* Pacify GCC when the user code never invokes YYERROR and the label
     yyerrorlab therefore never appears in user code.  */
  if (0)
     goto yyerrorlab;
#endif

  yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 677 "src/cfgparser.yy"


/*---------------------------------------------------------------------------*/
/* parser functions                                                          */
/*---------------------------------------------------------------------------*/


/* parse warnings */
void yy_warn(char *s)
{
	printf("Config Parse Warning at Line %d: %s \n", lineCount, s);
}

/* parse errors */
void yy_error(const char *s)
{
	//errorOut("Current token: "<<yytname[ (int)yytranslate[yychar] ]);
	errFatal("yy_error","Config Parse Error at Line "<<lineCount<<": "<<s,SIMWORLD_INITERROR);
	return;
}


/* get the global pointers from calling program */
char pointersInited = 0;
void setPointers(ntlRenderGlobals *setglob) 
{
	if(
		 (!setglob) ||
		 (!setglob) 
		 ) {
		errFatal("setPointers","Config Parse Error: Invalid Pointers!\n",SIMWORLD_INITERROR);
		return;
	}      
	
	reglob = setglob;
	pointersInited = 1;
}

  
/* parse given file as config file */
void parseFile(string filename)
{
	if(!pointersInited) {
		errFatal("parseFile","Config Parse Error: Pointers not set!\n", SIMWORLD_INITERROR);
		return;
	}
	
	/* open file */
	yy_in = fopen( filename.c_str(), "r");
	if(!yy_in) {
		errFatal("parseFile","Config Parse Error: Unable to open '"<<filename.c_str() <<"'!\n", SIMWORLD_INITERROR );
		return;
	}

	/* parse */
	//yy_debug = 1; /* Enable debugging? */
	yy_parse();
	
	/* close file */
	fclose( yy_in );
	// cleanup static map<string,AttributeList*> attrs
	for(map<string, AttributeList*>::iterator i=attrs.begin();
			i != attrs.end(); i++) {
		if((*i).second) {
			delete (*i).second;
			(*i).second = NULL;
		}
	}
}




