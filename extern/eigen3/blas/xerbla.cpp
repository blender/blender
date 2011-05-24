
#include <iostream>

#ifdef __cplusplus
extern "C"
{
#endif

int xerbla_(const char * msg, int *info, int)
{
  std::cerr << "Eigen BLAS ERROR #" << *info << ": " << msg << "\n";
  return 0;
}

#ifdef __cplusplus
}
#endif
