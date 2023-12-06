#include "animation.hpp"

namespace {

f32 tween_apply_ease(f32 t, EaseFunction ef) {

    // more ease functions here
    //  https://easings.net/#

    f32 t_f;

    // here u switch between tweening functions
    switch (ef) {
    case EaseFunction::Linear: {
        t_f = t;
    } break;
    case EaseFunction::CircOut: {
        t_f = sqrtf(1.0f - powf(t - 1.0f, 2.0f));
    } break;
    case EaseFunction::CircInOut: {
        if (t < 0.5f) {
            t_f = (1.0f - sqrtf(1.0f - powf(2.0f * t, 2.0f))) / 2.0f;
        } else {
            t_f = (sqrtf(1.0f - powf(-2.f * t + 2.0f, 2.0f)) + 1) / 2.0f;
        }
    } break;
    case EaseFunction::SineOut: {
        t_f = sin((t * math::Pi) / 2.f);
    } break;
    case EaseFunction::SineInOut: {
        t_f = -(cosf(math::Pi * t) - 1.f) / 2.f;
    } break;
    case EaseFunction::EaseOutBounce: {
        const f32 N1 = 7.5625f;
        const f32 D1 = 2.75f;

        if (t < (1.0f / D1)) {
            t_f = N1 * t * t;
        } else if (t < (2.0f / D1)) {
            f32 temp = t - (1.5f / D1);
            t_f = N1 * temp * temp + 0.75f;
        } else if (t < (2.5f / D1)) {
            f32 temp = t - (2.25f / D1);
            t_f = N1 * temp * temp + 0.9375f;
        } else {
            f32 temp = t - (2.625f / D1);
            t_f = N1 * temp * temp + 0.984375f;
        }
    } break;
    case EaseFunction::EaseOutElastic: {
        const f32 C4 = (2.0f * math::Pi) / 5.0f;

        if (t < 0.0f) {
            t_f = 0.0f;
        } else if (t > 1.0f) {
            t_f = 1.0f;
        } else {
            t_f = powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * C4) + 1.0f;
        }
    } break;
    default:
        t_f = 0.0f;
        break;
    }

    return t_f;
}

v3 tween_v3(v3 start, v3 end, f32 t, EaseFunction ef) {
    f32 t_f = tween_apply_ease(t, ef);

    XMVECTOR v_s = XMLoadFloat3(&start);
    XMVECTOR v_e = XMLoadFloat3(&end);

    XMVECTOR v_res = v_s * (1.0f - t_f) + v_e * t_f;

    v3 res;

    XMStoreFloat3(&res, v_res);

    return res;
}

v4 tween_v4(v4 start, v4 end, f32 t, EaseFunction ef) {
    f32 t_f = tween_apply_ease(t, ef);

    XMVECTOR v_s = XMLoadFloat4(&start);
    XMVECTOR v_e = XMLoadFloat4(&end);

    XMVECTOR v_res = v_s * (1.0f - t_f) + v_e * t_f;

    v4 res;

    XMStoreFloat4(&res, v_res);

    return res;
}

f32 tween_f(f32 start, f32 end, f32 t, EaseFunction ef) {
    f32 t_f = tween_apply_ease(t, ef);
    return start * (1.0f - t_f) + end * t_f;
}

void do_animation_tick(Animated &a, Entity &ent, bool fast_forward = false) {

    f32 the_t = a.t;

    if (fast_forward)
        the_t = 1.0f;

    EaseFunction the_ease = a.ease_function;

    switch (a.ent.what) {
    case EntityProp::Position: {
        v4 new_val = tween_v4(a.ent.prev, a.ent.target, the_t, the_ease);
        ent.box.pos = math::v4tov3(new_val);
    } break;
    case EntityProp::Scale: {
        v4 new_val = tween_v4(a.ent.prev, a.ent.target, the_t, the_ease);
        ent.box.scale = math::v4tov3(new_val);
    } break;
    case EntityProp::Color: {
        v4 new_val = tween_v4(a.ent.prev, a.ent.target, the_t, the_ease);
        ent.box.color = new_val;
    } break;
    case EntityProp::Rotation: {
        v3 new_axis = tween_v3(math::v4tov3(a.ent.prev), math::v4tov3(a.ent.target), the_t, the_ease);
        f32 new_angle = tween_f(a.ent.prev.w, a.ent.target.w, the_t, the_ease);
        ent.box.rot = v4(new_axis.x, new_axis.y, new_axis.z, new_angle);
    } break;
    }
}

bool handle_anim_conflict(vec<Animated>::iterator &it, vec<Animated> &anims, EntitySystem &es, Animated &a) {

    for (auto next_it = it + 1; next_it != anims.end(); ++next_it) {
        const Animated &anim_next = *next_it;

        bool targets_same_entity = anim_next.the_thing == AnimatedThing::Entity &&
                                   genkey_eq(a.ent.entity_id, anim_next.ent.entity_id) &&
                                   a.ent.what == anim_next.ent.what;

        if (!targets_same_entity)
            continue;

        switch (anim_next.conflict_resolution) {
        case AnimationConflictResolution::KillOthers: {
            // Found a future animation that should kill all others. so, this animation is killed.
            it = anims.erase(it);
            return true;
        } break;
        case AnimationConflictResolution::FastForwardOthers: {
            // Found a future animation that should fast forward all ohers. so, this animation is fast forwarded,
            // and killed.

            // only fast forward animations that don't have this conflict resolution
            if (a.conflict_resolution == AnimationConflictResolution::FastForwardOthers)
                continue;

            Entity *ent = es.get(a.ent.entity_id);
            if (!ent) {
                it = anims.erase(it);
                return true;
            }

            do_animation_tick(a, *ent, true);
            it = anims.erase(it);
            return true;

        } break;
        }
    }

    return false;
}

} // namespace

// --------------------------------- EXPORTED FUNCTIONS (START) ---------------------------------

void anims_tick(AnimationSystem &as, EntitySystem &es, f32 dt_sec) {
    auto it = as.begin();

    while (it != as.end()) {
        auto &a = *it;

        bool anim_done = false;

        a.delay_s -= dt_sec;
        if (a.delay_s > F32_EPSILON) {
            ++it;
            continue;
        }

        a.t += dt_sec / a.duration_s;
        if (a.t > 1.0f) {
            a.t = 1.0f;
            anim_done = true;
        }

        switch (a.the_thing) {
        case AnimatedThing::Entity: {
            // delete animation if there's another animation targeting the same
            //   entity and the thing to animate later on

            bool should_continue = handle_anim_conflict(it, as, es, a);

            if (should_continue) {
                continue;
            }

            // delete animation if the entity is not found
            Entity *ent = es.get(a.ent.entity_id);
            if (!ent) {
                it = as.erase(it);
                continue;
            }

            do_animation_tick(a, *ent);
        } break;
        case AnimatedThing::Float: {
            *a.simple.the_float = tween_f(a.simple.prev, a.simple.target, a.t, a.ease_function);
        } break;
        }

        if (anim_done) {
            it = as.erase(it);
        } else {
            ++it;
        }
    }
}

// looks up if there's an animation targeting the entity's position, and returns the position at t = 1.0f
//  it will take the LAST animation in the queue targeting this.
//  if there's no animation, it returns the current position
// v3 anims_get_future_pos(App &app, GenKey entity_id) {
v3 anims_get_future_pos(AnimationSystem &as, EntitySystem &es, GenKey entity_id) {

    bool anim_found = false;
    Animated *the_anim = 0;

    for (Animated &a : as) {
        bool found = a.the_thing == AnimatedThing::Entity && genkey_eq(a.ent.entity_id, entity_id) &&
                     a.ent.what == EntityProp::Position;

        if (found) {
            anim_found = true;
            the_anim = &a;
        }
    }

    if (anim_found) {
        return math::v4tov3(the_anim->ent.target);
    } else {
        Entity *ent = es.get(entity_id);
        lassert(ent);
        return ent->box.pos;
    }
}
