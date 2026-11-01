/* =============================================================================
 *  console_main.c  —— 智慧教学管理系统【控制台界面层】
 *  仅包含菜单、表格与柱状图等显示代码；数据与算法全部来自 teaching_core.h
 *  编译：gcc console_main.c teaching_core.c -o smart_teaching.exe -std=c11 -O2
 * ========================================================================== */
#include "teaching_core.h"

/* ------------------------- 界面层内部函数声明 ------------------------- */static float readFloatInRange(const char *prompt, float lo, float hi);
static int dispWidth(const char *s);
static int readIntInRange(const char *prompt, int lo, int hi);
static int utf8CharLen(unsigned char c);
static void barChart(const char *title, const char * const *labels,                      const float *vals, int n, const char *unit, int barWidth);
static void gradeOneCourse(void);
static void homeworkGrade(void);
static void inputString(const char *prompt, char *out, int size);
static void inputStringOpt(const char *prompt, char *out, int size);
static void menuAttendance(void);
static void menuCourse(void);
static void menuFile(void);
static void menuHonor(void);
static void menuNotify(void);
static void menuQA(void);
static void menuScore(void);
static void menuSearch(void);
static void menuSort(void);
static void menuStudent(void);
static void menuTeacher(void);
static void menuViz(void);
static void notifyShowQueue(void);
static void notifyShowSent(void);
static void pauseScreen(void);
static void printCells(const char * const *cells, const int *w, int n);
static void printPad(const char *s, int width);
static void printRule(int n, char ch);
static void readLine(char *buf, int size);
static void sanitizeField(char *s);
static void showCourseDetail(const Course *c);
static void showCourseHomework(void);
static void showCourseTable(void);
static void showMainMenu(void);
static void showStudentDetail(Student *s);
static void showStudentTable(Student **arr, int n, int startRank);
static void showTeacherDetail(Teacher *t);
static void showTeacherTable(Teacher **arr, int n);
static void vizAttendance(void);
static void vizCourseAvg(void);
static void vizDashboard(void);
static void vizGpaTop(void);
static void vizHistogram(void);
static void vizMisconduct(void);
static void vizTeacherEval(void);

/* 读取一行（自动去除回车换行），并做长度保护 */
void readLine(char *buf, int size)
{
    if (size <= 0) return;
    if (fgets(buf, size, stdin) == NULL) { buf[0] = '\0'; return; }
    {
        int n = (int)strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) { buf[--n] = '\0'; }
    }
}

/* 读取 [lo,hi] 范围内的整数，输入非法时循环提示 */
int readIntInRange(const char *prompt, int lo, int hi)
{
    char buf[64];
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL) return lo;   /* EOF 保护 */
        {
            int v = atoi(buf);
            if (v >= lo && v <= hi) return v;
        }
        printf("  [输入无效] 请输入 %d ~ %d 之间的整数。\n", lo, hi);
    }
}

/* 读取 [lo,hi] 范围内的浮点数 */
float readFloatInRange(const char *prompt, float lo, float hi)
{
    char buf[64];
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL) return lo;
        {
            float v = (float)atof(buf);
            if (v >= lo && v <= hi) return v;
        }
        printf("  [输入无效] 请输入 %.2f ~ %.2f 之间的数值。\n", lo, hi);
    }
}

/* 读取字符串；allowEmpty=1 时允许直接回车（返回空串） */
void inputString(const char *prompt, char *out, int size)
{
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        readLine(out, size);
        if (out[0] != '\0') { sanitizeField(out); return; }
        printf("  [输入无效] 内容不能为空，请重新输入。\n");
    }
}

/* 允许为空的字符串输入（回车则保留原值，由调用者决定） */
void inputStringOpt(const char *prompt, char *out, int size)
{
    printf("%s", prompt);
    fflush(stdout);
    readLine(out, size);
    if (out[0] != '\0') sanitizeField(out);
}

/* 把字段中的 '|' 替换为 '/'，避免破坏存档格式 */
void sanitizeField(char *s)
{
    int i;
    for (i = 0; s[i]; i++) if (s[i] == '|') s[i] = '/';
}

/* UTF-8 字符字节数 */
int utf8CharLen(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* 终端显示宽度：ASCII 记 1 列，汉字等宽字符记 2 列（UTF-8） */
int dispWidth(const char *s)
{
    int w = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) { w += 1; s += 1; }
        else          { w += 2; s += utf8CharLen(c); }
    }
    return w;
}

/* 按显示宽度左对齐输出；超宽内容自动截断并以 ".." 提示，保证表格不错行 */
void printPad(const char *s, int width)
{
    int w = 0;
    if (dispWidth(s) <= width) {
        printf("%s", s);
        w = dispWidth(s);
    } else {
        const char *p = s;
        int limit = (width >= 3) ? width - 2 : width;
        while (*p) {
            unsigned char c = (unsigned char)*p;
            int cl = utf8CharLen(c);
            int cw = (c < 0x80) ? 1 : 2;
            if (w + cw > limit) break;
            fwrite(p, 1, (size_t)cl, stdout);
            p += cl; w += cw;
        }
        if (width >= 3) { printf(".."); w += 2; }
    }
    while (w < width) { putchar(' '); w++; }
}

void printRule(int n, char ch)
{
    int i;
    for (i = 0; i < n; i++) putchar(ch);
    putchar('\n');
}

/* 输出一行表头/表体（按显示宽度对齐，支持中文），列宽数组 w，共 n 列 */
void printCells(const char * const *cells, const int *w, int n)
{
    int i;
    printf("  ");
    for (i = 0; i < n; i++) { printPad(cells[i], w[i]); printf("  "); }
    printf("\n");
}

void pauseScreen(void)
{
    char buf[16];
    printf("\n按回车键继续...");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) == NULL) { /* EOF 忽略 */ }
}

/* 学生表格：学号 姓名 性别 班级 加权分 绩点 出勤率 不良记录 */
void showStudentTable(Student **arr, int n, int startRank)
{
    int i;
    const int w[8] = { 10, 10, 6, 14, 9, 7, 9, 10 };
    printf("+");
    for (i = 0; i < 8; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n");
    printf("| "); printPad("学号", w[0]);      printf(" | "); printPad("姓名", w[1]);
    printf(" | "); printPad("性别", w[2]);      printf(" | "); printPad("班级", w[3]);
    printf(" | "); printPad("加权平均", w[4]);  printf(" | "); printPad("绩点", w[5]);
    printf(" | "); printPad("出勤率", w[6]);    printf(" | "); printPad("不良记录", w[7]); printf(" |\n");
    printf("+");
    for (i = 0; i < 8; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n");
    for (i = 0; i < n; i++) {
        Student *s = arr[i];
        char b0[32], b4[32], b5[32], b6[32], b7[32];
        if (startRank > 0) sprintf(b0, "%d.%s", startRank + i, s->id);
        else               setStr(b0, s->id, sizeof(b0));
        sprintf(b4, "%.2f", s->weighted);
        sprintf(b5, "%.2f", s->gpa);
        sprintf(b6, "%.1f%%", s->attRate);
        if (s->misCount > 0) sprintf(b7, "%d 条(%d分)", s->misCount, s->misScore);
        else                 setStr(b7, "无", sizeof(b7));
        printf("| "); printPad(b0, w[0]); printf(" | "); printPad(s->name, w[1]);
        printf(" | "); printPad(s->gender, w[2]); printf(" | "); printPad(s->className, w[3]);
        printf(" | "); printPad(b4, w[4]); printf(" | "); printPad(b5, w[5]);
        printf(" | "); printPad(b6, w[6]); printf(" | "); printPad(b7, w[7]); printf(" |\n");
    }
    printf("+");
    for (i = 0; i < 8; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n共 %d 条记录。\n", n);
}

/* 学生个人详细档案 */
void showStudentDetail(Student *s)
{
    int i;
    if (!s) { printf("  [提示] 学生不存在。\n"); return; }
    printf("\n================ 学生档案 ================\n");
    printf("  学号: %-10s  姓名: %s\n", s->id, s->name);
    printf("  性别: %-6s   班级: %s\n", s->gender, s->className);
    printf("  手机: %-14s 邮箱: %s\n", s->phone, s->email);
    printf("  加权平均分: %.2f    平均学分绩点: %.2f    出勤率: %.1f%%\n",
           s->weighted, s->gpa, s->attRate);

    printf("\n  ---- 各科成绩 ----\n");
    printf("  +----------+------------------+----------+----------+----------+--------+\n");
    printf("  | 课程号   | 课程名称         | 平时(40%%)| 考试(60%%)| 总评     | 绩点   |\n");
    printf("  +----------+------------------+----------+----------+----------+--------+\n");
    for (i = 0; i < s->gradeCount; i++) {
        Course *c = findCourse(s->grades[i].courseId);
        char b1[32], b2[32], b3[32], b4[32], b5[32];
        setStr(b1, s->grades[i].courseId, sizeof(b1));
        setStr(b2, c ? c->name : "未知课程", sizeof(b2));
        sprintf(b3, "%.1f", s->grades[i].usual);
        sprintf(b4, "%.1f", s->grades[i].exam);
        sprintf(b5, "%.1f", gradeTotal(&s->grades[i]));
        printf("  | "); printPad(b1, 8);  printf(" | "); printPad(b2, 16);
        printf(" | "); printPad(b3, 8);  printf(" | "); printPad(b4, 8);
        printf(" | "); printPad(b5, 8);  printf(" | ");
        { char b6[32]; sprintf(b6, "%.2f", scoreToPoint(gradeTotal(&s->grades[i])));
          printPad(b6, 6); printf(" |\n"); }
    }
    if (s->gradeCount == 0) printf("  | (暂无成绩记录)                                                        |\n");
    printf("  +----------+------------------+----------+----------+----------+--------+\n");

    printf("\n  ---- 考勤记录(最近 12 条) ----\n");
    if (s->attCount == 0) printf("    (暂无考勤记录)\n");
    else {
        int start = (s->attCount > 12) ? s->attCount - 12 : 0;
        printf("    ");
        for (i = start; i < s->attCount; i++)
            printf("%s/%s:%s  ", s->atts[i].courseId, s->atts[i].date, attStatusName(s->atts[i].status));
        printf("\n");
    }

    printf("\n  ---- 不良记录 ----\n");
    if (s->misCount == 0) printf("    无不良记录，表现良好。\n");
    else {
        MisRec *arr[MAX_MIS];
        for (i = 0; i < s->misCount; i++) arr[i] = &s->mis[i];
        sortPtrArray((void **)arr, s->misCount, cmpMisBySeverityDesc);   /* 按严重程度排序 */
        for (i = 0; i < s->misCount; i++)
            printf("    %s  %s(%d分/%s)  %s\n", arr[i]->date, arr[i]->type,
                   arr[i]->severity, misLevelName(arr[i]->severity), arr[i]->desc);
    }

    printf("\n  ---- 获奖 / 竞赛 / 学术交流 ----\n");
    if (s->honorCount == 0) printf("    暂无记录。\n");
    else {
        HonorRec *arr[MAX_HONOR];
        for (i = 0; i < s->honorCount; i++) arr[i] = &s->honors[i];
        sortPtrArray((void **)arr, s->honorCount, cmpHonorByLevelDesc);
        for (i = 0; i < s->honorCount; i++)
            printf("    %s  [%s] %s (%s)\n", arr[i]->date, arr[i]->type,
                   arr[i]->name, honorLevelName(arr[i]->level));
    }

    printf("\n  ---- 作业批改记录 ----\n");
    {
        int cnt = 0;
        for (i = 0; i < g_hwCount; i++) {
            if (strcmp(g_hw[i].studentId, s->id) == 0) {
                cnt++;
                printf("    %s  %s  %s  %.1f 分  (%s)\n", g_hw[i].date, g_hw[i].courseId,
                       g_hw[i].title, g_hw[i].score, g_hw[i].comment);
            }
        }
        if (cnt == 0) printf("    暂无作业记录。\n");
    }
    printf("==========================================\n");
}

void showTeacherTable(Teacher **arr, int n)
{
    int i;
    const int w[7] = { 8, 10, 10, 14, 10, 12, 10 };
    printf("+");
    for (i = 0; i < 7; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n");
    printf("| "); printPad("工号", w[0]); printf(" | "); printPad("姓名", w[1]);
    printf(" | "); printPad("职称", w[2]); printf(" | "); printPad("所属学院", w[3]);
    printf(" | "); printPad("任课数", w[4]); printf(" | "); printPad("平均评教", w[5]);
    printf(" | "); printPad("课程均分", w[6]); printf(" |\n");
    printf("+");
    for (i = 0; i < 7; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n");
    for (i = 0; i < n; i++) {
        Teacher *t = arr[i];
        char b4[32], b5[32], b6[32];
        sprintf(b4, "%d", t->itemCount);
        sprintf(b5, "%.2f", t->evalAvg);
        sprintf(b6, "%.2f", teacherAvgCourseScore(t));
        printf("| "); printPad(t->id, w[0]); printf(" | "); printPad(t->name, w[1]);
        printf(" | "); printPad(t->title, w[2]); printf(" | "); printPad(t->dept, w[3]);
        printf(" | "); printPad(b4, w[4]); printf(" | "); printPad(b5, w[5]);
        printf(" | "); printPad(b6, w[6]); printf(" |\n");
    }
    printf("+");
    for (i = 0; i < 7; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n共 %d 名教师。\n", n);
}

void showTeacherDetail(Teacher *t)
{
    int i, cnt = 0;
    if (!t) { printf("  [提示] 教师不存在。\n"); return; }
    printf("\n============== 教师档案 ==============\n");
    printf("  工号: %-8s 姓名: %s   职称: %s\n", t->id, t->name, t->title);
    printf("  学院: %-14s 手机: %-13s 邮箱: %s\n", t->dept, t->phone, t->email);
    printf("  平均评教分: %.2f    所授课程学生平均分: %.2f\n", t->evalAvg, teacherAvgCourseScore(t));
    printf("\n  ---- 任课情况 ----\n");
    printf("  +----------+------------------+----------+----------------+----------+\n");
    printf("  | 课程号   | 课程名称         | 学分     | 选课人数       | 评教分   |\n");
    printf("  +----------+------------------+----------+----------------+----------+\n");
    for (i = 0; i < t->itemCount; i++) {
        Course *c = findCourse(t->items[i].courseId);
        char b1[32], b2[32], b3[32], b4[32], b5[32];
        setStr(b1, t->items[i].courseId, sizeof(b1));
        setStr(b2, c ? c->name : "未知课程", sizeof(b2));
        if (c) sprintf(b3, "%.1f", c->credit); else setStr(b3, "-", sizeof(b3));
        sprintf(b4, "%d 人", courseStudentCount(t->items[i].courseId));
        sprintf(b5, "%.2f", t->items[i].evalScore);
        printf("  | "); printPad(b1, 8); printf(" | "); printPad(b2, 16);
        printf(" | "); printPad(b3, 8); printf(" | "); printPad(b4, 14);
        printf(" | "); printPad(b5, 8); printf(" |\n");
    }
    if (t->itemCount == 0) printf("  | (暂无任课记录)                                                          |\n");
    printf("  +----------+------------------+----------+----------------+----------+\n");

    printf("\n  ---- 答疑辅导记录 ----\n");
    for (i = 0; i < g_qaCount; i++) {
        if (strcmp(g_qa[i].teacherId, t->id) == 0) {
            cnt++;
            printf("    %s  %s -> 问: %s\n", g_qa[i].date, g_qa[i].studentId, g_qa[i].question);
        }
    }
    if (cnt == 0) printf("    暂无答疑记录。\n");
    else printf("    累计答疑 %d 次。\n", cnt);
    printf("=====================================\n");
}

void showCourseTable(void)
{
    int i;
    const int w[6] = { 8, 20, 8, 10, 12, 12 };
    printf("+");
    for (i = 0; i < 6; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n");
    printf("| "); printPad("课程号", w[0]); printf(" | "); printPad("课程名称", w[1]);
    printf(" | "); printPad("学分", w[2]); printf(" | "); printPad("学时", w[3]);
    printf(" | "); printPad("任课教师数", w[4]); printf(" | "); printPad("选课人数", w[5]); printf(" |\n");
    printf("+");
    for (i = 0; i < 6; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n");
    for (i = 0; i < g_courseCount; i++) {
        Teacher *p = g_teaHead; int tc = 0;
        char b2[32], b3[32], b4[32], b5[32];
        while (p) { int k; for (k = 0; k < p->itemCount; k++)
                        if (strcmp(p->items[k].courseId, g_courses[i].id) == 0) tc++;
                    p = p->next; }
        sprintf(b2, "%.1f", g_courses[i].credit);
        sprintf(b3, "%d", g_courses[i].hours);
        sprintf(b4, "%d 人", tc);
        sprintf(b5, "%d 人", courseStudentCount(g_courses[i].id));
        printf("| "); printPad(g_courses[i].id, w[0]); printf(" | "); printPad(g_courses[i].name, w[1]);
        printf(" | "); printPad(b2, w[2]); printf(" | "); printPad(b3, w[3]);
        printf(" | "); printPad(b4, w[4]); printf(" | "); printPad(b5, w[5]); printf(" |\n");
    }
    printf("+");
    for (i = 0; i < 6; i++) { int j; for (j = 0; j < w[i] + 2; j++) putchar('-'); putchar('+'); }
    printf("\n共 %d 门课程。\n", g_courseCount);
}

void showCourseDetail(const Course *c)
{
    int i;
    Teacher *p;
    if (!c) { printf("  [提示] 课程不存在。\n"); return; }
    printf("\n============== 课程信息 ==============\n");
    printf("  课程号: %s   课程名称: %s\n", c->id, c->name);
    printf("  学分: %.1f     学时: %d     开课学期: %s\n", c->credit, c->hours, c->term);
    printf("  选课人数: %d 人     平均总评: %.2f 分\n", courseStudentCount(c->id), courseAvgScore(c->id));
    printf("\n  ---- 任课教师 ----\n");
    p = g_teaHead;
    while (p) {
        for (i = 0; i < p->itemCount; i++) {
            if (strcmp(p->items[i].courseId, c->id) == 0)
                printf("    %s  %s  %s  评教 %.2f\n", p->id, p->name, p->title, p->items[i].evalScore);
        }
        p = p->next;
    }
    printf("\n  ---- 成绩分布 ----\n");
    {
        const char *labels[5] = { "0-59", "60-69", "70-79", "80-89", "90-100" };
        float vals[5] = { 0, 0, 0, 0, 0 };
        Student *s = g_stuHead;
        while (s) {
            Grade *g = findGrade(s, c->id);
            if (g) {
                float t = gradeTotal(g);
                if (t < 60) vals[0] += 1;
                else if (t < 70) vals[1] += 1;
                else if (t < 80) vals[2] += 1;
                else if (t < 90) vals[3] += 1;
                else vals[4] += 1;
            }
            s = s->next;
        }
        barChart("(人数/柱状图)", labels, vals, 5, "人", 30);
    }
    printf("======================================\n");
}

/* 通用 ASCII 柱状图：labels 中文自动按 2 列宽对齐 */
void barChart(const char *title, const char * const *labels,
                     const float *vals, int n, const char *unit, int barWidth)
{
    int i, j;
    float maxV = 0.0f;
    for (i = 0; i < n; i++) if (vals[i] > maxV) maxV = vals[i];
    if (maxV <= 0.0f) maxV = 1.0f;
    printf("\n  %s\n", title);
    printf("  ");
    printRule(60, '-');
    for (i = 0; i < n; i++) {
        int len = (int)(vals[i] / maxV * (float)barWidth + 0.5f);
        float ratio = vals[i] / maxV * 100.0f;
        printf("  ");
        printPad(labels[i], 16);
        printf(" |");
        for (j = 0; j < len; j++) putchar('#');
        for (j = len; j < barWidth; j++) putchar(' ');
        printf("| %8.2f %s (%5.1f%%)\n", vals[i], unit, ratio);
    }
    printf("  ");
    printRule(60, '-');
}

void notifyShowQueue(void)
{
    Notify *p = g_nqHead;
    int i = 1;
    printf("\n---- 待发送通知队列(先进先出) 共 %d 条 ----\n", g_nqCount);
    if (!p) { printf("  (队列为空)\n"); return; }
    while (p) {
        printf("  %2d. #%d %s -> %s(%s) [%s] %s\n", i++, p->id, p->channel,
               p->recvName, p->recvId, p->title, p->content);
        p = p->next;
    }
}

void notifyShowSent(void)
{
    Notify *p = g_sentTop;
    int i = 1;
    printf("\n---- 已发送记录栈(后进先出，最近的在最上) 共 %d 条 ----\n", g_sentCount);
    if (!p) { printf("  (暂无发送记录)\n"); return; }
    while (p) {
        printf("  %2d. #%d [%s] %s -> %s(%s)  %s\n", i++, p->id, p->timeStr,
               p->channel, p->recvName, p->recvId, p->title);
        p = p->next;
    }
}

void menuStudent(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(56, '=');
        printf("            学生信息管理\n");
        printRule(56, '=');
        printf("   1. 浏览全部学生          2. 新增学生\n");
        printf("   3. 查询学生详情(按学号)  4. 修改学生信息\n");
        printf("   5. 删除学生              6. 录入/修改该生成绩\n");
        printf("   0. 返回上级菜单\n");
        printRule(56, '-');
        ch = readIntInRange("请选择操作: ", 0, 6);
        if (ch == 0) return;

        if (ch == 1) {
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            printf("\n---- 全部学生（按学号升序，快速排序）----\n");
            sortPtrArray((void **)arr, n, cmpStuByIdAsc);
            showStudentTable(arr, n, 0);
            pauseScreen();
        } else if (ch == 2) {
            char id[16], name[32], gender[8], cls[24], phone[20], email[48];
            printf("\n---- 新增学生 ----\n");
            inputString("请输入学号(如 Z2023076): ", id, sizeof(id));
            if (findStudent(id)) { printf("  [错误] 该学号已存在。\n"); pauseScreen(); continue; }
            inputString("请输入姓名: ", name, sizeof(name));
            inputString("请输入性别(男/女): ", gender, sizeof(gender));
            inputString("请输入班级: ", cls, sizeof(cls));
            inputString("请输入手机号: ", phone, sizeof(phone));
            inputString("请输入邮箱: ", email, sizeof(email));
            {
                Student *s = createStudent(id, name, gender, cls, phone, email);
                if (s) {
                    int i;
                    printf("  [提示] 已自动为 %s 选上全部 %d 门课程，请录入初始成绩(可直接回车跳过=0分)。\n",
                           name, g_courseCount);
                    for (i = 0; i < g_courseCount; i++) {
                        char prompt[96], buf[32];
                        float usual = 0.0f, exam = 0.0f;
                        printf("    [%s %s]\n", g_courses[i].id, g_courses[i].name);
                        sprintf(prompt, "      平时成绩(0-100，回车跳过): ");
                        inputStringOpt(prompt, buf, sizeof(buf));
                        if (buf[0]) usual = (float)atof(buf);
                        sprintf(prompt, "      考试成绩(0-100，回车跳过): ");
                        inputStringOpt(prompt, buf, sizeof(buf));
                        if (buf[0]) exam = (float)atof(buf);
                        addGrade(s, g_courses[i].id, usual, exam);
                    }
                    recalcStudent(s);
                    printf("  [完成] 学生 %s 添加成功。\n", name);
                }
            }
            pauseScreen();
        } else if (ch == 3) {
            char id[16];
            inputString("\n请输入学号: ", id, sizeof(id));
            showStudentDetail(findStudent(id));
            pauseScreen();
        } else if (ch == 4) {
            char id[16], buf[64];
            Student *s;
            inputString("\n请输入学号: ", id, sizeof(id));
            s = findStudent(id);
            if (!s) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
            showStudentDetail(s);
            printf("\n(直接回车表示不修改该项)\n");
            inputStringOpt("新姓名: ", buf, sizeof(buf));    if (buf[0]) setStr(s->name, buf, sizeof(s->name));
            inputStringOpt("新性别: ", buf, sizeof(buf));    if (buf[0]) setStr(s->gender, buf, sizeof(s->gender));
            inputStringOpt("新班级: ", buf, sizeof(buf));    if (buf[0]) setStr(s->className, buf, sizeof(s->className));
            inputStringOpt("新手机: ", buf, sizeof(buf));    if (buf[0]) setStr(s->phone, buf, sizeof(s->phone));
            inputStringOpt("新邮箱: ", buf, sizeof(buf));    if (buf[0]) setStr(s->email, buf, sizeof(s->email));
            printf("  [完成] 修改成功。\n");
            pauseScreen();
        } else if (ch == 5) {
            char id[16];
            Student *s, *prev = NULL;
            inputString("\n请输入要删除的学号: ", id, sizeof(id));
            s = g_stuHead;
            while (s && strcmp(s->id, id) != 0) { prev = s; s = s->next; }
            if (!s) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
            printf("  确认删除 %s(%s) 吗？该生成绩、考勤、奖惩记录将一并删除。\n", s->name, s->id);
            if (readIntInRange("\n  1=确认删除  0=取消 : ", 0, 1) == 1) {
                int i, k = 0;
                if (prev) prev->next = s->next; else g_stuHead = s->next;
                g_stuCount--;
                /* 同步清理该生的作业记录（顺序表删除，O(n)） */
                for (i = 0; i < g_hwCount; i++) {
                    if (strcmp(g_hw[i].studentId, id) != 0) g_hw[k++] = g_hw[i];
                }
                g_hwCount = k;
                k = 0;
                for (i = 0; i < g_qaCount; i++)
                    if (strcmp(g_qa[i].studentId, id) != 0) g_qa[k++] = g_qa[i];
                g_qaCount = k;
                free(s);
                printf("  [完成] 已删除。\n");
            } else printf("  已取消。\n");
            pauseScreen();
        } else if (ch == 6) {
            char id[16], cid[16];
            Student *s; Course *c; Grade *g;
            inputString("\n请输入学号: ", id, sizeof(id));
            s = findStudent(id);
            if (!s) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
            showCourseTable();
            inputString("请输入课程号: ", cid, sizeof(cid));
            c = findCourse(cid);
            if (!c) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            g = findGrade(s, cid);
            if (!g) { addGrade(s, cid, 0.0f, 0.0f); g = findGrade(s, cid); }
            printf("  当前: 平时 %.1f, 考试 %.1f\n", g->usual, g->exam);
            g->usual = readFloatInRange("  新平时成绩(0-100): ", 0.0f, 100.0f);
            g->exam  = readFloatInRange("  新考试成绩(0-100): ", 0.0f, 100.0f);
            recalcStudent(s);
            printf("  [完成] 总评 %.2f 分，绩点 %.2f\n", gradeTotal(g), scoreToPoint(gradeTotal(g)));
            pauseScreen();
        }
    }
}

void menuTeacher(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(56, '=');
        printf("            教师信息管理\n");
        printRule(56, '=');
        printf("   1. 浏览全部教师          2. 新增教师\n");
        printf("   3. 查询教师详情(按工号)  4. 修改教师信息\n");
        printf("   5. 删除教师              6. 分配课程/设置评教分\n");
        printf("   0. 返回上级菜单\n");
        printRule(56, '-');
        ch = readIntInRange("请选择操作: ", 0, 6);
        if (ch == 0) return;

        if (ch == 1) {
            Teacher *arr[MAX_TEACHERS];
            int n = collectTeachers(arr, MAX_TEACHERS);
            printf("\n---- 全部教师（按工号升序）----\n");
            sortPtrArray((void **)arr, n, cmpTeaByIdAsc);
            showTeacherTable(arr, n);
            pauseScreen();
        } else if (ch == 2) {
            char id[16], name[32], title[16], dept[32], phone[20], email[48];
            printf("\n---- 新增教师 ----\n");
            inputString("请输入工号(如 T013): ", id, sizeof(id));
            if (findTeacher(id)) { printf("  [错误] 该工号已存在。\n"); pauseScreen(); continue; }
            inputString("请输入姓名: ", name, sizeof(name));
            inputString("请输入职称(教授/副教授/讲师/助教): ", title, sizeof(title));
            inputString("请输入所属学院: ", dept, sizeof(dept));
            inputString("请输入手机号: ", phone, sizeof(phone));
            inputString("请输入邮箱: ", email, sizeof(email));
            if (createTeacher(id, name, title, dept, phone, email))
                printf("  [完成] 教师 %s 添加成功，请用“分配课程”为其安排任课。\n", name);
            pauseScreen();
        } else if (ch == 3) {
            char id[16];
            inputString("\n请输入工号: ", id, sizeof(id));
            showTeacherDetail(findTeacher(id));
            pauseScreen();
        } else if (ch == 4) {
            char id[16], buf[64];
            Teacher *t;
            inputString("\n请输入工号: ", id, sizeof(id));
            t = findTeacher(id);
            if (!t) { printf("  [错误] 未找到该教师。\n"); pauseScreen(); continue; }
            showTeacherDetail(t);
            printf("\n(直接回车表示不修改该项)\n");
            inputStringOpt("新姓名: ", buf, sizeof(buf)); if (buf[0]) setStr(t->name, buf, sizeof(t->name));
            inputStringOpt("新职称: ", buf, sizeof(buf)); if (buf[0]) setStr(t->title, buf, sizeof(t->title));
            inputStringOpt("新学院: ", buf, sizeof(buf)); if (buf[0]) setStr(t->dept, buf, sizeof(t->dept));
            inputStringOpt("新手机: ", buf, sizeof(buf)); if (buf[0]) setStr(t->phone, buf, sizeof(t->phone));
            inputStringOpt("新邮箱: ", buf, sizeof(buf)); if (buf[0]) setStr(t->email, buf, sizeof(t->email));
            printf("  [完成] 修改成功。\n");
            pauseScreen();
        } else if (ch == 5) {
            char id[16];
            Teacher *t, *prev = NULL;
            inputString("\n请输入要删除的工号: ", id, sizeof(id));
            t = g_teaHead;
            while (t && strcmp(t->id, id) != 0) { prev = t; t = t->next; }
            if (!t) { printf("  [错误] 未找到该教师。\n"); pauseScreen(); continue; }
            if (readIntInRange("\n  1=确认删除  0=取消 : ", 0, 1) == 1) {
                if (prev) prev->next = t->next; else g_teaHead = t->next;
                g_teaCount--;
                free(t);
                printf("  [完成] 已删除。\n");
            } else printf("  已取消。\n");
            pauseScreen();
        } else if (ch == 6) {
            char id[16], cid[16];
            Teacher *t;
            inputString("\n请输入工号: ", id, sizeof(id));
            t = findTeacher(id);
            if (!t) { printf("  [错误] 未找到该教师。\n"); pauseScreen(); continue; }
            showCourseTable();
            inputString("请输入课程号: ", cid, sizeof(cid));
            if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            {
                float ev = readFloatInRange("请输入该课程评教分(0-5): ", 0.0f, 5.0f);
                addTeachItem(t, cid, ev);
                printf("  [完成] 已为该教师分配课程 %s，评教分 %.2f。\n", cid, ev);
            }
            pauseScreen();
        }
    }
}

void menuCourse(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(56, '=');
        printf("            课程信息管理\n");
        printRule(56, '=');
        printf("   1. 浏览全部课程(含任课/选课统计)  2. 新增课程\n");
        printf("   3. 课程详情(教师与成绩分布)       4. 修改课程信息\n");
        printf("   5. 删除课程                       6. 批量给课程加学生\n");
        printf("   0. 返回上级菜单\n");
        printRule(56, '-');
        ch = readIntInRange("请选择操作: ", 0, 6);
        if (ch == 0) return;

        if (ch == 1) { printf("\n---- 全部课程 ----\n"); showCourseTable(); pauseScreen(); }
        else if (ch == 2) {
            char id[16], name[32], term[24];
            float credit; int hours;
            printf("\n---- 新增课程 ----\n");
            inputString("请输入课程号(如 C005): ", id, sizeof(id));
            if (findCourse(id)) { printf("  [错误] 该课程号已存在。\n"); pauseScreen(); continue; }
            inputString("请输入课程名称: ", name, sizeof(name));
            credit = readFloatInRange("请输入学分(0.5-10): ", 0.5f, 10.0f);
            hours  = readIntInRange("请输入学时(8-128): ", 8, 128);
            inputString("请输入开课学期: ", term, sizeof(term));
            if (createCourse(id, name, credit, hours, term))
                printf("  [完成] 课程添加成功。可用“批量给课程加学生”完成选课。\n");
            pauseScreen();
        } else if (ch == 3) {
            char id[16];
            inputString("\n请输入课程号: ", id, sizeof(id));
            showCourseDetail(findCourse(id));
            pauseScreen();
        } else if (ch == 4) {
            char id[16], buf[64];
            Course *c;
            inputString("\n请输入课程号: ", id, sizeof(id));
            c = findCourse(id);
            if (!c) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            printf("\n(直接回车表示不修改该项)\n");
            inputStringOpt("新课程名称: ", buf, sizeof(buf)); if (buf[0]) setStr(c->name, buf, sizeof(c->name));
            inputStringOpt("新学期: ", buf, sizeof(buf));     if (buf[0]) setStr(c->term, buf, sizeof(c->term));
            inputStringOpt("新学分: ", buf, sizeof(buf));     if (buf[0]) c->credit = (float)atof(buf);
            inputStringOpt("新学时: ", buf, sizeof(buf));     if (buf[0]) c->hours = atoi(buf);
            printf("  [完成] 修改成功。\n");
            pauseScreen();
        } else if (ch == 5) {
            char id[16];
            int i, k;
            inputString("\n请输入要删除的课程号: ", id, sizeof(id));
            for (i = 0; i < g_courseCount; i++) if (strcmp(g_courses[i].id, id) == 0) break;
            if (i >= g_courseCount) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            if (readIntInRange("\n  1=确认删除  0=取消 : ", 0, 1) == 1) {
                Student *s = g_stuHead;
                Teacher *t = g_teaHead;
                /* 顺序表删除：后续元素前移 O(n) */
                for (k = i; k < g_courseCount - 1; k++) g_courses[k] = g_courses[k + 1];
                g_courseCount--;
                /* 级联删除学生的该课成绩、教师的任课信息、作业记录 */
                while (s) {
                    int j, m = 0;
                    for (j = 0; j < s->gradeCount; j++)
                        if (strcmp(s->grades[j].courseId, id) != 0) s->grades[m++] = s->grades[j];
                    s->gradeCount = m;
                    recalcStudent(s);
                    s = s->next;
                }
                while (t) { removeTeacherCourse(t, id); t = t->next; }
                k = 0;
                for (i = 0; i < g_hwCount; i++)
                    if (strcmp(g_hw[i].courseId, id) != 0) g_hw[k++] = g_hw[i];
                g_hwCount = k;
                printf("  [完成] 课程及其关联数据已删除。\n");
            } else printf("  已取消。\n");
            pauseScreen();
        } else if (ch == 6) {
            char cid[16];
            Course *c;
            int mode;
            inputString("\n请输入课程号: ", cid, sizeof(cid));
            c = findCourse(cid);
            if (!c) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            mode = readIntInRange("\n  1=全部学生选课  2=按学号逐个添加  0=取消 : ", 0, 2);
            if (mode == 1) {
                Student *s = g_stuHead; int cnt = 0;
                while (s) {
                    if (!findGrade(s, cid)) { addGrade(s, cid, 0.0f, 0.0f); cnt++; }
                    s = s->next;
                }
                recalcAll();
                printf("  [完成] 新增选课 %d 人，当前选课人数 %d 人。\n", cnt, courseStudentCount(cid));
            } else if (mode == 2) {
                while (1) {
                    char sid[16]; Student *s;
                    inputStringOpt("  学号(直接回车结束): ", sid, sizeof(sid));
                    if (!sid[0]) break;
                    s = findStudent(sid);
                    if (!s) { printf("    未找到该学生。\n"); continue; }
                    if (findGrade(s, cid)) { printf("    该生已选此课。\n"); continue; }
                    addGrade(s, cid, 0.0f, 0.0f);
                    printf("    已添加 %s。\n", s->name);
                }
                recalcAll();
            }
            pauseScreen();
        }
    }
}

void gradeOneCourse(void)
{
    char cid[16];
    Course *c;
    Student *arr[MAX_STUDENTS];
    int n, i;
    inputString("\n请输入课程号: ", cid, sizeof(cid));
    c = findCourse(cid);
    if (!c) { printf("  [错误] 未找到该课程。\n"); return; }
    n = collectStudents(arr, MAX_STUDENTS);
    sortPtrArray((void **)arr, n, cmpStuByIdAsc);
    printf("  课程: %s  共 %d 名学生\n", c->name, n);
    printf("  提示: 直接回车跳过该学生；输入 'q' 结束录入。\n");
    for (i = 0; i < n; i++) {
        char buf[32];
        Grade *g = findGrade(arr[i], cid);
        printf("  [%d/%d] %s %s  当前 平时%.1f 考试%.1f\n", i + 1, n, arr[i]->id, arr[i]->name,
               g ? g->usual : 0.0f, g ? g->exam : 0.0f);
        inputStringOpt("     新平时成绩(回车跳过, q结束): ", buf, sizeof(buf));
        if (buf[0] == 'q' || buf[0] == 'Q') break;
        if (buf[0]) {
            float u = (float)atof(buf);
            if (u < 0.0f || u > 100.0f) { printf("    数值超范围，已忽略。\n"); }
            else {
                if (!g) { addGrade(arr[i], cid, u, 0.0f); g = findGrade(arr[i], cid); }
                else g->usual = u;
            }
        }
        inputStringOpt("     新考试成绩(回车跳过): ", buf, sizeof(buf));
        if (buf[0]) {
            float e = (float)atof(buf);
            if (e < 0.0f || e > 100.0f) { printf("    数值超范围，已忽略。\n"); }
            else {
                if (!g) { addGrade(arr[i], cid, 0.0f, e); g = findGrade(arr[i], cid); }
                else g->exam = e;
            }
        }
        recalcStudent(arr[i]);
    }
    printf("  [完成] 成绩录入结束，已自动重算加权平均分与绩点。\n");
}

void homeworkGrade(void)
{
    char cid[16], title[48], tcode[16], date[12];
    Course *c;
    Teacher *t;
    Student *arr[MAX_STUDENTS];
    int n, i;
    printf("\n---- 作业批改 ----\n");
    showCourseTable();
    inputString("请输入课程号: ", cid, sizeof(cid));
    c = findCourse(cid);
    if (!c) { printf("  [错误] 未找到该课程。\n"); return; }
    inputString("请输入作业标题: ", title, sizeof(title));
    inputString("请输入批改教师工号: ", tcode, sizeof(tcode));
    t = findTeacher(tcode);
    if (!t) { printf("  [提示] 未找到该教师，将记为未知教师(仍需继续)。\n"); }
    inputString("请输入批改日期(如 2024-05-10): ", date, sizeof(date));

    n = collectStudents(arr, MAX_STUDENTS);
    printf("  共 %d 名学生，输入 0-100 的分数；直接回车跳过，输入 -1 结束。\n", n);
    for (i = 0; i < n; i++) {
        char prompt[96], buf[32];
        sprintf(prompt, "  [%d/%d] %s %s 分数(-1结束): ", i + 1, n, arr[i]->id, arr[i]->name);
        inputStringOpt(prompt, buf, sizeof(buf));
        if (!buf[0]) continue;
        {
            float sc = (float)atof(buf);
            if (sc < 0.0f) break;
            if (sc > 100.0f) { printf("    分数超范围，已忽略。\n"); continue; }
            addHomework(cid, arr[i]->id, tcode[0] ? tcode : "T000", title, sc,
                        (sc >= 90) ? "优秀" : (sc >= 75) ? "良好" : (sc >= 60) ? "合格" : "需订正", date);
            refreshUsualFromHomework(arr[i]->id, cid);   /* 平时成绩自动折算 */
        }
    }
    printf("  [完成] 作业成绩已保存，相关学生平时成绩已按作业平均分自动更新。\n");
}

void showCourseHomework(void)
{
    char cid[16];
    int i, cnt = 0;
    float sum = 0.0f, maxS = -1.0f, minS = 101.0f;
    inputString("\n请输入课程号: ", cid, sizeof(cid));
    if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); return; }
    printf("\n---- 课程 %s 作业批改情况 ----\n", cid);
    {
        const char *hdr[6] = { "序号", "日期", "学号", "作业标题", "得分", "姓名" };
        const int   w[6]   = { 6, 12, 10, 22, 8, 10 };
        printCells(hdr, w, 6);
        printRule(80, '-');
        for (i = 0; i < g_hwCount; i++) {
            if (strcmp(g_hw[i].courseId, cid) == 0) {
                Student *s = findStudent(g_hw[i].studentId);
                char b0[16], b4[16];
                const char *row[6];
                sprintf(b0, "%d", g_hw[i].id);
                sprintf(b4, "%.1f", g_hw[i].score);
                row[0] = b0; row[1] = g_hw[i].date; row[2] = g_hw[i].studentId;
                row[3] = g_hw[i].title; row[4] = b4; row[5] = s ? s->name : "?";
                printCells(row, w, 6);
                sum += g_hw[i].score; cnt++;
                if (g_hw[i].score > maxS) maxS = g_hw[i].score;
                if (g_hw[i].score < minS) minS = g_hw[i].score;
            }
        }
    }
    if (cnt == 0) printf("  (该课程暂无作业记录)\n");
    else printf("\n  合计 %d 条，平均 %.2f 分，最高 %.1f 分，最低 %.1f 分。\n",
                cnt, sum / (float)cnt, maxS, minS);
}

void menuScore(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(56, '=');
        printf("            成绩与作业管理\n");
        printRule(56, '=');
        printf("   1. 录入/修改单个学生成绩   2. 按课程批量录入成绩\n");
        printf("   3. 作业批改(自动折算平时分) 4. 查看课程作业情况\n");
        printf("   5. 查看学生成绩单          6. 重算全部统计指标\n");
        printf("   7. 成绩排行榜(Top 10)      0. 返回上级菜单\n");
        printRule(56, '-');
        ch = readIntInRange("请选择操作: ", 0, 7);
        if (ch == 0) return;

        if (ch == 1) {
            char sid[16], cid[16];
            Student *s; Grade *g;
            inputString("\n请输入学号: ", sid, sizeof(sid));
            s = findStudent(sid);
            if (!s) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
            showCourseTable();
            inputString("请输入课程号: ", cid, sizeof(cid));
            if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            g = findGrade(s, cid);
            if (!g) { addGrade(s, cid, 0.0f, 0.0f); g = findGrade(s, cid); }
            g->usual = readFloatInRange("  平时成绩(0-100): ", 0.0f, 100.0f);
            g->exam  = readFloatInRange("  考试成绩(0-100): ", 0.0f, 100.0f);
            recalcStudent(s);
            printf("  [完成] 总评 %.2f 分（绩点 %.2f）\n", gradeTotal(g), scoreToPoint(gradeTotal(g)));
            pauseScreen();
        } else if (ch == 2) { gradeOneCourse(); pauseScreen(); }
        else if (ch == 3) { homeworkGrade(); pauseScreen(); }
        else if (ch == 4) { showCourseHomework(); pauseScreen(); }
        else if (ch == 5) {
            char sid[16];
            inputString("\n请输入学号: ", sid, sizeof(sid));
            showStudentDetail(findStudent(sid));
            pauseScreen();
        } else if (ch == 6) {
            recalcAll();
            printf("\n  [完成] 已按最新数据重算全体学生的加权平均分、绩点、出勤率、不良记录分值。\n");
            pauseScreen();
        } else if (ch == 7) {
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            int top = n < 10 ? n : 10;
            sortPtrArray((void **)arr, n, cmpStuByWeightedDesc);
            printf("\n---- 加权平均分排行榜 Top %d（快速排序 + 名次）----\n", top);
            showStudentTable(arr, top, 1);
            pauseScreen();
        }
    }
}

void menuAttendance(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(56, '=');
        printf("            考勤管理\n");
        printRule(56, '=');
        printf("   1. 课堂签到(整班)      2. 补录/查询学生考勤\n");
        printf("   3. 课程考勤统计        4. 出勤率排行(缺勤预警榜)\n");
        printf("   5. 一键生成缺勤预警通知(入队)  0. 返回上级菜单\n");
        printRule(56, '-');
        ch = readIntInRange("请选择操作: ", 0, 5);
        if (ch == 0) return;

        if (ch == 1) {
            char cid[16], date[12];
            Course *c;
            Student *arr[MAX_STUDENTS];
            int n, i, mode;
            inputString("\n请输入课程号: ", cid, sizeof(cid));
            c = findCourse(cid);
            if (!c) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            inputString("请输入上课日期(如 2024-05-13): ", date, sizeof(date));
            n = collectStudents(arr, MAX_STUDENTS);
            printf("  签到方式: 1=全部出勤(再补录异常)  2=逐个录入\n");
            mode = readIntInRange("  请选择: ", 1, 2);
            for (i = 0; i < n; i++) {
                int st = 3;
                if (mode == 2) {
                    char p[96];
                    sprintf(p, "  [%d/%d] %s %s (0缺勤 1迟到 2请假 3出勤): ", i + 1, n, arr[i]->id, arr[i]->name);
                    st = readIntInRange(p, 0, 3);
                }
                if (arr[i]->attCount < MAX_ATT) {
                    AttRec *a = &arr[i]->atts[arr[i]->attCount++];
                    setStr(a->courseId, cid, sizeof(a->courseId));
                    setStr(a->date, date, sizeof(a->date));
                    a->status = st;
                }
                recalcStudent(arr[i]);
            }
            printf("  [完成] 已录入 %d 名学生的考勤记录。\n", n);
            if (mode == 1) {
                printf("  [提示] 如需补录异常，可再次签到并选择逐个录入方式。\n");
            }
            pauseScreen();
        } else if (ch == 2) {
            char sid[16];
            Student *s;
            int i;
            inputString("\n请输入学号: ", sid, sizeof(sid));
            s = findStudent(sid);
            if (!s) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
            printf("\n---- %s(%s) 考勤明细 共 %d 条，出勤率 %.1f%% ----\n",
                   s->name, s->id, s->attCount, s->attRate);
            for (i = 0; i < s->attCount; i++)
                printf("  %2d. %s  %s  %s\n", i + 1, s->atts[i].courseId,
                       s->atts[i].date, attStatusName(s->atts[i].status));
            if (s->attCount == 0) printf("  (暂无考勤记录)\n");
            pauseScreen();
        } else if (ch == 3) {
            char cid[16];
            int i, j;
            int cnt[4] = { 0, 0, 0, 0 };
            inputString("\n请输入课程号: ", cid, sizeof(cid));
            if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            {
                Student *s = g_stuHead;
                while (s) {
                    for (i = 0; i < s->attCount; i++)
                        if (strcmp(s->atts[i].courseId, cid) == 0) {
                            int st = s->atts[i].status;
                            if (st < 0 || st > 3) st = 0;
                            cnt[st]++;
                        }
                    s = s->next;
                }
            }
            printf("\n---- 课程 %s 考勤统计 ----\n", cid);
            for (j = 0; j < 4; j++) printf("  %s: %d 次\n", attStatusName(j), cnt[j]);
            {
                const char *labels[4] = { "缺勤", "迟到", "请假", "出勤" };
                float vals[4];
                for (j = 0; j < 4; j++) vals[j] = (float)cnt[j];
                barChart("考勤分布", labels, vals, 4, "次", 30);
            }
            pauseScreen();
        } else if (ch == 4) {
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            int top = n < 15 ? n : 15;
            sortPtrArray((void **)arr, n, cmpStuByAttAsc);   /* 出勤率升序 = 缺勤最严重在前 */
            printf("\n---- 出勤率排行榜(缺勤最严重 Top %d) ----\n", top);
            showStudentTable(arr, top, 1);
            pauseScreen();
        } else if (ch == 5) {
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            int i, cnt = 0;
            float thr = readFloatInRange("\n请输入预警阈值出勤率(%, 低于该值即预警, 建议 85): ", 0.0f, 100.0f);
            for (i = 0; i < n; i++) {
                if (arr[i]->attRate < thr) {
                    char title[64], content[192];
                    sprintf(title, "考勤预警通知");
                    sprintf(content, "%s同学：你本学期的出勤率为 %.1f%%，低于 %.1f%% 的预警线，"
                                     "请及时与任课教师/辅导员联系并按时到课。", arr[i]->name,
                            arr[i]->attRate, thr);
                    notifyEnqueue('S', arr[i]->id, arr[i]->name, "短信", title, content);
                    cnt++;
                }
            }
            printf("  [完成] 已生成 %d 条考勤预警通知，进入待发送队列（可在“通知中心”发送）。\n", cnt);
            pauseScreen();
        }
    }
}

void menuHonor(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(56, '=');
        printf("            奖惩与第二课堂管理\n");
        printRule(56, '=');
        printf("   1. 添加不良记录          2. 查看不良记录(按严重程度)\n");
        printf("   3. 添加获奖/竞赛/交流    4. 查看获奖与竞赛记录\n");
        printf("   5. 学生综合素质统计      0. 返回上级菜单\n");
        printRule(56, '-');
        ch = readIntInRange("请选择操作: ", 0, 5);
        if (ch == 0) return;

        if (ch == 1) {
            char sid[16], date[12], desc[64];
            int sev;
            Student *s;
            inputString("\n请输入学号: ", sid, sizeof(sid));
            s = findStudent(sid);
            if (!s) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
            inputString("请输入日期(如 2024-05-08): ", date, sizeof(date));
            printf("  严重程度: 1警告 2严重警告 3记过 4留校察看 5开除学籍\n");
            sev = readIntInRange("  请选择(1-5): ", 1, 5);
            inputString("请输入情况说明: ", desc, sizeof(desc));
            if (s->misCount >= MAX_MIS) { printf("  [提示] 该生不良记录已达上限。\n"); pauseScreen(); continue; }
            {
                static const char *T[5] = { "警告", "严重警告", "记过", "留校察看", "开除学籍" };
                MisRec *m = &s->mis[s->misCount++];
                setStr(m->date, date, sizeof(m->date));
                setStr(m->type, T[sev - 1], sizeof(m->type));
                m->severity = sev;
                setStr(m->desc, desc, sizeof(m->desc));
                recalcStudent(s);
                printf("  [完成] 已记录，该生不良记录 %d 条，严重程度合计 %d 分。\n", s->misCount, s->misScore);
                if (sev >= 3) {
                    char content[192];
                    sprintf(content, "%s同学：你于 %s 因“%s”被给予 %s 处分，请注意遵守校纪校规。",
                            s->name, date, desc, T[sev - 1]);
                    notifyEnqueue('S', s->id, s->name, "短信", "违纪处分通知", content);
                    printf("  [提示] 已自动生成一条处分通知进入待发送队列。\n");
                }
            }
            pauseScreen();
        } else if (ch == 2) {
            Student *s = g_stuHead;
            int cnt = 0;
            printf("\n---- 全校不良记录（按严重程度降序）----\n");
            {
                const char *hdr[5] = { "学号", "姓名", "日期", "类型", "程度" };
                const int   w[5]   = { 10, 10, 12, 10, 10 };
                printCells(hdr, w, 5);
                printf("  说明\n");
                printRule(76, '-');
                /* 先把所有记录收集起来，再按严重程度排序 */
                {
                    typedef struct { Student *s; MisRec *m; } Item;
                    static Item items[MAX_STUDENTS * MAX_MIS];
                    int n = 0, i;
                    while (s) {
                        for (i = 0; i < s->misCount; i++) { items[n].s = s; items[n].m = &s->mis[i]; n++; }
                        s = s->next;
                    }
                    /* 插入排序：按严重程度降序，O(n^2) */
                    for (i = 1; i < n; i++) {
                        Item key = items[i];
                        int j = i - 1;
                        while (j >= 0 && items[j].m->severity < key.m->severity) { items[j + 1] = items[j]; j--; }
                        items[j + 1] = key;
                    }
                    for (i = 0; i < n; i++) {
                        const char *row[5];
                        row[0] = items[i].s->id; row[1] = items[i].s->name; row[2] = items[i].m->date;
                        row[3] = items[i].m->type; row[4] = misLevelName(items[i].m->severity);
                        printCells(row, w, 5);
                        printf("  %s\n", items[i].m->desc);
                        cnt++;
                    }
                }
            }
            if (cnt == 0) printf("  (全校无不良记录)\n");
            else printf("\n  共 %d 条不良记录。\n", cnt);
            pauseScreen();
        } else if (ch == 3) {
            char sid[16], date[12], name[48], type[16];
            int lv;
            Student *s;
            inputString("\n请输入学号: ", sid, sizeof(sid));
            s = findStudent(sid);
            if (!s) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
            inputString("请输入日期: ", date, sizeof(date));
            inputString("请输入名称(如 全国大学生数学建模竞赛): ", name, sizeof(name));
            inputString("请输入类型(获奖/竞赛/学术交流): ", type, sizeof(type));
            printf("  级别: 1院级 2校级 3省级 4国家级\n");
            lv = readIntInRange("  请选择(1-4): ", 1, 4);
            if (s->honorCount >= MAX_HONOR) { printf("  [提示] 该生记录已达上限。\n"); pauseScreen(); continue; }
            {
                HonorRec *h = &s->honors[s->honorCount++];
                setStr(h->date, date, sizeof(h->date));
                setStr(h->name, name, sizeof(h->name));
                setStr(h->type, type, sizeof(h->type));
                h->level = lv;
                printf("  [完成] 已记录 %s - %s(%s)\n", type, name, honorLevelName(lv));
            }
            pauseScreen();
        } else if (ch == 4) {
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            int i, k, cnt = 0;
            printf("\n---- 获奖 / 竞赛 / 学术交流记录（按级别降序）----\n");
            {
                typedef struct { Student *s; HonorRec *h; } Item;
                static Item items[MAX_STUDENTS * MAX_HONOR];
                const char *hdr[5] = { "学号", "姓名", "日期", "类型", "级别" };
                const int   w[5]   = { 10, 10, 12, 10, 8 };
                int m = 0;
                printCells(hdr, w, 5);
                printf("  名称\n");
                printRule(76, '-');
                for (i = 0; i < n; i++)
                    for (k = 0; k < arr[i]->honorCount; k++) { items[m].s = arr[i]; items[m].h = &arr[i]->honors[k]; m++; }
                for (i = 1; i < m; i++) {
                    Item key = items[i];
                    k = i - 1;
                    while (k >= 0 && items[k].h->level < key.h->level) { items[k + 1] = items[k]; k--; }
                    items[k + 1] = key;
                }
                for (i = 0; i < m; i++) {
                    const char *row[5];
                    row[0] = items[i].s->id; row[1] = items[i].s->name; row[2] = items[i].h->date;
                    row[3] = items[i].h->type; row[4] = honorLevelName(items[i].h->level);
                    printCells(row, w, 5);
                    printf("  %s\n", items[i].h->name);
                    cnt++;
                }
            }
            if (cnt == 0) printf("  (暂无记录)\n");
            else printf("\n  共 %d 条记录。\n", cnt);
            pauseScreen();
        } else if (ch == 5) {
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            int i, cnt = 0;
            printf("\n---- 学生综合素质统计（获奖数 > 0 或 有不良记录）----\n");
            {
                const char *hdr[6] = { "学号", "姓名", "绩点", "获奖/竞赛", "不良记录", "综合评定" };
                const int   w[6]   = { 10, 10, 8, 10, 10, 12 };
                printCells(hdr, w, 6);
                printRule(76, '-');
                for (i = 0; i < n; i++) {
                    if (arr[i]->honorCount > 0 || arr[i]->misCount > 0) {
                        const char *grade;
                        char gpaBuf[16], hBuf[16], mBuf[16];
                        const char *row[6];
                        if (arr[i]->gpa >= 3.5f && arr[i]->misCount == 0) grade = "优秀";
                        else if (arr[i]->misCount >= 2 || arr[i]->misScore >= 5) grade = "需重点关注";
                        else if (arr[i]->gpa >= 3.0f) grade = "良好";
                        else grade = "合格";
                        sprintf(gpaBuf, "%.2f", arr[i]->gpa);
                        sprintf(hBuf, "%d", arr[i]->honorCount);
                        sprintf(mBuf, "%d", arr[i]->misCount);
                        row[0] = arr[i]->id; row[1] = arr[i]->name; row[2] = gpaBuf;
                        row[3] = hBuf; row[4] = mBuf; row[5] = grade;
                        printCells(row, w, 6);
                        cnt++;
                    }
                }
            }
            if (cnt == 0) printf("  (无相关记录)\n");
            else printf("\n  共 %d 名学生。\n", cnt);
            pauseScreen();
        }
    }
}

void menuQA(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(56, '=');
        printf("            答疑 / 辅导管理\n");
        printRule(56, '=');
        printf("   1. 新增答疑记录          2. 查看全部答疑记录\n");
        printf("   3. 教师答疑工作量统计    0. 返回上级菜单\n");
        printRule(56, '-');
        ch = readIntInRange("请选择操作: ", 0, 3);
        if (ch == 0) return;

        if (ch == 1) {
            char sid[16], tid[16], cid[16], date[12], q[80], a[80];
            if (g_qaCount >= MAX_QA) { printf("\n  [提示] 答疑记录已达上限。\n"); pauseScreen(); continue; }
            inputString("\n请输入学号: ", sid, sizeof(sid));
            if (!findStudent(sid)) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
            inputString("请输入教师工号: ", tid, sizeof(tid));
            if (!findTeacher(tid)) { printf("  [错误] 未找到该教师。\n"); pauseScreen(); continue; }
            showCourseTable();
            inputString("请输入课程号: ", cid, sizeof(cid));
            if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            inputString("请输入日期: ", date, sizeof(date));
            inputString("请输入问题: ", q, sizeof(q));
            inputString("请输入解答: ", a, sizeof(a));
            setStr(g_qa[g_qaCount].studentId, sid, sizeof(g_qa[0].studentId));
            setStr(g_qa[g_qaCount].teacherId, tid, sizeof(g_qa[0].teacherId));
            setStr(g_qa[g_qaCount].courseId, cid, sizeof(g_qa[0].courseId));
            setStr(g_qa[g_qaCount].date, date, sizeof(g_qa[0].date));
            setStr(g_qa[g_qaCount].question, q, sizeof(g_qa[0].question));
            setStr(g_qa[g_qaCount].answer, a, sizeof(g_qa[0].answer));
            g_qaCount++;
            printf("  [完成] 答疑记录已保存。\n");
            pauseScreen();
        } else if (ch == 2) {
            int i;
            printf("\n---- 全部答疑记录（按日期插入排序）----\n");
            /* 用插入排序按日期降序排列索引 */
            {
                static int idx[MAX_QA];
                int n = g_qaCount;
                for (i = 0; i < n; i++) idx[i] = i;
                for (i = 1; i < n; i++) {
                    int key = idx[i], j = i - 1;
                    while (j >= 0 && strcmp(g_qa[idx[j]].date, g_qa[key].date) < 0) { idx[j + 1] = idx[j]; j--; }
                    idx[j + 1] = key;
                }
                for (i = 0; i < n; i++) {
                    QaRec *r = &g_qa[idx[i]];
                    Student *s = findStudent(r->studentId);
                    Teacher *t = findTeacher(r->teacherId);
                    printf("  %2d. %s [%s] %s(%s) 问 %s(%s)：%s\n", i + 1, r->date, r->courseId,
                           s ? s->name : "?", r->studentId, t ? t->name : "?", r->teacherId, r->question);
                    printf("      答: %s\n", r->answer);
                }
                if (n == 0) printf("  (暂无答疑记录)\n");
            }
            pauseScreen();
        } else if (ch == 3) {
            Teacher *arr[MAX_TEACHERS];
            int n = collectTeachers(arr, MAX_TEACHERS);
            int i, k;
            printf("\n---- 教师答疑工作量统计 ----\n");
            for (i = 0; i < n; i++) {
                int c = 0;
                for (k = 0; k < g_qaCount; k++) if (strcmp(g_qa[k].teacherId, arr[i]->id) == 0) c++;
                if (c > 0) {
                    const char *row[4];
                    char b3[16];
                    sprintf(b3, "%d", c);
                    row[0] = arr[i]->id; row[1] = arr[i]->name; row[2] = arr[i]->title; row[3] = b3;
                    printCells(row, (const int[]){ 8, 10, 10, 6 }, 4);
                }
            }
            pauseScreen();
        }
    }
}

void menuSearch(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(60, '=');
        printf("            查询中心（精确查找 / 模糊查找）\n");
        printRule(60, '=');
        printf("   1. 精确查找学生(学号, 二分查找)   2. 精确查找教师(工号)\n");
        printf("   3. 精确查找课程(课程号)           4. 模糊查找学生(KMP)\n");
        printf("   5. 模糊查找教师(KMP)              6. 模糊查找课程(KMP)\n");
        printf("   7. 按分数区间查找学生             0. 返回上级菜单\n");
        printRule(60, '-');
        ch = readIntInRange("请选择操作: ", 0, 7);
        if (ch == 0) return;

        if (ch == 1) {
            /* 精确查找：先按学号快速排序，再二分查找 O(n log n + log n) */
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            char sid[16];
            inputString("\n请输入完整学号: ", sid, sizeof(sid));
            sortPtrArray((void **)arr, n, cmpStuByIdAsc);
            {
                int lo = 0, hi = n - 1, found = -1;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    int c = strcmp(arr[mid]->id, sid);
                    if (c == 0) { found = mid; break; }
                    if (c < 0) lo = mid + 1; else hi = mid - 1;
                }
                printf("  [算法] 已按学号升序排序，采用二分查找（比较次数约 log2(%d) ≈ %.0f 次）\n",
                       n, n > 0 ? (log((double)n) / log(2.0)) : 0.0);
                if (found >= 0) showStudentDetail(arr[found]);
                else printf("  [结果] 未找到学号为 %s 的学生。\n", sid);
            }
            pauseScreen();
        } else if (ch == 2) {
            Teacher *arr[MAX_TEACHERS];
            int n = collectTeachers(arr, MAX_TEACHERS);
            char tid[16];
            inputString("\n请输入完整工号: ", tid, sizeof(tid));
            sortPtrArray((void **)arr, n, cmpTeaByIdAsc);
            {
                int lo = 0, hi = n - 1, found = -1;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    int c = strcmp(arr[mid]->id, tid);
                    if (c == 0) { found = mid; break; }
                    if (c < 0) lo = mid + 1; else hi = mid - 1;
                }
                if (found >= 0) showTeacherDetail(arr[found]);
                else printf("  [结果] 未找到工号为 %s 的教师。\n", tid);
            }
            pauseScreen();
        } else if (ch == 3) {
            char cid[16];
            inputString("\n请输入完整课程号: ", cid, sizeof(cid));
            showCourseDetail(findCourse(cid));
            pauseScreen();
        } else if (ch == 4) {
            char key[32];
            Student *p = g_stuHead;
            int cnt = 0;
            inputString("\n请输入关键字(姓名/学号/班级的任意片段): ", key, sizeof(key));
            printf("\n---- 模糊查找结果（KMP 字符串匹配）----\n");
            {
                const char *hdr[6] = { "学号", "姓名", "性别", "班级", "绩点", "出勤率" };
                const int   w[6]   = { 10, 10, 6, 14, 8, 8 };
                printCells(hdr, w, 6);
                printRule(66, '-');
                while (p) {
                    if (containsIgnoreCase(p->id, key) || containsIgnoreCase(p->name, key) ||
                        containsIgnoreCase(p->className, key)) {
                        char b4[16], b5[16];
                        const char *row[6];
                        sprintf(b4, "%.2f", p->gpa);
                        sprintf(b5, "%.1f", p->attRate);
                        row[0] = p->id; row[1] = p->name; row[2] = p->gender;
                        row[3] = p->className; row[4] = b4; row[5] = b5;
                        printCells(row, w, 6);
                        cnt++;
                    }
                    p = p->next;
                }
            }
            printf("\n  共匹配到 %d 条记录。\n", cnt);
            pauseScreen();
        } else if (ch == 5) {
            char key[32];
            Teacher *p = g_teaHead;
            int cnt = 0;
            inputString("\n请输入关键字(姓名/工号/学院/职称的任意片段): ", key, sizeof(key));
            printf("\n---- 模糊查找结果（KMP 字符串匹配）----\n");
            {
                const char *hdr[5] = { "工号", "姓名", "职称", "学院", "评教" };
                const int   w[5]   = { 8, 10, 10, 14, 8 };
                printCells(hdr, w, 5);
                printRule(66, '-');
                while (p) {
                    if (containsIgnoreCase(p->id, key) || containsIgnoreCase(p->name, key) ||
                        containsIgnoreCase(p->dept, key) || containsIgnoreCase(p->title, key)) {
                        char b4[16];
                        const char *row[5];
                        sprintf(b4, "%.2f", p->evalAvg);
                        row[0] = p->id; row[1] = p->name; row[2] = p->title;
                        row[3] = p->dept; row[4] = b4;
                        printCells(row, w, 5);
                        cnt++;
                    }
                    p = p->next;
                }
            }
            printf("\n  共匹配到 %d 条记录。\n", cnt);
            pauseScreen();
        } else if (ch == 6) {
            char key[32];
            int i, cnt = 0;
            inputString("\n请输入关键字(课程号/课程名称的任意片段): ", key, sizeof(key));
            printf("\n---- 模糊查找结果（KMP 字符串匹配）----\n");
            for (i = 0; i < g_courseCount; i++) {
                if (containsIgnoreCase(g_courses[i].id, key) || containsIgnoreCase(g_courses[i].name, key)) {
                    showCourseDetail(&g_courses[i]);
                    cnt++;
                }
            }
            if (cnt == 0) printf("  未找到匹配的课程。\n");
            pauseScreen();
        } else if (ch == 7) {
            float lo = readFloatInRange("\n请输入分数下限(0-100): ", 0.0f, 100.0f);
            float hi = readFloatInRange("请输入分数上限(0-100): ", lo, 100.0f);
            char cid[16];
            int i, cnt = 0;
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            inputStringOpt("请输入课程号(直接回车=全部课程): ", cid, sizeof(cid));
            printf("\n---- 总评成绩在 [%.1f, %.1f] 区间的记录 ----\n", lo, hi);
            for (i = 0; i < n; i++) {
                int k;
                for (k = 0; k < arr[i]->gradeCount; k++) {
                    Grade *g = &arr[i]->grades[k];
                    float t = gradeTotal(g);
                    if (t >= lo && t <= hi && (cid[0] == '\0' || strcmp(g->courseId, cid) == 0)) {
                        char b3[16], b4[16], b5[16];
                        const char *row[5];
                        sprintf(b3, "%s", g->courseId);
                        sprintf(b4, "%.1f", g->usual);
                        sprintf(b5, "%.1f", g->exam);
                        row[0] = arr[i]->id; row[1] = arr[i]->name; row[2] = b3; row[3] = b4; row[4] = b5;
                        printCells(row, (const int[]){ 10, 10, 8, 8, 8 }, 5);
                        printf("  总评 %.2f\n", t);
                        cnt++;
                    }
                }
            }
            printf("\n  共 %d 条记录。\n", cnt);
            pauseScreen();
        }
    }
}

void menuSort(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(60, '=');
        printf("            排序与排名（快速排序 / 插入排序）\n");
        printRule(60, '=');
        printf("   1. 学生按学号升序            2. 学生按姓名升序\n");
        printf("   3. 学生按班级升序            4. 学生按加权平均分降序\n");
        printf("   5. 学生按学分绩点降序        6. 学生按出勤率升序(缺勤在前)\n");
        printf("   7. 学生按不良记录严重程度降序 8. 教师按评教分降序\n");
        printf("   9. 教师按授课班级平均分降序 10. 课程按平均分降序\n");
        printf("  11. 课程按选课人数降序       12. 选定课程成绩排名\n");
        printf("   0. 返回上级菜单\n");
        printRule(60, '-');
        ch = readIntInRange("请选择操作: ", 0, 12);
        if (ch == 0) return;

        if (ch >= 1 && ch <= 7) {
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            const char *name;
            switch (ch) {
                case 1: sortPtrArray((void **)arr, n, cmpStuByIdAsc);        name = "学号升序"; break;
                case 2: sortPtrArray((void **)arr, n, cmpStuByNameAsc);      name = "姓名升序"; break;
                case 3: sortPtrArray((void **)arr, n, cmpStuByClassAsc);     name = "班级升序"; break;
                case 4: sortPtrArray((void **)arr, n, cmpStuByWeightedDesc); name = "加权平均分降序"; break;
                case 5: sortPtrArray((void **)arr, n, cmpStuByGpaDesc);      name = "学分绩点降序"; break;
                case 6: sortPtrArray((void **)arr, n, cmpStuByAttAsc);       name = "出勤率升序(缺勤最多在前)"; break;
                default: sortPtrArray((void **)arr, n, cmpStuByMisDesc);     name = "不良记录严重程度降序"; break;
            }
            printf("\n---- 学生排序结果：%s ----\n", name);
            showStudentTable(arr, n, (ch == 4 || ch == 5) ? 1 : 0);
            pauseScreen();
        } else if (ch == 8 || ch == 9) {
            Teacher *arr[MAX_TEACHERS];
            int n = collectTeachers(arr, MAX_TEACHERS);
            if (ch == 8) {
                sortPtrArray((void **)arr, n, cmpTeaByEvalDesc);
                printf("\n---- 教师授课效果评价排名（降序）----\n");
            } else {
                sortPtrArray((void **)arr, n, cmpTeaByAvgScoreDesc);
                printf("\n---- 教师所授课程学生平均分排名（降序）----\n");
            }
            showTeacherTable(arr, n);
            pauseScreen();
        } else if (ch == 10 || ch == 11) {
            Course *arr[MAX_COURSES];
            int i, n = g_courseCount;
            for (i = 0; i < n; i++) arr[i] = &g_courses[i];
            if (ch == 10) {
                sortPtrArray((void **)arr, n, cmpCourseByAvgDesc);
                printf("\n---- 课程平均总评排名（降序）----\n");
            } else {
                sortPtrArray((void **)arr, n, cmpCourseByCountDesc);
                printf("\n---- 课程选课人数排名（降序）----\n");
            }
            {
                const char *hdr[5] = { "课程号", "课程名称", "平均总评", "选课人数", "任课教师数" };
                const int   w[5]   = { 8, 16, 10, 10, 12 };
                printCells(hdr, w, 5);
                printRule(66, '-');
                for (i = 0; i < n; i++) {
                    Teacher *p = g_teaHead; int tc = 0;
                    char b2[16], b3[16], b4[16];
                    const char *row[5];
                    while (p) { int k; for (k = 0; k < p->itemCount; k++)
                                    if (strcmp(p->items[k].courseId, arr[i]->id) == 0) tc++;
                                p = p->next; }
                    sprintf(b2, "%.2f", courseAvgScore(arr[i]->id));
                    sprintf(b3, "%d", courseStudentCount(arr[i]->id));
                    sprintf(b4, "%d", tc);
                    row[0] = arr[i]->id; row[1] = arr[i]->name; row[2] = b2; row[3] = b3; row[4] = b4;
                    printCells(row, w, 5);
                }
            }
            pauseScreen();
        } else if (ch == 12) {
            char cid[16];
            Student *arr[MAX_STUDENTS];
            int n = collectStudents(arr, MAX_STUDENTS);
            int i, m = 0;
            inputString("\n请输入课程号: ", cid, sizeof(cid));
            if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            /* 只有选了该课的学生参与排名 */
            for (i = 0; i < n; i++) if (findGrade(arr[i], cid)) arr[m++] = arr[i];
            /* 按该课程总评成绩冒泡降序（演示另一种排序算法） */
            for (i = 0; i < m - 1; i++) {
                int j, swapped = 0;
                for (j = 0; j < m - 1 - i; j++) {
                    Grade *g1 = findGrade(arr[j], cid), *g2 = findGrade(arr[j + 1], cid);
                    if (gradeTotal(g1) < gradeTotal(g2)) {
                        Student *tmp = arr[j]; arr[j] = arr[j + 1]; arr[j + 1] = tmp;
                        swapped = 1;
                    }
                }
                if (!swapped) break;   /* 冒泡优化：某趟无交换则已有序 */
            }
            printf("\n---- 课程 %s 成绩排名（冒泡排序，降序）----\n", cid);
            {
                const char *hdr[7] = { "名次", "学号", "姓名", "平时", "考试", "总评", "绩点" };
                const int   w[7]   = { 6, 10, 10, 8, 8, 8, 8 };
                printCells(hdr, w, 7);
                printRule(70, '-');
                for (i = 0; i < m; i++) {
                    Grade *g = findGrade(arr[i], cid);
                    char b0[16], b3[16], b4[16], b5[16], b6[16];
                    const char *row[7];
                    sprintf(b0, "%d", i + 1);
                    sprintf(b3, "%.1f", g->usual);
                    sprintf(b4, "%.1f", g->exam);
                    sprintf(b5, "%.2f", gradeTotal(g));
                    sprintf(b6, "%.2f", scoreToPoint(gradeTotal(g)));
                    row[0] = b0; row[1] = arr[i]->id; row[2] = arr[i]->name;
                    row[3] = b3; row[4] = b4; row[5] = b5; row[6] = b6;
                    printCells(row, w, 7);
                }
            }
            printf("\n  共 %d 名学生参与排名。\n", m);
            pauseScreen();
        }
    }
}

void vizHistogram(void)
{
    int scope;
    printf("\n  1=全部课程综合  2=指定课程\n");
    scope = readIntInRange("  请选择: ", 1, 2);
    {
        const char *labels[5] = { "0-59分", "60-69分", "70-79分", "80-89分", "90-100分" };
        float vals[5] = { 0, 0, 0, 0, 0 };
        Student *s = g_stuHead;
        char cid[16] = "";
        char title[96];
        if (scope == 2) {
            inputString("  请输入课程号: ", cid, sizeof(cid));
            if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); return; }
            sprintf(title, "课程 %s 总评成绩分布直方图", cid);
        } else {
            setStr(title, "全部课程总评成绩分布直方图", sizeof(title));
        }
        while (s) {
            int k;
            for (k = 0; k < s->gradeCount; k++) {
                float t = gradeTotal(&s->grades[k]);
                if (cid[0] && strcmp(s->grades[k].courseId, cid) != 0) continue;
                if (t < 60) vals[0] += 1;
                else if (t < 70) vals[1] += 1;
                else if (t < 80) vals[2] += 1;
                else if (t < 90) vals[3] += 1;
                else vals[4] += 1;
            }
            s = s->next;
        }
        barChart(title, labels, vals, 5, "人次", 40);
        {
            float sum = vals[0] * 30 + vals[1] * 65 + vals[2] * 75 + vals[3] * 85 + vals[4] * 95;
            float total = vals[0] + vals[1] + vals[2] + vals[3] + vals[4];
            printf("  参考: 样本 %d 人次，及格率 %.1f%%，优秀率(>=90) %.1f%%\n",
                   (int)total, total > 0 ? (1 - vals[0] / total) * 100.0f : 0.0f,
                   total > 0 ? vals[4] / total * 100.0f : 0.0f);
            printf("  (加权估算平均分 ≈ %.2f)\n", total > 0 ? sum / total : 0.0f);
        }
    }
}

void vizCourseAvg(void)
{
    const char *labels[MAX_COURSES];
    float vals[MAX_COURSES];
    int i;
    for (i = 0; i < g_courseCount; i++) {
        labels[i] = g_courses[i].name;
        vals[i] = courseAvgScore(g_courses[i].id);
    }
    barChart("各课程平均总评成绩对比", labels, vals, g_courseCount, "分", 36);
}

void vizTeacherEval(void)
{
    Teacher *arr[MAX_TEACHERS];
    int n = collectTeachers(arr, MAX_TEACHERS);
    const char *labels[MAX_TEACHERS];
    float vals[MAX_TEACHERS];
    int i;
    sortPtrArray((void **)arr, n, cmpTeaByEvalDesc);
    for (i = 0; i < n; i++) { labels[i] = arr[i]->name; vals[i] = arr[i]->evalAvg; }
    barChart("教师授课效果评价排行（学生评教，满分 5 分）", labels, vals, n, "分", 36);
}

void vizGpaTop(void)
{
    Student *arr[MAX_STUDENTS];
    int n = collectStudents(arr, MAX_STUDENTS);
    int top;
    if (n == 0) { printf("\n  [提示] 暂无学生数据，无法排行。\n"); return; }
    printf("\n  请输入显示人数(1-%d): ", n);
    top = readIntInRange("", 1, n);
    sortPtrArray((void **)arr, n, cmpStuByGpaDesc);
    {
        const char *labels[MAX_STUDENTS];
        float vals[MAX_STUDENTS];
        int i;
        for (i = 0; i < top; i++) { labels[i] = arr[i]->name; vals[i] = arr[i]->gpa; }
        barChart("学生学分绩点排行（Top N）", labels, vals, top, "绩点", 30);
    }
    printf("\n---- 详细名单 ----\n");
    showStudentTable(arr, top, 1);
}

void vizAttendance(void)
{
    const char *labels[MAX_COURSES];
    float vals[MAX_COURSES];
    int i;
    for (i = 0; i < g_courseCount; i++) {
        Student *s = g_stuHead;
        float sum = 0.0f; int n = 0;
        while (s) {
            int k; float p = 0.0f, t = 0.0f;
            for (k = 0; k < s->attCount; k++) {
                if (strcmp(s->atts[k].courseId, g_courses[i].id) != 0) continue;
                t += 1.0f;
                if (s->atts[k].status == 3) p += 1.0f;
                else if (s->atts[k].status == 1) p += 0.5f;
            }
            if (t > 0.0f) { sum += p / t * 100.0f; n++; }
            s = s->next;
        }
        labels[i] = g_courses[i].name;
        vals[i] = n ? sum / (float)n : 0.0f;
    }
    barChart("各课程平均出勤率对比（%）", labels, vals, g_courseCount, "%", 36);
}

void vizMisconduct(void)
{
    const char *labels[5] = { "1警告", "2严重警告", "3记过", "4留校察看", "5开除学籍" };
    float vals[5] = { 0, 0, 0, 0, 0 };
    Student *s = g_stuHead;
    while (s) {
        int i;
        for (i = 0; i < s->misCount; i++) {
            int sev = s->mis[i].severity;
            if (sev < 1) sev = 1;
            if (sev > 5) sev = 5;
            vals[sev - 1] += 1;
        }
        s = s->next;
    }
    barChart("不良记录严重程度分布", labels, vals, 5, "条", 30);
}

void vizDashboard(void)
{
    Student *arr[MAX_STUDENTS];
    int n = collectStudents(arr, MAX_STUDENTS);
    float sumGpa = 0.0f, sumAtt = 0.0f;
    float maxW = -1.0f;
    int failCnt = 0, misStu = 0, honorStu = 0;
    int i, k;
    Student *best = NULL;
    printf("\n");
    printRule(74, '=');
    printf("                        智 慧 教 学 综 合 看 板\n");
    printRule(74, '=');
    printf("   课程总数: %-4d   教师总数: %-4d   学生总数: %-4d   作业记录: %d 条\n",
           g_courseCount, g_teaCount, g_stuCount, g_hwCount);
    printf("   通知队列: %-4d   已发送记录: %-4d 答疑记录: %d 条\n",
           g_nqCount, g_sentCount, g_qaCount);
    printRule(74, '-');
    for (i = 0; i < n; i++) {
        sumGpa += arr[i]->gpa;
        sumAtt += arr[i]->attRate;
        if (arr[i]->weighted > maxW) { maxW = arr[i]->weighted; best = arr[i]; }
        if (arr[i]->misCount > 0) misStu++;
        if (arr[i]->honorCount > 0) honorStu++;
        for (k = 0; k < arr[i]->gradeCount; k++)
            if (gradeTotal(&arr[i]->grades[k]) < 60.0f) failCnt++;
    }
    printf("   全体平均绩点: %.2f        全体平均出勤率: %.1f%%\n",
           n ? sumGpa / (float)n : 0.0f, n ? sumAtt / (float)n : 0.0f);
    printf("   成绩最优学生: %s (%s)  加权平均 %.2f 分，绩点 %.2f\n",
           best ? best->name : "-", best ? best->id : "-", best ? best->weighted : 0.0f, best ? best->gpa : 0.0f);
    printf("   不及格课程门次: %-4d     有不良记录学生: %-4d   有获奖/竞赛学生: %d\n",
           failCnt, misStu, honorStu);
    printRule(74, '-');
    printf("   开课情况:\n");
    for (i = 0; i < g_courseCount; i++) {
        Teacher *p = g_teaHead; int tc = 0;
        while (p) { for (k = 0; k < p->itemCount; k++)
                        if (strcmp(p->items[k].courseId, g_courses[i].id) == 0) tc++;
                    p = p->next; }
        printf("     %-6s %-14s 学分 %.1f  教师 %d 人(%s)  选课 %3d 人(%s)  平均 %.1f 分\n",
               g_courses[i].id, g_courses[i].name, g_courses[i].credit, tc,
               tc >= 3 ? "达标" : "不足", courseStudentCount(g_courses[i].id),
               courseStudentCount(g_courses[i].id) >= 70 ? "达标" : "不足",
               courseAvgScore(g_courses[i].id));
    }
    printRule(74, '=');
}

void menuViz(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(60, '=');
        printf("            统计分析与可视化\n");
        printRule(60, '=');
        printf("   1. 成绩分布直方图        2. 各课程平均分对比图\n");
        printf("   3. 教师评教排行图        4. 学生绩点排行图(Top N)\n");
        printf("   5. 各课程出勤率对比图    6. 不良记录严重程度分布图\n");
        printf("   7. 综合看板(数据总览)    0. 返回上级菜单\n");
        printRule(60, '-');
        ch = readIntInRange("请选择操作: ", 0, 7);
        if (ch == 0) return;
        switch (ch) {
            case 1: vizHistogram(); break;
            case 2: vizCourseAvg(); break;
            case 3: vizTeacherEval(); break;
            case 4: vizGpaTop(); break;
            case 5: vizAttendance(); break;
            case 6: vizMisconduct(); break;
            default: vizDashboard(); break;
        }
        pauseScreen();
    }
}

void menuNotify(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(64, '=');
        printf("            通知中心（模拟短信 / 邮件发送）\n");
        printRule(64, '=');
        printf("   1. 发送通知给单个学生/教师     2. 群发通知给某课程全体学生\n");
        printf("   3. 群发教学活动通知给全体教师  4. 发布某课程成绩通知\n");
        printf("   5. 查看待发送队列              6. 发送队列中全部通知\n");
        printf("   7. 查看已发送记录(栈)          8. 撤销最近一次发送\n");
        printf("   9. 清空待发送队列              0. 返回上级菜单\n");
        printRule(64, '-');
        ch = readIntInRange("请选择操作: ", 0, 9);
        if (ch == 0) return;

        if (ch == 1) {
            int type, chan;
            char id[16], title[64], content[192];
            printf("\n  1=学生  2=教师\n");
            type = readIntInRange("  收件人类型: ", 1, 2);
            inputString("  请输入学号/工号: ", id, sizeof(id));
            printf("  1=短信  2=邮件\n");
            chan = readIntInRange("  发送方式: ", 1, 2);
            inputString("  通知标题: ", title, sizeof(title));
            inputString("  通知内容: ", content, sizeof(content));
            if (type == 1) {
                Student *s = findStudent(id);
                if (!s) { printf("  [错误] 未找到该学生。\n"); pauseScreen(); continue; }
                notifyEnqueue('S', s->id, s->name, chan == 1 ? "短信" : "邮件", title, content);
                printf("  [完成] 通知已进入待发送队列，请选择“发送队列中全部通知”执行模拟发送。\n");
            } else {
                Teacher *t = findTeacher(id);
                if (!t) { printf("  [错误] 未找到该教师。\n"); pauseScreen(); continue; }
                notifyEnqueue('T', t->id, t->name, chan == 1 ? "短信" : "邮件", title, content);
                printf("  [完成] 通知已进入待发送队列。\n");
            }
            pauseScreen();
        } else if (ch == 2) {
            char cid[16], title[64], content[192];
            int chan;
            showCourseTable();
            inputString("\n  请输入课程号: ", cid, sizeof(cid));
            if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            printf("  1=短信  2=邮件\n");
            chan = readIntInRange("  发送方式: ", 1, 2);
            inputString("  通知标题: ", title, sizeof(title));
            inputString("  通知内容: ", content, sizeof(content));
            notifyToStudentsOfCourse(cid, chan == 1 ? "短信" : "邮件", title, content);
            pauseScreen();
        } else if (ch == 3) {
            char title[64], content[192];
            int chan, cnt = 0;
            Teacher *t;
            printf("  1=短信  2=邮件\n");
            chan = readIntInRange("\n  发送方式: ", 1, 2);
            inputString("  通知标题: ", title, sizeof(title));
            inputString("  通知内容: ", content, sizeof(content));
            t = g_teaHead;
            while (t) {
                notifyEnqueue('T', t->id, t->name, chan == 1 ? "短信" : "邮件", title, content);
                cnt++; t = t->next;
            }
            printf("  [完成] 已为全体 %d 名教师生成通知，进入待发送队列。\n", cnt);
            pauseScreen();
        } else if (ch == 4) {
            char cid[16], content[192];
            Student *s;
            int chan, cnt = 0;
            showCourseTable();
            inputString("\n  请输入课程号: ", cid, sizeof(cid));
            if (!findCourse(cid)) { printf("  [错误] 未找到该课程。\n"); pauseScreen(); continue; }
            printf("  1=短信  2=邮件\n");
            chan = readIntInRange("  发送方式: ", 1, 2);
            s = g_stuHead;
            while (s) {
                Grade *g = findGrade(s, cid);
                if (g) {
                    sprintf(content, "%s同学：你《%s》课程的总评成绩为 %.2f 分（平时 %.1f / 考试 %.1f），"
                                     "绩点 %.2f。请登录教学平台查看详情。",
                            s->name, findCourse(cid)->name, gradeTotal(g), g->usual, g->exam,
                            scoreToPoint(gradeTotal(g)));
                    notifyEnqueue('S', s->id, s->name, chan == 1 ? "短信" : "邮件", "成绩发布通知", content);
                    cnt++;
                }
                s = s->next;
            }
            printf("  [完成] 已生成 %d 条成绩发布通知，进入待发送队列。\n", cnt);
            pauseScreen();
        } else if (ch == 5) { notifyShowQueue(); pauseScreen(); }
        else if (ch == 6) { notifySendAll(); pauseScreen(); }
        else if (ch == 7) { notifyShowSent(); pauseScreen(); }
        else if (ch == 8) { notifyUndoLast(); pauseScreen(); }
        else if (ch == 9) {
            notifyClear();
            printf("\n  [完成] 待发送队列与已发送记录已清空。\n");
            pauseScreen();
        }
    }
}

void menuFile(void)
{
    while (1) {
        int ch;
        printf("\n");
        printRule(56, '=');
        printf("            数据持久化与系统维护\n");
        printRule(56, '=');
        printf("   1. 保存数据到文件(%s)\n", DATA_FILE);
        printf("   2. 从文件载入数据(覆盖当前数据)\n");
        printf("   3. 重新生成示例数据(75名学生/12名教师/4门课程)\n");
        printf("   4. 查看当前数据规模统计\n");
        printf("   0. 返回上级菜单\n");
        printRule(56, '-');
        ch = readIntInRange("请选择操作: ", 0, 4);
        if (ch == 0) return;

        if (ch == 1) { saveData(); pauseScreen(); }
        else if (ch == 2) {
            if (readIntInRange("\n  1=确认载入(将覆盖当前数据)  0=取消: ", 0, 1) == 1) loadData();
            else printf("  已取消。\n");
            pauseScreen();
        } else if (ch == 3) {
            if (readIntInRange("\n  1=确认重新生成  0=取消: ", 0, 1) == 1) initSampleData();
            else printf("  已取消。\n");
            pauseScreen();
        } else if (ch == 4) {
            printf("\n---- 当前数据规模 ----\n");
            printf("  课程 %d 门 (容量 %d)\n", g_courseCount, MAX_COURSES);
            printf("  教师 %d 名 (容量 %d)\n", g_teaCount, MAX_TEACHERS);
            printf("  学生 %d 名 (容量 %d)\n", g_stuCount, MAX_STUDENTS);
            printf("  作业记录 %d 条 (容量 %d)\n", g_hwCount, MAX_HW);
            printf("  答疑记录 %d 条 (容量 %d)\n", g_qaCount, MAX_QA);
            printf("  待发送通知 %d 条，已发送 %d 条\n", g_nqCount, g_sentCount);
            pauseScreen();
        }
    }
}

void showMainMenu(void)
{
    printf("\n");
    printRule(72, '=');
    printf("               智 慧 教 学 管 理 系 统\n");
    printf("          Smart Teaching Management System  v1.0\n");
    printf("          《数据结构与算法基础》课程设计\n");
    printRule(72, '=');
    printf("    1. 学生信息管理           2. 教师信息管理\n");
    printf("    3. 课程信息管理           4. 成绩与作业管理\n");
    printf("    5. 考勤管理               6. 奖惩与第二课堂\n");
    printf("    7. 答疑 / 辅导管理        8. 查询中心(精确/模糊)\n");
    printf("    9. 排序与排名            10. 统计分析与可视化\n");
    printf("   11. 通知中心(模拟短信/邮件) 12. 数据保存与载入\n");
    printf("   13. 重新生成示例数据       0. 退出系统\n");
    printRule(72, '-');
    printf("  数据概览: 课程 %d 门 | 教师 %d 名 | 学生 %d 名 | 待发送通知 %d 条\n",
           g_courseCount, g_teaCount, g_stuCount, g_nqCount);
    printRule(72, '=');
}

int main(void)
{
#ifdef _WIN32
    /* 控制台切换为 UTF-8，保证中文正常显示 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    printf("\n 正在初始化智慧教学管理系统...\n");
    initSampleData();          /* 首次运行自动生成示例数据 */
    printf(" 提示: 如需从文件恢复数据，请选择菜单 12 -> 2。\n");

    while (!g_quit) {
        int ch;
        showMainMenu();
        ch = readIntInRange("请选择操作: ", 0, 13);
        switch (ch) {
            case 1:  menuStudent();    break;
            case 2:  menuTeacher();    break;
            case 3:  menuCourse();     break;
            case 4:  menuScore();      break;
            case 5:  menuAttendance(); break;
            case 6:  menuHonor();      break;
            case 7:  menuQA();         break;
            case 8:  menuSearch();     break;
            case 9:  menuSort();       break;
            case 10: menuViz();        break;
            case 11: menuNotify();     break;
            case 12: menuFile();       break;
            case 13: initSampleData(); pauseScreen(); break;
            case 0:
                printf("\n  是否保存数据后退出？ 1=保存并退出  0=直接退出\n");
                if (readIntInRange("  请选择: ", 0, 1) == 1) saveData();
                printf("\n  感谢使用智慧教学管理系统，再见！\n\n");
                g_quit = 1;
                break;
            default: break;
        }
    }

    clearAllData();   /* 释放全部链表结点，避免内存泄漏 */
    return 0;
}

