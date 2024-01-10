#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static fstats stats ATTRIBUTE_ALIGN(32);
#define ISFS_EEXIST -105
#define ISFS_ENOENT -106

void *ISFS_GetFile(u32 *size) {
  u64 title_id = 0;
  s32 ret = ES_GetTitleID(&title_id);
  if (ret < 0)
  {
    printf("Failed to get current title id\n");
    sleep(5);
    exit(0);
  }

  u32 lower = (u32)((title_id & 0xFFFFFFFF00000000LL) >> 32);
  u32 upper = (u32)(title_id & 0xFFFFFFFFLL);

  char path[128];
  sprintf(path, "/title/%08x/%08x/data/state.txt", lower, upper);

  *size = 0;

  s32 fd = ISFS_Open(path, ISFS_OPEN_READ);
  if (fd < 0) {
    if (fd == ISFS_ENOENT)
      return NULL;

    printf("ISFS_GetFile: unable to open file (error %d)\n", fd);
    sleep(5);
    exit(0);
  }

  void *buf = NULL;
  memset(&stats, 0, sizeof(fstats));

  ret = ISFS_GetFileStats(fd, &stats);
  if (ret >= 0) {
    s32 length = stats.file_length;

    // We must align our length by 32.
    // memalign itself is dreadfully broken for unknown reasons.
    s32 aligned_length = length;
    s32 remainder = aligned_length % 32;
    if (remainder != 0) {
      aligned_length += 32 - remainder;
    }

    buf = aligned_alloc(32, aligned_length);

    if (buf != NULL) {
      s32 tmp_size = ISFS_Read(fd, buf, length);

      if (tmp_size == length) {
        // We were successful.
        *size = tmp_size;
      } else {
        // If positive, the file could not be fully read.
        // If negative, it is most likely an underlying /dev/fs
        // error.
        if (tmp_size >= 0) {
          printf("ISFS_GetFile: only able to read %d out of "
                 "%d bytes!\n",
                 tmp_size, length);
          sleep(5);
          exit(0);
        } else if (tmp_size == ISFS_ENOENT) {
          // We ignore logging errors about files that do not exist.
        } else {
          printf("ISFS_GetFile: ISFS_Open failed! (error %d)\n",
                 tmp_size);
          sleep(5);
          exit(0);
        }

        free(buf);
      }
    } else {
      printf("ISFS_GetFile: failed to allocate buffer!\n");
      sleep(5);
      exit(0);
    }
  } else {
    printf("ISFS_GetFile: unable to retrieve file stats (error %d)\n", ret);
    sleep(5);
    exit(0);
  }
  ISFS_Close(fd);

  return buf;
}
