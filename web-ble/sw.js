/*
 * Service worker: sayfayi ve varliklarini cihazda onbellege alir, ki
 * internet olmadan sayfa yenilenince (F5) tarayicinin kendi "internet yok"
 * hata sayfasi degil, gercek uygulama acilsin ve butonlar calissin.
 *
 * Strateji: once internet (ag'dan en guncel surumu getir), sadece ag
 * basarisiz olursa (gercekten offline) onbellege dus. Boylece gelistirme
 * sirasinda internet varken hep en son deploy edilen surum gorunur.
 */
const CACHE_NAME = "mavi-alp-ble-v2";
const ASSETS = ["/", "/index.html", "/manifest.json", "/icon.svg"];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => cache.addAll(ASSETS))
  );
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE_NAME).map((k) => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener("fetch", (event) => {
  event.respondWith(
    fetch(event.request)
      .then((response) => {
        const clone = response.clone();
        caches.open(CACHE_NAME).then((cache) => cache.put(event.request, clone));
        return response;
      })
      .catch(() => caches.match(event.request))
  );
});
