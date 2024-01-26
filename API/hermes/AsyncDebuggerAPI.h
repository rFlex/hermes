/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef HERMES_ASYNCDEBUGGERAPI_H
#define HERMES_ASYNCDEBUGGERAPI_H

#ifdef HERMES_ENABLE_DEBUGGER

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

#include <hermes/DebuggerAPI.h>
#include <hermes/Public/HermesExport.h>
#include <hermes/hermes.h>
#include <hermes/inspector/chrome/ThreadSafetyAnalysis.h>

namespace facebook {
namespace hermes {
namespace debugger {

class AsyncDebuggerAPI;

enum class DebuggerEventType {
  // Informational Events
  ScriptLoaded, /// A script file was loaded, and the debugger has requested
  /// pausing after script load.
  Exception, /// An Exception was thrown.
  EvalComplete, /// An eval() function finished.
  Resumed, /// Script execution has resumed.

  // Events Requiring Next Command
  DebuggerStatement, /// A debugger; statement was hit.
  Breakpoint, /// A breakpoint was hit.
  StepFinish, /// A Step operation completed.
  ExplicitPause, /// A pause requested using Explicit AsyncBreak
};

using DebuggerEventCallback = std::function<void(
    HermesRuntime &runtime,
    AsyncDebuggerAPI &asyncDebugger,
    DebuggerEventType event)>;
using DebuggerEventCallbackID = uint32_t;
constexpr const uint32_t kInvalidDebuggerEventCallbackID = 0;
using InterruptCallback = std::function<void(HermesRuntime &runtime)>;

/// This class wraps the DebuggerAPI to expose an asynchronous didPause
/// functionality as well as an interrupt API. This class must be constructed at
/// the same time as HermesRuntime.
///
/// Functions in this class with the suffix "_TS" (Thread-Safe) are the only
/// functions that are safe to call on any thread. All other functions must be
/// called on the runtime thread.
class HERMES_EXPORT AsyncDebuggerAPI : private debugger::EventObserver {
  /// Hide the constructor so users can only construct via static create
  /// methods.
  AsyncDebuggerAPI(HermesRuntime &runtime);

 public:
  /// Creates an AsyncDebuggerAPI for use with the provided HermesRuntime. This
  /// should be called and created at the same time as creating HermesRuntime.
  static std::unique_ptr<AsyncDebuggerAPI> create(HermesRuntime &runtime);

  /// Must be destroyed on the runtime thread or when you're sure nothing is
  /// interacting with the runtime. Must be destroyed before destroying
  /// HermesRuntime.
  ~AsyncDebuggerAPI();

  /// Add a callback function to invoke when the runtime pauses due to various
  /// conditions such as hitting a "debugger;" statement. Can be called from any
  /// thread. If there are no DebuggerEventCallback, then any reason that might
  /// trigger a pause, such as a "debugger;" statement or breakpoints, will not
  /// actually pause and will simply continue execution.
  DebuggerEventCallbackID addDebuggerEventCallback_TS(
      DebuggerEventCallback callback);

  /// Remove a previously added callback function. If there is no callback
  /// registered using the provided \p id, the function does nothing.
  void removeDebuggerEventCallback_TS(DebuggerEventCallbackID id);

  /// Whether the runtime is currently paused waiting for the next
  /// debugger::Command. Should only be called from the runtime thread.
  bool isWaitingForCommand();

  /// Provide the next debugger::Command. Should only be called from the runtime
  /// thread and only if the next command is expected to be set.
  void setNextCommand(debugger::Command command);

  /// Request to interrupt the runtime at a convenient time and get a callback
  /// on the runtime thread. Guaranteed to run "exactly once". This function can
  /// be called from any thread, but cannot be called while inside a
  /// DebuggerEventCallback.
  void triggerInterrupt_TS(InterruptCallback callback);

  /// EventObserver implementation
  debugger::Command didPause(debugger::Debugger &debugger) override;

 private:
  struct EventCallbackEntry {
    DebuggerEventCallbackID id;
    DebuggerEventCallback callback;
  };

  /// This function infinite loops and uses \p signal_ to block the runtime
  /// thread. It gets woken up if new InterruptCallback is queued or if
  /// DebuggerEventCallback changes.
  void processInterruptWhilePaused() TSA_NO_THREAD_SAFETY_ANALYSIS;

  /// Dequeues the next InterruptCallback if any.
  std::optional<InterruptCallback> takeNextInterruptCallback();

  /// Runs every InterruptCallback that has been queued up so far.
  void runInterrupts();

  /// Returns the next DebuggerEventCallback to execute if any.
  std::optional<DebuggerEventCallback> takeNextEventCallback();

  /// Runs every DebuggerEventCallback that has been registered.
  void runEventCallbacks(DebuggerEventType event);

  HermesRuntime &runtime_;

  /// Whether the runtime thread is currently paused in \p didPause and needs to
  /// be told what debugger::Command to use next.
  bool isWaitingForCommand_;

  /// Stores the command to return from \p didPause.
  debugger::Command nextCommand_;

  /// Tracks whether we are already in a didPause callback to detect recursive
  /// calls to didPause.
  bool inDidPause_ = false;

  /// Next ID to use when adding a DebuggerEventCallback.
  uint32_t nextEventCallbackID_ TSA_GUARDED_BY(mutex_);

  /// Callback functions to invoke to notify events in \p didPause. Using
  /// std::list which requires O(N) search when removing an element, but removal
  /// should be a rare event. So the choice of using std::list is to optimize
  /// for typical usage.
  std::list<EventCallbackEntry> eventCallbacks_ TSA_GUARDED_BY(mutex_){};

  /// Iterator for eventCallbacks_. Used to traverse through the list when
  /// running the callbacks.
  std::list<EventCallbackEntry>::iterator eventCallbackIterator_
      TSA_GUARDED_BY(mutex_);

  /// Queue of interrupt callback functions to invoke.
  std::queue<InterruptCallback> interruptCallbacks_ TSA_GUARDED_BY(mutex_){};

  /// Used as a mechanism to block the runtime thread in \p didPause and for
  /// protecting variables used across threads.
  std::mutex mutex_{};
  /// Used to implement \p triggerInterrupt while \p didPause is holding onto
  /// the runtime thread.
  std::condition_variable signal_{};
};

} // namespace debugger
} // namespace hermes
} // namespace facebook

#else // !HERMES_ENABLE_DEBUGGER

#include <hermes/DebuggerAPI.h>
#include <hermes/Public/HermesExport.h>
#include <hermes/hermes.h>

namespace facebook {
namespace hermes {
namespace debugger {

class AsyncDebuggerAPI;

enum class DebuggerEventType {
  // Informational Events
  ScriptLoaded, /// A script file was loaded, and the debugger has requested
  /// pausing after script load.
  Exception, /// An Exception was thrown.
  EvalComplete, /// An eval() function finished.
  Resumed, /// Script execution has resumed.

  // Events Requiring Next Command
  DebuggerStatement, /// A debugger; statement was hit.
  Breakpoint, /// A breakpoint was hit.
  StepFinish, /// A Step operation completed.
  ExplicitPause, /// A pause requested using Explicit AsyncBreak
};

using DebuggerEventCallback = std::function<void(
    HermesRuntime &runtime,
    AsyncDebuggerAPI &asyncDebugger,
    DebuggerEventType event)>;
using DebuggerEventCallbackID = uint32_t;
constexpr const uint32_t kInvalidDebuggerEventCallbackID = 0;
using InterruptCallback = std::function<void(HermesRuntime &runtime)>;

class HERMES_EXPORT AsyncDebuggerAPI {
 public:
  static std::unique_ptr<AsyncDebuggerAPI> create(HermesRuntime &runtime) {
    return nullptr;
  }

  ~AsyncDebuggerAPI() {}

  DebuggerEventCallbackID addDebuggerEventCallback_TS(
      DebuggerEventCallback callback) {
    return kInvalidDebuggerEventCallbackID;
  }

  void removeDebuggerEventCallback_TS(DebuggerEventCallbackID id) {}

  bool isWaitingForCommand() {
    return false;
  }

  void setNextCommand(debugger::Command command) {}

  void triggerInterrupt_TS(InterruptCallback callback) {}
};

} // namespace debugger
} // namespace hermes
} // namespace facebook

#endif // !HERMES_ENABLE_DEBUGGER

#endif // HERMES_ASYNCDEBUGGERAPI_H
