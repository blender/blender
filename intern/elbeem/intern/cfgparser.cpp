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

/* Substitute the variable and function names.  */
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
	SimulationObject    *currentGeometrySim;
	ntlGeometryObjModel *currentGeometryModel;
	AttributeList				*currentAttrib;
	string							currentAttrName, currentAttribAddName;

#ifndef ELBEEM_BLENDER
#include "ntl_geometrybox.h"
#include "ntl_geometrysphere.h"
	ntlGeometryBox      *currentGeometryBox;
#endif //ELBEEM_BLENDER
	
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
#line 87 "src/cfgparser.yy"
typedef union YYSTYPE {
  int    intValue;
  float  floatValue;
  char  *charValue;
} YYSTYPE;
/* Line 190 of yacc.c.  */
#line 367 "bld-std-gcc40/src/cfgparser.cpp"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 213 of yacc.c.  */
#line 379 "bld-std-gcc40/src/cfgparser.cpp"

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
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
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
#define YYFINAL  15
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   277

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  106
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  78
/* YYNRULES -- Number of rules. */
#define YYNRULES  138
/* YYNRULES -- Number of states. */
#define YYNSTATES  240

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   360

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
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     6,     8,    10,    12,    14,    16,    19,
      22,    27,    30,    32,    34,    36,    38,    40,    42,    44,
      46,    48,    50,    52,    54,    56,    58,    60,    62,    64,
      66,    68,    70,    72,    74,    76,    78,    80,    82,    85,
      88,    91,    94,    98,   101,   106,   111,   116,   119,   122,
     127,   132,   135,   138,   141,   144,   148,   151,   154,   157,
     164,   167,   169,   171,   173,   175,   177,   179,   182,   185,
     190,   195,   196,   204,   207,   209,   211,   213,   215,   217,
     219,   221,   223,   225,   227,   229,   231,   233,   235,   237,
     240,   243,   246,   249,   252,   257,   262,   265,   270,   277,
     280,   282,   284,   286,   288,   290,   292,   294,   296,   298,
     300,   302,   304,   306,   309,   314,   319,   323,   326,   329,
     332,   335,   340,   343,   344,   351,   354,   356,   357,   358,
     365,   368,   370,   372,   374,   376,   378,   380,   382
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     107,     0,    -1,   107,   108,    -1,   108,    -1,   111,    -1,
     171,    -1,   109,    -1,   110,    -1,    11,     3,    -1,    12,
       3,    -1,    34,    35,   112,    36,    -1,   112,   113,    -1,
     113,    -1,   116,    -1,   114,    -1,   115,    -1,   117,    -1,
     118,    -1,   119,    -1,   120,    -1,   121,    -1,   122,    -1,
     123,    -1,   124,    -1,   125,    -1,   126,    -1,   127,    -1,
     128,    -1,   129,    -1,   130,    -1,   131,    -1,   132,    -1,
     133,    -1,   134,    -1,   135,    -1,   143,    -1,   174,    -1,
     157,    -1,    48,   182,    -1,    10,   182,    -1,    49,   181,
      -1,    50,   183,    -1,    45,   181,   181,    -1,    46,     3,
      -1,    47,   180,   180,   180,    -1,    51,   180,   180,   180,
      -1,    52,   180,   180,   180,    -1,    53,   180,    -1,    54,
     180,    -1,    55,   180,   180,   180,    -1,    56,   180,   180,
     180,    -1,    37,     5,    -1,    43,   181,    -1,    44,   181,
      -1,    39,   181,    -1,    57,     3,     3,    -1,    58,   183,
      -1,    59,     5,    -1,    60,     5,    -1,    97,    35,    67,
     137,   136,    36,    -1,   136,   138,    -1,   138,    -1,   101,
      -1,   139,    -1,   140,    -1,   141,    -1,   142,    -1,    98,
     183,    -1,    72,   183,    -1,    99,   180,   180,   180,    -1,
     100,   180,   180,   180,    -1,    -1,    66,    35,    67,   146,
     144,   145,    36,    -1,   145,   147,    -1,   147,    -1,    68,
      -1,    71,    -1,    70,    -1,     8,    -1,     9,    -1,   148,
      -1,   149,    -1,   150,    -1,   151,    -1,   152,    -1,   153,
      -1,   154,    -1,   155,    -1,   156,    -1,    87,     5,    -1,
      84,     5,    -1,    72,   183,    -1,    73,   183,    -1,    74,
     183,    -1,    76,   180,   180,   180,    -1,    75,   180,   180,
     180,    -1,    62,     5,    -1,    64,    35,   173,    36,    -1,
      84,    35,    67,   159,   158,    36,    -1,   158,   160,    -1,
     160,    -1,    85,    -1,    86,    -1,   161,    -1,   162,    -1,
     163,    -1,   164,    -1,   165,    -1,   167,    -1,   166,    -1,
     168,    -1,   169,    -1,   170,    -1,    87,     5,    -1,    88,
     178,   178,   178,    -1,    89,   178,   178,   178,    -1,    90,
     180,   180,    -1,    91,   179,    -1,    92,   179,    -1,    93,
     180,    -1,    94,   179,    -1,    95,   180,   180,   180,    -1,
      96,   182,    -1,    -1,    61,     6,    35,   172,   173,    36,
      -1,   173,   174,    -1,   174,    -1,    -1,    -1,     6,    63,
     175,   177,   176,    65,    -1,   177,     7,    -1,     7,    -1,
     179,    -1,   180,    -1,     4,    -1,     3,    -1,     3,    -1,
       3,    -1,     3,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   146,   146,   147,   150,   151,   152,   153,   157,   160,
     175,   176,   176,   179,   180,   181,   182,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   202,   203,   204,   205,   210,   214,
     219,   223,   229,   233,   237,   241,   245,   249,   253,   257,
     261,   265,   269,   273,   277,   281,   285,   289,   294,   303,
     314,   315,   318,   326,   327,   328,   329,   333,   338,   343,
     348,   366,   365,   386,   387,   390,   399,   407,   412,   418,
     430,   431,   432,   433,   434,   435,   436,   437,   438,   443,
     448,   454,   460,   466,   471,   486,   502,   508,   518,   529,
     530,   533,   538,   542,   543,   544,   545,   546,   547,   548,
     549,   550,   551,   556,   561,   566,   571,   577,   582,   587,
     592,   597,   602,   613,   613,   623,   623,   626,   627,   626,
     634,   637,   647,   650,   662,   664,   670,   681,   693
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DT_INTEGER", "DT_FLOAT", "DT_STRING",
  "DT_ATTRNAME", "DT_ATTRVALUE", "KW_LBMSIM", "KW_COMPARELBM",
  "KW_ANIFRAMETIME", "KW_DEBUGMODE", "KW_DEBUGLEVEL", "KW_P_RELAXTIME",
  "KW_P_REYNOLDS", "KW_P_VISCOSITY", "KW_P_SOUNDSPEED", "KW_P_DOMAINSIZE",
  "KW_P_FORCE", "KW_P_TIMELENGTH", "KW_P_STEPTIME", "KW_P_TIMEFACTOR",
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
  "setdebuglevel_expression", "raytrace_section", "raytrace_line",
  "raytrace_expression", "anistart_expression", "aniframetime_expression",
  "aniframes_expression", "frameskip_expression", "resolution_expression",
  "antialias_expression", "eyepoint_expression", "lookat_expression",
  "upvec_expression", "fovy_expression", "aspect_expression",
  "ambience_expression", "background_expression", "filename_expression",
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
     355,   356,   357,   358,   359,   360
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,   106,   107,   107,   108,   108,   108,   108,   109,   110,
     111,   112,   112,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,   113,   113,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   136,   137,   138,   138,   138,   138,   139,   140,   141,
     142,   144,   143,   145,   145,   146,   146,   146,   146,   146,
     147,   147,   147,   147,   147,   147,   147,   147,   147,   148,
     149,   150,   151,   152,   153,   154,   155,   156,   157,   158,
     158,   159,   159,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   172,   171,   173,   173,   175,   176,   174,
     177,   177,   178,   179,   180,   180,   181,   182,   183
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     2,     1,     1,     1,     1,     1,     2,     2,
       4,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       2,     2,     3,     2,     4,     4,     4,     2,     2,     4,
       4,     2,     2,     2,     2,     3,     2,     2,     2,     6,
       2,     1,     1,     1,     1,     1,     1,     2,     2,     4,
       4,     0,     7,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     2,     4,     4,     2,     4,     6,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     4,     4,     3,     2,     2,     2,
       2,     4,     2,     0,     6,     2,     1,     0,     0,     6,
       2,     1,     1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       0,     0,     0,     0,     0,     0,     3,     6,     7,     4,
       5,     8,     9,     0,     0,     1,     2,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    12,    14,    15,    13,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    37,    36,   123,
     127,   137,    39,    51,   136,    54,    52,    53,     0,    43,
     135,   134,     0,    38,    40,   138,    41,     0,     0,    47,
      48,     0,     0,     0,    56,    57,    58,     0,     0,     0,
      10,    11,     0,     0,    42,     0,     0,     0,     0,     0,
      55,     0,     0,     0,     0,   126,   131,   128,    44,    45,
      46,    49,    50,    78,    79,    75,    77,    76,    71,   101,
     102,     0,    62,     0,   124,   125,   130,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     100,   103,   104,   105,   106,   107,   109,   108,   110,   111,
     112,     0,     0,     0,     0,     0,    61,    63,    64,    65,
      66,   129,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    74,    80,    81,    82,    83,    84,    85,    86,
      87,    88,   113,     0,   132,   133,     0,     0,   117,   118,
     119,   120,     0,   122,    98,    99,    68,    67,     0,     0,
      59,    60,    96,     0,    91,    92,    93,     0,     0,    90,
      89,    72,    73,     0,     0,   116,     0,     0,     0,     0,
       0,     0,   114,   115,   121,    69,    70,    97,    95,    94
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,     5,     6,     7,     8,     9,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
     165,   133,   166,   167,   168,   169,   170,    66,   138,   181,
     128,   182,   183,   184,   185,   186,   187,   188,   189,   190,
     191,    67,   149,   131,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,    10,   102,   114,    68,   103,
     137,   117,   193,   194,   195,    75,    72,    86
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -115
static const short int yypact[] =
{
       7,    10,    22,    -9,    27,    11,  -115,  -115,  -115,  -115,
    -115,  -115,  -115,   170,     2,  -115,  -115,   -21,    35,    34,
      40,    40,    40,    40,    43,    17,    35,    40,    44,    17,
      17,    17,    17,    17,    17,    46,    44,    48,    51,    23,
      25,    29,    49,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,    40,  -115,
    -115,  -115,    17,  -115,  -115,  -115,  -115,    17,    17,  -115,
    -115,    17,    17,    62,  -115,  -115,  -115,   -13,     3,     4,
    -115,  -115,    63,    66,  -115,    17,    17,    17,    17,    17,
    -115,    19,   -69,   -23,    -5,  -115,  -115,    70,  -115,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,   181,  -115,   -48,  -115,  -115,  -115,    14,   169,   106,
      17,    17,    17,    17,    17,    17,    17,    17,    35,    74,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,    44,    44,    17,    17,   -24,  -115,  -115,  -115,  -115,
    -115,  -115,   107,    83,    44,    44,    44,    17,    17,   114,
     116,   124,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,  -115,  -115,    17,  -115,  -115,    17,    17,  -115,  -115,
    -115,  -115,    17,  -115,  -115,  -115,  -115,  -115,    17,    17,
    -115,  -115,  -115,    63,  -115,  -115,  -115,    17,    17,  -115,
    -115,  -115,  -115,    17,    17,  -115,    17,    17,    17,     8,
      17,    17,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
    -115,  -115,   118,  -115,  -115,  -115,  -115,    82,  -115,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,  -115,   -40,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,   -55,  -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,
    -115,  -115,  -115,  -115,   -20,  -115,  -115,  -115,  -115,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,   -82,   -99,  -115,
    -115,  -115,   -80,  -114,   -25,    13,   -16,   -34
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
      82,    17,    94,   115,    87,    88,    89,    90,    91,    92,
      83,    15,   210,    11,    17,   135,   129,   130,     1,     2,
      80,    81,     1,     2,   161,    12,    13,   123,   124,   198,
     199,   134,   201,    14,    76,    77,    78,    69,    71,    73,
      84,     3,    70,    74,   237,     3,    79,    85,   161,    93,
     162,   163,   164,    95,   111,    17,    96,   105,    97,    18,
      98,   196,   106,   107,    99,   110,   108,   109,     4,    17,
     112,   113,     4,   116,   162,   163,   164,   136,   132,   171,
     118,   119,   120,   121,   122,   100,    19,   125,    20,   126,
     127,   104,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
     204,   192,   212,   223,   115,    39,   224,   197,   213,   219,
     200,   220,   202,    16,   101,   211,   222,   206,   207,   205,
     135,   229,   203,    40,     0,     0,     0,     0,   208,   209,
     214,   215,   216,   232,   233,     0,    41,     0,     0,     0,
       0,     0,   217,   218,     0,     0,     0,     0,     0,     0,
     221,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,     0,   225,     0,     0,     0,    17,   226,     0,     0,
      18,     0,     0,   227,   228,     0,   172,     0,   173,     0,
       0,     0,   230,   231,     0,     0,   174,   175,   176,   177,
     178,   234,   235,   236,     0,   238,   239,    19,   179,    20,
       0,   180,     0,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,   172,     0,   173,     0,     0,    39,     0,     0,     0,
       0,   174,   175,   176,   177,   178,     0,     0,     0,     0,
       0,     0,     0,   179,    40,     0,   180,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    41,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148
};

static const short int yycheck[] =
{
      25,     6,    36,   102,    29,    30,    31,    32,    33,    34,
      26,     0,    36,     3,     6,   114,    85,    86,    11,    12,
       3,     4,    11,    12,    72,     3,    35,     8,     9,   143,
     144,    36,   146,     6,    21,    22,    23,    35,     3,     5,
      27,    34,    63,     3,    36,    34,     3,     3,    72,     3,
      98,    99,   100,     5,    67,     6,     5,    82,    35,    10,
      35,   141,    87,    88,    35,     3,    91,    92,    61,     6,
      67,    67,    61,     7,    98,    99,   100,     7,   101,    65,
     105,   106,   107,   108,   109,    36,    37,    68,    39,    70,
      71,    78,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      36,     5,     5,   193,   213,    66,   196,   142,    35,     5,
     145,     5,   147,     5,    42,   165,   181,   161,   162,   149,
     229,   213,   148,    84,    -1,    -1,    -1,    -1,   163,   164,
     174,   175,   176,   223,   224,    -1,    97,    -1,    -1,    -1,
      -1,    -1,   177,   178,    -1,    -1,    -1,    -1,    -1,    -1,
      36,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    -1,   197,    -1,    -1,    -1,     6,   202,    -1,    -1,
      10,    -1,    -1,   208,   209,    -1,    62,    -1,    64,    -1,
      -1,    -1,   217,   218,    -1,    -1,    72,    73,    74,    75,
      76,   226,   227,   228,    -1,   230,   231,    37,    84,    39,
      -1,    87,    -1,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    62,    -1,    64,    -1,    -1,    66,    -1,    -1,    -1,
      -1,    72,    73,    74,    75,    76,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    84,    84,    -1,    87,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    97,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    11,    12,    34,    61,   107,   108,   109,   110,   111,
     171,     3,     3,    35,     6,     0,   108,     6,    10,    37,
      39,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    66,
      84,    97,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   143,   157,   174,    35,
      63,     3,   182,     5,     3,   181,   181,   181,   181,     3,
       3,     4,   180,   182,   181,     3,   183,   180,   180,   180,
     180,   180,   180,     3,   183,     5,     5,    35,    35,    35,
      36,   113,   172,   175,   181,   180,   180,   180,   180,   180,
       3,    67,    67,    67,   173,   174,     7,   177,   180,   180,
     180,   180,   180,     8,     9,    68,    70,    71,   146,    85,
      86,   159,   101,   137,    36,   174,     7,   176,   144,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,   158,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,    72,    98,    99,   100,   136,   138,   139,   140,   141,
     142,    65,    62,    64,    72,    73,    74,    75,    76,    84,
      87,   145,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,     5,   178,   179,   180,   178,   180,   179,   179,
     180,   179,   180,   182,    36,   160,   183,   183,   180,   180,
      36,   138,     5,    35,   183,   183,   183,   180,   180,     5,
       5,    36,   147,   178,   178,   180,   180,   180,   180,   173,
     180,   180,   178,   178,   180,   180,   180,    36,   180,   180
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


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (0)
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
              (Loc).first_line, (Loc).first_column,	\
              (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
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

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Type, Value);	\
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
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
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
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);


# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  switch (yytype)
    {
			case 0:
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

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



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
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
  /* Look-ahead token as an internal (translated) token number.  */
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


  yyvsp[0] = yylval;

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
/* Read a look-ahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to look-ahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
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
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
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

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

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
        case 8:
#line 157 "src/cfgparser.yy"
    { yy_debug = (yyvsp[0].intValue); }
    break;

  case 9:
#line 160 "src/cfgparser.yy"
    { 
		int sdebug = (yyvsp[0].intValue); 
		if(sdebug<0) sdebug=0;
		if(sdebug>10) sdebug=10;
		gDebugLevel = sdebug;
	}
    break;

  case 38:
#line 211 "src/cfgparser.yy"
    { reglob->setAniStart( (yyvsp[0].intValue) ); }
    break;

  case 39:
#line 215 "src/cfgparser.yy"
    { /*reglob->setAniFrameTime( $2 );*/ debMsgStd("cfgparser",DM_NOTIFY,"Deprecated setting aniframetime!",1); }
    break;

  case 40:
#line 220 "src/cfgparser.yy"
    { reglob->setAniFrames( ((yyvsp[0].intValue))-1 ); }
    break;

  case 41:
#line 224 "src/cfgparser.yy"
    { reglob->setFrameSkip( ((yyvsp[0].intValue)) ); }
    break;

  case 42:
#line 230 "src/cfgparser.yy"
    { reglob->setResX( (yyvsp[-1].intValue) ); reglob->setResY( (yyvsp[0].intValue)); }
    break;

  case 43:
#line 234 "src/cfgparser.yy"
    { reglob->setAADepth( (yyvsp[0].intValue) ); }
    break;

  case 44:
#line 238 "src/cfgparser.yy"
    { reglob->setEye( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); }
    break;

  case 45:
#line 242 "src/cfgparser.yy"
    { reglob->setLookat( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); }
    break;

  case 46:
#line 246 "src/cfgparser.yy"
    { reglob->setUpVec( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); }
    break;

  case 47:
#line 250 "src/cfgparser.yy"
    { reglob->setFovy( (yyvsp[0].floatValue) ); }
    break;

  case 48:
#line 254 "src/cfgparser.yy"
    { reglob->setAspect( (yyvsp[0].floatValue) ); }
    break;

  case 49:
#line 258 "src/cfgparser.yy"
    { reglob->setAmbientLight( ntlColor((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue))  ); }
    break;

  case 50:
#line 262 "src/cfgparser.yy"
    { reglob->setBackgroundCol( ntlColor((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); }
    break;

  case 51:
#line 266 "src/cfgparser.yy"
    { reglob->setOutFilename( (yyvsp[0].charValue) ); }
    break;

  case 52:
#line 270 "src/cfgparser.yy"
    { reglob->setTreeMaxDepth( (yyvsp[0].intValue) ); }
    break;

  case 53:
#line 274 "src/cfgparser.yy"
    { reglob->setTreeMaxTriangles( (yyvsp[0].intValue) ); }
    break;

  case 54:
#line 278 "src/cfgparser.yy"
    { reglob->setRayMaxDepth( (yyvsp[0].intValue) ); }
    break;

  case 55:
#line 282 "src/cfgparser.yy"
    { reglob->setDebugPixel( (yyvsp[-1].intValue), (yyvsp[0].intValue) ); }
    break;

  case 56:
#line 286 "src/cfgparser.yy"
    { reglob->setTestMode( (yyvsp[0].intValue) ); }
    break;

  case 57:
#line 290 "src/cfgparser.yy"
    { if(attrs[(yyvsp[0].charValue)] == NULL){ yyerror("OPENGL ATTRIBUTES: The attribute was not found!"); }
			reglob->getOpenGlAttributes()->import( attrs[(yyvsp[0].charValue)] ); }
    break;

  case 58:
#line 295 "src/cfgparser.yy"
    { if(attrs[(yyvsp[0].charValue)] == NULL){ yyerror("BLENDER ATTRIBUTES: The attribute was not found!"); }
			reglob->getBlenderAttributes()->import( attrs[(yyvsp[0].charValue)] ); }
    break;

  case 59:
#line 307 "src/cfgparser.yy"
    { 
				/* reset light pointers */
				currentLightOmni = NULL; 
			}
    break;

  case 62:
#line 319 "src/cfgparser.yy"
    { currentLightOmni = new ntlLightObject( reglob );
		  currentLight = currentLightOmni;
			reglob->getLightList()->push_back(currentLight);
		}
    break;

  case 67:
#line 333 "src/cfgparser.yy"
    { 
			currentLight->setActive( (yyvsp[0].intValue) ); 
		}
    break;

  case 68:
#line 338 "src/cfgparser.yy"
    { 
			currentLight->setCastShadows( (yyvsp[0].intValue) ); 
		}
    break;

  case 69:
#line 343 "src/cfgparser.yy"
    { 
			currentLight->setColor( ntlColor((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); 
		}
    break;

  case 70:
#line 348 "src/cfgparser.yy"
    { 
		int init = 0;
		if(currentLightOmni != NULL) {
			currentLightOmni->setPosition( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); init = 1; }
		if(!init) yyerror("This property can only be set for omni-directional and rectangular lights!");
	}
    break;

  case 71:
#line 366 "src/cfgparser.yy"
    {
				// geo classes have attributes...
				reglob->getScene()->addGeoClass(currentGeoClass);
				currentAttrib = currentGeoClass->getAttributeList();
			}
    break;

  case 72:
#line 372 "src/cfgparser.yy"
    { 
				/* reset geometry object pointers */
				currentGeoObj			 = NULL; 
				currentGeoClass		 = NULL; 
				currentGeometrySim = NULL; 
				currentGeometryModel = NULL; 
				currentAttrib = NULL;
#ifndef ELBEEM_BLENDER
				currentGeometryBox = NULL; 
#endif // ELBEEM_BLENDER
			}
    break;

  case 75:
#line 390 "src/cfgparser.yy"
    { 
#ifndef ELBEEM_BLENDER
			currentGeometryBox = new ntlGeometryBox( );
			currentGeoClass = currentGeometryBox;
			currentGeoObj = (ntlGeometryObject*)( currentGeometryBox );
#else // ELBEEM_BLENDER
			yyerror("GEOTYPE_BOX : This object type is not supported in this version!");
#endif // ELBEEM_BLENDER
		}
    break;

  case 76:
#line 399 "src/cfgparser.yy"
    { 
#ifndef ELBEEM_BLENDER
			currentGeoClass = new ntlGeometrySphere( );
			currentGeoObj = (ntlGeometryObject*)( currentGeoClass );
#else // ELBEEM_BLENDER
			yyerror("GEOTYPE_SPHERE : This object type is not supported in this version!");
#endif // ELBEEM_BLENDER
		}
    break;

  case 77:
#line 407 "src/cfgparser.yy"
    { 
			currentGeometryModel = new ntlGeometryObjModel( );
			currentGeoClass = currentGeometryModel;
			currentGeoObj = (ntlGeometryObject*)( currentGeometryModel );
		}
    break;

  case 78:
#line 412 "src/cfgparser.yy"
    { 
			currentGeometrySim = new SimulationObject();
			currentGeoClass = currentGeometrySim;
			reglob->getSims()->push_back(currentGeometrySim);
			// dont add mcubes to geo list!
		}
    break;

  case 79:
#line 418 "src/cfgparser.yy"
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

  case 89:
#line 443 "src/cfgparser.yy"
    { 
			currentGeoClass->setName( (yyvsp[0].charValue) ); 
		}
    break;

  case 90:
#line 448 "src/cfgparser.yy"
    { 
			if(currentGeoObj == NULL){ yyerror(" MATERIAL : This property can only be set for geometry objects!"); }
			currentGeoObj->setMaterialName( (yyvsp[0].charValue) ); 
		}
    break;

  case 91:
#line 454 "src/cfgparser.yy"
    { 
			if(currentGeoObj == NULL){ yyerror(" CAST_SHADOW : This property can only be set for geometry objects!"); }
			currentGeoObj->setCastShadows( (yyvsp[0].intValue) ); 
		}
    break;

  case 92:
#line 460 "src/cfgparser.yy"
    { 
			if(currentGeoObj == NULL){ yyerror(" RECEIVE_SHADOW : This property can only be set for geometry objects!"); }
			currentGeoObj->setReceiveShadows( (yyvsp[0].intValue) ); 
		}
    break;

  case 93:
#line 466 "src/cfgparser.yy"
    { 
			currentGeoClass->setVisible( (yyvsp[0].intValue) ); 
		}
    break;

  case 94:
#line 471 "src/cfgparser.yy"
    { 
		int init = 0;
#ifndef ELBEEM_BLENDER
		if(currentGeometryBox != NULL){ 
			currentGeometryBox->setStart( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); init=1; }
#else // ELBEEM_BLENDER
#endif // ELBEEM_BLENDER
		if(currentGeometrySim != NULL){ 
			currentGeometrySim->setGeoStart( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); init=1; }
		if(currentGeometryModel != NULL){ 
			currentGeometryModel->setStart( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); init=1; }
		if(!init ){ yyerror("BOXSTART : This property can only be set for box, objmodel, fluid and isosurface objects!"); }
	}
    break;

  case 95:
#line 486 "src/cfgparser.yy"
    { 
		int init = 0;
#ifndef ELBEEM_BLENDER
		if(currentGeometryBox != NULL){ 
			currentGeometryBox->setEnd( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); init=1; }
#else // ELBEEM_BLENDER
#endif // ELBEEM_BLENDER
		if(currentGeometrySim != NULL){ 
			currentGeometrySim->setGeoEnd( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); init=1; }
		if(currentGeometryModel != NULL){ 
			currentGeometryModel->setEnd( ntlVec3Gfx((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); init=1; }
		if(!init ){ yyerror("BOXEND : This property can only be set for box, objmodel, fluid and isosurface objects!"); }
	}
    break;

  case 96:
#line 502 "src/cfgparser.yy"
    { 
			if(attrs[(yyvsp[0].charValue)] == NULL){ yyerror("GEO ATTRIBUTES: The attribute was not found!"); }
			currentGeoClass->getAttributeList()->import( attrs[(yyvsp[0].charValue)] ); 
		}
    break;

  case 97:
#line 510 "src/cfgparser.yy"
    { }
    break;

  case 98:
#line 522 "src/cfgparser.yy"
    { 
				/* reset geometry object pointers */
				currentMaterial = NULL; 
			}
    break;

  case 101:
#line 534 "src/cfgparser.yy"
    { currentMaterial = new ntlMaterial( );
			currentMaterial = currentMaterial;
			reglob->getMaterials()->push_back(currentMaterial);
		}
    break;

  case 102:
#line 538 "src/cfgparser.yy"
    {
		yyerror("MATTYPE: Blinn NYI!"); }
    break;

  case 113:
#line 556 "src/cfgparser.yy"
    { 
			currentMaterial->setName( (yyvsp[0].charValue) ); 
		}
    break;

  case 114:
#line 561 "src/cfgparser.yy"
    {
			currentMaterial->setAmbientRefl( ntlColor((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); 
		}
    break;

  case 115:
#line 566 "src/cfgparser.yy"
    { 
			currentMaterial->setDiffuseRefl( ntlColor((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); 
		}
    break;

  case 116:
#line 571 "src/cfgparser.yy"
    { 
			currentMaterial->setSpecular( (yyvsp[-1].floatValue) ); 
			currentMaterial->setSpecExponent( (yyvsp[0].floatValue) ); 
		}
    break;

  case 117:
#line 577 "src/cfgparser.yy"
    { 
			currentMaterial->setMirror( (yyvsp[0].floatValue) ); 
		}
    break;

  case 118:
#line 582 "src/cfgparser.yy"
    { 
			currentMaterial->setTransparence( (yyvsp[0].floatValue) ); 
		}
    break;

  case 119:
#line 587 "src/cfgparser.yy"
    { 
			currentMaterial->setRefracIndex( (yyvsp[0].floatValue) ); 
		}
    break;

  case 120:
#line 592 "src/cfgparser.yy"
    { 
			currentMaterial->setTransAdditive( (yyvsp[0].floatValue) ); 
		}
    break;

  case 121:
#line 597 "src/cfgparser.yy"
    { 
			currentMaterial->setTransAttCol( ntlColor((yyvsp[-2].floatValue),(yyvsp[-1].floatValue),(yyvsp[0].floatValue)) ); 
		}
    break;

  case 122:
#line 602 "src/cfgparser.yy"
    {
		currentMaterial->setFresnel( (yyvsp[0].intValue) ); 
	}
    break;

  case 123:
#line 613 "src/cfgparser.yy"
    { 
		currentAttrib = new AttributeList((yyvsp[-1].charValue)); 
		currentAttrName = (yyvsp[-1].charValue); }
    break;

  case 124:
#line 616 "src/cfgparser.yy"
    { // store attribute
			//std::cerr << " NEW ATTR " << currentAttrName << std::endl;
			//currentAttrib->print();
			attrs[currentAttrName] = currentAttrib;
			currentAttrib = NULL; }
    break;

  case 127:
#line 626 "src/cfgparser.yy"
    { currentAttrValue.clear(); currentAttribAddName = (yyvsp[-1].charValue); }
    break;

  case 128:
#line 627 "src/cfgparser.yy"
    {
      currentAttrib->addAttr( currentAttribAddName, currentAttrValue, lineCount); 
			//std::cerr << " ADD ATTR " << currentAttribAddName << std::endl;
			//currentAttrib->find( currentAttribAddName )->print();
		}
    break;

  case 130:
#line 634 "src/cfgparser.yy"
    { 
		//std::cerr << "LLL "<<$2<<endl; 
		currentAttrValue.push_back((yyvsp[0].charValue)); }
    break;

  case 131:
#line 637 "src/cfgparser.yy"
    {  
		//std::cerr << "LRR "<<$1<<endl; 
		currentAttrValue.push_back((yyvsp[0].charValue)); }
    break;

  case 133:
#line 651 "src/cfgparser.yy"
    {
  if ( ((yyvsp[0].floatValue) < 0.0) || ((yyvsp[0].floatValue) > 1.0) ) {
    yyerror("Value out of range (only 0 to 1 allowed)");
  }

  /* pass that value up the tree */
  (yyval.floatValue) = (yyvsp[0].floatValue);
}
    break;

  case 134:
#line 663 "src/cfgparser.yy"
    { (yyval.floatValue) = (yyvsp[0].floatValue); }
    break;

  case 135:
#line 665 "src/cfgparser.yy"
    { (yyval.floatValue) = (float) (yyvsp[0].intValue); /* conversion from integers */ }
    break;

  case 136:
#line 671 "src/cfgparser.yy"
    {
  if ( (yyvsp[0].intValue) <= 0 ) {
    yy_error("Value out of range (has to be above zero)");
  }

  /* pass that value up the tree */
  (yyval.intValue) = (yyvsp[0].intValue);
}
    break;

  case 137:
#line 682 "src/cfgparser.yy"
    {
  //cout << " " << $1 << " ";
  if ( (yyvsp[0].intValue) < 0 ) {
    yy_error("Value out of range (has to be above or equal to zero)");
  }

  /* pass that value up the tree */
  (yyval.intValue) = (yyvsp[0].intValue);
}
    break;

  case 138:
#line 694 "src/cfgparser.yy"
    {
  if( ( (yyvsp[0].intValue) != 0 ) && ( (yyvsp[0].intValue) != 1 ) ) {
    yy_error("Boolean value has to be 1|0, 'true'|'false' or 'on'|'off'!");
  }
  /* pass that value up the tree */
  (yyval.intValue) = (yyvsp[0].intValue);
}
    break;


    }

/* Line 1037 of yacc.c.  */
#line 2105 "bld-std-gcc40/src/cfgparser.cpp"

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
      /* If just tried and failed to reuse look-ahead token after an
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
		 yydestruct ("Error: popping",
                             yystos[*yyssp], yyvsp);
	       }
        }
      else
	{
	  yydestruct ("Error: discarding", yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
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


      yydestruct ("Error: popping", yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token. */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

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
  yydestruct ("Error: discarding lookahead",
              yytoken, &yylval);
  yychar = YYEMPTY;
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


#line 702 "src/cfgparser.yy"


/*---------------------------------------------------------------------------*/
/* parser functions                                                          */
/*---------------------------------------------------------------------------*/


/* parse warnings */
void yy_warn(char *s)
{
	debMsgStd("yy_warn",DM_WARNING,"Config Parse Warning at Line :"<<lineCount<<" '"<<s<<"' ",1);
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




