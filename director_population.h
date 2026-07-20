//========= director_population.h ================================================
// CDirectorPopulation - common infected population control: how many are
// alive, where new ones spawn (nav-mesh flow aware), and panic/mob events.
//==================================================================================
#ifndef DIRECTOR_POPULATION_H
#define DIRECTOR_POPULATION_H
#ifdef _WIN32
#pragma once
#endif

#include "director_shared.h"
#include "utlvector.h"
#include "utlmap.h"

class CNavArea;
class CBaseEntity;

// One node in the flow field: a nav area plus its travel distance from
// the anchor point the field was built from.
struct FlowNode_t
{
    CNavArea *pArea;
    float     flFlow;
};

class CDirectorPopulation
{
public:
    CDirectorPopulation();
    ~CDirectorPopulation();

    void    OnLevelInit();
    void    OnLevelShutdown();
    void    Update( DirectorPhase_t nPhase, float flDt );

    // Mappers (via a logic_ entity) or the Director's own phase
    // transitions can request an immediate horde.
    void    TriggerPanicEvent( int nMobSize = -1 );

    int     GetActiveCommonCount() const { return m_ActiveCommon.Count(); }
    void    NotifyCommonInfectedRemoved( CBaseEntity *pEntity );

    // Shared with CDirectorBossDirector so Tanks/Witches spawn on the
    // same flow-aware band as common infected instead of duplicating
    // the search.
    CNavArea *FindSpawnCandidateForBoss() const { return FindSpawnCandidate( true ); }

private:
    void        BuildFlowField();
    void        FloodFlowFromArea( CNavArea *pStartArea );
    float       GetLeadSurvivorFlow() const;

    int         GetTargetPopulation( DirectorPhase_t nPhase ) const;
    CNavArea    *FindSpawnCandidate( bool bAheadOfSurvivors ) const;
    bool        IsAreaSpawnable( CNavArea *pArea, float flMinDistToPlayers ) const;
    bool        IsPositionVisibleToAnyPlayer( const Vector &vecPos ) const;

    void        SpawnCommonInfectedAt( const Vector &vecPos, const QAngle &angFacing );
    void        ProcessPanicQueue( float flDt );

    CUtlVector< CBaseEntity* >     m_ActiveCommon;
    CUtlVector< PanicEvent_t >     m_PanicQueue;
    CUtlVector< FlowNode_t >       m_FlowOrderedAreas;     // sorted ascending by flFlow
    CUtlMap< unsigned int, float > m_FlowLookup;            // nav area ID -> flow distance

    float   m_flNextSpawnTime;
    float   m_flNextPanicCheckTime;
    bool    m_bFlowFieldValid;
};

#endif // DIRECTOR_POPULATION_H
