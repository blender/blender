void squeeze(float val, float width, float center, out float outval)
{
  outval = 1.0 / (1.0 + pow(2.71828183, -((val - center) * width)));
}
