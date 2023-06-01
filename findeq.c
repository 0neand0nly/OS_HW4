#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#define MAX_THREADS 64
#define DEFAULT_BYTES 1024
#define MAX_FILES 1000
#define MAX_PATH_LENGTH 1000
#define TABLE_SIZE 1000

int numThreads;
int minSize = DEFAULT_BYTES;
char *outputPath;
int file_out = 0;
int dupList_count = 0;

struct itimerval timer;

pthread_mutex_t lock;
pthread_cond_t cond;
int threadsCompleted = 0;

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
    struct node *next;
} duplicateFileList;

typedef struct
{
    duplicateFileList *list;
} HashTable;

HashTable table[TABLE_SIZE];

unsigned int hash(const char *str)
{
    unsigned int value = 0;
    for (; *str != '\0'; ++str)
        value = value * 37 + *str;
    return value % TABLE_SIZE;
}

void addTable(const char *path)
{
    unsigned int index = hash(path);
    duplicateFileList *newNode = malloc(sizeof(duplicateFileList));
    strncpy(newNode->path, path, MAX_PATH_LENGTH);
    newNode->next = table[index].list;
    table[index].list = newNode;
}

unsigned traverseTable(const char *path)
{
    unsigned int index = hash(path);
    duplicateFileList *curr = table[index].list;
    while (curr != NULL)
    {
        if (strcmp(curr->path, path) == 0)
            return true;
        curr = curr->next;
    }
    return false;
}

void dumpTable(FILE *fp)
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        duplicateFileList *curr = table[i].list;
        while (curr != NULL)
        {
            fprintf(fp, "%s\n", curr->path);
            curr = curr->next;
        }
        duplicateFileList *temp;
        while (curr != NULL)
        {
            temp = curr->next;
            free(curr->path);
            free(curr);
            curr = temp;
        }
    }
}

void dumpTableC()
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        duplicateFileList *curr = table[i].list;
        while (curr != NULL)
        {
            printf("%s\n", curr->path);
            curr = curr->next;
        }
        duplicateFileList *temp;
        while (curr != NULL)
        {
            temp = curr->next;
            free(curr->path);
            free(curr);
            curr = temp;
        }
    }
}

void killProgram()
{
    if (file_out == 1)
    {
        FILE *fp = fopen(outputPath, "w");
        if (fp == NULL)
        {
            perror("Failed to open the output file");
            exit(EXIT_FAILURE);
        }
        printf("File Written\n");
        dumpTable(fp);

        fclose(fp);
    }
    else
    {
        dumpTableC();
    }
}

void sig_Handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("Number of Duplicated Files : %d\n", dupList_count);
        killProgram();
    }
    else if (sig == SIGALRM)
    {
        printf("Number of Duplicated Files : %d\n", dupList_count);

        timer.it_value.tv_sec = 5;
        timer.it_value.tv_usec = 0;
        setitimer(ITIMER_REAL, &timer, NULL);
    }
}

void getinputs(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Invalid input USAGE: ./findeq -t [numthreads] -m [mim_size of bytes] -o[outputpath]\n DIR(directory to search)\n");
        exit(1);
    }
    for (int i = 1; i < argc - 1; i++)
    {
        if (strcmp(argv[i], "-t=") == 0)
        {
            i++;
            if (i >= argc - 1)
            {
                printf("Invalid input. -t option requires a value.\n");
                exit(1);
            }
            numThreads = atoi(argv[i]);
            if (numThreads > 65)
            {
                printf("Too Many Threads\n");
                exit(1);
            }
#ifdef DEBUG
            printf("Num Threads: %d\n", numThreads);
#endif
        }

        else if (strcmp(argv[i], "-m=") == 0)
        {
            i++;
            if (i >= argc - 1)
            {
                printf("Invalid input. -m option requires a value.\n");
                exit(1);
            }
            minSize = atoi(argv[i]);
            if (minSize < 1024)
            {
                printf("Size is Too Small\n");
                exit(1);
            }
#ifdef DEBUG
            printf("Min Bytes: %d\n", minSize);
#endif
        }

        else if (strcmp(argv[i], "-o=") == 0)
        {
            i++;
            outputPath = malloc(MAX_PATH_LENGTH);
            if (outputPath == NULL)
            {
                printf("Memory allocation for outputPath failed.\n");
                exit(1);
            }
            if (i >= argc - 1)
            {
                printf("Invalid input. -o option requires a value.\n");
                exit(1);
            }

            strcpy(outputPath, argv[i]);
#ifdef DEBUG
            printf("Output Path: %s\n", outputPath);
#endif
            //
            // change the prog to produce output via file
            file_out = 1;
        }
        else
        {
            printf("Invalid input USAGE: ./findeq -t [numthreads] -m [mim_size of bytes] -o[outputpath]\n DIR(directory to search)\n");
            exit(1);
        }
    }
}

bool Equalfiles(char *path1, char *path2)
{
    FILE *file1 = fopen(path1, "rb");
    FILE *file2 = fopen(path2, "rb");

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
    ThreadArgs *args = (ThreadArgs *)arg;
    FileList *fileList = args->FileList;
    int start = args->start;
    int end = args->end;

    for (int i = start; i < end; i++)
    {
        for (int j = 0; j < fileList->count; j++)
        {
            struct stat st1;
            struct stat st2;
            stat(fileList->paths[i], &st1);
            stat(fileList->paths[j], &st2);

            if (st1.st_size != st2.st_size)
            {
                continue;
            }
            if (i != j)
            {
                pthread_mutex_lock(&lock);
                if (!traverseTable(fileList->paths[i]) && Equalfiles(fileList->paths[i], fileList->paths[j]))
                {
#ifdef DEBUG
                    printf("File Path added to Dup List: %s\n", fileList->paths[i]);
#endif
                    addTable(fileList->paths[i]);
                    dupList_count++;
                }
                pthread_mutex_unlock(&lock);
            }
        }
    }

    pthread_mutex_lock(&lock);
    threadsCompleted++;
    if (threadsCompleted == numThreads)
    {
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&lock);

    return NULL;
}

void traverseDirectory(const char *dir, FileList *filelist)
{
    struct dirent *entry;
    DIR *dp;
    struct stat statbuf;
    dp = opendir(dir);

    if (dp == NULL)
    {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dp)))
    {

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        if (stat(path, &statbuf) != 0)
        {
            perror(path);
            return;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            traverseDirectory(path, filelist);
        }
        else if (S_ISREG(statbuf.st_mode) && statbuf.st_size >= minSize)
        {
            strncpy(filelist->paths[filelist->count++], path, MAX_PATH_LENGTH);
#ifdef DEBUG
            printf("(%d)File path added: %s\n", filelist->count, path);
#endif
            if (filelist->count > MAX_FILES - 1)
            {
                printf("Too many files \n");
                exit(1);
            }
        }
    }
    closedir(dp);
}

int main(int argc, char *argv[])
{
    clock_t start = clock();

    getinputs(argc, argv);
    const char *dir = argv[argc - 1];
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        table[i].list = NULL;
    }

    FileList filelist;
    filelist.count = 0;
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    traverseDirectory(dir, &filelist); // traverse files and save the paths to the filelist

    pthread_t threads[numThreads];
    ThreadArgs threadArgs[numThreads]; // init threads and thread args

    int filesPerThread = filelist.count / numThreads;

    for (int i = 0; i < numThreads; i++)
    {
        threadArgs[i].FileList = &filelist;
        threadArgs[i].start = i * filesPerThread;

        if (i == numThreads - 1)
        {
            threadArgs[i].end = filelist.count;
        }
        else
        {
            threadArgs[i].end = (i + 1) * filesPerThread;
        }

        pthread_create(&threads[i], NULL, compareFiles, &threadArgs[i]);
    }

    pthread_mutex_lock(&lock);
    while (threadsCompleted < numThreads)
    {
        pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);

    for (int i = 0; i < numThreads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    killProgram();

    clock_t end = clock();
    double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Execution Time: %.2f seconds\n", cpu_time_used);

    return 0;
}
