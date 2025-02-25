// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>

#include <mtk_dbg_common_v1.h>
#include <mtk_lpm_module.h>
#include <mtk_idle_sysfs.h>
#include <mtk_suspend_sysfs.h>
#include <mtk_spm_sysfs.h>

#include <mt6833_pwr_ctrl.h>
#include <mt6833_dbg_fs_common.h>
#include <mt6833_cond.h>
#include <mt6833_spm_comm.h>
#if IS_ENABLED(CONFIG_OPLUS_POWERINFO_STANDBY_DEBUG)
#include <mt-plat/mtk_ccci_common.h>
#endif

/* Determine for node route */
#define MT_LP_RQ_NODE	"/sys/kernel/debug/spm/spm_resource_req"

#define DEFINE_ATTR_RO(_name)			\
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = #_name,			\
			.mode = 0444,			\
		},					\
		.show	= _name##_show,			\
	}
#define DEFINE_ATTR_RW(_name)			\
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = #_name,			\
			.mode = 0644,			\
		},					\
		.show	= _name##_show,			\
		.store	= _name##_store,		\
	}
#define __ATTR_OF(_name)	(&_name##_attr.attr)



#undef mtk_dbg_spm_log
#define mtk_dbg_spm_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


static char *mt6833_pwr_ctrl_str[PW_MAX_COUNT] = {
	[PW_PCM_FLAGS] = "pcm_flags",
	[PW_PCM_FLAGS_CUST] = "pcm_flags_cust",
	[PW_PCM_FLAGS_CUST_SET] = "pcm_flags_cust_set",
	[PW_PCM_FLAGS_CUST_CLR] = "pcm_flags_cust_clr",
	[PW_PCM_FLAGS1] = "pcm_flags1",
	[PW_PCM_FLAGS1_CUST] = "pcm_flags1_cust",
	[PW_PCM_FLAGS1_CUST_SET] = "pcm_flags1_cust_set",
	[PW_PCM_FLAGS1_CUST_CLR] = "pcm_flags1_cust_clr",
	[PW_TIMER_VAL] = "timer_val",
	[PW_TIMER_VAL_CUST] = "timer_val_cust",
	[PW_TIMER_VAL_RAMP_EN] = "timer_val_ramp_en",
	[PW_TIMER_VAL_RAMP_EN_SEC] = "timer_val_ramp_en_sec",
	[PW_WAKE_SRC] = "wake_src",
	[PW_WAKE_SRC_CUST] = "wake_src_cust",
	[PW_WAKELOCK_TIMER_VAL] = "wakelock_timer_val",
	[PW_WDT_DISABLE] = "wdt_disable",
	/* SPM_CLK_CON */
	[PW_REG_SRCCLKEN0_CTL] = "reg_srcclken0_ctl",
	[PW_REG_SRCCLKEN1_CTL] = "reg_srcclken1_ctl",
	[PW_REG_SPM_LOCK_INFRA_DCM] = "reg_spm_lock_infra_dcm",
	[PW_REG_SRCCLKEN_MASK] = "reg_srcclken_mask",
	[PW_REG_MD1_C32RM_EN] = "reg_md1_c32rm_en",
	[PW_REG_MD2_C32RM_EN] = "reg_md2_c32rm_en",
	[PW_REG_CLKSQ0_SEL_CTRL] = "reg_clksq0_sel_ctrl",
	[PW_REG_CLKSQ1_SEL_CTRL] = "reg_clksq1_sel_ctrl",
	[PW_REG_SRCCLKEN0_EN] = "reg_srcclken0_en",
	[PW_REG_SRCCLKEN1_EN] = "reg_srcclken1_en",
	[PW_REG_SYSCLK0_SRC_MASK_B] = "reg_sysclk0_src_mask_b",
	[PW_REG_SYSCLK1_SRC_MASK_B] = "reg_sysclk1_src_mask_b",

	/* SPM_AP_STANDBY_CON */
	[PW_REG_WFI_OP] = "reg_wfi_op",
	[PW_REG_WFI_TYPE] = "reg_wfi_type",
	[PW_REG_MP0_CPUTOP_IDLE_MASK] = "reg_mp0_cputop_idle_mask",
	[PW_REG_MP1_CPUTOP_IDLE_MASK] = "reg_mp1_cputop_idle_mask",
	[PW_REG_MCUSYS_IDLE_MASK] = "reg_mcusys_idle_mask",
	[PW_REG_MD_APSRC_1_SEL] = "reg_md_apsrc_1_sel",
	[PW_REG_MD_APSRC_0_SEL] = "reg_md_apsrc_0_sel",
	[PW_REG_CONN_APSRC_SEL] = "reg_conn_apsrc_sel",
	/* SPM_SRC6_MASK */
	[PW_REG_CCIF_EVENT_INFRA_REQ_MASK_B] =
		"reg_ccif_event_infra_req_mask_b",
	[PW_REG_CCIF_EVENT_APSRC_REQ_MASK_B] =
		"reg_ccif_event_apsrc_req_mask_b",
	/* SPM_SRC_REQ */
	[PW_REG_SPM_APSRC_REQ] = "reg_spm_apsrc_req",
	[PW_REG_SPM_F26M_REQ] = "reg_spm_f26m_req",
	[PW_REG_SPM_INFRA_REQ] = "reg_spm_infra_req",
	[PW_REG_SPM_VRF18_REQ] = "reg_spm_vrf18_req",
	[PW_REG_SPM_DDREN_REQ] = "reg_spm_ddren_req",
	[PW_REG_SPM_DVFS_REQ] = "reg_spm_dvfs_req",
	[PW_REG_SPM_SW_MAILBOX_REQ] = "reg_spm_sw_mailbox_req",
	[PW_REG_SPM_SSPM_MAILBOX_REQ] = "reg_spm_sspm_mailbox_req",
	[PW_REG_SPM_ADSP_MAILBOX_REQ] = "reg_spm_adsp_mailbox_req",
	[PW_REG_SPM_SCP_MAILBOX_REQ] = "reg_spm_scp_mailbox_req",
	/* SPM_SRC_MASK */
	[PW_REG_MD_0_SRCCLKENA_MASK_B] = "reg_md_0_srcclkena_mask_b",
	[PW_REG_MD_0_INFRA_REQ_MASK_B] = "reg_md_0_infra_req_mask_b",
	[PW_REG_MD_0_APSRC_REQ_MASK_B] = "reg_md_0_apsrc_req_mask_b",
	[PW_REG_MD_0_VRF18_REQ_MASK_B] = "reg_md_0_vrf18_req_mask_b",
	[PW_REG_MD_0_DDREN_REQ_MASK_B] = "reg_md_0_ddren_req_mask_b",
	[PW_REG_MD_1_SRCCLKENA_MASK_B] = "reg_md_1_srcclkena_mask_b",
	[PW_REG_MD_1_INFRA_REQ_MASK_B] = "reg_md_1_infra_req_mask_b",
	[PW_REG_MD_1_APSRC_REQ_MASK_B] = "reg_md_1_apsrc_req_mask_b",
	[PW_REG_MD_1_VRF18_REQ_MASK_B] = "reg_md_1_vrf18_req_mask_b",
	[PW_REG_MD_1_DDREN_REQ_MASK_B] = "reg_md_1_ddren_req_mask_b",
	[PW_REG_CONN_SRCCLKENA_MASK_B] = "reg_conn_srcclkena_mask_b",
	[PW_REG_CONN_SRCCLKENB_MASK_B] = "reg_conn_srcclkenb_mask_b",
	[PW_REG_CONN_INFRA_REQ_MASK_B] = "reg_conn_infra_req_mask_b",
	[PW_REG_CONN_APSRC_REQ_MASK_B] = "reg_conn_apsrc_req_mask_b",
	[PW_REG_CONN_VRF18_REQ_MASK_B] = "reg_conn_vrf18_req_mask_b",
	[PW_REG_CONN_DDREN_REQ_MASK_B] = "reg_conn_ddren_req_mask_b",
	[PW_REG_CONN_VFE28_MASK_B] = "reg_conn_vfe28_mask_b",
	[PW_REG_SRCCLKENI_SRCCLKENA_MASK_B] = "reg_srcclkeni_srcclkena_mask_b",
	[PW_REG_SRCCLKENI_INFRA_REQ_MASK_B] = "reg_srcclkeni_infra_req_mask_b",
	[PW_REG_INFRASYS_APSRC_REQ_MASK_B] = "reg_infrasys_apsrc_req_mask_b",
	[PW_REG_INFRASYS_DDREN_REQ_MASK_B] = "reg_infrasys_ddren_req_mask_b",
	[PW_REG_SSPM_SRCCLKENA_MASK_B] = "reg_sspm_srcclkena_mask_b",
	[PW_REG_SSPM_INFRA_REQ_MASK_B] = "reg_sspm_infra_req_mask_b",
	[PW_REG_SSPM_APSRC_REQ_MASK_B] = "reg_sspm_apsrc_req_mask_b",
	[PW_REG_SSPM_VRF18_REQ_MASK_B] = "reg_sspm_vrf18_req_mask_b",
	[PW_REG_SSPM_DDREN_REQ_MASK_B] = "reg_sspm_ddren_req_mask_b",
	/* SPM_SRC2_MASK */
	[PW_REG_SCP_SRCCLKENA_MASK_B] = "reg_scp_srcclkena_mask_b",
	[PW_REG_SCP_INFRA_REQ_MASK_B] = "reg_scp_infra_req_mask_b",
	[PW_REG_SCP_APSRC_REQ_MASK_B] = "reg_scp_apsrc_req_mask_b",
	[PW_REG_SCP_VRF18_REQ_MASK_B] = "reg_scp_vrf18_req_mask_b",
	[PW_REG_SCP_DDREN_REQ_MASK_B] = "reg_scp_ddren_req_mask_b",
	[PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B] = "reg_audio_dsp_srcclkena_mask_b",
	[PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B] = "reg_audio_dsp_infra_req_mask_b",
	[PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B] = "reg_audio_dsp_apsrc_req_mask_b",
	[PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B] = "reg_audio_dsp_vrf18_req_mask_b",
	[PW_REG_AUDIO_DSP_DDREN_REQ_MASK_B] = "reg_audio_dsp_ddren_req_mask_b",
	[PW_REG_UFS_SRCCLKENA_MASK_B] = "reg_ufs_srcclkena_mask_b",
	[PW_REG_UFS_INFRA_REQ_MASK_B] = "reg_ufs_infra_req_mask_b",
	[PW_REG_UFS_APSRC_REQ_MASK_B] = "reg_ufs_apsrc_req_mask_b",
	[PW_REG_UFS_VRF18_REQ_MASK_B] = "reg_ufs_vrf18_req_mask_b",
	[PW_REG_UFS_DDREN_REQ_MASK_B] = "reg_ufs_ddren_req_mask_b",
	[PW_REG_DISP0_APSRC_REQ_MASK_B] = "reg_disp0_apsrc_req_mask_b",
	[PW_REG_DISP0_DDREN_REQ_MASK_B] = "reg_disp0_ddren_req_mask_b",
	[PW_REG_DISP1_APSRC_REQ_MASK_B] = "reg_disp1_apsrc_req_mask_b",
	[PW_REG_DISP1_DDREN_REQ_MASK_B] = "reg_disp1_ddren_req_mask_b",
	[PW_REG_GCE_INFRA_REQ_MASK_B] = "reg_gce_infra_req_mask_b",
	[PW_REG_GCE_APSRC_REQ_MASK_B] = "reg_gce_apsrc_req_mask_b",
	[PW_REG_GCE_VRF18_REQ_MASK_B] = "reg_gce_vrf18_req_mask_b",
	[PW_REG_GCE_DDREN_REQ_MASK_B] = "reg_gce_ddren_req_mask_b",
	[PW_REG_APU_SRCCLKENA_MASK_B] = "reg_apu_srcclkena_mask_b",
	[PW_REG_APU_INFRA_REQ_MASK_B] = "reg_apu_infra_req_mask_b",
	[PW_REG_APU_APSRC_REQ_MASK_B] = "reg_apu_apsrc_req_mask_b",
	[PW_REG_APU_VRF18_REQ_MASK_B] = "reg_apu_vrf18_req_mask_b",
	[PW_REG_APU_DDREN_REQ_MASK_B] = "reg_apu_ddren_req_mask_b",
	[PW_REG_CG_CHECK_SRCCLKENA_MASK_B] = "reg_cg_check_srcclkena_mask_b",
	[PW_REG_CG_CHECK_APSRC_REQ_MASK_B] = "reg_cg_check_apsrc_req_mask_b",
	[PW_REG_CG_CHECK_VRF18_REQ_MASK_B] = "reg_cg_check_vrf18_req_mask_b",
	[PW_REG_CG_CHECK_DDREN_REQ_MASK_B] = "reg_cg_check_ddren_req_mask_b",
	/* SPM_SRC3_MASK */
	[PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B] =
		"reg_dvfsrc_event_trigger_mask_b",
	[PW_REG_SW2SPM_WAKEUP_MASK_B] = "reg_sw2spm_wakeup_mask_b",
	[PW_REG_ADSP2SPM_WAKEUP_MASK_B] = "reg_adsp2spm_wakeup_mask_b",
	[PW_REG_SSPM2SPM_WAKEUP_MASK_B] = "reg_sspm2spm_wakeup_mask_b",
	[PW_REG_SCP2SPM_WAKEUP_MASK_B] = "reg_scp2spm_wakeup_mask_b",
	[PW_REG_CSYSPWRUP_ACK_MASK] = "reg_csyspwrup_ack_mask",
	[PW_REG_SPM_RESERVED_SRCCLKENA_MASK_B] =
		"reg_spm_reserved_srcclkena_mask_b",
	[PW_REG_SPM_RESERVED_INFRA_REQ_MASK_B] =
		"reg_spm_reserved_infra_req_mask_b",
	[PW_REG_SPM_RESERVED_APSRC_REQ_MASK_B] =
		"reg_spm_reserved_apsrc_req_mask_b",
	[PW_REG_SPM_RESERVED_VRF18_REQ_MASK_B] =
		"reg_spm_reserved_vrf18_req_mask_b",
	[PW_REG_SPM_RESERVED_DDREN_REQ_MASK_B] =
		"reg_spm_reserved_ddren_req_mask_b",
	[PW_REG_MCUPM_SRCCLKENA_MASK_B] = "reg_mcupm_srcclkena_mask_b",
	[PW_REG_MCUPM_INFRA_REQ_MASK_B] = "reg_mcupm_infra_req_mask_b",
	[PW_REG_MCUPM_APSRC_REQ_MASK_B] = "reg_mcupm_apsrc_req_mask_b",
	[PW_REG_MCUPM_VRF18_REQ_MASK_B] = "reg_mcupm_vrf18_req_mask_b",
	[PW_REG_MCUPM_DDREN_REQ_MASK_B] = "reg_mcupm_ddren_req_mask_b",
	[PW_REG_MSDC0_SRCCLKENA_MASK_B] = "reg_msdc0_srcclkena_mask_b",
	[PW_REG_MSDC0_INFRA_REQ_MASK_B] = "reg_msdc0_infra_req_mask_b",
	[PW_REG_MSDC0_APSRC_REQ_MASK_B] = "reg_msdc0_apsrc_req_mask_b",
	[PW_REG_MSDC0_VRF18_REQ_MASK_B] = "reg_msdc0_vrf18_req_mask_b",
	[PW_REG_MSDC0_DDREN_REQ_MASK_B] = "reg_msdc0_ddren_req_mask_b",
	[PW_REG_MSDC1_SRCCLKENA_MASK_B] = "reg_msdc1_srcclkena_mask_b",
	[PW_REG_MSDC1_INFRA_REQ_MASK_B] = "reg_msdc1_infra_req_mask_b",
	[PW_REG_MSDC1_APSRC_REQ_MASK_B] = "reg_msdc1_apsrc_req_mask_b",
	[PW_REG_MSDC1_VRF18_REQ_MASK_B] = "reg_msdc1_vrf18_req_mask_b",
	[PW_REG_MSDC1_DDREN_REQ_MASK_B] = "reg_msdc1_ddren_req_mask_b",
	/* SPM_SRC4_MASK */
	[PW_REG_CCIF_EVENT_SRCCLKENA_MASK_B] =
		"reg_ccif_event_srcclkena_mask_b",
	[PW_REG_BAK_PSRI_SRCCLKENA_MASK_B] = "reg_bak_psri_srcclkena_mask_b",
	[PW_REG_BAK_PSRI_INFRA_REQ_MASK_B] = "reg_bak_psri_infra_req_mask_b",
	[PW_REG_BAK_PSRI_APSRC_REQ_MASK_B] = "reg_bak_psri_apsrc_req_mask_b",
	[PW_REG_BAK_PSRI_VRF18_REQ_MASK_B] = "reg_bak_psri_vrf18_req_mask_b",
	[PW_REG_BAK_PSRI_DDREN_REQ_MASK_B] = "reg_bak_psri_ddren_req_mask_b",
	[PW_REG_DRAMC_MD32_INFRA_REQ_MASK_B] =
		"reg_dramc_md32_infra_req_mask_b",
	[PW_REG_DRAMC_MD32_VRF18_REQ_MASK_B] =
		"reg_dramc_md32_vrf18_req_mask_b",
	[PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B] =
		"reg_conn_srcclkenb2pwrap_mask_b",
	[PW_REG_DRAMC_MD32_APSRC_REQ_MASK_B] =
		"reg_dramc_md32_apsrc_req_mask_b",
	/* SPM_SRC5_MASK */
	[PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B] =
		"reg_mcusys_merge_apsrc_req_mask_b",
	[PW_REG_MCUSYS_MERGE_DDREN_REQ_MASK_B] =
		"reg_mcusys_merge_ddren_req_mask_b",
	[PW_REG_AFE_SRCCLKENA_MASK_B] = "reg_afe_srcclkena_mask_b",
	[PW_REG_AFE_INFRA_REQ_MASK_B] = "reg_afe_infra_req_mask_b",
	[PW_REG_AFE_APSRC_REQ_MASK_B] = "reg_afe_apsrc_req_mask_b",
	[PW_REG_AFE_VRF18_REQ_MASK_B] = "reg_afe_vrf18_req_mask_b",
	[PW_REG_AFE_DDREN_REQ_MASK_B] = "reg_afe_ddren_req_mask_b",
	[PW_REG_MSDC2_SRCCLKENA_MASK_B] = "reg_msdc2_srcclkena_mask_b",
	[PW_REG_MSDC2_INFRA_REQ_MASK_B] = "reg_msdc2_infra_req_mask_b",
	[PW_REG_MSDC2_APSRC_REQ_MASK_B] = "reg_msdc2_apsrc_req_mask_b",
	[PW_REG_MSDC2_VRF18_REQ_MASK_B] = "reg_msdc2_vrf18_req_mask_b",
	[PW_REG_MSDC2_DDREN_REQ_MASK_B] = "reg_msdc2_ddren_req_mask_b",
	/* SPM_SRC7_MASK */
	[PW_REG_PCIE_SRCCLKENA_MASK_B] = "reg_pcie_srcclkena_mask_b",
	[PW_REG_PCIE_INFRA_REQ_MASK_B] = "reg_pcie_infra_req_mask_b",
	[PW_REG_PCIE_APSRC_REQ_MASK_B] = "reg_pcie_apsrc_req_mask_b",
	[PW_REG_PCIE_VRF18_REQ_MASK_B] = "reg_pcie_vrf18_req_mask_b",
	[PW_REG_PCIE_DDREN_REQ_MASK_B] = "reg_pcie_ddren_req_mask_b",
	[PW_REG_DPMAIF_SRCCLKENA_MASK_B] = "reg_dpmaif_srcclkena_mask_b",
	[PW_REG_DPMAIF_INFRA_REQ_MASK_B] = "reg_dpmaif_infra_req_mask_b",
	[PW_REG_DPMAIF_APSRC_REQ_MASK_B] = "reg_dpmaif_apsrc_req_mask_b",
	[PW_REG_DPMAIF_VRF18_REQ_MASK_B] = "reg_dpmaif_vrf18_req_mask_b",
	[PW_REG_DPMAIF_DDREN_REQ_MASK_B] = "reg_dpmaif_ddren_req_mask_b",

	/* SPM_WAKEUP_EVENT_MASK */
	[PW_REG_WAKEUP_EVENT_MASK] = "reg_wakeup_event_mask",
	/* SPM_WAKEUP_EVENT_EXT_MASK */
	[PW_REG_EXT_WAKEUP_EVENT_MASK] = "reg_ext_wakeup_event_mask",
};

/**************************************
 * xxx_ctrl_show Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t mt6833_show_pwr_ctrl(int id, char *buf, size_t buf_sz)
{
	char *p = buf;
	size_t mSize = 0;
	int i;

	for (i = 0; i < PW_MAX_COUNT; i++) {
		mSize += scnprintf(p + mSize, buf_sz - mSize,
			"%s = 0x%zx\n",
			mt6833_pwr_ctrl_str[i],
			mtk_lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET, i, 0));
	}

	WARN_ON(buf_sz - mSize <= 0);

	return mSize;
}

/**************************************
 * xxx_ctrl_store Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t mt6833_store_pwr_ctrl(int id,	const char *buf, size_t count)
{
	u32 val;
	char cmd[64];
	int i;

	if (sscanf(buf, "%63s %x", cmd, &val) != 2)
		return -EPERM;
	pr_info("[SPM] pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	for (i = 0 ; i < PW_MAX_COUNT; i++) {
		if (!strcmp(cmd, mt6833_pwr_ctrl_str[i])) {
			mtk_lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET, i, val);
			break;
		}
	}

	return count;
}

static ssize_t suspend_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return mt6833_show_pwr_ctrl(MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL,
			     buf, get_mtk_lp_kernfs_bufsz_max());
}
static ssize_t idle_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return mt6833_show_pwr_ctrl(MT_SPM_DBG_SMC_UID_IDLE_PWR_CTRL,
			     buf, get_mtk_lp_kernfs_bufsz_max());
}
static ssize_t suspend_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	return mt6833_store_pwr_ctrl(MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL,
				     buf, count);
}
static ssize_t idle_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	return mt6833_store_pwr_ctrl(MT_SPM_DBG_SMC_UID_IDLE_PWR_CTRL,
				     buf, count);
}

/**************************************
 * Init Function
 **************************************/
DEFINE_ATTR_RW(suspend_ctrl);
DEFINE_ATTR_RW(idle_ctrl);

static struct attribute *pwrctrl_attrs[] = {
	/* for spm_lp_scen.pwrctrl */
	__ATTR_OF(suspend_ctrl),
	__ATTR_OF(idle_ctrl),
	/* must */
	NULL,
};
static struct attribute_group pwrctrl_attr_group = {
	.name = "spm",
	.attrs = pwrctrl_attrs,
};

static char *mt6833_spm_resource_str[MT_SPM_RES_MAX] = {
	[MT_SPM_RES_XO_FPM] = "XO_FPM",
	[MT_SPM_RES_CK_26M] = "CK_26M",
	[MT_SPM_RES_INFRA] = "INFRA",
	[MT_SPM_RES_SYSPLL] = "SYSPLL",
	[MT_SPM_RES_DRAM_S0] = "DRAM_S0",
	[MT_SPM_RES_DRAM_S1] = "DRAM_S1",
};

static ssize_t mt6833_spm_res_rq_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int i, s, u;
	unsigned int unum, uvalid, uname_i, uname_t;
	unsigned int rnum, rusage, per_usage;
	char uname[MT_LP_RQ_USER_NAME_LEN+1];

	mtk_dbg_spm_log("resource_num=%d, user_num=%d, user_valid=0x%x\n",
	    rnum = mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_NUM,
				       MT_LPM_SMC_ACT_GET, 0, 0),
	    unum = mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NUM,
				       MT_LPM_SMC_ACT_GET, 0, 0),
	    uvalid = mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					 MT_LPM_SMC_ACT_GET, 0, 0));
	rusage = mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
				     MT_LPM_SMC_ACT_GET,
				     MT_LP_RQ_ID_ALL_USAGE, 0);
	mtk_dbg_spm_log("\n");
	mtk_dbg_spm_log("user [bit][valid]:\n");
	for (i = 0; i < unum; i++) {
		uname_i = mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NAME,
					    MT_LPM_SMC_ACT_GET, i, 0);
		for (s = 0, u = 0; s < MT_LP_RQ_USER_NAME_LEN;
		     s++, u += MT_LP_RQ_USER_CHAR_U) {
			uname_t = ((uname_i >> u) & MT_LP_RQ_USER_CHAR_MASK);
			uname[s] = (uname_t) ? (char)uname_t : ' ';
		}
		uname[s] = '\0';
		mtk_dbg_spm_log("%4s [%3d][%3s]\n", uname, i,
		    ((1<<i) & uvalid) ? "yes" : "no");
	}
	mtk_dbg_spm_log("\n");

	if (rnum != MT_SPM_RES_MAX) {
		mtk_dbg_spm_log("Platform resource amount mismatch\n");
		rnum = (rnum > MT_SPM_RES_MAX) ? MT_SPM_RES_MAX : rnum;
	}

	mtk_dbg_spm_log("resource [bit][user_usage][blocking]:\n");
	for (i = 0; i < rnum; i++) {
		mtk_dbg_spm_log("%8s [%3d][0x%08x][%3s]\n",
			mt6833_spm_resource_str[i], i,
			(per_usage =
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
					    MT_LPM_SMC_ACT_GET, i, 0)),
			((1<<i) & rusage) ? "yes" : "no"
		   );
	}
	mtk_dbg_spm_log("\n");
	mtk_dbg_spm_log("resource request command help:\n");
	mtk_dbg_spm_log("echo enable ${user_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo bypass ${user_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo request ${resource_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo release > %s\n", MT_LP_RQ_NODE);

	return p - ToUserBuf;
}

static ssize_t mt6833_spm_res_rq_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int parm;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "bypass"))
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					    MT_LPM_SMC_ACT_SET,
					    parm, 0);
		else if (!strcmp(cmd, "enable"))
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					    MT_LPM_SMC_ACT_SET,
					    parm, 1);
		else if (!strcmp(cmd, "request"))
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_REQ,
					    MT_LPM_SMC_ACT_SET,
					    0, parm);
		return sz;
	} else if (sscanf(FromUserBuf, "%127s", cmd) == 1) {
		if (!strcmp(cmd, "release"))
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_REQ,
					    MT_LPM_SMC_ACT_CLR,
					    0, 0);
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		return sz;
	}

	return -EINVAL;
}

#if IS_ENABLED(CONFIG_OPLUS_POWERINFO_STANDBY_DEBUG)
#define MD_SLEEP_INFO_SMEM_OFFEST (4)
static ssize_t mtk_dbg_get_system_stats(char *ToUserBuf,
			  size_t sz, void *priv)
{
	/* dump sleep info */
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	u32 len = 0;
	u32 *share_mem = NULL;
	struct md_sleep_status md_data;

	share_mem = (u32 *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_LOW_POWER, NULL);
	memset(&md_data, 0, sizeof(struct md_sleep_status));
	if (share_mem != NULL) {
		share_mem = share_mem + MD_SLEEP_INFO_SMEM_OFFEST;
		memcpy(&md_data, share_mem, sizeof(struct md_sleep_status));
	}
	len = snprintf(ToUserBuf, sz,
	"26M:%lld:%lld.%03lld\nAP:%lld:%lld.%03lld\nMD:%lld:%lld.%03lld\n",
	spm_26M_off_count,
	PCM_TICK_TO_SEC(spm_26M_off_duration),
	PCM_TICK_TO_SEC((spm_26M_off_duration % PCM_32K_TICKS_PER_SEC)
		* 1000),
	ap_pd_count,
	PCM_TICK_TO_SEC(ap_slp_duration),
	PCM_TICK_TO_SEC((ap_slp_duration % PCM_32K_TICKS_PER_SEC)
		* 1000),
	md_data.sleep_cnt,
	PCM_TICK_TO_SEC(md_data.sleep_time),
	PCM_TICK_TO_SEC((md_data.sleep_time % PCM_32K_TICKS_PER_SEC)
		* 1000));

	return (len > sz) ? sz : len;
#else
	return 0;
#endif
}

static const struct mtk_lp_sysfs_op mtk_dbg_spm_system_stats_fops = {
	.fs_read = mtk_dbg_get_system_stats,
};
#endif

static const struct mtk_lp_sysfs_op mt6833_spm_res_rq_fops = {
	.fs_read = mt6833_spm_res_rq_read,
	.fs_write = mt6833_spm_res_rq_write,
};

int mt6833_dbg_spm_fs_init(void)
{
	int r;

	mtk_spm_sysfs_root_entry_create();
	mtk_spm_sysfs_entry_node_add("spm_resource_req", 0444
			, &mt6833_spm_res_rq_fops, NULL);
#if IS_ENABLED(CONFIG_OPLUS_POWERINFO_STANDBY_DEBUG)
	mtk_spm_sysfs_entry_node_add("system_stats", 0444
  			, &mtk_dbg_spm_system_stats_fops, NULL);
#endif

	/* create /sys/power/spm/xxx */
	r = mtk_spm_sysfs_power_create_group(&pwrctrl_attr_group);
	if (r)
		pr_info("[SPM] FAILED TO CREATE /sys/power/spm (%d)\n", r);
	return r;
}

int mt6833_dbg_spm_fs_deinit(void)
{
	return 0;
}

