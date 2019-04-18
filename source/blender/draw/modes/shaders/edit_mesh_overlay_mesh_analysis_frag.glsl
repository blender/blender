out vec4 fragColor;

#ifdef FACE_COLOR
flat in vec4 weightColor;
#endif

#ifdef VERTEX_COLOR
in vec4 weightColor;
#endif

void main()
{
  fragColor = weightColor;
}
