#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUF_SIZE 8192
#define MAX_FILES 1024
#define MAX_PATH 1024

// struct 사용해서, 파일 path랑 category로
// 아니면 배열 두 개


// 파일 목록을 저장할 배열을 선언하고 초기화한다.
char file_list[MAX_FILES][MAX_PATH];
int file_count = 0;


// 파일 같은지
// 기준 파일 받고, list 돌면서 검사하도록 바꿔야 함

    // // 모든 파일 쌍을 비교한다.
    // for (int i = 0; i < file_count; i++) {
    //     for (int j = i + 1; j < file_count; j++) {
    //         // 두 파일이 같은지 검사한다.
    //         if (are_files_equal(file_list[i], file_list[j])) {
    //             // 두 파일이 같으면 그 사실을 출력한다.
    //             printf("'%s' and '%s' are equal\n", file_list[i], file_list[j]);
    //         }
    //     }
    // }


///////////////////////////////
// 두 파일이 같은 파일인지 비교하는 함수

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

        if (bytes_read1 != bytes_read2 || memcmp(buf1, buf2, bytes_read1) != 0) { // 최소 바이트 처리도 추가해야 함 (option m)
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

///////////////////////////////
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

int main(int argc, char **argv) {
    // 명령행 인자의 수를 확인한다. 디렉토리 경로가 주어져야 하므로 인자의 수는 반드시 2여야 한다.
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <dir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // 주어진 디렉토리에서 파일을 검색하고 목록에 추가한다.
    check_files_in_dir(argv[1], file_list, &file_count);

    // 프로그램이 정상적으로 종료되었음을 나타내는 상태 코드를 반환한다.
    return EXIT_SUCCESS;
}

