/*************************************************************************
 *
 * SEAMLESSSTACK CONFIDENTIAL
 * __________________________
 *
 *  [2012] - [2013]  SeamlessStack Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of SeamlessStack Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to SeamlessStack Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from SeamlessStack Incorporated.
 */

size_t serialize_branch(sfs_client_request_t *req, char* buf)
{
	size_t size = 0;

	if (!req)
		return size;

	if (buf) {
		unsigned int tmp;

		tmp = htonl(req->hdr.magic);
		memcpy(buf, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl(req->hdr.type);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		memcpy(buf + size, req->u1.branches, BRANCH_MAX);
		size += BRANCH_MAX;
		memcpy(buf + size, req->u1.login_name, LOGIN_NAME_MAX);
		size += LOGIN_NAME_MAX;
	}

	return size;
}

size_t serialize_policy(sfs_client_request_t *req, char *buf)
{
	size_t size = 0;

	if (!req)
		return size;

	if (buf) {
		unsigned int tmp;

		tmp = htonl(req->hdr.magic);
		memcpy(buf, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl(req->hdr.type);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		memcpy(buf + size, req->u2.fname, PATH_MAX);
		size += PATH_MAX;
		memcpy(buf + size, req->u2.ftype, TYPENAME_MAX);
		size += TYPENAME_MAX;
		tmp = htonl((int) req->u2.uid);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl((int) req->u2.gid);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl((int) req->u2.hidden);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl((int) req->u2.qoslevel);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
	}

	return size;
}
