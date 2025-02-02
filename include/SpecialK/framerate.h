/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#ifndef __SK__FRAMERATE_H__
#define __SK__FRAMERATE_H__

// TODO: Separate header for RAII classes
#include <SpecialK/utility.h>

#include <boost/sort/pdqsort/pdqsort.hpp>

#include <Unknwnbase.h>
#include <Windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <cmath>
#include <numeric>
#include <limits>
#include <forward_list>

float SK_Framerate_GetPercentileByIdx (int idx);

using QueryPerformanceCounter_pfn   = BOOL (WINAPI *)(_Out_ LARGE_INTEGER *lpPerformanceCount) noexcept;
using QueryPerformanceFrequency_pfn = BOOL (WINAPI *)(_Out_ LARGE_INTEGER *lpFrequency)        noexcept;

BOOL
WINAPI
SK_QueryPerformanceCounter (_Out_ LARGE_INTEGER *lpPerformanceCount) noexcept;

extern int64_t SK_QpcFreq;
extern int64_t SK_QpcTicksPerMs;

__forceinline
LARGE_INTEGER
SK_CurrentPerf (void) noexcept
 {
   LARGE_INTEGER                time;
   SK_QueryPerformanceCounter (&time);
   return                       time;
 };

__forceinline
LARGE_INTEGER
SK_QueryPerf (void) noexcept
 {
   LARGE_INTEGER                time;
   SK_QueryPerformanceCounter (&time);
   return                       time;
 };

static auto SK_DeltaPerf =
 [](auto delta, auto freq) noexcept ->
  LARGE_INTEGER
   {
     LARGE_INTEGER time =
       SK_CurrentPerf ();

     time.QuadPart -=
       sk::narrow_cast <LONGLONG> (
         delta * freq
       );

     return
       time;
   };

static auto SK_DeltaPerfMS =
 [](auto delta, auto freq) noexcept ->
  double
   {
     return
       1000.0 * (double)(SK_DeltaPerf (delta, freq).QuadPart) /
                (double)(SK_QpcFreq);
   };


extern  __forceinline
ULONG64 __stdcall
SK_GetFramesDrawn (void) noexcept;


namespace SK
{
  namespace Framerate
  {
    void Init     (void);
    void Shutdown (void);

    void Tick        ( bool           wait       =  true,
                       double         dt         =   0.0,
                       LARGE_INTEGER  now        = { 0,0 },
                       IUnknown*      pSwapChain = nullptr );

    #define MAX_SAMPLES 1000

    class Stats;

    struct DeepFrameState
    {
      using
        SK_LazyStats =
          SK_LazyGlobal <
            SK::Framerate::Stats
          >;

      SK_LazyStats mean;
      SK_LazyStats min;
      SK_LazyStats max;
      SK_LazyStats percentile0;
      SK_LazyStats percentile1;

      void reset (void);
    };

    class Limiter {
    public:
      Limiter (double target = 60.0, bool tracks_game_window = true);
     ~Limiter (void) = default;

      void            init            (double target, bool tracks_window = true); // Todo, use an opaque handle to denote which window
      void            wait            (void);
      bool        try_wait            (void); // No actual wait, just return
                                              //  whether a wait would have occurred.

      void        set_limit           (double target);
      double      get_limit           (void) noexcept { return fps; };

      double      effective_frametime (void);

      void        set_undershoot      (float percent) noexcept { undershoot_percent = percent; };
      float       get_undershoot      (void)          noexcept {    return undershoot_percent; };

      int32_t     suspend             (void) noexcept { return ++limit_behavior;     }
      int32_t     resume              (void) noexcept { return --limit_behavior;     }
      bool        frozen              (void) noexcept { return   limit_behavior > 0; }

      void        reset (bool full = false) noexcept {
        if (full) full_restart = true;
        else           restart = true;
      }

      struct present_stats_s {
      //DXGI_FRAME_STATISTICS sequence_start = { };
        UINT                  queue_depth    =  0;
        std::vector <UINT>    frame_ids;
      };

      struct snapshot_s {
        bool        *pRestart     = nullptr;
        bool        *pFullRestart = nullptr;
        bool        *pBackground  = nullptr;

        double       ms              = 0.0;
        double       fps             = 0.0;
        double       effective_ms    = 0.0;

        uint64_t     ticks_per_frame =   0;

        volatile
          LONG64     time  = { },
                     start = { },
                     next  = { },
                     last  = { };

        volatile
          LONG64     frames = 0;
      } rate_shot;

      snapshot_s getSnapshot (void)
      {
        rate_shot.pRestart        = &restart;
        rate_shot.pFullRestart    = &full_restart;
        rate_shot.pBackground     = &background;
        rate_shot.ms              = ms;
        rate_shot.fps             = fps;
        rate_shot.effective_ms    = effective_ms;
        rate_shot.ticks_per_frame = ticks_per_frame;

        InterlockedExchange64 (&rate_shot.time,  ReadAcquire64 (&time));
        InterlockedExchange64 (&rate_shot.start, ReadAcquire64 (&start));
        InterlockedExchange64 (&rate_shot.next,  ReadAcquire64 (&next));
        InterlockedExchange64 (&rate_shot.last,  ReadAcquire64 (&last));

        WriteRelease64 (&rate_shot.frames,
                   ReadAcquire64 (&frames));

        return rate_shot;
      }

      // Record keeping data pertaining to all frames SK has seen
      //   for the entire application runtime, irrespective of user-
      //     defined preferences for limiting / skipping.
      struct frame_journal_s
      {
        struct dataspread_s
        {
          struct
          {
            LONG64 frame_idx = -1;
            LONG64 clock_val =  0;

            void initFrame (LONG64 lvi) noexcept { frame_idx =
             ( frame_idx == -1 ) ? lvi           : frame_idx;};
            void initClock (LONG64 lvc) noexcept { clock_val =
             ( clock_val ==  0 ) ? lvc           : clock_val;};
          } first, last;

          LONG64
            count (void) const noexcept
            { return
               last.frame_idx -
              first.frame_idx;
            }
          LONG64
            duration (void) const noexcept
            { return
                 last.clock_val -
                first.clock_val;
            }
        } frames_measured;

        struct {
          dataspread_s span = { { -1, 0 },
                                { -1, 0 } };
        } limiter_resets;

      //LONG64 time_waited = 0; // TODO
      } frames_of_fame;

      struct amortized_stats_s {
        int            phase      =  0;
        LARGE_INTEGER _last_frame = { };
      } amortization;

      SK_LazyGlobal <Stats>        frame_history;
      SK_LazyGlobal <Stats>        frame_history2;
                    DeepFrameState frame_history_snapshots;

      static
      float         undershoot_percent;

      static
      double        timer_res_ms;

    private:

      bool          restart      = false;
      bool          full_restart = false;
      bool          background   = false;

      double        ms           = 0.0,
                    fps          = 0.0,
                    effective_ms = 0.0;

      ULONGLONG     ticks_per_frame     = 0ULL;
      double        accum_per_frame     =  0.0;
      ULONGLONG     ticks_to_undershoot = 0ULL;

      // Don't align timing perfectly on a VBlank interval, because DWM composition is
      //   asynchronous to our present queue and we want to arrive with work to submit
      //     before VBlank begins, not _after_.

      volatile
        LONG64      time   = { },
                    start  = { },
                    next   = { },
                    last   = { };

      volatile
        LONG64      frames = 0;
      volatile
        LONG64      last_frame = 0; // Never apply a limit twice in one frame

#define LIMIT_APPLY     0
#define LIMIT_UNDERFLOW (limit_behavvior < 0)
#define LIMIT_SUSPENDED (limit_behavivor > 0)

      // 0 = Limiter runs, < 0 = Reference Counting Bug (dumbass)
      //                   > 0 = Temporarily Ignore Limits
      int32_t        limit_behavior =
                     LIMIT_APPLY;

      SK_AutoHandle  timer_wait;
      bool           tracks_window = true;
      bool           lazy_init     = false;

      // Two limits applied on the same frame would cause problems, don't allow it.
      std::unordered_map <DWORD, ULONG64> _frame_shame;
    };

    using EventCounter = class EventCounter_V1;

    class EventCounter_V1
    {
    public:
      class SleepStats
      {
      public:
        volatile ULONG attempts   = 0UL,
                       rejections = 0UL;

        struct
        {
          volatile LONG deprived = 0ULL,
                        allowed  = 0ULL;
        } time;


        void sleep (ULONG dwMilliseconds) { InterlockedIncrement (&attempts);
                                            InterlockedAdd       (&time.allowed,  dwMilliseconds); }
        void wake  (ULONG dwMilliseconds) { InterlockedIncrement (&attempts);
                                            InterlockedIncrement (&rejections);
                                            InterlockedAdd       (&time.deprived, dwMilliseconds); }
      };

      SleepStats& getMessagePumpStats  (void) noexcept { return message_pump;  }
      SleepStats& getRenderThreadStats (void) noexcept { return render_thread; }
      SleepStats& getMicroStats        (void) noexcept { return micro_sleep;   }
      SleepStats& getMacroStats        (void) noexcept { return macro_sleep;   }

    protected:
      SleepStats message_pump, render_thread,
                 micro_sleep,  macro_sleep;
    };

    extern        EventCounter                                       events;
    static inline EventCounter* GetEvents  (void) noexcept { return &events; }

    // The identifying SwapChain is an opaque handle, we have no reason to hold a reference to this
    //   and you can cast any value you want to this pointer (e.g. an OpenGL HGLRC). Prototype uses
    //     IUnknown because it's straightforward in DXGI / D3D9 to use these as handles :)
                  bool          HasLimiter (IUnknown *pSwapChain          = nullptr);
                  Limiter*      GetLimiter (IUnknown *pSwapChain          = nullptr,
                                            bool      bCreateIfNoneExists = true   );
    class Stats {
    public:
      Stats (void) noexcept {
      }

      std::vector <double>&
        sortAndCacheFrametimeHistory (void);

      struct sample_t {
        double        val  =   0.0   ;
        LARGE_INTEGER when = { 0ULL };
      } data [MAX_SAMPLES];

      struct worker_context_s
      {
        SK_AutoHandle hSignalProduce;
        SK_AutoHandle hSignalConsume;
                ULONG ulLastFrame    = 0;
        volatile LONG work_idx       = 0;
        volatile LONG _init          = 0;

        std::pair <ULONG, std::vector <double> >
                      sorted_frame_history [2];
      } worker;

      uint64_t samples    = 0ULL;
      double   last_delta = 0.0;

      bool addSample (double sample, LARGE_INTEGER time) noexcept
      {
        data [samples % MAX_SAMPLES].val  = sample;
        data [samples % MAX_SAMPLES].when = time;


        if ((samples % MAX_SAMPLES) == 0)
          last_delta = data [samples % MAX_SAMPLES].val - data [                MAX_SAMPLES - 1].val;
        else
          last_delta = data [samples % MAX_SAMPLES].val - data [(samples - 1) % MAX_SAMPLES    ].val;


        return
          ( ( ++samples % MAX_SAMPLES ) == 0 );
      }

      double getLastDelta (void) noexcept
      {
        return last_delta;
      }

      double calcDataTimespan (void) noexcept
      {
        const uint64_t
          samples_present =
            samples < MAX_SAMPLES ?
            samples : MAX_SAMPLES;

        LARGE_INTEGER min_time   {
                     .QuadPart = std::numeric_limits <LONGLONG>::max () };
        LARGE_INTEGER max_time = {                                 0ULL };

        for ( auto sample_idx = 0ULL            ;
                   sample_idx < samples_present ;
                 ++sample_idx )
        {
          const LARGE_INTEGER sampled_time =
            data [sample_idx].when;

          if ( sampled_time.QuadPart >
                   max_time.QuadPart && sampled_time.QuadPart < std::numeric_limits <LONGLONG>::max () )
          {
                max_time.QuadPart =
            sampled_time.QuadPart;
          }

          if ( sampled_time.QuadPart <
                   min_time.QuadPart && sampled_time.QuadPart >= 0 )
          {
                min_time.QuadPart =
            sampled_time.QuadPart;
          }
        }

        return
          static_cast <double> (max_time.QuadPart - min_time.QuadPart) /
          static_cast <double> (SK_QpcFreq);
      }

         int calcNumSamples         (double seconds  = 1.0);
      double calcMean               (double seconds  = 1.0) noexcept;
      double calcSqStdDev           (double mean,
                                     double seconds  = 1.0) noexcept;
      double calcMin                (double seconds  = 1.0) noexcept;
      double calcMax                (double seconds  = 1.0) noexcept;
         int calcHitches            (double tolerance,
                                     double mean,
                                     double seconds  = 1.0) noexcept;

      double calcOnePercentLow      (double seconds  = 1.0);
      double calcPointOnePercentLow (double seconds  = 1.0);

      double calcMean               (LARGE_INTEGER start) noexcept
      {
        double mean         = 0.0;
        int    samples_used =  0 ;

        for ( const auto& datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val > 0.0 && isnormal (datum.val))
            {
              ++samples_used;
              mean += datum.val;
            }
          }
        }

        return
          ( mean / static_cast <double> (std::max (1, samples_used)) );
      }

      double calcPercentile (float percent, LARGE_INTEGER start)
      {
        UNREFERENCED_PARAMETER (start);

        std::vector <double>&
                     sampled_lows = sortAndCacheFrametimeHistory ();
        const size_t samples_used = sampled_lows.size            ();

        const size_t end_sample_idx =
          std::max ( (size_t)0, std::min
              ( samples_used,
                  sk::narrow_cast <size_t> (
                    std::round ((float)samples_used * (percent / 100.0f))
                  )
              )
          );

        const double
            avg_ms =
              std::accumulate ( sampled_lows.begin (),
                                sampled_lows.begin ()  + end_sample_idx,
                                        0.0 ) / (double)(end_sample_idx);

        return
          avg_ms;
      }

      double calcOnePercentLow (LARGE_INTEGER start)
      {
        UNREFERENCED_PARAMETER (start);

        std::vector <double>&
                     sampled_lows = sortAndCacheFrametimeHistory ();
        const size_t samples_used = sampled_lows.size            ();

        const size_t end_sample_idx =
          sk::narrow_cast <size_t> (
            std::round ((float)samples_used / 100.0f)
          );

        const double
            one_percent_avg_ms =
              std::accumulate ( sampled_lows.begin (),
                                sampled_lows.begin ()  + end_sample_idx,
                                        0.0 ) / (double)(end_sample_idx);

        return
          one_percent_avg_ms;
      }

      double calcPointOnePercentLow (LARGE_INTEGER start)
      {
        UNREFERENCED_PARAMETER (start);

        std::vector <double>&
                     sampled_lows = sortAndCacheFrametimeHistory ();
        const size_t samples_used = sampled_lows.size            ();

        ////  Severity  Code    Description
        ////  Warning   C26467    Converting from floating point to unsigned
        ////                      integral types results in non-portable code
        ////                      if the double/float has a negative value.
        ////
        ////// Use gsl::narrow_cast or gsl::narrow instead to guard against
        ////// undefined behavior and potential data loss
        const size_t end_sample_idx =
          sk::narrow_cast <size_t> (
            std::round ((float)samples_used / 1000.0f)
          );

        const double
            point_one_percent_avg_ms =
              std::accumulate ( sampled_lows.begin (),
                                sampled_lows.begin ()  + end_sample_idx,
                                        0.0 ) / (double)(end_sample_idx);

        return
          point_one_percent_avg_ms;
      }

      double calcSqStdDev (double mean, LARGE_INTEGER start) noexcept
      {
        double sd2          = 0.0;
        int    samples_used = 0;

        for ( const auto& datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val > 0.0 && isnormal (datum.val))
            {
              sd2 += (datum.val - mean) *
                     (datum.val - mean);
              samples_used++;
            }
          }
        }

        return sd2 / static_cast <double> (std::max (1, samples_used));
      }

      double calcMin (LARGE_INTEGER start) noexcept
      {
        double min = INFINITY;

        for ( const auto& datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val > 0.0 && isnormal (datum.val))
            {
              if (datum.val < min)
                min = datum.val;
            }
          }
        }

        return min;
      }

      double calcMax (LARGE_INTEGER start) noexcept
      {
        double max = -INFINITY;

        for ( const auto& datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val > 0.0 && isnormal (datum.val))
            {
              if (datum.val > max)
                max = datum.val;
            }
          }
        }

        return max;
      }

      int calcHitches (double tolerance, double mean, LARGE_INTEGER start) noexcept
      {
        int  hitches   = 0;
        bool last_late = false;

        for ( const auto& datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val > tolerance * mean)
            {
              if (! last_late)
                hitches++;
              last_late = true;
            }

            else
            {
              last_late = false;
            }
          }
        }

        return hitches;
      }

      int calcNumSamples (LARGE_INTEGER start) noexcept
      {
        int samples_used = 0;

        for ( const auto& datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            //if (_isreal (datum.val) && datum.val > 0.0)
            //{
              samples_used++;
            //}
          }
        }

        return samples_used;
      }
    };
  };
};

using  Sleep_pfn = void (WINAPI *)(DWORD dwMilliseconds);
extern Sleep_pfn
       Sleep_Original;

using  SleepEx_pfn = DWORD (WINAPI *)(DWORD dwMilliseconds,
                                      BOOL  bAlertable);
extern SleepEx_pfn
       SleepEx_Original;

extern int __SK_FramerateLimitApplicationSite;



using NtQueryTimerResolution_pfn = NTSTATUS (NTAPI *)
(
  OUT PULONG              MinimumResolution,
  OUT PULONG              MaximumResolution,
  OUT PULONG              CurrentResolution
);

using NtSetTimerResolution_pfn = NTSTATUS (NTAPI *)
(
  IN  ULONG               DesiredResolution,
  IN  BOOLEAN             SetResolution,
  OUT PULONG              CurrentResolution
);

typedef NTSTATUS (WINAPI *NtDelayExecution_pfn)(
  IN  BOOLEAN        Alertable,
  IN  PLARGE_INTEGER DelayInterval
);


struct IDXGIOutput;

using WaitForVBlank_pfn = HRESULT (STDMETHODCALLTYPE *)(
  IDXGIOutput *This
);

typedef
NTSTATUS (NTAPI *NtWaitForSingleObject_pfn)(
  IN HANDLE         Handle,
  IN BOOLEAN        Alertable,
  IN PLARGE_INTEGER Timeout    // Microseconds
);


typedef enum _OBJECT_WAIT_TYPE {
  WaitAllObject,
  WaitAnyObject
} OBJECT_WAIT_TYPE,
*POBJECT_WAIT_TYPE;

typedef
NTSTATUS (NTAPI *NtWaitForMultipleObjects_pfn)(
  IN ULONG                ObjectCount,
  IN PHANDLE              ObjectsArray,
  IN OBJECT_WAIT_TYPE     WaitType,
  IN BOOLEAN              Alertable,
  IN PLARGE_INTEGER       TimeOut OPTIONAL );


extern void SK_Scheduler_Init     (void);
extern void SK_Scheduler_Shutdown (void);


#include <dwmapi.h>

extern HRESULT WINAPI
SK_DWM_GetCompositionTimingInfo (DWM_TIMING_INFO *pTimingInfo);

extern volatile LONG64 lD3DKMTPresentCalls;


#endif /* __SK__FRAMERATE_H__ */