#include <string.h>
#include <stdio.h>

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

#include "header/ota.h"

static const char *TAG = "ota";

static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_update_partition = NULL;
static uint32_t s_total_size = 0;
static uint32_t s_written_size = 0;
static bool s_in_progress = false;
static char s_status_buf[64] = "IDLE";

// ota_finish() basarili olunca, BLE bildirimi ("SUCCESS_REBOOTING") karsi
// tarafa ulassin diye, resetlemeyi hemen degil kisa bir gecikmeyle yapiyoruz.
static void reboot_timer_cb(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "OTA basarili, yeni yazilima gecmek icin resetleniyor...");
    esp_restart();
}

bool ota_begin(uint32_t total_size)
{
    if (s_in_progress)
    {
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:zaten devam ediyor");
        return false;
    }

    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (s_update_partition == NULL)
    {
        ESP_LOGE(TAG, "guncelleme icin uygun (bos/pasif) partition bulunamadi");
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:partition bulunamadi");
        return false;
    }

    ESP_LOGI(TAG, "OTA basliyor: hedef partition='%s' (0x%lx), boyut=%lu",
             s_update_partition->label, (unsigned long)s_update_partition->address, (unsigned long)total_size);

    if (total_size > s_update_partition->size)
    {
        ESP_LOGE(TAG, "gelen firmware (%lu byte) hedef yuvadan (%lu byte) buyuk!",
                 (unsigned long)total_size, (unsigned long)s_update_partition->size);
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:dosya cok buyuk");
        return false;
    }

    esp_err_t err = esp_ota_begin(s_update_partition, total_size, &s_ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin basarisiz: %s", esp_err_to_name(err));
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:baslatilamadi");
        return false;
    }

    s_total_size = total_size;
    s_written_size = 0;
    s_in_progress = true;
    snprintf(s_status_buf, sizeof(s_status_buf), "WRITING:0");
    return true;
}

bool ota_write_chunk(const uint8_t *data, uint16_t len)
{
    if (!s_in_progress)
    {
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:once START gonderilmeli");
        return false;
    }

    esp_err_t err = esp_ota_write(s_ota_handle, data, len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_write basarisiz: %s", esp_err_to_name(err));
        s_in_progress = false;
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:yazma hatasi");
        return false;
    }

    s_written_size += len;
    int pct = (s_total_size > 0) ? (int)(((uint64_t)s_written_size * 100) / s_total_size) : 0;
    if (pct > 100)
    {
        pct = 100;
    }
    snprintf(s_status_buf, sizeof(s_status_buf), "WRITING:%d", pct);
    return true;
}

bool ota_finish(void)
{
    if (!s_in_progress)
    {
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:aktif guncelleme yok");
        return false;
    }

    esp_err_t err = esp_ota_end(s_ota_handle);
    s_in_progress = false;

    if (err != ESP_OK)
    {
        // ESP-IDF burada imajin butunlugunu (checksum/imza) KENDI kontrol
        // ediyor - dev/master'daki elle MD5 karsilastirmasinin ESP32
        // karsiligi, ama biz hic kod yazmadan geliyor.
        ESP_LOGE(TAG, "esp_ota_end basarisiz (imaj gecersiz/eksik): %s", esp_err_to_name(err));
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:dogrulama basarisiz");
        return false;
    }

    err = esp_ota_set_boot_partition(s_update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition basarisiz: %s", esp_err_to_name(err));
        snprintf(s_status_buf, sizeof(s_status_buf), "ERROR:boot ayarlanamadi");
        return false;
    }

    ESP_LOGI(TAG, "OTA tamamlandi, '%s' bir sonraki acilista aktif olacak.", s_update_partition->label);
    snprintf(s_status_buf, sizeof(s_status_buf), "SUCCESS_REBOOTING");

    const esp_timer_create_args_t timer_args = {
        .callback = reboot_timer_cb,
        .name = "ota_reboot",
    };
    esp_timer_handle_t timer;
    if (esp_timer_create(&timer_args, &timer) == ESP_OK)
    {
        esp_timer_start_once(timer, 2000000); // 2 saniye - BLE bildirimi ulassin diye
    }
    else
    {
        // zamanlayici kurulamazsa bile hemen resetle, eski davranista kalma riski yok
        esp_restart();
    }

    return true;
}

void ota_abort(void)
{
    if (!s_in_progress)
    {
        return;
    }

    ESP_LOGW(TAG, "OTA yarida kesildi (baglanti koptu/iptal edildi), temizleniyor...");
    esp_ota_abort(s_ota_handle);
    s_in_progress = false;
    snprintf(s_status_buf, sizeof(s_status_buf), "IDLE");
}

const char *ota_get_status_str(void)
{
    return s_status_buf;
}

void ota_mark_valid(void)
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "esp_ota_mark_app_valid_cancel_rollback: %s (rollback zaten kapali/gerekmiyor olabilir)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Bu firmware saglikli olarak isaretlendi (rollback iptal edildi).");
    }
}
