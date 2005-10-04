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
 */

#include <time.h>
#include <math.h>
#include <stdlib.h>
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
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_radio_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_packedFile_types.h"

#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_packedFile.h"
#include "BKE_scene.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_vfontdata.h"
#include "BLI_editVert.h"
#include "BLI_dynstr.h"

#include "BSE_filesel.h"

#include "BIF_gl.h"
#include "BIF_editarmature.h"
#include "BIF_editconstraint.h"
#include "BIF_editdeform.h"
#include "BIF_editfont.h"
#include "BIF_editkey.h"
#include "BIF_editmesh.h"
#include "BIF_interface.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_poseobject.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_scrarea.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_previewrender.h"
#include "BIF_butspace.h"

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
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BDR_drawobject.h"
#include "BDR_editcurve.h"
#include "BDR_editface.h"
#include "BDR_editobject.h"
#include "BDR_vpaint.h"
#include "BDR_unwrapper.h"

#include "BSE_drawview.h"
#include "BSE_editipo.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"
#include "BSE_buttons.h"
#include "BSE_seqaudio.h"

#include "RE_renderconverter.h"		// make_sticky

#include "butspace.h" // own module

static float editbutweight=1.0;
float editbutvweight=1;
float uv_calc_radius= 1.0, uv_calc_cubesize= 1.0;
short uv_calc_mapdir= 1, uv_calc_mapalign= 1, facesel_draw_edges= 0;

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
				efa= em->faces.first;
				while(efa) {
					if(efa->f & SELECT) {
						if(index== -1) index= efa->mat_nr;
						else if(index!=efa->mat_nr) {
							error("Mixed colors");
							return;
						}
					}
					efa= efa->next;
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
			BIF_undo_push("Assign material index");
		}
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
				BIF_undo_push("Select material index");
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
		if(ob && G.obedit==0) {
			if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) tex_space_curve(ob->data);
		}
		break;
	case B_DOCENTRE:
		docentre(0);
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
			if( (ob->lay & G.vd->lay) && (BASACT->lay & G.vd->lay) );
			else if( (ob->lay & G.vd->lay)==0 && (BASACT->lay & G.vd->lay)==0 );
			else allqueue(REDRAWVIEW3D, 0);
			
			ob->lay= BASACT->lay;
		}
	}

}

/* *************************** MESH  ******************************** */

static void editing_panel_mesh_type(Object *ob, Mesh *me)
{
	uiBlock *block;
	float val;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_type", UI_EMBOSS, UI_HELV, curarea->win);
	if( uiNewPanel(curarea, block, "Mesh", "Editing", 320, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, ME_AUTOSMOOTH, REDRAWVIEW3D, "Auto Smooth",10,180,154,19, &me->flag, 0, 0, 0, 0, "Treats all set-smoothed faces with angles less than Degr: as 'smooth' during render");
	uiDefButS(block, NUM, B_DIFF, "Degr:",				10,160,154,19, &me->smoothresh, 1, 80, 0, 0, "Defines maximum angle between face normals that 'Auto Smooth' will operate on");

	uiBlockBeginAlign(block);
	uiBlockSetCol(block, TH_AUTO);

	if(me->mcol) val= 1.0; else val= 0.0;
	uiDefBut(block, LABEL, 0, "VertCol", 				10,50,70,20, 0, val, 0, 0, 0, "");
	if(me->mcol==NULL) {
		uiDefBut(block, BUT, B_MAKEVERTCOL, "Make",		80,50,84,19, 0, 0, 0, 0, 0, "Enables vertex colour painting on active Mesh");
	}
	else uiDefBut(block, BUT, B_DELVERTCOL, "Delete", 	80,50,84,19, 0, 0, 0, 0, 0, "Deletes vertex colours on active Mesh");

	if(me->tface) val= 1.0; else val= 0.0;
	uiDefBut(block, LABEL, 0, "TexFace", 				10,30,70,20, 0, val, 0, 0, 0, "");
	if(me->tface==NULL) {
		uiDefBut(block, BUT, B_MAKE_TFACES, "Make",		80,30,84,19, 0, 0, 0, 0, 0, "Enables the active Mesh's faces for UV coordinate mapping");
	}
	else uiDefBut(block, BUT, B_DEL_TFACES, "Delete", 	80,30,84,19, 0, 0, 0, 0, 0, "Deletes UV coordinates for active Mesh's faces");

	if(me->msticky) val= 1.0; else val= 0.0;
	uiDefBut(block, LABEL, 0, "Sticky", 				10,10,70,20, 0, val, 0, 0, 0, "");
	if(me->msticky==NULL) {
		uiDefBut(block, BUT, B_MAKESTICKY, "Make",		80,10,84,19, 0, 0, 0, 0, 0, "Creates Sticky coordinates for the active Mesh from the current camera view background picture");
	}
	else uiDefBut(block, BUT, B_DELSTICKY, "Delete", 	80,10,84,19, 0, 0, 0, 0, 0, "Deletes Sticky texture coordinates");

	uiBlockEndAlign(block);

	uiDefIDPoinBut(block, test_meshpoin_but, 0, "TexMesh: ",	175,124,230,19, &me->texcomesh, "Enter the name of a Meshblock");

	if(me->key) {
		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_DIFF, "Slurph:",				175,95,95,19, &(me->key->slurph), -500.0, 500.0, 0, 0, "");
		uiDefButS(block, TOG, B_RELKEY, "Relative Keys",		175,75,95,19, &me->key->type, 0, 0, 0, 0, "");
	}

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_SLOWERDRAW,"SlowerDraw",			175,30,95,19, 0, 0, 0, 0, 0, "Displays the active object with all possible edges shown");
	uiDefBut(block, BUT, B_FASTERDRAW,"FasterDraw",			175,10,95,19, 0, 0, 0, 0, 0, "Displays the active object faster by omitting some edges when drawing");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_DOCENTRE, "Centre",					275, 95, 130, 19, 0, 0, 0, 0, 0, "Shifts object data to be centered about object's origin");
	uiDefBut(block, BUT,B_DOCENTRENEW, "Centre New",			275, 75, 130, 19, 0, 0, 0, 0, 0, "Shifts object's origin to center of object data");
	uiDefBut(block, BUT,B_DOCENTRECURSOR, "Centre Cursor",		275, 55, 130, 19, 0, 0, 0, 0, 0, "Shifts object's origin to cursor location");

	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, ME_TWOSIDED, REDRAWVIEW3D, "Double Sided",	275,30,130,19, &me->flag, 0, 0, 0, 0, "Render/display the mesh as double or single sided");
	uiDefButBitS(block, TOG, ME_NOPUNOFLIP, REDRAWVIEW3D, "No V.Normal Flip",275,10,130,19, &me->flag, 0, 0, 0, 0, "Disables flipping of vertexnormals during render");
	uiBlockEndAlign(block);

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
		ob->softflag |= OB_SB_RESET;
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWOOPS, 0);
		countall();
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
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

static uiBlock *modifiers_add_menu(void *ob_v)
{
	Object *ob = ob_v;
	uiBlock *block;
	int i, yco=0;
	
	block= uiNewBlock(&curarea->uiblocks, "modifier_add_menu", UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, modifiers_add, ob);

	for (i=eModifierType_None+1; i<NUM_MODIFIER_TYPES; i++) {
		ModifierTypeInfo *mti = modifierType_getInfo(i);

			/* Only allow adding through appropriate other interfaces */
		if (ELEM(i, eModifierType_Softbody, eModifierType_Hook)) continue;
			
		if (	(mti->flags&eModifierTypeFlag_AcceptsCVs) || 
				(ob->type==OB_MESH && (mti->flags&eModifierTypeFlag_AcceptsMesh))) {
			uiDefBut(block, BUTM, B_MODIFIER_RECALC, mti->name,		0, yco-=20, 160, 19, NULL, 0, 0, 1, i, "");
		}
	}
	
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

	BLI_remlink(&ob->modifiers, md_v);

	modifier_free(md_v);

	BIF_undo_push("Del modifier");
}

static void modifiers_moveUp(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;

	if (md->prev) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->type!=eModifierTypeType_OnlyDeform) {
			ModifierTypeInfo *nmti = modifierType_getInfo(md->prev->type);

			if (nmti->flags&eModifierTypeFlag_RequiresOriginalData) {
				error("Cannot move above a modifier requiring original data.");
				return;
			}
		}

		BLI_remlink(&ob->modifiers, md);
		BLI_insertlink(&ob->modifiers, md->prev->prev, md);
	}

	BIF_undo_push("Move modifier");
}

static void modifiers_moveDown(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;

	if (md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->flags&eModifierTypeFlag_RequiresOriginalData) {
			ModifierTypeInfo *nmti = modifierType_getInfo(md->next->type);

			if (nmti->type!=eModifierTypeType_OnlyDeform) {
				error("Cannot move beyond a non-deforming modifier.");
				return;
			}
		}

		BLI_remlink(&ob->modifiers, md);
		BLI_insertlink(&ob->modifiers, md->next, md);
	}

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
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_MESH) {
				error ("Boolean modifier object must be a mesh");
				break;
			} 
			*idpp= id;
			return;
		}
	}
	*idpp= 0;
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

static void modifiers_applyModifier(void *obv, void *mdv)
{
	Object *ob = obv;
	ModifierData *md = mdv;
	DerivedMesh *dm;
	DispListMesh *dlm;
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
		dm = mesh_create_derived_for_modifier(ob, md);
		if (!dm) {
			error("Modifier is disabled or returned error, skipping apply");
			return;
		}

		dlm= dm->convertToDispListMesh(dm, 0);

		if ((!me->tface || dlm->tface) || okee("Applying will delete mesh UVs and vertex colors")) {
			if ((!me->mcol || dlm->mcol) || okee("Applying will delete mesh vertex colors")) {
				if (dlm->totvert==me->totvert || okee("Applying will delete mesh sticky, keys, and vertex groups")) {
					displistmesh_to_mesh(dlm, me);
					converted = 1;
				}
			}
		}

		if (!converted) {
			displistmesh_free(dlm);
		}
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
	
	for (md=ob->modifiers.first; md; md=md->next)
		if (md!=md_v)
			md->mode &= ~eModifierMode_OnCage;

	md = md_v;
	md->mode ^= eModifierMode_OnCage;
}

static void modifiers_clearHookOffset(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;
	
	if (hmd->object) {
		Mat4Invert(hmd->object->imat, hmd->object->obmat);
		Mat4MulSerie(hmd->parentinv, hmd->object->imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
		BIF_undo_push("Clear hook");
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
		if (md->type!=eModifierType_Softbody || !(ob->pd && ob->pd->deflect)) {
			uiDefIconButBitI(block, TOG, eModifierMode_Render, B_MODIFIER_RECALC, ICON_SCENE, x+10+buttonWidth-60, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during rendering");
			uiDefIconButBitI(block, TOG, eModifierMode_Realtime, B_MODIFIER_RECALC, VICON_VIEW3D, x+10+buttonWidth-40, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during interactive display");
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

		but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, VICON_X, x+width-70+40, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Delete modifier");
		uiButSetFunc(but, modifiers_del, ob, md);
		uiBlockSetCol(block, TH_AUTO);
	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	if (isVirtual || !(md->mode&eModifierMode_Expanded)) {
		y -= 18;
	} else {
		int cy = y - 8;
		int lx = x + width - 60 - 15;

		if (md->type==eModifierType_Subsurf) {
			height = 86;
		} else if (md->type==eModifierType_Lattice) {
			height = 46;
		} else if (md->type==eModifierType_Curve) {
			height = 46;
		} else if (md->type==eModifierType_Build) {
			height = 86;
		} else if (md->type==eModifierType_Mirror) {
			height = 46;
		} else if (md->type==eModifierType_Decimate) {
			height = 46;
		} else if (md->type==eModifierType_Wave) {
			height = 200;
		} else if (md->type==eModifierType_Armature) {
			height = 46;
		} else if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;
			height = 86;
			if (editing)
				height += 20;
			if(hmd->indexar==NULL)
				height += 20;
		} else if (md->type==eModifierType_Softbody) {
			height = 26;
		} else if (md->type==eModifierType_Boolean) {
			height = 46;
		}

							/* roundbox 4 free variables: corner-rounding, nop, roundbox type, shade */
		uiDefBut(block, ROUNDBOX, 0, "", x-10, y-height-2, width, height-2, NULL, 5.0, 0.0, 12, 40, ""); 

		y -= 18;

		if (!isVirtual) {
			uiBlockBeginAlign(block);
			but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Apply",	lx,(cy-=19),60,19, 0, 0, 0, 0, 0, "Apply the current modifier and remove from the stack");
			uiButSetFunc(but, modifiers_applyModifier, ob, md);
			if (md->type!=eModifierType_Softbody) {
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
			uiDefButS(block, NUM, B_MODIFIER_RECALC, "Render Levels:",		lx, (cy-=19), buttonWidth,19, &smd->renderLevels, 1, 6, 0, 0, "Number subdivisions to perform when rendering");

			/* Disabled until non-EM DerivedMesh implementation is complete */

			/*
			uiDefButBitS(block, TOG, eSubsurfModifierFlag_Incremental, B_MODIFIER_RECALC, "Incremental", lx, (cy-=19),90,19,&smd->flags, 0, 0, 0, 0, "Use incremental calculation, even outside of mesh mode");
			uiDefButBitS(block, TOG, eSubsurfModifierFlag_DebugIncr, B_MODIFIER_RECALC, "Debug", lx+90, cy,buttonWidth-90,19,&smd->flags, 0, 0, 0, 0, "Visualize the subsurf incremental calculation, for debugging effect of other modifiers");
			*/

			uiDefButBitS(block, TOG, eSubsurfModifierFlag_ControlEdges, B_MODIFIER_RECALC, "Optimal Draw", lx, (cy-=19), buttonWidth,19,&smd->flags, 0, 0, 0, 0, "Skip drawing/rendering of interior subdivided edges");
		} else if (md->type==eModifierType_Lattice) {
			LatticeModifierData *lmd = (LatticeModifierData*) md;
			uiDefIDPoinBut(block, modifier_testLatticeObj, B_CHANGEDEP, "Ob: ",	lx, (cy-=19), buttonWidth,19, &lmd->object, "Lattice object to deform with");
		} else if (md->type==eModifierType_Curve) {
			CurveModifierData *cmd = (CurveModifierData*) md;
			uiDefIDPoinBut(block, modifier_testCurveObj, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &cmd->object, "Curve object to deform with");
		} else if (md->type==eModifierType_Build) {
			BuildModifierData *bmd = (BuildModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Start:", lx, (cy-=19), buttonWidth,19, &bmd->start, 1.0, MAXFRAMEF, 100, 0, "Specify the start frame of the effect");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Length:", lx, (cy-=19), buttonWidth,19, &bmd->length, 1.0, MAXFRAMEF, 100, 0, "Specify the total time the build effect requires");
			uiDefButI(block, TOG, B_MODIFIER_RECALC, "Randomize", lx, (cy-=19), buttonWidth,19, &bmd->randomize, 0, 0, 1, 0, "Randomize the faces or edges during build.");
			uiDefButI(block, NUM, B_MODIFIER_RECALC, "Seed:", lx, (cy-=19), buttonWidth,19, &bmd->seed, 1.0, MAXFRAMEF, 100, 0, "Specify the seed for random if used.");
		} else if (md->type==eModifierType_Mirror) {
			MirrorModifierData *mmd = (MirrorModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Merge Limit:", lx, (cy-=19), buttonWidth,19, &mmd->tolerance, 0.0, 1, 0, 0, "Distance from axis within which mirrored vertices are merged");
			uiDefButI(block, ROW, B_MODIFIER_RECALC, "X",	lx, (cy-=19), 20,19, &mmd->axis, 1, 0, 0, 0, "Specify the axis to mirror about");
			uiDefButI(block, ROW, B_MODIFIER_RECALC, "Y",	lx+20, cy, 20,19, &mmd->axis, 1, 1, 0, 0, "Specify the axis to mirror about");
			uiDefButI(block, ROW, B_MODIFIER_RECALC, "Z",	lx+40, cy, 20,19, &mmd->axis, 1, 2, 0, 0, "Specify the axis to mirror about");
		} else if (md->type==eModifierType_Decimate) {
			DecimateModifierData *dmd = (DecimateModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Percent:",	lx,(cy-=19),buttonWidth,19, &dmd->percent, 0.0, 1.0, 0, 0, "Defines the percentage of triangles to reduce to");
			sprintf(str, "Face Count: %d", dmd->faceCount);
			uiDefBut(block, LABEL, 1, str,	lx, (cy-=19), 160,19, NULL, 0.0, 0.0, 0, 0, "Displays the current number of faces in the decimated mesh");
		} else if (md->type==eModifierType_Wave) {
			WaveModifierData *wmd = (WaveModifierData*) md;
			uiDefButBitS(block, TOG, WAV_X, B_MODIFIER_RECALC, "X",		lx,(cy-=19),45,19, &wmd->flag, 0, 0, 0, 0, "Enable X axis motion");
			uiDefButBitS(block, TOG, WAV_Y, B_MODIFIER_RECALC, "Y",		lx+45,cy,45,19, &wmd->flag, 0, 0, 0, 0, "Enable Y axis motion");
			uiDefButBitS(block, TOG, WAV_CYCL, B_MODIFIER_RECALC, "Cycl",	lx+90,cy,buttonWidth-90,19, &wmd->flag, 0, 0, 0, 0, "Enable cyclic wave effect");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Time sta:",	lx,(cy-=19),buttonWidth,19, &wmd->timeoffs, -1000.0, 1000.0, 100, 0, "Specify startingframe of the wave");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Lifetime:",	lx,(cy-=19),buttonWidth,19, &wmd->lifetime,  -1000.0, 1000.0, 100, 0, "Specify the lifespan of the wave");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Damptime:",	lx,(cy-=19),buttonWidth,19, &wmd->damp,  -1000.0, 1000.0, 100, 0, "Specify the dampingtime of the wave");
			cy -= 19;
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Sta x:",		lx,(cy-=19),113,19, &wmd->startx, -100.0, 100.0, 100, 0, "Starting position for the X axis");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Sta y:",		lx+115,cy,105,19, &wmd->starty, -100.0, 100.0, 100, 0, "Starting position for the Y axis");
			uiBlockBeginAlign(block);
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Speed:",	lx,(cy-=19),220,19, &wmd->speed, -2.0, 2.0, 0, 0, "Specify the wave speed");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Heigth:",	lx,(cy-=19),220,19, &wmd->height, -2.0, 2.0, 0, 0, "Specify the amplitude of the wave");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Width:",	lx,(cy-=19),220,19, &wmd->width, 0.0, 5.0, 0, 0, "Specify the width of the wave");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Narrow:",	lx,(cy-=19),220,19, &wmd->narrow, 0.0, 10.0, 0, 0, "Specify how narrow the wave follows");
		} else if (md->type==eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData*) md;
			uiDefIDPoinBut(block, modifier_testArmatureObj, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &amd->object, "Armature object to deform with");
		} else if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Falloff: ",		lx, (cy-=19), buttonWidth,19, &hmd->falloff, 0.0, 100.0, 100, 0, "If not zero, the distance from hook where influence ends");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Force: ",		lx, (cy-=19), buttonWidth,19, &hmd->force, 0.0, 1.0, 100, 0, "Set relative force of hook");
			uiDefIDPoinBut(block, test_obpoin_but, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &hmd->object, "Parent Object for hook, also recalculates and clears offset"); 
			if(hmd->indexar==NULL)
				uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",		lx, (cy-=19), buttonWidth,19, &hmd->name, 0.0, 31.0, 0, 0, "Vertex Group name");
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
			uiDefBut(block, LABEL, 1, "See Softbody panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_Boolean) {
			BooleanModifierData *bmd = (BooleanModifierData*) md;
			uiDefButI(block, MENU, B_MODIFIER_RECALC, "Operation%t|Intersect%x0|Union%x1|Difference%x2",	lx,(cy-=19),buttonWidth,19, &bmd->operation, 0.0, 1.0, 0, 0, "Boolean operation to perform");
			uiDefIDPoinBut(block, modifier_testMeshObj, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &bmd->object, "Mesh object to use for boolean operation");
		}
		uiBlockEndAlign(block);

		y-=height;
	}

	if (md->error) {
		char str[512];

		y -= 20;

		uiBlockSetCol(block, color);
					/* roundbox 4 free variables: corner-rounding, nop, roundbox type, shade */
		uiDefBut(block, ROUNDBOX, 0, "", x-10, y, width, 20, NULL, 5.0, 0.0, 15, 40, ""); 
		uiBlockSetCol(block, TH_AUTO);

		sprintf(str, "Modifier Error: %s", md->error);
		uiDefBut(block, LABEL, B_NOP, str, x+15, y+15, width-35, 19, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
	}

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

	uiNewPanelHeight(block, 204);

	uiDefBlockBut(block, modifiers_add_menu, ob, "Add Modifier", 0, 190, 130, 20, "Add a new modifier");

	sprintf(str, "To: %s", ob->id.name+2);
	uiDefBut(block, LABEL, 1, str,	140, 190, 150, 20, NULL, 0.0, 0.0, 0, 0, "Object whose modifier stack is being edited");

	xco = 0;
	yco = 160;

	md = modifiers_getVirtualModifierList(ob);

	for (i=0; md; i++, md=md->next) {
		draw_modifier(block, ob, md, &xco, &yco, i, cageIndex, lastCageIndex);
		if (md->mode&eModifierMode_Virtual) i--;
	}
	
	if(yco < 0) uiNewPanelHeight(block, 204-yco);
}

static char *make_key_menu(Key *key)
{
	KeyBlock *kb;
	int index= 1;
	char *str, item[64];

	for (kb = key->block.first; kb; kb=kb->next, index++);
	str= MEM_mallocN(index*40, "key string");
	str[0]= 0;
	
	index= 1;
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
	
	uiDefBut(block, BUT, B_ADDKEY, "Add Shape Key" ,	10, 180, 150, 20, NULL, 0.0, 0.0, 0, 0, "Add new Shape Key");
		
	key= ob_get_key(ob);
	if(key==NULL) 
		return;
	
	uiDefButS(block, TOG, B_RELKEY, "Relative",		170, 180,140,20, &key->type, 0, 0, 0, 0, "Makes Shape Keys relative");

	kb= BLI_findlink(&key->block, ob->shapenr-1);
	if(kb==NULL) {
		ob->shapenr= 1;
		kb= key->block.first;
	}

	uiBlockBeginAlign(block);
	if(ob->shapeflag & OB_SHAPE_LOCK) icon= ICON_PIN_HLT; else icon= ICON_PIN_DEHLT;
	uiDefIconButBitC(block, TOG, OB_SHAPE_LOCK, B_LOCKKEY, icon, 10,150,25,20, &ob->shapeflag, 0, 0, 0, 0, "Always show the current Shape for this Object");
	uiSetButLock(G.obedit==ob, "Unable to perform in EditMode");
	uiDefIconBut(block, BUT, B_PREVKEY, ICON_TRIA_LEFT,		35,150,20,20, NULL, 0, 0, 0, 0, "Previous Shape Key");
	strp= make_key_menu(key);
	uiDefButC(block, MENU, B_SETKEY, strp,					55,150,20,20, &ob->shapenr, 0, 0, 0, 0, "Browses existing choices or adds NEW");
	MEM_freeN(strp);
	uiDefIconBut(block, BUT, B_NEXTKEY, ICON_TRIA_RIGHT,	75,150,20,20, NULL, 0, 0, 0, 0, "Next Shape Key");
	uiClearButLock();
	uiDefBut(block, TEX, B_NAMEKEY, "",						95, 150, 190, 20, kb->name, 0.0, 31.0, 0, 0, "Current Shape Key name");
	uiDefIconBut(block, BUT, B_DELKEY, ICON_X,				285,150,25,20, 0, 0, 0, 0, 0, "Deletes current Shape Key");
	uiBlockEndAlign(block);

	if(key->type && (ob->shapeflag & OB_SHAPE_LOCK)==0 && ob->shapenr!=1) {
		uiBlockBeginAlign(block);
		make_rvk_slider(block, key, ob->shapenr-1,			10, 120, 150, 20, "Key value, when used it inserts an animation curve point");
		uiDefButF(block, NUM, B_REDR, "Min ",				160,120, 75, 20, &kb->slidermin, -10.0, 10.0, 100, 1, "Minumum for slider");
		uiDefButF(block, NUM, B_REDR, "Max ",				235,120, 75, 20, &kb->slidermax, -10.0, 10.0, 100, 1, "Maximum for slider");
		uiBlockEndAlign(block);
	}
	if(key->type && ob->shapenr!=1)
		uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",	10, 90, 150,19, &kb->vgroup, 0.0, 31.0, 0, 0, "Vertex Weight Group name, to blend with Basis Shape");
	
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
	if (file > 0) paste_unicodeText(file); 
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
	uiDefButS(block, ROW,B_MAKEFONT, "Left",		480,135,47,20, &cu->spacemode, 0.0,0.0, 0, 0, "Left align the text from the object centre");
	uiDefButS(block, ROW,B_MAKEFONT, "Center",		527,135,47,20, &cu->spacemode, 0.0,1.0, 0, 0, "Middle align the text from the object centre");
	uiDefButS(block, ROW,B_MAKEFONT, "Right",		574,135,47,20, &cu->spacemode, 0.0,2.0, 0, 0, "Right align the text from the object centre");
	uiDefButS(block, ROW,B_MAKEFONT, "Justify",		621,135,47,20, &cu->spacemode, 0.0,3.0, 0, 0, "Fill completed lines to maximum textframe width by expanding whitespace");
	uiDefButS(block, ROW,B_MAKEFONT, "Flush",		668,135,47,20, &cu->spacemode, 0.0,4.0, 0, 0, "Fill every line to maximum textframe width, distributing space among all characters");	
	uiDefBut(block, BUT, B_TOUPPER, "ToUpper",		715,135,78,20, 0, 0, 0, 0, 0, "Toggle between upper and lower case in editmode");
	uiBlockEndAlign(block);
	uiDefButBitS(block, TOG, CU_FAST, B_FASTFONT, "Fast Edit",		715,105,78,20, &cu->flag, 0, 0, 0, 0, "Don't fill polygons while editing");	

	uiDefIDPoinBut(block, test_obpoin_but, B_TEXTONCURVE, "TextOnCurve:",	480,105,220,19, &cu->textoncurve, "Apply a deforming curve to the text");
	uiDefBut(block, TEX,REDRAWVIEW3D, "Ob Family:",	480,84,220,19, cu->family, 0.0, 20.0, 0, 0, "Blender uses font from selfmade objects");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,B_MAKEFONT, "Size:",		480,56,155,20, &cu->fsize, 0.1,10.0, 10, 0, "Size of the text");
	uiDefButF(block, NUM,B_MAKEFONT, "Linedist:",	640,56,155,20, &cu->linedist, 0.0,10.0, 10, 0, "Distance between text lines");
	uiDefButF(block, NUM,B_MAKEFONT, "Word spacing:",	795,56,155,20, &cu->wordspace, 0.0,10.0, 10, 0, "Distance factor between words");		
	uiDefButF(block, NUM,B_MAKEFONT, "Spacing:",	480,34,155,20, &cu->spacing, 0.0,10.0, 10, 0, "Spacing of individual characters");
	uiDefButF(block, NUM,B_MAKEFONT, "X offset:",	640,34,155,20, &cu->xof, -50.0,50.0, 10, 0, "Horizontal position from object centre");
	uiDefButF(block, NUM,B_MAKEFONT, "UL position:",	795,34,155,20, &cu->ulpos, -0.2,0.8, 10, 0, "Vertical position of underline");			
	uiDefButF(block, NUM,B_MAKEFONT, "Shear:",		480,12,155,20, &cu->shear, -1.0,1.0, 10, 0, "Italic angle of the characters");
	uiDefButF(block, NUM,B_MAKEFONT, "Y offset:",	640,12,155,20, &cu->yof, -50.0,50.0, 10, 0, "Vertical position from object centre");
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
	case B_SUBSURFTYPE:
		/* fallthrough */
	case B_MAKEDISP:
		if(G.vd) {
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
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
	uiDefBut(block, BUT,B_HIDE,		"Hide",			400,120,150,18, 0, 0, 0, 0, 0, "Hides selected faces");
	uiDefBut(block, BUT,B_REVEAL,	"Reveal",		400,100,150,18, 0, 0, 0, 0, 0, "Reveals selected faces");
	uiDefBut(block, BUT,B_SELSWAP,	"Select Swap",	400,80,150,18, 0, 0, 0, 0, 0, "Selects unselected faces, and deselects selected faces");
	uiBlockEndAlign(block);

	uiDefButF(block, NUM,	REDRAWVIEW3D, "NSize:",	400, 40, 150, 19, &G.scene->editbutsize, 0.001, 1.0, 10, 0, "Normal size for drawing");
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
	uiDefBut(block, BUT,B_DOCENTRE, "Centre",					600, 115, 55, 19, 0, 0, 0, 0, 0, "Shifts object data to be centered about object's origin");
	uiDefBut(block, BUT,B_DOCENTRENEW, "Centre New",			655, 115, 95, 19, 0, 0, 0, 0, 0, "Shifts object's origin to center of object data");
	uiDefBut(block, BUT,B_DOCENTRECURSOR, "Centre Cursor",		600, 95, 150, 19, 0, 0, 0, 0, 0, "Shifts object's origin to cursor location");
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
			uiDefButS(block, NUM, B_RECALCPATH, "PathLen:",			600,50,150,19, &cu->pathlen, 1.0, MAXFRAMEF, 0, 0, "If no speed Ipo was set, the amount of frames of the path");
			uiDefButBitS(block, TOG, CU_PATH, B_RECALCPATH, "CurvePath",	600,30,75,19 , &cu->flag, 0, 0, 0, 0, "Enables curve to become translation path");
			uiDefButBitS(block, TOG, CU_FOLLOW, REDRAWVIEW3D, "CurveFollow",675,30,75,19, &cu->flag, 0, 0, 0, 0, "Makes curve path children to rotate along path");
			uiDefButBitS(block, TOG, CU_STRETCH, B_CURVECHECK, "CurveStretch", 600,10,150,19, &cu->flag, 0, 0, 0, 0, "Option for curve-deform: makes deformed child to stretch along entire path");
			uiDefButBitS(block, TOG, CU_OFFS_PATHDIST, REDRAWVIEW3D, "PathDist Offs", 600,-10,150,19, &cu->flag, 0, 0, 0, 0, "Children will use TimeOffs value as path distance offset");

			uiBlockEndAlign(block);
		}

		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_MAKEDISP, "DefResolU:",	760,160,120,19, &cu->resolu, 1.0, 1024.0, 0, 0, "Default resolution");
		uiDefBut(block, BUT, B_SETRESOLU, "Set",		880,160,30,19, 0, 0, 0, 0, 0, "Set resolution for interpolation");

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_MAKEDISP, "Width:",		760,90,150,19, &cu->width, 0.0, 2.0, 1, 0, "Make interpolated result thinner or fatter");
		uiDefButF(block, NUM, B_MAKEDISP, "Extrude:",		760,70,150,19, &cu->ext1, 0.0, 5.0, 10, 0, "Curve extrusion size when not using a bevel object");
		uiDefButF(block, NUM, B_MAKEDISP, "Bevel Depth:",		760,50,150,19, &cu->ext2, 0.0, 2.0, 1, 0, "Bevel depth when not using a bevel object");
		uiDefButS(block, NUM, B_MAKEDISP, "BevResol:",	760,30,150,19, &cu->bevresol, 0.0, 10.0, 0, 0, "Bevel resolution when depth is non-zero and not using a bevel object");
		uiDefIDPoinBut(block, test_obcurpoin_but, B_CHANGEDEP, "BevOb:",		760,10,150,19, &cu->bevobj, "Curve object name that defines the bevel shape");
		uiDefIDPoinBut(block, test_obcurpoin_but, B_CHANGEDEP, "TaperOb:",		760,-10,150,19, &cu->taperobj, "Curve object name that defines the taper (width)");

		uiBlockBeginAlign(block);
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButBitS(block, TOG, CU_BACK, B_MAKEDISP, "Back",	760,130,50,19, &cu->flag, 0, 0, 0, 0, "Draw filled back for curves");
		uiDefButBitS(block, TOG, CU_FRONT, B_MAKEDISP, "Front",810,130,50,19, &cu->flag, 0, 0, 0, 0, "Draw filled front for curves");
		uiDefButBitS(block, TOG, CU_3D, B_CU3D, "3D",		860,130,50,19, &cu->flag, 0, 0, 0, 0, "Allow Curve Object to be 3d, it doesn't fill then");
	}
}


/* *************************** CAMERA ******************************** */


static void editing_panel_camera_type(Object *ob, Camera *cam)
{
	uiBlock *block;
	float grid=0.0;

	if(G.vd) grid= G.vd->grid;
	if(grid<1.0) grid= 1.0;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_camera_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Camera", "Editing", 320, 0, 318, 204)==0) return;

	if(cam->type==CAM_ORTHO)
		uiDefButF(block, NUM,REDRAWVIEW3D, "Scale:", 470,178,160,20, &cam->ortho_scale, 0.01, 1000.0, 50, 0, "Specify the ortho scaling of the used camera");
	else
		uiDefButF(block, NUM,REDRAWVIEW3D, "Lens:", 470,178,160,20, &cam->lens, 1.0, 250.0, 100, 0, "Specify the lens of the camera");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,REDRAWVIEW3D, "ClipSta:", 470,147,160,20, &cam->clipsta, 0.001*grid, 100.0*grid, 10, 0, "Specify the startvalue of the the field of view");
	uiDefButF(block, NUM,REDRAWVIEW3D, "ClipEnd:", 470,125,160,20, &cam->clipend, 1.0, 5000.0*grid, 100, 0, "Specify the endvalue of the the field of view");
	uiBlockEndAlign(block);

	uiDefButF(block, NUM,REDRAWVIEW3D, "DrawSize:", 470,90,160,20, &cam->drawsize, 0.1*grid, 10.0, 10, 0, "Specify the drawsize of the camera");

	uiDefButS(block, TOG, REDRAWVIEW3D, "Ortho",		470,29,61,60, &cam->type, 0, 0, 0, 0, "Render orthogonally");
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, CAM_SHOWLIMITS, REDRAWVIEW3D, "ShowLimits", 533,59,97,30, &cam->flag, 0, 0, 0, 0, "Draw the field of view");
	uiDefButBitS(block, TOG, CAM_SHOWMIST, REDRAWVIEW3D, "Show Mist",  533,29,97,30, &cam->flag, 0, 0, 0, 0, "Draw a line that indicates the mist area");
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

		uiDefButS(block, ROW, B_RECALCMBALL, "Ball", 753,83,60,19, &lastelem->type, 1.0, 0.0, 0, 0, "Draw active meta as Ball");
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, B_RECALCMBALL, "Tube", 753,62,60,19, &lastelem->type, 1.0, 4.0, 0, 0, "Draw active meta as Ball");
		uiDefButS(block, ROW, B_RECALCMBALL, "Plane", 814,62,60,19, &lastelem->type, 1.0, 5.0, 0, 0, "Draw active meta as Plane");
		uiDefButS(block, ROW, B_RECALCMBALL, "Elipsoid", 876,62,60,19, &lastelem->type, 1.0, 6.0, 0, 0, "Draw active meta as Ellipsoid");
		uiDefButS(block, ROW, B_RECALCMBALL, "Cube", 938,62,60,19, &lastelem->type, 1.0, 7.0, 0, 0, "Draw active meta as Cube");
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
			ob->softflag |= OB_SB_REDO;
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
	case B_RESIZELAT:
		if(ob) {
			lt = ob->data;
			resizelattice(ob->data, lt->opntsu, lt->opntsv, lt->opntsw, ob);
			ob->softflag |= OB_SB_REDO;
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
	switch(event) {
	case B_ARM_RECALCDATA:
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 1);
		allqueue(REDRAWBUTSEDIT, 0);
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
	
	if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
		/* Attach this bone to its parent */
		VECCOPY(ebone->head, ebone->parent->tail);
	}
}

static void parnr_to_editbone(EditBone *bone)
{
	if (bone->parNr == -1){
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

static void build_bonestring (char *string, EditBone *bone)
{
	EditBone *curBone;
	EditBone *pBone;
	int		skip=0;
	int		index, numbones, i;
	char (*qsort_ptr)[32] = NULL;

	sprintf (string, "Parent%%t| %%x%d", -1);	/* That space is there
												 * for a reason
												 */

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

			if (skip)
				continue;

			sprintf (qsort_ptr[numbones], "|%s%%x%d", curBone->name, index);
			numbones++;
		}
	}
	qsort (qsort_ptr, numbones, sizeof (qsort_ptr[0]),
		   ( int (*)(const void *, const void *) ) strcmp);

	for (i=0; i < numbones; ++i) {
		sprintf (string, "%s%s", string, qsort_ptr[i]);
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

static void editing_panel_armature_type(Object *ob, bArmature *arm)
{
	uiBlock		*block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_armature_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Armature", "Editing", 320, 0, 318, 204)==0) return;

	uiDefBut(block, LABEL, 0, "Editing Options", 10,180,150,20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, ARM_MIRROR_EDIT, B_DIFF, "X-Axis Mirror Edit", 10, 160,150,20, &arm->flag, 0, 0, 0, 0, "Enable X-axis mirrored editing");
	uiDefButBitC(block, TOG, OB_DRAWXRAY,REDRAWVIEW3D, "X-Ray",				160,160,150,20, &ob->dtx, 0, 0, 0, 0, "Draw armature in front of solid objects");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Display Options", 10,140,150,20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButI(block, ROW, REDRAWVIEW3D, "Octahedron", 10, 120,90,20, &arm->drawtype, 0, ARM_OCTA, 0, 0, "Draw bones as octahedra");
	uiDefButI(block, ROW, REDRAWVIEW3D, "Stick",	100, 120,55,20, &arm->drawtype, 0, ARM_LINE, 0, 0, "Draw bones as simple 2d lines with dots");
	uiDefButI(block, ROW, REDRAWVIEW3D, "B-Bone",	155, 120,70,20, &arm->drawtype, 0, ARM_B_BONE, 0, 0, "Draw bones as boxes, showing subdivision and b-splines");
	uiDefButI(block, ROW, REDRAWVIEW3D, "Envelope",	225, 120,85,20, &arm->drawtype, 0, ARM_ENVELOPE, 0, 0, "Draw bones as extruded spheres, showing deformation influence volume");

	uiDefButBitI(block, TOG, ARM_DRAWAXES, REDRAWVIEW3D, "Draw Axes", 10, 100,150,20, &arm->flag, 0, 0, 0, 0, "Draw bone axes");
	uiDefButBitI(block, TOG, ARM_DRAWNAMES, REDRAWVIEW3D, "Draw Names", 160,100,150,20, &arm->flag, 0, 0, 0, 0, "Draw bone names");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Deform Options", 10,80,150,20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, ARM_DEF_VGROUP, B_ARM_RECALCDATA, "Vertex Groups",	10, 60,150,20, &arm->deformflag, 0, 0, 0, 0, "Enable VertexGroups defining deform");
	uiDefButBitI(block, TOG, ARM_DEF_ENVELOPE, B_ARM_RECALCDATA, "Envelopes",	160,60,150,20, &arm->deformflag, 0, 0, 0, 0, "Enable Bone Envelopes defining deform");
	uiDefButBitI(block, TOG, ARM_RESTPOS, B_ARM_RECALCDATA,"Rest Position",		10,40,150,20, &arm->flag, 0, 0, 0, 0, "Show armature rest position, no posing possible");
	uiDefButBitI(block, TOG, ARM_DELAYDEFORM, REDRAWVIEW3D, "Delay Deform",		160,40,150,20, &arm->flag, 0, 0, 0, 0, "Don't deform children when manipulating bones in pose mode");
	
}


static void editing_panel_armature_bones(Object *ob, bArmature *arm)
{
	uiBlock		*block;
	uiBut		*but;
	EditBone	*curBone;
	char		*boneString=NULL;
	int			bx=148, by=180;
	int			index;

	/* Draw the bone name block */

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_armature_bones", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Armature Bones", "Editing", 640, 0, 318, 204)==0) return;

	/* this is a variable height panel, newpanel doesnt force new size on existing panels */
	/* so first we make it default height */
	uiNewPanelHeight(block, 204);


	uiDefBut(block, LABEL, 0, "Selected Bones", bx,by,158,18, 0, 0, 0, 0, 0, "Only show in Armature Editmode");
	by-=20;
	for (curBone=G.edbo.first, index=0; curBone; curBone=curBone->next, index++){
		if (curBone->flag & (BONE_SELECTED)) {

			/*	Bone naming button */
			but=uiDefBut(block, TEX, REDRAWVIEW3D, "BO:", bx-10,by,117,18, &curBone->name, 0, 24, 0, 0, "Change the bone name");
			uiButSetFunc(but, validate_editbonebutton_cb, curBone, NULL);

			uiDefBut(block, LABEL, 0, "child of", bx+107,by,73,18, NULL, 0.0, 0.0, 0.0, 0.0, "");

			boneString = MEM_mallocN((BLI_countlist(&G.edbo) * 64)+64, "Bone str");
			build_bonestring (boneString, curBone);

			curBone->parNr = editbone_to_parnr(curBone->parent);
			but = uiDefButI(block, MENU,REDRAWVIEW3D, boneString, bx+180,by,120,18, &curBone->parNr, 0.0, 0.0, 0.0, 0.0, "Parent");
			/* last arg NULL means button will put old string there */
			uiButSetFunc(but, parnr_to_editbone_cb, curBone, NULL);

			MEM_freeN(boneString);

			/* Connect to parent flag */
			if (curBone->parent){
				but=uiDefButBitI(block, TOG, BONE_CONNECTED, B_ARM_RECALCDATA, "Con", bx+300,by,32,18, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "Connect this Bone to Parent");
				uiButSetFunc(but, attach_bone_to_parent_cb, curBone, NULL);
			}

			/* Segment, dist and weight buttons */
			uiBlockBeginAlign(block);
			uiDefButS(block, NUM, B_ARM_RECALCDATA, "Segm: ", bx-10,by-19,117,18, &curBone->segments, 1.0, 32.0, 0.0, 0.0, "Subdivisions for B-bones");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "Dist:", bx+110, by-19, 105, 18, &curBone->dist, 0.0, 1000.0, 10.0, 0.0, "Bone deformation distance");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "Weight:", bx+223, by-19,110, 18, &curBone->weight, 0.0F, 1000.0F, 10.0F, 0.0F, "Bone deformation weight");

			/* bone types */
			uiDefButBitI(block, TOG, BONE_HINGE, B_ARM_RECALCDATA, "Hinge",		bx-10,by-38,85,18, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Don't inherit rotation or scale from parent Bone");
			uiDefButBitI(block, TOGN, BONE_NO_DEFORM, B_ARM_RECALCDATA, "Deform",	bx+75, by-38, 85, 18, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "Indicate if Bone deforms geometry");
			uiDefButBitI(block, TOG, BONE_MULT_VG_ENV, B_ARM_RECALCDATA, "Mult", bx+160,by-38,85,18, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Multiply Bone Envelope with VertexGroup");
			uiDefButBitI(block, TOG, BONE_HIDDEN_A, REDRAWVIEW3D, "Hide",	bx+245,by-38,88,18, &curBone->flag, 0, 0, 0, 0, "Toggles display of this bone in Edit Mode");
			
			uiBlockEndAlign(block);
			by-=60;
			
			if(by < -200) break;	// for time being... extreme long panels are very slow
		}
	}

	if(by<0) {
		uiNewPanelHeight(block, 204 - by);
	}

}

static void editing_panel_pose_bones(Object *ob, bArmature *arm)
{
	uiBlock		*block;
	uiBut		*but;
	bPoseChannel *pchan;
	Bone		*curBone;
	int			bx=148, by=180;
	int			index, zerodof, zerolimit;
	
	/* Draw the bone name block */
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_pose_bones", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Armature Bones", "Editing", 640, 0, 318, 204)==0) return;
	
	/* this is a variable height panel, newpanel doesnt force new size on existing panels */
	/* so first we make it default height */
	uiNewPanelHeight(block, 204);
	
	uiDefBut(block, LABEL, 0, "Selected Bones", bx,by,158,18, 0, 0, 0, 0, 0, "Only show in Armature Editmode/Posemode");
	by-=20;
	for (pchan=ob->pose->chanbase.first, index=0; pchan; pchan=pchan->next, index++){
		curBone= pchan->bone;
		if (curBone->flag & (BONE_SELECTED)) {

			/*	Bone naming button */
			uiBlockBeginAlign(block);
			but=uiDefBut(block, TEX, REDRAWVIEW3D, "BO:", bx-10,by,117,18, &curBone->name, 0, 24, 0, 0, "Change the bone name");
			uiButSetFunc(but, validate_posebonebutton_cb, curBone, NULL);
			
			/* Dist and weight buttons */
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "Dist:", bx+107, by, 105, 18, &curBone->dist, 0.0, 1000.0, 10.0, 0.0, "Bone deformation distance");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "Weight:", bx+220, by,  110, 18, &curBone->weight, 0.0F, 1000.0F, 10.0F, 0.0F, "Bone deformation weight");
			
			
			/* Segment, ease in/out buttons */
			uiBlockBeginAlign(block);
			uiDefButS(block, NUM, B_ARM_RECALCDATA, "Segm: ",  bx-10,by-19,117,19, &curBone->segments, 1.0, 32.0, 0.0, 0.0, "Subdivisions for B-bones");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "In:",  bx+107, by-19,105, 19, &curBone->ease1, 0.0, 2.0, 10.0, 0.0, "First length of Bezier handle");
			uiDefButF(block, NUM,B_ARM_RECALCDATA, "Out:",  bx+220, by-19, 110, 19, &curBone->ease2, 0.0, 2.0, 10.0, 0.0, "Second length of Bezier handle");
			
			/* bone types */
			uiDefButBitI(block, TOG, BONE_HINGE, B_ARM_RECALCDATA, "Hinge", bx-10,by-38,85,18, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Don't inherit rotation or scale from parent Bone");
			uiDefButBitI(block, TOGN, BONE_NO_DEFORM, B_ARM_RECALCDATA, "Deform",	bx+75, by-38, 85, 18, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "Indicate if Bone deforms geometry");
			uiDefButBitI(block, TOG, BONE_MULT_VG_ENV, B_ARM_RECALCDATA, "Mult", bx+160,by-38,85,18, &curBone->flag, 1.0, 32.0, 0.0, 0.0, "Multiply Bone Envelope with VertexGroup");
			uiDefButBitI(block, TOG, BONE_HIDDEN_P, REDRAWVIEW3D, "Hide",	bx+245,by-38,88,18, &curBone->flag, 0, 0, 0, 0, "Toggles display of this bone in Pose Mode");
			uiBlockEndAlign(block);
			
			/* DOFs only for IK chains */
			zerodof = 1;
			zerolimit = 1;
			if(pose_channel_in_IK_chain(ob, pchan)) {
				
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, BONE_IK_NO_XDOF, B_ARM_RECALCDATA, "Lock X Rot", bx-10,by-60,114,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Disable X DoF for IK");
				if ((pchan->ikflag & BONE_IK_NO_XDOF)==0) {
					uiDefButF(block, NUM, B_ARM_RECALCDATA, "Stiff X:", bx-10, by-80, 114, 19, &pchan->stiffness[0], 0.0, 0.99, 1.0, 0.0, "Resistance to bending for X axis");
					uiDefButBitS(block, TOG, BONE_IK_XLIMIT, B_ARM_RECALCDATA, "Limit X", bx-10,by-100,114,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Limit rotation over X axis");
					if ((pchan->ikflag & BONE_IK_XLIMIT)) {
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Min X:", bx-10, by-120, 114, 19, &pchan->limitmin[0], -180.0, 0.0, 1000, 1, "Minimum X limit");
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Max X:", bx-10, by-140, 114, 19, &pchan->limitmax[0], 0.0, 180.0f, 1000, 1, "Maximum X limit");
						zerolimit = 0;
					}
					zerodof = 0;
				}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, BONE_IK_NO_YDOF, B_ARM_RECALCDATA, "Lock Y Rot", bx+104,by-60,113,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Disable Y DoF for IK");
				if ((pchan->ikflag & BONE_IK_NO_YDOF)==0) {
					uiDefButF(block, NUM, B_ARM_RECALCDATA, "Stiff Y:", bx+104, by-80, 114, 19, &pchan->stiffness[1], 0.0, 0.99, 1.0, 0.0, "Resistance to twisting over Y axis");
					uiDefButBitS(block, TOG, BONE_IK_YLIMIT, B_ARM_RECALCDATA, "Limit Y", bx+104,by-100,113,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Limit rotation over Y axis");
					if ((pchan->ikflag & BONE_IK_YLIMIT)) {
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Min Y:", bx+104, by-120, 113, 19, &pchan->limitmin[1], -180.0, 0.0, 1000, 1, "Minimum Y limit");
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Max Y:", bx+104, by-140, 113, 19, &pchan->limitmax[1], 0.0, 180.0, 1000, 1, "Maximum Y limit");
						zerolimit = 0;
					}
					zerodof = 0;
				}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, BONE_IK_NO_ZDOF, B_ARM_RECALCDATA, "Lock Z Rot", bx+217,by-60,113,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Disable Z DoF for IK");
				if ((pchan->ikflag & BONE_IK_NO_ZDOF)==0) {
					uiDefButF(block, NUM, B_ARM_RECALCDATA, "Stiff Z:", bx+217, by-80, 114, 19, &pchan->stiffness[2], 0.0, 0.99, 1.0, 0.0, "Resistance to bending for Z axis");
					uiDefButBitS(block, TOG, BONE_IK_ZLIMIT, B_ARM_RECALCDATA, "Limit Z", bx+217,by-100,113,19, &pchan->ikflag, 0.0, 0.0, 0.0, 0.0, "Limit rotation over Z axis");
					if ((pchan->ikflag & BONE_IK_ZLIMIT)) {
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Min Z:", bx+217, by-120, 113, 19, &pchan->limitmin[2], -180.0, 0.0, 1000, 1, "Minimum Z limit");
						uiDefButF(block, NUM, B_ARM_RECALCDATA, "Max Z:", bx+217, by-140, 113, 19, &pchan->limitmax[2], 0.0, 180.0, 1000, 1, "Maximum Z limit");
						zerolimit = 0;
					}
					zerodof = 0;
				}
				uiBlockEndAlign(block);
				
				by -= (zerodof)? 82: (zerolimit)? 122: 162;

				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, B_ARM_RECALCDATA, "Stretch:", bx-10, by, 113, 19, &pchan->ikstretch, 0.0, 1.0, 1.0, 0.0, "Allow scaling of the bone for IK");
				uiBlockEndAlign(block);

				by -= 20;
			}
			else {
				uiDefBut(block, LABEL, 0, "(DoF options only for IK chains)", bx-10,by-60, 300, 20, 0, 0, 0, 0, 0, "");

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
			if(base->object!=ob) {
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
		case B_NEWVGROUP:
			add_defgroup (G.obedit);
			scrarea_queue_winredraw(curarea);
			allqueue(REDRAWOOPS, 0);
			break;
		case B_DELVGROUP:
			del_defgroup (G.obedit);
			allqueue (REDRAWVIEW3D, 1);
			allqueue(REDRAWOOPS, 0);
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
			break;
		case B_DESELVGROUP:
			sel_verts_defgroup(0);
			allqueue (REDRAWVIEW3D, 1);
			allqueue(REDRAWOOPS, 0);
			break;
		case B_LINKEDVGROUP:
			copy_linked_vgroup_channels(ob);
			break;
		case B_DELSTICKY:

			if(me->msticky) MEM_freeN(me->msticky);
			me->msticky= NULL;
			allqueue(REDRAWBUTSEDIT, 0);
			break;
		case B_MAKESTICKY:
			RE_make_sticky();
			allqueue(REDRAWBUTSEDIT, 0);
			break;
		
		case B_MAKEVERTCOL:
			make_vertexcol();
			break;
		case B_DELVERTCOL:
			if(me->mcol) MEM_freeN(me->mcol);
			me->mcol= NULL;
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
			allqueue(REDRAWVIEW3D, 0);
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
		notice("Removed: %d", removedoublesflag(1, G.scene->toolsettings->doublimit));
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Rem Doubles");
		break;
	case B_SUBDIV:
		waitcursor(1);
		esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag & B_BEAUTY,1,0);
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
		esubdivideflag(1, fac, G.scene->toolsettings->editbutflag & B_BEAUTY,1,0);
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
	}
	/* WATCH IT: previous events only in editmode! */
}

static void editing_panel_mesh_tools(Object *ob, Mesh *me)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_tools", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Mesh Tools", "Editing", 640, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	//uiDefButBitS(block, TOG, B_AUTOFGON, 0, "FGon",		    10,195,30,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Causes 'Subdivide' To create FGon on inner edges where possible");
	uiDefButBitS(block, TOG, B_BEAUTY, 0, "Beauty",		    10,195,40,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Causes 'Subdivide' to split faces in halves instead of quarters using Long Edges Unless short is selected");
	uiDefButBitS(block, TOG, B_BEAUTY_SHORT, 0, "Short",		    50,195,40,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Causes 'Subdivide' to split faces in halves instead of quarters using Short Edges");

	uiDefBut(block, BUT,B_SUBDIV,"Subdivide",		90,195,80,19, 0, 0, 0, 0, 0, "Splits selected faces into halves or quarters");

	uiDefButS(block, MENU, B_DIFF, "Corner Cut Type %t|Path %x0|Innervert %x1|Fan %x2", 
												170, 195, 85, 19, &G.scene->toolsettings->cornertype , 0, 0, 0, 0, "Choose Quad Corner Cut Type");	

	uiDefBut(block, BUT,B_VERTEXNOISE,"Noise",		10,175,60,19, 0, 0, 0, 0, 0, "Use vertex coordinate as texture coordinate");
	uiDefBut(block, BUT,B_HASH,"Hash",				70,175,60,19, 0, 0, 0, 0, 0, "Randomizes selected vertice sequence data");
	uiDefBut(block, BUT,B_XSORT,"Xsort",			130,175,60,19, 0, 0, 0, 0, 0, "Sorts selected vertice data in the X direction");
	uiDefBut(block, BUT,B_FRACSUBDIV, "Fractal",	190,175,65,19, 0, 0, 0, 0, 0, "Subdivides selected faces with a random factor");


	uiDefBut(block, BUT,B_TOSPHERE,"To Sphere",		10,155,80,19, 0, 0, 0, 0, 0, "Moves selected vertices outwards into a spherical shape");
	uiDefBut(block, BUT,B_VERTEXSMOOTH,"Smooth",	90,155,80,19, 0, 0, 0, 0, 0, "Flattens angles of selected faces");
	uiDefBut(block, BUT,B_SPLIT,"Split",			170,155,85,19, 0, 0, 0, 0, 0, "Splits selected verts to separate sub-mesh.");

	uiDefBut(block, BUT,B_FLIPNORM,"Flip Normals",	10,135,80,19, 0, 0, 0, 0, 0, "Toggles the direction of the selected face's normals");
	uiDefBut(block, BUT,B_REMDOUB,"Rem Doubles",	90,135,80,19, 0, 0, 0, 0, 0, "Removes duplicates from selected vertices");
	uiDefButF(block, NUM, B_DIFF, "Limit:",			170,135,85,19, &G.scene->toolsettings->doublimit, 0.0001, 1.0, 10, 0, "Specifies the max distance 'Rem Doubles' will consider vertices as 'doubled'");
	uiBlockEndAlign(block);

	uiDefBut(block, BUT,B_EXTR,"Extrude",			10,105,245,24, 0, 0, 0, 0, 0, "Converts selected edges to faces and selects the new vertices");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_SCREW,"Screw",			10,75,80,24, 0, 0, 0, 0, 0, "Activates the screw tool");  // Bish - This could use some more definition
	uiDefBut(block, BUT,B_SPIN, "Spin",				90,75,80,24, 0, 0, 0, 0, 0, "Extrudes the selected vertices in a circle around the cursor in the indicated viewport");
	uiDefBut(block, BUT,B_SPINDUP,"Spin Dup",		170,75,85,24, 0, 0, 0, 0, 0, "Creates copies of the selected vertices in a circle around the cursor in the indicated viewport");

	uiDefButS(block, NUM, B_DIFF, "Degr:",			10,55,80,19, &G.scene->toolsettings->degr,10.0,360.0, 0, 0, "Specifies the number of degrees 'Spin' revolves");
	uiDefButS(block, NUM, B_DIFF, "Steps:",			90,55,80,19, &G.scene->toolsettings->step,1.0,180.0, 0, 0, "Specifies the total number of 'Spin' slices");
	uiDefButS(block, NUM, B_DIFF, "Turns:",			170,55,85,19, &G.scene->toolsettings->turn,1.0,360.0, 0, 0, "Specifies the number of revolutions the screw turns");
	uiDefButBitS(block, TOG, B_KEEPORIG, B_DIFF, "Keep Original",10,35,160,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Keeps a copy of the original vertices and faces after executing tools");
	uiDefButBitS(block, TOG, B_CLOCKWISE, B_DIFF, "Clockwise",	170,35,85,19, &G.scene->toolsettings->editbutflag, 0, 0, 0, 0, "Specifies the direction for 'Screw' and 'Spin'");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_EXTREP, "Extrude Dup",	10,10,120,19, 0, 0, 0, 0, 0, "Creates copies of the selected vertices in a straight line away from the current viewport");
	uiDefButF(block, NUM, B_DIFF, "Offset:",		130,10,125,19, &G.scene->toolsettings->extr_offs, 0.01, 100.0, 100, 0, "Sets the distance between each copy for 'Extrude Dup'");
	uiBlockEndAlign(block);
}

static void verify_vertexgroup_name_func(void *datav, void *data2_unused)
{
	unique_vertexgroup_name((bDeformGroup*)datav, OBACT);
}



static void editing_panel_mesh_tools1(Object *ob, Mesh *me)
{
	uiBlock *block;


	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_tools1", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Mesh Tools 1", "Editing", 960, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_DOCENTRE, "Centre",	955, 200, 160, 19, 0, 0, 0, 0, 0, "Shifts object data to be centered about object's origin");
	uiDefBut(block, BUT,B_HIDE,		"Hide",		1115, 200,  160, 19, 0, 0, 0, 0, 0, "Hides selected faces");
	uiDefBut(block, BUT,B_SELSWAP,	"Select Swap",	955, 180, 160, 19, 0, 0, 0, 0, 0, "Selects unselected faces, and deselects selected faces");
	uiDefBut(block, BUT,B_REVEAL,	"Reveal",		1115, 180,  160, 19, 0, 0, 0, 0, 0, "Reveals selected faces");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,		  REDRAWVIEW3D, "NSize:",	955, 131, 150, 19, &G.scene->editbutsize, 0.001, 2.0, 10, 0, "Sets the length to use when displaying face normals");
	uiDefButBitI(block, TOG, G_DRAWNORMALS, REDRAWVIEW3D, "Draw Normals",	955,110,150,19, &G.f, 0, 0, 0, 0, "Displays face normals as lines");
	uiDefButBitI(block, TOG, G_DRAWFACES, REDRAWVIEW3D, "Draw Faces",		955,88,150,19, &G.f, 0, 0, 0, 0, "Displays all faces as shades");
	uiDefButBitI(block, TOG, G_DRAWEDGES, REDRAWVIEW3D, "Draw Edges", 	955,66,150,19, &G.f, 0, 0, 0, 0, "Displays selected edges using hilights");
	uiDefButBitI(block, TOG, G_DRAWCREASES, REDRAWVIEW3D, "Draw Creases",	955,44,150,19, &G.f, 0, 0, 0, 0, "Displays creases created for subsurf weighting");
	uiDefButBitI(block, TOG, G_DRAWSEAMS, REDRAWVIEW3D, "Draw Seams",	955,22,150,19, &G.f, 0, 0, 0, 0, "Displays UV unwrapping seams");
	uiDefButBitI(block, TOG, G_ALLEDGES, 0, "All Edges",			955, 0,150,19, &G.f, 0, 0, 0, 0, "Displays all edges in object mode without optimization");
	uiBlockEndAlign(block);
	
	/* Measurement drawing options */
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, G_DRAW_VNORMALS, REDRAWVIEW3D, "Draw VNormals",1125,110,150,19, &G.f, 0, 0, 0, 0, "Displays vertex normals as lines");
	uiDefButBitI(block, TOG, G_DRAW_EDGELEN, REDRAWVIEW3D, "Edge Length",	1125,88,150,19, &G.f, 0, 0, 0, 0, "Displays selected edge lengths");
	uiDefButBitI(block, TOG, G_DRAW_EDGEANG, REDRAWVIEW3D, "Edge Angles",	1125,66,150,19,  &G.f, 0, 0, 0, 0, "Displays the angles in the selected edges in degrees");
	uiDefButBitI(block, TOG, G_DRAW_FACEAREA, REDRAWVIEW3D, "Face Area",	1125,44,150,19, &G.f, 0, 0, 0, 0, "Displays the area of selected faces");
	uiBlockEndAlign(block);

}

char *get_vertexgroup_menustr(Object *ob)
{
	bDeformGroup *dg;
	int defCount, min, index;
	char (*qsort_ptr)[32] = NULL;
	char *s, *menustr;
	
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
			snprintf (qsort_ptr[index - 1], sizeof (qsort_ptr[0]),
					  "%s%%x%d|", dg->name, index);
		}
		
		qsort (qsort_ptr, defCount, sizeof (qsort_ptr[0]),
			   ( int (*)(const void *, const void *) ) strcmp);
	}
	
	s= menustr = MEM_callocN((32 * defCount)+30, "menustr");	// plus 30 for when defCount==0
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
		xco= std_libbuttons(block, 143, 180, 0, NULL, browse, id, idfrom, &(G.buts->menunr), alone, local, 0, 0, B_KEEPDATA);
		uiBlockSetCol(block, TH_AUTO);
	}
	if(ob) {
		but = uiDefBut(block, TEX, B_IDNAME, "OB:",	xco, 180, 454-xco, YIC, ob->id.name+2, 0.0, 19.0, 0, 0, "Displays Active Object name. Click to change.");
		uiButSetFunc(but, test_idbutton_cb, ob->id.name, NULL);
	}



	/* to be sure */
	if ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL);
	else return;


	if(ob->type==OB_MESH) poin= &( ((Mesh *)ob->data)->texflag );
	else if(ob->type==OB_MBALL) poin= &( ((MetaBall *)ob->data)->texflag );
	else poin= &( ((Curve *)ob->data)->texflag );
	uiDefButBitI(block, TOG, AUTOSPACE, B_AUTOTEX, "AutoTexSpace",	143,15,140,19, poin, 0, 0, 0, 0, "Adjusts active object's texture space automatically when transforming object");

	sprintf(str,"%d Mat ", ob->totcol);
	if(ob->totcol) min= 1.0; else min= 0.0;
	ma= give_current_material(ob, ob->actcol);

	if(ma) uiDefBut(block, LABEL, 0, ma->id.name+2, 318,153, 103, 20, 0, 0, 0, 0, 0, "");

	uiBlockBeginAlign(block);
	if(ma) uiDefButF(block, COL, B_REDR, "",			292,123,31,30, &(ma->r), 0, 0, 0, 0, "");
	uiDefButC(block, NUM, B_ACTCOL,	str,		324,123,100,30, &ob->actcol, min, (float)(ob->totcol), 0, 0, "Displays total number of material indices and the current index");
	uiDefBut(block, BUT,B_MATWICH,	"?",		424,123,30,30, 0, 0, 0, 0, 0, "In EditMode, sets the active material index from selected faces");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_MATNEW,	"New",		292,98,80,20, 0, 0, 0, 0, 0, "Adds a new Material index");
	uiDefBut(block, BUT,B_MATDEL,	"Delete",	374,98,80,20, 0, 0, 0, 0, 0, "Deletes this Material index");
	uiDefBut(block, BUT,B_MATSEL,	"Select",	292,76,80,20, 0, 0, 0, 0, 0, "In EditMode, selects faces that have the active index");
	uiDefBut(block, BUT,B_MATDESEL,	"Deselect",	374,76,80,20, 0, 0, 0, 0, 0, "Deselects everything with current indexnumber");
	uiDefBut(block, BUT,B_MATASS,	"Assign",	292,47,162,26, 0, 0, 0, 0, 0, "In EditMode, assigns the active index to selected faces");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_SETSMOOTH,"Set Smooth",	291,15,80,20, 0, 0, 0, 0, 0, "In EditMode, sets 'smooth' rendering of selected faces");
	uiDefBut(block, BUT,B_SETSOLID,	"Set Solid",	373,15,80,20, 0, 0, 0, 0, 0, "In EditMode, sets 'solid' rendering of selected faces");
	uiBlockEndAlign(block);

	/* vertex group... partially editmode... */
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		uiBut *but;
		int	defCount;
		bDeformGroup	*defGroup;
	
		uiDefBut(block, LABEL,0,"Vertex Groups",
				 143,153,130,20, 0, 0, 0, 0, 0, "");

		defCount=BLI_countlist(&ob->defbase);

		uiBlockBeginAlign(block);
		if (defCount) {
			char *menustr= get_vertexgroup_menustr(ob);
			
			uiDefButS(block, MENU, REDRAWBUTSEDIT, menustr, 143, 132,18,21, &ob->actdef, 1, defCount, 0, 0, "Browses available vertex groups");
			MEM_freeN (menustr);
		}
		
		if (ob->actdef){
			defGroup = BLI_findlink(&ob->defbase, ob->actdef-1);
			but= uiDefBut(block, TEX,REDRAWBUTSEDIT,"",		161,132,140-18,21, defGroup->name, 0, 32, 0, 0, "Displays current vertex group name. Click to change. (Match bone name for deformation.)");
			uiButSetFunc(but, verify_vertexgroup_name_func, defGroup, NULL);

			uiDefButF(block, NUM, REDRAWVIEW3D, "Weight:",		143, 111, 140, 21, &editbutvweight, 0, 1, 10, 0, "Sets the current vertex group's bone deformation strength");
		}
		uiBlockEndAlign(block);

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
			if(me->id.us>1)
				uiDefBut(block, BUT,B_LINKEDVGROUP, "Copy To Linked",	143,69,140,20, 0, 0, 0, 0, 0, "Creates identical vertex group names in other Objects using this Mesh");
		}
	}


}

/* *************************** FACE/PAINT *************************** */

void do_fpaintbuts(unsigned short event)
{
	Mesh *me;
	Object *ob;
	bDeformGroup *defGroup;
	extern TFace *lasttface; /* caches info on tface bookkeeping ?*/
	extern VPaint Gwp;         /* from vpaint */

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
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			do_shared_vertexcol(me);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_SET_VCOL:
		if(G.f & G_FACESELECT) 
			clear_vpaint_selectedfaces();
		else
			clear_vpaint();
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
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
		set_lasttface();
		if(lasttface) {
			lasttface->mode &= ~TF_BILLBOARD2;
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;

	case B_TFACE_BILLB:
		set_lasttface();
		if(lasttface) {
			lasttface->mode &= ~TF_BILLBOARD;
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
}


/* -------------------- MODE: vpaint  ------------------- */

static void editing_panel_mesh_paint(void)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_paint", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Paint", "Editing", 640, 0, 318, 204)==0) return;
	
	
	if(G.f & G_WEIGHTPAINT) {
		extern VPaint Gwp;         /* from vpaint */
		Object *ob;
 	    ob= OBACT;
		
		if(ob==NULL) return;

		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, REDRAWVIEW3D, "Weight:",10,160,225,19, &editbutvweight, 0, 1, 10, 0, "Sets the current vertex group's bone deformation strength");
		
		uiDefBut(block, BUT, B_WEIGHT0_0 , "0",			 10,140,45,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_WEIGHT1_4 , "1/4",		 55,140,45,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_WEIGHT1_2 , "1/2",		 100,140,45,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_WEIGHT3_4 , "3/4",		 145,140,45,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_WEIGHT1_0 , "1",			 190,140,45,19, 0, 0, 0, 0, 0, "");
		
		uiDefButF(block, NUMSLI, B_NOP, "Opacity ",		10,120,225,19, &Gwp.a, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
		
		uiDefBut(block, BUT, B_OPA1_8 , "1/8",		10,100,45,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_OPA1_4 , "1/4",		55,100,45,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_OPA1_2 , "1/2",		100,100,45,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_OPA3_4 , "3/4",		145,100,45,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_OPA1_0 , "1",		190,100,45,19, 0, 0, 0, 0, 0, "");
		
		uiDefButF(block, NUMSLI, B_NOP, "Size ",		10,80,225,19, &Gwp.size, 2.0, 64.0, 0, 0, "The size of the brush");

		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, B_DIFF, "Mix",		250,160,60,19, &Gwp.mode, 1.0, 0.0, 0, 0, "Mix the vertex colours");
		uiDefButS(block, ROW, B_DIFF, "Add",		250,140,60,19, &Gwp.mode, 1.0, 1.0, 0, 0, "Add the vertex colour");
		uiDefButS(block, ROW, B_DIFF, "Sub",		250,120,60,19, &Gwp.mode, 1.0, 2.0, 0, 0, "Subtract from the vertex colour");
		uiDefButS(block, ROW, B_DIFF, "Mul",		250,100,60,19, &Gwp.mode, 1.0, 3.0, 0, 0, "Multiply the vertex colour");
		uiDefButS(block, ROW, B_DIFF, "Filter",		250, 80,60,19, &Gwp.mode, 1.0, 4.0, 0, 0, "Mix the colours with an alpha factor");
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, VP_AREA, 0, "All Faces", 	10,50,75,19, &Gwp.flag, 0, 0, 0, 0, "Paint on all faces inside brush");
		uiDefButBitS(block, TOG, VP_SOFT, 0, "Vertex Dist", 85,50,75,19, &Gwp.flag, 0, 0, 0, 0, "Use distances to vertices (instead of paint entire faces)");
		uiDefButBitS(block, TOG, VP_NORMALS, 0, "Normals", 	160,50,75,19, &Gwp.flag, 0, 0, 0, 0, "Applies the vertex normal before painting");
		uiDefButBitS(block, TOG, VP_SPRAY, 0, "Spray",		235,50,75,19, &Gwp.flag, 0, 0, 0, 0, "Keep applying paint effect while holding mouse");
		
		if(ob){
			uiBlockBeginAlign(block);
			uiDefButBitC(block, TOG, OB_DRAWWIRE, REDRAWVIEW3D, "Wire",	10,10,150,19, &ob->dtx, 0, 0, 0, 0, "Displays the active object's wireframe in shaded drawing modes");
			uiDefBut(block, BUT, B_CLR_WPAINT, "Clear",					160,10,150,19, NULL, 0, 0, 0, 0, "Removes reference to this deform group from all vertices");
			uiBlockEndAlign(block);
		}
	}
	else{
		extern VPaint Gvp;         /* from vpaint */
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_NOP, "R ",			979,160,194,19, &Gvp.r, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of red used for painting");
		uiDefButF(block, NUMSLI, B_NOP, "G ",			979,140,194,19, &Gvp.g, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of green used for painting");
		uiDefButF(block, NUMSLI, B_NOP, "B ",			979,120,194,19, &Gvp.b, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of blue used for painting");
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_NOP, "Opacity ",		979,95,194,19, &Gvp.a, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
		uiDefButF(block, NUMSLI, B_NOP, "Size ",		979,75,194,19, &Gvp.size, 2.0, 64.0, 0, 0, "The size of the brush");
		uiBlockEndAlign(block);
		
		uiDefButF(block, COL, B_REDR, "",			1176,120,28,60, &(Gvp.r), 0, 0, 0, B_VPCOLSLI, "");
		
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, B_DIFF, "Mix",			1212,160,63,19, &Gvp.mode, 1.0, 0.0, 0, 0, "Mix the vertex colours");
		uiDefButS(block, ROW, B_DIFF, "Add",			1212,140,63,19, &Gvp.mode, 1.0, 1.0, 0, 0, "Add the vertex colour");
		uiDefButS(block, ROW, B_DIFF, "Sub",			1212, 120,63,19, &Gvp.mode, 1.0, 2.0, 0, 0, "Subtract from the vertex colour");
		uiDefButS(block, ROW, B_DIFF, "Mul",			1212, 100,63,19, &Gvp.mode, 1.0, 3.0, 0, 0, "Multiply the vertex colour");
		uiDefButS(block, ROW, B_DIFF, "Filter",		1212, 80,63,19, &Gvp.mode, 1.0, 4.0, 0, 0, "Mix the colours with an alpha factor");
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, VP_AREA, 0, "All Faces", 		979,50,75,19, &Gvp.flag, 0, 0, 0, 0, "Paint on all faces inside brush");
		uiDefButBitS(block, TOG, VP_SOFT, 0, "Vertex Dist", 	1054,50,75,19, &Gvp.flag, 0, 0, 0, 0, "Use distances to vertices (instead of paint entire faces)");
		uiDefButBitS(block, TOG, VP_NORMALS, 0, "Normals", 	1129,50,75,19, &Gvp.flag, 0, 0, 0, 0, "Applies the vertex normal before painting");
		uiDefButBitS(block, TOG, VP_SPRAY, 0, "Spray",		1204,50,75,19, &Gvp.flag, 0, 0, 0, 0, "Keep applying paint effect while holding mouse");

		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_VPGAMMA, "Set",		979,25,81,19, 0, 0, 0, 0, 0, "Apply Mul and Gamma to vertex colours");
		uiDefButF(block, NUM, B_DIFF, "Mul:", 		1061,25,112,19, &Gvp.mul, 0.1, 50.0, 10, 0, "Set the number to multiply vertex colours with");
		uiDefButF(block, NUM, B_DIFF, "Gamma:", 	1174,25,102,19, &Gvp.gamma, 0.1, 5.0, 10, 0, "Change the clarity of the vertex colours");
		uiBlockEndAlign(block);
		
		uiDefBut(block, BUT, B_SET_VCOL, "Set VertCol",	979,0,81,20, 0, 0, 0, 0, 0, "Set Vertex colour of selection to current (Shift+K)");
	}
	
}

static void editing_panel_mesh_texface(void)
{
	extern VPaint Gvp;         /* from vpaint */
	uiBlock *block;
	extern TFace *lasttface;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_texface", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Texture face", "Editing", 960, 0, 318, 204)==0) return;

	set_lasttface();	// checks for ob type
	if(lasttface) {

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, TF_TEX, B_REDR_3D_IMA, "Tex",	600,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Render face with texture");
		uiDefButBitS(block, TOG, TF_TILES, B_REDR_3D_IMA, "Tiles",	660,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use tilemode for face");
		uiDefButBitS(block, TOG, TF_LIGHT, REDRAWVIEW3D, "Light",	720,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use light for face");
		uiDefButBitS(block, TOG, TF_INVISIBLE, REDRAWVIEW3D, "Invisible",780,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Make face invisible");
		uiDefButBitS(block, TOG, TF_DYNAMIC, REDRAWVIEW3D, "Collision", 840,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use face for collision detection");

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, TF_SHAREDCOL, REDRAWVIEW3D, "Shared",	600,135,60,19, &lasttface->mode, 0, 0, 0, 0, "Blend vertex colours across face when vertices are shared");
		uiDefButBitS(block, TOG, TF_TWOSIDE, REDRAWVIEW3D, "Twoside",660,135,60,19, &lasttface->mode, 0, 0, 0, 0, "Render face twosided");
		uiDefButBitS(block, TOG, TF_OBCOL, REDRAWVIEW3D, "ObColor",720,135,60,19, &lasttface->mode, 0, 0, 0, 0, "Use ObColor instead of vertex colours");

		uiBlockBeginAlign(block);
		
		uiDefButBitS(block, TOG, TF_BILLBOARD, B_TFACE_HALO, "Halo",	600,110,60,19, &lasttface->mode, 0, 0, 0, 0, "Screen aligned billboard");
		uiDefButBitS(block, TOG, TF_BILLBOARD2, B_TFACE_BILLB, "Billboard",660,110,60,19, &lasttface->mode, 0, 0, 0, 0, "Billboard with Z-axis constraint");
		uiDefButBitS(block, TOG, TF_SHADOW, REDRAWVIEW3D, "Shadow", 720,110,60,19, &lasttface->mode, 0, 0, 0, 0, "Face is used for shadow");
		uiDefButBitS(block, TOG, TF_BMFONT, REDRAWVIEW3D, "Text", 780,110,60,19, &lasttface->mode, 0, 0, 0, 0, "Enable bitmap text on face");

		uiBlockBeginAlign(block);
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButC(block, ROW, REDRAWVIEW3D, "Opaque",	600,80,60,19, &lasttface->transp, 2.0, 0.0, 0, 0, "Render colour of textured face as colour");
		uiDefButC(block, ROW, REDRAWVIEW3D, "Add",		660,80,60,19, &lasttface->transp, 2.0, 1.0, 0, 0, "Render face transparent and add colour of face");
		uiDefButC(block, ROW, REDRAWVIEW3D, "Alpha",	720,80,60,19, &lasttface->transp, 2.0, 2.0, 0, 0, "Render polygon transparent, depending on alpha channel of the texture");

		uiBlockSetCol(block, TH_AUTO);

		uiBlockBeginAlign(block);
		uiDefButF(block, COL, B_VPCOLSLI, "",			769,40,40,28, &(Gvp.r), 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_SET_VCOL, "Set VertCol",	809,40,103,28, 0, 0, 0, 0, 0, "Set Vertex colour of selection to current (Shift+K)");

		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_COPY_TF_MODE, "Copy DrawMode", 600,7,117,28, 0, 0, 0, 0, 0, "Copy the drawmode from active face to selected faces");
		uiDefBut(block, BUT, B_COPY_TF_UV, "Copy UV+tex",	  721,7,85,28, 0, 0, 0, 0, 0, "Copy UV information and textures from active face to selected faces");
		uiDefBut(block, BUT, B_COPY_TF_COL, "Copy VertCol",	  809,7,103,28, 0, 0, 0, 0, 0, "Copy vertex colours from active face to selected faces");
	}
}

void do_uvautocalculationbuts(unsigned short event)
{
	switch(event) {
	case B_UVAUTO_STD1:
	case B_UVAUTO_STD2:
	case B_UVAUTO_STD4:
	case B_UVAUTO_STD8:
	case B_UVAUTO_CUBE:
		calculate_uv_map(event);
		break;
	case B_UVAUTO_BOUNDS1:
	case B_UVAUTO_BOUNDS2:
	case B_UVAUTO_BOUNDS4:
	case B_UVAUTO_BOUNDS8:
	case B_UVAUTO_SPHERE:
	case B_UVAUTO_CYLINDER:
	case B_UVAUTO_WINDOW:
		if(select_area(SPACE_VIEW3D)) calculate_uv_map(event);
		break;
	case B_UVAUTO_LSCM:
		unwrap_lscm();
		break;
	}
}

static void editing_panel_mesh_uvautocalculation(void)
{
	uiBlock *block;
	int butH= 19, butHB= 20, row= 180, butS= 10;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_uvautocalculation", UI_EMBOSS, UI_HELV, curarea->win);
	/* make this a tab of "Texture face" to save screen space*/
	uiNewPanelTabbed("Texture face", "Editing");
	if(uiNewPanel(curarea, block, "UV Calculation", "Editing", 960, 0, 318, 204)==0)
		return;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_UVAUTO_LSCM,"LSCM Unwrap",100,row,200,butH, 0, 0, 0, 0, 0, "Applies conformal UV mapping, preserving local angles");
	uiBlockEndAlign(block);
	row-= butHB+butS;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_UVAUTO_STD1,"Standard",100,row,100,butH, 0, 0, 0, 0, 0, "Applies standard UV mapping");
	uiDefBut(block, BUT, B_UVAUTO_STD2,"/2",200,row,33,butH, 0, 0, 0, 0, 0, "Applies standard UV mapping 1/2");
	uiDefBut(block, BUT, B_UVAUTO_STD4,"/4",233,row,34,butH, 0, 0, 0, 0, 0, "Applies standard UV mapping 1/4");
	uiDefBut(block, BUT, B_UVAUTO_STD8,"/8",267,row,33,butH, 0, 0, 0, 0, 0, "Applies standard UV mapping 1/8");
	uiBlockEndAlign(block);
	row-= butHB+butS;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_UVAUTO_BOUNDS1,"Bounds",100,row,100,butH, 0, 0, 0, 0, 0, "Applies planar UV mapping with bounds 1/1");
	uiDefBut(block, BUT, B_UVAUTO_BOUNDS2,"/2",200,row,33,butH, 0, 0, 0, 0, 0, "Applies planar UV mapping with bounds 1/2");
	uiDefBut(block, BUT, B_UVAUTO_BOUNDS4,"/4",233,row,34,butH, 0, 0, 0, 0, 0, "Applies planar UV mapping with bounds 1/4");
	uiDefBut(block, BUT, B_UVAUTO_BOUNDS8,"/8",267,row,33,butH, 0, 0, 0, 0, 0, "Applies planar UV mapping with bounds 1/8");
	uiDefBut(block, BUT, B_UVAUTO_WINDOW,"From Window",100,row-butH,200,butH, 0, 0, 0, 0, 0, "Applies planar UV mapping from window");
	uiBlockEndAlign(block);
	row-= 2*butHB+butS;

	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, G_DRAWFACES, REDRAWVIEW3D, "Draw Faces",	100,row,200,butH, &G.f, 0, 0, 0, 0, "Displays all faces as shades");
	uiDefButBitI(block,TOG, G_DRAWEDGES, REDRAWVIEW3D,"Draw Edges",100,row-butHB,200,butH,&G.f, 2.0, 0, 0, 0,  "Displays edges of visible faces");
 	uiDefButBitI(block,TOG, G_HIDDENEDGES, REDRAWVIEW3D,"Draw Hidden Edges",100,row-2*butHB,200,butH,&G.f, 2.0, 1.0, 0, 0,  "Displays edges of hidden faces");
	uiDefButBitI(block,TOG, G_DRAWSEAMS, REDRAWVIEW3D,"Draw Seams",100,row-3*butHB,200,butH,&G.f, 2.0, 2.0, 0, 0,  "Displays UV unwrapping seams");
	uiBlockEndAlign(block);
	row-= 4*butHB+butS;

	row= 180;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_UVAUTO_CUBE,"Cube",315,row,200,butH, 0, 0, 0, 0, 0, "Applies cube UV mapping");
	uiDefButF(block, NUM,B_UVAUTO_CUBESIZE ,"Size:",315,row-butHB,200,butH, &uv_calc_cubesize, 0.0001, 100.0, 10, 3, "Defines the cubemap size");
	uiBlockEndAlign(block);
	row-= 2*butHB+butS;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_UVAUTO_SPHERE,"Sphere",315,row,200,butH, 0, 0, 0, 0, 0, "Applies spherical UV mapping");
	uiBlockEndAlign(block);
	row-= butHB+butS;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_UVAUTO_CYLINDER,"Cylinder",315,row,200,butH, 0, 0, 0, 0, 0, "Applies cylindrical UV mapping");
	uiDefButF(block, NUM,B_UVAUTO_CYLRADIUS ,"Radius:",315,row-butHB,200,butH, &uv_calc_radius, 0.1, 100.0, 10, 3, "Defines the radius of the UV mapping cylinder");
	uiBlockEndAlign(block);
	row-= 2*butHB+butS;


	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_UVAUTO_FACE,"View Aligns Face",315,row,200,butH, &uv_calc_mapdir,2.0, 1.0, 0.0,0.0, "View is on equator for cylindrical and spherical UV mapping");
	uiDefButS(block, ROW,B_UVAUTO_TOP,"VA Top",315,row-butHB,100,butH, &uv_calc_mapdir,2.0, 0.0, 0.0,0.0, "View is on poles for cylindrical and spherical UV mapping");
	uiDefButS(block, ROW,B_UVAUTO_TOP,"Al Obj",415,row-butHB,100,butH, &uv_calc_mapdir,2.0, 2.0, 0.0,0.0, "Align to object for cylindrical and spherical UV mapping");
	uiBlockEndAlign(block);
	row-= 2*butHB+butS;

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_UVAUTO_ALIGNX,"Polar ZX",315,row,100,butH, &uv_calc_mapalign,2.0, 0.0, 0.0,0.0, "Polar 0 is X for cylindrical and spherical UV mapping");
	uiDefButS(block, ROW,B_UVAUTO_ALIGNY,"Polar ZY",415,row,100,butH, &uv_calc_mapalign,2.0, 1.0, 0.0,0.0, "Polar 0 is Y for cylindrical and spherical UV mapping");
	uiBlockEndAlign(block);
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
	if(ob->id.lib) uiSetButLock(1, "Can't edit library data");
	
	switch(ob->type) {
	case OB_MESH:
		editing_panel_links(ob);
		editing_panel_mesh_type(ob, ob->data);
		editing_panel_modifiers(ob);
		editing_panel_shapes(ob);
		/* modes */
		if(G.obedit) {
			editing_panel_mesh_tools(ob, ob->data);
			editing_panel_mesh_tools1(ob, ob->data);
		}
		else {
			if(G.f & G_FACESELECT) {
				editing_panel_mesh_texface();
				editing_panel_mesh_uvautocalculation();
			}
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
//		editing_panel_shapes(ob);
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
			editing_panel_pose_bones(ob, arm);
			object_panel_constraint("Editing");
		}		
		break;
	}
	uiClearButLock();
}
