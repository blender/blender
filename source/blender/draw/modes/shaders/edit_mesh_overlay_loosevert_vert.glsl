
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewportSize;

in vec3 pos;
in ivec4 data;

/* these are the same for all vertices
 * and does not need interpolation */
flat out vec3 edgesCrease;
flat out vec3 edgesBweight;
flat out vec4 faceColor;
flat out ivec3 flag;
#ifdef VERTEX_SELECTION
out vec3 vertexColor;
#endif

/* See fragment shader */
flat out vec2 ssPos[3];

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void main()
{
	edgesCrease = vec3(0.0);
	edgesBweight = vec3(0.0);

	vec4 pPos = ModelViewProjectionMatrix * vec4(pos, 1.0);

	/* there is no face */
	faceColor = vec4(0.0);

#ifdef VERTEX_SELECTION
	vertexColor = vec3(0.0);
#endif

	ssPos[0] = ssPos[1] = ssPos[2] = proj(pPos);
	flag[0] = flag[1] = flag[2] = (data.x << 8);

	gl_PointSize = sizeEdgeFix;
	gl_Position = pPos;
}
