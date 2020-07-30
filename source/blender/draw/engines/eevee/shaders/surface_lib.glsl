/** This describe the entire interface of the shader.  */

/* Samplers */
uniform sampler2D colorBuffer;
uniform sampler2D depthBuffer;

/* Uniforms */
uniform float refractionDepth;

#define SURFACE_INTERFACE \
  vec3 worldPosition; \
  vec3 viewPosition; \
  vec3 worldNormal; \
  vec3 viewNormal;

#ifdef GPU_GEOMETRY_SHADER
in ShaderStageInterface{SURFACE_INTERFACE} dataIn[];

out ShaderStageInterface{SURFACE_INTERFACE} dataOut;

#  define PASS_SURFACE_INTERFACE(vert) \
    dataOut.worldPosition = dataIn[vert].worldPosition; \
    dataOut.viewPosition = dataIn[vert].viewPosition; \
    dataOut.worldNormal = dataIn[vert].worldNormal; \
    dataOut.viewNormal = dataIn[vert].viewNormal;

#else

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
