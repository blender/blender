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
 * Contributor(s): Blender Foundation, full update, glsl support
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_bmfont.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_property.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "GPU_extensions.h"
#include "GPU_draw.h"

#include "ED_mesh.h"

#include "view3d_intern.h"	// own include

/***/

	/* Flags for marked edges */
enum {
	eEdge_Visible = (1<<0),
	eEdge_Select = (1<<1),
};

	/* Creates a hash of edges to flags indicating
	 * adjacent tface select/active/etc flags.
	 */
static void get_marked_edge_info__orFlags(EdgeHash *eh, int v0, int v1, int flags)
{
	int *flags_p;

	if (!BLI_edgehash_haskey(eh, v0, v1)) {
		BLI_edgehash_insert(eh, v0, v1, 0);
	}

	flags_p = (int*) BLI_edgehash_lookup_p(eh, v0, v1);
	*flags_p |= flags;
}

static EdgeHash *get_tface_mesh_marked_edge_info(Mesh *me)
{
	EdgeHash *eh = BLI_edgehash_new();
	int i;
	MFace *mf;
	MTFace *tf = NULL;
	
	for (i=0; i<me->totface; i++) {
		mf = &me->mface[i];
		if (me->mtface)
			tf = &me->mtface[i];
		
		if (mf->v3) {
			if (!(mf->flag&ME_HIDE)) {
				unsigned int flags = eEdge_Visible;
				if (mf->flag&ME_FACE_SEL) flags |= eEdge_Select;

				get_marked_edge_info__orFlags(eh, mf->v1, mf->v2, flags);
				get_marked_edge_info__orFlags(eh, mf->v2, mf->v3, flags);
				if (mf->v4) {
					get_marked_edge_info__orFlags(eh, mf->v3, mf->v4, flags);
					get_marked_edge_info__orFlags(eh, mf->v4, mf->v1, flags);
				} else {
					get_marked_edge_info__orFlags(eh, mf->v3, mf->v1, flags);
				}
			}
		}
	}

	return eh;
}


static int draw_tfaces3D__setHiddenOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; } *data = userData;
	Mesh *me= data->me;
	MEdge *med = &me->medge[index];
	uintptr_t flags = (intptr_t) BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	if((me->drawflag & ME_DRAWSEAMS) && (med->flag&ME_SEAM)) {
		return 0;
	} else if(me->drawflag & ME_DRAWEDGES){ 
		if (me->drawflag & ME_HIDDENEDGES) {
			return 1;
		} else {
			return (flags & eEdge_Visible);
		}
	} else {
		return (flags & eEdge_Select);
	}
}

static int draw_tfaces3D__setSeamOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; } *data = userData;
	Mesh *me= data->me;
	MEdge *med = &data->me->medge[index];
	uintptr_t flags = (intptr_t) BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	if (med->flag & ME_SEAM) {
		if (me->drawflag & ME_HIDDENEDGES) {
			return 1;
		} else {
			return (flags & eEdge_Visible);
		}
	} else {
		return 0;
	}
}

static int draw_tfaces3D__setSelectOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; } *data = userData;
	MEdge *med = &data->me->medge[index];
	uintptr_t flags = (intptr_t) BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	return flags & eEdge_Select;
}

static int draw_tfaces3D__setActiveOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; } *data = userData;
	MEdge *med = &data->me->medge[index];
	uintptr_t flags = (intptr_t) BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	if (flags & eEdge_Select) {
		return 1;
	} else {
		return 0;
	}
}

static int draw_tfaces3D__drawFaceOpts(void *userData, int index)
{
	Mesh *me = (Mesh*)userData;

	MFace *mface = &me->mface[index];
	if (!(mface->flag&ME_HIDE) && (mface->flag&ME_FACE_SEL))
		return 2; /* Don't set color */
	else
		return 0;
}

static void draw_tfaces3D(RegionView3D *rv3d, Object *ob, Mesh *me, DerivedMesh *dm)
{
	struct { Mesh *me; EdgeHash *eh; } data;

	data.me = me;
	data.eh = get_tface_mesh_marked_edge_info(me);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	bglPolygonOffset(rv3d->dist, 1.0);

		/* Draw (Hidden) Edges */
	UI_ThemeColor(TH_EDGE_FACESEL);
	dm->drawMappedEdges(dm, draw_tfaces3D__setHiddenOpts, &data);

		/* Draw Seams */
	if(me->drawflag & ME_DRAWSEAMS) {
		UI_ThemeColor(TH_EDGE_SEAM);
		glLineWidth(2);

		dm->drawMappedEdges(dm, draw_tfaces3D__setSeamOpts, &data);

		glLineWidth(1);
	}

	/* Draw Selected Faces */
	if(me->drawflag & ME_DRAWFACES) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		UI_ThemeColor4(TH_FACE_SELECT);

		dm->drawMappedFacesTex(dm, draw_tfaces3D__drawFaceOpts, (void*)me);

		glDisable(GL_BLEND);
	}
	
	bglPolygonOffset(rv3d->dist, 1.0);

		/* Draw Stippled Outline for selected faces */
	glColor3ub(255, 255, 255);
	setlinestyle(1);
	dm->drawMappedEdges(dm, draw_tfaces3D__setSelectOpts, &data);
	setlinestyle(0);

	dm->drawMappedEdges(dm, draw_tfaces3D__setActiveOpts, &data);

	bglPolygonOffset(rv3d->dist, 0.0);	// resets correctly now, even after calling accumulated offsets

	BLI_edgehash_free(data.eh, NULL);
}

static Material *give_current_material_or_def(Object *ob, int matnr)
{
	extern Material defmaterial;	// render module abuse...
	Material *ma= give_current_material(ob, matnr);

	return ma?ma:&defmaterial;
}

static int set_draw_settings_cached(int clearcache, int textured, MTFace *texface, int lit, Object *litob, int litmatnr, int doublesided)
{
	static int c_textured;
	static int c_lit;
	static int c_doublesided;
	static MTFace *c_texface;
	static Object *c_litob;
	static int c_litmatnr;
	static int c_badtex;

	if (clearcache) {
		c_textured= c_lit= c_doublesided= -1;
		c_texface= (MTFace*) -1;
		c_litob= (Object*) -1;
		c_litmatnr= -1;
		c_badtex= 0;
	}

	if (texface) {
		lit = lit && (lit==-1 || texface->mode&TF_LIGHT);
		textured = textured && (texface->mode&TF_TEX);
		doublesided = texface->mode&TF_TWOSIDE;
	} else {
		textured = 0;
	}

	if (doublesided!=c_doublesided) {
		if (doublesided) glDisable(GL_CULL_FACE);
		else glEnable(GL_CULL_FACE);

		c_doublesided= doublesided;
	}

	if (textured!=c_textured || texface!=c_texface) {
		if (textured ) {
			c_badtex= !GPU_set_tpage(texface, !(litob->mode & OB_MODE_TEXTURE_PAINT));
		} else {
			GPU_set_tpage(NULL, 0);
			c_badtex= 0;
		}
		c_textured= textured;
		c_texface= texface;
	}

	if (c_badtex) lit= 0;
	if (lit!=c_lit || litob!=c_litob || litmatnr!=c_litmatnr) {
		if (lit) {
			Material *ma= give_current_material_or_def(litob, litmatnr+1);
			float spec[4];

			spec[0]= ma->spec*ma->specr;
			spec[1]= ma->spec*ma->specg;
			spec[2]= ma->spec*ma->specb;
			spec[3]= 1.0;

			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
			glEnable(GL_LIGHTING);
			glEnable(GL_COLOR_MATERIAL);
		}
		else {
			glDisable(GL_LIGHTING); 
			glDisable(GL_COLOR_MATERIAL);
		}
		c_lit= lit;
		c_litob= litob;
		c_litmatnr= litmatnr;
	}

	return c_badtex;
}

/* Icky globals, fix with userdata parameter */

struct TextureDrawState {
	Object *ob;
	int islit, istex;
	unsigned char obcol[4];
} Gtexdraw = {NULL, 0, 0, {0, 0, 0, 0}};

static void draw_textured_begin(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob)
{
	unsigned char obcol[4];
	int istex, solidtex= 0;

	// XXX scene->obedit warning
	if(v3d->drawtype==OB_SOLID || (ob==scene->obedit && v3d->drawtype!=OB_TEXTURE)) {
		/* draw with default lights in solid draw mode and edit mode */
		solidtex= 1;
		Gtexdraw.islit= -1;
	}
	else {
		/* draw with lights in the scene otherwise */
		Gtexdraw.islit= GPU_scene_object_lights(scene, ob, v3d->lay, rv3d->viewmat, get_view3d_ortho(v3d, rv3d));
	}
	
	obcol[0]= CLAMPIS(ob->col[0]*255, 0, 255);
	obcol[1]= CLAMPIS(ob->col[1]*255, 0, 255);
	obcol[2]= CLAMPIS(ob->col[2]*255, 0, 255);
	obcol[3]= CLAMPIS(ob->col[3]*255, 0, 255);
	
	glCullFace(GL_BACK); glEnable(GL_CULL_FACE);
	if(solidtex || v3d->drawtype==OB_TEXTURE) istex= 1;
	else istex= 0;

	Gtexdraw.ob = ob;
	Gtexdraw.istex = istex;
	memcpy(Gtexdraw.obcol, obcol, sizeof(obcol));
	set_draw_settings_cached(1, 0, 0, Gtexdraw.islit, 0, 0, 0);
	glShadeModel(GL_SMOOTH);
}

static void draw_textured_end()
{
	/* switch off textures */
	GPU_set_tpage(NULL, 0);

	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);

	/* XXX, bad patch - GPU_default_lights() calls
	 * glLightfv(GL_LIGHT_POSITION, ...) which
	 * is transformed by the current matrix... we
	 * need to make sure that matrix is identity.
	 * 
	 * It would be better if drawmesh.c kept track
	 * of and restored the light settings it changed.
	 *  - zr
	 */
	glPushMatrix();
	glLoadIdentity();	
	GPU_default_lights();
	glPopMatrix();
}


static int draw_tface__set_draw(MTFace *tface, MCol *mcol, int matnr)
{
	if (tface && (tface->mode&TF_INVISIBLE)) return 0;

	if (tface && set_draw_settings_cached(0, Gtexdraw.istex, tface, Gtexdraw.islit, Gtexdraw.ob, matnr, TF_TWOSIDE)) {
		glColor3ub(0xFF, 0x00, 0xFF);
		return 2; /* Don't set color */
	} else if (tface && tface->mode&TF_OBCOL) {
		glColor3ubv(Gtexdraw.obcol);
		return 2; /* Don't set color */
	} else if (!mcol) {
		if (tface) glColor3f(1.0, 1.0, 1.0);
		else {
			Material *ma= give_current_material(Gtexdraw.ob, matnr+1);
			if(ma) glColor3f(ma->r, ma->g, ma->b);
			else glColor3f(1.0, 1.0, 1.0);
		}
		return 2; /* Don't set color */
	} else {
		return 1; /* Set color from mcol */
	}
}

static int draw_tface_mapped__set_draw(void *userData, int index)
{
	Mesh *me = (Mesh*)userData;
	MTFace *tface = (me->mtface)? &me->mtface[index]: NULL;
	MFace *mface = (me->mface)? &me->mface[index]: NULL;
	MCol *mcol = (me->mcol)? &me->mcol[index]: NULL;
	int matnr = me->mface[index].mat_nr;
	if (mface && mface->flag&ME_HIDE) return 0;
	return draw_tface__set_draw(tface, mcol, matnr);
}

static int draw_em_tf_mapped__set_draw(void *userData, int index)
{
	EditMesh *em = userData;
	EditFace *efa= EM_get_face_for_index(index);
	MTFace *tface;
	MCol *mcol;
	int matnr;

	if (efa==NULL || efa->h)
		return 0;

	tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
	mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
	matnr = efa->mat_nr;

	return draw_tface__set_draw(tface, mcol, matnr);
}

static int wpaint__setSolidDrawOptions(void *userData, int index, int *drawSmooth_r)
{
	Mesh *me = (Mesh*)userData;
	MTFace *tface = (me->mtface)? &me->mtface[index]: NULL;
	MFace *mface = (me->mface)? &me->mface[index]: NULL;
	
	if ((mface->flag&ME_HIDE) || (tface && (tface->mode&TF_INVISIBLE))) 
			return 0;
	
	*drawSmooth_r = 1;
	return 1;
}

void draw_mesh_text(Scene *scene, Object *ob, int glsl)
{
	Mesh *me = ob->data;
	DerivedMesh *ddm;
	MFace *mf, *mface= me->mface;
	MTFace *tface= me->mtface;
	MCol *mcol= me->mcol;	/* why does mcol exist? */
	bProperty *prop = get_ob_property(ob, "Text");
	GPUVertexAttribs gattribs;
	int a, totface= me->totface;

	/* don't draw without tfaces */
	if(!tface)
		return;

	/* don't draw when editing */
	if(ob == scene->obedit)
		return;
	else if(ob==OBACT)
		if(paint_facesel_test(ob))
			return;

	ddm = mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);

	for(a=0, mf=mface; a<totface; a++, tface++, mf++) {
		int mode= tface->mode;
		int matnr= mf->mat_nr;
		int mf_smooth= mf->flag & ME_SMOOTH;

		if (!(mf->flag&ME_HIDE) && !(mode&TF_INVISIBLE) && (mode&TF_BMFONT)) {
			float v1[3], v2[3], v3[3], v4[3];
			char string[MAX_PROPSTRING];
			int characters, i, glattrib= -1, badtex= 0;

			if(glsl) {
				GPU_enable_material(matnr+1, &gattribs);

				for(i=0; i<gattribs.totlayer; i++) {
					if(gattribs.layer[i].type == CD_MTFACE) {
						glattrib = gattribs.layer[i].glindex;
						break;
					}
				}
			}
			else {
				badtex = set_draw_settings_cached(0, Gtexdraw.istex, tface, Gtexdraw.islit, Gtexdraw.ob, matnr, TF_TWOSIDE);
				if (badtex) {
					if (mcol) mcol+=4;
					continue;
				}
			}

			ddm->getVertCo(ddm, mf->v1, v1);
			ddm->getVertCo(ddm, mf->v2, v2);
			ddm->getVertCo(ddm, mf->v3, v3);
			if (mf->v4) ddm->getVertCo(ddm, mf->v4, v4);

			// The BM_FONT handling is in the gpu module, shared with the
			// game engine, was duplicated previously

			set_property_valstr(prop, string);
			characters = strlen(string);
			
			if(!BKE_image_get_ibuf(tface->tpage, NULL))
				characters = 0;

			if (!mf_smooth) {
				float nor[3];

				CalcNormFloat(v1, v2, v3, nor);

				glNormal3fv(nor);
			}

			GPU_render_text(tface, tface->mode, string, characters,
				(unsigned int*)mcol, v1, v2, v3, (mf->v4? v4: NULL), glattrib);
		}
		if (mcol) {
			mcol+=4;
		}
	}

	ddm->release(ddm);
}

void draw_mesh_textured(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob, DerivedMesh *dm, int faceselect)
{
	Mesh *me= ob->data;
	
	/* correct for negative scale */
	if(ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);
	
	/* draw the textured mesh */
	draw_textured_begin(scene, v3d, rv3d, ob);

	if(ob == scene->obedit) {
		dm->drawMappedFacesTex(dm, draw_em_tf_mapped__set_draw, me->edit_mesh);
	} else if(faceselect) {
		if(ob->mode & OB_MODE_WEIGHT_PAINT)
			dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, me, 1);
		else
			dm->drawMappedFacesTex(dm, draw_tface_mapped__set_draw, me);
	}
	else {
		dm->drawFacesTex(dm, draw_tface__set_draw);
	}

	/* draw game engine text hack */
	if(get_ob_property(ob, "Text")) 
		draw_mesh_text(scene, ob, 0);

	draw_textured_end();
	
	/* draw edges and selected faces over textured mesh */
	if(!(ob == scene->obedit) && faceselect)
		draw_tfaces3D(rv3d, ob, me, dm);

	/* reset from negative scale correction */
	glFrontFace(GL_CCW);
	
	/* in editmode, the blend mode needs to be set incase it was ADD */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

