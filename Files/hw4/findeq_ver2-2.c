#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h> // 스레드 처리를 위한 라이브러리 포함
#include <time.h>

#define BUF_SIZE 8192
#define MAX_FILES 1024
#define MAX_PATH 1024
#define MAX_THREADS 64 // 스레드 최대 개수를 정의

// 여러개의 인자를 스레드로 보내기 위한 구조체 정의
typedef struct {
    char (*file_list)[MAX_PATH]; // 파일 리스트
    int file_count; // 파일 개수
    int thread_id; // 스레드 ID
    int num_threads; // 스레드 개수
} ThreadArgs;

typedef struct {
    char** files;
    int size;
    int capacity;
} FileList;

FileList fileList; // 파일 리스트 선언


// are_files_equal 함수는 파일 하나와 파일 목록 전체를 비교합니다.
int are_files_equal(const char *path1, const char *path2) {
    // 두 파일을 열고, 파일 디스크립터를 얻는다.
    FILE *file1 = fopen(path1, "rb");
    if (file1 == NULL) {
        perror(path1);
        return 0;
    }

    FILE *file2 = fopen(path2, "rb");
    if (file2 == NULL) {
        perror(path2);
        fclose(file1);
        return 0;
    }

    // 두 파일의 크기를 비교한다.
    fseek(file1, 0, SEEK_END);
    fseek(file2, 0, SEEK_END);
    long size1 = ftell(file1);
    long size2 = ftell(file2);

    if (size1 != size2) {
        // 크기가 다르면 내용도 다르다고 판단한다.
        fclose(file1);
        fclose(file2);
        return 0;
    }

    // 파일의 크기가 같으면, 각 바이트를 비교한다.
    rewind(file1);
    rewind(file2);
    char buf1[BUF_SIZE], buf2[BUF_SIZE];
    size_t bytes_read1, bytes_read2;
    do {
        bytes_read1 = fread(buf1, 1, BUF_SIZE, file1);
        bytes_read2 = fread(buf2, 1, BUF_SIZE, file2);

        if (bytes_read1 != bytes_read2 || memcmp(buf1, buf2, bytes_read1) != 0) {
            // 바이트 수가 다르거나, 메모리 비교 결과가 다르면 두 파일은 다르다.
            fclose(file1);
            fclose(file2);
            return 0;
        }
    } while (bytes_read1 > 0);

    // 파일을 비교한 후에는 반드시 닫아준다.
    fclose(file1);
    fclose(file2);

    // 파일의 크기와 모든 바이트가 동일하므로 두 파일은 같다.
    return 1;
}

void *compare_files_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg; // arg를 ThreadArgs 형으로 변환

    printf("Thread %d started\n", args->thread_id);  // 스레드가 시작될 때 메시지 출력

    // 이 스레드가 담당할 파일 범위를 계산
    int start = args->thread_id * args->file_count / args->num_threads;
    int end = (args->thread_id + 1) * args->file_count / args->num_threads;

    // for (int i = start; i < end; i++) {
    //     for (int j = i + 1; j < args->file_count; j++) { // j를 i 이후의 파일을 가리키도록 초기화
    //         // 두 파일이 같은지 검사한다.
    //         if (are_files_equal(args->file_list[i], args->file_list[j])) {
    //             // 두 파일이 같으면 그 사실을 출력한다.
    //             printf("'%s' and '%s' are equal, thread number: %d\n", args->file_list[i], args->file_list[j],args->thread_id);
    //         }
    //     }
    // }

    for (int i = start; i < end; i++) {
        for (int j = i + 1; j < args->file_count; j++) { // j를 i 이후의 파일을 가리키도록 초기화
        if (are_files_equal(args->file_list[i], args->file_list[j])) {
            printf("'%s' and '%s' are equal, thread number: %d\n",  args->file_list[i], args->file_list[j], args->thread_id);
            add_file(&fileList, args->file_list[j]);  // 파일 목록에 파일 추가
        }
    }

    printf("Thread %d finished\n", args->thread_id);  // 스레드 작업 완료 시 메시지 출력

    return NULL;
}

// 지정된 디렉토리 내의 모든 파일을 검사하고 파일 목록에 추가하는 함수
void check_files_in_dir(const char *dir_path, char (*file_list)[MAX_PATH], int *file_count) {
    // opendir 함수를 통해 디렉토리를 열고, 이 디렉토리의 스트림 정보를 얻는다.
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror(dir_path);
        return;
    }

    // readdir 함수를 통해 디렉토리 내부의 파일 또는 디렉토리를 하나씩 읽는다.
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // "."과 ".."은 현재 디렉토리와 상위 디렉토리를 가리키므로, 이를 건너뛴다.
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 전체 파일 경로를 만든다.
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", dir_path, entry->d_name);

        // stat 함수를 통해 해당 파일의 정보를 얻는다.
        struct stat statbuf;
        if (stat(full_path, &statbuf) == -1) {
            perror(full_path);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // 만약 디렉토리라면, 재귀적으로 이 함수를 호출하여 그 디렉토리 내부를 탐색한다.
            check_files_in_dir(full_path, file_list, file_count);
        } else if (S_ISREG(statbuf.st_mode)) {
            // 만약 일반 파일이라면, 해당 파일 경로를 파일 목록에 추가한다.
            strncpy(file_list[*file_count], full_path, MAX_PATH);
            (*file_count)++;
        }
    }

    // opendir로 열었던 디렉토리는 반드시 closedir을 통해 닫아준다.
    closedir(dir);
}

void initialize_file_list(FileList* fileList) {
    fileList->files = NULL;
    fileList->size = 0;
    fileList->capacity = 0;
}

void add_file(FileList* fileList, const char* filename) {
    if (fileList->size >= fileList->capacity) {
        int newCapacity = (fileList->capacity == 0) ? 1 : fileList->capacity * 2;
        char** newFiles = realloc(fileList->files, newCapacity * sizeof(char*));
        if (newFiles == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            return;
        }
        fileList->files = newFiles;
        fileList->capacity = newCapacity;
    }

    fileList->files[fileList->size] = strdup(filename);
    fileList->size++;
}

void print_file_list(const FileList* fileList) {
    for (int i = 0; i < fileList->size; i++) {
        printf("%s\n", fileList->files[i]);
    }
}

int main(int argc, char **argv) {
    clock_t start, end;
    double cpu_time_used;

    start = clock();

    initialize_file_list(&fileList);

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <dir> <num_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // 파일 리스트를 저장할 배열 선언
    char file_list[MAX_FILES][MAX_PATH];
    int file_count = 0; // 파일 개수를 0으로 초기화

    // 디렉토리에서 파일을 찾고 리스트에 추가
    check_files_in_dir(argv[1], file_list, &file_count);

    // 스레드 개수 결정
    int num_threads = atoi(argv[2]);
    
    if (num_threads <= 0 || num_threads > MAX_THREADS) {
        fprintf(stderr, "Invalid number of threads: %d\n", num_threads);
        return EXIT_FAILURE;
    }

    // 스레드 객체와 인자를 저장할 배열 생성
    pthread_t threads[MAX_THREADS];
    ThreadArgs thread_args[MAX_THREADS];

    // 각 스레드 생성 및 시작
    for (int i = 0; i < num_threads; i++) {
        thread_args[i].file_list = file_list;
        thread_args[i].file_count = file_count;
        thread_args[i].thread_id = i;
        thread_args[i].num_threads = num_threads;

        if (pthread_create(&threads[i], NULL, compare_files_thread, &thread_args[i]) != 0) {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
    }

    // 모든 스레드가 끝날 때까지 기다림
    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            return EXIT_FAILURE;
        }
    }

    end = clock();

    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Execution time: %f seconds\n", cpu_time_used);

    return EXIT_SUCCESS;
}
