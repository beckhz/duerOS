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
#include <string.h>
#include <inttypes.h>

#include "fs.h"
#include "decompress.h"
#include "verification.h"
#include "cJSON.h"
#include "IOtaUpdater.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"

#define CHUNK 128

static void* my_alloc (opaque, items, size)
    void* opaque;
    unsigned items;
    unsigned size;
{
    if (opaque) items += size - size; /* make compiler happy */
    return sizeof(uInt) > 2 ? (voidpf)DUER_MALLOC(items * size) :
                              (voidpf)DUER_CALLOC(items, size);
}

static void  my_free (opaque, ptr)
    void* opaque;
    void* ptr;
{
    DUER_FREE(ptr);
    if (opaque) return; /* make compiler happy */
}

void* mbed_zlibstream_decompress_init()
{
   int ret = 0;
   decompress_context_t* ctx = NULL;

   ctx = (decompress_context_t*)DUER_MALLOC(sizeof(decompress_context_t));
   if (!ctx) {
	   DUER_LOGI("mbed_zlibstream_decompress_init ckp1\n");
	   return NULL;
   }

   // global footprint init
   ctx->stream_recieved_sz = 0;
   ctx->stream_processed_sz = 0;

   ctx->meta_data = 0;
   ctx->meta_size = ctx->meta_stored_size = 0;

   ctx->write_offset = 0;

   ctx->strmp = (z_streamp)DUER_MALLOC(sizeof(z_stream));
   if (!ctx->strmp) {
	   DUER_LOGI("mbed_zlibstream_decompress_init ckp2\n");
	DUER_FREE(ctx);
	return NULL;
   }

   /* allocate inflate state */
   ctx->strmp->zalloc = my_alloc;
   ctx->strmp->zfree = my_free;
   ctx->strmp->opaque = Z_NULL;
   ctx->strmp->avail_in = 0;
   ctx->strmp->next_in = Z_NULL;
   ret = inflateInit(ctx->strmp);
   if (ret != Z_OK) {
	   DUER_LOGI("mbed_zlibstream_decompress_init ckp3\n");
	   return NULL;
   }
   return ctx;
}

int get_json_value(cJSON *obj, const char *key, cJSON **child_obj)
{
	int find = 1;
	cJSON *child = 0;
	if (obj && (child = obj->child))
	{
		while (child != 0)
		{
			if (strcmp(child->string, key) == 0)
			{
				if (cJSON_Object == child->type || cJSON_Array == child->type)
				{
					find = 0;
					*child_obj = cJSON_Duplicate(child, 1);
				}
				break;
			}
			child = child->next;
		}
	}
	return find;
}

int get_plain_value(cJSON *obj, const char *key, char **value, unsigned int *length)
{
	int find = 1;
	cJSON *child = 0;
	if (obj && (child = obj->child))
	{
		while (child != 0)
		{
			DUER_LOGI("child->string:%s\n", child->string);
			if (strcmp(child->string, key) == 0)
			{
				int type = child->type;
				find = 0;
				if (cJSON_Number == type)
				{
					char buffer[20] = {0};
					if (child->valueint == child->valuedouble)
					{
						sprintf(buffer, "%d", child->valueint);
					}
					else
					{
						sprintf(buffer, "%f", child->valuedouble);
					}

					*length = strlen(buffer);
					*value = (char*)DUER_MALLOC(*length + 1);
					strcpy(*value, buffer);
				}
				else if (cJSON_String == type)
				{
					*length = strlen(child->valuestring);
					*value = (char*)DUER_MALLOC(*length + 1);
					strcpy(*value, child->valuestring);
				}
				else
				{
					find = 1;
				}
				break;
			}
			child = child->next;
		}
	}
	return find;
}

int parse_meta_data(char *meta_data, struct package_meta_data *meta)
{
	cJSON *meta_obj = cJSON_Parse(meta_data);
	if (meta_obj){
		cJSON *meta_basic_info_obj = 0;
		cJSON *meta_install_info_obj = 0;
		char *value = 0;
		unsigned int length = 0;
		if (0 == get_json_value(meta_obj, "basic_info", &meta_basic_info_obj)){
			DUER_LOGI("get basic info\n");
            if (0 == get_plain_value(meta_basic_info_obj, "app_name", &value, &length)){
		DUER_LOGI("app_name:%s\n", value);
		if (length <= PACKAGE_NAME_LENGTH_MAX){
		    memcpy(meta->basic_info.package_name, value, length);
		    meta->basic_info.package_name[length] = 0;
		}
		DUER_FREE(value);
		value = 0;
		length = 0;
            }
			cJSON_Delete(meta_basic_info_obj);
			meta_basic_info_obj = 0;
		}

		if (0 == get_json_value(meta_obj, "inst_and_uninst_info", &meta_install_info_obj)){
			cJSON *module_list_obj = 0;
			if (0 == get_json_value(meta_install_info_obj, "module_list", &module_list_obj)){
				unsigned int module_count = cJSON_GetArraySize(module_list_obj);
				if (module_count > 0){
					int i = 0;
					meta->install_info.module_count = module_count;
					meta->install_info.module_list = (struct module_info*)DUER_MALLOC(module_count*sizeof(struct module_info));
					for (i = 0; i < module_count; i++){
						cJSON *item = cJSON_GetArrayItem(module_list_obj, i);
						if (item){
							//get module name
							if (0 == get_plain_value(item, "name", &value, &length)){
								if (length <= MODULE_NAME_LENGTH_MAX){
									memcpy(meta->install_info.module_list[i].module_name, value, length);
									meta->install_info.module_list[i].module_name[length] = 0;
								}
								DUER_FREE(value);
								value = 0;
								length = 0;
							}

							//get module size
							if (0 == get_plain_value(item, "size", &value, &length)){
								meta->install_info.module_list[i].module_size = atoi(value);
								DUER_FREE(value);
								value = 0;
								length = 0;
							}

							//get module version
							if (0 == get_plain_value(item, "version", &value, &length)){
								if (length <= MODULE_VERSION_LENGTH_MAX){
									memcpy(meta->install_info.module_list[i].module_version, value, length);
									meta->install_info.module_list[i].module_version[length] = 0;
									DUER_FREE(value);
									value = 0;
									length = 0;
								}
							}
						}
					}
				}

				cJSON_Delete(module_list_obj);
				module_list_obj = 0;
			}

			cJSON_Delete(meta_install_info_obj);
			meta_install_info_obj = 0;
		}
		cJSON_Delete(meta_obj);
		meta_obj = 0;
	}
	return 0;
}

int mbed_zlibstream_decompress_process(void *verify_cxt, decompress_context_t* ctx, unsigned char* buffer, uint32_t bufferSZ,
		struct IOtaUpdater *updater, void *update_cxt)
{
   int debug = 0;
   int ret = 0;
   unsigned have;
   unsigned char out[CHUNK];
   uint16_t pck_header_sz = sizeof(package_header_t);


   if (ctx == NULL || buffer == NULL || bufferSZ == 0 /*|| !lFileHandle*/) {
	MBEDPACK_DEBUG_PRINT("func -- mbed_zlibstream_decompress_process : invalid parameters");
	return -7;
   }

   ctx->stream_recieved_sz += bufferSZ;
   if (ctx->stream_recieved_sz <= pck_header_sz) {
	return 0;
   } else {
	if (ctx->stream_processed_sz == 0) {
	   buffer = buffer + (bufferSZ - (ctx->stream_recieved_sz - pck_header_sz));
	   bufferSZ = ctx->stream_recieved_sz - pck_header_sz;
	}
   }

   //parse verification context
   if (0 == ctx->meta_size){
       verification_context_t *verify = (verification_context_t*)verify_cxt;
       package_header_t *pk_header = (package_header_t*)verify->pck_header_sig_part;
       DUER_LOGI("meta size:%d\n", pk_header->meta_size);
       if (pk_header->meta_size > 0){
           ctx->meta_data = (char*)DUER_MALLOC(pk_header->meta_size);
           if (ctx->meta_data){
		   ctx->meta_size = pk_header->meta_size;
		   ctx->meta_stored_size = 0;
           }
       }
   }
   ctx->strmp->avail_in = bufferSZ;
   ctx->strmp->next_in = buffer;

   /* run inflate() on input until output buffer not full */
   do {
	ctx->strmp->avail_out = CHUNK;
	ctx->strmp->next_out = out;
	ret = inflate(ctx->strmp, Z_NO_FLUSH);

	//assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
	switch (ret) {
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     /* and fall through */
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			MBEDPACK_DEBUG_PRINT("func -- mbed_zlibstream_decompress_process : DATA ERROR");
			(void)inflateEnd(ctx->strmp);
			ctx->stream_processed_sz += bufferSZ;
			return ret;
	}
	have = CHUNK - ctx->strmp->avail_out;
	debug = 0;
	if (ctx->meta_stored_size < ctx->meta_size){
		unsigned int copy_byte = (have > (ctx->meta_size - ctx->meta_stored_size))?(ctx->meta_size - ctx->meta_stored_size):have;
		memcpy(ctx->meta_data + ctx->meta_stored_size, out, copy_byte);
		ctx->meta_stored_size += copy_byte;
		debug = copy_byte;
		if (ctx->meta_stored_size == ctx->meta_size){
			DUER_LOGI("meta data has been completely copied, notify meta info\n");
			struct package_meta_data meta;
			memset(&meta, 0, sizeof (meta));
			if ( 0 == parse_meta_data(ctx->meta_data, &meta)){
				DUER_LOGI("parse_meta_data succeed\n");
				DUER_LOGI("meta.basic_info.package_name:%s\n", meta.basic_info.package_name);
				DUER_LOGI("module_count=%d\n", meta.install_info.module_count);
				DUER_LOGI("module_list[0]:\nname:%s,\nsize:%d,\nversion:%s\n", meta.install_info.module_list[0].module_name,
						meta.install_info.module_list[0].module_size, meta.install_info.module_list[0].module_version);
				if (updater && updater->notify_meta_data && update_cxt){
				    ret = updater->notify_meta_data(update_cxt, &meta);
					if (ret != 0)
					{
					    return ret;
					}
				}
			}

		}
	}

	if (debug < have){
		if (updater && updater->notify_module_data && update_cxt){
			ret = updater->notify_module_data(update_cxt, ctx->write_offset, out + debug, have - debug);
			ctx->write_offset += (have - debug);
			if (ret != 0)
			{
			    return ret;
			}
		}
	}
   } while (ctx->strmp->avail_out == 0);

   // DUER_LOGI("decompress_ctx->strmp->total_out=%d\n", ctx->strmp->total_out);
   ctx->stream_processed_sz += bufferSZ;

   return 0;
}


/**
 *
 * package decompress uninit
 *
 * \param ctx decompress context
 *
 */
void mbed_zlibstream_decompress_uninit(decompress_context_t* ctx)
{
   if (!ctx) {
	return;
   }

   if (ctx->strmp) {
	/* clean up and return */
	(void)inflateEnd(ctx->strmp);
	DUER_FREE(ctx->strmp);
   }

   if (ctx->meta_data){
	   DUER_FREE(ctx->meta_data);
	   ctx->meta_data = 0;
	   ctx->meta_size = ctx->meta_stored_size = 0;
   }
   DUER_FREE(ctx);
}


/*
void mbed_decompress_test()
{
   uint8_t module_count = 0;
   uint8_t index = 0;
   int ret = 0;
   module_info_t module = {0};

   MBEDPACK_DEBUG_PRINT("func-mbed_decompress_test : begin!!!\n\r");

   if (!decompress_info->decompress_meta_data || !decompress_info->decompress_module_data) {
	MBEDPACK_DEBUG_PRINT("func-mbed_decompress_test : invalid module info!!!\n\r");
   }

   module_count = mbed_get_module_num(decompress_info->decompress_meta_data, ModuleTypeALL);
   MBEDPACK_DEBUG_PRINT("func-mbed_decompress_test : \n\rmodule count : %u!!!\n\r", module_count);
   for (; index < module_count; index++) {
	ret = mbed_prebuild_module(decompress_info->decompress_meta_data, &module, index);
	if (ret == -1) {
	   continue;
	}
	mbed_get_module_info(decompress_info->decompress_meta_data, decompress_info->decompress_module_data, &module, index);
	MBEDPACK_DEBUG_PRINT("func-mbed_decompress_test : \n\rmbed_get_module_info return : %d!!!\n\r", ret);
	if (module.data) {
	   MBEDPACK_DEBUG_PRINT("func-mbed_decompress_test : module data filled success!!!\n\r");
	}
	mbed_postbuild_module(&module);
   }

   MBEDPACK_DEBUG_PRINT("func-mbed_decompress_test : end!!!\n\r");
}
*/
