
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
flat out ivec3 flag;
flat out vec4 faceColor;
flat out int clipCase;

/* See fragment shader */
noperspective out vec4 eData1;
flat out vec4 eData2;

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void main()
{
	clipCase = 0;

	vec4 pPos = ModelViewProjectionMatrix * vec4(pos, 1.0);

	/* there is no face */
	faceColor = vec4(0.0);

	/* only verterx position 0 is used */
	eData1 = eData2 = vec4(1e10);
	eData2.zw = proj(pPos);

	flag = ivec3(0);
	flag[0] = (data.x << 8);

	gl_PointSize = sizeEdgeFix;
	gl_Position = pPos;
}
