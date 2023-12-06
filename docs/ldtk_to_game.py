import json
import sys
import argparse

def get_tile_type(tileset_labels, id):
    for row in tileset_labels:
        if row['tileId'] == id:
            return row['data']

    # not found
    assert False

def tile_type_to_tile_char(tile_type):
    match tile_type:
        case 'PlayerSpawn':
            return 'P'
        case 'Goal':
            return 'G'
        case 'Floor':
            return 'F'
        case 'Box':
            return 'B'
        case 'Wall':
            return 'W'
        case 'Empty':
            return '.'
        case 'MirrorUL':
            return '<'
        case 'MirrorUR':
            return '>'
        case 'MirrorDL':
            return '['
        case 'MirrorDR':
            return ']'
        case tt:
            assert False, f"Found Unknown tile type: {tt}"

parser = argparse.ArgumentParser(description='Level importer')
parser.add_argument('--debug', action='store_true', help='Bring in debug levels')

args = parser.parse_args()

level_file = open("levels.ldtk", 'r')

if not level_file:
    print('could not read file')
    sys.exit(1)

ldtk_file = json.loads(level_file.read())

level_file.close()

if not ldtk_file:
    print('level not valid')
    sys.exit(1)

tileset = ldtk_file['defs']['tilesets'][1]
assert tileset['identifier'] == 'Lucyban_tileset'

tileset_labels = tileset['customData']

level_out = ""

real_level_count = 0
debug_level_count = 0

for level in ldtk_file["levels"]:

    if level['identifier'].startswith('Debug_'):
        debug_level_count += 1
    else:
        real_level_count += 1

    if level['identifier'].startswith('Debug_') and not args.debug:
        debug_level_count -= 1
        continue

    layer = level['layerInstances'][1]
    level_name = level['identifier'].replace('_', ' ')

    level_out += '---\n'
    level_out += level_name
    level_out += '\n--\n'

    width = int(level['pxWid'] / 32)
    height = int(level['pxHei'] / 32)

    # print(f"level size: {width} by {height}")

    assert layer['__identifier'] == 'ActualLevel'
    assert layer['__gridSize'] == 32

    last_y = 0

    for cell in layer['gridTiles']:
        x = int(cell['px'][0] / 32)
        y = int(cell['px'][1] / 32)

        if last_y != y:
            last_y = y
            level_out += '\n'

        tile_type = get_tile_type(tileset_labels, cell['t'])
        level_out += tile_type_to_tile_char(tile_type)

    level_out += '\n'

level_out += '\n'

with open("../assets/levels/1.lvl", "w") as level_out_file:
    level_out_file.write(level_out)

print(f"{real_level_count} real levels and {debug_level_count} debug levels exported.")
