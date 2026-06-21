# ESP-IDF Development Guide — ESP32-S3-RLCD-4.2

This board is developed with ESP-IDF. This document covers the workflow from environment
activation through build/flash/debug, all the way to the peripheral driver components.

## 1. Environment activation

ESP-IDF **v6.0.1** is installed on this machine under `~/.espressif/`.
You must run the following **every time you open a new terminal** so that `idf.py` is added to PATH:

```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"
```

Verify:

```bash
idf.py --version      # OK if it prints ESP-IDF v6.0.1
```

## 2. Creating a project

```bash
# Copy-an-example approach
cp -r $IDF_PATH/examples/get-started/hello_world my_app
cd my_app

# Or an empty project
idf.py create-project my_app && cd my_app
```

## 3. Setting the target (once per project)

```bash
idf.py set-target esp32s3
```

## 4. Board configuration (menuconfig)

```bash
idf.py menuconfig
```

Items you must verify for this board:

- **Serial flasher config → Flash size** → `16 MB`
- **Component config → ESP PSRAM**
  - Enable `Support for external, SPI-connected RAM`
  - `SPI RAM config → Mode` → **Octal Mode PSRAM**
  - Speed 80MHz (if needed)
- **Component config → ESP System Settings → Channel for console output**
  - When using the direct USB-C connection, you can select `USB Serial/JTAG Controller`
- Partition Table: to make use of the 16MB flash, a custom `partitions.csv` is recommended
  (app + SPIFFS/FAT + OTA, etc.).

> Writing the settings above into `sdkconfig.defaults` makes them reproducible across the team/CI. Example:
> ```
> CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
> CONFIG_SPIRAM=y
> CONFIG_SPIRAM_MODE_OCT=y
> CONFIG_SPIRAM_SPEED_80M=y
> ```

## 5. Build / flash / monitor

```bash
idf.py build
idf.py -p <PORT> flash monitor
```

- Exit the serial monitor: `Ctrl + ]`
- Finding `<PORT>` (macOS):
  ```bash
  ls /dev/cu.*
  ```
  - Native USB Serial/JTAG: `/dev/cu.usbmodem*`
  - UART bridge (CH34x/CP210x, etc.): `/dev/cu.usbserial-*` / `/dev/cu.wchusbserial*`
- If entering flash mode fails: **hold the BOOT button while clicking RESET → release BOOT**, then retry.
- Individual steps: `idf.py build`, `idf.py flash`, `idf.py monitor`, `idf.py app-flash`, etc.

## 6. Debugging (JTAG / USB Serial/JTAG)

The ESP32-S3 has a built-in native USB Serial/JTAG, so OpenOCD/GDB debugging is possible without an extra adapter:

```bash
idf.py openocd            # Start the OpenOCD server (in a separate terminal)
idf.py gdb                # Connect with GDB
# Or all at once
idf.py openocd gdbgui
```

- It uses the Espressif-patched OpenOCD build included with ESP-IDF (no separate installation needed).
- In VS Code, you can configure debugging with the "Espressif IDF" extension + `idf.py`.

## 7. Peripheral drivers (ESP-IDF components)

See [pinout.md](pinout.md) for the pin map. Components are fetched from the ESP Component Registry
(`idf.py add-dependency "<name>"`).

### Display (ST7305/ST7306, SPI, 400×300 monochrome)

- Use the `esp_lcd` component to create the SPI panel IO, then attach the dedicated ST7305/ST7306 panel driver.
- IDF may not include a driver for monochrome reflective controllers out of the box, so
  search the Component Registry for `esp_lcd_st7305` / `esp_lcd_st7306`, or port it yourself.
  ```bash
  idf.py add-dependency "espressif/esp_lcd_st7306"   # after checking whether it exists
  ```
- When integrating with LVGL, adjust the color depth and flush callback to match the 1bpp framebuffer / partial-refresh characteristics.

### Touch (capacitive, I2C)

- An `esp_lcd_touch`-family driver + the driver for the corresponding touch controller. Shares I2C0 (GPIO13/14).

### Audio (ES8311 codec + ES7210 microphone ADC, I2S)

- Use `esp_codec_dev` or the `es8311`/`es7210` drivers from ESP-ADF.
- I2S data: the I2S0 pins in [pinout.md](pinout.md); control: I2C0.

### Sensors / RTC

- SHTC3 (0x70), PCF85063A (0x51) — I2C0. Either a driver exists in the Component Registry,
  or do a simple datasheet-based implementation.

### microSD

- `esp_driver_sdmmc` (SDMMC, 1-bit). For FAT mounting, use `esp_vfs_fat`.

## 8. Summary of frequently used commands

```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"  # environment
idf.py set-target esp32s3        # target
idf.py menuconfig                # configuration
idf.py build                     # build
idf.py -p <PORT> flash monitor   # flash + monitor
idf.py fullclean                 # clean the build cache
idf.py size                      # memory usage
idf.py add-dependency "<comp>"   # add a component
```

## References

- The Zephyr/west workflow (`west flash`, `west espressif monitor`) is for Zephyr board definitions;
  this repository uses **ESP-IDF**. Since the pin map is identical, use the Zephyr devicetree only as a pin reference.
- Original docs/datasheets: [references.md](references.md)
