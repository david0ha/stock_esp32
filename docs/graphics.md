# Graphics Rendering Approaches — RLCD 4.2" (400×300, 1-bit Black & White)

This board uses a **black & white 1bpp** (no grayscale) reflective panel. Below we organize how the
examples draw graphics into two tracks. We also note the criteria for deciding which one to pull into
our repository (ESP-IDF) development.

Pinout: SPI `MOSI=12, SCLK=11, DC=5, CS=40, RST=41` (→ [pinout.md](pinout.md)).

---

## Approach A — 1-bit Framebuffer + Immediate-Mode GFX (Volos eBikeRLCD)

The actual working structure of `examples/VolosR-eBikeRLCD/espRLCD/`:

### Core Pipeline
```
Draw everything to an off-screen 1-bit canvas every frame
        ↓ (per-pixel copy)
RLCD panel buffer (display_bsp)
        ↓ (batched SPI transfer)
Screen refresh
```

1. **Off-screen canvas (RAM, 1bpp)**
   ```cpp
   TFT_eSprite canvas = TFT_eSprite(&tft);
   canvas.setColorDepth(1);        // 1-bit
   canvas.createSprite(400, 300);  // 400×300 = 15,000-byte buffer
   canvas.setTextDatum(4);         // center text alignment
   ```
   - Drawing happens not on the actual panel but on an in-memory 1-bit buffer (double-buffering → no flicker).

2. **Immediate-mode full redraw every frame** (`draw()`)
   ```cpp
   canvas.fillRect(0,0,400,300,0);          // clear screen (black)
   canvas.fillCircle(cx, cy, r+12, 0);      // gauge circle
   canvas.drawArc(cx,cy,r+2,r-2,70,290,1,0,0);   // tick arc
   canvas.drawWideLine(...);  canvas.drawWedgeLine(...);  // thick / wedge lines
   canvas.fillTriangle(...);  canvas.fillRoundRect(...);  // turn-signal / thermometer icons
   ```
   - Primitives used: `fillRect / fillCircle / fillTriangle / fillRoundRect /
     drawArc / drawWideLine / drawWedgeLine / drawString`.
   - Colors are only `0` (black) and `1` (white). No gradients → if needed, you must draw a dithering pattern yourself.

3. **Custom fonts** — TTF-converted headers (`smallFont.h`, `valueFont.h`, `midleFont.h`)
   switched at runtime:
   ```cpp
   canvas.loadFont(valueFont);
   canvas.drawString(String(speed), cx, 270);
   canvas.unloadFont();
   ```

4. **Gauge = precomputed trigonometric coordinates** (done once in `setup()`)
   ```cpp
   double rad = 0.01745;            // deg→rad
   for (int i=0;i<225;i++){         // precompute point coordinates per angle
     x[i]=(r-4)*cos(rad*a)+cx;  y[i]=(r-4)*sin(rad*a)+cy;   // outer tick point
     nx[i]=(r-60)*cos(rad*a)+cx; ny[i]=(r-60)*sin(rad*a)+cy; // needle tip point
     a++; if(a==360) a=0;
   }
   ```
   - The loop only does array lookups → the needle is `drawWedgeLine(nx[speed],ny[speed],cx,cy,2,8,1)`.
   - Avoiding `cos/sin` calls every frame saves CPU (important since reflective panels refresh slowly).

5. **Canvas → panel push** (`pushCanvasToRLCD()`)
   ```cpp
   uint8_t *buf = canvas.getPointer();        // direct access to the 1-bit buffer
   int bytesPerRow = (400 + 7) / 8;
   RlcdPort.RLCD_ColorClear(ColorWhite);
   for (y..) for (bit..) if (bit set) RlcdPort.RLCD_SetPixel(x, y, ColorBlack);
   RlcdPort.RLCD_Display();                   // batched transfer to panel over SPI
   ```

6. **Animation** — handled via state counters (e.g., turn-signal blink `ind_ani`, speed `speed`);
   `loop()` updates the values, then calls `draw()` again.

### ⭐ ESP-IDF Reuse Points
`display_bsp.h`/`.cpp` (the DisplayPort class) is a **pure ESP-IDF driver**
(uses `driver/spi_master.h`, `esp_lcd_panel_io.h`, no Arduino dependency). In other words:
- **The driver can be brought into an ESP-IDF project as-is.** It includes ST7305/ST7306 initialization
  plus pixel LUT optimization (`AlgorithmOptimization 3`, lookup-table x,y→buffer bit mapping).
- The only Arduino-bound part is the sketch-side `TFT_eSprite` canvas. In ESP-IDF you can replace it
  with **LVGL's 1bpp buffer** or your **own 1-bit buffer + Adafruit_GFX `GFXcanvas1`**.
- (Note: `VolosR-waveshareLRCL` follows the same pattern but uses `GFXcanvas1` as its canvas, making it lighter.)

---

## Approach B — LVGL (Draeger-IT Part 2 = Same Structure as the Waveshare Official Example)

Draeger-IT **Part 1** is an unboxing (no code); **Part 2** covers driving it with the Arduino IDE + **LVGL**.
However, the structure Part 2 uses (`display_bsp.h` + `lvgl_bsp.h` + threshold flush) is **essentially
identical to the example in the Waveshare official repository**. On top of that, the official repository
**already includes an ESP-IDF LVGL example**, so it can be brought directly into our target (ESP-IDF):

- `examples/waveshare-official/02_Example/ESP-IDF/09_LVGL_V9_Test` (LVGL 9)
- `examples/waveshare-official/02_Example/ESP-IDF/08_LVGL_V8_Test` (LVGL 8)
- Arduino version: `02_Example/Arduino/09_LVGL_V9_Test` etc. (Draeger Part 2 belongs to this line)

### Component Structure (based on the official ESP-IDF 09_LVGL_V9_Test)
```
main/                  app_main: driver init → LVGL init → UI init → start task
components/
├─ port_bsp/display_bsp  ST7305/6 panel driver (same esp_lcd SPI driver as Approach A)
├─ app_bsp/lvgl_bsp      LVGL porting: init, PSRAM double buffer, tick timer, dedicated task + mutex
├─ ui_bsp/               UI code — generated by GUI Guider (gui_guider.c, setup_scr_screen.c …)
└─ user_app/             app glue
```
LVGL is **installed automatically by the component manager** (`main/idf_component.yml`):
```yaml
dependencies:
  lvgl/lvgl: ^9.4.0
```

### Key Point 1 — RGB565 → 1-bit Threshold Conversion in the Flush Callback
LVGL internally renders in **RGB565 (16-bit color)**, and when pushing to the panel it decides
black/white per pixel via a threshold in the callback (`main/main.cpp`):
```cpp
static void Lvgl_FlushCallback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map) {
    uint16_t *buffer = (uint16_t *)color_map;
    for (int y = area->y1; y <= area->y2; y++)
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buffer < 0x7fff) ? ColorBlack : ColorWhite; // binarize at the midpoint
            RlcdPort.RLCD_SetPixel(x, y, color);
            buffer++;
        }
    RlcdPort.RLCD_Display();          // batched SPI transfer
    lv_disp_flush_ready(drv);
}
```
> Because it is a simple threshold, there is **no tonal gradation / dithering**. If you need gradients,
> either add dithering here, or use LVGL's native 1bpp format (`LV_COLOR_FORMAT_I1`) (saves memory).

### Key Point 2 — Buffers / Task / Tick (`lvgl_bsp.cpp`)
```cpp
lv_init();
lv_display_t *disp = lv_display_create(width, height);
lv_display_set_flush_cb(disp, flush_cb);
// allocate a full-frame double buffer in PSRAM (RGB565: 400*300*2 ≈ 240KB ×2)
buffer_1 = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
buffer_2 = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
lv_display_set_buffers(disp, buffer_1, buffer_2, buffer_size, LV_DISPLAY_RENDER_MODE_FULL);
// call lv_tick_inc() periodically via esp_timer; run lv_timer_handler() in a dedicated FreeRTOS task
```
- **Full-refresh mode** (`RENDER_MODE_FULL`) — suitable for slow-refreshing reflective panels.
- LVGL work is protected by a **dedicated task + mutex (`Lvgl_lock/unlock`)** → thread-safe when other
  tasks touch the UI.

### Key Point 3 — Fonts & Board Settings (the Draeger blog particularly emphasizes this)
- LVGL disables most fonts by default. In `lv_conf.h` you must enable only the ones you need:
  `LV_FONT_MONTSERRAT_12 1` … `LV_FONT_MONTSERRAT_48 1`.
- Board/build settings (boot assert if missing): **Flash QIO 80MHz / 16MB**, **Octal(OPI) PSRAM**,
  `FREERTOS_HZ=1000`, (Arduino: **USB CDC On Boot = Enabled**). The official example's
  `sdkconfig.defaults` contains the same:
  ```
  CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
  CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
  CONFIG_SPIRAM=y
  CONFIG_SPIRAM_MODE_OCT=y
  CONFIG_SPIRAM_SPEED_80M=y
  CONFIG_FREERTOS_HZ=1000
  ```

### Authoring the UI — GUI Guider
The official example's `ui_bsp/generated/` is code generated by **NXP GUI Guider** (a drag-and-drop UI tool).
Designing the screen in the GUI automatically generates LVGL C code, so you don't have to write widget code by hand.

---

## When Using Graphics in Our Repository (ESP-IDF) — Selection Guide

| Purpose | Recommendation | Rationale |
|------|------|------|
| **Custom graphics drawn by hand**, like gauges/dashboards | **Approach A** | `display_bsp` (ESP-IDF native) + 1bpp canvas. Lightweight with clear control. The Volos gauge pattern can be ported as-is |
| **Widget-based UI** such as buttons, lists, charts, touch | **Approach B (LVGL)** | Official LVGL porting + esp_lcd. Check `waveshare-official/02_Example/ESP-IDF` first |

Common principles (1bpp panel):
- Colors are black/white only — shading is expressed via **dithering (dot/hatch patterns)**.
- Refresh is slow because it is reflective → **draw into an off-screen buffer, then push at once**; for
  elements that don't change, don't recompute them every frame — use **precomputation / partial updates**
  to save CPU and power.
- For fonts, use header-converted bitmap fonts (GFX) or LVGL fonts.

> For general info on ESP-IDF build/driver components, see [esp-idf-development.md](esp-idf-development.md).
