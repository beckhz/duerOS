/**
 * Copyright (2017) Baidu Inc. All rights reserveed.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _MBED_OTA_PACK_INCLUDE_
#define _MBED_OTA_PACK_INCLUDE_

#include <stdint.h>
#include "lightduer_log.h"

#define KEY_LEN	128

#define _DEBUG_
//#define _DEBUG_OUT_

#ifndef _DEBUG_OUT_
	#define MBEDPACK_DEBUG_PRINT DUER_LOGI
#else
	#define MBEDPACK_DEBUG_PRINT DUER_LOGI
#endif

///
/// package header
typedef struct _package_header {

   ///
   /// 'mbed' verify tag
   unsigned char tag[4];

   ///
   /// package header size
   uint32_t header_size;

   ///
   /// package signature size : 1024 bits
   uint32_t package_sig_size;
   ///
   /// package signature : include meta.json and all modules
   unsigned char package_sig[KEY_LEN];

   ///
   /// meta.json signature size : 1024 bits
   uint32_t meta_sig_size;
   ///
   /// meta.json signature
   unsigned char meta_sig[KEY_LEN];

   ///
   /// meta.json size : used for decompress meta.json from package body
   uint32_t meta_size;

   ///
   /// package size before decompress
   uint32_t ori_package_size;

} package_header_t;

///
/// all modules included in mbed package is 'js' and mbed executable '.bin' file
typedef enum _module_type {
	ModuleTypeALL,
	ModuleTypeJS,
	ModuleTypeSO,
	ModuleTypeBIN,
	ModuleTypeJSON,
	ModuleTypeIMG
} module_type_t;


typedef struct _file_type_pair {
	char* string;
	module_type_t type;
} file_type;


typedef enum _package_type {
	PackageTypeApp,
	PackageTypeOS,
	PackageTypeProfile,
	PackageTypeUnknown
} package_type_t;

typedef enum {
	false = 0,
	true
} bool;

/*
///
/// package body modules info excluding meta.json
typedef struct _module_info {

   ///
   /// module name : xx.js / xx.bin
   unsigned char* name;

   ///
   /// module type : js / bin
   module_type_t type;

   uint32_t  module_size;
   uint8_t  update;
   unsigned char* version;
   unsigned char* hw_version;

   ///
   /// module data
   unsigned char* data;
} module_info_t;

///
/// decompress info : used for extract meta data and modules array data
typedef struct _decompress_info {

   ///
   /// decompressed meta data size
   uint32_t decompress_meta_data_size;
   ///
   /// decompressed meta data from package body
   unsigned char* decompress_meta_data;

   ///
   /// decompressed modules array data size
   uint32_t decompress_module_data_size;
   ///
   /// decompressed modules array data from package body
   unsigned char* decompress_module_data;
} decompress_info_t;
*/


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct _module_info {

   ///
   /// module name : xx.js / xx.bin
   unsigned char* name;

   ///
   /// module type : js / bin
   module_type_t type;

   uint32_t  module_size;
   uint8_t  update;
   unsigned char* version;
   unsigned char* hw_version;
   uint32_t offset;  // offset from file begin

} module_info_t;

///
/// meta info : used for extract meta data and modules array data
typedef struct _meta_info {

   ///
   /// decompressed meta data size
   uint32_t meta_data_size;

   ///
   /// decompressed module data offset
   uint32_t module_data_offset;

   ///
   /// decompressed meta data
   unsigned char* meta_data;

   ///
   /// json object
   void* meta_object;
} meta_info_t;

#endif
