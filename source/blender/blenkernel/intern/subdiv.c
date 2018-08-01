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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/subdiv.c
 *  \ingroup bke
 */

#include "BKE_subdiv.h"

#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "subdiv_converter.h"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_capi.h"
#  include "opensubdiv_converter_capi.h"
#  include "opensubdiv_evaluator_capi.h"
#  include "opensubdiv_topology_refiner_capi.h"
#endif

Subdiv *BKE_subdiv_new_from_converter(const SubdivSettings *settings,
                                      struct OpenSubdiv_Converter *converter)
{
#ifdef WITH_OPENSUBDIV
	SubdivStats stats;
	BKE_subdiv_stats_init(&stats);
	BKE_subdiv_stats_begin(&stats, SUBDIV_STATS_TOPOLOGY_REFINER_CREATION_TIME);
	OpenSubdiv_TopologyRefinerSettings topology_refiner_settings;
	topology_refiner_settings.level = settings->level;
	topology_refiner_settings.is_adaptive = settings->is_adaptive;
	struct OpenSubdiv_TopologyRefiner *osd_topology_refiner = NULL;
	if (converter->getNumVertices(converter) != 0) {
		osd_topology_refiner =
		        openSubdiv_createTopologyRefinerFromConverter(
		                converter, &topology_refiner_settings);

	}
	else {
		/* TODO(sergey): Check whether original geometry had any vertices.
		 * The thing here is: OpenSubdiv can only deal with faces, but our
		 * side of subdiv also deals with loose vertices and edges.
		 */
	}
	Subdiv *subdiv = MEM_callocN(sizeof(Subdiv), "subdiv from converetr");
	subdiv->settings = *settings;
	subdiv->topology_refiner = osd_topology_refiner;
	subdiv->evaluator = NULL;
	BKE_subdiv_stats_end(&stats, SUBDIV_STATS_TOPOLOGY_REFINER_CREATION_TIME);
	subdiv->stats = stats;
	return subdiv;
#else
	UNUSED_VARS(settings, converter);
	return NULL;
#endif
}

Subdiv *BKE_subdiv_new_from_mesh(const SubdivSettings *settings,
                                 struct Mesh *mesh)
{
#ifdef WITH_OPENSUBDIV
	if (mesh->totvert == 0) {
		return NULL;
	}
	OpenSubdiv_Converter converter;
	BKE_subdiv_converter_init_for_mesh(&converter, settings, mesh);
	Subdiv *subdiv = BKE_subdiv_new_from_converter(settings, &converter);
	BKE_subdiv_converter_free(&converter);
	return subdiv;
#else
	UNUSED_VARS(settings, mesh);
	return NULL;
#endif
}

void BKE_subdiv_free(Subdiv *subdiv)
{
#ifdef WITH_OPENSUBDIV
	if (subdiv->evaluator != NULL) {
		openSubdiv_deleteEvaluator(subdiv->evaluator);
	}
	if (subdiv->topology_refiner != NULL) {
		openSubdiv_deleteTopologyRefiner(subdiv->topology_refiner);
	}
	MEM_freeN(subdiv);
#else
	UNUSED_VARS(subdiv);
#endif
}
