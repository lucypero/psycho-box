#pragma once

#include "lucytypes.hpp"

#include <codeanalysis\warnings.h>
#pragma warning(push)
#pragma warning(disable : ALL_CODE_ANALYSIS_WARNINGS)
#include "miniaudio.h"
#pragma warning(pop)

enum struct Sound : u32 { Music, Win, Move, Teleport, Select, Back, MoveUI };

struct AudioData {
    ma_engine *engine;
    ma_sound music;
    ma_sound_group music_group;
    ma_sound_group sfx_group;
    // array<ma_sound, sounds.size()> sounds;
};

namespace Audio {
void init(AudioData &ad);
void play(AudioData &ad, Sound sound);
}; // namespace Audio
