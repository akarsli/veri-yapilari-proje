"""M4 / M5 / M8 hat haritası: data/*.csv dosyalarından docs/harita.png ve
docs/harita.svg üretir.

Kullanım (proje kökünden):  python3 tools/harita.py

Koordinatlar coğrafidir (OpenStreetMap); sadece boylam, enlemin kosinüsüyle
ölçeklenir ki km'ler iki eksende eşit görünsün.
"""
import csv
import math
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib import font_manager  # noqa: E402

DATA = "data"
OUT = "docs/harita"

# Ortak istasyonların stops.csv'de iki hattın ortalaması olarak tutulan
# koordinatı yerine, haritada her hattın kendi istasyonu gösterilir
# (OSM, iki yönün ortalaması). Bostancı'da iki istasyon ~1,6 km ayrıdır.
LINE_SPECIFIC = {
    ("Bostancı", "M4"): (40.964568, 29.105361),
    ("Bostancı", "M8"): (40.951694, 29.096746),
    ("Kozyatağı", "M4"): (40.975286, 29.099675),
    ("Kozyatağı", "M8"): (40.977499, 29.099890),
    ("Dudullu", "M5"): (41.015238, 29.162873),
    ("Dudullu", "M8"): (41.015206, 29.163339),
}

# Etiket yerleşimi: (dx, dy) nokta cinsinden kayma, hizalama, açı
# Varsayılanlar hat yönüne göre; sıkışık istasyonlar için tek tek düzeltme.
LINE_LABEL = {
    "M4": dict(dx=-7, dy=-7, ha="right", va="top", rot=35),
    "M5": dict(dx=7, dy=7, ha="left", va="bottom", rot=35),
    "M8": dict(dx=9, dy=0, ha="left", va="center", rot=0),
}
OVERRIDE = {
    ("Kadıköy", "M4"): dict(dx=-9, dy=0, ha="right", va="center", rot=0),
    ("Üsküdar", "M5"): dict(dx=-9, dy=0, ha="right", va="center", rot=0),
    ("Sultanbeyli", "M5"): dict(dx=9, dy=0, ha="left", va="center", rot=0),
    ("Sabiha Gökçen Havalimanı", "M4"): dict(dx=9, dy=-4, ha="left", va="top", rot=0),
    ("Kurtköy", "M4"): dict(dx=0, dy=10, ha="center", va="bottom", rot=0),
    ("Yayalar-Şeyhli", "M4"): dict(dx=-8, dy=6, ha="right", va="bottom", rot=0),
    ("Parseller", "M8"): dict(dx=0, dy=10, ha="center", va="bottom", rot=0),
    ("Huzur", "M8"): dict(dx=9, dy=0, ha="left", va="center", rot=0),
    ("Bostancı", "M8"): dict(dx=-9, dy=0, ha="right", va="center", rot=0),
    ("Emin Ali Paşa", "M8"): dict(dx=-9, dy=0, ha="right", va="center", rot=0),
    ("Ayşekadın", "M8"): dict(dx=-9, dy=0, ha="right", va="center", rot=0),
    ("Ayrılık Çeşmesi", "M4"): dict(dx=-8, dy=4, ha="right", va="bottom", rot=0),
    ("Yenisahra", "M4"): dict(dx=7, dy=7, ha="left", va="bottom", rot=35),
    ("Bostancı", "M4"): dict(dx=9, dy=0, ha="left", va="center", rot=0),
    ("Tavşantepe", "M4"): dict(dx=0, dy=-11, ha="center", va="top", rot=0),
    ("Fevzi Çakmak-Hastane", "M4"): dict(dx=10, dy=-3, ha="left", va="top", rot=0),
    ("Bulgurlu", "M5"): dict(dx=0, dy=-11, ha="center", va="top", rot=0),
    ("İmam Hatip Lisesi", "M5"): dict(dx=-6, dy=-8, ha="right", va="top", rot=35),
    ("Küçükbakkalköy", "M8"): dict(dx=6, dy=8, ha="left", va="bottom", rot=35),
    ("İçerenköy", "M8"): dict(dx=6, dy=-8, ha="left", va="top", rot=-35),
}
TRANSFER_LABEL = {
    # aktarma istasyonlarının adı bir kez, kalın yazılır
    "Kozyatağı": ("M8", dict(dx=-11, dy=8, ha="right", va="bottom", rot=0)),
    "Dudullu": ("M5", dict(dx=10, dy=-8, ha="left", va="top", rot=0)),
}


def pick_font():
    for name in ("Arial", "Helvetica Neue", "DejaVu Sans"):
        try:
            font_manager.findfont(name, fallback_to_default=False)
            return name
        except ValueError:
            continue
    return "DejaVu Sans"


def load():
    stops = {}
    with open(f"{DATA}/stops.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            stops[int(r["id"])] = (r["name"], float(r["lat"]), float(r["lon"]))
    lines = {}
    with open(f"{DATA}/lines.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            lines[r["line_id"]] = r
    # Her hattın istasyon sırası: kenarlar CSV'de hat boyunca sıralı
    order = defaultdict(list)
    with open(f"{DATA}/edges.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            a, b, line = int(r["from_id"]), int(r["to_id"]), r["line_id"]
            if not order[line]:
                order[line].append(a)
            order[line].append(b)
    transfers = {}
    with open(f"{DATA}/transfers.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            transfers[stops[int(r["stop_id"])][0]] = float(r["walk_min"])
    return stops, lines, order, transfers


def main():
    stops, lines, order, transfers = load()
    lat0 = 41.0
    kx = math.cos(math.radians(lat0))

    def xy(name, line, lat, lon):
        lat, lon = LINE_SPECIFIC.get((name, line), (lat, lon))
        return lon * kx, lat

    plt.rcParams["font.family"] = pick_font()
    fig, ax = plt.subplots(figsize=(16, 10.5), dpi=100)
    fig.patch.set_facecolor("white")
    ax.set_facecolor("white")
    ax.set_aspect("equal")
    ax.axis("off")

    pos = {}   # (ad, hat) -> (x, y)
    for line_id, ids in order.items():
        color = lines[line_id]["color"]
        pts = []
        for sid in ids:
            name, lat, lon = stops[sid]
            p = xy(name, line_id, lat, lon)
            pos[(name, line_id)] = p
            pts.append(p)
        xs, ys = zip(*pts)
        ax.plot(xs, ys, color=color, lw=7, solid_capstyle="round", solid_joinstyle="round", zorder=2)

    # Aktarma bağlantıları
    for name, walk in transfers.items():
        ends = [p for (n, _), p in pos.items() if n == name]
        (x1, y1), (x2, y2) = ends[0], ends[1]
        if name == "Bostancı":
            ax.plot([x1, x2], [y1, y2], color="#555555", lw=2, ls=(0, (3, 2.5)), zorder=1)
            bx, by = min((x1, y1), (x2, y2), key=lambda q: q[1])   # güneydeki (M8) istasyon
            ax.annotate("~1,6 km yürüme\n(20 dk aktarma)", (bx, by), xytext=(0, -13), textcoords="offset points",
                        fontsize=8.5, color="#444444", ha="center", va="top", style="italic")
        else:
            ax.plot([x1, x2], [y1, y2], color="#222222", lw=9, solid_capstyle="round", zorder=3)
            ax.plot([x1, x2], [y1, y2], color="white", lw=5.5, solid_capstyle="round", zorder=3.1)

    # İstasyonlar
    transfer_names = set(transfers)
    for (name, line_id), (x, y) in pos.items():
        color = lines[line_id]["color"]
        if name in transfer_names and name != "Bostancı":
            ax.scatter([x], [y], s=95, facecolor="white", edgecolor="#222222", linewidth=2.2, zorder=4)
        else:
            ax.scatter([x], [y], s=55, facecolor="white", edgecolor=color, linewidth=2.2, zorder=4)

        if name in TRANSFER_LABEL:
            label_line, spec = TRANSFER_LABEL[name]
            if line_id != label_line:
                continue
            weight = "bold"
        else:
            spec = OVERRIDE.get((name, line_id), LINE_LABEL[line_id])
            weight = "bold" if name == "Bostancı" or (name, line_id) in (
                ("Kadıköy", "M4"), ("Üsküdar", "M5"), ("Sultanbeyli", "M5"),
                ("Sabiha Gökçen Havalimanı", "M4"), ("Bostancı", "M8"), ("Parseller", "M8")) else "normal"
        ax.annotate(name, (x, y), xytext=(spec["dx"], spec["dy"]), textcoords="offset points",
                    ha=spec["ha"], va=spec["va"], rotation=spec["rot"], rotation_mode="anchor",
                    fontsize=9.5, fontweight=weight, color="#1a1a1a", zorder=5)

    # Kenar boşlukları: etiketler ve lejant sığsın
    xmin, xmax = ax.get_xlim()
    ymin, ymax = ax.get_ylim()
    ax.set_xlim(xmin - 0.012, xmax + 0.030)
    ax.set_ylim(ymin - 0.010, ymax + 0.030)
    xmin, xmax = ax.get_xlim()
    ymin, ymax = ax.get_ylim()

    # Başlık (sol üst) ve lejant (sol alttaki boş alan)
    ax.text(0.015, 0.985, "İstanbul Anadolu Yakası Metro Hatları — M4 · M5 · M8", transform=ax.transAxes,
            fontsize=17, fontweight="bold", va="top")
    y = 0.34
    for line_id in ("M4", "M5", "M8"):
        l = lines[line_id]
        n = len(order[line_id])
        ax.plot([0.017, 0.055], [y - 0.008, y - 0.008], transform=ax.transAxes, color=l["color"], lw=7,
                solid_capstyle="round")
        ax.text(0.065, y, f"{line_id}  {l['name']}", transform=ax.transAxes, fontsize=11.5, fontweight="bold",
                va="top")
        ax.text(0.065, y - 0.026, f"{n} istasyon · uçtan uca {float(l['total_min']):.0f} dk · "
                f"sefer aralığı ~{float(l['headway_min']):.0f} dk", transform=ax.transAxes, fontsize=9.5,
                color="#444444", va="top")
        y -= 0.065
    ax.scatter([0.036], [y - 0.008], s=95, facecolor="white", edgecolor="#222222", linewidth=2.2,
               transform=ax.transAxes)
    ax.text(0.065, y, "Aktarma istasyonu: Kozyatağı (M4–M8, 6 dk), Dudullu (M5–M8, 4 dk)",
            transform=ax.transAxes, fontsize=9.5, va="top")
    y -= 0.035
    ax.plot([0.017, 0.055], [y - 0.008, y - 0.008], transform=ax.transAxes, color="#555555", lw=2,
            ls=(0, (3, 2.5)))
    ax.text(0.065, y, "Bostancı: iki hattın istasyonu ayrı, yürüyerek aktarma",
            transform=ax.transAxes, fontsize=9.5, va="top")

    # Ölçek çubuğu (2 km) ve kuzey oku: sağ üstteki boş alan
    km_x = 1 / 111.32                   # ölçeklenmiş eksende 1 km = 1/111.32 birim
    sx, sy = xmax - (xmax - xmin) * 0.17, ymax - (ymax - ymin) * 0.07
    ax.plot([sx, sx + 2 * km_x], [sy, sy], color="black", lw=3)
    for t in (0, 1, 2):
        ax.plot([sx + t * km_x] * 2, [sy - 0.0006, sy + 0.0006], color="black", lw=1.5)
    ax.text(sx + km_x, sy + 0.0012, "2 km", ha="center", va="bottom", fontsize=9.5)
    ax.annotate("K", xy=(sx + 3.2 * km_x, sy + 0.012), xytext=(sx + 3.2 * km_x, sy + 0.001),
                ha="center", va="bottom", fontsize=11, fontweight="bold",
                arrowprops=dict(arrowstyle="-|>", color="black", lw=1.5))

    ax.text(0.99, 0.01, "Koordinatlar: © OpenStreetMap katkıda bulunanları (ODbL) · Süreler: Vikipedi, "
            "Metro İstanbul · Kasım 2026", transform=ax.transAxes, fontsize=8, color="#666666",
            ha="right", va="bottom")

    fig.tight_layout()
    fig.savefig(f"{OUT}.png", dpi=200, facecolor="white")
    fig.savefig(f"{OUT}.svg", facecolor="white")
    print(f"{OUT}.png ve {OUT}.svg yazildi")


if __name__ == "__main__":
    main()
