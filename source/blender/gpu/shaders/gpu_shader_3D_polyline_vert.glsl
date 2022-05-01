
void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#ifndef UNIFORM
  interp.color = color;
#endif
#ifdef CLIP
  interp.clip = dot(ModelMatrix * vec4(pos, 1.0), ClipPlane);
#endif
}
