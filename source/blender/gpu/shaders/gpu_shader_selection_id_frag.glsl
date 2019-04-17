
#ifdef UNIFORM_ID
uniform uint id;
#else
flat in uint id;
#endif

out uint fragColor;

void main()
{
  fragColor = id;
}
