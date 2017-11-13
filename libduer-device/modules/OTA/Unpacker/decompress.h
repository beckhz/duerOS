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

#ifndef _MBED_PACK_DECRYPTION_
#define _MBED_PACK_DECRYPTION_

#include <stdint.h>
#include "pack_include.h"
#include "Zliblite/zlib.h"
#include "IOtaUpdater.h"

typedef struct _decompress_context_ {
	// internal_footprint
	uint32_t stream_recieved_sz;	// current size of recieved stream
	uint32_t stream_processed_sz;	// current size of processed stream

	//cache meta data
	unsigned int meta_size;
	unsigned int meta_stored_size;
	char *meta_data;

	//module data write offset
	unsigned int write_offset;
	// zlib stream object
	z_streamp strmp;

} decompress_context_t;

/**
 *
 * package decompress
 *
 * \return decompress context
 */
void* mbed_zlibstream_decompress_init();


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
int mbed_zlibstream_decompress_process(void *verify_cxt, decompress_context_t* ctx, unsigned char* buffer, uint32_t bufferSZ,
		struct IOtaUpdater *updater, void *update_cxt);


/**
 *
 * package decompress uninit
 *
 * \param ctx decompress context
 *
 */
void mbed_zlibstream_decompress_uninit(decompress_context_t* ctx);




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





// for test
//void mbed_decompress_test();


#endif
