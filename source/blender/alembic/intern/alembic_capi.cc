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

#ifdef WITH_ALEMBIC_HDF5
#  include <Alembic/AbcCoreHDF5/All.h>
#endif

#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcMaterial/IMaterial.h>

#include <fstream>

#ifdef WIN32
#  include "utfconv.h"
#endif

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

using Alembic::AbcGeom::ErrorHandler;
using Alembic::AbcGeom::Exception;
using Alembic::AbcGeom::MetaData;
using Alembic::AbcGeom::P3fArraySamplePtr;
using Alembic::AbcGeom::kWrapExisting;

using Alembic::AbcGeom::IArchive;
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

static IArchive open_archive(const std::string &filename,
                             const std::vector<std::istream *> &input_streams,
                             bool &is_hdf5)
{
	try {
		is_hdf5 = false;
		Alembic::AbcCoreOgawa::ReadArchive archive_reader(input_streams);

		return IArchive(archive_reader(filename),
		                kWrapExisting,
		                ErrorHandler::kThrowPolicy);
	}
	catch (const Exception &e) {
		std::cerr << e.what() << '\n';

#ifdef WITH_ALEMBIC_HDF5
		try {
			is_hdf5 = true;
			Alembic::AbcCoreAbstract::ReadArraySampleCachePtr cache_ptr;

			return IArchive(Alembic::AbcCoreHDF5::ReadArchive(),
			                filename.c_str(), ErrorHandler::kThrowPolicy,
			                cache_ptr);
		}
		catch (const Exception &) {
			std::cerr << e.what() << '\n';
			return IArchive();
		}
#else
		return IArchive();
#endif
	}

	return IArchive();
}

/* Wrapper around an archive to be able to use streams so that unicode paths
 * work on Windows (T49112), and to make sure the input stream remains valid as
 * long as the archive is open. */
class ArchiveWrapper {
	IArchive m_archive;
	std::ifstream m_infile;
	std::vector<std::istream *> m_streams;

public:
	explicit ArchiveWrapper(const char *filename)
	{
#ifdef WIN32
		UTF16_ENCODE(filename);
		std::wstring wstr(filename_16);
		m_infile.open(wstr.c_str(), std::ios::in | std::ios::binary);
		UTF16_UN_ENCODE(filename);
#else
		m_infile.open(filename, std::ios::in | std::ios::binary);
#endif

		m_streams.push_back(&m_infile);

		bool is_hdf5;
		m_archive = open_archive(filename, m_streams, is_hdf5);

		/* We can't open an HDF5 file from a stream, so close it. */
		if (is_hdf5) {
			m_infile.close();
			m_streams.clear();
		}
	}

	bool valid() const
	{
		return m_archive.valid();
	}

	IObject getTop()
	{
		return m_archive.getTop();
	}
};

struct AbcArchiveHandle {
	int unused;
};

ABC_INLINE ArchiveWrapper *archive_from_handle(AbcArchiveHandle *handle)
{
	return reinterpret_cast<ArchiveWrapper *>(handle);
}

ABC_INLINE AbcArchiveHandle *handle_from_archive(ArchiveWrapper *archive)
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
	ArchiveWrapper *archive = new ArchiveWrapper(filename);

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
	job->settings.frame_start = params->frame_start;
	job->settings.frame_end = params->frame_end;
	job->settings.frame_step_xform = params->frame_step_xform;
	job->settings.frame_step_shape = params->frame_step_shape;
	job->settings.shutter_open = params->shutter_open;
	job->settings.shutter_close = params->shutter_close;
	job->settings.selected_only = params->selected_only;
	job->settings.export_face_sets = params->face_sets;
	job->settings.export_normals = params->normals;
	job->settings.export_uvs = params->uvs;
	job->settings.export_vcols = params->vcolors;
	job->settings.apply_subdiv = params->apply_subdiv;
	job->settings.flatten_hierarchy = params->flatten_hierarchy;
	job->settings.visible_layers_only = params->visible_layers_only;
	job->settings.renderable_only = params->renderable_only;
	job->settings.use_subdiv_schema = params->use_subdiv_schema;
	job->settings.export_ogawa = (params->compression_type == ABC_ARCHIVE_OGAWA);
	job->settings.pack_uv = params->packuv;
	job->settings.global_scale = params->global_scale;

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
	if (object.getNumChildren() != 2) {
		return false;
	}

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
	}

	return has_mesh && has_curve;
}

static void import_startjob(void *user_data, short *stop, short *do_update, float *progress)
{
	ImportJobData *data = static_cast<ImportJobData *>(user_data);

	data->stop = stop;
	data->do_update = do_update;
	data->progress = progress;

	ArchiveWrapper *archive = new ArchiveWrapper(data->filename);

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
			SFRA = min_time * FPS;
			EFRA = max_time * FPS;
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
		delete *iter;
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

/* ******************************* */

void ABC_get_transform(AbcArchiveHandle *handle, Object *ob, const char *object_path, float r_mat[4][4], float time, float scale)
{
	ArchiveWrapper *archive = archive_from_handle(handle);

	if (!archive || !archive->valid()) {
		return;
	}

	IObject tmp;
	find_iobject(archive->getTop(), tmp, object_path);

	IXform ixform;

	if (IXform::matches(tmp.getHeader())) {
		ixform = IXform(tmp, kWrapExisting);
	}
	else {
		ixform = IXform(tmp.getParent(), kWrapExisting);
	}

	IXformSchema schema = ixform.getSchema();

	if (!schema.valid()) {
		return;
	}

	ISampleSelector sample_sel(time);

	create_input_transform(sample_sel, ixform, ob, r_mat, scale);
}

/* ***************************************** */

static bool check_smooth_poly_flag(DerivedMesh *dm)
{
	MPoly *mpolys = dm->getPolyArray(dm);

	for (int i = 0, e = dm->getNumPolys(dm); i < e; ++i) {
		MPoly &poly = mpolys[i];

		if ((poly.flag & ME_SMOOTH) != 0) {
			return true;
		}
	}

	return false;
}

static void set_smooth_poly_flag(DerivedMesh *dm)
{
	MPoly *mpolys = dm->getPolyArray(dm);

	for (int i = 0, e = dm->getNumPolys(dm); i < e; ++i) {
		MPoly &poly = mpolys[i];
		poly.flag |= ME_SMOOTH;
	}
}

static void *add_customdata_cb(void *user_data, const char *name, int data_type)
{
	DerivedMesh *dm = static_cast<DerivedMesh *>(user_data);
	CustomDataType cd_data_type = static_cast<CustomDataType>(data_type);
	void *cd_ptr = NULL;

	if (ELEM(cd_data_type, CD_MLOOPUV, CD_MLOOPCOL)) {
		cd_ptr = CustomData_get_layer_named(dm->getLoopDataLayout(dm), cd_data_type, name);

		if (cd_ptr == NULL) {
			cd_ptr = CustomData_add_layer_named(dm->getLoopDataLayout(dm),
			                                    cd_data_type,
			                                    CD_DEFAULT,
			                                    NULL,
			                                    dm->getNumLoops(dm),
			                                    name);
		}
	}

	return cd_ptr;
}

ABC_INLINE CDStreamConfig get_config(DerivedMesh *dm)
{
	CDStreamConfig config;

	config.user_data = dm;
	config.mvert = dm->getVertArray(dm);
	config.mloop = dm->getLoopArray(dm);
	config.mpoly = dm->getPolyArray(dm);
	config.totloop = dm->getNumLoops(dm);
	config.totpoly = dm->getNumPolys(dm);
	config.loopdata = dm->getLoopDataLayout(dm);
	config.add_customdata_cb = add_customdata_cb;

	return config;
}

static DerivedMesh *read_mesh_sample(DerivedMesh *dm, const IObject &iobject, const float time, int read_flag)
{
	IPolyMesh mesh(iobject, kWrapExisting);
	IPolyMeshSchema schema = mesh.getSchema();
	ISampleSelector sample_sel(time);
	const IPolyMeshSchema::Sample sample = schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();
	const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
	const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

	DerivedMesh *new_dm = NULL;

	/* Only read point data when streaming meshes, unless we need to create new ones. */
	ImportSettings settings;
	settings.read_flag |= read_flag;

	if (dm->getNumVerts(dm) != positions->size()) {
		new_dm = CDDM_from_template(dm,
		                            positions->size(),
		                            0,
		                            0,
		                            face_indices->size(),
		                            face_counts->size());

		settings.read_flag |= MOD_MESHSEQ_READ_ALL;
	}

	CDStreamConfig config = get_config(new_dm ? new_dm : dm);

	bool do_normals = false;
	read_mesh_sample(&settings, schema, sample_sel, config, do_normals);

	if (new_dm) {
		/* Check if we had ME_SMOOTH flag set to restore it. */
		if (!do_normals && check_smooth_poly_flag(dm)) {
			set_smooth_poly_flag(new_dm);
		}

		CDDM_calc_normals(new_dm);
		CDDM_calc_edges(new_dm);

		return new_dm;
	}

	if (do_normals) {
		CDDM_calc_normals(dm);
	}

	return dm;
}

using Alembic::AbcGeom::ISubDSchema;

static DerivedMesh *read_subd_sample(DerivedMesh *dm, const IObject &iobject, const float time, int read_flag)
{
	ISubD mesh(iobject, kWrapExisting);
	ISubDSchema schema = mesh.getSchema();
	ISampleSelector sample_sel(time);
	const ISubDSchema::Sample sample = schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();
	const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
	const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

	DerivedMesh *new_dm = NULL;

	ImportSettings settings;
	settings.read_flag |= read_flag;

	if (dm->getNumVerts(dm) != positions->size()) {
		new_dm = CDDM_from_template(dm,
		                            positions->size(),
		                            0,
		                            0,
		                            face_indices->size(),
		                            face_counts->size());

		settings.read_flag |= MOD_MESHSEQ_READ_ALL;
	}

	/* Only read point data when streaming meshes, unless we need to create new ones. */
	CDStreamConfig config = get_config(new_dm ? new_dm : dm);
	read_subd_sample(&settings, schema, sample_sel, config);

	if (new_dm) {
		/* Check if we had ME_SMOOTH flag set to restore it. */
		if (check_smooth_poly_flag(dm)) {
			set_smooth_poly_flag(new_dm);
		}

		CDDM_calc_normals(new_dm);
		CDDM_calc_edges(new_dm);

		return new_dm;
	}

	return dm;
}

static DerivedMesh *read_points_sample(DerivedMesh *dm, const IObject &iobject, const float time)
{
	IPoints points(iobject, kWrapExisting);
	IPointsSchema schema = points.getSchema();
	ISampleSelector sample_sel(time);
	const IPointsSchema::Sample sample = schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();

	DerivedMesh *new_dm = NULL;

	if (dm->getNumVerts(dm) != positions->size()) {
		new_dm = CDDM_new(positions->size(), 0, 0, 0, 0);
	}

	CDStreamConfig config = get_config(new_dm ? new_dm : dm);
	read_points_sample(schema, sample_sel, config, time);

	return new_dm ? new_dm : dm;
}

/* NOTE: Alembic only stores data about control points, but the DerivedMesh
 * passed from the cache modifier contains the displist, which has more data
 * than the control points, so to avoid corrupting the displist we modify the
 * object directly and create a new DerivedMesh from that. Also we might need to
 * create new or delete existing NURBS in the curve.
 */
static DerivedMesh *read_curves_sample(Object *ob, const IObject &iobject, const float time)
{
	ICurves points(iobject, kWrapExisting);
	ICurvesSchema schema = points.getSchema();
	ISampleSelector sample_sel(time);
	const ICurvesSchema::Sample sample = schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();
	const Int32ArraySamplePtr num_vertices = sample.getCurvesNumVertices();

	int vertex_idx = 0;
	int curve_idx = 0;
	Curve *curve = static_cast<Curve *>(ob->data);

	const int curve_count = BLI_listbase_count(&curve->nurb);

	if (curve_count != num_vertices->size()) {
		BKE_nurbList_free(&curve->nurb);
		read_curve_sample(curve, schema, time);
	}
	else {
		Nurb *nurbs = static_cast<Nurb *>(curve->nurb.first);
		for (; nurbs; nurbs = nurbs->next, ++curve_idx) {
			const int totpoint = (*num_vertices)[curve_idx];

			if (nurbs->bp) {
				BPoint *point = nurbs->bp;

				for (int i = 0; i < totpoint; ++i, ++point, ++vertex_idx) {
					const Imath::V3f &pos = (*positions)[vertex_idx];
					copy_yup_zup(point->vec, pos.getValue());
				}
			}
			else if (nurbs->bezt) {
				BezTriple *bezier = nurbs->bezt;

				for (int i = 0; i < totpoint; ++i, ++bezier, ++vertex_idx) {
					const Imath::V3f &pos = (*positions)[vertex_idx];
					copy_yup_zup(bezier->vec[1], pos.getValue());
				}
			}
		}
	}

	return CDDM_from_curve(ob);
}

DerivedMesh *ABC_read_mesh(AbcArchiveHandle *handle,
                           Object *ob,
                           DerivedMesh *dm,
                           const char *object_path,
                           const float time,
                           const char **err_str,
                           int read_flag)
{
	ArchiveWrapper *archive = archive_from_handle(handle);

	if (!archive || !archive->valid()) {
		*err_str = "Invalid archive!";
		return NULL;
	}

	IObject iobject;
	find_iobject(archive->getTop(), iobject, object_path);

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

		return read_mesh_sample(dm, iobject, time, read_flag);
	}
	else if (ISubD::matches(header)) {
		if (ob->type != OB_MESH) {
			*err_str = "Object type mismatch: object path points to a subdivision mesh!";
			return NULL;
		}

		return read_subd_sample(dm, iobject, time, read_flag);
	}
	else if (IPoints::matches(header)) {
		if (ob->type != OB_MESH) {
			*err_str = "Object type mismatch: object path points to a point cloud (requires a mesh object)!";
			return NULL;
		}

		return read_points_sample(dm, iobject, time);
	}
	else if (ICurves::matches(header)) {
		if (ob->type != OB_CURVE) {
			*err_str = "Object type mismatch: object path points to a curve!";
			return NULL;
		}

		return read_curves_sample(ob, iobject, time);
	}

	*err_str = "Unsupported object type: verify object path"; // or poke developer
	return NULL;
}
