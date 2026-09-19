/*
 * Service worker: sayfayi ve varliklarini cihazda onbellege alir, ki
 * internet olmadan sayfa yenilenince (F5) tarayicinin kendi "internet yok"
 * hata sayfasi degil, gercek uygulama acilsin ve butonlar calissin.
 *
 * Strateji: once internet (ag'dan en guncel surumu getir), sadece ag
 * basarisiz olursa (gercekten offline) onbellege dus. Boylece gelistirme
 * sirasinda internet varken hep en son deploy edilen surum gorunur.
 */
const CACHE_NAME = "mavi-alp-ble-v10";
const ASSETS = [
  "/", "/index.html", "/style.css?v=10", "/app.js?v=10", "/manifest.json",
  "/optimized_logo_2.svg", "/logo-new-full-transparent-bg.png",
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => cache.addAll(ASSETS.map((url) => new Request(url, { cache: "reload" }))))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k.startsWith("mavi-alp-ble-") && k !== CACHE_NAME)
        .map((k) => caches.delete(k)))
    ).then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (event) => {
  if (event.request.method !== "GET" || new URL(event.request.url).origin !== self.location.origin) return;
  event.respondWith(
    // Varsayilan fetch, taze sayilan ESKI HTTP onbellegini dondurebilir.
    // Online iken sunucuya mutlaka dogrulat; offline iken PWA onbellegini kullan.
    fetch(event.request, { cache: "no-cache" })
      .then((response) => {
        if (response.ok) {
          const clone = response.clone();
          event.waitUntil(caches.open(CACHE_NAME).then((cache) => cache.put(event.request, clone)));
        }
        return response;
      })
      .catch(async () => {
        const cache = await caches.open(CACHE_NAME);
        const cached = await cache.match(event.request);
        if (cached) return cached;
        if (event.request.mode === "navigate") {
          const page = await cache.match("/index.html");
          if (page) return page;
        }
        return Response.error();
      })
  );
});
