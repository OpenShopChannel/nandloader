#include <gccore.h>
#include <unistd.h>
#include <stdio.h>
#include <sdcard/wiisd_io.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <fat.h>

#include "stub_bin.h"
#include "isfs.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

// I/O devices
const DISC_INTERFACE *sd_slot = &__io_wiisd;
const DISC_INTERFACE *usb = &__io_usbstorage;

typedef struct _dolheader {
    u32 text_pos[7];
    u32 data_pos[11];
    u32 text_start[7];
    u32 data_start[11];
    u32 text_size[7];
    u32 data_size[11];
    u32 bss_start;
    u32 bss_size;
    u32 entry_point;
} dolheader;

typedef void (*entry_point) (void);

// How Homebrew Channel relocates the dol into mem.
static void reloc(entry_point *ep, const u8 *addr, u32 size) {
  u32 i;
  dolheader *dolfile;

  dolfile = (dolheader *) addr;

  for (i = 0; i < 7; i++) {
    if (!dolfile->text_size[i])
      continue;

    if (dolfile->text_pos[i] + dolfile->text_size[i] > size)
      return;

    memmove ((void *) dolfile->text_start[i], addr + dolfile->text_pos[i],
             dolfile->text_size[i]);

    DCFlushRange ((void *) dolfile->text_start[i], dolfile->text_size[i]);
    ICInvalidateRange ((void *) dolfile->text_start[i],
                       dolfile->text_size[i]);
  }

  for(i = 0; i < 11; i++) {
    if (!dolfile->data_size[i])
      continue;

    if (dolfile->data_pos[i] + dolfile->data_size[i] > size)
      return;

    memmove ((void*) dolfile->data_start[i], addr + dolfile->data_pos[i],
             dolfile->data_size[i]);

    DCFlushRange ((void *) dolfile->data_start[i], dolfile->data_size[i]);
  }

  *ep = (entry_point) dolfile->entry_point;
}

static int load(entry_point *ep) {
  // OSC executable will always be in content 2
  s32 fd = ES_OpenContent(2);
  if (fd < 0)
    return -1;


  u32 data_size = ES_SeekContent(fd, 0, SEEK_END);
  // TODO: Bounds on the size in TMD?
  if (data_size <= sizeof(dolheader))
    return -1;

  ES_SeekContent(fd, 0, SEEK_SET);

  s32 aligned_length = data_size;
  s32 remainder = aligned_length % 32;
  if (remainder != 0) {
    aligned_length += 32 - remainder;
  }

  void* buf = aligned_alloc(32, aligned_length);
  ES_ReadContent(fd, buf, data_size);
  ES_CloseContent(fd);

  reloc(ep, (u8*)buf, data_size);
  return 0;
}

s32 init_fat() {
  // Initialize IO
  usb->startup();
  sd_slot->startup();

  // Check if the SD Card is inserted
  bool isInserted = __io_wiisd.isInserted();

  // Try to mount the SD Card before the USB
  if (isInserted) {
    fatMountSimple("fat", sd_slot);
  } else {
    // Since the SD Card is not inserted, we will attempt to mount the USB.
    bool USB = __io_usbstorage.isInserted();
    if (USB) {
      fatMountSimple("fat", usb);
    } else {
      __io_usbstorage.shutdown();
      __io_wiisd.shutdown();
      printf("No SD Card or USB is inserted");
      sleep(5);
      return -1;
    }
  }

  return 0;
}

int main() {
  // Stub from the homebrew channel
  memcpy((u32 *) 0x80001800, stub_bin, stub_bin_size);
  DCFlushRange((u32 *) 0x80001800, stub_bin_size);

  VIDEO_Init();

  rmode = VIDEO_GetPreferredMode(NULL);
  xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
               rmode->fbWidth * VI_DISPLAY_PIX_SZ);
  VIDEO_Configure(rmode);
  VIDEO_SetNextFramebuffer(xfb);
  VIDEO_SetBlack(FALSE);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  if (rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync();

  // Determine if we boot from the SD Card or run the installer
  ISFS_Initialize();
  init_fat();

  u32 size = 0;
  void* data = ISFS_GetFile(&size);
  if (data)
  {
    // Load from I/O
    // We first need to null terminate the buffer.
    char* file_path = (char*)data;
    file_path[size] = '\0';
    FILE* fp = fopen((char*)file_path, "rb");
    if (!fp)
    {
      printf("Application no longer exists. Please reinstall from the Open Shop Channel");
      sleep(5);
      return 0;
    }

    fseek(fp,0,SEEK_END);
    size_t fsize = ftell(fp);
    fseek(fp,0,SEEK_SET);

    void* buf = malloc(fsize+1);
    fread(buf, 1, fsize, fp);

    entry_point ep = 0;
    reloc(&ep, (u8*)buf, fsize);
    ep();
  }

  entry_point ep = 0;
  int ret = load(&ep);
  if (ret < 0 || ep == 0)
  {
    printf("unable to boot executable");
    sleep(5);
    return 0;
  }

  ep();

  return 0;
}
