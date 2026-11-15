# LED Application

## 1. 개요

LED 모듈은 E52248 LED controller의 전원·reset·OTP·I2C 통신과 16채널
current, brightness 및 port 설정을 관리합니다. 상위 동작은 이벤트 큐 기반
FSM으로 구성되며 실제 I2C 송수신은 별도의 비동기 communication FSM이
처리합니다.

## 2. 기능 사양 요약

| 기능 | 기능 동작 |
|---|---|
| 장치 초기화 | E52248 reset, I2C 초기화 및 정상 slave address 응답을 순차 확인합니다. |
| OTP programming | 정상 주소가 응답하지 않는 미기록 장치에 초기 주소로 OTP를 한 word씩 기록합니다. |
| OTP validation | 설정 table의 모든 page/address를 다시 읽어 기대값과 일치하는지 검증합니다. |
| Startup 설정 | Current, Brightness, Port 순서로 초기 register 설정을 완료한 뒤 NORMAL에 진입합니다. |
| Runtime 설정 | NORMAL에서 current, brightness 또는 port 변경 요청을 각각 처리한 뒤 NORMAL로 복귀합니다. |
| 출력 활성화 | 부팅 직후에는 OFF를 유지하고 유효한 LCAN 밝기 설정이 성공한 뒤 출력합니다. |
| 밝기 제어 | Detent 활성 시 최대 밝기를 우선하고 그 외에는 Auto Brightness 단계를 duty 값으로 변환합니다. |
| 진단 측정 | 16채널 VLED, VDIF, ILED 및 junction temperature를 비동기로 읽습니다. |
| 오류 처리 | I2C, OTP validation 또는 설정 오류 발생 시 FAULT로 전이하고 출력을 차단합니다. |
| 비동기 통신 | I2C callback은 flag만 설정하고 실제 통신 FSM과 결과 처리는 5ms task에서 수행합니다. |

## 3. 파일 구성

| 파일 | 역할 |
|---|---|
| `knob_led.c/.h` | LED 애플리케이션 FSM, context 및 공개 요청 API |
| `knob_led_cfg.c/.h` | 기본 current와 밝기 단계 변환 테이블 |
| `knob_led_otp.c/.h` | OTP 1-word write sequence와 read-back validation |
| `e52248_comm.c/.h` | 비동기 E52248 I2C 통신 FSM과 frame 변환 |
| `e52248_register.h` | E52248 register, page 및 주소 정의 |
| `knob_led_drv.c/.h` | GPIO, I2C 및 RTE hardware adaptation |
| `SWC_KnobLed.c` | AUTOSAR runnable과 I2C notification 어댑터 |

`SWC_KnobLed.c`와 `knob_led_drv.c`의 target 경로는 생성된 RTE 및 MemMap
헤더에 의존합니다.

## 4. 실행 주기

| 함수/runnable | 주기 또는 조건 | 역할 |
|---|---|---|
| `LedApp_MainFunction5ms()` | 5ms | E52248 통신 FSM 진행 및 완료 결과 처리 |
| `LedApp_MainFunction10ms()` | 10ms | LED timer 확인, 이벤트 1개 dispatch, do 실행 |
| `FSM_Timer_Notification()` | 1ms | LED를 포함한 중앙 timer 갱신 |
| I2C Tx notification | ISR/callback | 송신 완료 flag 설정 |
| I2C Rx notification | ISR/callback | 수신 완료 flag 설정 |
| I2C Error notification | ISR/callback | hardware error flag 설정 |

I2C callback에서는 통신 FSM을 직접 실행하지 않고 `volatile` notification
flag만 갱신합니다. 실제 결과 처리는 5ms 태스크에서 수행합니다.

## 5. LED 상태

| 상태 | 설명 |
|---|---|
| `LED_STATE_RESET` | E52248 reset assert 및 초기 지연 |
| `LED_STATE_I2C_INIT` | I2C 초기화 후 안정화 대기 |
| `LED_STATE_ADDR_CHK` | 정상 slave address 응답 확인 |
| `LED_STATE_OTP_RESET` | PROG 활성화 및 programming reset 유지 |
| `LED_STATE_OTP_READY_WAIT` | reset 해제 후 OTP 접근 준비 대기 |
| `LED_STATE_OTP_WRITE` | OTP 설정을 한 word씩 순차 기록 |
| `LED_STATE_OTP_VALIDITY` | 설정된 page/address 값을 읽어 기대값과 비교 |
| `LED_STATE_CONFIG_RESET` | OTP 완료 후 normal mode reset 유지 |
| `LED_STATE_CONFIG_READY_WAIT` | reset 해제 후 통신 준비 대기 |
| `LED_STATE_SET_CURRENT` | 16채널 current 설정 |
| `LED_STATE_SET_BRIGHT` | 16채널 brightness 설정 |
| `LED_STATE_SET_PORT` | 사용할 LED port 설정 |
| `LED_STATE_NORMAL` | runtime 설정 및 진단 요청 허용 |
| `LED_STATE_FAULT` | 출력 차단 및 reset 요청 대기 |

## 6. 초기화 상태 흐름

### 이미 OTP가 설정된 장치

정상 주소가 응답하면 OTP write를 생략합니다.

```text
RESET
→ I2C_INIT
→ ADDR_CHK
→ SET_CURRENT
→ SET_BRIGHT
→ SET_PORT
→ NORMAL
```

### OTP가 설정되지 않은 장치

정상 주소 접근이 통신 오류로 끝나면 초기 주소에서 OTP programming을
수행합니다.

```text
RESET
→ I2C_INIT
→ ADDR_CHK
→ OTP_RESET
→ OTP_READY_WAIT
→ OTP_WRITE
→ OTP_VALIDITY
→ CONFIG_RESET
→ CONFIG_READY_WAIT
→ SET_CURRENT
→ SET_BRIGHT
→ SET_PORT
→ NORMAL
```

OTP validation이 실패하거나 I2C 오류가 발생하면 `LED_STATE_FAULT`로
전이합니다.

## 7. Reset 및 OTP GPIO 타이밍

| 구간 | PROG | 논리 reset | 시간 |
|---|---|---|---:|
| `OTP_RESET` | ON | ON | 20ms |
| `OTP_READY_WAIT` | ON | OFF | 10ms |
| `OTP_WRITE/VALIDITY` | ON | OFF | 비동기 처리 |
| `CONFIG_RESET` | OFF | ON | 20ms |
| `CONFIG_READY_WAIT` | OFF | OFF | 15ms |

`GpioSet_E52248_nReset()`의 논리 ON은 active-low RESET_N pin을 Low로
구동하고, 논리 OFF는 pin을 High로 해제합니다.

OTP 완료 후에는 reset을 해제하자마자 I2C를 시작하지 않습니다.
`CONFIG_READY_WAIT`에서 15ms를 추가 대기한 뒤 첫 normal-mode current write를
시작합니다.

## 8. I2C 주소

| 매크로 | 값 | 용도 |
|---|---:|---|
| `I2C_ADDR_INIT` | `0x3F` | 미기록 IC의 startup OTP 접근 |
| `I2C_ADDR_VALID` | `0x21` | normal 통신 및 runtime OTP test write |

미기록 IC의 startup OTP write sequence는 초기 주소를 사용합니다. Normal
상태에서 `LedApp_RequestOtpWrite()`로 시작한 test write와 이어지는
validation은 유효 주소를 사용합니다. OTP 완료 후 PROG를 비활성화하고
reset한 뒤에도 normal 설정과 runtime 진단은 유효 주소를 사용합니다.

## 9. OTP programming 및 validation

`LedOtp_ConfigData[]`는 page, register address 및 기대값으로 구성됩니다.
각 항목은 다음 순서로 처리됩니다.

```text
OTP page 선택
→ 해당 register 1-word write
→ 다음 설정 항목 반복
```

Validation은 동일한 설정 테이블을 사용합니다.

```text
OTP page 선택
→ 해당 register 1-word read
→ actual value와 expected value 비교
→ 일치하면 다음 항목
→ 전체 일치 시 validation 성공
```

OTP write 직후 validation 성공 시 `CONFIG_RESET`으로 이동합니다. Normal
상태에서 `LedApp_RequestOtpValidation()`으로 요청한 validation이 성공하면
다시 `NORMAL`로 복귀합니다.

값 불일치는 `LED_EVENT_OTP_INVALID`, 통신 오류는
`LED_EVENT_COMM_ERROR`로 구분합니다. 실패 위치는 다음 context 값으로
확인할 수 있습니다.

```c
otpValidationValid
otpValidationIndex
otpValidationExpectedValue
otpValidationActualValue
```

### 현재 OTP 주요 설정

현재 `LedOtp_ConfigData[]`에는 다음 진단 임계값이 저장됩니다.

| 항목 | 비교 측정값 | 설정값 | Raw 값 |
|---|---|---:|---:|
| LED Open threshold 1 | `VLED` | 1V | 36 |
| LED Short threshold 1 | `VDIF` | 3V | 108 |
| ILED Open threshold | `ILED` | 2mA | 20 |

전압 임계값은 `36 counts/V`, 전류 임계값은 `10 counts/mA`로 인코딩합니다.
기본 진단 방향은 다음과 같습니다.

```text
VLED < Open threshold   → Open 후보
VDIF < Short threshold → Short 후보
```

최종 상태 비트에는 채널 enable, threshold selector, DIAG routing, PWM sampling
및 ILED 조건도 영향을 줄 수 있습니다. 따라서 측정값과 상태 비트를 함께
확인해야 합니다.

현재 채널 관련 OTP 구성은 다음과 같습니다.

- CH2~CH9를 LED output으로 활성화
- CH2~CH7을 DIAG0 group에 할당
- CH8~CH9를 DIAG1 group에 할당
- CH8~CH9의 PWM combine 및 analog current bundle 활성화
- CH2~CH9에 lower IDAC current range 선택

`OTP_LED_OPEN_SEL_*`, `OTP_LED_SHORT_SEL_*` 값은 네 채널의 selector가
하나의 register에 인코딩된 값입니다. 값을 변경할 때는 단순 채널 bitmask로
취급하지 말고 E52248의 채널별 selector bit field를 기준으로 계산해야 합니다.
`OTP_DIAG_CONFIG`의 `0xC5`도 DIAG output, operating mode 및 counter 설정이
결합된 인코딩 값입니다.

### OTP 설정 그룹별 설명

`LedOtp_ConfigData[]`의 배열 순서가 실제 programming 순서입니다. 각 항목은
page를 선택한 뒤 한 register word씩 기록됩니다.

| Page/설정 그룹 | 현재 설정 | 기능 및 영향 |
|---|---|---|
| Page 1 PWM | Prescaler `25U`, Period `1023U` | 300Hz, 10-bit PWM 기준을 설정합니다. |
| Page 1 PWM combine | `OTP_PWM_COMBINE_PRI = 0x10U` | CH8과 CH9의 PWM 결합 제어를 활성화합니다. |
| Page 1 Open 진단 | Threshold 1 = `36U` | VLED 기준 1V Open 임계값입니다. |
| Page 1 Short 진단 | Threshold 1 = `108U` | VDIF 기준 3V Short 임계값입니다. |
| Page 1 ILED Open | `20U` | ILED 기준 2mA Open 임계값입니다. |
| Page 1 보호 | Critical temperature = `125 + 273` | Junction temperature critical 기준을 Kelvin raw 값으로 설정합니다. |
| Page 1 통신 | `OTP_COM_TIMEOUT = 0x00U` | E52248 communication timeout 기능을 비활성화합니다. |
| Page 2 IDAC range | CH2~CH9 lower range 선택 | 낮은 LED 전류 범위에서 사용할 current DAC reference를 선택합니다. |
| Page 2 Thermal derating | 100°C 시작, 124°C 종료 | 과온 구간에서 LED 전류를 감소시키는 범위와 gain을 설정합니다. |
| Page 2 DIAG common | `OTP_DIAG_CONFIG = 0xC5U` | DIAG output, operating mode 및 counter 관련 bit field의 결합값입니다. |
| Page 2 DIAG0 routing | CH2~CH7 | 해당 채널의 진단 결과를 DIAG0 group에 연결합니다. |
| Page 2 DIAG1 routing | CH8~CH9 | 해당 채널의 진단 결과를 DIAG1 group에 연결합니다. |
| Page 2 ISINK bundle | `OTP_ISINK_BUNDLE = 0x10U` | CH8과 CH9의 analog current bundling을 활성화합니다. |
| Page 2 Supply selector | 모든 group `0x00U` | 진단 기준 supply로 VS를 선택합니다. |
| Page 3 Channel enable | `0xFCU`, `0x03U` | CH2~CH7과 CH8~CH9를 활성화합니다. |
| Device address | `I2C_ADDR_VALID` | Programming 이후 normal 통신 주소를 `0x21`로 설정합니다. |
| Customer validation | `I2C_ADDR_VALID` | 정상 OTP 설정 여부를 확인하기 위한 customer 값을 저장합니다. |

### OTP 설정 간 관계와 변경 주의사항

- `OTP_LED_ENABLE_*`은 채널 사용 가능 여부를 결정합니다. Runtime current나
  brightness 값을 기록하더라도 OTP에서 비활성화된 채널은 정상 출력되지
  않습니다.
- `OTP_IDAC_REF_SEL_*`은 current 설정값과 실제 출력 전류의 관계를 바꿉니다.
  Current code만 변경하지 말고 선택한 40mA/100mA 범위와 데이터시트의 보증
  구간을 함께 확인해야 합니다.
- `OTP_PWM_COMBINE_PRI`와 `OTP_ISINK_BUNDLE`은 채널 결합 동작에 관여합니다.
  특히 CH8과 CH9를 다른 채널처럼 독립적으로 밝기·전류 제어해야 한다면 두
  설정의 bit mapping과 실제 보드 동작을 별도로 검증해야 합니다.
- `OTP_DIAG0_CONFIG_*`와 `OTP_DIAG1_CONFIG_*`은 진단 결과의 route 설정이며
  LED channel enable mask가 아닙니다. DIAG group 설정과 channel enable 값을
  서로 대체해서 사용할 수 없습니다.
- Open/Short selector register는 채널별 threshold 선택값을 packed field로
  저장합니다. `0x50U`, `0x51U`, `0x01U`를 단순 channel bitmask로 해석하면
  잘못된 진단 임계값이 선택될 수 있습니다.
- `OTP_DIAG_CONFIG = 0xC5U`의 정확한 operating mode 명칭과 각 bit 의미는
  사용하는 E52248 datasheet revision의 bit-field 정의로 확인해야 합니다.
  주석만으로 MLM 등의 mode를 단정하지 않습니다.
- OTP에 저장되는 값은 reset 이후에도 장치 동작의 기본값으로 사용됩니다.
  `LedApp_RequestSetCurrent()`, `LedApp_RequestSetBrightness()` 등의 runtime
  write는 현재 동작 register를 변경하지만 OTP 값을 다시 programming하지는
  않습니다.
- OTP programming 후에는 전체 table의 address와 value를 read-back하여
  validation합니다. `otpValidationValid == TRUE`일 때만 모든 항목이 기대값과
  일치한 것으로 판단합니다.
- OTP는 되돌리기 어렵기 때문에 설정 변경 전 PWM, IDAC, bundling, DIAG route,
  supply 및 channel enable 조합을 새 IC와 실제 부하 조건에서 검증해야 합니다.

### OTP sequence service

| API | 기능 |
|---|---|
| `LedOtp_StartWrite()` | 지정 slave address에서 OTP write sequence 시작 |
| `LedOtp_ContinueWrite()` | 완료된 I2C job 이후 다음 page 또는 data write 진행 |
| `LedOtp_AbortWrite()` | 진행 중인 write sequence 중단 |
| `LedOtp_StartValidityCheck()` | 전체 설정 table read-back 검증 시작 |
| `LedOtp_ContinueValidityCheck()` | 완료된 read 이후 비교 및 다음 항목 진행 |
| `LedOtp_GetValidationResult()` | 검증 index, expected 및 actual 결과 반환 |
| `LedOtp_AbortValidityCheck()` | 진행 중인 validation sequence 중단 |

## 10. Current, brightness 및 port 설정

### Current

`LedApp_RequestSetCurrent(portNumber, currentValue)`는 개별 port 또는 전체
port current 값을 변경합니다.

- `portNumber` 0~15: 개별 채널
- `portNumber == LED_PORT_MAX`: 전체 채널
- `currentValue`: 0~1023의 E52248 10-bit 설정값

### Brightness

`LedApp_RequestSetBrightness()`의 입력 범위는 다음과 같습니다.

| 입력 step | 처리 |
|---:|---|
| 0~1 | 무시 |
| 2~100 | `LedCfg_BrightnessTable[]` 사용 |
| 101~200 | 100%, `0x3FF` 사용 |
| 201 이상 | 무시 |

밝기 register 값은 10-bit PWM duty 값입니다.

```text
0%   = 0x000
50%  = 0x200
100% = 0x3FF
```

설정 service는 `LedCfg_LoadDefaultCurrent()`로 기본 channel current를
초기화하고, `LedCfg_GetBrightnessValue()`로 밝기 step을 10-bit duty 값으로
변환합니다. `LedCfg_GetDefaultBrightnessValue()`는 startup 기본 duty를
제공합니다.

### Runtime 상태 흐름

```text
NORMAL + CUR_REQ    → SET_CURRENT → NORMAL
NORMAL + BRIGHT_REQ → SET_BRIGHT  → NORMAL
NORMAL + PORT_REQ   → SET_PORT    → NORMAL
```

동시에 두 설정을 요청하지 않도록 `settingRequestPending`으로 보호합니다.

## 11. LCAN 밝기 제어

`lcan_input.c`는 HU 입력을 LED brightness 요청으로 변환합니다.

우선순위는 다음과 같습니다.

1. `CLU_DtntOutSta_L == ON`: 자동 밝기를 무시하고 최대 밝기
2. 유효한 auto-brightness 값 사용
3. auto-brightness가 0이면 step 13 사용

부팅 후 LED output은 기본적으로 OFF입니다. 유효한 detent 또는 자동 밝기
요청이 설정에 성공하면 `LedApp_EnableOutput()`으로 출력을 활성화합니다.
이후 fault, power sleep 또는 명시적인 disable 요청이 있기 전까지 output
enable 상태를 유지합니다.

## 12. 진단 읽기

다음 요청은 `LED_STATE_NORMAL`이고 다른 설정/진단 작업이 없을 때 허용됩니다.

| API | 결과 |
|---|---|
| `LedApp_RequestReadVled()` | `vledValues[16]` |
| `LedApp_RequestReadVdif()` | `vdifValues[16]` |
| `LedApp_RequestReadIled()` | `iledValues[16]` |
| `LedApp_RequestReadTemperature()` | `temperatureRaw`, `temperatureCelsius` |

각 결과의 `*Valid` 값이 `TRUE`일 때만 최신 완료 결과로 사용해야 합니다.

## 13. E52248 communication FSM

통신 모듈은 1, 4, 16개의 10-bit word read/write를 지원합니다. 시작 API는
job을 등록하고 즉시 반환하며, 완료 결과는 다음 API로 소비합니다.

```c
E52248_Comm_StartRead();
E52248_Comm_StartWrite();
E52248_Comm_TakeResult();
E52248_Comm_CopyReadData();
```

공개 상태:

```c
E_COMM_STS_IDLE
E_COMM_STS_BUSY
E_COMM_STS_ERR_COM
E_COMM_STS_ERR_CRC
```

통신 service의 `E52248_Comm_Init()`과 `E52248_Comm_MainFunction5ms()`는
context와 비동기 FSM을 초기화·진행합니다. Tx/Rx/Error callback은 각각
`E52248_Comm_TxNotification()`, `E52248_Comm_RxNotification()` 및
`E52248_Comm_ErrorNotification()`으로 완료 flag만 전달합니다.
`E52248_Comm_GetContext()`는 read-only 통신 context를 제공합니다.

### I2C 채널 선택

`App_CCP/Common/prj_defs.h`의 `I2C_SCB3_ENABLE`로 E52248에 사용할 I2C
하드웨어 채널을 빌드 시 선택합니다.

| 설정값 | 선택 채널 | Reset 출력 | 통신 context |
|---|---|---|---|
| `1U` | SCB3 | `LED_RST_N_Output` | `E52248_I2C_CHANNEL_SCB3` |
| `0U` | SCB7 | `LED_RST_N_Output2` | `E52248_I2C_CHANNEL_SCB7` |

선택값은 I2C 초기화와 read/write 요청에도 동일하게 적용됩니다. SCB3와 SCB7의
Rx, Tx, Error notification runnable은 모두 E52248 통신 서비스에 연결되어
있으며, 실제 통신 완료 알림은 선택한 하드웨어 채널에서 발생해야 합니다.
E52248에는 두 채널을 동시에 사용하지 않습니다. 설정을 변경한 뒤에는 전체
재빌드가 필요합니다.

### Hardware adaptation

| API 그룹 | 기능 |
|---|---|
| `I2C_E52248_Init/ReadData/WriteData()` | 선택된 SCB 채널 초기화와 비동기 I2C 요청 |
| `GpioSet_E52248_VccEnable()` | E52248 공급전원 출력 제어 |
| `GpioSet_E52248_OutputEnable()` | LED output enable 제어 |
| `GpioSet_E52248_nReset()` | Active-low reset의 논리 On/Off 변환 및 출력 |
| `GpioSet_E52248_OtpProg()` | OTP programming pin 제어 |
| `GpioGet_E52248_Diag0/Diag1()` | E52248 diagnostic GPIO level 조회 |

## 14. 주요 API

| API | 설명 |
|---|---|
| `LedApp_Init()` | GPIO, 통신 context 및 LED FSM 초기화 |
| `LedApp_MainFunction5ms()` | 비동기 통신 진행 및 결과 처리 |
| `LedApp_MainFunction10ms()` | LED FSM timer/event 처리 |
| `LedApp_EnableOutput()` | Normal 상태에서 LED output 활성화 |
| `LedApp_DisableOutput()` | LED output 즉시 비활성화 |
| `LedApp_IsOutputEnabled()` | 현재 LED output 활성 상태 반환 |
| `LedApp_RequestSetCurrent()` | current runtime 변경 |
| `LedApp_RequestSetBrightness()` | brightness runtime 변경 |
| `LedApp_RequestSetPort()` | port runtime 변경 |
| `LedApp_RequestOtpValidation()` | Normal 상태에서 OTP 재검증 |
| `LedApp_RequestReadVled()` | 16채널 VLED 비동기 읽기 |
| `LedApp_RequestReadVdif()` | 16채널 VDIF 비동기 읽기 |
| `LedApp_RequestReadIled()` | 16채널 ILED 비동기 읽기 |
| `LedApp_RequestReadTemperature()` | junction temperature 비동기 읽기 |
| `LedApp_RequestFault()` | LED FSM fault event 요청 |
| `LedApp_RequestReset()` | Fault 또는 Normal에서 재초기화 요청 |
| `LedApp_GetState()` | 현재 LED FSM 상태 반환 |
| `LedApp_GetContext()` | read-only LED context 반환 |

## 15. 안전성 및 사용 제약

- FSM state를 강제로 변경할 때 mirror인 `LedApp_Context.state`가 아니라 실제
  `LedApp_Context.fsm.state`를 변경해야 합니다.
- 공개 context 포인터는 `const`이며 외부 모듈에서 수정하지 않습니다.
- 비동기 요청에 전달된 payload는 communication 모듈 내부 storage로
  복사합니다.
- fault 진입 시 output을 OFF하고 pending 설정 상태를 해제합니다.
- OTP programming 도중 PROG/reset timing과 대상 slave address를 변경하지
  않습니다.
- 큐와 timer는 고정 크기 storage를 사용하며 동적 메모리를 사용하지
  않습니다.

## 부록: PC 빌드

PC 예제 Makefile은 생성 RTE에 의존하는 `SWC_KnobLed.c`와 target driver
경로를 직접 사용하지 않고 host simulation 경로로 LED FSM을 검증합니다.

## 부록: Normal 상태 OTP write 테스트

다음 설정을 `1U`로 변경하면 테스트 전용 OTP write 경로가 컴파일됩니다.

```c
#define PRJ_LED_OTP_TEST_WRITE_ENABLE (1U)
```

LED FSM이 `NORMAL`이고 다른 설정 또는 진단 요청이 없을 때
`LedApp_RequestOtpWrite()`를 호출하거나 standalone menu에서 `4`를
입력합니다. 요청은 한 번의 부팅에서 한 번만 허용됩니다.

```text
NORMAL → OTP_RESET → OTP_READY_WAIT → OTP_WRITE → OTP_VALIDITY
       → CONFIG_RESET → CONFIG_READY_WAIT
       → SET_CURRENT → SET_BRIGHT → SET_PORT → NORMAL
```

OTP는 되돌리기 어려우므로 양산 빌드에서는 반드시 옵션을 `0U`로 유지합니다.

## 부록: Open/Short 상태 레지스터 테스트

다음 옵션을 활성화하면 테스트 API와 testcase 명령이 컴파일됩니다.

```c
#define PRJ_LED_OPEN_SHORT_TEST_ENABLE (1U)
```

`LED_STATE_NORMAL`에서 testcase의 `5`를 입력하거나
`LedApp_RequestReadOpenShort()`를 호출합니다.

| E52248 레지스터 | Context 필드 | 대상 채널 |
|---|---|---|
| `REG_LED_OPEN_0_7` (`0xB8`) | `openStatus0To7` | CH0-CH7 Open 상태 |
| `REG_LED_OPEN_8_15` (`0xB9`) | `openStatus8To15` | CH8-CH15 Open 상태 |
| `REG_LED_SHORT_0_7` (`0xBA`) | `shortStatus0To7` | CH0-CH7 Short 상태 |
| `REG_LED_SHORT_8_15` (`0xBB`) | `shortStatus8To15` | CH8-CH15 Short 상태 |

`openShortStatusValid == TRUE`는 네 레지스터를 성공적으로 읽었다는 뜻이며
고장이 없다는 뜻은 아닙니다. 상태 bit `n`은 각 0~7 또는 8~15 channel
group의 해당 채널 검출 결과입니다. 양산 빌드에서는 옵션을 `0U`로 유지합니다.

## 부록: Trace32 확인

```c
LedApp_Context.state
LedApp_Context.event
LedApp_Context.updateMode
LedApp_Context.settingRequestPending
LedApp_Context.outputEnabled
LedApp_Context.otpSlaveAddress
LedApp_Context.otpProgrammingActive
LedApp_Context.otpValidationValid
LedApp_Context.otpValidationIndex
LedApp_Context.otpValidationExpectedValue
LedApp_Context.otpValidationActualValue
LedApp_Context.brightnessValue
LedApp_Context.currentValues
LedApp_Context.diagnosticRequest
LedApp_Context.vledValues
LedApp_Context.vledValuesValid
LedApp_Context.vdifValues
LedApp_Context.vdifValuesValid
LedApp_Context.iledValues
LedApp_Context.iledValuesValid
LedApp_Context.temperatureRaw
LedApp_Context.temperatureCelsius
LedApp_Context.temperatureValid
LedApp_Context.fsm
LedApp_Context.ctrl_ic
```

통신 오류 분석 시 `ctrl_ic`의 `i2c_ch`, `i2c_addr`, `reg_addr`, `dir`,
`comm_sts`, `size_type`, `job_result`, `rx_size`, `tx_size`, `fsm.state` 및
`fsm.lastEvent`를 함께 확인합니다.

