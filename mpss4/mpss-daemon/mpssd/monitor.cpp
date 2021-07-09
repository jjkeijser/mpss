/*
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include "mpssd.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <pwd.h>
#include <scif.h>
#include <string.h>
#include <sys/stat.h>

#define MONITOR_START		1
#define MONITOR_START_ACK	2

#define REQ_CREDENTIAL		4
#define REQ_CREDENTIAL_ACK	5
#define REQ_CREDENTIAL_NACK	6
#define MONITOR_STOPPING	7

#define CRED_SUCCESS		0
#define CRED_FAIL_UNKNOWNUID	1
#define CRED_FAIL_READCOOKIE	2
#define CRED_FAIL_MALLOC	3

void
mpssd_info::card_monitor_thread()
{
	unsigned int proto;
	unsigned int jobid;
	uint16_t stopID;
	struct scif_pollepd pfds;

	set_thread_name(name().c_str(), "monitor");

	pfds.epd = m_recv_ep;
	pfds.events = SCIF_POLLIN | SCIF_POLLERR;
	while (1) {
		if (scif_poll(&pfds, 1, 1000) == 0) {
			continue;
		}
		if (pfds.revents & (SCIF_POLLHUP | SCIF_POLLERR)) {
			close_scif_connection();
			break;
		}

		if (scif_recv(m_recv_ep, &proto, sizeof(proto), SCIF_RECV_BLOCK) < 0) {
			if (errno == ECONNRESET) {
				mpssd_log(PERROR, "MIC card mpssd daemon disconnect: %s",
					strerror(errno));
				close_scif_connection();
				break;
			}
			continue;
		}

		switch (proto) {
		case REQ_CREDENTIAL_ACK:
		case REQ_CREDENTIAL_NACK:
			if (scif_recv(m_recv_ep, &jobid, sizeof(jobid), SCIF_RECV_BLOCK) < 0) {
				mpssd_log(PERROR, "MIC card mpssd daemon error %s",
					strerror(errno));
				continue;
			}
			g_manager.close_authentication_job(jobid);

			break;

		case MONITOR_STOPPING:
			if (scif_recv(m_recv_ep, &stopID, sizeof(stopID), SCIF_RECV_BLOCK) < 0) {
				mpssd_log(PERROR, "MIC card mpssd daemon error %s", strerror(errno));
				continue;
			}
			mpssd_log(PERROR, "card mpssd daemon exiting");
			close_scif_connection();
			return;
		}
	}
}

int
mpssd_info::establish_connection(scif_epd_t recv_ep, struct scif_portID recvID)
{
	struct scif_portID sendID = {0, MPSSD_MONSEND};
	scif_epd_t send_ep;
	unsigned int msg;
	uint16_t send_port;
	uint16_t remote_port = 0;
	int err;

	if (m_recv_ep != -1 || m_send_ep != -1) {
		mpssd_log(PINFO, "another card monitor thread is running");
		return -EINVAL;
	}

	m_recv_ep = recv_ep;

	if ((err = scif_recv(recv_ep, &msg, sizeof(msg), SCIF_RECV_BLOCK)) != sizeof(msg)) {
		mpssd_log(PINFO, "MIC card mpssd daemon startup connection error %s",
				strerror(errno));
		return -EINVAL;
	}

	if (msg != MONITOR_START) {
		mpssd_log(PERROR, "Invalid message: %d", msg);
		return -EINVAL;
	}

	if ((send_ep = scif_open()) < 0) {
		mpssd_log(PINFO, "Failed to open SCIF: %s", strerror(errno));
		return -EINVAL;
	}
	m_send_ep = send_ep;

	sendID.node = m_mic->id + 1;
	while ((send_port = scif_connect(send_ep, &sendID)) < 0) {
		mpssd_log(PINFO, "Failed to connect to monitor thread on card: %s",
			strerror(errno));
		sleep(1);
	}

	// Over reliable connection, mpssd tells us which port number it uses
	// to talk back to us. If this port matches actual recv_ep remote port
	// then we know that recv_ep and send_ep reference the same client.
	// We also know that send_ep, references mpssd on mic, as port we
	// connect to on that endpoint requires privileges to listen on.
	if (scif_recv(send_ep, &remote_port, sizeof(remote_port), SCIF_RECV_BLOCK) < 0) {
		mpssd_log(PINFO, "MIC card mpssd daemon handshake error %s",
			strerror(errno));
		return -EINVAL;
	}

	if (remote_port != recvID.port || sendID.node != recvID.node) {
		mpssd_log(PINFO, "Failed to authenticate connection with mic mpssd");
		return -EINVAL;
	}

	// Similarly, provide info for the client, so that he can also verify
	// that both connections send_ep & recv_ep belong to us.
	if (scif_send(recv_ep, &send_port, sizeof(send_port), SCIF_SEND_BLOCK) < 0) {
		mpssd_log(PINFO, "MIC card mpssd daemon handshake error %s",
			strerror(errno));
		return -EINVAL;
	}

	start_monitor_thread();
	msg = MONITOR_START_ACK;
	scif_send(send_ep, &msg, sizeof(msg), SCIF_RECV_BLOCK);
	mpssd_log(PINFO, "Monitor connection established: %s", name().c_str());
	return 0;
}

#define MPSS_COOKIE_SIZE 8

int
mpssd_info::send_coi_authentication_data_to_card(unsigned int jobid, char *username, char *cookie)
{
	unsigned int proto;
	unsigned int len;
	if (m_send_ep != -1) {
		proto = REQ_CREDENTIAL;
		if ((scif_send(m_send_ep, &proto, sizeof(proto), 0)) < 0) {
			if (errno == ECONNRESET) {
				return -ECONNRESET;
			}
		}
		len = strlen(username);
		scif_send(m_send_ep, &jobid, sizeof(jobid), 0);
		scif_send(m_send_ep, &len, sizeof(len), 0);
		scif_send(m_send_ep, username, len, 0);
		len = MPSS_COOKIE_SIZE;
		scif_send(m_send_ep, &len, sizeof(len), 0);
		scif_send(m_send_ep, cookie, len, SCIF_SEND_BLOCK);
	}
	return 0;
}

void
mpssd_manager::monitor_thread()
{
	struct mpssd_info *mpssdi;
	struct scif_portID recvID;
	scif_epd_t lep;
	scif_epd_t recv_ep;
	struct scif_pollepd pfds;

	set_thread_name("main", "monitor");

	if ((lep = scif_open()) < 0) {
		mpssd_log(PINFO, "Cannot open mpssd monitor SCIF listen port: %s", strerror(errno));
		return;
	}

	if (scif_bind(lep, MPSSD_MONRECV) < 0) {
		mpssd_log(PINFO, "Cannot bind to mpssd monitor SCIF PORT: %s", strerror(errno));
		scif_close(lep);
		return;
	}

	if (scif_listen(lep, 16) < 0) {
		mpssd_log(PINFO, "Set Listen on mpssd monitor SCIF PORT fail: %s", strerror(errno));
		scif_close(lep);
		return;
	}

	pfds.epd = lep;
	pfds.events = SCIF_POLLIN | SCIF_POLLERR;

	while (1) {
		if (m_status.is_shutdown_requested()) {
			break;
		}
		if (scif_poll(&pfds, 1, 1000) == 0) {
			continue;
		}

		if (scif_accept(lep, &recvID, &recv_ep, 0)) {
			if (errno != EINTR)
				mpssd_log(PINFO, "Wait for card connect failed: %s", strerror(errno));
			sleep(1);
			continue;
		}

		mpssd_log(PINFO, "receiving node: %d", recvID.node);
		if ((mpssdi = get_card_by_id(recvID.node - 1)) == NULL) {
			mpssd_log(PINFO, "Cannot configure - node %d does not seem to exist",
				       recvID.node - 1);
			scif_close(recv_ep);
			continue;
		}
		if ((mpssdi->establish_connection(recv_ep, recvID)) != 0) {
			mpssdi->close_scif_connection();
		}
	}
	scif_close(lep);
}


static int
get_cookie(struct passwd *pass, char *cookie)
{
	char cookiename[PATH_MAX];
	struct stat sbuf;
	int createcookie = TRUE;
	int len;
	int fd;
	int err = -1;

	snprintf(cookiename, PATH_MAX, "%s/.mpsscookie", pass->pw_dir);

	if (setegid(pass->pw_gid) < 0) {
		mpssd_log(PERROR, "%s Cannot create: Failed to setegid to gid %d : %s",
				cookiename, pass->pw_gid, strerror(errno));
		return -1;
	}

	if (seteuid(pass->pw_uid) < 0) {
		mpssd_log(PERROR, "%s Cannot create: Failed to seteuid to uid %d : %s",
				cookiename, pass->pw_uid, strerror(errno));
		setegid(0);
		return -1;
	}

	if (lstat(cookiename, &sbuf) == 0) {
		if (S_ISLNK(sbuf.st_mode)) {
			if (unlink(cookiename) < 0) {
				mpssd_log(PERROR, "%s Cannot create: is a link and removal failed: %s",
					cookiename, strerror(errno));
				goto cookie_done;
			}

			mpssd_log(PERROR, "%s: Is a link - remove and recreate", cookiename);

		} else if (sbuf.st_nlink != 1) {
			if (unlink(cookiename) < 0) {
				mpssd_log(PERROR, "%s Cannot create: has more than one hard link and "
						"removal failed: %s", cookiename, strerror(errno));
				goto cookie_done;
			}

			mpssd_log(PERROR, "%s: Too many hard links - remove and recreate", cookiename);

		} else {
			createcookie = FALSE;
		}
	}

	if (!createcookie) {
		if ((fd = open(cookiename, O_RDONLY)) < 0) {
			mpssd_log(PERROR, "Failed to open %s: %s", cookiename, strerror(errno));
			goto cookie_done;
		}

		if ((len = read(fd, cookie, MPSS_COOKIE_SIZE)) != MPSS_COOKIE_SIZE) {
			if (unlink(cookiename) < 0) {
				mpssd_log(PERROR, "Cannot create cookie file %s bad size and removal failed: %s",
					cookiename, strerror(errno));
				goto cookie_done;
			}

			mpssd_log(PERROR, "%s: Bad size remove and recreate", cookiename);
			createcookie = TRUE;
		}
		close(fd);
	}

	if (createcookie) {
		if ((fd = open("/dev/urandom", O_RDONLY)) < 0) {
			mpssd_log(PERROR, "Create cookie %s failed to open dev random: %s", cookiename, strerror(errno));
			goto cookie_done;
		}

		len = read(fd, cookie, MPSS_COOKIE_SIZE);
		close(fd);

		if ((fd = open(cookiename, O_WRONLY|O_CREAT)) < 0) {
			mpssd_log(PERROR, "Failed to open %s: %s", cookiename, strerror(errno));
			goto cookie_done;
		}

		write(fd, cookie, len);
		fchmod(fd, S_IRUSR);
		fchown(fd, pass->pw_uid, pass->pw_gid);
		close(fd);
	}

	err = 0;

cookie_done:
	seteuid(0);
	setegid(0);
	return err;
}

struct passwd *
mpssd_manager::find_passwd_entry(uid_t uid)
{
	struct passwd *pass = NULL;
	while ((pass = getpwent()) != NULL) {
		if (uid == pass->pw_uid) {
			break;
		}
	}
	endpwent();
	return pass;
}

void
mpssd_manager::coi_authenticate_thread()
{
	struct mpssd_info *mpssdi;
	struct jobs *job;
	struct scif_portID portID;
	struct passwd *pass;
	char *username = NULL;
	char cookie[MPSS_COOKIE_SIZE];
	unsigned int proto;
	scif_epd_t lep;
	scif_epd_t dep;
	uid_t uid;
	int err;
	struct scif_pollepd pfds;

	set_thread_name("main", "coiauth");

	if ((lep = scif_open()) < 0) {
		mpssd_log(PINFO, "Cannot open mpssd credentials SCIF listen port: %s",
			       strerror(errno));
		return;
	}

	if (scif_bind(lep, MPSSD_CRED) < 0) {
		mpssd_log(PINFO, "Cannot bind to mpssd credentials SCIF PORT: %s", strerror(errno));
		scif_close(lep);
		return;
	}

	if (scif_listen(lep, 16) < 0) {
		mpssd_log(PINFO, "Set Listen on mpssd credentials SCIF PORT fail: %s", strerror(errno));
		scif_close(lep);
		return;
	}

	pfds.epd = lep;
	pfds.events = SCIF_POLLIN | SCIF_POLLERR;
	while (1) {
		if (m_status.is_shutdown_requested()) {
			break;
		}
		if (scif_poll(&pfds, 1, 1000) == 0) {
			continue;
		}

		if (scif_accept(lep, &portID, &dep, 0)) {
			if (errno != EINTR) {
				mpssd_log(PINFO, "Wait for credentials request fail: %s", strerror(errno));
			}
			continue;
		}

		if ((err = scif_recv(dep, &uid, sizeof(uid), SCIF_RECV_BLOCK)) != sizeof(uid)) {
			mpssd_log(PINFO, "Credential connect receive error %s", strerror(errno));
			scif_close(dep);
			continue;
		}

		pass = find_passwd_entry(uid);
		if (pass == NULL) {
			mpssd_log(PERROR, "User request unknown UID %d", uid);
			proto = CRED_FAIL_UNKNOWNUID;
			scif_send(dep, &proto, sizeof(proto), 0);
			scif_close(dep);
			continue;
		}
		username = pass->pw_name;

		if (get_cookie(pass, cookie) < 0) {
			proto = CRED_FAIL_READCOOKIE;
			scif_send(dep, &proto, sizeof(proto), 0);
			scif_close(dep);
			continue;
		}

		if ((job = new(std::nothrow) jobs) == nullptr) {
			proto = CRED_FAIL_MALLOC;
			scif_send(dep, &proto, sizeof(proto), 0);
			scif_close(dep);
			continue;
		}
		m_job_mutex.lock();
		job->jobid = m_nextjobid++;
		job->dep = dep;
		job->cnt = 0;

		for (auto& iter : m_map) {
			mpssdi = iter.second;
			if (!mpssdi->send_coi_authentication_data_to_card(
				job->jobid, username, cookie)) {
				job->cnt++;
			}
		}
		if (job->cnt == 0) {
			proto = CRED_SUCCESS;
			scif_send(job->dep, &proto, sizeof(proto), 0);
			scif_close(job->dep);
		} else {
			g_manager.m_job[job->jobid] = job;
		}
		m_job_mutex.unlock();
	}
	scif_close(lep);
}

void
mpssd_manager::close_authentication_job(unsigned int jobid)
{
	unsigned int proto;
	std::map<unsigned int, jobs *>::iterator it;

	m_job_mutex.lock();
	it = m_job.find(jobid);
	if (it != m_job.end()) {
		if (--it->second->cnt == 0) {
			proto = CRED_SUCCESS;
			scif_send(it->second->dep, &proto, sizeof(proto), 0);
			scif_close(it->second->dep);
			delete it->second;
			m_job.erase(it);
		}
	}
	m_job_mutex.unlock();
}
