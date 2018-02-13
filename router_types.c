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

#include "router_types.h"

#include <stdio.h>

#include "flash.h"
#include "router_images.h"
#include "router_redboot.h"
#include "router_tftp_client.h"
#include "router_tftp_server.h"

#if defined(CLEAR_SCREEN)
#if defined(LINUX) || defined(WIN32)
#include <stdlib.h>
#endif
#endif

int router_types_priv_size = 0;

static const struct router_type *router_types[] = {
	&a40,
	&a42,
	&a60,
	&a62,
	&d200,
	&g200,
	&mr1750,
	&mr500,
	&mr600,
	&mr900,
	&om2p,
	&om5p,
	&om5pac,
	&om5pan,
	&p60,
	&redboot,
	&ubnt,
	&zyxel,
	NULL,
};

int router_types_init(void)
{
	int ret = -1;
	const struct router_type **router_type;

	for (router_type = router_types; *router_type; ++router_type) {
		if (!(*router_type)->image) {
			fprintf(stderr,
				"Error - can't have router defintion without image attribute set: %s\n",
				(*router_type)->desc);
			goto out;
		}

		router_types_priv_size += (*router_type)->priv_size;

		if (is_zero_ether_addr((*router_type)->mac_mask)) {
			fprintf(stderr,
				"Error - can't have router definition without mac address mask set: %s\n",
				(*router_type)->desc);
			goto out;
		}
	}

	ret = 0;

out:
	return ret;
}

void router_types_detect_pre(const uint8_t *our_mac)
{
	const struct router_type **router_type;

	for (router_type = router_types; *router_type; ++router_type) {
		if (!(*router_type)->detect_pre)
			continue;

		(*router_type)->detect_pre(our_mac);
	}
}

int router_types_detect_main(struct node *node, const char *packet_buff,
			     int packet_buff_len)
{
	const struct router_type **router_type;
	struct router_info *router_info;
	void *priv = node + 1;
	int ret = 0;

	for (router_type = router_types; *router_type; ++router_type) {
		if (!(*router_type)->detect_main)
			goto next;

		ret = (*router_type)->detect_main(priv, packet_buff,
						  packet_buff_len);
		if (ret != 1)
			goto next;

		/* we detected a router that we have no image for */
		if ((*router_type)->image->file_size < 1) {
			fprintf(stderr, "[%02x:%02x:%02x:%02x:%02x:%02x]: is of type '%s' that we have no image for\n",
				node->his_mac_addr[0], node->his_mac_addr[1],
				node->his_mac_addr[2], node->his_mac_addr[3],
				node->his_mac_addr[4], node->his_mac_addr[5],
				(*router_type)->desc);

			node->status = NODE_STATUS_NO_FLASH;
			ret = 0;
			break;
		}

		if ((*router_type)->image->type == IMAGE_TYPE_CE) {
			router_info = router_image_router_get((*router_type)->image,
							      (*router_type)->image_desc ? (char *)(*router_type)->image_desc : (char *)(*router_type)->desc);
			if (!router_info) {
				fprintf(stderr, "[%02x:%02x:%02x:%02x:%02x:%02x]: is of type '%s' that we have no image for (ce)\n",
					node->his_mac_addr[0],
					node->his_mac_addr[1],
					node->his_mac_addr[2],
					node->his_mac_addr[3],
					node->his_mac_addr[4],
					node->his_mac_addr[5],
					(*router_type)->desc);

				node->status = NODE_STATUS_NO_FLASH;
				ret = 0;
				break;
			}
		}

		our_mac_set(node);
		node->router_type = (struct router_type *)(*router_type);
		node->router_priv = priv;

#if defined(CLEAR_SCREEN)
#if defined(LINUX)
		if (num_nodes_flashed > 0)
			ret = system("clear");
#elif defined(WIN32)
		if (num_nodes_flashed > 0)
			ret = system("cls");
#else
#error CLEAR_SCREEN is not supported on your OS
#endif
		/* keep gcc happy by retrieving the return value of the system() call */
		ret = 1;
#endif

		fprintf(stderr, "[%02x:%02x:%02x:%02x:%02x:%02x]: type '%s router' detected\n",
			node->his_mac_addr[0], node->his_mac_addr[1],
			node->his_mac_addr[2], node->his_mac_addr[3],
			node->his_mac_addr[4], node->his_mac_addr[5],
			node->router_type->desc);

		if (!(*router_type)->detect_post)
			break;

		(*router_type)->detect_post(node, packet_buff, packet_buff_len);
		break;

next:
		priv = (char *)priv + (*router_type)->priv_size;
	}

	return ret;
}
