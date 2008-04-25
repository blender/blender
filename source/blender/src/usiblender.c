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

	/* placed up here because of crappy
	 * winsock stuff.
	 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <windows.h> /* need to include windows.h so _WIN32_IE is defined  */
#ifndef _WIN32_IE
#define _WIN32_IE 0x0400 /* minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already */
#endif
#include <shlobj.h> /* for SHGetSpecialFolderPath, has to be done before BLI_winstuff because 'near' is disabled through BLI_windstuff */
#include "BLI_winstuff.h"
#include <process.h> /* getpid */
#else
#include <unistd.h> /* getpid */
#endif
#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_linklist.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_sound_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_exotic.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_node.h"
#include "BKE_packedFile.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_pointcache.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BLI_vfontdata.h"

#include "BIF_fsmenu.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_usiblender.h"
#include "BIF_drawtext.h"
#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editlattice.h"
#include "BIF_editfont.h"
#include "BIF_editmesh.h"
#include "BIF_editmode_undo.h"
#include "BIF_editsound.h"
#include "BIF_filelist.h"
#include "BIF_poseobject.h"
#include "BIF_previewrender.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_cursors.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif


#include "BSE_drawview.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_node.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_editcurve.h"
#include "BDR_imagepaint.h"
#include "BDR_vpaint.h"

#include "BPY_extern.h"

#include "blendef.h"

#include "RE_pipeline.h"		/* RE_ free stuff */

#include "radio.h"
#include "datatoc.h"

#include "SYS_System.h"

#include "PIL_time.h"

/***/

/* define for setting colors in theme below */
#define SETCOL(col, r, g, b, a)  col[0]=r; col[1]=g; col[2]= b; col[3]= a;

/* patching UserDef struct, set globals for UI stuff */
static void init_userdef_file(void)
{
	
	BIF_InitTheme();	// sets default again
	
	mainwindow_set_filename_to_title("");	// empty string re-initializes title to "Blender"
	countall();
	G.save_over = 0;	// start with save preference untitled.blend
	
	/*  disable autoplay in .B.blend... */
	G.fileflags &= ~G_FILE_AUTOPLAY;
	
	/* the UserDef struct is not corrected with do_versions() .... ugh! */
	if(U.wheellinescroll == 0) U.wheellinescroll = 3;
	if(U.menuthreshold1==0) {
		U.menuthreshold1= 5;
		U.menuthreshold2= 2;
	}
	if(U.tb_leftmouse==0) {
		U.tb_leftmouse= 5;
		U.tb_rightmouse= 5;
	}
	if(U.mixbufsize==0) U.mixbufsize= 2048;
	if (BLI_streq(U.tempdir, "/")) {
		BLI_where_is_temp(U.tempdir, 0);
	}
	if (U.savetime <= 0) {
		U.savetime = 1;
		error(".B.blend is buggy, please consider removing it.\n");
	}
	/* transform widget settings */
	if(U.tw_hotspot==0) {
		U.tw_hotspot= 14;
		U.tw_size= 20;			// percentage of window size
		U.tw_handlesize= 16;	// percentage of widget radius
	}
	if(U.pad_rot_angle==0)
		U.pad_rot_angle= 15;
	
   if (U.ndof_pan==0) {
        U.ndof_pan = 100;
   }
    if (U.ndof_rotate==0) {
        U.ndof_rotate = 100;
   }

	if(U.flag & USER_CUSTOM_RANGE) 
		vDM_ColorBand_store(&U.coba_weight); /* signal for derivedmesh to use colorband */
	
	/* Auto-keyframing settings */
	if(U.autokey_mode == 0) {
		/* AUTOKEY_MODE_NORMAL - AUTOKEY_ON = x  <==> 3 - 1 = 2 */
		U.autokey_mode |= 2;
		
		if(U.flag & (1<<15)) U.autokey_flag |= AUTOKEY_FLAG_INSERTAVAIL;
		if(U.flag & (1<<19)) U.autokey_flag |= AUTOKEY_FLAG_INSERTNEEDED;
		if(G.flags & (1<<30)) U.autokey_flag |= AUTOKEY_FLAG_AUTOMATKEY;
	}
	
	if (G.main->versionfile <= 191) {
		strcpy(U.plugtexdir, U.textudir);
		strcpy(U.sounddir, "/");
	}
	
	/* patch to set Dupli Armature */
	if (G.main->versionfile < 220) {
		U.dupflag |= USER_DUP_ARM;
	}
	
	/* userdef new option */
	if (G.main->versionfile <= 222) {
		U.vrmlflag= USER_VRML_LAYERS;
	}
	
	/* added seam, normal color, undo */
	if (G.main->versionfile <= 234) {
		bTheme *btheme;
		
		U.uiflag |= USER_GLOBALUNDO;
		if (U.undosteps==0) U.undosteps=32;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->tv3d.edge_seam[3]==0) {
				SETCOL(btheme->tv3d.edge_seam, 230, 150, 50, 255);
			}
			if(btheme->tv3d.normal[3]==0) {
				SETCOL(btheme->tv3d.normal, 0x22, 0xDD, 0xDD, 255);
			}
			if(btheme->tv3d.face_dot[3]==0) {
				SETCOL(btheme->tv3d.face_dot, 255, 138, 48, 255);
				btheme->tv3d.facedot_size= 4;
			}
		}
	}
	if (G.main->versionfile <= 235) {
		/* illegal combo... */
		if (U.flag & USER_LMOUSESELECT) 
			U.flag &= ~USER_TWOBUTTONMOUSE;
	}
	if (G.main->versionfile <= 236) {
		bTheme *btheme;
		/* new space type */
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->ttime.back[3]==0) {
				btheme->ttime = btheme->tsnd;	// copy from sound
			}
			if(btheme->text.syntaxn[3]==0) {
				SETCOL(btheme->text.syntaxn,	0, 0, 200, 255);	/* Numbers  Blue*/
				SETCOL(btheme->text.syntaxl,	100, 0, 0, 255);	/* Strings  red */
				SETCOL(btheme->text.syntaxc,	0, 100, 50, 255);	/* Comments greenish */
				SETCOL(btheme->text.syntaxv,	95, 95, 0, 255);	/* Special */
				SETCOL(btheme->text.syntaxb,	128, 0, 80, 255);	/* Builtin, red-purple */
			}
		}
	}
	if (G.main->versionfile <= 237) {
		bTheme *btheme;
		/* bone colors */
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->tv3d.bone_solid[3]==0) {
				SETCOL(btheme->tv3d.bone_solid, 200, 200, 200, 255);
				SETCOL(btheme->tv3d.bone_pose, 80, 200, 255, 80);
			}
		}
	}
	if (G.main->versionfile <= 238) {
		bTheme *btheme;
		/* bone colors */
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->tnla.strip[3]==0) {
				SETCOL(btheme->tnla.strip_select, 	0xff, 0xff, 0xaa, 255);
				SETCOL(btheme->tnla.strip, 0xe4, 0x9c, 0xc6, 255);
			}
		}
	}
	if (G.main->versionfile <= 239) {
		bTheme *btheme;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* Lamp theme, check for alpha==0 is safe, then color was never set */
			if(btheme->tv3d.lamp[3]==0) {
				SETCOL(btheme->tv3d.lamp, 	0, 0, 0, 40);
/* TEMPORAL, remove me! (ton) */				
				U.uiflag |= USER_PLAINMENUS;
			}
			
			/* check for text field selection highlight, set it to text editor highlight by default */
			if(btheme->tui.textfield_hi[3]==0) {
				SETCOL(btheme->tui.textfield_hi, 	
					btheme->text.shade2[0], 
					btheme->text.shade2[1], 
					btheme->text.shade2[2],
					255);
			}
		}
		if(U.obcenter_dia==0) U.obcenter_dia= 6;
	}
	if (G.main->versionfile <= 241) {
		bTheme *btheme;
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* Node editor theme, check for alpha==0 is safe, then color was never set */
			if(btheme->tnode.syntaxn[3]==0) {
				/* re-uses syntax color storage */
				btheme->tnode= btheme->tv3d;
				SETCOL(btheme->tnode.edge_select, 255, 255, 255, 255);
				SETCOL(btheme->tnode.syntaxl, 150, 150, 150, 255);	/* TH_NODE, backdrop */
				SETCOL(btheme->tnode.syntaxn, 129, 131, 144, 255);	/* in/output */
				SETCOL(btheme->tnode.syntaxb, 127,127,127, 255);	/* operator */
				SETCOL(btheme->tnode.syntaxv, 142, 138, 145, 255);	/* generator */
				SETCOL(btheme->tnode.syntaxc, 120, 145, 120, 255);	/* group */
			}
			/* Group theme colors */
			if(btheme->tv3d.group[3]==0) {
				SETCOL(btheme->tv3d.group, 0x10, 0x40, 0x10, 255);
				SETCOL(btheme->tv3d.group_active, 0x66, 0xFF, 0x66, 255);
			}
			/* Sequence editor theme*/
			if(btheme->tseq.movie[3]==0) {
				SETCOL(btheme->tseq.movie, 	81, 105, 135, 255);
				SETCOL(btheme->tseq.image, 	109, 88, 129, 255);
				SETCOL(btheme->tseq.scene, 	78, 152, 62, 255);
				SETCOL(btheme->tseq.audio, 	46, 143, 143, 255);
				SETCOL(btheme->tseq.effect, 	169, 84, 124, 255);
				SETCOL(btheme->tseq.plugin, 	126, 126, 80, 255);
				SETCOL(btheme->tseq.transition, 162, 95, 111, 255);
				SETCOL(btheme->tseq.meta, 	109, 145, 131, 255);
			}
			if(!(btheme->tui.iconfile)) {
				BLI_strncpy(btheme->tui.iconfile, "", sizeof(btheme->tui.iconfile));
			}
		}
		
		/* set defaults for 3D View rotating axis indicator */ 
		/* since size can't be set to 0, this indicates it's not saved in .B.blend */
		if (U.rvisize == 0) {
			U.rvisize = 15;
			U.rvibright = 8;
			U.uiflag |= USER_SHOW_ROTVIEWICON;
		}
		
	}
	if (G.main->versionfile <= 242) {
		bTheme *btheme;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* long keyframe color */
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->tact.strip[3]==0) {
				SETCOL(btheme->tv3d.edge_sharp, 255, 32, 32, 255);
				SETCOL(btheme->tact.strip_select, 	0xff, 0xff, 0xaa, 204);
				SETCOL(btheme->tact.strip, 0xe4, 0x9c, 0xc6, 204);
			}
			
			/* IPO-Editor - Vertex Size*/
			if(btheme->tipo.vertex_size == 0) {
				btheme->tipo.vertex_size= 3;
			}
		}
	}
	if (G.main->versionfile <= 243) {
		/* set default number of recently-used files (if not set) */
		if (U.recent_files == 0) U.recent_files = 10;
	}
	if (G.main->versionfile < 245 || (G.main->versionfile == 245 && G.main->subversionfile < 3)) {
		bTheme *btheme;
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			SETCOL(btheme->tv3d.editmesh_active, 255, 255, 255, 128);
		}
		if(U.coba_weight.tot==0)
			init_colorband(&U.coba_weight, 1);
	}
	if ((G.main->versionfile < 245) || (G.main->versionfile == 245 && G.main->subversionfile < 11)) {
		bTheme *btheme;
		for (btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* these should all use the same colour */
			SETCOL(btheme->tv3d.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tipo.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tact.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tnla.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tseq.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tsnd.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->ttime.cframe, 0x60, 0xc0, 0x40, 255);
		}
	}
	if ((G.main->versionfile < 245) || (G.main->versionfile == 245 && G.main->subversionfile < 13)) {
		bTheme *btheme;
		for (btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* action channel groups (recolour anyway) */
			SETCOL(btheme->tact.group, 0x39, 0x7d, 0x1b, 255);
			SETCOL(btheme->tact.group_active, 0x7d, 0xe9, 0x60, 255);
			
			/* bone custom-color sets */
			// FIXME: this check for initialised colors is bad
			if (btheme->tarm[0].solid[3] == 0) {
					/* set 1 */
				SETCOL(btheme->tarm[0].solid, 0x9a, 0x00, 0x00, 255);
				SETCOL(btheme->tarm[0].select, 0xbd, 0x11, 0x11, 255);
				SETCOL(btheme->tarm[0].active, 0xf7, 0x0a, 0x0a, 255);
					/* set 2 */
				SETCOL(btheme->tarm[1].solid, 0xf7, 0x40, 0x18, 255);
				SETCOL(btheme->tarm[1].select, 0xf6, 0x69, 0x13, 255);
				SETCOL(btheme->tarm[1].active, 0xfa, 0x99, 0x00, 255);
					/* set 3 */
				SETCOL(btheme->tarm[2].solid, 0x1e, 0x91, 0x09, 255);
				SETCOL(btheme->tarm[2].select, 0x59, 0xb7, 0x0b, 255);
				SETCOL(btheme->tarm[2].active, 0x83, 0xef, 0x1d, 255);
					/* set 4 */
				SETCOL(btheme->tarm[3].solid, 0x0a, 0x36, 0x94, 255);
				SETCOL(btheme->tarm[3].select, 0x36, 0x67, 0xdf, 255);
				SETCOL(btheme->tarm[3].active, 0x5e, 0xc1, 0xef, 255);
					/* set 5 */
				SETCOL(btheme->tarm[4].solid, 0xa9, 0x29, 0x4e, 255);
				SETCOL(btheme->tarm[4].select, 0xc1, 0x41, 0x6a, 255);
				SETCOL(btheme->tarm[4].active, 0xf0, 0x5d, 0x91, 255);
					/* set 6 */
				SETCOL(btheme->tarm[5].solid, 0x43, 0x0c, 0x78, 255);
				SETCOL(btheme->tarm[5].select, 0x54, 0x3a, 0xa3, 255);
				SETCOL(btheme->tarm[5].active, 0x87, 0x64, 0xd5, 255);
					/* set 7 */
				SETCOL(btheme->tarm[6].solid, 0x24, 0x78, 0x5a, 255);
				SETCOL(btheme->tarm[6].select, 0x3c, 0x95, 0x79, 255);
				SETCOL(btheme->tarm[6].active, 0x6f, 0xb6, 0xab, 255);
					/* set 8 */
				SETCOL(btheme->tarm[7].solid, 0x4b, 0x70, 0x7c, 255);
				SETCOL(btheme->tarm[7].select, 0x6a, 0x86, 0x91, 255);
				SETCOL(btheme->tarm[7].active, 0x9b, 0xc2, 0xcd, 255);
					/* set 9 */
				SETCOL(btheme->tarm[8].solid, 0xf4, 0xc9, 0x0c, 255);
				SETCOL(btheme->tarm[8].select, 0xee, 0xc2, 0x36, 255);
				SETCOL(btheme->tarm[8].active, 0xf3, 0xff, 0x00, 255);
					/* set 10 */
				SETCOL(btheme->tarm[9].solid, 0x1e, 0x20, 0x24, 255);
				SETCOL(btheme->tarm[9].select, 0x48, 0x4c, 0x56, 255);
				SETCOL(btheme->tarm[9].active, 0xff, 0xff, 0xff, 255);
					/* set 11 */
				SETCOL(btheme->tarm[10].solid, 0x6f, 0x2f, 0x6a, 255);
				SETCOL(btheme->tarm[10].select, 0x98, 0x45, 0xbe, 255);
				SETCOL(btheme->tarm[10].active, 0xd3, 0x30, 0xd6, 255);
					/* set 12 */
				SETCOL(btheme->tarm[11].solid, 0x6c, 0x8e, 0x22, 255);
				SETCOL(btheme->tarm[11].select, 0x7f, 0xb0, 0x22, 255);
				SETCOL(btheme->tarm[11].active, 0xbb, 0xef, 0x5b, 255);
					/* set 13 */
				SETCOL(btheme->tarm[12].solid, 0x8d, 0x8d, 0x8d, 255);
				SETCOL(btheme->tarm[12].select, 0xb0, 0xb0, 0xb0, 255);
				SETCOL(btheme->tarm[12].active, 0xde, 0xde, 0xde, 255);
					/* set 14 */
				SETCOL(btheme->tarm[13].solid, 0x83, 0x43, 0x26, 255);
				SETCOL(btheme->tarm[13].select, 0x8b, 0x58, 0x11, 255);
				SETCOL(btheme->tarm[13].active, 0xbd, 0x6a, 0x11, 255);
					/* set 15 */
				SETCOL(btheme->tarm[14].solid, 0x08, 0x31, 0x0e, 255);
				SETCOL(btheme->tarm[14].select, 0x1c, 0x43, 0x0b, 255);
				SETCOL(btheme->tarm[14].active, 0x34, 0x62, 0x2b, 255);
			}
		}
	}
	if ((G.main->versionfile < 245) || (G.main->versionfile == 245 && G.main->subversionfile < 16)) {
		U.flag |= USER_ADD_VIEWALIGNED|USER_ADD_EDITMODE;
	}
	
	/* GL Texture Garbage Collection (variable abused above!) */
	if (U.textimeout == 0) {
		U.texcollectrate = 60;
		U.textimeout = 120;
	}
	if (U.memcachelimit <= 0) {
		U.memcachelimit = 32;
	}
	if (U.frameserverport == 0) {
		U.frameserverport = 8080;
	}

	MEM_CacheLimiter_set_maximum(U.memcachelimit * 1024 * 1024);
	
	reset_autosave();
	
#ifdef INTERNATIONAL
	read_languagefile();
#endif
	
	refresh_interface_font();

#ifdef WITH_VERSE
	if(strlen(U.versemaster)<1) {
			strcpy(U.versemaster, "master.uni-verse.org");
	}
	if(strlen(U.verseuser)<1) {
			char *name = verse_client_name();
			strcpy(U.verseuser, name);
			MEM_freeN(name);
	}
#endif

}

#ifdef WITH_VERSE
extern ListBase session_list;
#endif

void BIF_read_file(char *name)
{
	extern short winqueue_break; /* editscreen.c */
	int retval;
#ifdef WITH_VERSE
	struct VerseSession *session;
	struct VNode *vnode;

	session = session_list.first;
	while(session) {
		vnode = session->nodes.lb.first;
		while(vnode) {
			switch(vnode->type) {
				case V_NT_OBJECT:
					unsubscribe_from_obj_node(vnode);
					break;
				case V_NT_GEOMETRY:
					unsubscribe_from_geom_node(vnode);
					break;
				case V_NT_BITMAP:
					unsubscribe_from_bitmap_node(vnode);
					break;
			}
			vnode = vnode->next;
		}
		session = session->next;
	}
#endif

	/* first try to read exotic file formats... */
	/* it throws error box when file doesnt exist and returns -1 */
	retval= BKE_read_exotic(name);
	
	if (retval== 0) {
		BIF_clear_tempfiles();
		
		/* we didn't succeed, now try to read Blender file */
		retval= BKE_read_file(name, NULL);

		mainwindow_set_filename_to_title(G.main->name);
		countall();
		sound_initialize_sounds();

		winqueue_break= 1;	/* leave queues everywhere */

		if(retval==2) init_userdef_file();	// in case a userdef is read from regular .blend
		
		if (retval!=0) G.relbase_valid = 1;

		undo_editmode_clear();
		BKE_reset_undo();
		BKE_write_undo("original");	/* save current state */

		refresh_interface_font();
	}
	else if(retval==1)
		BIF_undo_push("Import file");
}

static void outliner_242_patch(void)
{
	ScrArea *sa;
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceLink *sl= sa->spacedata.first;
		for(; sl; sl= sl->next) {
			if(sl->spacetype==SPACE_OOPS) {
				SpaceOops *soops= (SpaceOops *)sl;
				if(soops->type!=SO_OUTLINER) {
					soops->type= SO_OUTLINER;
					init_v2d_oops(sa, soops);
				}
			}
		}
	}
	G.fileflags |= G_FILE_GAME_MAT;
}

/* only here settings for fullscreen */
int BIF_read_homefile(int from_memory)
{
	char tstr[FILE_MAXDIR+FILE_MAXFILE], scestr[FILE_MAX];
	char *home= BLI_gethome();
	int success;
	struct TmpFont *tf;
	
	BIF_clear_tempfiles();
	
	BLI_clean(home);

	tf= G.ttfdata.first;
	while(tf)
	{
		freePackedFile(tf->pf);
		tf->pf = NULL;
		tf->vfont = NULL;
		tf= tf->next;
	}
	BLI_freelistN(&G.ttfdata);
		
	G.relbase_valid = 0;
	if (!from_memory) BLI_make_file_string(G.sce, tstr, home, ".B.blend");
	BLI_strncpy(scestr, G.sce, FILE_MAX);	/* temporal store */
	
	/* prevent loading no UI */
	G.fileflags &= ~G_FILE_NO_UI;
	
	if (!from_memory && BLI_exists(tstr)) {
		success = BKE_read_file(tstr, NULL);
	} else {
		success = BKE_read_file_from_memory(datatoc_B_blend, datatoc_B_blend_size, NULL);
		/* outliner patch for 2.42 .b.blend */
		outliner_242_patch();
	}

	BLI_clean(scestr);
	strcpy(G.sce, scestr);

	space_set_commmandline_options();
	
	init_userdef_file();

	undo_editmode_clear();
	BKE_reset_undo();
	BKE_write_undo("original");	/* save current state */
	
	/* if from memory, need to refresh python scripts */
	if (from_memory) {
		BPY_path_update();
	}
	return success;
}


static void get_autosave_location(char buf[FILE_MAXDIR+FILE_MAXFILE])
{
	char pidstr[32];
#ifdef WIN32
	char subdir[9];
	char savedir[FILE_MAXDIR];
#endif

	sprintf(pidstr, "%d.blend", abs(getpid()));
	
#ifdef WIN32
	if (!BLI_exists(btempdir)) {
		BLI_strncpy(subdir, "autosave", sizeof(subdir));
		BLI_make_file_string("/", savedir, BLI_gethome(), subdir);
		
		/* create a new autosave dir
		 * function already checks for existence or not */
		BLI_recurdir_fileops(savedir);
	
		BLI_make_file_string("/", buf, savedir, pidstr);
		return;
	}
#endif
	
	BLI_make_file_string("/", buf, btempdir, pidstr);
}

void BIF_read_autosavefile(void)
{
	char tstr[FILE_MAX], scestr[FILE_MAX];
	int save_over;

	BLI_strncpy(scestr, G.sce, FILE_MAX);	/* temporal store */
	
	get_autosave_location(tstr);

	save_over = G.save_over;
	BKE_read_file(tstr, NULL);
	G.save_over = save_over;
	BLI_strncpy(G.sce, scestr, FILE_MAX);
}

/* free strings of open recent files */
static void free_openrecent(void)
{
	struct RecentFile *recent;

	for(recent = G.recent_files.first; recent; recent=recent->next)
		MEM_freeN(recent->filename);

	BLI_freelistN(&(G.recent_files));
}

static void readBlog(void)
{
	char name[FILE_MAX], filename[FILE_MAX];
	LinkNode *l, *lines;
	struct RecentFile *recent;
	char *line;
	int num;

	BLI_make_file_string("/", name, BLI_gethome(), ".Blog");
	lines= BLI_read_file_as_lines(name);

	G.recent_files.first = G.recent_files.last = NULL;

	/* read list of recent opend files from .Blog to memory */
	for (l= lines, num= 0; l && (num<U.recent_files); l= l->next, num++) {
		line = l->link;
		if (!BLI_streq(line, "")) {
			if (num==0) 
				strcpy(G.sce, line);
			
			recent = (RecentFile*)MEM_mallocN(sizeof(RecentFile),"RecentFile");
			BLI_addtail(&(G.recent_files), recent);
			recent->filename = (char*)MEM_mallocN(sizeof(char)*(strlen(line)+1), "name of file");
			recent->filename[0] = '\0';
			
			strcpy(recent->filename, line);
		}
	}

	if(G.sce[0] == 0)
		BLI_make_file_string("/", G.sce, BLI_gethome(), "untitled.blend");
	
	BLI_free_file_lines(lines);

#ifdef WIN32
	/* Add the drive names to the listing */
	{
		__int64 tmp;
		char folder[MAX_PATH];
		char tmps[4];
		int i;
			
		tmp= GetLogicalDrives();
		
		for (i=2; i < 26; i++) {
			if ((tmp>>i) & 1) {
				tmps[0]='a'+i;
				tmps[1]=':';
				tmps[2]='\\';
				tmps[3]=0;
				
				fsmenu_insert_entry(tmps, 0, 0);
			}
		}

		/* Adding Desktop and My Documents */
		fsmenu_append_separator();

		SHGetSpecialFolderPath(0, folder, CSIDL_PERSONAL, 0);
		fsmenu_insert_entry(folder, 0, 0);
		SHGetSpecialFolderPath(0, folder, CSIDL_DESKTOPDIRECTORY, 0);
		fsmenu_insert_entry(folder, 0, 0);

		fsmenu_append_separator();
	}
#else
	/* add home dir on linux systems */
	fsmenu_insert_entry(BLI_gethome(), 0, 0);
#endif

	BLI_make_file_string(G.sce, name, BLI_gethome(), ".Bfs");
	lines= BLI_read_file_as_lines(name);

	for (l= lines; l; l= l->next) {
		char *line= l->link;
			
		if (!BLI_streq(line, "")) {
			fsmenu_insert_entry(line, 0, 1);
		}
	}

	fsmenu_append_separator();
	
	/* add last saved file */
	BLI_split_dirfile(G.sce, name, filename); /* G.sce shouldn't be relative */
	
	fsmenu_insert_entry(name, 0, 0);
	
	BLI_free_file_lines(lines);
}

static void writeBlog(void)
{
	struct RecentFile *recent, *next_recent;
	char name[FILE_MAXDIR+FILE_MAXFILE];
	FILE *fp;
	int i;

	BLI_make_file_string("/", name, BLI_gethome(), ".Blog");

	recent = G.recent_files.first;
	/* refresh .Blog of recent opened files, when current file was changed */
	if(!(recent) || (strcmp(recent->filename, G.sce)!=0)) {
		fp= fopen(name, "w");
		if (fp) {
			/* add current file to the beginning of list */
			recent = (RecentFile*)MEM_mallocN(sizeof(RecentFile),"RecentFile");
			recent->filename = (char*)MEM_mallocN(sizeof(char)*(strlen(G.sce)+1), "name of file");
			recent->filename[0] = '\0';
			strcpy(recent->filename, G.sce);
			BLI_addhead(&(G.recent_files), recent);
			/* write current file to .Blog */
			fprintf(fp, "%s\n", recent->filename);
			recent = recent->next;
			i=1;
			/* write rest of recent opened files to .Blog */
			while((i<U.recent_files) && (recent)){
				/* this prevents to have duplicities in list */
				if (strcmp(recent->filename, G.sce)!=0) {
					fprintf(fp, "%s\n", recent->filename);
					recent = recent->next;
				}
				else {
					next_recent = recent->next;
					MEM_freeN(recent->filename);
					BLI_freelinkN(&(G.recent_files), recent);
					recent = next_recent;
				}
				i++;
			}
			fclose(fp);
		}
	}
}

static void do_history(char *name)
{
	char tempname1[FILE_MAXDIR+FILE_MAXFILE], tempname2[FILE_MAXDIR+FILE_MAXFILE];
	int hisnr= U.versions;
	
	if(U.versions==0) return;
	if(strlen(name)<2) return;
		
	while(  hisnr > 1) {
		sprintf(tempname1, "%s%d", name, hisnr-1);
		sprintf(tempname2, "%s%d", name, hisnr);
	
		if(BLI_rename(tempname1, tempname2))
			error("Unable to make version backup");
			
		hisnr--;
	}
		
	/* is needed when hisnr==1 */
	sprintf(tempname1, "%s%d", name, hisnr);
	
	if(BLI_rename(name, tempname1))
		error("Unable to make version backup");
}

void BIF_write_file(char *target)
{
	Library *li;
	int writeflags, len;
	char di[FILE_MAX];
	char *err;
	
	len = strlen(target);
	
	if (len == 0) return;
	if (len >= FILE_MAX) {
		error("Path too long, cannot save");
		return;
	}
 
	/* send the OnSave event */
	if (G.f & G_DOSCRIPTLINKS) BPY_do_pyscript(&G.scene->id, SCRIPT_ONSAVE);

	for (li= G.main->library.first; li; li= li->id.next) {
		if (BLI_streq(li->name, target)) {
			error("Cannot overwrite used library");
			return;
		}
	}
	
	if (!BLO_has_bfile_extension(target) && (len+6 < FILE_MAX)) {
		sprintf(di, "%s.blend", target);
	} else {
		strcpy(di, target);
	}

	if (BLI_exists(di)) {
		if(!saveover(di))
			return; 
	}
	
	if(G.obedit) {
		exit_editmode(0);	/* 0 = no free data */
	}
	if (G.fileflags & G_AUTOPACK) {
		packAll();
	}
	
	waitcursor(1);	// exit_editmode sets cursor too

	do_history(di);
	
	/* we use the UserDef to define compression flag */
	writeflags= G.fileflags & ~G_FILE_COMPRESS;
	if(U.flag & USER_FILECOMPRESS)
		writeflags |= G_FILE_COMPRESS;
	
	if (BLO_write_file(di, writeflags, &err)) {
		strcpy(G.sce, di);
		G.relbase_valid = 1;
		BLI_strncpy(G.main->name, di, FILE_MAX);	/* is guaranteed current file */

		mainwindow_set_filename_to_title(G.main->name);

		G.save_over = 1;

		writeBlog();
	} else {
		error("%s", err);
	}

	waitcursor(0);
}

void BIF_write_homefile(void)
{
	char *err, tstr[FILE_MAXDIR+FILE_MAXFILE];
	int write_flags;
	
	BLI_make_file_string("/", tstr, BLI_gethome(), ".B.blend");
		
	/*  force save as regular blend file */
	write_flags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_LOCK | G_FILE_SIGN);
	BLO_write_file(tstr, write_flags, &err);
}

void BIF_write_autosave(void)
{
	char *err, tstr[FILE_MAXDIR+FILE_MAXFILE];
	int write_flags;
	
	get_autosave_location(tstr);

		/*  force save as regular blend file */
	write_flags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_LOCK | G_FILE_SIGN);
	BLO_write_file(tstr, write_flags, &err);
}

/* remove temp files assosiated with this blend file when quitting, loading or saving in a new path */
void BIF_clear_tempfiles( void )
{
	/* TODO - remove exr files from the temp dir */
	
	if (!G.relbase_valid) { /* We could have pointcache saved in tyhe temp dir, if its there */
		BKE_ptcache_remove();
	}
}

/* if global undo; remove tempsave, otherwise rename */
static void delete_autosave(void)
{
	char tstr[FILE_MAXDIR+FILE_MAXFILE];
	
	get_autosave_location(tstr);

	if (BLI_exists(tstr)) {
		char str[FILE_MAXDIR+FILE_MAXFILE];
		BLI_make_file_string("/", str, btempdir, "quit.blend");

		if(U.uiflag & USER_GLOBALUNDO) BLI_delete(tstr, 0, 0);
		else BLI_rename(tstr, str);
	}
}

/***/

static void initbuttons(void)
{
	uiDefFont(UI_HELVB, 
				BMF_GetFont(BMF_kHelveticaBold14), 
				BMF_GetFont(BMF_kHelveticaBold12), 
				BMF_GetFont(BMF_kHelveticaBold10), 
				BMF_GetFont(BMF_kHelveticaBold8));
	uiDefFont(UI_HELV, 
				BMF_GetFont(BMF_kHelvetica12), 
				BMF_GetFont(BMF_kHelvetica12), 
				BMF_GetFont(BMF_kHelvetica10), 
				BMF_GetFont(BMF_kHelveticaBold8));

	glClearColor(.7f, .7f, .6f, 0.0);
	
	G.font= BMF_GetFont(BMF_kHelvetica12);
	G.fonts= BMF_GetFont(BMF_kHelvetica10);
	G.fontss= BMF_GetFont(BMF_kHelveticaBold8);

	clear_matcopybuf();
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}


static void sound_init_listener(void)
{
	G.listener = MEM_callocN(sizeof(bSoundListener), "soundlistener");
	G.listener->gain = 1.0;
	G.listener->dopplerfactor = 1.0;
	G.listener->dopplervelocity = 340.29f;
}

void BIF_init(void)
{

	initscreen();	/* for (visuele) speed, this first, then setscreen */
	initbuttons();
	InitCursorData();
	sound_init_listener();
	init_node_butfuncs();
	
	BIF_preview_init_dbase();
	BIF_read_homefile(0);

	BIF_resources_init();	/* after homefile, to dynamically load an icon file based on theme settings */
	
	BIF_filelist_init_icons();

	init_gl_stuff();	/* drawview.c, after homefile */
	readBlog();
	BLI_strncpy(G.lib, G.sce, FILE_MAX);
}

/***/

extern ListBase editNurb;
extern ListBase editelems;

void exit_usiblender(void)
{
	struct TmpFont *tf;
	
	BIF_clear_tempfiles();
	
	tf= G.ttfdata.first;
	while(tf)
	{
		freePackedFile(tf->pf);
		tf->pf= NULL;
		tf->vfont= NULL;
		tf= tf->next;
	}
	BLI_freelistN(&G.ttfdata);
#ifdef WITH_VERSE
	end_all_verse_sessions();
#endif
	free_openrecent();

	freeAllRad();
	BKE_freecubetable();

	if (G.background == 0)
		sound_end_all_sounds();

	if(G.obedit) {
		if(G.obedit->type==OB_FONT) {
			free_editText();
		}
		else if(G.obedit->type==OB_MBALL) BLI_freelistN(&editelems);
		free_editMesh(G.editMesh);
	}

	free_editLatt();
	free_editArmature();
	free_posebuf();

	/* before free_blender so py's gc happens while library still exists */
	/* needed at least for a rare sigsegv that can happen in pydrivers */
	BPY_end_python();

	fastshade_free_render();	/* shaded view */
	free_blender();				/* blender.c, does entire library */
	free_matcopybuf();
	free_ipocopybuf();
	free_actcopybuf();
	free_vertexpaint();
	free_imagepaint();
	
	/* editnurb can remain to exist outside editmode */
	freeNurblist(&editNurb);

	fsmenu_free();

#ifdef INTERNATIONAL
	free_languagemenu();
#endif
	
	RE_FreeAllRender();
	
	free_txt_data();

	sound_exit_audio();
	if(G.listener) MEM_freeN(G.listener);


	libtiff_exit();

#ifdef WITH_QUICKTIME
	quicktime_exit();
#endif

	/* undo free stuff */
	undo_editmode_clear();
	
	BKE_undo_save_quit();	// saves quit.blend if global undo is on
	BKE_reset_undo(); 
	
	if (!G.background) {
		BIF_resources_free();
		
		BIF_filelist_free_icons();

		BIF_free_render_spare();
		BIF_close_render_display();
		mainwindow_close();
	}

#ifdef INTERNATIONAL
	FTF_End();
#endif

	if (copybuf) MEM_freeN(copybuf);
	if (copybufinfo) MEM_freeN(copybufinfo);

// 	
	BLI_freelistN(&U.themes);
	BIF_preview_free_dbase();
	
	if(totblock!=0) {
		printf("Error Totblock: %d\n",totblock);
		MEM_printmemlist();
	}
	delete_autosave();
	
	printf("\nBlender quit\n");

#ifdef WIN32   
	/* ask user to press enter when in debug mode */
	if(G.f & G_DEBUG) {
		printf("press enter key to exit...\n\n");
		getchar();
	}
#endif 


	SYS_DeleteSystem(SYS_GetSystem());
	
	exit(G.afbreek==1);
}
