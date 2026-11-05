/* stats.h - asks the operating system about CPU, memory and running processes.
 *
 * This file is part of the system monitor tutorial. It's the only part of the program
 * that's different on each operating system. You don't need to understand it to follow
 * the tutorial: the three functions below are all you need. The last section of the
 * tutorial explains how the Linux version works, if you're curious.
 *
 * Include this AFTER cterm.h, so that cterm can set up your system's headers first:
 *
 *     #define CTERM_IMPLEMENTATION
 *     #include "cterm.h"
 *     #include "stats.h"
 */
#ifndef STATS_H
#define STATS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Early steps of the tutorial don't use all of these functions. This stops the compiler
 * warning about that. */
#if defined(__GNUC__) || defined(__clang__)
  #define STATS_API static __attribute__((unused))
#else
  #define STATS_API static
#endif

/* One running program. */
typedef struct stats_process {
    int pid;                   /* the number the OS uses to identify it */
    unsigned long long memory; /* memory in use, in bytes; 0 if the OS won't tell us */
    char name[64];             /* its name, as UTF-8 */
} stats_process;

/* Returns how busy the CPU has been since the last call, from 0 to 100. Averaged across
 * all CPU cores. The first call has nothing to compare with, so it returns 0. */
STATS_API double stats_cpu_percent(void);

/* Gets how much memory is in use, and how much there is in total, in bytes. */
STATS_API void stats_memory(unsigned long long *used, unsigned long long *total);

/* Fills `out` with up to `max` running programs, and returns how many it found. */
STATS_API int stats_processes(stats_process *out, int max);

/* Shared by all three versions: turns the OS's two running totals into a percentage.
 * "Busy" and "total" only ever grow, so what matters is how much they grew since last
 * time. For example, if total grew by 100 and busy grew by 25, the CPU was 25% busy. */
static double stats__percent_since_last(unsigned long long busy, unsigned long long total) {
    static unsigned long long last_busy = 0, last_total = 0;
    double percent = 0.0;
    if (last_total != 0 && total > last_total) {
        percent = 100.0 * (double)(busy - last_busy) / (double)(total - last_total);
    }
    last_busy = busy;
    last_total = total;
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    return percent;
}

/* ======================================================================= */
#if defined(__linux__)
/* Linux shows this information as text files in the /proc folder. */

#include <dirent.h>
#include <unistd.h>

static double stats_cpu_percent(void) {
    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    unsigned long long busy;
    FILE *file = fopen("/proc/stat", "r");
    if (!file) return 0.0;
    if (fscanf(file, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
        fclose(file);
        return 0.0;
    }
    fclose(file);
    busy = user + nice + system + irq + softirq + steal;
    return stats__percent_since_last(busy, busy + idle + iowait);
}

static void stats_memory(unsigned long long *used, unsigned long long *total) {
    char line[256];
    unsigned long long total_kb = 0, available_kb = 0;
    FILE *file = fopen("/proc/meminfo", "r");
    *used = 0;
    *total = 0;
    if (!file) return;
    while (fgets(line, sizeof line, file)) {
        sscanf(line, "MemTotal: %llu kB", &total_kb);
        sscanf(line, "MemAvailable: %llu kB", &available_kb);
    }
    fclose(file);
    *total = total_kb * 1024;
    *used = (total_kb - available_kb) * 1024;
}

static int stats_processes(stats_process *out, int max) {
    unsigned long long page_size = (unsigned long long)sysconf(_SC_PAGESIZE);
    DIR *folder = opendir("/proc");
    struct dirent *entry;
    int count = 0;
    if (!folder) return 0;

    /* Each running program has a folder in /proc named after its pid. */
    while (count < max && (entry = readdir(folder)) != NULL) {
        char path[300];
        FILE *file;
        stats_process *process = &out[count];
        unsigned long long pages = 0;
        size_t length;

        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

        snprintf(path, sizeof path, "/proc/%s/comm", entry->d_name);
        file = fopen(path, "r");
        if (!file) continue; /* it may have exited a moment ago */
        if (!fgets(process->name, sizeof process->name, file)) process->name[0] = '\0';
        fclose(file);
        length = strlen(process->name);
        if (length > 0 && process->name[length - 1] == '\n') process->name[length - 1] = '\0';

        /* statm counts memory in "pages". Its second number is the pages in use. */
        snprintf(path, sizeof path, "/proc/%s/statm", entry->d_name);
        file = fopen(path, "r");
        if (file) {
            if (fscanf(file, "%*u %llu", &pages) != 1) pages = 0;
            fclose(file);
        }

        process->pid = atoi(entry->d_name);
        process->memory = pages * page_size;
        count++;
    }
    closedir(folder);
    return count;
}

/* ======================================================================= */
#elif defined(__APPLE__)
/* macOS provides this through its "Mach" and "libproc" system interfaces. */

#include <libproc.h>
#include <mach/mach.h>
#include <sys/sysctl.h>

static double stats_cpu_percent(void) {
    host_cpu_load_info_data_t load;
    mach_msg_type_number_t size = HOST_CPU_LOAD_INFO_COUNT;
    unsigned long long busy;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&load, &size) != KERN_SUCCESS) return 0.0;
    busy = (unsigned long long)load.cpu_ticks[CPU_STATE_USER] + load.cpu_ticks[CPU_STATE_SYSTEM] +
           load.cpu_ticks[CPU_STATE_NICE];
    return stats__percent_since_last(busy, busy + load.cpu_ticks[CPU_STATE_IDLE]);
}

static void stats_memory(unsigned long long *used, unsigned long long *total) {
    uint64_t installed = 0;
    size_t length = sizeof installed;
    vm_statistics64_data_t vm;
    mach_msg_type_number_t size = HOST_VM_INFO64_COUNT;
    vm_size_t page_size = 0;

    *used = 0;
    *total = 0;
    if (sysctlbyname("hw.memsize", &installed, &length, NULL, 0) == 0) *total = installed;
    host_page_size(mach_host_self(), &page_size);
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &size) == KERN_SUCCESS) {
        *used = ((unsigned long long)vm.active_count + vm.wire_count + vm.compressor_page_count) * page_size;
    }
}

static int stats_processes(stats_process *out, int max) {
    static int pids[8192];
    int found = proc_listallpids(pids, (int)sizeof pids);
    int count = 0, i;
    for (i = 0; i < found && count < max; i++) {
        stats_process *process = &out[count];
        struct proc_taskinfo task;
        if (pids[i] <= 0) continue;
        if (proc_name(pids[i], process->name, sizeof process->name) <= 0) {
            snprintf(process->name, sizeof process->name, "(pid %d)", pids[i]);
        }
        process->pid = pids[i];
        /* macOS only tells us this about our own programs, unless running as root. */
        process->memory = 0;
        if (proc_pidinfo(pids[i], PROC_PIDTASKINFO, 0, &task, sizeof task) == (int)sizeof task) {
            process->memory = task.pti_resident_size;
        }
        count++;
    }
    return count;
}

/* ======================================================================= */
#elif defined(_WIN32)
/* Windows provides this through its system libraries. */

#include <windows.h>
#include <tlhelp32.h>
/* Version 2 lives in a library every program already links, so there's nothing extra
 * to add to the compile command. */
#ifndef PSAPI_VERSION
  #define PSAPI_VERSION 2
#endif
#include <psapi.h>

static unsigned long long stats__filetime(FILETIME time_value) {
    return ((unsigned long long)time_value.dwHighDateTime << 32) | time_value.dwLowDateTime;
}

static double stats_cpu_percent(void) {
    FILETIME idle, kernel, user;
    unsigned long long total;
    if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0;
    /* On Windows, the "kernel" time already includes the idle time. */
    total = stats__filetime(kernel) + stats__filetime(user);
    return stats__percent_since_last(total - stats__filetime(idle), total);
}

static void stats_memory(unsigned long long *used, unsigned long long *total) {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof status;
    *used = 0;
    *total = 0;
    if (GlobalMemoryStatusEx(&status)) {
        *total = status.ullTotalPhys;
        *used = status.ullTotalPhys - status.ullAvailPhys;
    }
}

static int stats_processes(stats_process *out, int max) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry;
    int count = 0;
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    entry.dwSize = sizeof entry;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            stats_process *process = &out[count];
            HANDLE handle;
            WideCharToMultiByte(CP_UTF8, 0, entry.szExeFile, -1, process->name, (int)sizeof process->name, NULL, NULL);
            process->name[sizeof process->name - 1] = '\0';
            process->pid = (int)entry.th32ProcessID;
            process->memory = 0;
            /* Windows won't tell us about some protected system programs. */
            handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (handle) {
                PROCESS_MEMORY_COUNTERS counters;
                if (GetProcessMemoryInfo(handle, &counters, sizeof counters)) {
                    process->memory = counters.WorkingSetSize;
                }
                CloseHandle(handle);
            }
            count++;
        } while (count < max && Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count;
}

/* ======================================================================= */
#else
  #error "stats.h doesn't support this operating system yet. Add a version above."
#endif

#endif /* STATS_H */
