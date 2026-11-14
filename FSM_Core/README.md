# FSM Core

## 1. 개요

FSM Core는 여러 애플리케이션 기능이 공유하는 이벤트 큐 기반 전이 테이블
FSM과 중앙 1ms one-shot timer를 제공합니다. 각 기능은 동일한 Core API를
사용하지만 상태, 이벤트, 전이 테이블, callback 및 runtime context는 독립적으로
소유합니다.

설계 목표는 다음과 같습니다.

- 정적 메모리만 사용하는 결정론적 실행
- 상태와 이벤트를 애플리케이션별 enum으로 관리
- 전이 조건과 실제 동작을 guard/action으로 분리
- entry/exit/do callback 제공
- 동일 context의 실행 재진입 차단 및 거부 횟수 진단
- 상태/전이 개수를 `sizeof`로 자동 계산하고 설정 배열 크기를 컴파일 시 검증
- 초기화 시 설정된 개수 내에서 state/event 값과 callback을 검증
- ISR과 태스크가 공유하는 timer 상태 보호

## 2. 기능 사양 요약

| 기능 | 기능 동작 |
|---|---|
| FSM 초기화 | 상태·이벤트 범위와 모든 callback을 검증한 뒤 초기 상태 entry를 실행합니다. |
| 이벤트 등록 | FSM별 고정 길이 FIFO에 이벤트를 저장하고 overflow 시 `STD_NOT_OK`를 반환합니다. |
| 이벤트 dispatch | 한 번 호출할 때 이벤트를 최대 한 개 소비하고 첫 번째 일치 전이를 처리합니다. |
| 상태 전이 | Guard가 허용하면 `exit → action → state 변경 → entry` 순서로 실행합니다. |
| 주기 동작 | `Fsm_Do()` 호출 시 현재 상태의 `doAction`을 한 번 실행합니다. |
| 재진입 방어 | Init/Dispatch/Do 실행 중 동일 context의 실행 요청을 거부하고 진단 횟수를 누적합니다. |
| 설정 방어 | 잘못된 state/event 범위와 NULL callback을 초기화 및 실행 시점에 차단합니다. |
| 설정 배열 검증 | `FSM_DEFINE_CONFIG()` 사용 시 상태 배열 길이와 STATE_MAX의 일치, count 표현 범위를 컴파일 시 확인합니다. |
| 중앙 timer | 기능별 ID의 1ms one-shot timer를 제공하며 만료 상태를 latch합니다. |
| 동시성 보호 | ISR과 태스크가 공유할 수 있는 timer 다중 필드 접근을 임계구역으로 보호합니다. |

## 3. 파일 구성

| 파일 | 역할 |
|---|---|
| `fsm.c/.h` | FSM 초기화, 이벤트 등록, 전이 dispatch 및 do 실행 |
| `fsm_queue.c/.h` | FSM instance별 고정 크기 FIFO ring queue |
| `fsm_timer.c/.h` | 기능별 ID를 사용하는 중앙 1ms one-shot timer |

### 공개 API

| API | 기능 |
|---|---|
| `Fsm_Init()` | 설정 검증 후 context와 초기 상태를 초기화합니다. |
| `Fsm_PostEvent()` | 유효한 이벤트를 instance FIFO에 등록합니다. |
| `Fsm_Dispatch()` | FIFO에서 이벤트를 최대 한 개 꺼내 전이를 처리합니다. |
| `Fsm_Do()` | 현재 상태의 `doAction`을 실행합니다. |
| `Fsm_GetState()` | Authoritative FSM state를 반환합니다. |
| `Fsm_GetLastEvent()` | 마지막으로 소비한 event 또는 sentinel을 반환합니다. |
| `Fsm_GetReentryRejectCount()` | 누적 재진입 거부 횟수를 지우지 않고 조회합니다. |
| `FsmQueue_Init/Push/Pop()` | 고정 ring queue를 초기화하고 데이터를 등록·소비합니다. |
| `FsmQueue_IsEmpty/IsFull()` | Queue 상태를 반환합니다. |
| `FSM_Timer_Init()` | 중앙 timer table과 1ms time base를 초기화합니다. |
| `FSM_Timer_Notification()` | 설정된 time base마다 모든 active timer를 갱신합니다. |
| `FSM_Timer_Start/Stop()` | 지정 ID의 one-shot timer를 시작하거나 중지합니다. |
| `FSM_Timer_IsExpired()` | 만료 latch를 읽고 clear합니다. |

## 4. 핵심 구조

### FSM 설정

`Fsm_ConfigType`은 변경되지 않는 정적 설정입니다.

```c
typedef struct
{
    Fsm_StateType initialState;
    Fsm_StateType stateCount;
    Fsm_EventType eventCount;
    const Fsm_StateOperationType *stateOperations;
    const Fsm_TransitionType *transitionTable;
    uint16 transitionCount;
} Fsm_ConfigType;
```

동일한 설정 테이블은 공유할 수 있지만 각 FSM instance는 별도의
`Fsm_ContextType`을 가져야 합니다.

### FSM runtime context

```c
typedef struct
{
    Fsm_StateType state;
    Fsm_EventType lastEvent;
    const Fsm_ConfigType *config;
    FsmQueue_ContextType eventQueue;
    uint8 initialized;
    uint8 executing;
    uint16 reentryRejectCount;
} Fsm_ContextType;
```

`state`는 실제 FSM 상태의 기준값입니다. 애플리케이션 context의 별도 typed
state는 주기적으로 동기화되는 관찰용 mirror일 수 있으므로 상태 판단에는
`Fsm_GetState()` 또는 `fsm.state`를 사용합니다.

context는 최초 API 호출 전에 전체를 zero initialization해야 합니다. 정적 저장
기간의 context 또는 `Fsm_ContextType context = {0};` 형태를 사용합니다.
기능 context에 FSM context를 포함한 경우에도 동일한 초기화 조건이 적용됩니다.
실행 중인 context를 직접 초기화하거나 복사하여 덮어쓰면 안 됩니다.

## 5. 상태 callback

각 상태는 다음 세 callback을 반드시 가져야 합니다.

```c
typedef struct
{
    Fsm_StateFuncType entry;
    Fsm_StateFuncType exit;
    Fsm_StateFuncType doAction;
} Fsm_StateOperationType;
```

| Callback | 실행 시점 |
|---|---|
| `entry` | 초기 상태 진입 또는 전이 완료 후 한 번 |
| `exit` | 현재 상태를 떠나기 직전에 한 번 |
| `doAction` | `Fsm_Do()`가 호출될 때마다 한 번 |

동작이 필요하지 않은 callback도 NULL로 두면 안 됩니다. 모듈별
`EntryNoAction`, `ExitNoAction`, `DoNoAction` 함수를 지정해야 합니다.

### Callback 호출 규칙과 재진입 방어

`Fsm_Init()`, `Fsm_Dispatch()`, `Fsm_Do()`는 context별 `executing` 플래그를
공유합니다. 각 API는 callback을 실행하기 전에 플래그를 설정하고 실행 종료 시
해제합니다. 초기 상태 entry, guard, exit, action, 전이 후 entry, doAction 모두
보호 범위에 포함됩니다.

| Callback에서 호출하는 API | 동일 context에 대한 규칙 |
|---|---|
| `Fsm_Init()` | 금지. 재진입 시 `STD_NOT_OK`를 반환합니다. |
| `Fsm_Dispatch()` | 금지. 재진입 시 이벤트를 소비하지 않고 반환합니다. |
| `Fsm_Do()` | 금지. 재진입 시 callback을 실행하지 않고 반환합니다. |
| `Fsm_PostEvent()` | 허용. 이벤트는 이후 dispatch에서 처리하며 등록 실패를 확인해야 합니다. |
| `Fsm_GetState()`, `Fsm_GetLastEvent()` | 허용. 호출 시점에 저장된 값을 반환합니다. |
| `Fsm_GetReentryRejectCount()` | 허용. 진단 횟수를 변경하지 않습니다. |

guard/exit/action에서 상태를 조회하면 전이 전 상태이며, 전이 후 entry에서는
새 상태입니다. 다른 context는 독립적으로 실행할 수 있지만, context 사이의
callback 호출이 순환하여 이미 실행 중인 context로 돌아오면 해당 호출은 거부됩니다.

재진입 거부 시 state, config, lastEvent, event queue와 실행 소유권은 유지되고,
`reentryRejectCount`만 호출마다 1씩 증가합니다. 횟수는 65535에서 포화되어
wrap되지 않습니다. 정상 재초기화와 잘못된 설정의 초기화 요청도 진단 횟수를
지우지 않습니다. 진단은 최초 zero initialization 이후 계속 유지하며 별도
clear API는 제공하지 않습니다. NULL context의 진단 조회는 0을 반환합니다.

이 플래그는 하나의 cooperative 실행 문맥에서 발생하는 재귀 호출을 방어합니다.
동일 context의 모든 API 접근은 소유 task에서 직렬화해야 합니다. task/ISR 간
배타 보호 기능은 제공하지 않으며, 다중 task 사용 시에는 외부 보호가 필요합니다.
callback에서 context 필드를 직접 변경하거나 내부 `eventQueue`를 직접 조작하지
않고 공개 FSM API를 사용해야 합니다.

## 6. 전이 테이블

전이 한 행은 다음 정보를 가집니다.

```c
typedef struct
{
    Fsm_StateType currentState;
    Fsm_EventType event;
    Fsm_GuardFuncType guard;
    Fsm_ActionFuncType action;
    Fsm_StateType nextState;
} Fsm_TransitionType;
```

권장 선언 형식:

```c
typedef enum
{
    EXAMPLE_STATE_IDLE = 0U,
    EXAMPLE_STATE_ACTIVE,
    EXAMPLE_STATE_MAX
} Example_StateType;

typedef enum
{
    EXAMPLE_EVENT_START = 0U,
    EXAMPLE_EVENT_MAX
} Example_EventType;

/* Callbacks are implemented by the application. Use an inferred array size. */
static const Fsm_StateOperationType Example_StateOperations[] =
{
    { Example_IdleEntry, Example_IdleExit, Example_IdleDo },
    { Example_ActiveEntry, Example_ActiveExit, Example_ActiveDo }
};

static const Fsm_TransitionType Example_Transitions[] =
{
    { (Fsm_StateType)EXAMPLE_STATE_IDLE,
      (Fsm_EventType)EXAMPLE_EVENT_START,
      Example_GuardAlways,
      Example_ActionStart,
      (Fsm_StateType)EXAMPLE_STATE_ACTIVE }
};

FSM_DEFINE_CONFIG(Example_Config,
                  EXAMPLE_STATE_IDLE,
                  EXAMPLE_STATE_MAX,
                  EXAMPLE_EVENT_MAX,
                  Example_StateOperations,
                  Example_Transitions);
```

상태와 이벤트 enum을 generic `uint16` 필드에 넣을 때 명시적 cast를 사용합니다.
설정 선언은 파일 범위에서 `FSM_DEFINE_CONFIG()`를 사용합니다. 이 매크로가
`static const Fsm_ConfigType` 객체를 선언하고, 상태/전이 개수를 해당 배열의
`sizeof`로 계산합니다. 별도의 수동 count와 배열 길이가 달라지는 것을 방지합니다.
매크로에 전달하는 `STATE_MAX`, `EVENT_MAX`에는 미리 `uint16` cast를 적용하지
않습니다. 범위를 검사하기 전에 cast하면 큰 값이 잘려 잘못된 설정을 놓칠 수 있습니다.

배열은 정의가 보이는 완전한 배열이어야 하며 pointer나 함수의 배열 매개변수를
전달하면 안 됩니다. 표준 C99의 `sizeof` 기반 검사는 배열과 pointer를 일반적으로
구분하지 못하므로 이 호출 조건은 사용자가 지켜야 합니다. 상태 배열은 `[]`로
크기를 추론하도록 선언하는 것을 권장합니다. `[STATE_MAX]`로 크기를 고정하면
누락된 초기화 행도 배열 길이에 포함되며, NULL callback은 런타임 검증 대상입니다.

## 7. 이벤트 처리 순서

`Fsm_Dispatch()`는 호출할 때마다 큐에서 이벤트를 최대 한 개만 꺼냅니다.
큐가 비어 있으면 아무 callback도 실행하지 않습니다.

전이가 허용되면 실행 순서는 다음과 같습니다.

```text
1. event queue에서 이벤트 하나 pop
2. transition table을 처음부터 검색
3. currentState와 event가 일치하는 첫 행 선택
4. guard() 실행
5. 현재 상태 exit()
6. transition action()
7. context->state = nextState
8. 새 상태 entry()
```

따라서 다음 예에서는 실행 순서가 `Exit → Action → Entry`입니다.

```c
{ LED_STATE_OFF, LED_EVENT_POWER_ON,
  LedApp_GuardAlways, LedApp_ActionStart, LED_STATE_ON }
```

`doAction()`은 전이 내부에서 자동 호출되지 않으며 애플리케이션이 별도로
`Fsm_Do()`를 호출할 때 실행됩니다.

### 소비되는 이벤트

다음 경우에도 pop한 이벤트는 다시 큐로 돌아가지 않습니다.

- 현재 상태와 일치하는 전이 행이 없음
- guard가 `STD_FALSE` 반환
- dispatch 시점의 방어적 runtime 검증 실패

동일한 state/event 조합을 여러 행에 선언하면 첫 번째 일치 행만 검사합니다.
첫 guard가 실패해도 뒤의 중복 행은 검사하지 않으므로 중복 전이보다는 하나의
guard에서 조건을 명확히 판단하는 방식을 권장합니다.

## 8. 초기화 및 설정 검증

### 컴파일 시 배열 크기 검증

`FSM_DEFINE_CONFIG()`는 C99에서도 사용할 수 있는 typedef 배열 크기 검사로
다음 조건을 확인합니다. 조건을 만족하지 않으면 음수 배열 크기로 컴파일이 실패합니다.

- `STATE_MAX`가 1~65535이고 상태 배열의 원소 수와 같음
- `EVENT_MAX`가 1~65535임
- 전이 배열의 원소 수가 1~65535임 (`uint16` 변환 전에 검사)

상태와 이벤트 enum은 0부터 연속된 값으로 정의하고 sentinel을 마지막에 둡니다.
이 검사는 개수에 대한 검사이며 enum 값의 연속성, 상태 배열의 의미적 순서,
각 전이 행의 값 및 callback 유효성까지 컴파일 시 검증하지는 않습니다.

`Fsm_ConfigType`을 직접 초기화하는 기존 방식은 유지하지만, 이 경우에는 위
컴파일 검사가 적용되지 않습니다. 런타임 API는 pointer에서 실제 배열 길이를
복원할 수 없으므로 정확한 count와 배열 수명은 설정 작성자가 보장해야 합니다.

### 런타임 설정 검증

`Fsm_Init()`은 설정이 유효할 때만 context를 초기화하고 초기 상태의 entry를
호출합니다.

검증 항목:

- config와 table pointer가 NULL이 아님
- `stateCount`, `eventCount`, `transitionCount`가 0보다 큼
- `initialState < stateCount`
- 모든 상태의 entry/exit/do callback이 NULL이 아님
- 모든 전이의 current/next state가 범위 안에 있음
- 모든 전이 event가 범위 안에 있음
- 모든 guard/action callback이 NULL이 아님

초기화 직후 `lastEvent`에는 유효 이벤트가 아닌 `eventCount` sentinel이
저장됩니다.

`Fsm_Init()`은 실행 중인 context에 대해서는 설정을 검사하거나 context/queue를
초기화하기 전에 요청을 거부합니다. 이 경우 진행 중인 FSM 데이터와 실행 소유권을
유지하고 재진입 거부 진단 횟수를 증가시키며 `STD_NOT_OK`를 반환합니다.

실행 중이 아닌 context에 NULL 또는 잘못된 설정이 전달되면 `STD_NOT_OK`를
반환하고, 이전 초기화 성공 여부와 관계없이 다음과 같이 비활성화합니다.

- `initialized = STD_FALSE`
- `config = NULL_PTR`
- `state = 0`, `lastEvent = 0` (비활성 상태의 초기값이며 유효 상태/이벤트를 의미하지 않음)
- 이벤트 큐의 buffer/head/tail/count 초기화
- 재진입 거부 진단 횟수 유지, `executing`은 반환 전에 `STD_FALSE`로 해제

비활성화 후 `Fsm_PostEvent()`는 실패하고 `Fsm_Dispatch()`와 `Fsm_Do()`는
callback을 실행하지 않습니다. 다시 사용하려면 유효한 설정으로 `Fsm_Init()`에
성공해야 합니다. NULL context 요청은 기록하거나 비활성화할 대상 없이 실패합니다.

실패 처리 중에는 기존 상태의 exit를 포함한 callback을 실행하지 않습니다.
중앙 타이머와 하드웨어 출력, 애플리케이션 데이터 정리는 호출자가 수행해야 합니다.
호출자는 반환값을 확인하고 애플리케이션의 오류 처리 정책을 적용해야 합니다.

## 9. 이벤트 큐

각 `Fsm_ContextType`은 독립된 FIFO ring queue를 포함합니다.

| 항목 | 값 |
|---|---:|
| Queue 길이 | 16 events |
| Element 타입 | `uint16` |
| 메모리 방식 | context 내부 고정 배열 |
| Overflow 동작 | 새 이벤트 거부, `STD_NOT_OK` 반환 |

`Fsm_PostEvent()`는 다음 조건을 확인합니다.

- context가 NULL이 아님
- FSM 초기화 완료
- config가 유효함
- `event < eventCount`
- queue에 빈 공간이 있음

이벤트 생산 속도가 `Fsm_Dispatch()` 처리 속도보다 빠르면 queue가 가득 찰 수
있습니다. 호출자는 `STD_NOT_OK`를 확인하고 재시도 또는 overflow 진단
카운터 정책을 구현해야 합니다.

### Queue 동시성

`fsm_queue` 자체에는 `volatile`이나 critical section이 없습니다. 동일한 FSM
queue의 Push/Pop이 하나의 cooperative task에서 실행될 때 사용해야 합니다.

ISR에서 직접 `Fsm_PostEvent()`를 호출하면 안 됩니다. ISR 입력은 Wheel 모듈과
같이 별도의 ISR-safe FIFO/flag에 저장하고, task context에서 FSM 이벤트로
변환하는 방식을 사용합니다. 선점 가능한 여러 태스크가 같은 FSM queue에
접근해야 한다면 module 전용 SchM exclusive area가 필요합니다.

## 10. 기본 사용 순서

```c
/* Example_Context has static storage duration and is zero-initialized. */
Std_ReturnType Example_Init(void)
{
    return Fsm_Init(&Example_Context.fsm, &Example_Config);
}

Std_ReturnType Example_RequestStart(void)
{
    return Fsm_PostEvent(&Example_Context.fsm,
                         (Fsm_EventType)EXAMPLE_EVENT_START);
}

void Example_MainFunction10ms(void)
{
    Fsm_Dispatch(&Example_Context.fsm);
    Fsm_Do(&Example_Context.fsm);
}
```

한 주기에 이벤트를 하나만 처리하는 것이 기본 계약입니다. 여러 번 dispatch하면
한 주기에 여러 상태를 연속 통과할 수 있으므로 timing과 callback 실행량이
달라집니다.

## 11. 중앙 FSM Timer

`fsm_timer`는 기능별 고정 ID를 사용하는 중앙 one-shot timer입니다.

```c
FSM_Timer_Init();
FSM_Timer_Notification();
FSM_Timer_Start(timerId, ticks);
FSM_Timer_Stop(timerId);
FSM_Timer_IsExpired(timerId);
```

현재 time base는 1ms입니다.

```text
ticks = 1     → 1ms
ticks = 10    → 10ms
ticks = 1000  → 1초
```

`FSM_Timer_Start()`의 ticks가 0이거나 ID가 잘못되면 기존 timer 상태를 변경하지
않습니다. Start는 같은 ID의 timer를 재시작하며 기존 counter와 expiration을
덮어씁니다.

### 만료 처리

counter가 0이 되면 다음 상태가 latch됩니다.

```c
active = FALSE;
expired = TRUE;
```

`FSM_Timer_IsExpired()`는 만료 여부를 읽은 뒤 `expired`를 FALSE로 지웁니다.
따라서 한 번의 만료는 한 번만 소비됩니다.

Timer는 FSM event queue에 이벤트를 자동으로 등록하지 않습니다. 각 기능의
주기 함수가 만료를 확인하고 필요한 이벤트를 직접 등록해야 합니다.

```c
if (FSM_Timer_IsExpired(FSM_TIMER_LED_RESET) == TRUE)
{
    (void)Fsm_PostEvent(&LedApp_Context.fsm,
                        (Fsm_EventType)LED_EVENT_RESET_DONE);
}
```

## 12. Timer 범위

counter는 `uint32`이며 1ms 기준 최대 설정 시간은 다음과 같습니다.

```text
4,294,967,295ms
= 4,294,967.295초
= 약 71,582.8분
= 약 1,193.0시간
= 약 49.7일
```

5분은 `300000U` ticks로 설정할 수 있습니다.

## 13. Timer 동시성

Timer table은 GPT 1ms interrupt와 task API가 공유할 수 있으므로 각 member가
`volatile`로 선언되어 있습니다.

```c
volatile uint32 counter;
volatile boolean active;
volatile boolean expired;
```

Start/Stop/IsExpired의 다중 필드 접근과 read-clear 구간은 critical section으로
보호합니다. 현재 MOBILGENE target 구현은 다음 API를 사용합니다.

```c
SuspendAllInterrupts();
ResumeAllInterrupts();
```

전체 interrupt를 중단하므로 critical section 내부에서는 필드 몇 개만
갱신하고 즉시 빠져나옵니다. 양산 프로젝트에서는 OS/BSW 설정이 제공한다면
다음과 같은 module 전용 exclusive area로 교체하는 것이 권장됩니다.

```c
SchM_Enter_FsmTimer_EXCLUSIVE_AREA_0();
SchM_Exit_FsmTimer_EXCLUSIVE_AREA_0();
```

ISR에서 실행되는 `FSM_Timer_Notification()`과 task API의 우선순위 및 target의
원자적 접근 크기를 고려하여 SchM 구현을 선택해야 합니다.

## 14. Timer ID 관리

`FSM_TimerIdType`은 기능별로 그룹화되어 있습니다.

- Power management
- Button input
- LED control
- CAN communication
- NVM
- Knob Wheel

새 timer를 추가할 때는 해당 기능 그룹 안에 ID를 추가하고
`FSM_TIMER_MAX`는 항상 마지막에 유지해야 합니다. Timer storage 크기는
`FSM_TIMER_MAX`로 자동 결정됩니다.

동일한 timer ID를 여러 기능이 동시에 사용하면 서로 Start/Stop 상태를
덮어쓰므로 각 독립 timing 조건마다 전용 ID를 사용해야 합니다.

## 15. 새 FSM 추가 절차

1. 기능별 `StateType`과 마지막 `STATE_MAX` 정의
2. 기능별 `EventType`과 마지막 `EVENT_MAX` 정의
3. 기능 context에 `Fsm_ContextType fsm` 포함
4. 모든 상태의 entry/exit/do callback 작성
5. guard와 action callback 작성
6. state operation table을 enum 순서대로 작성
7. transition table 작성
8. 상태 배열 길이와 STATE_MAX가 일치하도록 작성
9. `FSM_DEFINE_CONFIG()`로 상태/전이 개수를 자동 계산하는 설정 선언
10. 초기화에서 `Fsm_Init()` 결과 확인
11. request API에서 `Fsm_PostEvent()` 결과 반환 또는 확인
12. 주기 함수에서 `Fsm_Dispatch()`와 `Fsm_Do()` 호출

State operation table은 state enum 값으로 직접 인덱싱되므로 enum 순서와 배열
순서가 반드시 일치해야 합니다.

## 16. MISRA-C 및 유지보수 기준

- enum에서 generic state/event 타입으로 변환할 때 명시적 cast 사용
- unsigned 정수 상수에 대문자 `U` suffix 사용
- 모든 switch에 `default` case 작성
- callback pointer를 NULL로 두지 않음
- state/event sentinel을 실제 상태나 이벤트로 사용하지 않음
- transition count를 수동으로 관리하지 않음
- `FSM_DEFINE_CONFIG()`에 완전한 배열과 cast하지 않은 count 상수를 전달함
- 배열과 queue index를 범위 확인 후 사용
- event queue overflow 반환값을 무시하지 않음
- ISR에서는 긴 callback이나 FSM dispatch를 실행하지 않음
- callback에서 동일 context의 Init/Dispatch/Do를 호출하지 않음
- context는 최초 API 호출 전에 zero initialization하고 모든 접근을 직렬화함
- static storage만 사용하고 동적 메모리를 사용하지 않음

## 부록: 테스트 및 Trace32 확인

각 애플리케이션 context의 `fsm`에서 다음 항목을 확인합니다.

```c
fsm.state
fsm.lastEvent
fsm.initialized
fsm.executing
fsm.reentryRejectCount
fsm.config
fsm.eventQueue.buffer
fsm.eventQueue.head
fsm.eventQueue.tail
fsm.eventQueue.count
```

`fsm.state`가 실제 상태이고 애플리케이션의 typed `state`는 주기 함수에서
갱신되는 관찰용 mirror일 수 있습니다.

`fsm.executing`은 Init/Dispatch/Do와 그 callback 실행 중에만 `STD_TRUE`입니다.
`Fsm_GetReentryRejectCount(&context.fsm)` 또는 `fsm.reentryRejectCount`로
재진입 호출 위반을 확인할 수 있습니다. 정상적인 사용에서는 횟수가 0으로 유지됩니다.

호스트 회귀 테스트 실행 방법과 검증 범위는 `tests/README.md`를 참고합니다.

Timer table `Fsm_Timer[]`는 `fsm_timer.c`의 static 변수입니다. debugger가
static symbol을 표시하도록 ELF debug symbol을 유지해야 내부 counter,
active, expired 값을 확인할 수 있습니다.

