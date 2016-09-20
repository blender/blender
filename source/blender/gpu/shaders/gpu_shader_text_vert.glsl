
// TODO(merwin):
// - uniform color, not per vertex
// - generic attrib inputs (2D pos, tex coord)

#if __VERSION__ == 120
  flat varying vec4 color;
  noperspective varying vec2 texcoord;
#else
  flat out vec4 color;
  noperspective out vec2 texcoord;
#endif

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	color = gl_Color;
	texcoord = gl_MultiTexCoord0.st;
}
