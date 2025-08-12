#include "fs_lvgl.hpp"
#include <lvgl.h>
#include <LittleFS.h>

static void* _open(lv_fs_drv_t* drv, const char* path, lv_fs_mode_t mode) {
  (void)drv;
  const char* in = path;
  char normalized[256];
  // strip leading '/' if present
  if (in[0] == '/') snprintf(normalized, sizeof(normalized), "%s", in + 1);
  else snprintf(normalized, sizeof(normalized), "%s", in);
  // LittleFS absolute path
  char fsPath[260];
  snprintf(fsPath, sizeof(fsPath), "/%s", normalized);
  const char* modeStr = (mode == LV_FS_MODE_WR) ? "w" : "r";
  File f = LittleFS.open(fsPath, modeStr);
  if (!f) return nullptr;
  return new File(f);
}
static lv_fs_res_t _close(lv_fs_drv_t* drv, void* file_p) {
  (void)drv; File* f = static_cast<File*>(file_p); f->close(); delete f; return LV_FS_RES_OK;
}
static lv_fs_res_t _read(lv_fs_drv_t* drv, void* file_p, void* buf, uint32_t btr, uint32_t* br) {
  (void)drv; File* f = static_cast<File*>(file_p); *br = f->read((uint8_t*)buf, btr); return LV_FS_RES_OK;
}
static lv_fs_res_t _seek(lv_fs_drv_t* drv, void* file_p, uint32_t pos, lv_fs_whence_t whence) {
  (void)drv; File* f = static_cast<File*>(file_p);
  SeekMode m = (whence == LV_FS_SEEK_SET) ? SeekSet : (whence == LV_FS_SEEK_CUR ? SeekCur : SeekEnd);
  if (!f->seek(pos, m)) return LV_FS_RES_UNKNOWN; return LV_FS_RES_OK;
}
static lv_fs_res_t _tell(lv_fs_drv_t* drv, void* file_p, uint32_t* pos) {
  (void)drv; File* f = static_cast<File*>(file_p); *pos = f->position(); return LV_FS_RES_OK;
}

void lv_fs_littlefs_init() {
  static lv_fs_drv_t drv;
  lv_fs_drv_init(&drv);
  drv.letter = 'L';
  drv.open_cb = _open;
  drv.close_cb = _close;
  drv.read_cb  = _read;
  drv.seek_cb  = _seek;
  drv.tell_cb  = _tell;
  lv_fs_drv_register(&drv);
}
