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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed,
 *                 Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string>

#include "COLLADASWColor.h"
#include "COLLADASWLight.h"

#include "DNA_lamp_types.h"

#include "LightExporter.h"
#include "collada_internal.h"

template<class Functor>
void forEachLampObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		if (ob->type == OB_LAMP && ob->data) {
			f(ob);
		}
		base= base->next;
	}
}

LightsExporter::LightsExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryLights(sw){}
void LightsExporter::exportLights(Scene *sce)
{
	openLibrary();
	
	forEachLampObjectInScene(sce, *this);
	
	closeLibrary();
}
void LightsExporter::operator()(Object *ob)
{
	Lamp *la = (Lamp*)ob->data;
	std::string la_id(get_light_id(ob));
	std::string la_name(id_name(la));
	COLLADASW::Color col(la->r, la->g, la->b);
	float e = la->energy;
	
	// sun
	if (la->type == LA_SUN) {
		COLLADASW::DirectionalLight cla(mSW, la_id, la_name, e);
		cla.setColor(col);
		addLight(cla);
	}
	// hemi
	else if (la->type == LA_HEMI) {
		COLLADASW::AmbientLight cla(mSW, la_id, la_name, e);
		cla.setColor(col);
		addLight(cla);
	}
	// spot
	else if (la->type == LA_SPOT) {
		COLLADASW::SpotLight cla(mSW, la_id, la_name, e);
		cla.setColor(col);
		cla.setFallOffAngle(la->spotsize);
		cla.setFallOffExponent(la->spotblend);
		cla.setLinearAttenuation(la->att1);
		cla.setQuadraticAttenuation(la->att2);
		addLight(cla);
	}
	// lamp
	else if (la->type == LA_LOCAL) {
		COLLADASW::PointLight cla(mSW, la_id, la_name, e);
		cla.setColor(col);
		cla.setLinearAttenuation(la->att1);
		cla.setQuadraticAttenuation(la->att2);
		addLight(cla);
	}
	// area lamp is not supported
	// it will be exported as a local lamp
	else {
		COLLADASW::PointLight cla(mSW, la_id, la_name, e);
		cla.setColor(col);
		cla.setLinearAttenuation(la->att1);
		cla.setQuadraticAttenuation(la->att2);
		addLight(cla);
	}
}
