#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <pthread.h>

#define MAX_THREADS 64
#define DEFAULT_BYTES 1024
#define MAX_FILES 1000
#define MAX_PATH_LENGTH 256
#define HASH_SIZE 10007 // hash table size

int numThreads;
int numDuplicates = 0;
int minSize = DEFAULT_BYTES;
char *outputPath;
FILE *outFile;
pthread_mutex_t lock;

// Node structure 
typedef struct Node {
  char path[MAX_PATH_LENGTH];
  struct Node *next;
  off_t size; // file size
} Node;

typedef struct {
  char paths[MAX_FILES][MAX_PATH_LENGTH];
  int count;
} FileList;

typedef struct {
  FileList *FileList;
  int start;
  int end;
} ThreadArgs;

typedef struct {
  char** data ;
  size_t size ;
  size_t capacity ;
} StringSet ;

// Hash table
Node *hashTable[HASH_SIZE];

StringSet* createSet(){
  StringSet * set = (StringSet*)malloc(sizeof(StringSet));

  set->data = (char**)malloc(sizeof(char*)*10);
  set->size = 0 ;
  set->capacity = 10;
  return set;
}

void addToSet(StringSet* set, const char* str){
  if(set->size == set->capacity){
    set->capacity *= 2;
    set-> data = (char**)realloc(set->data, sizeof(char*)* set->capacity);
  }
  set->data[set->size++] = strdup(str);
}

bool isInSet(const StringSet* set, const char* str){
  for(size_t i = 0 ; i<set->size; ++i){
    if(strcmp(set -> data[i],str) == 0){
      return true;
    }
  }
  return false;
}

//Create a node
Node *createNode(const char *path, off_t size) {
  Node *node = (Node *)malloc(sizeof(Node));
  strncpy(node->path, path, MAX_PATH_LENGTH);
  node->next = NULL;
  node->size = size;
  return node;
}

void getInputs(int argc, char *argv[]) {
  numThreads = 0;
  minSize = 0;
  outputPath = NULL;
  bool validInput = true;

  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "-t") == 0) {
      i++;
      numThreads = atoi(argv[i]);
    } else if (strcmp(argv[i], "-m") == 0) {
      i++;
      minSize = atoi(argv[i]);
    } else if (strcmp(argv[i], "-o") == 0) {
      i++;
      outputPath = argv[i];
    } else {
      validInput = false;
      break;
    }
  }

  if(outputPath != NULL) {
    outFile = fopen(outputPath, "w");
    if(outFile == NULL){
      printf("Failed to open output file.\n");
      exit(EXIT_FAILURE);
    }
  }

  if (!validInput || numThreads <= 0 || minSize <= 0 || outputPath == NULL) {
    printf("Invalid input USAGE: ./findeq -t [numthreads] -m [min_size of bytes] -o [outputpath]\n DIR (directory to search)\n");
    exit(EXIT_FAILURE);
  }


}

void traverseDirectory(const char *dir, FileList *filelist) {
  struct dirent *entry;
  DIR *dp;

  dp = opendir(dir);
  if (dp == NULL) {
    perror("opendir");
    return;
  }

  while ((entry = readdir(dp))) {
    struct stat statbuf;
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
      continue;

    char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
    stat(path, &statbuf);

    if (S_ISDIR(statbuf.st_mode)) {
      traverseDirectory(path, filelist);
    } else {
      strncpy(filelist->paths[filelist->count++], path, MAX_PATH_LENGTH);
      printf("File added to the list: %s\n" , path);
    }
  }

  closedir(dp);
}

int compare_files(const char *file1, const char *file2)
{
    FILE *fp1 = fopen(file1, "rb");
    FILE *fp2 = fopen(file2, "rb");
    if (!fp1 || !fp2)
    {
        if (fp1) fclose(fp1);
        if (fp2) fclose(fp2);
        return 0; // 파일 열기 실패
    }

    int result = 1; // 파일이 같다고 가정
    while (!feof(fp1) && !feof(fp2))
    {
        char buf1[1024], buf2[1024];
        size_t size1 = fread(buf1, 1, sizeof(buf1), fp1);
        size_t size2 = fread(buf2, 1, sizeof(buf2), fp2);

        if (size1 != size2 || memcmp(buf1, buf2, size1) != 0)
        {
            result = 0; // 파일이 다름
            break;
        }
    }

    if (result == 1 && (feof(fp1) != feof(fp2)))
        result = 0; // 파일 크기가 다름

    fclose(fp1);
    fclose(fp2);
    return result;
}


StringSet* duplicateSet ;

void *processFiles(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    duplicateSet = createSet();

    for (int i = args->start; i < args->end; ++i) {
        const char *path = args->FileList->paths[i];

        struct stat statbuf;
        stat(path, &statbuf);
        unsigned int hashIdx = statbuf.st_size % HASH_SIZE;


        Node *current = hashTable[hashIdx];
        while (current != NULL) {
          if(statbuf.st_size == current->size){
            pthread_mutex_lock(&lock);
            if(compare_files(path,current->path) == 1){
              if(!isInSet(duplicateSet,path)){
                addToSet(duplicateSet,path);
                printf("Duplicate file: %s\n" , path);
                if(outFile != NULL) {
                  fprintf(outFile,"Duplicate file: %s\n" , path);
                }
                numDuplicates++;
              }
            }
            pthread_mutex_unlock(&lock);
          }
          current = current->next;

        }
        if (current == NULL) {
            Node *node = createNode(path, statbuf.st_size);
            node->next = hashTable[hashIdx];
            hashTable[hashIdx] = node;
        }
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
  getInputs(argc, argv);
  const char *dir = argv[argc - 1];

  FileList filelist;
  filelist.count = 0;
  traverseDirectory(dir, &filelist);

  //Iniitialize hash table
  memset(hashTable, 0, sizeof(Node*) * HASH_SIZE);

  //Initialilze mutex
  pthread_mutex_init(&lock, NULL);

  //Divide work among threads
  pthread_t threads[MAX_THREADS];
  int filesPerThread = filelist.count / numThreads;

  for (int i = 0; i < numThreads; ++i) {
    ThreadArgs *args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    args->FileList = &filelist;
    args->start = i * filesPerThread;
    args->end = (i == numThreads - 1) ? filelist.count : (i + 1) * filesPerThread;

    if (pthread_create(&threads[i], NULL, processFiles, args) != 0) {
      fprintf(stderr, "Error creating thread\n");
      return 1;
    }
  }

  //Wait for all threads to finish
  for (int i = 0; i < numThreads; ++i) {
    pthread_join(threads[i], NULL);
  }

  //No duplicate files found
  if (numDuplicates == 0) {
    printf("No duplicate files found\n");
  } else {
    printf("Total duplicate files found: %d\n", numDuplicates);
  }

  //Destroy the lock
  pthread_mutex_destroy(&lock);

  if (outFile != NULL) {
    fclose(outFile);
  }

  return 0;
}

