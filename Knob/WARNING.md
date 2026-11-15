# Knob 검토 경고 및 확인 사항

검토일: 2026-11-15  
대상 브랜치: `prj_mobilgene_ccp`  
상태: 미조치 검토 사항 기록. 이 문서 작성에 따른 소스 수정은 없음.

## 1. 검토 범위와 검증 조건

Button, Wheel, Led의 애플리케이션 FSM, 드라이버, SWC 어댑터와 README를
검토했습니다. 기존 Common/FSM Core 및 Comm의 보고 처리 계약도 대조했습니다.

버튼·휠 검증은 실제 Common/Core와 standalone 입력 드라이버로 수행했습니다.
LED 검증은 실제 FSM·통신·설정·OTP 소스와 임시 GPIO/I2C mock으로 수행했습니다.
정상 초기화에 필요한 수신 frame은 현재 코드의 CRC 계산을 이용해 생성했습니다.
따라서 이 검증은 실제 E52248 프로토콜 규격이나 하드웨어 통신의 적합성을
독립적으로 입증하지 않습니다. 임시 검증 코드는 정식 회귀 테스트에 포함되지 않습니다.

테스트 종료 후 삭제 예정인 임시 테스트 설정의 기본값은 이번 수정 대상으로
분류하지 않았습니다.

## 2. 항목 요약

| ID | 우선순위 | 항목 | 확인 상태 |
|---|---|---|---|
| KNOB-W001 | 높음·ISR/task 공유 시 | 휠 입력 큐의 critical section이 빈 동작 | 코드 확인. 실제 선점 시험 미수행 |
| KNOB-W002 | 높음 | LED 설정 중 Fault 요청 무시 | 정상 초기화 후 SET_BRIGHT에서 재현 |
| KNOB-W003 | 높음·완료 알림 누락 시 | I2C 시작 실패 전달 및 통신 timeout 부족 | 5초 모의 시간 후 BUSY 유지 확인 |
| KNOB-W004 | 중간 | 30ms 이후 휠 edge를 정상 회전으로 수락 | 31ms 간격으로 재현 |
| KNOB-W005 | 중간 | 버튼 보고 큐 포화로 최종 CLEAR 유실 | 큐 회복 후에도 해제 보고 미생성 확인 |
| KNOB-W006 | 중간 | 휠 이동 보고의 30ms 유지 시작점 불일치 | 지연 확인 직후 Invalid 준비 재현 |
| KNOB-W007 | 중간 | 버튼 ID 축소 변환 후 범위 검사 | ID 256이 APPS로 수락됨을 확인 |
| KNOB-W008 | 중간 | E52248 수신 변환 코드의 엄격 경고 빌드 실패 | GCC -Wconversion/-Werror에서 재현 |
| KNOB-W009 | 낮음 | 새 FSM 설정에 컴파일 시 크기 검사 미적용 | 직접 초기화 방식 사용 확인 |

## 3. KNOB-W001: 휠 ISR 입력 큐 배타 보호

위치: [Wheel/wheel_app.c](Wheel/wheel_app.c)의 critical section 매크로와
`KnobWheel_InputQueuePush/Pop/Init()`.

`KNOB_WHEEL_ENTER_CRITICAL()`과 `KNOB_WHEEL_EXIT_CRITICAL()`은 빌드 환경과
관계없이 빈 동작입니다. ISR 생산자와 task 소비자가 공유하는 count의 증가/감소가
겹치면 read-modify-write 갱신을 잃을 수 있습니다. ISR 간 선점이 허용되는 경우에는
생산자 사이의 충돌도 검토해야 합니다. volatile은 배타성을 보장하지 않습니다.
Wheel README의 target critical section 보호 설명과 구현이 다릅니다.

권장 조치 및 완료 기준:

- 실제 ISR 우선순위, task 실행 코어와 큐 접근 문맥을 확인합니다.
- 같은 코어에서 해당 ISR을 차단하는 보호 또는 검증된 큐 알고리즘을 적용합니다.
- interrupt 상태 복원과 중첩 호출 조건을 보장합니다. 다른 코어의 접근은 별도 보호가 필요합니다.
- Push/Pop 선점, wrap, full/empty 경계와 재초기화 중 접근을 검증합니다.

## 4. KNOB-W002: LED 설정 상태에서 Fault 요청 소비

위치: [Led/knob_led.c](Led/knob_led.c)의 `LedApp_Transitions[]`와
`LedApp_RequestFault()`.

`LED_EVENT_FAULT` 전이가 NORMAL에만 정의되어 있습니다. 다른 설정 상태에서
등록에 성공한 Fault 이벤트는 일치하는 전이가 없어 소비될 수 있습니다.

재현 순서:

1. mock I2C로 초기화를 완료하고 NORMAL 진입.
2. 출력을 활성화한 뒤 밝기 변경을 요청하여 SET_BRIGHT 진입.
3. `LedApp_RequestFault()`가 STD_OK를 반환하는 것을 확인.
4. LED main 실행 후에도 SET_BRIGHT와 출력 활성 상태가 유지됨을 확인.

STD_OK는 큐 수용 성공을 의미하므로 그 자체가 Fault 처리 완료를 보장하지 않습니다.
안전 차단 요청으로 사용하는 상위 기능은 이 차이를 고려해야 합니다.

권장 조치 및 완료 기준:

- Fault를 수용해야 하는 모든 운영 상태를 정의하고 전이 또는 즉시 출력 차단을 구현합니다.
- 진행 중 I2C/OTP 작업과 뒤늦은 완료 알림을 어떻게 정리할지 결정합니다.
- 초기화·설정·진단 중 Fault를 주입하고 출력 차단과 복구 조건을 확인합니다.

## 5. KNOB-W003: I2C 실패 전달과 통신 timeout

위치: [Led/knob_led_drv.c](Led/knob_led_drv.c)의 I2C Read/Write wrapper와
[Led/e52248_comm.c](Led/e52248_comm.c)의 대기 상태 처리.

타깃 wrapper가 RTE Read/Write 시작 반환값을 무시하며 상위에 결과를 전달하지
않습니다. 통신 FSM에는 command/data/write 완료 알림을 기다리는 timeout이 없습니다.
시작 거부나 callback 누락이 발생하면 BUSY에서 빠져나오지 못할 수 있습니다.

mock에서 Write를 수락한 뒤 완료·오류 알림을 주지 않고 5초의 모의 tick과
1,000회 통신 cyclic을 진행했으나 BUSY/PENDING이 유지되고 결과가 반환되지 않았습니다.
이는 실제 I2C 버스 장애 시험이 아닌 알림 누락 경로 검증입니다.

권장 조치 및 완료 기준:

- 실제 RTE 서비스의 반환 계약을 확인하고 시작 실패를 통신 FSM으로 전달합니다.
- 하위 드라이버가 제한 시간 내 오류 callback을 반드시 보장하는지 확인합니다.
- 해당 보장이 없으면 각 대기 단계에 timeout, 취소·복구 및 오류 결과를 추가합니다.
- timeout 이후 도착한 오래된 callback이 다음 작업의 완료로 해석되지 않도록 합니다.
- 시작 거부, 무응답, callback 누락과 지연 callback을 주입해 검증합니다.

## 6. KNOB-W004: 휠 edge timeout보다 입력 처리가 우선

위치: [Wheel/wheel_app.c](Wheel/wheel_app.c)의 `WheelApp_MainFunction10ms()`.

입력 큐를 처리한 뒤 timer 만료를 확인합니다. 두 번째 edge가 먼저 회전으로
처리되면 action에서 타이머를 중지하고, 이미 latch된 만료도 지워집니다.
첫 edge를 처리한 뒤 31ms 후 유효 패턴의 두 번째 edge를 넣었을 때 정상 회전이
확정되고 waitTimeoutCount가 증가하지 않는 것을 확인했습니다.

권장 조치 및 완료 기준:

- 30ms 조건이 실제 센서 edge 간격인지 task 처리 간격인지 명확히 합니다.
- 실제 edge 간격이면 ISR snapshot에 timestamp를 포함하여 판정합니다.
- 만료 검사 순서만 바꾸면 일찍 도착했지만 처리가 늦어진 edge를 잘못 거부할 수 있습니다.
- 경계 전/후 edge와 task 지연·입력 backlog를 조합해 검증합니다.

## 7. KNOB-W005: 버튼 큐 포화 후 최종 상태 복구 부족

위치: [Button/button_app.c](Button/button_app.c)의 보고 생성과
`ButtonApp_ReportQueuePush()`.

보고 저장 실패 시 overflow counter는 증가하지만 물리 상태와 long-key 상태는
그대로 진행됩니다. 떨어진 CLEAR를 별도로 보존하거나 이후 재생성하지 않습니다.

재현에서는 보고 소비를 중단하고 짧은 누름 6회, 긴 누름 1회, 짧은 누름 1회를
처리했습니다. 큐의 32개 항목을 소비한 뒤 마지막 신호가 SHORT_KEY로 남았으며,
CLEAR 저장 실패 counter가 1 증가했습니다. 해제 상태에서 cyclic을 계속 실행해도
CLEAR는 재생성되지 않았습니다.

권장 조치 및 완료 기준:

- 큐 포화 이후 최종 물리 상태를 다시 동기화하거나 해제 보고를 보존합니다.
- PRESSED/INVALID와 SHORT/CLEAR 같은 연속 보고의 부분 저장 정책을 정의합니다.
- 이벤트 순서 보존과 최신 상태 병합 중 어떤 계약을 사용할지 결정합니다.
- 장시간 RTE 실패 후 복구 시 수신 측 최종 버튼 상태가 해제로 수렴하는지 검증합니다.

## 8. KNOB-W006: 휠 보고 유지 타이머의 시작점

위치: [Wheel/wheel_app.c](Wheel/wheel_app.c)의 `KnobWheel_MainFunction()`과
`WheelApp_ConfirmReport()`.

이동 보고를 pending으로 준비할 때 30ms 타이머를 시작합니다. RTE가 성공했다고
확인하는 시점에는 타이머를 시작하지 않습니다. README는 RTE 전달 후 30ms 유지로
설명하므로 전달 실패가 길어지면 문서의 유지 시간을 만족하지 못합니다.

이동 보고를 pending 상태로 100ms 지연시킨 뒤 Confirm하고 즉시 다음 main을
실행했을 때 Invalid가 준비되는 것을 확인했습니다.

권장 조치 및 완료 기준:

- 유지 시간의 기준을 보고 준비, RTE 수용 또는 실제 CAN 전송 중 하나로 확정합니다.
- RTE 수용 기준이면 이동 보고 Confirm 시점에 타이머를 시작하도록 상태를 분리합니다.
- Invalid 보고 확인에는 이동 유지 타이머를 재시작하지 않도록 구분합니다.
- RTE 실패/회복과 연속 이동 보고에서 최소 유지 시간 및 순서를 검증합니다.

## 9. KNOB-W007: 버튼 ID 축소 변환 후 검사

위치: [Button/drv_button.c](Button/drv_button.c)의 `DrvButton_ReadById/SetInput()`과
[Button/button_app.c](Button/button_app.c)의 `ButtonApp_GetContextById()`.

범위 검사 전에 ID를 uint8로 변환합니다. 호스트에서 `(Button_IdType)256`이
0으로 변환되어 APPS 입력으로 수락되고, context 조회도 APPS를 반환함을 확인했습니다.

권장 조치 및 완료 기준:

- 원래 enum 값의 하한·상한을 검사한 뒤 배열 인덱스 타입으로 변환합니다.
- 필요하면 switch로 지원 ID를 열거하여 검증합니다.
- 음수, BUTTON_ID_MAX, 255, 256 및 정상 ID를 시험하여 잘못된 요청이 기존 입력을
  변경하지 않고 조회도 실패하는지 확인합니다.

## 10. KNOB-W008: E52248 변환 경고로 컴파일 실패

위치: [Led/e52248_comm.c](Led/e52248_comm.c)의 `Comm_CheckCrc()`와
`Convert_4WordsWithCrcRx()`.

수신 데이터를 조립하는 shift와 복합 OR 대입에서 integer promotion에 따른
int→uint16 변환 경고가 발생합니다. GCC C99의 `-Wconversion -Werror` 빌드가
실패했습니다. LED 런타임 검증은 이 경고만 오류 승격에서 제외하여 수행했습니다.
따라서 엄격 경고 설정으로 전체 Knob 빌드가 통과한 것은 아닙니다.

연산 폭과 변환 위치를 명시하고, 경계 데이터·타깃 컴파일러·QAC 분석으로 확인하는
것을 권장합니다. 경고가 곧 실제 overflow 재현을 뜻하는 것은 아닙니다.

## 11. KNOB-W009: FSM 설정 컴파일 검사 적용 누락

Button, Wheel, LED application 및 E52248 communication 설정은 모두
`Fsm_ConfigType`을 직접 초기화합니다. 추가된 `FSM_DEFINE_CONFIG()`의 count
범위·상태 배열 길이 검사는 적용되지 않습니다. 현재 배열에서 크기 오류를 발견한
것은 아니지만, 향후 변경 시 사용할 방어가 빠져 있습니다.

설정을 새 매크로 방식으로 선언하고 실제 배열 길이를 추론하도록 구성하며,
잘못된 count가 컴파일에서 거부되는지 검증하는 것을 권장합니다.

## 12. 검증 한계와 타깃 확인

- 버튼·휠 로직 검증은 엄격한 GCC C99 경고 옵션으로 빌드했습니다.
- LED mock은 정상 초기화 후 NORMAL 진입과 Fault/통신 정지 경로를 확인했습니다.
- ISR 선점·멀티코어 충돌은 실제 하드웨어에서 재현하지 않았습니다.
- 실제 생성 RTE/OS 설정과 GPIO/I2C 반환 계약은 별도 확인이 필요합니다.
- E52248 데이터시트에 대한 CRC, register field, OTP 순서·전기 조건 검증은 수행하지 않았습니다.
- 타깃 컴파일·링크, MISRA-C/Helix QAC, MCU 및 실제 버스 검증이 필요합니다.
