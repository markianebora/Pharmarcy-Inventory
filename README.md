# Pharmacy Inventory Management System (PIMS)

> CC104: Data Structures and Algorithms — Final Project  
> Central Bicol State University of Agriculture  
> 2nd Semester, SY 2025-2026

## Overview

PIMS is a console-based pharmacy inventory system written in C. It allows pharmacy staff to manage drug stock, monitor expiry dates, search records, and track dispensing history. The system is designed to demonstrate meaningful real-world application of core DSA concepts taught in CC104.

---

## Data Structures & Algorithms Used

### Data Structures

| # | Structure | Location in Code | Purpose |
|---|-----------|-----------------|---------|
| DS1 | **Doubly Linked List** | `LinkedList`, `list_insert`, `list_remove` | Core inventory storage. O(1) insert/delete with pointer. Maintains all drug records. |
| DS2 | **Hash Table** (djb2, separate chaining) | `HashTable`, `hash_insert`, `hash_search` | Fast drug lookup by name. Average O(1) lookup — avoids scanning the entire list. |
| DS3 | **Min-Heap** (array-based) | `MinHeap`, `heap_insert`, `heap_sift_up/down` | Expiry priority queue. Always keeps the soonest-to-expire drug at the root for O(1) peek and O(log n) insert/remove. |
| DS4 | **Stack** | `ActionStack`, `stack_push`, `stack_print` | LIFO action history log. Records the last N operations (add, restock, dispense, remove) for audit purposes. |

### Algorithms

| # | Algorithm | Location in Code | Purpose |
|---|-----------|-----------------|---------|
| AL1 | **Merge Sort** | `merge_sort`, `merge` | Sorts inventory by expiry date, drug name, or stock level. O(n log n) time. Used for all sorted display views. |
| AL2 | **Binary Search** | `binary_search_by_name` | Searches a sorted array of drug names in O(log n) time. Used as a fallback when hash lookup misses. |

---

## Features

- **Add / Remove / Restock / Dispense drugs** — full CRUD on inventory
- **Expiry Tracking** — powered by the Min-Heap; alerts for drugs expiring within N days; expired drugs are blocked from dispensing
- **Low Stock Report** — shows all drugs at or below the configurable threshold
- **Multi-mode Sorting** — sort inventory by expiry date, name, or stock level using Merge Sort
- **Fast Search** — hash table lookup (O(1) avg); falls back to binary search (O(log n)) on miss
- **Action History** — Stack-based audit log of all operations (LIFO display)
- **Inventory Statistics** — total units, total value, expired count, low-stock count

---

## How to Compile and Run

### Requirements
- GCC (or any C99-compatible compiler)
- Linux / macOS terminal, or Windows with MinGW/WSL

### Compile

```bash
gcc -Wall -o pims main.c
```

### Run

```bash
./pims
```

When prompted, type `yes` to load the built-in sample data (10 drugs), or `no` to start with an empty inventory.

### Menu Navigation

```
[1] View Inventory     - sorted display (choose sort key)
[2] Add Drug           - add a new drug record
[3] Restock Drug       - increase quantity of existing drug
[4] Dispense Drug      - decrease quantity (blocks expired drugs)
[5] Remove Drug        - permanently delete a drug record
[6] Search Drug        - by name (hash table + binary search)
[7] Expiry Alerts      - peek min-heap or scan within N days
[8] Low Stock Report   - sorted merge sort view of low stock
[9] Statistics         - summary of entire inventory
[10] Action History    - LIFO stack of recent operations
[0] Exit
```

---

## Academic Integrity

This project was implemented by the student. AI tools were consulted as a learning aid for understanding algorithm implementations. All final code was written, adapted, and is fully understood by the submitting student.

---

## Author
> Atillano, Aluna Z.
> Ebora, Mark Ian B.
> Gremio, Carlo M.
> Oringo, Pearl Nicole F.
BS Information Technology  
Central Bicol State University of Agriculture - Impig_Sipocot
