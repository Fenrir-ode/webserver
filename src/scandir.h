#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
    typedef int (*scandir_cbk_t)(const char *fullpath, const char *basename, uintptr_t ud);
    int tree_scandir(char *dirname, scandir_cbk_t cbk, uintptr_t ud);

#ifdef __cplusplus
}
#endif