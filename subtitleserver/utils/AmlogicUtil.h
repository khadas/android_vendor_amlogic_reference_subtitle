/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file
 *  一些常用宏和辅助函数
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-12: create the document
 ***************************************************************************/

#ifndef _AM_UTIL_H
#define _AM_UTIL_H

#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define pthread_create_name(t,a,s,p,n)     pthread_create(t,a,s,p)
#define AM_THREAD_ENTER()
#define AM_THREAD_LEAVE()
#define AM_THREAD_FUNC(do)                 do
#define AM_pthread_dump()


enum AM_MOD_ID
{
    AM_MOD_EVT,    /**< Event module*/
    AM_MOD_DMX,    /**< Demux module*/
    AM_MOD_DVR,    /**< DVR module*/
    AM_MOD_NET,    /**< Network manager module*/
    AM_MOD_OSD,    /**< OSD module*/
    AM_MOD_AV,     /**< AV decoder module*/
    AM_MOD_AOUT,   /**< Audio output device module*/
    AM_MOD_VOUT,   /**< Video output device module*/
    AM_MOD_SMC,    /**< Smartcard module*/
    AM_MOD_INP,    /**< Input device module*/
    AM_MOD_FEND,   /**< DVB frontend device module*/
    AM_MOD_DSC,    /**< Descrambler device module*/
    AM_MOD_CFG,    /**< Configure file manager module*/
    AM_MOD_SI,     /**< SI decoder module*/
    AM_MOD_SCAN,   /**< Channel scanner module*/
    AM_MOD_EPG,    /**< EPG scanner module*/
    AM_MOD_IMG,    /**< Image loader module*/
    AM_MOD_FONT,   /**< Font manager module*/
    AM_MOD_DB,     /**< Database module*/
    AM_MOD_GUI,    /**< GUI module*/
    AM_MOD_REC,    /**< Recorder module*/
    AM_MOD_TV,     /**< TV manager module*/
    AM_MOD_SUB,    /**< Subtitle module*/
    AM_MOD_SUB2,   /**< Subtitle(version 2) module*/
    AM_MOD_TT,     /**< Teletext module*/
    AM_MOD_TT2,    /**< Teletext(version 2) module*/
    AM_MOD_FEND_DISEQCCMD,/**< Diseqc command module*/
    AM_MOD_FENDCTRL, /**< DVB frontend high level control module*/
    AM_MOD_PES,    /**< PES parser module*/
    AM_MOD_CAMAN,  /**< CA manager module*/
    AM_MOD_CI,     /**< DVB-CI module*/
    AM_MOD_USERDATA, /**< MPEG user data reader device module*/
    AM_MOD_CC,     /**< Close caption module*/
    AM_MOD_AD,     /**< Audio description module*/
    AM_MOD_UPD,    /**< Uploader module*/
    AM_MOD_TFILE, /*File wrapper module*/
    AM_MOD_SCTE27,
    AM_MOD_MAX
};

/** Get the error code base of each module
 * _mod The module's index
 */
#define ERROR_BASE(_mod)    ((_mod)<<24)
#define AM_SUCCESS     (0)
#define AM_FAILURE     (-1)
#define UNUSED(x) (void)(x)

/** 计算数值_a和_b中的最大值*/
#define AM_MAX(_a,_b)    ((_a)>(_b)?(_a):(_b))

/** 计算数值_a和_b中的最小值*/
#define AM_MIN(_a,_b)    ((_a)<(_b)?(_a):(_b))

/** 计算数值_a的绝对值*/
#define AM_ABS(_a)       ((_a)>0?(_a):-(_a))

/** 计算数值a与b差值的绝对值*/
#define AM_ABSSUB(a,b) ((a>=b)?(a-b):(b-a))

/** 添加在命令行式宏定义的开头
 * 一些宏需要完成一系列语句，为了使这些语句形成一个整体不被打断，需要用
 * AM_MACRO_BEGIN和AM_MACRO_END将这些语句括起来。如:
 * \code
 * #define CHECK(_n)    \
 *    AM_MACRO_BEGIN \
 *    if ((_n)>0) printf(">0"); \
 *    else if ((n)<0) printf("<0"); \
 *    else printf("=0"); \
 *    AM_MACRO_END
 * \endcode
 */
#define AM_MACRO_BEGIN   do {

/** 添加在命令行式定义的末尾*/
#define AM_MACRO_END     } while(0)

/** 计算数组常数的元素数*/
#define AM_ARRAY_SIZE(_a)    (sizeof(_a)/sizeof((_a)[0]))

/** 检查如果返回值是否错误，返回错误代码给调用函数*/
#define AM_TRY(_func) \
    AM_MACRO_BEGIN\
    int _ret;\
    if ((_ret=(_func))!=AM_SUCCESS)\
        return _ret;\
    AM_MACRO_END

/** 检查返回值是否错误，如果错误，跳转到final标号。注意:函数中必须定义"int ret"和标号"final"*/
#define AM_TRY_FINAL(_func)\
    AM_MACRO_BEGIN\
    if ((ret=(_func))!=AM_SUCCESS)\
        goto final;\
    AM_MACRO_END

/** 开始解析一个被指定字符隔开的字符串*/
#define AM_TOKEN_PARSE_BEGIN(_str, _delim, _token) \
    {\
    char *_strb =  strdup(_str);\
    if (_strb) {\
        _token = strtok(_strb, _delim);\
        while (_token != NULL) {

#define AM_TOKEN_PARSE_END(_str, _delim, _token) \
        _token = strtok(NULL, _delim);\
        }\
        free(_strb);\
    }\
    }


/** 从一个被指定字符隔开的字符串中取指定位置的值，int类型，如未找到指定位置，则使用默认值_default代替*/
#define AM_TOKEN_VALUE_INT(_str, _delim, _index, _default) \
    ({\
        char *token;\
        char *_strbak = strdup(_str);\
        int counter = 0;\
        int val = _default;\
        if (_strbak != NULL) {\
            AM_TOKEN_PARSE_BEGIN(_strbak, _delim, token)\
                if (counter == (_index)) {\
                    val = atoi(token);\
                    break;\
                }\
                counter++;\
            AM_TOKEN_PARSE_END(_strbak, _delim, token)\
            free(_strbak);\
        }\
        val;\
    })

#define pthread_cond_init(c, a)\
    ({\
        pthread_condattr_t attr;\
        pthread_condattr_t *pattr;\
        int r;\
        if (a) {\
            pattr = a;\
        } else {\
            pattr = &attr;\
            pthread_condattr_init(pattr);\
        }\
        pthread_condattr_setclock(pattr, CLOCK_MONOTONIC);\
        r = pthread_cond_init(c, pattr);\
        if (pattr == &attr) {\
            pthread_condattr_destroy(pattr);\
        }\
        r;\
     })

/** ADP global lock*/
extern pthread_mutex_t am_gAdpLock;
extern pthread_mutex_t am_gHwDmxLock;

/** Read a string from a file
  * name file name
  * buf buffer to store strings
  * len buffer size
  * - AM_SUCCESS Success
  * - other values Error code
  */
extern int AmlogicFileRead(const char *name, char *buf, int len);

#ifdef __cplusplus
}
#endif

#endif

