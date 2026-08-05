/*
 * BLE'nin okuyup yazdigi sayac verisi arayuzu.
 *
 * ⚠️ Bu dosya, meter_port'un GERCEK/uretim portu tamamlandiktan sonra,
 * gercek veriye gore YENIDEN TASARLANDI - ble_meter_test'teki dummy
 * (meter_data_mock.c) surumuyle ayni ARAYUZU (fonksiyon isimlerini) buyuk
 * olcude koruyor ama BAZI ALANLAR DEGISTI/EKLENDI: alan seti artik
 * kartin GERCEKTEN urettigi kisa/uzun okuma ciktisina (readout-mode.py ile
 * dogrulanan) VE ek olarak protokolde hic olmayan "kart durumu" bilgilerine
 * (uptime, bos bellek, ADC ornekleme hizi, LED/watchdog durumu) gore
 * kuruldu - "testte oyle yaptik diye ayni format sart degil" karariyla.
 *
 * Gercek implementasyon: ble/src/meter_data_real.c (meter_data_mock.c
 * artik meter_port'ta KULLANILMIYOR, sadece ble_meter_test'te duruyor).
 */
#ifndef METER_DATA_H
#define METER_DATA_H

/* Includes */
#include <stdint.h>
#include <stdbool.h>

/* Defines */
#define METER_DATA_TASK_PERIOD (1000 / portTICK_PERIOD_MS)

/* --- Meter Info: salt okunur + yazilabilir degerler (gercek -rms ciktisindaki
 * alanlarla birebir ayni) --- */
const char *get_threshold_str(void);
void set_threshold_str(const uint8_t *data, uint16_t len);

const char *get_calibration_str(void);
void set_calibration_str(const uint8_t *data, uint16_t len);

const char *get_load_profile_period_str(void);
void set_load_profile_period_str(const uint8_t *data, uint16_t len);

// ⚠️ Artik tamamen salt okunur - gercek protokolde kalici/degistirilebilir
// bir "varsayilan baud" kavrami yok (bkz. meter_data_real.c), yaziya
// izin vermek yaniltici oluyordu.
const char *get_baud_rate_str(void);

/* RTC saati - ⚠️ artik YAZILABILIR (kullanicinin istegiyle eklendi): RTC
 * gelistirme sirasinda rastgele bir test degerine ayarlanmisti, gercek
 * saatle eslesmiyordu - bu, koda tekrar saat gomup yeniden flaslamadan
 * duzeltebilmek icin kalici cozum. Format: "YYYY-MM-DD HH:MM:SS" (get ile
 * ayni format, round-trip uyumlu). */
const char *get_rtc_time_str(void);
void set_rtc_time_str(const uint8_t *data, uint16_t len);

/* Salt okunur - gercek -rms ciktisindaki sabit alanlar */
const char *get_serial_number_str(void);
const char *get_firmware_version_str(void);
const char *get_production_date_str(void);

/* --- Meter Live: read + notify, gercek cihazdaki gibi VRMS max/min/ortalama
 * (32.7.0/52.7.0/72.7.0) - tek "canli" deger degil --- */
const char *get_vrms_max_str(void);
const char *get_vrms_min_str(void);
const char *get_vrms_mean_str(void);
void update_meter_live_data(void);

/* YENI (kullanicinin istegiyle, RS485/protokolde karsiligi yok): periyodik
 * max/min/mean'in disinda, her hesaplama penceresinde (saniyede birkac
 * kere) taze VRMS degerini de anlik olarak veren, ayri read+notify alan. */
const char *get_vrms_instant_str(void);

/* --- Meter Control: kisa/uzun okuma komutlari + gecmis kayit (RS485'teki
 * -rms/-rml ile birebir ayni veri, ayni "T,slot,tarih,saat,vrms,varyans;
 * R,slot,tarih,saat" formatinda) --- */
const char *get_load_history_str(void);
void trigger_short_read(void);
void trigger_long_read(void);

/* --- YENI (kullanicinin istegiyle): gercek modem/okuyucu gibi tarih
 * aralikli load profile sorgusu - RS485'teki "P.01(start;end)" mekanizmasinin
 * (spiflash.c/get_record_indexes, dev'de GERCEKTEN kullanildigi dogrulanmis)
 * BLE karsiligi. Web sayfasindaki takvim, hangi gunlerin secilebilir
 * oldugunu gormek icin get_load_profile_dates_str()'i, secilen aralik icin
 * de "LP:YY-MM-DD;YY-MM-DD" komutunu (Meter Control'un komut characteristic'i
 * uzerinden) kullanir. --- */
// flash'taki TUM load profile kayitlarinin FARKLI tarihlerini dondurur
// ("YY-MM-DD,YY-MM-DD,..." formatinda) - takvimde aktif/soluk gun ayrimi icin.
const char *get_load_profile_dates_str(void);
// En son "LP:..." sorgusunun sonucunu dondurur.
const char *get_load_profile_query_result_str(void);
// "YY-MM-DD;YY-MM-DD" formatindaki bir BLE komutunu ayristirip sorguyu
// calistirir, sonucu get_load_profile_query_result_str() icin hazirlar.
void trigger_load_profile_query(const uint8_t *data, uint16_t len);

/* --- Meter Status (YENI - RS485/protokolde hic yok, sadece BLE'ye ozel):
 * calisma suresi, bos bellek, ADC ornekleme hizi/pencere bilgisi,
 * LED/gorev sagligi durumu. Salt okunur, "yenile" ile taze okunuyor. --- */
const char *get_uptime_str(void);
const char *get_free_heap_str(void);
const char *get_adc_rate_str(void);
const char *get_led_status_str(void);
/* Bos bellek son kontrolden beri GERCEKTEN degistiyse true doner (gereksiz
 * BLE bildirimi/trafigi olmasin diye) - gatt_svc.c'nin periyodik
 * send_status_indication() cagrisinda kullanilir. */
bool free_heap_changed_since_last_check(void);

/* --- Yonetim komutlari (YENI, kullanicinin istegiyle eklendi): web
 * sayfasindaki "Varsayilan Ayarlara Sifirla" ve gecmis kayit "Sil"
 * butonlarindan (ikisi de ONCE ONAY ISTIYOR) Meter Control'un komut
 * characteristic'i uzerinden tetikleniyor. --- */
void reset_to_defaults(void);
void clear_threshold_history(void);
void clear_reset_history(void);

/* nvs_flash_init() sonrasi bir kere cagrilir - yazilabilir alanlari NVS'ten
 * (varsa) okuyup RAM tamponlarina yukler, kalicilik saglar. */
void meter_data_load_from_nvs(void);

#endif // METER_DATA_H
