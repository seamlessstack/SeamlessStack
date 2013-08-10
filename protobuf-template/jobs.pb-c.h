/* Generated by the protocol buffer compiler.  DO NOT EDIT! */

#ifndef PROTOBUF_C_jobs_2eproto__INCLUDED
#define PROTOBUF_C_jobs_2eproto__INCLUDED

#include <google/protobuf-c/protobuf-c.h>

PROTOBUF_C_BEGIN_DECLS


typedef struct _NfsReadCmd NfsReadCmd;
typedef struct _NfsData NfsData;
typedef struct _NfsWriteCmd NfsWriteCmd;
typedef struct _NfsCreateCmd NfsCreateCmd;
typedef struct _NfsAccessCmd NfsAccessCmd;
typedef struct _NfsRenameCmd NfsRenameCmd;
typedef struct _NfsCommitCmd NfsCommitCmd;
typedef struct _SstackNfsCommandStruct SstackNfsCommandStruct;
typedef struct _NfsLookupResp NfsLookupResp;
typedef struct _NfsAccessResp NfsAccessResp;
typedef struct _NfsReadResp NfsReadResp;
typedef struct _NfsWriteResp NfsWriteResp;
typedef struct _NfsCreateResp NfsCreateResp;
typedef struct _NfsRemoveResp NfsRemoveResp;
typedef struct _NfsRenameResp NfsRenameResp;
typedef struct _NfsFsstatResp NfsFsstatResp;
typedef struct _NfsFsinfoResp NfsFsinfoResp;
typedef struct _NfsPathconfResp NfsPathconfResp;
typedef struct _NfsCommitResp NfsCommitResp;
typedef struct _SstackNfsResultStruct SstackNfsResultStruct;
typedef struct _SstackPayloadHdrT SstackPayloadHdrT;
typedef struct _SstackPayloadT SstackPayloadT;


/* --- enums --- */

typedef enum _SstackPayloadT__SstackNfsCommandT {
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_GETATTR = 1,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_SETATTR = 2,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_LOOKUP = 3,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_ACCESS = 4,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_READLINK = 5,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_READ = 6,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_WRITE = 7,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_CREATE = 8,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_MKDIR = 9,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_SYMLINK = 10,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_MKNOD = 11,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_REMOVE = 12,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_RMDIR = 13,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_RENAME = 14,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_LINK = 15,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_READDIR = 16,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_READDIRPLUS = 17,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_FSSTAT = 18,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_FSINFO = 19,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_PATHCONF = 20,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_COMMIT = 21,
  SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_VALID_MAX = 21
} SstackPayloadT__SstackNfsCommandT;

/* --- messages --- */

struct  _NfsReadCmd
{
  ProtobufCMessage base;
  uint64_t offset;
  uint64_t count;
  NfsData *data;
};
#define NFS_READ_CMD__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_read_cmd__descriptor) \
    , 0, 0, NULL }


struct  _NfsData
{
  ProtobufCMessage base;
  uint64_t data_len;
  ProtobufCBinaryData data_val;
};
#define NFS_DATA__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_data__descriptor) \
    , 0, {0,NULL} }


struct  _NfsWriteCmd
{
  ProtobufCMessage base;
  uint64_t offset;
  uint64_t count;
  NfsData *data;
};
#define NFS_WRITE_CMD__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_write_cmd__descriptor) \
    , 0, 0, NULL }


struct  _NfsCreateCmd
{
  ProtobufCMessage base;
  uint32_t mode;
  NfsData *data;
};
#define NFS_CREATE_CMD__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_create_cmd__descriptor) \
    , 0, NULL }


struct  _NfsAccessCmd
{
  ProtobufCMessage base;
  uint32_t mode;
};
#define NFS_ACCESS_CMD__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_access_cmd__descriptor) \
    , 0 }


struct  _NfsRenameCmd
{
  ProtobufCMessage base;
  ProtobufCBinaryData new_path;
};
#define NFS_RENAME_CMD__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_rename_cmd__descriptor) \
    , {0,NULL} }


struct  _NfsCommitCmd
{
  ProtobufCMessage base;
  uint64_t offset;
  uint64_t count;
};
#define NFS_COMMIT_CMD__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_commit_cmd__descriptor) \
    , 0, 0 }


struct  _SstackNfsCommandStruct
{
  ProtobufCMessage base;
  NfsReadCmd *read_cmd;
  NfsWriteCmd *write_cmd;
  NfsCreateCmd *create_cmd;
  NfsAccessCmd *access_cmd;
  NfsRenameCmd *rename_cmd;
  NfsCommitCmd *commit_cmd;
};
#define SSTACK_NFS_COMMAND_STRUCT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&sstack_nfs_command_struct__descriptor) \
    , NULL, NULL, NULL, NULL, NULL, NULL }


struct  _NfsLookupResp
{
  ProtobufCMessage base;
  ProtobufCBinaryData lookup_path;
};
#define NFS_LOOKUP_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_lookup_resp__descriptor) \
    , {0,NULL} }


struct  _NfsAccessResp
{
  ProtobufCMessage base;
  uint32_t access;
};
#define NFS_ACCESS_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_access_resp__descriptor) \
    , 0 }


struct  _NfsReadResp
{
  ProtobufCMessage base;
  uint64_t count;
  int32_t eof;
  NfsData *data;
};
#define NFS_READ_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_read_resp__descriptor) \
    , 0, 0, NULL }


struct  _NfsWriteResp
{
  ProtobufCMessage base;
  uint32_t file_write_ok;
  uint32_t file_wc;
};
#define NFS_WRITE_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_write_resp__descriptor) \
    , 0, 0 }


struct  _NfsCreateResp
{
  ProtobufCMessage base;
  uint32_t file_create_ok;
  uint32_t file_wc;
};
#define NFS_CREATE_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_create_resp__descriptor) \
    , 0, 0 }


struct  _NfsRemoveResp
{
  ProtobufCMessage base;
  uint32_t file_remove_ok;
};
#define NFS_REMOVE_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_remove_resp__descriptor) \
    , 0 }


struct  _NfsRenameResp
{
  ProtobufCMessage base;
  uint32_t file_rename_ok;
};
#define NFS_RENAME_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_rename_resp__descriptor) \
    , 0 }


struct  _NfsFsstatResp
{
  ProtobufCMessage base;
};
#define NFS_FSSTAT_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_fsstat_resp__descriptor) \
     }


struct  _NfsFsinfoResp
{
  ProtobufCMessage base;
};
#define NFS_FSINFO_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_fsinfo_resp__descriptor) \
     }


struct  _NfsPathconfResp
{
  ProtobufCMessage base;
};
#define NFS_PATHCONF_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_pathconf_resp__descriptor) \
     }


struct  _NfsCommitResp
{
  ProtobufCMessage base;
};
#define NFS_COMMIT_RESP__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&nfs_commit_resp__descriptor) \
     }


struct  _SstackNfsResultStruct
{
  ProtobufCMessage base;
  NfsLookupResp *lookup_resp;
  NfsAccessResp *access_resp;
  NfsReadResp *read_resp;
  NfsWriteResp *write_resp;
  NfsCreateResp *create_resp;
  NfsRemoveResp *remove_resp;
  NfsRenameResp *rename_resp;
  NfsFsstatResp *fsstat_resp;
  NfsFsinfoResp *fsinfo_resp;
  NfsPathconfResp *pathconf_resp;
  NfsCommitResp *commit_resp;
};
#define SSTACK_NFS_RESULT_STRUCT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&sstack_nfs_result_struct__descriptor) \
    , NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }


struct  _SstackPayloadHdrT
{
  ProtobufCMessage base;
  uint32_t sequence;
  uint32_t payload_len;
};
#define SSTACK_PAYLOAD_HDR_T__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&sstack_payload_hdr_t__descriptor) \
    , 0, 0 }


struct  _SstackPayloadT
{
  ProtobufCMessage base;
  SstackPayloadHdrT *hdr;
  SstackPayloadT__SstackNfsCommandT command;
  SstackNfsCommandStruct *command_struct;
  SstackNfsResultStruct *result_struct;
};
#define SSTACK_PAYLOAD_T__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&sstack_payload_t__descriptor) \
    , NULL, 0, NULL, NULL }


/* NfsReadCmd methods */
void   nfs_read_cmd__init
                     (NfsReadCmd         *message);
size_t nfs_read_cmd__get_packed_size
                     (const NfsReadCmd   *message);
size_t nfs_read_cmd__pack
                     (const NfsReadCmd   *message,
                      uint8_t             *out);
size_t nfs_read_cmd__pack_to_buffer
                     (const NfsReadCmd   *message,
                      ProtobufCBuffer     *buffer);
NfsReadCmd *
       nfs_read_cmd__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_read_cmd__free_unpacked
                     (NfsReadCmd *message,
                      ProtobufCAllocator *allocator);
/* NfsData methods */
void   nfs_data__init
                     (NfsData         *message);
size_t nfs_data__get_packed_size
                     (const NfsData   *message);
size_t nfs_data__pack
                     (const NfsData   *message,
                      uint8_t             *out);
size_t nfs_data__pack_to_buffer
                     (const NfsData   *message,
                      ProtobufCBuffer     *buffer);
NfsData *
       nfs_data__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_data__free_unpacked
                     (NfsData *message,
                      ProtobufCAllocator *allocator);
/* NfsWriteCmd methods */
void   nfs_write_cmd__init
                     (NfsWriteCmd         *message);
size_t nfs_write_cmd__get_packed_size
                     (const NfsWriteCmd   *message);
size_t nfs_write_cmd__pack
                     (const NfsWriteCmd   *message,
                      uint8_t             *out);
size_t nfs_write_cmd__pack_to_buffer
                     (const NfsWriteCmd   *message,
                      ProtobufCBuffer     *buffer);
NfsWriteCmd *
       nfs_write_cmd__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_write_cmd__free_unpacked
                     (NfsWriteCmd *message,
                      ProtobufCAllocator *allocator);
/* NfsCreateCmd methods */
void   nfs_create_cmd__init
                     (NfsCreateCmd         *message);
size_t nfs_create_cmd__get_packed_size
                     (const NfsCreateCmd   *message);
size_t nfs_create_cmd__pack
                     (const NfsCreateCmd   *message,
                      uint8_t             *out);
size_t nfs_create_cmd__pack_to_buffer
                     (const NfsCreateCmd   *message,
                      ProtobufCBuffer     *buffer);
NfsCreateCmd *
       nfs_create_cmd__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_create_cmd__free_unpacked
                     (NfsCreateCmd *message,
                      ProtobufCAllocator *allocator);
/* NfsAccessCmd methods */
void   nfs_access_cmd__init
                     (NfsAccessCmd         *message);
size_t nfs_access_cmd__get_packed_size
                     (const NfsAccessCmd   *message);
size_t nfs_access_cmd__pack
                     (const NfsAccessCmd   *message,
                      uint8_t             *out);
size_t nfs_access_cmd__pack_to_buffer
                     (const NfsAccessCmd   *message,
                      ProtobufCBuffer     *buffer);
NfsAccessCmd *
       nfs_access_cmd__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_access_cmd__free_unpacked
                     (NfsAccessCmd *message,
                      ProtobufCAllocator *allocator);
/* NfsRenameCmd methods */
void   nfs_rename_cmd__init
                     (NfsRenameCmd         *message);
size_t nfs_rename_cmd__get_packed_size
                     (const NfsRenameCmd   *message);
size_t nfs_rename_cmd__pack
                     (const NfsRenameCmd   *message,
                      uint8_t             *out);
size_t nfs_rename_cmd__pack_to_buffer
                     (const NfsRenameCmd   *message,
                      ProtobufCBuffer     *buffer);
NfsRenameCmd *
       nfs_rename_cmd__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_rename_cmd__free_unpacked
                     (NfsRenameCmd *message,
                      ProtobufCAllocator *allocator);
/* NfsCommitCmd methods */
void   nfs_commit_cmd__init
                     (NfsCommitCmd         *message);
size_t nfs_commit_cmd__get_packed_size
                     (const NfsCommitCmd   *message);
size_t nfs_commit_cmd__pack
                     (const NfsCommitCmd   *message,
                      uint8_t             *out);
size_t nfs_commit_cmd__pack_to_buffer
                     (const NfsCommitCmd   *message,
                      ProtobufCBuffer     *buffer);
NfsCommitCmd *
       nfs_commit_cmd__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_commit_cmd__free_unpacked
                     (NfsCommitCmd *message,
                      ProtobufCAllocator *allocator);
/* SstackNfsCommandStruct methods */
void   sstack_nfs_command_struct__init
                     (SstackNfsCommandStruct         *message);
size_t sstack_nfs_command_struct__get_packed_size
                     (const SstackNfsCommandStruct   *message);
size_t sstack_nfs_command_struct__pack
                     (const SstackNfsCommandStruct   *message,
                      uint8_t             *out);
size_t sstack_nfs_command_struct__pack_to_buffer
                     (const SstackNfsCommandStruct   *message,
                      ProtobufCBuffer     *buffer);
SstackNfsCommandStruct *
       sstack_nfs_command_struct__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   sstack_nfs_command_struct__free_unpacked
                     (SstackNfsCommandStruct *message,
                      ProtobufCAllocator *allocator);
/* NfsLookupResp methods */
void   nfs_lookup_resp__init
                     (NfsLookupResp         *message);
size_t nfs_lookup_resp__get_packed_size
                     (const NfsLookupResp   *message);
size_t nfs_lookup_resp__pack
                     (const NfsLookupResp   *message,
                      uint8_t             *out);
size_t nfs_lookup_resp__pack_to_buffer
                     (const NfsLookupResp   *message,
                      ProtobufCBuffer     *buffer);
NfsLookupResp *
       nfs_lookup_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_lookup_resp__free_unpacked
                     (NfsLookupResp *message,
                      ProtobufCAllocator *allocator);
/* NfsAccessResp methods */
void   nfs_access_resp__init
                     (NfsAccessResp         *message);
size_t nfs_access_resp__get_packed_size
                     (const NfsAccessResp   *message);
size_t nfs_access_resp__pack
                     (const NfsAccessResp   *message,
                      uint8_t             *out);
size_t nfs_access_resp__pack_to_buffer
                     (const NfsAccessResp   *message,
                      ProtobufCBuffer     *buffer);
NfsAccessResp *
       nfs_access_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_access_resp__free_unpacked
                     (NfsAccessResp *message,
                      ProtobufCAllocator *allocator);
/* NfsReadResp methods */
void   nfs_read_resp__init
                     (NfsReadResp         *message);
size_t nfs_read_resp__get_packed_size
                     (const NfsReadResp   *message);
size_t nfs_read_resp__pack
                     (const NfsReadResp   *message,
                      uint8_t             *out);
size_t nfs_read_resp__pack_to_buffer
                     (const NfsReadResp   *message,
                      ProtobufCBuffer     *buffer);
NfsReadResp *
       nfs_read_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_read_resp__free_unpacked
                     (NfsReadResp *message,
                      ProtobufCAllocator *allocator);
/* NfsWriteResp methods */
void   nfs_write_resp__init
                     (NfsWriteResp         *message);
size_t nfs_write_resp__get_packed_size
                     (const NfsWriteResp   *message);
size_t nfs_write_resp__pack
                     (const NfsWriteResp   *message,
                      uint8_t             *out);
size_t nfs_write_resp__pack_to_buffer
                     (const NfsWriteResp   *message,
                      ProtobufCBuffer     *buffer);
NfsWriteResp *
       nfs_write_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_write_resp__free_unpacked
                     (NfsWriteResp *message,
                      ProtobufCAllocator *allocator);
/* NfsCreateResp methods */
void   nfs_create_resp__init
                     (NfsCreateResp         *message);
size_t nfs_create_resp__get_packed_size
                     (const NfsCreateResp   *message);
size_t nfs_create_resp__pack
                     (const NfsCreateResp   *message,
                      uint8_t             *out);
size_t nfs_create_resp__pack_to_buffer
                     (const NfsCreateResp   *message,
                      ProtobufCBuffer     *buffer);
NfsCreateResp *
       nfs_create_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_create_resp__free_unpacked
                     (NfsCreateResp *message,
                      ProtobufCAllocator *allocator);
/* NfsRemoveResp methods */
void   nfs_remove_resp__init
                     (NfsRemoveResp         *message);
size_t nfs_remove_resp__get_packed_size
                     (const NfsRemoveResp   *message);
size_t nfs_remove_resp__pack
                     (const NfsRemoveResp   *message,
                      uint8_t             *out);
size_t nfs_remove_resp__pack_to_buffer
                     (const NfsRemoveResp   *message,
                      ProtobufCBuffer     *buffer);
NfsRemoveResp *
       nfs_remove_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_remove_resp__free_unpacked
                     (NfsRemoveResp *message,
                      ProtobufCAllocator *allocator);
/* NfsRenameResp methods */
void   nfs_rename_resp__init
                     (NfsRenameResp         *message);
size_t nfs_rename_resp__get_packed_size
                     (const NfsRenameResp   *message);
size_t nfs_rename_resp__pack
                     (const NfsRenameResp   *message,
                      uint8_t             *out);
size_t nfs_rename_resp__pack_to_buffer
                     (const NfsRenameResp   *message,
                      ProtobufCBuffer     *buffer);
NfsRenameResp *
       nfs_rename_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_rename_resp__free_unpacked
                     (NfsRenameResp *message,
                      ProtobufCAllocator *allocator);
/* NfsFsstatResp methods */
void   nfs_fsstat_resp__init
                     (NfsFsstatResp         *message);
size_t nfs_fsstat_resp__get_packed_size
                     (const NfsFsstatResp   *message);
size_t nfs_fsstat_resp__pack
                     (const NfsFsstatResp   *message,
                      uint8_t             *out);
size_t nfs_fsstat_resp__pack_to_buffer
                     (const NfsFsstatResp   *message,
                      ProtobufCBuffer     *buffer);
NfsFsstatResp *
       nfs_fsstat_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_fsstat_resp__free_unpacked
                     (NfsFsstatResp *message,
                      ProtobufCAllocator *allocator);
/* NfsFsinfoResp methods */
void   nfs_fsinfo_resp__init
                     (NfsFsinfoResp         *message);
size_t nfs_fsinfo_resp__get_packed_size
                     (const NfsFsinfoResp   *message);
size_t nfs_fsinfo_resp__pack
                     (const NfsFsinfoResp   *message,
                      uint8_t             *out);
size_t nfs_fsinfo_resp__pack_to_buffer
                     (const NfsFsinfoResp   *message,
                      ProtobufCBuffer     *buffer);
NfsFsinfoResp *
       nfs_fsinfo_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_fsinfo_resp__free_unpacked
                     (NfsFsinfoResp *message,
                      ProtobufCAllocator *allocator);
/* NfsPathconfResp methods */
void   nfs_pathconf_resp__init
                     (NfsPathconfResp         *message);
size_t nfs_pathconf_resp__get_packed_size
                     (const NfsPathconfResp   *message);
size_t nfs_pathconf_resp__pack
                     (const NfsPathconfResp   *message,
                      uint8_t             *out);
size_t nfs_pathconf_resp__pack_to_buffer
                     (const NfsPathconfResp   *message,
                      ProtobufCBuffer     *buffer);
NfsPathconfResp *
       nfs_pathconf_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_pathconf_resp__free_unpacked
                     (NfsPathconfResp *message,
                      ProtobufCAllocator *allocator);
/* NfsCommitResp methods */
void   nfs_commit_resp__init
                     (NfsCommitResp         *message);
size_t nfs_commit_resp__get_packed_size
                     (const NfsCommitResp   *message);
size_t nfs_commit_resp__pack
                     (const NfsCommitResp   *message,
                      uint8_t             *out);
size_t nfs_commit_resp__pack_to_buffer
                     (const NfsCommitResp   *message,
                      ProtobufCBuffer     *buffer);
NfsCommitResp *
       nfs_commit_resp__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   nfs_commit_resp__free_unpacked
                     (NfsCommitResp *message,
                      ProtobufCAllocator *allocator);
/* SstackNfsResultStruct methods */
void   sstack_nfs_result_struct__init
                     (SstackNfsResultStruct         *message);
size_t sstack_nfs_result_struct__get_packed_size
                     (const SstackNfsResultStruct   *message);
size_t sstack_nfs_result_struct__pack
                     (const SstackNfsResultStruct   *message,
                      uint8_t             *out);
size_t sstack_nfs_result_struct__pack_to_buffer
                     (const SstackNfsResultStruct   *message,
                      ProtobufCBuffer     *buffer);
SstackNfsResultStruct *
       sstack_nfs_result_struct__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   sstack_nfs_result_struct__free_unpacked
                     (SstackNfsResultStruct *message,
                      ProtobufCAllocator *allocator);
/* SstackPayloadHdrT methods */
void   sstack_payload_hdr_t__init
                     (SstackPayloadHdrT         *message);
size_t sstack_payload_hdr_t__get_packed_size
                     (const SstackPayloadHdrT   *message);
size_t sstack_payload_hdr_t__pack
                     (const SstackPayloadHdrT   *message,
                      uint8_t             *out);
size_t sstack_payload_hdr_t__pack_to_buffer
                     (const SstackPayloadHdrT   *message,
                      ProtobufCBuffer     *buffer);
SstackPayloadHdrT *
       sstack_payload_hdr_t__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   sstack_payload_hdr_t__free_unpacked
                     (SstackPayloadHdrT *message,
                      ProtobufCAllocator *allocator);
/* SstackPayloadT methods */
void   sstack_payload_t__init
                     (SstackPayloadT         *message);
size_t sstack_payload_t__get_packed_size
                     (const SstackPayloadT   *message);
size_t sstack_payload_t__pack
                     (const SstackPayloadT   *message,
                      uint8_t             *out);
size_t sstack_payload_t__pack_to_buffer
                     (const SstackPayloadT   *message,
                      ProtobufCBuffer     *buffer);
SstackPayloadT *
       sstack_payload_t__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   sstack_payload_t__free_unpacked
                     (SstackPayloadT *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*NfsReadCmd_Closure)
                 (const NfsReadCmd *message,
                  void *closure_data);
typedef void (*NfsData_Closure)
                 (const NfsData *message,
                  void *closure_data);
typedef void (*NfsWriteCmd_Closure)
                 (const NfsWriteCmd *message,
                  void *closure_data);
typedef void (*NfsCreateCmd_Closure)
                 (const NfsCreateCmd *message,
                  void *closure_data);
typedef void (*NfsAccessCmd_Closure)
                 (const NfsAccessCmd *message,
                  void *closure_data);
typedef void (*NfsRenameCmd_Closure)
                 (const NfsRenameCmd *message,
                  void *closure_data);
typedef void (*NfsCommitCmd_Closure)
                 (const NfsCommitCmd *message,
                  void *closure_data);
typedef void (*SstackNfsCommandStruct_Closure)
                 (const SstackNfsCommandStruct *message,
                  void *closure_data);
typedef void (*NfsLookupResp_Closure)
                 (const NfsLookupResp *message,
                  void *closure_data);
typedef void (*NfsAccessResp_Closure)
                 (const NfsAccessResp *message,
                  void *closure_data);
typedef void (*NfsReadResp_Closure)
                 (const NfsReadResp *message,
                  void *closure_data);
typedef void (*NfsWriteResp_Closure)
                 (const NfsWriteResp *message,
                  void *closure_data);
typedef void (*NfsCreateResp_Closure)
                 (const NfsCreateResp *message,
                  void *closure_data);
typedef void (*NfsRemoveResp_Closure)
                 (const NfsRemoveResp *message,
                  void *closure_data);
typedef void (*NfsRenameResp_Closure)
                 (const NfsRenameResp *message,
                  void *closure_data);
typedef void (*NfsFsstatResp_Closure)
                 (const NfsFsstatResp *message,
                  void *closure_data);
typedef void (*NfsFsinfoResp_Closure)
                 (const NfsFsinfoResp *message,
                  void *closure_data);
typedef void (*NfsPathconfResp_Closure)
                 (const NfsPathconfResp *message,
                  void *closure_data);
typedef void (*NfsCommitResp_Closure)
                 (const NfsCommitResp *message,
                  void *closure_data);
typedef void (*SstackNfsResultStruct_Closure)
                 (const SstackNfsResultStruct *message,
                  void *closure_data);
typedef void (*SstackPayloadHdrT_Closure)
                 (const SstackPayloadHdrT *message,
                  void *closure_data);
typedef void (*SstackPayloadT_Closure)
                 (const SstackPayloadT *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor nfs_read_cmd__descriptor;
extern const ProtobufCMessageDescriptor nfs_data__descriptor;
extern const ProtobufCMessageDescriptor nfs_write_cmd__descriptor;
extern const ProtobufCMessageDescriptor nfs_create_cmd__descriptor;
extern const ProtobufCMessageDescriptor nfs_access_cmd__descriptor;
extern const ProtobufCMessageDescriptor nfs_rename_cmd__descriptor;
extern const ProtobufCMessageDescriptor nfs_commit_cmd__descriptor;
extern const ProtobufCMessageDescriptor sstack_nfs_command_struct__descriptor;
extern const ProtobufCMessageDescriptor nfs_lookup_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_access_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_read_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_write_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_create_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_remove_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_rename_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_fsstat_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_fsinfo_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_pathconf_resp__descriptor;
extern const ProtobufCMessageDescriptor nfs_commit_resp__descriptor;
extern const ProtobufCMessageDescriptor sstack_nfs_result_struct__descriptor;
extern const ProtobufCMessageDescriptor sstack_payload_hdr_t__descriptor;
extern const ProtobufCMessageDescriptor sstack_payload_t__descriptor;
extern const ProtobufCEnumDescriptor    sstack_payload_t__sstack_nfs_command_t__descriptor;

PROTOBUF_C_END_DECLS


#endif  /* PROTOBUF_jobs_2eproto__INCLUDED */
