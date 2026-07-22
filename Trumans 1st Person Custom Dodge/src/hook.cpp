#include "hook.h"
#include "SimpleDodge.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>

namespace
{
    // Signature of bhkCharacterStateOnGround::SimulateStatePhysics
    using SimulateStatePhysics_t = void(RE::bhkCharacterStateOnGround*, RE::bhkCharacterController*);

    // Signature of bhkCharacterStateInAir::SimulateStatePhysics
    using SimulateStatePhysicsAir_t = void(RE::bhkCharacterStateInAir*, RE::bhkCharacterController*);

    // Pointer to the original vfuncs
    REL::Relocation<SimulateStatePhysics_t> g_originalSimulate;
    REL::Relocation<SimulateStatePhysicsAir_t>    g_originalSimulateAir;

    // Our hook – very small on purpose
    void SimulateStatePhysics_Hook(RE::bhkCharacterStateOnGround* a_this,
        RE::bhkCharacterController* a_controller)
    {
        // Let our dodge logic tweak velocityMod for the player (or do nothing)
        if (a_controller) {
            SimpleDodge::ApplyOnGround(a_controller);
        }

        // Then run the original ground physics
        g_originalSimulate(a_this, a_controller);
    }


    // In-air state hook – cancels any active dodge as soon as we're in the air
    //
    void SimulateStatePhysics_Air_Hook(RE::bhkCharacterStateInAir* a_this,
        RE::bhkCharacterController* a_controller)
    {
        if (a_controller) {
            // If the player was mid-dodge when leaving the ground, kill the dodge
            // and clear any stale forward velocityMod.
            SimpleDodge::CancelDodgeOnAir(a_controller);
        }

        // Run the original in-air physics
        g_originalSimulateAir(a_this, a_controller);
    }
}

namespace Hooks
{
    void Install()
    {
        // Hook vfunc index 8 on bhkCharacterStateOnGround's vtable,
        // same slot AMR uses for SimulateStatePhysics.
        REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_bhkCharacterStateOnGround[0] };
        g_originalSimulate = vtbl.write_vfunc(8, SimulateStatePhysics_Hook);
    
        // Also hook vfunc index 8 on bhkCharacterStateInAir's vtable
        // so we can cancel dodges while the player is airborne.
        REL::Relocation<std::uintptr_t> vtblAir{ RE::VTABLE_bhkCharacterStateInAir[0] };
        g_originalSimulateAir = vtblAir.write_vfunc(8, SimulateStatePhysics_Air_Hook);
    }
}