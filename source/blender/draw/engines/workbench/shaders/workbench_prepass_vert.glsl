uniform mat4 ModelViewProjectionMatrix;
#ifdef V3D_LIGHTING_STUDIO
uniform mat3 NormalMatrix;
#endif /* V3D_LIGHTING_STUDIO */

in vec3 pos;
#ifdef V3D_LIGHTING_STUDIO
in vec3 nor;
#endif /* V3D_LIGHTING_STUDIO */

#ifdef V3D_LIGHTING_STUDIO
out vec3 normal_viewport;
#endif /* V3D_LIGHTING_STUDIO */

void main()
{
#ifdef V3D_LIGHTING_STUDIO
	normal_viewport = normalize(NormalMatrix * nor);
#endif /* V3D_LIGHTING_STUDIO */
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
