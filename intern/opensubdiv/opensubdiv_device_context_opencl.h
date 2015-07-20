/*
 * Adopted from OpenSubdiv with the following license:
 *
 *   Copyright 2015 Pixar
 *
 *   Licensed under the Apache License, Version 2.0 (the "Apache License")
 *   with the following modification; you may not use this file except in
 *   compliance with the Apache License and the following modification to it:
 *   Section 6. Trademarks. is deleted and replaced with:
 *
 *   6. Trademarks. This License does not grant permission to use the trade
 *      names, trademarks, service marks, or product names of the Licensor
 *      and its affiliates, except as required to comply with Section 4(c) of
 *      the License and to reproduce the content of the NOTICE file.
 *
 *   You may obtain a copy of the Apache License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the Apache License with the above modification is
 *   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *   KIND, either express or implied. See the Apache License for the specific
 *   language governing permissions and limitations under the Apache License.
 *
 */

#ifndef __OPENSUBDIV_DEV_CE_CONTEXT_OPENCL_H__
#define __OPENSUBDIV_DEV_CE_CONTEXT_OPENCL_H__

#include <opensubdiv/osd/opencl.h>

class CLDeviceContext {
public:
	CLDeviceContext();
	~CLDeviceContext();

	static bool HAS_CL_VERSION_1_1 ();

	bool Initialize();

	bool IsInitialized() const {
		return (_clContext != NULL);
	}

	cl_context GetContext() const {
		return _clContext;
	}
	cl_command_queue GetCommandQueue() const {
		return _clCommandQueue;
	}

protected:
	cl_context _clContext;
	cl_command_queue _clCommandQueue;
};

#endif /* __OPENSUBDIV_DEV_CE_CONTEXT_OPENCL_H__ */
