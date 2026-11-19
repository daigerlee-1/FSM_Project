# Power Management FSM

## 1. 개요

`Power` 모듈은 CCP/RRC의 전원 상태를 관리하는 이벤트 큐 구동형 FSM이다.
ACC ADC 입력, CAN 수신, HU Power State, HU 메시지 timeout 및 안전모드
메시지를 조합하여 전원 상태를 결정한다.

주요 파일은 다음과 같다.

| 파일 | 역할 |
|---|---|
| `power_app.h` | Power 모듈 내부 상태, 이벤트, context 및 API 정의 |
| `power_app.c` | 전이 테이블, guard, 상태 entry 및 전압 판정 구현 |
| `SWC_PWR_MGR.c/.h` | 외부 공개 API와 AUTOSAR RTE 입출력 연동 |
| `SWC_PDC.c` | 향후 PDC 관련 RTE 연동을 위한 위치 |

Power FSM은 `PowerApp_MainFunction10ms()`에서 실행되며 한 번 호출될 때
이벤트 큐에서 최대 한 개의 이벤트만 처리한다. 모든 상태 변경은
`PowerApp_Transitions[]` 전이 테이블을 통해 수행한다.

## 2. 기능 사양 요약

| 기능 | 기능 동작 |
|---|---|
| 전원 상태 관리 | INIT, SLEEP, SLEEP_IN, STANDBY, POWER_ON, POWER_OFF 및 ERROR 상태를 전이 테이블로 관리한다. |
| ACC 판정 | 6.5V/7.0V 히스테리시스와 100ms 유지시간으로 ACC Off/On을 확정한다. |
| 배터리 판정 | 6.5~18.5V 경계의 히스테리시스와 100ms 유지시간으로 공용 동작전압 상태를 확정한다. |
| HU 명령 처리 | HU Sleep, Power Off 및 Power On 명령을 현재 상태에 맞게 전원 전이로 변환한다. |
| HU timeout | STANDBY 5초, POWER_ON 3분 5초, POWER_OFF 5분의 상태별 미수신 시간을 감시한다. |
| LCAN sleep | SLEEP에서 NoCom을 요청하고 확인한 뒤 EN/STB 출력을 적용하여 SLEEP_IN으로 전이한다. |
| LCAN wake | CAN 수신 latch로 sleep 출력을 차단하고 Power 상태와 FullCom 확인 후 latch를 해제한다. |
| 출력 제어 | POWER_ON에서만 외부 power output을 활성화하며 나머지 상태에서는 비활성화한다. |
| 오류 처리 | Fault 시 출력과 LED를 fail-safe 상태로 전환하고 Reset을 통해 INIT부터 재시작한다. |

## 3. 상태 정의

```c
typedef enum
{
    POWER_STATE_INIT = 0U,
    POWER_STATE_SLEEP,
    POWER_STATE_SLEEP_IN,
    POWER_STATE_STANDBY,
    POWER_STATE_POWER_ON,
    POWER_STATE_POWER_OFF,
    POWER_STATE_ERROR,
    POWER_STATE_MAX
} Power_StateType;
```

CAN 상태 보고값과 NVM 저장값은 별도 변환 없이 위 enum 값을 `uint8`로
명시적 캐스팅하여 사용한다.

| 상태 | CAN/NVM 값 |
|---|---:|
| `INIT` | `0U` |
| `SLEEP` | `1U` |
| `SLEEP_IN` | `2U` |
| `STANDBY` | `3U` |
| `POWER_ON` | `4U` |
| `POWER_OFF` | `5U` |
| `ERROR` | `6U` |

이 순서를 적용하기 전의 NVM 상태값과는 숫자 호환성을 유지하지 않는다.
기존 NVM 상태값은 사용하지 않고 새 펌웨어의 상태 진입 시 새 값으로
다시 기록한다. 향후 enum 순서를 변경하면 CAN/NVM 규격도 함께 변경해야
한다.

| 상태 | 의미 | 주요 entry 동작 |
|---|---|---|
| `INIT` | Power FSM 초기화 상태 | HU timeout 정지, CAN TX Offline 설정 |
| `SLEEP` | System Off 및 LCAN sleep 준비 상태 | HU timeout 정지, CAN TX Offline, LED Off, LCAN NoCom 완료 대기 |
| `SLEEP_IN` | LCAN 준비가 완료된 최종 저전류 상태 | LCAN EN/STB sleep 출력 유지, CAN TX Offline, LED Off |
| `STANDBY` | HU 전원 명령 대기 상태 | CAN RX 유지, 애플리케이션 LCAN TX 억제, LED Off, HU timeout 5초 시작 |
| `POWER_ON` | 정상 전원 동작 상태 | 출력 On, 조건 충족 시 CAN TX 허용, HU timeout 3분 5초 시작 |
| `POWER_OFF` | 조작계 전원 Off 상태 | 출력 Off, CAN RX 유지, CAN TX Offline, HU timeout 5분 시작 |
| `ERROR` | 오류 및 fail-safe 상태 | 출력 Off, HU timeout 정지, CAN TX Offline, LED Fault 요청 |

`PowerApp_IsOutputEnabled()`는 `POWER_STATE_POWER_ON`에서만 `TRUE`를
반환한다. `SWC_PWR_MGR.c`는 이 값을 MPQ4323 Enable 출력에 반영한다.

LED 출력은 POWER_ON entry에서 직접 켜지지 않는다. 유효한 LCAN detent 또는
자동 밝기 요청이 LED 설정에 성공해야 별도로 활성화된다.

## 4. 초기 전이

초기 상태는 `POWER_STATE_INIT`이다. `PowerApp_Init()`이 FSM 초기화 후
`POWER_EVENT_INIT_DONE`을 큐에 등록하며 첫 번째 10ms 실행에서 다음과
같이 전이한다.

```text
INIT --INIT_DONE--> SLEEP --SLEEP_READY--> SLEEP_IN
```

ERROR에서 Reset을 수행한 경우에도 바로 SLEEP으로 변경하지 않고
`ERROR → INIT → SLEEP → SLEEP_IN` 순서를 사용한다.

### CAN 연동

Power FSM은 CCP/RRC 다이어그램의 CAN/HU 기반 전체 전이 경로를 항상
사용한다. ACC ON 또는 일반 CAN 수신으로 SLEEP에서 STANDBY로 전이하며,
HU Power On은 SLEEP 계열 상태에서 POWER_ON으로 직접 전이할 수 있다.

CAN 수신은 Power FSM 이벤트 처리와 별도로 LCAN wake latch를 즉시
설정한다. 이 latch가 설정된 동안에는 NoCom 요청과 LCAN EN/STB sleep
출력을 금지하고 ComM FullCom 복구를 우선한다. 따라서 CAN wake로 MCU가
기동한 직후 Power FSM이 아직 INIT 또는 SLEEP이어도 INH Low 절차를 다시
실행하지 않는다.

## 5. 이벤트 정의

| 이벤트 | 발생 조건 |
|---|---|
| `INIT_DONE` | 초기화 완료 또는 ERROR Reset 완료 |
| `ACC_ON` | ACC ADC가 On 임계값을 100ms 동안 연속 만족 |
| `ACC_OFF` | ACC ADC가 Off 임계값을 100ms 동안 연속 만족 |
| `CAN_RX` | SLEEP 또는 SLEEP_IN 상태에서 CAN 프레임 수신 |
| `HU_SLEEP` | HU Power State `0x0` 수신 |
| `HU_POWER_OFF` | HU Power State `0x1` 수신 |
| `HU_POWER_ON` | HU Power State `0x2` 수신 |
| `HU_TIMEOUT` | 현재 상태의 HU 메시지 미수신 시간이 만료 |
| `SAFETY_MODE` | 안전모드 Test Message 수신 |
| `FAULT` | Power fault 요청 |
| `RESET` | ERROR 상태 복구 요청 |
| `SLEEP_READY` | SLEEP에서 LCAN NoCom 및 sleep 출력 준비 완료 |

HU Power State `0x3`은 Invalid 값이다. Invalid 메시지를 수신하면 이전에
저장한 유효 HU 상태는 유지하지만 메시지 자체는 수신되었으므로 현재
상태의 HU timeout 타이머는 다시 시작한다.

## 6. 전체 상태 전이표

다음 표는 `PowerApp_Transitions[]`의 모든 전이를 상태별로 정리한 것이다.
표에 없는 상태·이벤트 조합은 상태를 변경하지 않는다. Guard가 `FALSE`인
경우에도 해당 이벤트는 큐에서 소비되지만 현재 상태를 유지한다.

### INIT

| 이벤트 | Guard | 다음 상태 |
|---|---|---|
| `INIT_DONE` | Always | `SLEEP` |
| `FAULT` | Always | `ERROR` |

### SLEEP

| 이벤트 | Guard | 다음 상태 |
|---|---|---|
| `ACC_ON` | Always | `STANDBY` |
| `CAN_RX` | Always | `STANDBY` |
| `HU_POWER_ON` | Always | `POWER_ON` |
| `SLEEP_READY` | Always | `SLEEP_IN` |
| `FAULT` | Always | `ERROR` |

`SLEEP_READY`는 LCAN NoCom이 확인되고 LCAN EN/STB sleep 출력이 적용된
후에만 등록된다. ACC ON 또는 CAN RX가 먼저 발생하면 SLEEP 완료를
기다리지 않고 STANDBY로 복귀한다.

### SLEEP_IN

| 이벤트 | Guard | 다음 상태 |
|---|---|---|
| `ACC_ON` | Always | `STANDBY` |
| `CAN_RX` | Always | `STANDBY` |
| `HU_POWER_ON` | Always | `POWER_ON` |
| `FAULT` | Always | `ERROR` |

SLEEP과 SLEEP_IN에서는 모든 CAN 프레임이 wake 조건이 된다. 일반 CAN
프레임은 `CAN_RX`로 STANDBY에 진입한다. HU Power State `POWER_ON`은
`CAN_RX` 이벤트 순서에 의존하지 않고 SLEEP 또는 SLEEP_IN에서 POWER_ON으로
직접 전이할 수도 있다.

### STANDBY

| 이벤트 | Guard | 다음 상태 |
|---|---|---|
| `HU_SLEEP` | Always | `SLEEP` |
| `HU_POWER_OFF` | Always | `POWER_OFF` |
| `HU_POWER_ON` | Always | `POWER_ON` |
| `HU_TIMEOUT` | ACC Off | `SLEEP` |
| `ACC_OFF` | HU timeout 완료 | `SLEEP` |
| `FAULT` | Always | `ERROR` |

STANDBY 진입 시 HU timeout은 5초로 시작한다.

### POWER_ON

| 이벤트 | Guard | 다음 상태 |
|---|---|---|
| `HU_POWER_OFF` | Always | `POWER_OFF` |
| `HU_TIMEOUT` | ACC Off | `POWER_OFF` |
| `ACC_OFF` | HU timeout 완료 | `POWER_OFF` |
| `SAFETY_MODE` | ACC Off | `SLEEP` |
| `FAULT` | Always | `ERROR` |

POWER_ON 진입 시 HU timeout은 3분 5초로 시작한다. HU timeout이 먼저
발생해도 ACC가 On이면 상태를 유지한다. 이후 ACC가 Off가 되면 이미
저장된 timeout 결과를 guard가 확인하여 POWER_OFF로 전이한다.

현재 다이어그램 기준으로 POWER_ON의 `HU_SLEEP` 이벤트에는 전이 행이
없다. 따라서 해당 이벤트는 큐에서 소비되지만 상태는 유지된다.

### POWER_OFF

| 이벤트 | Guard | 다음 상태 |
|---|---|---|
| `HU_SLEEP` | Always | `SLEEP` |
| `HU_POWER_ON` | Always | `POWER_ON` |
| `HU_TIMEOUT` | ACC Off | `SLEEP` |
| `ACC_OFF` | HU timeout 완료 | `SLEEP` |
| `FAULT` | Always | `ERROR` |

POWER_OFF 진입 시 HU timeout은 5분으로 시작한다. 이 상태에서는 CAN RX는
유지하지만 CAN TX FSM은 Offline으로 요청된다.

### ERROR

| 이벤트 | Guard | 다음 상태 |
|---|---|---|
| `RESET` | Always | `INIT` |

Reset action은 LED와 NVM reset을 요청하고 `INIT_DONE` 이벤트를 등록한다.

### Sleep 진입 순서

모든 Sleep 요청은 요구사항 다이어그램의 SLEEP으로 먼저 전이한다.
LCAN 준비가 완료되면 내부 최종 상태인 SLEEP_IN으로 전이한다.

```text
STANDBY  --HU_SLEEP--> SLEEP --SLEEP_READY--> SLEEP_IN
POWER_OFF --HU_SLEEP--> SLEEP --SLEEP_READY--> SLEEP_IN
```

초기화, HU timeout 또는 안전모드 조건도 동일하게 SLEEP을 거친다.

```text
INIT --INIT_DONE--> SLEEP --SLEEP_READY--> SLEEP_IN
STANDBY --HU_TIMEOUT + ACC Off--> SLEEP
POWER_OFF --HU_TIMEOUT + ACC Off--> SLEEP
POWER_ON --SAFETY_MODE + ACC Off--> SLEEP
```

SLEEP 상태에서 `SWC_PWR_MGR.c`가 LCAN NoCom 요청, 200ms 재요청 및
EN/STB sleep 출력을 수행한다. 준비가 완료되면 `SLEEP_READY` 이벤트를
등록하여 SLEEP_IN으로 전이한다. NoCom이 확인되지 않으면 SLEEP 상태에서
대기한다.

LCAN 통신 모드 조회와 요청은 `lcan_comm.c`가 소유한 ComM UserRequest
Client/Server Port를 통해 수행한다. `SWC_PWR_MGR.c`는 생성 RTE 헤더를
공유하지 않고 다음 프로젝트 API를 사용한다.

```c
LcanCom_GetCurrentMode();
LcanCom_GetRequestedMode();
LcanCom_RequestMode();
```

`RequestMode(NO_COM)` 성공은 요청 접수만 의미한다. 실제 SLEEP 완료는
`GetCurrentMode()`가 `LCAN_COM_MODE_NO_COM`을 반환한 뒤에만 처리한다.
Client/Server Port의 인자 타입은 `ComM_ModeType`이며, 이 프로젝트의 생성
RTE 헤더가 제공하는 `RTE_MODE_MDG_ComMMode_NO_COM`,
`RTE_MODE_MDG_ComMMode_SILENT_COM` 및
`RTE_MODE_MDG_ComMMode_FULL_COM`을 명시적으로 캐스트하여 사용한다.

### CAN wake latch

`SWC_PWR_MGR_NotifyCanRx()`는 CAN 수신을 확인하면
`SwcPwrMgr_LcanWakePending`을 즉시 설정한다. Power FSM 초기화 전 수신된
wake 요청과 HU Power State도 보관하며, 초기화가 끝난 뒤 FSM event queue에
전달한다. Event queue가 가득 찬 경우에는 다음 10ms 실행에서 재시도한다.

Wake latch가 설정된 동안의 제어 순서는 다음과 같다.

```text
CAN 수신
→ Wake latch 설정
→ 진행 중인 LCAN sleep 절차 취소
→ NoCom 및 EN/STB sleep 출력 차단
→ FullCom 요청
→ Power FSM wake 이벤트 처리
→ Power 상태와 ComM 상태 확인
→ Wake latch 해제
```

Wake latch는 다음 조건을 모두 만족할 때만 해제한다.

```text
Power 상태가 STANDBY, POWER_ON 또는 POWER_OFF
AND ComM Current Mode가 LCAN_COM_MODE_FULL_COM
```

Power 상태가 `INIT`, `SLEEP` 또는 `SLEEP_IN`이거나 ComM이 아직
`NO_COM/SILENT_COM`이면 latch를 유지한다. 이 정책은 CAN wake 직후 MCU가
다시 sleep 출력을 적용하여 INH와 전원이 반복적으로 ON/OFF되는 현상을
방지한다.

## 7. HU 메시지 Timeout

HU timeout은 중앙 1ms FSM 타이머의 다음 ID를 사용한다.

```c
FSM_TIMER_POWER_HU_TIMEOUT
```

| 상태 | 설정값 | 실제 시간 |
|---|---:|---:|
| `STANDBY` | `5000U` ticks | 5초 |
| `POWER_ON` | `185000U` ticks | 3분 5초 |
| `POWER_OFF` | `300000U` ticks | 5분 |

동작 순서는 다음과 같다.

1. STANDBY, POWER_ON 또는 POWER_OFF entry에서 상태별 타이머를 시작한다.
2. HU Power State 프레임을 받으면 현재 상태의 타이머를 다시 시작한다.
3. 타이머가 만료되면 `huMessageTimedOut`을 `TRUE`로 설정한다.
4. `HU_TIMEOUT` 이벤트를 Power FSM 큐에 등록한다.
5. ACC가 Off이면 전이하고, ACC가 On이면 현재 상태를 유지한다.
6. Timeout 이후 ACC가 Off로 변경되면 `ACC_OFF` guard가 즉시 전이한다.

AUTOSAR RTE의 `L_HU_State` 수신 timeout은 수신 snapshot의 validity만
무효화한다. RTE timeout runnable은 Power FSM의 상태별 timeout 타이머를
중지하거나 `HU_TIMEOUT` 이벤트를 직접 등록하지 않는다.

Timeout 이벤트 등록이 큐 full로 실패하면 10ms 타이머를 시작하여 다음
Power main-function 실행에서 재시도한다.

`FSM_Timer_Notification()`은 반드시 1ms 주기로 호출되어야 한다.

## 8. ACC 판정

ACC 입력은 `SWC_PWR_MGR.c`의 5ms runnable에서 ADC 값을 읽어
`PowerApp_SetAccVoltage()`로 전달한다. 상태 판정은 10ms Power main
function에서 수행한다.

| 조건 | 전압 | ADC 값 | 연속 유지 시간 |
|---|---:|---:|---:|
| ACC On | 7.0V 이상 | `1450U` 이상 | 100ms |
| ACC Off | 6.5V 이하 | `1352U` 이하 | 100ms |

ADC 값이 `1352U` 초과, `1450U` 미만이면 현재 ACC 상태를 유지한다.
100ms 확인에는 `FSM_TIMER_POWER_ACC_HYSTERESIS`를 사용한다. 임계 조건에서
벗어나면 타이머를 정지하고 처음부터 다시 판단한다.

## 9. 배터리 동작전압 판정

배터리 판정 결과는 CAN 전용 값이 아니라 여러 기능이 공유할 수 있는
`batteryOperationOn`으로 관리한다.

| 동작 | 저전압 조건 | 고전압 조건 |
|---|---:|---:|
| Operation On | 7.0V, ADC `1440U` 이상 | 18.0V, ADC `3675U` 이하 |
| Operation Off | 6.5V, ADC `1335U` 이하 | 18.5V, ADC `3768U` 이상 |

전압 조건은 100ms 동안 연속 유지되어야 반영되며
`FSM_TIMER_POWER_BAT_HYSTERESIS`를 사용한다. 확인된 결과는
`PowerApp_IsBatteryOperationOn()`으로 조회할 수 있다.

현재 CAN TX가 수행되려면 다음 조건을 모두 만족해야 한다.

```text
PowerApp_IsOperational() == TRUE              (POWER_STATE_POWER_ON)
AND PowerApp_IsBatteryOperationOn() == TRUE   (동작전압 확인 완료)
AND ComM Current Mode == LCAN_COM_MODE_FULL_COM
```

`SWC_LCAN.c`의 `LCAN_IsTxAllowed()`가 위 조건을 조합한다. ComM mode 조회가
실패하거나 유효하지 않은 mode가 반환되면 fail-safe로 LCAN RTE Write를
수행하지 않는다. 이 판정은 신규 RTE Write 허용 여부이며 실제 물리 송신
정지는 ComM과 COM Tx I-PDU 설정도 함께 보장해야 한다.

향후 PDC reset 조건에서도 `PowerApp_IsBatteryOperationOn()` 또는 Power
context의 read-only 값을 사용할 수 있다.

## 10. 주요 API 및 실행 순서

다른 모듈은 `power_app.h`를 직접 include하지 않고 `SWC_PWR_MGR.h`의 다음
공개 wrapper를 사용한다.

| 외부 공개 API | 역할 |
|---|---|
| `SWC_PWR_MGR_NotifyCanRx()` | LCAN 수신 사실 전달 |
| `SWC_PWR_MGR_NotifyHuPowerState()` | HU Power State 전달 |
| `SWC_PWR_MGR_RequestSafetyMode()` | 안전모드 전이 요청 |
| `SWC_PWR_MGR_IsOperational()` | POWER_ON 상태 확인 |
| `SWC_PWR_MGR_IsAccOn()` | 100ms 히스테리시스로 확정된 ACC ON 상태 확인 |
| `SWC_PWR_MGR_IsBatteryOperationOn()` | 배터리 동작전압 상태 확인 |
| `SWC_PWR_MGR_IsOutputEnabled()` | 외부 power output 요청 확인 |
| `SWC_PWR_MGR_GetState()` | 현재 Power FSM 상태 반환 |
| `SWC_PWR_MGR_GetContext()` | Trace32/Main용 read-only context 반환 |

PC standalone 빌드에서는 위 공개 이름을 동일한 `PowerApp` 구현에 매핑하며,
AUTOSAR 빌드에서는 `SWC_PWR_MGR.c`의 wrapper 함수를 호출한다. 초기화,
10ms scheduler 실행 및 standalone sleep 완료 처리는 `task.c`가 내부
`PowerApp` API를 직접 사용한다.

다음 API는 Power 모듈 내부 구현 인터페이스이다.

| API | 역할 |
|---|---|
| `PowerApp_Init()` | Context와 FSM을 초기화하고 `INIT_DONE`을 등록한다. |
| `PowerApp_MainFunction10ms()` | 전압, timeout, 이벤트 및 출력을 10ms 주기로 처리한다. |
| `PowerApp_SetAccVoltage()` | 최신 ACC ADC 값을 전달한다. |
| `PowerApp_SetBatteryVoltage()` | 최신 배터리 ADC 값을 전달한다. |
| `PowerApp_NotifyCanRx()` | Sleep 계열 상태의 CAN wake 이벤트를 요청한다. |
| `PowerApp_NotifyHuPowerState()` | HU 상태값을 검증하고 상태별 이벤트와 timeout 갱신을 처리한다. |
| `PowerApp_NotifyHuPowerStateTimeout()` | 외부에서 확정한 HU timeout을 Power FSM 이벤트로 등록한다. |
| `PowerApp_RequestSafetyMode()` | 안전모드 전이를 요청한다. |
| `PowerApp_RequestFault()` / `PowerApp_RequestReset()` | 오류 진입과 복구를 요청한다. |
| `PowerApp_RequestSleepReady()` | LCAN sleep 준비 완료 후 SLEEP_IN 전이를 요청한다. |
| `PowerApp_IsOperational()` | 현재 상태가 POWER_ON인지 반환한다. |
| `PowerApp_IsOutputEnabled()` | 외부 power output 요청 상태를 반환한다. |
| `PowerApp_IsAccOn()` | 100ms 히스테리시스 처리로 확정된 ACC ON 상태를 반환한다. |
| `PowerApp_IsBatteryOperationOn()` | 확정된 공용 배터리 동작전압 상태를 반환한다. |
| `PowerApp_GetState()` / `PowerApp_GetContext()` | 상태와 read-only context를 제공한다. |

10ms 실행 순서는 다음과 같다.

`PowerApp_MainFunction10ms()`의 실행 순서는 다음과 같다.

```text
ACC hysteresis 처리
→ Battery operation hysteresis 처리
→ HU timeout 만료 처리
→ Fsm_Dispatch()로 이벤트 한 개 처리
→ 현재 상태의 do 함수 실행
→ public state/event 동기화
→ 외부 출력 요청 갱신
```

FSM의 실제 authoritative state는 `PowerApp_Context.fsm.state`이며,
`PowerApp_Context.state`는 main function에서 동기화되는 진단용 typed
snapshot이다.

## 11. 유지보수 주의사항

- 새 상태를 추가하면 `Power_StateType`, `PowerApp_StateOperations[]`, 전이
  테이블 및 `PowerApp_UpdateOutputRequest()`를 함께 갱신한다.
- 새 이벤트를 추가하면 `Power_EventType`과 필요한 전이 행을 함께 추가한다.
- 전이 개수는 `sizeof`로 자동 계산하므로 수동 count 매크로를 만들지 않는다.
- 모든 `switch`에는 `default`를 유지한다.
- 새 timeout은 기능 전용 `FSM_TimerIdType`을 할당하여 다른 기능과 공유하지
  않는다.
- 긴 timeout도 1ms `uint32` 카운터 범위 안에서 설정한다.
- 이벤트 큐 overflow 가능성이 있는 입력은 `Std_ReturnType`을 확인하고
  필요한 경우 재시도 또는 진단 카운터 정책을 추가한다.
- LCAN wake latch는 Power 상태와 ComM FullCom이 모두 확인되기 전에
  해제하지 않는다.
- CAN 수신 runnable과 Power 10ms runnable이 선점 가능한 경우 wake latch와
  보류 HU 상태의 공유 접근에 프로젝트 SchM exclusive area 적용을 검토한다.

## 부록: PC 예제 연동 및 테스트

Standalone의 `CanInput_ProcessFrame()`은 다음 시험용 frame을 Power API로
변환한다.

`PowerApp_RequestIgnitionOn()`과 `PowerApp_RequestIgnitionOff()`는 ADC 없이
ACC 이벤트를 직접 주입하는 standalone 시험 API이다. Target에서는
`PowerApp_SetAccVoltage()`로 전달된 ADC 값과 100ms 판정 경로를 사용한다.

| CAN ID | Data[0] | 처리 |
|---:|---:|---|
| `0x100` | `0x00` | HU Sleep |
| `0x100` | `0x01` | HU Power Off |
| `0x100` | `0x02` | HU Power On |
| `0x100` | `0x03` | Invalid, 이전 HU 값 유지 |
| `0x103` | `0x01` | Safety Mode Test Message |

SLEEP 또는 SLEEP_IN의 모든 CAN 수신은 wake 이벤트로 전달한다. PC의
`Sleep(1)` 기반 scheduler는 Windows timer resolution의 영향을 받으므로
실제 ECU의 1ms OS Task와 wall-clock 결과가 다를 수 있다.

| 키 | 동작 |
|---|---|
| `p` | ACC On 이벤트 |
| `o` | ACC Off 이벤트 |
| `s` | HU Sleep (`0x0`) |
| `d` | HU Power Off (`0x1`) |
| `w` | HU Power On (`0x2`) |
| `z` | HU Invalid (`0x3`) |
| `a` | Safety Mode Test Message |
| `f` | Power Fault |
| `r` | ERROR Reset |
| `i` | 전체 FSM 상태 출력 |

대표 시험 순서는 다음과 같다.

```text
시작: INIT → SLEEP → SLEEP_IN
p:   SLEEP_IN → STANDBY
w:   STANDBY → POWER_ON
d:   POWER_ON → POWER_OFF
s:   POWER_OFF → SLEEP → SLEEP_IN
```

## 부록: Trace32 확인

`PowerApp_GetContext()`가 반환하는 context에서 다음 값을 확인할 수 있다.

| 항목 | 의미 |
|---|---|
| `fsm.state` | 실제 FSM 상태 |
| `state` | 최근 10ms 실행에서 동기화된 상태 |
| `event` | 마지막으로 큐에서 꺼낸 이벤트 |
| `fsm.eventQueue.count` | 현재 대기 중인 이벤트 수 |
| `accVoltage` / `accOn` | ACC ADC 값과 확인된 상태 |
| `batteryVoltage` | 최근 배터리 ADC 값 |
| `batteryOperationOn` | 확인된 배터리 동작전압 상태 |
| `huPowerState` | 마지막 유효 HU Power State |
| `huPowerStateValid` | 유효 HU 값을 받은 적이 있는지 여부 |
| `huMessageTimedOut` | 현재 HU timeout 결과 |
| `huTimeoutTimerActive` | HU timeout 타이머 실행 여부 |

`SWC_PWR_MGR.c`의 module-local 변수에서는 다음 wake 진행 상태를 확인할 수
있다.

| 항목 | 의미 |
|---|---|
| `SwcPwrMgr_LcanWakePending` | CAN wake 처리 및 FullCom 확인이 진행 중인지 여부 |
| `SwcPwrMgr_LcanWakeEventQueued` | CAN_RX가 Power FSM queue에 등록되었는지 여부 |
| `SwcPwrMgr_HuPowerStatePending` | 초기화 또는 queue 재시도를 기다리는 HU 상태 존재 여부 |
| `SwcPwrMgr_PendingHuPowerState` | 보류 중인 최신 HU Power State 값 |

`fsm.state`를 강제로 변경하면 entry 함수가 실행되지 않아 타이머와 출력
상태가 일치하지 않을 수 있다. 가능한 한 공개 이벤트 API를 사용한다.
