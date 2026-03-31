# Gigantes

UE5 C++ 기반 3인칭 싱글플레이 파쿠르 TPS 게임입니다.  
커스텀 무브먼트, 무기/장비, 카메라, 데미지, AI, 아이템 데이터 시스템을 설계 및 구현했습니다.

---

## 프로젝트 개요

| 항목      | 내용              |
| --------- | ----------------- |
| 엔진      | Unreal Engine 5.5 |
| 개발 방식 | C++               |
| 장르      | 3인칭 파쿠르 TPS  |
| 플랫폼    | PC (Windows)      |
| 개발 인원 | 3인               |
| 개발 기간 | 20XX.XX ~ 20XX.XX |

---

## 팀원 및 역할

| 이름   | 역할             | 담당 파트                                         |
| ------ | ---------------- | ------------------------------------------------- |
| 김민영 | Character System | 커스텀 무브먼트, 애니메이션 시스템, 카메라 시스템 |
| 고명수 | AI System        | 일반 AI 로직, 보스 AI 로직                        |
| 임동휘 | Item             | 아이템 데이터 설계                                |

---

## 주요 시스템

### 커스텀 무브먼트

- WallRun, Slide, Sprint, Crouch를 CMC 확장으로 구현하고, 입력 타입별 핸들 함수를 CMC 내부에 배치하여 상태 분기를 일원화
- 상태 전환 시 `OnMovementModeChanged` 콜백으로 StatusTag를 갱신해 애니메이션·카메라와 연동

### 애니메이션 레이어 아키텍처

- `AnimInstanceProxy`로 게임 스레드 데이터를 캐싱하고 워커 스레드에서 소비하는 멀티스레드 구조
- Linked Anim Layer로 무기 타입별 애니메이션 레이어를 런타임에 교체

### 무기 / 장비 시스템

- `LoadoutComponent`로 슬롯 기반 무기 관리(장착·해제·교체)와 `CurveVector`/`CurveFloat` 커브 기반 반동·확산 처리
- 카메라 1차 → 총구 2차의 2-Trace 방식으로 3인칭 시차 보정 조준

### 카메라 시스템

- GameplayTag 기반 상태별 카메라 옵션 전환에 Additive/Override 동적 Modifier를 합성하는 구조
- 무기가 `IGtCameraModifierSource` 인터페이스를 통해 조준 시 카메라 모디파이어를 직접 제공

### 데미지 시스템

- `IGtDamageable` 인터페이스로 데미지 적용 경로를 통일하고, `DamageReceiverComponent`에서 계산 로직을 분리
- `AttributeComponent` Health 변경 → 사망 처리까지의 흐름을 델리게이트 체인으로 연결

### 아이템 데이터 시스템

- JSON 파일로 아이템 데이터를 외부 정의하고, `GameInstanceSubsystem`에서 파싱·캐싱 후 비동기 프리로드
- GameplayTag → 클래스 매핑 + `ItemFactory` 패턴으로 런타임 스폰

### AI

- MotherAI가 플레이어 위치를 탐지·노이즈 적용 후 개별 EnemyAI에 브로드캐스트하는 계층 구조
- 보스(EliteEnemy)는 패턴 풀 랜덤 선택 → 쿨다운 → 복귀 방식의 공격 사이클

---

## 설계 특징

- GameplayTag 하나의 변경으로 이동·애니메이션·카메라가 연쇄 반응하는 상태 전파 구조
- 인터페이스(`IGtDamageable`, `IGtEquippable`, `IGtCameraModifierSource`) 기반 설계로 시스템 간 직접 참조 제거
- 데이터 수집(게임 스레드)과 소비(워커 스레드)를 `AnimInstanceProxy`로 명확히 분리
- 아이템 정의를 JSON 외부 파일로 분리하고, 비동기 프리로드로 런타임 파일 I/O를 제거

---

## 사용 기술

- Unreal Engine 5.5
- C++
- GameplayTag
- CharacterMovementComponent 확장 (Custom Movement Mode)
- AnimInstanceProxy / Linked Anim Layers
- AIPerception
- StreamableManager (비동기 에셋 로딩)
- JSON 파싱 (FJsonObject)

---

## 확인 포인트

| 분류          | 관련 파일                                                | 내용                                       |
| ------------- | -------------------------------------------------------- | ------------------------------------------ |
| 캐릭터        | `GtHeroCharacter.h/cpp`                                  | 입력 처리, 상태 태그 관리, 장비 연동       |
| 무브먼트      | `GtHeroMovementComponent.h/cpp`                          | WallRun / Slide / Sprint / Crouch 구현     |
| 애니메이션    | `GtBaseAnimInstance.h/cpp`, `GtHeroAnimInstance.h/cpp`   | Proxy 기반 스레드 안전 데이터 처리, HandIK |
| 무기          | `GtTestWeaponBase.h/cpp`                                 | 반동 · 확산 · 2-Trace 조준 · 재장전        |
| 장비 관리     | `GtLoadoutComponent.h/cpp`                               | 슬롯 관리, 장착 / 해제 / 교체 흐름         |
| 카메라        | `GtPlayerCameraManager.h/cpp`                            | 상태별 카메라 옵션 + 동적 Modifier         |
| 데미지        | `GtDamageable.h`, `GtDamageReceiverComponent.h/cpp`      | 인터페이스 기반 데미지 파이프라인          |
| 어트리뷰트    | `GtAttributeComponent.h/cpp`                             | 태그 기반 수치 관리, 변경 델리게이트       |
| 아이템 데이터 | `GigantesItemDataSubsystem.h/cpp`, `GtItemFactory.h/cpp` | JSON 파싱 → 프리로드 → 스폰                |
| AI            | `GtEnemyAiController.h/cpp`, `GtEliteEnemy.h/cpp`        | 감지 · 추적 · 공격 전환, 보스 패턴         |
