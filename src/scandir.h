#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
    typedef int (*scandir_cbk_t)(const char *fullpath, const char *basename, int attr);
    int tree_scandir(char *dirname, scandir_cbk_t cbk);

#ifdef __cplusplus
}
#endif