#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <pthread.h>
#include <openssl/md5.h>


#define MAX_THREADS 64
#define DEFAULT_BYTES 1024
#define MAX_FILES 1000
#define MAX_PATH_LENGTH 256
#define HASH_SIZE 10007 // hash table size
#define MD5_DIGEST_LENGTH_STR (MD5_DIGEST_LENGTH*2+1)

int numThreads;
int minSize = DEFAULT_BYTES;
char *outputPath;
int file_out = 0;

// Node structure to store file size and last modification
typedef struct Node {
  char path[MAX_PATH_LENGTH] ;
  struct Node * next ;
  char hash[MD5_DIGEST_LENGTH_STR] ; // 추가된 부분
} Node;

// Hash table
Node * hashTable[HASH_SIZE] ;

//Mutex for synchronization
pthread_mutex_t lock ;

typedef struct {
  char paths[MAX_FILES][MAX_PATH_LENGTH];
  int count;
} FileList;

typedef struct {
  FileList *FileList;
  int start;
  int end;
} ThreadArgs;


unsigned int hashStr(char *str) {
    unsigned int hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;

    return hash % HASH_SIZE;
}

char* hashFile(const char *filePath) {
    FILE *file = fopen(filePath, "r");
    if (file == NULL) {
        perror("Failed to open file");
        return NULL;
    }

    MD5_CTX mdContext ;
    MD5_Init(&mdContext) ;
    int bytes;
    unsigned char data[1024];

    while((bytes = fread(data,1,1024, file)) != 0 ) {
      MD5_Update(&mdContext, data, bytes);
    }

    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_Final(c, &mdContext);

    char *hash = (char*)malloc(MD5_DIGEST_LENGTH_STR);
    for(int i = 0 ; i <MD5_DIGEST_LENGTH; i ++){
      sprintf(&hash[i*2] , "%02x", (unsigned int)c[i]);
    }

    fclose(file);

    return hash;

}

//Create a node
Node * createNode(const char* path) {
  Node* node = (Node*)malloc(sizeof(Node));
  strncpy(node->path, path, MAX_PATH_LENGTH);
  node -> next = NULL ;

  return node ;
}

//Find file in hash table
bool findFile(const char* path) {
  char *hash = hashFile(path);
  if (hash == NULL) return false;
  unsigned int hashIdx = hashStr(hash);

  Node * node = hashTable[hashIdx] ;

  while (node != NULL) {
    if(strcmp(hashFile(node->path), hash) == 0 ){ // 변경된 부분
      free(hash);
      return true;
    }
    node = node -> next ;
  }

  free(hash);
  return false; 
}

//Add file to hash table
void addFile(const char* path){
  char *hash = hashFile(path);
  if (hash == NULL) return;
  unsigned int hashIdx = hashStr(hash);

  Node * node = createNode(path) ;

  if (hashTable[hashIdx] == NULL) {
    hashTable[hashIdx] = node;
  }
  else {
    Node* currentNode = hashTable[hashIdx] ;
    while (currentNode->next != NULL) {
      currentNode = currentNode->next;
    }
    currentNode->next = node ;
  }
  free(hash);
}


int numDuplicates = 0;

//Process files
void * processFiles(void* arg) {
  ThreadArgs * args = (ThreadArgs *) arg ;

  for(int i = args->start; i < args -> end ; ++ i) {
    const char* path = args -> FileList ->paths[i];

    pthread_mutex_lock(&lock) ;

    if(findFile(path)) {
      if (file_out == 1) {
        FILE *outFile = fopen(outputPath, "a");  // Open the file in append mode
        if (outFile != NULL) {
          fprintf(outFile, "Duplicate file: %s\n", path);  // Write to the file
          printf("Duplicate file: %s\n", path); // 콘솔 창에도 출력
          fclose(outFile); // Close the file
          numDuplicates ++ ;
        }
      } else {
        printf("Duplicate file: %s\n", path);
      }
    } else {
      addFile(path);
    }
    pthread_mutex_unlock(&lock);
  }

  return NULL ;
}


void getinputs(int argc, char *argv[]) {
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
      file_out = 1;
    } else {
      validInput = false;
      break;
    }
  }

  // Check if all options have been set and are valid
  if (!validInput || numThreads <= 0 || minSize <= 0 || outputPath == NULL) {
    printf("Invalid input USAGE: ./findeq -t [numthreads] -m [mim_size of bytes] -o[outputpath]\n DIR(directory to search)\n");
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

int main(int argc, char *argv[]) {

  getinputs(argc, argv);
  const char *dir = argv[argc - 1];

  FileList filelist;
  filelist.count = 0;

  traverseDirectory(dir, &filelist);

  //Iniitialize hash table
  memset(hashTable, 0 , sizeof(Node*) * HASH_SIZE) ;

  //Initialilze mutex
  pthread_mutex_init(&lock, NULL) ;

  //Divide work among threads
  pthread_t threads[MAX_THREADS] ;
  int filesPerThread = filelist.count / numThreads ;

  for(int i = 0 ; i < numThreads ; ++i ){
    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    args -> FileList = &filelist;
    args -> start = i * filesPerThread;
    args -> end = (i == numThreads - 1) ? filelist.count : (i+1) * filesPerThread ;

    if(pthread_create(&threads[i], NULL , processFiles, args) != 0){
      fprintf(stderr, "Error creating thread\n");
      return 1;
    }
  }

  //wait for all threads to finish
  for(int i = 0 ; i < numThreads ; ++i) {
    pthread_join(threads[i] , NULL);
  }

  // No duplicate files found
  if(numDuplicates == 0){
    printf("No duplicate files found\n");
  }

  //Clean
  for(int i = 0 ; i < HASH_SIZE; ++i) {
    Node* node = hashTable[i];
    while(node != NULL){
      Node * next = node -> next ;
      free(node);
      node = next;
    }
  }

  pthread_mutex_destroy(&lock);

  return 0;
}

/*
[Compile Message]
export LDFLAGS="-L/opt/homebrew/opt/openssl@3/lib"
export CPPFLAGS="-I/opt/homebrew/opt/openssl@3/include"
gcc -o findeq findeq.c -pthread -lssl -lcrypto


[Execute Message]
./findeq -t 4 -m 1024 -o output.txt ./Files  
*/