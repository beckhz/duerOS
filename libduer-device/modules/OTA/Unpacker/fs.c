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

#include <inttypes.h>
#include "fs.h"


int32_t sl_FsOpen(const uint8_t *pFileName,const uint32_t AccessModeAndMaxSize,uint32_t *pToken,int32_t *pFileHandle){return 0;}

int16_t sl_FsClose(const int32_t FileHdl,const uint8_t* pCeritificateFileName,const uint8_t* pSignature,const uint32_t SignatureLen){}

int32_t sl_FsRead(const int32_t FileHdl, uint32_t Offset , uint8_t*  pData, uint32_t Len){ return 0; }

int32_t sl_FsWrite(const int32_t FileHdl, uint32_t Offset, uint8_t*  pData, uint32_t Len){ return Len; }
