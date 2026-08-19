/*
 * sf2000-devtest - Sequential hardware device test runner.
 *
 * Launched by sf2000-powerd on START+A.  Runs each device test binary,
 * collects exit codes, and logs a summary to kmsg.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

static void log_line(const char *msg)
{
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd >= 0) {
		write(fd, msg, strlen(msg));
		close(fd);
	}
}

int main(void)
{
	static const char *tests[] = {
		"/usr/bin/test_efuse_device",
		"/usr/bin/test_vdec_device",
		"/usr/bin/test_dsc_device",
	};
	char line[128];
	int failures = 0;
	unsigned i;

	log_line("sf2000-devtest: device test suite begin\n");

	for (i = 0; i < 3; i++) {
		pid_t pid = vfork();
		int status = 0;

		if (pid == 0) {
			char *argv[] = { (char *)tests[i], NULL };
			char *envp[] = { NULL };

			execve(tests[i], argv, envp);
			_exit(127);
		}
		if (pid < 0) {
			failures++;
			continue;
		}
		waitpid(pid, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			failures++;
			snprintf(line, sizeof(line),
				"sf2000-devtest: FAIL %s status=%d\n",
				tests[i], status);
			log_line(line);
		}
	}

	snprintf(line, sizeof(line),
		"sf2000-devtest: suite complete failures=%d\n", failures);
	log_line(line);
	return failures ? 1 : 0;
}
