/*
    meshio.cpp: Mesh file input/output routines

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "meshio.h"
#include "normal.h"
#include <unordered_map>
#include <fstream>
#if !defined(_WIN32)
#include <libgen.h>
#endif

extern "C" {
    #include "rply.h"
}

void load_mesh_or_pointcloud(const std::string &filename, MatrixXu &F, MatrixXf &V, MatrixXf &N,
              const ProgressCallback &progress) {
    std::string extension;
    if (filename.size() > 4)
        extension = str_tolower(filename.substr(filename.size()-4));

    if (extension == ".ply")
        load_ply(filename, F, V, N, false, progress);
    else if (extension == ".obj")
        load_obj(filename, F, V, progress);
    else if (extension == ".aln")
        load_pointcloud(filename, V, N, progress);
    else
        throw std::runtime_error("load_mesh_or_pointcloud: Unknown file extension \"" + extension + "\" (.ply/.obj/.aln are supported)");
}

void write_mesh(const std::string &filename, const MatrixXu &F,
                const MatrixXf &V, const MatrixXf &N, const MatrixXf &Nf,
                const MatrixXf &UV, const MatrixXf &C,
                const ProgressCallback &progress) {
    std::string extension;
    if (filename.size() > 4)
        extension = str_tolower(filename.substr(filename.size()-4));

    if (extension == ".ply")
        write_ply(filename, F, V, N, Nf, UV, C, progress);
    else if (extension == ".obj")
        write_obj(filename, F, V, N, Nf, UV, C, progress);
    else
        throw std::runtime_error("write_mesh: Unknown file extension \"" + extension + "\" (.ply/.obj are supported)");
}

void load_ply(const std::string &filename, MatrixXu &F, MatrixXf &V,
              MatrixXf &N, bool pointcloud, const ProgressCallback &progress) {
    auto message_cb = [](p_ply ply, const char *msg) { cerr << "rply: " << msg << endl; };

    Timer<> timer;
    cout << "Loading \"" << filename << "\" .. ";
    cout.flush();

    p_ply ply = ply_open(filename.c_str(), message_cb, 0, nullptr);
    if (!ply)
        throw std::runtime_error("Unable to open PLY file \"" + filename + "\"!");

    if (!ply_read_header(ply)) {
        ply_close(ply);
        throw std::runtime_error("Unable to open PLY header of \"" + filename + "\"!");
    }

    p_ply_element element = nullptr;
    uint32_t vertexCount = 0, faceCount = 0;

    /* Inspect the structure of the PLY file, load number of faces if avaliable */
    while ((element = ply_get_next_element(ply, element)) != nullptr) {
        const char *name;
        long nInstances;

        ply_get_element_info(element, &name, &nInstances);
        if (!strcmp(name, "vertex"))
            vertexCount = (uint32_t) nInstances;
        else if (!strcmp(name, "face"))
            faceCount = (uint32_t) nInstances;
    }

    if (vertexCount == 0 && faceCount == 0)
        throw std::runtime_error("PLY file \"" + filename + "\" is invalid! No face/vertex/elements found!");
    else if (!pointcloud && faceCount == 0)
        throw std::runtime_error("PLY file \"" + filename + "\" is invalid! No faces found!");

    F.resize(3, faceCount);
    V.resize(3, vertexCount);

    struct VertexCallbackData {
        MatrixXf &V;
        const ProgressCallback &progress;
        VertexCallbackData(MatrixXf &V, const ProgressCallback &progress)
            : V(V), progress(progress) { }
    };

    struct FaceCallbackData {
        MatrixXu &F;
        const ProgressCallback &progress;
        FaceCallbackData(MatrixXu &F, const ProgressCallback &progress)
            : F(F), progress(progress) { }
    };

    struct VertexNormalCallbackData {
        MatrixXf &N;
        const ProgressCallback &progress;
        VertexNormalCallbackData(MatrixXf &_N, const ProgressCallback &progress)
            : N(_N), progress(progress) { }
    };

    auto rply_vertex_cb = [](p_ply_argument argument) -> int {
        VertexCallbackData *data; long index, coord;
        ply_get_argument_user_data(argument, (void **) &data, &coord);
        ply_get_argument_element(argument, nullptr, &index);
        data->V(coord, index) = (Float) ply_get_argument_value(argument);
        if (data->progress && coord == 0 && index % 500000 == 0)
            data->progress("Loading vertex data", index / (Float) data->V.cols());
        return 1;
    };

    auto rply_vertex_normal_cb = [](p_ply_argument argument) -> int {
        VertexNormalCallbackData *data; long index, coord;
        ply_get_argument_user_data(argument, (void **) &data, &coord);
        ply_get_argument_element(argument, nullptr, &index);
        data->N(coord, index) = (Float) ply_get_argument_value(argument);
        if (data->progress && coord == 0 && index % 500000 == 0)
            data->progress("Loading vertex normal data", index / (Float)data->N.cols());
        return 1;
    };

    auto rply_index_cb = [](p_ply_argument argument) -> int {
        FaceCallbackData *data;
        long length, value_index, index;
        ply_get_argument_property(argument, nullptr, &length, &value_index);

        if (length != 3)
            throw std::runtime_error("Only triangle faces are supported!");

        ply_get_argument_user_data(argument, (void **) &data, nullptr);
        ply_get_argument_element(argument, nullptr, &index);

        if (value_index >= 0)
            data->F(value_index, index) = (uint32_t) ply_get_argument_value(argument);

        if (data->progress && value_index == 0 && index % 500000 == 0)
            data->progress("Loading face data", index / (Float) data->F.cols());

        return 1;
    };

    VertexCallbackData vcbData(V, progress);
    FaceCallbackData fcbData(F, progress);
    VertexNormalCallbackData vncbData(N, progress);

    if (!ply_set_read_cb(ply, "vertex", "x", rply_vertex_cb, &vcbData, 0) ||
        !ply_set_read_cb(ply, "vertex", "y", rply_vertex_cb, &vcbData, 1) ||
        !ply_set_read_cb(ply, "vertex", "z", rply_vertex_cb, &vcbData, 2)) {
        ply_close(ply);
        throw std::runtime_error("PLY file \"" + filename + "\" does not contain vertex position data!");
    }

    if (pointcloud && faceCount == 0) {
        N.resize(3, vertexCount);
        if (!ply_set_read_cb(ply, "vertex", "nx", rply_vertex_normal_cb, &vncbData, 0) ||
            !ply_set_read_cb(ply, "vertex", "ny", rply_vertex_normal_cb, &vncbData, 1) ||
            !ply_set_read_cb(ply, "vertex", "nz", rply_vertex_normal_cb, &vncbData, 2)) {
            ply_close(ply);
            throw std::runtime_error("PLY file \"" + filename + "\" does not contain vertex normal or face data!");
        }
    } else {
        if (!ply_set_read_cb(ply, "face", "vertex_indices", rply_index_cb, &fcbData, 0)) {
            ply_close(ply);
            throw std::runtime_error("PLY file \"" + filename + "\" does not contain vertex indices!");
        }
    }

    if (!ply_read(ply)) {
        ply_close(ply);
        throw std::runtime_error("Error while loading PLY data from \"" + filename + "\"!");
    }

    ply_close(ply);
    cout << "done. (V=" << vertexCount;
    if (faceCount > 0)
        cout << ", F=" << faceCount;
    cout << ", took " << timeString(timer.value()) << ")" << endl;
}

void write_ply(const std::string &filename, const MatrixXu &F,
               const MatrixXf &V, const MatrixXf &N, const MatrixXf &Nf, const MatrixXf &UV,
               const MatrixXf &C, const ProgressCallback &progress) {
    auto message_cb = [](p_ply ply, const char *msg) { cerr << "rply: " << msg << endl; };

    Timer<> timer;
    cout << "Writing \"" << filename << "\" (V=" << V.cols()
         << ", F=" << F.cols() << ") .. ";
    cout.flush();

    if (N.size() > 0 && Nf.size() > 0)
        throw std::runtime_error("Please specify either face or vertex normals but not both!");

    p_ply ply = ply_create(filename.c_str(), PLY_DEFAULT, message_cb, 0, nullptr);
    if (!ply)
        throw std::runtime_error("Unable to write PLY file!");

    ply_add_comment(ply, "Generated by Instant Meshes");
    ply_add_element(ply, "vertex", V.cols());
    ply_add_scalar_property(ply, "x", PLY_FLOAT);
    ply_add_scalar_property(ply, "y", PLY_FLOAT);
    ply_add_scalar_property(ply, "z", PLY_FLOAT);

    if (N.size() > 0) {
        ply_add_scalar_property(ply, "nx", PLY_FLOAT);
        ply_add_scalar_property(ply, "ny", PLY_FLOAT);
        ply_add_scalar_property(ply, "nz", PLY_FLOAT);
        if (N.cols() != V.cols() || N.rows() != 3)
            throw std::runtime_error("Vertex normal matrix has incorrect size");
    }

    if (UV.size() > 0) {
        ply_add_scalar_property(ply, "u", PLY_FLOAT);
        ply_add_scalar_property(ply, "v", PLY_FLOAT);
        if (UV.cols() != V.cols() || UV.rows() != 2)
            throw std::runtime_error("Texture coordinate matrix has incorrect size");
    }

    if (C.size() > 0) {
        ply_add_scalar_property(ply, "red", PLY_FLOAT);
        ply_add_scalar_property(ply, "green", PLY_FLOAT);
        ply_add_scalar_property(ply, "blue", PLY_FLOAT);
        if (C.cols() != V.cols() || (C.rows() != 3 && C.rows() != 4))
            throw std::runtime_error("Color matrix has incorrect size");
    }

    /* Check for irregular faces */
    std::map<uint32_t, std::pair<uint32_t, std::map<uint32_t, uint32_t>>> irregular;
    size_t nIrregular = 0;
    if (F.rows() == 4) {
        for (uint32_t f=0; f<F.cols(); ++f) {
            if (F(2, f) == F(3, f)) {
                nIrregular++;
                auto &value = irregular[F(2, f)];
                value.first = f;
                value.second[F(0, f)] = F(1, f);
            }
        }
    }

    ply_add_element(ply, "face", F.cols() - nIrregular + irregular.size());
    ply_add_list_property(ply, "vertex_indices", PLY_UINT8, PLY_INT);
    if (Nf.size() > 0) {
        ply_add_scalar_property(ply, "nx", PLY_FLOAT);
        ply_add_scalar_property(ply, "ny", PLY_FLOAT);
        ply_add_scalar_property(ply, "nz", PLY_FLOAT);
        if (Nf.cols() != F.cols() || Nf.rows() != 3)
            throw std::runtime_error("Face normal matrix has incorrect size");
    }
    ply_write_header(ply);

    for (uint32_t j=0; j<V.cols(); ++j) {
        for (uint32_t i=0; i<V.rows(); ++i)
            ply_write(ply, V(i, j));
        if (N.size() > 0) {
            for (uint32_t i=0; i<N.rows(); ++i)
                ply_write(ply, N(i, j));
        }
        if (UV.size() > 0) {
            for (uint32_t i=0; i<UV.rows(); ++i)
                ply_write(ply, UV(i, j));
        }
        if (C.size() > 0) {
            for (uint32_t i=0; i<std::min(3u, (uint32_t) C.rows()); ++i)
                ply_write(ply, C(i, j));
        }
        if (progress && j % 500000 == 0)
            progress("Writing vertex data", j / (Float) V.cols());
    }

    for (uint32_t f=0; f<F.cols(); ++f) {
        if (F.rows() == 4 && F(2, f) == F(3, f))
            continue;
        ply_write(ply, F.rows());
        for (uint32_t i=0; i<F.rows(); ++i)
            ply_write(ply, F(i, f));
        if (Nf.size() > 0) {
            for (uint32_t i=0; i<Nf.rows(); ++i)
                ply_write(ply, Nf(i, f));
        }
        if (progress && f % 500000 == 0)
            progress("Writing face data", f / (Float) F.cols());
    }

    for (auto item : irregular) {
        auto face = item.second;
        uint32_t v = face.second.begin()->first, first = v, i = 0;
        ply_write(ply, face.second.size());
        while (true) {
            ply_write(ply, v);
            v = face.second[v];
            ++i;
            if (v == first || i == face.second.size())
                break;
        }
        while (i != face.second.size()) {
            ply_write(ply, v);
            ++i;
        }
        if (Nf.size() > 0) {
            for (uint32_t i=0; i<Nf.rows(); ++i)
                ply_write(ply, Nf(i, face.first));
        }
    }

    ply_close(ply);
    cout << "done. (";
    if (irregular.size() > 0)
        cout << irregular.size() << " irregular faces, ";
    cout << "took " << timeString(timer.value()) << ")" << endl;
}

void load_obj(const std::string &filename, MatrixXu &F, MatrixXf &V,
              const ProgressCallback &progress) {
    /// Vertex indices used by the OBJ format
    struct obj_vertex {
        uint32_t p = (uint32_t) -1;
        uint32_t n = (uint32_t) -1;
        uint32_t uv = (uint32_t) -1;

        inline obj_vertex() { }

        inline obj_vertex(const std::string &string) {
            std::vector<std::string> tokens = str_tokenize(string, '/', true);

            if (tokens.size() < 1 || tokens.size() > 3)
                throw std::runtime_error("Invalid vertex data: \"" + string + "\"");

            p = str_to_uint32_t(tokens[0]);

            #if 0
                if (tokens.size() >= 2 && !tokens[1].empty())
                    uv = str_to_uint32_t(tokens[1]);

                if (tokens.size() >= 3 && !tokens[2].empty())
                    n = str_to_uint32_t(tokens[2]);
            #endif
        }

        inline bool operator==(const obj_vertex &v) const {
            return v.p == p && v.n == n && v.uv == uv;
        }
    };

    /// Hash function for obj_vertex
    struct obj_vertexHash : std::unary_function<obj_vertex, size_t> {
        std::size_t operator()(const obj_vertex &v) const {
            size_t hash = std::hash<uint32_t>()(v.p);
            hash = hash * 37 + std::hash<uint32_t>()(v.uv);
            hash = hash * 37 + std::hash<uint32_t>()(v.n);
            return hash;
        }
    };

    typedef std::unordered_map<obj_vertex, uint32_t, obj_vertexHash> VertexMap;

    std::ifstream is(filename);
    if (is.fail())
        throw std::runtime_error("Unable to open OBJ file \"" + filename + "\"!");
    cout << "Loading \"" << filename << "\" .. ";
    cout.flush();
    Timer<> timer;

    std::vector<Vector3f>   positions;
    //std::vector<Vector2f>   texcoords;
    //std::vector<Vector3f>   normals;
    std::vector<uint32_t>   indices;
    std::vector<obj_vertex> vertices;
    VertexMap vertexMap;

    std::string line_str;
    while (std::getline(is, line_str)) {
        std::istringstream line(line_str);

        std::string prefix;
        line >> prefix;

        if (prefix == "v") {
            Vector3f p;
            line >> p.x() >> p.y() >> p.z();
            positions.push_back(p);
        } else if (prefix == "vt") {
            /*
            Vector2f tc;
            line >> tc.x() >> tc.y();
            texcoords.push_back(tc);
            */
        } else if (prefix == "vn") {
            /*
            Vector3f n;
            line >> n.x() >> n.y() >> n.z();
            normals.push_back(n);
            */
        } else if (prefix == "f") {
            std::string v1, v2, v3, v4;
            line >> v1 >> v2 >> v3 >> v4;
            obj_vertex tri[6];
            int nVertices = 3;

            tri[0] = obj_vertex(v1);
            tri[1] = obj_vertex(v2);
            tri[2] = obj_vertex(v3);

            if (!v4.empty()) {
                /* This is a quad, split into two triangles */
                tri[3] = obj_vertex(v4);
                tri[4] = tri[0];
                tri[5] = tri[2];
                nVertices = 6;
            }
            /* Convert to an indexed vertex list */
            for (int i=0; i<nVertices; ++i) {
                const obj_vertex &v = tri[i];
                VertexMap::const_iterator it = vertexMap.find(v);
                if (it == vertexMap.end()) {
                    vertexMap[v] = (uint32_t) vertices.size();
                    indices.push_back((uint32_t) vertices.size());
                    vertices.push_back(v);
                } else {
                    indices.push_back(it->second);
                }
            }
        }
    }

    F.resize(3, indices.size()/3);
    memcpy(F.data(), indices.data(), sizeof(uint32_t)*indices.size());

    V.resize(3, vertices.size());
    for (uint32_t i=0; i<vertices.size(); ++i)
        V.col(i) = positions.at(vertices[i].p-1);

    cout << "done. (V=" << V.cols() << ", F=" << F.cols() << ", took "
         << timeString(timer.value()) << ")" << endl;
}

void load_pointcloud(const std::string &filename, MatrixXf &V, MatrixXf &N,
                     const ProgressCallback &progress) {
    std::ifstream is(filename);
    if (is.fail())
        throw std::runtime_error("Unable to open ALN file \"" + filename + "\"!");
    cout.flush();
    Timer<> timer;
    std::istringstream line;

    auto fetch_line = [&]() {
        std::string line_str;
        do {
            std::getline(is, line_str);
            if (is.eof())
                throw std::runtime_error("Parser error while processing ALN file!");
        } while (line_str.empty() || line_str[0] == '#');
        line.clear();
        line.str(std::move(line_str));
    };

    auto fetch_string = [&](std::string &value) {
        while (!(line >> value)) {
            if (line.eof())
                fetch_line();
            else
                throw std::runtime_error("Parser error while processing ALN file!");
        }
    };
    auto fetch_uint = [&](uint32_t &value) {
        while (!(line >> value)) {
            if (line.eof())
                fetch_line();
            else
                throw std::runtime_error("Parser error while processing ALN file!");
        }
    };

    auto fetch_float = [&](Float &value) {
        while (!(line >> value)) {
            if (line.eof())
                fetch_line();
            else
                throw std::runtime_error("Parser error while processing ALN file!");
        }
    };

    uint32_t nFiles;
    fetch_uint(nFiles);

#if defined(_WIN32)
    char path_drive[_MAX_DRIVE];
    char path_dir[_MAX_DIR];
    char path_fname[_MAX_FNAME];
    char path_ext[_MAX_EXT];
    _splitpath(filename.c_str(), path_drive, path_dir, path_fname, path_ext);
#else
    char *path_dir = dirname((char *) filename.c_str());
#endif

    for (uint32_t i=0; i<nFiles; ++i) {
        std::string filename_sub;
        fetch_string(filename_sub);
        MatrixXu F_sub;
        MatrixXf V_sub, N_sub;
        load_ply(std::string(path_dir) + "/" + filename_sub, F_sub, V_sub, N_sub, true);
        Eigen::Matrix<Float, 4, 4> M;
        for (uint32_t k=0; k<16; ++k)
            fetch_float(M.data()[k]);
        M.transposeInPlace();
        for (uint32_t k=0; k<V_sub.cols(); ++k) {
            Vector4f p;
            p << V_sub.col(k), 1.0f;
            p = (M*p).eval();
            p /= p.w();
            V_sub.col(k) = p.head<3>();
        }
        if (N_sub.cols() == 0)
            generate_smooth_normals(F_sub, V_sub, N_sub, true);
        uint32_t base = (uint32_t) V.cols();
        V.conservativeResize(3, base + V_sub.cols());
        V.block(0, base, 3, V_sub.cols()) = V_sub;
        N.conservativeResize(3, base + N_sub.cols());
        N.block(0, base, 3, N_sub.cols()) = N_sub;
        if (progress)
            progress("Loading point cloud", i / (Float) (nFiles-1));
    }

    cout << "Point cloud loading finished. (V=" << V.cols() << ", took "
         << timeString(timer.value()) << ")" << endl;
}

void write_obj(const std::string &filename, const MatrixXu &F,
                const MatrixXf &V, const MatrixXf &N, const MatrixXf &Nf,
                const MatrixXf &UV, const MatrixXf &C,
                const ProgressCallback &progress) {
    Timer<> timer;
    cout << "Writing \"" << filename << "\" (V=" << V.cols()
         << ", F=" << F.cols() << ") .. ";
    cout.flush();
    std::ofstream os(filename);
    if (os.fail())
        throw std::runtime_error("Unable to open OBJ file \"" + filename + "\"!");
    if (N.size() > 0 && Nf.size() > 0)
        throw std::runtime_error("Please specify either face or vertex normals but not both!");

    for (uint32_t i=0; i<V.cols(); ++i)
        os << "v " << V(0, i) << " " << V(1, i) << " " << V(2, i) << endl;

    for (uint32_t i=0; i<N.cols(); ++i)
        os << "vn " << N(0, i) << " " << N(1, i) << " " << N(2, i) << endl;

    for (uint32_t i=0; i<Nf.cols(); ++i)
        os << "vn " << Nf(0, i) << " " << Nf(1, i) << " " << Nf(2, i) << endl;

    for (uint32_t i=0; i<UV.cols(); ++i)
        os << "vt " << UV(0, i) << " " << UV(1, i) << endl;

    /* Check for irregular faces */
    std::map<uint32_t, std::pair<uint32_t, std::map<uint32_t, uint32_t>>> irregular;
    size_t nIrregular = 0;

    for (uint32_t f=0; f<F.cols(); ++f) {
        if (F.rows() == 4) {
            if (F(2, f) == F(3, f)) {
                nIrregular++;
                auto &value = irregular[F(2, f)];
                value.first = f;
                value.second[F(0, f)] = F(1, f);
                continue;
            }
        }
        os << "f ";
        for (uint32_t j=0; j<F.rows(); ++j) {
            uint32_t idx = F(j, f);
            idx += 1;
            os << idx;
            if (Nf.size() > 0)
                idx = f + 1;
            os << "//" << idx << " ";
        }
        os << endl;
    }

    for (auto item : irregular) {
        auto face = item.second;
        uint32_t v = face.second.begin()->first, first = v, i = 0;
        os << "f ";
        while (true) {
            uint32_t idx = v + 1;
            os << idx;
            if (Nf.size() > 0)
                idx = face.first + 1;
            os << "//" << idx << " ";

            v = face.second[v];
            if (v == first || ++i == face.second.size())
                break;
        }
        os << endl;
    }

    cout << "done. (";
    if (irregular.size() > 0)
        cout << irregular.size() << " irregular faces, ";
    cout << "took " << timeString(timer.value()) << ")" << endl;
}
