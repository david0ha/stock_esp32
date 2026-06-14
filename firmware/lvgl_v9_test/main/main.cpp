
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "display_bsp.h"
#include "lvgl_bsp.h"
#include "user_app.h"
#include "user_config.h"
#include "provisioning.h"

static const char *TAG = "main";

DisplayPort RlcdPort(12,11,5,40,41,LCD_WIDTH,LCD_HEIGHT);

static void Lvgl_FlushCallback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
  	uint16_t *buffer = (uint16_t *)color_map;
  	for(int y = area->y1; y <= area->y2; y++)
  	{
  	 	for(int x = area->x1; x <= area->x2; x++)
  	 	{
  	 	   	uint8_t color = (*buffer < 0x7fff) ? ColorBlack : ColorWhite;
  	 	   	RlcdPort.RLCD_SetPixel(x, y, color);
  	 	   	buffer++;
  	 	}
  	}
  	RlcdPort.RLCD_Display();
	lv_disp_flush_ready(drv);
}

// --- Lightweight setup/status screen shown while provisioning runs ---------

static lv_obj_t *s_status_msg;
static lv_obj_t *s_status_hint;

static lv_obj_t *make_label(lv_obj_t *parent, int width, const lv_font_t *font)
{
	lv_obj_t *l = lv_label_create(parent);
	lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(l, width);
	lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_color(l, lv_color_black(), 0);
	if (font) {
		lv_obj_set_style_text_font(l, font, 0);
	}
	return l;
}

static void BuildStatusScreen(void)
{
	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	lv_obj_t *title = make_label(scr, 380, NULL);
	lv_label_set_text(title, "TICKER BOARD");
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

	s_status_msg = make_label(scr, 380, NULL);
	lv_label_set_text(s_status_msg, "Starting up...");
	lv_obj_align(s_status_msg, LV_ALIGN_CENTER, 0, -8);

	s_status_hint = make_label(scr, 380, NULL);
	lv_label_set_text(s_status_hint, "");
	lv_obj_align(s_status_hint, LV_ALIGN_BOTTOM_MID, 0, -28);
}

static void SetStatus(const char *msg, const char *hint)
{
	if (Lvgl_lock(-1)) {
		if (s_status_msg) {
			lv_label_set_text(s_status_msg, msg);
		}
		if (s_status_hint) {
			lv_label_set_text(s_status_hint, hint ? hint : "");
		}
		Lvgl_unlock();
	}
}

static void OnProvisioningEvent(prov_event_t event, const char *info, void *user)
{
	(void)user;
	char msg[96];
	char hint[160];
	switch (event) {
	case PROV_EVENT_STA_CONNECTING:
		snprintf(msg, sizeof(msg), "Connecting to\n%s", info ? info : "");
		SetStatus(msg, "Please wait");
		break;
	case PROV_EVENT_STA_CONNECTED:
		snprintf(msg, sizeof(msg), "Connected to\n%s", info ? info : "");
		SetStatus(msg, "Loading your watchlist...");
		break;
	case PROV_EVENT_PORTAL_STARTED:
		snprintf(hint, sizeof(hint),
		         "1. Join Wi-Fi:  %s\n2. Open  192.168.4.1", info ? info : "");
		SetStatus("Set up Wi-Fi", hint);
		break;
	case PROV_EVENT_CONFIG_SAVED:
		snprintf(msg, sizeof(msg), "Saved \"%s\"", info ? info : "");
		SetStatus(msg, "Restarting...");
		break;
	}
}

extern "C" void app_main(void)
{
	UserApp_AppInit();
	RlcdPort.RLCD_Init();
	Lvgl_PortInit(400,300,Lvgl_FlushCallback);

	if (Lvgl_lock(-1)) {
		BuildStatusScreen();
		Lvgl_unlock();
	}

	prov_options_t opts;
	provisioning_default_options(&opts);
	strlcpy(opts.ap_ssid_prefix, "Ticker Board", sizeof(opts.ap_ssid_prefix));
	opts.event_cb = OnProvisioningEvent;

	prov_config_t cfg;
	bool connected = provisioning_run(&opts, &cfg);  // blocks (and reboots) until configured

	if (connected) {
		ESP_LOGI(TAG, "online — %u ticker(s) saved", (unsigned)cfg.ticker_count);
		// Hand off to the application UI (placeholder demo until the ticker view lands).
		if (Lvgl_lock(-1)) {
			UserApp_UiInit();
			Lvgl_unlock();
		}
		UserApp_TaskInit();
	}
}
