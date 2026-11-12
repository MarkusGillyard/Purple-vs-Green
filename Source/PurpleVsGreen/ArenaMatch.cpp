#include "ArenaMatch.h"

#include "ArenaController.h"
#include "ArenaHud.h"
#include "ArenaSoldier.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr int SoldiersPerTeam = 3;
constexpr float ArenaHalfX = 2900.f;
constexpr float ArenaHalfY = 2220.f;
}

AArenaMatch::AArenaMatch()
{
    PrimaryActorTick.bCanEverTick = true;
    PlayerControllerClass = AArenaController::StaticClass();
    HUDClass = AArenaHud::StaticClass();
    DefaultPawnClass = nullptr;
    bStartPlayersAsSpectators = true;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ColorAsset(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Cube = BoxAsset.Object;
    Sphere = SphereAsset.Object;
    Cylinder = CylinderAsset.Object;
    BaseMaterial = ColorAsset.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Aether(TEXT("/Game/Materials/M_Aether.M_Aether"));
    SpellMaterial = Aether.Object;
}

void AArenaMatch::StartPlay()
{
    Super::StartPlay();
    BuildArena();
    BuildRoutes();
    BuildSounds();
    LocalController = Cast<AArenaController>(GetWorld()->GetFirstPlayerController());
    if (LocalController)
    {
        LocalController->SetViewTarget(Overview);
        LocalController->ShowMenu();
    }
}

FLinearColor AArenaMatch::TeamTint(PvG::Team Side)
{
    return Side == PvG::Team::Purple ? FLinearColor(.55f, .18f, .92f) : FLinearColor(.20f, .72f, .29f);
}

FLinearColor AArenaMatch::SpellTint(PvG::Spell Spell)
{
    constexpr FLinearColor Tints[] = {
        {.30f, .73f, 1.f}, {.96f, .39f, .12f}, {.92f, .88f, 1.f}, {1.f, .26f, .057f}
    };
    return Tints[PvG::Index(Spell)];
}

UMaterialInstanceDynamic* AArenaMatch::Paint(UObject* Outer, FLinearColor Color, bool Luminous) const
{
    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Luminous && SpellMaterial ? SpellMaterial : BaseMaterial, Outer);
    if (Material)
        Material->SetVectorParameterValue(TEXT("Color"), Color);
    return Material;
}

AActor* AArenaMatch::Mesh(UStaticMesh* Shape, UMaterialInterface* Material,
                         const FTransform& Transform, bool Solid, float Lifetime)
{
    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
    UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetStaticMesh(Shape);
    Component->SetMaterial(0, Material);
    Component->SetCollisionProfileName(Solid ? TEXT("BlockAll") : TEXT("NoCollision"));
    Component->SetCastShadow(Solid);
    if (Lifetime > 0.f)
        Actor->SetLifeSpan(Lifetime);
    return Actor;
}

void AArenaMatch::BuildArena()
{
    auto Material = [this](FLinearColor Tint) { return Paint(this, Tint); };
    auto Block = [this](FVector Position, FVector Dimensions, UMaterialInterface* Mat,
                        bool Solid = true, float Yaw = 0.f)
    {
        Mesh(Cube, Mat, FTransform(FRotator(0, Yaw, 0), Position, Dimensions / 100.), Solid);
    };

    UMaterialInterface* Base = Material({.125f, .14f, .155f});
    UMaterialInterface* Stone = Material({.235f, .226f, .213f});
    UMaterialInterface* Slate = Material({.047f, .074f, .106f});
    UMaterialInterface* Pale = Material({.36f, .37f, .34f});
    UMaterialInterface* Brass = Material({.49f, .285f, .079f});
    UMaterialInterface* Background = Material({.10f, .14f, .17f});
    Block({0, 0, -30}, {5800, 4440, 60}, Base);
    Block({0, 0, -170}, {32000, 32000, 50}, Background, false);

    AActor* Detail = GetWorld()->SpawnActor<AActor>();
    USceneComponent* Origin = NewObject<USceneComponent>(Detail);
    Detail->SetRootComponent(Origin);
    Origin->RegisterComponent();
    auto Instances = [Detail, Origin, this](UMaterialInterface* Mat)
    {
        UInstancedStaticMeshComponent* Batch = NewObject<UInstancedStaticMeshComponent>(Detail);
        Batch->SetupAttachment(Origin);
        Batch->SetStaticMesh(Cube);
        Batch->SetMaterial(0, Mat);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetCastShadow(false);
        Batch->RegisterComponent();
        return Batch;
    };
    auto Tile = [](UInstancedStaticMeshComponent* Batch, FVector Center, FVector Size, FRotator Angle = FRotator::ZeroRotator)
    {
        Batch->AddInstance(FTransform(Angle, Center, Size / 100.));
    };
    FRandomStream Pattern(5129);
    UInstancedStaticMeshComponent* Pavers[3] = {
        Instances(Material({.178f, .188f, .199f})), Instances(Material({.194f, .199f, .204f})),
        Instances(Material({.156f, .17f, .181f}))
    };
    UInstancedStaticMeshComponent* Inlays = Instances(Brass);
    UInstancedStaticMeshComponent* Moldings = Instances(Pale);
    for (int Y = -9; Y <= 9; ++Y)
    {
        const double CenterY = Y * 225.;
        for (int X = -9; X <= 9; ++X)
        {
            const double CenterX = X * 303. + (Y % 2 ? 70. : -70.);
            Tile(Pavers[Pattern.RandRange(0, 2)], {CenterX, CenterY, .34}, {299, 221, .68});
        }
    }
    for (const int Side : {-1, 1})
    {
        for (int Bay = -7; Bay <= 7; ++Bay)
        {
            const double X = Bay * 378.;
            const double Y = Side * 2156.;
            Tile(Moldings, {X, Y, 182}, {340, 16, 236});
            Tile(Inlays, {X, Y - Side * 10., 77}, {322, 8, 9});
            Tile(Inlays, {X, Y - Side * 10., 289}, {322, 8, 9});
            Tile(Inlays, {X, Y - Side * 10., 189}, {24, 8, 24}, FRotator(45, 0, 0));
        }
        for (int Span = -2; Span <= 2; ++Span)
        {
            const double Y = Span * 830. + 415.;
            const double X = Side * 2780.;
            for (const int Leg : {-1, 1})
            {
                Tile(Moldings, {X, Y + Leg * 244., 104}, {28, 52, 208});
                Tile(Inlays, {X - Side * 18., Y + Leg * 244., 200}, {15, 56, 16});
            }
            for (int Segment = 0; Segment <= 12; ++Segment)
            {
                const double Angle = Segment * UE_PI / 12.;
                Tile(Segment == 6 ? Inlays : Moldings, {X, Y + 244. * FMath::Cos(Angle), 206. + 150. * FMath::Sin(Angle)},
                    {35, 50, 43}, FRotator(0, 0, FMath::RadiansToDegrees(Angle)));
            }
        }
    }
    for (int Obelisk = 0; Obelisk < 14; ++Obelisk)
    {
        const double Angle = Obelisk * 2. * UE_PI / 14.;
        const double Height = Pattern.FRandRange(720.f, 1960.f);
        const FVector Axis(FMath::Cos(Angle), FMath::Sin(Angle), 0.);
        const FVector Position = Axis * Pattern.FRandRange(5500.f, 8300.f);
        Block(Position + FVector(0, 0, Height / 2. - 145.), {510, 450, Height}, Background, false, Obelisk * 24.f);
        Block(Position + FVector(0, 0, Height - 125.), {570, 490, 43}, Slate, false, Obelisk * 24.f);
    }

    for (const int Side : {-1, 1})
    {
        UMaterialInterface* Banner = Material(TeamTint(Side < 0 ? PvG::Team::Purple : PvG::Team::Green));
        Block({0, Side * ArenaHalfY, 181}, {5960, 108, 362}, Slate);
        Block({Side * ArenaHalfX, 0, 243}, {110, 4440, 486}, Slate);
        Block({0, Side * ArenaHalfY, 362}, {5960, 140, 34}, Brass, false);
        Block({Side * ArenaHalfX, 0, 486}, {150, 4440, 36}, Stone, false);

        for (int Arch = -2; Arch <= 2; ++Arch)
        {
            const double Y = Arch * 830.;
            Block({Side * 2820., Y, 235}, {160, 147, 470}, Stone);
            Block({Side * 2732., Y, 275}, {13, 80, 240}, Banner, false);
            const FRotator Emblem(Side < 0 ? 45.f : 0.f, 0, Side < 0 ? 0.f : 45.f);
            Mesh(Cube, Brass, FTransform(Emblem, FVector(Side * 2723., Y, 329), FVector(.11, .42, .42)), false);
            if (Side > 0)
                Block({Side * 2723., Y, 291}, {11, 42, 10}, Brass, false);
        }

        for (const int Wing : {-1, 1})
        {
            const double Y = Wing * 1280.;
            Block({Side * 1450., Y, 152}, {550, 222, 304}, Stone);
            Block({Side * 1450., Y, 309}, {585, 250, 36}, Slate);
            Block({Side * 1450., Y + Wing * 123., 219}, {425, 24, 61}, Brass, false);
            Block({Side * 2110., Wing * 830., 67}, {425, 168, 134}, Slate);
            Block({Side * 2110., Wing * 830., 138}, {443, 183, 22}, Banner, false);
            Block({Side * 630., Wing * 1740., 89}, {223, 365, 178}, Stone);
            Block({Side * 630., Wing * 1740., 180}, {254, 395, 25}, Pale, false);
            Block({Side * 600., Wing * 2055., 6}, {1500, 18, 3}, Brass, false);
        }

        Block({Side * 1200., 0, 119}, {243, 520, 238}, Slate);
        Block({Side * 1200., 0, 240}, {267, 550, 24}, Pale, false);
        Block({Side * 2510., 0, 2}, {490, 680, 6}, Banner, false);
        for (int Stripe = -2; Stripe <= 2; ++Stripe)
            Block({Side * (2530. + Stripe * 92.), 0, 7}, {9, 630, 2}, Brass, false);

        TArray<FVector>& SideSpawns = Spawns[Side < 0 ? 0 : 1];
        for (const int Lane : {-1, 0, 1})
        {
            SideSpawns.Add({Side * 2520., Lane * 1480., FloorCenter});
            SideSpawns.Add({Side * 1820., Lane * 1750., FloorCenter});
        }
    }

    Mesh(Cylinder, Slate, FTransform(FRotator::ZeroRotator, FVector(0, 0, 103), FVector(6.65, 6.65, 2.06)), true);
    Mesh(Cylinder, Brass, FTransform(FRotator::ZeroRotator, FVector(0, 0, 207), FVector(7.2, 7.2, .13)), false);
    for (int Pillar = 0; Pillar < 8; ++Pillar)
    {
        const double Angle = Pillar * UE_PI / 4.;
        const FVector Axis(FMath::Cos(Angle), FMath::Sin(Angle), 0.);
        const FVector Center = Axis * 525.;
        Block(Center + FVector(0, 0, 127), {172, 157, 254}, Stone, true, Pillar * 45.f);
        Block(Center + FVector(0, 0, 258), {203, 183, 24}, Brass, false, Pillar * 45.f);
        Block(Axis * 760. + FVector(0, 0, 3), {240, 12, 4}, Brass, false, Pillar * 45.f);
        Block(Axis * 1000. + FVector(0, 0, 3), {120, 6, 4}, Pale, false, Pillar * 45.f);
    }
    Mesh(Sphere, Pale, FTransform(FRotator::ZeroRotator, FVector(0, 0, 614), FVector(1.54, 1.54, 1.54)), false);
    for (int Segment = 0; Segment < 24; ++Segment)
    {
        const double Angle = Segment * UE_PI / 12.;
        const FVector Point(FMath::Cos(Angle) * 204., FMath::Sin(Angle) * 204., 614.);
        Block(Point, {55, 14, 15}, Brass, false, Segment * 15.f + 90.f);
    }

    GetWorld()->SpawnActor<ASkyAtmosphere>();
    auto Sun = [this](FRotator Angle, FLinearColor Tint, float Intensity, bool Shadows)
    {
        ADirectionalLight* Lamp = GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector, Angle);
        UDirectionalLightComponent* Light = CastChecked<UDirectionalLightComponent>(Lamp->GetLightComponent());
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetLightColor(Tint);
        Light->SetIntensity(Intensity);
        Light->SetCastShadows(Shadows);
        Light->SetAtmosphereSunLight(Shadows);
        Light->SetForwardShadingPriority(Shadows ? 1 : 0);
    };
    Sun({-51, -35, 0}, {1.f, .79f, .58f}, 2.4f, true);
    Sun({-49, 145, 0}, {.47f, .69f, 1.f}, .9f, false);
    Sun({-18, 65, 0}, {.45f, .74f, 1.f}, .65f, false);

    ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(FVector(-2090, -1940, 1340), FRotator(-21, 41, 0));
    Camera->GetCameraComponent()->SetFieldOfView(75.f);
    Overview = Camera;
}

AArenaSoldier* AArenaMatch::PlayerSoldier() const
{
    return Soldiers.IsEmpty() ? nullptr : Soldiers[0].Get();
}

void AArenaMatch::ClearRound()
{
    if (LocalController && LocalController->GetPawn())
        LocalController->UnPossess();
    for (AArenaSoldier* Soldier : Soldiers)
        Soldier->Destroy();
    Soldiers.Reset();
    Bots.Reset();
    for (const FArenaMissile& Missile : Missiles)
        if (Missile.Visual.IsValid())
            Missile.Visual->Destroy();
    Missiles.Reset();
}

void AArenaMatch::StartMatch()
{
    if (!LocalController)
        return;
    ClearRound();
    const double Now = GetWorld()->GetTimeSeconds();
    PlayerHits = 0;
    PlayerDamageEvents = 0;
    NavigationRecoveries = 0;
    Respawns = 0;
    LastPlayerHitAt = -10.;
    LastPlayerHurtAt = -10.;
    Rules.Start(ScoreLimit, Minutes * 60., Now);
    Bots.SetNum(SoldiersPerTeam * 2);
    for (int Index = 0; Index < Bots.Num(); ++Index)
    {
        const PvG::Team Side = Index < SoldiersPerTeam ? ChosenTeam : PvG::EnemyOf(ChosenTeam);
        const FTransform Spawn = SelectSpawn(Side, Index);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AArenaSoldier* Soldier = GetWorld()->SpawnActor<AArenaSoldier>(AArenaSoldier::StaticClass(), Spawn, Params);
        Soldier->Initialize(this, Side, Index);
        Soldier->Respawn(Spawn);
        Soldiers.Add(Soldier);
        FArenaBot& Brain = Bots[Index];
        Brain.NextThink = Now + .1 * Index;
        Brain.Patrol = Index;
    }
    LocalController->Possess(PlayerSoldier());
    LocalController->BeginRound();
    Feedback(PlayerSoldier()->Eye(), 7, .72f);
}

void AArenaMatch::ReturnToMenu()
{
    Rules.State = PvG::Phase::Menu;
    ClearRound();
    LocalController->SetViewTarget(Overview);
    LocalController->ShowMenu();
}

void AArenaMatch::Finish()
{
    for (AArenaSoldier* Soldier : Soldiers)
    {
        Soldier->SetSprinting(false);
        Soldier->ConsumeMovementInputVector();
    }
    LocalController->SetViewTarget(Overview);
    LocalController->ShowResults();
    const int Own = Rules.Score[PvG::Index(ChosenTeam)];
    const int Opposing = Rules.Score[PvG::Index(PvG::EnemyOf(ChosenTeam))];
    Feedback(FVector::ZeroVector, Own == Opposing ? 12 : Own > Opposing ? 10 : 11, .74f);
    UE_LOG(LogTemp, Display,
        TEXT("PvG round result: Purple %d Green %d Player hits %d Player damage %d Respawns %d Route recoveries %d"),
        Rules.Score[0], Rules.Score[1], PlayerHits, PlayerDamageEvents, Respawns, NavigationRecoveries);
}

void AArenaMatch::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsPlaying())
        return;
    const double Now = GetWorld()->GetTimeSeconds();
    Rules.Advance(Now);
    if (!IsPlaying())
    {
        Finish();
        return;
    }

    for (int Index = 0; Index < Soldiers.Num() && IsPlaying(); ++Index)
    {
        AArenaSoldier* Soldier = Soldiers[Index];
        if (!Soldier->Rules.Alive() && Now >= Soldier->RespawnAt)
        {
            Soldier->Respawn(SelectSpawn(Soldier->Team, Index));
            ++Respawns;
            Bots[Index] = {};
            Bots[Index].Patrol = Index;
        }
        if (Index != 0 && Soldier->Rules.Alive())
            UpdateBot(Index, DeltaSeconds, Now);
    }
    if (IsPlaying())
        UpdateMissiles(DeltaSeconds);
}

FTransform AArenaMatch::SelectSpawn(PvG::Team Side, int Slot) const
{
    const TArray<FVector>& Positions = Spawns[PvG::Index(Side)];
    double Best = -TNumericLimits<double>::Max();
    FVector Choice = Positions[0];
    for (int Index = 0; Index < Positions.Num(); ++Index)
    {
        const FVector Candidate = Positions[(Index + Slot) % Positions.Num()];
        double Score = 0.;
        for (const AArenaSoldier* Soldier : Soldiers)
        {
            if (!Soldier->Rules.Alive())
                continue;
            const double Distance = FVector::Dist2D(Candidate, Soldier->GetActorLocation());
            if (Distance < 115.)
                Score -= 15000.;
            else if (Soldier->Team != Side)
            {
                FHitResult Hit;
                Score += FMath::Min(Distance, 3600.) * .35;
                if (!Trace(Soldier->Eye(), Candidate + FVector(0, 0, 50), Soldier, Hit))
                    Score -= 1850.;
            }
            else if (Distance < 550.)
                Score -= 550. - Distance;
        }
        if (Score > Best)
        {
            Best = Score;
            Choice = Candidate;
        }
    }
    return FTransform(FRotator(0, Side == PvG::Team::Purple ? 0.f : 180.f, 0), Choice);
}

bool AArenaMatch::Trace(FVector From, FVector To, const AActor* Ignore, FHitResult& Hit) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PvGSpell), false, Ignore);
    return GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
}

void AArenaMatch::Defeated(AArenaSoldier* Soldier, AArenaSoldier* Killer)
{
    if (!Rules.Eliminate(Killer->Team, Soldier->Team))
        return;
    ++Soldier->Defeats;
    ++Killer->Eliminations;
    Spark(Soldier->GetActorLocation() + FVector(0, 0, 34), TeamTint(Soldier->Team), 58.f, .38f);
    Feedback(Soldier->GetActorLocation(), 5);
    if (!IsPlaying())
        Finish();
}

void AArenaMatch::Spark(FVector Location, FLinearColor Tint, float Radius, float Duration)
{
    Mesh(Sphere, Paint(this, Tint, true), FTransform(FRotator::ZeroRotator, Location, FVector(Radius / 50.f)), false, Duration);
}

void AArenaMatch::Beam(FVector From, FVector To, FLinearColor Tint, float Width, float Duration)
{
    const FVector Ray = To - From;
    if (Ray.SizeSquared() < 1.)
        return;
    Mesh(Cube, Paint(this, Tint, true), FTransform(Ray.Rotation(), (From + To) / 2.,
        FVector(Ray.Size() / 100., Width / 100., Width / 100.)), false, Duration);
}

void AArenaMatch::Fire(AArenaSoldier* Caster, PvG::Spell Spell, FVector Direction)
{
    if (!IsPlaying())
        return;
    const PvG::SpellSpec& Spec = PvG::Arsenal[PvG::Index(Spell)];
    const FVector From = Caster->Eye();
    const FVector Hand = From + Direction * 55. + Direction.Rotation().RotateVector(FVector(0, 19, -21));
    const FLinearColor Tint = SpellTint(Spell);
    Feedback(From, PvG::Index(Spell), .58f);
    Spark(Hand, Tint, 12.f + PvG::Index(Spell) * 3.f, .12f);

    if (Spec.ProjectileSpeed > 0.f)
    {
        FHitResult Obstruction;
        if (Trace(From, From + Direction * 45., Caster, Obstruction))
        {
            Explode(Caster, Obstruction.ImpactPoint + Obstruction.ImpactNormal * 4.);
            return;
        }
        const FVector Origin = From + Direction * 42.;
        AActor* Orb = Mesh(Sphere, Paint(this, Tint, true), FTransform(Direction.Rotation(), Origin,
            FVector(.42, .29, .29)), false);
        Missiles.Add({Caster, Orb, Origin, Direction, Spec.Range});
        return;
    }

    TMap<AArenaSoldier*, float> Hits;
    for (int Ray = 0; Ray < Spec.Rays; ++Ray)
    {
        const FVector Trajectory = Random.VRandCone(Direction, FMath::DegreesToRadians(Spec.SpreadDegrees));
        FHitResult Impact;
        const FVector End = From + Trajectory * Spec.Range;
        const bool Blocked = Trace(From, End, Caster, Impact);
        Beam(Hand, Blocked ? Impact.ImpactPoint : End, Tint, Spell == PvG::Spell::Lance ? 8.f : 3.f, .09f);
        if (Blocked)
        {
            Spark(Impact.ImpactPoint, Tint, 8.f, .16f);
            if (AArenaSoldier* Target = Cast<AArenaSoldier>(Impact.GetActor()))
                Hits.FindOrAdd(Target) += Spec.Damage;
        }
    }
    for (const auto& Pair : Hits)
    {
        if (Pair.Key->ReceiveSpell(Caster, Pair.Value))
            Feedback(Pair.Key->GetActorLocation(), 4, .56f);
    }
}

void AArenaMatch::UpdateMissiles(float DeltaSeconds)
{
    const PvG::SpellSpec& Spec = PvG::Arsenal[PvG::Index(PvG::Spell::Comet)];
    for (int Index = Missiles.Num() - 1; Index >= 0 && IsPlaying(); --Index)
    {
        FArenaMissile& Missile = Missiles[Index];
        const float Step = FMath::Min(Missile.DistanceLeft, DeltaSeconds * Spec.ProjectileSpeed);
        const FVector Destination = Missile.Location + Missile.Direction * Step;
        FHitResult Impact;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PvGMissile), false, Missile.Caster.Get());
        const bool Blocked = GetWorld()->SweepSingleByChannel(Impact, Missile.Location, Destination,
            FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(13.f), Query);
        Missile.DistanceLeft -= Step;
        if (Blocked || Missile.DistanceLeft <= 0.f)
        {
            Explode(Missile.Caster.Get(), Blocked ? Impact.Location : Destination);
            if (Missile.Visual.IsValid())
                Missile.Visual->Destroy();
            Missiles.RemoveAtSwap(Index, EAllowShrinking::No);
        }
        else
        {
            Missile.Location = Destination;
            if (Missile.Visual.IsValid())
                Missile.Visual->SetActorLocation(Destination);
        }
    }
}

void AArenaMatch::Explode(AArenaSoldier* Caster, FVector Location)
{
    const PvG::SpellSpec& Spec = PvG::Arsenal[PvG::Index(PvG::Spell::Comet)];
    const FLinearColor Tint = SpellTint(PvG::Spell::Comet);
    Spark(Location, Tint, 74.f, .18f);
    for (int Ray = 0; Ray < 10; ++Ray)
        Beam(Location, Location + Random.VRand() * Random.FRandRange(120.f, 280.f), Tint, 5.f, .16f);
    Feedback(Location, 5, .84f);
    if (!Caster)
        return;
    for (AArenaSoldier* Target : Soldiers)
    {
        const float Distance = FVector::Dist(Target->GetActorLocation(), Location);
        FHitResult Impact;
        if (Distance >= Spec.BlastRadius ||
            (Trace(Location, Target->GetActorLocation(), Caster, Impact) && Impact.GetActor() != Target))
            continue;
        const float Damage = Spec.Damage * FMath::Lerp(1.f, .32f, Distance / Spec.BlastRadius);
        Target->ReceiveSpell(Caster, Damage);
    }
}

void AArenaMatch::MarkHit(const AArenaSoldier* Caster, const AArenaSoldier* Victim)
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (Caster == PlayerSoldier())
    {
        ++PlayerHits;
        LastPlayerHitAt = Now;
    }
    if (Victim == PlayerSoldier())
    {
        ++PlayerDamageEvents;
        LastPlayerHurtAt = Now;
    }
}

void AArenaMatch::ShowWard(const AArenaSoldier* Soldier)
{
    const FVector Center = Soldier->GetActorLocation() + FVector(0, 0, 30);
    const FLinearColor Tint(.19f, .74f, .93f);
    for (int Segment = 0; Segment < 10; ++Segment)
    {
        const double A = Segment * UE_PI / 5.;
        const double B = (Segment + 1) * UE_PI / 5.;
        const FVector From = Center + FVector(FMath::Cos(A), FMath::Sin(A), .16 * FMath::Sin(A * 2.)) * 64.;
        const FVector To = Center + FVector(FMath::Cos(B), FMath::Sin(B), .16 * FMath::Sin(B * 2.)) * 64.;
        Beam(From, To, Tint, 3.f, .26f);
    }
}

void AArenaMatch::BuildSounds()
{
    SpatialAudio = NewObject<USoundAttenuation>(this);
    SpatialAudio->Attenuation.bAttenuate = true;
    SpatialAudio->Attenuation.bSpatialize = true;
    SpatialAudio->Attenuation.AttenuationShapeExtents = FVector(320, 0, 0);
    SpatialAudio->Attenuation.FalloffDistance = 3950.f;
    constexpr int Rate = 22050;
    constexpr float Start[] = {680.f, 280.f, 1110.f, 190.f, 925.f, 146.f, 330.f, 440.f, 142.f, 564.f, 330.f, 348.f, 294.f};
    constexpr float End[] = {220.f, 104.f, 402.f, 84.f, 570.f, 48.f, 660.f, 523.f, 61.f, 368.f, 660.f, 164.f, 294.f};
    constexpr float Length[] = {.115f, .26f, .25f, .34f, .09f, .42f, .41f, .63f, .095f, .060f, .77f, .64f, .52f};
    FRandomStream Noise(40691);
    for (int Event = 0; Event < UE_ARRAY_COUNT(Start); ++Event)
    {
        TArray<uint8>& PCM = SoundSamples.AddDefaulted_GetRef();
        double Phase = 0.;
        for (int Sample = 0; Sample < FMath::RoundToInt(Length[Event] * Rate); ++Sample)
        {
            const float Time = static_cast<float>(Sample) / Rate;
            const float Progress = Time / Length[Event];
            const float Envelope = FMath::Min(1.f, Time / .008f) * FMath::Square(1.f - Progress);
            const float Frequency = FMath::Lerp(Start[Event], End[Event], Progress);
            Phase += Frequency * 2. * UE_PI / Rate;
            const float Harmonic = Event == 7 || Event >= 10 ? .32f : .18f;
            const float Texture = Event == 1 || Event == 5 || Event == 8 ? .35f : .065f;
            const float Signal = .54f * FMath::Sin(Phase) + Harmonic * FMath::Sin(Phase * 1.5)
                + Texture * Noise.FRandRange(-1.f, 1.f);
            const int16 Value = FMath::RoundToInt(24000.f * Envelope * Signal);
            PCM.Add(static_cast<uint8>(Value & 0xff));
            PCM.Add(static_cast<uint8>((static_cast<uint16>(Value) >> 8) & 0xff));
        }
    }
}

void AArenaMatch::Feedback(FVector Location, int Event, float Volume)
{
    if (!SoundSamples.IsValidIndex(Event) || !LocalController)
        return;
    const bool Interface = Event >= 9;
    if (MasterVolume <= 0.f)
        return;
    if (!Interface)
    {
        FVector Listener;
        FRotator Facing;
        LocalController->GetPlayerViewPoint(Listener, Facing);
        if (FVector::DistSquared(Listener, Location) > FMath::Square(4270.))
            return;
    }
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>();
    Wave->SetSampleRate(22050);
    Wave->NumChannels = 1;
    Wave->Duration = SoundSamples[Event].Num() / 44100.f;
    Wave->QueueAudio(SoundSamples[Event].GetData(), SoundSamples[Event].Num());
    UAudioComponent* Audio = Interface
        ? UGameplayStatics::SpawnSound2D(this, Wave, MasterVolume * Volume)
        : UGameplayStatics::SpawnSoundAtLocation(this, Wave, Location, FRotator::ZeroRotator,
            MasterVolume * Volume, 1.f, 0.f, SpatialAudio);
    if (Audio)
    {
        if (Interface)
            Audio->SetUISound(true);
        Audio->StopDelayed(Wave->Duration + .03f);
    }
}
