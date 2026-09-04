#include "SimpleDodge.h"


namespace SimpleDodge
{

        // Any new float settings for the ini can just be added here.
    constexpr FloatSettingDef kFloatSettings[] = {
            { "Distance",          &g_distance },
            { "Duration",          &g_duration },
            { "Speed",             &g_speed },
            { "PostDodgeMomentum", &g_postDodgeMomentumFactor },
            { "LandingLockSeconds",&g_landingLockSeconds },
            { "InputLockSeconds",  &g_inputLockSeconds },
            { "StaminaCost",       &g_staminaCost },
            { "IFrameTime",        &g_iFrameTime },
            { "DodgeCooldownSeconds", &g_dodgeCooldownSeconds },
            { "TapDodgeWindow",        &g_tapDodgeWindowSeconds },
            { "SFXVolume",         &g_DodgeSoundVolume },
            { "ShakeStrength",         &g_ShakeStrength },
            { "ShakeDuration",         &g_ShakeDuration },
            ///Far Dodge settings:
            { "FarDodgeWindow",             &g_farDodgeWindowSeconds },
            { "FarDodge_StaminaMult",       &g_farDodgeStaminaMult },
            { "FarDodge_SpeedMult",         &g_farDodgeSpeedMult },
            { "FarDodge_DistMult",          &g_farDodgeDistMult },
            { "FarDodge_DurationMult",      &g_farDodgeDurationMult },
            { "FarDodge_InputLockSeconds",  &g_farDodgeInputLockSeconds },
            { "FarDodge_LandingLockSeconds",&g_farDodgeLandingLockSeconds },
            { "FarDodgeCooldownSeconds",    &g_farDodgeCooldownSeconds },
            { "SlowSpeedMult",         &g_slowMult },
            { "SlowSpeedTime",         &g_slowTime },
            { "SlowSpeedTimeFarDodge", &g_slowTimeFarDodge },
        };


        //for reading from .esp
        // Name of the plugin that owns the globals:
        constexpr auto kDodgePluginName = "CustomDodge.esp";  // <-- change this
        // Relative FormID for Dodge_IsDodging (strip the load index: 01000D62 -> 000D62)
        constexpr RE::FormID kIsDodgingFormID = 0x000D62;
        RE::TESGlobal* g_IsDodging = nullptr;
        constexpr RE::FormID kDirectionFormID = 0x000800; // Dodge_Direction 
        RE::TESGlobal* g_DodgeDirection = nullptr;
        constexpr RE::FormID kDistanceFormID = 0x000804;  // Dodge_Distance
        RE::TESGlobal* g_DistanceGlobal = nullptr;
        constexpr RE::FormID kSpeedFormID = 0x000801;  // Dodge_Speed
        RE::TESGlobal* g_SpeedGlobal = nullptr;
        constexpr RE::FormID kTimeFormID = 0x000803;  // Dodge_Time
        RE::TESGlobal* g_TimeGlobal = nullptr;
        constexpr RE::FormID kStaminaFormID = 0x000802;  // Dodge_StaminaCost
        RE::TESGlobal* g_StaminaGlobal = nullptr;
        constexpr RE::FormID kIFrameFormID = 0x000805;  // Dodge_IFrameTime
        RE::TESGlobal* g_IFrameGlobal = nullptr;
        constexpr RE::FormID kIFrameWindowFormID = 0x000806;  // Dodge_InIFrameWindow 
        RE::TESGlobal* g_IFrameWindowGlobal = nullptr;
        constexpr RE::FormID kIFramePerkFormID = 0x000807; //IFrame perk
        RE::BGSPerk* g_IFramePerk = nullptr;
        constexpr RE::FormID kOutgoingDamageGlobalFormID = 0x000809; // Dodge_OutgoingDamageMult global
        RE::TESGlobal* g_OutgoingDamageGlobal = nullptr;
        constexpr RE::FormID kDamagePerkFormID = 0x000808;           // Dodge_OutgoingDamagePerk_ID perk
        RE::BGSPerk* g_DamagePerk = nullptr;
        constexpr RE::FormID kIsFarDodgingFormID = 0x0000D64;        //Far dodge global
        RE::TESGlobal* g_IsFarDodging = nullptr;

        constexpr RE::FormID kDodgeStartSoundFormID = 0x00080A;           // Dodge takeoff sound
        RE::BGSSoundDescriptorForm* g_DodgeStartSound = nullptr;
        constexpr RE::FormID kDodgeEndSoundFormID = 0x00080B;           // Dodge land sound
        RE::BGSSoundDescriptorForm* g_DodgeEndSound = nullptr;
        constexpr RE::FormID kDodgeImodFormID = 0x00080C;           //IMod for dodge
        RE::TESImageSpaceModifier* g_DodgeImod = nullptr;
        RE::ImageSpaceModifierInstanceForm* g_DodgeImodInstance = nullptr;

        // master switch – if true, read values from TESGlobals instead of INI
        bool g_useGlobalVars = false;

        // if true, interpret StaminaCost as a percentage of max stamina
        bool g_staminaAsPercent = false;


        //DEBUG
        float g_debugPrintAccumulator = 0.0f;
        constexpr float g_debugPrintInterval = 0.5f;

    
     
    RE::NiPoint2 DirectionToLocalVector(DodgeDirection dir)
    {
        // unit circle 8-way; 0, not 1, and diagonals = 1/sqrt(2)
        constexpr float s = 0.70710678f;

        switch (dir) {
        case DodgeDirection::Forward:      return { 0.0f,  1.0f };
        case DodgeDirection::ForwardRight: return { s,     s };
        case DodgeDirection::Right:        return { 1.0f,  0.0f };
        case DodgeDirection::BackRight:    return { s,    -s };
        case DodgeDirection::Back:         return { 0.0f, -1.0f };
        case DodgeDirection::BackLeft:     return { -s,    -s };
        case DodgeDirection::Left:         return { -1.0f,  0.0f };
        case DodgeDirection::ForwardLeft:  return { -s,     s };
        case DodgeDirection::None:
        default:
            // No movement input at trigger time -> default to backward
            return { 0.0f, -1.0f };
        }
    }
 
    //For world-relative rotations during dodge.
    static inline float DegToRad(float deg)
    {
        return deg * 0.017453292519943295769f; // pi / 180
    }

    static inline RE::NiPoint2 Rotate2D(const RE::NiPoint2& v, float yawRad)
    {
        const float s = std::sin(yawRad);
        const float c = std::cos(yawRad);

        return RE::NiPoint2{
            v.x * c - v.y * s,
            v.x * s + v.y * c
        };
    }

   
    
    DodgeDirection GetLastInputDirection()
    {
        return g_lastInputDirection;
    }

    void SetLastInputDirection(DodgeDirection a_dir)
    {
        g_lastInputDirection = a_dir;
    }

    State& GetPlayerState()
    {
        return g_playerState;
    }

    bool IsTapDodgeEnabled()
    {
        return g_tapDodgeOnRelease;
    }

    float GetTapDodgeWindowSeconds()
    {
        return g_tapDodgeWindowSeconds;
    }


    
    static inline void Trim(std::string& s)
    {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

        // left trim
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        // right trim
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    }

    void ApplySingleSetting(const std::string& key, const std::string& value, bool& seenSpeed)
    {
        // First try float settings
        float f = std::strtof(value.c_str(), nullptr);

        for (auto& def : kFloatSettings) {
            if (_stricmp(key.c_str(), def.name) == 0) {
                *def.target = f;
                if (def.target == &g_speed) {
                    seenSpeed = true;
                }
                return;
            }
        }

        // Non-float / special cases below

        
        // 0/1 – use StaminaCost as percent of max stamina
        if (_stricmp(key.c_str(), "StaminaAsPercent") == 0) {
            long v = std::strtol(value.c_str(), nullptr, 10);
            g_staminaAsPercent = (v != 0);
            return;
        }
        
        // 0/1 - use vars from globals instead of ini
        if (_stricmp(key.c_str(), "UseGlobalVars") == 0) {
            // treat any non-zero as true
            long v = std::strtol(value.c_str(), nullptr, 10);
            g_useGlobalVars = (v != 0);
            return;
        }

        // allow attacking while dodging
        // 1 = you can attack during the dodge
        // 0 = attacks are blocked while the dodge is active
        if (_stricmp(key.c_str(), "AttackWhileDodging") == 0) {
            long v = std::strtol(value.c_str(), nullptr, 10);
            g_attackWhileDodging = (v != 0);
            return;
        }

        // block and attack strength when dodging.
        // 1 = multiply by 0 during dodge
        // 0 = attack/block at normal strength while dodging
        if (_stricmp(key.c_str(), "ModifyAttackDamage") == 0) {
            long v = std::strtol(value.c_str(), nullptr, 10);
            g_dodgeDamageNull = (v != 0);

            // If the ESP global is already resolved, reflect the setting immediately.
            if (g_OutgoingDamageGlobal) {
                g_OutgoingDamageGlobal->value = g_dodgeDamageNull ? 1.0f : 0.0f;
            }
            return;
        }

        // 0/1 – enable tap-to-dodge on key release.
        if (_stricmp(key.c_str(), "TapDodgeOnRelease") == 0) {
            long v = std::strtol(value.c_str(), nullptr, 10);
            g_tapDodgeOnRelease = (v != 0);
            return;
        }

        // 0/1 - Disable this dodge mod while in third person.
        if (_stricmp(key.c_str(), "DisableDodgeInThirdPerson") == 0) {
            long v = std::strtol(value.c_str(), nullptr, 10);
            g_disableDodgeInThirdPerson = (v != 0);

            spdlog::info(
                "SimpleDodge: DisableDodgeInThirdPerson = {}",
                g_disableDodgeInThirdPerson ? 1 : 0
            );

            return;
        }

        // key assigned to dodge
        if (_stricmp(key.c_str(), "DodgeKey") == 0) {
            uint32_t v = 0;
            if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0) {
                v = std::strtoul(value.c_str(), nullptr, 16);
            }
            else {
                v = std::strtoul(value.c_str(), nullptr, 10);
            }
            g_dodgeKey = v;
            return;
        }

        // key assigned to INI reload hotkey
        if (_stricmp(key.c_str(), "ReloadIniKey") == 0) {
            uint32_t v = 0;
            if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0) {
                v = std::strtoul(value.c_str(), nullptr, 16);
            }
            else {
                v = std::strtoul(value.c_str(), nullptr, 10);
            }
            g_reloadIniKey = v;
            return;
        }


        // 0/1 - Able to steer dodge?
        if (_stricmp(key.c_str(), "SteerDodge") == 0) {
            // treat any non-zero as true
            long v = std::strtol(value.c_str(), nullptr, 10);
            g_SteerDodge = (v != 0);
            return;
        }

        // Unknown keys: ignore or log
        // spdlog::info("SimpleDodge: unknown INI key '{}'", key);
    }

    void LoadSettings()
    {
        constexpr const char* kIniPath = "Data/SKSE/Plugins/CustomDodge.ini";

        std::ifstream file(kIniPath);
        if (!file.is_open()) {
            spdlog::info("SimpleDodge: INI not found ({}), using defaults.", kIniPath);
            return;
        }

        spdlog::info("SimpleDodge: loading settings from {}", kIniPath);

        bool seenSpeed = false;

        std::string line;
        while (std::getline(file, line)) {
            // Strip comments ; and #
            auto commentPos = line.find_first_of(";#");
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

            Trim(line);
            if (line.empty()) {
                continue;
            }

            // Skip [Section] headers
            if (!line.empty() && line.front() == '[' && line.back() == ']') {
                continue;
            }

            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);

            Trim(key);
            Trim(value);

            if (key.empty() || value.empty()) {
                continue;
            }

            ApplySingleSetting(key, value, seenSpeed);


        }
        // Backwards compatibility: if INI doesn't specify Speed,
        // approximate old behavior (distance / duration).
        if (!seenSpeed && g_duration > 0.0f) {
            g_speed = g_distance / g_duration;
        }
    }
    
    void ReloadIni()
    {
        SimpleDodge::LoadSettings();
        if (auto* console = RE::ConsoleLog::GetSingleton()) {
            console->Print("CustomDodge: reloaded Data/SKSE/Plugins/CustomDodge.ini");
        }
    }
    
    uint32_t GetDodgeKey()
    {
        return g_dodgeKey;
    }

    uint32_t GetReloadIniKey()
    {
        return g_reloadIniKey;
    }
    
    void InitGlobals()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return;
        }

        auto* form = dataHandler->LookupForm(kIsDodgingFormID, kDodgePluginName);
        g_IsDodging = form ? form->As<RE::TESGlobal>() : nullptr;

        //DEBUG. Does esp and globals load?
        /*
        if (!g_IsDodging) {
             spdlog::error("SimpleDodge::InitGlobals: FAILED to find Dodge_IsDodging (formID {:06X} in {})",
                 kIsDodgingFormID, kDodgePluginName);

             if (auto* console = RE::ConsoleLog::GetSingleton()) {
                 console->Print("Failed to find global.");
             }
         }

         if (g_IsDodging) {
             spdlog::info("SimpleDodge::InitGlobals: found Dodge_IsDodging at {:08X}", g_IsDodging->GetFormID());
             g_IsDodging->value = 0.0f;
             if (auto* console = RE::ConsoleLog::GetSingleton()) {
                 console->Print("Found global.");
             }
         }
         */
         //DEBUG END-----------------

        if (g_IsDodging) {
            g_IsDodging->value = 0.0f;  // start as "not dodging"
        }
        
        // Far-dodge global starts off as 0
        {
            auto* form = dataHandler->LookupForm(kIsFarDodgingFormID, kDodgePluginName);
            g_IsFarDodging = form ? form->As<RE::TESGlobal>() : nullptr;
            if (g_IsFarDodging) {
                g_IsFarDodging->value = 0.0f;
            }
        }


        // NEW: Dodge_Direction
        {
            auto* form = dataHandler->LookupForm(kDirectionFormID, kDodgePluginName);
            g_DodgeDirection = form ? form->As<RE::TESGlobal>() : nullptr;
            if (g_DodgeDirection) {
                g_DodgeDirection->value = 0.0f;  // no direction at startup
            }
        }

        //Various globals to pull info from if enabled
        {
            auto* form = dataHandler->LookupForm(kDistanceFormID, kDodgePluginName);
            g_DistanceGlobal = form ? form->As<RE::TESGlobal>() : nullptr;
        }
        {
            auto* form = dataHandler->LookupForm(kSpeedFormID, kDodgePluginName);
            g_SpeedGlobal = form ? form->As<RE::TESGlobal>() : nullptr;
        }
        {
            auto* form = dataHandler->LookupForm(kTimeFormID, kDodgePluginName);
            g_TimeGlobal = form ? form->As<RE::TESGlobal>() : nullptr;
        }
        {
            auto* form = dataHandler->LookupForm(kStaminaFormID, kDodgePluginName);
            g_StaminaGlobal = form ? form->As<RE::TESGlobal>() : nullptr;
        }
        {
            auto* form = dataHandler->LookupForm(kIFrameFormID, kDodgePluginName);
            g_IFrameGlobal = form ? form->As<RE::TESGlobal>() : nullptr;
        }
        {
            auto* form = dataHandler->LookupForm(kIFrameWindowFormID, kDodgePluginName); //whether in IFrame
            g_IFrameWindowGlobal = form ? form->As<RE::TESGlobal>() : nullptr;
            if (g_IFrameWindowGlobal) {
                g_IFrameWindowGlobal->value = 0.0f;
            }
        }
        {
            auto* form = dataHandler->LookupForm(kOutgoingDamageGlobalFormID, kDodgePluginName);
            g_OutgoingDamageGlobal = form ? form->As<RE::TESGlobal>() : nullptr;
            if (g_OutgoingDamageGlobal) {
                // Reflect current INI choice: 1 when enabled, 0 when disabled
                g_OutgoingDamageGlobal->value = g_dodgeDamageNull ? 1.0f : 0.0f;
            }
        }
        {
            auto* form = dataHandler->LookupForm(kDodgeStartSoundFormID, kDodgePluginName);
            g_DodgeStartSound = form ? form->As<RE::BGSSoundDescriptorForm>() : nullptr;
        }

        {
            auto* form = dataHandler->LookupForm(kDodgeEndSoundFormID, kDodgePluginName);
            g_DodgeEndSound = form ? form->As<RE::BGSSoundDescriptorForm>() : nullptr;
        }
        {
            auto* form = dataHandler->LookupForm(kDodgeImodFormID, kDodgePluginName);
            g_DodgeImod = form ? form->As<RE::TESImageSpaceModifier>() : nullptr;
        }

    }

    void EnsureIFramePerkOnPlayer()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return;
        }

        // Resolve and cache the perk pointer
        if (!g_IFramePerk) {
            auto* form = dataHandler->LookupForm(kIFramePerkFormID, kDodgePluginName);
            g_IFramePerk = form ? form->As<RE::BGSPerk>() : nullptr;
        }

        if (!g_IFramePerk) {
            // Optional: log an error here if you want.
            // spdlog::error("SimpleDodge: failed to resolve iframe perk {:06X} from {}", kIFramePerkFormID, kDodgePluginName);
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        // Don't add duplicates; perk system is rank-based, not "stacking" the same form.
        if (player->HasPerk(g_IFramePerk)) {
            return;
        }

        // Rank 0 is the normal "first rank" of the perk.
        player->AddPerk(g_IFramePerk, 0);
    }

    void EnsureDamagePerkOnPlayer()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return;
        }

        // Resolve and cache the outgoing-damage perk pointer
        if (!g_DamagePerk) {
            auto* form = dataHandler->LookupForm(kDamagePerkFormID, kDodgePluginName);
            g_DamagePerk = form ? form->As<RE::BGSPerk>() : nullptr;
        }

        if (!g_DamagePerk) {
            // Optional: log an error here if you want.
            // spdlog::error("SimpleDodge: failed to resolve damage perk {:06X} from {}", kDamagePerkFormID, kDodgePluginName);
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        // Don't add duplicates; perk system is rank-based, not "stacking" the same form.
        if (player->HasPerk(g_DamagePerk)) {
            return;
        }

        // Rank 0 is the normal "first rank" of the perk.
        player->AddPerk(g_DamagePerk, 0);
    }

    
    void SetIsDodging(bool a_isDodging)
    {
        if (g_IsDodging) {
            g_IsDodging->value = a_isDodging ? 1.0f : 0.0f;
        }
    }

    void SetIsFarDodging(bool a_isFarDodging)
    {
        if (g_IsFarDodging) {
            g_IsFarDodging->value = a_isFarDodging ? 1.0f : 0.0f;
        }
    }

    void SetDodgeDirection(DodgeDirection a_dir)
    {
        if (!g_DodgeDirection) {
            return;
        }

        if (a_dir == DodgeDirection::None) {
            g_DodgeDirection->value = 0.0f;
        }
        else {
            g_DodgeDirection->value = static_cast<float>(a_dir);  // 1–8
        }
    }

    float GetEffectiveDistance()
    {
        if (g_useGlobalVars && g_DistanceGlobal) {
            return g_DistanceGlobal->value;
        }
        return g_distance;
    }

    float GetEffectiveDuration()
    {
        if (g_useGlobalVars && g_TimeGlobal) {
            return g_TimeGlobal->value;
        }
        return g_duration;
    }

    float GetEffectiveSpeed()
    {
        if (g_useGlobalVars && g_SpeedGlobal) {
            return g_SpeedGlobal->value;
        }
        return g_speed;
    }

    float GetEffectiveStaminaCost()
    {
        if (g_useGlobalVars && g_StaminaGlobal) {
            return g_StaminaGlobal->value;
        }
        return g_staminaCost;
    }

    float GetResolvedStaminaCost(RE::PlayerCharacter* player)
    {
        if (!player) {
            return 0.0f;
        }

        // First: get the configured value (INI or global)
        float rawCost = GetEffectiveStaminaCost();
        if (rawCost <= 0.0f) {
            return 0.0f;
        }

        // If we're not in percent mode, just use it as-is
        if (!g_staminaAsPercent) {
            return rawCost;
        }

        // Percent mode: interpret rawCost as 0–100% of max stamina
        if (auto* avOwner = player->AsActorValueOwner()) {
            float maxStamina = avOwner->GetPermanentActorValue(RE::ActorValue::kStamina);
            if (maxStamina <= 0.0f) {
                return 0.0f;
            }

            // Clamp to [0, 100] just in case someone sets something crazy
            float pct = std::clamp(rawCost, 0.0f, 100.0f);
            return maxStamina * (pct / 100.0f);
        }

        return 0.0f;
    }

    float GetEffectiveIFrameTime()
    {
        if (g_useGlobalVars && g_IFrameGlobal) {
            return g_IFrameGlobal->value;
        }
        return g_iFrameTime;
    }

    void SetIFrameWindow(bool a_active)
    {
        if (g_IFrameWindowGlobal) {
            g_IFrameWindowGlobal->value = a_active ? 1.0f : 0.0f;
        }
    }

    bool SetActorGhost_VM(RE::Actor* a_actor, bool a_enable)
    {
        if (!a_actor) {
            return false;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            return false;
        }

        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            return false;
        }

        const auto handle = policy->GetHandleForObject(a_actor->GetFormType(), a_actor);
        if (!handle) {
            return false;
        }

        // IMPORTANT: force prvalue -> avoids FunctionArguments<void, bool&>
        auto* args = RE::MakeFunctionArguments(static_cast<bool>(a_enable));

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;

        const bool ok = vm->DispatchMethodCall2(
            handle,
            RE::BSFixedString("Actor"),
            RE::BSFixedString("SetGhost"),
            args,
            cb
        ); // DispatchMethodCall2 is part of the VM interface in NG 

        // CommonLib’s MakeFunctionArguments allocates; you own it.
        delete args;

        return ok;
    }

 
    void StartIFrameWindow()
    {
        const float windowTime = GetEffectiveIFrameTime();
        if (windowTime <= 0.0f) {
            // Feature disabled or zero; make sure global is off and state reset
            g_iFrameWindowActive = false;
            g_iFrameWindowRemaining = 0.0f;
            SetIFrameWindow(false);
            SetActorGhost_VM(RE::PlayerCharacter::GetSingleton(), false);
            return;
        }

        g_iFrameWindowActive = true;
        g_iFrameWindowRemaining = windowTime;
        SetIFrameWindow(true);
        SetActorGhost_VM(RE::PlayerCharacter::GetSingleton(), true);
    }

    void UpdateIFrameWindow(float dt)
    {
        if (!g_iFrameWindowActive || dt <= 0.0f) {
            return;
        }

        g_iFrameWindowRemaining -= dt;
        if (g_iFrameWindowRemaining <= 0.0f) {
            g_iFrameWindowActive = false;
            g_iFrameWindowRemaining = 0.0f;
            SetIFrameWindow(false);
            SetActorGhost_VM(RE::PlayerCharacter::GetSingleton(), false);
        }
    }

    void StartDodgeCooldown()
    {
        if (g_dodgeCooldownSeconds <= 0.0f) {
            // Cooldown disabled
            return;
        }

        g_dodgeCooldownActive = true;
        g_dodgeCooldownRemaining = g_dodgeCooldownSeconds;
    }

    void StartFarDodgeCooldown()
    {
        if (g_farDodgeCooldownSeconds <= 0.0f) {
            // Extra FarDodge cooldown disabled
            return;
        }

        g_farDodgeCooldownActive = true;
        g_farDodgeCooldownRemaining = g_farDodgeCooldownSeconds;
    }

    bool IsDodgeOnCooldown()
    {
        return g_dodgeCooldownActive || g_farDodgeCooldownActive;
    }

    void UpdateDodgeCooldown(float dt)
    {
        if (!g_dodgeCooldownActive || dt <= 0.0f) {
            return;
        }

        g_dodgeCooldownRemaining -= dt;
        if (g_dodgeCooldownRemaining <= 0.0f) {
            g_dodgeCooldownActive = false;
            g_dodgeCooldownRemaining = 0.0f;
        }
    }

    void UpdateFarDodgeCooldown(float dt)
    {
        if (!g_farDodgeCooldownActive || dt <= 0.0f) {
            return;
        }

        g_farDodgeCooldownRemaining -= dt;
        if (g_farDodgeCooldownRemaining <= 0.0f) {
            g_farDodgeCooldownActive = false;
            g_farDodgeCooldownRemaining = 0.0f;
        }
    }

    static float EvaluateDodgeCurve(float t)
    {
        if (t <= 0.0f) {
            return 1.0f;   // full speed at the very start
        }
        if (t >= 1.0f) {
            return 0.0f;   // no speed at / after the end
        }

        // Example: simple ease-out (fast at start, slows down)
        // curve(t) = 1 - (t^2)
        const float tt = t * t;
        return 1.0f - tt;
    }

    static float EvaluateFarDodgeCurve(float t)
    {
        // You can later swap this to something spikier or longer-tailed.
        return EvaluateDodgeCurve(t);
    }

    void CancelDodgeOnAir(RE::bhkCharacterController* controller)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !controller) {
            return;
        }

        auto* playerController = player->GetCharController();
        if (playerController != controller) {
            return;  // not the player
        }

        auto& st = g_playerState;
        if (!st.active) {
            return;  // nothing to cancel
        }

        // Just stop the dodge; we *don't* apply post-dodge braking here
        // so we don't mess with legit in-air momentum.
        FinishDodge(player, controller);

        // Ensure we won't apply any stale forward velocityMod.
        auto* vel = reinterpret_cast<float*>(&(controller->velocityMod));
        vel[0] = 0.0f;
        vel[1] = 0.0f;
    }
    
    bool DodgeIsSafe(RE::PlayerCharacter* player)
    {
        if (!player) return false; //null player
        
        if (player->IsDead() || player->IsInKillMove()) return false;

        auto* controller = player->GetCharController();
        if (!controller) return false;
        if (controller->context.currentState != RE::hkpCharacterStateType::kOnGround) return false; 

        if (player->IsOnMount()) return false; 
        
        
        // Actor state gates: sit/sleep, knock, fly, stagger
        if (auto* state = player->AsActorState()) {
            // Sitting / sleeping / in furniture
            if (state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) {
                return false;
            }
            // Knocked down / ragdolled / weird knock states
            if (state->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal) {
                return false;
            }
            // Flying (dragon riding / certain special states)
            if (state->GetFlyState() != RE::FLY_STATE::kNone) {
                return false;
            }
            //Stagger state
            if (state->actorState2.staggered != 0) {
                return false;
            }
        }

       
        auto* ui = RE::UI::GetSingleton();
        if (ui)
        {
            auto* ints = RE::InterfaceStrings::GetSingleton();
            if (ui->GameIsPaused()) return false;
            if (ui->IsMenuOpen(ints->dialogueMenu)) return false;
            if (ui->IsMenuOpen(ints->craftingMenu)) return false;
            if (ui->IsApplicationMenuOpen() || ui->IsItemMenuOpen()) {
                return false;
            }
        }

        // ControlMap: movement / fighting controls must be enabled. Not sure about fighting. 
        if (auto* controlMap = RE::ControlMap::GetSingleton()) {
            // If either movement or fighting controls are disabled, we bail.
            if (!controlMap->IsMovementControlsEnabled() ||
                !controlMap->IsFightingControlsEnabled()) {
                return false;
            }
        }

        auto* controls = RE::PlayerControls::GetSingleton();
        if (controls && controls->blockPlayerInput) return false; //vanilla control lockout

        //no weird conditions are true; safe to proceed
        return true;
    }

    void StartDodge(RE::PlayerCharacter* player)
    {
        
        if (!player) {return;}

        auto& st = g_playerState;

        //various safety checks
        if (!DodgeIsSafe(player)) return;

        //attack/block check
        if (!g_attackWhileDodging && IsAttackOrBlockActive(player)) {
            return;
        }

        // --- Far-dodge chaining: if we're already in a dodge, and within the window,
        // interpret this StartDodge() as "upgrade" to FarDodge instead of starting fresh.
        if (g_farDodgeWindowSeconds > 0.0f &&
            st.active &&
            st.elapsed <= g_farDodgeWindowSeconds)
        {
            // Also respect FarDodge cooldown – no chaining if its cooldown is active.
            if (!g_farDodgeCooldownActive) {
                StartFarDodge(player);
            }
            return;
        }


        // If we're not currently dodging (or window has expired), this is a normal dodge.
        // Respect BOTH the normal and FarDodge cooldowns.
        if (IsDodgeOnCooldown()) return;

        //stamina check
        if (!HasStaminaForDodge(player)) return;


        // --- Tunable parameters (from INI  or global vars) ---
        const float maxDistance = GetEffectiveDistance();   // 3D distance cutoff
        const float maxDuration = GetEffectiveDuration();   // time cutoff
        const float speed = GetEffectiveSpeed();      // base forward speed (units/sec)


        // Safety: avoid divide-by-zero; if duration is 0 or negative, bail.
        if (maxDuration <= 0.0f) {
            //DEBUG: 
            //spdlog::warn("SimpleDodge: duration <= 0, dodge not started");
            return;
        }

        st.active = true;
        st.elapsed = 0.0f;
        st.duration = maxDuration;
        st.baseSpeed = speed;
        st.maxDistance = maxDistance;
        st.startPos = player->GetPosition();  // 3D start point (slope-safe)
        st.localDir = DirectionToLocalVector(g_lastInputDirection); //Direction to dodge in, based on input

        


        // Compute a world-space direction from the current 3D transform
        {
            if (auto* root = player->Get3D()) {
                // Promote local 2D direction to 3D (no vertical component)
                RE::NiPoint3 local3{
                    st.localDir.x,
                    st.localDir.y,
                    0.0f
                };

                // root->world.rotate is an NiMatrix3 – this gives us world-space direction
                const auto& rot = root->world.rotate;
                RE::NiPoint3 world3 = rot * local3;

                // Zero any tiny vertical component and normalize
                world3.z = 0.0f;
                const float len = world3.Length();
                if (len > 1e-6f) {
                    st.WorldDir = world3 / len;
                }
                else {
                    // Fallback: just use current forward as worldDir
                    st.WorldDir = { 0.0f, 1.0f, 0.0f };
                }
            }
            else {
                // Fallback if there's no 3D yet: approximate with angles.z
                const auto angles = player->GetAngle();
                const float yawRad = DegToRad(angles.z);

                const float s = std::sin(yawRad);
                const float c = std::cos(yawRad);

                RE::NiPoint3 world3{
                    st.localDir.x * c - st.localDir.y * s,
                    st.localDir.x * s + st.localDir.y * c,
                    0.0f
                };

                const float len = world3.Length();
                if (len > 1e-6f) {
                    st.WorldDir = world3 / len;
                }
                else {
                    st.WorldDir = { 0.0f, 1.0f, 0.0f };
                }
            }
        }


        //block movement input immediately if configured
        StartInputLock(player, g_inputLockSeconds);

        // Prevent attacks, if option enabled
        StartAttackLock(player);

        //For Globals
        SetIsDodging(true);
        SetDodgeDirection(g_lastInputDirection);
        StartDodgeCooldown();

        //Start the iframes
        StartIFrameWindow();

        // consume stamina now that dodge has begun 
        ConsumeDodgeStamina(player);

        // Play start sound (3D at player)
        PlayDodgeSound(g_DodgeStartSound);

        //Imod
        ApplyDodgeImod();

        TriggerDodgeShake(g_ShakeStrength, g_ShakeDuration);

    }

    void StartFarDodge(RE::PlayerCharacter* player)
    {
        if (!player) {
            return;
        }

        auto& st = g_playerState;

        // We only allow FarDodge as an "upgrade" while a normal dodge is active.
        if (!st.active) {
            return;
        }

        // If the feature is disabled, bail out.
        if (g_farDodgeWindowSeconds <= 0.0f) {
            return;
        }

        // Safety: if we've somehow slipped past the window, don't fire FarDodge.
        if (st.elapsed > g_farDodgeWindowSeconds) {
            return;
        }

        // Respect FarDodge's own cooldown.
        if (g_farDodgeCooldownActive) {
            return;
        }

        // Check stamina for the second dodge.
        if (!HasStaminaForFarDodge(player)) {
            return;
        }

        // Base values (normal dodge) at the time FarDodge is triggered.
        const float baseDistance = GetEffectiveDistance();
        const float baseDuration = GetEffectiveDuration();
        const float baseSpeed = GetEffectiveSpeed();

        const float distMult = std::max(g_farDodgeDistMult, 0.0f);
        const float speedMult = std::max(g_farDodgeSpeedMult, 0.0f);
        const float durationMult = std::max(g_farDodgeDurationMult, 0.0f);

        float resolvedDuration = baseDuration * durationMult;
        if (resolvedDuration <= 0.0f) {
            return;
        }


        // Reset the dodge timers and magnitudes – we are *replacing* the original dodge.
        st.isFarDodge = true;
        st.elapsed = 0.0f;
        st.duration = resolvedDuration;               
        st.baseSpeed = baseSpeed * speedMult;      // scaled speed
        st.maxDistance = baseDistance * distMult;    // scaled distance
        st.startPos = player->GetPosition();      // new "start" position

        // Crucially: we do NOT touch st.localDir or st.WorldDir here.
        // That preserves the initial direction snapshot.

        // Restart input lock using FarDodge's own lock time if set, else reuse normal value.
        float inputLockTime = g_inputLockSeconds;
        if (g_farDodgeInputLockSeconds > 0.0f) {
            inputLockTime = g_farDodgeInputLockSeconds;
        }
        StartInputLock(player, inputLockTime);

        // Attack lock is the same as normal dodge.
        StartAttackLock(player);

        // Globals: IsDodging stays true, but IsFarDodging becomes true.
        SetIsDodging(true);
        SetIsFarDodging(true);
        // Direction global stays whatever the normal dodge set.

        // Start FarDodge cooldown (extra) and also refresh the normal cooldown
        // so the combined cooldown extends from the FarDodge start.
        StartFarDodgeCooldown();
        StartDodgeCooldown();

        // IFrames: DO NOT restart the iframe window – it continues uninterrupted.

        // Consume the second batch of stamina.
        ConsumeFarDodgeStamina(player);

        // Kick the same start sound / imod / shake for FarDodge.
        PlayDodgeSound(g_DodgeStartSound);
        ApplyDodgeImod();
        TriggerDodgeShake(g_ShakeStrength, g_ShakeDuration);
    }

    void ConsumeDodgeStamina(RE::PlayerCharacter* player)
    {
        if (!player) { return; }

        const float staminaCost = GetResolvedStaminaCost(player);
        
        if (staminaCost > 0.0f) {
            if (auto* avOwner = player->AsActorValueOwner()) {
                // Treat it as "damage" to stamina so it plays nice with AV layering
                avOwner->DamageActorValue(             // damage layer
                    RE::ActorValue::kStamina,         // which AV
                    -staminaCost                       // amount
                );
            }
        }
    }

    void ConsumeFarDodgeStamina(RE::PlayerCharacter* player)
    {
        if (!player) {
            return;
        }

        const float baseCost = GetResolvedStaminaCost(player);
        const float mult = std::max(g_farDodgeStaminaMult, 0.0f);
        const float staminaCost = baseCost * mult;

        if (staminaCost > 0.0f) {
            if (auto* avOwner = player->AsActorValueOwner()) {
                avOwner->DamageActorValue(
                    RE::ActorValue::kStamina,
                    -staminaCost
                );
            }
        }
    }

    bool HasStaminaForDodge(RE::PlayerCharacter* player)
    {

        if (!player) { return false; }

        const float staminaCost = GetResolvedStaminaCost(player);
        if (staminaCost > 0.0f) {
            if (auto* avOwner = player->AsActorValueOwner()) {
                const float currentStamina = avOwner->GetActorValue(RE::ActorValue::kStamina);

                if (currentStamina < staminaCost) {
                    // Not enough stamina, no dodge
                    return false;
                }
            }
        }
        return true;
    }

    bool HasStaminaForFarDodge(RE::PlayerCharacter* player)
    {
        if (!player) {
            return false;
        }

        const float baseCost = GetResolvedStaminaCost(player);
        const float mult = std::max(g_farDodgeStaminaMult, 0.0f);
        const float staminaCost = baseCost * mult;

        if (staminaCost > 0.0f) {
            if (auto* avOwner = player->AsActorValueOwner()) {
                const float currentStamina = avOwner->GetActorValue(RE::ActorValue::kStamina);
                if (currentStamina < staminaCost) {
                    return false;
                }
            }
        }
        return true;
    }

    void ApplyOnGround(RE::bhkCharacterController* controller)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !controller) {
            return;
        }

        // Only act on the player’s controller
        auto* playerController = player->GetCharController();
        if (playerController != controller) {
            return;
        }

        const float dt = controller->stepInfo.deltaTime;


        // always tick landing lock; it may be active even when not dodging
        if (dt > 0.0f) {
            UpdateLandingLock(controller, dt);
            UpdateInputLock(dt);
            UpdateDodgeCooldown(dt);
            UpdateFarDodgeCooldown(dt);
            UpdateIFrameWindow(dt);
            UpdatePostDodgeSlow(dt);
        }

        auto& st = g_playerState;
        if (!st.active) {
            // Not in a dodge; landing lock (if any) is handled above.
            return;
        }

        // Stop dodge if no longer on the ground.
        if (controller->context.currentState != RE::hkpCharacterStateType::kOnGround) {
            FinishDodge(player, controller);
            return;
        }

        if (dt <= 0.0f) {
            return;
        }

        //Stop dodge if blocking/attacking and ini is set to block.
        //if attacks/blocks are NOT allowed during a dodge, and the player
        if (!g_attackWhileDodging && IsAttackOrBlockActive(player)) {
            FinishDodge(player, controller);
            return;
        }

       
        // Update timer and clamp normalized time
        st.elapsed += dt;
        const float t = std::clamp(st.elapsed / st.duration, 0.0f, 1.0f);

        // If we've fully elapsed, we consider the dodge done.
        if (st.elapsed >= st.duration) {
            FinishDodge(player, controller);
            return;
        }

        // ---- 3D distance clamp (slope-safe) ----
        const RE::NiPoint3 currentPos = player->GetPosition();
        const RE::NiPoint3 delta = currentPos - st.startPos;
        const float traveled = delta.Length();  // 3D distance (includes vertical)

        if (traveled >= st.maxDistance) {
            // We've reached or exceeded our target distance in world space.
            // Turn off the dodge and zero our forward contribution for this frame.
            FinishDodge(player, controller);
            return;
        }

        // ---- Curve-based speed for this frame ----
        //Which dodge are we in?
        const float curveFactor = st.isFarDodge ?
            EvaluateFarDodgeCurve(t) :
            EvaluateDodgeCurve(t);   // [0..1]

        if (curveFactor <= 0.0f) {
            // No more speed requested by the curve
            FinishDodge(player, controller);
            return;
        }

        const float desiredSpeed = st.baseSpeed * curveFactor;  // units/sec in "local forward"

       //CHANGE DIRECTION DURING DODGE, DO THIS:
        if (g_SteerDodge)
        { 
            // Apply forward movement in character-local +Y via velocityMod.
            // We still use the packed-float hack because hkVector4 accessors differ by CLib version.
            auto* vel = reinterpret_cast<float*>(&(controller->velocityMod));
            // X = strafe, Y = forward/back in character-local space
            vel[0] = desiredSpeed * st.localDir.x;
            vel[1] = desiredSpeed * st.localDir.y;
       }

        //NO CHANGE DIRECTION  DURING DODGE, DO THIS:
        else //g_SteerDodge == false
        {  
            RE::NiPoint2 localDirThisFrame = st.localDir; // fallback

            if (auto* root = player->Get3D()) {
                const auto& rot = root->world.rotate;

                // Inverse of an orthonormal rotation matrix is its transpose
                RE::NiMatrix3 invRot = rot.Transpose();

                // Project the stored world direction back into *current* local space
                RE::NiPoint3 local3 = invRot * st.WorldDir;

                localDirThisFrame.x = local3.x;
                localDirThisFrame.y = local3.y;

                // Optional: renormalize in case of numerical drift
                const float len2 = localDirThisFrame.x * localDirThisFrame.x +
                    localDirThisFrame.y * localDirThisFrame.y;
                if (len2 > 1e-6f) {
                    const float invLen = 1.0f / std::sqrt(len2);
                    localDirThisFrame.x *= invLen;
                    localDirThisFrame.y *= invLen;
                }
            }
            // Feed the adjusted local direction into velocityMod as usual
            auto* vel = reinterpret_cast<float*>(&(controller->velocityMod));
            vel[0] = desiredSpeed * localDirThisFrame.x;
            vel[1] = desiredSpeed * localDirThisFrame.y;
        }
    }

    void FinishDodge(RE::PlayerCharacter* player, RE::bhkCharacterController* controller)
    {
        if (!player || !controller) {
            return;
        }

        auto& st = g_playerState;

        const bool wasFarDodge = st.isFarDodge;

        st.active = false;
        st.isFarDodge = false;

        SetIsDodging(false);
        SetIsFarDodging(false);
        SetDodgeDirection(DodgeDirection::None);

        //just to be safe, remove ghost again
        SetActorGhost_VM(player, false);

        // Play sound (3D at player)
        PlayDodgeSound(g_DodgeEndSound);

        // Imod
        RemoveDodgeImod();

        StopAttackLock();

        ApplyPostDodgeMomentum(controller);

        // Choose landing lock duration: FarDodge override if configured, else normal.
        float landingLockTime = g_landingLockSeconds;
        if (wasFarDodge && g_farDodgeLandingLockSeconds > 0.0f) {
            landingLockTime = g_farDodgeLandingLockSeconds;
        }
        StartLandingLock(player, landingLockTime);

        // Choose post-slow duration: separate ini time for far dodge.
        // Only if the time setting is > 0
        if ((g_slowTime > 0.0f || g_slowTimeFarDodge > 0.0f) && (g_slowMult != 1.f))
        { 
            float slowTime = g_slowTime;
            if (wasFarDodge && g_slowTimeFarDodge > 0.0f) {
                slowTime = g_slowTimeFarDodge;
            }
            StartPostDodgeSlow(slowTime);
        }
    }

    static void StopPostDodgeSlow()
    {
        g_postSlowActive = false;
        g_postSlowRemaining = 0.0f;

        // Remove whatever we applied
        if (gAppliedSlowBonus != 0.0f) {
            SetWantedSlowBonus(0.0f);
        }
    }

    static void StartPostDodgeSlow(float slowSeconds)
    {
        if (slowSeconds <= 0.0f) {
            StopPostDodgeSlow();
            return;
        }

        const float slowMult = std::clamp(g_slowMult, 0.0f, 10.0f);

        // No slowdown if >= 1.0
        if (slowMult >= 1.0f) {
            StopPostDodgeSlow();
            return;
        }

        // Convert multiplier to SpeedMult "bonus" (negative)
        // Example: 0.75 => wanted = (75 - 100) = -25
        const float wanted = (100.0f * slowMult) - 100.0f;

        SetWantedSlowBonus(wanted);

        g_postSlowActive = true;
        g_postSlowRemaining = slowSeconds;
    }

    static void UpdatePostDodgeSlow(float dt)
    {
        if (!g_postSlowActive || dt <= 0.0f) {
            return;
        }

        g_postSlowRemaining -= dt;
        if (g_postSlowRemaining <= 0.0f) {
            StopPostDodgeSlow();
        }
    }

    static void SetWantedSlowBonus(float wanted)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        const float delta = wanted - gAppliedSlowBonus;
        if (std::abs(delta) < 0.001f) {
            return;
        }

        auto* avo = player->AsActorValueOwner();
        if (!avo) {
            return;
        }

        // Use TEMPORARY lane (works for SpeedMult, stacks cleanly)
        avo->ModActorValue(
            RE::ACTOR_VALUE_MODIFIER::kTemporary,
            RE::ActorValue::kSpeedMult,
            delta
        );

        gAppliedSlowBonus = wanted;
    }

    void StartLandingLock(RE::PlayerCharacter* player, float lockSeconds)
    {
        if (!player) {
            return;
        }

        if (lockSeconds <= 0.0f) {
            // Feature disabled for this call
            return;
        }

        auto& st = g_playerState;

        // Already in landing lock? Just refresh duration.
        if (st.landingLockActive) {
            st.landingLockRemaining = lockSeconds;
            return;
        }

        st.landingLockActive = true;
        st.landingLockRemaining = lockSeconds;

        // --- Block movement input only ---
        if (auto* controls = RE::PlayerControls::GetSingleton()) {
            if (auto* moveHandler = controls->movementHandler) {
                // Remember the previous state so we don't fight other mods
                s_prevMovementInputEnabled = moveHandler->IsInputEventHandlingEnabled();
                moveHandler->SetInputEventHandlingEnabled(false);
            }
        }
    }

    void UpdateLandingLock(RE::bhkCharacterController* controller, float dt)
    {
        auto& st = g_playerState;

        if (st.landingLockActive) {
            if (dt > 0.0f) {
                st.landingLockRemaining -= dt;
                if (st.landingLockRemaining <= 0.0f) {
                    st.landingLockActive = false;
                    st.landingLockRemaining = 0.0f;

                    // Re-enable movement input to whatever it was before the lock
                    if (auto* controls = RE::PlayerControls::GetSingleton()) {
                        if (auto* moveHandler = controls->movementHandler) {
                            moveHandler->SetInputEventHandlingEnabled(s_prevMovementInputEnabled);
                        }
                    }
                }
            }
        }

        // While locked, force our own motion injection to zero as a backup.
        if (st.landingLockActive && controller) {
            auto* vel = reinterpret_cast<float*>(&(controller->velocityMod));
            vel[0] = 0.0f;  // local X
            vel[1] = 0.0f;  // local Y
        }
    }

    void StartInputLock(RE::PlayerCharacter* player, float lockSeconds)
    {
        if (!player) {
            return;
        }

        if (lockSeconds <= 0.0f) {
            // Feature disabled for this call
            return;
        }

        // If already active, just refresh the timer.
        if (g_inputLockActive) {
            g_inputLockRemaining = lockSeconds;
            return;
        }

        g_inputLockActive = true;
        g_inputLockRemaining = lockSeconds;

        // Block movement input only (same mechanism as landing lock)
        if (auto* controls = RE::PlayerControls::GetSingleton()) {
            if (auto* moveHandler = controls->movementHandler) {
                // Remember previous state so we don't fight other mods
                s_prevMovementInputEnabled = moveHandler->IsInputEventHandlingEnabled();
                moveHandler->SetInputEventHandlingEnabled(false);
            }
        }
    }

    void UpdateInputLock(float dt)
    {
        if (!g_inputLockActive || dt <= 0.0f) {
            return;
        }

        g_inputLockRemaining -= dt;
        if (g_inputLockRemaining <= 0.0f) {
            g_inputLockActive = false;
            g_inputLockRemaining = 0.0f;

            // Re-enable movement input to whatever it was before the lock
            if (auto* controls = RE::PlayerControls::GetSingleton()) {
                if (auto* moveHandler = controls->movementHandler) {
                    moveHandler->SetInputEventHandlingEnabled(s_prevMovementInputEnabled);
                }
            }
        }
    }

    
    
    
    
    void ApplyPostDodgeMomentum(RE::bhkCharacterController* controller)
        {
            // Clamp just in case someone sets a weird value from ini.
            float factor = std::clamp(g_postDodgeMomentumFactor, 0.0f, 1.0f);

            // Scale the *actual* physics velocity (outVelocity).
            auto* out = reinterpret_cast<float*>(&(controller->outVelocity));
            out[0] *= factor;  // X
            out[1] *= factor;  // Y (forward/back)
            out[2] *= factor;  // Z (up/down)

            // We also stop driving forward via velocityMod itself this frame;
            // we don't touch z here so vanilla movement can resume normally.
            auto* vel = reinterpret_cast<float*>(&(controller->velocityMod));
            vel[0] = 0.0f;
            vel[1] = 0.0f;
        }

     void StartAttackLock(RE::PlayerCharacter* player)
     {
         if (!player) {
             return;
         }

         // If config says attacks ARE allowed during dodge, do nothing.
         if (g_attackWhileDodging) {
             return;
         }

         auto* controls = RE::PlayerControls::GetSingleton();
         if (!controls) {
             return;
         }

         auto* attackHandler = controls->attackBlockHandler;
         if (!attackHandler) {
             return;
         }

         // If we already locked once for this dodge, don't stomp the previous state again.
         if (g_attackLockActive) {
             return;
         }

         // Cache previous state so we can restore it safely.
         s_prevAttackInputEnabled = attackHandler->IsInputEventHandlingEnabled();
         attackHandler->SetInputEventHandlingEnabled(false);

         g_attackLockActive = true;
     }

     void StopAttackLock()
     {
         if (!g_attackLockActive) {
             return;
         }

         auto* controls = RE::PlayerControls::GetSingleton();
         if (controls) {
             if (auto* attackHandler = controls->attackBlockHandler) {
                 // Restore whatever the state was before we touched it.
                 attackHandler->SetInputEventHandlingEnabled(s_prevAttackInputEnabled);
             }
         }

         g_attackLockActive = false;
     }

     bool IsAttackOrBlockActive(RE::PlayerCharacter* player)
     {
         if (!player) {
             return false;
         }

         // Check attack states via ActorState.
         if (auto* actorState = player->AsActorState()) {
             const auto atk = actorState->GetAttackState();
             // Disallow these states:
             // 1 = Draw, 2 = Swing, 3 = Hit, 6 = Bash
             if (atk == RE::ATTACK_STATE_ENUM::kDraw ||
                 atk == RE::ATTACK_STATE_ENUM::kSwing ||
                 atk == RE::ATTACK_STATE_ENUM::kHit ||
                 atk == RE::ATTACK_STATE_ENUM::kBash) {
                 return true;
             }
         }

         // Also disallow if currently blocking.
         if (player->IsBlocking()) {
             return true;
         }

         return false;
     }
     void PlayDodgeSound(RE::BGSSoundDescriptorForm* a_form)
     {
         if (!a_form) {
             return;
         }

         auto* audioMgr = RE::BSAudioManager::GetSingleton();
         if (!audioMgr) {
             return;
         }

         RE::BSSoundHandle soundHandle;

         // Get the sound handle from the descriptor.
         audioMgr->GetSoundHandle(
             soundHandle,
             a_form,
             0x1A
         );

         // Position the sound at the player.
         if (auto* player = RE::PlayerCharacter::GetSingleton()) {
             soundHandle.SetPosition(player->GetPosition());

             if (auto* node = player->Get3D()) {
                 soundHandle.SetObjectToFollow(node);
             }
         }

         soundHandle.Play();
     }

     void ApplyDodgeImod()
     {
         if (!g_DodgeImod) {
             return;
         }

         auto* player = RE::PlayerCharacter::GetSingleton();
         auto* camera = RE::PlayerCamera::GetSingleton();

         RE::NiAVObject* target = nullptr;
         if (camera && camera->cameraRoot) {
             target = camera->cameraRoot.get();
         }
         else if (player) {
             target = player->Get3D();
         }

         if (!target) {
             return;
         }

         // If we don't already have an instance, or it finished, make a new one
         if (!g_DodgeImodInstance) {
             g_DodgeImodInstance = g_DodgeImod->TriggerIfNotActive(1.0f, target);
         }
         else {
             // Re-use existing instance: bump its strength back up
             // (use whatever the header exposes: a strength field or SetStrength/SetTargetStrength)
             g_DodgeImodInstance->strength = 1.0f;   // or g_DodgeImodInstance->SetStrength(1.0f);
         }
     }

     void RemoveDodgeImod()
     {
         if (!g_DodgeImodInstance) {
             return;
         }

         // Turn the effect “off” by setting overall strength to zero
         g_DodgeImodInstance->strength = 0.0f;       // or g_DodgeImodInstance->SetStrength(0.0f);
     }

     void TriggerDodgeShake(float strength, float duration)
     {
         if (auto* player = RE::PlayerCharacter::GetSingleton()) {
             RE::ShakeCamera(strength, player->GetPosition(), duration);
         }
     }


     static void PrintConsole(const char* fmt, ...)
     {
         auto* cl = RE::ConsoleLog::GetSingleton();
         if (!cl) {
             return;
         }

         char buf[512]{};
         va_list args;
         va_start(args, fmt);
         vsnprintf(buf, sizeof(buf), fmt, args);
         va_end(args);

         cl->Print(buf);
     }



}