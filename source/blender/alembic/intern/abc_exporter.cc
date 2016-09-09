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

#include "abc_exporter.h"

#include <cmath>

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
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"  /* for FILE_MAX */

#include "BLI_string.h"

#ifdef WIN32
/* needed for MSCV because of snprintf from BLI_string */
#	include "BLI_winstuff.h"
#endif

#include "BKE_anim.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
}

using Alembic::Abc::TimeSamplingPtr;
using Alembic::Abc::OBox3dProperty;

/* ************************************************************************** */

ExportSettings::ExportSettings()
    : scene(NULL)
    , selected_only(false)
    , visible_layers_only(false)
    , renderable_only(false)
    , frame_start(1)
    , frame_end(1)
    , frame_step_xform(1)
    , frame_step_shape(1)
    , shutter_open(0.0)
    , shutter_close(1.0)
    , global_scale(1.0f)
    , flatten_hierarchy(false)
    , export_normals(false)
    , export_uvs(false)
    , export_vcols(false)
    , export_face_sets(false)
    , export_vweigths(false)
    , apply_subdiv(false)
    , use_subdiv_schema(false)
    , export_child_hairs(true)
    , export_ogawa(true)
    , pack_uv(false)
    , do_convert_axis(false)
{}

static bool object_is_smoke_sim(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Smoke);

	if (md) {
		SmokeModifierData *smd = reinterpret_cast<SmokeModifierData *>(md);
		return (smd->type == MOD_SMOKE_TYPE_DOMAIN);
	}

	return false;
}

static bool object_is_shape(Object *ob)
{
	switch (ob->type) {
		case OB_MESH:
			if (object_is_smoke_sim(ob)) {
				return false;
			}

			return true;
		case OB_CURVE:
		case OB_SURF:
		case OB_CAMERA:
			return true;
		default:
			return false;
	}
}

static bool export_object(const ExportSettings * const settings, Object *ob)
{
	if (settings->selected_only && !parent_selected(ob)) {
		return false;
	}

	if (settings->visible_layers_only && !(settings->scene->lay & ob->lay)) {
		return false;
	}

	if (settings->renderable_only && (ob->restrictflag & OB_RESTRICT_RENDER)) {
		return false;
	}

	return true;
}

/* ************************************************************************** */

AbcExporter::AbcExporter(Scene *scene, const char *filename, ExportSettings &settings)
    : m_settings(settings)
    , m_filename(filename)
    , m_trans_sampling_index(0)
    , m_shape_sampling_index(0)
    , m_scene(scene)
{}

AbcExporter::~AbcExporter()
{
	std::map<std::string, AbcTransformWriter*>::iterator it, e;
	for (it = m_xforms.begin(), e = m_xforms.end(); it != e; ++it) {
		delete it->second;
	}

	for (int i = 0, e = m_shapes.size(); i != e; ++i) {
		delete m_shapes[i];
	}
}

void AbcExporter::getShutterSamples(double step, bool time_relative,
                                    std::vector<double> &samples)
{
	samples.clear();

	const double time_factor = time_relative ? m_scene->r.frs_sec : 1.0;
	const double shutter_open = m_settings.shutter_open;
	const double shutter_close = m_settings.shutter_close;

	/* sample all frame */
	if (shutter_open == 0.0 && shutter_close == 1.0) {
		for (double t = 0; t < 1.0; t += step) {
			samples.push_back((t + m_settings.frame_start) / time_factor);
		}
	}
	else {
		/* sample between shutter open & close */
		const int nsamples = std::max((1.0 / step) - 1.0, 1.0);
		const double time_inc = (shutter_close - shutter_open) / nsamples;

		for (double t = shutter_open; t <= shutter_close; t += time_inc) {
			samples.push_back((t + m_settings.frame_start) / time_factor);
		}
	}
}

Alembic::Abc::TimeSamplingPtr AbcExporter::createTimeSampling(double step)
{
	TimeSamplingPtr time_sampling;
	std::vector<double> samples;

	if (m_settings.frame_start == m_settings.frame_end) {
		time_sampling.reset(new Alembic::Abc::TimeSampling());
		return time_sampling;
	}

	getShutterSamples(step, true, samples);

	Alembic::Abc::TimeSamplingType ts(static_cast<uint32_t>(samples.size()), 1.0 / m_scene->r.frs_sec);
	time_sampling.reset(new Alembic::Abc::TimeSampling(ts, samples));

	return time_sampling;
}

void AbcExporter::getFrameSet(double step, std::set<double> &frames)
{
	frames.clear();

	std::vector<double> shutter_samples;

	getShutterSamples(step, false, shutter_samples);

	for (int frame = m_settings.frame_start; frame <= m_settings.frame_end; ++frame) {
		for (int j = 0, e = shutter_samples.size(); j < e; ++j) {
			frames.insert(frame + shutter_samples[j]);
		}
	}
}

void AbcExporter::operator()(Main *bmain, float &progress, bool &was_canceled)
{
	std::string scene_name;

	if (bmain->name[0] != '\0') {
		char scene_file_name[FILE_MAX];
		BLI_strncpy(scene_file_name, bmain->name, FILE_MAX);
		scene_name = scene_file_name;
	}
	else {
		scene_name = "untitled";
	}

	Scene *scene = m_scene;
	const int fps = FPS;
	char buf[16];
	snprintf(buf, 15, "%d", fps);
	const std::string str_fps = buf;

	Alembic::AbcCoreAbstract::MetaData md;
	md.set("FramesPerTimeUnit", str_fps);

	ArchiveWriter writer(m_filename, scene_name.c_str(), m_settings.export_ogawa, md);
	m_archive = writer.archive();

	/* Create time samplings for transforms and shapes. */

	TimeSamplingPtr trans_time = createTimeSampling(m_settings.frame_step_xform);

	m_trans_sampling_index = m_archive.addTimeSampling(*trans_time);

	TimeSamplingPtr shape_time;

	if ((m_settings.frame_step_shape == m_settings.frame_step_xform) ||
	    (m_settings.frame_start == m_settings.frame_end))
	{
		shape_time = trans_time;
		m_shape_sampling_index = m_trans_sampling_index;
	}
	else {
		shape_time = createTimeSampling(m_settings.frame_step_shape);
		m_shape_sampling_index = m_archive.addTimeSampling(*shape_time);
	}

	OBox3dProperty archive_bounds_prop = Alembic::AbcGeom::CreateOArchiveBounds(m_archive, m_trans_sampling_index);

	if (m_settings.flatten_hierarchy) {
		createTransformWritersFlat();
	}
	else {
		createTransformWritersHierarchy(bmain->eval_ctx);
	}

	createShapeWriters(bmain->eval_ctx);

	/* Make a list of frames to export. */

	std::set<double> xform_frames;
	getFrameSet(m_settings.frame_step_xform, xform_frames);

	std::set<double> shape_frames;
	getFrameSet(m_settings.frame_step_shape, shape_frames);

	/* Merge all frames needed. */

	std::set<double> frames(xform_frames);
	frames.insert(shape_frames.begin(), shape_frames.end());

	/* Export all frames. */

	std::set<double>::const_iterator begin = frames.begin();
	std::set<double>::const_iterator end = frames.end();

	const float size = static_cast<float>(frames.size());
	size_t i = 0;

	for (; begin != end; ++begin) {
		progress = (++i / size);

		if (G.is_break) {
			was_canceled = true;
			break;
		}

		const double frame = *begin;

		/* 'frame' is offset by start frame, so need to cancel the offset. */
		setCurrentFrame(bmain, frame - m_settings.frame_start);

		if (shape_frames.count(frame) != 0) {
			for (int i = 0, e = m_shapes.size(); i != e; ++i) {
				m_shapes[i]->write();
			}
		}

		if (xform_frames.count(frame) == 0) {
			continue;
		}

		std::map<std::string, AbcTransformWriter *>::iterator xit, xe;
		for (xit = m_xforms.begin(), xe = m_xforms.end(); xit != xe; ++xit) {
			xit->second->write();
		}

		/* Save the archive 's bounding box. */
		Imath::Box3d bounds;

		for (xit = m_xforms.begin(), xe = m_xforms.end(); xit != xe; ++xit) {
			Imath::Box3d box = xit->second->bounds();
			bounds.extendBy(box);
		}

		archive_bounds_prop.set(bounds);
	}
}

void AbcExporter::createTransformWritersHierarchy(EvaluationContext *eval_ctx)
{
	Base *base = static_cast<Base *>(m_scene->base.first);

	while (base) {
		Object *ob = base->object;

		if (export_object(&m_settings, ob)) {
			switch(ob->type) {
				case OB_LAMP:
				case OB_LATTICE:
				case OB_MBALL:
				case OB_SPEAKER:
					/* We do not export transforms for objects of these classes. */
					break;

				default:
					exploreTransform(eval_ctx, ob, ob->parent, NULL);
			}
		}

		base = base->next;
	}
}

void AbcExporter::createTransformWritersFlat()
{
	Base *base = static_cast<Base *>(m_scene->base.first);

	while (base) {
		Object *ob = base->object;

		if (export_object(&m_settings, ob) && object_is_shape(ob)) {
			std::string name = get_id_name(ob);
			m_xforms[name] = new AbcTransformWriter(ob, m_archive.getTop(), 0, m_trans_sampling_index, m_settings);
		}

		base = base->next;
	}
}

void AbcExporter::exploreTransform(EvaluationContext *eval_ctx, Object *ob, Object *parent, Object *dupliObParent)
{
	createTransformWriter(ob, parent, dupliObParent);

	ListBase *lb = object_duplilist(eval_ctx, m_scene, ob);

	if (lb) {
		DupliObject *link = static_cast<DupliObject *>(lb->first);
		Object *dupli_ob = NULL;
		Object *dupli_parent = NULL;
		
		while (link) {
			if (link->type == OB_DUPLIGROUP) {
				dupli_ob = link->ob;
				dupli_parent = (dupli_ob->parent) ? dupli_ob->parent : ob;

				exploreTransform(eval_ctx, dupli_ob, dupli_parent, ob);
			}

			link = link->next;
		}
	}

	free_object_duplilist(lb);
}

void AbcExporter::createTransformWriter(Object *ob, Object *parent, Object *dupliObParent)
{
	const std::string name = get_object_dag_path_name(ob, dupliObParent);

	/* check if we have already created a transform writer for this object */
	if (m_xforms.find(name) != m_xforms.end()){
		std::cerr << "xform " << name << " already exists\n";
		return;
	}

	AbcTransformWriter *parent_xform = NULL;

	if (parent) {
		const std::string parentname = get_object_dag_path_name(parent, dupliObParent);
		parent_xform = getXForm(parentname);

		if (!parent_xform) {
			if (parent->parent) {
				createTransformWriter(parent, parent->parent, dupliObParent);
			}
			else {
				createTransformWriter(parent, dupliObParent, dupliObParent);
			}

			parent_xform = getXForm(parentname);
		}
	}

	if (parent_xform) {
		m_xforms[name] = new AbcTransformWriter(ob, parent_xform->alembicXform(), parent_xform, m_trans_sampling_index, m_settings);
		m_xforms[name]->setParent(parent);
	}
	else {
		m_xforms[name] = new AbcTransformWriter(ob, m_archive.getTop(), NULL, m_trans_sampling_index, m_settings);
	}
}

void AbcExporter::createShapeWriters(EvaluationContext *eval_ctx)
{
	Base *base = static_cast<Base *>(m_scene->base.first);

	while (base) {
		Object *ob = base->object;
		exploreObject(eval_ctx, ob, NULL);

		base = base->next;
	}
}

void AbcExporter::exploreObject(EvaluationContext *eval_ctx, Object *ob, Object *dupliObParent)
{
	ListBase *lb = object_duplilist(eval_ctx, m_scene, ob);
	
	createShapeWriter(ob, dupliObParent);
	
	if (lb) {
		DupliObject *dupliob = static_cast<DupliObject *>(lb->first);

		while (dupliob) {
			if (dupliob->type == OB_DUPLIGROUP) {
				exploreObject(eval_ctx, dupliob->ob, ob);
			}

			dupliob = dupliob->next;
		}
	}

	free_object_duplilist(lb);
}

void AbcExporter::createShapeWriter(Object *ob, Object *dupliObParent)
{
	if (!object_is_shape(ob)) {
		return;
	}

	if (!export_object(&m_settings, ob)) {
		return;
	}

	std::string name;

	if (m_settings.flatten_hierarchy) {
		name = get_id_name(ob);
	}
	else {
		name = get_object_dag_path_name(ob, dupliObParent);
	}
	
	AbcTransformWriter *xform = getXForm(name);

	if (!xform) {
		std::cerr << __func__ << ": xform " << name << " is NULL\n";
		return;
	}

	ParticleSystem *psys = static_cast<ParticleSystem *>(ob->particlesystem.first);

	for (; psys; psys = psys->next) {
		if (!psys_check_enabled(ob, psys, G.is_rendering) || !psys->part) {
			continue;
		}

		if (psys->part->type == PART_HAIR) {
			m_settings.export_child_hairs = true;
			m_shapes.push_back(new AbcHairWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings, psys));
		}
		else if (psys->part->type == PART_EMITTER) {
			m_shapes.push_back(new AbcPointsWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings, psys));
		}
	}

	switch(ob->type) {
		case OB_MESH:
		{
			Mesh *me = static_cast<Mesh *>(ob->data);

			if (!me || me->totvert == 0) {
				return;
			}

			m_shapes.push_back(new AbcMeshWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings));
			break;
		}
		case OB_SURF:
		{
			Curve *cu = static_cast<Curve *>(ob->data);

			if (!cu) {
				return;
			}

			m_shapes.push_back(new AbcNurbsWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings));
			break;
		}
		case OB_CURVE:
		{
			Curve *cu = static_cast<Curve *>(ob->data);

			if (!cu) {
				return;
			}

			m_shapes.push_back(new AbcCurveWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings));
			break;
		}
		case OB_CAMERA:
		{
			Camera *cam = static_cast<Camera *>(ob->data);

			if (cam->type == CAM_PERSP) {
				m_shapes.push_back(new AbcCameraWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings));
			}

			break;
		}
	}
}

AbcTransformWriter *AbcExporter::getXForm(const std::string &name)
{
	std::map<std::string, AbcTransformWriter *>::iterator it = m_xforms.find(name);

	if (it == m_xforms.end()) {
		return NULL;
	}

	return it->second;
}

void AbcExporter::setCurrentFrame(Main *bmain, double t)
{
	m_scene->r.cfra = std::floor(t);
	m_scene->r.subframe = t - m_scene->r.cfra;
	BKE_scene_update_for_newframe(bmain->eval_ctx, bmain, m_scene, m_scene->lay);
}
