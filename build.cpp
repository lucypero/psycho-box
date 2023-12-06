/*
DISCLAIMER: DO NOT USE!!! USE PREMAKE.
    this was written as just an experiment. i am not using this to build 
    the project at the moment.

    it should still work, though. 

build script that writes the ninja file required for compilation
also writes the compile_commands.json file for intellisense.
*/

// TODO(lucy): Write the compile_commands.json file too.
// TODO(lucy): Maybe write all the files on a subdirectory to avoid all the mess.

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;
using std::string_view;

const string_view exe_name = "lucyban";

const string_view src_dir = "src";

const string_view cpp_flags = 
    "/W4 /sdl /MP /std:c++20 /EHsc /wd4100 /MDd "
    "/Z7 /D\"_DEBUG\" /Isrc /Ithird_party/include /nologo";

const string_view link_flags = 
    "/LIBPATH:\"third_party/lib\" /NOLOGO /SUBSYSTEM:WINDOWS /DEBUG /SUBSYSTEM:WINDOWS";

const string_view link_deps = 
    "user32.lib gdi32.lib d3d11.lib dxgi.lib "
    "dxguid.lib freetype.lib assimp-vc143-mt.lib imguid.obj D3DCompiler.lib";

int main() {

    // Check if the folder exists
    if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) {
        std::cerr << "Folder does not exist or is not a directory." << std::endl;
        return 1;
    }

    std::vector<std::string> cpp_files = {};

    // Iterate through the files in the folder
    for (const auto& entry : fs::directory_iterator(src_dir)) {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".cpp")
            continue;

        std::string fileNameWithoutExtension = entry.path().stem().string();
        cpp_files.push_back(fileNameWithoutExtension);
    }

    std::ofstream outputFile("build.ninja");

    if (!outputFile.is_open()) {
        std::cerr << "Failed to open the file!" << std::endl;
        return 1;
    }

    outputFile << "ninja_required_version = 1.6" << std::endl;
    outputFile << std::endl;

    outputFile << "rule cxx" << std::endl;
    outputFile << "  command = cl " << cpp_flags << " /showIncludes /c /Tp$in /Fo$out" << std::endl;
    outputFile << "  deps = msvc" << std::endl;

    outputFile << std::endl;

    outputFile << "rule link" << std::endl;
    outputFile << "  command = link $in " <<
      link_deps << " " << link_flags << " /out:$out" << std::endl;

    outputFile << std::endl;

    for(const auto &f: cpp_files) {
        outputFile << "build bin/obj/Debug/" << f << ".obj: cxx " << src_dir << "/" << f << ".cpp" << std::endl;
    }

    outputFile << std::endl;

    outputFile << "build bin/Debug/" << exe_name << ".exe: link ";

    for(const auto &f: cpp_files) {
        outputFile << "bin/obj/Debug/" << f << ".obj ";
    }

    outputFile << std::endl;

    outputFile.close();

    return 0;
}