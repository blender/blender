/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __OSL_GLOBALS_H__
#define __OSL_GLOBALS_H__

#ifdef WITH_OSL

#include <OSL/oslexec.h>

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
	OSL::PerThreadInfo *osl_thread_info;
	OSLTraceData tracedata;
	OSL::ShadingContext *context[SHADER_CONTEXT_NUM];
	OIIO::TextureSystem::Perthread *oiio_thread_info;
};

CCL_NAMESPACE_END

#endif

#endif /* __OSL_GLOBALS_H__ */

