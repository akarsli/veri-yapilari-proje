# Veri seti: İstanbul M4, M5, M8 metro hatları

Durum: 24.11.2026 itibarıyla hizmette olan istasyonlar. İnşaat ve proje aşamasındaki istasyonlar dahil değil.

## Dosyalar

| Dosya | Açıklama |
|---|---|
| `stops.csv` | `id,name,lat,lon,crowd,connections`: 57 benzersiz istasyon. `crowd` 1–5 arası temel yoğunluk puanı. `connections` kapsam dışı bağlantılar (yalnızca bilgi amaçlı, `;` ile ayrılmış) |
| `lines.csv` | `line_id,name,type,color,headway_min,total_min,air_km` |
| `edges.csv` | `from_id,to_id,line_id,minutes,distance_km`: ardışık istasyonlar. **İki yönlü**, her iki yönde aynı süre |
| `transfers.csv` | `stop_id,from_line,to_line,walk_min`: aynı istasyonda hat değiştirme |

Dosyalar UTF-8 kodlu. Alanların içinde virgül yok, bu yüzden C'de basit `strtok`/`strsep` ile okunabilir.

## Kaynaklar
- **İstasyon listesi, sırası ve uçtan uca süreler:** Türkçe Vikipedi'deki M4, M5 ve M8 sayfaları (M4 52 dk, M5 50 dk, M8 26 dk)
- **Koordinatlar:** OpenStreetMap, Overpass API ile hat ilişkilerindeki `stop` noktaları. İki yönün ortalaması alındı.
  © OpenStreetMap katkıda bulunanları, ODbL lisansı. Raporda kaynak olarak belirtilmeli.

## Varsayımlar (raporda belirtilecek)
1. **İstasyonlar arası süre:** her aralık = 0,75 dk sabit pay (duruş/kalkış) + hattın kalan süresinin kuş uçuşu mesafeye orantılı payı. Hat toplamları resmi sürelerle uyumlu (±0,3 dk).
2. **`distance_km` ve `air_km`** kuş uçuşu (haversine) mesafedir. Ray uzunluğu biraz daha fazladır. Bu yüzden A* sezgiselinde kullanılabilir (fazla tahmin etmez).
3. **Sefer aralığı** (`headway_min`): M4 5 dk, M5 ve M8 6 dk varsayıldı. Binişte ve aktarmada ortalama bekleme = sefer aralığı / 2.
4. **Aktarma yürüme süreleri:** Kozyatağı 6 dk (yaya tüneli, istasyonlar arası yaklaşık 250 m), Dudullu 4 dk.
5. **Bostancı:** M4 Bostancı ile M8 Bostancı aynı adı taşısa da yaklaşık **1,6 km uzaktadır** ve aralarında fiziksel bağlantı yoktur. 20 dk yürüme aktarması olarak modellendi. Tek bir durak kaydı tutuluyor, koordinat iki istasyonun ortalaması.
6. **Ortak istasyonların koordinatı** (Kozyatağı, Dudullu, Bostancı) iki hattaki koordinatların ortalamasıdır.
7. **Yoğunluk puanları** tahmindir: merkezi ve aktarmalı istasyonlar yüksek, hat uçları düşük. Çalışma anında güncellenebilir.
8. Marmaray, metrobüs, vapur gibi diğer hatlar rota hesabına dahil değil.
