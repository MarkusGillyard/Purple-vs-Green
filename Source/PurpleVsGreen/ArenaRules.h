#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace PvG
{
enum class Team : std::uint8_t { Purple, Green };
enum class Spell : std::uint8_t { Thread, Fan, Lance, Comet, Count };
enum class Phase : std::uint8_t { Menu, Playing, Results };

struct SpellSpec
{
    const char* Name;
    float Damage;
    float Range;
    float Cooldown;
    float Cost;
    float Windup;
    int Rays;
    float SpreadDegrees;
    float ProjectileSpeed;
    float BlastRadius;
};

inline constexpr std::array<SpellSpec, 4> Arsenal{{
    {"Witchthread", 14.f, 3600.f, 0.19f, 5.f, 0.00f, 1, 0.45f, 0.f, 0.f},
    {"Briar Fan",    11.f,  870.f, 0.65f, 18.f, 0.05f, 7, 8.5f,  0.f, 0.f},
    {"Glass Lance",  57.f, 7600.f, 0.95f, 23.f, 0.27f, 1, 0.f,   0.f, 0.f},
    {"Cinder Comet", 64.f, 4300.f, 1.10f, 29.f, 0.12f, 1, 0.f, 1550.f, 340.f},
}};
inline constexpr float FullHealth = 100.f;
inline constexpr float FullFocus = 100.f;
inline constexpr float FocusPerSecond = 21.f;
inline constexpr float RespawnDelay = 2.5f;

constexpr int Index(Team Value) { return static_cast<int>(Value); }
constexpr int Index(Spell Value) { return static_cast<int>(Value); }
constexpr Team EnemyOf(Team Value) { return Value == Team::Purple ? Team::Green : Team::Purple; }

struct SoldierRules
{
    float Health = FullHealth;
    float Focus = FullFocus;
    double ReadyAt = 0.;
    double WardUntil = 0.;
    double WardReadyAt = 0.;
    double ProtectedUntil = 0.;
    Spell ActiveSpell = Spell::Thread;
    bool Charging = false;

    bool Alive() const { return Health > 0.f; }
    void Refill(float Delta) { Focus = std::min(FullFocus, Focus + Delta * FocusPerSecond); }

    bool StartCast(Spell Selected, double Now, double& ReleaseAt)
    {
        const int Slot = Index(Selected);
        if (!Alive() || Charging || Now < ReadyAt || Slot >= Index(Spell::Count))
            return false;
        const SpellSpec& Spec = Arsenal[Slot];
        if (Focus < Spec.Cost)
            return false;
        Focus -= Spec.Cost;
        ReleaseAt = Now + Spec.Windup;
        ReadyAt = ReleaseAt + Spec.Cooldown;
        ActiveSpell = Selected;
        Charging = true;
        return true;
    }

    bool Ward(double Now)
    {
        constexpr float Cost = 28.f;
        if (!Alive() || Charging || Focus < Cost || Now < WardReadyAt)
            return false;
        Focus -= Cost;
        WardUntil = Now + 1.65;
        WardReadyAt = Now + 5.;
        return true;
    }

    float Hit(Team Own, Team Attacker, float Damage, double Now)
    {
        if (!Alive() || Own == Attacker || Damage <= 0.f || Now < ProtectedUntil)
            return 0.f;
        const float Applied = std::min(Health, Damage * (Now < WardUntil ? 0.4f : 1.f));
        Health -= Applied;
        if (!Alive())
            Charging = false;
        return Applied;
    }
};

struct MatchRules
{
    Phase State = Phase::Menu;
    std::array<int, 2> Score{};
    int Limit = 20;
    double EndsAt = 0.;

    void Start(int Target, double Duration, double Now)
    {
        Score = {};
        Limit = std::max(1, Target);
        EndsAt = Now + Duration;
        State = Phase::Playing;
    }

    bool Eliminate(Team Killer, Team Defeated)
    {
        if (State != Phase::Playing || Killer == Defeated)
            return false;
        if (++Score[Index(Killer)] >= Limit)
            State = Phase::Results;
        return true;
    }

    void Advance(double Now)
    {
        if (State == Phase::Playing && Now >= EndsAt)
            State = Phase::Results;
    }
};
}
