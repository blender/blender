# pcg32
This is a tiny self-contained C++ implementation of the PCG32 random number
based on code by Melissa O'Neill available at http://www.pcg-random.org.

I decided to make put together my own version because the official small
implementation lacks a C++ interface and various important features (e.g.
rewind/difference support, shuffling, floating point sample generation), and
the big C++ version is extremely large and uses very recent language features
that are not yet supported by all compilers.
