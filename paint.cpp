// ============================================================
//  MiniPaint v2.2 - Tamamen sifirdan yazilmis ozel cizim programi
//  Dil: C++ (saf Win32 API + WIC, harici kutuphane YOK)
//  Bu kodun tamami bu proje icin ozgun olarak yazilmistir.
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#include <oleauto.h>
#include <wincodec.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

// ------------------------------------------------------------
//  Sabitler
// ------------------------------------------------------------
static const int TOOLBAR_H   = 152;  // ust arac cubugu yuksekligi (dort satir)
static const int STATUSBAR_H = 24;   // alt durum cubugu yuksekligi
static const int PALETTE_CNT = 60;   // hazir renk sayisi (5 satir x 12 renk)
static const int GECMIS_MAX  = 32;   // geri alma derinligi

enum Arac {
    ARAC_KALEM = 0, ARAC_CIZGI, ARAC_DIKDORTGEN, ARAC_ELIPS,
    ARAC_DOLGU, ARAC_SILGI, ARAC_DAMLALIK,
    ARAC_SPREY, ARAC_UCGEN, ARAC_YUVARLAK, ARAC_YILDIZ,
    ARAC_METIN, ARAC_SECIM,
    ARAC_EGRI, ARAC_ELMAS, ARAC_BESGEN, ARAC_ALTIGEN, ARAC_OK, ARAC_KALP,
    ARAC_COKGEN, ARAC_BULANIK, ARAC_DAMGA,
    ARAC_SULUBOYA, ARAC_ISLAK, ARAC_YAGLIFIRCA,
    ARAC_KARAKALEM, ARAC_KALIGRAFI, ARAC_SERIT,
    ARAC_NEON, ARAC_GOLGE, ARAC_DITHER,
    ARAC_GPEN, ARAC_TON, ARAC_HIZCIZGI, ARAC_BALON, ARAC_PANEL,   // manga/arac seti
    ARAC_SAYISI
};

// Hazir renk paleti (RGB) - tam spektrum:
// 1. satir gri tonlari, 2. satir canli renkler, 3. satir pastel,
// 4. satir orta koyu, 5. satir koyu tonlar (12 renk tonu x 4 golge)
static const COLORREF PALET_VARSAYILAN[PALETTE_CNT] = {
    RGB(  0,  0,  0), RGB( 23, 23, 23), RGB( 46, 46, 46), RGB( 70, 70, 70),
    RGB( 93, 93, 93), RGB(116,116,116), RGB(139,139,139), RGB(162,162,162),
    RGB(185,185,185), RGB(209,209,209), RGB(232,232,232), RGB(255,255,255),
    RGB(255,  0,  0), RGB(255,128,  0), RGB(255,255,  0), RGB(128,255,  0),
    RGB(  0,255,  0), RGB(  0,255,128), RGB(  0,255,255), RGB(  0,128,255),
    RGB(  0,  0,255), RGB(128,  0,255), RGB(255,  0,255), RGB(255,  0,128),
    RGB(255,140,140), RGB(255,198,140), RGB(255,255,140), RGB(198,255,140),
    RGB(140,255,140), RGB(140,255,198), RGB(140,255,255), RGB(140,198,255),
    RGB(140,140,255), RGB(198,140,255), RGB(255,140,255), RGB(255,140,198),
    RGB(184,  0,  0), RGB(184, 92,  0), RGB(184,184,  0), RGB( 92,184,  0),
    RGB(  0,184,  0), RGB(  0,184, 92), RGB(  0,184,184), RGB(  0, 92,184),
    RGB(  0,  0,184), RGB( 92,  0,184), RGB(184,  0,184), RGB(184,  0, 92),
    RGB(115,  0,  0), RGB(115, 57,  0), RGB(115,115,  0), RGB( 57,115,  0),
    RGB(  0,115,  0), RGB(  0,115, 57), RGB(  0,115,115), RGB(  0, 57,115),
    RGB(  0,  0,115), RGB( 57,  0,115), RGB(115,  0,115), RGB(115,  0, 57)
};

// Aktif palet: varsayilan ya da piksel paletinin kopyasi (PaletUygula doldurur)
static COLORREF PALET[PALETTE_CNT];
// Piksel sanati icin islemsel uretilen palet (ilk kullanimda bir kez dolar)
static COLORREF PALET_PIKSEL[PALETTE_CNT];
static bool g_paletPikselHazir = false;

// Palet renklerinin hazir fircalari: PaletUygula'da bir kez yaratilir.
// Eskiden her WM_PAINT'te 60+ firca yaratilip siliniyordu (GDI kirilanmasi).
static HBRUSH g_paletFirca[PALETTE_CNT] = {};

// HSV -> RGB donusumu (piksel paletini kod icinde uretmek icin)
static COLORREF HsvRenk(double h, double s, double v)
{
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;
    double r = 0, y = 0, b = 0;
    if      (h <  60) { r = c; y = x; }
    else if (h < 120) { r = x; y = c; }
    else if (h < 180) { y = c; b = x; }
    else if (h < 240) { y = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }
    return RGB((int)((r + m) * 255 + 0.5),
               (int)((y + m) * 255 + 0.5),
               (int)((b + m) * 255 + 0.5));
}

// Piksel sanati paleti: 11 renk tonu + 1 gri eksen, her biri 5 ton derinligi
// (canli / pastel / orta / koyu / cok koyu) - golgeleme rampalari hazir gelir
static void PaletPikselOlustur()
{
    if (g_paletPikselHazir) return;
    g_paletPikselHazir = true;
    static const double V[5] = { 1.00, 1.00, 0.78, 0.52, 0.30 };
    static const double S[5] = { 1.00, 0.42, 0.92, 0.95, 1.00 };
    static const double GV[5] = { 1.00, 0.72, 0.48, 0.28, 0.10 };
    for (int sutun = 0; sutun < 12; sutun++) {
        for (int satir = 0; satir < 5; satir++) {
            int i = satir * 12 + sutun;
            if (sutun == 11) {   // gri eksen
                int t = (int)(GV[satir] * 255 + 0.5);
                PALET_PIKSEL[i] = RGB(t, t, t);
            } else {
                PALET_PIKSEL[i] = HsvRenk(sutun * 30.0, S[satir], V[satir]);
            }
        }
    }
}

// Yaklasim (zoom) kademeleri - yuzde
static const int ZOOM_ADIM[] = {12, 25, 50, 75, 100, 150, 200, 300, 400, 600, 800};
static const int ZOOM_SAYISI = 11;

// ------------------------------------------------------------
//  Coklu dil destegi
// ------------------------------------------------------------
enum Dil {
    DIL_TR = 0, DIL_EN, DIL_DE, DIL_FR, DIL_ES,
    DIL_IT, DIL_PT, DIL_RU, DIL_JA, DIL_ZH,
    DIL_NL, DIL_PL, DIL_UK, DIL_KO, DIL_AR, DIL_HI,
    DIL_ID, DIL_VI, DIL_TH, DIL_EL,
    DIL_CS, DIL_SV, DIL_HU, DIL_RO,
    DIL_DA, DIL_FI, DIL_NO,
    DIL_BG, DIL_HR,
    DIL_SAYISI
};

static const wchar_t* DIL_AD[DIL_SAYISI] = {
    L"Türkçe", L"English", L"Deutsch", L"Français", L"Español",
    L"Italiano", L"Português", L"Русский", L"日本語", L"中文",
    L"Nederlands", L"Polski", L"Українська", L"한국어", L"العربية", L"हिन्दी",
    L"Bahasa Indonesia", L"Tiếng Việt", L"ไทย", L"Ελληνικά",
    L"Čeština", L"Svenska", L"Magyar", L"Română",
    L"Dansk", L"Suomi", L"Norsk",
    L"Български", L"Hrvatski"
};

enum MetinId {
    M_ARAC_KALEM = 0, M_ARAC_CIZGI, M_ARAC_KUTU, M_ARAC_ELIPS,
    M_ARAC_DOLGU, M_ARAC_SILGI, M_ARAC_DAMLALIK,
    M_ARAC_SPREY, M_ARAC_UCGEN, M_ARAC_YUVARLAK, M_ARAC_YILDIZ,
    M_ARAC_METIN, M_ARAC_SECIM,
    M_ARAC_EGRI, M_ARAC_ELMAS, M_ARAC_BESGEN, M_ARAC_ALTIGEN, M_ARAC_OK, M_ARAC_KALP,
    M_MENU_DOSYA, M_MENU_YENI, M_MENU_AC, M_MENU_KAYDET, M_MENU_FARKLI, M_MENU_CIKIS,
    M_MENU_DUZEN, M_MENU_GERIAL, M_MENU_YINELE, M_MENU_TEMIZLE,
    M_MENU_RESIM, M_DON90, M_DON270, M_AYNAY, M_AYNAD, M_BOYUTLANDIR,
    M_MENU_EFEKT, M_EF_GRI, M_EF_NEG, M_EF_SEPYA, M_EF_BULANIK, M_EF_KESKIN,
    M_EF_PIKSEL, M_EF_PAR_ARTI, M_EF_PAR_EKSI,
    M_EF_EMBOSS, M_EF_KENAR, M_EF_POSTER, M_EF_GREN,
    M_EF_KON_ARTI, M_EF_KON_EKSI, M_EF_DOY_ARTI, M_EF_DOY_EKSI,
    M_EF_GAUSS, M_EF_YAGLI, M_EF_SOLAR, M_EF_KANAL,
    M_MENU_GORUNUM, M_IZGARA, M_Z100, M_SIGDIR, M_SIMETRI,
    M_MENU_YARDIM, M_MENU_HAKKINDA, M_MENU_DIL,
    M_BASLIK, M_DURUM_FMT,
    M_HATA_BASLIK, M_HATA_ACMA,
    M_KAPANIS_SORU, M_HAKKINDA_METIN, M_FILTRE,
    M_TAMAM, M_IPTAL, M_GENISLIK, M_YUKSEKLIK,
    M_RENK_BASLIK, M_BOYUT_BASLIK, M_DOLU_ETIKET, M_UC_YUVARLAK, M_UC_KARE,
    M_ARAC_COKGEN, M_ARAC_BULANIK, M_ARAC_DAMGA,
    M_MENU_KILAVUZ, M_KILAVUZ_BASLIK, M_KILAVUZ_METIN,
    M_MENU_KAYNAK, M_KAYNAK_BASLIK,
    M_ARAC_SULUBOYA, M_ARAC_ISLAK, M_ARAC_YAGLI,
    M_ARAC_KARAKALEM, M_ARAC_KALIGRAFI, M_ARAC_SERIT,
    M_ARAC_NEON, M_ARAC_GOLGE,
    M_MENU_REFERANS, M_REF_YUKLE, M_REF_GOSTER, M_REF_KALDIR, M_MENU_STABIL,
    M_ARAC_DITHER, M_PIKSEL_MUK, M_MENU_PALET_PIKSEL,
    M_ARAC_GPEN, M_ARAC_TON, M_ARAC_HIZCIZGI, M_ARAC_BALON, M_ARAC_PANEL,
    M_HATA_KAYDET,
    M_METIN_SAYISI
};

static const wchar_t* METIN[DIL_SAYISI][M_METIN_SAYISI] = {
// ---- Turkce ----
{ L"Kalem", L"Cizgi", L"Kutu", L"Elips", L"Dolgu", L"Silgi", L"Renk Al",
  L"Sprey", L"Ucgen", L"Yuvarlak", L"Yildiz", L"Metin", L"Secim",
  L"Egri", L"Elmas", L"Besgen", L"Altigen", L"Ok", L"Kalp",
  L"&Dosya", L"&Yeni", L"&Ac...", L"&Kaydet", L"Farkli Ka&ydet...", L"C&ikis",
  L"&Duzen", L"&Geri Al\tCtrl+Z", L"&Yinele\tCtrl+Y", L"&Tuvali Temizle",
  L"Re&sim", L"90° Saat Yonunde", L"90° Ters Yonde", L"Yatay Aynala", L"Dikey Aynala", L"Tuval &Boyutlandir...",
  L"&Efektler", L"Gri Tonlama", L"Negatif", L"Sepya", L"Bulaniklastir", L"Keskinlestir",
  L"Piksellestir", L"Parlaklik +", L"Parlaklik -",
  L"Kabartma", L"Kenar Bulma", L"Posterlestir", L"Gren Ekle",
  L"Kontrast +", L"Kontrast -", L"Doygunluk +", L"Doygunluk -",
  L"Gauss Bulanik", L"Yagli Boya", L"Solarizasyon", L"Kanal Degis",
  L"&Gorunum", L"&Izgara", L"&%100 Boyut", L"Pencereye &Sigdir", L"S&imetri",
  L"&Yardim", L"&Hakkinda", L"&Dil",
  L"MiniPaint - Ozel Cizim Programi",
  L"  Arac: %s   Renk: #%02X%02X%02X   Kalinlik: %d   Yaklasim: %d%%   Konum: %d,%d   [Ctrl+S Kaydet, Ctrl+Z Geri Al]",
  L"Hata", L"Dosya acilamadi veya format desteklenmiyor.",
  L"Kaydedilmemis degisiklikler var. Cikmadan once kaydetmek ister misiniz?",
  L"MiniPaint v2.2\n\nTamamen sifirdan, saf C++ ve Win32 API ile\nozgun olarak yazilmis bir cizim programidir.\nHarici kutuphane icermez.\n\n"
  L"36 arac, 32 adim geri alma, 20 efekt, yaklasim, simetri,\nBMP/PNG/JPEG/GIF/TIFF/ICO destegi ve 29 dil icerir.",
  L"Resim Dosyalari (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"Tamam", L"Iptal", L"Genislik:", L"Yukseklik:",
  L"Ozel Renk Sec", L"Tuval Boyutu", L"Dolu", L"Yuvarlak Uc", L"Kare Uc",
  L"Cokgen", L"Bulanik", L"Damga",
  L"&Kullanim Kilavuzu", L"Kullanim Kilavuzu",
  L"MINIPAINT KULLANIM KILAVUZU\r\n\r\nARACLAR\r\nKalem (P): serbest cizim. Sag tus ikincil renkle cizer.\r\n"
  L"Cizgi (L), Kutu (R), Elips (O), Ucgen, Yuvarlak, Yildiz, Elmas, Besgen, Altigen, Ok ve Kalp: surukleyerek cizilir. 'Dolu' dugmesi seklin icini doldurur.\r\n"
  L"Egri: once iki ucu surukleyin, sonra kontrol noktasi icin tiklayin. Esc iptal eder.\r\n"
  L"Cokgen: tiklayarak nokta ekleyin, cift tiklama sekli kapatir.\r\n"
  L"Dolgu (F): alani boyar. Sprey (S): sprey boya. Bulanik: renkleri islak boya gibi surukler.\r\n"
  L"Silgi (E): ikincil renge siler. Renk Al (I): tuvalden renk ornegi alir.\r\n"
  L"Metin (T): tuvale tiklayip yazin; baska yere tiklayinca uygulanir.\r\n"
  L"Secim (M): dikdortgen secin, icinden surukleyerek tasiyin. Delete siler, Esc iptal eder.\r\n"
  L"Damga: gulen yuz basar; boyutu firca kalinligina baglidir.\r\n\r\nRENKLER\r\n"
  L"Paletten sol tik birincil, sag tik ikincil rengi secer.\r\n"
  L"Palete cift tiklama veya sagdaki renk kutulari ozel renk penceresi acar.\r\n\r\nGORUNUM\r\n"
  L"Ctrl + fare tekerlegi: yaklasim. Orta tus surukleme: kaydirma. Ctrl+G: izgara.\r\n"
  L"Simetri: firca vuruslarini dikey eksende aynalar.\r\n\r\nKISAYOLLAR\r\n"
  L"Ctrl+S kaydet, Ctrl+O ac, Ctrl+N yeni, Ctrl+Z geri al, Ctrl+Y yinele, Ctrl+C kopyala, Ctrl+V yapistir.\r\n\r\n"
  L"EFEKTLER\r\nEfektler menusunde gri tonlama, negatif, sepya, bulaniklik, keskinlik, kabartma, yagli boya ve dahasi olmak uzere 20 filtre bulunur.\r\n"
  L"Dosyalar BMP, PNG, JPEG, GIF, TIFF veya ICO olarak kaydedilir.\r\n\r\nEFEKT REFERANSI\r\n"
  L"Gri Tonlama: renkleri parlakliga donusturur. Negatif: tum renkleri tersine cevirir.\r\n"
  L"Sepya: eski fotograf gorunumu verir. Bulaniklastir: 3x3 kutu bulanikligi uygular.\r\n"
  L"Keskinlestir: kenarlari belirginlestirir. Piksellestir: goruntuyu 8x8 bloklara indirger.\r\n"
  L"Parlaklik +/-: goruntuyu acar veya koyultur. Kabartma: yuzey kabartmasi izlenimi verir.\r\n"
  L"Kenar Bulma: kenarlari cikarir. Posterlestir: renkleri 4 kademeye indirir.\r\n"
  L"Gren Ekle: rastgele noktaciklar ekler. Kontrast ve Doygunluk +/-: ton ayarlari.\r\n"
  L"Gauss Bulanik: yumusak 5x5 bulaniklik. Yagli Boya: firca darbesi gorunumu verir.\r\n"
  L"Solarizasyon: acik tonlari tersler. Kanal Degis: kirmizi ve mavi kanali degistirir.\r\n\r\nSSS\r\n"
  L"S: Geri alma kac adim? C: 32 adim.\r\n"
  L"S: Ayarlar nereye kaydediliyor? C: Programin yanindaki minipaint.ini dosyasina.\r\n"
  L"S: Cizimlerimi ticari kullanabilir miyim? C: Evet, cizimleriniz tamamen sizindir.\r\n"
  L"S: Ekran neden titremiyor? C: Tum cizim cift tampon teknigiyle yapilir.\r\n",
  L"Kaynak &Kodu", L"Kaynak Kodu - MiniPaint",
  L"Sulu Boya", L"Islak", L"Yagli", L"Karakalem", L"Kaligrafi", L"Serit",
  L"Neon", L"Golge", L"Referans", L"Yukle...", L"Goster/Gizle", L"Kaldir", L"Firca Sabitleme", L"Dither", L"Mükemmel Piksel", L"Piksel Paleti", L"G-Pen", L"Ton", L"Hız", L"Balon", L"Panel",
  L"Dosya kaydedilemedi. Konum salt okunur olabilir veya disk dolu olabilir." },
// ---- English ----
{ L"Pencil", L"Line", L"Box", L"Ellipse", L"Fill", L"Eraser", L"Pick",
  L"Spray", L"Triangle", L"Rounded", L"Star", L"Text", L"Select",
  L"Curve", L"Diamond", L"Pentagon", L"Hexagon", L"Arrow", L"Heart",
  L"&File", L"&New", L"&Open...", L"&Save", L"Save &As...", L"E&xit",
  L"&Edit", L"&Undo\tCtrl+Z", L"&Redo\tCtrl+Y", L"&Clear Canvas",
  L"&Image", L"Rotate 90° CW", L"Rotate 90° CCW", L"Flip Horizontal", L"Flip Vertical", L"&Resize Canvas...",
  L"E&ffects", L"Grayscale", L"Invert", L"Sepia", L"Blur", L"Sharpen",
  L"Pixelate", L"Brightness +", L"Brightness -",
  L"Emboss", L"Edge Detect", L"Posterize", L"Add Noise",
  L"Contrast +", L"Contrast -", L"Saturation +", L"Saturation -",
  L"Gaussian Blur", L"Oil Paint", L"Solarize", L"Swap Channels",
  L"&View", L"&Grid", L"&Actual Size (100%)", L"&Fit to Window", L"S&ymmetry",
  L"&Help", L"&About", L"&Language",
  L"MiniPaint - Custom Paint Program",
  L"  Tool: %s   Color: #%02X%02X%02X   Size: %d   Zoom: %d%%   Pos: %d,%d   [Ctrl+S Save, Ctrl+Z Undo]",
  L"Error", L"Could not open file, or the format is not supported.",
  L"There are unsaved changes. Do you want to save before exiting?",
  L"MiniPaint v2.2\n\nA paint program written entirely from scratch\nin pure C++ and the Win32 API.\nNo external libraries.\n\n"
  L"Features 36 tools, 32-step undo, 20 effects, zoom, symmetry,\nBMP/PNG/JPEG/GIF/TIFF/ICO support and 29 languages.",
  L"Image Files (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Cancel", L"Width:", L"Height:",
  L"Pick Custom Color", L"Canvas Size", L"Filled", L"Round Tip", L"Square Tip",
  L"Polygon", L"Smudge", L"Stamp",
  L"&User Guide", L"User Guide",
  L"MINIPAINT USER GUIDE\r\n\r\nTOOLS\r\nPencil (P): freehand drawing. Right-click draws with the secondary color.\r\n"
  L"Line (L), Box (R), Ellipse (O), Triangle, Rounded, Star, Diamond, Pentagon, Hexagon, Arrow and Heart: drag to draw. The 'Filled' button fills shapes.\r\n"
  L"Curve: drag the endpoints, then click to place the control point. Esc cancels.\r\n"
  L"Polygon: click to add points, double-click to close the shape.\r\n"
  L"Fill (F): flood-fills an area. Spray (S): airbrush. Smudge: smears colors like wet paint.\r\n"
  L"Eraser (E): erases to the secondary color. Pick (I): samples a color from the canvas.\r\n"
  L"Text (T): click on the canvas and type; click elsewhere to apply.\r\n"
  L"Select (M): drag a rectangle, then drag inside it to move the content. Delete clears it, Esc cancels.\r\n"
  L"Stamp: stamps a smiley; its size follows the brush size.\r\n\r\nCOLORS\r\n"
  L"Left-click a palette color for the primary color, right-click for the secondary.\r\n"
  L"Double-click the palette or click a color box on the right for a custom color.\r\n\r\nVIEW\r\n"
  L"Ctrl + mouse wheel: zoom. Middle mouse drag: pan. Ctrl+G: grid.\r\n"
  L"Symmetry mirrors your strokes across the vertical axis.\r\n\r\nSHORTCUTS\r\n"
  L"Ctrl+S save, Ctrl+O open, Ctrl+N new, Ctrl+Z undo, Ctrl+Y redo, Ctrl+C copy, Ctrl+V paste.\r\n\r\n"
  L"EFFECTS\r\nThe Effects menu offers 20 filters including blur, sharpen, oil paint, emboss and more.\r\n"
  L"Files can be saved as BMP, PNG, JPEG, GIF, TIFF or ICO.\r\n\r\nEFFECT REFERENCE\r\n"
  L"Grayscale: converts colors to luminance. Invert: reverses all colors.\r\n"
  L"Sepia: gives an old-photo look. Blur: applies a 3x3 box blur.\r\n"
  L"Sharpen: emphasizes edges. Pixelate: reduces the image to 8x8 blocks.\r\n"
  L"Brightness +/-: lightens or darkens. Emboss: gives a raised-surface look.\r\n"
  L"Edge Detect: extracts outlines. Posterize: reduces colors to 4 levels.\r\n"
  L"Add Noise: adds random speckles. Contrast and Saturation +/-: tone adjustments.\r\n"
  L"Gaussian Blur: soft 5x5 blur. Oil Paint: gives a brush-stroke look.\r\n"
  L"Solarize: inverts the bright tones. Swap Channels: swaps red and blue.\r\n\r\nFAQ\r\n"
  L"Q: How many undo steps? A: 32 steps.\r\n"
  L"Q: Where are settings stored? A: In minipaint.ini next to the program.\r\n"
  L"Q: Can I use my drawings commercially? A: Yes, your drawings are entirely yours.\r\n"
  L"Q: Why does the canvas not flicker? A: All drawing uses double buffering.\r\n",
  L"&Source Code", L"Source Code - MiniPaint",
  L"W.Color", L"Wet Mix", L"Oil", L"Pencil", L"Callig", L"Ribbon",
  L"Neon", L"Shade", L"Reference", L"Load...", L"Show/Hide", L"Remove", L"Brush Stabilizer", L"Dither", L"Pixel Perfect", L"Pixel Palette", L"G-Pen", L"Tone", L"Speed", L"Bubble", L"Panel",
  L"Could not save the file. The location may be read-only or the disk is full." },
// ---- Deutsch ----
{ L"Stift", L"Linie", L"Rechteck", L"Ellipse", L"Füllen", L"Radierer", L"Pipette",
  L"Spray", L"Dreieck", L"Abgerundet", L"Stern", L"Text", L"Auswahl",
  L"Kurve", L"Raute", L"Fünfeck", L"Sechseck", L"Pfeil", L"Herz",
  L"&Datei", L"&Neu", L"&Öffnen...", L"&Speichern", L"Speichern &unter...", L"&Beenden",
  L"&Bearbeiten", L"&Rückgängig\tStrg+Z", L"&Wiederholen\tStrg+Y", L"&Leinwand löschen",
  L"&Bild", L"90° im Uhrzeigersinn", L"90° gegen Uhrzeigersinn", L"Horizontal spiegeln", L"Vertikal spiegeln", L"Leinwand&größe...",
  L"Eff&ekte", L"Graustufen", L"Invertieren", L"Sepia", L"Weichzeichnen", L"Schärfen",
  L"Verpixeln", L"Helligkeit +", L"Helligkeit -",
  L"Relief", L"Kantenerkennung", L"Posterisieren", L"Rauschen",
  L"Kontrast +", L"Kontrast -", L"Sättigung +", L"Sättigung -",
  L"Gauß-Unschärfe", L"Ölfarbe", L"Solarisation", L"Kanäle tauschen",
  L"&Ansicht", L"&Raster", L"&Originalgröße", L"An Fenster &anpassen", L"S&ymmetrie",
  L"&Hilfe", L"&Info", L"&Sprache",
  L"MiniPaint - Eigenes Malprogramm",
  L"  Werkzeug: %s   Farbe: #%02X%02X%02X   Größe: %d   Zoom: %d%%   Position: %d,%d   [Strg+S Speichern, Strg+Z Rückgängig]",
  L"Fehler", L"Datei konnte nicht geöffnet werden oder Format wird nicht unterstützt.",
  L"Es gibt ungespeicherte Änderungen. Möchten Sie vor dem Beenden speichern?",
  L"MiniPaint v2.2\n\nEin Malprogramm, komplett von Grund auf\nin reinem C++ und der Win32-API geschrieben.\nKeine externen Bibliotheken.\n\n"
  L"36 Werkzeuge, 32-stufiges Rückgängig, 20 Effekte, Zoom, Symmetrie,\nBMP/PNG/JPEG/GIF/TIFF/ICO-Unterstützung und 29 Sprachen.",
  L"Bilddateien (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Abbrechen", L"Breite:", L"Höhe:",
  L"Eigene Farbe wählen", L"Leinwandgröße", L"Gefüllt", L"Runde Spitze", L"Eckige Spitze",
  L"Vieleck", L"Verwischen", L"Stempel",
  L"&Handbuch", L"Handbuch",
  L"MINIPAINT HANDBUCH\r\n\r\nWERKZEUGE\r\nStift (P): Freihandzeichnen. Rechte Taste zeichnet mit der Sekundärfarbe.\r\n"
  L"Linie (L), Rechteck (R), Ellipse (O), Dreieck, Abgerundet, Stern, Raute, Fünfeck, Sechseck, Pfeil und Herz: per Ziehen zeichnen. 'Gefüllt' füllt Formen.\r\n"
  L"Kurve: Endpunkte ziehen, dann Kontrollpunkt klicken. Esc bricht ab.\r\n"
  L"Vieleck: Punkte anklicken, Doppelklick schließt die Form.\r\n"
  L"Füllen (F): füllt eine Fläche. Spray (S): Airbrush. Verwischen: verschmiert Farben wie nasse Farbe.\r\n"
  L"Radierer (E): radiert zur Sekundärfarbe. Pipette (I): nimmt eine Farbe von der Leinwand auf.\r\n"
  L"Text (T): auf die Leinwand klicken und tippen; anderswo klicken zum Übernehmen.\r\n"
  L"Auswahl (M): Rechteck aufziehen, dann Inhalt ziehen. Entf löscht, Esc bricht ab.\r\n"
  L"Stempel: stempelt ein Smiley; die Größe folgt der Pinselstärke.\r\n\r\nFARBEN\r\n"
  L"Linksklick auf die Palette wählt die Primärfarbe, Rechtsklick die Sekundärfarbe.\r\n"
  L"Doppelklick auf die Palette oder Klick auf eine Farbbox rechts öffnet die eigene Farbwahl.\r\n\r\nANSICHT\r\n"
  L"Strg + Mausrad: Zoom. Mittlere Maustaste ziehen: verschieben. Strg+G: Raster.\r\n"
  L"Symmetrie spiegelt Striche an der vertikalen Achse.\r\n\r\nTASTENKÜRZEL\r\n"
  L"Strg+S speichern, Strg+O öffnen, Strg+N neu, Strg+Z rückgängig, Strg+Y wiederholen, Strg+C kopieren, Strg+V einfügen.\r\n\r\n"
  L"EFFEKTE\r\nDas Effekte-Menü bietet 20 Filter, darunter Weichzeichnen, Schärfen, Ölfarbe, Relief und mehr.\r\n"
  L"Dateien können als BMP, PNG, JPEG, GIF, TIFF oder ICO gespeichert werden.\r\n\r\nEFFEKT-REFERENZ\r\n"
  L"Graustufen: wandelt Farben in Helligkeit um. Invertieren: kehrt alle Farben um.\r\n"
  L"Sepia: verleiht einen Alt-Foto-Look. Weichzeichnen: 3x3-Box-Weichzeichner.\r\n"
  L"Schärfen: betont Kanten. Verpixeln: reduziert das Bild auf 8x8-Blöcke.\r\n"
  L"Helligkeit +/-: hellt auf oder dunkelt ab. Relief: erzeugt einen 3D-Oberflächeneffekt.\r\n"
  L"Kantenerkennung: hebt Umrisse hervor. Posterisieren: reduziert Farben auf 4 Stufen.\r\n"
  L"Rauschen: fügt zufällige Punkte hinzu. Kontrast und Sättigung +/-: Tonanpassungen.\r\n"
  L"Gauß-Unschärfe: weiche 5x5-Unschärfe. Ölfarbe: erzeugt einen Pinselstrich-Look.\r\n"
  L"Solarisation: kehrt helle Töne um. Kanäle tauschen: vertauscht Rot und Blau.\r\n\r\nFAQ\r\n"
  L"F: Wie viele Rückgängig-Schritte? A: 32 Schritte.\r\n"
  L"F: Wo werden Einstellungen gespeichert? A: In minipaint.ini neben dem Programm.\r\n"
  L"F: Darf ich meine Zeichnungen kommerziell nutzen? A: Ja, sie gehören ganz Ihnen.\r\n"
  L"F: Warum flackert die Leinwand nicht? A: Alle Zeichnungen nutzen Double-Buffering.\r\n",
  L"&Quellcode", L"Quellcode - MiniPaint",
  L"Aquarell", L"Nass", L"Öl", L"Skizze", L"Kalligr", L"Band",
  L"Neon", L"Schatten", L"Referenz", L"Laden...", L"Ein-/Ausblenden", L"Entfernen", L"Stabilisierung", L"Dither", L"Pixelperfekt", L"Pixel-Palette", L"G-Pen", L"Raster", L"Tempo", L"Blase", L"Panel",
  L"Die Datei konnte nicht gespeichert werden. Der Speicherort ist möglicherweise schreibgeschützt oder der Datenträger voll." },
// ---- Français ----
{ L"Crayon", L"Ligne", L"Rectangle", L"Ellipse", L"Remplir", L"Gomme", L"Pipette",
  L"Spray", L"Triangle", L"Arrondi", L"Étoile", L"Texte", L"Sélect.",
  L"Courbe", L"Losange", L"Pentagone", L"Hexagone", L"Flèche", L"Cœur",
  L"&Fichier", L"&Nouveau", L"&Ouvrir...", L"&Enregistrer", L"Enregistrer &sous...", L"&Quitter",
  L"&Édition", L"&Annuler\tCtrl+Z", L"&Rétablir\tCtrl+Y", L"&Effacer le canevas",
  L"&Image", L"Pivoter 90° horaire", L"Pivoter 90° antihoraire", L"Retourner horiz.", L"Retourner vert.", L"&Redimensionner...",
  L"Eff&ets", L"Niveaux de gris", L"Négatif", L"Sépia", L"Flou", L"Netteté",
  L"Pixéliser", L"Luminosité +", L"Luminosité -",
  L"Relief", L"Contours", L"Posteriser", L"Bruit",
  L"Contraste +", L"Contraste -", L"Saturation +", L"Saturation -",
  L"Flou gaussien", L"Peinture à l'huile", L"Solarisation", L"Échanger canaux",
  L"&Affichage", L"&Grille", L"Taille &réelle", L"Ajuster à la &fenêtre", L"S&ymétrie",
  L"&Aide", L"À &propos", L"&Langue",
  L"MiniPaint - Programme de dessin personnalisé",
  L"  Outil : %s   Couleur : #%02X%02X%02X   Taille : %d   Zoom : %d%%   Position : %d,%d   [Ctrl+S Enregistrer, Ctrl+Z Annuler]",
  L"Erreur", L"Impossible d'ouvrir le fichier ou format non pris en charge.",
  L"Des modifications non enregistrées existent. Voulez-vous enregistrer avant de quitter ?",
  L"MiniPaint v2.2\n\nUn programme de dessin écrit entièrement à partir de zéro\nen C++ pur et API Win32.\nAucune bibliothèque externe.\n\n"
  L"36 outils, annulation 32 pas, 20 effets, zoom, symétrie,\nprise en charge BMP/PNG/JPEG/GIF/TIFF/ICO et 29 langues.",
  L"Fichiers image (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Annuler", L"Largeur :", L"Hauteur :",
  L"Choisir une couleur", L"Taille du canevas", L"Rempli", L"Pointe ronde", L"Pointe carrée",
  L"Polygone", L"Étaler", L"Tampon",
  L"&Guide", L"Guide d'utilisation",
  L"GUIDE MINIPAINT\r\n\r\nOUTILS\r\nCrayon (P) : dessin à main levée. Le clic droit dessine avec la couleur secondaire.\r\n"
  L"Ligne (L), Rectangle (R), Ellipse (O), Triangle, Arrondi, Étoile, Losange, Pentagone, Hexagone, Flèche et Cœur : dessiner par glisser. 'Rempli' remplit les formes.\r\n"
  L"Courbe : glisser les extrémités, puis cliquer le point de contrôle. Échap annule.\r\n"
  L"Polygone : cliquer pour ajouter des points, double-clic pour fermer.\r\n"
  L"Remplir (F) : remplit une zone. Spray (S) : aérographe. Étaler : étale les couleurs comme de la peinture fraîche.\r\n"
  L"Gomme (E) : gomme vers la couleur secondaire. Pipette (I) : échantillonne une couleur.\r\n"
  L"Texte (T) : cliquer sur le canevas et taper ; cliquer ailleurs pour appliquer.\r\n"
  L"Sélect. (M) : glisser un rectangle, puis déplacer le contenu. Suppr efface, Échap annule.\r\n"
  L"Tampon : tamponne un smiley ; la taille suit l'épaisseur du pinceau.\r\n\r\nCOULEURS\r\n"
  L"Clic gauche sur la palette pour la couleur principale, clic droit pour la secondaire.\r\n"
  L"Double-clic sur la palette ou clic sur une boîte de couleur à droite pour une couleur personnalisée.\r\n\r\nAFFICHAGE\r\n"
  L"Ctrl + molette : zoom. Glisser avec le bouton du milieu : déplacer. Ctrl+G : grille.\r\n"
  L"Symétrie : reflète les traits sur l'axe vertical.\r\n\r\nRACCOURCIS\r\n"
  L"Ctrl+S enregistrer, Ctrl+O ouvrir, Ctrl+N nouveau, Ctrl+Z annuler, Ctrl+Y rétablir, Ctrl+C copier, Ctrl+V coller.\r\n\r\n"
  L"EFFETS\r\nLe menu Effets propose 20 filtres : flou, netteté, peinture à l'huile, relief et plus.\r\n"
  L"Les fichiers peuvent être enregistrés en BMP, PNG, JPEG, GIF, TIFF ou ICO.\r\n\r\nRÉFÉRENCE DES EFFETS\r\n"
  L"Niveaux de gris : convertit les couleurs en luminance. Négatif : inverse toutes les couleurs.\r\n"
  L"Sépia : donne un aspect photo ancienne. Flou : applique un flou 3x3.\r\n"
  L"Netteté : accentue les contours. Pixéliser : réduit l'image en blocs 8x8.\r\n"
  L"Luminosité +/- : éclaircit ou assombrit. Relief : donne un effet de surface en relief.\r\n"
  L"Contours : extrait les bords. Posteriser : réduit les couleurs à 4 niveaux.\r\n"
  L"Bruit : ajoute des points aléatoires. Contraste et Saturation +/- : ajustements de ton.\r\n"
  L"Flou gaussien : flou doux 5x5. Peinture à l'huile : donne un aspect coup de pinceau.\r\n"
  L"Solarisation : inverse les tons clairs. Échanger canaux : échange rouge et bleu.\r\n\r\nFAQ\r\n"
  L"Q : Combien de pas d'annulation ? R : 32 pas.\r\n"
  L"Q : Où sont stockés les réglages ? R : Dans minipaint.ini à côté du programme.\r\n"
  L"Q : Puis-je utiliser mes dessins commercialement ? R : Oui, ils vous appartiennent.\r\n"
  L"Q : Pourquoi le canevas ne scintille pas ? R : Tout le dessin utilise le double tampon.\r\n",
  L"&Code source", L"Code source - MiniPaint",
  L"Aquarelle", L"Mouillé", L"Huile", L"Crayon", L"Calligr", L"Ruban",
  L"Néon", L"Ombre", L"Référence", L"Charger...", L"Afficher/Masquer", L"Supprimer", L"Stabilisateur", L"Tramage", L"Pixel parfait", L"Palette pixel", L"G-Pen", L"Trame", L"Vitesse", L"Bulle", L"Case",
  L"Impossible d'enregistrer le fichier. L'emplacement est peut-être en lecture seule ou le disque est plein." },
// ---- Español ----
{ L"Lápiz", L"Línea", L"Rectángulo", L"Elipse", L"Relleno", L"Borrador", L"Cuentagotas",
  L"Spray", L"Triángulo", L"Redondeado", L"Estrella", L"Texto", L"Selección",
  L"Curva", L"Rombo", L"Pentágono", L"Hexágono", L"Flecha", L"Corazón",
  L"&Archivo", L"&Nuevo", L"&Abrir...", L"&Guardar", L"Guardar &como...", L"&Salir",
  L"&Editar", L"&Deshacer\tCtrl+Z", L"&Rehacer\tCtrl+Y", L"&Limpiar lienzo",
  L"I&magen", L"Girar 90° horario", L"Girar 90° antihorario", L"Voltear horizontal", L"Voltear vertical", L"Cambiar &tamaño...",
  L"E&fectos", L"Escala de grises", L"Negativo", L"Sepia", L"Desenfocar", L"Enfocar",
  L"Pixelar", L"Brillo +", L"Brillo -",
  L"Relieve", L"Detectar bordes", L"Posterizar", L"Añadir ruido",
  L"Contraste +", L"Contraste -", L"Saturación +", L"Saturación -",
  L"Desenfoque gaussiano", L"Pintura al óleo", L"Solarización", L"Intercambiar canales",
  L"&Vista", L"Cuad&rícula", L"Tamaño &real (100%)", L"A&justar a ventana", L"S&imetría",
  L"A&yuda", L"&Acerca de", L"&Idioma",
  L"MiniPaint - Programa de dibujo personalizado",
  L"  Herramienta: %s   Color: #%02X%02X%02X   Grosor: %d   Zoom: %d%%   Posición: %d,%d   [Ctrl+S Guardar, Ctrl+Z Deshacer]",
  L"Error", L"No se pudo abrir el archivo o el formato no es compatible.",
  L"Hay cambios sin guardar. ¿Desea guardar antes de salir?",
  L"MiniPaint v2.2\n\nUn programa de dibujo escrito completamente desde cero\nen C++ puro y API Win32.\nSin bibliotecas externas.\n\n"
  L"36 herramientas, deshacer de 32 pasos, 20 efectos, zoom, simetría,\ncompatible con BMP/PNG/JPEG/GIF/TIFF/ICO y 29 idiomas.",
  L"Archivos de imagen (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"Aceptar", L"Cancelar", L"Anchura:", L"Altura:",
  L"Elegir color personalizado", L"Tamaño del lienzo", L"Relleno", L"Punta redonda", L"Punta cuadrada",
  L"Polígono", L"Difuminar", L"Sello",
  L"&Guía", L"Guía de uso",
  L"GUÍA DE MINIPAINT\r\n\r\nHERRAMIENTAS\r\nLápiz (P): dibujo a mano alzada. El botón derecho dibuja con el color secundario.\r\n"
  L"Línea (L), Rectángulo (R), Elipse (O), Triángulo, Redondeado, Estrella, Rombo, Pentágono, Hexágono, Flecha y Corazón: arrastre para dibujar. 'Relleno' rellena las formas.\r\n"
  L"Curva: arrastre los extremos y luego haga clic para el punto de control. Esc cancela.\r\n"
  L"Polígono: haga clic para añadir puntos, doble clic para cerrar.\r\n"
  L"Relleno (F): rellena un área. Spray (S): aerógrafo. Difuminar: arrastra colores como pintura fresca.\r\n"
  L"Borrador (E): borra al color secundario. Cuentagotas (I): toma un color del lienzo.\r\n"
  L"Texto (T): haga clic en el lienzo y escriba; clic en otro lugar para aplicar.\r\n"
  L"Selección (M): arrastre un rectángulo y mueva el contenido. Supr borra, Esc cancela.\r\n"
  L"Sello: estampa una carita; el tamaño sigue al grosor del pincel.\r\n\r\nCOLORES\r\n"
  L"Clic izquierdo en la paleta para el color principal, clic derecho para el secundario.\r\n"
  L"Doble clic en la paleta o clic en un cuadro de color a la derecha para un color personalizado.\r\n\r\nVISTA\r\n"
  L"Ctrl + rueda: zoom. Arrastrar con botón central: desplazar. Ctrl+G: cuadrícula.\r\n"
  L"Simetría: refleja los trazos en el eje vertical.\r\n\r\nATAJOS\r\n"
  L"Ctrl+S guardar, Ctrl+O abrir, Ctrl+N nuevo, Ctrl+Z deshacer, Ctrl+Y rehacer, Ctrl+C copiar, Ctrl+V pegar.\r\n\r\n"
  L"EFECTOS\r\nEl menú Efectos ofrece 20 filtros: desenfoque, enfoque, pintura al óleo, relieve y más.\r\n"
  L"Los archivos se pueden guardar como BMP, PNG, JPEG, GIF, TIFF o ICO.\r\n\r\nREFERENCIA DE EFECTOS\r\n"
  L"Escala de grises: convierte colores a luminancia. Negativo: invierte todos los colores.\r\n"
  L"Sepia: da un aspecto de foto antigua. Desenfocar: aplica un desenfoque 3x3.\r\n"
  L"Enfocar: acentúa los bordes. Pixelar: reduce la imagen a bloques de 8x8.\r\n"
  L"Brillo +/-: aclara u oscurece. Relieve: da un aspecto de superficie elevada.\r\n"
  L"Detectar bordes: extrae los contornos. Posterizar: reduce los colores a 4 niveles.\r\n"
  L"Añadir ruido: añade puntos aleatorios. Contraste y Saturación +/-: ajustes de tono.\r\n"
  L"Desenfoque gaussiano: desenfoque suave 5x5. Pintura al óleo: da aspecto de pincelada.\r\n"
  L"Solarización: invierte los tonos claros. Intercambiar canales: cambia rojo y azul.\r\n\r\nPREGUNTAS\r\n"
  L"P: ¿Cuántos pasos de deshacer? R: 32 pasos.\r\n"
  L"P: ¿Dónde se guardan los ajustes? R: En minipaint.ini junto al programa.\r\n"
  L"P: ¿Puedo usar mis dibujos comercialmente? R: Sí, sus dibujos son totalmente suyos.\r\n"
  L"P: ¿Por qué el lienzo no parpadea? R: Todo el dibujo usa doble búfer.\r\n",
  L"&Código fuente", L"Código fuente - MiniPaint",
  L"Acuarela", L"Húmedo", L"Óleo", L"Lápiz", L"Caligr", L"Cinta",
  L"Neón", L"Sombra", L"Referencia", L"Cargar...", L"Mostrar/Ocultar", L"Quitar", L"Estabilizador", L"Tramado", L"Píxel perfecto", L"Paleta pixel", L"G-Pen", L"Trama", L"Veloz", L"Globo", L"Viñeta",
  L"No se pudo guardar el archivo. Es posible que la ubicación sea de solo lectura o que el disco esté lleno." },
// ---- Italiano ----
{ L"Matita", L"Linea", L"Rettangolo", L"Ellisse", L"Riempi", L"Gomma", L"Preleva",
  L"Spray", L"Triangolo", L"Arrotondato", L"Stella", L"Testo", L"Seleziona",
  L"Curva", L"Rombo", L"Pentagono", L"Esagono", L"Freccia", L"Cuore",
  L"&File", L"&Nuovo", L"&Apri...", L"&Salva", L"Salva con &nome...", L"E&sci",
  L"&Modifica", L"&Annulla\tCtrl+Z", L"&Ripeti\tCtrl+Y", L"&Pulisci area",
  L"&Immagine", L"Ruota 90° orario", L"Ruota 90° antiorario", L"Capovolgi orizz.", L"Capovolgi vert.", L"&Ridimensiona area...",
  L"Eff&etti", L"Scala di grigi", L"Inverti", L"Seppia", L"Sfoca", L"Nitidezza",
  L"Pixela", L"Luminosità +", L"Luminosità -",
  L"Rilievo", L"Rileva bordi", L"Posterizza", L"Rumore",
  L"Contrasto +", L"Contrasto -", L"Saturazione +", L"Saturazione -",
  L"Sfocatura gaussiana", L"Pittura a olio", L"Solarizzazione", L"Scambia canali",
  L"&Visualizza", L"&Griglia", L"&Dimensioni reali", L"A&datta alla finestra", L"Simmetria",
  L"&Aiuto", L"&Informazioni", L"&Lingua",
  L"MiniPaint - Programma di disegno personalizzato",
  L"  Strumento: %s   Colore: #%02X%02X%02X   Dim.: %d   Zoom: %d%%   Pos.: %d,%d   [Ctrl+S Salva, Ctrl+Z Annulla]",
  L"Errore", L"Impossibile aprire il file o formato non supportato.",
  L"Ci sono modifiche non salvate. Salvare prima di uscire?",
  L"MiniPaint v2.2\n\nUn programma di disegno scritto interamente da zero\nin C++ puro e API Win32.\nNessuna libreria esterna.\n\n"
  L"36 strumenti, annullamento a 32 passi, 20 effetti, zoom, simmetria,\nsupporto BMP/PNG/JPEG/GIF/TIFF/ICO e 29 lingue.",
  L"File immagine (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Annulla", L"Larghezza:", L"Altezza:",
  L"Scegli colore personalizzato", L"Dimensioni area", L"Pieno", L"Punta tonda", L"Punta quadrata",
  L"Poligono", L"Sfuma", L"Timbro",
  L"&Guida", L"Guida utente",
  L"GUIDA DI MINIPAINT\r\n\r\nSTRUMENTI\r\nMatita (P): disegno a mano libera. Il tasto destro disegna con il colore secondario.\r\n"
  L"Linea (L), Rettangolo (R), Ellisse (O), Triangolo, Arrotondato, Stella, Rombo, Pentagono, Esagono, Freccia e Cuore: trascina per disegnare. 'Pieno' riempie le forme.\r\n"
  L"Curva: trascina gli estremi, poi clicca il punto di controllo. Esc annulla.\r\n"
  L"Poligono: clicca per aggiungere punti, doppio clic per chiudere.\r\n"
  L"Riempi (F): riempie un'area. Spray (S): aerografo. Sfuma: trascina i colori come pittura fresca.\r\n"
  L"Gomma (E): cancella verso il colore secondario. Preleva (I): campiona un colore dall'area.\r\n"
  L"Testo (T): clicca sull'area e scrivi; clicca altrove per applicare.\r\n"
  L"Seleziona (M): trascina un rettangolo e sposta il contenuto. Canc elimina, Esc annulla.\r\n"
  L"Timbro: stampa una faccina; la dimensione segue lo spessore del pennello.\r\n\r\nCOLORI\r\n"
  L"Clic sinistro sulla tavolozza per il colore primario, clic destro per il secondario.\r\n"
  L"Doppio clic sulla tavolozza o clic su una casella colore a destra per un colore personalizzato.\r\n\r\nVISTA\r\n"
  L"Ctrl + rotellina: zoom. Trascina col tasto centrale: sposta la vista. Ctrl+G: griglia.\r\n"
  L"Simmetria: riflette i tratti sull'asse verticale.\r\n\r\nSCORCIATOIE\r\n"
  L"Ctrl+S salva, Ctrl+O apri, Ctrl+N nuovo, Ctrl+Z annulla, Ctrl+Y ripeti, Ctrl+C copia, Ctrl+V incolla.\r\n\r\n"
  L"EFFETTI\r\nIl menu Effetti offre 20 filtri: sfocatura, nitidezza, pittura a olio, rilievo e altro.\r\n"
  L"I file possono essere salvati come BMP, PNG, JPEG, GIF, TIFF o ICO.\r\n\r\nRIFERIMENTO EFFETTI\r\n"
  L"Scala di grigi: converte i colori in luminanza. Inverti: inverte tutti i colori.\r\n"
  L"Seppia: dona un aspetto da foto antica. Sfoca: applica una sfocatura 3x3.\r\n"
  L"Nitidezza: enfatizza i bordi. Pixela: riduce l'immagine a blocchi 8x8.\r\n"
  L"Luminosità +/-: schiarisce o scurisce. Rilievo: dà un aspetto di superficie in rilievo.\r\n"
  L"Rileva bordi: estrae i contorni. Posterizza: riduce i colori a 4 livelli.\r\n"
  L"Rumore: aggiunge punti casuali. Contrasto e Saturazione +/-: regolazioni di tono.\r\n"
  L"Sfocatura gaussiana: sfocatura morbida 5x5. Pittura a olio: dà un aspetto a pennellate.\r\n"
  L"Solarizzazione: inverte i toni chiari. Scambia canali: scambia rosso e blu.\r\n\r\nFAQ\r\n"
  L"D: Quanti passi di annullamento? R: 32 passi.\r\n"
  L"D: Dove sono salvate le impostazioni? R: In minipaint.ini accanto al programma.\r\n"
  L"D: Posso usare i miei disegni commercialmente? R: Sì, sono interamente vostri.\r\n"
  L"D: Perché l'area non sfarfalla? R: Tutto il disegno usa il doppio buffer.\r\n",
  L"&Codice sorgente", L"Codice sorgente - MiniPaint",
  L"Acquerello", L"Bagnato", L"Olio", L"Matita", L"Calligr", L"Nastro",
  L"Neon", L"Ombra", L"Riferimento", L"Carica...", L"Mostra/Nascondi", L"Rimuovi", L"Stabilizzatore", L"Retino", L"Pixel perfetto", L"Tavolozza pixel", L"G-Pen", L"Retino", L"Veloce", L"Nuvola", L"Vignetta",
  L"Impossibile salvare il file. Il percorso potrebbe essere di sola lettura o il disco pieno." },
// ---- Português ----
{ L"Lápis", L"Linha", L"Retângulo", L"Elipse", L"Preencher", L"Borracha", L"Conta-gotas",
  L"Spray", L"Triângulo", L"Arredondado", L"Estrela", L"Texto", L"Seleção",
  L"Curva", L"Losango", L"Pentágono", L"Hexágono", L"Seta", L"Coração",
  L"&Ficheiro", L"&Novo", L"&Abrir...", L"&Guardar", L"Guardar &como...", L"&Sair",
  L"&Editar", L"&Desfazer\tCtrl+Z", L"&Refazer\tCtrl+Y", L"&Limpar tela",
  L"&Imagem", L"Rodar 90° horário", L"Rodar 90° anti-horário", L"Inverter horizontal", L"Inverter vertical", L"&Redimensionar tela...",
  L"E&feitos", L"Escala de cinzentos", L"Inverter", L"Sépia", L"Desfocar", L"Nitidez",
  L"Pixelizar", L"Brilho +", L"Brilho -",
  L"Relevo", L"Detetar contornos", L"Posterizar", L"Ruído",
  L"Contraste +", L"Contraste -", L"Saturação +", L"Saturação -",
  L"Desfoque gaussiano", L"Pintura a óleo", L"Solarização", L"Trocar canais",
  L"&Ver", L"&Grelha", L"Tamanho &real", L"A&justar à janela", L"Simetria",
  L"A&juda", L"&Sobre", L"&Idioma",
  L"MiniPaint - Programa de pintura personalizado",
  L"  Ferramenta: %s   Cor: #%02X%02X%02X   Tamanho: %d   Zoom: %d%%   Pos.: %d,%d   [Ctrl+S Guardar, Ctrl+Z Desfazer]",
  L"Erro", L"Não foi possível abrir o ficheiro ou formato não suportado.",
  L"Existem alterações não guardadas. Deseja guardar antes de sair?",
  L"MiniPaint v2.2\n\nUm programa de pintura escrito inteiramente do zero\nem C++ puro e API Win32.\nSem bibliotecas externas.\n\n"
  L"36 ferramentas, desfazer de 32 passos, 20 efeitos, zoom, simetria,\nsuporte BMP/PNG/JPEG/GIF/TIFF/ICO e 29 idiomas.",
  L"Ficheiros de imagem (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Cancelar", L"Largura:", L"Altura:",
  L"Escolher cor personalizada", L"Tamanho da tela", L"Preenchido", L"Ponta redonda", L"Ponta quadrada",
  L"Polígono", L"Borrar", L"Carimbo",
  L"&Guia", L"Guia do utilizador",
  L"GUIA DO MINIPAINT\r\n\r\nFERRAMENTAS\r\nLápis (P): desenho à mão livre. O botão direito desenha com a cor secundária.\r\n"
  L"Linha (L), Retângulo (R), Elipse (O), Triângulo, Arredondado, Estrela, Losango, Pentágono, Hexágono, Seta e Coração: arraste para desenhar. 'Preenchido' preenche as formas.\r\n"
  L"Curva: arraste as extremidades e clique para o ponto de controlo. Esc cancela.\r\n"
  L"Polígono: clique para adicionar pontos, duplo clique para fechar.\r\n"
  L"Preencher (F): preenche uma área. Spray (S): aerógrafo. Borrar: arrasta cores como tinta fresca.\r\n"
  L"Borracha (E): apaga para a cor secundária. Conta-gotas (I): recolhe uma cor da tela.\r\n"
  L"Texto (T): clique na tela e escreva; clique noutro lugar para aplicar.\r\n"
  L"Seleção (M): arraste um retângulo e mova o conteúdo. Delete apaga, Esc cancela.\r\n"
  L"Carimbo: carimba um sorriso; o tamanho segue a espessura do pincel.\r\n\r\nCORES\r\n"
  L"Clique esquerdo na paleta para a cor primária, clique direito para a secundária.\r\n"
  L"Duplo clique na paleta ou clique numa caixa de cor à direita para cor personalizada.\r\n\r\nVER\r\n"
  L"Ctrl + roda do rato: zoom. Arrastar com o botão do meio: deslocar. Ctrl+G: grelha.\r\n"
  L"Simetria: espelha os traços no eixo vertical.\r\n\r\nATALHOS\r\n"
  L"Ctrl+S guardar, Ctrl+O abrir, Ctrl+N novo, Ctrl+Z desfazer, Ctrl+Y refazer, Ctrl+C copiar, Ctrl+V colar.\r\n\r\n"
  L"EFEITOS\r\nO menu Efeitos oferece 20 filtros: desfoque, nitidez, pintura a óleo, relevo e mais.\r\n"
  L"Os ficheiros podem ser guardados como BMP, PNG, JPEG, GIF, TIFF ou ICO.\r\n\r\nREFERÊNCIA DE EFEITOS\r\n"
  L"Escala de cinzentos: converte cores em luminância. Inverter: inverte todas as cores.\r\n"
  L"Sépia: dá um aspeto de foto antiga. Desfocar: aplica um desfoque 3x3.\r\n"
  L"Nitidez: realça os contornos. Pixelizar: reduz a imagem a blocos 8x8.\r\n"
  L"Brilho +/-: clareia ou escurece. Relevo: dá um aspeto de superfície em relevo.\r\n"
  L"Detetar contornos: extrai as bordas. Posterizar: reduz as cores a 4 níveis.\r\n"
  L"Ruído: adiciona pontos aleatórios. Contraste e Saturação +/-: ajustes de tom.\r\n"
  L"Desfoque gaussiano: desfoque suave 5x5. Pintura a óleo: dá aspeto de pinceladas.\r\n"
  L"Solarização: inverte os tons claros. Trocar canais: troca vermelho e azul.\r\n\r\nPERGUNTAS\r\n"
  L"P: Quantos passos de desfazer? R: 32 passos.\r\n"
  L"P: Onde ficam as definições? R: Em minipaint.ini junto ao programa.\r\n"
  L"P: Posso usar os meus desenhos comercialmente? R: Sim, são inteiramente seus.\r\n"
  L"P: Porque é que a tela não pisca? R: Todo o desenho usa duplo buffer.\r\n",
  L"Código &fonte", L"Código fonte - MiniPaint",
  L"Aquarela", L"Molhado", L"Óleo", L"Lápis", L"Caligr", L"Fita",
  L"Neon", L"Sombra", L"Referência", L"Carregar...", L"Mostrar/Ocultar", L"Remover", L"Estabilizador", L"Pontilhado", L"Pixel perfeito", L"Paleta pixel", L"G-Pen", L"Retícula", L"Veloz", L"Balão", L"Painel",
  L"Não foi possível guardar o ficheiro. A localização pode ser só de leitura ou o disco está cheio." },
// ---- Russki ----
{ L"Карандаш", L"Линия", L"Прямоуг.", L"Эллипс", L"Заливка", L"Ластик", L"Пипетка",
  L"Спрей", L"Треугольник", L"Скруглён.", L"Звезда", L"Текст", L"Выбор",
  L"Кривая", L"Ромб", L"Пятиуг.", L"Шестиуг.", L"Стрелка", L"Сердце",
  L"&Файл", L"&Новый", L"&Открыть...", L"&Сохранить", L"Сохранить &как...", L"В&ыход",
  L"&Правка", L"&Отменить\tCtrl+Z", L"&Повторить\tCtrl+Y", L"&Очистить холст",
  L"&Изображение", L"Повернуть 90° вправо", L"Повернуть 90° влево", L"Отразить гориз.", L"Отразить верт.", L"&Размер холста...",
  L"Э&ффекты", L"Оттенки серого", L"Негатив", L"Сепия", L"Размытие", L"Резкость",
  L"Пикселизация", L"Яркость +", L"Яркость -",
  L"Рельеф", L"Контуры", L"Постеризация", L"Шум",
  L"Контраст +", L"Контраст -", L"Насыщенность +", L"Насыщенность -",
  L"Размытие Гаусса", L"Масляная краска", L"Соляризация", L"Поменять каналы",
  L"&Вид", L"&Сетка", L"&Реальный размер", L"&По размеру окна", L"Симметрия",
  L"&Справка", L"&О программе", L"&Язык",
  L"MiniPaint - Программа для рисования",
  L"  Инструмент: %s   Цвет: #%02X%02X%02X   Размер: %d   Масштаб: %d%%   Позиция: %d,%d   [Ctrl+S Сохранить, Ctrl+Z Отменить]",
  L"Ошибка", L"Не удалось открыть файл или формат не поддерживается.",
  L"Есть несохранённые изменения. Сохранить перед выходом?",
  L"MiniPaint v2.2\n\nПрограмма для рисования, полностью написанная с нуля\nна чистом C++ и Win32 API.\nБез внешних библиотек.\n\n"
  L"36 инструмента, отмена на 32 шага, 20 эффектов, масштаб, симметрия,\nподдержка BMP/PNG/JPEG/GIF/TIFF/ICO и 29 языков.",
  L"Файлы изображений (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"ОК", L"Отмена", L"Ширина:", L"Высота:",
  L"Выбор цвета", L"Размер холста", L"Залито", L"Круглая кисть", L"Квадратная кисть",
  L"Многоуг.", L"Размазать", L"Штамп",
  L"&Руководство", L"Руководство пользователя",
  L"РУКОВОДСТВО MINIPAINT\r\n\r\nИНСТРУМЕНТЫ\r\nКарандаш (P): рисование от руки. Правая кнопка рисует дополнительным цветом.\r\n"
  L"Линия (L), Прямоугольник (R), Эллипс (O), Треугольник, Скруглённый, Звезда, Ромб, Пятиугольник, Шестиугольник, Стрелка и Сердце: рисуются перетаскиванием. 'Залито' заполняет фигуры.\r\n"
  L"Кривая: перетащите концы, затем щёлкните контрольную точку. Esc отменяет.\r\n"
  L"Многоугольник: щелчками добавляйте точки, двойной щелчок замыкает фигуру.\r\n"
  L"Заливка (F): заливает область. Спрей (S): аэрограф. Размазать: размазывает цвета как свежую краску.\r\n"
  L"Ластик (E): стирает до дополнительного цвета. Пипетка (I): берёт цвет с холста.\r\n"
  L"Текст (T): щёлкните холст и печатайте; щелчок в другом месте применяет текст.\r\n"
  L"Выбор (M): растяните прямоугольник и перетащите содержимое. Delete удаляет, Esc отменяет.\r\n"
  L"Штамп: ставит смайлик; размер зависит от толщины кисти.\r\n\r\nЦВЕТА\r\n"
  L"Левый щелчок по палитре выбирает основной цвет, правый — дополнительный.\r\n"
  L"Двойной щелчок по палитре или щелчок по цветному полю справа открывает выбор цвета.\r\n\r\nВИД\r\n"
  L"Ctrl + колесо мыши: масштаб. Перетаскивание средней кнопкой: сдвиг. Ctrl+G: сетка.\r\n"
  L"Симметрия: отражает штрихи по вертикальной оси.\r\n\r\nГОРЯЧИЕ КЛАВИШИ\r\n"
  L"Ctrl+S сохранить, Ctrl+O открыть, Ctrl+N новый, Ctrl+Z отменить, Ctrl+Y повторить, Ctrl+C копировать, Ctrl+V вставить.\r\n\r\n"
  L"ЭФФЕКТЫ\r\nМеню эффектов предлагает 20 фильтров: размытие, резкость, масляная краска, рельеф и другие.\r\n"
  L"Файлы сохраняются как BMP, PNG, JPEG, GIF, TIFF или ICO.\r\n\r\nСПРАВКА ПО ЭФФЕКТАМ\r\n"
  L"Оттенки серого: переводит цвета в яркость. Негатив: инвертирует все цвета.\r\n"
  L"Сепия: придаёт вид старой фотографии. Размытие: применяет размытие 3x3.\r\n"
  L"Резкость: подчёркивает края. Пикселизация: уменьшает изображение до блоков 8x8.\r\n"
  L"Яркость +/-: осветляет или затемняет. Рельеф: придаёт эффект выпуклой поверхности.\r\n"
  L"Контуры: выделяет границы. Постеризация: сокращает цвета до 4 уровней.\r\n"
  L"Шум: добавляет случайные точки. Контраст и Насыщенность +/-: тональная настройка.\r\n"
  L"Размытие Гаусса: мягкое размытие 5x5. Масляная краска: придаёт вид мазков кисти.\r\n"
  L"Соляризация: инвертирует светлые тона. Поменять каналы: меняет красный и синий.\r\n\r\nЧАВО\r\n"
  L"В: Сколько шагов отмены? О: 32 шага.\r\n"
  L"В: Где хранятся настройки? О: В файле minipaint.ini рядом с программой.\r\n"
  L"В: Можно ли использовать рисунки коммерчески? О: Да, они полностью ваши.\r\n"
  L"В: Почему холст не мерцает? О: Вся отрисовка использует двойную буферизацию.\r\n",
  L"&Исходный код", L"Исходный код - MiniPaint",
  L"Акварель", L"Мокрый", L"Масло", L"Карандаш", L"Каллигр", L"Лента",
  L"Неон", L"Тень", L"Референс", L"Загрузить...", L"Показать/скрыть", L"Убрать", L"Стабилизатор", L"Дизеринг", L"Идеальный пиксель", L"Пиксельная палитра", L"G-перо", L"Тон", L"Скорость", L"Пузырь", L"Панель",
  L"Не удалось сохранить файл. Возможно, папка доступна только для чтения или диск заполнен." },
// ---- Japonca ----
{ L"鉛筆", L"直線", L"四角", L"楕円", L"塗りつぶし", L"消しゴム", L"スポイト",
  L"スプレー", L"三角形", L"角丸", L"星", L"テキスト", L"選択",
  L"曲線", L"ひし形", L"五角形", L"六角形", L"矢印", L"ハート",
  L"ファイル(&F)", L"新規(&N)", L"開く(&O)...", L"保存(&S)", L"名前を付けて保存(&A)...", L"終了(&X)",
  L"編集(&E)", L"元に戻す(&U)\tCtrl+Z", L"やり直し(&R)\tCtrl+Y", L"キャンバスをクリア(&C)",
  L"画像(&I)", L"時計回りに90°回転", L"反時計回りに90°回転", L"左右反転", L"上下反転", L"キャンバスサイズ(&R)...",
  L"効果(&E)", L"グレースケール", L"ネガ反転", L"セピア", L"ぼかし", L"シャープ",
  L"モザイク", L"明るさ +", L"明るさ -",
  L"エンボス", L"輪郭検出", L"ポスター化", L"ノイズ",
  L"コントラスト +", L"コントラスト -", L"彩度 +", L"彩度 -",
  L"ガウスぼかし", L"油絵", L"ソラリゼーション", L"チャンネル交換",
  L"表示(&V)", L"グリッド(&G)", L"実際のサイズ(&A)", L"ウィンドウに合わせる(&F)", L"対称",
  L"ヘルプ(&H)", L"バージョン情報(&A)", L"言語(&L)",
  L"MiniPaint - カスタムペイントプログラム",
  L"  ツール: %s   色: #%02X%02X%02X   サイズ: %d   ズーム: %d%%   位置: %d,%d   [Ctrl+S 保存, Ctrl+Z 元に戻す]",
  L"エラー", L"ファイルを開けませんでした。形式がサポートされていない可能性があります。",
  L"保存されていない変更があります。終了する前に保存しますか？",
  L"MiniPaint v2.2\n\n純粋なC++とWin32 APIだけでゼロから書かれた\nペイントプログラムです。外部ライブラリは使用していません。\n\n"
  L"36種類のツール、32段階の元に戻す、20の効果、ズーム、対称、\nBMP/PNG/JPEG/GIF/TIFF/ICO対応、29か国語対応。",
  L"画像ファイル (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0ビットマップ (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"キャンセル", L"幅:", L"高さ:",
  L"カスタム色の選択", L"キャンバスサイズ", L"塗りつぶし", L"丸筆", L"角筆",
  L"多角形", L"ぼかし筆", L"スタンプ",
  L"&ガイド", L"ユーザーガイド",
  L"MINIPAINT ユーザーガイド\r\n\r\nツール\r\n鉛筆 (P): フリーハンド描画。右クリックは第2色で描きます。\r\n"
  L"直線 (L), 四角 (R), 楕円 (O), 三角形, 角丸, 星, ひし形, 五角形, 六角形, 矢印, ハート: ドラッグで描画。「塗りつぶし」ボタンで図形を塗ります。\r\n"
  L"曲線: 両端をドラッグし、制御点をクリック。Escでキャンセル。\r\n"
  L"多角形: クリックで点を追加、ダブルクリックで閉じます。\r\n"
  L"塗りつぶし (F): 領域を塗ります。スプレー (S): エアブラシ。ぼかし筆: 色を伸ばします。\r\n"
  L"消しゴム (E): 第2色で消します。スポイト (I): キャンバスから色を取得。\r\n"
  L"テキスト (T): キャンバスをクリックして入力。別の場所をクリックで確定。\r\n"
  L"選択 (M): 矩形をドラッグし、中をドラッグで移動。Deleteで削除、Escでキャンセル。\r\n"
  L"スタンプ: スマイリーを押します。サイズは筆の太さに連動。\r\n\r\n色\r\n"
  L"パレットを左クリックで第1色、右クリックで第2色を選択。\r\n"
  L"パレットのダブルクリックまたは右の色ボックスでカスタム色を選択できます。\r\n\r\n表示\r\n"
  L"Ctrl + ホイール: ズーム。中ボタンドラッグ: 移動。Ctrl+G: グリッド。\r\n"
  L"対称: 縦軸でストロークを反転します。\r\n\r\nショートカット\r\n"
  L"Ctrl+S 保存, Ctrl+O 開く, Ctrl+N 新規, Ctrl+Z 元に戻す, Ctrl+Y やり直し, Ctrl+C コピー, Ctrl+V 貼り付け。\r\n\r\n"
  L"効果\r\n効果メニューにはぼかし、シャープ、油絵、エンボスなど20種類のフィルタがあります。\r\n"
  L"ファイルはBMP、PNG、JPEG、GIF、TIFFまたはICOで保存できます。\r\n\r\n効果リファレンス\r\n"
  L"グレースケール: 色を輝度に変換。ネガ反転: すべての色を反転。\r\n"
  L"セピア: 古い写真風に。ぼかし: 3x3ボックスぼかしを適用。\r\n"
  L"シャープ: 輪郭を強調。モザイク: 画像を8x8ブロックに。\r\n"
  L"明るさ +/-: 明るく/暗く。エンボス: 浮き彫り風の見た目に。\r\n"
  L"輪郭検出: 輪郭を抽出。ポスター化: 色を4段階に。\r\n"
  L"ノイズ: ランダムな点を追加。コントラストと彩度 +/-: トーン調整。\r\n"
  L"ガウスぼかし: 滑らかな5x5ぼかし。油絵: 筆致のある見た目に。\r\n"
  L"ソラリゼーション: 明るいトーンを反転。チャンネル交換: 赤と青を交換。\r\n\r\nよくある質問\r\n"
  L"Q: 元に戻せるのは何段階? A: 32段階。\r\n"
  L"Q: 設定はどこに保存? A: プログラムの隣のminipaint.iniに。\r\n"
  L"Q: 描いた絵を商用利用できる? A: はい、完全にあなたのものです。\r\n"
  L"Q: キャンバスがちらつかないのはなぜ? A: すべての描画にダブルバッファを使用。\r\n",
  L"&ソースコード", L"ソースコード - MiniPaint",
  L"水彩", L"濡れ混色", L"油彩", L"鉛筆", L"カリグラ", L"リボン",
  L"ネオン", L"影", L"参照画像", L"読み込み...", L"表示/非表示", L"削除", L"手ブレ補正", L"ディザ", L"ピクセルパーフェクト", L"ピクセルパレット", L"Gペン", L"トーン", L"効果線", L"吹き出し", L"コマ",
  L"ファイルを保存できませんでした。場所が読み取り専用か、ディスクがいっぱいの可能性があります。" },
// ---- Cince ----
{ L"铅笔", L"直线", L"矩形", L"椭圆", L"填充", L"橡皮", L"取色",
  L"喷枪", L"三角形", L"圆角矩形", L"星形", L"文本", L"选择",
  L"曲线", L"菱形", L"五边形", L"六边形", L"箭头", L"心形",
  L"文件(&F)", L"新建(&N)", L"打开(&O)...", L"保存(&S)", L"另存为(&A)...", L"退出(&X)",
  L"编辑(&E)", L"撤销(&U)\tCtrl+Z", L"重做(&R)\tCtrl+Y", L"清空画布(&C)",
  L"图像(&I)", L"顺时针旋转90°", L"逆时针旋转90°", L"水平翻转", L"垂直翻转", L"调整画布大小(&R)...",
  L"效果(&E)", L"灰度", L"反色", L"怀旧", L"模糊", L"锐化",
  L"像素化", L"亮度 +", L"亮度 -",
  L"浮雕", L"边缘检测", L"色调分离", L"添加噪点",
  L"对比度 +", L"对比度 -", L"饱和度 +", L"饱和度 -",
  L"高斯模糊", L"油画", L"曝光反转", L"交换通道",
  L"视图(&V)", L"网格(&G)", L"实际大小(&A)", L"适应窗口(&F)", L"对称",
  L"帮助(&H)", L"关于(&A)", L"语言(&L)",
  L"MiniPaint - 自定义绘图程序",
  L"  工具: %s   颜色: #%02X%02X%02X   大小: %d   缩放: %d%%   位置: %d,%d   [Ctrl+S 保存, Ctrl+Z 撤销]",
  L"错误", L"无法打开文件，或不支持该格式。",
  L"有未保存的更改。是否在退出前保存？",
  L"MiniPaint v2.2\n\n完全从零开始用纯C++和Win32 API编写的绘图程序。\n不使用任何外部库。\n\n"
  L"36种工具、32步撤销、20种特效、缩放、对称、\n支持BMP/PNG/JPEG/GIF/TIFF/ICO和29种语言。",
  L"图像文件 (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0位图 (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"确定", L"取消", L"宽度:", L"高度:",
  L"选择自定义颜色", L"画布大小", L"填充", L"圆笔头", L"方笔头",
  L"多边形", L"涂抹", L"图章",
  L"&用户指南", L"用户指南",
  L"MINIPAINT 用户指南\r\n\r\n工具\r\n铅笔 (P): 自由绘制。右键使用副颜色绘制。\r\n"
  L"直线 (L)、矩形 (R)、椭圆 (O)、三角形、圆角矩形、星形、菱形、五边形、六边形、箭头和心形: 拖动绘制。“填充”按钮可填充形状。\r\n"
  L"曲线: 拖动两端点，然后单击控制点。Esc 取消。\r\n"
  L"多边形: 单击添加点，双击闭合。\r\n"
  L"填充 (F): 填充区域。喷枪 (S): 喷涂。涂抹: 像湿颜料一样涂抹颜色。\r\n"
  L"橡皮 (E): 擦除为副颜色。取色 (I): 从画布取样颜色。\r\n"
  L"文本 (T): 单击画布并输入；单击其他位置应用。\r\n"
  L"选择 (M): 拖动矩形，然后拖动内部移动内容。Delete 删除，Esc 取消。\r\n"
  L"图章: 盖上笑脸；大小跟随画笔粗细。\r\n\r\n颜色\r\n"
  L"左键单击调色板选择主颜色，右键选择副颜色。\r\n"
  L"双击调色板或单击右侧颜色框可选择自定义颜色。\r\n\r\n视图\r\n"
  L"Ctrl + 滚轮: 缩放。中键拖动: 平移。Ctrl+G: 网格。\r\n"
  L"对称: 沿垂直轴镜像笔画。\r\n\r\n快捷键\r\n"
  L"Ctrl+S 保存, Ctrl+O 打开, Ctrl+N 新建, Ctrl+Z 撤销, Ctrl+Y 重做, Ctrl+C 复制, Ctrl+V 粘贴。\r\n\r\n"
  L"效果\r\n效果菜单提供20种滤镜: 模糊、锐化、油画、浮雕等。\r\n"
  L"文件可保存为BMP、PNG、JPEG、GIF、TIFF或ICO。\r\n\r\n效果参考\r\n"
  L"灰度: 将颜色转换为亮度。反色: 反转所有颜色。\r\n"
  L"怀旧: 呈现老照片效果。模糊: 应用3x3盒式模糊。\r\n"
  L"锐化: 突出边缘。像素化: 将图像缩减为8x8方块。\r\n"
  L"亮度 +/-: 增亮或变暗。浮雕: 呈现凸起表面效果。\r\n"
  L"边缘检测: 提取轮廓。色调分离: 将颜色缩减为4级。\r\n"
  L"添加噪点: 添加随机斑点。对比度和饱和度 +/-: 色调调整。\r\n"
  L"高斯模糊: 柔和的5x5模糊。油画: 呈现笔触效果。\r\n"
  L"曝光反转: 反转亮色调。交换通道: 交换红色和蓝色。\r\n\r\n常见问题\r\n"
  L"问: 撤销有多少步? 答: 32步。\r\n"
  L"问: 设置保存在哪里? 答: 程序旁边的minipaint.ini。\r\n"
  L"问: 我的画可以商用吗? 答: 可以，完全属于您。\r\n"
  L"问: 画布为什么不闪烁? 答: 所有绘制都使用双缓冲。\r\n",
  L"&源代码", L"源代码 - MiniPaint",
  L"水彩", L"湿混色", L"油画", L"铅笔", L"书法", L"彩带",
  L"霓虹", L"阴影", L"参考图", L"加载...", L"显示/隐藏", L"移除", L"防抖", L"抖动", L"完美像素", L"像素调色板", L"G笔", L"网点", L"速度线", L"气泡", L"分格",
  L"无法保存文件。位置可能为只读，或磁盘已满。" },
// ---- Nederlands ----
{ L"Potlood", L"Lijn", L"Vak", L"Ellips", L"Vullen", L"Gum", L"Kleur nemen",
  L"Spray", L"Driehoek", L"Afgerond", L"Ster", L"Tekst", L"Selectie",
  L"Kromme", L"Ruit", L"Vijfhoek", L"Zeshoek", L"Pijl", L"Hart",
  L"&Bestand", L"&Nieuw", L"&Openen...", L"&Opslaan", L"Opslaan &als...", L"&Afsluiten",
  L"&Bewerken", L"&Ongedaan maken\tCtrl+Z", L"&Opnieuw\tCtrl+Y", L"&Canvas wissen",
  L"&Afbeelding", L"90° rechtsom draaien", L"90° linksom draaien", L"Horizontaal spiegelen", L"Verticaal spiegelen", L"Canvas&grootte...",
  L"Eff&ecten", L"Grijstinten", L"Inverteren", L"Sepia", L"Vervagen", L"Verscherpen",
  L"Pixelen", L"Helderheid +", L"Helderheid -",
  L"Reliëf", L"Randdetectie", L"Posteriseren", L"Ruis",
  L"Contrast +", L"Contrast -", L"Verzadiging +", L"Verzadiging -",
  L"Gaussisch vervagen", L"Olieverf", L"Solariseren", L"Kanalen wisselen",
  L"&Weergave", L"&Raster", L"&Werkelijk formaat", L"&Aanpassen aan venster", L"S&ymmetrie",
  L"&Help", L"&Info", L"&Taal",
  L"MiniPaint - Eigen tekenprogramma",
  L"  Gereedschap: %s   Kleur: #%02X%02X%02X   Grootte: %d   Zoom: %d%%   Positie: %d,%d   [Ctrl+S Opslaan, Ctrl+Z Ongedaan maken]",
  L"Fout", L"Kan bestand niet openen of formaat wordt niet ondersteund.",
  L"Er zijn niet-opgeslagen wijzigingen. Wilt u opslaan voordat u afsluit?",
  L"MiniPaint v2.2\n\nEen tekenprogramma volledig vanaf nul geschreven\nin pure C++ en de Win32 API.\nGeen externe bibliotheken.\n\n"
  L"36 gereedschappen, 32-staps ongedaan maken, 20 effecten, zoom, symmetrie,\nBMP/PNG/JPEG/GIF/TIFF/ICO-ondersteuning en 29 talen.",
  L"Afbeeldingsbestanden (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Annuleren", L"Breedte:", L"Hoogte:",
  L"Aangepaste kleur kiezen", L"Canvasgrootte", L"Gevuld", L"Ronde punt", L"Vierkante punt",
  L"Veelhoek", L"Veeg uit", L"Stempel",
  L"&Handleiding", L"Handleiding",
  L"MINIPAINT HANDLEIDING\r\n\r\nGEREEDSCHAPPEN\r\nPotlood (P): vrijhand tekenen. Rechtermuisknop tekent met de secundaire kleur.\r\n"
  L"Lijn (L), Vak (R), Ellips (O), Driehoek, Afgerond, Ster, Ruit, Vijfhoek, Zeshoek, Pijl en Hart: sleep om te tekenen. 'Gevuld' vult vormen.\r\n"
  L"Kromme: sleep de eindpunten, klik daarna het controlepunt. Esc annuleert.\r\n"
  L"Veelhoek: klik om punten toe te voegen, dubbelklik om te sluiten.\r\n"
  L"Vullen (F): vult een gebied. Spray (S): airbrush. Veeg uit: smeert kleuren als natte verf.\r\n"
  L"Gum (E): wist naar de secundaire kleur. Kleur nemen (I): neemt een kleur van het canvas.\r\n"
  L"Tekst (T): klik op het canvas en typ; klik elders om toe te passen.\r\n"
  L"Selectie (M): sleep een rechthoek en verplaats de inhoud. Delete wist, Esc annuleert.\r\n"
  L"Stempel: stempelt een smiley; de grootte volgt de penseeldikte.\r\n\r\nKLEUREN\r\n"
  L"Linksklik op het palet voor de primaire kleur, rechtsklik voor de secundaire.\r\n"
  L"Dubbelklik op het palet of klik op een kleurvak rechts voor een aangepaste kleur.\r\n\r\nWEERGAVE\r\n"
  L"Ctrl + muiswiel: zoom. Slepen met middelste knop: verschuiven. Ctrl+G: raster.\r\n"
  L"Symmetrie spiegelt streken over de verticale as.\r\n\r\nSNELTOETSEN\r\n"
  L"Ctrl+S opslaan, Ctrl+O openen, Ctrl+N nieuw, Ctrl+Z ongedaan maken, Ctrl+Y opnieuw, Ctrl+C kopiëren, Ctrl+V plakken.\r\n\r\n"
  L"EFFECTEN\r\nHet menu Effecten biedt 20 filters zoals vervagen, verscherpen, olieverf en reliëf.\r\n"
  L"Bestanden kunnen worden opgeslagen als BMP, PNG, JPEG, GIF, TIFF of ICO.\r\n\r\nEFFECTENREFERENTIE\r\n"
  L"Grijstinten: zet kleuren om naar helderheid. Inverteren: keert alle kleuren om.\r\n"
  L"Sepia: geeft een oude-fotolook. Vervagen: past een 3x3-vervaging toe.\r\n"
  L"Verscherpen: benadrukt randen. Pixelen: reduceert het beeld tot 8x8-blokken.\r\n"
  L"Helderheid +/-: lichter of donkerder. Reliëf: geeft een verhoogd-oppervlak-effect.\r\n"
  L"Randdetectie: haalt contouren eruit. Posteriseren: reduceert kleuren tot 4 niveaus.\r\n"
  L"Ruis: voegt willekeurige stippen toe. Contrast en Verzadiging +/-: toonaanpassingen.\r\n"
  L"Gaussisch vervagen: zachte 5x5-vervaging. Olieverf: geeft een penseelstreek-look.\r\n"
  L"Solariseren: keert lichte tonen om. Kanalen wisselen: wisselt rood en blauw.\r\n\r\nFAQ\r\n"
  L"V: Hoeveel stappen ongedaan maken? A: 32 stappen.\r\n"
  L"V: Waar worden instellingen opgeslagen? A: In minipaint.ini naast het programma.\r\n"
  L"V: Mag ik mijn tekeningen commercieel gebruiken? A: Ja, ze zijn helemaal van u.\r\n"
  L"V: Waarom flikkert het canvas niet? A: Alles wordt met dubbele buffering getekend.\r\n",
  L"&Broncode", L"Broncode - MiniPaint",
  L"Aquarel", L"Nat", L"Olie", L"Potlood", L"Kalligr", L"Lint",
  L"Neon", L"Schaduw", L"Referentie", L"Laden...", L"Tonen/verbergen", L"Verwijderen", L"Stabilisatie", L"Dither", L"Pixelperfect", L"Pixelpalet", L"G-Pen", L"Raster", L"Snelheid", L"Ballon", L"Paneel",
  L"Kan het bestand niet opslaan. De locatie is mogelijk alleen-lezen of de schijf is vol." },
// ---- Polski ----
{ L"Ołówek", L"Linia", L"Prostokąt", L"Elipsa", L"Wypełnij", L"Gumka", L"Pobierz",
  L"Spray", L"Trójkąt", L"Zaokrągl.", L"Gwiazda", L"Tekst", L"Zaznacz",
  L"Krzywa", L"Romb", L"Pięciokąt", L"Sześciokąt", L"Strzałka", L"Serce",
  L"&Plik", L"&Nowy", L"&Otwórz...", L"&Zapisz", L"Zapisz &jako...", L"&Zakończ",
  L"&Edycja", L"&Cofnij\tCtrl+Z", L"&Ponów\tCtrl+Y", L"&Wyczyść płótno",
  L"&Obraz", L"Obróć 90° w prawo", L"Obróć 90° w lewo", L"Odbij poziomo", L"Odbij pionowo", L"&Rozmiar płótna...",
  L"&Efekty", L"Skala szarości", L"Negatyw", L"Sepia", L"Rozmycie", L"Wyostrzenie",
  L"Pikselizacja", L"Jasność +", L"Jasność -",
  L"Relief", L"Wykryj krawędzie", L"Posteryzacja", L"Szum",
  L"Kontrast +", L"Kontrast -", L"Nasycenie +", L"Nasycenie -",
  L"Rozmycie Gaussa", L"Farba olejna", L"Solaryzacja", L"Zamień kanały",
  L"&Widok", L"&Siatka", L"&Rzeczywisty rozmiar", L"&Dopasuj do okna", L"S&ymetria",
  L"Pomo&c", L"&Informacje", L"&Język",
  L"MiniPaint - Własny program do rysowania",
  L"  Narzędzie: %s   Kolor: #%02X%02X%02X   Rozmiar: %d   Zoom: %d%%   Pozycja: %d,%d   [Ctrl+S Zapisz, Ctrl+Z Cofnij]",
  L"Błąd", L"Nie można otworzyć pliku lub format nie jest obsługiwany.",
  L"Istnieją niezapisane zmiany. Zapisać przed zamknięciem?",
  L"MiniPaint v2.2\n\nProgram do rysowania napisany całkowicie od zera\nw czystym C++ i Win32 API.\nBez zewnętrznych bibliotek.\n\n"
  L"36 narzędzia, 32-krotne cofanie, 20 efektów, zoom, symetria,\nobsługa BMP/PNG/JPEG/GIF/TIFF/ICO i 29 języków.",
  L"Pliki graficzne (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Anuluj", L"Szerokość:", L"Wysokość:",
  L"Wybierz kolor niestandardowy", L"Rozmiar płótna", L"Wypełnione", L"Okrągła końcówka", L"Kwadratowa końcówka",
  L"Wielokąt", L"Rozmaż", L"Stempel",
  L"&Przewodnik", L"Przewodnik użytkownika",
  L"PRZEWODNIK MINIPAINT\r\n\r\nNARZĘDZIA\r\nOłówek (P): rysowanie odręczne. Prawy przycisk rysuje kolorem dodatkowym.\r\n"
  L"Linia (L), Prostokąt (R), Elipsa (O), Trójkąt, Zaokrąglony, Gwiazda, Romb, Pięciokąt, Sześciokąt, Strzałka i Serce: przeciągnij, aby narysować. 'Wypełnione' wypełnia kształty.\r\n"
  L"Krzywa: przeciągnij końce, a potem kliknij punkt kontrolny. Esc anuluje.\r\n"
  L"Wielokąt: klikaj, aby dodawać punkty, podwójne kliknięcie zamyka kształt.\r\n"
  L"Wypełnij (F): wypełnia obszar. Spray (S): aerograf. Rozmaż: rozciąga kolory jak świeża farba.\r\n"
  L"Gumka (E): usuwa do koloru dodatkowego. Pobierz (I): pobiera kolor z płótna.\r\n"
  L"Tekst (T): kliknij płótno i pisz; kliknij gdzie indziej, aby zastosować.\r\n"
  L"Zaznacz (M): przeciągnij prostokąt i przenieś zawartość. Delete usuwa, Esc anuluje.\r\n"
  L"Stempel: stempluje uśmiech; rozmiar zależy od grubości pędzla.\r\n\r\nKOLORY\r\n"
  L"Lewy klik na palecie wybiera kolor główny, prawy klik — dodatkowy.\r\n"
  L"Podwójny klik na palecie lub klik na pole koloru po prawej otwiera kolor niestandardowy.\r\n\r\nWIDOK\r\n"
  L"Ctrl + kółko myszy: zoom. Przeciąganie środkowym przyciskiem: przesuwanie. Ctrl+G: siatka.\r\n"
  L"Symetria odbija pociągnięcia względem osi pionowej.\r\n\r\nSKRÓTY\r\n"
  L"Ctrl+S zapisz, Ctrl+O otwórz, Ctrl+N nowy, Ctrl+Z cofnij, Ctrl+Y ponów, Ctrl+C kopiuj, Ctrl+V wklej.\r\n\r\n"
  L"EFEKTY\r\nMenu Efekty oferuje 20 filtrów, m.in. rozmycie, wyostrzenie, farbę olejną i relief.\r\n"
  L"Pliki można zapisywać jako BMP, PNG, JPEG, GIF, TIFF lub ICO.\r\n\r\nOPIS EFEKTÓW\r\n"
  L"Skala szarości: zamienia kolory na jasność. Negatyw: odwraca wszystkie kolory.\r\n"
  L"Sepia: nadaje wygląd starego zdjęcia. Rozmycie: stosuje rozmycie 3x3.\r\n"
  L"Wyostrzenie: podkreśla krawędzie. Pikselizacja: redukuje obraz do bloków 8x8.\r\n"
  L"Jasność +/-: rozjaśnia lub przyciemnia. Relief: nadaje efekt wypukłej powierzchni.\r\n"
  L"Wykryj krawędzie: wydobywa kontury. Posteryzacja: redukuje kolory do 4 poziomów.\r\n"
  L"Szum: dodaje losowe punkty. Kontrast i Nasycenie +/-: regulacja tonów.\r\n"
  L"Rozmycie Gaussa: miękkie rozmycie 5x5. Farba olejna: nadaje wygląd pociągnięć pędzla.\r\n"
  L"Solaryzacja: odwraca jasne tony. Zamień kanały: zamienia czerwony i niebieski.\r\n\r\nFAQ\r\n"
  L"P: Ile kroków cofania? O: 32 kroki.\r\n"
  L"P: Gdzie zapisywane są ustawienia? O: W pliku minipaint.ini obok programu.\r\n"
  L"P: Czy mogę używać rysunków komercyjnie? O: Tak, są całkowicie twoje.\r\n"
  L"P: Dlaczego płótno nie migocze? O: Całe rysowanie używa podwójnego bufora.\r\n",
  L"&Kod źródłowy", L"Kod źródłowy - MiniPaint",
  L"Akwarela", L"Mokry", L"Olej", L"Ołówek", L"Kaligr", L"Wstążka",
  L"Neon", L"Cień", L"Referencja", L"Wczytaj...", L"Pokaż/ukryj", L"Usuń", L"Stabilizacja", L"Dithering", L"Idealny piksel", L"Paleta pikseli", L"G-Pen", L"Raster", L"Szybkość", L"Dymek", L"Panel",
  L"Nie można zapisać pliku. Lokalizacja może być tylko do odczytu lub dysk jest pełny." },
// ---- Ukrayinska ----
{ L"Олівець", L"Лінія", L"Прямокут.", L"Еліпс", L"Заливка", L"Гумка", L"Піпетка",
  L"Спрей", L"Трикутник", L"Заокругл.", L"Зірка", L"Текст", L"Вибір",
  L"Крива", L"Ромб", L"П'ятикут.", L"Шестикут.", L"Стрілка", L"Серце",
  L"&Файл", L"&Новий", L"&Відкрити...", L"&Зберегти", L"Зберегти &як...", L"В&ихід",
  L"&Правка", L"&Скасувати\tCtrl+Z", L"&Повторити\tCtrl+Y", L"&Очистити полотно",
  L"&Зображення", L"Повернути 90° праворуч", L"Повернути 90° ліворуч", L"Віддзеркалити гориз.", L"Віддзеркалити верт.", L"&Розмір полотна...",
  L"Е&фекти", L"Відтінки сірого", L"Негатив", L"Сепія", L"Розмиття", L"Різкість",
  L"Пікселізація", L"Яскравість +", L"Яскравість -",
  L"Рельєф", L"Контури", L"Постеризація", L"Шум",
  L"Контраст +", L"Контраст -", L"Насиченість +", L"Насиченість -",
  L"Розмиття Гаусса", L"Олійна фарба", L"Соляризація", L"Поміняти канали",
  L"&Вигляд", L"&Сітка", L"&Реальний розмір", L"&За розміром вікна", L"Симетрія",
  L"&Довідка", L"&Про програму", L"&Мова",
  L"MiniPaint - Програма для малювання",
  L"  Інструмент: %s   Колір: #%02X%02X%02X   Розмір: %d   Масштаб: %d%%   Позиція: %d,%d   [Ctrl+S Зберегти, Ctrl+Z Скасувати]",
  L"Помилка", L"Не вдалося відкрити файл або формат не підтримується.",
  L"Є незбережені зміни. Зберегти перед виходом?",
  L"MiniPaint v2.2\n\nПрограма для малювання, повністю написана з нуля\nна чистому C++ і Win32 API.\nБез зовнішніх бібліотек.\n\n"
  L"36 інструменти, скасування на 32 кроки, 20 ефектів, масштаб, симетрія,\nпідтримка BMP/PNG/JPEG/GIF/TIFF/ICO і 29 мов.",
  L"Файли зображень (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"Гаразд", L"Скасувати", L"Ширина:", L"Висота:",
  L"Вибір кольору", L"Розмір полотна", L"Залито", L"Круглий пензель", L"Квадратний пензель",
  L"Багатокут.", L"Розмазати", L"Штамп",
  L"&Посібник", L"Посібник користувача",
  L"ПОСІБНИК MINIPAINT\r\n\r\nІНСТРУМЕНТИ\r\nОлівець (P): малювання від руки. Права кнопка малює додатковим кольором.\r\n"
  L"Лінія (L), Прямокутник (R), Еліпс (O), Трикутник, Заокруглений, Зірка, Ромб, П'ятикутник, Шестикутник, Стрілка і Серце: малюються перетягуванням. 'Залито' заповнює фігури.\r\n"
  L"Крива: перетягніть кінці, потім клацніть контрольну точку. Esc скасовує.\r\n"
  L"Багатокутник: клацайте, щоб додавати точки, подвійне клацання замикає фігуру.\r\n"
  L"Заливка (F): заливає область. Спрей (S): аерограф. Розмазати: розмазує кольори як свіжу фарбу.\r\n"
  L"Гумка (E): стирає до додаткового кольору. Піпетка (I): бере колір з полотна.\r\n"
  L"Текст (T): клацніть полотно і друкуйте; клацання в іншому місці застосовує текст.\r\n"
  L"Вибір (M): розтягніть прямокутник і перетягніть вміст. Delete видаляє, Esc скасовує.\r\n"
  L"Штамп: ставить смайлик; розмір залежить від товщини пензля.\r\n\r\nКОЛЬОРИ\r\n"
  L"Ліве клацання по палітрі вибирає основний колір, праве — додатковий.\r\n"
  L"Подвійне клацання по палітрі або клацання по кольоровому полю праворуч відкриває вибір кольору.\r\n\r\nВИГЛЯД\r\n"
  L"Ctrl + коліщатко: масштаб. Перетягування середньою кнопкою: зсув. Ctrl+G: сітка.\r\n"
  L"Симетрія віддзеркалює штрихи по вертикальній осі.\r\n\r\nГАРЯЧІ КЛАВІШІ\r\n"
  L"Ctrl+S зберегти, Ctrl+O відкрити, Ctrl+N новий, Ctrl+Z скасувати, Ctrl+Y повторити, Ctrl+C копіювати, Ctrl+V вставити.\r\n\r\n"
  L"ЕФЕКТИ\r\nМеню ефектів пропонує 20 фільтрів: розмиття, різкість, олійна фарба, рельєф тощо.\r\n"
  L"Файли зберігаються як BMP, PNG, JPEG, GIF, TIFF або ICO.\r\n\r\nДОВІДКА ПО ЕФЕКТАХ\r\n"
  L"Відтінки сірого: перетворює кольори в яскравість. Негатив: інвертує всі кольори.\r\n"
  L"Сепія: надає вигляд старої фотографії. Розмиття: застосовує розмиття 3x3.\r\n"
  L"Різкість: підкреслює краї. Пікселізація: зменшує зображення до блоків 8x8.\r\n"
  L"Яскравість +/-: освітлює або затемнює. Рельєф: надає ефект опуклої поверхні.\r\n"
  L"Контури: виділяє межі. Постеризація: скорочує кольори до 4 рівнів.\r\n"
  L"Шум: додає випадкові точки. Контраст і Насиченість +/-: тональне налаштування.\r\n"
  L"Розмиття Гаусса: м'яке розмиття 5x5. Олійна фарба: надає вигляд мазків пензля.\r\n"
  L"Соляризація: інвертує світлі тони. Поміняти канали: міняє червоний і синій.\r\n\r\nПИТАННЯ\r\n"
  L"П: Скільки кроків скасування? В: 32 кроки.\r\n"
  L"П: Де зберігаються налаштування? В: У файлі minipaint.ini поруч із програмою.\r\n"
  L"П: Чи можна використовувати малюнки комерційно? В: Так, вони повністю ваші.\r\n"
  L"П: Чому полотно не мерехтить? В: Все малювання використовує подвійну буферизацію.\r\n",
  L"&Вихідний код", L"Вихідний код - MiniPaint",
  L"Акварель", L"Мокрий", L"Олія", L"Олівець", L"Калігр", L"Стрічка",
  L"Неон", L"Тінь", L"Референс", L"Завантажити...", L"Показати/сховати", L"Прибрати", L"Стабілізатор", L"Дізеринг", L"Ідеальний піксель", L"Піксельна палітра", L"G-перо", L"Тон", L"Швидкість", L"Балон", L"Панель",
  L"Не вдалося зберегти файл. Можливо, розташування доступне лише для читання або диск заповнений." },
// ---- Korece ----
{ L"연필", L"직선", L"사각형", L"타원", L"채우기", L"지우개", L"색 추출",
  L"스프레이", L"삼각형", L"둥근 사각형", L"별", L"텍스트", L"선택",
  L"곡선", L"마름모", L"오각형", L"육각형", L"화살표", L"하트",
  L"파일(&F)", L"새로 만들기(&N)", L"열기(&O)...", L"저장(&S)", L"다른 이름으로 저장(&A)...", L"끝내기(&X)",
  L"편집(&E)", L"실행 취소(&U)\tCtrl+Z", L"다시 실행(&R)\tCtrl+Y", L"캔버스 지우기(&C)",
  L"이미지(&I)", L"시계 방향 90° 회전", L"반시계 방향 90° 회전", L"좌우 반전", L"상하 반전", L"캔버스 크기(&R)...",
  L"효과(&E)", L"회색조", L"반전", L"세피아", L"흐림", L"선명하게",
  L"모자이크", L"밝기 +", L"밝기 -",
  L"엠보스", L"윤곽 검출", L"포스터화", L"노이즈",
  L"대비 +", L"대비 -", L"채도 +", L"채도 -",
  L"가우시안 흐림", L"유화", L"솔라라이즈", L"채널 교환",
  L"보기(&V)", L"격자(&G)", L"실제 크기(&A)", L"창에 맞추기(&F)", L"대칭",
  L"도움말(&H)", L"정보(&A)", L"언어(&L)",
  L"MiniPaint - 사용자 정의 그림 프로그램",
  L"  도구: %s   색: #%02X%02X%02X   크기: %d   확대: %d%%   위치: %d,%d   [Ctrl+S 저장, Ctrl+Z 실행 취소]",
  L"오류", L"파일을 열 수 없거나 형식이 지원되지 않습니다.",
  L"저장되지 않은 변경 사항이 있습니다. 종료하기 전에 저장하시겠습니까?",
  L"MiniPaint v2.2\n\n순수 C++과 Win32 API만으로 처음부터 작성된\n그림 프로그램입니다. 외부 라이브러리를 사용하지 않습니다.\n\n"
  L"36가지 도구, 32단계 실행 취소, 20가지 효과, 확대, 대칭,\nBMP/PNG/JPEG/GIF/TIFF/ICO 지원 및 29개 언어.",
  L"이미지 파일 (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0비트맵 (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"확인", L"취소", L"너비:", L"높이:",
  L"사용자 지정 색 선택", L"캔버스 크기", L"채우기", L"둥근 붓", L"사각 붓",
  L"다각형", L"번지기", L"도장",
  L"&사용 안내서", L"사용 안내서",
  L"MINIPAINT 사용 안내서\r\n\r\n도구\r\n연필 (P): 자유 그리기. 오른쪽 버튼은 보조 색으로 그립니다.\r\n"
  L"직선 (L), 사각형 (R), 타원 (O), 삼각형, 둥근 사각형, 별, 마름모, 오각형, 육각형, 화살표, 하트: 드래그하여 그립니다. '채우기' 버튼으로 도형을 채웁니다.\r\n"
  L"곡선: 양끝을 드래그한 후 제어점을 클릭합니다. Esc로 취소.\r\n"
  L"다각형: 클릭으로 점을 추가하고 더블 클릭으로 닫습니다.\r\n"
  L"채우기 (F): 영역을 채웁니다. 스프레이 (S): 에어브러시. 번지기: 색을 번지게 합니다.\r\n"
  L"지우개 (E): 보조 색으로 지웁니다. 색 추출 (I): 캔버스에서 색을 가져옵니다.\r\n"
  L"텍스트 (T): 캔버스를 클릭하고 입력하세요. 다른 곳을 클릭하면 적용됩니다.\r\n"
  L"선택 (M): 사각형을 드래그한 후 내부를 드래그하여 이동합니다. Delete로 삭제, Esc로 취소.\r\n"
  L"도장: 스마일을 찍습니다. 크기는 붓 굵기에 따릅니다.\r\n\r\n색상\r\n"
  L"팔레트를 왼쪽 클릭하면 주 색상, 오른쪽 클릭하면 보조 색상이 선택됩니다.\r\n"
  L"팔레트를 더블 클릭하거나 오른쪽 색상 상자를 클릭하면 사용자 지정 색을 선택할 수 있습니다.\r\n\r\n보기\r\n"
  L"Ctrl + 마우스 휠: 확대/축소. 가운데 버튼 드래그: 이동. Ctrl+G: 격자.\r\n"
  L"대칭: 세로 축을 기준으로 선을 반전합니다.\r\n\r\n바로 가기\r\n"
  L"Ctrl+S 저장, Ctrl+O 열기, Ctrl+N 새로 만들기, Ctrl+Z 실행 취소, Ctrl+Y 다시 실행, Ctrl+C 복사, Ctrl+V 붙여넣기.\r\n\r\n"
  L"효과\r\n효과 메뉴에는 흐림, 선명하게, 유화, 엠보스 등 20가지 필터가 있습니다.\r\n"
  L"파일은 BMP, PNG, JPEG, GIF, TIFF 또는 ICO로 저장할 수 있습니다.\r\n\r\n효과 참조\r\n"
  L"회색조: 색을 밝기로 변환. 반전: 모든 색을 반전.\r\n"
  L"세피아: 오래된 사진 느낌. 흐림: 3x3 박스 흐림 적용.\r\n"
  L"선명하게: 윤곽 강조. 모자이크: 이미지를 8x8 블록으로.\r\n"
  L"밝기 +/-: 밝게 또는 어둡게. 엠보스: 돌출된 표면 효과.\r\n"
  L"윤곽 검출: 윤곽선 추출. 포스터화: 색을 4단계로.\r\n"
  L"노이즈: 무작위 점 추가. 대비와 채도 +/-: 톤 조정.\r\n"
  L"가우시안 흐림: 부드러운 5x5 흐림. 유화: 붓터치 느낌.\r\n"
  L"솔라라이즈: 밝은 톤 반전. 채널 교환: 빨강과 파랑 교환.\r\n\r\n자주 묻는 질문\r\n"
  L"Q: 실행 취소는 몇 단계? A: 32단계.\r\n"
  L"Q: 설정은 어디에 저장? A: 프로그램 옆 minipaint.ini에.\r\n"
  L"Q: 그린 그림을 상업적으로 쓸 수 있나요? A: 네, 전적으로 사용자의 것입니다.\r\n"
  L"Q: 캔버스가 깜빡이지 않는 이유는? A: 모든 그리기에 이중 버퍼링 사용.\r\n",
  L"&소스 코드", L"소스 코드 - MiniPaint",
  L"수채화", L"혼합", L"유화", L"연필", L"캘리그", L"리본",
  L"네온", L"그림자", L"참조 이미지", L"불러오기...", L"표시/숨기기", L"제거", L"흔들림 보정", L"디더", L"픽셀 퍼펙트", L"픽셀 팔레트", L"G펜", L"톤", L"스피드선", L"말풍선", L"패널",
  L"파일을 저장할 수 없습니다. 위치가 읽기 전용이거나 디스크가 가득 찼을 수 있습니다." },
// ---- Arapca ----
{ L"قلم رصاص", L"خط", L"مستطيل", L"بيضاوي", L"تعبئة", L"ممحاة", L"قطارة",
  L"رذاذ", L"مثلث", L"مستدير", L"نجمة", L"نص", L"تحديد",
  L"منحنى", L"معين", L"خماسي", L"سداسي", L"سهم", L"قلب",
  L"&ملف", L"&جديد", L"&فتح...", L"&حفظ", L"حفظ &باسم...", L"&خروج",
  L"&تحرير", L"&تراجع\tCtrl+Z", L"&إعادة\tCtrl+Y", L"&مسح اللوحة",
  L"&صورة", L"تدوير 90° لليمين", L"تدوير 90° لليسار", L"قلب أفقي", L"قلب عمودي", L"&حجم اللوحة...",
  L"&تأثيرات", L"تدرج رمادي", L"عكس", L"بني داكن", L"ضبابية", L"حدة",
  L"بكسلة", L"سطوع +", L"سطوع -",
  L"بارز", L"كشف الحواف", L"ملصق", L"ضوضاء",
  L"تباين +", L"تباين -", L"تشبع +", L"تشبع -",
  L"ضبابية غاوس", L"طلاء زيتي", L"تعريض", L"تبديل القنوات",
  L"&عرض", L"&شبكة", L"&الحجم الفعلي", L"&ملاءمة النافذة", L"&تماثل",
  L"&مساعدة", L"&حول", L"&اللغة",
  L"MiniPaint - برنامج رسم مخصص",
  L"  الأداة: %s   اللون: #%02X%02X%02X   الحجم: %d   التكبير: %d%%   الموضع: %d,%d   [Ctrl+S حفظ، Ctrl+Z تراجع]",
  L"خطأ", L"تعذر فتح الملف أو التنسيق غير مدعوم.",
  L"هناك تغييرات غير محفوظة. هل تريد الحفظ قبل الخروج؟",
  L"MiniPaint v2.2\n\nبرنامج رسم مكتوب بالكامل من الصفر\nبلغة C++ النقية وواجهة Win32.\nبدون مكتبات خارجية.\n\n"
  L"36 أداة، تراجع 32 خطوة، 20 تأثيراً، تكبير، تماثل،\nودعم BMP/PNG/JPEG/GIF/TIFF/ICO و29 لغة.",
  L"ملفات الصور (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"موافق", L"إلغاء", L"العرض:", L"الارتفاع:",
  L"اختيار لون مخصص", L"حجم اللوحة", L"معبأ", L"طرف دائري", L"طرف مربع",
  L"مضلع", L"تلطيخ", L"ختم",
  L"&دليل المستخدم", L"دليل المستخدم",
  L"دليل MINIPAINT\r\n\r\nالأدوات\r\nقلم رصاص (P): رسم حر. الزر الأيمن يرسم باللون الثانوي.\r\n"
  L"خط (L)، مستطيل (R)، بيضاوي (O)، مثلث، مستدير، نجمة، معين، خماسي، سداسي، سهم وقلب: اسحب للرسم. زر 'معبأ' يملأ الأشكال.\r\n"
  L"منحنى: اسحب الطرفين ثم انقر لنقطة التحكم. Esc للإلغاء.\r\n"
  L"مضلع: انقر لإضافة نقاط، انقر مرتين للإغلاق.\r\n"
  L"تعبئة (F): تملأ منطقة. رذاذ (S): بخاخ. تلطيخ: يمدد الألوان كالطلاء الرطب.\r\n"
  L"ممحاة (E): تمسح إلى اللون الثانوي. قطارة (I): تأخذ لوناً من اللوحة.\r\n"
  L"نص (T): انقر على اللوحة واكتب؛ انقر في مكان آخر للتطبيق.\r\n"
  L"تحديد (M): اسحب مستطيلاً ثم اسحب المحتوى لنقله. Delete يحذف، Esc يلغي.\r\n"
  L"ختم: يطبع وجهاً مبتسماً؛ الحجم يتبع سماكة الفرشاة.\r\n\r\nالألوان\r\n"
  L"نقرة يسرى على اللوحة للون الأساسي، نقرة يمنى للون الثانوي.\r\n"
  L"نقرة مزدوجة على لوحة الألوان أو على مربع اللون يميناً لفتح لون مخصص.\r\n\r\nالعرض\r\n"
  L"Ctrl + عجلة الفأرة: تكبير. السحب بالزر الأوسط: تحريك. Ctrl+G: شبكة.\r\n"
  L"التماثل يعكس الخطوط على المحور العمودي.\r\n\r\nالاختصارات\r\n"
  L"Ctrl+S حفظ، Ctrl+O فتح، Ctrl+N جديد، Ctrl+Z تراجع، Ctrl+Y إعادة، Ctrl+C نسخ، Ctrl+V لصق.\r\n\r\n"
  L"التأثيرات\r\nقائمة التأثيرات توفر 20 مرشحاً منها الضبابية والحدة والطلاء الزيتي والبارز.\r\n"
  L"يمكن حفظ الملفات بصيغة BMP أو PNG أو JPEG أو GIF أو TIFF أو ICO.\r\n\r\nمرجع التأثيرات\r\n"
  L"تدرج رمادي: يحول الألوان إلى إضاءة. عكس: يعكس كل الألوان.\r\n"
  L"بني داكن: يعطي مظهر صورة قديمة. ضبابية: يطبق ضبابية 3x3.\r\n"
  L"حدة: يبرز الحواف. بكسلة: يختزل الصورة إلى كتل 8x8.\r\n"
  L"سطوع +/-: يفتح أو يغمق. بارز: يعطي مظهر سطح بارز.\r\n"
  L"كشف الحواف: يستخرج الخطوط العريضة. ملصق: يختزل الألوان إلى 4 مستويات.\r\n"
  L"ضوضاء: يضيف نقاطاً عشوائية. تباين وتشبع +/-: تعديلات الدرجات.\r\n"
  L"ضبابية غاوس: ضبابية ناعمة 5x5. طلاء زيتي: يعطي مظهر ضربات الفرشاة.\r\n"
  L"تعريض: يعكس الدرجات الفاتحة. تبديل القنوات: يبدل الأحمر والأزرق.\r\n\r\nأسئلة شائعة\r\n"
  L"س: كم خطوة تراجع؟ ج: 32 خطوة.\r\n"
  L"س: أين تُحفظ الإعدادات؟ ج: في ملف minipaint.ini بجانب البرنامج.\r\n"
  L"س: هل يمكنني استخدام رسوماتي تجارياً؟ ج: نعم، هي ملكك بالكامل.\r\n"
  L"س: لماذا لا تومض اللوحة؟ ج: كل الرسم يستخدم التخزين المؤقت المزدوج.\r\n",
  L"&الشيفرة المصدرية", L"الشيفرة المصدرية - MiniPaint",
  L"ألوان مائية", L"رطب", L"زيتي", L"رصاص", L"خط", L"شريط",
  L"نيون", L"ظل", L"مرجع", L"تحميل...", L"إظهار/إخفاء", L"إزالة", L"مثبت الفرشاة", L"تنقيط", L"بكسل مثالي", L"لوحة بكسل", L"قلم G", L"نقاط", L"سرعة", L"فقاعة", L"لوحة",
  L"تعذر حفظ الملف. قد يكون الموقع للقراءة فقط أو القرص ممتلئًا." },
// ---- Hintce ----
{ L"पेंसिल", L"रेखा", L"आयत", L"दीर्घवृत्त", L"भरें", L"रबड़", L"रंग चुनें",
  L"स्प्रे", L"त्रिभुज", L"गोल कोण", L"तारा", L"टेक्स्ट", L"चयन",
  L"वक्र", L"समचतुर्भुज", L"पंचभुज", L"षट्भुज", L"तीर", L"दिल",
  L"&फ़ाइल", L"&नया", L"&खोलें...", L"&सहेजें", L"इस नाम से सहेजें...", L"&बाहर निकलें",
  L"&संपादन", L"&पूर्ववत करें\tCtrl+Z", L"&फिर से करें\tCtrl+Y", L"&कैनवस साफ़ करें",
  L"&छवि", L"90° दाएं घुमाएं", L"90° बाएं घुमाएं", L"क्षैतिज पलटें", L"लंबवत पलटें", L"&कैनवस आकार...",
  L"&प्रभाव", L"ग्रेस्केल", L"निगेटिव", L"सेपिया", L"धुंधला", L"तेज़",
  L"पिक्सेल", L"चमक +", L"चमक -",
  L"उभार", L"किनारा पहचान", L"पोस्टराइज़", L"शोर",
  L"कंट्रास्ट +", L"कंट्रास्ट -", L"संतृप्ति +", L"संतृप्ति -",
  L"गाऊसी धुंधला", L"तेल चित्र", L"सोलराइज़", L"चैनल बदलें",
  L"&दृश्य", L"&ग्रिड", L"&वास्तविक आकार", L"&विंडो में फिट", L"&सममिति",
  L"&सहायता", L"&परिचय", L"&भाषा",
  L"MiniPaint - कस्टम पेंट प्रोग्राम",
  L"  उपकरण: %s   रंग: #%02X%02X%02X   आकार: %d   ज़ूम: %d%%   स्थिति: %d,%d   [Ctrl+S सहेजें, Ctrl+Z पूर्ववत]",
  L"त्रुटि", L"फ़ाइल नहीं खुल सकी या प्रारूप समर्थित नहीं है।",
  L"सहेजे न गए परिवर्तन हैं। बाहर निकलने से पहले सहेजें?",
  L"MiniPaint v2.2\n\nशुद्ध C++ और Win32 API में पूरी तरह शुरुआत से\nलिखा गया पेंट प्रोग्राम। कोई बाहरी लाइब्रेरी नहीं।\n\n"
  L"36 उपकरण, 32-चरण पूर्ववत, 20 प्रभाव, ज़ूम, सममिति,\nBMP/PNG/JPEG/GIF/TIFF/ICO समर्थन और 29 भाषाएँ।",
  L"छवि फ़ाइलें (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"ठीक", L"रद्द करें", L"चौड़ाई:", L"ऊँचाई:",
  L"कस्टम रंग चुनें", L"कैनवस आकार", L"भरा हुआ", L"गोल नोक", L"चौकोर नोक",
  L"बहुभुज", L"मसलें", L"मुहर",
  L"&उपयोगकर्ता गाइड", L"उपयोगकर्ता गाइड",
  L"MINIPAINT उपयोगकर्ता गाइड\r\n\r\nउपकरण\r\nपेंसिल (P): मुक्त हाथ से चित्रण। दायाँ बटन द्वितीयक रंग से बनाता है।\r\n"
  L"रेखा (L), आयत (R), दीर्घवृत्त (O), त्रिभुज, गोल कोण, तारा, समचतुर्भुज, पंचभुज, षट्भुज, तीर और दिल: खींचकर बनाएं। 'भरा हुआ' बटन आकृतियों को भरता है।\r\n"
  L"वक्र: सिरे खींचें, फिर नियंत्रण बिंदु पर क्लिक करें। Esc रद्द करता है।\r\n"
  L"बहुभुज: बिंदु जोड़ने के लिए क्लिक करें, बंद करने के लिए डबल क्लिक।\r\n"
  L"भरें (F): क्षेत्र भरता है। स्प्रे (S): एयरब्रश। मसलें: रंगों को गीले रंग की तरह फैलाता है।\r\n"
  L"रबड़ (E): द्वितीयक रंग तक मिटाता है। रंग चुनें (I): कैनवस से रंग लेता है।\r\n"
  L"टेक्स्ट (T): कैनवस पर क्लिक करें और टाइप करें; लागू करने के लिए elsewhere क्लिक करें।\r\n"
  L"चयन (M): आयत खींचें और सामग्री को खींचकर ले जाएं। Delete मिटाता है, Esc रद्द करता है।\r\n"
  L"मुहर: स्माइली लगाता है; आकार ब्रश की मोटाई पर निर्भर करता है।\r\n\r\nरंग\r\n"
  L"पैलेट पर बायाँ क्लिक मुख्य रंग, दायाँ क्लिक द्वितीयक रंग चुनता है।\r\n"
  L"पैलेट पर डबल क्लिक या दाईं ओर रंग बॉक्स पर क्लिक करने से कस्टम रंग खुलता है।\r\n\r\nदृश्य\r\n"
  L"Ctrl + माउस व्हील: ज़ूम। मध्य बटन से खींचें: स्थानांतरण। Ctrl+G: ग्रिड।\r\n"
  L"सममिति: ऊर्ध्वाधर अक्ष पर स्ट्रोक को प्रतिबिंबित करती है।\r\n\r\nशॉर्टकट\r\n"
  L"Ctrl+S सहेजें, Ctrl+O खोलें, Ctrl+N नया, Ctrl+Z पूर्ववत, Ctrl+Y फिर से, Ctrl+C कॉपी, Ctrl+V पेस्ट।\r\n\r\n"
  L"प्रभाव\r\nप्रभाव मेनू में धुंधला, तेज़, तेल चित्र, उभार सहित 20 फ़िल्टर हैं।\r\n"
  L"फ़ाइलें BMP, PNG, JPEG, GIF, TIFF या ICO के रूप में सहेजी जा सकती हैं।\r\n\r\nप्रभाव संदर्भ\r\n"
  L"ग्रेस्केल: रंगों को चमक में बदलता है। निगेटिव: सभी रंग उलटता है।\r\n"
  L"सेपिया: पुरानी तस्वीर जैसा दिखाता है। धुंधला: 3x3 बॉक्स ब्लर लगाता है।\r\n"
  L"तेज़: किनारों को उभारता है। पिक्सेल: छवि को 8x8 ब्लॉक में बदलता है।\r\n"
  L"चमक +/-: हल्का या गहरा करता है। उभार: उभरी सतह जैसा प्रभाव।\r\n"
  L"किनारा पहचान: रूपरेखा निकालता है। पोस्टराइज़: रंगों को 4 स्तरों में घटाता है।\r\n"
  L"शोर: यादृच्छिक बिंदु जोड़ता है। कंट्रास्ट और संतृप्ति +/-: टोन समायोजन।\r\n"
  L"गाऊसी धुंधला: नरम 5x5 ब्लर। तेल चित्र: ब्रश स्ट्रोक जैसा दिखाता है।\r\n"
  L"सोलराइज़: हल्के टोन उलटता है। चैनल बदलें: लाल और नीला बदलता है।\r\n\r\nअक्सर पूछे प्रश्न\r\n"
  L"प्र: पूर्ववत कितने चरण? उ: 32 चरण।\r\n"
  L"प्र: सेटिंग्स कहाँ सहेजी जाती हैं? उ: प्रोग्राम के पास minipaint.ini में।\r\n"
  L"प्र: क्या मैं अपने चित्रों का व्यावसायिक उपयोग कर सकता हूँ? उ: हाँ, वे पूरी तरह आपके हैं।\r\n"
  L"प्र: कैनवस क्यों नहीं झिलमिलाता? उ: सभी चित्रण डबल बफरिंग का उपयोग करता है।\r\n",
  L"&स्रोत कोड", L"स्रोत कोड - MiniPaint",
  L"जलरंग", L"गीला", L"तेल", L"पेंसिल", L"सुलेख", L"रिबन",
  L"नियॉन", L"छाया", L"संदर्भ", L"लोड करें...", L"दिखाएँ/छिपाएँ", L"हटाएँ", L"स्टेबलाइज़र", L"डिथर", L"परफेक्ट पिक्सेल", L"पिक्सेल पैलेट", L"जी-पेन", L"टोन", L"गति", L"बबल", L"पैनल",
  L"फ़ाइल सहेजी नहीं जा सकी। हो सकता है स्थान केवल-पढ़ने योग्य हो या डिस्क भरी हो।" },
// ---- Endonezce ----
{ L"Pensil", L"Garis", L"Kotak", L"Elips", L"Isi", L"Penghapus", L"Ambil Warna",
  L"Semprot", L"Segitiga", L"Bulat", L"Bintang", L"Teks", L"Pilih",
  L"Kurva", L"Wajik", L"Segilima", L"Segienam", L"Panah", L"Hati",
  L"&Berkas", L"&Baru", L"&Buka...", L"&Simpan", L"Simpan Sebaga&i...", L"&Keluar",
  L"&Sunting", L"&Urungkan\tCtrl+Z", L"&Ulangi\tCtrl+Y", L"&Bersihkan Kanvas",
  L"&Gambar", L"Putar 90° Searah Jam", L"Putar 90° Berlawanan", L"Balik Horizontal", L"Balik Vertikal", L"&Ubah Ukuran Kanvas...",
  L"&Efek", L"Skala Abu", L"Negatif", L"Sepia", L"Buram", L"Tajam",
  L"Piksel", L"Kecerahan +", L"Kecerahan -",
  L"Timbul", L"Deteksi Tepi", L"Poster", L"Tambah Bintik",
  L"Kontras +", L"Kontras -", L"Saturasi +", L"Saturasi -",
  L"Buram Gauss", L"Cat Minyak", L"Solar", L"Tukar Kanal",
  L"&Tampilan", L"&Kisi", L"&Ukuran Asli (100%)", L"&Muat di Jendela", L"S&imetri",
  L"&Bantuan", L"&Tentang", L"&Bahasa",
  L"MiniPaint - Program Gambar Kustom",
  L"  Alat: %s   Warna: #%02X%02X%02X   Ukuran: %d   Zoom: %d%%   Posisi: %d,%d   [Ctrl+S Simpan, Ctrl+Z Urungkan]",
  L"Kesalahan", L"Berkas tidak dapat dibuka atau format tidak didukung.",
  L"Ada perubahan yang belum disimpan. Simpan sebelum keluar?",
  L"MiniPaint v2.2\n\nProgram gambar yang ditulis sepenuhnya dari awal\ndengan C++ murni dan Win32 API.\nTanpa pustaka eksternal.\n\n"
  L"36 alat, urungkan 32 langkah, 20 efek, zoom, simetri,\ndukungan BMP/PNG/JPEG/GIF/TIFF/ICO dan 29 bahasa.",
  L"Berkas Gambar (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Batal", L"Lebar:", L"Tinggi:",
  L"Pilih Warna Kustom", L"Ukuran Kanvas", L"Terisi", L"Ujung Bulat", L"Ujung Kotak",
  L"Poligon", L"Lumur", L"Stempel",
  L"&Panduan Pengguna", L"Panduan Pengguna",
  L"PANDUAN PENGGUNA MINIPAINT\r\n\r\nALAT\r\nPensil (P): menggambar bebas. Klik kanan menggambar dengan warna sekunder.\r\n"
  L"Garis (L), Kotak (R), Elips (O), Segitiga, Bulat, Bintang, Wajik, Segilima, Segienam, Panah dan Hati: seret untuk menggambar. Tombol 'Terisi' mengisi bentuk.\r\n"
  L"Kurva: seret kedua ujungnya, lalu klik untuk menempatkan titik kontrol. Esc membatalkan.\r\n"
  L"Poligon: klik untuk menambah titik, klik ganda untuk menutup bentuk.\r\n"
  L"Isi (F): mewarnai area. Semprot (S): cat semprot. Lumur: menyeret warna seperti cat basah.\r\n"
  L"Penghapus (E): menghapus ke warna sekunder. Ambil Warna (I): mengambil contoh warna dari kanvas.\r\n"
  L"Teks (T): klik kanvas lalu ketik; klik di tempat lain untuk menerapkan.\r\n"
  L"Pilih (M): seret persegi, seret isinya untuk memindahkan. Delete menghapus, Esc membatalkan.\r\n"
  L"Stempel: mencetak wajah tersenyum; ukurannya tergantung ketebalan kuas.\r\n\r\nWARNA\r\n"
  L"Klik kiri pada palet memilih warna utama, klik kanan warna sekunder.\r\n"
  L"Klik ganda pada palet atau kotak warna di kanan membuka pemilih warna kustom.\r\n\r\nTAMPILAN\r\n"
  L"Ctrl + roda mouse: zoom. Seret tombol tengah: menggeser. Ctrl+G: kisi.\r\n"
  L"Simetri: mencerminkan goresan kuas pada sumbu vertikal.\r\n\r\nPINTASAN\r\n"
  L"Ctrl+S simpan, Ctrl+O buka, Ctrl+N baru, Ctrl+Z urungkan, Ctrl+Y ulangi, Ctrl+C salin, Ctrl+V tempel.\r\n\r\n"
  L"EFEK\r\nMenu Efek berisi 20 filter termasuk skala abu, negatif, sepia, buram, tajam, timbul dan cat minyak.\r\n"
  L"Berkas dapat disimpan sebagai BMP, PNG, JPEG, GIF, TIFF atau ICO.\r\n\r\nREFERENSI EFEK\r\n"
  L"Skala Abu: mengubah warna menjadi kecerahan. Negatif: membalik semua warna.\r\n"
  L"Sepia: memberi tampilan foto lama. Buram: menerapkan buram kotak 3x3.\r\n"
  L"Tajam: mempertajam tepi. Piksel: mereduksi gambar menjadi blok 8x8.\r\n"
  L"Kecerahan +/-: mencerahkan atau menggelapkan. Timbul: kesan permukaan timbul.\r\n"
  L"Deteksi Tepi: mengekstrak garis tepi. Poster: mereduksi warna ke 4 tingkat.\r\n"
  L"Tambah Bintik: menambah titik acak. Kontras dan Saturasi +/-: penyesuaian nada.\r\n"
  L"Buram Gauss: buram lembut 5x5. Cat Minyak: tampilan sapuan kuas.\r\n"
  L"Solar: membalik nada terang. Tukar Kanal: menukar saluran merah dan biru.\r\n\r\nTANYA JAWAB\r\n"
  L"T: Berapa langkah urungkan? J: 32 langkah.\r\n"
  L"T: Di mana pengaturan disimpan? J: Di berkas minipaint.ini di samping program.\r\n"
  L"T: Bolehkah gambar saya dipakai komersial? J: Ya, gambar Anda sepenuhnya milik Anda.\r\n"
  L"T: Mengapa kanvas tidak berkedip? J: Semua penggambaran memakai buffering ganda.\r\n",
  L"Kode &Sumber", L"Kode Sumber - MiniPaint",
  L"Cat Air", L"Basah", L"Minyak", L"Pensil", L"Kaligraf", L"Pita",
  L"Neon", L"Bayangan", L"Referensi", L"Muat...", L"Tampil/Sembunyi", L"Hapus", L"Stabilizer", L"Dither", L"Piksel sempurna", L"Palet piksel", L"G-Pen", L"Tone", L"Cepat", L"Balon", L"Panel",
  L"Tidak dapat menyimpan berkas. Lokasi mungkin hanya-baca atau diska penuh." },
// ---- Vietnamca ----
{ L"Bút chì", L"Đường", L"Hộp", L"Elip", L"Tô", L"Tẩy", L"Lấy màu",
  L"Phun", L"Tam giác", L"Bo tròn", L"Ngôi sao", L"Văn bản", L"Chọn",
  L"Đường cong", L"Hình thoi", L"Ngũ giác", L"Lục giác", L"Mũi tên", L"Trái tim",
  L"&Tệp", L"&Mới", L"&Mở...", L"&Lưu", L"Lưu thành...", L"&Thoát",
  L"&Chỉnh sửa", L"&Hoàn tác\tCtrl+Z", L"&Làm lại\tCtrl+Y", L"&Xóa khung vẽ",
  L"&Hình ảnh", L"Xoay 90° theo chiều kim", L"Xoay 90° ngược chiều", L"Lật ngang", L"Lật dọc", L"&Đổi kích thước khung vẽ...",
  L"&Hiệu ứng", L"Thang xám", L"Âm bản", L"Nâu đỏ", L"Làm mờ", L"Làm nét",
  L"Pixel hóa", L"Độ sáng +", L"Độ sáng -",
  L"Nổi", L"Tìm cạnh", L"Áp phích", L"Thêm nhiễu",
  L"Tương phản +", L"Tương phản -", L"Độ bão hòa +", L"Độ bão hòa -",
  L"Mờ Gauss", L"Sơn dầu", L"Solar hóa", L"Đổi kênh",
  L"&Hiển thị", L"&Lưới", L"&Kích thước thật (100%)", L"&Vừa cửa sổ", L"Đố&i xứng",
  L"&Trợ giúp", L"&Giới thiệu", L"&Ngôn ngữ",
  L"MiniPaint - Chương trình vẽ tùy chỉnh",
  L"  Công cụ: %s   Màu: #%02X%02X%02X   Cỡ: %d   Thu phóng: %d%%   Vị trí: %d,%d   [Ctrl+S Lưu, Ctrl+Z Hoàn tác]",
  L"Lỗi", L"Không thể mở tệp hoặc định dạng không được hỗ trợ.",
  L"Có thay đổi chưa lưu. Bạn có muốn lưu trước khi thoát không?",
  L"MiniPaint v2.2\n\nChương trình vẽ được viết hoàn toàn từ đầu\nbằng C++ thuần và Win32 API.\nKhông dùng thư viện ngoài.\n\n"
  L"36 công cụ, hoàn tác 32 bước, 20 hiệu ứng, thu phóng, đối xứng,\nhỗ trợ BMP/PNG/JPEG/GIF/TIFF/ICO và 29 ngôn ngữ.",
  L"Tệp hình ảnh (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Hủy", L"Chiều rộng:", L"Chiều cao:",
  L"Chọn màu tùy chỉnh", L"Kích thước khung vẽ", L"Tô đầy", L"Đầu tròn", L"Đầu vuông",
  L"Đa giác", L"Nhòe", L"Con dấu",
  L"&Hướng dẫn sử dụng", L"Hướng dẫn sử dụng",
  L"HƯỚNG DẪN SỬ DỤNG MINIPAINT\r\n\r\nCÔNG CỤ\r\nBút chì (P): vẽ tự do. Chuột phải vẽ bằng màu phụ.\r\n"
  L"Đường (L), Hộp (R), Elip (O), Tam giác, Bo tròn, Ngôi sao, Hình thoi, Ngũ giác, Lục giác, Mũi tên và Trái tim: kéo để vẽ. Nút 'Tô đầy' tô kín hình.\r\n"
  L"Đường cong: kéo hai đầu, rồi nhấp để đặt điểm điều khiển. Esc để hủy.\r\n"
  L"Đa giác: nhấp để thêm điểm, nhấp đúp để đóng hình.\r\n"
  L"Tô (F): tô vùng. Phun (S): sơn phun. Nhòe: kéo màu như sơn ướt.\r\n"
  L"Tẩy (E): xóa về màu phụ. Lấy màu (I): lấy mẫu màu từ khung vẽ.\r\n"
  L"Văn bản (T): nhấp vào khung vẽ rồi gõ; nhấp chỗ khác để áp dụng.\r\n"
  L"Chọn (M): kéo hình chữ nhật, kéo bên trong để di chuyển. Delete xóa, Esc hủy.\r\n"
  L"Con dấu: in mặt cười; kích thước phụ thuộc độ dày cọ.\r\n\r\nMÀU SẮC\r\n"
  L"Nhấp trái trên bảng màu chọn màu chính, nhấp phải chọn màu phụ.\r\n"
  L"Nhấp đúp vào bảng màu hoặc hộp màu bên phải để mở bộ chọn màu tùy chỉnh.\r\n\r\nHIỂN THỊ\r\n"
  L"Ctrl + con lăn: thu phóng. Kéo nút giữa: cuộn. Ctrl+G: lưới.\r\n"
  L"Đối xứng: phản chiếu nét vẽ qua trục dọc.\r\n\r\nPHÍM TẮT\r\n"
  L"Ctrl+S lưu, Ctrl+O mở, Ctrl+N mới, Ctrl+Z hoàn tác, Ctrl+Y làm lại, Ctrl+C sao chép, Ctrl+V dán.\r\n\r\n"
  L"HIỆU ỨNG\r\nMenu Hiệu ứng có 20 bộ lọc gồm thang xám, âm bản, nâu đỏ, mờ, nét, nổi và sơn dầu.\r\n"
  L"Tệp có thể lưu dạng BMP, PNG, JPEG, GIF, TIFF hoặc ICO.\r\n\r\nTHAM CHIẾU HIỆU ỨNG\r\n"
  L"Thang xám: chuyển màu thành độ sáng. Âm bản: đảo mọi màu.\r\n"
  L"Nâu đỏ: tạo vẻ ảnh cũ. Làm mờ: áp dụng mờ hộp 3x3.\r\n"
  L"Làm nét: tăng độ nét cạnh. Pixel hóa: giảm ảnh thành khối 8x8.\r\n"
  L"Độ sáng +/-: làm sáng hoặc tối. Nổi: tạo cảm giác bề mặt nổi.\r\n"
  L"Tìm cạnh: trích xuất đường viền. Áp phích: giảm màu còn 4 mức.\r\n"
  L"Thêm nhiễu: thêm chấm ngẫu nhiên. Tương phản và Độ bão hòa +/-: chỉnh tông.\r\n"
  L"Mờ Gauss: mờ mềm 5x5. Sơn dầu: tạo vẻ nét cọ.\r\n"
  L"Solar hóa: đảo tông sáng. Đổi kênh: đổi kênh đỏ và xanh dương.\r\n\r\nHỎI ĐÁP\r\n"
  L"H: Hoàn tác bao nhiêu bước? Đ: 32 bước.\r\n"
  L"H: Cài đặt lưu ở đâu? Đ: Trong tệp minipaint.ini cạnh chương trình.\r\n"
  L"H: Tôi có thể dùng tranh của mình cho mục đích thương mại không? Đ: Có, tranh hoàn toàn của bạn.\r\n"
  L"H: Tại sao khung vẽ không nhấp nháy? Đ: Mọi thao tác vẽ đều dùng bộ đệm kép.\r\n",
  L"Mã &nguồn", L"Mã nguồn - MiniPaint",
  L"Màu nước", L"Ướt", L"Dầu", L"Bút chì", L"Thư pháp", L"Ruy băng",
  L"Neon", L"Bóng", L"Tham chiếu", L"Tải...", L"Hiện/Ẩn", L"Xóa", L"Ổn định nét", L"Dither", L"Pixel hoàn hảo", L"Bảng pixel", L"G-Pen", L"Tone", L"Tốc độ", L"Bong bóng", L"Khung",
  L"Không thể lưu tệp. Vị trí có thể chỉ-đọc hoặc đĩa đã đầy." },
// ---- Tayca ----
{ L"ดินสอ", L"เส้น", L"สี่เหลี่ยม", L"วงรี", L"เติมสี", L"ยางลบ", L"ดูดสี",
  L"สเปรย์", L"สามเหลี่ยม", L"มุมมน", L"ดาว", L"ข้อความ", L"เลือก",
  L"เส้นโค้ง", L"ข้าวหลามตัด", L"ห้าเหลี่ยม", L"หกเหลี่ยม", L"ลูกศร", L"หัวใจ",
  L"&ไฟล์", L"&ใหม่", L"&เปิด...", L"&บันทึก", L"บันทึกเป็น...", L"&ออก",
  L"&แก้ไข", L"&เลิกทำ\tCtrl+Z", L"&ทำซ้ำ\tCtrl+Y", L"&ล้างผืนผ้าใบ",
  L"&รูปภาพ", L"หมุน 90° ตามเข็มนาฬิกา", L"หมุน 90° ทวนเข็มนาฬิกา", L"พลิกแนวนอน", L"พลิกแนวตั้ง", L"&ปรับขนาดผืนผ้าใบ...",
  L"&เอฟเฟกต์", L"โทนสีเทา", L"ภาพเนกาทีฟ", L"ซีเปีย", L"เบลอ", L"คมชัด",
  L"พิกเซล", L"ความสว่าง +", L"ความสว่าง -",
  L"นูน", L"ตรวจขอบ", L"โปสเตอร์", L"เพิ่มสัญญาณรบกวน",
  L"คอนทราสต์ +", L"คอนทราสต์ -", L"ความอิ่มตัว +", L"ความอิ่มตัว -",
  L"เบลอเกาส์เซียน", L"สีน้ำมัน", L"โซลาไรซ์", L"สลับช่องสี",
  L"&มุมมอง", L"&เส้นตาราง", L"&ขนาดจริง (100%)", L"&พอดีหน้าต่าง", L"&สมมาตร",
  L"&ช่วยเหลือ", L"&เกี่ยวกับ", L"&ภาษา",
  L"MiniPaint - โปรแกรมวาดภาพ",
  L"  เครื่องมือ: %s   สี: #%02X%02X%02X   ขนาด: %d   ซูม: %d%%   ตำแหน่ง: %d,%d   [Ctrl+S บันทึก, Ctrl+Z เลิกทำ]",
  L"ข้อผิดพลาด", L"ไม่สามารถเปิดไฟล์หรือรูปแบบไม่รองรับ",
  L"มีการเปลี่ยนแปลงที่ยังไม่ได้บันทึก ต้องการบันทึกก่อนออกหรือไม่?",
  L"MiniPaint v2.2\n\nโปรแกรมวาดภาพที่เขียนขึ้นใหม่ทั้งหมด\nด้วย C++ ล้วนและ Win32 API\nไม่มีไลบรารีภายนอก\n\n"
  L"มี 36 เครื่องมือ เลิกทำ 32 ขั้น 20 เอฟเฟกต์ ซูม สมมาตร\nรองรับ BMP/PNG/JPEG/GIF/TIFF/ICO และ 29 ภาษา",
  L"ไฟล์รูปภาพ (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"ตกลง", L"ยกเลิก", L"ความกว้าง:", L"ความสูง:",
  L"เลือกสีแบบกำหนดเอง", L"ขนาดผืนผ้าใบ", L"เติมเต็ม", L"ปลายกลม", L"ปลายเหลี่ยม",
  L"หลายเหลี่ยม", L"ลากสี", L"ตราประทับ",
  L"&คู่มือการใช้งาน", L"คู่มือการใช้งาน",
  L"คู่มือการใช้งาน MINIPAINT\r\n\r\nเครื่องมือ\r\nดินสอ (P): วาดอิสระ คลิกขวาวาดด้วยสีรอง\r\n"
  L"เส้น (L), สี่เหลี่ยม (R), วงรี (O), สามเหลี่ยม, มุมมน, ดาว, ข้าวหลามตัด, ห้าเหลี่ยม, หกเหลี่ยม, ลูกศร และหัวใจ: ลากเพื่อวาด ปุ่ม 'เติมเต็ม' เติมสีในรูปทรง\r\n"
  L"เส้นโค้ง: ลากสองปลาย แล้วคลิกเพื่อวางจุดควบคุม Esc ยกเลิก\r\n"
  L"หลายเหลี่ยม: คลิกเพื่อเพิ่มจุด ดับเบิลคลิกเพื่อปิดรูป\r\n"
  L"เติมสี (F): เติมพื้นที่ สเปรย์ (S): สีสเปรย์ ลากสี: ลากสีเหมือนสีเปียก\r\n"
  L"ยางลบ (E): ลบเป็นสีรอง ดูดสี (I): เก็บตัวอย่างสีจากผืนผ้าใบ\r\n"
  L"ข้อความ (T): คลิกบนผืนผ้าใบแล้วพิมพ์ คลิกที่อื่นเพื่อใช้งาน\r\n"
  L"เลือก (M): ลากสี่เหลี่ยม ลากข้างในเพื่อย้าย Delete ลบ Esc ยกเลิก\r\n"
  L"ตราประทับ: พิมพ์หน้ายิ้ม ขนาดขึ้นกับความหนาของแปรง\r\n\r\nสี\r\n"
  L"คลิกซ้ายบนพาเลตเลือกสีหลัก คลิกขวาเลือกสีรอง\r\n"
  L"ดับเบิลคลิกพาเลตหรือกล่องสีด้านขวาเพื่อเปิดตัวเลือกสีแบบกำหนดเอง\r\n\r\nมุมมอง\r\n"
  L"Ctrl + ล้อเมาส์: ซูม ลากปุ่มกลาง: เลื่อน Ctrl+G: เส้นตาราง\r\n"
  L"สมมาตร: สะท้อนเส้นแปรงบนแกนตั้ง\r\n\r\nแป้นพิมพ์ลัด\r\n"
  L"Ctrl+S บันทึก, Ctrl+O เปิด, Ctrl+N ใหม่, Ctrl+Z เลิกทำ, Ctrl+Y ทำซ้ำ, Ctrl+C คัดลอก, Ctrl+V วาง\r\n\r\n"
  L"เอฟเฟกต์\r\nเมนูเอฟเฟกต์มี 20 ตัวกรอง เช่น โทนสีเทา เนกาทีฟ ซีเปีย เบลอ คมชัด นูน และสีน้ำมัน\r\n"
  L"ไฟล์สามารถบันทึกเป็น BMP, PNG, JPEG, GIF, TIFF หรือ ICO\r\n\r\nข้อมูลอ้างอิงเอฟเฟกต์\r\n"
  L"โทนสีเทา: แปลงสีเป็นความสว่าง ภาพเนกาทีฟ: กลับทุกสี\r\n"
  L"ซีเปีย: ให้ลุคภาพเก่า เบลอ: ใช้เบลอแบบกล่อง 3x3\r\n"
  L"คมชัด: เน้นขอบ พิกเซล: ลดภาพเป็นบล็อก 8x8\r\n"
  L"ความสว่าง +/-: สว่างหรือมืดขึ้น นูน: ให้ความรู้สึกพื้นผิวนูน\r\n"
  L"ตรวจขอบ: สกัดเส้นขอบ โปสเตอร์: ลดสีเหลือ 4 ระดับ\r\n"
  L"เพิ่มสัญญาณรบกวน: เพิ่มจุดสุ่ม คอนทราสต์และความอิ่มตัว +/-: ปรับโทน\r\n"
  L"เบลอเกาส์เซียน: เบลอนุ่ม 5x5 สีน้ำมัน: ให้ลุคฝีแปรง\r\n"
  L"โซลาไรซ์: กลับโทนสว่าง สลับช่องสี: สลับช่องแดงและน้ำเงิน\r\n\r\nคำถามที่พบบ่อย\r\n"
  L"ถาม: เลิกทำได้กี่ขั้น? ตอบ: 32 ขั้น\r\n"
  L"ถาม: การตั้งค่าบันทึกที่ไหน? ตอบ: ในไฟล์ minipaint.ini ข้างโปรแกรม\r\n"
  L"ถาม: ใช้ภาพของฉันเชิงพาณิชย์ได้ไหม? ตอบ: ได้ ภาพเป็นของคุณทั้งหมด\r\n"
  L"ถาม: ทำไมผืนผ้าใบไม่กะพริบ? ตอบ: การวาดทั้งหมดใช้ดับเบิลบัฟเฟอร์\r\n",
  L"&ซอร์สโค้ด", L"ซอร์สโค้ด - MiniPaint",
  L"สีน้ำ", L"เปียก", L"สีน้ำมัน", L"ดินสอ", L"คาลิกราฟี", L"ริบบิ้น",
  L"นีออน", L"เงา", L"ภาพอ้างอิง", L"โหลด...", L"แสดง/ซ่อน", L"ลบ", L"กันสั่น", L"ดิทเทอร์", L"พิกเซลสมบูรณ์", L"จานสีพิกเซล", L"G-Pen", L"โทน", L"เร็ว", L"คำพูด", L"พาเนล",
  L"ไม่สามารถบันทึกไฟล์ได้ ตำแหน่งอาจเป็นแบบอ่านอย่างเดียวหรือดิสก์เต็ม" },
// ---- Yunanca ----
{ L"Μολύβι", L"Γραμμή", L"Πλαίσιο", L"Έλλειψη", L"Γέμισμα", L"Γόμα", L"Ανάληψη",
  L"Σπρέι", L"Τρίγωνο", L"Στρογγυλό", L"Αστέρι", L"Κείμενο", L"Επιλογή",
  L"Καμπύλη", L"Ρόμβος", L"Πεντάγωνο", L"Εξάγωνο", L"Βέλος", L"Καρδιά",
  L"&Αρχείο", L"&Νέο", L"&Άνοιγμα...", L"&Αποθήκευση", L"Αποθήκευση &ως...", L"&Έξοδος",
  L"&Επεξεργασία", L"&Αναίρεση\tCtrl+Z", L"&Επανάληψη\tCtrl+Y", L"&Καθαρισμός καμβά",
  L"&Εικόνα", L"Περιστροφή 90° δεξιά", L"Περιστροφή 90° αριστερά", L"Οριζόντια αναστροφή", L"Κατακόρυφη αναστροφή", L"&Αλλαγή μεγέθους καμβά...",
  L"&Εφέ", L"Ασπρόμαυρο", L"Αρνητικό", L"Σέπια", L"Θόλωση", L"Όξυνση",
  L"Εικονοστοιχεία", L"Φωτεινότητα +", L"Φωτεινότητα -",
  L"Ανάγλυφο", L"Ανίχνευση ακμών", L"Πόστερ", L"Προσθήκη θορύβου",
  L"Αντίθεση +", L"Αντίθεση -", L"Κορεσμός +", L"Κορεσμός -",
  L"Θόλωση Gauss", L"Ελαιογραφία", L"Ηλιακή έκθεση", L"Εναλλαγή καναλιών",
  L"&Προβολή", L"&Πλέγμα", L"&Πραγματικό μέγεθος (100%)", L"&Προσαρμογή στο παράθυρο", L"Σ&υμμετρία",
  L"&Βοήθεια", L"&Σχετικά", L"&Γλώσσα",
  L"MiniPaint - Πρόγραμμα ζωγραφικής",
  L"  Εργαλείο: %s   Χρώμα: #%02X%02X%02X   Μέγεθος: %d   Ζουμ: %d%%   Θέση: %d,%d   [Ctrl+S Αποθήκευση, Ctrl+Z Αναίρεση]",
  L"Σφάλμα", L"Δεν ήταν δυνατό το άνοιγμα του αρχείου ή η μορφή δεν υποστηρίζεται.",
  L"Υπάρχουν μη αποθηκευμένες αλλαγές. Να αποθηκευτούν πριν από την έξοδο;",
  L"MiniPaint v2.2\n\nΠρόγραμμα ζωγραφικής γραμμένο εξ ολοκλήρου από την αρχή\nσε καθαρή C++ και Win32 API.\nΧωρίς εξωτερικές βιβλιοθήκες.\n\n"
  L"36 εργαλεία, αναίρεση 32 βημάτων, 20 εφέ, ζουμ, συμμετρία,\nυποστήριξη BMP/PNG/JPEG/GIF/TIFF/ICO και 29 γλώσσες.",
  L"Αρχεία εικόνας (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Άκυρο", L"Πλάτος:", L"Ύψος:",
  L"Επιλογή προσαρμοσμένου χρώματος", L"Μέγεθος καμβά", L"Γεμισμένο", L"Στρογγυλή μύτη", L"Τετράγωνη μύτη",
  L"Πολύγωνο", L"Μουντζούρα", L"Σφραγίδα",
  L"&Οδηγός χρήσης", L"Οδηγός χρήσης",
  L"ΟΔΗΓΟΣ ΧΡΗΣΗΣ MINIPAINT\r\n\r\nΕΡΓΑΛΕΙΑ\r\nΜολύβι (P): ελεύθερη σχεδίαση. Δεξί κλικ σχεδιάζει με το δευτερεύον χρώμα.\r\n"
  L"Γραμμή (L), Πλαίσιο (R), Έλλειψη (O), Τρίγωνο, Στρογγυλό, Αστέρι, Ρόμβος, Πεντάγωνο, Εξάγωνο, Βέλος και Καρδιά: σύρετε για σχεδίαση. Το κουμπί 'Γεμισμένο' γεμίζει τα σχήματα.\r\n"
  L"Καμπύλη: σύρετε τα δύο άκρα, μετά κάντε κλικ για το σημείο ελέγχου. Esc ακυρώνει.\r\n"
  L"Πολύγωνο: κλικ για προσθήκη σημείων, διπλό κλικ για κλείσιμο του σχήματος.\r\n"
  L"Γέμισμα (F): γεμίζει περιοχή. Σπρέι (S): ψεκασμός χρώματος. Μουντζούρα: σέρνει τα χρώματα σαν βρεγμένο χρώμα.\r\n"
  L"Γόμα (E): σβήνει στο δευτερεύον χρώμα. Ανάληψη (I): παίρνει δείγμα χρώματος από τον καμβά.\r\n"
  L"Κείμενο (T): κλικ στον καμβά και πληκτρολογήστε· κλικ αλλού για εφαρμογή.\r\n"
  L"Επιλογή (M): σύρετε ορθογώνιο, σύρετε μέσα για μετακίνηση. Delete διαγράφει, Esc ακυρώνει.\r\n"
  L"Σφραγίδα: τυπώνει χαμόγελο· το μέγεθος εξαρτάται από το πάχος του πινέλου.\r\n\r\nΧΡΩΜΑΤΑ\r\n"
  L"Αριστερό κλικ στην παλέτα επιλέγει το κύριο χρώμα, δεξί κλικ το δευτερεύον.\r\n"
  L"Διπλό κλικ στην παλέτα ή στα πλαίσια χρώματος δεξιά ανοίγει τον επιλογέα προσαρμοσμένου χρώματος.\r\n\r\nΠΡΟΒΟΛΗ\r\n"
  L"Ctrl + ροδέλα: ζουμ. Σύρσιμο μεσαίου κουμπιού: μετατόπιση. Ctrl+G: πλέγμα.\r\n"
  L"Συμμετρία: καθρεπτίζει τις πινελιές στον κατακόρυφο άξονα.\r\n\r\nΣΥΝΤΟΜΕΥΣΕΙΣ\r\n"
  L"Ctrl+S αποθήκευση, Ctrl+O άνοιγμα, Ctrl+N νέο, Ctrl+Z αναίρεση, Ctrl+Y επανάληψη, Ctrl+C αντιγραφή, Ctrl+V επικόλληση.\r\n\r\n"
  L"ΕΦΕ\r\nΤο μενού Εφέ περιέχει 20 φίλτρα όπως ασπρόμαυρο, αρνητικό, σέπια, θόλωση, όξυνση, ανάγλυφο και ελαιογραφία.\r\n"
  L"Τα αρχεία αποθηκεύονται ως BMP, PNG, JPEG, GIF, TIFF ή ICO.\r\n\r\nΑΝΑΦΟΡΑ ΕΦΕ\r\n"
  L"Ασπρόμαυρο: μετατρέπει τα χρώματα σε φωτεινότητα. Αρνητικό: αντιστρέφει όλα τα χρώματα.\r\n"
  L"Σέπια: δίνει όψη παλιάς φωτογραφίας. Θόλωση: εφαρμόζει θόλωση πλαισίου 3x3.\r\n"
  L"Όξυνση: τονίζει τις ακμές. Εικονοστοιχεία: μειώνει την εικόνα σε μπλοκ 8x8.\r\n"
  L"Φωτεινότητα +/-: φωτίζει ή σκοτεινιάζει. Ανάγλυφο: εντύπωση ανάγλυφης επιφάνειας.\r\n"
  L"Ανίχνευση ακμών: εξάγει περιγράμματα. Πόστερ: μειώνει τα χρώματα σε 4 επίπεδα.\r\n"
  L"Προσθήκη θορύβου: προσθέτει τυχαίες κουκκίδες. Αντίθεση και Κορεσμός +/-: ρυθμίσεις τόνου.\r\n"
  L"Θόλωση Gauss: απαλή θόλωση 5x5. Ελαιογραφία: όψη πινελιάς.\r\n"
  L"Ηλιακή έκθεση: αντιστρέφει τους ανοιχτούς τόνους. Εναλλαγή καναλιών: εναλλάσσει κόκκινο και μπλε.\r\n\r\nΣΥΧΝΕΣ ΕΡΩΤΗΣΕΙΣ\r\n"
  L"Ε: Πόσα βήματα αναίρεσης; Α: 32 βήματα.\r\n"
  L"Ε: Πού αποθηκεύονται οι ρυθμίσεις; Α: Στο αρχείο minipaint.ini δίπλα στο πρόγραμμα.\r\n"
  L"Ε: Μπορώ να χρησιμοποιήσω εμπορικά τα σχέδιά μου; Α: Ναι, ανήκουν εξ ολοκλήρου σε εσάς.\r\n"
  L"Ε: Γιατί δεν τρεμοπαίζει ο καμβάς; Α: Όλη η σχεδίαση γίνεται με διπλή προσωρινή αποθήκευση.\r\n",
  L"&Πηγαίος κώδικας", L"Πηγαίος κώδικας - MiniPaint",
  L"Ακουαρέλα", L"Υγρό", L"Λάδι", L"Μολύβι", L"Καλλιγρ", L"Κορδέλα",
  L"Νέον", L"Σκιά", L"Αναφορά", L"Φόρτωση...", L"Εμφάνιση/Απόκρυψη", L"Αφαίρεση", L"Σταθεροποίηση", L"Ντίθερ", L"Τέλειο pixel", L"Παλέτα pixel", L"G-Pen", L"Τόνος", L"Ταχύτητα", L"Φούσκα", L"Πάνελ",
  L"Δεν ήταν δυνατή η αποθήκευση του αρχείου. Η τοποθεσία μπορεί να είναι μόνο για ανάγνωση ή ο δίσκος είναι γεμάτος." },
// ---- Cekce ----
{ L"Tužka", L"Čára", L"Obdélník", L"Elipsa", L"Výplň", L"Guma", L"Kapátko",
  L"Sprej", L"Trojúhelník", L"Zaoblený", L"Hvězda", L"Text", L"Výběr",
  L"Křivka", L"Kosočtverec", L"Pětiúhelník", L"Šestiúhelník", L"Šipka", L"Srdce",
  L"&Soubor", L"&Nový", L"&Otevřít...", L"&Uložit", L"Uložit j&ako...", L"&Konec",
  L"&Úpravy", L"&Zpět\tCtrl+Z", L"&Znovu\tCtrl+Y", L"&Vyčistit plátno",
  L"&Obrázek", L"Otočit 90° vpravo", L"Otočit 90° vlevo", L"Převrátit vodorovně", L"Převrátit svisle", L"&Změnit velikost plátna...",
  L"&Efekty", L"Odstíny šedi", L"Negativ", L"Sépie", L"Rozmazání", L"Zostření",
  L"Pixelace", L"Jas +", L"Jas -",
  L"Reliéf", L"Detekce hran", L"Posterizace", L"Přidat šum",
  L"Kontrast +", L"Kontrast -", L"Sytost +", L"Sytost -",
  L"Gaussovské rozmazání", L"Olejomalba", L"Solarizace", L"Prohodit kanály",
  L"&Zobrazení", L"&Mřížka", L"&Skutečná velikost (100%)", L"&Přizpůsobit oknu", L"S&ymetrie",
  L"&Nápověda", L"&O programu", L"&Jazyk",
  L"MiniPaint - Vlastní malovací program",
  L"  Nástroj: %s   Barva: #%02X%02X%02X   Velikost: %d   Přiblížení: %d%%   Pozice: %d,%d   [Ctrl+S Uložit, Ctrl+Z Zpět]",
  L"Chyba", L"Soubor nelze otevřít nebo formát není podporován.",
  L"Existují neuložené změny. Chcete před ukončením uložit?",
  L"MiniPaint v2.2\n\nMalovací program napsaný zcela od nuly\nv čistém C++ a Win32 API.\nBez externích knihoven.\n\n"
  L"36 nástrojů, vracení o 32 kroků, 20 efektů, zoom, symetrie,\npodpora BMP/PNG/JPEG/GIF/TIFF/ICO a 29 jazyků.",
  L"Soubory obrázků (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Storno", L"Šířka:", L"Výška:",
  L"Vybrat vlastní barvu", L"Velikost plátna", L"Vyplněno", L"Kulatá špička", L"Hranatá špička",
  L"Mnohoúhelník", L"Rozmazání", L"Razítko",
  L"&Uživatelská příručka", L"Uživatelská příručka",
  L"UŽIVATELSKÁ PŘÍRUČKA MINIPAINT\r\n\r\nNÁSTROJE\r\nTužka (P): volné kreslení. Pravé tlačítko kreslí sekundární barvou.\r\n"
  L"Čára (L), Obdélník (R), Elipsa (O), Trojúhelník, Zaoblený, Hvězda, Kosočtverec, Pětiúhelník, Šestiúhelník, Šipka a Srdce: táhněte pro kreslení. Tlačítko 'Vyplněno' vyplní tvary.\r\n"
  L"Křivka: táhněte konce, pak klikněte pro řídicí bod. Esc zruší.\r\n"
  L"Mnohoúhelník: kliknutím přidávejte body, dvojklik uzavře tvar.\r\n"
  L"Výplň (F): vybarví oblast. Sprej (S): stříkaná barva. Rozmazání: roztahuje barvy jako mokrou malbu.\r\n"
  L"Guma (E): maže do sekundární barvy. Kapátko (I): odebírá vzorek barvy z plátna.\r\n"
  L"Text (T): klikněte na plátno a pište; kliknutí jinde použije text.\r\n"
  L"Výběr (M): táhněte obdélník, uvnitř táhněte pro přesun. Delete smaže, Esc zruší.\r\n"
  L"Razítko: otiskne smajlík; velikost závisí na tloušťce štětce.\r\n\r\nBARVY\r\n"
  L"Levé kliknutí na paletu vybere hlavní barvu, pravé kliknutí sekundární.\r\n"
  L"Dvojklik na paletu nebo barevná políčka vpravo otevře výběr vlastní barvy.\r\n\r\nZOBRAZENÍ\r\n"
  L"Ctrl + kolečko myši: zoom. Tažení prostředním tlačítkem: posun. Ctrl+G: mřížka.\r\n"
  L"Symetrie: zrcadlí tahy štětce ve svislé ose.\r\n\r\nZKRATKY\r\n"
  L"Ctrl+S uložit, Ctrl+O otevřít, Ctrl+N nový, Ctrl+Z zpět, Ctrl+Y znovu, Ctrl+C kopírovat, Ctrl+V vložit.\r\n\r\n"
  L"EFEKTY\r\nNabídka Efekty obsahuje 20 filtrů včetně odstínů šedi, negativu, sépie, rozmazání, zostření, reliéfu a olejomalby.\r\n"
  L"Soubory lze uložit jako BMP, PNG, JPEG, GIF, TIFF nebo ICO.\r\n\r\nREFERENCE EFEKTŮ\r\n"
  L"Odstíny šedi: převádí barvy na jas. Negativ: invertuje všechny barvy.\r\n"
  L"Sépie: vzhled staré fotografie. Rozmazání: aplikuje rámové rozmazání 3x3.\r\n"
  L"Zostření: zvýrazňuje hrany. Pixelace: redukuje obrázek na bloky 8x8.\r\n"
  L"Jas +/-: zesvětluje nebo ztmavuje. Reliéf: dojem reliéfního povrchu.\r\n"
  L"Detekce hran: extrahuje obrysy. Posterizace: redukuje barvy na 4 úrovně.\r\n"
  L"Přidat šum: přidává náhodné tečky. Kontrast a Sytost +/-: úpravy tónu.\r\n"
  L"Gaussovské rozmazání: jemné rozmazání 5x5. Olejomalba: vzhled tahů štětcem.\r\n"
  L"Solarizace: invertuje světlé tóny. Prohodit kanály: vymění červený a modrý kanál.\r\n\r\nČASTÉ DOTAZY\r\n"
  L"Ot: Kolik kroků zpět? Od: 32 kroků.\r\n"
  L"Ot: Kde se ukládají nastavení? Od: V souboru minipaint.ini vedle programu.\r\n"
  L"Ot: Mohu své kresby používat komerčně? Od: Ano, kresby jsou zcela vaše.\r\n"
  L"Ot: Proč plátno nebliká? Od: Veškeré kreslení používá dvojitou vyrovnávací paměť.\r\n",
  L"&Zdrojový kód", L"Zdrojový kód - MiniPaint",
  L"Akvarel", L"Mokrý", L"Olej", L"Tužka", L"Kaligr", L"Stuha",
  L"Neon", L"Stín", L"Reference", L"Načíst...", L"Zobrazit/skrýt", L"Odebrat", L"Stabilizace", L"Rozptyl", L"Dokonalý pixel", L"Pixelová paleta", L"G-Pen", L"Rastr", L"Rychlost", L"Bublina", L"Panel",
  L"Soubor se nepodařilo uložit. Umístění může být jen pro čtení nebo je disk plný." },
// ---- Isvecce ----
{ L"Penna", L"Linje", L"Ruta", L"Ellips", L"Fyllning", L"Suddgummi", L"Pipett",
  L"Spray", L"Triangel", L"Rundad", L"Stjärna", L"Text", L"Markering",
  L"Kurva", L"Romb", L"Femhörning", L"Sexhörning", L"Pil", L"Hjärta",
  L"&Arkiv", L"&Nytt", L"&Öppna...", L"&Spara", L"Spara s&om...", L"&Avsluta",
  L"&Redigera", L"&Ångra\tCtrl+Z", L"&Gör om\tCtrl+Y", L"&Rensa duk",
  L"&Bild", L"Rotera 90° medurs", L"Rotera 90° moturs", L"Spegla horisontellt", L"Spegla vertikalt", L"&Ändra dukstorlek...",
  L"&Effekter", L"Gråskala", L"Negativ", L"Sepia", L"Oskärpa", L"Skärpa",
  L"Pixla", L"Ljusstyrka +", L"Ljusstyrka -",
  L"Relief", L"Kantdetektion", L"Posterisera", L"Lägg till brus",
  L"Kontrast +", L"Kontrast -", L"Mättnad +", L"Mättnad -",
  L"Gaussisk oskärpa", L"Oljemålning", L"Solarisation", L"Byt kanaler",
  L"&Visa", L"&Rutnät", L"&Verklig storlek (100%)", L"&Anpassa till fönster", L"S&ymmetri",
  L"&Hjälp", L"&Om", L"&Språk",
  L"MiniPaint - Eget ritprogram",
  L"  Verktyg: %s   Färg: #%02X%02X%02X   Storlek: %d   Zoom: %d%%   Position: %d,%d   [Ctrl+S Spara, Ctrl+Z Ångra]",
  L"Fel", L"Kunde inte öppna filen eller formatet stöds inte.",
  L"Det finns osparade ändringar. Vill du spara innan du avslutar?",
  L"MiniPaint v2.2\n\nEtt ritprogram skrivet helt från grunden\ni ren C++ och Win32 API.\nInga externa bibliotek.\n\n"
  L"36 verktyg, 32-stegs ångra, 20 effekter, zoom, symmetri,\nBMP/PNG/JPEG/GIF/TIFF/ICO-stöd och 29 språk.",
  L"Bildfiler (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Avbryt", L"Bredd:", L"Höjd:",
  L"Välj anpassad färg", L"Dukstorlek", L"Fylld", L"Rund spets", L"Fyrkantig spets",
  L"Polygon", L"Smeta", L"Stämpel",
  L"&Användarhandbok", L"Användarhandbok",
  L"ANVÄNDARHANDBOK MINIPAINT\r\n\r\nVERKTYG\r\nPenna (P): frihandsritning. Högerklick ritar med sekundärfärgen.\r\n"
  L"Linje (L), Ruta (R), Ellips (O), Triangel, Rundad, Stjärna, Romb, Femhörning, Sexhörning, Pil och Hjärta: dra för att rita. Knappen 'Fylld' fyller former.\r\n"
  L"Kurva: dra ändpunkterna, klicka sedan för att placera kontrollpunkten. Esc avbryter.\r\n"
  L"Polygon: klicka för att lägga till punkter, dubbelklicka för att sluta formen.\r\n"
  L"Fyllning (F): fyller ett område. Spray (S): sprayfärg. Smeta: drar färger som våt färg.\r\n"
  L"Suddgummi (E): suddar till sekundärfärgen. Pipett (I): tar färgprov från duken.\r\n"
  L"Text (T): klicka på duken och skriv; klicka någon annanstans för att tillämpa.\r\n"
  L"Markering (M): dra en rektangel, dra inuti för att flytta. Delete raderar, Esc avbryter.\r\n"
  L"Stämpel: trycker ett leende ansikte; storleken beror på penselns tjocklek.\r\n\r\nFÄRGER\r\n"
  L"Vänsterklick på paletten väljer primärfärg, högerklick sekundärfärg.\r\n"
  L"Dubbelklick på paletten eller färgrutorna till höger öppnar den anpassade färgväljaren.\r\n\r\nVISNING\r\n"
  L"Ctrl + mushjul: zoom. Dra med mittknappen: panorera. Ctrl+G: rutnät.\r\n"
  L"Symmetri: speglar penseldragen i den vertikala axeln.\r\n\r\nGENVÄGAR\r\n"
  L"Ctrl+S spara, Ctrl+O öppna, Ctrl+N nytt, Ctrl+Z ångra, Ctrl+Y gör om, Ctrl+C kopiera, Ctrl+V klistra in.\r\n\r\n"
  L"EFFEKT\r\nEffektmenyn innehåller 20 filter inklusive gråskala, negativ, sepia, oskärpa, skärpa, relief och oljemålning.\r\n"
  L"Filer kan sparas som BMP, PNG, JPEG, GIF, TIFF eller ICO.\r\n\r\nEFFEKTREFERENS\r\n"
  L"Gråskala: omvandlar färger till ljusstyrka. Negativ: inverterar alla färger.\r\n"
  L"Sepia: ger gammalt fotoutseende. Oskärpa: tillämpar 3x3-lådokskärpa.\r\n"
  L"Skärpa: framhäver kanter. Pixla: reducerar bilden till 8x8-block.\r\n"
  L"Ljusstyrka +/-: ljusare eller mörkare. Relief: intryck av upphöjd yta.\r\n"
  L"Kantdetektion: extraherar konturer. Posterisera: reducerar färger till 4 nivåer.\r\n"
  L"Lägg till brus: lägger till slumpmässiga prickar. Kontrast och Mättnad +/-: tonjusteringar.\r\n"
  L"Gaussisk oskärpa: mjuk 5x5-oskärpa. Oljemålning: utseende av penseldrag.\r\n"
  L"Solarisation: inverterar ljusa toner. Byt kanaler: byter röda och blå kanalen.\r\n\r\nVANLIGA FRÅGOR\r\n"
  L"F: Hur många ångra-steg? S: 32 steg.\r\n"
  L"F: Var sparas inställningarna? S: I filen minipaint.ini bredvid programmet.\r\n"
  L"F: Får jag använda mina teckningar kommersiellt? S: Ja, dina teckningar är helt dina egna.\r\n"
  L"F: Varför flimrar inte duken? S: All ritning använder dubbelbuffring.\r\n",
  L"&Källkod", L"Källkod - MiniPaint",
  L"Akvarell", L"Våt", L"Olja", L"Blyerts", L"Kalligr", L"Band",
  L"Neon", L"Skugga", L"Referens", L"Ladda...", L"Visa/dölj", L"Ta bort", L"Stabilisering", L"Dither", L"Pixelperfekt", L"Pixelpalett", L"G-Pen", L"Raster", L"Hastighet", L"Bubbla", L"Panel",
  L"Det gick inte att spara filen. Platsen kan vara skrivskyddad eller disken full." },
// ---- Macarca ----
{ L"Ceruza", L"Vonal", L"Téglalap", L"Ellipszis", L"Kitöltés", L"Radír", L"Pipetta",
  L"Permet", L"Háromszög", L"Lekerekített", L"Csillag", L"Szöveg", L"Kijelölés",
  L"Görbe", L"Rombusz", L"Ötszög", L"Hatszög", L"Nyíl", L"Szív",
  L"&Fájl", L"&Új", L"&Megnyitás...", L"&Mentés", L"Mentés má&sként...", L"&Kilépés",
  L"&Szerkesztés", L"&Visszavonás\tCtrl+Z", L"&Ismétlés\tCtrl+Y", L"&Vászon törlése",
  L"&Kép", L"Forgatás 90° jobbra", L"Forgatás 90° balra", L"Vízszintes tükrözés", L"Függőleges tükrözés", L"&Vászon átméretezése...",
  L"&Effektek", L"Szürkeárnyalatos", L"Negatív", L"Szépia", L"Elmosás", L"Élesítés",
  L"Pixelesítés", L"Fényerő +", L"Fényerő -",
  L"Domborítás", L"Élfelismerés", L"Poszterezés", L"Zaj hozzáadása",
  L"Kontraszt +", L"Kontraszt -", L"Telítettség +", L"Telítettség -",
  L"Gauss-féle elmosás", L"Olajfesték", L"Szolarizáció", L"Csatornacsere",
  L"&Nézet", L"&Rács", L"&Valós méret (100%)", L"&Ablakhoz igazítás", L"S&zimmetria",
  L"&Súgó", L"&Névjegy", L"&Nyelv",
  L"MiniPaint - Egyéni rajzprogram",
  L"  Eszköz: %s   Szín: #%02X%02X%02X   Méret: %d   Nagyítás: %d%%   Pozíció: %d,%d   [Ctrl+S Mentés, Ctrl+Z Visszavonás]",
  L"Hiba", L"A fájl nem nyitható meg, vagy a formátum nem támogatott.",
  L"Nem mentett módosítások vannak. Menti kilépés előtt?",
  L"MiniPaint v2.2\n\nTeljesen a nulláról írt rajzprogram\ntiszta C++ és Win32 API használatával.\nKülső könyvtárak nélkül.\n\n"
  L"36 eszköz, 32 lépéses visszavonás, 20 effekt, nagyítás, szimmetria,\nBMP/PNG/JPEG/GIF/TIFF/ICO támogatás és 29 nyelv.",
  L"Képfájlok (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Mégse", L"Szélesség:", L"Magasság:",
  L"Egyéni szín választása", L"Vászon mérete", L"Kitöltött", L"Kerek hegy", L"Szögletes hegy",
  L"Sokszög", L"Kenés", L"Bélyegző",
  L"&Felhasználói útmutató", L"Felhasználói útmutató",
  L"MINIPAINT FELHASZNÁLÓI ÚTMUTATÓ\r\n\r\nESZKÖZÖK\r\nCeruza (P): szabadkézi rajzolás. A jobb gomb a másodlagos színnel rajzol.\r\n"
  L"Vonal (L), Téglalap (R), Ellipszis (O), Háromszög, Lekerekített, Csillag, Rombusz, Ötszög, Hatszög, Nyíl és Szív: húzással rajzolható. A 'Kitöltött' gomb kitölti az alakzatokat.\r\n"
  L"Görbe: húzza a két végpontot, majd kattintson a vezérlőponthoz. Esc megszakítja.\r\n"
  L"Sokszög: kattintással adjon pontokat, dupla kattintás zárja az alakzatot.\r\n"
  L"Kitöltés (F): területet színez. Permet (S): szórófesték. Kenés: nedves festékként húzza a színeket.\r\n"
  L"Radír (E): a másodlagos színre töröl. Pipetta (I): színmintát vesz a vászonról.\r\n"
  L"Szöveg (T): kattintson a vászonra és gépeljen; máshová kattintva alkalmazza.\r\n"
  L"Kijelölés (M): húzzon téglalapot, belül húzva mozgatható. Delete töröl, Esc megszakít.\r\n"
  L"Bélyegző: mosolygós arcot nyom; mérete az ecset vastagságától függ.\r\n\r\nSZÍNEK\r\n"
  L"A palettán bal kattintás az elsődleges, jobb kattintás a másodlagos színt választja.\r\n"
  L"A paletta dupla kattintása vagy a jobb oldali színdobozok megnyitják az egyéni színválasztót.\r\n\r\nNÉZET\r\n"
  L"Ctrl + egérgörgő: nagyítás. Középső gombbal húzás: eltolás. Ctrl+G: rács.\r\n"
  L"Szimmetria: tükrözi az ecsetvonásokat a függőleges tengelyen.\r\n\r\nGYORSBILLENTYŰK\r\n"
  L"Ctrl+S mentés, Ctrl+O megnyitás, Ctrl+N új, Ctrl+Z visszavonás, Ctrl+Y ismétlés, Ctrl+C másolás, Ctrl+V beillesztés.\r\n\r\n"
  L"EFFEKTUSOK\r\nAz Effektek menü 20 szűrőt tartalmaz: szürkeárnyalatos, negatív, szépia, elmosás, élesítés, domborítás és olajfesték.\r\n"
  L"A fájlok BMP, PNG, JPEG, GIF, TIFF vagy ICO formátumban menthetők.\r\n\r\nEFFEKTUS REFERENCIA\r\n"
  L"Szürkeárnyalatos: a színeket fényerővé alakítja. Negatív: minden színt megfordít.\r\n"
  L"Szépia: régi fénykép hatást ad. Elmosás: 3x3-as dobozelmosást alkalmaz.\r\n"
  L"Élesítés: kiemeli az éleket. Pixelesítés: 8x8-as blokkokra redukálja a képet.\r\n"
  L"Fényerő +/-: világosít vagy sötétít. Domborítás: dombornyomott felület benyomását kelti.\r\n"
  L"Élfelismerés: kivonja a körvonalakat. Poszterezés: 4 szintre redukálja a színeket.\r\n"
  L"Zaj hozzáadása: véletlenszerű pontokat ad hozzá. Kontraszt és Telítettség +/-: tónusbeállítások.\r\n"
  L"Gauss-féle elmosás: lágy 5x5-ös elmosás. Olajfesték: ecsetvonás megjelenést ad.\r\n"
  L"Szolarizáció: megfordítja a világos tónusokat. Csatornacsere: felcseréli a vörös és kék csatornát.\r\n\r\nGYAKORI KÉRDÉSEK\r\n"
  L"K: Hány lépés vonható vissza? V: 32 lépés.\r\n"
  L"K: Hol mentődnek a beállítások? V: A program melletti minipaint.ini fájlba.\r\n"
  L"K: Kereskedelmi célra használhatom a rajzaimat? V: Igen, a rajzok teljesen az Önéi.\r\n"
  L"K: Miért nem villog a vászon? V: Minden rajzolás dupla pufferelést használ.\r\n",
  L"&Forráskód", L"Forráskód - MiniPaint",
  L"Akvarell", L"Nedves", L"Olaj", L"Ceruza", L"Kalligr", L"Szalag",
  L"Neon", L"Árnyék", L"Referencia", L"Betöltés...", L"Mutat/Elrejt", L"Eltávolítás", L"Stabilizálás", L"Szórás", L"Tökéletes pixel", L"Pixelpaletta", L"G-Pen", L"Raszter", L"Sebesség", L"Bubi", L"Panel",
  L"A fájl mentése nem sikerült. Lehet, hogy a hely írásvédett vagy a lemez tele van." },
// ---- Rumence ----
{ L"Creion", L"Linie", L"Casetă", L"Elipsă", L"Umplere", L"Radieră", L"Pipetă",
  L"Spray", L"Triunghi", L"Rotunjit", L"Stea", L"Text", L"Selecție",
  L"Curbă", L"Romb", L"Pentagon", L"Hexagon", L"Săgeată", L"Inimă",
  L"&Fișier", L"&Nou", L"&Deschidere...", L"&Salvare", L"Salvare c&a...", L"&Ieșire",
  L"&Editare", L"&Anulare\tCtrl+Z", L"&Refacere\tCtrl+Y", L"&Curățare pânză",
  L"&Imagine", L"Rotire 90° dreapta", L"Rotire 90° stânga", L"Oglindire orizontală", L"Oglindire verticală", L"&Redimensionare pânză...",
  L"&Efecte", L"Tonuri de gri", L"Negativ", L"Sepia", L"Estompare", L"Accentuare",
  L"Pixelizare", L"Luminozitate +", L"Luminozitate -",
  L"Relief", L"Detectare margini", L"Posterizare", L"Adăugare zgomot",
  L"Contrast +", L"Contrast -", L"Saturație +", L"Saturație -",
  L"Estompare gaussiană", L"Pictură în ulei", L"Solarizare", L"Schimbare canale",
  L"&Vizualizare", L"&Grilă", L"&Dimensiune reală (100%)", L"&Potrivire în fereastră", L"S&imetrie",
  L"&Ajutor", L"&Despre", L"&Limbă",
  L"MiniPaint - Program de desen personalizat",
  L"  Instrument: %s   Culoare: #%02X%02X%02X   Dimensiune: %d   Zoom: %d%%   Poziție: %d,%d   [Ctrl+S Salvare, Ctrl+Z Anulare]",
  L"Eroare", L"Fișierul nu a putut fi deschis sau formatul nu este acceptat.",
  L"Există modificări nesalvate. Doriți să salvați înainte de ieșire?",
  L"MiniPaint v2.2\n\nUn program de desen scris integral de la zero\nîn C++ pur și Win32 API.\nFără biblioteci externe.\n\n"
  L"36 de instrumente, anulare în 32 de pași, 20 de efecte, zoom, simetrie,\nsuport BMP/PNG/JPEG/GIF/TIFF/ICO și 29 de limbi.",
  L"Fișiere imagine (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Anulare", L"Lățime:", L"Înălțime:",
  L"Alegere culoare personalizată", L"Dimensiune pânză", L"Umplut", L"Vârf rotund", L"Vârf pătrat",
  L"Poligon", L"Ștergere", L"Ștampilă",
  L"&Ghid de utilizare", L"Ghid de utilizare",
  L"GHID DE UTILIZARE MINIPAINT\r\n\r\nINSTRUMENTE\r\nCreion (P): desen liber. Clic dreapta desenează cu culoarea secundară.\r\n"
  L"Linie (L), Casetă (R), Elipsă (O), Triunghi, Rotunjit, Stea, Romb, Pentagon, Hexagon, Săgeată și Inimă: trageți pentru a desena. Butonul 'Umplut' umple formele.\r\n"
  L"Curbă: trageți capetele, apoi faceți clic pentru punctul de control. Esc anulează.\r\n"
  L"Poligon: clic pentru a adăuga puncte, dublu clic pentru a închide forma.\r\n"
  L"Umplere (F): colorează o zonă. Spray (S): vopsea pulverizată. Ștergere: trage culorile ca vopseaua umedă.\r\n"
  L"Radieră (E): șterge până la culoarea secundară. Pipetă (I): prelevează culoarea de pe pânză.\r\n"
  L"Text (T): clic pe pânză și tastați; clic în altă parte pentru aplicare.\r\n"
  L"Selecție (M): trageți un dreptunghi, trageți în interior pentru mutare. Delete șterge, Esc anulează.\r\n"
  L"Ștampilă: tipărește un zâmbet; dimensiunea depinde de grosimea pensulei.\r\n\r\nCULORI\r\n"
  L"Clic stânga pe paletă selectează culoarea principală, clic dreapta pe cea secundară.\r\n"
  L"Dublu clic pe paletă sau pe casetele de culoare din dreapta deschide selectorul de culoare personalizată.\r\n\r\nVIZUALIZARE\r\n"
  L"Ctrl + rotița mouse-ului: zoom. Tragere cu butonul din mijloc: panoramare. Ctrl+G: grilă.\r\n"
  L"Simetrie: oglindește penițele pe axa verticală.\r\n\r\nSCURTĂTURI\r\n"
  L"Ctrl+S salvare, Ctrl+O deschidere, Ctrl+N nou, Ctrl+Z anulare, Ctrl+Y refacere, Ctrl+C copiere, Ctrl+V lipire.\r\n\r\n"
  L"EFECTE\r\nMeniul Efecte conține 20 de filtre inclusiv tonuri de gri, negativ, sepia, estompare, accentuare, relief și pictură în ulei.\r\n"
  L"Fișierele pot fi salvate ca BMP, PNG, JPEG, GIF, TIFF sau ICO.\r\n\r\nREFERINȚĂ EFECTE\r\n"
  L"Tonuri de gri: convertește culorile în luminozitate. Negativ: inversează toate culorile.\r\n"
  L"Sepia: dă aspect de fotografie veche. Estompare: aplică estompare cadru 3x3.\r\n"
  L"Accentuare: evidențiază marginile. Pixelizare: reduce imaginea la blocuri 8x8.\r\n"
  L"Luminozitate +/-: luminează sau întunecă. Relief: impresie de suprafață în relief.\r\n"
  L"Detectare margini: extrage contururile. Posterizare: reduce culorile la 4 niveluri.\r\n"
  L"Adăugare zgomot: adaugă puncte aleatorii. Contrast și Saturație +/-: ajustări de ton.\r\n"
  L"Estompare gaussiană: estompare fină 5x5. Pictură în ulei: aspect de tușă de pensulă.\r\n"
  L"Solarizare: inversează tonurile deschise. Schimbare canale: schimbă canalul roșu cu cel albastru.\r\n\r\nÎNTREBĂRI FRECVENTE\r\n"
  L"Î: Câți pași de anulare? R: 32 de pași.\r\n"
  L"Î: Unde se salvează setările? R: În fișierul minipaint.ini de lângă program.\r\n"
  L"Î: Pot folosi comercial desenele mele? R: Da, desenele vă aparțin integral.\r\n"
  L"Î: De ce nu pâlpâie pânza? R: Tot desenul folosește dublă bufferizare.\r\n",
  L"Cod &sursă", L"Cod sursă - MiniPaint",
  L"Acuarelă", L"Umed", L"Ulei", L"Creion", L"Caligraf", L"Panglică",
  L"Neon", L"Umbră", L"Referință", L"Încarcă...", L"Afișează/Ascunde", L"Elimină", L"Stabilizare", L"Dither", L"Pixel perfect", L"Paletă pixel", L"G-Pen", L"Raster", L"Viteză", L"Balon", L"Panou",
  L"Fișierul nu a putut fi salvat. Locația poate fi doar pentru citire sau discul este plin." },
// ---- Danca ----
{ L"Blyant", L"Linje", L"Kasse", L"Ellipse", L"Udfyld", L"Viskelæder", L"Pipette",
  L"Spray", L"Trekant", L"Afrundet", L"Stjerne", L"Tekst", L"Markering",
  L"Kurve", L"Rombe", L"Femkant", L"Sekskant", L"Pil", L"Hjerte",
  L"&Filer", L"&Ny", L"&Åbn...", L"&Gem", L"Gem &som...", L"&Afslut",
  L"&Rediger", L"&Fortryd\tCtrl+Z", L"&Gentag\tCtrl+Y", L"&Ryd lærred",
  L"&Billede", L"Rotér 90° med uret", L"Rotér 90° mod uret", L"Vend vandret", L"Vend lodret", L"&Ændr lærredstørrelse...",
  L"&Effekter", L"Gråtoner", L"Negativ", L"Sepia", L"Sløring", L"Skarphed",
  L"Pixélér", L"Lysstyrke +", L"Lysstyrke -",
  L"Relief", L"Kantdetektion", L"Posterisér", L"Tilføj støj",
  L"Kontrast +", L"Kontrast -", L"Mætning +", L"Mætning -",
  L"Gaussisk sløring", L"Oliemaling", L"Solisering", L"Byt kanaler",
  L"&Vis", L"&Gitter", L"&Faktisk størrelse (100%)", L"&Tilpas til vindue", L"S&ymmetri",
  L"&Hjælp", L"&Om", L"&Sprog",
  L"MiniPaint - Tilpasset maleprogram",
  L"  Værktøj: %s   Farve: #%02X%02X%02X   Størrelse: %d   Zoom: %d%%   Position: %d,%d   [Ctrl+S Gem, Ctrl+Z Fortryd]",
  L"Fejl", L"Filen kunne ikke åbnes, eller formatet understøttes ikke.",
  L"Der er ikke-gemte ændringer. Vil du gemme før afslutning?",
  L"MiniPaint v2.2\n\nEt maleprogram skrevet helt fra bunden\ni ren C++ og Win32 API.\nIngen eksterne biblioteker.\n\n"
  L"36 værktøjer, 32-trins fortryd, 20 effekter, zoom, symmetri,\nBMP/PNG/JPEG/GIF/TIFF/ICO-understøttelse og 29 sprog.",
  L"Billedfiler (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Annuller", L"Bredde:", L"Højde:",
  L"Vælg brugerdefineret farve", L"Lærredstørrelse", L"Udfyldt", L"Rund spids", L"Firkantet spids",
  L"Polygon", L"Udstryg", L"Stempel",
  L"&Brugervejledning", L"Brugervejledning",
  L"MINIPAINT BRUGERVEJLEDNING\r\n\r\nVÆRKTØJER\r\nBlyant (P): frihåndstegning. Højreklik tegner med den sekundære farve.\r\n"
  L"Linje (L), Kasse (R), Ellipse (O), Trekant, Afrundet, Stjerne, Rombe, Femkant, Sekskant, Pil og Hjerte: træk for at tegne. Knappen 'Udfyldt' udfylder figurer.\r\n"
  L"Kurve: træk endepunkterne, klik derefter for at placere kontrolpunktet. Esc annullerer.\r\n"
  L"Polygon: klik for at tilføje punkter, dobbeltklik for at lukke figuren.\r\n"
  L"Udfyld (F): farver et område. Spray (S): spraymaling. Udstryg: trækker farver som våd maling.\r\n"
  L"Viskelæder (E): sletter til sekundærfarven. Pipette (I): tager en farveprøve fra lærredet.\r\n"
  L"Tekst (T): klik på lærredet og skriv; klik et andet sted for at anvende.\r\n"
  L"Markering (M): træk et rektangel, træk indeni for at flytte. Delete sletter, Esc annullerer.\r\n"
  L"Stempel: stempler et smilende ansigt; størrelsen afhænger af penslens tykkelse.\r\n\r\nFARVER\r\n"
  L"Venstreklik på paletten vælger primærfarven, højreklik sekundærfarven.\r\n"
  L"Dobbeltklik på paletten eller farvefelterne til højre åbner den brugerdefinerede farvevælger.\r\n\r\nVISNING\r\n"
  L"Ctrl + musehjul: zoom. Træk med midterknappen: panorér. Ctrl+G: gitter.\r\n"
  L"Symmetri: spejler penselstrøg i den lodrette akse.\r\n\r\nGENVEJE\r\n"
  L"Ctrl+S gem, Ctrl+O åbn, Ctrl+N ny, Ctrl+Z fortryd, Ctrl+Y gentag, Ctrl+C kopiér, Ctrl+V sæt ind.\r\n\r\n"
  L"EFFEKTER\r\nEffektmenuen indeholder 20 filtre herunder gråtoner, negativ, sepia, sløring, skarphed, relief og oliemaling.\r\n"
  L"Filer kan gemmes som BMP, PNG, JPEG, GIF, TIFF eller ICO.\r\n\r\nEFEKTREFERENCE\r\n"
  L"Gråtoner: konverterer farver til lysstyrke. Negativ: inverterer alle farver.\r\n"
  L"Sepia: giver et gammelt foto-look. Sløring: anvender 3x3-bokssløring.\r\n"
  L"Skarphed: fremhæver kanter. Pixélér: reducerer billedet til 8x8-blokke.\r\n"
  L"Lysstyrke +/-: lysere eller mørkere. Relief: indtryk af en hævet overflade.\r\n"
  L"Kantdetektion: udtrækker konturer. Posterisér: reducerer farver til 4 niveauer.\r\n"
  L"Tilføj støj: tilføjer tilfældige prikker. Kontrast og Mætning +/-: tonejusteringer.\r\n"
  L"Gaussisk sløring: blød 5x5-sløring. Oliemaling: udseende af penselstrøg.\r\n"
  L"Solisering: inverterer lyse toner. Byt kanaler: bytter den røde og blå kanal.\r\n\r\nOFTTE STILLEDE SPØRGSMÅL\r\n"
  L"S: Hvor mange fortryd-trin? A: 32 trin.\r\n"
  L"S: Hvor gemmes indstillingerne? A: I filen minipaint.ini ved siden af programmet.\r\n"
  L"S: Må jeg bruge mine tegninger kommercielt? A: Ja, dine tegninger er helt dine egne.\r\n"
  L"S: Hvorfor blinker lærredet ikke? A: Al tegning bruger dobbeltbuffering.\r\n",
  L"&Kildekode", L"Kildekode - MiniPaint",
  L"Akvarel", L"Våd", L"Olie", L"Blyant", L"Kalligr", L"Bånd",
  L"Neon", L"Skygge", L"Reference", L"Indlæs...", L"Vis/skjul", L"Fjern", L"Stabilisering", L"Dither", L"Pixelperfekt", L"Pixelpalet", L"G-Pen", L"Raster", L"Hastighed", L"Boble", L"Panel",
  L"Filen kunne ikke gemmes. Placeringen er muligvis skrivebeskyttet, eller disken er fuld." },
// ---- Fince ----
{ L"Lyijykynä", L"Viiva", L"Laatikko", L"Ellipsi", L"Täyttö", L"Pyyhekumi", L"Pipetti",
  L"Suihke", L"Kolmio", L"Pyöristetty", L"Tähti", L"Teksti", L"Valinta",
  L"Käyrä", L"Timantti", L"Viisikulmio", L"Kuusikulmio", L"Nuoli", L"Sydän",
  L"&Tiedosto", L"&Uusi", L"&Avaa...", L"&Tallenna", L"Tallenna &nimellä...", L"&Lopeta",
  L"&Muokkaa", L"&Kumoa\tCtrl+Z", L"&Tee uudelleen\tCtrl+Y", L"&Tyhjennä kangas",
  L"&Kuva", L"Kierrä 90° myötäpäivään", L"Kierrä 90° vastapäivään", L"Käännä vaakasuunnassa", L"Käännä pystysuunnassa", L"&Muuta kankaan kokoa...",
  L"&Tehosteet", L"Harmaasävyt", L"Negatiivi", L"Seepia", L"Sumennus", L"Terävöitys",
  L"Pikselöinti", L"Kirkkaus +", L"Kirkkaus -",
  L"Kohokuvio", L"Reunantunnistus", L"Posterisointi", L"Lisää kohinaa",
  L"Kontrasti +", L"Kontrasti -", L"Saturaatio +", L"Saturaatio -",
  L"Gaussin sumennus", L"Öljymaalaus", L"Solarisaatio", L"Vaihda kanavat",
  L"&Näkymä", L"&Ruudukko", L"&Todellinen koko (100%)", L"&Sovita ikkunaan", L"S&ymmetria",
  L"&Ohje", L"&Tietoja", L"&Kieli",
  L"MiniPaint - Mukautettu piirto-ohjelma",
  L"  Työkalu: %s   Väri: #%02X%02X%02X   Koko: %d   Zoomaus: %d%%   Sijainti: %d,%d   [Ctrl+S Tallenna, Ctrl+Z Kumoa]",
  L"Virhe", L"Tiedostoa ei voitu avata tai muotoa ei tueta.",
  L"Tallentamattomia muutoksia on. Haluatko tallentaa ennen poistumista?",
  L"MiniPaint v2.2\n\nTäysin alusta kirjoitettu piirto-ohjelma\npuhtaalla C++:lla ja Win32 API:lla.\nEi ulkoisia kirjastoja.\n\n"
  L"36 työkalua, 32-askeleen kumoaminen, 20 tehostetta, zoomaus, symmetria,\nBMP/PNG/JPEG/GIF/TIFF/ICO-tuki ja 29 kieltä.",
  L"Kuvatiedostot (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Peruuta", L"Leveys:", L"Korkeus:",
  L"Valitse mukautettu väri", L"Kankaan koko", L"Täytetty", L"Pyöreä kärki", L"Neliökärki",
  L"Monikulmio", L"Sively", L"Leima",
  L"&Käyttöopas", L"Käyttöopas",
  L"MINIPAINT KÄYTTÖOPAS\r\n\r\nTYÖKALUT\r\nLyijykynä (P): vapaan käden piirto. Oikea painike piirtää toissijaisella värillä.\r\n"
  L"Viiva (L), Laatikko (R), Ellipsi (O), Kolmio, Pyöristetty, Tähti, Timantti, Viisikulmio, Kuusikulmio, Nuoli ja Sydän: vedä piirtääksesi. 'Täytetty'-painike täyttää muodot.\r\n"
  L"Käyrä: vedä päätepisteet, napsauta sitten ohjauspistettä. Esc peruuttaa.\r\n"
  L"Monikulmio: napsauta lisätäksesi pisteitä, kaksoisnapsauta sulkeaksesi muodon.\r\n"
  L"Täyttö (F): värjää alueen. Suihke (S): suihkemaali. Sively: vetää värejä kuin märkää maalia.\r\n"
  L"Pyyhekumi (E): pyyhkii toissijaiseen väriin. Pipetti (I): ottaa väriäytteen kankaalta.\r\n"
  L"Teksti (T): napsauta kangasta ja kirjoita; napsauta muualla soveltaaksesi.\r\n"
  L"Valinta (M): vedä suorakulmio, vedä sisältä siirtääksesi. Delete poistaa, Esc peruuttaa.\r\n"
  L"Leima: painaa hymyilevän kasvot; koko riippuu siveltimen paksuudesta.\r\n\r\nVÄRIT\r\n"
  L"Vasen napsautus paletilla valitsee päävärin, oikea napsautus toissijaisen värin.\r\n"
  L"Kaksoisnapsauta palettia tai oikealla olevia väriruutuja avataksesi mukautetun värivalitsimen.\r\n\r\nNÄKYMÄ\r\n"
  L"Ctrl + hiiren rulla: zoomaus. Keskipainikkeella vetäminen: siirtyminen. Ctrl+G: ruudukko.\r\n"
  L"Symmetria: peilaa siveltimen vedot pystyakselilla.\r\n\r\nPIKANÄPPÄIMET\r\n"
  L"Ctrl+S tallenna, Ctrl+O avaa, Ctrl+N uusi, Ctrl+Z kumoa, Ctrl+Y tee uudelleen, Ctrl+C kopioi, Ctrl+V liitä.\r\n\r\n"
  L"TEHOSTEET\r\nTehosteet-valikossa on 20 suodinta mukaan lukien harmaasävyt, negatiivi, seepia, sumennus, terävöitys, kohokuvio ja öljymaalaus.\r\n"
  L"Tiedostot voidaan tallentaa muodossa BMP, PNG, JPEG, GIF, TIFF tai ICO.\r\n\r\nTEHOSTEVIITE\r\n"
  L"Harmaasävyt: muuntaa värit kirkkaudeksi. Negatiivi: kääntää kaikki värit.\r\n"
  L"Seepia: antaa vanhan valokuvan ilmeen. Sumennus: soveltaa 3x3-laatikkósumennusta.\r\n"
  L"Terävöitys: korostaa reunoja. Pikselöinti: pelkistää kuvan 8x8-lohkoihin.\r\n"
  L"Kirkkaus +/-: vaalentaa tai tummettaa. Kohokuvio: kohokuvioituneen pinnan vaikutelma.\r\n"
  L"Reunantunnistus: poimii ääriviivat. Posterisointi: pelkistää värit 4 tasoon.\r\n"
  L"Lisää kohinaa: lisää satunnaisia pisteitä. Kontrasti ja Saturaatio +/-: sävysäädöt.\r\n"
  L"Gaussin sumennus: pehmeä 5x5-sumennus. Öljymaalaus: siveltimen vedon ilme.\r\n"
  L"Solarisaatio: kääntää vaaleat sävyt. Vaihda kanavat: vaihtaa punaisen ja sinisen kanavan.\r\n\r\nUSEIN KYSYTYT KYSYMYKSET\r\n"
  L"K: Kuinka monta kumousaskelta? V: 32 askelta.\r\n"
  L"K: Mihin asetukset tallennetaan? V: Ohjelman vieressä olevaan minipaint.ini-tiedostoon.\r\n"
  L"K: Voinko käyttää piirroksiani kaupallisesti? V: Kyllä, piirroksesi ovat täysin sinun.\r\n"
  L"K: Miksi kangas ei vilku? V: Kaikki piirto käyttää kaksoispuskurointia.\r\n",
  L"&Lähdekoodi", L"Lähdekoodi - MiniPaint",
  L"Akvarelli", L"Märkä", L"Öljy", L"Lyijy", L"Kalligr", L"Nauha",
  L"Neon", L"Varjo", L"Viite", L"Lataa...", L"Näytä/Piilota", L"Poista", L"Vakautus", L"Dither", L"Täydellinen pikseli", L"Pikselipaletti", L"G-Pen", L"Rasteri", L"Nopeus", L"Kupla", L"Paneeli",
  L"Tiedostoa ei voitu tallentaa. Sijainti voi olla vain luku -tilassa tai levy on täynnä." },
// ---- Norveççe ----
{ L"Blyant", L"Linje", L"Boks", L"Ellipse", L"Fyll", L"Viskelær", L"Pipette",
  L"Spray", L"Trekant", L"Avrundet", L"Stjerne", L"Tekst", L"Utvalg",
  L"Kurve", L"Diamant", L"Femkant", L"Sekskant", L"Pil", L"Hjerte",
  L"&Fil", L"&Ny", L"&Åpne...", L"&Lagre", L"Lagre s&om...", L"&Avslutt",
  L"&Rediger", L"&Angre\tCtrl+Z", L"&Gjenta\tCtrl+Y", L"&Tøm lerret",
  L"&Bilde", L"Roter 90° med klokka", L"Roter 90° mot klokka", L"Vend horisontalt", L"Vend vertikalt", L"&Endre lerretsstørrelse...",
  L"&Effekter", L"Gråtoner", L"Negativ", L"Sepia", L"Uskarphet", L"Gjør skarpere",
  L"Pikselering", L"Lysstyrke +", L"Lysstyrke -",
  L"Relief", L"Kantdeteksjon", L"Posterisering", L"Legg til støy",
  L"Kontrast +", L"Kontrast -", L"Metning +", L"Metning -",
  L"Gaussisk uskarphet", L"Oljemaling", L"Solarisering", L"Bytt kanaler",
  L"&Visning", L"&Rutenett", L"&Faktisk størrelse (100%)", L"&Tilpass til vindu", L"S&ymmetri",
  L"&Hjelp", L"&Om", L"&Språk",
  L"MiniPaint - Tilpasset maleprogram",
  L"  Verktøy: %s   Farge: #%02X%02X%02X   Størrelse: %d   Zoom: %d%%   Posisjon: %d,%d   [Ctrl+S Lagre, Ctrl+Z Angre]",
  L"Feil", L"Kunne ikke åpne filen, eller formatet støttes ikke.",
  L"Det finnes ulagrede endringer. Vil du lagre før du avslutter?",
  L"MiniPaint v2.2\n\nEt maleprogram skrevet helt fra bunnen av\ni ren C++ og Win32 API.\nIngen eksterne biblioteker.\n\n"
  L"36 verktøy, 32-stegs angre, 20 effekter, zoom, symmetri,\nBMP/PNG/JPEG/GIF/TIFF/ICO-støtte og 29 språk.",
  L"Bildefiler (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Avbryt", L"Bredde:", L"Høyde:",
  L"Velg egendefinert farge", L"Lerretsstørrelse", L"Fylt", L"Rund spiss", L"Firkantet spiss",
  L"Polygon", L"Smør", L"Stempel",
  L"&Brukerhåndbok", L"Brukerhåndbok",
  L"MINIPAINT BRUKERHÅNDBOK\r\n\r\nVERKTØY\r\nBlyant (P): frihåndstegning. Høyreklikk tegner med sekundærfargen.\r\n"
  L"Linje (L), Boks (R), Ellipse (O), Trekant, Avrundet, Stjerne, Diamant, Femkant, Sekskant, Pil og Hjerte: dra for å tegne. 'Fylt'-knappen fyller figurer.\r\n"
  L"Kurve: dra endepunktene, klikk deretter for å plassere kontrollpunktet. Esc avbryter.\r\n"
  L"Polygon: klikk for å legge til punkter, dobbeltklikk for å lukke figuren.\r\n"
  L"Fyll (F): farger et område. Spray (S): spraymaling. Smør: drar farger som våt maling.\r\n"
  L"Viskelær (E): sletter til sekundærfargen. Pipette (I): tar en fargeprøve fra lerretet.\r\n"
  L"Tekst (T): klikk på lerretet og skriv; klikk et annet sted for å bruke.\r\n"
  L"Utvalg (M): dra et rektangel, dra inni for å flytte. Delete sletter, Esc avbryter.\r\n"
  L"Stempel: stempler et smilefjes; størrelsen avhenger av penseltykkelsen.\r\n\r\nFARGER\r\n"
  L"Venstreklikk på paletten velger primærfargen, høyreklikk sekundærfargen.\r\n"
  L"Dobbeltklikk på paletten eller fargeboksene til høyre åpner den egendefinerte fargevelgeren.\r\n\r\nVISNING\r\n"
  L"Ctrl + mushjul: zoom. Dra med midtknappen: panorering. Ctrl+G: rutenett.\r\n"
  L"Symmetri: speiler penselstrøk i den vertikale aksen.\r\n\r\nSNARVEIER\r\n"
  L"Ctrl+S lagre, Ctrl+O åpne, Ctrl+N ny, Ctrl+Z angre, Ctrl+Y gjenta, Ctrl+C kopier, Ctrl+V lim inn.\r\n\r\n"
  L"EFFEKTER\r\nEffektmenyen inneholder 20 filtre inkludert gråtoner, negativ, sepia, uskarphet, skarphet, relief og oljemaling.\r\n"
  L"Filer kan lagres som BMP, PNG, JPEG, GIF, TIFF eller ICO.\r\n\r\nEFEKTREFERANSE\r\n"
  L"Gråtoner: konverterer farger til lysstyrke. Negativ: inverterer alle farger.\r\n"
  L"Sepia: gir et gammelt fotoutseende. Uskarphet: bruker 3x3-boksuskarphet.\r\n"
  L"Gjør skarpere: fremhever kanter. Pikselering: reduserer bildet til 8x8-blokker.\r\n"
  L"Lysstyrke +/-: lysere eller mørkere. Relief: inntrykk av hevet overflate.\r\n"
  L"Kantdeteksjon: trekker ut konturer. Posterisering: reduserer farger til 4 nivåer.\r\n"
  L"Legg til støy: legger til tilfeldige prikker. Kontrast og Metning +/-: tonejusteringer.\r\n"
  L"Gaussisk uskarphet: myk 5x5-uskarphet. Oljemaling: utseende av penselstrøk.\r\n"
  L"Solarisering: inverterer lyse toner. Bytt kanaler: bytter rød og blå kanal.\r\n\r\nOFTE STILTE SPØRSMÅL\r\n"
  L"S: Hvor mange angretrinn? S: 32 trinn.\r\n"
  L"S: Hvor lagres innstillingene? S: I filen minipaint.ini ved siden av programmet.\r\n"
  L"S: Kan jeg bruke tegningene mine kommersielt? S: Ja, tegningene dine er helt dine egne.\r\n"
  L"S: Hvorfor blinker ikke lerretet? S: All tegning bruker dobbeltbufre.\r\n",
  L"&Kildekode", L"Kildekode - MiniPaint",
  L"Akvarell", L"Våt", L"Olje", L"Blyant", L"Kalligr", L"Bånd",
  L"Neon", L"Skygge", L"Referanse", L"Last...", L"Vis/skjul", L"Fjern", L"Stabilisering", L"Dither", L"Pixelperfekt", L"Pixelpalett", L"G-Pen", L"Raster", L"Hastighet", L"Boble", L"Panel",
  L"Kunne ikke lagre filen. Plasseringen kan være skrivebeskyttet eller disken full." },
// ---- Bulgarca ----
{ L"Молив", L"Линия", L"Кутия", L"Елипса", L"Запълване", L"Гума", L"Пипетка",
  L"Спрей", L"Триъгълник", L"Заоблен", L"Звезда", L"Текст", L"Избор",
  L"Крива", L"Ромб", L"Петоъгълник", L"Шестоъгълник", L"Стрелка", L"Сърце",
  L"&Файл", L"&Нов", L"&Отвори...", L"&Запази", L"Запази &като...", L"&Изход",
  L"&Редакция", L"&Отмяна\tCtrl+Z", L"&Повтори\tCtrl+Y", L"&Изчисти платното",
  L"&Изображение", L"Завърти 90° надясно", L"Завърти 90° наляво", L"Хоризонтално огледало", L"Вертикално огледало", L"&Размер на платното...",
  L"&Ефекти", L"Сиви тонове", L"Негатив", L"Сепия", L"Размазване", L"Изостряне",
  L"Пикселизиране", L"Яркост +", L"Яркост -",
  L"Релеф", L"Откриване на ръбове", L"Постеризация", L"Добави шум",
  L"Контраст +", L"Контраст -", L"Наситеност +", L"Наситеност -",
  L"Гаусово размазване", L"Маслена живопис", L"Соларизация", L"Размени канали",
  L"&Изглед", L"&Мрежа", L"&Реален размер (100%)", L"&Побери в прозореца", L"С&иметрия",
  L"&Помощ", L"&Относно", L"&Език",
  L"MiniPaint - Програма за рисуване",
  L"  Инструмент: %s   Цвят: #%02X%02X%02X   Размер: %d   Увеличение: %d%%   Позиция: %d,%d   [Ctrl+S Запази, Ctrl+Z Отмяна]",
  L"Грешка", L"Файлът не може да бъде отворен или форматът не се поддържа.",
  L"Има незапазени промени. Искате ли да запазите преди изход?",
  L"MiniPaint v2.2\n\nПрограма за рисуване, написана изцяло от нулата\nна чист C++ и Win32 API.\nБез външни библиотеки.\n\n"
  L"36 инструмента, отмяна на 32 стъпки, 20 ефекта, увеличение, симетрия,\nподдръжка на BMP/PNG/JPEG/GIF/TIFF/ICO и 29 езика.",
  L"Файлове с изображения (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Отказ", L"Ширина:", L"Височина:",
  L"Избор на цвят", L"Размер на платното", L"Запълнено", L"Кръгъл връх", L"Квадратен връх",
  L"Многоъгълник", L"Размазване", L"Печат",
  L"&Ръководство", L"Ръководство за употреба",
  L"РЪКОВОДСТВО ЗА УПОТРЕБА НА MINIPAINT\r\n\r\nИНСТРУМЕНТИ\r\nМолив (P): свободно рисуване. Десният бутон рисува с вторичния цвят.\r\n"
  L"Линия (L), Кутия (R), Елипса (O), Триъгълник, Заоблен, Звезда, Ромб, Петоъгълник, Шестоъгълник, Стрелка и Сърце: влачете за рисуване. Бутонът 'Запълнено' запълва фигурите.\r\n"
  L"Крива: влачете краищата, после щракнете за контролната точка. Esc отменя.\r\n"
  L"Многоъгълник: щраквайте за добавяне на точки, двойно щракване затваря фигурата.\r\n"
  L"Запълване (F): оцветява област. Спрей (S): спрей боя. Размазване: разтегля цветовете като мокра боя.\r\n"
  L"Гума (E): трие до вторичния цвят. Пипетка (I): взема проба от цвят от платното.\r\n"
  L"Текст (T): щракнете върху платното и пишете; щракване другаде прилага текста.\r\n"
  L"Избор (M): влачете правоъгълник, влачете вътре за преместване. Delete изтрива, Esc отменя.\r\n"
  L"Печат: отпечатва усмихнато лице; размерът зависи от дебелината на четката.\r\n\r\nЦВЕТОВЕ\r\n"
  L"Ляво щракване върху палитрата избира основния цвят, дясно щракване – вторичния.\r\n"
  L"Двойно щракване върху палитрата или цветните полета вдясно отваря избора на цвят.\r\n\r\nИЗГЛЕД\r\n"
  L"Ctrl + колелце на мишката: увеличение. Влачене със среден бутон: преместване. Ctrl+G: мрежа.\r\n"
  L"Симетрия: огледално отразява мазките във вертикалната ос.\r\n\r\nПРЕКИ ПЪТИЩА\r\n"
  L"Ctrl+S запази, Ctrl+O отвори, Ctrl+N нов, Ctrl+Z отмяна, Ctrl+Y повтори, Ctrl+C копирай, Ctrl+V постави.\r\n\r\n"
  L"ЕФЕКТИ\r\nМенюто Ефекти съдържа 20 филтъра, включително сиви тонове, негатив, сепия, размазване, изостряне, релеф и маслена живопис.\r\n"
  L"Файловете могат да се запазват като BMP, PNG, JPEG, GIF, TIFF или ICO.\r\n\r\nСПРАВКА ЗА ЕФЕКТИТЕ\r\n"
  L"Сиви тонове: преобразува цветовете в яркост. Негатив: обръща всички цветове.\r\n"
  L"Сепия: придава вид на стара снимка. Размазване: прилага рамково размазване 3x3.\r\n"
  L"Изостряне: подчертава ръбовете. Пикселизиране: свежда изображението до блокове 8x8.\r\n"
  L"Яркост +/-: изсветлява или затъмнява. Релеф: впечатление за релефна повърхност.\r\n"
  L"Откриване на ръбове: извлича контурите. Постеризация: свежда цветовете до 4 нива.\r\n"
  L"Добави шум: добавя случайни точки. Контраст и Наситеност +/-: тонални настройки.\r\n"
  L"Гаусово размазване: меко размазване 5x5. Маслена живопис: вид на мазки с четка.\r\n"
  L"Соларизация: обръща светлите тонове. Размени канали: разменя червения и синия канал.\r\n\r\nЧЕСТО ЗАДАВАНИ ВЪПРОСИ\r\n"
  L"В: Колко стъпки на отмяна? О: 32 стъпки.\r\n"
  L"В: Къде се запазват настройките? О: Във файла minipaint.ini до програмата.\r\n"
  L"В: Мога ли да използвам рисунките си търговски? О: Да, те са изцяло ваши.\r\n"
  L"В: Защо платното не трепти? О: Цялото рисуване използва двойно буфериране.\r\n",
  L"&Изходен код", L"Изходен код - MiniPaint",
  L"Акварел", L"Мокър", L"Масло", L"Молив", L"Калигр", L"Лента",
  L"Неон", L"Сянка", L"Референция", L"Зареди...", L"Покажи/скрий", L"Премахни", L"Стабилизатор", L"Дизеринг", L"Перфектен пиксел", L"Пикселна палитра", L"G-перо", L"Тон", L"Скорост", L"Балон", L"Панел",
  L"Файлът не можа да бъде запазен. Възможно е мястото да е само за четене или дискът да е пълен." },
// ---- Hirvatça ----
{ L"Olovka", L"Linija", L"Okvir", L"Elipsa", L"Ispuna", L"Brisač", L"Kapaljka",
  L"Sprej", L"Trokut", L"Zaobljeni", L"Zvijezda", L"Tekst", L"Odabir",
  L"Krivulja", L"Romb", L"Peterokut", L"Šesterokut", L"Strijelica", L"Srce",
  L"&Datoteka", L"&Novo", L"&Otvori...", L"&Spremi", L"Spremi &kao...", L"&Izlaz",
  L"&Uredi", L"&Poništi\tCtrl+Z", L"&Ponovi\tCtrl+Y", L"&Očisti platno",
  L"&Slika", L"Rotiraj 90° u desno", L"Rotiraj 90° u lijevo", L"Zrcali vodoravno", L"Zrcali okomito", L"&Promijeni veličinu platna...",
  L"&Efekti", L"Sive nijanse", L"Negativ", L"Sepija", L"Zamućenje", L"Izoštravanje",
  L"Pikselizacija", L"Svjetlina +", L"Svjetlina -",
  L"Reljef", L"Detekcija rubova", L"Posterizacija", L"Dodaj šum",
  L"Kontrast +", L"Kontrast -", L"Zasićenost +", L"Zasićenost -",
  L"Gaussovo zamućenje", L"Ulje na platnu", L"Solarizacija", L"Zamijeni kanale",
  L"&Prikaz", L"&Mreža", L"&Stvarna veličina (100%)", L"&Prilagodi prozoru", L"S&imetrija",
  L"&Pomoć", L"&O programu", L"&Jezik",
  L"MiniPaint - Prilagođeni program za crtanje",
  L"  Alat: %s   Boja: #%02X%02X%02X   Veličina: %d   Zum: %d%%   Položaj: %d,%d   [Ctrl+S Spremi, Ctrl+Z Poništi]",
  L"Greška", L"Datoteka se ne može otvoriti ili format nije podržan.",
  L"Postoje nespremljene promjene. Želite li spremiti prije izlaska?",
  L"MiniPaint v2.2\n\nProgram za crtanje napisan potpuno od nule\nu čistom C++ i Win32 API-ju.\nBez vanjskih biblioteka.\n\n"
  L"36 alata, poništavanje u 32 koraka, 20 efekata, zum, simetrija,\npodrška za BMP/PNG/JPEG/GIF/TIFF/ICO i 29 jezika.",
  L"Datoteke slika (*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico\0Bitmap (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0GIF (*.gif)\0*.gif\0TIFF (*.tif;*.tiff)\0*.tif;*.tiff\0ICO (*.ico)\0*.ico\0",
  L"OK", L"Odustani", L"Širina:", L"Visina:",
  L"Odabir prilagođene boje", L"Veličina platna", L"Ispunjeno", L"Okrugli vrh", L"Četvrtasti vrh",
  L"Poligon", L"Razmazivanje", L"Pečat",
  L"&Korisnički vodič", L"Korisnički vodič",
  L"KORISNIČKI VODIČ MINIPAINT\r\n\r\nALATI\r\nOlovka (P): slobodno crtanje. Desni klik crta sekundarnom bojom.\r\n"
  L"Linija (L), Okvir (R), Elipsa (O), Trokut, Zaobljeni, Zvijezda, Romb, Peterokut, Šesterokut, Strijelica i Srce: povucite za crtanje. Gumb 'Ispunjeno' ispunjava oblike.\r\n"
  L"Krivulja: povucite krajnje točke, zatim kliknite za kontrolnu točku. Esc poništava.\r\n"
  L"Poligon: kliknite za dodavanje točaka, dvostruki klik zatvara oblik.\r\n"
  L"Ispuna (F): boji područje. Sprej (S): sprej boja. Razmazivanje: vuče boje poput mokre boje.\r\n"
  L"Brisač (E): briše do sekundarne boje. Kapaljka (I): uzima uzorak boje s platna.\r\n"
  L"Tekst (T): kliknite na platno i tipkajte; klik negdje drugdje primjenjuje tekst.\r\n"
  L"Odabir (M): povucite pravokutnik, povucite unutra za pomicanje. Delete briše, Esc poništava.\r\n"
  L"Pečat: otiska smješko; veličina ovisi o debljini kista.\r\n\r\nBOJE\r\n"
  L"Lijevi klik na paletu bira primarnu boju, desni klik sekundarnu.\r\n"
  L"Dvostruki klik na paletu ili okvire boja desno otvara izbornik prilagođene boje.\r\n\r\nPRIKAZ\r\n"
  L"Ctrl + kotačić miša: zum. Povlačenje srednjim gumbom: pomicanje. Ctrl+G: mreža.\r\n"
  L"Simetrija: zrcali poteze kista u okomitoj osi.\r\n\r\nPREČACI\r\n"
  L"Ctrl+S spremi, Ctrl+O otvori, Ctrl+N novo, Ctrl+Z poništi, Ctrl+Y ponovi, Ctrl+C kopiraj, Ctrl+V zalijepi.\r\n\r\n"
  L"EFEKTI\r\nIzbornik Efekti sadrži 20 filtara uključujući sive nijanse, negativ, sepiju, zamućenje, izoštravanje, reljef i ulje na platnu.\r\n"
  L"Datoteke se mogu spremiti kao BMP, PNG, JPEG, GIF, TIFF ili ICO.\r\n\r\nREFERENCA EFEKATA\r\n"
  L"Sive nijanse: pretvara boje u svjetlinu. Negativ: obrće sve boje.\r\n"
  L"Sepija: daje izgled stare fotografije. Zamućenje: primjenjuje zamućenje okvira 3x3.\r\n"
  L"Izoštravanje: ističe rubove. Pikselizacija: svodi sliku na blokove 8x8.\r\n"
  L"Svjetlina +/-: posvjetljava ili potamnjuje. Reljef: dojam reljefne površine.\r\n"
  L"Detekcija rubova: izvlači konture. Posterizacija: svodi boje na 4 razine.\r\n"
  L"Dodaj šum: dodaje slučajne točkice. Kontrast i Zasićenost +/-: podešavanje tona.\r\n"
  L"Gaussovo zamućenje: meko zamućenje 5x5. Ulje na platnu: izgled poteza kista.\r\n"
  L"Solarizacija: obrće svijetle tonove. Zamijeni kanale: mijenja crveni i plavi kanal.\r\n\r\nČESTA PITANJA\r\n"
  L"P: Koliko koraka poništavanja? O: 32 koraka.\r\n"
  L"P: Gdje se spremaju postavke? O: U datoteku minipaint.ini pokraj programa.\r\n"
  L"P: Mogu li svoje crteže koristiti komercijalno? O: Da, vaši crteži u potpunosti su vaši.\r\n"
  L"P: Zašto platno ne treperi? O: Svo crtanje koristi dvostruko spremanje.\r\n",
  L"&Izvorni kod", L"Izvorni kod - MiniPaint",
  L"Akvarel", L"Mokro", L"Ulje", L"Olovka", L"Kaligraf", L"Vrpca",
  L"Neon", L"Sjena", L"Referenca", L"Učitaj...", L"Prikaži/sakrij", L"Ukloni", L"Stabilizacija", L"Dither", L"Savršeni piksel", L"Pikselna paleta", L"G-Pen", L"Raster", L"Brzina", L"Balon", L"Panel",
  L"Datoteku nije bilo moguće spremiti. Lokacija je možda samo za čitanje ili je disk pun." }
};

// ------------------------------------------------------------
//  Uygulama durumu
// ------------------------------------------------------------
// Mukemmel piksel modu icin vurus durumu (son 2 nokta + geri yukleme rengi)
struct PikselDurum {
    int      sayi;         // vurusta kac nokta islendi
    int      x0, y0;       // once-onceki nokta
    int      x1, y1;       // son nokta
    COLORREF eski1;        // son noktanin altindaki orijinal renk
};

struct Uygulama {
    HINSTANCE hInst;
    HWND      pencere;
    HDC       tuvalDC;
    HBITMAP   tuvalBMP;
    HBITMAP   eskiBMP;
    int       tuvalW, tuvalH;

    Arac      arac;
    COLORREF  renk;            // birincil renk (sol tus)
    COLORREF  renk2;           // ikincil renk (sag tus / silgi zemini)
    COLORREF  islakRenk;       // islak karisim fircasinin tasidigi boya
    float     islakBoya;       // fircadaki boya miktari (0..1, surudukce azalir)
    double    gpenKal;         // G-Pen: anlik yumusatilmis kalinlik
    DWORD     gpenTick;        // G-Pen: onceki vurus zamani (hiz olcumu)
    int       kalinlik;
    bool      dolu;            // sekiller ici dolu mu
    bool      kareUc;          // firca ucu: kare mi yuvarlak mi
    bool      izgara;          // izgara gorunumu
    bool      simetri;         // dikey simetri modu
    bool      tamEkran;        // pencere ekrani kaplasin mi (varsayilan: evet)
    bool      sagCiziyor;      // sag tus ile cizim aktif
    int       egriAsama;       // egri araci: 0=yok, 2=kontrol noktasi bekleniyor
    POINT     egriA, egriB;    // egri uclari
    POINT     cokgen[64];      // serbest cokgen noktalari
    int       cokgenSayi;

    int       zoomIdx;         // ZOOM_ADIM indeksi
    int       panX, panY;      // ekran kaydirma (piksel)
    bool      panTut;          // orta tus ile kaydirma
    POINT     panBas;          // kaydirma baslangic fare
    int       panBasPX, panBasPY;

    bool      ciziyor;
    POINT     baslangic;
    POINT     onceki;

    // Metin araci
    HWND      metinEdit;
    HFONT     metinFont;
    int       metinX, metinY;  // tuval koordinati
    bool      metinKapaniyor;

    // Referans goruntu: tuvalin ustunde yari saydam katman olarak gosterilir,
    // cizime DAHIL OLMAZ (kopya kagidi gibi - karmasik resimler icin iz surme)
    HDC       refDC;
    HBITMAP   refBMP, refEski;
    int       refW, refH;
    bool      refVar, refGoster;
    int       refOpaklik;      // yuzde (25/50/75)

    // Firca sabitleme: el titremesini yumusatir (duzgun hatlar icin)
    bool      stabil;
    double    stabX, stabY;

    // Piksel sanati: mukemmel piksel (diz eklemi temizleme) + piksel paleti
    bool        pikselmuk;
    bool        paletPiksel;
    PikselDurum pik;         // ana vurus durumu
    PikselDurum pikA;        // simetri aynasi vurus durumu

    // Secim araci
    bool      secimAktif;
    bool      secimTanimla;    // su an dikdortgen ciziliyor
    bool      secimTasima;     // su an tasima yapiliyor
    bool      secimKesildi;    // icerik tuvalden alinip yuzuyor
    RECT      secim;           // tuval koordinati
    RECT      secimEv;         // kesildigi ilk konum
    POINT     secimAnchor;
    HDC       secimDC;
    HBITMAP   secimBMP, secimEski;
    int       secimW, secimH;

    bool      degisti;
    wchar_t   dosyaYolu[MAX_PATH];

    wchar_t   durumMetin[192];
    int       dil;
    int       sonX, sonY;
};

static Uygulama g = {};

// Geri alma / yineleme gecmisi (tuvalle ayni boyutta bitmap kopyalari)
static HDC     g_gecDC[GECMIS_MAX]  = {};
static HBITMAP g_gecBMP[GECMIS_MAX] = {};
static HBITMAP g_gecEski[GECMIS_MAX]= {};
static int     g_gecSayi  = 0;
static int     g_gecKonum = 0;
// Her kayit kendi donemindeki tuval boyutunu saklar; boylece dondurme ve
// boyutlandirma gibi boyut degistiren islemler de geri alinabilir ve geri
// alma gecmisi boyut degisiminde SILINMEZ (eskiden hepsi kayboluyordu).
static int     g_gecW[GECMIS_MAX]   = {};
static int     g_gecH[GECMIS_MAX]   = {};

static const wchar_t* T(int id) { return METIN[g.dil][id]; }

// Secili palet turunu aktif palete uygular (baslangicta ve degisimde cagrilir)
static void PaletUygula()
{
    PaletPikselOlustur();
    memcpy(PALET, g.paletPiksel ? PALET_PIKSEL : PALET_VARSAYILAN, sizeof(PALET));
    for (int i = 0; i < PALETTE_CNT; i++) {   // fircalari da guncelle
        if (g_paletFirca[i]) DeleteObject(g_paletFirca[i]);
        g_paletFirca[i] = CreateSolidBrush(PALET[i]);
    }
}

// Arac butonu -> metin tablosu eslemesi
static const int ARAC_METIN_ESLE[ARAC_SAYISI] = {
    M_ARAC_KALEM, M_ARAC_CIZGI, M_ARAC_KUTU, M_ARAC_ELIPS,
    M_ARAC_DOLGU, M_ARAC_SILGI, M_ARAC_DAMLALIK,
    M_ARAC_SPREY, M_ARAC_UCGEN, M_ARAC_YUVARLAK, M_ARAC_YILDIZ,
    M_ARAC_METIN, M_ARAC_SECIM,
    M_ARAC_EGRI, M_ARAC_ELMAS, M_ARAC_BESGEN, M_ARAC_ALTIGEN, M_ARAC_OK, M_ARAC_KALP,
    M_ARAC_COKGEN, M_ARAC_BULANIK, M_ARAC_DAMGA,
    M_ARAC_SULUBOYA, M_ARAC_ISLAK, M_ARAC_YAGLI,
    M_ARAC_KARAKALEM, M_ARAC_KALIGRAFI, M_ARAC_SERIT,
    M_ARAC_NEON, M_ARAC_GOLGE, M_ARAC_DITHER,
    M_ARAC_GPEN, M_ARAC_TON, M_ARAC_HIZCIZGI, M_ARAC_BALON, M_ARAC_PANEL
};

static int ZoomPct() { return ZOOM_ADIM[g.zoomIdx]; }
static float ZoomF() { return ZOOM_ADIM[g.zoomIdx] / 100.0f; }

// ------------------------------------------------------------
//  Koordinat donusumleri (ekran <-> tuval)
// ------------------------------------------------------------
static void Ekran2Tuval(int sx, int sy, int& cx, int& cy)
{
    float z = ZoomF();
    cx = (int)((sx - g.panX) / z);
    cy = (int)((sy - TOOLBAR_H - g.panY) / z);
}

// ------------------------------------------------------------
//  Geri alma / yineleme
// ------------------------------------------------------------
static void GecmisSil(int i)
{
    if (g_gecDC[i]) {
        SelectObject(g_gecDC[i], g_gecEski[i]);
        DeleteObject(g_gecBMP[i]);
        DeleteDC(g_gecDC[i]);
        g_gecDC[i] = NULL; g_gecBMP[i] = NULL;
    }
}

static void GecmisTemizle()
{
    for (int i = 0; i < GECMIS_MAX; i++) GecmisSil(i);
    g_gecSayi = 0; g_gecKonum = 0;
}

// Degisiklikten ONCE cagrilir: mevcut tuvali gecmise koyar
static void GecmisPush()
{
    for (int i = g_gecKonum; i < g_gecSayi; i++) GecmisSil(i); // yineleme kuyrugunu at
    g_gecSayi = g_gecKonum;
    if (g_gecSayi == GECMIS_MAX) {           // dolu: en eskiyi at, kaydir
        GecmisSil(0);
        for (int i = 1; i < GECMIS_MAX; i++) {
            g_gecDC[i-1] = g_gecDC[i]; g_gecBMP[i-1] = g_gecBMP[i]; g_gecEski[i-1] = g_gecEski[i];
            g_gecW[i-1] = g_gecW[i]; g_gecH[i-1] = g_gecH[i];
            g_gecDC[i] = NULL;
        }
        g_gecSayi--; g_gecKonum--;
    }
    HDC ekran = GetDC(NULL);
    int i = g_gecKonum;
    g_gecBMP[i]  = CreateCompatibleBitmap(ekran, g.tuvalW, g.tuvalH);
    g_gecDC[i]   = CreateCompatibleDC(ekran);
    if (!g_gecBMP[i] || !g_gecDC[i]) {
        // Kayit olusturulamadi: yarim kalan tutamaclari temizle, konumu
        // ilerletme; yoksa geri almada bos/bozuk kayit takas edilirdi.
        if (g_gecBMP[i]) { DeleteObject(g_gecBMP[i]); g_gecBMP[i] = NULL; }
        if (g_gecDC[i])  { DeleteDC(g_gecDC[i]);      g_gecDC[i] = NULL; }
        ReleaseDC(NULL, ekran);
        return;
    }
    g_gecEski[i] = (HBITMAP)SelectObject(g_gecDC[i], g_gecBMP[i]);
    BitBlt(g_gecDC[i], 0, 0, g.tuvalW, g.tuvalH, g.tuvalDC, 0, 0, SRCCOPY);
    g_gecW[i] = g.tuvalW; g_gecH[i] = g.tuvalH;
    ReleaseDC(NULL, ekran);
    g_gecKonum++; g_gecSayi = g_gecKonum;
}

// Son atilan kaydi geri cikar (hicbir sey degistirmeyen islemler icin,
// ornegin ayni renge yapilan dolgu denemesi). Yineleme kuyrugu zaten
// push sirasinda silinmisti; konum ve sayi push oncesi degerine doner.
static void GecmisPop()
{
    if (g_gecKonum <= 0) return;
    g_gecKonum--;
    GecmisSil(g_gecKonum);
    g_gecSayi = g_gecKonum;
}

// Tuval ile gecmis kaydinin ICERIKLERINI takas eder (bitmap tutamaclarini degistirerek)
// ONEMLI: bir bitmap ayni anda iki DC'de secili olamaz; bu yuzden once coz,
// sonra tak sirasi zorunludur. g_gecEski hic degismez: GecmisSil'in cozme
// yer tutucusu olarak kalici kalir.
static void GecmisTakas(int i)
{
    HBITMAP A = g.tuvalBMP;          // su an tuval DC'de secili
    HBITMAP B = g_gecBMP[i];         // su an gecmis DC'de secili
    SelectObject(g_gecDC[i], g_gecEski[i]);  // B'yi gecmis DC'den coz
    SelectObject(g.tuvalDC, B);              // B serbest: tuvale tak (A cozuldu)
    SelectObject(g_gecDC[i], A);             // A serbest: gecmise tak
    g.tuvalBMP  = B;
    g_gecBMP[i] = A;
    // Tuval boyutlari da birlikte takaslanir: boyut degistiren islemlerin
    // (dondurme/boyutlandirma) geri alinmasi tuvale eski boyutunu iade eder
    int tw = g.tuvalW, th = g.tuvalH;
    g.tuvalW = g_gecW[i]; g.tuvalH = g_gecH[i];
    g_gecW[i] = tw; g_gecH[i] = th;
}

static void GeriAl()
{
    if (g_gecKonum <= 0) return;
    g_gecKonum--;
    GecmisTakas(g_gecKonum);
    g.degisti = true;
    InvalidateRect(g.pencere, NULL, FALSE);
}

static void Yinele()
{
    if (g_gecKonum >= g_gecSayi) return;
    GecmisTakas(g_gecKonum);
    g_gecKonum++;
    g.degisti = true;
    InvalidateRect(g.pencere, NULL, FALSE);
}

// ------------------------------------------------------------
//  Tuval yonetimi
// ------------------------------------------------------------
// gecmisKoru=true ise geri alma gecmisi silinmez; cagiran islem
// (dondurme/boyutlandirma) kendisi GecmisPush yapar ve boyut degisimi de
// geri alinabilir olur. Donus: yeni tuval basariyla olusturulduysa true.
static bool TuvalOlustur(int w, int h, COLORREF zemin, bool gecmisKoru = false)
{
    HDC ekran = GetDC(NULL);
    HBITMAP yeni = CreateCompatibleBitmap(ekran, w, h);
    HDC yeniDC = CreateCompatibleDC(ekran);
    if (!yeni || !yeniDC) {
        // Bellek yetmezse (asiri buyuk resim vb.) mevcut tuvali koruyarak
        // geri cik; aksi halde tuval NULL kalir ve sonraki cizim cokerdi.
        if (yeni) DeleteObject(yeni);
        if (yeniDC) DeleteDC(yeniDC);
        ReleaseDC(NULL, ekran);
        return false;
    }
    HBITMAP yeniEski = (HBITMAP)SelectObject(yeniDC, yeni);
    HBRUSH f = CreateSolidBrush(zemin);
    RECT r = {0, 0, w, h};
    FillRect(yeniDC, &r, f);
    DeleteObject(f);

    if (g.tuvalDC) {
        int mw = w < g.tuvalW ? w : g.tuvalW;
        int mh = h < g.tuvalH ? h : g.tuvalH;
        BitBlt(yeniDC, 0, 0, mw, mh, g.tuvalDC, 0, 0, SRCCOPY);
        SelectObject(g.tuvalDC, g.eskiBMP);
        DeleteObject(g.tuvalBMP);
        DeleteDC(g.tuvalDC);
    }
    g.tuvalBMP = yeni; g.tuvalDC = yeniDC; g.eskiBMP = yeniEski;
    g.tuvalW = w; g.tuvalH = h;
    if (!gecmisKoru) GecmisTemizle();  // yeni belge: eski tuvalin gecmisi anlamsiz
    ReleaseDC(NULL, ekran);
    return true;
}

// ------------------------------------------------------------
//  Cizim yardimcilari
// ------------------------------------------------------------
static void CizgiCiz(HDC dc, int x0, int y0, int x1, int y1, COLORREF renk, int kal);
static void YumusakVurus(int x0, int y0, int x1, int y1, COLORREF renk, int kal);

// Piksel modunda her vurus sert kare piksel bloklariyla cizilir (gercek
// piksel gorunumu); normal modda kullanicinin sectigi uc gecerlidir
static bool KareUcAktif() { return g.kareUc || g.pikselmuk; }

static void NoktaKoy(HDC dc, int x, int y, COLORREF renk, int kal)
{
    if (kal <= 1) { SetPixel(dc, x, y, renk); return; }
    HBRUSH f = CreateSolidBrush(renk);
    HBRUSH eski = (HBRUSH)SelectObject(dc, f);
    HPEN p = CreatePen(PS_NULL, 0, 0);
    HPEN eskiP = (HPEN)SelectObject(dc, p);
    if (KareUcAktif())
        Rectangle(dc, x - kal/2, y - kal/2, x + kal/2 + 1, y + kal/2 + 1);
    else
        Ellipse(dc, x - kal/2, y - kal/2, x + kal/2, y + kal/2);
    SelectObject(dc, eskiP); DeleteObject(p);
    SelectObject(dc, eski);  DeleteObject(f);
}

// Simetri destekli firca vurusu (kalem / silgi icin).
// Yuvarlak ucta kenar yumusatilmis (puruzsuz) damgalar kullanilir;
// kare uc ve piksel modu bilerek sert ve piksel izgarasina oturuk kalir.
static void FircaVurus(int x0, int y0, int x1, int y1, COLORREF renk, int kal)
{
    if (!g.kareUc && !g.pikselmuk) {
        YumusakVurus(x0, y0, x1, y1, renk, kal);
        if (g.simetri)
            YumusakVurus(g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk, kal);
    } else {
        CizgiCiz(g.tuvalDC, x0, y0, x1, y1, renk, kal);
        if (g.simetri)
            CizgiCiz(g.tuvalDC, g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk, kal);
    }
}

// Tek tiklama noktasi: FircaVurus ile ayni yumusaklik kurallari
// (ayrica simetri acikken tek nokta da aynalanir - eskiden aynalanmiyordu)
static void FircaNokta(int x, int y, COLORREF renk, int kal)
{
    if (!g.kareUc && !g.pikselmuk) {
        YumusakVurus(x, y, x, y, renk, kal);
        if (g.simetri) YumusakVurus(g.tuvalW - 1 - x, y, g.tuvalW - 1 - x, y, renk, kal);
    } else {
        NoktaKoy(g.tuvalDC, x, y, renk, kal);
        if (g.simetri) NoktaKoy(g.tuvalDC, g.tuvalW - 1 - x, y, renk, kal);
    }
}

// --- Mukemmel piksel: serbest cizimdeki 'L' diz eklemlerini temizler ---
// Koseye oturan ara piksel kaldirilinca capraz hatlar tek piksel kalinliginda
// tertemiz akar (piksel sanati icin klasik teknik).
static void PikselBaslat(PikselDurum& s, int x, int y, COLORREF renk)
{
    s.eski1 = GetPixel(g.tuvalDC, x, y);
    SetPixel(g.tuvalDC, x, y, renk);
    s.x1 = x; s.y1 = y; s.sayi = 1;
}

static void PikselNokta(PikselDurum& s, int x, int y, COLORREF renk)
{
    if (x == s.x1 && y == s.y1) return;
    if (s.sayi >= 2) {
        // s0->s1 ve s1->yeni adimlari yatay/dikey ve yon 90 degise donduyse
        // s1 bir diz eklemi: kaldir, zincir capraz olarak bagli kalir
        bool dik0 = (s.x0 == s.x1) || (s.y0 == s.y1);
        bool dik1 = (s.x1 == x) || (s.y1 == y);
        if (dik0 && dik1 && s.x0 != x && s.y0 != y) {
            SetPixel(g.tuvalDC, s.x1, s.y1, s.eski1);
            s.eski1 = GetPixel(g.tuvalDC, x, y);
            SetPixel(g.tuvalDC, x, y, renk);
            s.x1 = x; s.y1 = y;
            return;
        }
    }
    COLORREF e = GetPixel(g.tuvalDC, x, y);
    SetPixel(g.tuvalDC, x, y, renk);
    s.x0 = s.x1; s.y0 = s.y1;
    s.x1 = x; s.y1 = y; s.eski1 = e;
    if (s.sayi < 2) s.sayi++;
}

// Iki nokta arasini Bresenham ile yuruyup her pikseli tek tek isler
static void PikselYuru(PikselDurum& s, int x0, int y0, int x1, int y1, COLORREF renk)
{
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int hata = dx + dy, x = x0, y = y0;
    for (;;) {
        if (x == x1 && y == y1) { PikselNokta(s, x, y, renk); break; }
        int e2 = 2 * hata;
        if (e2 >= dy) { hata += dy; x += sx; }
        if (e2 <= dx) { hata += dx; y += sy; }
        PikselNokta(s, x, y, renk);
    }
}

static void PikselFircaVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    PikselYuru(g.pik, x0, y0, x1, y1, renk);
    if (g.simetri)
        PikselYuru(g.pikA, g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk);
}

// Ikinci dereceden Bezier egrisi (64 parca)
static void BezierCiz(HDC dc, POINT a, POINT c, POINT b, COLORREF renk, int kal)
{
    const int N = 64;
    POINT once = a;
    for (int i = 1; i <= N; i++) {
        double t = (double)i / N, u = 1.0 - t;
        int x = (int)(u*u*a.x + 2*u*t*c.x + t*t*b.x);
        int y = (int)(u*u*a.y + 2*u*t*c.y + t*t*b.y);
        CizgiCiz(dc, once.x, once.y, x, y, renk, kal);
        once.x = x; once.y = y;
    }
}

static void CizgiCiz(HDC dc, int x0, int y0, int x1, int y1, COLORREF renk, int kal)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int hata = dx + dy;
    // Kalin uclarda firca/kalem nesneleri tum cizgi icin BIR KEZ yaratilir.
    // Eskiden her pikselde yaratilip siliniyordu; uzun vuruslar belirgin
    // sekilde yavasliyordu (500 piksellik cizgi = 1000 GDI nesnesi).
    HBRUSH f = NULL; HPEN p = NULL; HBRUSH eskiF = NULL; HPEN eskiP = NULL;
    if (kal > 1) {
        f = CreateSolidBrush(renk);
        p = CreatePen(PS_NULL, 0, 0);
        eskiF = (HBRUSH)SelectObject(dc, f);
        eskiP = (HPEN)SelectObject(dc, p);
    }
    int r = kal / 2;
    bool kare = KareUcAktif();
    for (;;) {
        if (kal <= 1) SetPixel(dc, x0, y0, renk);
        else if (kare) Rectangle(dc, x0 - r, y0 - r, x0 + r + 1, y0 + r + 1);
        else Ellipse(dc, x0 - r, y0 - r, x0 + r, y0 + r);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * hata;
        if (e2 >= dy) { hata += dy; x0 += sx; }
        if (e2 <= dx) { hata += dx; y0 += sy; }
    }
    if (p) { SelectObject(dc, eskiP); DeleteObject(p); }
    if (f) { SelectObject(dc, eskiF); DeleteObject(f); }
}

// Gulen yuz damgasi (boyut firca kalinligina bagli)
static void DamgaBas(int cx, int cy)
{
    int s = g.kalinlik * 4 + 20;
    int kal = g.kalinlik / 2 + 1;
    HPEN p = CreatePen(PS_SOLID, kal, g.renk);
    HPEN eskiP = (HPEN)SelectObject(g.tuvalDC, p);
    HBRUSH eskiF = (HBRUSH)SelectObject(g.tuvalDC, GetStockObject(NULL_BRUSH));
    Ellipse(g.tuvalDC, cx - s/2, cy - s/2, cx + s/2, cy + s/2);   // yuz
    HBRUSH f = CreateSolidBrush(g.renk);
    SelectObject(g.tuvalDC, f);
    int gz = s / 10 + 1;
    Ellipse(g.tuvalDC, cx - s/5 - gz, cy - s/6 - gz, cx - s/5 + gz, cy - s/6 + gz); // sol goz
    Ellipse(g.tuvalDC, cx + s/5 - gz, cy - s/6 - gz, cx + s/5 + gz, cy - s/6 + gz); // sag goz
    SelectObject(g.tuvalDC, GetStockObject(NULL_BRUSH));
    Arc(g.tuvalDC, cx - s/3, cy - s/4, cx + s/3, cy + s/3,        // gulumseme
        cx - s/4, cy + s/8, cx + s/4, cy + s/8);
    SelectObject(g.tuvalDC, eskiF);
    SelectObject(g.tuvalDC, eskiP);
    DeleteObject(p);
    DeleteObject(f);
}

// Piksel arabellek yardimcilari (ileride tanimli) - dolgu icin on bildirim
static BYTE* TuvalPikselAl();
static void  TuvalPikselYaz(BYTE* buf);

// Kuyruk tabanli dolgu (flood fill) - bellek uzerinde calisan hizli surum.
// Eski surum her piksel icin GetPixel/SetPixel cagiriyordu; milyonlarca GDI
// cagrisi saniyelerce takilmaya yol aciyordu. Bu surum tuvali tek seferde
// RAM'e alir, dolguyu dogrudan bellekte yapar ve tek seferde geri yazar.
// Donus: tuvalde gercekten piksel degistiyse true. Ayni renge yapilan
// dolgu hicbir pikseli degistirmez; cagiran bos gecmis kaydini atabilir.
static bool DolguYap(HDC dc, int basX, int basY, COLORREF yeniRenk)
{
    (void)dc;
    if (basX < 0 || basY < 0 || basX >= g.tuvalW || basY >= g.tuvalH) return false;
    GdiFlush();   // bekleyen GDI cizimlerinin tamamlandigindan emin ol

    BYTE* buf = TuvalPikselAl();   // 32-bit BGRX, ustten alta
    if (!buf) return false;
    const int W = g.tuvalW, H = g.tuvalH;

    // Baslangic pikselinin rengi (hedef) ve yeni renk bilesenleri
    BYTE* bp = buf + ((size_t)basY * W + basX) * 4;
    const BYTE hb = bp[0], hg = bp[1], hr = bp[2];
    const BYTE nb = GetBValue(yeniRenk), ng = GetGValue(yeniRenk), nr = GetRValue(yeniRenk);
    if (hb == nb && hg == ng && hr == nr) { free(buf); return false; }  // ayni renk

    int kapasite = W * H;
    POINT* yigin = (POINT*)malloc(sizeof(POINT) * kapasite);
    if (!yigin) { free(buf); return false; }
    int ust = 0;
    yigin[ust].x = (SHORT)basX; yigin[ust].y = (SHORT)basY; ust++;

    while (ust > 0) {
        ust--;
        int x = yigin[ust].x, y = yigin[ust].y;
        BYTE* satir = buf + (size_t)y * W * 4;
        // Satin sol kenarina git
        while (x >= 0 && satir[(size_t)x*4] == hb && satir[(size_t)x*4+1] == hg && satir[(size_t)x*4+2] == hr) x--;
        x++;
        bool ustEkle = false, altEkle = false;
        for (; x < W; x++) {
            BYTE* px = satir + (size_t)x * 4;
            if (px[0] != hb || px[1] != hg || px[2] != hr) break;
            px[0] = nb; px[1] = ng; px[2] = nr;
            if (y > 0) {
                BYTE* p = px - (size_t)W * 4;
                bool esit = (p[0] == hb && p[1] == hg && p[2] == hr);
                if (esit && !ustEkle && ust < kapasite) {
                    yigin[ust].x = (SHORT)x; yigin[ust].y = (SHORT)(y - 1); ust++;
                    ustEkle = true;
                } else if (!esit) ustEkle = false;
            }
            if (y < H - 1) {
                BYTE* p = px + (size_t)W * 4;
                bool esit = (p[0] == hb && p[1] == hg && p[2] == hr);
                if (esit && !altEkle && ust < kapasite) {
                    yigin[ust].x = (SHORT)x; yigin[ust].y = (SHORT)(y + 1); ust++;
                    altEkle = true;
                } else if (!esit) altEkle = false;
            }
        }
    }
    free(yigin);
    TuvalPikselYaz(buf);   // degisen tuvali tek seferde geri yazar (buf'i serbest birakir)
    return true;
}

// ------------------------------------------------------------
//  Boya simulasyonu fircalari (tamamen ozgun algoritmalar)
// ------------------------------------------------------------

// Iki rengi karistirir: t=0 -> a, t=1 -> b
static COLORREF RenkKaristir(COLORREF a, COLORREF b, float t)
{
    int r = (int)(GetRValue(a) * (1 - t) + GetRValue(b) * t + 0.5f);
    int gg = (int)(GetGValue(a) * (1 - t) + GetGValue(b) * t + 0.5f);
    int bl = (int)(GetBValue(a) * (1 - t) + GetBValue(b) * t + 0.5f);
    return RGB(r, gg, bl);
}

// Kismi GetDIBits/SetDIBits (startScan > 0) bu sistemde guvenilir calismiyor;
// bu yuzden kucuk bolgeler kalici bir DIB section uzerinden BitBlt ile tasinir.
static HDC     g_bolgeDC   = NULL;
static HBITMAP g_bolgeBMP  = NULL;
static HBITMAP g_bolgeEski = NULL;
static BYTE*   g_bolgeBits = NULL;
static int     g_bolgeW = 0, g_bolgeH = 0;

static void BolgeSerbest()
{
    if (g_bolgeDC) {
        SelectObject(g_bolgeDC, g_bolgeEski);
        DeleteObject(g_bolgeBMP);
        DeleteDC(g_bolgeDC);
        g_bolgeDC = NULL; g_bolgeBMP = NULL; g_bolgeBits = NULL;
        g_bolgeW = g_bolgeH = 0;
    }
}

// Tuvalin kucuk bir bolgesini hazir bellege alir (32bpp BGRX, ustten alta).
// Donen isaretci bir sonraki BolgeAl cagrisina kadar gecerlidir.
static BYTE* BolgeAl(int x, int y, int w, int h)
{
    if (w > g_bolgeW || h > g_bolgeH) {
        BolgeSerbest();
        int nw = w < 256 ? 256 : w, nh = h < 256 ? 256 : h;
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = nw;
        bi.bmiHeader.biHeight = -nh;   // ustten alta
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = NULL;
        g_bolgeBMP = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!g_bolgeBMP) return NULL;
        HDC ekran = GetDC(NULL);
        g_bolgeDC = CreateCompatibleDC(ekran);
        ReleaseDC(NULL, ekran);
        g_bolgeEski = (HBITMAP)SelectObject(g_bolgeDC, g_bolgeBMP);
        g_bolgeBits = (BYTE*)bits;
        g_bolgeW = nw; g_bolgeH = nh;
    }
    GdiFlush();
    BitBlt(g_bolgeDC, 0, 0, w, h, g.tuvalDC, x, y, SRCCOPY);
    return g_bolgeBits;
}

static void BolgeYaz(int x, int y, int w, int h)
{
    BitBlt(g.tuvalDC, x, y, w, h, g_bolgeDC, 0, 0, SRCCOPY);
}

// Sprey benekleri artik GDI yerine dogrudan bellekte islenir
// (bolge tek seferde alinir, tek seferde yazilir)
static void SpreyBolge(int cx, int cy, COLORREF renk, int r, int n)
{
    int xA = cx - r, yA = cy - r;
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    int xB = cx + r, yB = cy + r;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    size_t adim = (size_t)g_bolgeW * 4;
    BYTE nb = GetBValue(renk), ng = GetGValue(renk), nr2 = GetRValue(renk);
    for (int i = 0; i < n; i++) {
        int dx = (rand() % (2 * r + 1)) - r;
        int dy = (rand() % (2 * r + 1)) - r;
        if (dx * dx + dy * dy > r * r) continue;
        int px = cx + dx - xA, py = cy + dy - yA;
        if (px < 0 || py < 0 || px >= w || py >= h) continue;
        BYTE* p = buf + (size_t)py * adim + (size_t)px * 4;
        p[0] = nb; p[1] = ng; p[2] = nr2;
    }
    BolgeYaz(xA, yA, w, h);
}

static void SpreyPuskurt(int cx, int cy, COLORREF renk)
{
    int r = 5 + g.kalinlik * 2;
    int n = 8 + g.kalinlik * 4;
    SpreyBolge(cx, cy, renk, r, n);
    if (g.simetri)
        SpreyBolge(g.tuvalW - 1 - cx, cy, renk, r, n);
}

// --- Yuvarlak uc icin kenar yumusatilmis (anti-alias) vurus ---
// Damgalar cizgi boyunca mesafe aralikli basilir; her damganin kenarinda
// opaklik kademeli olarak sifira duser. Sonuc: testere disi (merdiven)
// olmayan, puruzsuz cizgiler. Kare uc ve piksel modu bilerek sert kalir.
static void YumusakVurus(int x0, int y0, int x1, int y1, COLORREF renk, int kal)
{
    double r = kal / 2.0;
    if (r < 0.75) r = 0.75;
    double dis = r + 1.0;                              // kenar yumusatma payi
    int xA = (int)floor((x0 < x1 ? x0 : x1) - dis);
    int yA = (int)floor((y0 < y1 ? y0 : y1) - dis);
    int xB = (int)ceil((x0 > x1 ? x0 : x1) + dis);
    int yB = (int)ceil((y0 > y1 ? y0 : y1) + dis);
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    const int nb = GetBValue(renk), ng = GetGValue(renk), nr = GetRValue(renk);
    size_t adim = (size_t)g_bolgeW * 4;

    double ddx = x1 - x0, ddy = y1 - y0;
    double len = sqrt(ddx * ddx + ddy * ddy);
    double aralik = r / 3.0;                           // damgalar ust uste biner
    if (aralik < 0.5) aralik = 0.5;
    int n = (int)(len / aralik) + 1;
    for (int s = 0; s <= n; s++) {
        double t = (double)s / n;
        double cx = x0 + ddx * t, cy = y0 + ddy * t;
        int pxA = (int)floor(cx - dis) - xA, pxB = (int)ceil(cx + dis) - xA;
        int pyA = (int)floor(cy - dis) - yA, pyB = (int)ceil(cy + dis) - yA;
        if (pxA < 0) pxA = 0;
        if (pyA < 0) pyA = 0;
        if (pxB >= w) pxB = w - 1;
        if (pyB >= h) pyB = h - 1;
        for (int py = pyA; py <= pyB; py++) {
            BYTE* satir = buf + (size_t)py * adim;
            double qy = py + yA + 0.5 - cy;            // piksel merkezine gore
            for (int px = pxA; px <= pxB; px++) {
                double qx = px + xA + 0.5 - cx;
                double d = sqrt(qx * qx + qy * qy);
                double a = r + 0.75 - d;               // kenarda ~1.5 px yumusak gecis
                if (a <= 0.0) continue;
                if (a > 1.0) a = 1.0;
                BYTE* p = satir + (size_t)px * 4;
                p[0] = (BYTE)(p[0] * (1 - a) + nb * a);
                p[1] = (BYTE)(p[1] * (1 - a) + ng * a);
                p[2] = (BYTE)(p[2] * (1 - a) + nr * a);
            }
        }
    }
    BolgeYaz(xA, yA, w, h);
}

// --- G-Pen: hiza duyarli murekkep kalemi (manga/anime murekkeplemesi) ---
// Yavas surukleme kalin, hizli surukleme (fiske) ince cizgi uretir;
// kalinlik gecisleri yumusatilir, boylece gercek G-pen ucu hissi verir.
static void GPenVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    DWORD simdi = GetTickCount();
    double dx = x1 - x0, dy = y1 - y0;
    double dis = sqrt(dx * dx + dy * dy);
    DWORD dt = simdi - g.gpenTick;
    if (dt == 0) dt = 1;
    g.gpenTick = simdi;
    double hiz = dis / dt;                            // piksel / ms
    double hedef = g.kalinlik * (1.15 - hiz * 0.35);  // hiz arttikca incelir
    double alt = g.kalinlik / 5.0;
    if (alt < 1.0) alt = 1.0;
    if (hedef < alt) hedef = alt;
    if (hedef > g.kalinlik) hedef = g.kalinlik;
    g.gpenKal += (hedef - g.gpenKal) * 0.45;          // yumusak gecis
    int k = (int)(g.gpenKal + 0.5);
    if (k < 1) k = 1;
    CizgiCiz(g.tuvalDC, x0, y0, x1, y1, renk, k);
    if (g.simetri)
        CizgiCiz(g.tuvalDC, g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk, k);
}

// --- Screentone: manga yarim-ton nokta deseni ---
// Noktalar tuval-sabit bir izgaraya oturur; ust uste vuruslarda desen
// kaymaz ve ayni nokta iki kez basilmaz (koyulasmaz) - gercek ton kagidi gibi.
static void TonDamga(int cx, int cy, COLORREF renk)
{
    int kal = g.kalinlik;
    int aralik = 8 + kal / 2;                         // izgara araligi
    int r = 16 + kal;                                 // firca yaricapi
    int noktaR = 2 + kal / 8;                         // nokta yaricapi
    int xA = cx - r, yA = cy - r;
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    int xB = cx + r, yB = cy + r;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    size_t adim = (size_t)g_bolgeW * 4;
    BYTE nb = GetBValue(renk), ng = GetGValue(renk), nr2 = GetRValue(renk);
    int r2 = r * r;
    for (int gy = (yA / aralik) * aralik; gy <= yB; gy += aralik) {
        for (int gx = (xA / aralik) * aralik; gx <= xB; gx += aralik) {
            int ddx = gx - cx, ddy = gy - cy;
            if (ddx * ddx + ddy * ddy > r2) continue;
            for (int oy = -noktaR; oy <= noktaR; oy++) {
                int py = gy + oy - yA;
                if (py < 0 || py >= h) continue;
                for (int ox = -noktaR; ox <= noktaR; ox++) {
                    if (ox * ox + oy * oy > noktaR * noktaR) continue;
                    int px = gx + ox - xA;
                    if (px < 0 || px >= w) continue;
                    BYTE* p = buf + (size_t)py * adim + (size_t)px * 4;
                    p[0] = nb; p[1] = ng; p[2] = nr2;
                }
            }
        }
    }
    BolgeYaz(xA, yA, w, h);
}

static void TonVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    double dx = x1 - x0, dy = y1 - y0;
    double len = sqrt(dx * dx + dy * dy);
    int aralik = 8 + g.kalinlik / 2;
    int n = (int)(len / (aralik / 2.0)) + 1;          // izgarayla ayni ritim
    for (int i = 0; i <= n; i++) {
        double t = (double)i / n;
        int px = (int)(x0 + dx * t + 0.5), py = (int)(y0 + dy * t + 0.5);
        TonDamga(px, py, renk);
        if (g.simetri) TonDamga(g.tuvalW - 1 - px, py, renk);
    }
}

// Noktanin dogru parcasina olan uzakligi
static double NoktaSegmanUzaklik(double px, double py, double x0, double y0, double x1, double y1)
{
    double dx = x1 - x0, dy = y1 - y0;
    double L2 = dx * dx + dy * dy;
    double t = L2 > 0 ? ((px - x0) * dx + (py - y0) * dy) / L2 : 0;
    if (t < 0) t = 0; else if (t > 1) t = 1;
    double qx = x0 + t * dx - px, qy = y0 + t * dy - py;
    return sqrt(qx * qx + qy * qy);
}

// Piksel bazli deterministik gurultu (sulu boya dokusu icin)
static unsigned PikselGurultu(int x, int y)
{
    unsigned h = (unsigned)x * 2654435761u ^ (unsigned)y * 2246822519u;
    h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return h;
}

// --- Sulu boya: saydam, kenarlari dagink, suyla yayilmis damga ---
static void SuluBoyaDamga(int x0, int y0, int x1, int y1, COLORREF renk)
{
    int r = 3 + g.kalinlik * 2;
    int xA = (x0 < x1 ? x0 : x1) - r, xB = (x0 > x1 ? x0 : x1) + r;
    int yA = (y0 < y1 ? y0 : y1) - r, yB = (y0 > y1 ? y0 : y1) + r;
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    int nr = GetRValue(renk), ng = GetGValue(renk), nb = GetBValue(renk);
    size_t adim = (size_t)g_bolgeW * 4;   // DIB section satir genisligi
    for (int py = 0; py < h; py++) {
        BYTE* satir = buf + (size_t)py * adim;
        for (int px = 0; px < w; px++) {
            double d = NoktaSegmanUzaklik(xA + px, yA + py, x0, y0, x1, y1);
            if (d >= r) continue;
            double t = 1.0 - d / r;
            double a = t * t * 0.20;                       // cok saydam katman
            unsigned gu = PikselGurultu(xA + px, yA + py) & 1023;
            a *= 0.55 + 0.65 * (gu / 1023.0);              // grenli, su lekesi kenari
            BYTE* p = satir + (size_t)px * 4;
            p[0] = (BYTE)(p[0] * (1 - a) + nb * a);
            p[1] = (BYTE)(p[1] * (1 - a) + ng * a);
            p[2] = (BYTE)(p[2] * (1 - a) + nr * a);
        }
    }
    BolgeYaz(xA, yA, w, h);
}

static void SuluBoyaVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    SuluBoyaDamga(x0, y0, x1, y1, renk);
    if (g.simetri)
        SuluBoyaDamga(g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk);
}

// --- Islak karisim: firca tuvaldeki boyayi surukler, rengi tasinir ---
static void IslakBolge(int x0, int y0, int x1, int y1)
{
    int r = 4 + g.kalinlik * 2;
    int xA = (x0 < x1 ? x0 : x1) - r, xB = (x0 > x1 ? x0 : x1) + r;
    int yA = (y0 < y1 ? y0 : y1) - r, yB = (y0 > y1 ? y0 : y1) + r;
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    int tr = GetRValue(g.islakRenk), tg = GetGValue(g.islakRenk), tb = GetBValue(g.islakRenk);
    long topR = 0, topG = 0, topB = 0; int sayac = 0;
    double yuk = 0.15 + 0.45 * (double)g.islakBoya;   // firca bosaldikca iz solar
    size_t adim = (size_t)g_bolgeW * 4;
    for (int py = 0; py < h; py++) {
        BYTE* satir = buf + (size_t)py * adim;
        for (int px = 0; px < w; px++) {
            double d = NoktaSegmanUzaklik(xA + px, yA + py, x0, y0, x1, y1);
            if (d >= r) continue;
            double t = 1.0 - d / r;
            double a = t * yuk;                              // tasinan boyayla karistir
            BYTE* p = satir + (size_t)px * 4;
            topB += p[0]; topG += p[1]; topR += p[2]; sayac++;
            p[0] = (BYTE)(p[0] * (1 - a) + tb * a);
            p[1] = (BYTE)(p[1] * (1 - a) + tg * a);
            p[2] = (BYTE)(p[2] * (1 - a) + tr * a);
        }
    }
    BolgeYaz(xA, yA, w, h);
    if (sayac > 0) {
        // Gercek islak firca gibi: boyali yerden guclu, bos kagittan
        // neredeyse hic boya toplamaz (boylece rengi beyaza solmaz)
        int ortR = (int)(topR / sayac), ortG = (int)(topG / sayac), ortB = (int)(topB / sayac);
        double lum = (ortR + ortG + ortB) / 3.0;
        float pigment = (float)((255.0 - lum) / 255.0);
        float toplama = 0.35f * pigment;
        if (toplama > 0.01f)
            g.islakRenk = RenkKaristir(g.islakRenk, RGB(ortR, ortG, ortB), toplama);
    }
}

static void IslakVurus(int x0, int y0, int x1, int y1)
{
    IslakBolge(x0, y0, x1, y1);
    if (g.simetri)
        IslakBolge(g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1);
    // fircadaki boya mesafeyle azalir (uzun ama sonunda solan vuruslar)
    double mesafe = sqrt((double)(x1 - x0) * (x1 - x0) + (double)(y1 - y0) * (y1 - y0));
    g.islakBoya -= (float)(mesafe * 0.0025);
    if (g.islakBoya < 0.15f) g.islakBoya = 0.15f;   // hic tam kurumaz, soluk iz surer
}

// --- Yagli boya: kalin, kabarik (impasto) dokulu damga ---
static void YagliDamga(int cx, int cy, HBRUSH koyu, HBRUSH ana, HBRUSH acik, int r)
{
    HBRUSH eski = (HBRUSH)SelectObject(g.tuvalDC, koyu);
    Ellipse(g.tuvalDC, cx - r + 1, cy - r + 2, cx + r + 1, cy + r + 2);  // koyu taban (golge)
    SelectObject(g.tuvalDC, ana);
    Ellipse(g.tuvalDC, cx - r, cy - r, cx + r, cy + r);                  // ana govde
    SelectObject(g.tuvalDC, acik);
    int vr = r / 2 + 1;
    Ellipse(g.tuvalDC, cx - vr, cy - vr - 1, cx + vr - 1, cy + vr - 1);  // isik vurgusu
    SelectObject(g.tuvalDC, eski);
}

static void YagliVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    int r = 2 + g.kalinlik;
    HBRUSH koyu = CreateSolidBrush(RenkKaristir(renk, RGB(0, 0, 0), 0.45f));
    HBRUSH ana  = CreateSolidBrush(renk);
    HBRUSH acik = CreateSolidBrush(RenkKaristir(renk, RGB(255, 255, 255), 0.40f));
    HPEN eskiP = (HPEN)SelectObject(g.tuvalDC, GetStockObject(NULL_PEN));
    double dx = x1 - x0, dy = y1 - y0;
    double len = sqrt(dx * dx + dy * dy);
    int adimAralik = r / 2 > 1 ? r / 2 : 1;
    int adim = (int)(len / adimAralik) + 1;
    for (int i = 0; i <= adim; i++) {
        double t = (double)i / adim;
        int px = (int)(x0 + dx * t + 0.5), py = (int)(y0 + dy * t + 0.5);
        YagliDamga(px, py, koyu, ana, acik, r);
        if (g.simetri) YagliDamga(g.tuvalW - 1 - px, py, koyu, ana, acik, r);
    }
    SelectObject(g.tuvalDC, eskiP);
    DeleteObject(koyu); DeleteObject(ana); DeleteObject(acik);
}

// --- Karakalem: grenli, yavas harekette koyulasan benekli cizgi ---
static void KarakalemDamga(int cx, int cy, COLORREF renk, int yogunluk)
{
    int r = g.kalinlik;
    // Eskiden her benek icin GetPixel/SetPixel cagrilirdi; artik bolge tek
    // seferde bellege alinip benekler RAM'de islenir (belirgin hizlanma)
    int xA = cx - r, yA = cy - r;
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    int xB = cx + r, yB = cy + r;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    int nr = GetRValue(renk), ng = GetGValue(renk), nb = GetBValue(renk);
    size_t adim = (size_t)g_bolgeW * 4;
    for (int i = 0; i < yogunluk; i++) {
        int dx = (rand() % (2 * r + 1)) - r;
        int dy = (rand() % (2 * r + 1)) - r;
        if (dx * dx + dy * dy > r * r) continue;
        int px = cx + dx - xA, py = cy + dy - yA;
        if (px < 0 || py < 0 || px >= w || py >= h) continue;
        float a = 0.12f + (rand() % 100) / 100.0f * 0.35f;   // kagit dokusu hissi
        BYTE* p = buf + (size_t)py * adim + (size_t)px * 4;
        p[0] = (BYTE)(p[0] * (1 - a) + nb * a);
        p[1] = (BYTE)(p[1] * (1 - a) + ng * a);
        p[2] = (BYTE)(p[2] * (1 - a) + nr * a);
    }
    BolgeYaz(xA, yA, w, h);
}

static void KarakalemVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    double dx = x1 - x0, dy = y1 - y0;
    double len = sqrt(dx * dx + dy * dy);
    int adim = (int)len + 1;
    int yogun = 2 + g.kalinlik;
    if (len < 4) yogun *= 2;   // yavas surukleme: kursun koyu iz birakir
    for (int i = 0; i <= adim; i++) {
        double t = (double)i / adim;
        int px = (int)(x0 + dx * t), py = (int)(y0 + dy * t);
        KarakalemDamga(px, py, renk, yogun);
        if (g.simetri) KarakalemDamga(g.tuvalW - 1 - px, py, renk, yogun);
    }
}

// --- Kaligrafi: hareket yonune gore incelip kalinlasan kesik uc ---
static void KaligrafiCizgi(int x0, int y0, int x1, int y1, COLORREF renk)
{
    double dx = x1 - x0, dy = y1 - y0;
    double len = sqrt(dx * dx + dy * dy);
    double gen;
    if (len < 1.0) {
        gen = g.kalinlik * 1.2;                            // tek tiklama: dolma nokta
    } else {
        double aci = atan2(dy, dx);
        double k = fabs(sin(aci - 3.14159265358979 / 4.0));
        gen = g.kalinlik * (0.35 + 0.75 * k) * 1.6;        // caprazda kalin, dikte ince
    }
    int w = (int)(gen + 0.5); if (w < 1) w = 1;
    HPEN p = CreatePen(PS_SOLID, w, renk);
    HPEN eskiP = (HPEN)SelectObject(g.tuvalDC, p);
    MoveToEx(g.tuvalDC, x0, y0, NULL);
    LineTo(g.tuvalDC, x1, y1);
    SelectObject(g.tuvalDC, eskiP);
    DeleteObject(p);
    NoktaKoy(g.tuvalDC, x1, y1, renk, w);                  // yuvarlak bitis
}

static void KaligrafiVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    KaligrafiCizgi(x0, y0, x1, y1, renk);
    if (g.simetri)
        KaligrafiCizgi(g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk);
}

// --- Serit firca: harekete dik bes paralel cizgi ---
static void SeritCizgi(int x0, int y0, int x1, int y1, COLORREF renk)
{
    double dx = x1 - x0, dy = y1 - y0;
    double len = sqrt(dx * dx + dy * dy);
    if (len < 0.5) { NoktaKoy(g.tuvalDC, x1, y1, renk, 1); return; }
    double nx = -dy / len, ny = dx / len;
    int bosluk = g.kalinlik > 1 ? g.kalinlik : 2;
    for (int k = -2; k <= 2; k++) {
        double o = k * bosluk;
        int ox = (int)(nx * o + (nx * o >= 0 ? 0.5 : -0.5));
        int oy = (int)(ny * o + (ny * o >= 0 ? 0.5 : -0.5));
        CizgiCiz(g.tuvalDC, x0 + ox, y0 + oy, x1 + ox, y1 + oy, renk, 1);
    }
}

static void SeritVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    SeritCizgi(x0, y0, x1, y1, renk);
    if (g.simetri)
        SeritCizgi(g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk);
}

// --- Neon: toplamsal (additive) parlama - isikli enerji efektleri icin ---
static void NeonDamga(int x0, int y0, int x1, int y1, COLORREF renk)
{
    int r = 4 + g.kalinlik * 2;
    int xA = (x0 < x1 ? x0 : x1) - r, xB = (x0 > x1 ? x0 : x1) + r;
    int yA = (y0 < y1 ? y0 : y1) - r, yB = (y0 > y1 ? y0 : y1) + r;
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    int nr = GetRValue(renk), ng = GetGValue(renk), nb = GetBValue(renk);
    size_t adim = (size_t)g_bolgeW * 4;
    int cekirdek = r * 35 / 100;
    for (int py = 0; py < h; py++) {
        BYTE* satir = buf + (size_t)py * adim;
        for (int px = 0; px < w; px++) {
            double d = NoktaSegmanUzaklik(xA + px, yA + py, x0, y0, x1, y1);
            if (d >= r) continue;
            double t = 1.0 - d / r;
            BYTE* p = satir + (size_t)px * 4;
            // dis hale: renkli yumusak karisim (beyaz zeminde de gorunur)
            double a = t * t * 0.60;
            int b2 = (int)(p[0] * (1 - a) + nb * a);
            int g2 = (int)(p[1] * (1 - a) + ng * a);
            int r2 = (int)(p[2] * (1 - a) + nr * a);
            // sicak cekirdek: merkez beyaza doner (neon tup gibi, koyu zeminde isilar)
            if (d < cekirdek) {
                double k = 1.0 - d / (cekirdek > 0 ? cekirdek : 1);
                int beyaz = (int)(k * k * 95);
                b2 += beyaz; g2 += beyaz; r2 += beyaz;
            }
            p[0] = (BYTE)(b2 > 255 ? 255 : b2);
            p[1] = (BYTE)(g2 > 255 ? 255 : g2);
            p[2] = (BYTE)(r2 > 255 ? 255 : r2);
        }
    }
    BolgeYaz(xA, yA, w, h);
}

static void NeonVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    NeonDamga(x0, y0, x1, y1, renk);
    if (g.simetri)
        NeonDamga(g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk);
}

// --- Golge: cok yumusak, dusuk opaklikli golgeleme fircasi ---
static void GolgeDamga(int x0, int y0, int x1, int y1, COLORREF renk)
{
    int r = 6 + g.kalinlik * 3;
    int xA = (x0 < x1 ? x0 : x1) - r, xB = (x0 > x1 ? x0 : x1) + r;
    int yA = (y0 < y1 ? y0 : y1) - r, yB = (y0 > y1 ? y0 : y1) + r;
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    int nr = GetRValue(renk), ng = GetGValue(renk), nb = GetBValue(renk);
    size_t adim = (size_t)g_bolgeW * 4;
    for (int py = 0; py < h; py++) {
        BYTE* satir = buf + (size_t)py * adim;
        for (int px = 0; px < w; px++) {
            double d = NoktaSegmanUzaklik(xA + px, yA + py, x0, y0, x1, y1);
            if (d >= r) continue;
            double t = 1.0 - d / r;
            double a = t * t * 0.055;   // cok hafif katman: ust uste golgelendir
            BYTE* p = satir + (size_t)px * 4;
            p[0] = (BYTE)(p[0] * (1 - a) + nb * a);
            p[1] = (BYTE)(p[1] * (1 - a) + ng * a);
            p[2] = (BYTE)(p[2] * (1 - a) + nr * a);
        }
    }
    BolgeYaz(xA, yA, w, h);
}

static void GolgeVurus(int x0, int y0, int x1, int y1, COLORREF renk)
{
    GolgeDamga(x0, y0, x1, y1, renk);
    if (g.simetri)
        GolgeDamga(g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, renk);
}

// --- Dither: birincil + ikincil rengi dama deseninde karistirir ---
// Desen tuval koordinatina sabitlidir; ust uste surmek koyuluk BIRIKTIRMEZ,
// bu yuzden ara tonlar (golgeleme) cok rahat yapilir. Piksel sanati klasiği.
static void DitherDamga(int x0, int y0, int x1, int y1, COLORREF renkA, COLORREF renkB)
{
    int r = g.kalinlik <= 1 ? 1 : g.kalinlik / 2 + 1;
    int xA = (x0 < x1 ? x0 : x1) - r, xB = (x0 > x1 ? x0 : x1) + r;
    int yA = (y0 < y1 ? y0 : y1) - r, yB = (y0 > y1 ? y0 : y1) + r;
    if (xA < 0) xA = 0;
    if (yA < 0) yA = 0;
    if (xB >= g.tuvalW) xB = g.tuvalW - 1;
    if (yB >= g.tuvalH) yB = g.tuvalH - 1;
    int w = xB - xA + 1, h = yB - yA + 1;
    if (w <= 0 || h <= 0) return;
    BYTE* buf = BolgeAl(xA, yA, w, h);
    if (!buf) return;
    size_t adim = (size_t)g_bolgeW * 4;
    for (int py = 0; py < h; py++) {
        BYTE* satir = buf + (size_t)py * adim;
        for (int px = 0; px < w; px++) {
            double d = NoktaSegmanUzaklik(xA + px, yA + py, x0, y0, x1, y1);
            if (d > r) continue;
            BYTE* p = satir + (size_t)px * 4;
            COLORREF c = (((xA + px) + (yA + py)) & 1) ? renkB : renkA;
            p[0] = GetBValue(c);
            p[1] = GetGValue(c);
            p[2] = GetRValue(c);
        }
    }
    BolgeYaz(xA, yA, w, h);
}

static void DitherVurus(int x0, int y0, int x1, int y1, COLORREF a, COLORREF b)
{
    DitherDamga(x0, y0, x1, y1, a, b);
    if (g.simetri)
        DitherDamga(g.tuvalW - 1 - x0, y0, g.tuvalW - 1 - x1, y1, a, b);
}

// ------------------------------------------------------------
//  Sekil cizimi (onizleme ve kesin cizim ortak)
// ------------------------------------------------------------
static RECT NormalRect(POINT a, POINT b)
{
    RECT r;
    r.left   = a.x < b.x ? a.x : b.x;
    r.top    = a.y < b.y ? a.y : b.y;
    r.right  = a.x > b.x ? a.x : b.x;
    r.bottom = a.y > b.y ? a.y : b.y;
    return r;
}

static bool SekilMi(Arac a)
{
    return a == ARAC_CIZGI || a == ARAC_DIKDORTGEN || a == ARAC_ELIPS ||
           a == ARAC_UCGEN || a == ARAC_YUVARLAK || a == ARAC_YILDIZ ||
           a == ARAC_ELMAS || a == ARAC_BESGEN || a == ARAC_ALTIGEN ||
           a == ARAC_OK || a == ARAC_KALP ||
           a == ARAC_HIZCIZGI || a == ARAC_BALON || a == ARAC_PANEL;
}

// Duzenli cokgen noktalari (ustten baslar)
static void CokgenNoktalar(RECT r, int n, POINT* p)
{
    double cx = (r.left + r.right) / 2.0, cy = (r.top + r.bottom) / 2.0;
    double rx = (r.right - r.left) / 2.0, ry = (r.bottom - r.top) / 2.0;
    for (int i = 0; i < n; i++) {
        double aci = -3.14159265358979 / 2 + i * 2.0 * 3.14159265358979 / n;
        p[i].x = (LONG)(cx + cos(aci) * rx);
        p[i].y = (LONG)(cy + sin(aci) * ry);
    }
}

static void YildizNoktalari(RECT r, POINT* p)
{
    double cx = (r.left + r.right) / 2.0, cy = (r.top + r.bottom) / 2.0;
    double rx = (r.right - r.left) / 2.0, ry = (r.bottom - r.top) / 2.0;
    for (int i = 0; i < 10; i++) {
        double aci = -3.14159265358979 / 2 + i * 3.14159265358979 / 5;
        double oran = (i % 2 == 0) ? 1.0 : 0.382;
        p[i].x = (LONG)(cx + cos(aci) * rx * oran);
        p[i].y = (LONG)(cy + sin(aci) * ry * oran);
    }
}

static void SekilCiz(HDC dc, Arac arac, POINT a, POINT b, COLORREF renk, int kal, bool dolu)
{
    HPEN p = CreatePen(PS_SOLID, kal > 0 ? kal : 1, renk);
    HPEN eskiP = (HPEN)SelectObject(dc, p);
    HBRUSH f = dolu ? CreateSolidBrush(renk) : (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH eskiF = (HBRUSH)SelectObject(dc, f);
    RECT r = NormalRect(a, b);

    switch (arac) {
    case ARAC_CIZGI:
        CizgiCiz(dc, a.x, a.y, b.x, b.y, renk, kal);
        break;
    case ARAC_DIKDORTGEN:
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        break;
    case ARAC_ELIPS:
        Ellipse(dc, r.left, r.top, r.right, r.bottom);
        break;
    case ARAC_UCGEN: {
        POINT u[3] = { {(r.left + r.right) / 2, r.top}, {r.left, r.bottom}, {r.right, r.bottom} };
        Polygon(dc, u, 3);
        break;
    }
    case ARAC_YUVARLAK: {
        int w = r.right - r.left, h = r.bottom - r.top;
        int rr = (w < h ? w : h) / 4;
        RoundRect(dc, r.left, r.top, r.right, r.bottom, rr, rr);
        break;
    }
    case ARAC_YILDIZ: {
        POINT y[10];
        YildizNoktalari(r, y);
        Polygon(dc, y, 10);
        break;
    }
    case ARAC_ELMAS: {
        POINT e[4] = { {(r.left + r.right) / 2, r.top}, {r.right, (r.top + r.bottom) / 2},
                       {(r.left + r.right) / 2, r.bottom}, {r.left, (r.top + r.bottom) / 2} };
        Polygon(dc, e, 4);
        break;
    }
    case ARAC_BESGEN: {
        POINT b5[5];
        CokgenNoktalar(r, 5, b5);
        Polygon(dc, b5, 5);
        break;
    }
    case ARAC_ALTIGEN: {
        POINT a6[6];
        CokgenNoktalar(r, 6, a6);
        Polygon(dc, a6, 6);
        break;
    }
    case ARAC_OK: {
        double dx = b.x - a.x, dy = b.y - a.y;
        double len = sqrt(dx * dx + dy * dy);
        if (len < 2) len = 2;
        double ux = dx / len, uy = dy / len;   // birim yon
        double nx = -uy, ny = ux;              // dik vektor
        double sw = len * 0.10;                // govde yarim genisligi
        double hw = len * 0.26;                // ok basi yarim genisligi
        double hl = len * 0.65;                // ok basinin basladigi yer
        POINT ok[7] = {
            { (LONG)(a.x + nx * sw),          (LONG)(a.y + ny * sw) },
            { (LONG)(a.x + ux * hl + nx * sw), (LONG)(a.y + uy * hl + ny * sw) },
            { (LONG)(a.x + ux * hl + nx * hw), (LONG)(a.y + uy * hl + ny * hw) },
            { b.x, b.y },
            { (LONG)(a.x + ux * hl - nx * hw), (LONG)(a.y + uy * hl - ny * hw) },
            { (LONG)(a.x + ux * hl - nx * sw), (LONG)(a.y + uy * hl - ny * sw) },
            { (LONG)(a.x - nx * sw),           (LONG)(a.y - ny * sw) }
        };
        Polygon(dc, ok, 7);
        break;
    }
    case ARAC_KALP: {
        // Parametrik kalp egrisi
        POINT kp[48];
        double cx = (r.left + r.right) / 2.0, cy = (r.top + r.bottom) / 2.0;
        double rx = (r.right - r.left) / 2.0, ry = (r.bottom - r.top) / 2.0;
        for (int i = 0; i < 48; i++) {
            double t = i * 2.0 * 3.14159265358979 / 48;
            double kx = 16 * sin(t) * sin(t) * sin(t);
            double ky = 13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t);
            kp[i].x = (LONG)(cx + kx * rx / 17.0);
            kp[i].y = (LONG)(cy - ky * ry / 17.0);
        }
        Polygon(dc, kp, 48);
        break;
    }
    case ARAC_HIZCIZGI: {
        // Hiz cizgileri: surukleme merkezinden disa dogru radyal cizgiler
        // (manga aksiyon efekti). Tohum uclardan uretilir; boylece onizleme
        // her boyamada ayni cizgileri gosterir (titresim olmaz).
        double dx = b.x - a.x, dy = b.y - a.y;
        double len = sqrt(dx * dx + dy * dy);
        if (len < 4) break;
        unsigned tohum = (unsigned)(a.x * 73856093) ^ (unsigned)(a.y * 19349663) ^
                         (unsigned)(b.x * 83492791) ^ (unsigned)(b.y * 2654435761u);
        const int N = 42;
        for (int i = 0; i < N; i++) {
            tohum = tohum * 1664525u + 1013904223u;   // LCG: tekrarlanabilir
            double aci = (i + ((tohum >> 8) % 100) / 100.0) * 2.0 * 3.14159265358979 / N;
            tohum = tohum * 1664525u + 1013904223u;
            double ic = len * (0.15 + 0.25 * ((tohum >> 8) % 100) / 100.0);
            tohum = tohum * 1664525u + 1013904223u;
            double dis2 = len * (0.75 + 0.25 * ((tohum >> 8) % 100) / 100.0);
            CizgiCiz(dc, (int)(a.x + cos(aci) * ic),  (int)(a.y + sin(aci) * ic),
                         (int)(a.x + cos(aci) * dis2), (int)(a.y + sin(aci) * dis2), renk, kal);
        }
        break;
    }
    case ARAC_BALON: {
        // Konusma balonu: elips govde + suruklemenin BASLANGICINA bakan
        // kuyruk. Manga gelenegi: govde her zaman ikincil renkle (beyaz)
        // doldurulur, cerceve birincil renkle cizilir.
        HBRUSH fb = CreateSolidBrush(g.renk2);
        HBRUSH eski2 = (HBRUSH)SelectObject(dc, fb);
        Ellipse(dc, r.left, r.top, r.right, r.bottom);
        double cx = (r.left + r.right) / 2.0, cy = (r.top + r.bottom) / 2.0;
        double rx = (r.right - r.left) / 2.0, ry = (r.bottom - r.top) / 2.0;
        double ux = a.x - cx, uy = a.y - cy;
        double ul = sqrt(ux * ux + uy * uy);
        if (ul > 4 && rx > 10 && ry > 10) {
            ux /= ul; uy /= ul;
            double nx2 = -uy, ny2 = ux;
            double bx = cx + ux * rx * 0.92, by = cy + uy * ry * 0.92;
            int ky = 6 + kal * 2;   // kuyruk taban genisligi
            POINT k[3] = {
                { (LONG)(bx + nx2 * ky), (LONG)(by + ny2 * ky) },
                { (LONG)a.x, (LONG)a.y },
                { (LONG)(bx - nx2 * ky), (LONG)(by - ny2 * ky) }
            };
            Polygon(dc, k, 3);
        }
        SelectObject(dc, eski2);
        DeleteObject(fb);
        break;
    }
    case ARAC_PANEL: {
        // Cizgi roman paneli: ici ikincil renkle (beyaz) silinen ve
        // kalin cerceveli kare. Alttaki cizimi kapatir, kare duzenini kurar.
        HBRUSH fb = CreateSolidBrush(g.renk2);
        HBRUSH eski2 = (HBRUSH)SelectObject(dc, fb);
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        SelectObject(dc, eski2);
        DeleteObject(fb);
        HPEN kp2 = CreatePen(PS_SOLID, kal + 2, renk);
        HPEN eskiK2 = (HPEN)SelectObject(dc, kp2);
        HBRUSH bos = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH eskiB = (HBRUSH)SelectObject(dc, bos);
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        SelectObject(dc, eskiB);
        SelectObject(dc, eskiK2);
        DeleteObject(kp2);
        break;
    }
    default: break;
    }
    SelectObject(dc, eskiF);
    if (dolu) DeleteObject(f);
    SelectObject(dc, eskiP);
    DeleteObject(p);
}

// ------------------------------------------------------------
//  32-bit piksel erisimi (efektler icin)
// ------------------------------------------------------------
static BYTE* TuvalPikselAl()
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g.tuvalW;
    bi.bmiHeader.biHeight = -g.tuvalH;   // ustten alta
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    BYTE* buf = (BYTE*)malloc((size_t)g.tuvalW * g.tuvalH * 4);
    if (buf) {
        GetDIBits(g.tuvalDC, g.tuvalBMP, 0, g.tuvalH, buf, &bi, DIB_RGB_COLORS);
        // GDI, 32bpp bitmaplerde 4. bayti (alfa) 0 birakir. Tuvaldeki her
        // piksel kavramsal olarak opaktir; aksi halde PNG/TIFF gibi alfa
        // tasiyan bicimlere kayitta resim tamamen seffaf cikar.
        for (size_t i = 3; i < (size_t)g.tuvalW * g.tuvalH * 4; i += 4)
            buf[i] = 255;
    }
    return buf;
}

static void TuvalPikselYaz(BYTE* buf)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g.tuvalW;
    bi.bmiHeader.biHeight = -g.tuvalH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetDIBits(g.tuvalDC, g.tuvalBMP, 0, g.tuvalH, buf, &bi, DIB_RGB_COLORS);
    free(buf);
}

static int Sinirla(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// ------------------------------------------------------------
//  Efektler
// ------------------------------------------------------------
static void EfektUygula(int idx)
{
    BYTE* buf = TuvalPikselAl();
    if (!buf) return;
    int w = g.tuvalW, h = g.tuvalH;
    GecmisPush();
    bool uygulandi = true;   // ara bellek ayrilamazsa false: bos kayit atilir

    switch (idx) {
    case 0: // Gri tonlama
        for (int i = 0; i < w * h * 4; i += 4) {
            int g2 = (buf[i] * 114 + buf[i+1] * 587 + buf[i+2] * 299) / 1000;
            buf[i] = buf[i+1] = buf[i+2] = (BYTE)g2;
        }
        break;
    case 1: // Negatif
        for (int i = 0; i < w * h * 4; i += 4) {
            buf[i] = 255 - buf[i]; buf[i+1] = 255 - buf[i+1]; buf[i+2] = 255 - buf[i+2];
        }
        break;
    case 2: // Sepya
        for (int i = 0; i < w * h * 4; i += 4) {
            int b = buf[i], gr = buf[i+1], r = buf[i+2];
            buf[i+2] = (BYTE)Sinirla((r * 393 + gr * 769 + b * 189) / 1000);
            buf[i+1] = (BYTE)Sinirla((r * 349 + gr * 686 + b * 168) / 1000);
            buf[i]   = (BYTE)Sinirla((r * 272 + gr * 534 + b * 131) / 1000);
        }
        break;
    case 3: { // Bulaniklastir (3x3 kutu bulanikligi)
        BYTE* kopya = (BYTE*)malloc((size_t)w * h * 4);
        if (!kopya) { uygulandi = false; break; }
        memcpy(kopya, buf, (size_t)w * h * 4);
        for (int y = 1; y < h - 1; y++)
            for (int x = 1; x < w - 1; x++) {
                int sb = 0, sg = 0, sr = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int o = ((y + dy) * w + (x + dx)) * 4;
                        sb += kopya[o]; sg += kopya[o+1]; sr += kopya[o+2];
                    }
                int o = (y * w + x) * 4;
                buf[o] = (BYTE)(sb / 9); buf[o+1] = (BYTE)(sg / 9); buf[o+2] = (BYTE)(sr / 9);
            }
        free(kopya);
        break;
    }
    case 4: { // Keskinlestir
        BYTE* kopya = (BYTE*)malloc((size_t)w * h * 4);
        if (!kopya) { uygulandi = false; break; }
        memcpy(kopya, buf, (size_t)w * h * 4);
        for (int y = 1; y < h - 1; y++)
            for (int x = 1; x < w - 1; x++) {
                int o = (y * w + x) * 4;
                int u = ((y - 1) * w + x) * 4, a = ((y + 1) * w + x) * 4;
                int s = (y * w + x - 1) * 4, g2 = (y * w + x + 1) * 4;
                for (int c = 0; c < 3; c++)
                    buf[o+c] = (BYTE)Sinirla(5 * kopya[o+c] - kopya[u+c] - kopya[a+c] - kopya[s+c] - kopya[g2+c]);
            }
        free(kopya);
        break;
    }
    case 5: { // Piksellestir (8x8 blok)
        int B = 8;
        for (int by = 0; by < h; by += B)
            for (int bx = 0; bx < w; bx += B) {
                int sb = 0, sg = 0, sr = 0, n = 0;
                for (int y = by; y < by + B && y < h; y++)
                    for (int x = bx; x < bx + B && x < w; x++) {
                        int o = (y * w + x) * 4;
                        sb += buf[o]; sg += buf[o+1]; sr += buf[o+2]; n++;
                    }
                for (int y = by; y < by + B && y < h; y++)
                    for (int x = bx; x < bx + B && x < w; x++) {
                        int o = (y * w + x) * 4;
                        buf[o] = (BYTE)(sb / n); buf[o+1] = (BYTE)(sg / n); buf[o+2] = (BYTE)(sr / n);
                    }
            }
        break;
    }
    case 6: // Parlaklik +
    case 7: { // Parlaklik -
        int d = (idx == 6) ? 25 : -25;
        for (int i = 0; i < w * h * 4; i += 4) {
            buf[i]   = (BYTE)Sinirla(buf[i] + d);
            buf[i+1] = (BYTE)Sinirla(buf[i+1] + d);
            buf[i+2] = (BYTE)Sinirla(buf[i+2] + d);
        }
        break;
    }
    case 8:   // Kabartma (emboss)
    case 9: { // Kenar bulma
        BYTE* kopya = (BYTE*)malloc((size_t)w * h * 4);
        if (!kopya) { uygulandi = false; break; }
        memcpy(kopya, buf, (size_t)w * h * 4);
        for (int y = 1; y < h - 1; y++)
            for (int x = 1; x < w - 1; x++) {
                int o = (y * w + x) * 4;
                for (int c = 0; c < 3; c++) {
                    int sol = kopya[o - 4 + c], sag = kopya[o + 4 + c];
                    int ust2 = kopya[o - w * 4 + c], alt = kopya[o + w * 4 + c];
                    int v;
                    if (idx == 8)
                        v = (sag - sol) + (alt - ust2) + 128;   // kabartma
                    else
                        v = 4 * kopya[o + c] - sol - sag - ust2 - alt + 128; // kenar
                    buf[o + c] = (BYTE)Sinirla(v);
                }
            }
        free(kopya);
        break;
    }
    case 10: // Posterlestir (4 kademe)
        for (int i = 0; i < w * h * 4; i += 4)
            for (int c = 0; c < 3; c++)
                buf[i + c] = (BYTE)(((buf[i + c] + 42) / 85) * 85);
        break;
    case 11: // Gren ekle
        for (int i = 0; i < w * h * 4; i += 4) {
            int d = (rand() % 41) - 20;
            buf[i]   = (BYTE)Sinirla(buf[i] + d);
            buf[i+1] = (BYTE)Sinirla(buf[i+1] + d);
            buf[i+2] = (BYTE)Sinirla(buf[i+2] + d);
        }
        break;
    case 12:  // Kontrast +
    case 13: { // Kontrast -
        double f = (idx == 12) ? 1.25 : 0.8;
        for (int i = 0; i < w * h * 4; i += 4)
            for (int c = 0; c < 3; c++)
                buf[i + c] = (BYTE)Sinirla((int)((buf[i + c] - 128) * f + 128));
        break;
    }
    case 14:  // Doygunluk +
    case 15: { // Doygunluk -
        double f = (idx == 14) ? 1.35 : 0.65;
        for (int i = 0; i < w * h * 4; i += 4) {
            int luma = (buf[i] * 114 + buf[i+1] * 587 + buf[i+2] * 299) / 1000;
            for (int c = 0; c < 3; c++)
                buf[i + c] = (BYTE)Sinirla((int)(luma + (buf[i + c] - luma) * f));
        }
        break;
    }
    case 16: { // Gauss bulanikligi (5x5, ayrik iki gecis)
        const int k[5] = {1, 4, 6, 4, 1};
        BYTE* t1 = (BYTE*)malloc((size_t)w * h * 4);
        if (!t1) { uygulandi = false; break; }
        for (int y = 0; y < h; y++)          // yatay gecis
            for (int x = 0; x < w; x++)
                for (int c = 0; c < 3; c++) {
                    int toplam = 0;
                    for (int kx = -2; kx <= 2; kx++) {
                        int sx = x + kx;
                        if (sx < 0) sx = 0;
                        if (sx >= w) sx = w - 1;
                        toplam += buf[(y * w + sx) * 4 + c] * k[kx + 2];
                    }
                    t1[(y * w + x) * 4 + c] = (BYTE)(toplam / 16);
                }
        for (int y = 0; y < h; y++)          // dikey gecis
            for (int x = 0; x < w; x++)
                for (int c = 0; c < 3; c++) {
                    int toplam = 0;
                    for (int ky = -2; ky <= 2; ky++) {
                        int sy = y + ky;
                        if (sy < 0) sy = 0;
                        if (sy >= h) sy = h - 1;
                        toplam += t1[(sy * w + x) * 4 + c] * k[ky + 2];
                    }
                    buf[(y * w + x) * 4 + c] = (BYTE)(toplam / 16);
                }
        free(t1);
        break;
    }
    case 17: { // Yagli boya efekti (en sik parlaklik kumesi)
        BYTE* kopya = (BYTE*)malloc((size_t)w * h * 4);
        if (!kopya) { uygulandi = false; break; }
        memcpy(kopya, buf, (size_t)w * h * 4);
        const int R = 2, SEVIYE = 16;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int sayac[SEVIYE] = {}, tb[SEVIYE] = {}, tg[SEVIYE] = {}, tr[SEVIYE] = {};
                for (int dy = -R; dy <= R; dy++)
                    for (int dx = -R; dx <= R; dx++) {
                        int sx = x + dx, sy = y + dy;
                        if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
                        int o = (sy * w + sx) * 4;
                        int sev = (kopya[o] * 114 + kopya[o+1] * 587 + kopya[o+2] * 299) * SEVIYE / 256000;
                        if (sev >= SEVIYE) sev = SEVIYE - 1;
                        sayac[sev]++;
                        tb[sev] += kopya[o]; tg[sev] += kopya[o+1]; tr[sev] += kopya[o+2];
                    }
                int en = 0;
                for (int i = 1; i < SEVIYE; i++)
                    if (sayac[i] > sayac[en]) en = i;
                int o = (y * w + x) * 4;
                if (sayac[en] > 0) {
                    buf[o]   = (BYTE)(tb[en] / sayac[en]);
                    buf[o+1] = (BYTE)(tg[en] / sayac[en]);
                    buf[o+2] = (BYTE)(tr[en] / sayac[en]);
                }
            }
        free(kopya);
        break;
    }
    case 18: // Solarize (koyu tonlar ayni, acik tonlar ters)
        for (int i = 0; i < w * h * 4; i += 4)
            for (int c = 0; c < 3; c++)
                if (buf[i + c] > 128) buf[i + c] = 255 - buf[i + c];
        break;
    case 19: // Kanal degistir (kirmizi <-> mavi)
        for (int i = 0; i < w * h * 4; i += 4) {
            BYTE t2 = buf[i]; buf[i] = buf[i + 2]; buf[i + 2] = t2;
        }
        break;
    }
    if (!uygulandi) {   // hicbir piksel degismedi: bos gecmis kaydini at
        GecmisPop();
        free(buf);
        return;
    }
    TuvalPikselYaz(buf);
    g.degisti = true;
    InvalidateRect(g.pencere, NULL, FALSE);
}

// ------------------------------------------------------------
//  Dondurme / aynalama
// ------------------------------------------------------------
static void Dondur90(bool saatYonu)
{
    BYTE* kaynak = TuvalPikselAl();
    if (!kaynak) return;
    int w = g.tuvalW, h = g.tuvalH;
    BYTE* hedef = (BYTE*)malloc((size_t)w * h * 4);
    if (!hedef) { free(kaynak); return; }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int nx, ny;
            if (saatYonu) { nx = h - 1 - y; ny = x; }
            else          { nx = y;         ny = w - 1 - x; }
            memcpy(hedef + ((size_t)ny * h + nx) * 4, kaynak + ((size_t)y * w + x) * 4, 4);
        }
    free(kaynak);
    // Boyut degisimi de geri alinabilir: once mevcut durumu gecmise it,
    // gecmisi koruyarak tuvale yeni boyut ver (eskiden tum gecmis silinirdi)
    GecmisPush();
    if (!TuvalOlustur(h, w, RGB(255, 255, 255), true)) {
        GecmisPop();        // tuval degismedi: bos kaydi geri cikar
        free(hedef);
        return;
    }
    TuvalPikselYaz(hedef);
    g.degisti = true;
    InvalidateRect(g.pencere, NULL, FALSE);
}

static void Aynala(bool yatay)
{
    BYTE* buf = TuvalPikselAl();
    if (!buf) return;
    int w = g.tuvalW, h = g.tuvalH;
    GecmisPush();
    if (yatay) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w / 2; x++) {
                BYTE* p1 = buf + ((size_t)y * w + x) * 4;
                BYTE* p2 = buf + ((size_t)y * w + (w - 1 - x)) * 4;
                for (int c = 0; c < 4; c++) { BYTE t = p1[c]; p1[c] = p2[c]; p2[c] = t; }
            }
    } else {
        BYTE* satir = (BYTE*)malloc((size_t)w * 4);
        if (!satir) {           // bellek yok: degisiklik yapilmadi, bos kaydi at
            GecmisPop();
            free(buf);
            return;
        }
        for (int y = 0; y < h / 2; y++) {
            BYTE* s1 = buf + (size_t)y * w * 4;
            BYTE* s2 = buf + (size_t)(h - 1 - y) * w * 4;
            memcpy(satir, s1, (size_t)w * 4);
            memcpy(s1, s2, (size_t)w * 4);
            memcpy(s2, satir, (size_t)w * 4);
        }
        free(satir);
    }
    TuvalPikselYaz(buf);
    g.degisti = true;
    InvalidateRect(g.pencere, NULL, FALSE);
}

// ------------------------------------------------------------
//  Dosya turu tespiti
// ------------------------------------------------------------
static int DosyaTuru(const wchar_t* yol)  // 1=BMP, 2=PNG, 3=JPEG, 4=GIF, 5=TIFF, 6=ICO, 0=bilinmiyor
{
    unsigned char m[8] = {};
    DWORD okunan = 0;
    HANDLE h = CreateFileW(yol, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        ReadFile(h, m, 8, &okunan, NULL);
        CloseHandle(h);
    }
    if (okunan >= 2 && m[0] == 'B' && m[1] == 'M') return 1;
    if (okunan >= 8 && m[0] == 0x89 && m[1] == 'P' && m[2] == 'N' && m[3] == 'G') return 2;
    if (okunan >= 3 && m[0] == 0xFF && m[1] == 0xD8 && m[2] == 0xFF) return 3;
    if (okunan >= 4 && m[0] == 'G' && m[1] == 'I' && m[2] == 'F' && m[3] == '8') return 4;
    if (okunan >= 4 && ((m[0] == 'I' && m[1] == 'I' && m[2] == 42 && m[3] == 0) ||
                        (m[0] == 'M' && m[1] == 'M' && m[2] == 0 && m[3] == 42))) return 5;
    if (okunan >= 4 && m[0] == 0 && m[1] == 0 && m[2] == 1 && m[3] == 0) return 6;
    const wchar_t* u = wcsrchr(yol, L'.');
    if (u) {
        if (_wcsicmp(u, L".png") == 0) return 2;
        if (_wcsicmp(u, L".bmp") == 0) return 1;
        if (_wcsicmp(u, L".jpg") == 0 || _wcsicmp(u, L".jpeg") == 0) return 3;
        if (_wcsicmp(u, L".gif") == 0) return 4;
        if (_wcsicmp(u, L".tif") == 0 || _wcsicmp(u, L".tiff") == 0) return 5;
        if (_wcsicmp(u, L".ico") == 0) return 6;
    }
    return 0;
}

// ------------------------------------------------------------
//  BMP kaydetme / yukleme
// ------------------------------------------------------------
static bool BmpKaydet(const wchar_t* yol)
{
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = g.tuvalW;
    bi.biHeight = g.tuvalH;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    int satirBayt = ((g.tuvalW * 3 + 3) / 4) * 4;
    int veriBoyut = satirBayt * g.tuvalH;
    BYTE* veri = (BYTE*)calloc(veriBoyut, 1);
    if (!veri) return false;

    BITMAPINFO bilgi = {};
    bilgi.bmiHeader = bi;
    GetDIBits(g.tuvalDC, g.tuvalBMP, 0, g.tuvalH, veri, &bilgi, DIB_RGB_COLORS);

    BITMAPFILEHEADER bh = {};
    bh.bfType = 0x4D42;
    bh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bh.bfSize = bh.bfOffBits + veriBoyut;

    HANDLE h = CreateFileW(yol, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { free(veri); return false; }
    DWORD yazilan;
    WriteFile(h, &bh, sizeof(bh), &yazilan, NULL);
    WriteFile(h, &bi, sizeof(bi), &yazilan, NULL);
    WriteFile(h, veri, veriBoyut, &yazilan, NULL);
    CloseHandle(h);
    free(veri);
    return true;
}

static bool BmpYukle(const wchar_t* yol)
{
    HANDLE h = CreateFileW(yol, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    BITMAPFILEHEADER bh;
    BITMAPINFOHEADER bi;
    DWORD okunan;
    if (!ReadFile(h, &bh, sizeof(bh), &okunan, NULL) || bh.bfType != 0x4D42 ||
        !ReadFile(h, &bi, sizeof(bi), &okunan, NULL)) {
        CloseHandle(h);
        return false;
    }
    if (bi.biCompression != BI_RGB || (bi.biBitCount != 24 && bi.biBitCount != 32)) {
        CloseHandle(h);
        return false;
    }
    int w = bi.biWidth;
    int hh = bi.biHeight;
    bool alttan = hh > 0;
    if (hh < 0) hh = -hh;
    // Bozuk/kotu niyetli dosyalara karsi sinir: asiri genislikler tamsayi
    // tasmansi ve bellek tukenmesi yaratirdi (WIC yukleyiciyle ayni sinir)
    if (w <= 0 || hh <= 0 || w > 8192 || hh > 8192) {
        CloseHandle(h);
        return false;
    }
    int baytRenk = bi.biBitCount / 8;
    int satirBayt = ((w * baytRenk + 3) / 4) * 4;
    BYTE* veri = (BYTE*)malloc((size_t)satirBayt * hh);
    if (!veri) { CloseHandle(h); return false; }
    SetFilePointer(h, bh.bfOffBits, NULL, FILE_BEGIN);
    ReadFile(h, veri, (DWORD)((size_t)satirBayt * hh), &okunan, NULL);
    CloseHandle(h);

    // Tuval yaratimi basarisiz olursa eski tuvalin ustune yazilmaz
    if (!TuvalOlustur(w, hh, RGB(255, 255, 255))) { free(veri); return false; }
    BITMAPINFO bilgi = {};
    bilgi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bilgi.bmiHeader.biWidth = w;
    bilgi.bmiHeader.biHeight = alttan ? hh : -hh;
    bilgi.bmiHeader.biPlanes = 1;
    bilgi.bmiHeader.biBitCount = bi.biBitCount;
    bilgi.bmiHeader.biCompression = BI_RGB;
    SetDIBits(g.tuvalDC, g.tuvalBMP, 0, hh, veri, &bilgi, DIB_RGB_COLORS);
    free(veri);
    return true;
}

// ------------------------------------------------------------
//  PNG kaydetme / yukleme (Windows WIC altyapisi)
// ------------------------------------------------------------
static bool PngKaydet(const wchar_t* yol)
{
    bool sonuc = false;
    IWICImagingFactory* fabrika = NULL;
    IWICStream* akis = NULL;
    IWICBitmapEncoder* kodlayici = NULL;
    IWICBitmapFrameEncode* cerceve = NULL;
    BYTE* pikseller = TuvalPikselAl();   // BGRA ustten alta
    if (!pikseller) return false;

    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&fabrika))) &&
        SUCCEEDED(fabrika->CreateStream(&akis)) &&
        SUCCEEDED(akis->InitializeFromFilename(yol, GENERIC_WRITE)) &&
        SUCCEEDED(fabrika->CreateEncoder(GUID_ContainerFormatPng, NULL, &kodlayici)) &&
        SUCCEEDED(kodlayici->Initialize(akis, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(kodlayici->CreateNewFrame(&cerceve, NULL)) &&
        SUCCEEDED(cerceve->Initialize(NULL)) &&
        SUCCEEDED(cerceve->SetSize(g.tuvalW, g.tuvalH))) {
        GUID pf = GUID_WICPixelFormat32bppBGRA;
        if (SUCCEEDED(cerceve->SetPixelFormat(&pf)) &&
            SUCCEEDED(cerceve->WritePixels(g.tuvalH, g.tuvalW * 4, g.tuvalW * g.tuvalH * 4, pikseller)) &&
            SUCCEEDED(cerceve->Commit()) &&
            SUCCEEDED(kodlayici->Commit()))
            sonuc = true;
    }
    if (cerceve) cerceve->Release();
    if (kodlayici) kodlayici->Release();
    if (akis) akis->Release();
    if (fabrika) fabrika->Release();
    free(pikseller);
    return sonuc;
}

// WIC ile goruntu yukleme: PNG, JPEG ve sistemin destekledigi diger
// bicimler (GIF, TIFF...) icin ortak yukleyici
static bool WicYukle(const wchar_t* yol)
{
    bool sonuc = false;
    IWICImagingFactory* fabrika = NULL;
    IWICBitmapDecoder* cozucu = NULL;
    IWICBitmapFrameDecode* cerceve = NULL;
    IWICFormatConverter* donusturucu = NULL;
    BYTE* buf = NULL;
    UINT w = 0, h = 0;

    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&fabrika))) &&
        SUCCEEDED(fabrika->CreateDecoderFromFilename(yol, NULL, GENERIC_READ,
                                                     WICDecodeMetadataCacheOnLoad, &cozucu)) &&
        SUCCEEDED(cozucu->GetFrame(0, &cerceve)) &&
        SUCCEEDED(cerceve->GetSize(&w, &h)) && w > 0 && h > 0 &&
        w <= 8192 && h <= 8192 &&   // asiri buyuk resimler bellegi tuketir
        SUCCEEDED(fabrika->CreateFormatConverter(&donusturucu)) &&
        SUCCEEDED(donusturucu->Initialize(cerceve, GUID_WICPixelFormat32bppBGRA,
                                          WICBitmapDitherTypeNone, NULL, 0.0,
                                          WICBitmapPaletteTypeCustom))) {
        buf = (BYTE*)malloc((size_t)w * h * 4);
        if (buf && SUCCEEDED(donusturucu->CopyPixels(NULL, w * 4, w * h * 4, buf)) &&
            TuvalOlustur(w, h, RGB(255, 255, 255))) {   // basarisizsa eski tuvale yazilmaz
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -(int)h;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            SetDIBits(g.tuvalDC, g.tuvalBMP, 0, h, buf, &bi, DIB_RGB_COLORS);
            sonuc = true;
        }
    }
    free(buf);
    if (donusturucu) donusturucu->Release();
    if (cerceve) cerceve->Release();
    if (cozucu) cozucu->Release();
    if (fabrika) fabrika->Release();
    return sonuc;
}

// JPEG kaydetme (WIC): alfa kanali desteklenmedigi icin 24bpp BGR'a
// donusturulur, kalite %90 olarak ayarlanir
static bool JpegKaydet(const wchar_t* yol)
{
    bool sonuc = false;
    IWICImagingFactory* fabrika = NULL;
    IWICStream* akis = NULL;
    IWICBitmapEncoder* kodlayici = NULL;
    IWICBitmapFrameEncode* cerceve = NULL;
    IPropertyBag2* ozellikler = NULL;
    BYTE* pikseller = TuvalPikselAl();   // 32bpp BGRA
    if (!pikseller) return false;

    int w = g.tuvalW, h = g.tuvalH;
    BYTE* bgr = (BYTE*)malloc((size_t)w * h * 3);
    if (!bgr) { free(pikseller); return false; }
    for (size_t i = 0, j = 0; i < (size_t)w * h * 4; i += 4, j += 3) {
        bgr[j] = pikseller[i]; bgr[j + 1] = pikseller[i + 1]; bgr[j + 2] = pikseller[i + 2];
    }
    free(pikseller);

    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&fabrika))) &&
        SUCCEEDED(fabrika->CreateStream(&akis)) &&
        SUCCEEDED(akis->InitializeFromFilename(yol, GENERIC_WRITE)) &&
        SUCCEEDED(fabrika->CreateEncoder(GUID_ContainerFormatJpeg, NULL, &kodlayici)) &&
        SUCCEEDED(kodlayici->Initialize(akis, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(kodlayici->CreateNewFrame(&cerceve, &ozellikler))) {
        PROPBAG2 pb = {};
        pb.pstrName = (LPOLESTR)L"ImageQuality";
        VARIANT v; VariantInit(&v);
        v.vt = VT_R4; v.fltVal = 0.90f;              // kalite %90
        ozellikler->Write(1, &pb, &v);
        GUID pf = GUID_WICPixelFormat24bppBGR;
        if (SUCCEEDED(cerceve->Initialize(ozellikler)) &&
            SUCCEEDED(cerceve->SetSize(w, h)) &&
            SUCCEEDED(cerceve->SetPixelFormat(&pf)) &&
            IsEqualGUID(pf, GUID_WICPixelFormat24bppBGR) &&
            SUCCEEDED(cerceve->WritePixels(h, (UINT)w * 3, (UINT)((size_t)w * h * 3), bgr)) &&
            SUCCEEDED(cerceve->Commit()) &&
            SUCCEEDED(kodlayici->Commit()))
            sonuc = true;
    }
    if (ozellikler) ozellikler->Release();
    if (cerceve) cerceve->Release();
    if (kodlayici) kodlayici->Release();
    if (akis) akis->Release();
    if (fabrika) fabrika->Release();
    free(bgr);
    return sonuc;
}

// GIF kaydetme (WIC): GIF en fazla 256 renk icerdiginden resim once
// 8bpp indeksli formata donusturulur. Palet resmin kendisinden turetilir
// (InitializeFromBitmap) ve hata yayilimli (Floyd-Steinberg) titretim ile
// kalite korunur.
static bool GifKaydet(const wchar_t* yol)
{
    bool sonuc = false;
    IWICImagingFactory* fabrika = NULL;
    IWICStream* akis = NULL;
    IWICBitmapEncoder* kodlayici = NULL;
    IWICBitmapFrameEncode* cerceve = NULL;
    IPropertyBag2* ozellikler = NULL;
    IWICBitmap* kaynak = NULL;
    IWICPalette* palet = NULL;
    IWICFormatConverter* donusturucu = NULL;
    BYTE* pikseller = TuvalPikselAl();   // 32bpp BGRA, ustten alta
    if (!pikseller) return false;

    UINT w = (UINT)g.tuvalW, h = (UINT)g.tuvalH;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&fabrika))) &&
        SUCCEEDED(fabrika->CreateBitmapFromMemory(w, h, GUID_WICPixelFormat32bppBGRA,
                                                  w * 4, w * h * 4, pikseller, &kaynak)) &&
        SUCCEEDED(fabrika->CreatePalette(&palet)) &&
        SUCCEEDED(palet->InitializeFromBitmap(kaynak, 256, FALSE)) &&
        SUCCEEDED(fabrika->CreateFormatConverter(&donusturucu)) &&
        SUCCEEDED(donusturucu->Initialize(kaynak, GUID_WICPixelFormat8bppIndexed,
                                          WICBitmapDitherTypeErrorDiffusion, palet, 0.0,
                                          WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(fabrika->CreateStream(&akis)) &&
        SUCCEEDED(akis->InitializeFromFilename(yol, GENERIC_WRITE)) &&
        SUCCEEDED(fabrika->CreateEncoder(GUID_ContainerFormatGif, NULL, &kodlayici)) &&
        SUCCEEDED(kodlayici->Initialize(akis, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(kodlayici->CreateNewFrame(&cerceve, &ozellikler))) {
        GUID pf = GUID_WICPixelFormat8bppIndexed;
        if (SUCCEEDED(cerceve->Initialize(ozellikler)) &&
            SUCCEEDED(kodlayici->SetPalette(palet)) &&   // GIF genel palet kullanir
            SUCCEEDED(cerceve->SetSize(w, h)) &&
            SUCCEEDED(cerceve->SetPixelFormat(&pf)) &&
            IsEqualGUID(pf, GUID_WICPixelFormat8bppIndexed) &&
            SUCCEEDED(cerceve->WriteSource(donusturucu, NULL)) &&
            SUCCEEDED(cerceve->Commit()) &&
            SUCCEEDED(kodlayici->Commit()))
            sonuc = true;
    }
    if (ozellikler) ozellikler->Release();
    if (cerceve) cerceve->Release();
    if (kodlayici) kodlayici->Release();
    if (akis) akis->Release();
    if (donusturucu) donusturucu->Release();
    if (palet) palet->Release();
    if (kaynak) kaynak->Release();
    if (fabrika) fabrika->Release();
    free(pikseller);
    return sonuc;
}

// TIFF kaydetme (WIC): 32bpp BGRA dogrudan desteklenir, donusum gerekmez.
// Kayipsiz LZW sikistirma uygulanir; boylece dosya boyutu makul kalir.
static bool TiffKaydet(const wchar_t* yol)
{
    bool sonuc = false;
    IWICImagingFactory* fabrika = NULL;
    IWICStream* akis = NULL;
    IWICBitmapEncoder* kodlayici = NULL;
    IWICBitmapFrameEncode* cerceve = NULL;
    IPropertyBag2* ozellikler = NULL;
    BYTE* pikseller = TuvalPikselAl();   // 32bpp BGRA, ustten alta
    if (!pikseller) return false;

    UINT w = (UINT)g.tuvalW, h = (UINT)g.tuvalH;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&fabrika))) &&
        SUCCEEDED(fabrika->CreateStream(&akis)) &&
        SUCCEEDED(akis->InitializeFromFilename(yol, GENERIC_WRITE)) &&
        SUCCEEDED(fabrika->CreateEncoder(GUID_ContainerFormatTiff, NULL, &kodlayici)) &&
        SUCCEEDED(kodlayici->Initialize(akis, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(kodlayici->CreateNewFrame(&cerceve, &ozellikler))) {
        PROPBAG2 pb = {};
        pb.pstrName = (LPOLESTR)L"TiffCompressionMethod";
        VARIANT v; VariantInit(&v);
        v.vt = VT_UI1; v.bVal = WICTiffCompressionLZW;   // kayipsiz sikistirma
        ozellikler->Write(1, &pb, &v);
        GUID pf = GUID_WICPixelFormat32bppBGRA;
        if (SUCCEEDED(cerceve->Initialize(ozellikler)) &&
            SUCCEEDED(cerceve->SetSize(w, h)) &&
            SUCCEEDED(cerceve->SetPixelFormat(&pf)) &&
            IsEqualGUID(pf, GUID_WICPixelFormat32bppBGRA) &&
            SUCCEEDED(cerceve->WritePixels(h, w * 4, w * h * 4, pikseller)) &&
            SUCCEEDED(cerceve->Commit()) &&
            SUCCEEDED(kodlayici->Commit()))
            sonuc = true;
    }
    if (ozellikler) ozellikler->Release();
    if (cerceve) cerceve->Release();
    if (kodlayici) kodlayici->Release();
    if (akis) akis->Release();
    if (fabrika) fabrika->Release();
    free(pikseller);
    return sonuc;
}

// ICO kaydetme: Windows, WIC ile ICO kodlayicisi sunmaz (yalniz cozucu
// vardir); bu yuzden ICO kapsayicisi burada elle yazilir. Goruntu kare
// olacak sekilde oran korunarak en fazla 256x256'ya olceklenir, beyaz
// zemine oturtulur ve govde olarak PNG verisi gomulur (PNG'li ICO,
// Windows Vista ve sonrasinin gecerli simge bicimidir).
static bool IcoKaydet(const wchar_t* yol)
{
    int w = g.tuvalW, h = g.tuvalH;
    int mx = (w > h) ? w : h;
    int taraf = (mx > 256) ? 256 : mx;
    int nw = (int)(w * (double)taraf / mx + 0.5);
    int nh = (int)(h * (double)taraf / mx + 0.5);
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;

    // Kare gecici bitmap: beyaz zemin + merkeze olceklenmis tuval
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = taraf;
    bi.bmiHeader.biHeight = -taraf;   // ustten alta
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HDC ekran = GetDC(NULL);
    HBITMAP bmp = CreateDIBSection(ekran, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp) { ReleaseDC(NULL, ekran); return false; }
    HDC dc = CreateCompatibleDC(ekran);
    HBITMAP eski = (HBITMAP)SelectObject(dc, bmp);
    RECT rc = { 0, 0, taraf, taraf };
    FillRect(dc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SetStretchBltMode(dc, HALFTONE);            // kaliteli kucultme
    SetBrushOrgEx(dc, 0, 0, NULL);
    StretchBlt(dc, (taraf - nw) / 2, (taraf - nh) / 2, nw, nh,
               g.tuvalDC, 0, 0, w, h, SRCCOPY);
    BYTE* pik = (BYTE*)malloc((size_t)taraf * taraf * 4);
    if (pik) {
        GetDIBits(dc, bmp, 0, taraf, pik, &bi, DIB_RGB_COLORS);
        for (size_t i = 3; i < (size_t)taraf * taraf * 4; i += 4)
            pik[i] = 255;                       // GDI alfa baytini 0 birakir
    }
    SelectObject(dc, eski);
    DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(NULL, ekran);
    if (!pik) return false;

    bool sonuc = false;
    IWICImagingFactory* fabrika = NULL;
    IWICStream* akis = NULL;
    IWICBitmapEncoder* kodlayici = NULL;
    IWICBitmapFrameEncode* cerceve = NULL;
    IStream* bellek = NULL;
    HGLOBAL hg = NULL;

    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&fabrika))) &&
        SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &bellek)) &&
        SUCCEEDED(fabrika->CreateStream(&akis)) &&
        SUCCEEDED(akis->InitializeFromIStream(bellek)) &&
        SUCCEEDED(fabrika->CreateEncoder(GUID_ContainerFormatPng, NULL, &kodlayici)) &&
        SUCCEEDED(kodlayici->Initialize(akis, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(kodlayici->CreateNewFrame(&cerceve, NULL)) &&
        SUCCEEDED(cerceve->Initialize(NULL)) &&
        SUCCEEDED(cerceve->SetSize(taraf, taraf))) {
        GUID pf = GUID_WICPixelFormat32bppBGRA;
        if (SUCCEEDED(cerceve->SetPixelFormat(&pf)) &&
            SUCCEEDED(cerceve->WritePixels(taraf, (UINT)taraf * 4,
                                           (UINT)((size_t)taraf * taraf * 4), pik)) &&
            SUCCEEDED(cerceve->Commit()) &&
            SUCCEEDED(kodlayici->Commit()) &&
            SUCCEEDED(GetHGlobalFromStream(bellek, &hg))) {
            DWORD boy = (DWORD)GlobalSize(hg);
            void* veri = GlobalLock(hg);
            if (veri && boy > 0) {
                HANDLE f = CreateFileW(yol, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
                if (f != INVALID_HANDLE_VALUE) {
                    BYTE kafa[22] = {};
                    kafa[2] = 1;                        // tip: simge (1=ICO)
                    kafa[4] = 1;                        // goruntu sayisi: 1
                    kafa[6] = (taraf >= 256) ? 0 : (BYTE)taraf;   // 0 = 256
                    kafa[7] = (taraf >= 256) ? 0 : (BYTE)taraf;
                    kafa[10] = 1;                       // renk duzlemi
                    kafa[12] = 32;                      // bpp
                    kafa[14] = (BYTE)(boy & 0xFF);
                    kafa[15] = (BYTE)((boy >> 8) & 0xFF);
                    kafa[16] = (BYTE)((boy >> 16) & 0xFF);
                    kafa[17] = (BYTE)((boy >> 24) & 0xFF);
                    kafa[18] = 22;                      // goruntu ofseti
                    DWORD yazilan = 0;
                    WriteFile(f, kafa, 22, &yazilan, NULL);
                    WriteFile(f, veri, boy, &yazilan, NULL);
                    CloseHandle(f);
                    sonuc = true;
                }
                GlobalUnlock(hg);
            }
        }
    }
    if (cerceve) cerceve->Release();
    if (kodlayici) kodlayici->Release();
    if (akis) akis->Release();
    if (bellek) bellek->Release();
    if (fabrika) fabrika->Release();
    free(pik);
    return sonuc;
}

// ------------------------------------------------------------
//  Referans goruntu (kopya kagidi katmani)
// ------------------------------------------------------------
static void ReferansSerbest()
{
    if (g.refDC) {
        SelectObject(g.refDC, g.refEski);
        DeleteObject(g.refBMP);
        DeleteDC(g.refDC);
        g.refDC = NULL; g.refBMP = NULL;
    }
    g.refVar = false; g.refGoster = false;
    g.refW = g.refH = 0;
}

static bool ReferansYukle(const wchar_t* yol)
{
    bool sonuc = false;
    IWICImagingFactory* fabrika = NULL;
    IWICBitmapDecoder* cozucu = NULL;
    IWICBitmapFrameDecode* cerceve = NULL;
    IWICFormatConverter* donusturucu = NULL;
    BYTE* buf = NULL;
    UINT w = 0, h = 0;

    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&fabrika))) &&
        SUCCEEDED(fabrika->CreateDecoderFromFilename(yol, NULL, GENERIC_READ,
                                                     WICDecodeMetadataCacheOnLoad, &cozucu)) &&
        SUCCEEDED(cozucu->GetFrame(0, &cerceve)) &&
        SUCCEEDED(cerceve->GetSize(&w, &h)) && w > 0 && h > 0 &&
        w <= 8192 && h <= 8192 &&   // asiri buyuk referanslar bellegi tuketir
        SUCCEEDED(fabrika->CreateFormatConverter(&donusturucu)) &&
        SUCCEEDED(donusturucu->Initialize(cerceve, GUID_WICPixelFormat32bppBGRA,
                                          WICBitmapDitherTypeNone, NULL, 0.0,
                                          WICBitmapPaletteTypeCustom))) {
        buf = (BYTE*)malloc((size_t)w * h * 4);
        if (buf && SUCCEEDED(donusturucu->CopyPixels(NULL, w * 4, w * h * 4, buf))) {
            ReferansSerbest();
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -(int)h;   // ustten alta
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            void* bits = NULL;
            HDC ekran = GetDC(NULL);
            HBITMAP yeni = CreateDIBSection(ekran, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
            if (yeni && bits) {
                memcpy(bits, buf, (size_t)w * h * 4);
                g.refDC = CreateCompatibleDC(ekran);
                g.refBMP = yeni;
                g.refEski = (HBITMAP)SelectObject(g.refDC, g.refBMP);
                g.refW = w; g.refH = h;
                g.refVar = true; g.refGoster = true;
                sonuc = true;
            } else if (yeni) {
                DeleteObject(yeni);
            }
            ReleaseDC(NULL, ekran);
        }
    }
    free(buf);
    if (donusturucu) donusturucu->Release();
    if (cerceve) cerceve->Release();
    if (cozucu) cozucu->Release();
    if (fabrika) fabrika->Release();
    return sonuc;
}

// ------------------------------------------------------------
//  Arayuz duzeni
// ------------------------------------------------------------
static RECT AracButonRect(int i)
{
    RECT r;
    int satir = i / 11, sutun = i % 11;
    r.left   = 4 + sutun * 56;
    r.top    = 4 + satir * 37;
    r.right  = r.left + 52;
    r.bottom = r.top + 33;
    return r;
}

static const int PALET_X = 4 + 11 * 56 + 14;  // 634
// Palet: 12 sutun x 5 satir, kompakt kutular
static RECT RenkKutuRect(int i)
{
    RECT r;
    r.left   = PALET_X + (i % 12) * 14;
    r.top    = 6 + (i / 12) * 19;
    r.right  = r.left + 13;
    r.bottom = r.top + 17;
    return r;
}

static const int KONTROL_X = PALET_X + 8 * 24 + 16;  // 842

// Birincil / ikincil renk kutulari
static RECT OnRenkRect()   { return RECT{KONTROL_X + 80,  6, KONTROL_X + 114, 38}; }
static RECT ArkaRenkRect() { return RECT{KONTROL_X + 118, 6, KONTROL_X + 152, 38}; }
// Firca ucu dugmesi
static RECT UcButonRect()  { return RECT{KONTROL_X + 78, 40, KONTROL_X + 174, 72}; }
static RECT BoyutButonRect(int i)  // 0 = kucult, 1 = buyut
{
    RECT r;
    r.left   = KONTROL_X + i * 38;
    r.top    = 6;
    r.right  = r.left + 34;
    r.bottom = r.top + 30;
    return r;
}

static RECT DoluButonRect()
{
    RECT r;
    r.left = KONTROL_X; r.top = 40;
    r.right = r.left + 72; r.bottom = r.top + 32;
    return r;
}

// ------------------------------------------------------------
//  Arayuz cizimi
// ------------------------------------------------------------
static void DurumCubuguCiz(HDC dc, RECT rc, const wchar_t* metin)
{
    RECT sb = {0, rc.bottom - STATUSBAR_H, rc.right, rc.bottom};
    HBRUSH f = CreateSolidBrush(RGB(235, 235, 240));
    FillRect(dc, &sb, f);
    DeleteObject(f);
    HPEN p = CreatePen(PS_SOLID, 1, RGB(160, 160, 165));
    HPEN eskiP = (HPEN)SelectObject(dc, p);
    MoveToEx(dc, 0, sb.top, NULL);
    LineTo(dc, rc.right, sb.top);
    SelectObject(dc, eskiP);
    DeleteObject(p);
    SetBkMode(dc, TRANSPARENT);
    RECT tr = sb; tr.top += 4;
    DrawTextW(dc, metin, -1, &tr, DT_LEFT | DT_SINGLELINE);
}

static void DurumGuncelle(int cx, int cy)
{
    wchar_t yeni[192];
    swprintf(yeni, 192, T(M_DURUM_FMT),
        T(ARAC_METIN_ESLE[(int)g.arac]), GetRValue(g.renk), GetGValue(g.renk), GetBValue(g.renk),
        g.kalinlik, ZoomPct(), cx, cy);
    g.sonX = cx; g.sonY = cy;
    if (wcscmp(yeni, g.durumMetin) == 0) return;
    wcscpy_s(g.durumMetin, yeni);
    RECT rc; GetClientRect(g.pencere, &rc);
    RECT sb = {0, rc.bottom - STATUSBAR_H, rc.right, rc.bottom};
    InvalidateRect(g.pencere, &sb, FALSE);
}

static void ArayuzCiz(HDC dc, RECT tum)
{
    // Ortak firca/kalemler boyama basina bir kez yaratilir. Eskiden her
    // boyamada ~200 GDI nesnesi yaratilip siliniyordu; fare hareketinde
    // surekli boyama yapildigi icin arayuzde yavaslamaya yol aciyordu.
    HBRUSH fBar     = CreateSolidBrush(RGB(235, 235, 240));
    HBRUSH fBeyaz   = CreateSolidBrush(RGB(250, 250, 250));
    HBRUSH fSecili  = CreateSolidBrush(RGB(150, 190, 255));
    HBRUSH fCerceve = CreateSolidBrush(RGB(80, 80, 90));
    HBRUSH fCerSoluk = CreateSolidBrush(RGB(90, 90, 90));
    HBRUSH fIsaret  = CreateSolidBrush(RGB(255, 40, 40));
    HBRUSH fKoyu    = CreateSolidBrush(RGB(60, 60, 60));

    RECT bar = {0, 0, tum.right, TOOLBAR_H};
    FillRect(dc, &bar, fBar);
    HPEN ay = CreatePen(PS_SOLID, 1, RGB(160, 160, 165));
    HPEN eskiAy = (HPEN)SelectObject(dc, ay);
    MoveToEx(dc, 0, TOOLBAR_H - 1, NULL);
    LineTo(dc, tum.right, TOOLBAR_H - 1);
    SelectObject(dc, eskiAy);
    DeleteObject(ay);

    SetBkMode(dc, TRANSPARENT);

    // Arac butonlari (iki satir)
    for (int i = 0; i < ARAC_SAYISI; i++) {
        RECT r = AracButonRect(i);
        FillRect(dc, &r, g.arac == (Arac)i ? fSecili : fBeyaz);
        FrameRect(dc, &r, fCerceve);
        RECT tr = r; tr.top += 7;
        DrawTextW(dc, T(ARAC_METIN_ESLE[i]), -1, &tr, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    // Renk paleti (secili renk cift tiklamayla ozel renk acar)
    for (int i = 0; i < PALETTE_CNT; i++) {
        RECT r = RenkKutuRect(i);
        if (g_paletFirca[i]) FillRect(dc, &r, g_paletFirca[i]);
        FrameRect(dc, &r, PALET[i] == g.renk ? fIsaret : fCerSoluk);
        if (PALET[i] == g.renk) {
            RECT r2 = {r.left + 2, r.top + 2, r.right - 2, r.bottom - 2};
            FrameRect(dc, &r2, fIsaret);
        }
    }

    // Kalinlik butonlari
    const wchar_t* simge[2] = {L"-", L"+"};
    for (int i = 0; i < 2; i++) {
        RECT r = BoyutButonRect(i);
        FillRect(dc, &r, fBeyaz);
        FrameRect(dc, &r, fCerceve);
        DrawTextW(dc, simge[i], -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    // Dolu / bos sekil dugmesi
    RECT dr = DoluButonRect();
    FillRect(dc, &dr, g.dolu ? fSecili : fBeyaz);
    FrameRect(dc, &dr, fCerceve);
    RECT dtr = dr; dtr.top += 7;
    DrawTextW(dc, T(M_DOLU_ETIKET), -1, &dtr, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Firca ucu dugmesi (yuvarlak / kare)
    RECT ur = UcButonRect();
    FillRect(dc, &ur, g.kareUc ? fSecili : fBeyaz);
    FrameRect(dc, &ur, fCerceve);
    RECT utr = ur; utr.top += 7;
    DrawTextW(dc, g.kareUc ? T(M_UC_KARE) : T(M_UC_YUVARLAK), -1, &utr,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Birincil ve ikincil renk kutulari (tiklayinca ozel renk penceresi acilir)
    RECT on = OnRenkRect(), arka = ArkaRenkRect();
    HBRUSH of = CreateSolidBrush(g.renk);
    FillRect(dc, &on, of);
    DeleteObject(of);
    FrameRect(dc, &on, fKoyu);
    HBRUSH af = CreateSolidBrush(g.renk2);
    FillRect(dc, &arka, af);
    DeleteObject(af);
    FrameRect(dc, &arka, fKoyu);

    DeleteObject(fBar);
    DeleteObject(fBeyaz);
    DeleteObject(fSecili);
    DeleteObject(fCerceve);
    DeleteObject(fCerSoluk);
    DeleteObject(fIsaret);
    DeleteObject(fKoyu);
}

// ------------------------------------------------------------
//  Menu tanimlari
// ------------------------------------------------------------
enum {
    MENU_YENI = 1001, MENU_AC, MENU_KAYDET, MENU_FARKLI_KAYDET,
    MENU_GERI_AL, MENU_YINELE, MENU_TEMIZLE, MENU_CIKIS, MENU_HAKKINDA,
    MENU_DON90, MENU_DON270, MENU_AYNAY, MENU_AYNAD, MENU_BOYUTLANDIR,
    MENU_IZGARA, MENU_Z100, MENU_SIGDIR, MENU_SIMETRI,
    MENU_KILAVUZ, MENU_KAYNAK,
    MENU_REF_YUKLE, MENU_REF_GOSTER, MENU_REF_KALDIR, MENU_STABIL,
    MENU_PIKSELMUK, MENU_PALET_PIKSEL,
    MENU_DIL_BASE  = 1200,   // + dil indeksi
    MENU_EF_BASE   = 1300,   // + efekt indeksi (0..19)
    MENU_BOYUT_PRE = 1400,   // + hazir tuval boyutu indeksi
    MENU_REF_OP    = 1420    // + referans saydamlik indeksi (0=25%, 1=50%, 2=75%)
};

// Hazir tuval boyutlari (Resim menusu) - sayisal oldugu icin ceviri gerektirmez
static const int BOYUT_PRE[8][2] = {
    { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1280, 720 },
    { 1600, 900 }, { 1920, 1080 }, { 512, 512 }, { 1024, 1024 }
};
static const int BOYUT_PRE_SAYISI = 8;

static void MenuyuKur(HWND hwnd)
{
    HMENU eski = GetMenu(hwnd);
    HMENU menu = CreateMenu();

    HMENU dosya = CreatePopupMenu();
    AppendMenuW(dosya, MF_STRING, MENU_YENI,          T(M_MENU_YENI));
    AppendMenuW(dosya, MF_STRING, MENU_AC,            T(M_MENU_AC));
    AppendMenuW(dosya, MF_STRING, MENU_KAYDET,        T(M_MENU_KAYDET));
    AppendMenuW(dosya, MF_STRING, MENU_FARKLI_KAYDET, T(M_MENU_FARKLI));
    AppendMenuW(dosya, MF_SEPARATOR, 0, NULL);
    AppendMenuW(dosya, MF_STRING, MENU_CIKIS,         T(M_MENU_CIKIS));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)dosya, T(M_MENU_DOSYA));

    HMENU duzen = CreatePopupMenu();
    AppendMenuW(duzen, MF_STRING, MENU_GERI_AL, T(M_MENU_GERIAL));
    AppendMenuW(duzen, MF_STRING, MENU_YINELE,  T(M_MENU_YINELE));
    AppendMenuW(duzen, MF_SEPARATOR, 0, NULL);
    AppendMenuW(duzen, MF_STRING, MENU_TEMIZLE, T(M_MENU_TEMIZLE));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)duzen, T(M_MENU_DUZEN));

    HMENU resim = CreatePopupMenu();
    AppendMenuW(resim, MF_STRING, MENU_DON90,        T(M_DON90));
    AppendMenuW(resim, MF_STRING, MENU_DON270,       T(M_DON270));
    AppendMenuW(resim, MF_STRING, MENU_AYNAY,        T(M_AYNAY));
    AppendMenuW(resim, MF_STRING, MENU_AYNAD,        T(M_AYNAD));
    AppendMenuW(resim, MF_SEPARATOR, 0, NULL);
    AppendMenuW(resim, MF_STRING, MENU_BOYUTLANDIR,  T(M_BOYUTLANDIR));
    AppendMenuW(resim, MF_SEPARATOR, 0, NULL);
    for (int i = 0; i < BOYUT_PRE_SAYISI; i++) {
        wchar_t et[32];
        swprintf(et, 32, L"%d x %d", BOYUT_PRE[i][0], BOYUT_PRE[i][1]);
        AppendMenuW(resim, MF_STRING, MENU_BOYUT_PRE + i, et);
    }
    // Referans goruntu alt menusu (kopya kagidi gibi iz surme katmani)
    HMENU refm = CreatePopupMenu();
    AppendMenuW(refm, MF_STRING, MENU_REF_YUKLE, T(M_REF_YUKLE));
    AppendMenuW(refm, MF_STRING | (g.refGoster ? MF_CHECKED : 0), MENU_REF_GOSTER, T(M_REF_GOSTER));
    AppendMenuW(refm, MF_STRING, MENU_REF_KALDIR, T(M_REF_KALDIR));
    AppendMenuW(refm, MF_SEPARATOR, 0, NULL);
    static const int OP_ADIM[3] = { 25, 50, 75 };
    for (int i = 0; i < 3; i++) {
        wchar_t et[16];
        swprintf(et, 16, L"%%%d", OP_ADIM[i]);
        AppendMenuW(refm, MF_STRING | (g.refOpaklik == OP_ADIM[i] ? MF_CHECKED : 0),
                    MENU_REF_OP + i, et);
    }
    AppendMenuW(resim, MF_SEPARATOR, 0, NULL);
    AppendMenuW(resim, MF_POPUP, (UINT_PTR)refm, T(M_MENU_REFERANS));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)resim, T(M_MENU_RESIM));

    HMENU efekt = CreatePopupMenu();
    for (int i = 0; i < 20; i++)
        AppendMenuW(efekt, MF_STRING, MENU_EF_BASE + i, T(M_EF_GRI + i));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)efekt, T(M_MENU_EFEKT));

    HMENU gorunum = CreatePopupMenu();
    AppendMenuW(gorunum, MF_STRING | (g.izgara ? MF_CHECKED : 0), MENU_IZGARA, T(M_IZGARA));
    AppendMenuW(gorunum, MF_STRING, MENU_Z100,   T(M_Z100));
    AppendMenuW(gorunum, MF_STRING, MENU_SIGDIR, T(M_SIGDIR));
    AppendMenuW(gorunum, MF_STRING | (g.simetri ? MF_CHECKED : 0), MENU_SIMETRI, T(M_SIMETRI));
    AppendMenuW(gorunum, MF_STRING | (g.stabil ? MF_CHECKED : 0), MENU_STABIL, T(M_MENU_STABIL));
    AppendMenuW(gorunum, MF_SEPARATOR, 0, NULL);
    AppendMenuW(gorunum, MF_STRING | (g.pikselmuk ? MF_CHECKED : 0), MENU_PIKSELMUK, T(M_PIKSEL_MUK));
    AppendMenuW(gorunum, MF_STRING | (g.paletPiksel ? MF_CHECKED : 0), MENU_PALET_PIKSEL, T(M_MENU_PALET_PIKSEL));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)gorunum, T(M_MENU_GORUNUM));

    HMENU dil = CreatePopupMenu();
    for (int i = 0; i < DIL_SAYISI; i++)
        AppendMenuW(dil, MF_STRING | (i == g.dil ? MF_CHECKED : 0),
                    MENU_DIL_BASE + i, DIL_AD[i]);
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)dil, T(M_MENU_DIL));

    HMENU yardim = CreatePopupMenu();
    AppendMenuW(yardim, MF_STRING, MENU_KILAVUZ, T(M_MENU_KILAVUZ));
    AppendMenuW(yardim, MF_STRING, MENU_KAYNAK,  T(M_MENU_KAYNAK));
    AppendMenuW(yardim, MF_SEPARATOR, 0, NULL);
    AppendMenuW(yardim, MF_STRING, MENU_HAKKINDA, T(M_MENU_HAKKINDA));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)yardim, T(M_MENU_YARDIM));

    SetMenu(hwnd, menu);
    if (eski) DestroyMenu(eski);
}

// ------------------------------------------------------------
//  Ozel renk secme penceresi (tamamen kendi cizdigimiz)
// ------------------------------------------------------------
struct RenkDlgVeri { int r, gr, b; bool ok; };

static RECT RenkBarRect(int i)
{
    RECT r = {40, 40 + i * 45, 240, 60 + i * 45};
    return r;
}

static LRESULT CALLBACK RenkDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    RenkDlgVeri* v = (RenkDlgVeri*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        v = (RenkDlgVeri*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)v);
        CreateWindowExW(0, L"BUTTON", T(M_TAMAM), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        40, 190, 120, 30, hwnd, (HMENU)1, g.hInst, NULL);
        CreateWindowExW(0, L"BUTTON", T(M_IPTAL), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        180, 190, 120, 30, hwnd, (HMENU)2, g.hInst, NULL);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1 || LOWORD(wp) == 2) {
            v->ok = (LOWORD(wp) == 1);
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        [[fallthrough]];   // tiklama noktasini da surukleme gibi isle
    case WM_MOUSEMOVE: {
        if (msg == WM_MOUSEMOVE && !(wp & MK_LBUTTON)) break;
        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
        for (int i = 0; i < 3; i++) {
            RECT r = RenkBarRect(i);
            if (my >= r.top && my <= r.bottom) {
                int deger = (mx - r.left) * 255 / (r.right - r.left);
                if (deger < 0) deger = 0;
                if (deger > 255) deger = 255;
                if (i == 0) v->r = deger; else if (i == 1) v->gr = deger; else v->b = deger;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }
    case WM_LBUTTONUP:
        ReleaseCapture();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        const wchar_t* etiket[3] = {L"R", L"G", L"B"};
        COLORREF baz[3] = {RGB(255,0,0), RGB(0,200,0), RGB(0,80,255)};
        int deger[3] = {v->r, v->gr, v->b};
        SetBkMode(dc, TRANSPARENT);
        for (int i = 0; i < 3; i++) {
            RECT r = RenkBarRect(i);
            RECT te = {10, r.top, 36, r.bottom};
            DrawTextW(dc, etiket[i], -1, &te, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            HBRUSH arka = CreateSolidBrush(RGB(245, 245, 245));
            FillRect(dc, &r, arka);
            DeleteObject(arka);
            RECT dol = r;
            dol.right = r.left + (r.right - r.left) * deger[i] / 255;
            HBRUSH df = CreateSolidBrush(baz[i]);
            FillRect(dc, &dol, df);
            DeleteObject(df);
            HBRUSH cer = CreateSolidBrush(RGB(90, 90, 90));
            FrameRect(dc, &r, cer);
            DeleteObject(cer);
            wchar_t sayi[8];
            swprintf(sayi, 8, L"%d", deger[i]);
            RECT ts = {r.right + 8, r.top, r.right + 44, r.bottom};
            DrawTextW(dc, sayi, -1, &ts, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        }
        // Onizleme kutusu
        RECT on = {300, 40, 336, 150};
        HBRUSH pf = CreateSolidBrush(RGB(v->r, v->gr, v->b));
        FillRect(dc, &on, pf);
        DeleteObject(pf);
        HBRUSH pcer = CreateSolidBrush(RGB(60, 60, 60));
        FrameRect(dc, &on, pcer);
        DeleteObject(pcer);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool RenkDialogu(HWND sahip, COLORREF& renk)
{
    RenkDlgVeri v;
    v.r = GetRValue(renk); v.gr = GetGValue(renk); v.b = GetBValue(renk); v.ok = false;
    RECT sr; GetWindowRect(sahip, &sr);
    int x = sr.left + (sr.right - sr.left - 360) / 2;
    int y = sr.top + (sr.bottom - sr.top - 280) / 2;
    HWND d = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"MPRenkDlg", T(M_RENK_BASLIK),
                             WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                             x, y, 360, 280, sahip, NULL, g.hInst, &v);
    EnableWindow(sahip, FALSE);
    MSG m;
    while (IsWindow(d) && GetMessageW(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    EnableWindow(sahip, TRUE);
    SetForegroundWindow(sahip);
    if (v.ok) renk = RGB(v.r, v.gr, v.b);
    return v.ok;
}

// ------------------------------------------------------------
//  Tuval boyutlandirma penceresi
// ------------------------------------------------------------
struct BoyutDlgVeri { int w, h; bool ok; };

static LRESULT CALLBACK BoyutDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    BoyutDlgVeri* v = (BoyutDlgVeri*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        v = (BoyutDlgVeri*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)v);
        wchar_t sayi[16];
        CreateWindowExW(0, L"STATIC", T(M_GENISLIK), WS_CHILD | WS_VISIBLE,
                        14, 18, 90, 22, hwnd, NULL, g.hInst, NULL);
        swprintf(sayi, 16, L"%d", v->w);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", sayi, WS_CHILD | WS_VISIBLE | ES_NUMBER,
                        110, 14, 90, 24, hwnd, (HMENU)10, g.hInst, NULL);
        CreateWindowExW(0, L"STATIC", T(M_YUKSEKLIK), WS_CHILD | WS_VISIBLE,
                        14, 52, 90, 22, hwnd, NULL, g.hInst, NULL);
        swprintf(sayi, 16, L"%d", v->h);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", sayi, WS_CHILD | WS_VISIBLE | ES_NUMBER,
                        110, 48, 90, 24, hwnd, (HMENU)11, g.hInst, NULL);
        CreateWindowExW(0, L"BUTTON", T(M_TAMAM), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        30, 92, 90, 28, hwnd, (HMENU)1, g.hInst, NULL);
        CreateWindowExW(0, L"BUTTON", T(M_IPTAL), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        130, 92, 90, 28, hwnd, (HMENU)2, g.hInst, NULL);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) {
            wchar_t b[16];
            GetDlgItemTextW(hwnd, 10, b, 16); int w2 = _wtoi(b);
            GetDlgItemTextW(hwnd, 11, b, 16); int h2 = _wtoi(b);
            if (w2 < 16) w2 = 16;
            if (w2 > 4096) w2 = 4096;
            if (h2 < 16) h2 = 16;
            if (h2 > 4096) h2 = 4096;
            v->w = w2; v->h = h2; v->ok = true;
            DestroyWindow(hwnd);
        } else if (LOWORD(wp) == 2) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool BoyutDialogu(HWND sahip, int& w, int& h)
{
    BoyutDlgVeri v; v.w = w; v.h = h; v.ok = false;
    RECT sr; GetWindowRect(sahip, &sr);
    int x = sr.left + (sr.right - sr.left - 260) / 2;
    int y = sr.top + (sr.bottom - sr.top - 180) / 2;
    HWND d = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"MPBoyutDlg", T(M_BOYUT_BASLIK),
                             WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                             x, y, 260, 180, sahip, NULL, g.hInst, &v);
    EnableWindow(sahip, FALSE);
    MSG m;
    while (IsWindow(d) && GetMessageW(&m, NULL, 0, 0)) {
        if (!IsDialogMessageW(d, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    EnableWindow(sahip, TRUE);
    SetForegroundWindow(sahip);
    if (v.ok) { w = v.w; h = v.h; }
    return v.ok;
}

// ------------------------------------------------------------
//  Kaydirilabilir metin penceresi (kilavuz / kaynak kodu icin)
// ------------------------------------------------------------
static LRESULT CALLBACK MetinPencereProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_SIZE) {
        HWND e = GetWindow(hwnd, GW_CHILD);
        if (e) MoveWindow(e, 0, 0, (short)LOWORD(lp), (short)HIWORD(lp), TRUE);
        return 0;
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_NCDESTROY) {
        // Pencereye ozel yazi tipini serbest birak (her acilista siziyordu)
        HFONT f = (HFONT)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (f) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            DeleteObject(f);
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND MetinPencereAc(const wchar_t* baslik, const wchar_t* icerik)
{
    HWND d = CreateWindowExW(0, L"MPMetinPencere", baslik,
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT, 660, 540,
                             g.pencere, NULL, g.hInst, NULL);
    if (!d) return NULL;
    HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", NULL,
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                             ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                             0, 0, 644, 501, d, NULL, g.hInst, NULL);
    if (!e) return d;
    SendMessageW(e, EM_LIMITTEXT, 4 * 1024 * 1024, 0);   // buyuk metinlere izin ver
    HFONT f = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, FF_MODERN, L"Consolas");
    if (f) {
        // Yazi tipi pencereye emanet edilir; WM_NCDESTROY'da silinir
        SetWindowLongPtrW(d, GWLP_USERDATA, (LONG_PTR)f);
        SendMessageW(e, WM_SETFONT, (WPARAM)f, TRUE);
    }
    SetWindowTextW(e, icerik);
    return d;
}

// Kaynak kodu kaynaktan (resource) okuyup gosterir
static void KaynakGoster()
{
    HRSRC r = FindResourceW(NULL, L"KAYNAK", RT_RCDATA);
    if (!r) return;
    HGLOBAL h = LoadResource(NULL, r);
    DWORD boyut = SizeofResource(NULL, r);
    const char* veri = (const char*)LockResource(h);
    if (!veri || !boyut) return;

    int uzunluk = MultiByteToWideChar(CP_UTF8, 0, veri, (int)boyut, NULL, 0);
    if (uzunluk <= 0) return;
    wchar_t* ham = (wchar_t*)malloc(((size_t)uzunluk + 1) * sizeof(wchar_t));
    if (!ham) return;
    MultiByteToWideChar(CP_UTF8, 0, veri, (int)boyut, ham, uzunluk);
    ham[uzunluk] = 0;

    // EDIT denetimi icin LF -> CRLF donusumu
    int crlf = 0;
    for (int i = 0; i < uzunluk; i++)
        if (ham[i] == L'\n') crlf++;
    wchar_t* duzgun = (wchar_t*)malloc(((size_t)uzunluk + crlf + 1) * sizeof(wchar_t));
    if (!duzgun) { free(ham); return; }
    int j = 0;
    for (int i = 0; i < uzunluk; i++) {
        if (ham[i] == L'\n') duzgun[j++] = L'\r';
        duzgun[j++] = ham[i];
    }
    duzgun[j] = 0;
    MetinPencereAc(T(M_KAYNAK_BASLIK), duzgun);
    free(duzgun);
    free(ham);
}

// ------------------------------------------------------------
//  Metin araci
// ------------------------------------------------------------
#define IDC_METIN 2001

static void MetniUygula()
{
    if (!g.metinEdit || g.metinKapaniyor) return;
    g.metinKapaniyor = true;
    wchar_t buf[512];
    GetWindowTextW(g.metinEdit, buf, 512);
    if (buf[0]) {
        GecmisPush();
        HFONT f = CreateFontW(-(8 + g.kalinlik * 3), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, FF_SWISS, L"Segoe UI");
        HFONT eski = (HFONT)SelectObject(g.tuvalDC, f);
        SetBkMode(g.tuvalDC, TRANSPARENT);
        SetTextColor(g.tuvalDC, g.renk);
        TextOutW(g.tuvalDC, g.metinX, g.metinY, buf, (int)wcslen(buf));
        SelectObject(g.tuvalDC, eski);
        DeleteObject(f);
        g.degisti = true;
    }
    if (g.metinFont) { DeleteObject(g.metinFont); g.metinFont = NULL; }
    DestroyWindow(g.metinEdit);
    g.metinEdit = NULL;
    g.metinKapaniyor = false;
    InvalidateRect(g.pencere, NULL, FALSE);
}

static void MetinBaslat(int cx, int cy)
{
    MetniUygula();   // acik metin varsa once onu uygula
    float z = ZoomF();
    int sx = g.panX + (int)(cx * z);
    int sy = TOOLBAR_H + g.panY + (int)(cy * z);
    int yukseklik = (int)((12 + g.kalinlik * 3) * z);
    if (yukseklik < 14) yukseklik = 14;
    g.metinX = cx; g.metinY = cy;
    g.metinFont = CreateFontW(-yukseklik, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, FF_SWISS, L"Segoe UI");
    g.metinEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  sx, sy, 240, yukseklik + 12,
                                  g.pencere, (HMENU)IDC_METIN, g.hInst, NULL);
    SendMessageW(g.metinEdit, WM_SETFONT, (WPARAM)g.metinFont, TRUE);
    SetFocus(g.metinEdit);
}

// ------------------------------------------------------------
//  Secim araci
// ------------------------------------------------------------
static void SecimSerbest()
{
    if (g.secimDC) {
        SelectObject(g.secimDC, g.secimEski);
        DeleteObject(g.secimBMP);
        DeleteDC(g.secimDC);
        g.secimDC = NULL; g.secimBMP = NULL;
    }
    g.secimAktif = false;
    g.secimKesildi = false;
}

static void SecimUygula()
{
    if (!g.secimAktif) return;
    if (g.secimKesildi && g.secimDC) {
        BitBlt(g.tuvalDC, g.secim.left, g.secim.top, g.secimW, g.secimH,
               g.secimDC, 0, 0, SRCCOPY);
        g.degisti = true;
    }
    SecimSerbest();
    InvalidateRect(g.pencere, NULL, FALSE);
}

static void SecimKes()
{
    // secili bolgeyi yuzen katmana al, yerine ikincil rengi birak
    g.secimW = g.secim.right - g.secim.left;
    g.secimH = g.secim.bottom - g.secim.top;
    if (g.secimW <= 0 || g.secimH <= 0) { SecimSerbest(); return; }
    HDC ekran = GetDC(NULL);
    g.secimBMP = CreateCompatibleBitmap(ekran, g.secimW, g.secimH);
    g.secimDC  = CreateCompatibleDC(ekran);
    if (!g.secimBMP || !g.secimDC) {
        // Bellek yok: yarim kalan tutamaclari temizle, secimi iptal et
        // (yoksa bos yuzen katman uretilip tuvalde delik acilirdi)
        if (g.secimBMP) { DeleteObject(g.secimBMP); g.secimBMP = NULL; }
        if (g.secimDC)  { DeleteDC(g.secimDC);      g.secimDC = NULL; }
        ReleaseDC(NULL, ekran);
        SecimSerbest();
        return;
    }
    g.secimEski = (HBITMAP)SelectObject(g.secimDC, g.secimBMP);
    BitBlt(g.secimDC, 0, 0, g.secimW, g.secimH, g.tuvalDC, g.secim.left, g.secim.top, SRCCOPY);
    ReleaseDC(NULL, ekran);
    HBRUSH f = CreateSolidBrush(g.renk2);
    FillRect(g.tuvalDC, &g.secim, f);
    DeleteObject(f);
    g.secimEv = g.secim;
    g.secimKesildi = true;
}

// ------------------------------------------------------------
//  Pano (kopyala / yapistir)
// ------------------------------------------------------------
static void PanoyaKopyala()
{
    // Yuzen (kesilmis/yapistirilmis) secim once tuvale demirlenir; yoksa
    // kopyalanan goruntu secim icerigi yerine ALTINDAKI bos tuval olurdu
    // (yuzen icerik secimDC'de durur, BolgeAl tuvalDC'den okur)
    if (g.secimAktif && g.secimKesildi) SecimUygula();
    RECT kaynak = g.secimAktif ? g.secim : RECT{0, 0, g.tuvalW, g.tuvalH};
    int w = kaynak.right - kaynak.left, h = kaynak.bottom - kaynak.top;
    if (w <= 0 || h <= 0) return;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;   // alttan uste (pano standardi)
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    int boyut = w * h * 4;
    HGLOBAL bellek = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + boyut);
    if (!bellek) return;
    BYTE* hedef = (BYTE*)GlobalLock(bellek);
    memcpy(hedef, &bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    // Kismi GetDIBits guvenilir degil; bolgeyi BitBlt ile alip satirlari cevirelim
    BYTE* bolge = BolgeAl(kaynak.left, kaynak.top, w, h);
    if (!bolge) { GlobalUnlock(bellek); GlobalFree(bellek); return; }
    BYTE* bits = hedef + sizeof(BITMAPINFOHEADER);
    size_t adim = (size_t)g_bolgeW * 4;
    for (int satir = 0; satir < h; satir++)   // pano standardi alttan uste
        memcpy(bits + (size_t)(h - 1 - satir) * w * 4, bolge + (size_t)satir * adim,
               (size_t)w * 4);
    GlobalUnlock(bellek);

    if (OpenClipboard(g.pencere)) {
        EmptyClipboard();
        SetClipboardData(CF_DIB, bellek);
        CloseClipboard();
    } else {
        GlobalFree(bellek);
    }
}

static void PanodanYapistir()
{
    if (!IsClipboardFormatAvailable(CF_DIB)) return;
    if (!OpenClipboard(g.pencere)) return;
    HGLOBAL bellek = GetClipboardData(CF_DIB);
    if (!bellek) { CloseClipboard(); return; }
    BITMAPINFOHEADER* bh = (BITMAPINFOHEADER*)GlobalLock(bellek);
    if (!bh) { CloseClipboard(); return; }

    int w = bh->biWidth;
    int h = bh->biHeight > 0 ? bh->biHeight : -bh->biHeight;
    int baytRenk = bh->biBitCount / 8;
    if ((baytRenk != 3 && baytRenk != 4) ||
        (bh->biCompression != BI_RGB && bh->biCompression != BI_BITFIELDS)) {
        GlobalUnlock(bellek); CloseClipboard(); return;
    }
    // BI_BITFIELDS (biSize==40) basligin ardindan 3 maske DWORD'u gelir;
    // piksel verisi onlardan sonra baslar (or. baska uygulamalardan kopyalar)
    DWORD ek = (bh->biCompression == BI_BITFIELDS && bh->biSize == sizeof(BITMAPINFOHEADER)) ? 12 : 0;
    BYTE* veri = (BYTE*)bh + bh->biSize + ek;

    SecimUygula();   // onceki secimi kapat
    GecmisPush();

    // Yuzen secim olarak ekle
    int kopyaW = w > g.tuvalW - 20 ? g.tuvalW - 20 : w;
    int kopyaH = h > g.tuvalH - 20 ? g.tuvalH - 20 : h;
    if (kopyaW < 1) kopyaW = 1;
    if (kopyaH < 1) kopyaH = 1;

    HDC ekran = GetDC(NULL);
    g.secimBMP = CreateCompatibleBitmap(ekran, kopyaW, kopyaH);
    g.secimDC  = CreateCompatibleDC(ekran);
    if (!g.secimBMP || !g.secimDC) {
        // Bellek yok: yapistirmayi iptal et, bos gecmis kaydini geri cikar
        if (g.secimBMP) { DeleteObject(g.secimBMP); g.secimBMP = NULL; }
        if (g.secimDC)  { DeleteDC(g.secimDC);      g.secimDC = NULL; }
        ReleaseDC(NULL, ekran);
        GlobalUnlock(bellek);
        CloseClipboard();
        GecmisPop();
        return;
    }
    g.secimEski = (HBITMAP)SelectObject(g.secimDC, g.secimBMP);

    if (kopyaW == w && kopyaH == h) {
        BITMAPINFO bi = {};
        bi.bmiHeader = *bh;      // yukari/asagi yon isareti korunur
        bi.bmiHeader.biCompression = BI_RGB;   // standart BGR dizilimi
        SetDIBits(g.secimDC, g.secimBMP, 0, h, veri, &bi, DIB_RGB_COLORS);
    } else {
        // Pano goruntusu tuvalden buyuk: once tam boyut gecici bitmape yaz,
        // sonra gorunebilen kismi kirp. (SetDIBits kaynak genisliginde yazar;
        // kucuk bitmape dogrudan yazmak bellek tasmasi olurdu.)
        HBITMAP tBMP = CreateCompatibleBitmap(ekran, w, h);
        HDC tDC = CreateCompatibleDC(ekran);
        HBITMAP tEski = (HBITMAP)SelectObject(tDC, tBMP);
        BITMAPINFO bi = {};
        bi.bmiHeader = *bh;
        bi.bmiHeader.biCompression = BI_RGB;   // standart BGR dizilimi
        SetDIBits(tDC, tBMP, 0, h, veri, &bi, DIB_RGB_COLORS);
        BitBlt(g.secimDC, 0, 0, kopyaW, kopyaH, tDC, 0, 0, SRCCOPY);
        SelectObject(tDC, tEski);
        DeleteObject(tBMP);
        DeleteDC(tDC);
    }
    ReleaseDC(NULL, ekran);

    GlobalUnlock(bellek);
    CloseClipboard();

    g.secimW = kopyaW; g.secimH = kopyaH;
    g.secim = RECT{10, 10, 10 + kopyaW, 10 + kopyaH};
    g.secimEv = g.secim;
    g.secimAktif = true;
    g.secimKesildi = true;   // icerik zaten yuzuyor
    g.arac = ARAC_SECIM;
    g.degisti = true;
    InvalidateRect(g.pencere, NULL, FALSE);
}

// ------------------------------------------------------------
//  Ayarlar (INI dosyasi)
// ------------------------------------------------------------
static void IniYol(wchar_t* out)
{
    GetModuleFileNameW(NULL, out, MAX_PATH);
    wchar_t* s = wcsrchr(out, L'\\');
    if (s) wcscpy_s(s + 1, MAX_PATH - (int)(s + 1 - out), L"minipaint.ini");
}

static void AyarlariYukle()
{
    wchar_t ini[MAX_PATH];
    IniYol(ini);
    g.dil      = GetPrivateProfileIntW(L"ayar", L"dil", DIL_EN, ini);
    if (g.dil < 0 || g.dil >= DIL_SAYISI) g.dil = DIL_EN;
    g.arac     = (Arac)GetPrivateProfileIntW(L"ayar", L"arac", ARAC_KALEM, ini);
    if (g.arac < 0 || g.arac >= ARAC_SAYISI) g.arac = ARAC_KALEM;
    g.kalinlik = GetPrivateProfileIntW(L"ayar", L"kalinlik", 3, ini);
    if (g.kalinlik < 1) g.kalinlik = 1;
    if (g.kalinlik > 40) g.kalinlik = 40;
    g.renk     = (COLORREF)GetPrivateProfileIntW(L"ayar", L"renk", 0, ini);
    g.zoomIdx  = GetPrivateProfileIntW(L"ayar", L"zoom", 4, ini);
    if (g.zoomIdx < 0 || g.zoomIdx >= ZOOM_SAYISI) g.zoomIdx = 4;
    g.izgara   = GetPrivateProfileIntW(L"ayar", L"izgara", 0, ini) != 0;
    g.dolu     = GetPrivateProfileIntW(L"ayar", L"dolu", 0, ini) != 0;
    g.kareUc   = GetPrivateProfileIntW(L"ayar", L"kareuc", 0, ini) != 0;
    g.simetri  = GetPrivateProfileIntW(L"ayar", L"simetri", 0, ini) != 0;
    g.renk2    = (COLORREF)GetPrivateProfileIntW(L"ayar", L"renk2", 0xFFFFFF, ini);
    // ilk kurulumda (ini yoksa) pencere tam ekran acilir; kullanici kuculturse
    // tercihi kaydedilir ve bir sonraki acilista o sekilde gelir
    g.tamEkran = GetPrivateProfileIntW(L"ayar", L"tamekran", 1, ini) != 0;
    g.pikselmuk   = GetPrivateProfileIntW(L"ayar", L"pikselmuk", 0, ini) != 0;
    g.paletPiksel = GetPrivateProfileIntW(L"ayar", L"palet", 0, ini) != 0;
    // Firca sabitleme ve referans saydamligi da kalici (eskiden her
    // acilista sifirlaniyordu; kullanici tercihi kayboluyordu)
    g.stabil      = GetPrivateProfileIntW(L"ayar", L"stabil", 0, ini) != 0;
    g.refOpaklik  = GetPrivateProfileIntW(L"ayar", L"refopaklik", 50, ini);
    if (g.refOpaklik != 25 && g.refOpaklik != 50 && g.refOpaklik != 75)
        g.refOpaklik = 50;
    PaletUygula();
}

static void AyarlariKaydet()
{
    wchar_t ini[MAX_PATH], b[32];
    IniYol(ini);
    swprintf(b, 32, L"%d", g.dil);      WritePrivateProfileStringW(L"ayar", L"dil", b, ini);
    swprintf(b, 32, L"%d", (int)g.arac); WritePrivateProfileStringW(L"ayar", L"arac", b, ini);
    swprintf(b, 32, L"%d", g.kalinlik); WritePrivateProfileStringW(L"ayar", L"kalinlik", b, ini);
    swprintf(b, 32, L"%u", (unsigned)g.renk); WritePrivateProfileStringW(L"ayar", L"renk", b, ini);
    swprintf(b, 32, L"%d", g.zoomIdx);  WritePrivateProfileStringW(L"ayar", L"zoom", b, ini);
    WritePrivateProfileStringW(L"ayar", L"izgara", g.izgara ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"ayar", L"dolu",   g.dolu   ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"ayar", L"kareuc", g.kareUc ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"ayar", L"simetri", g.simetri ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"ayar", L"pikselmuk", g.pikselmuk ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"ayar", L"palet", g.paletPiksel ? L"1" : L"0", ini);
    swprintf(b, 32, L"%u", (unsigned)g.renk2); WritePrivateProfileStringW(L"ayar", L"renk2", b, ini);
    WritePrivateProfileStringW(L"ayar", L"stabil", g.stabil ? L"1" : L"0", ini);
    swprintf(b, 32, L"%d", g.refOpaklik); WritePrivateProfileStringW(L"ayar", L"refopaklik", b, ini);
    WritePrivateProfileStringW(L"ayar", L"tamekran",
        (g.pencere && IsZoomed(g.pencere)) ? L"1" : L"0", ini);
}

// ------------------------------------------------------------
//  Dosya islemleri
// ------------------------------------------------------------
static bool DosyaKaydetYol(const wchar_t* yol)
{
    int tur = DosyaTuru(yol);
    if (tur == 2) return PngKaydet(yol);
    if (tur == 3) return JpegKaydet(yol);
    if (tur == 4) return GifKaydet(yol);
    if (tur == 5) return TiffKaydet(yol);
    if (tur == 6) return IcoKaydet(yol);
    return BmpKaydet(yol);
}

static bool KaydetSor(bool farkli)
{
    // Yarim metin ve yuzen secim de kayda dahil olsun; yoksa kaydedilen
    // resimde kesilmis bolge bos (delik) olarak kalirdi.
    MetniUygula();
    SecimUygula();
    if (!farkli && g.dosyaYolu[0]) {
        if (DosyaKaydetYol(g.dosyaYolu)) return true;
        // Dogrudan kayit basarisiz oldu (ornegin dosya salt okunur oldu):
        // eskiden sessizce geciliyordu; kullanici kaydedildigini sanirdi
        MessageBoxW(g.pencere, T(M_HATA_KAYDET), T(M_HATA_BASLIK), MB_ICONWARNING);
        return false;
    }

    wchar_t yol[MAX_PATH] = L"resim.png";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g.pencere;
    ofn.lpstrFile   = yol;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = T(M_FILTRE);
    ofn.Flags       = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return false;

    // Uzanti yazilmadiysa secili suzgece uygun uzantiyi ekle.
    // (Eskiden hep .png eklenirdi; GIF secip 'resim' yazan kullanici
    // farkinda olmadan PNG dosyasi kaydediyordu.)
    if (!wcsrchr(yol, L'.')) {
        const wchar_t* uz = L".png";    // 1 = tum resimler grubu
        switch (ofn.nFilterIndex) {
        case 2: uz = L".bmp"; break;
        case 3: uz = L".png"; break;
        case 4: uz = L".jpg"; break;
        case 5: uz = L".gif"; break;
        case 6: uz = L".tif"; break;
        case 7: uz = L".ico"; break;
        }
        wcscat_s(yol, uz);
    }

    if (DosyaKaydetYol(yol)) {
        wcscpy_s(g.dosyaYolu, yol);
        return true;
    }
    // Kaydetme basarisiz (izin yok, disk dolu vb.): kullaniciya bildir
    MessageBoxW(g.pencere, T(M_HATA_KAYDET), T(M_HATA_BASLIK), MB_ICONWARNING);
    return false;
}

// Kaydedilmemis degisiklikler icin sorar; true = isleme devam edilebilir.
// Yeni / Ac / Cikis yollarinin hepsi bunu kullanir (hicbir dilde atlanmaz).
static bool DegisiklikSor(HWND hwnd)
{
    if (!g.degisti) return true;
    int cevap = MessageBoxW(hwnd, T(M_KAPANIS_SORU), L"MiniPaint",
                            MB_YESNOCANCEL | MB_ICONQUESTION);
    if (cevap == IDCANCEL) return false;
    if (cevap == IDYES && !KaydetSor(false)) return false;
    g.degisti = false;
    return true;
}

// ------------------------------------------------------------
//  Yaklasim yardimcilari
// ------------------------------------------------------------
static void ZoomSigdir()
{
    RECT rc; GetClientRect(g.pencere, &rc);
    int alanW = rc.right - 16;
    int alanH = rc.bottom - TOOLBAR_H - STATUSBAR_H - 16;
    if (alanW < 50 || alanH < 50) return;
    int zx = alanW * 100 / g.tuvalW;
    int zy = alanH * 100 / g.tuvalH;
    int hedef = zx < zy ? zx : zy;
    int enIyi = 0;
    for (int i = 0; i < ZOOM_SAYISI; i++)
        if (ZOOM_ADIM[i] <= hedef) enIyi = i;
    g.zoomIdx = enIyi;
    g.panX = g.panY = 8;
    InvalidateRect(g.pencere, NULL, FALSE);
}

// ------------------------------------------------------------
//  Boyama tamponlari: her WM_PAINT'te ayirma/silme yerine yeniden
//  kullanilir, yalnizca boyut degisince basilanir (akici cizim icin)
// ------------------------------------------------------------
static HDC g_tamponDC = NULL; static HBITMAP g_tamponBMP = NULL, g_tamponEski = NULL;
static int g_tamponW = 0, g_tamponH = 0;
static HDC g_onDC = NULL; static HBITMAP g_onBMP = NULL, g_onEski = NULL;
static int g_onW = 0, g_onH = 0;

static void TamponSerbest()
{
    if (g_tamponDC) {
        SelectObject(g_tamponDC, g_tamponEski);
        DeleteObject(g_tamponBMP);
        DeleteDC(g_tamponDC);
        g_tamponDC = NULL; g_tamponBMP = NULL;
        g_tamponW = g_tamponH = 0;
    }
}

static void OnizlemeSerbest()
{
    if (g_onDC) {
        SelectObject(g_onDC, g_onEski);
        DeleteObject(g_onBMP);
        DeleteDC(g_onDC);
        g_onDC = NULL; g_onBMP = NULL;
        g_onW = g_onH = 0;
    }
}

// ------------------------------------------------------------
//  Pencere proseduru
// ------------------------------------------------------------
static LRESULT CALLBACK PencereProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        g.pencere = hwnd;
        MenuyuKur(hwnd);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right > 200 ? rc.right - 32 : 1200;
        int h = rc.bottom > 200 ? rc.bottom - TOOLBAR_H - STATUSBAR_H - 32 : 800;
        TuvalOlustur(w, h, RGB(255, 255, 255));
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        // arac cubugu + palet + kontrollerin sigmasi icin asgari pencere boyutu
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = 1060;
        mmi->ptMinTrackSize.y = 420;
        return 0;
    }

    case WM_COMMAND: {
        // Metin kutusu odak kaybetti: metni tuvale uygula
        if (LOWORD(wp) == IDC_METIN && HIWORD(wp) == EN_KILLFOCUS) {
            MetniUygula();
            return 0;
        }
        // Dil secimi
        if (LOWORD(wp) >= MENU_DIL_BASE && LOWORD(wp) < MENU_DIL_BASE + (int)DIL_SAYISI) {
            g.dil = LOWORD(wp) - MENU_DIL_BASE;
            MenuyuKur(hwnd);
            SetWindowTextW(hwnd, T(M_BASLIK));
            g.durumMetin[0] = 0;
            DurumGuncelle(g.sonX, g.sonY);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        // Efektler
        if (LOWORD(wp) >= MENU_EF_BASE && LOWORD(wp) < MENU_EF_BASE + 20) {
            MetniUygula(); SecimUygula();
            EfektUygula(LOWORD(wp) - MENU_EF_BASE);
            return 0;
        }
        // Hazir tuval boyutlari
        if (LOWORD(wp) >= MENU_BOYUT_PRE && LOWORD(wp) < MENU_BOYUT_PRE + BOYUT_PRE_SAYISI) {
            MetniUygula(); SecimUygula();
            int i = LOWORD(wp) - MENU_BOYUT_PRE;
            // Boyut degisimi de geri alinabilir (gecmis korunur)
            GecmisPush();
            if (TuvalOlustur(BOYUT_PRE[i][0], BOYUT_PRE[i][1], RGB(255, 255, 255), true))
                g.degisti = true;
            else
                GecmisPop();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        // Referans saydamlik secimi
        if (LOWORD(wp) >= MENU_REF_OP && LOWORD(wp) < MENU_REF_OP + 3) {
            static const int OP_DEGER[3] = { 25, 50, 75 };
            g.refOpaklik = OP_DEGER[LOWORD(wp) - MENU_REF_OP];
            MenuyuKur(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        switch (LOWORD(wp)) {
        case MENU_YENI:
            MetniUygula(); SecimUygula();
            if (!DegisiklikSor(hwnd)) return 0;
            GecmisPush();
            {
                HBRUSH f = CreateSolidBrush(RGB(255, 255, 255));
                RECT r = {0, 0, g.tuvalW, g.tuvalH};
                FillRect(g.tuvalDC, &r, f);
                DeleteObject(f);
            }
            g.degisti = false;
            g.dosyaYolu[0] = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_TEMIZLE:
            MetniUygula(); SecimUygula();
            GecmisPush();
            {
                HBRUSH f = CreateSolidBrush(RGB(255, 255, 255));
                RECT r = {0, 0, g.tuvalW, g.tuvalH};
                FillRect(g.tuvalDC, &r, f);
                DeleteObject(f);
            }
            g.degisti = true;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_AC: {
            MetniUygula(); SecimUygula();
            if (!DegisiklikSor(hwnd)) return 0;
            wchar_t yol[MAX_PATH] = L"";
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFile   = yol;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrFilter = T(M_FILTRE);
            if (GetOpenFileNameW(&ofn)) {
                int tur = DosyaTuru(yol);
                bool ok = (tur == 1) ? BmpYukle(yol) : WicYukle(yol);   // WIC: PNG/JPEG/GIF/TIFF...
                if (ok) {
                    wcscpy_s(g.dosyaYolu, yol);
                    g.degisti = false;
                    ZoomSigdir();
                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    MessageBoxW(hwnd, T(M_HATA_ACMA), T(M_HATA_BASLIK), MB_ICONWARNING);
                }
            }
            return 0;
        }
        case MENU_KAYDET:
            if (KaydetSor(false)) g.degisti = false;
            return 0;
        case MENU_FARKLI_KAYDET:
            if (KaydetSor(true)) g.degisti = false;
            return 0;
        case MENU_GERI_AL: MetniUygula(); SecimUygula(); GeriAl(); return 0;
        case MENU_YINELE:  MetniUygula(); SecimUygula(); Yinele(); return 0;
        case MENU_DON90:   MetniUygula(); SecimUygula(); Dondur90(true);  return 0;
        case MENU_DON270:  MetniUygula(); SecimUygula(); Dondur90(false); return 0;
        case MENU_AYNAY:   MetniUygula(); SecimUygula(); Aynala(true);  return 0;
        case MENU_AYNAD:   MetniUygula(); SecimUygula(); Aynala(false); return 0;
        case MENU_BOYUTLANDIR: {
            MetniUygula(); SecimUygula();
            int w = g.tuvalW, h = g.tuvalH;
            if (BoyutDialogu(hwnd, w, h)) {
                // Boyut degisimi de geri alinabilir (gecmis korunur)
                GecmisPush();
                if (TuvalOlustur(w, h, RGB(255, 255, 255), true))
                    g.degisti = true;
                else
                    GecmisPop();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case MENU_IZGARA:
            g.izgara = !g.izgara;
            MenuyuKur(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_Z100:
            g.zoomIdx = 4; g.panX = g.panY = 8;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_SIGDIR:
            ZoomSigdir();
            return 0;
        case MENU_SIMETRI:
            g.simetri = !g.simetri;
            MenuyuKur(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_STABIL:
            g.stabil = !g.stabil;
            MenuyuKur(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_PIKSELMUK:
            g.pikselmuk = !g.pikselmuk;
            MenuyuKur(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_PALET_PIKSEL:
            g.paletPiksel = !g.paletPiksel;
            PaletUygula();
            MenuyuKur(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_REF_YUKLE: {
            wchar_t yol[MAX_PATH] = L"";
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFile   = yol;
            ofn.nMaxFile    = MAX_PATH;
            // Ana dosya filtresiyle ayni (cevirili ve tum turleri icerir);
            // eskiden burada Turkce'ye sabitlenmis ayri bir suzgec vardi
            ofn.lpstrFilter = T(M_FILTRE);
            if (GetOpenFileNameW(&ofn)) {
                if (!ReferansYukle(yol))
                    MessageBoxW(hwnd, T(M_HATA_ACMA), T(M_HATA_BASLIK), MB_ICONWARNING);
                MenuyuKur(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case MENU_REF_GOSTER:
            if (g.refVar) {
                g.refGoster = !g.refGoster;
                MenuyuKur(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        case MENU_REF_KALDIR:
            ReferansSerbest();
            MenuyuKur(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case MENU_CIKIS:
            // WM_CLOSE uzerinden gonder ki 'kaydedilmemis degisiklik' sorusu
            // (her dilde) gorunsun; dogrudan DestroyWindow bu kontrolu atlar.
            SendMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case MENU_HAKKINDA:
            MessageBoxW(hwnd, T(M_HAKKINDA_METIN), T(M_MENU_HAKKINDA), MB_ICONINFORMATION);
            return 0;
        case MENU_KILAVUZ:
            MetinPencereAc(T(M_KILAVUZ_BASLIK), T(M_KILAVUZ_METIN));
            return 0;
        case MENU_KAYNAK:
            KaynakGoster();
            return 0;
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (g.metinEdit && GetFocus() == g.metinEdit) break;  // metin kutusuna birak
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrl) {
            switch (wp) {
            // Menu yolundakiyle ayni hazirlik: acik metin ve yuzen secim
            // once uygulanir; yoksa boyut degisimi geri alinirken secim
            // koordinatlari eski boyutta kalip gecersizlesirdi
            case 'Z': MetniUygula(); SecimUygula(); GeriAl(); return 0;
            case 'Y': MetniUygula(); SecimUygula(); Yinele(); return 0;
            case 'S': if (KaydetSor(false)) g.degisti = false; return 0;
            case 'O': SendMessageW(hwnd, WM_COMMAND, MENU_AC, 0); return 0;
            case 'N': SendMessageW(hwnd, WM_COMMAND, MENU_YENI, 0); return 0;
            case 'C': PanoyaKopyala(); return 0;
            case 'V': PanodanYapistir(); return 0;
            case 'G': SendMessageW(hwnd, WM_COMMAND, MENU_IZGARA, 0); return 0;
            }
        } else {
            Arac oncekiArac = g.arac;
            switch (wp) {
            case VK_DELETE:
                if (g.secimAktif && !g.secimKesildi) {
                    GecmisPush();
                    HBRUSH f = CreateSolidBrush(g.renk2);   // silgi gibi ikincil renge sil
                    FillRect(g.tuvalDC, &g.secim, f);
                    DeleteObject(f);
                    SecimSerbest();
                    g.degisti = true;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            case VK_ESCAPE:
                if (g.cokgenSayi > 0) {   // yarim cokgeni geri al
                    g.cokgenSayi = 0;
                    GeriAl();
                }
                if (g.egriAsama == 2) {   // yarim egriyi de geri al (bos undo kalmasin)
                    g.egriAsama = 0;
                    GeriAl();
                }
                g.egriAsama = 0;
                if (g.secimAktif && g.secimKesildi) {
                    BitBlt(g.tuvalDC, g.secimEv.left, g.secimEv.top, g.secimW, g.secimH,
                           g.secimDC, 0, 0, SRCCOPY);
                    SecimSerbest();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            case 'P': g.arac = ARAC_KALEM;  break;
            case 'L': g.arac = ARAC_CIZGI;  break;
            case 'R': g.arac = ARAC_DIKDORTGEN; break;
            case 'O': g.arac = ARAC_ELIPS;  break;
            case 'F': g.arac = ARAC_DOLGU;  break;
            case 'E': g.arac = ARAC_SILGI;  break;
            case 'I': g.arac = ARAC_DAMLALIK; break;
            case 'S': g.arac = ARAC_SPREY;  break;
            case 'T': g.arac = ARAC_METIN;  break;
            case 'M': g.arac = ARAC_SECIM;  break;
            case 'W': g.arac = ARAC_SULUBOYA;   break;
            case 'N': g.arac = ARAC_ISLAK;      break;
            case 'Y': g.arac = ARAC_YAGLIFIRCA; break;
            case 'K': g.arac = ARAC_KARAKALEM;  break;
            case 'C': g.arac = ARAC_KALIGRAFI;  break;
            case 'B': g.arac = ARAC_SERIT;      break;
            case 'G': g.arac = ARAC_NEON;       break;
            case 'U': g.arac = ARAC_GOLGE;      break;
            case 'D': g.arac = ARAC_DITHER;     break;
            default: break;
            }
            // Klavyeden arac degisimi: acik metni uygula, yuzen secimi
            // yerine birak, yarim egri/cokgen durumunu sifirla (arac
            // cubugundaki dugmelerle ayni davranis).
            if (g.arac != oncekiArac) {
                MetniUygula();
                if (oncekiArac == ARAC_SECIM) SecimUygula();
                g.egriAsama = 0;
                g.cokgenSayi = 0;
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        if (y > TOOLBAR_H) {
            g.panTut = true;
            g.panBas.x = x; g.panBas.y = y;
            g.panBasPX = g.panX; g.panBasPY = g.panY;
            SetCapture(hwnd);
        }
        return 0;
    }

    case WM_MBUTTONUP:
        g.panTut = false;
        ReleaseCapture();
        return 0;

    case WM_MOUSEWHEEL: {
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            short delta = (short)HIWORD(wp);
            int eskiIdx = g.zoomIdx;
            if (delta > 0 && g.zoomIdx < ZOOM_SAYISI - 1) g.zoomIdx++;
            if (delta < 0 && g.zoomIdx > 0) g.zoomIdx--;
            if (g.zoomIdx != eskiIdx) {
                // imlecin altindaki nokta sabit kalsin
                POINT p = { (short)LOWORD(lp), (short)HIWORD(lp) };
                ScreenToClient(hwnd, &p);
                float eskiZ = ZOOM_ADIM[eskiIdx] / 100.0f;
                float yeniZ = ZoomF();
                float tx = (p.x - g.panX) / eskiZ;
                float ty = (p.y - TOOLBAR_H - g.panY) / eskiZ;
                g.panX = (int)(p.x - tx * yeniZ);
                g.panY = (int)(p.y - TOOLBAR_H - ty * yeniZ);
                g.durumMetin[0] = 0;
                DurumGuncelle(g.sonX, g.sonY);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        break;
    }

    case WM_RBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        if (y < TOOLBAR_H) {
            // palet uzerinde sag tik: ikincil rengi ayarla
            for (int i = 0; i < PALETTE_CNT; i++) {
                RECT r = RenkKutuRect(i);
                if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                    g.renk2 = PALET[i];
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            return 0;
        }
        int cx, cy;
        Ekran2Tuval(x, y, cx, cy);
        if (cx < 0 || cy < 0 || cx >= g.tuvalW || cy >= g.tuvalH) return 0;

        if (g.arac == ARAC_DAMLALIK) {   // renk ornekleme: cizim degil
            COLORREF c = GetPixel(g.tuvalDC, cx, cy);
            if (c != CLR_INVALID) { g.renk2 = c; InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }
        // Sag tus ile gercekten cizen araclar disindakiler bos gecmis
        // kaydi atmasin ve belgeyi kirletmesin (sekil/metin/secim vb.)
        bool sagCizer =
            g.arac == ARAC_KALEM || g.arac == ARAC_SILGI || g.arac == ARAC_SPREY ||
            g.arac == ARAC_DOLGU || g.arac == ARAC_BULANIK || g.arac == ARAC_SULUBOYA ||
            g.arac == ARAC_ISLAK || g.arac == ARAC_YAGLIFIRCA || g.arac == ARAC_KARAKALEM ||
            g.arac == ARAC_KALIGRAFI || g.arac == ARAC_SERIT || g.arac == ARAC_NEON ||
            g.arac == ARAC_GOLGE || g.arac == ARAC_DITHER ||
            g.arac == ARAC_GPEN || g.arac == ARAC_TON;
        if (!sagCizer) return 0;
        // Sol tusla devam eden bir vurus/secim sirasinda sag tusu yoksay;
        // aksi halde iki vurus ayni g.onceki uzerinden ic ice gecer ve
        // yarim vurus gecmise itilirdi.
        if (g.ciziyor || g.secimTanimla || g.secimTasima || g.egriAsama == 2) return 0;

        SetCapture(hwnd);
        g.sagCiziyor = true;
        g.onceki.x = cx; g.onceki.y = cy;
        g.stabX = cx; g.stabY = cy;
        GecmisPush();
        if (g.arac == ARAC_KALEM) {
            if (g.pikselmuk && g.kalinlik <= 1) {
                PikselBaslat(g.pik, cx, cy, g.renk2);
                if (g.simetri) PikselBaslat(g.pikA, g.tuvalW - 1 - cx, cy, g.renk2);
            } else {
                FircaNokta(cx, cy, g.renk2, g.kalinlik);
            }
        }
        else if (g.arac == ARAC_SILGI) FircaNokta(cx, cy, g.renk2, g.kalinlik * 2);
        else if (g.arac == ARAC_SPREY) SpreyPuskurt(cx, cy, g.renk2);
        else if (g.arac == ARAC_DOLGU) {
            if (!DolguYap(g.tuvalDC, cx, cy, g.renk2)) {   // hic degismedi
                GecmisPop();
                g.sagCiziyor = false;
                ReleaseCapture();
                return 0;
            }
        }
        else if (g.arac == ARAC_SULUBOYA) SuluBoyaVurus(cx, cy, cx, cy, g.renk2);
        else if (g.arac == ARAC_ISLAK) {
            COLORREF c = GetPixel(g.tuvalDC, cx, cy);
            if (c == CLR_INVALID) c = RGB(255, 255, 255);
            g.islakRenk = RenkKaristir(c, g.renk2, 0.75f);   // fircayi secili renkle yukle
            g.islakBoya = 1.0f;
            IslakVurus(cx, cy, cx, cy);
        }
        else if (g.arac == ARAC_YAGLIFIRCA) YagliVurus(cx, cy, cx, cy, g.renk2);
        else if (g.arac == ARAC_KARAKALEM) KarakalemVurus(cx, cy, cx, cy, g.renk2);
        else if (g.arac == ARAC_KALIGRAFI) KaligrafiVurus(cx, cy, cx, cy, g.renk2);
        else if (g.arac == ARAC_SERIT) SeritVurus(cx, cy, cx, cy, g.renk2);
        else if (g.arac == ARAC_NEON) NeonVurus(cx, cy, cx, cy, g.renk2);
        else if (g.arac == ARAC_GOLGE) GolgeVurus(cx, cy, cx, cy, g.renk2);
        else if (g.arac == ARAC_DITHER) DitherVurus(cx, cy, cx, cy, g.renk2, g.renk);
        else if (g.arac == ARAC_GPEN) {
            g.gpenKal = g.kalinlik;
            g.gpenTick = GetTickCount();
            GPenVurus(cx, cy, cx, cy, g.renk2);
        }
        else if (g.arac == ARAC_TON) TonDamga(cx, cy, g.renk2);
        g.degisti = true;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_RBUTTONUP:
        if (g.sagCiziyor) {
            g.sagCiziyor = false;
            ReleaseCapture();
            g.degisti = true;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONDBLCLK: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        if (y < TOOLBAR_H) {
            for (int i = 0; i < PALETTE_CNT; i++) {
                RECT r = RenkKutuRect(i);
                if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                    if (RenkDialogu(hwnd, g.renk))
                        InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            return 0;
        }
        // Cokgen aracinda cift tiklama: sekli kapat
        if (g.arac == ARAC_COKGEN && g.cokgenSayi >= 3) {
            CizgiCiz(g.tuvalDC, g.cokgen[g.cokgenSayi-1].x, g.cokgen[g.cokgenSayi-1].y,
                     g.cokgen[0].x, g.cokgen[0].y, g.renk, g.kalinlik);
            g.cokgenSayi = 0;
            g.degisti = true;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);

        if (y < TOOLBAR_H) {
            for (int i = 0; i < ARAC_SAYISI; i++) {
                RECT r = AracButonRect(i);
                if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                    MetniUygula();
                    if (g.arac == ARAC_SECIM && (Arac)i != ARAC_SECIM) SecimUygula();
                    g.egriAsama = 0;
                    g.cokgenSayi = 0;
                    g.arac = (Arac)i;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            for (int i = 0; i < PALETTE_CNT; i++) {
                RECT r = RenkKutuRect(i);
                if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                    g.renk = PALET[i];
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            for (int i = 0; i < 2; i++) {
                RECT r = BoyutButonRect(i);
                if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                    g.kalinlik += (i == 0 ? -1 : 1);
                    if (g.kalinlik < 1)  g.kalinlik = 1;
                    if (g.kalinlik > 40) g.kalinlik = 40;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            RECT dr = DoluButonRect();
            if (x >= dr.left && x < dr.right && y >= dr.top && y < dr.bottom) {
                g.dolu = !g.dolu;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            RECT ur = UcButonRect();
            if (x >= ur.left && x < ur.right && y >= ur.top && y < ur.bottom) {
                g.kareUc = !g.kareUc;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            RECT onR = OnRenkRect(), arkaR = ArkaRenkRect();
            if (x >= onR.left && x < onR.right && y >= onR.top && y < onR.bottom) {
                if (RenkDialogu(hwnd, g.renk)) InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= arkaR.left && x < arkaR.right && y >= arkaR.top && y < arkaR.bottom) {
                if (RenkDialogu(hwnd, g.renk2)) InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            return 0;
        }

        // --- Tuval bolgesi ---
        MetniUygula();
        // Sag tusla vurus devam ederken sol tus islemlerini yoksay
        // (iki vurusun ic ice gecmesini onler)
        if (g.sagCiziyor) return 0;
        int cx, cy;
        Ekran2Tuval(x, y, cx, cy);

        // Serbest cokgen: tiklayarak nokta ekle, cift tikla kapat
        if (g.arac == ARAC_COKGEN) {
            if (cx < 0 || cy < 0 || cx >= g.tuvalW || cy >= g.tuvalH) return 0;
            if (g.cokgenSayi == 0) GecmisPush();
            if (g.cokgenSayi < 64) {
                if (g.cokgenSayi > 0)
                    CizgiCiz(g.tuvalDC, g.cokgen[g.cokgenSayi-1].x, g.cokgen[g.cokgenSayi-1].y,
                             cx, cy, g.renk, g.kalinlik);
                g.cokgen[g.cokgenSayi].x = cx;
                g.cokgen[g.cokgenSayi].y = cy;
                g.cokgenSayi++;
                g.degisti = true;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        // Egri araci: iki asamali etkilesim (surukle + kontrol noktasi)
        if (g.arac == ARAC_EGRI) {
            if (g.egriAsama == 2) {
                BezierCiz(g.tuvalDC, g.egriA, POINT{cx, cy}, g.egriB, g.renk, g.kalinlik);
                g.egriAsama = 0;
                g.degisti = true;
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (cx >= 0 && cy >= 0 && cx < g.tuvalW && cy < g.tuvalH) {
                SetCapture(hwnd);
                g.ciziyor = true;
                g.baslangic.x = cx; g.baslangic.y = cy;
                GecmisPush();
            }
            return 0;
        }

        if (g.arac == ARAC_SECIM) {
            bool icinde = g.secimAktif && cx >= g.secim.left && cx < g.secim.right &&
                          cy >= g.secim.top && cy < g.secim.bottom;
            if (icinde) {
                if (!g.secimKesildi) {
                    GecmisPush();
                    SecimKes();
                    g.degisti = true;
                }
                g.secimTasima = true;
                g.secimAnchor.x = cx - g.secim.left;
                g.secimAnchor.y = cy - g.secim.top;
                SetCapture(hwnd);
            } else {
                SecimUygula();
                // Secim her zaman tuval icinde kalsin (disari tasan secim
                // kopyalama/kesme sirasinda bozuk piksel uretebiliyordu)
                if (cx < 0) cx = 0;
                if (cy < 0) cy = 0;
                if (cx >= g.tuvalW) cx = g.tuvalW - 1;
                if (cy >= g.tuvalH) cy = g.tuvalH - 1;
                g.secimTanimla = true;
                g.secimAktif = true;
                g.secim.left = g.secim.right = cx;
                g.secim.top = g.secim.bottom = cy;
                g.baslangic.x = cx; g.baslangic.y = cy;
                SetCapture(hwnd);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (cx < 0 || cy < 0 || cx >= g.tuvalW || cy >= g.tuvalH) return 0;

        if (g.arac == ARAC_DAMLALIK) {   // renk ornekleme: cizim degil
            COLORREF c = GetPixel(g.tuvalDC, cx, cy);
            if (c != CLR_INVALID) { g.renk = c; InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }
        if (g.arac == ARAC_METIN) {      // gecmis itimi MetniUygula icinde yapilir
            MetinBaslat(cx, cy);
            return 0;
        }
        SetCapture(hwnd);
        g.ciziyor = true;
        g.baslangic.x = cx; g.baslangic.y = cy;
        g.onceki = g.baslangic;
        g.stabX = cx; g.stabY = cy;
        GecmisPush();

        if (g.arac == ARAC_KALEM) {
            if (g.pikselmuk && g.kalinlik <= 1) {
                PikselBaslat(g.pik, cx, cy, g.renk);
                if (g.simetri) PikselBaslat(g.pikA, g.tuvalW - 1 - cx, cy, g.renk);
            } else {
                FircaNokta(cx, cy, g.renk, g.kalinlik);
            }
        } else if (g.arac == ARAC_SILGI) {
            FircaNokta(cx, cy, g.renk2, g.kalinlik * 2);
        } else if (g.arac == ARAC_SPREY) {
            SpreyPuskurt(cx, cy, g.renk);
        } else if (g.arac == ARAC_DOLGU) {
            if (!DolguYap(g.tuvalDC, cx, cy, g.renk)) {   // hic degismedi
                GecmisPop();
                g.ciziyor = false;
                ReleaseCapture();
                return 0;
            }
        } else if (g.arac == ARAC_DAMGA) {
            DamgaBas(cx, cy);
        } else if (g.arac == ARAC_BULANIK) {
            // bulanik firca sadece surukleme sirasinda calisir
        } else if (g.arac == ARAC_SULUBOYA) {
            SuluBoyaVurus(cx, cy, cx, cy, g.renk);
        } else if (g.arac == ARAC_ISLAK) {
            COLORREF c = GetPixel(g.tuvalDC, cx, cy);
            if (c == CLR_INVALID) c = RGB(255, 255, 255);
            g.islakRenk = RenkKaristir(c, g.renk, 0.75f);   // fircayi secili renkle yukle
            g.islakBoya = 1.0f;
            IslakVurus(cx, cy, cx, cy);
        } else if (g.arac == ARAC_YAGLIFIRCA) {
            YagliVurus(cx, cy, cx, cy, g.renk);
        } else if (g.arac == ARAC_KARAKALEM) {
            KarakalemVurus(cx, cy, cx, cy, g.renk);
        } else if (g.arac == ARAC_KALIGRAFI) {
            KaligrafiVurus(cx, cy, cx, cy, g.renk);
        } else if (g.arac == ARAC_SERIT) {
            SeritVurus(cx, cy, cx, cy, g.renk);
        } else if (g.arac == ARAC_NEON) {
            NeonVurus(cx, cy, cx, cy, g.renk);
        } else if (g.arac == ARAC_GOLGE) {
            GolgeVurus(cx, cy, cx, cy, g.renk);
        } else if (g.arac == ARAC_DITHER) {
            DitherVurus(cx, cy, cx, cy, g.renk, g.renk2);
        } else if (g.arac == ARAC_GPEN) {
            g.gpenKal = g.kalinlik;
            g.gpenTick = GetTickCount();
            GPenVurus(cx, cy, cx, cy, g.renk);
        } else if (g.arac == ARAC_TON) {
            TonDamga(cx, cy, g.renk);
        }
        g.degisti = true;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        int cx, cy;
        Ekran2Tuval(x, y, cx, cy);
        DurumGuncelle(cx, cy);

        if (g.panTut) {
            g.panX = g.panBasPX + (x - g.panBas.x);
            g.panY = g.panBasPY + (y - g.panBas.y);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (g.secimTanimla) {
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            if (cx >= g.tuvalW) cx = g.tuvalW - 1;
            if (cy >= g.tuvalH) cy = g.tuvalH - 1;
            g.secim = NormalRect(g.baslangic, POINT{cx, cy});
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (g.secimTasima) {
            int nx = cx - g.secimAnchor.x;
            int ny = cy - g.secimAnchor.y;
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx > g.tuvalW - g.secimW) nx = g.tuvalW - g.secimW;
            if (ny > g.tuvalH - g.secimH) ny = g.tuvalH - g.secimH;
            g.secim.left = nx; g.secim.top = ny;
            g.secim.right = nx + g.secimW; g.secim.bottom = ny + g.secimH;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (g.sagCiziyor && cy >= 0 && cy < g.tuvalH && cx >= 0 && cx < g.tuvalW) {
            if (g.stabil) {   // el titremesini yumusat
                g.stabX += (cx - g.stabX) * 0.30;
                g.stabY += (cy - g.stabY) * 0.30;
                cx = (int)(g.stabX + 0.5); cy = (int)(g.stabY + 0.5);
                if (cx == g.onceki.x && cy == g.onceki.y) { InvalidateRect(hwnd, NULL, FALSE); return 0; }
            }
            if (g.arac == ARAC_KALEM) {
                if (g.pikselmuk && g.kalinlik <= 1) PikselFircaVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                else FircaVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2, g.kalinlik);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_SILGI) {
                FircaVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2, g.kalinlik * 2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_SPREY) {
                SpreyPuskurt(cx, cy, g.renk2);
            } else if (g.arac == ARAC_BULANIK) {
                COLORREF c = GetPixel(g.tuvalDC, g.onceki.x, g.onceki.y);
                if (c != CLR_INVALID)
                    CizgiCiz(g.tuvalDC, g.onceki.x, g.onceki.y, cx, cy, c, g.kalinlik + 1);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_SULUBOYA) {
                SuluBoyaVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_ISLAK) {
                IslakVurus(g.onceki.x, g.onceki.y, cx, cy);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_YAGLIFIRCA) {
                YagliVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_KARAKALEM) {
                KarakalemVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_KALIGRAFI) {
                KaligrafiVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_SERIT) {
                SeritVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_NEON) {
                NeonVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_GOLGE) {
                GolgeVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_DITHER) {
                DitherVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_GPEN) {
                GPenVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_TON) {
                TonVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (g.ciziyor && cy >= 0 && cy < g.tuvalH && cx >= 0 && cx < g.tuvalW) {
            if (g.stabil) {   // el titremesini yumusat
                g.stabX += (cx - g.stabX) * 0.30;
                g.stabY += (cy - g.stabY) * 0.30;
                cx = (int)(g.stabX + 0.5); cy = (int)(g.stabY + 0.5);
                if (cx == g.onceki.x && cy == g.onceki.y) { InvalidateRect(hwnd, NULL, FALSE); return 0; }
            }
            if (g.arac == ARAC_KALEM) {
                if (g.pikselmuk && g.kalinlik <= 1) PikselFircaVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                else FircaVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk, g.kalinlik);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_SILGI) {
                FircaVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk2, g.kalinlik * 2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_SPREY) {
                SpreyPuskurt(cx, cy, g.renk);
            } else if (g.arac == ARAC_BULANIK) {
                COLORREF c = GetPixel(g.tuvalDC, g.onceki.x, g.onceki.y);
                if (c != CLR_INVALID)
                    CizgiCiz(g.tuvalDC, g.onceki.x, g.onceki.y, cx, cy, c, g.kalinlik + 1);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_SULUBOYA) {
                SuluBoyaVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_ISLAK) {
                IslakVurus(g.onceki.x, g.onceki.y, cx, cy);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_YAGLIFIRCA) {
                YagliVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_KARAKALEM) {
                KarakalemVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_KALIGRAFI) {
                KaligrafiVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_SERIT) {
                SeritVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_NEON) {
                NeonVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_GOLGE) {
                GolgeVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_DITHER) {
                DitherVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk, g.renk2);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_GPEN) {
                GPenVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            } else if (g.arac == ARAC_TON) {
                TonVurus(g.onceki.x, g.onceki.y, cx, cy, g.renk);
                g.onceki.x = cx; g.onceki.y = cy;
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        int cx, cy;
        Ekran2Tuval(x, y, cx, cy);

        if (g.secimTanimla) {
            g.secimTanimla = false;
            ReleaseCapture();
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            if (cx >= g.tuvalW) cx = g.tuvalW - 1;
            if (cy >= g.tuvalH) cy = g.tuvalH - 1;
            g.secim = NormalRect(g.baslangic, POINT{cx, cy});
            if (g.secim.right - g.secim.left < 2 || g.secim.bottom - g.secim.top < 2)
                SecimSerbest();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (g.secimTasima) {
            g.secimTasima = false;
            ReleaseCapture();
            // tasinan icerigi hemen yerine birak
            if (g.secimKesildi && g.secimDC) {
                BitBlt(g.tuvalDC, g.secim.left, g.secim.top, g.secimW, g.secimH,
                       g.secimDC, 0, 0, SRCCOPY);
                g.secimKesildi = false;
                SecimSerbest();
            }
            g.degisti = true;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (!g.ciziyor) return 0;
        g.ciziyor = false;
        ReleaseCapture();
        if (g.arac == ARAC_EGRI) {
            g.egriA = g.baslangic;
            g.egriB.x = cx; g.egriB.y = cy;
            g.egriAsama = 2;   // kontrol noktasi bekleniyor
        } else if (SekilMi(g.arac)) {
            SekilCiz(g.tuvalDC, g.arac, g.baslangic, POINT{cx, cy}, g.renk, g.kalinlik, g.dolu);
        }
        g.degisti = true;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_SETCURSOR: {
        if (LOWORD(lp) == HTCLIENT) {
            POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p);
            RECT rc; GetClientRect(hwnd, &rc);
            if (p.y > TOOLBAR_H && p.y < rc.bottom - STATUSBAR_H) {
                SetCursor(LoadCursor(NULL, IDC_CROSS));
                return TRUE;
            }
        }
        break;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(hwnd, &ps); return 0; }

        // Cift tampon onbellekte tutulur, yalnizca pencere boyutu degisince yenilenir
        if (rc.right != g_tamponW || rc.bottom != g_tamponH) {
            TamponSerbest();
            g_tamponDC = CreateCompatibleDC(dc);
            g_tamponBMP = g_tamponDC ? CreateCompatibleBitmap(dc, rc.right, rc.bottom) : NULL;
            if (!g_tamponDC || !g_tamponBMP) {
                // Bellek baskisi altinda tampon yaratilamazsa cizimi atla;
                // yoksa NULL DC'ye yapilan cagri zinciri cokerdi
                TamponSerbest();
                EndPaint(hwnd, &ps);
                return 0;
            }
            g_tamponEski = (HBITMAP)SelectObject(g_tamponDC, g_tamponBMP);
            g_tamponW = rc.right; g_tamponH = rc.bottom;
        }
        HDC tampon = g_tamponDC;

        HBRUSH zemin = CreateSolidBrush(RGB(170, 170, 175));
        FillRect(tampon, &rc, zemin);
        DeleteObject(zemin);

        float z = ZoomF();
        int dw = (int)(g.tuvalW * z), dh = (int)(g.tuvalH * z);
        int dx = g.panX, dy = TOOLBAR_H + g.panY;

        // Sekil onizlemesi gerekiyorsa once onbellekteki kopyaya ciz
        HDC kaynak = g.tuvalDC;
        bool onizlemeGerek = (g.ciziyor && SekilMi(g.arac)) ||
                             (g.egriAsama == 2 && g.arac == ARAC_EGRI);
        if (onizlemeGerek) {
            if (g.tuvalW != g_onW || g.tuvalH != g_onH) {   // onizleme tamponu da onbellekli
                OnizlemeSerbest();
                g_onDC = CreateCompatibleDC(dc);
                g_onBMP = g_onDC ? CreateCompatibleBitmap(dc, g.tuvalW, g.tuvalH) : NULL;
                if (!g_onDC || !g_onBMP) {
                    // Onizleme tamponsuz da yasanir: sekil cizilmeden tuval gosterilir
                    OnizlemeSerbest();
                } else {
                    g_onEski = (HBITMAP)SelectObject(g_onDC, g_onBMP);
                    g_onW = g.tuvalW; g_onH = g.tuvalH;
                }
            }
            if (!g_onDC) {   // tampon yoksa onizlemeyi atla (cokme olmaz)
                onizlemeGerek = false;
                goto onizlemeAtla;
            }
            POINT im; GetCursorPos(&im); ScreenToClient(hwnd, &im);
            int icx, icy; Ekran2Tuval(im.x, im.y, icx, icy);
            BitBlt(g_onDC, 0, 0, g.tuvalW, g.tuvalH, g.tuvalDC, 0, 0, SRCCOPY);
            if (g.egriAsama == 2 && g.arac == ARAC_EGRI)
                BezierCiz(g_onDC, g.egriA, POINT{icx, icy}, g.egriB, g.renk, g.kalinlik);
            else
                SekilCiz(g_onDC, g.arac, g.baslangic, POINT{icx, icy}, g.renk, g.kalinlik, g.dolu);
            kaynak = g_onDC;
        }
    onizlemeAtla:;

        SetStretchBltMode(tampon, HALFTONE);
        POINT org; SetBrushOrgEx(tampon, 0, 0, &org);
        StretchBlt(tampon, dx, dy, dw, dh, kaynak, 0, 0, g.tuvalW, g.tuvalH, SRCCOPY);

        // Yuzen secim icerigi
        if (g.secimAktif && g.secimKesildi && g.secimDC) {
            StretchBlt(tampon, dx + (int)(g.secim.left * z), dy + (int)(g.secim.top * z),
                       (int)(g.secimW * z), (int)(g.secimH * z),
                       g.secimDC, 0, 0, g.secimW, g.secimH, SRCCOPY);
        }

        // Izgara
        if (g.izgara && z >= 3.0f) {
            HPEN ip = CreatePen(PS_SOLID, 1, RGB(210, 210, 215));
            HPEN eskiIp = (HPEN)SelectObject(tampon, ip);
            for (int i = 0; i <= g.tuvalW; i++) {
                int gx = dx + (int)(i * z);
                MoveToEx(tampon, gx, dy, NULL);
                LineTo(tampon, gx, dy + dh);
            }
            for (int i = 0; i <= g.tuvalH; i++) {
                int gy = dy + (int)(i * z);
                MoveToEx(tampon, dx, gy, NULL);
                LineTo(tampon, dx + dw, gy);
            }
            SelectObject(tampon, eskiIp);
            DeleteObject(ip);
        }

        // Tuval cercevesi
        HPEN cp = CreatePen(PS_SOLID, 1, RGB(90, 90, 95));
        HPEN eskiCp = (HPEN)SelectObject(tampon, cp);
        HBRUSH eskiCf = (HBRUSH)SelectObject(tampon, GetStockObject(NULL_BRUSH));
        Rectangle(tampon, dx - 1, dy - 1, dx + dw + 1, dy + dh + 1);
        SelectObject(tampon, eskiCf);
        SelectObject(tampon, eskiCp);
        DeleteObject(cp);

        // Referans goruntu: tuvalin ustune yari saydam katman (cizime dahil degil)
        if (g.refVar && g.refGoster && g.refDC) {
            BLENDFUNCTION bf = {};
            bf.BlendOp = AC_SRC_OVER;
            bf.SourceConstantAlpha = (BYTE)(g.refOpaklik * 255 / 100);
            AlphaBlend(tampon, dx, dy, dw, dh, g.refDC, 0, 0, g.refW, g.refH, bf);
        }

        // Secim cercevesi (kesikli)
        if (g.secimAktif) {
            HPEN sp = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
            HPEN eskiSp = (HPEN)SelectObject(tampon, sp);
            HBRUSH eskiSf = (HBRUSH)SelectObject(tampon, GetStockObject(NULL_BRUSH));
            int eskiRop = SetROP2(tampon, R2_NOT);
            Rectangle(tampon,
                      dx + (int)(g.secim.left * z), dy + (int)(g.secim.top * z),
                      dx + (int)(g.secim.right * z), dy + (int)(g.secim.bottom * z));
            SetROP2(tampon, eskiRop);
            SelectObject(tampon, eskiSf);
            SelectObject(tampon, eskiSp);
            DeleteObject(sp);
        }

        ArayuzCiz(tampon, rc);
        DurumCubuguCiz(tampon, rc, g.durumMetin);

        BitBlt(dc, 0, 0, rc.right, rc.bottom, tampon, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        MetniUygula();
        if (!DegisiklikSor(hwnd)) return 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        AyarlariKaydet();
        SecimSerbest();
        GecmisTemizle();
        BolgeSerbest();
        ReferansSerbest();
        TamponSerbest();
        OnizlemeSerbest();
        for (int i = 0; i < PALETTE_CNT; i++)
            if (g_paletFirca[i]) { DeleteObject(g_paletFirca[i]); g_paletFirca[i] = NULL; }
        if (g.metinFont) DeleteObject(g.metinFont);
        if (g.tuvalDC) { SelectObject(g.tuvalDC, g.eskiBMP); DeleteObject(g.tuvalBMP); DeleteDC(g.tuvalDC); }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ------------------------------------------------------------
//  Giris noktasi
// ------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int goster)
{
    g.hInst = hInst;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    srand(GetTickCount());

    g.arac = ARAC_KALEM;
    g.renk = RGB(0, 0, 0);
    g.renk2 = RGB(255, 255, 255);
    g.kalinlik = 3;
    g.dil = DIL_EN;
    g.zoomIdx = 4;
    g.panX = g.panY = 8;
    g.refOpaklik = 50;   // referans katmani varsayilan saydamlik (%50)
    g.dosyaYolu[0] = 0;

    AyarlariYukle();   // onceki oturumun ayarlarini geri yukle

    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = PencereProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MiniPaintSinif";
    RegisterClassW(&wc);

    WNDCLASSW mp = {};
    mp.lpfnWndProc = MetinPencereProc;
    mp.hInstance = hInst;
    mp.hCursor = LoadCursor(NULL, IDC_ARROW);
    mp.hIcon = wc.hIcon;
    mp.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    mp.lpszClassName = L"MPMetinPencere";
    RegisterClassW(&mp);

    WNDCLASSW rd = {};
    rd.lpfnWndProc = RenkDlgProc;
    rd.hInstance = hInst;
    rd.hCursor = LoadCursor(NULL, IDC_ARROW);
    rd.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    rd.lpszClassName = L"MPRenkDlg";
    RegisterClassW(&rd);

    WNDCLASSW bd = {};
    bd.lpfnWndProc = BoyutDlgProc;
    bd.hInstance = hInst;
    bd.hCursor = LoadCursor(NULL, IDC_ARROW);
    bd.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    bd.lpszClassName = L"MPBoyutDlg";
    RegisterClassW(&bd);

    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, T(M_BASLIK),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1150, 760,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, g.tamEkran ? SW_SHOWMAXIMIZED : goster);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return (int)msg.wParam;
}
