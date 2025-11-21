/*
 * nv_cfg_store.h
 *
 *  Created on: Sep 2, 2025
 *      Author: Burak Ozdemir
 */

#ifndef NV_CFG_STORE_H
#define NV_CFG_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "stm32f4xx_hal.h"





#define NV_USE_AES_GCM      1
#define NV_AES_KEY_LEN      16
#define NV_GCM_NONCE_LEN    12
#define NV_GCM_TAG_LEN      16

#define NV_CFG_MAGIC        (0xC0FFEE31u)
#define NV_CFG_VERSION      (1u)


#define NV_SSID_MAX_LEN     (32)
#define NV_PASS_MAX_LEN     (64)
#define NV_SERIAL_MAX_LEN   (16)

#define NV_DEVICE_UUID_LEN       16
#define NV_DEVICE_UUID_STR_LEN   37
#define NV_DEVICE_SHORT_STR_LEN  13



typedef enum {
    NV_CFG_OK = 0,
    NV_CFG_ERR_INVALID_PARAM,
    NV_CFG_ERR_NO_VALID_SLOT,
    NV_CFG_ERR_CRC_MISMATCH,
    NV_CFG_ERR_FLASH_OP,
    NV_CFG_ERR_NOT_IMPLEMENTED,
} nv_cfg_result_t;

#pragma pack(push,1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t seq;
    uint32_t crc32;

    uint8_t  first_online_flag;
    char     serial[NV_SERIAL_MAX_LEN];
    char     ssid[NV_SSID_MAX_LEN];

    uint16_t pass_len;
    uint8_t  pass_ct[NV_PASS_MAX_LEN];

    uint32_t fw_version;

    uint8_t  dev_uuid[NV_DEVICE_UUID_LEN];
} nv_cfg_record_t;
#pragma pack(pop)

typedef struct {
    uint32_t base_addr;
    uint32_t end_addr;
    uint32_t slot_size;
    uint32_t slotA_addr;
    uint32_t slotB_addr;

    uint32_t active_slot_addr;
    uint32_t active_seq;
    bool     active_valid;

    nv_cfg_record_t rec;
    bool rec_loaded;
} nv_cfg_ctx_t;

typedef struct {
    char     ssid[NV_SSID_MAX_LEN];
    char     pass[NV_PASS_MAX_LEN];
    uint16_t pass_len;
    char     serial[NV_SERIAL_MAX_LEN];
    bool     first_online_flag;
    uint32_t version;
    uint8_t  dev_uuid[NV_DEVICE_UUID_LEN];
} nv_runtime_cfg_t;


nv_cfg_result_t nv_cfg_init(nv_cfg_ctx_t *ctx);


nv_cfg_result_t nv_cfg_get_first_online_flag(nv_cfg_ctx_t *ctx, bool *out_flag);
nv_cfg_result_t nv_cfg_get_serial(nv_cfg_ctx_t *ctx, char out_serial[NV_SERIAL_MAX_LEN]);
nv_cfg_result_t nv_cfg_get_wifi(nv_cfg_ctx_t *ctx, char out_ssid[NV_SSID_MAX_LEN],
                                uint8_t out_pass_ct[NV_PASS_MAX_LEN], uint16_t *out_pass_len);
nv_cfg_result_t nv_cfg_get_version(nv_cfg_ctx_t *ctx, uint32_t *out_version);
nv_cfg_result_t nv_cfg_set_version(nv_cfg_ctx_t *ctx, uint32_t new_version);

nv_cfg_result_t nv_cfg_set_first_online_flag(nv_cfg_ctx_t *ctx, bool flag);
nv_cfg_result_t nv_cfg_set_serial(nv_cfg_ctx_t *ctx, const char *serial);
nv_cfg_result_t nv_cfg_set_wifi(nv_cfg_ctx_t *ctx, const char *ssid,
                                const uint8_t *pass_ct, uint16_t pass_len);
nv_cfg_result_t nv_cfg_load_globals(void);
nv_cfg_result_t nv_cfg_set_first_online_flag_and_commit(bool flag);

nv_cfg_result_t nv_cfg_commit(nv_cfg_ctx_t *ctx);
nv_cfg_result_t nv_cfg_save_wifi_and_reload_globals(const char *ssid, const char *password);


nv_cfg_result_t nv_id_get_uuid(nv_cfg_ctx_t *ctx, uint8_t out[NV_DEVICE_UUID_LEN]);
nv_cfg_result_t nv_id_set_uuid_and_commit(const uint8_t uuid[NV_DEVICE_UUID_LEN]);
void            nv_id_format_uuid(const uint8_t uuid[16], char out[NV_DEVICE_UUID_STR_LEN]);
void            nv_id_format_short12(const uint8_t uuid[16], char out[NV_DEVICE_SHORT_STR_LEN]);
nv_cfg_result_t nv_id_ensure_uuid(void);


static inline uint32_t nv_cfg_active_slot_addr(const nv_cfg_ctx_t *ctx) { return ctx->active_slot_addr; }

extern nv_runtime_cfg_t g_nv_cfg;
extern uint32_t __cfg_start;
extern uint32_t __cfg_end;
extern uint32_t __cfg_slot_size;

#endif /* NV_CFG_STORE_H */

