
#include <typedefs.h>
#include <osl.h>

#include <bcmutils.h>
#include <hndsoc.h>
#if defined(HW_OOB)
#include <bcmdefs.h>
#include <bcmsdh.h>
#include <sdio.h>
#include <sbchipc.h>
#endif

#include <dhd_config.h>
#include <dhd_dbg.h>
#include <wl_cfg80211.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

/* message levels */
#define CONFIG_ERROR_LEVEL	0x0001
#define CONFIG_TRACE_LEVEL	0x0002

uint config_msg_level = CONFIG_ERROR_LEVEL;

#define CONFIG_ERROR(x) \
	do { \
		if (config_msg_level & CONFIG_ERROR_LEVEL) { \
			printk(KERN_ERR "CONFIG-ERROR) ");	\
			printk x; \
		} \
	} while (0)
#define CONFIG_TRACE(x) \
	do { \
		if (config_msg_level & CONFIG_TRACE_LEVEL) { \
			printk(KERN_ERR "CONFIG-TRACE) ");	\
			printk x; \
		} \
	} while (0)

#define SBSDIO_CIS_SIZE_LIMIT		0x200		/* maximum bytes in one CIS */
#define MAXSZ_BUF		1000
#define	MAXSZ_CONFIG	4096

#define BCM43362A0_CHIP_REV     0
#define BCM43362A2_CHIP_REV     1
#define BCM4330B2_CHIP_REV      4
#define BCM43340B0_CHIP_REV     2
#define BCM43341B0_CHIP_REV     2
#define BCM43241B4_CHIP_REV     5
#define BCM4335A0_CHIP_REV      2
#define BCM4339A0_CHIP_REV      1

#define FW_TYPE_STA     0
#define FW_TYPE_APSTA   1
#define FW_TYPE_P2P     2
#define FW_TYPE_MFG     3
#define FW_TYPE_G       0
#define FW_TYPE_AG      1

const static char *bcm4330b2_fw_name[] = {
	"fw_bcm40183b2.bin",
	"fw_bcm40183b2_apsta.bin",
	"fw_bcm40183b2_p2p.bin",
	"fw_bcm40183b2_mfg.bin"
};

const static char *bcm4330b2ag_fw_name[] = {
	"fw_bcm40183b2_ag.bin",
	"fw_bcm40183b2_ag_apsta.bin",
	"fw_bcm40183b2_ag_p2p.bin",
	"fw_bcm40183b2_ag_mfg.bin"
};

const static char *bcm43362a0_fw_name[] = {
	"fw_bcm40181a0.bin",
	"fw_bcm40181a0_apsta.bin",
	"fw_bcm40181a0_p2p.bin",
	"fw_bcm40181a0_mfg.bin"
};

const static char *bcm43362a2_fw_name[] = {
	"fw_bcm40181a2.bin",
	"fw_bcm40181a2_apsta.bin",
	"fw_bcm40181a2_p2p.bin",
	"fw_bcm40181a2_mfg.bin"
};

const static char *bcm43341b0ag_fw_name[] = {
	"fw_bcm43341b0_ag.bin",
	"fw_bcm43341b0_ag_apsta.bin",
	"fw_bcm43341b0_ag_p2p.bin",
	"fw_bcm43341b0_ag_mfg.bin"
};

const static char *bcm43241b4ag_fw_name[] = {
	"fw_bcm43241b4_ag.bin",
	"fw_bcm43241b4_ag_apsta.bin",
	"fw_bcm43241b4_ag_p2p.bin",
	"fw_bcm43241b4_ag_mfg.bin"
};

const static char *bcm4339a0ag_fw_name[] = {
	"fw_bcm4339a0_ag.bin",
	"fw_bcm4339a0_ag_apsta.bin",
	"fw_bcm4339a0_ag_p2p.bin",
	"fw_bcm4339a0_ag_mfg.bin"
};

const static char *nv_name[] = {
	"ap6210_nvram.txt",
	"ap6330_nvram.txt",
	"ap6181_nvram.txt",
	"ap6335e_nvram.txt",
	"ap62x2_nvram.txt",
};

#define htod32(i) i
#define htod16(i) i
#define dtoh32(i) i
#define dtoh16(i) i
#define htodchanspec(i) i
#define dtohchanspec(i) i

void
dhd_conf_free_mac_list(wl_mac_list_ctrl_t *mac_list)
{
	CONFIG_TRACE(("%s called\n", __FUNCTION__));

	if (mac_list->m_mac_list_head) {
		CONFIG_TRACE(("%s Free %p\n", __FUNCTION__, mac_list->m_mac_list_head));
		if (mac_list->m_mac_list_head->mac) {
			CONFIG_TRACE(("%s Free %p\n", __FUNCTION__, mac_list->m_mac_list_head->mac));
			kfree(mac_list->m_mac_list_head->mac);
		}
		kfree(mac_list->m_mac_list_head);
	}
	mac_list->count = 0;
}

int
dhd_conf_get_mac(dhd_pub_t *dhd, bcmsdh_info_t *sdh, uint8 *mac)
{
	int i, err = -1;
	uint8 *ptr = 0;
	unsigned char tpl_code, tpl_link;
	uint8 header[3] = {0x80, 0x07, 0x19};
	uint8 *cis;

	if (!(cis = MALLOC(dhd->osh, SBSDIO_CIS_SIZE_LIMIT))) {
		CONFIG_ERROR(("%s: cis malloc failed\n", __FUNCTION__));
		return err;
	}
	bzero(cis, SBSDIO_CIS_SIZE_LIMIT);

	if ((err = bcmsdh_cis_read(sdh, 0, cis, SBSDIO_CIS_SIZE_LIMIT))) {
		CONFIG_ERROR(("%s: cis read err %d\n", __FUNCTION__, err));
		MFREE(dhd->osh, cis, SBSDIO_CIS_SIZE_LIMIT);
		return err;
	}
	err = -1; // reset err;
	ptr = cis;
	do {
		/* 0xff means we're done */
		tpl_code = *ptr;
		ptr++;
		if (tpl_code == 0xff)
			break;

		/* null entries have no link field or data */
		if (tpl_code == 0x00)
			continue;

		tpl_link = *ptr;
		ptr++;
		/* a size of 0xff also means we're done */
		if (tpl_link == 0xff)
			break;
		if (config_msg_level & CONFIG_TRACE_LEVEL) {
			printf("%s: tpl_code=0x%02x, tpl_link=0x%02x, tag=0x%02x\n",
				__FUNCTION__, tpl_code, tpl_link, *ptr);
			printf("%s: value:", __FUNCTION__);
			for (i=0; i<tpl_link-1; i++) {
				printf("%02x ", ptr[i+1]);
				if ((i+1)%16==0)
					printf("\n");
			}
			printf("\n");
		}

		if (tpl_code == 0x80 && tpl_link == 0x07 && *ptr == 0x19)
			break;

		ptr += tpl_link;
	} while (1);

	if (tpl_code == 0x80 && tpl_link == 0x07 && *ptr == 0x19) {
		/* Normal OTP */
		memcpy(mac, ptr+1, 6);
		err = 0;
	} else {
		ptr = cis;
		/* Special OTP */
		if (bcmsdh_reg_read(sdh, SI_ENUM_BASE, 4) == 0x16044330) {
			for (i=0; i<SBSDIO_CIS_SIZE_LIMIT; i++) {
				if (!memcmp(header, ptr, 3)) {
					memcpy(mac, ptr+1, 6);
					err = 0;
					break;
				}
				ptr++;
			}
		}
	}

	ASSERT(cis);
	MFREE(dhd->osh, cis, SBSDIO_CIS_SIZE_LIMIT);

	return err;
}

void
dhd_conf_set_fw_name_by_mac(dhd_pub_t *dhd, bcmsdh_info_t *sdh, char *fw_path)
{
	int i, j;
	uint8 mac[6]={0};
	int fw_num=0, mac_num=0;
	uint32 oui, nic;
	wl_mac_list_t *mac_list;
	wl_mac_range_t *mac_range;
	char *pfw_name;
	int fw_type, fw_type_new;

	mac_list = dhd->conf->fw_by_mac.m_mac_list_head;
	fw_num = dhd->conf->fw_by_mac.count;
	if (!mac_list || !fw_num)
		return;

	if (dhd_conf_get_mac(dhd, sdh, mac)) {
		CONFIG_ERROR(("%s: Can not read MAC address\n", __FUNCTION__));
		return;
	}
	oui = (mac[0] << 16) | (mac[1] << 8) | (mac[2]);
	nic = (mac[3] << 16) | (mac[4] << 8) | (mac[5]);

	/* find out the last '/' */
	i = strlen(fw_path);
	while (i>0){
		if (fw_path[i] == '/') break;
		i--;
	}
	pfw_name = &fw_path[i+1];
	fw_type = (strstr(pfw_name, "_mfg") ?
		FW_TYPE_MFG : (strstr(pfw_name, "_apsta") ?
		FW_TYPE_APSTA : (strstr(pfw_name, "_p2p") ?
		FW_TYPE_P2P : FW_TYPE_STA)));

	for (i=0; i<fw_num; i++) {
		mac_num = mac_list[i].count;
		mac_range = mac_list[i].mac;
		fw_type_new = (strstr(mac_list[i].name, "_mfg") ?
			FW_TYPE_MFG : (strstr(mac_list[i].name, "_apsta") ?
			FW_TYPE_APSTA : (strstr(mac_list[i].name, "_p2p") ?
			FW_TYPE_P2P : FW_TYPE_STA)));
		if (fw_type != fw_type_new) {
			printf("%s: fw_typ=%d != fw_type_new=%d\n", __FUNCTION__, fw_type, fw_type_new);
			continue;
		}
		for (j=0; j<mac_num; j++) {
			if (oui == mac_range[j].oui) {
				if (nic >= mac_range[j].nic_start && nic <= mac_range[j].nic_end) {
					strcpy(pfw_name, mac_list[i].name);
					printf("%s: matched oui=0x%06X, nic=0x%06X\n",
						__FUNCTION__, oui, nic);
					printf("%s: fw_path=%s\n", __FUNCTION__, fw_path);
					return;
				}
			}
		}
	}
}

void
dhd_conf_set_nv_name_by_mac(dhd_pub_t *dhd, bcmsdh_info_t *sdh, char *nv_path)
{
	int i, j;
	uint8 mac[6]={0};
	int nv_num=0, mac_num=0;
	uint32 oui, nic;
	wl_mac_list_t *mac_list;
	wl_mac_range_t *mac_range;
	char *pnv_name;

	mac_list = dhd->conf->nv_by_mac.m_mac_list_head;
	nv_num = dhd->conf->nv_by_mac.count;
	if (!mac_list || !nv_num)
		return;

	if (dhd_conf_get_mac(dhd, sdh, mac)) {
		CONFIG_ERROR(("%s: Can not read MAC address\n", __FUNCTION__));
		return;
	}
	oui = (mac[0] << 16) | (mac[1] << 8) | (mac[2]);
	nic = (mac[3] << 16) | (mac[4] << 8) | (mac[5]);

	/* find out the last '/' */
	i = strlen(nv_path);
	while (i>0){
		if (nv_path[i] == '/') break;
		i--;
	}
	pnv_name = &nv_path[i+1];

	for (i=0; i<nv_num; i++) {
		mac_num = mac_list[i].count;
		mac_range = mac_list[i].mac;
		for (j=0; j<mac_num; j++) {
			if (oui == mac_range[j].oui) {
				if (nic >= mac_range[j].nic_start && nic <= mac_range[j].nic_end) {
					strcpy(pnv_name, mac_list[i].name);
					printf("%s: matched oui=0x%06X, nic=0x%06X\n",
						__FUNCTION__, oui, nic);
					printf("%s: nv_path=%s\n", __FUNCTION__, nv_path);
					return;
				}
			}
		}
	}
}
void
dhd_conf_set_nv_name_by_chip(dhd_pub_t *dhd, char *dst, char *src)
{
	int fw_type, ag_type;
	static uint chip, chiprev, first=1;
	int i;

	if (first) {
		chip = dhd_bus_chip_id(dhd);
		chiprev = dhd_bus_chiprev_id(dhd);
		first = 0;
	}

	if (src[0] == '\0') {
#ifdef CONFIG_BCMDHD_NVRAM_PATH
		bcm_strncpy_s(src, sizeof(nvram_path), CONFIG_BCMDHD_NVRAM_PATH, MOD_PARAM_PATHLEN-1);
		if (src[0] == '\0')
#endif
		{
			printf("src nvram path is null\n");
			return;
		}
	}

		strcpy(dst, src);
	#ifndef FW_PATH_AUTO_SELECT
		return;
	#endif

	/* find out the last '/' */
	i = strlen(dst);
	while (i>0){
		if (dst[i] == '/') break;
		i--;
	}
	switch (chip) {
		case BCM4330_CHIP_ID:
					strcpy(&dst[i+1], nv_name[1]);
						break;
		case BCM43362_CHIP_ID:
					strcpy(&dst[i+1], nv_name[0]);
						break;	
		case BCM43340_CHIP_ID: //bcm43341b0ag BCM43340B0
					strcpy(&dst[i+1], nv_name[0]);
						break;
//		case BCM43341_CHIP_ID: //bcm43341b0ag BCM43341B0 
//					strcpy(&dst[i+1], nv_name[0]);
//						break;
		case BCM4324_CHIP_ID: //bcm43241b4ag BCM43241B4
					strcpy(&dst[i+1], nv_name[4]);
						break;
		case BCM4335_CHIP_ID: //bcm4339a0ag BCM4335A0
					strcpy(&dst[i+1], nv_name[3]);
						break;
		case BCM4339_CHIP_ID: //bcm4339a0ag
					strcpy(&dst[i+1], nv_name[3]);
						break;
	}
	printf("%s: nvram_path=%s\n", __FUNCTION__, dst);
}


void
dhd_conf_set_fw_name_by_chip(dhd_pub_t *dhd, char *dst, char *src)
{
	int fw_type, ag_type;
	static uint chip, chiprev, first=1;
	int i;

	if (first) {
		chip = dhd_bus_chip_id(dhd);
		chiprev = dhd_bus_chiprev_id(dhd);
		first = 0;
	}

	if (src[0] == '\0') {
#ifdef CONFIG_BCMDHD_FW_PATH
		bcm_strncpy_s(src, sizeof(fw_path), CONFIG_BCMDHD_FW_PATH, MOD_PARAM_PATHLEN-1);
		if (src[0] == '\0')
#endif
		{
			printf("src firmware path is null\n");
			return;
		}
	}

	strcpy(dst, src);
#ifndef FW_PATH_AUTO_SELECT
	return;
#endif

	/* find out the last '/' */
	i = strlen(dst);
	while (i>0){
		if (dst[i] == '/') break;
		i--;
	}
#ifdef BAND_AG
	ag_type = FW_TYPE_AG;
#else
	ag_type = strstr(&dst[i], "_ag") ? FW_TYPE_AG : FW_TYPE_G;
#endif
	fw_type = (strstr(&dst[i], "_mfg") ?
		FW_TYPE_MFG : (strstr(&dst[i], "_apsta") ?
		FW_TYPE_APSTA : (strstr(&dst[i], "_p2p") ?
		FW_TYPE_P2P : FW_TYPE_STA)));

	switch (chip) {
		case BCM4330_CHIP_ID:
			if (ag_type == FW_TYPE_G) {
				if (chiprev == BCM4330B2_CHIP_REV)
					strcpy(&dst[i+1], bcm4330b2_fw_name[fw_type]);
				break;
			} else {
				if (chiprev == BCM4330B2_CHIP_REV)
					strcpy(&dst[i+1], bcm4330b2ag_fw_name[fw_type]);
				break;
			}
		case BCM43362_CHIP_ID:
			if (chiprev == BCM43362A0_CHIP_REV)
				strcpy(&dst[i+1], bcm43362a0_fw_name[fw_type]);
			else
				strcpy(&dst[i+1], bcm43362a2_fw_name[fw_type]);
			break;
		case BCM43340_CHIP_ID:
			if (chiprev == BCM43340B0_CHIP_REV)
				strcpy(&dst[i+1], bcm43341b0ag_fw_name[fw_type]);
			break;
		case BCM43341_CHIP_ID:
			if (chiprev == BCM43341B0_CHIP_REV)
				strcpy(&dst[i+1], bcm43341b0ag_fw_name[fw_type]);
			break;
		case BCM4324_CHIP_ID:
			if (chiprev == BCM43241B4_CHIP_REV)
				strcpy(&dst[i+1], bcm43241b4ag_fw_name[fw_type]);
			break;
		case BCM4335_CHIP_ID:
			if (chiprev == BCM4335A0_CHIP_REV)
				strcpy(&dst[i+1], bcm4339a0ag_fw_name[fw_type]);
			break;
		case BCM4339_CHIP_ID:
			if (chiprev == BCM4339A0_CHIP_REV)
				strcpy(&dst[i+1], bcm4339a0ag_fw_name[fw_type]);
			break;
	}

	printf("%s: firmware_path=%s\n", __FUNCTION__, dst);
}

#if defined(HW_OOB)
void
dhd_conf_set_hw_oob_intr(bcmsdh_info_t *sdh, uint chip)
{
	uint32 gpiocontrol, addr;

	if (CHIPID(chip) == BCM43362_CHIP_ID) {
		printf("%s: Enable HW OOB for 43362\n", __FUNCTION__);
		addr = SI_ENUM_BASE + OFFSETOF(chipcregs_t, gpiocontrol);
		gpiocontrol = bcmsdh_reg_read(sdh, addr, 4);
		gpiocontrol |= 0x2;
		bcmsdh_reg_write(sdh, addr, 4, gpiocontrol);
		bcmsdh_cfg_write(sdh, SDIO_FUNC_1, 0x10005, 0xf, NULL);
		bcmsdh_cfg_write(sdh, SDIO_FUNC_1, 0x10006, 0x0, NULL);
		bcmsdh_cfg_write(sdh, SDIO_FUNC_1, 0x10007, 0x2, NULL);
	}
}
#endif

void
dhd_conf_set_fw_path(dhd_pub_t *dhd, char *fw_path)
{
	if (dhd->conf->fw_path[0]) {
		strcpy(fw_path, dhd->conf->fw_path);
		printf("%s: fw_path is changed to %s\n", __FUNCTION__, fw_path);
	}
}

void
dhd_conf_set_nv_path(dhd_pub_t *dhd, char *nv_path)
{
	if (dhd->conf->nv_path[0]) {
		strcpy(nv_path, dhd->conf->nv_path);
		printf("%s: nv_path is changed to %s\n", __FUNCTION__, nv_path);
	}
}

int
dhd_conf_set_band(dhd_pub_t *dhd)
{
	int bcmerror = -1;

	printf("%s: Set band %d\n", __FUNCTION__, dhd->conf->band);
	if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_BAND, &dhd->conf->band,
		sizeof(dhd->conf->band), TRUE, 0)) < 0)
		CONFIG_ERROR(("%s: WLC_SET_BAND setting failed %d\n", __FUNCTION__, bcmerror));

	return bcmerror;
}

uint
dhd_conf_get_band(dhd_pub_t *dhd)
{
	uint band = WLC_BAND_AUTO;

	if (dhd && dhd->conf)
		band = dhd->conf->band;
	else
		CONFIG_ERROR(("%s: dhd or conf is NULL\n", __FUNCTION__));

	return band;
}

int
dhd_conf_set_country(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */

	memset(&dhd->dhd_cspec, 0, sizeof(wl_country_t));
	printf("%s: Set country %s, revision %d\n", __FUNCTION__,
		dhd->conf->cspec.ccode, dhd->conf->cspec.rev);
	bcm_mkiovar("country", (char *)&dhd->conf->cspec,
		sizeof(wl_country_t), iovbuf, sizeof(iovbuf));
	if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0)) < 0)
		printf("%s: country code setting failed %d\n", __FUNCTION__, bcmerror);

	return bcmerror;
}

int
dhd_conf_get_country(dhd_pub_t *dhd, wl_country_t *cspec)
{
	int bcmerror = -1;

	memset(cspec, 0, sizeof(wl_country_t));
	bcm_mkiovar("country", NULL, 0, (char*)cspec, sizeof(wl_country_t));
	if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_GET_VAR, cspec, sizeof(wl_country_t), FALSE, 0)) < 0)
		printf("%s: country code getting failed %d\n", __FUNCTION__, bcmerror);
	else
		printf("Country code: %s (%s/%d)\n", cspec->country_abbrev, cspec->ccode, cspec->rev);

	return bcmerror;
}

int
dhd_conf_fix_country(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	uint band;
	wl_uint32_list_t *list;
	u8 valid_chan_list[sizeof(u32)*(WL_NUMCHANNELS + 1)];

	if (!(dhd && dhd->conf)) {
		return bcmerror;
	}

	memset(valid_chan_list, 0, sizeof(valid_chan_list));
	list = (wl_uint32_list_t *)(void *) valid_chan_list;
	list->count = htod32(WL_NUMCHANNELS);
	if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_GET_VALID_CHANNELS, valid_chan_list, sizeof(valid_chan_list), FALSE, 0)) < 0) {
		CONFIG_ERROR(("%s: get channels failed with %d\n", __FUNCTION__, bcmerror));
	}

	band = dhd_conf_get_band(dhd);

	if (bcmerror || ((band==WLC_BAND_AUTO || band==WLC_BAND_2G) &&
			dtoh32(list->count)<11)) {
		CONFIG_ERROR(("%s: bcmerror=%d, # of channels %d\n",
			__FUNCTION__, bcmerror, dtoh32(list->count)));
		if ((bcmerror = dhd_conf_set_country(dhd)) < 0) {
			strcpy(dhd->conf->cspec.country_abbrev, "US");
			dhd->conf->cspec.rev = 0;
			strcpy(dhd->conf->cspec.ccode, "US");
			dhd_conf_set_country(dhd);
		}
	}

	return bcmerror;
}

bool
dhd_conf_match_channel(dhd_pub_t *dhd, uint32 channel)
{
	int i;
	bool match = false;

	if (dhd && dhd->conf) {
		if (dhd->conf->channels.count == 0)
			return true;
		for (i=0; i<dhd->conf->channels.count; i++) {
			if (channel == dhd->conf->channels.channel[i])
				match = true;
		}
	} else {
		match = true;
		CONFIG_ERROR(("%s: dhd or conf is NULL\n", __FUNCTION__));
	}

	return match;
}

int
dhd_conf_set_roam(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */

	printf("%s: Set roam_off %d\n", __FUNCTION__, dhd->conf->roam_off);
	dhd_roam_disable = dhd->conf->roam_off;
	bcm_mkiovar("roam_off", (char *)&dhd->conf->roam_off, 4, iovbuf, sizeof(iovbuf));
	dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);

	if (!dhd->conf->roam_off || !dhd->conf->roam_off_suspend) {
		printf("%s: Set roam_trigger %d\n", __FUNCTION__, dhd->conf->roam_trigger[0]);
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_ROAM_TRIGGER, dhd->conf->roam_trigger,
				sizeof(dhd->conf->roam_trigger), TRUE, 0)) < 0)
			CONFIG_ERROR(("%s: roam trigger setting failed %d\n", __FUNCTION__, bcmerror));

		printf("%s: Set roam_scan_period %d\n", __FUNCTION__, dhd->conf->roam_scan_period[0]);
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_ROAM_SCAN_PERIOD, dhd->conf->roam_scan_period,
				sizeof(dhd->conf->roam_scan_period), TRUE, 0)) < 0)
			CONFIG_ERROR(("%s: roam scan period setting failed %d\n", __FUNCTION__, bcmerror));

		printf("%s: Set roam_delta %d\n", __FUNCTION__, dhd->conf->roam_delta[0]);
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_ROAM_DELTA, dhd->conf->roam_delta,
				sizeof(dhd->conf->roam_delta), TRUE, 0)) < 0)
			CONFIG_ERROR(("%s: roam delta setting failed %d\n", __FUNCTION__, bcmerror));

		printf("%s: Set fullroamperiod %d\n", __FUNCTION__, dhd->conf->fullroamperiod);
		bcm_mkiovar("fullroamperiod", (char *)&dhd->conf->fullroamperiod, 4, iovbuf, sizeof(iovbuf));
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0)) < 0)
			CONFIG_ERROR(("%s: roam fullscan period setting failed %d\n", __FUNCTION__, bcmerror));
	}

	return bcmerror;
}

void
dhd_conf_set_mimo_bw_cap(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */
	uint32 mimo_bw_cap;
	uint chip;

	chip = dhd_bus_chip_id(dhd);
	if (chip!=BCM43362_CHIP_ID && chip!=BCM4330_CHIP_ID) {
		if (dhd->conf->mimo_bw_cap >= 0) {
			mimo_bw_cap = (uint)dhd->conf->mimo_bw_cap;
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_DOWN, NULL, 0, TRUE, 0)) < 0)
			CONFIG_ERROR(("%s: WLC_DOWN setting failed %d\n", __FUNCTION__, bcmerror));
		/*  0:HT20 in ALL, 1:HT40 in ALL, 2: HT20 in 2G HT40 in 5G */
		printf("%s: Set mimo_bw_cap %d\n", __FUNCTION__, mimo_bw_cap);
		bcm_mkiovar("mimo_bw_cap", (char *)&mimo_bw_cap, 4, iovbuf, sizeof(iovbuf));
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0)) < 0)
			CONFIG_ERROR(("%s: mimo_bw_cap setting failed %d\n", __FUNCTION__, bcmerror));
		}
	}
}

void
dhd_conf_force_wme(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */

	if (dhd_bus_chip_id(dhd) == BCM43362_CHIP_ID && dhd->conf->force_wme_ac) {
		bcm_mkiovar("force_wme_ac", (char *)&dhd->conf->force_wme_ac, 4, iovbuf, sizeof(iovbuf));
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0)) < 0)
			CONFIG_ERROR(("%s: force_wme_ac setting failed %d\n", __FUNCTION__, bcmerror));
	}
}

void
dhd_conf_get_wme(dhd_pub_t *dhd, edcf_acparam_t *acp)
{
	int bcmerror = -1;
	char iovbuf[WLC_IOCTL_SMLEN];
	edcf_acparam_t *acparam;

	bzero(iovbuf, sizeof(iovbuf));

	/*
	 * Get current acparams, using buf as an input buffer.
	 * Return data is array of 4 ACs of wme params.
	 */
	bcm_mkiovar("wme_ac_sta", NULL, 0, iovbuf, sizeof(iovbuf));
	if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_GET_VAR, iovbuf, sizeof(iovbuf), FALSE, 0)) < 0) {
		CONFIG_ERROR(("%s: wme_ac_sta getting failed %d\n", __FUNCTION__, bcmerror));
		return;
	}
	memcpy((char*)acp, iovbuf, sizeof(edcf_acparam_t)*AC_COUNT);

	acparam = &acp[AC_BK];
	CONFIG_TRACE(("%s: BK: aci %d aifsn %d ecwmin %d ecwmax %d size %d\n", __FUNCTION__,
		acparam->ACI, acparam->ACI&EDCF_AIFSN_MASK,
		acparam->ECW&EDCF_ECWMIN_MASK, (acparam->ECW&EDCF_ECWMAX_MASK)>>EDCF_ECWMAX_SHIFT,
		sizeof(acp)));
	acparam = &acp[AC_BE];
	CONFIG_TRACE(("%s: BE: aci %d aifsn %d ecwmin %d ecwmax %d size %d\n", __FUNCTION__,
		acparam->ACI, acparam->ACI&EDCF_AIFSN_MASK,
		acparam->ECW&EDCF_ECWMIN_MASK, (acparam->ECW&EDCF_ECWMAX_MASK)>>EDCF_ECWMAX_SHIFT,
		sizeof(acp)));
	acparam = &acp[AC_VI];
	CONFIG_TRACE(("%s: VI: aci %d aifsn %d ecwmin %d ecwmax %d size %d\n", __FUNCTION__,
		acparam->ACI, acparam->ACI&EDCF_AIFSN_MASK,
		acparam->ECW&EDCF_ECWMIN_MASK, (acparam->ECW&EDCF_ECWMAX_MASK)>>EDCF_ECWMAX_SHIFT,
		sizeof(acp)));
	acparam = &acp[AC_VO];
	CONFIG_TRACE(("%s: VO: aci %d aifsn %d ecwmin %d ecwmax %d size %d\n", __FUNCTION__,
		acparam->ACI, acparam->ACI&EDCF_AIFSN_MASK,
		acparam->ECW&EDCF_ECWMIN_MASK, (acparam->ECW&EDCF_ECWMAX_MASK)>>EDCF_ECWMAX_SHIFT,
		sizeof(acp)));

	return;
}

void
dhd_conf_update_wme(dhd_pub_t *dhd, edcf_acparam_t *acparam_cur, int aci)
{
	int bcmerror = -1;
	int aifsn, ecwmin, ecwmax;
	edcf_acparam_t *acp;
	char iovbuf[WLC_IOCTL_SMLEN];

	/* Default value */
	aifsn = acparam_cur->ACI&EDCF_AIFSN_MASK;
	ecwmin = acparam_cur->ECW&EDCF_ECWMIN_MASK;
	ecwmax = (acparam_cur->ECW&EDCF_ECWMAX_MASK)>>EDCF_ECWMAX_SHIFT;

	/* Modified value */
	if (dhd->conf->wme.aifsn[aci] > 0)
		aifsn = dhd->conf->wme.aifsn[aci];
	if (dhd->conf->wme.cwmin[aci] > 0)
		ecwmin = dhd->conf->wme.cwmin[aci];
	if (dhd->conf->wme.cwmax[aci] > 0)
		ecwmax = dhd->conf->wme.cwmax[aci];

	/* Update */
	acp = acparam_cur;
	acp->ACI = (acp->ACI & ~EDCF_AIFSN_MASK) | (aifsn & EDCF_AIFSN_MASK);
	acp->ECW = ((ecwmax << EDCF_ECWMAX_SHIFT) & EDCF_ECWMAX_MASK) | (acp->ECW & EDCF_ECWMIN_MASK);
	acp->ECW = ((acp->ECW & EDCF_ECWMAX_MASK) | (ecwmin & EDCF_ECWMIN_MASK));

	CONFIG_TRACE(("%s: mod aci %d aifsn %d ecwmin %d ecwmax %d size %d\n", __FUNCTION__,
		acp->ACI, acp->ACI&EDCF_AIFSN_MASK,
		acp->ECW&EDCF_ECWMIN_MASK, (acp->ECW&EDCF_ECWMAX_MASK)>>EDCF_ECWMAX_SHIFT,
		sizeof(edcf_acparam_t)));

	/*
	* Now use buf as an output buffer.
	* Put WME acparams after "wme_ac\0" in buf.
	* NOTE: only one of the four ACs can be set at a time.
	*/
	bcm_mkiovar("wme_ac_sta", (char*)acp, sizeof(edcf_acparam_t), iovbuf,
		sizeof(iovbuf));
	if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), FALSE, 0)) < 0) {
		CONFIG_ERROR(("%s: wme_ac_sta setting failed %d\n", __FUNCTION__, bcmerror));
		return;
	}
}

void
dhd_conf_set_wme(dhd_pub_t *dhd)
{
	edcf_acparam_t acparam_cur[AC_COUNT];

	if (dhd && dhd->conf) {
		if (!dhd->conf->force_wme_ac) {
			CONFIG_TRACE(("%s: force_wme_ac is not enabled %d\n",
				__FUNCTION__, dhd->conf->force_wme_ac));
			return;
		}

		CONFIG_TRACE(("%s: Before change:\n", __FUNCTION__));
		dhd_conf_get_wme(dhd, acparam_cur);

		dhd_conf_update_wme(dhd, &acparam_cur[AC_BK], AC_BK);
		dhd_conf_update_wme(dhd, &acparam_cur[AC_BE], AC_BE);
		dhd_conf_update_wme(dhd, &acparam_cur[AC_VI], AC_VI);
		dhd_conf_update_wme(dhd, &acparam_cur[AC_VO], AC_VO);

		CONFIG_TRACE(("%s: After change:\n", __FUNCTION__));
		dhd_conf_get_wme(dhd, acparam_cur);
	} else {
		CONFIG_ERROR(("%s: dhd or conf is NULL\n", __FUNCTION__));
	}

	return;
}

void
dhd_conf_set_stbc(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */
	uint stbc = 0;

	if (dhd_bus_chip_id(dhd) == BCM4324_CHIP_ID) {
		if (dhd->conf->stbc >= 0) {
			stbc = (uint)dhd->conf->stbc;
			printf("%s: set stbc_tx %d\n", __FUNCTION__, stbc);
			bcm_mkiovar("stbc_tx", (char *)&stbc, 4, iovbuf, sizeof(iovbuf));
			if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0)) < 0)
				CONFIG_ERROR(("%s: stbc_tx setting failed %d\n", __FUNCTION__, bcmerror));

			printf("%s: set stbc_rx %d\n", __FUNCTION__, stbc);
			bcm_mkiovar("stbc_rx", (char *)&stbc, 4, iovbuf, sizeof(iovbuf));
			if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0)) < 0)
				CONFIG_ERROR(("%s: stbc_rx setting failed %d\n", __FUNCTION__, bcmerror));
		}
	}
}

void
dhd_conf_set_phyoclscdenable(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */
	uint phy_oclscdenable = 0;

	if (dhd_bus_chip_id(dhd) == BCM4324_CHIP_ID) {
		if (dhd->conf->phy_oclscdenable >= 0) {
			phy_oclscdenable = (uint)dhd->conf->phy_oclscdenable;
			printf("%s: set stbc_tx %d\n", __FUNCTION__, phy_oclscdenable);
			bcm_mkiovar("phy_oclscdenable", (char *)&phy_oclscdenable, 4, iovbuf, sizeof(iovbuf));
			if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0)) < 0)
				CONFIG_ERROR(("%s: stbc_tx setting failed %d\n", __FUNCTION__, bcmerror));
		}
	}
}

#ifdef PKT_FILTER_SUPPORT
void
dhd_conf_add_pkt_filter(dhd_pub_t *dhd)
{
	int i;

	/*
	All pkt: pkt_filter_add=99 0 0 0 0x000000000000 0xFFFFFFFFFFFF
	Netbios pkt: 120 0 0 12 0xFFFF000000000000000000FF00000000000000000000000000FF 0x0800000000000000000000110000000000000000000000000089
	*/
	for(i=0; i<dhd->conf->pkt_filter_add.count; i++) {
		dhd->pktfilter[i+dhd->pktfilter_count] = dhd->conf->pkt_filter_add.filter[i];
		printf("%s: %s\n", __FUNCTION__, dhd->pktfilter[i+dhd->pktfilter_count]);
	}
	dhd->pktfilter_count += i;
}

bool
dhd_conf_del_pkt_filter(dhd_pub_t *dhd, uint32 id)
{
	int i;

	for(i=0; i<dhd->conf->pkt_filter_del.count; i++) {
		if (id == dhd->conf->pkt_filter_del.id[i]) {
			printf("%s: %d\n", __FUNCTION__, dhd->conf->pkt_filter_del.id[i]);
			return true;
		}
	}
	return false;
}

void
dhd_conf_discard_pkt_filter(dhd_pub_t *dhd)
{
	dhd->pktfilter[DHD_UNICAST_FILTER_NUM] = NULL;
	dhd->pktfilter[DHD_BROADCAST_FILTER_NUM] = "101 0 0 0 0xFFFFFFFFFFFF 0xFFFFFFFFFFFF";
	dhd->pktfilter[DHD_MULTICAST4_FILTER_NUM] = "102 0 0 0 0xFFFFFF 0x01005E";
	dhd->pktfilter[DHD_MULTICAST6_FILTER_NUM] = "103 0 0 0 0xFFFF 0x3333";
	dhd->pktfilter[DHD_MDNS_FILTER_NUM] = NULL;
	/* Do not enable ARP to pkt filter if dhd_master_mode is false.*/
	dhd->pktfilter[DHD_ARP_FILTER_NUM] = NULL;

	/* IPv4 broadcast address XXX.XXX.XXX.255 */
	dhd->pktfilter[dhd->pktfilter_count] = "110 0 0 12 0xFFFF00000000000000000000000000000000000000FF 0x080000000000000000000000000000000000000000FF";
	dhd->pktfilter_count++;
	/* discard IPv4 multicast address 224.0.0.0/4 */
	dhd->pktfilter[dhd->pktfilter_count] = "111 0 0 12 0xFFFF00000000000000000000000000000000F0 0x080000000000000000000000000000000000E0";
	dhd->pktfilter_count++;
	/* discard IPv6 multicast address FF00::/8 */
	dhd->pktfilter[dhd->pktfilter_count] = "112 0 0 12 0xFFFF000000000000000000000000000000000000000000000000FF 0x86DD000000000000000000000000000000000000000000000000FF";
	dhd->pktfilter_count++;
	/* discard Netbios pkt */
	dhd->pktfilter[dhd->pktfilter_count] = "120 0 0 12 0xFFFF000000000000000000FF000000000000000000000000FFFF 0x0800000000000000000000110000000000000000000000000089";
	dhd->pktfilter_count++;

}
#endif /* PKT_FILTER_SUPPORT */

void
dhd_conf_set_srl(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	uint srl = 0;

	if (dhd->conf->srl >= 0) {
		srl = (uint)dhd->conf->srl;
		printf("%s: set srl %d\n", __FUNCTION__, srl);
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_SRL, &srl , sizeof(srl), true, 0)) < 0)
			CONFIG_ERROR(("%s: WLC_SET_SRL setting failed %d\n", __FUNCTION__, bcmerror));
	}
}

void
dhd_conf_set_lrl(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	uint lrl = 0;

	if (dhd->conf->lrl >= 0) {
		lrl = (uint)dhd->conf->lrl;
		printf("%s: set lrl %d\n", __FUNCTION__, lrl);
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_LRL, &lrl , sizeof(lrl), true, 0)) < 0)
			CONFIG_ERROR(("%s: WLC_SET_LRL setting failed %d\n", __FUNCTION__, bcmerror));
	}
}

void
dhd_conf_set_glom(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */
	uint32 bus_txglom = 0;

	if (dhd->conf->bus_txglom) {
		bus_txglom = (uint)dhd->conf->bus_txglom;
		printf("%s: set bus:txglom %d\n", __FUNCTION__, bus_txglom);
		bcm_mkiovar("bus:txglom", (char *)&bus_txglom, 4, iovbuf, sizeof(iovbuf));
		dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0)) < 0)
			CONFIG_ERROR(("%s: bus:txglom setting failed %d\n", __FUNCTION__, bcmerror));
	}
}

void
dhd_conf_set_ampdu_ba_wsize(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */
	uint32 ampdu_ba_wsize = dhd->conf->ampdu_ba_wsize;

	/* Set ampdu ba wsize */
	if (dhd_bus_chip_id(dhd) == BCM4339_CHIP_ID && ampdu_ba_wsize > 0) {
		printf("%s: set ampdu_ba_wsize %d\n", __FUNCTION__, ampdu_ba_wsize);
		bcm_mkiovar("ampdu_ba_wsize", (char *)&ampdu_ba_wsize, 4, iovbuf, sizeof(iovbuf));
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf,
				sizeof(iovbuf), TRUE, 0)) < 0) {
			DHD_ERROR(("%s Set ampdu_ba_wsize to %d failed	%d\n",
				__FUNCTION__, ampdu_ba_wsize, bcmerror));
		}
	}
}

void
dhd_conf_set_spect(dhd_pub_t *dhd)
{
	int bcmerror = -1;
	int spect = 0;

	if (dhd->conf->spect >= 0) {
		spect = (uint)dhd->conf->spect;
		printf("%s: set spect %d\n", __FUNCTION__, spect);
		if ((bcmerror = dhd_wl_ioctl_cmd(dhd, WLC_SET_SPECT_MANAGMENT, &spect , sizeof(spect), true, 0)) < 0)
			CONFIG_ERROR(("%s: WLC_SET_SPECT_MANAGMENT setting failed %d\n", __FUNCTION__, bcmerror));
	}
}

unsigned int
process_config_vars(char *varbuf, unsigned int len, char *pickbuf, char *param)
{
	bool findNewline, changenewline=FALSE, pick=FALSE;
	int column;
	unsigned int n, pick_column=0;

	findNewline = FALSE;
	column = 0;

	for (n = 0; n < len; n++) {
		if (varbuf[n] == '\r')
			continue;
		if ((findNewline || changenewline) && varbuf[n] != '\n')
			continue;
		findNewline = FALSE;
		if (varbuf[n] == '#') {
			findNewline = TRUE;
			continue;
		}
		if (varbuf[n] == '\\') {
			changenewline = TRUE;
			continue;
		}
		if (!changenewline && varbuf[n] == '\n') {
			if (column == 0)
				continue;
			column = 0;
			continue;
		}
		if (changenewline && varbuf[n] == '\n') {
			changenewline = FALSE;
			continue;
		}
		if (!memcmp(&varbuf[n], param, strlen(param)) && column==0) {
			pick = TRUE;
			column = strlen(param);
			n += column;
			pick_column = 0;
		} else {
			if (pick && column==0)
				pick = FALSE;
			else
				column++;
		}
		if (pick) {
			if (varbuf[n] == 0x9)
				continue;
			if (pick_column>0 && pickbuf[pick_column-1]==' ' && varbuf[n]==' ')
				continue;
			pickbuf[pick_column] = varbuf[n];
			pick_column++;
		}
	}

	return pick_column;
}

void
dhd_conf_read_log_level(dhd_pub_t *dhd, char *bufp, uint len)
{
	uint len_val;
	char pick[MAXSZ_BUF];

	/* Process dhd_msglevel */
	memset(pick, 0, MAXSZ_BUF);
	len_val = process_config_vars(bufp, len, pick, "msglevel=");
	if (len_val) {
		dhd_msg_level = (int)simple_strtol(pick, NULL, 0);
		printf("%s: dhd_msg_level = 0x%X\n", __FUNCTION__, dhd_msg_level);
	}
	/* Process sd_msglevel */
	memset(pick, 0, MAXSZ_BUF);
	len_val = process_config_vars(bufp, len, pick, "sd_msglevel=");
	if (len_val) {
		sd_msglevel = (int)simple_strtol(pick, NULL, 0);
		printf("%s: sd_msglevel = 0x%X\n", __FUNCTION__, sd_msglevel);
	}
	/* Process android_msg_level */
	memset(pick, 0, MAXSZ_BUF);
	len_val = process_config_vars(bufp, len, pick, "android_msg_level=");
	if (len_val) {
		android_msg_level = (int)simple_strtol(pick, NULL, 0);
		printf("%s: android_msg_level = 0x%X\n", __FUNCTION__, android_msg_level);
	}
	/* Process config_msg_level */
	memset(pick, 0, MAXSZ_BUF);
	len_val = process_config_vars(bufp, len, pick, "config_msg_level=");
	if (len_val) {
		config_msg_level = (int)simple_strtol(pick, NULL, 0);
		printf("%s: config_msg_level = 0x%X\n", __FUNCTION__, config_msg_level);
	}
#ifdef WL_CFG80211
	/* Process wl_dbg_level */
	memset(pick, 0, MAXSZ_BUF);
	len_val = process_config_vars(bufp, len, pick, "wl_dbg_level=");
	if (len_val) {
		wl_dbg_level = (int)simple_strtol(pick, NULL, 0);
		printf("%s: wl_dbg_level = 0x%X\n", __FUNCTION__, wl_dbg_level);
	}
#endif
#if defined(WL_WIRELESS_EXT)
	/* Process iw_msg_level */
	memset(pick, 0, MAXSZ_BUF);
	len_val = process_config_vars(bufp, len, pick, "iw_msg_level=");
	if (len_val) {
		iw_msg_level = (int)simple_strtol(pick, NULL, 0);
		printf("%s: iw_msg_level = 0x%X\n", __FUNCTION__, iw_msg_level);
	}
#endif
}

/*
 * [fw_by_mac]:
 * fw_by_mac=[fw_mac_num] \
 *  [fw_name1] [mac_num1] [oui1-1] [nic_start1-1] [nic_end1-1] \
 *                                    [oui1-1] [nic_start1-1] [nic_end1-1]... \
 *                                    [oui1-n] [nic_start1-n] [nic_end1-n] \
 *  [fw_name2] [mac_num2] [oui2-1] [nic_start2-1] [nic_end2-1] \
 *                                    [oui2-1] [nic_start2-1] [nic_end2-1]... \
 *                                    [oui2-n] [nic_start2-n] [nic_end2-n] \
 * Ex: fw_by_mac=2 \
 *  fw_bcmdhd1.bin 2 0x0022F4 0xE85408 0xE8549D 0x983B16 0x3557A9 0x35582A \
 *  fw_bcmdhd2.bin 3 0x0022F4 0xE85408 0xE8549D 0x983B16 0x3557A9 0x35582A \
 *                           0x983B16 0x916157 0x916487
 * [nv_by_mac]: The same format as fw_by_mac
 *
*/

int
dhd_conf_read_config(dhd_pub_t *dhd)
{
	int bcmerror = -1, i, j;
	uint len, len_val;
	void * image = NULL;
	char * memblock = NULL;
	char *bufp, *pick=NULL, *pch, *pick_tmp;
	char *pconf_path;
	bool conf_file_exists;
	wl_mac_list_t *mac_list;
	wl_mac_range_t *mac_range;

	pick = kmalloc(MAXSZ_BUF, GFP_KERNEL);
	if(!pick){
		printk("kmalloc pick error \n");
		goto err;
	}
	memset(pick, 0, MAXSZ_BUF);
	pconf_path = dhd->conf_path;

	conf_file_exists = ((pconf_path != NULL) && (pconf_path[0] != '\0'));
	if (!conf_file_exists)
		return (0);

	if (conf_file_exists) {
		image = dhd_os_open_image(pconf_path);
		if (image == NULL) {
			printk("%s: Ignore config file %s\n", __FUNCTION__, pconf_path);
			goto err;
		}
	}

	memblock = MALLOC(dhd->osh, MAXSZ_CONFIG);
	if (memblock == NULL) {
		CONFIG_ERROR(("%s: Failed to allocate memory %d bytes\n",
			__FUNCTION__, MAXSZ_CONFIG));
		goto err;
	}

	/* Read variables */
	if (conf_file_exists) {
		len = dhd_os_get_image_block(memblock, MAXSZ_CONFIG, image);
	}
	if (len > 0 && len < MAXSZ_CONFIG) {
		bufp = (char *)memblock;
		bufp[len] = 0;

		/* Process log_level */
		dhd_conf_read_log_level(dhd, bufp, len);

		/* Process fw_by_mac */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "fw_by_mac=");
		if (len_val) {
			pick_tmp = pick;
			pch = bcmstrtok(&pick_tmp, " ", 0);
			dhd->conf->fw_by_mac.count = (uint32)simple_strtol(pch, NULL, 0);
			if (!(mac_list = kmalloc(sizeof(wl_mac_list_t)*dhd->conf->fw_by_mac.count, GFP_KERNEL))) {
				dhd->conf->fw_by_mac.count = 0;
				CONFIG_ERROR(("%s: kmalloc failed\n", __FUNCTION__));
			}
			printf("%s: fw_count=%d\n", __FUNCTION__, dhd->conf->fw_by_mac.count);
			dhd->conf->fw_by_mac.m_mac_list_head = mac_list;
			for (i=0; i<dhd->conf->fw_by_mac.count; i++) {
				pch = bcmstrtok(&pick_tmp, " ", 0);
				strcpy(mac_list[i].name, pch);
				pch = bcmstrtok(&pick_tmp, " ", 0);
				mac_list[i].count = (uint32)simple_strtol(pch, NULL, 0);
				printf("%s: name=%s, mac_count=%d\n", __FUNCTION__,
					mac_list[i].name, mac_list[i].count);
				if (!(mac_range = kmalloc(sizeof(wl_mac_range_t)*mac_list[i].count, GFP_KERNEL))) {
					mac_list[i].count = 0;
					CONFIG_ERROR(("%s: kmalloc failed\n", __FUNCTION__));
					break;
				}
				mac_list[i].mac = mac_range;
				for (j=0; j<mac_list[i].count; j++) {
					pch = bcmstrtok(&pick_tmp, " ", 0);
					mac_range[j].oui = (uint32)simple_strtol(pch, NULL, 0);
					pch = bcmstrtok(&pick_tmp, " ", 0);
					mac_range[j].nic_start = (uint32)simple_strtol(pch, NULL, 0);
					pch = bcmstrtok(&pick_tmp, " ", 0);
					mac_range[j].nic_end = (uint32)simple_strtol(pch, NULL, 0);
					printf("%s: oui=0x%06X, nic_start=0x%06X, nic_end=0x%06X\n",
						__FUNCTION__, mac_range[j].oui,
						mac_range[j].nic_start, mac_range[j].nic_end);
				}
			}
		}

		/* Process nv_by_mac */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "nv_by_mac=");
		if (len_val) {
			pick_tmp = pick;
			pch = bcmstrtok(&pick_tmp, " ", 0);
			dhd->conf->nv_by_mac.count = (uint32)simple_strtol(pch, NULL, 0);
			if (!(mac_list = kmalloc(sizeof(wl_mac_list_t)*dhd->conf->nv_by_mac.count, GFP_KERNEL))) {
				dhd->conf->nv_by_mac.count = 0;
				CONFIG_ERROR(("%s: kmalloc failed\n", __FUNCTION__));
			}
			printf("%s: nv_count=%d\n", __FUNCTION__, dhd->conf->nv_by_mac.count);
			dhd->conf->nv_by_mac.m_mac_list_head = mac_list;
			for (i=0; i<dhd->conf->nv_by_mac.count; i++) {
				pch = bcmstrtok(&pick_tmp, " ", 0);
				strcpy(mac_list[i].name, pch);
				pch = bcmstrtok(&pick_tmp, " ", 0);
				mac_list[i].count = (uint32)simple_strtol(pch, NULL, 0);
				printf("%s: name=%s, mac_count=%d\n", __FUNCTION__,
					mac_list[i].name, mac_list[i].count);
				if (!(mac_range = kmalloc(sizeof(wl_mac_range_t)*mac_list[i].count, GFP_KERNEL))) {
					mac_list[i].count = 0;
					CONFIG_ERROR(("%s: kmalloc failed\n", __FUNCTION__));
					break;
				}
				mac_list[i].mac = mac_range;
				for (j=0; j<mac_list[i].count; j++) {
					pch = bcmstrtok(&pick_tmp, " ", 0);
					mac_range[j].oui = (uint32)simple_strtol(pch, NULL, 0);
					pch = bcmstrtok(&pick_tmp, " ", 0);
					mac_range[j].nic_start = (uint32)simple_strtol(pch, NULL, 0);
					pch = bcmstrtok(&pick_tmp, " ", 0);
					mac_range[j].nic_end = (uint32)simple_strtol(pch, NULL, 0);
					printf("%s: oui=0x%06X, nic_start=0x%06X, nic_end=0x%06X\n",
						__FUNCTION__, mac_range[j].oui,
						mac_range[j].nic_start, mac_range[j].nic_end);
				}
			}
		}

		/* Process firmware path */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "fw_path=");
		if (len_val) {
			memcpy(dhd->conf->fw_path, pick, len_val);
			printf("%s: fw_path = %s\n", __FUNCTION__, dhd->conf->fw_path);
		}

		/* Process nvram path */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "nv_path=");
		if (len_val) {
			memcpy(dhd->conf->nv_path, pick, len_val);
			printf("%s: nv_path = %s\n", __FUNCTION__, dhd->conf->nv_path);
		}

		/* Process band */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "band=");
		if (len_val) {
			if (!strncmp(pick, "b", len_val))
				dhd->conf->band = WLC_BAND_2G;
			else if (!strncmp(pick, "a", len_val))
				dhd->conf->band = WLC_BAND_5G;
			else
				dhd->conf->band = WLC_BAND_AUTO;
			printf("%s: band = %d\n", __FUNCTION__, dhd->conf->band);
		}

		/* Process bandwidth */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "mimo_bw_cap=");
		if (len_val) {
			dhd->conf->mimo_bw_cap = (uint)simple_strtol(pick, NULL, 10);
			printf("%s: mimo_bw_cap = %d\n", __FUNCTION__, dhd->conf->mimo_bw_cap);
		}

		/* Process country code */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "ccode=");
		if (len_val) {
			memset(&dhd->conf->cspec, 0, sizeof(wl_country_t));
			memcpy(dhd->conf->cspec.country_abbrev, pick, len_val);
			memcpy(dhd->conf->cspec.ccode, pick, len_val);
			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "regrev=");
			if (len_val)
				dhd->conf->cspec.rev = (int32)simple_strtol(pick, NULL, 10);
		}

		/* Process channels */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "channels=");
		pick_tmp = pick;
		if (len_val) {
			pch = bcmstrtok(&pick_tmp, " ,.-", 0);
			i=0;
			while (pch != NULL && i<WL_NUMCHANNELS) {
				dhd->conf->channels.channel[i] = (uint32)simple_strtol(pch, NULL, 10);
				pch = bcmstrtok(&pick_tmp, " ,.-", 0);
				i++;
			}
			dhd->conf->channels.count = i;
			printf("%s: channels = ", __FUNCTION__);
			for (i=0; i<dhd->conf->channels.count; i++)
				printf("%d ", dhd->conf->channels.channel[i]);
			printf("\n");
		}

		/* Process roam */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "roam_off=");
		if (len_val) {
			if (!strncmp(pick, "0", len_val))
				dhd->conf->roam_off = 0;
			else
				dhd->conf->roam_off = 1;
			printf("%s: roam_off = %d\n", __FUNCTION__, dhd->conf->roam_off);
		}

		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "roam_off_suspend=");
		if (len_val) {
			if (!strncmp(pick, "0", len_val))
				dhd->conf->roam_off_suspend = 0;
			else
				dhd->conf->roam_off_suspend = 1;
			printf("%s: roam_off_suspend = %d\n", __FUNCTION__,
				dhd->conf->roam_off_suspend);
		}

		if (!dhd->conf->roam_off || !dhd->conf->roam_off_suspend) {
			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "roam_trigger=");
			if (len_val)
				dhd->conf->roam_trigger[0] = (int)simple_strtol(pick, NULL, 10);
			printf("%s: roam_trigger = %d\n", __FUNCTION__,
				dhd->conf->roam_trigger[0]);

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "roam_scan_period=");
			if (len_val)
				dhd->conf->roam_scan_period[0] = (int)simple_strtol(pick, NULL, 10);
			printf("%s: roam_scan_period = %d\n", __FUNCTION__,
				dhd->conf->roam_scan_period[0]);

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "roam_delta=");
			if (len_val)
				dhd->conf->roam_delta[0] = (int)simple_strtol(pick, NULL, 10);
			printf("%s: roam_delta = %d\n", __FUNCTION__, dhd->conf->roam_delta[0]);

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "fullroamperiod=");
			if (len_val)
				dhd->conf->fullroamperiod = (int)simple_strtol(pick, NULL, 10);
			printf("%s: fullroamperiod = %d\n", __FUNCTION__,
				dhd->conf->fullroamperiod);
		}

		/* Process keep alive period */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "keep_alive_period=");
		if (len_val) {
			dhd->conf->keep_alive_period = (int)simple_strtol(pick, NULL, 10);
			printf("%s: keep_alive_period = %d\n", __FUNCTION__,
				dhd->conf->keep_alive_period);
		}

		/* Process WMM parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "force_wme_ac=");
		if (len_val) {
			dhd->conf->force_wme_ac = (int)simple_strtol(pick, NULL, 10);
			printf("%s: force_wme_ac = %d\n", __FUNCTION__, dhd->conf->force_wme_ac);
		}

		if (dhd->conf->force_wme_ac) {
			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "bk_aifsn=");
			if (len_val) {
				dhd->conf->wme.aifsn[AC_BK] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_BK aifsn = %d\n", __FUNCTION__, dhd->conf->wme.aifsn[AC_BK]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "bk_cwmin=");
			if (len_val) {
				dhd->conf->wme.cwmin[AC_BK] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_BK cwmin = %d\n", __FUNCTION__, dhd->conf->wme.cwmin[AC_BK]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "bk_cwmax=");
			if (len_val) {
				dhd->conf->wme.cwmax[AC_BK] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_BK cwmax = %d\n", __FUNCTION__, dhd->conf->wme.cwmax[AC_BK]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "be_aifsn=");
			if (len_val) {
				dhd->conf->wme.aifsn[AC_BE] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_BE aifsn = %d\n", __FUNCTION__, dhd->conf->wme.aifsn[AC_BE]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "be_cwmin=");
			if (len_val) {
				dhd->conf->wme.cwmin[AC_BE] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_BE cwmin = %d\n", __FUNCTION__, dhd->conf->wme.cwmin[AC_BE]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "be_cwmax=");
			if (len_val) {
				dhd->conf->wme.cwmax[AC_BE] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_BE cwmax = %d\n", __FUNCTION__, dhd->conf->wme.cwmax[AC_BE]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "vi_aifsn=");
			if (len_val) {
				dhd->conf->wme.aifsn[AC_VI] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_VI aifsn = %d\n", __FUNCTION__, dhd->conf->wme.aifsn[AC_VI]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "vi_cwmin=");
			if (len_val) {
				dhd->conf->wme.cwmin[AC_VI] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_VI cwmin = %d\n", __FUNCTION__, dhd->conf->wme.cwmin[AC_VI]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "vi_cwmax=");
			if (len_val) {
				dhd->conf->wme.cwmax[AC_VI] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_VI cwmax = %d\n", __FUNCTION__, dhd->conf->wme.cwmax[AC_VI]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "vo_aifsn=");
			if (len_val) {
				dhd->conf->wme.aifsn[AC_VO] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_VO aifsn = %d\n", __FUNCTION__, dhd->conf->wme.aifsn[AC_VO]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "vo_cwmin=");
			if (len_val) {
				dhd->conf->wme.cwmin[AC_VO] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_VO cwmin = %d\n", __FUNCTION__, dhd->conf->wme.cwmin[AC_VO]);
			}

			memset(pick, 0, MAXSZ_BUF);
			len_val = process_config_vars(bufp, len, pick, "vo_cwmax=");
			if (len_val) {
				dhd->conf->wme.cwmax[AC_VO] = (int)simple_strtol(pick, NULL, 10);
				printf("%s: AC_VO cwmax = %d\n", __FUNCTION__, dhd->conf->wme.cwmax[AC_VO]);
			}
		}

		/* Process STBC parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "stbc=");
		if (len_val) {
			dhd->conf->stbc = (int)simple_strtol(pick, NULL, 10);
			printf("%s: stbc = %d\n", __FUNCTION__, dhd->conf->stbc);
		}

		/* Process phy_oclscdenable parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "phy_oclscdenable=");
		if (len_val) {
			dhd->conf->phy_oclscdenable = (int)simple_strtol(pick, NULL, 10);
			printf("%s: phy_oclscdenable = %d\n", __FUNCTION__, dhd->conf->phy_oclscdenable);
		}

		/* Process dhd_doflow parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "dhd_doflow=");
		if (len_val) {
			if (!strncmp(pick, "0", len_val))
				dhd_doflow = FALSE;
			else
				dhd_doflow = TRUE;
			printf("%s: dhd_doflow = %d\n", __FUNCTION__, dhd_doflow);
		}

		/* Process dhd_master_mode parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "dhd_master_mode=");
		if (len_val) {
			if (!strncmp(pick, "0", len_val))
				dhd_master_mode = FALSE;
			else
				dhd_master_mode = TRUE;
			printf("%s: dhd_master_mode = %d\n", __FUNCTION__, dhd_master_mode);
		}

		/* Process pkt_filter_add */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "pkt_filter_add=");
		pick_tmp = pick;
		if (len_val) {
			pch = bcmstrtok(&pick_tmp, ",.-", 0);
			i=0;
			while (pch != NULL && i<DHD_CONF_FILTER_MAX) {
				strcpy(&dhd->conf->pkt_filter_add.filter[i][0], pch);
				printf("%s: pkt_filter_add[%d][] = %s\n", __FUNCTION__, i, &dhd->conf->pkt_filter_add.filter[i][0]);
				pch = bcmstrtok(&pick_tmp, ",.-", 0);
				i++;
			}
			dhd->conf->pkt_filter_add.count = i;
		}

		/* Process pkt_filter_del */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "pkt_filter_del=");
		pick_tmp = pick;
		if (len_val) {
			pch = bcmstrtok(&pick_tmp, " ,.-", 0);
			i=0;
			while (pch != NULL && i<DHD_CONF_FILTER_MAX) {
				dhd->conf->pkt_filter_del.id[i] = (uint32)simple_strtol(pch, NULL, 10);
				pch = bcmstrtok(&pick_tmp, " ,.-", 0);
				i++;
			}
			dhd->conf->pkt_filter_del.count = i;
			printf("%s: pkt_filter_del id = ", __FUNCTION__);
			for (i=0; i<dhd->conf->pkt_filter_del.count; i++)
				printf("%d ", dhd->conf->pkt_filter_del.id[i]);
			printf("\n");
		}

		/* Process srl parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "srl=");
		if (len_val) {
			dhd->conf->srl = (int)simple_strtol(pick, NULL, 10);
			printf("%s: srl = %d\n", __FUNCTION__, dhd->conf->srl);
		}

		/* Process lrl parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "lrl=");
		if (len_val) {
			dhd->conf->lrl = (int)simple_strtol(pick, NULL, 10);
			printf("%s: lrl = %d\n", __FUNCTION__, dhd->conf->lrl);
		}

		/* Process beacon timeout parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "bcn_timeout=");
		if (len_val) {
			dhd->conf->bcn_timeout= (int)simple_strtol(pick, NULL, 10);
			printf("%s: bcn_timeout = %d\n", __FUNCTION__, dhd->conf->bcn_timeout);
		}

		/* Process bus_txglom */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "bus_txglom=");
		if (len_val) {
			dhd->conf->bus_txglom = (int)simple_strtol(pick, NULL, 10);
			printf("%s: bus_txglom = %d\n", __FUNCTION__, dhd->conf->bus_txglom);
		}

		/* Process ampdu_ba_wsize parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "ampdu_ba_wsize=");
		if (len_val) {
			dhd->conf->ampdu_ba_wsize = (int)simple_strtol(pick, NULL, 10);
			printf("%s: ampdu_ba_wsize = %d\n", __FUNCTION__, dhd->conf->ampdu_ba_wsize);
		}

		/* Process kso parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "kso_enable=");
		if (len_val) {
			if (!strncmp(pick, "0", len_val))
				dhd->conf->kso_enable = FALSE;
			else
				dhd->conf->kso_enable = TRUE;
			printf("%s: kso_enable = %d\n", __FUNCTION__, dhd->conf->kso_enable);
		}

		/* Process spect parameters */
		memset(pick, 0, MAXSZ_BUF);
		len_val = process_config_vars(bufp, len, pick, "spect=");
		if (len_val) {
			dhd->conf->spect = (int)simple_strtol(pick, NULL, 10);
			printf("%s: spect = %d\n", __FUNCTION__, dhd->conf->spect);
		}
 
		bcmerror = 0;
	} else {
		CONFIG_ERROR(("%s: error reading config file: %d\n", __FUNCTION__, len));
		bcmerror = BCME_SDIO_ERROR;
	}

err:
	if (memblock)
		MFREE(dhd->osh, memblock, MAXSZ_CONFIG);

	if (image)
		dhd_os_close_image(image);

	if(pick)
		kfree(pick);
	return bcmerror;
}

int
dhd_conf_preinit(dhd_pub_t *dhd)
{
	CONFIG_TRACE(("%s: Enter\n", __FUNCTION__));

	dhd_conf_free_mac_list(&dhd->conf->fw_by_mac);
	dhd_conf_free_mac_list(&dhd->conf->nv_by_mac);
	memset(dhd->conf, 0, sizeof(dhd_conf_t));

	dhd->conf->band = WLC_BAND_AUTO;
	dhd->conf->mimo_bw_cap = -1;
	if (dhd_bus_chip_id(dhd) == BCM43362_CHIP_ID ||
			dhd_bus_chip_id(dhd) == BCM4330_CHIP_ID) {
		strcpy(dhd->conf->cspec.country_abbrev, "ALL");
		strcpy(dhd->conf->cspec.ccode, "ALL");
		dhd->conf->cspec.rev = 0;
	} else {
		strcpy(dhd->conf->cspec.country_abbrev, "CN");
		strcpy(dhd->conf->cspec.ccode, "CN");
		dhd->conf->cspec.rev = 0;
	}
	memset(&dhd->conf->channels, 0, sizeof(wl_channel_list_t));
	dhd->conf->roam_off = 1;
	dhd->conf->roam_off_suspend = 1;
#ifdef CUSTOM_ROAM_TRIGGER_SETTING
	dhd->conf->roam_trigger[0] = CUSTOM_ROAM_TRIGGER_SETTING;
#else
	dhd->conf->roam_trigger[0] = -65;
#endif
	dhd->conf->roam_trigger[1] = WLC_BAND_ALL;
	dhd->conf->roam_scan_period[0] = 10;
	dhd->conf->roam_scan_period[1] = WLC_BAND_ALL;
#ifdef CUSTOM_ROAM_DELTA_SETTING
	dhd->conf->roam_delta[0] = CUSTOM_ROAM_DELTA_SETTING;
#else
	dhd->conf->roam_delta[0] = 15;
#endif
	dhd->conf->roam_delta[1] = WLC_BAND_ALL;
#ifdef FULL_ROAMING_SCAN_PERIOD_60_SEC
	dhd->conf->fullroamperiod = 60;
#else /* FULL_ROAMING_SCAN_PERIOD_60_SEC */
	dhd->conf->fullroamperiod = 120;
#endif /* FULL_ROAMING_SCAN_PERIOD_60_SEC */
#ifdef CUSTOM_KEEP_ALIVE_SETTING
	dhd->conf->keep_alive_period = CUSTOM_KEEP_ALIVE_SETTING;
#else
	dhd->conf->keep_alive_period = 28000;
#endif
	dhd->conf->force_wme_ac = 0;
	dhd->conf->stbc = -1;
	dhd->conf->phy_oclscdenable = -1;
#ifdef PKT_FILTER_SUPPORT
	memset(&dhd->conf->pkt_filter_add, 0, sizeof(conf_pkt_filter_add_t));
	memset(&dhd->conf->pkt_filter_del, 0, sizeof(conf_pkt_filter_del_t));
#endif
	dhd->conf->srl = -1;
	dhd->conf->lrl = -1;
	dhd->conf->bcn_timeout = 8;
	if (dhd_bus_chip_id(dhd) == BCM4339_CHIP_ID) {
		dhd->conf->bus_txglom = 8;
		dhd->conf->ampdu_ba_wsize = 40;
	}
	dhd->conf->kso_enable = TRUE;
	dhd->conf->spect = -1;

	return 0;
}

int
dhd_conf_attach(dhd_pub_t *dhd)
{
	dhd_conf_t *conf;

	CONFIG_TRACE(("%s: Enter\n", __FUNCTION__));

	if (dhd->conf != NULL) {
		printf("%s: config is attached before!\n", __FUNCTION__);
		return 0;
	}
	/* Allocate private bus interface state */
	if (!(conf = MALLOC(dhd->osh, sizeof(dhd_conf_t)))) {
		CONFIG_ERROR(("%s: MALLOC failed\n", __FUNCTION__));
		goto fail;
	}
	memset(conf, 0, sizeof(dhd_conf_t));

	dhd->conf = conf;

	return 0;

fail:
	if (conf != NULL)
		MFREE(dhd->osh, conf, sizeof(dhd_conf_t));
	return BCME_NOMEM;
}

void
dhd_conf_detach(dhd_pub_t *dhd)
{
	CONFIG_TRACE(("%s: Enter\n", __FUNCTION__));

	if (dhd->conf) {
		dhd_conf_free_mac_list(&dhd->conf->fw_by_mac);
		dhd_conf_free_mac_list(&dhd->conf->nv_by_mac);
		MFREE(dhd->osh, dhd->conf, sizeof(dhd_conf_t));
	}
	dhd->conf = NULL;
}

#ifdef POWER_OFF_IN_SUSPEND
struct net_device *g_netdev;
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
struct sdio_early_suspend_info {
	struct sdio_func *func;
	struct early_suspend sdio_early_suspend;
	struct work_struct	tqueue;
	int do_late_resume;
#if defined(CONFIG_HAS_WAKELOCK)
    struct wake_lock power_wake_lock;
#endif
};
struct sdio_early_suspend_info sdioinfo[4];

void
dhd_conf_wifi_stop(struct net_device *dev)
{
	if (!dev) {
		CONFIG_ERROR(("%s: dev is null\n", __FUNCTION__));
		return;
	}

	printk("%s in 1\n", __FUNCTION__);
	dhd_net_if_lock(dev);
	printk("%s in 2: g_wifi_on=%d, name=%s\n", __FUNCTION__, g_wifi_on, dev->name);
	if (g_wifi_on) {
#ifdef WL_CFG80211
		wl_cfg80211_user_sync(true);
		wl_cfg80211_stop();
#endif
		dhd_bus_devreset(bcmsdh_get_drvdata(), true);
		sdioh_stop(NULL);
		dhd_customer_gpio_wlan_ctrl(WLAN_POWER_OFF);
		g_wifi_on = FALSE;
#ifdef WL_CFG80211
		wl_cfg80211_user_sync(false);
#endif
	}
	printk("%s out\n", __FUNCTION__);
	dhd_net_if_unlock(dev);

}

bool wifi_ready = true;

void
dhd_conf_wifi_power(bool on)
{
	extern struct wl_priv *wlcfg_drv_priv;
	printk("%s: Enter %d\n", __FUNCTION__, on);
	if (on) {
#ifdef WL_CFG80211
		wl_cfg80211_user_sync(true);
#endif
		if(wl_android_wifi_on(g_netdev) < 0) {
            /* wifi on failed, send HANG message to tell wpa_supplicant to restart wifi*/
            if(g_netdev)
                net_os_send_hang_message(g_netdev);
		}
        else {
#ifdef WL_CFG80211
    		wl_cfg80211_send_disconnect();
#endif
    		if (wlcfg_drv_priv && wlcfg_drv_priv->p2p)
    			wl_cfgp2p_start_p2p_device(NULL, NULL);
    		else		
    			printk("======= ON : no p2p ======\n");
        }
#ifdef WL_CFG80211
		wl_cfg80211_user_sync(false);
#endif
		wifi_ready = true;
	} else {
		wifi_ready = false;
		if (wlcfg_drv_priv && wlcfg_drv_priv->p2p) {
			wl_cfgp2p_clear_management_ie(wlcfg_drv_priv, 0);
			wl_cfgp2p_clear_management_ie(wlcfg_drv_priv, 1);
			wl_cfgp2p_stop_p2p_device(NULL, wlcfg_drv_priv->p2p_wdev);
		} else 
			printk("======= OFF : no p2p ======\n");
		dhd_conf_wifi_stop(g_netdev);
	}
	printk("%s: Exit %d\n", __FUNCTION__, on);
}

void
dhd_conf_power_workqueue(struct work_struct *work)
{
#if defined(CONFIG_HAS_WAKELOCK)
    struct sdio_early_suspend_info *sdioinfo =container_of(work, struct sdio_early_suspend_info, tqueue);
    wake_lock(&sdioinfo->power_wake_lock);
#endif
    dhd_conf_wifi_power(true);
#if defined(CONFIG_HAS_WAKELOCK)
    wake_unlock(&sdioinfo->power_wake_lock);
#endif
}

void
dhd_conf_early_suspend(struct early_suspend *h)
{
	struct sdio_early_suspend_info *sdioinfo = container_of(h, struct sdio_early_suspend_info, sdio_early_suspend);

	printk("%s: Enter\n", __FUNCTION__);
	if (sdioinfo->func->num == 2)
		sdioinfo->do_late_resume = 0;
}

void
dhd_conf_late_resume(struct early_suspend *h)
{
	struct sdio_early_suspend_info *sdioinfo = container_of(h, struct sdio_early_suspend_info, sdio_early_suspend);

	printk("%s: Enter\n", __FUNCTION__);
	if(sdioinfo->func->num == 2 && sdioinfo->do_late_resume ){
		sdioinfo->do_late_resume = 0;
		schedule_work(&sdioinfo->tqueue);
	}
}
#endif /* defined(CONFIG_HAS_EARLYSUSPEND) */

void
dhd_conf_wifi_suspend(struct sdio_func *func)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	if (!sdioinfo[func->num].do_late_resume) {
		dhd_conf_wifi_power(false);
		sdioinfo[func->num].do_late_resume = 1;
	}
#endif
}

void
dhd_conf_register_wifi_suspend(struct sdio_func *func)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	if (func->num == 2) {
		sdioinfo[func->num].func = func;
		sdioinfo[func->num].do_late_resume = 0;
		sdioinfo[func->num].sdio_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 30;
		sdioinfo[func->num].sdio_early_suspend.suspend = dhd_conf_early_suspend;
		sdioinfo[func->num].sdio_early_suspend.resume = dhd_conf_late_resume;
		register_early_suspend(&sdioinfo[func->num].sdio_early_suspend);
		INIT_WORK(&sdioinfo[func->num].tqueue, dhd_conf_power_workqueue);
#ifdef CONFIG_HAS_WAKELOCK
        wake_lock_init(&sdioinfo[func->num].power_wake_lock, WAKE_LOCK_SUSPEND, "wifi_power_wake");
        printk("init wifi power_wake lock\n");
#endif
	}
#endif
}

void
dhd_conf_unregister_wifi_suspend(struct sdio_func *func)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	if (func->num == 2) {
		if (sdioinfo[func->num].sdio_early_suspend.suspend) {
#ifdef CONFIG_HAS_WAKELOCK
            wake_lock_destroy(&sdioinfo[func->num].power_wake_lock);
#endif
			unregister_early_suspend(&sdioinfo[func->num].sdio_early_suspend);
			sdioinfo[func->num].sdio_early_suspend.suspend = NULL;
		}
	}
#endif
}
#endif

