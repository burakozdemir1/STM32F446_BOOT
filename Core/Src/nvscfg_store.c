/*
 * nv_cfg_store.c
 *
 *  Created on: Sep 2, 2025
 *      Author: Burak Ozdemir
 */

#include <string.h>
#include <stddef.h>
#include "nvscfg_store.h"
#include "nvslogger.h"

nv_runtime_cfg_t g_nv_cfg;

static inline int field_present_in_rec(const nv_cfg_record_t *rec, size_t field_offset, size_t field_size)
{

    return (rec->length >= (uint16_t)(field_offset + field_size));
}

nv_cfg_result_t nv_cfg_get_version(nv_cfg_ctx_t *ctx, uint32_t *out_version)
{
    if (!ctx || !out_version) return NV_CFG_ERR_INVALID_PARAM;
    if (!ctx->rec_loaded)     return NV_CFG_ERR_NO_VALID_SLOT;

    if (field_present_in_rec(&ctx->rec,
                             offsetof(nv_cfg_record_t, fw_version),
                             sizeof(ctx->rec.fw_version))) {
        *out_version = ctx->rec.fw_version;
    } else {
        *out_version = 0u;
    }
    return NV_CFG_OK;
}
nv_cfg_result_t nv_cfg_set_version(nv_cfg_ctx_t *ctx, uint32_t new_version)
{
    if (!ctx) return NV_CFG_ERR_INVALID_PARAM;
    if (!ctx->rec_loaded) return NV_CFG_ERR_NO_VALID_SLOT;
    ctx->rec.fw_version = new_version;
    return NV_CFG_OK;
}

#ifdef STM32F4xx
static uint32_t nv_flash_addr_to_sector(uint32_t addr)
{
    /* STM32F446RE iç flash sector haritası:
     * Sector 0: 0x0800 0000 - 0x0800 3FFF (16 KB)
     * Sector 1: 0x0800 4000 - 0x0800 7FFF (16 KB)
     * Sector 2: 0x0800 8000 - 0x0800 BFFF (16 KB)
     * Sector 3: 0x0800 C000 - 0x0800 FFFF (16 KB)
     * Sector 4: 0x0801 0000 - 0x0801 FFFF (64 KB)
     * Sector 5: 0x0802 0000 - 0x0803 FFFF (128 KB)
     * Sector 6: 0x0804 0000 - 0x0805 FFFF (128 KB)
     * Sector 7: 0x0806 0000 - 0x0807 FFFF (128 KB)
     */

    if (addr < 0x08004000)      return FLASH_SECTOR_0;
    else if (addr < 0x08008000) return FLASH_SECTOR_1;
    else if (addr < 0x0800C000) return FLASH_SECTOR_2;
    else if (addr < 0x08010000) return FLASH_SECTOR_3;
    else if (addr < 0x08020000) return FLASH_SECTOR_4;
    else if (addr < 0x08040000) return FLASH_SECTOR_5;
    else if (addr < 0x08060000) return FLASH_SECTOR_6;
    else if (addr < 0x08080000) return FLASH_SECTOR_7;

    return 0xFFFFFFFFu; /* Geçersiz adres */
}
#endif


nv_cfg_result_t nv_cfg_load_globals(void)
{
    nv_cfg_ctx_t ctx;
    nv_cfg_result_t r = nv_cfg_init(&ctx);
    if (r != NV_CFG_OK) {
        memset(&g_nv_cfg, 0, sizeof(g_nv_cfg));
        logInfo("CFG: init FAILED (%d)\r\n", r);
        return r;
    }

    /* ---- Flash Area ---- */
    logInfo("CFG region:  [0x%08lX .. 0x%08lX) slot_size=0x%08lX\r\n",
                 (unsigned long)ctx.base_addr,
                 (unsigned long)ctx.end_addr,
                 (unsigned long)ctx.slot_size);

    if (ctx.active_valid) {
    	logInfo("CFG active: slot=0x%08lX seq=%lu (A=0x%08lX, B=0x%08lX)\r\n",
                     (unsigned long)ctx.active_slot_addr,
                     (unsigned long)ctx.active_seq,
                     (unsigned long)ctx.slotA_addr,
                     (unsigned long)ctx.slotB_addr);
    } else {
    	logInfo("CFG active: NO VALID SLOT (first-time/defaults)\r\n");
    }
    /* ---- SSID + PASS ---- */
    char ssid[NV_SSID_MAX_LEN] = {0};
    uint8_t pass[NV_PASS_MAX_LEN] = {0};
    uint16_t pass_len = 0;
    (void)nv_cfg_get_wifi(&ctx, ssid, pass, &pass_len);

    memset(g_nv_cfg.ssid, 0, NV_SSID_MAX_LEN);
    strncpy(g_nv_cfg.ssid, ssid, NV_SSID_MAX_LEN - 1);

    memset(g_nv_cfg.pass, 0, NV_PASS_MAX_LEN);
    if (pass_len > NV_PASS_MAX_LEN) pass_len = NV_PASS_MAX_LEN;
    if (pass_len) memcpy(g_nv_cfg.pass, pass, pass_len);
    g_nv_cfg.pass_len = pass_len;

    /* ---- SERIAL ---- */
    char serial[NV_SERIAL_MAX_LEN] = {0};
    (void)nv_cfg_get_serial(&ctx, serial);
    memset(g_nv_cfg.serial, 0, NV_SERIAL_MAX_LEN);
    strncpy(g_nv_cfg.serial, serial, NV_SERIAL_MAX_LEN - 1);

    /* ---- FIRST-ONLINE FLAG ---- */
    bool f = false;
    (void)nv_cfg_get_first_online_flag(&ctx, &f);
    g_nv_cfg.first_online_flag = f;

    /* ---- VERSION ---- */
    uint32_t v = 0;
    (void)nv_cfg_get_version(&ctx, &v);
    g_nv_cfg.version = v;


    memcpy(g_nv_cfg.dev_uuid, ctx.rec.dev_uuid, 16);

    if (ctx.active_valid) {
        uint32_t base   = ctx.active_slot_addr;
        uint32_t a_magic   = base + (uint32_t)offsetof(nv_cfg_record_t, magic);
        uint32_t a_ver     = base + (uint32_t)offsetof(nv_cfg_record_t, version);
        uint32_t a_len     = base + (uint32_t)offsetof(nv_cfg_record_t, length);
        uint32_t a_seq     = base + (uint32_t)offsetof(nv_cfg_record_t, seq);
        uint32_t a_crc     = base + (uint32_t)offsetof(nv_cfg_record_t, crc32);
        uint32_t a_flag    = base + (uint32_t)offsetof(nv_cfg_record_t, first_online_flag);
        uint32_t a_serial  = base + (uint32_t)offsetof(nv_cfg_record_t, serial);
        uint32_t a_ssid    = base + (uint32_t)offsetof(nv_cfg_record_t, ssid);
        uint32_t a_passlen = base + (uint32_t)offsetof(nv_cfg_record_t, pass_len);
        uint32_t a_passct  = base + (uint32_t)offsetof(nv_cfg_record_t, pass_ct);
        uint32_t a_fwver   = base + (uint32_t)offsetof(nv_cfg_record_t, fw_version);


        logInfo("CFG[HDR] base=0x%08lX\n",  (unsigned long)base);
        logInfo("CFG[HDR] magic   @0x%08lX = 0x%08lX\n", (unsigned long)a_magic, (unsigned long)ctx.rec.magic);
        logInfo("CFG[HDR] version @0x%08lX = %u\n",      (unsigned long)a_ver,   (unsigned)ctx.rec.version);
        logInfo("CFG[HDR] length  @0x%08lX = %u\n",      (unsigned long)a_len,   (unsigned)ctx.rec.length);
        logInfo("CFG[HDR] seq     @0x%08lX = %lu\n",     (unsigned long)a_seq,   (unsigned long)ctx.rec.seq);
        logInfo("CFG[HDR] crc32   @0x%08lX = 0x%08lX\n", (unsigned long)a_crc,   (unsigned long)ctx.rec.crc32);


        char pass_mask[32];
        if (g_nv_cfg.pass_len == 0) {
            // boş
            pass_mask[0] = '('; pass_mask[1] = 'e'; pass_mask[2] = 'm'; pass_mask[3] = 'p';
            pass_mask[4] = 't'; pass_mask[5] = 'y'; pass_mask[6] = ')'; pass_mask[7] = '\0';
        } else if (g_nv_cfg.pass_len >= 4) {
            pass_mask[0] = g_nv_cfg.pass[0];
            pass_mask[1] = g_nv_cfg.pass[1];
            pass_mask[2] = '*'; pass_mask[3] = '*'; pass_mask[4] = '*';
            pass_mask[5] = g_nv_cfg.pass[g_nv_cfg.pass_len - 2];
            pass_mask[6] = g_nv_cfg.pass[g_nv_cfg.pass_len - 1];
            pass_mask[7] = '\0';
        } else {

            size_t m = (g_nv_cfg.pass_len < sizeof(pass_mask) - 1) ? g_nv_cfg.pass_len : (sizeof(pass_mask) - 1);
            for (size_t i = 0; i < m; i++) pass_mask[i] = '*';
            pass_mask[m] = '\0';
        }

        logInfo("CFG[PL]  flag    @0x%08lX = %u\n",      (unsigned long)a_flag,  (unsigned)g_nv_cfg.first_online_flag);
        logInfo("CFG[PL]  serial  @0x%08lX = \"%s\"\n",  (unsigned long)a_serial, g_nv_cfg.serial);
        logInfo("CFG[PL]  ssid    @0x%08lX = \"%s\"\n",  (unsigned long)a_ssid,   g_nv_cfg.ssid);
        logInfo("CFG[PL]  passlen @0x%08lX = %u\n",      (unsigned long)a_passlen,(unsigned)g_nv_cfg.pass_len);
        logInfo("CFG[PL]  pass_ct @0x%08lX = %s\n",      (unsigned long)a_passct, pass_mask);
        logInfo("CFG[PL]  fw_ver  @0x%08lX = %lu\n",     (unsigned long)a_fwver,  (unsigned long)g_nv_cfg.version);
    } else {
    	logInfo("CFG: no active slot (defaults in RAM)\n");
    }
    return NV_CFG_OK;
}


nv_cfg_result_t nv_cfg_set_first_online_flag_and_commit(bool flag)
{
    nv_cfg_ctx_t ctx;
    nv_cfg_result_t r = nv_cfg_init(&ctx);
    if (r != NV_CFG_OK) return r;

    ctx.rec.first_online_flag = flag ? 1u : 0u;

    r = nv_cfg_commit(&ctx);
    if (r != NV_CFG_OK) return r;

    nv_cfg_load_globals();

    return NV_CFG_OK;
}


nv_cfg_result_t nv_cfg_save_wifi_and_reload_globals(const char *ssid, const char *password)
{
    /* ---- Parametre kontrolü ---- */
    logInfo("save_wifi: called (ssid=%s, pass_ptr=%p)\r\n",
            ssid ? ssid : "(null)", (void*)password);

    if (!ssid || !password) {
        logInfo("save_wifi: null param (ssid=%p, pass=%p)\r\n", (void*)ssid, (void*)password);
        return NV_CFG_ERR_INVALID_PARAM;
    }

    /* ---- Uzunluk kontrolü ---- */
    size_t ssid_len = strnlen(ssid, NV_SSID_MAX_LEN);
    size_t pass_len = strnlen(password, NV_PASS_MAX_LEN);

    logInfo("save_wifi: ssid_len=%u pass_len=%u\r\n",
            (unsigned)ssid_len, (unsigned)pass_len);

    if (ssid_len == 0 || ssid_len >= NV_SSID_MAX_LEN) {
        logInfo("save_wifi: invalid ssid len=%u (max=%u)\r\n",
                (unsigned)ssid_len, (unsigned)NV_SSID_MAX_LEN);
        return NV_CFG_ERR_INVALID_PARAM;
    }
    if (pass_len >= NV_PASS_MAX_LEN) {
        logInfo("save_wifi: pass too long len=%u (max=%u)\r\n",
                (unsigned)pass_len, (unsigned)NV_PASS_MAX_LEN);
        return NV_CFG_ERR_INVALID_PARAM;
    }

    /* ---- Context init ---- */
    nv_cfg_ctx_t ctx;
    nv_cfg_result_t r = nv_cfg_init(&ctx);
    if (r != NV_CFG_OK) {
        logInfo("save_wifi: nv_cfg_init failed (%d)\r\n", r);
        return r;
    }

    logInfo("save_wifi: after init base=0x%08lX end=0x%08lX slot_size=0x%08lX\r\n",
            (unsigned long)ctx.base_addr,
            (unsigned long)ctx.end_addr,
            (unsigned long)ctx.slot_size);
    logInfo("save_wifi: slotA=0x%08lX slotB=0x%08lX active=0x%08lX seq=%lu valid=%u\r\n",
            (unsigned long)ctx.slotA_addr,
            (unsigned long)ctx.slotB_addr,
            (unsigned long)ctx.active_slot_addr,
            (unsigned long)ctx.active_seq,
            (unsigned)ctx.active_valid);

    /* ---- Yeni SSID + PASS yaz ---- */
    r = nv_cfg_set_wifi(&ctx, ssid, (const uint8_t*)password, (uint16_t)pass_len);
    if (r != NV_CFG_OK) {
        logInfo("save_wifi: set_wifi failed (%d)\r\n", r);
        return r;
    }

    /* Debug: RAM’deki kayıt içeriğini özetle */
    logInfo("save_wifi: ctx.rec.ssid=\"%s\" pass_len=%u seq=%lu magic=0x%08lX ver=%u len=%u\r\n",
            ctx.rec.ssid,
            (unsigned)ctx.rec.pass_len,
            (unsigned long)ctx.rec.seq,
            (unsigned long)ctx.rec.magic,
            (unsigned)ctx.rec.version,
            (unsigned)ctx.rec.length);

    /* ---- Flash'a commit ---- */
    logInfo("save_wifi: calling nv_cfg_commit...\r\n");
    r = nv_cfg_commit(&ctx);
    logInfo("save_wifi: nv_cfg_commit result=%d\r\n", r);
    if (r != NV_CFG_OK) {
        logInfo("save_wifi: commit failed (%d)\r\n", r);
        return r;   /* Burada şu an F446'da 4 (NV_CFG_ERR_FLASH_OP) görüyorsun */
    }

    /* ---- Globals yeniden yükle ---- */
    logInfo("save_wifi: calling nv_cfg_load_globals...\r\n");
    r = nv_cfg_load_globals();
    logInfo("save_wifi: nv_cfg_load_globals result=%d\r\n", r);
    if (r != NV_CFG_OK) {
        logInfo("save_wifi: load_globals failed (%d)\r\n", r);
        return r;
    }

    /* ---- Doğrulama ---- */
    if (strncmp(g_nv_cfg.ssid, ssid, NV_SSID_MAX_LEN) != 0 ||
        g_nv_cfg.pass_len != (uint16_t)pass_len ||
        (g_nv_cfg.pass_len && memcmp(g_nv_cfg.pass, password, pass_len) != 0)) {

        logInfo("save_wifi: verify mismatch\r\n");
        logInfo("save_wifi: g_nv_cfg.ssid=\"%s\" g_len=%u, expected_ssid=\"%s\" expected_len=%u\r\n",
                g_nv_cfg.ssid,
                (unsigned)g_nv_cfg.pass_len,
                ssid,
                (unsigned)pass_len);
        return NV_CFG_ERR_CRC_MISMATCH;
    }

    /* ---- Log: NVM'e yazılan değerler ---- */
    char pass_log[NV_PASS_MAX_LEN + 1];
    uint16_t pl = g_nv_cfg.pass_len;
    if (pl > NV_PASS_MAX_LEN) pl = NV_PASS_MAX_LEN;
    memcpy(pass_log, g_nv_cfg.pass, pl);
    pass_log[pl] = '\0';

    logInfo("NVM SSID: %s\r\n", g_nv_cfg.ssid);
    logInfo("NVM PASS: %s\r\n", pass_log);

    return NV_CFG_OK;
}




static nv_cfg_result_t flash_read(uint32_t addr, void *dst, uint32_t len)
{
    memcpy(dst, (const void *)addr, len);
    return NV_CFG_OK;
}


static uint32_t calc_crc32(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (uint32_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}


static bool slot_is_valid(uint32_t slot_addr, nv_cfg_record_t *out_hdr)
{
    nv_cfg_record_t tmp;
    if (flash_read(slot_addr, &tmp, sizeof(nv_cfg_record_t)) != NV_CFG_OK) return false;

    if (tmp.magic   != NV_CFG_MAGIC)   return false;
    if (tmp.version != NV_CFG_VERSION) return false;
    if (tmp.length  == 0 || tmp.length > sizeof(nv_cfg_record_t)) return false;
    if (tmp.pass_len > NV_PASS_MAX_LEN) return false;

    uint32_t stored_crc = tmp.crc32;
    nv_cfg_record_t tmp_for_crc = tmp;
    tmp_for_crc.crc32 = 0;

    uint32_t calc = calc_crc32(&tmp_for_crc, tmp.length);
    if (calc != stored_crc) return false;

    if (out_hdr) *out_hdr = tmp;
    return true;
}
nv_cfg_result_t nv_cfg_get_first_online_flag(nv_cfg_ctx_t *ctx, bool *out_flag)
{
    if (!ctx || !out_flag) return NV_CFG_ERR_INVALID_PARAM;
    if (!ctx->rec_loaded)  return NV_CFG_ERR_NO_VALID_SLOT;
    *out_flag = (ctx->rec.first_online_flag != 0);
    return NV_CFG_OK;
}


static void select_active_slot(nv_cfg_ctx_t *ctx)
{
    nv_cfg_record_t ha = {0}, hb = {0};
    bool va = slot_is_valid(ctx->slotA_addr, &ha);
    bool vb = slot_is_valid(ctx->slotB_addr, &hb);

    ctx->active_valid = false;
    ctx->active_seq   = 0;
    ctx->active_slot_addr = ctx->slotA_addr;

    if (va && vb) {
        if (hb.seq > ha.seq) {
            ctx->active_slot_addr = ctx->slotB_addr;
            ctx->active_seq = hb.seq;
        } else {
            ctx->active_slot_addr = ctx->slotA_addr;
            ctx->active_seq = ha.seq;
        }
        ctx->active_valid = true;
    } else if (va) {
        ctx->active_slot_addr = ctx->slotA_addr;
        ctx->active_seq = ha.seq;
        ctx->active_valid = true;
    } else if (vb) {
        ctx->active_slot_addr = ctx->slotB_addr;
        ctx->active_seq = hb.seq;
        ctx->active_valid = true;
    } else {

        ctx->active_valid = false;
        ctx->active_seq = 0;
        ctx->active_slot_addr = ctx->slotA_addr;
    }
}

/* === Public API === */
nv_cfg_result_t nv_cfg_init(nv_cfg_ctx_t *ctx)
{
	logInfo("CFG base=0x%08lX end=0x%08lX slot_size=0x%08lX\r\n",
                 (uint32_t)&__cfg_start, (uint32_t)&__cfg_end, (uint32_t)&__cfg_slot_size);

    uint32_t base = (uint32_t)&__cfg_start;
    uint32_t slot = (uint32_t)&__cfg_slot_size;
    logInfo("SlotA=0x%08lX SlotB=0x%08lX (expect A=0x08060000, B=0x08062000)\r\n",
            base, base + slot);

    if (!ctx) return NV_CFG_ERR_INVALID_PARAM;
    memset(ctx, 0, sizeof(*ctx));

    ctx->base_addr = (uint32_t)&__cfg_start;
    ctx->end_addr  = (uint32_t)&__cfg_end;
    ctx->slot_size = (uint32_t)&__cfg_slot_size;

    ctx->slotA_addr = ctx->base_addr;
    ctx->slotB_addr = ctx->base_addr + ctx->slot_size;

    select_active_slot(ctx);

    if (ctx->active_valid) {
        (void)flash_read(ctx->active_slot_addr, &ctx->rec, sizeof(nv_cfg_record_t));
        ctx->rec_loaded = true;
        return NV_CFG_OK;
    } else {
        ctx->rec.magic   = NV_CFG_MAGIC;
        ctx->rec.version = NV_CFG_VERSION;
        ctx->rec.length  = sizeof(nv_cfg_record_t);
        ctx->rec.seq     = 0;
        ctx->rec.crc32   = 0;

        ctx->rec.first_online_flag = 0;
        ctx->rec.pass_len = 0;
        memset(ctx->rec.serial, 0, NV_SERIAL_MAX_LEN);
        memset(ctx->rec.ssid,   0, NV_SSID_MAX_LEN);
        memset(ctx->rec.pass_ct,0, NV_PASS_MAX_LEN);

        ctx->rec.fw_version = 0;
        ctx->rec_loaded = true;
        return NV_CFG_OK;
    }
}


nv_cfg_result_t nv_cfg_get_serial(nv_cfg_ctx_t *ctx, char out_serial[NV_SERIAL_MAX_LEN])
{
    if (!ctx || !out_serial) return NV_CFG_ERR_INVALID_PARAM;
    if (!ctx->rec_loaded)    return NV_CFG_ERR_NO_VALID_SLOT;
    memcpy(out_serial, ctx->rec.serial, NV_SERIAL_MAX_LEN);
    return NV_CFG_OK;
}

nv_cfg_result_t nv_cfg_get_wifi(nv_cfg_ctx_t *ctx, char out_ssid[NV_SSID_MAX_LEN],
                                uint8_t out_pass_pt[NV_PASS_MAX_LEN], uint16_t *out_pass_len)
{
    if (!ctx || !out_ssid || !out_pass_pt || !out_pass_len) return NV_CFG_ERR_INVALID_PARAM;
    if (!ctx->rec_loaded) return NV_CFG_ERR_NO_VALID_SLOT;

    memcpy(out_ssid, ctx->rec.ssid, NV_SSID_MAX_LEN);
    *out_pass_len = ctx->rec.pass_len;
    if (ctx->rec.pass_len > 0) {
        memcpy(out_pass_pt, ctx->rec.pass_ct, ctx->rec.pass_len);
    }
    return NV_CFG_OK;
}

nv_cfg_result_t nv_cfg_set_first_online_flag(nv_cfg_ctx_t *ctx, bool flag)
{
    if (!ctx || !ctx->rec_loaded) return NV_CFG_ERR_INVALID_PARAM;
    ctx->rec.first_online_flag = flag ? 1u : 0u;
    return NV_CFG_OK;
}

nv_cfg_result_t nv_cfg_set_serial(nv_cfg_ctx_t *ctx, const char *serial)
{
    if (!ctx || !serial) return NV_CFG_ERR_INVALID_PARAM;
    if (!ctx->rec_loaded) return NV_CFG_ERR_NO_VALID_SLOT;

    memset(ctx->rec.serial, 0, NV_SERIAL_MAX_LEN);
    strncpy(ctx->rec.serial, serial, NV_SERIAL_MAX_LEN - 1);
    return NV_CFG_OK;
}


nv_cfg_result_t nv_cfg_set_wifi(nv_cfg_ctx_t *ctx, const char *ssid,
                                const uint8_t *pass_pt, uint16_t pass_len)
{
    if (!ctx || !ssid || !pass_pt) return NV_CFG_ERR_INVALID_PARAM;
    if (!ctx->rec_loaded) return NV_CFG_ERR_NO_VALID_SLOT;
    if (pass_len > NV_PASS_MAX_LEN) return NV_CFG_ERR_INVALID_PARAM;

    memset(ctx->rec.ssid, 0, NV_SSID_MAX_LEN);
    strncpy(ctx->rec.ssid, ssid, NV_SSID_MAX_LEN - 1);

    ctx->rec.pass_len = pass_len;
    memset(ctx->rec.pass_ct, 0, NV_PASS_MAX_LEN);
    if (pass_len) memcpy(ctx->rec.pass_ct, pass_pt, pass_len);

    return NV_CFG_OK;
}


static nv_cfg_result_t flash_erase_page(uint32_t addr)
{
    HAL_StatusTypeDef       st;
    FLASH_EraseInitTypeDef  erase;
    uint32_t                sector_error = 0;
    uint32_t                sector = 0;

    /* ---- Adres -> Sector eşlemesi (RM0390 Tablo 4) ----
     *
     * Sector 0: 0x0800 0000 - 0x0800 3FFF (16 KB)
     * Sector 1: 0x0800 4000 - 0x0800 7FFF (16 KB)
     * Sector 2: 0x0800 8000 - 0x0800 BFFF (16 KB)
     * Sector 3: 0x0800 C000 - 0x0800 FFFF (16 KB)
     * Sector 4: 0x0801 0000 - 0x0801 FFFF (64 KB)
     * Sector 5: 0x0802 0000 - 0x0803 FFFF (128 KB)
     * Sector 6: 0x0804 0000 - 0x0805 FFFF (128 KB)
     * Sector 7: 0x0806 0000 - 0x0807 FFFF (128 KB)
     */

    if      (addr < 0x08004000) sector = FLASH_SECTOR_0;
    else if (addr < 0x08008000) sector = FLASH_SECTOR_1;
    else if (addr < 0x0800C000) sector = FLASH_SECTOR_2;
    else if (addr < 0x08010000) sector = FLASH_SECTOR_3;
    else if (addr < 0x08020000) sector = FLASH_SECTOR_4;
    else if (addr < 0x08040000) sector = FLASH_SECTOR_5;
    else if (addr < 0x08060000) sector = FLASH_SECTOR_6;
    else if (addr < 0x08080000) sector = FLASH_SECTOR_7;
    else {
        logInfo("CFG[FLASH] erase: INVALID addr=0x%08lX\r\n", (unsigned long)addr);
        return NV_CFG_ERR_FLASH_OP;
    }

    logInfo("CFG[FLASH] erase: addr=0x%08lX -> sector=%lu\r\n",
            (unsigned long)addr, (unsigned long)sector);

    memset(&erase, 0, sizeof(erase));
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Banks        = FLASH_BANK_1;              /* F446RE tek bank */
    erase.Sector       = sector;
    erase.NbSectors    = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;     /* 2.7V - 3.6V */

    HAL_FLASH_Unlock();
    st = HAL_FLASHEx_Erase(&erase, &sector_error);
    HAL_FLASH_Lock();

    logInfo("CFG[FLASH] erase: st=%ld err=0x%08lX\r\n",
            (long)st, (unsigned long)sector_error);

    if (st != HAL_OK) {
        return NV_CFG_ERR_FLASH_OP;
    }
    return NV_CFG_OK;
}


static nv_cfg_result_t flash_program(uint32_t addr, const void *src, uint32_t len)
{
    const uint8_t     *p  = (const uint8_t *)src;
    HAL_StatusTypeDef  st;

    logInfo("CFG[FLASH] prog: addr=0x%08lX len=%lu\r\n",
            (unsigned long)addr, (unsigned long)len);

    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t word  = 0xFFFFFFFFu;
        uint32_t chunk = (len - i >= 4) ? 4 : (len - i);

        memcpy(&word, p + i, chunk);

        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word);
        if (st != HAL_OK) {
            HAL_FLASH_Lock();
            logInfo("CFG[FLASH] prog FAIL at 0x%08lX st=%ld\r\n",
                    (unsigned long)(addr + i), (long)st);
            return NV_CFG_ERR_FLASH_OP;
        }
    }
    HAL_FLASH_Lock();

    logInfo("CFG[FLASH] prog: OK\r\n");
    return NV_CFG_OK;
}



nv_cfg_result_t nv_cfg_commit(nv_cfg_ctx_t *ctx)
{
    if (!ctx || !ctx->rec_loaded) {
        logInfo("CFG commit: invalid ctx or !rec_loaded\r\n");
        return NV_CFG_ERR_INVALID_PARAM;
    }

    /* Aktif slot A ise hedef B, aktif B ise hedef A */
    uint32_t target = (ctx->active_slot_addr == ctx->slotA_addr) ?
                      ctx->slotB_addr : ctx->slotA_addr;

    nv_cfg_record_t tmp = ctx->rec;
    tmp.seq     = ctx->active_seq + 1;
    tmp.magic   = NV_CFG_MAGIC;
    tmp.version = NV_CFG_VERSION;
    tmp.length  = sizeof(nv_cfg_record_t);

    tmp.crc32 = 0;
    uint32_t crc = calc_crc32(&tmp, tmp.length);
    tmp.crc32 = crc;

    logInfo("CFG commit: target=0x%08lX old_active=0x%08lX seq=%lu->%lu len=%u crc=0x%08lX\r\n",
            (unsigned long)target,
            (unsigned long)ctx->active_slot_addr,
            (unsigned long)ctx->active_seq,
            (unsigned long)tmp.seq,
            (unsigned)tmp.length,
            (unsigned long)tmp.crc32);

    /* 1) Sector erase */
    nv_cfg_result_t er = flash_erase_page(target);
    logInfo("CFG commit: erase result=%d\r\n", er);
    if (er != NV_CFG_OK) {
        logInfo("CFG commit: ERASE FAILED\r\n");
        return er;
    }

    /* 2) Program */
    nv_cfg_result_t pr = flash_program(target, &tmp, sizeof(nv_cfg_record_t));
    logInfo("CFG commit: prog result=%d\r\n", pr);
    if (pr != NV_CFG_OK) {
        logInfo("CFG commit: PROGRAM FAILED\r\n");
        return pr;
    }

    /* 3) Verify */
    nv_cfg_record_t verify;
    flash_read(target, &verify, sizeof(nv_cfg_record_t));
    if (memcmp(&tmp, &verify, sizeof(nv_cfg_record_t)) != 0) {
        logInfo("CFG commit: VERIFY FAILED (memcmp mismatch)\r\n");
        return NV_CFG_ERR_FLASH_OP;
    }

    ctx->active_slot_addr = target;
    ctx->active_seq       = tmp.seq;
    ctx->active_valid     = true;
    ctx->rec              = tmp;

    logInfo("CFG commit: OK new_active=0x%08lX seq=%lu\r\n",
            (unsigned long)ctx->active_slot_addr,
            (unsigned long)ctx->active_seq);

    return NV_CFG_OK;
}


__attribute__((weak))
void port_get_unique_96bit(uint8_t out12[12]) {
#ifdef STM32F0xx
    const uint32_t *UID = (const uint32_t *)0x1FFFF7AC; /* F0 UID base */
    uint32_t w0 = UID[0], w1 = UID[1], w2 = UID[2];
    memcpy(&out12[0], &w0, 4);
    memcpy(&out12[4], &w1, 4);
    memcpy(&out12[8], &w2, 4);
#else
    memset(out12, 0, 12);
#endif
}
static void uuid_from_uid_crc32(const uint8_t uid12[12], uint8_t out16[16]) {
    uint32_t s1 = 0xA5A5A5A5u ^ calc_crc32(uid12, 12);
    uint32_t s2 = 0x3C3C3C3Cu ^ calc_crc32(uid12, 8);
    uint32_t s3 = 0x96969696u ^ calc_crc32(uid12+4, 8);
    uint32_t s4 = 0x5A5A5A5Au ^ calc_crc32(uid12+2, 10);

    memcpy(&out16[0],  &s1, 4);
    memcpy(&out16[4],  &s2, 4);
    memcpy(&out16[8],  &s3, 4);
    memcpy(&out16[12], &s4, 4);

    out16[6] = (out16[6] & 0x0F) | 0x40;
    out16[8] = (out16[8] & 0x3F) | 0x80;
}

void nv_id_format_uuid(const uint8_t u[16], char out[37]) {
    static const char *hex = "0123456789abcdef";
    int pos = 0, i;
    for (i = 0; i < 16; i++) {
        out[pos++] = hex[(u[i] >> 4) & 0xF];
        out[pos++] = hex[u[i] & 0xF];
        if (i==3 || i==5 || i==7 || i==9) out[pos++] = '-';
    }
    out[pos] = '\0';
}


void nv_id_format_short12(const uint8_t u[16], char out[13]) {
    static const char *hex = "0123456789abcdef";
    int pos = 0;
    for (int i = 10; i < 16; i++) { /* son 6 byte */
        out[pos++] = hex[(u[i] >> 4) & 0xF];
        out[pos++] = hex[u[i] & 0xF];
    }
    out[pos] = '\0';
}

nv_cfg_result_t nv_id_get_uuid(nv_cfg_ctx_t *ctx, uint8_t out[16]) {
    if (!ctx || !out) return NV_CFG_ERR_INVALID_PARAM;
    if (!ctx->rec_loaded) return NV_CFG_ERR_NO_VALID_SLOT;
    memcpy(out, ctx->rec.dev_uuid, 16);
    return NV_CFG_OK;
}


nv_cfg_result_t nv_id_set_uuid_and_commit(const uint8_t uuid[16]) {
    if (!uuid) return NV_CFG_ERR_INVALID_PARAM;

    nv_cfg_ctx_t ctx;
    nv_cfg_result_t r = nv_cfg_init(&ctx);
    if (r != NV_CFG_OK) return r;

    memcpy(ctx.rec.dev_uuid, uuid, 16);

    r = nv_cfg_commit(&ctx);
    if (r == NV_CFG_OK) {
        nv_cfg_load_globals();
    }
    return r;
}

nv_cfg_result_t nv_id_ensure_uuid(void) {
    nv_cfg_ctx_t ctx;
    nv_cfg_result_t r = nv_cfg_init(&ctx);
    if (r != NV_CFG_OK) return r;

    bool empty = true;
    for (int i=0;i<16;i++) {
        if (ctx.rec.dev_uuid[i] != 0x00 && ctx.rec.dev_uuid[i] != 0xFF) { empty = false; break; }
    }
    if (!empty) {

        memcpy(g_nv_cfg.dev_uuid, ctx.rec.dev_uuid, 16);
        return NV_CFG_OK;
    }
    uint8_t uid12[12], uuid16[16];
    port_get_unique_96bit(uid12);
    uuid_from_uid_crc32(uid12, uuid16);
    return nv_id_set_uuid_and_commit(uuid16);
}
