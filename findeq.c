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

typedef struct {
  char hash[MAX_PATH_LENGTH];
  StringSet* paths;
} HashPath;

HashPath* hashTable[HASH_SIZE];

StringSet* createSet(){
  StringSet * set = (StringSet*)malloc(sizeof(StringSet));
  set->data = (char**)malloc(sizeof(char*)*10);
  set->size = 0 ;
  set->capacity = 10;
  return set;
}

HashPath *createHashPath(unsigned long hash) {
  HashPath *newHashPath = (HashPath *)malloc(sizeof(HashPath));
  sprintf(newHashPath->hash, "%lu", hash);
  newHashPath->paths = createSet();
  return newHashPath;
}

void addToSet(StringSet* set, const char* str){
  if(set->size == set->capacity){
    set->capacity *= 2;
    set->data = (char**)realloc(set->data, sizeof(char*)* set->capacity);
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

unsigned long calculate_hash(const char *filename) {
    char data[1024];
    unsigned long hash = 5381;
    FILE *inFile = fopen(filename, "rb");
    int bytes;

    while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
        for(int i = 0; i < bytes; ++i) {
            hash = ((hash << 5) + hash) + data[i];
        }
    }

    fclose(inFile);
    return hash;
}

unsigned int hash_function(unsigned long hash) {
    return hash % HASH_SIZE;
}

void *processFiles(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;

    for (int i = args->start; i < args->end; ++i) {
        const char *path = args->FileList->paths[i];

        // Calculate hash value
        unsigned long hash = calculate_hash(path);
        unsigned int hashIdx = hash_function(hash) % HASH_SIZE;

        pthread_mutex_lock(&lock);

        HashPath *current = hashTable[hashIdx];
        if (current == NULL) {
            // This is a new file, so create a new hash path
            HashPath *newHashPath = createHashPath(hash);
            addToSet(newHashPath->paths, path);
            hashTable[hashIdx] = newHashPath;
        } else {
            // This file already exists, so add the path to the existing hash path
            addToSet(current->paths, path);
        }

        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

void printDuplicates() {
    for (int i = 0; i < HASH_SIZE; ++i) {
        HashPath* current = hashTable[i];
        if (current != NULL && current->paths->size > 1) {
            printf("[\n");
            for (size_t j = 0; j < current->paths->size; ++j) {
                printf("%s,\n", current->paths->data[j]);
            }
            printf("]\n");
        }
    }
}

int main(int argc, char *argv[]) {
  getInputs(argc, argv);
  const char *dir = argv[argc - 1];

  FileList filelist;
  filelist.count = 0;
  // Traverse directory and fill file list
  traverseDirectory(dir, &filelist);

  pthread_t threads[MAX_THREADS];
  ThreadArgs args[MAX_THREADS];

  int filesPerThread = filelist.count / numThreads;
  for (int i = 0; i < numThreads; ++i) {
    args[i].FileList = &filelist;
    args[i].start = i * filesPerThread;
    args[i].end = (i == numThreads - 1) ? filelist.count : (i + 1) * filesPerThread;

    if (pthread_create(&threads[i], NULL, processFiles, (void *)&args[i]) != 0) {
      printf("Error creating thread\n");
      return EXIT_FAILURE;
    }
  }

  // Wait for all threads to finish
  for (int i = 0; i < numThreads; ++i) {
    pthread_join(threads[i], NULL);
  }

  printDuplicates();

  if(outFile != NULL) {
    fclose(outFile);
  }

  pthread_mutex_destroy(&lock);

  return EXIT_SUCCESS;
}
