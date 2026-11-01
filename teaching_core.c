/* =============================================================================
 *  teaching_core.c  —— 智慧教学管理系统【数据层】实现
 *  与界面无关：结构体、增删改查、排序、查找、统计、示例数据、存档、通知队列
 *  编译：与 console_main.c 或 gui_main.c 一起编译
 * ========================================================================== */
#include "teaching_core.h"
#include <stdarg.h>

/* --------------------------- 消息输出钩子实现 --------------------------- */
static CoreMsgFn g_msgFn = NULL;

void coreSetMessageHandler(CoreMsgFn fn)
{
    g_msgFn = fn;
}

/* 数据层的统一输出出口：界面层可接管（图形界面写入日志框） */
void coreMsg(const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (g_msgFn) g_msgFn(buf);
    else { fputs(buf, stdout); fflush(stdout); }
}

/* ============================== 全局数据表 ============================== *//* ------------------------------- 全局变量 ------------------------------- */
Course   g_courses[MAX_COURSES];  int g_courseCount = 0;
Student *g_stuHead = NULL;        int g_stuCount = 0;
Teacher *g_teaHead = NULL;        int g_teaCount = 0;
Homework g_hw[MAX_HW];            int g_hwCount = 0;
QaRec    g_qa[MAX_QA];            int g_qaCount = 0;
int      g_hwNextId = 1;

Notify  *g_nqHead = NULL, *g_nqTail = NULL;   /* 待发送队列 */
int      g_nqCount = 0;
Notify  *g_sentTop = NULL;                    /* 已发送栈   */
int      g_sentCount = 0;

unsigned int g_seed = 20240501u;              /* 随机数种子 */
int g_quit = 0;


void setStr(char *dst, const char *src, int size)
{
    if (size <= 0) return;
    strncpy(dst, src, (size_t)(size - 1));
    dst[size - 1] = '\0';
}

void nowStr(char *buf, int size)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt) strftime(buf, (size_t)size, "%Y-%m-%d %H:%M:%S", lt);
    else    setStr(buf, "1970-01-01 00:00:00", size);
}

Student *findStudent(const char *id)
{
    Student *p = g_stuHead;
    while (p) { if (strcmp(p->id, id) == 0) return p; p = p->next; }
    return NULL;
}

Teacher *findTeacher(const char *id)
{
    Teacher *p = g_teaHead;
    while (p) { if (strcmp(p->id, id) == 0) return p; p = p->next; }
    return NULL;
}

Course *findCourse(const char *id)
{
    int i;
    for (i = 0; i < g_courseCount; i++)
        if (strcmp(g_courses[i].id, id) == 0) return &g_courses[i];
    return NULL;
}

Grade *findGrade(Student *s, const char *courseId)
{
    int i;
    if (!s) return NULL;
    for (i = 0; i < s->gradeCount; i++)
        if (strcmp(s->grades[i].courseId, courseId) == 0) return &s->grades[i];
    return NULL;
}

void addGrade(Student *s, const char *courseId, float usual, float exam)
{
    Grade *g = findGrade(s, courseId);
    if (g) { g->usual = usual; g->exam = exam; return; }
    if (s->gradeCount >= MAX_GRADES) { coreMsg("  [提示] 该生选课数已达上限。\n"); return; }
    g = &s->grades[s->gradeCount++];
    setStr(g->courseId, courseId, sizeof(g->courseId));
    g->usual = usual;
    g->exam  = exam;
}

/* 总评成绩 = 平时成绩 * 40% + 考试成绩 * 60% */
float gradeTotal(const Grade *g)
{
    return g->usual * 0.4f + g->exam * 0.6f;
}

/* 百分制 -> 绩点（五分制/四分制常用换算） */
float scoreToPoint(float t)
{
    if (t >= 90.0f) return 4.0f;
    if (t >= 85.0f) return 3.7f;
    if (t >= 82.0f) return 3.3f;
    if (t >= 78.0f) return 3.0f;
    if (t >= 75.0f) return 2.7f;
    if (t >= 72.0f) return 2.3f;
    if (t >= 68.0f) return 2.0f;
    if (t >= 64.0f) return 1.5f;
    if (t >= 60.0f) return 1.0f;
    return 0.0f;
}

/* 某课程全体学生总评平均分 */
float courseAvgScore(const char *courseId)
{
    Student *p = g_stuHead;
    float sum = 0.0f; int n = 0;
    while (p) {
        Grade *g = findGrade(p, courseId);
        if (g) { sum += gradeTotal(g); n++; }
        p = p->next;
    }
    return n ? sum / (float)n : 0.0f;
}

int courseStudentCount(const char *courseId)
{
    Student *p = g_stuHead;
    int n = 0;
    while (p) { if (findGrade(p, courseId)) n++; p = p->next; }
    return n;
}

/* 教师所授全部课程的学生平均分（授课效果客观指标） */
float teacherAvgCourseScore(const Teacher *t)
{
    int i; float sum = 0.0f; int n = 0;
    for (i = 0; i < t->itemCount; i++) {
        sum += courseAvgScore(t->items[i].courseId);
        n++;
    }
    return n ? sum / (float)n : 0.0f;
}

/* 重算学生的加权平均分、学分绩点、出勤率、不良记录分值 */
void recalcStudent(Student *s)
{
    float sumCredit = 0.0f, sumWeight = 0.0f, sumPoint = 0.0f;
    int i;
    for (i = 0; i < s->gradeCount; i++) {
        Course *c = findCourse(s->grades[i].courseId);
        float credit = c ? c->credit : 1.0f;
        float total  = gradeTotal(&s->grades[i]);
        sumCredit += credit;
        sumWeight += total * credit;
        sumPoint  += scoreToPoint(total) * credit;
    }
    s->weighted = (sumCredit > 0.0f) ? sumWeight / sumCredit : 0.0f;
    s->gpa      = (sumCredit > 0.0f) ? sumPoint  / sumCredit : 0.0f;

    {
        /* 出勤率 = (出勤次数 + 迟到次数*0.5) / 总考勤次数 * 100% */
        float p = 0.0f;
        int total = s->attCount;
        for (i = 0; i < s->attCount; i++) {
            if (s->atts[i].status == 3)      p += 1.0f;
            else if (s->atts[i].status == 1) p += 0.5f;
        }
        s->attRate = total ? (p / (float)total) * 100.0f : 100.0f;
    }

    s->misScore = 0;
    for (i = 0; i < s->misCount; i++) s->misScore += s->mis[i].severity;
}

void recalcAll(void)
{
    Student *p = g_stuHead;
    while (p) { recalcStudent(p); p = p->next; }
}

Student *createStudent(const char *id, const char *name, const char *gender,
                              const char *cls, const char *phone, const char *email)
{
    Student *s;
    if (g_stuCount >= MAX_STUDENTS) { coreMsg("  [提示] 学生数量已达上限。\n"); return NULL; }
    s = (Student *)calloc(1, sizeof(Student));
    if (!s) { coreMsg("  [错误] 内存分配失败。\n"); return NULL; }
    setStr(s->id, id, sizeof(s->id));
    setStr(s->name, name, sizeof(s->name));
    setStr(s->gender, gender, sizeof(s->gender));
    setStr(s->className, cls, sizeof(s->className));
    setStr(s->phone, phone, sizeof(s->phone));
    setStr(s->email, email, sizeof(s->email));
    s->attRate = 100.0f;
    s->next = NULL;
    /* 头插法 O(1) */
    s->next = g_stuHead;
    g_stuHead = s;
    g_stuCount++;
    return s;
}

Teacher *createTeacher(const char *id, const char *name, const char *title,
                              const char *dept, const char *phone, const char *email)
{
    Teacher *t;
    if (g_teaCount >= MAX_TEACHERS) { coreMsg("  [提示] 教师数量已达上限。\n"); return NULL; }
    t = (Teacher *)calloc(1, sizeof(Teacher));
    if (!t) { coreMsg("  [错误] 内存分配失败。\n"); return NULL; }
    setStr(t->id, id, sizeof(t->id));
    setStr(t->name, name, sizeof(t->name));
    setStr(t->title, title, sizeof(t->title));
    setStr(t->dept, dept, sizeof(t->dept));
    setStr(t->phone, phone, sizeof(t->phone));
    setStr(t->email, email, sizeof(t->email));
    t->next = g_teaHead;
    g_teaHead = t;
    g_teaCount++;
    return t;
}

Course *createCourse(const char *id, const char *name, float credit, int hours, const char *term)
{
    Course *c;
    if (g_courseCount >= MAX_COURSES) { coreMsg("  [提示] 课程数量已达上限。\n"); return NULL; }
    c = &g_courses[g_courseCount++];
    setStr(c->id, id, sizeof(c->id));
    setStr(c->name, name, sizeof(c->name));
    c->credit = credit;
    c->hours  = hours;
    setStr(c->term, term, sizeof(c->term));
    return c;
}

/* 计算教师的平均评教分（每次任课信息变化后调用，保证 evalAvg 始终有效） */
void refreshTeacherEval(Teacher *t)
{
    int i;
    float sum = 0.0f;
    if (!t) return;
    for (i = 0; i < t->itemCount; i++) sum += t->items[i].evalScore;
    t->evalAvg = t->itemCount ? sum / (float)t->itemCount : 0.0f;
}

void addTeachItem(Teacher *t, const char *courseId, float eval)
{
    int i;
    if (!t) return;
    for (i = 0; i < t->itemCount; i++)
        if (strcmp(t->items[i].courseId, courseId) == 0) {
            t->items[i].evalScore = eval;
            refreshTeacherEval(t);
            return;
        }
    if (t->itemCount >= MAX_TC) { coreMsg("  [提示] 该教师任课数已达上限。\n"); return; }
    setStr(t->items[t->itemCount].courseId, courseId, sizeof(t->items[0].courseId));
    t->items[t->itemCount].evalScore = eval;
    t->itemCount++;
    refreshTeacherEval(t);
}

int removeTeacherCourse(Teacher *t, const char *courseId)
{
    int i, j;
    if (!t) return 0;
    for (i = 0; i < t->itemCount; i++) {
        if (strcmp(t->items[i].courseId, courseId) == 0) {
            for (j = i; j < t->itemCount - 1; j++) t->items[j] = t->items[j + 1];
            t->itemCount--;
            refreshTeacherEval(t);
            return 1;
        }
    }
    return 0;
}

void addHomework(const char *courseId, const char *studentId, const char *teacherId,
                        const char *title, float score, const char *comment, const char *date)
{
    int i;
    for (i = 0; i < g_hwCount; i++) {
        if (strcmp(g_hw[i].courseId, courseId) == 0 &&
            strcmp(g_hw[i].studentId, studentId) == 0 &&
            strcmp(g_hw[i].title, title) == 0) {
            g_hw[i].score = score;
            setStr(g_hw[i].comment, comment, sizeof(g_hw[i].comment));
            setStr(g_hw[i].date, date, sizeof(g_hw[i].date));
            setStr(g_hw[i].teacherId, teacherId, sizeof(g_hw[i].teacherId));
            return;
        }
    }
    if (g_hwCount >= MAX_HW) { coreMsg("  [提示] 作业记录已达上限。\n"); return; }
    g_hw[g_hwCount].id = g_hwNextId++;
    setStr(g_hw[g_hwCount].courseId, courseId, sizeof(g_hw[0].courseId));
    setStr(g_hw[g_hwCount].studentId, studentId, sizeof(g_hw[0].studentId));
    setStr(g_hw[g_hwCount].teacherId, teacherId, sizeof(g_hw[0].teacherId));
    setStr(g_hw[g_hwCount].title, title, sizeof(g_hw[0].title));
    g_hw[g_hwCount].score = score;
    setStr(g_hw[g_hwCount].comment, comment, sizeof(g_hw[0].comment));
    setStr(g_hw[g_hwCount].date, date, sizeof(g_hw[0].date));
    g_hwCount++;
}

/* 依据作业成绩自动折算该生该课程的平时成绩（作业平均分） */
void refreshUsualFromHomework(const char *studentId, const char *courseId)
{
    int i, n = 0;
    float sum = 0.0f;
    Student *s = findStudent(studentId);
    if (!s) return;
    for (i = 0; i < g_hwCount; i++)
        if (strcmp(g_hw[i].studentId, studentId) == 0 &&
            strcmp(g_hw[i].courseId, courseId) == 0) { sum += g_hw[i].score; n++; }
    if (n > 0) {
        Grade *g = findGrade(s, courseId);
        if (g) g->usual = sum / (float)n;
        else   addGrade(s, courseId, sum / (float)n, 0.0f);
        recalcStudent(s);
    }
}

void clearAllData(void)
{
    Student *s = g_stuHead;
    while (s) { Student *n = s->next; free(s); s = n; }
    g_stuHead = NULL; g_stuCount = 0;

    {
        Teacher *t = g_teaHead;
        while (t) { Teacher *n = t->next; free(t); t = n; }
        g_teaHead = NULL; g_teaCount = 0;
    }
    g_courseCount = 0;
    g_hwCount = 0;
    g_qaCount = 0;
    g_hwNextId = 1;
    notifyClear();
}

int rnd(int lo, int hi)
{
    g_seed = g_seed * 1103515245u + 12345u;
    return lo + (int)((g_seed >> 16) % (unsigned int)(hi - lo + 1));
}

float rndf(float lo, float hi)
{
    return lo + (float)(rnd(0, 1000)) / 1000.0f * (hi - lo);
}

void initSampleData(void)
{
    static const char *SURNAME[] = {
        "王","李","张","刘","陈","杨","赵","黄","周","吴","徐","孙","马","朱","胡",
        "郭","何","高","林","罗","郑","梁","谢","宋","唐","许","韩","冯","邓","曹"
    };
    static const char *GIVEN1[] = {
        "雨","子","小","雅","思","语","佳","明","浩","宇","欣","嘉","文","永","志",
        "国","建","海","晓","梦","晨","雪","伟","俊","舒"
    };
    static const char *GIVEN2[] = {
        "涵","轩","琪","萱","宸","睿","泽","彤","阳","然","豪","杰","婷","萌",
        "洋","博","宁","辰","颖","菲","伦","凯","楠","航","月"
    };
    static const char *ATTDATE[6] = {
        "2024-03-04","2024-03-11","2024-03-18","2024-03-25","2024-04-01","2024-04-08"
    };
    static const char *HWTITLE[4][2] = {
        { "高数作业一(极限与导数)", "高数作业二(积分应用)" },
        { "数据结构作业一(线性表)", "数据结构作业二(树与图)" },
        { "英语作业一(阅读理解)",   "英语作业二(写作训练)"   },
        { "计网作业一(体系结构)",   "计网作业二(TCP/IP协议)" }
    };

    Teacher *t1, *t2, *t3, *t4, *t5, *t6, *t7, *t8, *t9, *t10, *t11, *t12;
    int i, k;

    clearAllData();
    g_seed = 20240501u;

    /* ---------- 1. 课程（4 门） ---------- */
    createCourse("C001", "高等数学",   4.0f, 64, "2023-2024学年第二学期");
    createCourse("C002", "数据结构",   4.0f, 64, "2023-2024学年第二学期");
    createCourse("C003", "大学英语",   3.0f, 48, "2023-2024学年第二学期");
    createCourse("C004", "计算机网络", 3.5f, 56, "2023-2024学年第二学期");

    /* ---------- 2. 教师（12 名，每门课任课教师不少于 3 人） ---------- */
    t1  = createTeacher("T001", "张伟", "教授",   "数学学院",     "13800000001", "zhangwei@univ.edu.cn");
    t2  = createTeacher("T002", "李静", "副教授", "数学学院",     "13800000002", "lijing@univ.edu.cn");
    t3  = createTeacher("T003", "王强", "讲师",   "数学学院",     "13800000003", "wangqiang@univ.edu.cn");
    t4  = createTeacher("T004", "刘洋", "教授",   "计算机学院",   "13800000004", "liuyang@univ.edu.cn");
    t5  = createTeacher("T005", "陈晨", "副教授", "计算机学院",   "13800000005", "chenchen@univ.edu.cn");
    t6  = createTeacher("T006", "赵敏", "讲师",   "计算机学院",   "13800000006", "zhaomin@univ.edu.cn");
    t7  = createTeacher("T007", "孙丽", "教授",   "外国语学院",   "13800000007", "sunli@univ.edu.cn");
    t8  = createTeacher("T008", "周涛", "副教授", "外国语学院",   "13800000008", "zhoutao@univ.edu.cn");
    t9  = createTeacher("T009", "吴芳", "讲师",   "外国语学院",   "13800000009", "wufang@univ.edu.cn");
    t10 = createTeacher("T010", "郑凯", "副教授", "计算机学院",   "13800000010", "zhengkai@univ.edu.cn");
    t11 = createTeacher("T011", "冯磊", "讲师",   "计算机学院",   "13800000011", "fenglei@univ.edu.cn");
    t12 = createTeacher("T012", "何雪", "助教",   "计算机学院",   "13800000012", "hexue@univ.edu.cn");

    addTeachItem(t1,  "C001", 4.80f); addTeachItem(t1,  "C002", 4.60f);
    addTeachItem(t2,  "C001", 4.50f);
    addTeachItem(t3,  "C001", 4.20f);
    addTeachItem(t4,  "C002", 4.90f); addTeachItem(t4,  "C004", 4.70f);
    addTeachItem(t5,  "C002", 4.40f);
    addTeachItem(t6,  "C002", 4.10f);
    addTeachItem(t7,  "C003", 4.60f);
    addTeachItem(t8,  "C003", 4.30f);
    addTeachItem(t9,  "C003", 4.00f);
    addTeachItem(t10, "C004", 4.50f);
    addTeachItem(t11, "C004", 4.20f);
    addTeachItem(t12, "C001", 3.90f); addTeachItem(t12, "C003", 3.80f);

    /* ---------- 3. 学生（75 名，每门课程选课人数均为 75 > 70） ---------- */
    for (i = 0; i < 75; i++) {
        char id[16], name[32], phone[20], email[48], cls[24];
        const char *g = (i % 2 == 0) ? "男" : "女";
        sprintf(id, "Z2023%03d", i + 1);
        sprintf(name, "%s%s%s", SURNAME[rnd(0, 29)], GIVEN1[rnd(0, 24)], GIVEN2[rnd(0, 24)]);
        sprintf(phone, "139%08d", 10000000 + i * 137);
        sprintf(email, "stu%03d@univ.edu.cn", i + 1);
        sprintf(cls, "计算机230%d", (i < 38) ? 1 : 2);
        createStudent(id, name, g, cls, phone, email);
    }

    /* ---------- 4. 成绩 + 考勤 + 作业 ---------- */
    for (i = 0; i < g_courseCount; i++) {
        const char *cid = g_courses[i].id;
        Student *p = g_stuHead;
        while (p) {
            int base = rnd(0, 100);
            float usual = (float)rnd(65, 98);
            float exam;
            if (base < 8)       exam = rndf(38.0f, 59.0f);   /* 约 8% 不及格 */
            else if (base < 30) exam = rndf(60.0f, 74.0f);
            else if (base < 75) exam = rndf(75.0f, 89.0f);
            else                exam = rndf(90.0f, 99.0f);
            addGrade(p, cid, usual, exam);

            /* 考勤：6 次课，绝大多数正常出勤 */
            for (k = 0; k < 6; k++) {
                int st = 3, r = rnd(0, 99);
                if (r < 4)       st = 0;   /* 缺勤 */
                else if (r < 10) st = 1;   /* 迟到 */
                else if (r < 14) st = 2;   /* 请假 */
                if (p->attCount < MAX_ATT) {
                    setStr(p->atts[p->attCount].courseId, cid, sizeof(p->atts[0].courseId));
                    setStr(p->atts[p->attCount].date, ATTDATE[k], sizeof(p->atts[0].date));
                    p->atts[p->attCount].status = st;
                    p->attCount++;
                }
            }

            /* 作业：每门课 2 次，成绩略高于期末 */
            for (k = 0; k < 2; k++) {
                float sc = (float)rnd(60, 100);
                char date[12];
                sprintf(date, "2024-0%d-%02d", 3 + k, 10 + rnd(0, 15));
                addHomework(cid, p->id, (i == 0) ? "T001" : (i == 1) ? "T004" : (i == 2) ? "T007" : "T010",
                            HWTITLE[i][k], sc, (sc >= 90) ? "优秀" : (sc >= 75) ? "良好" : (sc >= 60) ? "合格" : "需订正", date);
            }
            p = p->next;
        }
    }

    /* 依据作业成绩刷新平时成绩 */
    {
        Student *p = g_stuHead;
        while (p) {
            for (i = 0; i < g_courseCount; i++) refreshUsualFromHomework(p->id, g_courses[i].id);
            p = p->next;
        }
    }

    /* ---------- 5. 不良记录（9 条） ---------- */
    {
        static const char *MTYPE[5] = { "警告", "严重警告", "记过", "留校察看", "开除学籍" };
        static const char *MDESC[5] = {
            "课堂多次迟到早退", "作业抄袭他人", "考试违纪(携带小抄)",
            "旷课累计达10学时", "严重违反校纪校规"
        };
        for (i = 0; i < 9; i++) {
            int idx = rnd(0, 74);
            char sid[16];
            Student *s;
            sprintf(sid, "Z2023%03d", idx + 1);
            s = findStudent(sid);
            if (!s) continue;
            {
                int sev = rnd(1, 4);
                if (s->misCount < MAX_MIS) {
                    setStr(s->mis[s->misCount].date, "2024-04-1", sizeof(s->mis[0].date));
                    setStr(s->mis[s->misCount].type, MTYPE[sev - 1], sizeof(s->mis[0].type));
                    s->mis[s->misCount].severity = sev;
                    setStr(s->mis[s->misCount].desc, MDESC[sev - 1], sizeof(s->mis[0].desc));
                    s->misCount++;
                }
            }
        }
    }

    /* ---------- 6. 获奖 / 竞赛 / 学术交流（30 条） ---------- */
    {
        static const char *HTYPE[3] = { "获奖", "竞赛", "学术交流" };
        static const char *HNAME[6] = {
            "全国大学生数学建模竞赛", "蓝桥杯程序设计大赛", "ACM-ICPC 区域赛",
            "校级三好学生", "中国大学生计算机设计大赛", "国际学术会议(学生报告)"
        };
        struct { char sid[16]; int t; int lv; int n; } H[30];
        for (i = 0; i < 30; i++) {
            int idx = rnd(0, 74);
            sprintf(H[i].sid, "Z2023%03d", idx + 1);
            H[i].t  = rnd(0, 2);
            H[i].lv = rnd(1, 4);
            H[i].n  = rnd(0, 5);
        }
        for (i = 0; i < 30; i++) {
            Student *s = findStudent(H[i].sid);
            if (!s) continue;
            if (s->honorCount < MAX_HONOR) {
                HonorRec *h = &s->honors[s->honorCount++];
                sprintf(h->date, "2024-0%d-%02d", rnd(1, 5), rnd(1, 28));
                setStr(h->name, HNAME[H[i].n], sizeof(h->name));
                setStr(h->type, HTYPE[H[i].t], sizeof(h->type));
                h->level = H[i].lv;
            }
        }
    }

    /* ---------- 7. 答疑辅导记录（24 条） ---------- */
    {
        static const char *Q[4] = {
            "老师，泰勒公式的余项怎么理解？",
            "链表和数组的适用场景有什么区别？",
            "英语作文如何提高逻辑连贯性？",
            "TCP 三次握手为什么不能是两次？"
        };
        static const char *A[4] = {
            "余项表示近似误差，可用拉格朗日型理解。",
            "频繁插入删除用链表，随机访问用数组。",
            "多用连接词并先列提纲，注意段落主题句。",
            "防止历史重复连接，需同步双方序号。"
        };
        for (i = 0; i < 24; i++) {
            int ci = rnd(0, 3), si = rnd(0, 74);
            if (g_qaCount < MAX_QA) {
                char sid[16];
                sprintf(sid, "Z2023%03d", si + 1);
                setStr(g_qa[g_qaCount].studentId, sid, sizeof(g_qa[0].studentId));
                setStr(g_qa[g_qaCount].courseId, g_courses[ci].id, sizeof(g_qa[0].courseId));
                setStr(g_qa[g_qaCount].teacherId,
                       (ci == 0) ? "T002" : (ci == 1) ? "T005" : (ci == 2) ? "T008" : "T010",
                       sizeof(g_qa[0].teacherId));
                sprintf(g_qa[g_qaCount].date, "2024-04-%02d", rnd(1, 28));
                setStr(g_qa[g_qaCount].question, Q[ci], sizeof(g_qa[0].question));
                setStr(g_qa[g_qaCount].answer, A[ci], sizeof(g_qa[0].answer));
                g_qaCount++;
            }
        }
    }

    recalcAll();

    coreMsg("\n  [完成] 示例数据已生成：\n");
    coreMsg("         课程 %d 门  |  教师 %d 名  |  学生 %d 名  |  作业记录 %d 条  |  答疑 %d 条\n",
           g_courseCount, g_teaCount, g_stuCount, g_hwCount, g_qaCount);
    coreMsg("         每门课程任课教师 >= 3 人，选课学生 75 人（>= 70 人），满足课程设计要求。\n");
}

/* 插入排序：小规模序列排序 / 快排的混合优化，O(n^2) 但常数极小 */
void insertionSortPtr(void **a, int lo, int hi, CmpFn cmp)
{
    int i;
    for (i = lo + 1; i <= hi; i++) {
        void *key = a[i];
        int j = i - 1;
        while (j >= lo && cmp(a[j], key) > 0) { a[j + 1] = a[j]; j--; }
        a[j + 1] = key;
    }
}

/* 快速排序：三数取中选枢轴 + 小区间插入排序 + 尾递归削减栈深，平均 O(n log n) */
void quickSortRec(void **a, int lo, int hi, CmpFn cmp)
{
    while (lo < hi) {
        int mid, i, j;
        void *pivot;
        if (hi - lo < 12) { insertionSortPtr(a, lo, hi, cmp); return; }

        mid = lo + (hi - lo) / 2;
        if (cmp(a[mid], a[lo]) < 0) SWAP(a[mid], a[lo]);
        if (cmp(a[hi],  a[lo]) < 0) SWAP(a[hi],  a[lo]);
        if (cmp(a[hi], a[mid]) < 0) SWAP(a[hi], a[mid]);
        SWAP(a[mid], a[hi - 1]);
        pivot = a[hi - 1];

        i = lo; j = hi - 1;
        for (;;) {
            while (cmp(a[++i], pivot) < 0) { }
            while (cmp(a[--j], pivot) > 0) { }
            if (i >= j) break;
            SWAP(a[i], a[j]);
        }
        SWAP(a[i], a[hi - 1]);

        /* 先递归较小的一半，循环处理较大的一半（尾递归优化） */
        if (i - lo < hi - i) { quickSortRec(a, lo, i - 1, cmp); lo = i + 1; }
        else                 { quickSortRec(a, i + 1, hi, cmp); hi = i - 1; }
    }
}

void sortPtrArray(void **arr, int n, CmpFn cmp)
{
    if (n > 1) quickSortRec(arr, 0, n - 1, cmp);
}

/* KMP 算法：返回 pat 首次出现在 text 中的下标，未找到返回 -1，O(n+m) */
int kmpIndex(const char *text, const char *pat)
{
    int n = (int)strlen(text), m = (int)strlen(pat);
    int *next, i, j, pos = -1;
    if (m == 0) return 0;
    if (n < m)  return -1;

    next = (int *)malloc(sizeof(int) * (size_t)m);
    if (!next) return -1;

    /* 构造 next 数组（部分匹配表） */
    next[0] = -1;
    i = 0; j = -1;
    while (i < m - 1) {
        if (j == -1 || pat[i] == pat[j]) { i++; j++; next[i] = j; }
        else j = next[j];
    }

    /* 匹配过程 */
    i = 0; j = 0;
    while (i < n && j < m) {
        if (j == -1 || text[i] == pat[j]) { i++; j++; }
        else j = next[j];
    }
    if (j >= m) pos = i - m;
    free(next);
    return pos;
}

/* 忽略大小写的子串包含判断（把两边都转小写后调用 KMP） */
int containsIgnoreCase(const char *text, const char *pat)
{
    char *t, *p;
    int r;
    size_t i, ln = strlen(text), lp = strlen(pat);
    t = (char *)malloc(ln + 1);
    p = (char *)malloc(lp + 1);
    if (!t || !p) { free(t); free(p); return 0; }
    for (i = 0; i < ln; i++) t[i] = (char)tolower((unsigned char)text[i]);
    for (i = 0; i < lp; i++) p[i] = (char)tolower((unsigned char)pat[i]);
    t[ln] = '\0'; p[lp] = '\0';
    r = (kmpIndex(t, p) >= 0);
    free(t); free(p);
    return r;
}

int cmpStuByIdAsc(const void *a, const void *b)
{ return strcmp(((const Student *)a)->id, ((const Student *)b)->id); }

int cmpStuByNameAsc(const void *a, const void *b)
{
    int r = strcmp(((const Student *)a)->name, ((const Student *)b)->name);
    return r ? r : strcmp(((const Student *)a)->id, ((const Student *)b)->id);
}

int cmpStuByClassAsc(const void *a, const void *b)
{
    int r = strcmp(((const Student *)a)->className, ((const Student *)b)->className);
    return r ? r : strcmp(((const Student *)a)->id, ((const Student *)b)->id);
}

/* 绩点降序（并列时按加权平均分、学号） */
int cmpStuByGpaDesc(const void *a, const void *b)
{
    const Student *x = (const Student *)a, *y = (const Student *)b;
    if (x->gpa < y->gpa) return 1;
    if (x->gpa > y->gpa) return -1;
    if (x->weighted < y->weighted) return 1;
    if (x->weighted > y->weighted) return -1;
    return strcmp(x->id, y->id);
}

int cmpStuByWeightedDesc(const void *a, const void *b)
{
    const Student *x = (const Student *)a, *y = (const Student *)b;
    if (x->weighted < y->weighted) return 1;
    if (x->weighted > y->weighted) return -1;
    return strcmp(x->id, y->id);
}

/* 出勤率升序：缺勤最严重的排在前面 */
int cmpStuByAttAsc(const void *a, const void *b)
{
    const Student *x = (const Student *)a, *y = (const Student *)b;
    if (x->attRate > y->attRate) return 1;
    if (x->attRate < y->attRate) return -1;
    return strcmp(x->id, y->id);
}

/* 不良记录严重程度降序 */
int cmpStuByMisDesc(const void *a, const void *b)
{
    const Student *x = (const Student *)a, *y = (const Student *)b;
    if (x->misScore != y->misScore) return (x->misScore < y->misScore) ? 1 : -1;
    if (x->misCount != y->misCount) return (x->misCount < y->misCount) ? 1 : -1;
    return strcmp(x->id, y->id);
}

int cmpTeaByIdAsc(const void *a, const void *b)
{ return strcmp(((const Teacher *)a)->id, ((const Teacher *)b)->id); }

int cmpTeaByEvalDesc(const void *a, const void *b)
{
    const Teacher *x = (const Teacher *)a, *y = (const Teacher *)b;
    if (x->evalAvg < y->evalAvg) return 1;
    if (x->evalAvg > y->evalAvg) return -1;
    return strcmp(x->id, y->id);
}

int cmpTeaByAvgScoreDesc(const void *a, const void *b)
{
    float sx = teacherAvgCourseScore((const Teacher *)a);
    float sy = teacherAvgCourseScore((const Teacher *)b);
    if (sx < sy) return 1;
    if (sx > sy) return -1;
    return strcmp(((const Teacher *)a)->id, ((const Teacher *)b)->id);
}

int cmpCourseByIdAsc(const void *a, const void *b)
{ return strcmp(((const Course *)a)->id, ((const Course *)b)->id); }

int cmpCourseByAvgDesc(const void *a, const void *b)
{
    float x = courseAvgScore(((const Course *)a)->id);
    float y = courseAvgScore(((const Course *)b)->id);
    if (x < y) return 1;
    if (x > y) return -1;
    return cmpCourseByIdAsc(a, b);
}

int cmpCourseByCountDesc(const void *a, const void *b)
{
    int x = courseStudentCount(((const Course *)a)->id);
    int y = courseStudentCount(((const Course *)b)->id);
    if (x != y) return (x < y) ? 1 : -1;
    return cmpCourseByIdAsc(a, b);
}

int cmpMisBySeverityDesc(const void *a, const void *b)
{
    const MisRec *x = (const MisRec *)a, *y = (const MisRec *)b;
    if (x->severity != y->severity) return (x->severity < y->severity) ? 1 : -1;
    return strcmp(y->date, x->date);
}

int cmpHonorByLevelDesc(const void *a, const void *b)
{
    const HonorRec *x = (const HonorRec *)a, *y = (const HonorRec *)b;
    if (x->level != y->level) return (x->level < y->level) ? 1 : -1;
    return strcmp(y->date, x->date);
}

int collectStudents(Student **out, int maxN)
{
    int n = 0;
    Student *p = g_stuHead;
    while (p && n < maxN) { out[n++] = p; p = p->next; }
    return n;
}

int collectTeachers(Teacher **out, int maxN)
{
    int n = 0;
    Teacher *p = g_teaHead;
    while (p && n < maxN) { out[n++] = p; p = p->next; }
    return n;
}

/* 中文状态名 */
const char *attStatusName(int st)
{
    switch (st) {
        case 0: return "缺勤";
        case 1: return "迟到";
        case 2: return "请假";
        default: return "出勤";
    }
}

const char *misLevelName(int sev)
{
    switch (sev) {
        case 1: return "轻微";
        case 2: return "一般";
        case 3: return "较重";
        case 4: return "严重";
        default: return "极其严重";
    }
}

const char *honorLevelName(int lv)
{
    switch (lv) {
        case 1: return "院级";
        case 2: return "校级";
        case 3: return "省级";
        default: return "国家级";
    }
}

void notifyEnqueue(char recvType, const char *recvId, const char *recvName,
                          const char *channel, const char *title, const char *content)
{
    static int notifyIdSeq = 1;
    Notify *n = (Notify *)calloc(1, sizeof(Notify));
    if (!n) { coreMsg("  [错误] 内存不足，通知入队失败。\n"); return; }
    n->id = notifyIdSeq++;
    n->recvType = recvType;
    setStr(n->recvId, recvId, sizeof(n->recvId));
    setStr(n->recvName, recvName, sizeof(n->recvName));
    setStr(n->channel, channel, sizeof(n->channel));
    setStr(n->title, title, sizeof(n->title));
    setStr(n->content, content, sizeof(n->content));
    nowStr(n->timeStr, sizeof(n->timeStr));
    n->next = NULL;

    /* 链式队列入队：尾插 O(1) */
    if (g_nqTail) g_nqTail->next = n;
    else          g_nqHead = n;
    g_nqTail = n;
    g_nqCount++;
}

/* 模拟发送队首通知（出队），并压入“已发送栈” */
int notifySendOne(void)
{
    Notify *n = g_nqHead;
    if (!n) return 0;
    g_nqHead = n->next;
    if (!g_nqHead) g_nqTail = NULL;
    g_nqCount--;

    {
        char block[1024];
        char sep[80];
        char contact[64] = "";
        int i;
        if (n->recvType == 'S') {
            Student *s = findStudent(n->recvId);
            if (s) setStr(contact, (strcmp(n->channel, "邮件") == 0) ? s->email : s->phone, sizeof(contact));
        } else {
            Teacher *t = findTeacher(n->recvId);
            if (t) setStr(contact, (strcmp(n->channel, "邮件") == 0) ? t->email : t->phone, sizeof(contact));
        }
        for (i = 0; i < 76; i++) sep[i] = '=';
        sep[76] = '\0';
        snprintf(block, sizeof(block),
                 "\n%s\n  [模拟%s发送]  编号 #%d\n  接收人: %s (%s %s)\n  %s: %s\n"
                 "  标题: %s\n  内容: %s\n  时间: %s\n%s\n",
                 sep, n->channel, n->id, n->recvName,
                 (n->recvType == 'S') ? "学生" : "教师", n->recvId,
                 (strcmp(n->channel, "邮件") == 0) ? "邮箱" : "手机号", contact,
                 n->title, n->content, n->timeStr, sep);
        block[sizeof(block) - 1] = '\0';
        coreMsg("%s", block);
    }

    /* 压栈（后进先出，便于撤销） */
    n->next = g_sentTop;
    g_sentTop = n;
    g_sentCount++;
    return 1;
}

int notifySendAll(void)
{
    int c = 0;
    while (notifySendOne()) c++;
    if (c == 0) coreMsg("\n  [提示] 待发送队列为空。\n");
    else coreMsg("\n  [完成] 共模拟发送 %d 条通知。\n", c);
    return c;
}

int notifyUndoLast(void)
{
    Notify *n = g_sentTop;
    if (!n) { coreMsg("\n  [提示] 没有可撤销的发送记录。\n"); return 0; }
    g_sentTop = n->next;
    g_sentCount--;
    coreMsg("\n  [撤销] 已撤回最近一次发送：#%d -> %s  <%s>\n", n->id, n->recvName, n->title);
    free(n);
    return 1;
}

void notifyClear(void)
{
    Notify *p = g_nqHead;
    while (p) { Notify *n = p->next; free(p); p = n; }
    g_nqHead = g_nqTail = NULL; g_nqCount = 0;
    p = g_sentTop;
    while (p) { Notify *n = p->next; free(p); p = n; }
    g_sentTop = NULL; g_sentCount = 0;
}

void notifyToStudentsOfCourse(const char *courseId, const char *channel,
                                     const char *title, const char *content)
{
    Student *s = g_stuHead;
    int cnt = 0;
    while (s) {
        if (findGrade(s, courseId)) {
            notifyEnqueue('S', s->id, s->name, channel, title, content);
            cnt++;
        }
        s = s->next;
    }
    coreMsg("  [完成] 已为课程 %s 的 %d 名学生生成通知（%s），进入待发送队列。\n", courseId, cnt, channel);
}

/* 按 '|' 拆分行（保留空字段） */
int splitLine(char *line, char *fields[], int maxFields)
{
    int n = 0;
    char *p = line;
    if (!line) return 0;
    fields[n++] = p;
    while (*p && n < maxFields) {
        if (*p == '|') { *p = '\0'; fields[n++] = p + 1; }
        p++;
    }
    return n;
}

int saveData(void)
{
    FILE *fp = fopen(DATA_FILE, "w");
    int i;
    if (!fp) { coreMsg("  [错误] 无法写入文件 %s\n", DATA_FILE); return 0; }

    fprintf(fp, "# 智慧教学管理系统数据文件 (Smart Teaching Management System)\n");
    fprintf(fp, "# 字段以 '|' 分隔，请勿手工修改字段顺序\n");

    for (i = 0; i < g_courseCount; i++)
        fprintf(fp, "C|%s|%s|%.2f|%d|%s\n", g_courses[i].id, g_courses[i].name,
                g_courses[i].credit, g_courses[i].hours, g_courses[i].term);

    {
        Teacher *t = g_teaHead;
        while (t) {
            fprintf(fp, "T|%s|%s|%s|%s|%s|%s\n", t->id, t->name, t->title, t->dept, t->phone, t->email);
            t = t->next;
        }
        t = g_teaHead;
        while (t) {
            for (i = 0; i < t->itemCount; i++)
                fprintf(fp, "TC|%s|%s|%.2f\n", t->id, t->items[i].courseId, t->items[i].evalScore);
            t = t->next;
        }
    }

    {
        Student *s = g_stuHead;
        while (s) {
            fprintf(fp, "S|%s|%s|%s|%s|%s|%s\n", s->id, s->name, s->gender, s->className, s->phone, s->email);
            for (i = 0; i < s->gradeCount; i++)
                fprintf(fp, "G|%s|%s|%.2f|%.2f\n", s->id, s->grades[i].courseId,
                        s->grades[i].usual, s->grades[i].exam);
            for (i = 0; i < s->attCount; i++)
                fprintf(fp, "A|%s|%s|%s|%d\n", s->id, s->atts[i].courseId,
                        s->atts[i].date, s->atts[i].status);
            for (i = 0; i < s->misCount; i++)
                fprintf(fp, "M|%s|%s|%s|%d|%s\n", s->id, s->mis[i].date,
                        s->mis[i].type, s->mis[i].severity, s->mis[i].desc);
            for (i = 0; i < s->honorCount; i++)
                fprintf(fp, "H|%s|%s|%s|%s|%d\n", s->id, s->honors[i].date,
                        s->honors[i].name, s->honors[i].type, s->honors[i].level);
            s = s->next;
        }
    }

    for (i = 0; i < g_hwCount; i++)
        fprintf(fp, "W|%d|%s|%s|%s|%s|%.2f|%s|%s\n", g_hw[i].id, g_hw[i].courseId,
                g_hw[i].studentId, g_hw[i].teacherId, g_hw[i].title, g_hw[i].score,
                g_hw[i].comment, g_hw[i].date);

    for (i = 0; i < g_qaCount; i++)
        fprintf(fp, "Q|%s|%s|%s|%s|%s|%s\n", g_qa[i].studentId, g_qa[i].teacherId,
                g_qa[i].courseId, g_qa[i].date, g_qa[i].question, g_qa[i].answer);

    fclose(fp);
    coreMsg("  [完成] 数据已保存至 %s（课程 %d 门 / 教师 %d 名 / 学生 %d 名 / 作业 %d 条 / 答疑 %d 条）\n",
           DATA_FILE, g_courseCount, g_teaCount, g_stuCount, g_hwCount, g_qaCount);
    return 1;
}

int loadData(void)
{
    FILE *fp = fopen(DATA_FILE, "r");
    char line[1024];
    int loaded = 0;
    if (!fp) { coreMsg("  [提示] 未找到数据文件 %s，请先保存或生成示例数据。\n", DATA_FILE); return 0; }

    clearAllData();

    while (fgets(line, sizeof(line), fp)) {
        char *f[16];
        int n;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;
        n = splitLine(line, f, 16);
        if (n < 1) continue;

        if (strcmp(f[0], "C") == 0 && n >= 6) {
            createCourse(f[1], f[2], (float)atof(f[3]), atoi(f[4]), f[5]);
        } else if (strcmp(f[0], "T") == 0 && n >= 7) {
            createTeacher(f[1], f[2], f[3], f[4], f[5], f[6]);
        } else if (strcmp(f[0], "TC") == 0 && n >= 4) {
            Teacher *t = findTeacher(f[1]);
            if (t) addTeachItem(t, f[2], (float)atof(f[3]));
        } else if (strcmp(f[0], "S") == 0 && n >= 7) {
            createStudent(f[1], f[2], f[3], f[4], f[5], f[6]);
        } else if (strcmp(f[0], "G") == 0 && n >= 5) {
            Student *s = findStudent(f[1]);
            if (s) addGrade(s, f[2], (float)atof(f[3]), (float)atof(f[4]));
        } else if (strcmp(f[0], "A") == 0 && n >= 5) {
            Student *s = findStudent(f[1]);
            if (s && s->attCount < MAX_ATT) {
                AttRec *a = &s->atts[s->attCount++];
                setStr(a->courseId, f[2], sizeof(a->courseId));
                setStr(a->date, f[3], sizeof(a->date));
                a->status = atoi(f[4]);
            }
        } else if (strcmp(f[0], "M") == 0 && n >= 6) {
            Student *s = findStudent(f[1]);
            if (s && s->misCount < MAX_MIS) {
                MisRec *m = &s->mis[s->misCount++];
                setStr(m->date, f[2], sizeof(m->date));
                setStr(m->type, f[3], sizeof(m->type));
                m->severity = atoi(f[4]);
                setStr(m->desc, f[5], sizeof(m->desc));
            }
        } else if (strcmp(f[0], "H") == 0 && n >= 6) {
            Student *s = findStudent(f[1]);
            if (s && s->honorCount < MAX_HONOR) {
                HonorRec *h = &s->honors[s->honorCount++];
                setStr(h->date, f[2], sizeof(h->date));
                setStr(h->name, f[3], sizeof(h->name));
                setStr(h->type, f[4], sizeof(h->type));
                h->level = atoi(f[5]);
            }
        } else if (strcmp(f[0], "W") == 0 && n >= 9) {
            if (g_hwCount < MAX_HW) {
                Homework *w = &g_hw[g_hwCount++];
                w->id = atoi(f[1]);
                if (w->id >= g_hwNextId) g_hwNextId = w->id + 1;
                setStr(w->courseId, f[2], sizeof(w->courseId));
                setStr(w->studentId, f[3], sizeof(w->studentId));
                setStr(w->teacherId, f[4], sizeof(w->teacherId));
                setStr(w->title, f[5], sizeof(w->title));
                w->score = (float)atof(f[6]);
                setStr(w->comment, f[7], sizeof(w->comment));
                setStr(w->date, f[8], sizeof(w->date));
            }
        } else if (strcmp(f[0], "Q") == 0 && n >= 7) {
            if (g_qaCount < MAX_QA) {
                QaRec *q = &g_qa[g_qaCount++];
                setStr(q->studentId, f[1], sizeof(q->studentId));
                setStr(q->teacherId, f[2], sizeof(q->teacherId));
                setStr(q->courseId, f[3], sizeof(q->courseId));
                setStr(q->date, f[4], sizeof(q->date));
                setStr(q->question, f[5], sizeof(q->question));
                setStr(q->answer, f[6], sizeof(q->answer));
            }
        }
        loaded++;
    }
    fclose(fp);

    /* 载入后统一重算教师平均评教分与学生统计指标 */
    {
        Teacher *t = g_teaHead;
        while (t) { refreshTeacherEval(t); t = t->next; }
    }
    recalcAll();
    coreMsg("  [完成] 数据已从 %s 载入：课程 %d 门 / 教师 %d 名 / 学生 %d 名 / 作业 %d 条 / 答疑 %d 条\n",
           DATA_FILE, g_courseCount, g_teaCount, g_stuCount, g_hwCount, g_qaCount);
    return 1;
}

