/*
 * Sayacin BLE GATT servisleri - dummy veriyle test.
 *
 * Uc servis:
 *  1) Meter Info    (read + write, bazilari salt okunur):
 *       threshold, kalibrasyon sabiti, load profile periyodu, baud rate
 *       (yazilabilir) + RTC saati, seri no, firmware versiyonu, uretim
 *       tarihi (salt okunur - gercek cihazin kisa okuma ciktisindaki sabit
 *       alanlar)
 *  2) Meter Live    (read + notify): VRMS max/min/ortalama (gercek cihazda
 *       32.7.0/52.7.0/72.7.0 OBIS kodlarina karsilik geliyor). Not: "hata
 *       kodu" kaldirildi - gercek -rms/-rml ciktisinda boyle bir alan yok,
 *       orijinal koda birebir uyumlu kalmak icin cikarildi.
 *  3) Meter Control (write + read/notify): komut (kisa/uzun okuma tetikleme),
 *                                          gecmis kayit (uzun okuma sonucu)
 *
 * Butun degerler basitlik icin ASCII metin olarak gonderiliyor (projenin
 * mevcut IEC 62056-21 protokolu de metin tabanli, ayni ruhla uyumlu).
 */
#include <stdlib.h>
#include "gatt_svc.h"
#include "common.h"
#include "meter_data.h"
#include "header/ota.h"

/* Salt okunur/yazilabilir bir characteristic'in getter+setter cifti.
 * setter == NULL ise write denemesi reddedilir (salt okunur alanlar icin). */
typedef struct {
    const char *(*getter)(void);
    void (*setter)(const uint8_t *data, uint16_t len);
} rw_field_t;

/* Private function declarations */
static int meter_info_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static int meter_live_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static int command_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ota_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ota_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

/* Proje ozel UUID tabani: "MTR-BLE-TEST" (ilk 12 byte), son 4 byte
 * servis/characteristic ayirt edici. */
#define METER_UUID_BASE \
    0x4d, 0x54, 0x52, 0x2d, 0x42, 0x4c, 0x45, 0x2d, 0x54, 0x45, 0x53, 0x54

/* --- Meter Info servisi --- */
static const ble_uuid128_t meter_info_svc_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x00, 0x00, 0x00);
static const ble_uuid128_t threshold_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x01, 0x00, 0x00);
static const ble_uuid128_t calibration_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x02, 0x00, 0x00);
static const ble_uuid128_t load_profile_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x03, 0x00, 0x00);
static const ble_uuid128_t rtc_time_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x04, 0x00, 0x00);
static const ble_uuid128_t baud_rate_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x05, 0x00, 0x00);
static const ble_uuid128_t serial_number_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x06, 0x00, 0x00);
static const ble_uuid128_t firmware_version_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x07, 0x00, 0x00);
static const ble_uuid128_t production_date_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x10, 0x08, 0x00, 0x00);

static uint16_t threshold_chr_val_handle;
static uint16_t calibration_chr_val_handle;
static uint16_t load_profile_chr_val_handle;
static uint16_t rtc_time_chr_val_handle;
static uint16_t baud_rate_chr_val_handle;
static uint16_t serial_number_chr_val_handle;
static uint16_t firmware_version_chr_val_handle;
static uint16_t production_date_chr_val_handle;

static const rw_field_t threshold_field = {get_threshold_str, set_threshold_str};
static const rw_field_t calibration_field = {get_calibration_str, set_calibration_str};
static const rw_field_t load_profile_field = {get_load_profile_period_str, set_load_profile_period_str};
/* ⚠️ artik yazilabilir - RTC saatini duzeltmek icin (bkz. meter_data.h) */
static const rw_field_t rtc_time_field = {get_rtc_time_str, set_rtc_time_str};
/* ⚠️ artik salt okunur - gercek protokolde kalici/degistirilebilir bir
 * "varsayilan baud" kavrami yok, yaziya izin vermek yaniltici oluyordu
 * (kullanici fark edip duzeltilmesini istedi). */
static const rw_field_t baud_rate_field = {get_baud_rate_str, NULL};
static const rw_field_t serial_number_field = {get_serial_number_str, NULL};
static const rw_field_t firmware_version_field = {get_firmware_version_str, NULL};
static const rw_field_t production_date_field = {get_production_date_str, NULL};

/* --- Meter Live servisi (read + notify) --- */
static const ble_uuid128_t meter_live_svc_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x20, 0x00, 0x00, 0x00);
static const ble_uuid128_t vrms_max_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x20, 0x01, 0x00, 0x00);
static const ble_uuid128_t vrms_min_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x20, 0x02, 0x00, 0x00);
static const ble_uuid128_t vrms_mean_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x20, 0x03, 0x00, 0x00);
/* YENI (kullanicinin istegiyle): periyodik max/min/mean'in disinda, HER
 * pencerede taze hesaplanan ham/anlik VRMS degeri. */
static const ble_uuid128_t vrms_instant_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x20, 0x04, 0x00, 0x00);

static uint16_t vrms_max_chr_val_handle;
static uint16_t vrms_min_chr_val_handle;
static uint16_t vrms_mean_chr_val_handle;
static uint16_t vrms_instant_chr_val_handle;

/* --- Meter Control servisi (komut yazma + gecmis kayit okuma/notify) --- */
static const ble_uuid128_t meter_control_svc_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x30, 0x00, 0x00, 0x00);
static const ble_uuid128_t command_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x30, 0x01, 0x00, 0x00);
static const ble_uuid128_t load_history_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x30, 0x02, 0x00, 0x00);
/* YENI (kullanicinin istegiyle): tarih-aralikli load profile sorgusu -
 * hangi gunlerde veri oldugunu (takvim icin) ve son sorgunun sonucunu veren
 * iki salt okunur alan. Sorgu, command characteristic'ine "LP:..." yazarak
 * tetikleniyor (SHORT/LONG ile ayni desen). */
static const ble_uuid128_t load_profile_dates_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x30, 0x03, 0x00, 0x00);
static const ble_uuid128_t load_profile_data_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x30, 0x04, 0x00, 0x00);

static uint16_t command_chr_val_handle;
static uint16_t load_history_chr_val_handle;
static uint16_t load_profile_dates_chr_val_handle;
static uint16_t load_profile_data_chr_val_handle;

/* --- Meter Status servisi (YENI - RS485/protokolde hic yok, sadece BLE'ye
 * ozel "kart durumu" bilgileri): calisma suresi, bos bellek, ADC ornekleme
 * hizi/pencere bilgisi, LED/gorev sagligi durumu. Cogu salt okunur, "yenile"
 * ile taze okunuyor - ama bos bellek (free_heap) kullanicinin istegiyle
 * read+notify oldu, deger degistikce anlik guncelleniyor (bkz. asagida
 * send_status_indication()). --- */
static const ble_uuid128_t meter_status_svc_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x40, 0x00, 0x00, 0x00);
static const ble_uuid128_t uptime_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x40, 0x01, 0x00, 0x00);
static const ble_uuid128_t free_heap_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x40, 0x02, 0x00, 0x00);
static const ble_uuid128_t adc_rate_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x40, 0x03, 0x00, 0x00);
static const ble_uuid128_t led_status_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x40, 0x04, 0x00, 0x00);

static uint16_t uptime_chr_val_handle;
static uint16_t free_heap_chr_val_handle;
static uint16_t adc_rate_chr_val_handle;
static uint16_t led_status_chr_val_handle;

/* --- Meter OTA servisi (YENI - BLE uzerinden firmware guncelleme):
 * control (write: "START:<toplam_byte>" / "FINISH"), data (write: ham
 * firmware baytlari, kucuk parcalar halinde), status (read+notify: "IDLE",
 * "WRITING:45", "SUCCESS_REBOOTING", "ERROR:<sebep>"). Is mantigi ota.c'de -
 * burada sadece BLE'ye baglama var. --- */
static const ble_uuid128_t meter_ota_svc_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x50, 0x00, 0x00, 0x00);
static const ble_uuid128_t ota_control_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x50, 0x01, 0x00, 0x00);
static const ble_uuid128_t ota_data_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x50, 0x02, 0x00, 0x00);
static const ble_uuid128_t ota_status_chr_uuid =
    BLE_UUID128_INIT(METER_UUID_BASE, 0x50, 0x03, 0x00, 0x00);

static uint16_t ota_control_chr_val_handle;
static uint16_t ota_data_chr_val_handle;
static uint16_t ota_status_chr_val_handle;
static bool ota_status_notify_enabled = false;

/* Notify abonelik durumu (tek merkez/telefon baglantisi varsayiliyor) */
static uint16_t meter_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool meter_conn_handle_inited = false;
static bool vrms_max_notify_enabled = false;
static bool vrms_min_notify_enabled = false;
static bool vrms_mean_notify_enabled = false;
static bool vrms_instant_notify_enabled = false;
/* ⚠️ YENI (kullanicinin istegiyle): bos bellek de anlik/notify ile
 * guncelleniyor, sadece "yenile"ye basinca degil. */
static bool free_heap_notify_enabled = false;

/* GATT services table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* Meter Info service */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &meter_info_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &threshold_chr_uuid.u,
              .access_cb = meter_info_chr_access,
              .arg = (void *)&threshold_field,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
              .val_handle = &threshold_chr_val_handle},
             {.uuid = &calibration_chr_uuid.u,
              .access_cb = meter_info_chr_access,
              .arg = (void *)&calibration_field,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
              .val_handle = &calibration_chr_val_handle},
             {.uuid = &load_profile_chr_uuid.u,
              .access_cb = meter_info_chr_access,
              .arg = (void *)&load_profile_field,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
              .val_handle = &load_profile_chr_val_handle},
             {.uuid = &rtc_time_chr_uuid.u,
              .access_cb = meter_info_chr_access,
              .arg = (void *)&rtc_time_field,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
              .val_handle = &rtc_time_chr_val_handle},
             {.uuid = &baud_rate_chr_uuid.u,
              .access_cb = meter_info_chr_access,
              .arg = (void *)&baud_rate_field,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &baud_rate_chr_val_handle},
             {.uuid = &serial_number_chr_uuid.u,
              .access_cb = meter_info_chr_access,
              .arg = (void *)&serial_number_field,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &serial_number_chr_val_handle},
             {.uuid = &firmware_version_chr_uuid.u,
              .access_cb = meter_info_chr_access,
              .arg = (void *)&firmware_version_field,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &firmware_version_chr_val_handle},
             {.uuid = &production_date_chr_uuid.u,
              .access_cb = meter_info_chr_access,
              .arg = (void *)&production_date_field,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &production_date_chr_val_handle},
             {0, /* No more characteristics in this service. */}}},

    /* Meter Live service - read + notify */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &meter_live_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &vrms_max_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_vrms_max_str,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &vrms_max_chr_val_handle},
             {.uuid = &vrms_min_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_vrms_min_str,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &vrms_min_chr_val_handle},
             {.uuid = &vrms_mean_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_vrms_mean_str,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &vrms_mean_chr_val_handle},
             {.uuid = &vrms_instant_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_vrms_instant_str,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &vrms_instant_chr_val_handle},
             {0, /* No more characteristics in this service. */}}},

    /* Meter Control service - komut yazma + gecmis kayit okuma/notify */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &meter_control_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &command_chr_uuid.u,
              .access_cb = command_chr_access,
              .flags = BLE_GATT_CHR_F_WRITE,
              .val_handle = &command_chr_val_handle},
             {.uuid = &load_history_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_load_history_str,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &load_history_chr_val_handle},
             {.uuid = &load_profile_dates_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_load_profile_dates_str,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &load_profile_dates_chr_val_handle},
             {.uuid = &load_profile_data_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_load_profile_query_result_str,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &load_profile_data_chr_val_handle},
             {0, /* No more characteristics in this service. */}}},

    /* Meter Status service - kart durumu (RS485/protokolde yok, sadece BLE) */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &meter_status_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &uptime_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_uptime_str,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &uptime_chr_val_handle},
             {.uuid = &free_heap_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_free_heap_str,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &free_heap_chr_val_handle},
             {.uuid = &adc_rate_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_adc_rate_str,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &adc_rate_chr_val_handle},
             {.uuid = &led_status_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)get_led_status_str,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &led_status_chr_val_handle},
             {0, /* No more characteristics in this service. */}}},

    /* Meter OTA service - BLE uzerinden firmware guncelleme */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &meter_ota_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &ota_control_chr_uuid.u,
              .access_cb = ota_control_chr_access,
              .flags = BLE_GATT_CHR_F_WRITE,
              .val_handle = &ota_control_chr_val_handle},
             {.uuid = &ota_data_chr_uuid.u,
              .access_cb = ota_data_chr_access,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
              .val_handle = &ota_data_chr_val_handle},
             {.uuid = &ota_status_chr_uuid.u,
              .access_cb = meter_live_chr_access,
              .arg = (void *)ota_get_status_str,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &ota_status_chr_val_handle},
             {0, /* No more characteristics in this service. */}}},

    {
        0, /* No more services. */
    },
};

/* Private functions */

/* Bir gelen BLE yazma isteginin baytlarini duz bir tampona kopyalar. */
static int copy_write_data(struct ble_gatt_access_ctxt *ctxt, uint8_t *buf, size_t buf_size, uint16_t *out_len) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len >= buf_size) {
        len = buf_size - 1;
    }
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, len, NULL);
    if (rc != 0) {
        return rc;
    }
    buf[len] = '\0';
    *out_len = len;
    return 0;
}

/* Meter Info: getter+setter cifti (arg olarak rw_field_t*) ile hem okuma hem
 * yazma destekliyor. Setter NULL ise (RTC saati, seri no, firmware
 * versiyonu, uretim tarihi gibi) yazma reddediliyor. */
static int meter_info_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const rw_field_t *field = (const rw_field_t *)arg;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: {
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
        }
        const char *value = field->getter();
        int rc = os_mbuf_append(ctxt->om, value, strlen(value));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        if (field->setter == NULL) {
            ESP_LOGE(TAG, "write rejected (salt okunur alan), attr_handle=%d", attr_handle);
            return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        }
        uint8_t buf[24];
        uint16_t len;
        int rc = copy_write_data(ctxt, buf, sizeof(buf), &len);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        ESP_LOGI(TAG, "characteristic write; conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
        field->setter(buf, len);
        return 0;
    }

    default:
        ESP_LOGE(TAG, "unexpected access operation to meter info characteristic, opcode: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* Meter Live / gecmis kayit: sadece okuma+notify, arg olarak dogrudan bir
 * getter fonksiyon pointer'i aliyor. */
static int meter_live_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        ESP_LOGE(TAG, "unexpected access operation to meter live characteristic, opcode: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
    }

    const char *(*getter)(void) = (const char *(*)(void))arg;
    const char *value = getter();
    int rc = os_mbuf_append(ctxt->om, value, strlen(value));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

/* Komut characteristic'i: "SHORT" -> kisa okuma, "LONG" -> uzun okuma */
static int command_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGE(TAG, "unexpected access operation to command characteristic, opcode: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }

    /* "LP:YY-MM-DD;YY-MM-DD" en uzun komut (21 karakter) - eski 16 byte'lik
     * tampon bunu kesiyordu, 32'ye buyutuldu. */
    uint8_t buf[32];
    uint16_t len;
    int rc = copy_write_data(ctxt, buf, sizeof(buf), &len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    ESP_LOGI(TAG, "command received: %s", (char *)buf);

    if (strcmp((char *)buf, "SHORT") == 0) {
        trigger_short_read();
        send_vrms_indication();
    } else if (strcmp((char *)buf, "LONG") == 0) {
        trigger_long_read();
    } else if (strcmp((char *)buf, "RESET_DEFAULTS") == 0) {
        /* Web sayfasi zaten telefonda onay istiyor - burada tekrar sormaya
         * gerek yok, komut geldiyse kullanici zaten onaylamis demektir. */
        reset_to_defaults();
    } else if (strcmp((char *)buf, "CLEAR_THRESHOLD") == 0) {
        clear_threshold_history();
    } else if (strcmp((char *)buf, "CLEAR_RESET") == 0) {
        clear_reset_history();
    } else if (strncmp((char *)buf, "LP:", 3) == 0) {
        trigger_load_profile_query(buf + 3, len - 3);
    } else {
        ESP_LOGE(TAG, "bilinmeyen komut: %s", (char *)buf);
    }

    return 0;
}

/* OTA kontrol: "START:<toplam_byte>" ile yeni bir guncelleme baslatir,
 * "FINISH" ile bitirir (basariliysa cihaz kisa bir sure sonra resetlenir). */
static int ota_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGE(TAG, "unexpected access operation to ota control characteristic, opcode: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint8_t buf[32];
    uint16_t len;
    int rc = copy_write_data(ctxt, buf, sizeof(buf), &len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    ESP_LOGI(TAG, "ota control command: %s", (char *)buf);

    if (strncmp((char *)buf, "START:", 6) == 0) {
        uint32_t total_size = (uint32_t)strtoul((char *)buf + 6, NULL, 10);
        ota_begin(total_size);
    } else if (strcmp((char *)buf, "FINISH") == 0) {
        ota_finish();
    } else {
        ESP_LOGE(TAG, "bilinmeyen ota komutu: %s", (char *)buf);
    }

    send_ota_status_indication();
    return 0;
}

/* OTA veri: firmware binary'sinin bir parcasi - dogrudan ota_write_chunk()'a
 * gecirilir. Metin degil ham bayt oldugu icin (char *)buf gibi string
 * fonksiyonlariyla islenmiyor. */
static int ota_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGE(TAG, "unexpected access operation to ota data characteristic, opcode: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }

    static uint8_t chunk_buf[512];
    uint16_t len;
    int rc = copy_write_data(ctxt, chunk_buf, sizeof(chunk_buf), &len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    ota_write_chunk(chunk_buf, len);
    send_ota_status_indication();
    return 0;
}

/* Public functions */
void send_vrms_indication(void) {
    if (meter_conn_handle_inited) {
        if (vrms_max_notify_enabled) {
            ble_gatts_notify(meter_conn_handle, vrms_max_chr_val_handle);
        }
        if (vrms_min_notify_enabled) {
            ble_gatts_notify(meter_conn_handle, vrms_min_chr_val_handle);
        }
        if (vrms_mean_notify_enabled) {
            ble_gatts_notify(meter_conn_handle, vrms_mean_chr_val_handle);
        }
        ESP_LOGI(TAG, "vrms max/min/mean notification sent!");
    }
}

/* YENI (kullanicinin istegiyle): send_vrms_indication()'dan AYRI - o sadece
 * periyodik (load profile periyodunda bir) VRMS max/min/mean icin, bu ise
 * ADCReadTask'in HER penceresinde (saniyede birkac kere) cagriliyor. */
void send_vrms_instant_indication(void) {
    if (meter_conn_handle_inited && vrms_instant_notify_enabled) {
        ble_gatts_notify(meter_conn_handle, vrms_instant_chr_val_handle);
    }
}

/* ⚠️ YENI (kullanicinin istegiyle): bos bellek degeri GERCEKTEN degistiyse
 * (free_heap_changed_since_last_check()) abone bir telefon varsa bildirim
 * gonderir - main.c'nin WriteDebugTask'inda (zaten 1sn'de bir calisan
 * gorev) periyodik olarak cagriliyor. */
void send_status_indication(void) {
    if (meter_conn_handle_inited && free_heap_notify_enabled) {
        if (free_heap_changed_since_last_check()) {
            ble_gatts_notify(meter_conn_handle, free_heap_chr_val_handle);
        }
    }
}

/* OTA durumu (IDLE/WRITING:%/SUCCESS_REBOOTING/ERROR:...) her komut/veri
 * parcasindan sonra tazelenip abone tarafa gonderiliyor - web sayfasi
 * ilerleme cubugunu bununla besliyor. */
void send_ota_status_indication(void) {
    if (meter_conn_handle_inited && ota_status_notify_enabled) {
        ble_gatts_notify(meter_conn_handle, ota_status_chr_val_handle);
    }
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {

    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    }

    meter_conn_handle = event->subscribe.conn_handle;
    meter_conn_handle_inited = true;

    if (event->subscribe.attr_handle == vrms_max_chr_val_handle) {
        vrms_max_notify_enabled = event->subscribe.cur_notify;
    } else if (event->subscribe.attr_handle == vrms_min_chr_val_handle) {
        vrms_min_notify_enabled = event->subscribe.cur_notify;
    } else if (event->subscribe.attr_handle == vrms_mean_chr_val_handle) {
        vrms_mean_notify_enabled = event->subscribe.cur_notify;
    } else if (event->subscribe.attr_handle == vrms_instant_chr_val_handle) {
        vrms_instant_notify_enabled = event->subscribe.cur_notify;
    } else if (event->subscribe.attr_handle == free_heap_chr_val_handle) {
        free_heap_notify_enabled = event->subscribe.cur_notify;
    } else if (event->subscribe.attr_handle == ota_status_chr_val_handle) {
        ota_status_notify_enabled = event->subscribe.cur_notify;
    }
}

void gatt_svr_reset_subscriptions(void) {
    meter_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    meter_conn_handle_inited = false;
    ota_status_notify_enabled = false;
    vrms_max_notify_enabled = false;
    vrms_min_notify_enabled = false;
    vrms_mean_notify_enabled = false;
    vrms_instant_notify_enabled = false;
    free_heap_notify_enabled = false;
}

int gatt_svc_init(void) {
    int rc = 0;

    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
