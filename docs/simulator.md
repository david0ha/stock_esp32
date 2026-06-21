# LVGL Simulator (Verify the UI Without a Board)

A tool for rendering and checking the LVGL UI of the ESP32-S3-RLCD-4.2 on macOS **without connecting a board**.
Location: `sim/`.

## What It Does

- **Reuses the LVGL sources and GUI Guider UI as-is** from the ESP-IDF project (repository root).
- Applies the **same black-and-white binarization rule** as the device's `Lvgl_FlushCallback`
  (`pixel < 0x7FFF ? black : white`) → reproducing exactly how it will actually look on the reflective black-and-white panel.
- Renders headlessly and saves **PNG screenshots** (no GUI window required).

## Usage

```bash
cd sim
./sim.sh        # build → run → generate shots/sim_frame1.png, sim_frame2.png
```

Manual steps:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/sim shots                       # BMP output
sips -s format png shots/sim_frame1.bmp --out shots/sim_frame1.png
```

## Structure

| File | Role |
|------|------|
| `main_sim.c` | Headless LVGL display + RGB565→black/white + BMP save |
| `lv_conf.h` | LVGL configuration (color depth 16 = same as device, snapshot/log on, CLIB malloc) |
| `CMakeLists.txt` | Bundles LVGL (example managed_components) + ui_bsp sources for the build |
| `sim.sh` | Build + run + PNG conversion in one step |

## UI of the Current Example (09_LVGL_V9_Test)

Two full-screen images, `screen_img_1` (bear) and `screen_img_2` (cat), alternate every 1.5 seconds.
`main_sim.c` captures the two images, each in its displayed state.

## Things to Know / Limitations

- **The binarization rule is coarse**: it is the simple `px < 0x7FFF` threshold that the device uses, so color/midtone
  images (the bear) come out as a nearly black silhouette. Photos (the cat) have high contrast, so black and white separate well.
  → For a better monochrome representation, you should switch to **dithering** or a **luminance threshold**
  (this must be changed together with the device firmware's flush callback to match the real hardware).
- The simulator reproduces **only the LVGL UI layer**. Audio/sensors/SD/actual SPI timing/the physical contrast of the
  reflective panel can only be verified on the real board.
- Because the UI code (`ui_bsp`) is hardware-independent, it shares the **same UI sources** as the board firmware,
  allowing you to iterate on the design quickly.

## Button/State Simulation

The current example UI has no buttons, so it only captures the toggle between the two images. Once buttons/widgets are added,
in `main_sim.c` you can (a) change the widget state directly or (b) inject `lv_indev` input, then take a
snapshot to capture per-state screens.

## (Optional) Interactive SDL Window

The current approach is headless screenshots. If you need a real-time window where you can **click directly** with mouse/keyboard,
you can add a windowed mode using LVGL 9's SDL driver (`lv_sdl_window` + `lv_sdl_mouse`)
(SDL2 is installed: `/opt/homebrew/opt/sdl2`). Request it when needed.
