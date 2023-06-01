#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>

#define MAX_THREADS 64
#define DEFAULT_BYTES 1024
#define MAX_FILES 1000
#define MAX_PATH_LENGTH 256
#define TABLE_SIZE 1000

int numThreads;
int minSize = DEFAULT_BYTES;
char *outputPath;
int file_out = 0;
int dupList_count =0;
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
    struct node *next;

} duplicateFileList;


typedef struct
{
    duplicateFileList* list;
} HashTable;

HashTable table[TABLE_SIZE];

unsigned int hash(const char* str)
{
    unsigned int value = 0;
    for(; *str !='\0'; ++str)
        value = value * 37 + *str;
    return value & TABLE_SIZE;
}

unsigned addTable(const char * path)
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
    while(curr!=NULL)
    {
        if(strcmp(curr->path, path)==0)
            return true;
        curr = curr->next;
    }
    return false;
}

/*int int_Handler(int sig)
{
    if(sig==SIGINT)
    {
        for(int i=0;i<numThreads;i++)
        {
           pthread_join(threads[i], NULL);
        }
        printf("Number of Duplicated Files : %d\n",dupList_count);
        exit(1);
    }
}*/

void getinputs(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Invalid input USAGE: ./findeq -t [numthreads] -m [mim_size of bytes] -o[outputpath]\n DIR(directory to search)\n");
        exit(1);
    }
    for (int i = 1; i < argc - 1; i++)
    {
        if (strcmp(argv[i], "-t") == 0)
        {
            i++;
            if (i >= argc - 1)
            {
                printf("Invalid input. -t option requires a value.\n");
                exit(1);
            }
            numThreads = atoi(argv[i]);
        #ifdef DEBUG
                    printf("Num Threads: %d\n", numThreads);
        #endif
                }

        else if (strcmp(argv[i], "-m") == 0)
        {
            i++;
            if (i >= argc - 1)
            {
                printf("Invalid input. -m option requires a value.\n");
                exit(1);
            }
            minSize = atoi(argv[i]);

            #ifdef DEBUG
                        printf("Min Bytes: %d\n", minSize);
            #endif
                    }

        else if (strcmp(argv[i], "-o") == 0)
        {
            i++;
            if (i >= argc - 1)
            {
                printf("Invalid input. -o option requires a value.\n");
                exit(1);
            }
            outputPath = argv[i];

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
    /*struct stat st1;
    struct stat st2;
    stat(path1, &st1);
    stat(path2, &st2);
    if(st1.st_size!=st2.st_size)
    {
        return false;
    }*/
    
    
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
        for (int j =0; j < fileList->count; j++)
        {
            if(i != j)
            {
                pthread_mutex_lock(&lock);
                if(!traverseTable(fileList->paths[i])&& Equalfiles(fileList->paths[i],fileList->paths[j]))
                {
                    #ifdef DEBUG
                    printf("File Path added to Dup List: %s\n", fileList->paths[i]);
                    #endif
                    addTable(fileList->paths[i]);
                    dupList_count ++;
                }
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
        //perror("opendir");
        return;
    }

    while ((entry = readdir(dp)))
    {
        struct stat statbuf;
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s%s", dir, entry->d_name);

        if (stat(path, &statbuf) != 0)
        {
            //perror("stat");
            return;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            traverseDirectory(path, filelist);
        }
        else if (statbuf.st_size >= minSize)
        {
        #ifdef DEBUG
            printf("File path added: %s\n", path);
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
    outputPath = malloc(MAX_PATH_LENGTH);
    if (outputPath == NULL)
    {
        printf("Memory allocation for outputPath failed.\n");
        return 1;
    }
    for(int i=0; i<TABLE_SIZE; i++)
    {
        table[i].list =NULL;
    }

    FileList filelist;
    filelist.count = 0;
    pthread_mutex_init(&lock, NULL);

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
            threadArgs[i].end = filesPerThread;
        }
        else
        {
            threadArgs[i].end = (i + 1) * filelist.count;
        }
        pthread_create(&threads[i], NULL, compareFiles, &threadArgs[i]);
    }


    for (int i = 0; i < numThreads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    return 0;
}