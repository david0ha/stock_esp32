#pragma once

#include "prov_config.h"   /* prov_config_t — the provisioned WiFi + watchlist */

#ifdef __cplusplus
extern "C" {
#endif

void UserApp_AppInit(void);                       /* nvs + cJSON PSRAM hooks      */
void UserApp_UiInit(void);                        /* build the stock UI           */
void UserApp_TaskInit(const prov_config_t *cfg);  /* spawn the fetch/rotate task  */

#ifdef __cplusplus
}
#endif
