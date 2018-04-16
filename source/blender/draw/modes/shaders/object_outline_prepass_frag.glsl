uniform int callId;
uniform int baseId;

/* using uint because 16bit uint can contain more ids than int. */
out uint outId;

void main()
{
	outId = uint(baseId + callId);
}
