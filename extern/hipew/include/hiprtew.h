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
#define HIPRT_MINOR_VERSION 3
#define HIPRT_PATCH_VERSION 0x7df94af

#define HIPRT_API_VERSION 2003
#define HIPRT_VERSION_STR "02003"
#define HIP_VERSION_STR "6.0"

#ifdef _WIN32
#define HIPRTAPI __stdcall
#else
#define HIPRTAPI
#define HIP_CB
#endif

typedef unsigned int hiprtuint32_t;

/* Function types. */
typedef hiprtError(thiprtCreateContext)(hiprtuint32_t hiprtApiVersion,
                                        const hiprtContextCreationInput &input,
                                        hiprtContext *outContext);
typedef hiprtError(thiprtDestroyContext)(hiprtContext context);
typedef hiprtError(thiprtCreateGeometry)(hiprtContext context,
                                         const hiprtGeometryBuildInput &buildInput,
                                         const hiprtBuildOptions buildOptions,
                                         hiprtGeometry &outGeometry);
typedef hiprtError(thiprtDestroyGeometry)(hiprtContext context,
                                          hiprtGeometry outGeometry);
typedef hiprtError(thiprtCreateGeometries)(hiprtContext context,
                                           uint32_t numGeometries,
                                         const hiprtGeometryBuildInput *buildInput,
                                         const hiprtBuildOptions buildOptions,
                                         hiprtGeometry **outGeometries);
typedef hiprtError(thiprtDestroyGeometries)(hiprtContext context, uint32_t numGeometries,
                                          hiprtGeometry* outGeometry);

typedef hiprtError(thiprtBuildGeometry)(hiprtContext context,
                                        hiprtBuildOperation buildOperation,
                                        const hiprtGeometryBuildInput &buildInput,
                                        const hiprtBuildOptions buildOptions,
                                        hiprtDevicePtr temporaryBuffer,
                                        hiprtApiStream stream,
                                        hiprtGeometry outGeometry);

typedef hiprtError(thiprtBuildGeometries)(hiprtContext context,
                                        uint32_t numGeometries,
                                        hiprtBuildOperation buildOperation,
                                        const hiprtGeometryBuildInput *buildInput,
                                        const hiprtBuildOptions *buildOptions,
                                        hiprtDevicePtr temporaryBuffer,
                                        hiprtApiStream stream,
                                        hiprtGeometry *outGeometries);


typedef hiprtError(thiprtGetGeometryBuildTemporaryBufferSize)(
    hiprtContext context,
    const hiprtGeometryBuildInput &buildInput,
    const hiprtBuildOptions buildOptions,
    size_t &outSize);

typedef hiprtError(thiprtGetGeometriesBuildTemporaryBufferSize)(
    hiprtContext context,
    uint32_t numGeometries,
    const hiprtGeometryBuildInput *buildInput,
    const hiprtBuildOptions *buildOptions,
    size_t &outSize);

typedef hiprtError(thiprtCompactGeometry)( hiprtContext context, hiprtApiStream stream, hiprtGeometry geometryIn, hiprtGeometry& geometryOut);

typedef hiprtError(thiprtCompactGeometries)(
	hiprtContext	context,
	uint32_t		numGeometries,
	hiprtApiStream	stream,
	hiprtGeometry*	geometriesIn,
	hiprtGeometry** geometriesOut );

typedef hiprtError(thiprtCreateScene)(hiprtContext context,
                                      const hiprtSceneBuildInput &buildInput,
                                      const hiprtBuildOptions buildOptions,
                                      hiprtScene &outScene);

typedef hiprtError(thiprtCreateScenes)(hiprtContext context,
                                      uint32_t numScenes,
                                      const hiprtSceneBuildInput *buildInput,
                                      const hiprtBuildOptions buildOptions,
                                      hiprtScene **outScene);

typedef hiprtError(thiprtDestroyScene)(hiprtContext context, hiprtScene outScene);
typedef hiprtError(thiprtDestroyScenes)( hiprtContext context, uint32_t numScenes,hiprtScene *scene );
typedef hiprtError(thiprtBuildScene)(hiprtContext context,
                                     hiprtBuildOperation buildOperation,
                                     const hiprtSceneBuildInput &buildInput,
                                     const hiprtBuildOptions buildOptions,
                                     hiprtDevicePtr temporaryBuffer,
                                     hiprtApiStream stream,
                                     hiprtScene outScene);
typedef hiprtError(thiprtBuildScenes)(hiprtContext context,
                                     uint32_t numScenes,
                                     hiprtBuildOperation buildOperation,
                                     const hiprtSceneBuildInput *buildInput,
                                     const hiprtBuildOptions *buildOptions,
                                     hiprtDevicePtr temporaryBuffer,
                                     hiprtApiStream stream,
                                     hiprtScene *outScene);
typedef hiprtError(thiprtGetSceneBuildTemporaryBufferSize)(
    hiprtContext context,
    const hiprtSceneBuildInput &buildInput,
    const hiprtBuildOptions buildOptions,
    size_t &outSize);

typedef hiprtError(thiprtGetScenesBuildTemporaryBufferSize)(
    hiprtContext context,
    uint32_t numScenes,
    const hiprtSceneBuildInput *buildInput,
    const hiprtBuildOptions buildOptions,
    size_t &outSize);

typedef hiprtError(thiprtCompactScene)( hiprtContext context, hiprtApiStream stream, hiprtScene sceneIn, hiprtScene& sceneOut );

typedef hiprtError(thiprtCompactScenes)(
	hiprtContext context, uint32_t numScenes, hiprtApiStream stream, hiprtScene* scenesIn, hiprtScene** scenesOut );

typedef hiprtError(thiprtCreateFuncTable)(hiprtContext context,
                                          hiprtuint32_t numGeomTypes,
                                          hiprtuint32_t numRayTypes,
                                          hiprtFuncTable &outFuncTable);
typedef hiprtError(thiprtSetFuncTable)(hiprtContext context,
                                       hiprtFuncTable funcTable,
                                       hiprtuint32_t geomType,
                                       hiprtuint32_t rayType,
                                        hiprtFuncDataSet set);


typedef hiprtError (thiprtCreateGlobalStackBuffer)(hiprtContext context, const hiprtGlobalStackBufferInput& input, hiprtGlobalStackBuffer& stackBufferOut );
typedef hiprtError (thiprtDestroyGlobalStackBuffer)( hiprtContext context, hiprtGlobalStackBuffer stackBuffer );

typedef hiprtError(thiprtDestroyFuncTable)(hiprtContext context,
                                           hiprtFuncTable funcTable);
typedef void(thiprtSetLogLevel)( hiprtLogLevel level );

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
extern thiprtCreateGlobalStackBuffer *hiprtCreateGlobalStackBuffer;
extern thiprtDestroyGlobalStackBuffer *hiprtDestroyGlobalStackBuffer;
extern thiprtDestroyFuncTable *hiprtDestroyFuncTable;
extern thiprtSetLogLevel *hiprtSetLogLevel;

/* HIPEW API. */

bool hiprtewInit();

#endif  /* __HIPRTEW_H__ */
