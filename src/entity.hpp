#pragma once

#include "renderer.hpp"
#include "gen_vec.hpp"

struct Entity {
    Shape box;
    bool visible = true;
    bool has_anchor;
    GenKey anchor_id;
};

using EntitySystem = GenVec<Entity>;