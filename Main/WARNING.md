# Main 검토 경고 및 확인 사항

검토일: 2026-11-15  
대상 브랜치: `prj_mobilgene_ccp`  
상태: 검토 사항 기록. 이 문서 작성에 따른 소스 수정은 없음.

## 1. 범위와 검증 조건

대상은 `ccp_main.c/.h`, `SWC_CCP_Main.c/.h`, `ccp_context.h`,
`ccp_ivd_check.c/.h`입니다. 기존 Common/FSM Core와의 연결도 확인했습니다.

호스트 GCC C99 빌드와 임시 mock으로 동작을 확인했습니다. mock은 RTE, 기능별
context, 전원 상태 및 SecureFlash 서비스를 대체하며 실제 타깃 구현의 계약이나
타이밍을 증명하지 않습니다. 검증 코드는 저장소의 정식 회귀 테스트에 포함되어
있지 않습니다.

## 2. 항목 요약

| ID | 우선순위 | 항목 | 확인 상태 |
|---|---|---|---|
| MAIN-W001 | 높음 | GPT 초기화 실패 무시 및 재시도 누락 | 반환 실패 mock으로 재현 |
| MAIN-W002 | 중간 | IVD 비교 길이·계산 완료에 대한 호출 계약 | 일부 길이 및 미완료 비교 성공 재현. API 목적 확인 필요 |
| MAIN-W003 | 중간 | 5ms 처리와 10ms 초기화 간 실행 순서·선점 보호 | 코드 확인. 실제 OS/RTE 설정에 따라 영향 결정 |
| MAIN-W004 | 중간 | 호스트 스케줄러의 실제 시간과 tick 불일치 | 코드 구조 확인. 실제 시간 오차는 미측정 |

## 3. MAIN-W001: GPT 초기화 실패 처리

위치:

- `ccp_main.c`: `GPT_Timer_Enable()`, `GPT_Timer_Disable()`, `CCP_Init()`
- `SWC_CCP_Main.c`: `RE_SWC_CCP_Main_10ms_Task()`

### 현재 동작과 영향

EnableNotification/StartTimer의 반환값을 무시하며, 성공 여부와 관계없이
`GPT_TimerEnabled`를 TRUE로 설정합니다. 상위 runnable도 `CCP_Init()` 호출 전에
`isMainInit`을 TRUE로 설정하고, 이후 주기에는 초기화를 다시 시도하지 않습니다.

GPT가 시작되지 않으면 software timer가 진행되지 않아 만료 이벤트를 기다리는
기능이 정지할 수 있습니다. DisableNotification/StopTimer의 반환값도 무시하므로
실제 하드웨어 상태와 소프트웨어 플래그가 일치하는지 보장하지 못합니다.

### 재현 결과

RTE의 EnableNotification과 StartTimer가 실패하도록 한 mock에서 다음을 확인했습니다.

- 초기화 이후에도 `GPT_Timer_Notification()`이 FSM timer로 전달됨
- 다음 10ms runnable에서도 GPT 시작을 재시도하지 않음

이 재현은 소프트웨어 플래그와 호출 동작을 확인한 것이며, 시작 실패한 실제 GPT가
interrupt를 발생시킨다는 뜻은 아닙니다.

### 권장 조치와 완료 기준

- 생성된 RTE의 실제 반환 타입·실패 의미를 확인하고 성공 여부를 처리합니다.
- GPT 준비 결과를 `CCP_Init()`과 상위 runnable로 전달합니다.
- 모든 필수 초기화가 성공한 뒤에만 준비 완료 상태를 설정합니다.
- 부분 성공 시 rollback과 안전 상태, 제한된 재시도 또는 오류 latch 정책을 정합니다.
- 각 RTE 호출의 실패를 주입하여 초기화 결과, 실제 호출 순서 및 복구 동작을 검증합니다.

## 4. MAIN-W002: IVD 검증 API의 길이와 준비 상태

위치:

- `ccp_ivd_check.c`: `CcpIvdCheck_Validate()`, `CcpIvdCheck_GetCalculatedData()`
- `ccp_ivd_check.h`: 비교 함수와 계산 완료 조회 API의 계약

### 현재 동작과 영향

`CcpIvdCheck_Validate()`는 초기화 여부, NULL pointer 및 길이 0만 거부하고,
전달받은 `dataLength` 바이트를 비교합니다. SHA-256 digest 길이인 32바이트와
계산 완료 여부를 강제하지 않습니다.

### 재현 결과

- 32바이트 전체가 다른 두 데이터도 첫 바이트가 같으면 길이 1 비교에서 STD_OK 반환
- 초기화 직후 계산되지 않은 0으로 채워진 출력도 동일한 데이터와 비교하면 STD_OK 반환

현재 헤더는 임의의 양수 길이와 외부에서 받은 데이터의 비교도 허용합니다.
따라서 위 결과는 현재 비교 함수의 계약과 일치합니다. 다만 이 성공값을 곧바로
"완료된 SHA-256 IVD 검증 성공"으로 사용하면 불완전한 검증을 수용할 수 있습니다.

### 권장 조치와 완료 기준

- 범용 바이트 비교와 고정 길이 IVD 검증 중 API의 목적을 확정합니다.
- SHA-256 IVD 검증 API라면 길이가 정확히 `PRJ_IVD_DIGEST_LENGTH`인지 확인합니다.
- 모듈 내부 계산 데이터를 사용하는 검증 경로는 계산 완료와 오류 상태를 확인합니다.
- 필요하면 외부 데이터 비교와 내부 계산 결과 검증을 별도 API로 분리합니다.
- 짧은 길이, 초과 길이, 미완료, 계산 오류, 정상 완료의 수용·거부 기준을 테스트합니다.

## 5. MAIN-W003: 초기화와 주기 처리의 실행 순서

위치:

- `SWC_CCP_Main.c`: 10ms/5ms runnable
- `ccp_ivd_check.c`: `CcpIvdCheck_Init()`, `CcpIvdCheck_MainFunction5ms()`

### 현재 동작과 영향

IVD 초기화는 첫 10ms runnable에서 수행하지만, 5ms runnable은 준비 상태와
관계없이 IVD 처리를 호출합니다. 초기화되지 않은 상태의 주기 호출은 result를
ERROR로 설정합니다.

또한 `CcpIvdCheck_Init()`은 나머지 필드와 계산 상태·버퍼의 초기화를 완료하기
전에 `initialized`를 TRUE로 설정합니다. 선점 가능한 별도 task에서 5ms 처리가
실행되면 부분 초기화 상태를 관찰하거나 초기화와 상태 갱신이 겹칠 수 있습니다.

같은 task의 정해진 순서로 직렬 실행한다면 영향이 제한됩니다. 실제 runnable
배치, 우선순위, 선점 및 재진입 설정이 없어 타깃에서의 발생 여부는 미확인입니다.

### 권장 조치와 완료 기준

- 초기화 완료 전에는 5ms 처리를 호출하지 않도록 통합 규칙을 정합니다.
- 초기화 중에는 준비 상태를 해제하고, 전체 초기화가 끝난 뒤 준비 완료를 게시합니다.
- 별도 task가 접근한다면 OS/RTE 설정 또는 배타 보호로 초기화와 처리를 직렬화합니다.
- 플래그 설정 순서만 바꾸거나 volatile만 추가하여 동시성 문제가 해결된다고 간주하지 않습니다.
- 5ms 선행 호출과 초기화 도중 선점 조건을 검증하고 재초기화 허용 조건도 명시합니다.

## 6. MAIN-W004: 호스트 스케줄러 시간 정확도

위치:

- `ccp_main.c`: standalone `main()` loop, `Main_SleepMs()`

### 현재 동작과 영향

task 실행 후 1ms sleep하며, loop 반복 횟수로 5ms/10ms 호출 시점을 결정합니다.
따라서 실제 한 loop의 주기는 task 실행 시간과 실제 sleep 시간을 합한 값입니다.
task 실행이 길어지면 타이머와 주기 동작이 실제 시간보다 느리게 진행됩니다.

논리적인 호출 순서 검증에는 사용할 수 있지만 실제 debounce, timeout 및
주기 정확도 검증에 그대로 사용할 수는 없습니다. 타이머가 전진하려면 외부
`Task_1ms()` 구현이 실제로 Notification을 호출하는지도 확인해야 합니다.

### 권장 조치와 완료 기준

- 반복 횟수 기반 논리 테스트인지 실제 시간 기반 실행인지 목적을 명시합니다.
- 실제 시간 기준이라면 monotonic clock으로 다음 실행 시각을 관리합니다.
- 처리 지연 시 누락 tick과 task 활성화를 보정할지, 오류로 처리할지 정책을 정합니다.
- task 부하를 변화시키며 실제 1ms/5ms/10ms 호출 간격과 timer 만료 시간을 측정합니다.

## 7. 통과한 검증과 추가 통합 확인

| 검증 | 결과 |
|---|---|
| IVD 정상 분할 처리 | 6,656회 호출로 두 구간 총 1,703,168바이트 처리 완료 |
| SecureFlash Update 요청량 | 한 번에 최대 256바이트 준수 |
| SecureFlash 지속 busy | timeout 이후 계산 오류 latch 확인 |
| GPT 실패 및 IVD 비교 경계 | 위 경고 항목의 mock 재현 완료 |

5ms 호출을 전제로 한 정상 경로의 명목 처리 시간은 33.28초입니다.
이는 mock에서의 상태 진행 횟수에 대한 계산이며 SecureFlash 실제 실행 시간이나
타깃의 최악 실행 시간 측정값이 아닙니다.

아래 항목은 실제 통합 환경에서 추가 확인해야 합니다.

- GPT 입력 클럭과 count 1000의 대응: 현재 코드의 1MHz 입력 가정에서 1ms인지 확인
- GPT 연속 동작 모드, notification 연결 및 중복 호출 유무
- 기능별 context getter, 초기화 순서 및 오류 처리 계약
- 생성된 RTE/MemMap과 실제 OS runnable 배치·우선순위·선점 설정
- SecureFlash의 반환 코드, 호출 시간, 중단·재시작 및 서비스 공유 계약
- linker/flash bank mapping과 IVD 서명 생성 범위의 일치
- 타깃 컴파일, 링크, MISRA-C/Helix QAC 분석 및 MCU 실행 검증
