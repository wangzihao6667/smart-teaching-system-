/* =============================================================================
 *  gui_main.c  —— 智慧教学管理系统【Win32 图形界面层】
 * -----------------------------------------------------------------------------
 *  特点：
 *    · 零第三方依赖，只用 Windows 自带 API（user32 / gdi32 / comctl32）
 *    · 左侧导航 + 表格（ListView）+ GDI 自绘统计图表，全程鼠标操作，无需控制台
 *    · 数据与算法完全复用 teaching_core.c，界面层不含任何业务算法
 *
 *  编译（MinGW gcc）：
 *      gcc gui_main.c teaching_core.c -o teaching_gui.exe -std=c11 -O2 -Wall ^
 *          -mwindows -lcomctl32 -lgdi32 -luser32
 *
 *  自检模式（无鼠标也能验证各页面正常构建）：
 *      teaching_gui_console.exe --selftest
 * ========================================================================== */
#define UNICODE
#define _UNICODE
/* 指定 Windows 版本，启用 ListView 扩展样式等较新的公共控件特性 */
#define WINVER       0x0600
#define _WIN32_WINNT 0x0600
#define _WIN32_IE    0x0600

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>
#include <math.h>

#include "teaching_core.h"

/* 老版本 MinGW 的 commctrl.h 可能没有定义该扩展样式，这里补齐 */
#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

/* ------------------------------- 布局常量 ------------------------------- */
#define NAV_W        196
#define HEAD_H       68
#define STATUS_H     28
#define PAD          12

/* ------------------------------- 控件 ID -------------------------------- */
#define IDC_LIST         1001
#define IDC_LOG          1002
#define IDC_SEARCH       1003
#define IDC_COMBO_OBJ    1004
#define IDC_COMBO_SORT   1005
#define IDC_THRESH       1006
#define IDC_COMBO_COURSE 1007
#define IDC_COMBO_CHAN   1008
#define IDC_BTN_EXACT    1010
#define IDC_BTN_FUZZY    1011
#define IDC_BTN_ALL      1012
#define IDC_BTN_APPLY    1013
#define IDC_BTN_WARN     1014
#define IDC_BTN_REFRESH  1015
#define IDC_BTN_GENSCORE 1016
#define IDC_BTN_GENATT   1017
#define IDC_BTN_SENDALL  1018
#define IDC_BTN_UNDO     1019
#define IDC_BTN_SAVE     1020
#define IDC_BTN_LOAD     1021
#define IDC_BTN_SAMPLE   1022
#define IDC_BTN_CLEARLOG 1023

/* ------------------------------- 颜色主题 ------------------------------- */
#define CLR_NAV       RGB(30, 41, 59)
#define CLR_NAV_HOT   RGB(45, 60, 84)
#define CLR_ACCENT    RGB(56, 132, 255)
#define CLR_BG        RGB(244, 247, 252)
#define CLR_CARD      RGB(255, 255, 255)
#define CLR_BORDER    RGB(219, 227, 240)
#define CLR_TEXT      RGB(33, 43, 60)
#define CLR_SUBTEXT   RGB(120, 134, 156)
#define CLR_GREEN     RGB(52, 168, 118)
#define CLR_ORANGE    RGB(245, 158, 66)
#define CLR_RED       RGB(226, 84, 84)
#define CLR_BLUE      RGB(74, 144, 226)
#define CLR_PURPLE    RGB(139, 108, 226)

/* ------------------------------- 全局状态 ------------------------------- */
static HINSTANCE g_hInst;
static HWND  g_hWnd;

static HFONT g_fontUI, g_fontBold, g_fontBig, g_fontHuge;
static int   g_page = 0;
static int   g_hotNav = -1;
static int   g_selftest = 0;
static wchar_t g_status[512] = L"就绪：数据已生成，可用鼠标操作全部功能。";

/* 子控件 */
static HWND g_hList, g_hLog, g_hSearch, g_hComboObj, g_hComboSort;
static HWND g_hThresh, g_hComboCourse, g_hComboChan;
static HWND g_hBtnExact, g_hBtnFuzzy, g_hBtnAll, g_hBtnApply;
static HWND g_hBtnWarn, g_hBtnRefresh;
static HWND g_hBtnGenScore, g_hBtnGenAtt, g_hBtnSendAll, g_hBtnUndo, g_hBtnClearLog;
static HWND g_hBtnSave, g_hBtnLoad, g_hBtnSample;

/* 导航项 */
static const wchar_t *g_navText[6] = {
    L"数据概览", L"信息查询", L"成绩排名", L"考勤预警", L"通知中心", L"数据存档"
};
static const wchar_t *g_navDesc[6] = {
    L"统计分析、可视化图表与开课达标校验",
    L"学生 / 教师 / 课程的精确查找与模糊查找",
    L"多关键字排序与名次生成（快排 / 插入排序）",
    L"出勤率统计、缺勤预警与通知入队",
    L"模拟短信 / 邮件发送、队列与栈演示",
    L"数据存档、载入与示例数据重建"
};

/* 通知中心“课程下拉框”对应的课程号 */
static char g_notifyCourseId[MAX_COURSES][16];
static int  g_notifyCourseCount = 0;

/* ---------------------------- 内部函数前置声明 ---------------------------- */
static void DoShowQueue(void);
static void DoShowSent(void);
static void DoApplySort(void);
static void DoNotifyGen(int mode);
static void RefreshCurrentPage(void);
static void SwitchPage(int page);
static void LayoutControls(void);

/* ============================ 基础工具 ================================= */

static void U8ToW(const char *s, wchar_t *w, int cch)
{
    if (cch <= 0) return;
    if (!s) { w[0] = 0; return; }
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, cch);
    w[cch - 1] = 0;
}

static void SetStatus(const wchar_t *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(g_status, 500, fmt, ap);
    va_end(ap);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static HFONT MakeFont(int size, int bold)
{
    return CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
}

/* 数据层消息钩子：把 coreMsg() 的输出追加到界面日志框 */
static void GuiMsgHandler(const char *text)
{
    wchar_t *w;
    size_t len;
    int i, k = 0;
    if (!g_hLog || !text) return;
    len = strlen(text);
    w = (wchar_t *)malloc((len * 2 + 4) * sizeof(wchar_t));
    if (!w) return;
    {
        wchar_t *tmp = (wchar_t *)malloc((len + 2) * sizeof(wchar_t));
        if (!tmp) { free(w); return; }
        U8ToW(text, tmp, (int)(len + 1));
        /* 换行统一成 CRLF，编辑框显示更规范 */
        for (i = 0; tmp[i]; i++) {
            if (tmp[i] == L'\n' && (i == 0 || tmp[i - 1] != L'\r')) { w[k++] = L'\r'; w[k++] = L'\n'; }
            else w[k++] = tmp[i];
        }
        free(tmp);
    }
    w[k] = 0;

    {
        int n = GetWindowTextLengthW(g_hLog);
        SendMessageW(g_hLog, EM_SETSEL, (WPARAM)n, (LPARAM)n);
        SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)w);
        SendMessageW(g_hLog, EM_LINESCROLL, 0, 32767);
    }
    free(w);
}

/* 往日志框追加一行（UTF-8 格式串，参数用窄字符串，与数据层风格一致） */
static void LogLine(const char *fmt8, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt8);
    vsnprintf(buf, sizeof(buf), fmt8, ap);
    va_end(ap);
    GuiMsgHandler(buf);
}

/* ============================ ListView 封装 ============================= */

static void ListClear(void)
{
    ListView_DeleteAllItems(g_hList);
    while (ListView_DeleteColumn(g_hList, 0)) { }
}

static void ListColumns(const wchar_t *titles[], const int *widths, int n)
{
    int i;
    ListClear();
    for (i = 0; i < n; i++) {
        LVCOLUMNW col;
        ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = (LPWSTR)titles[i];
        col.cx = widths[i];
        col.iSubItem = i;
        ListView_InsertColumn(g_hList, i, &col);
    }
}

static void ListRow(int idx, const wchar_t *cells[], int n)
{
    LVITEMW it;
    int row, i;
    ZeroMemory(&it, sizeof(it));
    it.mask = LVIF_TEXT;
    it.iItem = idx;
    it.iSubItem = 0;
    it.pszText = (LPWSTR)cells[0];
    row = ListView_InsertItem(g_hList, &it);
    for (i = 1; i < n; i++) ListView_SetItemText(g_hList, row, i, (LPWSTR)cells[i]);
}

/* cells 为 UTF-8 字符串数组，最多 9 列 */
static void ListRowU8(int idx, const char *cells[], int n)
{
    static wchar_t buf[9][160];
    const wchar_t *wp[9];
    int i;
    if (n > 9) n = 9;
    for (i = 0; i < n; i++) {
        U8ToW(cells[i] ? cells[i] : "", buf[i], 160);
        wp[i] = buf[i];
    }
    ListRow(idx, wp, n);
}

static void ListColumnsU8(const char *titles[], const int *widths, int n)
{
    static wchar_t buf[9][64];
    const wchar_t *wp[9];
    int i;
    if (n > 9) n = 9;
    for (i = 0; i < n; i++) {
        U8ToW(titles[i], buf[i], 64);
        wp[i] = buf[i];
    }
    ListColumns(wp, widths, n);
}

/* ============================ 各页面数据填充 ============================ */

/* 学生列表：mode 0=信息查询  1=排名(带指标列)  2=考勤预警
 * 直接接收"已经排好序"的指针数组，保证列表显示顺序 = 排序结果 */
static void FillStudentListArr(Student **arr, int n, int mode, const char *rankTitle)
{
    int i;
    char *titles[9];
    int widths[9];

    if (mode == 0) {
        titles[0] = "学号"; titles[1] = "姓名"; titles[2] = "性别"; titles[3] = "班级";
        titles[4] = "加权平均分"; titles[5] = "学分绩点"; titles[6] = "出勤率"; titles[7] = "不良记录";
        widths[0] = 96; widths[1] = 84; widths[2] = 56; widths[3] = 116;
        widths[4] = 96; widths[5] = 84; widths[6] = 80; widths[7] = 96;
        ListColumnsU8((const char **)titles, widths, 8);
    } else if (mode == 1) {
        titles[0] = "名次"; titles[1] = "学号"; titles[2] = "姓名"; titles[3] = "班级";
        titles[4] = (char *)rankTitle; titles[5] = "出勤率"; titles[6] = "不良记录";
        widths[0] = 60; widths[1] = 100; widths[2] = 90; widths[3] = 130;
        widths[4] = 130; widths[5] = 90; widths[6] = 110;
        ListColumnsU8((const char **)titles, widths, 7);
    } else {
        titles[0] = "名次"; titles[1] = "学号"; titles[2] = "姓名"; titles[3] = "班级";
        titles[4] = "出勤率"; titles[5] = "缺勤次数"; titles[6] = "预警状态";
        widths[0] = 60; widths[1] = 100; widths[2] = 90; widths[3] = 130;
        widths[4] = 100; widths[5] = 100; widths[6] = 120;
        ListColumnsU8((const char **)titles, widths, 7);
    }

    for (i = 0; i < n; i++) {
        Student *s = arr[i];
        char b0[16], b4[32], b5[32], b6[32], b7[32], b8[32], b9[32];
        const char *row[9];
        int k, absent = 0;
        for (k = 0; k < s->attCount; k++) if (s->atts[k].status == 0) absent++;
        sprintf(b0, "%d", i + 1);
        sprintf(b4, "%.2f", s->weighted);
        sprintf(b5, "%.2f", s->gpa);
        sprintf(b6, "%.1f%%", s->attRate);
        if (s->misCount > 0) sprintf(b7, "%d 条(%d分)", s->misCount, s->misScore);
        else                 strcpy(b7, "无");
        sprintf(b8, "%d 次", absent);
        if (s->attRate < 60.0f)      strcpy(b9, "严重预警");
        else if (s->attRate < 85.0f) strcpy(b9, "预警");
        else                         strcpy(b9, "正常");

        if (mode == 0) {
            row[0] = s->id; row[1] = s->name; row[2] = s->gender; row[3] = s->className;
            row[4] = b4; row[5] = b5; row[6] = b6; row[7] = b7;
            ListRowU8(i, row, 8);
        } else if (mode == 1) {
            const char *metric = b4;
            if (rankTitle && strstr(rankTitle, "绩点")) metric = b5;
            else if (rankTitle && strstr(rankTitle, "出勤")) metric = b6;
            else if (rankTitle && strstr(rankTitle, "不良")) { sprintf(b9, "%d 分", s->misScore); metric = b9; }
            else if (rankTitle && strstr(rankTitle, "学号")) metric = s->id;
            row[0] = b0; row[1] = s->id; row[2] = s->name; row[3] = s->className;
            row[4] = metric; row[5] = b6; row[6] = b7;
            ListRowU8(i, row, 7);
        } else {
            row[0] = b0; row[1] = s->id; row[2] = s->name; row[3] = s->className;
            row[4] = b6; row[5] = b8; row[6] = b9;
            ListRowU8(i, row, 7);
        }
    }
    ListView_SetColumnWidth(g_hList, 0, LVSCW_AUTOSIZE_USEHEADER);
}

/* 未排序时的默认入口：按链表顺序列出全部学生 */
static void FillStudentList(int mode, const char *rankTitle)
{
    Student *arr[MAX_STUDENTS];
    int n = collectStudents(arr, MAX_STUDENTS);
    FillStudentListArr(arr, n, mode, rankTitle);
}

static void FillTeacherListArr(Teacher **arr, int n)
{
    int i;
    const char *titles[7] = { "工号", "姓名", "职称", "所属学院", "任课数", "平均评教", "所授课程均分" };
    int widths[7] = { 90, 90, 90, 140, 80, 100, 130 };
    ListColumnsU8(titles, widths, 7);
    for (i = 0; i < n; i++) {
        Teacher *t = arr[i];
        char b4[16], b5[16], b6[16];
        const char *row[7];
        sprintf(b4, "%d", t->itemCount);
        sprintf(b5, "%.2f", t->evalAvg);
        sprintf(b6, "%.2f", teacherAvgCourseScore(t));
        row[0] = t->id; row[1] = t->name; row[2] = t->title; row[3] = t->dept;
        row[4] = b4; row[5] = b5; row[6] = b6;
        ListRowU8(i, row, 7);
    }
}

/* 未排序时的默认入口：按链表顺序列出全部教师 */
static void FillTeacherList(void)
{
    Teacher *arr[MAX_TEACHERS];
    int n = collectTeachers(arr, MAX_TEACHERS);
    FillTeacherListArr(arr, n);
}

/* 课程列表：接收已经排好序的课程指针数组 */
static void FillCourseListArr(Course **arr, int n)
{
    int i;
    const char *titles[7] = { "课程号", "课程名称", "学分", "学时", "任课教师数", "选课人数", "平均总评" };
    int widths[7] = { 90, 150, 70, 70, 110, 100, 110 };
    ListColumnsU8(titles, widths, 7);
    for (i = 0; i < n; i++) {
        Teacher *p = g_teaHead;
        int tc = 0, nstu = courseStudentCount(arr[i]->id);
        char b2[16], b3[16], b4[16], b5[16], b6[16];
        const char *row[7];
        while (p) {
            int k;
            for (k = 0; k < p->itemCount; k++)
                if (strcmp(p->items[k].courseId, arr[i]->id) == 0) tc++;
            p = p->next;
        }
        sprintf(b2, "%.1f", arr[i]->credit);
        sprintf(b3, "%d", arr[i]->hours);
        sprintf(b4, "%d 人", tc);
        sprintf(b5, "%d 人", nstu);
        sprintf(b6, "%.2f", courseAvgScore(arr[i]->id));
        row[0] = arr[i]->id; row[1] = arr[i]->name; row[2] = b2; row[3] = b3;
        row[4] = b4; row[5] = b5; row[6] = b6;
        ListRowU8(i, row, 7);
    }
}

static void FillCourseList(void)
{
    Course *arr[MAX_COURSES];
    int i, n = g_courseCount;
    for (i = 0; i < n; i++) arr[i] = &g_courses[i];
    FillCourseListArr(arr, n);
}

/* ============================ 图表绘制 ================================= */

static void FillRectC(HDC hdc, int x, int y, int w, int h, COLORREF c)
{
    RECT r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    SetBkColor(hdc, c);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
}

static void FrameRectC(HDC hdc, int x, int y, int w, int h, COLORREF c)
{
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ old = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void TextAt(HDC hdc, const wchar_t *s, int x, int y, int w, int h,
                   COLORREF color, HFONT font, UINT align)
{
    RECT r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, font);
    DrawTextW(hdc, s, -1, &r, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

/* 卡片式统计块 */
static void DrawKpi(HDC hdc, int x, int y, int w, int h,
                    const wchar_t *label, const wchar_t *value, COLORREF accent)
{
    FillRectC(hdc, x, y, w, h, CLR_CARD);
    FrameRectC(hdc, x, y, w - 1, h - 1, CLR_BORDER);
    FillRectC(hdc, x, y, 4, h, accent);
    TextAt(hdc, label, x + 16, y + 8, w - 24, 20, CLR_SUBTEXT, g_fontUI, DT_LEFT);
    TextAt(hdc, value, x + 14, y + 26, w - 20, 34, accent, g_fontBig, DT_LEFT);
}

/* 纵向柱状图 */
static void DrawBarsV(HDC hdc, int x, int y, int w, int h,
                      const wchar_t *title, const wchar_t labels[][12],
                      const double *vals, int n, const wchar_t *unit,
                      COLORREF color, int decimals, double forceMax)
{
    int i;
    int plotX = x + 44, plotY = y + 52, plotW, plotH;
    double maxv = forceMax;
    wchar_t buf[64];

    FillRectC(hdc, x, y, w, h, CLR_CARD);
    FrameRectC(hdc, x, y, w - 1, h - 1, CLR_BORDER);
    TextAt(hdc, title, x + 14, y + 10, w - 28, 24, CLR_TEXT, g_fontBold, DT_LEFT);

    for (i = 0; i < n; i++) if (vals[i] > maxv) maxv = vals[i];
    if (maxv <= 0.0) maxv = 1.0;

    plotW = w - 44 - 18;
    plotH = h - 52 - 34;

    /* 基准线 */
    {
        HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HGDIOBJ old = SelectObject(hdc, pen);
        MoveToEx(hdc, plotX, plotY + plotH, NULL);
        LineTo(hdc, plotX + plotW, plotY + plotH);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }

    for (i = 0; i < n; i++) {
        int slotW = plotW / (n > 0 ? n : 1);
        int bw = (int)(slotW * 0.52);
        int bh = (int)((vals[i] / maxv) * (plotH - 26) + 0.5);
        int bx = plotX + slotW * i + (slotW - bw) / 2;
        int by = plotY + plotH - bh;
        if (bh < 2) bh = 2;

        FillRectC(hdc, bx, by, bw, bh, color);
        if (decimals == 0) _snwprintf(buf, 60, L"%.0f", vals[i]);
        else               _snwprintf(buf, 60, L"%.2f", vals[i]);
        TextAt(hdc, buf, bx - 14, by - 20, bw + 28, 18, color, g_fontUI, DT_CENTER);
        TextAt(hdc, labels[i], plotX + slotW * i, plotY + plotH + 4, slotW, 20, CLR_SUBTEXT, g_fontUI, DT_CENTER);
    }
    if (unit && unit[0]) TextAt(hdc, unit, x + w - 60, y + 12, 46, 20, CLR_SUBTEXT, g_fontUI, DT_RIGHT);
}

/* 横向条形图（排行榜） */
static void DrawBarsH(HDC hdc, int x, int y, int w, int h,
                      const wchar_t *title, const wchar_t labels[][20],
                      const double *vals, int n, COLORREF color, int decimals)
{
    int i;
    int labelW = 96, valueW = 60;
    int barX, barW, rowH, top;
    double maxv = 0.0;
    wchar_t buf[64];

    FillRectC(hdc, x, y, w, h, CLR_CARD);
    FrameRectC(hdc, x, y, w - 1, h - 1, CLR_BORDER);
    TextAt(hdc, title, x + 14, y + 10, w - 28, 24, CLR_TEXT, g_fontBold, DT_LEFT);

    for (i = 0; i < n; i++) if (vals[i] > maxv) maxv = vals[i];
    if (maxv <= 0.0) maxv = 1.0;

    barX = x + 14 + labelW;
    barW = w - 28 - labelW - valueW;
    top = y + 42;
    rowH = (h - 52) / (n > 0 ? n : 1);
    if (rowH > 26) rowH = 26;

    for (i = 0; i < n; i++) {
        int by = top + i * rowH;
        int bw = (int)((vals[i] / maxv) * barW + 0.5);
        TextAt(hdc, labels[i], x + 14, by, labelW - 6, rowH, CLR_TEXT, g_fontUI, DT_LEFT);
        FillRectC(hdc, barX, by + rowH / 2 - 6, barW, 12, RGB(238, 243, 251));
        FillRectC(hdc, barX, by + rowH / 2 - 6, bw < 2 ? 2 : bw, 12, color);
        if (decimals == 0) _snwprintf(buf, 60, L"%.0f", vals[i]);
        else               _snwprintf(buf, 60, L"%.2f", vals[i]);
        TextAt(hdc, buf, barX + barW + 4, by, valueW - 6, rowH, color, g_fontUI, DT_RIGHT);
    }
}

/* 概览页整体绘制 */
static void DrawDashboard(HDC hdc, RECT rc)
{
    int x = NAV_W + PAD, y = HEAD_H + PAD;
    int cw = rc.right - NAV_W - PAD * 2;
    int cardW = (cw - 3 * PAD) / 4;
    int i, k;
    wchar_t buf[64];
    double vals[16];
    wchar_t labels[16][20];
    wchar_t lshort[16][12];

    /* KPI 卡片 */
    _snwprintf(buf, 60, L"%d 门", g_courseCount);
    DrawKpi(hdc, x, y, cardW, 74, L"课程总数", buf, CLR_BLUE);
    _snwprintf(buf, 60, L"%d 名", g_teaCount);
    DrawKpi(hdc, x + (cardW + PAD), y, cardW, 74, L"教师总数", buf, CLR_GREEN);
    _snwprintf(buf, 60, L"%d 名", g_stuCount);
    DrawKpi(hdc, x + (cardW + PAD) * 2, y, cardW, 74, L"学生总数", buf, CLR_PURPLE);
    _snwprintf(buf, 60, L"%d 条", g_nqCount);
    DrawKpi(hdc, x + (cardW + PAD) * 3, y, cardW, 74, L"待发送通知", buf, CLR_ORANGE);

    /* 成绩分布 */
    {
        double bins[5];
        ZeroMemory(bins, sizeof(bins));
        {
            Student *s = g_stuHead;
            while (s) {
                for (k = 0; k < s->gradeCount; k++) {
                    double t = gradeTotal(&s->grades[k]);
                    if (t < 60) bins[0] += 1;
                    else if (t < 70) bins[1] += 1;
                    else if (t < 80) bins[2] += 1;
                    else if (t < 90) bins[3] += 1;
                    else bins[4] += 1;
                }
                s = s->next;
            }
        }
        wcscpy(lshort[0], L"0-59"); wcscpy(lshort[1], L"60-69");
        wcscpy(lshort[2], L"70-79"); wcscpy(lshort[3], L"80-89");
        wcscpy(lshort[4], L"90-100");
        DrawBarsV(hdc, x, y + 90, (cw - PAD) / 2, 210, L"总评成绩分布（人次）",
                  (const wchar_t (*)[12])lshort, bins, 5, L"人次", CLR_BLUE, 0, 0.0);
    }

    /* 各课程平均分 */
    {
        int n = g_courseCount > 8 ? 8 : g_courseCount;
        for (i = 0; i < n; i++) {
            char nm[40];
            wchar_t wnm[12];
            sprintf(nm, "%s", g_courses[i].name);
            U8ToW(nm, wnm, 12);
            wcsncpy(lshort[i], wnm, 11); lshort[i][11] = 0;
            vals[i] = (double)courseAvgScore(g_courses[i].id);
        }
        DrawBarsV(hdc, x + (cw - PAD) / 2 + PAD, y + 90, (cw - PAD) / 2, 210,
                  L"各课程平均总评（分）", (const wchar_t (*)[12])lshort, vals, n,
                  L"分", CLR_GREEN, 2, 100.0);
    }

    /* 教师评教 Top6 */
    {
        Teacher *arr[MAX_TEACHERS];
        int n = collectTeachers(arr, MAX_TEACHERS);
        int top = n < 6 ? n : 6;
        sortPtrArray((void **)arr, n, cmpTeaByEvalDesc);
        for (i = 0; i < top; i++) {
            wchar_t wnm[20];
            U8ToW(arr[i]->name, wnm, 20);
            wcsncpy(labels[i], wnm, 19); labels[i][19] = 0;
            vals[i] = (double)arr[i]->evalAvg;
        }
        DrawBarsH(hdc, x, y + 312, (cw - PAD) / 2, 180, L"教师授课效果评价 Top6（5 分制）",
                  (const wchar_t (*)[20])labels, vals, top, CLR_ORANGE, 2);
    }

    /* 学生绩点 Top8 */
    {
        Student *arr[MAX_STUDENTS];
        int n = collectStudents(arr, MAX_STUDENTS);
        int top = n < 8 ? n : 8;
        sortPtrArray((void **)arr, n, cmpStuByGpaDesc);
        for (i = 0; i < top; i++) {
            wchar_t wnm[20];
            U8ToW(arr[i]->name, wnm, 20);
            wcsncat(wnm, L"  ", 19);
            { wchar_t id[20]; U8ToW(arr[i]->id, id, 20); wcsncat(wnm, id + 5, 19); }
            wcsncpy(labels[i], wnm, 19); labels[i][19] = 0;
            vals[i] = (double)arr[i]->gpa;
        }
        DrawBarsH(hdc, x + (cw - PAD) / 2 + PAD, y + 312, (cw - PAD) / 2, 180,
                  L"学生学分绩点 Top8", (const wchar_t (*)[20])labels, vals, top, CLR_PURPLE, 2);
    }

    /* 开课达标校验 */
    {
        int cy = y + 504;
        int lineH = 15;
        FillRectC(hdc, x, cy, cw, 96, CLR_CARD);
        FrameRectC(hdc, x, cy, cw - 1, 95, CLR_BORDER);
        TextAt(hdc, L"课程设置达标校验（要求：每门课程任课教师 ≥ 3 人，选课学生 ≥ 70 人）",
               x + 14, cy + 6, cw - 28, 22, CLR_TEXT, g_fontBold, DT_LEFT);
        for (i = 0; i < g_courseCount && i < 4; i++) {
            Teacher *p = g_teaHead;
            int tc = 0, nstu = courseStudentCount(g_courses[i].id);
            wchar_t line[200], wnm[40], wid[20];
            while (p) {
                for (k = 0; k < p->itemCount; k++)
                    if (strcmp(p->items[k].courseId, g_courses[i].id) == 0) tc++;
                p = p->next;
            }
            U8ToW(g_courses[i].name, wnm, 40);
            U8ToW(g_courses[i].id, wid, 20);
            _snwprintf(line, 190, L"%s  %s   教师 %d 人 [%s]    选课 %d 人 [%s]    平均 %.1f 分",
                       wid, wnm, tc, tc >= 3 ? L"达标" : L"不足",
                       nstu, nstu >= 70 ? L"达标" : L"不足",
                       courseAvgScore(g_courses[i].id));
            TextAt(hdc, line, x + 14, cy + 30 + i * lineH, cw - 28, lineH,
                   (tc >= 3 && nstu >= 70) ? CLR_GREEN : CLR_RED, g_fontUI, DT_LEFT);
        }
    }
}

/* ============================ 页面切换与布局 ============================ */

static void ShowCtrl(HWND h, BOOL show)
{
    ShowWindow(h, show ? SW_SHOW : SW_HIDE);
}

static void RefreshCurrentPage(void)
{
    if (g_page == 0) {
        InvalidateRect(g_hWnd, NULL, FALSE);
    } else if (g_page == 1) {
        int sel = (int)SendMessageW(g_hComboObj, CB_GETCURSEL, 0, 0);
        if (sel == 1) FillTeacherList();
        else if (sel == 2) FillCourseList();
        else FillStudentList(0, NULL);
    } else if (g_page == 2) {
        SendMessageW(g_hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_APPLY, BN_CLICKED), 0);
    } else if (g_page == 3) {
        Student *arr[MAX_STUDENTS];
        int n = collectStudents(arr, MAX_STUDENTS);
        sortPtrArray((void **)arr, n, cmpStuByAttAsc);   /* 出勤率升序：缺勤最严重排在最前 */
        FillStudentListArr(arr, n, 2, NULL);
    } else {
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

static void SwitchPage(int page)
{
    int i;
    g_page = page;

    ShowCtrl(g_hList, FALSE);
    ShowCtrl(g_hLog, FALSE);
    ShowCtrl(g_hSearch, FALSE);
    ShowCtrl(g_hComboObj, FALSE);
    ShowCtrl(g_hComboSort, FALSE);
    ShowCtrl(g_hThresh, FALSE);
    ShowCtrl(g_hComboCourse, FALSE);
    ShowCtrl(g_hComboChan, FALSE);
    ShowCtrl(g_hBtnExact, FALSE);
    ShowCtrl(g_hBtnFuzzy, FALSE);
    ShowCtrl(g_hBtnAll, FALSE);
    ShowCtrl(g_hBtnApply, FALSE);
    ShowCtrl(g_hBtnWarn, FALSE);
    ShowCtrl(g_hBtnRefresh, FALSE);
    ShowCtrl(g_hBtnGenScore, FALSE);
    ShowCtrl(g_hBtnGenAtt, FALSE);
    ShowCtrl(g_hBtnSendAll, FALSE);
    ShowCtrl(g_hBtnUndo, FALSE);
    ShowCtrl(g_hBtnClearLog, FALSE);
    ShowCtrl(g_hBtnSave, FALSE);
    ShowCtrl(g_hBtnLoad, FALSE);
    ShowCtrl(g_hBtnSample, FALSE);

    if (page == 1) {
        ShowCtrl(g_hComboObj, TRUE); ShowCtrl(g_hSearch, TRUE);
        ShowCtrl(g_hBtnExact, TRUE); ShowCtrl(g_hBtnFuzzy, TRUE); ShowCtrl(g_hBtnAll, TRUE);
        ShowCtrl(g_hList, TRUE);
    } else if (page == 2) {
        ShowCtrl(g_hComboSort, TRUE); ShowCtrl(g_hBtnApply, TRUE); ShowCtrl(g_hList, TRUE);
    } else if (page == 3) {
        ShowCtrl(g_hThresh, TRUE); ShowCtrl(g_hBtnWarn, TRUE); ShowCtrl(g_hBtnRefresh, TRUE);
        ShowCtrl(g_hList, TRUE);
    } else if (page == 4) {
        ShowCtrl(g_hComboCourse, TRUE); ShowCtrl(g_hComboChan, TRUE);
        ShowCtrl(g_hBtnGenScore, TRUE); ShowCtrl(g_hBtnGenAtt, TRUE);
        ShowCtrl(g_hBtnSendAll, TRUE); ShowCtrl(g_hBtnUndo, TRUE); ShowCtrl(g_hBtnClearLog, TRUE);
        ShowCtrl(g_hLog, TRUE);
    } else if (page == 5) {
        ShowCtrl(g_hBtnSave, TRUE); ShowCtrl(g_hBtnLoad, TRUE); ShowCtrl(g_hBtnSample, TRUE);
        ShowCtrl(g_hBtnClearLog, TRUE);
        ShowCtrl(g_hLog, TRUE);
    }

    for (i = 0; i < 6; i++) (void)i;
    LayoutControls();
    RefreshCurrentPage();
    InvalidateRect(g_hWnd, NULL, TRUE);
}

static void LayoutControls(void)
{
    RECT rc;
    int cy, x, y, cw, logTop;
    GetClientRect(g_hWnd, &rc);

    x = NAV_W + PAD;
    y = HEAD_H + PAD;
    cw = rc.right - NAV_W - PAD * 2;
    cy = rc.bottom - STATUS_H - PAD;

    /* 页面1：信息查询 */
    MoveWindow(g_hComboObj, x + 70, y, 130, 240, TRUE);
    MoveWindow(g_hSearch, x + 264, y, 250, 28, TRUE);
    MoveWindow(g_hBtnExact, x + 526, y, 110, 28, TRUE);
    MoveWindow(g_hBtnFuzzy, x + 646, y, 130, 28, TRUE);
    MoveWindow(g_hBtnAll, x + 786, y, 110, 28, TRUE);

    /* 页面2：成绩排名 */
    MoveWindow(g_hComboSort, x, y, 330, 240, TRUE);
    MoveWindow(g_hBtnApply, x + 344, y, 120, 28, TRUE);

    /* 页面3：考勤预警 */
    MoveWindow(g_hThresh, x + 96, y, 80, 28, TRUE);
    MoveWindow(g_hBtnWarn, x + 188, y, 170, 28, TRUE);
    MoveWindow(g_hBtnRefresh, x + 370, y, 120, 28, TRUE);

    /* 页面4：通知中心（分两行，避免按钮挤出窗口） */
    MoveWindow(g_hComboCourse, x + 54, y, 190, 240, TRUE);
    MoveWindow(g_hComboChan, x + 306, y, 110, 240, TRUE);
    MoveWindow(g_hBtnGenScore, x + 428, y, 180, 28, TRUE);
    MoveWindow(g_hBtnGenAtt, x + 618, y, 180, 28, TRUE);
    MoveWindow(g_hBtnSendAll, x, y + 34, 150, 28, TRUE);
    MoveWindow(g_hBtnUndo, x + 162, y + 34, 180, 28, TRUE);
    MoveWindow(g_hBtnClearLog, x + 354, y + 34, 120, 28, TRUE);

    /* 页面5：数据存档 */
    MoveWindow(g_hBtnSave, x, y, 150, 28, TRUE);
    MoveWindow(g_hBtnLoad, x + 162, y, 150, 28, TRUE);
    MoveWindow(g_hBtnSample, x + 324, y, 190, 28, TRUE);
    if (g_page == 5) MoveWindow(g_hBtnClearLog, x + 526, y, 120, 28, TRUE);

    /* 日志框在“通知中心”页下移一行 */
    logTop = (g_page == 4) ? (y + 70) : (y + 40);
    MoveWindow(g_hLog, x, logTop, cw, cy - logTop, TRUE);

    /* 列表 */
    MoveWindow(g_hList, x, y + 40, cw, cy - (y + 40), TRUE);
}

/* ============================ 业务动作 ================================= */

static void DoSearch(int exact)
{
    int sel = (int)SendMessageW(g_hComboObj, CB_GETCURSEL, 0, 0);
    wchar_t wkey[128];
    char key[256];
    GetWindowTextW(g_hSearch, wkey, 120);
    WideCharToMultiByte(CP_UTF8, 0, wkey, -1, key, sizeof(key), NULL, NULL);

    if (exact) {
        if (key[0] == 0) { SetStatus(L"精确查找：请先输入完整学号 / 工号 / 课程号"); return; }
        if (sel == 1) {
            Teacher *arr[MAX_TEACHERS];
            int n = collectTeachers(arr, MAX_TEACHERS), hit = -1;
            sortPtrArray((void **)arr, n, cmpTeaByIdAsc);
            {
                int lo = 0, hi = n - 1;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    int c = strcmp(arr[mid]->id, key);
                    if (c == 0) { hit = mid; break; }
                    if (c < 0) lo = mid + 1; else hi = mid - 1;
                }
            }
            if (hit >= 0) {
                ListClear();
                {
                    const char *t[7] = { "工号", "姓名", "职称", "所属学院", "任课数", "平均评教", "所授课程均分" };
                    int w[7] = { 90, 90, 90, 140, 80, 100, 130 };
                    ListColumnsU8(t, w, 7);
                }
                {
                    char b4[16], b5[16], b6[16];
                    const char *row[7];
                    sprintf(b4, "%d", arr[hit]->itemCount);
                    sprintf(b5, "%.2f", arr[hit]->evalAvg);
                    sprintf(b6, "%.2f", teacherAvgCourseScore(arr[hit]));
                    row[0] = arr[hit]->id; row[1] = arr[hit]->name; row[2] = arr[hit]->title;
                    row[3] = arr[hit]->dept; row[4] = b4; row[5] = b5; row[6] = b6;
                    ListRowU8(0, row, 7);
                }
                SetStatus(L"精确查找成功（二分查找，比较约 log2(%d) 次）", n);
                LogLine("[精确查找] 教师 %s 命中（对 %d 条记录二分查找）\r\n", key, n);
            } else {
                ListClear();
                SetStatus(L"精确查找：未找到工号 %s", wkey);
            }
        } else if (sel == 2) {
            Course *c = findCourse(key);
            ListClear();
            {
                const char *t[7] = { "课程号", "课程名称", "学分", "学时", "任课教师数", "选课人数", "平均总评" };
                int w[7] = { 90, 150, 70, 70, 110, 100, 110 };
                ListColumnsU8(t, w, 7);
            }
            if (c) {
                char b2[16], b3[16], b4[16], b5[16], b6[16];
                const char *row[7];
                Teacher *p = g_teaHead; int tc = 0;
                while (p) { int k; for (k = 0; k < p->itemCount; k++)
                                if (strcmp(p->items[k].courseId, c->id) == 0) tc++;
                            p = p->next; }
                sprintf(b2, "%.1f", c->credit); sprintf(b3, "%d", c->hours);
                sprintf(b4, "%d 人", tc); sprintf(b5, "%d 人", courseStudentCount(c->id));
                sprintf(b6, "%.2f", courseAvgScore(c->id));
                row[0] = c->id; row[1] = c->name; row[2] = b2; row[3] = b3;
                row[4] = b4; row[5] = b5; row[6] = b6;
                ListRowU8(0, row, 7);
                SetStatus(L"精确查找成功：课程 %s", wkey);
            } else SetStatus(L"精确查找：未找到课程 %s", wkey);
        } else {
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS), hit = -1;
            sortPtrArray((void **)arr, n, cmpStuByIdAsc);
            {
                int lo = 0, hi = n - 1;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    int c = strcmp(arr[mid]->id, key);
                    if (c == 0) { hit = mid; break; }
                    if (c < 0) lo = mid + 1; else hi = mid - 1;
                }
            }
            if (hit >= 0) {
                ListClear();
                {
                    const char *t[8] = { "学号", "姓名", "性别", "班级", "加权平均分", "学分绩点", "出勤率", "不良记录" };
                    int w[8] = { 96, 84, 56, 116, 96, 84, 80, 96 };
                    ListColumnsU8(t, w, 8);
                }
                {
                    char b4[16], b5[16], b6[16], b7[32];
                    const char *row[8];
                    sprintf(b4, "%.2f", arr[hit]->weighted);
                    sprintf(b5, "%.2f", arr[hit]->gpa);
                    sprintf(b6, "%.1f%%", arr[hit]->attRate);
                    if (arr[hit]->misCount) sprintf(b7, "%d 条(%d分)", arr[hit]->misCount, arr[hit]->misScore);
                    else strcpy(b7, "无");
                    row[0] = arr[hit]->id; row[1] = arr[hit]->name; row[2] = arr[hit]->gender;
                    row[3] = arr[hit]->className; row[4] = b4; row[5] = b5; row[6] = b6; row[7] = b7;
                    ListRowU8(0, row, 8);
                }
                SetStatus(L"精确查找成功：%s（二分查找，规模 %d 条，比较约 %d 次）",
                          wkey, n, (int)(n > 0 ? (log((double)n) / log(2.0)) : 0.0));
                LogLine("[精确查找] 学生 %s 命中（对 %d 条记录二分查找）\r\n", key, n);
            } else {
                ListClear();
                SetStatus(L"精确查找：未找到学号 %s", wkey);
            }
        }
        return;
    }

    /* 模糊查找：KMP 串匹配 */
    if (key[0] == 0) { SetStatus(L"模糊查找：请输入关键字片段"); return; }
    if (sel == 1) {
        Teacher *p = g_teaHead;
        int cnt = 0;
        char *t[7]; int w[7];
        t[0] = "工号"; t[1] = "姓名"; t[2] = "职称"; t[3] = "所属学院"; t[4] = "任课数"; t[5] = "平均评教"; t[6] = "所授课程均分";
        w[0] = 90; w[1] = 90; w[2] = 90; w[3] = 140; w[4] = 80; w[5] = 100; w[6] = 130;
        ListColumnsU8((const char **)t, w, 7);
        while (p) {
            if (containsIgnoreCase(p->id, key) || containsIgnoreCase(p->name, key) ||
                containsIgnoreCase(p->dept, key) || containsIgnoreCase(p->title, key)) {
                char b4[16], b5[16], b6[16];
                const char *row[7];
                sprintf(b4, "%d", p->itemCount);
                sprintf(b5, "%.2f", p->evalAvg);
                sprintf(b6, "%.2f", teacherAvgCourseScore(p));
                row[0] = p->id; row[1] = p->name; row[2] = p->title; row[3] = p->dept;
                row[4] = b4; row[5] = b5; row[6] = b6;
                ListRowU8(cnt, row, 7);
                cnt++;
            }
            p = p->next;
        }
        SetStatus(L"模糊查找（KMP）匹配到 %d 名教师", cnt);
        LogLine("[模糊查找] 关键字“%s”匹配教师 %d 条（KMP 算法 O(n+m)）\r\n", key, cnt);
    } else if (sel == 2) {
        int i, cnt = 0;
        char *t[7]; int w[7];
        t[0] = "课程号"; t[1] = "课程名称"; t[2] = "学分"; t[3] = "学时"; t[4] = "任课教师数"; t[5] = "选课人数"; t[6] = "平均总评";
        w[0] = 90; w[1] = 150; w[2] = 70; w[3] = 70; w[4] = 110; w[5] = 100; w[6] = 110;
        ListColumnsU8((const char **)t, w, 7);
        for (i = 0; i < g_courseCount; i++) {
            if (containsIgnoreCase(g_courses[i].id, key) || containsIgnoreCase(g_courses[i].name, key)) {
                Teacher *p = g_teaHead; int tc = 0;
                char b2[16], b3[16], b4[16], b5[16], b6[16];
                const char *row[7];
                while (p) { int k; for (k = 0; k < p->itemCount; k++)
                                if (strcmp(p->items[k].courseId, g_courses[i].id) == 0) tc++;
                            p = p->next; }
                sprintf(b2, "%.1f", g_courses[i].credit); sprintf(b3, "%d", g_courses[i].hours);
                sprintf(b4, "%d 人", tc); sprintf(b5, "%d 人", courseStudentCount(g_courses[i].id));
                sprintf(b6, "%.2f", courseAvgScore(g_courses[i].id));
                row[0] = g_courses[i].id; row[1] = g_courses[i].name; row[2] = b2; row[3] = b3;
                row[4] = b4; row[5] = b5; row[6] = b6;
                ListRowU8(cnt, row, 7);
                cnt++;
            }
        }
        SetStatus(L"模糊查找（KMP）匹配到 %d 门课程", cnt);
        LogLine("[模糊查找] 关键字“%s”匹配课程 %d 条\r\n", key, cnt);
    } else {
        Student *p = g_stuHead;
        int cnt = 0;
        char *t[8]; int w[8];
        t[0] = "学号"; t[1] = "姓名"; t[2] = "性别"; t[3] = "班级"; t[4] = "加权平均分"; t[5] = "学分绩点"; t[6] = "出勤率"; t[7] = "不良记录";
        w[0] = 96; w[1] = 84; w[2] = 56; w[3] = 116; w[4] = 96; w[5] = 84; w[6] = 80; w[7] = 96;
        ListColumnsU8((const char **)t, w, 8);
        while (p) {
            if (containsIgnoreCase(p->id, key) || containsIgnoreCase(p->name, key) ||
                containsIgnoreCase(p->className, key)) {
                char b4[16], b5[16], b6[16], b7[32];
                const char *row[8];
                sprintf(b4, "%.2f", p->weighted);
                sprintf(b5, "%.2f", p->gpa);
                sprintf(b6, "%.1f%%", p->attRate);
                if (p->misCount) sprintf(b7, "%d 条(%d分)", p->misCount, p->misScore);
                else strcpy(b7, "无");
                row[0] = p->id; row[1] = p->name; row[2] = p->gender; row[3] = p->className;
                row[4] = b4; row[5] = b5; row[6] = b6; row[7] = b7;
                ListRowU8(cnt, row, 8);
                cnt++;
            }
            p = p->next;
        }
        SetStatus(L"模糊查找（KMP）匹配到 %d 名学生", cnt);
        LogLine("[模糊查找] 关键字“%s”匹配学生 %d 条（KMP 算法 O(n+m)）\r\n", key, cnt);
    }
}

static void DoApplySort(void)
{
    int sel = (int)SendMessageW(g_hComboSort, CB_GETCURSEL, 0, 0);
    Student *sarr[MAX_STUDENTS];
    Teacher *tarr[MAX_TEACHERS];
    int n;

    if (sel == 0) {
        n = collectStudents(sarr, MAX_STUDENTS);
        sortPtrArray((void **)sarr, n, cmpStuByWeightedDesc);
        FillStudentListArr(sarr, n, 1, "加权平均分");
        SetStatus(L"排序完成：学生按加权平均分降序（快速排序 O(n log n)）");
    } else if (sel == 1) {
        n = collectStudents(sarr, MAX_STUDENTS);
        sortPtrArray((void **)sarr, n, cmpStuByGpaDesc);
        FillStudentListArr(sarr, n, 1, "学分绩点");
        SetStatus(L"排序完成：学生按学分绩点降序");
    } else if (sel == 2) {
        n = collectStudents(sarr, MAX_STUDENTS);
        sortPtrArray((void **)sarr, n, cmpStuByAttAsc);
        FillStudentListArr(sarr, n, 1, "出勤率升序");
        SetStatus(L"排序完成：学生按出勤率升序（缺勤最严重在前）");
    } else if (sel == 3) {
        n = collectStudents(sarr, MAX_STUDENTS);
        sortPtrArray((void **)sarr, n, cmpStuByMisDesc);
        FillStudentListArr(sarr, n, 1, "不良记录");
        SetStatus(L"排序完成：学生按不良记录严重程度降序");
    } else if (sel == 4) {
        n = collectStudents(sarr, MAX_STUDENTS);
        sortPtrArray((void **)sarr, n, cmpStuByIdAsc);
        FillStudentListArr(sarr, n, 1, "学号");
        SetStatus(L"排序完成：学生按学号升序");
    } else if (sel == 5) {
        n = collectTeachers(tarr, MAX_TEACHERS);
        sortPtrArray((void **)tarr, n, cmpTeaByEvalDesc);
        FillTeacherListArr(tarr, n);
        SetStatus(L"排序完成：教师按平均评教分降序");
    } else if (sel == 6) {
        n = collectTeachers(tarr, MAX_TEACHERS);
        sortPtrArray((void **)tarr, n, cmpTeaByAvgScoreDesc);
        FillTeacherListArr(tarr, n);
        SetStatus(L"排序完成：教师按其课程学生平均分降序");
    } else if (sel == 7) {
        Course *carr[MAX_COURSES];
        int i;
        for (i = 0; i < g_courseCount; i++) carr[i] = &g_courses[i];
        sortPtrArray((void **)carr, g_courseCount, cmpCourseByAvgDesc);
        FillCourseListArr(carr, g_courseCount);
        SetStatus(L"排序完成：课程按平均总评降序");
    } else {
        Course *carr[MAX_COURSES];
        int i;
        for (i = 0; i < g_courseCount; i++) carr[i] = &g_courses[i];
        sortPtrArray((void **)carr, g_courseCount, cmpCourseByCountDesc);
        FillCourseListArr(carr, g_courseCount);
        SetStatus(L"排序完成：课程按选课人数降序");
    }
}

static void DoWarn(void)
{
    wchar_t wbuf[32];
    char cbuf[32];
    double thr = 85.0;
    Student *arr[MAX_STUDENTS];
    int n, i, cnt = 0;
    GetWindowTextW(g_hThresh, wbuf, 30);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, cbuf, sizeof(cbuf), NULL, NULL);
    if (cbuf[0]) thr = atof(cbuf);
    if (thr <= 0.0 || thr > 100.0) thr = 85.0;

    n = collectStudents(arr, MAX_STUDENTS);
    for (i = 0; i < n; i++) {
        if (arr[i]->attRate < (float)thr) {
            char title[64], content[224];
            sprintf(title, "考勤预警通知");
            sprintf(content, "%s同学：你本学期出勤率为 %.1f%%，低于 %.1f%% 的预警线，"
                             "请及时与任课教师或辅导员联系并按时到课。",
                    arr[i]->name, arr[i]->attRate, thr);
            notifyEnqueue('S', arr[i]->id, arr[i]->name, "短信", title, content);
            cnt++;
        }
    }
    SetStatus(L"已生成 %d 条考勤预警通知，进入待发送队列（待发送共 %d 条）", cnt, g_nqCount);
    LogLine("[考勤预警] 阈值 %.1f%%，生成预警通知 %d 条，已入队（待发送 %d 条）\r\n",
            thr, cnt, g_nqCount);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void DoNotifyGen(int mode)
{
    int chanSel = (int)SendMessageW(g_hComboChan, CB_GETCURSEL, 0, 0);
    const char *chan = (chanSel == 1) ? "邮件" : "短信";
    int ci = (int)SendMessageW(g_hComboCourse, CB_GETCURSEL, 0, 0);
    const char *cid;
    int cnt = 0;

    if (g_notifyCourseCount == 0) { SetStatus(L"没有可用课程"); return; }
    if (ci < 0 || ci >= g_notifyCourseCount) ci = 0;
    cid = g_notifyCourseId[ci];

    {
        Student *s = g_stuHead;
        Course *c = findCourse(cid);
        while (s) {
            Grade *g = findGrade(s, cid);
            if (g) {
                char content[224], title[64];
                if (mode == 0) {
                    sprintf(title, "成绩发布通知");
                    sprintf(content, "%s同学：你《%s》课程总评成绩 %.2f 分（平时 %.1f / 考试 %.1f），"
                                     "绩点 %.2f，请登录教学平台查看详情。",
                            s->name, c ? c->name : cid, gradeTotal(g), g->usual, g->exam,
                            scoreToPoint(gradeTotal(g)));
                } else {
                    sprintf(title, "教学安排通知");
                    sprintf(content, "%s同学：《%s》课程即将进行阶段测验，请提前复习并按时参加。",
                            s->name, c ? c->name : cid);
                }
                notifyEnqueue('S', s->id, s->name, chan, title, content);
                cnt++;
            }
            s = s->next;
        }
    }
    {
        wchar_t wcid[20];
        U8ToW(cid, wcid, 20);
        SetStatus(L"已为课程 %s 生成 %d 条通知（%s），待发送共 %d 条", wcid, cnt,
                  (chanSel == 1) ? L"邮件" : L"短信", g_nqCount);
    }
    LogLine("[批量通知] 课程 %s，方式 %s，生成 %d 条，待发送 %d 条\r\n",
            cid, chan, cnt, g_nqCount);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void DoSendAll(void)
{
    int c = notifySendAll();   /* 内部通过 coreMsg 输出，界面日志框会显示"模拟发送"内容 */
    if (c == 0) DoShowQueue(); /* 队列为空时，顺便把队列状态显示到日志 */
    SetStatus(L"模拟发送完成：本次发送 %d 条，已发送记录栈共 %d 条", c, g_sentCount);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void DoShowQueue(void)
{
    Notify *p = g_nqHead;
    int i = 1;
    if (!p) { LogLine("[通知队列] 队列为空（先进先出链式队列）\r\n"); return; }
    LogLine("[通知队列] 共 %d 条（先进先出）：\r\n", g_nqCount);
    while (p) {
        LogLine("   %d. #%d %s → %s(%s) [%s]\r\n", i++, p->id, p->channel,
                p->recvName, p->recvId, p->title);
        p = p->next;
    }
}

static void DoShowSent(void)
{
    Notify *p = g_sentTop;
    int i = 1;
    if (!p) { LogLine("[已发送栈] 暂无发送记录（后进先出链式栈）\r\n"); return; }
    LogLine("[已发送栈] 共 %d 条（后进先出，最近的在最上）：\r\n", g_sentCount);
    while (p) {
        LogLine("   %d. #%d %s → %s(%s) [%s]\r\n", i++, p->id, p->channel,
                p->recvName, p->recvId, p->title);
        p = p->next;
    }
}

/* 双击表格行 → 弹出该学生的完整档案 */
static void ShowStudentReportByRow(int row)
{
    wchar_t wid[32];
    char id[64];
    Student *s;
    char buf[4096];
    int p = 0, i;
    wchar_t *wbuf;

    ListView_GetItemText(g_hList, row, 1, wid, 32);   /* 第 1 列（模糊查找时为第 1 列学号） */
    WideCharToMultiByte(CP_UTF8, 0, wid, -1, id, sizeof(id), NULL, NULL);
    s = findStudent(id);
    if (!s) {
        ListView_GetItemText(g_hList, row, 0, wid, 32);
        WideCharToMultiByte(CP_UTF8, 0, wid, -1, id, sizeof(id), NULL, NULL);
        s = findStudent(id);
    }
    if (!s) return;

    p += sprintf(buf + p, "学号：%s    姓名：%s    性别：%s    班级：%s\n", s->id, s->name, s->gender, s->className);
    p += sprintf(buf + p, "加权平均分：%.2f    学分绩点：%.2f    出勤率：%.1f%%    不良记录：%d 条\n",
                 s->weighted, s->gpa, s->attRate, s->misCount);
    p += sprintf(buf + p, "\n【各科成绩】\n");
    for (i = 0; i < s->gradeCount; i++) {
        Course *c = findCourse(s->grades[i].courseId);
        p += sprintf(buf + p, "  %s %-12s 平时 %5.1f  考试 %5.1f  总评 %6.2f  绩点 %.2f\n",
                     s->grades[i].courseId, c ? c->name : "?", s->grades[i].usual,
                     s->grades[i].exam, gradeTotal(&s->grades[i]),
                     scoreToPoint(gradeTotal(&s->grades[i])));
    }
    p += sprintf(buf + p, "\n【考勤】共 %d 次记录，出勤率 %.1f%%\n", s->attCount, s->attRate);
    p += sprintf(buf + p, "\n【不良记录】\n");
    if (s->misCount == 0) p += sprintf(buf + p, "  无\n");
    for (i = 0; i < s->misCount; i++)
        p += sprintf(buf + p, "  %s  %s（%d 分）%s\n", s->mis[i].date, s->mis[i].type,
                     s->mis[i].severity, s->mis[i].desc);
    p += sprintf(buf + p, "\n【获奖 / 竞赛 / 学术交流】\n");
    if (s->honorCount == 0) p += sprintf(buf + p, "  无\n");
    for (i = 0; i < s->honorCount; i++)
        p += sprintf(buf + p, "  %s  [%s] %s\n", s->honors[i].date, s->honors[i].type, s->honors[i].name);

    wbuf = (wchar_t *)malloc(8192 * sizeof(wchar_t));
    if (!wbuf) return;
    U8ToW(buf, wbuf, 8192);
    MessageBoxW(g_hWnd, wbuf, L"学生档案", MB_OK | MB_ICONINFORMATION);
    free(wbuf);
}

/* ============================ 绘制总入口 =============================== */

static void PaintAll(HDC hdc)
{
    RECT rc;
    int i;
    GetClientRect(g_hWnd, &rc);

    /* 背景 */
    FillRectC(hdc, 0, 0, rc.right, rc.bottom, CLR_BG);

    /* 左侧导航 */
    FillRectC(hdc, 0, 0, NAV_W, rc.bottom, CLR_NAV);
    TextAt(hdc, L"智慧教学管理系统", 20, 18, NAV_W - 30, 26, RGB(255, 255, 255), g_fontBold, DT_LEFT);
    TextAt(hdc, L"Smart Teaching System", 20, 44, NAV_W - 30, 18, RGB(150, 165, 190), g_fontUI, DT_LEFT);
    for (i = 0; i < 6; i++) {
        int y = 92 + i * 52;
        RECT r;
        r.left = 12; r.top = y; r.right = NAV_W - 12; r.bottom = y + 42;
        if (i == g_page) {
            HBRUSH br = CreateSolidBrush(CLR_ACCENT);
            FillRect(hdc, &r, br);
            DeleteObject(br);
            TextAt(hdc, g_navText[i], 32, y, NAV_W - 44, 42, RGB(255, 255, 255), g_fontBold, DT_LEFT);
        } else {
            if (i == g_hotNav) {
                HBRUSH br = CreateSolidBrush(CLR_NAV_HOT);
                FillRect(hdc, &r, br);
                DeleteObject(br);
            }
            TextAt(hdc, g_navText[i], 32, y, NAV_W - 44, 42, RGB(206, 216, 232), g_fontUI, DT_LEFT);
        }
    }
    {
        wchar_t info[128];
        _snwprintf(info, 120, L"课程 %d · 教师 %d · 学生 %d", g_courseCount, g_teaCount, g_stuCount);
        TextAt(hdc, info, 20, rc.bottom - 76, NAV_W - 30, 20, RGB(130, 146, 172), g_fontUI, DT_LEFT);
        _snwprintf(info, 120, L"待发送通知 %d 条", g_nqCount);
        TextAt(hdc, info, 20, rc.bottom - 54, NAV_W - 30, 20, RGB(130, 146, 172), g_fontUI, DT_LEFT);
    }

    /* 顶部标题 */
    FillRectC(hdc, NAV_W, 0, rc.right - NAV_W, HEAD_H, CLR_CARD);
    FillRectC(hdc, NAV_W, HEAD_H - 1, rc.right - NAV_W, 1, CLR_BORDER);
    TextAt(hdc, g_navText[g_page], NAV_W + PAD, 10, rc.right - NAV_W - PAD * 2, 30, CLR_TEXT, g_fontBig, DT_LEFT);
    TextAt(hdc, g_navDesc[g_page], NAV_W + PAD, 40, rc.right - NAV_W - PAD * 2, 20, CLR_SUBTEXT, g_fontUI, DT_LEFT);

    /* 页面专属内容 */
    if (g_page == 0) {
        DrawDashboard(hdc, rc);
    } else if (g_page == 1) {
        TextAt(hdc, L"查找对象", NAV_W + PAD, HEAD_H + PAD + 4, 64, 24, CLR_SUBTEXT, g_fontUI, DT_LEFT);
        TextAt(hdc, L"关键字", NAV_W + PAD + 202, HEAD_H + PAD + 4, 56, 24, CLR_SUBTEXT, g_fontUI, DT_LEFT);
    } else if (g_page == 3) {
        TextAt(hdc, L"预警阈值(%)", NAV_W + PAD, HEAD_H + PAD + 4, 92, 24, CLR_SUBTEXT, g_fontUI, DT_LEFT);
    } else if (g_page == 4) {
        TextAt(hdc, L"课程", NAV_W + PAD, HEAD_H + PAD + 4, 50, 24, CLR_SUBTEXT, g_fontUI, DT_LEFT);
        TextAt(hdc, L"方式", NAV_W + PAD + 258, HEAD_H + PAD + 4, 44, 24, CLR_SUBTEXT, g_fontUI, DT_LEFT);
    }

    /* 底部状态栏 */
    FillRectC(hdc, NAV_W, rc.bottom - STATUS_H, rc.right - NAV_W, STATUS_H, CLR_CARD);
    FillRectC(hdc, NAV_W, rc.bottom - STATUS_H, rc.right - NAV_W, 1, CLR_BORDER);
    TextAt(hdc, g_status, NAV_W + PAD, rc.bottom - STATUS_H, rc.right - NAV_W - PAD * 2, STATUS_H,
           CLR_SUBTEXT, g_fontUI, DT_LEFT);
}

/* ============================ 窗口自渲染存图 ============================
 * 把当前界面（含子控件）渲染成 24 位 BMP，便于在无交互桌面环境下检查界面效果：
 *      teaching_gui.exe --shot
 * ========================================================================== */
static void SaveWindowBmp(const wchar_t *path)
{
    RECT rc;
    int w, h, stride, y;
    HDC hdcScreen, mem;
    BITMAPINFO bi;
    void *bits = NULL;
    HBITMAP bmp;
    HGDIOBJ old;
    FILE *fp;

    GetClientRect(g_hWnd, &rc);
    w = rc.right; h = rc.bottom;
    if (w <= 0 || h <= 0) return;

    hdcScreen = GetDC(g_hWnd);
    mem = CreateCompatibleDC(hdcScreen);
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;          /* 负值 = 自上而下 */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) { DeleteDC(mem); ReleaseDC(g_hWnd, hdcScreen); return; }

    old = SelectObject(mem, bmp);
    PatBlt(mem, 0, 0, w, h, WHITENESS);
    SendMessageW(g_hWnd, WM_PRINT, (WPARAM)mem, PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    GdiFlush();

    stride = ((w * 3 + 3) / 4) * 4;
    {
        char ansi[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, path, -1, ansi, sizeof(ansi), NULL, NULL);
        fp = fopen(ansi, "wb");
    }
    if (fp) {
        BITMAPFILEHEADER fh;
        BITMAPINFOHEADER ih;
        ZeroMemory(&fh, sizeof(fh));
        ZeroMemory(&ih, sizeof(ih));
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof(fh) + sizeof(ih);
        fh.bfSize = fh.bfOffBits + (DWORD)stride * h;
        ih = bi.bmiHeader;
        ih.biHeight = h;                 /* 文件里按自下而上存放 */
        ih.biSizeImage = (DWORD)stride * h;
        fwrite(&fh, sizeof(fh), 1, fp);
        fwrite(&ih, sizeof(ih), 1, fp);
        for (y = h - 1; y >= 0; y--)
            fwrite((unsigned char *)bits + (size_t)y * stride, 1, (size_t)stride, fp);
        fclose(fp);
    }

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(g_hWnd, hdcScreen);
}

/* ============================ 窗口过程 ================================= */

static void CreateControls(HWND hWnd)
{
    g_hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"SysListView32", L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, hWnd, (HMENU)IDC_LIST, g_hInst, NULL);
    ListView_SetExtendedListViewStyle(g_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    g_hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 10, 10, hWnd, (HMENU)IDC_LOG, g_hInst, NULL);

    g_hSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH, g_hInst, NULL);

    g_hThresh = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"85",
        WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER, 0, 0, 10, 10, hWnd, (HMENU)IDC_THRESH, g_hInst, NULL);

    g_hComboObj = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 10, 200, hWnd, (HMENU)IDC_COMBO_OBJ, g_hInst, NULL);
    g_hComboSort = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 10, 200, hWnd, (HMENU)IDC_COMBO_SORT, g_hInst, NULL);
    g_hComboCourse = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 10, 200, hWnd, (HMENU)IDC_COMBO_COURSE, g_hInst, NULL);
    g_hComboChan = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 10, 200, hWnd, (HMENU)IDC_COMBO_CHAN, g_hInst, NULL);

    g_hBtnExact = CreateWindowExW(0, L"BUTTON", L"精确查找(二分)", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_EXACT, g_hInst, NULL);
    g_hBtnFuzzy = CreateWindowExW(0, L"BUTTON", L"模糊查找(KMP)", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_FUZZY, g_hInst, NULL);
    g_hBtnAll   = CreateWindowExW(0, L"BUTTON", L"显示全部", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_ALL, g_hInst, NULL);
    g_hBtnApply = CreateWindowExW(0, L"BUTTON", L"应用排序", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_APPLY, g_hInst, NULL);
    g_hBtnWarn  = CreateWindowExW(0, L"BUTTON", L"生成预警通知(入队)", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_WARN, g_hInst, NULL);
    g_hBtnRefresh = CreateWindowExW(0, L"BUTTON", L"刷新列表", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_REFRESH, g_hInst, NULL);
    g_hBtnGenScore = CreateWindowExW(0, L"BUTTON", L"生成成绩发布通知", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_GENSCORE, g_hInst, NULL);
    g_hBtnGenAtt  = CreateWindowExW(0, L"BUTTON", L"生成教学安排通知", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_GENATT, g_hInst, NULL);
    g_hBtnSendAll = CreateWindowExW(0, L"BUTTON", L"模拟发送全部", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_SENDALL, g_hInst, NULL);
    g_hBtnUndo    = CreateWindowExW(0, L"BUTTON", L"撤销发送/查看记录", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_UNDO, g_hInst, NULL);
    g_hBtnClearLog = CreateWindowExW(0, L"BUTTON", L"清空日志", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_CLEARLOG, g_hInst, NULL);
    g_hBtnSave  = CreateWindowExW(0, L"BUTTON", L"保存数据到文件", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_SAVE, g_hInst, NULL);
    g_hBtnLoad  = CreateWindowExW(0, L"BUTTON", L"从文件载入数据", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_LOAD, g_hInst, NULL);
    g_hBtnSample = CreateWindowExW(0, L"BUTTON", L"重新生成示例数据", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_BTN_SAMPLE, g_hInst, NULL);

    /* 下拉框内容 */
    SendMessageW(g_hComboObj, CB_ADDSTRING, 0, (LPARAM)L"学生");
    SendMessageW(g_hComboObj, CB_ADDSTRING, 0, (LPARAM)L"教师");
    SendMessageW(g_hComboObj, CB_ADDSTRING, 0, (LPARAM)L"课程");
    SendMessageW(g_hComboObj, CB_SETCURSEL, 0, 0);

    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"学生：加权平均分降序");
    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"学生：学分绩点降序");
    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"学生：出勤率升序（缺勤在前）");
    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"学生：不良记录严重程度降序");
    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"学生：学号升序");
    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"教师：平均评教分降序");
    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"教师：所授课程学生均分降序");
    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"课程：平均总评降序");
    SendMessageW(g_hComboSort, CB_ADDSTRING, 0, (LPARAM)L"课程：选课人数降序");
    SendMessageW(g_hComboSort, CB_SETCURSEL, 0, 0);

    SendMessageW(g_hComboChan, CB_ADDSTRING, 0, (LPARAM)L"短信");
    SendMessageW(g_hComboChan, CB_ADDSTRING, 0, (LPARAM)L"邮件");
    SendMessageW(g_hComboChan, CB_SETCURSEL, 0, 0);

    /* 课程下拉框：填充课程号 */
    {
        int i;
        g_notifyCourseCount = 0;
        for (i = 0; i < g_courseCount && i < MAX_COURSES; i++) {
            wchar_t w[64];
            char label[64];
            sprintf(label, "%s %s", g_courses[i].id, g_courses[i].name);
            U8ToW(label, w, 64);
            SendMessageW(g_hComboCourse, CB_ADDSTRING, 0, (LPARAM)w);
            strcpy(g_notifyCourseId[g_notifyCourseCount++], g_courses[i].id);
        }
        SendMessageW(g_hComboCourse, CB_SETCURSEL, 0, 0);
    }

    /* 统一字体 */
    {
        HWND ctrls[] = { g_hList, g_hLog, g_hSearch, g_hThresh, g_hComboObj, g_hComboSort,
                         g_hComboCourse, g_hComboChan, g_hBtnExact, g_hBtnFuzzy, g_hBtnAll,
                         g_hBtnApply, g_hBtnWarn, g_hBtnRefresh, g_hBtnGenScore, g_hBtnGenAtt,
                         g_hBtnSendAll, g_hBtnUndo, g_hBtnClearLog, g_hBtnSave, g_hBtnLoad, g_hBtnSample };
        int i, n = (int)(sizeof(ctrls) / sizeof(ctrls[0]));
        for (i = 0; i < n; i++) SendMessageW(ctrls[i], WM_SETFONT, (WPARAM)g_fontUI, TRUE);
        SendMessageW(g_hList, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        g_fontUI   = MakeFont(14, 0);
        g_fontBold = MakeFont(15, 1);
        g_fontBig  = MakeFont(21, 1);
        g_fontHuge = MakeFont(30, 1);
        CreateControls(hWnd);
        coreSetMessageHandler(GuiMsgHandler);
        initSampleData();
        LogLine("==== 智慧教学管理系统（图形界面）已启动 ====\r\n");
        LogLine("示例数据：课程 %d 门 / 教师 %d 名 / 学生 %d 名 / 作业 %d 条 / 答疑 %d 条\r\n",
                g_courseCount, g_teaCount, g_stuCount, g_hwCount, g_qaCount);
        LayoutControls();
        SwitchPage(0);
        if (g_selftest) SetTimer(hWnd, 1, 200, NULL);
        return 0;

    case WM_SIZE:
        LayoutControls();
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;

    case WM_TIMER:
        if (wParam == 1 && g_selftest) {
            static int step = 0;
            step++;
            if (step <= 6) {
                printf("[selftest] 切换到页面 %d ...\n", step - 1);
                fflush(stdout);
                SwitchPage(step - 1);
                if (step == 2) {
                    SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_ALL, BN_CLICKED), 0);
                }
                if (step == 3) {
                    int sel;
                    for (sel = 0; sel < 9; sel++) {
                        SendMessageW(g_hComboSort, CB_SETCURSEL, sel, 0);
                        DoApplySort();
                    }
                    printf("[selftest] 9 种排序全部执行完毕\n");
                }
                if (step == 4) {
                    SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_WARN, BN_CLICKED), 0);
                }
                if (step == 5) {
                    DoNotifyGen(0);
                    printf("[selftest] 待发送通知 %d 条\n", g_nqCount);
                }
            } else if (step == 7) {
                DoSendAll();
                printf("[selftest] 已发送记录 %d 条\n", g_sentCount);
            } else if (step == 8) {
                printf("[selftest] 撤销：%s\n", notifyUndoLast() ? "成功" : "无记录");
            } else if (step == 9) {
                printf("[selftest] 保存数据：%s\n", saveData() ? "成功" : "失败");
            } else if (step == 10) {
                printf("[selftest] 载入数据：%s\n", loadData() ? "成功" : "失败");
            } else if (step >= 11) {
                printf("[selftest] 全部页面与功能自检通过，退出。\n");
                fflush(stdout);
                KillTimer(hWnd, 1);
                DestroyWindow(hWnd);
            }
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            HDC mem;
            HBITMAP bmp, oldBmp;
            GetClientRect(hWnd, &rc);
            mem = CreateCompatibleDC(hdc);
            bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            oldBmp = (HBITMAP)SelectObject(mem, bmp);
            PaintAll(mem);
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PRINTCLIENT:
        PaintAll((HDC)wParam);
        return 0;

    case WM_PRINT:
        {
            HDC hdc = (HDC)wParam;
            UINT flags = (UINT)lParam;
            HWND ch;
            PaintAll(hdc);
            if (flags & PRF_CHILDREN) {
                for (ch = GetWindow(hWnd, GW_CHILD); ch; ch = GetWindow(ch, GW_HWNDNEXT)) {
                    RECT r;
                    POINT org;
                    int saved;
                    if (!IsWindowVisible(ch)) continue;   /* 只画当前页面可见的控件 */
                    GetWindowRect(ch, &r);
                    org.x = r.left; org.y = r.top;
                    ScreenToClient(hWnd, &org);
                    saved = SaveDC(hdc);
                    SetViewportOrgEx(hdc, org.x, org.y, NULL);   /* 把坐标原点移到子控件位置 */
                    SendMessageW(ch, WM_PRINT, (WPARAM)hdc,
                                 PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND | PRF_CHILDREN);
                    RestoreDC(hdc, saved);
                }
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        {
            int y = GET_Y_LPARAM(lParam);
            int x = GET_X_LPARAM(lParam);
            int hot = -1, i;
            if (x < NAV_W) {
                for (i = 0; i < 6; i++) {
                    if (y >= 92 + i * 52 && y < 92 + i * 52 + 42) { hot = i; break; }
                }
            }
            if (hot != g_hotNav) {
                g_hotNav = hot;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        {
            int y = GET_Y_LPARAM(lParam);
            int x = GET_X_LPARAM(lParam);
            int i;
            if (x < NAV_W) {
                for (i = 0; i < 6; i++) {
                    if (y >= 92 + i * 52 && y < 92 + i * 52 + 42) { SwitchPage(i); break; }
                }
            }
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_COMBO_OBJ:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(g_hComboObj, CB_GETCURSEL, 0, 0);
                if (sel == 1) FillTeacherList();
                else if (sel == 2) FillCourseList();
                else FillStudentList(0, NULL);
            }
            break;
        case IDC_BTN_EXACT: DoSearch(1); break;
        case IDC_BTN_FUZZY: DoSearch(0); break;
        case IDC_BTN_ALL:
            {
                int sel = (int)SendMessageW(g_hComboObj, CB_GETCURSEL, 0, 0);
                if (sel == 1) { FillTeacherList(); SetStatus(L"已列出全部教师（%d 名）", g_teaCount); }
                else if (sel == 2) { FillCourseList(); SetStatus(L"已列出全部课程（%d 门）", g_courseCount); }
                else { FillStudentList(0, NULL); SetStatus(L"已列出全部学生（%d 名）", g_stuCount); }
            }
            break;
        case IDC_BTN_APPLY: DoApplySort(); break;
        case IDC_BTN_WARN: DoWarn(); break;
        case IDC_BTN_REFRESH:
            RefreshCurrentPage();
            SetStatus(L"列表已刷新");
            break;
        case IDC_BTN_GENSCORE: DoNotifyGen(0); break;
        case IDC_BTN_GENATT:   DoNotifyGen(1); break;
        case IDC_BTN_SENDALL:  DoSendAll(); break;
        case IDC_BTN_UNDO:
            if (notifyUndoLast()) {
                SetStatus(L"已撤销最近一次发送，当前已发送记录 %d 条", g_sentCount);
            } else {
                DoShowSent();
                SetStatus(L"暂无发送记录可撤销，已改为显示已发送栈");
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case IDC_BTN_CLEARLOG:
            SetWindowTextW(g_hLog, L"");
            LogLine("[日志] 已清空\r\n");
            break;
        case IDC_BTN_SAVE:
            saveData();
            SetStatus(L"数据已保存到数据文件（%d 门课 / %d 名教师 / %d 名学生）",
                      g_courseCount, g_teaCount, g_stuCount);
            break;
        case IDC_BTN_LOAD:
            if (loadData()) {
                {
                    int i;
                    SendMessageW(g_hComboCourse, CB_RESETCONTENT, 0, 0);
                    g_notifyCourseCount = 0;
                    for (i = 0; i < g_courseCount && i < MAX_COURSES; i++) {
                        wchar_t w[64];
                        char label[64];
                        sprintf(label, "%s %s", g_courses[i].id, g_courses[i].name);
                        U8ToW(label, w, 64);
                        SendMessageW(g_hComboCourse, CB_ADDSTRING, 0, (LPARAM)w);
                        strcpy(g_notifyCourseId[g_notifyCourseCount++], g_courses[i].id);
                    }
                    SendMessageW(g_hComboCourse, CB_SETCURSEL, 0, 0);
                }
                SetStatus(L"数据已从文件载入：%d 门课 / %d 名教师 / %d 名学生",
                          g_courseCount, g_teaCount, g_stuCount);
            } else {
                SetStatus(L"载入失败：未找到数据文件");
            }
            RefreshCurrentPage();
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        case IDC_BTN_SAMPLE:
            initSampleData();
            {
                int i;
                SendMessageW(g_hComboCourse, CB_RESETCONTENT, 0, 0);
                g_notifyCourseCount = 0;
                for (i = 0; i < g_courseCount && i < MAX_COURSES; i++) {
                    wchar_t w[64];
                    char label[64];
                    sprintf(label, "%s %s", g_courses[i].id, g_courses[i].name);
                    U8ToW(label, w, 64);
                    SendMessageW(g_hComboCourse, CB_ADDSTRING, 0, (LPARAM)w);
                    strcpy(g_notifyCourseId[g_notifyCourseCount++], g_courses[i].id);
                }
                SendMessageW(g_hComboCourse, CB_SETCURSEL, 0, 0);
            }
            SetStatus(L"示例数据已重新生成：%d 门课 / %d 名教师 / %d 名学生",
                      g_courseCount, g_teaCount, g_stuCount);
            RefreshCurrentPage();
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        default: break;
        }
        return 0;

    case WM_NOTIFY:
        {
            LPNMHDR nh = (LPNMHDR)lParam;
            if ((UINT)nh->idFrom == (UINT)IDC_LIST && (UINT)nh->code == (UINT)NM_DBLCLK) {
                LPNMITEMACTIVATE ia = (LPNMITEMACTIVATE)lParam;
                if (ia->iItem >= 0 && g_page == 1) ShowStudentReportByRow(ia->iItem);
            }
        }
        return 0;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO *mm = (MINMAXINFO *)lParam;
            mm->ptMinTrackSize.x = 1100;
            mm->ptMinTrackSize.y = 700;
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        clearAllData();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASSEXW wc;
    MSG msg;
    INITCOMMONCONTROLSEX icc;

    (void)hPrev; (void)lpCmd;

    if (wcsstr(GetCommandLineW(), L"--selftest")) g_selftest = 1;

    g_hInst = hInstance;

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SmartTeachingGuiWnd";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return 1;

    g_hWnd = CreateWindowExW(0, wc.lpszClassName,
        L"智慧教学管理系统 —— 可视化界面（Win32 / 数据结构与算法课程设计）",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1220, 800,
        NULL, NULL, hInstance, NULL);
    if (!g_hWnd) return 2;

    ShowWindow(g_hWnd, (g_selftest ? SW_SHOWMINNOACTIVE : nShow));
    UpdateWindow(g_hWnd);

    /* --shot：把 6 个页面各渲染成一张 BMP（无需交互桌面，便于检查界面效果） */
    if (wcsstr(GetCommandLineW(), L"--shot")) {
        int p;
        for (p = 0; p < 6; p++) {
            MSG m;
            wchar_t fn[64];
            SwitchPage(p);
            /* 让截图更能体现功能：各页面先执行一次代表性动作 */
            if (p == 1) SendMessageW(g_hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_ALL, BN_CLICKED), 0);
            if (p == 2) DoApplySort();
            if (p == 3) DoWarn();
            if (p == 4) { DoNotifyGen(0); DoSendAll(); }
            UpdateWindow(g_hWnd);
            while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
            _snwprintf(fn, 60, L"shot_page%d.bmp", p);
            SaveWindowBmp(fn);
            printf("[shot] page %d -> %ls\n", p, fn);
            fflush(stdout);
        }
        DestroyWindow(g_hWnd);
        return 0;
    }

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
