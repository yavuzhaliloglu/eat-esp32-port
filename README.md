# ESP32-C3 Elektrik Sayacı Portu

Bu repo, RP2040 + FreeRTOS ile yazılmış bir hat gerilimi izleme cihazının (elektrik sayacı) firmware'ini ESP32-C3-MINI-1'e taşıdığımız staj projesinin son hali. Cihaz hat gerilimini ADC ile örnekleyip VRMS/varyans hesaplıyor, sonuçları flash belleğe kaydediyor ve IEC 62056-21'e benzer bir protokolle RS485 üzerinden bir okuyucuya/modeme raporluyor.

Orijinal cihaz RP2040 (çift çekirdek) üzerinde çalışıyordu, görevimiz bunu tek çekirdekli ESP32-C3'e; protokolü ve donanımın dışarıya verdiği davranışı bozmadan taşımaktı. Buna ek olarak, portun üzerine yeni bir BLE özelliği (telefon/tarayıcıdan cihaza bağlanıp veri okuma ve bazı ayarları değiştirme) ekledik.

## Klasör yapısı

```
esp32-meter-port/
│
├── esp32-port/                 ESP32-C3 üzerinde çalışan asıl firmware (bu projenin ana çıktısı)
│   ├── main/
│   │   ├── main.c              FreeRTOS görev (task) mimarisi, app_main
│   │   ├── header/              .h dosyaları (adc, rtc, uart, spiflash, bcc, fifo, mutex, print, defines, project_conf, project_globals)
│   │   ├── src/                 header'lara karşılık gelen .c dosyaları
│   │   └── ble/                 BLE / NimBLE GATT sunucusu
│   │       ├── include/
│   │       └── src/
│   ├── CMakeLists.txt
│   ├── partitions.csv           Flash partition haritası
│   └── sdkconfig(.defaults)
│
├── rp2040-original/            Port edilirken referans aldığımız orijinal RP2040 kodu (karşılaştırma için duruyor)
│   ├── main.c
│   ├── header/                  .h dosyaları (+ md5.c/h, eski OTA kalıntısı)
│   └── testfiles/               Orijinal cihazı test etmek için kullanılan Python script'leri (readout-mode.py, programming-mode.py, ...)
│       └── field_logger.py       Saha testi icin surekli RS485 okuma + Excel'e kayit script'i (port secimi, adaptor/cihaz kopmasi durumunda otomatik tekrar deneme, .venv icinde izole bagimliliklar)
│
└── web-ble/                    Telefon/tarayıcıdan BLE ile cihaza bağlanan Web Bluetooth sayfası
    ├── index.html
    ├── manifest.json / sw.js    PWA dosyaları
    └── icon.svg
```

`esp32-port/build/` klasörünü bilerek repoya koymadık (200MB+ derleme çıktısı, `idf.py build` ile yeniden üretiliyor).

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
- `esp32-port/` klasörünü ESP-IDF projesi olarak aç (VS Code ESP-IDF eklentisiyle çok daha sorunsuz - ham terminalden `idf.py` bir türlü doğru Python ortamını bulamadı, build/flash/monitor'ü hep VS Code üzerinden yaptık)
- Flash yöntemi UART olmalı (JTAG değil) - kart native USB-Serial-JTAG kullanıyor, manuel bir şey yapmaya gerek yok, `idf.py flash` otomatik giriyor
- İlk açılışta flash tam boş değilse (başka bir proje test etmiş olabilirsiniz) `idf.py erase-flash` çalıştırmak iyi olur, partition adresleri çakışırsa "boş" (0xFF) varsayımı yanlış çıkabiliyor

Web sayfası:
- Sadece Android + Chrome açıyor (Web Bluetooth API) - iOS/Safari desteklemiyor
- HTTPS üzerinden servis edilmesi lazım (Web Bluetooth localhost hariç HTTP'ye izin vermiyor) - biz Vercel'e deploy ettik, GitHub Pages ya da benzeri statik bir hosting de olur
- `web-ble/index.html` tek başına çalışan bir dosya, build adımı yok

Saha testi script'i (`field_logger.py`):
- `.venv/` bilerek repoya eklenmedi (kişiye/makineye özel), her klonda bir kere kurulması lazım. Önce klasöre gir: `cd rp2040-original/testfiles`, sonra işletim sistemine göre:

  **Mac / Linux (terminal):**
  ```
  python3 -m venv .venv
  ./.venv/bin/pip install -r requirements.txt
  ./.venv/bin/python3 field_logger.py
  ```

  **Windows (cmd):**
  ```
  python -m venv .venv
  .venv\Scripts\pip.exe install -r requirements.txt
  .venv\Scripts\python.exe field_logger.py
  ```

- CachyOS/Arch gibi dağıtımlarda sistem Python'una doğrudan `pip install` yapmak engellenmiş olabilir (PEP 668) - bu yüzden sanal ortam kullanıyoruz, sisteme hiç dokunmuyor

## Eski proje (RP2040) ne yapıyordu

RP2040 tarafında kod header-only bir yapıdaydı (fonksiyon gövdeleri `.h` dosyalarının içindeydi, ayrı `.c` dosyası yoktu). Çift çekirdek kullanılıyordu: bir çekirdek sadece ADC örnekleme yapıyordu, diğerinde ADC hesaplama + UART birlikteydi. Portu, projenin `master` branch'i değil `origin/dev` branch'i üzerinden yaptık - `master` daha eski/basit bir sürümdü, `dev`'de donanım watchdog, kesme tabanlı (interrupt-driven) UART okuma ve derleme zamanı özellik anahtarları (`CONF_*_ENABLED`) gibi üretime çok daha yakın parçalar vardı, bunları taşımak daha mantıklıydı.

Mutex'ler eski kodda `portMAX_DELAY` ile, yani süresiz bekliyordu - bir mutex hiç serbest kalmazsa sistem orada tıkanıp kalırdı.

## Neyi birebir taşıdık

- BCC hesaplama, FIFO, VRMS/varyans matematiği (`calculateVariance`, `getMean`) - mantık satır satır aynı, sadece Pico SDK → ESP-IDF include yolu farkları var
- Protokol state machine'i (Greeting → Setting → Listening → ReProgram), IEC 62056-21 mesaj formatı, OBIS kodları - birebir
- RTC register haritası ve BCD çevirme mantığı - birebir, sadece Pico'nun `i2c_write_blocking`/`i2c_read_blocking` API'si yerine ESP-IDF'in handle tabanlı `i2c_master_*` API'si kullanıldı
- Flash veri formatı (16 byte'lık sabit boyutlu kayıtlar - threshold, reset, load profile alanları) - birebir, ham SPI NOR komutları (`flash_range_erase` vs.) yerine `esp_partition` API'si kullanıldı

## Ne değişti, neden

**Watchdog** - RP2040 tarafında donanım watchdog + her kritik görevin kendi "hâlâ hayattayım" bitini işaretlediği bir bitmask deseni vardı, merkezi bir görev bunu kontrol edip donanım watchdog'unu besliyordu. ESP-IDF'in kendi Task Watchdog Timer'ı (`esp_task_wdt`) zaten aynı işi yapıyor - her kritik görev kendini `esp_task_wdt_add()` ile abone edip periyodik `esp_task_wdt_reset()` çağırıyor. Ayrı bir "WatchdogTask" yazmaya gerek kalmadı.

**UART** - RP2040 tarafında elle yazılmış bir kesme (ISR) rutini vardı, gelen baytları bir FreeRTOS MessageBuffer'a yazıyordu. ESP-IDF'in UART sürücüsü zaten donanım destekli RX tamponlaması yapıyor, bunun yerine görev seviyesinde (`uart_read_bytes` ile byte-byte okuyan) bir state machine yazıldı - mesaj sonu tespiti (LINE_FEED ya da ETX+BCC) aynı mantıkla çalışıyor, sadece ISR yerine bir görevin içinde.

**Görev/çekirdek dağılımı** - tek çekirdek olduğu için "hangi görev hangi çekirdekte" sorusu ortadan kalktı, önemli olan öncelik sıralaması oldu. RP2040'taki sıralamayı aynen koruduk: `ResetTask`(7) > `ADCSampleTask`(6) > `ADCReadTask`(5) = `WriteDebugTask/RTC`(5) > `UARTTask`(4) > `StatusLedTask`(1).

**ADC** - üretim kodu artık ham/doğrusal formül yerine `adc_cali_*` kalibrasyon API'sini kullanıyor, çünkü ESP32 serisi ADC'lerin bilinen bir doğrusal olmama (non-linearity) sorunu var. Bunun dışında iki önemli iyileştirme daha var (ikisi de ekip arkadaşımızın fiziksel testle doğrulayıp gönderdiği sürümden geldi):
- Bias referansı artık çapraz kanaldan (GPIO4'ün ortalaması) değil, her ölçüm penceresinin kendi ortalamasından çıkarılıyor (self-referencing/AC coupling) - GPIO4 artık sadece teşhis amaçlı okunuyor
- RMS hesabı artık 16 örneklik blok ortalamasıyla (decimation) yapılıyor
- Ölçüm penceresi sabit değil, `initADC()` içinde gerçek ölçülen örnekleme hızına göre 50Hz'in tam katı olacak şekilde dinamik hesaplanıyor - böylece pencere her zaman tam periyot sayısına denk geliyor, yarım periyot kesilmesi yok

**Mutex bekleme deseni** - eskiden süresizdi, artık her yerde 250ms timeout var, alınamazsa LED üzerinden bir hata deseni gösterip fonksiyon güvenli şekilde geri dönüyor - sonsuza kadar takılı kalma riski ortadan kalktı.

## Ne çıkarıldı, neden

- RP2040'ın eski RS485 tabanlı OTA'sı (`md5.c/h`, elle MD5 hesaplama, RP2040'a özel watchdog-scratch-register ile flash-swap) - zaten `dev` branch'inin kendisinden kaldırılmıştı, taşınacak çalışan bir implementasyon kalmamıştı (`master`'da hâlâ vardı, referans için oraya bakıldı). Yerine, aşağıda anlatılan **yeni ve tamamen farklı bir OTA** (BLE üzerinden, ESP32'nin kendi `esp_ota_ops` API'siyle) eklendi - eskisinin bire bir portu değil, sıfırdan.
- RP2040'a özel, Pico SDK'nın kendi dahili donanım RTC'sini ayarlayan ikinci bir zaman kaynağı (`rtc_set_datetime`) - ESP32'de buna gerek yok, tek zaman kaynağımız zaten harici PT7C4338 çipi

## Testler

Donanımı parça parça, ayrı küçük test projeleriyle doğruladık, sonra hepsini gerçek porta birleştirdik.

### Donanım bring-up testleri

| Parça | Sonuç |
|---|---|
| GPIO (LED) | GPIO7'de 500ms yanıp sönme doğrulandı, active-high |
| UART / RS485 | 9600 baud, PC ↔ kart iki yönlü test edildi (gerçek RS485 dönüştürücü üzerinden) |
| I2C + RTC | 0x68 adresinde bulundu, saat yazıldı/okundu, doğru ilerledi. Pil testi de yapıldı: USB tamamen çekilip birkaç dakika beklendi, saat sıfırlanmadı |
| Flash / partition | Kalıcılık testi (reset sonrası veri duruyor) + eşzamanlılık testi (iki görev aynı sektöre mutex korumalı yarışarak yazdı, 16/16 PASS) |
| ADC (attenuation, continuous mode) | `ADC_ATTEN_DB_12` (~2.45V nominal / ~2.88V ölçülen tam skala) kullanılıyor, sadece ADC1 güvenilir (ADC2 ESP32-C3'te errata'ya göre kararsız). Continuous/DMA modu 83kHz'e kadar destekliyor, bizim ihtiyacımız (~2kHz) için fazlasıyla yeterli - şu an basit oneshot+yield kullanılıyor, DMA moduna geçiş bir sonraki iyileştirme |

### Gerçek porta entegrasyon testi

Gerçek görev mimarisine (tüm görevler aynı anda çalışırken) geçtiğimizde eski `testfiles/readout-mode.py` script'iyle (RP2040 döneminden kalma, orijinal/resmi test aracı) karşılaştırma yaptık - bu, gerçek bir stack overflow hatası yakalamamıza sebep oldu: `send_reset_dates()` içinde 4096 byte'lık bir dizi stack üzerindeydi ama o görevin stack'i 2048 byte'tı. `static` yapıp stack'i 4096'ya çıkararak düzelttik, sonrasında BCC checksum'ları birebir eşleşti.

Ayrıca özel bir Python script'iyle gerçek bir IEC62056-21 okuyucusunu simüle edip kimlik doğrulama + kısa okuma isteği gönderdik - cevap tam ve doğru geldi, BCC checksum elle hesaplanıp cihazdan gelenle birebir eşleşti. Bu, `UARTTask`'ın en yüksek öncelikli `ADCSampleTask` ile aynı anda çalışırken protokolü bozmadan doğru cevap verdiğinin kanıtı.

### BLE testleri

Alt yapıyı önce izole, küçük test projeleriyle doğruladık:
- Beacon (sadece yayın, bağlantı yok) - telefon cihaz adını görebiliyor mu
- Bağlantı (connect/disconnect) - gerçek bağlanma/kopma
- GATT server (okuma/yazma/notify) - bir characteristic üzerinden hem veri okuma hem kartın gerçek LED'ini BLE'den açıp kapatma

Gerçek porta entegre ettikten sonra:
- Fiziksel boot testinde 4 GATT servisimiz (Meter Info, Meter Live, Meter Control, Meter Status) doğru characteristic sayısıyla kaydoldu, cihaz "METER-TEST" adıyla yayına başladı, BLE aktifken ADC/RTC/UART görevleri hiç kesintiye uğramadan çalışmaya devam etti
- nRF Connect (Android) ile okunabilir tüm alanlar tek tek doğrulandı - threshold, kalibrasyon sabiti, load profile periyodu, RTC saati, baud rate, seri no, firmware versiyonu, üretim tarihi, VRMS max/min/ortalama, kart durumu (uptime, boş bellek, ADC hızı, LED durumu) - hepsi gerçek cihaz durumuyla birebir eşleşti
- Kendi web sayfamızdan yazma tarafını test ettik: threshold/kalibrasyon/load profile periyodu/RTC saatinin gerçekten `ADCReadTask`'ın kullandığı aynı değişkenleri değiştirdiğini, reset sonrası kalıcı kaldığını, varsayılana sıfırlama ve geçmiş kayıt silme butonlarının (ikisi de onay penceresinden geçiyor) doğru çalıştığını doğruladık
- Tarih-aralıklı load profile sorgusu (takvim özelliği) kod olarak tamamlandı ama henüz fiziksel olarak test edilmedi - sıradaki adım bu

### ADC doğruluğu karşılaştırması

Bu, staj raporunun ana konusu: aynı bilinen test gerilimlerini (multimetreyle ölçülmüş) hem ham/doğrusal formülle hem `adc_cali_*` kalibrasyon API'siyle okuyup hata yüzdelerini karşılaştırmak. Bu kısmı ekip arkadaşımız fiziksel olarak yürütüyor, sonuçlar netleşince buraya eklenecek. Şimdilik bildiğimiz: `ADC_ATTEN_DB_12` kullanırken ham formülde hâlâ RP2040'tan kalma `3.3V` referans varsayımı var, oysa bu attenuation'ın gerçek tam skalası ~2.45-2.88V - yani ham formül kasıtlı olarak "eski platformdan düşünmeden kopyalarsan bu kadar sapma olur" senaryosunu temsil ediyor.

## Sık sorulan teknik sorular

**Flash'a yazarken ADC örneklemesi duruyor mu?**

İki farklı katman var. Yazılım tarafında `xFlashMutex` sadece flash'a erişen kod yollarını birbirine karşı koruyor (250ms timeout ile) - ADC örnekleme görevi flash'a hiç dokunmadığı için bu mutex'ten doğrudan etkilenmiyor. Ama donanım tarafında farklı bir gerçek var: ESP32'de flash'a yazma/silme sırasında, çalışan kodun kendisi de aynı SPI flash'tan execute edildiği için flash cache'i kısa süreliğine devre dışı kalıyor - bu da tek çekirdekte birkaç ms boyunca tüm görevlerin donmasına neden oluyor. `esp_partition` API'si bunu kendi içinde hallediyor, biz göremiyoruz/engelleyemiyoruz. Flash yazmaları sık değil (load profile periyodunda bir, ya da BLE'den bir ayar değiştiğinde), ve zaten decimasyon/pencere ortalaması bu birkaç ms'lik boşluğu absorbe ediyor. Osiloskopla kesin ölçüm yapmadık ama davranış bu şekilde açıklanıyor.

**"FLASH_MUTEX_ALINAMADI" ne zaman görünüyor?**

Bu, firmware'de bir hata değil - web sayfasındaki "Kart Durumu" ekranındaki "Sistem durumu" alanında görünebilen, cihazın kendi LED hata gösterge sisteminden gelen bir durum adı. Flash erişimi 250ms timeout'lu bir mutex ile korunuyor; bu süre içinde mutex alınamazsa (örn. cihaz kendi flash kaydını yaparken tam o anda BLE'den bir ayar değiştirilmeye çalışılırsa) bu durum tetikleniyor. Kendiliğinden temizlenmiyor - farklı bir durum gelene ya da cihaz resetlenene kadar ekranda öyle görünmeye devam ediyor. Yani ekranda bu yazıyı görmek "şu an aktif bir sorun var" anlamına gelmiyor, geçmişte bir kere tetiklenmiş ve o zamandan beri orada asılı kalmış olabilir.

**BLE'den bir değeri değiştirince gerçekten cihaz mı değişiyor, yoksa sadece ekranda mı görünüyor?**

Threshold, kalibrasyon sabiti, load profile periyodu ve RTC saati gerçekten `ADCReadTask`'ın kullandığı aynı global değişkenleri değiştiriyor, kalıcı (flash/NVS). Baud rate ise tamamen bilgi amaçlı gösteriliyor - gerçek protokolde her istekte yeniden pazarlık edildiği için kalıcı/değiştirilebilir bir "varsayılan baud" kavramı hiç yok, o yüzden BLE'de de salt okunur bıraktık.

## BLE / Web arayüzü

Cihaz "METER-TEST" adıyla yayın yapıyor, beş GATT servisi var:

| Servis | İçerik |
|---|---|
| Meter Info | threshold, kalibrasyon sabiti, load profile periyodu, RTC saati (hepsi okunur+yazılır) + baud rate, seri no, firmware versiyonu, üretim tarihi (salt okunur) |
| Meter Live | VRMS max/min/ortalama (load profile periyodunda güncellenir) + VRMS anlık (her ölçüm penceresinde güncellenir) |
| Meter Control | komut yazma (kısa/uzun okuma tetikleme, tarih-aralıklı load profile sorgusu, varsayılana sıfırlama, geçmiş kayıt silme) + geçmiş kayıt okuma |
| Meter Status | çalışma süresi, boş bellek, ADC örnekleme hızı, LED/görev sağlığı durumu - bunların hiçbiri RS485 protokolünde yok, sadece BLE'ye özel |
| Meter OTA | firmware güncelleme - komut (`START:<boyut>`/`FINISH`), veri (ham firmware parçaları), durum (okuma+notify) - detay aşağıda |

Şifre/eşleştirme yok - fiziksel yakınlık zaten doğal bir güvenlik sınırı olarak yeterli görüldü.

Web sayfası ekran/menü tabanlı: Kısa Okuma, Uzun Okuma, Kart Durumu. Uzun Okuma ekranında ayrıca bir takvim var - flash'ta gerçekten veri olan günler aktif/tıklanabilir görünüyor, olmayanlar soluk kalıyor, bir gün (veya aralık) seçince RS485'teki gerçek `P.01(start;end)` sorgusunun BLE karşılığı çalışıp o aralığın verilerini gösteriyor.

BLE'den gelen hiçbir veri `innerHTML` ile sayfaya eklenmiyor (hep `textContent`/DOM node) - eşleştirme olmadığı için "METER-TEST" adını taklit eden sahte bir cihaz kötü niyetli HTML/script gönderebilir, bunu kapatmak için. Sayfa PWA - bir kere internetle açılınca sonraki yenilemeler internet olmadan da çalışıyor.

## Firmware güncelleme (OTA)

Kartı yeniden USB'ye takmadan, BLE üzerinden yeni firmware yükleyebiliyoruz. Bunun için partition tablosu değişti - tek bir `factory` alanı yerine, ESP-IDF'in standart OTA düzeni:

| Partition | Ne işe yarıyor |
|---|---|
| `otadata` | Hangi yuvanın (aşağıdakilerden) aktif olduğunu tutan küçük (8KB) bir kayıt - **iki kopya** halinde (sıra numarası + checksum ile), tek kopya bozulsa bile diğeri geçerliliğini korur |
| `ota_0` / `ota_1` | İki ayrı 1MB'lık uygulama alanı - cihaz her zaman birinden çalışıyor, güncelleme DİĞERİNE yazılıyor |

**Akış**: web sayfasından `.bin` dosyası seçilip onaylanınca, dosya küçük parçalar halinde BLE'den gönderiliyor (`esp_ota_write`), bitince ESP-IDF kendi bütünlük kontrolünü yapıyor (`esp_ota_end` - checksum/imza doğrulaması, elle MD5 hesaplamamıza gerek yok) ve başarılıysa "bundan sonra diğer yuvadan başla" diye işaretlenip (`esp_ota_set_boot_partition`) cihaz resetleniyor.

**Bootloader nasıl karar veriyor**: ESP-IDF'in kendi 2. aşama bootloader'ı (biz yazmadık, framework'ün standart parçası) her açılışta `otadata`'daki iki kopyayı okuyup checksum'ı geçerli olan ve sıra numarası büyük olanı esas alıyor, seçtiği yuvanın imaj başlığını doğruluyor, sonra oraya atlıyor. Ayrıca **otomatik geri alma (rollback)** açık (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`): yeni yazılan yuva ilk açılışta "onaylanmamış" durumda başlıyor, kod `esp_ota_mark_app_valid_cancel_rollback()`'e (bizde `app_main()`'in sonunda) kadar sorunsuz gelirse "sağlıklı" işaretleniyor - gelemezse (çöker/sürekli resetlenirse), bir sonraki açılışta bootloader kendiliğinden bir önceki (bilinen sağlam) yuvaya dönüyor. Yani yarım/bozuk bir güncelleme cihazı asla kalıcı olarak bozamıyor.

**Geliştirirken bulup düzelttiğimiz iki gerçek hata**:
- `writeThresholdRecord()`'daki mutex'in kendi kendine ikinci kez kilitlenmesi gibi, burada da BLE bağlantısı transfer ortasında koparsa (kullanıcı iptal etsin, sinyal gitsin fark etmez) OTA durumu hiç sıfırlanmıyordu - bir sonraki bağlantıda yeni deneme sürekli reddediliyordu. Artık bağlantı koptuğunda (`esp_ota_abort()` ile) otomatik temizleniyor.
- Web tarafında, dosya seçme ekranı (`<input type=file>`) açılınca Android sayfayı "arka plana" alıyor - sayfa bunu "kullanıcı ayrıldı" sanıp BLE bağlantısını kendi kendine kesiyordu (`visibilitychange` olayı). Artık sadece sayfa gerçekten kapanırken/yenilenirken bağlantı kesiliyor, dosya seçme gibi geçici arka plana almalar etkilemiyor.

## Bilinen sınırlamalar / henüz bitmemiş işler

- Gerçek 220V hat gerilimiyle uçtan uca test henüz yapılmadı - güvenlik gereği bu bağlantı gözetimsiz kurulmuyor
- Düşük test gerilimlerinde (örn. 15-40V) doğru ölçüyor ama 220V civarında okunan değer ~170V'ta bir tavana vuruyor - şu an araştırılıyor, muhtemelen mevcut gerilim bölücü direnç oranının 220V'un tam genliğini ADC'nin güvenli aralığına sığdırmaya yetmediği (donanımsal, kod tarafında değil)
- iOS/Safari Web Bluetooth desteklemiyor - sahadaki teknisyenlerin hangi telefonu kullandığı henüz netleşmedi, bu önemli bir açık soru
- Seri numarası şu an derleme zamanında sabit kodlanmış bir placeholder - gerçek üretim stratejisi (NVS mi, efuse mi, cihaz başına ayrı derleme mi) henüz kararlaştırılmadı
- OTA kod olarak tamamlandı, farklı bir `.bin` ile uçtan uca fiziksel testi sürüyor - ayrıca güncelleme sürerken RS485 okuma/threshold yazma gibi işlemlerle aynı anda çalıştırılınca (veri bozulması beklenmiyor ama) zamanlama etkisi olup olmadığı henüz fiziksel olarak ölçülmedi
