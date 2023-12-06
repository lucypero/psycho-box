#include "gen_vec.hpp"

bool genkey_eq(GenKey a, GenKey b) {
    return a.index == b.index && a.gen == b.gen;
}