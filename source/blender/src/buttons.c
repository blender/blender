/**
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
 * Everything for drawing buttons (and I do mean _everything_).
 */


/* System includes ----------------------------------------------------- */

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#include "BLI_winstuff.h"
#else
#include <unistd.h>
#endif
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_ika_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_radio_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_ika.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_plugin_types.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_writeavi.h"

/* Everything from source (BIF, BDR, BSE) ------------------------------ */ 

#include "BDR_drawobject.h"
#include "BDR_editcurve.h"
#include "BDR_editface.h"
#include "BDR_editobject.h"
#include "BDR_vpaint.h"

#include "BSE_drawview.h"
#include "BSE_editipo.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"
#include "BSE_buttons.h"

#include "BIF_gl.h"
#include "BIF_editarmature.h"	
#include "BIF_editconstraint.h"	
#include "BIF_editdeform.h"
#include "BIF_editfont.h"
#include "BIF_editmesh.h"
#include "BIF_editsca.h"
#include "BIF_editsound.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_scrarea.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_previewrender.h"
#include "BIF_writeimage.h"
#include "BIF_writeavicodec.h"
#ifdef WITH_QUICKTIME
#include "quicktime_export.h"
#endif

/* 'old' stuff": defines and types ------------------------------------- */
#include "blendef.h"
#include "interface.h"

/* old style modules --------------------------------------------------- */

#include "mydevice.h"

#include "render.h"
#include "radio.h"
#include "nla.h"			/* For __NLA: Do not remove! */

/* Decimation includes. See LOD_DependKludge.h for enabling Decimation   */
#include "LOD_DependKludge.h"
#ifdef NAN_DECIMATION
  #include "LOD_decimation.h"
#endif

/* own include --------------------------------------------------------- */
#include "BSE_buttons.h"

/* some dirt ... let the linker deal with it :( ------------------------ */
extern ListBase editNurb;  /* from editcurve */
extern VPaint Gvp;         /* from vpaint */

/* Local vars ---------------------------------------------------------- */
short bgpicmode=0, near=1000, far=1000;
short degr= 90, step= 9, turn= 1, editbutflag= 1;
float hspeed=0.1f, prspeed=0.0f, prlen=0.0f, doublimit= 0.001f;
int decim_faces=0;

#ifdef __NLA
float editbutvweight=1;
#endif
float extr_offs= 1.0, editbutweight=1.0, editbutsize=0.1, cumapsize= 1.0;
MTex emptytex;
char texstr[15][8]= {"None"  , "Clouds" , "Wood",
					 "Marble", "Magic"  , "Blend",
					 "Stucci", "Noise"  , "Image",
					 "Plugin", "EnvMap" , "",
					 ""      , ""       , ""};


/* Local functions ----------------------------------------------------- */

/* event for buttons (ROW) to indicate the backbuffer isn't OK (ogl) */
#define B_DIFF			1	

/* *********************** */
#define B_VIEWBUTS		1100

#define B_LOADBGPIC		1001
#define B_BLENDBGPIC	1002
#define B_BGPICBROWSE	1003
#define B_BGPICTEX		1004
#define B_BGPICCLEAR	1005
#define B_BGPICTEXCLEAR	1006

/* *********************** */
#define B_LAMPBUTS		1200

#define B_LAMPREDRAW	1101
#define B_COLLAMP		1102
#define B_TEXCLEARLAMP	1103
#define B_SBUFF			1104

/* *********************** */
#define B_MATBUTS		1300

#define B_MATCOL		1201
#define B_SPECCOL		1202
#define B_MIRCOL		1203
#define B_ACTCOL		1204
#define B_MATFROM		1205
#define B_MATPRV		1206
#define B_MTEXCOL		1207
#define B_TEXCLEAR		1208
#define B_MATPRV_DRAW	1209
#define B_MTEXPASTE		1210
#define B_MTEXCOPY		1211
#define B_MATLAY		1212

/* *********************** */
#define B_TEXBUTS		1400

#define B_TEXTYPE		1301
#define B_DEFTEXVAR		1302
#define B_LOADTEXIMA	1303
#define B_NAMEIMA		1304
#define B_TEXCHANNEL	1305
#define B_TEXREDR_PRV	1306
#define B_TEXIMABROWSE	1307
#define B_IMAPTEST		1308
#define B_RELOADIMA		1309
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
#define B_PLUGBUT		1321

/* plugbut reserves 24 buttons at least! */

#define B_ENV_MAKE		1350
#define B_ENV_FREE		1351
#define B_ENV_DELETE	1352
#define B_ENV_SAVE		1353
#define B_ENV_OB		1354

#define B_PACKIMA		1355
#define B_TEXSETFRAMES	1356

/* *********************** */
#define B_ANIMBUTS		1500

#define B_RECALCPATH	1401
#define B_MUL_IPO		1402
#define B_AUTOTIMEOFS	1403
#define B_FRAMEMAP		1404
#define B_NEWEFFECT		1405
#define B_PREVEFFECT	1406
#define B_NEXTEFFECT	1407
#define B_CHANGEEFFECT	1408
#define B_CALCEFFECT	1409
#define B_DELEFFECT		1410
#define B_RECALCAL		1411
#define B_SETSPEED		1412
#define B_PRINTSPEED	1413
#define B_PRINTLEN		1414
#define B_RELKEY		1415

	/* this has MAX_EFFECT settings! Next free define is 1450... */
#define B_SELEFFECT	1430	


/* *********************** */
#define B_WORLDBUTS		1600

#define B_TEXCLEARWORLD	1501

/* *********************** */
#define B_RENDERBUTS	1700

#define B_FS_PIC		1601
#define B_FS_BACKBUF	1602

#define B_FS_FTYPE		1604
#define B_DORENDER		1605
#define B_DOANIM		1606
#define B_PLAYANIM		1607
#define B_PR_PAL		1608
#define B_PR_FULL		1609
#define B_PR_PRV		1610
#define B_PR_CDI		1611
#define B_PR_PAL169		1612
#define B_PR_D2MAC		1613
#define B_PR_MPEG		1614
#define B_REDRAWDISP	1615
#define B_SETBROWSE		1616
#define B_CLEARSET		1617
#define B_PR_PRESET		1618
#define B_PR_PANO		1619
#define B_PR_NTSC		1620

#define B_IS_FTYPE		1622
#define B_IS_BACKBUF	1623
#define B_PR_PC			1624

#define B_PR_PANO360    1627
#define B_PR_HALFFIELDS	1628
#define B_NEWRENDERPIPE 1629
#define B_R_SCALE       1630
#define B_G_SCALE       1631
#define B_B_SCALE       1632
#define B_USE_R_SCALE   1633
#define B_USE_G_SCALE   1634
#define B_USE_B_SCALE   1635
#define B_EDGECOLSLI    1636
#define B_GAMMASLI      1637

#define B_FILETYPEMENU  1638
#define B_SELECTCODEC   1639
#define B_RTCHANGED		1640

#ifdef __NLA
/* *********************** */
enum {
	B_ARMATUREBUTS	=	1800,
	B_POSE			=	1701
};
#endif

/* *********************** */
#define B_COMMONEDITBUTS	2049

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
#define B_DOCENTRE		2015
#define B_DOCENTRENEW	2016
#define B_DOCENTRECURSOR	2017

	/* 32 values! */
#define B_OBLAY			2018

#define B_MESHBUTS		2100

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
#define B_DELSTICKY		2062
#define B_DELVERTCOL	2063
#define B_MAKE_TFACES	2064
#define B_TOSPHERE		2065
#define B_DEL_TFACES	2066
#define B_NEWVGROUP		2067
#define B_DELVGROUP		2068
#define B_ASSIGNVGROUP	2069
#define B_REMOVEVGROUP	2070
#define B_SELVGROUP		2071	
#define B_DESELVGROUP	2072
#define B_DECIM_FACES	2073
#define B_DECIM_CANCEL	2074
#define B_DECIM_APPLY	2075
#define B_AUTOVGROUP	2076
#define B_SLOWERDRAW	2077
#define B_FASTERDRAW	2078
#define B_VERTEXNOISE	2079
#define B_VERTEXSMOOTH	2080
#define B_MAKESTICKY	2082
#define B_MAKEVERTCOL	2083

/* *********************** */
#define B_CURVEBUTS		2200

#define B_CONVERTPOLY	2101
#define B_CONVERTBEZ	2102
#define B_CONVERTBSPL	2103
#define B_CONVERTCARD	2104
#define B_CONVERTNURB	2105
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
#define B_SUBDIVCURVE	2118
#define B_SPINNURB		2119
#define B_CU3D			2120
#define B_SETRESOLU		2121
#define B_SETW4			2122


/* *********************** */
#define B_FONTBUTS		2300

#define B_MAKEFONT		2201
#define B_TOUPPER		2202
#define B_SETFONT		2203
#define B_LOADFONT		2204
#define B_TEXTONCURVE	2205
#define B_PACKFONT		2206

/* *********************** */
#define B_IKABUTS		2400

#define B_IKASETREF		2301
#define B_IKARECALC		2302

/* *********************** */
#define B_CAMBUTS		2500

/* *********************** */
#define B_MBALLBUTS		2600

#define B_RECALCMBALL	2501

/* *********************** */
#define B_LATTBUTS		2700

#define B_RESIZELAT		2601
#define B_DRAWLAT		2602
#define B_LATTCHANGED	2603

/* *********************** */
#define B_GAMEBUTS		2800

/* in editsca.c */

/* *********************** */
#define B_FPAINTBUTS	2900

#define B_VPCOLSLI		2801
#define B_VPGAMMA		2802

#define B_COPY_TF_MODE	2804
#define B_COPY_TF_UV	2805
#define B_COPY_TF_COL	2806
#define B_REDR_3D_IMA	2807
#define B_SET_VCOL		2808

#define B_COPY_TF_TEX	2814
#define B_TFACE_HALO	2815
#define B_TFACE_BILLB	2816

#define B_SHOWTEX		2832
#define B_ASSIGNMESH	2833


/* *********************** */
#define B_RADIOBUTS		3000

#define B_RAD_GO		2901
#define B_RAD_INIT		2902
#define B_RAD_LIMITS	2903
#define B_RAD_FAC		2904
#define B_RAD_NODELIM	2905
#define B_RAD_NODEFILT	2906
#define B_RAD_FACEFILT	2907
#define B_RAD_ADD		2908
#define B_RAD_DELETE	2909
#define B_RAD_COLLECT	2910
#define B_RAD_SHOOTP	2911
#define B_RAD_SHOOTE	2912
#define B_RAD_REPLACE	2913
#define B_RAD_DRAW		2914
#define B_RAD_FREE		2915
#define B_RAD_ADDMESH	2916

/* *********************** */
#define B_SCRIPTBUTS	3100

#define B_SCRIPT_ADD	3001
#define B_SCRIPT_DEL	3002
#define B_SCRIPT_TYPE	3003

/* Scene script buttons */
#define B_SSCRIPT_ADD	3004
#define B_SSCRIPT_DEL	3005
#define B_SSCRIPT_TYPE	3006

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
		B_SOUND_BIDIRECTIONAL
};

/* *********************** */
#define B_CONSTRAINTBUTS	3300
enum {
	B_CONSTRAINT_REDRAW = 3201,
	B_CONSTRAINT_ADD,
	B_CONSTRAINT_DEL,
	B_CONSTRAINT_TEST,
	B_CONSTRAINT_CHANGETYPE,
	B_CONSTRAINT_CHANGENAME,
	B_CONSTRAINT_CHANGETARGET
};

/* *********************** */
/*  BUTTON BUT: > 4000	   */
/*  BUTTON 4001-4032: layers */


static char *physics_pup(void)
{
  /* the number needs to match defines in KX_PhysicsBlenderSceneConverter.cpp */
  return "Physics %t|None %x1|Sumo %x2|"
         "ODE %x3 |Dynamo %x4|";
}


static void draw_buttons_edge(int win, float x1)
{
	float asp, winmat[4][4];
	int w,h;

	bwin_getsinglematrix(win, winmat);
	bwin_getsize(win, &w, &h);
	asp= (float)(2.0/(w*winmat[0][0]));

	glColor3ub(0,0,0);
	fdrawline(x1, -1000, x1, 2000);
	glColor3ub(255,255,255);
	fdrawline(x1+asp, -1000, x1+asp, 2000);
}

static int packdummy = 0;

void test_scriptpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	id= G.main->text.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}
#ifdef __NLA
void test_actionpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	id= G.main->action.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}
#endif

void test_obpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if(idpp == (ID **)&(emptytex.object)) {
		error("Add texture first");
		*idpp= 0;
		return;
	}
	
	id= G.main->object.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_obcurpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if(idpp == (ID **)&(emptytex.object)) {
		error("Add texture first");
		*idpp= 0;
		return;
	}
	
	id= G.main->object.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_CURVE) {
				error ("Bevel object must be a Curve");
				break;
			} 
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_meshpoin_but(char *name, ID **idpp)
{
	ID *id;

	if( *idpp ) (*idpp)->us--;
	
	id= G.main->mesh.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_matpoin_but(char *name, ID **idpp)
{
	ID *id;

	if( *idpp ) (*idpp)->us--;
	
	id= G.main->mat.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_scenepoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if( *idpp ) (*idpp)->us--;
	
	id= G.main->scene.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}



/* ************************************* */

static void do_common_editbuts(unsigned short event)
{
	EditVlak *evl;
	Base *base;
	Object *ob;
	Mesh *me;
	Nurb *nu;
	Curve *cu;
	MFace *mface;
	BezTriple *bezt;
	BPoint *bp;
	unsigned int local;
	int a, bit, index= -1;

	switch(event) {
		
	case B_MATWICH:
		if(G.obedit && G.obedit->actcol>0) {
			if(G.obedit->type == OB_MESH) {
				evl= G.edvl.first;
				while(evl) {
					if( vlakselectedAND(evl, 1) ) {
						if(index== -1) index= evl->mat_nr;
						else if(index!=evl->mat_nr) {
							error("Mixed colors");
							return;
						}
					}
					evl= evl->next;
				}
			}
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
				nu= editNurb.first;
				while(nu) {
					if( isNurbsel(nu) ) {
						if(index== -1) index= nu->mat_nr;
						else if(index!=nu->mat_nr) {
							error("Mixed colors");
							return;
						}
					}
					nu= nu->next;
				}				
			}
			if(index>=0) {
				G.obedit->actcol= index+1;
				scrarea_queue_winredraw(curarea);
			}
		}
		break;
	case B_MATNEW:
		new_material_to_objectdata((G.scene->basact) ? (G.scene->basact->object) : 0);
		scrarea_queue_winredraw(curarea);
		allqueue(REDRAWVIEW3D_Z, 0);
		break;
	case B_MATDEL:
		delete_material_index();
		scrarea_queue_winredraw(curarea);
		allqueue(REDRAWVIEW3D_Z, 0);
		break;
	case B_MATASS:
		if(G.obedit && G.obedit->actcol>0) {
			if(G.obedit->type == OB_MESH) {
				evl= G.edvl.first;
				while(evl) {
					if( vlakselectedAND(evl, 1) )
						evl->mat_nr= G.obedit->actcol-1;
					evl= evl->next;
				}
				allqueue(REDRAWVIEW3D_Z, 0);
				makeDispList(G.obedit);
			}
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
				nu= editNurb.first;
				while(nu) {
					if( isNurbsel(nu) )
						nu->mat_nr= G.obedit->actcol-1;
					nu= nu->next;
				}
			}
		}
		break;
	case B_MATSEL:
	case B_MATDESEL:
		if(G.obedit) {
			if(G.obedit->type == OB_MESH) {
				evl= G.edvl.first;
				while(evl) {
					if(evl->mat_nr== G.obedit->actcol-1) {
						if(event==B_MATSEL) {
							if(evl->v1->h==0) evl->v1->f |= 1;
							if(evl->v2->h==0) evl->v2->f |= 1;
							if(evl->v3->h==0) evl->v3->f |= 1;
							if(evl->v4 && evl->v4->h==0) evl->v4->f |= 1;
						}
						else {
							if(evl->v1->h==0) evl->v1->f &= ~1;
							if(evl->v2->h==0) evl->v2->f &= ~1;
							if(evl->v3->h==0) evl->v3->f &= ~1;
							if(evl->v4 && evl->v4->h==0) evl->v4->f &= ~1;
						}
					}
					evl= evl->next;
				}
				tekenvertices_ext( event==B_MATSEL );
			}
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
				nu= editNurb.first;
				while(nu) {
					if(nu->mat_nr==G.obedit->actcol-1) {
						if(nu->bezt) {
							a= nu->pntsu;
							bezt= nu->bezt;
							while(a--) {
								if(bezt->hide==0) {
									if(event==B_MATSEL) {
										bezt->f1 |= 1;
										bezt->f2 |= 1;
										bezt->f3 |= 1;
									}
									else {
										bezt->f1 &= ~1;
										bezt->f2 &= ~1;
										bezt->f3 &= ~1;
									}
								}
								bezt++;
							}
						}
						else if(nu->bp) {
							a= nu->pntsu*nu->pntsv;
							bp= nu->bp;
							while(a--) {
								if(bp->hide==0) {
									if(event==B_MATSEL) bp->f1 |= 1;
									else bp->f1 &= ~1;
								}
								bp++;
							}
						}
					}
					nu= nu->next;
				}
				allqueue(REDRAWVIEW3D, 0);
			}
		}
		break;
	case B_HIDE:
		if(G.obedit) {
			if(G.obedit->type == OB_MESH) hide_mesh(0);
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) hideNurb(0);
		}
		break;
	case B_REVEAL:
		if(G.obedit) {
			if(G.obedit->type == OB_MESH) reveal_mesh();
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) revealNurb();
		}
		else if(G.f & G_FACESELECT) reveal_tface();
		
		break;
	case B_SELSWAP:
		if(G.obedit) {
			if(G.obedit->type == OB_MESH) selectswap_mesh();
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) selectswapNurb();
		}
		break;
	case B_AUTOTEX:
		ob= OBACT;
		if(ob && G.obedit==0) {
			if(ob->type==OB_MESH) tex_space_mesh(ob->data);
			else if(ob->type==OB_MBALL) ;
			else tex_space_curve(ob->data);
		}
		break;
	case B_DOCENTRE:
		docentre();
		break;
	case B_DOCENTRENEW:
		docentre_new();
		break;
	case B_DOCENTRECURSOR:
		docentre_cursor();
		break;
	case B_SETSMOOTH:
	case B_SETSOLID:
		if(G.obedit) {
			if(G.obedit->type == OB_MESH) {
				evl= G.edvl.first;
				while(evl) {
					if( vlakselectedAND(evl, 1) ) {
						if(event==B_SETSMOOTH) evl->flag |= ME_SMOOTH;
						else evl->flag &= ~ME_SMOOTH;
					}
					evl= evl->next;
				}

				makeDispList(G.obedit);
				allqueue(REDRAWVIEW3D, 0);
			}
			else {
				nu= editNurb.first;
				while(nu) {
					if(isNurbsel(nu)) {
						if(event==B_SETSMOOTH) nu->flag |= ME_SMOOTH;
						else nu->flag &= ~ME_SMOOTH;
					}
					nu= nu->next;
				}
				
			}
		}
		else {
			base= FIRSTBASE;
			while(base) {
				if(TESTBASELIB(base)) {
					if(base->object->type==OB_MESH) {
						me= base->object->data;
						mface= me->mface;
						for(a=0; a<me->totface; a++, mface++) {
							if(event==B_SETSMOOTH) mface->flag |= ME_SMOOTH;
							else mface->flag &= ~ME_SMOOTH;
						}

						makeDispList(base->object);
					}
					else if ELEM(base->object->type, OB_SURF, OB_CURVE) {
						cu= base->object->data;
						nu= cu->nurb.first;
						while(nu) {
							if(event==B_SETSMOOTH) nu->flag |= ME_SMOOTH;
							else nu->flag &= ~ME_SMOOTH;
							nu= nu->next;
						}
					}
				}
				base= base->next;
			}
			allqueue(REDRAWVIEW3D, 0);
		}
		break;

	default:
		if(event>=B_OBLAY && event<=B_OBLAY+31) {
			local= BASACT->lay & 0xFF000000;
			BASACT->lay -= local;
			if(BASACT->lay==0 || (G.qual & LR_SHIFTKEY)==0) {
				bit= event-B_OBLAY;
				BASACT->lay= 1<<bit;
				scrarea_queue_winredraw(curarea);
			}
			BASACT->lay += local;
			/* optimal redraw */
			if( (OBACT->lay & G.vd->lay) && (BASACT->lay & G.vd->lay) );
			else if( (OBACT->lay & G.vd->lay)==0 && (BASACT->lay & G.vd->lay)==0 );
			else allqueue(REDRAWVIEW3D, 0);
			
			OBACT->lay= BASACT->lay;
		}
	}

}

void common_editbuts(void)
{
	Object *ob;
	ID *id;
	Material *ma;
	uiBlock *block;
	void *poin;
	float min;
	int xco, a, dx, dy;
	char str[32];
	
	ob= OBACT;
	if(ob==0) return;
	
	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	/* LAYERS */
	xco= 291;
	dx= 32;
	dy= 30;
	for(a=0; a<10; a++) {
		/* the (a+10) evaluates correctly because of
           precedence... brackets aren't a bad idea though */
		uiDefButI(block, TOG|BIT|(a+10), B_OBLAY+a+10, "",	(short)(xco+a*(dx/2)), 180, (short)(dx/2), (short)(dy/2), &(BASACT->lay), 0, 0, 0, 0, "");
		uiDefButI(block, TOG|BIT|a, B_OBLAY+a, "",(short)(xco+a*(dx/2)), (short)(180+dy/2), (short)(dx/2), (short)(1+dy/2), &(BASACT->lay), 0, 0, 0, 0, "");
		if(a==4) xco+= 5;
	}

	id= ob->data;
	if(id && id->lib) uiSetButLock(1, "Can't edit library data");

	uiBlockSetCol(block, BUTGREY);
	uiDefBut(block, LABEL, 0, "Drawtype",						28,200,100,18, 0, 0, 0, 0, 0, "");
	uiDefButC(block, MENU, REDRAWVIEW3D, "Drawtype%t|Bounds %x1|Wire %x2|Solid %x3|Shaded %x4",	
																28,180,100,18, &ob->dt, 0, 0, 0, 0, "Drawtype menu");
	uiDefBut(block, LABEL, 0, "Draw Extra",						28,160,100,18, 0, 0, 0, 0, 0, "");
	uiDefButC(block, TOG|BIT|0, REDRAWVIEW3D, "Bounds",		28, 140, 100, 18, &ob->dtx, 0, 0, 0, 0, "Display bounding object");
	uiDefButS(block, MENU, REDRAWVIEW3D, "Bounding volume%t|Box%x0|Sphere%x1|Cylinder%x2|Cone%x3|Polyheder",
																28, 120, 100, 18, &ob->boundtype, 0, 0, 0, 0, "Choose between bound objects");
	uiDefButC(block, TOG|BIT|5, REDRAWVIEW3D, "Wire",		28, 100, 100, 18, &ob->dtx, 0, 0, 0, 0, "Display wireframe in shaded mode");
	uiDefButC(block, TOG|BIT|1, REDRAWVIEW3D, "Axis",		28, 80, 100, 18, &ob->dtx, 0, 0, 0, 0, "Draw axis");
	uiDefButC(block, TOG|BIT|2, REDRAWVIEW3D, "TexSpace",	28, 60, 100, 18, &ob->dtx, 0, 0, 0, 0, "Display texture space");
	uiDefButC(block, TOG|BIT|3, REDRAWVIEW3D, "Name",		28, 40, 100, 18, &ob->dtx, 0, 0, 0, 0, "Print object name");
	
	uiBlockSetCol(block, BUTGREY);
	
	/* material and select swap and hide */
	if ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL) {
		
			if(ob->type==OB_MESH) poin= &( ((Mesh *)ob->data)->texflag );
			else if(ob->type==OB_MBALL) poin= &( ((MetaBall *)ob->data)->texflag );
			else poin= &( ((Curve *)ob->data)->texflag );
			uiDefButI(block, TOG|BIT|0, B_AUTOTEX, "AutoTexSpace",	143,180,130,19, poin, 0, 0, 0, 0, "To switch automatic calculation of texture space");

		sprintf(str,"%d Mat:", ob->totcol);
		if(ob->totcol) min= 1.0; else min= 0.0;
		ma= give_current_material(ob, ob->actcol);
		
		if(ma) {
			uiDefButF(block, COL, 0, "",			291,123,24,30, &(ma->r), 0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, ma->id.name+2, 318,146, 103, 30, 0, 0, 0, 0, 0, "");
		}
		uiDefButC(block, NUM, B_REDR,	str,		318,123,103,30, &ob->actcol, min, (float)(ob->totcol), 0, 0, "Total indices, active index");
		uiDefBut(block, BUT,B_MATWICH,	"?",		423,123,31,30, 0, 0, 0, 0, 0, "In EditMode, sets the active material index from selected faces");
		
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT,B_MATNEW,	"New",		292,101,80,21, 0, 0, 0, 0, 0, "Add a new Material index");
		uiDefBut(block, BUT,B_MATDEL,	"Delete",	374,101,80,21, 0, 0, 0, 0, 0, "Delete this Material index");
		uiDefBut(block, BUT,B_MATASS,	"Assign",	291,47,162,26, 0, 0, 0, 0, 0, "In EditMode, assign the active index to selected faces");

		uiBlockSetCol(block, BUTGREY);
		uiDefBut(block, BUT,B_MATSEL,	"Select",	292,76,79,22, 0, 0, 0, 0, 0, "In EditMode, select faces that have the active index");
		uiDefBut(block, BUT,B_MATDESEL,	"Deselect",	373,76,79,21, 0, 0, 0, 0, 0, "Deselect everything with current indexnumber");
		
		if(ob->type!=OB_FONT) {
			uiDefBut(block, BUT,B_HIDE,		"Hide",		1091,152,77,18, 0, 0, 0, 0, 0, "Hide selected faces");
			uiDefBut(block, BUT,B_REVEAL,	"Reveal",	1171,152,86,18, 0, 0, 0, 0, 0, "Reveal selected faces");
			uiDefBut(block, BUT,B_SELSWAP,	"Select Swap",	1091,129,166,18, 0, 0, 0, 0, 0, "Select not-selected, and deselect selected faces");
		}
		uiDefBut(block, BUT,B_SETSMOOTH,	"Set Smooth",	291,15,80,20, 0, 0, 0, 0, 0, "In EditMode: set 'smooth' rendering of selected faces");
		uiDefBut(block, BUT,B_SETSOLID,	"Set Solid",	373,15,80,20, 0, 0, 0, 0, 0, "In EditMode: set 'solid' rendering of selected faces");

	}
	
	if ELEM3(ob->type, OB_MESH, OB_SURF, OB_CURVE) {
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT,B_DOCENTRE, "Centre",				961, 115, 100, 19, 0, 0, 0, 0, 0, "Shift object data to be centered about object's origin");
		uiDefBut(block, BUT,B_DOCENTRENEW, "Centre New",			961, 95, 100, 19, 0, 0, 0, 0, 0, "Shift object's origin to center of object data");
		uiDefBut(block, BUT,B_DOCENTRECURSOR, "Centre Cursor",		961, 75, 100, 19, 0, 0, 0, 0, 0, "Shift object's origin to cursor location");
	}

	
	uiDrawBlock(block);
	
}



/* *************************** MESH ******************************** */

#ifdef NAN_DECIMATION

static int decimate_count_tria(Object *ob)
{
	int tottria;
	MFace *mface;
	Mesh *me;
	int a;
	
	me= ob->data;
	
	/* count number of trias, since decimator doesnt allow quads */
	tottria= 0;
	mface= me->mface;
	for(a=0; a<me->totface; a++, mface++) {
		if(mface->v4) tottria++;
		if(mface->v3) tottria++;		
	}
	
	return tottria;
}

static void decimate_faces(void)
{
	Object *ob;
	Mesh *me;
	MVert *mvert;
	MFace *mface;
	LOD_Decimation_Info lod; 
	float *vb=NULL;
	float *vnb=NULL;
	int *tib=NULL;
	int a, tottria;
	
	/* we assume the active object being decimated */
	ob= OBACT;
	if(ob==NULL || ob->type!=OB_MESH) return;
	me= ob->data;

	/* add warning for vertex col and tfaces */
	if(me->tface || me->mcol) {
		if(okee("This will remove UV coords and vertexcolors")==0) return;
		if(me->tface) MEM_freeN(me->tface);
		if(me->mcol) MEM_freeN(me->mcol);
		me->tface= NULL;
		me->mcol= NULL;
	}
	
	/* count number of trias, since decimator doesnt allow quads */
	tottria= decimate_count_tria(ob);

	if(tottria<3) {
		error("Need more input faces than just 3");
		return;
	}
	/* allocate and init */
	lod.vertex_buffer= MEM_mallocN(3*sizeof(float)*me->totvert, "vertices");
	lod.vertex_normal_buffer= MEM_mallocN(3*sizeof(float)*me->totvert, "normals");
	lod.triangle_index_buffer= MEM_mallocN(3*sizeof(int)*tottria, "trias");
	lod.vertex_num= me->totvert;
	lod.face_num= tottria;
	
	/* fill vertex buffer */
	vb= lod.vertex_buffer;
	vnb= lod.vertex_normal_buffer;
	mvert= me->mvert;
	for(a=0; a<me->totvert; a++, mvert++, vb+=3, vnb+=3) {
		VECCOPY(vb, mvert->co);
		VECCOPY(vnb, mvert->no);
		Normalise(vnb);
	}
	
	/* fill index buffer */
	mface= me->mface;
	tib= lod.triangle_index_buffer;
	for(a=0; a<me->totface; a++, mface++) {
		if(mface->v4) {
			tib[0]= mface->v1;
			tib[1]= mface->v3;
			tib[2]= mface->v4;
			tib+= 3;
		}
		if(mface->v3) {
			tib[0]= mface->v1;
			tib[1]= mface->v2;
			tib[2]= mface->v3;
			tib+= 3;
		}
	}

	if(LOD_LoadMesh(&lod) ) {
		if( LOD_PreprocessMesh(&lod) ) {
			DispList *dl;
			DispListMesh *dlm;
			MFaceInt *mfaceint;
			
			/* we assume the decim_faces tells how much to reduce */
			
			while(lod.face_num > decim_faces) {
				if( LOD_CollapseEdge(&lod)==0) break;
			}

			/* ok, put back the stuff in a displist */
			freedisplist(&(ob->disp));
			dl= MEM_callocN(sizeof(DispList), "disp");
			BLI_addtail(&ob->disp, dl);
			dl->type= DL_MESH;
			dlm=dl->mesh= MEM_callocN(sizeof(DispListMesh), "dispmesh");
			dlm->mvert= MEM_callocN(lod.vertex_num*sizeof(MVert), "mvert");
			dlm->mface= MEM_callocN(lod.face_num*sizeof(MFaceInt), "mface");
			dlm->totvert= lod.vertex_num;
			dlm->totface= lod.face_num;
			
			mvert= dlm->mvert;
			vb= lod.vertex_buffer;
			for(a=0; a<lod.vertex_num; a++, vb+=3, mvert++) {
				VECCOPY(mvert->co, vb);
			}
			
			mfaceint= dlm->mface;
			tib= lod.triangle_index_buffer;
			for(a=0; a<lod.face_num; a++, mfaceint++, tib+=3) {
				mfaceint->v1= tib[0];
				mfaceint->v2= tib[1];
				mfaceint->v3= tib[2];
			}
		}
		else error("No memory");

		LOD_FreeDecimationData(&lod);
	}
	else error("No manifold Mesh");
	
	MEM_freeN(lod.vertex_buffer);
	MEM_freeN(lod.vertex_normal_buffer);
	MEM_freeN(lod.triangle_index_buffer);
	
	allqueue(REDRAWVIEW3D, 0);
}



static void decimate_cancel(void)
{
	Object *ob;
	
	ob= OBACT;
	if(ob) {
		freedisplist(&ob->disp);
		makeDispList(ob);
	}
	allqueue(REDRAWVIEW3D, 0);
}

static void decimate_apply(void)
{
	Object *ob;
	DispList *dl;
	DispListMesh *dlm;
	Mesh *me;
	MFace *mface;
	MFaceInt *mfaceint;
	int a;
	
	if(G.obedit) return;
	
	ob= OBACT;
	if(ob) {
		dl= ob->disp.first;
		if(dl && dl->mesh) {
			dlm= dl->mesh;
			me= ob->data;
			
			// vertices 
			if(me->mvert) MEM_freeN(me->mvert);
			me->mvert= dlm->mvert;
			dlm->mvert= NULL;
			me->totvert= dlm->totvert;
			
			// faces
			if(me->mface) MEM_freeN(me->mface);
			me->mface= MEM_callocN(dlm->totface*sizeof(MFace), "mface");
			me->totface= dlm->totface;
			mface= me->mface;
			mfaceint= dlm->mface;
			for(a=0; a<me->totface; a++, mface++, mfaceint++) {
				mface->v1= mfaceint->v1;
				mface->v2= mfaceint->v2;
				mface->v3= mfaceint->v3;
				test_index_mface(mface, 3);
			}
			
			freedisplist(&ob->disp);
			
			G.obedit= ob;
			make_editMesh();
			load_editMesh();
			free_editMesh();
			G.obedit= NULL;
			tex_space_mesh(me);
		}
		else error("Not a decimated Mesh");
	}
}

#endif

void do_meshbuts(unsigned short event)
{
	Object *ob;
	Mesh *me;
	float fac;
	short randfac;

	ob= OBACT;
	if(ob && ob->type==OB_MESH) {
		
		me= get_mesh(ob);
		if(me==0) return;
		
		switch(event) {
#ifdef __NLA
		case B_AUTOVGROUP:
			if (!get_armature(ob->parent)){
				error ("Mesh must be the child of an armature");
				break;
			}
				/* Verify that there are vertex groups for bones in armature */
				/* Remove selected vertices from all defgroups */
				/* Perform assignment for selected vertices */

			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_NEWVGROUP:
			add_defgroup (G.obedit);
			scrarea_queue_winredraw(curarea);
			break;
		case B_DELVGROUP:
			del_defgroup (G.obedit);
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_ASSIGNVGROUP:
			assign_verts_defgroup ();
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_REMOVEVGROUP:
			remove_verts_defgroup (0);
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_SELVGROUP:
			sel_verts_defgroup(1);
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_DESELVGROUP:
			sel_verts_defgroup(0);
			allqueue (REDRAWVIEW3D, 1);
			break;
#endif
		case B_DELSTICKY:
		
			if(me->msticky) MEM_freeN(me->msticky);
			me->msticky= 0;
			allqueue(REDRAWBUTSEDIT, 0);
			break;
		case B_MAKESTICKY:
			make_sticky();
			break;
		case B_MAKEVERTCOL:
			make_vertexcol();
			break;
		case B_DELVERTCOL:
			if(me->mcol) MEM_freeN(me->mcol);
			me->mcol= 0;
			G.f &= ~G_VERTEXPAINT;
			freedisplist(&(ob->disp));
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
			break;

		case B_MAKE_TFACES:
			make_tfaces(me);
			allqueue(REDRAWBUTSEDIT, 0);
			break;

		case B_DEL_TFACES:
			if(me->tface) MEM_freeN(me->tface);
			me->tface= 0;
			G.f &= ~G_FACESELECT;
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
			break;
			
		case B_FLIPNORM:
			if(G.obedit) {
				flip_editnormals();
			}
			else flipnorm_mesh( get_mesh(ob) );
			
			allqueue(REDRAWVIEW3D, 0);
			break;

		case B_DECIM_FACES:
			decimate_faces();
			break;
		case B_DECIM_CANCEL:
			decimate_cancel();
			break;
		case B_DECIM_APPLY:
			decimate_apply();
			break;

		case B_SLOWERDRAW:
			slowerdraw();
			break;
		case B_FASTERDRAW:
			fasterdraw();
			break;
		}
	}
	
	if(G.obedit==0 || (G.obedit->type!=OB_MESH)) return;
	
	switch(event) {
	case B_SPIN:
		if( select_area(SPACE_VIEW3D)) spin_mesh(step, degr, 0, 0);
		break;
	case B_SPINDUP:
		if( select_area(SPACE_VIEW3D)) spin_mesh(step, degr, 0, 1);
		break;
	case B_EXTR:
		G.f |= G_DISABLE_OK;
		if( select_area(SPACE_VIEW3D)) extrude_mesh();
		G.f -= G_DISABLE_OK;
		break;
	case B_SCREW:
		if( select_area(SPACE_VIEW3D)) screw_mesh(step, turn);
		break;
	case B_EXTREP:
		if( select_area(SPACE_VIEW3D)) extrude_repeat_mesh(step, extr_offs);
		break;
	case B_SPLIT:
		G.f |= G_DISABLE_OK;
		split_mesh();
		G.f -= G_DISABLE_OK;
		break;
	case B_REMDOUB:
		notice("Removed: %d", removedoublesflag(1, doublimit));
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_SUBDIV:
		waitcursor(1);
		subdivideflag(1, 0.0, editbutflag & B_BEAUTY);
		countall();
		waitcursor(0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_FRACSUBDIV:
		randfac= 10;
		if(button(&randfac, 1, 100, "Rand fac:")==0) return;
		waitcursor(1);
		fac= -( (float)randfac )/100;
		subdivideflag(1, fac, editbutflag & B_BEAUTY);
		countall();
		waitcursor(0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_XSORT:
		if( select_area(SPACE_VIEW3D)) xsortvert_flag(1);
		break;
	case B_HASH:
		hashvert_flag(1);
		break;
	case B_TOSPHERE:
		vertices_to_sphere();
		break;
	case B_VERTEXNOISE:
		vertexnoise();
		break;
	case B_VERTEXSMOOTH:
		vertexsmooth();
		break;
	}
	/* WATCH IT: previous events only in editmode! */
}

static void verify_vertexgroup_name_func(void *datav, void *data2_unused)
{
	unique_vertexgroup_name((bDeformGroup*)datav, OBACT);
}

void meshbuts(void)
{
	Object *ob;
	Mesh *me;
	uiBlock *block;
	uiBut *but;
	float val;
	char str[64];
#ifdef __NLA
	int by;
	float	min;
	int	defCount;
	bDeformGroup	*defGroup;
#endif

	ob= OBACT;
	if(ob==0) return;
	
	sprintf(str, "editbuttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	me= get_mesh(ob);
	
	if(me) {
		uiDefButS(block, TOG|BIT|1, REDRAWVIEW3D, "No V.Normal Flip",	143,160,130,18, &me->flag, 0, 0, 0, 0, "Disable flipping of vertexnormals during render");
		uiBlockSetCol(block, BUTGREEN);
		uiDefButS(block, TOG|BIT|5, REDRAWVIEW3D, "Auto Smooth",	143,140,130,18, &me->flag, 0, 0, 0, 0, "Automatic detection of smooth rendered faces during render");
		uiBlockSetCol(block, BUTGREY);
		uiDefButS(block, NUM, B_DIFF, "Degr:",							143, 120, 130, 18, &me->smoothresh, 1, 80, 0, 0, "Maximum angle (between face normals) that defines smooth rendering");
		uiBlockSetCol(block, BUTGREEN);
		uiDefButS(block, TOG|BIT|7, B_MAKEDISP, "SubSurf",	143,100,130,18, &me->flag, 0, 0, 0, 0, "Catmull-Clark Subdivision Surface");
		uiBlockSetCol(block, BUTGREY);
		uiDefButS(block, NUM, B_MAKEDISP, "Subdiv:",			143, 80, 100, 18, &me->subdiv, 0, 12, 0, 0, "Level of subdivision for interactive display");
		uiDefButS(block, NUM, B_MAKEDISP, "",				243, 80, 30, 18, &me->subdivr, 0, 12, 0, 0, "Level of subdivision for rendering");
		uiDefButS(block, TOG|BIT|2, REDRAWVIEW3D, "Double Sided",	1090,184,164,19, &me->flag, 0, 0, 0, 0, "Make faces doublesided");
		
		uiBlockSetCol(block, BUTSALMON);
		
		if(me->msticky) val= 1.0; else val= 0.0;
		uiDefBut(block, LABEL, 0, "Sticky", 137,55,70,20, 0, val, 0, 0, 0, "");
		if(me->msticky==0) {
			uiDefBut(block, BUT, B_MAKESTICKY, "Make",	210,58,63,19, 0, 0, 0, 0, 0, "Make sticky texture coords (projected from view)");
		}
		else uiDefBut(block, BUT, B_DELSTICKY, "Delete", 210,58,63,19, 0, 0, 0, 0, 0, "Delete sticky texture coords");
	
		if(me->mcol) val= 1.0; else val= 0.0;
		uiDefBut(block, LABEL, 0, "VertCol", 140,33,70,20, 0, val, 0, 0, 0, "");
		if(me->mcol==0) {
			uiDefBut(block, BUT, B_MAKEVERTCOL, "Make",	209,36,64,19, 0, 0, 0, 0, 0, "Enable vertex colours");
		}
		else uiDefBut(block, BUT, B_DELVERTCOL, "Delete", 209,36,64,19, 0, 0, 0, 0, 0, "");

			if(me->tface) val= 1.0; else val= 0.0;
			uiDefBut(block, LABEL, 0, "TexFace", 142,13,70,20, 0, val, 0, 0, 0, "");
			if(me->tface==0) {
				uiDefBut(block, BUT, B_MAKE_TFACES, "Make",	209,14,64,20, 0, 0, 0, 0, 0, "Enable texture face");
			}
			else uiDefBut(block, BUT, B_DEL_TFACES, "Delete", 209,14,64,20, 0, 0, 0, 0, 0, "Delete texture face");
		
		uiBlockSetCol(block, BUTGREY);
	
		uiDefIDPoinBut(block, test_meshpoin_but, 0, "TexMesh:",		477,185,249,19, &me->texcomesh, "Enter the name of a Meshblock");
	}		
	

	/* EDIT */
		
	if(me) {
#ifdef NAN_DECIMATION
		int tottria= decimate_count_tria(ob);
		DispList *dl;

		// wacko, wait for new displist system (ton)
		if( (dl=ob->disp.first) && dl->mesh);
		else decim_faces= tottria;

		uiBlockSetCol(block, BUTPURPLE);
		uiDefButI(block, NUMSLI,B_DECIM_FACES, "Decimator",	477,155,249,20, &decim_faces, 4.0, tottria, 10, 10, "The number of triangles to reduce to");
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT,B_DECIM_CANCEL, "Cancel",	477,135,124,19, 0, 0, 0, 0, 0, "restore Mesh");
		uiDefBut(block, BUT,B_DECIM_APPLY, "Apply",		602,135,124,19, 0, 0, 0, 0, 0, "apply decimation to Mesh");
#endif
	
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT,B_EXTR,"Extrude",	477,100,249,24, 0, 0, 0, 0, 0, "Convert selected edges to faces");
		uiDefBut(block, BUT,B_SPINDUP,"Spin Dup",	639,75,87,24, 0, 0, 0, 0, 0, "Use spin with duplication tool");
		uiDefBut(block, BUT,B_SPIN, "Spin",		558,75,78,24, 0, 0, 0, 0, 0, "Use spin tool");
		uiDefBut(block, BUT,B_SCREW,"Screw",		477,75,79,24, 0, 0, 0, 0, 0, "Use screw tool");
		uiDefBut(block, BUT,B_EXTREP, "ExtrudeRepeat",477,15,128,19, 0, 0, 0, 0, 0, "Create a repetitive extrude along a straight line");
	
		uiBlockSetCol(block, BUTGREY);
		uiDefButS(block, NUM, B_DIFF, "Degr:",		477,55,78,19, &degr,10.0,360.0, 0, 0, "Specify the number of degrees the spin revolves");
		uiDefButS(block, NUM, B_DIFF, "Steps:",		558,55,78,19, &step,1.0,180.0, 0, 0, "Specify the total number of spin revolutions");
		uiDefButS(block, NUM, B_DIFF, "Turns:",		639,55,86,19, &turn,1.0,360.0, 0, 0, "Specify the number of revolutions the screw turns");
		uiDefButS(block, TOG|BIT|0, B_DIFF, "Clockwise",	639,35,86,19, &editbutflag, 0, 0, 0, 0, "Specify the direction for screw and spin");
		uiDefButS(block, TOG|BIT|1, B_DIFF, "Keep Original",	477,35,156,19, &editbutflag, 0, 0, 0, 0, "Seperate original and new vertices and faces");
		uiDefButF(block, NUM, B_DIFF, "Offset:",		608,15,117,19, &extr_offs, 0.01, 10.0, 100, 0, "Set the distance between each step of the extrude repeat");
	}

	by=206;

	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|2, 0, "Beauty",	847,by-=20,94,19, &editbutflag, 0, 0, 0, 0, "Split face in halves");
	uiBlockSetCol(block, BUTSALMON);

	uiDefBut(block, BUT,B_SPLIT,"Split",			847,by-=19,94,18, 0, 0, 0, 0, 0, "Split msh without removing faces");
	uiDefBut(block, BUT,B_TOSPHERE,"To Sphere",	847,by-=19,94,18, 0, 0, 0, 0, 0, "Blow vertices up into spherical shape");
	uiDefBut(block, BUT,B_SUBDIV,"Subdivide",	847,by-=19,94,18, 0, 0, 0, 0, 0, "Split face in quarters");
	uiDefBut(block, BUT,B_FRACSUBDIV, "Fract Subd",847,by-=19,94,18, 0, 0, 0, 0, 0, "Split face with random factor");
	
	uiDefBut(block, BUT,B_VERTEXNOISE,"Noise",				847,by-=19,94,18, 0, 0, 0, 0, 0, "Use vertex coordinate as texture coordinate");
	uiDefBut(block, BUT,B_VERTEXSMOOTH,"Smooth",				847,by-=19,94,18, 0, 0, 0, 0, 0, "Flatten angels");
	uiDefBut(block, BUT,B_XSORT,"Xsort",			847,by-=19,94,18, 0, 0, 0, 0, 0, "Sort vertices in the X direction");
	uiDefBut(block, BUT,B_HASH,"Hash",			847,by-=19,94,18, 0, 0, 0, 0, 0, "Randomize vertices sequence");

	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUM, B_DIFF, "Limit:",			959,151,100,19, &doublimit, 0.0001, 1.0, 10, 0, "Specify the limit in distance to remove doubles");

	uiBlockSetCol(block, BUTSALMON);
	
	uiDefBut(block, BUT,B_REMDOUB,"Rem Doubles",	958,173,101,32, 0, 0, 0, 0, 0, "Remove doubles");

	uiDefBut(block, BUT,B_FLIPNORM,"Flip Normals",		961,55,100,19, 0, 0, 0, 0, 0, "Toggle the direction of the face normals");

	uiDefBut(block, BUT, B_SLOWERDRAW,"SlowerDraw",			961,35,100,19, 0, 0, 0, 0, 0, "Draw slow but accurate");
	uiDefBut(block, BUT, B_FASTERDRAW,"FasterDraw",			961,15,100,19, 0, 0, 0, 0, 0, "Draw fast but less accurate");

#ifdef __NLA
		
		/* Draw Vertex grouping buttons if we're in editmode*/
	if (ob){
		char *s, *menustr;
		bDeformGroup *dg;
		int index;
		
		by = 210;
		uiBlockSetCol(block, BUTGREY);
		uiDefBut(block, LABEL,0,"Vertex Groups",			740,by-=19,93,18, 0, 0, 0, 0, 0, "");

		defCount=BLI_countlist(&ob->defbase);

		if (!defCount)
			min=0;
		else
			min=1;
				
#if 0
		sprintf (str, "%d Group:", defCount);
		uiDefButS(block, NUM, REDRAWBUTSEDIT, str,	740, by-=22,93,18, &ob->actdef, min, defCount, 0, 0, "");
#else
		s= menustr = MEM_callocN((32 * defCount)+20, "menustr");

		for (index = 1, dg = ob->defbase.first; dg; index++, dg=dg->next){
			int cnt= sprintf (s, "%s%%x%d|", dg->name, index);
			
			if (cnt>0)
				s+= cnt;
		}
		
		by-=22;
		if (defCount)
			uiDefButS(block, MENU, REDRAWBUTSEDIT, menustr,	740, by,18,18, &ob->actdef, min, defCount, 0, 0, "Active deformation group");
		MEM_freeN (menustr);
#endif
		if (ob->actdef){
			defGroup = BLI_findlink(&ob->defbase, ob->actdef-1);
			but= uiDefBut(block, TEX,REDRAWBUTSEDIT,"",			758,by,93-18,18, defGroup->name, 0, 32, 0, 0, "Change the current deformations group's name (and bone affiliation)");
			uiButSetFunc(but, verify_vertexgroup_name_func, defGroup, NULL);
		}
		uiDefButF(block, NUM, REDRAWVIEW3D, "Weight:",		740, by-=22, 93, 18, &editbutvweight, 0, 1, 10, 0, "Change the bone's deformation strength");

	}
		
	if (G.obedit && G.obedit==ob){

		uiBlockSetCol(block, BUTSALMON);
/*		uiDefBut(block, BUT,B_AUTOVGROUP,"Auto Weight",			740,by-=22,93,18, 0, 0, 0, 0, 0, "Automatically assign deformation groups"); */
		uiDefBut(block, BUT,B_NEWVGROUP,"New",				740,by-=22,45,18, 0, 0, 0, 0, 0, "Create a new deformation group");
		uiDefBut(block, BUT,B_DELVGROUP,"Delete",			788,by,45,18, 0, 0, 0, 0, 0, "Remove the current deformation group");

		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT,B_ASSIGNVGROUP,"Assign",			740,by-=22,93,18, 0, 0, 0, 0, 0, "Assign selected vertices to the current deformation group");
		uiDefBut(block, BUT,B_REMOVEVGROUP,"Remove",			740,by-=22,93,18, 0, 0, 0, 0, 0, "Remove selected vertices from the current deformation group");

		uiBlockSetCol(block, BUTGREY);
		uiDefBut(block, BUT,B_SELVGROUP,"Select",			740,by-=22,93,18, 0, 0, 0, 0, 0, "Select vertices belonging to the current deformation group");
		uiDefBut(block, BUT,B_DESELVGROUP,"Deselect",			740,by-=22,93,18, 0, 0, 0, 0, 0, "Deselect vertices belonging to the current deformation group");

}
#endif

	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUM,		  REDRAWVIEW3D, "NSize:",		1090, 90, 164, 19, &editbutsize, 0.001, 2.0, 10, 0, "Set the length of the face normals");
	uiDefButI(block, TOG|BIT|6, REDRAWVIEW3D, "Draw Normals",	1090,70,164,19, &G.f, 0, 0, 0, 0, "Draw face normals");
	uiDefButI(block, TOG|BIT|7, REDRAWVIEW3D, "Draw Faces",	1090,50,164,19, &G.f, 0, 0, 0, 0, "Draw faces");
	uiDefButI(block, TOG|BIT|11, 0, "All edges",				1090,10,164,19, &G.f, 0, 0, 0, 0, "Draw edges normally, without optimisation");

	uiDrawBlock(block);
}

/* *************************** FONT ******************************** */

short give_vfontnr(VFont *vfont)
{
	VFont *vf;
	short nr= 1;

	vf= G.main->vfont.first;
	while(vf) {
		if(vf==vfont) return nr;
		nr++;
		vf= vf->id.next;
	}
	return -1;
}

VFont *give_vfontpointer(int nr)	/* nr= button */
{
	VFont *vf;
	short tel= 1;

	vf= G.main->vfont.first;
	while(vf) {
		if(tel==nr) return vf;
		tel++;
		vf= vf->id.next;
	}
	return G.main->vfont.first;
}

VFont *exist_vfont(char *str)
{
	VFont *vf;

	vf= G.main->vfont.first;
	while(vf) {
		if(strcmp(vf->name, str)==0) return vf;
		vf= vf->id.next;
	}
	return 0;
}

static char *give_vfontbutstr(void)
{
	VFont *vf;
	int len= 0;
	char *str, di[FILE_MAXDIR], fi[FILE_MAXFILE];

	vf= G.main->vfont.first;
	while(vf) {
		strcpy(di, vf->name);
		BLI_splitdirstring(di, fi);
		len+= strlen(fi)+4;
		vf= vf->id.next;
	}
	
	str= MEM_callocN(len+21, "vfontbutstr");
	strcpy(str, "FONTS %t");
	vf= G.main->vfont.first;
	while(vf) {
		
		if(vf->id.us==0) strcat(str, "|0 ");
		else strcat(str, "|   ");
		
		strcpy(di, vf->name);
		BLI_splitdirstring(di, fi);
		
		strcat(str, fi);
		vf= vf->id.next;
	}
	return str;
}

void load_buts_vfont(char *name)
{
	VFont *vf;
	Curve *cu;
	
	if(OBACT && OBACT->type==OB_FONT) cu= OBACT->data;
	else return;
	
	vf= exist_vfont(name);
	if(vf==0) {
		vf= load_vfont(name);
		if(vf==0) return;
	}
	else id_us_plus((ID *)vf);
	
	if(cu->vfont) cu->vfont->id.us--;
	cu->vfont= vf;
	
	text_to_curve(OBACT, 0);
	makeDispList(OBACT);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

void do_fontbuts(unsigned short event)
{
	Curve *cu;
	VFont *vf;
	Object *ob;
	ScrArea *sa;
	char str[80];
	
	ob= OBACT;
	
	switch(event) {
	case B_MAKEFONT:
		text_to_curve(ob, 0);
		makeDispList(ob);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_TOUPPER:
		to_upper();
		break;
	case B_LOADFONT:
		vf= give_vfontpointer(G.buts->texnr);
		if(vf && vf->id.prev!=vf->id.next) strcpy(str, vf->name);
		else strcpy(str, U.fontdir);
		
		sa= closest_bigger_area();
		areawinset(sa->win);

		activate_fileselect(FILE_SPECIAL, "SELECT FONT", str, load_buts_vfont);

		break;
	case B_PACKFONT:
		if (ob) {
			cu= ob->data;
			if(cu && cu->vfont) {
				if (cu->vfont->packedfile) {
					if (G.fileflags & G_AUTOPACK) {
						if (okee("Disable AutoPack ?")) {
							G.fileflags &= ~G_AUTOPACK;
						}
					}
					
					if ((G.fileflags & G_AUTOPACK) == 0) {
						if (unpackVFont(cu->vfont, PF_ASK) == RET_OK) {
							text_to_curve(ob, 0);
							makeDispList(ob);
							allqueue(REDRAWVIEW3D, 0);
						}
					}
				} else {
					cu->vfont->packedfile = newPackedFile(cu->vfont->name);
				}
			}
		}
		allqueue(REDRAWHEADERS, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;

	case B_SETFONT:
		if(ob) {
			cu= ob->data;

			vf= give_vfontpointer(G.buts->texnr);
			if(vf) {
				id_us_plus((ID *)vf);
				cu->vfont->id.us--;
				cu->vfont= vf;
				text_to_curve(ob, 0);
				makeDispList(ob);
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWBUTSEDIT, 0);
			}
		}	
		break;
	case B_TEXTONCURVE:
		if(ob) {
			cu= ob->data;
			if(cu->textoncurve && cu->textoncurve->type!=OB_CURVE) {
				error("Only Curve Objects");
				cu->textoncurve= 0;
				allqueue(REDRAWBUTSEDIT, 0);
			}
			text_to_curve(ob, 0);
			makeDispList(ob);
		}
	}
}



void fontbuts(void)
{
	Curve *cu;
	uiBlock *block;
	char *strp, str[64];
	
	if(OBACT==0) return;

	sprintf(str, "editbuttonswin1 %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	cu= OBACT->data;

	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, ROW,B_MAKEFONT, "Left",		484,139,53,18, &cu->spacemode, 0.0,0.0, 0, 0, "");
	uiDefButS(block, ROW,B_MAKEFONT, "Middle",	604,139,61,18, &cu->spacemode, 0.0,1.0, 0, 0, "");
	uiDefButS(block, ROW,B_MAKEFONT, "Right",		540,139,62,18, &cu->spacemode, 0.0,2.0, 0, 0, "");
	uiDefButS(block, ROW,B_MAKEFONT, "Flush",		665,139,61,18, &cu->spacemode, 0.0,3.0, 0, 0, "");

	uiBlockSetCol(block, BUTGREY);

	uiDefIDPoinBut(block, test_obpoin_but, B_TEXTONCURVE, "TextOnCurve:",	484,115,243,19, &cu->textoncurve, "");

	uiDefButF(block, NUM,B_MAKEFONT, "Size:",		482,56,121,19, &cu->fsize, 0.1,10.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "Linedist:",	605,56,121,19, &cu->linedist, 0.0,10.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "Spacing:",	482,34,121,19, &cu->spacing, 0.0,10.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "Y offset:",	605,34,121,19, &cu->yof, -50.0,50.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "Shear:",	482,12,121,19, &cu->shear, -1.0,1.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "X offset:",	605,12,121,19, &cu->xof, -50.0,50.0, 10, 0, "");

	uiDefBut(block, TEX,REDRAWVIEW3D, "Ob Family:",	752,192,164,19, cu->family, 0.0, 20.0, 0, 0, "");

	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT, B_TOUPPER, "ToUpper",		623,163,103,23, 0, 0, 0, 0, 0, "");
	
	uiBlockSetCol(block, BUTGREY);

	G.buts->texnr= give_vfontnr(cu->vfont);
	
	strp= give_vfontbutstr();
	
	uiDefButS(block, MENU, B_SETFONT, strp, 484,191,220,20, &G.buts->texnr, 0, 0, 0, 0, "");
	
	if (cu->vfont->packedfile) {
		packdummy = 1;
	} else {
		packdummy = 0;
	}
	
	uiBlockSetCol(block, BUTYELLOW);
	uiDefIconButI(block, TOG|BIT|0, B_PACKFONT, ICON_PACKAGE,	706,191,20,20, &packdummy, 0, 0, 0, 0, "Pack/Unpack this Vectorfont");
	
	MEM_freeN(strp);
	
	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT,B_LOADFONT, "Load Font",	484,163,103,23, 0, 0, 0, 0, 0, "");
	
	uiDrawBlock(block);
}

/* *************************** CURVE ******************************** */


void do_curvebuts(unsigned short event)
{
	extern Nurb *lastnu;
	Object *ob;
	Curve *cu;
	Nurb *nu;
	
	ob= OBACT;
	if(ob==0) return;
	
	switch(event) {	

	case B_CONVERTPOLY:
	case B_CONVERTBEZ:
	case B_CONVERTBSPL:
	case B_CONVERTCARD:
	case B_CONVERTNURB:
		if(G.obedit) {
			setsplinetype(event-B_CONVERTPOLY);
			makeDispList(G.obedit);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_UNIFU:
	case B_ENDPU:
	case B_BEZU:
	case B_UNIFV:
	case B_ENDPV:
	case B_BEZV:
		if(G.obedit) {
			nu= editNurb.first;
			while(nu) {
				if(isNurbsel(nu)) {
					if((nu->type & 7)==CU_NURBS) {
						if(event<B_UNIFV) {
							nu->flagu &= 1;
							nu->flagu += ((event-B_UNIFU)<<1);
							makeknots(nu, 1, nu->flagu>>1);
						}
						else if(nu->pntsv>1) {
							nu->flagv &= 1;
							nu->flagv += ((event-B_UNIFV)<<1);
							makeknots(nu, 2, nu->flagv>>1);
						}
					}
				}
				nu= nu->next;
			}
			makeDispList(G.obedit);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SETWEIGHT:
		if(G.obedit) {
			weightflagNurb(1, editbutweight, 0);
			makeDispList(G.obedit);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SETW1:
		editbutweight= 1.0;
		scrarea_queue_winredraw(curarea);
		break;
	case B_SETW2:
		editbutweight= sqrt(2.0)/4.0;
		scrarea_queue_winredraw(curarea);
		break;
	case B_SETW3:
		editbutweight= 0.25;
		scrarea_queue_winredraw(curarea);
		break;
	case B_SETW4:
		editbutweight= sqrt(0.5);
		scrarea_queue_winredraw(curarea);
		break;
	case B_SETORDER:
		if(G.obedit) {
			nu= lastnu;
			if(nu && (nu->type & 7)==CU_NURBS ) {
				if(nu->orderu>nu->pntsu) {
					nu->orderu= nu->pntsu;
					scrarea_queue_winredraw(curarea);
				}
				makeknots(nu, 1, nu->flagu>>1);
				if(nu->orderv>nu->pntsv) {
					nu->orderv= nu->pntsv;
					scrarea_queue_winredraw(curarea);
				}
				makeknots(nu, 2, nu->flagv>>1);
			}
			makeDispList(G.obedit);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_MAKEDISP:
		if(ob->type==OB_FONT) text_to_curve(ob, 0);
		makeDispList(ob);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */
		break;
	
	case B_SUBDIVCURVE:
		subdivideNurb();
		break;
	case B_SPINNURB:
		/* bad bad bad!!! use brackets!!! In case you wondered:
		  {==,!=} goes before & goes before || */
		if( (G.obedit==0) || 
		    (G.obedit->type!=OB_SURF) || 
			((G.obedit->lay & G.vd->lay) == 0) ) return;
		spinNurb(0, 0);
		countall();
		makeDispList(G.obedit);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_CU3D:	    /* allow 3D curve */
		if(G.obedit) {
			cu= G.obedit->data;
			nu= editNurb.first;
			while(nu) {
				nu->type &= ~CU_2D;
				if((cu->flag & CU_3D)==0) nu->type |= CU_2D;
				test2DNurb(nu);
				nu= nu->next;
			}
		}
		if(ob->type==OB_CURVE) {
			cu= ob->data;
			nu= cu->nurb.first;
			while(nu) {
				nu->type &= ~CU_2D;
				if((cu->flag & CU_3D)==0) nu->type |= CU_2D;
				test2DNurb(nu);
				nu= nu->next;
			}
		}
		break;
	case B_SETRESOLU:
		if(ob->type==OB_CURVE) {
			cu= ob->data;
			if(ob==G.obedit) nu= editNurb.first;
			else nu= cu->nurb.first;
			
			while(nu) {
				nu->resolu= cu->resolu;
				nu= nu->next;
			}
		}
		else if(ob->type==OB_FONT) text_to_curve(ob, 0);
		
		makeDispList(ob);
		allqueue(REDRAWVIEW3D, 0);

		break;
	}
}

void curvebuts(void)
{
	Object *ob;
	Curve *cu;
	Nurb *nu;
	extern Nurb *lastnu;
	uiBlock *block;
	short *sp;
	char str[64];
	
	ob= OBACT;
	if(ob==0) return;

	sprintf(str, "editbuttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	cu= ob->data;

	if(ob->type==OB_CURVE || ob->type==OB_SURF) {
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, LABEL, 0, "Convert",	463,173,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTPOLY,"Poly",		467,152,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTBEZ,"Bezier",	467,132,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTBSPL,"Bspline",	467,112,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTCARD,"Cardinal",	467,92,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTNURB,"Nurb",		467,72,72, 18, 0, 0, 0, 0, 0, "");
	
		uiDefBut(block, LABEL, 0, "Make Knots",562,173,102, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_UNIFU,"Uniform U",	565,152,102, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_ENDPU,"Endpoint U",	565,132,102, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_BEZU,"Bezier U",	565,112,102, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_UNIFV,"V",		670,152,50, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_ENDPV,"V",		670,132,50, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_BEZV,"V",		670,112,50, 18, 0, 0, 0, 0, 0, "");
	
		uiDefBut(block, BUT,B_SETWEIGHT,"Set Weight",	465,11,95,49, 0, 0, 0, 0, 0, "");
		uiBlockSetCol(block, BUTGREY);
		uiDefButF(block, NUM,0,"Weight:",	564,36,102,22, &editbutweight, 0.01, 10.0, 10, 0, "");
		uiDefBut(block, BUT,B_SETW1,"1.0",		669,36,50,22, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_SETW2,"sqrt(2)/4",	564,11,57,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_SETW3,"0.25",		621,11,43,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_SETW4,"sqrt(0.5)",	664,11,57,20, 0, 0, 0, 0, 0, "");
		
		if(ob==G.obedit) {
			nu= lastnu;
			if(nu==0) nu= editNurb.first;
			if(nu) sp= &(nu->orderu); 
			else sp= 0;
			uiDefButS(block, NUM, B_SETORDER, "Order U:", 565,91,102, 18, sp, 2.0, 6.0, 0, 0, "");
			if(nu) sp= &(nu->orderv); 
			else sp= 0;
			uiDefButS(block, NUM, B_SETORDER, "V:",	 670,91,50, 18, sp, 2.0, 6.0, 0, 0, "");
			if(nu) sp= &(nu->resolu); 
			else sp= 0;
			uiDefButS(block, NUM, B_MAKEDISP, "Resol U:", 565,70,102, 18, sp, 1.0, 128.0, 0, 0, "");
			if(nu) sp= &(nu->resolv); 
			else sp= 0;
			uiDefButS(block, NUM, B_MAKEDISP, "V:", 670,70,50, 18, sp, 1.0, 128.0, 0, 0, "");
		}

		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_SUBDIVCURVE, "Subdivide",	1092,105,165,20, 0, 0, 0, 0, 0, "");
	}

	if(ob->type==OB_SURF) {
		uiDefBut(block, BUT, B_SPINNURB, "Spin",	808,92,101,36, 0, 0, 0, 0, 0, "");

		uiBlockSetCol(block, BUTGREY);
		uiDefButS(block, TOG|BIT|5, 0, "UV Orco",					143,160,130,18, &cu->flag, 0, 0, 0, 0, "");
		uiDefButS(block, TOG|BIT|6, REDRAWVIEW3D, "No Puno Flip",	143,140,130,18, &cu->flag, 0, 0, 0, 0, "");
	}
	else {

		uiBlockSetCol(block, BUTGREY);
		uiDefButS(block, TOG|BIT|5, 0, "UV Orco",			143,160,130,18, &cu->flag, 0, 0, 0, 0, "");
		
		uiDefButS(block, NUM, B_MAKEDISP, "DefResolU:",	752,163,132,21, &cu->resolu, 1.0, 128.0, 0, 0, "");
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_SETRESOLU, "Set",				887,163,29,21, 0, 0, 0, 0, 0, "");
		
		uiBlockSetCol(block, BUTGREY);
		uiDefButS(block, NUM, B_MAKEDISP, "BevResol:",	753,30,163,18, &cu->bevresol, 0.0, 10.0, 0, 0, "");

		uiDefIDPoinBut(block, test_obcurpoin_but, B_MAKEDISP, "BevOb:",		753,10,163,18, &cu->bevobj, "");
		uiDefButF(block, NUM, B_MAKEDISP, "Width:",		753,90,163,18, &cu->width, 0.0, 2.0, 1, 0, "");
		uiDefButF(block, NUM, B_MAKEDISP, "Ext1:",		753,70,163,18, &cu->ext1, 0.0, 5.0, 10, 0, "");
		uiDefButF(block, NUM, B_MAKEDISP, "Ext2:",		753,50,163,18, &cu->ext2, 0.0, 2.0, 1, 0, "");
		uiBlockSetCol(block, BUTBLUE);
		if(ob->type==OB_FONT) {
			uiDefButS(block, TOG|BIT|1, B_MAKEDISP, "Front",	833,130,79,18, &cu->flag, 0, 0, 0, 0, "");
			uiDefButS(block, TOG|BIT|2, B_MAKEDISP, "Back",	753,130,76,18, &cu->flag, 0, 0, 0, 0, "");
		}
		else {
			uiDefButS(block, TOG|BIT|0, B_CU3D, "3D",			867,130,47,18, &cu->flag, 0, 0, 0, 0, "");
			uiDefButS(block, TOG|BIT|1, B_MAKEDISP, "Front",	810,130,55,18, &cu->flag, 0, 0, 0, 0, "");
			uiDefButS(block, TOG|BIT|2, B_MAKEDISP, "Back",	753,130,53,18, &cu->flag, 0, 0, 0, 0, "");
		}
		uiBlockSetCol(block, BUTGREY);
	}

	uiDefButF(block, NUM,		  REDRAWVIEW3D, "NSize:",		1090, 80, 164, 19, &editbutsize, 0.001, 1.0, 10, 0, "");

	uiDrawBlock(block);
}


/* *************************** CAMERA ******************************** */


void camerabuts(void)
{
	Camera *cam;
	Object *ob;
	uiBlock *block;
	float grid=0.0;
	char str[64];
	
	if(G.vd) grid= G.vd->grid; 
	if(grid<1.0) grid= 1.0;
	
	ob= OBACT;
	if(ob==0) return;

	sprintf(str, "editbuttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	
	cam= ob->data;
	uiDefButF(block, NUM,REDRAWVIEW3D, "Lens:", 470,178,160,20, &cam->lens, 1.0, 250.0, 100, 0, "Specify the lens of the camera");
	uiDefButF(block, NUM,REDRAWVIEW3D, "ClipSta:", 470,147,160,20, &cam->clipsta, 0.001*grid, 100.0*grid, 10, 0, "Specify the startvalue of the the field of view");
	uiDefButF(block, NUM,REDRAWVIEW3D, "ClipEnd:", 470,125,160,20, &cam->clipend, 1.0, 5000.0*grid, 100, 0, "Specify the endvalue of the the field of view");
	uiDefButF(block, NUM,REDRAWVIEW3D, "DrawSize:", 470,90,160,20, &cam->drawsize, 0.1*grid, 10.0, 10, 0, "Specify the drawsize of the camera");

	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG, REDRAWVIEW3D, "Ortho", 470,49,61,40, &cam->type, 0, 0, 0, 0, "Render orthogonally");

	uiDefButS(block, TOG|BIT|0,REDRAWVIEW3D, "ShowLimits", 533,69,97,20, &cam->flag, 0, 0, 0, 0, "Draw the field of view");
	uiDefButS(block, TOG|BIT|1,REDRAWVIEW3D, "Show Mist", 533,49,97,20, &cam->flag, 0, 0, 0, 0, "Draw a line that indicates the mist area");
	
	if(G.special1 & G_HOLO) {
		uiBlockSetCol(block, BUTGREY);
		if(cam->netend==0.0) cam->netend= EFRA;
		uiDefButF(block, NUM, REDRAWVIEW3D, "Anim len",		670,80,100,20, &cam->netend, 1.0, 2500.0, 0, 0, "");
		uiDefButF(block, NUM, REDRAWVIEW3D, "Path len:",		670,160,100,20, &cam->hololen, 0.1, 25.0, 10, 0, "");
		uiDefButF(block, NUM, REDRAWVIEW3D, "Shear fac:",		670,140,100,20, &cam->hololen1, 0.1, 5.0, 10, 0, "");
		uiBlockSetCol(block, BUTGREEN);
		uiDefButS(block, TOG|BIT|4, REDRAWVIEW3D, "Holo 1",	670,120,100,20, &cam->flag, 0.0, 0.0, 0, 0, "");
		uiDefButS(block, TOG|BIT|5, REDRAWVIEW3D, "Holo 2",	670,100,100,20, &cam->flag, 0.0, 0.0, 0, 0, "");
		
	}
	uiDrawBlock(block);
}

/* *************************** FACE/PAINT *************************** */

void do_fpaintbuts(unsigned short event)
{
	Mesh *me;
	Object *ob;
	extern TFace *lasttface; /* caches info on tface bookkeeping ?*/
	
	ob= OBACT;
	if(ob==0) return;

	switch(event) { 
		
	case B_VPGAMMA:
		vpaint_dogamma();
		break;
	case B_COPY_TF_MODE:
	case B_COPY_TF_UV:
	case B_COPY_TF_COL:
	case B_COPY_TF_TEX:
		me= get_mesh(ob);
		if(me && me->tface) {
/*  			extern TFace *lasttface; */
			TFace *tface= me->tface;
			int a= me->totface;
			
			set_lasttface();
			if(lasttface) {
			
				while(a--) {
					if(tface!=lasttface && (tface->flag & TF_SELECT)) {
						if(event==B_COPY_TF_MODE) {
							tface->mode= lasttface->mode;
							tface->transp= lasttface->transp;
						}
						else if(event==B_COPY_TF_UV) {
							memcpy(tface->uv, lasttface->uv, sizeof(tface->uv));
							tface->tpage= lasttface->tpage;
							tface->tile= lasttface->tile;
							
							if(lasttface->mode & TF_TILES) tface->mode |= TF_TILES;
							else tface->mode &= ~TF_TILES;
							
						}
						else if(event==B_COPY_TF_TEX) {
							tface->tpage= lasttface->tpage;
							tface->tile= lasttface->tile;

							if(lasttface->mode & TF_TILES) tface->mode |= TF_TILES;
							else tface->mode &= ~TF_TILES;
						}
						else if(event==B_COPY_TF_COL) memcpy(tface->col, lasttface->col, sizeof(tface->col));
					}
					tface++;
				}
			}
			do_shared_vertexcol(me);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_SET_VCOL:
		clear_vpaint_selectedfaces();
		break;
	case B_REDR_3D_IMA:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
	case B_ASSIGNMESH:
		
		test_object_materials(ob->data);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSGAME, 0);
		break;
		
	case B_TFACE_HALO:
		set_lasttface();
		if(lasttface) {
			lasttface->mode &= ~TF_BILLBOARD2;
			allqueue(REDRAWBUTSGAME, 0);
		}
		break;

	case B_TFACE_BILLB:
		set_lasttface();
		if(lasttface) {
			lasttface->mode &= ~TF_BILLBOARD;
			allqueue(REDRAWBUTSGAME, 0);
		}
		break;
	}	
}

void fpaintbuts(void)
{
/*  	extern VPaint Gvp; already in the top of the file */
	Object *ob;
	uiBlock *block;
	char str[32];
	
	ob= OBACT;
	if(ob==0) return;

	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	/* VPAINT BUTTONS */
	uiBlockSetCol(block, BUTGREY);

	if (G.f & G_VERTEXPAINT) {
		uiDefBut(block, LABEL, 0, "Vertex Paint",	1037,180,194,18, 0, 0, 0, 0, 0, "");
	} else if (G.f & G_TEXTUREPAINT) {
		uiDefBut(block, LABEL, 0, "Texture Paint",	1037,180,194,18, 0, 0, 0, 0, 0, "");
	} 

	uiDefButF(block, NUMSLI, 0, "R ",			979,160,194,19, &Gvp.r, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of red used for painting");
	uiDefButF(block, NUMSLI, 0, "G ",			979,140,194,19, &Gvp.g, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of green used for painting");
	uiDefButF(block, NUMSLI, 0, "B ",			979,120,194,19, &Gvp.b, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of blue used for painting");
	uiDefButF(block, NUMSLI, 0, "Opacity ",		979,100,194,19, &Gvp.a, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
	uiDefButF(block, NUMSLI, 0, "Size ",		979,80,194,19, &Gvp.size, 2.0, 64.0, 0, 0, "The size of the brush");

	uiDefButF(block, COL, B_VPCOLSLI, "",		1176,100,28,80, &(Gvp.r), 0, 0, 0, 0, "");

	uiDefButS(block, ROW, B_DIFF, "Mix",			1212,160,63,19, &Gvp.mode, 1.0, 0.0, 0, 0, "Mix the vertex colours");
	uiDefButS(block, ROW, B_DIFF, "Add",			1212,140,63,19, &Gvp.mode, 1.0, 1.0, 0, 0, "Add the vertex colour");
	uiDefButS(block, ROW, B_DIFF, "Sub",			1212, 120,63,19, &Gvp.mode, 1.0, 2.0, 0, 0, "Subtract from the vertex colour");
	uiDefButS(block, ROW, B_DIFF, "Mul",			1212, 100,63,19, &Gvp.mode, 1.0, 3.0, 0, 0, "Multiply the vertex colour");
	uiDefButS(block, ROW, B_DIFF, "Filter",		1212, 80,63,19, &Gvp.mode, 1.0, 4.0, 0, 0, "Mix the colours with an alpha factor");

	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|1, 0, "Area", 		980,50,80,19, &Gvp.flag, 0, 0, 0, 0, "Set the area the brush covers");
	uiDefButS(block, TOG|BIT|2, 0, "Soft", 		1061,50,112,19, &Gvp.flag, 0, 0, 0, 0, "Use a soft brush");
	uiDefButS(block, TOG|BIT|3, 0, "Normals", 	1174,50,102,19, &Gvp.flag, 0, 0, 0, 0, "Use vertex normal for painting");

	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT, B_VPGAMMA, "Set", 	980,30,80,19, 0, 0, 0, 0, 0, "Apply Mul and Gamma to vertex colours");
	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUM, B_DIFF, "Mul:", 		1061,30,112,19, &Gvp.mul, 0.1, 50.0, 10, 0, "Set the number to multiply vertex colours with");
	uiDefButF(block, NUM, B_DIFF, "Gamma:", 	1174,30,102,19, &Gvp.gamma, 0.1, 5.0, 10, 0, "Change the clarity of the vertex colours");
	
	uiDefBut(block, LABEL, 0, "Face Select",	600,180,194,18, 0, 0, 0, 0, 0, "");
	if(G.f & G_FACESELECT) {
		extern TFace *lasttface;
		
		set_lasttface();
		if(lasttface) {
			
			uiBlockSetCol(block, BUTGREEN);
			uiDefButS(block, TOG|BIT|2, B_REDR_3D_IMA, "Tex",	600,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Render face with texture");
			uiDefButS(block, TOG|BIT|7, B_REDR_3D_IMA, "Tiles",	660,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use tilemode for face");
			uiDefButS(block, TOG|BIT|4, REDRAWVIEW3D, "Light",	720,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use light for face");
			uiDefButS(block, TOG|BIT|10, REDRAWVIEW3D, "Invisible",780,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Make face invisible");
			uiDefButS(block, TOG|BIT|0, REDRAWVIEW3D, "Collision", 840,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use face for collision detection");

			uiDefButS(block, TOG|BIT|6, REDRAWVIEW3D, "Shared",	600,140,60,19, &lasttface->mode, 0, 0, 0, 0, "Blend vertex colours across face when vertices are shared");
			uiDefButS(block, TOG|BIT|9, REDRAWVIEW3D, "Twoside",	660,140,60,19, &lasttface->mode, 0, 0, 0, 0, "Render face twosided");
			uiDefButS(block, TOG|BIT|11, REDRAWVIEW3D, "ObColor",720,140,60,19, &lasttface->mode, 0, 0, 0, 0, "Use ObColor instead of vertex colours");

			uiDefButS(block, TOG|BIT|8, B_TFACE_HALO, "Halo",	600,120,60,19, &lasttface->mode, 0, 0, 0, 0, "Screen aligned billboard");
			uiDefButS(block, TOG|BIT|12, B_TFACE_BILLB, "Billboard",660,120,60,19, &lasttface->mode, 0, 0, 0, 0, "Billboard with Z-axis constraint");
			uiDefButS(block, TOG|BIT|13, REDRAWVIEW3D, "Shadow", 720,120,60,19, &lasttface->mode, 0, 0, 0, 0, "Face is used for shadow");
			uiDefButS(block, TOG|BIT|14, REDRAWVIEW3D, "Text", 780,120,60,19, &lasttface->mode, 0, 0, 0, 0, "Enable bitmap text on face");

			uiBlockSetCol(block, BUTPURPLE);
			uiDefButC(block, ROW, REDRAWVIEW3D, "Opaque",	600,100,60,19, &lasttface->transp, 2.0, 0.0, 0, 0, "Render colour of textured face as colour");
			uiDefButC(block, ROW, REDRAWVIEW3D, "Add",		660,100,60,19, &lasttface->transp, 2.0, 1.0, 0, 0, "Render face transparent and add colour of face");
			uiDefButC(block, ROW, REDRAWVIEW3D, "Alpha",		720,100,60,19, &lasttface->transp, 2.0, 2.0, 0, 0, "Render polygon transparent, depending on alpha channel of the texture");
			/* uiDefButC(block, ROW, REDRAWVIEW3D, "Sub",	780,100,60,19, &lasttface->transp, 2.0, 3.0, 0, 0); ,""*/

		}
	}
	uiBlockSetCol(block, BUTSALMON);
	if(G.f & G_FACESELECT) {
		uiDefBut(block, BUT, B_SET_VCOL, "Set VertCol",	859,37,103,28, 0, 0, 0, 0, 0, "Set Vertex colour of selection to current (Shift+K)");

	}
	uiDefBut(block, BUT, B_COPY_TF_MODE, "Copy DrawMode", 650,7,117,28, 0, 0, 0, 0, 0, "Copy the drawmode");
	uiDefBut(block, BUT, B_COPY_TF_UV, "Copy UV+tex",		771,7,85,28, 0, 0, 0, 0, 0, "Copy UV information and textures");
	uiDefBut(block, BUT, B_COPY_TF_COL, "Copy VertCol",	859,7,103,28, 0, 0, 0, 0, 0, "Copy vertex colours");

	uiDrawBlock(block);
}

/* *************************** RADIO ******************************** */

void do_radiobuts(short event)
{
	Radio *rad;
	int phase;
	
	phase= rad_phase();
	rad= G.scene->radio;
	
	switch(event) {
	case B_RAD_ADD:
		add_radio();
		allqueue(REDRAWBUTSRADIO, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_DELETE:
		delete_radio();
		allqueue(REDRAWBUTSRADIO, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_FREE:
		freeAllRad();
		allqueue(REDRAWBUTSRADIO, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_COLLECT:
		rad_collect_meshes();
		allqueue(REDRAWBUTSRADIO, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_INIT:
		if(phase==RAD_PHASE_PATCHES) {
			rad_limit_subdivide();
			allqueue(REDRAWBUTSRADIO, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_RAD_SHOOTP:
		if(phase==RAD_PHASE_PATCHES) {
			waitcursor(1);
			rad_subdivshootpatch();
			allqueue(REDRAWBUTSRADIO, 0);
			allqueue(REDRAWVIEW3D, 0);
			waitcursor(0);
		}
		break;
	case B_RAD_SHOOTE:
		if(phase==RAD_PHASE_PATCHES) {
			waitcursor(1);
			rad_subdivshootelem();
			allqueue(REDRAWBUTSRADIO, 0);
			allqueue(REDRAWVIEW3D, 0);
			waitcursor(0);
		}
		break;
	case B_RAD_GO:
		if(phase==RAD_PHASE_PATCHES) {
			waitcursor(1);
			rad_go();
			waitcursor(0);
			allqueue(REDRAWBUTSRADIO, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_RAD_LIMITS:
		rad_setlimits();
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSRADIO, 0);
		break;
	case B_RAD_FAC:
		set_radglobal();
		if(phase & RAD_PHASE_FACES) make_face_tab();
		else make_node_display();
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_NODELIM:
		if(phase & RAD_PHASE_FACES) {
			set_radglobal();
			removeEqualNodes(rad->nodelim);
			make_face_tab();
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSRADIO, 0);
		}
		break;
	case B_RAD_NODEFILT:
		if(phase & RAD_PHASE_FACES) {
			set_radglobal();
			filterNodes();
			make_face_tab();
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_RAD_FACEFILT:
		if(phase & RAD_PHASE_FACES) {
			filterFaces();
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_RAD_DRAW:
		set_radglobal();
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_ADDMESH:
		if(phase & RAD_PHASE_FACES) rad_addmesh();
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_REPLACE:
		if(phase & RAD_PHASE_FACES) rad_replacemesh();
		allqueue(REDRAWVIEW3D, 0);
		break;
	}

}


void radiobuts(void)
{
	Radio *rad;
	uiBlock *block;
	int flag;
	char str[128];

	rad= G.scene->radio;
	if(rad==0) {
		add_radio();
		rad= G.scene->radio;
	}
	
	flag= rad_phase();

	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	uiAutoBlock(block, 10, 30, 190, 100, UI_BLOCK_ROWS);

	if(flag == RAD_PHASE_PATCHES) uiBlockSetCol(block, BUTSALMON);
	else uiBlockSetCol(block, BUTGREY);
	uiDefBut(block,  BUT, B_RAD_INIT, "Limit Subdivide",	0, 0, 10, 10, NULL, 0, 0, 0, 0, "Subdivide patches");
	if(flag & RAD_PHASE_PATCHES) uiBlockSetCol(block, BUTPURPLE);
	else uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block,  BUT, B_RAD_COLLECT, "Collect Meshes",	1, 0, 10, 10, NULL, 0, 0, 0, 0, "Convert selected and visible meshes to patches");
	uiDrawBlock(block);

	sprintf(str, "buttonswin1 %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	uiAutoBlock(block, 210, 30, 230, 150, UI_BLOCK_ROWS);
	
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block,  ROW, B_RAD_DRAW, "Wire",			0, 0, 10, 10, &rad->drawtype, 0.0, 0.0, 0, 0, "Enable wireframe drawmode");
	uiDefButS(block,  ROW, B_RAD_DRAW, "Solid",			0, 0, 10, 10, &rad->drawtype, 0.0, 1.0, 0, 0, "Enable solid drawmode");
	uiDefButS(block,  ROW, B_RAD_DRAW, "Gour",			0, 0, 10, 10, &rad->drawtype, 0.0, 2.0, 0, 0, "Enable Gourad drawmode");
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block,  TOG|BIT|0, B_RAD_DRAW, "ShowLim",  1, 0, 10, 10, &rad->flag, 0, 0, 0, 0, "Visualize patch and element limits");
	uiDefButS(block,  TOG|BIT|1, B_RAD_DRAW, "Z",		1, 0, 3, 10, &rad->flag, 0, 0, 0, 0, "Draw limits different");
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block,  NUM, B_RAD_LIMITS, "ElMax:", 		2, 0, 10, 10, &rad->elma, 1.0, 500.0, 0, 0, "Set maximum size of an element");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "ElMin:", 		2, 0, 10, 10, &rad->elmi, 1.0, 100.0, 0, 0, "Set minimum size of an element");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "PaMax:", 		3, 0, 10, 10, &rad->pama, 10.0, 1000.0, 0, 0, "Set maximum size of a patch");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "PaMin:", 		3, 0, 10, 10, &rad->pami, 10.0, 1000.0, 0, 0, "Set minimum size of a patch");
	uiDrawBlock(block);
	
	sprintf(str, "buttonswin2 %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	uiAutoBlock(block, 450, 30, 180, 150, UI_BLOCK_ROWS);
	
	if(flag == RAD_PHASE_PATCHES) uiBlockSetCol(block, BUTSALMON);
	else uiBlockSetCol(block, BUTGREY);
	uiDefBut(block,  BUT, B_RAD_SHOOTE, "Subdiv Shoot Element", 0, 0, 12, 10, NULL, 0, 0, 0, 0, "");
	uiDefBut(block,  BUT, B_RAD_SHOOTP, "Subdiv Shoot Patch",	1, 0, 12, 10, NULL, 0, 0, 0, 0, "Detect high energy changes");
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block,  NUM, 0, "Max Subdiv Shoot:", 			2, 0, 10, 10, &rad->maxsublamp, 1.0, 250.0, 0, 0, "Set the maximum number of shoot patches that are evaluated");
	uiDefButI(block,  NUM, 0, "MaxEl:",						3, 0, 10, 10, &rad->maxnode, 1.0, 250000.0, 0, 0, "Set the maximum allowed number of elements");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "Hemires:", 		4, 0, 10, 10, &rad->hemires, 100.0, 1000.0, 100, 0, "Set the size of a hemicube");
	uiDrawBlock(block);
	
	sprintf(str, "buttonswin3 %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	uiAutoBlock(block, 640, 30, 200, 150, UI_BLOCK_ROWS);
	
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block,  NUM, 0, "Max Iterations:", 	0, 0, 10, 10, &rad->maxiter, 0.0, 10000.0, 0, 0, "Maximum number of radiosity rounds");
	uiDefButF(block,  NUM, 0, "Convergence:", 		1, 0, 10, 10, &rad->convergence, 0.0, 1.0, 10, 0, "Set the lower threshold of unshot energy");
	uiDefButS(block,  NUM, 0, "SubSh P:", 			2, 0, 10, 10, &rad->subshootp, 0.0, 10.0, 0, 0, "Set the number of times the environment is tested to detect pathes");
	uiDefButS(block,  NUM, 0, "SubSh E:", 			2, 0, 10, 10, &rad->subshoote, 0.0, 10.0, 0, 0, "Set the number of times the environment is tested to detect elements");
	if(flag == RAD_PHASE_PATCHES) uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block,  BUT, B_RAD_GO, "GO",				3, 0, 10, 15, NULL, 0, 0, 0, 0, "Start the radiosity simulation");
	uiDrawBlock(block);
	
	sprintf(str, "buttonswin4 %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	uiAutoBlock(block, 850, 30, 200, 150, UI_BLOCK_ROWS);

	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block,  NUM, B_RAD_FAC, "Mult:",			0, 0, 50, 17, &rad->radfac, 0.001, 250.0, 100, 0, "Mulitply the energy values");
	uiDefButF(block,  NUM, B_RAD_FAC, "Gamma:",			0, 0, 50, 17, &rad->gamma, 0.2, 10.0, 10, 0, "Change the contrast of the energy values");
	if(flag & RAD_PHASE_FACES) uiBlockSetCol(block, BUTSALMON);
	else uiBlockSetCol(block, BUTGREY);
	uiDefBut(block,  BUT, B_RAD_FACEFILT, "FaceFilter",		1, 0, 10, 10, NULL, 0, 0, 0, 0, "Force an extra smoothing");
	if(flag & RAD_PHASE_FACES) uiBlockSetCol(block, BUTSALMON);
	else uiBlockSetCol(block, BUTGREY);
	uiDefBut(block,  BUT, B_RAD_NODELIM, "RemoveDoubles",	2, 0, 30, 10, NULL, 0.0, 50.0, 0, 0, "Join elements which differ less than 'Lim'");
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block,  NUM, 0, "Lim:",					2, 0, 10, 10, &rad->nodelim, 0.0, 50.0, 0, 0, "Set the range for removing doubles");
	if(flag & RAD_PHASE_FACES) uiBlockSetCol(block, BUTSALMON);
	else uiBlockSetCol(block, BUTGREY);
	uiDefBut(block,  BUT, B_RAD_NODEFILT, "Element Filter",	3, 0, 10, 10, NULL, 0, 0, 0, 0, "Filter elements to remove aliasing artefacts");
	uiDrawBlock(block);

	sprintf(str, "buttonswin5 %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	uiAutoBlock(block, 1060, 30, 190, 150, UI_BLOCK_ROWS);

	if(flag & RAD_PHASE_PATCHES) uiBlockSetCol(block, BUTSALMON);
	else uiBlockSetCol(block, BUTGREY);
	uiDefBut(block,  BUT, B_RAD_FREE, "Free Radio Data",	0, 0, 10, 10, NULL, 0, 0, 0, 0, "Release all memory used by Radiosity");	
	if(flag & RAD_PHASE_FACES) uiBlockSetCol(block, BUTSALMON);
	else uiBlockSetCol(block, BUTGREY);
	uiDefBut(block,  BUT, B_RAD_REPLACE, "Replace Meshes",	1, 0, 10, 10, NULL, 0, 0, 0, 0, "Convert meshes to Mesh objects with vertex colours, changing input-meshes");
	uiDefBut(block,  BUT, B_RAD_ADDMESH, "Add new Meshes",	2, 0, 10, 10, NULL, 0, 0, 0, 0, "Convert meshes to Mesh objects with vertex colours, unchanging input-meshes");
	uiDrawBlock(block);
	
	rad_status_str(str);
	cpack(0);
	glRasterPos2i(210, 189);
	BMF_DrawString(uiBlockGetCurFont(block), str);
}


/* *************************** MBALL ******************************** */

void do_mballbuts(unsigned short event)
{
	switch(event) {
	case B_RECALCMBALL:
		makeDispList(OBACT);
		allqueue(REDRAWVIEW3D, 0);
		break;
	}
}

void mballbuts(void)
{
	extern MetaElem *lastelem;
	MetaBall *mb;
	Object *ob;
	uiBlock *block;
	char str[64];
	
	ob= OBACT;
	if(ob==0) return;

	sprintf(str, "editbuttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	
	mb= ob->data;	
	if (ob==find_basis_mball(ob)) {
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "Wiresize:",	470,178,250,19, &mb->wiresize, 0.05, 1.0, 0, 0, "");
		uiDefButF(block, NUMSLI, 0, "Rendersize:",			470,158,250,19, &mb->rendersize, 0.05, 1.0, 0, 0, "");
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "Threshold:", 470,138,250,19, &mb->thresh, 0.0001, 5.0, 0, 0, "");

		uiBlockSetCol(block, BUTBLUE);
		uiDefBut(block, LABEL, 0, "Update:",		471,108,120,19, 0, 0, 0, 0, 0, "");
		uiDefButS(block, ROW, B_DIFF, "Always",	471, 85, 120, 19, &mb->flag, 0.0, 0.0, 0, 0, "");
		uiDefButS(block, ROW, B_DIFF, "Half Res",	471, 65, 120, 19, &mb->flag, 0.0, 1.0, 0, 0, "");
		uiDefButS(block, ROW, B_DIFF, "Fast",		471, 45, 120, 19, &mb->flag, 0.0, 2.0, 0, 0, "");
		uiBlockSetCol(block, BUTGREY);
	}
	
	if(ob==G.obedit && lastelem) {
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "Stiffness:", 750,178,250,19, &lastelem->s, 0.0, 10.0, 0, 0, "");
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "Len:",		750,158,250,19, &lastelem->len, 0.0, 20.0, 0, 0, "");

		uiBlockSetCol(block, BUTGREEN);
		uiDefButS(block, TOG|BIT|1, B_RECALCMBALL, "Negative",752,116,60,19, &lastelem->flag, 0, 0, 0, 0, "");

		uiDefButS(block, ROW, B_RECALCMBALL, "Ball",			753,83,60,19, &lastelem->type, 1.0, 0.0, 0, 0, "");
		uiDefButS(block, ROW, B_RECALCMBALL, "TubeX",			753,62,60,19, &lastelem->type, 1.0, 1.0, 0, 0, "");
		uiDefButS(block, ROW, B_RECALCMBALL, "TubeY",			814,62,60,19, &lastelem->type, 1.0, 2.0, 0, 0, "");
		uiDefButS(block, ROW, B_RECALCMBALL, "TubeZ",			876,62,60,19, &lastelem->type, 1.0, 3.0, 0, 0, "");

	}
	uiDrawBlock(block);
}

/* *************************** SCRIPT ******************************** */

static void extend_scriptlink(ScriptLink *slink)
{
	void *stmp, *ftmp;

	if (!slink) return;
		
	stmp= slink->scripts;		
	slink->scripts= MEM_mallocN(sizeof(ID*)*(slink->totscript+1), "scriptlistL");
	
	ftmp= slink->flag;		
	slink->flag= MEM_mallocN(sizeof(short*)*(slink->totscript+1), "scriptlistF");
	
	if (slink->totscript) {
		memcpy(slink->scripts, stmp, sizeof(ID*)*(slink->totscript));
		MEM_freeN(stmp);

		memcpy(slink->flag, ftmp, sizeof(short)*(slink->totscript));
		MEM_freeN(ftmp);
	}

	slink->scripts[slink->totscript]= NULL;
	slink->flag[slink->totscript]= SCRIPT_FRAMECHANGED;

	slink->totscript++;
				
	if(slink->actscript<1) slink->actscript=1;
}

static void delete_scriptlink(ScriptLink *slink)
{
	int i;
	
	if (!slink) return;
	
	if (slink->totscript>0) {
		for (i=slink->actscript-1; i<slink->totscript-1; i++) {
			slink->flag[i]= slink->flag[i+1];
			slink->scripts[i]= slink->scripts[i+1];
		}
		
		slink->totscript--;
	}
		
	CLAMP(slink->actscript, 1, slink->totscript);
		
	if (slink->totscript==0) {
		if (slink->scripts) MEM_freeN(slink->scripts);
		if (slink->flag) MEM_freeN(slink->flag);

		slink->scripts= NULL;
		slink->flag= NULL;
		slink->totscript= slink->actscript= 0;			
	}
}

void do_scriptbuts(short event)
{
	Object *ob=NULL;
	ScriptLink *script=NULL;
	Material *ma;
	
	switch (event) {
	case B_SSCRIPT_ADD:
		extend_scriptlink(&G.scene->scriptlink);
		break;
	case B_SSCRIPT_DEL:
		delete_scriptlink(&G.scene->scriptlink);
		break;
		
	case B_SCRIPT_ADD:
	case B_SCRIPT_DEL:
		ob= OBACT;

		if (ob && G.buts->scriptblock==ID_OB) {
				script= &ob->scriptlink;

		} else if (ob && G.buts->scriptblock==ID_MA) {
			ma= give_current_material(ob, ob->actcol);
			if (ma)	script= &ma->scriptlink;

		} else if (ob && G.buts->scriptblock==ID_CA) {
			if (ob->type==OB_CAMERA)
				script= &((Camera *)ob->data)->scriptlink;

		} else if (ob && G.buts->scriptblock==ID_LA) {
			if (ob->type==OB_LAMP)
				script= &((Lamp *)ob->data)->scriptlink;

		} else if (G.buts->scriptblock==ID_WO) {
			if (G.scene->world) 
				script= &(G.scene->world->scriptlink);
		}
		
		if (event==B_SCRIPT_ADD) extend_scriptlink(script);
		else delete_scriptlink(script);
		
		break;
	default:
		break;
	}

	allqueue(REDRAWBUTSSCRIPT, 0);
}

void draw_scriptlink(uiBlock *block, ScriptLink *script, int sx, int sy, int scene) 
{
	char str[256];

	uiBlockSetCol(block, BUTGREY);

	if (script->totscript) {
		strcpy(str, "FrameChanged%x 1|");
		strcat(str, "Redraw%x 4|");
		if (scene) {
			strcat(str, "OnLoad%x 2");
		}

		uiDefButS(block, MENU, 1, str, (short)sx, (short)sy, 148, 19, &script->flag[script->actscript-1], 0, 0, 0, 0, "Script links for the Frame changed event");

		uiDefIDPoinBut(block, test_scriptpoin_but, 1, "", (short)(sx+150),(short)sy, 98, 19, &script->scripts[script->actscript-1], "Name of Script to link");
	}

	sprintf(str,"%d Scr:", script->totscript);
	uiDefButS(block, NUM, REDRAWBUTSSCRIPT, str, (short)(sx+250), (short)sy,98,19, &script->actscript, 1, script->totscript, 0, 0, "Total / Active Script link (LeftMouse + Drag to change)");

	uiBlockSetCol(block, BUTSALMON);

	if (scene) {
		if (script->totscript<32767) 
			uiDefBut(block, BUT, B_SSCRIPT_ADD, "New", (short)(sx+350), (short)sy, 38, 19, 0, 0, 0, 0, 0, "Add a new Script link");
		if (script->totscript) 
			uiDefBut(block, BUT, B_SSCRIPT_DEL, "Del", (short)(sx+390), (short)sy, 38, 19, 0, 0, 0, 0, 0, "Delete the current Script link");
	} else {
		if (script->totscript<32767) 
			uiDefBut(block, BUT, B_SCRIPT_ADD, "New", (short)(sx+350), (short)sy, 38, 19, 0, 0, 0, 0, 0, "Add a new Script link");
		if (script->totscript) 
			uiDefBut(block, BUT, B_SCRIPT_DEL, "Del", (short)(sx+390), (short)sy, 38, 19, 0, 0, 0, 0, 0, "Delete the current Script link");
	}		
}

void scriptbuts(void)
{
	Object *ob=NULL;
	ScriptLink *script=NULL;
	Material *ma;
	uiBlock *block;
	char str[64];
	
	ob= OBACT;

	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	if (ob && G.buts->scriptblock==ID_OB) {
		script= &ob->scriptlink;
		
	} else if (ob && G.buts->scriptblock==ID_MA) {
		ma= give_current_material(ob, ob->actcol);
		if (ma)	script= &ma->scriptlink;
		
	} else if (ob && G.buts->scriptblock==ID_CA) {
		if (ob->type==OB_CAMERA)
			script= &((Camera *)ob->data)->scriptlink;
			
	} else if (ob && G.buts->scriptblock==ID_LA) {
		if (ob->type==OB_LAMP)
			script= &((Lamp *)ob->data)->scriptlink;

	} else if (G.buts->scriptblock==ID_WO) {
		if (G.scene->world)
			script= &(G.scene->world->scriptlink);
	}

	if (script) draw_scriptlink(block, script, 25, 180, 0);			
	
	/* EVENTS */
	draw_buttons_edge(curarea->win, 540);

	draw_scriptlink(block, &G.scene->scriptlink, 600, 180, 1);

	uiDrawBlock(block);
}

/* *************************** IKA ******************************** */
/* is this number used elsewhere? */
/*  static int ika_del_number; */
void do_ikabuts(unsigned short event)
{
	Base *base;
	Object *ob;
	
	ob= OBACT;
	
	switch(event) {
	case B_IKASETREF:
		base= FIRSTBASE;
		while(base) {
			if TESTBASELIB(base) {
				if(base->object->type==OB_IKA) init_defstate_ika(base->object);
			}
			base= base->next;
		}
		break;	
	case B_IKARECALC:
		itterate_ika(ob);
		break;
	}
}

void ikabuts(void)
{
	Ika *ika;
	Object *ob;
	Limb *li;
	Deform *def;
	uiBlock *block;
	int nr, cury, nlimbs;
	char str[32];
	
	ob= OBACT;
	if(ob==0) return;

	sprintf(str, "editbuttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	ika= ob->data;
	
	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT, B_IKASETREF,	"Set Reference",470,180,200,20, 0, 0, 0, 0, 0, "");

	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|1, B_DIFF, "Lock XY Plane",	470,140,200,20, &ika->flag, 0.0, 1.0, 0, 0, "New IK option: allows both X and Y axes to rotate");
	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUM, B_DIFF, "XY constraint ",		470,120,200,20, &ika->xyconstraint, 0.0, 1.0, 100, 0, "Constrain in radians");

	uiDefButF(block, NUMSLI, B_DIFF, "Mem ",				470,80,200,20, &ika->mem, 0.0, 1.0, 0, 0, "");
	uiDefButS(block, NUM, B_DIFF, "Iter: ",				470,60,200,20, &ika->iter, 2.0, 16.0, 0, 0, "");


	uiBlockSetCol(block, BUTGREY);

	uiDefBut(block, LABEL, 0, "Limb Weight",			680, 200, 150, 19, 0, 0, 0, 0, 0, "");
	cury= 180;
	li= ika->limbbase.first;

	nlimbs= BLI_countlist(&ika->limbbase);

	for (nr = 0; nr < nlimbs; nr++) {
		sprintf(str, "Limb %d:", nr);
		uiDefButF(block, NUM, B_DIFF, str, 680, (short)cury, 150, 19, &li->fac, 0.01, 1.0, 10, 0, "");
		cury-= 20;
		li= li->next;
	}

	
	
	uiDefBut(block, LABEL, 0, "Deform Max Dist",	955, 200, 140, 19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, LABEL, 0, "Deform Weight",	1095, 200, 140, 19, 0, 0, 0, 0, 0, "");
	

	cury= 180;
	def= ika->def;
	for (nr = 0; nr < ika->totdef; nr++) {
		def = ika->def+nr;
		if(def->ob) {
			if(def->ob->type!=OB_IKA) sprintf(str, "%s   :", def->ob->id.name+2);
			else sprintf(str, "%s (%d):", def->ob->id.name+2, def->par1);
		}
		
		uiDefBut(block, LABEL, 0, str,			855, (short)cury, 100, 19, 0, 0.01, 0.0, 0, 0, "");
		uiDefButF(block, NUM, B_DIFF, "",	955, (short)cury, 140, 19, &def->dist, 0.0, 40.0, 100, 0, "Beyond this distance the Limb doesn't influence deformation. '0.0' is global influence.");
		uiDefButF(block, NUM, B_DIFF, "",	1095,(short)cury, 140, 19, &def->fac, 0.01, 10.0, 10, 0, "");

		cury-= 20;
	}
	uiDrawBlock(block);
}

/* *************************** LATTICE ******************************** */

void do_latticebuts(unsigned short event)
{
	Object *ob;
	Lattice *lt;
	
	ob= OBACT;
	
	switch(event) {
	case B_RESIZELAT:
		if(ob) {
			if(ob==G.obedit) resizelattice(editLatt);
			else resizelattice(ob->data);
		}
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_DRAWLAT:
		if(ob==G.obedit) calc_lattverts_ext();
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_LATTCHANGED:
		
		lt= ob->data;
		if(lt->flag & LT_OUTSIDE) outside_lattice(lt);
		
		make_displists_by_parent(ob);

		allqueue(REDRAWVIEW3D, 0);
		
		break;
	}
}

void latticebuts(void)
{
	Lattice *lt;
	Object *ob;
	uiBlock *block;
	char str[64];
	
	ob= OBACT;
	if(ob==0) return;

	sprintf(str, "editbuttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	if(ob==G.obedit) lt= editLatt;
	else lt= ob->data;

	uiSetButLock(lt->key!=0, "Not with VertexKeys");
	uiSetButLock(ob==G.obedit, "Unable to perform function in EditMode");
	uiDefButS(block, NUM, B_RESIZELAT,	"U:",			470,178,100,19, &lt->pntsu, 1.0, 64.0, 0, 0, "");
	uiDefButS(block, NUM, B_RESIZELAT,	"V:",			470,158,100,19, &lt->pntsv, 1.0, 64.0, 0, 0, "");
	uiDefButS(block, NUM, B_RESIZELAT,	"W:",			470,138,100,19, &lt->pntsw, 1.0, 64.0, 0, 0, "");
	uiClearButLock();
	
	uiBlockSetCol(block, BUTGREEN);
	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_LINEAR, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		612, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_CARDINAL, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_BSPLINE, 0, 0, "");

	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 158, 40, 19, &lt->typev, 2.0, (float)KEY_LINEAR, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		612, 158, 40, 19, &lt->typev, 2.0, (float)KEY_CARDINAL, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 158, 40, 19, &lt->typev, 2.0, (float)KEY_BSPLINE, 0, 0, "");

	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 138, 40, 19, &lt->typew, 3.0, (float)KEY_LINEAR, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		612, 138, 40, 19, &lt->typew, 3.0, (float)KEY_CARDINAL, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 138, 40, 19, &lt->typew, 3.0, (float)KEY_BSPLINE, 0, 0, "");
	
	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT, B_RESIZELAT,	"Make Regular",		470,101,99,32, 0, 0, 0, 0, 0, "");

	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|1, B_LATTCHANGED, "Outside",	571,101,120,31, &lt->flag, 0, 0, 0, 0, "");

	uiDrawBlock(block);
}


/* *************************** TEXTURE ******************************** */

Tex *cur_imatex=0;
int prv_win= 0;

void load_tex_image(char *str)	/* called from fileselect */
{
	Image *ima=0;
	Tex *tex;
	
	tex= cur_imatex;
	if(tex->type==TEX_IMAGE || tex->type==TEX_ENVMAP) {

		ima= add_image(str);
		if(ima) {
			if(tex->ima) {
				tex->ima->id.us--;
			}
			tex->ima= ima;

			free_image_buffers(ima);	/* force reading again */
			ima->ok= 1;
		}

		allqueue(REDRAWBUTSTEX, 0);

		BIF_preview_changed(G.buts);
	}
}

void load_plugin_tex(char *str)	/* called from fileselect */
{
	Tex *tex;
	
	tex= cur_imatex;
	if(tex->type!=TEX_PLUGIN) return;
	
	if(tex->plugin) free_plugin_tex(tex->plugin);
	
	tex->stype= 0;
	tex->plugin= add_plugin_tex(str);

	allqueue(REDRAWBUTSTEX, 0);
	BIF_preview_changed(G.buts);
}

int vergcband(const void *a1, const void *a2)
{
	const CBData *x1=a1, *x2=a2;

	if( x1->pos > x2->pos ) return 1;
	else if( x1->pos < x2->pos) return -1;
	return 0;
}



void save_env(char *name)
{
	Tex *tex;
	char str[FILE_MAXFILE];
	
	strcpy(str, name);
	BLI_convertstringcode(str, G.sce, G.scene->r.cfra);
	tex= G.buts->lockpoin;
	
	if(tex && GS(tex->id.name)==ID_TE) {
		if(tex->env && tex->env->ok && saveover(str)) {
			waitcursor(1);
			BIF_save_envmap(tex->env, str);
			strcpy(G.ima, name);
			waitcursor(0);
		}
	}
	
}

void drawcolorband(ColorBand *coba, float x1, float y1, float sizex, float sizey)
{
	CBData *cbd;
	float v3[2], v1[2], v2[2];
	int a;
	
	if(coba==0) return;
	
	/* outline */
	v1[0]= x1; v1[1]= y1;
	glLineWidth((GLfloat)(3));
	cpack(0x0);
	glBegin(GL_LINE_LOOP);
		glVertex2fv(v1);
		v1[0]+= sizex;
		glVertex2fv(v1);
		v1[1]+= sizey;
		glVertex2fv(v1);
		v1[0]-= sizex;
		glVertex2fv(v1);
	glEnd();
	glLineWidth((GLfloat)(1));


	glShadeModel(GL_SMOOTH);
	cbd= coba->data;
	
	v1[0]= v2[0]= x1;
	v1[1]= y1;
	v2[1]= y1+sizey;
	
	glBegin(GL_QUAD_STRIP);
	
	glColor3fv( &cbd->r );
	glVertex2fv(v1); glVertex2fv(v2);
	
	for(a=0; a<coba->tot; a++, cbd++) {
		
		v1[0]=v2[0]= x1+ cbd->pos*sizex;

		glColor3fv( &cbd->r );
		glVertex2fv(v1); glVertex2fv(v2);
	}
	
	v1[0]=v2[0]= x1+ sizex;
	glVertex2fv(v1); glVertex2fv(v2);
	
	glEnd();
	glShadeModel(GL_FLAT);
	
	/* help lines */
	
	v1[0]= v2[0]=v3[0]= x1;
	v1[1]= y1;
	v2[1]= y1+0.5*sizey;
	v3[1]= y1+sizey;
	
	cbd= coba->data;
	glBegin(GL_LINES);
	for(a=0; a<coba->tot; a++, cbd++) {
		v1[0]=v2[0]=v3[0]= x1+ cbd->pos*sizex;
		
		glColor3ub(0, 0, 0);
		glVertex2fv(v1);
		glVertex2fv(v2);

		if(a==coba->cur) {
			glVertex2f(v1[0]-1, v1[1]);
			glVertex2f(v2[0]-1, v2[1]);
			glVertex2f(v1[0]+1, v1[1]);
			glVertex2f(v2[0]+1, v2[1]);
		}
			
		glColor3ub(255, 255, 255);
		glVertex2fv(v2);
		glVertex2fv(v3);
		
		if(a==coba->cur) {
			glVertex2f(v2[0]-1, v2[1]);
			glVertex2f(v3[0]-1, v3[1]);
			glVertex2f(v2[0]+1, v2[1]);
			glVertex2f(v3[0]+1, v3[1]);
		}
	}
	glEnd();
	
	glFlush();
}



void do_texbuts(unsigned short event)
{
	Tex *tex;
	ImBuf *ibuf;
	ScrArea *sa;
	ID *id;
	CBData *cbd;
	float dx;
	int a, nr;
	short mvalo[2], mval[2];
	char *name, str[80];
	
	tex= G.buts->lockpoin;
	
	switch(event) {
	case B_TEXCHANNEL:
		scrarea_queue_headredraw(curarea);
		BIF_preview_changed(G.buts);
		allqueue(REDRAWBUTSTEX, 0);
		break;
	case B_TEXTYPE:
		if(tex==0) return;
		tex->stype= 0;
		allqueue(REDRAWBUTSTEX, 0);
		BIF_preview_changed(G.buts);
		break;
	case B_DEFTEXVAR:
		if(tex==0) return;
		default_tex(tex);
		allqueue(REDRAWBUTSTEX, 0);
		BIF_preview_changed(G.buts);
		break;
	case B_LOADTEXIMA:
	case B_LOADTEXIMA1:
		if(tex==0) return;
		/* globals: temporal store them: we make another area a fileselect */
		cur_imatex= tex;
		prv_win= curarea->win;
		
		sa= closest_bigger_area();
		areawinset(sa->win);
		if(tex->ima) name= tex->ima->name;
#ifdef _WIN32
		else {
			if (strcmp (U.textudir, "/") == 0)
				name= G.sce;
			else
				name= U.textudir;
		}
#else
		else name = U.textudir;
#endif
		
		if(event==B_LOADTEXIMA)
			activate_imageselect(FILE_SPECIAL, "SELECT IMAGE", name, load_tex_image);
		else 
			activate_fileselect(FILE_SPECIAL, "SELECT IMAGE", name, load_tex_image);
		
		break;
	case B_NAMEIMA:
		if(tex==0) return;
		if(tex->ima) {
			cur_imatex= tex;
			prv_win= curarea->win;
			
			/* name in tex->ima has been changed by button! */
			strcpy(str, tex->ima->name);
			if(tex->ima->ibuf) strcpy(tex->ima->name, tex->ima->ibuf->name);

			load_tex_image(str);
		}
		break;
	case B_TEXREDR_PRV:
		allqueue(REDRAWBUTSTEX, 0);
		BIF_preview_changed(G.buts);
		break;
	case B_TEXIMABROWSE:
		if(tex) {
			id= (ID*) tex->ima;
			
			if(G.buts->menunr== -2) {
				activate_databrowse(id, ID_IM, 0, B_TEXIMABROWSE, &G.buts->menunr, do_texbuts);
			} else if (G.buts->menunr>0) {
				Image *newima= (Image*) BLI_findlink(&G.main->image, G.buts->menunr-1);
				
				if (newima && newima!=(Image*) id) {
					tex->ima= newima;
					id_us_plus((ID*) newima);
					if(id) id->us--;
				
					allqueue(REDRAWBUTSTEX, 0);
					BIF_preview_changed(G.buts);
				}
			}
		}
		break;
	case B_IMAPTEST:
		if(tex) {
			if( (tex->imaflag & (TEX_FIELDS+TEX_MIPMAP))== TEX_FIELDS+TEX_MIPMAP ) {
				error("Cannot combine fields and mipmap");
				tex->imaflag -= TEX_MIPMAP;
				allqueue(REDRAWBUTSTEX, 0);
			}
			
			if(tex->ima && tex->ima->ibuf) {
				ibuf= tex->ima->ibuf;
				nr= 0;
				if( !(tex->imaflag & TEX_FIELDS) && (ibuf->flags & IB_fields) ) nr= 1;
				if( (tex->imaflag & TEX_FIELDS) && !(ibuf->flags & IB_fields) ) nr= 1;
				if(nr) {
					IMB_freeImBuf(ibuf);
					tex->ima->ibuf= 0;
					tex->ima->ok= 1;
					BIF_preview_changed(G.buts);
				}
			}
		}
		break;
	case B_RELOADIMA:
		if(tex && tex->ima) {
			// check if there is a newer packedfile

			if (tex->ima->packedfile) {
				PackedFile *pf;
				pf = newPackedFile(tex->ima->name);
				if (pf) {
					freePackedFile(tex->ima->packedfile);
					tex->ima->packedfile = pf;
				} else {
					error("Image not available. Keeping packed image.");
				}
			}

			IMB_freeImBuf(tex->ima->ibuf);
			tex->ima->ibuf= 0;
			tex->ima->ok= 1;
			allqueue(REDRAWBUTSTEX, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
			BIF_preview_changed(G.buts);
		}
		break;

	case B_TEXSETFRAMES:
		if(tex->ima->anim) tex->frames = IMB_anim_get_duration(tex->ima->anim);
		allqueue(REDRAWBUTSTEX, 0);
		break;

	case B_PACKIMA:
		if(tex && tex->ima) {
			if (tex->ima->packedfile) {
				if (G.fileflags & G_AUTOPACK) {
					if (okee("Disable AutoPack ?")) {
						G.fileflags &= ~G_AUTOPACK;
					}
				}
				
				if ((G.fileflags & G_AUTOPACK) == 0) {
					unpackImage(tex->ima, PF_ASK);
				}
			} else {
				if (tex->ima->ibuf && (tex->ima->ibuf->userflags & IB_BITMAPDIRTY)) {
					error("Can't pack painted image. Save image from Image window first.");
				} else {
					tex->ima->packedfile = newPackedFile(tex->ima->name);
				}
			}
			allqueue(REDRAWBUTSTEX, 0);
			allqueue(REDRAWHEADERS, 0);
		}
		break;
	case B_LOADPLUGIN:
		if(tex==0) return;

		/* globals: store temporal: we make another area a fileselect */
		cur_imatex= tex;
		prv_win= curarea->win;
			
		sa= closest_bigger_area();
		areawinset(sa->win);
		if(tex->plugin) strcpy(str, tex->plugin->name);
		else {
			strcpy(str, U.plugtexdir);
		}
		activate_fileselect(FILE_SPECIAL, "SELECT PLUGIN", str, load_plugin_tex);
		
		break;

	case B_NAMEPLUGIN:
		if(tex==0 || tex->plugin==0) return;
		strcpy(str, tex->plugin->name);
		free_plugin_tex(tex->plugin);
		tex->stype= 0;
		tex->plugin= add_plugin_tex(str);
		allqueue(REDRAWBUTSTEX, 0);
		BIF_preview_changed(G.buts);
		break;
	
	case B_COLORBAND:
		if(tex==0) return;
		if(tex->coba==0) tex->coba= add_colorband();
		allqueue(REDRAWBUTSTEX, 0);
		BIF_preview_changed(G.buts);
		break;
	
	case B_ADDCOLORBAND:
		if(tex==0 || tex->coba==0) return;
		
		if(tex->coba->tot < MAXCOLORBAND-1) tex->coba->tot++;
		tex->coba->cur= tex->coba->tot-1;
		
		do_texbuts(B_CALCCBAND);
		
		break;

	case B_DELCOLORBAND:
		if(tex==0 || tex->coba==0 || tex->coba->tot<2) return;
		
		for(a=tex->coba->cur; a<tex->coba->tot; a++) {
			tex->coba->data[a]= tex->coba->data[a+1];
		}
		if(tex->coba->cur) tex->coba->cur--;
		tex->coba->tot--;

		allqueue(REDRAWBUTSTEX, 0);
		BIF_preview_changed(G.buts);
		break;

	case B_CALCCBAND:
	case B_CALCCBAND2:
		if(tex==0 || tex->coba==0 || tex->coba->tot<2) return;
		
		for(a=0; a<tex->coba->tot; a++) tex->coba->data[a].cur= a;
		qsort(tex->coba->data, tex->coba->tot, sizeof(CBData), vergcband);
		for(a=0; a<tex->coba->tot; a++) {
			if(tex->coba->data[a].cur==tex->coba->cur) {
				if(tex->coba->cur!=a) addqueue(curarea->win, REDRAW, 0);	/* button cur */
				tex->coba->cur= a;
				break;
			}
		}
		if(event==B_CALCCBAND2) return;
		
		allqueue(REDRAWBUTSTEX, 0);
		BIF_preview_changed(G.buts);
		
		break;
		
	case B_DOCOLORBAND:
		if(tex==0 || tex->coba==0) return;
		
		cbd= tex->coba->data + tex->coba->cur;
		uiGetMouse(mywinget(), mvalo);

		while(get_mbut() & L_MOUSE) {
			uiGetMouse(mywinget(), mval);
			if(mval[0]!=mvalo[0]) {
				dx= mval[0]-mvalo[0];
				dx/= 345.0;
				cbd->pos+= dx;
				CLAMP(cbd->pos, 0.0, 1.0);

				glDrawBuffer(GL_FRONT);
				drawcolorband(tex->coba, 923,81,345,20);
				/* uiSetButs(B_CALCCBAND, B_CALCCBAND); */
				glDrawBuffer(GL_BACK);
				
				do_texbuts(B_CALCCBAND2);
				cbd= tex->coba->data + tex->coba->cur;	/* because qsort */
				
				mvalo[0]= mval[0];
			}
			BIF_wait_for_statechange();
		}
		allqueue(REDRAWBUTSTEX, 0);
		BIF_preview_changed(G.buts);
		
		break;
	
	case B_REDRAWCBAND:
		glDrawBuffer(GL_FRONT);
		drawcolorband(tex->coba, 923,81,345,20);
		glDrawBuffer(GL_BACK);
		BIF_preview_changed(G.buts);
		break;
	
	case B_ENV_DELETE:
		if(tex->env) {
			RE_free_envmap(tex->env);
			tex->env= 0;
			allqueue(REDRAWBUTSTEX, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	case B_ENV_FREE:
		if(tex->env) {
			RE_free_envmapdata(tex->env);
			allqueue(REDRAWBUTSTEX, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	case B_ENV_SAVE:
		if(tex->env && tex->env->ok) {
			sa= closest_bigger_area();
			areawinset(sa->win);
			save_image_filesel_str(str);
			activate_fileselect(FILE_SPECIAL, str, G.ima, save_env);
		}
		break;	
	case B_ENV_OB:
		if(tex->env && tex->env->object) {
			BIF_preview_changed(G.buts);
			if ELEM(tex->env->object->type, OB_CAMERA, OB_LAMP) {
				error("Camera or Lamp not allowed");
				tex->env->object= 0;
			}
		}
		break;
		
	default:
		if(event>=B_PLUGBUT && event<=B_PLUGBUT+23) {
			PluginTex *pit= tex->plugin;
			if(pit && pit->callback) {
				pit->callback(event - B_PLUGBUT);
				BIF_preview_changed(G.buts);
			}
		}
	}
}

static 	void test_idbutton_cb(void *namev, void *arg2_unused)
{
	char *name= namev;
	test_idbutton(name+2);
}

void texbuts(void)
{
	Object *ob;
	Material *ma=0;
	World *wrld=0;
	Lamp *la=0;
	ID *id = NULL;
	MTex *mtex = NULL;
	Tex *tex;
	VarStruct *varstr;
	PluginTex *pit;
	CBData *cbd;
	EnvMap *env;
	uiBlock *block;
	uiBut *but;
	int a, xco, yco, loos, dx, dy, ok;
	char str[30], *strp;
	
	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	uiBlockSetCol(block, BUTSALMON);

	uiDefButC(block, ROW, B_TEXREDR_PRV, "Mat",		200,172,40,20, &G.buts->texfrom, 3.0, 0.0, 0, 0, "Display the texture of the active material");
	uiDefButC(block, ROW, B_TEXREDR_PRV, "World",		240,172,52,20, &G.buts->texfrom, 3.0, 1.0, 0, 0, "Display the texture of the world block");
	uiDefButC(block, ROW, B_TEXREDR_PRV, "Lamp",		292,172,46,20, &G.buts->texfrom, 3.0, 2.0, 0, 0, "Display the texture of the lamp");
	uiBlockSetCol(block, BUTGREY);
	
	ok= 0;
	
	if(G.buts->texfrom==0) {
		ob= OBACT;
		if(ob) {
			id= ob->data;
			if(id) {
				ma= give_current_material(ob, ob->actcol);
				if(ma) ok= 1;
			}
		}
		
	}
	else if(G.buts->texfrom==1) {
		wrld= G.scene->world;
		if(wrld) {
			id= (ID *)wrld;
			ok= 1;
		}
	}
	else if(G.buts->texfrom==2) {
		ob= OBACT;
		if(ob) {
			if(ob->type==OB_LAMP) {
				la= ob->data;
				id= (ID *)la;
				ok= 1;
			}
		}
	}
	
	if(ok==0) {
		uiDrawBlock(block);
		return;
	}
	
	uiSetButLock(id->lib!=0, "Can't edit library data");

	/* CHANNELS */
	yco= 140;
	for(a= 0; a<8; a++) {
		if(G.buts->texfrom==0) mtex= ma->mtex[a];
		else if(G.buts->texfrom==1) mtex= wrld->mtex[a];
		else if(G.buts->texfrom==2)  mtex= la->mtex[a];
		
		if(mtex && mtex->tex) splitIDname(mtex->tex->id.name+2, str, &loos);
		else strcpy(str, "");
		str[14]= 0;
		if(G.buts->texfrom==0) {
			uiDefButC(block, ROW, B_TEXCHANNEL, str,	200,(short)yco,140,18, &(ma->texact), 0.0, (float)a, 0, 0, "Linked channel");
		}
		else if(G.buts->texfrom==1) {
			uiDefButS(block, ROW, B_TEXCHANNEL, str,	200,(short)yco,140,18, &(wrld->texact), 0.0, (float)a, 0, 0, "");
			if(a==5) break;
		}
		else if(G.buts->texfrom==2) {
			uiDefButS(block, ROW, B_TEXCHANNEL, str,	200,(short)yco,140,18, &(la->texact), 0.0, (float)a, 0, 0, "");
			if(a==5) break;
		}
		yco-= 19;
	}
	
	if(G.buts->texfrom==0) {
		but= uiDefBut(block, TEX, B_IDNAME, "MA:",	200,195,140,20, ma->id.name+2, 0.0, 18.0, 0, 0, "Name of the datablock");
		uiButSetFunc(but, test_idbutton_cb, ma->id.name, NULL);
		mtex= ma->mtex[ ma->texact ];
	}
	else if(G.buts->texfrom==1) {
		but= uiDefBut(block, TEX, B_IDNAME, "WO:",					200,195,140,20, wrld->id.name+2, 0.0, 18.0, 0, 0, "Name of the datablock");
		uiButSetFunc(but, test_idbutton_cb, wrld->id.name, NULL);
		mtex= wrld->mtex[ wrld->texact ];
	}
	else if(G.buts->texfrom==2) {
		but= uiDefBut(block, TEX, B_IDNAME, "LA:",					200,195,140,20, la->id.name+2, 0.0, 18.0, 0, 0, "Name of the datablock");
		uiButSetFunc(but, test_idbutton_cb, la->id.name, NULL);
		mtex= la->mtex[ la->texact ];
	}

	if(mtex && mtex->tex) {
		tex= mtex->tex;

		uiSetButLock(tex->id.lib!=0, "Can't edit library data");
		xco= 275;
		uiDefButS(block, ROW, B_TEXTYPE, texstr[0],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, 0.0, 0, 0, "Default");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_IMAGE],(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_IMAGE, 0, 0, "Use image texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_ENVMAP],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_ENVMAP, 0, 0, "Use environment maps");
		if(tex->plugin && tex->plugin->doit) strp= tex->plugin->pname; else strp= texstr[TEX_PLUGIN];
		uiDefButS(block, ROW, B_TEXTYPE, strp,				(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_PLUGIN, 0, 0, "Use plugin");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_CLOUDS],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_CLOUDS, 0, 0, "Use clouds texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_WOOD],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_WOOD, 0, 0, "Use wood texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_MARBLE],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_MARBLE, 0, 0, "Use marble texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_MAGIC],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_MAGIC, 0, 0, "Use magic texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_BLEND],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_BLEND, 0, 0, "Use blend texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_STUCCI],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_STUCCI, 0, 0, "Use strucci texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_NOISE],	(short)(xco+=75), 195, 75, 20, &tex->type, 1.0, (float)TEX_NOISE, 0, 0, "Use noise texture");
		
		/* TYPES */
		uiBlockSetCol(block, BUTGREEN);	
		switch(tex->type) {
		case TEX_CLOUDS:
			uiDefButS(block, ROW, B_MATPRV, "Default",	350, 170, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Use standard noise"); 
			uiDefButS(block, ROW, B_MATPRV, "Color",		425, 170, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Let Noise give RGB value"); 
			uiBlockSetCol(block, BUTGREY);	
			uiDefButF(block, NUM, B_MATPRV, "NoiseSize :",	350, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the noise table");
			uiDefButS(block, NUM, B_MATPRV, "NoiseDepth:",	350, 90, 150, 19, &tex->noisedepth, 0.0, 6.0, 0, 0, "Set the depth of the cloud calculation");
			uiBlockSetCol(block, BUTGREEN);
			uiDefButS(block, ROW, B_MATPRV, "Soft noise",		350, 40, 100, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Use soft noise");
			uiDefButS(block, ROW, B_MATPRV, "Hard noise",		450, 40, 100, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Use hard noise");
			break;
	
		case TEX_WOOD:
			uiDefButS(block, ROW, B_MATPRV, "Bands",		350, 170, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Use standard wood texture"); 
			uiDefButS(block, ROW, B_MATPRV, "Rings",		425, 170, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Use wood rings"); 
			uiDefButS(block, ROW, B_MATPRV, "BandNoise",	500, 170, 75, 18, &tex->stype, 2.0, 2.0, 0, 0, "Add noise to standard wood"); 
			uiDefButS(block, ROW, B_MATPRV, "RingNoise",	575, 170, 75, 18, &tex->stype, 2.0, 3.0, 0, 0, "Add noise to rings"); 
			uiBlockSetCol(block, BUTGREY);	
			uiDefButF(block, NUM, B_MATPRV, "NoiseSize :",	350, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the noise table");
			uiDefButF(block, NUM, B_MATPRV, "Turbulence:",	350, 90, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Set the turbulence of the bandnoise and ringnoise types");
			uiBlockSetCol(block, BUTGREEN);
			uiDefButS(block, ROW, B_MATPRV, "Soft noise",		350, 40, 100, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Use soft noise");
			uiDefButS(block, ROW, B_MATPRV, "Hard noise",		450, 40, 100, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Use hard noise");
			break;
	
		case TEX_MARBLE:
			uiDefButS(block, ROW, B_MATPRV, "Soft",		350, 170, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Use soft marble"); 
			uiDefButS(block, ROW, B_MATPRV, "Sharp",		425, 170, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Use more clearly defined marble"); 
			uiDefButS(block, ROW, B_MATPRV, "Sharper",	500, 170, 75, 18, &tex->stype, 2.0, 2.0, 0, 0, "Use very clear defined marble"); 
			uiBlockSetCol(block, BUTGREY);	
			uiDefButF(block, NUM, B_MATPRV, "NoiseSize :",	350, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the noise table");
			uiDefButS(block, NUM, B_MATPRV, "NoiseDepth:",	350, 90, 150, 19, &tex->noisedepth, 0.0, 6.0, 0, 0, "Set the depth of the marble calculation");
			uiDefButF(block, NUM, B_MATPRV, "Turbulence:",	350, 70, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Set the turbulence of the sine bands");
			uiBlockSetCol(block, BUTGREEN);
			uiDefButS(block, ROW, B_MATPRV, "Soft noise",		350, 40, 100, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Use soft noise");
			uiDefButS(block, ROW, B_MATPRV, "Hard noise",		450, 40, 100, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Use hard noise");
			break;
	
		case TEX_MAGIC:
			uiBlockSetCol(block, BUTGREY);
			uiDefButF(block, NUM, B_MATPRV, "Size :",			350, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the pattern");
			uiDefButS(block, NUM, B_MATPRV, "Depth:",			350, 90, 150, 19, &tex->noisedepth, 0.0, 10.0, 0, 0, "Set the depth of the pattern");
			uiDefButF(block, NUM, B_MATPRV, "Turbulence:",	350, 70, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Set the strength of the pattern");
			break;
	
		case TEX_BLEND:
			uiDefButS(block, ROW, B_MATPRV, "Lin",		350, 170, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Use a linear progresion"); 
			uiDefButS(block, ROW, B_MATPRV, "Quad",		425, 170, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Use a quadratic progression"); 
			uiDefButS(block, ROW, B_MATPRV, "Ease",		500, 170, 75, 18, &tex->stype, 2.0, 2.0, 0, 0, ""); 
			uiDefButS(block, ROW, B_MATPRV, "Diag",		575, 170, 75, 18, &tex->stype, 2.0, 3.0, 0, 0, "Use a diagonal progression");
			uiDefButS(block, ROW, B_MATPRV, "Sphere",		650, 170, 75, 18, &tex->stype, 2.0, 4.0, 0, 0, "Use progression with the shape of a sphere");
			uiDefButS(block, ROW, B_MATPRV, "Halo",		725, 170, 75, 18, &tex->stype, 2.0, 5.0, 0, 0, "Use a quadratic progression with the shape of a sphere");
			
			uiDefButS(block, TOG|BIT|1, B_MATPRV, "Flip XY",	350, 130, 75, 18, &tex->flag, 0, 0, 0, 0, "Flip the direction of the progression a quarter turn");
			break;
			
		case TEX_STUCCI:
			uiDefButS(block, ROW, B_MATPRV, "Plastic",	350, 170, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Use standard stucci");
			uiDefButS(block, ROW, B_MATPRV, "Wall In",	425, 170, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Set start value"); 
			uiDefButS(block, ROW, B_MATPRV, "Wall Out",	500, 170, 75, 18, &tex->stype, 2.0, 2.0, 0, 0, "Set end value"); 
			uiBlockSetCol(block, BUTGREY);	
			uiDefButF(block, NUM, B_MATPRV, "NoiseSize :",	350, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the noise table");
			uiDefButF(block, NUM, B_MATPRV, "Turbulence:",	350, 90, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Set the depth of the stucci");
			uiBlockSetCol(block, BUTGREEN);
			uiDefButS(block, ROW, B_MATPRV, "Soft noise",		350, 40, 100, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Use soft noise");
			uiDefButS(block, ROW, B_MATPRV, "Hard noise",		450, 40, 100, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Use hard noise");
	
			break;
			
		case TEX_NOISE:
			break;
			
		case TEX_IMAGE:
			
			break;
		}
		
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_DEFTEXVAR, "Default Vars",	1180,169,93,47, 0, 0, 0, 0, 0, "Return to standard values");
		
		uiBlockSetCol(block, BUTGREY);
		/* SPECIFIC */
		if(tex->type==TEX_IMAGE) {
			uiDefButF(block, NUM, B_REDR, "MinX ",		350,30,140,19, &tex->cropxmin, -10.0, 10.0, 10, 0, "Set minimum X value for cropping");
			uiDefButF(block, NUM, B_REDR, "MaxX ",		350,10,140,19, &tex->cropxmax, -10.0, 10.0, 10, 0, "Set maximum X value for cropping");
			uiDefButF(block, NUM, B_REDR, "MinY ",		494,30,140,19, &tex->cropymin, -10.0, 10.0, 10, 0, "Set minimum Y value for cropping");
			uiDefButF(block, NUM, B_REDR, "MaxY ",		494,10,140,19, &tex->cropymax, -10.0, 10.0, 10, 0, "Set maximum Y value for cropping");
	
	
			uiDefButS(block, ROW, 0, "Extend",			350,85,69,19, &tex->extend, 4.0, 1.0, 0, 0, "Extend the colour of the edge");
			uiDefButS(block, ROW, 0, "Clip",				421,85,59,19, &tex->extend, 4.0, 2.0, 0, 0, "Return alpha 0.0 outside image");
			uiDefButS(block, ROW, 0, "Repeat",			565,85,68,19, &tex->extend, 4.0, 3.0, 0, 0, "Repeat image horizontally and vertically");
			uiDefButS(block, ROW, 0, "ClipCube",			482,85,82,19, &tex->extend, 4.0, 4.0, 0, 0, "Return alpha 0.0 outside cubeshaped area around image");
	
			uiDefButF(block, NUM, B_MATPRV, "Filter :",	352,109,135,19, &tex->filtersize, 0.1, 25.0, 0, 0, "Set the filter size used by mipmap and interpol");
			
			uiDefButS(block, NUM, B_MATPRV, "Xrepeat:",	350,60,140,19, &tex->xrepeat, 1.0, 512.0, 0, 0, "Set the degree of repetition in the X direction");
			uiDefButS(block, NUM, B_MATPRV, "Yrepeat:",	494,60,140,19, &tex->yrepeat, 1.0, 512.0, 0, 0, "Set the degree of repetition in the Y direction");
			
			uiDefButS(block, NUM, B_MATPRV, "Frames :",	642,110,150,19, &tex->frames, 0.0, 18000.0, 0, 0, "Activate animation option");
			uiDefButS(block, NUM, B_MATPRV, "Offset :",	642,90,150,19, &tex->offset, -9000.0, 9000.0, 0, 0, "Set the number of the first picture of the animation");
			uiDefButS(block, NUM, B_MATPRV, "Fie/Ima:",	642,60,98,19, &tex->fie_ima, 1.0, 200.0, 0, 0, "Set the number of fields per rendered frame");
			uiDefButS(block, NUM, B_MATPRV, "StartFr:",	642,30,150,19, &tex->sfra, 1.0, 9000.0, 0, 0, "Set the start frame of the animation");
			uiDefButS(block, NUM, B_MATPRV, "Len:",		642,10,150,19, &tex->len, 0.0, 9000.0, 0, 0, "Set the length of the animation");
	
			uiDefButS(block, NUM, B_MATPRV, "Fra:",		802,70,73,19, &(tex->fradur[0][0]), 0.0, 18000.0, 0, 0, "Montage mode: frame start");
			uiDefButS(block, NUM, B_MATPRV, "",			879,70,37,19, &(tex->fradur[0][1]), 0.0, 250.0, 0, 0, "Montage mode: amount of displayed frames");
			uiDefButS(block, NUM, B_MATPRV, "Fra:",		802,50,73,19, &(tex->fradur[1][0]), 0.0, 18000.0, 0, 0, "Montage mode: frame start");
			uiDefButS(block, NUM, B_MATPRV, "",			879,50,37,19, &(tex->fradur[1][1]), 0.0, 250.0, 0, 0, "Montage mode: amount of displayed frames");
			uiDefButS(block, NUM, B_MATPRV, "Fra:",		802,30,73,19, &(tex->fradur[2][0]), 0.0, 18000.0, 0, 0, "Montage mode: frame start");
			uiDefButS(block, NUM, B_MATPRV, "",			879,30,37,19, &(tex->fradur[2][1]), 0.0, 250.0, 0, 0, "Montage mode: amount of displayed frames");
			uiDefButS(block, NUM, B_MATPRV, "Fra:",		802,10,73,19, &(tex->fradur[3][0]), 0.0, 18000.0, 0, 0, "Montage mode: frame start");
			uiDefButS(block, NUM, B_MATPRV, "",			879,10,37,19, &(tex->fradur[3][1]), 0.0, 250.0, 0, 0, "Montage mode: amount of displayed frames");
	
			uiBlockSetCol(block, BUTGREEN);
			uiDefButS(block, TOG|BIT|6, 0, "Cyclic",		743,60,48,19, &tex->imaflag, 0, 0, 0, 0, "Repeat animation image");
			
			uiBlockSetCol(block, BUTSALMON);
			uiDefBut(block, BUT, B_LOADTEXIMA, "Load Image", 350,137,132,24, 0, 0, 0, 0, 0, "Load image - thumbnail view");
			uiBlockSetCol(block, BUTGREY);
			uiDefBut(block, BUT, B_LOADTEXIMA1, "", 485,137,10,24, 0, 0, 0, 0, 0, "Load image - file view");
	
			id= (ID *)tex->ima;
			IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->image), id, &(G.buts->menunr));
			if(strp[0])
				uiDefButS(block, MENU, B_TEXIMABROWSE, strp, 496,137,23,24, &(G.buts->menunr), 0, 0, 0, 0, "Browse");
			MEM_freeN(strp);
	
			if(tex->ima) {
				uiDefBut(block, TEX, B_NAMEIMA, "",			520,137,412,24, tex->ima->name, 0.0, 79.0, 0, 0, "Texture name");
				sprintf(str, "%d", tex->ima->id.us);
				uiDefBut(block, BUT, 0, str,					934,137,23,24, 0, 0, 0, 0, 0, "Number of users");
				uiDefBut(block, BUT, B_RELOADIMA, "Reload",	986,137,68,24, 0, 0, 0, 0, 0, "Reload");

				if (tex->ima->packedfile) {
					packdummy = 1;
				} else {
					packdummy = 0;
				}
				uiDefIconButI(block, TOG|BIT|0, B_PACKIMA, ICON_PACKAGE,	960,137,24,24, &packdummy, 0, 0, 0, 0, "Pack/Unpack this Image");
			}
			
			uiBlockSetCol(block, BUTGREEN);
			
			uiDefButS(block, TOG|BIT|0, 0, "InterPol",			350, 170, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Interpolate pixels of the image");
			uiDefButS(block, TOG|BIT|1, B_MATPRV, "UseAlpha",	425, 170, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Use the alpha layer");
			uiDefButS(block, TOG|BIT|5, B_MATPRV, "CalcAlpha",	500, 170, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Calculate an alpha based on the RGB");
			uiDefButS(block, TOG|BIT|2, B_MATPRV, "NegAlpha",	575, 170, 75, 18, &tex->flag, 0, 0, 0, 0, "Reverse the alpha value");
			uiDefButS(block, TOG|BIT|2, B_IMAPTEST, "MipMap",	650, 170, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Generate a series of pictures used for mipmapping");
			uiDefButS(block, TOG|BIT|3, B_IMAPTEST, "Fields",	725, 170, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Work with field images");
			uiDefButS(block, TOG|BIT|4, B_MATPRV, "Rot90",		800, 170, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Rotate image 90 degrees when rendered");
			uiDefButS(block, TOG|BIT|7, B_RELOADIMA, "Movie",	850, 170, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Use a movie for an image");
			uiDefButS(block, TOG|BIT|8, 0, "Anti",				900, 170, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Use anti-aliasing");
			uiDefButS(block, TOG|BIT|10, 0, "StField",			950, 170, 50, 18, &tex->imaflag, 0, 0, 0, 0, "");
			
			uiBlockSetCol(block, BUTGREY);
	
			/* print amount of frames anim */
			if(tex->ima && tex->ima->anim) {
				uiDefBut(block, BUT, B_TEXSETFRAMES, "<",      802, 110, 20, 18, 0, 0, 0, 0, 0, "Paste number of frames in Frames: button");
				sprintf(str, "%d frs  ", IMB_anim_get_duration(tex->ima->anim));
				uiDefBut(block, LABEL, 0, str,      834, 110, 90, 18, 0, 0, 0, 0, 0, "");
				sprintf(str, "%d cur  ", tex->ima->lastframe);
				uiDefBut(block, LABEL, 0, str,      834, 90, 90, 18, 0, 0, 0, 0, 0, "");
			}
			
			
		}
		else if(tex->type==TEX_PLUGIN) {
			if(tex->plugin && tex->plugin->doit) {
				
				pit= tex->plugin;
	
				uiBlockSetCol(block, BUTGREEN);
				for(a=0; a<pit->stypes; a++) {
					uiDefButS(block, ROW, B_MATPRV, pit->stnames+16*a, (short)(350+75*a), 170, 75, 18, &tex->stype, 2.0, (float)a, 0, 0, "");
				}
				
				uiBlockSetCol(block, BUTGREY);
				varstr= pit->varstr;
				if(varstr) {
					for(a=0; a<pit->vars; a++, varstr++) {
						xco= 350 + 140*(a/6);
						yco= 110 - 20*(a % 6);
						uiDefBut(block, varstr->type, B_PLUGBUT+a, varstr->name, (short)xco,(short)yco,137,19, &(pit->data[a]), varstr->min, varstr->max, 100, 0, varstr->tip);
					}
				}
				uiDefBut(block, TEX, B_NAMEPLUGIN, "",			520,137,412,24, pit->name, 0.0, 79.0, 0, 0, "Browse");
			}
	
			uiBlockSetCol(block, BUTSALMON);
			uiDefBut(block, BUT, B_LOADPLUGIN, "Load Plugin", 350,137,137,24, 0, 0, 0, 0, 0, "");
			
		}
		else if(tex->type==TEX_ENVMAP) {
			
			if(tex->env==0) tex->env= RE_add_envmap();
				
			if(tex->env) {
				env= tex->env;
				
				uiBlockSetCol(block, BUTGREEN);
				uiDefButS(block, ROW, B_REDR, 	"Static", 350, 170, 75, 18, &env->stype, 2.0, 0.0, 0, 0, "Calculate map only once");
				uiDefButS(block, ROW, B_REDR, 	"Anim", 425, 170, 75, 18, &env->stype, 2.0, 1.0, 0, 0, "Calculate map each rendering");
				uiDefButS(block, ROW, B_ENV_FREE, "Load", 500, 170, 75, 18, &env->stype, 2.0, 2.0, 0, 0, "Load map from disk");
				
				if(env->stype==ENV_LOAD) {
					uiBlockSetCol(block, BUTSALMON);
					uiDefBut(block, BUT, B_LOADTEXIMA, "Load Image", 350,137,132,24, 0, 0, 0, 0, 0, "Load image - thumbnail view");
					uiBlockSetCol(block, BUTGREY);
					uiDefBut(block, BUT, B_LOADTEXIMA1, "", 485,137,10,24, 0, 0, 0, 0, 0, "Load image - file view");
					
					id= (ID *)tex->ima;
					IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->image), id, &(G.buts->menunr));
					if(strp[0])
						uiDefButS(block, MENU, B_TEXIMABROWSE, strp, 496,137,23,24, &(G.buts->menunr), 0, 0, 0, 0, "");
					MEM_freeN(strp);
	
					if(tex->ima) {
						uiDefBut(block, TEX, B_NAMEIMA, "",			520,137,412,24, tex->ima->name, 0.0, 79.0, 0, 0, "");
						sprintf(str, "%d", tex->ima->id.us);
						uiDefBut(block, BUT, 0, str,					934,137,23,24, 0, 0, 0, 0, 0, "");
						if (tex->ima->packedfile) {
							packdummy = 1;
						} else {
							packdummy = 0;
						}
						uiDefIconButI(block, TOG|BIT|0, B_PACKIMA, ICON_PACKAGE,	960,137,24,24, &packdummy, 0, 0, 0, 0, "Pack/Unpack this Image");
						uiDefBut(block, BUT, B_RELOADIMA, "Reload",	986,137,68,24, 0, 0, 0, 0, 0, "");
					}
				}
				else {
					uiBlockSetCol(block, BUTSALMON);
					uiDefBut(block, BUT, B_ENV_FREE, "Free Data", 350,137,107,24, 0, 0, 0, 0, 0, "Release all images associated with environment map");
					uiBlockSetCol(block, BUTGREY);
					uiDefBut(block, BUT, B_ENV_SAVE, "Save EnvMap", 461,137,115,24, 0, 0, 0, 0, 0, "Save environment map");
				}
				uiBlockSetCol(block, BUTGREY);
				uiDefIDPoinBut(block, test_obpoin_but, B_ENV_OB, "Ob:",	  350,95,206,24, &(env->object), "Object name");
				uiBlockSetCol(block, BUTGREY);
				uiDefButF(block, NUM, REDRAWVIEW3D, 	"ClipSta", 350,68,122,24, &env->clipsta, 0.01, 50.0, 100, 0, "Set start value for clipping");
				uiDefButF(block, NUM, 0, 	"ClipEnd", 475,68,142,24, &env->clipend, 0.1, 5000.0, 1000, 0, "Set end value for clipping");
				if(env->stype!=ENV_LOAD) uiDefButI(block, NUM, B_ENV_FREE, 	"CubeRes", 620,68,140,24, &env->cuberes, 50, 1000.0, 0, 0, "Set the resolution in pixels");
	
				uiDefButF(block, NUM, B_MATPRV, "Filter :",	558,95,201,24, &tex->filtersize, 0.1, 25.0, 0, 0, "Adjust sharpness or blurriness of the reflection"),
	
				uiDefBut(block, LABEL, 0, "Don't render layer:",		772,100,140,22, 0, 0.0, 0.0, 0, 0, "");	
				xco= 772;
				dx= 28;
				dy= 26;
				for(a=0; a<10; a++) {
					uiDefButI(block, TOG|BIT|(a+10), 0, "",(short)(xco+a*(dx/2)), 68, (short)(dx/2), (short)(dy/2), &env->notlay, 0, 0, 0, 0, "Render this layer");
					uiDefButI(block, TOG|BIT|a, 0, "",	(short)(xco+a*(dx/2)), (short)(68+dy/2), (short)(dx/2), (short)(1+dy/2), &env->notlay, 0, 0, 0, 0, "Render this layer");
					if(a==4) xco+= 5;
				}
	
			}
		}
	
		/* COLORBAND */
		uiBlockSetCol(block, BUTSALMON);
		uiDefButS(block, TOG|BIT|0, B_COLORBAND, "Colorband",		923,103,102,20, &tex->flag, 0, 0, 0, 0, "Use colorband");
		if(tex->flag & TEX_COLORBAND) {
			uiDefBut(block, BUT, B_ADDCOLORBAND, "Add",				1029,103,50,20, 0, 0, 0, 0, 0, "Add new colour to the colorband");
			uiDefBut(block, BUT, B_DELCOLORBAND, "Del",				1218,104,50,20, 0, 0, 0, 0, 0, "Delete the active colour");
			uiBlockSetCol(block, BUTPURPLE);
			uiDefButS(block, NUM, B_REDR,		"Cur:",				1082,104,132,20, &tex->coba->cur, 0.0, (float)(tex->coba->tot-1), 0, 0, "The active colour from the colorband");
	
			uiDefBut(block, LABEL, B_DOCOLORBAND, "", 923,81,345,20, 0, 0, 0, 0, 0, "Colorband"); /* only for event! */
			
			drawcolorband(tex->coba, 923,81,345,20);
			cbd= tex->coba->data + tex->coba->cur;
			
			uiDefButF(block, NUM, B_CALCCBAND, "Pos",			923,59,89,20, &cbd->pos, 0.0, 1.0, 10, 0, "Set the position of the active colour");
			uiBlockSetCol(block, BUTGREEN);
			uiDefButS(block, ROW, B_REDRAWCBAND, "E",		1013,59,20,20, &tex->coba->ipotype, 5.0, 1.0, 0, 0, "Interpolation type Ease");
			uiDefButS(block, ROW, B_REDRAWCBAND, "L",		1033,59,20,20, &tex->coba->ipotype, 5.0, 0.0, 0, 0, "Interpolation type Linear");
			uiDefButS(block, ROW, B_REDRAWCBAND, "S",		1053,59,20,20, &tex->coba->ipotype, 5.0, 2.0, 0, 0, "Interpolation type Spline");
			uiBlockSetCol(block, BUTPURPLE);
			uiDefButF(block, COL, B_BANDCOL, "",					1076,59,28,20, &(cbd->r), 0, 0, 0, 0, "");
			uiDefButF(block, NUMSLI, B_REDRAWCBAND, "A ",			1107,58,163,20, &cbd->a, 0.0, 1.0, 0, 0, "Set the alpha value");
			
			uiDefButF(block, NUMSLI, B_REDRAWCBAND, "R ",			923,37,116,20, &cbd->r, 0.0, 1.0, B_BANDCOL, 0, "Set the red value");
			uiDefButF(block, NUMSLI, B_REDRAWCBAND, "G ",			1042,37,111,20, &cbd->g, 0.0, 1.0, B_BANDCOL, 0, "Set the green value");
			uiDefButF(block, NUMSLI, B_REDRAWCBAND, "B ",			1156,36,115,20, &cbd->b, 0.0, 1.0, B_BANDCOL, 0, "Set the blue value");
			
		}
	
	
		/* RGB-BRICON */
		uiBlockSetCol(block, BUTGREY);
		uiDefButF(block, NUMSLI, B_MATPRV, "Bright",			923,11,166,20, &tex->bright, 0.0, 2.0, 0, 0, "Set the brightness of the colour or intensity of a texture");
		
		uiDefButF(block, NUMSLI, B_MATPRV, "Contr",			1093,11,180,20, &tex->contrast, 0.01, 2.0, 0, 0, "Set the contrast of the colour or intensity of a texture");
	
		if((tex->flag & TEX_COLORBAND)==0) {
			uiDefButF(block, NUMSLI, B_MATPRV, "R ",			923,37,116,20, &tex->rfac, 0.0, 2.0, 0, 0, "Set the red value");
			uiDefButF(block, NUMSLI, B_MATPRV, "G ",			1042,37,111,20, &tex->gfac, 0.0, 2.0, 0, 0, "Set the green value");
			uiDefButF(block, NUMSLI, B_MATPRV, "B ",			1156,36,115,20, &tex->bfac, 0.0, 2.0, 0, 0, "Set the blue value");
		}
	}
	
	/* PREVIEW RENDER */
	
	BIF_previewdraw(G.buts);

	uiDrawBlock(block);
}

/* ****************************** MATERIAL ************************ */
MTex mtexcopybuf;


void do_matbuts(unsigned short event)
{
	static short mtexcopied=0;
	Material *ma;
	MTex *mtex;

	switch(event) {		
	case B_ACTCOL:
		scrarea_queue_headredraw(curarea);
		allqueue(REDRAWBUTSMAT, 0);
		allqueue(REDRAWIPO, 0);
		BIF_preview_changed(G.buts);
		break;
	case B_MATFROM:

		scrarea_queue_headredraw(curarea);
		allqueue(REDRAWBUTSMAT, 0);
		BIF_previewdraw(G.buts);
		break;
	case B_MATPRV:
		/* this event also used by lamp, tex and sky */
		BIF_preview_changed(G.buts);
		break;
	case B_MATPRV_DRAW:
		BIF_preview_changed(G.buts);
		allqueue(REDRAWBUTSMAT, 0);
		break;
	case B_TEXCLEAR:
		ma= G.buts->lockpoin;
		mtex= ma->mtex[(int) ma->texact ];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
			ma->mtex[ (int) ma->texact ]= 0;
			allqueue(REDRAWBUTSMAT, 0);
			allqueue(REDRAWOOPS, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	case B_MTEXCOPY:
		ma= G.buts->lockpoin;
		if(ma && ma->mtex[(int)ma->texact] ) {
			mtex= ma->mtex[(int)ma->texact];
			if(mtex->tex==0) {
				error("No texture available");
			}
			else {
				memcpy(&mtexcopybuf, ma->mtex[(int)ma->texact], sizeof(MTex));
				notice("copied!");
				mtexcopied= 1;
			}
		}
		break;
	case B_MTEXPASTE:
		ma= G.buts->lockpoin;
		if(ma && mtexcopied && mtexcopybuf.tex) {
			if(ma->mtex[(int)ma->texact]==0 ) ma->mtex[(int)ma->texact]= MEM_mallocN(sizeof(MTex), "mtex"); 
			memcpy(ma->mtex[(int)ma->texact], &mtexcopybuf, sizeof(MTex));
			
			id_us_plus((ID *)mtexcopybuf.tex);
			notice("pasted!");
			BIF_preview_changed(G.buts);
			scrarea_queue_winredraw(curarea);
		}
		break;
	case B_MATLAY:
		ma= G.buts->lockpoin;
		if(ma && ma->lay==0) {
			ma->lay= 1;
			scrarea_queue_winredraw(curarea);
		}
	}
}

void matbuts(void)
{
	Object *ob;
	Material *ma;
	ID *id, *idn;
	MTex *mtex;
	uiBlock *block;
	uiBut *but;
	float *colpoin = NULL, min;
	int rgbsel = 0, a, loos;
	char str[30], *strp;
	short xco;
	
	ob= OBACT;
	if(ob==0 || ob->data==0) return;

	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	if(ob->actcol==0) ob->actcol= 1;	/* because of TOG|BIT button */
	
	/* indicate which one is linking a material */
	uiBlockSetCol(block, BUTSALMON);
	uiDefButS(block, TOG|BIT|(ob->actcol-1), B_MATFROM, "OB",	342, 195, 33, 20, &ob->colbits, 0, 0, 0, 0, "Link material to object");
	idn= ob->data;
	strncpy(str, idn->name, 2);
	str[2]= 0;
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOGN|BIT|(ob->actcol-1), B_MATFROM, str,		380, 195, 33, 20, &ob->colbits, 0, 0, 0, 0, "Show the block the material is linked to");
	uiBlockSetCol(block, BUTGREY);
	
	/* id is the block from which the material is used */
	if( BTST(ob->colbits, ob->actcol-1) ) id= (ID *)ob;
	else id= ob->data;

	sprintf(str, "%d Mat", ob->totcol);
	if(ob->totcol) min= 1.0; else min= 0.0;
	uiDefButC(block, NUM, B_ACTCOL, str,	415,195,140,20, &(ob->actcol), min, (float)ob->totcol, 0, 0, "Number of materials on object / Active material");
	
	uiSetButLock(id->lib!=0, "Can't edit library data");
	
	strncpy(str, id->name, 2);
	str[2]= ':'; str[3]= 0;
	but= uiDefBut(block, TEX, B_IDNAME, str,		200,195,140,20, id->name+2, 0.0, 18.0, 0, 0, "Show the block the material is linked to");
	uiButSetFunc(but, test_idbutton_cb, id->name, NULL);

	if(ob->totcol==0) {
		uiDrawBlock(block);
		return;
	}
	
	ma= give_current_material(ob, ob->actcol);
	
	if(ma==0) {
		uiDrawBlock(block);
		return;
	}
	uiSetButLock(ma->id.lib!=0, "Can't edit library data");
	
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, ROW, REDRAWBUTSMAT, "RGB",			200,166,44,22, &(ma->colormodel), 1.0, (float)MA_RGB, 0, 0, "Create colour by red, green and blue");
	uiDefButS(block, ROW, REDRAWBUTSMAT, "HSV",			200,143,44,22, &(ma->colormodel), 1.0, (float)MA_HSV, 0, 0, "Mix colour with hue, saturation and value");
	uiDefButS(block, TOG|BIT|0, REDRAWBUTSMAT, "DYN",	200,120,44,22, &(ma->dynamode), 0.0, 0.0, 0, 0, "Adjust parameters for dynamics options");

	if((ma->mode & MA_HALO)==0)
		uiDefButF(block, NUM, 0, "Zoffset:",		200,91,174,19, &(ma->zoffs), 0.0, 10.0, 0, 0, "Give face an artificial offset");
	
	if(ma->dynamode & MA_DRAW_DYNABUTS) {
		uiDefButF(block, NUMSLI, 0, "Restitut ",			380,168,175,21, &ma->reflect, 0.0, 1.0, 0, 0, "Elasticity of collisions");
		uiDefButF(block, NUMSLI, 0, "Friction ",			
				 380,144,175,21, &ma->friction, 0.0, 100.0, 0, 0, 
				 "Coulomb friction coefficient");
/**/
		uiDefButF(block, NUMSLI, 0, "Fh Force ",				380,120,175,21, &ma->fh, 0.0, 1.0, 0, 0, "Upward spring force within the Fh area");
		uiDefButF(block, NUM, 0,	  "Fh Damp ",			260,144,120,21, &ma->xyfrict, 0.0, 1.0, 10, 0, "Damping of the Fh spring force");
		uiDefButF(block, NUM, 0, "Fh Dist ",				260,120,120,21, &ma->fhdist, 0.0, 20.0, 10, 0, "Height of the Fh area");
		uiBlockSetCol(block, BUTGREEN);
		uiDefButS(block, TOG|BIT|1, 0, "Fh Norm",				260,168,120,21, &ma->dynamode, 0.0, 0.0, 0, 0, "Add a horizontal spring force on slopes");
		uiBlockSetCol(block, BUTGREY);
	}
	else {
		uiDefButF(block, COL, B_MIRCOL, "",		246,143,37,45, &(ma->mirr), 0, 0, 0, 0, "");
		uiDefButF(block, COL, B_SPECCOL, "",		287,143,37,45, &(ma->specr), 0, 0, 0, 0, "");
		uiDefButF(block, COL, B_MATCOL, "",		326,143,47,45, &(ma->r), 0, 0, 0, 0, "");
	
		if(ma->mode & MA_HALO) {
			uiDefButC(block, ROW, REDRAWBUTSMAT, "Ring",			246,120,37,22, &(ma->rgbsel), 2.0, 2.0, 0, 0, "Mix the colour of the rings with the RGB sliders");
			uiDefButC(block, ROW, REDRAWBUTSMAT, "Line",			287,120,37,22, &(ma->rgbsel), 2.0, 1.0, 0, 0, "Mix the colour of the lines with the RGB sliders");
			uiDefButC(block, ROW, REDRAWBUTSMAT, "Halo",			326,120,47,22, &(ma->rgbsel), 2.0, 0.0, 0, 0, "Mix the colour of the halo with the RGB sliders");
		}
		else {
			uiDefButC(block, ROW, REDRAWBUTSMAT, "Mir",			246,120,37,22, &(ma->rgbsel), 2.0, 2.0, 0, 0, "Use mirror colour");
			uiDefButC(block, ROW, REDRAWBUTSMAT, "Spec",			287,120,37,22, &(ma->rgbsel), 2.0, 1.0, 0, 0, "Set the colour of the specularity");
			uiDefButC(block, ROW, REDRAWBUTSMAT, "Color",			326,120,47,22, &(ma->rgbsel), 2.0, 0.0, 0, 0, "Set the basic colour of the material");
		}
		if(ma->rgbsel==0) {colpoin= &(ma->r); rgbsel= B_MATCOL;}
		else if(ma->rgbsel==1) {colpoin= &(ma->specr); rgbsel= B_SPECCOL;}
		else if(ma->rgbsel==2) {colpoin= &(ma->mirr); rgbsel= B_MIRCOL;}
		
		if(ma->rgbsel==0 && (ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE) && !(ma->mode & MA_HALO)));
		else if(ma->colormodel==MA_HSV) {
			uiBlockSetCol(block, BUTPURPLE);
			uiDefButF(block, HSVSLI, B_MATPRV, "H ",			380,168,175,21, colpoin, 0.0, 0.9999, rgbsel, 0, "");
			uiBlockSetCol(block, BUTPURPLE);
			uiDefButF(block, HSVSLI, B_MATPRV, "S ",			380,144,175,21, colpoin, 0.0001, 1.0, rgbsel, 0, "");
			uiBlockSetCol(block, BUTPURPLE);
			uiDefButF(block, HSVSLI, B_MATPRV, "V ",			380,120,175,21, colpoin, 0.0001, 1.0, rgbsel, 0, "");
			uiBlockSetCol(block, BUTGREY);
		}
		else {
			uiDefButF(block, NUMSLI, B_MATPRV, "R ",			380,168,175,21, colpoin, 0.0, 1.0, rgbsel, 0, "");
			uiDefButF(block, NUMSLI, B_MATPRV, "G ",			380,144,175,21, colpoin+1, 0.0, 1.0, rgbsel, 0, "");
			uiDefButF(block, NUMSLI, B_MATPRV, "B ",			380,120,175,21, colpoin+2, 0.0, 1.0, rgbsel, 0, "");
		}
	}
	if(ma->mode & MA_HALO) {
		uiDefButF(block, NUM, B_MATPRV, "HaloSize: ",		200,70,175,18, &(ma->hasize), 0.0, 100.0, 10, 0, "Set the dimension of the halo");
		uiDefButF(block, NUMSLI, B_MATPRV, "Alpha ",		200,50,175,18, &(ma->alpha), 0.0, 1.0, 0, 0, "Set the degree of coverage");
		uiDefButS(block, NUMSLI, B_MATPRV, "Hard ",		200,30,175,18, &(ma->har), 1.0, 127.0, 0, 0, "Set the hardness of the halo");
		uiDefButF(block, NUMSLI, B_MATPRV, "Add  ",		200,10,175,18, &(ma->add), 0.0, 1.0, 0, 0, "Strength of the add effect");
		
		uiDefButS(block, NUM, B_MATPRV, "Rings: ",		380,90,85,18, &(ma->ringc), 0.0, 24.0, 0, 0, "Set the number of rings rendered over the basic halo");
		uiDefButS(block, NUM, B_MATPRV, "Lines: ",		465,90,90,18, &(ma->linec), 0.0, 250.0, 0, 0, "Set the number of star shaped lines rendered over the halo");
		uiDefButS(block, NUM, B_MATPRV, "Star: ",			380,70,85,18, &(ma->starc), 3.0, 50.0, 0, 0, "Set the number of points on the star shaped halo");
		uiDefButC(block, NUM, B_MATPRV, "Seed: ",			465,70,90,18, &(ma->seed1), 0.0, 255.0, 0, 0, "Use random values for ring dimension and line location");
		
		uiDefButF(block, NUM, B_MATPRV, "FlareSize: ",	380,50,85,18, &(ma->flaresize), 0.1, 25.0, 10, 0, "Set the factor the flare is larger than the halo");
		uiDefButF(block, NUM, B_MATPRV, "Sub Size: ",		465,50,90,18, &(ma->subsize), 0.1, 25.0, 10, 0, "Set the dimension of the subflares, dots and circles");
		uiDefButF(block, NUM, B_MATPRV, "FlareBoost: ",	380,30,175,18, &(ma->flareboost), 0.1, 10.0, 10, 0, "Give the flare extra strength");
		uiDefButC(block, NUM, B_MATPRV, "Fl.seed: ",		380,10,85,18, &(ma->seed2), 0.0, 255.0, 0, 0, "Specify an offset in the seed table");
		uiDefButS(block, NUM, B_MATPRV, "Flares: ",		465,10,90,18, &(ma->flarec), 1.0, 32.0, 0, 0, "Set the nuber of subflares");

		uiBlockSetCol(block, BUTBLUE);
		
		uiDefButI(block, TOG|BIT|15, B_MATPRV, "Flare",		571, 181, 77, 36, &(ma->mode), 0, 0, 0, 0, "Render halo as a lensflare");
		uiDefButI(block, TOG|BIT|8, B_MATPRV, "Rings",		571, 143, 77, 18, &(ma->mode), 0, 0, 0, 0, "Render rings over basic halo");
		uiDefButI(block, TOG|BIT|9, B_MATPRV, "Lines",		571, 124, 77, 18, &(ma->mode), 0, 0, 0, 0, "Render star shaped lines over the basic halo");
		uiDefButI(block, TOG|BIT|11, B_MATPRV, "Star",		571, 105, 77, 18, &(ma->mode), 0, 0, 0, 0, "Render halo as a star");
		uiDefButI(block, TOG|BIT|5, B_MATPRV_DRAW, "Halo",	571, 86, 77, 18, &(ma->mode), 0, 0, 0, 0, "Render as a halo");
		
		uiDefButI(block, TOG|BIT|12, B_MATPRV, "HaloTex",		571, 67, 77, 18, &(ma->mode), 0, 0, 0, 0, "Give halo a texture");
		uiDefButI(block, TOG|BIT|13, B_MATPRV, "HaloPuno",	571, 48, 77, 18, &(ma->mode), 0, 0, 0, 0, "Use the vertex normal to specify the dimension of the halo");
		uiDefButI(block, TOG|BIT|10, B_MATPRV, "X Alpha",		571, 28, 77, 18, &(ma->mode), 0, 0, 0, 0, "Use extreme alpha");
		uiDefButI(block, TOG|BIT|14, B_MATPRV, "Shaded",		571, 10, 77, 18, &(ma->mode), 0, 0, 0, 0, "Let halo receive light");
	}
	else {
		uiDefButF(block, NUMSLI, B_MATPRV, "Spec ",		200,70,175,18, &(ma->spec), 0.0, 2.0, 0, 0, "Set the degree of specularity");
		uiDefButS(block, NUMSLI, B_MATPRV, "Hard ",		200,50,175,18, &(ma->har), 1.0, 255.0, 0, 0, "Set the hardness of the specularity");
		uiDefButF(block, NUMSLI, B_MATPRV, "SpTr ",		200,30,175,18, &(ma->spectra), 0.0, 1.0, 0, 0, "Make sheen areas opaque");
		uiDefButF(block, NUMSLI, B_MATPRV, "Add  ",		200,10,175,18, &(ma->add), 0.0, 1.0, 0, 0, "Glow factor");
	
		uiDefButF(block, NUMSLI, B_MATPRV, "Ref   ",		380,70,175,18, &(ma->ref), 0.0, 1.0, 0, 0, "Set the amount of reflection");
		uiDefButF(block, NUMSLI, B_MATPRV, "Alpha ",		380,50,175,18, &(ma->alpha), 0.0, 1.0, 0, 0, "Set the amount of coverage, to make materials transparent");
		uiDefButF(block, NUMSLI, B_MATPRV, "Emit  ",		380,30,175,18, &(ma->emit), 0.0, 1.0, 0, 0, "Set the amount of emitting light");
		uiDefButF(block, NUMSLI, B_MATPRV, "Amb   ",		380,10,175,18, &(ma->amb), 0.0, 1.0, 0, 0, "Set the amount of global ambient color");
		/* transparent solids : exponential dropoff */
/*  		uiDefButF(block, NUMSLI, B_MATPRV, "K     ",		380,-10,175,18, &(ma->kfac), 0.0, 10.0, 0, 0, ""); */
	
		uiBlockSetCol(block, BUTBLUE);
	
		uiDefButI(block, TOG|BIT|0, 0,	"Traceable",		571,200,77,18, &(ma->mode), 0, 0, 0, 0, "Make material visible for shadow lamps");
		uiDefButI(block, TOG|BIT|1, 0,	"Shadow",		571,181,77,18, &(ma->mode), 0, 0, 0, 0, "Enable material for shadows");
		uiDefButI(block, TOG|BIT|2, B_MATPRV, "Shadeless",	571, 162, 77, 18, &(ma->mode), 0, 0, 0, 0, "Make material insensitive to light or shadow");
		uiDefButI(block, TOG|BIT|3, 0,	"Wire",			571, 143, 77, 18, &(ma->mode), 0, 0, 0, 0, "Render only the edges of faces");
		uiDefButI(block, TOG|BIT|4, B_REDR,	"VCol Light",		571, 124, 77, 18, &(ma->mode), 0, 0, 0, 0, "Add vertex colours as extra light");
		uiDefButI(block, TOG|BIT|7, B_REDR, "VCol Paint",	571,105, 77, 18, &(ma->mode), 0, 0, 0, 0, "Replace basic colours with vertex colours");
		uiDefButI(block, TOG|BIT|5, B_MATPRV_DRAW, "Halo",571, 86, 77, 18, &(ma->mode), 0, 0, 0, 0, "Render as a halo");
		uiDefButI(block, TOG|BIT|6, 0,	"ZTransp",			571, 67, 77, 18, &(ma->mode), 0, 0, 0, 0, "Z-Buffer transparent faces");
		uiDefButI(block, TOG|BIT|8, 0,	"ZInvert",			571, 48, 77, 18, &(ma->mode), 0, 0, 0, 0, "Render with inverted Z Buffer");
		uiDefButI(block, TOG|BIT|9, 0,	"Env",			571, 29, 77, 18, &(ma->mode), 0, 0, 0, 0, "Do not render material");
		uiDefButI(block, TOG|BIT|10, 0,	"OnlyShadow",		571, 10, 77, 18, &(ma->mode), 0, 0, 0, 0, "Let alpha be determined on the degree of shadow");
		/* transparent solids */
/*  		uiDefButI(block, TOG|BIT|0, 0,	"Transp",		571,-10, 77, 18, &(ma->mode2), 0, 0, 0, 0, ""); */

		uiDefButI(block, TOG|BIT|14, 0,	"No Mist",		477,95,77,18, &(ma->mode), 0, 0, 0, 0, "Set the material insensitive to mist");
		uiDefButI(block, TOG|BIT|11, B_REDR,	"TexFace",		398,95,77,18, &(ma->mode), 0, 0, 0, 0, "UV-Editor assigned texture gives color and texture info for the faces");
	}
	/* PREVIEW RENDER */
	
	BIF_previewdraw(G.buts);

	uiDefIconButC(block, ROW, B_MATPRV, ICON_MATPLANE,		10,195,25,20, &(ma->pr_type), 10, 0, 0, 0, "");
	uiDefIconButC(block, ROW, B_MATPRV, ICON_MATSPHERE,		35,195,25,20, &(ma->pr_type), 10, 1, 0, 0, "");
	uiDefIconButC(block, ROW, B_MATPRV, ICON_MATCUBE,		60,195,25,20, &(ma->pr_type), 10, 2, 0, 0, "");

	uiDefIconButS(block, ICONTOG|BIT|0, B_MATPRV, ICON_TRANSP_HLT,		95,195,25,20, &(ma->pr_back), 0, 0, 0, 0, "");
	
	uiDefIconBut(block, BUT, B_MATPRV, ICON_EYE,		159,195,30,20, 0, 0, 0, 0, 0, "");

	/* TEX CHANNELS */
	uiBlockSetCol(block, BUTGREY);
	xco= 665;
	for(a= 0; a<8; a++) {
		mtex= ma->mtex[a];
		if(mtex && mtex->tex) splitIDname(mtex->tex->id.name+2, str, &loos);
		else strcpy(str, "");
		str[10]= 0;
		uiDefButC(block, ROW, B_MATPRV_DRAW, str,	xco, 195, 63, 20, &(ma->texact), 3.0, (float)a, 0, 0, "");
		xco+= 65;
	}
	
	uiDefIconBut(block, BUT, B_MTEXCOPY, ICON_COPYUP,	(short)xco,195,20,21, 0, 0, 0, 0, 0, "Copy the material settings to the buffer");
	uiDefIconBut(block, BUT, B_MTEXPASTE, ICON_PASTEUP,	(short)(xco+20),195,20,21, 0, 0, 0, 0, 0, "Paste the material settings from the buffer");

	
	uiBlockSetCol(block, BUTGREEN);
	uiDefButC(block, TOG, B_MATPRV, "SepT", (short)(xco+40), 195, 40, 20, &(ma->septex), 0, 0, 0, 0, "Render only use active texture channel");
	uiBlockSetCol(block, BUTGREY);

	mtex= ma->mtex[ ma->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
	}
	
	/* TEXCO */
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, ROW, B_MATPRV, "Object",		694,166,49,18, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "Use linked object's coordinates for texture coordinates");
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "",		745,166,133,18, &(mtex->object), "");
	uiDefButS(block, ROW, B_MATPRV, "UV",			664,166,29,18, &(mtex->texco), 4.0, (float)TEXCO_UV, 0, 0, "Use UV coordinates for texture coordinates");

	uiDefButS(block, ROW, B_MATPRV, "Glob",			665,146,35,18, &(mtex->texco), 4.0, (float)TEXCO_GLOB, 0, 0, "Use global coordinates for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Orco",			701,146,38,18, &(mtex->texco), 4.0, (float)TEXCO_ORCO, 0, 0, "Use the original coordinates of the mesh");
	uiDefButS(block, ROW, B_MATPRV, "Stick",			739,146,38,18, &(mtex->texco), 4.0, (float)TEXCO_STICKY, 0, 0, "Use mesh sticky coordaintes for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Win",			779,146,31,18, &(mtex->texco), 4.0, (float)TEXCO_WINDOW, 0, 0, "Use screen coordinates as texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Nor",			811,146,32,18, &(mtex->texco), 4.0, (float)TEXCO_NORM, 0, 0, "Use normal vector as texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Refl",			844,146,33,18, &(mtex->texco), 4.0, (float)TEXCO_REFL, 0, 0, "Use reflection vector as texture coordinates");
	
	uiBlockSetCol(block, BUTGREY);
	
	/* COORDS */
	uiDefButC(block, ROW, B_MATPRV, "Flat",			666,114,48,18, &(mtex->mapping), 5.0, (float)MTEX_FLAT, 0, 0, "Map X and Y coordinates directly");
	uiDefButC(block, ROW, B_MATPRV, "Cube",			717,114,50,18, &(mtex->mapping), 5.0, (float)MTEX_CUBE, 0, 0, "Map using the normal vector");
	uiDefButC(block, ROW, B_MATPRV, "Tube",			666,94,48,18, &(mtex->mapping), 5.0, (float)MTEX_TUBE, 0, 0, "Map with Z as central axis (tube-like)");
	uiDefButC(block, ROW, B_MATPRV, "Sphe",			716,94,50,18, &(mtex->mapping), 5.0, (float)MTEX_SPHERE, 0, 0, "Map with Z as central axis (sphere-like)");

	xco= 665;
	for(a=0; a<4; a++) {
		if(a==0) strcpy(str, "");
		else if(a==1) strcpy(str, "X");
		else if(a==2) strcpy(str, "Y");
		else strcpy(str, "Z");
		
		uiDefButC(block, ROW, B_MATPRV, str,			(short)xco, 50, 24, 18, &(mtex->projx), 6.0, (float)a, 0, 0, "");
		uiDefButC(block, ROW, B_MATPRV, str,			(short)xco, 30, 24, 18, &(mtex->projy), 7.0, (float)a, 0, 0, "");
		uiDefButC(block, ROW, B_MATPRV, str,			(short)xco, 10, 24, 18, &(mtex->projz), 8.0, (float)a, 0, 0, "");
		xco+= 26;
	}
	
	uiDefButF(block, NUM, B_MATPRV, "ofsX",		778,114,100,18, mtex->ofs, -10.0, 10.0, 10, 0, "Fine tune X coordinate");
	uiDefButF(block, NUM, B_MATPRV, "ofsY",		778,94,100,18, mtex->ofs+1, -10.0, 10.0, 10, 0, "Fine tune Y coordinate");
	uiDefButF(block, NUM, B_MATPRV, "ofsZ",		778,74,100,18, mtex->ofs+2, -10.0, 10.0, 10, 0, "Fine tune Z coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	778,50,100,18, mtex->size, -100.0, 100.0, 10, 0, "Set an extra scaling for the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	778,30,100,18, mtex->size+1, -100.0, 100.0, 10, 0, "Set an extra scaling for the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	778,10,100,18, mtex->size+2, -100.0, 100.0, 10, 0, "Set an extra scaling for the texture coordinate");
	
	/* TEXTUREBLOK SELECT */
	if(G.main->tex.first==0)
		id= NULL;
	else
		id= (ID*) mtex->tex;
	IDnames_to_pupstring(&strp, NULL, "ADD NEW %x32767", &(G.main->tex), id, &(G.buts->texnr));
	uiDefButS(block, MENU, B_EXTEXBROWSE, strp, 900,146,20,19, &(G.buts->texnr), 0, 0, 0, 0, "The name of the texture");
	MEM_freeN(strp);

	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	900,166,163,19, id->name+2, 0.0, 18.0, 0, 0, "The name of the texture block");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,				996,146,21,19, 0, 0, 0, 0, 0, "");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 1041,146,21,19, 0, 0, 0, 0, 0, "Auto-assign name to texture");
		if(id->lib) {
			if(ma->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	1019,146,21,19, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	1019,146,21,19, 0, 0, 0, 0, 0, "");		
		}
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_TEXCLEAR, "Clear", 922, 146, 72, 19, 0, 0, 0, 0, 0, "Erase link to datablock");
		uiBlockSetCol(block, BUTGREY);
	}
	
	/* TEXTURE OUTPUT */
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	900,114,52,18, &(mtex->texflag), 0, 0, 0, 0, "Set the mapping to stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		954,114,38,18, &(mtex->texflag), 0, 0, 0, 0, "Reverse the effect of the texture");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "No RGB",	994,114,69,18, &(mtex->texflag), 0, 0, 0, 0, "Use an RGB texture as an intensity texture");
	
	uiDefButF(block, COL, B_MTEXCOL, "",				900,100,163,12, &(mtex->r), 0, 0, 0, 0, "Browse datablocks");

	if(ma->colormodel==MA_HSV) {
		uiBlockSetCol(block, BUTPURPLE);
		uiDefButF(block, HSVSLI, B_MATPRV, "H ",			900,80,163,18, &(mtex->r), 0.0, 0.9999, B_MTEXCOL, 0, "");
		uiBlockSetCol(block, BUTPURPLE);
		uiDefButF(block, HSVSLI, B_MATPRV, "S ",			900,60,163,18, &(mtex->r), 0.0001, 1.0, B_MTEXCOL, 0, "");
		uiBlockSetCol(block, BUTPURPLE);
		uiDefButF(block, HSVSLI, B_MATPRV, "V ",			900,40,163,18, &(mtex->r), 0.0001, 1.0, B_MTEXCOL, 0, "");
		uiBlockSetCol(block, BUTGREY);
	}
	else {
		uiDefButF(block, NUMSLI, B_MATPRV, "R ",			900,80,163,18, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "Set the amount of red the intensity texture blends with");
		uiDefButF(block, NUMSLI, B_MATPRV, "G ",			900,60,163,18, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "Set the amount of green the intensity texture blends with");
		uiDefButF(block, NUMSLI, B_MATPRV, "B ",			900,40,163,18, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "Set the amount of blue the intensity texture blends with");
	}
	
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		900,10,163,18, &(mtex->def_var), 0.0, 1.0, 0, 0, "Set the value the texture blends with the current value");
	
	/* MAP TO */
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Col",		1087,166,35,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect basic colour of the material");
	uiDefButS(block, TOG3|BIT|1, B_MATPRV, "Nor",		1126,166,31,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the rendered normal");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Csp",		1160,166,34,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the specularity colour");
	uiDefButS(block, TOG|BIT|3, B_MATPRV, "Cmir",		1196,166,35,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affext the mirror colour");
	uiDefButS(block, TOG3|BIT|4, B_MATPRV, "Ref",		1234,166,31,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the value of the materials reflectivity");
	uiDefButS(block, TOG3|BIT|5, B_MATPRV, "Spec",	1087,146,36,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the value of specularity");
	uiDefButS(block, TOG3|BIT|8, B_MATPRV, "Hard",	1126,146,44,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the hardness value");
	uiDefButS(block, TOG3|BIT|7, B_MATPRV, "Alpha",	1172,146,45,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the alpha value");
	uiDefButS(block, TOG3|BIT|6, B_MATPRV, "Emit",	1220,146,45,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the emit value");
	
/* 	uiDefButS(block, TOG|BIT|3, B_MATPRV, "Alpha Mix",1087,114,100,18, &(mtex->texflag), 0, 0, 0, 0); ,""*/

	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, ROW, B_MATPRV, "Mix",			1087,94,48,18, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "The texture blends the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,94,44,18, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "The texture multiplies the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,94,41,18, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "The texture adds the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,94,40,18, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "The texture subtracts the values or colour");
	
	uiDefButF(block, NUMSLI, B_MATPRV, "Col ",		1087,50,179,18, &(mtex->colfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor ",		1087,30,179,18, &(mtex->norfac), 0.0, 5.0, 0, 0, "Set the amount the texture affects the normal");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var ",		1087,10,179,18, &(mtex->varfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects a value");
	
	uiDrawBlock(block);
}


/* ************************ SOUND *************************** */
static void load_new_sample(char *str)	/* called from fileselect */
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	bSound *sound;
	bSample *sample, *newsample;

	sound = G.buts->lockpoin;

	if (sound) {
		// save values
		sample = sound->sample;
		strcpy(name, sound->sample->name);

		strcpy(sound->name, str);
		sound_set_sample(sound, NULL);
		sound_initialize_sample(sound);

		if (sound->sample->type == SAMPLE_INVALID) {
			error("Not a valid sample: %s", str);

			newsample = sound->sample;

			// restore values
			strcpy(sound->name, name);
			sound_set_sample(sound, sample);

			// remove invalid sample

			sound_free_sample(newsample);
			BLI_remlink(samples, newsample);
			MEM_freeN(newsample);
		}
	}

	allqueue(REDRAWBUTSSOUND, 0);
	if (curarea) BIF_preview_changed(G.buts);
}


void do_soundbuts(unsigned short event)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	bSound *sound;
	bSample *sample;
	bSound* tempsound;
	ID *id;
	
	sound = G.buts->lockpoin;
	
	switch(event)
	{
	case B_SOUND_REDRAW:
		{
			allqueue(REDRAWBUTSSOUND, 0);
			break;
		}
	case B_SOUND_LOAD_SAMPLE:
		{
			if (sound) strcpy(name, sound->name);
			else strcpy(name, U.sounddir);
			
			activate_fileselect(FILE_SPECIAL, "SELECT WAV FILE", name, load_new_sample);
			break;
		}
	case B_SOUND_PLAY_SAMPLE:
		{
			if (sound)
			{
				if (sound->sample->type != SAMPLE_INVALID)
				{
					sound_play_sound(sound);
					allqueue(REDRAWBUTSSOUND, 0);
				}
			}
			break;
		}
	case B_SOUND_MENU_SAMPLE:
		{
			if (G.buts->menunr == -2) {
				if (sound) {
					activate_databrowse((ID *)sound->sample, ID_SAMPLE, 0, B_SOUND_MENU_SAMPLE, &G.buts->menunr, do_soundbuts);
				}
			} else if (G.buts->menunr > 0) {
				sample = BLI_findlink(samples, G.buts->menunr - 1);
				if (sample && sound) {
					BLI_strncpy(sound->name, sample->name, sizeof(sound->name));
					sound_set_sample(sound, sample);
					do_soundbuts(B_SOUND_REDRAW);
				}
			}
			
			break;
		}
	case B_SOUND_NAME_SAMPLE:
		{
			load_new_sample(sound->name);
			break;
		}
	case B_SOUND_UNPACK_SAMPLE:
		if(sound && sound->sample) {
			sample = sound->sample;
			
			if (sample->packedfile) {
				if (G.fileflags & G_AUTOPACK) {
					if (okee("Disable AutoPack ?")) {
						G.fileflags &= ~G_AUTOPACK;
					}
				}
				
				if ((G.fileflags & G_AUTOPACK) == 0) {
					unpackSample(sample, PF_ASK);
				}
			} else {
				sound_set_packedfile(sample, newPackedFile(sample->name));
			}
			allqueue(REDRAWHEADERS, 0);
			do_soundbuts(B_SOUND_REDRAW);
		}
		break;
	case B_SOUND_COPY_SOUND:
		{
			if (sound)
			{
				tempsound = sound_make_copy(sound);
				sound = tempsound;
				id = &sound->id;
				G.buts->lockpoin = (bSound*)id;
				do_soundbuts(B_SOUND_REDRAW);
			}
			break;
		}
	case B_SOUND_LOOPSTART:
		{
#ifdef SOUND_UNDER_DEVELOPMENT
/*			if (sound->loopstart > sound->loopend)
				sound->loopstart = sound->loopend;*/
#endif
			allqueue(REDRAWBUTSSOUND, 0);
			BIF_preview_changed(G.buts);
			break;
		}
	case B_SOUND_LOOPEND:
		{
#ifdef SOUND_UNDER_DEVELOPMENT
/*			if (sound->loopend < sound->loopstart)
				sound->loopend = sound->loopstart;*/
#endif
			allqueue(REDRAWBUTSSOUND, 0);
			BIF_preview_changed(G.buts);
			break;
		}

	default:
		{
			if (G.f & G_DEBUG)
			{
				printf("do_soundbuts: unhandled event %d\n", event);
			}
			break;
		}
	}
}


void soundbuts(void)
{
	short xco, yco, xcostart = 20;
	bSound *sound;
	bSample *sample;
	uiBlock *block;
	char *strp, str[32];
	ID *id;
	char ch[20];
	char sampleinfo[200];
	char mixrateinfo[50];
	int mixrate;
	
	sound = G.buts->lockpoin;
	yco = 195;

	if (sound)
	{
		sound_initialize_sample(sound);

		sample = sound->sample;

		xco = xcostart;
		sprintf(str, "buttonswin %d", curarea->win);
		block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
		
		uiSetButLock(sound->id.lib!=0, "Can't edit library data");

		/* sound settings ------------------------------------------------------------------ */

		uiDefBut(block, LABEL, 0, "Sound settings:",xco,yco,195,20, 0, 0, 0, 0, 0, "");

		yco -= 30;
		uiBlockSetCol(block, BUTGREEN);
		uiDefBut(block, BUT, B_SOUND_PLAY_SAMPLE, "Play", xco, yco, 195, 24, 0, 0.0, 0, 0, 0,
			"Playback sample using settings below");
		
		uiBlockSetCol(block, BUTGREY);
		xco += 225;

		if (sound->sample && sound->sample->len)
		{
			if (sound->sample->channels == 1)
				strcpy(ch, "Mono");
			else if (sound->sample->channels == 2)
				strcpy(ch, "Stereo");
			else
				strcpy(ch, "Unknown");
			
			uiDefBut(block, LABEL, 0, "Sample: ",xco,yco,195,20, 0, 0, 0, 0, 0, "");
			xco +=55;
			sprintf(sampleinfo, "%s, %d bit, %d Hz, %d samples", ch, sound->sample->bits, sound->sample->rate, sound->sample->len);
			uiDefBut(block, LABEL, 0, sampleinfo,xco,yco,295,20, 0, 0, 0, 0, 0, "");
		}
		else
		{
			uiDefBut(block, LABEL, 0, "No sample info available.",xco,yco,195,20, 0, 0, 0, 0, 0, "");
			xco +=55;
		}

		xco += 314;
		uiDefBut(block, BUT, B_SOUND_COPY_SOUND, "Copy sound", 
			xco,yco,95,24, 0, 0, 0, 0, 0, "Make a copy of the current sound");
		/*
		xco += 25;
		if (sample->channels > 1)
		{
			xco += 100;
			uiDefButC(block, ROW, B_SOUND_REDRAW, "Left",	xco, yco, 95, 20, &sound->channels, 1.0, (float)SOUND_CHANNELS_LEFT, 0, 0, "");
			xco += 100;
			uiDefButC(block, ROW, B_SOUND_REDRAW, "Stereo",	xco, yco, 95, 20, &sound->channels, 1.0, (float)SOUND_CHANNELS_STEREO, 0, 0, "");
			xco += 100;
			uiDefButC(block, ROW, B_SOUND_REDRAW, "Right",	xco, yco, 95, 20, &sound->channels, 1.0, (float)SOUND_CHANNELS_RIGHT, 0, 0, "");
		}
		*/
		
		xco = xcostart;
		yco -= 30;
		uiDefBut(block, BUT, B_SOUND_LOAD_SAMPLE, "Load sample", 
			xco, yco,195,24, 0, 0, 0, 0, 0, "Load a different sample");
		
		uiBlockSetCol(block, BUTGREY);
		
		id= (ID *)sound->sample;
		IDnames_to_pupstring(&strp, NULL, NULL, samples, id, &(G.buts->menunr));
		if (strp[0]) {
			xco += 200;
			uiDefButS(block, MENU, B_SOUND_MENU_SAMPLE, strp,xco,yco,23,24, &(G.buts->menunr), 0, 0, 0, 0, "Select another loaded sample");
		}
		MEM_freeN(strp);
		
		xco += 25;
		uiDefBut(block, TEX, B_SOUND_NAME_SAMPLE, "",xco,yco,412,24, sound->name, 0.0, 79.0, 0, 0, "The sample used by this sound");
		
		sprintf(str, "1");
		// sprintf(str, "%d", tex->ima->id.us);
		xco += 415;
		uiDefBut(block, BUT, B_SOUND_UNLINK_SAMPLE, str,xco,yco,23,24, 0, 0, 0, 0, 0, "The number of users");
		
		if (sound->sample->packedfile)
			packdummy = 1;
		else
			packdummy = 0;
		
		xco += 25;
		uiDefIconButI(block, TOG|BIT|0, B_SOUND_UNPACK_SAMPLE, ICON_PACKAGE,
			xco, yco,24,24, &packdummy, 0, 0, 0, 0,"Pack/Unpack this sample");
		/*
		xco += 25;
		uiDefBut(block, BUT, B_SOUND_RELOAD_SAMPLE, "Reload",xco, yco,68,24, 0, 0, 0, 0, 0, "");
		*/
		/* parameters settings ------------------------------------------------------------------ */
		
		xco = xcostart;
		yco -= 45;
		uiDefBut(block, LABEL, 0, "Parameter settings:",xco,yco,195,20, 0, 0, 0, 0, 0, "");

		yco -= 30;
		uiBlockSetCol(block, BUTGREY);
		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Volume: ",
			xco,yco,195,24,&sound->volume, 0.0, 1.0, 0, 0, "Set the volume of this sound");

		xco += 200;
		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Pitch: ",
			xco,yco,195,24,&sound->pitch, -12.0, 12.0, 0, 0, "Set the pitch of this sound");

		xco = xcostart;
		yco -= 30;
		uiBlockSetCol(block, BUTSALMON);
		uiDefButI(block, TOG|BIT|SOUND_FLAGS_LOOP_BIT, B_SOUND_REDRAW, "Loop",
			xco, yco, 95, 24, &sound->flags, 0.0, 0.0, 0, 0,"Toggle between looping on/off");

		if (sound->flags & SOUND_FLAGS_LOOP)
		{
			xco += 100;
			uiDefButI(block, TOG|BIT|SOUND_FLAGS_BIDIRECTIONAL_LOOP_BIT, B_SOUND_REDRAW, "Ping Pong",
				xco, yco, 95, 24, &sound->flags, 0.0, 0.0, 0, 0,"Toggle between A->B and A->B->A looping");
			
#ifdef SOUND_UNDER_DEVELOPMENT
/*			uiBlockSetCol(block, REDALERT);
			xco += 100;
			uiDefButI(block, NUM, B_SOUND_LOOPSTART, "loopstart: ", xco,yco,195,24,
				&sound->loopstart, 0, sound->sample->len, 0, 0, "Set the startpoint for the loop of this sound");
			
			xco += 200;
			uiDefButI(block, NUM, B_SOUND_LOOPEND, "loopend: ",xco,yco,195,24,
				&sound->loopend, 0, sound->sample->len, 0, 0, "Set the endpoint for the loop of this sound");
*/
#endif
		}

#ifdef SOUND_UNDER_DEVELOPMENT
		xco = xcostart;
		yco -= 30;
		uiDefButI(block, TOG|BIT|SOUND_FLAGS_PRIORITY_BIT, B_SOUND_REDRAW, "Priority",
			xco, yco, 95, 24, &sound->flags, 0.0, 0.0, 0, 0,"Toggle between high and low priority");
#endif

		/* 2D & 3D settings ------------------------------------------------------------------ */

		uiBlockSetCol(block, BUTGREY);
		if (sound->sample->channels == 1)
		{
			xco = xcostart;
			yco -= 30;
			uiDefButI(block, TOG|BIT|SOUND_FLAGS_3D_BIT, B_SOUND_REDRAW, "3D Sound",
				xco, yco, 95, 24, &sound->flags, 0, 0, 0, 0, "Turns 3D sound on");
			
			if (sound->flags & SOUND_FLAGS_3D)
			{
				xco = xcostart;
				yco -= 30;
				uiBlockSetCol(block, BUTGREY);
				uiDefBut(block, LABEL, 0, "3D surround settings:",xco,yco,195,20, 0, 0, 0, 0, 0, "");
				uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Scale: ",
					xco,(short)(yco-=30),195,24,&sound->attenuation, 0.0, 5.0, 1.0, 0, "Sets the world-scaling factor for this sound");
				
					/*
					xco += 200;
					uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Distance: ",
					xco,yco,195,20,&sound->distance, 0.0, 100.0, 1.0, 0, "Reference distance: sets the distance at which the listener will experience gain");
					xco -= 200;
					yco -= 30;
					uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Minvol: ",
					xco,yco,195,20,&sound->min_gain, 0.0, 1.0, 1.0, 0, "Minimal volume: sets the lower threshold for the gain of this sound");
					xco += 200;
					uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Maxvol: ",
					xco,yco,195,20,&sound->max_gain, 0.0, 10.0, 1.0, 0, "Maximal volume: sets the upper threshold for the gain of this sound");
					*/
			}
		}

		/* listener settings ------------------------------------------------------------------ */

		draw_buttons_edge(curarea->win, 740);
		
		xco = xcostart + 750;
		yco = 195;
		uiBlockSetCol(block, BUTGREY);
		mixrate = sound_get_mixrate();
		sprintf(mixrateinfo, "Mixrate: %d Hz", mixrate);
		uiDefBut(block, LABEL, 0, mixrateinfo, xco,yco,295,20, 0, 0, 0, 0, 0, "");

		yco -= 30;

		uiDefBut(block, LABEL, 0, "Listener settings:",xco,yco,195,20, 0, 0, 0, 0, 0, "");

		yco -= 30;
		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Volume: ",
			xco,yco,195,24,&G.listener->gain, 0.0, 1.0, 1.0, 0, "Sets the maximum volume for the overall sound");
		
		yco -= 30;
		uiDefBut(block, LABEL, 0, "Doppler effect settings:",xco,yco,195,20, 0, 0, 0, 0, 0, "");
		/*
		yco -= 30;
		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Scale: ",
			xco,yco,195,20,&G.listener->dopplerfactor, 0.0, 10.0, 1.0, 0, "Doppler scaling: sets the scaling factor for doppler effect");
		*/
		yco -= 30;
		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Doppler: ",
		xco,yco,195,24,&G.listener->dopplervelocity, 0.0, 10.0, 1.0, 0, "Use this for scaling the doppler effect");
		/*
		if (sound->channels != SOUND_CHANNELS_STEREO || sample->channels == 1)
		{
			uiBlockSetCol(block, BUTGREEN);
			uiDefButI(block, TOGN|BIT|SOUND_FLAGS_FIXED_PANNING_BIT, B_SOUND_REDRAW, "3D pan",
				xco, yco, 95, 20, &sound->flags, 0, 0, 0, 0, "");
		
			uiBlockSetCol(block, BUTSALMON);
			xco += 100;
			uiDefButI(block, TOG|BIT|SOUND_FLAGS_FIXED_PANNING_BIT, B_SOUND_REDRAW, "Fixed",
				xco, yco, 95, 20, &sound->flags, 0, 0, 0, 0, "");
		  
			uiBlockSetCol(block, BUTGREY);
			if (sound->flags & SOUND_FLAGS_FIXED_PANNING)
			{
				xco += 100;
				uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Pann: ",
					xco,yco,195,20,&sound->panning, -1.0, 1.0, 0, 0, "");
			}
		}
		*/
		uiDrawBlock(block);
	}
}

/* ************************ LAMP *************************** */

void do_lampbuts(unsigned short event)
{
	Lamp *la;
	MTex *mtex;
		
	switch(event) {
	case B_LAMPREDRAW:
		BIF_preview_changed(G.buts);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_TEXCLEARLAMP:
		la= G.buts->lockpoin;
		mtex= la->mtex[ la->texact ];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
			la->mtex[ la->texact ]= 0;
			allqueue(REDRAWBUTSLAMP, 0);
			allqueue(REDRAWOOPS, 0);
			BIF_preview_changed(G.buts);
		}
		break;
      case B_SBUFF: 
		{ 
			la= G.buts->lockpoin; 
			la->bufsize = la->bufsize&=(~15); 
			allqueue(REDRAWBUTSLAMP, 0); 
			allqueue(REDRAWOOPS, 0); 
			/*la->bufsize = la->bufsize % 64;*/ 
		} 
		break; 
	}
	
	if(event) freefastshade();
}


void lampbuts(void)
{
	Object *ob;
	Lamp *la;
	MTex *mtex;
	ID *id;
	uiBlock *block;
	float grid=0.0;
	int loos, a;
	char *strp, str[32];
	short xco;
	
	if(G.vd) grid= G.vd->grid; 
	if(grid<1.0) grid= 1.0;
	
	ob= OBACT;
	if(ob==0) return;
	if(ob->type!=OB_LAMP) return;

	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	la= ob->data;
	uiSetButLock(la->id.lib!=0, "Can't edit library data");

	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, ROW,B_LAMPREDRAW,"Lamp",	317,190,61,25,&la->type,1.0,(float)LA_LOCAL, 0, 0, "Use a point light source");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Spot",	379,190,59,25,&la->type,1.0,(float)LA_SPOT, 0, 0, "Restrict lamp to conical space");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Sun",	439,190,58,25,&la->type,1.0,(float)LA_SUN, 0, 0, "Light shines from constant direction");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Hemi",	499,190,55,25,&la->type,1.0,(float)LA_HEMI, 0, 0, "Light shines as half a sphere");

	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUM,B_LAMPREDRAW,"Dist:",611,190,104,25,&la->dist, 0.01, 5000.0, 100, 0, "Set the distance value");

	uiBlockSetCol(block, BUTBLUE);
	uiDefButS(block, TOG|BIT|3, B_MATPRV,"Quad",		203,196,100,19,&la->mode, 0, 0, 0, 0, "Use inverse quadratic proportion");
	uiDefButS(block, TOG|BIT|6, REDRAWVIEW3D,"Sphere",203,176,100,19,&la->mode, 0, 0, 0, 0, "Lamp only shines inside a sphere");
	uiDefButS(block, TOG|BIT|0, REDRAWVIEW3D, "Shadows", 203,156,100,19,&la->mode, 0, 0, 0, 0, "Let lamp produce shadows");
 	uiDefButS(block, TOG|BIT|1, 0,"Halo",				203,136,100,19,&la->mode, 0, 0, 0, 0, "Render spotlights with a volumetric halo"); 
	uiDefButS(block, TOG|BIT|2, 0,"Layer",			203,116,100,19,&la->mode, 0, 0, 0, 0, "Illuminate objects in the same layer only");
	uiDefButS(block, TOG|BIT|4, B_MATPRV,"Negative",	203,96,100,19,&la->mode, 0, 0, 0, 0, "Cast negative light");
	uiDefButS(block, TOG|BIT|5, 0,"OnlyShadow",		203,76,100,19,&la->mode, 0, 0, 0, 0, "Render shadow only");
	uiDefButS(block, TOG|BIT|7, B_LAMPREDRAW,"Square",		203,56,100,19,&la->mode, 0, 0, 0, 0, "Use square spotbundles");
#ifdef __SHADOW_EXP
	/* move this elsewhere */
	uiDefButS(block, TOG|BIT|10, 0,"DeepShadow",		203,216,100,19,&la->mode, 0, 0, 0, 0, "");	
#endif
	
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, NUM,B_SBUFF,"ShadowBuffSize:",	203,30,140,19,	&la->bufsize,512,5120, 0, 0, "Set the size of the shadow buffer");
	uiDefButF(block, NUM,REDRAWVIEW3D,"ClipSta:",	346,30,146,19,	&la->clipsta, 0.1*grid,1000.0*grid, 10, 0, "Set the shadow map clip start");
	uiDefButF(block, NUM,REDRAWVIEW3D,"ClipEnd:",	346,9,146,19,&la->clipend, 1.0, 5000.0*grid, 100, 0, "Set the shadow map clip end");

	uiDefButS(block, NUM,0,"Samples:",	496,30,105,19,	&la->samp,1.0,16.0, 0, 0, "Number of shadow map samples");
	uiDefButS(block, NUM,0,"Halo step:",	496,10,105,19,	&la->shadhalostep, 0.0, 12.0, 0, 0, "Volumetric halo sampling frequency");
	uiDefButF(block, NUM,0,"Bias:",		605,30,108,19,	&la->bias, 0.01, 5.0, 1, 0, "Shadow map sampling bias");
	uiDefButF(block, NUM,0,"Soft:",		605,10,108,19,	&la->soft,1.0,100.0, 100, 0, "Set the size of the shadow sample area");
	
	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUMSLI,B_MATPRV,"Energy ",	520,156,195,20, &(la->energy), 0.0, 10.0, 0, 0, "Set the intensity of the light");

	uiDefButF(block, NUMSLI,B_MATPRV,"R ",		520,128,194,20,&la->r, 0.0, 1.0, B_COLLAMP, 0, "Set the red component of the light");
	uiDefButF(block, NUMSLI,B_MATPRV,"G ",		520,108,194,20,&la->g, 0.0, 1.0, B_COLLAMP, 0, "Set the green component of the light");
	uiDefButF(block, NUMSLI,B_MATPRV,"B ",		520,88,194,20,&la->b, 0.0, 1.0, B_COLLAMP, 0, "Set the blue component of the light");
	
	uiDefButF(block, COL, B_COLLAMP, "",			520,64,193,23, &la->r, 0, 0, 0, 0, "");
	
	uiDefButF(block, NUMSLI,B_LAMPREDRAW,"SpotSi ",317,157,192,19,&la->spotsize, 1.0, 180.0, 0, 0, "Set the angle of the spot beam in degrees");
	uiDefButF(block, NUMSLI,B_MATPRV,"SpotBl ",	316,136,192,19,&la->spotblend, 0.0, 1.0, 0, 0, "Set the softness of the spot edge");
	uiDefButF(block, NUMSLI,B_MATPRV,"Quad1 ",	316,106,192,19,&la->att1, 0.0, 1.0, 0, 0, "Set the light intensity value 1 for a quad lamp");
	uiDefButF(block, NUMSLI,B_MATPRV,"Quad2 ",  	317,86,191,19,&la->att2, 0.0, 1.0, 0, 0, "Set the light intensity value 2 for a quad lamp");
	uiDefButF(block, NUMSLI,0,"HaloInt ",		316,64,193,19,&la->haint, 0.0, 5.0, 0, 0, "Set the intensity of the spot halo");


	/* TEX CHANNELS */
	uiBlockSetCol(block, BUTGREY);
	xco= 745;
	for(a= 0; a<6; a++) {
		mtex= la->mtex[a];
		if(mtex && mtex->tex) splitIDname(mtex->tex->id.name+2, str, &loos);
		else strcpy(str, "");
		str[10]= 0;
		uiDefButS(block, ROW, B_REDR, str,	xco, 195, 83, 20, &(la->texact), 3.0, (float)a, 0, 0, "");
		xco+= 85;
	}
	
	mtex= la->mtex[ la->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
		mtex->texco= TEXCO_VIEW;
	}
	
	/* TEXCO */
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, ROW, B_MATPRV, "Object",		745,146,49,18, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "Use linked object's coordinates for texture coordinates");
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "",		745,166,133,18, &(mtex->object), "");
	uiDefButS(block, ROW, B_MATPRV, "Glob",			795,146,45,18, &(mtex->texco), 4.0, (float)TEXCO_GLOB, 0, 0, "Generate texture coordinates from global coordinates");
	uiDefButS(block, ROW, B_MATPRV, "View",			839,146,39,18, &(mtex->texco), 4.0, (float)TEXCO_VIEW, 0, 0, "Generate texture coordinates from view coordinates");
	
	uiBlockSetCol(block, BUTGREY);	
	uiDefButF(block, NUM, B_MATPRV, "dX",		745,114,133,18, mtex->ofs, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dY",		745,94,133,18, mtex->ofs+1, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dZ",		745,74,133,18, mtex->ofs+2, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	745,50,133,18, mtex->size, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	745,30,133,18, mtex->size+1, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	745,10,133,18, mtex->size+2, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");

	/* TEXTUREBLOK SELECT */
	id= (ID *)mtex->tex;
	IDnames_to_pupstring(&strp, NULL, "ADD NEW %x 32767", &(G.main->tex), id, &(G.buts->texnr));
	
	/* doesnt work, because lockpoin points to lamp, not to texture */
	uiDefButS(block, MENU, B_LTEXBROWSE, strp, 900,146,20,19, &(G.buts->texnr), 0, 0, 0, 0, "Select an existing texture, or create new");	
	MEM_freeN(strp);
	
	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	900,166,163,19, id->name+2, 0.0, 18.0, 0, 0, "Name of the texture block");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,				996,146,21,19, 0, 0, 0, 0, 0, "Select an existing texture, or create new");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 1041,146,21,19, 0, 0, 0, 0, 0, "Auto assign a name to the texture");
		if(id->lib) {
			if(la->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	1019,146,21,19, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	1019,146,21,19, 0, 0, 0, 0, 0, "");	
		}
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_TEXCLEARLAMP, "Clear", 922, 146, 72, 19, 0, 0, 0, 0, 0, "Erase link to texture");
		uiBlockSetCol(block, BUTGREY);
	}

	/* TEXTURE OUTPUT */
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	900,114,52,18, &(mtex->texflag), 0, 0, 0, 0, "Set the mapping to stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		954,114,38,18, &(mtex->texflag), 0, 0, 0, 0, "Apply the inverse of the texture");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "RGBtoInt",	994,114,69,18, &(mtex->texflag), 0, 0, 0, 0, "Use an RGB texture as an intensity texture");
	
	uiDefButF(block, COL, B_MTEXCOL, "",				900,100,163,12, &(mtex->r), 0, 0, 0, 0, "");
	uiDefButF(block, NUMSLI, B_MATPRV, "R ",			900,80,163,18, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "Set the red component of the intensity texture to blend with");
	uiDefButF(block, NUMSLI, B_MATPRV, "G ",			900,60,163,18, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "Set the green component of the intensity texture to blend with");
	uiDefButF(block, NUMSLI, B_MATPRV, "B ",			900,40,163,18, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "Set the blue component of the intensity texture to blend with");
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		900,10,163,18, &(mtex->def_var), 0.0, 1.0, 0, 0, "Set the value the texture blends with");
	
	/* MAP TO */
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Col",		1087,166,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the colour of the lamp");
	
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, ROW, B_MATPRV, "Blend",			1087,114,48,18, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "Mix the values");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,114,44,18, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "Multiply the values");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,114,41,18, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "Add the values");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,114,40,18, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "Subtract the values");
	
	uiDefButF(block, NUMSLI, B_MATPRV, "Col ",		1087,50,179,18, &(mtex->colfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects the colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor ",		1087,30,179,18, &(mtex->norfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects the normal");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var ",		1087,10,179,18, &(mtex->varfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects the value");


	BIF_previewdraw(G.buts);

	uiDrawBlock(block);
}

/* ***************************** ANIM ************************** */

void do_animbuts(unsigned short event)
{
	Object *ob;
	Base *base;
	Effect *eff, *effn;
	int type;
	
	ob= OBACT;

	switch(event) {
		
	case B_RECALCPATH:
		calc_curvepath(OBACT);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_MUL_IPO:
		scale_editipo();
		allqueue(REDRAWBUTSANIM, 0);
		break;
	case B_AUTOTIMEOFS:
		auto_timeoffs();
		break;
	case B_FRAMEMAP:
		G.scene->r.framelen= G.scene->r.framapto;
		G.scene->r.framelen/= G.scene->r.images;
		break;
	case B_NEWEFFECT:
		if(ob) {
			if (BLI_countlist(&ob->effect)==MAX_EFFECT)
				error("Unable to add: effect limit reached");
			else
				copy_act_effect(ob);
		}
		allqueue(REDRAWBUTSANIM, 0);
		break;
	case B_DELEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			effn= eff->next;
			if(eff->flag & SELECT) {
				BLI_remlink(&ob->effect, eff);
				free_effect(eff);
				break;
			}
			eff= effn;
		}
		allqueue(REDRAWBUTSANIM, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_NEXTEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) {
				if(eff->next) {
					eff->flag &= ~SELECT;
					eff->next->flag |= SELECT;
				}
				break;
			}
			eff= eff->next;
		}
		allqueue(REDRAWBUTSANIM, 0);
		break;
	case B_PREVEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) {
				if(eff->prev) {
					eff->flag &= ~SELECT;
					eff->prev->flag |= SELECT;
				}
				break;
			}
			eff= eff->next;
		}
		allqueue(REDRAWBUTSANIM, 0);
		break;
	case B_CHANGEEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) {
				if(eff->type!=eff->buttype) {
					BLI_remlink(&ob->effect, eff);
					type= eff->buttype;
					free_effect(eff);
					eff= add_effect(type);
					BLI_addtail(&ob->effect, eff);
				}
				break;
			}
			eff= eff->next;
		}
		allqueue(REDRAWBUTSANIM, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_CALCEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) {
				if(eff->type==EFF_PARTICLE) build_particle_system(ob);
				else if(eff->type==EFF_WAVE) object_wave(ob);
			}
			eff= eff->next;
		}
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSANIM, 0);
		break;
	case B_RECALCAL:
		base= FIRSTBASE;
		while(base) {
			if(base->lay & G.vd->lay) {
				ob= base->object;
				eff= ob->effect.first;
				while(eff) {
					if(eff->flag & SELECT) {
						if(eff->type==EFF_PARTICLE) build_particle_system(ob);
					}
					eff= eff->next;
				}
			}
			base= base->next;
		}
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_SETSPEED:
		set_speed_editipo(hspeed);
		break;
	case B_PRINTSPEED:
		ob= OBACT;
		if(ob) {
			float vec[3];
			CFRA++;
			do_ob_ipo(ob);
			where_is_object(ob);
			VECCOPY(vec, ob->obmat[3]);
			CFRA--;
			do_ob_ipo(ob);
			where_is_object(ob);
			VecSubf(vec, vec, ob->obmat[3]);
			prspeed= Normalise(vec);
			scrarea_queue_winredraw(curarea);
		}
		break;
	case B_PRINTLEN:
		ob= OBACT;
		if(ob && ob->type==OB_CURVE) {
			Curve *cu=ob->data;
			
			if(cu->path) prlen= cu->path->totdist; else prlen= -1.0;
			scrarea_queue_winredraw(curarea);
		} 
		break;
	case B_RELKEY:
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWBUTSANIM, 0);
		allqueue(REDRAWIPO, 0);
		break;
		
	default:
		if(event>=B_SELEFFECT && event<B_SELEFFECT+MAX_EFFECT) {
			ob= OBACT;
			if(ob) {
				int a=B_SELEFFECT;
				
				eff= ob->effect.first;
				while(eff) {
					if(event==a) eff->flag |= SELECT;
					else eff->flag &= ~SELECT;
					
					a++;
					eff= eff->next;
				}
				allqueue(REDRAWBUTSANIM, 0);
			}
		}
	}
}

void animbuts(void)
{
	Object *ob;
	Mesh *me;
	Lattice *lt;
	Effect *eff;
	Curve *cu;
	ScrArea *sa;
	uiBlock *block;
	int a, ok;
	char str[32];
	short x, y;
	
	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	uiDefButS(block, NUM,REDRAWSEQ,"Sta:",	320,17,93,27,&G.scene->r.sfra,1.0,18000.0, 0, 0, "Specify the start frame of the animation");
	uiDefButS(block, NUM,REDRAWSEQ,"End:",	416,17,95,27,&G.scene->r.efra,1.0,18000.0, 0, 0, "Specify the end frame of the animation");

	uiDefButS(block, NUM,B_FRAMEMAP,"Map Old:",	320,69,93,22,&G.scene->r.framapto,1.0,900.0, 0, 0, "Specify old map value in frames");
	uiDefButS(block, NUM,B_FRAMEMAP,"Map New:",	416,69,95,22,&G.scene->r.images,1.0,900.0, 0, 0, "Specify new map value in frames");

	uiDefButS(block, NUM, 0, "AnimSpeed:",	320,47,192,19, &G.animspeed, 1.0, 9.0, 0, 0, "Set the maximum speed of the animation");
	
	ob= OBACT;
	if(ob) {
	
		uiBlockSetCol(block, BUTGREEN);
/* 		uiDefButC(block, TOG|BIT|1, REDRAWVIEW3D, "Quaternions",	320,190,192,19, &ob->transflag, 0.0, 0.0, 0, 0, "Use quaternions for rotation"); */
		uiBlockSetCol(block, BUTGREY);

		uiDefButF(block, NUM, REDRAWALL, "TimeOffset:",				23,18,114,30, &ob->sf, -9000.0, 9000.0, 100, 0, "Specify an offset in frames");
	
		uiBlockSetCol(block, BUTGREEN);
		uiDefButC(block, TOG|BIT|0, REDRAWVIEW3D, "Draw Key",		25,144,84,19, &ob->ipoflag, 0, 0, 0, 0, "Draw object as key position");
		uiDefButC(block, TOG|BIT|1, REDRAWVIEW3D, "Draw Key Sel",	25,123,84,19, &ob->ipoflag, 0, 0, 0, 0, "Limit the drawing of object keys");
		
		uiDefButC(block, TOG|BIT|2, REDRAWALL, "Offs Ob",			25,64,60,20, &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on its own objectipo");
		uiDefButC(block, TOG|BIT|6, REDRAWALL, "Offs Par",		85,64,60,20, &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on the parent");
		uiDefButC(block, TOG|BIT|7, REDRAWALL, "Offs Parti",		145,64,60,20, &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on the particle effect");
	
		uiDefButS(block, TOG|BIT|4, 0, "SlowPar",			205,64,60,20, &ob->partype, 0, 0, 0, 0, "Create a delay in the parent relationship");
	
		/* uiDefButC(block, TOG|BIT|5, REDRAWALL, "Offs Path",	85,64,60,20, &ob->ipoflag, 0, 0, 0, 0); ,""*/
		/* uiDefButC(block, TOG|BIT|3, REDRAWALL, "Offs Mat",		145,64,60,20, &ob->ipoflag, 0, 0, 0, 0); ,""*/
		/* uiDefButC(block, TOG|BIT|4, REDRAWALL, "Offs VertKey",	205,64,60,20, &ob->ipoflag, 0, 0, 0, 0); ,""*/
	
	
		uiBlockSetCol(block, BUTGREY);
		uiDefButC(block, TOG|BIT|3, REDRAWVIEW3D, "DupliFrames",	112,144,106,19, &ob->transflag, 0, 0, 0, 0, "Make copy of object for every frame");
		uiDefButC(block, TOG|BIT|4, REDRAWVIEW3D, "DupliVerts",	112,123,80,19, &ob->transflag, 0, 0, 0, 0, "Duplicate child objects on all vertices");
		uiBlockSetCol(block, BUTGREEN);
		uiDefButC(block, TOG|BIT|5, REDRAWVIEW3D, "Rot",	194,123,24,19, &ob->transflag, 0, 0, 0, 0, "Rotate dupli according to facenormal");
	
		uiBlockSetCol(block, BUTGREY);
		uiDefButS(block, NUM, REDRAWVIEW3D, "DupSta:",	220,144,93,19, &ob->dupsta, 1.0, 1500.0, 0, 0, "Specify startframe for Dupliframes");
		uiDefButS(block, NUM, REDRAWVIEW3D, "DupEnd",		315,144,93,19, &ob->dupend, 1.0, 2500.0, 0, 0, "Specify endframe for Dupliframes");
		uiDefButS(block, NUM, REDRAWVIEW3D, "DupOn:",		220,123,93,19, &ob->dupon, 1.0, 1500.0, 0, 0, "");
		uiDefButS(block, NUM, REDRAWVIEW3D, "DupOff",		315,123,93,19, &ob->dupoff, 0.0, 1500.0, 0, 0, "");
		uiBlockSetCol(block, BUTGREEN);
		uiDefButC(block, TOG|BIT|6, REDRAWVIEW3D, "No Speed",		410,144,93,19, &ob->transflag, 0, 0, 0, 0, "Set dupliframes to still, regardless of frame");
		uiDefButC(block, TOG|BIT|7, REDRAWVIEW3D, "Powertrack",	410,123,93,19, &ob->transflag, 0, 0, 0, 0, "Switch objects rotation off");
	
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_AUTOTIMEOFS, "Automatic Time",		140,18,104,31, 0, 0, 0, 0, 0, "Generate automatic timeoffset values for all selected frames");
		uiBlockSetCol(block, BUTGREY);
		sprintf(str, "%.4f", prspeed);
		uiDefBut(block, LABEL, 0, str,			247,40,63,31, 0, 1.0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_PRINTSPEED,	"PrSpeed",	247,18,63,31, 0, 0, 0, 0, 0, "Print objectspeed");
		
		if(ob->type==OB_MESH) {
			me= ob->data;
			if(me->key) {
				uiDefButS(block, NUM, B_DIFF, "Slurph:",				125,101,93,19, &(me->key->slurph), -500.0, 500.0, 0, 0, "");
				uiDefButS(block, TOG, B_RELKEY, "Relative Keys",	220,100,93,19, &me->key->type, 0, 0, 0, 0, "");
			}
		}
		if(ob->type==OB_CURVE) {
			cu= ob->data;
			uiDefButS(block, NUM, B_RECALCPATH, "PathLen:",			34,100,90,19, &cu->pathlen, 1.0, 9000.0, 0, 0, "");
			/* if(cu->key==0) { */
				uiDefButS(block, TOG|BIT|3, B_RECALCPATH, "CurvePath",	125,100,90,19 , &cu->flag, 0, 0, 0, 0, "");
				uiDefButS(block, TOG|BIT|4, REDRAWVIEW3D, "CurveFollow",	216,100,90,19, &cu->flag, 0, 0, 0, 0, "");
			/* } */
			sprintf(str, "%.4f", prlen);
			uiDefBut(block, LABEL, 0, str,			396,100,90,19, 0, 1.0, 0, 0, 0, "");
			uiDefBut(block, BUT, B_PRINTLEN,		"PrintLen",	306,100,90,19, 0, 0, 0, 0, 0, "");
		}
		if(ob->type==OB_SURF) {
			cu= ob->data;
			
			if(cu->key) {
				/* uiDefButS(block, NUM, B_DIFF, "Slurph:",				124,100,93,19, &(cu->key->slurph), -500.0, 500.0,0,0); ,""*/
				uiDefButS(block, TOG, B_RELKEY, "Relative Keys",	220,100,93,19, &cu->key->type, 0, 0, 0, 0, "");
			}
		}
		if(ob->type==OB_LATTICE) {
			lt= ob->data;
			if(lt->key) {
				uiDefButS(block, NUM, B_DIFF, "Slurph:",				124,100,93,19, &(lt->key->slurph), -500.0, 500.0, 0, 0, "");
				uiDefButS(block, TOG, B_RELKEY, "Relative Keys",	370,190,133,19, &lt->key->type, 0, 0, 0, 0, "");
			}
		}
		
		uiBlockSetCol(block, BUTGREEN);
		uiDefButC(block, ROW,REDRAWVIEW3D,"TrackX",	27,190,58,17, &ob->trackflag, 12.0, 0.0, 0, 0, "Specify the axis that points to another object");
		uiDefButC(block, ROW,REDRAWVIEW3D,"Y",		85,190,19,17, &ob->trackflag, 12.0, 1.0, 0, 0, "Specify the axis that points to another object");
		uiDefButC(block, ROW,REDRAWVIEW3D,"Z",		104,190,19,17, &ob->trackflag, 12.0, 2.0, 0, 0, "Specify the axis that points to another object");
		uiDefButC(block, ROW,REDRAWVIEW3D,"-X",		123,190,24,17, &ob->trackflag, 12.0, 3.0, 0, 0, "Specify the axis that points to another object");
		uiDefButC(block, ROW,REDRAWVIEW3D,"-Y",		147,190,24,17, &ob->trackflag, 12.0, 4.0, 0, 0, "Specify the axis that points to another object");
		uiDefButC(block, ROW,REDRAWVIEW3D,"-Z",		171,190,24,17, &ob->trackflag, 12.0, 5.0, 0, 0, "Specify the axis that points to another object");
		uiDefButC(block, ROW,REDRAWVIEW3D,"UpX",		205,190,40,17, &ob->upflag, 13.0, 0.0, 0, 0, "Specify the axis that points up");
		uiDefButC(block, ROW,REDRAWVIEW3D,"Y",		245,190,20,17, &ob->upflag, 13.0, 1.0, 0, 0, "Specify the axis that points up");
		uiDefButC(block, ROW,REDRAWVIEW3D,"Z",		265,190,19,17, &ob->upflag, 13.0, 2.0, 0, 0, "Specify the axis that points up");
	
		uiBlockSetCol(block, BUTSALMON);
		
		/* EFFECTS */
		
		draw_buttons_edge(curarea->win, 540);
		draw_buttons_edge(curarea->win, 1010);
		
		if (ob->type == OB_MESH) {
			uiDefBut(block, BUT, B_NEWEFFECT, "NEW Effect", 550,187,124,27, 0, 0, 0, 0, 0, "Create a new effect");
			uiDefBut(block, BUT, B_DELEFFECT, "Delete", 676,187,62,27, 0, 0, 0, 0, 0, "Delete the effect");
		}

		uiBlockSetCol(block, BUTGREY);
		
		/* select effs */
		eff= ob->effect.first;
		a= 0;
		while(eff) {
			
			x= 15 * a + 550;
			y= 172; // - 12*( abs(a/10) ) ;
			uiDefButS(block, TOG|BIT|0, B_SELEFFECT+a, "", x, y, 15, 12, &eff->flag, 0, 0, 0, 0, "");
			
			a++;
			if(a==MAX_EFFECT) break;
			eff= eff->next;
		}
		
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) break;
			eff= eff->next;
		}
		
		if(eff) {
			uiDefButS(block, MENU, B_CHANGEEFFECT, "Build %x0|Particles %x1|Wave %x2", 895,187,107,27, &eff->buttype, 0, 0, 0, 0, "Start building the effect");
			
			if(eff->type==EFF_BUILD) {
				BuildEff *bld;
				
				bld= (BuildEff *)eff;
				
				uiDefButF(block, NUM, 0, "Len:",			649,138,95,21, &bld->len, 1.0, 9000.0, 100, 0, "Specify the total time the building requires");
				uiDefButF(block, NUM, 0, "Sfra:",			746,138,94,22, &bld->sfra, 1.0, 9000.0, 100, 0, "Specify the startframe of the effect");
			}
			else if(eff->type==EFF_WAVE) {
				WaveEff *wav;
				
				wav= (WaveEff *)eff;
				
				uiBlockSetCol(block, BUTGREEN);
				uiDefButS(block, TOG|BIT|1, B_CALCEFFECT, "X",		782,135,54,23, &wav->flag, 0, 0, 0, 0, "Enable X axis");
				uiDefButS(block, TOG|BIT|2, B_CALCEFFECT, "Y",		840,135,47,23, &wav->flag, 0, 0, 0, 0, "Enable Y axis");
				uiDefButS(block, TOG|BIT|3, B_CALCEFFECT, "Cycl",		890,135,111,23, &wav->flag, 0, 0, 0, 0, "Enable cyclic wave efefct");
				
				uiBlockSetCol(block, BUTGREY);
				uiDefButF(block, NUM, B_CALCEFFECT, "Sta x:",		550,135,113,24, &wav->startx, -100.0, 100.0, 100, 0, "Starting position for the X axis");
				uiDefButF(block, NUM, B_CALCEFFECT, "Sta y:",		665,135,104,24, &wav->starty, -100.0, 100.0, 100, 0, "Starting position for the Y axis");
				
				uiDefButF(block, NUMSLI, B_CALCEFFECT, "Speed:",	550,100,216,20, &wav->speed, -2.0, 2.0, 0, 0, "Specify the wave speed");
				uiDefButF(block, NUMSLI, B_CALCEFFECT, "Heigth:",	550,80,216,20, &wav->height, -2.0, 2.0, 0, 0, "Specify the amplitude of the wave");
				uiDefButF(block, NUMSLI, B_CALCEFFECT, "Width:",	550,60,216,20, &wav->width, 0.0, 5.0, 0, 0, "Specify the width of the wave");
				uiDefButF(block, NUMSLI, B_CALCEFFECT, "Narrow:",	550,40,216,20, &wav->narrow, 0.0, 10.0, 0, 0, "Specify how narrow the wave follows");
	
				uiDefButF(block, NUM, B_CALCEFFECT, "Time sta:",	780,100,219,20, &wav->timeoffs, -1000.0, 1000.0, 100, 0, "Specify startingframe of the wave");
	
				uiDefButF(block, NUM, B_CALCEFFECT, "Lifetime:",	780,80,219,20, &wav->lifetime,  -1000.0, 1000.0, 100, 0, "Specify the lifespan of the wave");
				uiDefButF(block, NUM, B_CALCEFFECT, "Damptime:",	780,60,219,20, &wav->damp,  -1000.0, 1000.0, 100, 0, "Specify the dampingtime of the wave");
	
			}
			else if(eff->type==EFF_PARTICLE) {
				PartEff *paf;
				
				paf= (PartEff *)eff;
				
				uiDefBut(block, BUT, B_RECALCAL, "RecalcAll", 741,187,67,27, 0, 0, 0, 0, 0, "Update the particle system");
				uiBlockSetCol(block, BUTGREEN);
				uiDefButS(block, TOG|BIT|2, B_CALCEFFECT, "Static",	825,187,67,27, &paf->flag, 0, 0, 0, 0, "Make static particles");
				uiBlockSetCol(block, BUTGREY);
				
				uiDefButI(block, NUM, B_CALCEFFECT, "Tot:",		550,146,91,20, &paf->totpart, 1.0, 100000.0, 0, 0, "Set the total number of particles");
				if(paf->flag & PAF_STATIC) {
					uiDefButS(block, NUM, REDRAWVIEW3D, "Step:",		644,146,84,20, &paf->staticstep, 1.0, 100.0, 10, 0, "");
				}
				else {
					uiDefButF(block, NUM, B_CALCEFFECT, "Sta:",		644,146,84,20, &paf->sta, -250.0, 9000.0, 100, 0, "Specify the startframe");
					uiDefButF(block, NUM, B_CALCEFFECT, "End:",		731,146,97,20, &paf->end, 1.0, 9000.0, 100, 0, "Specify the endframe");
				}
				uiDefButF(block, NUM, B_CALCEFFECT, "Life:",		831,146,88,20, &paf->lifetime, 1.0, 9000.0, 100, 0, "Specify the life span of the particles");
				uiDefButI(block, NUM, B_CALCEFFECT, "Keys:",		922,146,80,20, &paf->totkey, 1.0, 32.0, 0, 0, "Specify the number of key positions");
				
				uiBlockSetCol(block, BUTGREEN);
				uiDefButS(block, NUM, B_REDR,		"CurMul:",		550,124,91,20, &paf->curmult, 0.0, 3.0, 0, 0, "Multiply the particles");
				uiBlockSetCol(block, BUTGREY);
				uiDefButS(block, NUM, B_CALCEFFECT, "Mat:",		644,124,84,20, paf->mat+paf->curmult, 1.0, 8.0, 0, 0, "Specify the material used for the particles");
				uiDefButF(block, NUM, B_CALCEFFECT, "Mult:",		730,124,98,20, paf->mult+paf->curmult, 0.0, 1.0, 10, 0, "Probability \"dying\" particle spawns a new one.");
				uiDefButS(block, NUM, B_CALCEFFECT, "Child:",	922,124,80,20, paf->child+paf->curmult, 1.0, 600.0, 100, 0, "Specify the number of children of a particle that multiply itself");
				uiDefButF(block, NUM, B_CALCEFFECT, "Life:",		831,124,89,20, paf->life+paf->curmult, 1.0, 600.0, 100, 0, "Specify the lifespan of the next generation particles");
	
				uiDefButF(block, NUM, B_CALCEFFECT, "Randlife:",	550,96,96,20, &paf->randlife, 0.0, 2.0, 10, 0, "Give the particlelife a random variation");
				uiDefButI(block, NUM, B_CALCEFFECT, "Seed:",		652,96,80,20, &paf->seed, 0.0, 255.0, 0, 0, "Set an offset in the random table");
	
				uiDefButF(block, NUM, B_DIFF,			"VectSize",		885,96,116,20, &paf->vectsize, 0.0, 1.0, 10, 0, "Set the speed for Vect");	
				uiBlockSetCol(block, BUTGREEN);
				uiDefButS(block, TOG|BIT|3, B_CALCEFFECT, "Face",				735,96,46,20, &paf->flag, 0, 0, 0, 0, "Emit particles also from faces");
				uiDefButS(block, TOG|BIT|1, B_CALCEFFECT, "Bspline",			782,96,54,20, &paf->flag, 0, 0, 0, 0, "Use B spline formula for particle interpolation");
				uiDefButS(block, TOG, REDRAWVIEW3D, "Vect",					837,96,45,20, &paf->stype, 0, 0, 0, 0, "Give the particles a rotation direction");
				
				uiBlockSetCol(block, BUTPURPLE);
				uiDefButF(block, NUM, B_CALCEFFECT, "Norm:",		550,67,96,20, &paf->normfac, -2.0, 2.0, 10, 0, "Let the mesh give the particle a starting speed");
				uiDefButF(block, NUM, B_CALCEFFECT, "Ob:",		649,67,86,20, &paf->obfac, -1.0, 1.0, 10, 0, "Let the object give the particle a starting speed");
				uiDefButF(block, NUM, B_CALCEFFECT, "Rand:",		738,67,86,20, &paf->randfac, 0.0, 2.0, 10, 0, "Give the startingspeed a random variation");
				uiDefButF(block, NUM, B_CALCEFFECT, "Tex:",		826,67,85,20, &paf->texfac, 0.0, 2.0, 10, 0, "Let the texture give the particle a starting speed");
				uiDefButF(block, NUM, B_CALCEFFECT, "Damp:",		913,67,89,20, &paf->damp, 0.0, 1.0, 10, 0, "Specify the damping factor");
	
				uiBlockSetCol(block, BUTGREY);
				uiDefButF(block, NUM, B_CALCEFFECT, "X:",			550,31,72,20, paf->force, -1.0, 1.0, 1, 0, "Specify the X axis of a continues force");
				uiDefButF(block, NUM, B_CALCEFFECT, "Y:",			624,31,78,20, paf->force+1,-1.0, 1.0, 1, 0, "Specify the Y axis of a continues force");
				uiDefBut(block, LABEL, 0, "Force:",						550,9,72,20, 0, 1.0, 0, 0, 0, "");
				uiDefButF(block, NUM, B_CALCEFFECT, "Z:",			623,9,79,20, paf->force+2, -1.0, 1.0, 1, 0, "Specify the Z axis of a continues force");
	
				uiDefBut(block, LABEL, 0, "Texture:",				722,9,74,20, 0, 1.0, 0, 0, 0, "");
				uiBlockSetCol(block, BUTGREEN);
				uiDefButS(block, ROW, B_CALCEFFECT, "Int",		875,9,32,43, &paf->texmap, 14.0, 0.0, 0, 0, "Use texture intensity as a factor for texture force");
				uiDefButS(block, ROW, B_CALCEFFECT, "RGB",		911,31,45,20, &paf->texmap, 14.0, 1.0, 0, 0, "Use RGB values as a factor for particle speed");
				uiDefButS(block, ROW, B_CALCEFFECT, "Grad",		958,31,44,20, &paf->texmap, 14.0, 2.0, 0, 0, "Use texture gradient as a factor for particle speed");
				uiBlockSetCol(block, BUTGREY);
				uiDefButF(block, NUM, B_CALCEFFECT, "Nabla:",		911,9,91,20, &paf->nabla, 0.0001, 1.0, 1, 0, "Specify the dimension of the area for gradient calculation");
				uiDefButF(block, NUM, B_CALCEFFECT, "X:",			722,31,74,20, paf->defvec, -1.0, 1.0, 1, 0, "Specify the X axis of a force, determined by the texture");
				uiDefButF(block, NUM, B_CALCEFFECT, "Y:",			798,31,74,20, paf->defvec+1,-1.0, 1.0, 1, 0, "Specify the Y axis of a force, determined by the texture");
				uiDefButF(block, NUM, B_CALCEFFECT, "Z:",			797,9,75,20, paf->defvec+2, -1.0, 1.0, 1, 0, "Specify the Z axis of a force, determined by the texture");
	
			}
		}
	}
	
	/* IPO BUTTONS AS LAST */
	ok= 0;
	if(G.sipo) {
		/* do these exist? */
		sa= G.curscreen->areabase.first;
		while(sa) {
			if(sa->spacetype==SPACE_IPO && sa->spacedata.first==G.sipo) break;
			sa= sa->next;
		}
		if(sa) {
			if(G.sipo->ipo && G.sipo->ipo->curve.first) ok= 1;
		}
	}
	
	uiBlockSetCol(block, BUTGREEN);
	// RMGRP uiDefButC(block, ROW, B_REDR, "Ipo settings",			1020, 180, 100, 19, &G.buts->showgroup, 15.0, 0.0, 0, 0, "");
	// RMGRP uiDefButC(block, ROW, B_REDR, "Group settings",			1120, 180, 100, 19, &G.buts->showgroup, 15.0, 1.0, 0, 0, "");
	uiBlockSetCol(block, BUTGREY);
	
	if(ok && G.buts->showgroup==0) {
		sprintf(str, "%.3f", G.sipo->v2d.tot.xmin);
		uiDefBut(block, LABEL, 0, str,			1020, 140, 100, 19, 0, 0, 0, 0, 0, "");
		sprintf(str, "%.3f", G.sipo->v2d.tot.xmax);
		uiDefBut(block, LABEL, 0, str,			1120, 140, 100, 19, 0, 0, 0, 0, 0, "");
	
		uiDefButF(block, NUM, B_DIFF, "Xmin:",		1020, 120, 100, 19, &G.sipo->tot.xmin, -G.sipo->v2d.max[0], G.sipo->v2d.max[0], 100, 0, "");
		uiDefButF(block, NUM, B_DIFF, "Xmax:",		1120, 120, 100, 19, &G.sipo->tot.xmax, -G.sipo->v2d.max[0], G.sipo->v2d.max[0], 100, 0, "");
		
		sprintf(str, "%.3f", G.sipo->v2d.tot.ymin);
		uiDefBut(block, LABEL, 0, str,			1020, 100, 100, 19, 0, 0, 0, 0, 0, "");
		sprintf(str, "%.3f", G.sipo->v2d.tot.ymax);
		uiDefBut(block, LABEL, 0, str,			1120, 100, 100, 19, 0, 0, 0, 0, 0, "");
	
		uiDefButF(block, NUM, B_DIFF, "Ymin:",		1020, 80, 100, 19, &G.sipo->tot.ymin, -G.sipo->v2d.max[1], G.sipo->v2d.max[1], 100, 0, "");
		uiDefButF(block, NUM, B_DIFF, "Ymax:",		1120, 80, 100, 19, &G.sipo->tot.ymax, -G.sipo->v2d.max[1], G.sipo->v2d.max[1], 100, 0, "");
	
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_MUL_IPO,	"SET",		1220,79,50,62, 0, 0, 0, 0, 0, "");
		
		
		/* SPEED BUTTON */
		uiBlockSetCol(block, BUTGREY);
		uiDefButF(block, NUM, B_DIFF, "Speed:",		1020,23,164,28, &hspeed, 0.0, 180.0, 1, 0, "");
		
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_SETSPEED,	"SET",		1185,23,83,29, 0, 0, 0, 0, 0, "");
	}
	
	if(G.buts->showgroup && G.scene->group) {
		GroupKey *gk;	
		short yco= 140;
		
		gk= G.scene->group->gkey.first;
		while(gk) {
			if(gk==G.scene->group->active) uiBlockSetCol(block, BUTPURPLE);
			else uiBlockSetCol(block, BUTGREY);
			uiDefBut(block, TEX, B_DIFF, "Name:",		1020, yco, 140, 19, &gk->name, 0.0, 31.0, 10, 0, "");
			uiDefButS(block, NUM, B_DIFF, "Sta:",		1160, yco, 60, 19, &gk->sfra, 0.0, 5000.0, 10, 0, "");
			uiDefButS(block, NUM, B_DIFF, "End:",		1220, yco, 50, 19, &gk->efra, 0.0, 5000.0, 10, 0, "");
			yco-= 20;
			gk= gk->next;
		}
	}
	
	uiDrawBlock(block);
}




/* ***************************** WORLD ************************** */

void do_worldbuts(unsigned short event)
{
	World *wrld;
	MTex *mtex;
	
	switch(event) {
	case B_TEXCLEARWORLD:
		wrld= G.buts->lockpoin;
		mtex= wrld->mtex[ wrld->texact ];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
			wrld->mtex[ wrld->texact ]= 0;
			allqueue(REDRAWBUTSWORLD, 0);
			allqueue(REDRAWOOPS, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	}
}

void worldbuts(void)
{
	World *wrld;
	MTex *mtex;
	ID *id;
	uiBlock *block;
	int a, loos;
	char str[30], *strp;
	short xco;

	wrld= G.scene->world;
	if(wrld==0) return;

	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	uiSetButLock(wrld->id.lib!=0, "Can't edit library data");
	uiBlockSetCol(block, BUTGREEN);

	uiDefButS(block, TOG|BIT|1,B_MATPRV,"Real",	286,190,71,19, &wrld->skytype, 0, 0, 0, 0, "Render background with real horizon");
	uiDefButS(block, TOG|BIT|0,B_MATPRV,"Blend",	208,190,74,19, &wrld->skytype, 0, 0, 0, 0, "Render background with natural progression");
	uiDefButS(block, TOG|BIT|2,B_MATPRV,"Paper",	361,190,71,19, &wrld->skytype, 0, 0, 0, 0, "Flatten blend or texture coordinates");
	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUMSLI,B_MATPRV,"HoR ",		200,55,175,18,	&(wrld->horr), 0.0, 1.0, 0,0, "The amount of red of the horizon colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"HoG ",		200,34,175,18,	&(wrld->horg), 0.0, 1.0, 0,0, "The amount of green of the horizon colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"HoB ",		200,13,175,18,	&(wrld->horb), 0.0, 1.0, 0,0, "The amount of blue of the horizon colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeR ",		200,136,175,18,	&(wrld->zenr), 0.0, 1.0, 0,0, "The amount of red of the zenith colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeG ",		200,116,175,18,	&(wrld->zeng), 0.0, 1.0, 0,0, "The amount of green of the zenith colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeB ",		200,96,175,18,	&(wrld->zenb), 0.0, 1.0, 0,0, "The amount of blue of the zenith colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbR ",	380,55,175,18,	&(wrld->ambr), 0.0, 1.0 ,0,0, "The amount of red of the ambient colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbG ",	380,34,175,18,	&(wrld->ambg), 0.0, 1.0 ,0,0, "The amount of red of the ambient colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbB ",	380,13,175,18,	&(wrld->ambb), 0.0, 1.0 ,0,0, "The amount of red of the ambient colour");

	uiDefBut(block, MENU|SHO, 1, physics_pup(),	
	380,152,175,18, &wrld->pad1, 0, 0, 0, 0, "Physics Engine");
	

	/* Activity bubble */	
	// uiDefButS(block, TOG|BIT|3,B_DIFF,	"Do activity culling",
	//		 380,152,175,18, &wrld->mode, 0, 0, 0, 0,
	//		 "Disable logic and physics for far away objects.");
/*  	if (wrld->mode & WO_ACTIVITY_CULLING) { */
	//	uiDefButF(block, NUM,0, "Active R ",
	//			 380,132,175,18,	&(wrld->activityBoxRadius), 0.5, 10000.0, 100.0, 0,
	//			 "Radius for activity culling (in Manhattan length).");
/*  	} */
		
	/* Gravitation for the game worlds */
	uiDefButF(block, NUMSLI,0, "Grav ",
			 380,112,175,18,	&(wrld->gravity), 0.0, 25.0, 0, 0,
			 "Gravitation constant of the game world.");
	
	uiDefButF(block, NUMSLI,0, "Expos ",	380,92,175,18,	&(wrld->exposure), 0.2, 5.0, 0, 0, "Set the lighting time, exposure");

	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|0,REDRAWVIEW3D,"Mist",	571,190,100,19, &wrld->mode, 0, 0, 0, 0, "Enable mist");
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, ROW, B_DIFF, "Qua", 571, 170, 33, 19, &wrld->mistype, 1.0, 0.0, 0, 0, "Use quadratic progression");
	uiDefButS(block, ROW, B_DIFF, "Lin", 604, 170, 33, 19, &wrld->mistype, 1.0, 1.0, 0, 0, "Use linear progression");
	uiDefButS(block, ROW, B_DIFF, "Sqr", 637, 170, 33, 19, &wrld->mistype, 1.0, 2.0, 0, 0, "Use inverse quadratic progression");
	
	uiDefButF(block, NUM,REDRAWVIEW3D, "Sta:",			571,150,100,19, &wrld->miststa, 0.0, 1000.0, 10, 0, "Specify the starting distance of the mist");
	uiDefButF(block, NUM,REDRAWVIEW3D, "Di:",			571,130,100,19, &wrld->mistdist, 0.0,1000.0, 10, 00, "Specify the depth of the mist");
	uiDefButF(block, NUM,B_DIFF,"Hi:",			571,110,100,19, &wrld->misthi,0.0,100.0, 10, 0, "Specify the factor for a less dense mist with increasing height");
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|1,B_DIFF,	"Stars",571,90,100,19, &wrld->mode, 0, 0, 0, 0, "Enable stars");
	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUM,B_DIFF,"StarDist:",	571,70,100,19, &(wrld->stardist), 2.0, 1000.0, 100, 0, "Specify the average distance between two stars");
	uiDefButF(block, NUM,B_DIFF,"MinDist:",	571,50,100,19, &(wrld->starmindist), 0.0, 1000.0, 100, 0, "Specify the minimum distance to the camera");
	uiDefButF(block, NUM,B_DIFF,"Size:",		571,30,100,19, &(wrld->starsize), 0.0, 10.0, 10, 0, "Specify the average screen dimension");
	uiDefButF(block, NUM,B_DIFF,"Colnoise:",	571,10,100,19, &(wrld->starcolnoise), 0.0, 1.0, 100, 0, "Randomize starcolour");


	/* TEX CHANNELS */
	uiBlockSetCol(block, BUTGREY);
	xco= 745;
	for(a= 0; a<6; a++) {
		mtex= wrld->mtex[a];
		if(mtex && mtex->tex) splitIDname(mtex->tex->id.name+2, str, &loos);
		else strcpy(str, "");
		str[10]= 0;
		uiDefButS(block, ROW, REDRAWBUTSWORLD, str,	xco, 195, 83, 20, &(wrld->texact), 3.0, (float)a, 0, 0, "Texture channel");
		xco+= 85;
	}
	
	mtex= wrld->mtex[ wrld->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
		mtex->texco= TEXCO_VIEW;
	}
	
	/* TEXCO */
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, ROW, B_MATPRV, "Object",		745,146,49,18, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "The name of the object used as a source for texture coordinates");
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "",		745,166,133,18, &(mtex->object), "");
	uiDefButS(block, ROW, B_MATPRV, "View",			839,146,39,18, &(mtex->texco), 4.0, (float)TEXCO_VIEW, 0, 0, "Pass camera view vector on to the texture");
	
	uiBlockSetCol(block, BUTGREY);	
	uiDefButF(block, NUM, B_MATPRV, "dX",		745,114,133,18, mtex->ofs, -20.0, 20.0, 10, 0, "Fine tune X coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dY",		745,94,133,18, mtex->ofs+1, -20.0, 20.0, 10, 0, "Fine tune Y coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dZ",		745,74,133,18, mtex->ofs+2, -20.0, 20.0, 10, 0, "Fine tune Z coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	745,50,133,18, mtex->size, -20.0, 20.0, 10, 0, "Set an extra scaling for the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	745,30,133,18, mtex->size+1, -20.0, 20.0, 10, 0, "Set an extra scaling for the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	745,10,133,18, mtex->size+2, -20.0, 20.0, 10, 0, "Set an extra scaling for the texture coordinate");
	
	/* TEXTUREBLOCK SELECT */
	id= (ID *)mtex->tex;
	IDnames_to_pupstring(&strp, NULL, "ADD NEW %x 32767", &(G.main->tex), id, &(G.buts->texnr));
	uiDefButS(block, MENU, B_WTEXBROWSE, strp, 900,146,20,19, &(G.buts->texnr), 0, 0, 0, 0, "Browse");
	MEM_freeN(strp);
	
	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	900,166,163,19, id->name+2, 0.0, 18.0, 0, 0, "Specify the texture name");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,				996,146,21,19, 0, 0, 0, 0, 0, "Number of users");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 1041,146,21,19, 0, 0, 0, 0, 0, "Auto assign name to texture");
		if(id->lib) {
			if(wrld->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	1019,146,21,19, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	1019,146,21,19, 0, 0, 0, 0, 0, "");	
		}
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_TEXCLEARWORLD, "Clear", 922, 146, 72, 19, 0, 0, 0, 0, 0, "Erase link to texture");
		uiBlockSetCol(block, BUTGREY);
	}
	
	/* TEXTURE OUTPUT */
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	900,114,52,18, &(mtex->texflag), 0, 0, 0, 0, "Use stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		954,114,38,18, &(mtex->texflag), 0, 0, 0, 0, "Inverse texture operation");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "RGBtoInt",	994,114,69,18, &(mtex->texflag), 0, 0, 0, 0, "Use RGB values for intensity texure");
	
	uiDefButF(block, COL, B_MTEXCOL, "",				900,100,163,12, &(mtex->r), 0, 0, 0, 0, "");
	uiDefButF(block, NUMSLI, B_MATPRV, "R ",			900,80,163,18, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "The amount of red that blends with the intensity colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "G ",			900,60,163,18, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "The amount of green that blends with the intensity colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "B ",			900,40,163,18, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "The amount of blue that blends with the intensity colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		900,10,163,18, &(mtex->def_var), 0.0, 1.0, 0, 0, "The value that an intensity texture blends with the current value");
	
	/* MAP TO */
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Blend",		1087,166,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture work on the colour progression in the sky");
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Hori",		1172,166,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture work on the colour of the horizon");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "ZenUp",		1087,147,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture work on the colour of the zenith above");
	uiDefButS(block, TOG|BIT|3, B_MATPRV, "ZenDo",		1172,147,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture work on the colour of the zenith below");
	
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, ROW, B_MATPRV, "Blend",			1087,114,48,18, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "The texture blends the values");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,114,44,18, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "The texture multiplies the values");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,114,41,18, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "The texture adds the values");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,114,40,18, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "The texture subtracts the values");
	
	uiDefButF(block, NUMSLI, B_MATPRV, "Col ",		1087,50,179,18, &(mtex->colfac), 0.0, 1.0, 0, 0, "Specify the extent to which the texture works on colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor ",		1087,30,179,18, &(mtex->norfac), 0.0, 1.0, 0, 0, "Specify the extent to which the texture works on the normal");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var ",		1087,10,179,18, &(mtex->varfac), 0.0, 1.0, 0, 0, "Specify the extent to which the texture works on a value");


	BIF_previewdraw(G.buts);

	uiDrawBlock(block);
}


/* ****************************  VIEW ************************ */

static void view3d_change_bgpic_ima(View3D *v3d, Image *newima) {
	if (v3d->bgpic && v3d->bgpic->ima!=newima) {
		if (newima)
			id_us_plus((ID*) newima);
		if (v3d->bgpic->ima)
			v3d->bgpic->ima->id.us--;
		v3d->bgpic->ima= newima;

		if(v3d->bgpic->rect) MEM_freeN(v3d->bgpic->rect);
		v3d->bgpic->rect= NULL;
		
		allqueue(REDRAWBUTSVIEW, 0);
	}
}
static void view3d_change_bgpic_tex(View3D *v3d, Tex *newtex) {
	if (v3d->bgpic && v3d->bgpic->tex!=newtex) {
		if (newtex)
			id_us_plus((ID*) newtex);
		if (v3d->bgpic->tex)
			v3d->bgpic->tex->id.us--;
		v3d->bgpic->tex= newtex;
		
		allqueue(REDRAWBUTSVIEW, 0);
	}
}

static void load_bgpic_image(char *name)
{
	Image *ima;
	View3D *vd;
	
	vd= scrarea_find_space_of_type(curarea, SPACE_VIEW3D);
	if(vd==0 || vd->bgpic==0) return;
	
	ima= add_image(name);
	if(ima) {
		if(vd->bgpic->ima) {
			vd->bgpic->ima->id.us--;
		}
		vd->bgpic->ima= ima;
		
		free_image_buffers(ima);	/* force read again */
		ima->ok= 1;
	}
	allqueue(REDRAWBUTSVIEW, 0);
	
}

void do_viewbuts(unsigned short event)
{
	View3D *vd;
	char *name;
	
	vd= scrarea_find_space_of_type(curarea, SPACE_VIEW3D);
	if(vd==0) return;

	switch(event) {
	case B_LOADBGPIC:
		if(vd->bgpic && vd->bgpic->ima) name= vd->bgpic->ima->name;
		else name= G.ima;
		
		activate_imageselect(FILE_SPECIAL, "SELECT IMAGE", name, load_bgpic_image);
		break;
	case B_BLENDBGPIC:
		if(vd->bgpic && vd->bgpic->rect) setalpha_bgpic(vd->bgpic);
		break;
	case B_BGPICBROWSE:
		if(vd->bgpic) {
			if (G.buts->menunr==-2) {
				activate_databrowse((ID*) vd->bgpic->ima, ID_IM, 0, B_BGPICBROWSE, &G.buts->menunr, do_viewbuts);
			} else if (G.buts->menunr>0) {
				Image *newima= (Image*) BLI_findlink(&G.main->image, G.buts->menunr-1);

				if (newima)
					view3d_change_bgpic_ima(vd, newima);
			}
		}
		break;
	case B_BGPICCLEAR:
		if (vd->bgpic)
			view3d_change_bgpic_ima(vd, NULL);
		break;
	case B_BGPICTEX:
		if (vd->bgpic) {
			if (G.buts->texnr==-2) {
				activate_databrowse((ID*) vd->bgpic->tex, ID_TE, 0, B_BGPICTEX, &G.buts->texnr, do_viewbuts);
			} else if (G.buts->texnr>0) {
				Tex *newtex= (Tex*) BLI_findlink(&G.main->tex, G.buts->texnr-1);
				
				if (newtex)
					view3d_change_bgpic_tex(vd, newtex);
			}
		}
		break;
	case B_BGPICTEXCLEAR:
		if (vd->bgpic)
			view3d_change_bgpic_tex(vd, NULL);
		break;
	}
}

void viewbuts(void)
{
	View3D *vd;
	ID *id;
	uiBlock *block;
	char *strp, str[64];
	
	/* searching for spacedata */
	vd= scrarea_find_space_of_type(curarea, SPACE_VIEW3D);
	if(vd==0) return;
	
	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	if(vd->flag & V3D_DISPBGPIC) {
		if(vd->bgpic==0) {
			vd->bgpic= MEM_callocN(sizeof(BGpic), "bgpic");
			vd->bgpic->size= 5.0;
			vd->bgpic->blend= 0.5;
		}
	}
	
	uiDefButS(block, TOG|BIT|1, REDRAWBUTSVIEW, "BackGroundPic",	347,160,127,29 ,
		&vd->flag, 0, 0, 0, 0, "Display a picture in the 3D background");
	if(vd->bgpic) {
		uiDefButF(block, NUM, B_DIFF, "Size:",
			478,160,82,29, &vd->bgpic->size, 0.1,
			250.0, 100, 0, "Set the size for the width of the BackGroundPic");
		
		id= (ID *)vd->bgpic->ima;
		IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->image), id, &(G.buts->menunr));
		if(strp[0])
			uiDefButS(block, MENU, B_BGPICBROWSE, strp, 347,112,20,19, &(G.buts->menunr), 0, 0, 0, 0, "Browse");
		MEM_freeN(strp);
		
		uiDefBut(block, BUT,	    B_LOADBGPIC, "LOAD",		370,112,189,19, 0, 0, 0, 0, 0, "Specify the BackGroundPic");
		uiDefButF(block, NUMSLI, B_BLENDBGPIC, "Blend:",	347,84,213,19,&vd->bgpic->blend, 0.0,1.0, 0, 0, "Set the BackGroundPic transparency");
		
		if(vd->bgpic->ima)  {
			uiDefBut(block, TEX,	    0,"BGpic: ",			347,136,211,19,&vd->bgpic->ima->name,0.0,100.0, 0, 0, "The Selected BackGroundPic");
			uiDefIconBut(block, BUT, B_BGPICCLEAR, ICON_X, 347+211,112,20,19, 0, 0, 0, 0, 0, "Remove background image link");
		}
		
		/* There is a bug here ... (what bug? where? what is this? - zr) */
		/* texture block: */
		id= (ID *)vd->bgpic->tex;
		IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->tex), id, &(G.buts->texnr));
		if (strp[0]) 
			uiDefButS(block, MENU, B_BGPICTEX, strp,		347, 20, 20,19, &(G.buts->texnr), 0, 0, 0, 0, "Browse");
		MEM_freeN(strp);
		
		uiDefBut(block, LABEL, 0, "Select texture for animated backgroundimage", 370, 20, 300,19, 0, 0, 0, 0, 0, "");
		
		if (id) {
			uiDefBut(block, TEX, B_IDNAME, "TE:",	347,0,211,19, id->name+2, 0.0, 18.0, 0, 0, "");
			uiDefIconBut(block, BUT, B_BGPICTEXCLEAR, ICON_X, 347+211,0,20,19, 0, 0, 0, 0, 0, "Remove background texture link");
		}
	}

	uiDefButF(block, NUM, B_DIFF, "Grid:",			347, 60, 105, 19, &vd->grid, 0.001, 1000.0, 100, 0, "Set the distance between gridlines");
	uiDefButS(block, NUM, B_DIFF, "GridLines:",	452, 60, 105, 19, &vd->gridlines, 0.0, 100.0, 100, 0, "Set the number of gridlines");
	uiDefButF(block, NUM, B_DIFF, "Lens:",			557, 60, 105, 19, &vd->lens, 10.0, 120.0, 100, 0, "Set the lens for the perspective view");
	
	uiDefButF(block, NUM, B_DIFF, "ClipStart:",			347, 40, 105, 19, &vd->near, 0.1*vd->grid, 100.0, 100, 0, "Set startvalue in perspective view mode");
	uiDefButF(block, NUM, B_DIFF, "ClipEnd:",			452, 40, 105, 19, &vd->far, 1.0, 1000.0*vd->grid, 100, 0, "Set endvalue in perspective view mode");

	/* for(b=0; b<8; b++) { */
	/* 	for(a=0; a<8; a++) { */
	/* 		uiDefButC(block, TOG|BIT|(7-a), 0, "", 100+12*a, 100-12*b, 12, 12, &(arr[b]),0,0,0,0); ,""*/
	/* 	} */
	/* } */
	/* DefBut(BUT, 1001, "print",	50,100,50,20, 0, 0, 0, 0,0); */
	
	uiDrawBlock(block);
}

void output_pic(char *name)
{
	strcpy(G.scene->r.pic, name);
	allqueue(REDRAWBUTSRENDER, 0);
}

void backbuf_pic(char *name)
{
	Image *ima;
	
	strcpy(G.scene->r.backbuf, name);
	allqueue(REDRAWBUTSRENDER, 0);

	ima= add_image(name);
	if(ima) {
		free_image_buffers(ima);	/* force read again */
		ima->ok= 1;
	}
}

void ftype_pic(char *name)
{
	strcpy(G.scene->r.ftype, name);
	allqueue(REDRAWBUTSRENDER, 0);
}


/* ****************************  VIEW ************************ */


static void scene_change_set(Scene *sc, Scene *set) {
	if (sc->set!=set) {
		sc->set= set;
		
		allqueue(REDRAWBUTSRENDER, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
}

static void run_playanim(char *file) {
	extern char bprogname[];	/* usiblender.c */
	char str[FILE_MAXDIR+FILE_MAXFILE];
	int pos[2], size[2];

	calc_renderwin_rectangle(R.winpos, pos, size);

	sprintf(str, "%s -a -p %d %d \"%s\"", bprogname, pos[0], pos[1], file);
	system(str);
}

void do_renderbuts(unsigned short event)
{
	ScrArea *sa;
	ID *id;
	char file[FILE_MAXDIR+FILE_MAXFILE];

	switch(event) {

	case B_DORENDER:
		BIF_do_render(0);
		break;
	case B_RTCHANGED:
		allqueue(REDRAWALL, 0);
		break;
	case B_PLAYANIM:
#ifdef WITH_QUICKTIME
		if(G.scene->r.imtype == R_QUICKTIME) 
			makeqtstring(file);
		else
#endif
			makeavistring(file);
		if(BLI_exist(file)) {
			run_playanim(file);
		}
		else {
			makepicstring(file, G.scene->r.sfra);
			if(BLI_exist(file)) {
				run_playanim(file);
			}
			else error("Can't find image: %s", file);
		}
		break;
		
	case B_DOANIM:
		BIF_do_render(1);
		break;
	
	case B_FS_PIC:
		sa= closest_bigger_area();
		areawinset(sa->win);
		activate_fileselect(FILE_SPECIAL, "SELECT OUTPUT PICTURES", G.scene->r.pic, output_pic);
		break;
	case B_FS_BACKBUF:
		sa= closest_bigger_area();
		areawinset(sa->win);
		activate_fileselect(FILE_SPECIAL, "SELECT BACKBUF PICTURE", G.scene->r.backbuf, backbuf_pic);
		break;
	case B_IS_BACKBUF:
		sa= closest_bigger_area();
		areawinset(sa->win);
		activate_imageselect(FILE_SPECIAL, "SELECT BACKBUF PICTURE", G.scene->r.backbuf, backbuf_pic);
		break;
	case B_FS_FTYPE:
		sa= closest_bigger_area();
		areawinset(sa->win);
		activate_fileselect(FILE_SPECIAL, "SELECT FTYPE", G.scene->r.ftype, ftype_pic);
		break;
	case B_IS_FTYPE:
		sa= closest_bigger_area();
		areawinset(sa->win);
		activate_imageselect(FILE_SPECIAL, "SELECT FTYPE", G.scene->r.ftype, ftype_pic);
		break;
	
	case B_PR_PAL:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 54;
		G.scene->r.yasp= 51;
		G.scene->r.size= 100;
		G.scene->r.frs_sec= 25;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;
		
		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWBUTSRENDER, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;

#ifdef WITH_QUICKTIME
	case B_FILETYPEMENU:
		allqueue(REDRAWBUTSRENDER, 0);
#if defined (_WIN32) || defined (__APPLE__)
		// fall through to codec settings if this is the first
		// time R_AVICODEC is selected for this scene.
		if (((G.scene->r.imtype == R_AVICODEC) 
			 && (G.scene->r.avicodecdata == NULL)) ||
			((G.scene->r.imtype == R_QUICKTIME) 
			 && (have_qtcodec == FALSE))) {
		} else {
		  break;
		}
#else /* libquicktime */
		if(G.scene->r.imtype == R_QUICKTIME) {
		  /* i'm not sure if this should be here... */
		  /* set default quicktime codec */
		  if (!G.scene->r.qtcodecdata) {
			G.scene->r.qtcodecdata = MEM_callocN(sizeof(QtCodecData), 
												 "QtCodecData");
			qtcodec_idx = 1;
		  }
			
		  qt_init_codecs();
		  if (qtcodec_idx < 1) qtcodec_idx = 1;	
			
		  G.scene->r.qtcodecdata->fourcc = 
			qtcodecidx_to_fcc(qtcodec_idx-1);
		  qt_init_codecdata(G.scene->r.qtcodecdata);
/* I'm not sure if this is really needed, so don't remove it yet */
#if 0
		  /* get index of codec that can handle a given fourcc */
		  if (qtcodec_idx < 1)
			qtcodec_idx = get_qtcodec_idx(G.scene->r.qtcodecdata->fourcc)+1;

		  /* no suitable codec found, alert user */
		  if (qtcodec_idx < -1) {
			error("no suitable codec found!");
			qtcodec_idx = 1;
		  }
#endif /* 0 */
		}
#endif /*_WIN32 || __APPLE__ */

	case B_SELECTCODEC:
#if defined (_WIN32) || defined (__APPLE__)
		if ((G.scene->r.imtype == R_QUICKTIME)) /* || (G.scene->r.qtcodecdata)) */
			get_qtcodec_settings();
#ifdef _WIN32
		else
			get_avicodec_settings();
#endif /* _WIN32 */
#else /* libquicktime */
		  if (!G.scene->r.qtcodecdata) {
			G.scene->r.qtcodecdata = MEM_callocN(sizeof(QtCodecData), 
												 "QtCodecData");
			qtcodec_idx = 1;
		  }
		if (qtcodec_idx < 1) {
			qtcodec_idx = 1;
			qt_init_codecs();
		}

		G.scene->r.qtcodecdata->fourcc = qtcodecidx_to_fcc(qtcodec_idx-1);
		/* if the selected codec differs from the previous one, reinit it */
		qt_init_codecdata(G.scene->r.qtcodecdata);	
		allqueue(REDRAWBUTSRENDER, 0);
#endif /* _WIN32 || __APPLE__ */
		break;
#endif /* WITH_QUICKTIME */

	case B_PR_FULL:
		G.scene->r.xsch= 1280;
		G.scene->r.ysch= 1024;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWBUTSRENDER, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;
	case B_PR_PRV:
		G.scene->r.xsch= 640;
		G.scene->r.ysch= 512;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 50;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSRENDER, 0);
		break;
	case B_PR_CDI:
		G.scene->r.xsch= 384;
		G.scene->r.ysch= 280;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.15, 0.85, 0.15, 0.85);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSRENDER, 0);
		break;
	case B_PR_PAL169:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 64;
		G.scene->r.yasp= 45;
		G.scene->r.size= 100;
		G.scene->r.frs_sec= 25;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSRENDER, 0);
		break;
	case B_PR_D2MAC:
		G.scene->r.xsch= 1024;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 50;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSRENDER, 0);
		break;
	case B_PR_MPEG:
		G.scene->r.xsch= 368;
		G.scene->r.ysch= 272;
		G.scene->r.xasp= 105;
		G.scene->r.yasp= 100;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSRENDER, 0);
		break;
	case B_PR_PC:
		G.scene->r.xsch= 640;
		G.scene->r.ysch= 480;
		G.scene->r.xasp= 100;
		G.scene->r.yasp= 100;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.0, 1.0, 0.0, 1.0);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSRENDER, 0);
		break;
	case B_PR_PRESET:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 54;
		G.scene->r.yasp= 51;
		G.scene->r.size= 100;
		G.scene->r.mode= R_OSA+R_SHADOW+R_FIELDS;
		G.scene->r.imtype= R_TARGA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSRENDER, 0);
		break;
	case B_PR_PANO:
		G.scene->r.xsch= 36;
		G.scene->r.ysch= 176;
		G.scene->r.xasp= 115;
		G.scene->r.yasp= 100;
		G.scene->r.size= 100;
		G.scene->r.mode |= R_PANORAMA;
		G.scene->r.xparts=  16;
		G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSRENDER, 0);
		break;
	case B_PR_NTSC:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 480;
		G.scene->r.xasp= 10;
		G.scene->r.yasp= 11;
		G.scene->r.size= 100;
		G.scene->r.frs_sec= 30;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;
		
		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWBUTSRENDER, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;

	case B_SETBROWSE:
		id= (ID*) G.scene->set;
		
		if (G.buts->menunr==-2) {
			 activate_databrowse(id, ID_SCE, 0, B_SETBROWSE, &G.buts->menunr, do_renderbuts);
		} else if (G.buts->menunr>0) {
			Scene *newset= (Scene*) BLI_findlink(&G.main->scene, G.buts->menunr-1);
			
			if (newset==G.scene)
				error("Not allowed");
			else if (newset)
				scene_change_set(G.scene, newset);
		}  
		break;
	case B_CLEARSET:
		scene_change_set(G.scene, NULL);
		break;
	}
}

uiBlock *edge_render_menu(void *arg_unused)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks,
			  "edge render", UI_EMBOSSX, UI_HELV,
			  curarea->win);
		
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",
/* 		 285, -20, 230, 100, NULL, */
		 285, -20, 230, 120, NULL,
		 0, 0, 0, 0, "");
	
	uiDefButS(block, NUM, 0,"Eint:",
		  295,50,70,19,
		  &G.scene->r.edgeint, 0.0, 255.0, 0, 0,
		  "Sets edge intensity for Toon shading");
	uiBlockSetCol(block, BUTGREEN);
	uiDefButI(block, TOG, 0,"Shift",
		  365,50,70,19,
		  &G.compat, 0, 0, 0, 0,
		  "For unified renderer: use old offsets for edges");
	uiDefButI(block, TOG, 0,"All",		435,50,70,19,
		  &G.notonlysolid, 0, 0, 0, 0,
		  "For unified renderer: also consider transparent "
		  "faces for toon shading");

	/* colour settings for the toon shading */
	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, COL, B_EDGECOLSLI, "",
		  295,-10,30,60,
		  &(G.scene->r.edgeR), 0, 0, 0, 0,
		  "");
	
	uiDefButF(block, NUMSLI, 0, "R ",
		  325, 30, 180,19,
		  &G.scene->r.edgeR, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "For unified renderer: Colour for edges in toon shading mode.");
	uiDefButF(block, NUMSLI, 0, "G ",
		  325, 10, 180,19,
		  &G.scene->r.edgeG, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "For unified renderer: Colour for edges in toon shading mode.");
	uiDefButF(block, NUMSLI, 0, "B ",
		  325, -10, 180,19,
		  &G.scene->r.edgeB, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "For unified renderer: Colour for edges in toon shading mode.");

	uiDefButS(block, NUM, 0,"AntiShift",
		  365,70,140,19,
		  &(G.scene->r.same_mat_redux), 0, 255.0, 0, 0,
		  "For unified renderer: reduce intensity on boundaries "
		  "with identical materials with this number.");
	
	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}

static uiBlock *post_render_menu(void *arg_unused)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "post render", UI_EMBOSSX, UI_HELV, curarea->win);
		
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-10, 10, 200, 80, NULL, 0, 0, 0, 0, "");
	
	uiDefButF(block, NUMSLI, 0,"Add:",		0,60,180,19,
			 &G.scene->r.postadd, -1.0, 1.0, 0, 0, "");
	uiDefButF(block, NUMSLI, 0,"Mul:",		0,40,180,19,
			 &G.scene->r.postmul, 0.01, 4.0, 0, 0, "");
	uiDefButF(block, NUMSLI, 0,"Gamma:",		0,20,180,19,
			 &G.scene->r.postgamma, 0.2, 2.0, 0, 0, "");

	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}


static uiBlock *framing_render_menu(void *arg_unused)
{
	uiBlock *block;
	short yco = 60, xco = 0;
	int randomcolorindex = 1234;

	block= uiNewBlock(&curarea->uiblocks, "framing_options", UI_EMBOSSX, UI_HELV, curarea->win);

	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-10, -10, 300, 100, NULL, 0, 0, 0, 0, "");

	uiDefBut(block, LABEL, B_NOP, "Framing:", xco, yco, 68,19, 0, 0, 0, 0, 0, "");
	uiDefButC(block, ROW, 0, "Stretch",	xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_SCALE , 0, 0, "Stretch or squeeze the viewport to fill the display window");
	uiDefButC(block, ROW, 0, "Expose",	xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_EXTEND, 0, 0, "Show the entire viewport in the display window, viewing more horizontally or vertically");
	uiDefButC(block, ROW, 0, "Bars",	    xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_BARS  , 0, 0, "Show the entire viewport in the display window, using bar horizontally or vertically");

	yco -= 20;
	xco = 35;

	uiDefButF(block, COL, randomcolorindex, "",                0, yco - 58 + 18, 33, 58, &G.scene->framing.col[0], 0, 0, 0, 0, "");

	uiDefButF(block, NUMSLI, 0, "R ", xco,yco,243,18, &G.scene->framing.col[0], 0.0, 1.0, randomcolorindex, 0, "Set the red component of the bars");
	yco -= 20;
	uiDefButF(block, NUMSLI, 0, "G ", xco,yco,243,18, &G.scene->framing.col[1], 0.0, 1.0, randomcolorindex, 0, "Set the green component of the bars");
	yco -= 20;
	uiDefButF(block, NUMSLI, 0, "B ", xco,yco,243,18, &G.scene->framing.col[2], 0.0, 1.0, randomcolorindex, 0, "Set the blue component of the bars");

	uiBlockSetDirection(block, UI_TOP);

	return block;
}


static char *imagetype_pup(void)
{
	static char string[1024];
	char formatstring[1024];

	strcpy(formatstring, "Save image as: %%t|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d");

#ifdef __sgi
	strcat(formatstring, "|%s %%x%d");	// add space for Movie
#endif

	strcat(formatstring, "|%s %%x%d");	// add space for PNG

#ifdef _WIN32
	strcat(formatstring, "|%s %%x%d");	// add space for AVI Codec
#endif

#ifdef WITH_QUICKTIME
	if(G.have_quicktime)
		strcat(formatstring, "|%s %%x%d");	// add space for Quicktime
#endif

	if(G.have_quicktime) {
		sprintf(string, formatstring,
			"AVI Raw",        R_AVIRAW,
			"AVI Jpeg",       R_AVIJPEG,
#ifdef _WIN32
			"AVI Codec",      R_AVICODEC,
#endif
#ifdef WITH_QUICKTIME
			"QuickTime",      R_QUICKTIME,
#endif
			"Targa",          R_TARGA,
			"Targa Raw",      R_RAWTGA,
			"PNG",            R_PNG,
			"Jpeg",           R_JPEG90,
			"HamX",           R_HAMX,
			"Iris",           R_IRIS,
			"Iris + Zbuffer", R_IRIZ,
			"Ftype",          R_FTYPE,
			"Movie",          R_MOVIE
		);
	} else {
		sprintf(string, formatstring,
			"AVI Raw",        R_AVIRAW,
			"AVI Jpeg",       R_AVIJPEG,
#ifdef _WIN32
			"AVI Codec",      R_AVICODEC,
#endif
			"Targa",          R_TARGA,
			"Targa Raw",      R_RAWTGA,
			"PNG",            R_PNG,
			"Jpeg",           R_JPEG90,
			"HamX",           R_HAMX,
			"Iris",           R_IRIS,
			"Iris + Zbuffer", R_IRIZ,
			"Ftype",          R_FTYPE,
			"Movie",          R_MOVIE
		);
	}

	return (string);
}


void renderbuts(void)
{
	ID *id;
	int a,b;
	uiBlock *block;
	char *strp;
	char str[64];
	int yofs;
	
	sprintf(str, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	
	uiDefBut(block, TEX,0,"",				34,172,257,19,G.scene->r.pic, 0.0,79.0, 0, 0, "Directory/name to save rendered Pics to");
	uiDefBut(block, BUT,B_FS_PIC," ",		10,172,22,19, 0, 0, 0, 0, 0, "Open Fileselect to get Pics dir/name");
	uiDefBut(block, TEX,0,"",				34,149,257,19,G.scene->r.backbuf, 0.0,79.0, 0, 0, "Image to use as background for rendering");
	uiDefBut(block, BUT,B_FS_BACKBUF," ",	21,149,11,19, 0, 0, 0, 0, 0, "Open Fileselect to get Backbuf image");
	uiDefBut(block, TEX,0,"",				34,126,257,19,G.scene->r.ftype,0.0,79.0, 0, 0, "Image to use with FTYPE Image type");
	uiDefBut(block, BUT,B_FS_FTYPE," ",		21,126,11,19, 0, 0, 0, 0, 0, "Open Fileselect to get Ftype image");
	uiDefIconBut(block, BUT, B_CLEARSET, ICON_X, 267,102,24,21, 0, 0, 0, 0, 0, "Remove Set link");

	/* SET BUTTON */
	id= (ID *)G.scene->set;
	IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->scene), id, &(G.buts->menunr));
	if(strp[0])
		uiDefButS(block, MENU, B_SETBROWSE, strp, 10,103,22,19, &(G.buts->menunr), 0, 0, 0, 0, "Scene to link as a Set");
	MEM_freeN(strp);
	
	uiDefBut(block, LABEL, 0, "Set",				295,103,63,19, 0, 0, 0, 0, 0, "");

	uiBlockSetCol(block, BUTBLUE);

	if(G.scene->set) {
		uiSetButLock(1, NULL);
		uiDefIDPoinBut(block, test_scenepoin_but, 0, "",			34,103,231,19, &(G.scene->set), "Name of the Set");
		uiClearButLock();
	}

	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT,B_IS_BACKBUF," ",	10,149,11,19, 0, 0, 0, 0, 0, "Open Imageselect to get Backbuf image");
	uiDefBut(block, BUT,B_IS_FTYPE," ",		10,126,11,19, 0, 0, 0, 0, 0, "Open Imageselect to get Ftype image");
	uiBlockSetCol(block, BUTGREY);

	uiDefBut(block, LABEL,0,"Pics",				295,172,63,19, 0, 0, 0, 0, 0, "");
	uiDefButS(block, TOG|BIT|0, 0,"Backbuf",	295,149,63,19, &G.scene->r.bufflag, 0, 0, 0, 0, "Enable/Disable use of Backbuf image");	
	uiDefBut(block, LABEL,0,"Ftype",				295,126,63,19, 0, 0, 0, 0, 0, "");
	
	uiBlockSetCol(block, BUTGREY);
			
	for(b=0; b<3; b++) 
		for(a=0; a<3; a++)
			uiDefButS(block, TOG|BIT|(3*b+a),800,"",	(short)(34+18*a),(short)(11+12*b),16,10, &R.winpos, 0, 0, 0, 0, "Render window placement on screen");

	uiDefButS(block, ROW, B_REDR, "DispView",	99,28,77,18, &R.displaymode, 0.0, (float)R_DISPLAYVIEW, 0, 0, "Sets render output to display in 3D view");
	uiDefButS(block, ROW, B_REDR, "DispWin",	99,10,78,18, &R.displaymode, 0.0, (float)R_DISPLAYWIN, 0, 0, "Sets render output to display in a seperate window");

	uiDefButS(block, TOG|BIT|4, 0, "Extensions",	190,10,95,18, &G.scene->r.scemode, 0.0, 0.0, 0, 0, "Adds extensions to the output when rendering animations");

	uiBlockSetCol(block, BUTSALMON);
	
 	uiDefBut(block, BUT,B_DORENDER,"RENDER",	369,142,192,47, 0, 0, 0, 0, 0, "Start the rendering");
	
	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, TOG|BIT|1,0,"Shadows",	565,167,122,22, &G.scene->r.mode, 0, 0, 0, 0, "Enable shadow calculation");
	uiDefButS(block, TOG|BIT|10,0,"Panorama",565,142,122,22, &G.scene->r.mode, 0, 0, 0, 0, "Enable panorama rendering (output width is multiplied by Xparts)");
	
	uiDefButS(block, ROW,B_DIFF,"100%",			565,114,121,20,&G.scene->r.size,1.0,100.0, 0, 0, "Set render size to defined size");
	uiDefButS(block, ROW,B_DIFF,"75%",			565,90,36,20,&G.scene->r.size,1.0,75.0, 0, 0, "Set render size to 3/4 of defined size");
	uiDefButS(block, ROW,B_DIFF,"50%",			604,90,40,20,&G.scene->r.size,1.0,50.0, 0, 0, "Set render size to 1/2 of defined size");
	uiDefButS(block, ROW,B_DIFF,"25%",			647,90,39,20,&G.scene->r.size,1.0,25.0, 0, 0, "Set render size to 1/4 of defined size");
	
	uiDefButS(block, TOG|BIT|0, 0, "OSA",		369,114,124,20,&G.scene->r.mode, 0, 0, 0, 0, "Enables Oversampling (Anti-aliasing)");
	uiDefButF(block, NUM,B_DIFF,"Bf:",							495,90,65,20,&G.scene->r.blurfac, 0.01, 5.0, 10, 0, "Sets motion blur factor");
	uiDefButS(block, TOG|BIT|14, 0, "MBLUR",	495,114,66,20,&G.scene->r.mode, 0, 0, 0, 0, "Enables Motion Blur calculation");
	
	uiDefButS(block, ROW,B_DIFF,"5",			369,90,29,20,&G.scene->r.osa,2.0,5.0, 0, 0, "Sets oversample level to 5");
	uiDefButS(block, ROW,B_DIFF,"8",			400,90,29,20,&G.scene->r.osa,2.0,8.0, 0, 0, "Sets oversample level to 8 (Recommended)");
	uiDefButS(block, ROW,B_DIFF,"11",			431,90,33,20,&G.scene->r.osa,2.0,11.0, 0, 0, "Sets oversample level to 11");
	uiDefButS(block, ROW,B_DIFF,"16",			466,90,28,20,&G.scene->r.osa,2.0,16.0, 0, 0, "Sets oversample level to 16");
		
	uiDefButS(block, NUM,B_DIFF,"Xparts:",		369,42,99,31,&G.scene->r.xparts,1.0, 64.0, 0, 0, "Sets the number of horizontal parts to render image in (For panorama sets number of camera slices)");
	uiDefButS(block, NUM,B_DIFF,"Yparts:",		472,42,86,31,&G.scene->r.yparts,1.0, 64.0, 0, 0, "Sets the number of vertical parts to render image in");

	uiDefButS(block, TOG|BIT|6,0,"Fields",564,42,90,31,&G.scene->r.mode, 0, 0, 0, 0, "Enables field rendering");

	uiDefButS(block, TOG|BIT|13,0,"Odd",	655,57,30,16,&G.scene->r.mode, 0, 0, 0, 0, "Enables Odd field first rendering (Default: Even field)");
	uiDefButS(block, TOG|BIT|7,0,"x",		655,42,30,15,&G.scene->r.mode, 0, 0, 0, 0, "Disables time difference in field calculations");

	uiDefButS(block, ROW,800,"Sky",		369,11,38,24,&G.scene->r.alphamode,3.0,0.0, 0, 0, "Fill background with sky");
	uiDefButS(block, ROW,800,"Premul",	410,11,54,24,&G.scene->r.alphamode,3.0,1.0, 0, 0, "Multiply alpha in advance");
	uiDefButS(block, ROW,800,"Key",		467,11,44,24,&G.scene->r.alphamode,3.0,2.0, 0, 0, "Alpha and colour values remain unchanged");

	/* Toon shading buttons */
	uiDefButS(block, TOG|BIT|5, 0,"Edge",	295,70,70,19,
			 &G.scene->r.mode, 0, 0, 0, 0, "Enable Toon shading");
	uiDefBlockBut(block, edge_render_menu, NULL, "Edge Settings |>> ", 155, 70, 138, 19, "Display edge settings");

	/* unified render buttons */
	if(G.scene->r.mode & R_UNIFIED) {
		uiDefBlockBut(block, post_render_menu, NULL, "Post process |>> ", 15, 70, 138, 19, "Only for unified render");
		if (G.scene->r.mode & R_GAMMA) {
			uiDefButF(block, NUMSLI, 0,"Gamma:",		15, 50, 280, 19,
					 &(G.scene->r.gamma), 0.2, 5.0, B_GAMMASLI, 0,
					 "The gamma value for blending oversampled images (1.0 = no correction).");
		}		
	}


	uiDefButS(block, TOG|BIT|9,REDRAWVIEWCAM, "Border",	565,11,58,24, &G.scene->r.mode, 0, 0, 0, 0, "Render a small cut-out of the image");
	uiDefButS(block, TOG|BIT|2,0, "Gamma",	626,11,58,24, &G.scene->r.mode, 0, 0, 0, 0, "Enable gamma correction");

	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT,B_DOANIM,"ANIM",		692,142,192,47, 0, 0, 0, 0, 0, "Start rendering a sequence");
	
	uiBlockSetCol(block, BUTBLUE);

	uiDefButS(block, TOG|BIT|0, 0, "Do Sequence",	692,114,192,20, &G.scene->r.scemode, 0, 0, 0, 0, "Enables sequence output rendering (Default: 3D rendering)");
	uiDefButS(block, TOG|BIT|1, 0, "Render Daemon",	692,90,192,20, &G.scene->r.scemode, 0, 0, 0, 0, "Let external network render current scene");
	
	uiBlockSetCol(block, BUTGREY);
	uiDefBut(block, BUT,B_PLAYANIM, "PLAY",	692,40,94,33, 0, 0, 0, 0, 0, "Play animation of rendered images/avi (searches Pics: field)");

	uiDefButS(block, NUM, B_RTCHANGED, "rt:",	790,40,95,33, &G.rt, 0.0, 256.0, 0, 0, "General testing/debug button");

	uiDefButS(block, ROW,B_DIFF,"BW",			892, 10,74,20, &G.scene->r.planes, 5.0,(float)R_PLANESBW, 0, 0, "Images are saved with BW (grayscale) data");
	uiDefButS(block, ROW,B_DIFF,"RGB",		    968, 10,74,20, &G.scene->r.planes, 5.0,(float)R_PLANES24, 0, 0, "Images are saved with RGB (color) data");
	uiDefButS(block, ROW,B_DIFF,"RGBA",		   1044, 10,75,20, &G.scene->r.planes, 5.0,(float)R_PLANES32, 0, 0, "Images are saved with RGB and Alpha data (if supported)");

	yofs = 54;

#ifdef __sgi
	yofs = 76;
	uiDefButS(block, NUM,B_DIFF,"MaxSize:", 892,32,165,20, &G.scene->r.maximsize, 0.0, 500.0, 0, 0, "Maximum size per frame to save in an SGI movie");
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|12,0,"Cosmo", 1059,32,60,20, &G.scene->r.mode, 0, 0, 0, 0, "Attempt to save SGI movies using Cosmo hardware");
	uiBlockSetCol(block, BUTGREY);
#endif

	uiDefButS(block, MENU,B_FILETYPEMENU,imagetype_pup(),	892,yofs,174,20, &G.scene->r.imtype, 0, 0, 0, 0, "Images are saved in this file format");
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|11,0, "Crop",          1068,yofs,51,20, &G.scene->r.mode, 0, 0, 0, 0, "Exclude border rendering from total image");
	uiBlockSetCol(block, BUTGREY);

	yofs -= 22;

	if(G.scene->r.quality==0) G.scene->r.quality= 90;

#ifdef WITH_QUICKTIME
	if (G.scene->r.imtype == R_AVICODEC || G.scene->r.imtype == R_QUICKTIME) {
#else /* WITH_QUICKTIME */
	if (0) {
#endif
		if(G.scene->r.imtype == R_QUICKTIME) {
#ifdef WITH_QUICKTIME
#if defined (_WIN32) || defined (__APPLE__)
			if(!have_qtcodec)
				uiDefBut(block, LABEL, 0, "Codec: undefined",  892,yofs+42,225,20, 0, 0, 0, 0, 0, "");
			else
				uiDefBut(block, LABEL, 0, qtcdname,  892,yofs+42,225,20, 0, 0, 0, 0, 0, "");
			uiDefBut(block, BUT,B_SELECTCODEC, "Set codec",  892,yofs,112,20, 0, 0, 0, 0, 0, "Set codec settings for Quicktime");
#else /* libquicktime */
			if (!G.scene->r.qtcodecdata) G.scene->r.qtcodecdata = MEM_callocN(sizeof(QtCodecData), "QtCodecData");
			uiDefButI(block, MENU, B_SELECTCODEC, qtcodecs_pup(), 892,yofs, 112, 20, &qtcodec_idx, 0, 0, 0, 0, "Codec");
			/* make sure the codec stored in G.scene->r.qtcodecdata matches the selected
			 * one, especially if it's not set.. */
			if (!G.scene->r.qtcodecdata->fourcc) {
				G.scene->r.qtcodecdata->fourcc = qtcodecidx_to_fcc(qtcodec_idx-1);
				qt_init_codecdata(G.scene->r.qtcodecdata);	
			}
			
			yofs -= 22;
			uiDefBlockBut(block, qtcodec_menu, NULL, "Codec Settings |>> ", 892,yofs, 227, 20, "Edit Codec settings for QuickTime");
			yofs +=22;

#endif /* libquicktime */
#endif /* WITH_QUICKTIME */
		} else {
#if defined (_WIN32) || defined (__APPLE__)
			if(!have_avicodec)
				uiDefBut(block, LABEL, 0, "Codec: not set.",  892,yofs+42,225,20, 0, 0, 0, 0, 0, "");
			else
				uiDefBut(block, LABEL, 0, avicdname,  892,yofs+42,225,20, 0, 0, 0, 0, 0, "");
#endif
			uiDefBut(block, BUT,B_SELECTCODEC, "Set codec",  892,yofs,112,20, 0, 0, 0, 0, 0, "Set codec settings for AVI");
		}
	} else {
		uiDefButS(block, NUM,0, "Quality:",           892,yofs,112,20, &G.scene->r.quality, 10.0, 100.0, 0, 0, "Quality setting for JPEG images, AVI Jpeg and SGI movies");
	}
	uiDefButS(block, NUM,REDRAWSEQ,"Frs/sec:",   1006,yofs,113,20, &G.scene->r.frs_sec, 1.0, 120.0, 100.0, 0, "Frames per second, for AVI and Sequence window grid");

	uiDefButS(block, NUM,REDRAWSEQ,"Sta:",	692,10,94,24, &G.scene->r.sfra,1.0,18000.0, 0, 0, "The start frame of the animation");
	uiDefButS(block, NUM,REDRAWSEQ,"End:",	790,10,95,24, &G.scene->r.efra,1.0,18000.0, 0, 0, "The end  frame of the animation");

	uiDefBlockBut(block, framing_render_menu, NULL, "Game framing settings |>> ", 892, 169, 227, 20, "Display game framing settings");
	
	uiDefButS(block, NUM,REDRAWVIEWCAM,"SizeX:",	892 ,136,112,27, &G.scene->r.xsch, 4.0, 10000.0, 0, 0, "The image width in pixels");
	uiDefButS(block, NUM,REDRAWVIEWCAM,"SizeY:",	1007,136,112,27, &G.scene->r.ysch, 4.0,10000.0, 0, 0, "The image height in scanlines");
	uiDefButS(block, NUM,REDRAWVIEWCAM,"AspX:",	892 ,114,112,20, &G.scene->r.xasp, 1.0,200.0, 0, 0, "The horizontal aspect ratio");
	uiDefButS(block, NUM,REDRAWVIEWCAM,"AspY:",	1007,114,112,20, &G.scene->r.yasp, 1.0,200.0, 0, 0, "The vertical aspect ratio");

	uiDefBut(block, BUT,B_PR_PAL, "PAL",		1146,170,133,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x576, Aspect ratio - 54x51, 25 fps");
	uiDefBut(block, BUT,B_PR_NTSC, "NTSC",		1146,150,133,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x480, Aspect ratio - 10x11, 30 fps");
	uiDefBut(block, BUT,B_PR_PRESET, "Default",	1146,130,133,18, 0, 0, 0, 0, 0, "Same as PAL, with render settings (OSA, Shadows, Fields)");
	uiDefBut(block, BUT,B_PR_PRV, "Preview",	1146,110,133,18, 0, 0, 0, 0, 0, "Size preset: Image size - 640x512, Render size 50%");
	uiDefBut(block, BUT,B_PR_PC, "PC",			1146,90,133,18, 0, 0, 0, 0, 0, "Size preset: Image size - 640x480, Aspect ratio - 100x100");
	uiDefBut(block, BUT,B_PR_PAL169, "PAL 16:9",1146,70,133,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x576, Aspect ratio - 64x45");
	uiDefBut(block, BUT,B_PR_PANO, "PANO",		1146,50,133,18, 0, 0, 0, 0, 0, "Standard panorama settings");
	uiDefBut(block, BUT,B_PR_FULL, "FULL",		1146,30,133,18, 0, 0, 0, 0, 0, "Size preset: Image size - 1280x1024, Aspect ratio - 1x1");
	uiDefButS(block, TOG|BIT|15, B_REDR, "Unified Renderer", 1146,10,133,18,
			 &G.scene->r.mode, 0, 0, 0, 0,
			 "Use the unified renderer.");
	
	uiDrawBlock(block);
}

/* ********************* CONSTRAINT ***************************** */

static void activate_constraint_ipo_func (void *arg1v, void *unused)
{

	bConstraint *con = arg1v;
	bConstraintChannel *chan;
	ListBase *conbase;

	get_constraint_client(NULL, NULL, NULL);

	conbase = get_constraint_client_channels(1);

	if (!conbase)
		return;

	/* See if this list already has an appropriate channel */
	chan = find_constraint_channel(conbase, con->name);

	if (!chan){
		/* Add a new constraint channel */
		chan = add_new_constraint_channel(con->name);
		BLI_addtail(conbase, chan);
	}

	/* Ensure there is an ipo to display */
	if (!chan->ipo){
		chan->ipo = add_ipo(con->name, IPO_CO);
	}

	/* Make this the active channel */
	OBACT->activecon = chan;

	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

static void del_constraint_func (void *arg1v, void *arg2v)
{
	bConstraint *con= arg1v;
	Object *ob;

	ListBase *lb= arg2v;
	
	ob=OBACT;
	
	if (ob->activecon && !strcmp(ob->activecon->name, con->name))
		ob->activecon = NULL;

	free_constraint_data (con);

	BLI_freelinkN(lb, con);

	allqueue(REDRAWBUTSCONSTRAINT, 0);
	allqueue(REDRAWIPO, 0); 

}

static void verify_constraint_name_func (void *data, void *data2_unused)
{
	ListBase *conlist;
	bConstraint *con;
	char ownerstr[64];
	short type;
	
	con = (bConstraint*) data;
	if (!con)
		return;
	
	conlist = get_constraint_client(ownerstr, &type, NULL);
	unique_constraint_name (con, conlist);
}

static void constraint_changed_func (void *data, void *data2_unused)
{
	bConstraint *con = (bConstraint*) data;

	if (con->type == con->otype)
		return;

	free_constraint_data (con);
	con->data = new_constraint_data(con->type);

}

static void move_constraint_func (void *datav, void *data2_unused)
{
	bConstraint *constraint_to_move= datav;
	int val;
	ListBase *conlist;
	char ownerstr[64];
	short	type;
	bConstraint *curCon, *con, *neighbour;
	
	val= pupmenu("Move up%x1|Move down %x2");
	
	con = constraint_to_move;

	if(val>0) {
		conlist = get_constraint_client(ownerstr, &type, NULL);
		for (curCon = conlist->first; curCon; curCon = curCon->next){
			if (curCon == con){
				/* Move up */
				if (val == 1 && con->prev){
					neighbour = con->prev;
					BLI_remlink(conlist, neighbour);
					BLI_insertlink(conlist, con, neighbour);
				}
				/* Move down */
				else if (val == 2 && con->next){
					neighbour = con->next;
					BLI_remlink (conlist, con);
					BLI_insertlink(conlist, neighbour, con);
				}
				break;
			}
		}
	}
}

static void get_constraint_typestring (char *str, bConstraint *con)
{
	switch (con->type){
	case CONSTRAINT_TYPE_CHILDOF:
		strcpy (str, "Child Of");
		return;
	case CONSTRAINT_TYPE_NULL:
		strcpy (str, "Null");
		return;
	case CONSTRAINT_TYPE_TRACKTO:
		strcpy (str, "Track To");
		return;
	case CONSTRAINT_TYPE_KINEMATIC:
		strcpy (str, "IK Solver");
		return;
	case CONSTRAINT_TYPE_ROTLIKE:
		strcpy (str, "Copy Rotation");
		return;
	case CONSTRAINT_TYPE_LOCLIKE:
		strcpy (str, "Copy Location");
		return;
	case CONSTRAINT_TYPE_ACTION:
		strcpy (str, "Action");
		return;
	default:
		strcpy (str, "Unknown");
		return;
	}
}

static BIFColorID get_constraint_col(bConstraint *con)
{
	switch (con->type) {
	case CONSTRAINT_TYPE_NULL:
		return BUTWHITE;
	case CONSTRAINT_TYPE_KINEMATIC:
		return BUTPURPLE;
	case CONSTRAINT_TYPE_TRACKTO:
		return BUTGREEN;
	case CONSTRAINT_TYPE_ROTLIKE:
		return BUTBLUE;
	case CONSTRAINT_TYPE_LOCLIKE:
		return BUTYELLOW;
	case CONSTRAINT_TYPE_ACTION:
		return BUTPINK;
	default:
		return REDALERT;
	}
}

static void draw_constraint (uiBlock *block, ListBase *list, bConstraint *con, short *xco, short *yco, short type)
{
	uiBut *but;
	char typestr[64];
	short height, width = 268;
	BIFColorID curCol;

	uiBlockSetEmboss(block, UI_EMBOSSW);

	get_constraint_typestring (typestr, con);

	curCol = get_constraint_col(con);
	/* Draw constraint header */
	uiBlockSetCol(block, BUTSALMON);

	but = uiDefIconBut(block, BUT, B_CONSTRAINT_REDRAW, ICON_X, *xco, *yco, 20, 20, list, 0.0, 0.0, 0.0, 0.0, "Delete constraint");

	uiButSetFunc(but, del_constraint_func, con, list);

	if (con->flag & CONSTRAINT_EXPAND){
		uiBlockSetCol(block, BUTYELLOW);
		
		if (con->flag & CONSTRAINT_DISABLE)
			uiBlockSetCol(block, REDALERT);
		
		if (type==TARGET_BONE)
			but = uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Bone Constraint%t|Track To%x2|IK Solver%x3|Copy Rotation%x8|Copy Location%x9|Action%x12|Null%x0", *xco+20, *yco, 100, 20, &con->type, 0.0, 0.0, 0.0, 0.0, "Constraint type"); 
		else
			but = uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Object Constraint%t|Track To%x2|Copy Rotation%x8|Copy Location%x9|Null%x0", *xco+20, *yco, 100, 20, &con->type, 0.0, 0.0, 0.0, 0.0, "Constraint type"); 
		
		uiButSetFunc(but, constraint_changed_func, con, NULL);
		con->otype = con->type;
		
		but = uiDefBut(block, TEX, B_CONSTRAINT_REDRAW, "", *xco+120, *yco, 128, 20, con->name, 0.0, 32.0, 0.0, 0.0, "Constraint name"); 
		uiButSetFunc(but, verify_constraint_name_func, con, NULL);
	}	
	else{
		uiBlockSetEmboss(block, UI_EMBOSSP);
		uiBlockSetCol(block, BUTGREY);

		if (con->flag & CONSTRAINT_DISABLE){
			uiBlockSetCol(block, REDALERT);
			BIF_set_color(REDALERT, COLORSHADE_MEDIUM);
		}
		else
			BIF_set_color(curCol, COLORSHADE_MEDIUM);

		glRects(*xco+20, *yco, *xco+248, *yco+20);
		
		but = uiDefBut(block, LABEL, B_CONSTRAINT_TEST, typestr, *xco+20, *yco, 100, 20, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
		uiButSetFunc(but, move_constraint_func, con, NULL);
		but = uiDefBut(block, LABEL, B_CONSTRAINT_TEST, con->name, *xco+120, *yco, 128, 20, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
		uiButSetFunc(but, move_constraint_func, con, NULL);
	}

	uiBlockSetCol(block, BUTGREY);	
	
	uiBlockSetEmboss(block, UI_EMBOSSW);
	uiDefIconButS(block, ICONTOG|BIT|CONSTRAINT_EXPAND_BIT, B_CONSTRAINT_REDRAW, ICON_RIGHTARROW, *xco+248, *yco, 20, 20, &con->flag, 0.0, 0.0, 0.0, 0.0, "Collapse");


	/* Draw constraint data*/
#ifdef __CON_IPO
	if (con->type!=CONSTRAINT_TYPE_NULL)
	{
		uiDefBut(block, NUMSLI|FLO, B_CONSTRAINT_REDRAW, "Influence:", *xco+280, *yco, 196, 20, &con->enforce, 0.0, 1.0, 0.0, 0.0, "Amount of influence this constraint will have on the final solution");
		but = uiDefBut(block, BUT, B_CONSTRAINT_REDRAW, "Edit Ipo", *xco+480, *yco, 64, 20, 0, 0.0, 1.0, 0.0, 0.0, "Show this constraint's ipo in the object's Ipo window");
		/* If this is on an object, add the constraint to the object */
		uiButSetFunc (but, activate_constraint_ipo_func, con, NULL);
		/* If this is on a bone, add the constraint to the action (if any) */
	}
#endif

	if (!(con->flag & CONSTRAINT_EXPAND)){
		(*yco)-=21;
		return;
	}

	switch (con->type){
	case CONSTRAINT_TYPE_ACTION:
		{
			bActionConstraint *data = con->data;
			bArmature *arm;

			height = 86;
			BIF_set_color(curCol, COLORSHADE_GREY);
			glRects(*xco, *yco-height, *xco+width, *yco);
			uiEmboss((float)*xco, (float)*yco-height, (float)*xco+width, (float)*yco, 1);

			/* Draw target parameters */
			uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+((width/2)-48), *yco-20, 96, 18, &data->tar, "Target Object"); 

			arm = get_armature(data->tar);
			if (arm){
				but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+((width/2)-48), *yco-40,96,18, &data->subtarget, 0, 24, 0, 0, "Bone");
			}
			else
				strcpy (data->subtarget, "");

			/* Draw action button */
			uiDefIDPoinBut(block, test_actionpoin_but, B_CONSTRAINT_CHANGETARGET, "AC:", *xco+((width/2)-120), *yco-60, 80, 18, &data->act, "Action containing the keyed motion for this bone"); 

			uiDefButS(block, NUM, B_CONSTRAINT_CHANGETARGET, "Start:", *xco+((width/2)-40), *yco-60, 80, 18, &data->start, 1, 18000, 0.0, 0.0, "Starting frame of the keyed motion"); 
			uiDefButS(block, NUM, B_CONSTRAINT_CHANGETARGET, "End:", *xco+((width/2)+40), *yco-60, 80, 18, &data->end, 1, 18000, 0.0, 0.0, "Ending frame of the keyed motion"); 
			
			/* Draw XYZ toggles */
			uiDefButI(block, MENU, B_CONSTRAINT_REDRAW, "Key on%t|X Rot%x0|Y Rot%x1|Z Rot%x2", *xco+((width/2)-120), *yco-80, 80, 18, &data->type, 0, 24, 0, 0, "Specify which transformation channel from the target is used to key the action");
			uiDefButF(block, NUM, B_CONSTRAINT_REDRAW, "Min:", *xco+((width/2)-40), *yco-80, 80, 18, &data->min, -180, 180, 0, 0, "Minimum value for target channel range");
			uiDefButF(block, NUM, B_CONSTRAINT_REDRAW, "Max:", *xco+((width/2)+40), *yco-80, 80, 18, &data->max, -180, 180, 0, 0, "Maximum value for target channel range");
			
		}
		break;
	case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data = con->data;
			bArmature *arm;
			height = 66;
			BIF_set_color(curCol, COLORSHADE_GREY);
			glRects(*xco, *yco-height, *xco+width, *yco);
			uiEmboss((float)*xco, (float)*yco-height, (float)*xco+width, (float)*yco, 1);

			/* Draw target parameters */
			uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+((width/2)-48), *yco-20, 96, 18, &data->tar, "Target Object"); 

			arm = get_armature(data->tar);
			if (arm){
				but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+((width/2)-48), *yco-40,96,18, &data->subtarget, 0, 24, 0, 0, "Bone");
			}
			else
				strcpy (data->subtarget, "");

			/* Draw XYZ toggles */
				but=uiDefButI(block, TOG|BIT|0, B_CONSTRAINT_TEST, "X", *xco+((width/2)-48), *yco-60, 32, 18, &data->flag, 0, 24, 0, 0, "Copy X component");
				but=uiDefButI(block, TOG|BIT|1, B_CONSTRAINT_TEST, "Y", *xco+((width/2)-16), *yco-60, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Y component");
				but=uiDefButI(block, TOG|BIT|2, B_CONSTRAINT_TEST, "Z", *xco+((width/2)+16), *yco-60, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Z component");
		}
		break;
	case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data = con->data;
			bArmature *arm;
			height = 46;
			BIF_set_color(curCol, COLORSHADE_GREY);
			glRects(*xco, *yco-height, *xco+width, *yco);
			uiEmboss((float)*xco, (float)*yco-height, (float)*xco+width, (float)*yco, 1);

			uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+((width/2)-48), *yco-20, 96, 18, &data->tar, "Target Object"); 

			arm = get_armature(data->tar);
			if (arm){
				but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+((width/2)-48), *yco-40,96,18, &data->subtarget, 0, 24, 0, 0, "Bone");
			}
			else
				strcpy (data->subtarget, "");

		}
		break;
	case CONSTRAINT_TYPE_KINEMATIC:
		{
			bKinematicConstraint *data = con->data;
			bArmature *arm;
			
			height = 66;
			BIF_set_color(curCol, COLORSHADE_GREY);
			glRects(*xco, *yco-height, *xco+width, *yco);
			uiEmboss((float)*xco, (float)*yco-height, (float)*xco+width, (float)*yco, 1);
			
			uiDefButF(block, NUM, B_CONSTRAINT_REDRAW, "Tolerance:", *xco+((width/2)-96), *yco-20, 96, 18, &data->tolerance, 0.0001, 1.0, 0.0, 0.0, "Maximum distance to target after solving"); 
			uiDefButI(block, NUM, B_CONSTRAINT_REDRAW, "Iterations:", *xco+((width/2)), *yco-20, 96, 18, &data->iterations, 1, 10000, 0.0, 0.0, "Maximum number of solving iterations"); 

			uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+((width/2)-48), *yco-40, 96, 18, &data->tar, "Target Object"); 
			
			arm = get_armature(data->tar);
			if (arm){
				but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+((width/2)-48), *yco-60,96,18, &data->subtarget, 0, 24, 0, 0, "Bone");
			}
			else
				strcpy (data->subtarget, "");
			
		}
		break;
	case CONSTRAINT_TYPE_NULL:
		{
			height = 20;
			BIF_set_color(curCol, COLORSHADE_GREY);
			glRects(*xco, *yco-height, *xco+width, *yco);
			uiEmboss((float)*xco, (float)*yco-height, (float)*xco+width, (float)*yco, 1);
		}
		break;
	case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data = con->data;
			bArmature *arm;

			height = 46;
			BIF_set_color(curCol, COLORSHADE_GREY);
			glRects(*xco, *yco-height, *xco+width, *yco);
			uiEmboss((float)*xco, (float)*yco-height, (float)*xco+width, (float)*yco, 1);
			
			uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+((width/2)-48), *yco-20, 96, 18, &data->tar, "Target Object"); 
			
			arm = get_armature(data->tar);
			if (arm){
				but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+((width/2)-48), *yco-40,96,18, &data->subtarget, 0, 24, 0, 0, "Bone");
			}
			else
				strcpy (data->subtarget, "");
		}
		break;
	default:
		height = 0;
		break;
	}

	(*yco)-=(24+height);

}

static void constraintbuts(void)
{
	short xco, yco, type;
	uiBlock *block;
	char str[32];
	ListBase *conlist;
	char ownerstr[64];
	bConstraint *curcon;
	
	xco = 320;
	yco = 195;

	sprintf(str, "buttonswin %d", curarea->win);
	
	block= uiNewBlock(&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);

	conlist = get_constraint_client(ownerstr, &type, NULL);
	
	if (conlist){
		
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_CONSTRAINT_ADD, "Add", xco, yco, 95, 20, 0, 0.0, 0, 0, 0,"Add new constraint");
		
		/* Go through the list of constraints and draw them */
		xco = 465;
		yco = 195;
		
		for (curcon = conlist->first; curcon; curcon=curcon->next)
		{
			/* Draw default constraint header */			
			draw_constraint(block, conlist, curcon, &xco, &yco, type);	
		}
		
	}
	
	uiDrawBlock(block);
	
}

static void do_constraintbuts(unsigned short event)
{
	ListBase *list;
	short	type;

	switch(event) {
	case B_CONSTRAINT_CHANGENAME:
		break;
	case B_CONSTRAINT_TEST:
		test_scene_constraints();
		allqueue (REDRAWVIEW3D, 0);
		allqueue (REDRAWBUTSCONSTRAINT, 0);
		break;
	case B_CONSTRAINT_REDRAW:
		test_scene_constraints();
		allqueue (REDRAWVIEW3D, 0);
		allqueue (REDRAWBUTSCONSTRAINT, 0);
		break;
	case B_CONSTRAINT_CHANGETARGET:
		test_scene_constraints();
		allqueue (REDRAWVIEW3D, 0);
		allqueue (REDRAWBUTSCONSTRAINT, 0);
		break;
	case B_CONSTRAINT_CHANGETYPE:
		test_scene_constraints();
		allqueue (REDRAWVIEW3D, 0);
		allqueue (REDRAWBUTSCONSTRAINT, 0);
		break;
	case B_CONSTRAINT_ADD:
		{
			bConstraint *con;
		//	ListBase *chanbase;
		//	bConstraintChannel *chan;

			Object *ob = OBACT;
			list = get_constraint_client(NULL, &type, NULL);
		//	chanbase= get_constraint_client_channels(0);
			if (list){
				con = add_new_constraint();
				unique_constraint_name(con, list);
		//		chan = add_new_constraint_channel(con->name);
		//		ob->activecon = chan;
		//		BLI_addtail(chanbase, chan);
				BLI_addtail(list, con);
			}
			test_scene_constraints();
			allqueue (REDRAWVIEW3D, 0);
			allqueue (REDRAWBUTSCONSTRAINT, 0);
		}
		break;
	case B_CONSTRAINT_DEL:
		test_scene_constraints();
		allqueue (REDRAWVIEW3D, 0);
		allqueue (REDRAWBUTSCONSTRAINT, 0);
		break;
	default:
		break;
	}
}

/* ********************* GAME ***************************** */

/* in editsca.c */

/* ***************************<>******************************** */

void drawbutspace(ScrArea *sa, void *spacedata)
{
	SpaceButs *sbuts= curarea->spacedata.first;
	View2D *v2d= &sbuts->v2d;
	ID *id;
	Object *ob;
	float vec[2];
	
	if(curarea->headertype==0) {
		ID *id, *idfrom;
		
		buttons_active_id(&id, &idfrom);
		G.buts->lockpoin= id;
	}
	
	ob= OBACT;
	
//	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin-0.6, v2d->cur.ymax+0.6);
	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);
	
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	cpack(0x909090);
	vec[0]= v2d->cur.xmin;
	vec[1]= v2d->cur.ymax-15;
	glVertex2fv(vec);
	vec[0]= v2d->cur.xmax;
	glVertex2fv(vec);
	cpack(0x646464);
	vec[1]= v2d->cur.ymax;
	glVertex2fv(vec);
	vec[0]= v2d->cur.xmin;
	glVertex2fv(vec);
	glEnd();
	glShadeModel(GL_FLAT);
	
	cpack(0x909090);
	glRectf(v2d->cur.xmin,  v2d->cur.ymin,  v2d->cur.xmax,  v2d->cur.ymax-15);

	uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");	
	uiFreeBlocksWin(&curarea->uiblocks, curarea->win);
 
	switch(G.buts->mainb) {
	case BUTS_VIEW:
		viewbuts();
		break;
	case BUTS_LAMP:
		lampbuts();
		break;
	case BUTS_MAT:
		if(ob==0) return;
		if(ob->type>=OB_LAMP) return;
		
		matbuts();
		break;
	case BUTS_TEX:		
		texbuts();
		break;
	case BUTS_ANIM:
		animbuts();
		break;
	case BUTS_WORLD:
		worldbuts();
		break;
	case BUTS_RENDER:
		renderbuts();
		break;
	case BUTS_GAME:
		gamebuts();
		break;
	case BUTS_FPAINT:
		fpaintbuts();
		break;
	case BUTS_RADIO:
		radiobuts();
		break;
	case BUTS_SOUND:
		soundbuts();
		break;
	case BUTS_CONSTRAINT:
		constraintbuts();
		break;
	case BUTS_SCRIPT:
		scriptbuts();
		break;
	case BUTS_EDIT:
		if(ob==0) return;
		
		common_editbuts();

		id= ob->data;
		if(id && id->lib) uiSetButLock(1, "Can't edit library data");

		if(ob->type==OB_MESH) meshbuts();
		else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
			curvebuts();
			if(ob->type==OB_FONT) fontbuts();
		}
		else if(ob->type==OB_CAMERA) camerabuts();
		else if(ob->type==OB_MBALL) mballbuts();
		else if(ob->type==OB_LATTICE) latticebuts();
		else if(ob->type==OB_IKA) ikabuts();
#ifdef __NLA
		else if(ob->type==OB_ARMATURE) armaturebuts();
#endif
			
		break;
	}
	
	uiClearButLock();
	
	test_butspace();
	
	curarea->win_swap= WIN_BACK_OK;
}

void do_blenderbuttons(unsigned short event)
{
	SpaceButs *buts;
	
	/* redraw windows of the same type? */
	buts= curarea->spacedata.first;
	if(buts->mainb==BUTS_VIEW) allqueue(REDRAWBUTSVIEW, curarea->win);
	else if(buts->mainb==BUTS_LAMP) allqueue(REDRAWBUTSLAMP, curarea->win);
	else if(buts->mainb==BUTS_MAT || buts->mainb==BUTS_TEX) {
		allqueue(REDRAWBUTSMAT, curarea->win);
		allqueue(REDRAWBUTSTEX, curarea->win);
	}
	else if(buts->mainb==BUTS_WORLD) allqueue(REDRAWBUTSWORLD, curarea->win);
	else if(buts->mainb==BUTS_ANIM) allqueue(REDRAWBUTSANIM, curarea->win);
	else if(buts->mainb==BUTS_RENDER) allqueue(REDRAWBUTSRENDER, curarea->win);
	else if(buts->mainb==BUTS_EDIT) allqueue(REDRAWBUTSEDIT, curarea->win);
	else if(buts->mainb==BUTS_FPAINT) allqueue(REDRAWBUTSGAME, curarea->win);
	else if(buts->mainb==BUTS_RADIO) allqueue(REDRAWBUTSRADIO, curarea->win);
	else if(buts->mainb==BUTS_SCRIPT) allqueue(REDRAWBUTSSCRIPT, curarea->win);
	else if(buts->mainb==BUTS_SOUND) allqueue(REDRAWBUTSSOUND, curarea->win);
	else if(buts->mainb==BUTS_CONSTRAINT) allqueue(REDRAWBUTSCONSTRAINT, curarea->win);
	
	if(event<=100) {
		do_global_buttons(event);
	}
	else if(event<=B_VIEWBUTS) {
		do_viewbuts(event);
	}
	else if(event<=B_LAMPBUTS) {
		do_lampbuts(event);
	}
	else if(event<=B_MATBUTS) {
		do_matbuts(event);
	}
	else if(event<=B_TEXBUTS) {
		do_texbuts(event);
	}
	else if(event<=B_ANIMBUTS) {
		do_animbuts(event);
	}
	else if(event<=B_WORLDBUTS) {
		do_worldbuts(event);
	}
	else if(event<=B_RENDERBUTS) {
		do_renderbuts(event);
	}
	else if(event<=B_COMMONEDITBUTS) {
		do_common_editbuts(event);
	}
	else if(event<=B_MESHBUTS) {
		do_meshbuts(event);
	}
	else if(event<=B_CURVEBUTS) {
		do_curvebuts(event);
	}
	else if(event<=B_FONTBUTS) {
		do_fontbuts(event);
	}
	else if(event<=B_IKABUTS) {
		do_ikabuts(event);
	}
	else if(event<=B_CAMBUTS) {
		;
	}
	else if(event<=B_MBALLBUTS) {
		do_mballbuts(event);
	}
	else if(event<=B_LATTBUTS) {
		do_latticebuts(event);
	}
	else if(event<=B_GAMEBUTS) {
		do_gamebuts(event);
	}
	else if(event<=B_FPAINTBUTS) {
		do_fpaintbuts(event);
	}
	else if(event<=B_RADIOBUTS) {
		do_radiobuts(event);
	}
	else if(event<=B_SCRIPTBUTS) {
		do_scriptbuts(event);
	}
	else if(event<=B_SOUNDBUTS) {
		do_soundbuts(event);
	}
	else if(event<=B_CONSTRAINTBUTS) {
		do_constraintbuts(event);
	}
	else if(event>=REDRAWVIEW3D) allqueue(event, 0);
}


void redraw_test_buttons(Base *new)
{
	ScrArea *sa;
	SpaceButs *buts;
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->spacetype==SPACE_BUTS) {
			buts= sa->spacedata.first;
			
			if(buts->mainb==BUTS_LAMP) {
				allqueue(REDRAWBUTSLAMP, 0);
				BIF_preview_changed(buts);
			}
			else if(buts->mainb==BUTS_MAT) {
				allqueue(REDRAWBUTSMAT, 0);
				BIF_preview_changed(buts);
			}
			else if(buts->mainb==BUTS_TEX) {
				allqueue(REDRAWBUTSTEX, 0);
				if(new && new->object->type==OB_LAMP) buts->texfrom= 2;
				else buts->texfrom= 0;
				BIF_preview_changed(buts);
			}
			else if(buts->mainb==BUTS_ANIM) {
				allqueue(REDRAWBUTSANIM, 0);
			}			
			else if(buts->mainb==BUTS_EDIT) {
				allqueue(REDRAWBUTSEDIT, 0);
			}			
			else if(buts->mainb==BUTS_GAME) {
				allqueue(REDRAWBUTSGAME, 0);
			}			
			else if(buts->mainb==BUTS_FPAINT) {
				allqueue(REDRAWBUTSGAME, 0);
			}
			else if(buts->mainb==BUTS_SCRIPT) {
				allqueue(REDRAWBUTSSCRIPT, 0);
			}			
			else if(buts->mainb==BUTS_SOUND) {
				allqueue(REDRAWBUTSSOUND, 0);
			}			
			else if(buts->mainb==BUTS_CONSTRAINT) {
				allqueue(REDRAWBUTSCONSTRAINT, 0);
			}
		}
		sa= sa->next;
	}
}

void clever_numbuts_buts()
{
	Material *ma;
	Lamp *la;
	World *wo;
	static char	hexrgb[8]; /* Uh... */
	static char	hexspec[8]; /* Uh... */
	static char	hexmir[8]; /* Uh... */
	static char hexho[8];
	static char hexze[8];
	int		rgb[3];
	
	switch (G.buts->mainb){
	case BUTS_FPAINT:

		sprintf(hexrgb, "%02X%02X%02X", (int)(Gvp.r*255), (int)(Gvp.g*255), (int)(Gvp.b*255));

		add_numbut(0, TEX, "RGB:", 0, 6, hexrgb, "HTML Hex value for the RGB color");
		do_clever_numbuts("Vertex Paint RGB Hex Value", 1, REDRAW); 
		
		/* Assign the new hex value */
		sscanf(hexrgb, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
		Gvp.r= (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
		Gvp.g = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
		Gvp.b = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;

		break;
	case BUTS_LAMP:
		la= G.buts->lockpoin;
		if (la){
			sprintf(hexrgb, "%02X%02X%02X", (int)(la->r*255), (int)(la->g*255), (int)(la->b*255));
			add_numbut(0, TEX, "RGB:", 0, 6, hexrgb, "HTML Hex value for the lamp color");
			do_clever_numbuts("Lamp RGB Hex Values", 1, REDRAW); 
			sscanf(hexrgb, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			la->r = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			la->g = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			la->b = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			BIF_preview_changed(G.buts);
		}
		break;
	case BUTS_WORLD:
		wo= G.buts->lockpoin;
		if (wo){
			sprintf(hexho, "%02X%02X%02X", (int)(wo->horr*255), (int)(wo->horg*255), (int)(wo->horb*255));
			sprintf(hexze, "%02X%02X%02X", (int)(wo->zenr*255), (int)(wo->zeng*255), (int)(wo->zenb*255));
			add_numbut(0, TEX, "Zen:", 0, 6, hexze, "HTML Hex value for the Zenith color");
			add_numbut(1, TEX, "Hor:", 0, 6, hexho, "HTML Hex value for the Horizon color");
			do_clever_numbuts("World RGB Hex Values", 2, REDRAW); 

			sscanf(hexho, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			wo->horr = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			wo->horg = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			wo->horb = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			sscanf(hexze, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			wo->zenr = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			wo->zeng = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			wo->zenb = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			BIF_preview_changed(G.buts);

		}
		break;
	case BUTS_MAT:

		ma= G.buts->lockpoin;

		/* Build a hex value */
		if (ma){
			sprintf(hexrgb, "%02X%02X%02X", (int)(ma->r*255), (int)(ma->g*255), (int)(ma->b*255));
			sprintf(hexspec, "%02X%02X%02X", (int)(ma->specr*255), (int)(ma->specg*255), (int)(ma->specb*255));
			sprintf(hexmir, "%02X%02X%02X", (int)(ma->mirr*255), (int)(ma->mirg*255), (int)(ma->mirb*255));

			add_numbut(0, TEX, "Col:", 0, 6, hexrgb, "HTML Hex value for the RGB color");
			add_numbut(1, TEX, "Spec:", 0, 6, hexspec, "HTML Hex value for the Spec color");
			add_numbut(2, TEX, "Mir:", 0, 6, hexmir, "HTML Hex value for the Mir color");
			do_clever_numbuts("Material RGB Hex Values", 3, REDRAW); 
			
			/* Assign the new hex value */
			sscanf(hexrgb, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			ma->r = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			ma->g = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			ma->b = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			sscanf(hexspec, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			ma->specr = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			ma->specg = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			ma->specb = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			sscanf(hexmir, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			ma->mirr = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			ma->mirg = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			ma->mirb = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			
			BIF_preview_changed(G.buts);
		}
		break;
	}
}
