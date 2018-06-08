/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed,
 *                 Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/EffectExporter.cpp
 *  \ingroup collada
 */


#include <map>
#include <set>

#include "COLLADASWEffectProfile.h"
#include "COLLADAFWColorOrTexture.h"

#include "EffectExporter.h"
#include "DocumentExporter.h"
#include "MaterialExporter.h"

#include "collada_internal.h"
#include "collada_utils.h"

extern "C" {
	#include "DNA_mesh_types.h"
	#include "DNA_world_types.h"

	#include "BKE_collection.h"
	#include "BKE_customdata.h"
	#include "BKE_mesh.h"
	#include "BKE_material.h"
}

// OB_MESH is assumed
static std::string getActiveUVLayerName(Object *ob)
{
	Mesh *me = (Mesh *)ob->data;

	int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
	if (num_layers)
		return std::string(bc_CustomData_get_active_layer_name(&me->fdata, CD_MTFACE));

	return "";
}

EffectsExporter::EffectsExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings) : COLLADASW::LibraryEffects(sw), export_settings(export_settings) {
}

bool EffectsExporter::hasEffects(Scene *sce)
{
	FOREACH_SCENE_OBJECT_BEGIN(sce, ob)
	{
		int a;
		for (a = 0; a < ob->totcol; a++) {
			Material *ma = give_current_material(ob, a + 1);

			// no material, but check all of the slots
			if (!ma) continue;

			return true;
		}
	}
	FOREACH_SCENE_OBJECT_END;
	return false;
}

void EffectsExporter::exportEffects(Scene *sce)
{
	if (hasEffects(sce)) {
		this->scene = sce;
		openLibrary();
		MaterialFunctor mf;
		mf.forEachMaterialInExportSet<EffectsExporter>(sce, *this, this->export_settings->export_set);

		closeLibrary();
	}
}

void EffectsExporter::writeLambert(COLLADASW::EffectProfile &ep, Material *ma)
{
	COLLADASW::ColorOrTexture cot;
	ep.setShaderType(COLLADASW::EffectProfile::LAMBERT);
}

void EffectsExporter::operator()(Material *ma, Object *ob)
{
	// TODO: add back texture and extended material parameter support

	openEffect(translate_id(id_name(ma)) + "-effect");

	COLLADASW::EffectProfile ep(mSW);
	ep.setProfileType(COLLADASW::EffectProfile::COMMON);
	ep.openProfile();
	writeLambert(ep, ma);

	COLLADASW::ColorOrTexture cot;

	// transparency
	if (ma->alpha != 1.0f) {
		// Tod: because we are in A_ONE mode transparency is calculated like this:
		cot = getcol(1.0f, 1.0f, 1.0f, ma->alpha);
		ep.setTransparent(cot);
		ep.setOpaque(COLLADASW::EffectProfile::A_ONE);
	}

	// emission
#if 0
	cot = getcol(ma->emit, ma->emit, ma->emit, 1.0f);
#endif

	// diffuse
	cot = getcol(ma->r, ma->g, ma->b, 1.0f);
	ep.setDiffuse(cot, false, "diffuse");

	// specular
	if (ep.getShaderType() != COLLADASW::EffectProfile::LAMBERT) {
		cot = getcol(ma->specr * ma->spec, ma->specg * ma->spec, ma->specb * ma->spec, 1.0f);
		ep.setSpecular(cot, false, "specular");
	}

	// XXX make this more readable if possible

#if 0
	// create <sampler> and <surface> for each image
	COLLADASW::Sampler samplers[MAX_MTEX];
	//COLLADASW::Surface surfaces[MAX_MTEX];
	//void *samp_surf[MAX_MTEX][2];
	void *samp_surf[MAX_MTEX];

	// image to index to samp_surf map
	// samp_surf[index] stores 2 pointers, sampler and surface
	std::map<std::string, int> im_samp_map;

	unsigned int a, b;
	for (a = 0, b = 0; a < tex_indices.size(); a++) {
		MTex *t = ma->mtex[tex_indices[a]];
		Image *ima = t->tex->ima;

		// Image not set for texture
		if (!ima) continue;

		std::string key(id_name(ima));
		key = translate_id(key);

		// create only one <sampler>/<surface> pair for each unique image
		if (im_samp_map.find(key) == im_samp_map.end()) {
			// //<newparam> <surface> <init_from>
			// COLLADASW::Surface surface(COLLADASW::Surface::SURFACE_TYPE_2D,
			//                         key + COLLADASW::Surface::SURFACE_SID_SUFFIX);
			// COLLADASW::SurfaceInitOption sio(COLLADASW::SurfaceInitOption::INIT_FROM);
			// sio.setImageReference(key);
			// surface.setInitOption(sio);

			// COLLADASW::NewParamSurface surface(mSW);
			// surface->setParamType(COLLADASW::CSW_SURFACE_TYPE_2D);

			//<newparam> <sampler> <source>
			COLLADASW::Sampler sampler(COLLADASW::Sampler::SAMPLER_TYPE_2D,
			                           key + COLLADASW::Sampler::SAMPLER_SID_SUFFIX,
			                           key + COLLADASW::Sampler::SURFACE_SID_SUFFIX);
			sampler.setImageId(key);
			// copy values to arrays since they will live longer
			samplers[a] = sampler;
			//surfaces[a] = surface;

			// store pointers so they can be used later when we create <texture>s
			samp_surf[b] = &samplers[a];
			//samp_surf[b][1] = &surfaces[a];

			im_samp_map[key] = b;
			b++;
		}
	}
#endif

	// used as fallback when MTex->uvname is "" (this is pretty common)
	// it is indeed the correct value to use in that case
	std::string active_uv(getActiveUVLayerName(ob));

	// write textures
	// XXX very slow
#if 0
	for (a = 0; a < tex_indices.size(); a++) {
		MTex *t = ma->mtex[tex_indices[a]];
		Image *ima = t->tex->ima;

		if (!ima) {
			continue;
		}

		std::string key(id_name(ima));
		key = translate_id(key);
		int i = im_samp_map[key];
		std::string uvname = strlen(t->uvname) ? t->uvname : active_uv;
		COLLADASW::Sampler *sampler = (COLLADASW::Sampler *)samp_surf[i];
		writeTextures(ep, key, sampler, t, ima, uvname);
	}
#endif

	// performs the actual writing
	ep.addProfileElements();
	bool twoSided = false;
	if (ob->type == OB_MESH && ob->data) {
		Mesh *me = (Mesh *)ob->data;
		if (me->flag & ME_TWOSIDED)
			twoSided = true;
	}
	if (twoSided)
		ep.addExtraTechniqueParameter("GOOGLEEARTH", "double_sided", 1);
	ep.addExtraTechniques(mSW);

	ep.closeProfile();
	if (twoSided)
		mSW->appendTextBlock("<extra><technique profile=\"MAX3D\"><double_sided>1</double_sided></technique></extra>");
	closeEffect();
}

COLLADASW::ColorOrTexture EffectsExporter::createTexture(Image *ima,
                                                         std::string& uv_layer_name,
                                                         COLLADASW::Sampler *sampler
                                                         /*COLLADASW::Surface *surface*/)
{

	COLLADASW::Texture texture(translate_id(id_name(ima)));
	texture.setTexcoord(uv_layer_name);
	//texture.setSurface(*surface);
	texture.setSampler(*sampler);

	COLLADASW::ColorOrTexture cot(texture);
	return cot;
}

COLLADASW::ColorOrTexture EffectsExporter::getcol(float r, float g, float b, float a)
{
	COLLADASW::Color color(r, g, b, a);
	COLLADASW::ColorOrTexture cot(color);
	return cot;
}
