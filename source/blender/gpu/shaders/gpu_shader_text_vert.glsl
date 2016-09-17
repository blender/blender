
// TODO(merwin):
// - use modern GLSL
// - uniform color, not per vertex
// - generic attrib inputs (2D pos, tex coord)

flat varying vec4 color;
varying vec2 texcoord;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	color = gl_Color;
	texcoord = (gl_TextureMatrix[0] * gl_MultiTexCoord0).st;
}
