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

#include "COLLADASWEffectProfile.h"
#include "COLLADAFWColorOrTexture.h"

#include "EffectExporter.h"
#include "DocumentExporter.h"
#include "MaterialExporter.h"

#include "collada_internal.h"
#include "collada_utils.h"

extern "C" {
	#include "DNA_mesh_types.h"
	#include "DNA_texture_types.h"
	#include "DNA_world_types.h"

	#include "BKE_customdata.h"
	#include "BKE_mesh.h"
	#include "BKE_material.h"
}

EffectsExporter::EffectsExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings) : COLLADASW::LibraryEffects(sw), export_settings(export_settings) {
}


bool EffectsExporter::hasEffects(Scene *sce)
{
	Base *base = (Base *)sce->base.first;
	
	while (base) {
		Object *ob = base->object;
		int a;
		for (a = 0; a < ob->totcol; a++) {
			Material *ma = give_current_material(ob, a + 1);

			// no material, but check all of the slots
			if (!ma) continue;

			return true;
		}
		base = base->next;
	}
	return false;
}

void EffectsExporter::exportEffects(Scene *sce)
{
	this->scene = sce;
	
	if (this->export_settings->export_texture_type == BC_TEXTURE_TYPE_MAT) {
		if (hasEffects(sce)) {
				MaterialFunctor mf;
				openLibrary();
				mf.forEachMaterialInExportSet<EffectsExporter>(sce, *this, this->export_settings->export_set);
				closeLibrary();
		}
	}
	else {
		std::set<Object *> uv_textured_obs = bc_getUVTexturedObjects(sce, !this->export_settings->active_uv_only);
		std::set<Image *> uv_images = bc_getUVImages(sce, !this->export_settings->active_uv_only);
		if (uv_images.size() > 0) {
			openLibrary();
			std::set<Image *>::iterator uv_images_iter;
			for (uv_images_iter = uv_images.begin();
				uv_images_iter != uv_images.end();
				uv_images_iter++) {

				Image *ima = *uv_images_iter;
				std::string key(id_name(ima));
				key = translate_id(key);
				COLLADASW::Sampler sampler(COLLADASW::Sampler::SAMPLER_TYPE_2D,
					key + COLLADASW::Sampler::SAMPLER_SID_SUFFIX,
					key + COLLADASW::Sampler::SURFACE_SID_SUFFIX);
				sampler.setImageId(key);

				openEffect(key + "-effect");
				COLLADASW::EffectProfile ep(mSW);
				ep.setProfileType(COLLADASW::EffectProfile::COMMON);
				ep.setShaderType(COLLADASW::EffectProfile::PHONG);
				ep.setDiffuse(createTexture(ima, key, &sampler), false, "diffuse");
				COLLADASW::ColorOrTexture cot = getcol(0, 0, 0, 1.0f);
				ep.setSpecular(cot, false, "specular");
				ep.openProfile();
				ep.addProfileElements();
				ep.addExtraTechniques(mSW);
				ep.closeProfile();
				closeEffect();
			}
			closeLibrary();
		}
	}
}

void EffectsExporter::writeBlinn(COLLADASW::EffectProfile &ep, Material *ma)
{
	COLLADASW::ColorOrTexture cot;
	ep.setShaderType(COLLADASW::EffectProfile::BLINN);
	// shininess
	ep.setShininess(ma->har, false, "shininess");
	// specular
	cot = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
	ep.setSpecular(cot, false, "specular");
}

void EffectsExporter::writeLambert(COLLADASW::EffectProfile &ep, Material *ma)
{
	COLLADASW::ColorOrTexture cot;
	ep.setShaderType(COLLADASW::EffectProfile::LAMBERT);
}

void EffectsExporter::writePhong(COLLADASW::EffectProfile &ep, Material *ma)
{
	COLLADASW::ColorOrTexture cot;
	ep.setShaderType(COLLADASW::EffectProfile::PHONG);
	// shininess
	ep.setShininess(ma->har, false, "shininess");
	// specular
	cot = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
	ep.setSpecular(cot, false, "specular");
}

void EffectsExporter::writeTextures(COLLADASW::EffectProfile &ep,
									std::string &key,
									COLLADASW::Sampler *sampler, 
									MTex *t, Image *ima,
									std::string &uvname ) {
		
	// Image not set for texture
	if (!ima) return;

	// color
	if (t->mapto & MAP_COL) {
		ep.setDiffuse(createTexture(ima, uvname, sampler), false, "diffuse");
	}
	// ambient
	if (t->mapto & MAP_AMB) {
		ep.setAmbient(createTexture(ima, uvname, sampler), false, "ambient");
	}
	// specular
	if (t->mapto & (MAP_SPEC | MAP_COLSPEC)) {
		ep.setSpecular(createTexture(ima, uvname, sampler), false, "specular");
	}
	// emission
	if (t->mapto & MAP_EMIT) {
		ep.setEmission(createTexture(ima, uvname, sampler), false, "emission");
	}
	// reflective
	if (t->mapto & MAP_REF) {
		ep.setReflective(createTexture(ima, uvname, sampler));
	}
	// alpha
	if (t->mapto & MAP_ALPHA) {
		ep.setTransparent(createTexture(ima, uvname, sampler));
	}
	// extension:
	// Normal map --> Must be stored with <extra> tag as different technique, 
	// since COLLADA doesn't support normal maps, even in current COLLADA 1.5.
	if (t->mapto & MAP_NORM) {
		COLLADASW::Texture texture(key);
		texture.setTexcoord(uvname);
		texture.setSampler(*sampler);
		// technique FCOLLADA, with the <bump> tag, is most likely the best understood,
		// most widespread de-facto standard.
		texture.setProfileName("FCOLLADA");
		texture.setChildElementName("bump");
		ep.addExtraTechniqueColorOrTexture(COLLADASW::ColorOrTexture(texture));
	}
}

void EffectsExporter::operator()(Material *ma, Object *ob)
{
	// create a list of indices to textures of type TEX_IMAGE
	std::vector<int> tex_indices;
	createTextureIndices(ma, tex_indices);

	openEffect(translate_id(id_name(ma)) + "-effect");
	
	COLLADASW::EffectProfile ep(mSW);
	ep.setProfileType(COLLADASW::EffectProfile::COMMON);
	ep.openProfile();
	// set shader type - one of three blinn, phong or lambert
	if (ma->spec > 0.0f) {
		if (ma->spec_shader == MA_SPEC_BLINN) {
			writeBlinn(ep, ma);
		}
		else {
			// \todo figure out handling of all spec+diff shader combos blender has, for now write phong
			// for now set phong in case spec shader is not blinn
			writePhong(ep, ma);
		}
	}
	else {
		if (ma->diff_shader == MA_DIFF_LAMBERT) {
			writeLambert(ep, ma);
		}
		else {
			// \todo figure out handling of all spec+diff shader combos blender has, for now write phong
			writePhong(ep, ma);
		}
	}
	
	// index of refraction
	if (ma->mode & MA_RAYTRANSP) {
		ep.setIndexOfRefraction(ma->ang, false, "index_of_refraction");
	}
	else {
		ep.setIndexOfRefraction(1.0f, false, "index_of_refraction");
	}

	COLLADASW::ColorOrTexture cot;

	// transparency
	if (ma->mode & MA_TRANSP) {
		// Tod: because we are in A_ONE mode transparency is calculated like this:
		cot = getcol(1.0f, 1.0f, 1.0f, ma->alpha);
		ep.setTransparent(cot);
		ep.setOpaque(COLLADASW::EffectProfile::A_ONE);
	}

	// emission
	cot = getcol(ma->emit, ma->emit, ma->emit, 1.0f);
	ep.setEmission(cot, false, "emission");

	// diffuse multiplied by diffuse intensity
	cot = getcol(ma->r * ma->ref, ma->g * ma->ref, ma->b * ma->ref, 1.0f);
	ep.setDiffuse(cot, false, "diffuse");

	// ambient
	/* ma->ambX is calculated only on render, so lets do it here manually and not rely on ma->ambX. */
	if (this->scene->world)
		cot = getcol(this->scene->world->ambr * ma->amb, this->scene->world->ambg * ma->amb, this->scene->world->ambb * ma->amb, 1.0f);
	else
		cot = getcol(ma->amb, ma->amb, ma->amb, 1.0f);

	ep.setAmbient(cot, false, "ambient");

	// reflective, reflectivity
	if (ma->mode & MA_RAYMIRROR) {
		cot = getcol(ma->mirr, ma->mirg, ma->mirb, 1.0f);
		ep.setReflective(cot);
		ep.setReflectivity(ma->ray_mirror);
	}
	// else {
	//  cot = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
	//  ep.setReflective(cot);
	//  ep.setReflectivity(ma->spec);
	// }

	// specular
	if (ep.getShaderType() != COLLADASW::EffectProfile::LAMBERT) {
		cot = getcol(ma->specr * ma->spec, ma->specg * ma->spec, ma->specb * ma->spec, 1.0f);
		ep.setSpecular(cot, false, "specular");
	}

	// XXX make this more readable if possible

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

	// used as fallback when MTex->uvname is "" (this is pretty common)
	// it is indeed the correct value to use in that case
	std::string active_uv(bc_get_active_uvlayer_name(ob));

	// write textures
	// XXX very slow
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

//returns the array of mtex indices which have image 
//need this for exporting textures
void EffectsExporter::createTextureIndices(Material *ma, std::vector<int> &indices)
{
	indices.clear();

	for (int a = 0; a < MAX_MTEX; a++) {
		if (ma->mtex[a] &&
		    ma->mtex[a]->tex &&
		    ma->mtex[a]->tex->type == TEX_IMAGE &&
		    ma->mtex[a]->texco == TEXCO_UV)
		{
			indices.push_back(a);
		}
	}
}
