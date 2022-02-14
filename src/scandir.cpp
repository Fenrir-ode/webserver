#include <stdio.h>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "scandir.h"
namespace fs = std::filesystem;

// #define PATH_MAX 256

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>

#define LAA(se)                                                      \
    {                                                                \
        {se}, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT \
    }

#define BEGIN_PRIVILEGES(tp, n)            \
    static const struct                    \
    {                                      \
        ULONG PrivilegeCount;              \
        LUID_AND_ATTRIBUTES Privileges[n]; \
    } tp = {n, {
#define END_PRIVILEGES \
    }                  \
    }                  \
    ;

// in case you not include wdm.h, where this defined
#define SE_BACKUP_PRIVILEGE (17L)

ULONG AdjustPrivileges()
{
    if (ImpersonateSelf(SecurityImpersonation))
    {
        HANDLE hToken;
        if (OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES, TRUE, &hToken))
        {
            BEGIN_PRIVILEGES(tp, 1)
            LAA(SE_BACKUP_PRIVILEGE),
                END_PRIVILEGES
                    AdjustTokenPrivileges(hToken, FALSE, (PTOKEN_PRIVILEGES)&tp, 0, 0, 0);
            CloseHandle(hToken);
        }
    }

    return GetLastError();
}
#endif

int fscandir(char *dirname, scandir_cbk_t cbk)
{
    std::error_code ec;
    int count = 1;
    char name[PATH_MAX];
    char path[PATH_MAX];

#if defined(WIN32) || defined(_WIN32)
    AdjustPrivileges();
    std::cout << "WINDOWWS" << std::endl;
#endif

    //for (auto const &dir_entry : fs::recursive_directory_iterator(dirname, ec))
    // auto dir_entry = std::filesystem::recursive_directory_iterator(dirname, fs::directory_options::skip_permission_denied, ec);
    try
    {
        //   for (; dir_entry != std::filesystem::recursive_directory_iterator(); dir_entry++)
        for (const auto &dirEntry : std::filesystem::recursive_directory_iterator(dirname, fs::directory_options::skip_permission_denied, ec))
        {
            /**/
            // const std::string directoryName = (*dir_entry).path().filename();
            try
            {

                std::cout << (dirEntry).path().filename() << '\n';
            }
            catch (...)
            {
            }
        }
    }
    catch (std::filesystem::filesystem_error e)
    {
        std::cout << e.code() << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.path1() << std::endl;
        std::cout << e.path2() << std::endl;
        return 0;
    }

    return 0;
}