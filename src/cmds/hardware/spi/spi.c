/**
 * @file spi.c
 * @brief
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version
 * @date 20.12.2019
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <drivers/spi.h>
#include <util/pretty_print.h>

static void print_help(char **argv) {
	printf("Transfer bytes via SPI bus\n");
	printf("Usage:\n");
	printf("%s bus_number line_number byte0 [byte1 [byte2 [...]]]\n", argv[0]);
}

static void list_spi_devices(void) {
	for (int i = 0; i < SPI_REGISTRY_SZ; i++) {
		struct spi_device *s = spi_dev_by_id(i);

		if (s == NULL) {
			continue;
		}

		printf("Bus %d: ", i);

		if (s->dev == NULL) {
			printf("(unnamed)\n");
			continue;
		}

		printf("%s\n", s->dev->name);
	}
}

int main(int argc, char **argv) {
	int spi_bus, spi_line;
	int ret, opt;
	struct spi_device *dev;

	while (-1 != (opt = getopt(argc, argv, "l"))) {
		switch(opt) {
		case 'l':
			list_spi_devices();
			return 0;
		case 'h':
			print_help(argv);
			return 0;
		}
	}

	if (argc < 4) {
		print_help(argv);
		return 0;
	}

	spi_bus = strtol(argv[1], NULL, 0);
	spi_line = strtol(argv[2], NULL, 0);

	dev = spi_dev_by_id(spi_bus);
	if (dev == NULL) {
		printf("Failed to select bus #%d\n", spi_bus);
		return -ENOENT;
	}

	ret = spi_select(dev, spi_line);
	if (ret < 0) {
		printf("Failed to select line #%d!\n", spi_line);
		return ret;
	}

	printf("Received bytes:");
	for (int i = 3; i < argc; i++) {
		uint8_t buf_in, buf_out;

		buf_out = strtol(argv[i], NULL, 0);

		spi_transfer(dev, &buf_out, &buf_in, 1);

		printf(" %02x", buf_in);
	}

	printf("\n");

	return 0;
}
