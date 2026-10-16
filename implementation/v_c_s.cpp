#include <iostream>
#include <string>
#include <unordered_map>
#include <set>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace std;

// ─────────────────────────────────────────────
//  COMMIT NODE  (Linked List Node)
// ─────────────────────────────────────────────
struct CommitNode {
    string  id;
    string  message;
    string  timestamp;
    string  parentId;   // ID of the previous commit ("" for root)
    CommitNode* next;   // pointer to next node in linked list

    CommitNode(const string& id,
               const string& message,
               const string& timestamp,
               const string& parentId = "")
        : id(id), message(message),
          timestamp(timestamp), parentId(parentId),
          next(nullptr) {}
};

// ─────────────────────────────────────────────
//  COMMIT LINKED LIST  (one per branch)
// ─────────────────────────────────────────────
class CommitList {
public:
    CommitNode* head;
    CommitNode* tail;
    int size;

    CommitList() : head(nullptr), tail(nullptr), size(0) {}

    // Deep-copy constructor (used when creating a new branch)
    CommitList(const CommitList& other) : head(nullptr), tail(nullptr), size(0) {
        CommitNode* cur = other.head;
        while (cur) {
            append(cur->id, cur->message, cur->timestamp, cur->parentId);
            cur = cur->next;
        }
    }

    ~CommitList() { clear(); }

    void clear() {
        CommitNode* cur = head;
        while (cur) {
            CommitNode* tmp = cur->next;
            delete cur;
            cur = tmp;
        }
        head = tail = nullptr;
        size = 0;
    }

    // Append a new commit node to the end of the list
    void append(const string& id,
                const string& message,
                const string& timestamp,
                const string& parentId = "") {
        CommitNode* node = new CommitNode(id, message, timestamp, parentId);
        if (!head) {
            head = tail = node;
        } else {
            node->parentId = tail->id;
            tail->next = node;
            tail = node;
        }
        size++;
    }

    // Check whether a commit ID already exists in this list
    bool contains(const string& id) const {
        CommitNode* cur = head;
        while (cur) {
            if (cur->id == id) return true;
            cur = cur->next;
        }
        return false;
    }

    // Find a node by commit ID
    CommitNode* find(const string& id) const {
        CommitNode* cur = head;
        while (cur) {
            if (cur->id == id) return cur;
            cur = cur->next;
        }
        return nullptr;
    }

    // Rollback: truncate list at the given commit ID (undo functionality)
    bool rollbackTo(const string& id) {
        CommitNode* cur = head;
        while (cur) {
            if (cur->id == id) {
                // Delete everything after this node
                CommitNode* toDelete = cur->next;
                while (toDelete) {
                    CommitNode* tmp = toDelete->next;
                    delete toDelete;
                    toDelete = tmp;
                    size--;
                }
                cur->next = nullptr;
                tail = cur;
                return true;
            }
            cur = cur->next;
        }
        return false; // ID not found
    }

    // Print all commits in this list
    void print(const string& prefix = "  ") const {
        CommitNode* cur = head;
        int idx = 1;
        while (cur) {
            cout << prefix << "[" << idx++ << "] "
                 << cur->id << "  \"" << cur->message << "\""
                 << "  (" << cur->timestamp << ")"
                 << (cur->next ? " -> " : "  [HEAD]") << "\n";
            cur = cur->next;
        }
        if (!head) cout << prefix << "(no commits)\n";
    }
};

// ─────────────────────────────────────────────
//  MERGE REQUEST  (queued item)
// ─────────────────────────────────────────────
struct MergeRequest {
    string sourceBranch;
    string targetBranch;
    MergeRequest* next;

    MergeRequest(const string& src, const string& tgt)
        : sourceBranch(src), targetBranch(tgt), next(nullptr) {}
};

// ─────────────────────────────────────────────
//  MERGE QUEUE  (FIFO Queue)
// ─────────────────────────────────────────────
class MergeQueue {
public:
    MergeRequest* front;
    MergeRequest* rear;
    int size;

    MergeQueue() : front(nullptr), rear(nullptr), size(0) {}

    ~MergeQueue() {
        while (!isEmpty()) dequeue();
    }

    bool isEmpty() const { return front == nullptr; }

    // Enqueue a merge request
    void enqueue(const string& src, const string& tgt) {
        MergeRequest* node = new MergeRequest(src, tgt);
        if (!rear) {
            front = rear = node;
        } else {
            rear->next = node;
            rear = node;
        }
        size++;
    }

    // Dequeue the front merge request (FIFO)
    MergeRequest dequeue() {
        if (isEmpty()) throw runtime_error("Merge queue is empty.");
        MergeRequest data = *front;
        MergeRequest* tmp = front;
        front = front->next;
        if (!front) rear = nullptr;
        delete tmp;
        size--;
        data.next = nullptr;
        return data;
    }

    MergeRequest peek() const {
        if (isEmpty()) throw runtime_error("Merge queue is empty.");
        return *front;
    }

    void print() const {
        MergeRequest* cur = front;
        int i = 1;
        while (cur) {
            cout << "  [" << i++ << "] "
                 << cur->sourceBranch << " -> " << cur->targetBranch << "\n";
            cur = cur->next;
        }
        if (!front) cout << "  (queue is empty)\n";
    }
};

// ─────────────────────────────────────────────
//  VCS ENGINE
// ─────────────────────────────────────────────
class VCS {
private:
    unordered_map<string, CommitList*> branches;
    string currentBranch;
    int commitCounter;
    MergeQueue mergeQueue;

    string generateId() {
        ostringstream ss;
        ss << "c" << setfill('0') << setw(3) << commitCounter++;
        return ss.str();
    }

    // REPAIRED: buffer enlarged to 24, format now includes full date
    string currentTime() {
        time_t now = time(nullptr);
        char buf[24];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return string(buf);
    }

    void line(char c = '-', int n = 56) {
        cout << string(n, c) << "\n";
    }

public:
    VCS() : currentBranch("main"), commitCounter(1) {
        branches["main"] = new CommitList();
        // Initial commit
        string id = generateId();
        branches["main"]->append(id, "Initial commit", currentTime());
        cout << "  VCS initialized. Branch 'main' created.\n";
        cout << "  Initial commit: " << id << "\n";
    }

    ~VCS() {
        for (auto& kv : branches) delete kv.second;
    }

    // ── COMMIT ────────────────────────────────
    void commit(const string& message) {
        if (message.empty()) {
            cout << "  [ERROR] Commit message cannot be empty.\n";
            return;
        }
        string id = generateId();
        string ts = currentTime();
        branches[currentBranch]->append(id, message, ts);
        cout << "  Committed to [" << currentBranch << "]: \""
             << message << "\"  ID=" << id
             << "  (" << ts << ")\n";
    }

    // ── CREATE BRANCH ─────────────────────────
    void createBranch(const string& name) {
        if (name.empty()) {
            cout << "  [ERROR] Branch name cannot be empty.\n";
            return;
        }
        if (branches.count(name)) {
            cout << "  [ERROR] Branch '" << name << "' already exists.\n";
            return;
        }
        // Clone current branch's linked list -> new independent chain
        branches[name] = new CommitList(*branches[currentBranch]);
        cout << "  Branch '" << name << "' created from '"
             << currentBranch << "' (" << branches[name]->size << " commits copied).\n";
    }

    // ── SWITCH BRANCH ─────────────────────────
    void switchBranch(const string& name) {
        if (!branches.count(name)) {
            cout << "  [ERROR] Branch '" << name << "' not found.\n";
            return;
        }
        currentBranch = name;
        cout << "  Switched to branch '" << name << "'.\n";
    }

    // ── ENQUEUE MERGE ─────────────────────────
    void enqueueMerge(const string& sourceBranch) {
        if (!branches.count(sourceBranch)) {
            cout << "  [ERROR] Source branch '" << sourceBranch << "' not found.\n";
            return;
        }
        if (sourceBranch == currentBranch) {
            cout << "  [ERROR] Cannot merge a branch into itself.\n";
            return;
        }
        mergeQueue.enqueue(sourceBranch, currentBranch);
        cout << "  Merge queued: '" << sourceBranch
             << "' -> '" << currentBranch << "'  (queue size=" << mergeQueue.size << ")\n";
    }

    // ── PROCESS MERGE (FIFO) ──────────────────
    void processMerge() {
        if (mergeQueue.isEmpty()) {
            cout << "  [INFO] Merge queue is empty. Nothing to process.\n";
            return;
        }

        MergeRequest req = mergeQueue.dequeue();
        cout << "  Processing merge: '" << req.sourceBranch
             << "' -> '" << req.targetBranch << "'\n";

        if (!branches.count(req.sourceBranch) || !branches.count(req.targetBranch)) {
            cout << "  [ERROR] One of the branches no longer exists. Merge aborted.\n";
            return;
        }

        CommitList* src = branches[req.sourceBranch];
        CommitList* tgt = branches[req.targetBranch];

        // ── Circular reference check ──────────
        // Collect all IDs from the target
        set<string> targetIds;
        CommitNode* cur = tgt->head;
        while (cur) { targetIds.insert(cur->id); cur = cur->next; }

        // Check if every source commit is already in target
        bool allPresent = true;
        CommitNode* s = src->head;
        while (s) {
            if (!targetIds.count(s->id)) { allPresent = false; break; }
            s = s->next;
        }
        if (allPresent) {
            cout << "  [WARN] Circular reference detected: '"
                 << req.sourceBranch << "' is already fully contained in '"
                 << req.targetBranch << "'. Merge aborted.\n";
            return;
        }

        // ── Apply unique commits via internal FIFO queue ──
        MergeQueue conflictQueue;
        s = src->head;
        while (s) {
            if (!targetIds.count(s->id)) {
                conflictQueue.enqueue(s->id, s->message);
            }
            s = s->next;
        }

        // Dequeue and apply in FIFO order
        int mergedCount = 0;
        while (!conflictQueue.isEmpty()) {
            MergeRequest item = conflictQueue.dequeue();
            tgt->append(item.sourceBranch,
                        "[merged] " + item.targetBranch,
                        currentTime());
            mergedCount++;
        }

        cout << "  Merge complete: " << mergedCount << " commit(s) applied via FIFO queue.\n";
    }

    // ── ROLLBACK (UNDO) ───────────────────────
    void rollback(const string& commitId) {
        if (!branches[currentBranch]->rollbackTo(commitId)) {
            cout << "  [ERROR] Commit '" << commitId
                 << "' not found in branch '" << currentBranch << "'.\n";
            return;
        }
        cout << "  [" << currentBranch << "] Rolled back to commit '"
             << commitId << "'. Subsequent commits discarded.\n";
    }

    // ── LOG (show all branches) ───────────────
    void log() const {
        cout << "\n  === BRANCH HISTORY ===\n";
        for (const auto& kv : branches) {
            bool isActive = (kv.first == currentBranch);
            cout << "\n  Branch: " << kv.first
                 << (isActive ? "  <-- HEAD" : "") << "\n";
            kv.second->print();
        }
    }

    // ── SHOW MERGE QUEUE ─────────────────────
    void showQueue() const {
        cout << "\n  === MERGE QUEUE (FIFO) === size=" << mergeQueue.size << "\n";
        mergeQueue.print();
    }

    // ── STATUS ────────────────────────────────
    void status() const {
        cout << "  Current branch : " << currentBranch << "\n";
        cout << "  All branches   : ";
        for (const auto& kv : branches) cout << kv.first << "  ";
        cout << "\n";
        cout << "  Merge queue    : " << mergeQueue.size << " pending\n";
        cout << "  Commits (HEAD) : " << branches.at(currentBranch)->size << "\n";
    }

    string getCurrent() const { return currentBranch; }
};

// ─────────────────────────────────────────────
//  MENU HELPERS
// ─────────────────────────────────────────────
void printMenu() {
    cout << "\n╔══════════════════════════════════════════════╗\n";
    cout << "║        VERSION CONTROL SYSTEM SIMULATOR      ║\n";
    cout << "╚══════════════════════════════════════════════╝\n";
    cout << "  1. Commit\n";
    cout << "  2. Create Branch\n";
    cout << "  3. Switch Branch\n";
    cout << "  4. Enqueue Merge\n";
    cout << "  5. Process Next Merge (FIFO)\n";
    cout << "  6. Rollback to Commit\n";
    cout << "  7. View Log (all branches)\n";
    cout << "  8. View Merge Queue\n";
    cout << "  9. Status\n";
    cout << "  0. Exit\n";
    cout << "────────────────────────────────────────────────\n";
}

void pause() {
    cout << "\n  [Press ENTER to continue...]";
    cin.ignore(1000, '\n');
    cin.get();
}

// ─────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────
int main() {
    cout << "\n  Initializing VCS...\n\n";
    VCS vcs;

    int choice;
    string input;

    do {
        printMenu();
        cout << "  Current HEAD: [" << vcs.getCurrent() << "]\n";
        cout << "  Choice: ";
        cin >> choice;
        cin.ignore();

        cout << "\n";

        switch (choice) {
            case 1: {
                cout << "  Commit message: ";
                getline(cin, input);
                vcs.commit(input);
                break;
            }
            case 2: {
                cout << "  New branch name: ";
                getline(cin, input);
                vcs.createBranch(input);
                break;
            }
            case 3: {
                cout << "  Switch to branch: ";
                getline(cin, input);
                vcs.switchBranch(input);
                break;
            }
            case 4: {
                cout << "  Source branch to merge into [" << vcs.getCurrent() << "]: ";
                getline(cin, input);
                vcs.enqueueMerge(input);
                break;
            }
            case 5: {
                vcs.processMerge();
                break;
            }
            case 6: {
                cout << "  Rollback to commit ID: ";
                getline(cin, input);
                vcs.rollback(input);
                break;
            }
            case 7: {
                vcs.log();
                break;
            }
            case 8: {
                vcs.showQueue();
                break;
            }
            case 9: {
                cout << "\n  === STATUS ===\n";
                vcs.status();
                break;
            }
            case 0: {
                cout << "  Exiting VCS Simulator. Goodbye!\n\n";
                break;
            }
            default: {
                cout << "  [ERROR] Invalid option. Please try again.\n";
                break;
            }
        }

        if (choice != 0) pause();

    } while (choice != 0);

    return 0;
}
