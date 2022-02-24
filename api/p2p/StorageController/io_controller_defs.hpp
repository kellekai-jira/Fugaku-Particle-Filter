#ifndef _IO_CONTROLLER_DEFS_H_
#define _IO_CONTROLLER_DEFS_H_

#define IO_TRY( expr, result, msg ) do { \
  if( (expr) != (result) ) \
    throw IoException( (msg), __FILE__, __LINE__, __func__); \
  } while(0)

const size_t IO_TRANSFER_SIZE = 1*1024*1024; // 1 Mb

typedef int64_t io_id_t;

enum io_status_t {
  IO_STATE_BUSY,
  IO_STATE_IDLE,
  IO_STATE_DONE
};

enum io_level_t {
  IO_STORAGE_L1,
  IO_STORAGE_L2,
  IO_STORAGE_L3,
  IO_STORAGE_L4
};

enum io_result_t {
  IO_SUCCESS,
  IO_FAILURE
};

enum io_type_t {
  IO_DOUBLE,
  IO_BYTE,
  IO_INT,
  IO_USER1,
  IO_USER2,
  IO_USER3
};

enum io_msg_t {
  IO_MSG_ALL,
  IO_MSG_ONE,
  IO_MSG_MST
};

enum io_tag_t {
  IO_TAG_LOAD,
  IO_TAG_PEER,
  IO_TAG_PULL,
  IO_TAG_PUSH,
  IO_TAG_POST,
  IO_TAG_DUMP,
  IO_TAG_FINI,
  IO_TAG_WORK
};

typedef struct io_zip_t {
  io_zip_t( int m, int p ) : mode(m), parameter(p) {}
  int mode;
  int parameter;
} io_zip_t;

//enum io_zip_type_t {
//  IO_ZIP_TYPE_DEFAULT = 0,
//  IO_ZIP_TYPE_A,
//  IO_ZIP_TYPE_B,
//  IO_ZIP_TYPE_C,
//  IO_ZIP_TYPE_D,
//  IO_ZIP_TYPE_E,
//  IO_ZIP_TYPE_F,
//  IO_ZIP_TYPE_G
//};
//
//enum io_zip_mode_t {
//  IO_ZIP_MODE_DEFAULT = 0,
//  IO_ZIP_MODE_A,
//  IO_ZIP_MODE_B,
//  IO_ZIP_MODE_C,
//  IO_ZIP_MODE_D,
//  IO_ZIP_MODE_E,
//  IO_ZIP_MODE_F,
//  IO_ZIP_MODE_G
//};

struct io_state_id_t {
  io_state_id_t( io_id_t _t, io_id_t _id ) : t(_t), id(_id) {}
  io_state_id_t() : t(0), id(0) {}
	io_id_t t;
  io_id_t id;
};

//struct io_zip_t {
//  io_zip_t() : mode(IO_ZIP_MODE_DEFAULT), type(IO_ZIP_TYPE_DEFAULT), parameter(0) {}
//  io_zip_mode_t mode;
//  int parameter;
//  io_zip_type_t type;
//};

struct io_var_t {
  io_id_t id;
  void* data;
  size_t size;
  io_type_t type;
  //io_zip_t zip;
};

struct io_ckpt_t {
  io_state_id_t state_id;
  io_level_t level;
};

#endif // _IO_CONTROLLER_DEFS_H_
