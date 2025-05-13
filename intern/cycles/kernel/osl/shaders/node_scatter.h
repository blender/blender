/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "stdcycles.h"

struct MieParameters {
  float g_HG;
  float g_D;
  float alpha;
  float mixture;
};

MieParameters phase_mie_fitted_parameters(float Diameter)
{
  float d = max(Diameter, 0.0);
  if (d <= 0.1) {
    /* Eq (11 - 14). */
    return {13.8 * d * d, 1.1456 * d * sin(9.29044 * d), 250.0, 0.252977 - 312.983 * pow(d, 4.3)};
  }

  if (d < 1.5) {
    /* Eq (15 - 18). */
    float log_d = log(d);
    float a = (log_d - 0.238604) * (log_d + 1.00667);
    float b = 0.507522 - 0.15677 * log_d;
    float c = 1.19692 * cos(a / b) + 1.37932 * log_d + 0.0625835;
    return {0.862 - 0.143 * log_d * log_d,
            0.379685 * cos(c) + 0.344213,
            250.0,
            0.146209 * cos(3.38707 * log_d + 2.11193) + 0.316072 + 0.0778917 * log_d};
  }

  if (d < 5.0) {
    /* Eq (19 - 22). */
    float log_d = log(d);
    float temp = cos(5.68947 * (log(log_d) - 0.0292149));
    return {0.0604931 * log(log_d) + 0.940256,
            0.500411 - (0.081287 / (-2.0 * log_d + tan(log_d) + 1.27551)),
            7.30354 * log_d + 6.31675,
            0.026914 * (log_d - temp) + 0.3764};
  }

  /* Eq (7 - 10). */
  return {exp(-0.0990567 / (d - 1.67154)),
          exp(-2.20679 / (d + 3.91029) - 0.428934),
          exp(3.62489 - 8.29288 / (d + 5.52825)),
          exp(-0.599085 / (d - 0.641583) - 0.665888)};
}

closure color
scatter(string phase, float Anisotropy, float IOR, float Backscatter, float Alpha, float Diameter)
{
  closure color scatter = 0;
  if (phase == "Fournier-Forand") {
    scatter = fournier_forand(Backscatter, IOR);
  }
  else if (phase == "Draine") {
    scatter = draine(Anisotropy, Alpha);
  }
  else if (phase == "Rayleigh") {
    scatter = rayleigh();
  }
  else if (phase == "Mie") {
    /* Approximation of Mie phase function for water droplets using a mix of Draine and H-G.
     * See `kernel/svm/closure.h` for details. */
    MieParameters param = phase_mie_fitted_parameters(Diameter);
    scatter = mix(henyey_greenstein(param.g_HG), draine(param.g_D, param.alpha), param.mixture);
  }
  else {
    scatter = henyey_greenstein(Anisotropy);
  }
  return scatter;
}
