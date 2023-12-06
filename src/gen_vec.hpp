#pragma once

#include "lucytypes.hpp"

struct GenKey {
    u64 index;
    u64 gen;
};

bool genkey_eq(GenKey a, GenKey b);

template <typename T>
struct GenEntry {
    T entry;
    u64 gen;
    bool live;
};

template <typename T>
struct GenVec {
    vec<GenEntry<T>> things;
    vec<u64> free_indices;

    T *get(GenKey k) {
        auto *ge = &things[k.index];
        if (ge->gen == k.gen) {
            return &ge->entry;
        }
        return 0;
    }

    void remove(GenKey k) {

        if (things.size() <= k.index)
            return;

        if (things[k.index].gen != k.gen)
            return;

        ++things[k.index].gen;
        things[k.index].live = false;
        free_indices.push_back(k.index);
    }

    GenKey add(T entry) {
        if (free_indices.size() > 0) {
            u64 index = free_indices.back();
            free_indices.pop_back();
            things[index].entry = entry;
            things[index].live = true;
            GenKey key = {};
            key.index = index;
            key.gen = things[index].gen;
            return key;
        }
        GenEntry<T> ge = {};
        ge.entry = entry;
        ge.gen = 0;
        ge.live = true;
        things.push_back(ge);
        GenKey key = {};
        key.index = things.size() - 1;
        key.gen = 0;
        return key;
    }

    // safe way to "clear"
    void remove_all() {
        free_indices.clear();
        free_indices.reserve(things.size());

        for (u64 i = 0; auto &ge : things) {
            ++ge.gen;
            ge.live = false;
            free_indices.push_back(i);
            ++i;
        }
    }

    void clear() {
        things.clear();
        free_indices.clear();
    }
};
