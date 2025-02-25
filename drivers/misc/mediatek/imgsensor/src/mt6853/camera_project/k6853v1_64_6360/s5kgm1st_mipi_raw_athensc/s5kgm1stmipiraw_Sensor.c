/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   gm1stmipi_Sensor.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5kgm1stmipiraw_Sensor.h"
#include "imgsensor_common.h"

#ifndef OPLUS_FEATURE_CAMERA_COMMON
#define OPLUS_FEATURE_CAMERA_COMMON
#endif

#ifdef OPLUS_FEATURE_CAMERA_COMMON
/*Feng.Hu@Camera.Driver 20170815 add for multi project using one build*/
#include <soc/oplus/system/oplus_project.h>
#endif

/***************Modify Following Strings for Debug**********************/
#define PFX "gm1st_camera_sensor"
#define LOG_1 LOG_INF("gm1st,MIPI 4LANE\n")
/****************************   Modify end  **************************/
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

#ifdef OPLUS_FEATURE_CAMERA_COMMON
#define DEVICE_VERSION_GM1ST     "gm1st"
extern void Oplusimgsensor_Registdeviceinfo(char *name, char *version, u8 module_id);
//static kal_uint8 deviceInfo_register_value = 0x00;
static kal_uint32 streaming_control(kal_bool enable);
#define MODULE_ID_OFFSET 0x0000
#define I2C_BUFFER_LEN 225  /* trans# max is 255, each 3 bytes */
#endif

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len);
static bool bNeedSetNormalMode = KAL_FALSE;
static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = ATHENSC_S5KGM1ST_SENSOR_ID,
    .module_id = 0x05,  //0x01 Sunny,0x05 QTEK
    .checksum_value = 0x8ac2d94a,

    .pre = {
        .pclk = 482000000,
        .linelength = 5024,
        .framelength = 3194,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4000,
        .grabwindow_height = 3000,
        .mipi_data_lp2hs_settle_dc = 85,
        /*   following for GetDefaultFramerateByScenario()  */
        .mipi_pixel_rate = 460800000,
        .max_framerate = 300,
    },

    .cap = {
        .pclk = 482000000,
        .linelength = 5024,
        .framelength = 3194,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4000,
        .grabwindow_height = 3000,
        .mipi_data_lp2hs_settle_dc = 85,
        /*   following for GetDefaultFramerateByScenario()  */
        .mipi_pixel_rate = 460800000,
        .max_framerate = 300,
    },

    .normal_video = { /*4000*2256@30fps*/
        .pclk = 482000000,
        .linelength = 5024,
        .framelength = 3194,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4000,
        .grabwindow_height = 2256,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 476800000,
        .max_framerate = 300,
    },

    .hs_video = { /* 1920x1080 @120fps (binning)*/
        .pclk = 482000000,
        .linelength = 2512,
        .framelength = 1599,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1920,
        .grabwindow_height = 1080,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 476800000,
        .max_framerate = 1200,
    },

    .slim_video = { /* 4608*2592@30fps */
        .pclk = 482000000,
        .linelength = 5024,
        .framelength = 3194,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4000,
        .grabwindow_height = 2256,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 476800000,
        .max_framerate = 300,
    },

    .custom1 = {
        .pclk = 482000000,
        .linelength = 5024,
        .framelength = 3992,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 4000,
        .grabwindow_height = 3000,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 240,
        .mipi_pixel_rate = 460800000,
    },

    .custom2 = {
        .pclk = 482000000,
        .linelength = 2512,
        .framelength = 3184,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2000,
        .grabwindow_height = 1500,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 460800000,
        .max_framerate = 600,
    },

    .custom3 = {
        .pclk = 492000000,
        .linelength = 2512,
        .framelength = 816,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1280,
        .grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 291200000,
        .max_framerate = 2400,
    },

    .custom4 = {
        .pclk = 492000000,
        .linelength = 2512,
        .framelength = 816,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1280,
        .grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 291200000,
        .max_framerate = 2400,
    },

    .custom5 = {
        .pclk = 482000000,
        .linelength = 5024,
        .framelength = 3194,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4000,
        .grabwindow_height = 2600,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 460800000,
        .max_framerate = 300,
    },

    .margin = 2,        /* sensor framelength & shutter margin */
    .min_shutter = 2,   /* min shutter */

    .min_gain = 64,
    .max_gain = 1024,
    .min_gain_iso = 100,
    .gain_step = 2,
    .gain_type = 2, //0-SONY; 1-OV; 2 - SUMSUN; 3 -HYNIX; 4 -GC

    .max_frame_length = 0xfffd,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,    /* isp gain delay frame for AE cycle */
    .ihdr_support = 0,  /* 1, support; 0,not support */
    .ihdr_le_firstline = 0, /* 1,le first ; 0, se first */
    .sensor_mode_num = 10,  /* support sensor mode num */

    .cap_delay_frame = 3,   /* enter capture delay frame num */
    .pre_delay_frame = 3,   /* enter preview delay frame num */
    .video_delay_frame = 3, /* enter video delay frame num */
    .hs_video_delay_frame = 3,
    .slim_video_delay_frame = 3,    /* enter slim video delay frame num */
    .custom1_delay_frame = 3,   /* enter custom1 delay frame num */
    .custom2_delay_frame = 3,   /* enter custom2 delay frame num */
    .custom3_delay_frame = 3,   /* enter custom3 delay frame num */
    .custom4_delay_frame = 3,   /* enter custom4 delay frame num */
    .custom5_delay_frame = 3,   /* enter custom5 delay frame num */
    .frame_time_delay_frame = 2,

    .isp_driving_current = ISP_DRIVING_6MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
    .mipi_settle_delay_mode = 1,
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
    .mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
    .mipi_lane_num = SENSOR_MIPI_4_LANE,
    .i2c_addr_table = {0x5B, 0x5A, 0xff},
    .i2c_speed = 400, /* i2c read/write speed */
};

static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL, /* NORMAL information */
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3D0,   /* current shutter */
    .gain = 0x100,      /* current gain */
    .dummy_pixel = 0,   /* current dummypixel */
    .dummy_line = 0,    /* current dummyline */
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = 0,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
    .i2c_write_id = 0x5B, /* record current sensor's i2c write id */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
    {4000, 3000,  0,   0, 4000, 3000, 4000, 3000, 0, 0, 4000, 3000, 0, 0, 4000, 3000}, /*Preview*/
    {4000, 3000,  0,   0, 4000, 3000, 4000, 3000, 0, 0, 4000, 3000, 0, 0, 4000, 3000}, /* capture */
    {4000, 3000,  0, 372, 4000, 2256, 4000, 2256, 0, 0, 4000, 2256, 0, 0, 4000, 2256}, /* normal video */
    {4000, 3000, 80, 420, 3840, 2160, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080}, /* hs_video */
    {4000, 3000,  0, 372, 4000, 2256, 4000, 2256, 0, 0, 4000, 2256, 0, 0, 4000, 2256}, /* slim video */
    {4000, 3000,  0,   0, 4000, 3000, 4000, 3000, 0, 0, 4000, 3000, 0, 0, 4000, 3000}, /* custom1 */
    {4000, 3000, 0, 0, 4000, 3000, 2000, 1500, 0, 0, 2000, 1500, 0, 0,2000, 1500}, /* custom2 */
    {4000, 3000, 80, 420, 3840, 2160, 1280,  720, 0, 0, 1280,  720, 0, 0, 1280,  720}, /* custom3 */
    {4000, 3000, 80, 420, 3840, 2160, 1280,  720, 0, 0, 1280,  720, 0, 0, 1280,  720}, /* custom4 */
    {4000, 3000,  0, 200, 4000, 2600, 4000, 2600, 0, 0, 4000, 2600, 0, 0, 4000, 2600}, /* custom5 */
};

/*VC1 for HDR(DT=0X35), VC2 for PDAF(DT=0X30), unit : 10bit */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[4] = {
    /* Preview mode setting */
   {0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
    0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
    0x01, 0x30, 0x026C, 0x02D0, 0x03, 0x00, 0x0000, 0x0000},
    /* Normal_Video mode setting */
   {0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
    0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
    0x01, 0x30, 0x026C, 0x0280, 0x03, 0x00, 0x0000, 0x0000},
    /* 4K_Video mode setting */
   {0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
    0x00, 0x2B, 0x0FA0, 0x08D0, 0x01, 0x00, 0x0000, 0x0000,
    0x01, 0x30, 0x026C, 0x02D0, 0x03, 0x00, 0x0000, 0x0000},
    /* Slim_Video mode setting */
   {0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
    0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
    0x01, 0x30, 0x026C, 0x0230, 0x03, 0x00, 0x0000, 0x0000},
};

/* If mirror flip */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX =  16,
    .i4OffsetY =  60,
    .i4PitchX  =  32,
    .i4PitchY  =  32,
    .i4PairNum  = 16,
    .i4SubBlkW  = 8,
    .i4SubBlkH  = 8,
    .i4PosL = {{18, 61},{26, 61},{34, 61},{42, 61},{22, 73},{30, 73},{38, 73},{46, 73},
                {18, 81},{26, 81},{34, 81},{42, 81},{22, 85},{30, 85},{38, 85},{46, 85}},
    .i4PosR = {{18, 65},{26, 65},{34, 65},{42, 65},{22, 69},{30, 69},{38, 69},{46, 69},
                {18, 77},{26, 77},{34, 77},{42, 77},{22, 89},{30, 89},{38, 89},{46, 89}},
    .i4BlockNumX = 124,
    .i4BlockNumY = 90,
    .iMirrorFlip = 0,
    .i4Crop = { {0,0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_20_13 =
{
    .i4OffsetX =  16,
    .i4OffsetY =  60,
    .i4PitchX  =  32,
    .i4PitchY  =  32,
    .i4PairNum  = 16,
    .i4SubBlkW  = 8,
    .i4SubBlkH  = 8,
    .i4PosL = {{18, 61},{26, 61},{34, 61},{42, 61},{22, 73},{30, 73},{38, 73},{46, 73},
                {18, 81},{26, 81},{34, 81},{42, 81},{22, 85},{30, 85},{38, 85},{46, 85}},
    .i4PosR = {{18, 65},{26, 65},{34, 65},{42, 65},{22, 69},{30, 69},{38, 69},{46, 69},
                {18, 77},{26, 77},{34, 77},{42, 77},{22, 89},{30, 89},{38, 89},{46, 89}},
    .i4BlockNumX = 124,
    .i4BlockNumY = 80,
    .iMirrorFlip = 0,
    .i4Crop = { {0,0}, {0, 0}, {0, 200}, {0, 0}, {0, 200}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_16_9 =
{
    .i4OffsetX = 16,
    .i4OffsetY = 8,
    .i4PitchX  = 32,
    .i4PitchY  = 32,
    .i4PairNum  =16,
    .i4SubBlkW  =8,
    .i4SubBlkH  =8,
    .i4PosL = {{18,9},{26,9},{34,9},{42,9},{22,21},{30,21},{38,21},{46,21},{18,29},{26,29},{34,29},{42,29},{22,33},{30,33},{38,33},{46,33}},
    .i4PosR = {{18,13},{26,13},{34,13},{42,13},{22,17},{30,17},{38,17},{46,17},{18,25},{26,25},{34,25},{42,25},{22,37},{30,37},{38,37},{46,37}},
    .i4BlockNumX = 124,
    .i4BlockNumY = 70,
    .i4LeFirst = 0,
    .iMirrorFlip = 0,
    .i4Crop = { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
};


static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_binning =
{
    .i4OffsetX =  16,
    .i4OffsetY =  60,
    .i4PitchX  =  32,
    .i4PitchY  =  32,
    .i4PairNum  = 16,
    .i4SubBlkW  = 8,
    .i4SubBlkH  = 8,
    .i4PosL = {{18, 61},{26, 61},{34, 61},{42, 61},{22, 73},{30, 73},{38, 73},{46, 73},
                {18, 81},{26, 81},{34, 81},{42, 81},{22, 85},{30, 85},{38, 85},{46, 85}},
    .i4PosR = {{18, 65},{26, 65},{34, 65},{42, 65},{22, 69},{30, 69},{38, 69},{46, 69},
                {18, 77},{26, 77},{34, 77},{42, 77},{22, 89},{30, 89},{38, 89},{46, 89}},
    .i4BlockNumX = 124,
    .i4BlockNumY = 90,
    .iMirrorFlip = 0,
    .i4Crop = { {0,0}, {0, 0}, {0, 0}, {80, 420}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
};
#if 0
#ifdef OPLUS_FEATURE_CAMERA_COMMON
/*Caohua.Lin@Camera.Driver add for 18011/18311  board 20180723*/
static kal_uint16 read_module_id(void)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(MODULE_ID_OFFSET >> 8) , (char)(MODULE_ID_OFFSET & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte,1,0xA0/*EEPROM_READ_ID*/);
    if (get_byte == 0) {
        iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA0/*EEPROM_READ_ID*/);
    }
    return get_byte;

}
#endif
#endif
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

    iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
    return ((get_byte<<8)&0xff00) | ((get_byte>>8)&0x00ff);
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
    char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
                 (char)(para >> 8), (char)(para & 0xFF)};

    /*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
    /* Add this func to set i2c speed by each sensor */
    iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}


static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
    kal_uint16 get_byte = 0;
    char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

    iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
    return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
            (char)(para & 0xFF)};

    iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}
#if 0
#define  CAMERA_MODULE_INFO_LENGTH  (8)
static kal_uint8 CAM_SN[CAMERA_MODULE_SN_LENGTH];
static kal_uint8 CAM_INFO[CAMERA_MODULE_INFO_LENGTH];
static kal_uint8 CAM_DUAL_DATA[DUALCAM_CALI_DATA_LENGTH_8ALIGN];
/*Henry.Chang@Camera.Driver add for google ARCode Feature verify 20190531*/
static kal_uint16 read_cmos_eeprom_8(kal_uint16 addr)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte, 1, 0xA0);
    return get_byte;
}

static void read_eepromData(void)
{
    kal_uint16 idx = 0;
    for (idx = 0; idx <DUALCAM_CALI_DATA_LENGTH; idx++) {
        CAM_DUAL_DATA[idx] = read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR + idx);
    }
    for (idx = 0; idx <CAMERA_MODULE_SN_LENGTH; idx++) {
        CAM_SN[idx] = read_cmos_eeprom_8(0xB0 + idx);
        LOG_INF("CAM_SN[%d]: 0x%x  0x%x\n", idx, CAM_SN[idx]);
    }
    CAM_INFO[0] = read_cmos_eeprom_8(0x0);
    CAM_INFO[1] = read_cmos_eeprom_8(0x1);
    CAM_INFO[2] = read_cmos_eeprom_8(0x6);
    CAM_INFO[3] = read_cmos_eeprom_8(0x7);
    CAM_INFO[4] = read_cmos_eeprom_8(0x8);
    CAM_INFO[5] = read_cmos_eeprom_8(0x9);
    CAM_INFO[6] = read_cmos_eeprom_8(0xA);
    CAM_INFO[7] = read_cmos_eeprom_8(0xB);
}

/*Henry.Chang@camera.driver 20181129, add for sensor Module SET*/
#define   WRITE_DATA_MAX_LENGTH     (16)
static kal_int32 table_write_eeprom_30Bytes(kal_uint16 addr, kal_uint8 *para, kal_uint32 len)
{
    kal_int32 ret = IMGSENSOR_RETURN_SUCCESS;
    char pusendcmd[WRITE_DATA_MAX_LENGTH+2];
    pusendcmd[0] = (char)(addr >> 8);
    pusendcmd[1] = (char)(addr & 0xFF);

    memcpy(&pusendcmd[2], para, len);

    ret = iBurstWriteReg((kal_uint8 *)pusendcmd , (len + 2), 0xA0);

    return ret;
}

static kal_int32 write_eeprom_protect(kal_uint16 enable)
{
    kal_int32 ret = IMGSENSOR_RETURN_SUCCESS;
    char pusendcmd[3];
    pusendcmd[0] = 0x80;
    pusendcmd[1] = 0x00;
    if (enable)
        pusendcmd[2] = 0x0E;
    else
        pusendcmd[2] = 0x00;

    ret = iBurstWriteReg((kal_uint8 *)pusendcmd , 3, 0xA0);
    return ret;
}

/*Henry.Chang@camera.driver 20181129, add for sensor Module SET*/
static kal_int32 write_Module_data(ACDK_SENSOR_ENGMODE_STEREO_STRUCT * pStereodata)
{
    kal_int32  ret = IMGSENSOR_RETURN_SUCCESS;
    kal_uint16 data_base, data_length;
    kal_uint32 idx, idy;
    kal_uint8 *pData;
    UINT32 i = 0;
    if(pStereodata != NULL) {
        LOG_INF("SET_SENSOR_OTP: 0x%x %d 0x%x %d\n",
                       pStereodata->uSensorId,
                       pStereodata->uDeviceId,
                       pStereodata->baseAddr,
                       pStereodata->dataLength);

        data_base = pStereodata->baseAddr;
        data_length = pStereodata->dataLength;
        pData = pStereodata->uData;
        if ((pStereodata->uSensorId == GM1ST_SENSOR_ID) && (data_length == DUALCAM_CALI_DATA_LENGTH)
            && (data_base == GM1ST_STEREO_START_ADDR)) {
            LOG_INF("Write: %x %x %x %x %x %x %x %x\n", pData[0], pData[39], pData[40], pData[1556],
                    pData[1557], pData[1558], pData[1559], pData[1560]);
            idx = data_length/WRITE_DATA_MAX_LENGTH;
            idy = data_length%WRITE_DATA_MAX_LENGTH;
            /* close write protect */
            write_eeprom_protect(0);
            msleep(6);
            for (i = 0; i < idx; i++ ) {
                ret = table_write_eeprom_30Bytes((data_base+WRITE_DATA_MAX_LENGTH*i),
                        &pData[WRITE_DATA_MAX_LENGTH*i], WRITE_DATA_MAX_LENGTH);
                if (ret != IMGSENSOR_RETURN_SUCCESS) {
                    LOG_ERR("write_eeprom error: i= %d\n", i);
                    /* open write protect */
                    write_eeprom_protect(1);
                    msleep(6);
                    return IMGSENSOR_RETURN_ERROR;
                }
                msleep(6);
            }
            ret = table_write_eeprom_30Bytes((data_base+WRITE_DATA_MAX_LENGTH*idx),
                    &pData[WRITE_DATA_MAX_LENGTH*idx], idy);
            if (ret != IMGSENSOR_RETURN_SUCCESS) {
                LOG_ERR("write_eeprom error: idx= %d idy= %d\n", idx, idy);
                /* open write protect */
                write_eeprom_protect(1);
                msleep(6);
                return IMGSENSOR_RETURN_ERROR;
            }
            msleep(6);
            /* open write protect */
            write_eeprom_protect(1);
            msleep(6);
            LOG_INF("com_0:0x%x\n", read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR));
            LOG_INF("com_39:0x%x\n", read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR+39));
            LOG_INF("innal_40:0x%x\n", read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR+40));
            LOG_INF("innal_1556:0x%x\n", read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR+1556));
            LOG_INF("tail1_1557:0x%x\n", read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR+1557));
            LOG_INF("tail2_1558:0x%x\n", read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR+1558));
            LOG_INF("tail3_1559:0x%x\n", read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR+1559));
            LOG_INF("tail4_1560:0x%x\n", read_cmos_eeprom_8(GM1ST_STEREO_START_ADDR+1560));
            LOG_INF("write_Module_data Write end\n");
        } else {
            LOG_ERR("Invalid Sensor id:0x%x write eeprom\n", pStereodata->uSensorId);
            return IMGSENSOR_RETURN_ERROR;
        }
    } else {
        LOG_ERR("s5kgh1 write_Module_data pStereodata is null\n");
        return IMGSENSOR_RETURN_ERROR;
    }
    return ret;
}
#endif
static void set_dummy(void)
{
    write_cmos_sensor(0x0340, imgsensor.frame_length);
    write_cmos_sensor(0x0342, imgsensor.line_length);
    LOG_INF("dummyline = %d, dummypixels = %d \n",
        imgsensor.dummy_line, imgsensor.dummy_pixel);
}   /*  set_dummy  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
    kal_uint8 itemp;
    LOG_INF("image_mirror = %d\n", image_mirror);
    itemp=read_cmos_sensor(0x0101);
    LOG_INF("image_mirror itemp = %d\n", itemp);
    itemp &= ~0x03;

    switch(image_mirror) {
        case IMAGE_NORMAL:
            write_cmos_sensor_8(0x0101, itemp);
            break;

        case IMAGE_V_MIRROR:
            write_cmos_sensor_8(0x0101, itemp | 0x02);
            break;

        case IMAGE_H_MIRROR:
            write_cmos_sensor_8(0x0101, itemp | 0x01);
            break;

        case IMAGE_HV_MIRROR:
            write_cmos_sensor_8(0x0101, itemp | 0x03);
            break;
    }
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
    kal_uint32 frame_length = imgsensor.frame_length;

    LOG_INF("framerate = %d, min framelength should enable %d\n", framerate,
        min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    if (frame_length >= imgsensor.min_frame_length) {
        imgsensor.frame_length = frame_length;
    } else {
        imgsensor.frame_length = imgsensor.min_frame_length;
    }
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en) {
        imgsensor.min_frame_length = imgsensor.frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}   /*  set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
    kal_uint16 realtime_fps = 0;
    kal_uint64 CintR = 0;
    kal_uint64 Time_Farme = 0;

    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin) {
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    } else {
        imgsensor.frame_length = imgsensor.min_frame_length;
    }
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);
    if (shutter < imgsensor_info.min_shutter) {
        shutter = imgsensor_info.min_shutter;
    }

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if (realtime_fps >= 297 && realtime_fps <= 305) {
            set_max_framerate(296,0);
        } else if (realtime_fps >= 147 && realtime_fps <= 150) {
            set_max_framerate(146,0);
        } else {
            // Extend frame length
            write_cmos_sensor(0x0340, imgsensor.frame_length);
        }
    } else {
        // Extend frame length
        write_cmos_sensor(0x0340, imgsensor.frame_length);
    }

    if (shutter >= 0xFFF0) {  // need to modify line_length & PCLK
        bNeedSetNormalMode = KAL_TRUE;

        if (shutter >= 3448275) {  //>32s
            shutter = 3448275;
        }

        CintR = ( (unsigned long long)shutter) / 128;
        Time_Farme = CintR + 0x0002;  // 1st framelength
        LOG_INF("CintR =%d \n", CintR);

        write_cmos_sensor(0x0340, Time_Farme & 0xFFFF);  // Framelength
        write_cmos_sensor(0x0202, CintR & 0xFFFF);  //shutter
        write_cmos_sensor(0x0702, 0x0700);
        write_cmos_sensor(0x0704, 0x0700);
    } else {
        if (bNeedSetNormalMode) {
            LOG_INF("exit long shutter\n");
            write_cmos_sensor(0x0702, 0x0000);
            write_cmos_sensor(0x0704, 0x0000);
            bNeedSetNormalMode = KAL_FALSE;
        }

        write_cmos_sensor(0x0340, imgsensor.frame_length);
        write_cmos_sensor(0x0202, imgsensor.shutter);
    }
    LOG_INF("shutter =%d, framelength =%d \n", shutter,imgsensor.frame_length);
}   /*  write_shutter  */

/*************************************************************************
 * FUNCTION
 *  set_shutter
 *
 * DESCRIPTION
 *  This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *  iShutter : exposured lines
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
    unsigned long flags;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    write_shutter(shutter);
} /* set_shutter */


/*************************************************************************
 * FUNCTION
 *  set_shutter_frame_length
 *
 * DESCRIPTION
 *  for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint32 shutter,
                     kal_uint32 frame_length,
                     kal_bool auto_extend_en)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;
    kal_uint64 CintR = 0;
    kal_uint64 Time_Farme = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    spin_lock(&imgsensor_drv_lock);
    /* Change frame time */
    if (frame_length > 1)
        dummy_line = frame_length - imgsensor.frame_length;

    imgsensor.frame_length = imgsensor.frame_length + dummy_line;

    if (shutter > imgsensor.frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    shutter = (shutter < imgsensor_info.min_shutter)
            ? imgsensor_info.min_shutter : shutter;

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if (realtime_fps >= 297 && realtime_fps <= 305) {
            set_max_framerate(296, 0);
        } else if (realtime_fps >= 147 && realtime_fps <= 150) {
            set_max_framerate(146, 0);
        } else {
            /* Extend frame length */
            write_cmos_sensor(0x0340, imgsensor.frame_length);
        }
    } else {
        /* Extend frame length */
        write_cmos_sensor(0x0340, imgsensor.frame_length);
    }

    if (shutter >= 0xFFF0) {
        bNeedSetNormalMode = KAL_TRUE;

        if (shutter >= 1538000) {
            shutter = 1538000;
        }
        CintR = (5013 * (unsigned long long)shutter) / 321536;
        Time_Farme = CintR + 0x0002;
        LOG_INF("CintR =%d \n", CintR);

        write_cmos_sensor(0x0340, Time_Farme & 0xFFFF);
        write_cmos_sensor(0x0202, CintR & 0xFFFF);
        write_cmos_sensor(0x0702, 0x0600);
        write_cmos_sensor(0x0704, 0x0600);
    } else {
        if (bNeedSetNormalMode) {
            LOG_INF("exit long shutter\n");
            write_cmos_sensor(0x0702, 0x0000);
            write_cmos_sensor(0x0704, 0x0000);
            bNeedSetNormalMode = KAL_FALSE;
        }

        write_cmos_sensor(0x0340, imgsensor.frame_length);
        write_cmos_sensor(0x0202, imgsensor.shutter);
    }

    LOG_INF("Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
        shutter, imgsensor.frame_length, frame_length, dummy_line, read_cmos_sensor(0x0350));
}   /* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
     kal_uint16 reg_gain = 0x0;

    reg_gain = gain/2;
    return (kal_uint16)reg_gain;
}

/*************************************************************************
 * FUNCTION
 *  set_gain
 *
 * DESCRIPTION
 *  This function is to set global gain to sensor.
 *
 * PARAMETERS
 *  iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *  the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;

    if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
        LOG_INF("Error gain setting");
        if (gain < BASEGAIN) {
            gain = BASEGAIN;
        } else if (gain > 16 * BASEGAIN) {
            gain = 16 * BASEGAIN;
        }
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    write_cmos_sensor_8(0x0204, (reg_gain >> 8));
    write_cmos_sensor_8(0x0205, (reg_gain & 0xff));

    return gain;
}   /*  set_gain  */

static kal_uint32 gm1st_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
    LOG_INF("awb_gain: 0x100\n");

    write_cmos_sensor(0x0D82, 0x100);
    write_cmos_sensor(0x0D84, 0x100);
    write_cmos_sensor(0x0D86, 0x100);
    return ERROR_NONE;
}

/*write AWB gain to sensor*/
static void feedback_awbgain(kal_uint32 r_gain, kal_uint32 b_gain)
{
    UINT32 r_gain_int = 0;
    UINT32 b_gain_int = 0;

    r_gain_int = r_gain / 2;
    b_gain_int = b_gain / 2;

    /*write r_gain*/
    write_cmos_sensor(0x0D82, r_gain_int);
    /*write _gain*/
    write_cmos_sensor(0x0D86, b_gain_int);
}

static void gm1st_set_lsc_reg_setting(
        kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
    int i;
    int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};
    /*0:B,1:Gb,2:Gr,3:R*/

    LOG_INF("E! index:%d, regNum:%d\n", index, regNum);

    write_cmos_sensor_8(0x0B00, 0x01); /*lsc enable*/
    write_cmos_sensor_8(0x9014, 0x01);
    write_cmos_sensor_8(0x4439, 0x01);
    mdelay(1);
    LOG_INF("Addr 0xB870, 0x380D Value:0x%x %x\n",
        read_cmos_sensor_8(0xB870), read_cmos_sensor_8(0x380D));
    /*define Knot point, 2'b01:u3.7*/
    write_cmos_sensor_8(0x9750, 0x01);
    write_cmos_sensor_8(0x9751, 0x01);
    write_cmos_sensor_8(0x9752, 0x01);
    write_cmos_sensor_8(0x9753, 0x01);

    for (i = 0; i < regNum; i++)
        write_cmos_sensor(startAddr[index] + 2*i, regDa[i]);

    write_cmos_sensor_8(0x0B00, 0x00); /*lsc disable*/
}
/*************************************************************************
 * FUNCTION
 *  night_mode
 *
 * DESCRIPTION
 *  This function night mode of sensor.
 *
 * PARAMETERS
 *  bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 streaming_control(kal_bool enable)
{
    int timeout = (10000 / imgsensor.current_fps) + 1;
    int i = 0;
    int framecnt = 0;

    LOG_INF("streaming_enable(0= Sw Standby,1= streaming): %d\n", enable);
    if (enable) {
        write_cmos_sensor_8(0x0100, 0x01);
        mDELAY(10);
    } else {
        write_cmos_sensor_8(0x0100, 0x00);
        for (i = 0; i < timeout; i++) {
            mDELAY(5);
            framecnt = read_cmos_sensor_8(0x0005);
            if (framecnt == 0xFF) {
                LOG_INF(" Stream Off OK at i=%d.\n", i);
                return ERROR_NONE;
            }
        }
        LOG_INF("Stream Off Fail! framecnt= %d.\n", framecnt);
    }
    return ERROR_NONE;
}

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
    char puSendCmd[I2C_BUFFER_LEN];
    kal_uint32 tosend, IDX;
    kal_uint16 addr = 0, addr_last = 0, data;

    tosend = 0;
    IDX = 0;

    while (len > IDX) {
        addr = para[IDX];
        {
            puSendCmd[tosend++] = (char)(addr >> 8);
            puSendCmd[tosend++] = (char)(addr & 0xFF);
            data = para[IDX + 1];
            puSendCmd[tosend++] = (char)(data >> 8);
            puSendCmd[tosend++] = (char)(data & 0xFF);
            IDX += 2;
            addr_last = addr;

        }
        if ((I2C_BUFFER_LEN - tosend) < 4
            || IDX == len || addr != addr_last) {
            iBurstWriteReg_multi(puSendCmd,
                        tosend,
                        imgsensor.i2c_write_id,
                        4,
                        imgsensor_info.i2c_speed);
            tosend = 0;
        }
    }
    return 0;
}

static const u16 uTnpArrayInit1[] = {
0x126F,
0x0000,
0x0000,
0xF000,
0xBB9E,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0xE92D,
0x5FFF,
0x48FF,
0x468B,
0x4617,
0x6800,
0x469A,
0xEA4F,
0x4910,
0xB280,
0x4680,
0x4601,
0x2200,
0x4648,
0xF000,
0xFB02,
0x4DF9,
0xF895,
0x006D,
0x2802,
0xD035,
0x2402,
0x4EF7,
0x4653,
0xF8B6,
0x02B8,
0xFBB0,
0xF0F4,
0xF8A6,
0x02B8,
0xF8D5,
0x1114,
0xF506,
0x762E,
0x4361,
0xF8C5,
0x1114,
0xF8B5,
0x118C,
0x1A41,
0xB289,
0xF825,
0x1B98,
0xF835,
0x2C14,
0x4362,
0x1E52,
0xFB00,
0x1002,
0xF8B5,
0x10F2,
0xFB07,
0xF204,
0x4408,
0xF8C5,
0x00F8,
0x4659,
0x9800,
0xF000,
0xFADB,
0x8830,
0x4641,
0x4360,
0x8030,
0x6FE8,
0x2201,
0xFBB0,
0xF0F4,
0x67E8,
0xB004,
0x4648,
0xE8BD,
0x5FF0,
0xF000,
0xBAC7,
0x2401,
0xE7C8,
0xE92D,
0x41F0,
0x4680,
0x48D8,
0x2200,
0x6841,
0x0C0D,
0xB28E,
0x4631,
0x4628,
0xF000,
0xFAB9,
0x4CD7,
0x4FD5,
0x7820,
0xF897,
0x128B,
0xFB10,
0xF001,
0x7020,
0x4640,
0xF000,
0xFAB8,
0x7820,
0xF897,
0x128B,
0x2201,
0xFBB0,
0xF0F1,
0x7020,
0x4631,
0x4628,
0xE8BD,
0x41F0,
0xF000,
0xBAA1,
0xE92D,
0x47FF,
0x4681,
0x48C6,
0x4617,
0x4688,
0x6880,
0x461C,
0xB285,
0x0C06,
0x2200,
0x4629,
0x4630,
0xF000,
0xFA92,
0x4623,
0x463A,
0x4641,
0x4648,
0xF000,
0xFA9B,
0x4AC1,
0x8890,
0xB3F0,
0x48BE,
0xF890,
0x10BA,
0xB3D1,
0xF8D0,
0x0128,
0x6811,
0x4288,
0xD300,
0x4608,
0x0A01,
0xFAB1,
0xF081,
0xF1C0,
0x0017,
0x40C1,
0xEB02,
0x0040,
0xB2C9,
0x8903,
0x88C2,
0x1A9B,
0x434B,
0x3380,
0xEB02,
0x2223,
0x9200,
0x8A43,
0x8A02,
0x1A9B,
0x434B,
0x3380,
0xEB02,
0x2223,
0x9201,
0x8B83,
0x8B42,
0x1A9B,
0x434B,
0x3380,
0xEB02,
0x2223,
0x9202,
0x8CC2,
0x8C80,
0x1A12,
0x434A,
0x3280,
0xEB00,
0x2022,
0x9003,
0x2200,
0x4668,
0xF854,
0x1022,
0xF850,
0x3022,
0x4359,
0x0B09,
0xF844,
0x1022,
0x1C52,
0xE000,
0xE001,
0x2A04,
0xD3F2,
0xB004,
0x4629,
0x4630,
0xE8BD,
0x47F0,
0x2201,
0xF000,
0xBA3F,
0xE92D,
0x41F0,
0xF44F,
0x717A,
0xFBB0,
0xF2F1,
0xFBB0,
0xF5F1,
0xFB01,
0x0712,
0x2400,
0x4E93,
0xE006,
0xF248,
0x01E8,
0x4348,
0x0B40,
0xF000,
0xFA40,
0x1C64,
0x6B70,
0x42AC,
0xEA4F,
0x0090,
0xD3F3,
0x4378,
0xE8BD,
0x41F0,
0xEB00,
0x1040,
0x0B40,
0xF000,
0xBA32,
0xB570,
0x2400,
0x4D8A,
0xE00C,
0xF5A0,
0x427F,
0x3AFE,
0xD013,
0x1E52,
0xD014,
0xF891,
0x110E,
0xF000,
0xFA29,
0x1C64,
0x2C14,
0xD205,
0xEB05,
0x0184,
0xF8B1,
0x010C,
0x2800,
0xD1EC,
0x497D,
0x2004,
0xF8A1,
0x06CA,
0xBD70,
0xF891,
0x010E,
0xE005,
0xF891,
0x010E,
0xF44F,
0x717A,
0xFB10,
0xF001,
0xF000,
0xFA15,
0xE7E5,
0xB570,
0x2400,
0x4D76,
0xE00C,
0xF5A0,
0x427F,
0x3AFE,
0xD013,
0x1E52,
0xD014,
0xF891,
0x115E,
0xF000,
0xFA01,
0x1C64,
0x2C14,
0xD205,
0xEB05,
0x0184,
0xF8B1,
0x015C,
0x2800,
0xD1EC,
0x4969,
0x2002,
0xF8A1,
0x06CA,
0xBD70,
0xF891,
0x015E,
0xE005,
0xF891,
0x015E,
0xF44F,
0x717A,
0xFB10,
0xF001,
0xF000,
0xF9ED,
0xE7E5,
0xB5F8,
0x4D60,
0xF8B5,
0x06CA,
0x2803,
0xD15B,
0x4E5F,
0xF896,
0x0038,
0xB190,
0xF896,
0x0039,
0x280A,
0xD80E,
0x2400,
0xE008,
0xEB06,
0x0044,
0x1932,
0x8F40,
0x2101,
0x324E,
0xF000,
0xF9D9,
0x1C64,
0xF896,
0x0039,
0x42A0,
0xD8F2,
0xF8D5,
0x06DC,
0x4780,
0xF896,
0x002E,
0xF44F,
0x717A,
0xFB10,
0xF001,
0xF000,
0xF9C4,
0x2002,
0xF000,
0xF9CB,
0x2001,
0xF000,
0xF9CD,
0x494D,
0x2000,
0x8348,
0x4C4B,
0xF894,
0x20FB,
0x834A,
0xF895,
0x10AC,
0xB1B1,
0xF885,
0x0748,
0xF000,
0xF9C4,
0x4606,
0x4630,
0xF000,
0xF9C5,
0xF8D4,
0x1264,
0x4281,
0xD201,
0x2101,
0xE000,
0x2100,
0xF895,
0x0748,
0xF88D,
0x0000,
0xF89D,
0x0000,
0x4308,
0xD0ED,
0xF895,
0x0698,
0x2800,
0xD00E,
0x2201,
0x0214,
0xF648,
0x25F8,
0x4621,
0x4628,
0xF000,
0xF9AF,
0x4621,
0x4628,
0xE8BD,
0x40F8,
0x2200,
0xF000,
0xB9A8,
0xBDF8,
0xE92D,
0x41F0,
0x4C34,
0x4932,
0x4606,
0xF894,
0x7069,
0x8889,
0xF894,
0x2081,
0x2000,
0xB1C1,
0x4621,
0xF8D1,
0x1094,
0xB172,
0xB18F,
0x4608,
0xF000,
0xF998,
0x4605,
0x6FE0,
0xF000,
0xF994,
0x4285,
0xD202,
0xF8D4,
0x0094,
0xE026,
0x6FE0,
0xE024,
0x2F00,
0xD1FB,
0x2A00,
0xD024,
0x4608,
0xE01E,
0x491E,
0x8E0D,
0x6B49,
0x424B,
0xB177,
0x4820,
0x6F80,
0xE010,
0x4242,
0xE000,
0x4602,
0x2900,
0xDB0F,
0x428A,
0xDD0F,
0x4630,
0xE8BD,
0x41F0,
0xF000,
0xB978,
0x2A00,
0xD00C,
0x4817,
0xF8D0,
0x008C,
0xB125,
0x2800,
0xDAED,
0xE7EA,
0x4619,
0xE7ED,
0xF000,
0xF970,
0x60E0,
0x2001,
0xE8BD,
0x81F0,
0xE92D,
0x5FF3,
0xF8DF,
0xA024,
0x460C,
0xF8BA,
0x04BE,
0xB108,
0xF000,
0xF967,
0x4E0B,
0x8830,
0x2801,
0xD119,
0x2C00,
0xD117,
0xE011,
0x0020,
0x9046,
0x0020,
0x302C,
0x0020,
0x302E,
0x0020,
0x8025,
0x0020,
0x0060,
0x0020,
0xE00D,
0x0040,
0x0070,
0x0020,
0xA02B,
0x0020,
0x0036,
0x4D6F,
0x8928,
0xB118,
0x1E40,
0x8128,
0xE8BD,
0x9FFC,
0xF8DF,
0x91B4,
0xF8D9,
0x0000,
0xF8B0,
0x02D6,
0xB138,
0x8930,
0x1C40,
0xB280,
0x8130,
0x28FF,
0xD901,
0x89E8,
0x8130,
0x4866,
0xF04F,
0x0800,
0xF8C6,
0x800C,
0xF8B0,
0xB05E,
0xF240,
0x31FF,
0x200B,
0xF000,
0xF931,
0xF8D9,
0x0000,
0x2700,
0x463C,
0xF8B0,
0x12D4,
0xB121,
0x9800,
0xF000,
0xF918,
0x4607,
0xE00B,
0xF8B0,
0x02D6,
0xB140,
0x8930,
0x89E9,
0x4288,
0xD304,
0x9800,
0xF7FF,
0xFF5B,
0x4607,
0x2401,
0x4638,
0xF000,
0xF91B,
0xF8D9,
0x0000,
0xF8B0,
0x02D6,
0xB908,
0xF8A6,
0x8002,
0xB3C7,
0x4647,
0xF8A6,
0x8008,
0xF000,
0xF913,
0x68F0,
0x6130,
0x8D68,
0xB350,
0x8DA8,
0xBB50,
0xF000,
0xF910,
0x89A8,
0xB320,
0xB31C,
0x6B70,
0x88AA,
0xF8DA,
0x1508,
0xB1CA,
0x4288,
0xDB0C,
0xFB90,
0xF3F1,
0xFB90,
0xF2F1,
0xFB01,
0x0313,
0xEBB3,
0x0F61,
0xDD00,
0x1C52,
0xFB01,
0x0012,
0xE00B,
0xFB91,
0xF3F0,
0xFB91,
0xF2F0,
0xFB00,
0x1313,
0xEBB3,
0x0F60,
0xDD00,
0x1C52,
0x4350,
0x1A40,
0x68F1,
0xEB01,
0x0040,
0x60F0,
0x8DA8,
0xB110,
0x89F0,
0x8730,
0x85AF,
0x4658,
0xE8BD,
0x5FFC,
0xF000,
0xB8E4,
0xB570,
0x4930,
0x4604,
0x2000,
0xF8C1,
0x0530,
0x482F,
0x2200,
0x68C1,
0x0C0D,
0xB28E,
0x4631,
0x4628,
0xF000,
0xF86C,
0x4620,
0xF000,
0xF8D7,
0x4631,
0x4628,
0xE8BD,
0x4070,
0x2201,
0xF000,
0xB862,
0xB510,
0x2200,
0xF2AF,
0x5167,
0x4824,
0xF000,
0xF8CE,
0x4C22,
0x2201,
0xF2AF,
0x41D9,
0x6020,
0x4821,
0xF000,
0xF8C6,
0x2200,
0xF2AF,
0x41A1,
0x6060,
0x481F,
0xF000,
0xF8BF,
0x2200,
0xF2AF,
0x31E9,
0x60A0,
0x481C,
0xF000,
0xF8B8,
0x2200,
0xF2AF,
0x31B7,
0x481A,
0xF000,
0xF8B2,
0x2200,
0xF2AF,
0x3173,
0x4818,
0xF000,
0xF8AC,
0x2200,
0xF2AF,
0x312F,
0x4816,
0xF000,
0xF8A6,
0x2200,
0xF2AF,
0x2175,
0x4814,
0xF000,
0xF8A0,
0x2200,
0xF2AF,
0x11ED,
0x4812,
0xF000,
0xF89A,
0x2200,
0xF2AF,
0x01AD,
0x4810,
0xF000,
0xF894,
0x60E0,
0xBD10,
0x0000,
0x0020,
0xA02B,
0x0020,
0x9008,
0x0040,
0x0070,
0x0020,
0x302E,
0x0020,
0x9046,
0x0000,
0xA724,
0x0100,
0xF31A,
0x0100,
0xBD09,
0x0000,
0x43A9,
0x0000,
0xF171,
0x0000,
0x3972,
0x0000,
0x875D,
0x0000,
0x6B57,
0x0000,
0xED57,
0x0000,
0x8DBF,
0xF64A,
0x3C29,
0xF2C0,
0x0C00,
0x4760,
0xF242,
0x4CA7,
0xF2C0,
0x0C00,
0x4760,
0xF641,
0x2CF3,
0xF2C0,
0x0C01,
0x4760,
0xF640,
0x1CBD,
0xF2C0,
0x0C01,
0x4760,
0xF64A,
0x1C2D,
0xF2C0,
0x0C00,
0x4760,
0xF248,
0x3C0B,
0xF2C0,
0x0C00,
0x4760,
0xF64A,
0x1C43,
0xF2C0,
0x0C00,
0x4760,
0xF248,
0x2C6F,
0xF2C0,
0x0C00,
0x4760,
0xF647,
0x7CA5,
0xF2C0,
0x0C00,
0x4760,
0xF645,
0x5C81,
0xF2C0,
0x0C00,
0x4760,
0xF64A,
0x0CE7,
0xF2C0,
0x0C00,
0x4760,
0xF64A,
0x1C17,
0xF2C0,
0x0C00,
0x4760,
0xF64A,
0x3C45,
0xF2C0,
0x0C00,
0x4760,
0xF64A,
0x2C53,
0xF2C0,
0x0C00,
0x4760,
0xF245,
0x7C37,
0xF2C0,
0x0C00,
0x4760,
0xF245,
0x6CD5,
0xF2C0,
0x0C00,
0x4760,
0xF245,
0x1CC9,
0xF2C0,
0x0C00,
0x4760,
0xF240,
0x2CAB,
0xF2C0,
0x0C00,
0x4760,
0xF644,
0x7C89,
0xF2C0,
0x0C00,
0x4760,
0xF245,
0x6CA5,
0xF2C0,
0x0C00,
0x4760,
0xF245,
0x6CEF,
0xF2C0,
0x0C00,
0x4760,
0xF240,
0x7C6D,
0xF2C0,
0x0C00,
0x4760,
0xF64B,
0x7C8D,
0xF2C0,
0x0C00,
0x4760,
0xF24B,
0x4CAB,
0xF2C0,
0x0C00,
0x4760,
};

static const u16 uTnpArrayInit2[] = {
0x126F,
0x4905,
0x4804,
0x4A05,
0xF8C1,
0x0550,
0x1A10,
0xF8A1,
0x0554,
0xF7FF,
0xBF0E,
0x0020,
0xD846,
0x0020,
0x302E,
0x0020,
0x0060,
0x4770,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0xD108,
0xA600,
0x0000,
0xFF00,
};

static kal_uint16 gm1st_capture_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0008,
    0x0346, 0x0008,
    0x0348, 0x0FA7,
    0x034A, 0x0BBF,
    0x034C, 0x0FA0,
    0x034E, 0x0BB8,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x0C7A,
    0x0342, 0x13A0,
    0x0900, 0x0111,
    0x0380, 0x0001,
    0x0382, 0x0001,
    0x0384, 0x0001,
    0x0386, 0x0001,
    0x0404, 0x1000,
    0x0402, 0x1010,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F1,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x0090,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0x007A,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0100,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xF0F0,
    0x602A, 0x166A,
    0x6F12, 0xF0F0,
    0x602A, 0x118A,
    0x6F12, 0x0802,
    0x602A, 0x151E,
    0x6F12, 0x0001,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0008,
    0x602A, 0x2522,
    0x6F12, 0x0804,
    0x602A, 0x2524,
    0x6F12, 0x0400,
    0x602A, 0x2568,
    0x6F12, 0x5500,
    0x602A, 0x2588,
    0x6F12, 0x1111,
    0x602A, 0x258C,
    0x6F12, 0x1111,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0141,
    0x602A, 0x15A6,
    0x6F12, 0x0545,
    0x602A, 0x15A8,
    0x6F12, 0x0649,
    0x602A, 0x15AA,
    0x6F12, 0x024D,
    0x602A, 0x15AC,
    0x6F12, 0x0151,
    0x602A, 0x15AE,
    0x6F12, 0x0555,
    0x602A, 0x15B0,
    0x6F12, 0x0659,
    0x602A, 0x15B2,
    0x6F12, 0x025D,
    0x602A, 0x15B4,
    0x6F12, 0x0161,
    0x602A, 0x15B6,
    0x6F12, 0x0565,
    0x602A, 0x15B8,
    0x6F12, 0x0669,
    0x602A, 0x15BA,
    0x6F12, 0x026D,
    0x602A, 0x15BC,
    0x6F12, 0x0171,
    0x602A, 0x15BE,
    0x6F12, 0x0575,
    0x602A, 0x15C0,
    0x6F12, 0x0679,
    0x602A, 0x15C2,
    0x6F12, 0x027D,
    0x602A, 0x15C4,
    0x6F12, 0x0141,
    0x602A, 0x15C6,
    0x6F12, 0x0545,
    0x602A, 0x15C8,
    0x6F12, 0x0649,
    0x602A, 0x15CA,
    0x6F12, 0x024D,
    0x602A, 0x15CC,
    0x6F12, 0x0151,
    0x602A, 0x15CE,
    0x6F12, 0x0555,
    0x602A, 0x15D0,
    0x6F12, 0x0659,
    0x602A, 0x15D2,
    0x6F12, 0x025D,
    0x602A, 0x15D4,
    0x6F12, 0x0161,
    0x602A, 0x15D6,
    0x6F12, 0x0565,
    0x602A, 0x15D8,
    0x6F12, 0x0669,
    0x602A, 0x15DA,
    0x6F12, 0x026D,
    0x602A, 0x15DC,
    0x6F12, 0x0171,
    0x602A, 0x15DE,
    0x6F12, 0x0575,
    0x602A, 0x15E0,
    0x6F12, 0x0679,
    0x602A, 0x15E2,
    0x6F12, 0x027D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0101,
    0x0D02, 0x0101,
    0x0114, 0x0301,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0028,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xC040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0020,
    0x602A, 0x25D6,
    0x6F12, 0x0020,
    0x6028, 0x4000,
    0xF45A, 0x0015,
};

static kal_uint16 gm1st_preview_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0008,
    0x0346, 0x0008,
    0x0348, 0x0FA7,
    0x034A, 0x0BBF,
    0x034C, 0x0FA0,
    0x034E, 0x0BB8,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x0C7A,
    0x0342, 0x13A0,
    0x0900, 0x0111,
    0x0380, 0x0001,
    0x0382, 0x0001,
    0x0384, 0x0001,
    0x0386, 0x0001,
    0x0404, 0x1000,
    0x0402, 0x1010,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F1,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x0090,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0x007A,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0100,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xF0F0,
    0x602A, 0x166A,
    0x6F12, 0xF0F0,
    0x602A, 0x118A,
    0x6F12, 0x0802,
    0x602A, 0x151E,
    0x6F12, 0x0001,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0008,
    0x602A, 0x2522,
    0x6F12, 0x0804,
    0x602A, 0x2524,
    0x6F12, 0x0400,
    0x602A, 0x2568,
    0x6F12, 0x5500,
    0x602A, 0x2588,
    0x6F12, 0x1111,
    0x602A, 0x258C,
    0x6F12, 0x1111,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0141,
    0x602A, 0x15A6,
    0x6F12, 0x0545,
    0x602A, 0x15A8,
    0x6F12, 0x0649,
    0x602A, 0x15AA,
    0x6F12, 0x024D,
    0x602A, 0x15AC,
    0x6F12, 0x0151,
    0x602A, 0x15AE,
    0x6F12, 0x0555,
    0x602A, 0x15B0,
    0x6F12, 0x0659,
    0x602A, 0x15B2,
    0x6F12, 0x025D,
    0x602A, 0x15B4,
    0x6F12, 0x0161,
    0x602A, 0x15B6,
    0x6F12, 0x0565,
    0x602A, 0x15B8,
    0x6F12, 0x0669,
    0x602A, 0x15BA,
    0x6F12, 0x026D,
    0x602A, 0x15BC,
    0x6F12, 0x0171,
    0x602A, 0x15BE,
    0x6F12, 0x0575,
    0x602A, 0x15C0,
    0x6F12, 0x0679,
    0x602A, 0x15C2,
    0x6F12, 0x027D,
    0x602A, 0x15C4,
    0x6F12, 0x0141,
    0x602A, 0x15C6,
    0x6F12, 0x0545,
    0x602A, 0x15C8,
    0x6F12, 0x0649,
    0x602A, 0x15CA,
    0x6F12, 0x024D,
    0x602A, 0x15CC,
    0x6F12, 0x0151,
    0x602A, 0x15CE,
    0x6F12, 0x0555,
    0x602A, 0x15D0,
    0x6F12, 0x0659,
    0x602A, 0x15D2,
    0x6F12, 0x025D,
    0x602A, 0x15D4,
    0x6F12, 0x0161,
    0x602A, 0x15D6,
    0x6F12, 0x0565,
    0x602A, 0x15D8,
    0x6F12, 0x0669,
    0x602A, 0x15DA,
    0x6F12, 0x026D,
    0x602A, 0x15DC,
    0x6F12, 0x0171,
    0x602A, 0x15DE,
    0x6F12, 0x0575,
    0x602A, 0x15E0,
    0x6F12, 0x0679,
    0x602A, 0x15E2,
    0x6F12, 0x027D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0101,
    0x0D02, 0x0101,
    0x0114, 0x0301,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0028,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xC040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0020,
    0x602A, 0x25D6,
    0x6F12, 0x0020,
    0x6028, 0x4000,
    0xF45A, 0x0015,
};

static kal_uint16 gm1st_custom2_setting[] = {
    0x6028,0x4000,
    0x6214,0x7971,
    0x6218,0x7150,
    0x0344,0x0008,
    0x0346,0x0008,
    0x0348,0x0FA7,
    0x034A,0x0BBF,
    0x034C,0x07D0,
    0x034E,0x05DC,
    0x0350,0x0000,
    0x0352,0x0000,
    0x0340,0x0C70,
    0x0342,0x09D0,
    0x0900,0x0121,
    0x0380,0x0001,
    0x0382,0x0003,
    0x0384,0x0001,
    0x0386,0x0001,
    0x0404,0x1000,
    0x0402,0x1020,
    0x0136,0x1800,
    0x0304,0x0006,
    0x030C,0x0000,
    0x0306,0x00F1,
    0x0302,0x0001,
    0x0300,0x0008,
    0x030E,0x0003,
    0x0312,0x0001,
    0x0310,0x0095,
    0x6028,0x2000,
    0x602A,0x1492,
    0x6F12,0x0078,
    0x602A,0x0E4E,
    0x6F12,0xFFFF,
    0x6028,0x4000,
    0x0118,0x0004,
    0x021E,0x0000,
    0x6028,0x2000,
    0x602A,0x2126,
    0x6F12,0x0000,
    0x602A,0x1168,
    0x6F12,0x0020,
    0x602A,0x2DB6,
    0x6F12,0x0001,
    0x602A,0x1668,
    0x6F12,0xFF00,
    0x602A,0x166A,
    0x6F12,0xFF00,
    0x602A,0x118A,
    0x6F12,0x0402,
    0x602A,0x151E,
    0x6F12,0x0001,
    0x602A,0x217E,
    0x6F12,0x0001,
    0x602A,0x1520,
    0x6F12,0x0100,
    0x602A,0x2522,
    0x6F12,0x0804,
    0x602A,0x2524,
    0x6F12,0x0400,
    0x602A,0x2568,
    0x6F12,0x5500,
    0x602A,0x2588,
    0x6F12,0x1111,
    0x602A,0x258C,
    0x6F12,0x1111,
    0x602A,0x25A6,
    0x6F12,0x0000,
    0x602A,0x252C,
    0x6F12,0x0601,
    0x602A,0x252E,
    0x6F12,0x0605,
    0x602A,0x25A8,
    0x6F12,0x1100,
    0x602A,0x25AC,
    0x6F12,0x0011,
    0x602A,0x25B0,
    0x6F12,0x1100,
    0x602A,0x25B4,
    0x6F12,0x0011,
    0x602A,0x15A4,
    0x6F12,0x0141,
    0x602A,0x15A6,
    0x6F12,0x0545,
    0x602A,0x15A8,
    0x6F12,0x0649,
    0x602A,0x15AA,
    0x6F12,0x024D,
    0x602A,0x15AC,
    0x6F12,0x0151,
    0x602A,0x15AE,
    0x6F12,0x0555,
    0x602A,0x15B0,
    0x6F12,0x0659,
    0x602A,0x15B2,
    0x6F12,0x025D,
    0x602A,0x15B4,
    0x6F12,0x0161,
    0x602A,0x15B6,
    0x6F12,0x0565,
    0x602A,0x15B8,
    0x6F12,0x0669,
    0x602A,0x15BA,
    0x6F12,0x026D,
    0x602A,0x15BC,
    0x6F12,0x0171,
    0x602A,0x15BE,
    0x6F12,0x0575,
    0x602A,0x15C0,
    0x6F12,0x0679,
    0x602A,0x15C2,
    0x6F12,0x027D,
    0x602A,0x15C4,
    0x6F12,0x0141,
    0x602A,0x15C6,
    0x6F12,0x0545,
    0x602A,0x15C8,
    0x6F12,0x0649,
    0x602A,0x15CA,
    0x6F12,0x024D,
    0x602A,0x15CC,
    0x6F12,0x0151,
    0x602A,0x15CE,
    0x6F12,0x0555,
    0x602A,0x15D0,
    0x6F12,0x0659,
    0x602A,0x15D2,
    0x6F12,0x025D,
    0x602A,0x15D4,
    0x6F12,0x0161,
    0x602A,0x15D6,
    0x6F12,0x0565,
    0x602A,0x15D8,
    0x6F12,0x0669,
    0x602A,0x15DA,
    0x6F12,0x026D,
    0x602A,0x15DC,
    0x6F12,0x0171,
    0x602A,0x15DE,
    0x6F12,0x0575,
    0x602A,0x15E0,
    0x6F12,0x0679,
    0x602A,0x15E2,
    0x6F12,0x027D,
    0x602A,0x1A50,
    0x6F12,0x0001,
    0x602A,0x1A54,
    0x6F12,0x0100,
    0x6028,0x4000,
    0x0D00,0x0101,
    0x0D02,0x0101,
    0x0114,0x0301,
    0xF486,0x0641,
    0xF488,0x0A45,
    0xF48A,0x0A49,
    0xF48C,0x064D,
    0xF48E,0x0651,
    0xF490,0x0A55,
    0xF492,0x0A59,
    0xF494,0x065D,
    0xF496,0x0661,
    0xF498,0x0A65,
    0xF49A,0x0A69,
    0xF49C,0x066D,
    0xF49E,0x0671,
    0xF4A0,0x0A75,
    0xF4A2,0x0A79,
    0xF4A4,0x067D,
    0xF4A6,0x0641,
    0xF4A8,0x0A45,
    0xF4AA,0x0A49,
    0xF4AC,0x064D,
    0xF4AE,0x0651,
    0xF4B0,0x0A55,
    0xF4B2,0x0A59,
    0xF4B4,0x065D,
    0xF4B6,0x0661,
    0xF4B8,0x0A65,
    0xF4BA,0x0A69,
    0xF4BC,0x066D,
    0xF4BE,0x0671,
    0xF4C0,0x0A75,
    0xF4C2,0x0A79,
    0xF4C4,0x067D,
    0x0202,0x0010,
    0x0226,0x0010,
    0x0204,0x0020,
    0xF45A,0x001A,
    0x0B06,0x0101,
    0x6028,0x2000,
    0x602A,0x107A,
    0x6F12,0x1D00,
    0x602A,0x1074,
    0x6F12,0x1D00,
    0x602A,0x0E7C,
    0x6F12,0x0000,
    0x602A,0x1120,
    0x6F12,0x0000,
    0x602A,0x1122,
    0x6F12,0x0028,
    0x602A,0x1128,
    0x6F12,0x0601,
    0x602A,0x1AC0,
    0x6F12,0x0200,
    0x602A,0x1AC2,
    0x6F12,0x0002,
    0x602A,0x1494,
    0x6F12,0x3D68,
    0x602A,0x1498,
    0x6F12,0xF10D,
    0x602A,0x1488,
    0x6F12,0x0F04,
    0x602A,0x148A,
    0x6F12,0x170B,
    0x602A,0x150E,
    0x6F12,0x40C2,
    0x602A,0x1510,
    0x6F12,0x80AF,
    0x602A,0x1512,
    0x6F12,0x00A0,
    0x602A,0x1486,
    0x6F12,0x1430,
    0x602A,0x1490,
    0x6F12,0x5009,
    0x602A,0x149E,
    0x6F12,0x01C4,
    0x602A,0x11CC,
    0x6F12,0x0008,
    0x602A,0x11CE,
    0x6F12,0x000B,
    0x602A,0x11D0,
    0x6F12,0x0006,
    0x602A,0x11DA,
    0x6F12,0x0012,
    0x602A,0x11E6,
    0x6F12,0x002A,
    0x602A,0x125E,
    0x6F12,0x0048,
    0x602A,0x11F4,
    0x6F12,0x0000,
    0x602A,0x11F8,
    0x6F12,0x0016,
    0x6028,0x4000,
    0xF444,0x05BF,
    0xF44A,0x0016,
    0xF44C,0x1414,
    0xF44E,0x0014,
    0xF458,0x0008,
    0xF46E,0xD040,
    0xF470,0x0008,
    0x6028,0x2000,
    0x602A,0x1CAA,
    0x6F12,0x0000,
    0x602A,0x1CAC,
    0x6F12,0x0000,
    0x602A,0x1CAE,
    0x6F12,0x0000,
    0x602A,0x1CB0,
    0x6F12,0x0000,
    0x602A,0x1CB2,
    0x6F12,0x0000,
    0x602A,0x1CB4,
    0x6F12,0x0000,
    0x602A,0x1CB6,
    0x6F12,0x0000,
    0x602A,0x1CB8,
    0x6F12,0x0000,
    0x602A,0x1CBA,
    0x6F12,0x0000,
    0x602A,0x1CBC,
    0x6F12,0x0000,
    0x602A,0x1CBE,
    0x6F12,0x0000,
    0x602A,0x1CC0,
    0x6F12,0x0000,
    0x602A,0x1CC2,
    0x6F12,0x0000,
    0x602A,0x1CC4,
    0x6F12,0x0000,
    0x602A,0x1CC6,
    0x6F12,0x0000,
    0x602A,0x1CC8,
    0x6F12,0x0000,
    0x602A,0x6000,
    0x6F12,0x000F,
    0x602A,0x6002,
    0x6F12,0xFFFF,
    0x602A,0x6004,
    0x6F12,0x0000,
    0x602A,0x6006,
    0x6F12,0x1000,
    0x602A,0x6008,
    0x6F12,0x1000,
    0x602A,0x600A,
    0x6F12,0x1000,
    0x602A,0x600C,
    0x6F12,0x1000,
    0x602A,0x600E,
    0x6F12,0x1000,
    0x602A,0x6010,
    0x6F12,0x1000,
    0x602A,0x6012,
    0x6F12,0x1000,
    0x602A,0x6014,
    0x6F12,0x1000,
    0x602A,0x6016,
    0x6F12,0x1000,
    0x602A,0x6018,
    0x6F12,0x1000,
    0x602A,0x601A,
    0x6F12,0x1000,
    0x602A,0x601C,
    0x6F12,0x1000,
    0x602A,0x601E,
    0x6F12,0x1000,
    0x602A,0x6020,
    0x6F12,0x1000,
    0x602A,0x6022,
    0x6F12,0x1000,
    0x602A,0x6024,
    0x6F12,0x1000,
    0x602A,0x6026,
    0x6F12,0x1000,
    0x602A,0x6028,
    0x6F12,0x1000,
    0x602A,0x602A,
    0x6F12,0x1000,
    0x602A,0x602C,
    0x6F12,0x1000,
    0x602A,0x1144,
    0x6F12,0x0100,
    0x602A,0x1146,
    0x6F12,0x1B00,
    0x602A,0x1080,
    0x6F12,0x0100,
    0x602A,0x1084,
    0x6F12,0x00C0,
    0x602A,0x108A,
    0x6F12,0x00C0,
    0x602A,0x1090,
    0x6F12,0x0001,
    0x602A,0x1092,
    0x6F12,0x0000,
    0x602A,0x1094,
    0x6F12,0xA32E,
    0x602A,0x602E,
    0x6F12,0x0000,
    0x602A,0x6038,
    0x6F12,0x0003,
    0x602A,0x603A,
    0x6F12,0x005F,
    0x602A,0x603C,
    0x6F12,0x0060,
    0x602A,0x603E,
    0x6F12,0x0061,
    0x602A,0x25D4,
    0x6F12,0x0020,
    0x602A,0x25D6,
    0x6F12,0x0020,
};

static kal_uint16 gm1st_custom5_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0008,
    0x0346, 0x00D0,
    0x0348, 0x0FA7,
    0x034A, 0x0AF7,
    0x034C, 0x0FA0,
    0x034E, 0x0A28,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x0C7A,
    0x0342, 0x13A0,
    0x0900, 0x0111,
    0x0380, 0x0001,
    0x0382, 0x0001,
    0x0384, 0x0001,
    0x0386, 0x0001,
    0x0404, 0x1000,
    0x0402, 0x1010,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F1,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x0090,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0x007A,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0100,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xF0F0,
    0x602A, 0x166A,
    0x6F12, 0xF0F0,
    0x602A, 0x118A,
    0x6F12, 0x0802,
    0x602A, 0x151E,
    0x6F12, 0x0001,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0008,
    0x602A, 0x2522,
    0x6F12, 0x0804,
    0x602A, 0x2524,
    0x6F12, 0x0400,
    0x602A, 0x2568,
    0x6F12, 0x5500,
    0x602A, 0x2588,
    0x6F12, 0x1111,
    0x602A, 0x258C,
    0x6F12, 0x1111,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0141,
    0x602A, 0x15A6,
    0x6F12, 0x0545,
    0x602A, 0x15A8,
    0x6F12, 0x0649,
    0x602A, 0x15AA,
    0x6F12, 0x024D,
    0x602A, 0x15AC,
    0x6F12, 0x0151,
    0x602A, 0x15AE,
    0x6F12, 0x0555,
    0x602A, 0x15B0,
    0x6F12, 0x0659,
    0x602A, 0x15B2,
    0x6F12, 0x025D,
    0x602A, 0x15B4,
    0x6F12, 0x0161,
    0x602A, 0x15B6,
    0x6F12, 0x0565,
    0x602A, 0x15B8,
    0x6F12, 0x0669,
    0x602A, 0x15BA,
    0x6F12, 0x026D,
    0x602A, 0x15BC,
    0x6F12, 0x0171,
    0x602A, 0x15BE,
    0x6F12, 0x0575,
    0x602A, 0x15C0,
    0x6F12, 0x0679,
    0x602A, 0x15C2,
    0x6F12, 0x027D,
    0x602A, 0x15C4,
    0x6F12, 0x0141,
    0x602A, 0x15C6,
    0x6F12, 0x0545,
    0x602A, 0x15C8,
    0x6F12, 0x0649,
    0x602A, 0x15CA,
    0x6F12, 0x024D,
    0x602A, 0x15CC,
    0x6F12, 0x0151,
    0x602A, 0x15CE,
    0x6F12, 0x0555,
    0x602A, 0x15D0,
    0x6F12, 0x0659,
    0x602A, 0x15D2,
    0x6F12, 0x025D,
    0x602A, 0x15D4,
    0x6F12, 0x0161,
    0x602A, 0x15D6,
    0x6F12, 0x0565,
    0x602A, 0x15D8,
    0x6F12, 0x0669,
    0x602A, 0x15DA,
    0x6F12, 0x026D,
    0x602A, 0x15DC,
    0x6F12, 0x0171,
    0x602A, 0x15DE,
    0x6F12, 0x0575,
    0x602A, 0x15E0,
    0x6F12, 0x0679,
    0x602A, 0x15E2,
    0x6F12, 0x027D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0101,
    0x0D02, 0x0101,
    0x0114, 0x0301,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0078,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xC040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0000,
    0x602A, 0x25D6,
    0x6F12, 0x0000,
    0x6028, 0x4000,
    0xF45A, 0x0015,
};

static kal_uint16 gm1st_normal_video_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0008,
    0x0346, 0x017C,
    0x0348, 0x0FA7,
    0x034A, 0x0A4B,
    0x034C, 0x0FA0,
    0x034E, 0x08D0,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x0C7A,
    0x0342, 0x13A0,
    0x0900, 0x0111,
    0x0380, 0x0001,
    0x0382, 0x0001,
    0x0384, 0x0001,
    0x0386, 0x0001,
    0x0404, 0x1000,
    0x0402, 0x1010,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F1,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x0090,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0x007A,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0100,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xF0F0,
    0x602A, 0x166A,
    0x6F12, 0xF0F0,
    0x602A, 0x118A,
    0x6F12, 0x0802,
    0x602A, 0x151E,
    0x6F12, 0x0001,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0008,
    0x602A, 0x2522,
    0x6F12, 0x0804,
    0x602A, 0x2524,
    0x6F12, 0x0400,
    0x602A, 0x2568,
    0x6F12, 0x5500,
    0x602A, 0x2588,
    0x6F12, 0x1111,
    0x602A, 0x258C,
    0x6F12, 0x1111,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0141,
    0x602A, 0x15A6,
    0x6F12, 0x0545,
    0x602A, 0x15A8,
    0x6F12, 0x0649,
    0x602A, 0x15AA,
    0x6F12, 0x024D,
    0x602A, 0x15AC,
    0x6F12, 0x0151,
    0x602A, 0x15AE,
    0x6F12, 0x0555,
    0x602A, 0x15B0,
    0x6F12, 0x0659,
    0x602A, 0x15B2,
    0x6F12, 0x025D,
    0x602A, 0x15B4,
    0x6F12, 0x0161,
    0x602A, 0x15B6,
    0x6F12, 0x0565,
    0x602A, 0x15B8,
    0x6F12, 0x0669,
    0x602A, 0x15BA,
    0x6F12, 0x026D,
    0x602A, 0x15BC,
    0x6F12, 0x0171,
    0x602A, 0x15BE,
    0x6F12, 0x0575,
    0x602A, 0x15C0,
    0x6F12, 0x0679,
    0x602A, 0x15C2,
    0x6F12, 0x027D,
    0x602A, 0x15C4,
    0x6F12, 0x0141,
    0x602A, 0x15C6,
    0x6F12, 0x0545,
    0x602A, 0x15C8,
    0x6F12, 0x0649,
    0x602A, 0x15CA,
    0x6F12, 0x024D,
    0x602A, 0x15CC,
    0x6F12, 0x0151,
    0x602A, 0x15CE,
    0x6F12, 0x0555,
    0x602A, 0x15D0,
    0x6F12, 0x0659,
    0x602A, 0x15D2,
    0x6F12, 0x025D,
    0x602A, 0x15D4,
    0x6F12, 0x0161,
    0x602A, 0x15D6,
    0x6F12, 0x0565,
    0x602A, 0x15D8,
    0x6F12, 0x0669,
    0x602A, 0x15DA,
    0x6F12, 0x026D,
    0x602A, 0x15DC,
    0x6F12, 0x0171,
    0x602A, 0x15DE,
    0x6F12, 0x0575,
    0x602A, 0x15E0,
    0x6F12, 0x0679,
    0x602A, 0x15E2,
    0x6F12, 0x027D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0101,
    0x0D02, 0x0101,
    0x0114, 0x0301,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0078,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xC040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0000,
    0x602A, 0x25D6,
    0x6F12, 0x0000,
    0x6028, 0x4000,
    0xF45A, 0x0015,
};

static kal_uint16 gm1st_hs_video_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0058,
    0x0346, 0x01AC,
    0x0348, 0x0F57,
    0x034A, 0x0A1B,
    0x034C, 0x0780,
    0x034E, 0x0438,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x063F,
    0x0342, 0x09D0,
    0x0900, 0x0122,
    0x0380, 0x0001,
    0x0382, 0x0003,
    0x0384, 0x0001,
    0x0386, 0x0003,
    0x0404, 0x1000,
    0x0402, 0x1010,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F1,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x0095,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0x0060,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0000,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xFF00,
    0x602A, 0x166A,
    0x6F12, 0xFF00,
    0x602A, 0x118A,
    0x6F12, 0x0402,
    0x602A, 0x151E,
    0x6F12, 0x0002,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0000,
    0x602A, 0x2522,
    0x6F12, 0x1004,
    0x602A, 0x2524,
    0x6F12, 0x0200,
    0x602A, 0x2568,
    0x6F12, 0x0000,
    0x602A, 0x2588,
    0x6F12, 0x0000,
    0x602A, 0x258C,
    0x6F12, 0x0000,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0641,
    0x602A, 0x15A6,
    0x6F12, 0x0145,
    0x602A, 0x15A8,
    0x6F12, 0x0149,
    0x602A, 0x15AA,
    0x6F12, 0x064D,
    0x602A, 0x15AC,
    0x6F12, 0x0651,
    0x602A, 0x15AE,
    0x6F12, 0x0155,
    0x602A, 0x15B0,
    0x6F12, 0x0159,
    0x602A, 0x15B2,
    0x6F12, 0x065D,
    0x602A, 0x15B4,
    0x6F12, 0x0661,
    0x602A, 0x15B6,
    0x6F12, 0x0165,
    0x602A, 0x15B8,
    0x6F12, 0x0169,
    0x602A, 0x15BA,
    0x6F12, 0x066D,
    0x602A, 0x15BC,
    0x6F12, 0x0671,
    0x602A, 0x15BE,
    0x6F12, 0x0175,
    0x602A, 0x15C0,
    0x6F12, 0x0179,
    0x602A, 0x15C2,
    0x6F12, 0x067D,
    0x602A, 0x15C4,
    0x6F12, 0x0641,
    0x602A, 0x15C6,
    0x6F12, 0x0145,
    0x602A, 0x15C8,
    0x6F12, 0x0149,
    0x602A, 0x15CA,
    0x6F12, 0x064D,
    0x602A, 0x15CC,
    0x6F12, 0x0651,
    0x602A, 0x15CE,
    0x6F12, 0x0155,
    0x602A, 0x15D0,
    0x6F12, 0x0159,
    0x602A, 0x15D2,
    0x6F12, 0x065D,
    0x602A, 0x15D4,
    0x6F12, 0x0661,
    0x602A, 0x15D6,
    0x6F12, 0x0165,
    0x602A, 0x15D8,
    0x6F12, 0x0169,
    0x602A, 0x15DA,
    0x6F12, 0x066D,
    0x602A, 0x15DC,
    0x6F12, 0x0671,
    0x602A, 0x15DE,
    0x6F12, 0x0175,
    0x602A, 0x15E0,
    0x6F12, 0x0179,
    0x602A, 0x15E2,
    0x6F12, 0x067D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0101,
    0x0D02, 0x0001,
    0x0114, 0x0300,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0078,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xD040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0000,
    0x602A, 0x25D6,
    0x6F12, 0x0000,
    0x6028, 0x4000,
    0xF45A, 0x001a,
};

static kal_uint16 gm1st_slim_video_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0008,
    0x0346, 0x017C,
    0x0348, 0x0FA7,
    0x034A, 0x0A4B,
    0x034C, 0x0FA0,
    0x034E, 0x08D0,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x0C7A,
    0x0342, 0x13A0,
    0x0900, 0x0111,
    0x0380, 0x0001,
    0x0382, 0x0001,
    0x0384, 0x0001,
    0x0386, 0x0001,
    0x0404, 0x1000,
    0x0402, 0x1010,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F1,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x0095,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0x0086,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0100,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xF0F0,
    0x602A, 0x166A,
    0x6F12, 0xF0F0,
    0x602A, 0x118A,
    0x6F12, 0x0802,
    0x602A, 0x151E,
    0x6F12, 0x0001,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0008,
    0x602A, 0x2522,
    0x6F12, 0x0804,
    0x602A, 0x2524,
    0x6F12, 0x0400,
    0x602A, 0x2568,
    0x6F12, 0x5500,
    0x602A, 0x2588,
    0x6F12, 0x1111,
    0x602A, 0x258C,
    0x6F12, 0x1111,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0141,
    0x602A, 0x15A6,
    0x6F12, 0x0545,
    0x602A, 0x15A8,
    0x6F12, 0x0649,
    0x602A, 0x15AA,
    0x6F12, 0x024D,
    0x602A, 0x15AC,
    0x6F12, 0x0151,
    0x602A, 0x15AE,
    0x6F12, 0x0555,
    0x602A, 0x15B0,
    0x6F12, 0x0659,
    0x602A, 0x15B2,
    0x6F12, 0x025D,
    0x602A, 0x15B4,
    0x6F12, 0x0161,
    0x602A, 0x15B6,
    0x6F12, 0x0565,
    0x602A, 0x15B8,
    0x6F12, 0x0669,
    0x602A, 0x15BA,
    0x6F12, 0x026D,
    0x602A, 0x15BC,
    0x6F12, 0x0171,
    0x602A, 0x15BE,
    0x6F12, 0x0575,
    0x602A, 0x15C0,
    0x6F12, 0x0679,
    0x602A, 0x15C2,
    0x6F12, 0x027D,
    0x602A, 0x15C4,
    0x6F12, 0x0141,
    0x602A, 0x15C6,
    0x6F12, 0x0545,
    0x602A, 0x15C8,
    0x6F12, 0x0649,
    0x602A, 0x15CA,
    0x6F12, 0x024D,
    0x602A, 0x15CC,
    0x6F12, 0x0151,
    0x602A, 0x15CE,
    0x6F12, 0x0555,
    0x602A, 0x15D0,
    0x6F12, 0x0659,
    0x602A, 0x15D2,
    0x6F12, 0x025D,
    0x602A, 0x15D4,
    0x6F12, 0x0161,
    0x602A, 0x15D6,
    0x6F12, 0x0565,
    0x602A, 0x15D8,
    0x6F12, 0x0669,
    0x602A, 0x15DA,
    0x6F12, 0x026D,
    0x602A, 0x15DC,
    0x6F12, 0x0171,
    0x602A, 0x15DE,
    0x6F12, 0x0575,
    0x602A, 0x15E0,
    0x6F12, 0x0679,
    0x602A, 0x15E2,
    0x6F12, 0x027D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0101,
    0x0D02, 0x0101,
    0x0114, 0x0301,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0078,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xC040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0000,
    0x602A, 0x25D6,
    0x6F12, 0x0000,
    0x6028, 0x4000,
    0xF45A, 0x0015,
};

static kal_uint16 gm1st_custom3_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0058,
    0x0346, 0x01AC,
    0x0348, 0x0F57,
    0x034A, 0x0A1B,
    0x034C, 0x0500,
    0x034E, 0x02D0,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x0330,
    0x0342, 0x09D0,
    0x0900, 0x0123,
    0x0380, 0x0001,
    0x0382, 0x0002,
    0x0384, 0x0001,
    0x0386, 0x0005,
    0x0404, 0x1000,
    0x0402, 0x1810,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F6,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x005B,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0xFFFF,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0000,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xF0F0,
    0x602A, 0x166A,
    0x6F12, 0xF0F0,
    0x602A, 0x118A,
    0x6F12, 0x0802,
    0x602A, 0x151E,
    0x6F12, 0x0000,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0000,
    0x602A, 0x2522,
    0x6F12, 0x0804,
    0x602A, 0x2524,
    0x6F12, 0x0400,
    0x602A, 0x2568,
    0x6F12, 0x5500,
    0x602A, 0x2588,
    0x6F12, 0x1111,
    0x602A, 0x258C,
    0x6F12, 0x1111,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0141,
    0x602A, 0x15A6,
    0x6F12, 0x0545,
    0x602A, 0x15A8,
    0x6F12, 0x0649,
    0x602A, 0x15AA,
    0x6F12, 0x024D,
    0x602A, 0x15AC,
    0x6F12, 0x0151,
    0x602A, 0x15AE,
    0x6F12, 0x0555,
    0x602A, 0x15B0,
    0x6F12, 0x0659,
    0x602A, 0x15B2,
    0x6F12, 0x025D,
    0x602A, 0x15B4,
    0x6F12, 0x0161,
    0x602A, 0x15B6,
    0x6F12, 0x0565,
    0x602A, 0x15B8,
    0x6F12, 0x0669,
    0x602A, 0x15BA,
    0x6F12, 0x026D,
    0x602A, 0x15BC,
    0x6F12, 0x0171,
    0x602A, 0x15BE,
    0x6F12, 0x0575,
    0x602A, 0x15C0,
    0x6F12, 0x0679,
    0x602A, 0x15C2,
    0x6F12, 0x027D,
    0x602A, 0x15C4,
    0x6F12, 0x0141,
    0x602A, 0x15C6,
    0x6F12, 0x0545,
    0x602A, 0x15C8,
    0x6F12, 0x0649,
    0x602A, 0x15CA,
    0x6F12, 0x024D,
    0x602A, 0x15CC,
    0x6F12, 0x0151,
    0x602A, 0x15CE,
    0x6F12, 0x0555,
    0x602A, 0x15D0,
    0x6F12, 0x0659,
    0x602A, 0x15D2,
    0x6F12, 0x025D,
    0x602A, 0x15D4,
    0x6F12, 0x0161,
    0x602A, 0x15D6,
    0x6F12, 0x0565,
    0x602A, 0x15D8,
    0x6F12, 0x0669,
    0x602A, 0x15DA,
    0x6F12, 0x026D,
    0x602A, 0x15DC,
    0x6F12, 0x0171,
    0x602A, 0x15DE,
    0x6F12, 0x0575,
    0x602A, 0x15E0,
    0x6F12, 0x0679,
    0x602A, 0x15E2,
    0x6F12, 0x027D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0100,
    0x0D02, 0x0001,
    0x0114, 0x0300,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0028,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xD040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0000,
    0x602A, 0x25D6,
    0x6F12, 0x0000,
    0x6028, 0x4000,
    0xF45A, 0x001a,
};

static kal_uint16 gm1st_custom4_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0058,
    0x0346, 0x01AC,
    0x0348, 0x0F57,
    0x034A, 0x0A1B,
    0x034C, 0x0500,
    0x034E, 0x02D0,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x0330,
    0x0342, 0x09D0,
    0x0900, 0x0123,
    0x0380, 0x0001,
    0x0382, 0x0002,
    0x0384, 0x0001,
    0x0386, 0x0005,
    0x0404, 0x1000,
    0x0402, 0x1810,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F6,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x005B,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0xFFFF,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0000,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xF0F0,
    0x602A, 0x166A,
    0x6F12, 0xF0F0,
    0x602A, 0x118A,
    0x6F12, 0x0802,
    0x602A, 0x151E,
    0x6F12, 0x0000,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0000,
    0x602A, 0x2522,
    0x6F12, 0x0804,
    0x602A, 0x2524,
    0x6F12, 0x0400,
    0x602A, 0x2568,
    0x6F12, 0x5500,
    0x602A, 0x2588,
    0x6F12, 0x1111,
    0x602A, 0x258C,
    0x6F12, 0x1111,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0141,
    0x602A, 0x15A6,
    0x6F12, 0x0545,
    0x602A, 0x15A8,
    0x6F12, 0x0649,
    0x602A, 0x15AA,
    0x6F12, 0x024D,
    0x602A, 0x15AC,
    0x6F12, 0x0151,
    0x602A, 0x15AE,
    0x6F12, 0x0555,
    0x602A, 0x15B0,
    0x6F12, 0x0659,
    0x602A, 0x15B2,
    0x6F12, 0x025D,
    0x602A, 0x15B4,
    0x6F12, 0x0161,
    0x602A, 0x15B6,
    0x6F12, 0x0565,
    0x602A, 0x15B8,
    0x6F12, 0x0669,
    0x602A, 0x15BA,
    0x6F12, 0x026D,
    0x602A, 0x15BC,
    0x6F12, 0x0171,
    0x602A, 0x15BE,
    0x6F12, 0x0575,
    0x602A, 0x15C0,
    0x6F12, 0x0679,
    0x602A, 0x15C2,
    0x6F12, 0x027D,
    0x602A, 0x15C4,
    0x6F12, 0x0141,
    0x602A, 0x15C6,
    0x6F12, 0x0545,
    0x602A, 0x15C8,
    0x6F12, 0x0649,
    0x602A, 0x15CA,
    0x6F12, 0x024D,
    0x602A, 0x15CC,
    0x6F12, 0x0151,
    0x602A, 0x15CE,
    0x6F12, 0x0555,
    0x602A, 0x15D0,
    0x6F12, 0x0659,
    0x602A, 0x15D2,
    0x6F12, 0x025D,
    0x602A, 0x15D4,
    0x6F12, 0x0161,
    0x602A, 0x15D6,
    0x6F12, 0x0565,
    0x602A, 0x15D8,
    0x6F12, 0x0669,
    0x602A, 0x15DA,
    0x6F12, 0x026D,
    0x602A, 0x15DC,
    0x6F12, 0x0171,
    0x602A, 0x15DE,
    0x6F12, 0x0575,
    0x602A, 0x15E0,
    0x6F12, 0x0679,
    0x602A, 0x15E2,
    0x6F12, 0x027D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0100,
    0x0D02, 0x0001,
    0x0114, 0x0300,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0028,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xD040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0000,
    0x602A, 0x25D6,
    0x6F12, 0x0000,
    0x6028, 0x4000,
    0xF45A, 0x001a,
};

static kal_uint16 gm1st_custom1_setting[] = {
    0x6028, 0x4000,
    0x6214, 0x7971,
    0x6218, 0x7150,
    0x0344, 0x0008,
    0x0346, 0x0008,
    0x0348, 0x0FA7,
    0x034A, 0x0BBF,
    0x034C, 0x0FA0,
    0x034E, 0x0BB8,
    0x0350, 0x0000,
    0x0352, 0x0000,
    0x0340, 0x0F98,
    0x0342, 0x13A0,
    0x0900, 0x0111,
    0x0380, 0x0001,
    0x0382, 0x0001,
    0x0384, 0x0001,
    0x0386, 0x0001,
    0x0404, 0x1000,
    0x0402, 0x1010,
    0x0136, 0x1800,
    0x0304, 0x0006,
    0x030C, 0x0000,
    0x0306, 0x00F1,
    0x0302, 0x0001,
    0x0300, 0x0008,
    0x030E, 0x0003,
    0x0312, 0x0001,
    0x0310, 0x0090,
    0x6028, 0x2000,
    0x602A, 0x1492,
    0x6F12, 0x0078,
    0x602A, 0x0E4E,
    0x6F12, 0x007A,
    0x6028, 0x4000,
    0x0118, 0x0004,
    0x021E, 0x0000,
    0x6028, 0x2000,
    0x602A, 0x2126,
    0x6F12, 0x0100,
    0x602A, 0x1168,
    0x6F12, 0x0020,
    0x602A, 0x2DB6,
    0x6F12, 0x0001,
    0x602A, 0x1668,
    0x6F12, 0xF0F0,
    0x602A, 0x166A,
    0x6F12, 0xF0F0,
    0x602A, 0x118A,
    0x6F12, 0x0802,
    0x602A, 0x151E,
    0x6F12, 0x0001,
    0x602A, 0x217E,
    0x6F12, 0x0001,
    0x602A, 0x1520,
    0x6F12, 0x0008,
    0x602A, 0x2522,
    0x6F12, 0x0804,
    0x602A, 0x2524,
    0x6F12, 0x0400,
    0x602A, 0x2568,
    0x6F12, 0x5500,
    0x602A, 0x2588,
    0x6F12, 0x1111,
    0x602A, 0x258C,
    0x6F12, 0x1111,
    0x602A, 0x25A6,
    0x6F12, 0x0000,
    0x602A, 0x252C,
    0x6F12, 0x0601,
    0x602A, 0x252E,
    0x6F12, 0x0605,
    0x602A, 0x25A8,
    0x6F12, 0x1100,
    0x602A, 0x25AC,
    0x6F12, 0x0011,
    0x602A, 0x25B0,
    0x6F12, 0x1100,
    0x602A, 0x25B4,
    0x6F12, 0x0011,
    0x602A, 0x15A4,
    0x6F12, 0x0141,
    0x602A, 0x15A6,
    0x6F12, 0x0545,
    0x602A, 0x15A8,
    0x6F12, 0x0649,
    0x602A, 0x15AA,
    0x6F12, 0x024D,
    0x602A, 0x15AC,
    0x6F12, 0x0151,
    0x602A, 0x15AE,
    0x6F12, 0x0555,
    0x602A, 0x15B0,
    0x6F12, 0x0659,
    0x602A, 0x15B2,
    0x6F12, 0x025D,
    0x602A, 0x15B4,
    0x6F12, 0x0161,
    0x602A, 0x15B6,
    0x6F12, 0x0565,
    0x602A, 0x15B8,
    0x6F12, 0x0669,
    0x602A, 0x15BA,
    0x6F12, 0x026D,
    0x602A, 0x15BC,
    0x6F12, 0x0171,
    0x602A, 0x15BE,
    0x6F12, 0x0575,
    0x602A, 0x15C0,
    0x6F12, 0x0679,
    0x602A, 0x15C2,
    0x6F12, 0x027D,
    0x602A, 0x15C4,
    0x6F12, 0x0141,
    0x602A, 0x15C6,
    0x6F12, 0x0545,
    0x602A, 0x15C8,
    0x6F12, 0x0649,
    0x602A, 0x15CA,
    0x6F12, 0x024D,
    0x602A, 0x15CC,
    0x6F12, 0x0151,
    0x602A, 0x15CE,
    0x6F12, 0x0555,
    0x602A, 0x15D0,
    0x6F12, 0x0659,
    0x602A, 0x15D2,
    0x6F12, 0x025D,
    0x602A, 0x15D4,
    0x6F12, 0x0161,
    0x602A, 0x15D6,
    0x6F12, 0x0565,
    0x602A, 0x15D8,
    0x6F12, 0x0669,
    0x602A, 0x15DA,
    0x6F12, 0x026D,
    0x602A, 0x15DC,
    0x6F12, 0x0171,
    0x602A, 0x15DE,
    0x6F12, 0x0575,
    0x602A, 0x15E0,
    0x6F12, 0x0679,
    0x602A, 0x15E2,
    0x6F12, 0x027D,
    0x602A, 0x1A50,
    0x6F12, 0x0001,
    0x602A, 0x1A54,
    0x6F12, 0x0100,
    0x6028, 0x4000,
    0x0D00, 0x0101,
    0x0D02, 0x0101,
    0x0114, 0x0301,
    0xF486, 0x0000,
    0xF488, 0x0000,
    0xF48A, 0x0000,
    0xF48C, 0x0000,
    0xF48E, 0x0000,
    0xF490, 0x0000,
    0xF492, 0x0000,
    0xF494, 0x0000,
    0xF496, 0x0000,
    0xF498, 0x0000,
    0xF49A, 0x0000,
    0xF49C, 0x0000,
    0xF49E, 0x0000,
    0xF4A0, 0x0000,
    0xF4A2, 0x0000,
    0xF4A4, 0x0000,
    0xF4A6, 0x0000,
    0xF4A8, 0x0000,
    0xF4AA, 0x0000,
    0xF4AC, 0x0000,
    0xF4AE, 0x0000,
    0xF4B0, 0x0000,
    0xF4B2, 0x0000,
    0xF4B4, 0x0000,
    0xF4B6, 0x0000,
    0xF4B8, 0x0000,
    0xF4BA, 0x0000,
    0xF4BC, 0x0000,
    0xF4BE, 0x0000,
    0xF4C0, 0x0000,
    0xF4C2, 0x0000,
    0xF4C4, 0x0000,
    0x0202, 0x0010,
    0x0226, 0x0010,
    0x0204, 0x0020,
    0x0B06, 0x0101,
    0x6028, 0x2000,
    0x602A, 0x107A,
    0x6F12, 0x1D00,
    0x602A, 0x1074,
    0x6F12, 0x1D00,
    0x602A, 0x0E7C,
    0x6F12, 0x0000,
    0x602A, 0x1120,
    0x6F12, 0x0200,
    0x602A, 0x1122,
    0x6F12, 0x0028,
    0x602A, 0x1128,
    0x6F12, 0x0604,
    0x602A, 0x1AC0,
    0x6F12, 0x0200,
    0x602A, 0x1AC2,
    0x6F12, 0x0002,
    0x602A, 0x1494,
    0x6F12, 0x3D68,
    0x602A, 0x1498,
    0x6F12, 0xF10D,
    0x602A, 0x1488,
    0x6F12, 0x0F04,
    0x602A, 0x148A,
    0x6F12, 0x170B,
    0x602A, 0x150E,
    0x6F12, 0x40C2,
    0x602A, 0x1510,
    0x6F12, 0x80AF,
    0x602A, 0x1512,
    0x6F12, 0x00A0,
    0x602A, 0x1486,
    0x6F12, 0x1430,
    0x602A, 0x1490,
    0x6F12, 0x5009,
    0x602A, 0x149E,
    0x6F12, 0x01C4,
    0x602A, 0x11CC,
    0x6F12, 0x0008,
    0x602A, 0x11CE,
    0x6F12, 0x000B,
    0x602A, 0x11D0,
    0x6F12, 0x0006,
    0x602A, 0x11DA,
    0x6F12, 0x0012,
    0x602A, 0x11E6,
    0x6F12, 0x002A,
    0x602A, 0x125E,
    0x6F12, 0x0048,
    0x602A, 0x11F4,
    0x6F12, 0x0000,
    0x602A, 0x11F8,
    0x6F12, 0x0016,
    0x6028, 0x4000,
    0xF444, 0x05BF,
    0xF44A, 0x0016,
    0xF44C, 0x1414,
    0xF44E, 0x0014,
    0xF458, 0x0008,
    0xF46E, 0xC040,
    0xF470, 0x0008,
    0x6028, 0x2000,
    0x602A, 0x1CAA,
    0x6F12, 0x0000,
    0x602A, 0x1CAC,
    0x6F12, 0x0000,
    0x602A, 0x1CAE,
    0x6F12, 0x0000,
    0x602A, 0x1CB0,
    0x6F12, 0x0000,
    0x602A, 0x1CB2,
    0x6F12, 0x0000,
    0x602A, 0x1CB4,
    0x6F12, 0x0000,
    0x602A, 0x1CB6,
    0x6F12, 0x0000,
    0x602A, 0x1CB8,
    0x6F12, 0x0000,
    0x602A, 0x1CBA,
    0x6F12, 0x0000,
    0x602A, 0x1CBC,
    0x6F12, 0x0000,
    0x602A, 0x1CBE,
    0x6F12, 0x0000,
    0x602A, 0x1CC0,
    0x6F12, 0x0000,
    0x602A, 0x1CC2,
    0x6F12, 0x0000,
    0x602A, 0x1CC4,
    0x6F12, 0x0000,
    0x602A, 0x1CC6,
    0x6F12, 0x0000,
    0x602A, 0x1CC8,
    0x6F12, 0x0000,
    0x602A, 0x6000,
    0x6F12, 0x000F,
    0x602A, 0x6002,
    0x6F12, 0xFFFF,
    0x602A, 0x6004,
    0x6F12, 0x0000,
    0x602A, 0x6006,
    0x6F12, 0x1000,
    0x602A, 0x6008,
    0x6F12, 0x1000,
    0x602A, 0x600A,
    0x6F12, 0x1000,
    0x602A, 0x600C,
    0x6F12, 0x1000,
    0x602A, 0x600E,
    0x6F12, 0x1000,
    0x602A, 0x6010,
    0x6F12, 0x1000,
    0x602A, 0x6012,
    0x6F12, 0x1000,
    0x602A, 0x6014,
    0x6F12, 0x1000,
    0x602A, 0x6016,
    0x6F12, 0x1000,
    0x602A, 0x6018,
    0x6F12, 0x1000,
    0x602A, 0x601A,
    0x6F12, 0x1000,
    0x602A, 0x601C,
    0x6F12, 0x1000,
    0x602A, 0x601E,
    0x6F12, 0x1000,
    0x602A, 0x6020,
    0x6F12, 0x1000,
    0x602A, 0x6022,
    0x6F12, 0x1000,
    0x602A, 0x6024,
    0x6F12, 0x1000,
    0x602A, 0x6026,
    0x6F12, 0x1000,
    0x602A, 0x6028,
    0x6F12, 0x1000,
    0x602A, 0x602A,
    0x6F12, 0x1000,
    0x602A, 0x602C,
    0x6F12, 0x1000,
    0x602A, 0x1144,
    0x6F12, 0x0100,
    0x602A, 0x1146,
    0x6F12, 0x1B00,
    0x602A, 0x1080,
    0x6F12, 0x0100,
    0x602A, 0x1084,
    0x6F12, 0x00C0,
    0x602A, 0x108A,
    0x6F12, 0x00C0,
    0x602A, 0x1090,
    0x6F12, 0x0001,
    0x602A, 0x1092,
    0x6F12, 0x0000,
    0x602A, 0x1094,
    0x6F12, 0xA32E,
    0x602A, 0x602E,
    0x6F12, 0x0000,
    0x602A, 0x6038,
    0x6F12, 0x0003,
    0x602A, 0x603A,
    0x6F12, 0x005F,
    0x602A, 0x603C,
    0x6F12, 0x0060,
    0x602A, 0x603E,
    0x6F12, 0x0061,
    0x602A, 0x25D4,
    0x6F12, 0x0020,
    0x602A, 0x25D6,
    0x6F12, 0x0020,
    0x6028, 0x4000,
    0xF45A, 0x0015,
};
static void sensor_init(void)
{
    LOG_INF("sensor_init start\n");
    write_cmos_sensor(0x6028, 0x4000);
    write_cmos_sensor(0x0000, 0x0002);
    write_cmos_sensor(0x0000, 0xF8D1);
    write_cmos_sensor(0x6010, 0x0001);
    mdelay(6);  //delay 6ms
    write_cmos_sensor(0x6214, 0x7971);
    write_cmos_sensor(0x6218, 0x7150);
    write_cmos_sensor(0x0A02, 0x0074);
    //Global
    write_cmos_sensor(0x6028, 0x2000);
    write_cmos_sensor(0x602A, 0x3F5C);
    iWriteRegI2C((u8*)uTnpArrayInit1 , (u16)sizeof(uTnpArrayInit1), imgsensor.i2c_write_id); // TNP burst
    write_cmos_sensor(0x602A, 0x46A0);
    iWriteRegI2C((u8*)uTnpArrayInit2 , (u16)sizeof(uTnpArrayInit2), imgsensor.i2c_write_id); // TNP burst
    /* Shipei.Chen@Cam.Drv, 20200520,  add for optimize mipi clk wave!*/
    write_cmos_sensor(0x6028, 0x4000);
    write_cmos_sensor(0x0b14, 0x0080);
    /*
    write_cmos_sensor(0x6028, 0x2000);
    write_cmos_sensor(0x602A, 0x3F5C);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x9EBB);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x2DE9);
    write_cmos_sensor(0x6F12, 0xFF5F);
    write_cmos_sensor(0x6F12, 0xFF48);
    write_cmos_sensor(0x6F12, 0x8B46);
    write_cmos_sensor(0x6F12, 0x1746);
    write_cmos_sensor(0x6F12, 0x0068);
    write_cmos_sensor(0x6F12, 0x9A46);
    write_cmos_sensor(0x6F12, 0x4FEA);
    write_cmos_sensor(0x6F12, 0x1049);
    write_cmos_sensor(0x6F12, 0x80B2);
    write_cmos_sensor(0x6F12, 0x8046);
    write_cmos_sensor(0x6F12, 0x0146);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0x4846);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x02FB);
    write_cmos_sensor(0x6F12, 0xF94D);
    write_cmos_sensor(0x6F12, 0x95F8);
    write_cmos_sensor(0x6F12, 0x6D00);
    write_cmos_sensor(0x6F12, 0x0228);
    write_cmos_sensor(0x6F12, 0x35D0);
    write_cmos_sensor(0x6F12, 0x0224);
    write_cmos_sensor(0x6F12, 0xF74E);
    write_cmos_sensor(0x6F12, 0x5346);
    write_cmos_sensor(0x6F12, 0xB6F8);
    write_cmos_sensor(0x6F12, 0xB802);
    write_cmos_sensor(0x6F12, 0xB0FB);
    write_cmos_sensor(0x6F12, 0xF4F0);
    write_cmos_sensor(0x6F12, 0xA6F8);
    write_cmos_sensor(0x6F12, 0xB802);
    write_cmos_sensor(0x6F12, 0xD5F8);
    write_cmos_sensor(0x6F12, 0x1411);
    write_cmos_sensor(0x6F12, 0x06F5);
    write_cmos_sensor(0x6F12, 0x2E76);
    write_cmos_sensor(0x6F12, 0x6143);
    write_cmos_sensor(0x6F12, 0xC5F8);
    write_cmos_sensor(0x6F12, 0x1411);
    write_cmos_sensor(0x6F12, 0xB5F8);
    write_cmos_sensor(0x6F12, 0x8C11);
    write_cmos_sensor(0x6F12, 0x411A);
    write_cmos_sensor(0x6F12, 0x89B2);
    write_cmos_sensor(0x6F12, 0x25F8);
    write_cmos_sensor(0x6F12, 0x981B);
    write_cmos_sensor(0x6F12, 0x35F8);
    write_cmos_sensor(0x6F12, 0x142C);
    write_cmos_sensor(0x6F12, 0x6243);
    write_cmos_sensor(0x6F12, 0x521E);
    write_cmos_sensor(0x6F12, 0x00FB);
    write_cmos_sensor(0x6F12, 0x0210);
    write_cmos_sensor(0x6F12, 0xB5F8);
    write_cmos_sensor(0x6F12, 0xF210);
    write_cmos_sensor(0x6F12, 0x07FB);
    write_cmos_sensor(0x6F12, 0x04F2);
    write_cmos_sensor(0x6F12, 0x0844);
    write_cmos_sensor(0x6F12, 0xC5F8);
    write_cmos_sensor(0x6F12, 0xF800);
    write_cmos_sensor(0x6F12, 0x5946);
    write_cmos_sensor(0x6F12, 0x0098);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xDBFA);
    write_cmos_sensor(0x6F12, 0x3088);
    write_cmos_sensor(0x6F12, 0x4146);
    write_cmos_sensor(0x6F12, 0x6043);
    write_cmos_sensor(0x6F12, 0x3080);
    write_cmos_sensor(0x6F12, 0xE86F);
    write_cmos_sensor(0x6F12, 0x0122);
    write_cmos_sensor(0x6F12, 0xB0FB);
    write_cmos_sensor(0x6F12, 0xF4F0);
    write_cmos_sensor(0x6F12, 0xE867);
    write_cmos_sensor(0x6F12, 0x04B0);
    write_cmos_sensor(0x6F12, 0x4846);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xF05F);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xC7BA);
    write_cmos_sensor(0x6F12, 0x0124);
    write_cmos_sensor(0x6F12, 0xC8E7);
    write_cmos_sensor(0x6F12, 0x2DE9);
    write_cmos_sensor(0x6F12, 0xF041);
    write_cmos_sensor(0x6F12, 0x8046);
    write_cmos_sensor(0x6F12, 0xD848);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0x4168);
    write_cmos_sensor(0x6F12, 0x0D0C);
    write_cmos_sensor(0x6F12, 0x8EB2);
    write_cmos_sensor(0x6F12, 0x3146);
    write_cmos_sensor(0x6F12, 0x2846);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xB9FA);
    write_cmos_sensor(0x6F12, 0xD74C);
    write_cmos_sensor(0x6F12, 0xD54F);
    write_cmos_sensor(0x6F12, 0x2078);
    write_cmos_sensor(0x6F12, 0x97F8);
    write_cmos_sensor(0x6F12, 0x8B12);
    write_cmos_sensor(0x6F12, 0x10FB);
    write_cmos_sensor(0x6F12, 0x01F0);
    write_cmos_sensor(0x6F12, 0x2070);
    write_cmos_sensor(0x6F12, 0x4046);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xB8FA);
    write_cmos_sensor(0x6F12, 0x2078);
    write_cmos_sensor(0x6F12, 0x97F8);
    write_cmos_sensor(0x6F12, 0x8B12);
    write_cmos_sensor(0x6F12, 0x0122);
    write_cmos_sensor(0x6F12, 0xB0FB);
    write_cmos_sensor(0x6F12, 0xF1F0);
    write_cmos_sensor(0x6F12, 0x2070);
    write_cmos_sensor(0x6F12, 0x3146);
    write_cmos_sensor(0x6F12, 0x2846);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xF041);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xA1BA);
    write_cmos_sensor(0x6F12, 0x2DE9);
    write_cmos_sensor(0x6F12, 0xFF47);
    write_cmos_sensor(0x6F12, 0x8146);
    write_cmos_sensor(0x6F12, 0xC648);
    write_cmos_sensor(0x6F12, 0x1746);
    write_cmos_sensor(0x6F12, 0x8846);
    write_cmos_sensor(0x6F12, 0x8068);
    write_cmos_sensor(0x6F12, 0x1C46);
    write_cmos_sensor(0x6F12, 0x85B2);
    write_cmos_sensor(0x6F12, 0x060C);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0x2946);
    write_cmos_sensor(0x6F12, 0x3046);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x92FA);
    write_cmos_sensor(0x6F12, 0x2346);
    write_cmos_sensor(0x6F12, 0x3A46);
    write_cmos_sensor(0x6F12, 0x4146);
    write_cmos_sensor(0x6F12, 0x4846);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x9BFA);
    write_cmos_sensor(0x6F12, 0xC14A);
    write_cmos_sensor(0x6F12, 0x9088);
    write_cmos_sensor(0x6F12, 0xF0B3);
    write_cmos_sensor(0x6F12, 0xBE48);
    write_cmos_sensor(0x6F12, 0x90F8);
    write_cmos_sensor(0x6F12, 0xBA10);
    write_cmos_sensor(0x6F12, 0xD1B3);
    write_cmos_sensor(0x6F12, 0xD0F8);
    write_cmos_sensor(0x6F12, 0x2801);
    write_cmos_sensor(0x6F12, 0x1168);
    write_cmos_sensor(0x6F12, 0x8842);
    write_cmos_sensor(0x6F12, 0x00D3);
    write_cmos_sensor(0x6F12, 0x0846);
    write_cmos_sensor(0x6F12, 0x010A);
    write_cmos_sensor(0x6F12, 0xB1FA);
    write_cmos_sensor(0x6F12, 0x81F0);
    write_cmos_sensor(0x6F12, 0xC0F1);
    write_cmos_sensor(0x6F12, 0x1700);
    write_cmos_sensor(0x6F12, 0xC140);
    write_cmos_sensor(0x6F12, 0x02EB);
    write_cmos_sensor(0x6F12, 0x4000);
    write_cmos_sensor(0x6F12, 0xC9B2);
    write_cmos_sensor(0x6F12, 0x0389);
    write_cmos_sensor(0x6F12, 0xC288);
    write_cmos_sensor(0x6F12, 0x9B1A);
    write_cmos_sensor(0x6F12, 0x4B43);
    write_cmos_sensor(0x6F12, 0x8033);
    write_cmos_sensor(0x6F12, 0x02EB);
    write_cmos_sensor(0x6F12, 0x2322);
    write_cmos_sensor(0x6F12, 0x0092);
    write_cmos_sensor(0x6F12, 0x438A);
    write_cmos_sensor(0x6F12, 0x028A);
    write_cmos_sensor(0x6F12, 0x9B1A);
    write_cmos_sensor(0x6F12, 0x4B43);
    write_cmos_sensor(0x6F12, 0x8033);
    write_cmos_sensor(0x6F12, 0x02EB);
    write_cmos_sensor(0x6F12, 0x2322);
    write_cmos_sensor(0x6F12, 0x0192);
    write_cmos_sensor(0x6F12, 0x838B);
    write_cmos_sensor(0x6F12, 0x428B);
    write_cmos_sensor(0x6F12, 0x9B1A);
    write_cmos_sensor(0x6F12, 0x4B43);
    write_cmos_sensor(0x6F12, 0x8033);
    write_cmos_sensor(0x6F12, 0x02EB);
    write_cmos_sensor(0x6F12, 0x2322);
    write_cmos_sensor(0x6F12, 0x0292);
    write_cmos_sensor(0x6F12, 0xC28C);
    write_cmos_sensor(0x6F12, 0x808C);
    write_cmos_sensor(0x6F12, 0x121A);
    write_cmos_sensor(0x6F12, 0x4A43);
    write_cmos_sensor(0x6F12, 0x8032);
    write_cmos_sensor(0x6F12, 0x00EB);
    write_cmos_sensor(0x6F12, 0x2220);
    write_cmos_sensor(0x6F12, 0x0390);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0x6846);
    write_cmos_sensor(0x6F12, 0x54F8);
    write_cmos_sensor(0x6F12, 0x2210);
    write_cmos_sensor(0x6F12, 0x50F8);
    write_cmos_sensor(0x6F12, 0x2230);
    write_cmos_sensor(0x6F12, 0x5943);
    write_cmos_sensor(0x6F12, 0x090B);
    write_cmos_sensor(0x6F12, 0x44F8);
    write_cmos_sensor(0x6F12, 0x2210);
    write_cmos_sensor(0x6F12, 0x521C);
    write_cmos_sensor(0x6F12, 0x00E0);
    write_cmos_sensor(0x6F12, 0x01E0);
    write_cmos_sensor(0x6F12, 0x042A);
    write_cmos_sensor(0x6F12, 0xF2D3);
    write_cmos_sensor(0x6F12, 0x04B0);
    write_cmos_sensor(0x6F12, 0x2946);
    write_cmos_sensor(0x6F12, 0x3046);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xF047);
    write_cmos_sensor(0x6F12, 0x0122);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x3FBA);
    write_cmos_sensor(0x6F12, 0x2DE9);
    write_cmos_sensor(0x6F12, 0xF041);
    write_cmos_sensor(0x6F12, 0x4FF4);
    write_cmos_sensor(0x6F12, 0x7A71);
    write_cmos_sensor(0x6F12, 0xB0FB);
    write_cmos_sensor(0x6F12, 0xF1F2);
    write_cmos_sensor(0x6F12, 0xB0FB);
    write_cmos_sensor(0x6F12, 0xF1F5);
    write_cmos_sensor(0x6F12, 0x01FB);
    write_cmos_sensor(0x6F12, 0x1207);
    write_cmos_sensor(0x6F12, 0x0024);
    write_cmos_sensor(0x6F12, 0x934E);
    write_cmos_sensor(0x6F12, 0x06E0);
    write_cmos_sensor(0x6F12, 0x48F2);
    write_cmos_sensor(0x6F12, 0xE801);
    write_cmos_sensor(0x6F12, 0x4843);
    write_cmos_sensor(0x6F12, 0x400B);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x40FA);
    write_cmos_sensor(0x6F12, 0x641C);
    write_cmos_sensor(0x6F12, 0x706B);
    write_cmos_sensor(0x6F12, 0xAC42);
    write_cmos_sensor(0x6F12, 0x4FEA);
    write_cmos_sensor(0x6F12, 0x9000);
    write_cmos_sensor(0x6F12, 0xF3D3);
    write_cmos_sensor(0x6F12, 0x7843);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xF041);
    write_cmos_sensor(0x6F12, 0x00EB);
    write_cmos_sensor(0x6F12, 0x4010);
    write_cmos_sensor(0x6F12, 0x400B);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x32BA);
    write_cmos_sensor(0x6F12, 0x70B5);
    write_cmos_sensor(0x6F12, 0x0024);
    write_cmos_sensor(0x6F12, 0x8A4D);
    write_cmos_sensor(0x6F12, 0x0CE0);
    write_cmos_sensor(0x6F12, 0xA0F5);
    write_cmos_sensor(0x6F12, 0x7F42);
    write_cmos_sensor(0x6F12, 0xFE3A);
    write_cmos_sensor(0x6F12, 0x13D0);
    write_cmos_sensor(0x6F12, 0x521E);
    write_cmos_sensor(0x6F12, 0x14D0);
    write_cmos_sensor(0x6F12, 0x91F8);
    write_cmos_sensor(0x6F12, 0x0E11);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x29FA);
    write_cmos_sensor(0x6F12, 0x641C);
    write_cmos_sensor(0x6F12, 0x142C);
    write_cmos_sensor(0x6F12, 0x05D2);
    write_cmos_sensor(0x6F12, 0x05EB);
    write_cmos_sensor(0x6F12, 0x8401);
    write_cmos_sensor(0x6F12, 0xB1F8);
    write_cmos_sensor(0x6F12, 0x0C01);
    write_cmos_sensor(0x6F12, 0x0028);
    write_cmos_sensor(0x6F12, 0xECD1);
    write_cmos_sensor(0x6F12, 0x7D49);
    write_cmos_sensor(0x6F12, 0x0420);
    write_cmos_sensor(0x6F12, 0xA1F8);
    write_cmos_sensor(0x6F12, 0xCA06);
    write_cmos_sensor(0x6F12, 0x70BD);
    write_cmos_sensor(0x6F12, 0x91F8);
    write_cmos_sensor(0x6F12, 0x0E01);
    write_cmos_sensor(0x6F12, 0x05E0);
    write_cmos_sensor(0x6F12, 0x91F8);
    write_cmos_sensor(0x6F12, 0x0E01);
    write_cmos_sensor(0x6F12, 0x4FF4);
    write_cmos_sensor(0x6F12, 0x7A71);
    write_cmos_sensor(0x6F12, 0x10FB);
    write_cmos_sensor(0x6F12, 0x01F0);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x15FA);
    write_cmos_sensor(0x6F12, 0xE5E7);
    write_cmos_sensor(0x6F12, 0x70B5);
    write_cmos_sensor(0x6F12, 0x0024);
    write_cmos_sensor(0x6F12, 0x764D);
    write_cmos_sensor(0x6F12, 0x0CE0);
    write_cmos_sensor(0x6F12, 0xA0F5);
    write_cmos_sensor(0x6F12, 0x7F42);
    write_cmos_sensor(0x6F12, 0xFE3A);
    write_cmos_sensor(0x6F12, 0x13D0);
    write_cmos_sensor(0x6F12, 0x521E);
    write_cmos_sensor(0x6F12, 0x14D0);
    write_cmos_sensor(0x6F12, 0x91F8);
    write_cmos_sensor(0x6F12, 0x5E11);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x01FA);
    write_cmos_sensor(0x6F12, 0x641C);
    write_cmos_sensor(0x6F12, 0x142C);
    write_cmos_sensor(0x6F12, 0x05D2);
    write_cmos_sensor(0x6F12, 0x05EB);
    write_cmos_sensor(0x6F12, 0x8401);
    write_cmos_sensor(0x6F12, 0xB1F8);
    write_cmos_sensor(0x6F12, 0x5C01);
    write_cmos_sensor(0x6F12, 0x0028);
    write_cmos_sensor(0x6F12, 0xECD1);
    write_cmos_sensor(0x6F12, 0x6949);
    write_cmos_sensor(0x6F12, 0x0220);
    write_cmos_sensor(0x6F12, 0xA1F8);
    write_cmos_sensor(0x6F12, 0xCA06);
    write_cmos_sensor(0x6F12, 0x70BD);
    write_cmos_sensor(0x6F12, 0x91F8);
    write_cmos_sensor(0x6F12, 0x5E01);
    write_cmos_sensor(0x6F12, 0x05E0);
    write_cmos_sensor(0x6F12, 0x91F8);
    write_cmos_sensor(0x6F12, 0x5E01);
    write_cmos_sensor(0x6F12, 0x4FF4);
    write_cmos_sensor(0x6F12, 0x7A71);
    write_cmos_sensor(0x6F12, 0x10FB);
    write_cmos_sensor(0x6F12, 0x01F0);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xEDF9);
    write_cmos_sensor(0x6F12, 0xE5E7);
    write_cmos_sensor(0x6F12, 0xF8B5);
    write_cmos_sensor(0x6F12, 0x604D);
    write_cmos_sensor(0x6F12, 0xB5F8);
    write_cmos_sensor(0x6F12, 0xCA06);
    write_cmos_sensor(0x6F12, 0x0328);
    write_cmos_sensor(0x6F12, 0x5BD1);
    write_cmos_sensor(0x6F12, 0x5F4E);
    write_cmos_sensor(0x6F12, 0x96F8);
    write_cmos_sensor(0x6F12, 0x3800);
    write_cmos_sensor(0x6F12, 0x90B1);
    write_cmos_sensor(0x6F12, 0x96F8);
    write_cmos_sensor(0x6F12, 0x3900);
    write_cmos_sensor(0x6F12, 0x0A28);
    write_cmos_sensor(0x6F12, 0x0ED8);
    write_cmos_sensor(0x6F12, 0x0024);
    write_cmos_sensor(0x6F12, 0x08E0);
    write_cmos_sensor(0x6F12, 0x06EB);
    write_cmos_sensor(0x6F12, 0x4400);
    write_cmos_sensor(0x6F12, 0x3219);
    write_cmos_sensor(0x6F12, 0x408F);
    write_cmos_sensor(0x6F12, 0x0121);
    write_cmos_sensor(0x6F12, 0x4E32);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xD9F9);
    write_cmos_sensor(0x6F12, 0x641C);
    write_cmos_sensor(0x6F12, 0x96F8);
    write_cmos_sensor(0x6F12, 0x3900);
    write_cmos_sensor(0x6F12, 0xA042);
    write_cmos_sensor(0x6F12, 0xF2D8);
    write_cmos_sensor(0x6F12, 0xD5F8);
    write_cmos_sensor(0x6F12, 0xDC06);
    write_cmos_sensor(0x6F12, 0x8047);
    write_cmos_sensor(0x6F12, 0x96F8);
    write_cmos_sensor(0x6F12, 0x2E00);
    write_cmos_sensor(0x6F12, 0x4FF4);
    write_cmos_sensor(0x6F12, 0x7A71);
    write_cmos_sensor(0x6F12, 0x10FB);
    write_cmos_sensor(0x6F12, 0x01F0);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xC4F9);
    write_cmos_sensor(0x6F12, 0x0220);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xCBF9);
    write_cmos_sensor(0x6F12, 0x0120);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xCDF9);
    write_cmos_sensor(0x6F12, 0x4D49);
    write_cmos_sensor(0x6F12, 0x0020);
    write_cmos_sensor(0x6F12, 0x4883);
    write_cmos_sensor(0x6F12, 0x4B4C);
    write_cmos_sensor(0x6F12, 0x94F8);
    write_cmos_sensor(0x6F12, 0xFB20);
    write_cmos_sensor(0x6F12, 0x4A83);
    write_cmos_sensor(0x6F12, 0x95F8);
    write_cmos_sensor(0x6F12, 0xAC10);
    write_cmos_sensor(0x6F12, 0xB1B1);
    write_cmos_sensor(0x6F12, 0x85F8);
    write_cmos_sensor(0x6F12, 0x4807);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xC4F9);
    write_cmos_sensor(0x6F12, 0x0646);
    write_cmos_sensor(0x6F12, 0x3046);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xC5F9);
    write_cmos_sensor(0x6F12, 0xD4F8);
    write_cmos_sensor(0x6F12, 0x6412);
    write_cmos_sensor(0x6F12, 0x8142);
    write_cmos_sensor(0x6F12, 0x01D2);
    write_cmos_sensor(0x6F12, 0x0121);
    write_cmos_sensor(0x6F12, 0x00E0);
    write_cmos_sensor(0x6F12, 0x0021);
    write_cmos_sensor(0x6F12, 0x95F8);
    write_cmos_sensor(0x6F12, 0x4807);
    write_cmos_sensor(0x6F12, 0x8DF8);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x9DF8);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0843);
    write_cmos_sensor(0x6F12, 0xEDD0);
    write_cmos_sensor(0x6F12, 0x95F8);
    write_cmos_sensor(0x6F12, 0x9806);
    write_cmos_sensor(0x6F12, 0x0028);
    write_cmos_sensor(0x6F12, 0x0ED0);
    write_cmos_sensor(0x6F12, 0x0122);
    write_cmos_sensor(0x6F12, 0x1402);
    write_cmos_sensor(0x6F12, 0x48F6);
    write_cmos_sensor(0x6F12, 0xF825);
    write_cmos_sensor(0x6F12, 0x2146);
    write_cmos_sensor(0x6F12, 0x2846);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xAFF9);
    write_cmos_sensor(0x6F12, 0x2146);
    write_cmos_sensor(0x6F12, 0x2846);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xF840);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xA8B9);
    write_cmos_sensor(0x6F12, 0xF8BD);
    write_cmos_sensor(0x6F12, 0x2DE9);
    write_cmos_sensor(0x6F12, 0xF041);
    write_cmos_sensor(0x6F12, 0x344C);
    write_cmos_sensor(0x6F12, 0x3249);
    write_cmos_sensor(0x6F12, 0x0646);
    write_cmos_sensor(0x6F12, 0x94F8);
    write_cmos_sensor(0x6F12, 0x6970);
    write_cmos_sensor(0x6F12, 0x8988);
    write_cmos_sensor(0x6F12, 0x94F8);
    write_cmos_sensor(0x6F12, 0x8120);
    write_cmos_sensor(0x6F12, 0x0020);
    write_cmos_sensor(0x6F12, 0xC1B1);
    write_cmos_sensor(0x6F12, 0x2146);
    write_cmos_sensor(0x6F12, 0xD1F8);
    write_cmos_sensor(0x6F12, 0x9410);
    write_cmos_sensor(0x6F12, 0x72B1);
    write_cmos_sensor(0x6F12, 0x8FB1);
    write_cmos_sensor(0x6F12, 0x0846);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x98F9);
    write_cmos_sensor(0x6F12, 0x0546);
    write_cmos_sensor(0x6F12, 0xE06F);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x94F9);
    write_cmos_sensor(0x6F12, 0x8542);
    write_cmos_sensor(0x6F12, 0x02D2);
    write_cmos_sensor(0x6F12, 0xD4F8);
    write_cmos_sensor(0x6F12, 0x9400);
    write_cmos_sensor(0x6F12, 0x26E0);
    write_cmos_sensor(0x6F12, 0xE06F);
    write_cmos_sensor(0x6F12, 0x24E0);
    write_cmos_sensor(0x6F12, 0x002F);
    write_cmos_sensor(0x6F12, 0xFBD1);
    write_cmos_sensor(0x6F12, 0x002A);
    write_cmos_sensor(0x6F12, 0x24D0);
    write_cmos_sensor(0x6F12, 0x0846);
    write_cmos_sensor(0x6F12, 0x1EE0);
    write_cmos_sensor(0x6F12, 0x1E49);
    write_cmos_sensor(0x6F12, 0x0D8E);
    write_cmos_sensor(0x6F12, 0x496B);
    write_cmos_sensor(0x6F12, 0x4B42);
    write_cmos_sensor(0x6F12, 0x77B1);
    write_cmos_sensor(0x6F12, 0x2048);
    write_cmos_sensor(0x6F12, 0x806F);
    write_cmos_sensor(0x6F12, 0x10E0);
    write_cmos_sensor(0x6F12, 0x4242);
    write_cmos_sensor(0x6F12, 0x00E0);
    write_cmos_sensor(0x6F12, 0x0246);
    write_cmos_sensor(0x6F12, 0x0029);
    write_cmos_sensor(0x6F12, 0x0FDB);
    write_cmos_sensor(0x6F12, 0x8A42);
    write_cmos_sensor(0x6F12, 0x0FDD);
    write_cmos_sensor(0x6F12, 0x3046);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xF041);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x78B9);
    write_cmos_sensor(0x6F12, 0x002A);
    write_cmos_sensor(0x6F12, 0x0CD0);
    write_cmos_sensor(0x6F12, 0x1748);
    write_cmos_sensor(0x6F12, 0xD0F8);
    write_cmos_sensor(0x6F12, 0x8C00);
    write_cmos_sensor(0x6F12, 0x25B1);
    write_cmos_sensor(0x6F12, 0x0028);
    write_cmos_sensor(0x6F12, 0xEDDA);
    write_cmos_sensor(0x6F12, 0xEAE7);
    write_cmos_sensor(0x6F12, 0x1946);
    write_cmos_sensor(0x6F12, 0xEDE7);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x70F9);
    write_cmos_sensor(0x6F12, 0xE060);
    write_cmos_sensor(0x6F12, 0x0120);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xF081);
    write_cmos_sensor(0x6F12, 0x2DE9);
    write_cmos_sensor(0x6F12, 0xF35F);
    write_cmos_sensor(0x6F12, 0xDFF8);
    write_cmos_sensor(0x6F12, 0x24A0);
    write_cmos_sensor(0x6F12, 0x0C46);
    write_cmos_sensor(0x6F12, 0xBAF8);
    write_cmos_sensor(0x6F12, 0xBE04);
    write_cmos_sensor(0x6F12, 0x08B1);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x67F9);
    write_cmos_sensor(0x6F12, 0x0B4E);
    write_cmos_sensor(0x6F12, 0x3088);
    write_cmos_sensor(0x6F12, 0x0128);
    write_cmos_sensor(0x6F12, 0x19D1);
    write_cmos_sensor(0x6F12, 0x002C);
    write_cmos_sensor(0x6F12, 0x17D1);
    write_cmos_sensor(0x6F12, 0x11E0);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x4690);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x2C30);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x2E30);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x2580);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x6000);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x0DE0);
    write_cmos_sensor(0x6F12, 0x4000);
    write_cmos_sensor(0x6F12, 0x7000);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x2BA0);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x3600);
    write_cmos_sensor(0x6F12, 0x6F4D);
    write_cmos_sensor(0x6F12, 0x2889);
    write_cmos_sensor(0x6F12, 0x18B1);
    write_cmos_sensor(0x6F12, 0x401E);
    write_cmos_sensor(0x6F12, 0x2881);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xFC9F);
    write_cmos_sensor(0x6F12, 0xDFF8);
    write_cmos_sensor(0x6F12, 0xB491);
    write_cmos_sensor(0x6F12, 0xD9F8);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0xB0F8);
    write_cmos_sensor(0x6F12, 0xD602);
    write_cmos_sensor(0x6F12, 0x38B1);
    write_cmos_sensor(0x6F12, 0x3089);
    write_cmos_sensor(0x6F12, 0x401C);
    write_cmos_sensor(0x6F12, 0x80B2);
    write_cmos_sensor(0x6F12, 0x3081);
    write_cmos_sensor(0x6F12, 0xFF28);
    write_cmos_sensor(0x6F12, 0x01D9);
    write_cmos_sensor(0x6F12, 0xE889);
    write_cmos_sensor(0x6F12, 0x3081);
    write_cmos_sensor(0x6F12, 0x6648);
    write_cmos_sensor(0x6F12, 0x4FF0);
    write_cmos_sensor(0x6F12, 0x0008);
    write_cmos_sensor(0x6F12, 0xC6F8);
    write_cmos_sensor(0x6F12, 0x0C80);
    write_cmos_sensor(0x6F12, 0xB0F8);
    write_cmos_sensor(0x6F12, 0x5EB0);
    write_cmos_sensor(0x6F12, 0x40F2);
    write_cmos_sensor(0x6F12, 0xFF31);
    write_cmos_sensor(0x6F12, 0x0B20);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x31F9);
    write_cmos_sensor(0x6F12, 0xD9F8);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0027);
    write_cmos_sensor(0x6F12, 0x3C46);
    write_cmos_sensor(0x6F12, 0xB0F8);
    write_cmos_sensor(0x6F12, 0xD412);
    write_cmos_sensor(0x6F12, 0x21B1);
    write_cmos_sensor(0x6F12, 0x0098);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x18F9);
    write_cmos_sensor(0x6F12, 0x0746);
    write_cmos_sensor(0x6F12, 0x0BE0);
    write_cmos_sensor(0x6F12, 0xB0F8);
    write_cmos_sensor(0x6F12, 0xD602);
    write_cmos_sensor(0x6F12, 0x40B1);
    write_cmos_sensor(0x6F12, 0x3089);
    write_cmos_sensor(0x6F12, 0xE989);
    write_cmos_sensor(0x6F12, 0x8842);
    write_cmos_sensor(0x6F12, 0x04D3);
    write_cmos_sensor(0x6F12, 0x0098);
    write_cmos_sensor(0x6F12, 0xFFF7);
    write_cmos_sensor(0x6F12, 0x5BFF);
    write_cmos_sensor(0x6F12, 0x0746);
    write_cmos_sensor(0x6F12, 0x0124);
    write_cmos_sensor(0x6F12, 0x3846);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x1BF9);
    write_cmos_sensor(0x6F12, 0xD9F8);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0xB0F8);
    write_cmos_sensor(0x6F12, 0xD602);
    write_cmos_sensor(0x6F12, 0x08B9);
    write_cmos_sensor(0x6F12, 0xA6F8);
    write_cmos_sensor(0x6F12, 0x0280);
    write_cmos_sensor(0x6F12, 0xC7B3);
    write_cmos_sensor(0x6F12, 0x4746);
    write_cmos_sensor(0x6F12, 0xA6F8);
    write_cmos_sensor(0x6F12, 0x0880);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x13F9);
    write_cmos_sensor(0x6F12, 0xF068);
    write_cmos_sensor(0x6F12, 0x3061);
    write_cmos_sensor(0x6F12, 0x688D);
    write_cmos_sensor(0x6F12, 0x50B3);
    write_cmos_sensor(0x6F12, 0xA88D);
    write_cmos_sensor(0x6F12, 0x50BB);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x10F9);
    write_cmos_sensor(0x6F12, 0xA889);
    write_cmos_sensor(0x6F12, 0x20B3);
    write_cmos_sensor(0x6F12, 0x1CB3);
    write_cmos_sensor(0x6F12, 0x706B);
    write_cmos_sensor(0x6F12, 0xAA88);
    write_cmos_sensor(0x6F12, 0xDAF8);
    write_cmos_sensor(0x6F12, 0x0815);
    write_cmos_sensor(0x6F12, 0xCAB1);
    write_cmos_sensor(0x6F12, 0x8842);
    write_cmos_sensor(0x6F12, 0x0CDB);
    write_cmos_sensor(0x6F12, 0x90FB);
    write_cmos_sensor(0x6F12, 0xF1F3);
    write_cmos_sensor(0x6F12, 0x90FB);
    write_cmos_sensor(0x6F12, 0xF1F2);
    write_cmos_sensor(0x6F12, 0x01FB);
    write_cmos_sensor(0x6F12, 0x1303);
    write_cmos_sensor(0x6F12, 0xB3EB);
    write_cmos_sensor(0x6F12, 0x610F);
    write_cmos_sensor(0x6F12, 0x00DD);
    write_cmos_sensor(0x6F12, 0x521C);
    write_cmos_sensor(0x6F12, 0x01FB);
    write_cmos_sensor(0x6F12, 0x1200);
    write_cmos_sensor(0x6F12, 0x0BE0);
    write_cmos_sensor(0x6F12, 0x91FB);
    write_cmos_sensor(0x6F12, 0xF0F3);
    write_cmos_sensor(0x6F12, 0x91FB);
    write_cmos_sensor(0x6F12, 0xF0F2);
    write_cmos_sensor(0x6F12, 0x00FB);
    write_cmos_sensor(0x6F12, 0x1313);
    write_cmos_sensor(0x6F12, 0xB3EB);
    write_cmos_sensor(0x6F12, 0x600F);
    write_cmos_sensor(0x6F12, 0x00DD);
    write_cmos_sensor(0x6F12, 0x521C);
    write_cmos_sensor(0x6F12, 0x5043);
    write_cmos_sensor(0x6F12, 0x401A);
    write_cmos_sensor(0x6F12, 0xF168);
    write_cmos_sensor(0x6F12, 0x01EB);
    write_cmos_sensor(0x6F12, 0x4000);
    write_cmos_sensor(0x6F12, 0xF060);
    write_cmos_sensor(0x6F12, 0xA88D);
    write_cmos_sensor(0x6F12, 0x10B1);
    write_cmos_sensor(0x6F12, 0xF089);
    write_cmos_sensor(0x6F12, 0x3087);
    write_cmos_sensor(0x6F12, 0xAF85);
    write_cmos_sensor(0x6F12, 0x5846);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0xFC5F);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xE4B8);
    write_cmos_sensor(0x6F12, 0x70B5);
    write_cmos_sensor(0x6F12, 0x3049);
    write_cmos_sensor(0x6F12, 0x0446);
    write_cmos_sensor(0x6F12, 0x0020);
    write_cmos_sensor(0x6F12, 0xC1F8);
    write_cmos_sensor(0x6F12, 0x3005);
    write_cmos_sensor(0x6F12, 0x2F48);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xC168);
    write_cmos_sensor(0x6F12, 0x0D0C);
    write_cmos_sensor(0x6F12, 0x8EB2);
    write_cmos_sensor(0x6F12, 0x3146);
    write_cmos_sensor(0x6F12, 0x2846);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x6CF8);
    write_cmos_sensor(0x6F12, 0x2046);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xD7F8);
    write_cmos_sensor(0x6F12, 0x3146);
    write_cmos_sensor(0x6F12, 0x2846);
    write_cmos_sensor(0x6F12, 0xBDE8);
    write_cmos_sensor(0x6F12, 0x7040);
    write_cmos_sensor(0x6F12, 0x0122);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x62B8);
    write_cmos_sensor(0x6F12, 0x10B5);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0x6751);
    write_cmos_sensor(0x6F12, 0x2448);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xCEF8);
    write_cmos_sensor(0x6F12, 0x224C);
    write_cmos_sensor(0x6F12, 0x0122);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0xD941);
    write_cmos_sensor(0x6F12, 0x2060);
    write_cmos_sensor(0x6F12, 0x2148);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xC6F8);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0xA141);
    write_cmos_sensor(0x6F12, 0x6060);
    write_cmos_sensor(0x6F12, 0x1F48);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xBFF8);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0xE931);
    write_cmos_sensor(0x6F12, 0xA060);
    write_cmos_sensor(0x6F12, 0x1C48);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xB8F8);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0xB731);
    write_cmos_sensor(0x6F12, 0x1A48);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xB2F8);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0x7331);
    write_cmos_sensor(0x6F12, 0x1848);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xACF8);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0x2F31);
    write_cmos_sensor(0x6F12, 0x1648);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xA6F8);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0x7521);
    write_cmos_sensor(0x6F12, 0x1448);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0xA0F8);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0xED11);
    write_cmos_sensor(0x6F12, 0x1248);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x9AF8);
    write_cmos_sensor(0x6F12, 0x0022);
    write_cmos_sensor(0x6F12, 0xAFF2);
    write_cmos_sensor(0x6F12, 0xAD01);
    write_cmos_sensor(0x6F12, 0x1048);
    write_cmos_sensor(0x6F12, 0x00F0);
    write_cmos_sensor(0x6F12, 0x94F8);
    write_cmos_sensor(0x6F12, 0xE060);
    write_cmos_sensor(0x6F12, 0x10BD);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x2BA0);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x0890);
    write_cmos_sensor(0x6F12, 0x4000);
    write_cmos_sensor(0x6F12, 0x7000);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x2E30);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x4690);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x24A7);
    write_cmos_sensor(0x6F12, 0x0001);
    write_cmos_sensor(0x6F12, 0x1AF3);
    write_cmos_sensor(0x6F12, 0x0001);
    write_cmos_sensor(0x6F12, 0x09BD);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0xA943);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x71F1);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x7239);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x5D87);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x576B);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x57ED);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0xBF8D);
    write_cmos_sensor(0x6F12, 0x4AF6);
    write_cmos_sensor(0x6F12, 0x293C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x42F2);
    write_cmos_sensor(0x6F12, 0xA74C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x41F6);
    write_cmos_sensor(0x6F12, 0xF32C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x010C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x40F6);
    write_cmos_sensor(0x6F12, 0xBD1C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x010C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x4AF6);
    write_cmos_sensor(0x6F12, 0x2D1C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x48F2);
    write_cmos_sensor(0x6F12, 0x0B3C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x4AF6);
    write_cmos_sensor(0x6F12, 0x431C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x48F2);
    write_cmos_sensor(0x6F12, 0x6F2C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x47F6);
    write_cmos_sensor(0x6F12, 0xA57C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x45F6);
    write_cmos_sensor(0x6F12, 0x815C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x4AF6);
    write_cmos_sensor(0x6F12, 0xE70C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x4AF6);
    write_cmos_sensor(0x6F12, 0x171C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x4AF6);
    write_cmos_sensor(0x6F12, 0x453C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x4AF6);
    write_cmos_sensor(0x6F12, 0x532C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x45F2);
    write_cmos_sensor(0x6F12, 0x377C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x45F2);
    write_cmos_sensor(0x6F12, 0xD56C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x45F2);
    write_cmos_sensor(0x6F12, 0xC91C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x40F2);
    write_cmos_sensor(0x6F12, 0xAB2C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x44F6);
    write_cmos_sensor(0x6F12, 0x897C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x45F2);
    write_cmos_sensor(0x6F12, 0xA56C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x45F2);
    write_cmos_sensor(0x6F12, 0xEF6C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x40F2);
    write_cmos_sensor(0x6F12, 0x6D7C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x4BF6);
    write_cmos_sensor(0x6F12, 0x8D7C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x6F12, 0x4BF2);
    write_cmos_sensor(0x6F12, 0xAB4C);
    write_cmos_sensor(0x6F12, 0xC0F2);
    write_cmos_sensor(0x6F12, 0x000C);
    write_cmos_sensor(0x6F12, 0x6047);
    write_cmos_sensor(0x602A, 0x46A0);
    write_cmos_sensor(0x6F12, 0x0549);
    write_cmos_sensor(0x6F12, 0x0448);
    write_cmos_sensor(0x6F12, 0x054A);
    write_cmos_sensor(0x6F12, 0xC1F8);
    write_cmos_sensor(0x6F12, 0x5005);
    write_cmos_sensor(0x6F12, 0x101A);
    write_cmos_sensor(0x6F12, 0xA1F8);
    write_cmos_sensor(0x6F12, 0x5405);
    write_cmos_sensor(0x6F12, 0xFFF7);
    write_cmos_sensor(0x6F12, 0x0EBF);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x46D8);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x2E30);
    write_cmos_sensor(0x6F12, 0x2000);
    write_cmos_sensor(0x6F12, 0x6000);
    write_cmos_sensor(0x6F12, 0x7047);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x08D1);
    write_cmos_sensor(0x6F12, 0x00A6);
    write_cmos_sensor(0x6F12, 0x0000);
    write_cmos_sensor(0x6F12, 0x00FF);
    */
    set_mirror_flip(imgsensor.mirror);
    LOG_INF("sensor_init End\n");
}   /*    sensor_init  */

static void preview_setting(void)
{
    LOG_INF("preview_setting Start\n");
    table_write_cmos_sensor(gm1st_preview_setting,
        sizeof(gm1st_preview_setting)/sizeof(kal_uint16));
    LOG_INF("preview_setting End\n");
} /* preview_setting */


/*full size 30fps*/
static void capture_setting(kal_uint16 currefps)
{
    LOG_INF("%s 30 fps E! currefps:%d\n", __func__, currefps);
    //Deng.Cao@Cam.Drv, change the capture setting to preview setting, 20191220 start.
    if (1) {
        preview_setting();
    } else {
        table_write_cmos_sensor(gm1st_capture_setting,
            sizeof(gm1st_capture_setting)/sizeof(kal_uint16));
    }
    //Deng.Cao@Cam.Drv, change the capture setting to preview setting, 20191220 end.
    LOG_INF("%s 30 fpsX\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
    LOG_INF("%s E! currefps:%d\n", __func__, currefps);
    if (1) {
        table_write_cmos_sensor(gm1st_slim_video_setting,
            sizeof(gm1st_slim_video_setting)/sizeof(kal_uint16));
    } else {
        table_write_cmos_sensor(gm1st_normal_video_setting,
            sizeof(gm1st_normal_video_setting)/sizeof(kal_uint16));
    }
    LOG_INF("X\n");
}

static void hs_video_setting(void)
{
    LOG_INF("%s E! currefps 120\n", __func__);
    table_write_cmos_sensor(gm1st_hs_video_setting,
        sizeof(gm1st_hs_video_setting)/sizeof(kal_uint16));
    LOG_INF("X\n");
}

static void slim_video_setting(void)
{
    LOG_INF("%s E! 4608*2592@30fps\n", __func__);
    table_write_cmos_sensor(gm1st_slim_video_setting,
        sizeof(gm1st_slim_video_setting)/sizeof(kal_uint16));
    LOG_INF("X\n");
}


/*full size 10fps*/
static void custom3_setting(void)
{
    LOG_INF("%s 7.5 fps E!\n", __func__);
    table_write_cmos_sensor(gm1st_custom3_setting,
        sizeof(gm1st_custom3_setting)/sizeof(kal_uint16));
    LOG_INF("%s 7.5 fpsX\n", __func__);
}

static void custom4_setting(void)
{
    LOG_INF("%s E!\n", __func__);
    table_write_cmos_sensor(gm1st_custom4_setting,
        sizeof(gm1st_custom4_setting)/sizeof(kal_uint16));
    LOG_INF("%s X\n", __func__);
}

static void custom1_setting(void)
{
    LOG_INF("%s CUS1_16M_24_FPS E! currefps\n", __func__);
    /*************MIPI output setting************/
    table_write_cmos_sensor(gm1st_custom1_setting,
        sizeof(gm1st_custom1_setting)/sizeof(kal_uint16));
    LOG_INF("X");
}

static void custom2_setting(void)
{
    LOG_INF("%s 4608*2592@60fps E! currefps\n", __func__);
    /*************MIPI output setting************/
    table_write_cmos_sensor(gm1st_custom2_setting,
        sizeof(gm1st_custom2_setting)/sizeof(kal_uint16));
    LOG_INF("X");
}

static void custom5_setting(void)
{
    LOG_INF("%s 4608*2592@60fps E! currefps\n", __func__);
    /*************MIPI output setting************/
    table_write_cmos_sensor(gm1st_custom5_setting,
        sizeof(gm1st_custom5_setting)/sizeof(kal_uint16));
    LOG_INF("X");
}

/*************************************************************************
 * FUNCTION
 *  get_imgsensor_id
 *
 * DESCRIPTION
 *  This function get the sensor ID
 *
 * PARAMETERS
 *  *sensorID : return the sensor ID
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    /*sensor have two i2c address 0x34 & 0x20,
     *we should detect the module used i2c address
     */
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = ((read_cmos_sensor_8(0x0000) << 8)
                    | read_cmos_sensor_8(0x0001));
            LOG_INF(
                "read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x *sensor_id:0x%x\n",
                read_cmos_sensor_8(0x0000),
                read_cmos_sensor_8(0x0001),
                read_cmos_sensor(0x0000), *sensor_id);
            if (*sensor_id == S5KGM1ST_SENSOR_ID_20633) {
                *sensor_id = imgsensor_info.sensor_id;
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, *sensor_id);
                #if 0
                imgsensor_info.module_id = read_module_id();
                LOG_INF("module_id=%d\n",imgsensor_info.module_id);
                /*
                if(deviceInfo_register_value == 0x00){
                    Oplusimgsensor_Registdeviceinfo("Cam_r0", DEVICE_VERSION_GM1ST,
                                    imgsensor_info.module_id);
                    deviceInfo_register_value = 0x01;
                }
                */
                read_eepromData();
                #endif
                return ERROR_NONE;
            }

            LOG_INF("Read sensor id fail, id: 0x%x *sensor_id:0x%x\n", imgsensor.i2c_write_id, *sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        /*if Sensor ID is not correct,
         *Must set *sensor_id to 0xFFFFFFFF
         */
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *  open
 *
 * DESCRIPTION
 *  This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *  None
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint16 sensor_id = 0;

    LOG_INF("open start\n");
    /*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
     *we should detect the module used i2c address
     */
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
            LOG_INF("Read sensor id [0x0000~1]: 0x%x, %x sensor_id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
            if (sensor_id == S5KGM1ST_SENSOR_ID_20633) {
                sensor_id = imgsensor_info.sensor_id;
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, sensor_id);
                break;
            }
            LOG_INF("Read sensor id fail, write id: 0x%x, sensor_id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init();
    spin_lock(&imgsensor_drv_lock);

    imgsensor.autoflicker_en = KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.shutter = 0x3D0;
    imgsensor.gain = 0x100;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_mode = 0;
    imgsensor.test_pattern = 0;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("open End\n");
    return ERROR_NONE;
} /* open */

/*************************************************************************
 * FUNCTION
 *  close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *  None
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
    LOG_INF("E\n");
    /* No Need to implement this function */
    streaming_control(KAL_FALSE);
    return ERROR_NONE;
} /* close */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *  This function start the sensor preview.
 *
 * PARAMETERS
 *  *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s E\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);

    preview_setting();
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
} /* preview */

/*************************************************************************
 * FUNCTION
 *  capture
 *
 * DESCRIPTION
 *  This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
        LOG_INF(
            "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
            imgsensor.current_fps,
            imgsensor_info.cap.max_framerate / 10);
    imgsensor.pclk = imgsensor_info.cap.pclk;
    imgsensor.line_length = imgsensor_info.cap.linelength;
    imgsensor.frame_length = imgsensor_info.cap.framelength;
    imgsensor.min_frame_length = imgsensor_info.cap.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;

    spin_unlock(&imgsensor_drv_lock);
    capture_setting(imgsensor.current_fps);
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.pclk = imgsensor_info.normal_video.pclk;
    imgsensor.line_length = imgsensor_info.normal_video.linelength;
    imgsensor.frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    normal_video_setting(imgsensor.current_fps);
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /*  normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    /*imgsensor.video_mode = KAL_TRUE;*/
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /*  hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s. 720P@240FPS\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    /*imgsensor.video_mode = KAL_TRUE;*/
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /* slim_video */


static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s.\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();

    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /* custom1 */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s.\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
    imgsensor.pclk = imgsensor_info.custom2.pclk;
    imgsensor.line_length = imgsensor_info.custom2.linelength;
    imgsensor.frame_length = imgsensor_info.custom2.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom2_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /* custom2 */

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s.\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
    imgsensor.pclk = imgsensor_info.custom3.pclk;
    imgsensor.line_length = imgsensor_info.custom3.linelength;
    imgsensor.frame_length = imgsensor_info.custom3.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom3_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /* custom3 */

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s.\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
    imgsensor.pclk = imgsensor_info.custom4.pclk;
    imgsensor.line_length = imgsensor_info.custom4.linelength;
    imgsensor.frame_length = imgsensor_info.custom4.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom4_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /* custom4 */

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s.\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
    imgsensor.pclk = imgsensor_info.custom5.pclk;
    imgsensor.line_length = imgsensor_info.custom5.linelength;
    imgsensor.frame_length = imgsensor_info.custom5.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom5_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}   /* custom5 */

static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");
    sensor_resolution->SensorFullWidth =
        imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight =
        imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth =
        imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight =
        imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth =
        imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight =
        imgsensor_info.normal_video.grabwindow_height;

    sensor_resolution->SensorHighSpeedVideoWidth =
        imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight =
        imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth =
        imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight =
        imgsensor_info.slim_video.grabwindow_height;

    sensor_resolution->SensorCustom1Width =
        imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height =
        imgsensor_info.custom1.grabwindow_height;

    sensor_resolution->SensorCustom2Width =
        imgsensor_info.custom2.grabwindow_width;
    sensor_resolution->SensorCustom2Height =
        imgsensor_info.custom2.grabwindow_height;

    sensor_resolution->SensorCustom5Width =
        imgsensor_info.custom5.grabwindow_width;
    sensor_resolution->SensorCustom5Height =
        imgsensor_info.custom5.grabwindow_height;

    sensor_resolution->SensorCustom3Width =
        imgsensor_info.custom3.grabwindow_width;
    sensor_resolution->SensorCustom3Height =
        imgsensor_info.custom3.grabwindow_height;

    sensor_resolution->SensorCustom4Width =
        imgsensor_info.custom4.grabwindow_width;
    sensor_resolution->SensorCustom4Height =
        imgsensor_info.custom4.grabwindow_height;

        return ERROR_NONE;
} /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
               MSDK_SENSOR_INFO_STRUCT *sensor_info,
               MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat =
        imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame =
        imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame =
        imgsensor_info.slim_video_delay_frame;
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
    sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
    sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
    sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
    sensor_info->AESensorGainDelayFrame =
        imgsensor_info.ae_sensor_gain_delay_frame;
    sensor_info->AEISPGainDelayFrame =
        imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
    sensor_info->PDAF_Support = 2;
    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
    sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
    sensor_info->SensorPacketECCOrder = 1;

    sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

        sensor_info->SensorGrabStartX =
            imgsensor_info.normal_video.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.normal_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        sensor_info->SensorGrabStartX =
            imgsensor_info.slim_video.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.slim_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

        break;

    case MSDK_SCENARIO_ID_CUSTOM1:
        sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_CUSTOM2:
        sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_CUSTOM3:
        sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_CUSTOM4:
        sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_CUSTOM5:
        sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
        break;

        default:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
        break;
    }

    return ERROR_NONE;
}   /*  get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
            MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
            MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        preview(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        capture(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        normal_video(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        hs_video(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        slim_video(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        custom1(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        custom2(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        custom3(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CUSTOM4:
        custom4(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CUSTOM5:
        custom5(image_window, sensor_config_data);
        break;
    default:
        LOG_INF("Error ScenarioId setting");
        preview(image_window, sensor_config_data);
        return ERROR_INVALID_SCENARIO_ID;
    }

    return ERROR_NONE;
}   /* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
    LOG_INF("framerate = %d\n ", framerate);
    /* SetVideoMode Function should fix framerate */
    if (framerate == 0)
        /* Dynamic frame rate */
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps, 1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) /*enable auto flicker*/
        imgsensor.autoflicker_en = KAL_TRUE;
    else /*Cancel Auto flick*/
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
        enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        frame_length = imgsensor_info.pre.pclk / framerate * 10
                / imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.pre.framelength)
        ? (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length =
            imgsensor_info.pre.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        if (framerate == 0)
            return ERROR_NONE;
        frame_length = imgsensor_info.normal_video.pclk /
                framerate * 10 /
                imgsensor_info.normal_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.normal_video.framelength)
        ? (frame_length - imgsensor_info.normal_video.framelength)
        : 0;
        imgsensor.frame_length =
            imgsensor_info.normal_video.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
            LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
                    , framerate, imgsensor_info.cap.max_framerate/10);
        frame_length = imgsensor_info.cap.pclk / framerate * 10
                / imgsensor_info.cap.linelength;
        spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line =
            (frame_length > imgsensor_info.cap.framelength)
              ? (frame_length - imgsensor_info.cap.framelength) : 0;
            imgsensor.frame_length =
                imgsensor_info.cap.framelength
                + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        frame_length = imgsensor_info.hs_video.pclk / framerate * 10
                / imgsensor_info.hs_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.hs_video.framelength)
              ? (frame_length - imgsensor_info.hs_video.framelength)
              : 0;
        imgsensor.frame_length =
            imgsensor_info.hs_video.framelength
                + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        frame_length = imgsensor_info.slim_video.pclk / framerate * 10
            / imgsensor_info.slim_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.slim_video.framelength)
            ? (frame_length - imgsensor_info.slim_video.framelength)
            : 0;
        imgsensor.frame_length =
            imgsensor_info.slim_video.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        frame_length = imgsensor_info.custom1.pclk / framerate * 10
                / imgsensor_info.custom1.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.custom1.framelength)
            ? (frame_length - imgsensor_info.custom1.framelength)
            : 0;
        imgsensor.frame_length =
            imgsensor_info.custom1.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        frame_length = imgsensor_info.custom2.pclk / framerate * 10
                / imgsensor_info.custom2.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.custom2.framelength)
            ? (frame_length - imgsensor_info.custom2.framelength)
            : 0;
        imgsensor.frame_length =
            imgsensor_info.custom2.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        frame_length = imgsensor_info.custom3.pclk / framerate * 10
                / imgsensor_info.custom3.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.custom3.framelength)
            ? (frame_length - imgsensor_info.custom3.framelength) : 0;
        imgsensor.frame_length =
            imgsensor_info.custom3.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM4:
        frame_length = imgsensor_info.custom4.pclk / framerate * 10
                / imgsensor_info.custom4.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.custom4.framelength)
            ? (frame_length - imgsensor_info.custom4.framelength) : 0;
        imgsensor.frame_length =
            imgsensor_info.custom4.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM5:
        frame_length = imgsensor_info.custom5.pclk / framerate * 10
                / imgsensor_info.custom5.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.custom5.framelength)
            ? (frame_length - imgsensor_info.custom5.framelength) : 0;
        imgsensor.frame_length =
            imgsensor_info.custom5.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    default:  /*coding with  preview scenario by default*/
        frame_length = imgsensor_info.pre.pclk / framerate * 10
            / imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.pre.framelength)
            ? (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length =
            imgsensor_info.pre.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        LOG_INF("error scenario_id = %d, we use preview scenario\n",
            scenario_id);
        break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
        enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        *framerate = imgsensor_info.pre.max_framerate;
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        *framerate = imgsensor_info.normal_video.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        *framerate = imgsensor_info.cap.max_framerate;
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        *framerate = imgsensor_info.hs_video.max_framerate;
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        *framerate = imgsensor_info.slim_video.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        *framerate = imgsensor_info.custom1.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        *framerate = imgsensor_info.custom2.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        *framerate = imgsensor_info.custom3.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM4:
        *framerate = imgsensor_info.custom4.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM5:
        *framerate = imgsensor_info.custom5.max_framerate;
        break;
    default:
        break;
    }

    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_uint8 modes, struct SET_SENSOR_PATTERN_SOLID_COLOR *pTestpatterndata)
{
    kal_uint16 Color_R, Color_Gr, Color_Gb, Color_B;
    pr_debug("set_test_pattern enum: %d\n", modes);

	if (modes) {
        write_cmos_sensor_8(0x0601, modes);
        if (modes == 1 && (pTestpatterndata != NULL)) { //Solid Color
            Color_R = (pTestpatterndata->COLOR_R >> 16) & 0xFFFF;
            Color_Gr = (pTestpatterndata->COLOR_Gr >> 16) & 0xFFFF;
            Color_B = (pTestpatterndata->COLOR_B >> 16) & 0xFFFF;
            Color_Gb = (pTestpatterndata->COLOR_Gb >> 16) & 0xFFFF;
            write_cmos_sensor_8(0x0602, Color_R >> 8);
            write_cmos_sensor_8(0x0603, Color_R & 0xFF);
            write_cmos_sensor_8(0x0604, Color_Gr >> 8);
            write_cmos_sensor_8(0x0605, Color_Gr & 0xFF);
            write_cmos_sensor_8(0x0606, Color_B >> 8);
            write_cmos_sensor_8(0x0607, Color_B & 0xFF);
            write_cmos_sensor_8(0x0608, Color_Gb >> 8);
            write_cmos_sensor_8(0x0609, Color_Gb & 0xFF);
        }
    } else {
        write_cmos_sensor_8(0x0600, 0x00); /*No pattern*/
        write_cmos_sensor_8(0x0601, 0x00);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = modes;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                 UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
    UINT16 *feature_data_16 = (UINT16 *) feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
    UINT32 *feature_data_32 = (UINT32 *) feature_para;
    unsigned long long *feature_data = (unsigned long long *) feature_para;
    struct SET_PD_BLOCK_INFO_T *PDAFinfo;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    struct SENSOR_VC_INFO_STRUCT *pvcinfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
        = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
    UINT32 *pScenarios = NULL;

    /*LOG_INF("feature_id = %d\n", feature_id);*/
    switch (feature_id) {
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_gain;
        *(feature_data + 2) = imgsensor_info.max_gain;
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        *(feature_data + 0) = imgsensor_info.min_gain_iso;
        *(feature_data + 1) = imgsensor_info.gain_step;
        *(feature_data + 2) = imgsensor_info.gain_type;
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_shutter;
        break;
    #if 0
    case SENSOR_FEATURE_GET_EEPROM_DATA:
        LOG_INF("SENSOR_FEATURE_GET_EEPROM_DATA:%d\n", *feature_para_len);
        memcpy(&feature_para[0], CAM_DUAL_DATA, DUALCAM_CALI_DATA_LENGTH_8ALIGN);
        break;
    case SENSOR_FEATURE_GET_MODULE_INFO:
        LOG_INF("GET_MODULE_CamInfo:%d %d\n", *feature_para_len, *feature_data_32);
        *(feature_data_32 + 1) = (CAM_INFO[1] << 24)
                    | (CAM_INFO[0] << 16)
                    | (CAM_INFO[3] << 8)
                    | (CAM_INFO[2] & 0xFF);
        *(feature_data_32 + 2) = (CAM_INFO[5] << 24)
                    | (CAM_INFO[4] << 16)
                    | (CAM_INFO[7] << 8)
                    | (CAM_INFO[6] & 0xFF);
        break;
    case SENSOR_FEATURE_GET_MODULE_SN:
        LOG_INF("GET_MODULE_SN:%d %d\n", *feature_para_len, *feature_data_32);
        if (*feature_data_32 < CAMERA_MODULE_SN_LENGTH/4)
            *(feature_data_32 + 1) = (CAM_SN[4*(*feature_data_32) + 3] << 24)
                        | (CAM_SN[4*(*feature_data_32) + 2] << 16)
                        | (CAM_SN[4*(*feature_data_32) + 1] << 8)
                        | (CAM_SN[4*(*feature_data_32)] & 0xFF);
        break;
    case SENSOR_FEATURE_SET_SENSOR_OTP:
    {
        kal_int32 ret = IMGSENSOR_RETURN_SUCCESS;
        LOG_INF("SENSOR_FEATURE_SET_SENSOR_OTP length :%d\n", (UINT32)*feature_para_len);
        ret = write_Module_data((ACDK_SENSOR_ENGMODE_STEREO_STRUCT *)(feature_para));
        if (ret == ERROR_NONE)
            return ERROR_NONE;
        else
            return ERROR_MSDK_IS_ACTIVED;
    }
        break;
    #endif
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = -7415000;
        break;
    case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
        pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                *pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
                break;
        case MSDK_SCENARIO_ID_CUSTOM5:
                *pScenarios = MSDK_SCENARIO_ID_CUSTOM5;
                break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM2:
        case MSDK_SCENARIO_ID_CUSTOM4:
        case MSDK_SCENARIO_ID_CUSTOM3:
        case MSDK_SCENARIO_ID_CUSTOM1:
        default:
                *pScenarios = 0xff;
                break;
        }
        pr_debug("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n", *feature_data, *pScenarios);
        break;

    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.cap.pclk;
                break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.normal_video.pclk;
                break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.hs_video.pclk;
                break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.slim_video.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM1:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.custom1.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM2:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.custom2.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM3:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.custom3.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM4:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.custom4.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM5:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.custom5.pclk;
                break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.pre.pclk;
                break;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.cap.framelength << 16)
                                 + imgsensor_info.cap.linelength;
                break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.normal_video.framelength << 16)
                                + imgsensor_info.normal_video.linelength;
                break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.hs_video.framelength << 16)
                                 + imgsensor_info.hs_video.linelength;
                break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.slim_video.framelength << 16)
                                 + imgsensor_info.slim_video.linelength;
                break;
        case MSDK_SCENARIO_ID_CUSTOM1:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.custom1.framelength << 16)
                                 + imgsensor_info.custom1.linelength;
                break;
        case MSDK_SCENARIO_ID_CUSTOM2:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.custom2.framelength << 16)
                                 + imgsensor_info.custom2.linelength;
                break;
        case MSDK_SCENARIO_ID_CUSTOM3:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.custom3.framelength << 16)
                                 + imgsensor_info.custom3.linelength;
                break;
        case MSDK_SCENARIO_ID_CUSTOM4:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.custom4.framelength << 16)
                                 + imgsensor_info.custom4.linelength;
                break;
        case MSDK_SCENARIO_ID_CUSTOM5:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.custom5.framelength << 16)
                                 + imgsensor_info.custom5.linelength;
                break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.pre.framelength << 16)
                                 + imgsensor_info.pre.linelength;
                break;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_para_16++ = imgsensor.line_length;
        *feature_return_para_16 = imgsensor.frame_length;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *feature_return_para_32 = imgsensor.pclk;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
         set_shutter(*feature_data);
        break;
    case SENSOR_FEATURE_SET_GAIN:
        set_gain((UINT16) *feature_data);
        break;
    case SENSOR_FEATURE_SET_REGISTER:
        write_cmos_sensor_8(sensor_reg_data->RegAddr,
                    sensor_reg_data->RegData);
        break;
    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData =
            read_cmos_sensor_8(sensor_reg_data->RegAddr);
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:
        set_video_mode(*feature_data);
        break;
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        get_imgsensor_id(feature_return_para_32);
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
        set_auto_flicker_mode((BOOL)*feature_data_16,
                      *(feature_data_16+1));
        break;
    case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
         set_max_framerate_by_scenario(
                (enum MSDK_SCENARIO_ID_ENUM)*feature_data,
                *(feature_data+1));
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
         get_default_framerate_by_scenario(
                (enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
                (MUINT32 *)(uintptr_t)(*(feature_data+1)));
        break;
    case SENSOR_FEATURE_GET_PDAF_DATA:
        LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        set_test_pattern_mode((UINT8)*feature_data, (struct SET_SENSOR_PATTERN_SOLID_COLOR *) feature_data+1);
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        /* for factory mode auto testing */
        *feature_return_para_32 = imgsensor_info.checksum_value;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = *feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_SET_HDR:
        LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.ihdr_mode = *feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
            (UINT32)*feature_data);
        wininfo =
            (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[1],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[2],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[3],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[4],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[5],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[6],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[7],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[8],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[9],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[0],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_INFO:
        LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
            (UINT16) *feature_data);
        PDAFinfo =
          (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CUSTOM1:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: //4000*3000
            //imgsensor_pd_info_binning.i4BlockNumX = 288;
            //imgsensor_pd_info_binning.i4BlockNumY = 216;
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:  //2000*1500
            //imgsensor_pd_info_binning.i4BlockNumX = 288;
            //imgsensor_pd_info_binning.i4BlockNumY = 162;
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info_binning, sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO: // 4000*2256
            //imgsensor_pd_info_binning.i4BlockNumX = 288;
            //imgsensor_pd_info_binning.i4BlockNumY = 162;
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info_16_9, sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:  //  4000*2600
            //imgsensor_pd_info_binning.i4BlockNumX = 288;
            //imgsensor_pd_info_binning.i4BlockNumY = 162;
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info_20_13, sizeof(struct SET_PD_BLOCK_INFO_T));
        default:
            break;
        }
        break;
    case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
        LOG_INF(
        "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
            (UINT16) *feature_data);
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM1:
        case MSDK_SCENARIO_ID_CUSTOM2:
        case MSDK_SCENARIO_ID_CUSTOM5:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
            break;
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
        break;
    case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
        break;
    case SENSOR_FEATURE_SET_PDAF:
        LOG_INF("PDAF mode :%d\n", *feature_data_16);
        imgsensor.pdaf_mode = *feature_data_16;
        break;
    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
        LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
            (UINT16)*feature_data,
            (UINT16)*(feature_data+1),
            (UINT16)*(feature_data+2));
        break;
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
        set_shutter_frame_length((UINT32) (*feature_data),
                    (UINT32) (*(feature_data + 1)),
                    (BOOL) (*(feature_data + 2)));
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        /*
         * 1, if driver support new sw frame sync
         * set_shutter_frame_length() support third para auto_extend_en
         */
        *(feature_data + 1) = 1;
        /* margin info by scenario */
        *(feature_data + 2) = imgsensor_info.margin;
        break;
    case SENSOR_FEATURE_SET_HDR_SHUTTER:
        LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
            (UINT16)*feature_data, (UINT16)*(feature_data+1));
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
        streaming_control(KAL_FALSE);
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
            *feature_data);
        if (*feature_data != 0)
            set_shutter(*feature_data);
        streaming_control(KAL_TRUE);
        break;
    #if 0
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
        case MSDK_SCENARIO_ID_CUSTOM3:
            *feature_return_para_32 = 1; /*BINNING_NONE*/
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        }
        LOG_ERR("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
            *feature_return_para_32);
        *feature_para_len = 4;
        break;
    #endif
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
    {
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.cap.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.normal_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.hs_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.slim_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom1.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom2.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom3.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom4.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom5.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.pre.mipi_pixel_rate;
            break;
        }
    }
        break;
    case SENSOR_FEATURE_GET_VC_INFO:
        LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
            (UINT16)*feature_data);
        pvcinfo =
            (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CUSTOM1:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
                sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
                sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
                sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
                sizeof(struct SENSOR_VC_INFO_STRUCT));
        default:
            break;
        }
        break;
    case SENSOR_FEATURE_SET_AWB_GAIN:
        /* modify to separate 3hdr and remosaic */
        if (imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3) {
            /*write AWB gain to sensor*/
            feedback_awbgain((UINT32)*(feature_data_32 + 1),
                    (UINT32)*(feature_data_32 + 2));
        } else {
            gm1st_awb_gain(
                (struct SET_SENSOR_AWB_GAIN *) feature_para);
        }
        break;
    case SENSOR_FEATURE_SET_LSC_TBL:
    {
        kal_uint8 index =
            *(((kal_uint8 *)feature_para) + (*feature_para_len));

        gm1st_set_lsc_reg_setting(index, feature_data_16,
                      (*feature_para_len)/sizeof(UINT16));
    }
        break;
    default:
        break;
    }

    return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};


UINT32 ATHENSC_S5KGM1ST_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc != NULL) {
        *pfFunc = &sensor_func;
    }
    return ERROR_NONE;
} /* GM1ST_MIPI_RAW_SensorInit */
