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

#include "sync_utils.h"

bool shutdown_status::is_shutdown_requested()
{
	std::unique_lock<std::mutex> l(m_mutex);
	return m_requested;
}
void shutdown_status::request_shutdown() {
	{
		std::unique_lock<std::mutex> l(m_mutex);
		m_requested = true;
	}
	m_cv.notify_all();
}
void shutdown_status::wait_for_shutdown() {
	std::unique_lock<std::mutex> l(m_mutex);
	m_cv.wait(l, [&] { return m_requested; });
}

void waitable_counter::inc() {
	{
		std::unique_lock<std::mutex> l(m_mutex);
		m_count++;
	}
	m_cv.notify_all();
}
void waitable_counter::dec() {
	{
		std::unique_lock<std::mutex> l(m_mutex);
		m_count--;
	}
	m_cv.notify_all();
}

