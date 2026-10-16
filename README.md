# MiniPaint

A fully custom paint program written **from scratch in pure C++ / Win32** — no frameworks, no external libraries, no copied code. Single source file, compiles into a single ~1.2 MB portable exe.

## Features

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

## Building

Requires a MinGW-w64 toolchain (e.g. [w64devkit](https://github.com/skeeto/w64devkit)):

```sh
windres -i kaynak.rc -o kaynak.o
g++ -O2 -municode -mwindows -static -o minipaint.exe paint.cpp kaynak.o -lcomdlg32 -lgdi32 -luser32 -lole32 -loleaut32 -lwindowscodecs -luuid -lmsimg32
```

No dependencies to install — everything links statically against the Windows API.

## Files

| File | Description |
|------|-------------|
| `paint.cpp` | The entire program (single translation unit, ~5800 lines) |
| `kaynak.rc` | Resource script: app icon + embedded source code |
| `ikon.ico` | Application icon |

A ready-to-run `minipaint.exe` is attached to the [Releases](../../releases) page.

## License

[MIT](LICENSE) — free to use, modify, and include in your own projects.

---

## Türkçe

MiniPaint, **saf C++ / Win32 ile sıfırdan** yazılmış, hiçbir dış kütüphane veya kopya kod içermeyen, tamamen özgün bir çizim programıdır. Tek kaynak dosyasından ~1.2 MB'lık taşınabilir bir exe üretir.

- 31 çizim aracı, sulu boya / yağlı boya gibi gerçekçi boya simülasyonları
- Pixel-art modu: özel palet ve mükemmel-piksel çizim yardımcıları
- 29 dil desteği (varsayılan İngilizce, menüden değiştirilebilir)
- 20 görüntü efekti, 32 adım geri al / yinele
- **BMP, PNG, JPEG, GIF, TIFF ve ICO** açma / kaydetme (ICO kapsayıcısı elle yazıldı)
- Anında tepki veren optimize dolgu aracı
- Kaydedilmemiş değişiklik koruması (tüm dillerde)

Derleme için yukarıdaki MinGW-w64 komutlarını kullanın. Lisans: MIT.
