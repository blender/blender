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
 * The Original Code is Copyright (C) 2014 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/library_query.c
 *  \ingroup bke
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_group_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"
#include "DNA_node_types.h"
#include "DNA_object_force_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_screen_types.h"
#include "DNA_speaker_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_vfont_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_linklist_stack.h"

#include "BKE_animsys.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_fcurve.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_particle.h"
#include "BKE_rigidbody.h"
#include "BKE_sequencer.h"
#include "BKE_tracking.h"
#include "BKE_workspace.h"


#define FOREACH_FINALIZE _finalize
#define FOREACH_FINALIZE_VOID \
	if (0) { goto FOREACH_FINALIZE; } \
	FOREACH_FINALIZE: ((void)0)

#define FOREACH_CALLBACK_INVOKE_ID_PP(_data, id_pp, _cb_flag) \
	CHECK_TYPE(id_pp, ID **); \
	if (!((_data)->status & IDWALK_STOP)) { \
		const int _flag = (_data)->flag; \
		ID *old_id = *(id_pp); \
		const int callback_return = (_data)->callback((_data)->user_data, (_data)->self_id, id_pp, _cb_flag | (_data)->cb_flag); \
		if (_flag & IDWALK_READONLY) { \
			BLI_assert(*(id_pp) == old_id); \
		} \
		if (old_id && (_flag & IDWALK_RECURSE)) { \
			if (!BLI_gset_haskey((_data)->ids_handled, old_id)) { \
				BLI_gset_add((_data)->ids_handled, old_id); \
				if (!(callback_return & IDWALK_RET_STOP_RECURSION)) { \
					BLI_LINKSTACK_PUSH((_data)->ids_todo, old_id); \
				} \
			} \
		} \
		if (callback_return & IDWALK_RET_STOP_ITER) { \
			(_data)->status |= IDWALK_STOP; \
			goto FOREACH_FINALIZE; \
		} \
	} \
	else { \
		goto FOREACH_FINALIZE; \
	} ((void)0)

#define FOREACH_CALLBACK_INVOKE_ID(_data, id, cb_flag) \
	{ \
		CHECK_TYPE_ANY(id, ID *, void *); \
		FOREACH_CALLBACK_INVOKE_ID_PP(_data, (ID **)&(id), cb_flag); \
	} ((void)0)

#define FOREACH_CALLBACK_INVOKE(_data, id_super, cb_flag) \
	{ \
		CHECK_TYPE(&((id_super)->id), ID *); \
		FOREACH_CALLBACK_INVOKE_ID_PP(_data, (ID **)&(id_super), cb_flag); \
	} ((void)0)

/* status */
enum {
	IDWALK_STOP     = 1 << 0,
};

typedef struct LibraryForeachIDData {
	ID *self_id;
	int flag;
	int cb_flag;
	LibraryIDLinkCallback callback;
	void *user_data;
	int status;

	/* To handle recursion. */
	GSet *ids_handled;  /* All IDs that are either already done, or still in ids_todo stack. */
	BLI_LINKSTACK_DECLARE(ids_todo, ID *);
} LibraryForeachIDData;

static void library_foreach_idproperty_ID_link(LibraryForeachIDData *data, IDProperty *prop, int flag)
{
	if (!prop)
		return;

	switch (prop->type) {
		case IDP_GROUP:
		{
			for (IDProperty *loop = prop->data.group.first; loop; loop = loop->next) {
				library_foreach_idproperty_ID_link(data, loop, flag);
			}
			break;
		}
		case IDP_IDPARRAY:
		{
			IDProperty *loop = IDP_Array(prop);
			for (int i = 0; i < prop->len; i++) {
				library_foreach_idproperty_ID_link(data, &loop[i], flag);
			}
			break;
		}
		case IDP_ID:
			FOREACH_CALLBACK_INVOKE_ID(data, prop->data.pointer, flag);
			break;
		default:
			break;  /* Nothing to do here with other types of IDProperties... */
	}

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_rigidbodyworldSceneLooper(
        struct RigidBodyWorld *UNUSED(rbw), ID **id_pointer, void *user_data, int cb_flag)
{
	LibraryForeachIDData *data = (LibraryForeachIDData *) user_data;
	FOREACH_CALLBACK_INVOKE_ID_PP(data, id_pointer, cb_flag);

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_modifiersForeachIDLink(
        void *user_data, Object *UNUSED(object), ID **id_pointer, int cb_flag)
{
	LibraryForeachIDData *data = (LibraryForeachIDData *) user_data;
	FOREACH_CALLBACK_INVOKE_ID_PP(data, id_pointer, cb_flag);

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_constraintObjectLooper(bConstraint *UNUSED(con), ID **id_pointer,
                                                   bool is_reference, void *user_data)
{
	LibraryForeachIDData *data = (LibraryForeachIDData *) user_data;
	const int cb_flag = is_reference ? IDWALK_CB_USER : IDWALK_CB_NOP;
	FOREACH_CALLBACK_INVOKE_ID_PP(data, id_pointer, cb_flag);

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_particlesystemsObjectLooper(
        ParticleSystem *UNUSED(psys), ID **id_pointer, void *user_data, int cb_flag)
{
	LibraryForeachIDData *data = (LibraryForeachIDData *) user_data;
	FOREACH_CALLBACK_INVOKE_ID_PP(data, id_pointer, cb_flag);

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_nla_strip(LibraryForeachIDData *data, NlaStrip *strip)
{
	NlaStrip *substrip;

	FOREACH_CALLBACK_INVOKE(data, strip->act, IDWALK_CB_USER);

	for (substrip = strip->strips.first; substrip; substrip = substrip->next) {
		library_foreach_nla_strip(data, substrip);
	}

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_animationData(LibraryForeachIDData *data, AnimData *adt)
{
	FCurve *fcu;
	NlaTrack *nla_track;
	NlaStrip *nla_strip;

	for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
		ChannelDriver *driver = fcu->driver;
		DriverVar *dvar;

		for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
			/* only used targets */
			DRIVER_TARGETS_USED_LOOPER(dvar)
			{
				FOREACH_CALLBACK_INVOKE_ID(data, dtar->id, IDWALK_CB_NOP);
			}
			DRIVER_TARGETS_LOOPER_END
		}
	}

	FOREACH_CALLBACK_INVOKE(data, adt->action, IDWALK_CB_USER);
	FOREACH_CALLBACK_INVOKE(data, adt->tmpact, IDWALK_CB_USER);

	for (nla_track = adt->nla_tracks.first; nla_track; nla_track = nla_track->next) {
		for (nla_strip = nla_track->strips.first; nla_strip; nla_strip = nla_strip->next) {
			library_foreach_nla_strip(data, nla_strip);
		}
	}

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_mtex(LibraryForeachIDData *data, MTex *mtex)
{
	FOREACH_CALLBACK_INVOKE(data, mtex->object, IDWALK_CB_NOP);
	FOREACH_CALLBACK_INVOKE(data, mtex->tex, IDWALK_CB_USER);

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_paint(LibraryForeachIDData *data, Paint *paint)
{
	FOREACH_CALLBACK_INVOKE(data, paint->brush, IDWALK_CB_USER);
	FOREACH_CALLBACK_INVOKE(data, paint->palette, IDWALK_CB_USER);

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_bone(LibraryForeachIDData *data, Bone *bone)
{
	library_foreach_idproperty_ID_link(data, bone->prop, IDWALK_CB_USER);

	for (Bone *curbone = bone->childbase.first; curbone; curbone = curbone->next) {
		library_foreach_bone(data, curbone);
	}

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_layer_collection(LibraryForeachIDData *data, ListBase *lb)
{
	for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
		FOREACH_CALLBACK_INVOKE(data, lc->collection, IDWALK_CB_NOP);
		library_foreach_layer_collection(data, &lc->layer_collections);
	}

	FOREACH_FINALIZE_VOID;
}

static void library_foreach_ID_as_subdata_link(
        ID **id_pp, LibraryIDLinkCallback callback, void *user_data, int flag, LibraryForeachIDData *data)
{
	/* Needed e.g. for callbacks handling relationships... This call shall be absolutely readonly. */
	ID *id = *id_pp;
	FOREACH_CALLBACK_INVOKE_ID_PP(data, id_pp, IDWALK_CB_PRIVATE);
	BLI_assert(id == *id_pp);

	if (flag & IDWALK_RECURSE) {
		/* Defer handling into main loop, recursively calling BKE_library_foreach_ID_link in IDWALK_RECURSE case is
		 * troublesome, see T49553. */
		if (!BLI_gset_haskey(data->ids_handled, id)) {
			BLI_gset_add(data->ids_handled, id);
			BLI_LINKSTACK_PUSH(data->ids_todo, id);
		}
	}
	else {
		BKE_library_foreach_ID_link(NULL, id, callback, user_data, flag);
	}

	FOREACH_FINALIZE_VOID;
}

/**
 * Loop over all of the ID's this datablock links to.
 *
 * \note: May be extended to be recursive in the future.
 */
void BKE_library_foreach_ID_link(Main *bmain, ID *id, LibraryIDLinkCallback callback, void *user_data, int flag)
{
	LibraryForeachIDData data;
	int i;

	if (flag & IDWALK_RECURSE) {
		/* For now, recusion implies read-only. */
		flag |= IDWALK_READONLY;

		data.ids_handled = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
		BLI_LINKSTACK_INIT(data.ids_todo);

		BLI_gset_add(data.ids_handled, id);
	}
	else {
		data.ids_handled = NULL;
	}
	data.flag = flag;
	data.status = 0;
	data.callback = callback;
	data.user_data = user_data;

#define CALLBACK_INVOKE_ID(check_id, cb_flag) \
	FOREACH_CALLBACK_INVOKE_ID(&data, check_id, cb_flag)

#define CALLBACK_INVOKE(check_id_super, cb_flag) \
	FOREACH_CALLBACK_INVOKE(&data, check_id_super, cb_flag)

	for (; id != NULL; id = (flag & IDWALK_RECURSE) ? BLI_LINKSTACK_POP(data.ids_todo) : NULL) {
		data.self_id = id;
		data.cb_flag = ID_IS_LINKED(id) ? IDWALK_CB_INDIRECT_USAGE : 0;

		if (bmain != NULL && bmain->relations != NULL && (flag & IDWALK_READONLY)) {
			/* Note that this is minor optimization, even in worst cases (like id being an object with lots of
			 * drivers and constraints and modifiers, or material etc. with huge node tree),
			 * but we might as well use it (Main->relations is always assumed valid, it's responsibility of code
			 * creating it to free it, especially if/when it starts modifying Main database). */
			MainIDRelationsEntry *entry = BLI_ghash_lookup(bmain->relations->id_user_to_used, id);
			for (; entry != NULL; entry = entry->next) {
				FOREACH_CALLBACK_INVOKE_ID_PP(&data, entry->id_pointer, entry->usage_flag);
			}
			continue;
		}

		if (id->override_static != NULL) {
			CALLBACK_INVOKE_ID(id->override_static->reference, IDWALK_CB_USER | IDWALK_CB_STATIC_OVERRIDE_REFERENCE);
			CALLBACK_INVOKE_ID(id->override_static->storage, IDWALK_CB_USER | IDWALK_CB_STATIC_OVERRIDE_REFERENCE);
		}

		library_foreach_idproperty_ID_link(&data, id->properties, IDWALK_CB_USER);

		AnimData *adt = BKE_animdata_from_id(id);
		if (adt) {
			library_foreach_animationData(&data, adt);
		}

		switch ((ID_Type)GS(id->name)) {
			case ID_LI:
			{
				Library *lib = (Library *) id;
				CALLBACK_INVOKE(lib->parent, IDWALK_CB_NOP);
				break;
			}
			case ID_SCE:
			{
				Scene *scene = (Scene *) id;
				ToolSettings *toolsett = scene->toolsettings;

				CALLBACK_INVOKE(scene->camera, IDWALK_CB_NOP);
				CALLBACK_INVOKE(scene->world, IDWALK_CB_USER);
				CALLBACK_INVOKE(scene->set, IDWALK_CB_NOP);
				CALLBACK_INVOKE(scene->clip, IDWALK_CB_USER);
				if (scene->nodetree) {
					/* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
					library_foreach_ID_as_subdata_link((ID **)&scene->nodetree, callback, user_data, flag, &data);
				}
				if (scene->ed) {
					Sequence *seq;
					SEQP_BEGIN(scene->ed, seq)
					{
						CALLBACK_INVOKE(seq->scene, IDWALK_CB_NOP);
						CALLBACK_INVOKE(seq->scene_camera, IDWALK_CB_NOP);
						CALLBACK_INVOKE(seq->clip, IDWALK_CB_USER);
						CALLBACK_INVOKE(seq->mask, IDWALK_CB_USER);
						CALLBACK_INVOKE(seq->sound, IDWALK_CB_USER);
						library_foreach_idproperty_ID_link(&data, seq->prop, IDWALK_CB_USER);
						for (SequenceModifierData *smd = seq->modifiers.first; smd; smd = smd->next) {
							CALLBACK_INVOKE(smd->mask_id, IDWALK_CB_USER);
						}
					}
					SEQ_END
				}


				for (CollectionObject *cob = scene->master_collection->gobject.first; cob; cob = cob->next) {
					CALLBACK_INVOKE(cob->ob, IDWALK_CB_USER);
				}
				for (CollectionChild *child = scene->master_collection->children.first; child; child = child->next) {
					CALLBACK_INVOKE(child->collection, IDWALK_CB_USER);
				}

				ViewLayer *view_layer;
				for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
					for (Base *base = view_layer->object_bases.first; base; base = base->next) {
						CALLBACK_INVOKE(base->object, IDWALK_CB_NOP);
					}

					library_foreach_layer_collection(&data, &view_layer->layer_collections);

					for (FreestyleModuleConfig  *fmc = view_layer->freestyle_config.modules.first; fmc; fmc = fmc->next) {
						if (fmc->script) {
							CALLBACK_INVOKE(fmc->script, IDWALK_CB_NOP);
						}
					}

					for (FreestyleLineSet *fls = view_layer->freestyle_config.linesets.first; fls; fls = fls->next) {
						if (fls->group) {
							CALLBACK_INVOKE(fls->group, IDWALK_CB_USER);
						}

						if (fls->linestyle) {
							CALLBACK_INVOKE(fls->linestyle, IDWALK_CB_USER);
						}
					}
				}

				for (TimeMarker *marker = scene->markers.first; marker; marker = marker->next) {
					CALLBACK_INVOKE(marker->camera, IDWALK_CB_NOP);
				}

				if (toolsett) {
					CALLBACK_INVOKE(toolsett->particle.scene, IDWALK_CB_NOP);
					CALLBACK_INVOKE(toolsett->particle.object, IDWALK_CB_NOP);
					CALLBACK_INVOKE(toolsett->particle.shape_object, IDWALK_CB_NOP);

					library_foreach_paint(&data, &toolsett->imapaint.paint);
					CALLBACK_INVOKE(toolsett->imapaint.stencil, IDWALK_CB_USER);
					CALLBACK_INVOKE(toolsett->imapaint.clone, IDWALK_CB_USER);
					CALLBACK_INVOKE(toolsett->imapaint.canvas, IDWALK_CB_USER);

					if (toolsett->vpaint) {
						library_foreach_paint(&data, &toolsett->vpaint->paint);
					}
					if (toolsett->wpaint) {
						library_foreach_paint(&data, &toolsett->wpaint->paint);
					}
					if (toolsett->sculpt) {
						library_foreach_paint(&data, &toolsett->sculpt->paint);
						CALLBACK_INVOKE(toolsett->sculpt->gravity_object, IDWALK_CB_NOP);
					}
					if (toolsett->uvsculpt) {
						library_foreach_paint(&data, &toolsett->uvsculpt->paint);
					}
					if (toolsett->gp_paint) {
						library_foreach_paint(&data, &toolsett->gp_paint->paint);
					}
				}

				if (scene->rigidbody_world) {
					BKE_rigidbody_world_id_loop(scene->rigidbody_world, library_foreach_rigidbodyworldSceneLooper, &data);
				}

				break;
			}

			case ID_OB:
			{
				Object *object = (Object *) id;
				ParticleSystem *psys;

				/* Object is special, proxies make things hard... */
				const int data_cb_flag = data.cb_flag;
				const int proxy_cb_flag = ((data.flag & IDWALK_NO_INDIRECT_PROXY_DATA_USAGE) == 0 && (object->proxy || object->proxy_group)) ?
				                              IDWALK_CB_INDIRECT_USAGE : 0;

				/* object data special case */
				data.cb_flag |= proxy_cb_flag;
				if (object->type == OB_EMPTY) {
					/* empty can have NULL or Image */
					CALLBACK_INVOKE_ID(object->data, IDWALK_CB_USER);
				}
				else {
					/* when set, this can't be NULL */
					if (object->data) {
						CALLBACK_INVOKE_ID(object->data, IDWALK_CB_USER | IDWALK_CB_NEVER_NULL);
					}
				}
				data.cb_flag = data_cb_flag;

				CALLBACK_INVOKE(object->parent, IDWALK_CB_NOP);
				CALLBACK_INVOKE(object->track, IDWALK_CB_NOP);
				/* object->proxy is refcounted, but not object->proxy_group... *sigh* */
				CALLBACK_INVOKE(object->proxy, IDWALK_CB_USER);
				CALLBACK_INVOKE(object->proxy_group, IDWALK_CB_NOP);

				/* Special case!
				 * Since this field is set/owned by 'user' of this ID (and not ID itself), it is only indirect usage
				 * if proxy object is linked... Twisted. */
				if (object->proxy_from) {
					data.cb_flag = ID_IS_LINKED(object->proxy_from) ? IDWALK_CB_INDIRECT_USAGE : 0;
				}
				CALLBACK_INVOKE(object->proxy_from, IDWALK_CB_LOOPBACK);
				data.cb_flag = data_cb_flag;

				CALLBACK_INVOKE(object->poselib, IDWALK_CB_USER);

				data.cb_flag |= proxy_cb_flag;
				for (i = 0; i < object->totcol; i++) {
					CALLBACK_INVOKE(object->mat[i], IDWALK_CB_USER);
				}
				data.cb_flag = data_cb_flag;

				CALLBACK_INVOKE(object->gpd, IDWALK_CB_USER);
				CALLBACK_INVOKE(object->dup_group, IDWALK_CB_USER);

				if (object->pd) {
					CALLBACK_INVOKE(object->pd->tex, IDWALK_CB_USER);
					CALLBACK_INVOKE(object->pd->f_source, IDWALK_CB_NOP);
				}
				/* Note that ob->effect is deprecated, so no need to handle it here. */

				if (object->pose) {
					bPoseChannel *pchan;

					data.cb_flag |= proxy_cb_flag;
					for (pchan = object->pose->chanbase.first; pchan; pchan = pchan->next) {
						library_foreach_idproperty_ID_link(&data, pchan->prop, IDWALK_CB_USER);
						CALLBACK_INVOKE(pchan->custom, IDWALK_CB_USER);
						BKE_constraints_id_loop(&pchan->constraints, library_foreach_constraintObjectLooper, &data);
					}
					data.cb_flag = data_cb_flag;
				}

				if (object->rigidbody_constraint) {
					CALLBACK_INVOKE(object->rigidbody_constraint->ob1, IDWALK_CB_NOP);
					CALLBACK_INVOKE(object->rigidbody_constraint->ob2, IDWALK_CB_NOP);
				}

				if (object->lodlevels.first) {
					LodLevel *level;
					for (level = object->lodlevels.first; level; level = level->next) {
						CALLBACK_INVOKE(level->source, IDWALK_CB_NOP);
					}
				}

				modifiers_foreachIDLink(object, library_foreach_modifiersForeachIDLink, &data);
				BKE_constraints_id_loop(&object->constraints, library_foreach_constraintObjectLooper, &data);

				for (psys = object->particlesystem.first; psys; psys = psys->next) {
					BKE_particlesystem_id_loop(psys, library_foreach_particlesystemsObjectLooper, &data);
				}

				if (object->soft) {
					CALLBACK_INVOKE(object->soft->collision_group, IDWALK_CB_NOP);

					if (object->soft->effector_weights) {
						CALLBACK_INVOKE(object->soft->effector_weights->group, IDWALK_CB_NOP);
					}
				}
				break;
			}

			case ID_AR:
			{
				bArmature *arm = (bArmature *)id;

				for (Bone *bone = arm->bonebase.first; bone; bone = bone->next) {
					library_foreach_bone(&data, bone);
				}
				break;
			}

			case ID_ME:
			{
				Mesh *mesh = (Mesh *) id;
				CALLBACK_INVOKE(mesh->texcomesh, IDWALK_CB_USER);
				CALLBACK_INVOKE(mesh->key, IDWALK_CB_USER);
				for (i = 0; i < mesh->totcol; i++) {
					CALLBACK_INVOKE(mesh->mat[i], IDWALK_CB_USER);
				}
				break;
			}

			case ID_CU:
			{
				Curve *curve = (Curve *) id;
				CALLBACK_INVOKE(curve->bevobj, IDWALK_CB_NOP);
				CALLBACK_INVOKE(curve->taperobj, IDWALK_CB_NOP);
				CALLBACK_INVOKE(curve->textoncurve, IDWALK_CB_NOP);
				CALLBACK_INVOKE(curve->key, IDWALK_CB_USER);
				for (i = 0; i < curve->totcol; i++) {
					CALLBACK_INVOKE(curve->mat[i], IDWALK_CB_USER);
				}
				CALLBACK_INVOKE(curve->vfont, IDWALK_CB_USER);
				CALLBACK_INVOKE(curve->vfontb, IDWALK_CB_USER);
				CALLBACK_INVOKE(curve->vfonti, IDWALK_CB_USER);
				CALLBACK_INVOKE(curve->vfontbi, IDWALK_CB_USER);
				break;
			}

			case ID_MB:
			{
				MetaBall *metaball = (MetaBall *) id;
				for (i = 0; i < metaball->totcol; i++) {
					CALLBACK_INVOKE(metaball->mat[i], IDWALK_CB_USER);
				}
				break;
			}

			case ID_MA:
			{
				Material *material = (Material *) id;
				if (material->nodetree) {
					/* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
					library_foreach_ID_as_subdata_link((ID **)&material->nodetree, callback, user_data, flag, &data);
				}
				if (material->texpaintslot != NULL) {
					CALLBACK_INVOKE(material->texpaintslot->ima, IDWALK_CB_NOP);
				}
				if (material->gp_style != NULL) {
					CALLBACK_INVOKE(material->gp_style->sima, IDWALK_CB_USER);
					CALLBACK_INVOKE(material->gp_style->ima, IDWALK_CB_USER);
				}
				break;
			}

			case ID_TE:
			{
				Tex *texture = (Tex *) id;
				if (texture->nodetree) {
					/* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
					library_foreach_ID_as_subdata_link((ID **)&texture->nodetree, callback, user_data, flag, &data);
				}
				CALLBACK_INVOKE(texture->ima, IDWALK_CB_USER);
				break;
			}

			case ID_LT:
			{
				Lattice *lattice = (Lattice *) id;
				CALLBACK_INVOKE(lattice->key, IDWALK_CB_USER);
				break;
			}

			case ID_LA:
			{
				Lamp *lamp = (Lamp *) id;
				if (lamp->nodetree) {
					/* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
					library_foreach_ID_as_subdata_link((ID **)&lamp->nodetree, callback, user_data, flag, &data);
				}
				break;
			}

			case ID_CA:
			{
				Camera *camera = (Camera *) id;
				CALLBACK_INVOKE(camera->dof_ob, IDWALK_CB_NOP);
				for (CameraBGImage *bgpic = camera->bg_images.first; bgpic; bgpic = bgpic->next) {
					if (bgpic->source == CAM_BGIMG_SOURCE_IMAGE) {
						CALLBACK_INVOKE(bgpic->ima, IDWALK_CB_USER);
					}
					else if (bgpic->source == CAM_BGIMG_SOURCE_MOVIE) {
						CALLBACK_INVOKE(bgpic->clip, IDWALK_CB_USER);
					}
				}

				break;
			}

			case ID_KE:
			{
				Key *key = (Key *) id;
				CALLBACK_INVOKE_ID(key->from, IDWALK_CB_LOOPBACK);
				break;
			}

			case ID_WO:
			{
				World *world = (World *) id;
				if (world->nodetree) {
					/* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
					library_foreach_ID_as_subdata_link((ID **)&world->nodetree, callback, user_data, flag, &data);
				}
				break;
			}

			case ID_SPK:
			{
				Speaker *speaker = (Speaker *) id;
				CALLBACK_INVOKE(speaker->sound, IDWALK_CB_USER);
				break;
			}

			case ID_LP:
			{
				LightProbe *probe = (LightProbe *) id;
				CALLBACK_INVOKE(probe->image, IDWALK_CB_USER);
				CALLBACK_INVOKE(probe->visibility_grp, IDWALK_CB_NOP);
				break;
			}

			case ID_GR:
			{
				Collection *collection = (Collection *) id;
				for (CollectionObject *cob = collection->gobject.first; cob; cob = cob->next) {
					CALLBACK_INVOKE(cob->ob, IDWALK_CB_USER);
				}
				for (CollectionChild *child = collection->children.first; child; child = child->next) {
					CALLBACK_INVOKE(child->collection, IDWALK_CB_USER);
				}
				break;
			}

			case ID_NT:
			{
				bNodeTree *ntree = (bNodeTree *) id;
				bNode *node;
				bNodeSocket *sock;

				CALLBACK_INVOKE(ntree->gpd, IDWALK_CB_USER);

				for (node = ntree->nodes.first; node; node = node->next) {
					CALLBACK_INVOKE_ID(node->id, IDWALK_CB_USER);

					library_foreach_idproperty_ID_link(&data, node->prop, IDWALK_CB_USER);
					for (sock = node->inputs.first; sock; sock = sock->next) {
						library_foreach_idproperty_ID_link(&data, sock->prop, IDWALK_CB_USER);
					}
					for (sock = node->outputs.first; sock; sock = sock->next) {
						library_foreach_idproperty_ID_link(&data, sock->prop, IDWALK_CB_USER);
					}
				}

				for (sock = ntree->inputs.first; sock; sock = sock->next) {
					library_foreach_idproperty_ID_link(&data, sock->prop, IDWALK_CB_USER);
				}
				for (sock = ntree->outputs.first; sock; sock = sock->next) {
					library_foreach_idproperty_ID_link(&data, sock->prop, IDWALK_CB_USER);
				}
				break;
			}

			case ID_BR:
			{
				Brush *brush = (Brush *) id;
				CALLBACK_INVOKE(brush->toggle_brush, IDWALK_CB_NOP);
				CALLBACK_INVOKE(brush->clone.image, IDWALK_CB_NOP);
				CALLBACK_INVOKE(brush->paint_curve, IDWALK_CB_USER);
				if (brush->gpencil_settings) {
					CALLBACK_INVOKE(brush->gpencil_settings->material, IDWALK_CB_USER);
				}
				library_foreach_mtex(&data, &brush->mtex);
				library_foreach_mtex(&data, &brush->mask_mtex);
				break;
			}

			case ID_PA:
			{
				ParticleSettings *psett = (ParticleSettings *) id;
				CALLBACK_INVOKE(psett->dup_group, IDWALK_CB_NOP);
				CALLBACK_INVOKE(psett->dup_ob, IDWALK_CB_NOP);
				CALLBACK_INVOKE(psett->bb_ob, IDWALK_CB_NOP);
				CALLBACK_INVOKE(psett->collision_group, IDWALK_CB_NOP);

				for (i = 0; i < MAX_MTEX; i++) {
					if (psett->mtex[i]) {
						library_foreach_mtex(&data, psett->mtex[i]);
					}
				}

				if (psett->effector_weights) {
					CALLBACK_INVOKE(psett->effector_weights->group, IDWALK_CB_NOP);
				}

				if (psett->pd) {
					CALLBACK_INVOKE(psett->pd->tex, IDWALK_CB_USER);
					CALLBACK_INVOKE(psett->pd->f_source, IDWALK_CB_NOP);
				}
				if (psett->pd2) {
					CALLBACK_INVOKE(psett->pd2->tex, IDWALK_CB_USER);
					CALLBACK_INVOKE(psett->pd2->f_source, IDWALK_CB_NOP);
				}

				if (psett->boids) {
					BoidState *state;
					BoidRule *rule;

					for (state = psett->boids->states.first; state; state = state->next) {
						for (rule = state->rules.first; rule; rule = rule->next) {
							if (rule->type == eBoidRuleType_Avoid) {
								BoidRuleGoalAvoid *gabr = (BoidRuleGoalAvoid *)rule;
								CALLBACK_INVOKE(gabr->ob, IDWALK_CB_NOP);
							}
							else if (rule->type == eBoidRuleType_FollowLeader) {
								BoidRuleFollowLeader *flbr = (BoidRuleFollowLeader *)rule;
								CALLBACK_INVOKE(flbr->ob, IDWALK_CB_NOP);
							}
						}
					}
				}

				for (ParticleDupliWeight *dw = psett->dupliweights.first; dw; dw = dw->next) {
					CALLBACK_INVOKE(dw->ob, IDWALK_CB_NOP);
				}
				break;
			}

			case ID_MC:
			{
				MovieClip *clip = (MovieClip *) id;
				MovieTracking *tracking = &clip->tracking;
				MovieTrackingObject *object;
				MovieTrackingTrack *track;
				MovieTrackingPlaneTrack *plane_track;

				CALLBACK_INVOKE(clip->gpd, IDWALK_CB_USER);

				for (track = tracking->tracks.first; track; track = track->next) {
					CALLBACK_INVOKE(track->gpd, IDWALK_CB_USER);
				}
				for (object = tracking->objects.first; object; object = object->next) {
					for (track = object->tracks.first; track; track = track->next) {
						CALLBACK_INVOKE(track->gpd, IDWALK_CB_USER);
					}
				}

				for (plane_track = tracking->plane_tracks.first; plane_track; plane_track = plane_track->next) {
					CALLBACK_INVOKE(plane_track->image, IDWALK_CB_USER);
				}
				break;
			}

			case ID_MSK:
			{
				Mask *mask = (Mask *) id;
				MaskLayer *mask_layer;
				for (mask_layer = mask->masklayers.first; mask_layer; mask_layer = mask_layer->next) {
					MaskSpline *mask_spline;

					for (mask_spline = mask_layer->splines.first; mask_spline; mask_spline = mask_spline->next) {
						for (i = 0; i < mask_spline->tot_point; i++) {
							MaskSplinePoint *point = &mask_spline->points[i];
							CALLBACK_INVOKE_ID(point->parent.id, IDWALK_CB_USER);
						}
					}
				}
				break;
			}

			case ID_LS:
			{
				FreestyleLineStyle *linestyle = (FreestyleLineStyle *) id;
				LineStyleModifier *lsm;
				for (i = 0; i < MAX_MTEX; i++) {
					if (linestyle->mtex[i]) {
						library_foreach_mtex(&data, linestyle->mtex[i]);
					}
				}
				if (linestyle->nodetree) {
					/* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
					library_foreach_ID_as_subdata_link((ID **)&linestyle->nodetree, callback, user_data, flag, &data);
				}

				for (lsm = linestyle->color_modifiers.first; lsm; lsm = lsm->next) {
					if (lsm->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
						LineStyleColorModifier_DistanceFromObject *p = (LineStyleColorModifier_DistanceFromObject *)lsm;
						if (p->target) {
							CALLBACK_INVOKE(p->target, IDWALK_CB_NOP);
						}
					}
				}
				for (lsm = linestyle->alpha_modifiers.first; lsm; lsm = lsm->next) {
					if (lsm->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
						LineStyleAlphaModifier_DistanceFromObject *p = (LineStyleAlphaModifier_DistanceFromObject *)lsm;
						if (p->target) {
							CALLBACK_INVOKE(p->target, IDWALK_CB_NOP);
						}
					}
				}
				for (lsm = linestyle->thickness_modifiers.first; lsm; lsm = lsm->next) {
					if (lsm->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
						LineStyleThicknessModifier_DistanceFromObject *p = (LineStyleThicknessModifier_DistanceFromObject *)lsm;
						if (p->target) {
							CALLBACK_INVOKE(p->target, IDWALK_CB_NOP);
						}
					}
				}
				break;
			}
			case ID_AC:
			{
				bAction *act = (bAction *) id;

				for (TimeMarker *marker = act->markers.first; marker; marker = marker->next) {
					CALLBACK_INVOKE(marker->camera, IDWALK_CB_NOP);
				}
				break;
			}

			case ID_WM:
			{
				wmWindowManager *wm = (wmWindowManager *)id;

				for (wmWindow *win = wm->windows.first; win; win = win->next) {
					ID *workspace = (ID *)BKE_workspace_active_get(win->workspace_hook);

					CALLBACK_INVOKE(win->scene, IDWALK_CB_USER_ONE);

					CALLBACK_INVOKE_ID(workspace, IDWALK_CB_NOP);
					/* allow callback to set a different workspace */
					BKE_workspace_active_set(win->workspace_hook, (WorkSpace *)workspace);
				}
				break;
			}

			case ID_WS:
			{
				WorkSpace *workspace = (WorkSpace *)id;
				ListBase *layouts = BKE_workspace_layouts_get(workspace);

				for (WorkSpaceLayout *layout = layouts->first; layout; layout = layout->next) {
					bScreen *screen = BKE_workspace_layout_screen_get(layout);

					/* CALLBACK_INVOKE expects an actual pointer, not a variable holding the pointer.
					 * However we can't acess layout->screen here since we are outside the workspace project. */
					CALLBACK_INVOKE(screen, IDWALK_CB_NOP);
					/* allow callback to set a different screen */
					BKE_workspace_layout_screen_set(layout, screen);
				}
				break;
			}
			case ID_GD:
			{
				bGPdata *gpencil = (bGPdata *) id;
				/* materials */
				for (i = 0; i < gpencil->totcol; i++) {
					CALLBACK_INVOKE(gpencil->mat[i], IDWALK_CB_USER);
				}

				for (bGPDlayer *gplayer = gpencil->layers.first; gplayer != NULL; gplayer = gplayer->next) {
					CALLBACK_INVOKE(gplayer->parent, IDWALK_CB_NOP);
				}

				break;
			}

			/* Nothing needed for those... */
			case ID_SCR:
			case ID_IM:
			case ID_VF:
			case ID_TXT:
			case ID_SO:
			case ID_PAL:
			case ID_PC:
			case ID_CF:
				break;

			/* Deprecated. */
			case ID_IP:
				break;

		}
	}

FOREACH_FINALIZE:
	if (data.ids_handled) {
		BLI_gset_free(data.ids_handled, NULL);
		BLI_LINKSTACK_FREE(data.ids_todo);
	}

#undef CALLBACK_INVOKE_ID
#undef CALLBACK_INVOKE
}

#undef FOREACH_CALLBACK_INVOKE_ID
#undef FOREACH_CALLBACK_INVOKE

/**
 * re-usable function, use when replacing ID's
 */
void BKE_library_update_ID_link_user(ID *id_dst, ID *id_src, const int cb_flag)
{
	if (cb_flag & IDWALK_CB_USER) {
		id_us_min(id_src);
		id_us_plus(id_dst);
	}
	else if (cb_flag & IDWALK_CB_USER_ONE) {
		id_us_ensure_real(id_dst);
	}
}

/**
 * Say whether given \a id_type_owner can use (in any way) a datablock of \a id_type_used.
 *
 * This is a 'simplified' abstract version of #BKE_library_foreach_ID_link() above, quite useful to reduce
 * useless iterations in some cases.
 */
/* XXX This has to be fully rethink, basing check on ID type is not really working anymore (and even worth once
 *     IDProps will support ID pointers), we'll have to do some quick checks on IDs themselves... */
bool BKE_library_id_can_use_idtype(ID *id_owner, const short id_type_used)
{
	/* any type of ID can be used in custom props. */
	if (id_owner->properties) {
		return true;
	}

	const short id_type_owner = GS(id_owner->name);

	/* IDProps of armature bones and nodes, and bNode->id can use virtually any type of ID. */
	if (ELEM(id_type_owner, ID_NT, ID_AR)) {
		return true;
	}

	if (ntreeFromID(id_owner)) {
		return true;
	}

	if (BKE_animdata_from_id(id_owner)) {
		return true;  /* AnimationData can use virtually any kind of datablocks, through drivers especially. */
	}

	switch ((ID_Type)id_type_owner) {
		case ID_LI:
			return ELEM(id_type_used, ID_LI);
		case ID_SCE:
			return (ELEM(id_type_used, ID_OB, ID_WO, ID_SCE, ID_MC, ID_MA, ID_GR, ID_TXT,
			                           ID_LS, ID_MSK, ID_SO, ID_GD, ID_BR, ID_PAL, ID_IM, ID_NT));
		case ID_OB:
			/* Could be the following, but simpler to just always say 'yes' here. */
#if 0
			return ELEM(id_type_used, ID_ME, ID_CU, ID_MB, ID_LT, ID_SPK, ID_AR, ID_LA, ID_CA,  /* obdata */
			                          ID_OB, ID_MA, ID_GD, ID_GR, ID_TE, ID_PA, ID_TXT, ID_SO, ID_MC, ID_IM, ID_AC
			                          /* + constraints and modifiers ... */);
#else
			return true;
#endif
		case ID_ME:
			return ELEM(id_type_used, ID_ME, ID_KE, ID_MA, ID_IM);
		case ID_CU:
			return ELEM(id_type_used, ID_OB, ID_KE, ID_MA, ID_VF);
		case ID_MB:
			return ELEM(id_type_used, ID_MA);
		case ID_MA:
			return (ELEM(id_type_used, ID_TE, ID_GR));
		case ID_TE:
			return (ELEM(id_type_used, ID_IM, ID_OB));
		case ID_LT:
			return ELEM(id_type_used, ID_KE);
		case ID_LA:
			return (ELEM(id_type_used, ID_TE));
		case ID_CA:
			return ELEM(id_type_used, ID_OB);
		case ID_KE:
			return ELEM(id_type_used, ID_ME, ID_CU, ID_LT);  /* Warning! key->from, could be more types in future? */
		case ID_SCR:
			return ELEM(id_type_used, ID_SCE);
		case ID_WO:
			return (ELEM(id_type_used, ID_TE));
		case ID_SPK:
			return ELEM(id_type_used, ID_SO);
		case ID_GR:
			return ELEM(id_type_used, ID_OB, ID_GR);
		case ID_NT:
			/* Could be the following, but node.id has no type restriction... */
#if 0
			return ELEM(id_type_used, ID_GD /* + node.id types... */);
#else
			return true;
#endif
		case ID_BR:
			return ELEM(id_type_used, ID_BR, ID_IM, ID_PC, ID_TE, ID_MA);
		case ID_PA:
			return ELEM(id_type_used, ID_OB, ID_GR, ID_TE);
		case ID_MC:
			return ELEM(id_type_used, ID_GD, ID_IM);
		case ID_MSK:
			return ELEM(id_type_used, ID_MC);  /* WARNING! mask->parent.id, not typed. */
		case ID_LS:
			return (ELEM(id_type_used, ID_TE, ID_OB));
		case ID_LP:
			return ELEM(id_type_used, ID_IM);
		case ID_GD:
			return ELEM(id_type_used, ID_MA);
		case ID_WS:
			return ELEM(id_type_used, ID_SCR, ID_SCE);
		case ID_IM:
		case ID_VF:
		case ID_TXT:
		case ID_SO:
		case ID_AR:
		case ID_AC:
		case ID_WM:
		case ID_PAL:
		case ID_PC:
		case ID_CF:
			/* Those types never use/reference other IDs... */
			return false;
		case ID_IP:
			/* Deprecated... */
			return false;
	}
	return false;
}


/* ***** ID users iterator. ***** */
typedef struct IDUsersIter {
	ID *id;

	ListBase *lb_array[MAX_LIBARRAY];
	int lb_idx;

	ID *curr_id;
	int count_direct, count_indirect;  /* Set by callback. */
} IDUsersIter;

static int foreach_libblock_id_users_callback(void *user_data, ID *UNUSED(self_id), ID **id_p, int cb_flag)
{
	IDUsersIter *iter = user_data;

	if (*id_p) {
		/* 'Loopback' ID pointers (the ugly 'from' ones, Object->proxy_from and Key->from).
		 * Those are not actually ID usage, we can ignore them here.
		 */
		if (cb_flag & IDWALK_CB_LOOPBACK) {
			return IDWALK_RET_NOP;
		}

		if (*id_p == iter->id) {
#if 0
			printf("%s uses %s (refcounted: %d, userone: %d, used_one: %d, used_one_active: %d, indirect_usage: %d)\n",
				   iter->curr_id->name, iter->id->name, (cb_flag & IDWALK_USER) ? 1 : 0, (cb_flag & IDWALK_USER_ONE) ? 1 : 0,
				   (iter->id->tag & LIB_TAG_EXTRAUSER) ? 1 : 0, (iter->id->tag & LIB_TAG_EXTRAUSER_SET) ? 1 : 0,
				   (cb_flag & IDWALK_INDIRECT_USAGE) ? 1 : 0);
#endif
			if (cb_flag & IDWALK_CB_INDIRECT_USAGE) {
				iter->count_indirect++;
			}
			else {
				iter->count_direct++;
			}
		}
	}

	return IDWALK_RET_NOP;
}

/**
 * Return the number of times given \a id_user uses/references \a id_used.
 *
 * \note This only checks for pointer references of an ID, shallow usages (like e.g. by RNA paths, as done
 *       for FCurves) are not detected at all.
 *
 * \param id_user the ID which is supposed to use (reference) \a id_used.
 * \param id_used the ID which is supposed to be used (referenced) by \a id_user.
 * \return the number of direct usages/references of \a id_used by \a id_user.
 */
int BKE_library_ID_use_ID(ID *id_user, ID *id_used)
{
	IDUsersIter iter;

	/* We do not care about iter.lb_array/lb_idx here... */
	iter.id = id_used;
	iter.curr_id = id_user;
	iter.count_direct = iter.count_indirect = 0;

	BKE_library_foreach_ID_link(NULL, iter.curr_id, foreach_libblock_id_users_callback, (void *)&iter, IDWALK_READONLY);

	return iter.count_direct + iter.count_indirect;
}

static bool library_ID_is_used(Main *bmain, void *idv, const bool check_linked)
{
	IDUsersIter iter;
	ListBase *lb_array[MAX_LIBARRAY];
	ID *id = idv;
	int i = set_listbasepointers(bmain, lb_array);
	bool is_defined = false;

	iter.id = id;
	iter.count_direct = iter.count_indirect = 0;
	while (i-- && !is_defined) {
		ID *id_curr = lb_array[i]->first;

		if (!id_curr || !BKE_library_id_can_use_idtype(id_curr, GS(id->name))) {
			continue;
		}

		for (; id_curr && !is_defined; id_curr = id_curr->next) {
			if (id_curr == id) {
				/* We are not interested in self-usages (mostly from drivers or bone constraints...). */
				continue;
			}
			iter.curr_id = id_curr;
			BKE_library_foreach_ID_link(
			            bmain, id_curr, foreach_libblock_id_users_callback, &iter, IDWALK_READONLY);

			is_defined = ((check_linked ? iter.count_indirect : iter.count_direct) != 0);
		}
	}

	return is_defined;
}

/**
 * Check whether given ID is used locally (i.e. by another non-linked ID).
 */
bool BKE_library_ID_is_locally_used(Main *bmain, void *idv)
{
	return library_ID_is_used(bmain, idv, false);
}

/**
 * Check whether given ID is used indirectly (i.e. by another linked ID).
 */
bool BKE_library_ID_is_indirectly_used(Main *bmain, void *idv)
{
	return library_ID_is_used(bmain, idv, true);
}

/**
 * Combine \a BKE_library_ID_is_locally_used() and \a BKE_library_ID_is_indirectly_used() in a single call.
 */
void BKE_library_ID_test_usages(Main *bmain, void *idv, bool *is_used_local, bool *is_used_linked)
{
	IDUsersIter iter;
	ListBase *lb_array[MAX_LIBARRAY];
	ID *id = idv;
	int i = set_listbasepointers(bmain, lb_array);
	bool is_defined = false;

	iter.id = id;
	iter.count_direct = iter.count_indirect = 0;
	while (i-- && !is_defined) {
		ID *id_curr = lb_array[i]->first;

		if (!id_curr || !BKE_library_id_can_use_idtype(id_curr, GS(id->name))) {
			continue;
		}

		for (; id_curr && !is_defined; id_curr = id_curr->next) {
			if (id_curr == id) {
				/* We are not interested in self-usages (mostly from drivers or bone constraints...). */
				continue;
			}
			iter.curr_id = id_curr;
			BKE_library_foreach_ID_link(bmain, id_curr, foreach_libblock_id_users_callback, &iter, IDWALK_READONLY);

			is_defined = (iter.count_direct != 0 && iter.count_indirect != 0);
		}
	}

	*is_used_local = (iter.count_direct != 0);
	*is_used_linked = (iter.count_indirect != 0);
}

/* ***** IDs usages.checking/tagging. ***** */
static int foreach_libblock_used_linked_data_tag_clear_cb(
        void *user_data, ID *self_id, ID **id_p, int UNUSED(cb_flag))
{
	bool *is_changed = user_data;

	if (*id_p) {
		/* XXX This is actually some kind of hack...
		 * Issue is, shapekeys' 'from' ID pointer is not actually ID usage.
		 * Maybe we should even nuke it from BKE_library_foreach_ID_link, not 100% sure yet...
		 */
		if ((GS(self_id->name) == ID_KE) && (((Key *)self_id)->from == *id_p)) {
			return IDWALK_RET_NOP;
		}
		/* XXX another hack, for similar reasons as above one. */
		if ((GS(self_id->name) == ID_OB) && (((Object *)self_id)->proxy_from == (Object *)*id_p)) {
			return IDWALK_RET_NOP;
		}

		/* If checked id is used by an assumed used ID, then it is also used and not part of any linked archipelago. */
		if (!(self_id->tag & LIB_TAG_DOIT) && ((*id_p)->tag & LIB_TAG_DOIT)) {
			(*id_p)->tag &= ~LIB_TAG_DOIT;
			*is_changed = true;
		}
	}

	return IDWALK_RET_NOP;
}

/**
 * Detect orphaned linked data blocks (i.e. linked data not used (directly or indirectly) in any way by any local data),
 * including complex cases like 'linked archipelagoes', i.e. linked datablocks that use each other in loops,
 * which prevents their deletion by 'basic' usage checks...
 *
 * \param do_init_tag if \a true, all linked data are checked, if \a false, only linked datablocks already tagged with
 *                    LIB_TAG_DOIT are checked.
 */
void BKE_library_unused_linked_data_set_tag(Main *bmain, const bool do_init_tag)
{
	ListBase *lb_array[MAX_LIBARRAY];

	if (do_init_tag) {
		int i = set_listbasepointers(bmain, lb_array);

		while (i--) {
			for (ID *id = lb_array[i]->first; id; id = id->next) {
				if (id->lib && (id->tag & LIB_TAG_INDIRECT) != 0) {
					id->tag |= LIB_TAG_DOIT;
				}
				else {
					id->tag &= ~LIB_TAG_DOIT;
				}
			}
		}
	}

	bool do_loop = true;
	while (do_loop) {
		int i = set_listbasepointers(bmain, lb_array);
		do_loop = false;

		while (i--) {
			for (ID *id = lb_array[i]->first; id; id = id->next) {
				if (id->tag & LIB_TAG_DOIT) {
					/* Unused ID (so far), no need to check it further. */
					continue;
				}
				BKE_library_foreach_ID_link(
				            bmain, id, foreach_libblock_used_linked_data_tag_clear_cb, &do_loop, IDWALK_READONLY);
			}
		}
	}
}

/**
 * Untag linked data blocks used by other untagged linked datablocks.
 * Used to detect datablocks that we can forcefully make local (instead of copying them to later get rid of original):
 * All datablocks we want to make local are tagged by caller, after this function has ran caller knows datablocks still
 * tagged can directly be made local, since they are only used by other datablocks that will also be made fully local.
 */
void BKE_library_indirectly_used_data_tag_clear(Main *bmain)
{
	ListBase *lb_array[MAX_LIBARRAY];

	bool do_loop = true;
	while (do_loop) {
		int i = set_listbasepointers(bmain, lb_array);
		do_loop = false;

		while (i--) {
			for (ID *id = lb_array[i]->first; id; id = id->next) {
				if (id->lib == NULL || id->tag & LIB_TAG_DOIT) {
					/* Local or non-indirectly-used ID (so far), no need to check it further. */
					continue;
				}
				BKE_library_foreach_ID_link(
				            bmain, id, foreach_libblock_used_linked_data_tag_clear_cb, &do_loop, IDWALK_READONLY);
			}
		}
	}
}
