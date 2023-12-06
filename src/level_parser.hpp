#pragma once

#include "gameplay.hpp"
#include "lucytypes.hpp"

bool load_levels_from_file(string_view filename, vec<LevelNamed> &levels);
