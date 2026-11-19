# Storage 검토 경고 및 확인 사항

검토일: 2026-11-15  
대상 브랜치: `prj_mobilgene_ccp`  
상태: 미조치 검토 사항 기록. 이 문서 작성에 따른 소스 수정은 없음.

## 1. 검토 범위와 검증 조건

`nvm_app.c/h`, `drv_nvm.c/h`, `SWC_NVM_MGR.c`, `SWC_NvM.h`,
`App_NvM_RamBlock.c/h`, README와 관련 Power/FSM Core 인터페이스를 검토했습니다.

실제 NvmApp 및 FSM Core/큐 소스에 임시 비동기 드라이버 mock을 연결하여
상태 조회 실패 후 Reset, PENDING 유지, 잘못된 블록 ID 처리를 검증했습니다.
해당 구성은 GCC C99의 `-Wall -Wextra -Wconversion -Werror`를 통과했습니다.
별도로 임시 RTE 선언 헤더를 제공하여 standalone 드라이버의 구문 검사를
수행했으며, RAM 저장소 배열 미선언으로 실패했습니다.

AUTOSAR 생성 헤더와 외부 `user_*` 정의가 현재 저장소에 없어 타깃 전체 빌드,
실제 NvM/Flash 동작, 선점 안전성 및 MISRA-C/Helix QAC 적합성은 검증하지
못했습니다. 기존 SWC 경로는 코드 검토 결과이며 실제 BSW를 연결한 재현 결과와
구분해야 합니다. 임시 검증 코드는 정식 회귀 테스트에 포함되지 않습니다.

## 2. 코드 항목 요약

| ID | 우선순위 | 항목 | 확인 상태 |
|---|---|---|---|
| STORAGE-W001 | 높음 | 상태 조회 실패 후 Reset 시 진행 중인 작업 버퍼 재사용 | mock 작업 진행 중 소스 덮어쓰기 재현 |
| STORAGE-W002 | 높음 | 신규 FSM과 기존 직접 NvM 요청 경로 병행 | `OLD_NVM_M` 활성화 및 runnable 경로 확인 |
| STORAGE-W003 | 높음 | 기존 쓰기 요청 유실 및 성공 오판정 | 요청 플래그·반환값·callback 처리 코드 확인 |
| STORAGE-W004 | 높음 | standalone RAM 드라이버 구현 누락 | 2026-11-19 조치: PC 드라이버 추가 및 회귀 테스트 통과 |
| STORAGE-W005 | 중간·상위 비트 사용 시 | 작업 비트맵 해제 시 상위 8비트 손실 | 축소 마스크 확인. 외부 비트 정의 미확인 |
| STORAGE-W006 | 중간 | PENDING 지속 시 전체 후속 요청 무기한 거부 | 모의 100초 후 WRITING 유지 확인 |
| STORAGE-W007 | 중간 | 잘못된 블록 ID와 길이 0 요청 수락 | BLOCK_MAX / 길이 0으로 재현 |
| STORAGE-W008 | 중간·호환 API 사용 시 | Power record와 기존 UserInfo 데이터 충돌 | 동일 블록 매핑 및 기존 저장 경로 확인 |

## 3. STORAGE-W001: 진행 중인 작업의 버퍼 재사용

위치: [nvm_app.c](nvm_app.c)의 `NvmApp_DoActiveRequest()`,
`NvmApp_PostJobFailed()`, `NvmApp_EntryIdle()` 및 요청 API.

`DrvNvm_GetErrorStatus()` 호출 실패는 NvM 작업이 끝났음을 의미하지 않습니다.
하지만 현재 코드는 JOB_FAILED를 등록하고 ERROR로 전이한 후, Reset으로
IDLE에 복귀하면 같은 `NvmApp_Shadow`를 새 요청에 사용합니다. 이전 비동기
작업이 여전히 버퍼를 참조하면 쓰기 데이터가 바뀌거나 읽기 대상 버퍼가 다른
작업과 공유될 수 있습니다. 활성 작업 중 `NvmApp_Init()`을 다시 호출해도
버퍼를 초기화하므로 재초기화 계약을 함께 정의해야 합니다.

재현 순서:

1. 값 `0x11`의 쓰기를 접수하고 mock 드라이버가 소스 포인터를 보관합니다.
2. 실제 작업은 진행 중으로 유지한 채 상태 조회 호출만 실패시킵니다.
3. ERROR 전이 후 Reset하여 IDLE로 복귀합니다.
4. 값 `0x22`의 새 쓰기를 요청합니다.
5. 이전 작업이 진행 중인데 새 요청은 STD_OK를 반환하고, 이전 소스 포인터의
   값도 `0x22`로 변경되는 것을 확인했습니다.

권장 조치 및 완료 기준:

- 상태 조회 실패와 실제 작업의 최종 실패를 구분합니다.
- 이전 작업의 종료 또는 취소 완료를 확인하기 전에는 버퍼 소유권을 해제하지 않습니다.
- Reset 및 재초기화 허용 조건을 명시하고 활성 작업 중 버퍼 초기화를 방지합니다.
- 조회 실패, 지연 완료, 취소 실패 및 Reset 후 새 요청에서 버퍼 수명을 검증합니다.

## 4. STORAGE-W002: 신규 FSM과 기존 직접 요청 경로 병행

위치: [SWC_NVM_MGR.c](SWC_NVM_MGR.c)의 `OLD_NVM_M` 정의와
`RE_NVM_MGR_10ms_Task()`.

소스에서 `OLD_NVM_M`을 정의하므로 신규 NvmApp 처리와 기존 RTE 직접 쓰기
경로가 같은 runnable에서 활성화됩니다. 기존 요청은 NvmApp의 busy 검사와
버퍼 관리 범위를 우회합니다. 같은 블록에 다른 작업이 접수되면 polling 결과를
원래 작업의 결과와 구분하지 못할 위험이 있습니다. 실제 요청 겹침 여부는 BSW의
블록별 접수 정책 및 호출 순서에 따라 달라집니다.

권장 조치 및 완료 기준:

- 블록별 요청 소유자를 정하고 기존 요청도 하나의 관리 경로로 전달합니다.
- 이전 코드와 신규 코드를 전환하는 설정 및 지원 조합을 명확히 합니다.
- 같은 블록의 기존/신규 요청, 요청 거부 및 완료 직후 재요청을 시험합니다.
- 외부 task/ISR의 요청도 포함하여 실행 직렬화 또는 배타 보호 조건을 정의합니다.

## 5. STORAGE-W003: 기존 요청 유실 및 성공 오판정

위치: [SWC_NVM_MGR.c](SWC_NVM_MGR.c)의 PartNO, MDate, SerialNO,
HWVersionInfo, ACCCnt, UserInfo 쓰기 처리와 `RE_NvMJobFinished_*()`.

일부 기존 경로는 블록이 PENDING이라 쓰기를 시작하지 못한 경우에도 요청
플래그를 해제합니다. 또한 `WriteBlock()` 반환값을 무시한 채 작업 비트를
설정하고 성공을 표시하는 경로가 있습니다. 요청이 거부되면 완료 callback이
발생하지 않아 작업 비트가 남을 수 있습니다. 완료 callback은 `JobResult`를
확인하지 않으므로 실제 쓰기 실패를 최종 결과에 반영하지 못합니다.

NotiInfo 경로는 접수 반환값에 따른 재시도가 있으므로 다른 경로와 구분해야
하지만, 여기서도 접수 성공 시점의 성공 표시와 실제 완료 결과를 구별해야 합니다.

권장 조치 및 완료 기준:

- 블록이 busy이거나 접수를 거부하면 요청을 보존하고 재시도 정책을 적용합니다.
- 작업 비트는 요청 접수가 성공한 경우에만 설정합니다.
- 요청 접수, 진행 중, 최종 성공 및 실패를 구분합니다.
- callback의 `JobResult` 또는 일관된 polling 결과로 최종 상태를 갱신합니다.
- PENDING, 접수 거부, 완료 실패 및 복구 시 요청/작업 플래그를 검증합니다.

## 6. STORAGE-W004: standalone 드라이버 구현 누락

조치 기록(2026-11-19): main에 `drv_nvm_pc.c`를 추가하고 기존 `drv_nvm.c`를
타깃 전용 조건으로 분리했습니다. PC 드라이버의 전체 블록 Read/Write/Erase,
잘못된 인자 거부 및 NvmApp 연동 회귀 테스트가 엄격 경고 빌드를 통과했습니다.
아래는 수정 전 검토 기록입니다. 실제 Flash 동작 검증을 의미하지 않습니다.

위치: [drv_nvm.c](drv_nvm.c)의 `DrvNvm_Init()` 및 Read/Write/Erase/상태 조회.

`PRJ_MOBILGENE_ENABLE == 0U` 분기는 `DrvNvm_Block`, `DrvNvm_BlockValid`,
`DrvNvm_BlockResult`를 참조하지만 현재 파일에 저장소 선언이 없습니다.
Read/Write/Erase와 상태 조회도 RAM 구현 분기 없이 생성 RTE 함수를 호출합니다.
RTE 헤더 역시 무조건 포함합니다.

임시 RTE 타입/함수 선언을 제공한 standalone 구문 검사에서도 위 세 배열의
미선언 오류가 발생했습니다. README의 RAM-backed 구현 설명과 일치하지 않습니다.

권장 조치 및 완료 기준:

- RTE 포함과 서비스 호출을 타깃 빌드로 한정하고 standalone 저장소를 구현합니다.
- 각 블록의 초기 invalid 상태, Write/Read/Erase 및 결과 상태를 정의합니다.
- 생성 RTE 없이 standalone 전체 드라이버가 엄격 경고 빌드를 통과하는지 확인합니다.
- 블록 간 격리와 잘못된 ID/길이/포인터 거부를 검증합니다.

## 7. STORAGE-W005: 작업 비트맵 마스크 폭 불일치

위치: [SWC_NVM_MGR.c](SWC_NVM_MGR.c)의 `b_WorkNvMStatus` 선언과
각 JobFinished callback의 비트 해제 연산.

`b_WorkNvMStatus`는 `u16`인데 비트 해제 마스크는 `(u8)(~d_WkBitPos_*)`로
축소됩니다. 이 마스크와 AND 연산하면 상위 8비트가 보존되지 않습니다.
상위 비트에 다른 작업 상태가 있으면 하위 작업 하나의 완료로 함께 지워져
`NVM_IsNotWorking()`이 작업 종료를 잘못 판단할 수 있습니다.

외부 `d_WkBitPos_*` 정의가 없어 실제 상위 비트 사용 여부는 확인하지 못했습니다.
따라서 현재 설정에서의 발생 여부와 폭이 맞지 않는 코드 자체를 구분해야 합니다.

권장 조치 및 완료 기준:

- 비트맵과 같은 폭의 타입으로 보수 및 해제 연산을 수행합니다.
- 외부 비트 정의의 중복, 범위 및 형식을 확인합니다.
- 상위/하위 작업 비트가 동시에 설정된 상태에서 한 비트만 해제되는지 검증합니다.

## 8. STORAGE-W006: PENDING 무기한 유지

위치: [nvm_app.c](nvm_app.c)의 `NvmApp_DoActiveRequest()`.

PENDING이 지속되는 경우 작업 제한 시간이나 지연 진단이 없습니다.
한 작업이 끝나지 않으면 다른 블록의 요청도 계속 STD_NOT_OK로 거부됩니다.
10ms main 함수를 10,000회 실행한 mock 시험에서 WRITING이 유지되고
후속 요청이 거부되는 것을 확인했습니다.

권장 조치 및 완료 기준:

- BSW의 최대 처리 시간과 상위 요구사항에 맞춰 지연 진단 및 제한 시간을 정합니다.
- 제한 시간 초과 시 취소/종료 확인 절차를 정의합니다.
- timeout만으로 버퍼를 해제하여 STORAGE-W001을 다시 만들지 않도록 합니다.
- 장기 PENDING, 제한 시간 직전 완료, 제한 시간 후 지연 완료를 검증합니다.

## 9. STORAGE-W007: 잘못된 ID와 길이 0 요청 수락

위치: [nvm_app.c](nvm_app.c)의 `NvmApp_RequestBlockRead()`와
`NvmApp_RequestBlockWrite()`.

잘못된 ID에 대한 `DrvNvm_GetBlockLength()` 결과는 0입니다. 요청 API는
ID 범위와 양수 길이를 직접 확인하지 않으므로, 잘못된 ID와 길이 0을 전달하면
길이 일치 조건을 통과합니다. BLOCK_MAX, 유효한 포인터, 길이 0으로 Write가
STD_OK를 반환하는 것을 재현했습니다.

실제 드라이버의 switch는 유효하지 않은 블록을 거부하므로 이 재현이 곧바로
메모리 범위 초과를 의미하지는 않습니다. 다만 잘못된 요청을 성공 접수하고
이후 ERROR로 처리하는 API 계약 문제가 있습니다.

권장 조치 및 완료 기준:

- 축소 변환 전 블록 ID 범위를 확인하고 길이 0을 거부합니다.
- Read/Write/Erase 및 드라이버의 ID 검증 방식을 일관되게 정합니다.
- BLOCK_MAX, 음수 및 큰 값으로 변환한 ID, 길이 0/불일치를 검증합니다.

## 10. STORAGE-W008: Power record와 UserInfo 충돌

위치: [nvm_app.c](nvm_app.c)의 `NVM_APP_POWER_BLOCK_ID`와
`NvmApp_RequestWrite()`, [SWC_NVM_MGR.c](SWC_NVM_MGR.c)의 기존 UserInfo 쓰기.

Power 호환 API는 UserInfo 블록의 8바이트 전체를 Power 상태, sequence,
reserved 및 checksum으로 생성합니다. 기존 `rub_UserInfo` 저장 경로도 있으므로
두 용도를 함께 사용하면 서로의 데이터를 덮어쓸 수 있습니다. README는 이
제한을 설명하지만 코드에서는 블록 용도를 분리하지 않습니다.

발생 여부는 Power 호환 API 활성화와 기존 UserInfo 사용 여부에 따라 달라집니다.

권장 조치 및 완료 기준:

- Power 전용 NvM 블록을 배정하거나 명시적인 공용 레코드 형식을 정의합니다.
- BSW 블록 길이, Driver 매핑 및 데이터 형식을 함께 맞춥니다.
- 기존 데이터가 있는 경우 변환 또는 초기화 정책을 정합니다.
- 두 용도의 저장·읽기와 재기동 후 데이터 보존을 검증합니다.

## 11. README 보완 사항

코드 경고와 별도로 [README.md](README.md)를 다음과 같이 보완해야 합니다.

| ID | 항목 | 필요한 보완 |
|---|---|---|
| STORAGE-D001 | standalone RAM 동작 | 현재 드라이버에는 구현이 누락됨. 구현·검증 후 실제 지원 범위에 맞게 설명 |
| STORAGE-D002 | 직렬화 및 버퍼 보호 보장 | 기존 직접 쓰기, 동시 호출, 상태 조회 실패 후 복구 조건을 포함하여 보장 범위를 한정 |
| STORAGE-D003 | callback 역할 | 신규 FSM은 polling을 사용하지만 기존 callback은 작업 비트맵을 변경함. 두 경로를 구분하고 통합 후 설명 갱신 |
| STORAGE-D004 | 요청 예제의 반환값 무시 | `(void)NvmApp_Request*()` 대신 접수 실패 처리 및 재시도 예제를 제시하고 최종 완료 확인과 구분 |
| STORAGE-D005 | standalone scheduler | 설명에 등장하는 `task.c`가 현재 저장소에서 확인되지 않음. 외부 제공 위치 또는 실제 실행 구성을 명시 |

문서 변경만으로 코드 항목을 조치 완료로 표시해서는 안 됩니다. 구현 수정과
해당 경로 검증을 마친 뒤 각 항목의 확인 상태를 갱신해야 합니다.
