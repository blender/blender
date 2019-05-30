
/* Should be 2 bits only [0..3]. */
uniform int outlineId;

flat in int objectId;

/* using uint because 16bit uint can contain more ids than int. */
out uint outId;

/* Replace top 2 bits (of the 16bit output) by outlineId.
 * This leaves 16K different IDs to create outlines between objects.
 * SHIFT = (32 - (16 - 2)) */
#define SHIFT 18u

void main()
{
  outId = (uint(outlineId) << 14u) | ((uint(objectId) << SHIFT) >> SHIFT);
}
