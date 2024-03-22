#include $(srctree)/drivers/misc/mediatek/Makefile.custom
#wen.luo@BSP.Kernel.Stability, 2019/2/21,  for enable KASAN, use GCC complile tool, add flag format
ifeq ($(CONFIG_KASAN),y)
subdir-ccflags-y += -Wno-format
endif
obj-y += s5kgh1mipiraw_Sensor.o

