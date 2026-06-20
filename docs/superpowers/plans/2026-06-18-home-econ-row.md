# 홈 화면 "다음 중요도3 경제이벤트" 행 — 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 홈(메인) 화면의 주식 정보행과 온/습도 strip 사이에, 현재 시각 다음으로 발생할 중요도 3(HIGH) 경제 캘린더 이벤트 한 건을 표시하고, 시간이 지나면 자동으로 다음 이벤트로 전진시킨다.

**Architecture:** econ-calendar-week-paging 브랜치의 **데이터 계층만**(model/parse/service + 호스트 테스트) 이식하고, 순수 헬퍼 2개(`econ_next_after`, `econ_when_label`)를 추가한다. 저우선 `EconTask`가 HIGH 이벤트를 시간당 fetch해 PSRAM 캐시에 저장하고, 기존 홈 틱(15초)이 캐시에서 다음 이벤트를 재계산해 `ui_home`의 2줄 행(캡션+굵은 이벤트명)에 그린다. 네트워크는 절대 응답성 태스크(StockTask)에서 돌지 않는다.

**Tech Stack:** C11, ESP-IDF v6.0.1, LVGL, 벤더링된 cJSON, 호스트 단위테스트(CMake/ctest), 데스크톱 시뮬레이터(libcurl).

## Global Constraints

- ESP-IDF 명령 전 매 셸에서 `source "/Users/mimi/.espressif/tools/activate_idf_v6.0.1.sh"` 선행. `idf.py`는 저장소(워크트리) 루트 `/Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock` 에서 실행.
- 호스트 단위테스트는 **ASan 금지(샌드박스에서 hang) — 기본 빌드(SANITIZE OFF)로 실행**. 필요 시 UBSan만.
- 1-bpp 커스텀 폰트(`ui_font_black_26`)·`lv_font_montserrat_14`에 `★`/`·` 글리프가 없을 수 있으므로 화면 문자열은 **ASCII만** 사용(중요도 마커 `***`, 구분자 공백).
- 홈 행은 **HIGH(3) 전용** — `econ_service_fetch` 호출 시 `ECON_IMPACT_HIGH`를 인자로 전달. `STOCK_ECON_MIN_IMPACT` Kconfig는 도입하지 않는다(가져오지 않는 캘린더 화면 전용).
- 이식 소스는 `origin/econ-calendar-week-paging` 의 해당 파일을 **그대로** 사용(아래 각 태스크에 전문 수록).
- LVGL float printf는 펌웨어 설정에서 비활성 → 표시 텍스트는 `lv_label_set_text_fmt` 대신 `snprintf` 사용(기존 코드 관례).

---

### Task 1: 공유 텍스트 헬퍼 추출 (`stock_text.h`)

`econ_parse.c`가 `copy_cstr`/`to_ascii`를 공유 헤더 `stock_text.h`에서 가져오므로, 먼저 현재 `stock_parse.c`에 인라인된 두 함수를 헤더로 추출한다(econ 브랜치와 동일한 설계).

**Files:**
- Create: `components/stock_core/include/stock_text.h`
- Modify: `components/stock_core/stock_parse.c` (인라인 `copy_cstr`/`append`/`to_ascii` 제거, include 추가)
- Test: `components/stock_core/test/host/test_parse.c` (기존, 변경 없음 — 회귀 확인용)

**Interfaces:**
- Produces: `static void copy_cstr(char *dst, size_t cap, const char *src);`,
  `static void to_ascii(char *dst, size_t cap, const char *src);` (헤더-온리, static)

- [ ] **Step 1: `stock_text.h` 생성**

`components/stock_core/include/stock_text.h`:

```c
/*
 * stock_text.h — tiny shared text helpers for the parsers.
 *
 * Header-only (static) so the firmware, simulator and host tests all compile
 * byte-identical copies without extra build wiring. Used by stock_parse.c and
 * econ_parse.c so API free-text is folded to ASCII in exactly one place — the
 * built-in mono font has no curly quotes / dashes / accented letters, so any
 * raw UTF-8 would otherwise render as tofu boxes.
 */
#pragma once

#include <stddef.h>
#include <string.h>

/* Bounded copy + NUL-terminate (truncates to fit `cap`). */
static void copy_cstr(char *dst, size_t cap, const char *src) {
    if (src) {
        strncpy(dst, src, cap - 1);
        dst[cap - 1] = '\0';
    } else {
        dst[0] = '\0';
    }
}

static void append(char *dst, size_t cap, size_t *j, const char *s) {
    for (; *s && *j < cap - 1; s++) dst[(*j)++] = *s;
}

/* Fold a UTF-8 string down to ASCII: common typographic punctuation is mapped
 * to its ASCII equivalent; any other non-ASCII byte sequence is dropped. */
static void to_ascii(char *dst, size_t cap, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j < cap - 1; ) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {                       /* plain ASCII */
            dst[j++] = (char)c; i++;
        } else if (c == 0xE2 && (unsigned char)src[i + 1] == 0x80
                             && src[i + 2] != '\0') {   /* complete 3-byte seq only */
            switch ((unsigned char)src[i + 2]) {   /* General Punctuation */
                case 0x98: case 0x99: append(dst, cap, &j, "'");   break;
                case 0x9C: case 0x9D: append(dst, cap, &j, "\"");  break;
                case 0x93: case 0x94: append(dst, cap, &j, "-");   break;
                case 0xA6:            append(dst, cap, &j, "...");  break;
                default: break;                /* drop other punctuation */
            }
            i += 3;
        } else {                              /* drop any other UTF-8 char */
            i++;
            while (((unsigned char)src[i] & 0xC0) == 0x80) i++;
        }
    }
    dst[j] = '\0';
}
```

- [ ] **Step 2: `stock_parse.c` 리팩터**

`components/stock_core/stock_parse.c` 상단 include에 추가(`#include "stock_parse.h"` 다음 줄):

```c
#include "stock_text.h"   /* copy_cstr / to_ascii (shared with econ_parse.c) */
```

그리고 인라인 정의 블록을 삭제한다. 현재 파일의 다음 블록(라인 ~43–94)을 제거:
- `static void copy_cstr(...) { ... }` 함수 전체
- `static void append(...) { ... }` 함수 전체
- `static void to_ascii(...) { ... }` 함수 전체

`copy_str`(아래)는 **남겨둔다** — 이건 cJSON 래퍼라 stock_parse.c 고유:

```c
static void copy_str(char *dst, size_t cap, const cJSON *item) {
    copy_cstr(dst, cap, cJSON_IsString(item) ? item->valuestring : NULL);
}
```

- [ ] **Step 3: 호스트 테스트로 회귀 확인**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock/components/stock_core/test/host
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: `parse` 와 `service` 테스트 PASS (기존 동작 유지 — `to_ascii`/`copy_cstr`가 헤더에서 동일하게 동작).

- [ ] **Step 4: 커밋**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
git add components/stock_core/include/stock_text.h components/stock_core/stock_parse.c
git commit -m "refactor(stock_core): extract shared text helpers into stock_text.h"
```

---

### Task 2: econ 데이터 모델 + 파서 이식 + 호스트 테스트

순수 데이터 계층(네트워크 없음)과 그 호스트 테스트를 이식한다.

**Files:**
- Create: `components/stock_core/include/econ_model.h`
- Create: `components/stock_core/include/econ_parse.h`
- Create: `components/stock_core/econ_parse.c`
- Create: `components/stock_core/test/host/test_econ.c`
- Create: `components/stock_core/test/host/fixtures/fmp_econ.json`
- Modify: `components/stock_core/test/host/CMakeLists.txt` (test_econ 타깃 추가)

**Interfaces:**
- Produces: `econ_event_t`, `econ_calendar_t`, `econ_impact_t`(NONE=0/LOW=1/MEDIUM=2/HIGH=3),
  `econ_page_count(int)`; 파서 `int econ_parse_calendar(const char*, long tz_off, int min_impact, econ_calendar_t*)`,
  `int64_t econ_ymd_to_epoch(int,int,int,int,int,int)`, `int econ_impact_from_str(const char*)`,
  `long econ_local_tz_off(time_t)`, `void econ_week_range(...)`, `void econ_month_week_span(...)`.

- [ ] **Step 1: `econ_model.h` 생성** — `git show origin/econ-calendar-week-paging:components/stock_core/include/econ_model.h` 의 내용을 그대로 작성. (필드: `when[12]`, `country[6]`, `event[40]`, `estimate/actual/previous[12]`, `int impact`, `int64_t ts`; 컨테이너: `count`, `total_matched`, `items[64]`, `week_label[24]`, `valid`, `error[96]`.)

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
git show origin/econ-calendar-week-paging:components/stock_core/include/econ_model.h > components/stock_core/include/econ_model.h
```

- [ ] **Step 2: `econ_parse.h` 생성** (verbatim)

```bash
git show origin/econ-calendar-week-paging:components/stock_core/include/econ_parse.h > components/stock_core/include/econ_parse.h
```

- [ ] **Step 3: `econ_parse.c` 생성** (verbatim)

```bash
git show origin/econ-calendar-week-paging:components/stock_core/econ_parse.c > components/stock_core/econ_parse.c
```

- [ ] **Step 4: 픽스처 + 호스트 테스트 생성** (verbatim)

```bash
git show origin/econ-calendar-week-paging:components/stock_core/test/host/fixtures/fmp_econ.json > components/stock_core/test/host/fixtures/fmp_econ.json
git show origin/econ-calendar-week-paging:components/stock_core/test/host/test_econ.c > components/stock_core/test/host/test_econ.c
```

- [ ] **Step 5: `test/host/CMakeLists.txt` 에 test_econ 타깃 추가**

`enable_testing()` 줄 **앞에** 다음 블록을 삽입:

```cmake
# economic-calendar parse/time helpers
add_executable(test_econ
    test_econ.c
    ${CORE}/econ_parse.c
    ${CJSON}/cJSON.c)

target_include_directories(test_econ PRIVATE
    ${CORE}/include
    ${CJSON})

target_compile_definitions(test_econ PRIVATE FIXDIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_options(test_econ PRIVATE -Wall -Wextra -O0 -g)
target_link_libraries(test_econ m)

if(SANITIZE)
    target_compile_options(test_econ PRIVATE -fsanitize=address,undefined)
    target_link_options(test_econ PRIVATE -fsanitize=address,undefined)
endif()
```

그리고 파일 끝 `add_test(NAME service ...)` 줄 **뒤에** 추가:

```cmake
add_test(NAME econ COMMAND test_econ)
```

- [ ] **Step 6: 빌드 & 실행**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock/components/stock_core/test/host
rm -rf build && cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: `econ` 테스트 PASS — `test_econ` 출력에 `OK  (... checks, 0 failed)` (test_ymd_to_epoch, test_week_range, test_month_week_span, test_parse_high_only 등 통과).

- [ ] **Step 7: 커밋**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
git add components/stock_core/include/econ_model.h components/stock_core/include/econ_parse.h \
        components/stock_core/econ_parse.c components/stock_core/test/host/test_econ.c \
        components/stock_core/test/host/fixtures/fmp_econ.json \
        components/stock_core/test/host/CMakeLists.txt
git commit -m "feat(stock_core): port economic-calendar data model + parser (host-tested)"
```

---

### Task 3: 순수 헬퍼 `econ_next_after` + `econ_when_label` (TDD)

홈 행 로직을 받치는 두 순수 함수. 먼저 실패 테스트를 쓰고 구현한다.

**Files:**
- Modify: `components/stock_core/include/econ_parse.h` (선언 추가)
- Modify: `components/stock_core/econ_parse.c` (구현 추가, `<ctype.h>` include)
- Test: `components/stock_core/test/host/test_econ.c` (테스트 추가)

**Interfaces:**
- Consumes: `econ_calendar_t`, `econ_ymd_to_epoch`, `econ_parse_calendar` (Task 2)
- Produces:
  - `int econ_next_after(const econ_calendar_t *cal, int64_t now_utc);`
    → `cal->items`(ts 오름차순)에서 `ts > now_utc`인 첫 인덱스, 없거나 `!cal->valid`면 -1.
  - `void econ_when_label(int64_t ts, time_t now, long tz_off, char *out, size_t n);`
    → 디바이스-로컬 상대일 라벨: `"TODAY HH:MM"` / `"TOMORROW HH:MM"` / `"<WD> HH:MM"`(2–6일 뒤, WD=MON..SUN) / `"MM-DD HH:MM"`(그 외). 출력 최대 `"TOMORROW HH:MM"`=14자 → 버퍼는 16바이트 이상.

- [ ] **Step 1: 실패 테스트 작성**

`components/stock_core/test/host/test_econ.c` 의 다른 `static void test_*` 함수들 뒤, `int main(void)` **앞에** 추가:

```c
static void test_next_after(void) {
    printf("test_next_after\n");
    char *j = slurp("fmp_econ.json");
    econ_calendar_t c;
    /* HIGH-only -> CPI(06-15 12:30), BoJ(06-17 03:00), Fed(06-18 14:00), sorted. */
    CHECK(econ_parse_calendar(j, 0, ECON_IMPACT_HIGH, &c) == 0);
    free(j);

    time_t tue = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);  /* Tue noon UTC */
    int idx = econ_next_after(&c, (int64_t)tue);
    CHECK(idx == 1);                              /* CPI is past -> BoJ is next */
    CHECK_STR(c.items[idx].country, "JP");

    /* before everything -> first event */
    CHECK(econ_next_after(&c, 0) == 0);
    /* after everything -> none */
    time_t far = (time_t)econ_ymd_to_epoch(2030, 1, 1, 0, 0, 0);
    CHECK(econ_next_after(&c, (int64_t)far) == -1);

    /* empty / invalid calendar -> -1 */
    econ_calendar_t empty;
    CHECK(econ_parse_calendar("[]", 0, ECON_IMPACT_HIGH, &empty) == 0);
    CHECK(econ_next_after(&empty, 0) == -1);
}

static void test_when_label(void) {
    printf("test_when_label\n");
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);  /* Tue */
    char buf[16];

    econ_when_label(econ_ymd_to_epoch(2026, 6, 16, 18, 0, 0), now, 0, buf, sizeof buf);
    CHECK_STR(buf, "TODAY 18:00");
    econ_when_label(econ_ymd_to_epoch(2026, 6, 17, 9, 0, 0), now, 0, buf, sizeof buf);
    CHECK_STR(buf, "TOMORROW 09:00");
    econ_when_label(econ_ymd_to_epoch(2026, 6, 19, 14, 0, 0), now, 0, buf, sizeof buf);
    CHECK_STR(buf, "FRI 14:00");                  /* 06-19 is a Friday */
    econ_when_label(econ_ymd_to_epoch(2026, 6, 26, 8, 30, 0), now, 0, buf, sizeof buf);
    CHECK_STR(buf, "06-26 08:30");                /* >6 days -> date */

    /* tz shift rolls the event into the next local day */
    econ_when_label(econ_ymd_to_epoch(2026, 6, 16, 16, 0, 0), now, KST, buf, sizeof buf);
    CHECK_STR(buf, "TOMORROW 01:00");             /* 16:00 UTC = 01:00 KST next day */
}
```

`int main(void)` 안, 기존 호출들 뒤에 추가:

```c
    test_next_after();
    test_when_label();
```

- [ ] **Step 2: 실패 확인**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock/components/stock_core/test/host
cmake --build build 2>&1 | head -20
```

Expected: 링크 실패 — `undefined reference to 'econ_next_after'` / `'econ_when_label'`.

- [ ] **Step 3: 선언 추가**

`components/stock_core/include/econ_parse.h` 의 마지막 함수 선언(`econ_parse_calendar(...)`) 뒤, `#ifdef __cplusplus` 닫기 **앞에** 추가:

```c
/* Index of the first event with ts > now_utc (items are ascending). Returns -1
 * when none are still upcoming or the calendar is invalid. */
int econ_next_after(const econ_calendar_t *cal, int64_t now_utc);

/* Device-local relative-day label for `ts`: "TODAY HH:MM" / "TOMORROW HH:MM" /
 * "<WD> HH:MM" (2..6 days out) / "MM-DD HH:MM". `out` needs >= 16 bytes. */
void econ_when_label(int64_t ts, time_t now, long tz_off, char *out, size_t n);
```

- [ ] **Step 4: 구현 추가**

`components/stock_core/econ_parse.c` 의 include 목록에 추가(`#include <strings.h>` 뒤):

```c
#include <ctype.h>     /* toupper */
```

파일 맨 끝(마지막 함수 `econ_parse_calendar` 뒤)에 추가:

```c
int econ_next_after(const econ_calendar_t *cal, int64_t now_utc) {
    if (!cal || !cal->valid) return -1;
    for (int i = 0; i < cal->count; i++)
        if (cal->items[i].ts > now_utc) return i;
    return -1;
}

void econ_when_label(int64_t ts, time_t now, long tz_off, char *out, size_t n) {
    if (!out || n == 0) return;
    time_t ev_local  = (time_t)(ts  + tz_off);
    time_t now_local = (time_t)(now + tz_off);
    struct tm evt, nwt;
    gmtime_r(&ev_local,  &evt);
    gmtime_r(&now_local, &nwt);

    int64_t ev_day  = days_from_civil(evt.tm_year + 1900, evt.tm_mon + 1, evt.tm_mday);
    int64_t now_day = days_from_civil(nwt.tm_year + 1900, nwt.tm_mon + 1, nwt.tm_mday);
    int64_t d = ev_day - now_day;

    char hm[8];
    strftime(hm, sizeof hm, "%H:%M", &evt);

    if (d == 0) {
        snprintf(out, n, "TODAY %s", hm);
    } else if (d == 1) {
        snprintf(out, n, "TOMORROW %s", hm);
    } else if (d >= 2 && d <= 6) {
        char wd[8];
        strftime(wd, sizeof wd, "%a", &evt);          /* "Mon".."Sun" */
        for (char *p = wd; *p; ++p) *p = (char)toupper((unsigned char)*p);
        snprintf(out, n, "%s %s", wd, hm);
    } else {
        char md[8];
        strftime(md, sizeof md, "%m-%d", &evt);
        snprintf(out, n, "%s %s", md, hm);
    }
}
```

(`days_from_civil` 는 같은 파일 상단의 static 함수 — 그대로 재사용한다.)

- [ ] **Step 5: 통과 확인**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock/components/stock_core/test/host
cmake --build build && ctest --test-dir build --output-on-failure -R econ
```

Expected: `econ` PASS — `test_next_after`, `test_when_label` 포함 `0 failed`.

- [ ] **Step 6: 커밋**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
git add components/stock_core/include/econ_parse.h components/stock_core/econ_parse.c \
        components/stock_core/test/host/test_econ.c
git commit -m "feat(stock_core): add econ_next_after + econ_when_label helpers"
```

---

### Task 4: econ_service 이식 + Kconfig + 컴포넌트 등록 + 펌웨어 빌드

네트워크 fetch 계층과 그 호스트 테스트(가짜 http_get)를 이식하고, 컴포넌트/Kconfig에 등록해 펌웨어가 빌드되게 한다.

**Files:**
- Create: `components/stock_core/include/econ_service.h`
- Create: `components/stock_core/econ_service.c`
- Create: `components/stock_core/test/host/test_econ_service.c`
- Modify: `components/stock_core/test/host/CMakeLists.txt` (test_econ_service 타깃)
- Modify: `components/stock_core/CMakeLists.txt` (econ_parse.c + econ_service.c 등록)
- Modify: `components/stock_core/Kconfig.projbuild` (STOCK_FMP_API_KEY + STOCK_ECON_BASE_URL)

**Interfaces:**
- Consumes: `econ_parse.*`(Task 2/3), `http_get`(http_port.h)
- Produces: `int econ_service_fetch(const char *fmp_key, time_t now_utc, long tz_off, int week_offset, int min_impact, econ_calendar_t *out);`
  → 성공 1, 실패 0. 실패 시 `out->valid=false` + `out->error` 설정, `out->week_label` 항상 설정.
- Produces (Kconfig): `CONFIG_STOCK_FMP_API_KEY`, `CONFIG_STOCK_ECON_BASE_URL`.

- [ ] **Step 1: econ_service 헤더/소스/테스트 생성** (verbatim)

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
git show origin/econ-calendar-week-paging:components/stock_core/include/econ_service.h > components/stock_core/include/econ_service.h
git show origin/econ-calendar-week-paging:components/stock_core/econ_service.c > components/stock_core/econ_service.c
git show origin/econ-calendar-week-paging:components/stock_core/test/host/test_econ_service.c > components/stock_core/test/host/test_econ_service.c
```

- [ ] **Step 2: `test/host/CMakeLists.txt` 에 test_econ_service 타깃 추가**

`enable_testing()` **앞에** 삽입:

```cmake
# econ_service tests: link a fake http_get (defined in test_econ_service.c).
add_executable(test_econ_service
    test_econ_service.c
    ${CORE}/econ_service.c
    ${CORE}/econ_parse.c
    ${CJSON}/cJSON.c)

target_include_directories(test_econ_service PRIVATE
    ${CORE}/include
    ${CJSON})

target_compile_options(test_econ_service PRIVATE -Wall -Wextra -O0 -g)
target_link_libraries(test_econ_service m)

if(SANITIZE)
    target_compile_options(test_econ_service PRIVATE -fsanitize=address,undefined)
    target_link_options(test_econ_service PRIVATE -fsanitize=address,undefined)
endif()
```

파일 끝에 추가:

```cmake
add_test(NAME econ_service COMMAND test_econ_service)
```

- [ ] **Step 3: 컴포넌트 `CMakeLists.txt` 에 소스 등록**

`components/stock_core/CMakeLists.txt` 의 `SRCS` 목록에서 `"stock_service.c"` 줄 뒤에 두 줄 추가:

```cmake
        "econ_parse.c"
        "econ_service.c"
```

- [ ] **Step 4: Kconfig 항목 추가**

`components/stock_core/Kconfig.projbuild` 의 `config STOCK_REFRESH_SECONDS` **앞에** 삽입:

```
config STOCK_FMP_API_KEY
    string "Financial Modeling Prep (FMP) / econ-proxy API key"
    default ""
    help
      Powers the home-screen "next high-impact event" row. Free FMP keys get
      HTTP 402 on the economic calendar (paid endpoint); in practice set
      STOCK_ECON_BASE_URL to the bundled free investing.com proxy and put the
      proxy's shared secret here (any non-empty placeholder if the proxy ignores
      it). Stored per-developer in your local sdkconfig — do NOT commit a real key.
      Leave empty to disable the econ row.

config STOCK_ECON_BASE_URL
    string "Economic calendar base URL"
    default "https://financialmodelingprep.com/stable/economic-calendar"
    help
      Fetched as <base>?from=YYYY-MM-DD&to=YYYY-MM-DD&apikey=<key>. Default is FMP
      (paid). To use the free investing.com proxy (tools/econ_proxy on another
      branch), set this to your host, e.g.
      https://econ.example.com/economic-calendar.
```

- [ ] **Step 5: 호스트 테스트 전체 빌드 & 실행**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock/components/stock_core/test/host
rm -rf build && cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: `parse`, `service`, `econ`, `econ_service` 모두 PASS. `test_econ_service` 는 가짜 http_get으로 URL(`/stable/economic-calendar`, `from=`, `to=`, `apikey=`)·에러 본문 처리 검증.

- [ ] **Step 6: 펌웨어 빌드**

```bash
source "/Users/mimi/.espressif/tools/activate_idf_v6.0.1.sh"
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
idf.py build 2>&1 | tail -25
```

Expected: 빌드 성공(`Project build complete`). `econ_parse.c`/`econ_service.c` 가 컴파일되고 `CONFIG_STOCK_FMP_API_KEY`/`CONFIG_STOCK_ECON_BASE_URL` 가 sdkconfig에 생성됨. (아직 호출부 없음 — 미사용 함수 경고는 IDF 기본 설정에서 에러 아님.)

- [ ] **Step 7: 커밋**

```bash
git add components/stock_core/include/econ_service.h components/stock_core/econ_service.c \
        components/stock_core/test/host/test_econ_service.c \
        components/stock_core/test/host/CMakeLists.txt \
        components/stock_core/CMakeLists.txt components/stock_core/Kconfig.projbuild
git commit -m "feat(stock_core): port econ_service fetch layer + Kconfig (host-tested)"
```

---

### Task 5: 홈 행 UI (`ui_home` 라벨 + `ui_stock` 패스스루)

홈 페이지에 캡션+굵은 이벤트명 2줄과 구분선을 추가하고, 외부에서 데이터를 넣는 API를 만든다.

**Files:**
- Modify: `components/stock_core/include/ui_home.h` (include + 선언)
- Modify: `components/stock_core/ui_home.c` (라벨 2개 + rule + `ui_home_set_econ`)
- Modify: `components/stock_core/include/ui_stock.h` (include + 선언)
- Modify: `components/stock_core/ui_stock.c` (`ui_stock_update_econ` 패스스루)

**Interfaces:**
- Consumes: `econ_event_t`(Task 2)
- Produces:
  - `void ui_home_set_econ(const econ_event_t *ev, const char *when_label, bool valid);`
  - `void ui_stock_update_econ(const econ_event_t *ev, const char *when_label, bool valid);`
  - `valid==false`(또는 `ev`/`when_label` NULL)면 행을 비운다.

- [ ] **Step 1: `ui_home.h` 에 include + 선언 추가**

`#include "ui_stock.h"   /* ui_env_t */` 줄 뒤에 추가:

```c
#include "econ_model.h"   /* econ_event_t */
```

`ui_home_tick(void);` 선언 뒤, `#ifdef __cplusplus` 닫기 앞에 추가:

```c
/* Feed the next high-impact economic event into the row below the stock line.
 * when_label is a caller-formatted relative time ("TODAY 21:30"). Pass
 * valid=false (ev/when_label ignored) to clear the row. */
void ui_home_set_econ(const econ_event_t *ev, const char *when_label, bool valid);
```

- [ ] **Step 2: `ui_home.c` 구조체에 라벨 추가**

`static struct { ... } S;` 안, `lv_obj_t *change;` 뒤에 추가:

```c
    lv_obj_t *econ_meta;
    lv_obj_t *econ_name;
```

- [ ] **Step 3: `ui_home_create` 에 행 + 구분선 추가**

`ui_home_create` 안의 `rule(page, 170, 12, 2);` 줄 **뒤**, `/* bottom strip: temp | humidity | battery */` 주석 **앞**에 삽입:

```c
    /* next high-impact economic event: caption + bold name */
    S.econ_meta = mk(page, F_CAP);
    lv_label_set_text(S.econ_meta, "");
    lv_obj_align(S.econ_meta, LV_ALIGN_TOP_LEFT, 16, 176);

    S.econ_name = mk(page, F_TEXT);
    lv_label_set_text(S.econ_name, "");
    lv_obj_set_width(S.econ_name, W - 32);
    lv_label_set_long_mode(S.econ_name, LV_LABEL_LONG_DOTS);
    lv_obj_align(S.econ_name, LV_ALIGN_TOP_LEFT, 16, 196);

    rule(page, 240, 12, 2);
```

(주: `rule()` 의 static 링버퍼는 4쌍 — 호출이 y=52,170,240 로 3개라 한도 내. 변경 불필요.)

- [ ] **Step 4: `ui_home_set_econ` 구현 추가**

`ui_home.c` 의 `ui_home_set_env(...)` 함수 **뒤**에 추가:

```c
void ui_home_set_econ(const econ_event_t *ev, const char *when_label, bool valid) {
    if (!S.econ_name) return;
    if (valid && ev && when_label) {
        char buf[80];
        /* mono font has no star/middle-dot glyph -> ASCII only */
        snprintf(buf, sizeof(buf), "%s   %s   ***", when_label, ev->country);
        lv_label_set_text(S.econ_meta, buf);
        lv_label_set_text(S.econ_name, ev->event[0] ? ev->event : "--");
    } else {
        lv_label_set_text(S.econ_meta, "");
        lv_label_set_text(S.econ_name, "");
    }
    lv_obj_align(S.econ_meta, LV_ALIGN_TOP_LEFT, 16, 176);
    lv_obj_align(S.econ_name, LV_ALIGN_TOP_LEFT, 16, 196);
}
```

- [ ] **Step 5: `ui_stock.h` 에 include + 선언 추가**

`#include "stock_model.h"` 줄 뒤에 추가:

```c
#include "econ_model.h"
```

`void ui_stock_show_page(int index);` 선언 뒤에 추가:

```c
void ui_stock_update_econ(const econ_event_t *ev, const char *when_label, bool valid);
```

- [ ] **Step 6: `ui_stock.c` 패스스루 추가**

`ui_stock_update_env(...)` 함수 **뒤**에 추가:

```c
void ui_stock_update_econ(const econ_event_t *ev, const char *when_label, bool valid) {
    ui_home_set_econ(ev, when_label, valid);
}
```

- [ ] **Step 7: 펌웨어 빌드**

```bash
source "/Users/mimi/.espressif/tools/activate_idf_v6.0.1.sh"
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
idf.py build 2>&1 | tail -20
```

Expected: 빌드 성공. (`ui_home_set_econ`/`ui_stock_update_econ` 컴파일. 호출부는 Task 7.)

- [ ] **Step 8: 커밋**

```bash
git add components/stock_core/include/ui_home.h components/stock_core/ui_home.c \
        components/stock_core/include/ui_stock.h components/stock_core/ui_stock.c
git commit -m "feat(ui): add next-high-impact econ row to the home screen"
```

---

### Task 6: 시뮬레이터에서 행 렌더 확인

보드 없이 더미 econ 이벤트로 새 행이 그려지는지 확인한다.

**Files:**
- Modify: `sim/CMakeLists.txt` (econ_parse.c 링크)
- Modify: `sim/main_sim.c` (헤더 include + 더미 econ push)

**Interfaces:**
- Consumes: `ui_stock_update_econ`(Task 5), `econ_when_label`/`econ_ymd_to_epoch`(Task 2/3)

- [ ] **Step 1: `sim/CMakeLists.txt` 에 econ_parse.c 추가**

`add_executable(sim ...)` 의 SRCS에서 `${CORE}/stock_service.c` 줄 뒤에 추가:

```cmake
    ${CORE}/econ_parse.c
```

- [ ] **Step 2: `sim/main_sim.c` include 추가**

`#include "stock_service.h"` 줄 뒤에 추가:

```c
#include "econ_model.h"
#include "econ_parse.h"
```

그리고 표준 헤더 블록(`#include <math.h>` 부근)에 `#include <time.h>` 가 없으면 추가.

- [ ] **Step 3: 더미 econ 이벤트 push**

`main_sim.c` 의 `ui_stock_update_env(&env);` 줄 **뒤**에 삽입:

```c
    /* Sample economic event so the new "next high-impact" row renders. */
    econ_event_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.country, sizeof ev.country, "%s", "US");
    snprintf(ev.event,   sizeof ev.event,   "%s", "Nonfarm Payrolls");
    ev.impact = ECON_IMPACT_HIGH;
    time_t now_demo = time(NULL);
    ev.ts = (int64_t)now_demo + 3 * 3600;          /* ~3h out -> "TODAY HH:MM" */
    char when_demo[16];
    econ_when_label(ev.ts, now_demo, 0, when_demo, sizeof when_demo);
    ui_stock_update_econ(&ev, when_demo, true);
```

- [ ] **Step 4: 시뮬레이터 빌드 & 렌더**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock/sim
rm -rf build && ./sim.sh 2>&1 | tail -8
```

Expected: `screenshots: shots/sim_page0.png (home) ...` 출력. `shots/sim_page0.png` 의 주식행과 온/습도 strip 사이에 `TODAY HH:MM   US   ***` + `Nonfarm Payrolls` 2줄이 보임. (확인: `open shots/sim_page0.png` 또는 이미지 뷰)

- [ ] **Step 5: 커밋**

```bash
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
git add sim/CMakeLists.txt sim/main_sim.c
git commit -m "test(sim): render the home econ row with a sample event"
```

---

### Task 7: EconTask + 홈 틱 배선 (`user_app.cpp`)

백그라운드 fetch 태스크와, 홈 틱에서 캐시→다음 이벤트 재계산→UI push 를 연결한다.

**Files:**
- Modify: `components/user_app/user_app.cpp`

**Interfaces:**
- Consumes: `econ_service_fetch`, `econ_next_after`, `econ_when_label`, `econ_local_tz_off`(Task 2–4), `ui_stock_update_econ`(Task 5), `CONFIG_STOCK_FMP_API_KEY`(Task 4)

- [ ] **Step 1: include 추가**

`user_app.cpp` 의 `#include "stock_service.h"` 줄 뒤에 추가:

```cpp
#include "econ_service.h"
#include "econ_parse.h"
#include "econ_model.h"
```

- [ ] **Step 2: econ 전역 + 상수 추가**

`#define HOME_TICK_SECONDS 15` 줄 뒤에 추가:

```cpp
/* The economic calendar (high-impact events) changes by the day, so an hourly
 * background refresh is plenty; between refreshes the home tick advances to the
 * next event locally as each one passes. */
#define ECON_REFRESH_SECONDS 3600
```

`static SemaphoreHandle_t s_fetch_wake;` 줄 뒤에 추가:

```cpp
static SemaphoreHandle_t s_econ_mtx;
static econ_calendar_t  *s_econ;          /* shared cache (PSRAM)        */
static econ_calendar_t  *s_econ_scratch;  /* EconTask fetch buffer (PSRAM) */
```

- [ ] **Step 3: `tick_home_econ` 추가**

`tick_home_env(void)` 함수 **뒤**에 추가:

```cpp
/* Recompute the "next high-impact event" from the cached calendar and push it to
 * the home row. Cheap + local: runs on the home tick so the row advances to the
 * next event (and TODAY/TOMORROW stays honest) without a network round-trip. */
static void tick_home_econ(void) {
    econ_event_t ev;
    char when[16];
    bool valid = false;
    time_t now = time(NULL);
    long   tz  = econ_local_tz_off(now);

    xSemaphoreTake(s_econ_mtx, portMAX_DELAY);
    int idx = econ_next_after(s_econ, (int64_t)now);
    if (idx >= 0) { ev = s_econ->items[idx]; valid = true; }
    xSemaphoreGive(s_econ_mtx);

    if (valid) econ_when_label(ev.ts, now, tz, when, sizeof when);
    if (Lvgl_lock(-1)) {
        ui_stock_update_econ(valid ? &ev : nullptr, valid ? when : "", valid);
        Lvgl_unlock();
    }
}
```

- [ ] **Step 4: `EconTask` 추가**

`FetchTask(...)` 함수 **뒤**(또는 `StockTask` 앞)에 추가:

```cpp
/*
 * EconTask — background fetch of the High-impact economic calendar. Never runs
 * on StockTask (which must stay responsive). Pulls this week; if no High event
 * is still upcoming, pulls next week too. Refreshes hourly; the home tick does
 * the per-event advance locally between refreshes.
 */
static void EconTask(void *arg) {
    (void)arg;
    const char *key = CONFIG_STOCK_FMP_API_KEY;
    for (;;) {
        time_t now = time(NULL);
        long   tz  = econ_local_tz_off(now);

        econ_service_fetch(key, now, tz, 0, ECON_IMPACT_HIGH, s_econ_scratch);
        if (s_econ_scratch->valid &&
            econ_next_after(s_econ_scratch, (int64_t)now) < 0) {
            econ_service_fetch(key, now, tz, +1, ECON_IMPACT_HIGH, s_econ_scratch);
        }

        if (s_econ_scratch->valid) {
            xSemaphoreTake(s_econ_mtx, portMAX_DELAY);
            *s_econ = *s_econ_scratch;                /* commit the good fetch */
            xSemaphoreGive(s_econ_mtx);
            ESP_LOGI(TAG, "econ: %d high-impact event(s) (%s)",
                     s_econ->count, s_econ->week_label);
        } else {
            ESP_LOGW(TAG, "econ fetch failed: %s", s_econ_scratch->error);
        }

        tick_home_econ();                            /* repaint with new cache */
        vTaskDelay(pdMS_TO_TICKS(ECON_REFRESH_SECONDS * 1000));
    }
}
```

- [ ] **Step 5: StockTask 호출부에 econ 틱 추가**

`StockTask` 안, 시작부 프라임 구간의 `tick_home_env();` `render_current();` 두 줄 **뒤**에 추가:

```cpp
    tick_home_econ();
```

그리고 idle 분기의 `/* Idle tick: keep the home clock + sensors current (cheap, local). */` 아래 `tick_home_env();` 줄 **뒤**에 추가:

```cpp
            tick_home_econ();
```

- [ ] **Step 6: `UserApp_TaskInit` 에서 할당 + 태스크 생성**

`s_mtx = xSemaphoreCreateMutex();` 줄 **뒤**에 추가:

```cpp
    s_econ_mtx = xSemaphoreCreateMutex();
    s_econ = (econ_calendar_t *)heap_caps_calloc(1, sizeof(econ_calendar_t),
                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_econ_scratch = (econ_calendar_t *)heap_caps_malloc(sizeof(econ_calendar_t),
                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_econ || !s_econ_scratch) { ESP_LOGE(TAG, "econ alloc failed"); return; }
```

`StockTask` 생성 줄(`xTaskCreatePinnedToCore(StockTask, "ui", ...)`) **뒤**에 추가:

```cpp
    /* EconTask: background HTTPS/JSON -> big stack, low prio (UI never waits on it). */
    xTaskCreatePinnedToCore(EconTask, "econ", 16 * 1024, NULL, 2, NULL, 1);
```

- [ ] **Step 7: 펌웨어 빌드**

```bash
source "/Users/mimi/.espressif/tools/activate_idf_v6.0.1.sh"
cd /Users/mimi/Documents/stock_esp32/.claude/worktrees/fix+moreclock
idf.py build 2>&1 | tail -20
```

Expected: 빌드 성공(`Project build complete`).

- [ ] **Step 8: 커밋**

```bash
git add components/user_app/user_app.cpp
git commit -m "feat(app): EconTask + home-tick wiring for the next-high-impact row"
```

---

## 검증 요약 (전체)

- [ ] 호스트 테스트 4종(parse/service/econ/econ_service) 모두 PASS (SANITIZE OFF, ASan 미사용).
- [ ] `idf.py build` 성공 (Task 4/5/7).
- [ ] 시뮬레이터 `shots/sim_page0.png` 에 econ 2줄 행이 주식행과 온/습도 strip 사이에 렌더.
- [ ] (선택, 보드 보유 시) `idf.py -p <PORT> flash monitor` 로 실기 확인 — `STOCK_FMP_API_KEY`/`STOCK_ECON_BASE_URL`(프록시) 설정 후 다음 HIGH 이벤트 표시, 이벤트가 지나면 다음으로 전진.

## 비고

- 실데이터 소스: 메모리 기록상 FMP 무료 키는 econ 캘린더에서 HTTP 402 → 실사용은 investing.com 프록시 URL을 `STOCK_ECON_BASE_URL` 에 설정(프록시 툴 자체는 본 작업 범위 밖, 다른 브랜치).
- 마지막 캐시 HIGH 이벤트가 지나가면 다음 시간당 fetch(최대 1h) 때 다음 주를 끌어와 채운다. 그 사이 행은 비어 있음(의도된 단순화 — 스핀 위험 없는 폴링).
