/*
 * Copyright 2011-2021 Blender Foundation
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
 * limitations under the License
 */

#ifndef __HIPRTEW_H__
#define __HIPRTEW_H__

#include <hiprt/hiprt_types.h>

#define HIPRT_MAJOR_VERSION 2
#define HIPRT_MINOR_VERSION 0
#define HIPRT_PATCH_VERSION 0xb68861

#define HIPRT_API_VERSION 2000
#define HIPRT_VERSION_STR "02000"

typedef unsigned int hiprtuint32_t;

/* Function types. */
typedef hiprtError(thiprtCreateContext)(hiprtuint32_t hiprtApiVersion,
                                        hiprtContextCreationInput &input,
                                        hiprtContext *outContext);
typedef hiprtError(thiprtDestroyContext)(hiprtContext context);
typedef hiprtError(thiprtCreateGeometry)(hiprtContext context,
                                         const hiprtGeometryBuildInput *buildInput,
                                         const hiprtBuildOptions *buildOptions,
                                         hiprtGeometry *outGeometry);
typedef hiprtError(thiprtDestroyGeometry)(hiprtContext context,
                                          hiprtGeometry outGeometry);
typedef hiprtError(thiprtBuildGeometry)(hiprtContext context,
                                        hiprtBuildOperation buildOperation,
                                        const hiprtGeometryBuildInput *buildInput,
                                        const hiprtBuildOptions *buildOptions,
                                        hiprtDevicePtr temporaryBuffer,
                                        hiprtApiStream stream,
                                        hiprtGeometry outGeometry);
typedef hiprtError(thiprtGetGeometryBuildTemporaryBufferSize)(
    hiprtContext context,
    const hiprtGeometryBuildInput *buildInput,
    const hiprtBuildOptions *buildOptions,
    size_t *outSize);
typedef hiprtError(thiprtCreateScene)(hiprtContext context,
                                      const hiprtSceneBuildInput *buildInput,
                                      const hiprtBuildOptions *buildOptions,
                                      hiprtScene *outScene);
typedef hiprtError(thiprtDestroyScene)(hiprtContext context, hiprtScene outScene);
typedef hiprtError(thiprtBuildScene)(hiprtContext context,
                                     hiprtBuildOperation buildOperation,
                                     const hiprtSceneBuildInput *buildInput,
                                     const hiprtBuildOptions *buildOptions,
                                     hiprtDevicePtr temporaryBuffer,
                                     hiprtApiStream stream,
                                     hiprtScene outScene);
typedef hiprtError(thiprtGetSceneBuildTemporaryBufferSize)(
    hiprtContext context,
    const hiprtSceneBuildInput *buildInput,
    const hiprtBuildOptions *buildOptions,
    size_t *outSize);
typedef hiprtError(thiprtCreateFuncTable)(hiprtContext context,
                                          hiprtuint32_t numGeomTypes,
                                          hiprtuint32_t numRayTypes,
                                          hiprtFuncTable *outFuncTable);
typedef hiprtError(thiprtSetFuncTable)(hiprtContext context,
                                       hiprtFuncTable funcTable,
                                       hiprtuint32_t geomType,
                                       hiprtuint32_t rayType,
                                        hiprtFuncDataSet set);
typedef hiprtError(thiprtDestroyFuncTable)(hiprtContext context,
                                           hiprtFuncTable funcTable);

/* Function declarations. */
extern thiprtCreateContext *hiprtCreateContext;
extern thiprtDestroyContext *hiprtDestroyContext;
extern thiprtCreateGeometry *hiprtCreateGeometry;
extern thiprtDestroyGeometry *hiprtDestroyGeometry;
extern thiprtBuildGeometry *hiprtBuildGeometry;
extern thiprtGetGeometryBuildTemporaryBufferSize *hiprtGetGeometryBuildTemporaryBufferSize;
extern thiprtCreateScene *hiprtCreateScene;
extern thiprtDestroyScene *hiprtDestroyScene;
extern thiprtBuildScene *hiprtBuildScene;
extern thiprtGetSceneBuildTemporaryBufferSize *hiprtGetSceneBuildTemporaryBufferSize;
extern thiprtCreateFuncTable *hiprtCreateFuncTable;
extern thiprtSetFuncTable *hiprtSetFuncTable;
extern thiprtDestroyFuncTable *hiprtDestroyFuncTable;

/* HIPEW API. */

bool hiprtewInit();

#endif  /* __HIPRTEW_H__ */
