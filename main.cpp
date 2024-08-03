/*
 * MIT License
 * 
 * Copyright (c) 2024 Dominik Protasewicz
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// System includes
#include <windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <numeric>

// 3rd party includes
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "yaml-cpp/yaml.h"

// Local includes
#include "utils.hpp"

HMODULE baseModule = GetModuleHandle(NULL);
YAML::Node config = YAML::LoadFile("CodeVeinFix.yml");

void logInit() {
    // spdlog initialisation
    auto logger = spdlog::basic_logger_mt("CodeVein", "CodeVeinFix.log");
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::debug);

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    std::filesystem::path exeFilePath = exePath;
    std::string exeName = exeFilePath.filename().string();

    // Log module details
    spdlog::info("-------------------------------------");
    spdlog::info("Module Name: {:s}", exeName.c_str());
    spdlog::info("Module Path: {:s}", exeFilePath.string().c_str());
    spdlog::info("Module Addr: 0x{:x}", (uintptr_t)baseModule);
}

void pillarBoxFix() {
    const char* patternFind  = "F6 41 2C 01 4C";
    const char* patternPatch = "F6 41 2C 00";

    if (config["settings"]["pillarbox_fix"].as<bool>()) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        uint8_t* hit = (uint8_t*)addr[0];
        uintptr_t absAddr = (uintptr_t)hit;
        uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
        if (hit) {
            spdlog::info("{} : Found '{}' @ 0x{:x}", 
                __func__, patternFind, relAddr
            );
            Utils::patch(absAddr, patternPatch);
            spdlog::info("{} : Patched '{}' with '{}'", 
                __func__, patternFind, patternPatch
            );
        }
        else {
            spdlog::info("{} : Did not find '{}'", __func__, patternFind);
        }
    }
}

void resolutionFix() {
    const char* patternFind  = "39 8E E3 3F";

    // Get desktop resolution and perform some calculations
    std::pair<int, int> desktopDimensions = Utils::GetDesktopDimensions();
    int resAbsWidth = desktopDimensions.first;
    int resAbsHeight = desktopDimensions.second;
    int resGcdWidth = resAbsWidth / std::gcd(resAbsWidth, resAbsHeight);
    int resGcdHeight = resAbsHeight / std::gcd(resAbsWidth, resAbsHeight);
    float aspectRatio = (float)resAbsWidth / (float)resAbsHeight;

    spdlog::info("{} : Desktop resolution: {}x{}", 
        __func__, resAbsWidth, resAbsHeight
    );
    spdlog::info("{} : Aspect Ratio: {}:{} {}", 
        __func__, resGcdWidth, resGcdHeight, aspectRatio
    );

    // Use acquired desktop resolution to resolve the pattern that will be used to patch
    std::string patternPatchString = Utils::bytesToString((void*)&aspectRatio, sizeof(aspectRatio));
    const char* patternPatch = patternPatchString.c_str();

    if (config["settings"]["fov_fix"].as<bool>()) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        for (size_t i = 0; i < addr.size(); i++) {
            uint8_t* hit = (uint8_t*)addr[i];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            if (hit) {
                spdlog::info("{} : Found '{}' @ 0x{:x}", 
                    __func__, patternFind, relAddr
                );
                Utils::patch(absAddr, patternPatch);
                spdlog::info("{} : Patched '{}' with '{}'", 
                    __func__, patternFind, patternPatch
                );
            }
            else {
                spdlog::info("{} : Did not find '{}'", __func__, patternFind);
            }
        }
    }
}

void fovFix() {
    const char* patternFind  = "35 FA 0E 3C A4";
    const char* patternPatch = "EF D4 83 3C";

    if (config["settings"]["fov_fix"].as<bool>()) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        uint8_t* hit = (uint8_t*)addr[0];
        uintptr_t absAddr = (uintptr_t)hit;
        uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
        if (hit) {
            spdlog::info("{} : Found '{}' @ 0x{:x}", 
                __func__, patternFind, relAddr
            );
            Utils::patch(absAddr, patternPatch);
            spdlog::info("{} : Patched '{}' with '{}'", 
                __func__, patternFind, patternPatch
            );
        }
        else {
            spdlog::info("{} : Did not find '{}'", __func__, patternFind);
        }
    }
}

DWORD __stdcall Main(void*) {
    logInit();
    resolutionFix();
    pillarBoxFix();
    fovFix();
    return true;
}

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    HANDLE mainHandle;
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        spdlog::info("DLL_PROCESS_ATTACH");
        mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

