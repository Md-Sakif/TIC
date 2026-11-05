/* e2e_conpty.c - end-to-end test for the Windows backend.
 *
 * ConPTY is Windows' pseudo-terminal: it gives a program a real console, with this
 * test playing the part of the terminal window. It's the Windows version of
 * tests/e2e.py. It starts tests/e2e_app.c inside a pseudo console, sends it keys,
 * resizes it, and checks the event log. It needs Windows 10 version 1809 or newer.
 *
 * Usage: e2e_conpty e2e_app.exe */
#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
  #define NTDDI_VERSION 0x0A000006
#endif
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
  #define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

#define LOG_FILE "e2e_conpty.log"

static int failures = 0;
static HANDLE to_app;     /* write here: the keyboard */
static HANDLE from_app;   /* read here: the screen */

static void fail(const char *message) {
    failures++;
    printf("  FAIL: %s\n", message);
}

/* The screen output has to be read constantly, or the app blocks when the pipe fills
 * up. This thread keeps it moving and remembers the text for checks. */
static char screen[1 << 20];
static volatile LONG screen_length = 0;

static DWORD WINAPI read_screen(LPVOID unused) {
    char chunk[4096];
    DWORD got;
    (void)unused;
    while (ReadFile(from_app, chunk, sizeof chunk, &got, NULL) && got > 0) {
        LONG length = screen_length;
        if ((size_t)length + got < sizeof screen) {
            memcpy(screen + length, chunk, got);
            screen_length = length + (LONG)got;
        }
    }
    return 0;
}

static int log_contains(const char *line) {
    char text[1 << 16];
    size_t n;
    FILE *f = fopen(LOG_FILE, "rb");
    if (!f) return 0;
    n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = '\0';
    return strstr(text, line) != NULL;
}

static int wait_for_log(const char *line) {
    int i;
    for (i = 0; i < 300; i++) { /* up to about 15 seconds */
        if (log_contains(line)) return 1;
        Sleep(50);
    }
    return 0;
}

static void send_bytes(const char *bytes, DWORD n) {
    DWORD written;
    WriteFile(to_app, bytes, n, &written, NULL);
}

static void expect(const char *line) {
    if (!wait_for_log(line)) {
        char message[256];
        snprintf(message, sizeof message, "the log never contained \"%s\"", line);
        fail(message);
    }
}

static int running_on_wine(void) {
    return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
}

int main(int argc, char **argv) {
    HANDLE pty_in_read, pty_out_write, thread;
    HPCON console;
    COORD size;
    STARTUPINFOEXA startup;
    PROCESS_INFORMATION process;
    SIZE_T attr_size = 0;
    char command[1024];
    DWORD exit_code = 1;
    HRESULT result;

    if (argc < 2) {
        printf("usage: e2e_conpty <path to e2e_app.exe>\n");
        return 2;
    }
    DeleteFileA(LOG_FILE);

    if (!CreatePipe(&pty_in_read, &to_app, NULL, 0) || !CreatePipe(&from_app, &pty_out_write, NULL, 0)) {
        printf("could not create pipes\n");
        return 2;
    }
    size.X = 80;
    size.Y = 24;
    result = CreatePseudoConsole(size, pty_in_read, pty_out_write, 0, &console);
    if (FAILED(result)) {
        printf("could not create a pseudo console (0x%08lX). Windows 10 1809 or newer is required.\n", (unsigned long)result);
        return 2;
    }
    thread = CreateThread(NULL, 0, read_screen, NULL, 0, NULL);

    memset(&startup, 0, sizeof startup);
    startup.StartupInfo.cb = sizeof startup;
    /* On Windows, this stops the child inheriting this test's own redirected handles
     * (CI runs tests with pipes as stdin and stdout), so it uses the pseudo console.
     * Wine doesn't fill in the console handles when asked this way, so skip it there. */
    if (!running_on_wine()) {
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    }
    InitializeProcThreadAttributeList(NULL, 1, 0, &attr_size);
    startup.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attr_size);
    InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attr_size);
    UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console,
                              sizeof console, NULL, NULL);

    snprintf(command, sizeof command, "\"%s\" %s", argv[1], LOG_FILE);
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                        &startup.StartupInfo, &process)) {
        printf("could not start %s (error %lu)\n", argv[1], (unsigned long)GetLastError());
        return 2;
    }

    printf("  startup\n");
    expect("ready 80 24");
    if (failures) {
        /* Every other check would wait for its full timeout, so stop here. */
        FILE *f = fopen(LOG_FILE, "rb");
        char why[256] = "";
        if (f) {
            why[fread(why, 1, sizeof why - 1, f)] = '\0';
            fclose(f);
        }
        printf("  the app's log says: %s\n", why);
        TerminateProcess(process.hProcess, 1);
        return 1;
    }

    printf("  plain keys\n");
    send_bytes("a", 1);
    expect("key 97 0");

    /* Wine 10 drops this byte, and console waits then stop working, even in a program
     * that doesn't use TIC. So this can only be checked on real Windows. */
    if (!running_on_wine()) {
        printf("  Ctrl+C arrives as a key and doesn't stop the app\n");
        send_bytes("\x03", 1);
        expect("key 99 4");
    }

    printf("  arrow key\n");
    send_bytes("\x1b[A", 3);
    expect("key 1114116 0");

    printf("  non-ASCII text\n");
    send_bytes("\xE0\xA6\x95", 3);
    expect("key 2453 0");
    send_bytes("\xF0\x9F\x98\x80", 4); /* above U+FFFF, so two UTF-16 units on Windows */
    expect("key 128512 0");

    printf("  Esc key timing\n");
    send_bytes("\x1b", 1);
    expect("key 1114115 0");

    printf("  drawing reaches the terminal\n");
    send_bytes("\x10", 1);
    expect("key 112 4");
    {
        int i, found = 0;
        for (i = 0; i < 100 && !found; i++) {
            found = strstr(screen, "frame-marker") != NULL;
            if (!found) Sleep(50);
        }
        if (!found) fail("\"frame-marker\" never reached the screen");
    }

    /* Wine 10's ResizePseudoConsole changes nothing, even in a program that doesn't use
     * TIC. So this can only be checked on real Windows. */
    if (!running_on_wine()) {
        printf("  resize\n");
        size.X = 100;
        size.Y = 30;
        ResizePseudoConsole(console, size);
        expect("resize 100 30");
    }

    printf("  suspend is refused on Windows\n");
    send_bytes("\x13", 1);
    expect("suspend -1");

    printf("  quit\n");
    send_bytes("\x11", 1);
    expect("done");
    if (WaitForSingleObject(process.hProcess, 10000) != WAIT_OBJECT_0) {
        fail("the app did not exit");
        TerminateProcess(process.hProcess, 1);
    } else {
        GetExitCodeProcess(process.hProcess, &exit_code);
        if (exit_code != 0) fail("the app exited with an error");
    }

    ClosePseudoConsole(console);
    CloseHandle(to_app);
    CloseHandle(pty_in_read);
    CloseHandle(pty_out_write);
    WaitForSingleObject(thread, 2000);
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    free(startup.lpAttributeList);
    DeleteFileA(LOG_FILE);

    printf("%s: ConPTY end-to-end test, %d failed\n", failures ? "FAILED" : "ok", failures);
    return failures ? 1 : 0;
}
