# NVM 관리

## 1. 개요

`Storage` 모듈은 Application에서 요청한 비휘발성 데이터의 읽기, 쓰기 및
지우기를 AUTOSAR NvM 서비스와 연결합니다. Application은 생성된 RTE 함수나
`drv_nvm`을 직접 호출하지 않고 `NvmApp` 공개 API를 사용합니다.

```text
Power 및 기타 Application
        ↓ NvmApp_RequestBlock*()
NvmApp 비동기 FSM
        ↓ DrvNvm_*()
DrvNvm 블록 추상화
        ↓ Rte_Call_PS_NvMBlock_*()
AUTOSAR NvM
```

이 구조의 목적은 다음과 같습니다.

- 생성된 RTE 이름을 Application 코드로부터 격리
- 비동기 NvM 요청을 한 위치에서 직렬화
- 요청이 완료될 때까지 필요한 버퍼 수명 보장
- `GetErrorStatus()` 결과에 따른 공통 완료 및 오류 처리
- AUTOSAR 타깃과 PC 빌드에서 동일한 Application API 사용

## 2. 파일 구성

| 파일 | 역할 |
|---|---|
| `nvm_app.h` | NVM 상태, 이벤트, context 및 Application용 요청 API 선언 |
| `nvm_app.c` | Read/Write/Erase 비동기 FSM과 요청 버퍼 관리 |
| `drv_nvm.h` | 논리 블록 ID, 블록 길이 및 Driver API 선언 |
| `drv_nvm.c` | 논리 블록을 생성된 AUTOSAR NvM RTE 서비스에 연결 |
| `SWC_NVM_MGR.c` | AUTOSAR 10ms runnable과 NvM JobFinished callback 제공 |

`SWC_NVM_MGR.c`의 `RE_NVM_MGR_10ms_Task()`는 처음 호출될 때
`NvmApp_Init()`을 실행하고 이후 호출부터 `NvmApp_MainFunction10ms()`를
실행합니다. Standalone 빌드에서는 `task.c`가 같은 주기 함수를 호출합니다.

## 3. NvM 블록 구성

| 논리 블록 ID | RTE 포트 이름 | 길이 |
|---|---|---:|
| `DRV_NVM_BLOCK_ACC_CNT` | `NvMBlock_ACCCnt` | 1 byte |
| `DRV_NVM_BLOCK_AVN_LEARNT_INFO` | `NvMBlock_AVN_LearntInfo` | 50 bytes |
| `DRV_NVM_BLOCK_DTC_INFO` | `NvMBlock_DTCInfo` | 8 bytes |
| `DRV_NVM_BLOCK_HW_VERSION_INFO` | `NvMBlock_HWVersionInfo` | 4 bytes |
| `DRV_NVM_BLOCK_IAU_LEARNT_INFO` | `NvMBlock_IAU_LearntInfo` | 50 bytes |
| `DRV_NVM_BLOCK_MDATE` | `NvMBlock_MDate` | 4 bytes |
| `DRV_NVM_BLOCK_NOTI_INFO` | `NvMBlock_Noti_Info` | 6 bytes |
| `DRV_NVM_BLOCK_PART_NO` | `NvMBlock_PartNO` | 10 bytes |
| `DRV_NVM_BLOCK_SERIAL_NO` | `NvMBlock_SerialNO` | 10 bytes |
| `DRV_NVM_BLOCK_SYS_LOG_DATA` | `NvMBlock_SysLogData` | 52 bytes |
| `DRV_NVM_BLOCK_USER_INFO` | `NvMBlock_UserInfo` | 8 bytes |

`NvMBlock_ACCClntClrDTC`라는 데이터 의미와 생성된 `ACCCnt` RTE 포트 이름이
다를 수 있으므로 ARXML의 NvM 블록 및 Port Prototype 매핑을 확인해야 합니다.

블록 길이는 `drv_nvm.h`의 `DRV_NVM_*_LENGTH` 매크로로 관리합니다.
요청 길이가 `DrvNvm_GetBlockLength()` 결과와 정확히 일치하지 않으면 요청을
거부합니다.

## 4. 공개 API

Application에서 사용하는 API는 다음과 같습니다.

| API | 기능 |
|---|---|
| `NvmApp_Init()` | Driver, shadow buffer, context 및 FSM 초기화 |
| `NvmApp_MainFunction10ms()` | 이벤트 하나를 처리하고 활성 NvM 작업 상태 확인 |
| `NvmApp_RequestBlockRead()` | 지정 블록의 비동기 읽기 요청 등록 |
| `NvmApp_RequestBlockWrite()` | 데이터를 내부 buffer에 복사하고 쓰기 요청 등록 |
| `NvmApp_RequestBlockErase()` | 지정 블록의 비동기 지우기 요청 등록 |
| `NvmApp_RequestReset()` | `NVM_STATE_ERROR`에서 IDLE 복구 요청 |
| `NvmApp_GetContext()` | 진단용 read-only NVM context 반환 |
| `NvmApp_RequestWrite()` | 기존 Power 상태 저장 호환 API |

Application 계층에서는 다음 Driver API를 직접 호출하지 않는 것을 원칙으로
합니다.

```c
DrvNvm_Read();
DrvNvm_Write();
DrvNvm_Erase();
DrvNvm_GetErrorStatus();
```

위 함수들은 `NvmApp`과 `drv_nvm` 사이의 내부 계층 인터페이스입니다.

## 5. FSM 상태

```c
typedef enum
{
    NVM_STATE_IDLE = 0U,
    NVM_STATE_READING,
    NVM_STATE_WRITING,
    NVM_STATE_ERASING,
    NVM_STATE_ERROR,
    NVM_STATE_MAX
} Nvm_StateType;
```

| 상태 | 의미 | 주요 동작 |
|---|---|---|
| `IDLE` | 새 요청 대기 | 요청 제어 flag와 이전 read destination 정리 |
| `READING` | 비동기 읽기 진행 | Entry에서 `DrvNvm_Read()` 1회 호출 후 상태 확인 |
| `WRITING` | 비동기 쓰기 진행 | Entry에서 `DrvNvm_Write()` 1회 호출 후 상태 확인 |
| `ERASING` | 비동기 지우기 진행 | Entry에서 `DrvNvm_Erase()` 1회 호출 후 상태 확인 |
| `ERROR` | 요청 실패 상태 | 명시적인 Reset 요청 대기 |

FSM은 `NvmApp_MainFunction10ms()` 호출 한 번에 이벤트를 최대 한 개 처리합니다.
완료 이벤트가 Do 함수에서 등록되면 다음 10ms 호출에서 IDLE 또는 ERROR로
전이합니다.

## 6. FSM 이벤트와 전체 전이

| 현재 상태 | 이벤트 | 다음 상태 |
|---|---|---|
| `IDLE` | `READ_REQUEST` | `READING` |
| `IDLE` | `WRITE_REQUEST` | `WRITING` |
| `IDLE` | `ERASE_REQUEST` | `ERASING` |
| `READING` | `JOB_DONE` | `IDLE` |
| `READING` | `JOB_FAILED` | `ERROR` |
| `WRITING` | `JOB_DONE` | `IDLE` |
| `WRITING` | `JOB_FAILED` | `ERROR` |
| `ERASING` | `JOB_DONE` | `IDLE` |
| `ERASING` | `JOB_FAILED` | `ERROR` |
| `ERROR` | `RESET` | `IDLE` |

```text
                  ┌── READ_REQUEST ──> READING ──┐
                  │                              │
IDLE ─────────────┼── WRITE_REQUEST ─> WRITING ──┼── JOB_DONE ──> IDLE
                  │                              │
                  └── ERASE_REQUEST ─> ERASING ──┘

READING / WRITING / ERASING ── JOB_FAILED ──> ERROR
ERROR ── RESET ──> IDLE
```

## 7. 비동기 요청 처리

### Read

```text
NvmApp_RequestBlockRead()
→ READ_REQUEST queue 등록
→ READING entry에서 DrvNvm_Read() 1회 호출
→ 10ms마다 DrvNvm_GetErrorStatus() 확인
→ OK이면 shadow 데이터를 Application destination에 복사
→ JOB_DONE 등록
→ 다음 10ms 실행에서 IDLE 전이
```

RTE에는 `NvmApp_Shadow` 주소를 전달합니다. 따라서 NvM이 비동기로 데이터를
기록하는 동안 함수 지역변수 주소가 사라지는 문제가 없습니다. Application이
전달한 destination은 완료 시점의 복사를 위해 작업 완료까지 유효해야 합니다.
Power context와 같은 정적 수명 객체의 멤버를 사용하는 것이 권장됩니다.

Read가 실패하면 Application destination은 변경하지 않습니다.

### Write

```text
NvmApp_RequestBlockWrite()
→ 입력 데이터를 NvmApp_Shadow에 즉시 복사
→ WRITE_REQUEST queue 등록
→ WRITING entry에서 DrvNvm_Write() 1회 호출
→ 10ms마다 DrvNvm_GetErrorStatus() 확인
→ OK이면 JOB_DONE 등록
→ 다음 10ms 실행에서 IDLE 전이
```

입력 데이터는 요청 함수 안에서 shadow buffer로 복사되므로 Write 호출자는
지역변수를 전달할 수 있습니다.

### Erase

```text
NvmApp_RequestBlockErase()
→ ERASE_REQUEST queue 등록
→ ERASING entry에서 DrvNvm_Erase() 1회 호출
→ 10ms마다 DrvNvm_GetErrorStatus() 확인
→ OK이면 JOB_DONE 등록
→ 다음 10ms 실행에서 IDLE 전이
```

## 8. NvM 결과 처리

Driver 계층은 AUTOSAR `NvM_RequestResultType`을 다음 공통 결과로 변환합니다.

| Driver 결과 | 의미 | FSM 처리 |
|---|---|---|
| `DRV_NVM_REQUEST_PENDING` | NvM 작업 진행 중 | 현재 활성 상태 유지 |
| `DRV_NVM_REQUEST_OK` | 작업 정상 완료 | `JOB_DONE` 등록 |
| `DRV_NVM_REQUEST_FAILED` | 요청 또는 NvM 작업 실패 | `JOB_FAILED` 등록 |

`NVM_REQ_OK`와 `NVM_REQ_PENDING` 이외의 NvM 결과는 현재 모두
`DRV_NVM_REQUEST_FAILED`로 변환합니다. Integrity failure, invalidated block,
cancel 및 redundancy failure를 구별해야 한다면 Driver 결과 enum을 확장해야
합니다.

RTE 서비스 함수의 `E_OK`는 작업 완료가 아니라 요청 접수를 의미합니다.
최종 완료 여부는 반드시 `GetErrorStatus()` 결과로 판단합니다.

## 9. 동시 요청과 버퍼 보호

현재 NVM FSM은 전체 블록을 통틀어 한 번에 하나의 요청만 처리합니다.

- FSM이 IDLE이 아니면 새 요청을 `STD_NOT_OK`로 거부합니다.
- 요청 이벤트가 queue에 있지만 아직 dispatch되지 않은 동안에도 추가 요청을
  거부합니다.
- Write 데이터는 최대 블록 길이인 52-byte 내부 shadow buffer에 보관합니다.
- Read도 같은 shadow buffer를 사용한 후 성공 시 destination으로 복사합니다.
- 동적 메모리는 사용하지 않습니다.

요청을 동시에 queue에 쌓지 않으므로 서로 다른 블록이 하나의 shadow buffer를
덮어쓰는 문제가 없습니다. 여러 요청을 순차 처리해야 한다면 Application에서
`STD_NOT_OK`를 확인하여 재시도하거나 별도 상위 요청 queue를 추가해야 합니다.

## 10. Power에서 ACC count 사용 예

Power context에 정적 수명의 저장 공간을 둡니다.

```c
typedef struct
{
    /* Existing members... */
    uint8 accCount;
} Power_ContextType;
```

읽기:

```c
(void)NvmApp_RequestBlockRead(
    DRV_NVM_BLOCK_ACC_CNT,
    &PowerApp_Context.accCount,
    DRV_NVM_ACC_CNT_LENGTH);
```

값 `8U` 쓰기:

```c
uint8 accCount = (uint8)8U;

(void)NvmApp_RequestBlockWrite(
    DRV_NVM_BLOCK_ACC_CNT,
    &accCount,
    DRV_NVM_ACC_CNT_LENGTH);
```

지우기:

```c
(void)NvmApp_RequestBlockErase(DRV_NVM_BLOCK_ACC_CNT);
```

요청 결과 확인:

```c
const Nvm_ContextType *nvmContext = NvmApp_GetContext();

if ((nvmContext->state == NVM_STATE_IDLE) &&
    (nvmContext->activeBlock == DRV_NVM_BLOCK_ACC_CNT) &&
    (nvmContext->requestResult == DRV_NVM_REQUEST_OK))
{
    /* The requested ACC count operation completed successfully. */
}
```

요청 함수의 반환값이 `STD_NOT_OK`이면 요청이 등록되지 않은 것이므로 상태를
완료로 판단해서는 안 됩니다.

## 11. 기존 Power 상태 저장 호환 API

`NvmApp_RequestWrite(uint8 powerState)`는 기존 코드 호환을 위한 함수입니다.
8-byte record는 다음 형식을 사용합니다.

| Byte | 내용 |
|---:|---|
| 0 | Power state |
| 1 | Rolling sequence |
| 2~6 | Reserved, `0U` |
| 7 | Byte 0~6 XOR checksum |

현재 별도의 Power 전용 NvM 블록이 없어서 다음 매핑을 사용합니다.

```c
#define NVM_APP_POWER_BLOCK_ID (DRV_NVM_BLOCK_USER_INFO)
```

실제 `UserInfo` 데이터도 사용한다면 Power record와 충돌하므로 이 매핑을 그대로
사용하면 안 됩니다. 양산 ARXML에 Power 상태 전용 NvM 블록을 생성하고
`DrvNvm_BlockIdType`, Driver RTE mapping 및 위 매크로를 함께 변경하는 것이
권장됩니다.

## 12. AUTOSAR RTE 연결

각 블록은 다음 네 종류의 생성 함수에 연결됩니다.

```c
Rte_Call_PS_NvMBlock_<Block>_ReadBlock();
Rte_Call_PS_NvMBlock_<Block>_WriteBlock();
Rte_Call_PS_NvMBlock_<Block>_EraseNvBlock();
Rte_Call_PS_NvMBlock_<Block>_GetErrorStatus();
```

`RE_NvMJobFinished_*()` callback은 현재 생성된 RTE 계약을 만족하기 위해
존재하지만 완료 판정에는 사용하지 않습니다. 현재 완료 판정 방식은 10ms
주기의 `GetErrorStatus()` polling입니다. 향후 callback 방식으로 변경한다면
polling과 callback이 같은 작업을 중복 완료 처리하지 않도록 한 방식만 선택해야
합니다.

## 13. 오류 처리와 복구

다음 조건에서 활성 작업은 `NVM_STATE_ERROR`로 전이합니다.

- RTE Read/Write/Erase 요청 함수가 `E_OK`를 반환하지 않음
- `GetErrorStatus()` RTE 호출 실패
- NvM 결과가 `NVM_REQ_OK` 또는 `NVM_REQ_PENDING`이 아님

복구는 다음 API로 요청합니다.

```c
(void)NvmApp_RequestReset();
```

Reset은 `ERROR → IDLE` 전이만 제공합니다. 실패한 NvM 작업을 자동으로 재시도하지
않습니다. Flash 수명과 반복 고장 가능성을 고려하여 재시도 횟수 및 지연 정책은
블록 요구사항에 따라 별도로 정의해야 합니다.

## 14. 유지보수 주의사항

- 새 NvM 블록을 추가하면 블록 ID, 길이, 네 종류의 Driver switch mapping을 모두
  추가합니다.
- `DRV_NVM_BLOCK_MAX`는 항상 enum의 마지막에 유지합니다.
- `DRV_NVM_MAX_BLOCK_LENGTH`는 가장 큰 블록보다 작아서는 안 됩니다.
- State enum 순서와 `NvmApp_StateOperations[]` 순서는 반드시 일치해야 합니다.
- 전이 개수는 `sizeof`로 자동 계산하며 수동 count를 사용하지 않습니다.
- 모든 switch에는 `default`를 유지합니다.
- 비동기 Read destination으로 함수 지역변수를 전달하지 않습니다.
- Application은 `DrvNvm_*`를 직접 호출하지 않습니다.
- NvM block 길이와 RTE/ARXML configured length가 일치하는지 확인합니다.
- NvM 쓰기 빈도를 제한하여 Flash endurance 요구사항을 만족해야 합니다.

## 부록: PC Standalone 동작

`PRJ_MOBILGENE_ENABLE == 0U`이면 `drv_nvm.c`는 AUTOSAR RTE 대신 RAM-backed
block storage를 사용합니다.

- 모든 block storage는 `DrvNvm_Init()`에서 0으로 초기화됩니다.
- 초기화 직후 Read는 valid data가 없으므로 실패합니다.
- Write는 RAM에 복사한 후 즉시 `DRV_NVM_REQUEST_OK`가 됩니다.
- Erase는 RAM block을 0으로 지우고 valid flag를 해제합니다.
- 실행 종료 후 데이터는 유지되지 않습니다.

PC 구현은 FSM과 Application 연동 검증용이며 실제 Flash endurance, NvM queue,
CRC, redundancy 및 power-loss 동작을 모사하지 않습니다.

## 부록: 테스트 및 Trace32 확인

`NvmApp_GetContext()`가 반환하는 context에서 다음 항목을 확인합니다.

| 항목 | 의미 |
|---|---|
| `state` | 최근 10ms 실행에서 동기화된 typed NVM 상태 |
| `event` | 마지막으로 dispatch된 NVM 이벤트 |
| `activeBlock` | 현재 또는 마지막으로 처리한 논리 블록 ID |
| `requestResult` | 최근 Driver request 결과 |
| `fsm.state` | 실제 authoritative FSM 상태 |
| `fsm.eventQueue.count` | 처리 대기 중인 이벤트 개수 |

정상 Write 예:

```text
IDLE → WRITING → IDLE
requestResult: PENDING → OK
```

정상 Read 예:

```text
IDLE → READING → IDLE
requestResult: PENDING → OK
```

정상 Erase 예:

```text
IDLE → ERASING → IDLE
requestResult: PENDING → OK
```

요청 실패 예:

```text
IDLE → READING / WRITING / ERASING → ERROR
requestResult: FAILED
```
