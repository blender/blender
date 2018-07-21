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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_meshsequencecache.c
 *  \ingroup modifiers
 */

#include "DNA_cachefile_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cachefile.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

#ifdef WITH_ALEMBIC
#	include "ABC_alembic.h"
#	include "BKE_global.h"
#endif

static void initData(ModifierData *md)
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *)md;

	mcmd->cache_file = NULL;
	mcmd->object_path[0] = '\0';
	mcmd->read_flag = MOD_MESHSEQ_READ_ALL;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
	const MeshSeqCacheModifierData *mcmd = (const MeshSeqCacheModifierData *)md;
#endif
	MeshSeqCacheModifierData *tmcmd = (MeshSeqCacheModifierData *)target;

	modifier_copyData_generic(md, target, flag);

	tmcmd->reader = NULL;
}

static void freeData(ModifierData *md)
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	if (mcmd->reader) {
#ifdef WITH_ALEMBIC
		CacheReader_free(mcmd->reader);
#endif
		mcmd->reader = NULL;
	}
}

static bool isDisabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	/* leave it up to the modifier to check the file is valid on calculation */
	return (mcmd->cache_file == NULL) || (mcmd->object_path[0] == '\0');
}

static Mesh *applyModifier(
        ModifierData *md, const ModifierEvalContext *ctx,
        Mesh *mesh)
{
#ifdef WITH_ALEMBIC
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	/* Only used to check whether we are operating on org data or not... */
	Mesh *me = (ctx->object->type == OB_MESH) ? ctx->object->data : NULL;
	Mesh *org_mesh = mesh;

	Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	const float frame = DEG_get_ctime(ctx->depsgraph);
	const float time = BKE_cachefile_time_offset(mcmd->cache_file, frame, FPS);
	const char *err_str = NULL;

	CacheFile *cache_file = (CacheFile *)DEG_get_original_id(&mcmd->cache_file->id);

	BKE_cachefile_ensure_handle(G.main, cache_file);

	if (!mcmd->reader) {
		mcmd->reader = CacheReader_open_alembic_object(cache_file->handle,
		                                               NULL,
		                                               ctx->object,
		                                               mcmd->object_path);
		if (!mcmd->reader) {
			modifier_setError(md, "Could not create Alembic reader for file %s", cache_file->filepath);
			return mesh;
		}
	}

	if (me != NULL) {
		MVert *mvert = mesh->mvert;
		MEdge *medge = mesh->medge;
		MPoly *mpoly = mesh->mpoly;
		if ((me->mvert == mvert) || (me->medge == medge) || (me->mpoly == mpoly)) {
			/* We need to duplicate data here, otherwise we'll modify org mesh, see T51701. */
			BKE_id_copy_ex(NULL, &mesh->id, (ID **)&mesh,
			               LIB_ID_CREATE_NO_MAIN |
			               LIB_ID_CREATE_NO_USER_REFCOUNT |
			               LIB_ID_CREATE_NO_DEG_TAG |
			               LIB_ID_COPY_NO_PREVIEW,
			               false);
		}
	}

	Mesh *result = ABC_read_mesh(mcmd->reader,
	                             ctx->object,
	                             mesh,
	                             time,
	                             &err_str,
	                             mcmd->read_flag);

	if (err_str) {
		modifier_setError(md, "%s", err_str);
	}

	if (!ELEM(result, NULL, mesh) && (mesh != org_mesh)) {
		BKE_id_free(NULL, mesh);
		mesh = org_mesh;
	}

	return result ? result : mesh;
#else
	return mesh;
	UNUSED_VARS(ctx, md);
#endif
}

static bool dependsOnTime(ModifierData *md)
{
	UNUSED_VARS(md);
	return true;
}

static void foreachIDLink(
        ModifierData *md, Object *ob,
        IDWalkFunc walk, void *userData)
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	walk(userData, ob, (ID **)&mcmd->cache_file, IDWALK_CB_USER);
}


static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	if (mcmd->cache_file != NULL) {
		DEG_add_object_cache_relation(ctx->node, mcmd->cache_file, DEG_OB_COMP_CACHE, "Mesh Cache File");
	}
}

ModifierTypeInfo modifierType_MeshSequenceCache = {
	/* name */              "Mesh Sequence Cache",
	/* structName */        "MeshSeqCacheModifierData",
	/* structSize */        sizeof(MeshSeqCacheModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL,
};
