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

/** \file EffectExporter.h
 *  \ingroup collada
 */

#ifndef __EFFECTEXPORTER_H__
#define __EFFECTEXPORTER_H__

#include <string>
#include <vector>

#include "COLLADASWColorOrTexture.h"
#include "COLLADASWStreamWriter.h"
#include "COLLADASWSampler.h"
#include "COLLADASWLibraryEffects.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ExportSettings.h"

class EffectsExporter: COLLADASW::LibraryEffects
{
public:
	EffectsExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings);
	void exportEffects(Scene *sce);

	void operator()(Material *ma, Object *ob);
	
	COLLADASW::ColorOrTexture createTexture(Image *ima,
											std::string& uv_layer_name,
											COLLADASW::Sampler *sampler
											/*COLLADASW::Surface *surface*/);
	
	COLLADASW::ColorOrTexture getcol(float r, float g, float b, float a);
private:
	/** Fills the array of mtex indices which have image. Used for exporting images. */
	void createTextureIndices(Material *ma, std::vector<int> &indices);
	
	void writeBlinn(COLLADASW::EffectProfile &ep, Material *ma);
	void writeLambert(COLLADASW::EffectProfile &ep, Material *ma);
	void writePhong(COLLADASW::EffectProfile &ep, Material *ma);
	void writeTextures(COLLADASW::EffectProfile &ep,
			std::string &key,
			COLLADASW::Sampler *sampler, 
			MTex *t, Image *ima,
			std::string &uvname );

	bool hasEffects(Scene *sce);
	
	const ExportSettings *export_settings;
	
	Scene *scene;
};

#endif
