# FSM Project — MOBILGENE/AUTOSAR 전용

공통 원본: main `a1d7f121ee6bfb44d25eb01e3c12555582c0ad05`. 이 브랜치는 main에서 자동 추출했습니다.

플랫폼 선택 매크로와 반대 플랫폼 분기는 제거되어 있습니다. 플랫폼 선택 옵션을 지정하지 않습니다.
기능 변경은 main에서 수행한 뒤 다시 추출합니다. 전용 브랜치 전체를 main에 병합하지 않습니다.
운영 방식 및 갱신 명령은 main의 README와 tools/export_platform.py를 참고합니다.

타깃 BSW/RTE/OS, MCAL 및 링커 설정을 외부 프로젝트에서 제공해야 합니다.
PC 회귀 테스트와 추출 도구는 이 브랜치에서 제외합니다.

기존 모듈 README/WARNING은 검토 이력이며 main의 통합 구성을 기준으로 한 설명이 포함됩니다.
분기 추출은 기존 WARNING의 기능 결함을 자동으로 해결하지 않습니다.
