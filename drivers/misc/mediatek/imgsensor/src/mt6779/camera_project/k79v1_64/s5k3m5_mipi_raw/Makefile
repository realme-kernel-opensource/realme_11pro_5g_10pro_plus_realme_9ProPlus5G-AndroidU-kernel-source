#include $(srctree)/drivers/misc/mediatek/Makefile.custom
#ifdef OPLUS_FEATURE_CAMERA_COMMON
#wen.luo@BSP.Kernel.Stability, 2019/2/21,  for enable KASAN, use GCC complile tool, add flag format
ifeq ($(CONFIG_KASAN),y)
subdir-ccflags-y += -Wno-format
endif
#endif /*OPLUS_FEATURE_CAMERA_COMMON*/
obj-y += s5k3m5mipiraw_Sensor.o

