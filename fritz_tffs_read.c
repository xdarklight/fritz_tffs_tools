/*
 * A tool for reading the TFFS partitions (a name-value storage usually
 * found in AVM Fritz!Box based devices).
 *
 * Copyright (c) 2015 Martin Blumenstingl <martin.blumenstingl@googlemail.com>
 *
 * Based on the TFFS 2.0 kernel driver from AVM:
 *     Copyright (c) 2004-2007 AVM GmbH <fritzbox_info@avm.de>
 * and the OpenWrt TFFS kernel driver:
 *     Copyright (c) 2013 John Crispin <blogic@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libgen.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define ARRAYSIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define DEFAULT_TFFS_SIZE (256 * 1024)

static char *progname;
static char *input_file;
static unsigned long tffs_size = DEFAULT_TFFS_SIZE;
static char *name_filter = 0;
static uint8_t show_all = 0;

struct tffs_entry {
    uint16_t id;
    uint16_t len;
};

static struct tffs_id {
	uint32_t id;
	char *name;
	void *val;
	uint32_t offset;
	uint32_t len;
} ids[] = {
	{ 0x0100, "hw_revision" },
	{ 0x0101, "productid" },
	{ 0x0102, "serialnumber" },
	{ 0x0103, "dmc" },
	{ 0x0104, "hw_subrevision" },
	{ 0x0182, "bootloader_version" },
	{ 0x0184, "macbluetooth" },
	{ 0x0188, "maca" },
	{ 0x0189, "macb" },
	{ 0x018A, "macwlan" },
	{ 0x018B, "macdsl" },
	{ 0x018F, "my_ipaddress" },
	{ 0x0195, "macwlan2" },
	{ 0x01A3, "usb_device_id" },
	{ 0x01A3, "usb_revision_id" },
	{ 0x01A4, "usb_device_name" },
	{ 0x01A5, "usb_manufacturer_name" },
	{ 0x01A6, "firmware_version" },
	{ 0x01A7, "language" },
	{ 0x01A8, "country" },
	{ 0x01A9, "annex" },
	{ 0x01AB, "wlan_key" },
	{ 0x01AD, "http_key" },
	{ 0x01B8, "wlan_cal" },
	{ 0x01FD, "urlader_version" },
};

static struct tffs_id* tffs_find_id(int id)
{
	int i;

	for (i = 0; i < ARRAYSIZE(ids); i++)
		if (id == ids[i].id)
			return &ids[i];

	return NULL;
}

static uint32_t tffs_parse(uint8_t *buffer)
{
	struct tffs_id *id;
	struct tffs_entry *entry;
	uint32_t pos = 0, count = 0;

	while (pos + sizeof(struct tffs_entry) < tffs_size) {
		entry = (struct tffs_entry *) &buffer[pos];
		entry->id = ntohs(entry->id);
		entry->len = ntohs(entry->len);

		if (entry->id == 0xffff)
			goto end;

		pos += sizeof(struct tffs_entry);

		id = tffs_find_id(entry->id);
		if (id) {
			id->len = entry->len;
			id->offset = pos;
			id->val = calloc(entry->len + 1, 1);

			memcpy(id->val, &buffer[pos], entry->len);

			++count;
		}

		pos += (entry->len + 3) & ~0x03;
	}

end:
	return count;
}

static void print_all_key_names(void)
{
	int i;

	for (i = 0; i < ARRAYSIZE(ids); i++)
		fprintf(stdout, "%s\n", ids[i].name);
}

static void usage(int status)
{
	FILE *stream = (status != EXIT_SUCCESS) ? stderr : stdout;

	fprintf(stream, "Usage: %s [OPTIONS...]\n", progname);
	fprintf(stream,
	"\n"
	"Options:\n"
	"  -a              list all key value pairs found in the TFFS file/device\n"
	"  -h              show this screen\n"
	"  -i <file>       inspect the given TFFS file/device <file>\n"
	"  -l              list all supported keys\n"
	"  -n <key name>   display the value of the given key\n"
	"  -s <size>       the (max) size of the TFFS file/device <size>\n"
	);

	exit(status);
}

static int file_exist(char *filename)
{
	struct stat buffer;

	return stat(filename, &buffer) == 0;
}

static void parse_options(int argc, char *argv[])
{
	while (1)
	{
		int c;

		c = getopt(argc, argv, "ahi:ln:s");
		if (c == -1)
			break;

		switch (c) {
			case 'a':
				show_all = 1;
				break;
			case 'h':
				usage(EXIT_SUCCESS);
				break;
			case 'i':
				input_file = optarg;
				break;
			case 'l':
				print_all_key_names();
				exit(EXIT_SUCCESS);
			case 'n':
				name_filter = optarg;
				break;
			case 's':
				tffs_size = strtoul(optarg, NULL, 0);
				break;
			default:
				usage(EXIT_FAILURE);
				break;
		}
	}

	if (!input_file) {
		fprintf(stderr, "ERROR: No input file (-i <file>) given!\n");
		exit(EXIT_FAILURE);
	}

	if (!file_exist(input_file)) {
		fprintf(stderr, "ERROR: %s does not exist\n", input_file);
		exit(EXIT_FAILURE);
	}

	if (!show_all && !name_filter) {
		fprintf(stderr,
			"ERROR: either -a or -n <key name> is required!\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	int i, ret = EXIT_FAILURE;
	uint8_t *buffer;
	FILE *fp;

	progname = basename(argv[0]);

	parse_options(argc, argv);

	fp = fopen(input_file, "r");

	if (!fp) {
		fprintf(stderr, "ERROR: Failed to open tffs input file %s\n",
			input_file);
		goto out;
	}

	buffer = malloc(tffs_size);

	if (fread(buffer, 1, tffs_size, fp) != tffs_size) {
		fprintf(stderr, "ERROR: Failed read tffs file %s\n",
			input_file);
		goto out_free;
	}

	if (!tffs_parse(buffer)) {
		fprintf(stderr, "ERROR: No values found in tffs file %s\n",
			input_file);
		goto out_free;
	}

	for (i = 0; i < ARRAYSIZE(ids); i++) {
		if (ids[i].val) {
			if (show_all) {
				fprintf(stdout, "%s=%s\n",
					ids[i].name, (char *) ids[i].val);
				ret = EXIT_SUCCESS;
			} else if (strcmp(name_filter, ids[i].name) == 0) {
				fprintf(stdout, "%s\n", (char *) ids[i].val);
				ret = EXIT_SUCCESS;
				break;
			}
		}
	}

	if (name_filter && ret == EXIT_FAILURE) {
		fprintf(stderr, "ERROR: Key '%s' was not found in %s\n",
			name_filter, input_file);
	}
out_free:
	fclose(fp);
	free(buffer);
out:
	return ret;
}
