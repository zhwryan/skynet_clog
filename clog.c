#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>

#include "skynet_malloc.h"
#include "lauxlib.h"

#define SZ_LONG_64 (64)
#define SZ_LONG_128 (SZ_LONG_64*2)
#define SZ_LONG_256 (SZ_LONG_128*2)
#define ONE_MB (1024*1024)

#define DFL_FTIME (3) // 默认刷新时间
#define SZ_DFL_FILE (1024*ONE_MB) // 默认文件大小

#define SZ_LOGGERS (1000) // logger可用的最大ID
#define SZ_ONE_LOG (10*1024) // 单条LOG最长4K
#define SZ_FILE_BFF (10*ONE_MB) // 缓冲区大小

#define RETURN_CLOGERR_SOME(msg, errno, rtr) {throwError((msg), (errno));return (rtr);}
#define RETURN_CLOGERR(msg, errno) {throwError((msg), (errno));return;}
#define VALID_CLOGFILE(file) (file != NULL && file != stdout) // 检查文件句柄

static char _CLOG_INITED = 0;
static const char * levelnames[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL", NULL};
enum logger_level {DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, FATAL = 4};

struct ClogContext {
    int ftime; // 异步写文件cd,单元秒
    int utcHour; // int(time(NULL)/3600)
    int tm_hour; // 当前小时
    int running; // 线程内使用
    char date[SZ_LONG_64];
    pthread_t thread;
    pthread_mutex_t mutex;
} context;

struct Logger {
    FILE* file; // 日志文件句柄
    int level; // 打印级别
    int fsize; // 单文件大小
    char cuthour; // 是否小时切割文件,1:true,0:false
    int bwrited; // 已写入缓冲区大小
    int fwrited; // 已写入文件大小
    int filen; // 日志文件索引
    char fname[SZ_LONG_64]; // 文件名
    char dir[SZ_LONG_128]; // 目录名
    char filepath[SZ_LONG_256]; // 全路径
};

// 关于指针的判断,不必所有用处都判断,关注入口和并发逻辑即可
struct Logger* loggers[SZ_LOGGERS] = {NULL};

static inline void
closeFile(struct Logger* logger) {
    if (VALID_CLOGFILE(logger->file)) {
        fflush(logger->file);
        fclose(logger->file);
    }
    logger->file = stdout;
}

static inline void
closeAllFiles() {
    for (int i = 0; loggers[i] != NULL; ++i)
        closeFile(loggers[i]);
}

static inline void
throwError(const char* msg, int* saved_errno) {
    fprintf(stderr, "%s error[%d]: %s\n", msg, *saved_errno, strerror(*saved_errno));
    closeAllFiles();
}

static inline void
pathfill(char* filepath, int index, struct Logger* logger) {
    if (filepath == NULL)
        RETURN_CLOGERR("filepath fill filepath err", NULL);

    struct tm tm;
    time_t now = time(NULL);
    localtime_r(&now, &tm);

    char date[SZ_LONG_64];
    if (logger->cuthour) {
        strftime(date, SZ_LONG_64, "%Y%m%d-%H", &tm);
        snprintf(filepath, SZ_LONG_256, "%s/%s-%s.log", logger->dir, logger->fname, date);
    } else if (index >= 0) {
        strftime(date, SZ_LONG_64, "%y%m%d", &tm);
        snprintf(filepath, SZ_LONG_256, "%s/%s.log.%s.%d", logger->dir, logger->fname, date, index);
    } else
        RETURN_CLOGERR("filepath fill index err", NULL);
}

static inline char
newDir(const char* strdir, int mode) {
    if (0 == access(strdir, F_OK)) return 1;

    int len = strlen(strdir);
    char temp[len];
    strncpy(temp, strdir, len);

    for (int i = 1; i < len; i++) { // i=1,因为首字符可能是/
        if (temp[i] != '/') continue;

        temp[i] = '\0';
        if (0 == access(temp, F_OK) || 0 == mkdir(temp, mode)) {
            temp[i] = '/';
            continue;
        }

        printf("for new dir error %s\n", temp);
        RETURN_CLOGERR_SOME("for new dir", &errno, 0);
    }

    if (0 != mkdir(strdir, mode)) {
        printf("new dir error %s\n", strdir);
        RETURN_CLOGERR_SOME("new dir", &errno, 0);
    }

    return 1;
}

static inline void
newFile(struct Logger* logger) {
    if (VALID_CLOGFILE(logger->file))
        logger->file = freopen(logger->filepath, "a+", logger->file);
    else
        logger->file = fopen(logger->filepath, "a+");

    if (luai_unlikely(logger->file == NULL)) {
        if (errno != ENOENT)
            RETURN_CLOGERR("new file", &errno);

        if (!newDir(logger->dir, 0755)) return;

        logger->file = fopen(logger->filepath, "a+");
        if (luai_unlikely(logger->file == NULL))
            RETURN_CLOGERR("new file fopen", &errno);
    }

    setvbuf(logger->file, NULL, _IOFBF, SZ_FILE_BFF);
    logger->filen++;
    logger->bwrited = 0;
    logger->fwrited = 0;
}

static inline void // 按小时切割时不能调用
rollFileSize(struct Logger* logger) {
    closeFile(logger);

    char oldfile[SZ_LONG_128];
    char newfile[SZ_LONG_128];
    for (int i = logger->filen; i > 0; --i) {
        pathfill(oldfile, i-1, logger);
        pathfill(newfile, i, logger);
        rename(oldfile, newfile);
    }

    pathfill(logger->filepath, 0, logger);
    newFile(logger);
}

static inline char
initFileHandle(struct Logger* logger) {
    if (logger->cuthour) {
        pathfill(logger->filepath, context.tm_hour, logger);
        newFile(logger);
    } else {
        char filepath[SZ_LONG_256];
        for (logger->filen = 0; logger->filen < 100000; logger->filen++) {
            pathfill(filepath, logger->filen, logger);
            if (0 != access(filepath, F_OK)) break;
        }
        rollFileSize(logger);
    }
    return 1;
}

static inline const char*
strLevel(int* level) {
    const char* strlv = levelnames[*level];
    return strlv ? strlv : "NULL";
}

static inline void
tryRollFile(time_t* logtime) {
    int utcHour = ceil(time(NULL)/3600);
    if (utcHour == context.utcHour) return;

    struct tm* local = localtime(logtime);
    if (NULL == local) return;

    context.utcHour = utcHour;
    context.tm_hour = local->tm_hour; // 0-23

    for (int i = 0; loggers[i] != NULL; i++) {
        struct Logger* logger = loggers[i];
        if (logger->cuthour || context.tm_hour == 0) {
            pathfill(logger->filepath, context.tm_hour, logger);
            newFile(logger);
        }
    }
}

static inline void
outputf(int logId, time_t* logtime, const char *fmt, ...) {
    struct Logger* logger = loggers[logId-1];
    if (NULL == logger || NULL == logtime || NULL == fmt) return;

    va_list args;
    va_start(args, fmt);
    if (VALID_CLOGFILE(logger->file)) {
        tryRollFile(logtime);
        logger->bwrited += vfprintf(logger->file, fmt, args);
    } else
        vprintf(fmt, args);
    va_end(args);
}

static inline void* // 日志线程处理函数
threading(void* p) {
    context.running = 1;
    while (context.running) {
        for (int i = 0; loggers[i] != NULL; ++i) {
            struct Logger* logger = loggers[i];
            if (logger->bwrited <= 0) continue;

            pthread_mutex_lock(&context.mutex);
            if (luai_unlikely(0 != access(logger->filepath, F_OK)))
                newFile(logger);

            if (VALID_CLOGFILE(logger->file)) {
                fflush(logger->file);
                logger->fwrited += logger->bwrited;
                logger->bwrited = 0;

                if (!logger->cuthour && logger->fwrited >= logger->fsize)
                    rollFileSize(logger);
            }
            pthread_mutex_unlock(&context.mutex);
        }
        sleep(context.ftime);
    }

    closeAllFiles();
    return NULL;
}

static inline int // 从表中取出string字段
loadStringField(lua_State* L, const char* key, char* loader, int sz) {
    lua_getfield(L, -1, key); // 取值并压栈

    size_t size;
    const char* str = lua_tolstring(L, -1, &size);
    if (str == NULL || size <= 0) return 0;

    strncpy(loader, str, sz);
    lua_pop(L, 1);

    return 1;
}

static inline int // 从表中取出int字段
loadIntField(lua_State* L, const char* key) {
    lua_getfield(L, -1, key); // 取值并压栈
    int val = lua_tointeger(L, -1);
    lua_pop(L, 1); // 弹出栈顶
    return val;
}

static inline void
initLoggers(lua_State* L) {
    for (int i = 0; loggers[i] == NULL; ++i) {
        int logId = i + 1; // i+1因lua表从1开始
        int loggerType = lua_rawgeti(L, 2, logId); // 取值并压栈
        if (LUA_TTABLE != loggerType) return;

        struct Logger *logger = skynet_malloc(sizeof(struct Logger));
        memset(logger, 0, sizeof(struct Logger));

        logger->level = loadIntField(L, "level"); // 日志等级
        if (loadStringField(L, "dir", logger->dir, sizeof(logger->dir))) { // 目录名
            logger->fsize = loadIntField(L, "fsize"); // 文件大小
            logger->fsize = logger->fsize > 0 ? (logger->fsize * ONE_MB) : SZ_DFL_FILE;
            logger->cuthour = (char)loadIntField(L, "cuthour"); // 按小时切割

            if (!loadStringField(L, "fname", logger->fname, sizeof(logger->fname)) || // 日志名
                !newDir(logger->dir, 0755) || // 创建目录
                !initFileHandle(logger))
            {
                skynet_free(logger);
                return;
            }

            printf("init logger %s/%s level(%d) fsize(%d B) ftime(%d s) logId(%d)\n",
                logger->dir, logger->fname, logger->level, logger->fsize, context.ftime, logId);
        } else {
            logger->file = stdout;
            printf("init logger Console logId(%d)\n", logId);
        }

        loggers[i] = logger;
    }
}

static inline const char*
concatParams(lua_State* L, short begin, int limit) {
    size_t size = 0;
    int n = lua_gettop(L);
    if (luai_unlikely(n < begin)) return NULL;

    luaL_Buffer b;
    luaL_buffinit(L, &b);

    size_t len;
    const char* str;
    for (register int i = begin; i <= n; i++) {
        str = luaL_tolstring(L, i, &len);
        if (str == NULL || len <= 0) break;

        luaL_addvalue(&b);
        size += len;

        if (limit > 0 && size >= limit) break;

        if (i < n) {
            luaL_addlstring(&b, " ", 1);
            size += 1;
        }
    }

    luaL_pushresult(&b);
    return lua_tolstring(L, -1, NULL);
}

static inline void // level, tag, ...
loggerf(lua_State* L, int logId) {
    if (luai_unlikely(1 > logId || logId > SZ_LOGGERS)) return;

    int level = lua_tointeger(L, 1); // NULL则为0
    struct Logger* logger = loggers[logId-1];
    if (NULL == logger || level < logger->level) return;

    const char* tag = lua_tostring(L, 2);
    if (luai_unlikely(tag == NULL)) return;

    const char* logmsg = concatParams(L, 3, SZ_ONE_LOG);
    if (luai_unlikely(logmsg == NULL)) return;

    if(level >= ERROR) { // 是否打印堆栈
        luaL_traceback(L, L, logmsg, 2);
        logmsg = lua_tolstring(L, -1, NULL);
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    pthread_mutex_lock(&context.mutex);
    strftime(context.date, SZ_LONG_64, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
    outputf(logId, &tv.tv_sec, "%s.%.3ld [%s][%s] %s\n", context.date, tv.tv_usec/1000, strLevel(&level), tag, logmsg);
    pthread_mutex_unlock(&context.mutex);
}

static inline int
initContext(lua_State* L) {
    if (0 != _CLOG_INITED) return 0; // 全局初始化标志

    _CLOG_INITED = 1;
    memset(&context, 0, sizeof(context));

    int ftime = lua_tointeger(L, 1); // NULL则为0
    context.ftime = ftime > 0 ? ftime : DFL_FTIME;

    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    if (NULL == local) return 0;

    context.utcHour = ceil(now/3600);
    context.tm_hour = local->tm_hour;

    if (pthread_mutex_init(&context.mutex, NULL) != 0) return 0;
    if (pthread_create(&context.thread, NULL, threading, NULL) != 0) return 0;

    return 1;
}

static inline int
linit(lua_State* L) {
    if (initContext(L))
        initLoggers(L);
    return 0;
}

static inline int // 进程关闭时调用
lexit(lua_State* L) {
    if (_CLOG_INITED == 0) return 0;

    context.running = 0;
    closeAllFiles();

    return 0;
}

static inline int // 使用序号为1的logger配置输出
loutput(lua_State* L) {
    loggerf(L, 1);
    return 0;
}

static inline int // 使用指定序号的logger配置输出
llawput(lua_State* L) {
    int logId = lua_tointeger(L, 1);
    if (luai_unlikely(1 > logId || logId > SZ_LOGGERS)) return 0;

    lua_remove(L, 1);
    loggerf(L, logId);

    return 0;
}

static inline int // 输出内容完全由lua决定的纯净输出
lrawput(lua_State* L) {
    int logId = lua_tointeger(L, 1);
    if (luai_unlikely(1 > logId || logId > SZ_LOGGERS)) return 0;

    const char* str = concatParams(L, 2, -1);
    if (luai_unlikely(str == NULL)) return 0;

    time_t logtime = time(NULL);
    pthread_mutex_lock(&context.mutex);
    outputf(logId, &logtime, "%s\n", str);
    pthread_mutex_unlock(&context.mutex);

    return 0;
}

static inline int // 格式化日志
lformat(lua_State* L) {
    int level = lua_tointeger(L, 1); // NULL则为0
    const char* tag = lua_tostring(L, 2);
    if (luai_unlikely(tag == NULL)) return 0;

    const char* msg = concatParams(L, 3, SZ_ONE_LOG);
    if (luai_unlikely(msg == NULL)) return 0;

    if(level >= ERROR) { // 是否打印堆栈
        luaL_traceback(L, L, msg, 2);
        msg = lua_tolstring(L, -1, NULL);
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    char date[SZ_LONG_64] = {0};
    char filepath[SZ_LONG_256] = {0};

    strftime(date, SZ_LONG_64, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
    snprintf(filepath, SZ_LONG_256, "%s.%.3d [%s][%s] %s\n", date, tv.tv_usec/1000, strLevel(&level), tag, msg);

    lua_pushstring(L, filepath);
    return 1;
}

int
luaopen_clog(lua_State* L) {
    luaL_checkversion(L);
    luaL_Reg l[] = {
        { "init", linit },
        { "exit", lexit },
        { "output", loutput },
        { "lawput", llawput },
        { "rawput", lrawput },
        { "format", lformat },
        { NULL, NULL },
    };
    luaL_newlib(L, l);

    return 1;
}
