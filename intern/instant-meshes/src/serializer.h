/*
    serializer.h: Helper class to serialize the application state to a .PLY file

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "common.h"

#include <map>
#include <set>
#include <stack>

class Serializer {
public:
    Serializer();
    Serializer(const std::string &filename, bool compatibilityMode = false,
               const ProgressCallback &progress = ProgressCallback());
    ~Serializer();

    void write(const std::string &filename,
               const ProgressCallback &progress = ProgressCallback());

    static bool isSerializedFile(const std::string &filename);

    void pushPrefix(const std::string &prefix) const { mPrefixStack.push(mPrefixStack.top() + prefix + "."); }
    void popPrefix() const { mPrefixStack.pop(); }

    bool diff(const Serializer &serializer) const;

    inline size_t totalSize() const;

    std::vector<std::string> getKeys() const;

    #define MISSING_KEY(key) \
        if (mCompatibilityMode) {\
            cerr << "Warning: Serializer: could not find element \"" + mPrefixStack.top() + key + "\"!" << endl; \
            return false; \
        } else { \
            throw std::runtime_error("Serializer: could not find element \"" + mPrefixStack.top() + key + "\"!"); \
        }

    #define IMPLEMENT(type, btype) \
        void set(const std::string &key, type value) { \
            Eigen::Matrix<btype, Eigen::Dynamic, Eigen::Dynamic> *mat = new Eigen::Matrix<btype, Eigen::Dynamic, Eigen::Dynamic>(1, 1); \
            (*mat)(0, 0) = (btype) value; \
            mData.emplace(mPrefixStack.top() + key, mat); \
        } \
        void set(const std::string &key, const Eigen::Matrix<type, 2, 1> &v) { \
            mData.emplace(mPrefixStack.top() + key, \
                new Eigen::Matrix<btype, Eigen::Dynamic, Eigen::Dynamic>(v.cast<btype>())); \
        } \
        \
        void set(const std::string &key, const Eigen::Matrix<type, 3, 1> &v) { \
            mData.emplace(mPrefixStack.top() + key, \
                new Eigen::Matrix<btype, Eigen::Dynamic, Eigen::Dynamic>(v.cast<btype>())); \
        } \
        \
        void set(const std::string &key, const Eigen::Matrix<type, 4, 1> &v) { \
            mData.emplace(mPrefixStack.top() + key, \
                new Eigen::Matrix<btype, Eigen::Dynamic, Eigen::Dynamic>(v.cast<btype>())); \
        } \
        \
        void set(const std::string &key, const Eigen::Matrix<type, 1, Eigen::Dynamic> &v) { \
            mData.emplace(mPrefixStack.top() + key, \
                new Eigen::Matrix<btype, Eigen::Dynamic, Eigen::Dynamic>(v.cast<btype>())); \
        } \
        \
        void set(const std::string &key, const Eigen::Matrix<type, Eigen::Dynamic, 1> &v) { \
            mData.emplace(mPrefixStack.top() + key, \
                new Eigen::Matrix<btype, Eigen::Dynamic, Eigen::Dynamic>(v.cast<btype>().transpose())); \
        } \
        \
        void set(const std::string &key, const Eigen::Matrix<type, Eigen::Dynamic, Eigen::Dynamic> &m) { \
            mData.emplace(mPrefixStack.top() + key, \
                new Eigen::Matrix<btype, Eigen::Dynamic, Eigen::Dynamic>(m.cast<btype>())); \
        } \
        \
        void set(const std::string &key, const std::vector<std::vector<type>> &v) { \
            mData.emplace(mPrefixStack.top() + key, new std::vector<std::vector<btype>>( \
                        reinterpret_cast<const std::vector<std::vector<btype>>&>(v))); \
        } \
        \
        bool get(const std::string &key, type &value) const { \
            auto it = mData.find(mPrefixStack.top() + key); \
            if (it == mData.end()) { MISSING_KEY(key); } \
            if (it->second.type_id != Variant::Type::Matrix_##btype) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!"); \
            if (it->second.matrix_##btype->size() != 1) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": size mismatch!"); \
            value = (type) (*(it->second.matrix_##btype))(0, 0); \
            return true; \
        } \
        \
        bool get(const std::string &key, Eigen::Matrix<type, 2, 1> &v) const { \
            auto it = mData.find(mPrefixStack.top() + key); \
            if (it == mData.end()) { MISSING_KEY(key); } \
            if (it->second.type_id != Variant::Type::Matrix_##btype) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!"); \
            if (it->second.matrix_##btype->cols() != 1 && it->second.matrix_##btype->rows() != 2) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": size mismatch!"); \
            v = it->second.matrix_##btype->col(0).cast<type>(); \
            return true; \
        } \
        \
        bool get(const std::string &key, Eigen::Matrix<type, 3, 1> &v) const { \
            auto it = mData.find(mPrefixStack.top() + key); \
            if (it == mData.end()) { MISSING_KEY(key); } \
            if (it->second.type_id != Variant::Type::Matrix_##btype) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!"); \
            if (it->second.matrix_##btype->cols() != 1 && it->second.matrix_##btype->rows() != 3) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": size mismatch!"); \
            v = it->second.matrix_##btype->col(0).cast<type>(); \
            return true; \
        } \
        \
        bool get(const std::string &key, Eigen::Matrix<type, 4, 1> &v) const { \
            auto it = mData.find(mPrefixStack.top() + key); \
            if (it == mData.end()) { MISSING_KEY(key); } \
            if (it->second.type_id != Variant::Type::Matrix_##btype) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!"); \
            if (it->second.matrix_##btype->cols() != 1 && it->second.matrix_##btype->rows() != 4) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": size mismatch!"); \
            v = it->second.matrix_##btype->col(0).cast<type>(); \
            return true; \
        } \
        \
        bool get(const std::string &key, Eigen::Matrix<type, Eigen::Dynamic, 1> &v) const { \
            auto it = mData.find(mPrefixStack.top() + key); \
            if (it == mData.end()) { MISSING_KEY(key); } \
            if (it->second.type_id != Variant::Type::Matrix_##btype) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!"); \
            if (it->second.matrix_##btype->rows() != 1) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": size mismatch!"); \
            v = it->second.matrix_##btype->row(0).cast<type>().transpose(); \
            return true; \
        } \
        \
        bool get(const std::string &key, Eigen::Matrix<type, 1, Eigen::Dynamic> &v) const { \
            auto it = mData.find(mPrefixStack.top() + key); \
            if (it == mData.end()) { MISSING_KEY(key); } \
            if (it->second.type_id != Variant::Type::Matrix_##btype) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!"); \
            if (it->second.matrix_##btype->rows() != 1) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": size mismatch!"); \
            v = it->second.matrix_##btype->row(0).cast<type>(); \
            return true; \
        } \
        \
        bool get(const std::string &key, Eigen::Matrix<type, Eigen::Dynamic, Eigen::Dynamic> &v) const { \
            auto it = mData.find(mPrefixStack.top() + key); \
            if (it == mData.end()) { MISSING_KEY(key); } \
            if (it->second.type_id != Variant::Type::Matrix_##btype) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!"); \
            v = it->second.matrix_##btype->cast<type>(); \
            return true; \
        } \
        \
        bool get(const std::string &key, std::vector<std::vector<type>> &v) const { \
            auto it = mData.find(mPrefixStack.top() + key); \
            if (it == mData.end()) { MISSING_KEY(key); } \
            if (it->second.type_id != Variant::Type::List_##btype) \
                throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!"); \
            v = reinterpret_cast<const std::vector<std::vector<type>>&>(*(it->second.list_##btype)); \
            return true; \
        }

    IMPLEMENT(bool,     uint8_t)
    IMPLEMENT(uint8_t,  uint8_t)
    IMPLEMENT(uint16_t, uint16_t)
    IMPLEMENT(uint32_t, uint32_t)
    IMPLEMENT(int32_t,  uint32_t)
    IMPLEMENT(float,    float)
    IMPLEMENT(double,   double)

    #undef IMPLEMENT
    #undef MISSING_KEY

    template <typename Scalar, int N> inline void set(const std::string &key, const Eigen::Matrix<Eigen::Matrix<Scalar, N, 1>, 1, Eigen::Dynamic> &m) {
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> temp(N, m.size());
        for (uint32_t i=0; i<m.size(); ++i)
            temp.col(i) = m[i];
        set(key, temp);
    }

    template <typename Scalar, int N> inline bool get(const std::string &key, Eigen::Matrix<Eigen::Matrix<Scalar, N, 1>, 1, Eigen::Dynamic> &m) const {
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> temp;
        if (!get(key, temp))
            return false;
        if (temp.rows() != N && temp.cols() != 0)
            throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": size mismatch!");
        m.resize(temp.cols());
        for (uint32_t i=0; i<temp.cols(); ++i)
            m[i] = temp.col(i);
        return true;
    }

    template <typename Key, typename Value> inline void set(const std::string &key, const std::map<Key, Value> &map) {
        Eigen::Matrix<Key, 1, Eigen::Dynamic> keys(map.size());
        Eigen::Matrix<Value, 1, Eigen::Dynamic> values(map.size());
        uint32_t ctr = 0;
        for (auto const &kv : map) {
            keys[ctr] = kv.first;
            values[ctr++] = kv.second;
        }
        set(key + ".keys", keys);
        set(key + ".values", values);
    }

    template <typename Key, typename Value> inline bool get(const std::string &key, std::map<Key, Value> &map) const {
        Eigen::Matrix<Key, 1, Eigen::Dynamic> keys;
        Eigen::Matrix<Value, 1, Eigen::Dynamic> values;
        if (!get(key + ".keys", keys) || !get(key + ".values", values))
            return false;
        if (keys.size() != values.size())
            throw std::runtime_error("Serializer: element \"" + mPrefixStack.top() + key + "\": type mismatch!");
        map.clear();
        for (uint32_t i=0; i<values.size(); ++i)
            map[keys[i]] = values[i];
        return true;
    }

    template <typename Key> inline void set(const std::string &key, const std::set<Key> &value) {
        Eigen::Matrix<Key, 1, Eigen::Dynamic> keys(value.size());
        uint32_t ctr = 0;
        for (auto const &k : value)
            keys[ctr++] = k;
        set(key + ".keys", keys);
    }

    template <typename Key> inline bool get(const std::string &key, std::set<Key> &value) const {
        Eigen::Matrix<Key, 1, Eigen::Dynamic> keys;
        if (!get(key + ".keys", keys))
            return false;
        value.clear();
        for (uint32_t i=0; i<keys.size(); ++i)
            value.insert(keys[i]);
        return true;
    }

    template <typename Scalar> inline bool get(const std::string &key, Eigen::Quaternion<Scalar> &quat) const {
        Eigen::Matrix<Scalar, 4, 1> coeffs;
        if (!get(key, coeffs))
            return false;
        quat.coeffs() << coeffs;
        return true;
    }

    template <typename Scalar> inline void set(const std::string &key, const Eigen::Quaternion<Scalar> &quat) {
        set(key, Eigen::Matrix<Scalar, 4, 1>(quat.coeffs()));
    }

    inline void set(const std::string &key, const std::string &str) {
        Eigen::Matrix<uint8_t, 1, Eigen::Dynamic> v(str.length());
        memcpy(v.data(), str.c_str(), str.length());
        set(key, v);
    }

    inline bool get(const std::string &key, std::string &str) const {
        Eigen::Matrix<uint8_t, 1, Eigen::Dynamic> v;
        if (!get(key, v))
            return false;
        v.conservativeResize(v.size() + 1);
        v[v.size()-1] = '\0';
        str = (const char *) v.data();
        return true;
    }

    friend std::ostream &operator<<(std::ostream &os, const Serializer &state);

protected:
    struct Variant {
        #define VARIANT_ENUM(type) \
            List_##type, Matrix_##type

        #define VARIANT_UNION(type) \
            const Eigen::Matrix<type, Eigen::Dynamic, Eigen::Dynamic> *matrix_##type; \
            const std::vector<std::vector<type>> *list_##type

        #define VARIANT_CONSTR(type) \
            inline Variant(const Eigen::Matrix<type,  Eigen::Dynamic, Eigen::Dynamic> *value) \
                : type_id(Type::Matrix_##type), matrix_##type(value) { } \
            inline Variant(const std::vector<std::vector<type>> *value) \
                : type_id(Type::List_##type), list_##type(value) { }

        enum Type {
            VARIANT_ENUM(uint8_t),
            VARIANT_ENUM(uint16_t),
            VARIANT_ENUM(uint32_t),
            VARIANT_ENUM(float),
            VARIANT_ENUM(double)
        } type_id;

        union {
            VARIANT_UNION(uint8_t);
            VARIANT_UNION(uint16_t);
            VARIANT_UNION(uint32_t);
            VARIANT_UNION(float);
            VARIANT_UNION(double);
        };

        inline Variant() { }
        VARIANT_CONSTR(uint8_t);
        VARIANT_CONSTR(uint16_t);
        VARIANT_CONSTR(uint32_t);
        VARIANT_CONSTR(float);
        VARIANT_CONSTR(double);

        #undef VARIANT_CONSTR
        #undef VARIANT_UNION
    };

    std::map<std::string, Variant> mData;
    mutable std::stack<std::string> mPrefixStack;
    bool mCompatibilityMode;
};
