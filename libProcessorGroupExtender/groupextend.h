/*
* (c)2022 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
*/
#pragma once

#include "LogOut.h"
#include <mutex>
#include <thread>

namespace GroupExtend
{
	const unsigned int REFRESH_MINIMUM_ALLOWED_MS = 100;
	const unsigned int REFRESH_MS = 1000;
}

// intended to manage only a single process per group extension instance
class ProcessorGroupExtender_SingleProcess
{
	std::thread m_hExtenderThread;		
	HANDLE m_hQuitNotifyEvent;	  // for signalling thread to stop
	HANDLE m_hThreadStoppedEvent; // for signalling caller thread stopped (prematurely)
	unsigned long m_pid;
	LogOut m_log;
	unsigned long m_nRefreshRateMs;	

	int ExtendGroupForProcess();
public:
	ProcessorGroupExtender_SingleProcess() : 
		m_pid(0), 
		m_nRefreshRateMs(GroupExtend::REFRESH_MS), 
		m_hQuitNotifyEvent(nullptr),
		m_hThreadStoppedEvent(nullptr)
	{}
	~ProcessorGroupExtender_SingleProcess()
	{
		if (IsActive())
		{
			Stop();
		}
		if (m_hQuitNotifyEvent)
		{
			CloseHandle(m_hQuitNotifyEvent);
		}
	}
	bool IsActive()
	{
		return m_hExtenderThread.joinable();
	}
	bool StartAsync(unsigned long pid, unsigned long nRefreshRateMs, LogOut::LOG_TARGET logTarget, HANDLE hThreadStoppedEvent)
	{			
		if (IsActive())
		{
			return false;
		}		
		m_pid = pid;
		m_log.SetTarget(logTarget);
		m_hThreadStoppedEvent = hThreadStoppedEvent;
		// if refresh rate param is above minimum, use it - otherwise use default
		if (nRefreshRateMs >= GroupExtend::REFRESH_MINIMUM_ALLOWED_MS)
		{
			m_nRefreshRateMs = nRefreshRateMs;
		}
		else
		{
			m_nRefreshRateMs = GroupExtend::REFRESH_MS;
		}
		m_hQuitNotifyEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		m_hExtenderThread = std::thread(&ProcessorGroupExtender_SingleProcess::ExtendGroupForProcess, this);
		return true;
	}
	bool Stop()
	{
		if (IsActive())
		{
			_ASSERT(m_hQuitNotifyEvent);
			SetEvent(m_hQuitNotifyEvent);
			m_hExtenderThread.join();
			CloseHandle(m_hQuitNotifyEvent);
			m_hQuitNotifyEvent = nullptr;
			return true;
		}
		return false;
	}
};

