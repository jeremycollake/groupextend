#pragma once

#include <Windows.h>
#include "LogOut.h"

int ExtendGroupForProcess(unsigned long pid, HANDLE hQuitNotifyEvent, LogOut& Log);
