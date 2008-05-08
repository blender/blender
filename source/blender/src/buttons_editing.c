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

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifdef WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

#include "MEM_guardedalloc.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_color_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_radio_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_packedFile_types.h"

#include "BKE_blender.h"
#include "BKE_brush.h"
#include "BKE_cloth.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_colortools.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_packedFile.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_bmesh.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_vfontdata.h"
#include "BLI_editVert.h"
#include "BLI_dynstr.h"

#include "BSE_filesel.h"

#include "BIF_gl.h"
#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editconstraint.h"
#include "BIF_editdeform.h"
#include "BIF_editfont.h"
#include "BIF_editkey.h"
#include "BIF_editmesh.h"
#include "BIF_editparticle.h"
#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_poselib.h"
#include "BIF_poseobject.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_retopo.h"
#include "BIF_screen.h"
#include "BIF_scrarea.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_previewrender.h"
#include "BIF_butspace.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#include "mydevice.h"
#include "blendef.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BIF_poseobject.h"

#include "BDR_drawobject.h"
#include "BDR_editcurve.h"
#include "BDR_editface.h"
#include "BDR_editobject.h"
#include "BDR_sculptmode.h"
#include "BDR_vpaint.h"
#include "BDR_unwrapper.h"

#include "BSE_drawview.h"
#include "BSE_editipo.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"
#include "BSE_seqaudio.h"

#include "RE_render_ext.h"		// make_sticky

#include "butspace.h" // own module
#include "multires.h"

static float editbutweight= 1.0;
float editbutvweight= 1;
static int actmcol= 0, acttface= 0, acttface_rnd = 0, actmcol_rnd = 0;

extern ListBase editNurb;

/* *************************** Unicode Character Groups ****************** */
unicodect uctabname[125] = {
	{"All", "All", 0x0000, 0xffff},
	{"Basic Latin", "Basic Latin", 0x0000, 0x007f},
	{"Latin 1 Supp", "Latin-1 Supplement", 0x0080, 0x00ff}, 

	{"Latin Ext. A.", "Latin Extended-A", 0x0100, 0x017F},
	{"Latin Ext. B.", "Latin Extended-B", 0x0180,0x024F}, 
	{"Latin Ext. Add.", "Latin Extended Additional", 0x1e00, 0x1eff},

	{"IPA Ext", "IPA Extensions", 0x0250, 0x02AF},
	{"Spacing Mod.", "Spacing Modifier Letters", 0x02b0, 0x02ff},

	{"Comb. Dia.", "Combining Diacritical Marks", 0x0300, 0x036F},
	{"Greek, Coptic", "Greek and Coptic", 0x0370, 0x03ff},
	{"Greek Ext.", "Greek Extended", 0x1f00, 0x1fff},

	{"Cyrillic", "Cyrillic", 0x0400, 0x04ff},
	{"Cyrillic Supp.", "Cyrillic Supplementary", 0x0500, 0x052f},

	{"Armenian", "Armenian", 0x0530, 0x058f},
	{"Hebrew", "Hebrew", 0x0590, 0x05ff},

	
	{"Arabic", "Arabic", 0x0600, 0x06ff},
	{"Syriac", "Syriac", 0x0700, 0x074f},

	{"Thaana", "Thaana", 0x0780, 0x07bf},
	{"Devanagari", "Devanagari", 0x0900, 0x097f},

	{"Bengali", "Bengali", 0x0980, 0x09ff},
	{"Gurmukhi", "Gurmukhi", 0x0a00, 0x0a7f},

	{"Gujarati", "Gujarati", 0x0a80, 0x0aff},
	{"Oriya", "Oriya", 0x0b00, 0x0b7f},

	{"Tamil", "Tamil", 0x0b80, 0x0bff},
	{"Tegulu", "Tegulu", 0x0c00, 0x0c7f},

	{"Kannada", "Kannada", 0x0c80, 0x0cff},
	{"Malayalam", "Malayalam", 0x0d00, 0x0d7f},

	{"Sinhala", "Sinhala", 0x0d80, 0x0dff},
	{"Thai", "Thai", 0x0e00, 0x0e7f},

	{"Lao", "Lao", 0x0e80, 0x0eff},
	{"Tibetan", "Tibetan", 0x0f00, 0x0fff},

	{"Myanmar", "Myanmar", 0x1000, 0x109f},
	{"Georgian", "Georgian", 0x10a0, 0x10ff},

	{"Ethiopic", "Ethiopic", 0x1200, 0x137f},

	{"Cherokee", "Cherokee", 0x13a0, 0x13ff},
	{"Unif. Canadian", "Unified Canadian Aboriginal Syllabics", 0x1400, 0x167f},

	{"Ogham", "Ogham", 0x1680, 0x169f},
	{"Runic", "Runic", 0x16a0, 0x16ff},

	{"Tagalog", "Tagalog", 0x1700, 0x171f},
	{"Hanunoo", "Hanunoo", 0x1720, 0x173f},

	{"Buhid", "Buhid", 0x1740, 0x175f},
	{"Tagbanwa", "Tagbanwa", 0x1760, 0x177f},

	{"Khmer", "Khmer", 0x1780, 0x17ff},
	{"Khmer Symb", "Khmer Symbols", 0x19e0, 0x19ff},

	{"Mongolian", "Mongolian", 0x1800, 0x18af},

	{"Limbu", "Limbu", 0x1900, 0x194f},
	{"Tai Le", "Tai Le", 0x1950, 0x197f},

	{"Phon. Ext.", "Phonetic Extensions", 0x1d00, 0x1d7f},


	{"Gen. Punct.", "General Punctutation", 0x2000, 0x206f},
	{"Super, Sub", "Superscripts and Subscripts", 0x2070, 0x209f},

	{"Curr. Symb.", "Currency Symbols", 0x20a0, 0x20cf},
	{"Comb. Diacrit.", "Combining Diacritical Marks for Symbols", 0x20d0, 0x20ff},

	{"Letter Symb", "Letterlike Symbols", 0x2100, 0x214f},
	{"Numb. Forms", "Number Forms", 0x2150, 0x218f},

	{"Arrows", "Arrows", 0x2190, 0x21ff},
	{"Math Oper.", "Mathematical Operators", 0x2200, 0x22ff},

	{"Misc. Tech.", "Miscellaneous Technical", 0x2300, 0x23ff},
	{"Ctrl. Pict.", "Control Pictures", 0x2400, 0x243f},

	{"OCR", "Optical Character Recognition", 0x2440, 0x245f},
	{"Enc. Alpha", "Enclosed Alphanumerics", 0x2460, 0x24ff},

	{"Bow Drawing", "Box Drawing", 0x2500, 0x257f},
	{"BLock Elem.", "Block Elements", 0x2580, 0x259f},

	{"Geom. Shapes", "Geometric Shapes", 0x25a0, 0x25ff},
	{"Misc. Symb.", "Miscellaneous Symbols", 0x2600, 0x26ff},

	{"Dingbats", "Dingbats", 0x2700, 0x27bf},
	{"Misc. Math A", "Miscellaneous Mathematical Symbols-A", 0x27c0, 0x27ef},

	{"Supp. Arrows-A", "Supplemental Arrows-A", 0x27f0, 0x27ff},
	{"Braille Pat.", "Braille Patterns", 0x2800, 0x28ff},

	{"Supp. Arrows-B", "Supplemental Arrows-B", 0x2900, 0x297f},
	{"Misc. Math B", "Miscellaneous Mathematical Symbols-B", 0x2980, 0x29ff},

	{"Supp. Math Op.", "Supplemental Mathematical Operators", 0x2a00, 0x2aff},
	{"Misc. Symb.", "Miscellaneous Symbols and Arrows", 0x2b00, 0x2bff},

	{"Kangxi Rad.", "Kangxi Radicals", 0x2f00, 0x2fdf},

	{"Ideographic", "Ideographic Description Characters", 0x2ff0, 0x2fff},

	{"Hiragana", "Hiragana", 0x3040, 0x309f},
	{"Katakana", "Katakana", 0x30a0, 0x30ff},
	{"Katakana Ext.", "Katakana Phonetic Extensions", 0x31f0, 0x31ff},

	{"Bopomofo", "Bopomofo", 0x3100, 0x312f},
	{"Bopomofo Ext.", "Bopomofo Extended", 0x31a0, 0x31bf},

	{"Hangul", "Hangul Jamo", 0x1100, 0x11ff},
	{"Hangul Comp.", "Hangul Compatibility Jamo", 0x3130, 0x318f},
	{"Hangul Syll.", "Hangul Syllables", 0xac00, 0xd7af},

	{"Kanbun", "Kanbun", 0x3190, 0x319f},



	{"Yijing Hex.", "Yijing Hexagram Symbols", 0x4dc0, 0x4dff},

	{"Yi Syllables", "Yi Syllables", 0xa000, 0xa48f},
	{"Yi Radicals", "Yi Radicals", 0xa490, 0xa4cf},

	{"High Surr.", "High Surrogate Area", 0xd800, 0xdbff},

	{"Low Surr.", "Low Surrogates", 0xdc00, 0xdfff},
	{"Priv. Use Area", "Private Use Area", 0xe000, 0xf8ff},

	{"CJK Rad. Supp.", "CJK Radicals Supplement", 0x2e80, 0x2eff},
	{"CJK Ideographs", "CJK Unified Ideographs", 0x4e00, 0x9faf},
	{"CJK Ideog. Ext. A", "CJK Unified Ideographs Extension A", 0x3400, 0x4dbf},
	{"CJK Ideog. Ext. B", "CJK Unified Ideographs Extension B", 0x20000, 0x2a6df},
	{"CJK Symbols.", "CJK Symbols and Punctuation", 0x3000, 0x303f},
	{"Enclosed CJK", "Enclosed CJK Letters and Months", 0x3200, 0x32ff},
	{"CJK Comp.", "CJK Compatibility", 0x3300, 0x33ff},
	{"CJK Comp. Ideog.", "CJK Compatibility Ideographs", 0xf900, 0xfaff},
	{"CJK Comp. Forms", "CJK Compatibility Forms", 0xfe30, 0xfe4f},
	{"CJK Comp. Supp.", "CJK Compatibility Ideographs Supplement", 0x2f800, 0x2fa1f},

	{"Alpha. Pres. Forms", "Alphabetic Presentation Forms", 0xfb00, 0xfb4f},

	{"Arabic Pres. A", "Arabic Presentation Forms-A", 0xfb50, 0xfdff},
	{"Arabic Pres. B", "Arabic Presentation Forms-B", 0xfe70, 0xfeff},

	{"Var. Sel.", "Variation Selectors", 0xfe00, 0xfe0f},

	{"Comb. Half", "Combining Half Marks", 0xfe20, 0xfe2f},

	{"Sml. From Var.", "Small Form Variants", 0xfe50, 0xfe6f},

	{"Half, Full Forms", "Halfwidth and Fullwidth Forms", 0xff00, 0xffef},
	{"Specials", "Specials", 0xfff0, 0xffff},

	{"Lin. B Syllab.", "Linear B Syllabary", 0x10000, 0x1007f},
	{"Lin. B Idog.", "Linear B Ideograms", 0x10080, 0x100ff},

	{"Aegean Num.", "Aegean Numbers", 0x10100, 0x1013f},
	{"Old Italic", "Old Italic", 0x10300, 0x1032f},

	{"Gothic", "Gothic", 0x10330, 0x1034f},
	{"Ugaritic", "Ugaritic", 0x10380, 0x1039f},

	{"Deseret", "Deseret", 0x10400, 0x1044f},
	{"Shavian", "Shavian", 0x10450, 0x1047f},

	{"Osmanya", "Osmanya", 0x10480, 0x104af},
	{"Cypriot Syll", "Cypriot Syllabary", 0x10800, 0x1083f},

	{"Bysantine Mus.", "Bysantine Musical Symbols", 0x1d000, 0x1d0ff},
	{"Music Symb.", "Musical Symbols", 0x1d100, 0x1d1ff},

	{"Tai Xuan Symb", "Tai Xuan Jing Symbols", 0x1d300, 0x1d35f},
	{"Math. Alpha Symb.", "Mathematical Alpanumeric Symbols", 0x1d400, 0x1d7ff},


	{"Tags", "Tags", 0xe0000, 0xe007f},
	{"Var. Supp", "Variation Selectors Supplement", 0xe0100, 0xe01ef},

	{"Supp. Priv. A", "Supplementary Private Use Area-A", 0xf0000, 0xffffd},
	{"Supp. Priv. B", "Supplementary Private Use Area-B", 0x100000, 0x10fffd}
};


/* *************************** static functions prototypes ****************** */
VFont *exist_vfont(char *str);

/* *************** */

void do_common_editbuts(unsigned short event) // old name, is a mix of object and editing events.... 
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	Base *base;
	Object *ob= OBACT;
	Material *ma;
	Nurb *nu;
	Curve *cu;
	BezTriple *bezt;
	BPoint *bp;
	unsigned int local;
	int a, bit, index= -1;

	switch(event) {
		
	case B_MATWICH:
		if(G.obedit && G.obedit->actcol>0) {
			if(G.obedit->type == OB_MESH) {
				for(efa= em->faces.first; efa; efa= efa->next) {
					if(efa->f & SELECT) {
						if(index== -1) index= efa->mat_nr;
						else if(index!=efa->mat_nr) {
							error("Mixed colors");
							return;
						}
					}
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
		new_material_to_objectdata(ob);
		scrarea_queue_winredraw(curarea);
		BIF_undo_push("New material");
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWVIEW3D_Z, 0);
		allqueue(REDRAWOOPS, 0);
		break;
	case B_MATDEL:
		delete_material_index();
		scrarea_queue_winredraw(curarea);
		BIF_undo_push("Delete material index");
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWVIEW3D_Z, 0);
		allqueue(REDRAWOOPS, 0);
		break;
	case B_MATASS:
		if(G.obedit && G.obedit->actcol>0) {
			if(G.obedit->type == OB_MESH) {
				efa= em->faces.first;
				while(efa) {
					if(efa->f & SELECT)
						efa->mat_nr= G.obedit->actcol-1;
					efa= efa->next;
				}
			}
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
				nu= editNurb.first;
				while(nu) {
					if( isNurbsel(nu) )
						nu->mat_nr= nu->charidx= G.obedit->actcol-1;
					nu= nu->next;
				}
			}
			else if (G.obedit->type == OB_FONT) {
        		if (mat_to_sel()) {
        			allqueue(REDRAWVIEW3D, 0);
        		}
			}
			allqueue(REDRAWVIEW3D_Z, 0);
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			shade_buttons_change_3d();
			BIF_undo_push("Assign material index");
		}
		break;
	case B_MATASS_BROWSE:
		/* if slot available, make that index active, and assign */
		/* else, make new slot, and assign */
		ma= BLI_findlink(&G.main->mat, G.buts->menunr-1);
		if(ma) {
			ob->actcol= find_material_index(ob, ma);
			if(ob->actcol==0) {
				assign_material(ob, ma, ob->totcol+1);
				ob->actcol= ob->totcol;
			}
		}
		else {
			do_common_editbuts(B_MATNEW);
		}
		do_common_editbuts(B_MATASS);
		break;
		
	case B_MATCOL2:
		ma= give_current_material(ob, ob->actcol);
		BKE_icon_changed(BKE_icon_getid((ID *)ma));
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
		
	case B_MATSEL:
	case B_MATDESEL:
		if(G.obedit) {
			if(G.obedit->type == OB_MESH) {
				if (event==B_MATSEL) {
					editmesh_select_by_material(G.obedit->actcol-1);
				} else {
					editmesh_deselect_by_material(G.obedit->actcol-1);
				}
				allqueue(REDRAWVIEW3D, 0);
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
										bezt->f1 |= SELECT;
										bezt->f2 |= SELECT;
										bezt->f3 |= SELECT;
									}
									else {
										bezt->f1 &= ~SELECT;
										bezt->f2 &= ~SELECT;
										bezt->f3 &= ~SELECT;
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
									if(event==B_MATSEL) bp->f1 |= SELECT;
									else bp->f1 &= ~SELECT;
								}
								bp++;
							}
						}
					}
					nu= nu->next;
				}
				BIF_undo_push("Select material index");
				allqueue(REDRAWVIEW3D, 0);
			}
		}
		countall();
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
		else if(FACESEL_PAINT_TEST) reveal_tface();
		
		break;
	case B_SELSWAP:
		if(G.obedit) {
			if(G.obedit->type == OB_MESH) selectswap_mesh();
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) selectswapNurb();
		}
		break;
	case B_AUTOTEX:
		if(ob && G.obedit==0) {
			if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) tex_space_curve(ob->data);
		}
		break;
	case B_DOCENTER:
		docenter(0);
		break;
	case B_DOCENTERNEW:
		docenter_new();
		break;
	case B_DOCENTERCURSOR:
		docenter_cursor();
		break;
	case B_SETSMOOTH:
	case B_SETSOLID:
		if(G.obedit) {
			if(G.obedit->type == OB_MESH) {
				mesh_set_smooth_faces((event==B_SETSMOOTH));
			}
			else {
				nurb_set_smooth((event==B_SETSMOOTH));
			}
		}
		else if(G.vd) {
			base= FIRSTBASE;
			while(base) {
				if(TESTBASELIB(base)) {
					if(base->object->type==OB_MESH) {
						mesh_set_smooth_flag(base->object, (event==B_SETSMOOTH));
					}
					else if ELEM(base->object->type, OB_SURF, OB_CURVE) {
						cu= base->object->data;
						nu= cu->nurb.first;
						while(nu) {
							if(event==B_SETSMOOTH) nu->flag |= ME_SMOOTH;
							else nu->flag &= ~ME_SMOOTH;
							nu= nu->next;
						}
						makeDispListCurveTypes(base->object, 0);
					}
				}
				base= base->next;
			}
			allqueue(REDRAWVIEW3D, 0);
			
			if(event == B_SETSMOOTH) BIF_undo_push("Set Smooth");
			else BIF_undo_push("Set Solid");
		}
		break;
	case B_CHANGEDEP:
		DAG_scene_sort(G.scene); // makes new dag
		if(ob) ob->recalc |= OB_RECALC;
		allqueue(REDRAWVIEW3D, 0);
		break;
	
	case B_ADDKEY:
		insert_shapekey(ob);
		break;
	case B_SETKEY:
		ob->shapeflag |= OB_SHAPE_TEMPLOCK;
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_LOCKKEY:
		ob->shapeflag &= ~OB_SHAPE_TEMPLOCK;
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_NEXTKEY:
	{
		Key *key= ob_get_key(ob);
		if(ob->shapenr == BLI_countlist(&key->block))
		   ob->shapenr= 1;
		else ob->shapenr++;
		do_common_editbuts(B_SETKEY);
		break;
	}
	case B_PREVKEY:
	{
		Key *key= ob_get_key(ob);
		if(ob->shapenr <= 1)
			ob->shapenr= BLI_countlist(&key->block);
		else ob->shapenr--;
		do_common_editbuts(B_SETKEY);
		break;
	}
	case B_NAMEKEY:
		allspace(REMAKEIPO, 0);
        allqueue (REDRAWIPO, 0);
		break;
	case B_DELKEY:
		delete_key(OBACT);
		allqueue(REDRAWACTION, 0);
		break;
		
		
	default:
		if (G.vd==NULL)
			break;
		
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
			if( (ob->lay & G.vd->lay) && (BASACT->lay & G.vd->lay) );
			else if( (ob->lay & G.vd->lay)==0 && (BASACT->lay & G.vd->lay)==0 );
			else {
				allqueue(REDRAWVIEW3D, 0);
				DAG_scene_sort(G.scene);
			}
			ob->lay= BASACT->lay;
		}
	}

}

/* *************************** MESH  ******************************** */

static void verify_customdata_name_func(void *data1, void *data2)
{
	CustomData *data= (CustomData*)data1;
	CustomDataLayer *layer= (CustomDataLayer*)data2;

	CustomData_set_layer_unique_name(data, layer - data->layers);
}

static void delete_customdata_layer(void *data1, void *data2)
{
	Mesh *me= (Mesh*)data1;
	CustomData *data= (G.obedit)? &G.editMesh->fdata: &me->fdata;
	CustomDataLayer *layer= (CustomDataLayer*)data2;
	void *actlayerdata, *rndlayerdata, *layerdata=layer->data;
	int type= layer->type;
	int index= CustomData_get_layer_index(data, type);
	int i, actindex, rndindex;
	
	/*ok, deleting a non-active layer needs to preserve the active layer indices.
	  to do this, we store a pointer to the .data member of both layer and the active layer,
	  (to detect if we're deleting the active layer or not), then use the active
	  layer data pointer to find where the active layer has ended up.
	  
	  this is necassary because the deletion functions only support deleting the active
	  layer. */
	actlayerdata = data->layers[CustomData_get_active_layer_index(data, type)].data;
	rndlayerdata = data->layers[CustomData_get_render_layer_index(data, type)].data;
	CustomData_set_layer_active(data, type, layer - &data->layers[index]);

	/* Multires is handled seperately because the display data is separate
	   from the data stored in multires */
	if(me && me->mr) {
		multires_delete_layer(me, &me->mr->fdata, type, layer - &data->layers[index]);
		multires_level_to_editmesh(OBACT, me, 0);
		multires_finish_mesh_update(OBACT);
	}
	else if(G.obedit) {
		EM_free_data_layer(data, type);
	}
	else if(me) {
		CustomData_free_layer_active(data, type, me->totface);
		mesh_update_customdata_pointers(me);
	}

	if(!CustomData_has_layer(data, type)) {
		if(type == CD_MCOL && (G.f & G_VERTEXPAINT))
			G.f &= ~G_VERTEXPAINT; /* get out of vertexpaint mode */
	}

	/*reconstruct active layer*/
	if (actlayerdata != layerdata) {
		/*find index. . .*/
		actindex = CustomData_get_layer_index(data, type);
		for (i=actindex; i<data->totlayer; i++) {
			if (data->layers[i].data == actlayerdata) {
				actindex = i - actindex;
				break;
			}
		}
		
		/*set index. . .*/
		CustomData_set_layer_active(data, type, actindex);
	}
	
	if (rndlayerdata != layerdata) {
		/*find index. . .*/
		rndindex = CustomData_get_layer_index(data, type);
		for (i=rndindex; i<data->totlayer; i++) {
			if (data->layers[i].data == rndlayerdata) {
				rndindex = i - rndindex;
				break;
			}
		}
		
		/*set index. . .*/
		CustomData_set_layer_render(data, type, rndindex);
	}
	
	
	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	
	if(type == CD_MTFACE)
		BIF_undo_push("Delete UV Texture");
	else if(type == CD_MCOL)
		BIF_undo_push("Delete Vertex Color");

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

static int customdata_buttons(
	uiBlock *block,	Mesh *me, CustomData *data,
	int type, int *activep,	int *renderp,
	int setevt, int setevt_rnd, int newevt,
	char *label, char *shortlabel, char *browsetip, char *browsetip_rnd,
	char *newtip, char *deltip, int x, int y)
{
	CustomDataLayer *layer;
	uiBut *but;
	int i, count= CustomData_number_of_layers(data, type);

	if(count >= MAX_MTFACE) {
		uiDefBut(block, LABEL, 0, label, x,y,220,19, 0, 0.0, 0, 0, 0, "");
	}
	else {
		uiDefBut(block, LABEL, 0, label, x,y,140,19, 0, 0.0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, newevt, "New", x+140,y,80,19, 0,0,0,0,0, newtip);
		uiBlockEndAlign(block);
	}

	y -= (count)? 24: 19;

	uiBlockBeginAlign(block);
	for (count=1, i=0; i<data->totlayer; i++) {
		layer= &data->layers[i];

		if(layer->type == type) {
			*activep= layer->active + 1;
			*renderp= layer->active_rnd + 1;
			
			uiDefIconButI(block, ROW, setevt, ICON_VIEW3D, x,y,25,19, activep, 1.0, count, 0, 0, browsetip);
			uiDefIconButI(block, ROW, setevt_rnd, ICON_SCENE, x+25,y,25,19, renderp, 1.0, count, 0, 0, browsetip_rnd);
			but=uiDefBut(block, TEX, setevt, "", x+50,y,145,19, layer->name, 0.0, 31.0, 0, 0, label);
			uiButSetFunc(but, verify_customdata_name_func, data, layer);
			but= uiDefIconBut(block, BUT, B_NOP, VICON_X, x+195,y,25,19, NULL, 0.0, 0.0, 0.0, 0.0, deltip);
			uiButSetFunc(but, delete_customdata_layer, me, layer);


			count++;
			y -= 19;
		}
	}
	uiBlockEndAlign(block);

	return y;
}

static void editing_panel_mesh_type(Object *ob, Mesh *me)
{
	uiBlock *block;
	uiBut *but;
	float val;
	CustomData *fdata;
	int yco;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_type", UI_EMBOSS, UI_HELV, curarea->win);
	if( uiNewPanel(curarea, block, "Mesh", "Editing", 320, 0, 318, 204)==0) return;
	uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);

	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, ME_AUTOSMOOTH, REDRAWVIEW3D, "Auto Smooth",10,180,170,19, &me->flag, 0, 0, 0, 0, "Treats all set-smoothed faces with angles less than Degr: as 'smooth' during render");
	uiDefButS(block, NUM, B_DIFF, "Degr:",				10,160,170,19, &me->smoothresh, 1, 80, 0, 0, "Defines maximum angle between face normals that 'Auto Smooth' will operate on");
	uiBlockEndAlign(block);

	/* Retopo */
	if(G.obedit) {
		uiBlockBeginAlign(block);
		but= uiDefButBitC(block,TOG,RETOPO,B_NOP, "Retopo", 10,130,170,19, &G.scene->toolsettings->retopo_mode, 0,0,0,0, "Turn on the re-topology tool");
		uiButSetFunc(but,retopo_toggle,ob,me);
		if(G.scene->toolsettings->retopo_mode) {
			but= uiDefButBitC(block,TOG,RETOPO_PAINT,B_NOP,"Paint", 10,110,55,19, &G.scene->toolsettings->retopo_mode,0,0,0,0, "Draw intersecting lines in the 3d view, ENTER creates quad or tri faces, wrapped onto other objects in the 3d view.");
			uiButSetFunc(but,retopo_paint_toggle,ob,me);
			but= uiDefBut(block,BUT,B_NOP,"Retopo All", 65,110,115,19, 0,0,0,0,0, "Apply the re-topology tool to all selected vertices");
			uiButSetFunc(but,retopo_do_all_cb,ob,me);
		}
		uiBlockEndAlign(block);
	}

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_DOCENTER, "Center",					10, 80, 65, 19, 0, 0, 0, 0, 0, "Shifts object data to be centered about object's origin");
	uiDefBut(block, BUT,B_DOCENTERNEW, "Center New",			75, 80, 105, 19, 0, 0, 0, 0, 0, "Shifts object's origin to center of object data");
	uiDefBut(block, BUT,B_DOCENTERCURSOR, "Center Cursor",		10, 60, 170, 19, 0, 0, 0, 0, 0, "Shifts object's origin to cursor location");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, ME_TWOSIDED, REDRAWVIEW3D, "Double Sided",	10,30,170,19, &me->flag, 0, 0, 0, 0, "Render/display the mesh as double or single sided");
	uiDefButBitS(block, TOG, ME_NOPUNOFLIP, REDRAWVIEW3D, "No V.Normal Flip", 10,10,170,19, &me->flag, 0, 0, 0, 0, "Disables flipping of vertexnormals during render");
	uiBlockEndAlign(block);

	uiDefIDPoinBut(block, test_meshpoin_but, ID_ME, B_REDR, "TexMesh: ",	190,180,220,19, &me->texcomesh, "Derive texture coordinates from another mesh.");

	if(me->msticky) val= 1.0; else val= 0.0;
	uiDefBut(block, LABEL, 0, "Sticky", 				190,155,140,19, 0, val, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	if(me->msticky==NULL) {
		uiDefBut(block, BUT, B_MAKESTICKY, "Make",		330,155, 80,19, 0, 0, 0, 0, 0, "Creates Sticky coordinates from the current camera view background picture");
	}
	else uiDefBut(block, BUT, B_DELSTICKY, "Delete", 	330,155, 80,19, 0, 0, 0, 0, 0, "Deletes Sticky texture coordinates");
	uiBlockEndAlign(block);

	fdata= (G.obedit)? &G.editMesh->fdata: &me->fdata;
	yco= customdata_buttons(block, me, fdata, CD_MTFACE, &acttface, &acttface_rnd,
		B_SETTFACE, B_SETTFACE_RND, B_NEWTFACE, "UV Texture", "UV Texture:",
		"Set active UV texture", "Set rendering UV texture", "Creates a new UV texture layer",
		"Removes the current UV texture layer", 190, 130);

	yco= customdata_buttons(block, me, fdata, CD_MCOL, &actmcol, &actmcol_rnd,
		B_SETMCOL, B_SETMCOL_RND, B_NEWMCOL, "Vertex Color", "Vertex Color:",
		"Sets active vertex color layer", "Sets rendering vertex color layer", "Creates a new vertex color layer",
		"Removes the current vertex color layer", 190, yco-5);

	if(yco < 0)
		uiNewPanelHeight(block, 204 - yco);
}

/* *************************** MODIFIERS ******************************** */

void do_modifier_panels(unsigned short event)
{
	Object *ob = OBACT;

	switch(event) {
	case B_MODIFIER_REDRAW:
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWOOPS, 0);
		break;

	case B_MODIFIER_RECALC:
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWOOPS, 0);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		object_handle_update(ob);
		countall();
		break;
	}
}

static void modifiers_add(void *ob_v, int type)
{
	Object *ob = ob_v;
	ModifierTypeInfo *mti = modifierType_getInfo(type);
	
	if (mti->flags&eModifierTypeFlag_RequiresOriginalData) {
		ModifierData *md = ob->modifiers.first;

		while (md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform) {
			md = md->next;
		}

		BLI_insertlinkbefore(&ob->modifiers, md, modifier_new(type));
	} else {
		BLI_addtail(&ob->modifiers, modifier_new(type));
	}
	BIF_undo_push("Add modifier");
}

typedef struct MenuEntry {
	char *name;
	int ID;
} MenuEntry;

static int menuEntry_compare_names(const void *entry1, const void *entry2)
{
	return strcmp(((MenuEntry *)entry1)->name, ((MenuEntry *)entry2)->name);
}

static uiBlock *modifiers_add_menu(void *ob_v)
{
	Object *ob = ob_v;
	uiBlock *block;
	int i, yco=0;
	int numEntries = 0;
	MenuEntry entries[NUM_MODIFIER_TYPES];
	
	block= uiNewBlock(&curarea->uiblocks, "modifier_add_menu",
	                  UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, modifiers_add, ob);

	for (i=eModifierType_None+1; i<NUM_MODIFIER_TYPES; i++) {
		ModifierTypeInfo *mti = modifierType_getInfo(i);

		/* Only allow adding through appropriate other interfaces */
		if(ELEM3(i, eModifierType_Softbody, eModifierType_Hook, eModifierType_ParticleSystem)) continue;
		
		if(ELEM(i, eModifierType_Cloth, eModifierType_Collision)) continue;

		if((mti->flags&eModifierTypeFlag_AcceptsCVs) ||
		   (ob->type==OB_MESH && (mti->flags&eModifierTypeFlag_AcceptsMesh))) {
			entries[numEntries].name = mti->name;
			entries[numEntries].ID = i;

			++numEntries;
		}
	}

	qsort(entries, numEntries, sizeof(*entries), menuEntry_compare_names);


	for(i = 0; i < numEntries; ++i)
		uiDefBut(block, BUTM, B_MODIFIER_RECALC, entries[i].name,
		         0, yco -= 20, 160, 19, NULL, 0, 0, 1, entries[i].ID, "");

	uiTextBoundsBlock(block, 50);
	uiBlockSetDirection(block, UI_DOWN);

	return block;
}

static void modifiers_del(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md;

		/* It seems on rapid delete it is possible to
		 * get called twice on same modifier, so make
		 * sure it is in list.
		 */
	for (md=ob->modifiers.first; md; md=md->next)
		if (md==md_v)
			break;
	
	if (!md)
		return;

	if(md->type==eModifierType_ParticleSystem){
		ParticleSystemModifierData *psmd=(ParticleSystemModifierData*)md;
		BLI_remlink(&ob->particlesystem, psmd->psys);
		psys_free(ob,psmd->psys);
	}

	BLI_remlink(&ob->modifiers, md_v);

	modifier_free(md_v);

	BIF_undo_push("Del modifier");
}

int mod_moveUp(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;

	if (md->prev) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->type!=eModifierTypeType_OnlyDeform) {
			ModifierTypeInfo *nmti = modifierType_getInfo(md->prev->type);

			if (nmti->flags&eModifierTypeFlag_RequiresOriginalData)
				return -1;
		}

		BLI_remlink(&ob->modifiers, md);
		BLI_insertlink(&ob->modifiers, md->prev->prev, md);
	}

	return 0;
}

static void modifiers_moveUp(void *ob_v, void *md_v)
{
	if( mod_moveUp( ob_v, md_v ) )
		error("Cannot move above a modifier requiring original data.");
	else
		BIF_undo_push("Move modifier");
}

int mod_moveDown(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;

	if (md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->flags&eModifierTypeFlag_RequiresOriginalData) {
			ModifierTypeInfo *nmti = modifierType_getInfo(md->next->type);

			if (nmti->type!=eModifierTypeType_OnlyDeform)
				return -1;
		}

		BLI_remlink(&ob->modifiers, md);
		BLI_insertlink(&ob->modifiers, md->next, md);
	}

	return 0;
}

static void modifiers_moveDown(void *ob_v, void *md_v)
{
	if( mod_moveDown( ob_v, md_v ) )
		error("Cannot move beyond a non-deforming modifier.");
	else
		BIF_undo_push("Move modifier");
}

static void modifier_testLatticeObj(char *name, ID **idpp)
{
	ID *id;

	for (id= G.main->object.first; id; id= id->next) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_LATTICE) {
				error ("Lattice deform object must be a lattice");
				break;
			} 
			*idpp= id;
			return;
		}
	}
	*idpp= 0;
}

static void modifier_testCurveObj(char *name, ID **idpp)
{
	ID *id;

	for (id= G.main->object.first; id; id= id->next) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_CURVE) {
				error ("Curve deform object must be a curve");
				break;
			} 
			*idpp= id;
			return;
		}
	}
	*idpp= 0;
}

static void modifier_testMeshObj(char *name, ID **idpp)
{
	ID *id;

	for (id= G.main->object.first; id; id= id->next) {
		/* no boolean on its own object */
		if(id != (ID *)OBACT) {
			if( strcmp(name, id->name+2)==0 ) {
				if (((Object *)id)->type != OB_MESH) {
					error ("Boolean modifier object must be a mesh");
					break;
				} 
				*idpp= id;
				return;
			}
		}
	}
	*idpp= NULL;
}

static void modifier_testArmatureObj(char *name, ID **idpp)
{
	ID *id;

	for (id= G.main->object.first; id; id= id->next) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_ARMATURE) {
				error ("Armature deform object must be an armature");
				break;
			} 
			*idpp= id;
			return;
		}
	}
	*idpp= 0;
}

static void modifier_testTexture(char *name, ID **idpp)
{
	ID *id;

	for(id = G.main->tex.first; id; id = id->next) {
		if(strcmp(name, id->name + 2) == 0) {
			*idpp = id;
			/* texture gets user, objects not: delete object = clear modifier */
			id_us_plus(id);
			return;
		}
	}
	*idpp = 0;
}

#if 0 /* this is currently unused, but could be useful in the future */
static void modifier_testMaterial(char *name, ID **idpp)
{
	ID *id;

	for(id = G.main->mat.first; id; id = id->next) {
		if(strcmp(name, id->name + 2) == 0) {
			*idpp = id;
			return;
		}
	}
	*idpp = 0;
}
#endif

static void modifier_testImage(char *name, ID **idpp)
{
	ID *id;

	for(id = G.main->image.first; id; id = id->next) {
		if(strcmp(name, id->name + 2) == 0) {
			*idpp = id;
			return;
		}
	}
	*idpp = 0;
}

/* autocomplete callback for ID buttons */
void autocomplete_image(char *str, void *arg_v)
{
	/* search if str matches the beginning of an ID struct */
	if(str[0]) {
		AutoComplete *autocpl = autocomplete_begin(str, 22);
		ID *id;

		for(id = G.main->image.first; id; id = id->next)
			autocomplete_do_name(autocpl, id->name+2);

		autocomplete_end(autocpl, str);
	}
}

/* autocomplete callback for ID buttons */
void autocomplete_meshob(char *str, void *arg_v)
{
	/* search if str matches the beginning of an ID struct */
	if(str[0]) {
		AutoComplete *autocpl = autocomplete_begin(str, 22);
		ID *id;

		for(id = G.main->object.first; id; id = id->next)
			if(((Object *)id)->type == OB_MESH)
				autocomplete_do_name(autocpl, id->name+2);

		autocomplete_end(autocpl, str);
	}
}
static void modifiers_convertParticles(void *obv, void *mdv)
{
	Object *obn;
	ModifierData *md = mdv;
	ParticleSystem *psys;
	ParticleCacheKey *key, **cache;
	Mesh *me;
	MVert *mvert;
	MEdge *medge;
	int a, k, kmax;
	int totvert=0, totedge=0, cvert=0;
	int totpart=0, totchild=0;

	if(md->type != eModifierType_ParticleSystem) return;

	if(G.f & G_PARTICLEEDIT) return;

	psys=((ParticleSystemModifierData *)md)->psys;

	if(psys->part->draw_as != PART_DRAW_PATH || psys->pathcache == 0) return;

	totpart= psys->totcached;
	totchild= psys->totchildcache;

	if(totchild && (psys->part->draw&PART_DRAW_PARENT)==0)
		totpart= 0;

	/* count */
	cache= psys->pathcache;
	for(a=0; a<totpart; a++) {
		key= cache[a];
		totvert+= key->steps+1;
		totedge+= key->steps;
	}

	cache= psys->childcache;
	for(a=0; a<totchild; a++) {
		key= cache[a];
		totvert+= key->steps+1;
		totedge+= key->steps;
	}

	if(totvert==0) return;

	/* add new mesh */
	obn= add_object(OB_MESH);
	me= obn->data;
	
	me->totvert= totvert;
	me->totedge= totedge;
	
	me->mvert= CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
	me->medge= CustomData_add_layer(&me->edata, CD_MEDGE, CD_CALLOC, NULL, totedge);
	me->mface= CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, 0);
	
	mvert= me->mvert;
	medge= me->medge;

	/* copy coordinates */
	cache= psys->pathcache;
	for(a=0; a<totpart; a++) {
		key= cache[a];
		kmax= key->steps;
		for(k=0; k<=kmax; k++,key++,cvert++,mvert++) {
			VECCOPY(mvert->co,key->co);
			if(k) {
				medge->v1= cvert-1;
				medge->v2= cvert;
				medge->flag= ME_EDGEDRAW|ME_EDGERENDER|ME_LOOSEEDGE;
				medge++;
			}
		}
	}

	cache=psys->childcache;
	for(a=0; a<totchild; a++) {
		key=cache[a];
		kmax=key->steps;
		for(k=0; k<=kmax; k++,key++,cvert++,mvert++) {
			VECCOPY(mvert->co,key->co);
			if(k) {
				medge->v1=cvert-1;
				medge->v2=cvert;
				medge->flag= ME_EDGEDRAW|ME_EDGERENDER|ME_LOOSEEDGE;
				medge++;
			}
		}
	}

	DAG_scene_sort(G.scene);
}

static void modifiers_applyModifier(void *obv, void *mdv)
{
	Object *ob = obv;
	ModifierData *md = mdv;
	DerivedMesh *dm;
	Mesh *me = ob->data;
	int converted = 0;

	if (G.obedit) {
		error("Modifiers cannot be applied in editmode");
		return;
	} else if (((ID*) ob->data)->us>1) {
		error("Modifiers cannot be applied to multi-user data");
		return;
	}

	if (md!=ob->modifiers.first) {
		if (!okee("Modifier is not first"))
			return;
	}

	if (ob->type==OB_MESH) {
		if(me->mr && multires_modifier_warning()) {
			error("Modifier changes topology; cannot apply with multires active");
			return;
		}
		if(me->key) {
			error("Modifier cannot be applied to Mesh with Shape Keys");
			return;
		}
	
		mesh_pmv_off(ob, me);
	
		dm = mesh_create_derived_for_modifier(ob, md);
		if (!dm) {
			error("Modifier is disabled or returned error, skipping apply");
			return;
		}

		DM_to_mesh(dm, me);
		converted = 1;

		dm->release(dm);
	} 
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		Curve *cu = ob->data;
		int numVerts;
		float (*vertexCos)[3];

		if (!okee("Apply will only change CV points, not tesselated/bevel vertices"))
			return;

		if (!(md->mode&eModifierMode_Realtime) || (mti->isDisabled && mti->isDisabled(md))) {
			error("Modifier is disabled, skipping apply");
			return;
		}

		vertexCos = curve_getVertexCos(cu, &cu->nurb, &numVerts);
		mti->deformVerts(md, ob, NULL, vertexCos, numVerts);
		curve_applyVertexCos(cu, &cu->nurb, vertexCos);

		converted = 1;

		MEM_freeN(vertexCos);

		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	}
	else {
		error("Cannot apply modifier for this object type");
		return;
	}

	if (converted) {
		BLI_remlink(&ob->modifiers, md);
		modifier_free(md);

		BIF_undo_push("Apply modifier");
	}
}

static void modifiers_copyModifier(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;
	ModifierData *nmd = modifier_new(md->type);

	modifier_copyData(md, nmd);

	BLI_insertlink(&ob->modifiers, md, nmd);

	BIF_undo_push("Copy modifier");
}

static void modifiers_setOnCage(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md;
	
	int i, cageIndex = modifiers_getCageIndex(ob, NULL );

	for( i = 0, md=ob->modifiers.first; md; ++i, md=md->next )
		if( md == md_v ) {
			if( i >= cageIndex )
				md->mode ^= eModifierMode_OnCage;
			break;
		}
}

static void modifiers_clearHookOffset(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;
	
	if (hmd->object) {
		Mat4Invert(hmd->object->imat, hmd->object->obmat);
		Mat4MulSerie(hmd->parentinv, hmd->object->imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
		BIF_undo_push("Clear hook offset");
	}
}

static void modifiers_cursorHookCenter(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;

	if (G.vd) {
		float *curs = give_cursor();
		float bmat[3][3], imat[3][3];

		where_is_object(ob);
	
		Mat3CpyMat4(bmat, ob->obmat);
		Mat3Inv(imat, bmat);

		curs= give_cursor();
		hmd->cent[0]= curs[0]-ob->obmat[3][0];
		hmd->cent[1]= curs[1]-ob->obmat[3][1];
		hmd->cent[2]= curs[2]-ob->obmat[3][2];
		Mat3MulVecfl(imat, hmd->cent);

		BIF_undo_push("Hook cursor center");
	}
}

static void modifiers_selectHook(void *ob_v, void *md_v)
{
	ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;

	hook_select(hmd);
}

static void modifiers_reassignHook(void *ob_v, void *md_v)
{
	ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;
	float cent[3];
	int *indexar, tot, ok;
	char name[32];
		
	ok= hook_getIndexArray(&tot, &indexar, name, cent);

	if (!ok) {
		error("Requires selected vertices or active Vertex Group");
	} else {
		if (hmd->indexar) {
			MEM_freeN(hmd->indexar);
		}

		VECCOPY(hmd->cent, cent);
		hmd->indexar = indexar;
		hmd->totindex = tot;
	}
}

static void modifiers_convertToReal(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;
	ModifierData *nmd = modifier_new(md->type);

	modifier_copyData(md, nmd);
	nmd->mode &= ~eModifierMode_Virtual;

	BLI_addhead(&ob->modifiers, nmd);

	ob->partype = PAROBJECT;

	BIF_undo_push("Modifier convert to real");
}

static void build_uvlayer_menu_vars(CustomData *data, char **menu_string,
                                    int *uvlayer_tmp, char *uvlayer_name)
{
	char strtmp[38];
	int totuv, i;
	CustomDataLayer *layer
	            = &data->layers[CustomData_get_layer_index(data, CD_MTFACE)];

	*uvlayer_tmp = -1;

	totuv = CustomData_number_of_layers(data, CD_MTFACE);

	*menu_string = MEM_callocN(sizeof(**menu_string) * (totuv * 38 + 10),
	                           "menu_string");
	sprintf(*menu_string, "UV Layer%%t");
	for(i = 0; i < totuv; i++) {
		/* assign first layer as uvlayer_name if uvlayer_name is null. */
		if(strcmp(layer->name, uvlayer_name) == 0) *uvlayer_tmp = i + 1;
		sprintf(strtmp, "|%s%%x%d", layer->name, i + 1);
		strcat(*menu_string, strtmp);
		layer++;
	}

	/* there is no uvlayer defined, or else it was deleted. Assign active
	 * layer, then recalc modifiers.
	 */
	if(*uvlayer_tmp == -1) {
		if(CustomData_get_active_layer_index(data, CD_MTFACE) != -1) {
			*uvlayer_tmp = 1;
			layer = data->layers;
			for(i = 0; i < CustomData_get_active_layer_index(data, CD_MTFACE);
			    i++, layer++) {
				if(layer->type == CD_MTFACE) (*uvlayer_tmp)++;
			}
			strcpy(uvlayer_name, layer->name);

			/* update the modifiers */
			do_modifier_panels(B_MODIFIER_RECALC);
		} else {
			/* ok we have no uv layers, so make sure menu button knows that.*/
			*uvlayer_tmp = 0;
		}
	}
}

void set_displace_uvlayer(void *arg1, void *arg2)
{
	DisplaceModifierData *dmd=arg1;
	CustomDataLayer *layer = arg2;

	/*check we have UV layers*/
	if (dmd->uvlayer_tmp < 1) return;
	layer = layer + (dmd->uvlayer_tmp-1);
	
	strcpy(dmd->uvlayer_name, layer->name);
}

void set_uvproject_uvlayer(void *arg1, void *arg2)
{
	UVProjectModifierData *umd=arg1;
	CustomDataLayer *layer = arg2;

	/*check we have UV layers*/
	if (umd->uvlayer_tmp < 1) return;
	layer = layer + (umd->uvlayer_tmp-1);
	
	strcpy(umd->uvlayer_name, layer->name);
}

static void modifiers_bindMeshDeform(void *ob_v, void *md_v)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md_v;
	Object *ob = (Object*)ob_v;

	if(mmd->bindcos) {
		if(mmd->bindweights) MEM_freeN(mmd->bindweights);
		if(mmd->bindcos) MEM_freeN(mmd->bindcos);
		if(mmd->dyngrid) MEM_freeN(mmd->dyngrid);
		if(mmd->dyninfluences) MEM_freeN(mmd->dyninfluences);
		if(mmd->dynverts) MEM_freeN(mmd->dynverts);
		mmd->bindweights= NULL;
		mmd->bindcos= NULL;
		mmd->dyngrid= NULL;
		mmd->dyninfluences= NULL;
		mmd->dynverts= NULL;
		mmd->totvert= 0;
		mmd->totcagevert= 0;
		mmd->totinfluence= 0;
	}
	else {
		DerivedMesh *dm;
		int mode= mmd->modifier.mode;

		/* force modifier to run, it will call binding routine */
		mmd->needbind= 1;
		mmd->modifier.mode |= eModifierMode_Realtime;

		if(ob->type == OB_MESH) {
			dm= mesh_create_derived_view(ob, 0);
			dm->release(dm);
		}
		else if(ob->type == OB_LATTICE) {
			lattice_calc_modifiers(ob);
		}
		else if(ob->type==OB_MBALL) {
			makeDispListMBall(ob);
		}
		else if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
			makeDispListCurveTypes(ob, 0);
		}

		mmd->needbind= 0;
		mmd->modifier.mode= mode;
	}
}

void modifiers_explodeFacepa(void *arg1, void *arg2)
{
	ExplodeModifierData *emd=arg1;

	emd->flag |= eExplodeFlag_CalcFaces;
}

static int modifier_is_fluid_particles(ModifierData *md) {
	if(md->type == eModifierType_ParticleSystem) {
		if(((ParticleSystemModifierData *)md)->psys->part->type == PART_FLUID)
			return 1;
	}
	return 0;
}
static void draw_modifier(uiBlock *block, Object *ob, ModifierData *md, int *xco, int *yco, int index, int cageIndex, int lastCageIndex)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	uiBut *but;
	int isVirtual = md->mode&eModifierMode_Virtual;
	int x = *xco, y = *yco, color = md->error?TH_REDALERT:TH_BUT_NEUTRAL;
	int editing = (G.obedit==ob);
	short height=26, width = 295, buttonWidth = width-120-10;
	char str[128];

	/* rounded header */
	uiBlockSetCol(block, color);
		/* roundbox 4 free variables: corner-rounding, nop, roundbox type, shade */
	uiDefBut(block, ROUNDBOX, 0, "", x-10, y-4, width, 25, NULL, 7.0, 0.0, 
			 (!isVirtual && (md->mode&eModifierMode_Expanded))?3:15, 20, ""); 
	uiBlockSetCol(block, TH_AUTO);
	
	/* open/close icon */
	if (!isVirtual) {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		uiDefIconButBitI(block, ICONTOG, eModifierMode_Expanded, B_MODIFIER_REDRAW, VICON_DISCLOSURE_TRI_RIGHT, x-10, y-2, 20, 20, &md->mode, 0.0, 0.0, 0.0, 0.0, "Collapse/Expand Modifier");
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
	
	if (isVirtual) {
		sprintf(str, "%s parent deform", md->name);
		uiDefBut(block, LABEL, 0, str, x+10, y-1, width-110, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Modifier name"); 

		but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Make Real", x+width-100, y, 80, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Convert virtual modifier to a real modifier");
		uiButSetFunc(but, modifiers_convertToReal, ob, md);
	} else {
		uiBlockBeginAlign(block);
		uiDefBut(block, TEX, B_MODIFIER_REDRAW, "", x+10, y-1, buttonWidth-60, 19, md->name, 0.0, sizeof(md->name)-1, 0.0, 0.0, "Modifier name"); 

		/* Softbody not allowed in this situation, enforce! */
		if ((md->type!=eModifierType_Softbody && md->type!=eModifierType_Collision) || !(ob->pd && ob->pd->deflect)) {
			uiDefIconButBitI(block, TOG, eModifierMode_Render, B_MODIFIER_RECALC, ICON_SCENE, x+10+buttonWidth-60, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during rendering");
			but= uiDefIconButBitI(block, TOG, eModifierMode_Realtime, B_MODIFIER_RECALC, VICON_VIEW3D, x+10+buttonWidth-40, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during interactive display");
			if (mti->flags&eModifierTypeFlag_SupportsEditmode) {
				uiDefIconButBitI(block, TOG, eModifierMode_Editmode, B_MODIFIER_RECALC, VICON_EDIT, x+10+buttonWidth-20, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during Editmode (only if enabled for display)");
			}
		}
		uiBlockEndAlign(block);

		uiBlockSetEmboss(block, UI_EMBOSSR);

		if (ob->type==OB_MESH && modifier_couldBeCage(md) && index<=lastCageIndex) {
			int icon, color;

			if (index==cageIndex) {
				color = TH_BUT_SETTING;
				icon = VICON_EDITMODE_HLT;
			} else if (index<cageIndex) {
				color = TH_BUT_NEUTRAL;
				icon = VICON_EDITMODE_DEHLT;
			} else {
				color = TH_BUT_NEUTRAL;
				icon = ICON_BLANK1;
			}
			uiBlockSetCol(block, color);
			but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, icon, x+width-105, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Apply modifier to editing cage during Editmode");
			uiButSetFunc(but, modifiers_setOnCage, ob, md);
			uiBlockSetCol(block, TH_AUTO);
		}

		uiBlockSetCol(block, TH_BUT_ACTION);

		but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, VICON_MOVE_UP, x+width-75, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Move modifier up in stack");
		uiButSetFunc(but, modifiers_moveUp, ob, md);

		but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, VICON_MOVE_DOWN, x+width-75+20, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Move modifier down in stack");
		uiButSetFunc(but, modifiers_moveDown, ob, md);
		
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		// deletion over the deflection panel
		// fluid particle modifier can't be deleted here
		if(md->type!=eModifierType_Collision && !modifier_is_fluid_particles(md))
		{
			but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, VICON_X, x+width-70+40, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Delete modifier");
			uiButSetFunc(but, modifiers_del, ob, md);
		}
		uiBlockSetCol(block, TH_AUTO);
	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	if (isVirtual || !(md->mode&eModifierMode_Expanded)) {
		y -= 18;
	} else {
		int cy = y - 8;
		int lx = x + width - 60 - 15;

		if (md->type==eModifierType_Subsurf) {
			height = 105;
		} else if (md->type==eModifierType_Lattice) {
			height = 48;
		} else if (md->type==eModifierType_Curve) {
			height = 72;
		} else if (md->type==eModifierType_Build) {
			height = 86;
		} else if (md->type==eModifierType_Mirror) {
			height = 86;
		} else if (md->type==eModifierType_Bevel) {
			BevelModifierData *bmd = (BevelModifierData*) md;
			height = 105; /* height = 124; */
			if ((bmd->lim_flags & BME_BEVEL_ANGLE) || ((bmd->lim_flags & BME_BEVEL_WEIGHT) && !(bmd->flags & BME_BEVEL_VERT))) height += 19;
		} else if (md->type==eModifierType_EdgeSplit) {
			EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;
			height = 48;
			if(emd->flags & MOD_EDGESPLIT_FROMANGLE) height += 19;
		} else if (md->type==eModifierType_Displace) {
			DisplaceModifierData *dmd = (DisplaceModifierData *)md;
			height = 124;
			if(dmd->texmapping == MOD_DISP_MAP_OBJECT ||
			   dmd->texmapping == MOD_DISP_MAP_UV)
				height += 19;
		} else if (md->type==eModifierType_UVProject) {
			height = 114 + ((UVProjectModifierData *)md)->num_projectors * 19;
		} else if (md->type==eModifierType_Decimate) {
			height = 48;
		} else if (md->type==eModifierType_Smooth) {
			height = 86;
		} else if (md->type==eModifierType_Cast) {
			height = 143;
		} else if (md->type==eModifierType_Wave) {
			WaveModifierData *wmd = (WaveModifierData *)md;
			height = 294;
			if(wmd->texmapping == MOD_WAV_MAP_OBJECT ||
			   wmd->texmapping == MOD_WAV_MAP_UV)
				height += 19;
			if(wmd->flag & MOD_WAVE_NORM)
				height += 19;
		} else if (md->type==eModifierType_Armature) {
			height = 105;
		} else if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;
			height = 86;
			if (editing)
				height += 20;
			if(hmd->indexar==NULL)
				height += 20;
		} else if (md->type==eModifierType_Softbody) {
			height = 31;
		} else if (md->type==eModifierType_Cloth) {
			height = 31;
		} else if (md->type==eModifierType_Collision) {
			height = 31;
		} else if (md->type==eModifierType_Boolean) {
			height = 48;
		} else if (md->type==eModifierType_Array) {
			height = 211;
		} else if (md->type==eModifierType_MeshDeform) {
			MeshDeformModifierData *mmd= (MeshDeformModifierData*)md;
			height = (mmd->bindcos)? 73: 93;
		} else if (md->type==eModifierType_ParticleSystem) {
			height = 31;
		} else if (md->type==eModifierType_ParticleInstance) {
			height = 94;
		} else if (md->type==eModifierType_Explode) {
			height = 94;
		}
							/* roundbox 4 free variables: corner-rounding, nop, roundbox type, shade */
		uiDefBut(block, ROUNDBOX, 0, "", x-10, y-height-2, width, height-2, NULL, 5.0, 0.0, 12, 40, ""); 

		y -= 18;

		if (!isVirtual && (md->type!=eModifierType_Collision)) {
			uiBlockBeginAlign(block);
			if (md->type==eModifierType_ParticleSystem) {
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Convert",	lx,(cy-=19),60,19, 0, 0, 0, 0, 0, "Convert the current particles to a mesh object");
				uiButSetFunc(but, modifiers_convertParticles, ob, md);
			}
			else{
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Apply",	lx,(cy-=19),60,19, 0, 0, 0, 0, 0, "Apply the current modifier and remove from the stack");
				uiButSetFunc(but, modifiers_applyModifier, ob, md);
			}
			
			if (md->type!=eModifierType_Softbody && md->type!=eModifierType_ParticleSystem && (md->type!=eModifierType_Cloth)) {
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Copy",	lx,(cy-=19),60,19, 0, 0, 0, 0, 0, "Duplicate the current modifier at the same position in the stack");
				uiButSetFunc(but, modifiers_copyModifier, ob, md);
			}
			uiBlockEndAlign(block);
		}

		lx = x + 10;
		cy = y + 10 - 1;
		uiBlockBeginAlign(block);
		if (md->type==eModifierType_Subsurf) {
			SubsurfModifierData *smd = (SubsurfModifierData*) md;
			char subsurfmenu[]="Subsurf Type%t|Catmull-Clark%x0|Simple Subdiv.%x1";
			uiDefButS(block, MENU, B_MODIFIER_RECALC, subsurfmenu,		lx,(cy-=19),buttonWidth,19, &smd->subdivType, 0, 0, 0, 0, "Selects type of subdivision algorithm.");
			uiDefButS(block, NUM, B_MODIFIER_RECALC, "Levels:",		lx, (cy-=19), buttonWidth,19, &smd->levels, 1, 6, 0, 0, "Number subdivisions to perform");
			uiDefButS(block, NUM, B_MODIFIER_REDRAW, "Render Levels:",		lx, (cy-=19), buttonWidth,19, &smd->renderLevels, 1, 6, 0, 0, "Number subdivisions to perform when rendering");

			/* Disabled until non-EM DerivedMesh implementation is complete */

			/*
			uiDefButBitS(block, TOG, eSubsurfModifierFlag_Incremental, B_MODIFIER_RECALC, "Incremental", lx, (cy-=19),90,19,&smd->flags, 0, 0, 0, 0, "Use incremental calculation, even outside of mesh mode");
			uiDefButBitS(block, TOG, eSubsurfModifierFlag_DebugIncr, B_MODIFIER_RECALC, "Debug", lx+90, cy,buttonWidth-90,19,&smd->flags, 0, 0, 0, 0, "Visualize the subsurf incremental calculation, for debugging effect of other modifiers");
			*/

			uiDefButBitS(block, TOG, eSubsurfModifierFlag_ControlEdges, B_MODIFIER_RECALC, "Optimal Draw", lx, (cy-=19), buttonWidth,19,&smd->flags, 0, 0, 0, 0, "Skip drawing/rendering of interior subdivided edges");
			uiDefButBitS(block, TOG, eSubsurfModifierFlag_SubsurfUv, B_MODIFIER_RECALC, "Subsurf UV", lx, (cy-=19),buttonWidth,19,&smd->flags, 0, 0, 0, 0, "Use subsurf to subdivide UVs");
		} else if (md->type==eModifierType_Lattice) {
			LatticeModifierData *lmd = (LatticeModifierData*) md;
			uiDefIDPoinBut(block, modifier_testLatticeObj, ID_OB, B_CHANGEDEP, "Ob: ",	lx, (cy-=19), buttonWidth,19, &lmd->object, "Lattice object to deform with");
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth,19, &lmd->name, 0.0, 31.0, 0, 0, "Vertex Group name");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
		} else if (md->type==eModifierType_Curve) {
			CurveModifierData *cmd = (CurveModifierData*) md;
			uiDefIDPoinBut(block, modifier_testCurveObj, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &cmd->object, "Curve object to deform with");
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth,19, &cmd->name, 0.0, 31.0, 0, 0, "Vertex Group name");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"X",		lx, (cy-=19), 19,19, &cmd->defaxis, 12.0, MOD_CURVE_POSX, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"Y",		(lx+buttonWidth/6), cy, 19,19, &cmd->defaxis, 12.0, MOD_CURVE_POSY, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"Z",		(lx+2*buttonWidth/6), cy, 19,19, &cmd->defaxis, 12.0, MOD_CURVE_POSZ, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"-X",		(lx+3*buttonWidth/6), cy, 24,19, &cmd->defaxis, 12.0, MOD_CURVE_NEGX, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"-Y",		(lx+4*buttonWidth/6), cy, 24,19, &cmd->defaxis, 12.0, MOD_CURVE_NEGY, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"-Z",		(lx+buttonWidth-buttonWidth/6), cy, 24,19, &cmd->defaxis, 12.0, MOD_CURVE_NEGZ, 0, 0, "The axis that the curve deforms along");
		} else if (md->type==eModifierType_Build) {
			BuildModifierData *bmd = (BuildModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Start:", lx, (cy-=19), buttonWidth,19, &bmd->start, 1.0, MAXFRAMEF, 100, 0, "Specify the start frame of the effect");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Length:", lx, (cy-=19), buttonWidth,19, &bmd->length, 1.0, MAXFRAMEF, 100, 0, "Specify the total time the build effect requires");
			uiDefButI(block, TOG, B_MODIFIER_RECALC, "Randomize", lx, (cy-=19), buttonWidth,19, &bmd->randomize, 0, 0, 1, 0, "Randomize the faces or edges during build.");
			uiDefButI(block, NUM, B_MODIFIER_RECALC, "Seed:", lx, (cy-=19), buttonWidth,19, &bmd->seed, 1.0, MAXFRAMEF, 100, 0, "Specify the seed for random if used.");
		} else if (md->type==eModifierType_Mirror) {
			MirrorModifierData *mmd = (MirrorModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Merge Limit:", lx, (cy-=19), buttonWidth,19, &mmd->tolerance, 0.0, 1.0, 10, 10, "Distance from axis within which mirrored vertices are merged");
			uiDefButBitS(block, TOG, MOD_MIR_AXIS_X, B_MODIFIER_RECALC, "X",	lx,(cy-=19),20,19, &mmd->flag, 0, 0, 0, 0, "Enable X axis mirror");
			uiDefButBitS(block, TOG, MOD_MIR_AXIS_Y, B_MODIFIER_RECALC, "Y",	lx+20,cy,20,19,    &mmd->flag, 0, 0, 0, 0, "Enable Y axis mirror");
			uiDefButBitS(block, TOG, MOD_MIR_AXIS_Z, B_MODIFIER_RECALC, "Z",	lx+40,cy,20,19,    &mmd->flag, 0, 0, 0, 0, "Enable Z axis mirror");
			uiDefButBitS(block, TOG, MOD_MIR_CLIPPING, B_MODIFIER_RECALC, "Do Clipping",	lx+60, cy, buttonWidth-60,19, &mmd->flag, 1, 2, 0, 0, "Prevents during Transform vertices to go through Mirror");
			uiDefButBitS(block, TOG, MOD_MIR_MIRROR_U, B_MODIFIER_RECALC,
			             "Mirror U",
			             lx, (cy-=19), buttonWidth/2, 19,
			             &mmd->flag, 0, 0, 0, 0,
			             "Mirror the U texture coordinate around "
			             "the 0.5 point");
			uiDefButBitS(block, TOG, MOD_MIR_MIRROR_V, B_MODIFIER_RECALC,
			             "Mirror V",
			             lx + buttonWidth/2 + 1, cy, buttonWidth/2, 19,
			             &mmd->flag, 0, 0, 0, 0,
			             "Mirror the V texture coordinate around "
			             "the 0.5 point");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
			               "Ob: ", lx, (cy -= 19), buttonWidth, 19,
			               &mmd->mirror_ob,
			               "Object to use as mirror");
		} else if (md->type==eModifierType_Bevel) {
			BevelModifierData *bmd = (BevelModifierData*) md;
			/*uiDefButS(block, ROW, B_MODIFIER_RECALC, "Distance",
					  lx, (cy -= 19), (buttonWidth/2), 19, &bmd->val_flags,
					  11.0, 0, 0, 0,
					  "Interpret bevel value as a constant distance from each edge");
			uiDefButS(block, ROW, B_MODIFIER_RECALC, "Radius",
					  (lx+buttonWidth/2), cy, (buttonWidth - buttonWidth/2), 19, &bmd->val_flags,
					  11.0, BME_BEVEL_RADIUS, 0, 0,
					  "Interpret bevel value as a radius - smaller angles will be beveled more");*/
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Width: ",
					  lx, (cy -= 19), buttonWidth, 19, &bmd->value,
					  0.0, 0.5, 5, 2,
					  "Bevel value/amount");
			/*uiDefButI(block, NUM, B_MODIFIER_RECALC, "Recurs",
					  lx, (cy -= 19), buttonWidth, 19, &bmd->res,
					  1, 4, 5, 2,
					  "Number of times to bevel");*/
			uiDefButBitS(block, TOG, BME_BEVEL_VERT,
					  B_MODIFIER_RECALC, "Only Vertices",
					  lx, (cy -= 19), buttonWidth, 19,
					  &bmd->flags, 0, 0, 0, 0,
					  "Bevel only verts/corners; not edges");
			uiBlockEndAlign(block);
					  
			uiDefBut(block, LABEL, 1, "Limit using:",	lx, (cy-=25), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
			uiBlockBeginAlign(block);
			uiDefButS(block, ROW, B_MODIFIER_RECALC, "None",
					  lx, (cy -= 19), (buttonWidth/3), 19, &bmd->lim_flags,
					  12.0, 0, 0, 0,
					  "Bevel the entire mesh by a constant amount");
			uiDefButS(block, ROW, B_MODIFIER_RECALC, "Angle",
					  (lx+buttonWidth/3), cy, (buttonWidth/3), 19, &bmd->lim_flags,
					  12.0, BME_BEVEL_ANGLE, 0, 0,
					  "Only bevel edges with sharp enough angles between faces");
			uiDefButS(block, ROW, B_MODIFIER_RECALC, "BevWeight",
					  lx+(2*buttonWidth/3), cy, buttonWidth-2*(buttonWidth/3), 19, &bmd->lim_flags,
					  12.0, BME_BEVEL_WEIGHT, 0, 0,
					  "Use bevel weights to determine how much bevel is applied; apply them separately in vert/edge select mode");
			if ((bmd->lim_flags & BME_BEVEL_WEIGHT) && !(bmd->flags & BME_BEVEL_VERT)) {
				uiDefButS(block, ROW, B_MODIFIER_RECALC, "Min",
					  lx, (cy -= 19), (buttonWidth/3), 19, &bmd->e_flags,
					  13.0, BME_BEVEL_EMIN, 0, 0,
					  "The sharpest edge's weight is used when weighting a vert");
				uiDefButS(block, ROW, B_MODIFIER_RECALC, "Average",
					  (lx+buttonWidth/3), cy, (buttonWidth/3), 19, &bmd->e_flags,
					  13.0, 0, 0, 0,
					  "The edge weights are averaged when weighting a vert");
				uiDefButS(block, ROW, B_MODIFIER_RECALC, "Max",
					  (lx+2*(buttonWidth/3)), cy, buttonWidth-2*(buttonWidth/3), 19, &bmd->e_flags,
					  13.0, BME_BEVEL_EMAX, 0, 0,
					  "The largest edge's wieght is used when weighting a vert");
			}
			else if (bmd->lim_flags & BME_BEVEL_ANGLE) {
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Angle:",
					  lx, (cy -= 19), buttonWidth, 19, &bmd->bevel_angle,
					  0.0, 180.0, 100, 2,
					  "Angle above which to bevel edges");
			}
		} else if (md->type==eModifierType_EdgeSplit) {
			EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;
			uiDefButBitI(block, TOG, MOD_EDGESPLIT_FROMANGLE,
			             B_MODIFIER_RECALC, "From Edge Angle",
			             lx, (cy -= 19), buttonWidth, 19,
			             &emd->flags, 0, 0, 0, 0,
			             "Split edges with high angle between faces");
			if(emd->flags & MOD_EDGESPLIT_FROMANGLE) {
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Split Angle:",
				          lx, (cy -= 19), buttonWidth, 19, &emd->split_angle,
				          0.0, 180.0, 100, 2,
				          "Angle above which to split edges");
			}
			uiDefButBitI(block, TOG, MOD_EDGESPLIT_FROMFLAG,
			             B_MODIFIER_RECALC, "From Marked As Sharp",
			             lx, (cy -= 19), buttonWidth, 19,
			             &emd->flags, 0, 0, 0, 0,
			             "Split edges that are marked as sharp");
		} else if (md->type==eModifierType_Displace) {
			DisplaceModifierData *dmd = (DisplaceModifierData*) md;
			but = uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",
			               lx, (cy -= 19), buttonWidth, 19,
			               &dmd->defgrp_name, 0.0, 31.0, 0, 0,
			               "Name of vertex group to displace"
			               " (displace whole mesh if blank)");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			uiDefIDPoinBut(block, modifier_testTexture, ID_TE, B_CHANGEDEP,
			               "Texture: ", lx, (cy -= 19), buttonWidth, 19,
			               &dmd->texture,
			               "Texture to use as displacement input");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Midlevel:",
			          lx, (cy -= 19), buttonWidth, 19, &dmd->midlevel,
			          0, 1, 10, 3,
			          "Material value that gives no displacement");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Strength:",
			          lx, (cy -= 19), buttonWidth, 19, &dmd->strength,
			          -1000, 1000, 10, 0.1,
			          "Strength of displacement");
			sprintf(str, "Direction%%t|Normal%%x%d|RGB -> XYZ%%x%d|"
			        "Z%%x%d|Y%%x%d|X%%x%d",
			        MOD_DISP_DIR_NOR, MOD_DISP_DIR_RGB_XYZ,
			        MOD_DISP_DIR_Z, MOD_DISP_DIR_Y, MOD_DISP_DIR_X);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			          lx, (cy -= 19), buttonWidth, 19, &dmd->direction,
			          0.0, 1.0, 0, 0, "Displace direction");
			sprintf(str, "Texture Coordinates%%t"
			        "|Local%%x%d|Global%%x%d|Object%%x%d|UV%%x%d",
			        MOD_DISP_MAP_LOCAL, MOD_DISP_MAP_GLOBAL,
			        MOD_DISP_MAP_OBJECT, MOD_DISP_MAP_UV);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			          lx, (cy -= 19), buttonWidth, 19, &dmd->texmapping,
			          0.0, 1.0, 0, 0,
			          "Texture coordinates used for displacement input");
			if (dmd->texmapping == MOD_DISP_MAP_UV) {
				char *strtmp;
				int i;
				CustomData *fdata = G.obedit ? &G.editMesh->fdata
				                             : &((Mesh*)ob->data)->fdata;
				build_uvlayer_menu_vars(fdata, &strtmp, &dmd->uvlayer_tmp,
				                        dmd->uvlayer_name);
				but = uiDefButI(block, MENU, B_MODIFIER_RECALC, strtmp,
				      lx, (cy -= 19), buttonWidth, 19, &dmd->uvlayer_tmp,
				      0.0, 1.0, 0, 0, "Set the UV layer to use");
				MEM_freeN(strtmp);
				i = CustomData_get_layer_index(fdata, CD_MTFACE);
				uiButSetFunc(but, set_displace_uvlayer, dmd,
				             &fdata->layers[i]);
			}
			if(dmd->texmapping == MOD_DISP_MAP_OBJECT) {
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
				               "Ob: ", lx, (cy -= 19), buttonWidth, 19,
				               &dmd->map_object,
				               "Object to get texture coordinates from");
			}
		} else if (md->type==eModifierType_UVProject) {
			UVProjectModifierData *umd = (UVProjectModifierData *) md;
			int i;
			char *strtmp;
			CustomData *fdata = G.obedit ? &G.editMesh->fdata
			                             : &((Mesh*)ob->data)->fdata;
			build_uvlayer_menu_vars(fdata, &strtmp, &umd->uvlayer_tmp,
			                        umd->uvlayer_name);
			but = uiDefButI(block, MENU, B_MODIFIER_RECALC, strtmp,
			      lx, (cy -= 19), buttonWidth, 19, &umd->uvlayer_tmp,
			      0.0, 1.0, 0, 0, "Set the UV layer to use");
			i = CustomData_get_layer_index(fdata, CD_MTFACE);
			uiButSetFunc(but, set_uvproject_uvlayer, umd, &fdata->layers[i]);
			MEM_freeN(strtmp);
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "AspX:",
			          lx, (cy -= 19), buttonWidth / 2, 19, &umd->aspectx,
			          1, 1000, 100, 2,
			          "Horizontal Aspect Ratio");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "AspY:",
			          lx + (buttonWidth / 2) + 1, cy, buttonWidth / 2, 19,
			          &umd->aspecty,
			          1, 1000, 100, 2,
			          "Vertical Aspect Ratio");
			uiDefButI(block, NUM, B_MODIFIER_RECALC, "Projectors:",
			          lx, (cy -= 19), buttonWidth, 19, &umd->num_projectors,
			          1, MOD_UVPROJECT_MAXPROJECTORS, 0, 0,
			          "Number of objects to use as projectors");
			for(i = 0; i < umd->num_projectors; ++i) {
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
				               "Ob: ", lx, (cy -= 19), buttonWidth, 19,
				               &umd->projectors[i],
				               "Object to use as projector");
			}
			uiDefIDPoinBut(block, modifier_testImage, ID_IM, B_CHANGEDEP,
			               "Image: ", lx, (cy -= 19), buttonWidth, 19,
			               &umd->image,
			               "Image to project (only faces with this image "
			               "will be altered");
			uiButSetCompleteFunc(but, autocomplete_image, (void *)ob);
			uiDefButBitI(block, TOG, MOD_UVPROJECT_OVERRIDEIMAGE,
			             B_MODIFIER_RECALC, "Override Image",
			             lx, (cy -= 19), buttonWidth, 19,
			             &umd->flags, 0, 0, 0, 0,
			             "Override faces' current images with the "
			             "given image");
		} else if (md->type==eModifierType_Decimate) {
			DecimateModifierData *dmd = (DecimateModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Ratio:",	lx,(cy-=19),buttonWidth,19, &dmd->percent, 0.0, 1.0, 10, 0, "Defines the percentage of triangles to reduce to");
			sprintf(str, "Face Count: %d", dmd->faceCount);
			uiDefBut(block, LABEL, 1, str,	lx, (cy-=19), 160,19, NULL, 0.0, 0.0, 0, 0, "Displays the current number of faces in the decimated mesh");
		} else if (md->type==eModifierType_Smooth) {
			SmoothModifierData *smd = (SmoothModifierData*) md;

			uiDefButBitS(block, TOG, MOD_SMOOTH_X, B_MODIFIER_RECALC, "X",		lx,(cy-=19),45,19, &smd->flag, 0, 0, 0, 0, "Enable X axis smoothing");
			uiDefButBitS(block, TOG, MOD_SMOOTH_Y, B_MODIFIER_RECALC, "Y",		lx+45,cy,45,19, &smd->flag, 0, 0, 0, 0, "Enable Y axis smoothing");
			uiDefButBitS(block, TOG, MOD_SMOOTH_Z, B_MODIFIER_RECALC, "Z",		lx+90,cy,45,19, &smd->flag, 0, 0, 0, 0, "Enable Z axis smoothing");

			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Factor:",	lx,(cy-=19),buttonWidth, 19, &smd->fac, -10.0, 10.0, 0.5, 0, "Define the amount of smoothing, from 0.0 to 1.0 (lower / higher values can deform the mesh)");
			uiDefButS(block, NUM, B_MODIFIER_RECALC, "Repeat:",	lx,(cy-=19),buttonWidth, 19, &smd->repeat, 0.0, 30.0, 1, 0, "Number of smoothing iterations");
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth,19, &smd->defgrp_name, 0.0, 31.0, 0, 0, "Vertex Group name to define which vertices are affected");
		} else if (md->type==eModifierType_Cast) {
			CastModifierData *cmd = (CastModifierData*) md;

			char casttypemenu[]="Projection Type%t|Sphere%x0|Cylinder%x1|Cuboid%x2";
			uiDefButS(block, MENU, B_MODIFIER_RECALC, casttypemenu,		lx,(cy-=19),buttonWidth - 30,19, &cmd->type, 0, 0, 0, 0, "Projection type to apply");
			uiDefButBitS(block, TOG, MOD_CAST_X, B_MODIFIER_RECALC, "X",		lx,(cy-=19),45,19, &cmd->flag, 0, 0, 0, 0, "Enable (local) X axis deformation");
			uiDefButBitS(block, TOG, MOD_CAST_Y, B_MODIFIER_RECALC, "Y",		lx+45,cy,45,19, &cmd->flag, 0, 0, 0, 0, "Enable (local) Y axis deformation");
			if (cmd->type != MOD_CAST_TYPE_CYLINDER) {
				uiDefButBitS(block, TOG, MOD_CAST_Z, B_MODIFIER_RECALC, "Z",		lx+90,cy,45,19, &cmd->flag, 0, 0, 0, 0, "Enable (local) Z axis deformation");
			}
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Factor:",	lx,(cy-=19),buttonWidth, 19, &cmd->fac, -10.0, 10.0, 5, 0, "Define the amount of deformation");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Radius:",	lx,(cy-=19),buttonWidth, 19, &cmd->radius, 0.0, 100.0, 10.0, 0, "Only deform vertices within this distance from the center of the effect (leave as 0 for infinite)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Size:",	lx,(cy-=19),buttonWidth, 19, &cmd->size, 0.0, 100.0, 10.0, 0, "Size of projection shape (leave as 0 for auto)");
			uiDefButBitS(block, TOG, MOD_CAST_SIZE_FROM_RADIUS, B_MODIFIER_RECALC, "From radius",		lx+buttonWidth,cy,80,19, &cmd->flag, 0, 0, 0, 0, "Use radius as size of projection shape (0 = auto)");
			if (ob->type == OB_MESH) {
				but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth,19, &cmd->defgrp_name, 0.0, 31.0, 0, 0, "Vertex Group name to define which vertices are affected");
			}
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP, "Ob: ", lx,(cy-=19), buttonWidth,19, &cmd->object, "Control object: if available, its location determines the center of the effect");
			if(cmd->object) {
				uiDefButBitS(block, TOG, MOD_CAST_USE_OB_TRANSFORM, B_MODIFIER_RECALC, "Use transform",		lx+buttonWidth,cy,80,19, &cmd->flag, 0, 0, 0, 0, "Use object transform to control projection shape");
			}
		} else if (md->type==eModifierType_Wave) {
			WaveModifierData *wmd = (WaveModifierData*) md;
			uiDefButBitS(block, TOG, MOD_WAVE_X, B_MODIFIER_RECALC, "X",		lx,(cy-=19),45,19, &wmd->flag, 0, 0, 0, 0, "Enable X axis motion");
			uiDefButBitS(block, TOG, MOD_WAVE_Y, B_MODIFIER_RECALC, "Y",		lx+45,cy,45,19, &wmd->flag, 0, 0, 0, 0, "Enable Y axis motion");
			uiDefButBitS(block, TOG, MOD_WAVE_CYCL, B_MODIFIER_RECALC, "Cycl",	lx+90,cy,buttonWidth-90,19, &wmd->flag, 0, 0, 0, 0, "Enable cyclic wave effect");
			uiDefButBitS(block, TOG, MOD_WAVE_NORM, B_MODIFIER_RECALC, "Normals",	lx,(cy-=19),buttonWidth,19, &wmd->flag, 0, 0, 0, 0, "Displace along normals");
			if (wmd->flag & MOD_WAVE_NORM){
				if (ob->type==OB_MESH) {
					uiDefButBitS(block, TOG, MOD_WAVE_NORM_X, B_MODIFIER_RECALC, "X",	lx,(cy-=19),buttonWidth/3,19, &wmd->flag, 0, 0, 0, 0, "Enable displacement along the X normal");
					uiDefButBitS(block, TOG, MOD_WAVE_NORM_Y, B_MODIFIER_RECALC, "Y",	lx+(buttonWidth/3),cy,buttonWidth/3,19, &wmd->flag, 0, 0, 0, 0, "Enable displacement along the Y normal");
					uiDefButBitS(block, TOG, MOD_WAVE_NORM_Z, B_MODIFIER_RECALC, "Z",	lx+(buttonWidth/3)*2,cy,buttonWidth/3,19, &wmd->flag, 0, 0, 0, 0, "Enable displacement along the Z normal");
				}
				else
					uiDefBut(block, LABEL, 1, "Meshes Only",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");				
			}

			uiBlockBeginAlign(block);
			if(wmd->speed >= 0)
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Time sta:",	lx,(cy-=19),buttonWidth,19, &wmd->timeoffs, -MAXFRAMEF, MAXFRAMEF, 100, 0, "Specify starting frame of the wave");
			else
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Time end:",	lx,(cy-=19),buttonWidth,19, &wmd->timeoffs, -MAXFRAMEF, MAXFRAMEF, 100, 0, "Specify ending frame of the wave");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Lifetime:",	lx,(cy-=19),buttonWidth,19, &wmd->lifetime,  -MAXFRAMEF, MAXFRAMEF, 100, 0, "Specify the lifespan of the wave");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Damptime:",	lx,(cy-=19),buttonWidth,19, &wmd->damp,  -MAXFRAMEF, MAXFRAMEF, 100, 0, "Specify the dampingtime of the wave");
			cy -= 9;
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Sta x:",		lx,(cy-=19),113,19, &wmd->startx, -100.0, 100.0, 100, 0, "Starting position for the X axis");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Sta y:",		lx+115,cy,105,19, &wmd->starty, -100.0, 100.0, 100, 0, "Starting position for the Y axis");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_MODIFIER_RECALC, "Ob: ", lx, (cy-=19), 220,19, &wmd->objectcenter, "Object to use as Starting Position (leave blank to disable)");
			uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",lx, (cy -= 19), 220, 19,&wmd->defgrp_name, 0.0, 31.0, 0, 0, "Name of vertex group with which to modulate displacement");
			uiDefIDPoinBut(block, modifier_testTexture, ID_TE, B_CHANGEDEP,"Texture: ", lx, (cy -= 19), 220, 19, &wmd->texture,"Texture with which to modulate wave");
			sprintf(str, "Texture Coordinates%%t"
			        "|Local%%x%d|Global%%x%d|Object%%x%d|UV%%x%d",
			        MOD_WAV_MAP_LOCAL, MOD_WAV_MAP_GLOBAL,
			        MOD_WAV_MAP_OBJECT, MOD_WAV_MAP_UV);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			          lx, (cy -= 19), 220, 19, &wmd->texmapping,
			          0.0, 1.0, 0, 0,
			          "Texture coordinates used for modulation input");
			if (wmd->texmapping == MOD_WAV_MAP_UV) {
				char *strtmp;
				int i;
				CustomData *fdata = G.obedit ? &G.editMesh->fdata
				                             : &((Mesh*)ob->data)->fdata;
				build_uvlayer_menu_vars(fdata, &strtmp, &wmd->uvlayer_tmp,
				                        wmd->uvlayer_name);
				but = uiDefButI(block, MENU, B_MODIFIER_RECALC, strtmp,
				      lx, (cy -= 19), 220, 19, &wmd->uvlayer_tmp,
				      0.0, 1.0, 0, 0, "Set the UV layer to use");
				MEM_freeN(strtmp);
				i = CustomData_get_layer_index(fdata, CD_MTFACE);
				uiButSetFunc(but, set_displace_uvlayer, wmd,
				             &fdata->layers[i]);
			}
			if(wmd->texmapping == MOD_DISP_MAP_OBJECT) {
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
				               "Ob: ", lx, (cy -= 19), 220, 19,
				               &wmd->map_object,
				               "Object to get texture coordinates from");
			}
            cy -= 9;
			uiBlockBeginAlign(block);
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Speed:",	lx,(cy-=19),220,19, &wmd->speed, -2.0, 2.0, 0, 0, "Specify the wave speed");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Height:",	lx,(cy-=19),220,19, &wmd->height, -2.0, 2.0, 0, 0, "Specify the amplitude of the wave");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Width:",	lx,(cy-=19),220,19, &wmd->width, 0.0, 5.0, 0, 0, "Specify the width of the wave");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Narrow:",	lx,(cy-=19),220,19, &wmd->narrow, 0.0, 10.0, 0, 0, "Specify how narrow the wave follows");
		} else if (md->type==eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData*) md;
			uiDefIDPoinBut(block, modifier_testArmatureObj, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &amd->object, "Armature object to deform with");
			
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth-40,19, &amd->defgrp_name, 0.0, 31.0, 0, 0, "Vertex Group name to control overall armature influence");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			uiDefButBitS(block, TOG, ARM_DEF_INVERT_VGROUP, B_ARM_RECALCDATA, "Inv",	lx+buttonWidth-40,cy, 40, 20, &amd->deformflag, 0, 0, 0, 0, "Invert vertex group influence");
			
			uiDefButBitS(block, TOG, ARM_DEF_VGROUP, B_ARM_RECALCDATA, "Vert.Groups",	lx,cy-=19,buttonWidth/2,20, &amd->deformflag, 0, 0, 0, 0, "Enable VertexGroups defining deform");
			uiDefButBitS(block, TOG, ARM_DEF_ENVELOPE, B_ARM_RECALCDATA, "Envelopes",	lx+buttonWidth/2,cy,(buttonWidth + 1)/2,20, &amd->deformflag, 0, 0, 0, 0, "Enable Bone Envelopes defining deform");
			uiDefButBitS(block, TOG, ARM_DEF_QUATERNION, B_ARM_RECALCDATA, "Quaternion",	lx,(cy-=19),buttonWidth/2,20, &amd->deformflag, 0, 0, 0, 0, "Enable deform rotation interpolation with Quaternions");
			uiDefButBitS(block, TOG, ARM_DEF_B_BONE_REST, B_ARM_RECALCDATA, "B-Bone Rest", lx+buttonWidth/2,cy,(buttonWidth + 1)/2,20, &amd->deformflag, 0, 0, 0, 0, "Make B-Bones deform already in rest position");
			
			uiDefButS(block, TOG, B_ARM_RECALCDATA, "MultiModifier",	lx,cy-=19, buttonWidth, 20, &amd->multi, 0, 0, 0, 0, "Use same input as previous modifier, and mix results using overall vgroup");

		} else if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Falloff: ",		lx, (cy-=19), buttonWidth,19, &hmd->falloff, 0.0, 100.0, 100, 0, "If not zero, the distance from hook where influence ends");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Force: ",		lx, (cy-=19), buttonWidth,19, &hmd->force, 0.0, 1.0, 100, 0, "Set relative force of hook");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &hmd->object, "Parent Object for hook, also recalculates and clears offset"); 
			if(hmd->indexar==NULL) {
				but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",		lx, (cy-=19), buttonWidth,19, &hmd->name, 0.0, 31.0, 0, 0, "Vertex Group name");
				uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			}
			uiBlockBeginAlign(block);
			but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Reset", 		lx, (cy-=19), 80,19,			NULL, 0.0, 0.0, 0, 0, "Recalculate and clear offset (transform) of hook");
			uiButSetFunc(but, modifiers_clearHookOffset, ob, md);
			but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Recenter", 	lx+80, cy, buttonWidth-80,19,	NULL, 0.0, 0.0, 0, 0, "Sets hook center to cursor position");
			uiButSetFunc(but, modifiers_cursorHookCenter, ob, md);

			if (editing) {
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Select", 		lx, (cy-=19), 80,19, NULL, 0.0, 0.0, 0, 0, "Selects effected vertices on mesh");
				uiButSetFunc(but, modifiers_selectHook, ob, md);
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Reassign", 		lx+80, cy, buttonWidth-80,19, NULL, 0.0, 0.0, 0, 0, "Reassigns selected vertices to hook");
				uiButSetFunc(but, modifiers_reassignHook, ob, md);
			}
		} else if (md->type==eModifierType_Softbody) {
			uiDefBut(block, LABEL, 1, "See Soft Body panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_Cloth) {
			uiDefBut(block, LABEL, 1, "See Cloth panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_Collision) {
			uiDefBut(block, LABEL, 1, "See Collision panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_Boolean) {
			BooleanModifierData *bmd = (BooleanModifierData*) md;
			uiDefButI(block, MENU, B_MODIFIER_RECALC, "Operation%t|Intersect%x0|Union%x1|Difference%x2",	lx,(cy-=19),buttonWidth,19, &bmd->operation, 0.0, 1.0, 0, 0, "Boolean operation to perform");
			uiDefIDPoinBut(block, modifier_testMeshObj, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &bmd->object, "Mesh object to use for boolean operation");
		} else if (md->type==eModifierType_Array) {
			ArrayModifierData *amd = (ArrayModifierData*) md;
			float range = 10000;
			int cytop, halfwidth = (width - 5)/2 - 15;
			int halflx = lx + halfwidth + 10;

			uiBlockSetEmboss(block, UI_EMBOSSX);
			uiBlockEndAlign(block);

			/* length parameters */
			uiBlockBeginAlign(block);
			sprintf(str, "Length Fit%%t|Fixed Count%%x%d|Fixed Length%%x%d"
			        "|Fit To Curve Length%%x%d",
			        MOD_ARR_FIXEDCOUNT, MOD_ARR_FITLENGTH, MOD_ARR_FITCURVE);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			          lx, (cy-=19), buttonWidth, 19, &amd->fit_type,
			          0.0, 1.0, 0, 0, "Array length calculation method");
			switch(amd->fit_type)
			{
			case MOD_ARR_FIXEDCOUNT:
				uiDefButI(block, NUM, B_MODIFIER_RECALC, "Count:",
				          lx, (cy -= 19), buttonWidth, 19, &amd->count,
				          1, 1000, 0, 0, "Number of duplicates to make");
				break;
			case MOD_ARR_FITLENGTH:
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Length:",
				          lx, (cy -= 19), buttonWidth, 19, &amd->length,
				          0, range, 10, 2,
				          "Length to fit array within");
				break;
			case MOD_ARR_FITCURVE:
				uiDefIDPoinBut(block, modifier_testCurveObj, ID_OB,
				               B_CHANGEDEP, "Ob: ",
				               lx, (cy -= 19), buttonWidth, 19, &amd->curve_ob,
				               "Curve object to fit array length to");
				break;
			}
			uiBlockEndAlign(block);

			/* offset parameters */
			cy -= 10;
			cytop= cy;
			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, MOD_ARR_OFF_CONST, B_MODIFIER_RECALC,
			             "Constant Offset", lx, (cy-=19), halfwidth, 19,
			             &amd->offset_type, 0, 0, 0, 0,
			             "Constant offset between duplicates "
			             "(local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "X:",
			          lx, (cy-=19), halfwidth, 19,
			          &amd->offset[0],
			          -range, range, 10, 3,
			          "Constant component for duplicate offsets "
			          "(local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Y:",
			          lx, (cy-=19), halfwidth, 19,
			          &amd->offset[1],
			          -range, range, 10, 3,
			          "Constant component for duplicate offsets "
			          "(local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Z:",
			          lx, (cy-=19), halfwidth, 19,
			          &amd->offset[2],
			          -range, range, 10, 3,
			          "Constant component for duplicate offsets "
			          "(local coordinates)");
			uiBlockEndAlign(block);

			cy= cytop;
			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, MOD_ARR_OFF_RELATIVE, B_MODIFIER_RECALC,
			             "Relative Offset", halflx, (cy-=19), halfwidth, 19,
			             &amd->offset_type, 0, 0, 0, 0,
			             "Offset between duplicates relative to object width "
			             "(local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "X:",
			          halflx, (cy-=19), halfwidth, 19,
			          &amd->scale[0],
			          -range, range, 10, 3,
			          "Component for duplicate offsets relative to object "
			          "width (local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Y:",
			          halflx, (cy-=19), halfwidth, 19,
			          &amd->scale[1],
			          -range, range, 10, 3,
			          "Component for duplicate offsets relative to object "
			          "width (local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Z:",
			          halflx, (cy-=19), halfwidth, 19,
			          &amd->scale[2],
			          -range, range, 10, 3,
			          "Component for duplicate offsets relative to object "
			          "width (local coordinates)");
			uiBlockEndAlign(block);

			/* vertex merging parameters */
			cy -= 10;
			cytop= cy;

			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, MOD_ARR_MERGE, B_MODIFIER_RECALC,
			             "Merge",
			             lx, (cy-=19), halfwidth/2, 19, &amd->flags,
			             0, 0, 0, 0,
			             "Merge vertices in adjacent duplicates");
			uiDefButBitI(block, TOG, MOD_ARR_MERGEFINAL, B_MODIFIER_RECALC,
			             "First Last",
			             lx + halfwidth/2, cy, (halfwidth+1)/2, 19,
			             &amd->flags,
			             0, 0, 0, 0,
			             "Merge vertices in first duplicate with vertices"
			             " in last duplicate");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Limit:",
					  lx, (cy-=19), halfwidth, 19, &amd->merge_dist,
					  0, 1.0f, 1, 4,
					  "Limit below which to merge vertices");

			/* offset ob */
			cy = cytop;
			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, MOD_ARR_OFF_OBJ, B_MODIFIER_RECALC,
			             "Object Offset", halflx, (cy -= 19), halfwidth, 19,
			             &amd->offset_type, 0, 0, 0, 0,
			             "Add an object transformation to the total offset");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
			               "Ob: ", halflx, (cy -= 19), halfwidth, 19,
			               &amd->offset_ob,
			               "Object from which to take offset transformation");
			uiBlockEndAlign(block);

			cy -= 10;
			but = uiDefIDPoinBut(block, test_meshobpoin_but, ID_OB,
			                     B_CHANGEDEP, "Start cap: ",
			                     lx, (cy -= 19), halfwidth, 19,
			                     &amd->start_cap,
			                     "Mesh object to use as start cap");
			uiButSetCompleteFunc(but, autocomplete_meshob, (void *)ob);
			but = uiDefIDPoinBut(block, test_meshobpoin_but, ID_OB,
			                     B_CHANGEDEP, "End cap: ",
			                     halflx, cy, halfwidth, 19,
			                     &amd->end_cap,
			                     "Mesh object to use as end cap");
			uiButSetCompleteFunc(but, autocomplete_meshob, (void *)ob);
		} else if (md->type==eModifierType_MeshDeform) {
			MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

			uiBlockBeginAlign(block);
			uiDefIDPoinBut(block, test_meshobpoin_but, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &mmd->object, "Mesh object to be use as cage"); 
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-19), buttonWidth-40,19, &mmd->defgrp_name, 0.0, 31.0, 0, 0, "Vertex Group name to control overall meshdeform influence");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			uiDefButBitS(block, TOG, MOD_MDEF_INVERT_VGROUP, B_MODIFIER_RECALC, "Inv", lx+buttonWidth-40, (cy-=19), 40,19, &mmd->flag, 0.0, 31.0, 0, 0, "Invert vertex group influence");

			uiBlockBeginAlign(block);
			if(mmd->bindcos) {
				but= uiDefBut(block, BUT, B_MODIFIER_RECALC, "Unbind", lx,(cy-=24), buttonWidth,19, 0, 0, 0, 0, 0, "Unbind mesh from cage");
				uiButSetFunc(but,modifiers_bindMeshDeform,ob,md);
			}
			else {
				but= uiDefBut(block, BUT, B_MODIFIER_RECALC, "Bind", lx,(cy-=24), buttonWidth,19, 0, 0, 0, 0, 0, "Bind mesh to cage");
				uiButSetFunc(but,modifiers_bindMeshDeform,ob,md);
				uiDefButS(block, NUM, B_NOP, "Precision:", lx,(cy-19), buttonWidth/2 + 20,19, &mmd->gridsize, 2, 10, 0.5, 0, "The grid size for binding");
				uiDefButBitS(block, TOG, MOD_MDEF_DYNAMIC_BIND, B_MODIFIER_RECALC, "Dynamic", lx+(buttonWidth+1)/2 + 20, (cy-=19), buttonWidth/2 - 20,19, &mmd->flag, 0.0, 31.0, 0, 0, "Invert vertex group influence");
			}
			uiBlockEndAlign(block);
		} else if (md->type==eModifierType_ParticleSystem) {
			uiDefBut(block, LABEL, 1, "See Particle buttons.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_ParticleInstance) {
			ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData*) md;
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy -= 19), buttonWidth, 19, &pimd->ob, "Object that has the particlesystem");
			uiDefButS(block, NUM, B_MODIFIER_RECALC, "PSYS:", lx, (cy -= 19), buttonWidth, 19, &pimd->psys, 1, 10, 10, 3, "Particlesystem number in the object");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Parents, B_MODIFIER_RECALC, "Normal",	lx, (cy -= 19), buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Create instances from normal particles");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Children, B_MODIFIER_RECALC, "Children",	lx+buttonWidth/3, cy, buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Create instances from child particles");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Path, B_MODIFIER_RECALC, "Path",	lx+buttonWidth*2/3, cy, buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Create instances along particle paths");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Unborn, B_MODIFIER_RECALC, "Unborn",	lx, (cy -= 19), buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Show instances when particles are unborn");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Alive, B_MODIFIER_RECALC, "Alive",	lx+buttonWidth/3, cy, buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Show instances when particles are alive");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Dead, B_MODIFIER_RECALC, "Dead",	lx+buttonWidth*2/3, cy, buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Show instances when particles are dead");
		} else if (md->type==eModifierType_Explode) {
			ExplodeModifierData *emd = (ExplodeModifierData*) md;
			uiBut *but;
			char *menustr= get_vertexgroup_menustr(ob);
			int defCount=BLI_countlist(&ob->defbase);
			if(defCount==0) emd->vgroup=0;

			but=uiDefButS(block, MENU, B_MODIFIER_RECALC, menustr,	lx, (cy-=19), buttonWidth/2,19, &emd->vgroup, 0, defCount, 0, 0, "Protect this vertex group");
			uiButSetFunc(but,modifiers_explodeFacepa,emd,0);
			MEM_freeN(menustr);

			but=uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "",	lx+buttonWidth/2, cy, buttonWidth/2,19, &emd->protect, 0.0f, 1.0f, 0, 0, "Clean vertex group edges");
			uiButSetFunc(but,modifiers_explodeFacepa,emd,0);

			but=uiDefBut(block, BUT, B_MODIFIER_RECALC, "Refresh",	lx, (cy-=19), buttonWidth/2,19, 0, 0, 0, 0, 0, "Recalculate faces assigned to particles");
			uiButSetFunc(but,modifiers_explodeFacepa,emd,0);

			uiDefButBitS(block, TOG, eExplodeFlag_EdgeSplit, B_MODIFIER_RECALC, "Split Edges",	lx+buttonWidth/2, cy, buttonWidth/2,19, &emd->flag, 0, 0, 0, 0, "Split face edges for nicer shrapnel");
			uiDefButBitS(block, TOG, eExplodeFlag_Unborn, B_MODIFIER_RECALC, "Unborn",	lx, (cy-=19), buttonWidth/3,19, &emd->flag, 0, 0, 0, 0, "Show mesh when particles are unborn");
			uiDefButBitS(block, TOG, eExplodeFlag_Alive, B_MODIFIER_RECALC, "Alive",	lx+buttonWidth/3, cy, buttonWidth/3,19, &emd->flag, 0, 0, 0, 0, "Show mesh when particles are alive");
			uiDefButBitS(block, TOG, eExplodeFlag_Dead, B_MODIFIER_RECALC, "Dead",	lx+buttonWidth*2/3, cy, buttonWidth/3,19, &emd->flag, 0, 0, 0, 0, "Show mesh when particles are dead");
		}

		uiBlockEndAlign(block);

		y-=height;
	}

	if (md->error) {
		y -= 6;

		uiBlockSetCol(block, color);
					/* roundbox 4 free variables: corner-rounding, nop, roundbox type, shade */
		uiDefBut(block, ROUNDBOX, 0, "", x-10, y, width, 20, NULL, 5.0, 0.0, 15, 40, ""); 
		uiBlockSetCol(block, TH_AUTO);

		uiDefIconBut(block,LABEL,B_NOP,ICON_ERROR, x-9, y,19,19, 0,0,0,0,0, "");
		uiDefBut(block, LABEL, B_NOP, md->error, x+5, y, width-15, 19, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

		y -= 18;
	}

	uiClearButLock();

	y -= 3+6;

	*xco = x;
	*yco = y;
}

static void editing_panel_modifiers(Object *ob)
{
	ModifierData *md;
	uiBlock *block;
	char str[64];
	int xco, yco, i, lastCageIndex, cageIndex = modifiers_getCageIndex(ob, &lastCageIndex);

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_modifiers", UI_EMBOSS, UI_HELV, curarea->win);
	if( uiNewPanel(curarea, block, "Modifiers", "Editing", 640, 0, 318, 204)==0) return;
	
	uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	uiNewPanelHeight(block, 204);

	uiDefBlockBut(block, modifiers_add_menu, ob, "Add Modifier", 0, 190, 130, 20, "Add a new modifier");

	sprintf(str, "To: %s", ob->id.name+2);
	uiDefBut(block, LABEL, 1, str,	140, 190, 160, 20, NULL, 0.0, 0.0, 0, 0, "Object whose modifier stack is being edited");

	xco = 0;
	yco = 160;

	md = modifiers_getVirtualModifierList(ob);

	for (i=0; md; i++, md=md->next) {
		draw_modifier(block, ob, md, &xco, &yco, i, cageIndex, lastCageIndex);
		if (md->mode&eModifierMode_Virtual) i--;
	}
	
	if(yco < 0) uiNewPanelHeight(block, 204-yco);
}

static char *make_key_menu(Key *key, int startindex)
{
	KeyBlock *kb;
	int index= 1;
	char *str, item[64];

	for (kb = key->block.first; kb; kb=kb->next, index++);
	str= MEM_mallocN(index*40, "key string");
	str[0]= 0;
	
	index= startindex;
	for (kb = key->block.first; kb; kb=kb->next, index++) {
		sprintf (item,  "|%s%%x%d", kb->name, index);
		strcat(str, item);
	}
	
	return str;
}

static void editing_panel_shapes(Object *ob)
{
	uiBlock *block;
	Key *key= NULL;
	KeyBlock *kb;
	int icon;
	char *strp;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_shapes", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Modifiers", "Editing");
	if( uiNewPanel(curarea, block, "Shapes", "Editing", 640, 0, 318, 204)==0) return;
	
	/* Todo check data is library here */
	uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	uiDefBut(block, BUT, B_ADDKEY, "Add Shape Key" ,	10, 180, 150, 20, NULL, 0.0, 0.0, 0, 0, "Add new Shape Key");
	
	key= ob_get_key(ob);
	if(key==NULL) {
		/* label aligns add button */
		uiDefBut(block, LABEL, 0, "",		170, 180,140,20, NULL, 0, 0, 0, 0, "");
		return;
	}
	
	uiDefButS(block, TOG, B_RELKEY, "Relative",		170, 180,140,20, &key->type, 0, 0, 0, 0, "Makes Shape Keys relative");

	kb= BLI_findlink(&key->block, ob->shapenr-1);
	if(kb==NULL) {
		ob->shapenr= 1;
		kb= key->block.first;
	}

	uiBlockBeginAlign(block);
	if(ob->shapeflag & OB_SHAPE_LOCK) icon= ICON_PIN_HLT; else icon= ICON_PIN_DEHLT;
	uiDefIconButBitS(block, TOG, OB_SHAPE_LOCK, B_LOCKKEY, icon, 10,150,25,20, &ob->shapeflag, 0, 0, 0, 0, "Always show the current Shape for this Object");
	if(kb->flag & KEYBLOCK_MUTE) icon= ICON_MUTE_IPO_ON; else icon = ICON_MUTE_IPO_OFF;
	uiDefIconButBitS(block, TOG, KEYBLOCK_MUTE, B_MODIFIER_RECALC, icon, 35,150,20,20, &kb->flag, 0, 0, 0, 0, "Mute the current Shape");
	uiSetButLock(G.obedit==ob, "Unable to perform in EditMode");
	uiDefIconBut(block, BUT, B_PREVKEY, ICON_TRIA_LEFT,		55,150,20,20, NULL, 0, 0, 0, 0, "Previous Shape Key");
	strp= make_key_menu(key, 1);
	uiDefButS(block, MENU, B_SETKEY, strp,					75,150,20,20, &ob->shapenr, 0, 0, 0, 0, "Browse existing choices");
	MEM_freeN(strp);
	
	uiDefIconBut(block, BUT, B_NEXTKEY, ICON_TRIA_RIGHT,	95,150,20,20, NULL, 0, 0, 0, 0, "Next Shape Key");
	uiClearButLock();
	uiDefBut(block, TEX, B_NAMEKEY, "",						115, 150, 170, 20, kb->name, 0.0, 31.0, 0, 0, "Current Shape Key name");
	uiDefIconBut(block, BUT, B_DELKEY, ICON_X,				285,150,25,20, 0, 0, 0, 0, 0, "Deletes current Shape Key");
	uiBlockEndAlign(block);

	if(key->type && (ob->shapeflag & OB_SHAPE_LOCK)==0 && ob->shapenr!=1) {
		uiBlockBeginAlign(block);
		make_rvk_slider(block, ob, ob->shapenr-1,			10, 120, 150, 20, "Key value, when used it inserts an animation curve point");
		uiDefButF(block, NUM, B_REDR, "Min ",				160,120, 75, 20, &kb->slidermin, -10.0, 10.0, 100, 1, "Minumum for slider");
		uiDefButF(block, NUM, B_REDR, "Max ",				235,120, 75, 20, &kb->slidermax, -10.0, 10.0, 100, 1, "Maximum for slider");
		uiBlockEndAlign(block);
	}
	if(key->type && ob->shapenr!=1) {
		uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",	10, 90, 150,19, &kb->vgroup, 0.0, 31.0, 0, 0, "Vertex Weight Group name, to blend with Basis Shape");

		strp= make_key_menu(key, 0);
		uiDefButS(block, MENU, B_MODIFIER_RECALC, strp,		160, 90, 150,19, &kb->relative, 0.0, 0.0, 0, 0, "Shape used as a relative key");
		MEM_freeN(strp);
	}
	
	if(key->type==0)
		uiDefButS(block, NUM, B_DIFF, "Slurph:",			10, 60, 150, 19, &(key->slurph), -500.0, 500.0, 0, 0, "Creates a delay in amount of frames in applying keypositions, first vertex goes first");
	
}

/* *************************** FONT ******************************** */

static short give_vfontnr(VFont *vfont)
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

static VFont *give_vfontpointer(int nr)	/* nr= button */
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

static void load_buts_vfont(char *name)
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
	
	switch(cu->curinfo.flag & CU_STYLE) {
		case CU_BOLD:
			if(cu->vfontb) cu->vfontb->id.us--;
			cu->vfontb= vf;
			break;
		case CU_ITALIC:
			if(cu->vfonti) cu->vfonti->id.us--;		
			cu->vfonti= vf;
			break;						
		case (CU_BOLD|CU_ITALIC):
			if(cu->vfontbi) cu->vfontbi->id.us--;
			cu->vfontbi= vf;
			break;
		default:
			if(cu->vfont) cu->vfont->id.us--;
			cu->vfont= vf;
			break;						
	}	

	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	BIF_undo_push("Load vector font");
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

static void set_unicode_text_fs(char *file)
{
	if (file) paste_unicodeText(file); 
}

void do_fontbuts(unsigned short event)
{
	Curve *cu;
	VFont *vf;
	Object *ob;
	ScrArea *sa;
	char str[80];
	int ctevt;
	char *ctmenu;
	DynStr *ds;
	int i, style=0;

	ob= OBACT;

	switch(event) {
	case B_MAKEFONT:
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		break;

	case B_STYLETOSELU:	
	case B_STYLETOSELB:
	case B_STYLETOSELI:
		switch (event) {
			case B_STYLETOSELU: style = CU_UNDERLINE; break;
			case B_STYLETOSELB: style = CU_BOLD; break;			
			case B_STYLETOSELI: style = CU_ITALIC; break;
		}
		if (style_to_sel(style, ((Curve*)ob->data)->curinfo.flag & style)) {
			text_to_curve(ob, 0);
			makeDispListCurveTypes(ob, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		allqueue(REDRAWBUTSEDIT, 0);
		break;		
		
	case B_FASTFONT:
		if (G.obedit) {
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_INSTB:
		cu= ob->data;
		if (cu->totbox < 256) {
			for (i = cu->totbox; i>cu->actbox; i--) cu->tb[i]= cu->tb[i-1];
			cu->tb[cu->actbox]= cu->tb[cu->actbox-1];
			cu->actbox++;
			cu->totbox++;
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
			text_to_curve(ob, 0);
			makeDispListCurveTypes(ob, 0);
		}
		else {
			error("Do you really need that many text frames?");
		}
		break;
	case B_DELTB:
		cu= ob->data;
		if (cu->totbox > 1) {
			for (i = cu->actbox-1; i < cu->totbox; i++) cu->tb[i]= cu->tb[i+1];
			cu->totbox--;
			cu->actbox--;
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
			text_to_curve(ob, 0);
			makeDispListCurveTypes(ob, 0);
		}
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

		activate_fileselect(FILE_LOADFONT, "SELECT FONT", str, load_buts_vfont);

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
							DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
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

	case B_LOAD3DTEXT:
		if (!G.obedit) { error("Only in editmode!"); return; }
		if (G.obedit->type != OB_FONT) return;	
		activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, load_3dtext_fs);
		break;
		
	case B_LOREM:
		if (!G.obedit) { error("Only in editmode!"); return; }
		if (G.obedit->type != OB_FONT) return;	
		add_lorem();
		
		break;		

	case B_SETFONT:
		if(ob) {
			cu= ob->data;

			vf= give_vfontpointer(G.buts->texnr);
			if(vf) {
				id_us_plus((ID *)vf);

				switch(cu->curinfo.flag & CU_STYLE) {
					case CU_BOLD:
						cu->vfontb->id.us--;
						cu->vfontb= vf;
						break;
					case CU_ITALIC:
						cu->vfonti->id.us--;
						cu->vfonti= vf;
						break;						
					case (CU_BOLD|CU_ITALIC):
						cu->vfontbi->id.us--;
						cu->vfontbi= vf;
						break;
					default:
						cu->vfont->id.us--;
						cu->vfont= vf;
						break;						
				}
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

				BIF_undo_push("Set vector font");
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWBUTSEDIT, 0);
			}
		}
		break;
		
	case B_SETCHAR:
		G.charmin = 0x0000;
		G.charmax = 0xffff;
		if(G.charstart < 0)
			G.charstart = 0;	
		if(G.charstart > (0xffff - 12*6))
			G.charstart = 0xffff - (12*6);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
		
	case B_SETUPCHAR:
		G.charstart = G.charstart - (12*6);
		if(G.charstart < 0)
			G.charstart = 0;	
		if(G.charstart < G.charmin)
			G.charstart = G.charmin;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
		
	case B_SETCAT:
		// Create new dynamic string
		ds = BLI_dynstr_new();
		
		// Fill the dynamic string with entries
		for(i=0;i<104;i++)
		{
			BLI_dynstr_append(ds, "|");
			BLI_dynstr_append(ds, uctabname[i].name);
		}
		
		// Create the menu string from dyn string
		ctmenu = BLI_dynstr_get_cstring(ds);
		
		// Call the popup menu
		ctevt = pupmenu_col(ctmenu, 40);
		G.charstart = uctabname[ctevt-1].start;
		G.charmin = uctabname[ctevt-1].start;
		G.charmax = uctabname[ctevt-1].end;

		// Free all data
		BLI_dynstr_free(ds);
		MEM_freeN(ctmenu);

		// And refresh
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		
		break;	
		
	case B_SETDOWNCHAR:
		G.charstart = G.charstart + (12*6);
		if(G.charstart > (0xffff - 12*6))
			G.charstart = 0xffff - (12*6);
		if(G.charstart > G.charmax - 12*6)
			G.charstart = G.charmax - 12*6;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
		
	case B_SETUNITEXT:
		sa= closest_bigger_area();
		areawinset(sa->win);

		if(ob==G.obedit) {
			activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, set_unicode_text_fs);
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
			DAG_scene_sort(G.scene); // makes new dag
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

#ifdef INTERNATIONAL
static void editing_panel_char_type(Object *ob, Curve *cu)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_char_type", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Font", "Editing");
	if(uiNewPanel(curarea, block, "Char", "Editing", 640, 0, 318, 204)==0) 
		return;

	// Set the selected font
	G.selfont = cu->vfont;
	
	uiDefIconBut(block, BUT, B_SETUNITEXT, ICON_TEXT,	0,210,20,20, 0, 0, 0, 0, 0, "Load Unicode Text file");

	// Unicode categorization selection button
	uiDefBut(block, BUT, B_SETCAT, "Unicode Table", 22,210,226,20, 0, 0, 0, 0, 0, "Select Unicode Table");
	uiDefButI(block, NUM, /*B_SETUPCHAR*/ 0, "", 250,210,50,20, &G.charstart, 0, 0xffff, 0, 0, "UT");

	// Character selection button
	uiDefBut(block, CHARTAB, B_SETCHAR, "", 0, 0, 264, 200, 0, 0, 0, 0, 0, "Select character");

	// Buttons to change the max, min
	uiDefButI(block, BUT, B_SETUPCHAR, "U", 280, 185, 15, 15, &G.charstart, 0, 0xffff, 0, 0, "Scroll character table up");
	uiDefButI(block, BUT, B_SETDOWNCHAR, "D", 280, 0, 15, 15, &G.charstart, 0, 0xffff, 0, 0, "Scroll character table down");
}
#endif

static void editing_panel_font_type(Object *ob, Curve *cu)
{
	uiBlock *block;
	char *strp;
	static int packdummy = 0;
	char str[32];

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_font_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Font", "Editing", 640, 0, 470, 204)==0) return;

	switch(cu->curinfo.flag & CU_STYLE) {
		case CU_BOLD:
			G.buts->texnr= give_vfontnr(cu->vfontb);
			break;
		case CU_ITALIC:
			G.buts->texnr= give_vfontnr(cu->vfonti);
			break;						
		case (CU_BOLD|CU_ITALIC):
			G.buts->texnr= give_vfontnr(cu->vfontbi);
			break;
		default:
			G.buts->texnr= give_vfontnr(cu->vfont);
			break;						
	}	

	strp= give_vfontbutstr();
//	vfd= cu->vfont->data;

	uiDefBut(block, BUT,B_LOADFONT, "Load",	480,188,68,20, 0, 0, 0, 0, 0, "Load a new font");
	uiDefButS(block, MENU, B_SETFONT, strp, 550,188,220,20, &G.buts->texnr, 0, 0, 0, 0, "Change font for object");

	if (cu->vfont->packedfile) {
		packdummy = 1;
	} else {
		packdummy = 0;
	}
	uiDefIconButI(block, TOG|BIT|0, B_PACKFONT, ICON_PACKAGE,	772,188,20,20, &packdummy, 0, 0, 0, 0, "Pack/Unpack this font");

	/* This doesn't work anyway */
//	uiDefBut(block, LABEL, 0, vfd->name,  480, 165,314,20, 0, 0, 0, 0, 0, "Postscript name of the font");

	uiDefBut(block, BUT, B_LOAD3DTEXT, "Insert Text", 480, 165, 90, 20, 0, 0, 0, 0, 0, "Insert text file at cursor");
	uiDefBut(block, BUT, B_LOREM, "Lorem", 575, 165, 70, 20, 0, 0, 0, 0, 0, "Insert a paragraph of Lorem Ipsum at cursor");	
	uiDefButC(block, TOG|BIT|2,B_STYLETOSELU, "U",		727,165,20,20, &(cu->curinfo.flag), 0,0, 0, 0, "");	
	uiBlockBeginAlign(block);
	uiDefButBitC(block, TOG, CU_BOLD, B_STYLETOSELB, "B",		752,165,20,20, &(cu->curinfo.flag), 0,0, 0, 0, "");
	uiDefButBitC(block, TOG, CU_ITALIC, B_STYLETOSELI, "i",		772,165,20,20, &(cu->curinfo.flag), 0, 0, 0, 0, "");	
	uiBlockEndAlign(block);

	MEM_freeN(strp);

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_MAKEFONT, "Left",		480,135,47,20, &cu->spacemode, 0.0,0.0, 0, 0, "Left align the text from the object center");
	uiDefButS(block, ROW,B_MAKEFONT, "Center",		527,135,47,20, &cu->spacemode, 0.0,1.0, 0, 0, "Middle align the text from the object center");
	uiDefButS(block, ROW,B_MAKEFONT, "Right",		574,135,47,20, &cu->spacemode, 0.0,2.0, 0, 0, "Right align the text from the object center");
	uiDefButS(block, ROW,B_MAKEFONT, "Justify",		621,135,47,20, &cu->spacemode, 0.0,3.0, 0, 0, "Fill completed lines to maximum textframe width by expanding whitespace");
	uiDefButS(block, ROW,B_MAKEFONT, "Flush",		668,135,47,20, &cu->spacemode, 0.0,4.0, 0, 0, "Fill every line to maximum textframe width, distributing space among all characters");	
	uiDefBut(block, BUT, B_TOUPPER, "ToUpper",		715,135,78,20, 0, 0, 0, 0, 0, "Toggle between upper and lower case in editmode");
	uiBlockEndAlign(block);
	uiDefButBitS(block, TOG, CU_FAST, B_FASTFONT, "Fast Edit",		715,105,78,20, &cu->flag, 0, 0, 0, 0, "Don't fill polygons while editing");	

	uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_TEXTONCURVE, "TextOnCurve:",	480,105,220,19, &cu->textoncurve, "Apply a deforming curve to the text");
	uiDefBut(block, TEX,REDRAWVIEW3D, "Ob Family:",	480,84,220,19, cu->family, 0.0, 20.0, 0, 0, "Blender uses font from selfmade objects");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,B_MAKEFONT, "Size:",		480,56,155,20, &cu->fsize, 0.1,10.0, 10, 0, "Size of the text");
	uiDefButF(block, NUM,B_MAKEFONT, "Linedist:",	640,56,155,20, &cu->linedist, 0.0,10.0, 10, 0, "Distance between text lines");
	uiDefButF(block, NUM,B_MAKEFONT, "Word spacing:",	795,56,155,20, &cu->wordspace, 0.0,10.0, 10, 0, "Distance factor between words");		
	uiDefButF(block, NUM,B_MAKEFONT, "Spacing:",	480,34,155,20, &cu->spacing, 0.0,10.0, 10, 0, "Spacing of individual characters");
	uiDefButF(block, NUM,B_MAKEFONT, "X offset:",	640,34,155,20, &cu->xof, -50.0,50.0, 10, 0, "Horizontal position from object center");
	uiDefButF(block, NUM,B_MAKEFONT, "UL position:",	795,34,155,20, &cu->ulpos, -0.2,0.8, 10, 0, "Vertical position of underline");			
	uiDefButF(block, NUM,B_MAKEFONT, "Shear:",		480,12,155,20, &cu->shear, -1.0,1.0, 10, 0, "Italic angle of the characters");
	uiDefButF(block, NUM,B_MAKEFONT, "Y offset:",	640,12,155,20, &cu->yof, -50.0,50.0, 10, 0, "Vertical position from object center");
	uiDefButF(block, NUM,B_MAKEFONT, "UL height:",	795,12,155,20, &cu->ulheight, 0.01,0.5, 10, 0, "Thickness of underline");				
	uiBlockEndAlign(block);	
	
	sprintf(str, "%d TextFrame: ", cu->totbox);
	uiBlockBeginAlign(block);
	uiDefButI(block, NUM, REDRAWVIEW3D, str, 805, 188, 145, 20, &cu->actbox, 1.0, cu->totbox, 0, 10, "Textbox to show settings for");
	uiDefBut(block, BUT,B_INSTB, "Insert", 805, 168, 72, 20, 0, 0, 0, 0, 0, "Insert a new text frame after the current one");
	uiDefBut(block, BUT,B_DELTB, "Delete", 877, 168, 73, 20, 0, 0, 0, 0, 0, "Delete current text frame and shift the others up");	
	uiDefButF(block, NUM,B_MAKEFONT, "X:", 805, 148, 72, 20, &(cu->tb[cu->actbox-1].x), -50.0, 50.0, 10, 0, "Horizontal offset of text frame");
	uiDefButF(block, NUM,B_MAKEFONT, "Y:", 877, 148, 73, 20, &(cu->tb[cu->actbox-1].y), -50.0, 50.0, 10, 0, "Horizontal offset of text frame");	
	uiDefButF(block, NUM,B_MAKEFONT, "Width:", 805, 128, 145, 20, &(cu->tb[cu->actbox-1].w), 0.0, 50.0, 10, 0, "Horizontal offset of text frame");
	uiDefButF(block, NUM,B_MAKEFONT, "Height:", 805, 108, 145, 20, &(cu->tb[cu->actbox-1].h), 0.0, 50.0, 10, 0, "Horizontal offset of text frame");		
	uiBlockEndAlign(block);
}


/* *************************** CURVE ******************************** */


void do_curvebuts(unsigned short event)
{
	extern Nurb *lastnu;
	extern ListBase editNurb;  /* from editcurve */
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
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
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
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SETWEIGHT:
		if(G.obedit) {
			weightflagNurb(1, editbutweight, 0);
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
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
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_TILTINTERP:
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_SUBSURFTYPE:
		/* fallthrough */
	case B_MAKEDISP:
		if(G.vd) {
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSALL, 0);
			allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */
		}
		break;

	case B_SUBDIVCURVE:
		subdivideNurb();
		break;
	case B_SPINNURB:
		if( (G.obedit==NULL) || (G.obedit->type!=OB_SURF) || (G.vd==NULL) ||
			((G.obedit->lay & G.vd->lay) == 0) ) return;
		spinNurb(0, 0);
		countall();
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
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
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
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

		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSALL, 0);
		allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */

		break;
	
	/* Buttons for aligning handles */
	case B_SETPT_AUTO:
		if(ob->type==OB_CURVE) {
			sethandlesNurb(1);
			BIF_undo_push("Auto Curve Handles");
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SETPT_VECTOR:
		if(ob->type==OB_CURVE) {
			sethandlesNurb(2);
			BIF_undo_push("Vector Curve Handles");
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SETPT_ALIGN:
		if(ob->type==OB_CURVE) {
			sethandlesNurb(5);
			BIF_undo_push("Align Curve Handles");
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SETPT_FREE:
		if(ob->type==OB_CURVE) {
			sethandlesNurb(6);
			BIF_undo_push("Free Align Curve Handles");
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	}
}

static void editing_panel_curve_tools(Object *ob, Curve *cu)
{
	Nurb *nu;
	extern ListBase editNurb;  /* from editcurve */
	extern Nurb *lastnu;
	uiBlock *block;
	short *sp;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_curve_tools", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Curve Tools", "Editing", 640, 0, 318, 204)==0) return;

	uiDefBut(block, LABEL, 0, "Make Knots",562,173,102, 18, 0, 0, 0, 0, 0, "");

	if(ob->type==OB_CURVE) {
		uiDefBut(block, LABEL, 0, "Convert",	463,173,72, 18, 0, 0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefBut(block, BUT,B_CONVERTPOLY,"Poly",		467,152,72, 18, 0, 0, 0, 0, 0, "Converts selected into regular Polygon vertices");
		uiDefBut(block, BUT,B_CONVERTBEZ,"Bezier",		467,132,72, 18, 0, 0, 0, 0, 0, "Converts selected to Bezier triples");
		uiDefBut(block, BUT,B_CONVERTNURB,"Nurb",		467,112,72, 18, 0, 0, 0, 0, 0, "Converts selected to Nurbs Points");
	}
	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_UNIFU,"Uniform U",	565,152,102, 18, 0, 0, 0, 0, 0, "Nurbs only; interpolated result doesn't go to end points in U");
	uiDefBut(block, BUT,B_UNIFV,"V",			670,152,50, 18, 0, 0, 0, 0, 0, "Nurbs only; interpolated result doesn't go to end points in V");
	uiDefBut(block, BUT,B_ENDPU,"Endpoint U",	565,132,102, 18, 0, 0, 0, 0, 0, "Nurbs only; interpolated result is forced to end points in U");
	uiDefBut(block, BUT,B_ENDPV,"V",			670,132,50, 18, 0, 0, 0, 0, 0, "Nurbs only; interpolated result is forced to end points in V");
	uiDefBut(block, BUT,B_BEZU,"Bezier U",		565,112,102, 18, 0, 0, 0, 0, 0, "Nurbs only; make knots array mimic a Bezier in U");
	uiDefBut(block, BUT,B_BEZV,"V",				670,112,50, 18, 0, 0, 0, 0, 0, "Nurbs only; make knots array mimic a Bezier in V");
	uiBlockEndAlign(block);

	uiDefBut(block, BUT,B_SETWEIGHT,"Set Weight",	465,11,95,49, 0, 0, 0, 0, 0, "Nurbs only; set weight for select points");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,0,"Weight:",		565,36,102,22, &editbutweight, 0.01, 100.0, 10, 0, "The weight you can assign");
	uiDefBut(block, BUT,B_SETW1,"1.0",		670,36,50,22, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_SETW2,"sqrt(2)/4",565,11,55,20, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_SETW3,"0.25",		620,11,45,20, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_SETW4,"sqrt(0.5)",665,11,55,20, 0, 0, 0, 0, 0, "");
	uiBlockEndAlign(block);

	if(ob==G.obedit) {
		nu= lastnu;
		if(nu==NULL) nu= editNurb.first;
		if(nu) {
			if (ob->type==OB_CURVE) {
				uiDefBut(block, LABEL, 0, "Tilt",
					467,87,72, 18, 0, 0, 0, 0, 0, "");
				/* KEY_LINEAR, KEY_CARDINAL, KEY_BSPLINE */
				uiDefButS(block, MENU, B_TILTINTERP, "Tilt Interpolation %t|Linear %x0|Cardinal %x1|BSpline %x2",
					467,67,72, 18, &(nu->tilt_interp), 0, 0, 0, 0, "Tilt interpolation");
			}
						
			uiBlockBeginAlign(block);
			sp= &(nu->orderu);
			uiDefButS(block, NUM, B_SETORDER, "Order U:", 565,90,102, 19, sp, 2.0, 6.0, 0, 0, "Nurbs only; the amount of control points involved");
			sp= &(nu->orderv);
			uiDefButS(block, NUM, B_SETORDER, "V:",	 	670,90,50, 19, sp, 2.0, 6.0, 0, 0, "Nurbs only; the amount of control points involved");
			sp= &(nu->resolu);
			uiDefButS(block, NUM, B_MAKEDISP, "Resol U:", 565,70,102, 19, sp, 1.0, 1024.0, 0, 0, "The amount of new points interpolated per control vertex pair");
			sp= &(nu->resolv);
			uiDefButS(block, NUM, B_MAKEDISP, "V:", 	670,70,50, 19, sp, 1.0, 1024.0, 0, 0, "The amount of new points interpolated per control vertex pair");
		}
	}


}

static void editing_panel_curve_tools1(Object *ob, Curve *cu)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_curve_tools1", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Curve Tools1", "Editing", 960, 0, 318, 204)==0) return;

	uiDefBut(block, BUT, B_SUBDIVCURVE, "Subdivide", 400,180,150,20, 0, 0, 0, 0, 0, "Subdivide selected");
	if(ob->type==OB_SURF) {
		uiDefBut(block, BUT, B_SPINNURB, "Spin",	 400,160,150,20, 0, 0, 0, 0, 0, "Spin selected 360 degrees");
	}
	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_HIDE,		"Hide",			400,140,150,18, 0, 0, 0, 0, 0, "Hides selected faces");
	uiDefBut(block, BUT,B_REVEAL,	"Reveal",		400,120,150,18, 0, 0, 0, 0, 0, "Reveals selected faces");
	uiDefBut(block, BUT,B_SELSWAP,	"Select Swap",	400,100,150,18, 0, 0, 0, 0, 0, "Selects unselected faces, and deselects selected faces");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,	REDRAWVIEW3D, "NSize:",	400, 60, 150, 19, &G.scene->editbutsize, 0.001, 1.0, 10, 0, "Normal size for drawing");
	uiDefButBitI(block, TOGN, G_HIDDENHANDLES, REDRAWVIEW3D, "Draw Handles", 	400, 40, 150, 19, &G.f, 0, 0, 0, 0, "Draw curve handles in 3D view");
	uiBlockEndAlign(block);
	
	if(G.obedit) {
		uiBut *but;
		uiBlockBeginAlign(block);
		but= uiDefButBitS(block,TOG,CU_RETOPO,B_NOP, "Retopo", 560,180,100,19, &cu->flag, 0,0,0,0, "Turn on the re-topology tool");
		uiButSetFunc(but,retopo_toggle,0,0);
		if(cu->flag & CU_RETOPO) {
			but= uiDefBut(block,BUT,B_NOP,"Retopo All", 560,160,100,19, 0,0,0,0,0, "Apply the re-topology tool to all selected vertices");
			uiButSetFunc(but,retopo_do_all_cb,0,0);
		}
	}
}

/* only for bevel or taper */
static void test_obcurpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	for(id= G.main->object.first; id; id= id->next) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_CURVE) {
				error ("Bevel/Taper Object must be a Curve");
				break;
			}
			if(id == (ID *)OBACT) {
				error ("Cannot Bevel/Taper own Object");
				break;
			}
			
			*idpp= id;
			return;
		}
	}
	*idpp= NULL;
}



/* for curve, surf and font! */
static void editing_panel_curve_type(Object *ob, Curve *cu)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_curve_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Curve and Surface", "Editing", 320, 0, 318, 204)==0) return;

	uiDefButBitS(block, TOG, CU_UV_ORCO, 0, "UV Orco",					600,160,150,19, &cu->flag, 0, 0, 0, 0, "Forces to use UV coordinates for texture mapping 'orco'");
	if(ob->type==OB_SURF)
		uiDefButBitS(block, TOG, CU_NOPUNOFLIP, REDRAWVIEW3D, "No Puno Flip",	600,140,150,19, &cu->flag, 0, 0, 0, 0, "Don't flip vertex normals while render");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_DOCENTER, "Center",					600, 115, 55, 19, 0, 0, 0, 0, 0, "Shifts object data to be centered about object's origin");
	uiDefBut(block, BUT,B_DOCENTERNEW, "Center New",			655, 115, 95, 19, 0, 0, 0, 0, 0, "Shifts object's origin to center of object data");
	uiDefBut(block, BUT,B_DOCENTERCURSOR, "Center Cursor",		600, 95, 150, 19, 0, 0, 0, 0, 0, "Shifts object's origin to cursor location");
	uiBlockEndAlign(block);

	if(cu->key) {
		/* uiDefButS(block, NUM, B_DIFF, "Slurph:",			600,25,140,19, &(cu->key->slurph), -500.0, 500.0,0,0); ,""*/
		uiDefButS(block, TOG, B_RELKEY, "Relative Keys",	600, 72,150,19, &cu->key->type, 0, 0, 0, 0, "");
	}


	if(ob->type!=OB_SURF) {
	
		if(ob->type==OB_CURVE) {
			extern float prlen;		// buttons_object.c, should be moved....
			char str[32];
			
			sprintf(str, "%.4f", prlen);
			uiDefBut(block, BUT, B_PRINTLEN,		"PrintLen",	600,135,75,19, 0, 0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, str,						675,135,75,19, 0, 1.0, 0, 0, 0, "");
			
			uiBlockBeginAlign(block);
			/*note, PathLen's max was MAXFRAMEF but this is a short, perhaps the pathlen should be increased later on */
			uiDefButS(block, NUM, B_RECALCPATH, "PathLen:",			600,50,150,19, &cu->pathlen, 1.0, 32767.0f, 0, 0, "If no speed Ipo was set, the amount of frames of the path");
			uiDefButBitS(block, TOG, CU_PATH, B_RECALCPATH, "CurvePath",	600,30,75,19 , &cu->flag, 0, 0, 0, 0, "Enables curve to become translation path");
			uiDefButBitS(block, TOG, CU_FOLLOW, REDRAWVIEW3D, "CurveFollow",675,30,75,19, &cu->flag, 0, 0, 0, 0, "Makes curve path children to rotate along path");
			uiDefButBitS(block, TOG, CU_STRETCH, B_CURVECHECK, "CurveStretch", 600,10,150,19, &cu->flag, 0, 0, 0, 0, "Option for curve-deform: makes deformed child to stretch along entire path");
			uiDefButBitS(block, TOG, CU_OFFS_PATHDIST, REDRAWVIEW3D, "PathDist Offs", 600,-10,150,19, &cu->flag, 0, 0, 0, 0, "Children will use TimeOffs value as path distance offset");

			uiBlockEndAlign(block);
		}

		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_SETRESOLU, "DefResolU:",	760,160,150,19, &cu->resolu, 1.0, 1024.0, 0, 0, "Default resolution");
		uiDefButS(block, NUM, B_NOP, "RenResolU",		760,140,150,19, &cu->resolu_ren, 0.0f, 1024, 0, 0, "Set resolution for rendering. A value of zero skips this operation.");

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_MAKEDISP, "Width:",		760,90,150,19, &cu->width, 0.0, 2.0, 1, 0, "Make interpolated result thinner or fatter");
		uiDefButF(block, NUM, B_MAKEDISP, "Extrude:",		760,70,150,19, &cu->ext1, 0.0, 100.0, 10, 0, "Curve extrusion size when not using a bevel object");
		uiDefButF(block, NUM, B_MAKEDISP, "Bevel Depth:",		760,50,150,19, &cu->ext2, 0.0, 2.0, 1, 0, "Bevel depth when not using a bevel object");
		uiDefButS(block, NUM, B_MAKEDISP, "BevResol:",	760,30,150,19, &cu->bevresol, 0.0, 32.0, 0, 0, "Bevel resolution when depth is non-zero and not using a bevel object");
		uiDefIDPoinBut(block, test_obcurpoin_but, ID_OB, B_CHANGEDEP, "BevOb:",		760,10,150,19, &cu->bevobj, "Curve object name that defines the bevel shape");
		uiDefIDPoinBut(block, test_obcurpoin_but, ID_OB, B_CHANGEDEP, "TaperOb:",		760,-10,150,19, &cu->taperobj, "Curve object name that defines the taper (width)");

		uiBlockBeginAlign(block);
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButBitS(block, TOG, CU_BACK, B_MAKEDISP, "Back",	760,115,50,19, &cu->flag, 0, 0, 0, 0, "Draw filled back for extruded/beveled curves");
		uiDefButBitS(block, TOG, CU_FRONT, B_MAKEDISP, "Front",810,115,50,19, &cu->flag, 0, 0, 0, 0, "Draw filled front for extruded/beveled curves");
		uiDefButBitS(block, TOG, CU_3D, B_CU3D, "3D",		860,115,50,19, &cu->flag, 0, 0, 0, 0, "Allow Curve to be 3d, it doesn't fill then");
	}
}


/* *************************** CAMERA ******************************** */

/* callback to handle angle to lens conversion */
static void do_angletolensconversion_cb(void *lens1, void *angle1) 
{
	float *lens= (float *)lens1;
	float *angle= (float *)angle1;
	float anglevalue= *angle;
	
	if(lens) {
		*lens= 16.0f / tan(M_PI*anglevalue/360.0f);
	} 

	allqueue(REDRAWVIEW3D, 0);
}

/* callback to handle lens to angle conversion */
static void do_lenstoangleconversion_cb(void *lens1, void *angle1) 
{
	float *lens= (float *)lens1;
	float *angle= (float *)angle1;
	float lensvalue= *lens;
	
	if(lens) {
		*angle= 360.0f * atan(16.0f/lensvalue) / M_PI;
	} 

	allqueue(REDRAWVIEW3D, 0);
}

static void editing_panel_camera_type(Object *ob, Camera *cam)
{
	uiBlock *block;
	uiBut *but;
	float grid=0.0;

	if(G.vd) grid= G.vd->grid;
	if(grid<1.0) grid= 1.0;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_camera_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Camera", "Editing", 320, 0, 318, 204)==0) return;
	
	uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	uiDefBut(block, LABEL, 10, "Lens:", 10, 180, 150, 20, 0, 0.0, 0.0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	if(cam->type==CAM_ORTHO) {
		uiDefButF(block, NUM,REDRAWVIEW3D, "Scale:",
				  10, 160, 150, 20, &cam->ortho_scale, 0.01, 1000.0, 50, 0, "Specify the ortho scaling of the used camera");
	} else {
		if(cam->flag & CAM_ANGLETOGGLE) {
			but= uiDefButF(block, NUM,REDRAWVIEW3D, "Lens:",
					  10, 160, 130, 20, &cam->angle, 7.323871, 172.847331, 100, 0, "Specify the lens of the camera in degrees");		
			uiButSetFunc(but,do_angletolensconversion_cb, &cam->lens, &cam->angle);
		}
		else {
			but= uiDefButF(block, NUM,REDRAWVIEW3D, "Lens:",
					  10, 160, 130, 20, &cam->lens, 1.0, 250.0, 100, 0, "Specify the lens of the camera");
			uiButSetFunc(but,do_lenstoangleconversion_cb, &cam->lens, &cam->angle);
		}
		
		uiDefButS(block, TOG|BIT|5, B_REDR, "D",
			140, 160, 20, 20, &cam->flag, 0, 0, 0, 0, "Use degree as the unit of the camera lens");
	}
	uiDefButS(block, TOG, REDRAWVIEW3D, "Orthographic",
		  10, 140, 150, 20, &cam->type, 0, 0, 0, 0, "Render with orthographic projection (no prespective)");
	uiBlockEndAlign(block);

/* qdn: focal dist. param. from yafray now enabled for Blender as well, to use with defocus composit node */
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, REDRAWVIEW3D, "Dof Dist:", 10, 110, 150, 20 /*0, 125, 150, 20*/, &cam->YF_dofdist, 0.0, 5000.0, 50, 0, "Sets distance to point of focus (enable 'Limits' to make visible in 3Dview)");
	uiDefIDPoinBut(block, test_obpoin_but, ID_OB, REDRAWVIEW3D, "Dof Ob:",	10, 90, 150, 20, &cam->dof_ob, "Focus on this object (overrides the 'Dof Dist')");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Clipping Start/End:", 10, 45, 150, 20, 0, 0.0, 0.0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,REDRAWVIEW3D, "Start:",
			  10, 25, 150, 20, &cam->clipsta, 0.001*grid, 100.0*grid, 10, 0, "Clip out geometry closer then this distance to the camera");
	uiDefButF(block, NUM,REDRAWVIEW3D, "End:",
			  10, 5, 150, 20, &cam->clipend, 1.0, 5000.0*grid, 100, 0, "Clip out geometry further then this distance to the camera");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Show:", 170, 180, 150, 20, 0, 0.0, 0.0, 0, 0, "");

	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|0, REDRAWVIEW3D, "Limits",
			  170, 160, 75, 20, &cam->flag, 0, 0, 0, 0, "Draw the clipping range and the focal point");
	uiDefButS(block, TOG|BIT|1, REDRAWVIEW3D, "Mist",
			  245, 160, 75, 20, &cam->flag, 0, 0, 0, 0, "Draw a line that indicates the mist area");
	
	uiDefButS(block, TOG|BIT|4, REDRAWVIEW3D, "Name",
			  170, 140, 75, 20, &cam->flag, 0, 0, 0, 0, "Draw the active camera's name in camera view");
		uiDefButS(block, TOG|BIT|3, REDRAWVIEW3D, "Title Safe",
			  245, 140, 75, 20, &cam->flag, 0, 0, 0, 0, "Draw a the title safe zone in camera view");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|2, REDRAWVIEW3D, "Passepartout",
			  170, 110, 150, 20, &cam->flag, 0, 0, 0, 0, "Draw a darkened passepartout over the off-screen area in camera view");
	uiDefButF(block, NUMSLI, REDRAWVIEW3D, "Alpha: ",
			170, 90, 150, 20, &cam->passepartalpha, 0.0, 1.0, 0, 0, "The opacity (darkness) of the passepartout");
	uiBlockEndAlign(block);

	uiDefButF(block, NUM,REDRAWVIEW3D, "Size:",
			  170, 50, 150, 20, &cam->drawsize, 0.1*grid, 10.0, 10, 0, "The size that the camera is displayed in the 3D View (different from the object's scale)");
	
	uiDefBut(block, LABEL, 0, "Shift:", 170, 25, 150, 20, 0, 0.0, 0.0, 0, 0, "");
				  
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,REDRAWVIEW3D, "X:",
		170, 5, 75, 20, &cam->shiftx, -2.0, 2.0, 1, 2, "Horizontally shift the camera view, without changing the perspective");
	uiDefButF(block, NUM,REDRAWVIEW3D, "Y:",
		245, 5, 75, 20, &cam->shifty, -2.0, 2.0, 1, 2, "Vertically shift the camera view, without changing the perspective");
	uiBlockEndAlign(block);
}

/* yafray: extra camera panel to set Depth-of-Field parameters */
static void editing_panel_camera_yafraydof(Object *ob, Camera *cam)
{
	uiBlock *block;
	char *mst1, *mst2;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_camera_yafraydof", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Camera", "Editing");
	if(uiNewPanel(curarea, block, "Yafray DoF", "Editing", 320, 0, 318, 204)==0) return;

	uiDefButF(block, NUM, REDRAWVIEW3D, "DoFDist:", 10, 147, 180, 20, &cam->YF_dofdist, 0.0, 5000.0, 50, 0, "Sets distance to point of focus (use camera 'ShowLimits' to make visible in 3Dview)");
	uiDefButF(block, NUM, B_DIFF, "Aperture:", 10, 125, 180, 20, &cam->YF_aperture, 0.0, 2.0, 1, 0, "Sets lens aperture, the larger, the more blur (use small values, 0 is no DoF)");

	uiDefButBitS(block, TOG, CAM_YF_NO_QMC, B_DIFF, "Random sampling", 10, 90, 180, 20, &cam->flag, 0, 0, 0, 0, "Use noisy random Lens sampling instead of QMC");

	uiDefBut(block, LABEL, 0, "Bokeh", 10, 60, 180, 19, 0, 0.0, 0.0, 0, 0, "");
	mst1 = "Bokeh Type%t|Disk1%x0|Disk2%x1|Triangle%x2|Square%x3|Pentagon%x4|Hexagon%x5|Ring%x6";
	uiDefButS(block, MENU, B_REDR, mst1, 10, 40, 89, 20, &cam->YF_bkhtype, 0.0, 0.0, 0, 0, "Sets Bokeh type");
	
	if ((cam->YF_bkhtype!=0) && (cam->YF_bkhtype!=6)) {
		mst2 = "Bokeh Bias%t|Uniform%x0|Center%x1|Edge%x2";
		uiDefButS(block, MENU, B_REDR, mst2, 100, 40, 90, 20, &cam->YF_bkhbias, 0.0, 0.0, 0, 0, "Sets Bokeh bias");
		if (cam->YF_bkhtype>1)
			uiDefButF(block, NUM, B_DIFF, "Rotation:", 10, 15, 180, 20, &cam->YF_bkhrot, 0.0, 360.0, 100, 0, "Shape rotation amount in degrees");
	}

}

/* **************************** CAMERA *************************** */

void do_cambuts(unsigned short event)
{
	Object *ob;
	Camera *cam;
	
	ob= OBACT;
	if (ob==0) return;
	cam= ob->data;

	switch(event) {
	case 0:
		;
		break;
	}
}

/* *************************** MBALL ******************************** */

void do_mballbuts(unsigned short event)
{
	switch(event) {
	case B_RECALCMBALL:
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		break;
	}
}

static void editing_panel_mball_type(Object *ob, MetaBall *mb)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mball_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "MetaBall", "Editing", 320, 0, 318, 204)==0) return;

	ob= find_basis_mball(ob);
	mb= ob->data;

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_RECALCMBALL, "Wiresize:", 470,178,250,19, &mb->wiresize, 0.05, 1.0, 1, 0, "Polygonization resolution in 3d window");
	uiDefButF(block, NUM, B_NOP, "Rendersize:", 470,158,250,19, &mb->rendersize, 0.05, 1.0, 1, 0, "Polygonization resolution in rendering");
	uiDefButF(block, NUM, B_RECALCMBALL, "Threshold:", 470,138,250,19, &mb->thresh, 0.0001, 5.0, 1, 0, "Defines influence of meta elements");

	uiBlockBeginAlign(block);
	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefBut(block, LABEL, 0, "Update:", 471,108,120,19, 0, 0, 0, 0, 0, "");
	uiDefButS(block, ROW, B_DIFF, "Always",	471, 85, 120, 19, &mb->flag, 0.0, 0.0, 0, 0, "While editing, always updates");
	uiDefButS(block, ROW, B_DIFF, "Half Res", 471, 65, 120, 19, &mb->flag, 0.0, 1.0, 0, 0, "While editing, updates in half resolution");
	uiDefButS(block, ROW, B_DIFF, "Fast", 471, 45, 120, 19, &mb->flag, 0.0, 2.0, 0, 0, "While editing, updates without polygonization");
	uiDefButS(block, ROW, B_DIFF, "Never", 471, 25, 120, 19, &mb->flag, 0.0, 3.0, 0, 0, "While editing, doesn't update");

}

static void editing_panel_mball_tools(Object *ob, MetaBall *mb)
{
	extern MetaElem *lastelem;
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mball_tools", UI_EMBOSS, UI_HELV, curarea->win);
	if( uiNewPanel(curarea, block, "MetaBall tools", "Editing", 640, 0, 318, 204)==0) return;

	if(ob==G.obedit && lastelem) {
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_RECALCMBALL, "Stiffness:", 750,178,250,19, &lastelem->s, 0.0, 10.0, 1, 0, "Stiffness for active meta");
		if(lastelem->type!=MB_BALL)
		uiDefButF(block, NUM, B_RECALCMBALL, "dx:", 750,158,250,19, &lastelem->expx, 0.0, 20.0, 1, 0, "X size for active meta");
		if((lastelem->type!=MB_BALL)&&(lastelem->type!=MB_TUBE))
		uiDefButF(block, NUM, B_RECALCMBALL, "dy:", 750,138,250,19, &lastelem->expy, 0.0, 20.0, 1, 0, "Y size for active meta");

		if((lastelem->type==MB_CUBE)||(lastelem->type==MB_ELIPSOID))
		uiDefButF(block, NUM, B_RECALCMBALL, "dz:", 750,118,250,19, &lastelem->expz, 0.0, 20.0, 1, 0, "Z size for active meta");
		uiBlockEndAlign(block);

		uiDefButS(block, ROW, B_RECALCMBALL, "Ball", 753,83,60,19, &lastelem->type, 1.0, MB_BALL, 0, 0, "Draw active meta as Ball");
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, B_RECALCMBALL, "Tube", 753,62,60,19, &lastelem->type, 1.0, MB_TUBE, 0, 0, "Draw active meta as Ball");
		uiDefButS(block, ROW, B_RECALCMBALL, "Plane", 814,62,60,19, &lastelem->type, 1.0, MB_PLANE, 0, 0, "Draw active meta as Plane");
		uiDefButS(block, ROW, B_RECALCMBALL, "Elipsoid", 876,62,60,19, &lastelem->type, 1.0, MB_ELIPSOID, 0, 0, "Draw active meta as Ellipsoid");
		uiDefButS(block, ROW, B_RECALCMBALL, "Cube", 938,62,60,19, &lastelem->type, 1.0, MB_CUBE, 0, 0, "Draw active meta as Cube");
		uiBlockEndAlign(block);

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, MB_NEGATIVE, B_RECALCMBALL, "Negative",753,16,125,19, &lastelem->flag, 0, 0, 0, 0, "Make active meta creating holes");
		uiDefButBitS(block, TOG, MB_HIDE, B_RECALCMBALL, "Hide",878,16,125,19, &lastelem->flag, 0, 0, 0, 0, "Make active meta invisible");
		uiBlockEndAlign(block);

	}

}


/* *************************** LATTICE ******************************** */

void do_latticebuts(unsigned short event)
{
	Object *ob;
	Lattice *lt;

	ob= OBACT;
	if(ob->type!=OB_LATTICE) return;

	switch(event) {
	case B_REGULARLAT:
		if(ob) {
			lt = ob->data;
			if(ob==G.obedit) resizelattice(editLatt, lt->opntsu, lt->opntsv, lt->opntsw, NULL);
			else resizelattice(ob->data, lt->opntsu, lt->opntsv, lt->opntsw, NULL);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
	case B_RESIZELAT:
		if(ob) {
			lt = ob->data;
			resizelattice(ob->data, lt->opntsu, lt->opntsv, lt->opntsw, ob);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_DRAWLAT:
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_LATTCHANGED:

		lt= ob->data;
		if(lt->flag & LT_OUTSIDE) outside_lattice(lt);

		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

		allqueue(REDRAWVIEW3D, 0);

		break;
	}
}

static void editing_panel_lattice_type(Object *ob, Lattice *lt)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_lattice_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Lattice", "Editing", 320, 0, 318, 204)==0) return;


	uiSetButLock(lt->key!=0, "Not with VertexKeys");
	uiSetButLock(ob==G.obedit, "Unable to perform function in EditMode");

	uiBlockBeginAlign(block);

	lt->opntsu = lt->pntsu;
	lt->opntsv = lt->pntsv;
	lt->opntsw = lt->pntsw;

	uiDefButS(block, NUM, B_RESIZELAT,	"U:",				469, 178,100,19, &lt->opntsu, 1.0, 64.0, 0, 0, "Points in U direction");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_LINEAR, 0, 0, "Set Linear interpolation");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		613, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_CARDINAL, 0, 0, "Set Cardinal interpolation");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_BSPLINE, 0, 0, "Set B-spline interpolation");

	uiDefButS(block, NUM, B_RESIZELAT,	"V:",				469, 156,100,19, &lt->opntsv, 1.0, 64.0, 0, 0, "Points in V direction");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 156, 40, 19, &lt->typev, 2.0, (float)KEY_LINEAR, 0, 0, "Set Linear interpolation");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		613, 156, 40, 19, &lt->typev, 2.0, (float)KEY_CARDINAL, 0, 0, "Set Cardinal interpolation");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 156, 40, 19, &lt->typev, 2.0, (float)KEY_BSPLINE, 0, 0, "Set B-spline interpolation");

	uiDefButS(block, NUM, B_RESIZELAT,	"W:",				469, 134,100,19, &lt->opntsw, 1.0, 64.0, 0, 0, "Points in W direction");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 134, 40, 19, &lt->typew, 3.0, (float)KEY_LINEAR, 0, 0, "Set Linear interpolation");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		613, 134, 40, 19, &lt->typew, 3.0, (float)KEY_CARDINAL, 0, 0, "Set Cardinal interpolation");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 134, 40, 19, &lt->typew, 3.0, (float)KEY_BSPLINE, 0, 0, "Set B-spline interpolation");

	uiBlockEndAlign(block);

	uiDefBut(block, BUT, B_REGULARLAT,	"Make Regular",		469,98,102,31, 0, 0, 0, 0, 0, "Make Lattice regular");

	uiClearButLock();
	uiDefButBitS(block, TOG, LT_OUTSIDE, B_LATTCHANGED, "Outside",	571,98,122,31, &lt->flag, 0, 0, 0, 0, "Only draw, and take into account, the outer vertices");

	if(lt->key) {
		uiDefButS(block, NUM, B_DIFF, "Slurph:",			469,60,120,19, &(lt->key->slurph), -500.0, 500.0, 0, 0, "Set time value to denote 'slurph' (sequential delay) vertices with key framing");
		uiDefButS(block, TOG, B_RELKEY, "Relative Keys",	469,40,120,19, &lt->key->type, 0, 0, 0, 0, "Use relative keys (instead of absolute)");
	}

}

/* *************************** ARMATURE ******************************** */

void do_armbuts(unsigned short event)
{
	Object *ob= OBACT;
	bAction *act;
	
	switch(event) {
	case B_ARM_RECALCDATA:
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 1);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_ARM_STRIDE:
		if(ob && ob->pose) {
			bPoseChannel *pchan;
			bActionStrip *strip;
			
			for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next)
				if(pchan->flag & POSE_STRIDE)
					break;
			
			/* we put the stride bone name in the strips, for lookup of action channel */
			for (strip=ob->nlastrips.first; strip; strip=strip->next){
				if(strip->flag & ACTSTRIP_USESTRIDE) {
					if(pchan) BLI_strncpy(strip->stridechannel, pchan->name, 32);
					else strip->stridechannel[0]= 0;
				}
			}
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 1);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;
	case B_ARM_CALCPATHS:
		if (ob && ob->pose) 
			pose_calculate_path(ob);
		break;
	case B_ARM_CLEARPATHS:
		if (ob && ob->pose)
			pose_clear_paths(ob);
		break;
	
	case B_POSELIB_ADDPOSE:
		if (ob && ob->pose)
			poselib_add_current_pose(ob, 1);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWACTION, 0);
		break;
	case B_POSELIB_REPLACEP:
		if (ob && ob->pose)
			poselib_add_current_pose(ob, 2);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWACTION, 0);
		break;
	case B_POSELIB_REMOVEP:
		if (ob && ob->pose) {
			bAction *act= ob->poselib;
			TimeMarker *marker= poselib_get_active_pose(act);
			
			poselib_remove_pose(ob, marker);
		}
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWACTION, 0);
		break;
	case B_POSELIB_VALIDATE:
		if (ob && ob->pose)
			poselib_validate_act(ob->poselib);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWACTION, 0);
		break;
	case B_POSELIB_APPLYP:
		if (ob && ob->pose)
			poselib_preview_poses(ob, 1);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
		
	/* note: copied from headerbuttons.c */
	case B_POSELIB_ALONE: //B_ACTALONE
		if (ob && ob->id.lib==0) {
			act= ob->poselib;
			
			if (act->id.us > 1) {
				if (okee("Single user")) {
					ob->poselib= copy_action(act);
					act->id.us--;
					allqueue(REDRAWBUTSEDIT, 0);
					allqueue(REDRAWACTION, 0);
				}
			}
		}
		break;
	case B_POSELIB_DELETE: //B_ACTIONDELETE
		act= ob->poselib;
		
		if (act)
			act->id.us--;
		ob->poselib=NULL;
		
		BIF_undo_push("Unlink PoseLib");
		
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWACTION, 0);
		break;
	case B_POSELIB_BROWSE: //B_ACTIONBROWSE:
		{
			ID *id, *idtest;
			int nr= 1;
			
			if (ob == NULL)
				break;
			act= ob->poselib;
			id= (ID *)act;
			
			if (G.buts->menunr == -2) {
				activate_databrowse((ID *)ob->poselib, ID_AC,  0, B_POSELIB_BROWSE, &G.buts->menunr, do_armbuts);
				return;
			}
			if (G.buts->menunr < 0) break;
			
			/*	See if we have selected a valid action */
			for (idtest= G.main->action.first; idtest; idtest= idtest->next) {
				if (nr == G.buts->menunr) {
					break;
				}
				nr++;
			}
			
			/* Store current action */
			if (!idtest) {
				/* 'Add New' option: 
				 * 	- make a copy of an exisiting action
				 *	- or make a new empty action if no existing action
				 */
				if (act) {
					idtest= (ID *)copy_action(act);
				} 
				else { 
					/* a plain action */
					idtest=(ID *)add_empty_action("PoseLib");
				}
				idtest->us--;
			}
			
			if ((idtest != id) && (ob)) {
				act= (bAction *)idtest;
				
				ob->poselib= act;
				id_us_plus(idtest);
				
				if (id) id->us--;
				
				/* Update everything */
				BIF_undo_push("Browse PoseLibs");
				
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWHEADERS, 0); 
			}
		}
		break;
		
	case B_POSEGRP_RECALC:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_POSEGRP_ADD:
		if (ob && ob->pose)
			pose_add_posegroup();
		break;
	case B_POSEGRP_REMOVE:
		if (ob && ob->pose)
			pose_remove_posegroup();
		break;
	case B_POSEGRP_MCUSTOM:
		if (ob && ob->pose) {
			if (ob->pose->active_group) {
				bActionGroup *grp= (bActionGroup *)BLI_findlink(&ob->pose->agroups, ob->pose->active_group-1);
				grp->customCol= -1;
			}
			
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;
	}
}

static void validate_stridebutton_cb(void *pchanv, void *poin)
{
	Object *ob= OBACT;
	bPoseChannel *pchan;
	
	if(ob && ob->pose) {
		for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next){
			if(pchan!=pchanv)
				pchan->flag &= ~POSE_STRIDE;
		}
	}
}	

static int editbone_to_parnr (EditBone *bone)
{
	EditBone *ebone;
	int	index;

	for (ebone=G.edbo.first, index=0; ebone; ebone=ebone->next, index++){
		if (ebone==bone)
			return index;
	}

	return -1;
}


/* the "IK" button in editbuttons */
static void attach_bone_to_parent_cb(void *bonev, void *arg2_unused)
{
	EditBone *ebone= bonev;
	
	if (ebone->parent) {
		if(ebone->flag & BONE_CONNECTED) {
			/* Attach this bone to its parent */
			VECCOPY(ebone->head, ebone->parent->tail);

			if(ebone->flag & BONE_ROOTSEL)
				ebone->parent->flag |= BONE_TIPSEL;
		}
		else if(!(ebone->parent->flag & BONE_ROOTSEL)) {
			ebone->parent->flag &= ~BONE_TIPSEL;
		}
	}
}

static void parnr_to_editbone(EditBone *bone)
{
	if (bone->parNr == -1){
		if(bone->parent && !(bone->parent->flag & BONE_ROOTSEL))
			bone->parent->flag &= ~BONE_TIPSEL;

		bone->parent = NULL;
		bone->flag &= ~BONE_CONNECTED;
	}
	else{
		bone->parent = BLI_findlink(&G.edbo, bone->parNr);
		attach_bone_to_parent_cb(bone, NULL);
	}
}

static void parnr_to_editbone_cb(void *bonev, void *arg2_unused)
{
	EditBone *curBone= bonev;
	parnr_to_editbone(curBone);
}

/* only used for showing parent of editbones */
static void build_bonestring (char *string, EditBone *bone)
{
	bArmature *arm= G.obedit->data;
	EditBone *curBone;
	EditBone *pBone;
	int		skip=0;
	int		index, numbones, i;
	char (*qsort_ptr)[32] = NULL;
	char *s = string;

	/* That space is there for a reason - for no parent */
	s += sprintf (string, "Parent%%t| %%x%d", -1);	

	numbones = BLI_countlist(&G.edbo);

	/*
	 * This will hold the bone names temporarily so we can sort them
	 */
	if (numbones > 0)
		qsort_ptr = MEM_callocN (numbones * sizeof (qsort_ptr[0]),
								 "qsort_ptr");

	numbones = 0;
	for (curBone = G.edbo.first, index=0; curBone;
		 curBone=curBone->next, index++){
		/* Make sure this is a valid child */
		if (curBone != bone){
			skip=0;
			for (pBone=curBone->parent; pBone; pBone=pBone->parent){
				if (pBone==bone){
					skip=1;
					break;
				}
			}
			/* no browsing for bones in invisible layers */
			if ((arm->layer & curBone->layer) == 0) {
				/* but ensure the current parent at least shows */
				if(bone->parent!=curBone)
					skip= 1;
			}

			if (skip)
				continue;

			sprintf (qsort_ptr[numbones], "|%s%%x%d", curBone->name, index);
			numbones++;
		}
	}
	qsort (qsort_ptr, numbones, sizeof (qsort_ptr[0]),
		   ( int (*)(const void *, const void *) ) strcmp);

	for (i=0; i < numbones; ++i) {
		strcat(s, qsort_ptr[i]);
	}

	if (qsort_ptr)
		MEM_freeN(qsort_ptr);
}

/* assumes armature editmode */
/* exported to drawview.c via BIF_butspace.h */
void validate_editbonebutton_cb(void *bonev, void *namev)
{
	EditBone *eBone= bonev;
	char oldname[32], newname[32];
	
	/* need to be on the stack */
	BLI_strncpy(newname, eBone->name, 32);
	BLI_strncpy(oldname, (char *)namev, 32);
	/* restore */
	BLI_strncpy(eBone->name, oldname, 32);
	
	armature_bone_rename(G.obedit->data, oldname, newname); // editarmature.c
	allqueue(REDRAWALL, 0);
}

/* assumes armature posemode */
static void validate_posebonebutton_cb(void *bonev, void *namev)
{
	Bone *bone= bonev;
	Object *ob= OBACT;
	char oldname[32], newname[32];
	
	/* need to be on the stack */
	BLI_strncpy(newname, bone->name, 32);
	BLI_strncpy(oldname, (char *)namev, 32);
	/* restore */
	BLI_strncpy(bone->name, oldname, 32);
	
	armature_bone_rename(ob->data, oldname, newname); // editarmature.c
	allqueue(REDRAWALL, 0);
}

static void armature_layer_cb(void *lay_v, void *value_v)
{
	short *layer= lay_v;
	int value= (long)value_v;
	
	if(*layer==0 || G.qual==0) *layer= value;
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
}

static void editing_panel_armature_type(Object *ob, bArmature *arm)
{
	uiBlock	*block;
	uiBut *but;
	int a;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_armature_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Armature", "Editing", 320, 0, 318, 204)==0) return;

	uiDefBut(block, LABEL, 0, "Editing Options", 10,180,150,20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, ARM_MIRROR_EDIT, B_DIFF, "X-Axis Mirror",  10, 160,100,20, &arm->flag, 0, 0, 0, 0, "Enable X-axis mirrored editing");
	uiDefButBitC(block, TOG, OB_DRAWXRAY,REDRAWVIEW3D, "X-Ray",			110,160,100,20, &ob->dtx, 0, 0, 0, 0, "Draw armature in front of solid objects");
	uiDefButBitI(block, TOG, ARM_AUTO_IK, B_DIFF, "Auto IK",			210,160,100,20, &arm->flag, 0, 0, 0, 0, "Adds temporal IK chains while grabbing Bones");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Display Options", 10,133,150,19, 0, 0, 0, 0, 0, "");
	
	/* layers */
	uiBlockBeginAlign(block);
	for(a=0; a<8; a++) {
		short dx= 18;
		but= uiDefButBitS(block, BUT_TOGDUAL, 1<<a, REDRAWVIEW3D, "", 10+a*dx, 115, dx, 15, &arm->layer, 0, 0, 0, 0, "Armature layer (Hold Ctrl for locking in a proxy instance)");
		uiButSetFunc(but, armature_layer_cb, &arm->layer, SET_INT_IN_POINTER(1<<a));
	}
	uiBlockBeginAlign(block);
	for(a=8; a<16; a++) {
		short dx= 18;
		but= uiDefButBitS(block, BUT_TOGDUAL, 1<<a, REDRAWVIEW3D, "", 18+a*dx, 115, dx, 15, &arm->layer, 0, 0, 0, 0, "Armature layer (Hold Ctrl for locking in a proxy instance)");
		uiButSetFunc(but, armature_layer_cb, &arm->layer, SET_INT_IN_POINTER(1<<a));
	}
	/* quite bad here, but I don't know a better place for copy... */
	if(ob->pose)
		ob->pose->proxy_layer= arm->layer;
	
	uiBlockBeginAlign(block);
	uiDefButI(block, ROW, REDRAWVIEW3D, "Octahedron", 10, 87,90,20, &arm->drawtype, 0, ARM_OCTA, 0, 0, "Draw bones as octahedra");
	uiDefButI(block, ROW, REDRAWVIEW3D, "Stick",	100, 87,55,20, &arm->drawtype, 0, ARM_LINE, 0, 0, "Draw bones as simple 2d lines with dots");
	uiDefButI(block, ROW, REDRAWVIEW3D, "B-Bone",	155, 87,70,20, &arm->drawtype, 0, ARM_B_BONE, 0, 0, "Draw bones as boxes, showing subdivision and b-splines");
	uiDefButI(block, ROW, REDRAWVIEW3D, "Envelope",	225, 87,85,20, &arm->drawtype, 0, ARM_ENVELOPE, 0, 0, "Draw bones as extruded spheres, showing deformation influence volume");

	uiDefButBitI(block, TOG, ARM_DRAWAXES, REDRAWVIEW3D, "Axes", 10, 67,75,20, &arm->flag, 0, 0, 0, 0, "Draw bone axes");
	uiDefButBitI(block, TOG, ARM_DRAWNAMES, REDRAWVIEW3D, "Names", 85,67,75,20, &arm->flag, 0, 0, 0, 0, "Draw bone names");
	uiDefButBitI(block, TOGN, ARM_NO_CUSTOM, REDRAWVIEW3D, "Shapes", 160,67,75,20, &arm->flag, 0, 0, 0, 0, "Draw custom bone shapes");
	uiDefButBitI(block, TOG, ARM_COL_CUSTOM, REDRAWVIEW3D, "Colors", 235,67,75,20, &arm->flag, 0, 0, 0, 0, "Draw custom bone colors (colors are set per Bone Group)");
	
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Deform Options", 10,40,150,20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, ARM_DEF_VGROUP, B_ARM_RECALCDATA, "Vertex Groups",	10, 20,100,20, &arm->deformflag, 0, 0, 0, 0, "Enable VertexGroups defining deform (not for Modifiers)");
	uiDefButBitS(block, TOG, ARM_DEF_ENVELOPE, B_ARM_RECALCDATA, "Envelopes",	110,20,100,20, &arm->deformflag, 0, 0, 0, 0, "Enable Bone Envelopes defining deform (not for Modifiers)");
	uiDefButBitS(block, TOG, ARM_DEF_QUATERNION, B_ARM_RECALCDATA, "Quaternion", 210,20,100,20, &arm->deformflag, 0, 0, 0, 0, "Enable deform rotation interpolation with Quaternions (not for Modifiers)");
	uiDefButBitI(block, TOG, ARM_RESTPOS, B_ARM_RECALCDATA,"Rest Position",		10,0,100,20, &arm->flag, 0, 0, 0, 0, "Show armature rest position, no posing possible");
	uiDefButBitI(block, TOG, ARM_DELAYDEFORM, REDRAWVIEW3D, "Delay Deform",		110,0,100,20, &arm->flag, 0, 0, 0, 0, "Don't deform children when manipulating bones in pose mode");
	uiDefButBitS(block, TOG, ARM_DEF_B_BONE_REST, B_ARM_RECALCDATA,"B-Bone Rest", 210,0,100,20, &arm->deformflag, 0, 0, 0, 0, "Make B-Bones deform already in rest position");
	uiBlockEndAlign(block);
}

static void editing_panel_armature_visuals(Object *ob, bArmature *arm)
{
	uiBlock	*block;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_armature_visuals", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Armature", "Editing");
	if(uiNewPanel(curarea, block, "Armature Visualisations", "Editing", 320, 0, 318, 204)==0) return;

	/* version patch for older files here (do_versions patch too complicated) */
	if ((arm->ghostsf == 0) || (arm->ghostef == 0)) {
		arm->ghostsf = CFRA - (arm->ghostep * arm->ghostsize);
		arm->ghostef = CFRA + (arm->ghostep * arm->ghostsize);
	}
	if ((arm->pathsf == 0) || (arm->pathef == 0)) {
		arm->pathsf = SFRA;
		arm->pathef = EFRA;
	}
	if ((arm->pathbc == 0) || (arm->pathac == 0)) {
		arm->pathbc = 15;
		arm->pathac = 15;
	}
	
	/* Ghost Drawing Options */
	uiDefBut(block, LABEL, 0, "Ghost Options", 10,180,150,20, 0, 0, 0, 0, 0, "");

	uiBlockBeginAlign(block);
		uiDefButS(block, MENU, REDRAWVIEW3D, "Ghosts %t|Around Current Frame %x0|In Range %x1|On Keyframes %x2", 
													10, 160, 150, 20, &arm->ghosttype, 0, 0, 0, 0, "Choose range of Ghosts to draw for current Action");	
		
		if (arm->ghosttype != ARM_GHOST_KEYS)
			uiDefButS(block, NUM, REDRAWVIEW3D, "GStep: ", 10,140,120,20, &arm->ghostsize, 1.0f, 20.0f, 0, 0, "How many frames between Ghost instances");
		else
			uiDefBut(block, LABEL, REDRAWVIEW3D, "GStep: N/A", 10,140,120,20, NULL, 0.0f, 0.0f, 0, 0, "How many frames between Ghost instances");
		uiDefButBitI(block, TOG, ARM_GHOST_ONLYSEL, REDRAWVIEW3D, "Sel", 130, 140, 30, 20, &arm->flag, 0, 0, 0, 0, "Only show Ghosts for selected bones");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	if (arm->ghosttype == ARM_GHOST_CUR) {
		/* range is around current frame */
		uiDefButS(block, NUM, REDRAWVIEW3D, "Ghost: ", 10,110,150,20, &arm->ghostep, 0.0f, 30.0f, 0, 0, "Draw Ghosts around current frame, for current Action");
	}
	else if (ELEM(arm->ghosttype, ARM_GHOST_RANGE, ARM_GHOST_KEYS)) {
		/* range is defined by start+end frame below */
		uiDefButI(block, NUM,REDRAWVIEW3D,"GSta:",10,110,150,20, &arm->ghostsf,1.0,MAXFRAMEF, 0, 0, "The start frame for Ghost display range");
		uiDefButI(block, NUM,REDRAWVIEW3D,"GEnd:",10,90,150,20, &arm->ghostef,arm->ghostsf,MAXFRAMEF, 0, 0, "The end frame for Ghost display range");	
	}
	uiBlockEndAlign(block);
	
	/* Bone Path Drawing Options */
	uiDefBut(block, LABEL, 0, "Bone Paths Drawing:", 165,180,170,20, 0, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
		uiDefButS(block, NUM, REDRAWVIEW3D, "PStep:",170,160,80,20, &arm->pathsize,1,100, 10, 50, "Frames between highlighted points on bone path");
		uiDefButBitS(block, TOG, ARM_PATH_FNUMS, REDRAWVIEW3D, "Frame Nums", 250, 160, 80, 20, &arm->pathflag, 0, 0, 0, 0, "Show frame numbers on path");
		
		uiDefButBitS(block, TOG, ARM_PATH_KFRAS, REDRAWVIEW3D, "Show Keys", 170, 140, 80, 20, &arm->pathflag, 0, 0, 0, 0, "Show key frames on path");
		uiDefButBitS(block, TOG, ARM_PATH_KFNOS, REDRAWVIEW3D, "Keyframe Nums", 250, 140, 80, 20, &arm->pathflag, 0, 0, 0, 0, "Show frame numbers of key frames on path");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, ARM_PATH_ACFRA, REDRAWVIEW3D, "Around Current Frame", 170, 110, 160, 20, &arm->pathflag, 0, 0, 0, 0, "Only show Bone Path around the current frame");
		
		/* only show extra ranges when needed */
		if (arm->pathflag & ARM_PATH_ACFRA) {
			uiDefButI(block, NUM, REDRAWVIEW3D,"PPre:",170,90,80,20, &arm->pathbc, 1.0, MAXFRAMEF/2, 0, 0, "The number of frames before current frame for Bone Path display range");
			uiDefButI(block, NUM, REDRAWVIEW3D,"PPost:",250,90,80,20, &arm->pathac, 1.0, MAXFRAMEF/2, 0, 0, "The number of frames after current frame for Bone Path display range");	
		}
	uiBlockEndAlign(block);
	
	/* Bone Path Calculation Options */
	uiDefBut(block, LABEL, 0, "Bone Paths Calc.", 10,50,170,20, 0, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_ARM_CALCPATHS, "Calculate Paths", 10,30,155,20, 0, 0, 0, 0, 0, "(Re)calculates the paths of the selected bones");
		uiDefBut(block, BUT, B_ARM_CLEARPATHS, "Clear Paths", 10,10,155,20, 0, 0, 0, 0, 0, "Clears bone paths of the selected bones");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, ARM_PATH_HEADS, REDRAWVIEW3D, "Bone-Head Path", 170, 30, 160, 20, &arm->pathflag, 0, 0, 0, 0, "Calculate the Path travelled by the Bone's Head instead of Tail");
		uiDefButI(block, NUM,REDRAWVIEW3D,"PSta:",170,10,80,20, &arm->pathsf, 1.0, MAXFRAMEF, 0, 0, "The start frame for Bone Path display range");
		uiDefButI(block, NUM,REDRAWVIEW3D,"PEnd:",250,10,80,20, &arm->pathef, arm->pathsf, MAXFRAMEF, 0, 0, "The end frame for Bone Path display range");
	uiBlockEndAlign(block);
}

/* autocomplete callback for editbones */
static void autocomplete_editbone(char *str, void *arg_v)
{
	if(G.obedit==NULL) return;
	
	/* search if str matches the beginning of an ID struct */
	if(str[0]) {
		AutoComplete *autocpl= autocomplete_begin(str, 32);
		EditBone *ebone;
		
		for (ebone=G.edbo.first; ebone; ebone=ebone->next)
			if(ebone->name!=str)
				autocomplete_do_name(autocpl, ebone->name);

		autocomplete_end(autocpl, str);
	}
}

static void editing_panel_armature_bones(Object *ob, bArmature *arm)
{
	uiBlock		*block;
	uiBut		*but;
	EditBone	*curBone;
	char		*boneString=NULL;
	int			by=180;
	int			index, a;

	/* Draw the bone name block */

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_armature_bones", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Armature Bones", "Editing", 640, 0, 318, 204)==0) return;

	/* this is a variable height panel, newpanel doesnt force new size on existing panels */
	/* so first we make it default height */
	uiNewPanelHeight(block, 204);


	uiDefBut(block, LABEL, 0, "Selected Bones", 0,by,158,18, 0, 0, 0, 0, 0, "Only show in Armature Editmode");
	by-=20;
	for (curBone=G.edbo.first, index=0; curBone; curBone=curBone->next, index++) {
		if ((curBone->flag & BONE_SELECTED) && (curBone->layer & arm->layer)) {
			/*	Bone naming button */
			but=uiDefBut(block, TEX, REDRAWVIEW3D, "BO:", -10,by,117,18, curBone->name, 0, 31, 0, 0, "Change the bone name");
			uiButSetFunc(but, validate_editbonebutton_cb, curBone, NULL);
			uiButSetCompleteFunc(but, autocomplete_editbone, (void *)OBACT);
			
			uiDefBut(block, LABEL, 0, "child of", 107,by,73,18, NULL, 0.0, 0.0, 0.0, 0.0, "");
			
			boneString = MEM_mallocN((BLI_countlist(&G.edbo) * 64)+64, "Bone str");
			build_bonestring (boneString, curBone);
			
			curBone->parNr = editbone_to_parnr(curBone->parent);
			but = uiDefButI(block, MENU,REDRAWVIEW3D, boneString, 180,by,120,18, &curBone->parNr, 0.0, 0.0, 0.0, 0.0, "Parent");
			/* last arg NULL means button will put old string there */
			uiButSetFunc(but, parnr_to_editbone_cb, curBone, NULL);
			
			MEM_freeN(boneString);
			
			if (curBone->parent) {
				/* Connect to parent flag */
				but=uiDefButBitI(block, TOG, BONE_CONNECTED, B_ARM_RECALCDATA, "Con", 300,by,32,18, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "Connect this Bone to Parent");
				uiButSetFunc(but, attach_bone_to_parent_cb, curBone, NULL);
			}
			else {
				/* No cyclic-offset flag */
				uiDefButBitI(block, TOGN, BONE_NO_CYCLICOFFSET, B_ARM_RECALCDATA, "Offs", 300,by,31,18, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "Apply cyclic-offset to this Bone");
			}
			
			/* Segment, dist and weight buttons */
			uiBlockBeginAlign(block);
			uiDefButS(block, NUM, B_ARM_RECALCDATA, "Segm: ", -10,by-19,117,18, &curBone->segments, 1.0, 32.0, 0.0, 0.0, "Subdivisions for B-bones");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "Dist:", 110, by-19, 105, 18, &curBone->dist, 0.0, 1000.0, 10.0, 0.0, "Bone deformation distance");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "Weight:", 225, by-19,105, 18, &curBone->weight, 0.0F, 1000.0F, 10.0F, 0.0F, "Bone deformation weight");
			
			/* bone types */
			uiDefButBitI(block, TOG, BONE_HINGE, B_ARM_RECALCDATA, "Hinge",		-10,by-38,80,18, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Don't inherit rotation or scale from parent Bone");
			uiDefButBitI(block, TOG, BONE_NO_SCALE, B_ARM_RECALCDATA, "S",		70,by-38,20,18, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Don't inherit rotation or scale from parent Bone");
			uiDefButBitI(block, TOGN, BONE_NO_DEFORM, B_ARM_RECALCDATA, "Deform",	90, by-38, 80, 18, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "Indicate if Bone deforms geometry");
			uiDefButBitI(block, TOG, BONE_MULT_VG_ENV, B_ARM_RECALCDATA, "Mult", 170,by-38,80,18, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Multiply Bone Envelope with VertexGroup");
			uiDefButBitI(block, TOG, BONE_HIDDEN_A, REDRAWVIEW3D, "Hide",	250,by-38,80,18, &curBone->flag, 0, 0, 0, 0, "Toggles display of this bone in Edit Mode");
			
			/* layers */
			uiBlockBeginAlign(block);
			for(a=0; a<8; a++) {
				short dx= 21;
				but= uiDefButBitS(block, TOG, 1<<a, REDRAWVIEW3D, "", -10+a*dx, by-57, dx, 15, &curBone->layer, 0, 0, 0, 0, "Armature layer that bone exists on");
				uiButSetFunc(but, armature_layer_cb, &curBone->layer, SET_INT_IN_POINTER(1<<a));
			}
			uiBlockBeginAlign(block);
			for(a=8; a<16; a++) {
				short dx= 21;
				but= uiDefButBitS(block, TOG, 1<<a, REDRAWVIEW3D, "", -6+a*dx, by-57, dx, 15, &curBone->layer, 0, 0, 0, 0, "Armature layer that bone exists on");
				uiButSetFunc(but, armature_layer_cb, &curBone->layer, SET_INT_IN_POINTER(1<<a));
			}
			
			uiBlockEndAlign(block);
			by-=80;
			
			if(by < -200) break;	// for time being... extreme long panels are very slow
		}
	}

	if(by<0) {
		uiNewPanelHeight(block, 204 - by);
	}

}

/* sets warning popup for buttons, and returns 1 for protected proxy posechannels */
static int ob_arm_bone_pchan_lock(Object *ob, bArmature *arm, Bone *bone, bPoseChannel *pchan)
{
	/* ob lib case is already set globally */
	if(ob->id.lib)
		return 0;
	if(arm->id.lib) {
		if(pchan==NULL)
			uiSetButLock(1, ERROR_LIBDATA_MESSAGE);
		else if(ob->proxy && bone->layer & arm->layer_protected) {
			uiSetButLock(1, "Can't edit protected proxy channel");
			return 1;
		}
		else
			uiClearButLock();
	}
	return 0;
}

static void editing_panel_pose_bones(Object *ob, bArmature *arm)
{
	uiBlock		*block;
	uiBut		*but;
	bPoseChannel *pchan;
	Bone		*curBone;
	int			by, a;
	int			index, zerodof, zerolimit;
	char 		*menustr;
	
	/* Draw the bone name block */
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_pose_bones", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Armature Bones", "Editing", 640, 0, 318, 204)==0) return;
	
	/* this is a variable height panel, newpanel doesnt force new size on existing panels */
	/* so first we make it default height */
	uiNewPanelHeight(block, 204);
	
	uiDefBut(block, LABEL, 0, "Selected Bones", 0,180,158,18, 0, 0, 0, 0, 0, "Only show in Armature Editmode/Posemode");
	by= 160;
	
	for (pchan=ob->pose->chanbase.first, index=0; pchan; pchan=pchan->next, index++){
		curBone= pchan->bone;
		if ((curBone->flag & BONE_SELECTED) && (curBone->layer & arm->layer)) {
			if(ob_arm_bone_pchan_lock(ob, arm, curBone, pchan))
				uiDefBut(block, LABEL, 0, "Proxy Locked", 160, 180,150,18, NULL, 1, 0, 0, 0, "");
			
			/* Bone naming button */
			uiBlockBeginAlign(block);
			but=uiDefBut(block, TEX, REDRAWVIEW3D, "BO:",		-10,by,117,19, curBone->name, 0, 24, 0, 0, "Change the bone name");
			uiButSetFunc(but, validate_posebonebutton_cb, curBone, NULL);
			uiButSetCompleteFunc(but, autocomplete_bone, (void *)ob);
			
			/* Bone custom drawing */
			menustr= build_posegroups_menustr(ob->pose, 0);
			uiDefButS(block, MENU,REDRAWVIEW3D, menustr, 107,by,105,19, &pchan->agrp_index, 0, 0.0, 0.0, 0.0, "Change the Pose Group this Bone belongs to");
			MEM_freeN(menustr);
			
			ob_arm_bone_pchan_lock(ob, arm, curBone, pchan);
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, REDRAWVIEW3D, "OB:",		220,by,90,19, &pchan->custom, "Object that defines custom draw type for this Bone");
			ob_arm_bone_pchan_lock(ob, arm, curBone, NULL);
			
			uiDefButBitI(block, TOG, BONE_DRAWWIRE, B_ARM_RECALCDATA, "W",			309,by,21,19, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Custom shape of this Bone should always be drawn as a wireframe");
			
			/* Segment, ease in/out buttons */
			uiBlockBeginAlign(block);
			uiDefButS(block, NUM, B_ARM_RECALCDATA, "Segm: ",  -10,by-19,117,19, &curBone->segments, 1.0, 32.0, 0.0, 0.0, "Subdivisions for B-bones");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "In:",		107, by-19,105, 19, &curBone->ease1, 0.0, 2.0, 10.0, 0.0, "First length of Bezier handle");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "Out:",		220, by-19, 110, 19, &curBone->ease2, 0.0, 2.0, 10.0, 0.0, "Second length of Bezier handle");
			
			/* bone types */
			uiDefButBitI(block, TOG, BONE_HINGE, B_ARM_RECALCDATA, "Hinge",			-10,by-38,80,19, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Don't inherit rotation or scale from parent Bone");
			uiDefButBitI(block, TOG, BONE_NO_SCALE, B_ARM_RECALCDATA, "S",			70,by-38,20,19, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Don't inherit scale from parent Bone");
			uiDefButBitI(block, TOGN, BONE_NO_DEFORM, B_ARM_RECALCDATA, "Deform",	90, by-38, 80, 19, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "Indicate if Bone deforms geometry");
			uiDefButBitI(block, TOG, BONE_MULT_VG_ENV, B_ARM_RECALCDATA, "Mult",	170,by-38,80,19, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Multiply Bone Envelope with VertexGroup");
			uiDefButBitI(block, TOG, BONE_MULT_VG_ENV, B_ARM_RECALCDATA, "Hide",	250,by-38,80,19, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Toggles display of this bone in Edit Mode");
			
			/* layers */
			uiBlockBeginAlign(block);
			for(a=0; a<8; a++) {
				short dx= 21;
				but= uiDefButBitS(block, TOG, 1<<a, REDRAWVIEW3D, "", -10+a*dx, by-57, dx, 15, &curBone->layer, 0, 0, 0, 0, "Armature layer that bone exists on");
				uiButSetFunc(but, armature_layer_cb, &curBone->layer, SET_INT_IN_POINTER(1<<a));
			}
			uiBlockBeginAlign(block);
			for(a=8; a<16; a++) {
				short dx= 21;
				but= uiDefButBitS(block, TOG, 1<<a, REDRAWVIEW3D, "", -6+a*dx, by-57, dx, 15, &curBone->layer, 0, 0, 0, 0, "Armature layer that bone exists on");
				uiButSetFunc(but, armature_layer_cb, &curBone->layer, SET_INT_IN_POINTER(1<<a));
			}
			uiBlockEndAlign(block);
			
			by-= 20;
			
			ob_arm_bone_pchan_lock(ob, arm, curBone, pchan);

			/* DOFs only for IK chains */
			zerodof = 1;
			zerolimit = 1;
			if(pose_channel_in_IK_chain(ob, pchan)) {
				
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, BONE_IK_NO_XDOF, B_ARM_RECALCDATA, "Lock X Rot", -10,by-60,114,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Disable X DoF for IK");
				if ((pchan->ikflag & BONE_IK_NO_XDOF)==0) {
					uiDefButF(block, NUM, B_ARM_RECALCDATA, "Stiff X:", -10, by-80, 114, 19, &pchan->stiffness[0], 0.0, 0.99, 1.0, 0.0, "Resistance to bending for X axis");
					uiDefButBitS(block, TOG, BONE_IK_XLIMIT, B_ARM_RECALCDATA, "Limit X", -10,by-100,114,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Limit rotation over X axis");
					if ((pchan->ikflag & BONE_IK_XLIMIT)) {
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Min X:", -10, by-120, 114, 19, &pchan->limitmin[0], -180.0, 0.0, 1000, 1, "Minimum X limit");
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Max X:", -10, by-140, 114, 19, &pchan->limitmax[0], 0.0, 180.0f, 1000, 1, "Maximum X limit");
						zerolimit = 0;
					}
					zerodof = 0;
				}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, BONE_IK_NO_YDOF, B_ARM_RECALCDATA, "Lock Y Rot", 104,by-60,113,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Disable Y DoF for IK");
				if ((pchan->ikflag & BONE_IK_NO_YDOF)==0) {
					uiDefButF(block, NUM, B_ARM_RECALCDATA, "Stiff Y:", 104, by-80, 114, 19, &pchan->stiffness[1], 0.0, 0.99, 1.0, 0.0, "Resistance to twisting over Y axis");
					uiDefButBitS(block, TOG, BONE_IK_YLIMIT, B_ARM_RECALCDATA, "Limit Y", 104,by-100,113,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Limit rotation over Y axis");
					if ((pchan->ikflag & BONE_IK_YLIMIT)) {
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Min Y:", 104, by-120, 113, 19, &pchan->limitmin[1], -180.0, 0.0, 1000, 1, "Minimum Y limit");
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Max Y:", 104, by-140, 113, 19, &pchan->limitmax[1], 0.0, 180.0, 1000, 1, "Maximum Y limit");
						zerolimit = 0;
					}
					zerodof = 0;
				}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, BONE_IK_NO_ZDOF, B_ARM_RECALCDATA, "Lock Z Rot", 217,by-60,113,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Disable Z DoF for IK");
				if ((pchan->ikflag & BONE_IK_NO_ZDOF)==0) {
					uiDefButF(block, NUM, B_ARM_RECALCDATA, "Stiff Z:", 217, by-80, 114, 19, &pchan->stiffness[2], 0.0, 0.99, 1.0, 0.0, "Resistance to bending for Z axis");
					uiDefButBitS(block, TOG, BONE_IK_ZLIMIT, B_ARM_RECALCDATA, "Limit Z", 217,by-100,113,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Limit rotation over Z axis");
					if ((pchan->ikflag & BONE_IK_ZLIMIT)) {
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Min Z:", 217, by-120, 113, 19, &pchan->limitmin[2], -180.0, 0.0, 1000, 1, "Minimum Z limit");
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Max Z:", 217, by-140, 113, 19, &pchan->limitmax[2], 0.0, 180.0, 1000, 1, "Maximum Z limit");
						zerolimit = 0;
					}
					zerodof = 0;
				}
				uiBlockEndAlign(block);
				
				by -= (zerodof)? 82: (zerolimit)? 122: 162;

				uiDefButF(block, NUM, B_ARM_RECALCDATA, "Stretch:", -10, by, 113, 19, &pchan->ikstretch, 0.0, 1.0, 1.0, 0.0, "Allow scaling of the bone for IK");

				by -= 20;
			}
			else {
				but= uiDefButBitS(block, TOG, POSE_STRIDE, B_ARM_STRIDE, "Stride Root", -10, by-60, 113, 19, &pchan->flag, 0.0, 0.0, 0, 0, "Set this PoseChannel to define the Stride distance");
				uiButSetFunc(but, validate_stridebutton_cb, pchan, NULL);
				
				uiDefBut(block, LABEL, 0, "(DoF only for IK chains)", 110,by-60, 190, 19, 0, 0, 0, 0, 0, "");
				by -= 82;
			}
			
				
			if(by < -200) break;	// for time being... extreme long panels are very slow
		}
	}
	
	if(by<0) {
		uiNewPanelHeight(block, 204 - by);
	}
	
}


/* *************************** MESH ******************************** */

/* from this object to all objects with same ob->data */
static void copy_linked_vgroup_channels(Object *ob)
{
	Base *base;
	
	for(base=FIRSTBASE; base; base= base->next) {
		if(base->object->type==ob->type) {
			if(base->object!=ob && base->object->data==ob->data) {
				BLI_freelistN(&base->object->defbase);
				duplicatelist(&base->object->defbase, &ob->defbase);
				base->object->actdef= ob->actdef;
				DAG_object_flush_update(G.scene, base->object, OB_RECALC_DATA);
			}
		}
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

void do_vgroupbuts(unsigned short event)
{
	Object *ob= OBACT;
	
	switch(event) {
		case B_NEWVGROUP:
			add_defgroup (ob);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			scrarea_queue_winredraw(curarea);
			allqueue(REDRAWOOPS, 0);
			
			break;
		case B_DELVGROUP:
			if ((G.obedit) && (G.obedit == ob)) {
				del_defgroup (ob);
			} else {
				del_defgroup_in_object_mode (ob);
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			}
			allqueue (REDRAWVIEW3D, 1);
			allqueue(REDRAWOOPS, 0);
			allqueue(REDRAWBUTSEDIT, 1);
			BIF_undo_push("Delete vertex group");
			break;
		case B_ASSIGNVGROUP:
			assign_verts_defgroup ();
			allqueue (REDRAWVIEW3D, 1);
			BIF_undo_push("Assign to vertex group");
			break;
		case B_REMOVEVGROUP:
			remove_verts_defgroup (0);
			allqueue (REDRAWVIEW3D, 1);
			allqueue(REDRAWOOPS, 0);
			BIF_undo_push("Remove from vertex group");
			break;
		case B_SELVGROUP:
			sel_verts_defgroup(1);
			allqueue (REDRAWVIEW3D, 1);
			allqueue(REDRAWOOPS, 0);
			countall();
			break;
		case B_DESELVGROUP:
			sel_verts_defgroup(0);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue (REDRAWVIEW3D, 1);
			allqueue(REDRAWOOPS, 0);
			countall();
			break;
		case B_LINKEDVGROUP:
			copy_linked_vgroup_channels(ob);
			break;
		case B_COPYVGROUP:
			duplicate_defgroup (ob);
			scrarea_queue_winredraw (curarea);
			allqueue (REDRAWOOPS, 0);
			break;
	}
}

void do_meshbuts(unsigned short event)
{
	Object *ob;
	Mesh *me;
	MCol *mcol;
	EditMesh *em= G.editMesh;
	float fac;
	int count; /* store num of changes made to see if redraw & undo are needed*/
	int layernum;
	short randfac;
	
	ob= OBACT;
	if(ob && ob->type==OB_MESH) {

		me= get_mesh(ob);
		if(me==NULL) return;
		
		switch(event) {
		case B_DELSTICKY:
			if(me->msticky) {
				CustomData_free_layer_active(&me->vdata, CD_MSTICKY, me->totvert);
				me->msticky= NULL;
				BIF_undo_push("Delete Sticky");
			}
			allqueue(REDRAWBUTSEDIT, 0);
			break;
		case B_MAKESTICKY:
			RE_make_sticky();
			BIF_undo_push("Make Sticky");
			allqueue(REDRAWBUTSEDIT, 0);
			break;

		case B_NEWMCOL:
			if(G.obedit) {
				layernum= CustomData_number_of_layers(&em->fdata, CD_MCOL);
				EM_add_data_layer(&em->fdata, CD_MCOL);
				CustomData_set_layer_active(&em->fdata, CD_MCOL, layernum);
			}
			else if(me) {
				mcol= me->mcol;
				layernum= CustomData_number_of_layers(&me->fdata, CD_MCOL);

				if(mcol)
					CustomData_add_layer(&me->fdata, CD_MCOL, CD_DUPLICATE,
					                     mcol, me->totface);
				else
					CustomData_add_layer(&me->fdata, CD_MCOL, CD_CALLOC,
					                     NULL, me->totface);

				CustomData_set_layer_active(&me->fdata, CD_MCOL, layernum);
				mesh_update_customdata_pointers(me);

				if(!mcol)
					shadeMeshMCol(ob, me);
			}
			
			if (me->mr) multires_load_cols(me);

			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			BIF_undo_push("New Vertex Color");
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSEDIT, 0);
			break;
		case B_SETMCOL:
			if (G.obedit || me) {
				CustomData *fdata= (G.obedit)? &em->fdata: &me->fdata;
				CustomData_set_layer_active(fdata, CD_MCOL, actmcol-1);
				mesh_update_customdata_pointers(me);

				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
				BIF_undo_push("Set Active Vertex Color");
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWBUTSEDIT, 0);
			}
			break;
		case B_SETMCOL_RND:
			if (G.obedit || me) {
				CustomData *fdata= (G.obedit)? &em->fdata: &me->fdata;
				CustomData_set_layer_render(fdata, CD_MCOL, actmcol_rnd-1);
				
				BIF_undo_push("Set Render Vertex Color");
				allqueue(REDRAWBUTSEDIT, 0);
			}
			break;

		case B_NEWTFACE:
			if(me && me->mr) {
				layernum= CustomData_number_of_layers(&me->fdata, CD_MTFACE);
				multires_add_layer(me, &me->mr->fdata, CD_MTFACE, layernum);
				multires_level_to_editmesh(ob, me, 0);
				multires_finish_mesh_update(ob);
			}
			else if(G.obedit) {
				layernum= CustomData_number_of_layers(&em->fdata, CD_MTFACE);
				EM_add_data_layer(&em->fdata, CD_MTFACE);
				CustomData_set_layer_active(&em->fdata, CD_MTFACE, layernum);
			}
			else if(me) {
				layernum= CustomData_number_of_layers(&me->fdata, CD_MTFACE);
				if(me->mtface)
					CustomData_add_layer(&me->fdata, CD_MTFACE, CD_DUPLICATE,
					                     me->mtface, me->totface);
				else
					CustomData_add_layer(&me->fdata, CD_MTFACE, CD_DEFAULT,
					                     NULL, me->totface);

				CustomData_set_layer_active(&me->fdata, CD_MTFACE, layernum);
				mesh_update_customdata_pointers(me);
			}

			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			BIF_undo_push("New UV Texture");
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWIMAGE, 0);
			break;
		case B_SETTFACE:
			if (G.obedit || me) {
				CustomData *fdata= (G.obedit)? &em->fdata: &me->fdata;

				CustomData_set_layer_active(fdata, CD_MTFACE, acttface-1);
				mesh_update_customdata_pointers(me);
				
				/* Update first-level face data in multires */
				if(me && me->mr && me->mr->current != 1)
					CustomData_set_layer_active(&me->mr->fdata, CD_MTFACE, acttface-1);

				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
				BIF_undo_push("Set Active UV Texture");
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWIMAGE, 0);
			}
			break;
		case B_SETTFACE_RND:
			if (G.obedit || me) {
				CustomData *fdata= (G.obedit)? &em->fdata: &me->fdata;
				CustomData_set_layer_render(fdata, CD_MTFACE, acttface_rnd-1);
				BIF_undo_push("Set Render UV Texture");
				allqueue(REDRAWBUTSEDIT, 0);
			}
			break;
			
		case B_FLIPNORM:
			if(G.obedit) {
				flip_editnormals();
				DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
				BIF_undo_push("Flip Normals");
				allqueue(REDRAWVIEW3D, 0);
			}
			break;
		}
	}
	if(G.obedit==NULL || (G.obedit->type!=OB_MESH)) return;

	switch(event) {
	case B_SPIN:
		if( select_area(SPACE_VIEW3D)) spin_mesh(G.scene->toolsettings->step, G.scene->toolsettings->degr, 0, 0);
		break;
	case B_SPINDUP:
		if( select_area(SPACE_VIEW3D)) spin_mesh(G.scene->toolsettings->step, G.scene->toolsettings->degr, 0, 1);
		break;
	case B_EXTR:
		G.f |= G_DISABLE_OK;
		if( select_area(SPACE_VIEW3D)) extrude_mesh();
		G.f -= G_DISABLE_OK;
		break;
	case B_SCREW:
		if( select_area(SPACE_VIEW3D)) screw_mesh(G.scene->toolsettings->step, G.scene->toolsettings->turn);
		break;
	case B_EXTREP:
		if( select_area(SPACE_VIEW3D)) extrude_repeat_mesh(G.scene->toolsettings->step, G.scene->toolsettings->extr_offs);
		break;
	case B_SPLIT:
		G.f |= G_DISABLE_OK;
		split_mesh();
		G.f -= G_DISABLE_OK;
		break;
	case B_REMDOUB:
		count= removedoublesflag(1, 0, G.scene->toolsettings->doublimit);
		notice("Removed: %d", count);
		if (count) { /* only undo and redraw if an action is taken */
			countall ();
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
			BIF_undo_push("Rem Doubles");
		}
		break;
	case B_SUBDIV:
		waitcursor(1);
		esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag,1,0);
		countall();
		waitcursor(0);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Subdivide");
		break;
	case B_FRACSUBDIV:
		randfac= 10;
		if(button(&randfac, 1, 100, "Rand fac:")==0) return;
		waitcursor(1);
		fac= -( (float)randfac )/100;
		esubdivideflag(1, fac, G.scene->toolsettings->editbutflag,1,0);
		countall();
		waitcursor(0);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Fractal Subdivide");
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
	case B_DRAWEDGES:
		G.f &= ~G_DRAWCREASES;
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_DRAWCREASES:
		G.f &= ~G_DRAWEDGES;
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	//~ case B_DRAWBWEIGHTS:
		//~ allqueue(REDRAWBUTSEDIT, 0);
		//~ allqueue(REDRAWVIEW3D, 0);
		//~ break;
	case B_JOINTRIA:
		join_triangles();
		break;
	case B_GEN_SKELETON:
		generateSkeleton();
		break;
	}

	/* WATCH IT: previous events only in editmode! */
}

static void editing_panel_mesh_tools(Object *ob, Mesh *me)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_tools", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Mesh Tools", "Editing", 640, 0, 318, 254)==0) return;

	uiBlockBeginAlign(block);
	//uiDefButBitS(block, TOG, B_AUTOFGON, 0, "FGon",		    10,195,30,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Causes 'Subdivide' To create FGon on inner edges where possible");
	uiDefButBitS(block, TOG, B_BEAUTY, 0, "Beauty",		    10,195,53,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Causes 'Subdivide' to split faces in halves instead of quarters using long edges unless 'Short' is selected");
	uiDefButBitS(block, TOG, B_BEAUTY_SHORT, 0, "Short",		    63,195,52,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "If Beauty is set, 'Subdivide' splits faces in halves using short edges");

	uiDefBut(block, BUT,B_SUBDIV,"Subdivide",		115,195,105,19, 0, 0, 0, 0, 0, "Splits selected faces into halves or quarters");

	uiDefButS(block, MENU, B_DIFF, "Corner Cut Type %t|Path %x0|Innervert %x1|Fan %x2", 
												220, 195, 105, 19, &G.scene->toolsettings->cornertype , 0, 0, 0, 0, "Choose Quad Corner Cut Type");	

	uiDefBut(block, BUT,B_VERTEXNOISE,"Noise",		10,175,78,19, 0, 0, 0, 0, 0, "Use vertex coordinate as texture coordinate");
	uiDefBut(block, BUT,B_HASH,"Hash",				88,175,78,19, 0, 0, 0, 0, 0, "Randomizes selected vertex sequence data");
	uiDefBut(block, BUT,B_XSORT,"Xsort",			166,175,78,19, 0, 0, 0, 0, 0, "Sorts selected vertex data in the X direction");
	uiDefBut(block, BUT,B_FRACSUBDIV, "Fractal",	244,175,81,19, 0, 0, 0, 0, 0, "Subdivides selected faces with a random factor");


	uiDefBut(block, BUT,B_TOSPHERE,"To Sphere",		10,155,78,19, 0, 0, 0, 0, 0, "Moves selected vertices outwards into a spherical shape");
	uiDefBut(block, BUT,B_VERTEXSMOOTH,"Smooth",	88,155,78,19, 0, 0, 0, 0, 0, "Flattens angles of selected faces");
	uiDefBut(block, BUT,B_SPLIT,"Split",			166,155,78,19, 0, 0, 0, 0, 0, "Splits selected vertices to separate sub-mesh");
	uiDefBut(block, BUT,B_FLIPNORM,"Flip Normals",	244,155,81,19, 0, 0, 0, 0, 0, "Toggles the direction of the selected face's normals");
	
	uiDefBut(block, BUT,B_REMDOUB,"Rem Doubles",	10,135,78,19, 0, 0, 0, 0, 0, "Removes duplicates from selected vertices");
	uiDefButF(block, NUM, B_DIFF, "Limit:",			88,135,117,19, &G.scene->toolsettings->doublimit, 0.0001, 1.0, 10, 0, "Specifies the max distance 'Rem Doubles' will consider vertices as 'doubled'");
	uiDefButF(block, NUM, B_DIFF, "Threshold:",			205,135,120,19, &G.scene->toolsettings->select_thresh, 0.0001, 1.0, 10, 0, "Tolerence for the 'Select Group' tool (Shift+G) and 'Knife Exact' (vertex snap).");
	uiBlockEndAlign(block);

	uiDefBut(block, BUT,B_EXTR,"Extrude",			10,105,315,24, 0, 0, 0, 0, 0, "Converts selected edges to faces and selects the new vertices");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_SPIN, "Spin",			10,75,100,24, 0, 0, 0, 0, 0, "Extrudes the selected vertices in a circle around the cursor in the indicated viewport");
	uiDefBut(block, BUT,B_SPINDUP,"Spin Dup",		110,75,100,24, 0, 0, 0, 0, 0, "Creates copies of the selected vertices in a circle around the cursor in the indicated viewport");
	uiDefBut(block, BUT,B_SCREW,"Screw",			210,75,115,24, 0, 0, 0, 0, 0, "Activates the screw tool");  // Bish - This could use some more definition
	
	uiDefButF(block, NUM, B_DIFF, "Degr:",			10,55,100,19, &G.scene->toolsettings->degr,-360.0,360.0, 1000, 0, "Specifies the number of degrees 'Spin' revolves");
	uiDefButS(block, NUM, B_DIFF, "Steps:",			110,55,100,19, &G.scene->toolsettings->step,1.0,180.0, 0, 0, "Specifies the total number of 'Spin' slices");
	uiDefButS(block, NUM, B_DIFF, "Turns:",			210,55,115,19, &G.scene->toolsettings->turn,1.0,360.0, 0, 0, "Specifies the number of revolutions the screw turns");
	uiDefButBitS(block, TOG, B_KEEPORIG, B_DIFF, "Keep Original",10,35,200,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Keeps a copy of the original vertices and faces after executing tools");
	uiDefButBitS(block, TOG, B_CLOCKWISE, B_DIFF, "Clockwise",	210,35,115,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Specifies the direction for 'Screw' and 'Spin'");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_EXTREP, "Extrude Dup",	10,10,150,19, 0, 0, 0, 0, 0, "Creates copies of the selected vertices in a straight line away from the current viewport");
	uiDefButF(block, NUM, B_DIFF, "Offset:",		160,10,165,19, &G.scene->toolsettings->extr_offs, 0.01, 100.0, 100, 0, "Sets the distance between each copy for 'Extrude Dup'");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_JOINTRIA, "Join Triangles", 10, -20, 120, 19, 0, 0, 0, 0, 0, "Convert selected triangles to Quads");
	uiDefButF(block, NUM, B_DIFF, "Threshold", 130, -20, 195, 19, &G.scene->toolsettings->jointrilimit, 0.0, 1.0, 5, 0, "Conversion threshold for complex islands");
	uiDefButBitS(block, TOG, B_JOINTRIA_UV, 0, "Delimit UVs",  10, -40, 78, 19, &G.scene->toolsettings->editbutflag, 0,0,0,0, "Join pairs only where UVs match");
	uiDefButBitS(block, TOG, B_JOINTRIA_VCOL, 0, "Delimit Vcol", 90, -40, 78, 19, &G.scene->toolsettings->editbutflag, 0,0,0,0, "Join pairs only where Vcols match"); 
	uiDefButBitS(block, TOG, B_JOINTRIA_SHARP, 0, "Delimit Sharp", 170, -40, 78, 19, &G.scene->toolsettings->editbutflag, 0,0,0,0, "Join pairs only where edge is not sharp"); 
	uiDefButBitS(block, TOG, B_JOINTRIA_MAT, 0, "Delimit Mat", 250, -40, 74, 19, &G.scene->toolsettings->editbutflag, 0,0,0,0, "Join pairs only where material matches");
	uiBlockEndAlign(block);

	
}

static void verify_vertexgroup_name_func(void *datav, void *data2_unused)
{
	unique_vertexgroup_name((bDeformGroup*)datav, OBACT);
}

static void skgen_reorder(void *option, void *arg2)
{
	char tmp;
	switch (GET_INT_FROM_POINTER(option))
	{
		case 0:
			tmp = G.scene->toolsettings->skgen_subdivisions[0];
			G.scene->toolsettings->skgen_subdivisions[0] = G.scene->toolsettings->skgen_subdivisions[1];
			G.scene->toolsettings->skgen_subdivisions[1] = tmp;
			break;
		case 1:
			tmp = G.scene->toolsettings->skgen_subdivisions[2];
			G.scene->toolsettings->skgen_subdivisions[2] = G.scene->toolsettings->skgen_subdivisions[1];
			G.scene->toolsettings->skgen_subdivisions[1] = tmp;
			break;
		case 2:
			tmp = G.scene->toolsettings->skgen_subdivisions[0];
			G.scene->toolsettings->skgen_subdivisions[0] = G.scene->toolsettings->skgen_subdivisions[2];
			G.scene->toolsettings->skgen_subdivisions[2] = G.scene->toolsettings->skgen_subdivisions[1];
			G.scene->toolsettings->skgen_subdivisions[1] = tmp;
			break;
	}
}

static void editing_panel_mesh_skgen(Object *ob, Mesh *me)
{
	uiBlock *block;
	uiBut *but;
	int i;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_skgen", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Skeleton Generator", "Editing", 960, 0, 318, 204)==0) return;
	
	uiDefBut(block, BUT, B_GEN_SKELETON, "Generate Skeleton",			1025,170,250,19, 0, 0, 0, 0, 0, "Generate Skeleton from Mesh");

	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, B_DIFF, "Resolution:",							1025,150,250,19, &G.scene->toolsettings->skgen_resolution,10.0,1000.0, 0, 0,		"Specifies the resolution of the graph's embedding");
	uiDefButBitS(block, TOG, SKGEN_FILTER_INTERNAL, B_DIFF, "Filter In",	1025,130, 83,19, &G.scene->toolsettings->skgen_options, 0, 0, 0, 0,					"Filter internal small arcs from graph");
	uiDefButF(block, NUM, B_DIFF, 							"T:",			1111,130,164,19, &G.scene->toolsettings->skgen_threshold_internal,0.0, 1.0, 10, 0,	"Specify the threshold ratio for filtering internal arcs");
	uiDefButBitS(block, TOG, SKGEN_FILTER_EXTERNAL, B_DIFF, "Filter Ex",	1025,110, 83,19, &G.scene->toolsettings->skgen_options, 0, 0, 0, 0,					"Filter external small arcs from graph");
	uiDefButF(block, NUM, B_DIFF, 							"T:",			1111,110,164,19, &G.scene->toolsettings->skgen_threshold_external,0.0, 1.0, 10, 0,	"Specify the threshold ratio for filtering external arcs");

	for(i = 0; i < SKGEN_SUB_TOTAL; i++)
	{
		int y = 90 - 20 * i;
		
		but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, VICON_MOVE_DOWN, 		1025, y, 16, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Change the order the subdivisions algorithm are applied");
		uiButSetFunc(but, skgen_reorder, SET_INT_IN_POINTER(i), NULL);
		
		switch(G.scene->toolsettings->skgen_subdivisions[i])
		{
			case SKGEN_SUB_LENGTH:
				uiDefButBitS(block, TOG, SKGEN_CUT_LENGTH, B_DIFF, 		"Length",		1041, y, 67,19, &G.scene->toolsettings->skgen_options, 0, 0, 0, 0,				"Subdivide arcs in bones of equal length");
				uiDefButF(block, NUM, B_DIFF, 							"T:",			1111, y, 82,19, &G.scene->toolsettings->skgen_length_ratio,1.0, 4.0, 10, 0,		"Specify the ratio limit between straight arc and embeddings to trigger equal subdivisions");
				uiDefButF(block, NUM, B_DIFF, 							"L:",			1193, y, 82,19, &G.scene->toolsettings->skgen_length_limit,0.1,50.0, 10, 0,		"Maximum length of the bones when subdividing");
				break;
			case SKGEN_SUB_ANGLE:
				uiDefButBitS(block, TOG, SKGEN_CUT_ANGLE, B_DIFF, 		"Angle",		1041, y, 67,19, &G.scene->toolsettings->skgen_options, 0, 0, 0, 0,					"Subdivide arcs based on angle");
				uiDefButF(block, NUM, B_DIFF, 							"T:",			1111, y,164,19, &G.scene->toolsettings->skgen_angle_limit,0.0, 90.0, 10, 0,			"Specify the threshold angle in degrees for subdivision");
				break;
			case SKGEN_SUB_CORRELATION:
				uiDefButBitS(block, TOG, SKGEN_CUT_CORRELATION, B_DIFF, "Correlation",	1041, y, 67,19, &G.scene->toolsettings->skgen_options, 0, 0, 0, 0,					"Subdivide arcs based on correlation");
				uiDefButF(block, NUM, B_DIFF, 							"T:",			1111, y,164,19, &G.scene->toolsettings->skgen_correlation_limit,0.0, 1.0, 0.01, 0,	"Specify the threshold correlation for subdivision");
				break;
		}
	}

	uiDefButBitS(block, TOG, SKGEN_SYMMETRY, B_DIFF, 		"Symmetry",		1025, 30,125,19, &G.scene->toolsettings->skgen_options, 0, 0, 0, 0,					"Restore symmetries based on topology");
	uiDefButF(block, NUM, B_DIFF, 							"T:",			1150, 30,125,19, &G.scene->toolsettings->skgen_symmetry_limit,0.0, 1.0, 10, 0,	"Specify the threshold distance for considering potential symmetric arcs");
	uiDefButC(block, NUM, B_DIFF, 							"P:",			1025, 10, 62,19, &G.scene->toolsettings->skgen_postpro_passes, 0, 10, 10, 0,		"Specify the number of processing passes on the embeddings");
	uiDefButC(block, ROW, B_DIFF,							"Smooth",		1087, 10, 63,19, &G.scene->toolsettings->skgen_postpro, 5.0, (float)SKGEN_SMOOTH, 0, 0, "Smooth embeddings");
	uiDefButC(block, ROW, B_DIFF,							"Average",		1150, 10, 62,19, &G.scene->toolsettings->skgen_postpro, 5.0, (float)SKGEN_AVERAGE, 0, 0, "Average embeddings");
	uiDefButC(block, ROW, B_DIFF,							"Sharpen",		1212, 10, 63,19, &G.scene->toolsettings->skgen_postpro, 5.0, (float)SKGEN_SHARPEN, 0, 0, "Sharpen embeddings");
	uiBlockEndAlign(block);
}

static void editing_panel_mesh_tools1(Object *ob, Mesh *me)
{
	uiBlock *block;


	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_tools1", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Mesh Tools More", "Editing", 960, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_SELSWAP,	"Select Swap",	955, 200,  106, 19, 0, 0, 0, 0, 0, "Selects unselected faces, and deselects selected faces (Ctrl+I)");
	uiDefBut(block, BUT,B_HIDE,		"Hide",		1061, 200, 106, 19, 0, 0, 0, 0, 0, "Hides selected faces (H)");
	uiDefBut(block, BUT,B_REVEAL,	"Reveal",		1167, 200,  107, 19, 0, 0, 0, 0, 0, "Reveals selected faces (Alt H)");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,					REDRAWVIEW3D, "NSize:",	955, 170, 150, 19, &G.scene->editbutsize, 0.001, 2.0, 10, 0, "Sets the length to use when displaying face normals");
	uiDefButBitI(block, TOG, G_DRAWNORMALS, REDRAWVIEW3D, "Draw Normals",	955,148,150,19, &G.f, 0, 0, 0, 0, "Displays face normals as lines");
	uiDefButBitI(block, TOG, G_DRAW_VNORMALS, REDRAWVIEW3D, "Draw VNormals",955,126,150,19, &G.f, 0, 0, 0, 0, "Displays vertex normals as lines");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, G_DRAWFACES, REDRAWVIEW3D_IMAGE, "Draw Faces",		955,88,150,19, &G.f, 0, 0, 0, 0, "Displays all faces as shades in the 3d view and UV editor");
	uiDefButBitI(block, TOG, G_DRAWEDGES, REDRAWVIEW3D_IMAGE, "Draw Edges", 955, 66,150,19, &G.f, 0, 0, 0, 0, "Displays selected edges using hilights in the 3d view and UV editor");
	uiDefButBitI(block, TOG, G_DRAWCREASES, REDRAWVIEW3D, "Draw Creases", 955, 42,150,19, &G.f, 0, 0, 0, 0, "Displays creases created for subsurf weighting");
	uiDefButBitI(block, TOG, G_DRAWBWEIGHTS, REDRAWVIEW3D, "Draw Bevel Weights", 955, 20,150,19, &G.f, 0, 0, 0, 0, "Displays weights created for the Bevel modifier");
	uiDefButBitI(block, TOG, G_DRAWSEAMS, REDRAWVIEW3D, "Draw Seams", 955, -2,150,19, &G.f, 0, 0, 0, 0, "Displays UV unwrapping seams");
	uiDefButBitI(block, TOG, G_DRAWSHARP, REDRAWVIEW3D, "Draw Sharp", 955,  -24,150,19, &G.f, 0, 0, 0, 0, "Displays sharp edges, used with the EdgeSplit modifier");
	uiBlockEndAlign(block);
	
	/* Measurement drawing options */
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, G_DRAW_EDGELEN, REDRAWVIEW3D, "Edge Length",	1125,170,150,19, &G.f, 0, 0, 0, 0, "Displays selected edge lengths");
	uiDefButBitI(block, TOG, G_DRAW_EDGEANG, REDRAWVIEW3D, "Edge Angles",	1125,148,150,19,  &G.f, 0, 0, 0, 0, "Displays the angles in the selected edges in degrees");
	uiDefButBitI(block, TOG, G_DRAW_FACEAREA, REDRAWVIEW3D, "Face Area",	1125,126,150,19, &G.f, 0, 0, 0, 0, "Displays the area of selected faces");
#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		uiDefButBitI(block, TOG, G_DRAW_VERSE_DEBUG, REDRAWVIEW3D, "Draw VDebug",1125,104,150,19, &G.f, 0, 0, 0, 0, "Displays verse debug information");
#endif
	
	uiBlockEndAlign(block);
	
	uiDefButBitS(block, TOG, B_MESH_X_MIRROR, B_DIFF, "X-axis mirror",1125,0,150,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "While using transforms, mirrors the transformation");
	
	uiDefButC(block, MENU, REDRAWBUTSEDIT, "Edge Alt-Select Mode%t|Loop Select%x0|Tag Edges (Seam)%x1|Tag Edges (Sharp)%x2|Tag Edges (Crease)%x3|Tag Edges (Bevel)%x4",1125,88,150,19, &G.scene->toolsettings->edge_mode, 0, 0, 0, 0, "Operation to use when Alt+RMB on edges, Use Alt+Shift+RMB to tag the shortest path from the active edge");
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, G_ALLEDGES, 0, "All Edges",			1125, 22,150,19, &G.f, 0, 0, 0, 0, "Displays all edges in object mode without optimization");
	uiDefButBitS(block, TOG, B_MESH_X_MIRROR, B_DIFF, "X-axis mirror",1125,0,150,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "While using transforms, mirrors the transformation");
	uiBlockEndAlign(block);
}

char *get_vertexgroup_menustr(Object *ob)
{
	bDeformGroup *dg;
	int defCount, min, index;
	char (*qsort_ptr)[sizeof(dg->name)+6] = NULL; // +6 for "%x999|" max 999 groups selectable
	char *s, *menustr;
	int printed;
	
	defCount=BLI_countlist(&ob->defbase);
	
	if (!defCount) min=0;
	else min=1;
	
	if (defCount > 0) {
		/*
		 * This will hold the group names temporarily
		 * so we can sort them
		 */
		qsort_ptr = MEM_callocN (defCount * sizeof (qsort_ptr[0]),
								 "qsort_ptr");
		for (index = 1, dg = ob->defbase.first; dg; index++, dg=dg->next) {
			printed = snprintf (qsort_ptr[index - 1], sizeof (dg->name), dg->name);
			snprintf (qsort_ptr[index - 1]+printed, 6+1, "%%x%d|", index); // +1 to move the \0   see above 999 max here too
		}
		
		qsort (qsort_ptr, defCount, sizeof (qsort_ptr[0]),
			   ( int (*)(const void *, const void *) ) strcmp);
	}
	
	s= menustr = MEM_callocN((sizeof(qsort_ptr[0]) * defCount)+30, "menustr");      // plus 30 for when defCount==0
	if(defCount) {
		for (index = 0; index < defCount; index++) {
			int cnt= sprintf (s, "%s", qsort_ptr[index]);
			if (cnt>0) s+= cnt;
		}
	}
	else strcpy(menustr, "No Vertex Groups in Object");
	
	if (qsort_ptr)
		MEM_freeN (qsort_ptr);
	
	return menustr;
}

static void verify_poselib_posename(void *arg1, void *arg2)
{
	bAction *act= (bAction *)arg1;
	TimeMarker *marker= (TimeMarker *)arg2;
	
	BLI_uniquename(&act->markers, marker, "Pose", offsetof(TimeMarker, name), 64);
}

static void verify_posegroup_groupname(void *arg1, void *arg2)
{
	bPose *pose= (bPose *)arg1;
	bActionGroup *grp= (bActionGroup *)arg2;
	
	BLI_uniquename(&pose->agroups, grp, "Group", offsetof(bActionGroup, name), 32);
}

static char *build_colorsets_menustr ()
{
	DynStr *pupds= BLI_dynstr_new();
	char *str;
	char buf[48];
	int i;
	
	/* add title first (and the "default" entry) */
	BLI_dynstr_append(pupds, "Bone Color Set%t|Default Colors%x0|");
	
	/* loop through set indices, adding them */
	for (i=1; i<21; i++) {
		sprintf(buf, "%d - Theme Color Set%%x%d|", i, i);
		BLI_dynstr_append(pupds, buf);
	}
	
	/* add the 'custom' entry */
	BLI_dynstr_append(pupds, "Custom Set %x-1");
	
	/* convert to normal MEM_malloc'd string */
	str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
	
	return str;
}

static void editing_panel_links(Object *ob)
{
	uiBlock *block;
	ID *id, *idfrom;
	int *poin, xco=143;
	float min;
	Material *ma;
	char str[64];
	uiBut *but;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_links", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Link and Materials", "Editing", 0, 0, 318, 204)==0) return;
	
	uiSetButLock((ob && ob->id.lib), ERROR_LIBDATA_MESSAGE);
	
	buttons_active_id(&id, &idfrom);
	
	if(id) {
		int alone= 0;
		int local= 0;
		int browse= B_EDITBROWSE;
		
		if(ob->type==OB_MESH) {
			browse= B_MESHBROWSE;
			alone= B_MESHALONE;
			local= B_MESHLOCAL;
			uiSetButLock(G.obedit!=0, "Unable to perform function in EditMode");
		}
		else if(ob->type==OB_MBALL) {
			alone= B_MBALLALONE;
			local= B_MBALLLOCAL;
		}
		else if ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF) {
			alone= B_CURVEALONE;
			local= B_CURVELOCAL;
		}
		else if(ob->type==OB_CAMERA) {
			alone= B_CAMERAALONE;
			local= B_CAMERALOCAL;
		}
		else if(ob->type==OB_LAMP) {
			alone= B_LAMPALONE;
			local= B_LAMPLOCAL;
		}
		else if (ob->type==OB_ARMATURE){
			alone = B_ARMALONE;
			local = B_ARMLOCAL;
		}
		else if(ob->type==OB_LATTICE) {
			alone= B_LATTALONE;
			local= B_LATTLOCAL;
		}
		uiBlockSetCol(block, TH_BUT_SETTING2);
		xco= std_libbuttons(block, 143, 180, 0, NULL, browse, GS(id->name), 0, id, idfrom, &(G.buts->menunr), alone, local, 0, 0, B_KEEPDATA);
		uiBlockSetCol(block, TH_AUTO);
	}
	if(ob) {
		uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
		but = uiDefBut(block, TEX, B_IDNAME, "OB:",	xco, 180, 454-xco, YIC, ob->id.name+2, 0.0, 21.0, 0, 0, "Active Object name.");
#ifdef WITH_VERSE
		if(ob->vnode) uiButSetFunc(but, test_and_send_idbutton_cb, ob, ob->id.name);
		else uiButSetFunc(but, test_idbutton_cb, ob->id.name, NULL);
#else
		uiButSetFunc(but, test_idbutton_cb, ob->id.name, NULL);
#endif

	}

	/* empty display handling, note it returns! */
	if (ob->type==OB_EMPTY) {
		uiDefBut(block, LABEL,0,"Empty Display:",
				xco, 154, 130,20, 0, 0, 0, 0, 0, "");
		
		uiBlockBeginAlign(block);
		uiDefButC(block, MENU, REDRAWVIEW3D, "Empty Drawtype%t|Arrows%x1|Single Arrow%x4|Plain Axes%x2|Circle%x3|Cube%x5|Sphere%x6|Cone%x7",
				xco, 128, 140, 20, &ob->empty_drawtype, 0, 0, 0, 0, "The Empty 3D View display style");
		uiDefButF(block, NUM, REDRAWVIEW3D, "Size:",
				xco, 108, 140, 21, &ob->empty_drawsize, 0.01, 10.0, 1, 0, "The size to display the Empty");
		uiBlockEndAlign(block);
		return;
	}
	
	/* poselib for armatures */
	if (ob->type==OB_ARMATURE) {
		if ((ob->pose) && (ob->flag & OB_POSEMODE) && (G.obedit != ob)) {
			bAction *act= ob->poselib;
			bPose *pose= ob->pose;
			bActionGroup *grp= NULL;
			int count;
			char *menustr;
			
			/* PoseLib settings for armature reside on the left */
			xco= 143;
			
			uiDefBut(block, LABEL,0, "Pose Library:", xco, 154, 200, 20, 0, 0, 0, 0, 0, "");
			
			/* PoseLib Action */
			uiBlockSetCol(block, TH_BUT_SETTING2);
			std_libbuttons(block, 143, 130, 0, NULL, B_POSELIB_BROWSE, ID_AC, 0, (ID *)act, (ID *)ob, &(G.buts->menunr), B_POSELIB_ALONE, 0, B_POSELIB_DELETE, 0, 0);
			uiBlockSetCol(block, TH_AUTO);
			
			/* PoseLib -  Pose editing controls */
			if (act) {
				uiDefBut(block, BUT, B_POSELIB_VALIDATE,  "Auto-Sync PoseLib",	xco,110,160,20, 0, 0, 0, 0, 0, "Syncs the current PoseLib with the poses available");
				
				uiBlockBeginAlign(block);
					/* currently 'active' pose */
					if (act->markers.first) {
						count= BLI_countlist(&act->markers);
						menustr= poselib_build_poses_menu(act, "PoseLib Poses");
						uiDefButI(block, MENU, B_POSELIB_APPLYP, menustr, xco, 85,18,20, &act->active_marker, 1, count, 0, 0, "Browses Poses in Pose Library. Applies chosen pose.");
						MEM_freeN(menustr);
						
						if (act->active_marker) {
							TimeMarker *marker= poselib_get_active_pose(act);
							
							but= uiDefBut(block, TEX, REDRAWBUTSEDIT,"",		xco+18,85,160-18-20,20, marker->name, 0, 63, 0, 0, "Displays current Pose Library Pose name. Click to change.");
							uiButSetFunc(but, verify_poselib_posename, act, marker);
							uiDefIconBut(block, BUT, B_POSELIB_REMOVEP, VICON_X, xco+160-20, 85, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Remove this Pose Library Pose from Pose Library.");
						}
					}
					
					/* add new poses */
					uiDefBut(block, BUT, B_POSELIB_ADDPOSE, "Add Pose",	xco,65,80,20, 0, 0, 0, 0, 0, "Add current pose to PoseLib");
					uiDefBut(block, BUT, B_POSELIB_REPLACEP, "Replace Pose",	xco+80,65,80,20, 0, 0, 0, 0, 0, "Replace existing PoseLib Pose with current pose");
				uiBlockEndAlign(block);	
			}
			
			
			/* Bone Groups settings for armature reside on the right */
			xco= 315;
			
			uiDefBut(block, LABEL,0, "Bone Groups:", xco, 154, 140, 20, 0, 0, 0, 0, 0, "");
			
			uiBlockBeginAlign(block);
				if (pose->agroups.first) {
					/* currently 'active' group - browse groups */
					count= BLI_countlist(&pose->agroups);
					menustr= build_posegroups_menustr(pose, 0);
					uiDefButI(block, MENU, B_POSEGRP_RECALC, menustr, xco, 130,18,20, &pose->active_group, 1, count, 0, 0, "Browses Bone Groups available for Armature. Click to change.");
					MEM_freeN(menustr);
					
					/* currently 'active' group - change name */
					if (pose->active_group) {
						grp= (bActionGroup *)BLI_findlink(&pose->agroups, pose->active_group-1);
						
						/* active group */
						but= uiDefBut(block, TEX, REDRAWBUTSEDIT,"", xco+18,130,140-18-20,20, grp->name, 0, 31, 0, 0, "Displays current Bone Group name. Click to change.");
						uiButSetFunc(but, verify_posegroup_groupname, pose, grp); 
						uiDefIconBut(block, BUT, B_POSEGRP_REMOVE, VICON_X, xco+140-20, 130, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Remove this Bone Group");
					}
				}
				
				uiDefBut(block, BUT, B_POSEGRP_ADD, "Add Group",	xco,110,140,20, 0, 21, 0, 0, 0, "Add a new Bone Group for the Pose");
			uiBlockEndAlign(block);
			
			/* colour set for 'active' group */
			if (pose->active_group && grp) {
				uiBlockBeginAlign(block);
					menustr= build_colorsets_menustr();
					uiDefButI(block, MENU,B_POSEGRP_RECALC, menustr, xco,85,140,19, &grp->customCol, -1, 20, 0.0, 0.0, "Index of set of Custom Colors to shade Group's bones with. 0 = Use Default Color Scheme, -1 = Use Custom Color Scheme");						
					MEM_freeN(menustr);
					
					/* show color-selection/preview */
					if (grp->customCol) {
						if (grp->customCol > 0) {
							/* copy theme colors on-to group's custom color in case user tries to edit color */
							bTheme *btheme= U.themes.first;
							ThemeWireColor *col_set= &btheme->tarm[(grp->customCol - 1)];
							
							memcpy(&grp->cs, col_set, sizeof(ThemeWireColor));
						}
						else {
							/* init custom colours with a generic multi-colour rgb set, if not initialised already */
							if (grp->cs.solid[0] == 0) {
								/* define for setting colors in theme below */
								#define SETCOL(col, r, g, b, a)  col[0]=r; col[1]=g; col[2]= b; col[3]= a;
								
								SETCOL(grp->cs.solid, 0xff, 0x00, 0x00, 255);
								SETCOL(grp->cs.select, 0x81, 0xe6, 0x14, 255);
								SETCOL(grp->cs.active, 0x18, 0xb6, 0xe0, 255);
								
								#undef SETCOL
							}
						}
						
						/* color changing */
						uiDefButC(block, COL, B_POSEGRP_MCUSTOM, "",		xco, 65, 30, 19, grp->cs.solid, 0, 0, 0, 0, "Color to use for surface of bones");
						uiDefButC(block, COL, B_POSEGRP_MCUSTOM, "",		xco+30, 65, 30, 19, grp->cs.select, 0, 0, 0, 0, "Color to use for 'selected' bones");
						uiDefButC(block, COL, B_POSEGRP_MCUSTOM, "",		xco+60, 65, 30, 19, grp->cs.active, 0, 0, 0, 0, "Color to use for 'active' bones");
						
						uiDefButBitS(block, TOG, TH_WIRECOLOR_CONSTCOLS, B_POSEGRP_MCUSTOM, "ConstCols",  xco+90,65,50,20, &grp->cs.flag, 0, 0, 0, 0, "Allow the use of colors indicating constraints/keyed status");
					}
				uiBlockEndAlign(block);
			}
		}	
		return;
	}
	
	/* vertex group... partially editmode... */
	if(ob->type==OB_MESH || ob->type==OB_LATTICE) {
		bDeformGroup *defGroup;
		uiBut *but;
		int	defCount;
		
		uiDefBut(block, LABEL,0,"Vertex Groups",
				 143,153,130,20, 0, 0, 0, 0, 0, "");
		
		defCount=BLI_countlist(&ob->defbase);
		
		if (defCount) {
			char *menustr= get_vertexgroup_menustr(ob);
			
			uiBlockBeginAlign(block);
			
			uiDefButS(block, MENU, B_MAKEDISP, menustr, 143, 132,18,21, (short *)&ob->actdef, 1, defCount, 0, 0, "Browses available vertex groups");
			MEM_freeN (menustr);
		
			if (ob->actdef){
				defGroup = BLI_findlink(&ob->defbase, ob->actdef-1);
				but= uiDefBut(block, TEX, REDRAWBUTSEDIT,"",		161,132,140-18,21, defGroup->name, 0, 31, 0, 0, "Displays current vertex group name. Click to change. (Match bone name for deformation.)");
				uiButSetFunc(but, verify_vertexgroup_name_func, defGroup, NULL);
				uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
				
				uiDefButF(block, NUM, REDRAWVIEW3D, "Weight:",		143, 111, 140, 21, &editbutvweight, 0, 1, 10, 0, "Sets the current vertex group's bone deformation strength");
			}
			uiBlockEndAlign(block);
		}
		
		if (G.obedit && G.obedit==ob){
			uiBlockBeginAlign(block);
			uiDefBut(block, BUT,B_NEWVGROUP,"New",			143,90,70,21, 0, 0, 0, 0, 0, "Creates a new vertex group");
			uiDefBut(block, BUT,B_DELVGROUP,"Delete",		213,90,70,21, 0, 0, 0, 0, 0, "Removes the current vertex group");
			
			uiDefBut(block, BUT,B_ASSIGNVGROUP,"Assign",	143,69,70,21, 0, 0, 0, 0, 0, "Assigns selected vertices to the current vertex group");
			uiDefBut(block, BUT,B_REMOVEVGROUP,"Remove",	213,69,70,21, 0, 0, 0, 0, 0, "Removes selected vertices from the current vertex group");
			
			uiDefBut(block, BUT,B_SELVGROUP,"Select",		143,48,70,21, 0, 0, 0, 0, 0, "Selects vertices belonging to the current vertex group");
			uiDefBut(block, BUT,B_DESELVGROUP,"Desel.",		213,48,70,21, 0, 0, 0, 0, 0, "Deselects vertices belonging to the current vertex group");
			uiBlockEndAlign(block);
		}
		else {
			ID *id= ob->data;
			
			uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
			
			uiBlockBeginAlign (block);
			uiDefBut (block, BUT, B_NEWVGROUP, "New", 143, 90, 70, 21, 0, 0, 0, 0, 0, "Creates a new vertex group");
			uiDefBut (block, BUT, B_DELVGROUP, "Delete", 213, 90, 70, 21, 0, 0, 0, 0, 0, "Removes the current vertex group");
			uiDefBut (block, BUT, B_COPYVGROUP, "Copy Group", 143, 70, 140, 19, 0, 0, 0, 0, 0, "Copy Group of Vertex");
			uiBlockEndAlign (block);

			if(id->us > 1)
				uiDefBut(block, BUT,B_LINKEDVGROUP, "Copy To Linked",	143,50,140,20, 0, 0, 0, 0, 0, "Creates identical vertex group names in other Objects using this Object-data");
		}
	}
	
	/* now only objects that can be visible rendered */
	if (!OB_SUPPORT_MATERIAL(ob)) return;
	
	uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	give_obdata_texspace(ob, &poin, NULL, NULL, NULL);
	uiDefButBitI(block, TOG, AUTOSPACE, B_AUTOTEX, "AutoTexSpace",	143,15,140,19, poin, 0, 0, 0, 0, "Adjusts active object's texture space automatically when transforming object");

	sprintf(str,"%d Mat ", ob->totcol);
	if(ob->totcol) min= 1.0; else min= 0.0;
	ma= give_current_material(ob, ob->actcol);

	if(G.obedit) {
		char *str= NULL;
		IDnames_to_pupstring(&str, NULL, "ADD NEW %x 32767", &G.main->mat, NULL, NULL);
		uiDefButS(block, MENU, B_MATASS_BROWSE, str, 292,150,20,20, &G.buts->menunr, 0, 0, 0, 0, "Browses existing choices and assign");
		MEM_freeN(str);
	}
	
	if(ma) uiDefBut(block, LABEL, 0, ma->id.name+2, 318,150, 103, 20, 0, 0, 0, 0, 0, "");

	uiBlockBeginAlign(block);
	if(ma) uiDefButF(block, COL, B_MATCOL2, "",	292,113,31,30, &(ma->r), 0, 0, 0, 0, "");
	uiDefButC(block, NUM, B_ACTCOL,	str,		324,113,100,30, &ob->actcol, min, (float)(ob->totcol), 0, 0, "Displays total number of material indices and the current index");
	uiDefBut(block, BUT,B_MATWICH,	"?",		424,113,30,30, 0, 0, 0, 0, 0, "In EditMode, sets the active material index from selected faces");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_MATNEW,	"New",		292,90,80,20, 0, 0, 0, 0, 0, "Adds a new Material index");
	uiDefBut(block, BUT,B_MATDEL,	"Delete",	372,90,80,20, 0, 0, 0, 0, 0, "Deletes this Material index");
	uiDefBut(block, BUT,B_MATSEL,	"Select",	292,70,80,20, 0, 0, 0, 0, 0, "In EditMode, selects faces that have the active index");
	uiDefBut(block, BUT,B_MATDESEL,	"Deselect",	372,70,80,20, 0, 0, 0, 0, 0, "Deselects everything with current indexnumber");
	uiDefBut(block, BUT,B_MATASS,	"Assign",	292,50,160,20, 0, 0, 0, 0, 0, "In EditMode, assigns the active index to selected faces");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_SETSMOOTH,"Set Smooth",	292,15,80,20, 0, 0, 0, 0, 0, "In EditMode, sets 'smooth' rendering of selected faces");
	uiDefBut(block, BUT,B_SETSOLID,	"Set Solid",	372,15,80,20, 0, 0, 0, 0, 0, "In EditMode, sets 'solid' rendering of selected faces");

	uiBlockEndAlign(block);


}

void editing_panel_sculpting_tools()
{
	uiBlock *block= uiNewBlock(&curarea->uiblocks, "editing_panel_sculpting_tools", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Sculpt", "Editing", 300, 0, 318, 204)==0) return;

	sculptmode_draw_interface_tools(block,0,200);
}

void editing_panel_sculpting_brush()
{
	uiBlock *block= uiNewBlock(&curarea->uiblocks, "editing_panel_sculpting_brush", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Brush", "Editing", 300, 0, 318, 204)==0) return;

	sculptmode_draw_interface_brush(block,0,200);
}

void editing_panel_sculpting_textures()
{
	uiBlock *block= uiNewBlock(&curarea->uiblocks, "editing_panel_sculpting_texture", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Texture", "Editing", 300, 0, 318, 204)==0) return;

	sculptmode_draw_interface_textures(block,0,200);
}

void sculptmode_draw_interface_tools(uiBlock *block, unsigned short cx, unsigned short cy)
{
	SculptData *sd;

	if(!G.scene) return;
	sd= &G.scene->sculptdata;

	uiBlockBeginAlign(block);

	uiDefBut(block,LABEL,B_NOP,"Brush",cx,cy,90,19,NULL,0,0,0,0,"");
	cy-= 20;
	
	uiBlockBeginAlign(block);	
	uiDefButS(block,ROW,REDRAWBUTSEDIT,"Draw",cx,cy,67,19,&sd->brush_type,14.0,DRAW_BRUSH,0,0,"Draw lines on the model");
	uiDefButS(block,ROW,REDRAWBUTSEDIT,"Smooth",cx+67,cy,67,19,&sd->brush_type,14.0,SMOOTH_BRUSH,0,0,"Interactively smooth areas of the model");
	uiDefButS(block,ROW,REDRAWBUTSEDIT,"Pinch",cx+134,cy,67,19,&sd->brush_type,14.0,PINCH_BRUSH,0,0,"Interactively pinch areas of the model");
	uiDefButS(block,ROW,REDRAWBUTSEDIT,"Inflate",cx+201,cy,67,19,&sd->brush_type,14,INFLATE_BRUSH,0,0,"Push vertices along the direction of their normals");
	cy-= 20;
	uiDefButS(block,ROW,REDRAWBUTSEDIT,"Grab", cx,cy,89,19,&sd->brush_type,14,GRAB_BRUSH,0,0,"Grabs a group of vertices and moves them with the mouse");
	uiDefButS(block,ROW,REDRAWBUTSEDIT,"Layer", cx+89,cy,89,19,&sd->brush_type,14, LAYER_BRUSH,0,0,"Adds a layer of depth");
	uiDefButS(block,ROW,REDRAWBUTSEDIT,"Flatten", cx+178,cy,90,19,&sd->brush_type,14, FLATTEN_BRUSH,0,0,"Interactively flatten areas of the model");
	cy-= 25;
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefBut(block,LABEL,B_NOP,"Shape",cx,cy,90,19,NULL,0,0,0,0,"");
	cy-= 20;
	uiBlockBeginAlign(block);
	if(sd->brush_type != SMOOTH_BRUSH && sd->brush_type != GRAB_BRUSH && sd->brush_type != FLATTEN_BRUSH) {
		uiDefButC(block,ROW,B_NOP,"Add",cx,cy,89,19,&sculptmode_brush()->dir,15.0,1.0,0, 0,"Add depth to model [Shift]");
		uiDefButC(block,ROW,B_NOP,"Sub",cx+89,cy,89,19,&sculptmode_brush()->dir,15.0,2.0,0, 0,"Subtract depth from model [Shift]");
	}
	if(sd->brush_type!=GRAB_BRUSH)
		uiDefButBitC(block, TOG, SCULPT_BRUSH_AIRBRUSH, 0, "Airbrush", cx+178,cy,89,19, &sculptmode_brush()->flag,0,0,0,0, "Brush makes changes without waiting for the mouse to move");
	cy-= 20;
	uiDefButS(block,NUMSLI,B_NOP,"Size: ",cx,cy,268,19,&sculptmode_brush()->size,1.0,200.0,0,0,"Set brush radius in pixels");
	cy-= 20;
	if(sd->brush_type!=GRAB_BRUSH)
		uiDefButC(block,NUMSLI,B_NOP,"Strength: ",cx,cy,268,19,&sculptmode_brush()->strength,1.0,100.0,0,0,"Set brush strength");
	cy-= 25;
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefBut( block,LABEL,B_NOP,"Symmetry",cx,cy,90,19,NULL,0,0,0,0,"");
	cy-= 20;
	uiBlockBeginAlign(block);
	uiDefButBitC(block, TOG, SYMM_X, 0, "X", cx,cy,40,19, &sd->symm, 0,0,0,0, "Mirror brush across X axis");
	uiDefButBitC(block, TOG, SYMM_Y, 0, "Y", cx+40,cy,40,19, &sd->symm, 0,0,0,0, "Mirror brush across Y axis");
	uiDefButBitC(block, TOG, SYMM_Z, 0, "Z", cx+80,cy,40,19, &sd->symm, 0,0,0,0, "Mirror brush across Z axis");
	uiBlockEndAlign(block);

	
	cy+= 20;
	uiBlockBeginAlign(block);
	uiDefBut( block,LABEL,B_NOP,"LockAxis",cx+140,cy,90,19,NULL,0,0,0,0,"");
	cy-= 20;
	uiBlockBeginAlign(block);
	uiDefButBitC(block, TOG, AXISLOCK_X, 0, "X", cx+140,cy,40,19, &sd->axislock, 0,0,0,0, "Constrain X axis");
	uiDefButBitC(block, TOG, AXISLOCK_Y, 0, "Y", cx+180,cy,40,19, &sd->axislock, 0,0,0,0, "Constrain Y axis");
	uiDefButBitC(block, TOG, AXISLOCK_Z, 0, "Z", cx+220,cy,40,19, &sd->axislock, 0,0,0,0, "Constrain Z axis");
	uiBlockEndAlign(block);
	
	
	
	cx+= 210;
}

static void sculptmode_curves_reset(void *sd_v, void *j)
{
	SculptData *sd = sd_v;
	sculpt_reset_curve(sd);
	curvemapping_changed(sd->cumap, 0);
}

void sculptmode_draw_interface_brush(uiBlock *block, unsigned short cx, unsigned short cy)
{
	SculptData *sd= sculpt_data();
	int orig_y = cy;
	rctf rect;
	uiBut *but;

	uiBlockBeginAlign(block);
	cy-= 20;
	uiDefButC(block,TOG,REDRAWBUTSEDIT, "Curve", cx,cy,80,19, &sd->texfade, 0,0,0,0,"Use curve control for radial brush intensity");
	cy-= 20;
	but= uiDefBut(block, BUT, REDRAWBUTSEDIT, "Reset",cx,cy,80,19, NULL, 0,0,0,0, "Default curve preset");
	uiButSetFunc(but, sculptmode_curves_reset, sd, NULL);
	cy-= 25;
	uiBlockEndAlign(block);	

	uiBlockBeginAlign(block);
	uiDefButS(block,NUM,B_NOP, "Space", cx,cy,80,19, &sd->spacing, 0,500,20,0,"Non-zero inserts N pixels between dots");
	cy-= 20;
	if(sd->brush_type == DRAW_BRUSH)
		uiDefButC(block,NUM,B_NOP, "View", cx,cy,80,19, &sculptmode_brush()->view, 0,10,20,0,"Pulls brush direction towards view");
	cy-= 20;
	uiDefButBitC(block, TOG, SCULPT_BRUSH_ANCHORED, 0, "Anchored", cx,cy,80,19, &sculptmode_brush()->flag, 0,0,0,0, "Keep the brush center anchored to the initial location");
	uiBlockEndAlign(block);

	/* Draw curve */
	cx += 90;
	cy = orig_y;
	rect.xmin= cx; rect.xmax= cx + 178;
	rect.ymin= cy - 160; rect.ymax= cy + 20;
	uiBlockBeginAlign(block);
	curvemap_buttons(block, sd->cumap, (char)0, B_NOP, 0, &rect);
	uiBlockEndAlign(block);
}

void sculptmode_draw_interface_textures(uiBlock *block, unsigned short cx, unsigned short cy)
{
	SculptData *sd= sculpt_data();
	MTex *mtex;
	int i;
	int orig_y= cy;
	char *strp;
	uiBut *but;

	uiBlockBeginAlign(block);
	cy-= 20;
	/* TEX CHANNELS */
	uiBlockBeginAlign(block);
	uiBlockSetCol(block, TH_BUT_NEUTRAL);
	for(i=-1; i<8; i++) {
		char str[64];
		int loos;
		mtex= sd->mtex[i];

		if(i==-1)
			strcpy(str, "Default");
		else {
			if(mtex && mtex->tex) splitIDname(mtex->tex->id.name+2, str, &loos);
			else strcpy(str, "");
		}
		str[10]= 0;
		uiDefButS(block, ROW, REDRAWBUTSEDIT, str,cx, cy, 80, 20, &sd->texact, 3.0, (float)i, 0, 0, "Texture channel");
		cy-= 18;
	}

	cy= orig_y-20;
	cx+= 85;
	mtex= sd->mtex[sd->texact];

	if(sd->texact == -1) {
		uiBlockBeginAlign(block);
		uiDefBut(block,LABEL,B_NOP,"",cx,cy,115,20,0,0,0,0,0,""); /* Padding */
	} else {
		ID *id= NULL;
		uiBlockBeginAlign(block);
		
		if(mtex && mtex->tex) id= &mtex->tex->id;
		IDnames_to_pupstring(&strp, NULL, "ADD NEW %x 32767", &G.main->tex, id, &G.buts->texnr);

		if(mtex && mtex->tex) {		
			uiDefBut(block, TEX, B_IDNAME, "TE:",cx,cy,115,19, mtex->tex->id.name+2, 0.0, 21.0, 0, 0, "Texture name");
			cy-= 20;
			
			uiDefButS(block,MENU,B_SCULPT_TEXBROWSE, strp, cx,cy,20,19, &G.buts->texnr, 0,0,0,0, "Selects an existing texture or creates new");
			uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, cx+21,cy,21,20, 0, 0, 0, 0, 0, "Auto-assigns name to texture");

			but= uiDefBut(block, BUT, B_NOP, "Clear",cx+43, cy, 72, 20, 0, 0, 0, 0, 0, "Erases link to texture");
			uiButSetFunc(but,sculptmode_rem_tex,0,0);
			cy-= 25;

			uiBlockBeginAlign(block);
			uiDefButC(block,ROW, REDRAWBUTSEDIT, "Drag", cx,   cy,39,19, &sd->texrept, 18,SCULPTREPT_DRAG,0,0,"Move the texture with the brush");
			uiDefButC(block,ROW, REDRAWBUTSEDIT, "Tile", cx+39,cy,39,19, &sd->texrept, 18,SCULPTREPT_TILE,0,0,"Treat the texture as a tiled image extending across the screen");
			uiDefButC(block,ROW, REDRAWBUTSEDIT, "3D",   cx+78,cy,37,19, &sd->texrept, 18,SCULPTREPT_3D,  0,0,"Use vertex coords as texture coordinates");
			cy-= 20;

			if(sd->texrept != SCULPTREPT_3D) {
				uiBlockBeginAlign(block);
				uiDefButF(block,NUM,0, "Angle", cx,cy,115,19, &mtex->warpfac, 0,360,100,0, "Rotate texture counterclockwise");
				/*Moved inside, so that following buttons aren't made bigger for no reason*/
				cy-= 20;
			}
			
			/* Added Rake button. Needs to be turned off if 3D is on / disappear*/
			if(sd->texrept != SCULPTREPT_3D){
				uiDefButC(block,TOG,B_NOP, "Rake", cx,cy,115,19, &sd->rake, 0,0,0,0,"Rotate the brush in the direction of motion");
				cy-=20;
			}
				
			if(sd->texrept != SCULPTREPT_DRAG) {
				uiBlockBeginAlign(block);
				but= uiDefIconButC(block, TOG, REDRAWBUTSEDIT, sd->texsep ? ICON_UNLOCKED : ICON_LOCKED, cx,cy,20,19, &sd->texsep,0,0,0,0, "Locks the texture sizes together");			
				uiBlockBeginAlign(block);
				uiDefButF(block,NUM,B_NOP, sd->texsep ? "SizeX" : "Size", cx+20,cy,95,19, &mtex->size[0],1,1000,100,0,"Scaling factor for texture");
				cy-= 20;
				if(sd->texsep) {
					uiDefButF(block,NUM,B_NOP, "SizeY", cx+20,cy,95,19, &mtex->size[1],1,1000,100,0,"Scaling factor for texture");
					cy-= 20;
					if(sd->texrept == SCULPTREPT_3D)
						uiDefButF(block,NUM,B_NOP, "SizeZ", cx+20,cy,95,19, &mtex->size[2],1,1000,100,0,"Scaling factor for texture");
					cy-= 20;
				}
			}
		}
		else {
		       uiDefButS(block,TOG,B_SCULPT_TEXBROWSE, "Add New" ,cx, cy, 115, 19, &G.buts->texnr,-1,32767,0,0, "Adds a new texture");
		       uiDefButS(block,MENU,B_SCULPT_TEXBROWSE, strp, cx,cy-20,20,19, &G.buts->texnr, 0,0,0,0, "Selects an existing texture or creates new");
		}

		MEM_freeN(strp);
	}
	
	uiBlockEndAlign(block);
}

/* *************************** FACE/PAINT *************************** */

void do_fpaintbuts(unsigned short event)
{
	Mesh *me;
	Object *ob;
	bDeformGroup *defGroup;
	MTFace *activetf, *tf;
	MFace *mf;
	MCol *activemcol;
	int a;
	SculptData *sd= &G.scene->sculptdata;
	ID *id, *idtest;
	extern VPaint Gwp;         /* from vpaint */
	ToolSettings *settings= G.scene->toolsettings;
	int nr= 1;
	MTex *mtex;
	ParticleSystem *psys;

	ob= OBACT;
	if(ob==NULL) return;

	switch(event) {

	case B_VPGAMMA:
		vpaint_dogamma();
		break;
	case B_COPY_TF_MODE:
	case B_COPY_TF_UV:
	case B_COPY_TF_COL:
	case B_COPY_TF_TEX:
		me= get_mesh(OBACT);
		activetf= get_active_mtface(NULL, &activemcol, 0);

		if(me && activetf) {
			for (a=0, tf=me->mtface, mf=me->mface; a < me->totface; a++, tf++, mf++) {
				if(tf!=activetf && (mf->flag & ME_FACE_SEL)) {
					if(event==B_COPY_TF_MODE) {
						tf->mode= activetf->mode;
						tf->transp= activetf->transp;
					}
					else if(event==B_COPY_TF_UV) {
						memcpy(tf->uv, activetf->uv, sizeof(tf->uv));
						tf->tpage= activetf->tpage;
						tf->tile= activetf->tile;

						if(activetf->mode & TF_TILES) tf->mode |= TF_TILES;
						else tf->mode &= ~TF_TILES;

					}
					else if(event==B_COPY_TF_TEX) {
						tf->tpage= activetf->tpage;
						tf->tile= activetf->tile;

						if(activetf->mode & TF_TILES) tf->mode |= TF_TILES;
						else tf->mode &= ~TF_TILES;
					}
					else if(event==B_COPY_TF_COL && activemcol)
						memcpy(&me->mcol[a*4], activemcol, sizeof(MCol)*4);
				}
			}

			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			do_shared_vertexcol(me);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_SET_VCOL:
		if(FACESEL_PAINT_TEST) 
			clear_vpaint_selectedfaces();
		else
			clear_vpaint();
		break;
	case B_REDR_3D_IMA:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
	case B_ASSIGNMESH:

		test_object_materials(ob->data);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;

	case B_TFACE_HALO:
		activetf = get_active_mtface(NULL, NULL, 0);
		if(activetf) {
			activetf->mode &= ~TF_BILLBOARD2;
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;

	case B_TFACE_BILLB:
		activetf = get_active_mtface(NULL, NULL, 0);
		if(activetf) {
			activetf->mode &= ~TF_BILLBOARD;
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;
	case B_WEIGHT0_0:
		editbutvweight = 0.0f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;

	case B_WEIGHT1_4:
		editbutvweight = 0.25f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_WEIGHT1_2:
		editbutvweight = 0.5f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_WEIGHT3_4:
		editbutvweight = 0.75f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_WEIGHT1_0:
		editbutvweight = 1.0f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
		
	case B_OPA1_8:
		Gwp.a = 0.125f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_OPA1_4:
		Gwp.a = 0.25f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_OPA1_2:
		Gwp.a = 0.5f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_OPA3_4:
		Gwp.a = 0.75f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_OPA1_0:
		Gwp.a = 1.0f;
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_CLR_WPAINT:
		if(!multires_level1_test()) {
			defGroup = BLI_findlink(&ob->defbase, ob->actdef-1);
			if(defGroup) {
				Mesh *me= ob->data;
				int a;
				for(a=0; a<me->totvert; a++)
					remove_vert_defgroup (ob, defGroup, a);
				allqueue(REDRAWVIEW3D, 0);
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			}
		}
		break;
	case B_SCULPT_TEXBROWSE:
		sd= &G.scene->sculptdata;

		if(G.buts->texnr== -2) {
			id= NULL;
			if(sd) {
				mtex= sd->mtex[sd->texact];
				if(mtex) id= &mtex->tex->id;
			}

			activate_databrowse((ID *)id, ID_TE, 0, B_SCULPT_TEXBROWSE, &G.buts->texnr, do_global_buttons);
			return;
		}
		if(G.buts->texnr < 0) break;

		if(G.buts->pin) {
			
		}
		else if(sd && sd->texact == -1) {
			error("No texture channel selected");
			allqueue(REDRAWBUTSSHADING, 0);
		}
		else if(sd && sd->texact != -1) {
			id= NULL;
			
			mtex= sd->mtex[sd->texact];
			if(mtex) id= &mtex->tex->id;

			idtest= G.main->tex.first;
			while(idtest) {
				if(nr==G.buts->texnr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) { /* new tex */
				if(id)	idtest= (ID *)copy_texture((Tex *)id);
				else idtest= (ID *)add_texture("Tex");
				idtest->us--;
			}
			if(idtest!=id && sd) {
				
				if(sd->mtex[sd->texact]==0) {
					sd->mtex[sd->texact]= add_mtex();
					sd->mtex[sd->texact]->texco= TEXCO_VIEW;
					sd->mtex[sd->texact]->size[0]=
						sd->mtex[sd->texact]->size[1]=
						sd->mtex[sd->texact]->size[2]= 100;
					sd->mtex[sd->texact]->warpfac= 0;
				}
				sd->mtex[sd->texact]->tex= (Tex *)idtest;
				id_us_plus(idtest);
				if(id) id->us--;
				
				BIF_undo_push("Texture browse");
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWOOPS, 0);
				BIF_preview_changed(ID_TE);
			}
		}
		break;

	case B_BRUSHBROWSE:
		if(G.buts->menunr==-2) {
			activate_databrowse((ID*)settings->imapaint.brush, ID_BR, 0, B_BRUSHBROWSE, &G.buts->menunr, do_global_buttons);
			break;
		}
		else if(G.buts->menunr < 0) break;
			
		if(brush_set_nr(&settings->imapaint.brush, G.buts->menunr)) {
			BIF_undo_push("Browse Brush");
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_BRUSHDELETE:
		if(brush_delete(&settings->imapaint.brush)) {
			BIF_undo_push("Unlink Brush");
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_BRUSHKEEPDATA:
		brush_toggled_fake_user(settings->imapaint.brush);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
	case B_BRUSHLOCAL:
		if(settings->imapaint.brush && settings->imapaint.brush->id.lib) {
			if(okee("Make local")) {
				make_local_brush(settings->imapaint.brush);
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWIMAGE, 0);
			}
		}
		break;
	case B_BTEXBROWSE:
		if(settings->imapaint.brush) {
			Brush *brush= settings->imapaint.brush;

			if(G.buts->menunr==-2) {
				MTex *mtex= brush->mtex[brush->texact];
				ID *id= (ID*)((mtex)? mtex->tex: NULL);
				if(G.qual & LR_CTRLKEY) {
					activate_databrowse_imasel(id, ID_TE, 0, B_BTEXBROWSE, &G.buts->menunr, do_fpaintbuts);
				} else {
					activate_databrowse(id, ID_TE, 0, B_BTEXBROWSE, &G.buts->menunr, do_fpaintbuts);
				}
				break;
			}
			else if(G.buts->menunr < 0) break;
				
			if(brush_texture_set_nr(brush, G.buts->menunr)) {
				BIF_undo_push("Browse Brush Texture");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWIMAGE, 0);
			}
		}
		break;
	case B_BTEXDELETE:
		if(settings->imapaint.brush) {
			if (brush_texture_delete(settings->imapaint.brush)) {
				BIF_undo_push("Unlink Brush Texture");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWIMAGE, 0);
			}
		}
		break;
	case B_BRUSHCHANGE:
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_BAKE_REDRAWEDIT:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_BAKE_RECACHE:
		psys=PE_get_current(ob);
		PE_hide_keys_time(psys,CFRA);
		psys_cache_paths(ob,psys,CFRA,0);
		if(PE_settings()->flag & PE_SHOW_CHILD)
			psys_cache_child_paths(ob,psys,CFRA,0);
		
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	}
}

/* -------------------- MODE: vpaint  ------------------- */

void weight_paint_buttons(uiBlock *block)
{
	extern VPaint Gwp;         /* from vpaint */
	Object *ob;
	ob= OBACT;
	
	if(ob==NULL) return;
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, REDRAWVIEW3D, "Weight:",10,170,225,19, &editbutvweight, 0, 1, 10, 0, "Sets the current vertex group's bone deformation strength");
	
	uiDefBut(block, BUT, B_WEIGHT0_0 , "0",			 10,150,45,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_WEIGHT1_4 , "1/4",		 55,150,45,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_WEIGHT1_2 , "1/2",		 100,150,45,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_WEIGHT3_4 , "3/4",		 145,150,45,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_WEIGHT1_0 , "1",			 190,150,45,19, 0, 0, 0, 0, 0, "");
	
	uiDefButF(block, NUMSLI, B_NOP, "Opacity ",		10,130,225,19, &Gwp.a, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
	
	uiDefBut(block, BUT, B_OPA1_8 , "1/8",		10,110,45,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_OPA1_4 , "1/4",		55,110,45,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_OPA1_2 , "1/2",		100,110,45,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_OPA3_4 , "3/4",		145,110,45,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT, B_OPA1_0 , "1",		190,110,45,19, 0, 0, 0, 0, 0, "");
	
	uiDefButF(block, NUMSLI, B_NOP, "Size ",	10,90,225,19, &Gwp.size, 2.0, 64.0, 0, 0, "The size of the brush");
	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_DIFF, "Mix",		250,170,60,17, &Gwp.mode, 1.0, 0.0, 0, 0, "Mix the vertex colors");
	uiDefButS(block, ROW, B_DIFF, "Add",		250,152,60,17, &Gwp.mode, 1.0, 1.0, 0, 0, "Add the vertex colors");
	uiDefButS(block, ROW, B_DIFF, "Sub",		250,134,60,17, &Gwp.mode, 1.0, 2.0, 0, 0, "Subtract from the vertex color");
	uiDefButS(block, ROW, B_DIFF, "Mul",		250,116,60,17, &Gwp.mode, 1.0, 3.0, 0, 0, "Multiply the vertex color");
	uiDefButS(block, ROW, B_DIFF, "Blur",		250, 98,60,17, &Gwp.mode, 1.0, 4.0, 0, 0, "Blur the weight with surrounding values");
	uiDefButS(block, ROW, B_DIFF, "Lighter",	250, 80,60,17, &Gwp.mode, 1.0, 5.0, 0, 0, "Paint over darker areas only");
	uiDefButS(block, ROW, B_DIFF, "Darker",		250, 62,60,17, &Gwp.mode, 1.0, 6.0, 0, 0, "Paint over lighter areas only");
	uiBlockEndAlign(block);
	
	/* draw options same as below */
	uiBlockBeginAlign(block);
	if (FACESEL_PAINT_TEST) {
		uiDefButBitI(block, TOG, G_DRAWFACES, B_UVAUTO_DRAWFACES, "Faces",	10,45,60,19, &G.f, 0, 0, 0, 0, "Displays all faces as shades");
		uiDefButBitI(block,TOG, G_DRAWEDGES, REDRAWVIEW3D,"Edges",70,45,60,19, &G.f, 2.0, 0, 0, 0,  "Displays edges of visible faces");
	 	uiDefButBitI(block,TOG, G_HIDDENEDGES, REDRAWVIEW3D,"Hidden Edges",130,45,100,19, &G.f, 2.0, 1.0, 0, 0,  "Displays edges of hidden faces");
	} else{ 
 		uiDefButBitC(block, TOG, OB_DRAWWIRE, REDRAWVIEW3D, "Wire",	10,45,75,19, &ob->dtx, 0, 0, 0, 0, "Displays the active object's wireframe in shaded drawing modes");
	}
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, VP_AREA, 0, "All Faces", 	10,20,60,19, &Gwp.flag, 0, 0, 0, 0, "Paint on all faces inside brush (otherwise only on face under mouse cursor)");
	uiDefButBitS(block, TOG, VP_SOFT, 0, "Vert Dist", 70,20,60,19, &Gwp.flag, 0, 0, 0, 0, "Use distances to vertices (instead of all vertices of face)");
	uiDefButBitS(block, TOGN, VP_HARD, 0, "Soft",		130,20,60,19, &Gwp.flag, 0, 0, 0, 0, "Use a soft brush");
	uiDefButBitS(block, TOG, VP_NORMALS, 0, "Normals", 	190,20,60,19, &Gwp.flag, 0, 0, 0, 0, "Applies the vertex normal before painting");
	uiDefButBitS(block, TOG, VP_SPRAY, 0, "Spray",		250,20,55,19, &Gwp.flag, 0, 0, 0, 0, "Keep applying paint effect while holding mouse");
	uiBlockEndAlign(block);
	
	if(ob) {
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, VP_ONLYVGROUP, REDRAWVIEW3D, "Vgroup",		10,0,100,19, &Gwp.flag, 0, 0, 0, 0, "Only paint on vertices in the selected vertex group.");
		uiDefButBitS(block, TOG, VP_MIRROR_X, REDRAWVIEW3D, "X-Mirror",	110,0,100,19, &Gwp.flag, 0, 0, 0, 0, "Mirrored Paint, applying on mirrored Weight Group name");
		uiDefBut(block, BUT, B_CLR_WPAINT, "Clear",					210,0,100,19, NULL, 0, 0, 0, 0, "Removes reference to this deform group from all vertices");
		uiBlockEndAlign(block);
	}
}

static void editing_panel_mesh_paint(void)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_paint", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Paint", "Editing", 640, 0, 318, 204)==0) return;
	
	
	if(G.f & G_WEIGHTPAINT) {
		weight_paint_buttons(block);
	}
	else if(G.f & G_VERTEXPAINT) {
		extern VPaint Gvp;         /* from vpaint */
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_NOP, "R ",			979,170,150,19, &Gvp.r, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of red used for painting");
		uiDefButF(block, NUMSLI, B_NOP, "G ",			979,150,150,19, &Gvp.g, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of green used for painting");
		uiDefButF(block, NUMSLI, B_NOP, "B ",			979,130,150,19, &Gvp.b, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of blue used for painting");
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_NOP, "Opacity ",		979,105,222,19, &Gvp.a, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
		uiDefButF(block, NUMSLI, B_NOP, "Size ",		979,85,222,19, &Gvp.size, 2.0, 64.0, 0, 0, "The size of the brush");
		uiBlockEndAlign(block);
		
		uiDefButF(block, COL, B_REDR, "",				1140,150,60,40, &(Gvp.r), 0, 0, 0, B_VPCOLSLI, "");
		uiDefBut(block, BUT, B_SET_VCOL, "SetVCol",	1140,130,60,20, 0, 0, 0, 0, 0, "Set Vertex color of selection to current (Shift+K)");
		
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, B_DIFF, "Mix",			1212,170,63,17, &Gvp.mode, 1.0, 0.0, 0, 0, "Mix the vertex colors");
		uiDefButS(block, ROW, B_DIFF, "Add",			1212,152,63,17, &Gvp.mode, 1.0, 1.0, 0, 0, "Add the vertex color");
		uiDefButS(block, ROW, B_DIFF, "Sub",			1212, 134,63,17, &Gvp.mode, 1.0, 2.0, 0, 0, "Subtract from the vertex color");
		uiDefButS(block, ROW, B_DIFF, "Mul",			1212, 116,63,17, &Gvp.mode, 1.0, 3.0, 0, 0, "Multiply the vertex color");
		uiDefButS(block, ROW, B_DIFF, "Blur",			1212, 98,63,17, &Gvp.mode, 1.0, 4.0, 0, 0, "Blur the color with surrounding values");
		uiDefButS(block, ROW, B_DIFF, "Lighter",		1212, 80,63,17, &Gvp.mode, 1.0, 5.0, 0, 0, "Paint over darker areas only");
		uiDefButS(block, ROW, B_DIFF, "Darker",			1212, 62,63,17, &Gvp.mode, 1.0, 6.0, 0, 0, "Paint over lighter areas only");
		uiBlockEndAlign(block);
		
		/* draw options */
		uiBlockBeginAlign(block);
		if (FACESEL_PAINT_TEST) {
			uiDefButBitI(block, TOG, G_DRAWFACES, B_UVAUTO_DRAWFACES, "Faces",	979,50,60,19, &G.f, 0, 0, 0, 0, "Displays all faces as shades");
			uiDefButBitI(block,TOG, G_DRAWEDGES, REDRAWVIEW3D,"Edges",1039,50,60,19, &G.f, 2.0, 0, 0, 0,  "Displays edges of visible faces");
		 	uiDefButBitI(block,TOG, G_HIDDENEDGES, REDRAWVIEW3D,"Hidden Edges",1099,50,100,19, &G.f, 2.0, 1.0, 0, 0,  "Displays edges of hidden faces");
		}
		uiBlockEndAlign(block);
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, VP_AREA, 0, "All Faces", 		979,25,75,19, &Gvp.flag, 0, 0, 0, 0, "Paint on all faces inside brush");
		uiDefButBitS(block, TOG, VP_SOFT, 0, "Vertex Dist", 	1054,25,75,19, &Gvp.flag, 0, 0, 0, 0, "Use distances to vertices (instead of paint entire faces)");
		uiDefButBitS(block, TOG, VP_NORMALS, 0, "Normals", 	1129,25,75,19, &Gvp.flag, 0, 0, 0, 0, "Applies the vertex normal before painting");
		uiDefButBitS(block, TOG, VP_SPRAY, 0, "Spray",		1204,25,72,19, &Gvp.flag, 0, 0, 0, 0, "Keep applying paint effect while holding mouse");

		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_VPGAMMA, "Set",		979,0,81,19, 0, 0, 0, 0, 0, "Apply Mul and Gamma to vertex colors");
		uiDefButF(block, NUM, B_DIFF, "Mul:", 		1061,0,112,19, &Gvp.mul, 0.1, 50.0, 10, 0, "Set the number to multiply vertex colors with");
		uiDefButF(block, NUM, B_DIFF, "Gamma:", 	1174,0,102,19, &Gvp.gamma, 0.1, 5.0, 10, 0, "Change the clarity of the vertex colors");
		uiBlockEndAlign(block);
	}
	else { /* texture paint */
		ToolSettings *settings= G.scene->toolsettings;
		Brush *brush= settings->imapaint.brush;
		ID *id;
		int yco, xco, butw;

		yco= 160;

		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, B_BRUSHCHANGE, "Draw",		0  ,yco,108,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_DRAW, 0, 0, "Draw brush");
		uiDefButS(block, ROW, B_BRUSHCHANGE, "Soften",		108 ,yco,106,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_SOFTEN, 0, 0, "Soften brush");
		uiDefButS(block, ROW, B_BRUSHCHANGE, "Smear",		214,yco,106,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_SMEAR, 0, 0, "Smear brush");	
		uiBlockEndAlign(block);
		yco -= 30;

		uiBlockSetCol(block, TH_BUT_SETTING2);
		id= (ID*)settings->imapaint.brush;
		xco= std_libbuttons(block, 0, yco, 0, NULL, B_BRUSHBROWSE, ID_BR, 0, id, NULL, &(G.buts->menunr), 0, B_BRUSHLOCAL, B_BRUSHDELETE, 0, B_BRUSHKEEPDATA);
		uiBlockSetCol(block, TH_AUTO);

		if(brush && !brush->id.lib) {
			MTex *mtex= brush->mtex[brush->texact];

			butw= 320-(xco+10);

			uiDefButS(block, MENU, B_NOP, "Mix %x0|Add %x1|Subtract %x2|Multiply %x3|Lighten %x4|Darken %x5|Erase Alpha %x6|Add Alpha %x7", xco+10,yco,butw,19, &brush->blend, 0, 0, 0, 0, "Blending method for applying brushes");

			uiDefButBitS(block, TOG|BIT, BRUSH_TORUS, B_BRUSHCHANGE, "Wrap",	xco+10,yco-25,butw,19, &brush->flag, 0, 0, 0, 0, "Enables torus wrapping");

			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOG|BIT, BRUSH_AIRBRUSH, B_BRUSHCHANGE, "Airbrush",	xco+10,yco-50,butw,19, &brush->flag, 0, 0, 0, 0, "Keep applying paint effect while holding mouse (spray)");
			uiDefButF(block, NUM, B_NOP, "Rate ", xco+10,yco-70,butw,19, &brush->rate, 0.01, 1.0, 0, 0, "Number of paints per second for Airbrush");
			uiBlockEndAlign(block);

			yco -= 25;

			uiBlockBeginAlign(block);
			uiDefButF(block, COL, B_VPCOLSLI, "",					0,yco,200,19, brush->rgb, 0, 0, 0, 0, "");
			uiDefButF(block, NUMSLI, B_NOP, "Opacity ",		0,yco-20,180,19, &brush->alpha, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
			uiDefButBitS(block, TOG|BIT, BRUSH_ALPHA_PRESSURE, B_NOP, "P",	180,yco-20,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
			uiDefButI(block, NUMSLI, B_NOP, "Size ",		0,yco-40,180,19, &brush->size, 1, 200, 0, 0, "The size of the brush");
			uiDefButBitS(block, TOG|BIT, BRUSH_SIZE_PRESSURE, B_NOP, "P",	180,yco-40,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
			uiDefButF(block, NUMSLI, B_NOP, "Falloff ",		0,yco-60,180,19, &brush->innerradius, 0.0, 1.0, 0, 0, "The fall off radius of the brush");
			uiDefButBitS(block, TOG|BIT, BRUSH_RAD_PRESSURE, B_NOP, "P",	180,yco-60,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
			uiDefButF(block, NUMSLI, B_NOP, "Spacing ",0,yco-80,180,19, &brush->spacing, 1.0, 100.0, 0, 0, "Repeating paint on %% of brush diameter");
		uiDefButBitS(block, TOG|BIT, BRUSH_SPACING_PRESSURE, B_NOP, "P",	180,yco-80,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
			uiBlockEndAlign(block);

			yco -= 110;

			uiBlockSetCol(block, TH_BUT_SETTING2);
			id= (mtex)? (ID*)mtex->tex: NULL;
			xco= std_libbuttons(block, 0, yco, 0, NULL, B_BTEXBROWSE, ID_TE, 0, id, NULL, &(G.buts->menunr), 0, 0, B_BTEXDELETE, 0, 0);
			/*uiDefButBitS(block, TOG|BIT, BRUSH_FIXED_TEX, B_BRUSHCHANGE, "Fixed",	xco+5,yco,butw,19, &brush->flag, 0, 0, 0, 0, "Keep texture origin in fixed position");*/
			uiBlockSetCol(block, TH_AUTO);
		}
	}
}

static void editing_panel_mesh_texface(void)
{
	uiBlock *block;
	MTFace *tf;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_texface", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Multires", "Editing");
	if(uiNewPanel(curarea, block, "Texture Face", "Editing", 960, 0, 318, 204)==0) return;
	
	tf = get_active_mtface(NULL, NULL, 0);
	if(tf) {
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, TF_TEX, B_REDR_3D_IMA, "Tex",	600,160,60,19, &tf->mode, 0, 0, 0, 0, "Render face with texture");
		uiDefButBitS(block, TOG, TF_TILES, B_REDR_3D_IMA, "Tiles",	660,160,60,19, &tf->mode, 0, 0, 0, 0, "Use tilemode for face");
		uiDefButBitS(block, TOG, TF_LIGHT, REDRAWVIEW3D, "Light",	720,160,60,19, &tf->mode, 0, 0, 0, 0, "Use light for face");
		uiDefButBitS(block, TOG, TF_INVISIBLE, REDRAWVIEW3D, "Invisible",780,160,60,19, &tf->mode, 0, 0, 0, 0, "Make face invisible");
		uiDefButBitS(block, TOG, TF_DYNAMIC, REDRAWVIEW3D, "Collision", 840,160,60,19, &tf->mode, 0, 0, 0, 0, "Use face for collision detection");

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, TF_SHAREDCOL, REDRAWVIEW3D, "Shared",	600,135,60,19, &tf->mode, 0, 0, 0, 0, "Blend vertex colors across face when vertices are shared");
		uiDefButBitS(block, TOG, TF_TWOSIDE, REDRAWVIEW3D, "Twoside",660,135,60,19, &tf->mode, 0, 0, 0, 0, "Render face twosided");
		uiDefButBitS(block, TOG, TF_OBCOL, REDRAWVIEW3D, "ObColor",720,135,60,19, &tf->mode, 0, 0, 0, 0, "Use ObColor instead of vertex colors");

		uiBlockBeginAlign(block);
		
		uiDefButBitS(block, TOG, TF_BILLBOARD, B_TFACE_HALO, "Halo",	600,110,60,19, &tf->mode, 0, 0, 0, 0, "Screen aligned billboard");
		uiDefButBitS(block, TOG, TF_BILLBOARD2, B_TFACE_BILLB, "Billboard",660,110,60,19, &tf->mode, 0, 0, 0, 0, "Billboard with Z-axis constraint");
		uiDefButBitS(block, TOG, TF_SHADOW, REDRAWVIEW3D, "Shadow", 720,110,60,19, &tf->mode, 0, 0, 0, 0, "Face is used for shadow");
		uiDefButBitS(block, TOG, TF_BMFONT, REDRAWVIEW3D, "Text", 780,110,60,19, &tf->mode, 0, 0, 0, 0, "Enable bitmap text on face");

		uiBlockBeginAlign(block);
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButC(block, ROW, REDRAWVIEW3D, "Opaque",	600,80,60,19, &tf->transp, 2.0, (float)TF_SOLID,0, 0, "Render color of textured face as color");
		uiDefButC(block, ROW, REDRAWVIEW3D, "Add",		660,80,60,19, &tf->transp, 2.0, (float)TF_ADD,	0, 0, "Render face transparent and add color of face");
		uiDefButC(block, ROW, REDRAWVIEW3D, "Alpha",	720,80,60,19, &tf->transp, 2.0, (float)TF_ALPHA,0, 0, "Render polygon transparent, depending on alpha channel of the texture");
	}
	else
		uiDefBut(block,LABEL,B_NOP, "(No Active Face)", 10,200,150,19,0,0,0,0,0,"");

}

void do_uvcalculationbuts(unsigned short event)
{
	switch(event) {
	case B_UVAUTO_DRAWFACES:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
	}
}

static void editing_panel_mesh_uvautocalculation(void)
{
	uiBlock *block;
	int butH= 19, butHB= 20, row= 180, butS= 10;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_uvautocalculation", UI_EMBOSS, UI_HELV, curarea->win);
	/* make this a tab of "Texture face" to save screen space*/
	uiNewPanelTabbed("Multires", "Editing");
	if(uiNewPanel(curarea, block, "UV Calculation", "Editing", 960, 0, 318, 204)==0)
		return;
	row-= 4*butHB+butS;

	uiBlockBeginAlign(block);
	uiDefButS(block, MENU, REDRAWBUTSEDIT, "Unwrapper%t|Conformal%x0|Angle Based%x1",100,row,200,butH, &G.scene->toolsettings->unwrapper, 0, 0, 0, 0, "Unwrap method");
	uiDefButBitS(block, TOG, UVCALC_FILLHOLES, B_NOP, "Fill Holes",100,row-butHB,200,butH,&G.scene->toolsettings->uvcalc_flag, 0, 0, 0, 0,  "Fill holes to prevent internal overlaps");
	uiBlockEndAlign(block);
	row-= 2*butHB+butS;

	row= 180;

	uiDefButBitS(block, TOGN, UVCALC_NO_ASPECT_CORRECT, B_NOP, "Image Aspect",100,row,200,butH,&G.scene->toolsettings->uvcalc_flag, 0, 0, 0, 0,  "Scale the UV Unwrapping to correct for the current images aspect ratio");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,B_UVAUTO_CUBESIZE ,"Cube Size:",315,row,200,butH, &G.scene->toolsettings->uvcalc_cubesize, 0.0001, 100.0, 10, 3, "Defines the cubemap size for cube mapping");
	uiBlockEndAlign(block);
	row-= butHB+butS;

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,B_UVAUTO_CYLRADIUS ,"Cyl Radius:",315,row,200,butH, &G.scene->toolsettings->uvcalc_radius, 0.1, 100.0, 10, 3, "Defines the radius of the UV mapping cylinder");
	uiBlockEndAlign(block);
	row-= butHB+butS;

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_UVAUTO_FACE,"View Aligns Face",315,row,200,butH, &G.scene->toolsettings->uvcalc_mapdir,2.0, 1.0, 0.0,0.0, "View is on equator for cylindrical and spherical UV mapping");
	uiDefButS(block, ROW,B_UVAUTO_TOP,"VA Top",315,row-butHB,100,butH, &G.scene->toolsettings->uvcalc_mapdir,2.0, 0.0, 0.0,0.0, "View is on poles for cylindrical and spherical UV mapping");
	uiDefButS(block, ROW,B_UVAUTO_TOP,"Al Obj",415,row-butHB,100,butH, &G.scene->toolsettings->uvcalc_mapdir,2.0, 2.0, 0.0,0.0, "Align to object for cylindrical and spherical UV mapping");
	uiBlockEndAlign(block);
	row-= 2*butHB+butS;

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_UVAUTO_ALIGNX,"Polar ZX",315,row,100,butH, &G.scene->toolsettings->uvcalc_mapalign,2.0, 0.0, 0.0,0.0, "Polar 0 is X for cylindrical and spherical UV mapping");
	uiDefButS(block, ROW,B_UVAUTO_ALIGNY,"Polar ZY",415,row,100,butH, &G.scene->toolsettings->uvcalc_mapalign,2.0, 1.0, 0.0,0.0, "Polar 0 is Y for cylindrical and spherical UV mapping");
	uiBlockEndAlign(block);
}

void editing_panel_mesh_multires()
{
	uiBlock *block;
	uiBut *but;
	Object *ob= OBACT;
	Mesh *me= get_mesh(ob);
	int cx= 100, cy= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_multires", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Multires", "Editing", 500, 0, 318, 204)==0) return;
	
	uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	if(!me->mr) {
		but= uiDefBut(block,BUT,B_NOP,"Add Multires", cx,cy,268,19,0,0,0,0,0,"Allow editing of the mesh at multiple subdivision levels (disables distructive mesh editing)");
		uiButSetFunc(but,multires_make,ob,me);
	} else {
		char subsurfmenu[]= "Subsurf Type%t|Catmull-Clark%x0|Simple Subdiv.%x1";

		but= uiDefBut(block,BUT,B_NOP,"Apply Multires", cx,cy,268,19,0,0,0,0,0,"Apply current multires level to mesh and the delete other levels");
		uiButSetFunc(but,multires_delete,ob,me);
		cy-= 24;

		uiBlockBeginAlign(block);
		but= uiDefBut(block,BUT,B_NOP,"Add Level", cx,cy,134,19,0,0,0,0,0,"Add a new level of subdivision at the end of the chain");
		uiButSetFunc(but, multires_subdivide, ob, me);
		uiDefButC(block, MENU, B_NOP, subsurfmenu, cx + 134, cy, 134, 19, &G.scene->toolsettings->multires_subdiv_type, 0, 0, 0, 0, "Selects type of subdivision algorithm.");
		cy-= 20;

		if(me->mr->level_count>1) {
			but= uiDefBut(block,BUT,B_NOP,"Del Lower", cx,cy,134,19,0,0,0,0,0,"Remove all levels of subdivision below the current one");
			uiButSetFunc(but,multires_del_lower,ob,me);
			but= uiDefBut(block,BUT,B_NOP,"Del Higher", cx+134,cy,134,19,0,0,0,0,0,"Remove all levels of subdivision above the current one");
			uiButSetFunc(but,multires_del_higher,ob,me);
			cy-= 20;
		
			but= uiDefButC(block,NUM,B_NOP,"Level: ",cx,cy,268,19,(char *)&me->mr->newlvl,1.0,me->mr->level_count,0,0,"");
			uiButSetFunc(but,multires_set_level_cb, ob, me);
			cy-= 20;

			but= uiDefButC(block,NUM,B_NOP,"Edges: ",cx,cy,268,19,(char *)&me->mr->edgelvl,1.0,me->mr->level_count,0,0,"Set level of edges to display");
			uiButSetFunc(but,multires_edge_level_update_cb,ob,me);
			cy-= 20;
			uiBlockEndAlign(block);
			
			cy-= 5;
			uiDefBut(block,LABEL,B_NOP,"Rendering",cx,cy,100,19,0,0,0,0,0,"");
			cy-= 20;

			uiBlockBeginAlign(block);
			uiDefButC(block,NUM,B_NOP,"Pin: ",cx,cy,268,19,(char *)&me->mr->pinlvl,1.0,me->mr->level_count,0,0,"Set level to apply modifiers to during render");
			cy-= 20;

			uiDefButC(block,NUM,B_NOP,"Render: ",cx,cy,268,19,(char *)&me->mr->renderlvl,1.0,me->mr->level_count,0,0,"Set level to render");
			cy-= 20;
			
			if(multires_modifier_warning()) {
				char *tip= "One or more modifiers are enabled that modify mesh topology";
				uiDefIconBut(block,LABEL,B_NOP,ICON_ERROR, cx,cy,20,20, 0,0,0,0,0, tip);
				uiDefBut(block,LABEL,B_NOP, "Cannot use render level", cx+20,cy,180,19, 0,0,0,0,0, tip);
			}
		}
	}

	uiBlockEndAlign(block);
}

void particle_edit_buttons(uiBlock *block)
{
	Object *ob=OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleEditSettings *pset = PE_settings();
	ParticleEdit *edit;
	uiBut *but;
	short butx=10,buty=150,butw=150,buth=20, lastbuty;
	static short partact;

	char *menustr;
	
	if(psys==NULL) return;
	
	menustr = psys_menu_string(ob, 0);
	partact = PE_get_current_num(ob)+1;
	
	but=uiDefButS(block, MENU, B_BAKE_REDRAWEDIT, menustr, 160,180,butw,buth, &partact, 14.0, 0.0, 0, 0, "Browse systems");
	uiButSetFunc(but, PE_change_act, ob, &partact);

	MEM_freeN(menustr);

	if(psys->edit) {
		edit= psys->edit;

		/* brushes (the update evend needs to be B_BAKE_RECACHE so that path colors are updated properly) */
		uiBlockBeginAlign(block);	
		uiDefButS(block,ROW,B_BAKE_RECACHE,"None",butx,buty,75,19,&pset->brushtype,14.0,PE_BRUSH_NONE,0,0,"Disable brush");
		uiDefButS(block,ROW,B_BAKE_RECACHE,"Comb",butx+75,buty,75,19,&pset->brushtype,14.0,PE_BRUSH_COMB,0,0,"Comb hairs");
		uiDefButS(block,ROW,B_BAKE_RECACHE,"Smooth",butx+150,buty,75,19,&pset->brushtype,14.0,PE_BRUSH_SMOOTH,0,0,"Smooth hairs");
		uiDefButS(block,ROW,B_BAKE_RECACHE,"Weight",butx+225,buty,75,19,&pset->brushtype,14,PE_BRUSH_WEIGHT,0,0,"Weight hairs");
		buty-= buth;
		uiDefButS(block,ROW,B_BAKE_RECACHE,"Add", butx,buty,75,19,&pset->brushtype,14,PE_BRUSH_ADD,0,0,"Add hairs");
		uiDefButS(block,ROW,B_BAKE_RECACHE,"Length", butx+75,buty,75,19,&pset->brushtype,14, PE_BRUSH_LENGTH,0,0,"Make hairs longer or shorter");
		uiDefButS(block,ROW,B_BAKE_RECACHE,"Puff", butx+150,buty,75,19,&pset->brushtype,14, PE_BRUSH_PUFF,0,0,"Make hairs stand up");
		uiDefButS(block,ROW,B_BAKE_RECACHE,"Cut", butx+225,buty,75,19,&pset->brushtype,14, PE_BRUSH_CUT,0,0,"Cut hairs");
		uiBlockEndAlign(block);

		buty-= 10;
		lastbuty= buty;

		/* brush options */
		if(pset->brushtype>=0) {
			ParticleBrushData *brush= &pset->brush[pset->brushtype];

			butw= 180;

			uiBlockBeginAlign(block);
			uiDefButS(block, NUMSLI, B_BAKE_REDRAWEDIT, "Size:", butx,(buty-=buth),butw,buth, &brush->size, 1.0, 100.0, 1, 1, "Brush size");
			uiDefButS(block, NUMSLI, B_BAKE_REDRAWEDIT, "Strength:", butx,(buty-=buth),butw,buth, &brush->strength, 1.0, 100.0, 1, 1, "Brush strength");

			if(ELEM(pset->brushtype, PE_BRUSH_LENGTH, PE_BRUSH_PUFF)) {
				char *str1, *str2, *tip1, *tip2;

				if(pset->brushtype == PE_BRUSH_LENGTH) {
					str1= "Grow"; tip1= "Make hairs longer [Shift]";
					str2= "Shrink"; tip2= "Make hairs shorter [Shift]";
				}
				else /*if(pset->brushtype == PE_BRUSH_PUFF)*/ {
					str1= "Add"; tip1= "Make hair more puffy [Shift]";
					str2= "Sub"; tip2= "Make hair less puffy [Shift]";
				}

				uiDefButS(block,ROW,B_NOP,str1, butx,(buty-=buth),butw/2,buth,&brush->invert,0.0,0.0,0, 0,tip1);
				uiDefButS(block,ROW,B_NOP,str2, butx+butw/2,buty,butw/2,buth,&brush->invert,0.0,1.0,0, 0,tip2);
			}
			uiBlockEndAlign(block);

			butx += butw+10;
			buty= lastbuty;
			butw= 110;

			if(pset->brushtype==PE_BRUSH_ADD) {
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, PE_INTERPOLATE_ADDED, B_BAKE_REDRAWEDIT, "Interpolate",	butx,(buty-=buth),butw,buth, &pset->flag, 0, 0, 0, 0, "Interpolate new particles from the existing ones");
				uiDefButS(block, NUMSLI, B_BAKE_REDRAWEDIT, "Step:",	butx,(buty-=buth),butw,buth, &brush->step, 1.0, 50.0, 1, 1, "Brush step");
				uiDefButS(block, NUMSLI, B_BAKE_REDRAWEDIT, "Keys:",	butx,(buty-=buth),butw,buth, &pset->totaddkey, 2.0, 20.0, 1, 1, "How many keys to make new particles with");
				uiBlockEndAlign(block);
			}
		}

		/* keep options */
		butw= 150;
		butx= 10;
		buty= lastbuty - (buth*3 + 10);
		lastbuty= buty;

		uiDefBut(block, LABEL, 0, "Keep",	butx,(buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, PE_KEEP_LENGTHS, B_BAKE_REDRAWEDIT, "Lengths",	butx,(buty-=buth),butw/2,buth, &pset->flag, 0, 0, 0, 0, "Keep path lengths constant");
		uiDefButBitS(block, TOG, PE_LOCK_FIRST, B_BAKE_REDRAWEDIT, "Root",	 butx+butw/2,buty,butw/2,buth, &pset->flag, 0, 0, 0, 0, "Keep first keys unmodified");
		uiBlockEndAlign(block);

		buty -= 5;

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, PE_DEFLECT_EMITTER, B_BAKE_REDRAWEDIT, "Deflect Emitter",	butx,(buty-=buth),butw,buth, &pset->flag, 0, 0, 0, 0, "Keep paths from intersecting the emitter");
		uiDefButF(block, NUM, B_BAKE_REDRAWEDIT, "Dist:",		butx,(buty-=buth),butw,buth, &pset->emitterdist, 0.0, 10.0, 1, 1, "Distance from emitter");
		uiBlockEndAlign(block);

		buty= lastbuty;
		butx += butw+10;
		butw -= 10;

		uiDefBut(block, LABEL, 0, "Draw",	butx,(buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefButS(block, NUMSLI, B_BAKE_RECACHE, "Steps:",	butx,(buty-=buth),butw,buth, &psys->part->draw_step, 0.0, 10.0, 1, 1, "Drawing accuracy of paths");
		uiBlockEndAlign(block);

		buty -= 5;

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, PE_SHOW_TIME, B_BAKE_REDRAWEDIT, "Show Time",	butx,(buty-=buth),butw,buth, &pset->flag, 0, 0, 0, 0, "Show time values of the baked keys");
		uiDefButBitS(block, TOG, PE_SHOW_CHILD, B_BAKE_RECACHE, "Show Children",	butx,(buty-=buth),butw,buth, &pset->flag, 0, 0, 0, 0, "Show child particles in particle mode");
		uiBlockEndAlign(block);
	}
	else{
		uiDefBut(block, LABEL, 0, "System isn't editable",	butx,(buty-=buth),250,buth, NULL, 0.0, 0, 0, 0, "");
	}
}
/* this is a mode context sensitive system */

void editing_panels()
{
	Object *ob;
	Curve *cu;
	MetaBall *mb;
	Lattice *lt;
	bArmature *arm;
	Camera *cam;

	ob= OBACT;
	if(ob==NULL) return;
	uiSetButLock(object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	switch(ob->type) {
	case OB_MESH:
		editing_panel_links(ob);
		editing_panel_mesh_type(ob, ob->data);
		editing_panel_modifiers(ob);
		editing_panel_shapes(ob);
		editing_panel_mesh_multires();
		/* modes */
		if(G.obedit) {
			editing_panel_mesh_tools(ob, ob->data);
			editing_panel_mesh_tools1(ob, ob->data);
			uiNewPanelTabbed("Mesh Tools 1", "Editing");
			
			if (G.rt == 42) /* hidden for now, no time for docs */
				editing_panel_mesh_skgen(ob, ob->data);
			
			editing_panel_mesh_uvautocalculation();
			if (EM_texFaceCheck())
				editing_panel_mesh_texface();
		}
		else if(G.f & G_SCULPTMODE) {
			uiNewPanelTabbed("Multires", "Editing");
			editing_panel_sculpting_tools();
			uiNewPanelTabbed("Multires", "Editing");
			editing_panel_sculpting_brush();
			uiNewPanelTabbed("Multires", "Editing");
			editing_panel_sculpting_textures();
		} else {
			if(G.f & (G_VERTEXPAINT | G_TEXTUREPAINT | G_WEIGHTPAINT) ) {
				editing_panel_mesh_paint();
			}
		}
		break;

	case OB_CURVE:
	case OB_SURF:
		cu= ob->data;
		editing_panel_links(ob);
		editing_panel_curve_type(ob, cu);
		editing_panel_modifiers(ob);
//		editing_panel_shapes(ob);
		if(G.obedit) {
			editing_panel_curve_tools(ob, cu);
			editing_panel_curve_tools1(ob, cu);
		}
		break;

	case OB_MBALL:
		mb= ob->data;
		editing_panel_links(ob);
		editing_panel_mball_type(ob, mb);
		if(G.obedit) {
			editing_panel_mball_tools(ob, mb);
		}
		break;

	case OB_FONT:
		cu= ob->data;
		editing_panel_links(ob);
		editing_panel_curve_type(ob, cu);
		editing_panel_font_type(ob, cu);

#ifdef INTERNATIONAL
		if(G.obedit)
		{
			editing_panel_char_type(ob, cu);
		}
#endif
		editing_panel_modifiers(ob);
		break;

	case OB_LATTICE:
		lt= ob->data;
		editing_panel_links(ob);
		editing_panel_lattice_type(ob, lt);
		editing_panel_modifiers(ob);
		editing_panel_shapes(ob);
		break;

	case OB_LAMP:
		editing_panel_links(ob);
		break;

	case OB_EMPTY:
		editing_panel_links(ob);
		break;

	case OB_CAMERA:
		cam= ob->data;
		editing_panel_links(ob); // no editmode!
		editing_panel_camera_type(ob, cam);
		/* yafray: extra panel for dof parameters */
		if (G.scene->r.renderer==R_YAFRAY) editing_panel_camera_yafraydof(ob, cam);
		break;

	case OB_ARMATURE:
		arm= ob->data;
		editing_panel_links(ob); // no editmode!
		editing_panel_armature_type(ob, arm);
		if(G.obedit) {
			editing_panel_armature_bones(ob, arm);
		}
		else if(ob->flag & OB_POSEMODE) {
			editing_panel_armature_visuals(ob, arm);
			editing_panel_pose_bones(ob, arm);
			object_panel_constraint("Editing");
		}		
		break;
	}
	uiClearButLock();
}
