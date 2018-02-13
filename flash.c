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

#include "flash.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "list.h"
#include "proto.h"
#include "router_tftp_client.h"
#include "router_types.h"
#include "socket.h"

static int running = 1;
static struct list *node_list;
static uint8_t our_mac[] = {0x00, 0xba, 0xbe, 0xca, 0xff, 0x00};

#if defined(CLEAR_SCREEN)
int num_nodes_flashed = 0;
#endif

#define PACKET_BUFF_LEN 2000
#define READ_SLEEP_SEC 0
#define READ_SLEEP_USEC 250000

static int node_list_init(void)
{
	node_list = NULL;
	return 0;
}

static struct node *node_list_find(const uint8_t *mac_addr)
{
	struct list *list;
	struct node *node;
	const uint8_t *mask;
	const uint8_t mask_all[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	slist_for_each (list, node_list) {
		node = (struct node *)list->data;

		if (node->router_type)
			mask = node->router_type->mac_mask;
		else
			mask  = mask_all;

		if (!ether_addr_equal_mask(node->his_mac_addr, mac_addr, mask))
			continue;

		return node;
	}

	return NULL;
}

struct node *node_list_get(const uint8_t *mac_addr)
{
	struct list *list;
	struct node *node;

	node = node_list_find(mac_addr);
	if (node)
		goto out;

	node = malloc(sizeof(struct node) + router_types_priv_size);
	if (!node)
		goto out;


	list = malloc(sizeof(struct list));
	if (!list)
		goto free_node;

	memset(list, 0, sizeof(struct list));
	memset(node, 0, sizeof(struct node) + router_types_priv_size);
	memcpy(node->his_mac_addr, mac_addr, ETH_ALEN);
	node->image_state.fd = -1;
	list->data = node;
	list->next = NULL;
	list_prepend(&node_list, list);
	goto out;

free_node:
	free(node);
	node = NULL;
out:
	return node;
}

static void _node_list_free(struct list *list)
{
	struct node *node = (struct node *)list->data;

	free(node);
	free(list);
}

static void node_list_free(void)
{
	struct list *list, *list_tmp;

	slist_for_each_safe (list, list_tmp, node_list)
		_node_list_free(list);

	node_list = NULL;
}

static void node_list_maintain(void)
{
	struct list *list, *list_tmp;
	struct node *node;
	int ret;

	slist_for_each_safe (list, list_tmp, node_list) {
		node = (struct node *)list->data;

		switch (node->status) {
		case NODE_STATUS_UNKNOWN:
		case NODE_STATUS_RESET_SENT:
		case NODE_STATUS_DETECTING:
			/* ignored */
			break;
		case NODE_STATUS_DETECTED:
			switch (node->flash_mode) {
			case FLASH_MODE_TFTP_SERVER:
				tftp_init_upload(node);
				break;
			case FLASH_MODE_REDBOOT:
				telnet_handle_connection(node);
				break;
			case FLASH_MODE_TFTP_CLIENT:
				/* ignored; handled in handle_udp_packet */
				break;
			case FLASH_MODE_UKNOWN:
				fprintf(stderr, "[%02x:%02x:%02x:%02x:%02x:%02x]: Error, flash mode unknown.\n",
					node->his_mac_addr[0], node->his_mac_addr[1],
					node->his_mac_addr[2], node->his_mac_addr[3],
					node->his_mac_addr[4], node->his_mac_addr[5]);
				break;
			}
			break;
		case NODE_STATUS_FLASHING:
			/* if (node->flash_mode == FLASH_MODE_REDBOOT)
				telnet_keep_alive(node); */
			break;
		case NODE_STATUS_FINISHED:
			if (node->flash_mode != FLASH_MODE_TFTP_CLIENT)
				break;

			ret = tftp_client_flash_completed(node);
			if (ret == 0)
				break;

			fprintf(stderr, "[%02x:%02x:%02x:%02x:%02x:%02x]: %s router: flash complete. Device ready to unplug.\n",
				node->his_mac_addr[0], node->his_mac_addr[1],
				node->his_mac_addr[2], node->his_mac_addr[3],
				node->his_mac_addr[4], node->his_mac_addr[5],
				node->router_type->desc);
			node->status = NODE_STATUS_REBOOTED;

			/* MR500 devices all have the same mac address during flash .. :( */
			if (node->router_type == &mr500) {
				node->status = NODE_STATUS_UNKNOWN;
				node->flash_mode = FLASH_MODE_UKNOWN;
				memset((void *)&node->image_state, 0,
				       sizeof(struct image_state));
				node->image_state.fd = -1;
			}
#if defined(CLEAR_SCREEN)
			num_nodes_flashed++;
#endif
			break;
		case NODE_STATUS_REBOOTED:
		case NODE_STATUS_NO_FLASH:
			/* check timeout and call _node_list_free(list); */
			break;
		}
	}
}

void our_mac_set(struct node *node)
{
	memcpy(node->our_mac_addr, our_mac, ETH_ALEN);

	/* TODO: 256 addresses might not be sufficient */
	our_mac[5]++;
}

static void sig_handler(int signal)
{
	switch (signal) {
	case SIGINT:
	case SIGTERM:
		running = 0;
		break;
	}
}

int flash_start(const char *iface)
{
	char *packet_buff;
	int ret, sleep_sec, sleep_usec;

	ret = socket_open(iface);
	if (ret < 0)
		goto out;

	ret = node_list_init();
	if (ret < 0)
		goto sock_close;

	packet_buff = malloc(PACKET_BUFF_LEN);
	if (!packet_buff)
		goto list_free;

	ret = proto_init();
	if (ret < 0)
		goto pack_free;

	ret = router_types_init();
	if (ret < 0)
		goto proto_free;

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	sleep_sec = READ_SLEEP_SEC;
	sleep_usec = READ_SLEEP_USEC;

	while (running) {
		ret = socket_read(packet_buff, PACKET_BUFF_LEN, &sleep_sec,
				  &sleep_usec);

		if (ret == 0) {
			router_types_detect_pre(our_mac);
			node_list_maintain();
		}

		if (ret <= 0)
			goto reset_sleep;

		handle_eth_packet(packet_buff, ret);
		continue;

reset_sleep:
		sleep_sec = READ_SLEEP_SEC;
		sleep_usec = READ_SLEEP_USEC;
	}

	ret = 0;

proto_free:
	proto_free();
pack_free:
	free(packet_buff);
list_free:
	node_list_free();
sock_close:
	socket_close(iface);
out:
	return ret;
}
