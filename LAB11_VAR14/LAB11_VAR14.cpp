// Var14.cpp : Определяет точку входа для приложения.
//

#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <commctrl.h>
#include <commdlg.h>
#include <Psapi.h>
#include <strsafe.h>
#include <processthreadsapi.h>
#define ID_BUTTON1 1000
#define ID_BUTTON2 500
#define ID_BUTTON3 250
#define ID_LIST1 2500
#define ID_LIST2 1500
// Global variables

// The main window class name.
static TCHAR szWindowClass[] = _T("LAB11_VAR14");

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("Ничего себе.");

HINSTANCE hInst;
HWND hProgress;
DWORD IDC_TIMER, nCounter, IDC_PROGRESS1 = 1000, IpProcessId;
HWND hCombo, hList, hList2;
HWND ReplDialog;
UINT uFindMsgString;


// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void LoadProcessesToListBox(HWND hwndCtrl)
{
    ListBox_ResetContent(hwndCtrl);

    DWORD aProcessIds[1024], cbNeeded = 0;
    BOOL bRet = EnumProcesses(aProcessIds, sizeof(aProcessIds), &cbNeeded);

    if (FALSE != bRet)
    {
        TCHAR szName[MAX_PATH], szBuffer[300];

        for (DWORD i = 0, n = cbNeeded / sizeof(DWORD); i < n; ++i)
        {
            DWORD dwProcessId = aProcessIds[i], cch = 0;
            if (0 == dwProcessId) continue;

            HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, dwProcessId);

            if (NULL != hProcess)

            {
                cch = GetModuleBaseName(hProcess, NULL, szName, MAX_PATH);
                CloseHandle(hProcess);
            }

            if (0 == cch)
                StringCchCopy(szName, MAX_PATH, TEXT("Неизвестный процесс"));
            StringCchPrintf(szBuffer, _countof(szBuffer), TEXT("%s (PID: %u)"), szName, dwProcessId);

            int iItem = ListBox_AddString(hwndCtrl, szBuffer);

            ListBox_SetItemData(hwndCtrl, iItem, dwProcessId);
        }
    }
}

void LoadModulesToListBox(HWND hwndCtrl, DWORD dwProcessId)
{
    ListBox_ResetContent(hwndCtrl);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);

    if (NULL != hProcess)
    {
        DWORD cb = 0;
        EnumProcessModulesEx(hProcess, NULL, 0, &cb, LIST_MODULES_ALL);

        DWORD nCount = cb / sizeof(HMODULE);

        HMODULE* hModule = new HMODULE[nCount];

        cb = nCount * sizeof(HMODULE);
        BOOL bRet = EnumProcessModulesEx(hProcess, hModule, cb, &cb, LIST_MODULES_ALL);

        if (FALSE != bRet)
        {
            TCHAR szFileName[MAX_PATH];

            for (DWORD i = 0; i < nCount; ++i)
            {
                bRet = GetModuleFileNameEx(hProcess, hModule[i], szFileName, MAX_PATH);
                if (FALSE != bRet)
                    ListBox_AddString(hwndCtrl, szFileName);
            }
        }
        delete[]hModule;

        CloseHandle(hProcess);
    }
}

BOOL WaitProcessById(DWORD dwProcessId, DWORD dwMilliseconds, LPDWORD IpExitCode)
{
    HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, dwProcessId);
    if (NULL == hProcess)
    {
        return FALSE;
    }

    WaitForSingleObject(hProcess, dwMilliseconds);

    if (NULL != IpExitCode)
    {
        GetExitCodeProcess(hProcess, IpExitCode);
    }

    CloseHandle(hProcess);

    return TRUE;
}

BOOL StartGroupProcessAsJob(HANDLE hJob, LPCTSTR lpszCmdLine[], DWORD nCount, BOOL bInheritHandles, DWORD dwCreationFlags)
{
    BOOL bInJob = FALSE;
    IsProcessInJob(GetCurrentProcess(), NULL, &bInJob);

    if (FALSE != bInJob)
    {
        JOBOBJECT_BASIC_LIMIT_INFORMATION jobli = { 0 };

        QueryInformationJobObject(NULL, JobObjectBasicLimitInformation, &jobli, sizeof(jobli), NULL);

        DWORD dwLimitMask = JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;

        if ((jobli.LimitFlags & dwLimitMask) == 0)
        {
            return FALSE;
        }
    }

    TCHAR szCmdLine[MAX_PATH];

    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;

    for (DWORD i = 0; i < nCount; ++i)
    {
        StringCchCopy(szCmdLine, MAX_PATH, lpszCmdLine[i]);

        BOOL bRet = CreateProcess(NULL, szCmdLine, NULL, NULL, bInheritHandles, dwCreationFlags
            | CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB, NULL, NULL, &si, &pi);

        if (FALSE != bRet)
        {
            AssignProcessToJobObject(hJob, pi.hProcess);

            ResumeThread(pi.hThread);

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            return TRUE;
        }
    }
}

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow
)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(wcex.hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL,
            _T("Call to RegisterClassEx failed!"),
            _T("Windows Desktop Guided Tour"),
            NULL);

        return 1;
    }

    // Store instance handle in our global variable
    hInst = hInstance;

    // The parameters to CreateWindowEx explained:
    // WS_EX_OVERLAPPEDWINDOW : An optional extended window style.
    // szWindowClass: the name of the application
    // szTitle: the text that appears in the title bar
    // WS_OVERLAPPEDWINDOW: the type of window to create
    // CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
    // 500, 100: initial size (width, length)
    // NULL: the parent of this window
    // NULL: this application does not have a menu bar
    // hInstance: the first parameter from WinMain
    // NULL: not used in this application
    HWND hWnd = CreateWindowEx(
        WS_EX_OVERLAPPEDWINDOW,
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1000, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

   

    if (!hWnd)
    {
        MessageBox(NULL,
            _T("Call to CreateWindow failed!"),
            _T("Windows Desktop Guided Tour"),
            NULL);

        return 1;
    }
    else
    {
        HWND hWnd_button = CreateWindow(TEXT("button"), TEXT("Ждать зовершения процесса"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 470, 200, 50, hWnd, (HMENU)ID_BUTTON1, hInstance, NULL);
        HWND hWnd_button2 = CreateWindow(TEXT("button"), TEXT("Зовершить процесс"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            220, 470, 200, 50, hWnd, (HMENU)ID_BUTTON2, hInstance, NULL);
        HWND hWnd_button3 = CreateWindow(TEXT("button"), TEXT("Создать задание"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            430, 470, 200, 50, hWnd, (HMENU)ID_BUTTON3, hInstance, NULL);
        //создаём список
        hList = CreateWindowEx(0,L"listbox", NULL, WS_CHILD | WS_VISIBLE | LBS_STANDARD,
            10, 10, 450, 450, hWnd, (HMENU)ID_LIST1, hInst, NULL);
        // Отменяем режим перерисовки списка
        LoadProcessesToListBox(hList);
        //создаём список
        hList2 = CreateWindowEx(0, L"listbox", NULL, WS_CHILD | WS_VISIBLE | LBS_STANDARD,
            460, 10, 450, 450, hWnd, (HMENU)ID_LIST2, hInst, NULL);
    }


    // The parameters to ShowWindow explained:
    // hWnd: the value returned from CreateWindow
    // nCmdShow: the fourth parameter from WinMain
    ShowWindow(hWnd,
        nCmdShow);
    UpdateWindow(hWnd);

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;

}


//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    char Temp[_MAX_DIR];
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_COMMAND: 
    {
        JOBOBJECT_BASIC_LIMIT_INFORMATION jobli;
        HANDLE hProcess;
        HANDLE hJob = CreateJobObject(NULL, NULL);
        DWORD ProcessId;
        int item;
        int wmId = LOWORD(wParam);
        // Разобрать выбор в меню:
        switch (wmId)
        {
        case ID_LIST1:

            item = SendMessage(hList, LB_GETCURSEL, 0, 0L);
            ProcessId = ListBox_GetItemData(hList, item);

            if (ProcessId != 0) 
            {
                LoadModulesToListBox(hList2, ProcessId);
            }
            else 
            {
                ListBox_ResetContent(hList2);
            }
            break;

        case ID_BUTTON1:
            item = SendMessage(hList, LB_GETCURSEL, 0, 0L);
            ProcessId = ListBox_GetItemData(hList, item);
            if (ProcessId != 0) {
                WaitProcessById(ProcessId, 5000, NULL);
            }
            break;
        case ID_BUTTON2:
            item = SendMessage(hList, LB_GETCURSEL, 0, 0L);
            ProcessId = ListBox_GetItemData(hList, item);

            if (ProcessId != 0) 
            {
                hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);
                TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);
                LoadProcessesToListBox(hList);
            }
            break;
        case ID_BUTTON3:
            if (NULL != hJob)
            {
                jobli = { 0 };

                jobli.PriorityClass = IDLE_PRIORITY_CLASS;
                jobli.LimitFlags = JOB_OBJECT_LIMIT_PRIORITY_CLASS | JOB_OBJECT_LIMIT_BREAKAWAY_OK;

                BOOL bRet = SetInformationJobObject(hJob, JobObjectBasicLimitInformation, &jobli, sizeof(jobli));

                if (FALSE != bRet)
                {
                    LPCTSTR szCmdLine[] = { TEXT("cmd.exe"), TEXT("notepad"), TEXT("calc.exe") };

                    bRet = StartGroupProcessAsJob(hJob, szCmdLine, _countof(szCmdLine), FALSE, 0);
                    LoadProcessesToListBox(hList);
                }
                CloseHandle(hJob);
            }
            break;
        }
        break; 
    }
    case WM_CREATE:
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return 0;
}
