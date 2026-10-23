# Laboratory index

| Submission | Topic evidenced in the personal source | Public treatment |
|---|---|---|
| Week 3 | External interrupts wired by register (NVIC, ICU, PFS) | [`a4_interrupt.patch`](../weekly_labs/a4_interrupt.patch) |
| Week 4 | AGT timer callback state machine | [`a5_agt_timer.patch`](../weekly_labs/a5_agt_timer.patch) |
| Week 5 | GPT servo PWM and AGT configured by register | [`a6_gpt_servo_pwm.patch`](../weekly_labs/a6_gpt_servo_pwm.patch) |
| Week 6 | ADC input mapped to servo angle and motor PWM | [`a7_adc_motor_servo.patch`](../weekly_labs/a7_adc_motor_servo.patch) |
| Week 7 | SCI-UART word quiz with backspace handling | [`a8_uart_quiz.patch`](../weekly_labs/a8_uart_quiz.patch) |
| Midterm | TCU state, manual/automatic shift logic, FND and motor output | Portable TCU core + [`board_original/midterm_tcu`](../board_original/midterm_tcu) |
| Final | Elevator scheduler, motor/door control, UART, CAN, FND, DAC audio | Portable elevator core + [`board_original/final_elevator`](../board_original/final_elevator) |

Weekly entries are diffs against the instructor-provided skeleton; only `+` lines are mine (see [`weekly_labs/README.md`](../weekly_labs/README.md)).

The original submissions target `board.ra6m3ek` / `R7FA6M3AH3CFC`. Generated board support and configuration are intentionally omitted.
