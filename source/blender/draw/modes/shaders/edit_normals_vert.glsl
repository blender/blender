
uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;
uniform mat4 ProjectionMatrix;
uniform mat4 ModelMatrix;
uniform float normalSize;

#ifdef USE_WORLD_CLIP_PLANES
uniform vec4 WorldClipPlanes[6];
uniform int  WorldClipPlanesLen;
#endif

in vec3 pos;

#ifdef LOOP_NORMALS
in vec3 lnor;
#define nor lnor

#elif defined(FACE_NORMALS)
in vec4 norAndFlag;
#define nor norAndFlag.xyz
#else

in vec3 vnor;
#define nor vnor
#endif

flat out vec4 v1;
flat out vec4 v2;

void main()
{
	v1 = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vec3 n = normalize(NormalMatrix * nor); /* viewspace */
	v2 = v1 + ProjectionMatrix * vec4(n * normalSize, 0.0);
#ifdef USE_WORLD_CLIP_PLANES
	{
		vec3 worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;
		for (int i = 0; i < WorldClipPlanesLen; i++) {
			gl_ClipDistance[i] = dot(WorldClipPlanes[i].xyz, worldPosition) + WorldClipPlanes[i].w;
		}
	}
#endif
}
