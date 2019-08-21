
#ifdef UNIFORM_ID
uniform int id;
#  define _id floatBitsToUint(intBitsToFloat(id))
#else
flat in uint id;
#  define _id id
#endif

out uint fragColor;

void main()
{
  fragColor = _id;
}
