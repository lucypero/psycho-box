#include "level_parser.hpp"

#include <unordered_map>

#include "utils.hpp"

using parse::Parser;
using parse::ParseResult;

namespace {

const constexpr string_view level_separator = "---\n";
const constexpr string_view plane_separator = "--\n";
const constexpr string_view metadata_start = "###\n";

using Metadata = std::unordered_map<string, string>;

bool is_plane_tile(char c) {
    return (c != '-' && c != '#' && c != '\n' && c != '\0');
}

// places level_end on the index 1 char after the last newline
ParseResult advance_until_plane_end(Parser p) {
    ParseResult res = {};

    // process line by line
    char c = (*p.b)[p.index];

    while (is_plane_tile(c)) {
        ParseResult res_i = parse::advance_until(p, "\n");
        if (!res_i.succeeded) {
            res.succeeded = false;
            return res;
        }

        p.index = res_i.i_next + 1;
        res.i_next = p.index;

        c = (*p.b)[p.index];
    }

    res.succeeded = true;
    return res;
}

// sets level width, height, and it fills the level plane with the right entities
// sets res.i_next to 1 char after the newline at the end of the plane
ParseResult parse_plane(Parser p, u32 &last_id, i32 &plane_w, i32 &plane_h, LevelPlane &lp) {

    i32 c_width = 0;
    i32 max_width = 0;
    i32 height = 0;

    ParseResult res = {};

    // determining plane end: start of line that is a - or a # or a \n

    ParseResult res_i = advance_until_plane_end(p);

    if (!res_i.succeeded) {
        res.succeeded = false;
        return res;
    }

    res.i_next = res_i.i_next;
    u32 level_end = res_i.i_next;

    // determining max width
    for (u32 i = p.index; i < level_end; ++i) {
        switch ((*p.b)[i]) {
        case '\n': {
            max_width = math::Max(max_width, c_width);
            c_width = 0;
            ++height;
        } break;
        default: {
            ++c_width;
        } break;
        }
    }

    const u32 CELL_PADDING = 3;

    // adding empty padding
    plane_w = max_width + (CELL_PADDING * 2);
    plane_h = height + (CELL_PADDING * 2);

    // populating the level cells
    {
        c_width = 0;

        // clearing plane
        lp = {};

        const auto place_te = [](LevelCell &cell, u32 id, Tile tile) {
            TileEntity te = {};
            te.id = id;
            te.tile = tile;
            cell_place(cell, te);
        };

        u32 y = 0;

        for (u32 i = p.index; i < level_end; ++i) {
            auto &cell = lp[c_width + CELL_PADDING][y + CELL_PADDING];

            switch ((*p.b)[i]) {
            case (char)Tile::Empty:
                ++c_width;
                break;
            case (char)Tile::Floor:
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case (char)Tile::Wall:
                place_te(cell, last_id++, Tile::Wall);
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case (char)Tile::Player:
                place_te(cell, last_id++, Tile::Player);
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case (char)Tile::Box:
                place_te(cell, last_id++, Tile::Box);
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case (char)Tile::Goal:
                place_te(cell, last_id++, Tile::Goal);
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case (char)Tile::MirrorUL:
                place_te(cell, last_id++, Tile::MirrorUL);
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case (char)Tile::MirrorUR:
                place_te(cell, last_id++, Tile::MirrorUR);
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case (char)Tile::MirrorDL:
                place_te(cell, last_id++, Tile::MirrorDL);
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case (char)Tile::MirrorDR:
                place_te(cell, last_id++, Tile::MirrorDR);
                place_te(cell, last_id++, Tile::Floor);
                ++c_width;
                break;
            case '\n':
                c_width = 0;
                ++y;
                break;
            default: {
                res.succeeded = false;
                return res;
            }
            }
        }
    }

    res.succeeded = true;
    return res;
}

ParseResult parse_key_value(Parser p, string &m_key, string &m_value) {

    ParseResult res = {};

    ParseResult res_i = parse::advance_until(p, "=");

    if (!res_i.succeeded) {
        res.succeeded = false;
        return res;
    }

    m_key = (*p.b).substr(p.index, res_i.i_next - p.index);

    p.index = res_i.i_next + 1;

    res_i = parse::advance_until(p, "\n");

    if (!res_i.succeeded) {
        res.succeeded = false;
        return res;
    }

    m_value = (*p.b).substr(p.index, res_i.i_next - p.index);

    res.i_next = res_i.i_next + 1;
    res.succeeded = true;
    return res;
}

ParseResult parse_level_metadata(Parser p, Metadata &metadata) {

    ParseResult r = {};

    r.i_next = p.index;

    while (true) {
        string m_key;
        string m_value;

        char c = (*p.b)[p.index];

        // if line doesn't start w a letter, return
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
            r.succeeded = true;
            return r;
        }

        ParseResult res_i = parse_key_value(p, m_key, m_value);

        if (!res_i.succeeded) {
            r.succeeded = false;
            return r;
        }

        metadata[m_key] = m_value;

        p.index = res_i.i_next;
        r.i_next = p.index;
    }

    r.succeeded = true;
    return r;
}

} // namespace

// --------------------------------- EXPORTED FUNCTIONS (START) ---------------------------------

bool load_levels_from_file(string_view filename, vec<LevelNamed> &levels) {

    vec<u8> file = load_file(filename, true);
    string file_s(file.begin(), file.end());

    Parser p = {};
    p.index = 0;
    p.b = &file_s;

    LevelNamed level_named = {};
    u32 plane_i = 0;
    u32 te_id_i = 1;

    enum struct ParseWhat { FindLevelName, LevelName, LevelPlane, Metadata };

    ParseWhat parse_mode = ParseWhat::FindLevelName;
    bool parsing_stopped = false;

    ParseResult res_i = {};
    bool is_parsing_level = false;

    while (p.index < file_s.size() && !parsing_stopped) {
        switch (parse_mode) {
        case ParseWhat::FindLevelName: {

            if (is_parsing_level) {
                levels.push_back(level_named);
                plane_i = 0;
                te_id_i = 1;
                is_parsing_level = false;
            }

            res_i = parse::is_next(p, level_separator);

            if (!res_i.succeeded) {
                parsing_stopped = true;
                break;
            }

            p.index = res_i.i_next;
            parse_mode = ParseWhat::LevelName;
        } break;
        case ParseWhat::LevelName: {
            res_i = parse::advance_until(p, "\n");

            if (!res_i.succeeded) {
                return false;
            }

            // level name found
            level_named.name = file_s.substr(p.index, res_i.i_next - p.index);

            p.index = res_i.i_next;

            res_i = parse::advance_until(p, plane_separator);

            if (!res_i.succeeded) {
                return false;
            }

            is_parsing_level = true;

            p.index = res_i.i_next + (i32)plane_separator.size();

            parse_mode = ParseWhat::LevelPlane;
        } break;
        case ParseWhat::LevelPlane: {

            i32 p_w, p_h;
            LevelPlane lp;

            res_i = parse_plane(p, te_id_i, p_w, p_h, lp);

            if (!res_i.succeeded) {
                return false;
            }

            p.index = res_i.i_next;

            level_named.level.width = p_w;
            level_named.level.height = p_h;

            level_named.level.data[plane_i++] = lp;
            level_named.level.plane_count = plane_i;

            res_i = parse::is_next(p, plane_separator);

            if (res_i.succeeded) {
                parse_mode = ParseWhat::LevelPlane;
                p.index = res_i.i_next;
            } else {
                parse_mode = ParseWhat::Metadata;
            }
        } break;
        case ParseWhat::Metadata: {
            res_i = parse::is_next(p, metadata_start);

            if (!res_i.succeeded) {
                parse_mode = ParseWhat::FindLevelName;
                continue;
            }

            p.index = res_i.i_next + (i32)metadata_start.size();

            Metadata metadata;
            res_i = parse_level_metadata(p, metadata);

            if (!res_i.succeeded) {
                return false;
            }

            // print metadata
            // log("printing metadata for level %s", level.name.c_str());
            // for (const auto &pair : metadata) {
            //     log("key: %s, value: %s", pair.first.c_str(),
            //         pair.second.c_str());
            // }

            p.index = res_i.i_next;

            parse_mode = ParseWhat::FindLevelName;
        } break;
        }
    }

    return true;
}