/*
 * Copyright 2011, Blender Foundation.
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
 */

#ifndef __OSL_GLOBALS_H__
#define __OSL_GLOBALS_H__

#ifdef WITH_OSL

#include <OSL/oslexec.h>
#include <cmath>

#include "util_map.h"
#include "util_param.h"
#include "util_thread.h"
#include "util_vector.h"

#ifndef WIN32
using std::isfinite;
#endif

CCL_NAMESPACE_BEGIN

class OSLRenderServices;

struct OSLGlobals {
	OSLGlobals()
	{
		ss = NULL;
		ts = NULL;
		services = NULL;
		use = false;
	}

	bool use;

	/* shading system */
	OSL::ShadingSystem *ss;
	OSL::TextureSystem *ts;
	OSLRenderServices *services;

	/* shader states */
	vector<OSL::ShadingAttribStateRef> surface_state;
	vector<OSL::ShadingAttribStateRef> volume_state;
	vector<OSL::ShadingAttribStateRef> displacement_state;
	OSL::ShadingAttribStateRef background_state;

	/* attributes */
	struct Attribute {
		TypeDesc type;
		AttributeElement elem;
		int offset;
		ParamValue value;
	};

	typedef unordered_map<ustring, Attribute, ustringHash> AttributeMap;
	typedef unordered_map<ustring, int, ustringHash> ObjectNameMap;

	vector<AttributeMap> attribute_map;
	ObjectNameMap object_name_map;
	vector<ustring> object_names;
};

/* trace() call result */
struct OSLTraceData {
	Ray ray;
	Intersection isect;
	ShaderData sd;
	bool setup;
	bool init;
};

/* thread key for thread specific data lookup */
struct OSLThreadData {
	OSL::ShaderGlobals globals;
	OSL::PerThreadInfo *thread_info;
	OSLTraceData tracedata;
	OSL::ShadingContext *context[SHADER_CONTEXT_NUM];
};

CCL_NAMESPACE_END

#endif

#endif /* __OSL_GLOBALS_H__ */

