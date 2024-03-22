# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_INPUT)		+= input-core.o
input-core-y := input.o input-compat.o input-mt.o ff-core.o

obj-$(CONFIG_INPUT_FF_MEMLESS)	+= ff-memless.o
obj-$(CONFIG_INPUT_POLLDEV)	+= input-polldev.o
obj-$(CONFIG_INPUT_SPARSEKMAP)	+= sparse-keymap.o
obj-$(CONFIG_INPUT_MATRIXKMAP)	+= matrix-keymap.o

obj-$(CONFIG_INPUT_LEDS)	+= input-leds.o
obj-$(CONFIG_INPUT_MOUSEDEV)	+= mousedev.o
obj-$(CONFIG_INPUT_JOYDEV)	+= joydev.o
obj-$(CONFIG_INPUT_EVDEV)	+= evdev.o
obj-$(CONFIG_INPUT_EVBUG)	+= evbug.o

obj-$(CONFIG_INPUT_KEYBOARD)	+= keyboard/
obj-$(CONFIG_INPUT_MOUSE)	+= mouse/
obj-$(CONFIG_INPUT_JOYSTICK)	+= joystick/
obj-$(CONFIG_INPUT_TABLET)	+= tablet/
obj-$(CONFIG_INPUT_TOUCHSCREEN)	+= touchscreen/
obj-$(CONFIG_INPUT_MISC)	+= misc/

obj-$(CONFIG_INPUT_APMPOWER)	+= apm-power.o

#ifdef OPLUS_FEATURE_SECURITY_COMMON
#Fangjie.Lei@BSP.Security.Basic, 2021-08-02, Add for oplus_secure_common driver
obj-$(CONFIG_OPLUS_SECURE)  += oplus_secure_drivers/
#endif /*OPLUS_FEATURE_SECURITY_COMMON*/

#ifdef OPLUS_FEATURE_FINGERPRINT
#zhoubo@BSP.Biometrics.Fingerprint, 2021-07-06, add for fingerpring driver
obj-$(CONFIG_OPLUS_FINGERPRINT) += oplus_fp_drivers/
#Zhipeng.Ma@BSP.Biometrics.Fingerprint, 2022-06-21, add for uff fingerprint driver
obj-$(CONFIG_OPLUS_FINGERPRINT)  += uff_fp_drivers/
#endif /*OPLUS_FEATURE_FINGERPRINT*/

obj-$(CONFIG_RMI4_CORE)		+= rmi4/
