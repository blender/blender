void node_holdout(out Closure result)
{
  result = CLOSURE_DEFAULT;
#ifndef VOLUMETRICS
  result.holdout = 1.0;
  result.flag = CLOSURE_HOLDOUT_FLAG;
#endif
}
