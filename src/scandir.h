#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
    typedef int (*scandir_cbk_t)(const char *fullpath, const char *name, int attr);
    int fscandir(char *dirname, scandir_cbk_t cbk);

#ifdef __cplusplus
}
#endif