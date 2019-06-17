
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

/* TODO: Port drawing to draw manager and
 * remove constants duplications. */
#define VERT_UV_SELECT (1 << 3)
#define EDGE_UV_SELECT (1 << 5)

void main()
{
#ifdef SMOOTH_COLOR
  bool is_select = (flag & VERT_UV_SELECT) != 0;
#else
  bool is_select = (flag & EDGE_UV_SELECT) != 0;
#endif

  gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
  gl_Position.z = float(!is_select);

  finalColor = (is_select) ? selectColor : edgeColor;
}
