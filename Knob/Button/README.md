# Button Application

## 1. 개요

Button 모듈은 Knob 및 Wheel 버튼의 active-low 디지털 입력을 읽고 다음 기능을 수행합니다.

- 버튼별 독립 FSM 관리
- 30 ms chattering 제거(debounce)
- 800 ms 기준 Short Key와 Long Key 구분
- 버튼 ID와 신호 값을 결합한 16-bit Report 생성
- Report queue를 통한 RTE 전달 재시도 지원

버튼 처리는 `SWC_KnobButton`의 10 ms runnable에서 수행됩니다.

## 2. 기능 사양 요약

| 기능 | 기능 동작 |
|---|---|
| 입력 수집 | 9개 active-low 디지털 입력을 버튼 ID별 raw 상태로 저장합니다. |
| Debounce | Press와 Release 입력이 30ms 동안 유지되어야 안정 상태로 확정합니다. |
| Key 분류 | 800ms 미만 누름은 Short Key, 800ms 이상 누름은 Long Key로 구분합니다. |
| Enter 결합 | ENTER1과 ENTER2 중 하나 이상이 눌리면 하나의 Enter 논리 출력으로 처리합니다. |
| 방향 결합 | Up/Down/Left/Right 조합을 0~315도의 8방향 논리 출력으로 변환합니다. |
| Report 전달 | 논리 Output ID와 Signal을 16-bit 값으로 패킹하고 FIFO로 전달합니다. |
| 전송 재시도 | RTE 전송 성공을 확인한 뒤에만 queue에서 report를 제거합니다. |
| 고착 진단 | 같은 물리 버튼이 11분 동안 안정적으로 눌리면 stuck 상태를 보고합니다. |

## 3. 파일 구성

| 파일 | 역할 |
|---|---|
| `button_app.c` | 버튼 FSM, debounce, 누름 시간 측정, Report queue 관리 |
| `button_app.h` | 상태, 이벤트, 신호, Context 및 외부 API 정의 |
| `drv_button.c` | 버튼별 raw 입력 저장 및 읽기 |
| `drv_button.h` | 버튼 ID와 active-low 입력 값 정의 |
| `SWC_KnobButton.c` | RTE 입력 수집, 10 ms 처리 및 Report 전송 |
| `../../Comm/SWC_LCAN.c` | RTE 버튼 Report 수신 및 Output 모듈 전달 |
| `../../Comm/lcan_output.c` | 버튼 Report 해석 및 진단 로그 저장 |

## 4. 버튼 목록

| Button ID | 값 | 설명 |
|---|---:|---|
| `BUTTON_ID_APPS` | 0 | APPS 버튼 |
| `BUTTON_ID_BACK` | 1 | Back 버튼 |
| `BUTTON_ID_ENTER1` | 2 | Enter 1 버튼 |
| `BUTTON_ID_ENTER2` | 3 | Enter 2 버튼 |
| `BUTTON_ID_SEARCH` | 4 | Search 버튼 |
| `BUTTON_ID_WHEEL_DOWN` | 5 | Wheel Down 버튼 |
| `BUTTON_ID_WHEEL_LEFT` | 6 | Wheel Left 버튼 |
| `BUTTON_ID_WHEEL_RIGHT` | 7 | Wheel Right 버튼 |
| `BUTTON_ID_WHEEL_UP` | 8 | Wheel Up 버튼 |

## 5. 입력 극성

버튼 입력은 active-low입니다.

| Raw 값 | 매크로 | 의미 |
|---:|---|---|
| `0U` | `BUTTON_RAW_STATE_PRESSED` | 버튼 눌림 |
| `1U` | `BUTTON_RAW_STATE_RELEASED` | 버튼 해제 |

`DrvButton_SetInput()`은 Button ID와 raw 값이 모두 유효할 때만 입력을 갱신합니다. 유효하지 않은 ID 또는 `0U`, `1U` 이외의 값은 거부합니다.

## 6. FSM 상태와 이벤트

### 상태

| 상태 | 설명 |
|---|---|
| `BUTTON_STATE_RELEASED` | 안정적으로 해제된 상태 |
| `BUTTON_STATE_DEBOUNCE_PRESS` | 눌림 입력의 debounce 확인 상태 |
| `BUTTON_STATE_PRESSED` | 안정적으로 눌린 상태 |
| `BUTTON_STATE_DEBOUNCE_RELEASE` | 해제 입력의 debounce 확인 상태 |

### 이벤트

| 이벤트 | 발생 조건 |
|---|---|
| `BUTTON_EVENT_RAW_PRESS` | raw 입력이 `0U`로 변경됨 |
| `BUTTON_EVENT_RAW_RELEASE` | raw 입력이 `1U`로 변경됨 |
| `BUTTON_EVENT_DEBOUNCE_DONE` | 동일한 raw 입력이 30 ms 동안 유지됨 |

### 상태 천이

```text
RELEASED
  | RAW_PRESS
  v
DEBOUNCE_PRESS -- RAW_RELEASE --> RELEASED
  | DEBOUNCE_DONE (30 ms)
  v
PRESSED
  | RAW_RELEASE
  v
DEBOUNCE_RELEASE -- RAW_PRESS --> PRESSED
  | DEBOUNCE_DONE (30 ms)
  v
RELEASED
```

Chattering 도중 입력이 원래 상태로 돌아오면 안정 상태로 복귀하며 잘못된 Press 또는 Release Report를 생성하지 않습니다.

## 7. Short Key 및 Long Key 판정

- `BUTTON_DEBOUNCE_TIME_MS`: 30 ms
- `BUTTON_LONG_KEY_TIME_MS`: 800 ms
- 기본 실행 주기: 10 ms

누름 시간은 raw 입력이 눌림으로 바뀐 시점부터 누적됩니다. 안정적인 Press 판정은 30 ms debounce 완료 후 발생합니다.

### Short Key

800 ms에 도달하기 전에 버튼이 해제되고 Release debounce가 완료되면 Short Key로 판정합니다.

```text
Press debounce 완료 : PRESSED, INVALID
Release debounce 완료: SHORT_KEY, CLEAR
```

### Long Key

안정적인 Press 상태에서 누름 시간이 800 ms 이상이 되면 Long Key를 한 번만 보고합니다.

```text
Press debounce 완료 : PRESSED, INVALID
800 ms 도달         : LONG_KEY, INVALID
Release debounce 완료: CLEAR
```

`INVALID`는 CAN 신호를 pulse 형태로 전달하기 위해 유효 신호 뒤에 추가되는 값입니다. `CLEAR`는 버튼 해제 후 최종 Key Off 값을 나타냅니다.

## 8. Report 형식

`ButtonApp_GetReport()`가 반환하는 `uint16` 값은 다음과 같습니다.

```text
15              8 7               0
+----------------+-----------------+
| Logical Output |  Button Signal  |
+----------------+-----------------+
```

```c
reportData = ((uint16)outputId << BUTTON_REPORT_ID_SHIFT)
           | ((uint16)signal & BUTTON_REPORT_SIGNAL_MASK);
```

상위 바이트는 물리 `Button_IdType`이 아니라 조합 처리가 끝난
`Button_OutputType`입니다. APPS, Back, Search, Enter 및 0~315도의 8방향
논리 출력을 구분합니다.

| Signal | 값 | 의미 |
|---|---:|---|
| `BUTTON_SIGNAL_CLEAR` | `0x00U` | Clear / Key Off |
| `BUTTON_SIGNAL_PRESSED` | `0x01U` | Pressed |
| `BUTTON_SIGNAL_SHORT_KEY` | `0x02U` | Short Key |
| `BUTTON_SIGNAL_LONG_KEY` | `0x03U` | Long Key |
| `BUTTON_SIGNAL_INVALID` | `0x0FU` | Invalid |

예를 들어 `0x0102U`는 `BUTTON_OUTPUT_BACK`의 `SHORT_KEY` Report입니다.

### Enter 및 방향 조합

`ENTER1`과 `ENTER2`는 하나의 `BUTTON_OUTPUT_ENTER`로 결합합니다. 두 입력 중
하나 이상이 눌려 있으면 Enter를 유지하고, 두 입력이 모두 해제되어야 Enter
Short/Long 및 Clear 처리를 완료합니다.

Up, Down, Left, Right는 모든 물리 버튼 FSM을 처리한 뒤 한 번만 평가합니다.
인접한 두 방향은 45도 단위의 대각선 출력으로 변환합니다. 논리 방향이
변경되면 이전 출력의 Short/Clear를 생성한 다음 새 출력의 Pressed/Invalid를
생성합니다. 서로 반대인 두 방향이나 3개 이상의 동시 입력은 논리 방향으로
선택하지 않습니다.

## 9. Report 전달 경로

```text
Digital Input RTE Call
    -> DrvButton_SetInput()
    -> ButtonApp_MainFunction10ms()
    -> ButtonApp_GetReport()
    -> Rte_Write_P_u16_Knob_Button_KnobButtonDetected()
    -> RE_SWC_Knob_Button_Event()
    -> LCAN_KnobButtonReport_Store()
```

`ButtonApp_GetReport()`는 queue의 첫 항목을 조회하지만 제거하지 않습니다. RTE Write가 `E_OK`를 반환한 경우에만 `ButtonApp_ConfirmReport()`가 항목을 제거합니다. 따라서 일시적인 RTE Write 실패 시 다음 10 ms 실행에서 동일한 Report를 다시 전송합니다.

현재 `SWC_LCAN.c`는 RTE Report를 수신하여 `lcan_output.c`로 전달합니다.
`lcan_output.c`는 이미 결정된 논리 Output ID와 Signal을 해당 CAN 출력
snapshot에 복사하며, 물리 버튼 조합을 판단하지 않습니다.

## 10. Report Queue

- Queue 크기: 32개
- 방식: FIFO circular queue
- Queue가 가득 차면 신규 Report를 저장하지 않습니다.
- 개별 버튼 Report 저장 실패 시 해당 Context의
  `reportQueueOverflowCount`가 증가합니다.
- Enter/방향 조합 Report 저장 실패 시
  `ButtonApp_GroupReportQueueOverflowCount`가 증가합니다.
- Overflow counter는 `uint16` 최댓값에서 포화됩니다.

한 번의 버튼 동작으로 여러 Report가 생성되므로 CAN/RTE 소비 속도와 queue 사용량을 함께 확인해야 합니다.

## 11. 버튼 고착 진단

각 물리 버튼은 30ms debounce가 완료된 안정 Press 상태부터 독립적으로
누름 시간을 누적합니다. 동일한 버튼이 11분(`660000 ms`) 동안 연속으로
눌려 있으면 해당 버튼을 stuck switch로 판정합니다. 다른 버튼의 누름
시간은 서로 합산하지 않습니다.

Release debounce 중에는 짧은 채터링으로 진단 시간이 초기화되지 않도록
누적값과 고장 상태를 보존합니다. 30ms Release가 확정되면 해당 버튼의
진단 시간과 고장 상태를 초기화합니다. 현재 진단은 래치 방식이 아닙니다.

`ButtonApp_GetDiagnosticStatus()`는 모든 물리 버튼의 결과를 통합하여 다음
값을 반환하며 `CCP_BtnState` CAN 신호에 사용됩니다.

| 값 | 매크로 | 의미 |
|---:|---|---|
| `1U` | `BUTTON_DIAG_STATUS_NORMAL` | 모든 버튼 정상 |
| `2U` | `BUTTON_DIAG_STATUS_STUCK` | 하나 이상의 버튼이 11분 이상 눌림 |

## 12. 주요 API

| API | 설명 |
|---|---|
| `ButtonApp_Init()` | Driver, Context, FSM 및 Report queue 초기화 |
| `ButtonApp_MainFunction10ms()` | 10 ms 기준 전체 버튼 처리 |
| `ButtonApp_MainFunction5ms()` | 5 ms 스케줄러 사용 시 전체 버튼 처리 |
| `ButtonApp_GetReport()` | 전송 대기 중인 첫 Report 조회 |
| `ButtonApp_ConfirmReport()` | 성공적으로 전송된 첫 Report 제거 |
| `ButtonApp_GetPressedMask()` | 현재 안정적으로 눌린 버튼 bit mask 반환 |
| `ButtonApp_GetDiagnosticStatus()` | 통합 버튼 고착 진단값 반환 |
| `ButtonApp_GetContext()` | 기본 APPS 버튼의 read-only Context 반환 |
| `ButtonApp_GetContextById()` | 지정 버튼의 read-only Context 반환 |
| `DrvButton_Init()` | 모든 raw 입력을 Release 상태로 초기화 |
| `DrvButton_Read()` | 기본 APPS 버튼 raw 입력 반환 |
| `DrvButton_ReadById()` | 지정 버튼 raw 입력 반환 |
| `DrvButton_SetInput()` | 지정 버튼의 active-low raw 입력 갱신 |

5 ms 함수와 10 ms 함수를 동시에 호출하면 시간 누적이 중복되므로 실제 스케줄러에서는 둘 중 하나만 사용해야 합니다.

## 13. 동시성 고려사항

현재 버튼 raw 입력 갱신과 `ButtonApp_MainFunction10ms()` 처리는 같은 10 ms
runnable에서 순차 실행됩니다. 다른 ISR 또는 Task에서
`DrvButton_SetInput()`이나 Button API를 호출하도록 변경하는 경우에는 공유
Context와 Report queue에 대한 임계구역 보호가 필요합니다.

## 부록: PC 입력 주입

Standalone testcase는 다음 driver API로 active-low 입력을 모사합니다.

| API | 기능 |
|---|---|
| `DrvButton_SetSimulatedInput()` | 기본 APPS 버튼의 Press/Release 입력 주입 |
| `DrvButton_SetSimulatedInputById()` | 지정 버튼의 Press/Release 입력 주입 |

## 부록: 테스트 및 Trace32 확인

각 버튼은 별도의 `Button_ContextType`을 사용합니다.

| Context 멤버 | 설명 |
|---|---|
| `id` | 버튼 ID |
| `state` | 외부 확인용 FSM 상태 mirror |
| `event` | 마지막 처리 이벤트 mirror |
| `rawState` | 현재 active-low raw 입력 |
| `debounceElapsedMs` | debounce 누적 시간 |
| `holdElapsedMs` | 누름 누적 시간 |
| `diagnosticHoldElapsedMs` | 11분 고착 진단용 안정 Press 누적 시간 |
| `stuckFault` | 해당 물리 버튼의 고착 고장 상태 |
| `longKeyReported` | Long Key 보고 완료 여부 |
| `lastSignal` | 개별 출력에서 전송 확인이 끝난 마지막 Signal |
| `reportQueueOverflowCount` | 해당 버튼 Report 저장 실패 횟수 |
| `fsm` | 실제 FSM Core Context |

Trace32에서 실제 FSM 상태를 강제로 변경해야 하는 경우 mirror인 `state`가 아니라 `fsm.state`를 변경해야 합니다. 다음 주기에는 `state`가 `fsm.state` 값으로 다시 갱신됩니다.

LCAN 수신 결과는 다음 변수로 확인할 수 있습니다.

```c
gLcanButtonReportLastRawData
gLcanButtonReportLastOutputId
gLcanButtonReportLastSignal
gLcanButtonReportLastValid
gLcanButtonReportLog
```

ButtonApp의 조합 상태는 `ButtonApp_EnterGroup`, `ButtonApp_DirectionGroup`,
`ButtonApp_PressedMask`에서 확인할 수 있습니다.
