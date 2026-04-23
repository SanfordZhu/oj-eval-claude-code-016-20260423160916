#include <iostream>
#include <cstdio>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <climits>

using namespace std;

const int PAGE_SIZE = 4096;
const int MAX_KEY_LEN = 64;
const char* FILE_NAME = "bpt_data.bin";

struct FileHeader {
    int root_page;
    int next_page;
    int free_list;
    char padding[PAGE_SIZE - 3 * sizeof(int)];
};

const int HEADER_SIZE = 1 + 4 * sizeof(int);

struct NodeHeader {
    char type;
    int count;
    int parent;
    int next;
    int leftmost_child;
};

struct LeafEntry {
    char key[MAX_KEY_LEN];
    int value;
};

struct InternalEntry {
    char key[MAX_KEY_LEN];
    int child;
};

class BPTree {
private:
    FILE* fp;
    FileHeader fh;
    int leaf_fanout, internal_fanout;
    int data_size;

    void init_file() {
        fh.root_page = -1;
        fh.next_page = 1;
        fh.free_list = -1;
        memset(fh.padding, 0, sizeof(fh.padding));
        fseek(fp, 0, SEEK_SET);
        fwrite(&fh, sizeof(FileHeader), 1, fp);
        fflush(fp);
    }

    void read_header() {
        fseek(fp, 0, SEEK_SET);
        fread(&fh, sizeof(FileHeader), 1, fp);
    }

    void write_header() {
        fseek(fp, 0, SEEK_SET);
        fwrite(&fh, sizeof(FileHeader), 1, fp);
        fflush(fp);
    }

    int alloc_page() {
        int page;
        if (fh.free_list != -1) {
            page = fh.free_list;
            fseek(fp, page * PAGE_SIZE, SEEK_SET);
            NodeHeader nh;
            fread(&nh, sizeof(NodeHeader), 1, fp);
            fh.free_list = nh.next;
            write_header();
        } else {
            page = fh.next_page++;
            write_header();
        }
        return page;
    }

    void free_page(int page) {
        NodeHeader nh;
        nh.type = 0;
        nh.count = 0;
        nh.parent = -1;
        nh.next = fh.free_list;
        nh.leftmost_child = -1;
        fseek(fp, page * PAGE_SIZE, SEEK_SET);
        fwrite(&nh, sizeof(NodeHeader), 1, fp);
        char padding[PAGE_SIZE - sizeof(NodeHeader)];
        memset(padding, 0, sizeof(padding));
        fwrite(padding, sizeof(padding), 1, fp);
        fflush(fp);
        fh.free_list = page;
        write_header();
    }

    void write_node_header(int page, const NodeHeader& nh) {
        fseek(fp, page * PAGE_SIZE, SEEK_SET);
        fwrite(&nh, sizeof(NodeHeader), 1, fp);
        fflush(fp);
    }

    void read_node_header(int page, NodeHeader& nh) {
        fseek(fp, page * PAGE_SIZE, SEEK_SET);
        fread(&nh, sizeof(NodeHeader), 1, fp);
    }

    void write_leaf_entries(int page, const vector<LeafEntry>& entries) {
        fseek(fp, page * PAGE_SIZE + HEADER_SIZE, SEEK_SET);
        fwrite(entries.data(), sizeof(LeafEntry), entries.size(), fp);
        fflush(fp);
    }

    void read_leaf_entries(int page, vector<LeafEntry>& entries, int count) {
        entries.resize(count);
        fseek(fp, page * PAGE_SIZE + HEADER_SIZE, SEEK_SET);
        fread(entries.data(), sizeof(LeafEntry), count, fp);
    }

    void write_internal_entries(int page, const vector<InternalEntry>& entries) {
        fseek(fp, page * PAGE_SIZE + HEADER_SIZE, SEEK_SET);
        fwrite(entries.data(), sizeof(InternalEntry), entries.size(), fp);
        fflush(fp);
    }

    void read_internal_entries(int page, vector<InternalEntry>& entries, int count) {
        entries.resize(count);
        fseek(fp, page * PAGE_SIZE + HEADER_SIZE, SEEK_SET);
        fread(entries.data(), sizeof(InternalEntry), count, fp);
    }

    int compare_keys(const char* a, const char* b) {
        return strcmp(a, b);
    }

    int find_leaf(const char* key) {
        if (fh.root_page == -1) return -1;
        int page = fh.root_page;
        while (true) {
            NodeHeader nh;
            read_node_header(page, nh);
            if (nh.type == 1) return page;
            vector<InternalEntry> entries;
            read_internal_entries(page, entries, nh.count);
            int child = nh.leftmost_child;
            int i;
            for (i = 0; i < nh.count; i++) {
                if (compare_keys(key, entries[i].key) < 0) break;
                child = entries[i].child;
            }
            page = child;
        }
    }

    int create_leaf() {
        int page = alloc_page();
        NodeHeader nh;
        nh.type = 1;
        nh.count = 0;
        nh.parent = -1;
        nh.next = -1;
        nh.leftmost_child = -1;
        write_node_header(page, nh);
        return page;
    }

    int create_internal() {
        int page = alloc_page();
        NodeHeader nh;
        nh.type = 0;
        nh.count = 0;
        nh.parent = -1;
        nh.next = -1;
        nh.leftmost_child = -1;
        write_node_header(page, nh);
        return page;
    }

    void insert_into_leaf(int leaf_page, const char* key, int value) {
        NodeHeader nh;
        read_node_header(leaf_page, nh);
        vector<LeafEntry> entries;
        read_leaf_entries(leaf_page, entries, nh.count);
        LeafEntry new_entry;
        strncpy(new_entry.key, key, MAX_KEY_LEN - 1);
        new_entry.key[MAX_KEY_LEN - 1] = '\0';
        new_entry.value = value;
        int pos = 0;
        while (pos < nh.count && compare_keys(key, entries[pos].key) > 0) pos++;
        while (pos < nh.count && compare_keys(key, entries[pos].key) == 0 && value > entries[pos].value) pos++;
        entries.insert(entries.begin() + pos, new_entry);
        nh.count++;
        write_node_header(leaf_page, nh);
        write_leaf_entries(leaf_page, entries);
    }

    pair<int, string> split_leaf(int leaf_page) {
        NodeHeader nh;
        read_node_header(leaf_page, nh);
        vector<LeafEntry> entries;
        read_leaf_entries(leaf_page, entries, nh.count);
        int mid = nh.count / 2;
        int new_page = create_leaf();
        vector<LeafEntry> left_entries(entries.begin(), entries.begin() + mid);
        vector<LeafEntry> right_entries(entries.begin() + mid, entries.end());
        NodeHeader new_nh;
        read_node_header(new_page, new_nh);
        new_nh.parent = nh.parent;
        new_nh.next = nh.next;
        nh.next = new_page;
        nh.count = left_entries.size();
        new_nh.count = right_entries.size();
        write_node_header(leaf_page, nh);
        write_leaf_entries(leaf_page, left_entries);
        write_node_header(new_page, new_nh);
        write_leaf_entries(new_page, right_entries);
        string split_key = right_entries[0].key;
        return {new_page, split_key};
    }

    void insert_into_parent(int left_page, int right_page, const string& key) {
        if (fh.root_page == left_page) {
            int new_root = create_internal();
            NodeHeader root_nh;
            read_node_header(new_root, root_nh);
            root_nh.count = 1;
            root_nh.leftmost_child = left_page;
            vector<InternalEntry> entries(1);
            strncpy(entries[0].key, key.c_str(), MAX_KEY_LEN - 1);
            entries[0].key[MAX_KEY_LEN - 1] = '\0';
            entries[0].child = right_page;
            write_node_header(new_root, root_nh);
            write_internal_entries(new_root, entries);
            NodeHeader left_nh, right_nh;
            read_node_header(left_page, left_nh);
            read_node_header(right_page, right_nh);
            left_nh.parent = new_root;
            right_nh.parent = new_root;
            write_node_header(left_page, left_nh);
            write_node_header(right_page, right_nh);
            fh.root_page = new_root;
            write_header();
            return;
        }
        NodeHeader parent_nh;
        read_node_header(left_page, parent_nh);
        int parent_page = parent_nh.parent;
        read_node_header(parent_page, parent_nh);
        vector<InternalEntry> entries;
        read_internal_entries(parent_page, entries, parent_nh.count);
        InternalEntry new_entry;
        strncpy(new_entry.key, key.c_str(), MAX_KEY_LEN - 1);
        new_entry.key[MAX_KEY_LEN - 1] = '\0';
        new_entry.child = right_page;
        int pos = 0;
        while (pos < parent_nh.count && entries[pos].child != left_page) pos++;
        entries.insert(entries.begin() + pos + 1, new_entry);
        parent_nh.count++;
        write_node_header(parent_page, parent_nh);
        write_internal_entries(parent_page, entries);
        NodeHeader right_nh;
        read_node_header(right_page, right_nh);
        right_nh.parent = parent_page;
        write_node_header(right_page, right_nh);
        if (parent_nh.count > internal_fanout) {
            auto result = split_internal(parent_page);
            insert_into_parent(parent_page, result.first, result.second);
        }
    }

    pair<int, string> split_internal(int page) {
        NodeHeader nh;
        read_node_header(page, nh);
        vector<InternalEntry> entries;
        read_internal_entries(page, entries, nh.count);
        int mid = nh.count / 2;
        string split_key = entries[mid].key;
        int new_page = create_internal();
        vector<InternalEntry> left_entries(entries.begin(), entries.begin() + mid);
        vector<InternalEntry> right_entries(entries.begin() + mid + 1, entries.end());
        NodeHeader new_nh;
        read_node_header(new_page, new_nh);
        new_nh.parent = nh.parent;
        new_nh.leftmost_child = entries[mid].child;
        nh.count = left_entries.size();
        new_nh.count = right_entries.size();
        write_node_header(page, nh);
        write_internal_entries(page, left_entries);
        write_node_header(new_page, new_nh);
        write_internal_entries(new_page, right_entries);
        for (auto& e : right_entries) {
            NodeHeader child_nh;
            read_node_header(e.child, child_nh);
            child_nh.parent = new_page;
            write_node_header(e.child, child_nh);
        }
        return {new_page, split_key};
    }

    bool delete_from_leaf(int leaf_page, const char* key, int value) {
        NodeHeader nh;
        read_node_header(leaf_page, nh);
        vector<LeafEntry> entries;
        read_leaf_entries(leaf_page, entries, nh.count);
        int pos = -1;
        for (int i = 0; i < nh.count; i++) {
            if (compare_keys(key, entries[i].key) == 0 && entries[i].value == value) {
                pos = i;
                break;
            }
        }
        if (pos == -1) return false;
        entries.erase(entries.begin() + pos);
        nh.count--;
        write_node_header(leaf_page, nh);
        write_leaf_entries(leaf_page, entries);
        return true;
    }

    void redistribute_leaf(int left_page, int right_page) {
        NodeHeader left_nh, right_nh;
        read_node_header(left_page, left_nh);
        read_node_header(right_page, right_nh);
        vector<LeafEntry> left_entries, right_entries;
        read_leaf_entries(left_page, left_entries, left_nh.count);
        read_leaf_entries(right_page, right_entries, right_nh.count);
        int total = left_nh.count + right_nh.count;
        if (left_nh.count > right_nh.count) {
            int move = left_nh.count - total / 2;
            right_entries.insert(right_entries.begin(), left_entries.end() - move, left_entries.end());
            left_entries.erase(left_entries.end() - move, left_entries.end());
        } else {
            int move = right_nh.count - total / 2;
            left_entries.insert(left_entries.end(), right_entries.begin(), right_entries.begin() + move);
            right_entries.erase(right_entries.begin(), right_entries.begin() + move);
        }
        left_nh.count = left_entries.size();
        right_nh.count = right_entries.size();
        write_node_header(left_page, left_nh);
        write_leaf_entries(left_page, left_entries);
        write_node_header(right_page, right_nh);
        write_leaf_entries(right_page, right_entries);
    }

    void merge_leaf(int left_page, int right_page) {
        NodeHeader left_nh, right_nh;
        read_node_header(left_page, left_nh);
        read_node_header(right_page, right_nh);
        vector<LeafEntry> left_entries, right_entries;
        read_leaf_entries(left_page, left_entries, left_nh.count);
        read_leaf_entries(right_page, right_entries, right_nh.count);
        left_entries.insert(left_entries.end(), right_entries.begin(), right_entries.end());
        left_nh.count = left_entries.size();
        left_nh.next = right_nh.next;
        write_node_header(left_page, left_nh);
        write_leaf_entries(left_page, left_entries);
        free_page(right_page);
    }

    void delete_entry(int page, const char* key, int value) {
        NodeHeader nh;
        read_node_header(page, nh);
        if (nh.type == 1) {
            delete_from_leaf(page, key, value);
        } else {
            vector<InternalEntry> entries;
            read_internal_entries(page, entries, nh.count);
            int child = nh.leftmost_child;
            int idx = -1;
            for (int i = 0; i < nh.count; i++) {
                if (compare_keys(key, entries[i].key) < 0) break;
                child = entries[i].child;
                idx = i;
            }
            delete_entry(child, key, value);
            NodeHeader child_nh;
            read_node_header(child, child_nh);
            if (child_nh.count < (leaf_fanout + 1) / 2 - 1) {
                handle_underflow(page, child, idx);
            }
        }
    }

    void handle_underflow(int parent_page, int child_page, int child_idx) {
        NodeHeader parent_nh;
        read_node_header(parent_page, parent_nh);
        vector<InternalEntry> entries;
        read_internal_entries(parent_page, entries, parent_nh.count);
        int left_sibling = -1, right_sibling = -1;
        if (child_idx > 0) left_sibling = entries[child_idx - 1].child;
        if (child_idx < (int)parent_nh.count - 1) right_sibling = entries[child_idx + 1].child;
        if (child_idx == -1 && parent_nh.count > 0) right_sibling = entries[0].child;
        NodeHeader child_nh, left_nh, right_nh;
        read_node_header(child_page, child_nh);
        if (left_sibling != -1) {
            read_node_header(left_sibling, left_nh);
            if (left_nh.count > (leaf_fanout + 1) / 2) {
                redistribute_leaf(left_sibling, child_page);
                update_parent_key(parent_page, child_idx, child_page);
                return;
            }
        }
        if (right_sibling != -1) {
            read_node_header(right_sibling, right_nh);
            if (right_nh.count > (leaf_fanout + 1) / 2) {
                redistribute_leaf(child_page, right_sibling);
                update_parent_key(parent_page, child_idx + 1, right_sibling);
                return;
            }
        }
        if (left_sibling != -1) {
            merge_leaf(left_sibling, child_page);
            remove_from_parent(parent_page, child_idx);
        } else if (right_sibling != -1) {
            merge_leaf(child_page, right_sibling);
            remove_from_parent(parent_page, child_idx + 1);
        }
    }

    void update_parent_key(int parent_page, int idx, int child_page) {
        NodeHeader parent_nh;
        read_node_header(parent_page, parent_nh);
        vector<InternalEntry> entries;
        read_internal_entries(parent_page, entries, parent_nh.count);
        NodeHeader child_nh;
        read_node_header(child_page, child_nh);
        if (child_nh.type == 1) {
            vector<LeafEntry> leaf_entries;
            read_leaf_entries(child_page, leaf_entries, child_nh.count);
            if (idx >= 0 && idx < (int)parent_nh.count) {
                strncpy(entries[idx].key, leaf_entries[0].key, MAX_KEY_LEN - 1);
                entries[idx].key[MAX_KEY_LEN - 1] = '\0';
            }
        }
        write_internal_entries(parent_page, entries);
    }

    void remove_from_parent(int parent_page, int idx) {
        NodeHeader parent_nh;
        read_node_header(parent_page, parent_nh);
        vector<InternalEntry> entries;
        read_internal_entries(parent_page, entries, parent_nh.count);
        if (idx >= 0 && idx < (int)parent_nh.count) {
            entries.erase(entries.begin() + idx);
            parent_nh.count--;
        }
        write_node_header(parent_page, parent_nh);
        write_internal_entries(parent_page, entries);
        if (parent_nh.count == 0 && fh.root_page == parent_page) {
            fh.root_page = -1;
            write_header();
        }
    }

public:
    BPTree() {
        data_size = PAGE_SIZE - HEADER_SIZE;
        leaf_fanout = data_size / sizeof(LeafEntry);
        internal_fanout = data_size / sizeof(InternalEntry);
        fp = fopen(FILE_NAME, "rb+");
        if (!fp) {
            fp = fopen(FILE_NAME, "wb");
            fclose(fp);
            fp = fopen(FILE_NAME, "rb+");
            init_file();
        } else {
            read_header();
        }
    }

    ~BPTree() {
        fclose(fp);
    }

    void insert(const char* key, int value) {
        int leaf_page = find_leaf(key);
        if (leaf_page == -1) {
            leaf_page = create_leaf();
            fh.root_page = leaf_page;
            write_header();
        }
        NodeHeader nh;
        read_node_header(leaf_page, nh);
        vector<LeafEntry> entries;
        read_leaf_entries(leaf_page, entries, nh.count);
        bool exists = false;
        for (auto& e : entries) {
            if (compare_keys(key, e.key) == 0 && e.value == value) {
                exists = true;
                break;
            }
        }
        if (exists) return;
        insert_into_leaf(leaf_page, key, value);
        read_node_header(leaf_page, nh);
        if (nh.count > leaf_fanout) {
            auto result = split_leaf(leaf_page);
            insert_into_parent(leaf_page, result.first, result.second);
        }
    }

    void remove(const char* key, int value) {
        if (fh.root_page == -1) return;
        delete_entry(fh.root_page, key, value);
    }

    vector<int> find(const char* key) {
        vector<int> result;
        int leaf_page = find_leaf(key);
        if (leaf_page == -1) return result;
        while (leaf_page != -1) {
            NodeHeader nh;
            read_node_header(leaf_page, nh);
            vector<LeafEntry> entries;
            read_leaf_entries(leaf_page, entries, nh.count);
            bool found = false;
            for (auto& e : entries) {
                int cmp = compare_keys(key, e.key);
                if (cmp == 0) {
                    result.push_back(e.value);
                    found = true;
                } else if (cmp < 0) {
                    break;
                }
            }
            if (found) {
                leaf_page = nh.next;
            } else {
                break;
            }
        }
        return result;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.setf(ios::unitbuf);
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
                            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) printf(" ");

                    cout << result[j];
                }
                printf("\n");
                            }
        }
    }
    return 0;
}
