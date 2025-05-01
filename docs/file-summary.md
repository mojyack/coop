# core
- cohandle      `std::coroutine_handle<>` abstraction
- promise       coroutine promise
- generator     return value of async functions, which generates promise
- runner        task scheduler
- task-handle   structure for user to control spawned tasks

# awaitable objects
- multi-event   multiple-waiters event
- single-event  single-waiter event
- thread-event  thread-safe event
- io            awaiter which waits for file descriptor read/write
- parallel      awaiter which runs multiple tasks in parallel
- thread        awaiter which wraps blocking function
- timer         awaiter which suspends task for specified duration

# utilities
- mutex             acts like regular mutex
- lock-guard        unlocks mutex automatically
- atomic-event      inter-thread synchronizer
- blocker           used to suspend runner in order to manipulate it from another thread
- recursive-blocker blocker but more efficient when multiple threads lock
- select            wait for any one of several tasks to finish
- task-injector     blocker wrapper to add task from another thread and retrieve its result
- assert            error check and print helper
- pipe              pipe abstraction

