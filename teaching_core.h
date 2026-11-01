/* =============================================================================
 *  teaching_core.h  —— 智慧教学管理系统【数据层】接口
 * -----------------------------------------------------------------------------
 *  本文件只描述“数据 + 算法”，不含任何界面代码：
 *      结构体定义、全局数据表、排序/查找/统计/存档/通知队列等函数原型。
 *  任何界面（控制台 console_main.c、图形界面 gui_main.c）都可链接本层复用。
 *
 *  分层关系：
 *      teaching_core.h/.c   数据层：学生/教师/课程/成绩/考勤/奖惩/通知 + 算法
 *      console_main.c       表现层：控制台菜单（等价于原单文件版界面）
 *      gui_main.c           表现层：Win32 图形界面
 *      smart_teaching.c     单文件版（数据层+控制台界面层合并，便于直接提交）
 * ========================================================================== */
#ifndef TEACHING_CORE_H
#define TEACHING_CORE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #define MKDIR(p) mkdir(p, 0755)
#endif

/* ------------------------------- 容量常量 ------------------------------- */
#define MAX_COURSES   12      /* 课程数量上限                     */
#define MAX_TEACHERS  60      /* 教师数量上限                     */
#define MAX_STUDENTS  400     /* 学生数量上限                     */
#define MAX_GRADES    12      /* 每个学生最多选课数               */
#define MAX_ATT       80      /* 每个学生最多考勤记录数           */
#define MAX_MIS       10      /* 每个学生最多不良记录数           */
#define MAX_HONOR     12      /* 每个学生最多获奖/竞赛记录数      */
#define MAX_TC        8       /* 每位教师最多任课数               */
#define MAX_HW        1200    /* 作业记录上限                     */
#define MAX_QA        400     /* 答疑记录上限                     */
#define DATA_FILE     "teaching_data.txt"   /* 数据存档文件名        */

/* ------------------------------- 数据结构 ------------------------------- */

/* 成绩（学生 - 课程） */
typedef struct {
    char  courseId[16];
    float usual;          /* 平时成绩（由作业成绩自动折算） */
    float exam;           /* 期末考试成绩                   */
} Grade;

/* 考勤记录 */
typedef struct {
    char courseId[16];
    char date[12];
    int  status;          /* 0缺勤 1迟到 2请假 3出勤 */
} AttRec;

/* 不良记录 */
typedef struct {
    char date[12];
    char type[24];        /* 警告 / 严重警告 / 记过 / 留校察看 / 开除学籍 */
    int  severity;        /* 严重程度 1~5 */
    char desc[64];
} MisRec;

/* 获奖 / 竞赛 / 学术交流记录 */
typedef struct {
    char date[12];
    char name[48];
    char type[16];        /* 获奖 / 竞赛 / 学术交流 */
    int  level;           /* 1院级 2校级 3省级 4国家级 */
} HonorRec;

/* 学生结点（单链表结点） */
typedef struct Student {
    char id[16];
    char name[40];
    char gender[8];
    char className[32];
    char phone[20];
    char email[48];

    Grade    grades[MAX_GRADES];  int gradeCount;
    AttRec   atts[MAX_ATT];       int attCount;
    MisRec   mis[MAX_MIS];        int misCount;
    HonorRec honors[MAX_HONOR];   int honorCount;

    float gpa;            /* 平均学分绩点   */
    float weighted;       /* 加权平均分     */
    float attRate;        /* 出勤率(%)      */
    int   misScore;       /* 不良记录严重程度合计 */

    struct Student *next;
} Student;

/* 教师任课信息 */
typedef struct {
    char  courseId[16];
    float evalScore;      /* 该课程的授课效果评价分(学生评教 0~5) */
} TeachItem;

/* 教师结点（单链表结点） */
typedef struct Teacher {
    char id[16];
    char name[32];
    char title[16];
    char dept[32];
    char phone[20];
    char email[48];

    TeachItem items[MAX_TC]; int itemCount;
    float evalAvg;        /* 平均评教分 */

    struct Teacher *next;
} Teacher;

/* 课程（顺序表元素） */
typedef struct {
    char  id[16];
    char  name[40];
    float credit;
    int   hours;
    char  term[40];
} Course;

/* 作业批改记录 */
typedef struct {
    int   id;
    char  courseId[16];
    char  studentId[16];
    char  teacherId[16];
    char  title[48];
    float score;
    char  comment[64];
    char  date[12];
} Homework;

/* 答疑/辅导记录 */
typedef struct {
    char studentId[16];
    char teacherId[16];
    char courseId[16];
    char date[12];
    char question[80];
    char answer[80];
} QaRec;

/* 通知结点（链式队列 / 链式栈通用结点） */
typedef struct Notify {
    int  id;
    char recvType;        /* 'S' 学生  'T' 教师 */
    char recvId[16];
    char recvName[32];
    char channel[12];     /* 短信 / 邮件 */
    char title[64];
    char content[192];
    char timeStr[24];
    struct Notify *next;
} Notify;

typedef int (*CmpFn)(const void *, const void *);

#define SWAP(a, b) do { void *_t = (a); (a) = (b); (b) = _t; } while (0)


/* ------------------------------- 全局数据表 -------------------------------
 * 定义在 teaching_core.c，界面层通过 extern 读取（遍历/展示）             */
extern Course   g_courses[MAX_COURSES];  extern int g_courseCount;
extern Student *g_stuHead;               extern int g_stuCount;
extern Teacher *g_teaHead;               extern int g_teaCount;
extern Homework g_hw[MAX_HW];            extern int g_hwCount;
extern QaRec    g_qa[MAX_QA];            extern int g_qaCount;
extern int      g_hwNextId;

/* 模拟通知：待发送队列(链式队列) + 已发送记录(链式栈) */
extern Notify  *g_nqHead, *g_nqTail;     extern int g_nqCount;
extern Notify  *g_sentTop;               extern int g_sentCount;

extern unsigned int g_seed;
extern int g_quit;

/* ------------------------- 消息输出钩子（界面无关） -------------------------
 * 数据层不直接 printf：核心层通过 coreMsg() 输出提示信息。
 *   - 未设置处理器时：默认打印到标准输出（控制台版行为不变）
 *   - 图形界面版用 coreSetMessageHandler() 把消息追加到界面日志框        */
typedef void (*CoreMsgFn)(const char *text);
void coreSetMessageHandler(CoreMsgFn fn);
void coreMsg(const char *fmt, ...);

/* --------------------------- 数据层函数接口 --------------------------- */
const char *attStatusName(int st);
const char *honorLevelName(int lv);
const char *misLevelName(int sev);
Course *createCourse(const char *id, const char *name, float credit, int hours, const char *term);
Course *findCourse(const char *id);
float courseAvgScore(const char *courseId);
float gradeTotal(const Grade *g);
float rndf(float lo, float hi);
float scoreToPoint(float t);
float teacherAvgCourseScore(const Teacher *t);
Grade *findGrade(Student *s, const char *courseId);
int cmpCourseByAvgDesc(const void *a, const void *b);
int cmpCourseByCountDesc(const void *a, const void *b);
int cmpCourseByIdAsc(const void *a, const void *b);
int cmpHonorByLevelDesc(const void *a, const void *b);
int cmpMisBySeverityDesc(const void *a, const void *b);
int cmpStuByAttAsc(const void *a, const void *b);
int cmpStuByClassAsc(const void *a, const void *b);
int cmpStuByGpaDesc(const void *a, const void *b);
int cmpStuByIdAsc(const void *a, const void *b);
int cmpStuByMisDesc(const void *a, const void *b);
int cmpStuByNameAsc(const void *a, const void *b);
int cmpStuByWeightedDesc(const void *a, const void *b);
int cmpTeaByAvgScoreDesc(const void *a, const void *b);
int cmpTeaByEvalDesc(const void *a, const void *b);
int cmpTeaByIdAsc(const void *a, const void *b);
int collectStudents(Student **out, int maxN);
int collectTeachers(Teacher **out, int maxN);
int containsIgnoreCase(const char *text, const char *pat);
int courseStudentCount(const char *courseId);
int kmpIndex(const char *text, const char *pat);
int loadData(void);
int notifySendAll(void);
int notifySendOne(void);
int notifyUndoLast(void);
int removeTeacherCourse(Teacher *t, const char *courseId);
int rnd(int lo, int hi);
int saveData(void);
int splitLine(char *line, char *fields[], int maxFields);
Student *createStudent(const char *id, const char *name, const char *gender,                               const char *cls, const char *phone, const char *email);
Student *findStudent(const char *id);
Teacher *createTeacher(const char *id, const char *name, const char *title,                               const char *dept, const char *phone, const char *email);
Teacher *findTeacher(const char *id);
void addGrade(Student *s, const char *courseId, float usual, float exam);
void addHomework(const char *courseId, const char *studentId, const char *teacherId,                         const char *title, float score, const char *comment, const char *date);
void addTeachItem(Teacher *t, const char *courseId, float eval);
void clearAllData(void);
void initSampleData(void);
void insertionSortPtr(void **a, int lo, int hi, CmpFn cmp);
void notifyClear(void);
void notifyEnqueue(char recvType, const char *recvId, const char *recvName,                           const char *channel, const char *title, const char *content);
void notifyToStudentsOfCourse(const char *courseId, const char *channel,                                      const char *title, const char *content);
void nowStr(char *buf, int size);
void quickSortRec(void **a, int lo, int hi, CmpFn cmp);
void recalcAll(void);
void recalcStudent(Student *s);
void refreshTeacherEval(Teacher *t);
void refreshUsualFromHomework(const char *studentId, const char *courseId);
void setStr(char *dst, const char *src, int size);
void sortPtrArray(void **arr, int n, CmpFn cmp);

#endif /* TEACHING_CORE_H */
