/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include "device/device.h"
#include "scene/camera.h"
#include "scene/integrator.h"
#include "scene/scene.h"
#include "session/buffers.h"
#include "session/session.h"

#include "util/args.h"
#include "util/foreach.h"
#include "util/function.h"
#include "util/image.h"
#include "util/log.h"
#include "util/path.h"
#include "util/progress.h"
#include "util/string.h"
#include "util/time.h"
#include "util/transform.h"
#include "util/unique_ptr.h"
#include "util/version.h"

#ifdef WITH_USD
#  include "hydra/file_reader.h"
#endif

#include "app/cycles_xml.h"
#include "app/oiio_output_driver.h"

#ifdef WITH_CYCLES_STANDALONE_GUI
#  include "opengl/display_driver.h"
#  include "opengl/window.h"
#endif

CCL_NAMESPACE_BEGIN

struct Options {
  Session *session;
  Scene *scene;
  string filepath;
  int width, height;
  SceneParams scene_params;
  SessionParams session_params;
  bool quiet;
  bool show_help, interactive, pause;
  string output_filepath;
  string output_pass;
} options;

static void session_print(const string &str)
{
  /* print with carriage return to overwrite previous */
  printf("\r%s", str.c_str());

  /* add spaces to overwrite longer previous print */
  static int maxlen = 0;
  int len = str.size();
  maxlen = max(len, maxlen);

  for (int i = len; i < maxlen; i++) {
    printf(" ");
  }

  /* flush because we don't write an end of line */
  fflush(stdout);
}

static void session_print_status()
{
  string status, substatus;

  /* get status */
  double progress = options.session->progress.get_progress();
  options.session->progress.get_status(status, substatus);

  if (substatus != "") {
    status += ": " + substatus;
  }

  /* print status */
  status = string_printf("Progress %05.2f   %s", (double)progress * 100, status.c_str());
  session_print(status);
}

static BufferParams &session_buffer_params()
{
  static BufferParams buffer_params;
  buffer_params.width = options.width;
  buffer_params.height = options.height;
  buffer_params.full_width = options.width;
  buffer_params.full_height = options.height;

  return buffer_params;
}

static void scene_init()
{
  options.scene = options.session->scene;

  /* Read XML or USD */
#ifdef WITH_USD
  if (!string_endswith(string_to_lower(options.filepath), ".xml")) {
    HD_CYCLES_NS::HdCyclesFileReader::read(options.session, options.filepath.c_str());
  }
  else
#endif
  {
    xml_read_file(options.scene, options.filepath.c_str());
  }

  /* Camera width/height override? */
  if (!(options.width == 0 || options.height == 0)) {
    options.scene->camera->set_full_width(options.width);
    options.scene->camera->set_full_height(options.height);
  }
  else {
    options.width = options.scene->camera->get_full_width();
    options.height = options.scene->camera->get_full_height();
  }

  /* Calculate Viewplane */
  options.scene->camera->compute_auto_viewplane();
}

static void session_init()
{
  options.output_pass = "combined";
  options.session = new Session(options.session_params, options.scene_params);

#ifdef WITH_CYCLES_STANDALONE_GUI
  if (!options.session_params.background) {
    options.session->set_display_driver(make_unique<OpenGLDisplayDriver>(
        window_opengl_context_enable, window_opengl_context_disable));
  }
#endif

  if (!options.output_filepath.empty()) {
    options.session->set_output_driver(make_unique<OIIOOutputDriver>(
        options.output_filepath, options.output_pass, session_print));
  }

  if (options.session_params.background && !options.quiet) {
    options.session->progress.set_update_callback(function_bind(&session_print_status));
  }
#ifdef WITH_CYCLES_STANDALONE_GUI
  else
    options.session->progress.set_update_callback(function_bind(&window_redraw));
#endif

  /* load scene */
  scene_init();

  /* add pass for output. */
  Pass *pass = options.scene->create_node<Pass>();
  pass->set_name(ustring(options.output_pass.c_str()));
  pass->set_type(PASS_COMBINED);

  options.session->reset(options.session_params, session_buffer_params());
  options.session->start();
}

static void session_exit()
{
  if (options.session) {
    delete options.session;
    options.session = NULL;
  }

  if (options.session_params.background && !options.quiet) {
    session_print("Finished Rendering.");
    printf("\n");
  }
}

#ifdef WITH_CYCLES_STANDALONE_GUI
static void display_info(Progress &progress)
{
  static double latency = 0.0;
  static double last = 0;
  double elapsed = time_dt();
  string str, interactive;

  latency = (elapsed - last);
  last = elapsed;

  double total_time, sample_time;
  string status, substatus;

  progress.get_time(total_time, sample_time);
  progress.get_status(status, substatus);
  double progress_val = progress.get_progress();

  if (substatus != "")
    status += ": " + substatus;

  interactive = options.interactive ? "On" : "Off";

  str = string_printf(
      "%s"
      "        Time: %.2f"
      "        Latency: %.4f"
      "        Progress: %05.2f"
      "        Average: %.4f"
      "        Interactive: %s",
      status.c_str(),
      total_time,
      latency,
      (double)progress_val * 100,
      sample_time,
      interactive.c_str());

  window_display_info(str.c_str());

  if (options.show_help)
    window_display_help();
}

static void display()
{
  options.session->draw();

  display_info(options.session->progress);
}

static void motion(int x, int y, int button)
{
  if (options.interactive) {
    Transform matrix = options.session->scene->camera->get_matrix();

    /* Translate */
    if (button == 0) {
      float3 translate = make_float3(x * 0.01f, -(y * 0.01f), 0.0f);
      matrix = matrix * transform_translate(translate);
    }

    /* Rotate */
    else if (button == 2) {
      float4 r1 = make_float4((float)x * 0.1f, 0.0f, 1.0f, 0.0f);
      matrix = matrix * transform_rotate(DEG2RADF(r1.x), make_float3(r1.y, r1.z, r1.w));

      float4 r2 = make_float4(y * 0.1f, 1.0f, 0.0f, 0.0f);
      matrix = matrix * transform_rotate(DEG2RADF(r2.x), make_float3(r2.y, r2.z, r2.w));
    }

    /* Update and Reset */
    options.session->scene->camera->set_matrix(matrix);
    options.session->scene->camera->need_flags_update = true;
    options.session->scene->camera->need_device_update = true;

    options.session->reset(options.session_params, session_buffer_params());
  }
}

static void resize(int width, int height)
{
  options.width = width;
  options.height = height;

  if (options.session) {
    /* Update camera */
    options.session->scene->camera->set_full_width(options.width);
    options.session->scene->camera->set_full_height(options.height);
    options.session->scene->camera->compute_auto_viewplane();
    options.session->scene->camera->need_flags_update = true;
    options.session->scene->camera->need_device_update = true;

    options.session->reset(options.session_params, session_buffer_params());
  }
}

static void keyboard(unsigned char key)
{
  /* Toggle help */
  if (key == 'h')
    options.show_help = !(options.show_help);

  /* Reset */
  else if (key == 'r')
    options.session->reset(options.session_params, session_buffer_params());

  /* Cancel */
  else if (key == 27)  // escape
    options.session->progress.set_cancel("Canceled");

  /* Pause */
  else if (key == 'p') {
    options.pause = !options.pause;
    options.session->set_pause(options.pause);
  }

  /* Interactive Mode */
  else if (key == 'i')
    options.interactive = !(options.interactive);

  /* Navigation */
  else if (options.interactive && (key == 'w' || key == 'a' || key == 's' || key == 'd')) {
    Transform matrix = options.session->scene->camera->get_matrix();
    float3 translate;

    if (key == 'w')
      translate = make_float3(0.0f, 0.0f, 0.1f);
    else if (key == 's')
      translate = make_float3(0.0f, 0.0f, -0.1f);
    else if (key == 'a')
      translate = make_float3(-0.1f, 0.0f, 0.0f);
    else if (key == 'd')
      translate = make_float3(0.1f, 0.0f, 0.0f);

    matrix = matrix * transform_translate(translate);

    /* Update and Reset */
    options.session->scene->camera->set_matrix(matrix);
    options.session->scene->camera->need_flags_update = true;
    options.session->scene->camera->need_device_update = true;

    options.session->reset(options.session_params, session_buffer_params());
  }

  /* Set Max Bounces */
  else if (options.interactive && (key == '0' || key == '1' || key == '2' || key == '3')) {
    int bounce;
    switch (key) {
      case '0':
        bounce = 0;
        break;
      case '1':
        bounce = 1;
        break;
      case '2':
        bounce = 2;
        break;
      case '3':
        bounce = 3;
        break;
      default:
        bounce = 0;
        break;
    }

    options.session->scene->integrator->set_max_bounce(bounce);

    options.session->reset(options.session_params, session_buffer_params());
  }
}
#endif

static int files_parse(int argc, const char *argv[])
{
  if (argc > 0) {
    options.filepath = argv[0];
  }

  return 0;
}

static void options_parse(int argc, const char **argv)
{
  options.width = 1024;
  options.height = 512;
  options.filepath = "";
  options.session = NULL;
  options.quiet = false;
  options.session_params.use_auto_tile = false;
  options.session_params.tile_size = 0;

  /* device names */
  string device_names = "";
  string devicename = "CPU";
  bool list = false;

  /* List devices for which support is compiled in. */
  vector<DeviceType> types = Device::available_types();
  foreach (DeviceType type, types) {
    if (device_names != "") {
      device_names += ", ";
    }

    device_names += Device::string_from_type(type);
  }

  /* shading system */
  string ssname = "svm";

  /* parse options */
  ArgParse ap;
  bool help = false, profile = false, debug = false, version = false;
  int verbosity = 1;

  ap.options("Usage: cycles [options] file.xml",
             "%*",
             files_parse,
             "",
             "--device %s",
             &devicename,
             ("Devices to use: " + device_names).c_str(),
#ifdef WITH_OSL
             "--shadingsys %s",
             &ssname,
             "Shading system to use: svm, osl",
#endif
             "--background",
             &options.session_params.background,
             "Render in background, without user interface",
             "--quiet",
             &options.quiet,
             "In background mode, don't print progress messages",
             "--samples %d",
             &options.session_params.samples,
             "Number of samples to render",
             "--output %s",
             &options.output_filepath,
             "File path to write output image",
             "--threads %d",
             &options.session_params.threads,
             "CPU Rendering Threads",
             "--width  %d",
             &options.width,
             "Window width in pixel",
             "--height %d",
             &options.height,
             "Window height in pixel",
             "--tile-size %d",
             &options.session_params.tile_size,
             "Tile size in pixels",
             "--list-devices",
             &list,
             "List information about all available devices",
             "--profile",
             &profile,
             "Enable profile logging",
#ifdef WITH_CYCLES_LOGGING
             "--debug",
             &debug,
             "Enable debug logging",
             "--verbose %d",
             &verbosity,
             "Set verbosity of the logger",
#endif
             "--help",
             &help,
             "Print help message",
             "--version",
             &version,
             "Print version number",
             NULL);

  if (ap.parse(argc, argv) < 0) {
    fprintf(stderr, "%s\n", ap.geterror().c_str());
    ap.usage();
    exit(EXIT_FAILURE);
  }

  if (debug) {
    util_logging_start();
    util_logging_verbosity_set(verbosity);
  }

  if (list) {
    vector<DeviceInfo> devices = Device::available_devices();
    printf("Devices:\n");

    foreach (DeviceInfo &info, devices) {
      printf("    %-10s%s%s\n",
             Device::string_from_type(info.type).c_str(),
             info.description.c_str(),
             (info.display_device) ? " (display)" : "");
    }

    exit(EXIT_SUCCESS);
  }
  else if (version) {
    printf("%s\n", CYCLES_VERSION_STRING);
    exit(EXIT_SUCCESS);
  }
  else if (help || options.filepath == "") {
    ap.usage();
    exit(EXIT_SUCCESS);
  }

  options.session_params.use_profiling = profile;

  if (ssname == "osl") {
    options.scene_params.shadingsystem = SHADINGSYSTEM_OSL;
  }
  else if (ssname == "svm") {
    options.scene_params.shadingsystem = SHADINGSYSTEM_SVM;
  }

#ifndef WITH_CYCLES_STANDALONE_GUI
  options.session_params.background = true;
#endif

  if (options.session_params.tile_size > 0) {
    options.session_params.use_auto_tile = true;
  }

  /* find matching device */
  DeviceType device_type = Device::type_from_string(devicename.c_str());
  vector<DeviceInfo> devices = Device::available_devices(DEVICE_MASK(device_type));

  bool device_available = false;
  if (!devices.empty()) {
    options.session_params.device = devices.front();
    device_available = true;
  }

  /* handle invalid configurations */
  if (options.session_params.device.type == DEVICE_NONE || !device_available) {
    fprintf(stderr, "Unknown device: %s\n", devicename.c_str());
    exit(EXIT_FAILURE);
  }
#ifdef WITH_OSL
  else if (!(ssname == "osl" || ssname == "svm")) {
    fprintf(stderr, "Unknown shading system: %s\n", ssname.c_str());
    exit(EXIT_FAILURE);
  }
  else if (options.scene_params.shadingsystem == SHADINGSYSTEM_OSL &&
           options.session_params.device.type != DEVICE_CPU)
  {
    fprintf(stderr, "OSL shading system only works with CPU device\n");
    exit(EXIT_FAILURE);
  }
#endif
  else if (options.session_params.samples < 0) {
    fprintf(stderr, "Invalid number of samples: %d\n", options.session_params.samples);
    exit(EXIT_FAILURE);
  }
  else if (options.filepath == "") {
    fprintf(stderr, "No file path specified\n");
    exit(EXIT_FAILURE);
  }
}

CCL_NAMESPACE_END

using namespace ccl;

int main(int argc, const char **argv)
{
  util_logging_init(argv[0]);
  path_init();
  options_parse(argc, argv);

#ifdef WITH_CYCLES_STANDALONE_GUI
  if (options.session_params.background) {
#endif
    session_init();
    options.session->wait();
    session_exit();
#ifdef WITH_CYCLES_STANDALONE_GUI
  }
  else {
    string title = "Cycles: " + path_filename(options.filepath);

    /* init/exit are callback so they run while GL is initialized */
    window_main_loop(title.c_str(),
                     options.width,
                     options.height,
                     session_init,
                     session_exit,
                     resize,
                     display,
                     keyboard,
                     motion);
  }
#endif

  return 0;
}
