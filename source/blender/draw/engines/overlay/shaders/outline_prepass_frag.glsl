
flat in uint objectId;

/* using uint because 16bit uint can contain more ids than int. */
out uint outId;

void main()
{
  outId = objectId;
}
