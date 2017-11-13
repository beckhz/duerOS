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

#ifndef _MBED_PACK_INCLUDE_
#define _MBED_PACK_INCLUDE_

#ifdef __cplusplus
#define MBEDPACK_EXTERN extern "C"
#else
#define MBEDPACK_EXTERN extern
#endif

#include "pack_include.h"
#include <stdint.h>

///
/// all modules type can included in package
/*typedef enum _module_type {
	ModuleTypeALL,
	ModuleTypeJS,
	ModuleTypeSO,
	ModuleTypeBIN,
	ModuleTypeJSON,
	ModuleTypeIMG
} module_type_t;
*/
///
/// package type : a package can contain app, os or profile
/*typedef enum _package_type {
	PackageTypeApp,
	PackageTypeOS,
	PackageTypeProfile,
	PackageTypeUnknown
} package_type_t;
*/
/*
typedef enum {
	false = 0,
	true
} bool;*/


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * verification init
 *
 * \return verification context if success, or NULL
 */
MBEDPACK_EXTERN void* mbed_verification_init();

/**
 * verification update context
 *
 * \param ctx verification context
 * \param buffer data to update context
 * \param buffer_size data size
 */
MBEDPACK_EXTERN void mbed_verification_update_ctx(void* ctx, unsigned char* buffer, uint32_t buffer_size);

/**
 * verify
 *
 * \param ctx verification context
 *
 * \return 0 if success, or fail
 */
MBEDPACK_EXTERN int mbed_verification(void* ctx);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 *
 * package decompress
 *
 * \return decompress context
 */
MBEDPACK_EXTERN void* mbed_decompress_init();


/**
 *
 * package decompress process
 *
 * \param ctx decompress context
 * \param buffer data to decompress
 * \param bufferSZ buffer size
 * \param lFileHandle fs file index
 *
 * \return process state
 */
MBEDPACK_EXTERN int mbed_decompress_process(void * verify_cxt, void* ctx, unsigned char* buffer, uint32_t bufferSZ, struct IOtaUpdater *updater, void *update_cxt);


/**
 *
 * package decompress uninit
 *
 * \param ctx decompress context
 *
 */
MBEDPACK_EXTERN void mbed_decompress_uninit(void* ctx);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/**
 *
 * package info collector init
 *
 * \return ctx collector context
 *
 */
MBEDPACK_EXTERN void* mbed_data_collector_init(unsigned char* filename);


/**
 *
 * package info collector uninit
 *
 * \param ctx collector context
 *
 */
MBEDPACK_EXTERN void mbed_data_collector_uninit(void* ctx);


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * get a certain type module count

 *
 * \param ctx package context
 * \param type module type of js , bin or all
 * \return module count of a certain type if success, or 0
 */
MBEDPACK_EXTERN uint8_t pkg_get_module_count(void* ctx, module_type_t type);


/**

 * get module handle
 *
 * \param ctx package context
 * \param type module type of js , bin or all

 * \param index index of module
 * \return module handle of a certain type if success, or 0
 */
MBEDPACK_EXTERN void* /*handle*/pkg_acquire_module(void* ctx, module_type_t type, int index);


/**
 * release module handle
 *
 * \param handle module handle to release
 */
MBEDPACK_EXTERN void pkg_release_module(void* handle);


/**
 * get module name
 *
 * \param handle module handle to operate module
 * \return module name if success, or NULL
 */
MBEDPACK_EXTERN const char* pkg_get_module_name(void* handle);


/**
 * get module version : software version or hardware version
 *
 * \param handle module handle to operate module
 * \param hardware_version version caller cared
 * \return module version if success, or NULL
 */
MBEDPACK_EXTERN const char* pkg_get_module_version(void* handle, bool hardware_version);


/**
 * get module offset and size, user can read module data from fs
 *
 * \param handle module handle to operate module
 * \param offset offset to the beginning of package
 * \param size module size if success or 0
 * \return 0 if success, or fail
 */
MBEDPACK_EXTERN int pkg_locate_module_data(void* handle, uint32_t* offset, uint32_t* size);

/**
 * get package name
 *
 * \param ctx package context
 * \param name package name
 * \param name_length length of name
 * \return 0 if success, 1 means no enough length buffer and return length need, or failed
 */
MBEDPACK_EXTERN int pkg_get_pkg_name(void* ctx, unsigned char* name, uint8_t* name_length);

/**
 * get package type
 *
 * \param ctx package context
 * \param type package type
 * \return 0 if success, or fail
 */
MBEDPACK_EXTERN int pkg_get_pkg_type(void* ctx, package_type_t* type);

#endif
