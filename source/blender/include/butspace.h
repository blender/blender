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
 */
#ifndef BUTSPACE_H
#define BUTSPACE_H

/* all internal calls and event codes for buttons space */

struct Base;
struct Object;
struct ID;
struct ColorBand;
struct uiBlock;
struct rctf;
struct CurveMap;
struct ImageUser;
struct RenderResult;
struct Image;

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
#define BUTS_SENS_STATE		512
#define BUTS_ACT_STATE		1024

/* internal */

/* scene */
extern void render_panels(void);
extern void do_render_panels(unsigned short event);
extern void anim_panels(void);
extern void sound_panels(void);
extern void do_soundbuts(unsigned short event);
extern void sequencer_panels(void);
extern void do_sequencer_panels(unsigned short event);

/* object */
extern void object_panels(void);
extern void physics_panels(void);
extern void particle_panels(void);
extern void do_object_panels(unsigned short event);
extern void do_constraintbuts(unsigned short event);
extern void object_panel_constraint(char *context);
extern void autocomplete_bone(char *str, void *arg_v);
extern void autocomplete_vgroup(char *str, void *arg_v);

/* effects */
extern void effects_panels(void);
extern void do_effects_panels(unsigned short event);

/* modifiers */
extern int mod_moveUp(void *ob_v, void *md_v);
extern int mod_moveDown(void *ob_v, void *md_v);

/* constraint */
extern void const_moveUp(void *ob_v, void *con_v);
extern void const_moveDown(void *ob_v, void *con_v);
extern void del_constr_func (void *ob_v, void *con_v);

/* editing */
extern void editing_panels(void);
extern void do_common_editbuts(unsigned short event);
extern void do_meshbuts(unsigned short event);
extern void do_vgroupbuts(unsigned short event);
extern void do_curvebuts(unsigned short event);
extern void do_fontbuts(unsigned short event);
extern void do_mballbuts(unsigned short event);
extern void do_latticebuts(unsigned short event);
extern void do_fpaintbuts(unsigned short event);
extern void do_cambuts(unsigned short event);
extern void do_armbuts(unsigned short event);
extern void do_uvcalculationbuts(unsigned short event);
extern void weight_paint_buttons(struct uiBlock *);
extern void particle_edit_buttons(struct uiBlock *);

extern char *get_vertexgroup_menustr(struct Object *ob);	// used in object buttons

/* shading */
extern void draw_colorband_buts_small(struct uiBlock *block, struct ColorBand *coba, rctf *rct, int event);
extern void material_panels(void);
extern void do_matbuts(unsigned short event);
extern void lamp_panels(void);
extern void do_lampbuts(unsigned short event);
extern void world_panels(void);
extern void do_worldbuts(unsigned short event);
extern void radio_panels(void);
extern void do_radiobuts(unsigned short event);
extern void texture_panels(void);
extern void do_texbuts(unsigned short event);
void uiblock_image_panel(struct uiBlock *block, struct Image **ima_pp, struct ImageUser *iuser, 
						 short redraw, short imagechanged);
void uiblock_layer_pass_buttons(struct uiBlock *block, struct RenderResult *rr, 
							   struct ImageUser *iuser, int event, int x, int y, int w);

/* logic */
extern void do_logic_buts(unsigned short event);
extern void logic_buts(void);

/* script */
extern void script_panels(void);
extern void do_scriptbuts(unsigned short event);

/* ipowindow */
extern void do_ipobuts(unsigned short event);	// drawipo.c (bad! ton)

/* butspace.c */
void test_meshpoin_but(char *name, struct ID **idpp);
void test_obpoin_but(char *name, struct ID **idpp);
void test_meshobpoin_but(char *name, struct ID **idpp);
void test_scenepoin_but(char *name, struct ID **idpp);
void test_matpoin_but(char *name, struct ID **idpp);
void test_scriptpoin_but(char *name, struct ID **idpp);
void test_actionpoin_but(char *name, ID **idpp);
void test_grouppoin_but(char *name, ID **idpp);
void test_texpoin_but(char *name, ID **idpp);
void test_imapoin_but(char *name, ID **idpp);

void test_idbutton_cb(void *namev, void *arg2_unused);

struct CurveMapping;
void curvemap_buttons(struct uiBlock *block, struct CurveMapping *cumap, char labeltype, short event, short redraw, struct rctf *rect);

/* -------------- internal event defines ------------ */


#define B_DIFF			1	

/* *********************** */
#define B_VIEWBUTS		1100

#define B_OBJECTPANELROT 	1007
#define B_OBJECTPANELMEDIAN 1008
#define B_ARMATUREPANEL1 	1009
#define B_ARMATUREPANEL2 	1010
#define B_OBJECTPANELPARENT 1011
#define B_OBJECTPANEL		1012
#define B_ARMATUREPANEL3 	1013
#define B_OBJECTPANELSCALE 	1014
#define B_OBJECTPANELDIMS 	1015
#define B_TRANSFORMSPACEADD	1016
#define B_TRANSFORMSPACECLEAR	1017

/* *********************** */
#define B_LAMPBUTS		1200

#define B_LAMPREDRAW		1101
#define B_COLLAMP		1102
#define B_TEXCLEARLAMP		1103
#define B_SBUFF			1104
#define B_SHADBUF		1105
#define B_SHADRAY		1106
#define B_LMTEXPASTE	1107
#define B_LMTEXCOPY		1108
#define B_LFALLOFFCHANGED	1109
#define B_LMTEXMOVEUP		1110
#define B_LMTEXMOVEDOWN		1111

/* *********************** */
#define B_MATBUTS		1300

#define B_MATCOL		1201
#define B_SPECCOL		1202
#define B_MIRCOL		1203
#define B_ACTCOL		1204
#define B_MATFROM		1205
#define B_MATPRV		1206
#define B_LAMPPRV		1207
#define B_WORLDPRV		1208
#define B_MTEXCOL		1210
#define B_TEXCLEAR		1211
#define B_MTEXPASTE		1212
#define B_MTEXCOPY		1213
#define B_MATLAY		1214
#define B_MATHALO		1215
#define B_MATZTRANSP	1216
#define B_MATRAYTRANSP	1217
#define B_MATCOLORBAND	1218
		/* yafray: material preset menu event */
#define B_MAT_YF_PRESET	1219

#define B_MAT_LAYERBROWSE	1220
#define B_MAT_USENODES		1221
#define B_MAT_VCOL_PAINT	1222
#define B_MAT_VCOL_LIGHT	1223

	/* world buttons: buttons-preview update, and redraw 3dview */
#define B_WORLDPRV2		1224

#define B_MAT_PARTICLE		1225

#define B_MTEXMOVEUP		1226
#define B_MTEXMOVEDOWN		1227

/* *********************** */
#define B_TEXBUTS		1400

#define B_TEXTYPE		1301
#define B_DEFTEXVAR		1302

#define B_NAMEIMA		1304
#define B_TEXCHANNEL	1305
#define B_TEXREDR_PRV	1306
#define B_IMAGECHANGED	1307

#define B_LOADPLUGIN	1310
#define B_NAMEPLUGIN	1311
#define B_COLORBAND		1312
#define B_ADDCOLORBAND	1313
#define B_DELCOLORBAND	1314
#define B_CALCCBAND		1315
#define B_CALCCBAND2	1316
#define B_DOCOLORBAND	1317
#define B_REDRAWCBAND	1318
#define B_BANDCOL		1319
#define B_LOADTEXIMA1	1320
#define B_TEXPRV		1321


#define B_PLUGBUT		1325
/* B_PLUGBUT reserves 24 buttons at least! */

#define B_ENV_MAKE		1350
#define B_ENV_FREE		1351
#define B_ENV_DELETE	1352
#define B_ENV_SAVE		1353
#define B_ENV_OB		1354

#define B_ENV_FREE_ALL	1357


/* **************** animbuts = object buttons ******* */
#define B_ANIMBUTS		1500

#define B_RECALCPATH	1401
#define B_TRACKBUTS		1402
#define B_DUPLI_FRAME	1403
#define B_DUPLI_VERTS	1404
#define B_DUPLI_FACES	1405
#define B_DUPLI_GROUP	1406


#define B_PRINTSPEED	1413
#define B_PRINTLEN		1414
#define B_RELKEY		1415
#define B_CURVECHECK	1416

#define B_SOFTBODY_CHANGE		1420
#define B_SOFTBODY_DEL_VG		1421
#define B_SOFTBODY_BAKE			1422
#define B_SOFTBODY_BAKE_FREE	1423

/* this has MAX_EFFECT settings! Next free define is 1450... */
#define B_SELEFFECT	1430

/* Fluidsim button defines */
#define B_FLUIDSIM_BAKE	        1450
#define B_FLUIDSIM_SELDIR	      1451
#define B_FLUIDSIM_FORCEREDRAW	1452
#define B_FLUIDSIM_MAKEPART	    1453
#define B_FLUIDSIM_CHANGETYPE   1454

#define B_GROUP_RELINK			1460
#define B_OBJECT_IPOFLAG		1461

#define B_BAKE_CACHE_CHANGE		1470

/* Cloth sim button defines */
#define B_CLOTH_CHANGEPREROLL	1480

/* *********************** */
#define B_WORLDBUTS		1600

#define B_TEXCLEARWORLD	1501
#define B_COLHOR		1502
#define B_COLZEN		1503
#define B_WMTEXPASTE	1504
#define B_WMTEXCOPY		1505
#define B_AO_FALLOFF	1506
#define B_WMTEXMOVEUP		1507
#define B_WMTEXMOVEDOWN		1508

/* *********************** */
#define B_RENDERBUTS	1690
#define B_SEQUENCERBUTS 1699

#define B_FS_PIC		1601
#define B_FS_BACKBUF	1602

#define B_FS_FTYPE		1604 /* FTYPE is no more */
#define B_DORENDER		1605
#define B_DOANIM		1606
#define B_PLAYANIM		1607
#define B_PR_PAL		1608
#define B_PR_FULL		1609
#define B_PR_PRV		1610
#define B_PR_HD			1611
#define B_PR_PAL169		1612

#define B_REDRAWDISP	1615
#define B_SETBROWSE		1616
#define B_CLEARSET		1617
#define B_PR_PRESET		1618
#define B_PR_PANO		1619
#define B_PR_NTSC		1620

#define B_IS_FTYPE		1622 /* FTYPE is nomore */
#define B_IS_BACKBUF		1623
#define B_PR_PC			1624

#define B_PR_PANO360    	1627
#define B_PR_HALFFIELDS		1628
#define B_NEWRENDERPIPE 	1629
#define B_R_SCALE       	1630
#define B_G_SCALE       	1631
#define B_B_SCALE       	1632
#define B_USE_R_SCALE   	1633
#define B_USE_G_SCALE   	1634
#define B_USE_B_SCALE   	1635
#define B_EDGECOLSLI    	1636
#define B_GAMMASLI      	1637

#define B_FILETYPEMENU  	1638
#define B_SELECTCODEC   	1639
#define B_RTCHANGED		1640
#define B_SWITCHRENDER		1641
#define B_FBUF_REDO		1642

#define B_SET_EDGE			1643
#define B_SET_ZBLUR			1644
#define B_ADD_RENDERLAYER	1645
#define B_SET_PASS			1646
#define B_ADD_FFMPEG_VIDEO_OPTION     1647
#define B_ADD_FFMPEG_AUDIO_OPTION     1648

#define B_SEQ_BUT_PLUGIN	1691
#define B_SEQ_BUT_RELOAD	1692
#define B_SEQ_BUT_EFFECT	1693
#define B_SEQ_BUT_RELOAD_ALL    1694
#define B_SEQ_BUT_TRANSFORM     1695
#define B_SEQ_BUT_RELOAD_FILE   1696
#define B_SEQ_BUT_REBUILD_PROXY 1697
#define B_SEQ_SEL_PROXY_DIR     1698
/* *********************** */
#define B_ARMATUREBUTS		1800
#define	B_POSE			1701

/* *********************** */
#define B_COMMONEDITBUTS	2049

#define B_CHANGEDEP		2002
#define B_MATWICH		2003
#define B_MATNEW		2004
#define B_MATDEL		2005
#define B_MATASS		2006
#define B_MATSEL		2007
#define B_MATDESEL		2008
#define B_HIDE			2009
#define B_REVEAL		2010
#define B_SELSWAP		2011
#define B_SETSMOOTH		2012
#define B_SETSOLID		2013
#define B_AUTOTEX		2014
#define B_DOCENTER		2015
#define B_DOCENTERNEW		2016
#define B_DOCENTERCURSOR	2017
#define B_MATASS_BROWSE	2018

	/* 20 values! */
#define B_OBLAY			2019

#define B_ADDKEY		2041
#define B_SETKEY		2042
#define B_DELKEY		2043
#define B_NAMEKEY		2044
#define B_PREVKEY		2045
#define B_NEXTKEY		2046
#define B_LOCKKEY		2047
#define B_MATCOL2		2048

#define B_MESHBUTS		2090

#define B_FLIPNORM		2050
#define B_SPIN			2051
#define B_SPINDUP		2052
#define B_EXTR			2053
#define B_SCREW			2054
#define B_EXTREP		2055
#define B_SPLIT			2056
#define B_REMDOUB		2057
#define B_SUBDIV		2058
#define B_FRACSUBDIV	2059
#define B_XSORT			2060
#define B_HASH			2061
#define B_MAKESTICKY	2062
#define B_DELSTICKY		2063
#define B_NEWMCOL		2064
#define B_DELMCOL		2065
#define B_TOSPHERE			2066
#define B_DECIM_FACES		2067
#define B_DECIM_CANCEL		2068
#define B_DECIM_APPLY		2069
/* B_SLOWERDRAW and B_FASTERDRAW removed */
#define B_VERTEXNOISE		2072
#define B_VERTEXSMOOTH		2073
#define B_NEWTFACE			2074
#define B_DELTFACE			2075
#define B_CHROMADEPTH		2076
#define B_DRAWEDGES			2077
#define B_DRAWCREASES		2078
#define B_SETTFACE			2079
#define B_SETMCOL			2080
#define B_JOINTRIA			2081
#define B_SETTFACE_RND		2082
#define B_SETMCOL_RND		2083
#define B_DRAWBWEIGHTS		2084

#define B_GEN_SKELETON		2090

/* *********************** */
#define B_VGROUPBUTS		2100

#define B_NEWVGROUP			2091
#define B_DELVGROUP			2092
#define B_ASSIGNVGROUP		2093
#define B_REMOVEVGROUP		2094
#define B_SELVGROUP			2095	
#define B_DESELVGROUP		2096
#define B_AUTOVGROUP		2097
#define B_LINKEDVGROUP		2098
#define B_COPYVGROUP 2099



/* *********************** */
#define B_CURVEBUTS		2200

#define B_CONVERTPOLY		2101
#define B_CONVERTBEZ		2102
#define B_CONVERTBSPL		2103
#define B_CONVERTCARD		2104
#define B_CONVERTNURB		2105
#define B_UNIFU			2106
#define B_ENDPU			2107
#define B_BEZU			2108
#define B_UNIFV			2109
#define B_ENDPV			2110
#define B_BEZV			2111
#define B_SETWEIGHT		2112
#define B_SETW1			2113
#define B_SETW2			2114
#define B_SETW3			2115
#define B_SETORDER		2116
#define B_MAKEDISP		2117
#define B_SUBDIVCURVE		2118
#define B_SPINNURB		2119
#define B_CU3D			2120
#define B_SETRESOLU		2121
#define B_SETW4			2122
#define B_SUBSURFTYPE		2123
#define B_TILTINTERP		2124
#define B_SETPT_AUTO	2125
#define B_SETPT_VECTOR	2126
#define B_SETPT_ALIGN	2127
#define B_SETPT_FREE	2128

/* *********************** */
#define B_FONTBUTS		2300

#define B_MAKEFONT		2201
#define B_TOUPPER		2202
#define B_SETFONT		2203
#define B_LOADFONT		2204
#define B_TEXTONCURVE		2205
#define B_PACKFONT		2206
#define B_LOAD3DTEXT		2207
#define B_LOREM			2208
#define B_FASTFONT		2209
#define B_INSTB			2210
#define B_DELTB			2211
#define B_STYLETOSELB		2212
#define B_STYLETOSELU		2213
#define B_STYLETOSELI		2214

#define B_SETCHAR		2215
#define B_SETUPCHAR		2216
#define B_SETDOWNCHAR		2217
#define B_SETCAT		2218
#define B_SETUNITEXT		2219

/* *********************** */
#define B_ARMBUTS		2400

#define B_ARM_RECALCDATA	2301
#define B_ARM_STRIDE		2302
#define B_ARM_CALCPATHS		2303
#define B_ARM_CLEARPATHS	2304

#define B_POSELIB_VALIDATE		2310
#define B_POSELIB_ADDPOSE		2311
#define B_POSELIB_REPLACEP		2312
#define B_POSELIB_REMOVEP		2313
#define B_POSELIB_APPLYP		2314

	/* these shouldn't be here... */
#define B_POSELIB_BROWSE		2320
#define B_POSELIB_ALONE			2321
#define B_POSELIB_DELETE		2322


#define B_POSEGRP_RECALC	2330
#define B_POSEGRP_ADD		2331
#define B_POSEGRP_REMOVE	2332
#define B_POSEGRP_MCUSTOM	2333

/* *********************** */
#define B_CAMBUTS		2500

/* *********************** */
#define B_MBALLBUTS		2600

#define B_RECALCMBALL		2501

/* *********************** */
#define B_LATTBUTS		2700

#define B_RESIZELAT		2601
#define B_DRAWLAT		2602
#define B_LATTCHANGED		2603
#define B_REGULARLAT		2604

/* *********************** */
#define B_GAMEBUTS		2800

#define B_ADD_PROP		2701
#define B_CHANGE_PROP		2702

#define B_ADD_SENS		2703
#define B_CHANGE_SENS		2704
#define B_DEL_SENS		2705

#define B_ADD_CONT		2706
#define B_CHANGE_CONT		2707
#define B_DEL_CONT		2708

#define B_ADD_ACT		2709
#define B_CHANGE_ACT		2710
#define B_DEL_ACT		2711

#define B_SOUNDACT_BROWSE	2712

#define B_SETSECTOR		2713
#define B_SETPROP		2714
#define B_SETACTOR		2715
#define B_SETMAINACTOR		2716
#define B_SETDYNA		2717
#define B_SET_STATE_BIT	2718
#define B_INIT_STATE_BIT	2719

/* *********************** */
#define B_FPAINTBUTS		2900

#define B_VPCOLSLI		2801
#define B_VPGAMMA		2802
#define B_COPY_TF_TRANSP	2803
#define B_COPY_TF_MODE		2804
#define B_COPY_TF_UV		2805
#define B_COPY_TF_COL		2806
#define B_REDR_3D_IMA		2807
#define B_SET_VCOL		2808

#define B_COPY_TF_TEX		2814
#define B_TFACE_HALO		2815
#define B_TFACE_BILLB		2816

#define B_SHOWTEX		2832
#define B_ASSIGNMESH		2833

#define B_WEIGHT0_0		2840
#define B_WEIGHT1_4		2841
#define B_WEIGHT1_2		2842
#define B_WEIGHT3_4		2843
#define B_WEIGHT1_0		2844

#define B_OPA1_8		2845
#define B_OPA1_4		2846
#define B_OPA1_2		2847
#define B_OPA3_4		2848
#define B_OPA1_0		2849

#define B_CLR_WPAINT		2850

#define B_BRUSHBROWSE	2851
#define B_BRUSHDELETE	2852
#define B_BRUSHLOCAL	2853
#define B_BRUSHCHANGE	2854
#define B_BTEXBROWSE	2855
#define B_BTEXDELETE	2856
#define B_BRUSHKEEPDATA 2857

/* Sculptmode */
#define B_SCULPT_TEXBROWSE      2860

/* Particles */
#define B_BAKE_OLENGTH		2870
#define B_BAKE_APPLY_AV		2871
#define B_BAKE_KEYTIME		2872
#define B_BAKE_AV_CHANGE	2873
#define B_BAKE_REDRAWEDIT	2874
#define B_BAKE_RECACHE		2875

/* *********************** */
#define B_RADIOBUTS		3000

#define B_RAD_GO		2901
#define B_RAD_INIT		2902
#define B_RAD_LIMITS		2903
#define B_RAD_FAC		2904
#define B_RAD_NODELIM		2905
#define B_RAD_NODEFILT		2906
#define B_RAD_FACEFILT		2907
#define B_RAD_ADD		2908
#define B_RAD_DELETE		2909
#define B_RAD_COLLECT		2910
#define B_RAD_SHOOTP		2911
#define B_RAD_SHOOTE		2912
#define B_RAD_REPLACE		2913
#define B_RAD_DRAW		2914
#define B_RAD_FREE		2915
#define B_RAD_ADDMESH		2916

/* *********************** */
#define B_SCRIPTBUTS		3100

#define B_SCRIPT_ADD		3001
#define B_SCRIPT_DEL		3002
#define B_SCRIPT_TYPE		3003

/* Scene script buttons */
#define B_SSCRIPT_ADD		3004
#define B_SSCRIPT_DEL		3005
#define B_SSCRIPT_TYPE		3006

/* *********************** */
#define B_SOUNDBUTS		3200
enum B_SOUND_BUTTONS {
	B_SOUND_CHANGED = 3101,
		B_SOUND_REDRAW,
		B_SOUND_VOLUME,
		B_SOUND_PANNING,
		B_SOUND_PITCH,
		B_SOUND_LOAD_SAMPLE,
		B_SOUND_MENU_SAMPLE,
		B_SOUND_NAME_SAMPLE,
		B_SOUND_UNLINK_SAMPLE,
		B_SOUND_RELOAD_SAMPLE,
		B_SOUND_UNPACK_SAMPLE,
		B_SOUND_PLAY_SAMPLE,
		B_SOUND_COPY_SOUND,
		B_SOUND_LOOPSTART,
		B_SOUND_LOOPEND,
		B_SOUND_BIDIRECTIONAL,
		B_SOUND_RECALC,
		B_SOUND_RATECHANGED,
		B_SOUND_MIXDOWN
};

/* *********************** */
#define B_CONSTRAINTBUTS	3300
enum {
	B_CONSTRAINT_TEST = 3201,
	B_CONSTRAINT_CHANGETARGET,
	B_CONSTRAINT_ADD_NULL,
	B_CONSTRAINT_ADD_KINEMATIC,
	B_CONSTRAINT_ADD_TRACKTO,
	B_CONSTRAINT_ADD_MINMAX,
	B_CONSTRAINT_ADD_ROTLIKE,
	B_CONSTRAINT_ADD_LOCLIKE,
	B_CONSTRAINT_ADD_SIZELIKE,
	B_CONSTRAINT_ADD_ACTION,
	B_CONSTRAINT_ADD_LOCKTRACK,
	B_CONSTRAINT_ADD_FOLLOWPATH,
	B_CONSTRAINT_ADD_DISTLIMIT,
	B_CONSTRAINT_ADD_STRETCHTO,
	B_CONSTRAINT_ADD_LOCLIMIT,
	B_CONSTRAINT_ADD_ROTLIMIT,
	B_CONSTRAINT_ADD_SIZELIMIT,
	B_CONSTRAINT_ADD_RIGIDBODYJOINT,
	B_CONSTRAINT_ADD_CHILDOF,
	B_CONSTRAINT_ADD_PYTHON,
	B_CONSTRAINT_ADD_CLAMPTO,
	B_CONSTRAINT_ADD_TRANSFORM,
	B_CONSTRAINT_INF
};

/* *********************** */
#define B_UVAUTOCALCBUTS	3400
enum {
	B_UVAUTO_REDRAW = 3301,
	B_UVAUTO_SPHERE,
	B_UVAUTO_CYLINDER,
	B_UVAUTO_CYLRADIUS,
	B_UVAUTO_WINDOW,
	B_UVAUTO_CUBE,
	B_UVAUTO_CUBESIZE,
	B_UVAUTO_RESET,
	B_UVAUTO_BOUNDS,
	B_UVAUTO_TOP,
	B_UVAUTO_FACE,
	B_UVAUTO_OBJECT,
	B_UVAUTO_ALIGNX,
	B_UVAUTO_ALIGNY,
	B_UVAUTO_UNWRAP,
	B_UVAUTO_DRAWFACES
};

#define B_EFFECTSBUTS		3500

#define B_AUTOTIMEOFS		3403 /* see B_OFSTIMEOFS, B_RANDTIMEOFS also */
#define B_FRAMEMAP		3404
#define B_PREVEFFECT		3406
#define B_CHANGEEFFECT		3408
#define B_RECALCAL		3411
#define B_RECALC_DEFL		3412
#define B_FIELD_DEP		3414
#define B_FIELD_CHANGE		3415
#define	B_PARTBROWSE		3418
#define B_PARTDELETE		3419
#define B_PARTALONE			3420
#define B_PARTLOCAL			3421
#define B_PARTAUTONAME		3422
#define B_PART_ALLOC		3423
#define B_PART_DISTR		3424
#define B_PART_INIT			3425
#define B_PART_RECALC		3426
#define B_PART_REDRAW		3427
#define B_PARTTYPE			3428
#define B_PARTACT			3429
#define B_PARTTARGET		3430
#define B_PART_ALLOC_CHILD	3431
#define B_PART_DISTR_CHILD	3432
#define B_PART_INIT_CHILD	3433
#define B_PART_RECALC_CHILD	3434
#define B_PART_EDITABLE		3435
#define B_PART_REKEY		3436
#define B_PART_ENABLE		3437
#define B_OFSTIMEOFS		3438 /* see B_AUTOTIMEOFS too */
#define B_RANDTIMEOFS		3439
#define B_PART_REDRAW_DEPS	3440

#define B_MODIFIER_BUTS		3600

#define B_MODIFIER_RECALC	3501
#define B_MODIFIER_REDRAW	3502

/* *********************** */
#define B_NODE_BUTS			4000
		/* 400 slots reserved, we want an exec event for each node */
#define B_NODE_LOADIMAGE	3601
#define B_NODE_SETIMAGE		3602
#define B_NODE_TREE_EXEC	3603


		/* exec should be last in this list */
#define B_NODE_EXEC			3610


/* *********************** */
/*  BUTTON 4001-4032: layers? (sort this out!) */

/* *********************** */
/* event code 0x4000 (16384) and larger: general events (redraws, etc) */


#endif

