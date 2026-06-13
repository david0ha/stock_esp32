/*
 * LVGL 데스크톱 시뮬레이터 (헤드리스 → PNG 스크린샷)
 *
 * ESP32-S3-RLCD-4.2 의 LVGL UI(GUI Guider 생성)를 보드 없이 렌더링한다.
 * 디바이스의 Lvgl_FlushCallback 과 동일하게 RGB565 프레임버퍼를
 * `(픽셀 < 0x7FFF) ? 검정 : 흰색` 규칙으로 이진화하여, 반사형 흑백 패널에
 * 실제로 보일 모습을 그대로 BMP로 저장한다(이후 sips로 PNG 변환).
 *
 * 사용법: ./sim [출력디렉토리]   (기본 /tmp)
 *   sim_frame1.bmp = screen_img_1, sim_frame2.bmp = screen_img_2
 */
#include "lvgl.h"
#include "gui_guider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HOR 400
#define VER 300

static uint8_t  fb[HOR * VER * 2];      /* LVGL 드로우 버퍼 (RGB565, FULL 모드) */
static uint16_t capture[HOR * VER];     /* 마지막 풀프레임 캡처 */
static int      captured = 0;
static uint32_t g_tick = 0;

static uint32_t tick_cb(void) { return g_tick; }

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    int w = a->x2 - a->x1 + 1;
    int h = a->y2 - a->y1 + 1;
    if (w == HOR && h == VER) {          /* FULL 모드: 전체 화면 한 번에 */
        memcpy(capture, px, sizeof(capture));
        captured = 1;
    }
    lv_display_flush_ready(d);
}

static void run_refresh(int steps)
{
    for (int i = 0; i < steps; i++) {
        g_tick += 16;
        lv_timer_handler();
    }
}

/* capture[] (RGB565) → 24bit BMP, 디바이스와 동일한 임계값으로 흑백화 */
static void write_mono_bmp(const char *path)
{
    int W = HOR, H = VER;
    int rowsize  = (W * 3 + 3) & ~3;
    int datasize = rowsize * H;
    int filesize = 54 + datasize;

    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2]  = filesize; hdr[3] = filesize >> 8; hdr[4] = filesize >> 16; hdr[5] = filesize >> 24;
    hdr[10] = 54;                 /* 픽셀 데이터 오프셋 */
    hdr[14] = 40;                 /* DIB 헤더 크기 */
    hdr[18] = W; hdr[19] = W >> 8; hdr[20] = W >> 16; hdr[21] = W >> 24;
    hdr[22] = H; hdr[23] = H >> 8; hdr[24] = H >> 16; hdr[25] = H >> 24;
    hdr[26] = 1;                  /* planes */
    hdr[28] = 24;                 /* bpp */
    hdr[34] = datasize; hdr[35] = datasize >> 8; hdr[36] = datasize >> 16; hdr[37] = datasize >> 24;

    FILE *f = fopen(path, "wb");
    if (!f) { printf("cannot open %s\n", path); return; }
    fwrite(hdr, 1, 54, f);

    uint8_t *row = (uint8_t *)calloc(1, rowsize);
    for (int y = H - 1; y >= 0; y--) {          /* BMP는 bottom-up */
        for (int x = 0; x < W; x++) {
            uint16_t p = capture[y * W + x];
            uint8_t  v = (p < 0x7FFF) ? 0 : 255; /* 디바이스 규칙 그대로 */
            row[x * 3 + 0] = v;                  /* B */
            row[x * 3 + 1] = v;                  /* G */
            row[x * 3 + 2] = v;                  /* R */
        }
        fwrite(row, 1, rowsize, f);
    }
    free(row);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *outdir = (argc > 1) ? argv[1] : "/tmp";

    lv_init();
    lv_tick_set_cb(tick_cb);

    lv_display_t *disp = lv_display_create(HOR, VER);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_FULL);

    static lv_ui ui;
    setup_ui(&ui);
    lv_screen_load(ui.screen);

    char path[600];

    /* 프레임 1: screen_img_1 표시 */
    lv_obj_clear_flag(ui.screen_img_1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.screen_img_2, LV_OBJ_FLAG_HIDDEN);
    captured = 0;
    run_refresh(10);
    snprintf(path, sizeof(path), "%s/sim_frame1.bmp", outdir);
    write_mono_bmp(path);
    printf("wrote %s (captured=%d)\n", path, captured);

    /* 프레임 2: screen_img_2 표시 */
    lv_obj_clear_flag(ui.screen_img_2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.screen_img_1, LV_OBJ_FLAG_HIDDEN);
    captured = 0;
    run_refresh(10);
    snprintf(path, sizeof(path), "%s/sim_frame2.bmp", outdir);
    write_mono_bmp(path);
    printf("wrote %s (captured=%d)\n", path, captured);

    return 0;
}
