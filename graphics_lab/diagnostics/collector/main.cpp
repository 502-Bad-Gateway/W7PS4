// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <string_view>

int main(const int argc, const char* const argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "shadPS4 Graphics Lab trace collector 0.1.0\n";
        return 0;
    }
    std::cout
        << "shadPS4 Graphics Lab trace collector foundation\n"
        << "Usage: shadps4_trace_collector --version\n"
        << "The shared-memory flight recorder and trace decoder are not implemented yet.\n";
    return 0;
}

