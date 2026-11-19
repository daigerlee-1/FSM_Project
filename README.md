# FSM Project — PC 테스트 전용

공통 원본: main `a1d7f121ee6bfb44d25eb01e3c12555582c0ad05`. 이 브랜치는 main에서 자동 추출했습니다.

플랫폼 선택 매크로와 반대 플랫폼 분기는 제거되어 있습니다. 플랫폼 선택 옵션을 지정하지 않습니다.
기능 변경은 main에서 수행한 뒤 다시 추출합니다. 전용 브랜치 전체를 main에 병합하지 않습니다.
운영 방식 및 갱신 명령은 main의 README와 tools/export_platform.py를 참고합니다.

FSM_Core/tests/run_tests.ps1로 FSM 회귀 테스트를 실행합니다. Storage/drv_nvm_pc.c는 RAM 저장소입니다.
AUTOSAR 전용 SWC, RTE 입출력 어댑터 및 기존 NvM RAM 연계 파일은 제외합니다.
전체 애플리케이션 실행에는 task.h/task.c, drv_console 및 CAN 서비스 대역이 추가로 필요합니다.
LED I2C 대역은 실제 완료 알림이나 IC 프로토콜을 모사하지 않습니다.

기존 모듈 README/WARNING은 검토 이력이며 main의 통합 구성을 기준으로 한 설명이 포함됩니다.
분기 추출은 기존 WARNING의 기능 결함을 자동으로 해결하지 않습니다.
