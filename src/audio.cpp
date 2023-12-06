#include "audio.hpp"

#pragma warning(push)
#pragma warning(disable : ALL_CODE_ANALYSIS_WARNINGS)
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#pragma warning(pop)

#include "utils.hpp"

const auto SOUND_DIR = "assets/sounds/"sv;

namespace {

const array sounds = {
    "Lucy7.mp3"sv,           // Music
    "level_finish.mp3"sv,    // Win
    "move.wav"sv,            // Move
    "teleport2.wav"sv,       // Teleport
    "UI_Select_Clean.mp3"sv, // Select
    "UI_Back_Clean.mp3"sv,   // Back
    "teleport3.wav"sv,       // MoveUI
};

}

void Audio::init(AudioData &ad) {
    ma_result result;
    ad.engine = new ma_engine();

    result = ma_engine_init(nullptr, ad.engine);
    lassert_s(result == MA_SUCCESS, "could not initialize audio.");
    ma_engine_set_volume(ad.engine, 0.2f);

    // for (u32 i = 0; const auto &sound_str : sounds) {
    //     string sound_path = string(SOUND_DIR);
    //     sound_path.append(sound_str);

    //     result = ma_sound_init_from_file(ad.engine, sound_path.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL,
    //     &ad.sounds[i]); lassert(result == 0);
    //     ++i;
    // }

    ma_sound_group_init(ad.engine, 0, NULL, &ad.music_group);
    ma_sound_group_init(ad.engine, 0, NULL, &ad.sfx_group);

    ma_sound_group_set_volume(&ad.sfx_group, 0.5f);

    // initing music (should be the first one)
    string sound_path = string(SOUND_DIR);
    sound_path.append(sounds[(u32)Sound::Music]);
    result = ma_sound_init_from_file(ad.engine, sound_path.c_str(), MA_SOUND_FLAG_DECODE, &ad.music_group, NULL,
                                     &ad.music);
    lassert(result == 0);

    ma_sound_set_looping(&ad.music, true);
    ma_sound_start(&ad.music);
}

void Audio::play(AudioData &ad, Sound sound) {
    ma_result result;
    string sound_path = string(SOUND_DIR);
    sound_path.append(sounds[(u32)sound]);
    result = ma_engine_play_sound(ad.engine, sound_path.c_str(), &ad.sfx_group);
    lassert(result == 0);
}