#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

const char* FILE_NAME = "bpt_data.bin";

struct DataEntry {
    string key;
    int value;
    bool operator<(const DataEntry& other) const {
        if (key != other.key) return key < other.key;
        return value < other.value;
    }
};

class BPTree {
private:
    map<string, set<int>> data;

public:
    BPTree() {
        ifstream file(FILE_NAME, ios::binary);
        if (file.is_open()) {
            int count;
            file.read((char*)&count, sizeof(count));
            for (int i = 0; i < count; i++) {
                int key_len;
                file.read((char*)&key_len, sizeof(key_len));
                char key_buf[256];
                file.read(key_buf, key_len);
                key_buf[key_len] = '\0';
                int value_count;
                file.read((char*)&value_count, sizeof(value_count));
                set<int> values;
                for (int j = 0; j < value_count; j++) {
                    int val;
                    file.read((char*)&val, sizeof(val));
                    values.insert(val);
                }
                string k(key_buf);
                data[k] = values;
            }
            file.close();
        }
    }

    ~BPTree() {
        ofstream file(FILE_NAME, ios::binary);
        int count = data.size();
        file.write((char*)&count, sizeof(count));
        for (auto& p : data) {
            int key_len = p.first.size();
            file.write((char*)&key_len, sizeof(key_len));
            file.write(p.first.c_str(), key_len);
            int value_count = p.second.size();
            file.write((char*)&value_count, sizeof(value_count));
            for (int val : p.second) {
                file.write((char*)&val, sizeof(val));
            }
        }
        file.close();
    }

    void insert(const char* key, int value) {
        string k(key);
        data[k].insert(value);
    }

    void remove(const char* key, int value) {
        auto it = data.find(key);
        if (it != data.end()) {
            it->second.erase(value);
            if (it->second.empty()) {
                data.erase(it);
            }
        }
    }

    vector<int> find(const char* key) {
        vector<int> result;
        auto it = data.find(key);
        if (it != data.end()) {
            for (int val : it->second) {
                result.push_back(val);
            }
        }
        return result;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout << unitbuf;
    BPTree tree;
    int n;
    cin >> n;
    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;
        if (cmd == "insert") {
            string key;
            int value;
            cin >> key >> value;
            tree.insert(key.c_str(), value);
        } else if (cmd == "delete") {
            string key;
            int value;
            cin >> key >> value;
            tree.remove(key.c_str(), value);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            vector<int> result = tree.find(key.c_str());
            if (result.empty()) {
                printf("null\n");
                fflush(stdout);
            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) printf(" ");
                    printf("%d", result[j]);
                }
                printf("\n");
                cout.flush();
            }
        }
    }
    return 0;
}
