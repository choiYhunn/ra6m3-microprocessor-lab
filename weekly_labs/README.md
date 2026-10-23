# 주차별 심화 과제 — 수업 skeleton 대비 본인 변경분

[상위 README](../README.md)

마이크로프로세서실험(EK-RA6M3, Cortex-M4)의 주차별 **심화 과제 제출본**을 수업이 배포한 skeleton과 비교한 결과입니다. 각 `.patch`는 `skeleton/src/hal_entry.c` → `submitted/src/hal_entry.c`의 unified diff이고, `+` 줄이 제가 작성한 코드입니다. skeleton 본문과 FSP 생성 코드는 수업 자료라 싣지 않았습니다.

| 주차 | 주제 | patch | 내가 쓴 줄 | skeleton 크기 |
|---|---|---|---:|---:|
| 3 | 외부 인터럽트 (register 수준) | [`a4_interrupt.patch`](a4_interrupt.patch) | +36 | 192줄 |
| 4 | AGT 타이머 콜백 상태기계 | [`a5_agt_timer.patch`](a5_agt_timer.patch) | +73 | 109줄 |
| 5 | GPT 서보 PWM, AGT register 설정 | [`a6_gpt_servo_pwm.patch`](a6_gpt_servo_pwm.patch) | +50 / −4 | 284줄 |
| 6 | ADC 입력 → 서보 각도·모터 PWM | [`a7_adc_motor_servo.patch`](a7_adc_motor_servo.patch) | +37 / −3 | 247줄 |
| 7 | SCI-UART 단어 입력 퀴즈 | [`a8_uart_quiz.patch`](a8_uart_quiz.patch) | +25 / −6 | 220줄 |

## 주차별 내용

### 3주차 — 외부 인터럽트를 레지스터로 직접 연결
FSP API 대신 레지스터로 인터럽트 경로를 구성했습니다.
- `NVIC->ISER[0]`로 IRQ 0–3 허용
- `R_ICU->IELSR_b[n].IELS`로 외부 핀 IRQ11–14를 NVIC 슬롯 0–3에 연결
- `R_PMISC->PWPR`로 PFS 쓰기 보호를 풀고 `PmnPFS_HA_b.ISEL`로 P006/P008/P009/P010을 IRQ 입력으로 설정한 뒤 다시 잠금
- 각 ISR에서 `IELSR.IR` 플래그를 지우고 두 카운터, 곱셈 결과, 초기화를 4자리 FND에 표시

### 4주차 — AGT 타이머 콜백 상태기계
- AGT 주기 콜백으로 LED 4개를 순서대로 켜고, 한 바퀴가 끝날 때마다 `cycle_count`를 올림. 10회가 되면 타이머 정지
- IRQ11 버튼으로 실행/일시정지 토글(`R_AGT_Stop`/`R_AGT_Start`)
- 일시정지 중에는 메인 루프에서 FND 4자리를 2 ms 간격으로 멀티플렉싱해 횟수 표시 (`FND_DisplayNum`)

### 5주차 — GPT 서보 PWM과 AGT를 레지스터로 설정
- `R_GPT0->GTIOR`(compare-match 출력, `OAE`)과 `GTCCR`로 20 ms 주기 서보 PWM을 만들고 `GTCR.CST`로 시작
- `R_AGT1->AGTMR1/AGTMR2`, reload 값 6553으로 AGT1을 직접 설정하고 underflow(`TUNDF`)마다 각도를 한 단계씩 변경
- 각도 0–180°를 duty 3–12%로 대응(`duty = 0.03 + 0.09 × angle / 180`), 끝에 닿으면 방향을 바꾸고 0°로 돌아오면 정지·LED 소등
- IRQ 핀(PFS), `IELSR`, `NVIC->ISER`, `NVIC->IP` 우선순위를 레지스터로 설정

### 6주차 — ADC 입력으로 서보와 모터 제어
- 가변저항 전압을 각도로 변환(`degree = 180 − mV × 180 / 3300`)해 서보 회전
- 조도 센서가 어두움이면 모터 정지
- 가속 입력은 루프 5회마다, 브레이크는 4회마다 속도 단계를 ±1(0–20), `R_GPT3->GTCCR`로 duty 반영 (`Update_Motor_PWM`)

### 7주차 — SCI-UART 단어 입력 퀴즈
- 버튼으로 퀴즈 시작: 점수 초기화, 단어 목록에서 무작위 단어를 UART로 전송
- 수신 콜백에서 Enter 시 입력과 비교해 정답 수·문항 수를 세고 다음 단어 전송, Backspace 시 버퍼에서 한 글자 지우고 `" \b"`를 다시 보내 터미널 표시를 맞춤
- 단어 목록 `vocabulary.h`는 수업 제공 파일이라 싣지 않았습니다

## 검증 범위

| 항목 | 상태 |
|---|---|
| 보드 실행 | 제출·채점됨(과목 A+). 주차별 실행 로그·영상은 남아 있지 않음 |
| host 빌드·테스트 | 해당 없음 — patch만으로는 빌드되지 않음(skeleton·FSP 미포함) |
| 저작 경계 | skeleton과 줄 단위로 비교. `+` 줄만 본인 작성 |

중간·기말 프로젝트(TCU, 엘리베이터)는 [`../board_original/`](../board_original)과 portable C11 재구성본([`../src/`](../src))에 있습니다.

---

**English:** Five weekly advanced assignments from the EK-RA6M3 microprocessor lab, published as unified diffs against the instructor-provided skeleton so that only my own lines (`+`) are shown. Week 3 wires external interrupts by register (NVIC ISER, ICU IELSR event links, PFS ISEL with PWPR write-protect handling); week 4 is an AGT-callback LED/FND state machine with IRQ pause/resume; week 5 configures GPT0 servo PWM (GTIOR/GTCCR, 20 ms period, 3–12 % duty for 0–180°) and AGT1 directly by register; week 6 maps ADC readings to servo angle and stepped motor PWM; week 7 is a UART word-typing quiz with backspace handling. These were submitted and graded on the board; no per-week run logs survive, and the patches do not build on their own because the skeleton and FSP files are not included.
