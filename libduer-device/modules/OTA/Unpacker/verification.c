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
#include <inttypes.h>
#include <string.h>
#include "verification.h"
#include "pack_include.h"
#include "mbedtls/sha1.h"
#include "lightduer_memory.h"

#define KEY_LEN	128
#define SHA1_LEN 20

/*
#define RSA_N   "9292758453063D803DD603D5E777D788" \
                "8ED1D5BF35786190FA2F23EBC0848AEA" \
                "DDA92CA6C3D80B32C4D109BE0F36D6AE" \
                "7130B9CED7ACDF54CFC7555AC14EEBAB" \
                "93A89813FBF3C4F8066D2D800F7C38A8" \
                "1AE31942917403FF4946B0A83D3D3E05" \
                "EE57C6F5F5606FB5D4BC6CD34EE0801A" \
                "5E94BB77B07507233A0BC7BAC8F90F79"

#define RSA_E   "10001"
*/
#define RSA_N   "b7699a83da6100367cd9f43ac124" \
		"dd0f4fce5a4d39c02a2f844fb7867f" \
		"869cb535373d0022039bf5cb58e869" \
		"b041998baaf10d08705d043227eb4b" \
		"20204e63c0d409f3225c6eb013abc6" \
		"fef7d89435258eaa872c52020c0f07" \
		"fd3ce59fb6a042f6de4b2c26e50732" \
		"c8691cb744df77856b8b2a59066cca" \
		"ccd4e289d0440fe34b"

#define RSA_E   "10001"


#define MBEDPACK_MPI_CHK(ret) do { if( ret != 0 ) goto cleanup; } while( 0 )

#define offset_of(T, M) ((size_t)(&(((T*)0)->M)))

int mbed_rsa_ca_pkcs1_init(mbedtls_rsa_context* rsa_ctx)
{
   if (!rsa_ctx) {
	MBEDPACK_DEBUG_PRINT("invalid rsa context!!!\n\r");
	return -1;
   }

   mbedtls_rsa_init( rsa_ctx, MBEDTLS_RSA_PKCS_V15, 0 );

   rsa_ctx->len = KEY_LEN;
   //printf("mbed_rsa_ca_pkcs1_init ckp 1\n");
   MBEDPACK_MPI_CHK( mbedtls_mpi_read_string( &rsa_ctx->N , 16, RSA_N  ) );
   //for(int j = rsa_ctx->N.n; j > 0; j-- )
   //	{
	//	printf("Y->p[%d]=%d ", j - 1, rsa_ctx->N.p[j - 1]);
   //	}
   //printf("mbed_rsa_ca_pkcs1_init ckp 2\n");
   MBEDPACK_MPI_CHK( mbedtls_mpi_read_string( &rsa_ctx->E , 16, RSA_E  ) );
   //for(int j = rsa_ctx->N.n; j > 0; j-- )
   //	{
	//	printf("Y->p[%d]=%d ", j - 1, rsa_ctx->N.p[j - 1]);
   //	}
   //printf("mbed_rsa_ca_pkcs1_init ckp 3\n");
   if( mbedtls_rsa_check_pubkey(rsa_ctx) != 0) {

	MBEDPACK_DEBUG_PRINT( "key-pair check failed!!!\n\r" );
	mbedtls_rsa_free(rsa_ctx);

	return -1;
   }
   //for(int j = rsa_ctx->N.n; j > 0; j-- )
   //	{
	//	printf("Y->p[%d]=%d ", j - 1, rsa_ctx->N.p[j - 1]);
   //	}
   printf("mbed_rsa_ca_pkcs1_init ckp ok\n");
   return 0;

cleanup:
   MBEDPACK_DEBUG_PRINT("mpi check failed!!!\n\r");
   mbedtls_rsa_free(rsa_ctx);
   return -1;
}

void mbed_rsa_ca_pkcs1_uninit(mbedtls_rsa_context* rsa_ctx)
{
   mbedtls_rsa_free(rsa_ctx);
}

/**
 * hash(sha1) init
 *
 * \return hash context if success, or NULL
 */
verification_context_t* mbed_hash_init()
{
   verification_context_t* ctx = NULL;
   uint32_t sig_part_sz = sizeof(package_header_t);//offset_of(package_header_t, meta_sig_size);

   ctx = (verification_context_t*)DUER_MALLOC(sizeof(verification_context_t));
   if (!ctx) {
	return NULL;
   }

   // gloabl verification footprint
   ctx->stream_recieved_sz = 0;
   ctx->stream_processed_sz = 0;
   ctx->stream_sig_stored_sz = 0;
   ctx->pck_header_sig_part = (unsigned char*)DUER_MALLOC(sig_part_sz);
   ctx->ctx = (mbedtls_sha1_context*)DUER_MALLOC(sizeof(mbedtls_sha1_context));
   memset(ctx->pck_header_sig_part, 0, sig_part_sz);

   if (!ctx->ctx) {
	DUER_FREE(ctx);
	return NULL;
   }

   mbedtls_sha1_starts(ctx->ctx);
   return ctx;
}

/**
 * hash(sha1) update key
 *
 * \param ctx hash context
 * \param buffer data source to gen hash
 * \param buffer_size
 */
void mbed_hash_update_key(verification_context_t* ctx, unsigned char* buffer, uint32_t buffer_size)
{
   uint16_t pck_header_sz = sizeof(package_header_t);
   unsigned char* sig_buffer = NULL;

   if (!ctx) {
	return;
   }

   ctx->stream_recieved_sz += buffer_size;

   if (ctx->stream_sig_stored_sz < pck_header_sz){
	   if (ctx->stream_recieved_sz <= pck_header_sz){
	       sig_buffer = ctx->pck_header_sig_part + ctx->stream_sig_stored_sz;
	       memcpy(sig_buffer, buffer, buffer_size);
	       ctx->stream_sig_stored_sz += buffer_size;
	       return;
	   } else {
		   sig_buffer = ctx->pck_header_sig_part + ctx->stream_sig_stored_sz;
		   memcpy(sig_buffer, buffer, pck_header_sz - ctx->stream_sig_stored_sz);
		   buffer = buffer + (pck_header_sz - ctx->stream_sig_stored_sz);
		   buffer_size = buffer_size - (pck_header_sz - ctx->stream_sig_stored_sz);
		   ctx->stream_sig_stored_sz = pck_header_sz;
	   }
   }

/*
   if (ctx->stream_recieved_sz <= pck_header_sz) {
	if (ctx->stream_recieved_sz <= offset_of(package_header_t, meta_sig_size)) {
	   sig_buffer = ctx->pck_header_sig_part + ctx->stream_sig_stored_sz;
	   memcpy(sig_buffer, buffer, buffer_size);
	   ctx->stream_sig_stored_sz += buffer_size;
	} else if (offset_of(package_header_t, meta_sig_size) > ctx->stream_sig_stored_sz) {
	   sig_buffer = ctx->pck_header_sig_part + ctx->stream_sig_stored_sz;
	   memcpy(sig_buffer, buffer, (offset_of(package_header_t, meta_sig_size) - ctx->stream_sig_stored_sz));
	   ctx->stream_sig_stored_sz = offset_of(package_header_t, meta_sig_size);
	}
	return;
   } else {
	if (ctx->stream_processed_sz == 0) {
	   buffer = buffer + (buffer_size - (ctx->stream_recieved_sz - pck_header_sz));
	   buffer_size = ctx->stream_recieved_sz - pck_header_sz;
	}
   }
*/
   mbedtls_sha1_update(ctx->ctx, buffer, buffer_size);
   ctx->stream_processed_sz += buffer_size;
}

/**
 * hash(sha1) verify key
 *
 * \param ctx hash context
 * \param buffer data source to gen hash
 * \param buffer_size
 *
 * \return 0 if success, or failed
 */
int mbed_rsa_ca_pkcs1_verify(mbedtls_rsa_context* rsa, verification_context_t* ctx)
{
   unsigned char sha1sum[SHA1_LEN];
   unsigned char rsa_ciphertext[KEY_LEN];
   int index = 0;
   int ret = 0;

   MBEDPACK_DEBUG_PRINT("func--mbed_rsa_ca_pkcs1_verify : begin\n\r");

   memcpy(rsa_ciphertext, ctx->pck_header_sig_part + offset_of(package_header_t, package_sig), KEY_LEN);

   for (index = 0; index < KEY_LEN; index++) {
	MBEDPACK_DEBUG_PRINT("%.2x ", rsa_ciphertext[index]);
   }

   mbedtls_sha1_finish(ctx->ctx, sha1sum);
   MBEDPACK_DEBUG_PRINT("\nfunc--mbed_rsa_ca_pkcs1_verify -- sha1 digest : \n");
   for (index = 0; index < SHA1_LEN; index++) {
	MBEDPACK_DEBUG_PRINT("%.2x ", sha1sum[index]);
   }

   //for(int j = rsa->N.n; j > 0; j-- )
   //	{
	//	printf("\n\nY->p[%d]=%d ", j - 1, rsa->N.p[j - 1]);
   //	}
   if( (ret = mbedtls_rsa_pkcs1_verify( rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC, MBEDTLS_MD_SHA1, 0, sha1sum, rsa_ciphertext )) != 0 ) {
	MBEDPACK_DEBUG_PRINT("func--mbed_rsa_ca_pkcs1_verify : verify failed, error code -- %d\n\r", ret);
	return -1;
   }
   MBEDPACK_DEBUG_PRINT("func--mbed_rsa_ca_pkcs1_verify : %s\n\r", rsa_ciphertext);
   MBEDPACK_DEBUG_PRINT("func--mbed_rsa_ca_pkcs1_verify : end\n\r");
   return 0;
}


/**
 * hash(sha1) uninit
 *
 * \return hash context if success, or NULL
 */
void mbed_hash_uninit(verification_context_t* ctx)
{
   if (!ctx) {
	return;
   }

   if (ctx->pck_header_sig_part) {
	DUER_FREE(ctx->pck_header_sig_part);
   }

   if (ctx->ctx) {
	DUER_FREE(ctx->ctx);
   }

   DUER_FREE(ctx);
   return;
}
