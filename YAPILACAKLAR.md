# Kent İçi Rota ve Hat Planlayıcı — Yapılacaklar (C)

Kaynak: `Final projesi.docx` (gereksinimler) + `Final projesine ait taslak.docx` (rapor şablonu)

## 0. Alınan kararlar (24.11.2026)

| Konu | Karar |
|---|---|
| Ekip | Tek kişi |
| Seviye | Temel + Orta + seçili ekstralar (A* karşılaştırması, performans grafiği). GUI yok |
| Veri seti | İstanbul Anadolu yakası metroları: **M4, M5, M8** — 57 istasyon, `data/` klasöründe hazır |
| Ağaç | Sadece **Trie** (AVL yalnızca hoca isterse) |
| Rota kriterleri | En kısa süre, en az aktarma, çok kriterli (süre + aktarma + yoğunluk) |
| Platform | macOS, clang + Makefile, saf C (taşınabilir, `conio.h` vb. yok) |
| Teslim / sunum | Sonra kararlaştırılacak |

---

## 1. Hocanın zorunlu tuttuğu şartlar → Bu projede karşılığı

| Zorunlu şart | Projede nasıl karşılanacak |
|---|---|
| Dil: tek dil (C) | Tüm proje C99/C11, `Makefile` ile derlenir |
| En az 1 dengeli ağaç (AVL/RB) **veya** Trie | **Trie**: durak adında prefix arama / otomatik tamamlama ("Kadık" → Kadıköy). İsteğe bağlı **AVL**: durak ID → durak kaydı |
| En az 1 kısa yol (BFS/Dijkstra) veya A* | **Dijkstra** (min-heap ile, en kısa süre), **BFS** (en az aktarma), isteğe bağlı **A*** (koordinatlarla haversine sezgiseli) |
| En az 2 sıralama + binary search | **Mergesort**: alternatif rotaları süreye göre sıralama; **Quicksort**: durakları ada/ID'ye göre sıralama; **Binary search**: sıralı durak dizisinde ID/ad arama |
| Stack & Queue gerçek kullanım | **Queue**: BFS, rota sorgu kuyruğu (toplu sorgu dosyası). **Stack**: yol geri izleme (prev[] → stack → doğru sırada yazdırma), **undo/redo** (yol kapatma/açma işlemleri için iki stack) |
| Girdi/Çıktı: JSON/CSV + CLI | `stops.csv`, `lines.csv`, `edges.csv` okuma; sonuçları `.csv` olarak dışa aktarma; menülü CLI |
| Zaman/Uzay analizi tablosu | Her ana işlem için Big-O tablosu (aşağıda taslak var) + ölçülen süreler |

Konunun önerdiği veri yapıları: **adjacency list graf, queue (BFS), min-heap, dizi/hash tablo (durak indeksi)** → hepsi kullanılacak.

---

## 2. Özellikler (zorluk ölçekleme sırasıyla)

### Temel (mutlaka)
1. CSV'den durak / hat / bağlantı yükleme, hatalı satırları raporlama
2. Durak arama: tam ad (hash), prefix (Trie), ID (binary search)
3. **En kısa süre rotası** — Dijkstra + min-heap
4. **En az aktarmalı rota** — BFS
5. Rotayı yazdırma: duraklar, binilecek hat, aktarma noktaları, toplam süre/mesafe
6. Durak / hat listeleme (ada, hat sayısına göre sıralı)

### Orta
7. **Çok kriterli rota**: `maliyet = a·süre + b·aktarma_cezası + c·yoğunluk` (ağırlıklar kullanıcıdan)
8. **Kapalı yollar**: kenarı dinamik kaldır/geri ekle + **undo/redo** (stack)
9. **Canlı yoğunluk puanı**: kenar/durak yoğunluğunu güncelleme (dosyadan veya rastgele simülasyon)
10. Alternatif rotalar (ör. en iyi k rota veya farklı kriterlerden çıkan rotalar) → mergesort ile sıralı listeleme

### Ekstra puan
11. A* ile Dijkstra karşılaştırması (genişletilen düğüm sayısı, süre)
12. Performans testi: rastgele büyük graf üretip (1K, 10K, 100K düğüm) süre ölçümü → grafik
13. GitHub repo + README, Medium yazısı, sınıfta sunum (14–15. hafta)
14. GUI (şart değil; C'de zor — gerekirse rotayı HTML/SVG'ye export etmek pratik bir alternatif)

---

## 3. Kritik tasarım noktası: aktarma modellemesi

Sadece "durak = düğüm" yaparsan Dijkstra hangi hatta olduğunu bilmez, aktarmayı sayamaz.
Çözüm: **durum = (durak, hat)** çifti.
- Aynı hat üzerinde ilerleme → süre kadar maliyet
- Aynı durakta hat değiştirme → aktarma cezası (ör. +5 dk) ve aktarma sayısı +1
- BFS (en az aktarma) için: hat değiştirme kenarı = 1, aynı hatta ilerleme = 0 → **0-1 BFS (deque)** veya hat-grafı üzerinde düz BFS

Bunu raporda ayrı bir alt başlık olarak anlatmak iyi puan getirir.

---

## 4. Veri modeli ve veri seti (HAZIR → `data/`)

Ayrıntılar ve varsayımlar: [data/README.md](data/README.md)

| Dosya | Sütunlar | Satır |
|---|---|---|
| `stops.csv` | id,name,lat,lon,crowd,connections | 57 istasyon |
| `lines.csv` | line_id,name,type,color,headway_min,total_min,air_km | 3 hat |
| `edges.csv` | from_id,to_id,line_id,minutes,distance_km (iki yönlü) | 57 bağlantı |
| `transfers.csv` | stop_id,from_line,to_line,walk_min | 6 (3 istasyon × 2 yön) |

| Hat | Güzergâh | İstasyon | Süre |
|---|---|---|---|
| M4 | Kadıköy – Sabiha Gökçen Havalimanı | 23 | 52 dk |
| M5 | Üsküdar – Sultanbeyli | 24 | 50 dk |
| M8 | Bostancı – Parseller | 13 | 26 dk |

**Aktarma noktaları:**
- **Kozyatağı** M4 ↔ M8: yaya tüneli, 6 dk
- **Dudullu** M5 ↔ M8: 4 dk
- **Bostancı** M4 ↔ M8: iki istasyon aslında **1,6 km uzakta**, fiziksel bağlantı yok → 20 dk yürüme olarak modellendi (pahalı aktarma, çok kriterli rota için iyi örnek)

Örnek: *Kadıköy → Ümraniye* ≈ 61 dk: M4 → Kozyatağı → M8 → Dudullu → M5 (2 aktarma).

Kalan veri işleri:
- `closures.csv` / yoğunluk güncellemeleri çalışma anında menüden yapılacak
- Performans testleri için rastgele büyük graf üreticisi (C içinde, `bench.c`)
- İleride Marmaray eklenebilir (Ayrılık Çeşmesi, Bostancı, Pendik, Üsküdar ile kesişir); sadece CSV satırı eklenerek

---

## 5. Klasör / modül yapısı

✅ = yazıldı ve test edildi (`make test`: 187 + 123 kontrol, ASan/UBSan ve `leaks` temiz)

```
Proje/
├── Makefile          make / test / sanitize / bench / clean
├── README.md         ✅ proje tanıtımı, kullanım, gereksinim eşleştirmesi
├── docs/KARMASIKLIK.md ✅ zaman/uzay tablosu + ölçümler (rapora aktarılacak)
├── data/             ✅ stops, lines, edges, transfers (.csv) + README
├── include/          başlık dosyaları
├── src/
│   ├── main.c        ✅ CLI: arama, 3 rota modu, yol kapat/aç, undo/redo, yoğunluk, algoritma karşılaştırma
│   ├── common.c      ✅ xmalloc vb. güvenli bellek fonksiyonları
│   ├── text.c        ✅ Türkçe karakter normalleştirme (Kadıköy → kadikoy)
│   ├── stack.c       ✅ genel stack (yol geri izleme, undo/redo)
│   ├── queue.c       ✅ dairesel kuyruk + öne ekleme (BFS, 0-1 BFS)
│   ├── heap.c        ✅ indeksli min-heap, decrease-key (Dijkstra, A*)
│   ├── hash.c        ✅ zincirlemeli hash, FNV-1a, otomatik rehash
│   ├── trie.c        ✅ prefix arama, alfabetik sonuç
│   ├── sort.c        ✅ mergesort (kararlı), quicksort, binary search
│   ├── graph.c       ✅ adjacency list, (durak, hat) durumları, kenar kapat/aç, canlı yoğunluk
│   ├── io_csv.c      ✅ CSV okuma + satır bazlı hata raporu (⏳ dışa aktarma)
│   ├── route.c       ✅ dijkstra, A*, en az aktarma (0-1 BFS + katmanlı Dijkstra), çok kriterli
│   ├── history.c     ✅ kapalı yol undo/redo (iki stack)
│   └── gen.c         ✅ sentetik ızgara şehir üreteci (performans testleri için)
├── bench/bench.c     ✅ make bench: veri yapıları + rota algoritmaları ölçümü → results/*.csv
└── tests/
    ├── test_ds.c     ✅ veri yapıları birim testleri
    └── test_route.c  ✅ yükleme, arama, Dijkstra, en az aktarma (tüm çiftler), kapalı yol, undo/redo, hatalı veri
```

Dikkat edilecekler: bellek sızıntısı (`valgrind` / macOS'ta `leaks`), Türkçe karakterler (Trie'yi byte bazlı kurmak en kolayı), hatalı CSV satırları, bağlantısız duraklar, aynı başlangıç-bitiş durağı.

---

## 6. CLI menü taslağı

```
1) Veri yükle            6) Yol kapat / aç
2) Durak ara (prefix)    7) Undo / Redo
3) En hızlı rota         8) Yoğunluk güncelle
4) En az aktarma         9) Durak/hat listele (sıralı)
5) Çok kriterli rota    10) Performans testi
0) Çıkış
```

---

## 7. Karmaşıklık tablosu (rapora girecek taslak)

V = durak (veya durak-hat durumu) sayısı, E = bağlantı sayısı, L = ad uzunluğu

| Yapı / İşlem | Zaman | Uzay |
|---|---|---|
| Graf kenar ekleme | O(1) | O(V+E) toplam |
| Graf kenar silme (kapalı yol) | O(derece) | — |
| Min-heap insert / extract-min | O(log n) | O(n) |
| Queue enqueue / dequeue | O(1) | O(n) |
| Stack push / pop | O(1) | O(n) |
| Hash ekle / ara | O(1) ort., O(n) en kötü | O(n) |
| Trie ekle / prefix ara | O(L) / O(L + k) | O(toplam karakter) |
| AVL ekle / ara / sil | O(log n) | O(n) |
| Mergesort | O(n log n) | O(n) |
| Quicksort | O(n log n) ort., O(n²) en kötü | O(log n) |
| Binary search | O(log n) | O(1) |
| BFS | O(V+E) | O(V) |
| Dijkstra (binary heap) | O((V+E) log V) | O(V) |
| A* | en kötü Dijkstra ile aynı, pratikte daha az düğüm | O(V) |

---

## 8. Rapor (taslak şablona göre)

Şablondaki bölümler — örnek metinler (yazılım testi, Cucumber vb.) silinip projeye göre yazılacak.
Not: Kapakta "VERİ BİLİMİNE GİRİŞ DERSİ" yazıyor; ders adını **Veri Yapıları ve Algoritmalar** olarak düzelt. Tarih: Ocak 2026.

1. **Giriş** — problem (toplu taşımada rota bulma), amaç, raporun düzeni
2. **Materyal ve Metod**
   - 2.1 Kullanılan araçlar (C, gcc/clang, Make, VS Code, Git, valgrind)
   - 2.2 Kullanılan veri yapıları (her biri alt başlık: graf, heap, queue, stack, hash, Trie, AVL) — neden seçildi
   - 2.3 Algoritmalar (Dijkstra, BFS, A*, sıralamalar, binary search) + karmaşıklık tablosu
3. **Uygulama: Kent İçi Rota ve Hat Planlayıcı**
   - 3.1 Tasarım (modül diyagramı, veri modeli, aktarma modellemesi)
   - 3.2 Ekran görüntüleri / örnek senaryolar (ör. Kadıköy → Taksim)
   - 3.3 Performans ölçümleri (tablo + grafik)
4. **Sonuç ve Tartışma** — öğrenilenler, kısıtlar, gelecek çalışma, AI araçlarının kullanımı
5. **Kaynakça** (şart değil ama önerilmiş — CLRS, Dijkstra 1959, Hart/Nilsson/Raphael A* 1968)
6. **Ekler** — GitHub, Medium, demo videosu linki

---

## 9. Zaman planı (dokümandaki haftalara göre)

| Hafta | İş |
|---|---|
| 10 | Gereksinim, veri modeli, CSV dosyaları, CLI taslağı, **ekip bilgi formunu doldur** |
| 10–13 | Veri yapıları kütüphanesi (graf, heap, queue, stack, hash, Trie), CSV okuma, temel CLI, küçük testler |
| 12–14 | Dijkstra / BFS / A* / çok kriterli, sıralamalar, kapalı yol + undo/redo, performans ölçümleri |
| 13–15 | Kenar durumları, README, rapor, demo senaryosu, (isteğe bağlı) sunum + Medium yazısı |

## 10. Tek kişilik çalışma sırası
Dokümandaki A/B/C rolleri tek kişide birleşiyor. Önerilen sıra:
1. Veri yapıları (stack, queue, heap, hash, Trie) + birim testleri
2. CSV okuma + graf kurma (M4/M5/M8 verisi)
3. Dijkstra → BFS → çok kriterli → A*
4. CLI menü, kapalı yol + undo/redo, yoğunluk
5. Sıralamalar / binary search ile listeleme, performans ölçümleri
6. README, rapor, GitHub
