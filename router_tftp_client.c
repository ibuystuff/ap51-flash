/*
 * Copyright (C) Marek Lindner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 3 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 * SPDX-License-Identifier: GPL-3.0+
 * License-Filename: LICENSES/preferred/GPL-3.0
 */

#include "router_tftp_client.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include "compat.h"
#include "flash.h"
#include "proto.h"
#include "router_images.h"
#include "router_types.h"

static const unsigned int mr500_ip = 3232260872UL; /* 192.168.99.8 */
static const unsigned int om2p_ip = 3232261128UL; /* 192.168.100.8 */
static const unsigned int zyxel_ip = 3232235875UL; /* 192.168.1.99 */

struct mr500_priv {
	time_t start_flash;
};

struct om2p_priv {
	time_t start_flash;
};

static void tftp_client_detect_post(struct node *node, const char *packet_buff,
				    int packet_buff_len)
{
	struct ether_arp *arphdr;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;

	node->flash_mode = FLASH_MODE_TFTP_CLIENT;
	node->his_ip_addr = *((unsigned int *)(arphdr->arp_spa));
	node->our_ip_addr = *((unsigned int *)(arphdr->arp_tpa));

out:
	return;
}

void tftp_client_flash_time_set(struct node *node)
{
	struct mr500_priv *mr500_priv;
	struct om2p_priv *om2p_priv;

	if (node->router_type == &mr500) {
		mr500_priv = node->router_priv;
		mr500_priv->start_flash = time(NULL);
	} else if ((node->router_type == &mr600) ||
		   (node->router_type == &mr900) ||
		   (node->router_type == &mr1750) ||
		   (node->router_type == &a40) ||
		   (node->router_type == &a42) ||
		   (node->router_type == &a60) ||
		   (node->router_type == &a62) ||
		   (node->router_type == &om2p) ||
		   (node->router_type == &om5p) ||
		   (node->router_type == &om5pac) ||
		   (node->router_type == &om5pan) ||
		   (node->router_type == &p60) ||
		   (node->router_type == &d200) ||
		   (node->router_type == &g200) ||
		   (node->router_type == &zyxel)) {

		om2p_priv = node->router_priv;
		om2p_priv->start_flash = time(NULL);
	}
}

int tftp_client_flash_completed(struct node *node)
{
	struct mr500_priv *mr500_priv;
	struct om2p_priv *om2p_priv;
	time_t time2flash;

	if (node->router_type == &mr500) {
		mr500_priv = node->router_priv;
		time2flash = mr500_priv->start_flash + 45 + (node->image_state.total_bytes_sent / 65536);
	} else if ((node->router_type == &mr600) ||
		   (node->router_type == &mr900) ||
		   (node->router_type == &mr1750) ||
		   (node->router_type == &a40) ||
		   (node->router_type == &a42) ||
		   (node->router_type == &a60) ||
		   (node->router_type == &a62) ||
		   (node->router_type == &om2p) ||
		   (node->router_type == &om5p) ||
		   (node->router_type == &om5pac) ||
		   (node->router_type == &om5pan) ||
		   (node->router_type == &p60) ||
		   (node->router_type == &d200) ||
		   (node->router_type == &g200) ||
		   (node->router_type == &zyxel)) {

		om2p_priv = node->router_priv;
		time2flash = om2p_priv->start_flash + 10 + (node->image_state.total_bytes_sent / 65536);
	} else {
		return 0;
	}

	if (time(NULL) < time2flash)
		return 0;

	return 1;
}

static int mr500_detect_main(void (*priv)__attribute__((unused)),
			     const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(mr500_ip))
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type mr500 = {
	.desc = "MR500 router",
	.detect_pre = NULL,
	.detect_main = mr500_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_uboot,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf8},
	.priv_size = sizeof(struct mr500_priv),
};

static int mr600_detect_main(void (*priv)__attribute__((unused)),
			     const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'M')
		goto out;

	if (arphdr->arp_tha[1] != 'R')
		goto out;

	if (arphdr->arp_tha[2] != '6')
		goto out;

	if (arphdr->arp_tha[3] != '0')
		goto out;

	if (arphdr->arp_tha[4] != '0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type mr600 = {
	.desc = "MR600",
	.detect_pre = NULL,
	.detect_main = mr600_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static int mr900_detect_main(void (*priv)__attribute__((unused)),
			     const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'M')
		goto out;

	if (arphdr->arp_tha[1] != 'R')
		goto out;

	if (arphdr->arp_tha[2] != '9')
		goto out;

	if (arphdr->arp_tha[3] != '0')
		goto out;

	if (arphdr->arp_tha[4] != '0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type mr900 = {
	.desc = "MR900",
	.detect_pre = NULL,
	.detect_main = mr900_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static int mr1750_detect_main(void (*priv)__attribute__((unused)),
			      const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'M')
		goto out;

	if (arphdr->arp_tha[1] != 'R')
		goto out;

	if (arphdr->arp_tha[2] != '1')
		goto out;

	if (arphdr->arp_tha[3] != '7')
		goto out;

	if (arphdr->arp_tha[4] != '5')
		goto out;

	if (arphdr->arp_tha[5] != '0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type mr1750 = {
	.desc = "MR1750",
	.detect_pre = NULL,
	.detect_main = mr1750_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static bool om2p_orig_arp(const uint8_t arp_tha[ETH_ALEN])
{
	/* target mac address field has to be zero */
	if (arp_tha[0] != '\0')
		return false;

	if (arp_tha[1] != '\0')
		return false;

	if (arp_tha[2] != '\0')
		return false;

	if (arp_tha[3] != '\0')
		return false;

	if (arp_tha[4] != '\0')
		return false;

	if (arp_tha[5] != '\0')
		return false;

	return true;
}

static bool om2p_v4_arp(const uint8_t arp_tha[ETH_ALEN])
{
	if (arp_tha[0] != 'O')
		return false;

	if (arp_tha[1] != 'M')
		return false;

	if (arp_tha[2] != '2')
		return false;

	if (arp_tha[3] != 'P')
		return false;

	if (arp_tha[4] != 'V')
		return false;

	if (arp_tha[5] != '4')
		return false;

	return true;
}

static int om2p_detect_main(void (*priv)__attribute__((unused)),
			    const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (!om2p_orig_arp(arphdr->arp_tha) && !om2p_v4_arp(arphdr->arp_tha))
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type om2p = {
	.desc = "OM2P",
	.detect_pre = NULL,
	.detect_main = om2p_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf8},
	.priv_size = sizeof(struct om2p_priv),
};

static int a40_detect_main(void (*priv)__attribute__((unused)),
			   const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'A')
		goto out;

	if (arphdr->arp_tha[1] != '4')
		goto out;

	if (arphdr->arp_tha[2] != '0')
		goto out;

	if (arphdr->arp_tha[3] != '\0')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}
const struct router_type a40 = {
	.desc = "A40",
	.detect_pre = NULL,
	.detect_main = a40_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.image_desc = "A60",
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static int a60_detect_main(void (*priv)__attribute__((unused)),
			   const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'A')
		goto out;

	if (arphdr->arp_tha[1] != '6')
		goto out;

	if (arphdr->arp_tha[2] != '0')
		goto out;

	if (arphdr->arp_tha[3] != '\0')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}
const struct router_type a60 = {
	.desc = "A60",
	.detect_pre = NULL,
	.detect_main = a60_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static int a42_detect_main(void (*priv)__attribute__((unused)),
			   const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'A')
		goto out;

	if (arphdr->arp_tha[1] != '4')
		goto out;

	if (arphdr->arp_tha[2] != '2')
		goto out;

	if (arphdr->arp_tha[3] != '\0')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type a42 = {
	.desc = "A42",
	.detect_pre = NULL,
	.detect_main = a42_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static int a62_detect_main(void (*priv)__attribute__((unused)),
			   const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'A')
		goto out;

	if (arphdr->arp_tha[1] != '6')
		goto out;

	if (arphdr->arp_tha[2] != '2')
		goto out;

	if (arphdr->arp_tha[3] != '\0')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type a62 = {
	.desc = "A62",
	.detect_pre = NULL,
	.detect_main = a62_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static int om5p_detect_main(void (*priv)__attribute__((unused)),
			    const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'O')
		goto out;

	if (arphdr->arp_tha[1] != 'M')
		goto out;

	if (arphdr->arp_tha[2] != '5')
		goto out;

	if (arphdr->arp_tha[3] != 'P')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type om5p = {
	.desc = "OM5P",
	.detect_pre = NULL,
	.detect_main = om5p_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf8},
	.priv_size = sizeof(struct om2p_priv),
};

static int om5pan_detect_main(void (*priv)__attribute__((unused)),
			      const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'O')
		goto out;

	if (arphdr->arp_tha[1] != 'M')
		goto out;

	if (arphdr->arp_tha[2] != '5')
		goto out;

	if (arphdr->arp_tha[3] != 'P')
		goto out;

	if (arphdr->arp_tha[4] != 'A')
		goto out;

	if (arphdr->arp_tha[5] != 'N')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type om5pan = {
	.desc = "OM5P-AN",
	.detect_pre = NULL,
	.detect_main = om5pan_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.image_desc = "OM5P",
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static int om5pac_detect_main(void (*priv)__attribute__((unused)),
			      const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'O')
		goto out;

	if (arphdr->arp_tha[1] != 'M')
		goto out;

	if (arphdr->arp_tha[2] != '5')
		goto out;

	if (arphdr->arp_tha[3] != 'P')
		goto out;

	if (arphdr->arp_tha[4] != 'A')
		goto out;

	if (arphdr->arp_tha[5] != 'C')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type om5pac = {
	.desc = "OM5P-AC",
	.detect_pre = NULL,
	.detect_main = om5pac_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.image_desc = "OM5PAC",
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0},
	.priv_size = sizeof(struct om2p_priv),
};

static int p60_detect_main(void (*priv)__attribute__((unused)),
			   const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'P')
		goto out;

	if (arphdr->arp_tha[1] != '6')
		goto out;

	if (arphdr->arp_tha[2] != '0')
		goto out;

	if (arphdr->arp_tha[3] != '\0')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type p60 = {
	.desc = "P60",
	.detect_pre = NULL,
	.detect_main = p60_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.image_desc = "P60",
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf8},
	.priv_size = sizeof(struct om2p_priv),
};

static int d200_detect_main(void (*priv)__attribute__((unused)),
			    const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'D')
		goto out;

	if (arphdr->arp_tha[1] != '2')
		goto out;

	if (arphdr->arp_tha[2] != '0')
		goto out;

	if (arphdr->arp_tha[3] != '0')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type d200 = {
	.desc = "D200",
	.detect_pre = NULL,
	.detect_main = d200_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.image_desc = "D200",
	/* TODO confirm the used mac addresses */
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.priv_size = sizeof(struct om2p_priv),
};

static int g200_detect_main(void (*priv)__attribute__((unused)),
			    const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(om2p_ip))
		goto out;

	if (arphdr->arp_tha[0] != 'G')
		goto out;

	if (arphdr->arp_tha[1] != '2')
		goto out;

	if (arphdr->arp_tha[2] != '0')
		goto out;

	if (arphdr->arp_tha[3] != '0')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type g200 = {
	.desc = "G200",
	.detect_pre = NULL,
	.detect_main = g200_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_ce,
	.image_desc = "G200",
	/* TODO confirm the used mac addresses */
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.priv_size = sizeof(struct om2p_priv),
};

static int zyxel_detect_main(void (*priv)__attribute__((unused)),
			     const char *packet_buff, int packet_buff_len)
{
	struct ether_arp *arphdr;
	int ret = 0;

	if (!len_check(packet_buff_len, sizeof(struct ether_arp), "ARP"))
		goto out;

	arphdr = (struct ether_arp *)packet_buff;
	if (arphdr->ea_hdr.ar_op != htons(ARPOP_REQUEST))
		goto out;

	if (*((unsigned int *)arphdr->arp_tpa) != htonl(zyxel_ip))
		goto out;

	if (arphdr->arp_tha[0] != '\0')
		goto out;

	if (arphdr->arp_tha[1] != '\0')
		goto out;

	if (arphdr->arp_tha[2] != '\0')
		goto out;

	if (arphdr->arp_tha[3] != '\0')
		goto out;

	if (arphdr->arp_tha[4] != '\0')
		goto out;

	if (arphdr->arp_tha[5] != '\0')
		goto out;

	ret = 1;

out:
	return ret;
}

const struct router_type zyxel = {
	.desc = "Zyxel",
	.detect_pre = NULL,
	.detect_main = zyxel_detect_main,
	.detect_post = tftp_client_detect_post,
	.image = &img_zyxel,
	.image_desc = "Zyxel",
	.mac_mask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.priv_size = sizeof(struct om2p_priv),
};
