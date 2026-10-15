# MiniPaint

A fully custom paint program written **from scratch in pure C++ / Win32** — no frameworks, no external libraries, no copied code. Single source file, compiles into a single ~1 MB portable exe.

## Features

- **31 drawing tools**: brush, pencil, airbrush, eraser, shapes, text, selection, and more
- **Paint simulations**: watercolor, oil paint, wet canvas blending and other realistic media behaviors
- **Pixel-art mode**: dedicated palette, perfect-pixel line cleanup, pixel-perfect drawing aids
- **29 languages**: full UI translation, defaults to English, switchable anytime from the menu
- **20 image effects** and adjustment filters
- **Unlimited undo/redo** with a robust history engine
- **Customizable canvas sizes** and fullscreen-by-default startup
- **Fill tool** optimized for instant response (scanline flood fill)
- Saves/loads images via **WIC** (PNG, JPEG, BMP, ...), clipboard paste support
- Unsaved-changes protection on exit, new, and open — in every language
- The full source code is embedded in the exe itself (Help menu) — see `kaynak.rc`

## Building

Requires a MinGW-w64 toolchain (e.g. [w64devkit](https://github.com/skeeto/w64devkit)):

```sh
windres -i kaynak.rc -o kaynak.o
g++ -O2 -municode -mwindows -static -o minipaint.exe paint.cpp kaynak.o -lcomdlg32 -lgdi32 -luser32 -lole32 -lwindowscodecs -luuid -lmsimg32
```

No dependencies to install — everything links statically against the Windows API.

## Files

| File | Description |
|------|-------------|
| `paint.cpp` | The entire program (single translation unit, ~5200 lines) |
| `kaynak.rc` | Resource script: app icon + embedded source code |
| `ikon.ico` | Application icon |

A ready-to-run `minipaint.exe` is attached to the [Releases](../../releases) page.

## License

[MIT](LICENSE) — free to use, modify, and include in your own projects.

---

## Turkce

MiniPaint, **saf C++ / Win32 ile sifirdan** yazilmis, hicbir dis kutuphane veya kopya kod icermeyen, tamamen ozgun bir cizim programidir. Tek kaynak dosyasindan ~1 MB'lik tasiyanabilir bir exe uretir.

- 31 cizim araci, sulu boya / yagli boya gibi gercekci boya simulasyonlari
- Pixel-art modu: ozel palet ve mukemmel-piksel cizim yardimcilari
- 29 dil destegi (varsayilan Ingilizce, menuden degistirilebilir)
- 20 goruntu efekti, sinirsiz geri al / yinele
- Aninda tepki veren optimize dolgu araci
- Kaydedilmemis degisiklik korumasi (tum dillerde)

Derleme icin yukaridaki MinGW-w64 komutlarini kullanin. Lisans: MIT.
