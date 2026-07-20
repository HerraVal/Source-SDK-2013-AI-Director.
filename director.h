//========= director.h ===========================================================
// CDirector - top-level AI Director game system.
//
// Tracks how much stress the survivors are currently under, cycles
// through a four-phase pacing model based on that stress, and drives
// the population + boss subsystems to match whatever phase it's in.
//
// Independent reimplementation for a Source SDK 2013 L4D-style mod.
// Contains none of Valve's original L4D code -- the stress weights
// and thresholds below are original tuning values, not extracted
// from the real game.
//=================================================================================
#ifndef DIRECTOR_H
#define DIRECTOR_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"
#include "GameEventListener.h"
#include "director_shared.h"

class CDirectorPopulation;
class CDirectorBossDirector;
class CBasePlayer;

class CDirector : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
    CDirector();
    virtual ~CDirector();

    // CAutoGameSystemPerFrame
    virtual bool Init();
    virtual void Shutdown();
    virtual void LevelInitPostEntity();
    virtual void LevelShutdownPreEntity();
    virtual void FrameUpdatePostEntityThink();

    // CGameEventListener
    virtual void FireGameEvent( IGameEvent *event );

    float               GetPlayerStress( CBasePlayer *pPlayer ) const;
    float               GetTeamStress() const { return m_flTeamStress; }
    DirectorPhase_t     GetPhase() const { return m_nPhase; }

    // Lets subsystems or mapper-triggered logic (e.g. a scripted
    // ambush) inject stress directly instead of going through a
    // game event.
    void    AddStress( CBasePlayer *pPlayer, float flAmount );

    CDirectorPopulation     *GetPopulation()   { return m_pPopulation; }
    CDirectorBossDirector   *GetBossDirector() { return m_pBossDirector; }

private:
    void    UpdateStress( float flDt );
    void    UpdatePhase( float flDt );
    void    DecayStress( float flDt );

    float               m_flPlayerStress[ MAX_PLAYERS + 1 ];  // indexed by entindex
    float               m_flTeamStress;
    DirectorPhase_t     m_nPhase;
    float               m_flPhaseTimer;    // time spent in the current phase
    float               m_flNextTickTime;

    CDirectorPopulation     *m_pPopulation;
    CDirectorBossDirector   *m_pBossDirector;
};

extern CDirector *TheDirector();

#endif // DIRECTOR_H
