//========= director_bosses.h ======================================================
// CDirectorBossDirector - Tank and Witch spawn pacing.
//
// Both use a pity-timer model: a hard cooldown during which they cannot
// spawn at all, followed by a climbing probability roll each tick until
// one succeeds. That keeps timing unpredictable run-to-run while still
// guaranteeing a boss doesn't stay silent forever the way a flat
// probability roll could.
//====================================================================================
#ifndef DIRECTOR_BOSSES_H
#define DIRECTOR_BOSSES_H
#ifdef _WIN32
#pragma once
#endif

#include "director_shared.h"

class CNavArea;

class CDirectorBossDirector
{
public:
    CDirectorBossDirector();

    void    OnLevelInit();
    void    Update( DirectorPhase_t nPhase, float flDt );

    void    NotifyTankKilled();
    void    NotifyWitchKilled();

private:
    bool    TryRollTank( float flDt );
    bool    TryRollWitch( float flDt );

    void    SpawnTank();
    void    SpawnWitch();

    CNavArea *FindTankSpawnArea() const;
    CNavArea *FindWitchSpawnArea() const;

    float   m_flTimeSinceLastTank;
    float   m_flTimeSinceLastWitch;
    float   m_flTankProbabilityRamp;    // grows each tick past the cooldown
    float   m_flWitchProbabilityRamp;

    bool    m_bTankAlive;
    bool    m_bWitchAlive;
};

#endif // DIRECTOR_BOSSES_H
