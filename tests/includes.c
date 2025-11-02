#ifndef MEGA_HEADERS_PORTABLE_H
#define MEGA_HEADERS_PORTABLE_H 1

/*================= プリセット =================*/
#define MEGA_ISO_EXTRA 1
#define MEGA_POSIX_BASIC 1
#define MEGA_POSIX_FS 1
#define MEGA_POSIX_NET 1
#define MEGA_POSIX_THREADS 1
#define MEGA_GNU_EXTRAS 1
#define MEGA_LINUX_UAPI 1
#define MEGA_BSD 0

/*================= ISO C（常時） =================*/
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*================= ISO C 拡張（スイッチ式） =================*/
#if MEGA_ISO_EXTRA
#include <complex.h>
#include <fenv.h>
#include <float.h>
#include <iso646.h>
#include <math.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdnoreturn.h>
#include <tgmath.h>
#include <threads.h>
#include <uchar.h>
#include <wchar.h>
#include <wctype.h>
/* C23 以降（対応していなければこの2つは消してOK） */
#include <stdbit.h>
#include <stdckdint.h>
#endif

/*================= POSIX 基本（プロセス/端末/正規表現など） =================*/
#if MEGA_POSIX_BASIC
#include <aio.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <grp.h>
#include <iconv.h>
#include <libgen.h> /* basename/dirname */
#include <link.h>
#include <mqueue.h>
#include <poll.h>
#include <pwd.h>
#include <regex.h>
#include <semaphore.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <utime.h>
#include <utmpx.h>
#include <wordexp.h>
#endif

/*================= POSIX ファイルシステム情報 =================*/
#if MEGA_POSIX_FS
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/xattr.h>
#endif

/*================= ネットワーク系 =================*/
#if MEGA_POSIX_NET
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <resolv.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

/*================= POSIX スレッド =================*/
#if MEGA_POSIX_THREADS
#include <pthread.h>
#endif

/*================= GNU/glibc 便利拡張 =================*/
#if MEGA_GNU_EXTRAS
#include <byteswap.h>
#include <endian.h>
#include <error.h>
#include <execinfo.h>
#include <getopt.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#endif

/*================= Linux UAPI 群 =================*/
#if MEGA_LINUX_UAPI
#include <linux/bpf.h>
#include <linux/if_packet.h>
#include <linux/if_tun.h>
#include <linux/limits.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#endif

/*================= BSD/Darwin（必要なときだけ） =================*/
#if MEGA_BSD
#include <err.h>
#include <sys/event.h> /* kqueue */
#include <sys/queue.h> /* TAILQ/LIST */
#endif

#endif
