#include "app.hpp"

#include "assimp/mesh.h"
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <stdint.h>
#include <vcruntime.h>

#include "level_parser.hpp"
#include "utils.hpp"
#include "audio.hpp"

#include "debug_gui.hpp"

namespace {

const f32 unit_length = 1.0f;
const f32 level_transition_add = unit_length * 16.0f;
const f32 cam_rot_duration = 0.3f;

const f32 move_anim_duration = 0.1f;

// -- level transition consts --
const f32 anchor_distance = 40.0f;
const v3 anchor_pos = v3(0.0f, anchor_distance, 0.0f);
const f32 transition_rot_duration = 1.0f;
const EaseFunction level_rot_ease_function = EaseFunction::SineInOut;
// -- /end --

const v4 BOX_COLOR = v4(0.76f, 0.6f, 0.75f, 1.0f);
const v4 MIRROR_COLOR = v4(0.5f, 0.5f, 0.7f, 0.7f);
const v3 BOX_SCALE = v3(0.8f, 0.8f, 0.8f);
const v4 FLOOR_COLOR = math::v4_one();
const v3 FLOOR_SCALE = math::v3_one();

v3 coord_to_v3(Coord c) {
    return v3(unit_length * c.x, -anchor_distance, unit_length * -c.y);
}

f32 get_elevation(const Shape &shape) {

    f32 shape_elev = 0.0f;

    switch (shape.kind) {
    case ShapeKind::Box:
        shape_elev = unit_length;
        break;
    case ShapeKind::Sphere:
        shape_elev = 0.2f;
        break;
    case ShapeKind::TriangularPrism:
        shape_elev = 0.82f;
        break;
    case ShapeKind::Goal:
        shape_elev = unit_length;
        break;
    case ShapeKind::BoxPlayer:
        shape_elev = unit_length;
        break;
    case ShapeKind::Count:
        shape_elev = unit_length;
        break;
    }

    return shape_elev - ((1.0f - shape.scale.y) * 0.5f);
}

void reset_cam_angle(App &app) {
    app.cam_controls.cam_y_angle = math::make_angle(app.cam_controls.cam_y_angle);
    app.cam_controls.cam_y_angle = math::make_closest(app.cam_controls.cam_y_angle, initial_cam_angle);
    app.current_angle = initial_cam_angle;

    Animated a = {};
    a.the_thing = AnimatedThing::Float;
    a.simple.the_float = &app.cam_controls.cam_y_angle;
    a.simple.prev = app.cam_controls.cam_y_angle;
    a.simple.target = app.current_angle;
    a.duration_s = cam_rot_duration;
    app.as.push_back(a);
}

// sets entities to reflect given level state
void set_entities(App &app, Level &level, bool do_transition_anim) {
    // clearing old level state
    app.es.clear();
    timers_defer_clear(app.ts);
    app.as.clear();
    app.player_has_control = true;
    app.anchors.clear();
    app.preview_keys.clear();

    if (do_transition_anim) {
        app.current_angle = initial_cam_angle;
        app.cam_controls.cam_y_angle = initial_cam_angle;
    }

    // instantiating entities

    v3 start_pos = anchor_pos;

    if (do_transition_anim) {
        start_pos.x += level.width * unit_length + level_transition_add;
    }

    // make anchor
    Entity anchor_e = {};
    anchor_e.box.pos = start_pos;
    anchor_e.box.color = v4(1.f, 1.f, 1.f, 1.f);
    anchor_e.visible = false;
    GenKey anchor_id = app.es.add(anchor_e);
    app.anchors.push_back(anchor_id);

    if (do_transition_anim) {
        // crazy rotation animation
        Animated a = {};
        a.the_thing = AnimatedThing::Entity;
        a.ent.entity_id = anchor_id;
        a.ent.prev = math::v3tov4(start_pos);
        a.ent.target = math::v3tov4(anchor_pos);
        a.ent.what = EntityProp::Position;
        a.duration_s = transition_rot_duration;
        a.ease_function = level_rot_ease_function;
        app.as.push_back(a);
    }

    LevelPlane &lp = level.data[0];

    for (u32 w = 0; w < level.width; ++w) {
        for (u32 h = 0; h < level.height; ++h) {

            auto &cell = lp[w][h];

            if (cell_is_empty(cell)) {
                continue;
            }

            Coord c = {(i32)w, (i32)h};
            const v3 def_pos = coord_to_v3(c);

            for (auto &te : cell) {
                switch (te.tile) {
                case Tile::Empty:
                    break;
                case Tile::Floor: {
                    Entity e = {};
                    e.box.pos = def_pos;
                    e.box.color = FLOOR_COLOR;
                    e.box.scale = FLOOR_SCALE;
                    e.has_anchor = true;
                    e.anchor_id = anchor_id;
                    GenKey k = app.es.add(e);
                    te.has_entity = true;
                    te.entity_id = k;
                } break;
                case Tile::Wall: {
                    Entity e = {};
                    e.box.pos = def_pos;
                    e.box.pos.y += get_elevation(e.box);
                    e.box.pos.y -= 0.25f;
                    e.box.scale.y = 0.5f;
                    e.has_anchor = true;
                    e.box.color = v4(0.24f, 0.08f, 0.24f, 0.8f);
                    e.anchor_id = anchor_id;
                    GenKey k = app.es.add(e);
                    te.has_entity = true;
                    te.entity_id = k;
                } break;
                case Tile::Player: {
                    Entity e = {};
                    e.box.kind = ShapeKind::BoxPlayer;
                    e.box.pos = def_pos;
                    e.box.color = math::v4_red();
                    e.box.scale = v3(0.7f, 0.7f, 0.7f);
                    e.has_anchor = true;
                    e.anchor_id = anchor_id;
                    e.box.pos.y += get_elevation(e.box);
                    GenKey k = app.es.add(e);
                    te.has_entity = true;
                    te.entity_id = k;
                } break;
                case Tile::Box: {
                    Entity e = {};
                    e.box.pos = def_pos;
                    e.box.color = BOX_COLOR;
                    e.box.scale = BOX_SCALE;
                    e.has_anchor = true;
                    e.anchor_id = anchor_id;
                    e.box.pos.y += get_elevation(e.box);
                    GenKey k = app.es.add(e);
                    te.has_entity = true;
                    te.entity_id = k;
                } break;
                case Tile::MirrorUL:
                case Tile::MirrorUR:
                case Tile::MirrorDL:
                case Tile::MirrorDR: {
                    Entity e = {};
                    e.box.kind = ShapeKind::TriangularPrism;
                    e.box.pos = def_pos;

                    e.box.rot.y = 1.0;
                    switch (te.tile) {
                    case Tile::MirrorUL:
                        e.box.rot.w = -math::Tau * 0.25f;
                        break;
                    case Tile::MirrorUR:
                        e.box.rot.w = 0.0f;
                        break;
                    case Tile::MirrorDL:
                        e.box.rot.w = math::Tau * 0.5f;
                        break;
                    case Tile::MirrorDR:
                        e.box.rot.w = math::Tau * 0.25f;
                        break;
                    }

                    e.box.color = MIRROR_COLOR;
                    e.box.scale = v3(1.f, 1.f, 1.f);
                    e.has_anchor = true;
                    e.anchor_id = anchor_id;
                    e.box.pos.y += get_elevation(e.box);
                    GenKey k = app.es.add(e);
                    te.has_entity = true;
                    te.entity_id = k;
                } break;
                case Tile::Goal: {
                    Entity e = {};
                    e.box.kind = ShapeKind::Goal;
                    e.box.pos = def_pos;
                    e.box.color = v4(1.f, 0.98f, 0.5f, 0.5f);
                    e.has_anchor = true;
                    e.anchor_id = anchor_id;
                    e.box.pos.y += get_elevation(e.box);
                    GenKey k = app.es.add(e);
                    te.has_entity = true;
                    te.entity_id = k;
                } break;
                }
            }
        }
    }
}

#ifdef _DEBUG
void run_genvec_tests() {

    struct Hello {
        i32 haha;
    };

    GenVec<Hello> hellos = {};

    GenKey k, k2, k3;
    hellos.add(Hello{1});
    k = hellos.add(Hello{2});
    hellos.add(Hello{3});
    hellos.add(Hello{4});
    k2 = hellos.add(Hello{5});
    k3 = hellos.add(Hello{6});
    hellos.add(Hello{7});

    hellos.remove(k);
    hellos.remove(k3);

    hellos.add(Hello{8});
    hellos.add(Hello{9});
    hellos.add(Hello{10});

    for (u64 i = 0; i < hellos.things.size(); ++i) {
        auto *ge = &hellos.things[i];
        if (!ge->live) {
            continue;
        }

        if (i == 0) {
            lassert(ge->entry.haha == 1);
        }
        if (i == 5) {
            lassert(ge->entry.haha == 8);
        }

        ++ge->entry.haha;

        if (i == 4) {
            hellos.remove(GenKey{i, ge->gen});
        }
    }

    Hello *h = hellos.get(k2);
    lassert(h == nullptr);

    lassert(hellos.things.size() == 8);
    lassert(hellos.free_indices.size() == 1);
    lassert(hellos.free_indices[0] == 4);

    for (u64 i = 0; i < hellos.things.size(); ++i) {
        auto *ge = &hellos.things[i];

        if (i == 4) {
            lassert(ge->live == false);
        } else {
            lassert(ge->live == true);
        }
    }
}
#endif

void feed_boxes_to_renderer(App &app, Renderer &r) {

    vec<Shape> hierarchy;
    hierarchy.reserve(4);

    // looping entities
    for (u64 i = 0; i < app.es.things.size(); ++i) {
        auto &ge = app.es.things[i];

        if (!ge.live) {
            continue;
        }

        if (!ge.entry.visible) {
            continue;
        }

        hierarchy.clear();
        hierarchy.push_back(ge.entry.box);

        // constructing hierarchy
        {
            Entity *e_temp = &ge.entry;

            while (e_temp->has_anchor) {
                Entity *a = app.es.get(e_temp->anchor_id);
                lassert(a != nullptr);
                hierarchy.push_back(a->box);
                e_temp = a;
            }
        }

        Shape new_box = {};

        // reverse iteration of the hierarchy (top to bottom)
        for (i32 i_hier = (i32)hierarchy.size() - 1; i_hier >= 0; --i_hier) {
            Shape &b = hierarchy[i_hier];

            XMMATRIX anchor_rot_mat;

            // the position calc
            {
                XMVECTOR box_point = XMLoadFloat3(&b.pos);
                XMVECTOR anchor_center = XMLoadFloat3(&new_box.pos);

                XMVECTOR translated_point = box_point;

                if (math::rot_is_valid(new_box.rot)) {
                    XMVECTOR anchor_rot = XMLoadFloat4(&new_box.rot);
                    anchor_rot_mat = XMMatrixRotationAxis(anchor_rot, new_box.rot.w);
                } else {
                    anchor_rot_mat = XMMatrixIdentity();
                }

                XMVECTOR transformed_point = XMVector3Transform(translated_point, anchor_rot_mat);

                // translate the point back to the original position
                transformed_point += anchor_center;

                XMStoreFloat3(&new_box.pos, transformed_point);
            }

            // color calc (alpha mul)
            new_box.color = v4(b.color.x, b.color.y, b.color.z, new_box.color.w * b.color.w);

            // scale calc
            new_box.scale = math::v3_mul(b.scale, new_box.scale);

            // rot calc
            if (math::rot_is_valid(b.rot)) {
                XMVECTOR box_rot = XMLoadFloat4(&b.rot);
                XMMATRIX box_rot_mat = XMMatrixRotationAxis(box_rot, b.rot.w);
                XMMATRIX new_box_rot = box_rot_mat * anchor_rot_mat;

                // converting rot matrix back to axis+angle
                XMVECTOR rot_quat;
                XMVECTOR _unused;
                XMMatrixDecompose(&_unused, &rot_quat, &_unused, new_box_rot);
                XMVECTOR rot_axis;
                f32 rot_angle;
                XMQuaternionToAxisAngle(&rot_axis, &rot_angle, rot_quat);
                XMStoreFloat4(&new_box.rot, rot_axis);
                new_box.rot.w = rot_angle;
            }

            new_box.kind = b.kind;
        }

        rend::draw_shape(r, new_box);
    }
}

void delete_entity(void *app_void, TimerProcData data) {
    App &app = *(App *)app_void;

    lassert(data.what == TimerDataWhat::GenKey);
    app.es.remove(data.data.entity_key);
}

void level_finish_anim_level(void *app_void, TimerProcData _data) {
    App &app = *(App *)app_void;

    for (auto anchor : app.anchors) {
        Entity *e = app.es.get(anchor);
        if (!e) {
            continue;
        }
        Animated a = {};

        a.ent.entity_id = anchor;
        a.ent.prev = math::v3tov4(anchor_pos);

        // find the bounds of the level
        Level &level = app.levels[app.current_level].level;
        a.ent.target = a.ent.prev;
        a.ent.target.x -= level.width * unit_length + level_transition_add;
        a.ent.what = EntityProp::Position;
        a.duration_s = transition_rot_duration;
        a.ease_function = level_rot_ease_function;
        app.as.push_back(a);
    }
}

void level_finish_anim_switch(void *app_void, TimerProcData _data) {
    App &app = *(App *)app_void;
    app.as.clear();

    if (app.current_level + 1 < app.levels.size()) {
        app_switch_to_level(app, app.current_level + 1, true);
    } else {
        app.completed_game = true;
    }
}

void level_finish_anim_entities(void *app_void, TimerProcData _data) {

    App &app = *(App *)app_void;

    for (u32 plane_i = 0; const auto &p : app.level_c.data) {
        if (plane_i >= app.level_c.plane_count)
            break;
        for (i32 col_i = 0; const auto &column : p) {
            for (i32 row_i = 0; const auto &cell : column) {
                for (const auto &te : cell) {
                    if (te.tile == Tile::Player) {

                        if (!te.has_entity)
                            continue;
                        Entity *e = app.es.get(te.entity_id);
                        if (!e) {
                            continue;
                        }

                        Coord coord_now = Coord{col_i, row_i};

                        Animated a = {};
                        a.the_thing = AnimatedThing::Entity;
                        a.ent.entity_id = te.entity_id;
                        a.ent.prev = math::v3tov4(e->box.pos);

                        v3 target_pos = coord_to_v3(coord_now);
                        target_pos.y += unit_length * 20.0f;

                        a.ent.target = math::v3tov4(target_pos);
                        a.ent.what = EntityProp::Position;
                        a.duration_s = 3.0f;
                        app.as.push_back(a);

                        a.ent.what = EntityProp::Color;
                        a.ent.prev = e->box.color;

                        v4 color_target = e->box.color;
                        color_target.w = 0.f;
                        a.duration_s = 1.0f;
                        a.ent.target = color_target;

                        app.as.push_back(a);
                    }
                }
                ++row_i;
            }
            ++col_i;
        }
        ++plane_i;
    }
}

string coord_to_string(Coord c) {
    return format("({}, {})", c.x, c.y);
}

string tile_to_string(Tile tile) {
    switch (tile) {
    case Tile::Empty:
        return "Empty";
    case Tile::Floor:
        return "Floor";
    case Tile::Wall:
        return "Wall";
    case Tile::Player:
        return "Player";
    case Tile::Box:
        return "Box";
    case Tile::Goal:
        return "Goal";
    case Tile::MirrorUL:
        return "MirrorUL";
    case Tile::MirrorUR:
        return "MirrorUR";
    case Tile::MirrorDL:
        return "MirrorDL";
    case Tile::MirrorDR:
        return "MirrorDR";
    default:
        return "Unknown";
    }
}

string te_to_string(TileEntity te) {
    return format("TileEntity ({}, {})", tile_to_string(te.tile), te.id);
}

void log_events(span<GameEvent> events) {

    if (events.size() != 0) {
        log("--- Game Tick Events: ---");
    }

    for (const auto &ev : events) {
        string log_str = {};
        log_str += "Game Event: ";

        switch (ev.kind) {
        case EventKind::NormalMove: {
            log_str += format("{} moved from {} to {}", te_to_string(ev.te), coord_to_string(ev.from),
                              coord_to_string(ev.to));
        } break;
        case EventKind::BoxFall: {
            log_str += format("{} from {} fell to a void in {} and became a bridge", te_to_string(ev.te),
                              coord_to_string(ev.from), coord_to_string(ev.to));
        } break;
        case EventKind::MirrorTeleport: {
            log_str += format("{} mirror teleported from {} to {}", te_to_string(ev.te), coord_to_string(ev.from),
                              coord_to_string(ev.to));
        } break;
        case EventKind::PlayerFall: {
            log_str += format("Player jumped to its death from {} to {}. Game over.", coord_to_string(ev.from),
                              coord_to_string(ev.to));
        } break;
        case EventKind::Won: {
            log_str += "Level completed!";
        } break;
        }

        log("%s", log_str.c_str());
    }
}

void level_finish_anim_ripple(App &app) {

    // getting goal coord
    Coord goal_coord = get_goal_coord(app.level_c);

    for (u32 plane_i = 0; const auto &p : app.level_c.data) {
        if (plane_i >= app.level_c.plane_count)
            break;
        for (i32 col_i = 0; const auto &column : p) {
            for (i32 row_i = 0; const auto &cell : column) {
                for (const auto &te : cell) {
                    if (te.tile != Tile::Player || te.tile == Tile::Goal) {
                        if (!te.has_entity)
                            continue;
                        Entity *e = app.es.get(te.entity_id);
                        if (!e) {
                            continue;
                        }

                        // calculate distance to goal
                        Coord coord_now = Coord{col_i, row_i};
                        v2 dist_to_goal_vec = v2(f32(goal_coord.x - coord_now.x), f32(goal_coord.y - coord_now.y));
                        XMVECTOR v = XMLoadFloat2(&dist_to_goal_vec);
                        XMVECTOR v_l = XMVector2Length(v);
                        f32 len;
                        XMStoreFloat(&len, v_l);

                        v3 future_pos = anims_get_future_pos(app.as, app.es, te.entity_id);

                        array<v3, 3> keyframes = {
                            v3(future_pos.x, future_pos.y - unit_length * 0.5f, future_pos.z),
                            v3(future_pos.x, future_pos.y + unit_length * 0.5f, future_pos.z),
                            v3(future_pos.x, future_pos.y, future_pos.z),
                        };

                        Animated a = {};
                        a.ent.entity_id = te.entity_id;
                        a.ent.prev = math::v3tov4(future_pos);
                        a.ent.target = math::v3tov4(keyframes[0]);
                        a.ent.what = EntityProp::Position;
                        a.delay_s = len * 0.03f;
                        a.duration_s = 0.1f;
                        a.conflict_resolution = AnimationConflictResolution::FastForwardOthers;
                        a.ease_function = EaseFunction::SineInOut;
                        app.as.push_back(a);

                        v4 temp = a.ent.target;
                        a.ent.target = math::v3tov4(keyframes[1]);
                        a.ent.prev = temp;
                        a.delay_s += a.duration_s;
                        app.as.push_back(a);

                        temp = a.ent.target;
                        a.ent.target = math::v3tov4(keyframes[2]);
                        a.ent.prev = temp;
                        a.delay_s += a.duration_s;
                        app.as.push_back(a);
                    }
                }
                ++row_i;
            }
            ++col_i;
        }
        ++plane_i;
    }
}

void app_update_boxes(App &app, span<GameEvent> eks) {

    const f32 move_dur = move_anim_duration;

    bool has_player_fall = false;

    for (const auto &ev : eks) {
        if (ev.kind == EventKind::PlayerFall) {
            has_player_fall = true;
            break;
        }
    }

    for (const auto &ev : eks) {
        switch (ev.kind) {
        case EventKind::NormalMove: {
            if (!ev.te.has_entity)
                continue;

            Entity *e = app.es.get(ev.te.entity_id);
            if (!e) {
                continue;
            }

            v3 pos_to = coord_to_v3(ev.to);
            pos_to.y += get_elevation(e->box);
            Animated a = {};
            a.ent.entity_id = ev.te.entity_id;
            a.ent.prev = math::v3tov4(e->box.pos);
            a.ent.target = math::v3tov4(pos_to);
            a.ent.what = EntityProp::Position;
            a.duration_s = move_dur;
            app.as.push_back(a);
        } break;
        case EventKind::MirrorTeleport: {
            if (!ev.te.has_entity)
                continue;

            Entity *e = app.es.get(ev.te.entity_id);
            if (!e) {
                continue;
            }

            v4 color = e->box.color;

            // delete position animation if there is one
            auto remove_start = std::remove_if(app.as.begin(), app.as.end(), [&](const Animated &anim) {
                return genkey_eq(anim.ent.entity_id, ev.te.entity_id) && anim.ent.what == EntityProp::Position;
            });
            app.as.erase(remove_start, app.as.end());

            auto clone_key = app.es.add(*e);
            Animated a = {};
            a.ent.entity_id = clone_key;
            a.ent.prev = color;
            color.w = 0.0f;
            a.ent.target = color;
            a.ent.what = EntityProp::Color;
            a.duration_s = 0.3f;
            app.as.push_back(a);

            TimerProcData tpc = {};
            tpc.what = TimerDataWhat::GenKey;
            tpc.data.entity_key = clone_key;
            timer_add_data(app.ts, move_dur, delete_entity, tpc);

            e->box.pos = coord_to_v3(ev.to);
            e->box.pos.y += get_elevation(e->box);

            if (!has_player_fall)
                e->box.color.w = 0.0f;

            a.ent.entity_id = ev.te.entity_id;
            color.w = 0.0f;
            a.ent.prev = color;
            color.w = 1.0f;
            a.ent.target = color;

            if (!has_player_fall)
                app.as.push_back(a);
        } break;
        case EventKind::BoxFall: {
            if (!ev.te.has_entity)
                continue;

            Entity *e = app.es.get(ev.te.entity_id);
            if (!e) {
                continue;
            }

            v3 pos_to = coord_to_v3(ev.to);
            pos_to.y += get_elevation(e->box);

            Animated a = {};
            a.ent.entity_id = ev.te.entity_id;
            a.ent.prev = math::v3tov4(e->box.pos);
            a.ent.target = math::v3tov4(pos_to);
            a.ent.what = EntityProp::Position;
            a.conflict_resolution = AnimationConflictResolution::DoNothing;
            a.duration_s = move_dur;
            app.as.push_back(a);

            // fall to snap into floor animation
            a.ent.prev = math::v3tov4(pos_to);
            v3 pos_void = coord_to_v3(ev.to);
            a.ent.target = math::v3tov4(pos_void);
            a.ent.what = EntityProp::Position;
            a.delay_s = move_dur;
            a.duration_s = move_dur;
            app.as.push_back(a);

            // color anim
            a.ent.prev = e->box.color;
            a.ent.target = FLOOR_COLOR;
            a.ent.what = EntityProp::Color;
            a.duration_s = move_dur;
            a.delay_s = move_dur;
            app.as.push_back(a);

            // scale anim
            a.ent.prev = math::v3tov4(e->box.scale);
            a.ent.target = math::v3tov4(FLOOR_SCALE);
            a.ent.what = EntityProp::Scale;
            a.duration_s = move_dur;
            a.delay_s = move_dur;
            app.as.push_back(a);
        } break;
        case EventKind::PlayerFall: {
            if (!ev.te.has_entity)
                continue;

            Entity *e = app.es.get(ev.te.entity_id);
            if (!e) {
                continue;
            }

            v3 pos_to = coord_to_v3(ev.to);
            pos_to.y += get_elevation(e->box);

            Animated a = {};
            a.ent.entity_id = ev.te.entity_id;
            a.ent.what = EntityProp::Position;
            a.ent.prev = math::v3tov4(pos_to);
            v3 pos_void = pos_to;
            pos_void.y -= unit_length * 30.0f;
            a.ent.target = math::v3tov4(pos_void);
            a.duration_s = 5.0f;
            a.delay_s = move_dur;
            a.conflict_resolution = AnimationConflictResolution::DoNothing;
            app.as.push_back(a);

            // color anim
            a.ent.prev = e->box.color;
            a.ent.target = e->box.color;
            a.ent.target.w = 0.0f;
            a.ent.what = EntityProp::Color;
            a.duration_s = 5.0f;
            a.delay_s = move_dur;
            app.as.push_back(a);
        } break;
        case EventKind::Won: {
            reset_cam_angle(app);
            level_finish_anim_ripple(app);
            timer_add(app.ts, 0.2f, level_finish_anim_entities);
            timer_add(app.ts, 0.2f + 0.5f, level_finish_anim_level);
            timer_add(app.ts, 0.2f + 0.5f + transition_rot_duration, level_finish_anim_switch);
        } break;
        default:
            break;
        }
    }
}

void map_input_actions(Input &in) {
    in.action_new(Action::Jump);
    in.action_add_key(Action::Jump, Key::SPACE);
    in.action_add_key(Action::Jump, Key::ENTER);
    in.action_add_joy_button(Action::Jump, JoyButton::A);

    in.action_new(Action::MoveUp);
    in.action_add_key(Action::MoveUp, Key::W);
    in.action_add_key(Action::MoveUp, Key::UP);
    in.action_add_joy_button(Action::MoveUp, JoyButton::DPAD_UP);

    in.action_new(Action::MoveDown);
    in.action_add_key(Action::MoveDown, Key::S);
    in.action_add_key(Action::MoveDown, Key::DOWN);
    in.action_add_joy_button(Action::MoveDown, JoyButton::DPAD_DOWN);

    in.action_new(Action::MoveLeft);
    in.action_add_key(Action::MoveLeft, Key::A);
    in.action_add_key(Action::MoveLeft, Key::LEFT);
    in.action_add_joy_button(Action::MoveLeft, JoyButton::DPAD_LEFT);

    in.action_new(Action::MoveRight);
    in.action_add_key(Action::MoveRight, Key::D);
    in.action_add_key(Action::MoveRight, Key::RIGHT);
    in.action_add_joy_button(Action::MoveRight, JoyButton::DPAD_RIGHT);

    in.action_new(Action::Undo);
    in.action_add_key(Action::Undo, Key::U);
    in.action_add_key(Action::Undo, Key::Z);
    in.action_add_joy_button(Action::Undo, JoyButton::X);

    in.action_new(Action::Reset);
    in.action_add_key(Action::Reset, Key::R);
    in.action_add_joy_button(Action::Reset, JoyButton::Y);

    in.action_new(Action::Back);
    in.action_add_key(Action::Back, Key::ESCAPE);
    in.action_add_joy_button(Action::Back, JoyButton::B);

    in.action_new(Action::CameraLeft);
    in.action_add_key(Action::CameraLeft, Key::V);
    in.action_add_joy_button(Action::CameraLeft, JoyButton::LS);

    in.action_new(Action::CameraRight);
    in.action_add_key(Action::CameraRight, Key::B);
    in.action_add_joy_button(Action::CameraRight, JoyButton::RS);
}

void gamestate_menu_tick(App &app, Ctx &ctx, f32 dt_sec) {
    Input &in = *ctx.input;
    Renderer &r = *ctx.renderer;
    MenuState &ms = app.menu_state;
    auto &au = *ctx.audio;

    const auto on_exit = [&app, &ms](GameState next_state) {
        ms.thing_selected = 0;
        // app.text_cam_pos.y = 0.f;
        app.game_state = next_state;
    };

    const array menu_strs = {"Play"sv, "Level Select"sv, "Info"sv, "Toggle Fullscreen"sv, "Exit"sv};
    const u32 items_count = (u32)menu_strs.size();

    if (in.was_up(Action::MoveDown)) {
        Audio::play(au, Sound::MoveUI);
        if (ms.thing_selected == items_count - 1) {
            ms.thing_selected = 0;
        } else {
            ++ms.thing_selected;
        }
    }

    if (in.was_up(Action::MoveUp)) {
        Audio::play(au, Sound::MoveUI);
        if (ms.thing_selected == 0) {
            ms.thing_selected = items_count - 1;
        } else {
            --ms.thing_selected;
        }
    }

    if (in.was_up(Action::Jump)) {
        switch (ms.thing_selected) {
        case 0: { // Play
            // init game state
            app.current_level = 0;
            app_switch_to_level(app, app.current_level, true);
            do_level_sanity_checks(app.level_c);
            on_exit(GameState::Game);
        } break;
        case 1: { // Level Select
            on_exit(GameState::LevelSelect);
        } break;
        case 2: { // Info
            on_exit(GameState::InfoMenu);
        } break;
        case 3: { // Toggle Fullscreen
            rend::toggle_fullscreen(r);
        } break;
        case 4: { // Exit
            ctx.exit = true;
        } break;
        }

        Audio::play(au, Sound::Select);
        return;
    }

    if (in.was_up(Action::Back)) {
        ctx.exit = true;
        return;
    }

    rend::draw_text(r, GAME_NAME, 1.5f, v2(0.0f, r.client_height * 0.5f - 110.0f), true);

    u32 i = 0;
    for (auto const &label : menu_strs) {
        string the_str = string(label);
        if (i == ms.thing_selected) {
            the_str = format("> {} <", label);
        }
        rend::draw_text(r, the_str, 1.0f, v2(0.0f, r.client_height * 0.5f - 250.0f - (100.0f * (f32)i)), true);
        ++i;
    }
}

void make_cam_drunk(DrunkParams drunk_params, f32 total_time, Camera &camera) {

    f32 offset_x = drunk_params.amplitude_factor_x * cos(total_time * drunk_params.period_factor_x);
    f32 offset_y = drunk_params.amplitude_factor_y * sin(total_time * drunk_params.period_factor_y);
    // offset_x = 0.0;
    XMVECTOR s = XMVectorReplicate(offset_x);
    XMVECTOR l = XMLoadFloat3(&camera.right);
    XMVECTOR p = XMLoadFloat3(&camera.position);
    XMStoreFloat3(&camera.position, XMVectorMultiplyAdd(s, l, p));

    s = XMVectorReplicate(offset_y);
    l = XMLoadFloat3(&camera.up);
    p = XMLoadFloat3(&camera.position);
    XMStoreFloat3(&camera.position, XMVectorMultiplyAdd(s, l, p));
    camera.update_view_matrix();
}

void do_preview(App &app) {
    MirrorPreviewData ev = {};
    bool has_preview = game_get_mirror_preview(app.level_c, ev);

    if (!has_preview) {
        return;
    }

    // create a ghost on the ev.to
    if (!ev.te.has_entity)
        return;

    Entity *e = app.es.get(ev.te.entity_id);
    if (!e) {
        return;
    }

    Entity ghost_e = *e;
    ghost_e.box.pos = coord_to_v3(ev.to);
    ghost_e.box.pos.y += get_elevation(ghost_e.box);
    ghost_e.box.color = v4(1.f, 0.f, 0.f, 0.3f);
    auto clone_key = app.es.add(ghost_e);
    app.preview_keys.push_back(clone_key);
}

Direction rotate_dir(Direction dir, bool right) {
    if (right) {
        switch (dir) {
        case Direction::Left:
            return Direction::Up;
        case Direction::Right:
            return Direction::Down;
        case Direction::Up:
            return Direction::Right;
        case Direction::Down:
            return Direction::Left;
        case Direction::JumpAction:
            return Direction::JumpAction;
            break;
        }
    } else {
        switch (dir) {
        case Direction::Left:
            return Direction::Down;
        case Direction::Right:
            return Direction::Up;
        case Direction::Up:
            return Direction::Left;
        case Direction::Down:
            return Direction::Right;
        case Direction::JumpAction:
            return Direction::JumpAction;
            break;
        }
    }

    return dir;
}

Direction get_actual_direction(f32 cam_angle, Direction dir) {
    f32 ang = math::make_angle(cam_angle);

    const f32 window = math::Tau * 0.20f;

    // rotating cam left
    if (abs(ang - math::Tau * 0.25f) <= window) {
        return rotate_dir(dir, false);
    }

    // rotating cam twice
    if (abs(ang - math::Tau * 0.50f) <= window) {
        return rotate_dir(rotate_dir(dir, true), true);
    }

    // rotating cam right
    if (abs(ang - math::Tau * 0.75f) <= window) {
        return rotate_dir(dir, true);
    }

    // not rotating
    if (abs(ang - math::Tau * 0.0f) <= window) {
        return dir;
    }

    return dir;
}

void draw_end_credits_scene(App &app, Renderer &r) {

    array info_strs = {"You completed Psycho Box. Congrats! Thank you for playing <3"sv,
                       " "sv,
                       "Programming by lucypero"sv,
                       "Music and Sound Effects by AttentiveColon"sv,
                       " "sv,
                       "Press >ESC< to go back to the main menu."sv};

    for (u32 i = 0; auto const &info_str : info_strs) {
        rend::draw_text(r, info_str, 0.7f, v2(0.0f, r.client_height * 0.5f - 200.0f - (40.0f * (f32)i)), true);
        ++i;
    }
}

void gamestate_game_tick(App &app, Ctx &ctx, f32 dt_sec) {
    auto &r = *ctx.renderer;

#ifdef _DEBUG
    show_imgui(app, r, dt_sec);
#endif

    Input &in = *ctx.input;
    auto &au = *ctx.audio;

    rend::set_camera_target_pos(r, v3(app.cam_controls.camera_center[0], app.cam_controls.camera_center[1],
                                      app.cam_controls.camera_center[2]));

    vec<GameEvent> eks = {};
    eks.reserve(10);

    if (app.player_has_control) {

        Direction dir = Direction::Down;

        bool moved = true;

        if (in.was_up(Action::MoveLeft)) {
            dir = get_actual_direction(app.current_angle, Direction::Left);
        } else if (in.was_up(Action::MoveRight)) {
            dir = get_actual_direction(app.current_angle, Direction::Right);
        } else if (in.was_up(Action::MoveDown)) {
            dir = get_actual_direction(app.current_angle, Direction::Up);
        } else if (in.was_up(Action::MoveUp)) {
            dir = get_actual_direction(app.current_angle, Direction::Down);
        } else if (in.was_up(Action::Jump)) {
            dir = Direction::JumpAction;
        } else {
            moved = false;
        }

        const auto add_rot_anim = [&]() {
            Animated a = {};
            a.the_thing = AnimatedThing::Float;
            a.simple.the_float = &app.cam_controls.cam_y_angle;
            a.simple.prev = app.cam_controls.cam_y_angle;
            a.simple.target = app.current_angle;
            a.duration_s = cam_rot_duration;
            app.as.push_back(a);
        };

        if (in.was_up(Action::CameraLeft)) {
            app.current_angle += math::Tau * 0.25f;
            add_rot_anim();
        } else if (in.was_up(Action::CameraRight)) {
            app.current_angle -= math::Tau * 0.25f;
            add_rot_anim();
        }

        if (moved) {
            game_tick(dir, app.level_c, app.levels[app.current_level].level, app.level_moves, eks);
            do_level_sanity_checks(app.level_c);
            if (eks.size() > 0) { // things happened
                // clear previews
                for (const auto &k : app.preview_keys) {
                    app.es.remove(k);
                }

                app_update_boxes(app, eks);
                // log_events(eks);
                if (is_game_over(eks)) {
                    app.player_has_control = false;
                } else {
                    do_preview(app);
                }

                if (is_game_won(eks)) {
                    Audio::play(au, Sound::Win);
                }

                if (is_teleport(eks)) {
                    Audio::play(au, Sound::Teleport);
                } else if (is_move(eks)) {
                    Audio::play(au, Sound::Move);
                }
            }
        }
    }

    if (in.was_up(Action::Undo) && !app.completed_game) {
        game_do_undo(app.level_c, app.levels[app.current_level].level, app.level_moves);
        set_entities(app, app.level_c, false);
        do_preview(app);
    }

    if (in.was_up(Action::Reset) && !app.completed_game) {
        app_switch_to_level(app, app.current_level, true);
    }

    if (in.was_up(Action::Back)) {
        app.game_state = GameState::Menu;
        Audio::play(au, Sound::Back);
    }

#ifdef _DEBUG
    if (in.was_up(Key::G)) {
        app.draw_grid = !app.draw_grid;
    }
#endif

    feed_boxes_to_renderer(app, r);

    if (app.completed_game) {
        draw_end_credits_scene(app, r);
        app.player_has_control = false;
    } else {
        string the_string = app.levels[app.current_level].name;
        rend::draw_text(r, the_string, 1.0f, v2(0.0f, r.client_height * 0.5f - 100.0f), true);
    }

    rend::set_light(*ctx.renderer, app.light);
    rend::set_material(*ctx.renderer, app.mat);

    if (app.draw_grid) {
        rend::draw_grid(r);
    }
}

void gamestate_info_menu_tick(App &app, Ctx &ctx, f32 dt_sec) {
    Input &in = *ctx.input;
    Renderer &r = *ctx.renderer;
    auto &au = *ctx.audio;

    array info_strs = {"Credits:"sv,
                       "        Programming by lucypero"sv,
                       "        Music and Sound Effects by AttentiveColon"sv,
                       " "sv,
                       "Controls (controller controls in parenthesis):"sv,
                       "        WASD or arrow keys (D-pad) - Move"sv,
                       "        SPACE or ENTER (A) - Use mirror teleport / Accept"sv,
                       "        R (Y) - Reset level"sv,
                       "        Z or U (X) - Undo last move"sv,
                       "        V and B (LB and RB) - Change camera angle"sv,
                       "        ESC (B) - Go back"sv,
                       " "sv,
                       "Press >SPACE< to go back."sv};

    for (u32 i = 0; auto const &info_str : info_strs) {
        rend::draw_text(r, info_str, 0.7f,
                        v2(-r.client_width / 2.0f + 200.f, r.client_height * 0.5f - 60.0f - (40.0f * (f32)i)),
                        false);
        ++i;
    }

    if (in.was_up(Action::Jump) || in.was_up(Action::Back)) {
        app.game_state = GameState::Menu;
        Audio::play(au, Sound::Back);
    }
}

void gamestate_level_select_tick(App &app, Ctx &ctx, f32 dt_sec) {
    Input &in = *ctx.input;
    Renderer &r = *ctx.renderer;
    MenuState &ms = app.menu_state;
    auto &au = *ctx.audio;

    const auto on_exit = [&app, &ms](GameState next_state) {
        ms.thing_selected = 0;
        app.text_cam_pos.y = 0.f;
        app.game_state = next_state;
    };

    const u32 items_count = (u32)app.levels.size();

    if (in.was_up(Action::MoveDown)) {
        Audio::play(au, Sound::MoveUI);
        if (ms.thing_selected == items_count - 1) {
            ms.thing_selected = 0;
        } else {
            ++ms.thing_selected;
        }
    }

    if (in.was_up(Action::MoveUp)) {
        Audio::play(au, Sound::MoveUI);
        if (ms.thing_selected == 0) {
            ms.thing_selected = items_count - 1;
        } else {
            --ms.thing_selected;
        }
    }

    app.text_cam_pos.y = -1 * (f32)ms.thing_selected * 100.f;

    if (in.was_up(Action::Back)) {
        Audio::play(au, Sound::Back);

        on_exit(GameState::Menu);
        return;
    }

    if (in.was_up(Action::Jump)) {
        Audio::play(au, Sound::Select);
        app.current_level = ms.thing_selected;
        app_switch_to_level(app, app.current_level, true);

        on_exit(GameState::Game);
        return;
    }

    // drawing the text

    u32 i = 0;
    for (auto const &lvl : app.levels) {
        string the_str = lvl.name;
        if (i == ms.thing_selected) {
            the_str = format("> {} <", the_str);
        }
        rend::draw_text(r, the_str, 1.0f, v2(0.0f, r.client_height * 0.5f - 250.0f - (100.0f * (f32)i)), true);
        ++i;
    }
}

} // namespace

// --------------------------------- EXPORTED FUNCTIONS (START) ---------------------------------

void app_init(App &app, Ctx &ctx) {

    auto &r = *ctx.renderer;

#ifdef _DEBUG
    run_genvec_tests();
#endif

    Renderer &renderer = *ctx.renderer;

    app.camera = camera_new();
    app.text_camera = camera_new();
    app.text_camera.update_view_matrix();

    app.text_offset = v2(-20.0f, 0.0f);
    app.text_scale = 1.0f;

    // initing cam controls
    app.cam_controls.init();
    app.cam_controls.cam_y_angle = app.current_angle;
    app.cam_controls.cam_x_angle = 0.84f;

    app_on_resize(app, renderer);

    app.text_test = "hello";

    // setting material and light for renderer
    {

        // init light
        app.light.Ambient = v4(0.1f, 0.1f, 0.1f, 1.0f);
        app.light.Diffuse = v4(1.0f, 1.0f, 1.0f, 1.0f);
        app.light.Specular = v4(0.5f, 0.5f, 0.5, 10.0f);
        app.light.Direction = v3(0.25f, -0.5f, 0.85f);

        // init boxes
        app.mat.Ambient = v4(0.1f, 0.1f, 0.1f, 1.0f);
        app.mat.Diffuse = v4(1.0f, 1.0f, 1.0f, 1.0f);
        app.mat.Specular = v4(0.5f, 0.5f, 0.5, 10.0f);
        app.mat.Reflect = v4(1.0f, 1.0f, 1.0f, 1.0f);

        rend::set_material(r, app.mat);
        rend::set_light(r, app.light);
    }

    bool res = load_levels_from_file("assets/levels/1.lvl", app.levels);

    lassert(res);

    app.current_level = 0;

    map_input_actions(*ctx.input);
}

void app_tick(App &app, Ctx &ctx, f32 dt_sec) {
    switch (app.game_state) {
    case GameState::Menu: {
        gamestate_menu_tick(app, ctx, dt_sec);
    } break;
    case GameState::LevelSelect: {
        gamestate_level_select_tick(app, ctx, dt_sec);
    } break;
    case GameState::InfoMenu: {
        gamestate_info_menu_tick(app, ctx, dt_sec);
    } break;
    case GameState::Game: {
        gamestate_game_tick(app, ctx, dt_sec);
    } break;
    case GameState::End: {
        gamestate_game_tick(app, ctx, dt_sec);
    } break;
    }

    auto &r = *ctx.renderer;

    // Input &in = *ctx.input;

    // app.cam_controls.tick(app.camera, in, dt_sec);
    app.cam_controls.tick_math(app.camera);

    app.text_camera.position = app.text_cam_pos;

    make_cam_drunk(app.drunk_params, app.time, app.camera);

    DrunkParams text_drunk_params = app.drunk_params;
    text_drunk_params.amplitude_factor_x *= 100.0;
    text_drunk_params.amplitude_factor_y *= 100.0;
    make_cam_drunk(text_drunk_params, app.time, app.text_camera);

    if (app.should_update_cameras) {
        app_on_resize(app, r);
        app.should_update_cameras = false;
    }

    rend::set_camera(r, app.camera);
    rend::set_text_camera(r, app.text_camera);

    anims_tick(app.as, app.es, dt_sec);
    timers_tick(app.ts, dt_sec, (void *)&app);

    app.time += dt_sec;
}

void app_on_resize(App &app, Renderer &renderer) {
    if (app.is_cam_ortho) {
        const f32 ratio = (f32)renderer.client_width / (f32)renderer.client_height;

        app.camera.set_lens_ortho(app.cam_ortho_zoom * app.ortho_factor * ratio,
                                app.cam_ortho_zoom * app.ortho_factor, 1.0f, 1000.0f);
    } else {
        app.camera.set_lens(app.cam_fovy, (f32)renderer.client_width / (f32)renderer.client_height, 1.0f, 1000.0f);
    }

    app.text_camera.set_lens_ortho((f32)renderer.client_width, (f32)renderer.client_height, 1.0f, 1000.0f);
}

// clears all entities and respawns them for the new level
void app_switch_to_level(App &app, i32 level_number, bool do_transition_anim) {
    app.current_level = level_number;
    Level &level = app.levels[level_number].level;
    set_entities(app, level, do_transition_anim);

    app.level_moves.clear();

    app.completed_game = false;
    app.level_c = level;

    // positioning camera
    {
        // setting camera position, centering it on level
        f32 x = ((level.width - 1) * unit_length) / 2.0f;
        f32 z = -((level.height - 1) * unit_length) / 2.0f;

        // centering camera to level center
        app.cam_controls.camera_center[0] = x;
        app.cam_controls.camera_center[2] = z;

        i32 l_max = math::Max(level.width, level.height);

        app.cam_controls.cam_distance = l_max + 8.0f;

        app.cam_ortho_zoom = l_max * 0.001f;
        app.should_update_cameras = true;
    }
}