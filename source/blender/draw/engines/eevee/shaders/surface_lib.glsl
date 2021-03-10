/** This describe the entire interface of the shader.  */

#define SURFACE_INTERFACE \
  vec3 worldPosition; \
  vec3 viewPosition; \
  vec3 worldNormal; \
  vec3 viewNormal;

#if defined(STEP_RESOLVE) || defined(STEP_RAYTRACE)
/* SSR will set these global variables itself.
 * Also make false positive compiler warnings disapear by setting values. */
vec3 worldPosition = vec3(0);
vec3 viewPosition = vec3(0);
vec3 worldNormal = vec3(0);
vec3 viewNormal = vec3(0);

#elif defined(GPU_GEOMETRY_SHADER)
in ShaderStageInterface{SURFACE_INTERFACE} dataIn[];

out ShaderStageInterface{SURFACE_INTERFACE} dataOut;

#  define PASS_SURFACE_INTERFACE(vert) \
    dataOut.worldPosition = dataIn[vert].worldPosition; \
    dataOut.viewPosition = dataIn[vert].viewPosition; \
    dataOut.worldNormal = dataIn[vert].worldNormal; \
    dataOut.viewNormal = dataIn[vert].viewNormal;

#else /* GPU_VERTEX_SHADER || GPU_FRAGMENT_SHADER*/

IN_OUT ShaderStageInterface{SURFACE_INTERFACE};

#endif

#ifdef HAIR_SHADER
IN_OUT ShaderHairInterface
{
  /* world space */
  vec3 hairTangent;
  float hairThickTime;
  float hairThickness;
  float hairTime;
  flat int hairStrandID;
};
#endif
