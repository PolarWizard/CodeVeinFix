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
#include <numbers>
#include <cmath>

// 3rd party includes
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "yaml-cpp/yaml.h"
#include "safetyhook.hpp"

// Local includes
#include "utils.hpp"

#define LOG(STRING, ...) spdlog::info("{} : " STRING, __func__, ##__VA_ARGS__)

// .yml to struct
typedef struct resolution_t {
    int width;
    int height;
    float aspectRatio; 
} resolution_t;
typedef struct pillarbox_t {
    bool enable;
} pillarbox_t;
typedef struct fov_t {
    bool enable;
    float multiplier;
} fov_t;

typedef struct fix_t {
    pillarbox_t pillarbox;
    fov_t fov;
} fix_t;

typedef struct yml_t {
    std::string name;
    bool masterEnable;
    resolution_t resolution;
    fix_t fix;
} yml_t;

HMODULE baseModule = GetModuleHandle(NULL);
YAML::Node config = YAML::LoadFile("CodeVeinFix.yml");
yml_t yml;

float nativeAspectRatio = 16.0f / 9.0f;

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
    LOG("-------------------------------------");
    LOG("Module Name: {:s}", exeName.c_str());
    LOG("Module Path: {:s}", exeFilePath.string().c_str());
    LOG("Module Addr: 0x{:x}", (uintptr_t)baseModule);
}

void readYml() {
    yml.name = config["name"].as<std::string>();

    yml.masterEnable = config["masterEnable"].as<bool>();

    yml.resolution.width = config["resolution"]["width"].as<int>();
    yml.resolution.height = config["resolution"]["height"].as<int>();

    yml.fix.pillarbox.enable = config["fixes"]["pillarbox"]["enable"].as<bool>();

    yml.fix.fov.enable = config["fixes"]["fov"]["enable"].as<bool>();
    yml.fix.fov.multiplier = config["fixes"]["fov"]["multiplier"].as<float>();

    // Initialize globals
    if (yml.resolution.width == 0 || yml.resolution.height == 0) {
        std::pair<int, int> dimensions = Utils::GetDesktopDimensions();
        yml.resolution.width  = dimensions.first;
        yml.resolution.height = dimensions.second;
    }
    yml.resolution.aspectRatio = (float)yml.resolution.width / (float)yml.resolution.height;

    LOG("Name: {}", yml.name);
    LOG("MasterEnable: {}", yml.masterEnable);
    LOG("Resolution.Width: {}", yml.resolution.width);
    LOG("Resolution.Height: {}", yml.resolution.height);
    LOG("Resolution.AspectRatio: {}", yml.resolution.aspectRatio);
    LOG("Fix.Pillarbox.Enable: {}", yml.fix.pillarbox.enable);
    LOG("Fix.Fov.Enable: {}", yml.fix.fov.enable);
    LOG("Fix.Fov.Multuplier: {}", yml.fix.fov.multiplier);
}

void pillarBoxFix() {
    const char* patternFind  = "F6 41 2C 01 4C";
    const char* patternPatch = "F6 41 2C 00";

    bool enable = yml.masterEnable & yml.fix.pillarbox.enable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        uint8_t* hit = (uint8_t*)addr[0];
        uintptr_t absAddr = (uintptr_t)hit;
        uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
        if (hit) {
            LOG("Found '{}' @ 0x{:x}", patternFind, relAddr);
            Utils::patch(absAddr, patternPatch);
            LOG("Patched '{}' with '{}'", patternFind, patternPatch);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

void resolutionFix() {
    const char* patternFind  = "39 8E E3 3F";
    const char* patternPatch;

    LOG("Desktop resolution: {}x{}", 
        yml.resolution.width, yml.resolution.height
    );
    LOG("Aspect Ratio: {}:{} {}", 
        yml.resolution.width / std::gcd(yml.resolution.width, yml.resolution.height), 
        yml.resolution.height / std::gcd(yml.resolution.width, yml.resolution.height),
        yml.resolution.aspectRatio
    );

    // Use acquired desktop resolution to resolve the pattern that will be used to patch
    std::string patternPatchString = Utils::bytesToString(
        (void*)&yml.resolution.aspectRatio, 
        sizeof(yml.resolution.aspectRatio)
    );
    patternPatch = patternPatchString.c_str();

    bool enable = yml.masterEnable & yml.fix.pillarbox.enable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        for (size_t i = 0; i < addr.size(); i++) {
            uint8_t* hit = (uint8_t*)addr[i];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            if (hit) {
                LOG("Found '{}' @ 0x{:x}", 
                    patternFind, relAddr
                );
                Utils::patch(absAddr, patternPatch);
                LOG("Patched '{}' with '{}'", 
                    patternFind, patternPatch
                );
            }
            else {
                LOG("Did not find '{}'", patternFind);
            }
        }
    }
}

void fovFix() {
    const char* patternFind  = "F3 0F 10 81 9C 03 00 00 0F 57 C9 0F 2F C1";
    uintptr_t hookOffset = 8;
    bool enable = yml.masterEnable & yml.fix.pillarbox.enable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        uint8_t* hit = (uint8_t*)addr[0];
        uintptr_t absAddr = (uintptr_t)hit;
        uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
        if (hit) {
            LOG("Found '{}' @ 0x{:x}", patternFind, relAddr);
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            static SafetyHookMid fovMidHook{};
            fovMidHook = safetyhook::create_mid(reinterpret_cast<void*>(hookAbsAddr),
                [](SafetyHookContext& ctx) {
                    float pi = std::numbers::pi_v<float>;
                    float newFov = atanf(tanf(68.0f * pi / 360.0f) / nativeAspectRatio * yml.resolution.aspectRatio) * 360.0f / pi;
                    ctx.xmm0.f32[0] = newFov * yml.fix.fov.multiplier;
                }
            );
            LOG("Hooked @ 0x{:x} + 0x{:x} = 0x{:x}", relAddr, hookOffset, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

DWORD __stdcall Main(void*) {
    logInit();
    readYml();
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
        LOG("DLL_PROCESS_ATTACH");
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

