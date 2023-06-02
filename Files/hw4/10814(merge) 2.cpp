#include <iostream>
using namespace std;

struct Mem {
    int age;
    string name;
};

Mem mem[100000]; // 입력 받는 곳
Mem arr[100000]; // 정렬된 것을 넣는 곳

void merge (int s, int m, int e){
    int i = s; 
    int k = s; // Sorting 시작되는 첫 index
    int j = m+1; // 가운데+1번째 index

    while (i <= m && j <= e) { // 비교하면서 원소 넣기
        if (mem[i].age <= mem[j].age)
            arr[k++] = mem[i++];
        else arr[k++] = mem[j++];
    }

    if (i > m) { // 원소가 남으면 그대로 배열에 넣음 (이미 정렬되어 있으므로)
        for (int z = j; z <= e; z++) 
            arr[k++] = mem[z];
    } else {
        for (int z = i; z <= m; z++) 
            arr[k++] = mem[z];
    }

    for(int z = s; z <= e; z++) {
        mem[z] = arr[z];
    }
    
}

void mergesort(int start, int end) {
    if (start < end) {
        int mid = (start+end)/2;
        mergesort(start, mid);
        mergesort(mid+1, end);
        merge(start, mid, end);
    }
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int num;
    cin >> num;

    for (int i = 0; i < num; i++) {
        cin >> mem[i].age;
        cin >> mem[i].name;
    }

    mergesort(0,num-1);

    for (int i = 0; i < num; i++) {
        cout << mem[i].age << ' ' << mem[i].name << '\n';
    }

}