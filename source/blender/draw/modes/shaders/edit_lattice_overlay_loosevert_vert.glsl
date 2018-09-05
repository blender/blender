
/* Draw Lattice Vertices */

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewportSize;
uniform ivec4 dataMask = ivec4(0xFF);

in vec3 pos;
in int data;

/* these are the same for all vertices
 * and does not need interpolation */
flat out int vertFlag;
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

	/* only vertex position 0 is used */
	eData1 = eData2 = vec4(1e10);
	eData2.zw = proj(pPos);

	vertFlag = data & dataMask;

	gl_PointSize = sizeVertex;
	gl_Position = pPos;
}
