#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

int is_directory(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

int main()
{
    char path[] = "./example1.c";
    
    if(is_regular_file(path))
    {
        printf("It is a file.\n");
    }
    else if(is_directory(path))
    {
        printf("It is a directory.\n");
    }
    else
    {
        printf("It is neither a regular file nor a directory.\n");
    }
    return 0;
}
