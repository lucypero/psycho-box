#pragma once

#include "lucytypes.hpp"
#include "entity.hpp"

enum struct EaseFunction { Linear, CircOut, CircInOut, SineOut, SineInOut, EaseOutBounce, EaseOutElastic, Count };

enum struct EntityProp { Position, Scale, Color, Rotation };
enum struct AnimatedThing { Entity, Float };

// Determines how to handle other animations on the same entity that are in the queue
enum struct AnimationConflictResolution {
    DoNothing,
    // Kills all other existing animations on the same entity
    KillOthers,
    // Fast forwards all other existing animations on the same entity (t -> 1.0f)
    FastForwardOthers,
};

struct Animated {
    // making it float4 so it can animate pos, scale, rot,
    //   and also color. the fourth component will be ignored when it's not
    //   color
    union {
        struct {
            v4 prev;
            v4 target;
            EntityProp what;
            GenKey entity_id;
        } ent;
        struct {
            f32 *the_float;
            f32 prev;
            f32 target;
        } simple;
    };
    AnimatedThing the_thing = AnimatedThing::Entity;

    f32 t;
    f32 delay_s;
    f32 duration_s;
    AnimationConflictResolution conflict_resolution = AnimationConflictResolution::KillOthers;
    EaseFunction ease_function = EaseFunction::SineInOut;
};

using AnimationSystem = vec<Animated>;

v3 anims_get_future_pos(AnimationSystem &as, EntitySystem &es, GenKey entity_id);
void anims_tick(AnimationSystem &as, EntitySystem &es, f32 dt_sec);