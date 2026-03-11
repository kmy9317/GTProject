#include "GtHeroMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Gigantes/GtGameplayTags.h"
#include "Gigantes/Character/GtHeroCharacter.h"
#include "Gigantes/Player/GtPlayerCameraManager.h"

UGtHeroMovementComponent::UGtHeroMovementComponent()
{
    NavAgentProps.bCanCrouch = true;
    bCanWalkOffLedgesWhenCrouching = true;
 
}

void UGtHeroMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    Super::SetUpdatedComponent(NewUpdatedComponent);

    HeroCharacterOwner = Cast<AGtHeroCharacter>(CharacterOwner);

    // 초기 가져와야할 캐릭터 정보들 캐싱
    if (CharacterOwner)
    {
        CacheInitialValues();
    }
}

void UGtHeroMovementComponent::CacheInitialValues()
{
    if (CharacterOwner && CharacterOwner->GetCapsuleComponent())
    {
        StandingCapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    }

    InitStateProperties();
}

void UGtHeroMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    if (bSprintInputHeld && CanSprintInCurrentState())
    {
        bWantsToSprint = true;
    }
    else
    {
        bWantsToSprint = false;
    }

    if (HeroCharacterOwner)
    {
        // Sprint 시작 체크
        if (!HeroCharacterOwner->bIsSprinting && bWantsToSprint)
        {
            Sprint();
        }
        // Sprint 종료 체크
        else if (HeroCharacterOwner->bIsSprinting &&!bWantsToSprint)
        {
            UnSprint(ESprintEndReason::Manual);
        }
    }

    // 내부적으로 Crouch 활성화 유무 처리
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

float UGtHeroMovementComponent::GetMaxSpeed() const
{
    float BaseSpeed = GetCurrentStateProperties().MaxSpeed;
    
    // Aiming 배율은 모든 상태에 공통 적용
    if (HeroCharacterOwner && HeroCharacterOwner->HasStatusTag(GtGameplayTags::Status_Action_Aiming))
    {
        BaseSpeed *= AimSpeedMultiplier;
    }
    
    return BaseSpeed;
}

float UGtHeroMovementComponent::GetMaxAcceleration() const
{
    float BaseAccel = GetCurrentStateProperties().MaxAcceleration;

    if (HeroCharacterOwner && HeroCharacterOwner->HasStatusTag(GtGameplayTags::Status_Action_Aiming))
    {
        BaseAccel *= AimSpeedMultiplier;
    }

    return BaseAccel;
}

void UGtHeroMovementComponent::InitStateProperties()
{
    // 기본 상태 (Walking)
    DefaultStateProperties.MaxSpeed = MaxWalkSpeed;
    DefaultStateProperties.MaxAcceleration = GetMaxAcceleration();
    DefaultStateProperties.bCanJump = true;
    DefaultStateProperties.bCanCrouch = true;
    DefaultStateProperties.bCanSprint = true;

    // Sprint 상태
    SprintStateProperties.MaxSpeed = SprintMaxSpeed;
    SprintStateProperties.MaxAcceleration = SprintAcceleration;
    SprintStateProperties.bCanJump = true;
    SprintStateProperties.bCanCrouch = true;   // Crouch 입력 → Slide 전환용
    SprintStateProperties.bCanSprint = true;

    // Slide 상태
    FMovementStateProperties SlideProps;
    SlideProps.MaxSpeed = SlideMaxSpeed;
    SlideProps.bCanJump = true;
    SlideProps.bCanCrouch = false;
    SlideProps.bCanSprint = false;
    CustomModePropertiesMap.Add(CMM_Slide, SlideProps);

    // WallRun 상태
    FMovementStateProperties WallRunProps;
    WallRunProps.MaxSpeed = WallRunMaxSpeed;
    WallRunProps.bCanJump = true;
    WallRunProps.bCanCrouch = false;
    WallRunProps.bCanSprint = false;
    CustomModePropertiesMap.Add(CMM_WallRun, WallRunProps);
}
const FMovementStateProperties& UGtHeroMovementComponent::GetCurrentStateProperties() const
{
    // Custom 모드 (Slide, WallRun)
    if (MovementMode == MOVE_Custom)
    {
        if (const FMovementStateProperties* Props = CustomModePropertiesMap.Find(CustomMovementMode))
        {
            return *Props;
        }
    }

    // Sprint 상태
    if (HeroCharacterOwner && HeroCharacterOwner->bIsSprinting)
    {
        return SprintStateProperties;
    }

    return DefaultStateProperties;
}

void UGtHeroMovementComponent::HandleMovementInput(EMovementInput InputType)
{
    // 전역 차단: Dead 상태면 모든 이동 입력 무시
    if (HeroCharacterOwner && HeroCharacterOwner->HasStatusTag(GtGameplayTags::Status_Dead))
    {
        return;
    }

    switch (InputType)
    {
    case EMovementInput::Jump:
        HandleJumpInput();
        break;

    case EMovementInput::Crouch:
        HandleCrouchInput();
        break;

    case EMovementInput::SprintStart:
        HandleSprintInput(true);
        break;

    case EMovementInput::SprintStop:
        HandleSprintInput(false);
        break;
    }
}

void UGtHeroMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
    
    InvalidateGroundInfo();
}

bool UGtHeroMovementComponent::TryEnterWallRun()
{
    // 1. 공중에 있는가?
    if (!IsFalling())
    {
        return false;
    }

    // 2. 충분히 빠른가?
    if (Velocity.Size2D() < WallRunMinSpeed)
    {
        return false;
    }

    // 3. 바닥에서 너무 가깝지 않은가? (바닥에서 바로 월런이 되는 현상 방지)
    FHitResult FloorHit;
    if (GetWorld()->LineTraceSingleByChannel(FloorHit, UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentLocation() + FVector::DownVector * WallRunMinHeight, ECC_Visibility))
    {
        return false; // 최소 높이보다 낮은 곳에 바닥이 감지되면 월런 불가
    }

    // 4. 좌/우 벽 감지
    const FVector Start = UpdatedComponent->GetComponentLocation();
    const FVector Right = UpdatedComponent->GetRightVector();
    const FVector Left = -Right;
    const FVector Forward = UpdatedComponent->GetForwardVector();
    
    FHitResult WallHit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());

    const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(20.f);
    
    bool bWallRunPossible = false;
    bool bIsRightWallLocal = false;
    
    // 오른쪽 벽 감지
    if (GetWorld()->SweepSingleByChannel(WallHit,Start,Start + Right * 75.f,FQuat::Identity,ECC_Visibility,ProbeShape,QueryParams))
    {
        if (FMath::Abs(WallHit.ImpactNormal.Z) < 0.3f)
        {
            if (FVector::DotProduct(Forward, -WallHit.ImpactNormal) > WallRunEntryAngleThreshold)
            {
                WallRunNormal = WallHit.ImpactNormal;
                bIsRightWallLocal = true;
                bWallRunPossible  = true;
            }
        }
    }
    // 왼쪽 벽 감지
    else if (GetWorld()->SweepSingleByChannel(WallHit,Start,Start + Left * 75.f,FQuat::Identity,ECC_Visibility,ProbeShape,QueryParams))
    {
        if (FMath::Abs(WallHit.ImpactNormal.Z) < 0.3f)
        {
            if (FVector::DotProduct(Forward, -WallHit.ImpactNormal) > WallRunEntryAngleThreshold)
            {
                WallRunNormal = WallHit.ImpactNormal;
                bIsRightWallLocal = false;
                bWallRunPossible  = true;
            }
        }
    }

    bIsRightWall = bIsRightWallLocal;
    
    if (bWallRunPossible)
    {
        return true;
    }
    
    return false;
}

void UGtHeroMovementComponent::HandleJumpInput()
{
    // 슬라이딩 중 점프 → 슬라이드 캔슬 + 부스트 점프
    if (IsSliding())
    {
        const FVector PreSlideVelocity = Velocity;
        EndSlide(ESlideEndReason::Jump);

        if (HeroCharacterOwner)
        {
            // CanJump 체크: 지상이거나 더블점프 가능
            if (HeroCharacterOwner->CanJump())
            {
                FVector JumpBoost = PreSlideVelocity.GetSafeNormal2D() * 200.0f;
                JumpBoost.Z = JumpZVelocity;
                HeroCharacterOwner->LaunchCharacter(JumpBoost, false, true);
                HeroCharacterOwner->IncrementJumpCount();
            }
        }
        return;
    }

    // 웅크린 상태에서 점프 → 웅크리기 해제만
    if (HeroCharacterOwner && HeroCharacterOwner->bIsCrouched)
    {
        HeroCharacterOwner->UnCrouch();
        return;
    }

    // 월런 중 점프 → 월 점프킥
    if (IsWallRunning())
    {
        const FVector LaunchVelocity = WallRunNormal * WallRunJumpOffForce
            + FVector::UpVector * JumpZVelocity;
        if (HeroCharacterOwner)
        {
            HeroCharacterOwner->LaunchCharacter(LaunchVelocity, false, true);
            HeroCharacterOwner->IncrementJumpCount();
        }
        EndWallRun();
        return;
    }

    // 일반 점프 / 더블 점프
    if (HeroCharacterOwner && HeroCharacterOwner->CanJump())
    {
        HeroCharacterOwner->Jump();
    }
}

void UGtHeroMovementComponent::StartWallRun(bool bIsRightWallParam)
{
    bIsRightWall = bIsRightWallParam;
    bOrientRotationToMovement = false;
    Velocity.Z = 0;
    SetMovementMode(MOVE_Custom, CMM_WallRun);
}

void UGtHeroMovementComponent::EndWallRun()
{
    WallRunNormal = FVector::ZeroVector;
    bOrientRotationToMovement = true;
    SetMovementMode(MOVE_Falling);
    WallRunEndTime = GetWorld()->GetTimeSeconds();
}

void UGtHeroMovementComponent::HandleCrouchInput()
{
    // 슬라이딩 중 Crouch → 슬라이드 종료
    if (IsSliding())
    {
        EndSlide(ESlideEndReason::CrouchInput);
        return;
    }

    if (!HeroCharacterOwner)
    {
        return;
    }

    // 이미 웅크린 상태 → 해제
    if (HeroCharacterOwner->bIsCrouched)
    {
        HeroCharacterOwner->UnCrouch();
        return;
    }

    // Sprint 중 Crouch → Slide 시도
    if (HeroCharacterOwner->bIsSprinting)
    {
        if (CanSlide())
        {
            StartSlide();
            return;
        }
    }

    // 그 외 → 일반 Crouch
    HeroCharacterOwner->Crouch();
}

void UGtHeroMovementComponent::StartSlide()
{
    if (!CharacterOwner || !CharacterOwner->GetCapsuleComponent())
        return;

    if (HeroCharacterOwner && HeroCharacterOwner->bIsSprinting)
    {
        // Sprint 실행만 종료, 입력 상태(bSprintInputHeld)는 유지
        HeroCharacterOwner->bIsSprinting = false;
        HeroCharacterOwner->OnEndSprint();
    }
    
    SetCapsuleSize(CrouchedHalfHeight);
    SetMovementMode(MOVE_Custom, CMM_Slide);

    // 속도 부스트
    Velocity *= SlideBoostMultiplier;
    
    // 최대 속도 제한
    float CurrentSpeed = Velocity.Size2D();
    if (CurrentSpeed > SlideMaxSpeed)
    {
        Velocity = Velocity.GetSafeNormal2D() * SlideMaxSpeed;
    }

}

void UGtHeroMovementComponent::EndSlide(ESlideEndReason Reason)
{
    if (!CharacterOwner || !CharacterOwner->GetCapsuleComponent())
        return;
    
    // 현재 속도 보존
    FVector PreSlideVelocity = Velocity;
    
    // Reason에 따른 처리
    switch(Reason)
    {
    case ESlideEndReason::Jump:
    case ESlideEndReason::Falling:
        {
            // 공중에 있는 경우 무조건 캡슐 복원 시도
            RestoreCapsuleSize();
            SetMovementMode(MOVE_Falling);
            break;
        }

    case ESlideEndReason::CrouchInput:
    case ESlideEndReason::BrakeInput:
    case ESlideEndReason::Normal:
    case ESlideEndReason::Collision:
        {
            if (false /*/CanStandUp() 조작감 테스트를 통해 활성화 유무 고려 중*/)
            {
                // 일어설 수 있으면 캡슐 복원
                RestoreCapsuleSize();
                SetMovementMode(MOVE_Walking);
            }
            else
            {
                // 일어설 수 없으면 Crouch 상태로 전환
                TransitionToCrouch();
                SetMovementMode(MOVE_Walking);
            }
            break;
        }
    }
    
    // 속도 복원
    Velocity = PreSlideVelocity;
}

bool UGtHeroMovementComponent::CanSlide() const
{
    // 지상에 있고 충분한 속도가 있는지 체크
    if (!IsMovingOnGround())
        return false;
        
    float CurrentSpeed = Velocity.Size2D();
    if (CurrentSpeed < SlideMinSpeed)
        return false;
        
    return true;
}

void UGtHeroMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
    Super::PhysCustom(DeltaTime, Iterations);
    if (CustomMovementMode == CMM_WallRun)
    {
        PhysWallRun(DeltaTime, Iterations);
    }
    else if (CustomMovementMode == CMM_Slide)
    {
        PhysSlide(DeltaTime, Iterations);
    }
}

void UGtHeroMovementComponent::PhysFalling(float DeltaTime, int32 Iterations)
{
    Super::PhysFalling(DeltaTime, Iterations);

    if (ShouldCheckForWallRun())
    {
        CheckForWallRun();
    }
}

void UGtHeroMovementComponent::PhysWallRun(float DeltaTime, int32 Iterations)
{
    const FVector TraceStart = UpdatedComponent->GetComponentLocation();
    const FVector TraceDirection = bIsRightWall ? UpdatedComponent->GetRightVector() : -UpdatedComponent->GetRightVector();

    const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(20.f);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    FHitResult WallHit;

    // 벽 지속 감지
    if (!GetWorld()->SweepSingleByChannel(WallHit, TraceStart, TraceStart + TraceDirection * 90.f, 
        FQuat::Identity, ECC_Visibility, ProbeShape, QueryParams))
    {
        EndWallRun();
        return;
    }

    // 벽 각도 체크
    if (FMath::Abs(WallHit.ImpactNormal.Z) >= 0.2f)
    {
        EndWallRun();
        return;
    }

    WallRunNormal = WallHit.ImpactNormal;
    
    const float CurrentGroundDistance = GetGroundDistance();
    
    // 월런 중 바닥과의 최소 거리 체크
    if (CurrentGroundDistance < WallRunKeepMinHeight)
    {
        // 바닥이 충분히 가까우면 어차피 곧 착지할 것이므로
        // FloorHit 없이 EndWallRun 호출
        EndWallRun();
        return;
    }
    
    // 최소 속도 미달 시 월런 불가
    // 수평 속도가 최소 유지 속도(초기 속도의 60%) 미만이면 종료
    float HorizontalSpeed = FVector(Velocity.X, Velocity.Y, 0).Size();
    const float MinMaintainSpeed = FMath::Max(150.f, WallRunMinSpeed * 0.6f);
    if (HorizontalSpeed < MinMaintainSpeed)
    {
        EndWallRun();
        return;
    }
    
    if (AGtHeroCharacter* Hero = HeroCharacterOwner)
    {
        if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
        {
            if (AGtPlayerCameraManager* CameraManager = Cast<AGtPlayerCameraManager>(PC->PlayerCameraManager))
            {
                const float CurrentAimOffsetYaw = CameraManager->GetAimOffsetYaw();
                
                // 벽을 직접 바라보면 월런 종료
                if (bIsRightWall)
                {
                    // 오른쪽 벽: 왼쪽을 봐야 함
                    if (CurrentAimOffsetYaw < -100.0f || CurrentAimOffsetYaw > 50.0f)
                    {
                        EndWallRun();
                        return;
                    }
                }
                else
                {
                    // 왼쪽 벽: 오른쪽을 봐야 함  
                    if (CurrentAimOffsetYaw < -50.0f || CurrentAimOffsetYaw > 100.0f)
                    {
                        EndWallRun();
                        return;
                    }
                }
            }
        }
    }
    
    // 월런 진행 방향 계산
    FVector WallDirection = FVector::CrossProduct(WallRunNormal, FVector::UpVector).GetSafeNormal();
    if (FVector::DotProduct(UpdatedComponent->GetForwardVector(), WallDirection) < 0.f)
    {
        WallDirection *= -1;
    }

    // 캐릭터의 회전 보간 처리
    if (CharacterOwner && CharacterOwner->Controller)
    {
        // 월런 방향으로의 목표 회전값 계산
        FRotator TargetRotation = WallDirection.Rotation();
        FRotator CurrentRotation = CharacterOwner->GetActorRotation();
        
        // 부드러운 회전을 위한 보간
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, WallRunRotationSpeed);
        
        // Yaw만 적용 (Pitch와 Roll은 0으로 유지)
        NewRotation.Pitch = 0.0f;
        NewRotation.Roll = 0.0f;
        
        CharacterOwner->SetActorRotation(NewRotation);
    }
    
    // 입력 처리
    const FVector InputVector = CharacterOwner ? CharacterOwner->GetLastMovementInputVector().GetSafeNormal() : FVector::ZeroVector;
    const float ForwardInputAmount = FVector::DotProduct(InputVector, WallDirection);
    
    // 가속/감속 적용
    // 월런 방향과 정방향인 Input 방향에 대해 가속 적용
    if (ForwardInputAmount > 0.1f)
    {
        HorizontalSpeed += WallRunAcceleration * ForwardInputAmount * DeltaTime;
        HorizontalSpeed = FMath::Min(HorizontalSpeed, WallRunMaxSpeed);
    }
    // 월런 방향과 반대 방향 Input 또는 Input이 없을때 감속 적용
    else
    {
        HorizontalSpeed = FMath::Max(0.f, HorizontalSpeed - WallRunBrakingDeceleration * DeltaTime);
    }
    
    // 중력 적용 로직: 월런 중 감소된 중력 적용
    // 현재 Z 속도에 스케일된 중력을 누적
    // v = u + at 공식 적용(초기 속도 + 가속도 * 시간)
    float NewZVelocity = Velocity.Z + (GetGravityZ() * WallRunGravityScale * DeltaTime);
    
    // 새로운 속도 벡터 구성
    // 수평 속도와 수직 속도를 독립적으로 계산하여 수직 이동은 오직 중력에 의해서만 결정되도록 유도
    FVector NewVelocity = WallDirection * HorizontalSpeed;
    NewVelocity.Z = NewZVelocity;
    
    // 벽과의 거리 보정
    // 이상적 거리 = 캡슐 반지름 + 오프셋(현재 거리와의 차이를 50%만 즉시 보정하여 부드러운 움직임 표현)
    const float CapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float IdealDistance = CapsuleRadius + WallRunOffsetDistance;
    const float CurrentDistance = FVector::DotProduct(WallHit.ImpactPoint - TraceStart, -WallRunNormal);
    const float DistanceDelta = IdealDistance - CurrentDistance;
    
    // 보정을 Delta에만 적용 (Velocity에는 영향 없음)
    const FVector CorrectionDelta = WallRunNormal * DistanceDelta * 0.5f; // 50% 보정(property화 가능)
    
    // 이동 적용
    const FVector MoveDelta = NewVelocity * DeltaTime + CorrectionDelta;

    // 충돌 검사를 포함하여 안전하게 캐릭터 이동
    FHitResult Hit;
    SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
    
    // Hit 처리
    if (Hit.bBlockingHit)
    {
        // 중간에 장애물 있을시 미끄러지듯 내려가도록 처리
        SlideAlongSurface(MoveDelta, 1.f - Hit.Time, Hit.Normal, Hit, true);
    }

    // 다음 프레임을 위한 속도 저장
    // Z 속도가 제대로 보존되도록 유도
    Velocity = NewVelocity;
}

void UGtHeroMovementComponent::PhysSlide(float DeltaTime, int32 Iterations)
{
    if (!CharacterOwner)
    {
        EndSlide(ESlideEndReason::Normal);
        return;
    }
    
    // Custom Mode에서는 CurrentFloor를 수동으로 업데이트 필요
    FFindFloorResult FloorResult;
    FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
    CurrentFloor = FloorResult;
    
    // 이제 CurrentFloor 사용 가능
    if (!CurrentFloor.IsWalkableFloor())
    {
        EndSlide(ESlideEndReason::Falling);
        return;
    }

    // 현재 속도
    float CurrentSpeed = Velocity.Size2D();
    FVector CurrentDirection = Velocity.GetSafeNormal2D();
    
    const FVector InputVector = CharacterOwner->GetLastMovementInputVector();
    if (!InputVector.IsNearlyZero())
    {
        FVector InputDir = InputVector.GetSafeNormal();
        float InputAlignment = FVector::DotProduct(CurrentDirection, InputDir);
    
        // 전방 콘 범위 체크
        const float ForwardConeThreshold = FMath::Cos(FMath::DegreesToRadians(SlideForwardConeAngle));
    
        if (InputAlignment < ForwardConeThreshold)
        {
            // 후방/측면 입력 - 슬라이드 종료
            EndSlide(ESlideEndReason::BrakeInput);
            return;
        }
        else if (SlideSteeringResponsiveness > 0.01f)  // 조향이 활성화된 경우만
        {
            // 전방 입력 - 조향 적용
            float AngleDiff = FMath::Acos(InputAlignment);
        
            // 최대 조향 속도 - 프로퍼티와 반응성 적용
            const float AdjustedSteeringRate = SlideMaxSteeringRate * SlideSteeringResponsiveness;
            const float MaxSteeringRadians = FMath::DegreesToRadians(AdjustedSteeringRate) * DeltaTime;
        
            // 실제 적용할 조향량 계산
            float SteeringAmount = FMath::Min(AngleDiff, MaxSteeringRadians);
        
            // 조향 적용
            if (SteeringAmount > 0.001f)
            {
                // 회전 방향 결정 (외적으로 좌/우 판단)
                FVector CrossProduct = FVector::CrossProduct(CurrentDirection, InputDir);
                float RotationDirection = FMath::Sign(CrossProduct.Z);
            
                // 쿼터니언으로 회전 적용
                FQuat RotationQuat(FVector::UpVector, SteeringAmount * RotationDirection);
                CurrentDirection = RotationQuat.RotateVector(CurrentDirection);
            }
        }
    }
    
    // 바닥 정보 사용
    const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;
    const float SlopeAngle = FMath::RadiansToDegrees(
        FMath::Acos(FVector::DotProduct(FloorNormal, FVector::UpVector)));
    
    // 경사면 처리
    if (SlopeAngle > 5.0f && SlopeAngle < GetWalkableFloorAngle())
    {
        // 경사 방향 계산
        FVector GravityDir = FVector(0, 0, -1);
        FVector RampDirection = (GravityDir - FloorNormal * 
            FVector::DotProduct(GravityDir, FloorNormal)).GetSafeNormal();
        
        // 경사 가속
        float SlopeAcceleration = FMath::Abs(GetGravityZ()) * SlideGravityScale * 
            FMath::Sin(FMath::DegreesToRadians(SlopeAngle));
        
        // 속도 증가
        CurrentSpeed += SlopeAcceleration * DeltaTime;
        
        // 방향을 경사면 방향으로 조정
        CurrentDirection = FMath::Lerp(CurrentDirection, RampDirection, 0.5f).GetSafeNormal();
    }
    else
    {
        // 평지 감속
        CurrentSpeed = FMath::Max(0.0f, CurrentSpeed - SlideBrakingDeceleration * DeltaTime);
        
        // 최소 속도 체크
        if (CurrentSpeed < SlideMinExitSpeed)
        {
            EndSlide(ESlideEndReason::Normal);
            return;
        }
    }
    
    // 속도 제한
    CurrentSpeed = FMath::Clamp(CurrentSpeed, 0.0f, SlideMaxSpeed);
    
    // 속도 설정
    Velocity = CurrentDirection * CurrentSpeed;
    Velocity.Z = 0;

    // 이동 적용 전 속도 저장
    const FVector PreMoveVelocity = Velocity;
    const FVector Delta = Velocity * DeltaTime;
    FHitResult Hit;
    SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
    
    if (Hit.bBlockingHit)
    {
        // 충돌 각도 계산 (이동 방향 vs 충돌면)
        const FVector MoveDirection = Delta.GetSafeNormal();
        const float ImpactAngle = FVector::DotProduct(MoveDirection, Hit.Normal);
        
        // 정면 충돌 판정
        const float FrontCollisionThreshold = -0.5f;  // cos(120도) = -0.5
        
        if (ImpactAngle < FrontCollisionThreshold)
        {
            // 정면 충돌 - 즉시 종료
            EndSlide(ESlideEndReason::Collision);
            
            // 충돌 시 속도 감소
            Velocity = PreMoveVelocity * 0.3f;  // 30%로 감속
        }
        else
        {
            // 측면 충돌 - 미끄러짐
            SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
            
            // 미끄러진 후 속도가 너무 낮아졌는지 체크
            if (Velocity.Size2D() < SlideMinExitSpeed * 0.5f)
            {
                EndSlide(ESlideEndReason::Collision);
            }
        }
    }
}

void UGtHeroMovementComponent::HandleSprintInput(bool bPressed)
{
    if (bPressed)
    {
        // Crouch 상태에서 Sprint 시작 시 먼저 Crouch 해제
        if (HeroCharacterOwner && HeroCharacterOwner->bIsCrouched)
        {
            HeroCharacterOwner->UnCrouch();
        }
        SetSprintInput(true);
    }
    else
    {
        SetSprintInput(false);
    }
}

void UGtHeroMovementComponent::Sprint()
{
    if (!HeroCharacterOwner)
        return;

    HeroCharacterOwner->bIsSprinting = true;
    HeroCharacterOwner->OnStartSprint();
}

void UGtHeroMovementComponent::UnSprint(ESprintEndReason Reason)
{
    if (!HeroCharacterOwner)
        return;
    
    // 종료 이유별 처리
    switch(Reason)
    {
    case ESprintEndReason::WrongDirection:
        Velocity *= 0.8f;  // 방향 전환 시 감속
        break;
            
    case ESprintEndReason::Collision:
        Velocity *= 0.5f;  // 충돌 시 더 많은 감속
        break;
    default:
        break;
    }

    HeroCharacterOwner->bIsSprinting = false;
    HeroCharacterOwner->OnEndSprint();
}

bool UGtHeroMovementComponent::CanSprintInCurrentState() const
{
    if (!GetCurrentStateProperties().bCanSprint)
    {
        return false;
    }

    // Sprint 상태였으면 공중에서도 우선 조건 통과 (착지 시 자동 재개를 위함)
    // 새로 시작할 때만 지상 체크
    if (!IsMovingOnGround())
    {
        // 이미 Sprint 중이었다면 유지 가능
        if (HeroCharacterOwner && HeroCharacterOwner->bIsSprinting)
            return true;
        
        return false;  // 새로 시작은 불가
    }

    // 최소 속도 체크
    if (!HeroCharacterOwner->bIsSprinting && Velocity.Size2D() < SprintMinSpeed)
    {
        return false;
    }
    
    const FVector InputVector = CharacterOwner->GetLastMovementInputVector();
    if (InputVector.IsNearlyZero())
    {
        return false;
    }

    // 플레이어의 현재 카메라 방향과 입력 방향에 대해 체크
    const FRotator CameraRotation = CharacterOwner->GetControlRotation();
    const FRotator YawRotation(0, CameraRotation.Yaw, 0);
    const FVector CameraForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector MovementDirection = InputVector.GetSafeNormal();

    const float CameraAlignment = FVector::DotProduct(CameraForward, MovementDirection);
    const float CameraThreshold = FMath::Cos(FMath::DegreesToRadians(SprintForwardAngleThreshold)); 

    if (CameraAlignment < CameraThreshold)
    {
        // 플레이어의 입력이 카메라 기준 전방이 아니면 질주 불가
        return false;
    }

    // 새로 Sprint 시작시 캐릭터가 이동 방향을 바라보는지 체크
    if (HeroCharacterOwner && !HeroCharacterOwner->bIsSprinting)
    {
        const FVector ActorForward = CharacterOwner->GetActorForwardVector();
        const float CharacterAlignment = FVector::DotProduct(ActorForward, MovementDirection);

        // 캐릭터가 이동하려는 방향을 아직 바라보고 있지 않다면
        // 먼저 회전해야 하므로 sprint 불가
        if (CharacterAlignment < SprintStartAlignmentThreshold)
        {
            return false;
        }
    }

    return true;
}

bool UGtHeroMovementComponent::CanStandUp() const
{
    if (!CharacterOwner)
        return false;

    if (!bWantsToCrouch && !CharacterOwner->bIsCrouched)
        return true;
    
    // 일어설 수 있는 공간이 있는지 체크
    const float CurrentCrouchedHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float StandingHalfHeight = StandingCapsuleHalfHeight * CharacterOwner->GetCapsuleComponent()->GetShapeScale();
    
    const FVector PawnLocation = CharacterOwner->GetActorLocation();
    const float HeightAdjust = StandingHalfHeight - CurrentCrouchedHalfHeight;
    
    // 위쪽 공간 체크
    FVector TraceStart = PawnLocation;
    FVector TraceEnd = PawnLocation + FVector(0.f, 0.f, HeightAdjust * 2.0f);
    
    FCollisionQueryParams QueryParams(NAME_None, false, CharacterOwner);
    FCollisionResponseParams ResponseParam;
    InitCollisionParams(QueryParams, ResponseParam);

    FHitResult Hit;
    bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        UpdatedComponent->GetCollisionObjectType(),
        QueryParams,
        ResponseParam
    );
    
    return !bBlocked;
}

bool UGtHeroMovementComponent::ShouldCheckForWallRun() const
{
    if (IsWallRunning())
        return false;
    
    // 월런 종료시 쿨다운 체크
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - WallRunEndTime < WallRunCooldownTime)
        return false;
    
    // 일정 주기 제한
    if (CurrentTime - LastWallRunCheckTime < WallRunCheckInterval)
        return false;
    
    return true;
}

void UGtHeroMovementComponent::CheckForWallRun()
{
    LastWallRunCheckTime = GetWorld()->GetTimeSeconds();
    
    if (TryEnterWallRun())
    {
        StartWallRun(bIsRightWall);
    }
}

void UGtHeroMovementComponent::SetCapsuleSize(float TargetHalfHeight)
{
    UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
    if (!CapsuleComp)
        return;
    
    const float ComponentScale = CapsuleComp->GetShapeScale();
    const float OldUnscaledHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
    const float OldUnscaledRadius = CapsuleComp->GetUnscaledCapsuleRadius();
    
    // 크기 변경
    CapsuleComp->SetCapsuleSize(OldUnscaledRadius, TargetHalfHeight);
    float HalfHeightAdjust = (OldUnscaledHalfHeight - TargetHalfHeight);
    float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
    
    // 캡슐 위치 조정
    if (FMath::Abs(ScaledHalfHeightAdjust) > 0.01f)
    {
        UpdatedComponent->MoveComponent(
            FVector(0.f, 0.f, -ScaledHalfHeightAdjust), 
            UpdatedComponent->GetComponentQuat(), 
            true, nullptr, 
            EMoveComponentFlags::MOVECOMP_NoFlags, 
            ETeleportType::TeleportPhysics);
    }
    
    // 캡슐 크기 변경 델리게이트 호출
    OnCapsuleSizeChanged.ExecuteIfBound(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void UGtHeroMovementComponent::RestoreCapsuleSize()
{
    SetCapsuleSize(StandingCapsuleHalfHeight);
}

void UGtHeroMovementComponent::TransitionToCrouch()
{
    // MovementComponent의 Crouch 플래그 설정(다음 업데이트 때 Crouch 상태로 변경)
    bWantsToCrouch = true;
    
    if (HeroCharacterOwner)
    {
        // 여기서 캐릭터의 Crouch 상태를 직접 설정하여 crouch 진입에 대한 메쉬 offset 조정 로직 실행 방지
        HeroCharacterOwner->bIsCrouched = true;
    }
}

float UGtHeroMovementComponent::GetGroundDistance()
{
    // 이미 이번 프레임에 계산했으면 캐시 반환
    if (GFrameCounter == CachedGroundInfoFrame)
    {
        return CachedGroundDistance;
    }
    
    // 새로 계산
    CachedGroundDistance = CalculateGroundDistance();
    CachedGroundInfoFrame = GFrameCounter;
    
    return CachedGroundDistance;
}

float UGtHeroMovementComponent::CalculateGroundDistance() const
{
    if (!CharacterOwner)
    {
        return 0.0f;
    }
    
    // 땅에 있으면 0
    if (IsMovingOnGround())
    {
        return 0.0f;
    }
    
    // 공중에 있을 때만 거리 계산
    const UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
    if (!CapsuleComp)
    {
        return GroundTraceDistance;
    }
    
    const float CapsuleHalfHeight = CapsuleComp->GetScaledCapsuleHalfHeight();
    const FVector TraceStart = CharacterOwner->GetActorLocation();
    const FVector TraceEnd = TraceStart - FVector(0, 0, GroundTraceDistance + CapsuleHalfHeight);
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams(NAME_None, false, CharacterOwner);
    QueryParams.bReturnPhysicalMaterial = false;
    
    // 바닥 체크
    if (GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams))
    {
        // 바닥을 찾았으면 거리 계산
        return FMath::Max(0.0f, HitResult.Distance - CapsuleHalfHeight);
    }
    
    // 바닥을 못 찾았으면 최대 거리
    return GroundTraceDistance;
}

void UGtHeroMovementComponent::InvalidateGroundInfo()
{
    CachedGroundInfoFrame = 0;
}
