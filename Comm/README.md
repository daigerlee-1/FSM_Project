# Communication Modules

## 1. 개요

Comm 폴더는 AUTOSAR RTE 기반 LCAN 통합 코드를 포함합니다. 애플리케이션
로직은 RTE에 직접 의존하지 않도록 `lcan_input`과
`lcan_output`에 분리합니다. `SWC_LCAN.c`는 AUTOSAR runnable 진입점을
유지하고, HU 수신 RTE 접근은 `lcan_input_rte`가 담당합니다.
버전 요청의 반복 전송은 `lcan_version`이 별도로 관리합니다.

## 2. 기능 사양 요약

LCAN 기능의 제품 동작 관점 요구사항은 다음과 같습니다. 세부 구현, 신호값 및
예외 처리는 각 장에서 설명합니다.

| 기능 | 기능 동작 |
|---|---|
| HU 메시지 수신 | 여섯 종류의 HU 메시지를 RTE로 읽고 값과 validity를 통합 context에 저장합니다. |
| Power 상태 연동 | HU Power State와 CAN 수신 사실을 Power FSM에 전달합니다. |
| LED 밝기 제어 | Detent가 활성화되면 최대 밝기를 우선 적용하고, 그 외에는 Auto Brightness 단계값을 적용합니다. |
| 수신 timeout | 해당 메시지의 signal validity를 무효화하여 오래된 수신값이 유효한 값으로 사용되지 않게 합니다. |
| LCAN 송신 허용 | Power On, 배터리 동작전압 정상 및 ComM FullCom 조건을 모두 만족할 때 출력 신호를 갱신합니다. |
| Button 출력 | 확정된 Press/Short/Long/Clear 상태를 CAN 신호로 변환하고 11분 연속 입력을 고착 고장으로 진단합니다. |
| Wheel 출력 | 회전 방향과 누적 count를 encoder 값으로 변환하고 Move Info 이후 Invalid 값을 출력합니다. |
| 입력 상호 배제 | Wheel encoder와 8-way 방향키가 동시에 의도하지 않게 출력되지 않도록 먼저 활성화된 입력에 우선권을 부여합니다. |
| 진단 출력 | 버튼 상태, CAN 통신 기본 상태, reset 상태 및 미지원 기능의 정의된 fallback 값을 출력합니다. |
| 버전 응답 | HU의 SW/CAN DB/HW 버전 요청마다 200ms 간격으로 총 3회 응답합니다. |
| 통신 모드 제어 | ComM의 Current/Requested mode를 조회하고 Power FSM의 LCAN sleep 진입을 위한 NoCom 요청을 제공합니다. |

## 3. 파일 구성

| 파일 | 역할 |
|---|---|
| `SWC_LCAN.c/.h` | LCAN RTE runnable, 출력 연결 및 외부 공개 API |
| `SWC_IPdu.c` | COM I-PDU callout 처리 |
| `lcan_input_rte.c/.h` | HU 수신 RTE Read, 유효성 판정 및 timeout 어댑터 |
| `lcan_input.c/.h` | HU 수신 메시지 snapshot, timeout 및 앱 전달 로직 |
| `lcan_output.c/.h` | Button/Wheel 리포트 해석과 CAN 출력 snapshot |
| `lcan_version.c/.h` | SW/CAN DB/HW 버전 요청 및 반복 응답 처리 |
| `lcan_comm.c/.h` | ComM mode 조회, 변환 및 요청 API |

`SWC_LCAN.c`, `lcan_input_rte.c`, `lcan_version.c`, `lcan_comm.c`,
`SWC_IPdu.c`는 AUTOSAR 생성 헤더와 설정에 의존합니다.

## 4. 계층 경계

```text
AUTOSAR RTE / COM / ComM
        ↕
SWC_LCAN.c, lcan_input_rte.c, lcan_version.c, lcan_comm.c, SWC_IPdu.c
        ↕
lcan_input.c, lcan_output.c
        ↕
Power / LED / Button / Wheel application modules
```

RTE 함수는 SWC 어댑터에서만 호출하고, 하위 애플리케이션 모듈에는 RTE와
무관한 타입과 API를 제공합니다.

## 5. LCAN 입력

수신 runnable은 `lcan_input_rte`의 메시지별 API를 호출합니다. 이 어댑터는
각 RTE Read의 `Std_ReturnType`을 확인하여 값과 validity를 동시에 갱신한 후
RTE 독립적인 `lcan_input`에 snapshot을 전달합니다. 현재 context는 다음 HU
메시지를 묶어서 관리합니다.

- `L_HU_CarInfo`
- `L_HU_E_USM`
- `L_HU_MoodInfo`
- `L_HU_State`
- `L_HU_SYS`
- `L_HU_SysInfo`

전체 snapshot은 다음 API로 읽을 수 있습니다.

```c
const LcanInput_ContextType *LcanInput_GetContext(void);
```

### 차량 동력원 상태 판정

`prj_defs.h`의 `VEHICLE_POWERTRAIN_TYPE`으로 차량 동력원을 선택합니다.

| 설정값 | 판정 신호 |
|---|---|
| `VEHICLE_ICE` | `engineState` (`L_ENG_EngSta`) |
| `VEHICLE_HEV` | `hevReadyState` (`L_HCU_HevRdySta`) |
| `VEHICLE_EV` | `evDriveReadyState` (`L_VCU_EvDrvRdySta`) |

내부 `LcanInput_IsEngineStarted()`는 선택된 신호의 RTE validity가 `TRUE`이고
신호값이 `1U`일 때만 `TRUE`를 반환합니다. 따라서 CarInfo timeout이나 RTE
Read 실패 이후에는 이전 값이 `1U`로 남아 있더라도 `FALSE`를 반환합니다.
Comm 외부 모듈은 내부 API 대신 `SWC_LCAN.h`를 include하고 다음 공개 API를
사용합니다.

```c
boolean SWC_LCAN_IsEngineStarted(void);
```

| API | 기능 |
|---|---|
| `LcanInput_UpdateHuCarInfo/EUsm/MoodInfo/State/Sys/SysInfo()` | RTE adapter가 완성한 메시지 snapshot 저장 및 application 알림 |
| `LcanInput_NotifyHuCarInfo/MoodInfo/State/SysInfoTimeout()` | 해당 메시지 validity 무효화 |
| `LcanInput_MainFunction10ms()` | Pending LED 밝기 요청 재시도와 output enable 처리 |
| `LcanInput_IsEngineStarted()` | 선택된 차량 형식의 유효한 시동 또는 Ready 상태 반환 |
| `LcanInput_GetContext()` | 모든 HU 입력 snapshot의 read-only context 반환 |

Timeout runnable은 관련 validity를 `FALSE`로 변경하고 필요한 애플리케이션
timeout 이벤트를 전달합니다. HU Power State는 Power FSM에 전달되며,
자동 밝기와 detent 신호는 LED 밝기 제어에 사용됩니다.

### 입력 메시지별 처리

모든 수신 runnable은 각 `Rte_Read`의 반환값이 `E_OK`인 신호만 valid로
표시합니다. 메시지 snapshot을 갱신하면 `SWC_PWR_MGR_NotifyCanRx()`를 호출하여
Power FSM에 LCAN 수신 사실을 전달합니다.

| 메시지 | 저장 신호 | 현재 기능 연결 |
|---|---|---|
| `L_HU_CarInfo` | ACC/IGN, rheostat, detent, terminal, 운전자 및 구동 상태 | detent 밝기 우선 제어, CAN 수신 알림 |
| `L_HU_E_USM` | Mood lamp On/Off | snapshot 및 CAN 수신 알림 |
| `L_HU_MoodInfo` | 색상, 밝기, Fade In/Out | snapshot 및 CAN 수신 알림 |
| `L_HU_State` | Power State, reset 관련 신호, 자동 밝기 | Power FSM과 LED 밝기 제어 |
| `L_HU_SYS` | SW/CAN DB/HW 버전 요청 | `lcan_version` 반복 응답 시작 |
| `L_HU_SysInfo` | Haptic level, Rotate change request | snapshot 및 CAN 수신 알림 |

Mood, reset 및 SysInfo 신호 중 snapshot 전용으로 표시된 값은 현재 별도의
애플리케이션 동작으로 연결되지 않았으며 통합 context에만 저장됩니다.

### LED 밝기 요청 우선순위

LCAN 밝기 요청은 다음 우선순위와 범위로 해석합니다.

| 우선순위 | 조건 | LED 밝기 step |
|---:|---|---:|
| 1 | valid `CLU_DtntOutSta_L == 1U` | 최대 밝기 `LED_BRIGHTNESS_STEP_FULL` |
| 2 | valid Auto Brightness `== 0U` | `13U` |
| 3 | valid Auto Brightness `2U~200U` | 수신값 그대로 |
| 제외 | Auto Brightness `1U` 또는 `200U` 초과 | 요청 무시 |

요청은 Power가 동작 상태일 때 `LedApp_RequestSetBrightness()`로 전달합니다.
LED FSM이 초기화 또는 설정 중이면 10ms마다 재시도합니다. 설정이 수락되고
LED가 `LED_STATE_NORMAL`이며 pending 설정이 없으면 출력을 활성화합니다.

### 수신 timeout 처리

| 메시지 | timeout 처리 |
|---|---|
| `L_HU_CarInfo` | 모든 signal validity 무효화 후 밝기 요청 재평가 |
| `L_HU_MoodInfo` | 색상·밝기·Fade signal validity 무효화 |
| `L_HU_State` | Power/reset/자동 밝기 signal validity 전체 무효화 |
| `L_HU_SysInfo` | Haptic/Rotate signal validity 무효화 |

현재 `L_HU_E_USM`과 `L_HU_SYS`에는 별도 timeout runnable이 구현되어 있지
않습니다. `L_HU_State`의 RTE timeout은 snapshot validity만 무효화하며 Power
FSM이 상태별로 관리하는 5초/3분 5초/5분 HU message timer를 직접 중지하거나
만료시키지 않습니다.

## 6. LCAN 출력

Button과 Wheel SWC에서 전달한 RTE 리포트는 다음 경로로 처리됩니다.

```text
Button/Wheel application
→ SWC_KnobButton/SWC_KnobWheel RTE Write
→ SWC_LCAN reception event
→ LcanOutput_ReportButton()/LcanOutput_ReportWheel()
→ LcanOutput_Context
→ RE_LCAN_Output_10ms_Process()
→ LCAN RTE Write
```

| API | 기능 |
|---|---|
| `LcanOutput_Init()` | Button/Wheel 출력 snapshot과 입력 중재 상태를 명시적으로 초기화 |
| `LcanOutput_ReportButton()` | Button report를 검증하고 출력 snapshot에 반영 |
| `LcanOutput_ReportWheel()` | Wheel direction/count report를 검증하고 중재 상태에 반영 |
| `LcanOutput_MainFunction10ms()` | Hold time, Invalid 전환 및 입력 우선권 갱신 |
| `LcanOutput_GetContext()` | 현재 LCAN 출력 snapshot의 read-only context 반환 |

### 출력 snapshot 초기화

`LcanOutput_Init()`은 정적 변수의 암묵적인 zero 초기화에 의존하지 않고
Button과 Wheel의 시작값을 다음과 같이 명시적으로 설정한다.

| 항목 | 초기값 |
|---|---:|
| APPS/BACK/SEARCH/ENTER | `LCAN_OUTPUT_KEY_CLEAR` |
| 8-way 방향 출력 | `LCAN_OUTPUT_KEY_CLEAR` |
| Wheel Encoder | `0xFFU` |
| Wheel Encoder validity | `TRUE` |
| Wheel 입력 소유권 | `LCAN_WHEEL_INPUT_OWNER_NONE` |
| 방향 release guard count | `0U` |
| 방향 입력 차단 상태 | `FALSE` |

`LCAN_EnsureOutputInitialized()`는 초기화를 한 번만 수행한다. 다음 경로는
출력 context를 읽거나 변경하기 전에 이 초기화 함수를 호출한다.

```text
SWC_LCAN_GetContext()
RE_SWC_Wheel_Direction_Event()
RE_SWC_Knob_Button_Event()
RE_LCAN_Output_10ms_Process()
```

따라서 통합 context 조회 또는 Button/Wheel report가 LCAN 출력 runnable보다
먼저 실행되더라도 동일한 초기값을 사용한다.

LCAN 출력 RTE Write는 다음 조건을 모두 만족할 때만 수행합니다.

```text
POWER_STATE_POWER_ON
AND SWC_PWR_MGR_IsBatteryOperationOn() == TRUE
AND ComM Current Mode == LCAN_COM_MODE_FULL_COM
```

`LcanOutput_MainFunction10ms()`는 조건과 관계없이 실행하여 Button/Wheel 출력
snapshot과 내부 시간을 계속 갱신합니다. 조건이 만족되지 않으면 RTE Write만
생략합니다. ComM mode 조회 실패도 TX 비허용으로 처리합니다. 실제 물리 CAN
송신 정지는 ComM 및 COM Tx I-PDU 설정과 함께 보장해야 합니다.

### TX 허용 상승 에지의 초기 출력 요청

`RE_LCAN_Output_10ms_Process()`는 직전 TX 허용 상태를
`LCAN_TxPreviouslyAllowed`에 보관한다. `LCAN_IsTxAllowed()` 결과가
`FALSE`에서 `TRUE`로 변경되면 `LCAN_InitialOutputPending`을 설정하고 현재
Button/Wheel 초기 출력값을 RTE에 한 번 명시적으로 기록한다.

```text
TX 비허용
→ Power/배터리/ComM 조건 만족
→ LCAN_IsTxAllowed(): FALSE → TRUE
→ LCAN_InitialOutputPending = TRUE
→ Button/Wheel 초기 출력 RTE Write
→ LCAN_InitialOutputPending = FALSE
→ 이후 기존 10ms 주기 출력 계속
```

이 검출은 최초 기동뿐 아니라 `NO_COM` 이후 다시 `FULL_COM`으로 전환될
때에도 적용된다. TX가 비허용인 동안에는 출력 snapshot을 유지하고,
재허용 상승 에지에서 현재 값을 다시 RTE에 전달한다.

이 초기 출력 요청은 RTE Write 수행을 보장한다. CAN 신호가 `OnChange`이고
COM 내부 초기값이 기록값과 동일하면 물리 프레임 전송 여부는 COM 전송 방식에
따라 달라질 수 있다. 부팅 프레임을 반드시 전송해야 하는 요구사항은 COM의
Mixed/Cyclic 설정 또는 I-PDU trigger 정책과 함께 검토한다.

### TX 허용과 I-PDU callout

LCAN 송신 허용은 두 단계에서 확인합니다.

| 단계 | 적용 범위 | 허용 조건 |
|---|---|---|
| `LCAN_IsTxAllowed()` | `RE_LCAN_Output_10ms_Process()`의 모든 RTE Write | Power 동작 상태, 배터리 동작전압 정상, ComM `FULL_COM` |
| `ComIPdu_LCAN_L_CCP_HU_PE_Diag()` | `L_CCP_HU_PE_Diag` TX I-PDU callout | Power 출력 활성, 배터리 동작전압 정상 |

첫 번째 단계는 애플리케이션이 RTE 출력값을 갱신할 수 있는지를 결정하고,
두 번째 단계는 진단 I-PDU가 실제 COM 전송 직전에 허용되는지를 다시
확인합니다. 진단 callout의 `PduId`와 payload는 현재 조건 판정에 사용하지
않으며 향후 I-PDU별 정책 확장을 위해 보존합니다.

### 진단 출력

`L_CCP_HU_PE_Diag`의 현재 진단값은 다음과 같습니다.

| 신호 | 값 | 의미 |
|---|---:|---|
| `CCP_BtnState` | `1U` | 모든 버튼 정상 |
| `CCP_BtnState` | `2U` | 동일 버튼이 11분 이상 연속으로 눌림 |
| `CCP_CanMsgErr` | `1U` | 기본 CAN message 통신 정상값 |
| `CCP_ResetOpSta` | `PDC_Get_ResetOpSte()` 반환값 | PDC reset 동작 상태 |
| `CCP_UpdateStat` | `0U` | OTA update 결과가 없는 기본 상태 |
| `CCP_TouchState` | `0x0FU` | 미지원 Touch 상태 |
| `CCP_HptState` | `0x0FU` | 미지원 Haptic 상태 |
| `CCP_RotateChangeSta` | `0x03U` | 미지원 Rotate-change 상태 |
| `CCP_HU_RotationErrorSta` | `0U` | 미지원 Rotation-error fallback |

버튼 고착 진단은 각 물리 버튼에서 독립적으로 수행하며, 확정된 Release 후
다시 정상값 `1U`로 복귀합니다. `CCP_CanMsgErr`는 현재 기본 정상값을
출력하며 향후 Bus-Off 또는 통신 오류 판정 API로 교체할 수 있습니다.

### 버전 요청 응답

`lcan_version.c`는 HU의 SW, CAN DB 및 HW 버전 요청을 각각 독립적으로
관리합니다. 다음 조건에서 새로운 응답 sequence를 시작합니다.

```text
request validity == TRUE
AND request value == 1U
AND 동일 요청이 아직 접수되지 않음
```

한 번 접수된 요청의 출력 순서는 다음과 같습니다.

```text
요청 접수
→ Major/Minor에 0xFF 기록
→ 실제 Major/Minor 기록 (1회)
→ 200ms 대기
→ Major/Minor에 0xFF 기록
→ 실제 Major/Minor 기록 (2회)
→ 200ms 대기
→ Major/Minor에 0xFF 기록
→ 실제 Major/Minor 기록 (3회)
→ 종료
```

버전 신호는 AUTOSAR OnChange 전송으로 설정되어 있으므로 동일한 실제 값을
반복해서 기록하기 전에 `0xFF`를 기록하여 값 변화를 만듭니다. 200ms는
`RE_LCAN_Output_10ms_Process()`의 10ms 호출을 20회 계수하여 생성합니다.

| 설정 | 값 | 의미 |
|---|---:|---|
| `LCAN_VERSION_CLEAR_VALUE` | `0xFFU` | OnChange 재전송을 위한 초기화값 |
| `LCAN_VERSION_TX_REPEAT_COUNT` | `3U` | 요청당 성공 응답 횟수 |
| `LCAN_VERSION_TX_INTERVAL_TICKS` | `20U` | 10ms 기준 200ms |

RTE Write가 실패한 시도는 전송 횟수에 포함하지 않으며 다음 10ms 처리에서
재시도합니다. 3회가 완료된 뒤 요청값이 계속 `1U`이면 다시 시작하지 않고,
요청이 비활성화된 후 다시 활성화되어야 새로운 sequence가 시작됩니다.

`LcanVersion_MainFunction10ms()`는 LCAN TX 허용 조건이 만족된 경우에만
호출됩니다. 따라서 Power, 배터리 동작전압 또는 ComM 조건으로 TX가 금지되면
반복 주기 계수도 진행되지 않으며, 다시 허용된 뒤 남은 sequence를 계속합니다.

실제 버전 응답 데이터는 공통 진단 데이터의 다음 설정을 사용합니다.

| 요청 종류 | 버전 데이터 설정 |
|---|---|
| SW version | `ECU_SW_VER_DATA_INFO` |
| CAN DB version | `ECU_CANDB_VER_INFO` |
| HW version | `ECU_HW_VER_DATA_INFO` |

### 선택적 미지원 출력

다음 설정은 `prj_defs.h`에서 관리합니다.

| 설정 | 기본값 | 동작 |
|---|---:|---|
| `PRJ_LCAN_LOG_OUTPUT_ENABLE` | `0U` | LOG 매크로, 함수 및 RTE Write 제외 |
| `PRJ_LCAN_TOUCH_OUTPUT_ENABLE` | `0U` | Touch 매크로, 함수 및 RTE Write 제외 |

값을 `1U`로 설정하면 해당 fallback 출력 코드가 컴파일됩니다. Touch 포트는
`Rte_Write`마다 OnWrite 전송을 유발할 수 있으므로 기능이 지원되기 전에는
비활성 상태를 유지합니다. 두 설정은 `0U` 또는 `1U`만 허용되며 다른 값은
전처리 오류로 거부합니다.

Button 신호는 `CLEAR`, `PRESSED`, `SHORT_KEY`, `LONG_KEY`, `INVALID` 값을
사용합니다. Invalid 값은 `0x0F`입니다.

항상 출력되는 미지원 또는 고정 신호는 다음과 같습니다.

| 출력 기능 | 값 | 의미 |
|---|---:|---|
| Tune/Volume scroll encoder | `0xFFU` | 미지원 encoder |
| Tune/Volume scroll key | `0x0FU` | 미지원 key |
| Reset key | `0x0FU` | 미지원 reset key |
| Home/Map/Menu key | `0U` | 구현되지 않은 key의 clear 상태 |
| Wheel direction angle | `0x01FFU` | Touch direction angle 미지원 |
| LOG signals | `0U` | 옵션 활성 시 미지원 fallback |
| Touch signals | `0U` | 옵션 활성 시 미지원 fallback |

### Wheel encoder

`CCP_WhlEncdr`는 다음 형식으로 출력합니다.

| 값 | 의미 |
|---:|---|
| `0x00` | Reserved |
| `0x01~0x7F` | Clockwise 이동 count |
| `0x80~0xFE` | Counterclockwise 이동 count |
| `0xFF` | Invalid |

Clockwise는 count 값을 그대로 사용하고 Counterclockwise는 `0x7F + count`로
변환합니다. Move Info는 30ms 유지한 뒤 `0xFF`로 변경합니다.

`L_CCP_E_Wheel`에는 Wheel encoder 외에도 angle, 방향 버튼 및 Enter 신호가
함께 패킹됩니다. 따라서 CAN frame의 첫 바이트 이후 값은 `CCP_WhlEncdr`의
count 연장값이 아니라 같은 메시지의 다른 신호입니다. Angle을 포함한 나머지
신호의 상세 매핑과 동작은 관련 기능이 확정된 뒤 별도로 정리합니다.

ButtonApp은 안정적으로 눌린 물리 입력의 조합을 다음과 같이 8방향 논리
Output ID로 변환하고, LCAN은 해당 Output ID를 CAN 키에 매핑합니다.

| 물리 입력 | CAN 출력 |
|---|---|
| Up | `CCP_Dir_0_degree` |
| Up + Right | `CCP_Dir_45_degree` |
| Right | `CCP_Dir_90_degree` |
| Down + Right | `CCP_Dir_135_degree` |
| Down | `CCP_Dir_180_degree` |
| Down + Left | `CCP_Dir_225_degree` |
| Left | `CCP_Dir_270_degree` |
| Up + Left | `CCP_Dir_315_degree` |

방향이 바뀌면 ButtonApp이 이전 논리 방향의 `CLEAR`와 새 논리 방향의
`PRESSED` Report를 순서대로 생성합니다. Up+Down, Left+Right, 3개 및 4개
동시 입력은 유효한 방향으로 선택하지 않습니다. LCAN 모듈은 물리 버튼 상태나
조합 mask를 보유하지 않습니다.

### Encoder와 8-way 상호 배제

Encoder 회전과 8-way 방향 버튼이 동시에 의도하지 않게 동작하는 것을 막기
위해 먼저 활성화된 입력이 Wheel CAN 출력의 우선권을 가집니다.

- Encoder 이동 Report가 먼저 수신되면 8-way 출력은 모두 `CLEAR`로
  유지합니다.
- 8-way `PRESSED` Report가 먼저 수신되면 `CCP_WhlEncdr`는 `0xFF`
  (Invalid)로 유지합니다.
- Encoder 우선권 중에 눌린 8-way 입력은 해제될 때까지 차단하므로 Encoder
  동작 종료 후 뒤늦게 방향 신호가 출력되지 않습니다.
- 8-way `CLEAR` 이후에는 방향 전환 Report 사이의 틈을 보호하기 위해 10ms
  기준 3주기(30ms) 동안 방향 우선권을 유지합니다.
- 현재 우선권은 `LcanOutput_Context.wheelInputOwner`에서 확인할 수 있습니다.

이 중재는 물리 방향 조합을 판단하지 않습니다. ButtonApp이 결정한 논리 방향
Report와 WheelApp의 Encoder Report 중 어느 입력을 CAN에 반영할지만 결정합니다.

## 7. ComM 모드

`lcan_comm.c/.h`는 ComM UserRequest RTE 호출과 모드 변환을 캡슐화하고
다음 프로젝트 모드를 제공합니다.

```c
LCAN_COM_MODE_NO_COM
LCAN_COM_MODE_SILENT_COM
LCAN_COM_MODE_FULL_COM
LCAN_COM_MODE_INVALID
```

공개 API:

```c
LcanCom_GetCurrentMode();
LcanCom_GetRequestedMode();
LcanCom_RequestMode();
```

- Current mode는 ComM이 실제 적용한 통신 모드입니다.
- Requested mode는 가장 최근 요청된 통신 모드입니다.
- 두 값은 상태 천이 중 일시적으로 다를 수 있습니다.

Power FSM의 LCAN sleep 진입은 이 API를 이용하여 `NO_COM`을 요청하고 실제
적용 상태를 확인합니다.

## 8. 실행 주기

| Runnable/API | 주기 또는 조건 | 역할 |
|---|---|---|
| `RE_LCAN_Input_10ms_Process()` | 10ms | 입력 모듈 cyclic 처리 |
| `RE_LCAN_Output_10ms_Process()` | 10ms | TX 허용 조건 확인 후 출력 snapshot을 RTE에 기록 |
| `LcanVersion_MainFunction10ms()` | TX 허용 중 10ms | 버전 요청 반복 응답 처리 |
| `RE_SWC_Wheel_Direction_Event()` | RTE data event | Wheel 리포트 수신 |
| `RE_SWC_Knob_Button_Event()` | RTE data event | Button 리포트 수신 |
| HU message runnable | CAN 수신 event | 메시지별 RTE Read 및 snapshot 갱신 |
| HU timeout runnable | timeout event | 해당 메시지 validity 무효화 |

## 9. 안전성 및 동시성

- 고정 크기 context와 queue만 사용하며 동적 메모리를 사용하지 않습니다.
- RTE Read 결과가 `E_OK`일 때만 신호를 유효한 값으로 취급합니다.
- 수신 timeout은 데이터 자체를 임의 변경하기보다 validity를 무효화합니다.
- ISR에서는 애플리케이션 FSM을 직접 dispatch하지 않습니다.
- ISR/태스크 또는 선점 태스크 사이에서 공유되는 큐는 target critical
  section으로 보호해야 합니다.

## 부록: PC 예제용 CAN 모듈

`can_input`, `can_output`, `drv_can`은 AUTOSAR CAN stack을 대체하는 양산용
구현이 아니라 FSM 동작과 testcase를 검증하기 위한 PC 예제 모듈입니다.

| 파일 | PC 예제 역할 |
|---|---|
| `can_input.c/.h` | CAN 입력 FSM |
| `can_output.c/.h` | CAN 출력 FSM 및 TX 허용 상태 |
| `drv_can.c/.h` | 고정 크기 CAN RX 큐와 송수신 driver |

`drv_can`은 동적 메모리 없이 8개 frame을 저장하는 FIFO를 사용합니다.
`can_input`은 10ms마다 frame을 처리하고, `can_output`은 Online/Offline 상태와
별도로 `txAllowed`를 확인하여 물리 전송 허용 여부를 결정합니다.

| 모듈 | 주요 standalone API |
|---|---|
| CAN input | `CanInput_Init/MainFunction10ms`, Start/Stop, BusOff/Recover 요청 |
| CAN output | `CanOutput_Init/MainFunction10ms`, Online/Offline, power state 및 TX 허용 설정 |
| CAN driver | `DrvCan_Init`, RX 주입/수신, TX 허용 설정 및 frame 전송 |

AUTOSAR 생성 헤더에 의존하는 `SWC_LCAN.c`, `lcan_input_rte.c`,
`lcan_version.c`, `lcan_comm.c`, `SWC_IPdu.c`는 PC 예제 Makefile에서 직접
컴파일하지 않습니다.

## 부록: 테스트 및 Trace32 확인

LCAN 입력은 `LcanInput_GetContext()`가 반환하는 통합 context에서 확인합니다.
Wheel/Button 출력 trace는 `LCAN_OUTPUT_TRACE_ENABLE == 1U`일 때만 RAM에
생성됩니다.

```c
gLcanWheelReportLog
gLcanWheelReportLastRawData
gLcanWheelReportLastDirection
gLcanWheelReportLastCount
gLcanWheelReportLastCanData

gLcanButtonReportLog
gLcanButtonReportLastRawData
gLcanButtonReportLastButtonId
gLcanButtonReportLastSignal
gLcanButtonReportLastValid
```

양산 빌드에서 trace RAM이 필요하지 않으면 `LCAN_OUTPUT_TRACE_ENABLE`을
`0U`로 설정합니다.

