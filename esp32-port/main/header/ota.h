#ifndef OTA_H
#define OTA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// BLE uzerinden firmware guncelleme (OTA) - ESP-IDF'in kendi esp_ota_ops
// API'sini kullanir. Cihaz su an calistigi yuvadan (ota_0/ota_1) FARKLI olan
// yuvaya yeni firmware'i yazar - yazma sirasinda ya da sonrasindaki dogrulamada
// (esp_ota_end, imzanin/checksum'in gecerliligini KENDI icinde kontrol eder)
// bir sorun cikarsa, su an CALISAN yazilim hic etkilenmez, cihaz bir sonraki
// acilista yine eski yazilimla kalir.
//
// dev/master'daki (RP2040) OTA'dan farki: orada MD5 hesaplama ve versiyon
// (epoch) karsilastirmasi elle yapiliyordu, flash-swap RP2040'a ozel watchdog
// scratch register numarasiyla tetikleniyordu. ESP32'de esp_ota_ops butun bunu
// (imaj bütünlüğü kontrolü dahil) kendi icinde hallediyor - elle MD5 yazmaya
// gerek yok.

// Yeni bir guncelleme baslatir - toplam byte boyutunu bilerek dogru
// partition'i (su an aktif OLMAYAN yuva) hazirlar. Basarisizsa false doner,
// ota_get_status_str() sebebi aciklar.
bool ota_begin(uint32_t total_size);

// Bir parca firmware verisini yazar (BLE karakteristiginden gelen ham bayt
// dizisi). ota_begin() basariyla cagrilmadan kullanilamaz.
bool ota_write_chunk(const uint8_t *data, uint16_t len);

// Yazmayi bitirir - ESP-IDF kendi butunluk kontrolunu yapar (esp_ota_end).
// Basarili olursa yeni yuvayi "bundan sonra buradan basla" olarak isaretler
// ve kisa bir gecikmeyle (BLE bildirimi ulassin diye) cihazi resetler.
bool ota_finish(void);

// Su anki durumu okunabilir bir metin olarak dondurur: "IDLE", "WRITING:45%%",
// "SUCCESS_REBOOTING", "ERROR:<sebep>" gibi. BLE'nin durum characteristic'i
// bunu okuyor.
const char *ota_get_status_str(void);

// app_main()'de, sistem saglikli sekilde baslatiktan HEMEN SONRA bir kere
// cagrilir - "bu firmware calisiyor, geri alma (rollback) gerekmiyor" diye
// isaretler. Bu cagrilmazsa ve rollback acobilirse, bootloader bir sonraki
// resette otomatik olarak ONCEKI yazilima doner (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE).
void ota_mark_valid(void);

#endif
