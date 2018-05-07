uniform mat4 ModelViewProjectionMatrix;
#ifdef V3D_LIGHTING_STUDIO
uniform mat3 NormalMatrix;
#endif /* V3D_LIGHTING_STUDIO */

in vec3 pos;
#ifdef V3D_LIGHTING_STUDIO
in vec3 nor;
#endif /* V3D_LIGHTING_STUDIO */
#ifdef OB_TEXTURE
in vec2 uv;
#endif

#ifdef V3D_LIGHTING_STUDIO
out vec3 normal_viewport;
#endif /* V3D_LIGHTING_STUDIO */
#ifdef OB_TEXTURE
out vec2 uv_interp;
#endif

void main()
{
#ifdef OB_TEXTURE
	uv_interp = uv;
#endif
#ifdef V3D_LIGHTING_STUDIO
	normal_viewport = normalize(NormalMatrix * nor);
#endif /* V3D_LIGHTING_STUDIO */
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
