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
 * Contributor(s): Esteban Tovagliari, Cedric Paille, Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../ABC_alembic.h"

#include <Alembic/AbcMaterial/IMaterial.h>

#include "abc_archive.h"
#include "abc_camera.h"
#include "abc_curves.h"
#include "abc_hair.h"
#include "abc_mesh.h"
#include "abc_nurbs.h"
#include "abc_points.h"
#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_cachefile_types.h"
#include "DNA_curve_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cachefile.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_scene.h"

/* SpaceType struct has a member called 'new' which obviously conflicts with C++
 * so temporarily redefining the new keyword to make it compile. */
#define new extern_new
#include "BKE_screen.h"
#undef new

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "WM_api.h"
#include "WM_types.h"
}

using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::ObjectHeader;

using Alembic::AbcGeom::MetaData;
using Alembic::AbcGeom::P3fArraySamplePtr;
using Alembic::AbcGeom::kWrapExisting;

using Alembic::AbcGeom::ICamera;
using Alembic::AbcGeom::ICurves;
using Alembic::AbcGeom::ICurvesSchema;
using Alembic::AbcGeom::IFaceSet;
using Alembic::AbcGeom::ILight;
using Alembic::AbcGeom::INuPatch;
using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IPoints;
using Alembic::AbcGeom::IPointsSchema;
using Alembic::AbcGeom::IPolyMesh;
using Alembic::AbcGeom::IPolyMeshSchema;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::ISubD;
using Alembic::AbcGeom::IV2fGeomParam;
using Alembic::AbcGeom::IXform;
using Alembic::AbcGeom::IXformSchema;
using Alembic::AbcGeom::N3fArraySamplePtr;
using Alembic::AbcGeom::XformSample;
using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::IN3fArrayProperty;
using Alembic::AbcGeom::IN3fGeomParam;
using Alembic::AbcGeom::V3fArraySamplePtr;

using Alembic::AbcMaterial::IMaterial;

struct AbcArchiveHandle {
	int unused;
};

ABC_INLINE ArchiveReader *archive_from_handle(AbcArchiveHandle *handle)
{
	return reinterpret_cast<ArchiveReader *>(handle);
}

ABC_INLINE AbcArchiveHandle *handle_from_archive(ArchiveReader *archive)
{
	return reinterpret_cast<AbcArchiveHandle *>(archive);
}

//#define USE_NURBS

/* NOTE: this function is similar to visit_objects below, need to keep them in
 * sync. */
static void gather_objects_paths(const IObject &object, ListBase *object_paths)
{
	if (!object.valid()) {
		return;
	}

	for (int i = 0; i < object.getNumChildren(); ++i) {
		IObject child = object.getChild(i);

		if (!child.valid()) {
			continue;
		}

		bool get_path = false;

		const MetaData &md = child.getMetaData();

		if (IXform::matches(md)) {
			/* Check whether or not this object is a Maya locator, which is
			 * similar to empties used as parent object in Blender. */
			if (has_property(child.getProperties(), "locator")) {
				get_path = true;
			}
			else {
				/* Avoid creating an empty object if the child of this transform
				 * is not a transform (that is an empty). */
				if (child.getNumChildren() == 1) {
					if (IXform::matches(child.getChild(0).getMetaData())) {
						get_path = true;
					}
#if 0
					else {
						std::cerr << "Skipping " << child.getFullName() << '\n';
					}
#endif
				}
				else {
					get_path = true;
				}
			}
		}
		else if (IPolyMesh::matches(md)) {
			get_path = true;
		}
		else if (ISubD::matches(md)) {
			get_path = true;
		}
		else if (INuPatch::matches(md)) {
#ifdef USE_NURBS
			get_path = true;
#endif
		}
		else if (ICamera::matches(md)) {
			get_path = true;
		}
		else if (IPoints::matches(md)) {
			get_path = true;
		}
		else if (IMaterial::matches(md)) {
			/* Pass for now. */
		}
		else if (ILight::matches(md)) {
			/* Pass for now. */
		}
		else if (IFaceSet::matches(md)) {
			/* Pass, those are handled in the mesh reader. */
		}
		else if (ICurves::matches(md)) {
			get_path = true;
		}
		else {
			assert(false);
		}

		if (get_path) {
			AlembicObjectPath *abc_path = static_cast<AlembicObjectPath *>(
			                                  MEM_callocN(sizeof(AlembicObjectPath), "AlembicObjectPath"));

			BLI_strncpy(abc_path->path, child.getFullName().c_str(), PATH_MAX);

			BLI_addtail(object_paths, abc_path);
		}

		gather_objects_paths(child, object_paths);
	}
}

AbcArchiveHandle *ABC_create_handle(const char *filename, ListBase *object_paths)
{
	ArchiveReader *archive = new ArchiveReader(filename);

	if (!archive->valid()) {
		delete archive;
		return NULL;
	}

	if (object_paths) {
		gather_objects_paths(archive->getTop(), object_paths);
	}

	return handle_from_archive(archive);
}

void ABC_free_handle(AbcArchiveHandle *handle)
{
	delete archive_from_handle(handle);
}

int ABC_get_version()
{
	return ALEMBIC_LIBRARY_VERSION;
}

static void find_iobject(const IObject &object, IObject &ret,
                         const std::string &path)
{
	if (!object.valid()) {
		return;
	}

	std::vector<std::string> tokens;
	split(path, '/', tokens);

	IObject tmp = object;

	std::vector<std::string>::iterator iter;
	for (iter = tokens.begin(); iter != tokens.end(); ++iter) {
		IObject child = tmp.getChild(*iter);
		tmp = child;
	}

	ret = tmp;
}

struct ExportJobData {
	Scene *scene;
	Main *bmain;

	char filename[1024];
	ExportSettings settings;

	short *stop;
	short *do_update;
	float *progress;

	bool was_canceled;
};

static void export_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
	ExportJobData *data = static_cast<ExportJobData *>(customdata);

	data->stop = stop;
	data->do_update = do_update;
	data->progress = progress;

	/* XXX annoying hack: needed to prevent data corruption when changing
	 * scene frame in separate threads
	 */
	G.is_rendering = true;
	BKE_spacedata_draw_locks(true);

	G.is_break = false;

	try {
		Scene *scene = data->scene;
		AbcExporter exporter(scene, data->filename, data->settings);

		const int orig_frame = CFRA;

		data->was_canceled = false;
		exporter(data->bmain, *data->progress, data->was_canceled);

		if (CFRA != orig_frame) {
			CFRA = orig_frame;

			BKE_scene_update_for_newframe(data->bmain->eval_ctx, data->bmain,
			                              scene, scene->lay);
		}
	}
	catch (const std::exception &e) {
		std::cerr << "Abc Export error: " << e.what() << '\n';
	}
	catch (...) {
		std::cerr << "Abc Export error\n";
	}
}

static void export_endjob(void *customdata)
{
	ExportJobData *data = static_cast<ExportJobData *>(customdata);

	if (data->was_canceled && BLI_exists(data->filename)) {
		BLI_delete(data->filename, false, false);
	}

	G.is_rendering = false;
	BKE_spacedata_draw_locks(false);
}

void ABC_export(
        Scene *scene,
        bContext *C,
        const char *filepath,
        const struct AlembicExportParams *params)
{
	ExportJobData *job = static_cast<ExportJobData *>(MEM_mallocN(sizeof(ExportJobData), "ExportJobData"));
	job->scene = scene;
	job->bmain = CTX_data_main(C);
	BLI_strncpy(job->filename, filepath, 1024);

	job->settings.scene = job->scene;

	/* Sybren: for now we only export the active scene layer.
	 * Later in the 2.8 development process this may be replaced by using
	 * a specific collection for Alembic I/O, which can then be toggled
	 * between "real" objects and cached Alembic files. */
	job->settings.sl = CTX_data_scene_layer(C);

	job->settings.frame_start = params->frame_start;
	job->settings.frame_end = params->frame_end;
	job->settings.frame_step_xform = params->frame_step_xform;
	job->settings.frame_step_shape = params->frame_step_shape;
	job->settings.shutter_open = params->shutter_open;
	job->settings.shutter_close = params->shutter_close;

	/* Sybren: For now this is ignored, until we can get selection
	 * detection working through Base pointers (instead of ob->flags). */
	job->settings.selected_only = params->selected_only;

	job->settings.export_face_sets = params->face_sets;
	job->settings.export_normals = params->normals;
	job->settings.export_uvs = params->uvs;
	job->settings.export_vcols = params->vcolors;
	job->settings.apply_subdiv = params->apply_subdiv;
	job->settings.flatten_hierarchy = params->flatten_hierarchy;

	/* Sybren: visible_layer & renderable only is ignored for now,
	 * to be replaced with collections later in the 2.8 dev process
	 * (also see note above). */
	job->settings.visible_layers_only = params->visible_layers_only;
	job->settings.renderable_only = params->renderable_only;

	job->settings.use_subdiv_schema = params->use_subdiv_schema;
	job->settings.export_ogawa = (params->compression_type == ABC_ARCHIVE_OGAWA);
	job->settings.pack_uv = params->packuv;
	job->settings.global_scale = params->global_scale;
	job->settings.triangulate = params->triangulate;
	job->settings.quad_method = params->quad_method;
	job->settings.ngon_method = params->ngon_method;

	if (job->settings.frame_start > job->settings.frame_end) {
		std::swap(job->settings.frame_start, job->settings.frame_end);
	}

	wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
	                            CTX_wm_window(C),
	                            job->scene,
	                            "Alembic Export",
	                            WM_JOB_PROGRESS,
	                            WM_JOB_TYPE_ALEMBIC);

	/* setup job */
	WM_jobs_customdata_set(wm_job, job, MEM_freeN);
	WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
	WM_jobs_callbacks(wm_job, export_startjob, NULL, NULL, export_endjob);

	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

/* ********************** Import file ********************** */

static void visit_object(const IObject &object,
                         std::vector<AbcObjectReader *> &readers,
                         GHash *parent_map,
                         ImportSettings &settings)
{
	if (!object.valid()) {
		return;
	}

	for (int i = 0; i < object.getNumChildren(); ++i) {
		IObject child = object.getChild(i);

		if (!child.valid()) {
			continue;
		}

		AbcObjectReader *reader = NULL;

		const MetaData &md = child.getMetaData();

		if (IXform::matches(md)) {
			bool create_xform = false;

			/* Check whether or not this object is a Maya locator, which is
			 * similar to empties used as parent object in Blender. */
			if (has_property(child.getProperties(), "locator")) {
				create_xform = true;
			}
			else {
				/* Avoid creating an empty object if the child of this transform
				 * is not a transform (that is an empty). */
				if (child.getNumChildren() == 1) {
					if (IXform::matches(child.getChild(0).getMetaData())) {
						create_xform = true;
					}
#if 0
					else {
						std::cerr << "Skipping " << child.getFullName() << '\n';
					}
#endif
				}
				else {
					create_xform = true;
				}
			}

			if (create_xform) {
				reader = new AbcEmptyReader(child, settings);
			}
		}
		else if (IPolyMesh::matches(md)) {
			reader = new AbcMeshReader(child, settings);
		}
		else if (ISubD::matches(md)) {
			reader = new AbcSubDReader(child, settings);
		}
		else if (INuPatch::matches(md)) {
#ifdef USE_NURBS
			/* TODO(kevin): importing cyclic NURBS from other software crashes
			 * at the moment. This is due to the fact that NURBS in other
			 * software have duplicated points which causes buffer overflows in
			 * Blender. Need to figure out exactly how these points are
			 * duplicated, in all cases (cyclic U, cyclic V, and cyclic UV).
			 * Until this is fixed, disabling NURBS reading. */
			reader = new AbcNurbsReader(child, settings);
#endif
		}
		else if (ICamera::matches(md)) {
			reader = new AbcCameraReader(child, settings);
		}
		else if (IPoints::matches(md)) {
			reader = new AbcPointsReader(child, settings);
		}
		else if (IMaterial::matches(md)) {
			/* Pass for now. */
		}
		else if (ILight::matches(md)) {
			/* Pass for now. */
		}
		else if (IFaceSet::matches(md)) {
			/* Pass, those are handled in the mesh reader. */
		}
		else if (ICurves::matches(md)) {
			reader = new AbcCurveReader(child, settings);
		}
		else {
			assert(false);
		}

		if (reader) {
			readers.push_back(reader);
			reader->incref();

			AlembicObjectPath *abc_path = static_cast<AlembicObjectPath *>(
			                                  MEM_callocN(sizeof(AlembicObjectPath), "AlembicObjectPath"));

			BLI_strncpy(abc_path->path, child.getFullName().c_str(), PATH_MAX);

			BLI_addtail(&settings.cache_file->object_paths, abc_path);

			/* Cast to `void *` explicitly to avoid compiler errors because it
			 * is a `const char *` which the compiler cast to `const void *`
			 * instead of the expected `void *`. */
			BLI_ghash_insert(parent_map, (void *)child.getFullName().c_str(), reader);
		}

		visit_object(child, readers, parent_map, settings);
	}
}

enum {
	ABC_NO_ERROR = 0,
	ABC_ARCHIVE_FAIL,
};

struct ImportJobData {
	Main *bmain;
	Scene *scene;

	char filename[1024];
	ImportSettings settings;

	GHash *parent_map;
	std::vector<AbcObjectReader *> readers;

	short *stop;
	short *do_update;
	float *progress;

	char error_code;
	bool was_cancelled;
};

ABC_INLINE bool is_mesh_and_strands(const IObject &object)
{
	bool has_mesh = false;
	bool has_curve = false;

	for (int i = 0; i < object.getNumChildren(); ++i) {
		const IObject &child = object.getChild(i);

		if (!child.valid()) {
			continue;
		}

		const MetaData &md = child.getMetaData();

		if (IPolyMesh::matches(md)) {
			has_mesh = true;
		}
		else if (ISubD::matches(md)) {
			has_mesh = true;
		}
		else if (ICurves::matches(md)) {
			has_curve = true;
		}
		else if (IPoints::matches(md)) {
			has_curve = true;
		}
	}

	return has_mesh && has_curve;
}

static void import_startjob(void *user_data, short *stop, short *do_update, float *progress)
{
	SCOPE_TIMER("Alembic import, objects reading and creation");

	ImportJobData *data = static_cast<ImportJobData *>(user_data);

	data->stop = stop;
	data->do_update = do_update;
	data->progress = progress;

	ArchiveReader *archive = new ArchiveReader(data->filename);

	if (!archive->valid()) {
		delete archive;
		data->error_code = ABC_ARCHIVE_FAIL;
		return;
	}

	CacheFile *cache_file = static_cast<CacheFile *>(BKE_cachefile_add(data->bmain, BLI_path_basename(data->filename)));

	/* Decrement the ID ref-count because it is going to be incremented for each
	 * modifier and constraint that it will be attached to, so since currently
	 * it is not used by anyone, its use count will off by one. */
	id_us_min(&cache_file->id);

	cache_file->is_sequence = data->settings.is_sequence;
	cache_file->scale = data->settings.scale;
	cache_file->handle = handle_from_archive(archive);
	BLI_strncpy(cache_file->filepath, data->filename, 1024);

	data->settings.cache_file = cache_file;

	*data->do_update = true;
	*data->progress = 0.05f;

	data->parent_map = BLI_ghash_str_new("alembic parent ghash");

	/* Parse Alembic Archive. */

	visit_object(archive->getTop(), data->readers, data->parent_map, data->settings);

	if (G.is_break) {
		data->was_cancelled = true;
		return;
	}

	*data->do_update = true;
	*data->progress = 0.1f;

	/* Create objects and set scene frame range. */

	const float size = static_cast<float>(data->readers.size());
	size_t i = 0;

	chrono_t min_time = std::numeric_limits<chrono_t>::max();
	chrono_t max_time = std::numeric_limits<chrono_t>::min();

	std::vector<AbcObjectReader *>::iterator iter;
	for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
		AbcObjectReader *reader = *iter;

		if (reader->valid()) {
			reader->readObjectData(data->bmain, 0.0f);
			reader->readObjectMatrix(0.0f);

			min_time = std::min(min_time, reader->minTime());
			max_time = std::max(max_time, reader->maxTime());
		}

		*data->progress = 0.1f + 0.6f * (++i / size);
		*data->do_update = true;

		if (G.is_break) {
			data->was_cancelled = true;
			return;
		}
	}

	if (data->settings.set_frame_range) {
		Scene *scene = data->scene;

		if (data->settings.is_sequence) {
			SFRA = data->settings.offset;
			EFRA = SFRA + (data->settings.sequence_len - 1);
			CFRA = SFRA;
		}
		else if (min_time < max_time) {
			SFRA = static_cast<int>(min_time * FPS);
			EFRA = static_cast<int>(max_time * FPS);
			CFRA = SFRA;
		}
	}

	/* Setup parentship. */

	i = 0;
	for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
		const AbcObjectReader *reader = *iter;
		const AbcObjectReader *parent_reader = NULL;
		const IObject &iobject = reader->iobject();

		IObject parent = iobject.getParent();

		if (!IXform::matches(iobject.getHeader())) {
			/* In the case of an non XForm node, the parent is the transform
			 * matrix of the data itself, so we get the its grand parent.
			 */

			/* Special case with object only containing a mesh and some strands,
			 * we want both objects to be parented to the same object. */
			if (!is_mesh_and_strands(parent)) {
				parent = parent.getParent();
			}
		}

		parent_reader = reinterpret_cast<AbcObjectReader *>(
		                    BLI_ghash_lookup(data->parent_map, parent.getFullName().c_str()));

		if (parent_reader) {
			Object *parent = parent_reader->object();

			if (parent != NULL && reader->object() != parent) {
				Object *ob = reader->object();
				ob->parent = parent;
			}
		}

		*data->progress = 0.7f + 0.3f * (++i / size);
		*data->do_update = true;

		if (G.is_break) {
			data->was_cancelled = true;
			return;
		}
	}
}

static void import_endjob(void *user_data)
{
	SCOPE_TIMER("Alembic import, cleanup");

	ImportJobData *data = static_cast<ImportJobData *>(user_data);

	std::vector<AbcObjectReader *>::iterator iter;

	/* Delete objects on cancelation. */
	if (data->was_cancelled) {
		for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
			Object *ob = (*iter)->object();

			if (ob->data) {
				BKE_libblock_free_us(data->bmain, ob->data);
				ob->data = NULL;
			}

			BKE_libblock_free_us(data->bmain, ob);
		}
	}
	else {
		/* Add object to scene. */
		BKE_scene_base_deselect_all(data->scene);

		for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
			Object *ob = (*iter)->object();
			ob->lay = data->scene->lay;

			BKE_scene_base_add(data->scene, ob);

			DAG_id_tag_update_ex(data->bmain, &ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		}

		DAG_relations_tag_update(data->bmain);
	}

	for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
		AbcObjectReader *reader = *iter;
		reader->decref();

		if (reader->refcount() == 0) {
			delete reader;
		}
	}

	if (data->parent_map) {
		BLI_ghash_free(data->parent_map, NULL, NULL);
	}

	switch (data->error_code) {
		default:
		case ABC_NO_ERROR:
			break;
		case ABC_ARCHIVE_FAIL:
			WM_report(RPT_ERROR, "Could not open Alembic archive for reading! See console for detail.");
			break;
	}

	WM_main_add_notifier(NC_SCENE | ND_FRAME, data->scene);
}

static void import_freejob(void *user_data)
{
	ImportJobData *data = static_cast<ImportJobData *>(user_data);
	delete data;
}

void ABC_import(bContext *C, const char *filepath, float scale, bool is_sequence, bool set_frame_range, int sequence_len, int offset, bool validate_meshes)
{
	/* Using new here since MEM_* funcs do not call ctor to properly initialize
	 * data. */
	ImportJobData *job = new ImportJobData();
	job->bmain = CTX_data_main(C);
	job->scene = CTX_data_scene(C);
	BLI_strncpy(job->filename, filepath, 1024);

	job->settings.scale = scale;
	job->settings.is_sequence = is_sequence;
	job->settings.set_frame_range = set_frame_range;
	job->settings.sequence_len = sequence_len;
	job->settings.offset = offset;
	job->settings.validate_meshes = validate_meshes;
	job->parent_map = NULL;
	job->error_code = ABC_NO_ERROR;
	job->was_cancelled = false;

	G.is_break = false;

	wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
	                            CTX_wm_window(C),
	                            job->scene,
	                            "Alembic Import",
	                            WM_JOB_PROGRESS,
	                            WM_JOB_TYPE_ALEMBIC);

	/* setup job */
	WM_jobs_customdata_set(wm_job, job, import_freejob);
	WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
	WM_jobs_callbacks(wm_job, import_startjob, NULL, NULL, import_endjob);

	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

/* ************************************************************************** */

void ABC_get_transform(CacheReader *reader, float r_mat[4][4], float time, float scale)
{
	if (!reader) {
		return;
	}

	AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);

	bool is_constant = false;
	abc_reader->read_matrix(r_mat, time, scale, is_constant);
}

/* ************************************************************************** */

DerivedMesh *ABC_read_mesh(CacheReader *reader,
                           Object *ob,
                           DerivedMesh *dm,
                           const float time,
                           const char **err_str,
                           int read_flag)
{
	AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);
	IObject iobject = abc_reader->iobject();

	if (!iobject.valid()) {
		*err_str = "Invalid object: verify object path";
		return NULL;
	}

	const ObjectHeader &header = iobject.getHeader();

	if (IPolyMesh::matches(header)) {
		if (ob->type != OB_MESH) {
			*err_str = "Object type mismatch: object path points to a mesh!";
			return NULL;
		}

		return abc_reader->read_derivedmesh(dm, time, read_flag, err_str);
	}
	else if (ISubD::matches(header)) {
		if (ob->type != OB_MESH) {
			*err_str = "Object type mismatch: object path points to a subdivision mesh!";
			return NULL;
		}

		return abc_reader->read_derivedmesh(dm, time, read_flag, err_str);
	}
	else if (IPoints::matches(header)) {
		if (ob->type != OB_MESH) {
			*err_str = "Object type mismatch: object path points to a point cloud (requires a mesh object)!";
			return NULL;
		}

		return abc_reader->read_derivedmesh(dm, time, read_flag, err_str);
	}
	else if (ICurves::matches(header)) {
		if (ob->type != OB_CURVE) {
			*err_str = "Object type mismatch: object path points to a curve!";
			return NULL;
		}

		return abc_reader->read_derivedmesh(dm, time, read_flag, err_str);
	}

	*err_str = "Unsupported object type: verify object path"; // or poke developer
	return NULL;
}

/* ************************************************************************** */

void CacheReader_free(CacheReader *reader)
{
	AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);
	abc_reader->decref();

	if (abc_reader->refcount() == 0) {
		delete abc_reader;
	}
}

CacheReader *CacheReader_open_alembic_object(AbcArchiveHandle *handle, CacheReader *reader, Object *object, const char *object_path)
{
	if (object_path[0] == '\0') {
		return reader;
	}

	ArchiveReader *archive = archive_from_handle(handle);

	if (!archive || !archive->valid()) {
		return reader;
	}

	IObject iobject;
	find_iobject(archive->getTop(), iobject, object_path);

	if (reader) {
		CacheReader_free(reader);
	}

	ImportSettings settings;
	AbcObjectReader *abc_reader = create_reader(iobject, settings);
	abc_reader->object(object);
	abc_reader->incref();

	return reinterpret_cast<CacheReader *>(abc_reader);
}
