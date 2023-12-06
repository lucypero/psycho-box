#ifdef _DEBUG

#include "system.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "utils.hpp"
#include "renderer.hpp"

u32 run_command(const char *command, const char *cwd) {
    STARTUPINFO si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    // Set the STARTF_USESHOWWINDOW flag and hide the window
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    DWORD exit_code = 1;

    // Create the process
    if (CreateProcessA(0, (char *)command, 0, 0, FALSE, 0, 0, cwd, &si, &pi)) {
        // Wait for the process to finish
        WaitForSingleObject(pi.hProcess, INFINITE);

        // Check the exit code of the process
        GetExitCodeProcess(pi.hProcess, &exit_code);

        // Close process and thread handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return (u32)exit_code;
}

void run_command_checked(const char *command, const char *cwd) {
    u32 exit_code = run_command(command, cwd);
    if (exit_code != 0) {
        log("command \"%s\" failed w code %i", command, exit_code);
        lassert(false);
    }
}

void pack_release_build() {
    // - build optimized shaders, put the blobs in assets/shader_bin
    compile_all_shaders_to_files();

    // - import levels from ldtk
    // - make new folder and copy:
    //   - all the dlls
    //   - all the assets
    //   - the exe in Release
    // - copy assets
    run_command_checked("python ldtk_to_game.py", ".\\docs");
    run_command_checked("rm -f -r tmp");
    run_command_checked("mkdir tmp");
    run_command_checked("cp assimp-vc143-mt.dll freetype.dll minizip.dll msvcp140.dll pugixml.dll vcruntime140.dll "
                        "vcruntime140_1.dll zlib1.dll tmp");
    run_command_checked("cp bin/Release/psychobox.exe tmp");
    run_command_checked("cp assets tmp -r");

    // - zip the folder
    run_command_checked("mkdir -p packed_builds");
    run_command_checked("rm -f packed_builds/new_build.zip");
    run_command_checked("7z a -tzip packed_builds/new_build.zip tmp/.");
    // - rm tmp
    run_command_checked("rm -r tmp");

    log("Build was packed.");
}

#endif