/**
 * $Id: 
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

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BSE_filesel.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BIF_editmesh.h"
#include "BDR_editcurve.h"
#include "BDR_editface.h"
#include "BDR_drawobject.h"
#include "BIF_butspace.h"

#include "interface.h"
#include "mydevice.h"
#include "blendef.h"

/* -----includes for this file specific----- */


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
#include "DNA_radio_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
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
#include "BKE_ika.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BSE_editipo.h"
#include "BDR_editobject.h"

#include "butspace.h" // own module

float hspeed=0.1, prspeed=0.0, prlen=0.0;



/* ************************************* */

#include "BLI_editVert.h"
extern ListBase editNurb;

void do_common_editbuts(unsigned short event) // old name
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

void object_panel_draw(Object *ob)
{
	ID *id;
	uiBlock *block;
	int xco, a, dx, dy;
	
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_draw", UI_EMBOSSX, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Draw", "Object", 320, 0, 318, 204)==0) return;

	/* LAYERS */
	xco= 151;
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
																28,180,100,18, &ob->dt, 0, 0, 0, 0, "Sets the drawing type of the active object");
	uiDefBut(block, LABEL, 0, "Draw Extra",						28,160,100,18, 0, 0, 0, 0, 0, "");
	uiDefButC(block, TOG|BIT|0, REDRAWVIEW3D, "Bounds",		28, 140, 100, 18, &ob->dtx, 0, 0, 0, 0, "Displays the active object's bounds");
	uiDefButS(block, MENU, REDRAWVIEW3D, "Boundary Display%t|Box%x0|Sphere%x1|Cylinder%x2|Cone%x3|Polyheder",
																28, 120, 100, 18, &ob->boundtype, 0, 0, 0, 0, "Selects the boundary display type");
	uiDefButC(block, TOG|BIT|5, REDRAWVIEW3D, "Wire",		28, 100, 100, 18, &ob->dtx, 0, 0, 0, 0, "Displays the active object's wireframe in shaded drawing modes");
	uiDefButC(block, TOG|BIT|1, REDRAWVIEW3D, "Axis",		28, 80, 100, 18, &ob->dtx, 0, 0, 0, 0, "Displays the active object's centre and axis");
	uiDefButC(block, TOG|BIT|2, REDRAWVIEW3D, "TexSpace",	28, 60, 100, 18, &ob->dtx, 0, 0, 0, 0, "Displays the active object's texture space");
	uiDefButC(block, TOG|BIT|3, REDRAWVIEW3D, "Name",		28, 40, 100, 18, &ob->dtx, 0, 0, 0, 0, "Displays the active object's name");
	
	uiBlockSetCol(block, BUTGREY);
	
	uiScalePanelBlock(block); // scales and centers	
	uiDrawBlock(block);
	
}




void do_object_panels(unsigned short event)
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
		allqueue(REDRAWBUTSOBJECT, 0);
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
		allqueue(REDRAWBUTSOBJECT, 0);
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
		allqueue(REDRAWBUTSOBJECT, 0);
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
		allqueue(REDRAWBUTSOBJECT, 0);
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
		allqueue(REDRAWBUTSOBJECT, 0);
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
		allqueue(REDRAWBUTSOBJECT, 0);
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
		allqueue(REDRAWBUTSOBJECT, 0);
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
		allqueue(REDRAWBUTSOBJECT, 0);
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
				allqueue(REDRAWBUTSOBJECT, 0);
			}
		}
	}

}

void object_panel_effects(Object *ob)
{
	Effect *eff;
	uiBlock *block;
	int a;
	short x, y;
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_effects", UI_EMBOSSX, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Effects", "Object", 640, 0, 418, 204)==0) return;

	uiBlockSetCol(block, BUTSALMON);
	
	/* EFFECTS */
	
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
	
	uiScalePanelBlock(block); // scales and centers
	uiDrawBlock(block);
}

static void object_panel_anim(Object *ob)
{
	uiBlock *block;
	char str[32];
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_anim", UI_EMBOSSX, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Anim settings", "Object", 0, 0, 318, 204)==0) return;
	
	uiBlockSetCol(block, BUTGREEN);
	uiDefButC(block, ROW,REDRAWVIEW3D,"TrackX",	27,190,58,17, &ob->trackflag, 12.0, 0.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,REDRAWVIEW3D,"Y",		85,190,19,17, &ob->trackflag, 12.0, 1.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,REDRAWVIEW3D,"Z",		104,190,19,17, &ob->trackflag, 12.0, 2.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,REDRAWVIEW3D,"-X",		124,190,24,17, &ob->trackflag, 12.0, 3.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,REDRAWVIEW3D,"-Y",		150,190,24,17, &ob->trackflag, 12.0, 4.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,REDRAWVIEW3D,"-Z",		177,190,24,17, &ob->trackflag, 12.0, 5.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,REDRAWVIEW3D,"UpX",	226,190,45,17, &ob->upflag, 13.0, 0.0, 0, 0, "Specify the axis that points up");
	uiDefButC(block, ROW,REDRAWVIEW3D,"Y",		274,190,20,17, &ob->upflag, 13.0, 1.0, 0, 0, "Specify the axis that points up");
	uiDefButC(block, ROW,REDRAWVIEW3D,"Z",		297,190,19,17, &ob->upflag, 13.0, 2.0, 0, 0, "Specify the axis that points up");

	uiBlockSetCol(block, BUTGREEN);
	uiDefButC(block, TOG|BIT|0, REDRAWVIEW3D, "Draw Key",		25,160,70,19, &ob->ipoflag, 0, 0, 0, 0, "Draw object as key position");
	uiDefButC(block, TOG|BIT|1, REDRAWVIEW3D, "Draw Key Sel",	97,160,81,20, &ob->ipoflag, 0, 0, 0, 0, "Limit the drawing of object keys");
	uiDefButS(block, TOG|BIT|4, 0, "SlowPar",			261,160,56,20, &ob->partype, 0, 0, 0, 0, "Create a delay in the parent relationship");
	uiDefButC(block, TOG|BIT|7, REDRAWVIEW3D, "Powertrack",	180,160,78,19, &ob->transflag, 0, 0, 0, 0, "Switch objects rotation off");


	uiBlockSetCol(block, BUTGREY);
	uiDefButC(block, TOG|BIT|3, REDRAWVIEW3D, "DupliFrames",	24,128,88,19, &ob->transflag, 0, 0, 0, 0, "Make copy of object for every frame");
	uiDefButC(block, TOG|BIT|4, REDRAWVIEW3D, "DupliVerts",		114,128,82,19, &ob->transflag, 0, 0, 0, 0, "Duplicate child objects on all vertices");
	uiBlockSetCol(block, BUTGREEN);
	uiDefButC(block, TOG|BIT|5, REDRAWVIEW3D, "Rot",		200,128,31,20, &ob->transflag, 0, 0, 0, 0, "Rotate dupli according to facenormal");
	uiDefButC(block, TOG|BIT|6, REDRAWVIEW3D, "No Speed",	234,128,82,19, &ob->transflag, 0, 0, 0, 0, "Set dupliframes to still, regardless of frame");

	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, NUM, REDRAWVIEW3D, "DupSta:",		24,105,141,18, &ob->dupsta, 1.0, 1500.0, 0, 0, "Specify startframe for Dupliframes");
	uiDefButS(block, NUM, REDRAWVIEW3D, "DupEnd",		24,83,140,19, &ob->dupend, 1.0, 2500.0, 0, 0, "Specify endframe for Dupliframes");
	uiDefButS(block, NUM, REDRAWVIEW3D, "DupOn:",		169,104,146,19, &ob->dupon, 1.0, 1500.0, 0, 0, "");
	uiDefButS(block, NUM, REDRAWVIEW3D, "DupOff",		169,82,145,19, &ob->dupoff, 0.0, 1500.0, 0, 0, "");

	uiBlockSetCol(block, BUTGREEN);
	uiDefButC(block, TOG|BIT|2, REDRAWALL, "Offs Ob",			23,51,56,20, &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on its own objectipo");
	uiDefButC(block, TOG|BIT|6, REDRAWALL, "Offs Par",			82,51,56,20 , &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on the parent");
	uiDefButC(block, TOG|BIT|7, REDRAWALL, "Offs Particle",		141,51,103,20, &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on the particle effect");

	uiBlockSetCol(block, BUTGREY);
	sprintf(str, "%.4f", prspeed);
	uiDefBut(block, LABEL, 0, str,							247,40,63,31, 0, 1.0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_PRINTSPEED,	"PrSpeed",			246,17,67,31, 0, 0, 0, 0, 0, "Print objectspeed");

	uiBlockSetCol(block, BUTGREY);
	uiDefButF(block, NUM, REDRAWALL, "TimeOffset:",			23,17,114,30, &ob->sf, -9000.0, 9000.0, 100, 0, "Specify an offset in frames");
	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT, B_AUTOTIMEOFS, "Automatic Time",	139,17,104,31, 0, 0, 0, 0, 0, "Generate automatic timeoffset values for all selected frames");
		
#if 0	
	/* IPO BUTTONS AS LAST */
	ScrArea *sa;

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
	
	uiBlockSetCol(block, BUTGREY);
	
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

#endif	
	

	uiScalePanelBlock(block); // scales and centers
	uiDrawBlock(block);
}

void object_panels()
{
	Object *ob;

	/* check context here */
	ob= OBACT;
	if(ob) {
		object_panel_anim(ob);
		object_panel_draw(ob);
		if(ob->type==OB_MESH) 
			object_panel_effects(ob);
	}
}

