#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jobs.pb-c.h"

int
main (void)
{
	SstackPayloadT msg = SSTACK_PAYLOAD_T__INIT;
	SstackPayloadHdrT hdr = SSTACK_PAYLOAD_HDR_T__INIT;
	SstackNfsCommandStruct cmd = SSTACK_NFS_COMMAND_STRUCT__INIT;
	SstackNfsWriteCmd writecmd = SSTACK_NFS_WRITE_CMD__INIT;
	SstackNfsData data = SSTACK_NFS_DATA__INIT;
	SstackPayloadT *msg1 = NULL;
	SstackPayloadHdrT *hdr1 = NULL;
	void *buf = NULL;
	unsigned int len = 0;

	hdr.sequence = 100;
	hdr.payload_len = 20;
	msg.hdr = &hdr;

	msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_WRITE;
	data.data_len = strlen("ABCDEFGHIJKLMNOPQRSTUVWXYZ") + 1;
	data.data_val.len = strlen("ABCDEFGHIJKLMNOPQRSTUVWXYZ") + 1;
	data.data_val.data = (uint8_t *) "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	
	writecmd.offset = 0;
	writecmd.count = 100;
	writecmd.data = &data;

	cmd.write_cmd = &writecmd;

	msg.command_struct = &cmd;

	len = sstack_payload_t__get_packed_size(&msg);
	buf = malloc(len);

	sstack_payload_t__pack(&msg, buf);
	fprintf(stderr,"Writing %d serialized bytes\n",len);
	fwrite (buf, len, 1, stdout);

	// Go ahead and unpack the structure
	fprintf(stdout, "\n");
	msg1 = sstack_payload_t__unpack(NULL, len, buf);
	if (NULL == msg1) {
		fprintf(stderr, "Unpack failed\n");
		return -1;
	}
	hdr1 = msg1->hdr;

	printf("Hdr sequence = %d\n", hdr1->sequence);
	
	return 0;
}
