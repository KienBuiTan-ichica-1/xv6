// [FILE MỚI] user/trace.c
// Chương trình user: trace <mask> <lệnh> [args...]
// Bật trace cho các syscall được chỉ định bởi <mask>, sau đó thay thế
// chính tiến trình này bằng <lệnh> (nhờ exec), nên mask vẫn còn hiệu lực.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // Cần ít nhất: trace <mask> <lệnh>  => argc >= 3
  if (argc < 3) {
    fprintf(2, "usage: trace mask command [args...]\n");
    exit(1);
  }

  // argv[1] là mask dạng chuỗi (ví dụ "32"); atoi() đổi sang int.
  // Gọi syscall trace() -> kernel lưu mask vào myproc()->trace_mask.
  trace(atoi(argv[1]));

  // Thực thi lệnh. &argv[2] truyền cả tên lệnh và các tham số còn lại,
  // ví dụ: argv = {trace, 32, grep, hello, README, 0}
  //        &argv[2] = {grep, hello, README, 0}
  // exec() giữ nguyên trace_mask của tiến trình (vì không tạo tiến trình mới).
  exec(argv[2], &argv[2]);

  // Chỉ tới đây nếu exec() thất bại (exec return -1 khi lỗi).
  fprintf(2, "trace: exec %s failed\n", argv[2]);
  exit(1);
}
