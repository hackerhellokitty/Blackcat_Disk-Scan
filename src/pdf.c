#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpdf.h"
#include "pdf.h"
#include "disk.h"
#include "utils.h"
#include "i18n.h"

/* -------------------------------------------------------------------------
 * Helpers: wchar -> UTF-8
 * ---------------------------------------------------------------------- */

static void w2u(const wchar_t* ws, char* buf, int sz)
{
    if (!ws || !buf || sz <= 0) { if (buf && sz > 0) buf[0] = '\0'; return; }
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, buf, sz, NULL, NULL);
    if (n <= 0) buf[0] = '\0';
}

/* -------------------------------------------------------------------------
 * Thai font loading helper
 * ---------------------------------------------------------------------- */

static HPDF_Font load_thai_font(HPDF_Doc pdf, BOOL* loaded_thai)
{
    const char* candidates[] = {
        "C:\\Windows\\Fonts\\LeelawUI.ttf",
        "C:\\Windows\\Fonts\\leelawad.ttf",
        "C:\\Windows\\Fonts\\Leelawad.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf",
        "C:\\Windows\\Fonts\\Tahoma.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\Arial.ttf",
        NULL
    };

    *loaded_thai = FALSE;
    HPDF_Font font = NULL;

    for (int i = 0; candidates[i]; i++) {
        if (GetFileAttributesA(candidates[i]) == INVALID_FILE_ATTRIBUTES) continue;

        const char* name = HPDF_LoadTTFontFromFile(pdf, candidates[i], HPDF_TRUE);
        if (name && name[0] != '\0') {
            font = HPDF_GetFont(pdf, name, "UTF-8");
            if (font) {
                if (i < 3) *loaded_thai = TRUE; /* LeelawUI variants support Thai */
                return font;
            }
        }
    }

    /* Fallback to built-in Helvetica */
    return HPDF_GetFont(pdf, "Helvetica", NULL);
}

/* -------------------------------------------------------------------------
 * Drawing primitives
 * ---------------------------------------------------------------------- */

#define PAGE_W  595.0f   /* A4 pt */
#define PAGE_H  842.0f
#define MARGIN  40.0f
#define CONTENT_W (PAGE_W - 2.0f * MARGIN)

#define MAX_PDF_PAGES 64

typedef struct {
    HPDF_Doc   pdf;
    HPDF_Page  page;
    HPDF_Font  font_reg;
    HPDF_Font  font_thai;   /* may equal font_reg if Thai not loaded */
    float      y;           /* current cursor y (from top, we invert) */
    int        lang;
    BOOL       thai_ok;
    HPDF_Page  pages[MAX_PDF_PAGES];
    int        page_count;
} DrawCtx;

/* Convert "from top" y to PDF y (from bottom) */
static float py(const DrawCtx* dc, float y_top)
{
    return PAGE_H - y_top;
}

static void ensure_page(DrawCtx* dc)
{
    if (!dc->page) {
        dc->page = HPDF_AddPage(dc->pdf);
        HPDF_Page_SetSize(dc->page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
        dc->y = MARGIN + 10.0f;
        if (dc->page_count < MAX_PDF_PAGES)
            dc->pages[dc->page_count++] = dc->page;
    }
}

static void new_page(DrawCtx* dc)
{
    dc->page = HPDF_AddPage(dc->pdf);
    HPDF_Page_SetSize(dc->page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    dc->y = MARGIN + 10.0f;
    if (dc->page_count < MAX_PDF_PAGES)
        dc->pages[dc->page_count++] = dc->page;
}

static void check_page_break(DrawCtx* dc, float needed)
{
    if (dc->y + needed > PAGE_H - MARGIN) {
        new_page(dc);
    }
}

static void draw_rect_fill(DrawCtx* dc, float x, float y_top, float w, float h,
                           float r, float g, float b)
{
    HPDF_Page_SetRGBFill(dc->page, r, g, b);
    HPDF_Page_Rectangle(dc->page, x, py(dc, y_top) - h, w, h);
    HPDF_Page_Fill(dc->page);
}

static void draw_text(DrawCtx* dc, HPDF_Font font, float size,
                      float x, float y_top,
                      float r, float g, float b,
                      const char* text)
{
    if (!text || !text[0]) return;
    HPDF_Page_BeginText(dc->page);
    HPDF_Page_SetFontAndSize(dc->page, font, size);
    HPDF_Page_SetRGBFill(dc->page, r, g, b);
    HPDF_Page_TextOut(dc->page, x, py(dc, y_top), text);
    HPDF_Page_EndText(dc->page);
}

static void draw_wtext(DrawCtx* dc, HPDF_Font font, float size,
                       float x, float y_top,
                       float r, float g, float b,
                       const wchar_t* wtext)
{
    char buf[1024];
    w2u(wtext, buf, sizeof(buf));
    draw_text(dc, font, size, x, y_top, r, g, b, buf);
}

/* Section header: red left bar + bold-looking text */
static void draw_section_header(DrawCtx* dc, const wchar_t* title)
{
    check_page_break(dc, 30.0f);
    float y = dc->y;

    /* Red left bar */
    draw_rect_fill(dc, MARGIN, y, 4.0f, 18.0f, 1.0f, 0.23f, 0.19f);
    /* Text */
    draw_wtext(dc, dc->font_thai, 13.0f, MARGIN + 10.0f, y + 13.0f, 0.1f, 0.1f, 0.1f, title);

    dc->y += 24.0f;
}

/* Key-value row with alternating background */
static void draw_kv_row(DrawCtx* dc, int row_index,
                        const wchar_t* key, const wchar_t* value)
{
    check_page_break(dc, 22.0f);
    float y = dc->y;
    float h = 20.0f;

    /* Alternating background */
    if (row_index % 2 == 0) {
        draw_rect_fill(dc, MARGIN, y, CONTENT_W, h, 0.96f, 0.96f, 0.97f);
    }

    char kbuf[256], vbuf[512];
    w2u(key,   kbuf, sizeof(kbuf));
    w2u(value, vbuf, sizeof(vbuf));

    draw_text(dc, dc->font_thai, 9.5f,
              MARGIN + 6.0f, y + 14.0f, 0.35f, 0.35f, 0.35f, kbuf);
    draw_text(dc, dc->font_thai, 9.5f,
              MARGIN + CONTENT_W * 0.38f, y + 14.0f, 0.05f, 0.05f, 0.05f, vbuf);

    dc->y += h + 2.0f;
}

/* -------------------------------------------------------------------------
 * Disk map grid
 * ---------------------------------------------------------------------- */

static void draw_disk_map(DrawCtx* dc, const ScanReport* report)
{
    draw_section_header(dc, TS(pdf_sec_map));

    float bsz  = 10.0f;   /* block size in points — เล็กพอให้ fit A4 */
    float gap  = 1.5f;
    float cell = bsz + gap;
    float map_x = MARGIN;
    float avail_w = CONTENT_W;
    int   cols = (int)(avail_w / cell);
    if (cols < 1) cols = 1;
    (void)map_x;

    int count = report->block_count;
    if (count <= 0) count = 1;

    int rows = (count + cols - 1) / cols;
    float map_h = rows * cell + 10.0f;

    check_page_break(dc, map_h + 30.0f);

    for (int i = 0; i < count; i++) {
        int row = i / cols;
        int col = i % cols;
        float bx = map_x + col * cell;
        float by = dc->y + row * cell;

        float r, g, b;
        switch (report->block_states[i]) {
        case BLOCK_OK:
            r = 0.19f; g = 0.82f; b = 0.35f; /* green */
            break;
        case BLOCK_BAD:
            r = 1.0f;  g = 0.23f; b = 0.19f; /* red */
            break;
        default: /* UNSCAN */
            r = 0.66f; g = 0.66f; b = 0.66f; /* grey */
            break;
        }
        draw_rect_fill(dc, bx, by, bsz, bsz, r, g, b);
    }

    dc->y += map_h;

    /* Legend */
    float lx = map_x;
    float ly = dc->y + 4.0f;
    char leg[64];

    struct { float r, g, b; const wchar_t* label; } legend[] = {
        { 0.66f, 0.66f, 0.66f, TS(leg_unscan) },
        { 0.19f, 0.82f, 0.35f, TS(leg_ok) },
        { 1.0f,  0.23f, 0.19f, TS(leg_bad) },
    };

    for (int i = 0; i < 3; i++) {
        draw_rect_fill(dc, lx, ly, 10.0f, 10.0f,
                       legend[i].r, legend[i].g, legend[i].b);
        w2u(legend[i].label, leg, sizeof(leg));
        draw_text(dc, dc->font_thai, 9.0f,
                  lx + 13.0f, ly + 9.0f, 0.3f, 0.3f, 0.3f, leg);
        lx += 80.0f;
    }

    dc->y += 20.0f;
}

/* -------------------------------------------------------------------------
 * Event log section
 * ---------------------------------------------------------------------- */

static void draw_event_log(DrawCtx* dc, const ScanReport* report)
{
    draw_section_header(dc, TS(pdf_sec_log));

    int n = report->log_count;
    if (n > MAX_LOG_EVENTS) n = MAX_LOG_EVENTS;

    for (int i = 0; i < n; i++) {
        check_page_break(dc, 18.0f);

        const LogEvent* ev = &report->log_events[i];
        float y = dc->y;
        float h = 16.0f;

        /* Alternating row background */
        if (i % 2 == 0) {
            draw_rect_fill(dc, MARGIN, y, CONTENT_W, h, 0.96f, 0.96f, 0.97f);
        }

        /* Timestamp */
        char tsbuf[64];
        w2u(ev->timestamp, tsbuf, sizeof(tsbuf));
        draw_text(dc, dc->font_reg, 7.5f,
                  MARGIN + 4.0f, y + 12.0f, 0.5f, 0.5f, 0.5f, tsbuf);

        /* Message with colour */
        float mr, mg, mb_c;
        switch (ev->color_type) {
        case 1:  mr=0.9f; mg=0.1f; mb_c=0.1f; break; /* red */
        case 2:  mr=0.1f; mg=0.7f; mb_c=0.2f; break; /* green */
        default: mr=0.1f; mg=0.1f; mb_c=0.1f; break;
        }
        char msgbuf[512];
        w2u(ev->message, msgbuf, sizeof(msgbuf));
        draw_text(dc, dc->font_thai, 8.0f,
                  MARGIN + 80.0f, y + 12.0f, mr, mg, mb_c, msgbuf);

        dc->y += h + 2.0f;
    }
}

/* -------------------------------------------------------------------------
 * generate_pdf_report
 * ---------------------------------------------------------------------- */

int generate_pdf_report(const ScanReport* report, const wchar_t* save_path, int lang)
{
    (void)lang; /* we use g_cur_lang via TS() macro */

    HPDF_Doc pdf = HPDF_New(NULL, NULL);
    if (!pdf) return -1;

    /* Encoding */
    HPDF_UseUTFEncodings(pdf);
    HPDF_SetCurrentEncoder(pdf, "UTF-8");

    BOOL thai_ok = FALSE;
    HPDF_Font font_main = load_thai_font(pdf, &thai_ok);
    if (!font_main) {
        HPDF_Free(pdf);
        return -1;
    }

    DrawCtx dc;
    memset(&dc, 0, sizeof(dc));
    dc.pdf       = pdf;
    dc.page      = NULL;
    dc.font_reg  = font_main;
    dc.font_thai = font_main;
    dc.y         = MARGIN;
    dc.lang      = lang;
    dc.thai_ok   = thai_ok;

    /* ---- PAGE 1: Cover / Info ---- */
    new_page(&dc);

    /* Header bar */
    draw_rect_fill(&dc, 0.0f, 0.0f, PAGE_W, 52.0f, 0.07f, 0.07f, 0.07f);
    /* Red accent line */
    draw_rect_fill(&dc, 0.0f, 50.0f, PAGE_W, 3.0f, 1.0f, 0.23f, 0.19f);

    /* Title */
    draw_text(&dc, dc.font_reg, 22.0f,
              MARGIN, 34.0f, 1.0f, 0.23f, 0.19f, "BlackCat");
    draw_text(&dc, dc.font_reg, 14.0f,
              MARGIN + 110.0f, 32.0f, 0.85f, 0.85f, 0.85f, "Disk Scanner - Scan Report");

    dc.y = 68.0f;

    /* ---- Section: Disk Information ---- */
    draw_section_header(&dc, TS(pdf_sec_info));

    wchar_t vbuf[256];

    draw_kv_row(&dc, 0, TS(pdf_disk),   report->disk_label);
    draw_kv_row(&dc, 1, TS(pdf_serial), report->disk_serial);
    draw_kv_row(&dc, 2, TS(pdf_mode),   report->scan_mode);

    wcsncpy_s(vbuf, _countof(vbuf),
              report->stop_on_bad ? TS(stop_first) : TS(scan_finish), _TRUNCATE);
    draw_kv_row(&dc, 3, TS(pdf_sob), vbuf);

    dc.y += 10.0f;

    /* ---- Section: Scan Statistics ---- */
    draw_section_header(&dc, TS(pdf_sec_stats));

    draw_kv_row(&dc, 0, TS(pdf_start), report->start_time);
    draw_kv_row(&dc, 1, TS(pdf_end),   report->end_time);

    fmt_duration(vbuf, _countof(vbuf), report->duration_s);
    draw_kv_row(&dc, 2, TS(pdf_duration), vbuf);

    draw_kv_row(&dc, 3, TS(pdf_result), report->result_msg);

    fmt_mb(vbuf, _countof(vbuf), report->total_mb);
    draw_kv_row(&dc, 4, TS(pdf_total), vbuf);

    fmt_mb(vbuf, _countof(vbuf), report->scanned_mb);
    draw_kv_row(&dc, 5, TS(pdf_scanned), vbuf);

    _snwprintf_s(vbuf, _countof(vbuf), _TRUNCATE, L"%d", report->bad_count);
    draw_kv_row(&dc, 6, TS(pdf_bad), vbuf);

    fmt_speed(vbuf, _countof(vbuf), report->avg_speed);
    draw_kv_row(&dc, 7, TS(pdf_speed), vbuf);

    dc.y += 14.0f;

    /* ---- Disk Map ---- */
    draw_disk_map(&dc, report);

    dc.y += 10.0f;

    /* ---- Event Log ---- */
    draw_event_log(&dc, report);

    /* ---- Footer on each page ---- */
    {
        for (int pi = 0; pi < dc.page_count; pi++) {
            HPDF_Page pg = dc.pages[pi];
            char footer[128];
            _snprintf_s(footer, sizeof(footer), _TRUNCATE,
                "BlackCat Disk Scanner  |  Page %d of %d", pi + 1, dc.page_count);
            HPDF_Page_BeginText(pg);
            HPDF_Page_SetFontAndSize(pg, dc.font_reg, 7.5f);
            HPDF_Page_SetRGBFill(pg, 0.6f, 0.6f, 0.6f);
            HPDF_Page_TextOut(pg, MARGIN, 20.0f, footer);
            HPDF_Page_EndText(pg);
            /* Bottom line */
            HPDF_Page_SetLineWidth(pg, 0.5f);
            HPDF_Page_SetRGBStroke(pg, 0.8f, 0.8f, 0.8f);
            HPDF_Page_MoveTo(pg, MARGIN, 28.0f);
            HPDF_Page_LineTo(pg, PAGE_W - MARGIN, 28.0f);
            HPDF_Page_Stroke(pg);
        }
    }

    /* Save */
    char save_path_u8[MAX_PATH * 3];
    w2u(save_path, save_path_u8, sizeof(save_path_u8));

    HPDF_STATUS status = HPDF_SaveToFile(pdf, save_path_u8);
    HPDF_Free(pdf);

    return (status == HPDF_OK) ? 0 : -1;
}
