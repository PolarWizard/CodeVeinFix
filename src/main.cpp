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

// Macros
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

// Globals
HMODULE baseModule = GetModuleHandle(NULL);
YAML::Node config = YAML::LoadFile("CodeVeinFix.yml");
yml_t yml;

float nativeAspectRatio = 16.0f / 9.0f;

/**
 * @brief Initializes logging for the application.
 *
 * This function performs the following tasks:
 * 1. Initializes the spdlog logging library and sets up a file logger.
 * 2. Retrieves and logs the path and name of the executable module.
 * 3. Logs detailed information about the module to aid in debugging.
 *
 * @return void
 */
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

/**
 * @brief Reads and parses configuration settings from a YAML file.
 *
 * This function performs the following tasks:
 * 1. Reads general settings from the configuration file and assigns them to the `yml` structure.
 * 2. Initializes global settings if certain values are missing or default.
 * 3. Logs the parsed configuration values for debugging purposes.
 *
 * @return void
 */
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

/**
 * @brief Applies a pillar box fix by patching a specific memory pattern.
 *
 * This function performs the following tasks:
 * 1. Checks if the pillar box fix is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Patches the found pattern with a new value to correct the pillar box issue.
 *
 * @details
 * The function first checks if the pillar box fix is enabled according to the configuration. It
 * then searches for a predefined byte sequence in the memory of the base module. When the pattern
 * is found, it patches the memory with a new byte sequence to address the pillar box issue.
 *
 * The pattern to find and the pattern to use for patching are hardcoded as byte sequences.
 *
 * How was this found?
 * No idea, will update this later.
 * 
 * @return void
 */
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

/**
 * @brief Applies a resolution fix by patching a specific memory pattern.
 *
 * This function performs the following tasks:
 * 1. Logs the current desktop resolution and aspect ratio.
 * 2. Determines the pattern to use for patching based on the aspect ratio.
 * 3. Searches for a specific memory pattern in the base module.
 * 4. Patches the found patterns with the calculated aspect ratio.
 *
 * @details
 * The function first logs the desktop resolution and aspect ratio for debugging purposes.
 * It then converts the aspect ratio to a byte pattern for use in memory patching.
 * The function performs a pattern scan to find occurrences of a predefined byte sequence in the
 * memory of the base module. For each occurrence found, it applies a patch using the calculated
 * aspect ratio.
 *
 * The patching is only performed if the fix is enabled according to the configuration.
 *
 * How was this found?
 * All Unreal Engine 4 games store the aspect ratio, a variable amount of times throughout the
 * executable. It is always hard coded to 16:9 and in order to get the game to render on
 * ultrawide+ resolutions, it is necessary to patch the aspect ratio to your target ratio,
 * usually 21:9 or 32:9.
 * 
 * So every location that has 16:9 (39 8E E3 3F) will be patched with target aspect ratio
 * hex pattern, for 21:9: 8E E3 18 40 (typically), for 32:9: 39 8E 63 40.
 * 
 * This game has two instances of the 16:9 pattern in the executable, so both will be patched.
 * 1. CodeVein-Win64-Shipping.exe+6A63D3D
 * 2. CodeVein-Win64-Shipping.exe+6A64786
 * 
 * @return void
 */
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

/**
 * @brief Applies a field of view (FOV) fix by hooking a specific pattern in memory.
 *
 * This function performs the following tasks:
 * 1. Checks if the FOV fix is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Hooks the identified pattern to modify the FOV value.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * If the pattern is found, a hook is created at an offset from the found pattern address. The hook
 * modifies the FOV value by adjusting it according to the aspect ratio and multiplier specified in
 * the configuration.
 *
 * The hook function calculates the new FOV value using trigonometric functions based on the current
 * aspect ratio and the configuration multiplier, then applies this new value to the appropriate register.
 *
 * How was this found?
 * With Universal Unreal Engine 4 Unlocker (UUU), through experimentation 68.0 is the default FOV value.
 * Using cheat engine the FOV value was tracked down to be at 0x4E87_756C and only one instruction
 * accesses this memory address: F3 0F10 81 9C030000 - movss xmm0,[rcx+0000039C].
 * Although that is only the instruction that interacts with the FOV value, there are more instructions
 * that paint a picture what is actually going on:
 * 1 - CodeVein-Win64-Shipping.exe+F7B8B80 : F3 0F10 81 9C030000  movss   xmm0,[rcx+0000039C]
 * 2 - CodeVein-Win64-Shipping.exe+F7B8B88 : 0F57 C9              xorps   xmm1,xmm1
 * 3 - CodeVein-Win64-Shipping.exe+F7B8B8B : 0F2F C1              comiss  xmm0,xmm1
 * 4 - CodeVein-Win64-Shipping.exe+F7B8B8E : 77 08                ja      0x4E87_7574
 * 5 - CodeVein-Win64-Shipping.exe+F7B8B90 : F3 0F10 81 18040000  movss   xmm0,[rcx+00000418]
 * 6 - CodeVein-Win64-Shipping.exe+F7B8B98 : C3                   ret
 * 
 * What's interesting to note is that the first instruction that reads the FOV value that location
 * is actually empty by default! When giving the `fov <value>` command to UUU it will write that 
 * value to this location, and if no value is given to the `fov` command it will write 0. And the 
 * next two instructions check if the FOV value is 0 or not. If it is then it will xmm0 will be 
 * loaded with the value of at rcx+00000418 (0x4E87_75E8), which is to 68.0. This value actually 
 * some instructions that continously rewrite it but we wont be exploring what is happening there, 
 * as we can use the first instruction to inject a new FOV value.
 * 
 * Based on all this information there is a lot one can do to change the FOV:
 * 1. Patch the new FOV into rcx+0000039C (0x4E87_756C)
 * 2. Hook the new FOV into xmm0 after instruction 1
 * 3. Explore what is writing into rcx+00000418 (0x4E87_75E8), and hook the new FOV into
 *    the correct register just before the write to that location takes place
 * 
 * There are probably alternate things that can be done too if you look long enough.
 * 
 * Anyway, for this fix it was decided that hooking was the best choice. This is subjective
 * though and wont be going into the details of why this choice was made.
 * 
 * @return void
 */
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

/**
 * @brief Main function that initializes and applies various fixes.
 *
 * This function serves as the entry point for the DLL. It performs the following tasks:
 * 1. Initializes the logging system.
 * 2. Reads the configuration from a YAML file.
 * 3. Applies a resolution fix.
 * 4. Applies a pillar box fix.
 * 5. Applies a field of view (FOV) fix.
 *
 * @param lpParameter Unused parameter.
 * @return Always returns TRUE to indicate successful execution.
 */
DWORD __stdcall Main(void* lpParameter) {
    logInit();
    readYml();
    resolutionFix();
    pillarBoxFix();
    fovFix();
    return true;
}

/**
 * @brief Entry point for a DLL, called by the system when the DLL is loaded or unloaded.
 *
 * This function handles various events related to the DLL's lifetime and performs actions
 * based on the reason for the call. Specifically, it creates a new thread when the DLL is
 * attached to a process.
 *
 * @details
 * The `DllMain` function is called by the system when the DLL is loaded or unloaded. It handles
 * different reasons for the call specified by `ul_reason_for_call`. In this implementation:
 *
 * - **DLL_PROCESS_ATTACH**: When the DLL is loaded into the address space of a process, it
 *   creates a new thread to run the `Main` function. The thread priority is set to the highest,
 *   and the thread handle is closed after creation.
 *
 * - **DLL_THREAD_ATTACH**: Called when a new thread is created in the process. No action is taken
 *   in this implementation.
 *
 * - **DLL_THREAD_DETACH**: Called when a thread exits cleanly. No action is taken in this implementation.
 *
 * - **DLL_PROCESS_DETACH**: Called when the DLL is unloaded from the address space of a process.
 *   No action is taken in this implementation.
 *
 * @param hModule Handle to the DLL module. This parameter is used to identify the DLL.
 * @param ul_reason_for_call Indicates the reason for the call (e.g., process attach, thread attach).
 * @param lpReserved Reserved for future use. This parameter is typically NULL.
 * @return BOOL Always returns TRUE to indicate successful execution.
 */
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
        break; // Ensure each case is properly terminated
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
