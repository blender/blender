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

#ifndef __ABC_ALEMBIC_H__
#define __ABC_ALEMBIC_H__

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct DerivedMesh;
struct ListBase;
struct Object;
struct Scene;

typedef struct AbcArchiveHandle AbcArchiveHandle;

enum {
	ABC_ARCHIVE_OGAWA = 0,
	ABC_ARCHIVE_HDF5  = 1,
};

int ABC_get_version(void);

struct AlembicExportParams {
	double frame_start;
	double frame_end;

	double frame_step_xform;
	double frame_step_shape;

	double shutter_open;
	double shutter_close;

	/* bools */
	unsigned int selected_only : 1;
	unsigned int uvs : 1;
	unsigned int normals : 1;
	unsigned int vcolors : 1;
	unsigned int apply_subdiv : 1;
	unsigned int flatten_hierarchy : 1;
	unsigned int visible_layers_only : 1;
	unsigned int renderable_only : 1;
	unsigned int face_sets : 1;
	unsigned int use_subdiv_schema : 1;
	unsigned int packuv : 1;
	unsigned int triangulate : 1;

	unsigned int compression_type : 1;

	int quad_method;
	int ngon_method;
	float global_scale;
};

void ABC_export(
        struct Scene *scene,
        struct bContext *C,
        const char *filepath,
        const struct AlembicExportParams *params);

void ABC_import(struct bContext *C,
                const char *filepath,
                float scale,
                bool is_sequence,
                bool set_frame_range,
                int sequence_len,
                int offset,
                bool validate_meshes);

AbcArchiveHandle *ABC_create_handle(const char *filename, struct ListBase *object_paths);

void ABC_free_handle(AbcArchiveHandle *handle);

void ABC_get_transform(AbcArchiveHandle *handle,
                       struct Object *ob,
                       const char *object_path,
                       float r_mat[4][4],
                       float time,
                       float scale);

struct DerivedMesh *ABC_read_mesh(AbcArchiveHandle *handle,
                                  struct Object *ob,
                                  struct DerivedMesh *dm,
                                  const char *object_path,
                                  const float time,
                                  const char **err_str,
                                  int flags);

#ifdef __cplusplus
}
#endif

#endif  /* __ABC_ALEMBIC_H__ */
