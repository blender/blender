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

#ifndef __ABC_UTIL_H__
#define __ABC_UTIL_H__

#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>

#ifdef _MSC_VER
#  define ABC_INLINE static __forceinline
#else
#  define ABC_INLINE static inline
#endif

/**
 * \brief The CacheReader struct is only used for anonymous pointers,
 * to interface between C and C++ code. This library only creates
 * pointers to AbcObjectReader (or subclasses thereof).
 */
struct CacheReader {
	int unused;
};

using Alembic::Abc::chrono_t;

class AbcObjectReader;
struct ImportSettings;

struct ID;
struct Object;

std::string get_id_name(const ID * const id);
std::string get_id_name(const Object * const ob);
std::string get_object_dag_path_name(const Object * const ob, Object *dupli_parent);

bool object_selected(Object *ob);
bool parent_selected(Object *ob);

Imath::M44d convert_matrix(float mat[4][4]);

typedef enum {
	ABC_MATRIX_WORLD = 1,
	ABC_MATRIX_LOCAL = 2,
} AbcMatrixMode;
void create_transform_matrix(Object *obj, float r_transform_mat[4][4],
                             AbcMatrixMode mode, Object *proxy_from);

void split(const std::string &s, const char delim, std::vector<std::string> &tokens);

template<class TContainer>
bool begins_with(const TContainer &input, const TContainer &match)
{
	return input.size() >= match.size()
	        && std::equal(match.begin(), match.end(), input.begin());
}

void convert_matrix(const Imath::M44d &xform, Object *ob, float r_mat[4][4]);

template <typename Schema>
void get_min_max_time_ex(const Schema &schema, chrono_t &min, chrono_t &max)
{
	const Alembic::Abc::TimeSamplingPtr &time_samp = schema.getTimeSampling();

	if (!schema.isConstant()) {
		const size_t num_samps = schema.getNumSamples();

		if (num_samps > 0) {
			const chrono_t min_time = time_samp->getSampleTime(0);
			min = std::min(min, min_time);

			const chrono_t max_time = time_samp->getSampleTime(num_samps - 1);
			max = std::max(max, max_time);
		}
	}
}

template <typename Schema>
void get_min_max_time(const Alembic::AbcGeom::IObject &object, const Schema &schema, chrono_t &min, chrono_t &max)
{
	get_min_max_time_ex(schema, min, max);

	const Alembic::AbcGeom::IObject &parent = object.getParent();
	if (parent.valid() && Alembic::AbcGeom::IXform::matches(parent.getMetaData())) {
		Alembic::AbcGeom::IXform xform(parent, Alembic::AbcGeom::kWrapExisting);
		get_min_max_time_ex(xform.getSchema(), min, max);
	}
}

bool has_property(const Alembic::Abc::ICompoundProperty &prop, const std::string &name);

float get_weight_and_index(float time,
                           const Alembic::AbcCoreAbstract::TimeSamplingPtr &time_sampling,
                           int samples_number,
                           Alembic::AbcGeom::index_t &i0,
                           Alembic::AbcGeom::index_t &i1);

AbcObjectReader *create_reader(const Alembic::AbcGeom::IObject &object, ImportSettings &settings);

/* ************************** */

/* TODO(kevin): for now keeping these transformations hardcoded to make sure
 * everything works properly, and also because Alembic is almost exclusively
 * used in Y-up software, but eventually they'll be set by the user in the UI
 * like other importers/exporters do, to support other axis. */

/* Copy from Y-up to Z-up. */

ABC_INLINE void copy_zup_from_yup(float zup[3], const float yup[3])
{
	const float old_yup1 = yup[1];  /* in case zup == yup */
	zup[0] = yup[0];
	zup[1] = -yup[2];
	zup[2] = old_yup1;
}

ABC_INLINE void copy_zup_from_yup(short zup[3], const short yup[3])
{
	const short old_yup1 = yup[1];  /* in case zup == yup */
	zup[0] = yup[0];
	zup[1] = -yup[2];
	zup[2] = old_yup1;
}

/* Copy from Z-up to Y-up. */

ABC_INLINE void copy_yup_from_zup(float yup[3], const float zup[3])
{
	const float old_zup1 = zup[1];  /* in case yup == zup */
	yup[0] = zup[0];
	yup[1] = zup[2];
	yup[2] = -old_zup1;
}

ABC_INLINE void copy_yup_from_zup(short yup[3], const short zup[3])
{
	const short old_zup1 = zup[1];  /* in case yup == zup */
	yup[0] = zup[0];
	yup[1] = zup[2];
	yup[2] = -old_zup1;
}

/* Names are given in (dst, src) order, just like
 * the parameters of copy_m44_axis_swap() */
typedef enum {
	ABC_ZUP_FROM_YUP = 1,
	ABC_YUP_FROM_ZUP = 2,
} AbcAxisSwapMode;

/* Create a rotation matrix for each axis from euler angles.
 * Euler angles are swaped to change coordinate system. */
void create_swapped_rotation_matrix(
        float rot_x_mat[3][3], float rot_y_mat[3][3],
        float rot_z_mat[3][3], const float euler[3],
        AbcAxisSwapMode mode);

void copy_m44_axis_swap(float dst_mat[4][4], float src_mat[4][4], AbcAxisSwapMode mode);

/* *************************** */

#undef ABC_DEBUG_TIME

class ScopeTimer {
	const char *m_message;
	double m_start;

public:
	ScopeTimer(const char *message);
	~ScopeTimer();
};

#ifdef ABC_DEBUG_TIME
#	define SCOPE_TIMER(message) ScopeTimer prof(message)
#else
#	define SCOPE_TIMER(message)
#endif

/* *************************** */

/**
 * Utility class whose purpose is to more easily log related informations. An
 * instance of the SimpleLogger can be created in any context, and will hold a
 * copy of all the strings passed to its output stream.
 *
 * Different instances of the class may be accessed from different threads,
 * although accessing the same instance from different threads will lead to race
 * conditions.
 */
class SimpleLogger {
	std::ostringstream m_stream;

public:
	/**
	 * Check whether or not the SimpleLogger's stream is empty.
	 */
	bool empty();

	/**
	 * Return a copy of the string contained in the SimpleLogger's stream.
	 */
	std::string str() const;

	/**
	 * Remove the bits set on the SimpleLogger's stream and clear its string.
	 */
	void clear();

	/**
	 * Return a reference to the SimpleLogger's stream, in order to e.g. push
	 * content into it.
	 */
	std::ostringstream &stream();
};

#define ABC_LOG(logger) logger.stream()

/**
 * Pass the content of the logger's stream to the specified std::ostream.
 */
std::ostream &operator<<(std::ostream &os, const SimpleLogger &logger);

#endif  /* __ABC_UTIL_H__ */
