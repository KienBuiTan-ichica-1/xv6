# Báo cáo Đồ án 0 – System Call `trace` (xv6-riscv)

> Tài liệu này giải thích **đồ án làm gì**, **đã thêm/sửa những file nào, dòng nào**,
> và **cách đoạn code hoạt động** từ user space xuống kernel rồi trở về.

---

## 1. Mục tiêu

Thêm một system call mới tên `trace` vào xv6:

```
int trace(int mask);
```

- Mỗi **bit thứ _n_** trong `mask` được bật (=1) nghĩa là: khi tiến trình gọi
  syscall số _n_ sắp trả về, kernel in ra một dòng:
  `syscall <tên> -> <giá trị trả về>`.
- Mask chỉ ảnh hưởng **tiến trình gọi `trace` và các con của nó** (do `fork`
  kế thừa), không ảnh hưởng tiến trình khác.
- Viết thêm chương trình user `trace.c`: `trace <mask> <lệnh> [args...]`.

Ví dụ (đề bài):

```
$ trace 32 grep hello README
syscall read -> 1023
syscall read -> 966
syscall read -> 70
syscall read -> 0
$
```

`32 = 1<<5` = bit 5 = syscall `read` (vì `SYS_read = 5`).

---

## 2. Kiến trúc luồng system call trong xv6

```
 user space                        kernel space
 ──────────                        ────────────
 trace(32)                          (stub do usys.S sinh ra)
   |                                    |
   v                                    v
 [ gọi hàm libc trace ]  --ecall-->  usertrap()  (kernel/trap.c)
                                            |
                                            v
                                       syscall()  (kernel/syscall.c)
                                            |
                       num = p->trapframe->a7  (số hiệu syscall)
                       p->trapframe->a0 = syscalls[num]()   (gọi hàm xử lý)
                                            |
                                            v
                                       sys_trace()  (kernel/sysproc.c)
                                            |
                                            v
                       p->trace_mask = mask ; return 0
                                            |
   (khi trả về) nếu bit num được bật:
   printk("syscall %s -> %d\n", tên, giá_trị)
                                            |
   <--ecall return---  a0 chứa giá trị trả về
```

Điểm mấu chốt:
- Tham số truyền qua các **thanh ghi a0..a5** của tiến trình; số hiệu syscall nằm
  ở **a7**; giá trị trả về ghi vào **a0**.
- Bảng `syscalls[]` ánh xạ *số hiệu* → *hàm xử lý*.

---

## 3. Các file đã thay đổi / thêm mới

| # | File | Loại | Nội dung |
|---|------|------|----------|
| 1 | `kernel/syscall.h` | sửa | `#define SYS_trace 23` |
| 2 | `kernel/syscall.c` | sửa | `extern sys_trace`, thêm vào `syscalls[]`, thêm `syscall_names[]`, in khi return |
| 3 | `kernel/proc.h` | sửa | thêm `int trace_mask;` vào `struct proc` |
| 4 | `kernel/sysproc.c` | sửa | cài `sys_trace()` |
| 5 | `kernel/proc.c` | sửa | khởi tạo/xoá mask (`allocproc`/`freeproc`) và kế thừa (`kfork`) |
| 6 | `user/user.h` | sửa | khai báo `int trace(int);` |
| 7 | `user/usys.pl` | sửa | `entry("trace");` |
| 8 | `user/trace.c` | **mới** | chương trình `trace` |
| 9 | `Makefile` | sửa | thêm `$U/_trace` vào `UPROGS` |

> **Lưu ý về số hiệu:** đề gợi ý `SYS_trace 22`, nhưng trong bản xv6 này số **22
> đã bị `SYS_sync` chiếm**, nên đã dùng **23** (số kế tiếp hợp lệ). Nếu dùng 22 sẽ
> đè lên `sync` và gây lỗi.

---

## 4. Giải thích chi tiết từng thay đổi

### 4.1. `kernel/syscall.h`

```c
#define SYS_close  21
#define SYS_sync   22
#define SYS_trace  23   // <-- thêm mới
```

Số hiệu syscall là "chìa khoá" để user space (qua `a7`) báo cho kernel biết cần
gọi syscall nào.

### 4.2. `kernel/syscall.c`

**(a)** Khai báo prototype hàm xử lý:

```c
extern uint64 sys_trace(void);
```

**(b)** Thêm vào bảng ánh xạ số hiệu → hàm:

```c
[SYS_trace] = sys_trace,
```

**(c)** Thêm bảng tên **song song về chỉ số** với `syscalls[]`:

```c
static char *syscall_names[] = {
  [SYS_fork]  = "fork",
  ...
  [SYS_trace] = "trace",
};
```

Dùng dạng `[SYS_xxx] = "xxx"` (designated initializer) nên chỉ số luôn khớp với
`sycalls[]`, không sợ lệch khi thêm/bớt syscall.

**(d)** In trace trong `syscall()`:

```c
p->trapframe->a0 = syscalls[num]();          // gọi syscall, lấy giá trị trả về
if ((p->trace_mask >> num) & 1)              // bit thứ num của mask bật?
  printk("syscall %s -> %d\n", syscall_names[num], (int)p->trapframe->a0);
```

- **Phải in SAU khi gán `a0`** thì mới có giá trị trả về đúng.
- `(p->trace_mask >> num) & 1` tách bit thứ `num`.
- Ép `(int)` cho khớp `%d` (kernel dùng `printk`, không phải `printf`).

### 4.3. `kernel/proc.h`

```c
struct proc {
  ...
  int trace_mask;   // <-- thêm mới: mask trace riêng của từng tiến trình
};
```

Vì mỗi tiến trình có mask riêng, đặt trong `struct proc` là hợp lý. Đây là dữ liệu
"private to the process" nên khi đọc trong `syscall()` **không cần** giữ `p->lock`.

### 4.4. `kernel/sysproc.c`

```c
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);            // lấy tham số thứ 0 (nằm ở trapframe->a0)
  myproc()->trace_mask = mask; // lưu vào tiến trình hiện tại
  return 0;                    // thành công -> in "syscall trace -> 0"
}
```

Dùng `argint()` thay vì đọc trực tiếp `trapframe->a0` để nhất quán và an toàn.

### 4.5. `kernel/proc.c` – 3 chỗ

**(a)** `allocproc()` – tiến trình mới bắt đầu sạch:

```c
p->state = USED;
p->trace_mask = 0;   // <-- thêm mới
```

**(b)** `freeproc()` – xoá mask khi tiến trình kết thúc (tránh "dính" mask cũ khi
slot được tái sử dụng):

```c
p->xstate = 0;
p->trace_mask = 0;   // <-- thêm mới
p->state = UNUSED;
```

**(c)** `kfork()` – con kế thừa mask của cha:

```c
safestrcpy(np->name, p->name, sizeof(p->name));
np->trace_mask = p->trace_mask;   // <-- thêm mới
```

Nhờ (c) nên khi `sh` chạy một lệnh, `sh` `fork` ra tiến trình con, con kế thừa
mask, rồi `exec` lệnh — mask vẫn giữ nguyên qua `exec` (vì `exec` không tạo tiến
trình mới).

### 4.6. `user/user.h` và `user/usys.pl`

```c
// user.h
int trace(int);
```

```perl
# usys.pl
entry("trace");
```

`usys.pl` sinh ra `user/usys.S` chứa stub assembly: nạp `SYS_trace` vào `a7` rồi
`ecall`. **Sau khi sửa `usys.pl` phải build lại để `usys.S` được cập nhật.**

### 4.7. `user/trace.c` (file mới)

```c
int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(2, "usage: trace mask command [args...]\n");
    exit(1);
  }
  trace(atoi(argv[1]));   // bật mask
  exec(argv[2], &argv[2]); // thay thế tiến trình bằng lệnh cần trace
  fprintf(2, "trace: exec %s failed\n", argv[2]); // chỉ tới đây nếu exec lỗi
  exit(1);
}
```

- `&argv[2]` truyền phần `{lệnh, args...}` làm argv cho chương trình mới.
- Nếu `exec` thành công thì **không bao giờ trở lại** đây.

### 4.8. `Makefile`

```make
UPROGS=\
  ...
  $U/_sync\
  $U/_trace\   # <-- thêm mới để đóng gói trace vào fs.img
```

---

## 5. Luồng hoạt động end-to-end (ví dụ `trace 32 grep hello README`)

1. `sh` đọc dòng lệnh, `fork` + `exec` `trace 32 grep hello README`.
2. `trace.c`: `argc=5`, gọi `trace(32)`.
   - Stub `usys.S`: `a7 = SYS_trace(23)`, `ecall`.
   - Kernel: `syscall()` → `sys_trace()` → `myproc()->trace_mask = 32`, trả 0.
   - Vì bit 23 của mask (32) = 0 nên **không** in `syscall trace -> 0`.
3. `trace.c`: `exec("grep", {grep, hello, README, 0})`.
   - Tiến trình **giữ nguyên** `trace_mask = 32` (exec không tạo proc mới).
   - `exec` là syscall số 7; bit 7 của 32 = 0 → không in `exec`.
4. `grep` chạy: `open`, `read`… `read` là số 5; bit 5 của 32 = 1 → mỗi lần
   `read` return đều in:
   ```
   syscall read -> 1023
   syscall read -> 966
   syscall read -> 70
   syscall read -> 0
   ```
5. `close` (số 21) bit 21 = 0 → không in. Kết thúc.

Với `trace 2147483647` (31 bit thấp đều bật): in **tất cả** syscall, kể cả
`trace -> 0`, `exec -> 3`, `open -> 3`, `close -> 0`…

---

## 6. Những điểm dễ sai (và đã xử lý)

| Điểm dễ sai | Cách xử lý trong bài |
|-------------|----------------------|
| Quên kế thừa mask khi `fork` | Đã thêm `np->trace_mask = p->trace_mask` trong `kfork()` |
| In trước khi có return value | In **sau** `p->trapframe->a0 = syscalls[num]()` |
| Lệch tên syscall | Dùng designated initializer `[SYS_xxx]="xxx"` |
| Số hiệu trùng | Dùng `SYS_trace 23` (vì 22 đã là `SYS_sync`) |
| Quên build lại `usys.S` | Chạy `make clean` (hoặc xoá `user/usys.S`) rồi `make qemu` |
| Không reset mask trong `freeproc` | Đã reset về 0 |
| Format `%d` với `uint64` | Ép `(int)` để tránh cảnh báo `-Wformat` (build dùng `-Werror`) |

---

## 7. Cách build và kiểm thử

```sh
# trong thư mục xv6-riscv
make clean          # để chắc chắn usys.S được sinh lại
make qemu           # build + boot xv6
```

Trong shell xv6:

```
$ trace 32 grep hello README
$ trace 2147483647 grep hello README
$ grep hello README
```

Hoặc chạy không cần gõ tay (grader):

```sh
./test-xv6.py usertests
```

---

## 8. Quy định nộp bài (nhắc lại từ đề)

- Nhóm tối đa 3 SV; tên file `MSSV1_MSSV2_MSSV3.zip`.
- Gồm: `..._Report.pdf`, patch `git diff > ....patch`, source đã `make clean`.
- Nộp trên Moodle.

Tạo patch:

```sh
git diff > MSSV1_MSSV2_MSSV3.patch
```
