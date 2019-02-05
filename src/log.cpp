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

#pragma warning (disable: 4996)

struct IUnknown;
#include <Unknwnbase.h>

#include <stdio.h>
#include <Shlwapi.h>
#include <SpecialK/log.h>
#include <SpecialK/tls.h>
#include <SpecialK/core.h>
#include <SpecialK/config.h>
#include <SpecialK/utility.h>


using GetSystemTimePreciseAsFileTime_pfn = void ( WINAPI * )(
  _Out_ LPFILETIME lpSystemTimeAsFileTime
  );                   GetSystemTimePreciseAsFileTime_pfn
  _k32GetSystemTimePreciseAsFileTime = nullptr;

WORD
SK_Timestamp (wchar_t* const out)
{
  SYSTEMTIME stLogTime;

  // Check for Windows 8 / Server 2012
  static bool __hasSystemTimePrecise =
    SK_GetProcAddress (L"kernel32", "GetSystemTimePreciseAsFileTime") != nullptr;
    //( LOBYTE (LOWORD (GetVersion ( ))) == 6   &&
    //  HIBYTE (LOWORD (GetVersion ( ))) >= 2 ) ||
    //  LOBYTE (LOWORD (GetVersion ( )    > 6));

 // More accurate timestamp is available on Windows 6.2+
  if (__hasSystemTimePrecise)
  {
    if (_k32GetSystemTimePreciseAsFileTime == nullptr)
    {
      _k32GetSystemTimePreciseAsFileTime =
        (GetSystemTimePreciseAsFileTime_pfn)
          SK_GetProcAddress ( L"kernel32",
                               "GetSystemTimePreciseAsFileTime" );
    }

    if (_k32GetSystemTimePreciseAsFileTime == nullptr)
    {
      __hasSystemTimePrecise = false;
      GetLocalTime (&stLogTime);
    }
    else
    {
      FILETIME                             ftLogTime;
      _k32GetSystemTimePreciseAsFileTime (&ftLogTime);
      FileTimeToSystemTime (&ftLogTime,
                            &stLogTime);
    }
  }

  else
  {
    GetLocalTime (&stLogTime);
  }

  wchar_t date [48] = { };
  wchar_t time [48] = { };

  GetDateFormat (LOCALE_INVARIANT, DATE_SHORTDATE,    &stLogTime, nullptr, date, 47);
  GetTimeFormat (LOCALE_INVARIANT, TIME_NOTIMEMARKER, &stLogTime, nullptr, time, 47);

  out [0] = L'\0';

  lstrcatW (out, date);
  lstrcatW (out, L" ");
  lstrcatW (out, time);
  lstrcatW (out, L".");

  return stLogTime.wMilliseconds;
}


#include <concurrent_unordered_map.h>
#include <SpecialK/thread.h>

// Due to the way concurrent data structures grow, we can't shrink this beast
//   and this _is_ technically a set, of sorts... but knowing if an element is
//     present requires treating it like a map and reading a boolean.
concurrency::concurrent_unordered_map <iSK_Logger *, bool> flush_set;
HANDLE                                                     hFlushReq  = nullptr;

DWORD
WINAPI
SK_Log_AsyncFlushThreadPump (LPVOID)
{
  SetCurrentThreadDescription (            L"[SK] Async Log Flush Thread Pump" );
  SetThreadPriority           ( SK_GetCurrentThread (), THREAD_PRIORITY_LOWEST );

  // TODO:  Consider an interlocked singly-linked list instead
  //
  //         ( The only drawback is Windows 7 does not support
  //             these in 64-bit code ... but what's new? )
  //
  //    _Have I remembered to mention how much Windows 7 sucks recently?_
  //
  while (! ReadAcquire (&__SK_DLL_Ending))
  {
    if (! flush_set.empty ())
    {
      for ( auto& it : flush_set )
      {
        if ( it.second != false  )
        {
          if ( it.first       != nullptr &&
               it.first->fLog != nullptr    )
          {
            fflush ( it.first->fLog );
          }

          it.second = false;
        }
      }
    }

    SK_WaitForSingleObject (hFlushReq, INFINITE);
    ResetEvent             (hFlushReq);
  }

  CloseHandle (hFlushReq);
               hFlushReq = nullptr;

  SK_Thread_CloseSelf  ();

  return 0;
}


BOOL
SK_FlushLog (iSK_Logger* pLog)
{
  // Perf. counter; made obsolete by SK's built-in
  //  thread profiler and per-thread file I/O stats.
#ifdef _DEBUG
  static volatile LONG   flush_reqs  (0);
#endif

  static volatile HANDLE
    hFlushThread = INVALID_HANDLE_VALUE;

  if ( INVALID_HANDLE_VALUE ==
         InterlockedCompareExchangePointer (
           &hFlushThread,
             reinterpret_cast <PVOID> (-2),
               INVALID_HANDLE_VALUE
         )
     )
  {
    hFlushReq =
      SK_CreateEvent ( nullptr, TRUE, TRUE,
        SK_FormatStringW ( LR"(Local\SK_LogFlush_pid%x)",
                      GetCurrentProcessId () ).c_str ()
                  );

    InterlockedExchangePointer (
      const_cast         <         LPVOID *> (
        reinterpret_cast <volatile LPVOID *>   (&hFlushThread)
                                             ),
        SK_Thread_CreateEx ( SK_Log_AsyncFlushThreadPump )
    );
  }

  if (! ReadAcquire (&__SK_DLL_Ending))
  {
    while ((intptr_t)hFlushReq <= 0)
      SK_Sleep (1);

    if ( (! flush_set.count ( pLog )) ||
            flush_set       [ pLog ] == false )
    { flush_set             [ pLog ]  = true;
      SetEvent              ( hFlushReq );
#ifdef _DEBUG
      InterlockedIncrement  (&flush_reqs);
#endif
    }
  }

  else if ( pLog       != nullptr &&
            pLog->fLog != nullptr    )
  {
    _fflush_nolock (pLog->fLog);
    _flushall      (          );
  }

  return TRUE;
}

iSK_Logger dll_log, budget_log;


std::wstring
__stdcall
SK_Log_GetPath (const wchar_t* wszFileName)
{
  std::wstring formatted_file =
    SK_FormatStringW ( LR"(%slogs\%s)",
                         SK_GetConfigPath (),
                           wszFileName );

  SK_CreateDirectories (
    formatted_file.c_str ()
  );

  return formatted_file;
}


void
iSK_Logger::close (void)
{
  if (initialized)
    EnterCriticalSection (&log_mutex);

  else
    return;


  if (fLog != nullptr)
  {
    if ( flush_set.count (this) )
    {
         flush_set       [this] = false;
    }

    fflush (fLog);
    fclose (fLog);
            fLog = nullptr;
  }


  if (lines == 0)
  {
    std::wstring  full_name (
      SK_GetConfigPath ()
    );

    full_name  += name;

    if (StrStrIW (name.c_str (), LR"(crash\)"))
      full_name = name;

    DeleteFileW  (full_name.c_str ());
  }

  initialized = false;
  silent      = true;

  Release ();

  LeaveCriticalSection  (&log_mutex);
  DeleteCriticalSection (&log_mutex);
}


bool
iSK_Logger::init ( const wchar_t* const wszFileName,
                   const wchar_t* const wszMode )
{
  if (initialized)
    return true;

  if (name.empty ())
    lines = 0;

  name = wszFileName;

  std::wstring full_name (
    SK_GetConfigPath ()
  );

  SK_CreateDirectories (
    std::wstring (full_name + LR"(logs\)").c_str ()
  );

  full_name  += wszFileName;

  if (StrStrIW (wszFileName, LR"(crash\)"))
  {
    full_name = wszFileName;
  }

  fLog   = _wfopen (full_name.c_str (), wszMode);
  silent = false;

  BOOL bRet =
    InitializeCriticalSectionEx (&log_mutex, 400, RTL_CRITICAL_SECTION_FLAG_DYNAMIC_SPIN |
                                                  SK_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO);
   lockless = true;

  if ((! bRet) || (fLog == nullptr))
  {
    silent = true;
    return false;
  }

  AddRef ();

  return (initialized = true);
}

void
iSK_Logger::LogEx ( bool                 _Timestamp,
  _In_z_ _Printf_format_string_
                     wchar_t const* const _Format,
                                          ... )
{
  if ((! initialized) || (! fLog) || silent)
    return;


  wchar_t wszFormattedTime [48] = { };

  if (_Timestamp)
  {
    wchar_t                    wszLogTime [48] = { };
    UINT    ms = SK_Timestamp (wszLogTime);

    _swprintf ( wszFormattedTime,
                  L"%s%03u: ",
                    wszLogTime,
                      ms );
  }


  va_list   _ArgList;
  va_start (_ArgList, _Format);
  size_t len =
    _vscwprintf (      _Format,
            _ArgList) + 1 + 48 + 2; // 48 extra for timestamp
  va_end   (_ArgList);


  SK_TLS *pTLS = nullptr;

  extern volatile
               LONG __SK_Threads_Attached;
  if (ReadAcquire (&__SK_Threads_Attached) > 0)
  {
    pTLS =
      SK_TLS_Bottom ();
  }

  wchar_t* wszOut =
    pTLS != nullptr ?
    pTLS->scratch_memory.log.formatted_output.alloc (
      len, true
    )    : nullptr;

  if (! wszOut)
  {
    wszOut =
      static_cast <wchar_t *> (
        _alloca ( len *
           sizeof (wchar_t)
        )
      );
  }

  if (! wszOut)
    return;



  wchar_t *wszAfterStamp =
    ( _Timestamp ? ( wszOut + wcslen (wszFormattedTime) ) :
                     wszOut );
  if (_Timestamp)
  {
    wcscpy (wszOut, wszFormattedTime);
  }

  va_start   (_ArgList,      _Format);
  _vswprintf (wszAfterStamp, _Format, _ArgList);
  va_end     (_ArgList);

  lock   ();
  //if (! lockless)
  //  _fwrite_nolock (wszOut, 1, wcslen (wszOut), fLog);
  //else
    fputws (wszOut, fLog);
  unlock ();

  ++lines;

  SK_FlushLog (this);
}

void
iSK_Logger::Log   ( _In_z_ _Printf_format_string_
                    wchar_t const* const _Format,
                                         ... )
{
  if ((! initialized) || (! fLog) || silent)
    return;


  wchar_t              wszFormattedTime [48] = { };
  wchar_t                    wszLogTime [48] = { };
  UINT    ms = SK_Timestamp (wszLogTime);

  _swprintf ( wszFormattedTime,
                L"%s%03u: ",
                  wszLogTime,
                    ms );

  va_list   _ArgList;
  va_start (_ArgList, _Format);

  size_t len =
    _vscwprintf (     _Format,
            _ArgList) + 1 + 2  //  2 extra for CrLf
                         + 48; // 32 extra for timestamp
  va_end   (_ArgList);


  SK_TLS *pTLS = nullptr;

  extern volatile
               LONG __SK_Threads_Attached;
  if (ReadAcquire (&__SK_Threads_Attached) > 0)
  {
    pTLS =
      SK_TLS_Bottom ();
  }

  wchar_t* wszOut =
    pTLS != nullptr ?
    pTLS->scratch_memory.log.formatted_output.alloc (
      len, true
    )    : nullptr;

  if (! wszOut)
  {
    wszOut =
      static_cast <wchar_t *> (
        _alloca ( len *
           sizeof (wchar_t)
        )
      );
  }

  if (! wszOut)
    return;


  wchar_t *wszAfterStamp =
    wszOut + wcslen (wszFormattedTime);
    wcscpy  (wszOut, wszFormattedTime);

  va_start (_ArgList,        _Format);
  _vswprintf (wszAfterStamp, _Format,
            _ArgList);
  va_end   (_ArgList);

  lstrcatW (wszAfterStamp, L"\n");

  lock   ();
  //if (! lockless)
  //  _fwrite_nolock (wszOut, 1, wcslen (wszOut), fLog);
  //else
    fputws (wszOut, fLog);
  unlock ();

  ++lines;

  SK_FlushLog (this);
}

void
[[deprecated]]
iSK_Logger::Log   ( _In_z_ _Printf_format_string_
                    char const* const _Format,
                                      ... )
{
  if ((! initialized) || (! fLog) || silent)
    return;


  wchar_t                    wszLogTime [48] = { };
  UINT    ms = SK_Timestamp (wszLogTime);

  lock ();

  fwprintf ( fLog,
               L"%s%03u: ",  wszLogTime, ms );

  va_list   _ArgList;
  va_start (_ArgList, _Format);
  {
    vfprintf ( fLog,  _Format,
            _ArgList);
  }
  va_end   (_ArgList);

  fwprintf (fLog, L"\n");

          ++lines;

  unlock   ();


  SK_FlushLog (this);
}

HRESULT
iSK_Logger::QueryInterface (THIS_ REFIID riid, void** ppvObj)
{
  if (IsEqualGUID (riid, IID_SK_Logger))
  {
    AddRef ();

    *ppvObj = this;

    return S_OK;
  }

  return E_NOINTERFACE;
}

ULONG
iSK_Logger::AddRef (THIS)
{
  return
    InterlockedIncrement (&refs);
}

ULONG
iSK_Logger::Release (THIS)
{
  if (   InterlockedDecrement (&refs) != 0UL )
    return (ULONG)ReadAcquire (&refs);

  if (initialized)
  {
    initialized = false;
    delete this;
  }

  return 0;
}


iSK_Logger*
__stdcall
SK_CreateLog (const wchar_t* const wszName)
{
  auto* pLog =
    new iSK_Logger ();

  pLog->init   (wszName, L"wtc+,ccs=UTF-8");
  pLog->silent = false;

  return pLog;
}


std::wstring
__stdcall
SK_SummarizeCaller (LPVOID lpReturnAddr)
{
  wchar_t wszSummary [256] = { };
  char    szSymbol   [256] = { };
  ULONG   ulLen            = 191;

  ulLen = SK_GetSymbolNameFromModuleAddr (
              SK_GetCallingDLL (lpReturnAddr),
  reinterpret_cast <uintptr_t> (lpReturnAddr),
                szSymbol,
                  ulLen );

  std::wstring caller =
    SK_GetCallerName (lpReturnAddr);

  if (ulLen > 0)
  {
    swprintf_s ( wszSummary, 255,
                   L"[ %-25s <%30hs>, tid=0x%04x ]",

           caller.c_str (),
             szSymbol,
               GetCurrentThreadId                ()
    );
  }

  else {
    swprintf_s ( wszSummary, 255,
                   L"[ %-58s, tid=0x%04x ]",

           caller.c_str         (),
             GetCurrentThreadId ()
    );
  }

  return wszSummary;
}