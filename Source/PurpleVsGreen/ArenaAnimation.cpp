#include "ArenaSoldier.h"

#include "ArenaMatch.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
struct FCastGesture
{
    float Reach;
    float Gather;
    float Spread;
    float Lift;
    float Roll;
    float Support;
};

constexpr FCastGesture Gestures[] = {
    {8.f, 2.f, 1.f, 4.f, 13.f, .16f},
    {13.f, 7.f, 12.f, 6.f, 24.f, .78f},
    {17.f, 11.f, -6.f, 8.f, -12.f, .86f},
    {15.f, 13.f, 4.f, 14.f, 28.f, 1.f},
};
static_assert(UE_ARRAY_COUNT(Gestures) == PvG::Index(PvG::Spell::Count));

float Smooth(float Value)
{
    const float T = FMath::Clamp(Value, 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
}
}

void AArenaSoldier::Animate(float DeltaSeconds, double Now)
{
    const FVector Velocity = GetVelocity();
    const FVector LocalVelocity = GetActorQuat().UnrotateVector(FVector(Velocity.X, Velocity.Y, 0));
    const float Speed = LocalVelocity.Size2D();
    const float Pace = FMath::Clamp(Speed / 590.f, 0.f, 1.f);
    const float Cadence = FMath::GetMappedRangeValueClamped(FVector2f(35.f, 820.f), FVector2f(1.72f, 4.7f), Speed);
    const bool Airborne = GetCharacterMovement()->IsFalling();
    if (!Airborne && Speed > 18.f)
        Stride = FMath::Fmod(Stride + DeltaSeconds * Cadence * .5f, 1.f);
    if (WasAirborne && !Airborne)
        Landing = 1.f;
    WasAirborne = Airborne;
    Landing = FMath::Max(0.f, Landing - DeltaSeconds * 4.6f);
    AirPose = FMath::FInterpTo(AirPose, Airborne ? 1.f : 0.f, DeltaSeconds, 13.f);

    const float CastAge = static_cast<float>(Now - LastCast);
    const float Hit = FMath::Exp(-FMath::Max(0.f, static_cast<float>(Now - LastHit)) * 12.f);
    const FCastGesture& Gesture = Gestures[PvG::Index(Rules.ActiveSpell)];
    const float Windup = PvG::Arsenal[PvG::Index(Rules.ActiveSpell)].Windup;
    const float Gather = Rules.Charging && Windup > 0.f ? Smooth(1.f - static_cast<float>(ReleaseAt - Now) / Windup) : 0.f;
    const float Reach = FMath::Exp(-FMath::Max(0.f, CastAge) * 6.5f);
    const float ReleasePulse = Reach * (1.f - FMath::Exp(-FMath::Max(0.f, CastAge) * 51.f));
    PoseCast = FMath::FInterpTo(PoseCast, Rules.Charging ? .30f + Gather * .7f : Reach,
        DeltaSeconds, Rules.Charging ? 24.f : 30.f);
    PoseWard = FMath::FInterpTo(PoseWard, Now < Rules.WardUntil ? 1.f : 0.f, DeltaSeconds, 16.f);

    const FRotator View = Controller ? Controller->GetControlRotation() : Aim;
    UpdateArmorPose(LocalVelocity, Cadence, Pace, AirPose, Hit, FRotator::NormalizeAxis(View.Pitch));
    if (Speed > 150.f && !Airborne && Now >= LastStep)
    {
        Arena->Feedback(GetActorLocation(), 8, .20f + .045f * Pace);
        LastStep = Now + 1.f / Cadence;
    }

    FVector2D Turn = FVector2D::ZeroVector;
    if (HasViewHistory)
    {
        const FRotator Delta = (View - LastView).GetNormalized();
        Turn = {FMath::Clamp(Delta.Yaw, -8., 8.), FMath::Clamp(Delta.Pitch, -7., 7.)};
    }
    HasViewHistory = true;
    LastView = View;
    ViewSway = FMath::Vector2DInterpTo(ViewSway, Turn, DeltaSeconds, 12.f);

    const float Cycle = Stride * 2.f * UE_PI;
    const float Bob = FMath::Cos(Cycle * 2.f) * Pace * (1.f - AirPose);
    const float Strafe = FMath::Clamp(LocalVelocity.Y / 590., -1., 1.);
    const float Breath = FMath::Sin(Now * 2.65) * .28f * (1.f - Pace);
    CastingHands->SetRelativeLocation({
        ReleasePulse * Gesture.Reach - Gather * Gesture.Gather - 3.5f * AirPose - 2.f * Hit,
        FMath::Sin(Cycle) * Pace * 1.15f - ViewSway.X * .44f - Strafe * 1.8f,
        Breath + Bob * 1.3f + PoseWard * 3.3f - Landing * 4.3f - ViewSway.Y * .23f
    });
    CastingHands->SetRelativeRotation(FRotator(
        -ViewSway.Y * .9f - Landing * 6.f + Hit * 2.f,
        -ViewSway.X * .7f, Bob * 1.8f + Strafe * 2.6f + HitSide * Hit * 4.f));

    CastingRight->SetRelativeLocation({61.f + PoseCast * 3.f, 23.f + PoseCast * Gesture.Spread,
        -25.f + PoseCast * Gesture.Lift * .56f - AirPose * 1.4f});
    CastingLeft->SetRelativeLocation({55.f + PoseCast * Gesture.Support * 6.f,
        -24.f - PoseCast * Gesture.Spread * Gesture.Support + PoseWard * 5.2f,
        -28.f + PoseCast * Gesture.Lift * Gesture.Support + PoseWard * 11.f});
    CastingRight->SetRelativeRotation(FRotator(5.f + Gather * 15.f - ReleasePulse * 9.f,
        -18.f + PoseCast * 8.f, 12.f + PoseCast * Gesture.Roll));
    CastingLeft->SetRelativeRotation(FRotator(7.f + PoseWard * 32.f + PoseCast * Gesture.Support * 15.f,
        17.f - PoseCast * Gesture.Support * 14.f - PoseWard * 16.f, -12.f - PoseWard * 26.f));
    for (int Index = 0; Index < Digits.Num(); ++Index)
    {
        const bool Right = Index >= 4;
        const float Spread = Right ? PoseCast : FMath::Max(PoseWard, PoseCast * Gesture.Support);
        const int Finger = Index % 4;
        Digits[Index]->SetRelativeRotation(FRotator(-22.f - Finger * 4.f + Spread * (28.f + Finger * 4.f),
            (Finger - 1.5f) * (2.6f + Spread * 5.f) * (Right ? 1.f : -1.f), 0.f));
    }

    Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,
        Rules.Charging && Rules.ActiveSpell == PvG::Spell::Lance ? 74.f : (Speed > 635.f ? 102.f : 96.f),
        DeltaSeconds, 10.f));
}

void AArenaSoldier::UpdateArmorPose(FVector LocalVelocity, float Cadence, float Pace, float Flight, float Hit, float AimPitch)
{
    const USkeletalMesh* Body = Cast<USkeletalMesh>(Battlemage->GetSkinnedAsset());
    if (!Body || Battlemage->BoneSpaceTransforms.Num() != ComponentReference.Num())
        return;

    const FReferenceSkeleton& Skeleton = Body->GetRefSkeleton();
    const TArray<FTransform>& Reference = Skeleton.GetRefBonePose();
    TArray<FTransform>& Pose = Battlemage->BoneSpaceTransforms;
    Pose = Reference;
    const auto Rotate = [&](FName Name, const FRotator& Angles)
    {
        const int Bone = Skeleton.FindBoneIndex(Name);
        if (Bone == INDEX_NONE)
            return;
        const FQuat Basis = ComponentReference[Bone].GetRotation();
        const FQuat Delta = Basis.Inverse() * Angles.Quaternion() * Basis;
        Pose[Bone].SetRotation(Reference[Bone].GetRotation() * Delta);
    };

    const float Cycle = Stride * 2.f * UE_PI;
    const float Swing = FMath::Sin(Cycle) * Pace;
    const float Strafe = FMath::Clamp(LocalVelocity.Y / 590., -1., 1.);
    const float Forward = FMath::Clamp(LocalVelocity.X / 590., -1., 1.);
    const float Follow = Gestures[PvG::Index(Rules.ActiveSpell)].Support;
    Rotate(TEXT("Pelvis"), FRotator(-Forward * 7.f, Swing * 7.f, Strafe * 6.f));
    Rotate(TEXT("Chest"), FRotator(-Forward * 5.f + Hit * 13.f, -Swing * 11.f + PoseCast * 9.f,
        -Strafe * 5.f + HitSide * Hit * 13.f));
    Rotate(TEXT("Head"), FRotator(AimPitch * .54f + Forward * 6.f - Hit * 5.f, Swing * 3.f, -HitSide * Hit * 4.f));
    Rotate(TEXT("UpperArm_R"), FRotator(12.f - Swing * 24.f + PoseCast * (58.f + AimPitch * .54f),
        -9.f + PoseCast * 5.f, -4.f - PoseCast * 8.f));
    Rotate(TEXT("UpperArm_L"), FRotator(14.f + Swing * 25.f + PoseWard * 61.f + PoseCast * Follow * 26.f,
        7.f - PoseWard * 17.f, 4.f + PoseWard * 16.f));
    Rotate(TEXT("Forearm_R"), FRotator(18.f + PoseCast * 26.f, 0, 0));
    Rotate(TEXT("Forearm_L"), FRotator(23.f + PoseWard * 49.f + PoseCast * Follow * 15.f, 0, 0));
    Rotate(TEXT("Hand_R"), FRotator(PoseCast * -13.f, PoseCast * 8.f, -PoseCast * 10.f));
    Rotate(TEXT("Hand_L"), FRotator(PoseWard * 12.f, -PoseWard * 9.f, PoseWard * 13.f));

    const int HipBody = Skeleton.FindBoneIndex(TEXT("Pelvis"));
    if (HipBody != INDEX_NONE)
    {
        const int Parent = Skeleton.GetParentIndex(HipBody);
        const double Bob = -Pace * (6. + 2.2 * FMath::Cos(Cycle * 2.f)) - Landing * 7.;
        Pose[HipBody].AddToTranslation(ComponentReference[Parent].GetRotation().UnrotateVector({0, Swing * 1.7f, Bob}));
    }
    for (int Bone = 0; Bone < Pose.Num(); ++Bone)
    {
        const int Parent = Skeleton.GetParentIndex(Bone);
        PoseComponents[Bone] = Parent == INDEX_NONE ? Pose[Bone] : Pose[Bone] * PoseComponents[Parent];
    }

    const FVector Step = (LocalVelocity * (FMath::Lerp(.62f, .36f, Pace) / Cadence)).GetClampedToMaxSize(53.);
    const double Stance = FMath::Lerp(.62, .36, static_cast<double>(Pace));
    const FTransform BodyTransform = Battlemage->GetComponentTransform();
    const auto Plant = [&](FName ThighName, FName ShinName, FName FootName, double Offset)
    {
        const int Thigh = Skeleton.FindBoneIndex(ThighName);
        const int Shin = Skeleton.FindBoneIndex(ShinName);
        const int Foot = Skeleton.FindBoneIndex(FootName);
        if (Thigh == INDEX_NONE || Shin == INDEX_NONE || Foot == INDEX_NONE)
            return;

        const FVector RefHip = ComponentReference[Thigh].GetTranslation();
        const FVector RefKnee = ComponentReference[Shin].GetTranslation();
        const FVector RefAnkle = ComponentReference[Foot].GetTranslation();
        const double Phase = FMath::Fmod(Stride + Offset, 1.);
        const bool Swinging = Phase >= Stance;
        const float LiftPhase = Swinging ? static_cast<float>((Phase - Stance) / (1. - Stance)) : 0.f;
        const double Travel = Swinging ? -1. + 2. * Smooth(LiftPhase) : 1. - 2. * Phase / Stance;
        const double Lift = FMath::Sin(LiftPhase * UE_PI) * Pace * 27.;
        FVector Ankle = RefAnkle + Step * Travel * (1. - Flight);
        const double Tuck = Offset > 0. ? 17. : 28.;
        Ankle.X += (Offset > 0. ? -12. : 25.) * Flight;
        Ankle.Z += Lift * (1. - Flight) + Tuck * Flight;

        const FVector Hip = PoseComponents[Thigh].GetTranslation();
        const double Upper = FVector::Dist(RefHip, RefKnee);
        const double Lower = FVector::Dist(RefKnee, RefAnkle);
        if (Flight < .05f)
        {
            const FVector FootWorld = BodyTransform.TransformPosition(FVector(Ankle.X, Ankle.Y, 0));
            FHitResult Floor;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(PvGFoot), false, this);
            if (GetWorld()->LineTraceSingleByObjectType(Floor, FootWorld + FVector(0, 0, 20),
                FootWorld - FVector(0, 0, 31), FCollisionObjectQueryParams(ECC_WorldStatic), Query))
                Ankle.Z += FMath::Clamp(Floor.ImpactPoint.Z - FootWorld.Z, -15., 13.);
        }

        const FVector Axis = (Ankle - Hip).GetSafeNormal(UE_SMALL_NUMBER, FVector(0, 0, -1));
        const double Distance = FMath::Clamp(FVector::Dist(Hip, Ankle), FMath::Abs(Upper - Lower) + 1., Upper + Lower - .65);
        const double Along = (Upper * Upper - Lower * Lower + Distance * Distance) / (2. * Distance);
        const double KneeBend = FMath::Sqrt(FMath::Max(0., Upper * Upper - Along * Along));
        const FVector KneeForward = FVector::VectorPlaneProject(FVector::ForwardVector, Axis).GetSafeNormal();
        const FVector Knee = Hip + Axis * Along + KneeForward * KneeBend;
        const FVector End = Hip + Axis * Distance;
        const FQuat ThighTurn = FQuat::FindBetweenVectors(RefKnee - RefHip, Knee - Hip) * ComponentReference[Thigh].GetRotation();
        const FQuat ShinTurn = FQuat::FindBetweenVectors(RefAnkle - RefKnee, End - Knee) * ComponentReference[Shin].GetRotation();
        const FQuat ParentTurn = PoseComponents[Skeleton.GetParentIndex(Thigh)].GetRotation();
        const double Toe = (Swinging ? FMath::Sin(LiftPhase * UE_PI * 2.) * 15. * Pace : 0.) - Flight * 15.;
        const FQuat FootTurn = FRotator(Toe, 0., 0.).Quaternion() * ComponentReference[Foot].GetRotation();
        Pose[Thigh].SetRotation(ParentTurn.Inverse() * ThighTurn);
        Pose[Shin].SetRotation(ThighTurn.Inverse() * ShinTurn);
        Pose[Foot].SetRotation(ShinTurn.Inverse() * FootTurn);
    };
    Plant(TEXT("Thigh_R"), TEXT("Shin_R"), TEXT("Foot_R"), 0.);
    Plant(TEXT("Thigh_L"), TEXT("Shin_L"), TEXT("Foot_L"), .5);
    Battlemage->MarkRefreshTransformDirty();
}

void AArenaSoldier::AnimateDefeat(double Now)
{
    if (IsHidden())
        return;
    const float Progress = static_cast<float>((Now - LastHit) / .54);
    if (Progress >= 1.f)
    {
        SetActorHiddenInGame(true);
        return;
    }

    const float Fall = Smooth(Progress);
    const float Side = HitSide < -.1f ? -1.f : 1.f;
    Battlemage->SetRelativeRotation(FRotator(Fall * -22.f, Fall * 14.f * Side, Fall * 71.f * Side));
    Battlemage->SetRelativeLocation({-19.f * Fall, 15.f * Fall * Side, -90.f - 8.f * Fall});
}
