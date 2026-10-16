# MiniPaint

[English](#english) | [Türkçe](#türkçe)

---

## English

A fully custom paint program written **from scratch in pure C++ / Win32** — no frameworks, no external libraries, no copied code. Single source file, compiles into a single ~1.2 MB portable exe.

### Features

- **31 drawing tools**: brush, pencil, airbrush, eraser, shapes, text, selection, and more
- **Paint simulations**: watercolor, oil paint, wet canvas blending and other realistic media behaviors
- **Pixel-art mode**: dedicated palette, perfect-pixel line cleanup, pixel-perfect drawing aids
- **29 languages**: full UI translation, defaults to English, switchable anytime from the menu
- **20 image effects** and adjustment filters
- **32-step undo/redo** with a robust history engine
- **Customizable canvas sizes** and fullscreen-by-default startup
- **Fill tool** optimized for instant response (scanline flood fill)
- Opens and saves **BMP, PNG, JPEG, GIF, TIFF and ICO** (via WIC; the ICO container is written by hand), clipboard paste support
- Unsaved-changes protection on exit, new, and open — in every language
- The full source code is embedded in the exe itself (Help menu) — see `kaynak.rc`

### Building

Requires a MinGW-w64 toolchain (e.g. [w64devkit](https://github.com/skeeto/w64devkit)):

```sh
windres -i kaynak.rc -o kaynak.o
g++ -O2 -municode -mwindows -static -o minipaint.exe paint.cpp kaynak.o -lcomdlg32 -lgdi32 -luser32 -lole32 -loleaut32 -lwindowscodecs -luuid -lmsimg32
```

No dependencies to install — everything links statically against the Windows API.

### Files

| File | Description |
|------|-------------|
| `paint.cpp` | The entire program (single translation unit, ~5800 lines) |
| `kaynak.rc` | Resource script: app icon + embedded source code |
| `ikon.ico` | Application icon |

A ready-to-run `minipaint.exe` is attached to the [Releases](../../releases) page.

### License

[MIT](LICENSE) — free to use, modify, and include in your own projects.

---

## Türkçe

**Saf C++ / Win32 ile sıfırdan** yazılmış, tamamen özgün bir çizim programıdır — çatı (framework) yok, dış kütüphane yok, kopya kod yok. Tek kaynak dosyasından ~1.2 MB'lık taşınabilir bir exe üretir.

### Özellikler

- **31 çizim aracı**: fırça, kalem, airbrush, silgi, şekiller, metin, seçim ve dahası
- **Boya simülasyonları**: sulu boya, yağlı boya, ıslak tuval karışımı ve diğer gerçekçi ortam davranışları
- **Pixel-art modu**: özel palet, mükemmel-piksel çizgi temizliği, piksel hassasiyetinde çizim yardımcıları
- **29 dil**: tam arayüz çevirisi, varsayılan İngilizce, menüden istenildiği zaman değiştirilebilir
- **20 görüntü efekti** ve ayar filtresi
- Sağlam bir geçmiş motoruyla **32 adım geri al / yinele**
- **Özelleştirilebilir tuval boyutları** ve varsayılan olarak tam ekran açılış
- Anında tepki için optimize edilmiş **dolgu aracı** (tarama çizgili flood fill)
- **BMP, PNG, JPEG, GIF, TIFF ve ICO** açma / kaydetme (WIC ile; ICO kapsayıcısı elle yazıldı), panodan yapıştırma desteği
- Çıkışta, yeni belgede ve açma işleminde kaydedilmemiş değişiklik koruması — her dilde
- Kaynak kodunun tamamı exe'nin içine gömülüdür (Yardım menüsü) — bkz. `kaynak.rc`

### Derleme

MinGW-w64 araç zinciri gerekir (örn. [w64devkit](https://github.com/skeeto/w64devkit)):

```sh
windres -i kaynak.rc -o kaynak.o
g++ -O2 -municode -mwindows -static -o minipaint.exe paint.cpp kaynak.o -lcomdlg32 -lgdi32 -luser32 -lole32 -loleaut32 -lwindowscodecs -luuid -lmsimg32
```

Kurulacak bağımlılık yok — her şey Windows API'sine statik olarak bağlanır.

### Dosyalar

| Dosya | Açıklama |
|------|-------------|
| `paint.cpp` | Programın tamamı (tek derleme birimi, ~5800 satır) |
| `kaynak.rc` | Kaynak betiği: uygulama simgesi + gömülü kaynak kodu |
| `ikon.ico` | Uygulama simgesi |

Çalıştırmaya hazır `minipaint.exe` dosyası [Releases](../../releases) sayfasına eklenmiştir.

### Lisans

[MIT](LICENSE) — kullanmakta, değiştirmekte ve kendi projelerinize dahil etmekte özgürsünüz.
