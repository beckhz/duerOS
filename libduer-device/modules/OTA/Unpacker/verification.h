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

#ifndef _MBEDPACK_VERIFICATION_H
#define _MBEDPACK_VERIFICATION_H

#include "mbedtls/rsa.h"
#include "mbedtls/sha1.h"
#include "pack_include.h"

typedef struct _verification_context_ {
	// internal footprint
	uint32_t stream_recieved_sz;	// current size of recieved stream
	uint32_t stream_processed_sz;	// current size of processed stream
	uint32_t stream_sig_stored_sz;	// size of pck_header_sig_part
	unsigned char* pck_header_sig_part;		// package header buffer to pck signature(include), for verify use

	// sha1 context
	mbedtls_sha1_context* ctx;

} verification_context_t;

/**
 * init rsa context
 *
 * \param rsa_ctx point to an instance of mbedtls_rsa_context struct
 * \return 0 if success, or failed
 */
int mbed_rsa_ca_pkcs1_init(mbedtls_rsa_context* rsa_ctx);

/**
 * uinit rsa context
 *
 * \param rsa_ctx point to an instance of mbedtls_rsa_context struct
 */
void mbed_rsa_ca_pkcs1_uninit(mbedtls_rsa_context* rsa_ctx);

///////////////////////////////////////////////////////////////////////////////////////

/**
 * verification init
 *
 * \return verification context if success, or NULL
 */
verification_context_t* mbed_hash_init();

/**
 * verification update key
 *
 * \param ctx verification context
 * \param buffer data source to gen hash
 * \param buffer_size
 */
void mbed_hash_update_key(verification_context_t* ctx, unsigned char* buffer, uint32_t buffer_size);

/**
 * verify key
 *
 * \param ctx verification context
 * \param buffer data source to gen hash
 * \param buffer_size
 *
 * \return 0 if success, or failed
 */
int mbed_rsa_ca_pkcs1_verify(mbedtls_rsa_context* rsa, verification_context_t* ctx);

/**
 * verification uninit
 *
 * \param ctx verification context
 */
void mbed_hash_uninit(verification_context_t* ctx);

#endif
