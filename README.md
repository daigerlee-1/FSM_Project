# FSM Project

## 브랜치 운영 규칙

`main`을 모든 공통 코드와 플랫폼 구현의 원본으로 관리합니다. 전용 브랜치는
main의 특정 커밋에서 반대 플랫폼 분기를 제거하여 생성하는 배포 스냅샷입니다.
세 브랜치의 파일이나 커밋 해시는 같지 않으며, `PLATFORM_SOURCE.json`의
`source_commit`이 같은 main 커밋을 가리키는 것을 동기화 기준으로 삼습니다.

| 브랜치 | 유지할 코드 | 플랫폼 선택 |
|---|---|---|
| `main` | MOBILGENE와 PC 구현, 테스트, 추출 도구 모두 | `PRJ_MOBILGENE_ENABLE=1U/0U`로 선택 |
| `prj_mobilgene_ccp` | MOBILGENE 구현과 공통 코드 | 선택 매크로 및 PC 분기 제거 |
| `prj_test_ccp` | PC 구현과 공통 코드, 호스트 테스트 | 선택 매크로 및 MOBILGENE 분기 제거 |

운영 절차:

1. 기능 추가, 공통 버그 수정 및 플랫폼별 수정은 모두 main에서 수행합니다.
2. 변경 내용을 검토하고 가능한 양쪽 환경의 검증을 수행한 뒤 main에 커밋합니다.
3. 깨끗한 main에서 추출·동기화 도구를 실행하여 두 전용 브랜치에 새 커밋을 만듭니다.
4. 전용 브랜치 검증 후 세 브랜치를 push합니다. 작업 종료 시 main으로 돌아옵니다.
5. 전용 브랜치의 긴급 수정은 해당 변경만 main에 반영한 뒤 다시 생성합니다.
   전용 브랜치 전체를 main에 병합하지 않습니다.

Python 3, PowerShell 및 GCC가 필요합니다. 저장소 루트에서 실행합니다.

```powershell
py tools/test_export_platform.py
& ./FSM_Core/tests/run_tests.ps1
& ./Storage/tests/run_tests.ps1
# 위 검증과 main 커밋을 완료하고 작업 트리가 깨끗한 상태에서 실행
py tools/sync_platforms.py
```

동기화 도구는 main에서 두 스냅샷을 먼저 생성한 뒤 전용 브랜치의 추적 파일을
갱신하고 각각 커밋합니다. 기존 커밋 이력을 보존하며 reset/강제 push를 사용하지
않습니다. 로컬 두 브랜치가 있어야 하며, 원격 변경이 있으면 먼저 fetch하고
변경 내용을 main에 반영해야 합니다. 도구는 push하지 않고 마지막에 main으로
돌아옵니다. 충돌이나 오류가 발생하면 상태를 확인하여 해결한 뒤 재실행합니다.
미커밋 변경은 거부하지만, 전용 브랜치에만 있는 커밋의 기능은 자동 병합하지
않으므로 실행 전에 main 반영 여부를 반드시 검토합니다.

단일 플랫폼을 별도 폴더로 검토하려면 아래처럼 추출합니다. 출력 경로는 저장소
밖의 새 디렉터리여야 합니다.

```powershell
py tools/export_platform.py --platform mobilgene --output "$env:TEMP/fsm_mobilgene_snapshot"
py tools/export_platform.py --platform pc --output "$env:TEMP/fsm_pc_snapshot"
```

추출 도구는 플랫폼 조건만 평가하고 차량/기능 선택, include guard 및 OS 선택
조건은 보존합니다. 해석할 수 없는 플랫폼 표현식은 생성 실패로 처리합니다.
MOBILGENE 스냅샷에서는 PC 테스트·RAM NvM 드라이버를 제외하고, PC 스냅샷에서는
AUTOSAR SWC C 파일, 기존 NvM RAM 연계 및 RTE 전용 CAN 어댑터를 제외합니다.
새 플랫폼 전용 파일을 추가하면 `tools/export_platform.py`의 제외 목록도 검토합니다.

현재 검증 범위와 제한:

- PC: FSM 회귀 테스트와 실제 Common 헤더/Core 구문 검사, RAM NvM 드라이버 및
  NvmApp 연동 테스트를 지원합니다. RAM NvM은 실제 비동기 Flash를 모사하지 않습니다.
- PC 전체 실행에는 누락된 task.h/task.c, drv_console과 CAN 서비스 대역 등이 필요합니다.
  LED 대역의 완료 알림도 추가로 구성해야 합니다. 전용 브랜치 생성이 전체 앱 실행
  가능함을 의미하지 않습니다.
- MOBILGENE: 외부 생성 RTE/BSW/OS/MCAL 및 링커 설정이 필요하며 이 저장소만으로
  타깃 전체 링크를 검증할 수 없습니다.
- 각 모듈 WARNING은 기존 검토 이력입니다. 플랫폼 분기 제거는 기능 결함의 일괄
  수정을 의미하지 않습니다. Storage의 standalone 구현 누락(STORAGE-W004)은
  `drv_nvm_pc.c` 추가로 보완했으며 나머지 항목은 별도 조치 대상입니다.


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
컴파일 옵션으로 덮어쓸 수 있습니다. 현재 기본값은 `1U`입니다.

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
