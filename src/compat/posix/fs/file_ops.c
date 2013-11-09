/**
 * @file
 *
 * @brief
 *
 * @date 06.11.2011
 * @author Anton Bondarev
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <kernel/task.h>
#include <kernel/task/idx.h>
#include <kernel/task/io_sync.h>

int close(int fd) {
	const struct task_idx_ops *ops;
	struct idx_desc *desc;

	assert(task_self_idx_table());

	desc = task_self_idx_get(fd);
	if (!desc) {
		SET_ERRNO(EBADF);
		return -1;
	}

	ops = task_idx_desc_ops(desc);

	assert(ops);
	assert(ops->close);

	return task_self_idx_table_unbind(fd);
}

ssize_t write(int fd, const void *buf, size_t nbyte) {
	const struct task_idx_ops *ops;
	struct idx_desc *desc;

	assert(task_self_idx_table());

	desc = task_self_idx_get(fd);
	if (!desc) {
		SET_ERRNO(EBADF);
		return -1;
	}

	if (!buf) {
		SET_ERRNO(EFAULT);
		return -1;
	}

	ops = task_idx_desc_ops(desc);
	assert(ops);
	assert(ops->write);
	return ops->write(desc, buf, nbyte);
}

ssize_t read(int fd, void *buf, size_t nbyte) {
	const struct task_idx_ops *ops;
	struct idx_desc *desc;

	assert(task_self_idx_table());

	desc = task_self_idx_get(fd);
	if (!desc) {
		SET_ERRNO(EBADF);
		return -1;
	}

	if (!buf) {
		SET_ERRNO(EFAULT);
		return -1;
	}

	ops = task_idx_desc_ops(desc);
	assert(ops);
	assert(ops->read);
	return ops->read(desc, buf, nbyte);
}

off_t lseek(int fd, off_t offset, int origin) {
	const struct task_idx_ops *ops;
	struct idx_desc *desc;

	assert(task_self_idx_table());

	desc = task_self_idx_get(fd);
	if (!desc) {
		SET_ERRNO(EBADF);
		return -1;
	}

	ops = task_idx_desc_ops(desc);
	assert(ops);
	assert(ops->fseek);
	return ops->fseek(desc, offset, origin);
}

int ioctl(int fd, int request, ...) {
	va_list args;
	const struct task_idx_ops *ops;
	int rc = -ENOTSUP;
	struct idx_desc *desc;

	assert(task_self_idx_table());

	desc = task_self_idx_get(fd);
	if (!desc) {
		SET_ERRNO(EBADF);
		rc = -1;
		DPRINTF(("EBADF "));
		goto end;
	}

	ops = task_idx_desc_ops(desc);

	assert(ops);

	va_start(args, request);

	switch (request) {
  	case FIONBIO:
		/* POSIX says, that this way to make read/write nonblocking is old
		 * and recommend use fcntl(fd, O_NONBLOCK, ...)
		 * */
		if (va_arg(args, int) != 0) {
			fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | O_NONBLOCK);
		}
		break;
	case FIONREAD:
		/* FIXME set actual amount of bytes */
		*va_arg(args, int *) = io_sync_ready(
				task_idx_desc_ios(desc), IO_SYNC_READING);
		return 0;
	}

	if (NULL == ops->ioctl) {
		rc = -1;
	} else {
		void *data = va_arg(args, void*);
		rc = ops->ioctl(desc, request, data);
	}

	va_end(args);

	end:
	DPRINTF(("ioctl(%d) = %d\n", fd, rc));
	return rc;
}

int fstat(int fd, struct stat *buff) {
	const struct task_idx_ops *ops;
	struct idx_desc *desc;
	int rc;

	assert(task_self_idx_table());

	desc = task_self_idx_get(fd);
	if (!desc) {
		SET_ERRNO(EBADF);
		DPRINTF(("EBADF "));
		rc = -1;
		goto end;
	}

	ops = task_idx_desc_ops(desc);
	assert(ops);
	if(NULL != ops->fstat) {
		rc = ops->fstat(desc, buff);
	}
	else {
		rc = -1;
	}

	end:
	DPRINTF(("fstat(%d) = %d\n", fd, rc));
	return rc;

}

/* XXX -- whole function seems to be covered by many workarounds
 * try blame it -- Anton Kozlov */
int fcntl(int fd, int cmd, ...) {
	va_list args;
	int res, err, flag;
	const struct task_idx_ops *ops;
	struct idx_desc *desc;

	assert(task_self_idx_table());

	desc = task_self_idx_get(fd);
	if (!desc) {
		DPRINTF(("EBADF "));
		err = -EBADF;
		res = -1;
		goto end;
	}

	flag = 0;
	ops = task_idx_desc_ops(desc);

	va_start(args, cmd);

	err = -EINVAL;
	res = -1;

	/* Fcntl works in two steps:
	 * 1. Make general commands like F_SETFD, F_GETFD.
	 * 2. If fd has some internal fcntl(), it will be called.
	 *    Otherwise result of point 1 will be returned. */
	switch(cmd) {
	case F_GETFD:
		res = *task_idx_desc_flags_ptr(desc);
		err = 0;
		break;
	case F_SETFD:
		flag = va_arg(args, int);
		*task_idx_desc_flags_ptr(desc) = flag;
		res = 0;
		err = 0;
		break;
	case F_GETPIPE_SZ:
	case F_SETPIPE_SZ:
		err = -ENOTSUP;
		res = -1;
		break;
	default:
		err = -EINVAL;
		res = -1;
		break;
	}

#if 0
	if (NULL == ops->fcntl) {
		if (NULL == ops->ioctl) {
			goto end;
		}

		/* default operation already done regardless of result,
 		 * is it right?
		 */
		if ((err = ops->ioctl(desc, cmd, (void *)flag))) {
			res = -1;
		}

		DPRINTF(("fcntl->ioctl(%d, %d) = %d\n", fd, cmd, res));
		goto end;
	}

	/* default operation already done regardless of result,
	 * is it right?
	 */
	if ((err = ops->fcntl(desc, cmd, args))) {
		res = -1;
	}
#endif

	res = ops->ioctl(desc, cmd, (void *)flag);

	va_end(args);
end:
	DPRINTF(("fcntl(%d, %d) = %d\n", fd, cmd, res));
	SET_ERRNO(-err);
	return res;
}

int fsync(int fd) {

	DPRINTF(("fsync(%d) = %d\n", fd, 0));
	return 0;
}

int ftruncate(int fd, off_t length) {
	const struct task_idx_ops *ops;
	struct idx_desc *desc;

	assert(task_self_idx_table());

	desc = task_self_idx_get(fd);

	if (!desc) {
		SET_ERRNO(EBADF);
		return -1;
	}

	ops = task_idx_desc_ops(desc);
	assert(ops);

	if (!ops->ftruncate) {
		SET_ERRNO(EBADF);
		return -1;
	}

	return ops->ftruncate(desc, length);
}
