/*
    main.cpp -- Instant Meshes application entry point

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "batch.h"
#include "viewer.h"
#include "serializer.h"
#include <thread>
#include <cstdlib>

/* Force usage of discrete GPU on laptops */
NANOGUI_FORCE_DISCRETE_GPU();

int nprocs = -1;

int main(int argc, char **argv) {
    std::vector<std::string> args;
    bool extrinsic = true, dominant = false, align_to_boundaries = false;
    bool fullscreen = false, help = false, deterministic = false, compat = false;
    int rosy = 4, posy = 4, face_count = -1, vertex_count = -1;
    uint32_t knn_points = 10, smooth_iter = 2;
    Float crease_angle = -1, scale = -1;
    std::string batchOutput;
    #if defined(__APPLE__)
        bool launched_from_finder = false;
    #endif

    try {
        for (int i=1; i<argc; ++i) {
            if (strcmp("--fullscreen", argv[i]) == 0 || strcmp("-F", argv[i]) == 0) {
                fullscreen = true;
            } else if (strcmp("--help", argv[i]) == 0 || strcmp("-h", argv[i]) == 0) {
                help = true;
            } else if (strcmp("--deterministic", argv[i]) == 0 || strcmp("-d", argv[i]) == 0) {
                deterministic = true;
            } else if (strcmp("--intrinsic", argv[i]) == 0 || strcmp("-i", argv[i]) == 0) {
                extrinsic = false;
            } else if (strcmp("--boundaries", argv[i]) == 0 || strcmp("-b", argv[i]) == 0) {
                align_to_boundaries = true;
            } else if (strcmp("--threads", argv[i]) == 0 || strcmp("-t", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing thread count!" << endl;
                    return -1;
                }
                nprocs = str_to_uint32_t(argv[i]);
            } else if (strcmp("--smooth", argv[i]) == 0 || strcmp("-S", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing smoothing iteration count argument!" << endl;
                    return -1;
                }
                smooth_iter = str_to_uint32_t(argv[i]);
            } else if (strcmp("--knn", argv[i]) == 0 || strcmp("-k", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing knn point count argument!" << endl;
                    return -1;
                }
                knn_points = str_to_uint32_t(argv[i]);
            } else if (strcmp("--crease", argv[i]) == 0 || strcmp("-c", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing crease angle argument!" << endl;
                    return -1;
                }
                crease_angle = str_to_float(argv[i]);
            } else if (strcmp("--rosy", argv[i]) == 0 || strcmp("-r", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing rotation symmetry type!" << endl;
                    return -1;
                }
                rosy = str_to_int32_t(argv[i]);
            } else if (strcmp("--posy", argv[i]) == 0 || strcmp("-p", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing position symmetry type!" << endl;
                    return -1;
                }
                posy = str_to_int32_t(argv[i]);
                if (posy == 6)
                    posy = 3;
            } else if (strcmp("--scale", argv[i]) == 0 || strcmp("-s", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing scale argument!" << endl;
                    return -1;
                }
                scale = str_to_float(argv[i]);
            } else if (strcmp("--faces", argv[i]) == 0 || strcmp("-f", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing face count argument!" << endl;
                    return -1;
                }
                face_count = str_to_int32_t(argv[i]);
            } else if (strcmp("--vertices", argv[i]) == 0 || strcmp("-v", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing vertex count argument!" << endl;
                    return -1;
                }
                vertex_count = str_to_int32_t(argv[i]);
            } else if (strcmp("--output", argv[i]) == 0 || strcmp("-o", argv[i]) == 0) {
                if (++i >= argc) {
                    cerr << "Missing batch mode output file argument!" << endl;
                    return -1;
                }
                batchOutput = argv[i];
            } else if (strcmp("--dominant", argv[i]) == 0 || strcmp("-D", argv[i]) == 0) {
                dominant = true;
            } else if (strcmp("--compat", argv[i]) == 0 || strcmp("-C", argv[i]) == 0) {
                compat = true;
#if defined(__APPLE__)
            } else if (strncmp("-psn", argv[i], 4) == 0) {
                launched_from_finder = true;
#endif
            } else {
                if (strncmp(argv[i], "-", 1) == 0) {
                    cerr << "Invalid argument: \"" << argv[i] << "\"!" << endl;
                    help = true;
                }
                args.push_back(argv[i]);
            }
        }
    } catch (const std::exception &e) {
        cout << "Error: " << e.what() << endl;
        help = true;
    }

    if ((posy != 3 && posy != 4) || (rosy != 2 && rosy != 4 && rosy != 6)) {
        cerr << "Error: Invalid symmetry type!" << endl;
        help  = true;
    }

    int nConstraints = 0;
    nConstraints += scale > 0 ? 1 : 0;
    nConstraints += face_count > 0 ? 1 : 0;
    nConstraints += vertex_count > 0 ? 1 : 0;

    if (nConstraints > 1) {
        cerr << "Error: Only one of the --scale, --face and --vertices parameters can be used at once!" << endl;
        help = true;
    }

    if (args.size() > 1 || help || (!batchOutput.empty() && args.size() == 0)) {
        cout << "Syntax: " << argv[0] << " [options] <input mesh / point cloud / application state snapshot>" << endl;
        cout << "Options:" << endl;
        cout << "   -o, --output <output>     Writes to the specified PLY/OBJ output file in batch mode" << endl;
        cout << "   -t, --threads <count>     Number of threads used for parallel computations" << endl;
        cout << "   -d, --deterministic       Prefer (slower) deterministic algorithms" << endl;
        cout << "   -c, --crease <degrees>    Dihedral angle threshold for creases" << endl;
        cout << "   -S, --smooth <iter>       Number of smoothing & ray tracing reprojection steps (default: 2)" << endl;
        cout << "   -D, --dominant            Generate a tri/quad dominant mesh instead of a pure tri/quad mesh" << endl;
        cout << "   -i, --intrinsic           Intrinsic mode (extrinsic is the default)" << endl;
        cout << "   -b, --boundaries          Align to boundaries (only applies when the mesh is not closed)" << endl;
        cout << "   -r, --rosy <number>       Specifies the orientation symmetry type (2, 4, or 6)" << endl;
        cout << "   -p, --posy <number>       Specifies the position symmetry type (4 or 6)" << endl;
        cout << "   -s, --scale <scale>       Desired world space length of edges in the output" << endl;
        cout << "   -f, --faces <count>       Desired face count of the output mesh" << endl;
        cout << "   -v, --vertices <count>    Desired vertex count of the output mesh" << endl;
        cout << "   -C, --compat              Compatibility mode to load snapshots from old software versions" << endl;
        cout << "   -k, --knn <count>         Point cloud mode: number of adjacent points to consider" << endl;
        cout << "   -F, --fullscreen          Open a full-screen window" << endl;
        cout << "   -h, --help                Display this message" << endl;
        return -1;
    }

    if (args.size() == 0)
        cout << "Running in GUI mode, start with -h for instructions on batch mode." << endl;

    tbb::task_scheduler_init init(nprocs == -1 ? tbb::task_scheduler_init::automatic : nprocs);

    if (!batchOutput.empty() && args.size() == 1) {
        try {
            batch_process(args[0], batchOutput, rosy, posy, scale, face_count,
                          vertex_count, crease_angle, extrinsic,
                          align_to_boundaries, smooth_iter, knn_points,
                          !dominant, deterministic);
            return 0;
        } catch (const std::exception &e) {
            cerr << "Caught runtime error : " << e.what() << endl;
            return -1;
        }
    }

    try {
        nanogui::init();

        #if defined(__APPLE__)
            if (launched_from_finder)
                nanogui::chdir_to_bundle_parent();
        #endif

        {
            nanogui::ref<Viewer> viewer = new Viewer(fullscreen, deterministic);
            viewer->setVisible(true);

            if (args.size() == 1) {
                if (Serializer::isSerializedFile(args[0])) {
                    viewer->loadState(args[0], compat);
                } else {
                    viewer->loadInput(args[0], crease_angle,
                            scale, face_count, vertex_count,
                            rosy, posy, knn_points);
                    viewer->setExtrinsic(extrinsic);
                }
            }

            nanogui::mainloop();
        }

        nanogui::shutdown();
    } catch (const std::runtime_error &e) {
        std::string error_msg = std::string("Caught a fatal error: ") + std::string(e.what());
        #if defined(_WIN32)
            MessageBoxA(nullptr, error_msg.c_str(), NULL, MB_ICONERROR | MB_OK);
        #else
            std::cerr << error_msg << endl;
        #endif
        return -1;
    }

    return EXIT_SUCCESS;
}
