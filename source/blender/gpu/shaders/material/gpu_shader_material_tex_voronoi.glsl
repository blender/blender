void node_tex_voronoi(vec3 co,
                      float scale,
                      float exponent,
                      float coloring,
                      float metric,
                      float feature,
                      out vec4 color,
                      out float fac)
{
  vec3 p = co * scale;
  int xx, yy, zz, xi, yi, zi;
  vec4 da = vec4(1e10);
  vec3 pa[4] = vec3[4](vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0));

  xi = floor_to_int(p[0]);
  yi = floor_to_int(p[1]);
  zi = floor_to_int(p[2]);

  for (xx = xi - 1; xx <= xi + 1; xx++) {
    for (yy = yi - 1; yy <= yi + 1; yy++) {
      for (zz = zi - 1; zz <= zi + 1; zz++) {
        vec3 ip = vec3(xx, yy, zz);
        vec3 vp = cellnoise_color(ip);
        vec3 pd = p - (vp + ip);

        float d = 0.0;
        if (metric == 0.0) { /* SHD_VORONOI_DISTANCE 0 */
          d = dot(pd, pd);
        }
        else if (metric == 1.0) { /* SHD_VORONOI_MANHATTAN 1 */
          d = abs(pd[0]) + abs(pd[1]) + abs(pd[2]);
        }
        else if (metric == 2.0) { /* SHD_VORONOI_CHEBYCHEV 2 */
          d = max(abs(pd[0]), max(abs(pd[1]), abs(pd[2])));
        }
        else if (metric == 3.0) { /* SHD_VORONOI_MINKOWSKI 3 */
          d = pow(pow(abs(pd[0]), exponent) + pow(abs(pd[1]), exponent) +
                      pow(abs(pd[2]), exponent),
                  1.0 / exponent);
        }

        vp += vec3(xx, yy, zz);
        if (d < da[0]) {
          da.yzw = da.xyz;
          da[0] = d;

          pa[3] = pa[2];
          pa[2] = pa[1];
          pa[1] = pa[0];
          pa[0] = vp;
        }
        else if (d < da[1]) {
          da.zw = da.yz;
          da[1] = d;

          pa[3] = pa[2];
          pa[2] = pa[1];
          pa[1] = vp;
        }
        else if (d < da[2]) {
          da[3] = da[2];
          da[2] = d;

          pa[3] = pa[2];
          pa[2] = vp;
        }
        else if (d < da[3]) {
          da[3] = d;
          pa[3] = vp;
        }
      }
    }
  }

  if (coloring == 0.0) {
    /* Intensity output */
    if (feature == 0.0) { /* F1 */
      fac = abs(da[0]);
    }
    else if (feature == 1.0) { /* F2 */
      fac = abs(da[1]);
    }
    else if (feature == 2.0) { /* F3 */
      fac = abs(da[2]);
    }
    else if (feature == 3.0) { /* F4 */
      fac = abs(da[3]);
    }
    else if (feature == 4.0) { /* F2F1 */
      fac = abs(da[1] - da[0]);
    }
    color = vec4(fac, fac, fac, 1.0);
  }
  else {
    /* Color output */
    vec3 col = vec3(fac, fac, fac);
    if (feature == 0.0) { /* F1 */
      col = pa[0];
    }
    else if (feature == 1.0) { /* F2 */
      col = pa[1];
    }
    else if (feature == 2.0) { /* F3 */
      col = pa[2];
    }
    else if (feature == 3.0) { /* F4 */
      col = pa[3];
    }
    else if (feature == 4.0) { /* F2F1 */
      col = abs(pa[1] - pa[0]);
    }

    color = vec4(cellnoise_color(col), 1.0);
    fac = (color.x + color.y + color.z) * (1.0 / 3.0);
  }
}
