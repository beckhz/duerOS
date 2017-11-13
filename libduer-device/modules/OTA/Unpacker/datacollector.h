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

#ifndef _MBED_PACK_DATAC_
#define _MBED_PACK_DATAC_

#include "pack_include.h"
#include "cJSON.h"


/**
 *
 * build meta.json data object
 *
 * \return ctx to collect package data
 *
 */
void* mbed_build_pck_config_block(unsigned char* filename);


/**
 *
 * destory meta.json data object
 *
 * \param ctx to collect package data
 *
 */
void mbed_destroy_pck_config_block(meta_info_t* ctx);


/**
 * pre-build module, init for extract module from package
 *
 * \param meta_object meta.json object decompress from package
 * \param module_info module info extracted from package by parse meta.json
 * \param index index of module record in record
 * \return 0 if success, or failed
 */
int mbed_prebuild_module(cJSON* meta_object, module_info_t* module_info, uint8_t index);

/**
 * post-build module, uninit for extract module from package
 *
 * \param module module info extracted from package by parse meta.json
 */
void mbed_postbuild_module(module_info_t* module);

/**
 * main operation of decompressing package
 *
 * \param package_bin original data of package
 * \param package_bin_size original package size
 * \return 0 if success, or failed
 */
//int mbed_decompress(decompress_info_t* decompress_info, unsigned char* package_bin, uint32_t package_bin_size);

/**
 * get module count by parse meta.json
 *
 * \param meta_object meta.json object
 * \param type the type caller want to get
 * \return if successful retrun module count, or return -1
 */
uint8_t mbed_get_module_num(cJSON* meta_object, module_type_t type);

/**
 * get module info from meta.json and modules array data
 *
 * \param meta_data meta.json data
 * \param module_begin_offset, modules begin offset from package
 * \param module_info used for store signle module info
 * \param index index of module in modules array
 * \return 0 if success, or failed
 */
int mbed_get_module_info(cJSON* meta_object, uint32_t module_begin_offset, module_info_t* module_info, uint8_t index);

/**
 * get module info from meta.json and modules array data
 *
 * \param meta_data meta.json data
 * \param name package name
 *\ param name_len length of name
 * \return 0 if success, or failed
 */
int mbed_get_pkg_name(cJSON* meta_object, unsigned char* name, uint8_t* name_len);

/**
 * get package type info from meta.json
 *
 * \param meta_data meta.json data
 * \param type package type
 * \return 0 if success, or failed
 */
int mbed_get_pkg_type(cJSON* meta_object, package_type_t* type);


#endif
