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
    FileList *fileList;
    int start;
    int end;
} ThreadArgs;

typedef struct node
{
    char path[MAX_PATH_LENGTH];
    struct node *next;
} DuplicateFileList;

typedef struct
{
    DuplicateFileList *list;
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
    DuplicateFileList *newNode = malloc(sizeof(DuplicateFileList));
    strncpy(newNode->path, path, MAX_PATH_LENGTH);
    newNode->next = NULL;

    if (table[index].list == NULL)
    {
        table[index].list = newNode;
    }
    else
    {
        DuplicateFileList *curr = table[index].list;
        while (curr->next != NULL)
        {
            curr = curr->next;
        }
        curr->next = newNode;
    }
}


bool traverseTable(const char *path)
{
    unsigned int index = hash(path);
    DuplicateFileList *curr = table[index].list;
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
    fprintf(fp, "[\n"); // Start of duplicate file list

    bool isFirstGroup = true;
    bool isFirstFile = true;

    for (int i = 0; i < TABLE_SIZE; i++)
    {
        DuplicateFileList *curr = table[i].list;
        if (curr != NULL)
        {
            if (isFirstGroup)
                isFirstGroup = false;
            else
                fprintf(fp, ",\n"); // Comma and new line for separating duplicate file groups

            fprintf(fp, "\t[\n"); // Start of duplicate file group

            isFirstFile = true;

            while (curr != NULL)
            {
                if (isFirstFile)
                    isFirstFile = false;
                else
                    fprintf(fp, ",\n"); // Comma and new line for separating duplicate files within the group

                fprintf(fp, "\t\t\"%s\"", curr->path); // Print duplicate file path
                curr = curr->next;
            }

            fprintf(fp, "\n\t]"); // End of duplicate file group
        }

        DuplicateFileList *temp;
        while (curr != NULL)
        {
            temp = curr->next;
            free(curr);
            curr = temp;
        }
    }

    fprintf(fp, "\n]\n"); // End of duplicate file list
}


void dumpTableC()
{
    bool isFirstGroup = true;

    for (int i = 0; i < TABLE_SIZE; i++)
    {
        DuplicateFileList *curr = table[i].list;
        if (curr != NULL)
        {
            if (isFirstGroup)
                isFirstGroup = false;
            else
                printf(",\n"); // Comma and new line for separating duplicate file groups

            printf("[\n"); // Start of duplicate file group

            printf("%s", curr->path); // Print duplicate file path
            curr = curr->next;

            while (curr != NULL)
            {
                printf(",\n%s", curr->path); // Print duplicate file path
                curr = curr->next;
            }

            printf("\n]"); // End of duplicate file group
        }

        DuplicateFileList *temp;
        while (curr != NULL)
        {
            temp = curr->next;
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
        printf("\nProgress: %d files processed\n", dupList_count);

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
        perror("\nProgress: %d files processed\n", dupList_count);
        killProgram();
        exit(0);
    }
    else if (sig == SIGALRM)
    {
        perror("Progress: %d files processed\n", dupList_count);
        alarm(5);
    }
}

void getInputs(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Invalid input USAGE: ./findeq -t [numthreads] -m [min_size of bytes] -o [outputpath]\n DIR (directory to search)\n");
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
            if (numThreads > MAX_THREADS)
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
            // Change the program to produce output via file
            file_out = 1;
        }
        else
        {
            printf("Invalid input USAGE: ./findeq -t [numthreads] -m [min_size of bytes] -o [outputpath]\n DIR (directory to search)\n");
            exit(1);
        }
    }
}

bool areEqualFiles(char *path1, char *path2)
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
    FileList *fileList = args->fileList;
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
                if (!traverseTable(fileList->paths[i]) && areEqualFiles(fileList->paths[i], fileList->paths[j]))
                {
                    pthread_mutex_lock(&lock);
                    #ifdef DEBUG
                        printf("File Path added to Dup List: %s\n", fileList->paths[i]);
                    #endif
                    addTable(fileList->paths[i]);
                    dupList_count++;
                    pthread_mutex_unlock(&lock);
                }
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

void traverseDirectory(const char *dir, FileList *fileList)
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
            traverseDirectory(path, fileList);
        }
        else if (S_ISREG(statbuf.st_mode) && statbuf.st_size >= minSize)
        {
            strncpy(fileList->paths[fileList->count++], path, MAX_PATH_LENGTH);
#ifdef DEBUG
            printf("(%d)File path added: %s\n", fileList->count, path);
#endif
            if (fileList->count > MAX_FILES - 1)
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
    signal(SIGINT, sig_Handler);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double elapsed;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sig_Handler;
    sigaction(SIGALRM, &sa, NULL);

    timer.it_value.tv_sec = 5;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);

    getInputs(argc, argv);
    const char *dir = argv[argc - 1];
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        table[i].list = NULL;
    }

    FileList fileList;
    fileList.count = 0;
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    traverseDirectory(dir, &fileList); // traverse files and save the paths to the fileList

    pthread_t threads[numThreads];
    ThreadArgs threadArgs[numThreads]; // init threads and thread args

    int filesPerThread = fileList.count / numThreads;

    for (int i = 0; i < numThreads; i++)
    {
        threadArgs[i].fileList = &fileList;
        threadArgs[i].start = i * filesPerThread;

        if (i == numThreads - 1)
        {
            threadArgs[i].end = fileList.count;
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
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec);
    elapsed += (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Execution Time: %.2f seconds\n", elapsed);

    killProgram();

    return 0;
}
