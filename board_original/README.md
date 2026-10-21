# board_original — EK-RA6M3 제출 애플리케이션 원본

[상위 README (host 재구성본)](../README.md)

이 폴더는 상위 저장소의 **portable C11 재구성본**(`src/`, `tests/`)이 유래한 **실제 보드 제출 애플리케이션 source**입니다. 재구성본이 보드·FSP 의존성을 제거하고 PC에서 테스트할 수 있게 다시 쓴 코드라면, 여기 파일들은 **EK-RA6M3에 올려 제출했던 원본 모듈**(peripheral 호출 포함)입니다.

## `src/` (재구성)과의 관계

| | 상위 `src/` (재구성) | `board_original/` (제출 원본) |
|---|---|---|
| 목적 | 보드 없이 읽고 `make test` | 실제 보드 제출본의 구조 보존 |
| 의존성 | 없음 (순수 C11) | FSP HAL (`hal_data.h`, `R_*` API), 보드 peripheral |
| 포함 모듈 | `tcu` / `elevator` core만 | `hal_entry` + `tcu`/`elevator` + `motor` + `fnd` |
| 실행 | host assertion test 통과 | **단독 flash 불가** (FSP `ra/`·pin 설정 제외) |

## 구성

- `midterm_tcu/src/` — 중간 프로젝트 TCU: `hal_entry.c`, `tcu.c/.h`, `motor.c/.h`, `fnd.c/.h`
- `final_elevator/src/` — 기말 프로젝트 엘리베이터: `hal_entry.c`, `elevator.c/.h`, `motor.c/.h`, `fnd.c/.h`

`hal_entry.c`는 FSP가 생성하는 main 템플릿(`FSP_CPP_HEADER/FOOTER`, `R_BSP_WarmStart` 선언, `hal_data.h` include)을 기반으로 하며, 그 위의 제어 로직·`motor`·`fnd`·`tcu`/`elevator` 모듈이 직접 작성한 부분입니다.

## 제외

- Renesas FSP 생성 트리(`ra/`, `ra_gen/`), `configuration.xml`, pin 설정, 빌드 산출물, e² studio/IDE 메타데이터 — 그래서 이 폴더만으로는 빌드/flash되지 않습니다.
- `Arrival Sound.c` — 도착 음원 PCM 데이터(대용량·비코드).
- 학번·이름이 들어간 제출 파일명·주석 (확인 결과 app source 본문에는 없음).

## Attribution

개인 수업 제출물에서 직접 작성한 애플리케이션 모듈만 포함합니다. FSP 생성 코드와 강의 제공 템플릿에 대한 새로운 라이선스·사용 권한은 부여하지 않습니다.

---

**English:** The actual EK-RA6M3 submission application source that the repository's portable C11 reconstruction (`src/`, `tests/`) was derived from. Unlike `src/` (dependency-free, host-testable), these are the original board modules with FSP HAL and peripheral calls: `hal_entry.c` plus `tcu`/`elevator`, `motor`, and `fnd`. The FSP-generated trees (`ra/`, `ra_gen/`), pin configuration, IDE metadata, build outputs, and the arrival-sound PCM (`Arrival Sound.c`) are excluded, so this folder is **not flashable on its own**. `hal_entry.c` keeps the FSP main-template scaffolding; the control logic and the `motor`/`fnd`/`tcu`/`elevator` modules are mine. No new license is granted over FSP-generated or course-provided code.
