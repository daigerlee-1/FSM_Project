# FSM Project

자동차 임베디드 C 환경에서 사용하는 이벤트 기반 FSM Core와 프로젝트 공통
헤더를 관리하는 저장소입니다. 정적 메모리 기반의 전이 테이블, 인스턴스별 이벤트
큐, 중앙 1ms one-shot timer를 제공합니다.

MOBILGENE/AUTOSAR 타깃 연동과 Windows 호스트 테스트를 구분하여 사용합니다.
실제 MCU용 실행 파일을 만드는 IDE 프로젝트, 링커 설정, MCAL/OS 및 애플리케이션
전체 소스는 포함되어 있지 않습니다.

## 1. 저장소 구성

```text
FSM_Project/
├── README.md
├── Common/
│   ├── prj_defs.h              # 프로젝트 및 빌드 환경 설정
│   └── prj_typedefs.h          # 공통 타입과 논리값/반환값 정의
└── FSM_Core/
    ├── README.md              # FSM/타이머 상세 계약과 사용 예제
    ├── fsm.c / fsm.h          # 설정 검증, 전이, 재진입 방어 및 진단
    ├── fsm_queue.c / .h       # 인스턴스별 고정 크기 FIFO
    ├── fsm_timer.c / .h       # 고정 ID 기반 중앙 타이머
    └── tests/                 # GCC 호스트 회귀 테스트 및 테스트용 타입
```

- [FSM Core 상세 문서](FSM_Core/README.md)
- [호스트 테스트 상세 문서](FSM_Core/tests/README.md)
- [프로젝트 설정](Common/prj_defs.h)
- [프로젝트 공통 타입](Common/prj_typedefs.h)

## 2. 빌드 환경 선택

`PRJ_MOBILGENE_ENABLE`은 `Common/prj_defs.h`에서 기본값을 정의하며,
컴파일 옵션으로 덮어쓸 수 있습니다. 현재 main 브랜치의 기본값은 `0U`입니다.

| 설정 | 타입 정의 | 타이머 보호 | 용도 |
|---|---|---|---|
| `1U` | 타깃의 `Platform_Types.h`, `Std_Types.h` | `Os.h`의 interrupt 차단/복원 API | MOBILGENE/AUTOSAR 통합 |
| `0U` | C99 `<stdint.h>` 기반 공통 타입 | critical section이 빈 동작 | 직렬 실행하는 호스트 테스트 |

빌드 옵션에는 `0U` 또는 `1U`를 사용합니다. 환경 설정 처리 시점에 아직 정의되지
않은 `STD_ON`/`STD_OFF` 등의 심볼을 사용하지 않습니다. `prj_typedefs.h`를
include하기 전에 컴파일 옵션으로 환경을 결정해야 합니다.

### MOBILGENE/AUTOSAR 타깃

타깃 빌드의 include 경로에 `Common`, `FSM_Core`와 타깃 BSW 헤더 경로를
추가하고, 프로젝트의 컴파일러 옵션으로 `PRJ_MOBILGENE_ENABLE=1U`를 지정합니다.
타깃에서 제공하는 `Platform_Types.h`, `Std_Types.h`, `Os.h` 및 대응 구현이 필요합니다.
이 저장소만으로는 CYT4BB용 펌웨어 전체를 링크할 수 없습니다.

### Windows에서 실제 Common 헤더 사용

타깃 기본값을 수정하지 않고 다음 옵션으로 호스트 빌드를 선택합니다.

```text
-DPRJ_MOBILGENE_ENABLE=0U
```

GCC가 PATH에 설정된 PowerShell에서 저장소 루트를 기준으로 실행합니다.
아래 명령은 실제 Common 헤더로 Core 3개 소스의 컴파일 구문을 확인하며,
실행 파일이나 object 파일은 생성하지 않습니다.

```powershell
gcc -std=c99 -Wall -Wextra -Wconversion -Werror -Wundef -pedantic `
    -DPRJ_MOBILGENE_ENABLE=0U -I ./Common -I ./FSM_Core -fsyntax-only `
    ./FSM_Core/fsm.c ./FSM_Core/fsm_queue.c ./FSM_Core/fsm_timer.c
```

## 3. 호스트 회귀 테스트

Windows PowerShell과 GCC를 준비한 후 저장소 루트에서 실행합니다.

```powershell
& ./FSM_Core/tests/run_tests.ps1
```

스크립트는 임시 실행 파일을 생성하여 테스트한 뒤 제거합니다. 검증 내용은 다음과
같습니다.

- 설정 배열 길이와 상태 개수 일치, count의 `uint16` 범위 검사
- 잘못된 설정의 예상 컴파일 실패 및 65535 상한 설정의 컴파일 성공
- callback 실행 순서와 동일 context의 Init/Dispatch/Do 재진입 차단
- 큐 보존, callback 내 이벤트 등록·조회, 인스턴스별 실행 독립성
- 재진입 진단 횟수 유지·포화
- 초기화 설정 검증 실패 시 비활성화 및 정상 설정을 통한 복구

현재 테스트 스크립트는 `tests/support/prj_typedefs.h`를 사용합니다. 따라서
Common 헤더의 설정 변경에 영향을 받지 않고 FSM 동작을 검증할 수 있지만,
실제 Common 헤더 통합을 대신 검증하지는 않습니다. 테스트용 include 경로를
타깃 빌드에 추가하면 안 됩니다.

스크립트는 `fsm.c`, `fsm_queue.c`를 빌드하며 `fsm_timer.c`의 런타임 동작은
검증하지 않습니다. 실제 Common 헤더와 타이머의 호스트 구문 확인에는 2절의
명령을 사용합니다. 호스트 테스트 통과가 MCU 실행 시간, task/ISR 동기화,
타깃 ABI 또는 MISRA-C/Helix QAC 적합성을 보장하지는 않습니다.

## 4. FSM 사용 시 지켜야 할 계약

1. context 전체를 최초 API 호출 전에 zero initialization합니다.
2. 정적 상태/전이 배열과 `FSM_DEFINE_CONFIG()`로 설정을 선언합니다.
3. `Fsm_Init()` 반환값을 확인하고 실패를 처리합니다.
4. 이벤트 등록 실패를 확인하고 소유 task에서 Dispatch/Do를 실행합니다.

초기화 설정 검증 실패 시 FSM은 비활성화되며 config pointer와 큐를 지웁니다.
다시 사용하려면 유효한 설정으로 초기화에 성공해야 합니다. 실행 중 callback에서
동일 context로 Init/Dispatch/Do를 호출하면 재진입으로 거부하며 진행 중인 FSM을
유지하고 진단 횟수만 증가시킵니다. callback에서는 이벤트 등록과 조회를 사용할 수
있습니다. 실행 중 플래그는 task/ISR 간 lock이 아니므로 접근을 직렬화해야 합니다.

상세 전이 규칙, 재초기화 정리 책임 및 예제는 [FSM Core README](FSM_Core/README.md)를
따릅니다.

## 5. 타이머 통합 조건

- 현재 타이머는 `FSM_TimerIdType`에 정의한 고정 ID 방식입니다. 런타임 등록 API는 없습니다.
- manager 초기화 이후 타이머를 시작하고, 외부의 단일 1ms ISR에서 Notification을 호출합니다.
- `FSM_Timer_Init()`은 GPT 하드웨어를 설정하거나 시작하지 않습니다.
- 각 tick에서 모든 ID의 active를 확인하고, 활성 타이머만 counter를 감소시킵니다.
- `PRJ_MOBILGENE_ENABLE=0U`에서 ISR/task를 공유하려면 별도의 플랫폼 보호 구현이 필요합니다.
- 현재 MOBILGENE 보호는 같은 코어의 ISR/task 접근과 해당 ISR의 interrupt 차단을 전제로 합니다.
- 만료를 이벤트로 전달할 때에는 큐 등록 성공까지 애플리케이션 pending 플래그를 유지합니다.
- `Fsm_Init()`의 성공/실패 처리는 중앙 타이머, 애플리케이션 pending, 하드웨어 출력을 자동 정리하지 않습니다.

시간 정밀도, 취소 정책, 배타 보호 및 pending 재시도 예제는
[타이머 상세 문서](FSM_Core/README.md#11-중앙-fsm-timer)를 참고합니다.

## 6. 브랜치

`main`, `prj_mobilgene_ccp`, `prj_test_ccp` 브랜치를 사용합니다.
브랜치 이름만으로 빌드 환경이 자동 전환되지는 않습니다. 선택한 브랜치의
`prj_defs.h`와 실제 빌드 옵션을 확인하여 타깃/호스트 설정을 결정합니다.
