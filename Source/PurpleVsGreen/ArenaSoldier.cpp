#include "ArenaSoldier.h"

#include "ArenaMatch.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AArenaSoldier::AArenaSoldier()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessAI = EAutoPossessAI::Disabled;
    bUseControllerRotationYaw = true;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    UCharacterMovementComponent* Move = GetCharacterMovement();
    Move->MaxWalkSpeed = 590.f;
    Move->bRunPhysicsWithNoController = true;
    Move->MaxAcceleration = 3400.f;
    Move->BrakingDecelerationWalking = 2800.f;
    Move->GroundFriction = 8.f;
    Move->JumpZVelocity = 510.f;
    Move->AirControl = 0.43f;
    Move->GravityScale = 1.35f;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Eyes"));
    Camera->SetupAttachment(RootComponent);
    Camera->SetRelativeLocation(FVector(0, 0, 63));
    Camera->bUsePawnControlRotation = true;
    Camera->SetFieldOfView(96.f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Box(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Vambrace(TEXT("/Game/Meridian/SM_MeridianVambrace.SM_MeridianVambrace"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(
        TEXT("/Game/Battlemage/SK_Battlemage.SK_Battlemage"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(
        TEXT("/Game/Materials/M_Armor.M_Armor"));
    ArmorMaterial = Surface.Object;
    Battlemage = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Battlemage"));
    Battlemage->SetupAttachment(RootComponent);
    Battlemage->SetSkinnedAssetAndUpdate(Body.Object);
    Battlemage->SetRelativeLocation({0, 0, -90});
    Battlemage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Battlemage->SetOwnerNoSee(true);
    Battlemage->bCastHiddenShadow = true;

    CastingHands = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonRig"));
    CastingHands->SetupAttachment(Camera);
    auto GauntletPart = [&](FName Name, USceneComponent* Parent, UStaticMesh* Mesh, FVector Location,
                            FVector Scale, FRotator Angle, bool Dyed, bool Metal = false)
    {
        UStaticMeshComponent* Shape = AddPart(Name, Parent, Mesh, Location, Scale);
        Shape->SetRelativeRotation(Angle);
        Shape->SetOnlyOwnerSee(true);
        Shape->SetCastShadow(false);
        (Metal ? Trim : Dyed ? Livery : Armor).Add(Shape);
    };
    for (const int Side : {-1, 1})
    {
        const bool Right = Side > 0;
        const FString Prefix = Right ? TEXT("CastRight") : TEXT("CastLeft");
        USceneComponent* Wrist = CreateDefaultSubobject<USceneComponent>(*Prefix);
        Wrist->SetupAttachment(CastingHands);
        Wrist->SetRelativeLocation(FVector(Right ? 61 : 55, Side * 23, Right ? -23 : -25));
        Wrist->SetRelativeRotation(FRotator(8, Side * -18, Side * 12));
        (Right ? CastingRight : CastingLeft) = Wrist;

        auto Segment = [&](const TCHAR* Label, UStaticMesh* Mesh, FVector Location, FVector Scale,
                           FRotator Angle, bool Dyed, bool Metal = false)
        {
            GauntletPart(FName(Prefix + Label), Wrist, Mesh, Location, Scale, Angle, Dyed, Metal);
        };
        Segment(TEXT("Sleeve"), Cylinder.Object, {-18, 0, -2}, {.142, .147, .47}, {90, 0, 0}, false);
        Segment(TEXT("Bracer"), Vambrace.Object, {-17, 0, -1.5}, {.34, .30, .36}, {90, 0, 0}, true);
        Segment(TEXT("BracerSpine"), Box.Object, {-15, 0, 14.3}, {.22, .015, .014}, {0, 0, 0}, false, true);
        for (const int Edge : {-1, 1})
        {
            const FString Rivet = FString::Printf(TEXT("BraceRivet%d"), Edge + 1);
            Segment(*Rivet, Sphere.Object, {-6, Edge * 5.8, 11.7}, {.023, .023, .019},
                FRotator::ZeroRotator, false, true);
        }
        Segment(TEXT("Cuff"), Cylinder.Object, {-5, 0, -1.3}, {.146, .152, .065}, {90, 0, 0}, false, true);
        Segment(TEXT("CuffBinding"), Cylinder.Object, {-7.8, 0, -1.3}, {.147, .153, .015}, {90, 0, 0}, true);
        Segment(TEXT("Palm"), Sphere.Object, {4.7, 0, 0}, {.151, .147, .077}, {0, 0, 0}, false);
        Segment(TEXT("Plate"), Sphere.Object, {4.4, 0, 3.1}, {.127, .119, .034}, {0, 0, 0}, true);
        Segment(TEXT("Seal"), Box.Object, {5, 0, 5.0}, {.034, .034, .014}, {0, 45, 0}, false, true);
        Segment(TEXT("Thumb"), Sphere.Object, {6.7, Side * -6.8, -1.5}, {.084, .038, .043}, {5, Side * -28.f, 0}, false);
        for (int Digit = 0; Digit < 4; ++Digit)
        {
            const double Length[] = {6.8, 8.4, 7.7, 5.6};
            const FString Name = Prefix + FString::Printf(TEXT("Finger%d"), Digit);
            USceneComponent* Joint = CreateDefaultSubobject<USceneComponent>(*Name);
            Joint->SetupAttachment(Wrist);
            Joint->SetRelativeLocation({10.1, Side * (-4.7 + Digit * 3.03), -.4});
            Digits.Add(Joint);
            GauntletPart(FName(Name + TEXT("Leather")), Joint, Sphere.Object, {Length[Digit] * .4, 0, 0},
                {Length[Digit] / 100., .028, .030}, FRotator::ZeroRotator, false);
            GauntletPart(FName(Name + TEXT("Tip")), Joint, Sphere.Object, {Length[Digit] * .79, 0, -.7},
                {Length[Digit] * .57 / 100., .024, .027}, FRotator(-14, 0, 0), false);
            GauntletPart(FName(Name + TEXT("Knuckle")), Joint, Sphere.Object, {1.7, 0, 1.3},
                {.039, .028, .021}, FRotator::ZeroRotator, false, true);
        }
    }
}

UStaticMeshComponent* AArenaSoldier::AddPart(FName Name, USceneComponent* Parent, UStaticMesh* Mesh,
                                           FVector Location, FVector Scale)
{
    UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Part->SetupAttachment(Parent);
    Part->SetStaticMesh(Mesh);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetRelativeLocation(Location);
    Part->SetRelativeScale3D(Scale);
    return Part;
}

void AArenaSoldier::Initialize(AArenaMatch* Match, PvG::Team Side, int InSlot)
{
    Arena = Match;
    Team = Side;
    Slot = InSlot;
    SetTeamAppearance();

    if (const USkeletalMesh* Body = Cast<USkeletalMesh>(Battlemage->GetSkinnedAsset()))
    {
        const FReferenceSkeleton& Skeleton = Body->GetRefSkeleton();
        ComponentReference.SetNum(Skeleton.GetNum());
        PoseComponents.SetNum(Skeleton.GetNum());
        for (int Index = 0; Index < Skeleton.GetNum(); ++Index)
        {
            const FTransform& Local = Skeleton.GetRefBonePose()[Index];
            const int Parent = Skeleton.GetParentIndex(Index);
            ComponentReference[Index] = Parent == INDEX_NONE ? Local : Local * ComponentReference[Parent];
        }
    }
}

void AArenaSoldier::SetTeamAppearance()
{
    const FLinearColor Dye = AArenaMatch::TeamTint(Team);
    const auto Finish = [this](FLinearColor Color, float Metallic, float Roughness)
    {
        if (!ArmorMaterial)
            return Arena->Paint(this, Color);
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ArmorMaterial, this);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
        Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        return Material;
    };
    UMaterialInstanceDynamic* Leather = Finish({.064f, .076f, .084f}, .02f, .79f);
    UMaterialInstanceDynamic* Enamel = Finish(Dye * .37f, .46f, .56f);
    UMaterialInstanceDynamic* Bronze = Finish({.58f, .34f, .13f}, .68f, .40f);
    for (UStaticMeshComponent* Part : Armor)
        Part->SetMaterial(0, Leather);
    for (UStaticMeshComponent* Part : Livery)
        Part->SetMaterial(0, Enamel);
    for (UStaticMeshComponent* Part : Trim)
        Part->SetMaterial(0, Bronze);

    const auto Surface = [this](FName Name, UMaterialInterface* Material)
    {
        if (const int Index = Battlemage->GetMaterialIndex(Name); Index != INDEX_NONE)
            Battlemage->SetMaterial(Index, Material);
    };
    Surface(TEXT("Obsidian"), Finish({.12f, .145f, .169f}, .51f, .46f));
    Surface(TEXT("Bronze"), Bronze);
    Surface(TEXT("Livery"), Finish(Dye * .78f, .045f, .73f));
    Surface(TEXT("Gaps"), Leather);
}

void AArenaSoldier::Respawn(const FTransform& Spawn)
{
    Rules = {};
    Rules.ProtectedUntil = GetWorld()->GetTimeSeconds() + 1.15;
    SetActorTransform(Spawn, false, nullptr, ETeleportType::TeleportPhysics);
    SetAim(Spawn.Rotator());
    SetActorHiddenInGame(false);
    CastingHands->SetVisibility(true, true);
    Battlemage->SetRelativeLocation({0, 0, -90});
    Battlemage->SetRelativeRotation(FRotator::ZeroRotator);
    SetActorEnableCollision(true);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    SetSprinting(false);
    Stride = 0.f;
    PoseCast = 0.f;
    PoseWard = 0.f;
    AirPose = 0.f;
    Landing = 0.f;
    WasAirborne = false;
    HasViewHistory = false;
    ViewSway = FVector2D::ZeroVector;
    LastCast = -10.;
    LastHit = -10.;
    LastStep = GetWorld()->GetTimeSeconds();
}

void AArenaSoldier::SetAim(FRotator Rotation)
{
    Aim = Rotation;
    SetActorRotation(FRotator(0, Aim.Yaw, 0));
    if (Controller)
        Controller->SetControlRotation(Aim);
}

FVector AArenaSoldier::Eye() const
{
    return Camera->GetComponentLocation();
}

void AArenaSoldier::SetSprinting(bool Enabled)
{
    GetCharacterMovement()->MaxWalkSpeed = Enabled ? 820.f : 590.f;
}

void AArenaSoldier::Cast(PvG::Spell Spell)
{
    if (!Arena || !Arena->IsPlaying())
        return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Rules.StartCast(Spell, Now, ReleaseAt))
        return;
    Rules.ProtectedUntil = Now;
    if (ReleaseAt <= Now)
        Release();
    else
        Arena->Feedback(Eye(), 4, 0.27f);
}

void AArenaSoldier::Release()
{
    Rules.Charging = false;
    LastCast = GetWorld()->GetTimeSeconds();
    Arena->Fire(this, Rules.ActiveSpell, Controller ? Controller->GetControlRotation().Vector() : Aim.Vector());
}

void AArenaSoldier::Ward()
{
    if (Arena && Arena->IsPlaying() && Rules.Ward(GetWorld()->GetTimeSeconds()))
    {
        Arena->PlayerWards += this == Arena->PlayerSoldier();
        Arena->ShowWard(this);
        Arena->Feedback(Eye(), 6);
    }
}

bool AArenaSoldier::ReceiveSpell(AArenaSoldier* Caster, float Damage)
{
    if (!Caster || !Arena->IsPlaying())
        return false;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Rules.Hit(Team, Caster->Team, Damage, Now) <= 0.f)
        return false;
    LastHit = Now;
    const FVector Inward = (GetActorLocation() - Caster->GetActorLocation()).GetSafeNormal2D();
    HitSide = FVector::DotProduct(GetActorRightVector(), Inward);
    Arena->MarkHit(Caster, this);
    if (Now < Rules.WardUntil)
        Arena->ShowWard(this);
    if (!Rules.Alive())
    {
        CastingHands->SetVisibility(false, true);
        SetActorEnableCollision(false);
        GetCharacterMovement()->DisableMovement();
        RespawnAt = LastHit + PvG::RespawnDelay;
        Arena->Defeated(this, Caster);
    }
    return true;
}

void AArenaSoldier::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Arena)
        return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Rules.Alive())
    {
        AnimateDefeat(Now);
        return;
    }
    if (!Arena->IsPlaying())
        return;
    Rules.Refill(DeltaSeconds);
    if (Rules.Charging && Now >= ReleaseAt)
        Release();
    Animate(DeltaSeconds, Now);
}
