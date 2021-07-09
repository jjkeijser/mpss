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

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class shutdown_status {
private:
	bool m_requested;

	std::mutex m_mutex;
	std::condition_variable m_cv;

public:
	shutdown_status() {
		m_requested = false;
	}
	bool is_shutdown_requested();
	void request_shutdown();
	void wait_for_shutdown();
};

class waitable_counter {
private:
	int m_count;
	std::mutex m_mutex;
	std::condition_variable m_cv;
public:
	waitable_counter(): m_count(0) { }
	waitable_counter(int init): m_count(init) { }

	void inc();
	void dec();
	template<class T>
	void wait_for_value(int value, int wake_period, T abortion) {
		auto timeout = std::chrono::milliseconds(wake_period);
		std::unique_lock<std::mutex> l(m_mutex);
		while (m_count != value) {
			if (abortion()) {
				break;
			}
			m_cv.wait_for(l, timeout, [&] { return m_count == value;});
		}
	}
};

