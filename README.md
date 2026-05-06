# Mini UnionFS using FUSE3

A lightweight userspace Union Filesystem implemented using **FUSE3** in C.

This project demonstrates core filesystem concepts such as:

- Read-only lower layer
- Writable upper layer
- Copy-on-Write (CoW)
- Whiteout-based deletion
- Layer merging using FUSE

---

# Features

- Merge two directories into a single mounted filesystem
- Read files from:
  - upper layer first
  - lower layer as fallback
- All writes occur only in the upper layer
- Copy-on-Write support for modifying lower-layer files
- Whiteout mechanism for delete operations
- Directory merging support
- Modular multi-file implementation

---

# Project Structure

```plaintext
unionfs/
├── src/
│   ├── unionfs.c
│   ├── unionfs.h
│   ├── ops_read.c
│   ├── ops_write.c
│   └── ops_delete.c
│
├── Makefile
└──  README.md
```

### File Descriptions

| File | Purpose |
|---|---|
| `unionfs.c` | Core filesystem setup and helper utilities |
| `unionfs.h` | Shared declarations and filesystem state |
| `ops_read.c` | Read operations and directory merging |
| `ops_write.c` | Write operations and Copy-on-Write engine |
| `ops_delete.c` | Delete operations and whiteout handling |

---

# Requirements

- Linux system / VM
- GCC
- FUSE3
- pkg-config

---

# Installation

## Ubuntu / Debian

```bash
sudo apt update
sudo apt install fuse3 libfuse3-dev pkg-config gcc make
```

---

# Compilation

From the project root directory:

```bash
make
```

This generates the executable:

```bash
mini_unionfs
```

---

# Running the Filesystem

## Step 1 — Create directories

```bash
mkdir lower upper mount
```

### Directory Roles

| Directory | Purpose |
|---|---|
| `lower/` | Read-only base layer |
| `upper/` | Writable layer |
| `mount/` | Mounted merged filesystem |

---

## Step 2 — Add sample files

```bash
echo "hello from lower layer" > lower/test.txt
```

---

## Step 3 — Mount the filesystem

```bash
./mini_unionfs lower upper mount
```

Expected startup logs:

```plaintext
[mini-unionfs] lower : /path/to/lower
[mini-unionfs] upper : /path/to/upper
[mini-unionfs] mount : mount
```

---

# Testing

Open another terminal for testing.

## Read File

```bash
cat mount/test.txt
```

Expected output:

```plaintext
hello from lower layer
```

---

## Modify Lower-Layer File (Triggers CoW)

```bash
echo "modified" >> mount/test.txt
```

Now verify:

```bash
cat upper/test.txt
```

The file should now exist inside the upper layer.

---

## Create New File

```bash
touch mount/newfile.txt
```

New files are created directly inside `upper/`.

---

## Delete File

```bash
rm mount/test.txt
```

A whiteout entry is created in the upper layer to hide the lower-layer file.

---

# Copy-on-Write (CoW)

When a file exists only in the lower layer:

1. The file is copied into the upper layer
2. Modifications are applied to the upper copy
3. The lower layer remains unchanged

This preserves immutability of the lower layer.

---

# Whiteout Mechanism

Deleting a file should not modify the lower layer.

Instead:

- a hidden whiteout entry is created in `upper/`
- the lower-layer file becomes invisible in the merged view

---

# Team Contributions

| Member | Responsibility |
|---|---|
| Siri Basavaraj | Core filesystem setup, path resolution, FUSE integration |
| Shubhangi Srivastava | Read operations and directory merging |
| Shuchika Joy | Write operations and Copy-on-Write engine |
| Sinchana Rathnakar | Delete operations and whiteout handling |

---

# Cleanup

Remove generated object files and binaries:

```bash
make clean
```

Unmount filesystem:

```bash
fusermount3 -u mount
```

---

# Authors

Mini UnionFS Team Project by Shubhangi, Shuchika, Sinchana & Siri

Implemented as part of coursework in Cloud Computing
