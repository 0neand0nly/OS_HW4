/*
-- 참고 문헌 -- 
ascii 코드 표(블로그) : https://stepbystep1.tistory.com/10
C++ 클래스 배열(블로그) : https://m.blog.naver.com/PostView.naver?isHttpsRedirect=true&blogId=nanocode-&logNo=221114088412
힙 정렬(블로그) : https://coderkoo.tistory.com/10
교과서 Ch6 page 10
*/

#include <iostream>
#include <string>
using namespace std;

int idx = 1; // queue의 인덱스 관리할 변수

class Node {
public:
    string name;
    int id;
    string school;
};

// min-heapify
void minHeap(Node members[], int index) {
    int parent = index;
    int left = parent*2;
    int right = parent*2 + 1;

    int smallest = parent;

    if (left < idx && members[left].id < members[parent].id) // 왼쪽 child가 더 작으면 smallest 갱신
        smallest = left;

    if (right < idx && members[right].id < members[smallest].id) // 오른쪽 child가 더 작으면 smallest 갱신
        smallest = right;

    if (smallest != index) {
        Node tmp = members[index];
        members[index] = members[smallest];
        members[smallest] = tmp;
        minHeap(members, smallest);
    }
}

// 새로운 node Insert
void INSERT(Node members[], Node x) {
    members[idx].name = x.name;
    members[idx].id = x.id;
    members[idx].school = x.school;
    idx++;
    cout << "New element [" << x.name << ", " << x.id << ", " << x.school << "] is inserted.\n";
}

// 최솟값 return
Node MINIMUM(Node members[]) {
    return members[1];
}

// 가장 작은 노드 삭제
void EXTRACT_MIN(Node members[]) {
    Node root = MINIMUM(members);
    cout << "[" << root.name << ", " << root.id << ", " << root.school << "] " << "is deleted.\n";
}

// 노드의 값 변경하기
Node DECREASE_KEY(Node members[], int x, int k) {
    members[x].id = k;
    return members[x];
}

// queue print
void PRINT(Node members[]) {
    for (int i = 1; i < idx; i++)
        cout << "[" << members[i].name << ", " <<  members[i].id << ", " << members[i].school << "] ";
    cout << '\n';
}


int main() {
    Node members[31];
    string input;

    do {
        // 초기 메세지
        cout << "\n*********** MENU ****************\nI : Insert new element into queue.\nD : Delete element with smallest key from queue. \nC : Decrease key of element in queue.\nP : Print out all elements in queue. \nQ : Quit\n\nChoose menu: ";
        cin >> input;
        cin.ignore(); // 버퍼 비우기

        // Insert
        if (input.compare("I") == 0) {
            // 넣을 정보 입력받기
            Node element;

            // name
            cout << "Enter name of element: ";
            getline(cin, element.name);
            
            // id 
            string inputid;
            string finalInput; // 최종적으로 유효값 저장되는 변수

            while(1) {
                cout << "Enter id of element: ";
                getline(cin, inputid);

                string tmp; // 유효한 숫자 임시저장할 변수

                int notZeroIdx; 
                for (int i = 0; i < tmp.length(); i++) {
                    if (inputid[i] != '0') {
                        notZeroIdx = i;
                        break;
                    }
                }
                
                for (int i = 0; i < inputid.length(); i++) { // 숫자가 아닌 다른 기호 or 띄어쓰기 일 때 continue -> 유효한 값만 tmp에 저장
                    if (i < notZeroIdx && inputid[i] == '0') continue;
                    if (inputid[i] < 48 || inputid[i] > 59) continue;
                    else if (inputid[i] == ' ') continue;

                    tmp += inputid[i];
                }
                
                cout << "tmp len: " << tmp.length()  << '\n';
                if (tmp.length() == 4) {
                    finalInput = tmp;
                    break;
                }

                else cout << "not 4 digit.\n";
            }

            element.id = stoi(finalInput); // tmp를 int값으로 변환하여 id에 저장

            // school
            cout << "Enter school of element: ";
            cin >> element.school;
            // school 형식 통일 (Handong, Doodong, Sedong)
            if (element.school[0] > 90) element.school[0] -= 32;
            for (int i = 1; i < element.school.length(); i++) {
                if(element.school[i] < 97) element.school[i] += 32;
            }

            // 입력받은 class 객체를 insert
            INSERT(members, element);
        }

        // Delete
        else if (input.compare("D") == 0) {
            EXTRACT_MIN(members);
            members[1] = members[--idx];
            for (int i = (idx-1)/2; i > 0; i--) // heap 재배열
                minHeap(members, i);
        }

        // Decrease
        else if (input.compare("C") == 0) {
            int x, k;
            cout << "Enter index of element: ";
            cin >> x;
            cout << "Enter id value: ";
            cin >> k;
            
            Node tmp = DECREASE_KEY(members, x, k);
            members[x] = tmp;

            for (int i = (idx-1)/2; i > 0; i--) // heap 재배열
                minHeap(members, i);
        }

        // Print
        else if (input.compare("P") == 0) {
            for (int i = (idx-1)/2; i > 0; i--) // heap 재배열
                minHeap(members, i);
            PRINT(members);
        }

    } while(input.compare("Q") != 0);

    cout << "Thank you. Bye!\n";

}
