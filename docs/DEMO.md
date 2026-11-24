# Sunum ve Demo Rehberi

Hocaya gösterirken izlenecek sıra, yazılacak girdiler ve her adımda ne
anlatılacağı. Toplam süre yaklaşık 8–10 dakika.

## 0. Hazırlık (sunumdan önce)

Terminali aç ve proje klasörüne geç:

```bash
cd ~/"Desktop/Üniversite/7.Dönem Dersleri/Veri Yapıları ve Algoritmalar/Proje"
```

Temiz derleme yap (hocanın önünde de yapılabilir, 2–3 saniye sürer):

```bash
make clean && make
```

Terminal yazı boyutunu büyüt (Cmd +), arka planı açık renk yap: projeksiyonda okunsun.

---

## 1. Problem ve model (1 dk, konuşarak)

> "Kent içi rota planlayıcı yaptım. Veri olarak Anadolu yakasındaki M4, M5
> ve M8 metrolarını kullandım: 57 istasyon, 3 hat. Aktarma noktaları
> Kozyatağı (M4–M8) ve Dudullu (M5–M8). Bostancı'da iki hattın istasyonu
> 1,6 km ayrı olduğu için onu 20 dakikalık yürüme aktarması olarak
> modelledim."

> "Grafın düğümleri durak değil, **(durak, hat) çiftleri**. Kozyatağı'nda
> iki düğüm var: M4'teki Kozyatağı ve M8'deki Kozyatağı. Aralarındaki kenar
> aktarma. Böylece algoritma hangi hatta olduğumu biliyor ve aktarmaları
> sayabiliyor."

İstersen `data/stops.csv` dosyasını açıp CSV girdisini göster.

---

## 2. Programı başlat

```bash
./bin/rota
```

Açılışta `57 durak, 3 hat, 57 baglanti yuklendi.` yazar ve menü gelir.

---

## 3. Durak arama: Trie + Hash  (menü 1)

| Yaz | Göster |
|---|---|
| `1` → `k` | "k" ile başlayan tüm duraklar alfabetik gelir |
| `1` → `kadikoy` | Türkçe karakter olmadan da bulur |

> "Tam adı hash tablosunda O(1) arıyorum, bulamazsam **Trie** ile önek
> araması yapıyorum. Türkçe karakterleri normalleştirdiğim için 'kadikoy'
> yazmak yeterli."

---

## 4. En hızlı rota: Dijkstra + Min-Heap  (menü 2)

`2` → Nereden: `kadikoy` → Nereye: `umraniye`

Beklenen: **60,7 dk, 2 aktarma** (M4 → Kozyatağı → M8 → Dudullu → M5)

> "Dijkstra'yı indeksli min-heap ile yazdım, decrease-key O(log n).
> Toplam süre; yolculuk, ortalama bekleme (sefer aralığının yarısı) ve
> aktarma yürüyüşünden oluşuyor. Rotayı sondan başa `prev` dizisiyle
> izliyorum, ters geldiği için **stack**'e itip sırayla çıkarıyorum."

---

## 5. Kapalı yol + Undo/Redo: Stack  (menü 5, 2, 8, 9)

1. `5` → Hat: `m8` → 1. istasyon: `kozyatagi` → 2. istasyon: `aysekadin`
   → "Kapatildi: M8 Kozyatağı - Ayşekadın"
2. `2` → `kadikoy` → `emin ali pasa`
   → rota artık **Bostancı'dan 20 dk yürüyerek** gidiyor (43,5 dk; normalde 29,9)
3. `8` (Geri al) → bağlantı yeniden açılır
4. `2` → `kadikoy` → `emin ali pasa` → yine Kozyatağı'ndan, 29,9 dk
5. `9` (Yinele) → tekrar kapanır; `8` ile tekrar aç

> "Yol kapatma işlemleri iki stack'te: undo ve redo. Geri alınca işlem redo
> stack'ine geçiyor. Yeni bir işlem yapınca redo temizleniyor, tıpkı metin
> editörlerindeki gibi. Kenarı silmiyorum, `closed` bayrağı tutuyorum;
> böylece açmak O(derece)."

Menüde undo/redo yanında köşeli parantez içindeki sayı stack boyutudur.

---

## 6. En az aktarma: 0-1 BFS + Queue  (menü 3)

`3` → `kartal` → `emin ali pasa` → **1 aktarma, 37,2 dk**

> "BFS'i 0-1 BFS olarak yazdım: aynı hatta ilerlemek 0, aktarma 1 maliyet.
> 0 maliyetli komşuyu kuyruğun önüne, 1'i arkaya ekliyorum (deque).
> Önce en az aktarma sayısını buluyorum, sonra o sayıyı aşmayan rotalar
> arasından en hızlısını seçiyorum. Yoksa Bostancı'daki 20 dk yürüyüşlü
> rotayı da seçebiliyordu."

---

## 7. Çok kriterli rota + canlı yoğunluk  (menü 4, 10)

1. `4` → `2` (Kalabalıktan kaçın) → `kartal` → `emin ali pasa`
   → Kalabalık Kozyatağı'na girmemek için **Bostancı'dan** gider;
   alt satırda "9,2 dk daha uzun, kalabalık yükü 17 → 13" yazar.
2. `10` → `1` → en kalabalık duraklar
   > "Önce quicksort ile ada göre, sonra **kararlı mergesort** ile yoğunluğa
   > göre sıraladım; eşit yoğunluktakiler alfabetik kalıyor."
3. `10` → `3` → canlı yoğunluk simülasyonu (rastgele yoğun saat)
4. `10` → `4` → başlangıç değerlerine dön

> "Çok kriterli rota ayrı bir algoritma değil; Dijkstra'nın kenar maliyetini
> ağırlıklarla hesaplıyorum: süre + aktarma cezası + yoğunluk cezası.
> En hızlı rota bunun {1, 0, 0} ağırlıklı hali."

---

## 8. Algoritma karşılaştırması: A*  (menü 11)

`11` → `kadikoy` → `sabiha`

| Algoritma | Süre | Açılan durum |
|---|---|---|
| Dijkstra | 54,6 dk | 43 |
| A* | 54,6 dk | 32 |

> "A* aynı sonucu daha az durum açarak buluyor. Sezgisel: kuş uçuşu mesafe
> bölü ağdaki en yüksek hız, gerçek süreyi hiç aşmıyor. Ama bu küçük ağda
> trigonometri maliyeti yüzünden süre olarak kazandırmıyor; büyük ağda
> 2 kat hızlı (bir sonraki adım)."

Çıkış: `0`

---

## 9. Testler ve performans (1–2 dk)

```bash
make test
```

> "310 kontrol var. Mesela 57×57 = 3249 durak çiftinin hepsinde A* ile
> Dijkstra'nın aynı sonucu verdiğini, sıralamaların C'nin qsort'u ile aynı
> sıralamayı ürettiğini kontrol ediyorum."

```bash
make bench
```

> "Gerçek veri küçük olduğu için sentetik ızgara şehir üreteci yazdım.
> 100 bin duraklı şehirde Dijkstra 12,6 ms, A* 6,6 ms. Ayrıntılar
> docs/KARMASIKLIK.md'de."

(`make bench` yaklaşık 2 saniye sürer; çıktının sonundaki "2) Rota algoritmalari"
bölümünü göster.)

---

## 10. Kapanış

- GitHub: https://github.com/akarsli/veri-yapilari-proje
- Kısıt: ağ neredeyse ağaç şeklinde, alternatif rota az. Marmaray eklenirse
  döngüler oluşur ve rota modları belirgin şekilde ayrışır; bunun için
  sadece CSV satırı eklemek yeterli.

---

## Olası sorular ve kısa cevaplar

**Neden AVL değil de Trie?**
Şart "AVL veya Trie". Durak adında otomatik tamamlama için Trie doğal seçim:
önek araması O(L), AVL'de önek araması bu kadar doğrudan değil.

**Dijkstra neden negatif ağırlıkla çalışmaz, sen ne yaptın?**
Bir düğüm heap'ten çıkınca kesinleşmiş sayılır; negatif kenar bunu bozar.
Çok kriterli rotada ağırlıklar negatifse fonksiyon `false` döndürüyor.

**Heap'te aynı düğüm birden fazla kez mi giriyor?**
Hayır. `pos[]` dizisiyle düğümün heap'teki yerini tutuyorum, daha kısa yol
bulunca decrease-key yapıyorum. Heap boyutu V'yi aşmıyor.

**A* sezgiselin neden doğru (kabul edilebilir)?**
Hiçbir rota hedefe ağdaki en yüksek hızdan daha hızlı gidemez; kuş uçuşu
mesafe de gerçek yoldan kısa. Yani tahmin gerçek süreyi asla aşmaz.
Testlerde 3249 çiftin hepsinde kontrol ediliyor.

**Mergesort'u neden kullandın, quicksort yetmez miydi?**
Mergesort kararlı. Yoğunluğa göre sıralarken önceki alfabetik sıra korunuyor;
quicksort bunu garanti etmez.

**Süreler gerçek mi?**
Hatların uçtan uca süreleri resmi (Vikipedi). İstasyonlar arası süreler bu
toplamın mesafeye orantılı paylaştırılması. Koordinatlar OpenStreetMap'ten.
Sefer aralığı, yürüme ve yoğunluk tahmini; data/README.md'de yazılı.

**Hatalı CSV verirsem ne olur?**
Çökmez; satırı `dosya:satır: açıklama` biçiminde raporlayıp atlar.
Testlerde bozuk dosyalarla denendi.

**Bellek sızıntısı var mı?**
AddressSanitizer ve macOS `leaks` aracıyla kontrol ettim: 0 sızıntı.
