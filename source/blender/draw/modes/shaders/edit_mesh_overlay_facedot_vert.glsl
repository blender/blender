
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

#ifdef USE_WORLD_CLIP_PLANES
uniform vec4 WorldClipPlanes[6];
uniform int  WorldClipPlanesLen;
#endif

in vec3 pos;
in vec4 norAndFlag;

flat out int isSelected;

#ifdef VERTEX_FACING
uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat3 NormalMatrix;

flat out float facing;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	/* Bias Facedot Z position in clipspace. */
	gl_Position.z -= 0.00035;
	gl_PointSize = sizeFaceDot;
	isSelected = int(norAndFlag.w);
#ifdef VERTEX_FACING
	vec3 view_normal = normalize(NormalMatrix * norAndFlag.xyz);
	vec3 view_vec = (ProjectionMatrix[3][3] == 0.0)
		? normalize((ModelViewMatrix * vec4(pos, 1.0)).xyz)
		: vec3(0.0, 0.0, 1.0);
	facing = dot(view_vec, view_normal);
#endif

#ifdef USE_WORLD_CLIP_PLANES
	{
		vec3 worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;
		for (int i = 0; i < WorldClipPlanesLen; i++) {
			gl_ClipDistance[i] = dot(WorldClipPlanes[i].xyz, worldPosition) + WorldClipPlanes[i].w;
		}
	}
#endif
}
