//========= director_shared.h ===================================================
// Shared enums and structs used across the AI Director subsystem.
//
// NOTE: this header assumes cbase.h has already been included by the
// translation unit that pulls it in (true for every .cpp in this
// module) so Vector/QAngle are already visible -- matches the usual
// Source SDK convention of cbase.h being the first include everywhere.
//=================================================================================
#ifndef DIRECTOR_SHARED_H
#define DIRECTOR_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// The Director's pacing model: survivors cycle through a build up,
// a sustained peak, a fade back down, and a relax/breather stretch,
// then repeat. This is an original interpretation of that idea, not
// Valve's exact formula, which was never published -- tune the
// ConVars in director.cpp / director_population.cpp / director_bosses.cpp
// to get the feel you want.
enum DirectorPhase_t
{
    PHASE_BUILDUP = 0,
    PHASE_SUSTAIN_PEAK,
    PHASE_PEAK_FADE,
    PHASE_RELAX,

    PHASE_COUNT
};

// A queued panic/mob event, trickle-spawned over a couple seconds
// rather than dumped in on a single frame.
struct PanicEvent_t
{
    Vector  vecOrigin;
    int     nMobSize;
    float   flTimeQueued;
};

#endif // DIRECTOR_SHARED_H
