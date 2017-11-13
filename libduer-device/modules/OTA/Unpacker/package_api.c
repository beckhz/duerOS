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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "verification.h"
#include "decompress.h"
#include "datacollector.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"

void* mbed_verification_init()
{
   return (void*)mbed_hash_init();
}

void mbed_verification_update_ctx(void* ctx, unsigned char* buffer, uint32_t buffer_size)
{
   mbed_hash_update_key(ctx, buffer, buffer_size);
}

int mbed_verification(void* ctx)
{
   int ret = 0;
   mbedtls_rsa_context rsa_ctx = {0};
   //uint32_t package_bin_size = 0;

   if (mbed_rsa_ca_pkcs1_init(&rsa_ctx) != 0) {
	MBEDPACK_DEBUG_PRINT("func--mbed_verification : init rsa context failed!!!\n\r");
	mbed_hash_uninit(ctx);
	return -1;
   }
   MBEDPACK_DEBUG_PRINT("func--mbed_verification : trace --- 1\n\r");
   //for(int j = rsa_ctx.N.n; j > 0; j-- )
   //	{
	//	printf("Y->p[%d]=%d ", j - 1, rsa_ctx.N.p[j - 1]);
   //	}
   ret = mbed_rsa_ca_pkcs1_verify(&rsa_ctx, ctx);
   if (ret == -1) {
	MBEDPACK_DEBUG_PRINT("func--mbed_verification : verify failed, illegal package!!!\n\r");
	mbed_rsa_ca_pkcs1_uninit(&rsa_ctx);
	mbed_hash_uninit(ctx);
	return -1;
   }
   MBEDPACK_DEBUG_PRINT("func--mbed_verification : trace --- 4\n\r");
   mbed_rsa_ca_pkcs1_uninit(&rsa_ctx);
   mbed_hash_uninit(ctx);
   MBEDPACK_DEBUG_PRINT("func--mbed_verification : end\n\r");
   return ret;
}


void* mbed_decompress_init()
{
   return mbed_zlibstream_decompress_init();
}

int mbed_decompress_process(void * verify_cxt, void* ctx, unsigned char* buffer, uint32_t bufferSZ, struct IOtaUpdater *updater, void *update_cxt)
{
   return mbed_zlibstream_decompress_process(verify_cxt, ctx, buffer, bufferSZ, updater, update_cxt);
}


void mbed_decompress_uninit(void* ctx)
{
   mbed_zlibstream_decompress_uninit(ctx);
}

void* mbed_data_collector_init(unsigned char* filename)
{
   return mbed_build_pck_config_block(filename);
}


void mbed_data_collector_uninit(void* ctx)
{
   mbed_destroy_pck_config_block(ctx);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t pkg_get_module_count(void* ctx, module_type_t type)
{
   meta_info_t* meta_info = (meta_info_t*)ctx;

   if (!meta_info->meta_object) {

	return 0;
   }
   return mbed_get_module_num(meta_info->meta_object, type);
}

void* pkg_acquire_module(void* ctx, module_type_t type, int index)
{
   int ret = 0;
   uint8_t tmp_index = 0;
   uint8_t local_index = 0;
   uint8_t module_count = 0;
   meta_info_t* meta_info = (meta_info_t*)ctx;
   module_info_t* module = NULL;

   MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : begin!!!\n\r");

   if (!meta_info) {
	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : meta info is null!!!\n\r");
	return NULL;
   }

   if (!meta_info->meta_object) {
	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : meta object is null!!!\n\r");
	return NULL;
   }

   module = (module_info_t *) DUER_MALLOC(sizeof(module_info_t));
   if (!module) {
	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : module is null!!!\n\r");
	return NULL;
   }
   memset(module, 0, sizeof(module_info_t));

   if (type == ModuleTypeALL) {
	ret = mbed_prebuild_module(meta_info->meta_object, module, tmp_index);
	if (ret == -1) {
	   MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : prebuild module return -1!!!\n\r");
	   DUER_FREE(module);
	   return NULL;
	}

	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : good!!!\n\r");
	mbed_get_module_info(meta_info->meta_object, meta_info->module_data_offset, module, index);
	return module;
   }

   module_count = mbed_get_module_num(meta_info->meta_object, ModuleTypeALL);
   if (module_count < index) {
	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : invalid index!!!\n\r");
	DUER_FREE(module);
	return NULL;
   }

   for (tmp_index = 0; tmp_index < module_count; tmp_index++) {
	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module tmp_index is %u\n\r", tmp_index);
	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module local_index is %u\n\r", local_index);
	ret = mbed_prebuild_module(meta_info->meta_object, module, tmp_index);
	if (ret == -1) {
	   MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : prebuild module return -1 in loop!!!\n\r");
	   DUER_FREE(module);
	   return NULL;
	}


	mbed_get_module_info(meta_info->meta_object, meta_info->module_data_offset, module, tmp_index);
	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module module->type is %u\n\r", module->type);
	MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module type is %u\n\r", type);
	if (module->type == type) {
	   MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module local index is %u\n\r", local_index);
	   MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module index is %u\n\r", index);
	   if (local_index == index) {
		MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : good!!!\n\r");
		return module;
	   } else {
		local_index++;
	   }
	}
	mbed_postbuild_module(module);
   }

   MBEDPACK_DEBUG_PRINT("func-pkg_acquire_module : bad!!!\n\r");
   DUER_FREE(module);
   return NULL;
}

void pkg_release_module(void* handle)
{
   MBEDPACK_DEBUG_PRINT("func-pkg_release_module : begin!!!\n\r");
   module_info_t *module = (module_info_t *) handle;
   if (module) {
	mbed_postbuild_module(module);
	DUER_FREE(module);
   }
   MBEDPACK_DEBUG_PRINT("func-pkg_release_module : end!!!\n\r");
}

const char* pkg_get_module_name(void* handle)
{
   module_info_t *module = (module_info_t *) handle;
   if (module) {
	return (const char *)module->name;
   }
   return NULL;
}

#if 0
typedef enum {
	false = 0,
	true
} bool;
#endif

const char* pkg_get_module_version(void* handle, bool hardware_version)
{
   module_info_t *module = (module_info_t *) handle;
   if (module) {
	if (hardware_version) {
	   return (const char *)module->hw_version;
	} else {
	   return (const char *)module->version;
	}
   }
   return NULL;
}

int pkg_locate_module_data(void* handle, uint32_t* offset, uint32_t* size)
{
   module_info_t *module = (module_info_t *) handle;
   if (module) {
	if (size) *size = module->module_size;
	if (offset) *offset = module->offset;
	return 0;
   }
   if (size) *size = 0;
   if (offset) *offset = 0;
   return -1;
}

int pkg_get_pkg_name(void* ctx, unsigned char* name, uint8_t* name_length)
{
   if (ctx == NULL) {
	return -1;
   }

   meta_info_t* meta_info = (meta_info_t*)ctx;
   if (meta_info == NULL) {
	return -1;
   }

   return mbed_get_pkg_name(meta_info->meta_object, name, name_length);
}

int pkg_get_pkg_type(void* ctx, package_type_t* type)
{
   if (ctx == NULL) {
	return -1;
   }

   meta_info_t* meta_info = (meta_info_t*)ctx;
   if (meta_info == NULL) {
	return -1;
   }

   return mbed_get_pkg_type(meta_info->meta_object, type);
}
