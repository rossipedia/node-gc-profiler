#include <node.h>
#include "../node_modules/nan/nan.h"
#include <time.h>

#ifdef WIN32
#include <windows.h>
#else
#include <chrono>
#endif

using namespace v8;

namespace GcProfiler
{
	struct GcProfilerData
	{
    	uv_work_t request;
		time_t startTime;
		GCType type;
		GCCallbackFlags flags;
		double duration;
	};

	// static variables
	GcProfilerData * _data;
	Persistent<Function> _callback;
	Persistent<Context> _context;
	
#ifdef WIN32
	
	double _pcFreq = 0.0;
	__int64 _counterStart = 0;
	
#else

	typedef std::chrono::duration<double, std::ratio<1, 1000>> millisecondsRatioDouble;
	std::chrono::time_point<std::chrono::high_resolution_clock> _timePointStart;

#endif
	
	// function prototypes
	void Init(Handle<Object> exports);
	NAN_METHOD(LoadProfiler);
	NAN_GC_CALLBACK(Before);
	NAN_GC_CALLBACK(After);
	void UvAsyncWork(uv_work_t * req);
	void UvAsyncAfter(uv_work_t * req);
	void StartTimer();
	double EndTimer();
	
	// init
	NODE_MODULE(GcProfiler, Init)
	
	// --- functions ---
	
	void Init (Handle<Object> exports)
	{
		NODE_SET_METHOD(exports, "loadProfiler", LoadProfiler);
	}

	NAN_METHOD(LoadProfiler)
	{
		NanScope();
		
		if (args.Length() == 0 || !args[0]->IsFunction())
		{
			NanThrowTypeError("Must provide a callback function to the profiler.");
		}
		
		NanAssignPersistent(_callback, args[0].As<Function>());
		NanAddGCPrologueCallback(Before);
		NanAddGCEpilogueCallback(After);
		
		NanReturnUndefined();
	}
	
	NAN_GC_CALLBACK(Before)
	{
		_data = new GcProfilerData();
		_data->startTime = time(NULL);
		StartTimer();
	}
	
	NAN_GC_CALLBACK(After)
	{
		_data->duration = EndTimer();
		_data->type = type;
		_data->flags = flags;
		_data->request.data = _data;
		
		// can't call the callback immediately - need to defer to when the event loop is ready
		uv_queue_work(uv_default_loop(), &_data->request, UvAsyncWork, (uv_after_work_cb)UvAsyncAfter);
	}
	
	void UvAsyncWork(uv_work_t * req)
	{
		// we don't actually have any work to do, we only care about the "after" callback
	}
	
	void UvAsyncAfter(uv_work_t * req)
	{
		NanScope();
		
		GcProfilerData * data = (GcProfilerData*)req->data;
		
		const unsigned argc = 4;
		Handle<Value> argv[argc] = {
			NanNew<Number>(data->startTime),
			NanNew<Number>(data->duration),
			NanNew<Number>((int)data->type),
			NanNew<Number>((int)data->flags)
		};
		
		delete data;
		NanMakeCallback(NanGetCurrentContext()->Global(), NanNew(_callback), argc, argv);
	}
	
#ifdef WIN32
	
	void StartTimer ()
	{
		LARGE_INTEGER li;
		
		if (_pcFreq == 0.0)
		{
			QueryPerformanceFrequency(&li);
			_pcFreq = (double)li.QuadPart / 1000; // so that the freq is in ms instead of seconds.
		}
		
		QueryPerformanceCounter(&li);
		_counterStart = li.QuadPart;
	}
	
	double EndTimer ()
	{
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		return double(li.QuadPart - _counterStart) / _pcFreq;
	}
#else

	void StartTimer ()
	{
		_timePointStart = std::chrono::high_resolution_clock::now();
	}
	
	double EndTimer ()
	{
		auto duration = std::chrono::high_resolution_clock::now() - _timePointStart;
		return millisecondsRatioDouble(duration).count();
	}

#endif

};
