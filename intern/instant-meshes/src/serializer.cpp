/*
    serializer.cpp: Helper class to serialize the application state to a .PLY file

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "serializer.h"
#include <set>

extern "C" {
    #include "rply.h"
}

Serializer::Serializer() : mCompatibilityMode(false) { mPrefixStack.push(""); }

bool Serializer::isSerializedFile(const std::string &filename) {
    auto message_cb = [](p_ply ply, const char *msg) { /* ignore */};
    p_ply ply = ply_open(filename.c_str(), message_cb, 0, nullptr);
    if (!ply)
        return false;
    if (!ply_read_header(ply)) {
        ply_close(ply);
        return false;
    }
    bool is_serialized = false;
    const char *comment = nullptr;
    while ((comment = ply_get_next_comment(ply, comment))) {
        if (strcmp(comment, "Instant Meshes Application State") == 0)
            is_serialized = true;
    }
    ply_close(ply);
    return is_serialized;
}

std::vector<std::string> Serializer::getKeys() const {
    const std::string &prefix = mPrefixStack.top();
    std::vector<std::string> result;
    for (auto const &kv : mData) {
        if (kv.first.substr(0, prefix.length()) == prefix)
            result.push_back(kv.first.substr(prefix.length()));
    }
    return result;
}

struct CallbackState {
    const ProgressCallback &progress;
    void *data;
    std::string name;
    size_t offset, total = 0;
    bool visited = false;

    CallbackState(const ProgressCallback &progress, void *data,
        const std::string &name, size_t offset)
        : progress(progress), data(data), name(name), offset(offset) {}
};


Serializer::Serializer(const std::string &filename, bool compatibilityMode, const ProgressCallback &progress)
    : mCompatibilityMode(compatibilityMode) {
    auto message_cb = [](p_ply ply, const char *msg) { cerr << "rply: " << msg << endl; };

    mPrefixStack.push("");
    Timer<> timer;
    cout << "Unserializing application state from \"" << filename << "\" .. ";
    cout.flush();

    p_ply ply = ply_open(filename.c_str(), message_cb, 0, nullptr);
    if (!ply)
        throw std::runtime_error("Serializer: unable to open PLY file \"" + filename + "\"!");

    if (!ply_read_header(ply)) {
        ply_close(ply);
        throw std::runtime_error("Serializer: unable to open PLY header of \"" + filename + "\"!");
    }

    p_ply_element element = nullptr;

    #define IMPLEMENT(type) \
        case Variant::Matrix_##type: \
            const_cast<Eigen::Matrix<type, Eigen::Dynamic, Eigen::Dynamic>&>(*data->matrix_##type)(coord, index) = (type) ply_get_argument_value(argument); \
            break; \
        \
        case Variant::List_##type: {\
                ply_get_argument_property(argument, nullptr, &length, &value_index); \
                std::vector<std::vector<type>> &vec = \
                    const_cast<std::vector<std::vector<type>>&>(*data->list_##type); \
                if (value_index >= 0) \
                    vec[index][value_index] = (type) ply_get_argument_value(argument); \
                else \
                    vec[index].resize(length); \
            } \
            break;


    auto rply_element_cb = [](p_ply_argument argument) -> int {
        CallbackState *state; long index, coord, length, value_index;
        ply_get_argument_user_data(argument, (void **) &state, &coord);
        Variant *data = (Variant *) state->data;
        ply_get_argument_element(argument, nullptr, &index);
        switch (data->type_id) {
            IMPLEMENT(uint8_t);
            IMPLEMENT(uint16_t);
            IMPLEMENT(uint32_t);
            IMPLEMENT(float);
            IMPLEMENT(double);
            default:
                throw std::runtime_error("Unexpected data type while unserializing! (1)");
        }
        if (state->progress && !state->visited) {
            state->progress("Loading field \"" + state->name + "\"", state->offset / (float) state->total);
            state->visited = true;
        }
        return 1;
    };
    #undef IMPLEMENT

    std::vector<CallbackState *> callbackStates;
    try {
        /* Inspect the structure of the PLY file */
        size_t totalSize = 0;
        while ((element = ply_get_next_element(ply, element)) != nullptr) {
            const char *element_name;
            long cols = 0, rows = 0;

            ply_get_element_info(element, &element_name, &cols);
            e_ply_type type = PLY_UINT8;
            bool fail = false, list = false;

            p_ply_property property = nullptr;
            while ((property = ply_get_next_property(element, property)) != nullptr) {
                e_ply_type property_type, length_type, list_type;
                const char *property_name;
                ply_get_property_info(property, &property_name, &property_type,
                                      &length_type, &list_type);
                if (property_type == PLY_LIST) {
                    property_type = list_type;
                    list = true;
                }
                if (rows == 0)
                    type = property_type;
                else if (type != property_type)
                    fail = true;
                if (cols == 0)
                    continue;
                Variant &variant = mData[element_name];
                CallbackState *state = new CallbackState(progress, &variant, element_name, totalSize);
                callbackStates.push_back(state);
                if (!ply_set_read_cb(ply, element_name, property_name, rply_element_cb, state, rows++)) {
                    ply_close(ply);
                    throw std::runtime_error(
                        "Serializer: could not register read callback for " + std::string(element_name) +
                        "." + std::string(property_name) + " in PLY file \"" + filename + "\"!");
                }
            }
            if (rows == 0 && cols == 0)
                rows = 1;

            if (fail)
                throw std::runtime_error("Serializer: unsupported data format in \"" + filename + "\"!");

            Variant &variant = mData[std::string(element_name)];
            totalSize += cols;

            #define IMPLEMENT(ply_type, type) \
                case ply_type: \
                    if (list) { \
                        auto l = new std::vector<std::vector<type>>(); \
                        variant.type_id = Variant::Type::List_##type; \
                        variant.list_##type = l; \
                        l->resize(cols); \
                    } else { \
                        auto mat = new Eigen::Matrix<type, Eigen::Dynamic, Eigen::Dynamic>(rows, cols); \
                        variant.type_id = Variant::Type::Matrix_##type; \
                        variant.matrix_##type = mat; \
                    } \
                    break;

            switch (type) {
                IMPLEMENT(PLY_UINT8, uint8_t)
                IMPLEMENT(PLY_UINT16, uint16_t)
                IMPLEMENT(PLY_UINT32, uint32_t)
                IMPLEMENT(PLY_FLOAT32, float)
                IMPLEMENT(PLY_FLOAT64, double)
                default:
                    throw std::runtime_error("Unexpected data type while unserializing! (2)");
            }
            #undef IMPLEMENT
        }
        for (auto c : callbackStates)
            c->total = totalSize;

        if (!ply_read(ply)) {
            ply_close(ply);
            throw std::runtime_error("Error while loading application state from \"" + filename + "\"!");
        }
    } catch (...) {
        for (auto c : callbackStates)
            delete c;
        throw;
    }
    ply_close(ply);
    for (auto c : callbackStates)
        delete c;

    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
}

Serializer::~Serializer() {
    #define IMPLEMENT(type) \
        case Variant::Type::List_##type: delete kv.second.list_##type; break; \
        case Variant::Type::Matrix_##type: delete kv.second.matrix_##type; break;

    for (auto const &kv : mData) {
        switch (kv.second.type_id) {
            IMPLEMENT(uint8_t);
            IMPLEMENT(uint16_t);
            IMPLEMENT(uint32_t);
            IMPLEMENT(float);
            IMPLEMENT(double);
            default: break;
        }
    }

    #undef IMPLEMENT
}

void Serializer::write(const std::string &filename, const ProgressCallback &progress) {
    auto message_cb = [](p_ply ply, const char *msg) { cerr << "rply: " << msg << endl; };

    Timer<> timer;
    cout << "Writing application state to \"" << filename << "\" .. ";
    cout.flush();

    p_ply ply = ply_create(filename.c_str(), PLY_DEFAULT, message_cb, 0, nullptr);
    if (!ply)
        throw std::runtime_error("Unable to write PLY file!");

    ply_add_comment(ply, "Instant Meshes Application State");

    for (auto const &kv : mData) {
        #define IMPLEMENT(ply_type, type) \
            case Variant::Type::Matrix_##type: \
                ply_add_element(ply, kv.first.c_str(), kv.second.matrix_##type->cols()); \
                if (kv.second.matrix_##type->rows() <= 1) \
                    ply_add_scalar_property(ply, "value", ply_type); \
            else \
                    for (uint32_t i=0; i<kv.second.matrix_##type->rows(); ++i) \
                        ply_add_scalar_property(ply, ("value_" + std::to_string(i)).c_str(), ply_type); \
                break; \
            case Variant::Type::List_##type: { \
                    ply_add_element(ply, kv.first.c_str(), kv.second.list_##type->size()); \
                    size_t max_size = 0; \
                    for (size_t i=0; i<kv.second.list_##type->size(); ++i) \
                        max_size = std::max(max_size, kv.second.list_##type->operator[](i).size()); \
                    ply_add_list_property(ply, "value", max_size < 256 ? PLY_UINT8 : (max_size < 65536 ? PLY_UINT16 : PLY_UINT32), ply_type); \
                } \
                break;

        switch (kv.second.type_id) {
            IMPLEMENT(PLY_UINT8, uint8_t)
            IMPLEMENT(PLY_UINT16, uint16_t)
            IMPLEMENT(PLY_UINT32, uint32_t)
            IMPLEMENT(PLY_FLOAT32, float)
            IMPLEMENT(PLY_FLOAT64, double)
            default:
                throw std::runtime_error("Unexpected data type while serializing! (1)");
                break;
        }

        #undef IMPLEMENT
    }

    ply_write_header(ply);

    #define IMPLEMENT(type) \
        case Variant::Type::Matrix_##type: \
            for (uint32_t i=0; i<kv.second.matrix_##type->size(); ++i) \
                ply_write(ply, kv.second.matrix_##type->data()[i]); \
            written += kv.second.matrix_##type->size() * sizeof(type); \
            break; \
        case Variant::Type::List_##type: { \
                const std::vector<std::vector<type>> &list = *kv.second.list_##type; \
                for (uint32_t i=0; i<list.size(); ++i) { \
                    ply_write(ply, list[i].size()); \
                    for (uint32_t j=0; j<list[i].size(); ++j) \
                        ply_write(ply, list[i][j]); \
                    written += list[i].size() * sizeof(type) + sizeof(uint32_t); \
                } \
            } \
            break;

    size_t size = totalSize(), written = 0;
    for (auto const &kv : mData) {
        switch (kv.second.type_id) {
            IMPLEMENT(uint8_t);
            IMPLEMENT(uint16_t);
            IMPLEMENT(uint32_t);
            IMPLEMENT(float);
            IMPLEMENT(double);
            default:
                throw std::runtime_error("Unexpected data type while serializing! (2)");
                break;
        }
        if (progress)
            progress("Writing \"" + kv.first  + "\"", written / (Float) size);
    }

    #undef IMPLEMENT

    ply_close(ply);
    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
}

size_t Serializer::totalSize() const {
    size_t result = 0;

    #define IMPLEMENT(type) \
        case Variant::Type::Matrix_##type: \
            result += kv.second.matrix_##type->size() * sizeof(type); \
            break; \
        case Variant::Type::List_##type: {\
                const std::vector<std::vector<type>> &list = *kv.second.list_##type; \
                for (uint32_t i=0; i<list.size(); ++i) \
                    result += list[i].size() * sizeof(type) + sizeof(uint32_t); \
            } \
            break;

    for (auto const &kv : mData) {
        switch (kv.second.type_id) {
            IMPLEMENT(uint8_t);
            IMPLEMENT(uint16_t);
            IMPLEMENT(uint32_t);
            IMPLEMENT(float);
            IMPLEMENT(double);
        }
    }

    #undef IMPLEMENT
    return result;
}


bool Serializer::diff(const Serializer &other) const {
    std::set<std::string> keys;
    for (auto const &kv : mData)
        keys.insert(kv.first);
    for (auto const &kv : other.mData)
        keys.insert(kv.first);

    bool diff = false;

    for (const std::string &key : keys) {
        auto it1 = mData.find(key);
        if (it1 == mData.end()) {
            cout << "Element " << key << " does not exist in serializer 1." << endl;
            diff = true;
            continue;
        }
        auto it2 = other.mData.find(key);
        if (it2 == other.mData.end()) {
            cout << "Element " << key << " does not exist in serializer 2." << endl;
            diff = true;
            continue;
        }
        const Variant &v1 = it1->second;
        const Variant &v2 = it2->second;
        if (v1.type_id != v2.type_id) {
            cout << "Element " << key << " have different types." << endl;
            diff = true;
            continue;
        }

        #define IMPLEMENT(type) \
            case Variant::Type::Matrix_##type: \
                if (v1.matrix_##type->cols() != v2.matrix_##type->cols() || \
                    v1.matrix_##type->rows() != v2.matrix_##type->rows()) \
                    result = true; \
                else \
                    result = *(v1.matrix_##type) != *(v2.matrix_##type); \
                break; \
            case Variant::Type::List_##type: \
                result = *(v1.list_##type) != *(v2.list_##type); \
                break;

        bool result = false;
        switch (v1.type_id) {
            IMPLEMENT(uint8_t);
            IMPLEMENT(uint16_t);
            IMPLEMENT(uint32_t);
            IMPLEMENT(float);
            IMPLEMENT(double);
            default:
                throw std::runtime_error("Unexpected data type while diffing!");
                break;
        }

        #undef IMPLEMENT

        if (result) {
            cout << "Element " << key << " differs." << endl;
            diff = true;
        }
    }

    return diff;
}

std::ostream &operator<<(std::ostream &os, const Serializer &state) {
    os << "Serializer[";
    bool first = true;
    for (auto const &kv : state.mData) {
        const Serializer::Variant &v = kv.second;
        if (!first)
            cout << ",";
        first = false;
        cout << endl;
        std::string tname, value;

        #define IMPLEMENT(type) \
            case Serializer::Variant::Type::Matrix_##type: { \
                    const Eigen::Matrix<type, Eigen::Dynamic, Eigen::Dynamic> &mat = *(v.matrix_##type); \
                    if (mat.size() == 1) { \
                        tname = #type; \
                        value = std::to_string(mat(0, 0)); \
                    } else if (mat.cols() == 1 && mat.rows() <= 4) { \
                        tname = "vec<" #type ">"; \
                        value = "["; \
                        for (int i=0; i<mat.rows(); ++i) { \
                            value += std::to_string(mat(i, 0)); \
                            if (i+1<mat.rows()) \
                                value += ", "; \
                        } \
                        value += "]"; \
                    } else { \
                        tname = std::string(mat.cols() == 1 ? "vec<" : "mat<") + #type + std::string(">"); \
                        value = "data[" + std::to_string(mat.rows()) + "x" + \
                                          std::to_string(mat.cols()) + "]"; \
                    } \
                } \
                break; \
            case Serializer::Variant::Type::List_##type: \
                tname = #type "**"; \
                value = "data[" + std::to_string(v.list_##type->size()) + "][]"; \
                break; \

        switch (v.type_id) {
            IMPLEMENT(uint8_t);
            IMPLEMENT(uint16_t);
            IMPLEMENT(uint32_t);
            IMPLEMENT(float);
            IMPLEMENT(double);
            default:
                throw std::runtime_error("Unexpected type in stream operator");
        }
        os << "\t" << tname << " " << kv.first << " = " << value;
    }
    os << endl << "]";
    return os;
}
