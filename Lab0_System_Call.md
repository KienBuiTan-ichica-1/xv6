# Đồ án 0 – System Call (xv6)

## 1. Mục tiêu

Thêm system call `trace` vào xv6 (RISC-V) để theo dõi các system call của một tiến trình. Qua đó hiểu luồng: user program → stub `ecall` → kernel `syscall()` → hàm `sys_*`.

## 2. Yêu cầu chính

- **`trace(mask)`**: bit thứ *n* trong mask bật → in ra khi syscall số *n* sắp return.
- **Định dạng in**: `syscall <tên> -> <giá trị trả về>`
- Chỉ ảnh hưởng tiến trình gọi `trace` (và con của nó, do kế thừa qua `fork`), không ảnh hưởng tiến trình khác.
- **`user/trace.c`**: chạy `trace <mask> <lệnh> [args...]`.

Ví dụ kết quả mong đợi:

```
$ trace 32 grep hello README
syscall read -> 1023
syscall read -> 966
syscall read -> 70
syscall read -> 0
$
$ trace 2147483647 grep hello README
syscall trace -> 0
syscall exec -> 3
syscall open -> 3
syscall read -> 1023
syscall read -> 966
syscall read -> 70
syscall read -> 0
syscall close -> 0
$
$ grep hello README
$
```

## 3. Các bước cài đặt

| # | File | Việc cần làm |
|---|------|--------------|
| 1 | `kernel/syscall.h` | `#define SYS_trace 22` (số tiếp theo sau syscall cuối) |
| 2 | `kernel/syscall.c` | `extern uint64 sys_trace(void);` + `[SYS_trace] = sys_trace` trong bảng `syscalls[]` |
| 3 | `kernel/proc.h` | Thêm `int trace_mask;` vào `struct proc` |
| 4 | `kernel/sysproc.c` | `sys_trace()`: `argint(0, &mask)` rồi lưu vào `myproc()->trace_mask` |
| 5 | `kernel/proc.c` | Trong `fork()`: `np->trace_mask = p->trace_mask;`. Trong `allocproc()`/`freeproc()`: khởi tạo/reset về 0 |
| 6 | `kernel/syscall.c` | Thêm mảng `syscall_names[]`; trong `syscall()` sau khi gán `p->trapframe->a0`, nếu `(p->trace_mask >> num) & 1` thì `printf("syscall %s -> %d\n", ...)` |
| 7 | `user/user.h` | `int trace(int);` |
| 8 | `user/usys.pl` | `entry("trace");` |
| 9 | `user/trace.c` | Gọi `trace(atoi(argv[1]))` rồi `exec(argv[2], &argv[2])` |
| 10 | `Makefile` | Thêm `$U/_trace\` vào `UPROGS` |

### Hướng dẫn chung: thêm một system call (ví dụ `hello`)

**Kernel space**

1. `syscall.h`: `#define SYS_hello 22`
2. `syscall.c`: khai báo `extern int sys_hello(void);` và thêm `[SYS_hello] = sys_hello,` vào bảng `syscalls[]`
3. `sysproc.c`: cài đặt hàm `sys_hello`

**User space**

4. `user.h`: `int hello(void);`
5. `usys.pl`: `entry("hello");`
6. Viết chương trình test `testhello.c`

**Biên dịch & chạy**

7. `Makefile`: thêm `$U/_testhello\`
8. `make qemu` rồi chạy `./testhello`

### Truyền tham số / trả dữ liệu

- Lấy tham số: `argint()`, `argaddr()`, `fetchaddr()` (gọi `copyin()`).
- Trả dữ liệu về user: `copyout()`.
- Ví dụ tham khảo: `sys_fstat()`, `sys_exec()` (`kernel/sysfile.c`), `filestat()` (`kernel/file.c`).

## 4. Các điểm dễ sai

- **Không kế thừa mask khi `fork`**: các chương trình dùng `fork` sẽ không được trace nếu quên bước 5.
- **In trước khi có return value**: phải in *sau* khi gán `a0 = syscalls[num]()`.
- **Thứ tự mảng tên syscall** phải khớp số hiệu trong `syscall.h` (dùng `[SYS_read] = "read"` để tránh lệch).
- **Mask 2147483647** (31 bit thấp): ví dụ mẫu in cả `trace -> 0` ở đầu vì `trace` cũng là syscall được bật.
- **Quên `make clean`/chạy lại `make qemu`** sau khi sửa `usys.pl` → `usys.S` không cập nhật.
- **`trace.c` thiếu kiểm tra** `argc < 3` và kiểm tra lỗi của `exec`.

## 5. Quy định nộp bài

- Nhóm tối đa **3 sinh viên**.
- Tên file: `MSSV1_MSSV2_MSSV3.zip` (MSSV1 < MSSV2 < MSSV3). Ví dụ: `2312001_2312002_2312003.zip`.
- Nội dung gồm:
  1. `MSSV1_MSSV2_MSSV3_Report.pdf`: ngắn gọn, không chèn source; nêu vấn đề gặp phải, hướng đã thử, phần chưa hiểu để được xét điểm một phần.
  2. **Release**: file patch (`git diff > MSSV1_MSSV2_MSSV3.patch`).
  3. **Source**: zip xv6 đã `make clean`.
- Nộp trên **Moodle**, không nhận email hay hình thức khác.
- **Cảnh báo**: bài giống nhau bị 0 điểm toàn bộ phần thực hành.
- **Phỏng vấn demo**: chấm theo mức hiểu code và đóng góp của từng thành viên.

## 6. Điểm số

| STT | Bài tập | Điểm |
|-----|---------|------|
| 1 | trace | 10 |

Kết quả đúng khi 3 lệnh mẫu cho output giống đề.

## 7. Gợi ý chuẩn bị phỏng vấn

- Vì sao dùng `argint` thay vì đọc trực tiếp tham số (tham số nằm trong `trapframe->a0..a5`).
- Luồng: `ecall` → `usertrap()` → `syscall()` → bảng `syscalls[]`.
- Vì sao mask lưu trong `struct proc` và vì sao phải copy khi `fork`.

## 8. Tham khảo

- https://pdos.csail.mit.edu/6.1810/2023/labs/syscall.html
- Xv6 book: Chương 2, mục 4.3 và 4.4 (Chương 4)
- File liên quan: `user/usys.pl`, `user/user.h`, `kernel/syscall.c`, `kernel/syscall.h`, `kernel/proc.h`, `kernel/proc.c`
