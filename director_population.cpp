//========= director_population.cpp ===============================================
#include "cbase.h"
#include "director_population.h"
#include "director.h"
#include "player.h"
#include "nav_mesh.h"
#include "nav_area.h"
#include <algorithm>

// ------------------------------------------------------------------------
// NOTE ON NAV MESH CALLS:
// The flow-field code below leans on CNavArea::GetAdjacentAreas() and the
// NavConnect{ area, length } pairing, which has held stable across most
// Source engine nav mesh branches (it's the same pattern the stock bot
// nav code uses). If FloodFlowFromArea() doesn't compile out of the box,
// this is the one section to check against your exact nav_area.h.
// ------------------------------------------------------------------------

ConVar director_common_max_active( "director_common_max_active", "30", FCVAR_CHEAT,
    "Hard ceiling on simultaneously alive common infected." );

ConVar director_common_target_relax( "director_common_target_relax", "6", FCVAR_CHEAT,
    "Target common infected population during PHASE_RELAX." );

ConVar director_common_target_buildup( "director_common_target_buildup", "14", FCVAR_CHEAT,
    "Target common infected population during PHASE_BUILDUP." );

ConVar director_common_target_peak( "director_common_target_peak", "24", FCVAR_CHEAT,
    "Target common infected population during PHASE_SUSTAIN_PEAK." );

ConVar director_spawn_min_dist( "director_spawn_min_dist", "600", FCVAR_CHEAT,
    "Minimum distance (units) a spawn point must be from every player." );

ConVar director_spawn_max_dist( "director_spawn_max_dist", "2000", FCVAR_CHEAT,
    "Maximum distance (units) ahead of the lead survivor to consider for spawning." );

ConVar director_panic_mob_size( "director_panic_mob_size", "20", FCVAR_CHEAT,
    "Default number of common infected in a panic/mob event." );

ConVar director_panic_min_interval( "director_panic_min_interval", "45", FCVAR_CHEAT,
    "Minimum seconds between panic events." );


CDirectorPopulation::CDirectorPopulation()
    : m_FlowLookup( DefLessFunc( unsigned int ) )
{
    m_flNextSpawnTime      = 0.0f;
    m_flNextPanicCheckTime = 0.0f;
    m_bFlowFieldValid      = false;
}

CDirectorPopulation::~CDirectorPopulation()
{
}

void CDirectorPopulation::OnLevelInit()
{
    m_ActiveCommon.Purge();
    m_PanicQueue.Purge();
    m_FlowOrderedAreas.Purge();
    m_FlowLookup.RemoveAll();

    m_bFlowFieldValid      = false;    // rebuilt lazily once a player exists
    m_flNextSpawnTime      = 0.0f;
    m_flNextPanicCheckTime = gpGlobals->curtime + director_panic_min_interval.GetFloat();
}

void CDirectorPopulation::OnLevelShutdown()
{
    m_ActiveCommon.Purge();
    m_PanicQueue.Purge();
}

void CDirectorPopulation::Update( DirectorPhase_t nPhase, float flDt )
{
    if ( !m_bFlowFieldValid )
        BuildFlowField();

    // Entities should call NotifyCommonInfectedRemoved() on death, but
    // prune stale/null pointers defensively in case one slips through.
    for ( int i = m_ActiveCommon.Count() - 1; i >= 0; i-- )
    {
        if ( !m_ActiveCommon[i] )
            m_ActiveCommon.Remove(i);
    }

    ProcessPanicQueue( flDt );

    if ( gpGlobals->curtime < m_flNextSpawnTime )
        return;

    m_flNextSpawnTime = gpGlobals->curtime + 1.0f;     // trickle-spawn check rate

    int nTarget = GetTargetPopulation( nPhase );
    if ( m_ActiveCommon.Count() >= nTarget )
        return;

    if ( m_ActiveCommon.Count() >= director_common_max_active.GetInt() )
        return;

    CNavArea *pArea = FindSpawnCandidate( true );
    if ( pArea )
    {
        Vector vecSpawn = pArea->GetRandomPoint();
        QAngle angFacing( 0, RandomFloat( 0, 360 ), 0 );
        SpawnCommonInfectedAt( vecSpawn, angFacing );
    }
}

int CDirectorPopulation::GetTargetPopulation( DirectorPhase_t nPhase ) const
{
    switch ( nPhase )
    {
    case PHASE_BUILDUP:      return director_common_target_buildup.GetInt();
    case PHASE_SUSTAIN_PEAK: return director_common_target_peak.GetInt();
    case PHASE_PEAK_FADE:    return ( director_common_target_buildup.GetInt() + director_common_target_relax.GetInt() ) / 2;
    case PHASE_RELAX:
    default:                 return director_common_target_relax.GetInt();
    }
}

void CDirectorPopulation::BuildFlowField()
{
    CBasePlayer *pAnchor = NULL;

    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
        if ( pPlayer && pPlayer->IsConnected() )
        {
            pAnchor = pPlayer;
            break;
        }
    }

    if ( !pAnchor )
        return;     // try again next Update() once someone has spawned in

    CNavArea *pStartArea = TheNavMesh->GetNavArea( pAnchor->GetAbsOrigin() );
    if ( !pStartArea )
        return;

    FloodFlowFromArea( pStartArea );
    m_bFlowFieldValid = true;
}

void CDirectorPopulation::FloodFlowFromArea( CNavArea *pStartArea )
{
    m_FlowLookup.RemoveAll();
    m_FlowOrderedAreas.Purge();

    CUtlVector< CNavArea* > openList;
    openList.AddToTail( pStartArea );
    m_FlowLookup.Insert( pStartArea->GetID(), 0.0f );

    int nHead = 0;
    while ( nHead < openList.Count() )
    {
        CNavArea *pCurrent = openList[ nHead++ ];

        int nCurIdx = m_FlowLookup.Find( pCurrent->GetID() );
        float flCurrentFlow = m_FlowLookup[ nCurIdx ];

        for ( int dir = 0; dir < 4; dir++ )
        {
            const NavConnectVector *pAdjacent = pCurrent->GetAdjacentAreas( (NavDirType)dir );
            if ( !pAdjacent )
                continue;

            for ( int j = 0; j < pAdjacent->Count(); j++ )
            {
                CNavArea *pNext = pAdjacent->Element(j).area;
                if ( !pNext || m_FlowLookup.Find( pNext->GetID() ) != m_FlowLookup.InvalidIndex() )
                    continue;

                float flStep = ( pNext->GetCenter() - pCurrent->GetCenter() ).Length();
                m_FlowLookup.Insert( pNext->GetID(), flCurrentFlow + flStep );
                openList.AddToTail( pNext );
            }
        }
    }

    for ( int i = 0; i < openList.Count(); i++ )
    {
        FlowNode_t node;
        node.pArea  = openList[i];
        node.flFlow = m_FlowLookup[ m_FlowLookup.Find( node.pArea->GetID() ) ];
        m_FlowOrderedAreas.AddToTail( node );
    }

    std::sort( m_FlowOrderedAreas.Base(), m_FlowOrderedAreas.Base() + m_FlowOrderedAreas.Count(),
        []( const FlowNode_t &a, const FlowNode_t &b ) { return a.flFlow < b.flFlow; } );
}

float CDirectorPopulation::GetLeadSurvivorFlow() const
{
    float flHighest = 0.0f;

    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
        if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->IsAlive() )
            continue;

        CNavArea *pArea = TheNavMesh->GetNavArea( pPlayer->GetAbsOrigin() );
        if ( !pArea )
            continue;

        int nIdx = m_FlowLookup.Find( pArea->GetID() );
        if ( nIdx != m_FlowLookup.InvalidIndex() )
            flHighest = MAX( flHighest, m_FlowLookup[nIdx] );
    }

    return flHighest;
}

CNavArea *CDirectorPopulation::FindSpawnCandidate( bool bAheadOfSurvivors ) const
{
    if ( m_FlowOrderedAreas.Count() == 0 )
        return NULL;

    float flLead = GetLeadSurvivorFlow();

    float flBandMin = bAheadOfSurvivors ? flLead + director_spawn_min_dist.GetFloat()
                                         : MAX( 0.0f, flLead - director_spawn_max_dist.GetFloat() );
    float flBandMax = bAheadOfSurvivors ? flLead + director_spawn_max_dist.GetFloat()
                                         : MAX( 0.0f, flLead - director_spawn_min_dist.GetFloat() );

    // Binary search the sorted flow array for the first node inside the
    // band, then gather every candidate in range before picking one at
    // random -- keeps spawns from clustering at the near edge of the
    // band every time.
    int nLow = 0, nHigh = m_FlowOrderedAreas.Count() - 1, nStart = -1;
    while ( nLow <= nHigh )
    {
        int nMid = ( nLow + nHigh ) / 2;
        if ( m_FlowOrderedAreas[nMid].flFlow >= flBandMin )
        {
            nStart = nMid;
            nHigh = nMid - 1;
        }
        else
        {
            nLow = nMid + 1;
        }
    }

    if ( nStart < 0 )
        return NULL;

    CUtlVector< CNavArea* > candidates;
    for ( int i = nStart; i < m_FlowOrderedAreas.Count(); i++ )
    {
        if ( m_FlowOrderedAreas[i].flFlow > flBandMax )
            break;

        if ( IsAreaSpawnable( m_FlowOrderedAreas[i].pArea, director_spawn_min_dist.GetFloat() ) )
            candidates.AddToTail( m_FlowOrderedAreas[i].pArea );
    }

    if ( candidates.Count() == 0 )
        return NULL;

    return candidates[ RandomInt( 0, candidates.Count() - 1 ) ];
}

bool CDirectorPopulation::IsAreaSpawnable( CNavArea *pArea, float flMinDistToPlayers ) const
{
    if ( !pArea )
        return false;

    Vector vecCenter = pArea->GetCenter();

    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
        if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->IsAlive() )
            continue;

        if ( ( pPlayer->GetAbsOrigin() - vecCenter ).Length() < flMinDistToPlayers )
            return false;
    }

    // The Director's golden rule: never spawn somewhere a survivor is
    // currently looking. Nothing breaks the illusion faster than a
    // zombie popping into existence in plain sight.
    if ( IsPositionVisibleToAnyPlayer( vecCenter ) )
        return false;

    return true;
}

bool CDirectorPopulation::IsPositionVisibleToAnyPlayer( const Vector &vecPos ) const
{
    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
        if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->IsAlive() )
            continue;

        trace_t tr;
        UTIL_TraceLine( pPlayer->EyePosition(), vecPos, MASK_OPAQUE, pPlayer, COLLISION_GROUP_NONE, &tr );

        if ( tr.fraction >= 1.0f )
            return true;    // clear line of sight -- disqualified
    }

    return false;
}

void CDirectorPopulation::SpawnCommonInfectedAt( const Vector &vecPos, const QAngle &angFacing )
{
    // ------------------------------------------------------------------
    // INTEGRATION POINT: swap "infected_common" for whatever classname
    // your common infected entity is registered under (LINK_ENTITY_TO_
    // CLASS), and adjust the cast if you need your concrete type here
    // rather than just a CBaseEntity* handle.
    // ------------------------------------------------------------------
    CBaseEntity *pInfected = CreateEntityByName( "infected_common" );
    if ( !pInfected )
        return;

    pInfected->SetAbsOrigin( vecPos );
    pInfected->SetAbsAngles( angFacing );
    DispatchSpawn( pInfected );

    m_ActiveCommon.AddToTail( pInfected );
}

void CDirectorPopulation::NotifyCommonInfectedRemoved( CBaseEntity *pEntity )
{
    m_ActiveCommon.FindAndRemove( pEntity );
}

void CDirectorPopulation::TriggerPanicEvent( int nMobSize )
{
    if ( gpGlobals->curtime < m_flNextPanicCheckTime )
        return;

    CNavArea *pArea = FindSpawnCandidate( true );
    if ( !pArea )
        return;

    PanicEvent_t event;
    event.vecOrigin    = pArea->GetCenter();
    event.nMobSize     = ( nMobSize > 0 ) ? nMobSize : director_panic_mob_size.GetInt();
    event.flTimeQueued = gpGlobals->curtime;

    m_PanicQueue.AddToTail( event );
    m_flNextPanicCheckTime = gpGlobals->curtime + director_panic_min_interval.GetFloat();
}

void CDirectorPopulation::ProcessPanicQueue( float flDt )
{
    // Trickle mobs out a few at a time instead of dumping them all in
    // one frame -- avoids a hitch and reads as a horde pouring in
    // rather than a wall popping into existence.
    const int MOBS_PER_TICK = 3;

    for ( int i = m_PanicQueue.Count() - 1; i >= 0; i-- )
    {
        PanicEvent_t &event = m_PanicQueue[i];

        int nToSpawn = MIN( MOBS_PER_TICK, event.nMobSize );
        for ( int n = 0; n < nToSpawn; n++ )
        {
            Vector vecOffset( RandomFloat( -150, 150 ), RandomFloat( -150, 150 ), 0 );
            QAngle angFacing( 0, RandomFloat( 0, 360 ), 0 );
            SpawnCommonInfectedAt( event.vecOrigin + vecOffset, angFacing );
        }

        event.nMobSize -= nToSpawn;
        if ( event.nMobSize <= 0 )
            m_PanicQueue.Remove(i);
    }
}
