# RA6M3 마이크로프로세서 실습 포트폴리오

[한국어](README.md) | [English](README.en.md)

Renesas EK-RA6M3 마이크로프로세서 실습에서 만든 **3층 엘리베이터 제어기**와 **교육용 변속 제어기(TCU)**를 정리했습니다.

보드용 C/FSP 애플리케이션에서 직접 작성한 상태 제어 로직을 분리해 portable C11로 다시 구성했습니다. 상태 전환과 요청 처리는 PC에서 `assert` 테스트로 확인할 수 있습니다. 실제 보드의 IRQ·PWM·CAN 타이밍을 이 테스트가 검증하는 것은 아닙니다.

원래 보드 애플리케이션의 본인 작성 소스는 [`board_original/`](board_original)에 두었고, Renesas FSP 생성 코드·수업 제공 자료·개인정보는 포함하지 않았습니다.

## 파일 안내

- [`src/elevator.c`](src/elevator.c) · [`include/elevator.h`](include/elevator.h): 엘리베이터 상태·스케줄링 core
- [`tests/test_elevator.c`](tests/test_elevator.c): 엘리베이터 전환과 요청 처리 test
- [`src/tcu.c`](src/tcu.c) · [`include/tcu.h`](include/tcu.h): TCU 모드·변속 core
- [`tests/test_tcu.c`](tests/test_tcu.c): 자동·수동 변속 조건 test
- [`docs/lab-index.md`](docs/lab-index.md): 주차별 수업 내용과 공개 처리 범위
- [`board_original/`](board_original): 재구성 전 EK-RA6M3 제출 원본 애플리케이션 source (FSP·음원 제외, flash 불가)
- [`weekly_labs/`](weekly_labs): 주차별 심화 과제 — 수업 skeleton 대비 본인 변경분 patch (인터럽트·타이머·PWM·ADC·UART를 레지스터 수준으로 설정)

## 프로젝트 개요

보드 종속적인 FSP 초기화와 peripheral 호출 사이에 있던 애플리케이션 로직을 추출해, 입력과 시간만으로 상태가 변하는 작은 C 모듈로 재구성했습니다. 이를 통해 실제 보드 없이도 제어 흐름을 읽고 테스트할 수 있으며, 공개하면 안 되는 생성 코드와 수업 자료는 제외했습니다.

```mermaid
flowchart LR
    I["Input / event / elapsed time"] --> C["Portable C11 control core"]
    C --> S["Observable state and command"]
    H["Host assertion tests"] --> C
    C -. "Board integration boundary" .-> P["IRQ · AGT/GPT · PWM · UART · CAN · DAC"]
```

## 엘리베이터 제어기

### 상태 구조

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> MOVING: pending request
    MOVING --> ARRIVED: target floor reached
    ARRIVED --> DOOR_OPEN: arrival audio completed
    DOOR_OPEN --> DOOR_CLOSE: close request or timeout
    DOOR_CLOSE --> IDLE: no pending request
    DOOR_CLOSE --> MOVING: next request selected
```

### 구현 내용

- `IDLE → MOVING → ARRIVED → DOOR_OPEN → DOOR_CLOSE`의 5상태 non-blocking 제어
- 3개 층의 요청을 `pending` buffer에 보관
- 현재 진행 방향의 중간층 요청을 우선 처리하는 scheduling
- 경과 시간으로 현재 층과 도착 조건을 갱신
- 도착 음원 재생 완료를 확인한 뒤 문 열림 상태로 전환
- 즉시 닫기 요청 또는 timer 만료에 따른 문 닫힘 처리

원래 보드 제출물은 층 버튼 IRQ, AGT 시간 관리, GPT motor/servo PWM, UART GUI 명령, CAN 상태 frame, FND multiplexing과 DAC 도착 음원을 연결했습니다. 공개 core에서는 peripheral을 입력과 관측 가능한 상태로 치환했습니다.

## 교육용 TCU

### 제어 흐름

```mermaid
flowchart TD
    A["Lever P / N / D / R"] --> B{"Drive mode"}
    B -- "Automatic" --> C["Throttle-based shift policy"]
    B -- "Manual" --> D["Guarded up/down request"]
    C --> E["Gear 1–4"]
    D --> E
```

`engine_error`는 원래 보드에서 CDS 센서 값으로 설정되어 FND에 `E`를 표시하고 UART 진단 문자열을 보내던 상태 플래그입니다. 공개 core에서는 상태 필드만 유지하며, 이 플래그가 변속을 막는 보호 로직은 구현되어 있지 않습니다.

### 구현 내용

- P/N/D/R lever 상태와 automatic/manual mode 분리
- 1–4단 gear 상태 관리
- throttle 조건을 이용한 자동 upshift/downshift
- 경계 부근 gear hunting을 줄이기 위한 서로 다른 상승·하강 threshold
- 허용 범위를 벗어난 수동 변속 요청 차단
- 초기화, lever 순환, mode 전환과 변속 정책을 작은 함수로 분리

## 작성·재구성한 부분

- MCU 수업에서 작성한 엘리베이터 scheduling과 TCU 변속 로직을 공개 가능한 core로 분리
- 보드의 1 ms timer 카운터에 묶여 있던 경과 시간 판단을 `tick(elapsed_ms)` 입력으로 옮겨 PC에서 재현 가능하게 함
- FSP·보드 peripheral 의존성을 입력, 상태와 함수 경계로 추상화
- 정상 전환뿐 아니라 중간층 요청, mode 변경, 잘못된 입력과 변속 경계를 assertion으로 검증
- 원본 제출물에 포함된 생성 코드·강의 skeleton과 개인정보를 공개 대상에서 제외

이 저장소는 수업 전체 프로젝트나 flash 가능한 e² studio workspace의 복사본이 아니라, 직접 구현한 애플리케이션 제어 로직을 검토 가능하게 추출한 포트폴리오입니다.

## 주요 문제와 해결

| 문제 | 해결 | 검증 |
|---|---|---|
| 보드 timer에 묶인 상태 전환은 보드 없이 재현하기 어려움 | 경과 시간을 입력으로 받는 non-blocking `tick` 구조 | 여러 시간 단계를 순서대로 입력해 상태 전환 확인 |
| 이동 중 추가 요청 처리 순서가 불명확 | `pending` buffer와 방향 우선 scheduling 적용 | 이동 방향의 중간층 정차와 남은 요청 처리 test |
| 도착과 문 동작이 동시에 실행될 수 있음 | `ARRIVED`에서 음원 완료를 명시적으로 기다림 | audio 완료 전후 상태를 분리해 assertion |
| 자동 변속 경계에서 gear가 반복 변경될 수 있음 | up/down threshold를 분리 | 경계 throttle을 반복 입력해 gear 유지 확인 |
| 보드 코드가 없으면 로직을 검토하기 어려움 | peripheral 호출을 입력·출력 인터페이스로 치환 | host compiler의 엄격한 warning 옵션으로 build |

## 빌드 및 테스트

Linux/macOS 또는 GCC/Clang이 설치된 환경에서 `make test`로 두 테스트를 빌드·실행할 수 있습니다(`make clean`으로 정리, `make test CC=clang`으로 컴파일러 변경). Make 없이 직접 실행하려면 다음과 같습니다.

```bash
mkdir -p build

gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Iinclude src/tcu.c tests/test_tcu.c \
  -o build/test_tcu

gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Iinclude src/elevator.c tests/test_elevator.c \
  -o build/test_elevator

./build/test_tcu
./build/test_elevator
```

테스트는 외부 framework 없이 표준 C `assert`를 사용합니다. 성공하면 각각 `TCU tests passed`, `Elevator tests passed`를 출력하고 종료 코드 0을 반환합니다. (2026-10-20 GCC 13 및 `-fsanitize=address,undefined` 빌드에서 통과 확인)

### 무엇이 보드에서, 무엇이 host에서 검증되었나

| 범위 | 원래 EK-RA6M3 제출물 | 이 저장소 (host) |
|---|---|---|
| 엘리베이터 상태 전환·요청 스케줄링 | 보드용으로 구현·빌드·제출 | C11 core + assertion test |
| TCU 모드·자동/수동 변속 정책 | 보드용으로 구현·빌드·제출 | C11 core + assertion test |
| IRQ, AGT/GPT timer, PWM, ADC, UART, CAN, DAC, FND | 보드에서 사용 | 포함하지 않음 (검증 범위 밖) |
| 시나리오별 통과 기록 | 빌드 산출물은 있으나 시나리오별 통과 로그는 남아 있지 않음 | — |

## 수업 범위

| 구분 | 확인된 주제 | 공개 처리 |
|---|---|---|
| 3주차 | 외부 interrupt (NVIC·ICU·PFS register) | [본인 변경분 patch](weekly_labs/a4_interrupt.patch) |
| 4–5주차 | AGT 타이머 상태기계, GPT 서보 PWM | [patch](weekly_labs/a5_agt_timer.patch) · [patch](weekly_labs/a6_gpt_servo_pwm.patch) |
| 6–7주차 | ADC 입력 → 서보·모터 PWM, SCI-UART | [patch](weekly_labs/a7_adc_motor_servo.patch) · [patch](weekly_labs/a8_uart_quiz.patch) |
| 중간 프로젝트 | TCU 상태·자동/수동 변속·출력 | portable TCU core 공개 |
| 기말 프로젝트 | 엘리베이터 scheduling·motor/door·통신·음원 | portable elevator core 공개 |

상세 목록은 [`docs/lab-index.md`](docs/lab-index.md)에 정리했습니다.

## 저장소 구조

```text
.
├─ include/
│  ├─ elevator.h
│  └─ tcu.h
├─ src/
│  ├─ elevator.c
│  └─ tcu.c
├─ tests/
│  ├─ test_elevator.c
│  └─ test_tcu.c
├─ docs/
│  └─ lab-index.md
├─ board_original/            # EK-RA6M3 제출 원본 app source (FSP·음원 제외)
│  ├─ midterm_tcu/src/        # hal_entry·tcu·motor·fnd
│  └─ final_elevator/src/     # hal_entry·elevator·motor·fnd
├─ weekly_labs/               # 주차별 과제: skeleton 대비 본인 변경분 (.patch 5개)
├─ README.md
└─ README.en.md
```

## 한계와 공개 범위

- 이 저장소만으로 RA6M3 보드에 flash할 수 없습니다.
- hardware pin map, FSP instance와 peripheral adapter를 대상 보드에 맞게 다시 구성해야 합니다.
- 실제 IRQ timing, PWM 파형, CAN bus와 actuator 응답은 host test의 검증 범위가 아닙니다.
- Renesas FSP/RA 생성 source, IDE metadata, 수업 PDF·예제·설치 파일과 sample video를 포함하지 않습니다.
- 개인 식별정보가 포함된 제출 파일명과 원본 PCM 음원을 제외했습니다.

## 출처와 공개 범위

이 저장소의 공개 core와 test는 개인 수업 제출물에서 직접 구현한 애플리케이션 로직을 분리한 것입니다. Renesas FSP 생성 코드와 강의 제공 skeleton은 포함하지 않으며, 해당 자료에 대한 새로운 라이선스나 사용 권한을 부여하지 않습니다.
