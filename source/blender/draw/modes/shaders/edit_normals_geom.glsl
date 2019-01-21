
layout(points) in;
layout(line_strip, max_vertices=2) out;

#ifdef USE_WORLD_CLIP_PLANES
uniform int  WorldClipPlanesLen;
#endif

flat in vec4 v1[1];
flat in vec4 v2[1];

void main()
{
	for (int v = 0; v < 2; v++) {
		gl_Position = (v == 0) ? v1[0] : v2[0];
#ifdef USE_WORLD_CLIP_PLANES
		for (int i = 0; i < WorldClipPlanesLen; i++) {
			gl_ClipDistance[i] = gl_in[0].gl_ClipDistance[i];
		}
#endif
		EmitVertex();
	}
	EndPrimitive();
}
