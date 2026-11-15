# Comm 검토 경고 및 통합 확인 사항

검토일: 2026-11-15  
대상 브랜치: `prj_mobilgene_ccp`  
상태: 검토 사항 기록. 이 문서 작성에 따른 통신 소스 수정은 없음.

## 1. 범위와 검증 조건

Comm 폴더의 LCAN 입력/출력, 입력 RTE 어댑터, 버전 응답, ComM 인터페이스,
SWC 어댑터, I-PDU callout, DCM 코드와 README를 검토했습니다.

검증에는 실제 Common 헤더와 임시 Power/LED/Button/Wheel/RTE/DCM mock을
사용했습니다. mock은 미제공 API의 최소 호출 계약과 타입을 가정하므로 실제
생성 헤더의 ABI, 보고값 encoding, 서비스 동작, task 배치 또는 버스 전송을
검증한 것은 아닙니다. 검증 코드는 정식 회귀 테스트에 포함되어 있지 않습니다.

## 2. 항목 요약

| ID | 우선순위 | 항목 | 확인 상태 |
|---|---|---|---|
| COMM-W001 | 높음 | ComM 서비스 구현이 저장소에 없음 | 선언과 미해결 참조 확인 |
| COMM-W002 | 높음·서비스 연결 시 | DTC 조회/삭제 callback의 실제 처리 미구현 | 코드 및 완료 통지 mock 확인 |
| COMM-W003 | 높음·알림 거부 가능 시 | Power 알림 반환값 무시 및 재시도 없음 | 실패 반환 mock으로 재현 |
| COMM-W004 | 중간·LED 설정 초기화 시 | LED 재초기화 후 밝기 캐시 불일치 | 외부 LED reset 모델로 재현 |
| COMM-W005 | 중간 | 엄격한 GCC 경고 설정에서 컴파일 실패 | 정수 변환 및 미사용 인자 경고 확인 |
| COMM-W006 | 낮음 | README의 trace 옵션·30ms 처리 설명과 구현 차이 | 코드와 trace symbol 확인 |
| COMM-W007 | 중간·선점 실행 시 | 공유 snapshot과 lazy initialization의 직렬화 계약 부족 | 코드 확인. OS/RTE 설정 미확인 |

## 3. COMM-W001: ComM 구현 누락

위치: `lcan_comm.h`, `SWC_LCAN.c`의 `LCAN_IsTxAllowed()`.

`LcanCom_GetCurrentMode()`, `LcanCom_GetRequestedMode()`,
`LcanCom_RequestMode()`의 선언은 있지만 저장소에서 정의를 찾지 못했습니다.
README에 기재된 `lcan_comm.c`도 없습니다. 헤더 설명은 SWC 어댑터가 구현한다고
되어 있으나 현재 `SWC_LCAN.c`에도 구현이 없습니다.

mock 헤더로 컴파일한 SWC object에서 `LcanCom_GetCurrentMode` 미해결 참조를
확인했습니다. 이 object를 포함하여 링크할 때 외부 제공 구현이 없다면 실패합니다.

권장 조치:

- 실제 ComM/RTE 서비스에 연결하는 구현 파일을 추가하거나 외부 제공 경로를 명시합니다.
- mode 변환, NULL 출력 pointer, 조회 실패 및 요청 실패 처리를 검증합니다.
- README의 파일 목록과 헤더의 구현 소유자 설명을 일치시킵니다.
- 실제 생성 헤더와 전체 링크에서 세 API의 제공 여부를 확인합니다.

## 4. COMM-W002: DTC callback의 실제 처리 미구현

위치: `SWC_DCM.c`의 `Dcm_ReadDTCInformation()`과
`Dcm_ClearDiagnosticInformation()`.

두 함수 모두 `OpStatus`를 사용하지 않고 `Dcm_ExternalProcessingDone()`을 호출한
뒤 성공값을 반환합니다. 활성 코드에는 DTC 조회/삭제, 요청 해석, 응답 데이터 구성,
처리 실패 또는 진행 상태 분기가 없습니다. 기존 DTC 관리 코드는 `#if 0`으로 제외됩니다.

DCM 완료 통지를 관찰하는 mock에서 두 함수가 응답 데이터와 길이를 변경하지 않고
성공 및 완료를 보고함을 확인했습니다. 실제 DCM stack의 응답 구성 범위와 서비스
연결 설정은 미제공 상태이므로 정확한 버스 응답 형태는 확인하지 않았습니다.

권장 조치:

- 해당 함수가 실제 진단 서비스에 연결되는지 먼저 확인합니다.
- 미지원 placeholder라면 서비스 설정에서 제외하거나 stack 계약에 맞는 미지원 처리를 합니다.
- 지원 서비스라면 실제 DTC 저장소/DEM 연동, 응답 구성, 실패 및 OpStatus 처리를 구현합니다.
- 조회 데이터와 실제 삭제 결과를 포함한 end-to-end 진단 테스트를 수행합니다.

## 5. COMM-W003: Power 알림 거부 시 유실

위치: `lcan_input.c`의 `LcanInput_UpdateHuCarInfo/EUsm/MoodInfo/State/Sys/SysInfo()`.

`SWC_PWR_MGR_NotifyCanRx()`와 `SWC_PWR_MGR_NotifyHuPowerState()`의 반환값을
무시합니다. 10ms cyclic은 밝기 요청만 재시도하며 Power 알림을 재전달하지 않습니다.

알림 API가 큐 포화 또는 미초기화로 STD_NOT_OK를 반환한다고 가정한 mock에서,
HU State를 한 번 갱신한 뒤 100회 cyclic을 실행해도 알림 시도가 한 번뿐임을
확인했습니다. 실제 Power 헤더/구현은 없어 그 API의 반환 계약과 거부 조건은
추가 확인이 필요합니다.

영향: 거부 가능한 API라면 입력 snapshot에는 최신 상태가 저장되어도 Power FSM은
해당 전이나 wake/sleep 관련 알림을 받지 못할 수 있습니다.

권장 조치:

- Power 알림 API의 실제 반환 계약과 실행 문맥을 확인합니다.
- 거부 가능 시 pending과 재시도 또는 거부 진단/복구 정책을 구현합니다.
- 최신 상태로 병합 가능한 알림과 모든 순서를 보존해야 하는 이벤트를 구분합니다.
- 초기화 전 수신과 큐 포화에서 알림 유실 여부를 검증합니다.

## 6. COMM-W004: 외부 LED reset 이후 밝기 캐시 불일치

위치: `lcan_input.c`의 `LcanInput_UpdateBrightnessRequest()`와
`LcanInput_ProcessBrightnessRequest()`.

`lastAcceptedStepValid`와 `lastAcceptedStep`은 LED에 수락된 값을 계속 보관합니다.
새 입력이 이 값과 같으면 밝기 설정을 다시 요청하지 않습니다. LED 모듈이 별도로
초기화되어 밝기 설정을 잃어도 이 캐시를 무효화하는 경로는 현재 Comm에 없습니다.

다음 reset 모델에서 재현했습니다.

1. HU 밝기 50 요청이 수락되고 출력이 활성화됨.
2. 외부 LED reset으로 실제 밝기가 기본값 2, 출력 비활성 상태가 됨.
3. 동일한 HU 밝기 50을 다시 수신함.
4. 설정 요청은 생략되고 기본값 2 상태에서 출력이 다시 활성화됨.

실제 LED가 재초기화 시 설정을 유지하는지 또는 상위 모듈이 재설정을 보장하는지는
LED 구현이 없어 미확인입니다.

권장 조치:

- LED 설정을 잃는 reset/재시작 경로에서 수락 캐시를 무효화합니다.
- 또는 LED의 적용 상태/설정 세대를 조회하여 재설정 필요 여부를 판단합니다.
- 출력 재활성화 전에 최신 밝기가 실제 적용되었는지 확인하는 계약을 정합니다.

## 7. COMM-W005: 엄격 경고 옵션에서 컴파일 실패

### Wheel encoding의 정수 변환

위치: `lcan_output.c:653`, `LCAN_WheelReport_EncodeCanValue()`.

```c
encodedValue = LCAN_WHEEL_ENCODER_CCW_BASE + (uint16)count;
```

호스트의 integer promotion으로 덧셈 결과가 int가 되어 uint16 대입에서
`-Wconversion`이 발생합니다. `-Werror`를 함께 사용한 빌드는 실패했습니다.
현재 유효 count와 offset 범위에서 산술 overflow가 재현된 것은 아닙니다.

범위를 확인한 뒤 명시적인 결과 변환을 적용하거나, unsigned 연산 폭을 일관되게
맞추는 것을 권장합니다. 수정 후 타깃 컴파일러와 QAC의 essential type 판정도 확인합니다.

### DCM 인자 미사용

`SWC_DCM.c`의 두 함수는 `OpStatus` 미사용 경고를 발생시킵니다.
서비스 구현에 맞게 사용하거나 의도적 미사용임을 명시해야 합니다.

런타임 mock 검증은 위 경고만 선택적으로 오류 승격에서 제외하여 진행했습니다.
따라서 전체 Comm이 엄격한 경고 설정으로 통과했다고 판단하면 안 됩니다.

## 8. COMM-W006: 문서와 trace/유지 시간 구현의 차이

### Trace 비활성화 옵션

README는 `LCAN_OUTPUT_TRACE_ENABLE=0U`이면 trace RAM을 제거할 수 있다고
설명하지만, 코드에는 이 매크로의 조건부 컴파일이 없습니다. 휠/버튼 각 512개의
volatile 로그 배열과 저장 코드가 항상 포함됩니다.

해당 매크로를 0U로 지정한 실행 파일에서도 `gLcanWheelReportLog`와
`gLcanButtonReportLog` symbol을 확인했습니다. 로그를 선택적으로 제거하려면
배열뿐 아니라 모든 로그 접근도 함께 조건부 컴파일해야 합니다.

### Wheel 30ms 유지 책임

README는 Move Info를 30ms 유지한 뒤 Invalid로 변경한다고 설명하지만,
`LcanOutput_MainFunction10ms()`는 방향키 release guard만 갱신합니다.
단일 이동 report 이후 100회 cyclic을 실행해도 encoder 이동값이 유지됨을 확인했습니다.

WheelApp이 Invalid report를 보내는 설계일 수 있으므로 곧바로 결함으로 단정하지
않습니다. 실제 upstream의 유지 시간·해제 report 계약을 확인하고 README에서
책임 모듈을 구분해야 합니다. 해제 report의 누락/실패 시 처리도 정의해야 합니다.

## 9. COMM-W007: 공유 상태와 초기화의 실행 계약

위치: `lcan_input_rte.c`의 정적 수신 snapshot, `lcan_input.c`의 context와
밝기 상태, `lcan_output.c`의 출력/중재 상태, `SWC_LCAN.c`의 lazy initialization.

수신 event, timeout event, Button/Wheel event와 10ms runnable이 같은 상태를
읽거나 갱신합니다. 현재 자체 배타 보호나 일관된 snapshot 복사 인터페이스는 없습니다.
조회 API의 const pointer는 호출자의 쓰기를 제한할 뿐 내부 변경과의 동기화를 보장하지 않습니다.

별도 선점 task 또는 ISR에서 겹쳐 실행하면 값/validity가 다른 시점에서 섞이거나,
출력 중재 변경과 RTE 기록이 겹칠 수 있습니다. 최초 두 호출이 lazy initialization에
동시에 진입하면 처리한 report를 뒤늦은 초기화가 지울 가능성도 있습니다.
실제 runnable의 task 매핑·우선순위·재진입 설정이 없어 발생 여부는 미확인입니다.

권장 조치:

- 가능한 경우 모든 갱신과 처리를 소유 task에 직렬 배치합니다.
- 최초 초기화는 event를 허용하기 전에 명시적으로 완료합니다.
- 선점 공유가 필요하면 생성 RTE의 일관성 수단 또는 적절한 exclusive area를 적용합니다.
- 보호 구간에서 외부 API를 호출할 때의 실행 시간과 중첩 호출도 함께 검토합니다.
- volatile만 추가하여 해결된 것으로 간주하지 않습니다.

## 10. 통과한 검증과 추가 통합 확인

| 검증 | 결과 |
|---|---|
| 선택한 동력원 ready 판정과 CarInfo timeout | timeout 후 FALSE 확인 |
| HU State timeout | Power/자동 밝기 validity 무효화 확인 |
| 출력 초기값 | encoder Invalid 및 방향키 해제 상태 확인 |
| 방향키 release guard | Clear 이후 3회 cyclic 후 소유권 해제 확인 |
| 버전 반복 | 정상 mock에서 호출 1/21/41에 응답 pair 기록 확인 |
| 버전 재요청·쓰기 실패 | 요청 비활성화 후 재접수 및 실패 후 재시도 확인 |
| SWC/입력 RTE/I-PDU 어댑터 | 임시 인터페이스에 대한 호스트 컴파일 구문 확인 |

다음 항목은 실제 통합 환경에서 확인해야 합니다.

- RTE Write 성공은 실제 CAN 전송 완료를 뜻하지 않습니다. 버전 함수의 3회 계수는
  RTE 기록 성공 기준입니다. CAN trace와 생성 COM/RTE 설정으로 실제 frame 횟수를 확인합니다.
- 버전 Major/Minor의 clear와 실제값은 네 번의 독립 Write입니다. OnChange 처리 시점,
  중간값 노출 및 signal group 적용 여부를 확인합니다.
- TX 금지 중에는 버전 처리 함수가 호출되지 않습니다. 그 사이 요청이 1에서 0으로
  바뀌면 최신 snapshot만 남으므로 요구사항상 요청 보존이 필요한지 확인합니다.
- 일반 출력의 RTE Write 반환값은 대부분 무시합니다. 중요한 변경의 실패 진단과
  재시도 정책을 확인합니다. 주기 재기록만으로 짧은 이벤트의 전달을 보장하지는 않습니다.
- I-PDU callout의 Power 출력 허용 조건과 SWC의 Operational/FullCom 조건이 실제
  전송 경로에서 의도한 정책을 함께 만족하는지 확인합니다.
- 실제 CAN DB 값/비트폭, 생성 헤더 ABI, ComM 구현, DCM/DEM 연결, OS/RTE 설정,
  타깃 컴파일·링크, MISRA-C/Helix QAC 및 버스 검증이 필요합니다.
