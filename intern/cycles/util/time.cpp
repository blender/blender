/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util/time.h"

#include <stdlib.h>

#if !defined(_WIN32)
#  include <sys/time.h>
#  include <unistd.h>
#endif

#include "util/math.h"
#include "util/string.h"
#include "util/windows.h"

CCL_NAMESPACE_BEGIN

#ifdef _WIN32
double time_dt()
{
  __int64 frequency, counter;

  QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);
  QueryPerformanceCounter((LARGE_INTEGER *)&counter);

  return (double)counter / (double)frequency;
}

void time_sleep(double t)
{
  Sleep((int)(t * 1000));
}
#else
double time_dt()
{
  struct timeval now;
  gettimeofday(&now, NULL);

  return now.tv_sec + now.tv_usec * 1e-6;
}

/* sleep t seconds */
void time_sleep(double t)
{
  /* get whole seconds */
  int s = (int)t;

  if (s >= 1) {
    sleep(s);

    /* adjust parameter to remove whole seconds */
    t -= s;
  }

  /* get microseconds */
  int us = (int)(t * 1e6);
  if (us > 0)
    usleep(us);
}
#endif

/* Time in format "hours:minutes:seconds.hundreds" */

string time_human_readable_from_seconds(const double seconds)
{
  const int h = (((int)seconds) / (60 * 60));
  const int m = (((int)seconds) / 60) % 60;
  const int s = (((int)seconds) % 60);
  const int r = (((int)(seconds * 100)) % 100);

  if (h > 0) {
    return string_printf("%.2d:%.2d:%.2d.%.2d", h, m, s, r);
  }
  else {
    return string_printf("%.2d:%.2d.%.2d", m, s, r);
  }
}

double time_human_readable_to_seconds(const string &time_string)
{
  /* Those are multiplies of a corresponding token surrounded by : in the
   * time string, which denotes how to convert value to seconds.
   * Effectively: seconds, minutes, hours, days in seconds. */
  const int multipliers[] = {1, 60, 60 * 60, 24 * 60 * 60};
  const int num_multiplies = sizeof(multipliers) / sizeof(*multipliers);
  if (time_string.empty()) {
    return 0.0;
  }
  double result = 0.0;
  /* Split fractions of a second from the encoded time. */
  vector<string> fraction_tokens;
  string_split(fraction_tokens, time_string, ".", false);
  const int num_fraction_tokens = fraction_tokens.size();
  if (num_fraction_tokens == 0) {
    /* Time string is malformed. */
    return 0.0;
  }
  else if (fraction_tokens.size() == 1) {
    /* There is no fraction of a second specified, the rest of the code
     * handles this normally. */
  }
  else if (fraction_tokens.size() == 2) {
    result = atof(fraction_tokens[1].c_str());
    result *= pow(0.1, fraction_tokens[1].length());
  }
  else {
    /* This is not a valid string, the result can not be reliable. */
    return 0.0;
  }
  /* Split hours, minutes and seconds.
   * Hours part is optional. */
  vector<string> tokens;
  string_split(tokens, fraction_tokens[0], ":", false);
  const int num_tokens = tokens.size();
  if (num_tokens > num_multiplies) {
    /* Can not reliably represent the value. */
    return 0.0;
  }
  for (int i = 0; i < num_tokens; ++i) {
    result += atoi(tokens[num_tokens - i - 1].c_str()) * multipliers[i];
  }
  return result;
}

CCL_NAMESPACE_END
