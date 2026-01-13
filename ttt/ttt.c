#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	uint32_t total_len;
	uint32_t n_chunks;
} modbus_file_send_header_t;

uint32_t __REV16(uint32_t x) {
	return (x >> 8) | (x << 8);
}

int main(void)
{
	const uint16_t count = 4;
	// uint8_t registers8[] = {0x00, 0x00, 0x60, 0x05, 0x00, 0x00, 0x06,
	// 0x00}; // big-endian
	uint8_t registers8[] = {0x05, 0x60, 0x00, 0x00,
				0x00, 0x06, 0x00, 0x00}; // little-endian
	uint16_t *registers = (void *)registers8;
	modbus_file_send_header_t *hdr = (void *)registers;

	for (int i = 0; i < count; i++) {
		registers[i] = __REV16(registers[i]);
	}

	printf("%s::MOBDUS_FILE_SEND_HEAD_REC_NUM hdr->total_len: %04x, "
	       "n_chunks: %04x\n",
	       __func__, (hdr->total_len), (hdr->n_chunks));

	printf("%s:: data:", __func__);
	for (int i = 0; i < count * 2; i++) {
		printf(" %02X", registers8[i]);
	}
	printf("\n");

	return 0;
}