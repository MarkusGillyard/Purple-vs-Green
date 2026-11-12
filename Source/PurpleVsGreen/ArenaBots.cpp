#include "ArenaMatch.h"

#include "ArenaSoldier.h"

bool AArenaMatch::Walkable(FVector From, FVector To) const
{
    FHitResult Hit;
    return !GetWorld()->SweepSingleByObjectType(Hit, From, To, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(34.f));
}

void AArenaMatch::BuildRoutes()
{
    constexpr int Step = 390;
    for (int X = -7; X <= 7; ++X)
    {
        for (int Y = -5; Y <= 5; ++Y)
        {
            const FVector Point(X * Step, Y * Step, FloorCenter);
            if (Walkable(Point, Point + FVector(0, 0, 1)))
                Nodes.Add(Point);
        }
    }
    Links.SetNum(Nodes.Num());
    for (int From = 0; From < Nodes.Num(); ++From)
    {
        for (int To = From + 1; To < Nodes.Num(); ++To)
        {
            if (FVector::DistSquared(Nodes[From], Nodes[To]) > FMath::Square(Step * 1.43) ||
                !Walkable(Nodes[From], Nodes[To]))
                continue;
            Links[From].Add(To);
            Links[To].Add(From);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("PvG arena navigation: %d nodes"), Nodes.Num());
}

int AArenaMatch::NearestNode(FVector Point, bool RequireLine) const
{
    int Result = INDEX_NONE;
    double Closest = TNumericLimits<double>::Max();
    Point.Z = FloorCenter;
    for (int Index = 0; Index < Nodes.Num(); ++Index)
    {
        const double Distance = FVector::DistSquared(Point, Nodes[Index]);
        if (Distance < Closest && (!RequireLine || Walkable(Point, Nodes[Index])))
        {
            Closest = Distance;
            Result = Index;
        }
    }
    return Result;
}

TArray<int> AArenaMatch::FindRoute(int From, int To) const
{
    if (From == INDEX_NONE || To == INDEX_NONE)
        return {};
    TArray<int> Previous;
    Previous.Init(INDEX_NONE, Nodes.Num());
    Previous[From] = From;
    TArray<int> Frontier{From};
    for (int Head = 0; Head < Frontier.Num() && Previous[To] == INDEX_NONE; ++Head)
    {
        const int Node = Frontier[Head];
        for (const int Neighbor : Links[Node])
        {
            if (Previous[Neighbor] == INDEX_NONE)
            {
                Previous[Neighbor] = Node;
                Frontier.Add(Neighbor);
            }
        }
    }

    TArray<int> Route;
    if (Previous[To] != INDEX_NONE)
    {
        for (int Node = To; Node != From; Node = Previous[Node])
            Route.Add(Node);
        Route.Add(From);
    }
    return Route;
}

void AArenaMatch::Think(AArenaSoldier* Soldier, FArenaBot& Brain, double Now)
{
    constexpr double Reaction[] = {.44, .25, .14};
    const FVector Position = Soldier->GetActorLocation();
    const FVector Eye = Soldier->Eye();
    AArenaSoldier* Visible = nullptr;
    double Best = 6500.;
    for (AArenaSoldier* Other : Soldiers)
    {
        if (!Other->Rules.Alive() || Other->Team == Soldier->Team)
            continue;
        const FVector Direction = Other->GetActorLocation() - Eye;
        const double Range = Direction.Size();
        if (Range > 5200. || (Range > 760. && FVector::DotProduct(Direction.GetSafeNormal(),
            Soldier->GetActorForwardVector()) < -.19))
            continue;
        FHitResult Hit;
        const FVector Chest = Other->GetActorLocation() + FVector(0, 0, 28);
        if (!Trace(Eye, Chest, Soldier, Hit) || Hit.GetActor() != Other)
            continue;
        const double Priority = Range * (Other == Brain.Target.Get() ? .72 : 1.);
        if (Priority < Best)
        {
            Best = Priority;
            Visible = Other;
        }
    }

    Brain.Visible = Visible != nullptr;
    FVector Goal = Brain.Destination;
    if (Visible)
    {
        Brain.Patrolling = false;
        if (Brain.Target.Get() != Visible || Now > Brain.SeesUntil)
            Brain.FireAt = Now + Reaction[Difficulty] + Random.FRandRange(0.f, .14f);
        Brain.Target = Visible;
        Brain.LastSeen = Visible->GetActorLocation();
        Brain.SeesUntil = Now + 2.;
        const FVector ToTarget = Brain.LastSeen - Position;
        const double Range = ToTarget.Size2D();
        const FVector Forward = ToTarget.GetSafeNormal2D();
        const FVector Flank = FVector::CrossProduct(FVector::UpVector, Forward) * (Soldier->Slot % 2 ? 1. : -1.);
        const bool Hurt = Soldier->Rules.Health < 39.f;
        const bool UnderFire = Now < Soldier->Rules.WardUntil;
        if (Hurt && Range < 2000.)
            Soldier->Ward();

        if (Range > 1500.)
            Goal = Position + Forward * 550. + Flank * 200.;
        else
            Goal = Position + Flank * 375. + Forward * (Range < 560. ? -310. : 85.);

        if (Hurt && !UnderFire && Range < 2450.)
        {
            int Cover = INDEX_NONE;
            double BestCover = FMath::Square(1000.);
            for (int Node = 0; Node < Nodes.Num(); ++Node)
            {
                const double Distance = FVector::DistSquared2D(Position, Nodes[Node]);
                FHitResult CoverHit;
                if (Distance >= BestCover || Distance < FMath::Square(160.) ||
                    !Trace(Visible->Eye(), Nodes[Node] + FVector(0, 0, 35), Visible, CoverHit) ||
                    Cast<AArenaSoldier>(CoverHit.GetActor()))
                    continue;
                BestCover = Distance;
                Cover = Node;
            }
            if (Cover != INDEX_NONE)
                Goal = Nodes[Cover];
        }
        if (Range < 630.)
            Brain.Spell = PvG::Spell::Fan;
        else if (Range > 2450.)
            Brain.Spell = PvG::Spell::Lance;
        else
            Brain.Spell = Range > 950. && Random.FRand() < .21f ? PvG::Spell::Comet : PvG::Spell::Thread;
    }
    else if (Now < Brain.SeesUntil && FVector::DistSquared2D(Position, Brain.LastSeen) > FMath::Square(160.))
    {
        Goal = Brain.LastSeen;
        Brain.Patrolling = false;
    }
    else if (!Brain.Patrolling || FVector::DistSquared2D(Position, Goal) < FMath::Square(170.))
    {
        Brain.Patrolling = true;
        static const FVector Patrols[] = {
            {-1940, -1560, FloorCenter}, {0, -1560, FloorCenter}, {1940, -1560, FloorCenter},
            {1940, 1560, FloorCenter}, {0, 1560, FloorCenter}, {-1940, 1560, FloorCenter},
        };
        Brain.Patrol = (Brain.Patrol + 1) % UE_ARRAY_COUNT(Patrols);
        Goal = Patrols[Brain.Patrol];
    }
    Goal.Z = FloorCenter;
    const FVector CurrentGround(Position.X, Position.Y, FloorCenter);
    Brain.Destination = Goal;
    Brain.Route.Reset();
    if (!Walkable(CurrentGround, Goal))
    {
        const int From = NearestNode(CurrentGround, true);
        const int To = NearestNode(Goal, false);
        Brain.Route = FindRoute(From, To);
        Brain.Destination = Brain.Route.IsEmpty() ? CurrentGround : Nodes[To];
    }
    if (Now >= Brain.RetryAt)
    {
        const bool Stalled = FVector::DistSquared2D(Position, Brain.LastPosition) < FMath::Square(75.);
        if (Stalled && FVector::DistSquared2D(Position, Goal) > FMath::Square(180.))
        {
            const int Node = NearestNode(CurrentGround, false);
            if (Node != INDEX_NONE && !Links[Node].IsEmpty())
            {
                const TArray<int>& Neighbors = Links[Node];
                Brain.Destination = Nodes[Neighbors[Random.RandRange(0, Neighbors.Num() - 1)]];
                Brain.Route.Reset();
                Brain.Patrolling = false;
                ++NavigationRecoveries;
                Brain.NextThink = Now + .48;
                if (!Soldier->GetVelocity().IsNearlyZero())
                    Soldier->Jump();
            }
        }
        Brain.RetryAt = Now + 1.35;
        Brain.LastPosition = Position;
    }
}

void AArenaMatch::UpdateBot(int Index, float DeltaSeconds, double Now)
{
    AArenaSoldier* Soldier = Soldiers[Index];
    FArenaBot& Brain = Bots[Index];
    if (Now >= Brain.NextThink)
    {
        Brain.NextThink = Now + .23 + Random.FRandRange(0.f, .07f);
        Think(Soldier, Brain, Now);
    }

    const FVector Position = Soldier->GetActorLocation();
    while (!Brain.Route.IsEmpty() && FVector::DistSquared2D(Position, Nodes[Brain.Route.Last()]) < FMath::Square(90.))
        Brain.Route.Pop(EAllowShrinking::No);
    const FVector Goal = Brain.Route.IsEmpty() ? Brain.Destination : Nodes[Brain.Route.Last()];
    const FVector Move = Goal - Position;
    if (Move.SizeSquared2D() > FMath::Square(70.))
        Soldier->AddMovementInput(Move.GetSafeNormal2D());
    Soldier->SetSprinting(!Brain.Visible && Move.SizeSquared2D() > FMath::Square(650.));

    AArenaSoldier* Target = Brain.Target.Get();
    FVector Aim = Move.GetSafeNormal2D();
    if (Brain.Visible && Target && Target->Rules.Alive())
    {
        const FVector Chest = Target->GetActorLocation() + FVector(0, 0, 23);
        const FVector Eye = Soldier->Eye();
        Aim = (Chest - Eye).GetSafeNormal();
        FHitResult Hit;
        const bool Sight = Trace(Eye, Chest, Soldier, Hit) && Hit.GetActor() == Target;
        const bool Ready = Now >= Brain.FireAt && Now >= Soldier->Rules.ReadyAt;
        if (!Sight)
            Brain.Visible = false;
        else if (Ready && FVector::DotProduct(Soldier->GetActorForwardVector(), Aim.GetSafeNormal2D()) > .91)
        {
            constexpr float Accuracy[] = {7.5f, 4.2f, 2.3f};
            if (Brain.Spell == PvG::Spell::Comet)
            {
                const float Flight = FVector::Dist(Chest, Eye) / PvG::Arsenal[PvG::Index(Brain.Spell)].ProjectileSpeed;
                Aim = (Chest + Target->GetVelocity() * FMath::Min(.7f, Flight) * .65f - Eye).GetSafeNormal();
            }
            Aim = Random.VRandCone(Aim, FMath::DegreesToRadians(Accuracy[Difficulty]));
            Soldier->SetAim(Aim.Rotation());
            Soldier->Cast(Brain.Spell);
            Brain.FireAt = Now + .12 + Random.FRandRange(0.f, .13f);
            return;
        }
    }
    if (!Aim.IsNearlyZero() && !Soldier->Rules.Charging)
        Soldier->SetAim(FMath::RInterpTo(Soldier->GetActorRotation(), Aim.Rotation(), DeltaSeconds, 8.f));
}
