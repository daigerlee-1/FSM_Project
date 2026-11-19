# Power 검토 경고 및 확인 사항

검토일: 2026-11-15  
대상 브랜치: `prj_mobilgene_ccp`  
상태: 미조치 검토 사항 기록. 이 문서 작성에 따른 소스 수정은 없음.

## 1. 검토 범위와 검증 조건

`power_app.c/h`, `SWC_PWR_MGR.c/h`, README와 Common/FSM Core, Comm,
Knob LED의 관련 인터페이스를 검토했습니다. `SWC_PDC.c`는 빈 파일입니다.

실제 Power 애플리케이션, FSM, 큐, 타이머 소스에 임시 RTE/ComM/LED mock을
연결하여 상태 전이와 재시도 경로를 확인했습니다. SWC 내부 절차는 검증용
translation unit에서 소스를 포함하여 호출했습니다. 저장소의 소스는 수정하지
않았습니다. 이 구성은 GCC C99의 `-Wall -Wextra -Wconversion -Werror`를
통과했습니다. 이는 AUTOSAR 생성 코드와의 통합 빌드, MISRA-C/Helix QAC 적합성,
실제 보드의 전기적 동작이나 선점 안전성을 입증하지 않습니다.
임시 검증 코드는 정식 회귀 테스트에 포함되지 않습니다.

## 2. 코드 항목 요약

| ID | 우선순위 | 항목 | 확인 상태 |
|---|---|---|---|
| POWER-W001 | 높음 | NoCom 재시도 실패 후 추가 요청 중단 | 모의 시간과 요청 실패로 재현 |
| POWER-W002 | 높음 | wake 시 대기 중인 NoCom 요청 미취소 | 현재 FullCom / 요청 NoCom 조건에서 확인 |
| POWER-W003 | 높음 | HU 상태 캐시로 필요한 상태 전이 누락 | Reset 및 수면 중 HU OFF 수신 경로 재현 |
| POWER-W004 | 높음 | ERROR 진입 시 LED 차단 처리 누락 | LED 차단 함수 호출 0회 확인 |
| POWER-W005 | 중간 | ADC 읽기 실패 후 이전 샘플 유효성 무기한 유지 | 코드 확인. 실제 ADC 고장 시험 미수행 |
| POWER-W006 | 중간 | LCAN sleep 출력 실패에도 준비 완료 등록 | 반환값 무시 경로 확인 |
| POWER-W007 | 중간·선점 가능 시 | pending HU 데이터의 동시 접근 보호 부족 | 코드 확인. 실제 선점 시험 미수행 |

## 3. POWER-W001: NoCom 재시도 실패 후 요청 중단

위치: [SWC_PWR_MGR.c](SWC_PWR_MGR.c)의 `SwcPwrMgr_RequestLcanNoCom()`.

최초 NoCom 요청 성공 시 `noComRequested`를 TRUE로 설정하고 200ms 타이머를
시작합니다. 재시도 시 `FSM_Timer_IsExpired()`가 만료를 소비한 뒤
`LcanCom_RequestMode()`가 실패하면, 플래그는 TRUE로 남고 타이머는 재시작되지
않습니다. 따라서 통신 모드가 계속 FullCom/SilentCom이면 다음 요청 조건을
만족하지 못합니다. 실패 분기의 '다음 10ms 호출에서 재시도' 주석과 다릅니다.

재현: 최초 요청 성공 → 200ms 경과 → 재요청 실패 → 서비스 정상 복구 및
추가 1초 경과 후 호출. 총 요청 횟수는 2회이며 세 번째 요청은 발생하지 않았습니다.

권장 조치 및 완료 기준:

- 실패 시 재시도 타이머를 시작하거나 요청 플래그를 해제합니다.
- 최초 실패, 재시도 실패, 연속 실패 후 복구에서 재요청이 계속되는지 확인합니다.
- 재요청 주기와 요청 폭주 방지 정책을 명시합니다.

## 4. POWER-W002: wake 시 대기 중인 NoCom 요청 미취소

위치: [SWC_PWR_MGR.c](SWC_PWR_MGR.c)의 `SwcPwrMgr_RestoreLcanFullCom()`.

현재 모드가 FullCom이면 요청 모드를 확인하지 않고 반환합니다. 비동기 ComM에서
현재 모드가 FullCom이어도 이전 NoCom 요청이 아직 처리 중일 수 있습니다.
이때 wake가 발생하면 FullCom 재요청 없이 wake latch가 해제될 수 있고,
이전 NoCom 요청이 뒤늦게 적용될 수 있습니다.

mock을 현재 FullCom / 요청 NoCom으로 설정했을 때 FullCom 요청 횟수는 0회였습니다.
실제 NoCom 적용 시점과 wake 영향은 ComM 통합 환경에서 검증해야 합니다.

권장 조치 및 완료 기준:

- 현재 모드와 요청 모드를 함께 확인하고, 요청 모드가 NoCom이면 FullCom으로 갱신합니다.
- wake latch 해제 조건에 대기 중인 sleep 요청의 취소 확인을 반영합니다.
- NoCom 요청 직후 CAN wake가 발생하는 순서를 통합 시험합니다.

## 5. POWER-W003: HU 상태 캐시로 필요한 전이 누락

위치: [power_app.c](power_app.c)의 `PowerApp_AcceptHuPowerState()`,
`PowerApp_ActionReset()` 및 상태 진입 처리.

HU 상태는 이벤트 큐 등록 성공 시 캐시에 반영됩니다. 이벤트가 현재 상태에서
실제 전이를 일으켰는지는 확인하지 않습니다. 또한 ERROR → RESET 경로에서
`huPowerStateValid`와 이전 HU 상태가 초기화되지 않습니다. 같은 값을 다시
수신하면 이벤트를 생성하지 않아 필요한 전이가 누락됩니다.

재현한 경로:

1. HU ON으로 POWER_ON 진입 → FAULT로 ERROR 진입 → RESET → INIT → SLEEP.
   같은 HU ON을 다시 전달해도 SLEEP에 머뭅니다.
2. SLEEP에서 HU OFF 전달 및 이벤트 소비 → CAN RX로 STANDBY 진입.
   같은 HU OFF를 다시 전달해도 STANDBY에 머뭅니다.

권장 조치 및 완료 기준:

- Reset 시 HU 캐시의 유효성을 초기화합니다.
- 수신 값의 변경 여부뿐 아니라 현재 FSM 상태에서 요청 처리가 필요한지도 판단합니다.
- 단순히 매 수신마다 이벤트를 생성하여 큐를 포화시키지 않도록 pending/중복 억제 정책을 설계합니다.
- 위 두 경로와 큐 포화 후 재시도를 검증합니다.

## 6. POWER-W004: ERROR 진입 시 LED 차단 누락

위치: [power_app.c](power_app.c)의 `PowerApp_EntryError()`.

`LedApp_RequestFault()`가 주석 처리되어 있으며 `LedApp_DisableOutput()`도
호출하지 않습니다. POWER_ON에서 FAULT를 처리하여 ERROR에 진입할 때
mock LED 차단 함수의 추가 호출 횟수는 0회였습니다.

MPQ4323 enable 요청은 별도로 OFF가 되지만, 그것이 실제 LED 전원을 차단하는지는
회로 확인이 필요합니다. 현재 코드만으로 LED enable 신호의 안전 상태를 보장할 수
없습니다. Knob의 Fault 이벤트 처리 제한도 [Knob 경고](../Knob/WARNING.md)의
KNOB-W002와 함께 검토해야 합니다.

권장 조치 및 완료 기준:

- ERROR 진입 시 즉시 LED 출력을 차단하고, 필요한 경우 Fault 이벤트를 별도 전달합니다.
- LED 설정/통신 중에도 차단되는지 확인합니다.
- Reset 후 LED 복구 정책과 실제 GPIO 및 전원 레일 상태를 검증합니다.

## 7. POWER-W005: ADC 샘플 유효성 무기한 유지

위치: [SWC_PWR_MGR.c](SWC_PWR_MGR.c)의 `RE_PWR_MGR_5ms_Task()`와
[power_app.c](power_app.c)의 ACC/배터리 판정.

읽기가 한 번 성공하면 샘플 valid가 TRUE가 됩니다. 이후 읽기가 계속 실패해도
이전 값과 유효성이 유지됩니다. 과거 샘플만으로 100ms 확인이 완료되거나 배터리
동작 허용이 유지될 수 있습니다. 타이머 경과는 정상 샘플의 연속 수신을 의미하지
않습니다.

권장 조치 및 완료 기준:

- 샘플 최대 유효 시간 또는 허용 연속 실패 횟수를 정의합니다.
- 유효 기간 초과 시 확인 타이머 취소, 출력/CAN 허용 정책 및 진단 처리를 정합니다.
- 첫 샘플 전, 확인 중 읽기 실패, 정상 동작 중 장기 실패 및 복구를 시험합니다.

## 8. POWER-W006: sleep 출력 실패에도 준비 완료 등록

위치: [SWC_PWR_MGR.c](SWC_PWR_MGR.c)의 `SwcPwrMgr_ApplyLcanSleepOutputs()`와
`SwcPwrMgr_CompleteLcanSleep()`.

LCAN EN/STB 출력 호출의 반환값을 무시하고 `SLEEP_READY` 이벤트를 등록합니다.
한쪽 출력 호출이 실패해도 FSM이 SLEEP_IN으로 진입할 수 있습니다. SLEEP_IN에서도
출력 적용을 반복하지만, 상태 진입 시점에 출력 성공이 확인된 것은 아닙니다.

권장 조치 및 완료 기준:

- 출력 적용 함수가 성공 여부를 반환하도록 하고 필요한 출력이 성공한 뒤 완료를 등록합니다.
- 부분 성공 시 안전한 출력 순서와 재시도 정책을 회로 조건에 맞춰 정의합니다.
- EN 실패, STB 실패 및 복구 시 준비 완료 상태와 진단을 검증합니다.

## 9. POWER-W007: pending HU 데이터 동시 접근

위치: [SWC_PWR_MGR.c](SWC_PWR_MGR.c)의
`SWC_PWR_MGR_NotifyHuPowerState()`와 `SwcPwrMgr_ProcessPendingHuPowerState()`.

CAN 수신 처리와 10ms runnable이 선점 가능하면 pending 값과 플래그의 읽기/처리가
하나의 원자적 절차가 아닙니다. 예를 들어 runnable이 이전 값을 복사한 뒤 CAN
처리가 새 값을 저장하고 pending을 TRUE로 설정해도, 이전 값 처리 완료 시
runnable이 pending을 FALSE로 지울 수 있습니다. 새 값의 직접 큐 등록까지
실패한 경우 재시도할 알림을 잃습니다. volatile은 이 절차의 배타성을 보장하지
않습니다. 동일 FSM 큐에 대한 직접 접근 문맥도 함께 확인해야 합니다.

이 항목은 CAN 처리와 runnable이 같은 문맥에서 직렬 실행된다는 보장이 있으면
해당 조건에서 발생하지 않습니다. 현재 검토에서는 실제 OS 매핑을 확인하지 못했습니다.

권장 조치 및 완료 기준:

- CAN 알림과 5ms/10ms runnable의 task, ISR, 코어 및 선점 관계를 명시합니다.
- 직렬 실행을 보장하거나 보호된 mailbox와 단일 FSM 소비 문맥을 적용합니다.
- 이전 값 복사와 pending 해제 사이의 새 수신, 큐 포화, 연속 수신을 시험합니다.

## 10. README 보완 사항

코드 수정 사항과 별도로 [README.md](README.md)를 다음과 같이 보완해야 합니다.

| ID | 항목 | 필요한 보완 |
|---|---|---|
| POWER-D001 | Fault 시 LED fail-safe 설명 | 현재 LED Fault 호출은 주석 상태이며 즉시 차단도 없음. 코드 조치 후 실제 동작에 맞춰 설명 |
| POWER-D002 | ADC 100ms 연속 조건 | 샘플 유효 시간, 정상 수신 조건, 읽기 실패 시 판정 정책 명시 |
| POWER-D003 | 실행 및 동시 접근 계약 | CAN 알림과 5ms/10ms runnable의 직렬 실행 또는 보호 조건 명시 |
| POWER-D004 | standalone 실행 구성 | 설명에 등장하는 `task.c`는 현재 저장소에서 확인되지 않음. 외부 제공 파일이면 위치와 의존성을 명시하고, 누락이면 실행 구성 보완 |

문서만 변경하여 코드 결함을 조치 완료로 표시해서는 안 됩니다. 각 코드 항목은
구현 수정 및 해당 경로 검증 후 상태를 갱신해야 합니다.
