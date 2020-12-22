/*
The MIT License (MIT)

Copyright (c) 2015-2017 Wayne Lin.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

//
// last modified: 2017/04/24
// 

//
// this is a header-only library
// in one cpp:
//   #define TIMER_IMPLEMENTATION
//   #include <timer.h>
//
// to use:
//   unlin::Timer timer;
//   timer.Start();
//   do_some_work();
//   timer.Update();
//   float deltaTime = timer.deltaTime;
//   std::string timeSinceStartString = timer.ToStdString();
//

#ifndef TIMER_H_
#define TIMER_H_

#ifdef _MSC_VER
#include <Windows.h>
#elif __APPLE__
#include <sys/time.h>
#endif

#include <string>
#include <cstdio>

class Timer
{	
public:
	Timer(void);
	void Start(long initMktime = 0);
	void Update(void);
	float DeltaTime(void);
	float timeScale;
	float TimeSinceStart(void);
	std::string ToStdString(void);
private:
	long totalMktime;
	float deltaTime;
#ifdef _MSC_VER
	float frequency;
	__int64 lastUpdate;
#elif __APPLE__
	long lastUpdate;
#else
	no_timer_implementation_available_in_this_environment
#endif
};

#ifdef TIMER_IMPLEMENTATION

Timer::Timer(void) :
	timeScale(1.0f)
{	
	Start(0);
}

void Timer::Start(long initMktime)
{
	totalMktime = initMktime;
	deltaTime = 0.0f;
#ifdef _MSC_VER
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	frequency = float(f.QuadPart) / 1000.0f;
	QueryPerformanceCounter(&f);
	lastUpdate = f.QuadPart;
#elif __APPLE__
	timeval time;
	gettimeofday(&time, NULL);
	lastUpdate = time.tv_sec * 1000 + time.tv_usec / 1000;
#endif
}

void Timer::Update(void)
{
#ifdef _MSC_VER
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	deltaTime =  (t.QuadPart - lastUpdate) / frequency * timeScale;
	totalMktime += long(deltaTime);
	deltaTime /= 1000.0f;
	lastUpdate = t.QuadPart;
#elif __APPLE__
	timeval time;
	gettimeofday(&time, NULL);
	long current = time.tv_sec * 1000 + time.tv_usec / 1000;
	deltaTime = (current - lastUpdate) * timeScale;
	totalMktime += long(deltaTime);
	deltaTime /= 1000.0f;
	lastUpdate = current;
#endif
}

std::string Timer::ToStdString(void)
{
	char buffer[18];
#ifdef _MSC_VER
	sprintf_s(buffer, "%02d:%02d:%02d'%03d",
#else
	sprintf(buffer, "%02d:%02d:%02d'%03d",
#endif
		totalMktime / 3600000 % 24,
		totalMktime / 60000 % 60,
		totalMktime / 1000 % 60,
		totalMktime % 1000);
	return buffer; // implicitly cast char* to std::string
}

float Timer::DeltaTime(void)
{
	return deltaTime;
}

float Timer::TimeSinceStart(void)
{
	return totalMktime / 1000.0f;
}

#endif //  TIMER_IMPLEMENTATION

#endif // TIMER_H
