// SPDX-License-Identifier: MIT
#include <unistd.h>

static char path[] = "/usr/sbin/sf2000-panel-probe";
static char *argv[] = { path, 0 };
static char env_home[] = "HOME=/";
static char env_path[] = "PATH=/bin:/sbin:/usr/bin:/usr/sbin";
static char env_term[] = "TERM=linux";
static char env_pad_profile[] = "SF2000_PAD_PROFILE=auto";
static char env_screen[] = "SF2000_SCREEN=0";
static char env_heartbeat[] = "SF2000_HEARTBEAT=0";
static char env_panel_probe[] = "SF2000_PANEL_PROBE=1";
static char *envp[] = {
	env_home, env_path, env_term, env_pad_profile,
	env_screen, env_heartbeat, env_panel_probe, 0
};

int main(void)
{
	execve(path, argv, envp);
	return 127;
}
