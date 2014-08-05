extra_code = """
static void path_join(const char *path1,
                      const char *path2,
                      int maxlen,
                      char *result) {
#if defined(WIN32) || defined(_WIN32)
  const char separator = '\\\\';
#else
  const char separator = '/';
#endif
  int n = snprintf(result, maxlen, "%s%c%s", path1, separator, path2);
  if (n != -1 && n < maxlen) {
    result[n] = '\\0';
  }
  else {
    result[maxlen - 1] = '\\0';
  }
}

static int path_exists(const char *path) {
  struct stat st;
  if (stat(path, &st)) {
    return 0;
  }
  return 1;
}

const char *cuewCompilerPath(void) {
#ifdef _WIN32
  const char *defaultpaths[] = {"C:/CUDA/bin", NULL};
  const char *executable = "nvcc.exe";
#else
  const char *defaultpaths[] = {
    "/Developer/NVIDIA/CUDA-5.0/bin",
    "/usr/local/cuda-5.0/bin",
    "/usr/local/cuda/bin",
    "/Developer/NVIDIA/CUDA-6.0/bin",
    "/usr/local/cuda-6.0/bin",
    "/Developer/NVIDIA/CUDA-5.5/bin",
    "/usr/local/cuda-5.5/bin",
    NULL};
  const char *executable = "nvcc";
#endif
  int i;

  const char *binpath = getenv("CUDA_BIN_PATH");

  static char nvcc[65536];

  if (binpath) {
    path_join(binpath, executable, sizeof(nvcc), nvcc);
    if (path_exists(nvcc))
      return nvcc;
  }

  for (i = 0; defaultpaths[i]; ++i) {
    path_join(defaultpaths[i], executable, sizeof(nvcc), nvcc);
    if (path_exists(nvcc))
      return nvcc;
  }

#ifndef _WIN32
  {
    FILE *handle = popen("which nvcc", "r");
    if (handle) {
      char buffer[4096] = {0};
      int len = fread(buffer, 1, sizeof(buffer) - 1, handle);
      buffer[len] = '\\0';
      pclose(handle);

      if (buffer[0])
        return "nvcc";
    }
  }
#endif

  return NULL;
}

int cuewCompilerVersion(void) {
  const char *path = cuewCompilerPath();
  const char *marker = "Cuda compilation tools, release ";
  FILE *pipe;
  int major, minor;
  char *versionstr;
  char buf[128];
  char output[65536] = "\\0";
  char command[65536] = "\\0";

  if (path == NULL)
    return 0;

  /* get --version output */
  strncpy(command, path, sizeof(command));
  strncat(command, " --version", sizeof(command) - strlen(path));
  pipe = popen(command, "r");
  if (!pipe) {
    fprintf(stderr, "CUDA: failed to run compiler to retrieve version");
    return 0;
  }

  while (!feof(pipe)) {
    if (fgets(buf, sizeof(buf), pipe) != NULL) {
      strncat(output, buf, sizeof(output) - strlen(output));
    }
  }

  pclose(pipe);

  /* parse version number */
  versionstr = strstr(output, marker);
  if (versionstr == NULL) {
    fprintf(stderr, "CUDA: failed to find version number in:\\n\\n%s\\n", output);
    return 0;
  }
  versionstr += strlen(marker);

  if (sscanf(versionstr, "%d.%d", &major, &minor) < 2) {
    fprintf(stderr, "CUDA: failed to parse version number from:\\n\\n%s\\n", output);
    return 0;
  }

  return 10 * major + minor;
}
"""
