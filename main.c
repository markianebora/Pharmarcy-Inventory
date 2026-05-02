/*
 * ============================================================
 *  PHARMACY INVENTORY MANAGEMENT SYSTEM (PIMS)
 *  CC104: Data Structures and Algorithms - Final Project
 *  Central Bicol State University of Agriculture
 * ============================================================
 *
 *  Data Structures Used:
 *    1. Hash Table        - Fast drug lookup by name (O(1) avg)
 *    2. Doubly Linked List- Core inventory storage
 *    3. Min-Heap          - Expiry date priority queue
 *    4. Stack             - Undo/action history log
 *
 *  Algorithms Used:
 *    1. Merge Sort        - Sort inventory by expiry/name/stock
 *    2. Binary Search     - Search sorted inventory quickly
 *    3. Hashing (djb2)    - Hash function for drug name lookup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define MAX_NAME        64
#define MAX_CATEGORY    32
#define MAX_SUPPLIER    64
#define HASH_SIZE       101  
#define HEAP_CAPACITY   512
#define STACK_CAPACITY  100
#define LOW_STOCK_THRESHOLD 20


typedef struct {
    int day;
    int month;
    int year;
} Date;

/* Convert Date to integer YYYYMMDD for easy comparison */
int date_to_int(Date d) {
    return d.year * 10000 + d.month * 100 + d.day;
}

/* Return days remaining until expiry from today */
int days_until_expiry(Date expiry) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    Date today = { t->tm_mday, t->tm_mon + 1, t->tm_year + 1900 };

    /* Rough day count: years * 365 + months * 30 + days */
    int exp_days = expiry.year * 365 + expiry.month * 30 + expiry.day;
    int tod_days = today.year * 365 + today.month * 30 + today.day;
    return exp_days - tod_days;
}

void print_date(Date d) {
    printf("%04d-%02d-%02d", d.year, d.month, d.day);
}

int parse_date(const char *str, Date *d) {
    return sscanf(str, "%d-%d-%d", &d->year, &d->month, &d->day) == 3;
}

typedef struct DrugNode {
    int   id;
    char  name[MAX_NAME];
    char  category[MAX_CATEGORY];
    char  supplier[MAX_SUPPLIER];
    int   quantity;
    float price;
    Date  expiry;
    int   heap_index;       
    struct DrugNode *prev; 
    struct DrugNode *next;
    struct DrugNode *hash_next; 
} DrugNode;

/* ============================================================
 *  DATA STRUCTURE 1: DOUBLY LINKED LIST (Core Inventory)
 *  - Stores all drug records
 *  - O(1) insertion at head/tail
 *  - O(n) traversal for display
 * ============================================================ */
typedef struct {
    DrugNode *head;
    DrugNode *tail;
    int       count;
    int       next_id;
} LinkedList;

void list_init(LinkedList *list) {
    list->head     = NULL;
    list->tail     = NULL;
    list->count    = 0;
    list->next_id  = 1;
}

void list_insert(LinkedList *list, DrugNode *node) {
    node->prev = list->tail;
    node->next = NULL;
    if (list->tail) list->tail->next = node;
    else            list->head       = node;
    list->tail = node;
    list->count++;
}

void list_remove(LinkedList *list, DrugNode *node) {
    if (node->prev) node->prev->next = node->next;
    else            list->head       = node->next;
    if (node->next) node->next->prev = node->prev;
    else            list->tail       = node->prev;
    list->count--;
}

/* ============================================================
 *  DATA STRUCTURE 2: HASH TABLE (Fast Drug Lookup by Name)
 *  - Separate chaining for collision resolution
 *  - Average O(1) lookup, insert, delete
 * ============================================================ */
typedef struct {
    DrugNode *buckets[HASH_SIZE];
} HashTable;

void hash_init(HashTable *ht) {
    memset(ht->buckets, 0, sizeof(ht->buckets));
}

unsigned int hash_djb2(const char *key) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*key++))
        hash = ((hash << 5) + hash) + c; 
    return hash % HASH_SIZE;
}

void normalize_key(const char *src, char *dst) {
    int j = 0;
    for (int i = 0; src[i] && j < MAX_NAME - 1; i++) {
        dst[j++] = tolower((unsigned char)src[i]);
    }
    dst[j] = '\0';
}

void hash_insert(HashTable *ht, DrugNode *node) {
    char key[MAX_NAME];
    normalize_key(node->name, key);
    unsigned int idx = hash_djb2(key);
    node->hash_next = ht->buckets[idx];
    ht->buckets[idx] = node;
}

DrugNode *hash_search(HashTable *ht, const char *name) {
    char key[MAX_NAME];
    normalize_key(name, key);
    unsigned int idx = hash_djb2(key);
    DrugNode *cur = ht->buckets[idx];
    while (cur) {
        char cur_key[MAX_NAME];
        normalize_key(cur->name, cur_key);
        if (strcmp(cur_key, key) == 0) return cur;
        cur = cur->hash_next;
    }
    return NULL;
}

void hash_remove(HashTable *ht, DrugNode *node) {
    char key[MAX_NAME];
    normalize_key(node->name, key);
    unsigned int idx = hash_djb2(key);
    DrugNode **cur = &ht->buckets[idx];
    while (*cur) {
        if (*cur == node) {
            *cur = node->hash_next;
            return;
        }
        cur = &(*cur)->hash_next;
    }
}

/* ============================================================
 *  DATA STRUCTURE 3: MIN-HEAP (Expiry Priority Queue)
 *  - Minimum expiry date is always at the root (index 0)
 *  - Allows O(log n) insert/extract
 *  - Used for "What expires next?" queries instantly
 * ============================================================ */
typedef struct {
    DrugNode *data[HEAP_CAPACITY];
    int        size;
} MinHeap;

void heap_init(MinHeap *h) {
    h->size = 0;
}

void heap_swap(MinHeap *h, int a, int b) {
    DrugNode *tmp = h->data[a];
    h->data[a] = h->data[b];
    h->data[b] = tmp;

    h->data[a]->heap_index = a;
    h->data[b]->heap_index = b;
}

void heap_sift_up(MinHeap *h, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (date_to_int(h->data[parent]->expiry) > date_to_int(h->data[i]->expiry)) {
            heap_swap(h, parent, i);
            i = parent;
        } else break;
    }
}

void heap_sift_down(MinHeap *h, int i) {
    int n = h->size;
    while (1) {
        int smallest = i;
        int left  = 2 * i + 1;
        int right = 2 * i + 2;
        if (left  < n && date_to_int(h->data[left]->expiry)  < date_to_int(h->data[smallest]->expiry))
            smallest = left;
        if (right < n && date_to_int(h->data[right]->expiry) < date_to_int(h->data[smallest]->expiry))
            smallest = right;
        if (smallest != i) {
            heap_swap(h, i, smallest);
            i = smallest;
        } else break;
    }
}

void heap_insert(MinHeap *h, DrugNode *node) {
    if (h->size >= HEAP_CAPACITY) return;
    int i = h->size++;
    h->data[i] = node;
    node->heap_index = i;
    heap_sift_up(h, i);
}

void heap_remove(MinHeap *h, DrugNode *node) {
    int i = node->heap_index;
    if (i < 0 || i >= h->size) return;
    heap_swap(h, i, h->size - 1);
    h->size--;
    if (i < h->size) {
        heap_sift_up(h, i);
        heap_sift_down(h, i);
    }
    node->heap_index = -1;
}

DrugNode *heap_peek_min(MinHeap *h) {
    return (h->size > 0) ? h->data[0] : NULL;
}

/* ============================================================
 *  DATA STRUCTURE 4: STACK (Action History / Undo Log)
 *  - Records last N operations (add, restock, remove)
 *  - LIFO: last action shown first in history
 * ============================================================ */
typedef struct {
    char entries[STACK_CAPACITY][128];
    int  top;
} ActionStack;

void stack_init(ActionStack *s) {
    s->top = -1;
}

void stack_push(ActionStack *s, const char *msg) {
    if (s->top < STACK_CAPACITY - 1) {
        s->top++;
        strncpy(s->entries[s->top], msg, 127);
        s->entries[s->top][127] = '\0';
    } else {
       
        memmove(s->entries[0], s->entries[1], (STACK_CAPACITY - 1) * 128);
        strncpy(s->entries[s->top], msg, 127);
    }
}

void stack_print(ActionStack *s) {
    if (s->top < 0) { printf("  (no actions recorded)\n"); return; }
    for (int i = s->top; i >= 0; i--)
        printf("  [%2d] %s\n", s->top - i + 1, s->entries[i]);
}

typedef struct {
    LinkedList  inventory;
    HashTable   lookup;
    MinHeap     expiry_heap;
    ActionStack history;
} PharmacySystem;

PharmacySystem sys;

/* ============================================================
 *  ALGORITHM 1: MERGE SORT (Sort inventory array)
 *  Used to sort drugs by: expiry date, name, or stock level
 * ============================================================ */
typedef int (*CmpFn)(const DrugNode *, const DrugNode *);

int cmp_by_expiry(const DrugNode *a, const DrugNode *b) {
    return date_to_int(a->expiry) - date_to_int(b->expiry);
}

int cmp_by_name(const DrugNode *a, const DrugNode *b) {
    return strcmp(a->name, b->name);
}

int cmp_by_stock(const DrugNode *a, const DrugNode *b) {
    return a->quantity - b->quantity; 
}

void merge(DrugNode **arr, int l, int m, int r, CmpFn cmp) {
    int n1 = m - l + 1, n2 = r - m;
    DrugNode **L = malloc(n1 * sizeof(DrugNode *));
    DrugNode **R = malloc(n2 * sizeof(DrugNode *));

    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int i = 0; i < n2; i++) R[i] = arr[m + 1 + i];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2)
        arr[k++] = (cmp(L[i], R[j]) <= 0) ? L[i++] : R[j++];
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    free(L); free(R);
}

void merge_sort(DrugNode **arr, int l, int r, CmpFn cmp) {
    if (l < r) {
        int m = (l + r) / 2;
        merge_sort(arr, l, m, cmp);
        merge_sort(arr, m + 1, r, cmp);
        merge(arr, l, m, r, cmp);
    }
}

DrugNode **list_to_sorted_array(LinkedList *list, CmpFn cmp, int *out_count) {
    if (list->count == 0) { *out_count = 0; return NULL; }
    DrugNode **arr = malloc(list->count * sizeof(DrugNode *));
    DrugNode *cur = list->head;
    int i = 0;
    while (cur) { arr[i++] = cur; cur = cur->next; }
    merge_sort(arr, 0, i - 1, cmp);
    *out_count = i;
    return arr;
}

/* ============================================================
 *  ALGORITHM 2: BINARY SEARCH (Search sorted inventory)
 *  Searches a sorted array by drug name - O(log n)
 * ============================================================ */
int binary_search_by_name(DrugNode **arr, int n, const char *name) {
    int lo = 0, hi = n - 1;
    char target[MAX_NAME];
    normalize_key(name, target);

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        char mid_key[MAX_NAME];
        normalize_key(arr[mid]->name, mid_key);
        int cmp = strcmp(mid_key, target);
        if (cmp == 0)  return mid;
        if (cmp < 0)   lo = mid + 1;
        else           hi = mid - 1;
    }
    return -1;
}

void print_separator(char c, int n) {
    for (int i = 0; i < n; i++) putchar(c);
    putchar('\n');
}

void print_drug_header() {
    print_separator('-', 105);
    printf("  %-4s  %-22s  %-14s  %-18s  %6s  %8s  %12s  %s\n",
           "ID", "Name", "Category", "Supplier", "Qty", "Price", "Expiry", "Status");
    print_separator('-', 105);
}

void print_drug_row(const DrugNode *d) {
    int days = days_until_expiry(d->expiry);
    const char *status;
    if (days < 0)         status = "EXPIRED  ";
    else if (days <= 30)  status = "CRITICAL ";
    else if (days <= 90)  status = "EXPIRING ";
    else if (d->quantity <= LOW_STOCK_THRESHOLD) status = "LOW STOCK";
    else                  status = "OK       ";

    printf("  %-4d  %-22s  %-14s  %-18s  %6d  %8.2f  ",
           d->id, d->name, d->category, d->supplier, d->quantity, d->price);
    print_date(d->expiry);
    printf("  %s\n", status);
}

void print_drug_detail(const DrugNode *d) {
    int days = days_until_expiry(d->expiry);
    printf("\n");
    print_separator('=', 50);
    printf("  Drug Detail\n");
    print_separator('=', 50);
    printf("  ID        : %d\n",   d->id);
    printf("  Name      : %s\n",   d->name);
    printf("  Category  : %s\n",   d->category);
    printf("  Supplier  : %s\n",   d->supplier);
    printf("  Quantity  : %d\n",   d->quantity);
    printf("  Price     : %.2f\n", d->price);
    printf("  Expiry    : ");      print_date(d->expiry); printf("\n");
    printf("  Days Left : %d %s\n", days, days < 0 ? "(EXPIRED)" : "");
    print_separator('=', 50);
}

/* ===================
 *  CORE OPERATIONS
 * ===================*/

DrugNode *drug_create(const char *name, const char *category,
                      const char *supplier, int qty, float price, Date expiry) {
    DrugNode *node = calloc(1, sizeof(DrugNode));
    node->id         = sys.inventory.next_id++;
    node->quantity   = qty;
    node->price      = price;
    node->expiry     = expiry;
    node->heap_index = -1;
    strncpy(node->name,     name,     MAX_NAME - 1);
    strncpy(node->category, category, MAX_CATEGORY - 1);
    strncpy(node->supplier, supplier, MAX_SUPPLIER - 1);
    return node;
}

void add_drug(const char *name, const char *category,
              const char *supplier, int qty, float price, Date expiry) {
    if (hash_search(&sys.lookup, name)) {
        printf("  [!] A drug named '%s' already exists. Use restock instead.\n", name);
        return;
    }
    DrugNode *node = drug_create(name, category, supplier, qty, price, expiry);
    list_insert(&sys.inventory, node);   
    hash_insert(&sys.lookup, node);      
    heap_insert(&sys.expiry_heap, node); 

    char log[128];
    snprintf(log, sizeof(log), "ADDED   : [ID:%d] %s  (Qty: %d)", node->id, name, qty);
    stack_push(&sys.history, log);

    printf("  [+] Drug '%s' added successfully (ID: %d).\n", name, node->id);
}

void restock_drug(const char *name, int amount) {
    DrugNode *node = hash_search(&sys.lookup, name);
    if (!node) {
        printf("  [!] Drug '%s' not found.\n", name);
        return;
    }
    node->quantity += amount;
    char log[128];
    snprintf(log, sizeof(log), "RESTOCK : [ID:%d] %s  (+%d -> total %d)", node->id, name, amount, node->quantity);
    stack_push(&sys.history, log);
    printf("  [+] Restocked '%s' by %d. New quantity: %d\n", name, amount, node->quantity);
}

void dispense_drug(const char *name, int amount) {
    DrugNode *node = hash_search(&sys.lookup, name);
    if (!node) { printf("  [!] Drug '%s' not found.\n", name); return; }
    if (node->quantity < amount) {
        printf("  [!] Insufficient stock. Available: %d\n", node->quantity);
        return;
    }
    int days = days_until_expiry(node->expiry);
    if (days < 0) {
        printf("  [!] WARNING: '%s' is EXPIRED. Dispensing blocked.\n", name);
        return;
    }
    node->quantity -= amount;
    char log[128];
    snprintf(log, sizeof(log), "DISPENSED: [ID:%d] %s  (-%d -> remaining %d)", node->id, name, amount, node->quantity);
    stack_push(&sys.history, log);
    printf("  [-] Dispensed %d units of '%s'. Remaining: %d\n", amount, name, node->quantity);
}

void remove_drug(const char *name) {
    DrugNode *node = hash_search(&sys.lookup, name);
    if (!node) { printf("  [!] Drug '%s' not found.\n", name); return; }

    char log[128];
    snprintf(log, sizeof(log), "REMOVED : [ID:%d] %s", node->id, name);
    stack_push(&sys.history, log);

    list_remove(&sys.inventory, node);
    hash_remove(&sys.lookup, node);
    heap_remove(&sys.expiry_heap, node);
    free(node);
    printf("  [-] Drug '%s' removed from inventory.\n", name);
}

void display_all_sorted(int sort_mode) {
    if (sys.inventory.count == 0) {
        printf("  (inventory is empty)\n"); return;
    }
    int n;
    CmpFn cmp;
    const char *label;
    if      (sort_mode == 1) { cmp = cmp_by_expiry; label = "Expiry Date (soonest first)"; }
    else if (sort_mode == 2) { cmp = cmp_by_name;   label = "Name (A-Z)"; }
    else                     { cmp = cmp_by_stock;  label = "Stock Level (lowest first)"; }

    DrugNode **arr = list_to_sorted_array(&sys.inventory, cmp, &n);
    printf("\n  Inventory sorted by: %s  (%d items)\n", label, n);
    print_drug_header();
    for (int i = 0; i < n; i++) print_drug_row(arr[i]);
    print_separator('-', 105);
    free(arr);
}

void check_expiry_alerts(int within_days) {
    printf("\n  === Expiry Alerts (within %d days) ===\n", within_days);
    int found = 0;
    for (int i = 0; i < sys.expiry_heap.size; i++) {
        DrugNode *d = sys.expiry_heap.data[i];
        int days = days_until_expiry(d->expiry);
        if (days <= within_days) {
            printf("  %s%-22s%s  Expiry: ", days < 0 ? "[EXPIRED] " : "[  WARN  ] ", d->name, "");
            print_date(d->expiry);
            printf("  (%d days)  Qty: %d\n", days, d->quantity);
            found++;
        }
    }
    if (!found) printf("  No drugs expiring within %d days.\n", within_days);
}

void show_next_expiry() {
    DrugNode *d = heap_peek_min(&sys.expiry_heap);
    if (!d) { printf("  (no drugs in inventory)\n"); return; }
    int days = days_until_expiry(d->expiry);
    printf("\n  Next to expire: %s  (", d->name);
    print_date(d->expiry);
    printf(", %d days)\n", days);
}

void show_low_stock() {
    printf("\n  === Low Stock Report (threshold: %d) ===\n", LOW_STOCK_THRESHOLD);
    int n, found = 0;
    DrugNode **arr = list_to_sorted_array(&sys.inventory, cmp_by_stock, &n);
    print_drug_header();
    for (int i = 0; i < n; i++) {
        if (arr[i]->quantity <= LOW_STOCK_THRESHOLD) {
            print_drug_row(arr[i]);
            found++;
        }
    }
    print_separator('-', 105);
    if (!found) printf("  All drugs are adequately stocked.\n");
    free(arr);
}

void search_drug(const char *name) {
    printf("\n  === Search Results for '%s' ===\n", name);

    DrugNode *found = hash_search(&sys.lookup, name);
    if (found) {
        printf("  [Hash Table] Found directly:\n");
        print_drug_detail(found);
        return;
    }

    printf("  [Hash Miss] Trying binary search on sorted array...\n");
    int n;
    DrugNode **arr = list_to_sorted_array(&sys.inventory, cmp_by_name, &n);
    int idx = binary_search_by_name(arr, n, name);
    if (idx >= 0) {
        printf("  [Binary Search] Found at index %d:\n", idx);
        print_drug_detail(arr[idx]);
    } else {
        printf("  Drug not found.\n");
    }
    free(arr);
}

void show_statistics() {
    if (sys.inventory.count == 0) { printf("  No data.\n"); return; }
    int total_qty = 0, expired = 0, low = 0;
    float total_value = 0.0f;
    DrugNode *cur = sys.inventory.head;
    while (cur) {
        total_qty   += cur->quantity;
        total_value += cur->quantity * cur->price;
        if (days_until_expiry(cur->expiry) < 0) expired++;
        if (cur->quantity <= LOW_STOCK_THRESHOLD) low++;
        cur = cur->next;
    }
    printf("\n");
    print_separator('=', 45);
    printf("  INVENTORY STATISTICS\n");
    print_separator('=', 45);
    printf("  Total Drug Entries   : %d\n",   sys.inventory.count);
    printf("  Total Units in Stock : %d\n",   total_qty);
    printf("  Total Inventory Value: PHP %.2f\n", total_value);
    printf("  Expired Drugs        : %d\n",   expired);
    printf("  Low Stock Drugs      : %d\n",   low);
    show_next_expiry();
    print_separator('=', 45);
}

/* ============================================================
 *  DRUG SAMPLES
 * ============================================================ */
void load_sample_data() {
    struct { const char *name, *cat, *sup; int qty; float price; int y,m,d; } samples[] = {
        {"Amoxicillin 500mg",  "Antibiotic",  "PharmaCorp",   150, 12.50, 2025, 6, 15},
        {"Paracetamol 500mg",  "Analgesic",   "MediSupply",   300, 5.75,  2026, 12, 1},
        {"Ibuprofen 400mg",    "NSAID",       "BioLab",       80,  9.00,  2026, 3, 20},
        {"Metformin 500mg",    "Antidiabetic","GlobalPharma", 200, 8.25,  2026, 9, 10},
        {"Losartan 50mg",      "Antihyper.",  "MediSupply",   15,  18.00, 2027, 1, 5},
        {"Cetirizine 10mg",    "Antihistamine","PharmaCorp",  5,   7.50,  2024, 11, 30},  /* expired */
        {"Omeprazole 20mg",    "Antacid",     "BioLab",       120, 14.00, 2026, 7, 22},
        {"Azithromycin 500mg", "Antibiotic",  "GlobalPharma", 10,  45.00, 2025, 9, 1},
        {"Atorvastatin 20mg",  "Statin",      "PharmaCorp",   90,  22.00, 2027, 3, 15},
        {"Salbutamol Inhaler", "Bronchodil.", "MediSupply",   40,  85.00, 2026, 5, 30},
    };
    int n = sizeof(samples) / sizeof(samples[0]);
    for (int i = 0; i < n; i++) {
        Date d = { samples[i].d, samples[i].m, samples[i].y };
        add_drug(samples[i].name, samples[i].cat, samples[i].sup,
                 samples[i].qty, samples[i].price, d);
    }
    printf("\n  [*] Sample data loaded: %d drugs.\n", n);
}

void flush_input() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void read_line(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, size, stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
    }
}

int read_int(const char *prompt) {
    char buf[32];
    read_line(prompt, buf, sizeof(buf));
    return atoi(buf);
}

float read_float(const char *prompt) {
    char buf[32];
    read_line(prompt, buf, sizeof(buf));
    return (float)atof(buf);
}

/* ================
 *  MENU
 * ================ */

void menu_add_drug() {
    char name[MAX_NAME], cat[MAX_CATEGORY], sup[MAX_SUPPLIER], date_str[16];
    int qty; float price; Date expiry;

    printf("\n  --- Add New Drug ---\n");
    read_line("  Name     : ", name, MAX_NAME);
    read_line("  Category : ", cat,  MAX_CATEGORY);
    read_line("  Supplier : ", sup,  MAX_SUPPLIER);
    qty   = read_int  ("  Quantity : ");
    price = read_float("  Price    : ");
    read_line("  Expiry (YYYY-MM-DD): ", date_str, sizeof(date_str));

    if (!parse_date(date_str, &expiry)) {
        printf("  [!] Invalid date format. Use YYYY-MM-DD.\n"); return;
    }
    add_drug(name, cat, sup, qty, price, expiry);
}

void menu_restock() {
    char name[MAX_NAME];
    printf("\n  --- Restock Drug ---\n");
    read_line("  Drug name : ", name, MAX_NAME);
    int amt = read_int("  Amount to add : ");
    restock_drug(name, amt);
}

void menu_dispense() {
    char name[MAX_NAME];
    printf("\n  --- Dispense Drug ---\n");
    read_line("  Drug name : ", name, MAX_NAME);
    int amt = read_int("  Amount to dispense : ");
    dispense_drug(name, amt);
}

void menu_remove() {
    char name[MAX_NAME];
    printf("\n  --- Remove Drug ---\n");
    read_line("  Drug name to remove : ", name, MAX_NAME);
    char confirm[4];
    printf("  Confirm remove '%s'? (yes/no): ", name);
    scanf("%3s", confirm); flush_input();
    if (strcmp(confirm, "yes") == 0) remove_drug(name);
    else printf("  Cancelled.\n");
}

void menu_search() {
    char name[MAX_NAME];
    printf("\n  --- Search Drug ---\n");
    read_line("  Drug name : ", name, MAX_NAME);
    search_drug(name);
}

void menu_expiry() {
    printf("\n  Expiry Check:\n");
    printf("  [1] Next to expire (heap peek - O(1))\n");
    printf("  [2] Expiring within N days\n");
    int c = read_int("  Choice: ");
    if (c == 1) show_next_expiry();
    else {
        int days = read_int("  Within how many days? ");
        check_expiry_alerts(days);
    }
}

void menu_view() {
    printf("\n  View Inventory:\n");
    printf("  [1] Sort by Expiry Date\n");
    printf("  [2] Sort by Name\n");
    printf("  [3] Sort by Stock Level\n");
    int c = read_int("  Choice: ");
    display_all_sorted(c);
}

/* =====================
 *  MAIN MENU
 * ===================== */
void print_banner() {
    printf("\n");
    print_separator('=', 60);
    printf("   PHARMACY INVENTORY MANAGEMENT SYSTEM (PIMS)\n");
    printf("   CC104: Data Structures and Algorithms\n");
    printf("   Central Bicol State University of Agriculture\n");
    print_separator('=', 60);
    printf("   DSA Used:\n");
    printf("   [DS1] Doubly Linked List   - Core inventory store\n");
    printf("   [DS2] Hash Table (djb2)    - O(1) drug lookup\n");
    printf("   [DS3] Min-Heap             - Expiry priority queue\n");
    printf("   [DS4] Stack                - Action history log\n");
    printf("   [AL1] Merge Sort           - Sort inventory O(n log n)\n");
    printf("   [AL2] Binary Search        - Search sorted list O(log n)\n");
    print_separator('=', 60);
}

void print_menu() {
    printf("\n  +----- MAIN MENU ---------------------------------+\n");
    printf("  | [1] View Inventory (sorted)                    |\n");
    printf("  | [2] Add Drug                                   |\n");
    printf("  | [3] Restock Drug                               |\n");
    printf("  | [4] Dispense Drug                              |\n");
    printf("  | [5] Remove Drug                                |\n");
    printf("  | [6] Search Drug                                |\n");
    printf("  | [7] Expiry Alerts                              |\n");
    printf("  | [8] Low Stock Report                           |\n");
    printf("  | [9] Statistics                                 |\n");
    printf("  | [10] Action History (Stack)                    |\n");
    printf("  | [0] Exit                                       |\n");
    printf("  +-------------------------------------------------+\n");
}

int main() {
    list_init(&sys.inventory);
    hash_init(&sys.lookup);
    heap_init(&sys.expiry_heap);
    stack_init(&sys.history);

    print_banner();

    printf("\n  Load sample data? (yes/no): ");
    char ans[8];
    scanf("%7s", ans); flush_input();
    if (strcmp(ans, "yes") == 0) load_sample_data();

    int choice;
    do {
        print_menu();
        choice = read_int("\n  Enter choice: ");
        switch (choice) {
            case 1:  menu_view(); break;
            case 2:  menu_add_drug(); break;
            case 3:  menu_restock(); break;
            case 4:  menu_dispense(); break; 
            case 5:  menu_remove(); break;                        
            case 6:  menu_search(); break;                           
            case 7:  menu_expiry(); break;                             
            case 8:  show_low_stock(); break;                          
            case 9:  show_statistics(); break;                             
            case 10:
                printf("\n  === Action History (Stack - LIFO) ===\n");
                stack_print(&sys.history);
                break;
            case 0:
                printf("\n  Exiting PIMS. Goodbye!\n\n");
                break;
            default:
                printf("  [!] Invalid choice.\n");
        }
    } while (choice != 0);

    DrugNode *cur = sys.inventory.head;
    while (cur) {
        DrugNode *next = cur->next;
        free(cur);
        cur = next;
    }

    return 0;
}
