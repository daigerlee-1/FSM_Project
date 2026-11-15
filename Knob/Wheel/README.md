# Wheel Application

## 1. 개요

Wheel 모듈은 두 개의 포토센서 인터럽트로부터 회전 방향과 이동 횟수를
검출합니다. ISR에서는 센서 스냅샷만 고정 크기 큐에 저장하고, FSM 처리와
RTE 전송은 10ms 태스크에서 수행합니다.

주요 기능은 다음과 같습니다.

- 포토센서 INT1/INT2 순서에 따른 좌·우 회전 판정
- ISR과 태스크 사이의 입력 FIFO
- 60ms 구간 동안 방향별 이동 횟수 누적
- RTE 전송 실패 시 동일 리포트 재시도
- Move Info 전송 후 30ms 유지 및 `0xFF` Invalid 전송

## 2. 기능 사양 요약

| 기능 | 기능 동작 |
|---|---|
| ISR 입력 수집 | INT1/INT2 callback에서 두 포토센서의 레벨과 edge 종류를 FIFO에 저장합니다. |
| 방향 판정 | 첫 edge와 반대 센서 edge의 순서 및 레벨 패턴으로 좌·우 회전을 판정합니다. |
| Edge timeout | 두 번째 edge가 30ms 안에 없으면 진행 중인 방향 판정을 취소합니다. |
| Count 집계 | 확정된 이동을 방향별로 60ms 동안 누적하며 한 report의 최대 count는 15입니다. |
| Report 전달 | 방향과 count를 8-bit 값으로 패킹하고 RTE 성공 확인 후 FIFO에서 제거합니다. |
| Invalid 출력 | Move Info를 30ms 유지한 뒤 `0xFF` Invalid report를 한 번 생성합니다. |
| 입력 상호 배제 | LCAN 출력에서 Wheel encoder와 8-way 방향 입력 중 먼저 활성화된 입력을 우선합니다. |
| Overflow 진단 | 입력, FSM event, count 및 report queue overflow를 포화 counter로 기록합니다. |

## 3. 파일 구성

| 파일 | 역할 |
|---|---|
| `wheel_app.c` | Wheel FSM, 입력 큐, 방향 판정, count 집계 및 리포트 큐 |
| `wheel_app.h` | 상태, 이벤트, context, 매크로 및 공개 API |
| `SWC_KnobWheel.c` | ICU/RTE 어댑터와 10ms runnable |
| `SWC_KnobWheel.h` | AUTOSAR 통합용 공개 선언 |

`SWC_KnobWheel.c`는 생성된 `Rte_SWC_KnobWheel.h`와 MemMap 헤더가 있는
AUTOSAR 대상 빌드에서 사용합니다.

## 4. 실행 컨텍스트

### ICU ISR

다음 콜백은 ISR 컨텍스트에서 실행됩니다.

```c
RE_PHOTO_INT1_Cbk();
RE_PHOTO_INT2_Cbk();
```

ISR에서는 두 센서 레벨을 읽고 다음 API로 스냅샷을 입력 FIFO에 저장합니다.

```c
WheelApp_NotifyPhoto1(photo1Level, photo2Level);
WheelApp_NotifyPhoto2(photo1Level, photo2Level);
```

ISR에서 FSM dispatch나 방향 판정을 수행하지 않습니다. 입력 FIFO 접근 구간은
대상 환경에서 interrupt critical section으로 보호됩니다.

### 10ms 태스크

`RE_ICU_Photo_Int_10ms_Task()`는 다음 순서로 실행합니다.

```text
WheelApp_MainFunction10ms()
→ WheelApp_GetReport()
→ Rte_Write_P_u8_Wheel_Direction_KnobWheelDirection()
→ 성공 시 WheelApp_ConfirmReport()
```

RTE Write가 실패하면 리포트를 제거하지 않고 다음 10ms 주기에 재전송합니다.

## 5. FSM

### 상태

| 상태 | 설명 |
|---|---|
| `KNOB_WHEEL_STATE_IDLE` | 첫 번째 센서 edge 대기 |
| `KNOB_WHEEL_STATE_WAIT_INT1` | INT2 이후 INT1 대기 |
| `KNOB_WHEEL_STATE_WAIT_INT2` | INT1 이후 INT2 대기 |

### 이벤트

| 이벤트 | 설명 |
|---|---|
| `KNOB_WHEEL_EVENT_INT1` | INT1 센서 스냅샷 입력 |
| `KNOB_WHEEL_EVENT_INT2` | INT2 센서 스냅샷 입력 |
| `KNOB_WHEEL_EVENT_WAIT_TIMEOUT` | 반대 센서 edge 대기시간 만료 |

### 기본 전이

```text
IDLE + INT1 → WAIT_INT2
WAIT_INT2 + INT2 → 방향 판정 → IDLE

IDLE + INT2 → WAIT_INT1
WAIT_INT1 + INT1 → 방향 판정 → IDLE

WAIT_INT1/WAIT_INT2 + TIMEOUT → IDLE
```

센서 조합이 유효한 방향 패턴과 일치할 때만 이동 count가 증가합니다.

## 6. 타이밍

| 설정 | 값 | 용도 |
|---|---:|---|
| `KNOB_WHEEL_EDGE_TIMEOUT_TICKS` | 30ms | 두 번째 edge 최대 대기시간 |
| `KNOB_WHEEL_COUNT_WINDOW_TICKS` | 60ms | 방향별 이동 횟수 집계 구간 |
| `KNOB_WHEEL_INVALID_DELAY_TICKS` | 30ms | Move Info 유지시간 |

타이머는 `FSM_Timer_Notification()`의 1ms time base를 사용합니다. 만료 결과는
10ms Wheel 태스크에서 소비합니다.

## 7. 리포트 처리

Wheel 앱 내부 리포트는 상위 4비트에 방향, 하위 4비트에 count를 저장합니다.

```text
7             4 3             0
+---------------+---------------+
|   Direction   |     Count     |
+---------------+---------------+
```

내부 count 최대값은 `0x0F`이며, 리포트 큐 크기는 16개입니다.

60ms 집계가 끝나면 방향별 count를 리포트 큐에 넣습니다. Move Info가 RTE에
전달되면 30ms 타이머를 시작하고, 타이머 만료 후 `0xFF` Invalid 리포트를 한
번 전달합니다. 대기 중인 다음 Move Info는 Invalid 리포트 이후에 처리합니다.

```text
Move Info → 30ms 유지 → 0xFF Invalid → 다음 Move Info
```

## 8. CAN 변환

`SWC_LCAN.c`가 Wheel RTE 리포트를 수신하고 `lcan_output.c`가 다음 규칙으로
`CCP_WhlEncdr` 값을 생성합니다.

| 방향/count | CAN 값 |
|---|---:|
| Clockwise 1~127 | `0x01~0x7F` |
| Counterclockwise 1~127 | `0x80~0xFE` |
| Invalid | `0xFF` |

현재 Wheel 앱 내부 count 범위는 1~15입니다. 예를 들어 Clockwise 4 step은
`0x04`, Counterclockwise 4 step은 `0x83`으로 전송합니다.

최종 RTE 출력은 다음 API를 사용합니다.

```c
Rte_Write_LCAN_L_CCP_E_Wheel_CCP_WhlEncdr(wheelEncoder);
```

LCAN 출력 계층에서는 Encoder와 8-way 방향 버튼에 먼저 활성화된 입력 우선의
상호 배제를 적용합니다. 8-way가 우선권을 가진 동안 Encoder Report는 CAN에
반영되지 않고 `CCP_WhlEncdr`가 `0xFF`로 유지됩니다. Encoder가 우선권을 가진
동안에는 모든 8-way 방향 출력이 `CLEAR`로 유지됩니다.

## 9. 큐와 오버플로

| 큐 | 크기 | 생산자 | 소비자 |
|---|---:|---|---|
| 입력 큐 | 16 | ICU ISR | 10ms Wheel 태스크 |
| 리포트 큐 | 16 | 방향 집계 로직 | RTE 어댑터 |

큐가 가득 차면 새 항목을 덮어쓰지 않고 진단 카운터를 포화 증가시킵니다.

```c
inputQueueOverflowCount
reportQueueOverflowCount
eventQueueOverflowCount
countOverflowCount
```

## 10. 주요 API

| API | 설명 |
|---|---|
| `WheelApp_Init()` | context, FIFO, FSM 및 타이머 초기화 |
| `WheelApp_MainFunction10ms()` | 입력 처리, 타이머 확인, FSM 및 리포트 처리 |
| `WheelApp_NotifyPhoto1()` | INT1 ISR 스냅샷 등록 |
| `WheelApp_NotifyPhoto2()` | INT2 ISR 스냅샷 등록 |
| `WheelApp_GetReport()` | 전송 대기 리포트 조회 |
| `WheelApp_ConfirmReport()` | 성공적으로 전송한 리포트 제거 |
| `WheelApp_GetContext()` | read-only context 조회 |

## 11. 안전성 및 동시성

- ISR에서는 FSM dispatch, 방향 판정 및 RTE Write를 실행하지 않습니다.
- ISR과 10ms 태스크가 공유하는 입력 FIFO의 Push/Pop은 target critical
  section으로 보호합니다.
- 유효한 센서 레벨은 `0U`와 `1U`이며 다른 값의 notification은 거부합니다.
- Queue가 가득 차면 기존 데이터를 덮어쓰지 않고 신규 입력 또는 report를
  거부합니다.
- 공개 context는 read-only 포인터로 제공하며 상태 변경은 공개 API를 통해
  수행합니다.

## 부록: PC 빌드

PC 예제 Makefile은 RTE 생성물에 의존하는 `SWC_KnobWheel.c`를 직접
컴파일하지 않고 `wheel_app.c`의 애플리케이션 로직만 사용합니다.

## 부록: 테스트 및 Trace32 확인

```c
KnobWheel_Context.state
KnobWheel_Context.event
KnobWheel_Context.rightCount
KnobWheel_Context.leftCount
KnobWheel_Context.countWindowActive
KnobWheel_Context.reportQueueCount
KnobWheel_Context.reportPending
KnobWheel_Context.reportData
KnobWheel_Context.invalidReportPending
KnobWheel_Context.invalidReportDue
KnobWheel_Context.invalidReported
```

LCAN 변환 결과는 trace 기능이 활성화된 경우 다음 값으로 확인할 수 있습니다.

```c
gLcanWheelReportLastRawData
gLcanWheelReportLastDirection
gLcanWheelReportLastCount
gLcanWheelReportLastCanData
gLcanWheelReportLog
```

