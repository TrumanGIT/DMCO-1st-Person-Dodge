#pragma once

#include <cmath>
#include <fstream>
#include <string>
#include <algorithm>
#include "logger.hpp"


namespace SimpleDodge
{
    // One extremely simple “impulse” type dodge
    struct State
    {
        bool          active{ false };
        float         elapsed{ 0.0f };     // seconds since start
        float         duration{ 0.0f };    // total dodge time (s)
        float         baseSpeed{ 0.0f };   // "peak" speed in units/sec
        float         maxDistance{ 0.0f }; // target world-space distance (units)
        RE::NiPoint3  startPos{};          // starting world position
        RE::NiPoint2  localDir{ 0.0f, -1.0f };  // dodge direction, default: forward
        bool          isFarDodge{ false }; 

        // world-space dodge direction (unit length, horizontal)
        RE::NiPoint3  WorldDir{ 0.0f, 0.0f, 0.0f };

        // Post-dodge landing lock (block movement input)
        bool          landingLockActive{ false };
        float         landingLockRemaining{ 0.0f };  // seconds left

    };

    //Directions
    enum class DodgeDirection : std::uint8_t
    {
        None = 0,
        Forward = 1,
        ForwardRight = 2,
        Right = 3,
        BackRight = 4,
        Back = 5,
        BackLeft = 6,
        Left = 7,
        ForwardLeft = 8
    };


    //DODGE INPUT------------------------------
    
    // Get/Set the direction snapshot we had when the dodge key was pressed.
    DodgeDirection GetLastInputDirection();
    void SetLastInputDirection(DodgeDirection a_dir);

    RE::NiPoint2 DirectionToLocalVector(DodgeDirection dir);

    //Handle tap vs. hold
    bool IsTapDodgeEnabled();
    float GetTapDodgeWindowSeconds();

    //------------------------------


    //DODGE IMPLEMENTATION--------------------------
    
    // Global state just for the player for now
    State& GetPlayerState();

    // Called from input: 
    void StartDodge(RE::PlayerCharacter* player);
    // Called from within StartDodge:
    void StartFarDodge(RE::PlayerCharacter* player);

    // Consumes the stamina when dodge successfully triggers
    void ConsumeDodgeStamina(RE::PlayerCharacter* player);
    void ConsumeFarDodgeStamina(RE::PlayerCharacter* player);

    //Checks if has enough stamina for dodge
    bool HasStaminaForDodge(RE::PlayerCharacter* player);
    bool HasStaminaForFarDodge(RE::PlayerCharacter* player);
    
    //Global var 
    void SetIsDodging(bool a_isDodging);
    void SetIsFarDodging(bool a_isFarDodging);
    void SetDodgeDirection(DodgeDirection a_dir);


    // Called from the Havok on-ground physics tick
    // to inject forward velocity when the dodge is active.
    void ApplyOnGround(RE::bhkCharacterController* controller);

    //Master function for dodge end
    void FinishDodge(RE::PlayerCharacter* player, RE::bhkCharacterController* controller);

    //Various checks to do to prevent dodge. 
    bool DodgeIsSafe(RE::PlayerCharacter* player);

    // What happens when dodge ends re:momentum
    void ApplyPostDodgeMomentum(RE::bhkCharacterController* controller);

    // t in [0, 1] -> curve factor in [0, 1]
    // Can swap these for any curve.
    static float EvaluateDodgeCurve(float t);
    static float EvaluateFarDodgeCurve(float t);

    //Cut dodge in air. Applied in the hook directly.
    void CancelDodgeOnAir(RE::bhkCharacterController* controller);

    //Initiate the stop movement after dodge. Tick the landing lock timer and apply backup velocityMod kill.
    void StartLandingLock(RE::PlayerCharacter* player, float lockSeconds);
    void UpdateLandingLock(RE::bhkCharacterController* controller, float dt);

    //Slow player after dodge.
    void StartPostDodgeSlow(float slowSeconds);
    void StopPostDodgeSlow();
    void UpdatePostDodgeSlow(float dt);
    static void SetWantedSlowBonus(float wanted);

    // movement input lock that starts when the dodge BEGINS.
    void StartInputLock(RE::PlayerCharacter* player, float lockSeconds);
    void UpdateInputLock(float dt);

    // Stopping attack/block while dodge
    void StartAttackLock(RE::PlayerCharacter* player);
    void StopAttackLock();

    //Play sound effect
    void PlayDodgeSound(RE::BGSSoundDescriptorForm* a_form);

    //IMods for dodge
    void ApplyDodgeImod();
    void RemoveDodgeImod();

    //Screen shake for dodge
    void TriggerDodgeShake(float strength, float duration);

    // Returns true if the player is in a "committed" attack state or blocking.
    bool IsAttackOrBlockActive(RE::PlayerCharacter* player);

    // Cooldown: blocks starting a new dodge for a while after one begins.
    void StartDodgeCooldown();
    void StartFarDodgeCooldown();
    bool IsDodgeOnCooldown();
    void UpdateDodgeCooldown(float dt);
    void UpdateFarDodgeCooldown(float dt);

    //Getters for dodge stats, will check globals
    float GetEffectiveDistance();
    float GetEffectiveDuration();
    float GetEffectiveSpeed();
    float GetEffectiveStaminaCost();
    float GetResolvedStaminaCost(RE::PlayerCharacter* player); // checks stamina percent setting
    float GetEffectiveIFrameTime();
    
    //--------------------------




    //INI READING------------------------------------------------------
    // Load tunable settings (distance, duration, momentum) from INI.
    void LoadSettings();
    // Trim helpers for INI parsing
    static inline void Trim(std::string& s);

    // Central place that applies one key=value line.
    void ApplySingleSetting(const std::string& key,
        const std::string& value,
        bool& seenSpeed);

    //Refresh values from ini
    void ReloadIni();
   
    //dodge key return
    uint32_t GetDodgeKey();

    //reload key return
    uint32_t GetReloadIniKey();
    //------------------------------------------------------


    //ESP------------------------------------------------------
    void InitGlobals();
    void SetIsDodging(bool a_isDodging);
    
    // IFrame-related funcs
    // Note the perk method is not really being used, instead opting for SetGhost().
    void SetIFrameWindow(bool a_active);
    void StartIFrameWindow();
    void UpdateIFrameWindow(float dt);
    void EnsureIFramePerkOnPlayer();
    bool SetActorGhost_VM(RE::Actor* a_actor, bool a_enable); //Uses papyrus-based function for ghost


    //Dodge damage
    void EnsureDamagePerkOnPlayer();

    //------------------------------------------------------

    //DEBUG------------------------------------------------------
    static void PrintConsole(const char* fmt, ...);

    //------------------------------------------------------
   
        // ----- Table-driven config -----
    struct FloatSettingDef
    {
        const char* name;
        float* target;
    };

    using AVModifier =
        RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER;

    inline State g_playerState;

    // Defaults – will be overridden by INI if present.
    inline float g_distance = 200.0f;
    inline float g_duration = 0.75f;
    inline float g_speed = 25.0f;

    // 0.0f = full stop at the end of the dodge
    // 1.0f = keep all built-up velocity
    inline float g_postDodgeMomentumFactor = 0.0f;

    // Snapshot of movement direction when dodge key was pressed.
    inline DodgeDirection g_lastInputDirection =
        DodgeDirection::None;

    // Keymap for dodge trigger
    inline uint32_t g_dodgeKey = 0x22;

    // Keymap for reload ini
    inline uint32_t g_reloadIniKey = 0;

    // How long to prevent movement after dodge
    inline float g_landingLockSeconds = 0.0f;

    // Remember previous state of movement input
    inline bool s_prevMovementInputEnabled = true;

    // Tap-to-dodge
    inline bool g_tapDodgeOnRelease = false;
    inline float g_tapDodgeWindowSeconds = 0.0f;

    // Input lock
    inline float g_inputLockSeconds = 0.0f;
    inline bool g_inputLockActive = false;
    inline float g_inputLockRemaining = 0.0f;

    // Post-dodge slow
    inline float g_slowMult = 1.0f;
    inline float g_slowTime = 0.0f;
    inline bool g_postSlowActive = false;
    inline float g_postSlowRemaining = 0.0f;

    inline float gAppliedSlowBonus = 0.0f;
    inline float g_postSlowAppliedDelta = 0.0f;

    // Far dodge
    inline float g_farDodgeWindowSeconds = 0.0f;

    inline float g_farDodgeStaminaMult = 1.0f;
    inline float g_farDodgeSpeedMult = 1.0f;
    inline float g_farDodgeDistMult = 1.0f;
    inline float g_farDodgeDurationMult = 1.0f;

    inline float g_slowTimeFarDodge = 0.0f;

    inline float g_farDodgeInputLockSeconds = 0.0f;
    inline float g_farDodgeLandingLockSeconds = 0.0f;

    // Far dodge cooldown
    inline float g_farDodgeCooldownSeconds = 0.0f;
    inline bool g_farDodgeCooldownActive = false;
    inline float g_farDodgeCooldownRemaining = 0.0f;

    // Attacking while dodging
    inline bool g_attackWhileDodging = true;
    inline bool s_prevAttackInputEnabled = true;
    inline bool g_attackLockActive = false;

    // Outgoing damage/block during dodge
    inline bool g_dodgeDamageNull = false;

    // Able to steer dodges
    inline bool g_SteerDodge = false;

    // Additional dodge settings
    inline float g_staminaCost = 0.0f;
    inline float g_iFrameTime = 0.0f;

    // Dodge cooldown
    inline float g_dodgeCooldownSeconds = 0.0f;
    inline bool g_dodgeCooldownActive = false;
    inline float g_dodgeCooldownRemaining = 0.0f;

    // Runtime iFrame window
    inline bool g_iFrameWindowActive = false;
    inline float g_iFrameWindowRemaining = 0.0f;

    // SFX volume
    inline float g_DodgeSoundVolume = 0.75f;

    // Screenshake
    inline float g_ShakeStrength = 1.0f;
    inline float g_ShakeDuration = 0.1f;

    inline bool g_disableDodgeInThirdPerson = true;

}