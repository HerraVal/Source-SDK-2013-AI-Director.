//========= director.cpp ==========================================================
#include "cbase.h"
#include "director.h"
#include "director_population.h"
#include "director_bosses.h"
#include "player.h"

// ------------------------------------------------------------------------
// Tuning ConVars -- all FCVAR_CHEAT so they're sv_cheats gated while you
// dial in numbers. Strip that flag once you're happy with defaults you
// want live.
// ------------------------------------------------------------------------
ConVar director_tick_interval( "director_tick_interval", "0.5", FCVAR_CHEAT,
    "How often (seconds) the Director re-evaluates stress and phase." );

ConVar director_stress_decay_rate( "director_stress_decay_rate", "0.05", FCVAR_CHEAT,
    "Stress lost per second while a player isn't taking or dealing damage." );

ConVar director_stress_hurt_scale( "director_stress_hurt_scale", "0.4", FCVAR_CHEAT,
    "Stress added per point of damage taken, scaled by damage / max health." );

ConVar director_stress_kill_bonus( "director_stress_kill_bonus", "0.03", FCVAR_CHEAT,
    "Flat stress added to a player when they down a common infected." );

ConVar director_phase_buildup_time( "director_phase_buildup_time", "35", FCVAR_CHEAT,
    "Seconds to spend building intensity before forcing a peak." );

ConVar director_phase_peak_time( "director_phase_peak_time", "12", FCVAR_CHEAT,
    "Seconds to sustain a peak once team stress crosses the peak threshold." );

ConVar director_phase_relax_time( "director_phase_relax_time", "20", FCVAR_CHEAT,
    "Minimum seconds spent relaxing before the Director will build up again." );

ConVar director_stress_peak_threshold( "director_stress_peak_threshold", "0.65", FCVAR_CHEAT,
    "Team stress value (0-1) that forces a transition into sustain-peak." );


static CDirector s_Director;
CDirector *TheDirector() { return &s_Director; }

CDirector::CDirector()
    : CAutoGameSystemPerFrame( "CDirector" )
{
    m_flTeamStress   = 0.0f;
    m_nPhase         = PHASE_RELAX;
    m_flPhaseTimer   = 0.0f;
    m_flNextTickTime = 0.0f;
    m_pPopulation    = NULL;
    m_pBossDirector  = NULL;

    for ( int i = 0; i <= MAX_PLAYERS; i++ )
        m_flPlayerStress[i] = 0.0f;
}

CDirector::~CDirector()
{
}

bool CDirector::Init()
{
    ListenForGameEvent( "player_hurt" );
    ListenForGameEvent( "infected_killed" );   // custom event -- see README
    ListenForGameEvent( "player_death" );

    m_pPopulation   = new CDirectorPopulation();
    m_pBossDirector = new CDirectorBossDirector();

    return true;
}

void CDirector::Shutdown()
{
    delete m_pPopulation;
    delete m_pBossDirector;
    m_pPopulation   = NULL;
    m_pBossDirector = NULL;
}

void CDirector::LevelInitPostEntity()
{
    m_flTeamStress   = 0.0f;
    m_nPhase         = PHASE_RELAX;
    m_flPhaseTimer   = 0.0f;
    m_flNextTickTime = gpGlobals->curtime + director_tick_interval.GetFloat();

    for ( int i = 0; i <= MAX_PLAYERS; i++ )
        m_flPlayerStress[i] = 0.0f;

    // Both subsystems keep their own per-level state (population
    // counts, boss cooldowns) so they need their own reset hook here --
    // the nav mesh / entities they depend on aren't guaranteed to
    // exist any earlier than LevelInitPostEntity.
    if ( m_pPopulation )
        m_pPopulation->OnLevelInit();

    if ( m_pBossDirector )
        m_pBossDirector->OnLevelInit();
}

void CDirector::LevelShutdownPreEntity()
{
    if ( m_pPopulation )
        m_pPopulation->OnLevelShutdown();
}

void CDirector::FrameUpdatePostEntityThink()
{
    if ( gpGlobals->curtime < m_flNextTickTime )
        return;

    float flDt = director_tick_interval.GetFloat();
    m_flNextTickTime = gpGlobals->curtime + flDt;

    UpdateStress( flDt );
    UpdatePhase( flDt );

    if ( m_pPopulation )
        m_pPopulation->Update( m_nPhase, flDt );

    if ( m_pBossDirector )
        m_pBossDirector->Update( m_nPhase, flDt );
}

void CDirector::FireGameEvent( IGameEvent *event )
{
    const char *pszName = event->GetName();

    if ( FStrEq( pszName, "player_hurt" ) )
    {
        int nUserID = event->GetInt( "userid" );
        int nDamage = event->GetInt( "damage" );
        CBasePlayer *pPlayer = UTIL_PlayerByUserId( nUserID );

        if ( pPlayer )
        {
            float flMaxHealth = MAX( 1, pPlayer->GetMaxHealth() );
            float flScaled = ( nDamage / flMaxHealth ) * director_stress_hurt_scale.GetFloat();
            AddStress( pPlayer, flScaled );
        }
    }
    else if ( FStrEq( pszName, "infected_killed" ) )
    {
        int nAttackerID = event->GetInt( "attacker" );
        CBasePlayer *pAttacker = UTIL_PlayerByUserId( nAttackerID );

        if ( pAttacker )
            AddStress( pAttacker, director_stress_kill_bonus.GetFloat() );
    }
    else if ( FStrEq( pszName, "player_death" ) )
    {
        // A death is a hard spike. Flood stress high now and let the
        // normal decay curve bring the team back down smoothly rather
        // than snapping straight to relax.
        int nUserID = event->GetInt( "userid" );
        CBasePlayer *pPlayer = UTIL_PlayerByUserId( nUserID );

        if ( pPlayer )
            AddStress( pPlayer, 1.0f );
    }
}

void CDirector::AddStress( CBasePlayer *pPlayer, float flAmount )
{
    if ( !pPlayer )
        return;

    int nIndex = pPlayer->entindex();
    if ( nIndex < 0 || nIndex > MAX_PLAYERS )
        return;

    m_flPlayerStress[ nIndex ] = clamp( m_flPlayerStress[ nIndex ] + flAmount, 0.0f, 1.0f );
}

float CDirector::GetPlayerStress( CBasePlayer *pPlayer ) const
{
    if ( !pPlayer )
        return 0.0f;

    int nIndex = pPlayer->entindex();
    if ( nIndex < 0 || nIndex > MAX_PLAYERS )
        return 0.0f;

    return m_flPlayerStress[ nIndex ];
}

void CDirector::UpdateStress( float flDt )
{
    DecayStress( flDt );

    // Team stress = the single most-stressed connected, living player.
    // One survivor getting swarmed should be enough to hold off a
    // peak fade even if everyone else is fine -- the Director should
    // always be reacting to whoever is having the worst time right now.
    float flHighest = 0.0f;

    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
        if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->IsAlive() )
            continue;

        flHighest = MAX( flHighest, m_flPlayerStress[i] );
    }

    m_flTeamStress = flHighest;
}

void CDirector::DecayStress( float flDt )
{
    float flDecay = director_stress_decay_rate.GetFloat() * flDt;

    for ( int i = 0; i <= MAX_PLAYERS; i++ )
        m_flPlayerStress[i] = MAX( 0.0f, m_flPlayerStress[i] - flDecay );
}

void CDirector::UpdatePhase( float flDt )
{
    m_flPhaseTimer += flDt;

    switch ( m_nPhase )
    {
    case PHASE_BUILDUP:
        if ( m_flTeamStress >= director_stress_peak_threshold.GetFloat()
          || m_flPhaseTimer >= director_phase_buildup_time.GetFloat() )
        {
            m_nPhase = PHASE_SUSTAIN_PEAK;
            m_flPhaseTimer = 0.0f;

            if ( m_pPopulation )
                m_pPopulation->TriggerPanicEvent();
        }
        break;

    case PHASE_SUSTAIN_PEAK:
        if ( m_flPhaseTimer >= director_phase_peak_time.GetFloat() )
        {
            m_nPhase = PHASE_PEAK_FADE;
            m_flPhaseTimer = 0.0f;
        }
        break;

    case PHASE_PEAK_FADE:
        // Wait for stress to actually come back down rather than just
        // a timer -- otherwise players can get dumped into relax
        // while they're still mid-fight.
        if ( m_flTeamStress < director_stress_peak_threshold.GetFloat() * 0.5f )
        {
            m_nPhase = PHASE_RELAX;
            m_flPhaseTimer = 0.0f;
        }
        break;

    case PHASE_RELAX:
        if ( m_flPhaseTimer >= director_phase_relax_time.GetFloat() )
        {
            m_nPhase = PHASE_BUILDUP;
            m_flPhaseTimer = 0.0f;
        }
        break;
    }
}
