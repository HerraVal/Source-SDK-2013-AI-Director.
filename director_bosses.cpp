//========= director_bosses.cpp ====================================================
#include "cbase.h"
#include "director_bosses.h"
#include "director.h"
#include "director_population.h"
#include "nav_area.h"

ConVar director_tank_min_cooldown( "director_tank_min_cooldown", "120", FCVAR_CHEAT,
    "Seconds after a Tank dies (or level start) before another can spawn." );

ConVar director_tank_ramp_rate( "director_tank_ramp_rate", "0.01", FCVAR_CHEAT,
    "Spawn probability added per second once past the cooldown." );

ConVar director_witch_min_cooldown( "director_witch_min_cooldown", "90", FCVAR_CHEAT,
    "Seconds after a Witch dies (or level start) before another can spawn." );

ConVar director_witch_ramp_rate( "director_witch_ramp_rate", "0.015", FCVAR_CHEAT,
    "Spawn probability added per second once past the cooldown." );


CDirectorBossDirector::CDirectorBossDirector()
{
    m_flTimeSinceLastTank    = 0.0f;
    m_flTimeSinceLastWitch   = 0.0f;
    m_flTankProbabilityRamp  = 0.0f;
    m_flWitchProbabilityRamp = 0.0f;
    m_bTankAlive             = false;
    m_bWitchAlive            = false;
}

void CDirectorBossDirector::OnLevelInit()
{
    m_flTimeSinceLastTank    = 0.0f;
    m_flTimeSinceLastWitch   = 0.0f;
    m_flTankProbabilityRamp  = 0.0f;
    m_flWitchProbabilityRamp = 0.0f;
    m_bTankAlive             = false;
    m_bWitchAlive            = false;
}

void CDirectorBossDirector::Update( DirectorPhase_t nPhase, float flDt )
{
    m_flTimeSinceLastTank  += flDt;
    m_flTimeSinceLastWitch += flDt;

    // Never stack a Tank fight on an existing one, and hold off rolling
    // a new one during pure relax -- let the last crisis actually
    // resolve before starting the next.
    if ( !m_bTankAlive && nPhase != PHASE_RELAX )
        TryRollTank( flDt );

    if ( !m_bWitchAlive )
        TryRollWitch( flDt );
}

bool CDirectorBossDirector::TryRollTank( float flDt )
{
    if ( m_flTimeSinceLastTank < director_tank_min_cooldown.GetFloat() )
        return false;

    m_flTankProbabilityRamp += director_tank_ramp_rate.GetFloat() * flDt;

    if ( RandomFloat( 0.0f, 1.0f ) <= m_flTankProbabilityRamp )
    {
        SpawnTank();
        return true;
    }

    return false;
}

bool CDirectorBossDirector::TryRollWitch( float flDt )
{
    if ( m_flTimeSinceLastWitch < director_witch_min_cooldown.GetFloat() )
        return false;

    m_flWitchProbabilityRamp += director_witch_ramp_rate.GetFloat() * flDt;

    if ( RandomFloat( 0.0f, 1.0f ) <= m_flWitchProbabilityRamp )
    {
        SpawnWitch();
        return true;
    }

    return false;
}

void CDirectorBossDirector::SpawnTank()
{
    CNavArea *pArea = FindTankSpawnArea();
    if ( !pArea )
        return;

    // INTEGRATION POINT: replace with your Tank entity's classname.
    CBaseEntity *pTank = CreateEntityByName( "infected_tank" );
    if ( !pTank )
        return;

    pTank->SetAbsOrigin( pArea->GetCenter() );
    DispatchSpawn( pTank );

    m_bTankAlive             = true;
    m_flTimeSinceLastTank    = 0.0f;
    m_flTankProbabilityRamp  = 0.0f;
}

void CDirectorBossDirector::SpawnWitch()
{
    CNavArea *pArea = FindWitchSpawnArea();
    if ( !pArea )
        return;

    // INTEGRATION POINT: replace with your Witch entity's classname.
    CBaseEntity *pWitch = CreateEntityByName( "infected_witch" );
    if ( !pWitch )
        return;

    pWitch->SetAbsOrigin( pArea->GetCenter() );
    DispatchSpawn( pWitch );

    m_bWitchAlive             = true;
    m_flTimeSinceLastWitch    = 0.0f;
    m_flWitchProbabilityRamp  = 0.0f;
}

CNavArea *CDirectorBossDirector::FindTankSpawnArea() const
{
    if ( !TheDirector()->GetPopulation() )
        return NULL;

    // Reuse the population system's flow-aware search so bosses land
    // ahead of the group on the critical path instead of off in some
    // side room the flow field never touches.
    return TheDirector()->GetPopulation()->FindSpawnCandidateForBoss();
}

CNavArea *CDirectorBossDirector::FindWitchSpawnArea() const
{
    if ( !TheDirector()->GetPopulation() )
        return NULL;

    return TheDirector()->GetPopulation()->FindSpawnCandidateForBoss();
}

void CDirectorBossDirector::NotifyTankKilled()
{
    m_bTankAlive = false;
}

void CDirectorBossDirector::NotifyWitchKilled()
{
    m_bWitchAlive = false;
}
