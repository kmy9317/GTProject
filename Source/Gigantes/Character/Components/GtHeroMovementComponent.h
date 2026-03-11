// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GtHeroMovementComponent.generated.h"

class AGtHeroCharacter;

UENUM()
enum class EMovementInput : uint8
{
	Jump,
	Crouch,
	SprintStart,
	SprintStop,
};

USTRUCT()
struct FMovementStateProperties
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	float MaxSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly)
	float MaxAcceleration = 2048.f;

	UPROPERTY(EditDefaultsOnly)
	bool bCanJump = true;

	UPROPERTY(EditDefaultsOnly)
	bool bCanCrouch = true;

	UPROPERTY(EditDefaultsOnly)
	bool bCanSprint = true;
};

DECLARE_DELEGATE_OneParam(FOnSlideStateChanged, bool /*bIsSliding*/);

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMM_None UMETA(DisplayName = "None"),
	CMM_WallRun UMETA(DisplayName = "WallRun"),
	CMM_Slide UMETA(DisplayName = "Slide")
};

UENUM(BlueprintType)
enum class ESlideEndReason : uint8
{
	Normal        UMETA(DisplayName = "Normal"),       // 속도 감소 등 자연스러운 종료
	Jump          UMETA(DisplayName = "Jump"),         // 점프로 인한 종료  
	Falling          UMETA(DisplayName = "Falling"),         // 낙하로 인한 종료  
	CrouchInput   UMETA(DisplayName = "CrouchInput"),  // Crouch 입력으로 인한 종료
	BrakeInput      UMETA(DisplayName = "Brake Input"),      // 브레이크(후방/측면) 입력
	Collision     UMETA(DisplayName = "Collision")     // 충돌로 인한 종료
};

UENUM(BlueprintType)
enum class ESprintEndReason : uint8
{
	Manual         UMETA(DisplayName = "Manual"),           // 사용자가 키를 뗌
	SpeedTooLow    UMETA(DisplayName = "Speed Too Low"),    // 속도 부족
	WrongDirection UMETA(DisplayName = "Wrong Direction"),  // 잘못된 방향
	StateChange    UMETA(DisplayName = "State Change"),     // 다른 상태로 전환
	Collision      UMETA(DisplayName = "Collision"),        // 충돌
};

DECLARE_DELEGATE_TwoParams(FOnCapsuleSizeChanged, float /*HalfHeightAdjust*/, float /*ScaledHalfHeightAdjust*/);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GIGANTES_API UGtHeroMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UGtHeroMovementComponent();

	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;

	const FMovementStateProperties& GetCurrentStateProperties() const;

	void HandleMovementInput(EMovementInput InputType);
	
	void StartWallRun(bool bIsRightWall);
	void EndWallRun();
	bool TryEnterWallRun(); 
	bool IsWallRunning() const { return MovementMode == MOVE_Custom && CustomMovementMode == CMM_WallRun; }
	bool IsWallRunningRight() const { return IsWallRunning() && bIsRightWall; }

	void StartSlide();
	void EndSlide(ESlideEndReason Reason = ESlideEndReason::Normal);
	bool CanSlide() const;
	bool CanStandUp() const;
	bool IsSliding() const { return MovementMode == MOVE_Custom && CustomMovementMode == CMM_Slide; }

	void Sprint();
	void UnSprint(ESprintEndReason Reason = ESprintEndReason::Manual);
	bool CanSprintInCurrentState() const;
	void SetSprintInput(const bool bIsPressed) { bSprintInputHeld = bIsPressed; }
	bool IsSprintInputHeld() const { return bSprintInputHeld; }

	uint8 GetCustomMovementMode() const { return CustomMovementMode; }
	
	UFUNCTION(BlueprintPure, Category = "Movement|WallRun")
	FVector GetWallRunNormal() const { return WallRunNormal; }

	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetGroundDistance();

	void InvalidateGroundInfo();

	FOnCapsuleSizeChanged OnCapsuleSizeChanged;
	
protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;
	void PhysWallRun(float DeltaTime, int32 Iterations);
	void PhysSlide(float DeltaTime, int32 Iterations);

private:

	void InitStateProperties();

	void HandleJumpInput();
	void HandleCrouchInput();
	void HandleSprintInput(bool bPressed);

	void CheckForWallRun();
	bool ShouldCheckForWallRun() const;
	
	void SetCapsuleSize(float TargetHalfHeight);
	void RestoreCapsuleSize();
	void TransitionToCrouch();
	float CalculateGroundDistance() const;
	void CacheInitialValues();
	
public:

	UPROPERTY(EditDefaultsOnly, Category = "Movement|StateProperties")
	FMovementStateProperties DefaultStateProperties;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|StateProperties")
	FMovementStateProperties SprintStateProperties;

	UPROPERTY()
	TMap<uint8, FMovementStateProperties> CustomModePropertiesMap;

	/**
	 * WallRun 관련 속성들
	 */
	
	// 월런 시작을 위한 최소 수평 속도
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunMinSpeed = 500.f; 
 
	// 월런을 시작하기 위한 최소 공중 높이 (바닥에서 바로 타는 것 방지)
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunMinHeight = 50.f; 

	// 월런을 시작하기 위한 최소 공중 높이 (바닥에서 바로 타는 것 방지)
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunKeepMinHeight = 10.f; 
	
	// 월런 중 적용될 중력 배율
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunGravityScale = 0.2f; 
 
	// 월 점프 시 벽에서 옆으로 밀어내는 힘
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunJumpOffForce = 500.f; 

	// 월런 중 최대 속도
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunMaxSpeed = 800.f;

	// 월런 중 가속도
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunAcceleration = 2000.f;
	
	// 월런 감속(마찰) 값 - 초당 몇 cm/s 줄일지
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunBrakingDeceleration = 800.f;

	// 캐릭터 캡슐과 벽 사이에 유지할 거리(여유 공간)
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunOffsetDistance = 5.f;

	// 월런 중 캐릭터 회전 속도
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunRotationSpeed = 10.0f;

	// 월런 쿨다운 시간
	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun")
	float WallRunCooldownTime = 0.3f;  // 0.5초 쿨다운

	UPROPERTY(EditDefaultsOnly, Category = "Movement|WallRun", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float WallRunEntryAngleThreshold = 0.1f;

	/**
	 * Slide 관련 속성들
	 */
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
	float SlideMinSpeed = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
	float SlideMaxSpeed = 1000.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
	float SlideGravityScale = 2.0f;  // 경사면 가속

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
	float SlideMinExitSpeed = 200.0f;  // 슬라이드 종료 최소 속도
    
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
	float SlideBoostMultiplier = 1.2f;  // 슬라이드 시작 시 속도 부스트

	UPROPERTY(EditDefaultsOnly, Category="Movement|Slide")
	float SlideBrakingDeceleration = 300.f; 

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float SlideForwardConeAngle = 60.0f;  // 전방 입력 판정 콘 각도 (도)
    
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SlideMaxSteeringRate = 15.0f;  // 초당 최대 회전 각도 (도)
    
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideSteeringResponsiveness = 1.0f;  // 조향 반응성 (0=무시, 1=최대)

	/**
	 * Sprint 관련 속성들
	 */
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Sprint")
	float SprintMaxSpeed = 900.0f;
    
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Sprint")
	float SprintAcceleration = 2000.0f;
    
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Sprint")
	float SprintMinSpeed = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float SprintForwardAngleThreshold = 50.0f;  // 전방 각도 허용 범위 (도)

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SprintStartAlignmentThreshold = 0.8f;  // 전방 각도 허용 범위 (라디안)

	uint8 bWantsToSprint : 1;

	bool bSprintInputHeld = false;

	/**
	 * Aim 관련 속성
	 */

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Aim")
	float AimSpeedMultiplier = 0.5f;  // 조준 시 이동 속도 배율
	
protected:
	UPROPERTY()
	TObjectPtr<AGtHeroCharacter> HeroCharacterOwner   =  nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Ground")
	float GroundTraceDistance = 500.0f;
	
private:
	
	FVector WallRunNormal = FVector::ZeroVector;

	bool bIsRightWall = false;
    
	// WallRun 체크 타이머
	float WallRunCheckInterval = 0.05f;
	float LastWallRunCheckTime = 0.0f;

	// WallRun 쿨다운 
	float WallRunEndTime = 0.0f;
	
	// 캐싱된 Ground Distance 정보
	float CachedGroundDistance = 0.0f;
	uint32 CachedGroundInfoFrame = 0;

	// Standing 캡슐 높이 캐싱
	float StandingCapsuleHalfHeight = 96.0f;

};
