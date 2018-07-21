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

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "subdiv_converter.h"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_capi.h"
#  include "opensubdiv_converter_capi.h"
#  include "opensubdiv_evaluator_capi.h"
#  include "opensubdiv_topology_refiner_capi.h"
#endif

#ifdef WITH_OPENSUBDIV
static void update_subdiv_after_topology_change(Subdiv *subdiv)
{
	/* Count ptex faces. */
	subdiv->num_ptex_faces = subdiv->topology_refiner->getNumPtexFaces(
	        subdiv->topology_refiner);
	/* Initialize offset of base faces in ptex indices. */
	MEM_SAFE_FREE(subdiv->face_ptex_offset);
	subdiv->face_ptex_offset = MEM_malloc_arrayN(subdiv->num_ptex_faces,
	                                             sizeof(int),
	                                             "subdiv ptex offset");
	subdiv->topology_refiner->fillFacePtexIndexOffset(
	        subdiv->topology_refiner,
	        subdiv->face_ptex_offset);
}
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
	struct OpenSubdiv_TopologyRefiner *osd_topology_refiner =
	        openSubdiv_createTopologyRefinerFromConverter(
	                converter, &topology_refiner_settings);
	if (osd_topology_refiner == NULL) {
		return NULL;
	}
	Subdiv *subdiv = MEM_callocN(sizeof(Subdiv), "subdiv from converetr");
	subdiv->settings = *settings;
	subdiv->topology_refiner = osd_topology_refiner;
	subdiv->evaluator = NULL;
	update_subdiv_after_topology_change(subdiv);
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
	MEM_SAFE_FREE(subdiv->face_ptex_offset);
	MEM_freeN(subdiv);
#endif
}
