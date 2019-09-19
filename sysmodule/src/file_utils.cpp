/*
 * --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */

#include "file_utils.h"
#include <nxExt.h>

static LockableMutex g_log_mutex;
static std::atomic_bool g_has_initialized = false;
static bool g_log_enabled = false;
static std::uint64_t g_last_flag_check = 0;

extern "C" void __libnx_init_time(void);

static void _FileUtils_InitializeThreadFunc(void *args)
{
    FileUtils::Initialize();
}

bool FileUtils::IsInitialized()
{
    return g_has_initialized;
}

void FileUtils::LogLine(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (g_has_initialized)
    {
        g_log_mutex.Lock();

        FileUtils::RefreshFlags(false);

        if(g_log_enabled)
        {
            FILE *file = fopen(FILE_LOG_FILE_PATH, "a");
            if (file)
            {
                time_t timer  = time(NULL);
                struct tm* timerTm = localtime(&timer);

                va_start(args, format);
                fprintf(file, "[%04d-%02d-%02d %02d:%02d:%02d] ", timerTm->tm_year+1900, timerTm->tm_mon+1, timerTm->tm_mday, timerTm->tm_hour, timerTm->tm_min, timerTm->tm_sec);
                vfprintf(file, format, args);
                fprintf(file, "\n");
                fclose(file);
            }
        }

        g_log_mutex.Unlock();
    }
    va_end(args);
}

void FileUtils::RefreshFlags(bool force)
{
    std::uint64_t now = armTicksToNs(armGetSystemTick());
    if(!force && (now - g_last_flag_check) < FILE_FLAG_CHECK_INTERVAL_NS)
    {
        return;
    }

    FILE *file = fopen(FILE_LOG_FLAG_PATH, "r");
    if (file)
    {
        g_log_enabled = true;
        fclose(file);
    } else {
        g_log_enabled = false;
    }

    g_last_flag_check = now;
}

void FileUtils::InitializeAsync()
{
    Thread initThread = {0};
    threadCreate(&initThread, _FileUtils_InitializeThreadFunc, NULL, 0x4000, 0x15, 0);
    threadStart(&initThread);
}

Result FileUtils::Initialize()
{
    Result rc = 0;

    if (R_SUCCEEDED(rc))
    {
        rc = timeInitialize();
    }

    __libnx_init_time();

    if (R_SUCCEEDED(rc))
    {
        rc = fsInitialize();
    }

    if (R_SUCCEEDED(rc))
    {
        rc = fsdevMountSdmc();
    }

    if (R_SUCCEEDED(rc))
    {
        FileUtils::RefreshFlags(true);
        FileUtils::LogLine("=== " TARGET " " TARGET_VERSION " ===");
        g_has_initialized = true;
    }

    return rc;
}

void FileUtils::Exit()
{
    if (!g_has_initialized)
    {
        return;
    }

    g_has_initialized = false;
    g_log_enabled = false;

    fsdevUnmountAll();
    fsExit();
    timeExit();
}