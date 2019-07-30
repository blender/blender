
#ifdef UNIFORM_ID
uniform int id;
#  define id floatBitsToUint(intBitsToFloat(id))
#else
flat in uint id;
#endif

out uint fragColor;

void main()
{
  fragColor = id;
}
