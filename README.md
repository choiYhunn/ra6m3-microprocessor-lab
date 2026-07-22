# RA6M3 Microprocessor Laboratory

Portfolio extraction from microprocessor laboratory coursework on the Renesas EK-RA6M3. It focuses on application logic that can be reviewed and tested without publishing the Renesas FSP-generated project tree or instructor-provided skeletons.

## Projects

### Elevator controller

A non-blocking five-state controller (`IDLE → MOVING → ARRIVED → DOOR_OPEN → DOOR_CLOSE`) with:

- three-floor request buffering;
- direction-priority scheduling and intermediate-stop insertion;
- time-driven floor estimation;
- explicit arrival-audio completion gating;
- immediate or timed door close behavior.

The original board submission integrated IRQ floor buttons, AGT timekeeping, GPT motor/servo PWM, UART GUI commands, CAN status frames, FND multiplexing, and DAC arrival audio. The public `elevator.c` keeps the scheduling/state logic and replaces peripherals with inputs and observable state.

### Transmission control unit

A four-speed teaching model with P/N/D/R lever states, automatic/manual modes, guarded manual shifts, and separate up/down thresholds to prevent gear hunting around a boundary.

## Verification

The public cores compile as ISO C11 and run host-side assertions:

```bash
mkdir build
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude src/tcu.c tests/test_tcu.c -o build/test_tcu
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude src/elevator.c tests/test_elevator.c -o build/test_elevator
./build/test_tcu
./build/test_elevator
```

## Coursework coverage

The inspected personal submissions also cover external interrupts, AGT/GPT timers, DC/servo motor control, ADC/DAC, and SCI-UART. See [`docs/lab-index.md`](docs/lab-index.md). Weekly `hal_entry.c` files are not copied because they are based on course/FSP skeletons and have no useful standalone build context.

## Publication boundary

Excluded:

- Renesas FSP/RA generated sources, IDE metadata, and board configuration;
- instructor PDFs, examples, installers, drivers, and sample videos;
- personal identifiers embedded in submission filenames;
- raw PCM arrival-sound data;
- code that cannot be separated from a supplied skeleton with confidence.

This repository is a portfolio extraction, not a complete flashable e² studio project. Hardware pin maps and FSP instances must be recreated for the target board.
