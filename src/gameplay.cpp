#include "gameplay.hpp"

#include "utils.hpp"

namespace {

u32 get_tile_priority(Tile t) {
    switch (t) {
    case Tile::Empty:
        return 0;
    case Tile::Floor:
        return 1;
    case Tile::Wall:
    case Tile::Player:
    case Tile::Box:
    case Tile::MirrorUL:
    case Tile::MirrorUR:
    case Tile::MirrorDL:
    case Tile::MirrorDR:
    case Tile::Goal:
        return 2;
    }

    return 0;
};

void print_plane(const Level &level, i32 plane) {

    auto const &p = level.data[plane];

    // gotta determine which one to show in the string.
    const auto cell_to_char = [](const LevelCell &lc) -> char {
        u32 max_priority = 0;
        Tile max_tile = Tile::Empty;

        for (const auto &te : lc) {
            auto prio = get_tile_priority(te.tile);
            if (prio > max_priority) {
                max_priority = prio;
                max_tile = te.tile;
            }
        }

        return (char)max_tile;
    };

    vec<char> char_map((level.height) * (level.width + 1));

    for (u32 col_i = 0; const auto &row : p) {
        if (col_i >= level.width)
            break;
        for (u32 row_i = 0; const auto &cell : row) {
            if (row_i >= level.height)
                break;
            char_map[((level.width + 1) * row_i) + col_i] = cell_to_char(cell);
            ++row_i;
        }
        ++col_i;
    }

    for (u32 i = 1; auto &c : char_map) {
        if ((i % (level.width + 1)) == 0) {
            c = '\n';
        }
        ++i;
    }

    {
        string level_str;
        level_str.reserve(char_map.size());

        for (char c : char_map) {
            level_str += c;
        }

        log("==== level state, plane %i ====", plane);
        log("%s", level_str.c_str());
    }
}

Coord get_player_coord(const Level &level) {
    for (u32 plane_i = 0; const auto &p : level.data) {
        if (plane_i >= level.plane_count)
            break;
        for (i32 col_i = 0; const auto &column : p) {
            for (i32 row_i = 0; const auto &cell : column) {
                for (const auto &te : cell) {
                    if (te.tile == Tile::Player) {
                        return Coord{col_i, row_i};
                    }
                }
                ++row_i;
            }
            ++col_i;
        }
        ++plane_i;
    }
    return Coord{};
}

bool level_query(const Level &level, Coord coord, LevelCell &c) {
    if (coord.x < 0 || coord.y < 0 || coord.x >= PLANE_MAX_WIDTH || coord.y >= PLANE_MAX_HEIGHT) {
        return false;
    }

    c = level.data[0][coord.x][coord.y];
    return true;
}

LevelCell &coord_get(Level &level, Coord c) {
    LevelCell &lc = level.data[0][c.x][c.y];
    return lc;
}

enum MoveDirection { Up, Right, Down, Left };

void get_blockages(MoveDirection dir, vec<Tile> &out_blockages) {
    out_blockages.clear();

    out_blockages.push_back(Tile::Box);
    out_blockages.push_back(Tile::Wall);

    switch (dir) {
    case MoveDirection::Up:
        out_blockages.push_back(Tile::MirrorUL);
        out_blockages.push_back(Tile::MirrorUR);
        break;
    case MoveDirection::Right:
        out_blockages.push_back(Tile::MirrorUR);
        out_blockages.push_back(Tile::MirrorDR);
        break;
    case MoveDirection::Down:
        out_blockages.push_back(Tile::MirrorDL);
        out_blockages.push_back(Tile::MirrorDR);
        break;
    case MoveDirection::Left:
        out_blockages.push_back(Tile::MirrorUL);
        out_blockages.push_back(Tile::MirrorDL);
        break;
    }
}

Coord coord_add(Coord c, MoveDirection dir) {
    Coord res = c;

    switch (dir) {
    case MoveDirection::Up:
        --res.y;
        break;
    case MoveDirection::Right:
        ++res.x;
        break;
    case MoveDirection::Down:
        ++res.y;
        break;
    case MoveDirection::Left:
        --res.x;
        break;
    }

    return res;
}

LevelCell coord_get_copy(const Level &level, Coord c) {
    return level.data[0][c.x][c.y];
}

bool coord_get_entity(const Level &level, Coord c, Tile tile, TileEntity &e) {

    const auto cell = coord_get_copy(level, c);

    for (const auto &te : cell) {
        if (te.tile == tile) {
            e = te;
            return true;
        }
    }

    return false;
}

// Checks if there's a tile of any of the `tiles` types. returns the first one that it finds.
bool coord_has_tiles(const Level &level, Coord c, span<Tile> tiles, TileEntity &e) {

    const auto cell = coord_get_copy(level, c);

    for (const auto &te : cell) {
        for (const auto &tile : tiles) {
            if (te.tile == tile) {
                e = te;
                return true;
            }
        }
    }

    return false;
}

bool tile_is_mirror(Tile tile) {
    return (tile == Tile::MirrorUL || tile == Tile::MirrorUR || tile == Tile::MirrorDL || tile == Tile::MirrorDR);
}

// tile util functions
bool tile_is_solid(Tile tile) {
    return (tile == Tile::Player || tile == Tile::Box || tile_is_mirror(tile) || tile == Tile::Wall);
}

// cell util functions

bool cell_is_there(const LevelCell &cell, Tile tile) {
    for (const auto &te : cell) {
        if (te.tile == tile) {
            return true;
        }
    }

    return false;
}

bool cell_is_free_with_floor(const LevelCell &cell) {

    bool has_floor = false;
    bool has_solid = false;

    for (const auto &te : cell) {
        if (te.tile == Tile::Floor) {
            has_floor = true;
        }

        if (tile_is_solid(te.tile)) {
            has_solid = true;
        }
    }

    return has_floor && !has_solid;
}

// cell utils end

void te_set_empty(TileEntity &te) {
    te.id = 0;
    te.tile = Tile::Empty;
    te.has_entity = false;
}

void cell_get_entity(const LevelCell &cell, Tile tile, TileEntity &te) {

    for (const auto &te_i : cell) {
        if (te_i.tile == tile) {
            te = te_i;
            return;
        }
    }

    lassert(false)
}

void level_find_player(const Level &level, Coord &out_c, TileEntity &out_te) {
    out_c = get_player_coord(level);
    cell_get_entity(coord_get_copy(level, out_c), Tile::Player, out_te);
}

TileEntity &level_move_te(Level &level, TileEntity te, Coord c_from, Coord c_to) {

    // setting c_from slot to empty
    auto &cell_from = coord_get(level, c_from);

    bool found = false;

    for (auto &te_i : cell_from) {
        if (te_i.id == te.id) {
            te_set_empty(te_i);
            found = true;
            break;
        }
    }

    lassert(found);

    // setting an empty c_to slot to te
    auto &cell_to = coord_get(level, c_to);
    TileEntity &te_ret = cell_place(cell_to, te);

    return te_ret;
}

bool coord_is_valid(const Level &level, Coord c) {
    return (c.x >= 0 && c.y >= 0 && c.x < (i32)level.width && c.y < (i32)level.height);
}

bool cell_has_moveable(const LevelCell &cell) {
    for (const auto &te : cell) {
        if (is_tile_moveable(te.tile)) {
            return true;
        }
    }

    return false;
}

void cell_get_moveable(const LevelCell &cell, TileEntity &out_te) {

    for (const auto &te : cell) {
        if (is_tile_moveable(te.tile)) {
            out_te = te;
            return;
        }
    }

    lassert(false);
}

void coord_get_moveable(const Level &level, Coord c, TileEntity &e) {
    const auto cell = coord_get_copy(level, c);

    for (const auto &te : cell) {
        if (is_tile_moveable(te.tile)) {
            e = te;
            return;
        }
    }

    lassert(false);
}

struct StepResult {
    Coord next_coord;   // relevant only when !is_done
    TileEntity next_te; // relevant only when !is_done
    bool is_done;
    bool is_valid;
};

void get_compat_teleps(MoveDirection dir, array<Tile, 2> &out_teleps) {
    switch (dir) {
    case MoveDirection::Up:
        out_teleps[0] = Tile::MirrorDL;
        out_teleps[1] = Tile::MirrorDR;
        break;
    case MoveDirection::Right:
        out_teleps[0] = Tile::MirrorUL;
        out_teleps[1] = Tile::MirrorDL;
        break;
    case MoveDirection::Down:
        out_teleps[0] = Tile::MirrorUL;
        out_teleps[1] = Tile::MirrorUR;
        break;
    case MoveDirection::Left:
        out_teleps[0] = Tile::MirrorUR;
        out_teleps[1] = Tile::MirrorDR;
        break;
    }
}

MoveDirection get_direction_bounce(MoveDirection in_dir, Tile telep) {
    if (in_dir == MoveDirection::Up || in_dir == MoveDirection::Down) {
        if (telep == Tile::MirrorUL || telep == Tile::MirrorDL)
            return MoveDirection::Left;
        else
            return MoveDirection::Right;
    } else {
        if (telep == Tile::MirrorUL || telep == Tile::MirrorUR)
            return MoveDirection::Up;
        else
            return MoveDirection::Down;
    }
}

// checks if it can teleport.
// if true, it will return the teleport Coord and stuff on the out args
bool can_teleport(const Level &level, Coord c, TileEntity &out_teleporter, Coord &out_telep_coord,
                  Coord &out_coord) {

    Coord telep_c = c;

    /// Finding the teleporter.

    bool telep_found = false;
    u32 bounce_distance = 0;
    TileEntity telep;

    array<MoveDirection, 4> directions = {MoveDirection::Up, MoveDirection::Right, MoveDirection::Down,
                                          MoveDirection::Left};

    MoveDirection direction_bounce = MoveDirection::Left;

    vec<Tile> blockages = {};
    blockages.reserve(4);

    for (u32 i = 0; const auto dir : directions) {
        while (true) {
            ++bounce_distance;
            telep_c = coord_add(telep_c, dir);

            array<Tile, 2> compat_teleps;
            get_compat_teleps(dir, compat_teleps);

            if (!coord_is_valid(level, telep_c)) {
                break;
            }

            get_blockages(dir, blockages);

            TileEntity temp;
            if (coord_has_tiles(level, telep_c, blockages, temp)) {
                break;
            }

            if (coord_has_tiles(level, telep_c, compat_teleps, telep)) {
                telep_found = true;
                direction_bounce = dir;
                goto done_searching;
            }
        }

        telep_c = c;
        bounce_distance = 0;

        ++i;
    }

done_searching:

    if (!telep_found)
        return false;

    //// Handling bounce

    // Finding the bounce direction.
    direction_bounce = get_direction_bounce(direction_bounce, telep.tile);

    // Check if there are blockages along the way.

    Coord destination = telep_c;

    bool keep_iterating = false;

    while (true) {
        for (u32 i = 0; i < bounce_distance; ++i) {

            destination = coord_add(destination, direction_bounce);

            if (!coord_is_valid(level, destination)) {
                return false;
            }

            get_blockages(direction_bounce, blockages);

            TileEntity temp;
            if (coord_has_tiles(level, destination, blockages, temp)) {
                return false;
            }

            // see if there is a telep that can take in the light.
            array<Tile, 2> compat_teleps;
            get_compat_teleps(direction_bounce, compat_teleps);

            if (coord_has_tiles(level, destination, compat_teleps, telep)) {
                telep_c = destination;
                direction_bounce = get_direction_bounce(direction_bounce, telep.tile);
                keep_iterating = true;
                break;
            }
        }

        if (!keep_iterating) {
            break;
        } else {
            keep_iterating = false;
        }
    }

    out_teleporter = telep;
    out_telep_coord = telep_c;
    out_coord = destination;

    return true;
}

bool try_mirror_teleport(Level &level, TileEntity te, Coord c, vec<GameEvent> &events) {

    TileEntity telep;
    Coord telep_coord;
    Coord destination;

    if (!can_teleport(level, c, telep, telep_coord, destination))
        return false;

    // moving player
    LevelCell cell_ahead;
    level_query(level, destination, cell_ahead);

    GameEvent ev = {};
    ev.kind = EventKind::MirrorTeleport;
    ev.from = c;
    ev.to = destination;
    ev.te = te;
    events.push_back(ev);

    if (cell_is_empty(cell_ahead)) {
        // fall player
        ev = {};
        ev.kind = EventKind::PlayerFall;
        ev.from = c;
        ev.to = destination;
        ev.te = te;
        events.push_back(ev);
    }

    // move player
    level_move_te(level, te, c, destination);

    return true;
}

// step logic here. it moves the thing to the next direction if is a possible valid move
// if there's a (moveable) there, out_keep_stepping is set to true
// out_is_valid_move is only meaningful when out_keep_stepping is false.
StepResult try_move_step(Level &level, TileEntity te, Coord c, Direction dir, vec<GameEvent> &events) {

    // this is in the step

    Coord coord_ahead = c;

    StepResult res = {};
    res.is_done = true;

    // this one is special
    if (dir == Direction::JumpAction) {
        bool did_jump = try_mirror_teleport(level, te, c, events);

        res.is_done = true;
        res.is_valid = did_jump;

        return res;
    }

    // get the coord ahead
    switch (dir) {
    case Direction::Left:
        --coord_ahead.x;
        break;
    case Direction::Right:
        ++coord_ahead.x;
        break;
    case Direction::Up:
        ++coord_ahead.y;
        break;
    case Direction::Down:
        --coord_ahead.y;
        break;
    }

    if (!coord_is_valid(level, coord_ahead)) {
        res.is_done = true;
        res.is_valid = false;
        return res;
    }

    LevelCell cell_ahead;
    level_query(level, coord_ahead, cell_ahead);

    if (cell_is_empty(cell_ahead)) {

        // if is a box, make it a bridge
        if (te.tile == Tile::Box) {
            GameEvent ev = {};
            ev.kind = EventKind::BoxFall;
            ev.from = c;
            ev.to = coord_ahead;
            ev.te = te;
            events.push_back(ev);

            TileEntity &moved = level_move_te(level, te, c, coord_ahead);
            moved.tile = Tile::Floor;
            res.is_done = true;
            res.is_valid = true;
            return res;
        }

        // if is player, fall
        if (te.tile == Tile::Player) {
            GameEvent ev = {};
            ev.kind = EventKind::NormalMove;
            ev.from = c;
            ev.to = coord_ahead;
            ev.te = te;
            events.push_back(ev);
            ev.kind = EventKind::PlayerFall;
            events.push_back(ev);

            level_move_te(level, te, c, coord_ahead);

            res.is_done = true;
            res.is_valid = true;
            return res;
        }

        // if not, it is not a valid move (do not move)
        res.is_done = true;
        res.is_valid = false;
        return res;
    } else if (cell_is_free_with_floor(cell_ahead)) {
        GameEvent ev = {};
        ev.kind = EventKind::NormalMove;
        ev.from = c;
        ev.to = coord_ahead;
        ev.te = te;
        events.push_back(ev);
        level_move_te(level, te, c, coord_ahead);

        res.is_done = true;
        res.is_valid = true;
        return res;
    } else if (cell_has_moveable(cell_ahead)) {

        TileEntity next_te;
        cell_get_moveable(cell_ahead, next_te);

        // moveable encountered. move te and run this function again.
        GameEvent ev = {};
        ev.kind = EventKind::NormalMove;
        ev.from = c;
        ev.to = coord_ahead;
        ev.te = te;
        events.push_back(ev);
        level_move_te(level, te, c, coord_ahead);

        res.next_coord = coord_ahead;
        res.next_te = next_te;
        res.is_done = false;
        return res;
    } else {

        // no idea what happens here. maybe it should never arrive here.

        res.is_done = true;
        res.is_valid = false;
        return res;
    }
}

void level_play_move(const Level &level, Direction dir, Level &new_level, vec<GameEvent> &events) {

    Coord p_c = get_player_coord(level);
    TileEntity player;
    assert(coord_get_entity(level, p_c, Tile::Player, player));

    // allocate level copy and events copy so they can be easily reversed
    auto lvl_copy = std::make_unique<Level>(level);
    vec<GameEvent> new_events = {};

    StepResult s_res = {};
    s_res.is_done = false;
    s_res.next_coord = p_c;
    s_res.next_te = player;

    while (!s_res.is_done) {
        s_res = try_move_step(*lvl_copy, s_res.next_te, s_res.next_coord, dir, new_events);
    }

    if (s_res.is_valid) {
        new_level = *lvl_copy;

        // check if it can mirror now
        p_c = get_player_coord(new_level);

        bool it_fell = false;
        for (const auto &ev : new_events) {
            if (ev.kind == EventKind::PlayerFall) {
                it_fell = true;
                break;
            }
        }

        events.insert(events.end(), new_events.begin(), new_events.end());
    }
}

Level level_play_moves(const Level &level, span<Direction> moves) {
    Level lvl = level;

    vec<GameEvent> events(5);

    for (auto move : moves) {
        Level new_lvl;
        level_play_move(lvl, move, new_lvl, events);
        lvl = new_lvl;
    }

    return lvl;
}

} // namespace

// --------------------------------- EXPORTED FUNCTIONS (START) ---------------------------------

void game_do_reset(Level &level, const Level &level_start, vec<Direction> &moves) {
    level = level_start;
    moves.clear();
    return;
}

void game_do_undo(Level &level, const Level &level_start, vec<Direction> &moves) {
    if (moves.size() <= 0) {
        return;
    }

    moves.pop_back();
    level = level_play_moves(level_start, moves);
    return;
}

void game_tick(Direction dir, Level &level, const Level &level_start, vec<Direction> &moves, vec<GameEvent> &eks) {

    // tile of where the player wants to go
    Level level_next;
    level_play_move(level, dir, level_next, eks);

    if (eks.size() == 0) {
        return;
    }

    // print_plane(level_next, 0);

    level = level_next;

    // add the move
    moves.push_back(dir);

    // check for win state
    bool is_win = false;

    for (u32 plane_i = 0; const auto &p : level.data) {
        if (plane_i >= level.plane_count)
            break;
        for (const auto &column : p) {
            for (const auto &cell : column) {
                if (cell_is_there(cell, Tile::Goal) && (cell_is_there(cell, Tile::Player))) {
                    is_win = true;
                    goto outer;
                }
            }
        }
        ++plane_i;
    }
outer:

    if (is_win) {
        GameEvent ev = {};
        ev.kind = EventKind::Won;
        eks.push_back(ev);
    }

    return;
}

// replaces an empty in the cell with te
TileEntity &cell_place(LevelCell &cell, TileEntity te) {

    for (auto &te_i : cell) {
        if (te_i.tile == Tile::Empty) {
            te_i = te;
            return te_i;
        }
    }

    lassert(false);

    return cell[0];
}

bool cell_is_empty(const LevelCell &cell) {
    for (const auto &te : cell) {
        if (te.tile != Tile::Empty) {
            return false;
        }
    }
    return true;
}

bool is_game_over(span<GameEvent> events) {
    bool is_over = false;

    for (const auto &ev : events) {
        switch (ev.kind) {
        case EventKind::PlayerFall:
        case EventKind::Won:
            is_over = true;
            break;
        }
    }

    return is_over;
}

bool is_game_won(span<GameEvent> events) {
    bool is_won = false;

    for (const auto &ev : events) {
        switch (ev.kind) {
        case EventKind::Won:
            is_won = true;
            break;
        }
    }

    return is_won;
}

bool is_move(span<GameEvent> events) {
    bool is_move = false;

    for (const auto &ev : events) {
        switch (ev.kind) {
        case EventKind::NormalMove:
        case EventKind::MirrorTeleport:
            is_move = true;
            break;
        }
    }

    return is_move;
}

bool is_teleport(span<GameEvent> events) {
    bool is_teleport = false;

    for (const auto &ev : events) {
        switch (ev.kind) {
        case EventKind::MirrorTeleport:
            is_teleport = true;
            break;
        }
    }

    return is_teleport;
}

// 1 - make sure there is only one player
// 2 - make sure all ids are unique except id 0 which should be an empty
// 3 - all id 0 should be empty
// 4 - all empty should be id 0
void do_level_sanity_checks(const Level &level) {

    u32 players = 0;

    for (const auto &row : level.data[0]) {
        for (const auto &cell : row) {
            for (const auto &te : cell) {
                if (te.tile == Tile::Player) {
                    ++players;
                }
            }
        }
    }

    lassert(players == 1);

    // TODO(lucy): more checks
}

bool is_tile_moveable(Tile tile) {
    switch (tile) {
    case Tile::Player:
    case Tile::Box:
    case Tile::MirrorUL:
    case Tile::MirrorUR:
    case Tile::MirrorDL:
    case Tile::MirrorDR:
        return true;
        break;
    }

    return false;
}

bool game_get_mirror_preview(const Level &level, MirrorPreviewData &out_preview) {
    TileEntity telep;
    Coord telep_coord;
    Coord destination;
    Coord p_c = get_player_coord(level);
    TileEntity player;
    assert(coord_get_entity(level, p_c, Tile::Player, player));

    if (can_teleport(level, p_c, telep, telep_coord, destination)) {
        out_preview.mirror_coord = telep_coord;
        out_preview.from = p_c;
        out_preview.to = destination;
        out_preview.te = player;
        return true;
    }

    return false;
}

Coord get_goal_coord(const Level &level) {
    for (u32 plane_i = 0; const auto &p : level.data) {
        if (plane_i >= level.plane_count)
            break;
        for (i32 col_i = 0; const auto &column : p) {
            for (i32 row_i = 0; const auto &cell : column) {
                bool has_player = false;
                bool has_goal = false;
                for (const auto &te : cell) {
                    if (te.tile == Tile::Goal) {
                        has_goal = true;
                    }
                    if (te.tile == Tile::Player) {
                        has_player = true;
                    }
                }
                if (has_player && has_goal) {
                    return Coord{col_i, row_i};
                }
                ++row_i;
            }
            ++col_i;
        }
        ++plane_i;
    }
    return Coord{};
}