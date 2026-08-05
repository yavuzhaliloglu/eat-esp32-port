# ESP32-C3 Elektrik Sayacı Portu

Bu repo, RP2040 + FreeRTOS ile yazılmış bir hat gerilimi izleme cihazının (elektrik sayacı) firmware'ini ESP32-C3-MINI-1'e taşıdığım staj projesinin son hali. Cihaz hat gerilimini ADC ile örnekleyip VRMS/varyans hesaplıyor, sonuçları flash belleğe kaydediyor ve IEC 62056-21'e benzer bir protokolle RS485 üzerinden bir okuyucuya/modeme raporluyor.

Orijinal cihaz RP2040 (çift çekirdek) üzerinde çalışıyordu, benim görevim bunu tek çekirdekli ESP32-C3'e; protokolü ve donanımın dışarıya verdiği davranışı bozmadan taşımaktı. Buna ek olarak, portun üzerine yeni bir BLE özelliği (telefon/tarayıcıdan cihaza bağlanıp veri okuma ve bazı ayarları değiştirme) ekledim.

## Klasör yapısı

```
esp32-port/       ESP32-C3 üzerinde çalışan asıl firmware - bu projenin ana çıktısı
  main/
    main.c          FreeRTOS görev (task) mimarisi, app_main
    header/         .h dosyaları
    src/            .c dosyaları (adc, rtc, uart, spiflash, bcc, fifo, mutex, print, globals)
    ble/            BLE / NimBLE GATT sunucusu
  CMakeLists.txt
  partitions.csv    Flash partition haritası
  sdkconfig(.defaults)

rp2040-original/  Port edilirken referans aldığım orijinal RP2040 kodu (karşılaştırma için duruyor)

web-ble/          Telefon/tarayıcıdan BLE ile cihaza bağlanan Web Bluetooth sayfası
```

`esp32-port/build/` klasörünü bilerek repoya koymadım (200MB+ derleme çıktısı, `idf.py build` ile yeniden üretiliyor).

## Donanım

Kart: ESP32-C3-MINI-1, tek çekirdek RISC-V (~160MHz), BLE var (klasik Bluetooth değil), WiFi kullanmıyoruz - kablosuz tarafı tamamen BLE.

Pin haritası, eski karttan yeni karta:

| İş | RP2040 (eski) | ESP32-C3 (yeni) | Not |
|---|---|---|---|
| UART TX/RX (RS485) | GPIO0 / GPIO1 | TX=GPIO21, RX=GPIO20 | |
| Power/Activity LED | GPIO18 | GPIO7 | active-high (HIGH = yanık) |
| I2C SDA/SCL (RTC) | GPIO20 / GPIO21 | SDA=GPIO8, SCL=GPIO9 | |
| Reset pulse | GPIO2 | GPIO5 | |
| Threshold pin | GPIO17 | GPIO6 | eşik aşımında tetiklenen hat |
| ADC ana sinyal | GPIO26 | GPIO3 | |
| ADC bias/referans | GPIO27 | GPIO4 | artık sadece teşhis amaçlı okunuyor, RMS'e girmiyor |

RTC çipi PT7C4338 (DS1307 uyumlu), I2C adresi 0x68, register haritası eskisiyle birebir aynı.

## Derleme / çalıştırma

Firmware:
- ESP-IDF v6.0.2
- `esp32-port/` klasörünü ESP-IDF projesi olarak aç (VS Code ESP-IDF eklentisiyle çok daha sorunsuz - benim makinede ham terminalden `idf.py` bir türlü doğru Python ortamını bulamadı, build/flash/monitor'ü hep VS Code üzerinden yaptım)
- Flash yöntemi UART olmalı (JTAG değil) - kart native USB-Serial-JTAG kullanıyor, manuel bir şey yapmaya gerek yok, `idf.py flash` otomatik giriyor
- İlk açılışta flash tam boş değilse (başka bir proje test etmiş olabilirsiniz) `idf.py erase-flash` çalıştırmak iyi olur, partition adresleri çakışırsa "boş" (0xFF) varsayımı yanlış çıkabiliyor

Web sayfası:
- Sadece Android + Chrome açıyor (Web Bluetooth API) - iOS/Safari desteklemiyor
- HTTPS üzerinden servis edilmesi lazım (Web Bluetooth localhost hariç HTTP'ye izin vermiyor) - ben Vercel'e deploy ettim, GitHub Pages ya da benzeri statik bir hosting de olur
- `web-ble/index.html` tek başına çalışan bir dosya, build adımı yok

## Eski proje (RP2040) ne yapıyordu

RP2040 tarafında kod header-only bir yapıdaydı (fonksiyon gövdeleri `.h` dosyalarının içindeydi, ayrı `.c` dosyası yoktu). Çift çekirdek kullanılıyordu: bir çekirdek sadece ADC örnekleme yapıyordu, diğerinde ADC hesaplama + UART birlikteydi. Ben portu, projenin `master` branch'i değil `origin/dev` branch'i üzerinden yaptım - `master` daha eski/basit bir sürümdü, `dev`'de donanım watchdog, kesme tabanlı (interrupt-driven) UART okuma ve derleme zamanı özellik anahtarları (`CONF_*_ENABLED`) gibi üretime çok daha yakın parçalar vardı, bunları taşımak daha mantıklıydı.

Mutex'ler eski kodda `portMAX_DELAY` ile, yani süresiz bekliyordu - bir mutex hiç serbest kalmazsa sistem orada tıkanıp kalırdı.

## Neyi birebir taşıdım

- BCC hesaplama, FIFO, VRMS/varyans matematiği (`calculateVariance`, `getMean`) - mantık satır satır aynı, sadece Pico SDK → ESP-IDF include yolu farkları var
- Protokol state machine'i (Greeting → Setting → Listening → ReProgram), IEC 62056-21 mesaj formatı, OBIS kodları - birebir
- RTC register haritası ve BCD çevirme mantığı - birebir, sadece Pico'nun `i2c_write_blocking`/`i2c_read_blocking` API'si yerine ESP-IDF'in handle tabanlı `i2c_master_*` API'si kullanıldı
- Flash veri formatı (16 byte'lık sabit boyutlu kayıtlar - threshold, reset, load profile alanları) - birebir, ham SPI NOR komutları (`flash_range_erase` vs.) yerine `esp_partition` API'si kullanıldı

## Neyi değiştirdim, neden

**Watchdog** - RP2040 tarafında donanım watchdog + her kritik görevin kendi "hâlâ hayattayım" bitini işaretlediği bir bitmask deseni vardı, merkezi bir görev bunu kontrol edip donanım watchdog'unu besliyordu. ESP-IDF'in kendi Task Watchdog Timer'ı (`esp_task_wdt`) zaten aynı işi yapıyor - her kritik görev kendini `esp_task_wdt_add()` ile abone edip periyodik `esp_task_wdt_reset()` çağırıyor. Ayrı bir "WatchdogTask" yazmaya gerek kalmadı.

**UART** - RP2040 tarafında elle yazılmış bir kesme (ISR) rutini vardı, gelen baytları bir FreeRTOS MessageBuffer'a yazıyordu. ESP-IDF'in UART sürücüsü zaten donanım destekli RX tamponlaması yapıyor, ben bunun yerine görev seviyesinde (`uart_read_bytes` ile byte-byte okuyan) bir state machine yazdım - mesaj sonu tespiti (LINE_FEED ya da ETX+BCC) aynı mantıkla çalışıyor, sadece ISR yerine bir görevin içinde.

**Görev/çekirdek dağılımı** - tek çekirdek olduğu için "hangi görev hangi çekirdekte" sorusu ortadan kalktı, önemli olan öncelik sıralaması oldu. RP2040'taki sıralamayı aynen koruduk: `ResetTask`(7) > `ADCSampleTask`(6) > `ADCReadTask`(5) = `WriteDebugTask/RTC`(5) > `UARTTask`(4) > `StatusLedTask`(1).

**ADC** - üretim kodu artık ham/doğrusal formül yerine `adc_cali_*` kalibrasyon API'sini kullanıyor, çünkü ESP32 serisi ADC'lerin bilinen bir doğrusal olmama (non-linearity) sorunu var. Bunun dışında iki önemli iyileştirme daha var (ikisi de ekip arkadaşımın fiziksel testle doğrulayıp gönderdiği sürümden geldi):
- Bias referansı artık çapraz kanaldan (GPIO4'ün ortalaması) değil, her ölçüm penceresinin kendi ortalamasından çıkarılıyor (self-referencing/AC coupling) - GPIO4 artık sadece teşhis amaçlı okunuyor
- RMS hesabı artık 16 örneklik blok ortalamasıyla (decimation) yapılıyor
- Ölçüm penceresi sabit değil, `initADC()` içinde gerçek ölçülen örnekleme hızına göre 50Hz'in tam katı olacak şekilde dinamik hesaplanıyor - böylece pencere her zaman tam periyot sayısına denk geliyor, yarım periyot kesilmesi yok

**Mutex bekleme deseni** - eskiden süresizdi, artık her yerde 250ms timeout var, alınamazsa LED üzerinden bir hata deseni gösterip fonksiyon güvenli şekilde geri dönüyor - sonsuza kadar takılı kalma riski ortadan kalktı.

## Neyi çıkardım, neden

- OTA (ve `md5.c/h`) - zaten `dev` branch'inin kendisinden kaldırılmıştı, taşınacak bir şey kalmamıştı
- RP2040'a özel, Pico SDK'nın kendi dahili donanım RTC'sini ayarlayan ikinci bir zaman kaynağı (`rtc_set_datetime`) - ESP32'de buna gerek yok, tek zaman kaynağımız zaten harici PT7C4338 çipi

## Testler

Donanımı parça parça, ayrı küçük test projeleriyle doğruladım, sonra hepsini gerçek porta birleştirdim:

| Parça | Sonuç |
|---|---|
| GPIO (LED) | GPIO7'de 500ms yanıp sönme doğrulandı, active-high |
| UART / RS485 | 9600 baud, PC ↔ kart iki yönlü test edildi (gerçek RS485 dönüştürücü üzerinden) |
| I2C + RTC | 0x68 adresinde bulundu, saat yazıldı/okundu, doğru ilerledi. Pil testi de yapıldı: USB tamamen çekilip birkaç dakika beklendi, saat sıfırlanmadı |
| Flash / partition | Kalıcılık testi (reset sonrası veri duruyor) + eşzamanlılık testi (iki görev aynı sektöre mutex korumalı yarışarak yazdı, 16/16 PASS) |
| ADC (attenuation, continuous mode) | `ADC_ATTEN_DB_12` (~2.45V nominal / ~2.88V ölçülen tam skala) kullanılıyor, sadece ADC1 güvenilir (ADC2 ESP32-C3'te errata'ya göre kararsız). Continuous/DMA modu 83kHz'e kadar destekliyor, bizim ihtiyacımız (~2kHz) için fazlasıyla yeterli - şu an basit oneshot+yield kullanılıyor, DMA moduna geçiş bir sonraki iyileştirme |
| BLE | Beacon (yayın), bağlantı, GATT server (okuma/yazma/notify) ayrı ayrı doğrulandı, sonra gerçek entegrasyon nRF Connect ve kendi web sayfamla test edildi |

Gerçek görev mimarisine (tüm görevler aynı anda çalışırken) geçtiğimde eski `testfiles/readout-mode.py` script'iyle (RP2040 döneminden kalma, orijinal/resmi test aracı) karşılaştırma yaptım - bu, gerçek bir stack overflow hatası yakalamama sebep oldu: `send_reset_dates()` içinde 4096 byte'lık bir dizi stack üzerindeydi ama o görevin stack'i 2048 byte'tı. `static` yapıp stack'i 4096'ya çıkararak düzelttim, sonrasında BCC checksum'ları birebir eşleşti.

### ADC doğruluğu karşılaştırması

Bu, staj raporunun ana konusu: aynı bilinen test gerilimlerini (multimetreyle ölçülmüş) hem ham/doğrusal formülle hem `adc_cali_*` kalibrasyon API'siyle okuyup hata yüzdelerini karşılaştırmak. Bu kısmı ekip arkadaşım fiziksel olarak yürütüyor, sonuçlar netleşince buraya eklenecek. Şimdilik bildiğimiz: `ADC_ATTEN_DB_12` kullanırken ham formülde hâlâ RP2040'tan kalma `3.3V` referans varsayımı var, oysa bu attenuation'ın gerçek tam skalası ~2.45-2.88V - yani ham formül kasıtlı olarak "eski platformdan düşünmeden kopyalarsan bu kadar sapma olur" senaryosunu temsil ediyor.

## Sık sorulan teknik sorular

**Flash'a yazarken ADC örneklemesi duruyor mu?**

İki farklı katman var. Yazılım tarafında `xFlashMutex` sadece flash'a erişen kod yollarını birbirine karşı koruyor (250ms timeout ile) - ADC örnekleme görevi flash'a hiç dokunmadığı için bu mutex'ten doğrudan etkilenmiyor. Ama donanım tarafında farklı bir gerçek var: ESP32'de flash'a yazma/silme sırasında, çalışan kodun kendisi de aynı SPI flash'tan execute edildiği için flash cache'i kısa süreliğine devre dışı kalıyor - bu da tek çekirdekte birkaç ms boyunca tüm görevlerin donmasına neden oluyor. `esp_partition` API'si bunu kendi içinde hallediyor, biz göremiyoruz/engelleyemiyoruz. Flash yazmaları sık değil (load profile periyodunda bir, ya da BLE'den bir ayar değiştiğinde), ve zaten decimasyon/pencere ortalaması bu birkaç ms'lik boşluğu absorbe ediyor. Osiloskopla kesin ölçüm yapmadık ama davranış bu şekilde açıklanıyor.

**"FLASH_MUTEX_ALINAMADI" durumu ne zaman görünüyor?**

250ms içinde mutex alınamazsa bu LED hata deseni tetikleniyor. Bu durum kendiliğinden temizlenmiyor - farklı bir hata deseni gelene ya da cihaz resetlenene kadar öyle kalıyor. BLE üzerindeki "Kart Durumu" ekranından da görülebiliyor.

**BLE'den bir değeri değiştirince gerçekten cihaz mı değişiyor, yoksa sadece ekranda mı görünüyor?**

Threshold, kalibrasyon sabiti, load profile periyodu ve RTC saati gerçekten `ADCReadTask`'ın kullandığı aynı global değişkenleri değiştiriyor, kalıcı (flash/NVS). Baud rate ise tamamen bilgi amaçlı gösteriliyor - gerçek protokolde her istekte yeniden pazarlık edildiği için kalıcı/değiştirilebilir bir "varsayılan baud" kavramı hiç yok, o yüzden BLE'de de salt okunur bıraktım.

## BLE / Web arayüzü

Cihaz "METER-TEST" adıyla yayın yapıyor, dört GATT servisi var:

| Servis | İçerik |
|---|---|
| Meter Info | threshold, kalibrasyon sabiti, load profile periyodu, RTC saati (hepsi okunur+yazılır) + baud rate, seri no, firmware versiyonu, üretim tarihi (salt okunur) |
| Meter Live | VRMS max/min/ortalama (load profile periyodunda güncellenir) + VRMS anlık (her ölçüm penceresinde güncellenir) |
| Meter Control | komut yazma (kısa/uzun okuma tetikleme, tarih-aralıklı load profile sorgusu, varsayılana sıfırlama, geçmiş kayıt silme) + geçmiş kayıt okuma |
| Meter Status | çalışma süresi, boş bellek, ADC örnekleme hızı, LED/görev sağlığı durumu - bunların hiçbiri RS485 protokolünde yok, sadece BLE'ye özel |

Şifre/eşleştirme yok - fiziksel yakınlık zaten doğal bir güvenlik sınırı olarak yeterli görüldü.

Web sayfası ekran/menü tabanlı: Kısa Okuma, Uzun Okuma, Kart Durumu. Uzun Okuma ekranında ayrıca bir takvim var - flash'ta gerçekten veri olan günler aktif/tıklanabilir görünüyor, olmayanlar soluk kalıyor, bir gün (veya aralık) seçince RS485'teki gerçek `P.01(start;end)` sorgusunun BLE karşılığı çalışıp o aralığın verilerini gösteriyor.

BLE'den gelen hiçbir veri `innerHTML` ile sayfaya eklenmiyor (hep `textContent`/DOM node) - eşleştirme olmadığı için "METER-TEST" adını taklit eden sahte bir cihaz kötü niyetli HTML/script gönderebilir, bunu kapatmak için. Sayfa PWA - bir kere internetle açılınca sonraki yenilemeler internet olmadan da çalışıyor.

## Bilinen sınırlamalar / henüz bitmemiş işler

- Gerçek 220V hat gerilimiyle uçtan uca test henüz yapılmadı - güvenlik gereği bu bağlantı gözetimsiz kurulmuyor
- Düşük test gerilimlerinde (örn. 15-40V) doğru ölçüyor ama 220V civarında okunan değer ~170V'ta bir tavana vuruyor - şu an araştırılıyor, muhtemelen mevcut gerilim bölücü direnç oranının 220V'un tam genliğini ADC'nin güvenli aralığına sığdırmaya yetmediği (donanımsal, kod tarafında değil)
- iOS/Safari Web Bluetooth desteklemiyor - sahadaki teknisyenlerin hangi telefonu kullandığı henüz netleşmedi, bu önemli bir açık soru
- Seri numarası şu an derleme zamanında sabit kodlanmış bir placeholder - gerçek üretim stratejisi (NVS mi, efuse mi, cihaz başına ayrı derleme mi) henüz kararlaştırılmadı

## Güvenlik notu

Kart üzerinde 220V bağlamak için hazır bir konnektör yok, gerçek hat gerilimi bağlantısı asla gözetimsiz kurulmuyor - bu proje gerçek elektrik çarpması riski taşıyan bir donanım, "acaba olur mu" diye tek başına deneme yapılmıyor.
