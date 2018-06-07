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
 * Contributor(s): Sybren A. Stüvel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __ABC_MBALL_H__
#define __ABC_MBALL_H__

#include "abc_object.h"

class AbcMeshWriter;
struct EvaluationContext;
struct Main;
struct MetaBall;
struct Object;

/* AbcMBallWriter converts the metaballs to meshes at every frame,
 * and defers to a wrapped AbcMeshWriter to perform the writing
 * to the Alembic file. Only the basis balls are exported, as this
 * results in the entire shape as one mesh. */
class AbcMBallWriter : public AbcObjectWriter {
	AbcMeshWriter *m_mesh_writer;
	Object *m_mesh_ob;
	bool m_is_animated;
	Main *m_bmain;
public:
	AbcMBallWriter(
	        Main *bmain,
	        Object *ob,
	        AbcTransformWriter *parent,
	        uint32_t time_sampling,
	        ExportSettings &settings);

	~AbcMBallWriter();

	static bool isBasisBall(Scene *scene, Object *ob);

private:
	virtual void do_write();
	bool isAnimated() const;
};


#endif  /* __ABC_MBALL_H__ */
