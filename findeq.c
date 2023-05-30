#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <pthread.h>

#define MAX_THREADS 64
#define DEFAULT_BYTES 1024
#define MAX_FILES 1000
#define MAX_PATH_LENGTH 256

int numThreads;
int minSize = DEFAULT_BYTES;
char *outputPath;
int file_out = 0;

pthread_mutex_t lock;

typedef struct
{
    char paths[MAX_FILES][MAX_PATH_LENGTH];
    int count;
} FileList;

typedef struct
{
    FileList *FileList;
    int start;
    int end;
} ThreadArgs;

typedef struct node
{
    char path[MAX_PATH_LENGTH];
    struct node * next;
    
} duplicateFileList;


duplicateFileList *head = NULL;

void getinputs(int argc, char *argv[])
{
    for (int i = 1; i < argc - 1; i++)
    {
        if (strcmp(argv[i], "-t") == 0)
        {
            i++;
            numThreads = atoi(argv[i]);

#ifdef DEBUG
            printf("Num Threads: %d\n", numThreads);
#endif
        }

        if (strcmp(argv[i], "-m") == 0)
        {
            i++;
            minSize = atoi(argv[i]);

#ifdef DEBUG
            printf("Min Bytes: %d\n", minSize);
#endif
        }

        if (strcmp(argv[i], "-o") == 0)
        {
            i++;
            strcpy(outputPath, argv[i]);

            // change the prog to produce output via file
            file_out = 1;
        }
        else
        {
            printf("Invalid input USAGE: ./findeq -t [numthreads] -m [mim_size of bytes] -o[outputpath]\n DIR(directory to search)\n");
            break;
        }
    }
}
bool Equalfiles(char *path1, char *path2)
{
    FILE *file1 = fopen(path1, "rb");
    FILE *file2 = fopen(path1, "rb");

    if (file1 == NULL || file2 == NULL)
    {
        return false;
    }

    bool areEqual = true;

    while (true)
    {
        int c1 = fgetc(file1);
        int c2 = fgetc(file2);

        if (c1 != c2)
        {
            areEqual = false;
            break;
        }
        if (c1 == EOF || c2 == EOF)
            break;
    }

    fclose(file1);
    fclose(file2);
    return areEqual;
}
void *compareFiles(void *arg)
{
    ThreadArgs *args = (ThreadArgs *) arg;
    FileList *fileList = args -> FileList;
    int start = args -> start;
    int end = args -> end;

    for(int i=start;i<end;i++)
    {
        for(int j=i+1;j<fileList->count;j++)
        {
            if(Equalfiles(fileList->paths[i],fileList->paths[i]))
            {
                pthread_mutex_lock(&lock);
                duplicateFileList * newNode = malloc(sizeof(duplicateFileList));
                strncpy(newNode->path, fileList->paths[i], MAX_PATH_LENGTH);
                #ifdef DEBUG
                printf("File Path added to Dup List: %s\n",fileList->paths[i]);
                #endif
                newNode -> next =head;
                head = newNode;
                pthread_mutex_unlock(&lock);
            }
        }
    }

    return NULL;
}
void traverseDirectory(const char *dir, FileList *filelist)
{
    struct dirent *entry;
    DIR *dp;

    dp = opendir(dir);

    if (dp == NULL)
    {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dp)))
    {
        struct stat statbuf;
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s%s", dir, entry->d_name);

        if(stat(path, &statbuf)!=0)
        {
            perror("stat");
            return;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            traverseDirectory(path, filelist);
        }
        else if(statbuf.st_size >= minSize)
        {
            int debugCount = filelist->count;
            #ifdef DEBUG
            printf("File path added: %s\n",path);
            #endif
            strncpy(filelist->paths[filelist->count++], path, MAX_PATH_LENGTH);
        }


    }
    closedir(dp);
}

int main(int argc, char *argv[])
{
    getinputs(argc, argv);
    const char *dir = argv[argc - 1];

    FileList filelist;
    filelist.count = 0;
    pthread_mutex_init(&lock,NULL);


    traverseDirectory(dir, &filelist); // traverse files and save the paths to the filelist

    pthread_t threads[numThreads];
    ThreadArgs threadArgs[numThreads];// init threads and thread args

    int filesPerThread = filelist.count/ numThreads;
    
    for(int i=0;i<numThreads;i++)
    {
        threadArgs[i].FileList = &filelist;
        threadArgs[i].start = i * filesPerThread;
        threadArgs[i].end = i+1 * filesPerThread;
        pthread_create(&threads[i],NULL,compareFiles,&threadArgs[i]);
    }

    threadArgs[numThreads - 1].end = filelist.count;

    for(int i=0;i<numThreads;i++)
    {
        pthread_join(threads[i],NULL);
    }

    return 0;
}