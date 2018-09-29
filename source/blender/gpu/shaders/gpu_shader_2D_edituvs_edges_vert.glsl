
uniform mat4 ModelViewProjectionMatrix;
uniform vec4 edgeColor;
uniform vec4 selectColor;

in vec2 pos;
in int flag;

#ifdef SMOOTH_COLOR
noperspective out vec4 finalColor;
#else
flat out vec4 finalColor;
#endif

#define VERTEX_SELECT (1 << 0)
#define EDGE_SELECT (1 << 4)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

#ifdef SMOOTH_COLOR
	bool is_select = (flag & VERTEX_SELECT) != 0;
#else
	bool is_select = (flag & EDGE_SELECT) != 0;
#endif

	finalColor = (is_select) ? selectColor : edgeColor;
}
