# Source-SDK-2013-AI-Director.
AI director in source sdk 2013



L4D-style AI Director — Source SDK 2013
A from-scratch reimplementation of the core Director loop: per-player
stress tracking, a four-phase pacing cycle (buildup / sustain peak /
peak fade / relax), nav-mesh-flow-aware common infected population
control, panic ("mob") events, and pity-timer Tank/Witch spawning.
This contains none of Valve's original L4D code. The exact stress
weights, phase durations, and probability curves were never published,
so every number here is an original tuning starting point exposed as
a ConVar — expect to spend time in-game adjusting them.
Files
File
Purpose
director_shared.h
Shared enums/structs
director.h / .cpp
CDirector — stress tracking + phase state machine, top-level tick
director_population.h / .cpp
CDirectorPopulation — nav-mesh flow field, spawn point selection, common infected population, panic events
director_bosses.h / .cpp
CDirectorBossDirector — Tank/Witch pity-timer spawn logic
Integration points
Entity classnames. SpawnCommonInfectedAt(), SpawnTank(), and
SpawnWitch() call CreateEntityByName() with placeholder
classnames (infected_common, infected_tank, infected_witch).
Swap these for whatever your entities are actually registered as
via LINK_ENTITY_TO_CLASS.
Death notifications. When a common infected, Tank, or Witch is
removed, call back so population counts stay accurate:
Cpp
The cleanest spot is wherever your entity's death path already lives.
Custom game event. CDirector::FireGameEvent listens for an
infected_killed event (with an attacker userid key) that isn't
part of stock HL2 — fire it from your common infected's death code,
or remove that listener and drive kill-stress a different way.
VPC. Add the files to your mod's server VPC script:
Code
Known caveat
director_population.cpp's flow field leans on
CNavArea::GetAdjacentAreas() and the NavConnect::area pairing. That
shape has held stable across most Source engine nav mesh branches, but
if FloodFlowFromArea() doesn't compile out of the box, it's the one
function to check against your exact nav_area.h.
Not included (yet)
Item/weapon placement pacing isn't covered here — it's a smaller
system than population/bosses and slots in cleanly as a follow-up if
you want it.
Most-used tuning ConVars
All prefixed director_, all FCVAR_CHEAT for now. Grep ConVar director_ across the three .cpp files for the full list with
descriptions.
director_stress_peak_threshold — how much stress forces a peak
director_common_target_peak / _buildup / _relax — population targets per phase
director_tank_min_cooldown / director_witch_min_cooldown — hard floors before a boss can roll