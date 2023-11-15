/***************************************************************************
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description:
 */
/**\file
 * \brief Misc tools
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-05: create the document
 ***************************************************************************/

#ifndef _AM_MISC_H
#define _AM_MISC_H

#include "am_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/


/****************************************************************************
 * Type definitions
 ***************************************************************************/

/****************************************************************************
 * API function prototypes
 ***************************************************************************/
/**\brief 读sysfs
 *  name 文件名
 *  buf  存放字符串的缓冲区
 *  len  缓冲区大小
 */
typedef void (*AM_Read_Sysfs_Cb)(const char *name, char *buf, int len);

/**\brief 写sysfs
 *  name 文件名
 *  cmd  向文件打印的字符串
 */
typedef void (*AM_Write_Sysfs_Cb)(const char *name, const char *cmd);


/**\brief 注册读写sysfs的回调函数
 * \param[in] fun 回调
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern void AM_RegisterRWSysfsFun(AM_Read_Sysfs_Cb RCb, AM_Write_Sysfs_Cb WCb);

/**\brief 卸载注册
 */
extern void AM_UnRegisterRWSysfsFun();

/**\brief 读prop
 *  name 文件名
 *  buf  存放字符串的缓冲区
 *  len  缓冲区大小
 */
typedef void (*AM_Read_Prop_Cb)(const char *name, char *buf, int len);

/**\brief 写prop
 *  name 文件名
 *  cmd  向文件打印的字符串
 */
typedef void (*AM_Write_Prop_Cb)(const char *name, const char *cmd);


/**\brief 注册读写sysfs的回调函数
 * \param[in] fun 回调
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern void AM_RegisterRWPropFun(AM_Read_Prop_Cb RCb, AM_Write_Prop_Cb WCb);

/**\brief 卸载注册
 */
extern void AM_UnRegisterRWPropFun();


/**\brief 向一个Prop set字符串
 * \param[in] name Prop名
 * \param[in] cmd 向Prop set的字符串
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_PropEcho(const char *name, const char *cmd);

/**\brief 读取一个Prop的字符串
 * \param[in] name Prop名
 * \param[out] buf 存放字符串的缓冲区
 * \param len 缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_PropRead(const char *name, char *buf, int len);

/**\brief 向一个文件打印字符串
 * \param[in] name 文件名
 * \param[in] cmd 向文件打印的字符串
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_FileEcho(const char *name, const char *cmd);

/**\brief 读取一个文件中的字符串
 * \param[in] name 文件名
 * \param[out] buf 存放字符串的缓冲区
 * \param len 缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_FileRead(const char *name, char *buf, int len);

/**\brief 创建本地socket服务
 * \param[in] name 服务名称
 * \param[out] fd 返回服务器socket
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_LocalServer(const char *name, int *fd);

/**\brief 通过本地socket连接到服务
 * \param[in] name 服务名称
 * \param[out] fd 返回socket
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_LocalConnect(const char *name, int *fd);

/**\brief 通过本地socket发送命令
 * \param fd socket
 * \param[in] cmd 命令字符串
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_LocalSendCmd(int fd, const char *cmd);

/**\brief 通过本地socket接收响应字符串
 * \param fd socket
 * \param[out] buf 缓冲区
 * \param len 缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_LocalGetResp(int fd, char *buf, int len);

/**\brief 跳过无效UTF8字符
 * \param[in] src 源字符缓冲区
 * \param src_len 源字符缓冲区大小
 * \param[out] dest 目标字符缓冲区
 * \param[in] dest_len 目标字符缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
extern AM_ErrorCode_t AM_Check_UTF8(const char *src, int src_len, char *dest, int *dest_len);

/**\brief Set the log level of debug print
 * \param[in] level
 */
extern void AM_DebugSetLogLevel(int level);

/**\brief Get the log level of debug print
 * \return
 *   - current log level
 */
extern int AM_DebugGetLogLevel();

/**\brief Make sure signal is declared only once in one process
 * \return
 *   None
 */
void AM_SigHandlerInit();

#ifdef __cplusplus
}
#endif

#endif


