#pragma once

#ifdef _DEBUG

#include "lucytypes.hpp"

u32 run_command(const char *command, const char *cwd = 0);
void run_command_checked(const char *command, const char *cwd = 0);
void pack_release_build();
#endif