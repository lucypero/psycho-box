package main

import "core:fmt"
import "core:os"
import "core:strings"

compile_flags :: "-Ithird_party/include -std:c++20 -D_DEBUG -W4 -WX"
src_folder :: "src"

main :: proc() {

    d, derr := os.open(src_folder, os.O_RDONLY)
    if derr != 0 {
        fmt.println("error opening source folder")
        return
    }

    {
        file_info, ferr := os.fstat(d)

        if ferr != 0 {
            return
        }
        if !file_info.is_dir {
            return
        }
    }

    fis, _ := os.read_dir(d, -1)

    cpp_files := make([dynamic]string)

    proj_folder : string

    for fi, i in fis {

        // getting project folder
        proj_folder = fi.fullpath[:len(fi.fullpath) - len(fi.name) - len(src_folder) - 1]

        if len(fi.name) <= 3 {
            continue
        }

        ext := fi.name[len(fi.name) - 3:]

        if ext == "cpp" {
            append(&cpp_files, fi.name)
        }
    }

    proj_folder_w_normal_slashes : string

    {
        b := strings.builder_make(0, len(proj_folder))
        for r in proj_folder {
            if r == '\\' {
                strings.write_rune(&b, '/')
            } else {
                strings.write_rune(&b, r)
            }
        }
        proj_folder_w_normal_slashes = strings.to_string(b)
    }

    builder := strings.builder_make()

    strings.write_string(&builder, "[\n")

    for cpp_file, i in cpp_files {
        strings.write_string(&builder, "{\n")
        strings.write_string(&builder, `"directory": "`)
        strings.write_string(&builder, proj_folder_w_normal_slashes)
        strings.write_string(&builder, "\",\n")

        strings.write_string(&builder, `"command": "cl -c src/`)
        strings.write_string(&builder, cpp_file)
        strings.write_string(&builder, " ")
        strings.write_string(&builder, compile_flags)
        strings.write_string(&builder, "\",\n")

        strings.write_string(&builder, `"file": "src/`)
        strings.write_string(&builder, cpp_file)
        strings.write_string(&builder, "\"\n")

        if i == len(cpp_files) - 1 {
            strings.write_string(&builder, "}\n")
        } else {
            strings.write_string(&builder, "},\n")
        }
    }

    strings.write_string(&builder, "]\n")
    os.write_entire_file("compile_commands.json", builder.buf[:])
}