#pragma once

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "logger.hpp"
#include "SimpleDodge.h"

using json = nlohmann::json;

inline std::filesystem::path GetDataDir()
{
    const auto root =
        std::filesystem::path(REL::Module::get().filename()).parent_path();

    return root / "Data";
}

inline int GetSprintKey()
{
    auto* controlMap = RE::ControlMap::GetSingleton();

    if (!controlMap) {
        logger::warn("ControlMap not found while getting Sprint key");
        return -1;
    }

    // Gameplay context
    auto* context =
        controlMap->controlMap[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay];

    if (!context) {
        logger::warn("Gameplay input context not found");
        return -1;
    }

    // Keyboard = 0
    auto& keyboardMappings = context->deviceMappings[0];

    for (auto& mapping : keyboardMappings)
    {
        if (mapping.eventID == "Sprint")
        {
            if (mapping.inputKey != 0xFF)
            {
                return mapping.inputKey;
            }
        }
    }

    logger::warn("Could not find Sprint keyboard mapping");
    return -1;
}

static constexpr bool IsGamepadOffset(std::uint32_t v)
{
    return v >= 266 && v <= 281;  // SKSE::InputMap::kMacro_GamepadOffset .. kMaxMacros-1
}


// Converts a raw XInput bitmask-style value (0x0001, 0x1000, etc.) into a
// CommonLib gamepad offset (266-281). Returns kMaxMacros if unrecognized.
static std::uint32_t GamepadOffsetToMask(std::uint32_t offset)
{
    switch (offset) {
    case 266: return 0x0001;  // Dpad Up
    case 267: return 0x0002;  // Dpad Down
    case 268: return 0x0004;  // Dpad Left
    case 269: return 0x0008;  // Dpad Right
    case 270: return 0x0010;  // Start
    case 271: return 0x0020;  // Back/Select
    case 272: return 0x0080;  // Left Stick Click  (matches your table: 272 = Left Stick)
    case 273: return 0x0040;  // Right Stick Click (273 = Right Stick)
    case 274: return 0x0100;  // Left Shoulder
    case 275: return 0x0200;  // Right Shoulder
    case 276: return 0x1000;  // A / Cross
    case 277: return 0x2000;  // B / Circle
    case 278: return 0x4000;  // X / Square
    case 279: return 0x8000;  // Y / Triangle
    case 280: return 0x0009;  // Left Trigger
    case 281: return 0x000A;  // Right Trigger
    default:   return 0;       // not a valid offset
    }
}

inline void SyncDodgeKey()
{
    const auto root =
        std::filesystem::path(REL::Module::get().filename()).parent_path();

    const std::filesystem::path settingsPath =
        root / "Data" / "MCM" / "Settings" / "DodgeFramework.ini";

    const std::filesystem::path iniPath =
        root / "Data" / "SKSE" / "Plugins" / "CustomDodge.ini";

    // ---------------------------------------------------------
    // Read DodgeFramework settings
    // ---------------------------------------------------------

    std::ifstream settingsFile(settingsPath);

    if (!settingsFile.is_open()) {
        logger::warn(
            "Failed to open DodgeFramework settings: {}",
            settingsPath.string());

        return;
    }

    int dodgeKey = -1;
    int useSprintButton = 0;
    float sprintHoldDuration = 0.25f;

    std::string line;

    while (std::getline(settingsFile, line))
    {
        // Remove whitespace
        line.erase(
            std::remove_if(
                line.begin(),
                line.end(),
                [](unsigned char c) {
                    return std::isspace(c);
                }),
            line.end());

        if (line.starts_with("uDodgeKey="))
        {
            try {
                dodgeKey = std::stoi(
                    line.substr(std::string("uDodgeKey=").length()));
            }
            catch (...) {
                logger::warn("Failed to parse uDodgeKey");
            }
        }
        else if (line.starts_with("bUseSprintButton="))
        {
            try {
                useSprintButton = std::stoi(
                    line.substr(std::string("bUseSprintButton=").length()));
            }
            catch (...) {
                logger::warn("Failed to parse bUseSprintButton");
            }
        }
        else if (line.starts_with("fSprintHoldDuration="))
        {
            try {
                sprintHoldDuration = std::stof(
                    line.substr(std::string("fSprintHoldDuration=").length()));
            }
            catch (...) {
                logger::warn("Failed to parse fSprintHoldDuration");
            }
        }
    }

    settingsFile.close();

    // ---------------------------------------------------------
    // Determine DodgeKey and Tap Dodge settings
    // ---------------------------------------------------------

    int tapDodgeOnRelease = 0;
    float tapDodgeWindow = 0.0f;

    if (useSprintButton == 1)
    {
        // Use the player's actual Sprint key as DodgeKey
        const int sprintKey = GetSprintKey();

        if (sprintKey != -1)
        {
            dodgeKey = sprintKey;

            logger::info(
                "DodgeFramework is using Sprint button. "
                "Sprint key detected: {}",
                sprintKey);
        }
        else
        {
            logger::warn(
                "DodgeFramework is configured to use Sprint button, "
                "but the Sprint key could not be found.");
        }

        // Tap-to-dodge mode
        tapDodgeOnRelease = 1;

        // Use DodgeFramework's sprint hold duration
        tapDodgeWindow = sprintHoldDuration;
    }
    else
    {
        // Normal dodge mode
        tapDodgeOnRelease = 0;

        // Not used when tap dodge is disabled
        tapDodgeWindow = 0.0f;
    }

    // ---------------------------------------------------------
    // APPLY DIRECTLY TO CUSTOM DODGE RUNTIME GLOBALS
    // ---------------------------------------------------------

    SimpleDodge::g_dodgeKey = static_cast<std::uint32_t>(dodgeKey);

    if (!IsGamepadOffset(SimpleDodge::g_dodgeKey)) {
        std::uint32_t asOffset = GamepadOffsetToMask(SimpleDodge::g_dodgeKey);
        if (asOffset != SKSE::InputMap::kMaxMacros) {
            logger::info("uDodgeKey {} looked like an XInput mask; normalized to {}",
                SimpleDodge::g_dodgeKey, asOffset);
            SimpleDodge::g_dodgeKey = asOffset;
        }
    }

    SimpleDodge::g_tapDodgeOnRelease = (tapDodgeOnRelease != 0);

    SimpleDodge::g_tapDodgeWindowSeconds = tapDodgeWindow;

    logger::info(
        "Applied CustomDodge runtime settings: "
        "DodgeKey={}, TapDodgeOnRelease={}, TapDodgeWindow={}",
        SimpleDodge::g_dodgeKey,
        SimpleDodge::g_tapDodgeOnRelease,
        SimpleDodge::g_tapDodgeWindowSeconds);

    // ---------------------------------------------------------
    // Read CustomDodge.ini;
    // ---------------------------------------------------------

    std::ifstream iniFile(iniPath);

    if (!iniFile.is_open()) {
        logger::warn(
            "Failed to open CustomDodge.ini: {}",
            iniPath.string());

        return;
    }

    std::vector<std::string> lines;

    while (std::getline(iniFile, line))
    {
        lines.push_back(line);
    }

    iniFile.close();

    // ---------------------------------------------------------
    // Helper to replace an INI value
    // ---------------------------------------------------------

    auto UpdateIniValue =
        [&](const std::string& key, const std::string& value)
        {
            bool found = false;

            for (auto& currentLine : lines)
            {
                std::string compareLine = currentLine;

                compareLine.erase(
                    std::remove_if(
                        compareLine.begin(),
                        compareLine.end(),
                        [](unsigned char c) {
                            return std::isspace(c);
                        }),
                    compareLine.end());

                if (compareLine.starts_with(key + "="))
                {
                    currentLine = key + " = " + value;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                lines.push_back(
                    key + " = " + value);
            }
        };

    // ---------------------------------------------------------
    // Update CustomDodge.ini
    // ---------------------------------------------------------

    UpdateIniValue(
        "DodgeKey",
        std::to_string(dodgeKey));

    UpdateIniValue(
        "TapDodgeOnRelease",
        std::to_string(tapDodgeOnRelease));

    UpdateIniValue(
        "TapDodgeWindow",
        std::to_string(tapDodgeWindow));

    // ---------------------------------------------------------
    // Rewrite CustomDodge.ini
    // ---------------------------------------------------------

    std::ofstream outputFile(
        iniPath,
        std::ios::trunc);

    if (!outputFile.is_open()) {
        logger::warn(
            "Failed to write CustomDodge.ini: {}",
            iniPath.string());

        return;
    }

    for (const auto& outputLine : lines)
    {
        outputFile << outputLine << '\n';
    }

    logger::info(
        "Synced CustomDodge settings: "
        "DodgeKey={}, TapDodgeOnRelease={}, TapDodgeWindow={}",
        dodgeKey,
        tapDodgeOnRelease,
        tapDodgeWindow);
}