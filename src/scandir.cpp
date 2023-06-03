#include <stdio.h>
#include <ghc/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "scandir.h"

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#endif

namespace fs = ghc::filesystem;

// #define PATH_MAX 256

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

int tree_scandir(char *dirname, scandir_cbk_t cbk, uintptr_t ud)
{
    std::error_code ec;
    int count = 1;
    char name[PATH_MAX];
    char path[PATH_MAX];

    try
    {
        for (const auto &dirEntry : fs::recursive_directory_iterator(dirname, fs::directory_options::skip_permission_denied, ec))
        {
            if (dirEntry.is_regular_file())
            {
                const std::string fullpath = dirEntry.path().generic_string();
                const std::string filename = dirEntry.path().filename().generic_string();
                
                cbk(fullpath.c_str(), filename.c_str(), ud);
            }
        }
    }
    catch (fs::filesystem_error e)
    {
        std::cout << e.code() << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.path1() << std::endl;
        std::cout << e.path2() << std::endl;
        return 0;
    }

    return 0;
}