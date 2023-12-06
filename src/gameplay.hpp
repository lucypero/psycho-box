#pragma once

#include "lucytypes.hpp"
#include "gen_vec.hpp"

enum struct Tile : char {
    Player = 'P',
    Wall = 'W',
    Empty = '.',
    Box = 'B',
    Goal = 'G',
    Floor = 'F',
    MirrorUL = '<',
    MirrorUR = '>',
    MirrorDL = '[',
    MirrorDR = ']'
};

struct TileEntity {
    u32 id; // turns out id is handy after all. 0 is reserved for empties.
    Tile tile = Tile::Empty;
    bool has_entity;
    GenKey entity_id; // entity associated w the tile entity
};

struct Coord {
    i32 x;
    i32 y;
};

inline constexpr u32 CELL_SLOT_COUNT = 4;

inline constexpr u32 PLANE_MAX_COUNT = 1;
inline constexpr u32 PLANE_MAX_WIDTH = 30;
inline constexpr u32 PLANE_MAX_HEIGHT = 30;

using LevelCell = array<TileEntity, CELL_SLOT_COUNT>;
using LevelPlane = array<array<LevelCell, PLANE_MAX_HEIGHT>, PLANE_MAX_WIDTH>;
using LevelData = array<LevelPlane, PLANE_MAX_COUNT>;

// level data + info on size of things and possibly other things
struct Level {
    LevelData data;
    u32 plane_count;
    u32 width;
    u32 height;
};

struct LevelNamed {
    string name;
    Level level;
};

enum struct EventKind {
    NormalMove,
    BoxFall,
    PlayerFall,
    Won,
    MirrorTeleport,
};

struct GameEvent {
    EventKind kind;
    TileEntity te;
    Coord from;
    Coord to;
    Coord mirror_coord;
};

struct MirrorPreviewData {
    TileEntity te;
    Coord from;
    Coord to;
    Coord mirror_coord;
};

enum struct Direction { Left, Right, Up, Down, JumpAction };

void game_tick(Direction dir, Level &level, const Level &level_start, vec<Direction> &moves, vec<GameEvent> &eks);
void game_do_undo(Level &level, const Level &level_start, vec<Direction> &moves);
void game_do_reset(Level &level, const Level &level_start, vec<Direction> &moves);
TileEntity &cell_place(LevelCell &cell, TileEntity te);
bool cell_is_empty(const LevelCell &cell);
bool is_game_over(span<GameEvent> events);
bool is_game_won(span<GameEvent> events);
bool is_move(span<GameEvent> events);
bool is_teleport(span<GameEvent> events);
void do_level_sanity_checks(const Level &level);
bool is_tile_moveable(Tile tile);
bool game_get_mirror_preview(const Level &level, MirrorPreviewData &out_preview);
Coord get_goal_coord(const Level &level);