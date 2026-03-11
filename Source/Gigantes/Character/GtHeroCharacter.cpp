#include "GtHeroCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/GtHeroMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gigantes/GtGameplayTags.h"
#include "Gigantes/Equipments/Components/GtLoadoutComponent.h"
#include "Gigantes/GameModes/GtGameModeBase.h"
#include "Gigantes/Input/GtInputComponent.h"
#include "Gigantes/Items/Systems/Manager/GtItemManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Test/GtTestWeaponBase.h"
#include "Test/TestGtGameplayTags.h"

AGtHeroCharacter::AGtHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGtHeroMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	HeroMovementComponent = Cast<UGtHeroMovementComponent>(GetCharacterMovement());
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->bUsePawnControlRotation = true;
	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->bUsePawnControlRotation = false;

	// TODO : 플레이어 컨트롤러로 이전 고민
	ItemManager = CreateDefaultSubobject<UGtItemManagerComponent>(TEXT("ItemManager"));
	
	LoadoutComponent = CreateDefaultSubobject<UGtLoadoutComponent>(TEXT("LoadoutComponent"));
}

// Called when the game starts or when spawned
void AGtHeroCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	OnStatusTagChanged.AddDynamic(this, &ThisClass::OnCharacterStatusTagChanged);
	LandedDelegate.AddDynamic(this, &ThisClass::OnLandedCallback);

	if (HeroMovementComponent)
	{
		HeroMovementComponent->OnCapsuleSizeChanged.BindUObject(this, &AGtHeroCharacter::HandleCapsuleSizeChanged);
	}
	if (LoadoutComponent)
	{
		LoadoutComponent->OnEquipmentWeaponChanged.AddDynamic(this, &AGtHeroCharacter::OnEquipmentChanged);
	}
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->OnMontageEnded.AddDynamic(this, &AGtHeroCharacter::OnMontageEnded);
	}
}

void AGtHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	checkf(InputConfigDataAsset, TEXT("Forgot to assign a valid data asset as input config"));

	const ULocalPlayer* LocalPlayer = GetController<APlayerController>()->GetLocalPlayer();

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	check(Subsystem);
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputConfigDataAsset->DefaultMappingContext, 0);

	UGtInputComponent* GtInputComponent = CastChecked<UGtInputComponent>(PlayerInputComponent);

	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_Jump, ETriggerEvent::Started, this, &ThisClass::Input_Jump);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_Crouch, ETriggerEvent::Started, this, &ThisClass::Input_Crouch);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_Sprint, ETriggerEvent::Started, this, &ThisClass::Input_SprintStart);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_Sprint, ETriggerEvent::Completed, this, &ThisClass::Input_SprintStop);
	
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_PrimaryAction, ETriggerEvent::Started, this, &ThisClass::Input_PrimaryActionPressed);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_PrimaryAction, ETriggerEvent::Completed, this, &ThisClass::Input_PrimaryActionReleased);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_SecondaryAction, ETriggerEvent::Started, this, &ThisClass::Input_SecondaryActionPressed);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_SecondaryAction, ETriggerEvent::Completed, this, &ThisClass::Input_SecondaryActionReleased);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_Reload, ETriggerEvent::Started, this, &ThisClass::Input_Reload);
	
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_EquipSlot1, ETriggerEvent::Started, this, &ThisClass::Input_EquipSlot1);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_EquipSlot2, ETriggerEvent::Started, this, &ThisClass::Input_EquipSlot2);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_UseGrenade, ETriggerEvent::Started, this, &ThisClass::Input_UseGrenadeSlot);
	GtInputComponent->BindNativeInputAction(InputConfigDataAsset, GtGameplayTags::InputTag_UseConsumable, ETriggerEvent::Started, this, &ThisClass::Input_UseConsumableSlot);
}

void AGtHeroCharacter::Die()
{
	Super::Die();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
    
	// 무브먼트 정지
	if (HeroMovementComponent)
	{
		HeroMovementComponent->DisableMovement();
		HeroMovementComponent->StopMovementImmediately();
	}

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp)
	{
		// 물리 시뮬레이션이 다른 오브젝트와 정상적으로 상호작용하도록 콜리전 프로파일을 "Ragdoll"로 설정합니다.
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetSimulatePhysics(true);
	}
	
	// GameMode에 플레이어 사망 알림
	if (UWorld* World = GetWorld())
	{
		if (AGtGameModeBase* GameMode = Cast<AGtGameModeBase>(UGameplayStatics::GetGameMode(World)))
		{
			GameMode->PlayerDied();
		}
	}

	APlayerController* PC = GetController<APlayerController>();
	if (PC)
	{
		DisableInput(PC);
	}
}

void AGtHeroCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	if (HasStatusTag(GtGameplayTags::Status_Dead))
	{
		return;
	}
	
	if (GetController())
	{
		const FVector2D Value = InputActionValue.Get<FVector2D>();
		const FRotator MovementRotation(0.0f, GetController()->GetControlRotation().Yaw, 0.0f);

		if (Value.X != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::RightVector);

			AddMovementInput(MovementDirection, Value.X);
		}

		if (Value.Y != 0.0f) 
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			AddMovementInput(MovementDirection, Value.Y);
		}
	}
}

void AGtHeroCharacter::Input_Look(const FInputActionValue& InputActionValue)
{
	const FVector2D Value = InputActionValue.Get<FVector2D>();
	if (Value.X != 0.0f)
	{
		AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		double AimInversionValue = -Value.Y;
		AddControllerPitchInput(AimInversionValue);
	}
}

void AGtHeroCharacter::Input_Jump(const FInputActionValue& InputActionValue)
{
	if (HeroMovementComponent)
	{
		HeroMovementComponent->HandleMovementInput(EMovementInput::Jump);
	}
}

void AGtHeroCharacter::Input_Crouch(const FInputActionValue& InputActionValue)
{
	if (HeroMovementComponent)
	{
		HeroMovementComponent->HandleMovementInput(EMovementInput::Crouch);
	}
}

void AGtHeroCharacter::Input_SprintStart(const FInputActionValue& InputActionValue)
{
	if (HeroMovementComponent)
	{
		HeroMovementComponent->HandleMovementInput(EMovementInput::SprintStart);
	}
}

void AGtHeroCharacter::Input_SprintStop(const FInputActionValue& InputActionValue)
{
	if (HeroMovementComponent)
	{
		HeroMovementComponent->HandleMovementInput(EMovementInput::SprintStop);
	}
}

void AGtHeroCharacter::Input_PrimaryActionPressed(const FInputActionValue& InputActionValue)
{
	
	// TODO : 행동 가능 상태 체크를 어떻게 구현할지 고민
	if (!CanPerformAction())
	{
		return;
	}
	
	if (LoadoutComponent)
	{
		LoadoutComponent->PrimaryActionPressed();
	}
}

void AGtHeroCharacter::Input_PrimaryActionReleased(const FInputActionValue& InputActionValue)
{
	// TODO : 행동 가능 상태 체크를 어떻게 구현할지 고민
	if (HasStatusTag(GtGameplayTags::Status_Dead))
	{
		return; 
	}
	
	if (LoadoutComponent)
	{
		LoadoutComponent->PrimaryActionReleased();
	}
}

void AGtHeroCharacter::Input_SecondaryActionPressed(const FInputActionValue& InputActionValue)
{
	if (!CanPerformAction())
	{
		return; 
	}
	
	bAimInputHeld = true;
	
	if (LoadoutComponent)
	{
		LoadoutComponent->SecondaryActionPressed();
	}
}

void AGtHeroCharacter::Input_SecondaryActionReleased(const FInputActionValue& InputActionValue)
{
	if (HasStatusTag(GtGameplayTags::Status_Dead))
	{
		return; 
	}

	bAimInputHeld = false;
	
	if (LoadoutComponent)
	{
		LoadoutComponent->SecondaryActionReleased();
	}
}

void AGtHeroCharacter::Input_Reload(const FInputActionValue& InputActionValue)
{
	// TODO : 행동 가능 상태 체크를 어떻게 구현할지 고민
	if (!CanPerformAction())
	{
		return; 
	}
	
	if (LoadoutComponent)
	{
		LoadoutComponent->ReloadAction();
	}
}

void AGtHeroCharacter::Input_EquipSlot1(const FInputActionValue& InputActionValue)
{
	if (HasStatusTag(GtGameplayTags::Status_Dead))
	{
		return;
	}
	
	if (LoadoutComponent)
	{
		LoadoutComponent->ChangeActiveWeaponSlot(GtGameplayTags::Loadout_Slot_Weapon_Primary);
	}
}

void AGtHeroCharacter::Input_EquipSlot2(const FInputActionValue& InputActionValue)
{
	if (HasStatusTag(GtGameplayTags::Status_Dead))
	{
		return;
	}
	
	if (LoadoutComponent)
	{
		LoadoutComponent->ChangeActiveWeaponSlot(GtGameplayTags::Loadout_Slot_Weapon_Secondary);
	}
}

void AGtHeroCharacter::Input_UseGrenadeSlot(const FInputActionValue& InputActionValue)
{
	if (HasStatusTag(GtGameplayTags::Status_Dead))
	{
		return;
	}
	
	if (LoadoutComponent)
	{
		LoadoutComponent->UseItemInSlot(GtGameplayTags::Loadout_Slot_Grenade);
	}
}

void AGtHeroCharacter::Input_UseConsumableSlot(const FInputActionValue& InputActionValue)
{
	if (HasStatusTag(GtGameplayTags::Status_Dead))
	{
		return;
	}
	
	if (LoadoutComponent)
	{
		LoadoutComponent->UseItemInSlot(GtGameplayTags::Loadout_Slot_Consumable);
	}
}

bool AGtHeroCharacter::CanJumpInternal_Implementation() const
{
	if (!HeroMovementComponent)
	{
		return false;
	}

	if (!HeroMovementComponent->GetCurrentStateProperties().bCanJump)
	{
		return false;
	}

	if (bIsCrouched)
	{
		return false;
	}
	
	return GetCharacterMovement()->IsMovingOnGround() || JumpCount < MaxJumpCount;
}

void AGtHeroCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	JumpCount++;

}

void AGtHeroCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	AddStatusTag(GtGameplayTags::Status_Action_Crouching);
}

void AGtHeroCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RemoveStatusTag(GtGameplayTags::Status_Action_Crouching);
}

bool AGtHeroCharacter::CanCrouch() const
{
	if (!HeroMovementComponent)
	{
		return false;
	}

	if (!HeroMovementComponent->GetCurrentStateProperties().bCanCrouch)
	{
		return false;
	}
    
	// 공중에서는 Crouch 불가
	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
	{
		return false;
	}
    
	// 죽은 상태에서는 Crouch 불가
	if (HasStatusTag(GtGameplayTags::Status_Dead))
	{
		return false;
	}
	
	return Super::CanCrouch();
}

void AGtHeroCharacter::Sprint()
{
	if (HeroMovementComponent)
	{
		HeroMovementComponent->HandleMovementInput(EMovementInput::SprintStart);
	}
}

void AGtHeroCharacter::UnSprint()
{
	if (HeroMovementComponent)
	{
		HeroMovementComponent->HandleMovementInput(EMovementInput::SprintStop);
	}
}

void AGtHeroCharacter::OnStartSprint()
{
	AddStatusTag(GtGameplayTags::Status_Action_Sprinting);
}

void AGtHeroCharacter::OnEndSprint()
{
	RemoveStatusTag(GtGameplayTags::Status_Action_Sprinting);
}

bool AGtHeroCharacter::CanPerformAction() const
{
	return !HasStatusTag(GtGameplayTags::Status_Dead) && 
		   !HasStatusTag(GtGameplayTags::Status_Action_Reloading);
}

void AGtHeroCharacter::HandleCapsuleSizeChanged(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// 메쉬 위치 조정
	RecalculateBaseEyeHeight();
    
	const ACharacter* DefaultChar = GetDefault<ACharacter>(GetClass());
	if (GetMesh() && DefaultChar->GetMesh())
	{
		FVector& MeshRelativeLocation = GetMesh()->GetRelativeLocation_DirectMutable();
        
		// 캡슐이 작아지면 메쉬를 위로 올림
		if (HalfHeightAdjust > 0) // 캡슐이 작아짐
		{
			MeshRelativeLocation.Z = DefaultChar->GetMesh()->GetRelativeLocation().Z + HalfHeightAdjust;
		}
		// 캡슐을 복구할 때 메쉬를 원래의 캡슐에 대한 상대 좌표로 이동
		else 
		{
			MeshRelativeLocation.Z = DefaultChar->GetMesh()->GetRelativeLocation().Z;
		}
        
		BaseTranslationOffset.Z = MeshRelativeLocation.Z;
	}
}

void AGtHeroCharacter::OnLandedCallback(const FHitResult& Hit)
{
	JumpCount = 0;
}

void AGtHeroCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	// 커스텀 움직임 모드(WallRun, Slide 등)에 대한 처리
	if (GetCharacterMovement()->MovementMode == MOVE_Custom)
	{
		if (HeroMovementComponent)
		{
			const uint8 CurrentCustomMode = HeroMovementComponent->GetCustomMovementMode();
            
			// Slide 시작
			if (CurrentCustomMode == CMM_Slide && PreviousCustomMode != CMM_Slide)
			{
				AddStatusTag(GtGameplayTags::Status_Action_Sliding);
				RemoveStatusTag(GtGameplayTags::Status_Action_Crouching);
			}
			// Wallrun 시작
			else if (CurrentCustomMode == CMM_WallRun && PreviousCustomMode != CMM_WallRun)
			{
				const FGameplayTag WallRunTag = HeroMovementComponent->IsWallRunningRight() ? 
					GtGameplayTags::Status_Action_WallRunning_Right : 
					GtGameplayTags::Status_Action_WallRunning_Left;
				AddStatusTag(WallRunTag);
			}
		}
	}
    
	// 슬라이드 종료 처리
	if (PreviousCustomMode == CMM_Slide)
	{
		RemoveStatusTag(GtGameplayTags::Status_Action_Sliding);
        
		if (GetCharacterMovement()->IsCrouching())
		{
			AddStatusTag(GtGameplayTags::Status_Action_Crouching);
		}
	}
    
	// 월런 종료 처리
	if (PreviousCustomMode == CMM_WallRun)
	{
		RemoveStatusTag(GtGameplayTags::Status_Action_WallRunning_Left);
		RemoveStatusTag(GtGameplayTags::Status_Action_WallRunning_Right);
	}
}

void AGtHeroCharacter::OnCharacterStatusTagChanged(const FGameplayTag& StatusTag, bool bAdded)
{
	// TODO : OnMovementModeChanged에서 처리하는 로직으로 대체 고려
	const bool bIsWallRunTag = StatusTag.MatchesTag(GtGameplayTags::Status_Action_WallRunning);
	if (bIsWallRunTag && bAdded)
	{
		JumpCount = 0;
	}
    
	// 조준 또는 전투 상태 변경 시 AimOffset 업데이트
	if (StatusTag == GtGameplayTags::Status_Action_Aiming || 
		StatusTag == GtGameplayTags::Status_Combat_Ranged)
	{
		UpdateAimOffsetState();
	}
}

void AGtHeroCharacter::OnEquipmentChanged(AGtTestWeaponBase* NewWeapon)
{
	// NewWeapon이 nullptr이면 무기 해제/유효한 포인터이면 무기 장착 상태
	bIsEquipped = (NewWeapon != nullptr);

	if (bIsEquipped)
	{
		// 무기 장착 시 (Strafing 모드)
		GetCharacterMovement()->bOrientRotationToMovement = false;
		bUseControllerRotationYaw = true;

		// TODO : 임시 코드로써 추후 무기 데이터에서 JointTargetLocation = WeaponItem->GetJointTargetLocation() 와 같이 가져오도록 수정
		JointTargetLocation = FVector(20.0, 45.0, -90.0);
	}
	else
	{
		// 무기 해제 시 (일반 이동 모드)
		GetCharacterMovement()->bOrientRotationToMovement = true;
		bUseControllerRotationYaw = false;
		JointTargetLocation = FVector::ZeroVector;
	}
}

void AGtHeroCharacter::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	AGtTestWeaponBase* CurrentWeapon = LoadoutComponent ? Cast<AGtTestWeaponBase>(LoadoutComponent->GetCurrentEquippedWeapon()) : nullptr;
    
	// 끝난 몽타주가 현재 무기의 재장전 몽타주가 맞는지 그리고 캐릭터가 재장전 상태인지 확인
	// 이렇게 하면 다른 몽타주가 끝나도 재장전 상태가 해제되는 버그를 막을 수 있음
	if (CurrentWeapon && Montage == CurrentWeapon->GetCharacterReloadMontage() && HasStatusTag(GtGameplayTags::Status_Action_Reloading))
	{
		// 재장전이 정상적으로 끝났든 무기 교체 등으로 중단되었든 상관없이 상태를 정리
		RemoveStatusTag(GtGameplayTags::Status_Action_Reloading);
		RemoveIKDisableTag(GtGameplayTags::Animation_IK_Disable_Reloading);
		CurrentWeapon->EndReload();

		TryUpdateAimingState();
	}
}

void AGtHeroCharacter::UpdateAimOffsetState()
{
	const bool bShouldEnableAimOffset = HasStatusTag(GtGameplayTags::Status_Action_Aiming) || HasStatusTag(GtGameplayTags::Status_Combat_Ranged);
    
	if (bUseAimOffset != bShouldEnableAimOffset)
	{
		bUseAimOffset = bShouldEnableAimOffset;
	}
}

void AGtHeroCharacter::TryUpdateAimingState()
{
	if (LoadoutComponent)
	{
		// 플레이어가 조준을 원하고 (버튼 누름), 캐릭터가 조준 가능한 상태라면
		if (bAimInputHeld && CanAim())
		{
			// 조준 시작 명령
			LoadoutComponent->SecondaryActionPressed();
		}
		else
		{
			// 그 외 모든 경우엔 조준 중지 명령
			LoadoutComponent->SecondaryActionReleased();
		}
	}
}

bool AGtHeroCharacter::CanAim() const
{
	return !HasStatusTag(GtGameplayTags::Status_Action_Reloading) &&
		   !HasStatusTag(GtGameplayTags::Status_Dead);
}

void AGtHeroCharacter::AddIKDisableTag(const FGameplayTag& DisableTag)
{
	IKDisableTags.AddTag(DisableTag);
}

void AGtHeroCharacter::RemoveIKDisableTag(const FGameplayTag& DisableTag)
{
	IKDisableTags.RemoveTag(DisableTag);
}

int32 AGtHeroCharacter::GetCurrentWeaponAmmo() const
{
	if (LoadoutComponent)
	{
		if (AGtTestWeaponBase* Weapon = Cast<AGtTestWeaponBase>(LoadoutComponent->GetCurrentEquippedWeapon()))
		{
			return Weapon->GetCurrentAmmo();
		}
	}
	return 0;
}

int32 AGtHeroCharacter::GetCurrentWeaponMaxAmmo() const
{
	if (LoadoutComponent)
	{
		if (AGtTestWeaponBase* Weapon = Cast<AGtTestWeaponBase>(LoadoutComponent->GetCurrentEquippedWeapon()))
		{
			return Weapon->GetMaxAmmo();
		}
	}
	return 0;
}