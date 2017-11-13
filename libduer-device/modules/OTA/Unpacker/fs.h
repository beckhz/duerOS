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

#ifndef _MBED_FS_H_
#define _MBED_FS_H_

/*  sl_FsOpen options */
/*  Open for Read */
#define FS_MODE_OPEN_READ                                     0
/*  Open for Write (in case file exist) */
#define FS_MODE_OPEN_WRITE                                    1


/*****************************************************************************/
/* Function prototypes                                                       */
/*****************************************************************************/

/*!
    \brief open file for read or write from/to storage device

    \param[in]      pFileName                  File Name buffer pointer
    \param[in]      AccessModeAndMaxSize       Options: As described below
    \param[in]      pToken                     Reserved for future use. Use NULL for this field
    \param[out]     pFileHandle      Pointing on the file and used for read and write commands to the file

     AccessModeAndMaxSize possible input                                                                        \n
     FS_MODE_OPEN_READ                                        - Read a file                                                                  \n
     FS_MODE_OPEN_WRITE                                       - Open for write for an existing file                                          \n
     FS_MODE_OPEN_CREATE(maxSizeInBytes,accessModeFlags)      - Open for creating a new file. Max file size is defined in bytes.             \n
                                                                For optimal FS size, use max size in 4K-512 bytes steps (e.g. 3584,7680,117760)  \n
                                                                Several access modes bits can be combined together from SlFileOpenFlags_e enum

    \return         On success, zero is returned. On error, an error code is returned

    \sa             sl_FsRead sl_FsWrite sl_FsClose
    \note           belongs to \ref basic_api
    \warning
    \par            Example:
    \code
       char*           DeviceFileName = "MyFile.txt";
       unsigned long   MaxSize = 63 * 1024; //62.5K is max file size
       long            DeviceFileHandle = -1;
       long            RetVal;        //negative retval is an error
       unsigned long   Offset = 0;
       unsigned char   InputBuffer[100];

       // Create a file and write data. The file in this example is secured, without signature and with a fail safe commit
       RetVal = sl_FsOpen((unsigned char *)DeviceFileName,
                                        FS_MODE_OPEN_CREATE(MaxSize , _FS_FILE_OPEN_FLAG_NO_SIGNATURE_TEST | _FS_FILE_OPEN_FLAG_COMMIT ),
                                        NULL, &DeviceFileHandle);

       Offset = 0;
       //Preferred in secure file that the Offset and the length will be aligned to 16 bytes.
       RetVal = sl_FsWrite( DeviceFileHandle, Offset, (unsigned char *)"HelloWorld", strlen("HelloWorld"));

       RetVal = sl_FsClose(DeviceFileHandle, NULL, NULL , 0);

       // open the same file for read, using the Token we got from the creation procedure above
       RetVal = sl_FsOpen((unsigned char *)DeviceFileName,
                                        FS_MODE_OPEN_READ,
                                        NULL, &DeviceFileHandle);

       Offset = 0;
       RetVal = sl_FsRead( DeviceFileHandle, Offset, (unsigned char *)InputBuffer, strlen("HelloWorld"));

       RetVal = sl_FsClose(DeviceFileHandle, NULL, NULL , 0);

     \endcode
*/
int32_t sl_FsOpen(const uint8_t *pFileName,const uint32_t AccessModeAndMaxSize,uint32_t *pToken,int32_t *pFileHandle);

/*!
    \brief close file in storage device

    \param[in]      FileHdl                 Pointer to the file (assigned from sl_FsOpen)
    \param[in]      pCeritificateFileName   Reserved for future use. Use NULL.
    \param[in]      pSignature              Reserved for future use. Use NULL.
    \param[in]      SignatureLen            Reserved for future use. Use 0.


    \return         On success, zero is returned.  On error, an error code is returned

    \sa             sl_FsRead sl_FsWrite sl_FsOpen
    \note           Call the fs_Close  with signature = 'A' signature len = 1 for activating an abort action
    \warning
    \par            Example:
    \code
    sl_FsClose(FileHandle,0,0,0);
    \endcode
*/
int16_t sl_FsClose(const int32_t FileHdl,const uint8_t* pCeritificateFileName,const uint8_t* pSignature,const uint32_t SignatureLen);

/*!
    \brief Read block of data from a file in storage device

    \param[in]      FileHdl Pointer to the file (assigned from sl_FsOpen)
    \param[in]      Offset  Offset to specific read block
    \param[out]     pData   Pointer for the received data
    \param[in]      Len     Length of the received data

    \return         On success, returns the number of read bytes. On error, negative number is returned

    \sa             sl_FsClose sl_FsWrite sl_FsOpen
    \note           belongs to \ref basic_api
    \warning
    \par            Example:
    \code
    Status = sl_FsRead(FileHandle, 0, &readBuff[0], readSize);
    \endcode
*/
int32_t sl_FsRead(const int32_t FileHdl, uint32_t Offset , uint8_t*  pData, uint32_t Len);

/*!
    \brief write block of data to a file in storage device

    \param[in]      FileHdl  Pointer to the file (assigned from sl_FsOpen)
    \param[in]      Offset   Offset to specific block to be written
    \param[in]      pData    Pointer the transmitted data to the storage device
    \param[in]      Len      Length of the transmitted data

    \return         On success, returns the number of written bytes.  On error, an error code is returned

    \sa
    \note           belongs to \ref basic_api
    \warning
    \par            Example:
    \code
    Status = sl_FsWrite(FileHandle, 0, &buff[0], readSize);
    \endcode
*/
int32_t sl_FsWrite(const int32_t FileHdl, uint32_t Offset, uint8_t*  pData, uint32_t Len);

/*!
    \brief get info on a file

    \param[in]      pFileName    File name
    \param[in]      Token        Reserved for future use. Use 0
    \param[out]     pFsFileInfo Returns the File's Information: flags,file size, allocated size and Tokens

    \return         On success, zero is returned.   On error, an error code is returned

    \sa             sl_FsOpen
    \note           belongs to \ref basic_api
    \warning
    \par            Example:
    \code
    Status = sl_FsGetInfo("FileName.html",0,&FsFileInfo);
    \endcode
*/



#endif /*  __FS_H__ */
