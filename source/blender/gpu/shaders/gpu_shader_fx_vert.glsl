varying vec4 uvcoordsvar;

//very simple shader for full screen FX, just pass values on

void main()
{
	uvcoordsvar = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
}
