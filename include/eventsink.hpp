#pragma once

#include <chrono>
#include <cstdio>   // for std::snprintf
#include <cstring>  // for std::strcmp
#include "SimpleDodge.h"
#include "hook.h"
#include "logger.hpp"

namespace eventsink
{

    //INPUT TRIGGERING
    // -----------------------------------------------------------------------------
    // Pick any DIK scancode
    // 0x38 = Left Alt, 0x2D = X, etc. This is the raw keyboard scancode.
    constexpr std::uint32_t kForwardDodgeScanCode = 0x22;  // G currently

    class DodgeInputSink : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static DodgeInputSink* GetSingleton()
        {
            static DodgeInputSink singleton;
            return std::addressof(singleton);
        }

        void Register()
        {
            auto* deviceManager = RE::BSInputDeviceManager::GetSingleton();
            if (deviceManager) {
                deviceManager->AddEventSink(this);
            }
        }

        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_events,
            RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (!a_events) {
                return RE::BSEventNotifyControl::kContinue;
            }

         

            //DEBUG
            // Periodic debug print of current input state (~4 times per second)
            ///*
            {
                const auto now = std::chrono::steady_clock::now();
                if (now - lastDebugPrint >= kDebugInterval) {
                    lastDebugPrint = now;
                    //DebugPrintInput();
                }
            }
            //*/


            for (auto input = *a_events; input; input = input->next) {

                // Analog gamepad movement (left thumbstick)
                if (input->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
                    if (auto* ide = input->AsIDEvent()) {
                        auto* stick = static_cast<RE::ThumbstickEvent*>(ide);
                        if (stick && stick->IsLeft()) {
                            // Normalize so +X = right, +Y = forward 
                            gamepadMove.x = stick->xValue;
                            gamepadMove.y = stick->yValue;
                        }
                    }
                    // Dont fall through: this event is not a ButtonEvent
                    continue;
                }

                // Buttons = keyboard/gamepad buttons (dodge key + WASD, etc.)
                auto* button = input->AsButtonEvent();
                if (!button) {
                    continue;
                }

                if (SimpleDodge::g_disableDodgeInThirdPerson) {
                    auto* camera = RE::PlayerCamera::GetSingleton();

                    if (camera && camera->IsInThirdPerson()) {
                        // spdlog::info("[DODGE] Dodge blocked: player is in third person.");
                        return RE::BSEventNotifyControl::kContinue;
                    }
                }

                HandleButton(button);

            }

            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        // --- Movement state we maintain across frames ---
        bool kbForward{ false };
        bool kbBack{ false };
        bool kbLeft{ false };
        bool kbRight{ false };

        RE::NiPoint2 gamepadMove{ 0.0f, 0.0f };  // x = left/right, y = forward/back

        // Simple constants
        static constexpr std::uint32_t kForwardDodgeScanCode = 0x22;  // G
        static constexpr float kGamepadDeadzone = 0.02f;  // tweak if needed


        // PI for angle math
        static constexpr float kPi = 3.14159265358979323846f;
        static constexpr float kTwoPi = 6.28318530717958647692f;


        // --- DEBUG POLLING ---
        std::chrono::steady_clock::time_point lastDebugPrint{ std::chrono::steady_clock::now() };
        static constexpr std::chrono::milliseconds kDebugInterval{ 250 };

        // --- Tap-to-dodge state ---
        bool tapDodgeKeyHeld{ false };
        std::chrono::steady_clock::time_point tapDodgePressTime{};


        const char* DirectionToString(SimpleDodge::DodgeDirection dir) const
        {
            using SimpleDodge::DodgeDirection;
            switch (dir) {
            case DodgeDirection::None:         return "None";
            case DodgeDirection::Forward:      return "Forward";
            case DodgeDirection::ForwardRight: return "ForwardRight";
            case DodgeDirection::Right:        return "Right";
            case DodgeDirection::BackRight:    return "BackRight";
            case DodgeDirection::Back:         return "Back";
            case DodgeDirection::BackLeft:     return "BackLeft";
            case DodgeDirection::Left:         return "Left";
            case DodgeDirection::ForwardLeft:  return "ForwardLeft";
            default:                           return "Unknown";
            }
        }

        //DEBUG for printing global vars
        static constexpr auto kDodgePlugin = "CustomDodge.esp";
        static constexpr RE::FormID kIsDodgingFormID = 0x000D62;
        static constexpr RE::FormID kDirFormID = 0x0012C7;
        //DEBUG .25 sec to console

        void DebugPrintInput()
        {
            auto* console = RE::ConsoleLog::GetSingleton();
            if (!console) {
                return;
            }

            // Reuse ComputeDirection to see what *would* be used right now.
            const auto dir = ComputeDirection();

            char buf[256];
            std::snprintf(
                buf,
                sizeof(buf),
                "DodgeInput: kb=(W:%d S:%d A:%d D:%d) gp=(%.2f, %.2f) dir=%s",
                kbForward ? 1 : 0,
                kbBack ? 1 : 0,
                kbLeft ? 1 : 0,
                kbRight ? 1 : 0,
                gamepadMove.x,
                gamepadMove.y,
                DirectionToString(dir)
            );

            //Print direction:
            //console->Print(buf);

            /*
            //print global variable IsDodging:
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                if (auto* form = data->LookupForm(kIsDodgingFormID, kDodgePlugin)) {
                    if (auto* global = form->As<RE::TESGlobal>()) {
                        if (auto* console = RE::ConsoleLog::GetSingleton()) {
                            console->Print("Dodge_IsDodging = %.1f", global->value);
                        }
                    }
                    else {
                        if (auto* console = RE::ConsoleLog::GetSingleton()) {
                            console->Print("ERROR: Form found but is not TESGlobal");
                        }
                    }
                }
                else {
                    if (auto* console = RE::ConsoleLog::GetSingleton()) {
                        console->Print("ERROR: Global not found (formID %06X)", kIsDodgingFormID);
                    }
                }
            }
            //

            //Debug DodgeDir global
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                if (auto* form = data->LookupForm(kDirFormID, kDodgePlugin)) {
                    if (auto* global = form->As<RE::TESGlobal>()) {
                        if (auto* console = RE::ConsoleLog::GetSingleton()) {
                            console->Print("Dodge_Direction = %.1f", global->value);
                        }
                    }
                }
            }
            //
            */


        }


        //Recieves both gamepad and keyboard buttons (not sticks)
        void HandleButton(RE::ButtonEvent* a_button)
        {
            // Movement should follow mapped actions, not hardcoded scancodes.
            const bool isPress = a_button->IsPressed();
            const bool isRelease = a_button->IsUp();

            auto updateKey = [&](bool& keyState) {
                if (isPress) {
                    keyState = true;
                }
                else if (isRelease) {
                    keyState = false;
                }
                };

            // High-level action name, e.g. "Forward", "Back", "Strafe Left", "Strafe Right"
            const auto& ev = a_button->GetUserEvent();
            if (ev.data()) {
                if (std::strcmp(ev.c_str(), "Forward") == 0) {
                    updateKey(kbForward);
                }
                else if (std::strcmp(ev.c_str(), "Back") == 0) {
                    updateKey(kbBack);
                }
                else if (std::strcmp(ev.c_str(), "Strafe Left") == 0) {
                    updateKey(kbLeft);
                }
                else if (std::strcmp(ev.c_str(), "Strafe Right") == 0) {
                    updateKey(kbRight);
                }
            }

            // Dodge hotkey: custom scancode (G by default).
            const auto scanCode = a_button->GetIDCode();
            if (scanCode == SimpleDodge::GetDodgeKey()) {

                // Mode 1: normal behavior � dodge on press.
                if (!SimpleDodge::IsTapDodgeEnabled()) {
                    if (a_button->IsDown()) {
                        TriggerDodge();
                    }
                    return;
                }

                // Mode 2: tap-to-dodge � dodge on release if held <= window.
                auto now = std::chrono::steady_clock::now();
                const float window = SimpleDodge::GetTapDodgeWindowSeconds();

                // On press, start tracking.
                if (a_button->IsDown()) {
                    tapDodgeKeyHeld = true;
                    tapDodgePressTime = now;
                }
                // On release, if we were tracking, decide whether to dodge.
                else if (isRelease && tapDodgeKeyHeld) {
                    tapDodgeKeyHeld = false;

                    if (window > 0.0f) {
                        const float heldSeconds =
                            std::chrono::duration<float>(now - tapDodgePressTime).count();

                        // Only treat as a tap if held within the window.
                        if (heldSeconds <= window) {
                            TriggerDodge();
                        }
                    }
                }
            }

            // INI reload hotkey (optional)
            const auto reloadKey = SimpleDodge::GetReloadIniKey();
            if (reloadKey != 0 && scanCode == reloadKey) {
                if (a_button->IsDown()) {
                    SimpleDodge::LoadSettings();

                    if (auto* cl = RE::ConsoleLog::GetSingleton()) {
                        cl->Print("CustomDodge: reloaded Data/SKSE/Plugins/CustomDodge.ini");
                    }
                }
                return;
            }


        }

        // Determines dodge direction, then kicks off dodge.
        void TriggerDodge()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }

            auto& st = SimpleDodge::GetPlayerState();

            // If we're NOT already dodging, this is a normal dodge start:
            if (!st.active) {
                const auto dir = ComputeDirection();

                // No movement input: no dodge.
                if (dir == SimpleDodge::DodgeDirection::None) {
                    return;
                }

                SimpleDodge::SetLastInputDirection(dir);
            }
            // If we ARE already dodging, we DON'T touch the direction �
            // FarDodge wants to reuse the initial direction snapshot.

            SimpleDodge::StartDodge(player);
        }

        SimpleDodge::DodgeDirection ComputeDirection() const
        {
            // --- Keyboard movement vector ---
            RE::NiPoint2 kbVec{ 0.0f, 0.0f };
            if (kbForward) kbVec.y += 1.0f;
            if (kbBack)    kbVec.y -= 1.0f;
            if (kbRight)   kbVec.x += 1.0f;
            if (kbLeft)    kbVec.x -= 1.0f;

            auto normalize = [](RE::NiPoint2& v) {
                const float lenSq = v.x * v.x + v.y * v.y;
                if (lenSq > 0.0f) {
                    const float invLen = 1.0f / std::sqrt(lenSq);
                    v.x *= invLen;
                    v.y *= invLen;
                }
                };

            normalize(kbVec);

            // --- Gamepad movement vector ---
            RE::NiPoint2 gpVec = gamepadMove;
            const float gpLenSq = gpVec.x * gpVec.x + gpVec.y * gpVec.y;
            if (gpLenSq < (kGamepadDeadzone * kGamepadDeadzone)) {
                gpVec.x = gpVec.y = 0.0f;
            }
            else {
                RE::NiPoint2 tmp = gpVec;
                normalize(tmp);
                gpVec = tmp;
            }

            // --- Choose winner: gamepad overrides if active, else keyboard ---
            RE::NiPoint2 chosen{ 0.0f, 0.0f };
            if (gpVec.x != 0.0f || gpVec.y != 0.0f) {
                chosen = gpVec;
            }
            else {
                chosen = kbVec;
            }

            const float lenSq = chosen.x * chosen.x + chosen.y * chosen.y;
            if (lenSq <= 0.0f) {
                return SimpleDodge::DodgeDirection::None;
            }

            // Angle where 0 rad = forward (+Y), positive X = right
            float angle = std::atan2(chosen.x, chosen.y);  // note: atan2(x, y)
            if (angle < 0.0f) {
                angle += kTwoPi;
            }

            const float sectorSize = kTwoPi / 8.0f;
            // Center the sectors by adding half a slice
            int sector = static_cast<int>(std::floor((angle + sectorSize * 0.5f) / sectorSize)) & 7;

            using SimpleDodge::DodgeDirection;

            switch (sector) {
            case 0:  return DodgeDirection::Forward;
            case 1:  return DodgeDirection::ForwardRight;
            case 2:  return DodgeDirection::Right;
            case 3:  return DodgeDirection::BackRight;
            case 4:  return DodgeDirection::Back;
            case 5:  return DodgeDirection::BackLeft;
            case 6:  return DodgeDirection::Left;
            case 7:  return DodgeDirection::ForwardLeft;
            default: return DodgeDirection::None;
            }
        }
    };
    // -----------------------------------------------------------------------------

    //ANIM EVENTS
    // -----------------------------------------------------------------------------
    class PlayerAnimEventSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
    {
    public:
        static PlayerAnimEventSink* GetSingleton()
        {
            static PlayerAnimEventSink s;
            return std::addressof(s);
        }

        // Same signature pattern as in OldAttackPlugin.cpp
        virtual RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>* /*a_source*/) override
        {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* actor = const_cast<RE::TESObjectREFR*>(a_event->holder);
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!actor || !player || actor != player) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* console = RE::ConsoleLog::GetSingleton();
            if (console) {
                const char* tag = a_event->tag.c_str();

                char buf[128];
                std::snprintf(
                    buf,
                    sizeof(buf),
                    "AnimEvent: %s",
                    (tag && tag[0]) ? tag : "(null)"
                );
                //DEBUG: Print anim graph events
                //console->Print(buf);
            }

            return RE::BSEventNotifyControl::kContinue;
        }

        void Register()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* console = RE::ConsoleLog::GetSingleton();

            if (!player) {
                if (console) {
                    console->Print("PlayerAnimEventSink::Register(): PLAYER NOT FOUND");
                }
                return;
            }

            player->AddAnimationGraphEventSink(this);

            if (console) {
                console->Print("PlayerAnimEventSink: registered on player");
            }
        }
    };

    struct PlayerCellEvent : RE::BSTEventSink<RE::BGSActorCellEvent>
    {
        static void RegisterEventSink();
        static PlayerCellEvent* GetSingleton()
        {
            static PlayerCellEvent singleton;
            return &singleton;
        }
    private:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::BGSActorCellEvent* a_event,
            RE::BSTEventSource<RE::BGSActorCellEvent>*) override;
    };
    // ideal for combat events runs on all npcs must filter for player
    struct CombatEventSink : public RE::BSTEventSink<RE::TESCombatEvent>
    {
        CombatEventSink() = default;
        CombatEventSink(const CombatEventSink&) = delete;
        CombatEventSink(CombatEventSink&&) = delete;
        CombatEventSink& operator=(const CombatEventSink&) = delete;
        CombatEventSink& operator=(CombatEventSink&&) = delete;
        static void RegisterEventSink();
        static CombatEventSink* GetSingleton()
        {
            static CombatEventSink singleton;
            return &singleton;
        }
    private:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESCombatEvent* event,
            RE::BSTEventSource<RE::TESCombatEvent>*) override;
    };

}