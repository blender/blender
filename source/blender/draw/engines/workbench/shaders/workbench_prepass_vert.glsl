uniform mat4 ModelViewProjectionMatrix;
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
uniform mat3 NormalMatrix;
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */

in vec3 pos;
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
in vec3 nor;
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */
#ifdef OB_TEXTURE
in vec2 uv;
#endif

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
out vec3 normal_viewport;
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */
#ifdef OB_TEXTURE
out vec2 uv_interp;
#endif

void main()
{
#ifdef OB_TEXTURE
	uv_interp = uv;
#endif
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
	normal_viewport = normalize(NormalMatrix * nor);
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
