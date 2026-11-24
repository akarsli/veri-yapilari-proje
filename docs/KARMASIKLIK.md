# Zaman ve Uzay Karmaşıklığı Analizi

Bu belge rapordaki "Zaman/Uzay Analizi" bölümü için hazırlandı. Teorik değerler
kaynak koddaki başlık dosyalarıyla (`include/*.h`) aynıdır. Ölçülen süreler
`make bench` çıktısından alındı (Apple Silicon Mac, `-O2`, n = 100 000).
Ham veriler `results/ds.csv` ve `results/routes.csv` dosyalarında.

**Gösterim**

| Sembol | Anlamı | Bu projede (M4+M5+M8) |
|---|---|---|
| n | yapıdaki eleman sayısı | — |
| S | durak sayısı | 57 |
| V | durum sayısı (durak, hat çiftleri) | 60 |
| E | yönlü kenar sayısı (2 × bağlantı + aktarma) | 2 × 57 + 6 = 120 |
| L | anahtar (durak adı) uzunluğu | ≤ 63 bayt |
| K | en az aktarma sayısı | 0–2 |
| d | bir durumun derecesi (kenar sayısı) | ≤ 3 |

---

## 1. Veri yapıları

### Stack (`src/stack.c`): dinamik dizi
| İşlem | Zaman | Uzay | Ölçülen | Projede kullanımı |
|---|---|---|---|---|
| push | O(1) amortize | O(n) toplam | 4,7 ns (push+pop) | rota geri izleme, undo/redo |
| pop / peek | O(1) | — | | |
| clear | O(1) | — | | redo geçmişini silme |

Kapasite dolunca iki katına çıkarılır. Bu yüzden n push'un toplam maliyeti O(n), push başına ortalama O(1) olur.

### Queue / Deque (`src/queue.c`): dairesel dizi
| İşlem | Zaman | Uzay | Ölçülen | Projede kullanımı |
|---|---|---|---|---|
| enqueue (arkaya) | O(1) amortize | O(n) | 9,1 ns (enq+deq) | 0-1 BFS: maliyeti 1 olan kenar |
| push_front (öne) | O(1) amortize | — | | 0-1 BFS: maliyeti 0 olan kenar |
| dequeue / peek | O(1) | — | | BFS'te sıradaki durum |

### İndeksli Min-Heap (`src/heap.c`)
| İşlem | Zaman | Uzay | Ölçülen | Projede kullanımı |
|---|---|---|---|---|
| push (ekle) | O(log n) | O(kapasite) | 21,5 ns | Dijkstra / A* sınır kümesi |
| decrease-key | O(log n) | — | 19,5 ns | daha kısa yol bulununca |
| pop (min çıkar) | O(log n) | — | 140,2 ns | sıradaki en ucuz durum |
| contains / empty | O(1) | — | | |
| clear | O(boyut) | — | | |

`pos[]` dizisi sayesinde decrease-key yerinde yapılır ve aynı düğüm heap'e ikinci kez girmez, heap boyutu V'yi aşmaz. pop'un push'tan yaklaşık 7 kat yavaş olmasının sebebi, her pop'ta kökten yaprağa kadar inilmesi (log n adım, her adımda iki karşılaştırma). Rastgele öncelikle push ise genellikle 1–2 adımda durur.

### Hash Tablosu (`src/hash.c`): ayrı zincirleme, FNV-1a
| İşlem | Ortalama | En kötü | Uzay | Ölçülen | Projede kullanımı |
|---|---|---|---|---|---|
| put | O(L) | O(n + L) | O(n) | 96,5 ns (rehash dahil) | ad → durak, hat kodu → hat |
| get | O(L) | O(n + L) | — | 19,9 ns | tam ad araması |
| remove | O(L) | O(n + L) | — | | |
| rehash | O(n) | O(n) | — | | doluluk > 0,75 olunca |

Anahtar uzunluğu sabit kabul edilirse ortalama O(1) olur. Kova sayısı iki katına çıktığı için rehash maliyeti amortize O(1)'dir. En kötü durum, bütün anahtarların aynı kovaya düşmesidir.

### Trie (`src/trie.c`): 37 harfli alfabe (a–z, 0–9, boşluk)
| İşlem | Zaman | Uzay | Ölçülen | Projede kullanımı |
|---|---|---|---|---|
| insert | O(L) | O(L · 37) yeni düğüm | 274,5 ns | durak adlarını dizine ekleme |
| exact | O(L) | — | | |
| prefix (k sonuç) | O(L + gezilen alt ağaç) | O(k) | 3,4 µs (ilk 10 sonuç) | "kad" → Kadıköy (otomatik tamamlama) |

Bellek: düğüm başına 37 işaretçi (yaklaşık 312 bayt). 100 000 rastgele anahtar 606 844 düğüm, yani **~181 MB** tutuyor. 57 durak için ise yaklaşık 0,1 MB. Zaman–bellek ödünleşimine iyi bir örnek: çocukları bağlı liste ya da hash ile tutmak belleği azaltır ama her adımı yavaşlatır.

### Sıralama ve arama (`src/sort.c`)
| Algoritma | En iyi | Ortalama | En kötü | Ek uzay | Kararlı | Ölçülen (n = 10⁵) |
|---|---|---|---|---|---|---|
| merge_sort | O(n log n)* | O(n log n) | O(n log n) | O(n) | **Evet** | 5,94 ms |
| quick_sort | O(n log n) | O(n log n) | O(n²) | O(log n) | Hayır | 6,62 ms (sıralı girdi 1,44 ms) |
| C `qsort` (karşılaştırma) | | | | | | 5,45 ms |
| binary_search | O(1) | O(log n) | O(log n) | O(1) | — | 59,4 ns/arama |

\* Mergesort, birleştirmeden önce iki yarının zaten sıralı olup olmadığına bakar. Sıralı girdide birleştirme adımları atlanır.

Quicksort'ta üçün ortancası pivot seçildiği için sıralı ve ters sıralı girdide O(n²) olmaz. Küçük parçada özyineleme, büyük parçada döngü kullanıldığı için yığın derinliği O(log n)'dir. 16 elemandan küçük parçalar insertion sort ile sıralanır.

**Projede kullanımı:**
- quick_sort: durakları id'ye göre sıralar (yüklemede), yoğunluk listesinde ön sıralamayı yapar
- merge_sort: yoğunluğa göre kararlı sıralama yapar, eşit yoğunluktaki duraklar alfabetik sırada kalır
- binary_search: CSV'deki durak id'sini dizi indeksine çevirir

---

## 2. Graf işlemleri (`src/graph.c`)

| İşlem | Zaman | Açıklama |
|---|---|---|
| Durak ekleme | O(1) amortize | dinamik dizi |
| Durak dizinleme (`network_index_stops`) | O(S log S + S · L) | quicksort + hash + trie |
| id → durak | O(log S) | binary search |
| (durak, hat) → durum | O(durağın hat sayısı) ≤ 8 | durağın küçük durum dizisi |
| Bağlantı ekleme (RIDE) | O(1) amortize | iki yönlü, iki kenar |
| Aktarma ekleme | O(1) amortize | |
| Ad araması | O(L) ortalama (hash), değilse O(L + alt ağaç) (trie) | |
| Yol kapatma / açma | O(d) | kenarın `closed` bayrağı; graftan silinmez |
| Bağlantı durumu sorgusu | O(d) | |
| CSV'den tüm ağı yükleme | O(S log S + E log S) | her kenar satırında 2 binary search |
| **Graf belleği** | **O(S + V + E)** | adjacency list |

Kapalı yol için kenarı silmek yerine bayrak tutulması bilinçli bir tercih: açma ve undo işlemleri bellek ayırmadan O(d)'de yapılabiliyor. Bedeli, arama sırasında kapalı kenarların da gezilip atlanması.

---

## 3. Rota algoritmaları (`src/route.c`)

| Algoritma | Zaman | Uzay | Açılan durum, gerçek veri (ort.) | 100K ızgara (ort.) |
|---|---|---|---|---|
| Dijkstra (en hızlı) | O((V + E) log V) | O(V) | 30,7 / 60 | 12,6 ms, %63 |
| A* (en hızlı) | O((V + E) log V) en kötü | O(V + S) | 25,4 / 60 | **6,6 ms, %27** |
| Ağırlıklı Dijkstra (çok kriterli) | O((V + E) log V) | O(V) | — | — |
| 0-1 BFS (en az aktarma sayısı) | **O(V + E)** | O(V) | — | — |
| Katmanlı Dijkstra (≤ K aktarma, en hızlı) | O(K (V + E) log(K V)) | O(K V) | 67,0 (iki aşama toplamı) | 10,3 ms |
| Rota geri izleme | O(rota uzunluğu) | O(rota uzunluğu) | stack ile | |
| Rota özeti / yazdırma | O(rota uzunluğu · d) | O(1) | | |

**Notlar**
- A*'ın sezgiseli, *kuş uçuşu mesafe ÷ ağdaki en yüksek hız*. Kabul edilebilir (admissible) ve tutarlı (consistent) olduğu için sonuç Dijkstra ile birebir aynı çıkıyor. Bu, testlerde 3249 çiftin hepsinde doğrulanıyor.
- A*'ın çok küçük ağda (60 durum) Dijkstra'dan yavaş olabilmesinin sebebi haversine hesabındaki trigonometri maliyeti. Ağ büyüdükçe açılan durumdaki azalma bu maliyeti geçiyor: 1K, 10K ve 100K durakta yaklaşık 1,7–1,9 kat hızlanma ölçüldü.
- 0-1 BFS, kenar ağırlıkları yalnızca 0 ve 1 olduğu için heap yerine çift uçlu kuyruk kullanıyor ve log V çarpanından kurtuluyor.
- Katmanlı grafta K genellikle 1–2 olduğundan pratikteki maliyet Dijkstra'nın birkaç katı ile sınırlı kalıyor.

---

## 4. Undo / Redo (`src/history.c`)

| İşlem | Zaman | Uzay |
|---|---|---|
| apply (kapat/aç + kaydet) | O(d) + O(1) amortize push | O(işlem sayısı) |
| undo | O(1) pop + O(d) + O(1) push | |
| redo | O(1) pop + O(d) + O(1) push | |
| yeni işlemde redo temizleme | O(1) | |
