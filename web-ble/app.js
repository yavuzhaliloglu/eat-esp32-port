const WEB_APP_VERSION = "10";
document.getElementById("appVersion").textContent = "Web v" + WEB_APP_VERSION;

// Sayfa/varliklarini onbellege alir ki internet olmadan yenilenince de
// gercek sayfa (ve calisan butonlar) acilsin.
if ("serviceWorker" in navigator) {
  window.addEventListener("load", () => {
    navigator.serviceWorker.register("/sw.js", { updateViaCache: "none" }).catch((err) => {
      console.warn("Service worker kaydi basarisiz:", err);
    });
  });
}

// project_conf.h'deki DEVICE_BLE_NAME ile uyumlu: METER-<seri numarasi>.
// Eski firmware'lerin METER-TEST adi da ayni onekle bulunur.
const METER_NAME_PREFIX = "METER-";

const METER_INFO_SVC = "00000010-5453-4554-2d45-4c422d52544d";
const METER_LIVE_SVC = "00000020-5453-4554-2d45-4c422d52544d";
const METER_CONTROL_SVC = "00000030-5453-4554-2d45-4c422d52544d";
const METER_STATUS_SVC = "00000040-5453-4554-2d45-4c422d52544d";
const METER_OTA_SVC = "00000050-5453-4554-2d45-4c422d52544d";

const CHR = {
  threshold:    "00000110-5453-4554-2d45-4c422d52544d",
  calibration:  "00000210-5453-4554-2d45-4c422d52544d",
  loadprofile:  "00000310-5453-4554-2d45-4c422d52544d",
  rtc:          "00000410-5453-4554-2d45-4c422d52544d",
  baud:         "00000510-5453-4554-2d45-4c422d52544d",
  serial:       "00000610-5453-4554-2d45-4c422d52544d",
  firmware:     "00000710-5453-4554-2d45-4c422d52544d",
  production:   "00000810-5453-4554-2d45-4c422d52544d",
  vrmsMax:      "00000120-5453-4554-2d45-4c422d52544d",
  vrmsMin:      "00000220-5453-4554-2d45-4c422d52544d",
  vrmsMean:     "00000320-5453-4554-2d45-4c422d52544d",
  vrmsInstant:  "00000420-5453-4554-2d45-4c422d52544d",
  command:      "00000130-5453-4554-2d45-4c422d52544d",
  history:      "00000230-5453-4554-2d45-4c422d52544d",
  lpDates:      "00000330-5453-4554-2d45-4c422d52544d",
  lpData:       "00000430-5453-4554-2d45-4c422d52544d",
  recordPage:   "00000530-5453-4554-2d45-4c422d52544d",
  parameterWrite: "00000630-5453-4554-2d45-4c422d52544d",
  uptime:       "00000140-5453-4554-2d45-4c422d52544d",
  freeHeap:     "00000240-5453-4554-2d45-4c422d52544d",
  adcRate:      "00000340-5453-4554-2d45-4c422d52544d",
  ledStatus:    "00000440-5453-4554-2d45-4c422d52544d",
  otaControl:   "00000150-5453-4554-2d45-4c422d52544d",
  otaData:      "00000250-5453-4554-2d45-4c422d52544d",
  otaStatus:    "00000350-5453-4554-2d45-4c422d52544d",
};

// Bir seferde BLE'ye yazilacak firmware parcasi boyutu (byte). Telefonlar
// arasi MTU farkliliklarina karsi güvenli/tutucu bir deger secildi - MTU
// pazarligi basarisiz olsa/dusuk kalsa bile calisir.
const OTA_CHUNK_SIZE = 200;

// ⚠️ "baud" buradan CIKARILDI: gercek protokolde baud rate her istekte
// yeniden pazarlik ediliyor, kalici/degistirilebilir bir "varsayilan baud"
// kavrami yok - yazilabilir birakmak, kullaniciya yaniltici bir kontrol
// hissi veriyordu (firmware tarafinda da ayni sekilde salt okunura cevrildi).
const EDITABLE_FIELDS = ["threshold", "calibration", "loadprofile", "rtc"];
// ESP'nin kalibrasyon yazma ve acilista okuma tamponu: 15 karakter + NUL.
const CALIBRATION_VALUE_MAX_LENGTH = 15;

const decoder = new TextDecoder("utf-8");
const encoder = new TextEncoder();

const connectBtn = document.getElementById("connectBtn");
const disconnectBtn = document.getElementById("disconnectBtn");
const statusDot = document.getElementById("statusDot");
const statusText = document.getElementById("statusText");
const errorBox = document.getElementById("errorBox");
const btnGoShort = document.getElementById("btnGoShort");
const btnGoLong = document.getElementById("btnGoLong");
const btnGoStatus = document.getElementById("btnGoStatus");
const btnGoOta = document.getElementById("btnGoOta");
const btnResetDefaults = document.getElementById("btnResetDefaults");
const refreshShort = document.getElementById("refreshShort");
const refreshLong = document.getElementById("refreshLong");
const refreshStatus = document.getElementById("refreshStatus");

let bleDevice = null;
let infoService = null;
let liveService = null;
let controlService = null;
let statusService = null;
let otaService = null;
let commandChr = null;
let recordPageChr = null;
let parameterWriteChr = null;
let parameterWritePending = false;
let recordReadQueue = Promise.resolve();
let otaSelectedFile = null;
let otaInProgress = false;
let otaSucceeded = false;
let otaAwaitingFinish = false;
let otaTransferError = "";

/* --- Ekran (view) gecisleri --- */
function showView(id) {
  document.querySelectorAll(".view").forEach((v) => v.classList.remove("active"));
  document.getElementById(id).classList.add("active");
}

function showError(msg) {
  errorBox.textContent = msg;
  errorBox.style.display = "block";
}

function clearError() {
  errorBox.style.display = "none";
}

const toast = document.getElementById("toast");
let toastTimer;
function showToast(message, success = false) {
  clearTimeout(toastTimer);
  toast.className = "toast " + (success ? "success" : "error");
  toast.setAttribute("role", success ? "status" : "alert");
  toast.setAttribute("aria-live", success ? "polite" : "assertive");
  document.getElementById("toastText").textContent = message;
  toast.hidden = false;
  toastTimer = setTimeout(() => { toast.hidden = true; }, success ? 6000 : 10000);
}
document.getElementById("toastClose").addEventListener("click", () => {
  clearTimeout(toastTimer);
  toast.hidden = true;
});

const passwordDialog = document.getElementById("passwordDialog");
const passwordForm = document.getElementById("passwordForm");
const devicePassword = document.getElementById("devicePassword");
const passwordCancel = document.getElementById("passwordCancel");
let cancelPasswordPrompt = null;

function askDevicePassword(message, action = "Onayla ve Kaydet") {
  return new Promise((resolve) => {
    document.getElementById("passwordMessage").textContent = message;
    document.getElementById("passwordSubmit").textContent = action;
    devicePassword.value = "";
    let finished = false;
    function finish(value) {
      if (finished) return;
      finished = true;
      passwordForm.removeEventListener("submit", submit);
      passwordDialog.removeEventListener("cancel", cancel);
      passwordCancel.removeEventListener("click", cancel);
      devicePassword.value = "";
      passwordDialog.close();
      cancelPasswordPrompt = null;
      resolve(value);
    }
    function submit(event) {
      event.preventDefault();
      if (devicePassword.value) finish(devicePassword.value);
    }
    function cancel(event) { event?.preventDefault(); finish(null); }
    cancelPasswordPrompt = () => finish(null);
    passwordForm.addEventListener("submit", submit);
    passwordDialog.addEventListener("cancel", cancel);
    passwordCancel.addEventListener("click", cancel);
    passwordDialog.showModal();
    devicePassword.focus();
  });
}

const PARAMETER_LABELS = {
  threshold: "VRMS eşik değeri", calibration: "Kalibrasyon sabiti",
  loadprofile: "Yük profili periyodu", rtc: "Tarih / saat", defaults: "Varsayılan ayarlar",
  ota: "Firmware güncellemesi",
};
const PARAMETER_ERRORS = {
  PASSWORD: "Şifre yanlış. İşlem yapılmadı.",
  VALUE: "Invalid value. Girilen değer geçersiz; değeri ve biçimini kontrol edin.",
  STORAGE: "Cihazın kalıcı belleğine kaydedilemedi. Tekrar deneyin.",
  RTC: "Tarih / saat RTC'ye yazılamadı veya doğrulanamadı.",
  BUSY: "Cihaz meşgul. Lütfen tekrar deneyin.",
  PARTIAL: "Sıfırlama tamamlanamadı; bazı ayarlar değişmiş olabilir. Güncel değerleri yeniden okuyun.",
  FIELD: "Cihaz bu parametrenin değiştirilmesini desteklemiyor.",
  FORMAT: "Cihaz değişiklik isteğini okuyamadı. Sayfayı yenileyip tekrar deneyin.",
};

async function writeParameterWithPassword(field, value, message) {
  if (parameterWritePending) return null;
  if (!bleDevice?.gatt?.connected) throw new Error("Cihaz bağlantısı yok. Önce cihaza bağlanın.");
  if (!parameterWriteChr) throw new Error("Şifreli işlem için cihaz yazılımını güncelleyin, ardından yeniden bağlanın.");
  parameterWritePending = true;
  const chr = parameterWriteChr;
  let password = "", request;
  try {
    password = await askDevicePassword(message || (PARAMETER_LABELS[field] +
      (field === "defaults" ? " geri yüklenecek." : " → " + value)),
      field === "ota" ? "Onayla ve Yükle" : "Onayla ve Kaydet");
    if (password === null) return null;
    if (!bleDevice?.gatt?.connected || chr !== parameterWriteChr) throw new Error("Cihaz bağlantısı kesildi.");
    request = encoder.encode(field + "\n" + password + "\n" + value);
    password = "";
    if (request.length >= 128) throw new Error("Şifre veya parametre değeri çok uzun.");
    await chr.writeValueWithResponse(request);
    request.fill(0);
    const result = decoder.decode(await chr.readValue());
    if (result.startsWith("ERR:")) {
      let message = PARAMETER_ERRORS[result.substring(4)] || "Cihaz işlemi tamamlayamadı.";
      if (field === "ota" && result === "ERR:FIELD")
        message = "Bu firmware şifreli OTA desteklemiyor. Önce cihaz yazılımını USB üzerinden güncelleyin.";
      if (result.startsWith("ERR:OTA:")) message = "OTA başlatılamadı: " + result.slice(8).replace(/^ERROR:/, "");
      const error = new Error(message);
      error.deviceRejected = true;
      throw error;
    }
    const prefix = "OK:" + field + "\n";
    if (!result.startsWith(prefix)) throw new Error("İşlem sonucu doğrulanamadı. Güncel değeri yeniden okuyun.");
    return { value: result.substring(prefix.length) };
  } finally {
    password = "";
    request?.fill(0);
    parameterWritePending = false;
  }
}

function setStatus(connected, text) {
  statusDot.classList.toggle("connected", connected);
  statusText.textContent = text;
  disconnectBtn.style.display = connected ? "block" : "none";
  btnGoShort.disabled = !connected;
  btnGoLong.disabled = !connected;
  btnGoStatus.disabled = !connected;
  btnGoOta.disabled = !connected;
  btnResetDefaults.disabled = !connected;
}

/* --- Paylasilan onay penceresi: silme/sifirlama gibi geri alinamaz
   islemler HICBIR ZAMAN dogrudan tiklamayla calismaz, hep once bunu
   sorar. askConfirm(mesaj) bir Promise<boolean> doner. --- */
const confirmModal = document.getElementById("confirm-modal");
const confirmModalText = document.getElementById("confirm-modal-text");
const confirmModalOk = document.getElementById("confirm-modal-ok");
const confirmModalCancel = document.getElementById("confirm-modal-cancel");

function askConfirm(message) {
  return new Promise((resolve) => {
    confirmModalText.textContent = message;
    confirmModal.classList.add("active");

    function cleanup(result) {
      confirmModal.classList.remove("active");
      confirmModalOk.removeEventListener("click", onOk);
      confirmModalCancel.removeEventListener("click", onCancel);
      resolve(result);
    }
    function onOk() { cleanup(true); }
    function onCancel() { cleanup(false); }

    confirmModalOk.addEventListener("click", onOk);
    confirmModalCancel.addEventListener("click", onCancel);
  });
}

function decodeValue(dataView) {
  return decoder.decode(dataView).trim();
}

function flashValue(elementId) {
  const el = document.getElementById(elementId);
  if (!el) return;
  el.classList.remove("flash");
  void el.offsetWidth; // reflow, animasyonun yeniden baslamasi icin
  el.classList.add("flash");
  setTimeout(() => el.classList.remove("flash"), 600);
}

async function readAndDisplay(service, uuid, elementId) {
  const chr = await service.getCharacteristic(uuid);
  const value = await chr.readValue();
  const el = document.getElementById(elementId);
  if (el) el.textContent = decodeValue(value);
}

/* Bir characteristic'e abone olup, degistiginde BIRDEN FAZLA elemani
 * (kisa ve uzun okuma ekranlarindaki ayni veri) birlikte gunceller. */
async function subscribeAndDisplay(service, uuid, elementIds, onValue) {
  const chr = await service.getCharacteristic(uuid);
  const initial = await chr.readValue();
  const initialText = decodeValue(initial);
  elementIds.forEach((id) => {
    const el = document.getElementById(id);
    if (el) el.textContent = initialText;
  });
  if (onValue) onValue(initialText);

  await chr.startNotifications();
  chr.addEventListener("characteristicvaluechanged", (event) => {
    const text = decodeValue(event.target.value);
    elementIds.forEach((id) => {
      const el = document.getElementById(id);
      if (el) { el.textContent = text; flashValue(id); }
    });
    if (onValue) onValue(text);
  });
}

/* --- Kisa okumadaki sabit/anlik alanlari, verilen on-ek (s- veya l-)
   altindaki elemanlara okuyup yazar --- */
async function populateInfoFields(prefix) {
  await readAndDisplay(infoService, CHR.serial, prefix + "serial");
  await readAndDisplay(infoService, CHR.firmware, prefix + "firmware");
  await readAndDisplay(infoService, CHR.production, prefix + "production");
  await readAndDisplay(infoService, CHR.loadprofile, prefix + "loadprofile");
  await readAndDisplay(infoService, CHR.rtc, prefix + "rtc");
  await readAndDisplay(infoService, CHR.threshold, prefix + "threshold");
  await readAndDisplay(infoService, CHR.calibration, prefix + "calibration");
  await readAndDisplay(infoService, CHR.baud, prefix + "baud");
}

async function populateLiveFields(prefix) {
  await readAndDisplay(liveService, CHR.vrmsMax, prefix + "vrms-max");
  await readAndDisplay(liveService, CHR.vrmsMin, prefix + "vrms-min");
  await readAndDisplay(liveService, CHR.vrmsMean, prefix + "vrms-mean");
  await readAndDisplay(liveService, CHR.vrmsInstant, prefix + "vrms-instant");
}

/* --- Kart Durumu: RS485 protokolunde hic olmayan, sadece BLE'ye ozel
   "cihaz sagligi" bilgileri (uptime, bos bellek, ADC hizi, LED durumu). --- */
async function populateStatusFields() {
  await readAndDisplay(statusService, CHR.uptime, "st-uptime");
  await readAndDisplay(statusService, CHR.freeHeap, "st-heap");
  await readAndDisplay(statusService, CHR.adcRate, "st-adcrate");
  await readAndDisplay(statusService, CHR.ledStatus, "st-led");
}

/* --- Uzun okumadaki 22 slotu (10 esik + 12 reset) EKSIKSIZ (bos olanlar
   dahil) satir satir gosterir. innerHTML kullanilmiyor - BLE'den gelen
   veri (pairing olmadigi icin sahte bir cihazdan da gelebilir) hicbir
   sekilde HTML olarak yorumlanmiyor (XSS'e kapali). --- */
/* Dakika cinsinden sureyi okunur hale getirir: 45 -> "45 dk",
   90 -> "1 sa 30 dk", 2880 -> "2 gün". */
function formatDuration(minutes) {
  if (!Number.isFinite(minutes) || minutes < 0) return "—";
  if (minutes < 60) return minutes + " dk";

  const days = Math.floor(minutes / 1440);
  const hours = Math.floor((minutes % 1440) / 60);
  const mins = minutes % 60;

  if (days > 0) return days + " gün" + (hours ? " " + hours + " sa" : "");
  return hours + " sa" + (mins ? " " + mins + " dk" : "");
}

function renderHistoryFull(text) {
  const thresholdBox = document.getElementById("threshold-log");
  const resetBox = document.getElementById("reset-log");
  thresholdBox.textContent = "";
  resetBox.textContent = "";

  const entries = text.split(";").map((s) => s.trim()).filter(Boolean);

  entries.forEach((entry) => {
    const parts = entry.split(",").map((s) => s.trim());
    const type = parts[0];
    const slot = parts[1];

    const row = document.createElement("div");
    row.className = "history-row";
    const label = document.createElement("span");
    label.className = "history-date";
    const value = document.createElement("span");
    value.className = "history-value";

    if (type === "T") {
      /* Kayitlar artik OLAY bazli: son alan varyans degil, olayin DAKIKA
         cinsinden suresi. Iki ozel deger olayin hala surdugunu gosterir:
           65535 -> olay BASLADI (ilk kayit)
           65534 -> olay SURUYOR (uzun olaylarda atilan ara kayit)
         vrms alani da cihaz tarafinda "V.VV" olarak bicimlendirilip
         geliyor (eskiden tam sayiya kirpilmis volttu). */
      const date = parts[2], time = parts[3], vrms = parts[4];
      const duration = parseInt(parts[5], 10);
      const empty = date === "00-00-00";
      const ongoing = duration === 65535 || duration === 65534;

      row.classList.toggle("history-empty", empty);
      row.classList.toggle("history-ongoing", !empty && ongoing);
      label.textContent = "#" + slot + (empty ? " — boş" : " — " + date + " " + time);

      if (empty) {
        value.textContent = "";
      } else if (ongoing) {
        /* Suren olayda zaman damgasi olayin BASI, vrms ise o ana kadarki
           tepe degerdir. */
        value.textContent = vrms + " V — sürüyor";
      } else {
        /* Biten olayda zaman damgasi DUSUS ani, vrms olay boyunca gorulen
           TEPE deger. */
        value.textContent = vrms + " V tepe — " + formatDuration(duration);
      }

      row.append(label, value);
      thresholdBox.append(row);
    } else if (type === "R") {
      const date = parts[2], time = parts[3];
      const empty = date === "00-00-00";
      row.classList.toggle("history-empty", empty);
      label.textContent = "#" + slot + (empty ? " — boş" : "");
      value.textContent = empty ? "" : date + " " + time;
      row.append(label, value);
      resetBox.append(row);
    }
  });
}

async function readHistoryFull() {
  return queueRecordRead(async () => {
    await sendCommand("LONG");
    const text = await readRecordPages("H", CHR.history);
    renderHistoryFull(text);
    const count = document.getElementById("reset-log").children.length;
    document.getElementById("resetHistorySummary").textContent =
      count + " / 12 kayıt yeri okundu." +
      (count < 12 ? (recordPageChr ? " Okuma eksik kaldı; tekrar deneyin." :
        " Tüm kayıtlar için güncel cihaz yazılımı gerekir. Zaten güncelledinizse tabletin Bluetooth'unu kapatıp açarak yeniden bağlanın.") : "");
  });
}

// Tek sayfa readValue sinirini asan kayitlar uygulama seviyesinde okunur.
// Eski cihazlarda yeni characteristic yoktur; mevcut okuma yolu korunur.
function queueRecordRead(task) {
  const result = recordReadQueue.then(task);
  recordReadQueue = result.catch(() => {});
  return result;
}

async function readRecordPages(kind, legacyUuid) {
  const pageChr = recordPageChr;
  if (!pageChr) {
    const chr = await controlService.getCharacteristic(legacyUuid);
    return decodeValue(await chr.readValue());
  }
  const chunks = [];
  let cursor = 0;
  // Flash'ta en fazla 3072 slot var; bozuk cevap sonsuz dongu olusturmasin.
  for (let page = 0; page <= 3072; page++) {
    if (pageChr !== recordPageChr) throw new Error("Kayıt okunurken bağlantı kesildi.");
    await pageChr.writeValueWithResponse(encoder.encode(kind + ":" + cursor));
    const value = await pageChr.readValue();
    const text = decoder.decode(value);
    const match = /^P1:(-1|\d+)\n/.exec(text);
    if (!match) throw new Error("Cihazdan geçersiz kayıt yanıtı geldi.");
    const payload = text.substring(match[0].length);
    chunks.push(payload);
    const next = Number(match[1]);
    if (next === -1) return chunks.join("");
    if (!Number.isSafeInteger(next) || next <= cursor || next > 3072 || !payload) {
      throw new Error("Kayıt aktarımı ilerlemiyor; tekrar deneyin.");
    }
    cursor = next;
  }
  throw new Error("Kayıt aktarımı tamamlanamadı.");
}

/* --- Load profile tarih-araligi sorgusu: gercek RS485 "P.01(start;end)"
   mekanizmasinin BLE karsiligi. Takvim, flash'ta GERCEKTEN veri olan
   gunleri (get_load_profile_dates_str) aktif/tiklanabilir gosterir, digerleri
   soluk/pasif kalir - kullanicinin "sadece o bellekteki mevcut olan
   tarihleri gostersin" istegi. --- */
let lpAvailableDates = new Set(); // "YYYY-MM-DD" (firmware'den 2 haneli yil geliyor, 20xx varsayiliyor)
let lpCalYear = null;
let lpCalMonth = null; // 0-indexli
let lpSelStart = null; // {y, m, d}
let lpSelEnd = null;
let lpQueryInProgress = false;

const lpCalLabel = document.getElementById("lpCalLabel");
const lpCalGrid = document.getElementById("lpCalGrid");
const lpSelectionText = document.getElementById("lpSelectionText");
const lpClearSelectionBtn = document.getElementById("lpClearSelection");
const lpQueryResult = document.getElementById("lpQueryResult");
const lpResultSummary = document.getElementById("lpResultSummary");

function lpParseAvailableDates(text) {
  lpAvailableDates = new Set();
  text.split(",").map((s) => s.trim()).filter(Boolean).forEach((d) => {
    const parts = d.split("-");
    if (parts.length !== 3) return;
    lpAvailableDates.add("20" + parts[0] + "-" + parts[1] + "-" + parts[2]);
  });
}

function lpKey(y, m, d) {
  return y + "-" + String(m + 1).padStart(2, "0") + "-" + String(d).padStart(2, "0");
}

function lpDateNum(y, m, d) {
  return y * 10000 + (m + 1) * 100 + d;
}

function lpResetSelection(clearResult) {
  lpSelStart = null;
  lpSelEnd = null;
  lpClearSelectionBtn.style.display = "none";
  lpSelectionText.textContent = "Bir gün seç (aralık için ikinci güne de dokun)";
  if (clearResult) {
    lpQueryResult.textContent = "Henüz sorgulanmadı";
    lpResultSummary.textContent = "";
  }
}

function renderLpCalendar() {
  if (lpCalYear === null) return;
  const monthNames = ["Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran",
                       "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"];
  lpCalLabel.textContent = monthNames[lpCalMonth] + " " + lpCalYear;
  lpCalGrid.textContent = "";

  ["Pt", "Sa", "Ça", "Pe", "Cu", "Ct", "Pz"].forEach((wd) => {
    const el = document.createElement("div");
    el.className = "lp-cal-weekday";
    el.textContent = wd;
    lpCalGrid.appendChild(el);
  });

  const firstDay = new Date(lpCalYear, lpCalMonth, 1);
  const startOffset = (firstDay.getDay() + 6) % 7; // Pazartesi ilk gun olsun
  const daysInMonth = new Date(lpCalYear, lpCalMonth + 1, 0).getDate();

  for (let i = 0; i < startOffset; i++) {
    const el = document.createElement("div");
    el.className = "lp-cal-day empty";
    lpCalGrid.appendChild(el);
  }

  for (let d = 1; d <= daysInMonth; d++) {
    const key = lpKey(lpCalYear, lpCalMonth, d);
    const active = lpAvailableDates.has(key);
    const el = document.createElement("div");
    el.className = "lp-cal-day " + (active ? "active" : "inactive");
    el.textContent = String(d);
    if ((lpSelStart && lpKey(lpSelStart.y, lpSelStart.m, lpSelStart.d) === key) ||
        (lpSelEnd && lpKey(lpSelEnd.y, lpSelEnd.m, lpSelEnd.d) === key)) {
      el.classList.add("selected");
    }
    if (active) {
      el.addEventListener("click", () => onLpDayClick(lpCalYear, lpCalMonth, d));
    }
    lpCalGrid.appendChild(el);
  }
}

async function onLpDayClick(y, m, d) {
  if (lpQueryInProgress) return;
  if (!lpSelStart || lpSelEnd) {
    lpSelStart = { y, m, d };
    lpSelEnd = null;
    lpClearSelectionBtn.style.display = "inline-block";
    lpSelectionText.textContent = "Başlangıç seçildi — aralık için başka bir güne, tek gün için aynı güne tekrar dokun";
    renderLpCalendar();
    return;
  }

  if (lpDateNum(y, m, d) < lpDateNum(lpSelStart.y, lpSelStart.m, lpSelStart.d)) {
    // baslangictan once bir gune dokunulduysa, secimi bu yeni gunden baslat
    lpSelStart = { y, m, d };
    renderLpCalendar();
    return;
  }

  lpSelEnd = { y, m, d };
  renderLpCalendar();
  await runLpQuery();
}

lpClearSelectionBtn.addEventListener("click", () => {
  if (lpQueryInProgress) return;
  lpResetSelection(true);
  renderLpCalendar();
});

document.getElementById("lpCalPrev").addEventListener("click", () => {
  lpCalMonth--;
  if (lpCalMonth < 0) { lpCalMonth = 11; lpCalYear--; }
  renderLpCalendar();
});
document.getElementById("lpCalNext").addEventListener("click", () => {
  lpCalMonth++;
  if (lpCalMonth > 11) { lpCalMonth = 0; lpCalYear++; }
  renderLpCalendar();
});

function lpFmtCmdDate(y, m, d) {
  // firmware "YY-MM-DD" bekliyor (2 haneli yil, dev'deki gercek kayit formatiyla ayni)
  return String(y % 100).padStart(2, "0") + "-" + String(m + 1).padStart(2, "0") + "-" + String(d).padStart(2, "0");
}

/* BLE'den gelen sonuc metni de (gecmis kayit gibi) hicbir zaman innerHTML
   ile eklenmiyor - textContent/DOM node ile, XSS'e kapali. */
function renderLpResult(text) {
  lpQueryResult.textContent = "";
  lpQueryResult.scrollTop = 0;
  lpResultSummary.textContent = "";
  const trimmed = text.trim();
  if (!trimmed || trimmed === "veri bulunamadi") {
    lpQueryResult.textContent = "Bu aralıkta kayıt yok.";
    return;
  }
  if (["gecersiz tarih formati", "tarih formati gecersiz", "partition bulunamadi", "okuma hatasi"].includes(trimmed)) {
    lpQueryResult.textContent = "Sorgu hatası: " + trimmed;
    return;
  }
  const entries = trimmed.split(";").map((s) => s.trim()).filter(Boolean);
  if (entries.length === 0) {
    lpQueryResult.textContent = "Bu aralıkta kayıt yok.";
    return;
  }
  entries.forEach((entry) => {
    const parts = entry.split(",").map((s) => s.trim());
    if (parts.length < 5) return;
    const [date, time, min, max, mean] = parts;
    const row = document.createElement("div");
    row.className = "history-row";
    const label = document.createElement("span");
    label.className = "history-date";
    label.textContent = date + " " + time;
    const value = document.createElement("span");
    value.className = "history-value";
    value.textContent = "min " + min + " / max " + max + " / ort " + mean;
    row.append(label, value);
    lpQueryResult.append(row);
  });
  lpResultSummary.textContent = lpQueryResult.children.length + " kayıt gösteriliyor." +
    (!recordPageChr ? " Kayıtlar eksik olabilir; tümü için cihaz yazılımını güncelleyin." : "");
}

async function runLpQuery() {
  if (!lpSelStart || lpQueryInProgress) return;
  lpQueryInProgress = true;
  const end = lpSelEnd || lpSelStart;
  const cmd = "LP:" + lpFmtCmdDate(lpSelStart.y, lpSelStart.m, lpSelStart.d) + ";" +
              lpFmtCmdDate(end.y, end.m, end.d);
  lpQueryResult.textContent = "Sorgulanıyor...";
  lpResultSummary.textContent = "";
  lpQueryResult.setAttribute("aria-busy", "true");
  lpClearSelectionBtn.disabled = true;
  try {
    await queueRecordRead(async () => {
      await sendCommand(cmd);
      renderLpResult(await readRecordPages("L", CHR.lpData));
    });
  } catch (err) {
    lpQueryResult.textContent = "Kayıtlar okunamadı. Tekrar deneyin.";
    showError("Load profile sorgu hatası: " + err.message);
  } finally {
    lpQueryInProgress = false;
    lpQueryResult.setAttribute("aria-busy", "false");
    lpClearSelectionBtn.disabled = false;
  }
}

async function loadLpAvailableDates() {
  const chr = await controlService.getCharacteristic(CHR.lpDates);
  const value = await chr.readValue();
  lpParseAvailableDates(decodeValue(value));

  const now = new Date();
  lpCalYear = now.getFullYear();
  lpCalMonth = now.getMonth();
  if (lpAvailableDates.size > 0) {
    const currentMonthPrefix = lpCalYear + "-" + String(lpCalMonth + 1).padStart(2, "0");
    const hasCurrentMonth = [...lpAvailableDates].some((k) => k.startsWith(currentMonthPrefix));
    if (!hasCurrentMonth) {
      // bu ayda hic veri yoksa, en son mevcut verinin ayina git
      const latest = [...lpAvailableDates].sort().slice(-1)[0];
      const [ly, lm] = latest.split("-").map(Number);
      lpCalYear = ly;
      lpCalMonth = lm - 1;
    }
  }
  lpResetSelection(true);
  renderLpCalendar();
}

/* --- Satır içi düzenleme: hem Kısa hem Uzun okuma ekranında ("s-"/"l-"
   önekleriyle) çalışır - kalem -> input + tik, tike basınca kaydet, başka
   yere basınca (blur) değişiklik yapmadan çık. Bir taraftan kaydedilen
   değer, aynı veriyi gösteren diğer ekrandaki kopyaya da yansıtılır. --- */
const PREFIXES = ["s-", "l-"];

function startEdit(prefix, field) {
  if (parameterWritePending) return;
  const display = document.getElementById(prefix + field);
  const input = document.getElementById("input-" + prefix + field);
  const editIcon = document.getElementById("edit-icon-" + prefix + field);
  const confirmIcon = document.getElementById("confirm-icon-" + prefix + field);

  input.value = display.textContent === "—" ? "" : display.textContent;
  display.style.display = "none";
  editIcon.style.display = "none";
  input.style.display = "inline-block";
  confirmIcon.style.display = "inline-block";
  input.focus();
  input.select();
}

function closeEditUI(prefix, field) {
  document.getElementById("input-" + prefix + field).style.display = "none";
  document.getElementById("confirm-icon-" + prefix + field).style.display = "none";
  document.getElementById(prefix + field).style.display = "inline";
  document.getElementById("edit-icon-" + prefix + field).style.display = "inline-block";
}

async function confirmEdit(prefix, field) {
  if (parameterWritePending) return;
  const input = document.getElementById("input-" + prefix + field);
  const value = input.value.trim();
  if (!value) {
    showToast(field === "threshold" || field === "calibration" ? PARAMETER_ERRORS.VALUE : "Bir değer girin.");
    return;
  }
  if (field === "threshold" && (!/^\d{1,3}$/.test(value) || Number(value) < 1 || Number(value) > 999)) {
    showToast("Invalid value. VRMS eşik değeri 1–999 arasında tam sayı olmalı."); return;
  }
  if (field === "loadprofile" && (!/^\d{1,3}$/.test(value) || Number(value) < 1 || Number(value) > 255)) {
    showToast("Yük profili periyodu 1–255 dakika arasında tam sayı olmalı."); return;
  }
  if (field === "calibration" && (value.length > CALIBRATION_VALUE_MAX_LENGTH ||
      !Number.isFinite(Number(value)) || Number(value) <= 0)) {
    showToast("Invalid value. Kalibrasyon sabiti sıfırdan büyük bir sayı olmalı ve en fazla " +
      CALIBRATION_VALUE_MAX_LENGTH + " karakter içermeli."); return;
  }
  const button = document.getElementById("confirm-icon-" + prefix + field);
  button.disabled = true;
  clearError();
  try {
    const result = await writeParameterWithPassword(field, value);
    if (!result) { input.focus(); return; }
    PREFIXES.forEach((p) => {
      const el = document.getElementById(p + field);
      if (el) { el.textContent = result.value; flashValue(p + field); }
    });
    closeEditUI(prefix, field);
    showToast("İşlem başarılı. " + PARAMETER_LABELS[field] + " kaydedildi.", true);
  } catch (err) {
    showToast(err.message || "Parametre kaydedilemedi.");
    input.focus();
  } finally {
    button.disabled = false;
  }
}

function cancelEdit(prefix, field) {
  closeEditUI(prefix, field);
}

PREFIXES.forEach((prefix) => {
  EDITABLE_FIELDS.forEach((field) => {
    const display = document.getElementById(prefix + field);
    const editIcon = document.getElementById("edit-icon-" + prefix + field);
    const confirmIcon = document.getElementById("confirm-icon-" + prefix + field);
    const input = document.getElementById("input-" + prefix + field);
    if (!display || !editIcon || !confirmIcon || !input) return;

    editIcon.addEventListener("click", () => startEdit(prefix, field));
    display.addEventListener("click", () => startEdit(prefix, field));

    // mousedown + preventDefault: input'un blur olmasını engeller ki
    // tike basınca "başka yere tıklandı" sanıp iptal etmesin.
    confirmIcon.addEventListener("mousedown", (e) => e.preventDefault());
    confirmIcon.addEventListener("click", () => confirmEdit(prefix, field));

    input.addEventListener("blur", () => {
      setTimeout(() => {
        if (!parameterWritePending && document.activeElement !== input && input.style.display !== "none") cancelEdit(prefix, field);
      }, 150);
    });

    input.addEventListener("keydown", (e) => {
      if (e.key === "Enter") { e.preventDefault(); confirmEdit(prefix, field); }
      if (e.key === "Escape") { e.preventDefault(); cancelEdit(prefix, field); }
    });
  });
});

async function sendCommand(cmd) {
  if (!commandChr) return;
  await commandChr.writeValue(encoder.encode(cmd));
}

/* --- Kısa/Uzun okuma tetikleme: cihaza komut yazip, guncel degerleri
   okuyup ilgili ekrana basar. Gercek cihazda da bu ikisi "iste, cevap
   gelsin" seklinde calisiyor - surekli akan bir veri degil. --- */
async function doShortRead() {
  clearError();
  try {
    await sendCommand("SHORT");
    await populateInfoFields("s-");
    await populateLiveFields("s-");
  } catch (err) {
    showError("Kısa okuma hatası: " + err.message);
  }
}

async function doLongRead() {
  clearError();
  try {
    await populateInfoFields("l-");
    await populateLiveFields("l-");
    await readHistoryFull();
    await loadLpAvailableDates();
  } catch (err) {
    showError("Uzun okuma hatası: " + err.message);
  }
}

async function doStatusRead() {
  clearError();
  try {
    await populateStatusFields();
  } catch (err) {
    showError("Kart durumu okuma hatası: " + err.message);
  }
}

btnGoShort.addEventListener("click", async () => {
  showView("view-short");
  btnGoShort.disabled = true;
  await doShortRead();
  btnGoShort.disabled = false;
});

btnGoLong.addEventListener("click", async () => {
  showView("view-long");
  btnGoLong.disabled = true;
  await doLongRead();
  btnGoLong.disabled = false;
});

btnGoStatus.addEventListener("click", async () => {
  showView("view-status");
  btnGoStatus.disabled = true;
  await doStatusRead();
  btnGoStatus.disabled = false;
});

refreshShort.addEventListener("click", () => doShortRead());
refreshLong.addEventListener("click", () => doLongRead());
refreshStatus.addEventListener("click", () => doStatusRead());

document.getElementById("backFromShort").addEventListener("click", () => showView("view-menu"));
document.getElementById("backFromLong").addEventListener("click", () => showView("view-menu"));
document.getElementById("backFromStatus").addEventListener("click", () => showView("view-menu"));

/* --- Firmware guncelleme (OTA) ---
   Akis: dosya sec -> sifreyle onay -> parameterWrite "ota" -> OTA_CHUNK_SIZE'lik
   parcalar halinde otaData characteristic'ine yaz -> "FINISH" -> durum
   bildirimini (notify) izleyip ilerleme cubugunu guncelle. Cihaz basariliysa
   birkac saniye icinde kendini resetleyip baglantiyi kesiyor - bu BEKLENEN
   bir davranis, hata degil. */
const otaFileInput = document.getElementById("ota-file-input");
const otaFileLabel = document.getElementById("ota-file-label");
const btnStartOta = document.getElementById("btnStartOta");
const otaProgressWrap = document.getElementById("ota-progress-wrap");
const otaProgressBar = document.getElementById("ota-progress-bar");
const otaStatusText = document.getElementById("ota-status-text");

function resetOtaUI() {
  otaSelectedFile = null;
  otaInProgress = false;
  otaSucceeded = false;
  otaAwaitingFinish = false;
  otaTransferError = "";
  otaFileInput.value = "";
  otaFileInput.disabled = false;
  otaFileLabel.textContent = "Dosya seçmek için dokun (.bin)";
  otaFileLabel.classList.remove("has-file");
  otaProgressWrap.style.display = "none";
  otaProgressBar.style.width = "0%";
  otaProgressBar.classList.remove("error", "done");
  btnStartOta.disabled = true;
  btnStartOta.textContent = "Güncellemeyi Başlat";
}

otaFileInput.addEventListener("change", () => {
  const file = otaFileInput.files[0];
  if (!file) return;
  otaSelectedFile = file;
  otaFileLabel.textContent = file.name + " (" + (file.size / 1024).toFixed(1) + " KB)";
  otaFileLabel.classList.add("has-file");
  btnStartOta.disabled = false;
});

function otaSetProgress(pct, text, cls) {
  otaProgressBar.style.width = Math.max(0, Math.min(100, pct)) + "%";
  otaProgressBar.classList.remove("error", "done");
  if (cls) otaProgressBar.classList.add(cls);
  otaStatusText.textContent = text;
}

/* otaStatus characteristic'inden gelen "IDLE" / "WRITING:45" /
   "SUCCESS_REBOOTING" / "ERROR:<sebep>" metnini ilerleme cubuguna yansitir. */
function handleOtaStatusText(text) {
  if (!otaInProgress || otaSucceeded || otaTransferError) return;
  if (text.startsWith("WRITING:")) {
    const pct = parseInt(text.split(":")[1], 10) || 0;
    otaSetProgress(pct, "Yazılıyor... %" + pct);
  } else if (text === "SUCCESS_REBOOTING" && otaAwaitingFinish) {
    otaSucceeded = true;
    otaSetProgress(100, "Başarılı! Cihaz yeniden başlıyor...", "done");
    showToast("İşlem başarılı. Firmware yüklendi; cihaz yeniden başlıyor.", true);
  } else if (text.startsWith("ERROR:")) {
    otaTransferError = text.slice(6);
  }
}

async function doOta() {
  if (!otaSelectedFile || otaInProgress || parameterWritePending) return;
  const file = otaSelectedFile;
  clearError();
  otaInProgress = true;
  otaSucceeded = false;
  otaAwaitingFinish = false;
  otaTransferError = "";
  btnStartOta.disabled = true;
  otaFileInput.disabled = true;
  btnStartOta.textContent = "Onay bekleniyor...";
  let controlChr, statusChr, startAttempted = false, started = false;
  try {
    if (!bleDevice?.gatt?.connected || !otaService) throw new Error("Cihaz bağlantısı yok. Önce cihaza bağlanın.");
    if (!file.size) throw new Error("Firmware dosyası boş.");
    controlChr = await otaService.getCharacteristic(CHR.otaControl);
    const dataChr = await otaService.getCharacteristic(CHR.otaData);
    statusChr = await otaService.getCharacteristic(CHR.otaStatus);
    const bytes = new Uint8Array(await file.arrayBuffer());
    if (bytes.length !== file.size) throw new Error("Firmware dosyası tam okunamadı.");
    startAttempted = true;
    const result = await writeParameterWithPassword("ota", String(bytes.length),
      file.name + " (" + (file.size / 1024).toFixed(1) + " KB) cihaza yüklenecek. " +
      "Cihaz şifresini girin. Yükleme boyunca bağlantıyı kesmeyin veya sayfadan ayrılmayın.");
    if (!result) return;
    started = true;
    if (result.value !== String(bytes.length)) throw new Error("OTA başlatma sonucu doğrulanamadı.");
    btnStartOta.textContent = "Güncelleniyor...";
    otaProgressWrap.style.display = "block";
    otaSetProgress(0, "Başlıyor...");

    for (let offset = 0; offset < bytes.length; offset += OTA_CHUNK_SIZE) {
      if (otaTransferError) throw new Error(otaTransferError);
      const chunk = bytes.slice(offset, offset + OTA_CHUNK_SIZE);
      // Her parcanin cihaz tarafindan kabul edildigini bekle; hatada aktarimi durdur.
      await dataChr.writeValueWithResponse(chunk);
      if (otaTransferError) throw new Error(otaTransferError);
      const pct = Math.round(((offset + chunk.length) / bytes.length) * 100);
      otaSetProgress(pct, "Gönderiliyor... %" + pct);
    }

    otaAwaitingFinish = true;
    otaSetProgress(100, "Doğrulanıyor...");
    await controlChr.writeValueWithResponse(encoder.encode("FINISH"));
    if (!otaSucceeded) handleOtaStatusText(decoder.decode(await statusChr.readValue()));
    if (!otaSucceeded) throw new Error(otaTransferError || "Cihaz firmware yüklemesini doğrulamadı.");
  } catch (err) {
    if (otaSucceeded) return; // Basari bildirimi sonrasi beklenen yeniden baslama.
    if (startAttempted && !err.deviceRejected && bleDevice?.gatt?.connected) {
      // Flash hatasinin aciklamasini al, yarim kalan kendi oturumumuzu kapat.
      try {
        if (started) {
          const status = decoder.decode(await statusChr.readValue());
          if (status.startsWith("ERROR:") && !otaTransferError) otaTransferError = status.slice(6);
        }
      } catch (_) { /* Baglanti kopmus olabilir. */ }
      try { await controlChr.writeValueWithResponse(encoder.encode("ABORT")); } catch (_) { /* Oturum zaten kapanmis olabilir. */ }
    }
    const message = otaTransferError || err.message;
    otaProgressWrap.style.display = "block";
    otaSetProgress(0, "Hata: " + message, "error");
    showToast(message);
  } finally {
    otaInProgress = false;
    otaAwaitingFinish = false;
    btnStartOta.disabled = otaSucceeded;
    otaFileInput.disabled = otaSucceeded;
    btnStartOta.textContent = otaSucceeded ? "Güncelleme Tamamlandı" : "Güncellemeyi Başlat";
  }
}

btnStartOta.addEventListener("click", doOta);

btnGoOta.addEventListener("click", () => {
  resetOtaUI();
  showView("view-ota");
});

document.getElementById("backFromOta").addEventListener("click", () => {
  if (otaInProgress) {
    showError("Güncelleme sürerken bu ekrandan çıkma.");
    return;
  }
  showView("view-menu");
});

/* --- Varsayılan ayarlara sıfırlama: ÖNCE ONAY İSTER, tıklayınca hemen
   yapmaz. Başarılıysa açık olan ekrandaki (kısa/uzun) değerleri de tazeler. --- */
btnResetDefaults.addEventListener("click", async () => {
  if (parameterWritePending) return;
  const ok = await askConfirm(
    "Eşik değeri, kalibrasyon sabiti ve yük profili periyodu fabrika ayarlarına dönecek. Devam edilsin mi?"
  );
  if (!ok) return;
  clearError();
  try {
    const result = await writeParameterWithPassword("defaults", "");
    if (!result) return;
    try {
      await populateInfoFields("s-");
      await populateInfoFields("l-");
    } catch {
      showToast("Ayarlar sıfırlandı, ancak ekran yenilenemedi. Yenile düğmesine basın.");
      return;
    }
    showToast("İşlem başarılı. Varsayılan ayarlar kaydedildi.", true);
  } catch (err) {
    showToast(err.message || "Ayarlar sıfırlanamadı.");
  }
});

/* --- Geçmiş kayıt silme: ikisi de ÖNCE ONAY İSTER, sonra ilgili komutu
   gönderip Uzun Okuma ekranını tazeler. --- */
document.getElementById("btnClearThreshold").addEventListener("click", async () => {
  const ok = await askConfirm("Eşik aşım kayıtlarının tamamı silinecek. Bu işlem geri alınamaz. Devam edilsin mi?");
  if (!ok) return;

  clearError();
  try {
    await sendCommand("CLEAR_THRESHOLD");
    await readHistoryFull();
  } catch (err) {
    showError("Silme hatası: " + err.message);
  }
});

document.getElementById("btnClearReset").addEventListener("click", async () => {
  const ok = await askConfirm("Reset/açılış kayıtlarının tamamı silinecek. Bu işlem geri alınamaz. Devam edilsin mi?");
  if (!ok) return;

  clearError();
  try {
    await sendCommand("CLEAR_RESET");
    await readHistoryFull();
  } catch (err) {
    showError("Silme hatası: " + err.message);
  }
});

/* --- Bağlantı kurma / GATT keşfi (hem manuel butonla hem otomatik
   yeniden bağlanmada ortak kullanılıyor) --- */
async function connectToServer() {
  const server = await bleDevice.gatt.connect();

  infoService = await server.getPrimaryService(METER_INFO_SVC);
  liveService = await server.getPrimaryService(METER_LIVE_SVC);
  controlService = await server.getPrimaryService(METER_CONTROL_SVC);
  statusService = await server.getPrimaryService(METER_STATUS_SVC);
  otaService = await server.getPrimaryService(METER_OTA_SVC);
  commandChr = await controlService.getCharacteristic(CHR.command);
  const controlCharacteristics = await controlService.getCharacteristics();
  recordPageChr = controlCharacteristics.find((chr) => chr.uuid === CHR.recordPage) || null;
  parameterWriteChr = controlCharacteristics.find((chr) => chr.uuid === CHR.parameterWrite) || null;

  // OTA durumu (yazma ilerlemesi/basari/hata) - view-ota ekraninda gosterilecek,
  // ama abonelik baglantida bir kere kuruluyor (diger notify'larla ayni desen).
  await subscribeAndDisplay(otaService, CHR.otaStatus, [], handleOtaStatusText);

  // VRMS max/min/mean periyodik (load profile periyodunda bir) guncelleniyor,
  // hem kisa hem uzun okuma ekranindaki karsiliklarini birlikte tazeliyor.
  await subscribeAndDisplay(liveService, CHR.vrmsMax, ["s-vrms-max", "l-vrms-max"]);
  await subscribeAndDisplay(liveService, CHR.vrmsMin, ["s-vrms-min", "l-vrms-min"]);
  await subscribeAndDisplay(liveService, CHR.vrmsMean, ["s-vrms-mean", "l-vrms-mean"]);
  // VRMS anlik ise GERCEKTEN anlik - cihaz her olcum penceresinde (saniyede
  // birkac kere) guncelliyor.
  await subscribeAndDisplay(liveService, CHR.vrmsInstant, ["s-vrms-instant", "l-vrms-instant"]);

  // Bos bellek de degistikce (saniyede bir kontrol ediliyor, cihaz tarafinda)
  // anlik guncelleniyor - Kart Durumu ekranini acmaya/yenile'ye basmaya gerek yok.
  await subscribeAndDisplay(statusService, CHR.freeHeap, ["st-heap"]);

  setStatus(true, "Bağlandı: " + bleDevice.name);
}

async function pickAndConnect() {
  clearError();
  connectBtn.disabled = true;
  setStatus(false, "Cihaz seçiliyor...");

  try {
    bleDevice = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: METER_NAME_PREFIX }],
      optionalServices: [METER_INFO_SVC, METER_LIVE_SVC, METER_CONTROL_SVC, METER_STATUS_SVC, METER_OTA_SVC],
    });

    bleDevice.addEventListener("gattserverdisconnected", onDisconnected);
    setStatus(false, "Bağlanıyor...");
    await connectToServer();
  } catch (err) {
    console.error(err);
    showError("Bağlantı hatası: " + err.message);
    setStatus(false, "Bağlı değil");
  } finally {
    connectBtn.disabled = false;
  }
}

function onDisconnected() {
  if (otaInProgress && !otaSucceeded) otaTransferError = "Cihaz bağlantısı kesildi. Güncelleme tamamlanmadı.";
  cancelPasswordPrompt?.();
  setStatus(false, "Bağlantı kesildi");
  showView("view-menu");
  infoService = null;
  liveService = null;
  controlService = null;
  statusService = null;
  otaService = null;
  commandChr = null;
  recordPageChr = null;
  parameterWriteChr = null;
  // OTA basariyla bitince cihaz KENDINI resetleyip baglantiyi keser - bu
  // BEKLENEN bir "disconnected" olayi, hata degil, bu yuzden UI'i ayrica
  // sifirlamiyoruz (otaInProgress zaten false'a dusmus olur bir sonraki
  // "Firmware Guncelleme" ekranina girildiginde resetOtaUI() calisir).
}

function disconnectIfConnected() {
  if (bleDevice && bleDevice.gatt && bleDevice.gatt.connected) {
    bleDevice.gatt.disconnect();
  }
}

connectBtn.addEventListener("click", () => {
  if (!navigator.bluetooth) {
    showError("Bu tarayıcı Web Bluetooth desteklemiyor. Android + Chrome kullanın.");
    return;
  }
  pickAndConnect();
});

disconnectBtn.addEventListener("click", disconnectIfConnected);

// Sayfa gercekten kapanirken/yenilenirken baglantiyi temiz sekilde kes,
// ki cihaz hemen tekrar yayina donsun ve yeniden bulunabilsin.
//
// ⚠️ GERCEK BIR HATA BURADAYDI, BULUNUP DUZELTILDI: daha once "visibilitychange"
// (sayfa arka plana gecince) olayinda da baglanti kesiliyordu - ama dosya secme
// ekrani (input type=file) acilinca da Android bu sayfayi "arka plana" alip
// AYNI olayi tetikliyor. Sonuc: kullanici sadece bir .bin dosyasi secmeye
// calisirken BLE baglantisi kesiliyor, "gattserverdisconnected" olayi
// onDisconnected()'i tetikleyip ekrani ana menuye geri atiyor, secilen dosya
// da (goruntude) kaybolmus gibi oluyordu. "visibilitychange" kaldirildi -
// artik sadece GERCEKTEN sayfadan ayrilma/kapanma/yenileme (pagehide/
// beforeunload) baglantiyi kesiyor, gecici arka plana alma (dosya secici,
// baska bir uygulamaya kisa sureligine gecme vs.) baglantiyi etkilemiyor.
window.addEventListener("pagehide", disconnectIfConnected);
window.addEventListener("beforeunload", disconnectIfConnected);

// Tarayıcı destekliyorsa (Chrome'un "persistent permissions" özelliği),
// daha önce izin verilen cihaza sayfa açılır açılmaz, tıklamaya gerek
// kalmadan otomatik yeniden bağlan.
async function tryAutoReconnect() {
  if (!navigator.bluetooth || !navigator.bluetooth.getDevices) return;
  try {
    const devices = await navigator.bluetooth.getDevices();
    const known = devices.find((d) => d.name?.startsWith(METER_NAME_PREFIX));
    if (!known) return;

    bleDevice = known;
    bleDevice.addEventListener("gattserverdisconnected", onDisconnected);
    setStatus(false, "Cihaz hatırlandı, bağlanılıyor...");
    await connectToServer();
  } catch (err) {
    console.warn("Otomatik bağlanma denenemedi:", err);
    setStatus(false, "Bağlı değil");
  }
}

tryAutoReconnect();
