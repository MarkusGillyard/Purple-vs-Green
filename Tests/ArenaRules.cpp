#include "ArenaRules.h"
#include "CastPose.h"
#include "KayKitCastMotion.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using namespace PvG;
    SoldierRules Soldier;
    double Release = -1.;
    assert(!Soldier.StartCast(Spell::Count, 0., Release));
    assert(Soldier.StartCast(Spell::Lance, 0., Release));
    assert(Release > 0.25 && Release < 0.30);
    assert(Soldier.Focus == 77.f);
    assert(!Soldier.StartCast(Spell::Thread, 0.1, Release));
    assert(!Soldier.Ward(0.1));
    Soldier.Charging = false;
    assert(!Soldier.StartCast(Spell::Thread, 1., Release));
    assert(Soldier.Ward(1.));
    assert(!Soldier.Ward(1.1));
    assert(Soldier.Hit(Team::Green, Team::Green, 100.f, 1.2) == 0.f);
    assert(Soldier.Hit(Team::Green, Team::Purple, 50.f, 1.2) == 20.f);
    Soldier.ProtectedUntil = 3.;
    assert(Soldier.Hit(Team::Green, Team::Purple, 100.f, 2.9) == 0.f);
    assert(Soldier.Hit(Team::Green, Team::Purple, 100.f, 3.) == 80.f);
    assert(!Soldier.StartCast(Spell::Thread, 4., Release));
    assert(!Soldier.Ward(8.));
    assert(Soldier.Hit(Team::Green, Team::Purple, 100.f, 8.) == 0.f);

    Soldier = {};
    Soldier.Focus = 4.f;
    assert(!Soldier.StartCast(Spell::Thread, 0., Release));
    Soldier.Refill(1.f);
    assert(Soldier.Focus == 25.f);
    Soldier.Refill(30.f);
    assert(Soldier.Focus == FullFocus);

    for (const Spell Active : {Spell::Thread, Spell::Fan, Spell::Lance, Spell::Comet})
    {
        const float Windup = Arsenal[Index(Active)].Windup;
        const CastPose Before = SampleCast(Active, true, -0.00001);
        const CastPose AtRelease = SampleCast(Active, false, 0.);
        assert(SampleCast(Active, true, -Windup).Gather == 0.f);
        assert(std::abs(Before.Gather - AtRelease.Gather) < 0.001f);
        assert(std::abs(Before.Phase - AtRelease.Phase) < 0.001f);
        assert(AtRelease.Release == 0.f);
        assert(SampleCast(Active, false, .03).Release > 0.f);
        const CastPose Settled = SampleCast(Active, false, Arsenal[Index(Active)].Cooldown);
        assert(Settled.Gather == 0.f && Settled.Release == 0.f && Settled.Phase == 0.f);
    }
    for (const auto* Samples : {KayKit::Shoot, KayKit::Spellcasting, KayKit::SpellcastingLong, KayKit::Raise})
        for (const int Index : {0, KayKit::CastSampleCount - 1})
            assert(Samples[Index].Lift == 0.f && Samples[Index].Reach == 0.f && Samples[Index].Sweep == 0.f);

    MatchRules Match;
    assert(!Match.Eliminate(Team::Purple, Team::Green));
    Match.Start(2, 60., 20.);
    assert(!Match.Eliminate(Team::Green, Team::Green));
    assert(Match.Eliminate(Team::Green, Team::Purple));
    assert(Match.State == Phase::Playing);
    assert(Match.Eliminate(Team::Green, Team::Purple));
    assert(Match.State == Phase::Results);
    assert(!Match.Eliminate(Team::Purple, Team::Green));
    Match.Start(2, 60., 100.);
    assert((Match.Score == std::array<int, 2>{}));
    Match.Advance(159.99);
    assert(Match.State == Phase::Playing);
    Match.Advance(160.);
    assert(Match.State == Phase::Results);
    std::cout << "Arena rules passed\n";
}
