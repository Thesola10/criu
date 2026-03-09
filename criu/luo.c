#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "linux/liveupdate.h"
#include "image-desc.h"
#include "cr_options.h"

#include "luo.h"

static int luo_create_session(int luo_fd, const char *name)
{
	struct liveupdate_ioctl_create_session arg = { .size = sizeof(arg) };

	pr_debug("Creating LUO session '%s'\n", name);

	snprintf((char *)arg.name, LIVEUPDATE_SESSION_NAME_LENGTH, "%.*s", LIVEUPDATE_SESSION_NAME_LENGTH - 1, name);

	if (ioctl(luo_fd, LIVEUPDATE_IOCTL_CREATE_SESSION, &arg) < 0)
		return -errno;

	return arg.fd;
}

static int luo_retrieve_session(int luo_fd, const char *name)
{
	struct liveupdate_ioctl_retrieve_session arg = { .size = sizeof(arg) };

	pr_debug("Restoring LUO session '%s'\n", name);

	snprintf((char *)arg.name, LIVEUPDATE_SESSION_NAME_LENGTH, "%.*s", LIVEUPDATE_SESSION_NAME_LENGTH - 1, name);

	if (ioctl(luo_fd, LIVEUPDATE_IOCTL_RETRIEVE_SESSION, &arg) < 0)
		return -errno;

	return arg.fd;
}

static uint64_t luo_get_token(const struct cr_img *image)
{
	union {
		uint64_t raw;
		struct {
			uint32_t type;
			uint32_t param;
		} __attribute__((packed));
	} token;

	token.type = image->type;
	sscanf(image->path, imgset_template[image->type].fmt, &token.param);

	return token.raw;
}

int luo_session_init(const char *session)
{
	int luo_fd = open(LUO_DEVICE, O_RDWR);
	if (luo_fd < 0)
		return -errno;

	opts.luo_session_fd = luo_create_session(luo_fd, session);
	if (opts.luo_session_fd < 0) {
		close(luo_fd);
		return -errno;
	}
	close(luo_fd);
	return 0;
}

int luo_session_open(const char *session)
{
	int luo_fd = open(LUO_DEVICE, O_RDWR);
	if (luo_fd < 0)
		return -errno;

	opts.luo_session_fd = luo_retrieve_session(luo_fd, session);
	if (opts.luo_session_fd < 0) {
		close(luo_fd);
		return -errno;
	}
	close(luo_fd);
	return 0;
}

int luo_image_open(const struct cr_img *image, int flags)
{
	if (opts.luo_session_fd < 0)
		return -ENOENT;

	if (flags & O_WRONLY) {
		int mfd;

		pr_debug("Creating in-memory image '%s'\n", image->path);
		mfd = memfd_create(image->path, 0);
		if (mfd < 0)
			return -errno;
		return mfd;
	} else {
		struct liveupdate_session_retrieve_fd arg = {
			.size = sizeof(arg),
			.token = luo_get_token(image)
		};

		pr_debug("Retireving image '%s' from token '%0llx'\n", image->path, arg.token);

		if (ioctl(opts.luo_session_fd, LIVEUPDATE_SESSION_RETRIEVE_FD, &arg) < 0)
			return -errno;

		lseek(arg.fd, 0, SEEK_SET);

		return arg.fd;
	}
}

int luo_image_close(const struct cr_img *image)
{
	struct liveupdate_session_preserve_fd arg = {
		.size = sizeof(arg),
		.token = luo_get_token(image),
		.fd = image->fd
	};

	if (opts.luo_session_fd < 0)
		return -ENOENT;

	pr_debug("Saving image '%s' as token '%0llx'\n", image->path, arg.token);

	if (ioctl(opts.luo_session_fd, LIVEUPDATE_SESSION_PRESERVE_FD, &arg) < 0)
		return -errno;

	return 0;
}

void luo_session_hang(void)
{
	pr_warn("Standing by for kexec...\n");
	for (;;)
		pause();
}

int luo_session_finish(void)
{
	struct liveupdate_session_finish arg = { .size = sizeof(arg) };

	if (ioctl(opts.luo_session_fd, LIVEUPDATE_SESSION_FINISH, &arg) < 0)
		return -errno;

	if (opts.luo_session)
		close(opts.luo_session_fd);
	return 0;
}
